/* converts old style .comment directories to .finderinfo directories */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#if !defined(MAXPATHLEN) || MAXPATHLEN > 4095
#define MAXPATHLEN 4095
#endif

/* finder metadata for CAP */
struct hfs_cap_info {
	u_int8_t	fi_fndr[32];	/* Finder's info */
	u_int16_t	fi_attr;	/* AFP attributes (f=file/d=dir) */
	u_int8_t	fi_magic1;	/* Magic number: */
#define HFS_CAP_MAGIC1		0xFF
	u_int8_t	fi_version;	/* Version of this structure: */
#define HFS_CAP_VERSION		0x10
	u_int8_t	fi_magic;	/* Another magic number: */
#define HFS_CAP_MAGIC		0xDA
	u_int8_t	fi_bitmap;	/* Bitmap of which names are valid: */
	u_int8_t	fi_shortfilename[12+1];	/* "short name" (unused) */
	u_int8_t	fi_macfilename[32+1];	/* Original (Macintosh) name */
	u_int8_t	fi_comln;	/* Length of comment (always 0) */
	u_int8_t	fi_comnt[200];	/* Finder comment (unused) */
	/* optional: 	used by aufs only if compiled with USE_MAC_DATES */
	u_int8_t	fi_datemagic;	/* Magic number for dates extension: */
#define HFS_CAP_DMAGIC		0xDA
	u_int8_t	fi_datevalid;	/* Bitmap of which dates are valid: */
#define HFS_CAP_MDATE		0x01
#define HFS_CAP_CDATE		0x02
	u_int8_t	fi_ctime[4];	/* Creation date (in AFP format) */
	u_int8_t	fi_mtime[4];	/* Modify date (in AFP format) */
	u_int8_t	fi_utime[4];	/* Un*x time of last mtime change */
	u_int8_t	pad;
};

#define SIZEOF_HFS_CAP_INFO	300

static int
conv (char *dirpath)
{
	DIR *dir;
	struct dirent *de;
	char pathbuf[MAXPATHLEN], infopath[MAXPATHLEN], compath[MAXPATHLEN], comment[200];
	struct stat sb;
	int err, r, ok = 1, cfd, fifd;
	struct hfs_cap_info cap;

	dir = opendir(dirpath);
	if (!dir) {
		fprintf(stderr, "opendir(%s): %s\n", dirpath, strerror(errno));
		return 1;
	}
	while ((de = readdir(dir))) {
		if (de->d_name[0] == '.' && ((de->d_name[1] == '.' && !de->d_name[2]) || !de->d_name[1]))
			continue;
		snprintf(pathbuf, MAXPATHLEN, "%s/%s", dirpath, de->d_name);
		if ((err = lstat(pathbuf, &sb))) {
			fprintf(stderr, "lstat(%s): %s\n", pathbuf, strerror(errno));
			goto ret;
		}

		snprintf(infopath, MAXPATHLEN, "%s/.finderinfo/%s", dirpath, de->d_name);
		snprintf(compath, MAXPATHLEN, "%s/.comment/%s", dirpath, de->d_name);
		cfd = open(compath, O_RDONLY);
		if (cfd >= 0) {
			r = read(cfd, comment, 200);
			fifd = open(infopath, O_RDWR|O_CREAT, 0644);
			if (fifd >= 0) {
				memset(&cap, 0, sizeof(cap));
				if (r > 0) {
					cap.fi_comln = r;
					memcpy(cap.fi_comnt, comment, r);
				}
				cap.fi_magic1 = HFS_CAP_MAGIC1;
				cap.fi_version = HFS_CAP_VERSION;
				cap.fi_magic = HFS_CAP_MAGIC;
				cap.fi_datemagic = HFS_CAP_DMAGIC;
				/* cap.fi_datevalid = HFS_CAP_MDATE|HFS_CAP_CDATE; */
				if (read(fifd, &cap, 8) < 0) {
					fprintf(stderr, "read: %s\n", strerror(errno));
					ok = 0;
				}
				if (lseek(fifd, 0, SEEK_SET)) {
					fprintf(stderr, "lseek: %s\n", strerror(errno));
					ok = 0;
				}
				if (write(fifd, &cap, SIZEOF_HFS_CAP_INFO) != SIZEOF_HFS_CAP_INFO) {
					fprintf(stderr, "write: %s\n", strerror(errno));
					ok = 0;
				}
				fsync(fifd);
				close(fifd);
			} else ok = 0;
			close(cfd);
			if (ok) {
				err = unlink(compath);
				if (err)
					goto ret;
			}
		}

		if (S_ISDIR(sb.st_mode) && !S_ISLNK(sb.st_mode))
			err = conv(pathbuf);
		if (err)
			goto ret;
	}
	snprintf(compath, MAXPATHLEN, "%s/.comment", dirpath);
	if (rmdir(compath) && errno != ENOENT) {
		err = -1;
		fprintf(stderr, "rmdir(%s): %s\n", compath, strerror(errno));
	} else
		err = 0;

ret:
	closedir(dir);

	return err;
}

int
main (int argc, char **argv)
{
	if (argc < 2) {
		printf("usage: %s <dirpath>\n", argv[0]);
		return 1;
	}

	conv(argv[1]);

	return 0;
}
