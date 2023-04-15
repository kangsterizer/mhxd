#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "sys_net.h"
#if !defined(__WIN32__)
#include <arpa/inet.h>
#endif
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include "hlserver.h"
#include "xmalloc.h"
#if defined(CONFIG_HFS)
#include "hfs.h"
#endif
#include "threads.h"

#define HTXF_BUFSIZE		0xf000

typedef ssize_t watch_time_t;

struct watch {
	struct timeval begin;
	/* 1/10^6 */
	watch_time_t time;
};

#if defined(CONFIG_HTXF_PTHREAD)
#define t_free(x)
#define t_paths_free(x)
#define t_watch_delete(x)
#else
#define t_free(x) free(x)
#define t_paths_free(x) paths_free(x)
#define t_watch_delete(x) watch_delete(x)
#endif

static inline struct watch *
watch_new (void)
{
	struct watch *wp;

	wp = malloc(sizeof(struct watch));
	if (!wp)
		return 0;
	memset(wp, 0, sizeof(struct watch));

	return wp;
}

static inline void
watch_delete (void *wp)
{
	if (wp)
		free(wp);
}

static inline void
watch_reset (struct watch *wp)
{
	memset(wp, 0, sizeof(struct watch));
}

static inline void
watch_start (struct watch *wp)
{
	gettimeofday(&wp->begin, 0);
}

static inline void
watch_stop (struct watch *wp)
{
	struct timeval now;
	watch_time_t sec, usec;

	gettimeofday(&now, 0);
	if ((usec = now.tv_usec - wp->begin.tv_usec) < 0) {
		usec += 1000000;
		sec = now.tv_sec - wp->begin.tv_sec - 1;
	} else {
		sec = now.tv_sec - wp->begin.tv_sec;
	}
	wp->time = usec + (sec * 1000000);
}

static void
shape_out (ssize_t bytes, watch_time_t usecs, ssize_t max_Bps)
{
	float Bpus, max_Bpus = (float)(max_Bps) / 1000000;

	Bpus = (float)bytes / (float)usecs;
	if (Bpus > max_Bpus) {
		size_t st = (size_t)((Bpus / max_Bpus) * (float)usecs) - usecs;
		usleep(st);
		if (errno == EINVAL && st > 999999) {
			sleep(st / 1000000);
			if (st > 1000000)
				usleep(st % 1000000);
		}
	}
}

#if !defined(__WIN32__) && !defined(CONFIG_HTXF_PTHREAD)
static RETSIGTYPE
term_catcher (int sig)
{
	_exit(sig);
}
#endif

static int
file_send (struct htxf_conn *htxf, u_int8_t *buf, struct watch *wp)
{
	u_int32_t data_size, data_pos, rsrc_size, rsrc_pos;
	u_int16_t preview;
	struct SOCKADDR_IN sa;
	int r, rlen, s, f;
	char *path;
	u_int32_t bufsize;
#if defined(CONFIG_HFS)
	int use_hfs = hxd_cfg.operation.hfs;
	struct hfsinfo fi;
#endif

	data_size = htxf->data_size;
	data_pos = htxf->data_pos;
	rsrc_size = htxf->rsrc_size;
	rsrc_pos = htxf->rsrc_pos;
	preview = htxf->preview;
	path = htxf->path;
	sa = htxf->sockaddr;
	s = htxf->fd;

	bufsize = htxf->limit_out_Bps;
	if (!bufsize || bufsize > HTXF_BUFSIZE)
		bufsize = HTXF_BUFSIZE;

	if (!preview) {
#if defined(CONFIG_HFS)
		if (use_hfs)
			hfsinfo_read(path, &fi);
#endif
		/* TODO: use a struct for this data structure */
		memcpy(buf,
"FILP"
"\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\2"
"INFO"
"\0\0\0\0\0\0\0\0\0\0\0^"
"AMAC"
"TYPECREA"
"\0\0\0\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0"
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
"\7\160\0\0\0\0\0\0\7\160\0\0\0\0\0\0\0\0\0\3hxd\0", 116);
#if defined(CONFIG_HFS)
		if (use_hfs) {
			memcpy(buf+44, fi.type, 8);
			buf[116] = fi.comlen;
			memcpy(buf+117, fi.comment, fi.comlen);
			if ((rsrc_size - rsrc_pos))
				buf[23] = 3;
			if (65 + fi.comlen + 12 > 0xff)
				buf[38] = 1;
			buf[39] = 65 + fi.comlen + 12;
	
			/* *((u_int16_t *)(&buf[92])) = htons(1904); */
			*((u_int32_t *)(&buf[96])) = hfs_h_to_mtime(fi.create_time);
			/* *((u_int16_t *)(&buf[100])) = htons(1904); */
			*((u_int32_t *)(&buf[104])) = hfs_h_to_mtime(fi.modify_time);
			memcpy(&buf[117] + fi.comlen, "DATA\0\0\0\0\0\0\0\0", 12);
			S32HTON((data_size - data_pos), &buf[129 + fi.comlen]);
			if (socket_write(s, buf, 133 + fi.comlen) != (ssize_t)(133 + fi.comlen)) {
				hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
				return -1;
			}
			htxf->total_pos += 133 + fi.comlen;
		} else {
#endif /* CONFIG_HFS */
			buf[39] = 65 + 12;
			memcpy(&buf[117], "DATA\0\0\0\0\0\0\0\0", 12);
			S32HTON((data_size - data_pos), &buf[129]);
			if (socket_write(s, buf, 133) != (ssize_t)(133)) {
				hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
				return -1;
			}
			htxf->total_pos += 133;
#if defined(CONFIG_HFS)
		}
#endif
	}

	if (htxf->gone)
		return 1;

	if ((data_size - data_pos)) {
		f = SYS_open(path, O_RDONLY, 0);
		if (f < 0) {
			hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
			return -1;
		}
		if (data_pos) {
			if (lseek(f, data_pos, SEEK_SET) != (off_t)data_pos) {
				hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
				close(f);
				return -1;
			}
		}
		if (htxf->gone) {
			close(f);
			return 1;
		}
		for (;;) {
			rlen = bufsize > (data_size - data_pos)
					? (data_size - data_pos) : bufsize;
			r = read(f, buf, rlen);
			if (r <= 0) {
				hxd_log("%s:%d: read(%d,%d)==%d; %s (%d)",
					__FILE__, __LINE__, f, rlen, r, strerror(errno), errno);
				close(f);
				return -1;
			}
			if (htxf->gone) {
				close(f);
				return 1;
			}
			if (wp) {
				watch_reset(wp);
				watch_start(wp);
			}
			rlen = r;
			r = socket_write(s, buf, rlen);
			if (r != rlen) {
				hxd_log("%s:%d: send(%d,%d)==%d; %s (%d)",
					__FILE__, __LINE__, s, rlen, r, strerror(errno), errno);
				close(f);
				return -1;
			}
			if (htxf->gone) {
				close(f);
				return 1;
			}
			if (wp)
				watch_stop(wp);
			data_pos += r;
			htxf->data_pos = data_pos;
			htxf->total_pos += r;
			if (data_pos >= data_size)
				break;
			if (wp) {
				shape_out(r, wp->time, htxf->limit_out_Bps);
			}
		}
		close(f);
	}
#if defined(CONFIG_HFS)
	if (!use_hfs || preview || !(rsrc_size - rsrc_pos))
		return 0;
	if (htxf->gone)
		return 1;
	memcpy(buf, "MACR\0\0\0\0\0\0\0\0", 12);
	S32HTON((rsrc_size - rsrc_pos), &buf[12]);
	if (socket_write(s, buf, 16) != 16) {
		hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
		return -1;
	}
	if (htxf->gone)
		return 1;
	if ((rsrc_size - rsrc_pos)) {
		f = resource_open(path, O_RDONLY, 0);
		if (f < 0) {
			hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
			return -1;
		}
		if (rsrc_pos) {
			if (lseek(f, rsrc_pos, SEEK_CUR) == (off_t)-1) {
				hxd_log("%s:%d: lseek: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
				close(f);
				return -1;
			}
		}
		if (htxf->gone) {
			close(f);
			return 1;
		}
		for (;;) {
			rlen = bufsize > (rsrc_size - rsrc_pos)
					? (rsrc_size - rsrc_pos) : bufsize;
			r = read(f, buf, rlen);
			if (r <= 0) {
				hxd_log("%s:%d: read(%d,%d)==%d; %s (%d)",
					__FILE__, __LINE__, f, rlen, r, strerror(errno), errno);
				close(f);
				return -1;
			}
			if (htxf->gone) {
				close(f);
				return 1;
			}
			if (wp) {
				watch_reset(wp);
				watch_start(wp);
			}
			rlen = r;

			r = socket_write(s, buf, rlen);
			if (r != rlen) {
				hxd_log("%s:%d: write(%d,%d)==%d; %s (%d)",
					__FILE__, __LINE__, s, rlen, r, strerror(errno), errno);
				close(f);
				return -1;
			}
			if (htxf->gone) {
				close(f);
				return 1;
			}
			if (wp)
				watch_stop(wp);
			rsrc_pos += r;
			htxf->rsrc_pos = rsrc_pos;
			htxf->total_pos += r;
			if (rsrc_pos >= rsrc_size)
				break;
			if (wp) {
				shape_out(r, wp->time, htxf->limit_out_Bps);
			}
		}
		close(f);
	}
#endif /* CONFIG_HFS */

	return 0;
}

struct path_file {
	u_int32_t data_size PACKED;
	u_int32_t rsrc_size PACKED;
	u_int32_t total_size PACKED;
	u_int16_t nlen PACKED;
	u_int16_t noff PACKED;
	u_int8_t type PACKED; /* 1 = folder, 0 = file */
	u_int8_t path[1] PACKED; /* really longer */
};

#define SIZEOF_PATH_FILE (17)

static int
paths_compare (const void *p1, const void *p2)
{
	struct path_file *path1 = *(struct path_file **)p1;
	struct path_file *path2 = *(struct path_file **)p2;
	int i, len1, len2, minlen;
	char *n1, *n2;

	n1 = path1->path + path1->noff;
	n2 = path2->path + path2->noff;
	len1 = path1->nlen;
	len2 = path2->nlen;
	minlen = len1 > len2 ? len2 : len1;
	for (i = 0; i < minlen; i++) {
		if (n1[i] > n2[i])
			return 1;
		else if (n1[i] != n2[i])
			return -1;
	}
	if (len1 > len2)
		return 1;
	else if (len1 != len2)
		return -1;

	return 0;
}

static void
paths_free (void *p)
{
	struct path_file **pf = (struct path_file **)p;
	u_int32_t i;

	for (i = 0; pf[i]; i++)
		free(pf[i]);
	free(pf);
}

/* Returns a sorted path array */
static struct path_file **
folder_getpaths (u_int32_t *npathsp, char *path)
{
	DIR *dir;
	struct dirent *de;
	struct stat sb;
	u_int32_t nfiles, i;
	struct path_file **paths = 0;
	char pathbuf[MAXPATHLEN];
	u_int32_t data_size, rsrc_size, size;
	u_int32_t data_pos = 0, rsrc_pos = 0;
	struct path_file *pf;
	u_int16_t preview = 0;

	dir = opendir(path);
	if (!dir)
		return 0;
	/* Read the number of entries in the directory */
	nfiles = 0;
	while ((de = readdir(dir))) {
		if (de->d_name[0] == '.')
			continue;
		nfiles++;
	}
	rewinddir(dir);
	paths = malloc(sizeof(char *) * (nfiles + 1));
	if (!paths) {
		*npathsp = 0;
		return 0;
	}
	paths[0] = 0;
	i = 0;
	while ((de = readdir(dir))) {
		if (de->d_name[0] == '.')
			continue;
		snprintf(pathbuf, sizeof(pathbuf), "%s/%s", path, de->d_name);
		if (stat(pathbuf, &sb))
			continue;
		if (i >= nfiles) {
			break;
		}
		pf = malloc(SIZEOF_PATH_FILE + strlen(pathbuf)+1);
		paths[i] = pf;
		if (!pf) {
			t_paths_free(paths);
			return 0;
		}
		pf->nlen = strlen(de->d_name);
		pf->noff = strlen(path)+1;
		strcpy(pf->path, pathbuf);
		i++;
		if (S_ISDIR(sb.st_mode)) {
			pf->type = 1;
			pf->data_size = 0;
			pf->rsrc_size = 0;
			pf->total_size = 0;
		} else {
			data_size = sb.st_size;
			rsrc_size = sizeof(pathbuf);
			size = (data_size - data_pos) + (preview ? 0 : (rsrc_size - rsrc_pos));
			size += 133 + ((rsrc_size - rsrc_pos) ? 16 : 0) + strlen(pathbuf);
			pf->type = 0;
			pf->data_size = data_size;
			pf->rsrc_size = rsrc_size;
			pf->total_size = size;
		}
	}
	closedir(dir);
	paths[i] = 0;

	qsort(paths, nfiles, sizeof(struct path_file *), paths_compare);
	*npathsp = nfiles;

	return paths;
}

struct next_file_info {
	u_int16_t len PACKED;
	u_int16_t type PACKED;
	u_int16_t pathcount PACKED;
};

static int
folder_send (struct htxf_conn *htxf, u_int8_t *buf, struct watch *wp)
{
	u_int32_t nfiles, curfile = 0;
	struct path_file **pf;
	u_int16_t cmd, len, lastcmd = 0;
	int r, s = htxf->fd;
	u_int8_t rflt[128];
	u_int32_t data_pos = 0, rsrc_pos = 0, size;
	struct next_file_info *nfi;
	int retval;

	pf = folder_getpaths(&nfiles, htxf->path);
	if (!pf)
		return -1;
#if defined(CONFIG_HTXF_PTHREAD)
	pthread_cleanup_push(paths_free, pf);
#endif
	for (;;) {
		r = socket_read(s, &cmd, sizeof(cmd));
		if (r != sizeof(cmd)) {
			retval = -1;
			goto ret;
		}
		cmd = ntohs(cmd);
#define FILE_SEND	1
#define FILE_RESUME	2
#define FILE_NEXT	3
		switch (cmd)  {
			case FILE_NEXT:
				if (lastcmd != 0) {
					if (lastcmd == FILE_NEXT)
						htxf->total_pos += pf[curfile]->total_size;
					curfile++;
				}
				data_pos = 0;
				rsrc_pos = 0;
				if (curfile >= nfiles) {
					retval = 0;
					goto ret;
				}
				nfi = (struct next_file_info *)buf;
				nfi->len = htons(7 + pf[curfile]->nlen);
				nfi->type = pf[curfile]->type ? htons(1) : 0;
				nfi->pathcount = htons(1);
				buf[6] = 0;
				buf[7] = 0;
				buf[8] = (u_int8_t)pf[curfile]->nlen;
				memcpy(buf+9, pf[curfile]->path+pf[curfile]->noff, (u_int32_t)buf[8]);
				socket_write(s, buf, 9 + pf[curfile]->nlen);
				break;
			case FILE_RESUME:
				r = socket_read(s, &len, sizeof(len));
				if (r != sizeof(len)) {
					retval = -1;
					goto ret;
				}
				len = ntohs(len);
				if (len >= sizeof(rflt))
					len = sizeof(rflt);
				r = socket_read(s, rflt, len);
				if (r != len) {
					retval = -1;
					goto ret;
				}
				if (len >= 50)
					L32NTOH(data_pos, &rflt[46]);
				if (len >= 66)
					L32NTOH(rsrc_pos, &rflt[62]);
				htxf->total_pos += data_pos + rsrc_pos;
				pf[curfile]->total_size -= (data_pos + rsrc_pos);
				/* send the file */
			case FILE_SEND:
				size = htonl(pf[curfile]->total_size);
				socket_write(s, &size, sizeof(size));
				htxf->data_pos = data_pos;
				htxf->rsrc_pos = rsrc_pos;
				htxf->data_size = pf[curfile]->data_size;
				htxf->rsrc_size = pf[curfile]->rsrc_size;
				strcpy(htxf->path, pf[curfile]->path);
				r = file_send(htxf, buf, wp);
				if (r) {
					retval = r;
					goto ret;
				}
				break;
			default:
				break;
		}
		lastcmd = cmd;
	}

ret:
#if defined(CONFIG_HTXF_PTHREAD)
	pthread_cleanup_pop(1);
#else
	t_paths_free(pf);
#endif

	return retval;
}

static int
fd_select (int fd, int seconds)
{
	fd_set fdr;
	struct timeval to;

	memset(&to, 0, sizeof(to));
	to.tv_sec = seconds;
	FD_SET(fd, &fdr);

	return select(fd, &fdr, 0, 0, &to);
}

static thread_return_type
get_thread (void *__arg)
{
	struct htxf_conn *htxf = (struct htxf_conn *)__arg;
	char *path;
	int r, s, retval = 0;
	struct SOCKADDR_IN sa;
	u_int8_t *buf;
	char abuf[HOSTLEN+1];
	struct watch *wp = 0;
#if defined(CONFIG_HTXF_QUEUE)
	int delay = 0;
#endif
#if !defined(__WIN32__) && (defined(CONFIG_HTXF_CLONE) || defined(CONFIG_HTXF_FORK))
	struct sigaction act;

	act.sa_flags = 0;
	act.sa_handler = term_catcher;
	sigfillset(&act.sa_mask);
	sigaction(SIGTERM, &act, 0);
	act.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &act, 0);
#endif
#if defined(__WIN32__)
	int wsaerr;
#endif

	path = htxf->path;

	sa = htxf->sockaddr;
	s = htxf->fd;

	inaddr2str(abuf, &sa);

	hxd_log("htxf get %s from %s:%u", path, abuf, ntohs(sa.SIN_PORT));

	if (htxf->gone)
		goto ret_nobuf;

#if defined(CONFIG_HTXF_QUEUE)
	while (htxf->queue_pos) {
		delay++;
		if (delay == 60) {		/* after every 2 minutes send empty packet */
			socket_write(s, "", 0);	/* to avoid timeout in masqueraded connections */
			delay = 0;
		}
		r = fd_select(s, 2);
		if (htxf->gone)
			goto ret_nobuf;
		if (r == 0)
			continue;
		else
			goto ret_nobuf;

		if (htxf->gone)
			goto ret_nobuf;
	}
#endif
	gettimeofday(&htxf->start, 0);

	buf = malloc(HTXF_BUFSIZE + 1024);
	if (!buf) {
		hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
		retval = errno;
		goto ret_nobuf;
	}
#if defined(CONFIG_HTXF_PTHREAD)
	pthread_cleanup_push(free, buf);
#endif
	if (htxf->gone)
		goto ret;

	if (htxf->limit_out_Bps)
		wp = watch_new();
#if defined(CONFIG_HTXF_PTHREAD)
	pthread_cleanup_push(watch_delete, wp);
#endif

	if (htxf->type == HTXF_TYPE_FILE) {
		r = file_send(htxf, buf, wp);
	} else if (htxf->type == HTXF_TYPE_BANNER) {
		r = file_send(htxf, buf, wp);
	} else if (htxf->type == HTXF_TYPE_FOLDER) {
		r = folder_send(htxf, buf, wp);
	} else {
		r = -1;
	}
	if (r == -1)
		goto ret_err;
	else if (r == 1)
		goto ret;
#ifdef CONFIG_WINDOS_CLIENT_FIX
	fd_select(s, 3);
#endif
	retval = 0;
#if defined(CONFIG_HTXF_PTHREAD)
	/* buf */
	pthread_cleanup_pop(1);
	/* watch */
	pthread_cleanup_pop(1);
#endif
	goto ret;
ret_err:
	retval = errno;
ret:
	t_free(buf);
ret_nobuf:
	t_watch_delete(wp);
	socket_close(s);
	hxd_log("closed htxf get %s from %s:%u",
		path, abuf, ntohs(sa.SIN_PORT));

	htxf->gone = 1;
#if !defined(__WIN32__)
	kill(hxd_pid, SIGALRM);
#endif

#if defined(CONFIG_HTXF_PTHREAD)
	pthread_exit((thread_return_type)retval);
#else
	return (thread_return_type)retval;
#endif
}

static int
b_read (int fd, void *bufp, size_t len)
{
	register u_int8_t *buf = (u_int8_t *)bufp;
	register int r, pos = 0;

	while (len) {
		if ((r = socket_read(fd, &(buf[pos]), len)) <= 0)
			return -1;
		pos += r;
		len -= r;
	}

	return pos;
}

static int
file_recv (struct htxf_conn *htxf, u_int8_t *buf, u_int32_t file_size)
{
	u_int32_t data_size, data_pos, rsrc_size, rsrc_pos;
	u_int16_t preview;
	struct SOCKADDR_IN sa;
	int r, len, s, f;
	char *path;
#if defined(CONFIG_HFS)
	int use_hfs = hxd_cfg.operation.hfs;
	struct hfsinfo fi;
#endif
	char typecrea[8];
	u_int32_t tot_pos, tot_len = file_size;

	data_size = htxf->data_size;
	data_pos = htxf->data_pos;
	rsrc_size = htxf->rsrc_size;
	rsrc_pos = htxf->rsrc_pos;
	preview = htxf->preview;
	path = htxf->path;
	sa = htxf->sockaddr;
	s = htxf->fd;

	len = 40;
	if ((r = b_read(s, buf, len)) != len) {
		if (r <= 0)
			hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
		else
			hxd_log("%s:%d: read %d but expected %d", __FILE__, __LINE__, r, len);
		return -1;
	}
	if (htxf->gone)
		return 1;
	len = (buf[38] ? 0x100 : 0) + buf[39] + 16;
	if ((r = b_read(s, buf, len)) != len) {
		if (r <= 0)
			hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
		else
			hxd_log("%s:%d: read %d but expected %d", __FILE__, __LINE__, r, len);
		return -1;
	}
	if (htxf->gone)
		return 1;
	L32NTOH(data_size, &buf[r - 4]);
	data_size += data_pos;
	htxf->data_size = data_size;
	tot_pos = 40 + r;
	memcpy(typecrea, &buf[4], 8);

	htxf->total_pos = tot_pos;

#if defined(CONFIG_HFS)
	if (use_hfs) {
		memset(&fi, 0, sizeof(struct hfsinfo));
		fi.comlen = buf[73 + buf[71]];
		memcpy(fi.type, "HTftHTLC", 8);
		memcpy(fi.comment, &buf[74 + buf[71]], fi.comlen);
		fi.create_time = hfs_m_to_htime(*((u_int32_t *)(&buf[56])));
		fi.modify_time = hfs_m_to_htime(*((u_int32_t *)(&buf[64])));
		fi.rsrclen = rsrc_pos;
		hfsinfo_write(path, &fi);
	}
#endif

	f = SYS_open(path, O_WRONLY|O_CREAT, hxd_cfg.permissions.files);
	if (f < 0) {
		hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
		return -1;
	}
	if (data_size > data_pos) {
		if (htxf->gone) {
			close(f);
			return 1;
		}
		if (fd_lock_write(f) == -1) {
#if !defined(__OpenBSD__)//OpenBSD has different errno references... phew.
			if (errno != ENOTSUP) {
#else
			if (errno != EOPNOTSUPP) {
#endif
				hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
				close(f);
				return -1;
			}
		}
		if (data_pos)
			if (lseek(f, data_pos, SEEK_SET) != (off_t)data_pos) {
				hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
				close(f);
				return -1;
			}
		for (;;) {
			r = socket_read(s, buf, HTXF_BUFSIZE > (data_size - data_pos) ? (data_size - data_pos) : HTXF_BUFSIZE);
			if (r <= 0) {
				hxd_log("%s:%d: r == %d; %s (%d)", __FILE__, __LINE__, r, strerror(errno), errno);
				close(f);
				return -1;
			}
			if (htxf->gone) {
				close(f);
				return 1;
			}
			if (write(f, buf, r) != r) {
				hxd_log("%s:%d: r == %d; %s (%d)", __FILE__, __LINE__, r, strerror(errno), errno);
				close(f);
				return -1;
			}
			if (htxf->gone) {
				close(f);
				return 1;
			}
			data_pos += r;
			tot_pos += r;
			htxf->data_pos = data_pos;
			htxf->total_pos = tot_pos;
			if (data_pos >= data_size)
				break;
		}
		SYS_fsync(f);
	}
	close(f);
	if (htxf->gone)
		return 1;
#if defined(CONFIG_HFS)
	if (!use_hfs || tot_pos >= tot_len)
		goto done;
	if (socket_read(s, buf, 16) != 16) {
		hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
		hxd_log("tot_pos: %u  tot_len: %u", tot_pos, tot_len);
		return -1;
	}
	L32NTOH(rsrc_size, &buf[12]);
	rsrc_size += rsrc_pos;
	htxf->rsrc_size = rsrc_size;
	if (rsrc_size > rsrc_pos) {
		if (htxf->gone)
			return 1;
		f = resource_open(path, /*O_WRONLY*/O_RDWR|O_CREAT, hxd_cfg.permissions.files);
		if (f < 0) {
			hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
			return -1;
		}
		if (fd_lock_write(f) == -1) {
#if !defined(__OpenBSD__)//OpenBSD has different errno references... phew.
			if (errno != ENOTSUP) {
#else
			if (errno != EOPNOTSUPP) {
#endif
				hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
				close(f);
				return -1;
			}
		}
		if (rsrc_pos) {
			if (lseek(f, rsrc_pos, SEEK_CUR) == -1) {
				hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
				close(f);
				return -1;
			}
		}
		if (htxf->gone) {
			close(f);
			return 1;
		}
		tot_pos += (rsrc_size - rsrc_pos);
		for (;;) {
			r = socket_read(s, buf, HTXF_BUFSIZE > (rsrc_size - rsrc_pos) ? (rsrc_size - rsrc_pos) : HTXF_BUFSIZE);
			if (r <= 0) {
				hxd_log("%s:%d: r == %d; %s (%d)", __FILE__, __LINE__, r, strerror(errno), errno);
				close(f);
				hfsinfo_write(path, &fi);
				return -1;
			}
			if (htxf->gone) {
				close(f);
				hfsinfo_write(path, &fi);
				return 1;
			}
			if (write(f, buf, r) != r) {
				hxd_log("%s:%d: r == %d; %s (%d)", __FILE__, __LINE__, r, strerror(errno), errno);
				close(f);
				hfsinfo_write(path, &fi);
				return -1;
			}
			rsrc_pos += r;
			tot_pos += r;
			fi.rsrclen = rsrc_pos;
			htxf->rsrc_pos = rsrc_pos;
			htxf->total_pos = tot_pos;
			if (rsrc_pos >= rsrc_size)
				break;
			if (htxf->gone) {
				close(f);
				hfsinfo_write(path, &fi);
				return 1;
			}
		}
		SYS_fsync(f);
		close(f);
	}
done:
	memcpy(fi.type, typecrea, 8);
	hfsinfo_write(path, &fi);
#endif /* CONFIG_HFS */

	return 0;
}

static int
folder_recv (struct htxf_conn *htxf, u_int8_t *buf)
{
	int r, s = htxf->fd;
	u_int16_t cmd, i;
	u_int8_t rflt[128];
	u_int32_t data_pos, rsrc_pos, size;
	struct next_file_info nfi;
	u_int8_t fname[256], dirpath[MAXPATHLEN], fpath[MAXPATHLEN], fnlen;
	int retval;
	int fplen;
	struct stat sb;

	strcpy(dirpath, htxf->path);
	for (;;) {
		cmd = htons(FILE_NEXT);
		r = socket_write(s, &cmd, sizeof(cmd));
		if (r != sizeof(cmd)) {
			retval = -1;
			goto ret;
		}
		r = socket_read(s, &nfi, 6);
		if (r != 6) {
			retval = -1;
			goto ret;
		}
		nfi.len = ntohs(nfi.len);
		nfi.type = ntohs(nfi.type);
		nfi.pathcount = ntohs(nfi.pathcount);
		strcpy(fpath, dirpath);
		fplen = strlen(fpath);
		for (i = 0; i < nfi.pathcount; i++) {
			r = socket_read(s, buf, 3);
			if (r != 3) {
				retval = -1;
				goto ret;
			}
			fnlen = buf[2];
			r = socket_read(s, fname, fnlen);
			if (r != fnlen) {
				retval = -1;
				goto ret;
			}
			fname[fnlen] = 0;
#define DIRCHAR '/'
			if ((fnlen == 2 && fname[0] == '.' && fname[1] == '.')
			    || memchr(fname, DIRCHAR, fnlen)) {
				retval = -1;
				goto ret;
			}
			r = snprintf(fpath+fplen, sizeof(fpath)-fplen, "/%s", fname);
			if (r == -1) {
				retval = -1;
				goto ret;
			}
			fplen += r;
		}
#ifdef HAVE_CORESERVICES
		resolve_alias_path(fpath, fpath);
#endif
		hxd_log("folder upload %s: %s", nfi.type==1?"folder":"file", fpath);
		if (nfi.type == 1) {
			if (mkdir(fpath, hxd_cfg.permissions.directories)) {
				retval = -1;
				goto ret;
			}
			continue;
		}
		if (stat(fpath, &sb)) {
			cmd = htons(FILE_SEND);
			r = socket_write(s, &cmd, sizeof(cmd));
			if (r != sizeof(cmd)) {
				retval = -1;
				goto ret;
			}
			htxf->data_pos = 0;
			htxf->rsrc_pos = 0;
		} else {
			data_pos = sb.st_size;
#if defined(CONFIG_HFS)
			if (hxd_cfg.operation.hfs)
				rsrc_pos = resource_len(fpath);
			else
#endif
				rsrc_pos = 0;
			memcpy(rflt+4, "RFLT\0\1\
\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\
\0\0\0\2DATA\0\0\0\0\0\0\0\0\0\0\0\0MACR\0\0\0\0\0\0\0\0\0\0\0\0", 74);
				S32HTON(data_pos, &rflt[46 + 4]);
				S32HTON(rsrc_pos, &rflt[62 + 4]);
			S16HTON(FILE_RESUME, &rflt[0]);
			S16HTON(74, &rflt[2]);
			r = socket_write(s, rflt, 78);
			if (r != 78) {
				retval = -1;
				goto ret;
			}
			htxf->data_pos = data_pos;
			htxf->rsrc_pos = rsrc_pos;
		}
		r = socket_read(s, &size, sizeof(size));
		if (r != sizeof(size)) {
			retval = -1;
			goto ret;
		}
		size = ntohl(size);
		strcpy(htxf->path, fpath);
		r = file_recv(htxf, buf, size);
		if (r) {
			retval = r;
			goto ret;
		}
	}

ret:
	strcpy(htxf->path, dirpath);
	return retval;
}

static thread_return_type
put_thread (void *__arg)
{
	struct htxf_conn *htxf = (struct htxf_conn *)__arg;
	char *path = htxf->path;
	int s, r, retval = 0;
	struct SOCKADDR_IN sa;
	u_int8_t *buf;
	char abuf[HOSTLEN+1];
#if !defined(__WIN32__) && (defined(CONFIG_HTXF_CLONE) || defined(CONFIG_HTXF_FORK))
	struct sigaction act;

	act.sa_flags = 0;
	act.sa_handler = term_catcher;
	sigfillset(&act.sa_mask);
	sigaction(SIGTERM, &act, 0);
	act.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &act, 0);
#endif
#if defined(__WIN32__)
	int wsaerr;
#endif

	sa = htxf->sockaddr;
	s = htxf->fd;

	if (htxf->gone)
		goto ret_nobuf;

	inaddr2str(abuf, &sa);
	hxd_log("htxf put %s from %s:%u", path, abuf, ntohs(sa.SIN_PORT));

	buf = malloc(HTXF_BUFSIZE + 1024);
	if (!buf) {
		hxd_log("%s:%d: %s (%d)", __FILE__, __LINE__, strerror(errno), errno);
		retval = errno;
		goto ret_nobuf;
	}

	if (htxf->type == HTXF_TYPE_FILE) {
		r = file_recv(htxf, buf, htxf->total_size);
	} else if (htxf->type == HTXF_TYPE_BANNER) {
		r = file_recv(htxf, buf, htxf->total_size);
	} else if (htxf->type == HTXF_TYPE_FOLDER) {
		r = folder_recv(htxf, buf);
	} else {
		r = -1;
	}
	if (r == -1)
		goto ret_err;
	else if (r == 1)
		goto ret;

#ifdef CONFIG_WINDOS_CLIENT_FIX
	socket_blocking(s, 0);
	r = 0;
#if defined(__WIN32__)
	wsaerr = WSAGetLastError();
	while (socket_read(s, buf, HTXF_BUFSIZE) && wsaerr == WSAEWOULDBLOCK) {
		wsaerr = WSAGetLastError();
#else
	while (socket_read(s, buf, HTXF_BUFSIZE) && errno == EWOULDBLOCK) {
#endif
		if (r++ > 3)
			break;
		sleep(1);
	}
#endif
	retval = 0;
	goto ret;
ret_err:
	retval = errno;
ret:
	t_free(buf);
ret_nobuf:
	socket_close(s);

	hxd_log("closed htxf put %s from %s:%u", path, abuf, ntohs(sa.SIN_PORT));

	htxf->gone = 1;
#if !defined(__WIN32__)
	kill(hxd_pid, SIGALRM);
#endif

#if defined(CONFIG_HTXF_PTHREAD)
	pthread_exit((thread_return_type)retval);
#else
	return (thread_return_type)retval;
#endif
}

static void
htxf_free (int fd)
{
	struct htxf_conn *htxf = hxd_files[fd].conn.htxf;

	hxd_fd_clr(fd, FDR|FDW);
	hxd_fd_del(fd);
	memset(&hxd_files[fd], 0, sizeof(struct hxd_file));
	if (!htxf)
		return;
	timer_delete_ptr(htxf);
	if (htxf->in.buf)
		xfree(htxf->in.buf);
	xfree(htxf);
}

static void
got_hdr (struct htxf_conn *htxf, struct htxf_hdr *h)
{
	int fd = htxf->fd;
	int err;
	char abuf[HOSTLEN+1], bbuf[HOSTLEN+1];
	unsigned int i;
	u_int32_t ref;
	struct htlc_conn *htlcp;

	timer_delete_ptr(htxf);
	if (ntohl(h->magic) != HTXF_MAGIC_INT) {
		socket_close(fd);
		htxf_free(fd);
		inaddr2str(abuf, &htxf->sockaddr);
		hxd_log("%s:%u -- htxf connection closed", abuf, ntohs(htxf->sockaddr.SIN_PORT));
		return;
	}
	ref = h->ref;
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		mutex_lock(&htlcp->htxf_mutex);
		for (i = 0; i < HTXF_GET_MAX; i++) {
			if (htlcp->htxf_out[i] && htlcp->htxf_out[i]->ref == ref) {
#ifdef CONFIG_IPV6 /* this is a strange hack */
				inet_ntop(AFINET, (char *)&htxf->sockaddr.SIN_ADDR, abuf, sizeof(abuf));
				inet_ntop(AFINET, (char *)&htlcp->htxf_out[i]->sockaddr.SIN_ADDR.S_ADDR, bbuf, sizeof(bbuf));
				if (strcmp(abuf, bbuf)) {
#else
				if (htxf->sockaddr.SIN_ADDR.S_ADDR
				    != htlcp->htxf_out[i]->sockaddr.SIN_ADDR.S_ADDR) {
					inet_ntoa_r(htxf->sockaddr.SIN_ADDR, abuf, sizeof(abuf));
					inet_ntoa_r(htlcp->htxf_out[i]->sockaddr.SIN_ADDR, bbuf, sizeof(bbuf));
#endif
					hxd_log("attempt to download %x from different address (%s) (expected %s)", ntohl(ref), abuf, bbuf);
					socket_close(fd);
					htxf_free(fd);
					xfree(htlcp->htxf_out[i]);
					htlcp->htxf_out[i] = 0;
					htlcp->nr_gets--;
					nr_gets--;
					mutex_unlock(&htlcp->htxf_mutex);
					return;

				}
				htxf_free(fd);
				socket_blocking(fd, 1);
				htlcp->htxf_out[i]->fd = fd;
				gettimeofday(&htlcp->htxf_out[i]->start, 0);
				err = hxd_thread_create(&htxf->tid, &htxf->stack, get_thread, htlcp->htxf_out[i]);
				if (err) {
					hxd_log("%s:%u -- htxf connection closed: thread error %s",
						abuf, ntohs(htxf->sockaddr.SIN_PORT), strerror(err));
					xfree(htlcp->htxf_out[i]);
					htlcp->htxf_out[i] = 0;
					htlcp->nr_gets--;
					nr_gets--;
					mutex_unlock(&htlcp->htxf_mutex);
					return;
				}
				htlcp->htxf_out[i]->tid = htxf->tid;
				mutex_unlock(&htlcp->htxf_mutex);
				return;
			}
		}
		for (i = 0; i < HTXF_PUT_MAX; i++) {
		   	if (htlcp->htxf_in[i] && htlcp->htxf_in[i]->ref == ref) {
				if (htxf->sockaddr.SIN_ADDR.S_ADDR
				    != htlcp->htxf_in[i]->sockaddr.SIN_ADDR.S_ADDR) {
					inaddr2str(abuf, &htxf->sockaddr);
					hxd_log("attempt to upload %x from wrong address (%s)", ntohl(ref), abuf);
					socket_close(fd);
					htxf_free(fd);
					xfree(htlcp->htxf_in[i]);
					htlcp->htxf_in[i] = 0;
					htlcp->nr_puts--;
					nr_puts--;
					mutex_unlock(&htlcp->htxf_mutex);
					return;
				}
				socket_blocking(fd, 1);
				htxf_free(fd);
				hxd_fd_clr(fd, FDR|FDW);
				hxd_fd_del(fd);
				htlcp->htxf_in[i]->fd = fd;
				htlcp->htxf_in[i]->total_size = ntohl(h->len);
				gettimeofday(&htlcp->htxf_in[i]->start, 0);
				err = hxd_thread_create(&htxf->tid, &htxf->stack, put_thread, htlcp->htxf_in[i]);
				if (err) {
					socket_close(fd);
					hxd_log("%s:%u -- htxf connection closed: thread error %s",
						abuf, ntohs(htxf->sockaddr.SIN_PORT), strerror(err));
					xfree(htlcp->htxf_in[i]);
					htlcp->htxf_in[i] = 0;
					htlcp->nr_puts--;
					nr_puts--;
					mutex_unlock(&htlcp->htxf_mutex);
					return;
				}
				htlcp->htxf_in[i]->tid = htxf->tid;
				mutex_unlock(&htlcp->htxf_mutex);
				return;
			}
		}
		mutex_unlock(&htlcp->htxf_mutex);
	}
	htxf_free(fd);
	socket_close(fd);
	hxd_log("%s:%u -- htxf connection closed: no ref %x",
		abuf, ntohs(htxf->sockaddr.SIN_PORT), ref);
}

static void
htxf_read_hdr (int fd)
{
	struct htxf_conn *htxf = hxd_files[fd].conn.htxf;
	char abuf[HOSTLEN+1];
	int r;
#if defined(__WIN32__)
	int wsaerr;
#endif

	r = socket_read(fd, &htxf->in.buf[htxf->in.pos], htxf->in.len);
	if (r <= 0) {
#if defined(__WIN32__)
		wsaerr = WSAGetLastError();
		if (r == 0 || (r < 0 && wsaerr != WSAEWOULDBLOCK && errno != EINTR)) {
#else
		if (r == 0 || (r < 0 && errno != EWOULDBLOCK && errno != EINTR)) {
#endif
			socket_close(fd);
			htxf_free(fd);
			inaddr2str(abuf, &htxf->sockaddr);
			hxd_log("%s:%u -- htxf connection closed: read error: %s",
				abuf, ntohs(htxf->sockaddr.SIN_PORT), strerror(errno));
		}
	} else {
		htxf->in.pos += r;
		htxf->in.len -= r;
		if (!htxf->in.len)
			got_hdr(htxf, (struct htxf_hdr *)htxf->in.buf);
	}
}

static int
htxf_timeout (struct htxf_conn *htxf)
{
	char abuf[HOSTLEN+1];

	inaddr2str(abuf, &htxf->sockaddr);
	hxd_log("%s:%u -- htxf connection closed: timed out",
		abuf, ntohs(htxf->sockaddr.SIN_PORT));
	socket_close(htxf->fd);
	htxf_free(htxf->fd);

	return 0;
}

static void
htxf_listen_ready_read (int fd)
{
	int s;
	struct SOCKADDR_IN saddr;
	int siz = sizeof(saddr);
	char abuf[HOSTLEN+1];
	struct htxf_conn *htxf;

	s = accept(fd, (struct sockaddr *)&saddr, &siz);
	if (s < 0) {
		hxd_log("htxf: accept: %s", strerror(errno));
		return;
	}
	if (s >= hxd_open_max) {
		hxd_log("%s:%d: %d >= hxd_open_max (%d)", __FILE__, __LINE__, s, hxd_open_max);
		close(s);
		return;
	}
	fd_closeonexec(s, 1);
	socket_blocking(s, 0);
	inaddr2str(abuf, &saddr);
	hxd_log("%s:%u -- htxf connection accepted", abuf, ntohs(saddr.SIN_PORT));

	htxf = xmalloc(sizeof(struct htxf_conn));
	memset(htxf, 0, sizeof(struct htxf_conn));
	htxf->fd = s;
	htxf->sockaddr = saddr;
	qbuf_set(&htxf->in, 0, SIZEOF_HTXF_HDR);
	hxd_fd_add(s);
	hxd_fd_set(s, FDR);

	hxd_files[s].ready_read = htxf_read_hdr;
	hxd_files[s].ready_write = 0;
	hxd_files[s].conn.htxf = htxf;

	timer_add_secs(8, htxf_timeout, htxf);
}

void
htxf_init (struct SOCKADDR_IN *saddr)
{
	int x, r, ls;

	ls = socket(AFINET, SOCK_STREAM, IPPROTO_TCP);
	if (ls < 0) {
		hxd_log("%s:%d: socket: %s", __FILE__, __LINE__, strerror(errno));
		exit(1);
	}
	if (ls >= hxd_open_max) {
		hxd_log("%s:%d: %d >= hxd_open_max (%d)", __FILE__, __LINE__, ls, hxd_open_max);
		close(ls);
		exit(1);
	}
	x = 1;
	setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (SETSOCKOPT_PTR_CAST_T)&x, sizeof(x));
	saddr->SIN_PORT = htons(ntohs(saddr->SIN_PORT) + 1);

#ifdef CONFIG_EUID
	if (hxd_cfg.options.htls_port < 1024) {
		r = seteuid(0);
		if (r)
			hxd_log("setuid(0): %s", strerror(errno));
	}
#endif
	r = bind(ls, (struct sockaddr *)saddr, sizeof(struct SOCKADDR_IN));
#ifdef CONFIG_EUID
	if (hxd_cfg.options.htls_port < 1024)
		seteuid(getuid());
#endif
	if (r < 0) {
		hxd_log("%s:%d: bind: %s", __FILE__, __LINE__, strerror(errno));
		exit(1);
	}
	if (listen(ls, 5) < 0) {
		hxd_log("%s:%d: listen: %s", __FILE__, __LINE__, strerror(errno));
		exit(1);
	}

	hxd_files[ls].ready_read = htxf_listen_ready_read;
	hxd_fd_add(ls);
	hxd_fd_set(ls, FDR);
	fd_closeonexec(ls, 1);
	socket_blocking(ls, 0);
}
