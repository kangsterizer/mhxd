#include "hfs.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include "getline.h"
#include "xmalloc.h"
#if defined(HAVE_CORESERVICES)
#include "apple/alias.h"
#include "apple/files.h"
#endif

#define DIRCHAR			'/'
#define UNKNOWN_TYPECREA	"TEXTR*ch"

struct hfs_config {
	long fork;
	long file_perm;
	long dir_perm;
	char *comment;
};

struct hfs_config cfg = {HFS_FORK_CAP, 0600, 0700, 0};

void
hfs_set_config (long fork, long file_perm, long dir_perm, char *comment)
{
	cfg.fork = fork;
	cfg.file_perm = file_perm;
	cfg.dir_perm = dir_perm;
	cfg.comment = comment;
}

int
finderinfo_path (char *infopath, const char *path, struct stat *statbuf)
{
	int i, len;
	char *p, pathbuf[MAXPATHLEN];
	struct stat sb;

	len = strlen(path);
	if (len + 16 >= MAXPATHLEN)
		return ENAMETOOLONG;
	strcpy(pathbuf, path);
	for (i = len - 1; i > 0; i--) {
		if (pathbuf[i] == DIRCHAR) {
			pathbuf[i++] = 0;
			break;
		} else if (i == 1) {
			if (pathbuf[0] == DIRCHAR) {
				pathbuf[0] = 0;
				i = 1;
			} else
				i = 0;
			break;
		}
	}

	switch (cfg.fork) {
		default:
		case HFS_FORK_CAP:
			if (i == 0)
				strcpy(infopath, ".finderinfo");
			else
				snprintf(infopath, MAXPATHLEN, "%s/.finderinfo", pathbuf);
			break;
		case HFS_FORK_NETATALK:
			if (!stat(path, &sb) && S_ISDIR(sb.st_mode)) {
				if (i == 0)
					strcpy(infopath, ".AppleDouble");
				else
					snprintf(infopath, MAXPATHLEN, "%s/.AppleDouble", path);
				strcpy(&pathbuf[i], ".Parent");
			} else {
				if (i == 0)
					strcpy(infopath, ".AppleDouble");
				else
					snprintf(infopath, MAXPATHLEN, "%s/.AppleDouble", pathbuf);
			}
			break;
		case HFS_FORK_DOUBLE:
			if (i == 0)
			snprintf(infopath, MAXPATHLEN, "%s/%%%s", pathbuf, &pathbuf[i]);
			goto no_dir;
	}
	if (stat(infopath, &sb)) {
		if (statbuf)
			return ENOENT;
		if (SYS_mkdir(infopath, cfg.dir_perm))
			return errno;
	}
	p = infopath + strlen(infopath);
	*p++ = DIRCHAR;
	*p = 0;
	strcat(infopath, &pathbuf[i]);

no_dir:
	if (statbuf && stat(infopath, statbuf))
		return errno;

	return 0;
}

int
resource_path (char *rsrcpath, const char *path, struct stat *statbuf)
{
	int i, len;
	char *p, pathbuf[MAXPATHLEN];
	struct stat sb;

	len = strlen(path);
	if (len + 16 >= MAXPATHLEN)
		return ENAMETOOLONG;
	strcpy(pathbuf, path);
	for (i = len - 1; i > 0; i--) {
		if (pathbuf[i] == DIRCHAR) {
			pathbuf[i++] = 0;
			break;
		} else if (i == 1) {
			if (pathbuf[0] == DIRCHAR) {
				pathbuf[0] = 0;
				i = 1;
			} else
				i = 0;
			break;
		}
	}

	if (i == 0)
		strcpy(rsrcpath, ".resource");
	else
		snprintf(rsrcpath, MAXPATHLEN, "%s/.resource", pathbuf);
	if (stat(rsrcpath, &sb)) {
		if (statbuf)
			return ENOENT;
		if (SYS_mkdir(rsrcpath, cfg.dir_perm))
			return errno;
	}
	p = rsrcpath + strlen(rsrcpath);
	*p++ = DIRCHAR;
	*p = 0;
	strcat(rsrcpath, &pathbuf[i]);
	if (statbuf && stat(rsrcpath, statbuf))
		return errno;

	return 0;
}

int
resource_open (const char *path, int mode, int perm)
{
	if (cfg.fork == HFS_FORK_CAP) {
		char rsrcpath[MAXPATHLEN];

		if (resource_path(rsrcpath, path, 0))
			return -1;
		return SYS_open(rsrcpath, mode, perm);
	} else { /* AppleDouble */
		char infopath[MAXPATHLEN];
		struct hfs_dbl_hdr dbl;
		struct stat sb;
		ssize_t r;
		u_int16_t i;
		int f;

		if (!finderinfo_path(infopath, path, &sb)) {
			f = SYS_open(infopath, mode, 0);
			if (f < 0)
				return 0;
			r = read(f, &dbl, SIZEOF_HFS_DBL_HDR);
			if (r != SIZEOF_HFS_DBL_HDR)
				goto funkdat;
			if (ntohs(dbl.entries) > HFS_HDR_MAX)
				dbl.entries = htons(HFS_HDR_MAX);
			r = read(f, &dbl.descrs, SIZEOF_HFS_HDR_DESCR * ntohs(dbl.entries));
			if (r != SIZEOF_HFS_HDR_DESCR * ntohs(dbl.entries))
				goto funkdat;
			for (i = 0; i < ntohs(dbl.entries); i++) {
				struct hfs_hdr_descr *descr = (struct hfs_hdr_descr *)(&dbl.descrs[SIZEOF_HFS_HDR_DESCR * i]);
				if (ntohl(descr->id) == HFS_HDR_RSRC) {
					if (lseek(f, ntohl(descr->offset), SEEK_SET) != (off_t)ntohl(descr->offset))
						goto funkdat;
					return f;
				}
			}
funkdat:
			close(f);
			return -1;
		}
	}

	return -1;
}

size_t
resource_len (const char *path)
{
	size_t len = 0;

	if (cfg.fork == HFS_FORK_CAP) {
		char rsrcpath[MAXPATHLEN];
		struct stat sb;

		if (!resource_path(rsrcpath, path, &sb))
			len = sb.st_size;
	} else { /* AppleDouble */
		char infopath[MAXPATHLEN];
		struct hfs_dbl_hdr dbl;
		struct stat sb;
		ssize_t r;
		u_int16_t i;
		int f;

		if (!finderinfo_path(infopath, path, &sb)) {
			f = SYS_open(infopath, O_RDONLY, 0);
			if (f < 0)
				return 0;
			r = read(f, &dbl, SIZEOF_HFS_DBL_HDR);
			if (r != SIZEOF_HFS_DBL_HDR)
				goto funkdat;
			if (ntohs(dbl.entries) > HFS_HDR_MAX)
				dbl.entries = htons(HFS_HDR_MAX);
			r = read(f, &dbl.descrs, SIZEOF_HFS_HDR_DESCR * ntohs(dbl.entries));
			if (r != SIZEOF_HFS_HDR_DESCR * ntohs(dbl.entries))
				goto funkdat;
			for (i = 0; i < ntohs(dbl.entries); i++) {
				struct hfs_hdr_descr *descr = (struct hfs_hdr_descr *)(&dbl.descrs[SIZEOF_HFS_HDR_DESCR * i]);
				if (ntohl(descr->id) == HFS_HDR_RSRC) {
					len = ntohl(descr->length);
					break;
				}
			}
funkdat:
			close(f);
		}
	}

	return len;
}

static inline void
strtolower (char *str)
{
	register char *p;

	for (p = str; *p; p++)
		*p = tolower(*p);
}

static inline char *
suffix (const char *path)
{
	char const *p = path + strlen(path);

	while (p > path) {
		p--;
		if (*p == '.')
			return (char *)p;
		if (*p == DIRCHAR)
			return 0;
	}

	return 0;
}

static void
suffix_type_creator (char *buf, const char *path)
{
	char *sp, suff[16];
	struct avolent avtc;
	int len;

	/* the period (`.') is used to map a type/creator to
	 * all files without an extension */
	sp = suffix(path);
	if (sp) {
		len = strlen(sp);
		if (len > (int)(sizeof(suff)-1))
			len = sizeof(suff)-1;
		memcpy(suff, sp, len);
		suff[len] = 0;
	} else {
		suff[0] = '.';
		suff[1] = 0;
	}

	/* convert the extension to lower case, then look it
	 * up in the extension list */
	strtolower(suff);
	check_avolume(suff, &avtc);

	memcpy(buf, avtc.type, 4);
	memcpy(buf+4, avtc.creator, 4);
}

void
type_creator (u_int8_t *buf, const char *path)
{
	char infopath[MAXPATHLEN];
	struct stat sb;
#ifdef HAVE_CORESERVICES
	u_int8_t is_link;

	if (!resolve_alias_path(path, infopath))
		return;

	if (is_alias(infopath, &is_link))
		return;

	if (is_link)
		strcpy(buf, "alis");
	else {
		char type[5], creator[5];
		if (!mac_get_type(infopath, type, creator)) {
			memcpy(buf, type, 4);
			memcpy(buf + 4, creator, 4);
		}
	}

	if (memcmp(buf, "????????", 8))
		return;
#endif

	if (!finderinfo_path(infopath, path, &sb)) {
		int r, f;

		f = SYS_open(infopath, O_RDONLY, 0);
		if (f < 0)
			goto use_suffix;
		switch (cfg.fork) {
			default:
			case HFS_FORK_CAP:
				r = read(f, buf, 8);
				if (r != 8
				    || (0==buf[0]&&0==buf[1]&&0==buf[2]&&0==buf[3])
				    || (0==buf[4]&&0==buf[5]&&0==buf[6]&&0==buf[7]))
					r = 0;
				break;
			case HFS_FORK_NETATALK:
			case HFS_FORK_DOUBLE:
				{
					struct hfs_dbl_hdr dbl;
					int i;

					r = read(f, &dbl, SIZEOF_HFS_DBL_HDR);
					if (r != SIZEOF_HFS_DBL_HDR) {
						r = -1;
						break;
					}
					if (ntohs(dbl.entries) > HFS_HDR_MAX)
						dbl.entries = htons(HFS_HDR_MAX);
					r = read(f, &dbl.descrs, SIZEOF_HFS_HDR_DESCR * ntohs(dbl.entries));
					if (r != SIZEOF_HFS_HDR_DESCR * ntohs(dbl.entries)) {
						r = -1;
						break;
					}
					for (i = 0; i < ntohs(dbl.entries); i++) {
						struct hfs_hdr_descr *descr = (struct hfs_hdr_descr *)(&dbl.descrs[SIZEOF_HFS_HDR_DESCR * i]);
						if (ntohl(descr->id) == HFS_HDR_FINFO) {
							if (lseek(f, ntohl(descr->offset), SEEK_SET) != (off_t)ntohl(descr->offset))
								continue;
							r = 8;
							r = read(f, buf, 8);
							break;
						}
					}
				}
				break;
		}
		close(f);
		if (r == 8)
			return;
	}

use_suffix:
	suffix_type_creator(buf, path);
}

void
hfsinfo_read (const char *path, struct hfsinfo *fi)
{
	char infopath[MAXPATHLEN];
	int f, i, r;
	struct stat sb;
	union {
		struct hfs_cap_info cap;
		struct hfs_dbl_hdr dbl;
	} hdr;
#ifdef HAVE_CORESERVICES
	char type[5], creator[5];
	char rpath[MAXPATHLEN];
#endif

	memset(fi, 0, sizeof(struct hfsinfo));
	if (!finderinfo_path(infopath, path, &sb)) {
		f = SYS_open(infopath, O_RDONLY, 0);
		if (f >= 0) {
			switch (cfg.fork) {
				case HFS_FORK_CAP:
					r = read(f, &hdr.cap, SIZEOF_HFS_CAP_INFO);
					if (r != SIZEOF_HFS_CAP_INFO)
						break;
					memcpy(fi->type, hdr.cap.fi_fndr, 8);
					if (hdr.cap.fi_datevalid & HFS_CAP_CDATE)
						memcpy(&fi->create_time, hdr.cap.fi_ctime, 4);
					else
						fi->create_time = 0;
					if (hdr.cap.fi_datevalid & HFS_CAP_MDATE)
						memcpy(&fi->modify_time, hdr.cap.fi_mtime, 4);
					else
						fi->modify_time = 0;
					fi->comlen = hdr.cap.fi_comln > 200 ? 200 : hdr.cap.fi_comln;
					memcpy(fi->comment, hdr.cap.fi_comnt, fi->comlen);
					break;
				case HFS_FORK_NETATALK:
				case HFS_FORK_DOUBLE:
					r = read(f, &hdr.dbl, SIZEOF_HFS_DBL_HDR);
					if (r != SIZEOF_HFS_DBL_HDR)
						break;
					if (ntohs(hdr.dbl.entries) > HFS_HDR_MAX)
						hdr.dbl.entries = htons(HFS_HDR_MAX);
					r = read(f, &hdr.dbl.descrs, SIZEOF_HFS_HDR_DESCR * ntohs(hdr.dbl.entries));
					if (r != SIZEOF_HFS_HDR_DESCR * ntohs(hdr.dbl.entries))
						break;
					for (i = 0; i < ntohs(hdr.dbl.entries); i++) {
						struct hfs_hdr_descr *descr = (struct hfs_hdr_descr *)(&hdr.dbl.descrs[SIZEOF_HFS_HDR_DESCR * i]);
						if (lseek(f, ntohl(descr->offset), SEEK_SET) != (off_t)ntohl(descr->offset))
							continue;
						switch (ntohl(descr->id)) {
							case HFS_HDR_COMNT:
								fi->comlen = ntohl(descr->length) > 200 ? 200 : ntohl(descr->length);
								r = read(f, fi->comment, fi->comlen);
								if (r != (int)fi->comlen)
									fi->comlen = 0;
								break;
							case HFS_HDR_OLDI:
							case HFS_HDR_DATES:
								r = read(f, &fi->create_time, 8);
								if (r != 8)
									fi->create_time = fi->modify_time = 0;
								break;
							case HFS_HDR_FINFO:
								r = read(f, fi->type, 8);
								if (r != 8)
									memset(fi->type, 0, 8);
								break;
							case HFS_HDR_RSRC:
								fi->rsrclen = ntohl(descr->length);
								break;
						}
					}
					break;
			}
			close(f);
		}
	}

	if (!(*(u_int32_t *)(&fi->type[0])) || !(*(u_int32_t *)(&fi->type[4])))
		suffix_type_creator(fi->type, path);
#ifdef HAVE_CORESERVICES
	if (resolve_alias_path(path, rpath)
	    && !mac_get_type(path, type, creator)) {
		if (strcmp(type, "????") || strcmp(creator, "????")) {
			memcpy(fi->type, type, 4);
			memcpy(fi->creator, creator, 4);
		}
	}
#endif
	if (!fi->create_time || !fi->modify_time) {
		if (!stat(path, &sb)) {
			u_int32_t htime = hfs_u_to_htime(sb.st_mtime);
			if (!fi->create_time)
				fi->create_time = htime;
			if (!fi->modify_time)
				fi->modify_time = htime;
		}
	}
	if (!fi->comlen && cfg.comment) {
		fi->comlen = strlen(cfg.comment);
		if (fi->comlen > 200)
			fi->comlen = 200;
		memcpy(fi->comment, cfg.comment, fi->comlen);
	}
}

void
hfsinfo_write (const char *path, struct hfsinfo *fi)
{
	char infopath[MAXPATHLEN];
	int f, i, r;
	union {
		struct hfs_cap_info cap;
		struct hfs_dbl_hdr dbl;
	} hdr;
#ifdef HAVE_CORESERVICES
	char type[5], creator[5];
	char rpath[MAXPATHLEN];
#endif

#ifdef HAVE_CORESERVICES
	if (resolve_alias_path(path, rpath)) {
		memcpy(type, fi->type, 4);
		type[4] = 0;
		memcpy(creator, fi->creator, 4);
		creator[4] = 0;
		mac_set_type(rpath, type, creator);
	}
#endif

	if (!finderinfo_path(infopath, path, 0)) {
		f = SYS_open(infopath, O_RDWR|O_CREAT, cfg.file_perm);
		if (f >= 0) {
			switch (cfg.fork) {
				case HFS_FORK_CAP:
					memset(&hdr.cap, 0, sizeof(struct hfs_cap_info));
					hdr.cap.fi_magic1 = HFS_CAP_MAGIC1;
					hdr.cap.fi_version = HFS_CAP_VERSION;
					hdr.cap.fi_magic = HFS_CAP_MAGIC;
					hdr.cap.fi_datemagic = HFS_CAP_DMAGIC;
					hdr.cap.fi_datevalid = HFS_CAP_MDATE|HFS_CAP_CDATE;
					memcpy(hdr.cap.fi_fndr, fi->type, 8);
					memcpy(hdr.cap.fi_ctime, &fi->create_time, 8);
					hdr.cap.fi_comln = fi->comlen > 200 ? 200 : fi->comlen;
					memcpy(hdr.cap.fi_comnt, fi->comment, hdr.cap.fi_comln);
					write(f, &hdr.cap, SIZEOF_HFS_CAP_INFO);
					break;
				case HFS_FORK_NETATALK:
				case HFS_FORK_DOUBLE:
					r = read(f, &hdr.dbl, SIZEOF_HFS_DBL_HDR);
					if (r != SIZEOF_HFS_DBL_HDR) {
						struct hfs_hdr_descr *descr;

#define NENTRIES 4
						hdr.dbl.entries = htons(NENTRIES);
						descr = (struct hfs_hdr_descr *)(&hdr.dbl.descrs[SIZEOF_HFS_HDR_DESCR * 0]);
						descr->id = htonl(HFS_HDR_COMNT);
						descr->offset = htonl(SIZEOF_HFS_DBL_HDR + SIZEOF_HFS_HDR_DESCR * NENTRIES);
						descr->length = htonl(fi->comlen);
						descr = (struct hfs_hdr_descr *)(&hdr.dbl.descrs[SIZEOF_HFS_HDR_DESCR * 1]);
						descr->id = htonl(HFS_HDR_DATES);
						descr->offset = htonl(SIZEOF_HFS_DBL_HDR + SIZEOF_HFS_HDR_DESCR * NENTRIES + fi->comlen);
						descr->length = htonl(8);
						descr = (struct hfs_hdr_descr *)(&hdr.dbl.descrs[SIZEOF_HFS_HDR_DESCR * 2]);
						descr->id = htonl(HFS_HDR_FINFO);
						descr->offset = htonl(SIZEOF_HFS_DBL_HDR + SIZEOF_HFS_HDR_DESCR * NENTRIES + fi->comlen + 8);
						descr->length = htonl(8);
						descr = (struct hfs_hdr_descr *)(&hdr.dbl.descrs[SIZEOF_HFS_HDR_DESCR * 3]);
						descr->id = htonl(HFS_HDR_RSRC);
						descr->offset = htonl(SIZEOF_HFS_DBL_HDR + SIZEOF_HFS_HDR_DESCR * NENTRIES + fi->comlen + 8 + 8);
						descr->length = htonl(0);
					} else {
						if (ntohs(hdr.dbl.entries) > HFS_HDR_MAX)
							hdr.dbl.entries = htons(HFS_HDR_MAX);
						r = read(f, &hdr.dbl.descrs, SIZEOF_HFS_HDR_DESCR * ntohs(hdr.dbl.entries));
						if (r != SIZEOF_HFS_HDR_DESCR * ntohs(hdr.dbl.entries))
							break;
					}
					for (i = 0; i < ntohs(hdr.dbl.entries); i++) {
						struct hfs_hdr_descr *descr = (struct hfs_hdr_descr *)(&hdr.dbl.descrs[SIZEOF_HFS_HDR_DESCR * i]);
						if (lseek(f, ntohl(descr->offset), SEEK_SET) != (off_t)ntohl(descr->offset))
							continue;
						switch (ntohl(descr->id)) {
							case HFS_HDR_COMNT:
								if (r == SIZEOF_HFS_DBL_HDR)
									break;
								descr->length = htonl(fi->comlen);
								write(f, fi->comment, fi->comlen);
								break;
							case HFS_HDR_OLDI:
							case HFS_HDR_DATES:
								if (descr->length < 8)
									descr->length = 8;
								write(f, &fi->create_time, 8);
								break;
							case HFS_HDR_FINFO:
								if (descr->length < 8)
									descr->length = 8;
								write(f, fi->type, 8);
								break;
							case HFS_HDR_RSRC:
								descr->length = htonl(fi->rsrclen);
								break;
						}
					}
					hdr.dbl.magic = htonl(HFS_DBL_MAGIC);
					if (cfg.fork == HFS_FORK_NETATALK)
						hdr.dbl.version = htonl(HFS_HDR_VERSION_1);
					else
						hdr.dbl.version = htonl(HFS_HDR_VERSION_2);
					lseek(f, 0, SEEK_SET);
					write(f, &hdr.dbl, SIZEOF_HFS_DBL_HDR + SIZEOF_HFS_HDR_DESCR * ntohs(hdr.dbl.entries));
					break;
			}
			SYS_fsync(f);
			close(f);
		}
	}
}

size_t
comment_len (const char *path)
{
	char infopath[MAXPATHLEN];
	int f, i, r, len = 0;
	struct stat sb;
	union {
		struct hfs_cap_info cap;
		struct hfs_dbl_hdr dbl;
	} hdr;

	if (!finderinfo_path(infopath, path, &sb)) {
		f = SYS_open(infopath, O_RDONLY, 0);
		if (f >= 0) {
			switch (cfg.fork) {
				case HFS_FORK_CAP:
					if (read(f, &hdr.cap, SIZEOF_HFS_CAP_INFO) == SIZEOF_HFS_CAP_INFO)
						len = hdr.cap.fi_comln > 200 ? 200 : hdr.cap.fi_comln;
					break;
				case HFS_FORK_NETATALK:
				case HFS_FORK_DOUBLE:
					r = read(f, &hdr.dbl, SIZEOF_HFS_DBL_HDR);
					if (r != SIZEOF_HFS_DBL_HDR)
						break;
					if (ntohs(hdr.dbl.entries) > HFS_HDR_MAX)
						hdr.dbl.entries = htons(HFS_HDR_MAX);
					r = read(f, &hdr.dbl.descrs, SIZEOF_HFS_HDR_DESCR * ntohs(hdr.dbl.entries));
					if (r != SIZEOF_HFS_HDR_DESCR * ntohs(hdr.dbl.entries))
						break;
					for (i = 0; i < ntohs(hdr.dbl.entries); i++) {
						struct hfs_hdr_descr *descr = (struct hfs_hdr_descr *)(&hdr.dbl.descrs[SIZEOF_HFS_HDR_DESCR * i]);
						if (ntohl(descr->id) == HFS_HDR_COMNT)
							len = ntohl(descr->length) > 200 ? 200 : ntohl(descr->length);
					}
					break;
			}
			close(f);
		}
	}
	if (!len && cfg.comment) {
		len = strlen(cfg.comment);
		if (len > 200)
			len = 200;
	}

	return len;
}

void
comment_write (const char *path, char *comment, int comlen)
{
	char infopath[MAXPATHLEN];
	int f;
#if 0
	int i, r;
#endif
	union {
		struct hfs_cap_info cap;
		struct hfs_dbl_hdr dbl;
	} hdr;
	if (!finderinfo_path(infopath, path, 0)) {
		f = SYS_open(infopath, O_RDWR|O_CREAT, cfg.file_perm);
		if (f >= 0) {
			if (comlen > 200)
				comlen = 200;
			switch (cfg.fork) {
				case HFS_FORK_CAP:
					if (read(f, &hdr.cap, SIZEOF_HFS_CAP_INFO) != SIZEOF_HFS_CAP_INFO) {
						memset(&hdr.cap, 0, sizeof(struct hfs_cap_info));
						hdr.cap.fi_magic1 = HFS_CAP_MAGIC1;
						hdr.cap.fi_version = HFS_CAP_VERSION;
						hdr.cap.fi_magic = HFS_CAP_MAGIC;
						hdr.cap.fi_datemagic = HFS_CAP_DMAGIC;
						suffix_type_creator(hdr.cap.fi_fndr, path);
					}
					hdr.cap.fi_comln = comlen;
					memcpy(hdr.cap.fi_comnt, comment, comlen);
					lseek(f, 0, SEEK_SET);
					write(f, &hdr.cap, SIZEOF_HFS_CAP_INFO);
					break;
#if 0
				case HFS_FORK_NETATALK:
				case HFS_FORK_DOUBLE:
					r = read(f, &hdr.dbl, SIZEOF_HFS_DBL_HDR);
					if (r != SIZEOF_HFS_DBL_HDR)
						break;
					if (ntohs(hdr.dbl.entries) > HFS_HDR_MAX)
						hdr.dbl.entries = htons(HFS_HDR_MAX);
					r = read(f, &hdr.dbl.descrs, SIZEOF_HFS_HDR_DESCR * ntohs(hdr.dbl.entries));
					if (r != SIZEOF_HFS_HDR_DESCR * ntohs(hdr.dbl.entries))
						break;
					for (i = 0; i < ntohs(hdr.dbl.entries); i++) {
						struct hfs_hdr_descr *descr = (struct hfs_hdr_descr *)(&hdr.dbl.descrs[SIZEOF_HFS_HDR_DESCR * i]);
						if (ntohl(descr->id) == HFS_HDR_COMNT) {
							descr->length = htonl(comlen);
							if (lseek(f, ntohl(descr->offset), SEEK_SET) != (off_t)ntohl(descr->offset))
								continue;
							write(f, comment, comlen);
							if (lseek(f, SIZEOF_HFS_DBL_HDR + SIZEOF_HFS_HDR_DESCR * i, SEEK_SET)
							    != (off_t)(SIZEOF_HFS_DBL_HDR + SIZEOF_HFS_HDR_DESCR * i))
								continue;
							write(f, descr, SIZEOF_HFS_HDR_DESCR);
						}
					}
					break;
#endif
			}
			SYS_fsync(f);
			close(f);
		}
	}
}

/* the apple volume entry (avolent) struct is defined in hfs.h */
static struct avolent *avol_list = 0;

void
read_applevolume (const char *avpath)
{
	char *p, *line;
	char extension[9], type[5], creator[5];
	struct avolent *av, *nextav, dummy_av; 
	int fd, linelen;
	getline_t *gl;

	for (av = avol_list; av; av = nextav) {
		nextav = av->next;
		xfree(av);
	}
	dummy_av.next = 0;
	av = &dummy_av;
	fd = open(avpath, O_RDONLY);
	if (fd < 0)
		return;
	gl = getline_open(fd);
	for (;;) {
		line = getline_line(gl, &linelen);
		if (!line)
			break;
		p = line;
		while (*p && isspace(*p))
			p++;
		if (*p == '#')
			continue;
		if (sscanf(p, "%8s%*1s%4c%*[\"]%*1s%4c", extension, type, creator) < 3)
			continue;
		av->next = xmalloc(sizeof(struct avolent));
		av = av->next;
		av->next = 0;

		/* terminate the strings */
		type[4] = 0;
		creator[4] = 0;

		strtolower(extension);
		strcpy(av->extension, extension);
		strcpy(av->type, type);
		strcpy(av->creator, creator);
	}
	close(fd);
	getline_close(gl);
	avol_list = dummy_av.next;
}

static char *
lastresort_type_creator (const char *suff)
{
	u_int8_t *tc = 0;

	if (!strcmp(suff, "jpg") || !strcmp(suff, "jpeg"))
		tc = "JPEGGKON";
	else if (!strcmp(suff, "png"))
		tc = "PNGfGKON";
	else if (!strcmp(suff, "gif"))
		tc = "GIFfGKON";
	else if (!strcmp(suff, "mp3") || !strcmp(suff, "mp2"))
		tc = "MP3 MAmp";
	else if (!strcmp(suff, "mpg") || !strcmp(suff, "mpeg"))
		tc = "MPEGTVOD";
	else if (!strcmp(suff, "mov"))
		tc = "MooVTVOD";
	else if (!strcmp(suff, "sit"))
		tc = "SITDSIT!";
	else if (!strcmp(suff, "zip") || !strcmp(suff, "pk3"))
		tc = "ZIP ZIP ";
	else if (!strcmp(suff, "app") || !strcmp(suff, "sea"))
		tc = "APPLpeff";
	else if (!strcmp(suff, "img"))
		tc = "rohdWrap";
	else if (!strcmp(suff, "pict"))
		tc = "PICTGKON";
	else if (!strcmp(suff, "wmv"))
		tc = "ASF_Ms01";
	else if (!strcmp(suff, ".c")
		 || !strcmp(suff, ".h")
		 || !strcmp(suff, ".cpp")
		 || !strcmp(suff, ".cxx")
		 || !strcmp(suff, ".txt"))
		tc = "TEXTttxt";


	return tc;
}

void
check_avolume (const char *extension, struct avolent *avtc)
{
	struct avolent *av, *nextav, dummy_av;
	char *tc;

	dummy_av.next = avol_list;
	for (av = avol_list; av; av = nextav) {
		nextav = av->next;
		if (!strcmp(av->extension, extension)) {
			strcpy(avtc->type, av->type);
			strcpy(avtc->creator, av->creator);
			return;
		}
	}

	tc = lastresort_type_creator(extension);
	if (tc) {
		memcpy(avtc->type, tc, 4);
		avtc->type[4] = 0;
		memcpy(avtc->creator, tc+4, 4);
		avtc->creator[4] = 0;
	} else {
		/* the default type/creator codes for unknown extensions */
		strcpy(avtc->type, "????");
		strcpy(avtc->creator, "????");
	}
}
