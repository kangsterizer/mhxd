#include "hx.h"
#include "hxd.h"
#include "xmalloc.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "sys_net.h"
#include <sys/stat.h>
#include <sys/types.h>
#if !defined(__WIN32__)
#include <sys/wait.h>
#include <signal.h>
#endif
#include <fnmatch.h>
#if !defined(__WIN32__)
#include <glob.h>
#endif

#include "threads.h"

#if defined(CONFIG_ICONV)
#include "conv.h"
#endif

#if defined(CONFIG_HFS)
#include "hfs.h"
#endif

#if defined(CONFIG_SOUND)
#include "sound.h"
#endif

static struct cached_filelist __cfl_list = {0, 0, 0, 0, 0, 0, 0, 0}, *cfl_list = &__cfl_list, *cfl_tail = &__cfl_list;

void
hx_dirchar_change (struct htlc_conn *htlc, u_int8_t dc)
{
	struct cached_filelist *cfl;
	char *p, old_dirchar = dir_char;

	dir_char = dc;
	for (p = htlc->rootdir; *p; p++)
		if (*p == old_dirchar)
			*p = dir_char;
	for (cfl = cfl_list->next; cfl; cfl = cfl->next)
		for (p = cfl->path; p && *p; p++)
			if (*p == old_dirchar)
				*p = dir_char;
}

#ifndef PACKED
#ifdef __GNUC__
#define PACKED __attribute__((__packed__))
#else
#define PACKED
#endif
#endif

struct x_fhdr {
	u_int16_t enc PACKED;
	u_int8_t len PACKED, name[1] PACKED;
};

struct hldir *
path_to_hldir (const char *path, int is_file)
{
	struct hldir *hldir;
	struct x_fhdr *fh;
	char const *p, *p2;
	u_int16_t pos = 2, dc = 0;
	u_int8_t nlen;
#if defined(CONFIG_ICONV)
	char *out_p;
	size_t out_len;
#endif

	hldir = xmalloc(sizeof(struct hldir));
	hldir->dirlen = 0;
	hldir->filelen = 0;
	p = path;
	while ((p2 = strchr(p, dir_char))) {
		if (!(p2 - p)) {
			p++;
			continue;
		}
		nlen = (u_int8_t)(p2 - p);
		pos += 3 + nlen;
		hldir = xrealloc(hldir, sizeof(struct hldir) + pos);
		fh = (struct x_fhdr *)((u_int8_t *)hldir + sizeof(struct hldir) + (pos - (3 + nlen)));
		memset(&fh->enc, 0, 2);
		fh->len = nlen;
#if defined(CONFIG_ICONV)
		out_len = convbuf(g_server_encoding, g_encoding, p, (size_t)nlen, &out_p);
		if (out_len) {
			if (out_len > (size_t)nlen)
				out_len = (size_t)nlen;
			memcpy(fh->name, out_p, out_len);
			xfree(out_p);
		} else
#endif
			memcpy(fh->name, p, (size_t)nlen);
		dc++;
		p = p2 + 1;
	}
	if (!is_file && *p) {
		nlen = (u_int8_t)strlen(p);
		pos += 3 + nlen;
		hldir = xrealloc(hldir, sizeof(struct hldir) + pos);
		fh = (struct x_fhdr *)((u_int8_t *)hldir + sizeof(struct hldir) + (pos - (3 + nlen)));
		memset(&fh->enc, 0, 2);
		fh->len = nlen;
#if defined(CONFIG_ICONV)
		out_len = convbuf(g_server_encoding, g_encoding, p, (size_t)nlen, &out_p);
		if (out_len) {
			if (out_len > (size_t)nlen)
				out_len = (size_t)nlen;
			memcpy(fh->name, out_p, out_len);
			xfree(out_p);
		} else
#endif
			memcpy(fh->name, p, (size_t)nlen);
		dc++;
	} else if (is_file && *p) {
		nlen = (u_int8_t)strlen(p);
		hldir = xrealloc(hldir, sizeof(struct hldir) + pos + nlen);
		hldir->file = (u_int8_t *)hldir + sizeof(struct hldir) + pos;
		hldir->filelen = (u_int16_t)nlen;
#if defined(CONFIG_ICONV)
		out_len = convbuf(g_server_encoding, g_encoding, p, (size_t)nlen, &out_p);
		if (out_len) {
			if (out_len > (size_t)nlen)
				out_len = (size_t)nlen;
			memcpy(hldir->file, out_p, out_len);
			xfree(out_p);
		} else
#endif
			memcpy(hldir->file, p, (size_t)nlen);
	}
	if (dc) {
		hldir->dir = (u_int8_t *)hldir + sizeof(struct hldir);
		*((u_int16_t *)(hldir->dir)) = htons(dc);
		hldir->dirlen = pos;
	}

	return hldir;
}

struct cached_filelist *
cfl_lookup (const char *path)
{
	struct cached_filelist *cfl;

	for (cfl = cfl_list->next; cfl; cfl = cfl->next)
		if (!strcmp(cfl->path, path))
			return cfl;

	cfl = xmalloc(sizeof(struct cached_filelist));
	memset(cfl, 0, sizeof(struct cached_filelist));

	cfl->next = 0;
	cfl->prev = cfl_tail;
	cfl_tail->next = cfl;
	cfl_tail = cfl;

	return cfl;
}

static void
cfl_delete (struct cached_filelist *cfl)
{
	if (cfl->path)
		xfree(cfl->path);
	if (cfl->fh)
		xfree(cfl->fh);
	if (cfl->next)
		cfl->next->prev = cfl->prev;
	if (cfl->prev)
		cfl->prev->next = cfl->next;
	if (cfl_tail == cfl)
		cfl_tail = cfl->prev;
	xfree(cfl);
}

struct hl_filelist_hdr *
fh_lookup (const char *path)
{
	struct hl_filelist_hdr *fh;
	char const *p, *ent;
	char *dirpath;
	int len, flen, blen = 0;
	u_int16_t fnlen;
	struct cached_filelist *cfl;

	ent = path;
	len = strlen(path);
	for (p = path + len - 1; p >= path; p--) {
		if (*p == dir_char) {
			ent = p+1;
			while (p > path && *p == dir_char)
				p--;
			blen = (p+1) - path;
			break;
		}
	}

	dirpath = xmalloc(blen + 1);
	memcpy(dirpath, path, blen);
	dirpath[blen] = 0;

	for (cfl = cfl_list->next; cfl; cfl = cfl->next)
		if (!strcmp(cfl->path, dirpath))
			break;
	xfree(dirpath);

	if (!cfl)
		return 0;

	for (fh = cfl->fh; (u_int32_t)((char *)fh - (char *)cfl->fh) < cfl->fhlen;
	     fh = fh + flen + SIZEOF_HL_DATA_HDR) {
		L16NTOH(flen, &fh->len);
		L16NTOH(fnlen, &fh->fnlen);
		if (!memcmp(fh->fname, ent, fnlen))
			return fh;
	}

	return fh;
}

char *
dirchar_basename (const char *path)
{
	size_t len;

	len = strlen(path);
	while (len--) {
		if (path[len] == dir_char)
			return (char *)(path+len+1);
	}

	return (char *)path;
}

static inline void
dirchar_fix (char *lpath)
{
	char *p;

	for (p = lpath; *p; p++)
		if (*p == '/')
			*p = (dir_char == '/' ? ':' : dir_char);
}

static inline void
dirmask (char *dst, char *src, char *mask)
{
	while (*mask && *src && *mask++ == *src++) ;

	strcpy(dst, src);
}

#define COMPLETE_NONE	0
#define COMPLETE_EXPAND	1
#define COMPLETE_LS_R	2
#define COMPLETE_GET_R	3

static void
cfl_print (struct htlc_conn *htlc, struct cached_filelist *cfl, int text)
{
	void (*fn)(struct htlc_conn *, struct cached_filelist *);

	if (text)
		hx_printf_prefix(htlc, 0, INFOPREFIX, "%s:\n", cfl->path);
	if (text)
		fn = hx_tty_output.file_list;
	else
		fn = hx_output.file_list;
	fn(htlc, cfl);
}

static void
rcv_task_file_list (struct htlc_conn *htlc, struct cached_filelist *cfl, int text)
{
	struct hl_filelist_hdr *fh = 0;
	u_int32_t fh_len, ftype, flen;

	if (task_inerror(htlc)) {
		cfl_delete(cfl);
		return;
	}
	if (cfl->listed) {
		xfree(cfl->fh);
		cfl->fh = 0;
		cfl->fhlen = 0;
	}
	cfl->listed = 1;
	dh_start(htlc)
		if (dh_type != HTLS_DATA_FILE_LIST)
			continue;
		fh = (struct hl_filelist_hdr *)dh;
		if (cfl->completing > 1) {
			char *pathbuf;
			u_int16_t fnlen;
			u_int32_t len;

			L16NTOH(fnlen, &fh->fnlen);
			len = strlen(cfl->path) + 1 + fnlen + 1;
			pathbuf = xmalloc(len);
			snprintf(pathbuf, len, "%s%c%.*s", cfl->path[1] ? cfl->path : "", dir_char, (int)fnlen, fh->fname);
			L32NTOH(ftype, &fh->ftype);
			if (ftype == 0x666c6472) {
				struct cached_filelist *ncfl;
				struct hldir *hldir;

				ncfl = cfl_lookup(pathbuf);
				ncfl->completing = cfl->completing;
				ncfl->filter_argv = cfl->filter_argv;
				if (!ncfl->path)
					ncfl->path = xstrdup(pathbuf);
				hldir = path_to_hldir(pathbuf, 0);
				task_new(htlc, rcv_task_file_list, ncfl, text, "ls_complete");
				hlwrite(htlc, HTLC_HDR_FILE_LIST, 0, 1,
					HTLC_DATA_DIR, hldir->dirlen, hldir->dir);
				xfree(hldir);
			} else if (cfl->completing & COMPLETE_GET_R) {
				struct htxf_conn *htxf;
				char *lpath, *p;

				lpath = xmalloc(len);
				dirmask(lpath, pathbuf, htlc->rootdir);
				p = lpath+1;
				while ((p = strchr(p, dir_char))) {
					*p = 0;
					if (SYS_mkdir(lpath+1, S_IRUSR|S_IWUSR|S_IXUSR)) {
						if (errno != EEXIST)
							hx_printf_prefix(htlc, 0, INFOPREFIX, "mkdir(%s): %s\n", lpath+1, strerror(errno));
					}
					*p++ = '/';
					while (*p == dir_char)
						*p++ = '/';
				}
				p = basename(lpath+1);
				if (p)
					dirchar_fix(p);
				htxf = xfer_new(htlc, lpath+1, pathbuf, XFER_GET);
				htxf->filter_argv = cfl->filter_argv;
				xfree(lpath);
			}
			xfree(pathbuf);
		}
		L16NTOH(flen, &fh->len);
		fh_len = SIZEOF_HL_DATA_HDR + flen;
		fh_len += 4 - (fh_len % 4);
		cfl->fh = xrealloc(cfl->fh, cfl->fhlen + fh_len);
#if defined(CONFIG_ICONV)
		{
			char *out_p;
			size_t out_len;
			u_int16_t fnlen;

			L16NTOH(fnlen, &fh->fnlen);
			out_len = convbuf(g_encoding, g_server_encoding, fh->fname, fnlen, &out_p);
			if (out_len) {
				if (out_len > fnlen)
					out_len = fnlen;
				memcpy(fh->fname, out_p, out_len);
				xfree(out_p);
			}
		}
#endif
		memcpy((u_int8_t *)cfl->fh + cfl->fhlen, fh, SIZEOF_HL_DATA_HDR + flen);
		S16HTON(fh_len - SIZEOF_HL_DATA_HDR, (u_int8_t *)cfl->fh + cfl->fhlen + 2);
		cfl->fhlen += fh_len;
	dh_end()
	if (cfl->completing != COMPLETE_EXPAND)
		cfl_print(htlc, cfl, text);
	cfl->completing = COMPLETE_NONE;
}

static struct cached_filelist *last_cfl = 0;

int
expand_path (struct htlc_conn *htlc, struct hx_chat *chat, char *path, int len, char *pathbuf)
{
	struct cached_filelist *cfl;
	struct hl_filelist_hdr *fh;
	u_int32_t ftype;
	u_int16_t fnlen;
	int flen, blen = 0, eplen, epchr, ambig, r, ambigi = 0;
	char *p, *ent, *ep, buf[MAXPATHLEN], ambigbuf[4096], *ambigbufp = ambigbuf;

	if (*path != dir_char) {
		len = snprintf(buf, MAXPATHLEN, "%s%c%.*s", hx_htlc.rootdir, dir_char, len, path);
		if (len < 0)
			len = MAXPATHLEN;
		path = buf;
	}
	ent = path;
	for (p = path + len - 1; p >= path; p--)
		if (*p == dir_char) {
			ent = p+1;
			while (p > path && *p == dir_char)
				p--;
			blen = (p+1) - path;
			len -= ent - path;
			break;
		}
	if (!*ent)
		return -1;

	for (cfl = cfl_list->next; cfl; cfl = cfl->next) {
		if (!strncmp(cfl->path, path, blen))
			break;
	}
	if (!cfl) {
		struct hldir *hldir;
		char xxxbuf[MAXPATHLEN];
		int i;

		snprintf(xxxbuf, MAXPATHLEN, "%.*s", blen, path);
		path = xxxbuf;
		i = strlen(path);
		while (i > 1 && path[--i] == dir_char)
			path[i] = 0;
		cfl = cfl_lookup(path);
		cfl->completing = COMPLETE_EXPAND;
		cfl->path = xstrdup(path);
		hldir = path_to_hldir(path, 0);
		task_new(htlc, rcv_task_file_list, cfl, 1, "ls_expand");
		hlwrite(htlc, HTLC_HDR_FILE_LIST, 0, 1,
			HTLC_DATA_DIR, hldir->dirlen, hldir->dir);
		xfree(hldir);
		return -2;
	}
	if (!cfl->fh)
		return -1;
	ep = 0;
	eplen = 0;
	epchr = 0;
	ambig = 0;
	for (fh = cfl->fh; (u_int32_t)((char *)fh - (char *)cfl->fh) < cfl->fhlen;
	     fh = fh + flen + SIZEOF_HL_DATA_HDR) {
		L16NTOH(flen, &fh->len);
		L16NTOH(fnlen, &fh->fnlen);
		if (fnlen >= len && !strncmp(fh->fname, ent, len)) {
			if (ep) {
				int i;
	
				if (fnlen < eplen)
					eplen = fnlen;
				for (i = 0; i < eplen; i++)
					if (fh->fname[i] != ep[i])
						break;
				eplen = i;
				ambig = 1;
				epchr = 0;
xxx:
				if (last_cfl == cfl) {
					r = snprintf(ambigbufp, sizeof(ambigbuf) - (ambigbufp - ambigbuf), "  %-16.*s", (int)fnlen, fh->fname);
					if (!(r == -1 || sizeof(ambigbuf) <= (unsigned)(ambigbufp + r - ambigbuf))) {
						ambigi++;
						ambigbufp += r;
						if (!(ambigi % 5))
							*ambigbufp++ = '\n';
					}
				}
			} else {
				eplen = fnlen;
				L32NTOH(ftype, &fh->ftype);
				epchr = ftype == 0x666c6472 ? dir_char : 0;
				if (epchr)
					ambig = 1;
				goto xxx;
			}
			ep = fh->fname;
		}
	}
	if (!ep)
		return -1;
	if (ambigi > 1)
		hx_printf(htlc, chat, "%s%c", ambigbuf, (ambigi % 5) ? '\n' : 0);
	else
		last_cfl = cfl;

	snprintf(pathbuf, MAXPATHLEN, "%.*s%c%.*s%c", path[blen-1] == dir_char ? blen - 1 : blen, path, dir_char, eplen, ep, epchr);

	return ambig;
}

char **
glob_remote (char *path, int *npaths, u_int32_t type)
{
	struct cached_filelist *cfl;
	struct hl_filelist_hdr *fh;
	char *p, *ent, *patternbuf, *pathbuf, **paths;
	int n, len, flen, blen = 0;
	u_int32_t ftype;
	u_int16_t fnlen;

	ent = path;
	len = strlen(path);
	for (p = path + len - 1; p >= path; p--) {
		if (*p == dir_char) {
			ent = p+1;
			while (p > path && *p == dir_char)
				p--;
			blen = (p+1) - path;
			break;
		}
	}

	patternbuf = xmalloc(blen + 1);
	memcpy(patternbuf, path, blen);
	patternbuf[blen] = 0;

	paths = 0;
	n = 0;
	for (cfl = cfl_list->next; cfl; cfl = cfl->next) {
		if (fnmatch(patternbuf, cfl->path, FNM_NOESCAPE))
			continue;

		for (fh = cfl->fh; (u_int32_t)((char *)fh - (char *)cfl->fh) < cfl->fhlen;
		     fh = fh + flen + SIZEOF_HL_DATA_HDR) {
			L16NTOH(flen, &fh->len);
			switch (type) {
				case 0:
					break;
				/* no folders */
				case 1:
					L32NTOH(ftype, &fh->ftype);
					if (ftype == 0x666c6472 /* 'fldr' */)
						continue;
					break;
				default:
					L32NTOH(ftype, &fh->ftype);
					if (type != ftype)
						continue;
					break;
			}
			L16NTOH(fnlen, &fh->fnlen);
			len = strlen(cfl->path) + 1 + fnlen + 1;
			pathbuf = xmalloc(len);
			snprintf(pathbuf, len, "%s%c%.*s", cfl->path[1] ? cfl->path : "", dir_char, (int)fnlen, fh->fname);
			if (!fnmatch(path, pathbuf, FNM_NOESCAPE)) {
				paths = xrealloc(paths, (n + 1) * sizeof(char *));
				paths[n] = pathbuf;
				n++;
			} else {
				xfree(pathbuf);
			}
		}
	}
	xfree(patternbuf);
	if (n)
		goto ret;

	paths = xmalloc(sizeof(char *));
	n = 1;
	paths[0] = xstrdup(path);

ret:
	*npaths = n;

	return paths;
}

static int
exists_remote (struct htlc_conn *htlc, char *path)
{
	struct cached_filelist *cfl;
	struct hl_filelist_hdr *fh;
	char *p, *ent, buf[MAXPATHLEN];
	u_int32_t flen;
	u_int16_t fnlen;
	int blen = 0, len;

	len = strlen(path);
	if (*path != dir_char) {
		len = snprintf(buf, MAXPATHLEN, "%s%c%.*s", hx_htlc.rootdir, dir_char, len, path);
		if (len < 0)
			len = MAXPATHLEN;
		path = buf;
	}
	ent = path;
	for (p = path + len - 1; p >= path; p--)
		if (*p == dir_char) {
			ent = p+1;
			while (p > path && *p == dir_char)
				p--;
			blen = (p+1) - path;
			len -= ent - path;
			break;
		}
	if (!*ent)
		return -1;

	for (cfl = cfl_list->next; cfl; cfl = cfl->next)
		if (!strncmp(cfl->path, path, blen))
			break;
	if (!cfl) {
		struct hldir *hldir;

		snprintf(buf, MAXPATHLEN, "%.*s", blen, path);
		path = buf;
		len = strlen(path);
		while (len > 1 && path[--len] == dir_char)
			path[len] = 0;
		cfl = cfl_lookup(path);
		cfl->completing = COMPLETE_EXPAND;
		cfl->path = xstrdup(path);
		hldir = path_to_hldir(path, 0);
		task_new(htlc, rcv_task_file_list, cfl, 1, "ls_exists");
		hlwrite(htlc, HTLC_HDR_FILE_LIST, 0, 1,
			HTLC_DATA_DIR, hldir->dirlen, hldir->dir);
		xfree(hldir);
		return 0;
	}
	if (!cfl->fh)
		return 0;

	for (fh = cfl->fh; (u_int32_t)((char *)fh - (char *)cfl->fh) < cfl->fhlen;
	     fh = fh + flen + SIZEOF_HL_DATA_HDR) {
		L16NTOH(flen, &fh->len);
		L16NTOH(fnlen, &fh->fnlen);
		if ((int)fnlen == len && !strncmp(fh->fname, ent, len))
			return 1;
	}

	return 0;
}

void
hx_list_dir (struct htlc_conn *htlc, const char *path, int reload, int recurs, int text)
{
	struct hldir *hldir;
	struct cached_filelist *cfl;

	cfl = cfl_lookup(path);
	if (cfl->fh) {
		if (reload) {
			xfree(cfl->fh);
			cfl->fh = 0;
			cfl->fhlen = 0;
		} else {
			cfl_print(htlc, cfl, text);
			return;
		}
	}
	if (recurs)
		cfl->completing = COMPLETE_LS_R;
	if (!cfl->path)
		cfl->path = xstrdup(path);
	hldir = path_to_hldir(path, 0);
	task_new(htlc, rcv_task_file_list, cfl, text, "ls");
	hlwrite(htlc, HTLC_HDR_FILE_LIST, 0, 1,
		HTLC_DATA_DIR, hldir->dirlen, hldir->dir);
	xfree(hldir);
}

void
hx_mkdir (struct htlc_conn *htlc, const char *path)
{
	struct hldir *hldir;

	hldir = path_to_hldir(path, 0);
	task_new(htlc, 0, 0, 1, "mkdir");
	hlwrite(htlc, HTLC_HDR_FILE_MKDIR, 0, 1,
		HTLC_DATA_DIR, hldir->dirlen, hldir->dir);
	xfree(hldir);
}

void
hx_file_delete (struct htlc_conn *htlc, const char *path, int text)
{
	struct hldir *hldir;

	task_new(htlc, 0, 0, text, "rm");
	hldir = path_to_hldir(path, 1);
	if (hldir->dirlen) {
		hlwrite(htlc, HTLC_HDR_FILE_DELETE, 0, 2,
			HTLC_DATA_FILE_NAME, hldir->filelen, hldir->file,
			HTLC_DATA_DIR, hldir->dirlen, hldir->dir);
		xfree(hldir);
	} else {
		hlwrite(htlc, HTLC_HDR_FILE_DELETE, 0, 1,
			HTLC_DATA_FILE_NAME, hldir->filelen, hldir->file);
	}
}

void
hx_file_link (struct htlc_conn *htlc, const char *src_path, const char *dst_path)
{
	struct hldir *hldir, *rnhldir;

	hldir = path_to_hldir(src_path, 1);
	rnhldir = path_to_hldir(dst_path, 1);
	task_new(htlc, 0, 0, 1, "ln");
	hlwrite(htlc, HTLC_HDR_FILE_SYMLINK, 0, 4,
		HTLC_DATA_FILE_NAME, hldir->filelen, hldir->file,
		HTLC_DATA_DIR, hldir->dirlen, hldir->dir,
		HTLC_DATA_DIR_RENAME, rnhldir->dirlen, rnhldir->dir,
		HTLC_DATA_FILE_RENAME, rnhldir->filelen, rnhldir->file);
	xfree(rnhldir);
	xfree(hldir);
}

void
hx_file_move (struct htlc_conn *htlc, const char *src_path, const char *dst_path)
{
	struct hldir *hldir, *rnhldir;
	size_t len;

	hldir = path_to_hldir(src_path, 1);
	rnhldir = path_to_hldir(dst_path, 1);
	len = strlen(dst_path) - rnhldir->filelen;
	if (len
	    && (len != (strlen(src_path) - hldir->filelen)
	        || memcmp(dst_path, src_path, len))) {
		/* Move the file to a different directory. */
		task_new(htlc, 0, 0, 1, "mv");
		hlwrite(htlc, HTLC_HDR_FILE_MOVE, 0, 3,
			HTLC_DATA_FILE_NAME, hldir->filelen, hldir->file,
			HTLC_DATA_DIR, hldir->dirlen, hldir->dir,
			HTLC_DATA_DIR_RENAME, rnhldir->dirlen, rnhldir->dir);
	} else if (rnhldir->filelen
	    && (hldir->filelen != rnhldir->filelen
		|| memcmp(hldir->file, rnhldir->file, hldir->filelen))) {
		/* Rename the file in the same directory. */
		task_new(htlc, 0, 0, 1, "mv_setinfo");
		hlwrite(htlc, HTLC_HDR_FILE_SETINFO, 0, 3,
			HTLC_DATA_FILE_NAME, hldir->filelen, hldir->file,
			HTLC_DATA_FILE_RENAME, rnhldir->filelen, rnhldir->file,
			HTLC_DATA_DIR, hldir->dirlen, hldir->dir);
	}
	xfree(hldir);
	xfree(rnhldir);
}

extern char *hx_timeformat;

void
mac_to_tm (struct tm *tm, u_int8_t *date)
{
	time_t t;
	struct tm btm;
	u_int16_t base;
	u_int32_t sec;

	base = ntohs(*((u_int16_t *)&date[0]));
	sec = ntohl(*((u_int32_t *)&date[4]));
	if (base == 1904) {
		t = sec - 0x7c25b080;
	} else {
		memset(&btm, 0, sizeof(btm));
		btm.tm_sec = sec;
		btm.tm_year = base - 1900;
		t = mktime(&btm);
	}
	localtime_r(&t, tm);
}

static void
rcv_task_file_getinfo (struct htlc_conn *htlc, void *ptr, int text)
{
	u_int8_t icon[4], type[32], crea[32], date_create[8], date_modify[8];
	u_int8_t name[256], comment[256];
	u_int16_t nlen, clen, tlen;
	u_int32_t size = 0;
	char created[64], modified[64];
	struct tm tm;

	if (task_inerror(htlc))
		return;

	name[0] = comment[0] = type[0] = crea[0] = 0;
	dh_start(htlc)
		switch(dh_type) {
			case HTLS_DATA_FILE_ICON:
				if (dh_len >= 4)
					memcpy(icon, dh_data, 4);
				break;
			case HTLS_DATA_FILE_TYPE:
				tlen = dh_len > 31 ? 31 : dh_len;
				memcpy(type, dh_data, tlen);
				type[tlen] = 0;
				break;
			case HTLS_DATA_FILE_CREATOR:
				clen = dh_len > 31 ? 31 : dh_len;
				memcpy(crea, dh_data, clen);
				crea[clen] = 0;
				break;
			case HTLS_DATA_FILE_SIZE:
				if (dh_len >= 4)
					L32NTOH(size, dh_data);
				break;
			case HTLS_DATA_FILE_NAME:
				nlen = dh_len > 255 ? 255 : dh_len;
				memcpy(name, dh_data, nlen);
				name[nlen] = 0;
				strip_ansi(name, nlen);
				break;
			case HTLS_DATA_FILE_DATE_CREATE:
				if (dh_len >= 8)
					memcpy(date_create, dh_data, 8);
				break;
			case HTLS_DATA_FILE_DATE_MODIFY:
				if (dh_len >= 8)
					memcpy(date_modify, dh_data, 8);
				break;
			case HTLS_DATA_FILE_COMMENT:
				clen = dh_len > 255 ? 255 : dh_len;
				memcpy(comment, dh_data, clen);
				comment[clen] = 0;
				CR2LF(comment, clen);
				strip_ansi(name, clen);
				break;
		}
	dh_end()

	mac_to_tm(&tm, date_create);
	strftime(created, sizeof(created)-1, hx_timeformat, &tm);
	mac_to_tm(&tm, date_modify);
	strftime(modified, sizeof(modified)-1, hx_timeformat, &tm);

#if 0
	/* ??? */
	if (tlen == 4 && type[0]==0&&type[1]==0&&type[2]==0&&type[3]==0)
		memcpy(type, icon, 4);
#endif

	if (text) {
		hx_tty_output.file_info(htlc, icon, type, crea, size, name, created, modified, comment);
	} else {
		hx_output.file_info(htlc, icon, type, crea, size, name, created, modified, comment);
	}
}

void
hx_get_file_info (struct htlc_conn *htlc, const char *path, int text)
{
	struct hldir *hldir;

	task_new(htlc, rcv_task_file_getinfo, 0, text, "finfo");
	hldir = path_to_hldir(path, 1);
	if (hldir->dirlen) {
		hlwrite(htlc, HTLC_HDR_FILE_GETINFO, 0, 2,
			HTLC_DATA_FILE_NAME, hldir->filelen, hldir->file,
			HTLC_DATA_DIR, hldir->dirlen, hldir->dir);
	} else {
		hlwrite(htlc, HTLC_HDR_FILE_GETINFO, 0, 1,
			HTLC_DATA_FILE_NAME, hldir->filelen, hldir->file);
	}
	xfree(hldir);
}

#if defined(CONFIG_HOPE)
static void
hash2str (char *str, u_int8_t *hash, int len)
{
	int i, c;

	for (i = 0; i < len; i++) {
		c = hash[i] >> 4;
		c = c > 9 ? c - 0xa + 'a' : c + '0';
		str[i*2 + 0] = c;
		c = hash[i] & 0xf;
		c = c > 9 ? c - 0xa + 'a' : c + '0';
		str[i*2 + 1] = c;
	}
}

static void
rcv_task_file_hash (struct htlc_conn *htlc)
{
	u_int8_t md5[32], sha1[40], haval[64];
	int got_md5 = 0, got_sha1 = 0, got_haval = 0;
	char buf[128];

	if (task_inerror(htlc))
		return;

	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_HASH_MD5:
				got_md5 = 1;
				memcpy(md5, dh_data, dh_len);
				break;
			case HTLS_DATA_HASH_HAVAL:
				got_haval = 1;
				memcpy(haval, dh_data, dh_len);
				break;
			case HTLS_DATA_HASH_SHA1:
				got_sha1 = 1;
				memcpy(sha1, dh_data, dh_len);
				break;
		}
	dh_end()
	if (got_md5) {
		hash2str(buf, md5, 16);
		hx_printf_prefix(htlc, 0, INFOPREFIX, "md5: %32.32s\n", buf);
	}
	if (got_sha1) {
		hash2str(buf, sha1, 20);
		hx_printf_prefix(htlc, 0, INFOPREFIX, "sha1: %40.40s\n", buf);
	}
	if (got_haval) {
		hash2str(buf, haval, 16);
		hx_printf_prefix(htlc, 0, INFOPREFIX, "haval: %32.32s\n", buf);
	}
}

void
hx_file_hash (struct htlc_conn *htlc, const char *path, u_int32_t data_size, u_int32_t rsrc_size)
{
	struct hldir *hldir;
	u_int8_t rflt[74];

	memcpy(rflt, "\
RFLT\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\
\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\2\
DATA\0\0\0\0\0\0\0\0\0\0\0\0\
MACR\0\0\0\0\0\0\0\0\0\0\0\0", 74);
	S32HTON(data_size, &rflt[46]);
	S32HTON(rsrc_size, &rflt[62]);
	task_new(htlc, rcv_task_file_hash, 0, 1, "hash");
	hldir = path_to_hldir(path, 1);
	if (hldir->dirlen) {
		hlwrite(htlc, HTLC_HDR_FILE_HASH, 0, 6,
			HTLC_DATA_FILE_NAME, hldir->filelen, hldir->file,
			HTLC_DATA_DIR, hldir->dirlen, hldir->dir,
			HTLC_DATA_RFLT, 74, rflt,
			HTLC_DATA_HASH_MD5, 0, 0,
			HTLC_DATA_HASH_HAVAL, 0, 0,
			HTLC_DATA_HASH_SHA1, 0, 0);
	} else {
		hlwrite(htlc, HTLC_HDR_FILE_HASH, 0, 5,
			HTLC_DATA_FILE_NAME, hldir->filelen, hldir->file,
			HTLC_DATA_RFLT, 74, rflt,
			HTLC_DATA_HASH_MD5, 0, 0,
			HTLC_DATA_HASH_HAVAL, 0, 0,
			HTLC_DATA_HASH_SHA1, 0, 0);
	}
	xfree(hldir);
}
#endif

#if !defined(__WIN32__)
static void
ignore_signals (sigset_t *oldset)
{
	sigset_t set;

	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, oldset);
}

static void
unignore_signals (sigset_t *oldset)
{
	sigprocmask(SIG_SETMASK, oldset, 0);
}
#endif

struct htxf_conn **xfers = 0;
int nxfers = 0;

static void rcv_task_file_get (struct htlc_conn *htlc, struct htxf_conn *htxf);
static void rcv_task_file_put (struct htlc_conn *htlc, struct htxf_conn *htxf);

void
hx_rcv_queueupdate (struct htlc_conn *htlc)
{
	u_int32_t ref = 0, pos;
	int i;

	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_HTXF_REF:
				dh_getint(ref);
				break;
			case HTLS_DATA_QUEUE_POSITION:
				dh_getint(pos);
				for (i = 0; i < nxfers; i++) {
					if (xfers[i]->ref == ref) {
						xfers[i]->queue_pos = pos;
						hx_printf_prefix(htlc, 0, INFOPREFIX, "queue position for '%s': %d\n", xfers[i]->path, pos);
						//if (pos == 0 && !xfers[i]->gone)
						//	xfer_ready_write(xfers[i]);
					}
				}
				break;
		}
	dh_end()
}

void
xfer_go (struct htxf_conn *htxf)
{
	struct htlc_conn *htlc;
	struct hldir *hldir;
	char *rfile;
	u_int32_t data_size = 0, rsrc_size = 0;
	u_int8_t rflt[74];
	struct stat sb;

	if (htxf->gone)
		return;
	htxf->gone = 1;
	htlc = htxf->htlc;
	if (htxf->type & XFER_GET)
		htlc->nr_gets++;
	else if (htxf->type & XFER_PUT)
		htlc->nr_puts++;
	if (htxf->type & XFER_BANNER) {
		task_new(htlc, rcv_task_file_get, htxf, 1, "xfer_go");
		hlwrite(htlc, HTLC_HDR_BANNER_GET, 0, 0);
	} else if (htxf->type & XFER_GET) {
		if (!stat(htxf->path, &sb))
			data_size = sb.st_size;
#if defined(CONFIG_HFS)
		rsrc_size = resource_len(htxf->path);
#else
		rsrc_size = 0;
#endif
		if (data_size || rsrc_size) {
			memcpy(rflt, "\
RFLT\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\
\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\2\
DATA\0\0\0\0\0\0\0\0\0\0\0\0\
MACR\0\0\0\0\0\0\0\0\0\0\0\0", 74);
			S32HTON(data_size, &rflt[46]);
			S32HTON(rsrc_size, &rflt[62]);
			htxf->data_pos = data_size;
			htxf->rsrc_pos = rsrc_size;
		}
		task_new(htlc, rcv_task_file_get, htxf, 1, "xfer_go");
		hldir = path_to_hldir(htxf->remotepath, 1);
		if (hldir->dirlen) {
			if (htxf->type & XFER_PREVIEW) {
				hlwrite(htlc, HTLC_HDR_FILE_GET, 0, (data_size || rsrc_size) ? 4 : 3,
					HTLC_DATA_FILE_NAME, hldir->filelen, hldir->file,
					HTLC_DATA_DIR, hldir->dirlen, hldir->dir,
					HTLC_DATA_FILE_PREVIEW, 2, "\0\1",
					HTLC_DATA_RFLT, 74, rflt);
			} else {
				hlwrite(htlc, HTLC_HDR_FILE_GET, 0, (data_size || rsrc_size) ? 3 : 2,
					HTLC_DATA_FILE_NAME, hldir->filelen, hldir->file,
					HTLC_DATA_DIR, hldir->dirlen, hldir->dir,
					HTLC_DATA_RFLT, 74, rflt);
			}
		} else {
			hlwrite(htlc, HTLC_HDR_FILE_GET, 0, (data_size || rsrc_size) ? 2 : 1,
				HTLC_DATA_FILE_NAME, hldir->filelen, hldir->file,
				HTLC_DATA_RFLT, 74, rflt);
		}
		xfree(hldir);
	} else {
		rfile = basename(htxf->path);
		hldir = path_to_hldir(htxf->remotepath, 1);
		if (exists_remote(htlc, htxf->remotepath)) {
			task_new(htlc, rcv_task_file_put, htxf, 1, "xfer_go");
			hlwrite(htlc, HTLC_HDR_FILE_PUT, 0, 3,
				HTLC_DATA_FILE_NAME, strlen(rfile), rfile,
				HTLC_DATA_DIR, hldir->dirlen, hldir->dir,
				HTLC_DATA_FILE_PREVIEW, 2, "\0\1");
		} else {
			task_new(htlc, rcv_task_file_put, htxf, 1, "xfer_go");
			hlwrite(htlc, HTLC_HDR_FILE_PUT, 0, 2,
				HTLC_DATA_FILE_NAME, strlen(rfile), rfile,
				HTLC_DATA_DIR, hldir->dirlen, hldir->dir);
		}
		xfree(hldir);
	}
}

static int
xfer_go_timer (void *__arg)
{
	mutex_lock(&hx_htlc.htxf_mutex);
	xfer_go((struct htxf_conn *)__arg);
	mutex_unlock(&hx_htlc.htxf_mutex);

	return 0;
}

struct htxf_conn *
xfer_new (struct htlc_conn *htlc, const char *path, const char *remotepath, u_int16_t type)
{
	struct htxf_conn *htxf;

	htxf = xmalloc(sizeof(struct htxf_conn));
	memset(htxf, 0, sizeof(struct htxf_conn));
	strcpy(htxf->remotepath, remotepath);
	strcpy(htxf->path, path);
	htxf->type = type;
	if (type & XFER_PREVIEW)
		htxf->preview = 1;

	mutex_lock(&htlc->htxf_mutex);
	xfers = xrealloc(xfers, (nxfers + 1) * sizeof(struct htxf_conn *));
	xfers[nxfers] = htxf;
	nxfers++;
	mutex_unlock(&htlc->htxf_mutex);
	htxf->htlc = htlc;
	htxf->total_pos = 0;
	htxf->total_size = 1;
	hx_output.file_update(htxf);

	mutex_lock(&htlc->htxf_mutex);
	if ((type & XFER_GET && !htlc->nr_gets) || (type & XFER_PUT && !htlc->nr_puts) || (type & XFER_BANNER))
		xfer_go(htxf);
	mutex_unlock(&htlc->htxf_mutex);

	return htxf;
}

void
xfer_tasks_update (struct htlc_conn *htlc)
{
	int i;

	mutex_lock(&htlc->htxf_mutex);
	for (i = 0; i < nxfers; i++) {
		if (xfers[i]->htlc == htlc)
			hx_output.file_update(xfers[i]);
	}
	mutex_unlock(&htlc->htxf_mutex);
}

void
xfer_delete (struct htxf_conn *htxf)
{
	struct htlc_conn *htlc;
	int i, j, type;

	for (i = 0; i < nxfers; i++) {
		if (xfers[i] == htxf) {
#if defined(CONFIG_HTXF_PTHREAD)
			if (htxf->tid) {
				void *thread_retval;

				pthread_cancel(htxf->tid);
				pthread_join(htxf->tid, &thread_retval);
			}
#else
			if (htxf->tid) {
				kill(htxf->tid, SIGKILL);
			} else {
#endif
				if (htxf->stack)
					xfree(htxf->stack);
				htlc = htxf->htlc;
				type = htxf->type;
				timer_delete_ptr(htxf);
				if (nxfers > (i+1))
					memcpy(xfers+i, xfers+i+1, (nxfers-(i+1)) * sizeof(struct htxf_conn *));
				nxfers--;
				if (htxf->gone) {
					if (type & XFER_GET)
						htlc->nr_gets--;
					else if (type & XFER_PUT)
						htlc->nr_puts--;
				}
				if ((type & XFER_GET && !htlc->nr_gets)
				    || (type & XFER_PUT && !htlc->nr_puts)) {
					for (j = 0; j < nxfers; j++) {
						if (xfers[j]->htlc == htlc) {
							xfer_go(xfers[j]);
							break;
						}
					}
				}
				/* free in xfer_pipe_ready_read */
				/* xfree(htxf); */
#if !defined(CONFIG_HTXF_PTHREAD)
			}
#endif
			break;
		}
	}
}

void
xfer_delete_all (void)
{
	struct htxf_conn *htxf;
	int i;

	for (i = 0; i < nxfers; i++) {
		htxf = xfers[i];
#if defined(CONFIG_HTXF_PTHREAD)
		if (htxf->tid) {
			void *thread_retval;

			pthread_cancel(htxf->tid);
			pthread_join(htxf->tid, &thread_retval);
		}
#else
		if (htxf->tid) {
			kill(htxf->tid, SIGKILL);
		} else {
#endif
			if (htxf->stack)
				xfree(htxf->stack);
			timer_delete_ptr(htxf);
			xfree(htxf);
#if !defined(CONFIG_HTXF_PTHREAD)
		}
#endif
	}
	nxfers = 0;
}

#if defined(CONFIG_HTXF_CLONE) || defined(CONFIG_HTXF_FORK)
static int
free_htxf (void *__arg)
{
	int i;
	pid_t pid = (pid_t)__arg;
	struct htxf_conn *htxfp;

	mutex_lock(&hx_htlc.htxf_mutex);
	for (i = 0; i < nxfers; i++) {
		htxfp = xfers[i];
		if (htxfp->tid == pid) {
			htxfp->tid = 0;
			xfer_delete(htxfp);
			break;
		}
	}
	mutex_unlock(&hx_htlc.htxf_mutex);

	return 0;
}
#endif

#if !defined(__WIN32__)
void
hlclient_reap_pid (pid_t pid, int status)
{
	int i;
#if defined(CONFIG_HTXF_CLONE) || defined(CONFIG_HTXF_FORK)
	struct htxf_conn *htxfp;

	/* mutex_lock(&hx_htlc.htxf_mutex); */
	for (i = 0; i < nxfers; i++) {
		htxfp = xfers[i];
		if (htxfp->tid == pid) {
			timer_add_secs(0, free_htxf, (void *)pid);
			return;
		}
	}
	/* mutex_unlock(&hx_htlc.htxf_mutex); */
#endif

	for (i = 0; i < (int)nsounds; i++) {
		if (sounds[i].pid == pid) {
			if ((WIFEXITED(status) && WEXITSTATUS(status)) && sounds[i].beep)
				write(1, "\a", 1);
		}
	}
}
#endif

static int
htxf_connect (struct htxf_conn *htxf)
{
	struct htxf_hdr h;
	int s;

	s = socket(AFINET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0) {
		return -1;
	}
	if (connect(s, (struct sockaddr *)&htxf->listen_sockaddr, sizeof(struct SOCKADDR_IN))) {
		close(s);
		return -1;
	}
	h.magic = htonl(HTXF_MAGIC_INT);
	h.ref = htonl(htxf->ref);
	if (htxf->type & XFER_BANNER)
		h.type = htons(HTXF_TYPE_BANNER);
	else
		h.type = htons(HTXF_TYPE_FILE);
	h.__reserved = 0;
	h.len = htonl(htxf->total_size);
	if (write(s, &h, SIZEOF_HTXF_HDR) != SIZEOF_HTXF_HDR) {
		return -1;
	}

	return s;
}

#if 0
static void
fark (const char *fmt, ...)
{
	FILE *fp;
	va_list ap;
	fp=stderr;
	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
	fflush(fp);
}
#endif

static void
xfer_pipe_close (int fd)
{
	close(fd);
	memset(&hxd_files[fd], 0, sizeof(struct hxd_file));
	hxd_fd_clr(fd, FDR);
	hxd_fd_del(fd);
}

struct file_update {
	u_int32_t type;
	u_int32_t pos;
	struct htxf_conn *htxf;
};

static void
xfer_pipe_ready_read (int fd)
{
	struct htxf_conn *htxf;
	struct htlc_conn *htlc;
	struct file_update fu;
	ssize_t r;

	r = read(fd, &fu, sizeof(fu));
	if (r != sizeof(fu) || (r < 0 && errno != EINTR)) {
		play_sound(snd_file_done);
		xfer_pipe_close(fd);
	} else {
		htxf = fu.htxf;
		htlc = htxf->htlc;
		if (fu.type == 0) {
			/* update */
			htxf->total_pos = fu.pos;
			hx_output.file_update(htxf);
		} else if (fu.type == 1) {
			/* done */
			struct timeval now;
			time_t sdiff, usdiff, Bps;
			char humanbuf[LONGEST_HUMAN_READABLE+1], *bpsstr;

			gettimeofday(&now, 0);
			sdiff = now.tv_sec - htxf->start.tv_sec;
			usdiff = now.tv_usec - htxf->start.tv_usec;
			if (!sdiff)
				sdiff = 1;
			Bps = htxf->total_pos / sdiff;
			bpsstr = human_size(Bps, humanbuf);
			hx_printf_prefix(htlc, 0, INFOPREFIX, "%s: %lu second%s %d bytes, %s/s\n",
					 htxf->path, sdiff, sdiff == 1 ? "," : "s,", htxf->total_pos,
					 bpsstr);
			hx_output.file_update(htxf);
			/* if (htxf->total_pos == htxf->total_size) */
			if (htxf->type & XFER_PREVIEW) {
				struct hl_filelist_hdr *fh;
				char typestr[5];

				fh = fh_lookup(htxf->remotepath);
				if (fh)
					memcpy(typestr, &fh->ftype, 4);
				else
					memcpy(typestr, "TEXT", 4);
				typestr[4] = 0;
				hx_output.file_preview(htlc, htxf->path, typestr);
			}
			close(htxf->pipe);
			/* Done with this */
			xfree(htxf);
			xfer_pipe_close(fd);
		} else {
			/* errors */
			int err = (int)fu.pos;
			switch (fu.type) {
				case 2:
					hx_printf_prefix(htlc, 0, INFOPREFIX,
							 "get_thread: pipe: %s\n", strerror(err));
					break;
				case 3:
					hx_printf_prefix(htlc, 0, INFOPREFIX,
							 "get_thread: fork: %s\n", strerror(err));
					break;
				case 4:
					hx_printf_prefix(htlc, 0, INFOPREFIX,
							 "get_thread: dup2: %s\n", strerror(err));
					break;
				case 5:
					hx_printf_prefix(htlc, 0, INFOPREFIX,
							 "%s: execve: %s\n", htxf->filter_argv[0],
							 strerror(err));
					break;
				default:
					break;
			}
		}
	}
}

static void
xfer_pipe_error (struct htxf_conn *htxf, u_int32_t type, int err)
{
	struct file_update fu;

	fu.type = type;
	fu.pos = (u_int32_t)err;
	fu.htxf = htxf;
	write(htxf->pipe, &fu, sizeof(fu));
}

static void
xfer_pipe_done (struct htxf_conn *htxf)
{
	struct file_update fu;

	fu.type = 1;
	fu.pos = htxf->total_pos;
	fu.htxf = htxf;
	write(htxf->pipe, &fu, sizeof(fu));
	close(htxf->pipe);
}

static void
xfer_pipe_update (struct htxf_conn *htxf)
{
	struct file_update fu;

	fu.type = 0;
	fu.pos = htxf->total_pos;
	fu.htxf = htxf;
	write(htxf->pipe, &fu, sizeof(fu));
}

static int
rd_wr (int rd_fd, int wr_fd, u_int32_t data_len, struct htxf_conn *htxf)
{
	int r, pos, len;
	u_int8_t *buf;
	size_t bufsiz;

	bufsiz = 0xf000;
	buf = malloc(bufsiz);
	if (!buf)
		return 111;
	while (data_len) {
		if ((len = read(rd_fd, buf, (bufsiz < data_len) ? bufsiz : data_len)) < 1)
			return len ? errno : EIO;
		pos = 0;
		while (len) {
			if ((r = write(wr_fd, &(buf[pos]), len)) < 1)
				return errno;
			pos += r;
			len -= r;
			htxf->total_pos += r;
			xfer_pipe_update(htxf);
		}
		data_len -= pos;
	}
	free(buf);

	return 0;
}

static thread_return_type
get_thread (void *__arg)
{
	struct htxf_conn *htxf = (struct htxf_conn *)__arg;
	u_int32_t pos, len, tot_len;
	int s, f, r, retval = 0;
	u_int8_t typecrea[8], buf[1024];
#if defined(CONFIG_HFS)
	struct hfsinfo fi;
#endif

	s = htxf_connect(htxf);
	if (s < 0) {
		retval = s;
		goto ret;
	}

	if (htxf->type & XFER_PREVIEW) {
		tot_len = len = htxf->total_size;
		goto preview;
	}
	len = 40;
	pos = 0;
	while (len) {
		if ((r = socket_read(s, &(buf[pos]), len)) < 1) {
			retval = errno;
			goto ret;
		}
		pos += r;
		len -= r;
		htxf->total_pos += r;
	}
	pos = 0;
	len = (buf[38] ? 0x100 : 0) + buf[39];
	len += 16;
	tot_len = 40 + len;
	while (len) {
		if ((r = socket_read(s, &(buf[pos]), len)) < 1) {
			retval = errno;
			goto ret;
		}
		pos += r;
		len -= r;
		htxf->total_pos += r;
		xfer_pipe_update(htxf);
	}
#if defined(CONFIG_HFS)
	memcpy(typecrea, &buf[4], 8);
	memset(&fi, 0, sizeof(struct hfsinfo));
	fi.comlen = buf[73 + buf[71]];
	memcpy(fi.type, "HTftHTLC", 8);
	memcpy(fi.comment, &buf[74 + buf[71]], fi.comlen);
	*((u_int32_t *)(&buf[56])) = hfs_m_to_htime(*((u_int32_t *)(&buf[56])));
	*((u_int32_t *)(&buf[64])) = hfs_m_to_htime(*((u_int32_t *)(&buf[64])));
	memcpy(&fi.create_time, &buf[56], 4);
	memcpy(&fi.modify_time, &buf[64], 4);
	hfsinfo_write(htxf->path, &fi);
#endif /* CONFIG_HFS */

	L32NTOH(len, &buf[pos - 4]);
	tot_len += len;
preview:
	if (!len)
		goto get_rsrc;
	if ((f = SYS_open(htxf->path, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR)) < 0) {
		retval = errno;
		goto ret;
	}
	if (htxf->data_pos)
		lseek(f, htxf->data_pos, SEEK_SET);
#if !defined(__WIN32__)
	if (htxf->filter_argv) {
		pid_t pid;
		int pfds[2];

		if (pipe(pfds)) {
			xfer_pipe_error(htxf, 2, errno);
			goto no_filter;
		}
		pid = fork();
		switch (pid) {
			case -1:
				xfer_pipe_error(htxf, 3, errno);
				close(pfds[0]);
				close(pfds[1]);
				goto no_filter;
			case 0:
				close(pfds[1]);
				if (pfds[0] != 0) {
					if (dup2(pfds[0], 0) == -1) {
						xfer_pipe_error(htxf, 4, errno);
						_exit(1);
					}
				}
				if (f != 1) {
					if (dup2(f, 1) == -1) {
						xfer_pipe_error(htxf, 4, errno);
						_exit(1);
					}
				}
				close(2);
				if (htxf->data_pos)
					lseek(0, htxf->data_pos, SEEK_SET);
				execve(htxf->filter_argv[0], htxf->filter_argv, hxd_environ);
				xfer_pipe_error(htxf, 5, errno);
				_exit(127);
			default:
				close(pfds[0]);
				close(f);
				f = pfds[1];
		}
	}
no_filter:
#endif
	retval = rd_wr(s, f, len, htxf);
	SYS_fsync(f);
	close(f);
	if (retval)
		goto ret;
get_rsrc:
#if defined(CONFIG_HFS)
	if (tot_len >= htxf->total_size)
		goto done;
	pos = 0;
	len = 16;
	while (len) {
		if ((r = socket_read(s, &(buf[pos]), len)) < 1) {
			retval = errno;
			goto ret;
		}
		pos += r;
		len -= r;
		htxf->total_pos += r;
		xfer_pipe_update(htxf);
	}
	L32NTOH(len, &buf[12]);
	if (!len)
		goto done;
	if ((f = resource_open(htxf->path, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR)) < 0) {
		retval = errno;
		goto ret;
	}
	if (htxf->rsrc_pos)
		lseek(f, htxf->rsrc_pos, SEEK_SET);
	retval = rd_wr(s, f, len, htxf);
	SYS_fsync(f);
	close(f);
	if (retval)
		goto ret;

done:
	memcpy(fi.type, typecrea, 8);
	hfsinfo_write(htxf->path, &fi);
#endif /* CONFIG_HFS */

ret:
	xfer_pipe_done(htxf);
	close(s);
#if defined(CONFIG_HTXF_PTHREAD)
	mutex_lock(&hx_htlc.htxf_mutex);
	htxf->tid = 0;
	xfer_delete(htxf);
	mutex_unlock(&hx_htlc.htxf_mutex);
#endif

	return (thread_return_type)retval;
}

static thread_return_type
put_thread (void *__arg)
{
	struct htxf_conn *htxf = (struct htxf_conn *)__arg;
	int s, f, retval = 0;
	u_int8_t buf[512];
#if defined(CONFIG_HFS)
	struct hfsinfo fi;
#endif

	s = htxf_connect(htxf);
	if (s < 0) {
		retval = s;
		goto ret;
	}

	memcpy(buf, "\
FILP\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\
\2INFO\0\0\0\0\0\0\0\0\0\0\0^AMAC\
TYPECREA\
\0\0\0\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\
\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\
\7\160\0\0\0\0\0\0\7\160\0\0\0\0\0\0\0\0\0\3hxd", 115);
#if defined(CONFIG_HFS)
	hfsinfo_read(htxf->path, &fi);
	if (htxf->rsrc_size - htxf->rsrc_pos)
		buf[23] = 3;
	if (65 + fi.comlen + 12 > 0xff)
		buf[38] = 1;
	buf[39] = 65 + fi.comlen + 12;
	type_creator(&buf[44], htxf->path);
	*((u_int32_t *)(&buf[96])) = hfs_h_to_mtime(*((u_int32_t *)(&fi.create_time)));
	*((u_int32_t *)(&buf[104])) = hfs_h_to_mtime(*((u_int32_t *)(&fi.modify_time)));
	buf[116] = fi.comlen;
	memcpy(buf+117, fi.comment, fi.comlen);
	memcpy(&buf[117] + fi.comlen, "DATA\0\0\0\0\0\0\0\0", 12);
	S32HTON((htxf->data_size - htxf->data_pos), &buf[129 + fi.comlen]);
	if (write(s, buf, 133 + fi.comlen) != (ssize_t)(133 + fi.comlen)) {
		retval = errno;
		goto ret;
	}
	htxf->total_pos += 133 + fi.comlen;
#else
	buf[39] = 65 + 12;
	memcpy(&buf[117], "DATA\0\0\0\0\0\0\0\0", 12);
	S32HTON((htxf->data_size - htxf->data_pos), &buf[129]);
	if (write(s, buf, 133) != 133) {
		retval = errno;
		goto ret;
	}
	htxf->total_pos += 133;
#endif /* CONFIG_HFS */
	if (!(htxf->data_size - htxf->data_pos))
		goto put_rsrc;
	if ((f = SYS_open(htxf->path, O_RDONLY, 0)) < 0) {
		retval = errno;
		goto ret;
	}
	if (htxf->data_pos)
		lseek(f, htxf->data_pos, SEEK_SET);
	retval = rd_wr(f, s, htxf->data_size, htxf);
	if (retval)
		goto ret;
	close(f);

put_rsrc:
#if defined(CONFIG_HFS)
	if (!(htxf->rsrc_size - htxf->rsrc_pos))
		goto done;
	memcpy(buf, "MACR\0\0\0\0\0\0\0\0", 12);
	S32HTON(htxf->rsrc_size, &buf[12]);
	if (write(s, buf, 16) != 16) {
		retval = 0;
		goto ret;
	}
	htxf->total_pos += 16;
	if ((f = resource_open(htxf->path, O_RDONLY, 0)) < 0) {
		retval = errno;
		goto ret;
	}
	if (htxf->rsrc_pos)
		lseek(f, htxf->rsrc_pos, SEEK_SET);
	retval = rd_wr(f, s, htxf->rsrc_size, htxf);
	if (retval)
		goto ret;
	close(f);

done:
#endif /* CONFIG_HFS */

	xfer_pipe_done(htxf);

ret:
	close(s);
#if defined(CONFIG_HTXF_PTHREAD)
	mutex_lock(&hx_htlc.htxf_mutex);
	htxf->tid = 0;
	xfer_delete(htxf);
	mutex_unlock(&hx_htlc.htxf_mutex);
#endif

	return (thread_return_type)retval;
}

static void
xfer_ready_write (struct htxf_conn *htxf)
{
	struct htlc_conn *htlc = htxf->htlc;
#if !defined(__WIN32__)
	sigset_t oldset;
	struct sigaction act, tstpact, contact;
	int pfds[2];
#endif
	int err;

#if !defined(__WIN32__)
	if (pipe(pfds)) {
		hx_printf_prefix(htlc, 0, INFOPREFIX, "xfer: pipe: %s\n", strerror(errno));
		goto err_fd;
	}
	if (pfds[0] >= hxd_open_max) {
		hx_printf_prefix(htlc, 0, INFOPREFIX, "%s:%d: %d >= hxd_open_max (%d)\n", __FILE__, __LINE__, pfds[0], hxd_open_max);
		goto err_pfds;
	}
	htxf->pipe = pfds[1];

	ignore_signals(&oldset);
	act.sa_flags = 0;
	act.sa_handler = SIG_DFL;
	sigfillset(&act.sa_mask);
	sigaction(SIGTSTP, &act, &tstpact);
	sigaction(SIGCONT, &act, &contact);
#else
	htxf->pipe = 0;
#endif
	err = hxd_thread_create(&htxf->tid, &htxf->stack, ((htxf->type & XFER_GET) ? get_thread : put_thread), htxf);
#if !defined(__WIN32__)
	sigaction(SIGTSTP, &tstpact, 0);
	sigaction(SIGCONT, &contact, 0);
	unignore_signals(&oldset);
#endif
	if (err) {
		hx_printf_prefix(htlc, 0, INFOPREFIX, "xfer: thread_create: %s\n", strerror(err));
		goto err_pfds;
	}

#if !defined(CONFIG_HTXF_PTHREAD)
	/* With pthreads, fds are shared by all threads */
	close(pfds[1]);
#endif
#if !defined(__WIN32__)
	hxd_files[pfds[0]].ready_read = xfer_pipe_ready_read;
	hxd_files[pfds[0]].conn.htxf = htxf;
	hxd_fd_add(pfds[0]);
	hxd_fd_set(pfds[0], FDR);
#endif

	return;

err_pfds:
#if !defined(__WIN32__)
	close(pfds[0]);
	close(pfds[1]);
#endif
err_fd:
	mutex_lock(&hx_htlc.htxf_mutex);
	xfer_delete(htxf);
	mutex_unlock(&hx_htlc.htxf_mutex);
}

static void
rcv_task_file_get (struct htlc_conn *htlc, struct htxf_conn *htxf)
{
	u_int32_t ref = 0, size = 0;
	u_int16_t queue_pos = 0;
	int i;

	mutex_lock(&htlc->htxf_mutex);
	for (i = 0; i < nxfers; i++)
		if (xfers[i] == htxf)
			break;
	mutex_unlock(&htlc->htxf_mutex);
	if (i == nxfers)
		return;
	if (task_inerror(htlc)) {
		if (htxf->opt.retry) {
			htxf->gone = 0;
			htxf->opt.retry--;
			timer_add_secs(1, xfer_go_timer, htxf);
		} else {
			mutex_lock(&htlc->htxf_mutex);
			xfer_delete(htxf);
			mutex_unlock(&htlc->htxf_mutex);
		}
		return;
	}

	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_HTXF_SIZE:
				dh_getint(size);
				break;
			case HTLS_DATA_HTXF_REF:
				dh_getint(ref);
				break;
			case HTLS_DATA_QUEUE_POSITION:
				dh_getint(queue_pos);
				break;
			default:
				break;
		}
	dh_end()
	/* if (!size || !ref)
		return; */
	hx_printf_prefix(htlc, 0, INFOPREFIX, "get: %s; %u bytes\n",
		  htxf->path, size, ref);

	if (queue_pos > 0)
		hx_printf_prefix(htlc, 0, INFOPREFIX, "queue position for '%s' is %d\n",htxf->path,queue_pos);
	else
		hx_printf_prefix(htlc, 0, INFOPREFIX, "'%s' can begin immediately\n",htxf->path);

	htxf->queue_pos = queue_pos;

	htxf->ref = ref;
	htxf->total_size = size;
	gettimeofday(&htxf->start, 0);
	htxf->listen_sockaddr = htlc->sockaddr;
	htxf->listen_sockaddr.SIN_PORT = htons(ntohs(htxf->listen_sockaddr.SIN_PORT) + 1);
	if (queue_pos == 0)
		xfer_ready_write(htxf);
}

void
hx_file_get (struct htlc_conn *htlc, char *rpath, int recurs, int retry, int preview, char **filter)
{
	char *lpath, *rfile, **paths, buf[MAXPATHLEN];
	int i, npaths;
	struct htxf_conn *htxf;
	struct cached_filelist *ncfl;
	struct hl_filelist_hdr *fh;
	u_int32_t ftype;

	if (*rpath == dir_char)
		strcpy(buf, rpath);
	else
		snprintf(buf, MAXPATHLEN, "%s%c%s", htlc->rootdir, dir_char, rpath);
	rpath = buf;
	paths = glob_remote(rpath, &npaths, 0);
	for (i = 0; i < npaths; i++) {
		rpath = paths[i];
		fh = fh_lookup(rpath);
		if (fh)
			L32NTOH(ftype, &fh->ftype);
		else
			ftype = 0;
		if (ftype == 0x666c6472 /* 'fldr' */) {
			if (recurs) {
				struct hldir *hldir;

				ncfl = cfl_lookup(rpath);
				ncfl->completing = COMPLETE_GET_R;
				ncfl->filter_argv = filter;
				if (!ncfl->path)
					ncfl->path = xstrdup(rpath);
				hldir = path_to_hldir(rpath, 0);
				task_new(htlc, rcv_task_file_list, ncfl, 1, "get_complete");
				hlwrite(htlc, HTLC_HDR_FILE_LIST, 0, 1,
					HTLC_DATA_DIR, hldir->dirlen, hldir->dir);
				xfree(hldir);
				xfree(rpath);
				continue;
			}
		} else {
			rfile = dirchar_basename(rpath);
			lpath = xstrdup(rfile);
			dirchar_fix(lpath);
			htxf = xfer_new(htlc, lpath, rpath, XFER_GET|(preview?XFER_PREVIEW:0));
			htxf->filter_argv = filter;
			htxf->opt.retry = retry;
			xfree(lpath);
			xfree(rpath);
		}
	}
	xfree(paths);
}

void
hx_banner_get (struct htlc_conn *htlc, const char *lpath)
{
	struct htxf_conn *htxf;

	htxf = xfer_new(htlc, lpath, "", XFER_GET|XFER_PREVIEW|XFER_BANNER);
}

static void
rcv_task_file_put (struct htlc_conn *htlc, struct htxf_conn *htxf)
{
	u_int32_t ref = 0, data_pos = 0, rsrc_pos = 0;
	struct stat sb;
	u_int16_t queue_pos = 0;

	if (task_inerror(htlc)) {
		mutex_lock(&htlc->htxf_mutex);
		xfer_delete(htxf);
		mutex_unlock(&htlc->htxf_mutex);
		return;
	}

	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_HTXF_REF:
				dh_getint(ref);
				break;
			case HTLS_DATA_RFLT:
				if (dh_len >= 66) {
					L32NTOH(data_pos, &dh_data[46]);
					L32NTOH(rsrc_pos, &dh_data[62]);
				}
				break;
			case HTLS_DATA_QUEUE_POSITION:
				dh_getint(queue_pos);
				break;
			default:
				break;
		}
	dh_end()
	if (!ref)
		return;
	htxf->data_pos = data_pos;
	htxf->rsrc_pos = rsrc_pos;
	if (!stat(htxf->path, &sb))
		htxf->data_size = sb.st_size;
	htxf->rsrc_size = sizeof(htxf->path);
	htxf->total_size = 133 + ((htxf->rsrc_size - htxf->rsrc_pos) ? 16 : 0) + strlen(htxf->path)
			 + (htxf->data_size - htxf->data_pos) + (htxf->rsrc_size - htxf->rsrc_pos);
	htxf->ref = ref;
	hx_printf_prefix(htlc, 0, INFOPREFIX, "put: %s; %u bytes\n",
		  htxf->path, htxf->total_size);
	gettimeofday(&htxf->start, 0);
	htxf->listen_sockaddr = htlc->sockaddr;
	htxf->listen_sockaddr.SIN_PORT = htons(ntohs(htxf->listen_sockaddr.SIN_PORT) + 1);
	if (queue_pos == 0)
		xfer_ready_write(htxf);
}

static int
glob_error (const char *epath, int eerrno)
{
	hx_printf_prefix(&hx_htlc, 0, INFOPREFIX, "glob: %s: %s\n", epath, strerror(eerrno));

	return 0;
}

void
hx_file_put (struct htlc_conn *htlc, char *lpath)
{
	struct htxf_conn *htxf;
	char *rpath, buf[MAXPATHLEN];
#if !defined(__WIN32__)
	size_t j;
	glob_t g;

#ifndef GLOB_NOESCAPE	/* BSD has GLOB_QUOTE */
#define GLOB_NOESCAPE	0
#endif
#ifndef GLOB_TILDE	/* GNU extension */
#define GLOB_TILDE	0
#endif
	if (glob(lpath, GLOB_TILDE|GLOB_NOESCAPE|GLOB_NOCHECK, glob_error, &g))
		return;
	for (j = 0; j < (size_t)g.gl_pathc; j++) {
		lpath = g.gl_pathv[j];
		snprintf(buf, sizeof(buf), "%s%c%s", htlc->rootdir, dir_char, basename(lpath));
		rpath = buf;
		htxf = xfer_new(htlc, lpath, rpath, XFER_PUT);
	}
	globfree(&g);
#else
	snprintf(buf, sizeof(buf), "%s%c%s", htlc->rootdir, dir_char, basename(lpath));
	htxf = xfer_new(htlc, lpath, rpath, XFER_PUT);
#endif
}
