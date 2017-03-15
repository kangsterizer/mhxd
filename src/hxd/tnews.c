#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "hlserver.h"
#include "xmalloc.h"
#include "getline.h"
#if defined(CONFIG_ICONV)
#include "conv.h"
#endif

#define DIRCHAR			'/'

#define log_mknewsbundle	1
#define log_deletebundle	1
#define log_deletethread	1
#define log_mknewscategory	1

/* in files.c */
extern int hldir_to_path (struct hl_data_hdr *dh, const char *root, char *rlpath, char *pathp);

/* categories are implemented as directories with names starting with "cat_" ... */

static int
cat_to_path (struct hl_data_hdr *dh, const char *root, char *rlpath, char *pathp)
{
	u_int16_t rootlen, pos, count;
	u_int8_t *p, nlen;
	char path[MAXPATHLEN];
	int err = 0;
	u_int16_t dh_len;

	rootlen = pos = strlen(root);
	memcpy(path, root, pos);
	path[pos++] = DIRCHAR;
	L16NTOH(count, dh->data);
	if (count > 32)
		count = 32;

	L16NTOH(dh_len, &dh->len);
	if (dh_len < 5) {
		pathp[0] = '/';
		pathp[1] = 0;
		strcpy(rlpath, root);
		return 0;
	}
	p = &dh->data[4];
	for (;;) {
		nlen = *p;
		p++;
		if (pos+6+nlen >= MAXPATHLEN) {
			return ENAMETOOLONG;
		}
		/* do not allow ".." or DIRCHAR */
		if (!(nlen == 2 && p[0] == '.' && p[1] == '.') && !memchr(p, DIRCHAR, nlen)) {
			if (count == 1) {
				memcpy(&path[pos], "cat_", 4);
				pos += 4;
			}
#if defined(CONFIG_ICONV)
			char *out_p;
			size_t out_len;

			if (hxd_cfg.text.client_encoding[0] && hxd_cfg.text.server_encoding[0]) {
				out_len = convbuf(hxd_cfg.text.server_encoding,
						  hxd_cfg.text.client_encoding,
						  p, (size_t)nlen, &out_p);
				if (out_len) {
					if (out_len > (size_t)nlen)
						out_len = (size_t)nlen;
					memcpy(&path[pos], out_p, out_len);
					xfree(out_p);
				} else {
					memcpy(&path[pos], p, nlen);
				}
			} else
#endif
			{
				memcpy(&path[pos], p, nlen);
			}
			pos += nlen;
		}
		count--;
		if (count) {
			path[pos++] = DIRCHAR;
			p += 2 + nlen;
		} else {
			path[pos] = 0;
			break;
		}
	}
	if (!sys_realpath(path, rlpath))
		err = errno;
	path[MAXPATHLEN-1] = 0;
	strcpy(pathp, path);

	return err;
}

static void
snd_news_dirlist (struct htlc_conn *htlc, struct hl_newslist_extended_hdr **fhdrs, u_int16_t fhdrcount)
{
	struct hl_hdr h;
	u_int16_t i;
	u_int32_t pos, this_off, len = SIZEOF_HL_HDR;

	h.type = htonl(HTLS_HDR_TASK);
	h.trans = htonl(htlc->replytrans);
	/* htlc->replytrans++; */
	h.flag = 0;
	h.hc = htons(fhdrcount);

	for (i = 0; i < fhdrcount; i++)
		len += SIZEOF_HL_DATA_HDR + ntohs(fhdrs[i]->len);
	h.len = htonl(len - (SIZEOF_HL_HDR - sizeof(h.hc)));
	h.totlen = h.len;
	pos = htlc->out.pos + htlc->out.len;
	this_off = pos;
	qbuf_set(&htlc->out, htlc->out.pos, htlc->out.len + len);
	memcpy(&htlc->out.buf[pos], &h, SIZEOF_HL_HDR);
	pos += SIZEOF_HL_HDR;
	FD_SET(htlc->fd, &hxd_wfds);

	for (i = 0; i < fhdrcount; i++) {
		memcpy(&htlc->out.buf[pos], fhdrs[i], SIZEOF_HL_DATA_HDR + ntohs(fhdrs[i]->len));
		pos += SIZEOF_HL_DATA_HDR + ntohs(fhdrs[i]->len);
	}

	len = pos - this_off;
#ifdef CONFIG_COMPRESS
	if (htlc->compress_encode_type != COMPRESS_NONE)
		len = compress_encode(htlc, this_off, len);
#endif
#ifdef CONFIG_CIPHER
	if (htlc->cipher_encode_type != CIPHER_NONE)
		cipher_encode(htlc, this_off, len);
#endif
}


static int
enum_dir_contents (const char *dirname, const char *sub)
{
	char path[MAXPATHLEN];
	DIR *dir;
	struct dirent *de;
	int count = 0;

	if (sub)
		snprintf(path, sizeof(path), "%s/%s", dirname, sub);
	else
		strcpy(path, dirname);

	if (!(dir = opendir(path)))
		return 0;
	while ((de = readdir(dir))) {
		/* skip files whose names start with '.' */
		if (de->d_name[0] == '.')
			continue;
		count++;
	}
	closedir(dir);

	return count;
}

/* ref: hxd_scandir */
void
tnews_send_dirlist (struct htlc_conn *htlc, const char *path)
{
	u_int8_t nlen, ntype;
	u_int16_t count = 0, maxcount = 0;
	struct hl_newslist_extended_hdr **fhdrs = 0;
	DIR *dir;
	struct dirent *de;
	struct stat sb;
	char pathbuf[MAXPATHLEN];

	if (!(dir = opendir(path))) {
		snd_strerror(htlc, errno);
		return;
	}
	while ((de = readdir(dir))) {
		/* skip files whose names start with '.' */
		if (de->d_name[0] == '.')
			continue;
		snprintf(pathbuf, sizeof(pathbuf), "%s/%s", path, de->d_name);
		if (SYS_lstat(pathbuf, &sb)) {
			continue;
		}
		/* skip files */
		if (!S_ISDIR(sb.st_mode))
			continue;
		if (count >= maxcount) {
			maxcount += 16;
			fhdrs = xrealloc(fhdrs, sizeof(struct hl_newslist_hdr *) * maxcount);
		}
		nlen = (u_int8_t)strlen(de->d_name);
		if (!strncmp(de->d_name, "cat_", 4)) {
			nlen -= 4;
			ntype = 3;
		} else {
			ntype = 2;
		}
		/* a bundle */
		if (ntype == 2) {
			fhdrs[count] = xmalloc(SIZEOF_HL_NEWSLIST_BUNDLE_HDR + nlen);
			fhdrs[count]->type = htons(HTLS_DATA_NEWS_DIRLIST_EXTENDED);
			fhdrs[count]->len = htons((u_int16_t)nlen + SIZEOF_HL_NEWSLIST_BUNDLE_HDR);
			fhdrs[count]->ntype = htons(2);
			/* number of subdirs */
			fhdrs[count]->count = htons(enum_dir_contents(path, de->d_name));
			((struct hl_newslist_bundle_hdr **)fhdrs)[count]->namelen = nlen;
			memcpy(((struct hl_newslist_bundle_hdr **)fhdrs)[count]->name, de->d_name, nlen);
		} else { /* a category */
			/* + 4 for the data header */
			fhdrs[count] = xmalloc(SIZEOF_HL_NEWSLIST_CATEGORY_HDR + nlen + 4);
			memset(fhdrs[count], 0, SIZEOF_HL_NEWSLIST_CATEGORY_HDR + nlen + 4);
			fhdrs[count]->type = htons(HTLS_DATA_NEWS_DIRLIST_EXTENDED);
			fhdrs[count]->len = htons((u_int16_t)nlen + SIZEOF_HL_NEWSLIST_CATEGORY_HDR);
			fhdrs[count]->ntype = htons(3);
			/* number of posts */
			fhdrs[count]->count = htons(enum_dir_contents(path, de->d_name));
			((struct hl_newslist_category_hdr **)fhdrs)[count]->namelen = nlen;
			memcpy(((struct hl_newslist_category_hdr **)fhdrs)[count]->name, de->d_name+4, nlen);
		}
		count++;
	}
	closedir(dir);
	snd_news_dirlist(htlc, fhdrs, count);
	while (count) {
		count--;
		xfree(fhdrs[count]);
	}
	xfree(fhdrs);
}

struct thread_hdr {
	u_int32_t id;
	u_int32_t parid;
	u_int32_t len;
	struct hl_news_thread_hdr *hdr;
};

struct faketree_el {
	u_int16_t n;
	u_int16_t *ti;
};

struct faketree {
	u_int16_t n;
	struct faketree_el *ts;
};

#define new_thread(_t, _idx)							\
do {										\
	(_t)->ts = xrealloc((_t)->ts, sizeof(struct faketree_el)*((_t)->n+1));	\
	(_t)->ts[(_t)->n].n = 1;						\
	(_t)->ts[(_t)->n].ti = xmalloc(sizeof(u_int16_t));			\
	(_t)->ts[(_t)->n].ti[0] = _idx;						\
	(_t)->n++;								\
} while (0)

#define addto_thread(_t, _paridx, _idx)					\
do {									\
	(_t)->ti = xrealloc((_t)->ti, sizeof(u_int16_t *)*((_t)->n+1));	\
	if (_paridx != (_t)->n-1)					\
		memmove(&(_t)->ti[_paridx+2],				\
			&(_t)->ti[_paridx+1],				\
			sizeof(u_int16_t)*((_t)->n-1-_paridx));		\
	(_t)->ti[_paridx+1] = _idx;					\
	(_t)->n++;							\
} while (0)

static struct faketree_el *
find_thread_first (u_int32_t id, u_int32_t thidx, struct faketree *tree, struct thread_hdr *ths)
{
	u_int16_t i;

	for (i = 0; i < tree->n; i++) {
		if (ths[tree->ts[i].ti[0]].id == id) {
			return &tree->ts[i];
		}
	}
	new_thread(tree, thidx);

	return &tree->ts[tree->n-1];
}

/* may recurse */
static struct faketree_el *
find_thread (u_int32_t parid, u_int16_t thidx, struct faketree *tree, struct thread_hdr *ths, u_int16_t nth)
{
	struct thread_hdr *th;
	u_int16_t i, j;

findintree:
	for (i = 0; i < tree->n; i++) {
		for (j = 0; j < tree->ts[i].n; j++) {
			if (ths[tree->ts[i].ti[j]].id == parid) {
				addto_thread(&tree->ts[i], j, thidx);
				return &tree->ts[i];
			}
		}
	}
	for (th = ths, i = 0; i < nth; i++, th++) {
		if (th->id == parid) {
			if (!th->parid) {
				new_thread(tree, i);
				goto findintree;
			}
			return find_thread(th->parid, i, tree, ths, nth);
		}
	}

	return 0;
}

static void
sort_threads (struct thread_hdr *ths, u_int16_t nth)
{
	struct faketree tree;
	struct faketree_el orphans;
	struct thread_hdr *th;
	u_int16_t i, j, k, idx;
	struct thread_hdr *newths;

	tree.n = 0;
	tree.ts = 0;
	orphans.n = 0;
	orphans.ti = 0;
	for (th = ths, i = 0; i < nth; i++, th++) {
		if (!th->parid) {
			find_thread_first(th->id, i, &tree, ths);
		} else {
			if (!find_thread(th->parid, i, &tree, ths, nth))
				addto_thread(&orphans, orphans.n-1, i);
		}
	}
	newths = xmalloc(sizeof(struct thread_hdr)*nth);
	k = 0;
	if (tree.ts) {
		for (i = 0; i < tree.n; i++) {
			for (j = 0; j < tree.ts[i].n; j++) {
				idx = tree.ts[i].ti[j];
				newths[k] = ths[idx];
				k++;
			}
			xfree(tree.ts[i].ti);
		}
		xfree(tree.ts);
	}
	if (orphans.ti) {
		for (i = 0; i < orphans.n; i++) {
			idx = orphans.ti[i];
			newths[k] = ths[idx];
			k++;
		}
		xfree(orphans.ti);
	}
	memcpy(ths, newths, sizeof(struct thread_hdr)*nth);
	xfree(newths);
}

static void
snd_news_catlist (struct htlc_conn *htlc, struct thread_hdr *fhdrs, u_int16_t fhdrcount)
{
	struct hl_news_threadlist_hdr *tlh;
	struct hl_news_thread_hdr *fh;
	struct hl_hdr h;
	u_int16_t i;
	u_int32_t pos, this_off, len = SIZEOF_HL_HDR;

	h.type = htonl(HTLS_HDR_TASK);
	h.trans = htonl(htlc->replytrans);
	/* htlc->replytrans++; */
	h.flag = 0;
	h.hc = htons(fhdrcount);

	len += SIZEOF_HL_NEWS_THREADLIST_HDR;
	for (i = 0; i < fhdrcount; i++)
		len += SIZEOF_HL_NEWS_THREAD_HDR + 5 + fhdrs[i].len;
	h.len = htonl(len - (SIZEOF_HL_HDR - sizeof(h.hc)));
	h.totlen = h.len;
	pos = htlc->out.pos + htlc->out.len;
	this_off = pos;
	qbuf_set(&htlc->out, htlc->out.pos, htlc->out.len + len);
	memcpy(&htlc->out.buf[pos], &h, SIZEOF_HL_HDR);
	pos += SIZEOF_HL_HDR;
	tlh = (struct hl_news_threadlist_hdr *)&htlc->out.buf[pos];
	tlh->type = htons(HTLS_DATA_NEWS_CATLIST);
	tlh->len = htons((u_int16_t)len - (SIZEOF_HL_HDR + SIZEOF_HL_DATA_HDR));
	tlh->__x0 = 0;
	tlh->thread_count = htonl(fhdrcount);
	tlh->__x1 = 0;
	pos += SIZEOF_HL_NEWS_THREADLIST_HDR;
	FD_SET(htlc->fd, &hxd_wfds);
	for (i = 0; i < fhdrcount; i++) {
		memcpy(&htlc->out.buf[pos], fhdrs[i].hdr, SIZEOF_HL_NEWS_THREAD_HDR + 5 + fhdrs[i].len);
		fh = (struct hl_news_thread_hdr *)&htlc->out.buf[pos];
		pos += SIZEOF_HL_NEWS_THREAD_HDR + 5 + fhdrs[i].len;
	}
	len = pos - this_off;
#ifdef CONFIG_COMPRESS
	if (htlc->compress_encode_type != COMPRESS_NONE)
		len = compress_encode(htlc, this_off, len);
#endif
#ifdef CONFIG_CIPHER
	if (htlc->cipher_encode_type != CIPHER_NONE)
		cipher_encode(htlc, this_off, len);
#endif
}

struct mac_date {
	u_int16_t base_year PACKED;
	u_int16_t pad PACKED;
	u_int32_t seconds PACKED;
};

/* poster, mimetype, subject are pascal strings */
static int
read_newsfile_ids (const char *pathbuf, u_int32_t *id, u_int32_t *parent_id)
{
	int fd, linelen;
	char *line, *p;
	size_t totalread = 0;
	struct stat sb;
	getline_t *gl;

	fd = SYS_open(pathbuf, O_RDONLY, 0);
	if (fd < 0)
		return errno;
	if (fstat(fd, &sb)) {
		close(fd);
		return errno;
	}
	gl = getline_open(fd);
	*id = *parent_id = 0;
	for (;;) {
		line = getline_line(gl, &linelen);
		if (!line)
			break;
		totalread += linelen+1;
		if (!linelen) {
			/* \n\n - end of header */
			close(fd);
			getline_close(gl);
			return 0;
		}
		p = line;
		if (!strncasecmp(p, "MESSAGE-ID: ", 12)) {
			*id = atou32(p+12);
		} else if (!strncasecmp(p, "REFERENCES: ", 12)) {
			*parent_id = atou32(p+12);
		}
	}
	close(fd);
	getline_close(gl);

	return 0;
}

static int
read_newsfile (const char *pathbuf, u_int8_t *poster, u_int8_t *mimetype, u_int8_t *subject,
	       u_int8_t *date, u_int32_t *id, u_int32_t *parent_id, u_int16_t *datasize, u_int8_t **datap)
{
	int fd, linelen;
	char *line, *p;
	size_t len, totalread = 0;
	struct stat sb;
	u_int16_t ds;
	getline_t *gl;

	fd = SYS_open(pathbuf, O_RDONLY, 0);
	if (fd < 0)
		return errno;
	if (fstat(fd, &sb)) {
		close(fd);
		return errno;
	}
	gl = getline_open(fd);
	*id = *parent_id = 0;
	*datasize = 0;
	poster[0] = mimetype[0] = subject[0];
	memset(date, 0, 8);
	for (;;) {
		line = getline_line(gl, &linelen);
		if (!line)
			break;
		totalread += linelen+1;
		if (!linelen) {
			/* \n\n - end of header */
			if ((sb.st_size - totalread) > 0xffff)
				ds = 0xffff;
			else
				ds = sb.st_size - totalread;
			*datasize = ds;
			if (datap) { /* XXX */
				*datap = xmalloc(ds);
				lseek(fd, (off_t)totalread, SEEK_SET);
				read(fd, *datap, ds);
			}
			close(fd);
			getline_close(gl);
			return 0;
		}
		p = line;
		if (!strncasecmp(p, "FROM: ", 6)) {
			len = strlen(p+6);
			if (len > 255)
				len = 255;
			poster[0] = (u_int8_t)len;
			memcpy(poster+1, p+6, len);
		} else if (!strncasecmp(p, "CONTENT-TYPE: ", 14)) {
			len = strlen(p+14);
			if (len > 255)
				len = 255;
			mimetype[0] = (u_int8_t)len;
			memcpy(mimetype+1, p+14, len);
		} else if (!strncasecmp(p, "SUBJECT: ", 9)) {
			len = strlen(p+9);
			if (len > 255)
				len = 255;
			subject[0] = (u_int8_t)len;
			memcpy(subject+1, p+9, len);
		} else if (!strncasecmp(p, "DATE: ", 6)) {
			struct mac_date md;
			struct tm tm;
			time_t tt;

			memset(&tm, 0, sizeof(struct tm));
			/* XXX %z is GNU extension, may have locale troubles with %a */
			strptime(p+6, "%a, %d %b %Y %H:%M:%S %z", &tm);
			tt = mktime(&tm);
			/* md.base_year = htons((u_int16_t)(tm.tm_year + 1900)); */
			md.base_year = htons(1904);
			md.pad = 0;
			md.seconds = htonl((u_int32_t)(tt + 2082844800U));
			memcpy(date, &md, 8);
		} else if (!strncasecmp(p, "MESSAGE-ID: ", 12)) {
			*id = atou32(p+12);
		} else if (!strncasecmp(p, "REFERENCES: ", 12)) {
			*parent_id = atou32(p+12);
		}
	}
	close(fd);
	getline_close(gl);

	return 0;
}

static void
tnews_send_catlist (struct htlc_conn *htlc, const char *path)
{
	u_int8_t *p, slen, plen, mlen, date[8], subject[256], poster[256], mimetype[256];
	u_int32_t id, parent_id;
	u_int16_t datasize;
	u_int16_t count = 0, maxcount = 0;
	struct thread_hdr *fhdrs = 0;
	struct hl_news_thread_hdr *th;
	DIR *dir;
	struct dirent *de;
	struct stat sb;
	char pathbuf[MAXPATHLEN];
	int err;

	if (!(dir = opendir(path))) {
		snd_strerror(htlc, errno);
		return;
	}
	while ((de = readdir(dir))) {
		/* skip files whose names start with '.' */
		if (de->d_name[0] == '.')
			continue;
		snprintf(pathbuf, sizeof(pathbuf), "%s/%s", path, de->d_name);
		if (SYS_lstat(pathbuf, &sb)) {
			continue;
		}
		/* skip directories */
		if (S_ISDIR(sb.st_mode))
			continue;
		if (count >= maxcount) {
			maxcount += 16;
			fhdrs = xrealloc(fhdrs, sizeof(struct thread_hdr) * maxcount);
		}
		err = read_newsfile(pathbuf, poster, mimetype, subject, date, &id, &parent_id, &datasize, 0);
		if (err)
			continue;
		slen = subject[0];
		plen = poster[0];
		mlen = mimetype[0];
		fhdrs[count].parid = parent_id;
		fhdrs[count].id = id;
		fhdrs[count].len = slen + plen + mlen;
		fhdrs[count].hdr = xmalloc(SIZEOF_HL_NEWS_THREAD_HDR + 5 + slen + plen + mlen);
		th = fhdrs[count].hdr;
		th->id = htonl(id);
		memcpy(&th->date, date, 8);
		th->parent_id = htonl(parent_id);
		th->flags = 0;
		th->part_count = htons(1);
		p = th->data;
		memcpy(p, subject, slen+1);
		p += slen+1;
		memcpy(p, poster, plen+1);
		p += plen+1;
		memcpy(p, mimetype, mlen+1);
		p += mlen+1;
		S16HTON(datasize, p);
		count++;
	}
	closedir(dir);
	if (fhdrs)
		sort_threads(fhdrs, count);
	snd_news_catlist(htlc, fhdrs, count);
	while (count) {
		count--;
		xfree(fhdrs[count].hdr);
	}
	if (fhdrs)
		xfree(fhdrs);
}

void
rcv_news_listdir (struct htlc_conn *htlc)
{
	char rlpath[MAXPATHLEN], path[MAXPATHLEN];
	int err;

	if (!hxd_cfg.operation.tnews) {
		snd_errorstr(htlc, "Threaded news directory listing denied: threaded news disabled");
		return;
	}
	if (htlc->in.pos == SIZEOF_HL_HDR) {
		tnews_send_dirlist(htlc, hxd_cfg.paths.newsdir);
		return;
	}
	dh_start(htlc)
		if (dh_type != HTLC_DATA_NEWS_DIR)
			continue;
		if ((err = hldir_to_path(dh, hxd_cfg.paths.newsdir, rlpath, path))) {
			snd_strerror(htlc, err);
			return;
		}
		tnews_send_dirlist(htlc, rlpath);
	dh_end()
}

void
rcv_news_listcat (struct htlc_conn *htlc)
{
	char rlpath[MAXPATHLEN], path[MAXPATHLEN];
	int err;

	if (!hxd_cfg.operation.tnews) {
		snd_errorstr(htlc, "Threaded category directory listing denied: threaded news disabled");
		return;
	}

	dh_start(htlc)
		if (dh_type != HTLC_DATA_NEWS_DIR)
			continue;
		if ((err = cat_to_path(dh, hxd_cfg.paths.newsdir, rlpath, path))) {
			snd_strerror(htlc, err);
			return;
		}
		tnews_send_catlist(htlc, rlpath);
	dh_end()
}

static void
tnews_send_thread (struct htlc_conn *htlc, const char *rlpath, u_int32_t tid)
{
	char pathbuf[MAXPATHLEN];
	int err;
	u_int8_t *data, date[8], subject[256], poster[256], mimetype[256];
	u_int32_t id, parent_id, lastid;
	u_int32_t ptid, ntid, nstid, partid32, ptid32, ntid32;
	u_int16_t datasize, slen, plen, mlen;
	DIR *dir;
	struct dirent *de;

	ptid = ntid = 0;
	/* XXX get prev/next tids */
	if (!(dir = opendir(rlpath))) {
		goto xxx;
	}
	lastid = 0;
	while ((de = readdir(dir))) {
		if (de->d_name[0] == '.')
			continue;
		id = atou32(de->d_name);
		if (id == tid) {
			ptid = lastid;
		} else if (lastid == tid) {
			ntid = id;
			break;
		}
		lastid = id;
	}
	closedir(dir);
xxx:
	snprintf(pathbuf, sizeof(pathbuf), "%s/%u", rlpath, tid);
	err = read_newsfile(pathbuf, poster, mimetype, subject, date, &id, &parent_id, &datasize, &data);
	if (err) {
		snd_strerror(htlc, err);
		return;
	}
	if (data)
		LF2CR(data, datasize);
	slen = (u_int16_t)subject[0];
	plen = (u_int16_t)poster[0];
	mlen = (u_int16_t)mimetype[0];
	ptid32 = htonl(ptid);
	ntid32 = htonl(ntid);
	nstid = htonl(0);
	partid32 = htonl(parent_id);
	hlwrite(htlc, HTLS_HDR_TASK, 0, 9,
		HTLS_DATA_NEWS_POST, datasize, data,
		HTLS_DATA_NEWS_PREVTHREADID, sizeof(ptid32), &ptid32,
		HTLS_DATA_NEWS_NEXTTHREADID, sizeof(ntid32), &ntid32,
		HTLS_DATA_NEWS_PARENTTHREADID, sizeof(partid32), &partid32,
		HTLS_DATA_NEWS_NEXTSUBTHREADID, sizeof(nstid), &nstid,
		HTLS_DATA_NEWS_SUBJECT, slen, subject+1,
		HTLS_DATA_NEWS_POSTER, plen, poster+1,
		HTLS_DATA_NEWS_MIMETYPE, mlen, mimetype+1,
		HTLS_DATA_NEWS_DATE, 8, date);
	if (data)
		xfree(data);
}

void
rcv_news_get_thread (struct htlc_conn *htlc)
{
	char rlpath[MAXPATHLEN], path[MAXPATHLEN];
	int err = 1;
	u_int16_t tid;

	if (!hxd_cfg.operation.tnews) {
		snd_errorstr(htlc, "Threaded news thread get denied: threaded news disabled");
		return;
	}

	tid = 0;
	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_NEWS_DIR:
				if ((err = cat_to_path(dh, hxd_cfg.paths.newsdir, rlpath, path))) {
					snd_strerror(htlc, err);
					return;
				}
				break;
			case HTLC_DATA_NEWS_THREADID:
				dh_getint(tid);
				break;
			case HTLC_DATA_NEWS_MIMETYPE:
				break;
		}
	dh_end()

	if (!err)
		tnews_send_thread(htlc, rlpath, tid);
}

static void
get_gmtime (struct tm *tm)
{
	time_t t;

	t = time(0);
	/* XXX gmtime_r */
	localtime_r(&t, tm);
}

static int
write_newsfile (const char *rlpath, u_int32_t partid, u_int32_t replytid,
		u_int8_t *poster, u_int8_t *mimetype, u_int8_t *subject, u_int16_t postlen, u_int8_t *postp)
{
	char linebuf[1024], datestr[128];
	size_t linelen;
	char pathbuf[MAXPATHLEN];
	int fd, r;
	DIR *dir;
	struct dirent *de;
	u_int32_t tid, id, highid;
	struct tm tm;

	if (!(dir = opendir(rlpath)))
		return -1;
	highid = 0;
	while ((de = readdir(dir))) {
		if (de->d_name[0] == '.')
			continue;
		id = atou32(de->d_name);
		if (id > highid)
			highid = id;
	}
	closedir(dir);
	if (highid == 0xffffffff) {
		return -1;
	}
	tid = highid+1;
	snprintf(pathbuf, sizeof(pathbuf), "%s/%u", rlpath, tid);
	fd = SYS_open(pathbuf, O_WRONLY|O_CREAT, hxd_cfg.permissions.news_files);
	if (fd < 0)
		return -1;
	get_gmtime(&tm);
	strftime(datestr, sizeof(datestr), "%a, %d %b %Y %H:%M:%S %z", &tm);
	linelen = snprintf(linebuf, sizeof(linebuf),
			  "From: %s\n"
			  "Content-Type: %s\n"
			  "Subject: %s\n"
			  "Date: %s\n"
			  "Message-ID: %u\n",
			  poster, mimetype, subject, datestr, tid);
	if (replytid)
		linelen += snprintf(linebuf+linelen, sizeof(linebuf)-linelen,
				  "References: %u\n", replytid);
	linebuf[linelen] = '\n';
	linelen++;
	r = write(fd, linebuf, linelen);
	CR2LF(postp, postlen);
	r = write(fd, postp, postlen);
	r = write(fd, "\n", 1);
	SYS_fsync(fd);
	close(fd);

	return 0;
}

void
rcv_news_post_thread (struct htlc_conn *htlc)
{
	u_int8_t slen, mlen, subject[256], mimetype[256], *postp = 0;
	u_int16_t postlen = 0;
	u_int32_t replytid = 0, partid = 0;
	char rlpath[MAXPATHLEN], path[MAXPATHLEN];
	int err = 1;

	if (!hxd_cfg.operation.tnews) {
		snd_errorstr(htlc, "Threaded news post thread denied: threaded news disabled");
		return;
	}

	mimetype[0] = 0;
	subject[0] = 0;
	
	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_NEWS_DIR:
				if ((err = cat_to_path(dh, hxd_cfg.paths.newsdir, rlpath, path))) {
					snd_strerror(htlc, err);
					return;
				}
				break;
			case HTLC_DATA_NEWS_PARENTTHREADID:
				dh_getint(partid);
				break;
			case HTLC_DATA_NEWS_MIMETYPE:
				mlen = dh_len > 0xff ? 0xff : dh_len;
				memcpy(mimetype, dh_data, mlen);
				mimetype[mlen] = 0;
				break;
			case HTLC_DATA_NEWS_SUBJECT:
				slen = dh_len > 0xff ? 0xff : dh_len;
				memcpy(subject, dh_data, slen);
				subject[slen] = 0;
				break;
			case HTLC_DATA_NEWS_POST:
				postlen = dh_len;
				postp = dh_data;
				break;
			case HTLC_DATA_NEWS_THREADID:
				dh_getint(replytid);
				break;
		}
	dh_end()

	if (!postp || err) {
		snd_strerror(htlc, EINVAL);
		return;
	}

	err = write_newsfile(rlpath, partid, replytid, htlc->name, mimetype, subject, postlen, postp);
	if (err)
		snd_errorstr(htlc, "Error writing post to file");
	else
		hlwrite(htlc, HTLS_HDR_TASK, 0, 0);
}

static int
delete_replies (u_int32_t tid, const char *catpath)
{
	u_int32_t id, parent_id;
	u_int16_t i, count = 0, maxcount = 0;
	struct thread_hdr *fhdrs = 0;
	DIR *dir;
	struct dirent *de;
	struct stat sb;
	char pathbuf[MAXPATHLEN];
	int err = 0;

	if (!(dir = opendir(catpath)))
		return -1;
	while ((de = readdir(dir))) {
		/* skip files whose names start with '.' */
		if (de->d_name[0] == '.')
			continue;
		snprintf(pathbuf, sizeof(pathbuf), "%s/%s", catpath, de->d_name);
		if (SYS_lstat(pathbuf, &sb)) {
			continue;
		}
		/* skip directories */
		if (S_ISDIR(sb.st_mode))
			continue;
		if (count >= maxcount) {
			maxcount += 16;
			fhdrs = xrealloc(fhdrs, sizeof(struct thread_hdr) * maxcount);
		}
		err = read_newsfile_ids(pathbuf, &id, &parent_id);
		if (err)
			continue;
		fhdrs[count].parid = parent_id;
		fhdrs[count].id = id;
		count++;
	}
	closedir(dir);
	if (fhdrs)
		sort_threads(fhdrs, count);
	for (i = 0; i < count; i++) {
		if (fhdrs[i].id == tid) {
			/* All replies come after this */
			u_int16_t j, tidx = i;
			for (i++; i < count; i++) {
				for (j = i-1; j >= tidx; j--) {
					if (fhdrs[i].parid == fhdrs[j].id)
						goto delete_this;
				}
				/* no more replies */
				break;
delete_this:
				snprintf(pathbuf, sizeof(pathbuf), "%s/%u", catpath, fhdrs[i].id);
				err = unlink(pathbuf);
				if (err)
					break;
			}
			break;
		}
	}
	if (fhdrs)
		xfree(fhdrs);

	return err;
}

void
rcv_news_delete_thread (struct htlc_conn *htlc)
{
	char rlpath[MAXPATHLEN], path[MAXPATHLEN], pathbuf[MAXPATHLEN];
	int err = 1;
	u_int32_t tid;
	u_int32_t delreplies;
	struct stat sb;

	if (!hxd_cfg.operation.tnews) {
	        snd_errorstr(htlc, "Threaded news delete thread denied: threaded news disabled");
	        return;
	}

	tid = 0;
	delreplies = 0;
	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_NEWS_DIR:
				if ((err = cat_to_path(dh, hxd_cfg.paths.newsdir, rlpath, path))) {
					snd_strerror(htlc, err);
					return;
				}
				break;
			case HTLC_DATA_NEWS_THREADID:
				dh_getint(tid);
				break;
			case HTLC_DATA_NEWS_MIMETYPE:
				break;
			case HTLC_DATA_NEWS_DELETEREPLIES:
				dh_getint(delreplies);
				break;
			default:
				break;
		}
	dh_end()

	if (err) {
		snd_strerror(htlc, EINVAL);
		return;
	}

	snprintf(pathbuf, sizeof(pathbuf), "%s/%u", rlpath, tid);

	if (log_deletethread)
		hxd_log("%s:%s:%u - deletethread %s", htlc->name, htlc->login, htlc->uid, pathbuf);

	if (SYS_lstat(pathbuf, &sb)) {
		snd_strerror(htlc, errno);
		return;
	}

	if (S_ISDIR(sb.st_mode) && !S_ISLNK(sb.st_mode)) {
		snd_errorstr(htlc, "Thread appears to be a bundle. Cannot delete.");
		return;
	} else {
		if (delreplies == 1) {
			err = delete_replies(tid, rlpath);
			if (!err)
				err = unlink(pathbuf);
		} else
			err = unlink(pathbuf);
	}

	if (err)
		snd_strerror(htlc, errno);
	else
		hlwrite(htlc, HTLS_HDR_TASK, 0, 0);
}

static int
recursive_rmdir (char *dirpath)
{
	DIR *dir;
	struct dirent *de;
	char pathbuf[MAXPATHLEN];
	struct stat sb;
	int err;

	dir = opendir(dirpath);
	if (!dir)
		return 1;
	while ((de = readdir(dir))) {
		if (de->d_name[0] == '.' && ((de->d_name[1] == '.' && !de->d_name[2]) || !de->d_name[1]))
			continue;
		snprintf(pathbuf, MAXPATHLEN, "%s/%s", dirpath, de->d_name);
		if ((err = SYS_lstat(pathbuf, &sb)))
			goto ret;
		if (S_ISDIR(sb.st_mode) && !S_ISLNK(sb.st_mode))
			err = recursive_rmdir(pathbuf);
		else
			err = unlink(pathbuf);
		if (err)
			goto ret;
	}
	closedir(dir);
	return rmdir(dirpath);
ret:
	closedir(dir);

	return err;
}

void
rcv_news_delete (struct htlc_conn *htlc)
{
	char path[MAXPATHLEN], rlpath[MAXPATHLEN];
	int err, isdir;
	struct stat sb;

	if (!hxd_cfg.operation.tnews) {
		snd_errorstr(htlc, "Threaded news delete denied: threaded news disabled");
		return;
	}

	rlpath[0] = 0;
	isdir = 1;
	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_NEWS_DIR:
				if ((err = hldir_to_path(dh, hxd_cfg.paths.newsdir, rlpath, path))) {
					if ((err = cat_to_path(dh, hxd_cfg.paths.newsdir, rlpath, path))) {
						snd_strerror(htlc, err);
						return;
					}
					isdir = 0;
				} else {
					isdir = 1;
				}
				break;
			default:
				break;
		}
	dh_end()

	if (log_deletebundle)
		hxd_log("%s:%s:%u - deletenewsbundle %s", htlc->name, htlc->login, htlc->uid, path);

	err = SYS_lstat(rlpath, &sb);
	if (err) {
		snd_strerror(htlc, errno);
		return;
	}

	if (S_ISDIR(sb.st_mode) && !S_ISLNK(sb.st_mode))
		err = recursive_rmdir(rlpath);
	else
		err = unlink(rlpath);

	if (err)
		snd_strerror(htlc, errno);
	else
		hlwrite(htlc, HTLS_HDR_TASK, 0, 0);
}

void
rcv_news_mkdir (struct htlc_conn *htlc)
{
	char path[MAXPATHLEN], rlpath[MAXPATHLEN];
	int err;
	u_int16_t dirnamelen = 0;
	char dirname[MAXPATHLEN];
	char catname[MAXPATHLEN];
	struct stat sb;

	if (!hxd_cfg.operation.tnews) {
		snd_errorstr(htlc, "Threaded news make bundle denied: threaded news disabled");
		return;
	}

	rlpath[0] = 0;
	dirname[0] = 0;
	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_NEWS_DIR:
				if ((err = hldir_to_path(dh, hxd_cfg.paths.newsdir, rlpath, path))) {
					snd_strerror(htlc, err);
					return;
				}
				break;

			case HTLC_DATA_FILE_NAME:
				memcpy(dirname, dh_data, dh_len);
				dirnamelen = dh_len;
				if (dirnamelen > sizeof(dirname)-1)
					dirnamelen = sizeof(dirname)-1;
				break;
			default:
				break;
		}
	dh_end()

	if (dirnamelen == 0) {
		snd_errorstr(htlc, "No Bundlename");
		return;
	}

	if (log_mknewsbundle)
		hxd_log("%s:%s:%u - mknewsbundle %s", htlc->name, htlc->login, htlc->uid, path);

	if (!strncmp(dirname, "cat_", 4)) {
		snd_errorstr(htlc, "Bundlenames must not start with 'cat_'");
		return;
	}

	/* check for category with same name */
	if (!rlpath[0]) {
		snprintf(catname, sizeof(catname), "%s/cat_%s", hxd_cfg.paths.newsdir, dirname);
	} else {
		snprintf(catname, sizeof(catname), "%s/cat_%s", rlpath, dirname);
	}
	err = SYS_lstat(catname, &sb);
	if (!err) {
		snd_errorstr(htlc, "There is already a category with the same name");
		return;
	}

	if (!rlpath[0]) {
		snprintf(path, sizeof(path), "%s/%s", hxd_cfg.paths.newsdir, dirname);
	} else {
		snprintf(path, sizeof(path), "%s/%s", rlpath, dirname);
	}

	if (SYS_mkdir(path, hxd_cfg.permissions.directories))
		snd_strerror(htlc, errno);
	else
		hlwrite(htlc, HTLS_HDR_TASK, 0, 0);
}

void
rcv_news_mkcategory (struct htlc_conn *htlc)
{
	char path[MAXPATHLEN], rlpath[MAXPATHLEN];
	int err;
	u_int16_t catnamelen;
	char catname[MAXPATHLEN];
	char dirname[MAXPATHLEN];
	struct stat sb;

	if (!hxd_cfg.operation.tnews) {
		snd_errorstr(htlc, "Threaded news make category listing denied: threaded news disabled");
		return;
	}

	catname[0] = 0;
	rlpath[0] = 0;
	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_NEWS_DIR:
				if ((err = hldir_to_path(dh, hxd_cfg.paths.newsdir, rlpath, path))) {
					snd_strerror(htlc, err);
					return;
				}
				break;
			case HTLC_DATA_NEWS_CATNAME:
				memcpy(catname, dh_data, dh_len);
				catnamelen = dh_len;
				if (catnamelen > sizeof(catname)-1)
					catnamelen = sizeof(catname)-1;
				catname[catnamelen] = 0;
				break;
			default:
				break;
		}
	dh_end()

	/* check for bundle with same name */
	if (!rlpath[0]) {
		snprintf(dirname, sizeof(dirname), "%s/%s", hxd_cfg.paths.newsdir, catname);
	} else {
		snprintf(dirname, sizeof(dirname), "%s/%s", rlpath, catname);
	}
	err = SYS_lstat(dirname, &sb);
	if (!err) {
		snd_errorstr(htlc, "There is already a bundle with the same name");
		return;
	}

	if (!rlpath[0]) {
		snprintf(path, sizeof(path), "%s/cat_%s", hxd_cfg.paths.newsdir, catname);
	} else {
		snprintf(path, sizeof(path), "%s/cat_%s", rlpath, catname);
	}

	if (log_mknewscategory)
		hxd_log("%s:%s:%u - mknewscategory %s", htlc->name, htlc->login, htlc->uid, path);

	if (SYS_mkdir(path, hxd_cfg.permissions.directories))
		snd_strerror(htlc, errno);
	else
		hlwrite(htlc, HTLS_HDR_TASK, 0, 0);
}
