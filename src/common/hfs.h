#ifndef __hxd_HFS_H
#define __hxd_HFS_H

#include "hxd.h"
#include <sys/stat.h>

#ifndef PACKED
#ifdef __GNUC__
#define PACKED __attribute__((__packed__))
#else
#define PACKED
#endif
#endif

#define HFS_FORK_CAP		1
#define HFS_FORK_DOUBLE		2
#define HFS_FORK_NETATALK	3

struct avolent {
	struct avolent *next PACKED;
	char extension[7] PACKED;
	char creator[5] PACKED;
	char type[5] PACKED;
};

extern void read_applevolume (const char *avpath);
extern void check_avolume (const char *extension, struct avolent *rav);

/* stuff from linux/include/linux/hfs_fs.h */

/*
 * There are three time systems.  All three are based on seconds since
 * a particular time/date.
 *	Unix:	unsigned lil-endian since 00:00 GMT, Jan. 1, 1970
 *	mac:	unsigned big-endian since 00:00 GMT, Jan. 1, 1904
 *	header:	  SIGNED big-endian since 00:00 GMT, Jan. 1, 2000
 *
 */
#define hfs_h_to_mtime(ARG)	htonl((u_int32_t)ntohl((ARG))+3029529600U)
#define hfs_u_to_mtime(ARG)	htonl((ARG)+2082844800U)
#define hfs_h_to_utime(ARG)	((u_int32_t)(ntohl((ARG))+946684800U))
#define hfs_m_to_utime(ARG)	((ntohl((ARG))-2082844800U))
#define hfs_u_to_htime(ARG)	((int32_t)htonl((ARG)-946684800U))
#define hfs_m_to_htime(ARG)	((int32_t)htonl(ntohl((ARG))-3029529600U))

/* IDs for elements of an AppleDouble or AppleSingle header */
#define HFS_HDR_DATA	1   /* data fork */
#define HFS_HDR_RSRC	2   /* resource fork */
#define HFS_HDR_FNAME	3   /* full (31-character) name */
#define HFS_HDR_COMNT	4   /* comment */
#define HFS_HDR_BWICN	5   /* b/w icon */
#define HFS_HDR_CICON	6   /* color icon info */
#define HFS_HDR_OLDI	7   /* old file info */
#define HFS_HDR_DATES	8   /* file dates info */
#define HFS_HDR_FINFO	9   /* Finder info */
#define HFS_HDR_MACI	10  /* Macintosh info */
#define HFS_HDR_PRODOSI 11  /* ProDOS info */
#define HFS_HDR_MSDOSI  12  /* MSDOS info */
#define HFS_HDR_SNAME   13  /* short name */
#define HFS_HDR_AFPI    14  /* AFP file info */
#define HFS_HDR_DID     15  /* directory id */
#define HFS_HDR_MAX	16

/* magic numbers for Apple Double header files */
#define HFS_DBL_MAGIC		0x00051607
#define HFS_SNGL_MAGIC		0x00051600
#define HFS_HDR_VERSION_1	0x00010000
#define HFS_HDR_VERSION_2	0x00020000

/*
 * A descriptor for a single entry within the header of an
 * AppleDouble or AppleSingle header file.
 * An array of these make up a table of contents for the file.
 */
struct hfs_hdr_descr {
	u_int32_t	id PACKED;	/* The Apple assigned ID for the entry type */
	u_int32_t	offset PACKED;	/* The offset to reach the entry */
	u_int32_t	length PACKED;	/* The length of the entry */
};

#define SIZEOF_HFS_HDR_DESCR	12

/* 
 * Default header layout for Netatalk and AppleDouble
 */
struct hfs_dbl_hdr {
	u_int32_t	magic PACKED;
	u_int32_t	version PACKED;
	u_int8_t	filler[16] PACKED;
	u_int16_t	entries PACKED;
	u_int8_t	descrs[SIZEOF_HFS_HDR_DESCR * HFS_HDR_MAX] PACKED;
};

#define SIZEOF_HFS_DBL_HDR	26

/* finder metadata for CAP */
struct hfs_cap_info {
	u_int8_t	fi_fndr[32] PACKED;	/* Finder's info */
	u_int16_t	fi_attr PACKED;		/* AFP attributes (f=file/d=dir) */
#define HFS_AFP_INV		0x001   /* Invisible bit (f/d) */
#define HFS_AFP_EXPFOLDER	0x002   /* exported folder (d) */
#define HFS_AFP_MULTI		0x002   /* Multiuser bit (f) */
#define HFS_AFP_SYS		0x004   /* System bit (f/d) */
#define HFS_AFP_DOPEN		0x008   /* data fork already open (f) */
#define HFS_AFP_MOUNTED		0x008   /* mounted folder (d) */
#define HFS_AFP_ROPEN		0x010   /* resource fork already open (f) */
#define HFS_AFP_INEXPFOLDER	0x010   /* folder in shared area (d) */
#define HFS_AFP_WRI		0x020	/* Write inhibit bit (readonly) (f) */
#define HFS_AFP_BACKUP		0x040   /* backup needed bit (f/d)  */
#define HFS_AFP_RNI		0x080	/* Rename inhibit bit (f/d) */
#define HFS_AFP_DEI		0x100	/* Delete inhibit bit (f/d) */
#define HFS_AFP_NOCOPY		0x400   /* Copy protect bit (f) */
#define HFS_AFP_RDONLY		(HFS_AFP_WRI|HFS_AFP_RNI|HFS_AFP_DEI)
	u_int8_t	fi_magic1 PACKED;	/* Magic number: */
#define HFS_CAP_MAGIC1		0xFF
	u_int8_t	fi_version PACKED;	/* Version of this structure: */
#define HFS_CAP_VERSION		0x10
	u_int8_t	fi_magic PACKED;	/* Another magic number: */
#define HFS_CAP_MAGIC		0xDA
	u_int8_t	fi_bitmap PACKED;	/* Bitmap of which names are valid: */
#define HFS_CAP_SHORTNAME	0x01
#define HFS_CAP_LONGNAME	0x02
	u_int8_t	fi_shortfilename[12+1] PACKED;	/* "short name" (unused) */
	u_int8_t	fi_macfilename[32+1] PACKED;	/* Original (Macintosh) name */
	u_int8_t	fi_comln PACKED;	/* Length of comment (always 0) */
	u_int8_t	fi_comnt[200] PACKED;	/* Finder comment (unused) */
	/* optional: 	used by aufs only if compiled with USE_MAC_DATES */
	u_int8_t	fi_datemagic PACKED;	/* Magic number for dates extension: */
#define HFS_CAP_DMAGIC		0xDA
	u_int8_t	fi_datevalid PACKED;	/* Bitmap of which dates are valid: */
#define HFS_CAP_MDATE		0x01
#define HFS_CAP_CDATE		0x02
	u_int8_t	fi_ctime[4] PACKED;	/* Creation date (in AFP format) */
	u_int8_t	fi_mtime[4] PACKED;	/* Modify date (in AFP format) */
	u_int8_t	fi_utime[4] PACKED;	/* Un*x time of last mtime change */
	u_int8_t	pad PACKED;
};

#define SIZEOF_HFS_CAP_INFO	300

struct hfsinfo {
	u_int8_t type[4];
	u_int8_t creator[4];
	u_int32_t create_time;
	u_int32_t modify_time;
	u_int32_t rsrclen;
	u_int32_t comlen;
	u_int8_t comment[200];
};

extern int finderinfo_path (char *infopath, const char *path, struct stat *statbuf);
extern int resource_path (char *rsrcpath, const char *path, struct stat *statbuf);
extern int resource_open (const char *path, int mode, int perm);
extern size_t resource_len (const char *path);
extern void type_creator (u_int8_t *buf, const char *path);
extern void hfsinfo_read (const char *path, struct hfsinfo *fi);
extern void hfsinfo_write (const char *path, struct hfsinfo *fi);
extern size_t comment_len (const char *path);
extern void comment_write (const char *path, char *comment, int comlen);

extern void hfs_set_config (long fork, long file_perm, long dir_perm, char *comment);

#endif /* __hxd_HFS_H */
