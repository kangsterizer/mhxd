#ifndef _HLSERVER_H
#define _HLSERVER_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "hxd.h"

extern void htxf_init (struct SOCKADDR_IN *saddr);
extern void htxf_close (int fd);

/* hxd server global variables */

extern struct htlc_conn *htlc_list, *htlc_tail;

extern u_int16_t nhtlc_conns;

extern u_int16_t nr_gets;
extern u_int16_t nr_puts;

/* SQL prototypes */
#if defined(CONFIG_SQL)
extern void init_database (const char *host, const char *user, const char *pass, const char *data);
extern void sql_init_user_tbl (void);
extern void sql_add_user (const char *userid, const char *nick, const char *ipaddr, int port, const char *login, int uid, int icon, int color);
extern void sql_modify_user(const char *nick, int icon, int color, int uid);
extern void sql_delete_user (const char *userid, const char *nick, const char *ipaddr, int port, const char *login, int uid);
extern void sql_download (const char *nick, const char *ipaddr, const char *login, const char *path);
extern void sql_upload (const char *nick, const char *ipaddr, const char *login, const char *path);
extern void sql_user_kick(const char *nick, const char *ipaddr, const char *login, const char *knick, const char *klogin);
extern void sql_user_ban (const char *nick, const char *ipaddr, const char *login, const char *knick, const char *klogin);
extern void sql_exec (const char *nick, const char *login, const char *ipaddr, const char *cmdpath, const char *thisarg);
extern void sql_start (const char *version);
extern void sql_query (const char *fmt, ...);
#endif

/* Misc. library prototypes */

#if (defined(CONFIG_HOTLINE_SERVER) || defined(CONFIG_HOTLINE_CLIENT))
#if !defined(HAVE_INET_NTOA_R)
extern int inet_ntoa_r (struct in_addr in, char *buf, size_t buflen);
#endif
#if !defined(HAVE_BASENAME)
extern char *basename (char *path);
#endif
#if !defined(HAVE_STRLCPY)
extern size_t strlcpy (char *dst, const char *src, size_t siz);
#endif
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

#if defined(CONFIG_HOTLINE_SERVER)
#if !defined(HAVE_STRCASESTR)
extern char *strcasestr (char *haystack, char *needle);
#endif
#if !defined(HAVE_REALPATH)
extern char *realpath (const char *pathname, char *result);
#endif
#if !defined(HAVE_STRPTIME)
extern char *strptime(char *, const char *, struct tm *);
#endif
#endif /* CONFIG_HOTLINE_SERVER */

/* hxd server function prototypes */

#if defined(CONFIG_TRACKER_REGISTER)
extern int tracker_register_timer (void *__arg);
#endif

extern void hxd_log (const char *fmt, ...);

extern void start_ident (struct htlc_conn *htlc);
extern int check_banlist (struct htlc_conn *htlc);
#include <time.h>
extern void addto_banlist (time_t t, const char *name, const char *login, const char *user, const char *address, const char *message);

extern void snd_strerror (struct htlc_conn *htlc, int err);
extern void snd_errorstr (struct htlc_conn *htlc, const u_int8_t *str);

extern int account_read (const char *login, char *password, char *name, struct hl_access_bits *acc);
extern int account_write (const char *login, const char *password, const char *name, const struct hl_access_bits *acc);
extern int account_delete (const char *login);
extern int account_trusted (const char *login, const char *userid, const char *addr);
extern void account_getconf (struct htlc_conn *htlc);
extern void account_get_access_extra (struct htlc_conn *htlc);

extern void news_send_file (struct htlc_conn *htlc);
extern void news_save_post (char *newsfile, u_int8_t *buf, u_int16_t len);
extern int agreement_send_file (struct htlc_conn *htlc);

extern int chat_isset (struct htlc_conn *, struct htlc_chat *, int invite);
extern struct htlc_chat *chat_lookup_ref (u_int32_t);
extern void chat_remove_from_all (struct htlc_conn *htlc);

extern void command_chat (struct htlc_conn *htlc, u_int32_t cid, char *chatbuf);

extern void toggle_away (struct htlc_conn *htlc);
extern void test_away (struct htlc_conn *htlc);
extern int away_timer (struct htlc_conn *htlc);

extern struct htlc_conn *isclient (u_int16_t uid);
extern u_int16_t uid_assign (struct htlc_conn *htlc);

#define mangle_uid(htlc)	(htlc->uid)

#if defined(CONFIG_MODULES)
extern void server_modules_load();
extern void server_modules_reload();
#endif

#endif /* ndef _HLSERVER_H */
