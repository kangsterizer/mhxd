#ifndef _HXD_H
#define _HXD_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if !defined(__GNUC__) || defined(__STRICT_ANSI__) || defined(__APPLE_CC__)
#define __attribute__(x)
#endif
#include "sys_deps.h"
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#if defined(HAVE_SYS_SELECT_H)
#include <sys/select.h>
#endif
#include <limits.h>
#include <sys/param.h>
#if defined(__WIN32__)
#include <winsock.h>
#else
#include <netinet/in.h>
#endif
#include <stdlib.h>
#include "hotline.h"

#include "threads.h"

#if defined(CONFIG_UNIXSOCKETS)
#include <sys/un.h>
#endif

#include "hxd_config.h"

extern struct hxd_config hxd_cfg;

extern void hxd_config_init (struct hxd_config *cfg);
extern void hxd_config_read (const char *file, void *mem, int perr);
extern void hxd_config_free (void *cfgmem);

extern pid_t hxd_pid;

/* IPv6 */
#ifdef CONFIG_IPV6
#define HOSTLEN 63
#define SOCKADDR_IN sockaddr_in6
#define SIN_PORT sin6_port
#define SIN_FAMILY sin6_family
#define SIN_ADDR sin6_addr
#define S_ADDR s6_addr
#define AFINET AF_INET6
#define IN_ADDR in6_addr
#else
/* IPv4 */
#define HOSTLEN 15
#define SOCKADDR_IN sockaddr_in
#define SIN_PORT sin_port
#define SIN_FAMILY sin_family
#define SIN_ADDR sin_addr
#define S_ADDR s_addr
#define AFINET AF_INET
#define IN_ADDR in_addr
#endif

extern void inaddr2str (char abuf[HOSTLEN+1], struct SOCKADDR_IN *sa);

#ifdef CONFIG_COMPRESS
#include "compress.h"
#endif

#ifdef CONFIG_CIPHER
#include "cipher.h"
#endif

#ifndef MAXPATHLEN
#ifdef PATH_MAX
#define MAXPATHLEN PATH_MAX
#else
#define MAXPATHLEN 4095
#endif
#endif

#if MAXPATHLEN > 4095
#undef MAXPATHLEN
#define MAXPATHLEN 4095
#endif

struct qbuf {
	u_int32_t pos, len;
	u_int8_t *buf;
};

struct extra_access_bits {
	u_int32_t chat_private:1,
		  msg:1,
		  user_getlist:1,
		  file_list:1,
		  file_getinfo:1,
		  file_hash:1,
		  can_login:1,
		  user_visibility:1,
		  user_color:1,
		  can_spam:1,
		  set_subject:1,
		  debug:1,
		  user_access:1,
		  access_volatile:1,
		  user_0wn:1,
		  is_0wn3d:1,
		  manage_users:1,
		  info_get_address:1,
		  info_get_login:1,
		  name_lock:1,
		  can_agree:1,
		  can_ping:1,
		  banner_get:1,
		  ignore_queue:1,
		  __reserved:8;
};

struct htlc_chat {
	struct htlc_chat *next, *prev;
	u_int32_t ref;
	u_int32_t nusers;
	u_int8_t subject[256];
	u_int8_t password[32];
	u_int16_t subjectlen;
	u_int16_t passwordlen;
	fd_set fds;
	fd_set invite_fds;
};

/* governs default attributes of the client at login time --Devin */
struct login_defaults {
       u_int32_t color:16,
       invisibility:1,
       icon:16;
       u_int8_t has_default_color:1,
       has_default_icon:1,
       has_default_invisibility:1,
       reserved:4;
       char name[32];
};	

struct htlc_conn;
typedef struct htlc_conn htlc_t;

struct htxf_conn {
	int fd;
	int pipe;
	u_int32_t data_size, data_pos, rsrc_size, rsrc_pos;
	u_int32_t total_size, total_pos;
	u_int32_t ref;
	u_int32_t limit_out_Bps;
	u_int16_t queue_pos;
	u_int16_t preview;
	u_int16_t gone;
	u_int16_t type;
	struct SOCKADDR_IN sockaddr;
	struct SOCKADDR_IN listen_sockaddr;
	struct qbuf in;
	struct timeval start;

	struct htlc_conn *htlc;
	hxd_thread_t tid;
	void *stack;

#if defined(CONFIG_HOTLINE_CLIENT)
	char **filter_argv;
	struct {
		u_int32_t retry:1,
			  reserved:31;
	} opt;
#endif

	char path[MAXPATHLEN];
	char remotepath[MAXPATHLEN];
};

#if defined(CONFIG_HOTLINE_CLIENT)
struct hx_chat;
#endif

#if defined(CONFIG_HOTLINE_SERVER)
#include "protocols.h"
#endif

struct htlc_conn {
	struct htlc_conn *next, *prev;
	int fd;
	int wfd;
	int identfd;
	void (*rcv)(struct htlc_conn *);
	void (*real_rcv)(struct htlc_conn *);
	struct qbuf in, out;
	struct qbuf read_in;
	struct SOCKADDR_IN sockaddr;
#if defined(CONFIG_UNIXSOCKETS)
	struct sockaddr_un usockaddr;
#endif
#if defined(CONFIG_HOTLINE_SERVER)
	struct protocol *proto;
	void *proto_data;
#endif
	u_int32_t trans;
	u_int32_t replytrans;
	u_int32_t icon;
	u_int16_t sid;
	u_int16_t uid;
	u_int16_t color;
	u_int16_t clientversion;
	u_int16_t serverversion;
	u_int16_t icon_gif_len;
	u_int8_t *icon_gif;
	
	struct timeval login_tv;
	struct timeval idle_tv;

#define AWAY_PERM		1
#define AWAY_INTERRUPTABLE	2
#define AWAY_INTERRUPTED	3
	struct {
		u_int32_t visible:1,
			  away:2,
			  is_hl:1,
			  is_tide:1,
			  is_aniclient:1,
			  is_heidrun:1,
			  is_frogblast:1,
			  is_irc:1,
			  is_kdx:1,
			  in_ping:1,
			  in_login:1,
			  sock_unix:1,
			  reserved:20;
	} flags; 

	struct hl_access_bits access;
	struct extra_access_bits access_extra;
	struct login_defaults defaults;

	char rootdir[MAXPATHLEN], newsfile[MAXPATHLEN], dropbox[MAXPATHLEN];
	u_int8_t userid[516];
	u_int8_t name[32];
	u_int8_t login[32];
	u_int8_t host[256];

#if defined(CONFIG_EXEC)
	u_int16_t nr_execs, exec_limit;
#endif
	u_int16_t nr_puts, put_limit, nr_gets, get_limit;
	u_int32_t limit_out_Bps;
	u_int32_t limit_uploader_out_Bps;
	u_int32_t cmd_output_chatref;

#define HTXF_PUT_MAX	16
#define HTXF_GET_MAX	16
	struct htxf_conn *htxf_in[HTXF_PUT_MAX], *htxf_out[HTXF_GET_MAX];
	hxd_mutex_t htxf_mutex;

#if defined(CONFIG_NOSPAM)
	int spam_points;
	int spam_max;
	int spam_time_limit;
	int spam_time;
	int spam_chat_lines;
	int spam_chat_max;
	int spam_chat_time_limit;
	int spam_chat_time;
#endif

#if defined(CONFIG_HOPE)
	u_int8_t macalg[32];
	u_int8_t sessionkey[64];
	u_int16_t sklen;
#endif

#if defined(CONFIG_CIPHER)
	u_int8_t cipheralg[32];
	union cipher_state cipher_encode_state;
	union cipher_state cipher_decode_state;
	u_int8_t cipher_encode_key[32];
	u_int8_t cipher_decode_key[32];
	/* keylen in bytes */
	u_int8_t cipher_encode_keylen, cipher_decode_keylen;
	u_int8_t cipher_encode_type, cipher_decode_type;
#if defined(CONFIG_COMPRESS)
	u_int8_t zc_hdrlen;
	u_int8_t zc_ran;
#endif
#endif

#if defined(CONFIG_COMPRESS)
	u_int8_t compressalg[32];
	union compress_state compress_encode_state;
	union compress_state compress_decode_state;
	u_int16_t compress_encode_type, compress_decode_type;
	unsigned long gzip_inflate_total_in, gzip_inflate_total_out;
	unsigned long gzip_deflate_total_in, gzip_deflate_total_out;
#endif

#if defined(CONFIG_HOTLINE_CLIENT)
	struct hx_chat *chat_list;
	u_int32_t news_len;
	u_int8_t *news_buf;
	u_int8_t password[32];
	int secure;
#endif
};

/* time after select() returns */
extern struct timeval loopZ_timeval;

extern struct timeval server_start_time;

struct htrk_conn {
	struct qbuf in;
	struct qbuf out;
	int state;
};

struct hxd_file {
	union {
		void *ptr;
		struct htlc_conn *htlc;
		struct htrk_conn *htrk;
		struct htxf_conn *htxf;
	} conn;
	void (*ready_read)(int fd);
	void (*ready_write)(int fd);
};

extern struct hxd_file *hxd_files;

extern int hxd_open_max;
extern int nr_open_files;

extern int high_fd;

extern fd_set hxd_rfds, hxd_wfds;

extern void hxd_fd_add (int fd);
extern void hxd_fd_del (int fd);
extern void hxd_fd_set (int fd, int rw);
extern void hxd_fd_clr (int fd, int rw);
#define FDR	1
#define FDW	2

extern const char *hxd_version;

extern char **hxd_environ;

/* Misc. library prototypes */
#if (defined(CONFIG_HOTLINE_SERVER) || defined(CONFIG_HOTLINE_CLIENT))
#if !defined(HAVE_INET_NTOA_R)
extern int inet_ntoa_r (struct in_addr in, char *buf, size_t buflen);
#endif
#if !defined(HAVE_BASENAME)
extern char *basename (char *path);
#endif
#endif

#if !defined(HAVE_STRLCPY)
extern size_t strlcpy (char *dst, const char *src, size_t siz);
#endif
#if !defined(HAVE_INET_ATON)
extern int inet_aton (const char *cp, struct in_addr *ia);
#endif
#if !defined(HAVE_LOCALTIME_R)
#if !defined(__WIN32__)
#include <time.h>
extern struct tm *localtime_r (const time_t *t, struct tm *tmp);
#endif
#endif
#if !defined(HAVE_SNPRINTF) || defined(__hpux__)
extern int snprintf (char *str, size_t count, const char *fmt, ...);
#endif
#if !defined(HAVE_VSNPRINTF) || defined(__hpux__)
#include <stdarg.h>
extern int vsnprintf (char *str, size_t count, const char *fmt, va_list ap);
#endif

#ifndef RETSIGTYPE
#define RETSIGTYPE void
#endif

extern char *sys_realpath (const char *pathname, char *result);

extern int socket_blocking (int fd, int on);
extern int fd_blocking (int fd, int on);
extern int fd_closeonexec (int fd, int on);
extern int fd_lock_write (int fd);

extern void hxd_log (const char *fmt, ...);

extern void timer_add (struct timeval *tv, int (*fn)(), void *ptr);
extern void timer_delete_ptr (void *ptr);
extern void timer_add_secs (time_t secs, int (*fn)(), void *ptr);
extern time_t tv_secdiff (struct timeval *tv0, struct timeval *tv1);

extern void qbuf_set (struct qbuf *q, u_int32_t pos, u_int32_t len);
extern void qbuf_add (struct qbuf *q, void *buf, u_int32_t len);

extern void htlc_flush_close (struct htlc_conn *htlc);
extern void htlc_close (struct htlc_conn *htlc);

extern void hlwrite (struct htlc_conn *htlc, u_int32_t type, u_int32_t flag, int hc, ...);
/* returns hoff */
extern u_int32_t hlwrite_hdr (struct htlc_conn *htlc, u_int32_t type, u_int32_t flag);
extern void hlwrite_data (struct htlc_conn *htlc, u_int32_t hoff, u_int16_t type, u_int16_t len, void *data);
extern void hlwrite_end (struct htlc_conn *htlc, u_int32_t hoff);
extern void hlwrite_dhdrs (struct htlc_conn *htlc, u_int16_t dhdrcount, struct hl_data_hdr **dhdrs);

extern void hl_code (void *__dst, const void *__src, size_t len);
#define hl_decode(d,s,l) hl_code(d,s,l)
#define hl_encode(d,s,l) hl_code(d,s,l)

#if defined(CONFIG_HOPE)
extern unsigned int random_bytes (u_int8_t *buf, unsigned int nbytes);
extern u_int16_t hmac_xxx (u_int8_t *md, u_int8_t *key, u_int32_t keylen,
			   u_int8_t *text, u_int32_t textlen, u_int8_t *macalg);
#endif

#define atou32(_str) ((u_int32_t)strtoul(_str, 0, 0))
#define atou16(_str) ((u_int16_t)strtoul(_str, 0, 0))

static inline void
memory_copy (void *__dst, void *__src, unsigned int len)
{
	u_int8_t *dst = __dst, *src = __src;

	for (; len; len--)
		*dst++ = *src++;
}

/* data must be accessed from locations that are aligned on multiples of the data size */
#define dh_start(_htlc)		\
{				\
	struct hl_data_hdr *dh = (struct hl_data_hdr *)(&((_htlc)->in.buf[SIZEOF_HL_HDR]));	\
	u_int32_t _pos, _max;		\
	u_int16_t dh_type, dh_len;	\
	u_int8_t *dh_data;		\
	dh_len = ntohs(dh->len);	\
	dh_data = dh->data;		\
	dh_type = ntohs(dh->type);	\
	for (_pos = SIZEOF_HL_HDR, _max = (_htlc)->in.pos;	\
	     _pos + SIZEOF_HL_DATA_HDR <= _max && dh_len <= ((_max - _pos) - SIZEOF_HL_DATA_HDR); \
	     _pos += SIZEOF_HL_DATA_HDR + dh_len,	\
	     dh = (struct hl_data_hdr *)(((u_int8_t *)dh) + SIZEOF_HL_DATA_HDR + dh_len),	\
		memory_copy(&dh_type, &dh->type, 2), dh_type = ntohs(dh_type),	\
		memory_copy(&dh_len, &dh->len, 2), dh_len = ntohs(dh_len),		\
		dh_data = dh->data) {\

#define L32NTOH(_word, _addr) \
	do { u_int32_t _x; memory_copy(&_x, (_addr), 4); _word = ntohl(_x); } while (0)
#define S32HTON(_word, _addr) \
	do { u_int32_t _x; _x = htonl(_word); memory_copy((_addr), &_x, 4); } while (0)
#define L16NTOH(_word, _addr) \
	do { u_int16_t _x; memory_copy(&_x, (_addr), 2); _word = ntohs(_x); } while (0)
#define S16HTON(_word, _addr) \
	do { u_int16_t _x; _x = htons(_word); memory_copy((_addr), &_x, 2); } while (0)

#define dh_getint(_word)			\
do {						\
	if (dh_len == 4)			\
		L32NTOH(_word, dh_data);	\
	else /* if (dh_len == 2) */		\
		L16NTOH(_word, dh_data);	\
} while (0)

#define dh_getstr(_buf)							\
do {									\
	int _len = dh_len > sizeof(_buf)-1 ? sizeof(_buf)-1 : dh_len;	\
	memcpy(_buf, dh_data, _len);					\
	(_buf)[_len] = 0;						\
} while (0)

#define dh_end()	\
	}		\
}

/* Hotline uses CR (mac style) for linebreaks */
#define X2X(_ptr, _len, _x1, _x2) \
do {						\
	char *_p = _ptr, *_end = _ptr + _len;	\
	for ( ; _p < _end; _p++)		\
		if (*_p == _x1)			\
			*_p = _x2;		\
} while (0)

#define CR2LF(_ptr, _len)	X2X(_ptr, _len, '\r', '\n')
#define LF2CR(_ptr, _len)	X2X(_ptr, _len, '\n', '\r')

#endif /* ndef _HXD_H */
