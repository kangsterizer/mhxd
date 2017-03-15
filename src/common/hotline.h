#ifndef _HOTLINE_H
#define _HOTLINE_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <sys/types.h>

#ifndef PACKED
//#ifdef __GNUC__
//#define PACKED __attribute__((__packed__))
//#else
#define PACKED
//#endif
#endif

#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#define ZERO_SIZE_ARRAY_SIZE	0
#else
#define ZERO_SIZE_ARRAY_SIZE	1
#endif

struct hl_hdr {
	/* should be:
	 * u_int8_t	flags;
	 * u_int8_t	reply;
	 * u_int16_t	type;
	 */
	u_int32_t	type PACKED,
			trans PACKED,
			flag PACKED,
			totlen PACKED,
			len PACKED;
	u_int16_t	hc PACKED;
	u_int8_t	data[ZERO_SIZE_ARRAY_SIZE] PACKED;
};
#define SIZEOF_HL_HDR		(22)

struct hl_data_hdr {
	u_int16_t	type PACKED,
			len PACKED;
	u_int8_t	data[ZERO_SIZE_ARRAY_SIZE] PACKED;
};
#define SIZEOF_HL_DATA_HDR	(4)

struct htxf_hdr {
	u_int32_t	magic PACKED,
			ref PACKED,
			len PACKED;
	u_int16_t	type PACKED;
	u_int16_t	__reserved PACKED;
};
#define SIZEOF_HTXF_HDR		(16)

#define HTXF_TYPE_FILE		0
#define HTXF_TYPE_FOLDER	1
#define HTXF_TYPE_BANNER	2

struct hl_filelist_hdr {
	u_int16_t	type PACKED,
			len PACKED;
	u_int32_t	ftype PACKED,
			fcreator PACKED;
	u_int32_t	fsize PACKED,
			unknown PACKED;
	u_int16_t	encoding PACKED,
			fnlen PACKED;
	u_int8_t	fname[ZERO_SIZE_ARRAY_SIZE] PACKED;
};
#define SIZEOF_HL_FILELIST_HDR	(24)

struct hl_userlist_hdr {
	u_int16_t	type PACKED,
			len PACKED;
	u_int16_t	uid PACKED,
			icon PACKED,
			color PACKED,
			nlen PACKED;
	u_int8_t	name[ZERO_SIZE_ARRAY_SIZE] PACKED;
};
#define SIZEOF_HL_USERLIST_HDR	(12)

struct hl_news_threadlist_hdr {
	u_int16_t type;
	u_int16_t len;
	u_int32_t __x0 PACKED;
	u_int32_t thread_count PACKED;
	u_int16_t __x1 PACKED;
};
#define SIZEOF_HL_NEWS_THREADLIST_HDR (14)

struct hl_news_thread_hdr {
	u_int32_t id PACKED;
	u_int8_t date[8] PACKED;
	u_int32_t parent_id PACKED;
	u_int32_t flags PACKED;
	u_int16_t part_count PACKED;
	/* subject */
	/* poster */	/* pascal strings */
	/* part_count of the struct below */
	/* mimetype */
	/* u_int16_t datasize */
	u_int8_t data[ZERO_SIZE_ARRAY_SIZE] PACKED;
};
#define SIZEOF_HL_NEWS_THREAD_HDR (22)

struct hl_newslist_hdr {
	u_int16_t type PACKED;
	u_int16_t len PACKED;
	u_int8_t ntype PACKED;
	u_int8_t name[ZERO_SIZE_ARRAY_SIZE] PACKED;
};
#define SIZEOF_HL_NEWSLIST_HDR (5)

struct hl_newslist_extended_hdr {
	u_int16_t type PACKED;
	u_int16_t len PACKED;
	u_int16_t ntype PACKED;
	u_int16_t count PACKED;
};

struct hl_newslist_bundle_hdr {
	u_int16_t type PACKED;
	u_int16_t len PACKED;
	u_int16_t ntype PACKED;
	u_int16_t count PACKED;
	u_int8_t namelen PACKED;
	u_int8_t name[ZERO_SIZE_ARRAY_SIZE] PACKED;
};
#define SIZEOF_HL_NEWSLIST_BUNDLE_HDR (9)

struct hl_newslist_category_hdr {
	u_int16_t type PACKED;
	u_int16_t len PACKED;
	u_int16_t ntype PACKED;
	u_int16_t count PACKED;
	u_int16_t guid_type PACKED;
	u_int16_t guid_type2 PACKED;
	u_int16_t guid_type3 PACKED;
	u_int16_t guid_type4 PACKED;
	u_int16_t guid_type5 PACKED;
	u_int16_t guid_type6 PACKED;
	u_int32_t guid_len PACKED;
	u_int32_t addsn PACKED;
	u_int32_t deletesn PACKED;
	u_int8_t namelen PACKED;
	u_int8_t name[ZERO_SIZE_ARRAY_SIZE] PACKED;
};
#define SIZEOF_HL_NEWSLIST_CATEGORY_HDR (29)

struct htrk_hdr {
	u_int16_t version PACKED;
	u_int16_t port PACKED;
	u_int16_t nusers PACKED;
	u_int16_t __reserved0 PACKED;
	u_int32_t id PACKED;
};
#define SIZEOF_HTRK_HDR		(12)

#ifndef WORDS_BIGENDIAN
#if defined(__BIG_ENDIAN__)
#define WORDS_BIGENDIAN 1
#endif
#endif

/* 64 bit structure */
struct hl_access_bits {
#if WORDS_BIGENDIAN
	u_int32_t delete_files:1,
		  upload_files:1,
		  download_files:1,
		  rename_files:1,
		  move_files:1,
		  create_folders:1,
		  delete_folders:1,
		  rename_folders:1,

		  move_folders:1,
		  read_chat:1,
		  send_chat:1,
		  create_pchats:1,
		  __reserved0:2,
		  create_users:1,
		  delete_users:1,

		  read_users:1,
		  modify_users:1,
		  __reserved1:2,
		  read_news:1,
		  post_news:1,
		  disconnect_users:1,
		  cant_be_disconnected:1,

		  get_user_info:1,
		  upload_anywhere:1,
		  use_any_name:1,
		  dont_show_agreement:1,
		  comment_files:1,
		  comment_folders:1,
		  view_drop_boxes:1,
		  make_aliases:1,

		  can_broadcast:1,
		  delete_articles:1,
		  create_categories:1,
		  delete_categories:1,
		  create_news_bundles:1,
		  delete_news_bundles:1,
		  upload_folders:1,
		  download_folders:1,

		  send_msgs:1,
		  __reserved2:7,
		  __reserved3:8,
		  __reserved4:8;
#else /* assumes little endian */
	u_int32_t rename_folders:1,
		  delete_folders:1,
		  create_folders:1,
		  move_files:1,
		  rename_files:1,
		  download_files:1,
		  upload_files:1,
		  delete_files:1,

		  delete_users:1,
		  create_users:1,
		  __reserved0:2,
		  create_pchats:1,
		  send_chat:1,
		  read_chat:1,
		  move_folders:1,

		  cant_be_disconnected:1,
		  disconnect_users:1,
		  post_news:1,
		  read_news:1,
		  __reserved1:2,
		  modify_users:1,
		  read_users:1,

		  make_aliases:1,
		  view_drop_boxes:1,
		  comment_folders:1,
		  comment_files:1,
		  dont_show_agreement:1,
		  use_any_name:1,
		  upload_anywhere:1,
		  get_user_info:1,

		  download_folders:1,
		  upload_folders:1,
		  delete_news_bundles:1,
		  create_news_bundles:1,
		  delete_categories:1,
		  create_categories:1,
		  delete_articles:1,
		  can_broadcast:1,

		  __reserved2:7,
		  send_msgs:1,
		  __reserved3:8,
		  __reserved4:8;
#endif
};

struct hl_user_data {
	u_int32_t magic PACKED;
	struct hl_access_bits access PACKED;
	u_int8_t pad[516] PACKED;
	u_int16_t nlen PACKED;
	u_int8_t name[134] PACKED;
	u_int16_t llen PACKED;
	u_int8_t login[34] PACKED;
	u_int16_t plen PACKED;
	u_int8_t password[32] PACKED;
};

struct hl_bookmark {
	u_int32_t magic PACKED;
	u_int16_t version PACKED;
	u_int8_t fill1[128] PACKED;
	u_int16_t login_len PACKED;
	u_int8_t login[32] PACKED;
	u_int16_t password_len PACKED;
	u_int8_t password[32] PACKED;
	u_int16_t addr_len PACKED;
	u_int8_t addr[32] PACKED;
	u_int8_t fill2[40] PACKED;
	/* this is openhl specific */
	u_int16_t icon PACKED;
	u_int16_t nick_len PACKED;
	u_int8_t nick[32] PACKED;
	u_int8_t fill3[148] PACKED;
};

#define HL_BOOKMARK_MAGIC "HTsc"

#define HTLC_MAGIC	"TRTPHOTL\0\1\0\2"
#define HTLC_MAGIC_LEN	12
#define HTLS_MAGIC	"TRTP\0\0\0\0"
#define HTLS_MAGIC_LEN	8
#define HTRK_MAGIC	"HTRK\0\1"
#define HTRK_MAGIC_LEN	6
#define HTXF_MAGIC	"HTXF"
#define HTXF_MAGIC_LEN	4
#define HTXF_MAGIC_INT	0x48545846

#define HTRK_TCPPORT	5498
#define HTRK_UDPPORT	5499
#define HTLS_TCPPORT	5500
#define HTXF_TCPPORT	5501

#define HTLC_HDR_NEWSFILE_GET		((u_int32_t) 0x00000065)
#define HTLC_HDR_NEWSFILE_POST		((u_int32_t) 0x00000067)
#define HTLC_HDR_CHAT			((u_int32_t) 0x00000069)
#define HTLC_HDR_LOGIN			((u_int32_t) 0x0000006b)
#define HTLC_HDR_MSG			((u_int32_t) 0x0000006c)
#define HTLC_HDR_USER_KICK		((u_int32_t) 0x0000006e)
#define HTLC_HDR_CHAT_CREATE		((u_int32_t) 0x00000070)
#define HTLC_HDR_CHAT_INVITE		((u_int32_t) 0x00000071)
#define HTLC_HDR_CHAT_DECLINE		((u_int32_t) 0x00000072)
#define HTLC_HDR_CHAT_JOIN		((u_int32_t) 0x00000073)
#define HTLC_HDR_CHAT_PART		((u_int32_t) 0x00000074)
#define HTLC_HDR_CHAT_SUBJECT		((u_int32_t) 0x00000078)
#define HTLC_HDR_AGREEMENTAGREE		((u_int32_t) 0x00000079)
#define HTLC_HDR_FILE_LIST		((u_int32_t) 0x000000c8)
#define HTLC_HDR_FILE_GET		((u_int32_t) 0x000000ca)
#define HTLC_HDR_FILE_PUT		((u_int32_t) 0x000000cb)
#define HTLC_HDR_FILE_DELETE		((u_int32_t) 0x000000cc)
#define HTLC_HDR_FILE_MKDIR		((u_int32_t) 0x000000cd)
#define HTLC_HDR_FILE_GETINFO		((u_int32_t) 0x000000ce)
#define HTLC_HDR_FILE_SETINFO		((u_int32_t) 0x000000cf)
#define HTLC_HDR_FILE_MOVE		((u_int32_t) 0x000000d0)
#define HTLC_HDR_FILE_SYMLINK		((u_int32_t) 0x000000d1)
#define HTLC_HDR_FILE_GETFOLDER		((u_int32_t) 0x000000d2)
#define HTLC_HDR_BANNER_GET		((u_int32_t) 0x000000d4)
#define HTLC_HDR_FILE_PUTFOLDER		((u_int32_t) 0x000000d5)
#define HTLC_HDR_KILLDOWNLOAD		((u_int32_t) 0x000000d6)
#define HTLC_HDR_USER_GETLIST		((u_int32_t) 0x0000012c)
#define HTLC_HDR_USER_GETINFO		((u_int32_t) 0x0000012f)
#define HTLC_HDR_USER_CHANGE		((u_int32_t) 0x00000130)
#define HTLC_HDR_ACCOUNT_CREATE		((u_int32_t) 0x0000015e)
#define HTLC_HDR_ACCOUNT_DELETE		((u_int32_t) 0x0000015f)
#define HTLC_HDR_ACCOUNT_READ		((u_int32_t) 0x00000160)
#define HTLC_HDR_ACCOUNT_MODIFY		((u_int32_t) 0x00000161)
#define HTLC_HDR_MSG_BROADCAST		((u_int32_t) 0x00000163)
#define HTLC_HDR_NEWS_LISTDIR		((u_int32_t) 0x00000172)
#define HTLC_HDR_NEWS_LISTCATEGORY	((u_int32_t) 0x00000173)
#define HTLC_HDR_NEWS_DELETE		((u_int32_t) 0x0000017c)
#define HTLC_HDR_NEWS_MKDIR		((u_int32_t) 0x0000017d)
#define HTLC_HDR_NEWS_MKCATEGORY	((u_int32_t) 0x0000017e)
#define HTLC_HDR_NEWS_GETTHREAD		((u_int32_t) 0x00000190)
#define HTLC_HDR_NEWS_POSTTHREAD	((u_int32_t) 0x0000019a)
#define HTLC_HDR_NEWS_DELETETHREAD	((u_int32_t) 0x0000019b)
#define HTLC_HDR_PING			((u_int32_t) 0x000001f4)

#define HTLC_DATA_CHAT			((u_int16_t) 0x0065)
#define HTLC_DATA_MSG			((u_int16_t) 0x0065)
#define HTLC_DATA_NEWSFILE_POST		((u_int16_t) 0x0065)
#define HTLC_DATA_NAME			((u_int16_t) 0x0066)
#define HTLC_DATA_UID			((u_int16_t) 0x0067)
#define HTLC_DATA_ICON			((u_int16_t) 0x0068)
#define HTLC_DATA_LOGIN			((u_int16_t) 0x0069)
#define HTLC_DATA_PASSWORD		((u_int16_t) 0x006a)
#define HTLC_DATA_HTXF_SIZE		((u_int16_t) 0x006c)
#define HTLC_DATA_STYLE			((u_int16_t) 0x006d)
#define HTLC_DATA_ACCESS		((u_int16_t) 0x006e)
#define HTLC_DATA_BAN			((u_int16_t) 0x0071)
#define HTLC_DATA_CHAT_ID		((u_int16_t) 0x0072)
#define HTLC_DATA_CHAT_SUBJECT		((u_int16_t) 0x0073)
#define HTLC_DATA_CLIENTVERSION		((u_int16_t) 0x00a0)
#define HTLC_DATA_FILE_NAME		((u_int16_t) 0x00c9)
#define HTLC_DATA_DIR			((u_int16_t) 0x00ca)
#define HTLC_DATA_RFLT			((u_int16_t) 0x00cb)
#define HTLC_DATA_FILE_PREVIEW		((u_int16_t) 0x00cc)
#define HTLC_DATA_FILE_COMMENT		((u_int16_t) 0x00d2)
#define HTLC_DATA_FILE_RENAME		((u_int16_t) 0x00d3)
#define HTLC_DATA_DIR_RENAME		((u_int16_t) 0x00d4)
#define HTLC_DATA_FILE_NFILES		((u_int16_t) 0x00dc)
#define HTLC_DATA_NEWS_DIRLIST		((u_int16_t) 0x0140)
#define HTLC_DATA_NEWS_CATLIST		((u_int16_t) 0x0141)
#define HTLC_DATA_NEWS_CATNAME		((u_int16_t) 0x0142)
#define HTLC_DATA_NEWS_DIR		((u_int16_t) 0x0145)
#define HTLC_DATA_NEWS_THREADID		((u_int16_t) 0x0146)
#define HTLC_DATA_NEWS_MIMETYPE		((u_int16_t) 0x0147)
#define HTLC_DATA_NEWS_SUBJECT		((u_int16_t) 0x0148)
#define HTLC_DATA_NEWS_POSTER		((u_int16_t) 0x0149)
#define HTLC_DATA_NEWS_DATE		((u_int16_t) 0x014a)
#define HTLC_DATA_NEWS_PREVTHREADID	((u_int16_t) 0x014b)
#define HTLC_DATA_NEWS_NEXTTHREADID	((u_int16_t) 0x014c)
#define HTLC_DATA_NEWS_POST		((u_int16_t) 0x014d)
#define HTLC_DATA_NEWS_PARENTTHREADID	((u_int16_t) 0x014f)
#define HTLC_DATA_NEWS_NEXTSUBTHREADID	((u_int16_t) 0x0150)
#define HTLC_DATA_NEWS_DELETEREPLIES	((u_int16_t) 0x0151)

#define HTLS_HDR_NEWSFILE_POST		((u_int32_t) 0x00000066)
#define HTLS_HDR_MSG			((u_int32_t) 0x00000068)
#define HTLS_HDR_CHAT			((u_int32_t) 0x0000006a)
#define HTLS_HDR_AGREEMENT		((u_int32_t) 0x0000006d)
#define HTLS_HDR_POLITEQUIT		((u_int32_t) 0x0000006f)
#define HTLS_HDR_CHAT_INVITE		((u_int32_t) 0x00000071)
#define HTLS_HDR_CHAT_USER_CHANGE	((u_int32_t) 0x00000075)
#define HTLS_HDR_CHAT_USER_PART		((u_int32_t) 0x00000076)
#define HTLS_HDR_CHAT_SUBJECT		((u_int32_t) 0x00000077)
#define HTLS_HDR_BANNER			((u_int32_t) 0x0000007a)
#define HTLS_HDR_QUEUE_UPDATE		((u_int32_t) 0x000000d3)
#define HTLS_HDR_USER_CHANGE		((u_int32_t) 0x0000012d)
#define HTLS_HDR_USER_PART		((u_int32_t) 0x0000012e)
#define HTLS_HDR_USER_SELFINFO		((u_int32_t) 0x00000162)
#define HTLS_HDR_MSG_BROADCAST		((u_int32_t) 0x00000163)
#define HTLS_HDR_TASK			((u_int32_t) 0x00010000)
#define HTLS_HDR_PING			((u_int32_t) 0x000001f4)
#define HTLS_HDR_ICON_CHANGE		((u_int32_t) 0x00000748)

#define HTLS_DATA_TASKERROR		((u_int16_t) 0x0064)
#define HTLS_DATA_TEXT			((u_int16_t) 0x0065)
#define HTLS_DATA_NEWS			((u_int16_t) 0x0065)
#define HTLS_DATA_AGREEMENT		((u_int16_t) 0x0065)
#define HTLS_DATA_USER_INFO		((u_int16_t) 0x0065)
#define HTLS_DATA_CHAT			((u_int16_t) 0x0065)
#define HTLS_DATA_MSG			((u_int16_t) 0x0065)
#define HTLS_DATA_NAME			((u_int16_t) 0x0066)
#define HTLS_DATA_UID			((u_int16_t) 0x0067)
#define HTLS_DATA_ICON			((u_int16_t) 0x0068)
#define HTLS_DATA_LOGIN			((u_int16_t) 0x0069)
#define HTLS_DATA_PASSWORD		((u_int16_t) 0x006a)
#define HTLS_DATA_HTXF_REF		((u_int16_t) 0x006b)
#define HTLS_DATA_HTXF_SIZE		((u_int16_t) 0x006c)
#define HTLS_DATA_STYLE			((u_int16_t) 0x006d)
#define HTLS_DATA_ACCESS		((u_int16_t) 0x006e)
#define HTLS_DATA_COLOR			((u_int16_t) 0x0070)
#define HTLS_DATA_CHAT_ID		((u_int16_t) 0x0072)
#define HTLS_DATA_CHAT_SUBJECT		((u_int16_t) 0x0073)
#define HTLS_DATA_QUEUE_POSITION	((u_int16_t) 0x0074)
#define HTLS_DATA_BANNER		((u_int16_t) 0x0097)
#define HTLS_DATA_BANNER_TYPE		((u_int16_t) 0x0098)
#define HTLS_DATA_BANNER_URL		((u_int16_t) 0x0099)
#define HTLS_DATA_NOAGREEMENT		((u_int16_t) 0x009a)
#define HTLS_DATA_SERVERVERSION		((u_int16_t) 0x00a0)
#define HTLS_DATA_BANNERID		((u_int16_t) 0x00a1)
#define HTLS_DATA_SERVERNAME		((u_int16_t) 0x00a2)
#define HTLS_DATA_FILE_LIST		((u_int16_t) 0x00c8)
#define HTLS_DATA_FILE_NAME		((u_int16_t) 0x00c9)
#define HTLS_DATA_RFLT			((u_int16_t) 0x00cb)
#define HTLS_DATA_FILE_TYPE		((u_int16_t) 0x00cd)
#define HTLS_DATA_FILE_CREATOR		((u_int16_t) 0x00ce)
#define HTLS_DATA_FILE_SIZE		((u_int16_t) 0x00cf)
#define HTLS_DATA_FILE_DATE_CREATE	((u_int16_t) 0x00d0)
#define HTLS_DATA_FILE_DATE_MODIFY	((u_int16_t) 0x00d1)
#define HTLS_DATA_FILE_COMMENT		((u_int16_t) 0x00d2)
#define HTLS_DATA_FILE_ICON		((u_int16_t) 0x00d5)
#define HTLS_DATA_FILE_NFILES		((u_int16_t) 0x00dc)
#define HTLS_DATA_USER_LIST		((u_int16_t) 0x012c)
#define HTLS_DATA_NEWS_DIRLIST		((u_int16_t) 0x0140)
#define HTLS_DATA_NEWS_CATLIST		((u_int16_t) 0x0141)
#define HTLS_DATA_NEWS_DIRLIST_EXTENDED	((u_int16_t) 0x0143)
#define HTLS_DATA_NEWS_DIR		((u_int16_t) 0x0145)
#define HTLS_DATA_NEWS_THREADID		((u_int16_t) 0x0146)
#define HTLS_DATA_NEWS_MIMETYPE		((u_int16_t) 0x0147)
#define HTLS_DATA_NEWS_SUBJECT		((u_int16_t) 0x0148)
#define HTLS_DATA_NEWS_POSTER		((u_int16_t) 0x0149)
#define HTLS_DATA_NEWS_DATE		((u_int16_t) 0x014a)
#define HTLS_DATA_NEWS_PREVTHREADID	((u_int16_t) 0x014b)
#define HTLS_DATA_NEWS_NEXTTHREADID	((u_int16_t) 0x014c)
#define HTLS_DATA_NEWS_POST		((u_int16_t) 0x014d)
#define HTLS_DATA_NEWS_PARENTTHREADID	((u_int16_t) 0x014f)
#define HTLS_DATA_NEWS_NEXTSUBTHREADID	((u_int16_t) 0x0150)

/* experimental */
#define HTLC_HDR_FILE_HASH		((u_int32_t) 0x00000ee0)

#define HTLC_DATA_HASH_MD5		((u_int16_t) 0x0e80)
#define HTLC_DATA_HASH_HAVAL		((u_int16_t) 0x0e81)
#define HTLC_DATA_HASH_SHA1		((u_int16_t) 0x0e82)
#define HTLC_DATA_CHAT_AWAY		((u_int16_t) 0x0ea1)

#define HTLS_DATA_HASH_MD5		((u_int16_t) 0x0e80)
#define HTLS_DATA_HASH_HAVAL		((u_int16_t) 0x0e81)
#define HTLS_DATA_HASH_SHA1		((u_int16_t) 0x0e82)
#define HTLS_DATA_ICON_CICN		((u_int16_t) 0x0e90)

/* network */
#define HTLS_DATA_SID			((u_int16_t) 0x0e67)

/* HOPE */
#define HTLS_DATA_SESSIONKEY		((u_int16_t) 0x0e03)
#define HTLC_DATA_SESSIONKEY		((u_int16_t) 0x0e03)
#define HTLS_DATA_MAC_ALG		((u_int16_t) 0x0e04)
#define HTLC_DATA_MAC_ALG		((u_int16_t) 0x0e04)

/* cipher */
#define HTLS_DATA_CIPHER_ALG		((u_int16_t) 0x0ec1)
#define HTLC_DATA_CIPHER_ALG		((u_int16_t) 0x0ec2)
#define HTLS_DATA_CIPHER_MODE		((u_int16_t) 0x0ec3)
#define HTLC_DATA_CIPHER_MODE		((u_int16_t) 0x0ec4)
#define HTLS_DATA_CIPHER_IVEC		((u_int16_t) 0x0ec5)
#define HTLC_DATA_CIPHER_IVEC		((u_int16_t) 0x0ec6)

#define HTLS_DATA_CHECKSUM_ALG		((u_int16_t) 0x0ec7)
#define HTLC_DATA_CHECKSUM_ALG		((u_int16_t) 0x0ec8)
#define HTLS_DATA_COMPRESS_ALG		((u_int16_t) 0x0ec9)
#define HTLC_DATA_COMPRESS_ALG		((u_int16_t) 0x0eca)

/* Avaraline */
#define HTLC_HDR_ICON_GETLIST		((u_int32_t) 0x00000745)
#define HTLC_HDR_ICON_SET               ((u_int32_t) 0x00000746)
#define HTLC_HDR_ICON_GET               ((u_int32_t) 0x00000747)
#define HTLS_HDR_ICON_CHANGE		((u_int32_t) 0x00000748)
#define HTLS_DATA_ICON_GIF		((u_int16_t) 0x0300)
#define HTLC_DATA_ICON_GIF		((u_int16_t) 0x0300)
#define HTLS_DATA_ICON_LIST		((u_int16_t) 0x0301)
#define HTLC_DATA_COLOR			((u_int16_t) 0x0500)

#endif /* ndef _HOTLINE_H */
