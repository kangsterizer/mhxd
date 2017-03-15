#ifndef __hxd_HX_H
#define __hxd_HX_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/param.h>

#include "hotline.h"

#include "news.h"

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

extern struct htlc_conn hx_htlc;

extern char last_msg_nick[32];

extern u_int8_t dir_char;

#define RED		"\033[0;31m"
#define RED_BOLD	"\033[1;31m"
#define GREEN		"\033[0;32m"
#define GREEN_BOLD	"\033[1;32m"
#define DEFAULT		"\033[0m"
#define BLACK		"\033[0;30m"
#define BLACK_BOLD	"\033[1;30m"
#define ORANGE		"\033[0;33m"
#define ORANGE_BOLD	"\033[1;33m"
#define BLUE		"\033[0;34m"
#define BLUE_BOLD	"\033[1;34m"
#define BLUE_BACK	"\033[1;44m"
#define WHITE		"\033[0;37m"
#define WHITE_BOLD	"\033[1;37m"

extern char *g_info_prefix;
#define INFOPREFIX	(g_info_prefix)
#define DEFAULT_INFOPREFIX	" \033[36m[\033[0m\033[32mhx\033[0m\033[36m]\033[0m "
extern char *g_user_colors[4];
#define USERCOLORS	(g_user_colors)
#define DEFAULT_USER_COLORS	{GREEN_BOLD, GREEN, RED_BOLD, RED}

extern char *colorstr (u_int16_t color);

struct hx_chat;

extern void hx_printf (struct htlc_conn *htlc, struct hx_chat *chat, const char *fmt, ...);
extern void hx_printf_prefix (struct htlc_conn *htlc, struct hx_chat *chat, const char *prefix, const char *fmt, ...);
extern void hx_save (struct htlc_conn *htlc, struct hx_chat *chat, const char *filename);
extern void hx_load (struct htlc_conn *htlc, struct hx_chat *chat, const char *filename);

extern void hx_exit (int stat);

extern void gen_command_hash (void);
extern void hx_command (struct htlc_conn *htlc, struct hx_chat *chat, char *str);
extern void hx_send_chat (struct htlc_conn *htlc, u_int32_t cid, const char *str, size_t len, u_int16_t opt);
extern void hotline_client_input (struct htlc_conn *htlc, struct hx_chat *chat, char *str);
extern int hotline_client_tab (struct htlc_conn *htlc, struct hx_chat *chat, char *buffer, int point);

extern int hist_size;

extern void hx_change_name_icon (struct htlc_conn *htlc, const char *name, u_int16_t icon);
extern void hx_post_news (struct htlc_conn *htlc, const char *news, u_int16_t len);
extern void hx_tnews_list (struct htlc_conn *htlc, const char *path, int is_cat);
extern void hx_tnews_get (struct htlc_conn *htlc, const char *path, u_int32_t tid, const u_int8_t *mimetype, void *item);
extern void hx_tnews_post (struct htlc_conn *htlc, char *path, char *subject,
	       u_int32_t threadid, char *text);
extern void hx_htlc_close (struct htlc_conn *htlc);
extern void hx_connect (struct htlc_conn *htlc, const char *serverstr, u_int16_t port,
			const char *name, u_int16_t icon, const char *login, const char *pass,
			int secure);
extern void hx_connect_finish (struct htlc_conn *htlc);
extern void hx_get_news (struct htlc_conn *htlc);
extern void hx_send_msg (struct htlc_conn *htlc, u_int32_t uid,
			 const char *msg, u_int16_t len, void *p);
extern void hx_get_user_list (struct htlc_conn *htlc, int text);
extern void hx_get_user_info (struct htlc_conn *htlc, u_int32_t uid, int text);
extern void hx_kick_user (struct htlc_conn *htlc, u_int32_t uid, u_int16_t ban);
extern void hx_chat_user (struct htlc_conn *htlc, u_int32_t uid);
extern void hx_chat_part (struct htlc_conn *htlc, struct hx_chat *chat);
extern void hx_chat_join (struct htlc_conn *htlc, u_int32_t cid, u_int8_t *pass, u_int16_t passlen);
extern void hx_chat_invite (struct htlc_conn *htlc, u_int32_t cid, u_int32_t uid);
extern void hx_set_subject (struct htlc_conn *htlc, u_int32_t cid, const char *subject);
extern void hx_list_dir (struct htlc_conn *htlc, const char *path,
			 int reload, int recurs, int text);
extern void hx_mkdir (struct htlc_conn *htlc, const char *path);
extern void hx_get_file_info (struct htlc_conn *htlc, const char *path, int text);
extern void hx_file_delete (struct htlc_conn *htlc, const char *path, int text);
extern void hx_file_move (struct htlc_conn *htlc, const char *frompath, const char *topath);
extern void hx_file_link (struct htlc_conn *htlc, const char *frompath, const char *topath);
extern void hx_tracker_list (struct htlc_conn *htlc, struct hx_chat *chat, const char *addrstr, u_int16_t port);
extern void hx_useredit_open (struct htlc_conn *htlc, const char *login, void (*fn)(void *, const char *, const char *, const char *, const struct hl_access_bits *), void *uesp);
extern void hx_useredit_create (struct htlc_conn *htlc, const char *login, const char *pass, const char *name, const struct hl_access_bits *access);
extern void hx_useredit_delete (struct htlc_conn *htlc, const char *login);

extern int task_inerror (struct htlc_conn *htlc);

#define XFER_GET	1
#define XFER_PUT	2
#define XFER_PREVIEW	4
#define XFER_BANNER	8

extern struct htxf_conn *xfer_new (struct htlc_conn *htlc, const char *path, const char *remotepath, u_int16_t type);
extern void xfer_go (struct htxf_conn *htxf);
extern void xfer_delete (struct htxf_conn *htxf);

extern u_int8_t *news_buf;
extern size_t news_len;

extern void expand_tilde (char *buf, const char *str);
extern int expand_command (char *, int, char *);
extern int expand_path (struct htlc_conn *htlc, struct hx_chat *chat, char *path, int len, char *pathbuf);
extern u_int32_t cmd_arg(int argc, char *str);

extern void chrexpand (char *str, int len);
extern int strexpand (char *str, int len);
extern int strunexpand (char *str, int slen, char *buf, int blen);

#define LONGEST_HUMAN_READABLE	32
extern char *human_size (u_int32_t size, char *buf);

struct variable {
	struct variable *prev;
	void *ptr;
	void (*set_fn)();
	char **namstr;
	char **valstr;
	unsigned int nstrs;
	char nam[1];
};

extern u_int16_t tty_show_user_changes;
extern int tty_show_user_joins;
extern int tty_show_user_parts;
extern int tty_chat_pretty;

extern void set_bool (int *boolp, const char *str);
extern void set_int16 (u_int16_t *intp, const char *str);
extern void set_float (float *floatp, const char *str);
extern void set_string (char **strp, const char *str);
extern struct variable *variable_add (void *ptr, void (*set_fn)(), const char *nam);
extern void variable_set (struct htlc_conn *htlc, struct hx_chat *chat, const char *varnam, const char *value);
extern void hx_savevars (void);

struct task {
	struct task *next, *prev;
	u_int32_t trans;
	u_int32_t pos, len;
	int text;
	char *str;
	void *ptr;
	void (*rcv)();
};

extern struct task *task_with_trans (u_int32_t trans);
extern struct task *task_new (struct htlc_conn *htlc, void (*rcv)(), void *ptr, int text, const char *str);
extern void task_delete (struct task *tsk);
extern void task_delete_all (void);
extern void task_error (struct htlc_conn *htlc);
extern void task_tasks_update (struct htlc_conn *htlc);
extern void task_print_list (struct htlc_conn *htlc, struct hx_chat *chat);

extern struct task *task_list;

struct hldir {
	u_int8_t *dir;
	u_int8_t *file;
	u_int16_t dirlen;
	u_int16_t filelen;
};

extern struct hldir *path_to_hldir (const char *path, int is_file);

extern char *hx_timeformat;

#if !defined(__va_copy)
#define __va_copy(_dst, _src) ((_dst) = (_src))
#endif

extern int g_strip_ansi;

static inline void
strip_ansi (char *buf, int len)
{
	register char *p, *end;

	if (!g_strip_ansi)
		return;
	end = buf + len;
	for (p = buf; p < end; p++)
		if (*p < 31 && *p > 13 && *p != 15 && *p != 22)
			*p = (*p & 127) | 64;
}

struct hx_user {
	struct hx_user *next, *prev;
	u_int32_t uid;
	u_int16_t icon;
	u_int16_t color;
	u_int8_t name[32];
	unsigned int flags;
#if defined(CONFIG_TRANSLATE)
	unsigned int translate_flags;
	char trans_fromto[8];
#endif
};

#define USER_FLAG_IGNORE	1
#define USER_FLAG_TRANSLATE	2
#define USER_IGNORE(_u) ((_u)->flags & USER_FLAG_IGNORE)
#define USER_TRANSLATE(_u) ((_u)->flags & USER_FLAG_TRANSLATE)
#define USER_TOGGLE(_u, _f) ((_u)->flags ^= (_f))

#if defined(CONFIG_TRANSLATE)
#include "translate.h"
#endif

extern struct hx_user *hx_user_new (struct hx_user **utailp);
extern void hx_user_delete (struct hx_user **utailp, struct hx_user *user);
extern struct hx_user *hx_user_with_uid (struct hx_user *ulist, u_int32_t uid);
extern struct hx_user *hx_user_with_name (struct hx_user *ulist, u_int8_t *name);

struct hx_chat {
	struct hx_chat *next, *prev;
	u_int32_t cid;
	u_int32_t nusers;
	struct hx_user __user_list;
	struct hx_user *user_list;
	struct hx_user *user_tail;
	u_int8_t subject[256];
	u_int8_t password[32];
	u_int16_t subjectlen;
	u_int16_t passwordlen;
#if defined(CONFIG_TRANSLATE)
	int translate_flags;
	char translate_fromto[6];
#endif
};

extern struct hx_chat *hx_chat_new (struct htlc_conn *htlc, u_int32_t cid);
extern void hx_chat_delete (struct htlc_conn *htlc, struct hx_chat *chat);
extern struct hx_chat *hx_chat_with_cid (struct htlc_conn *htlc, u_int32_t cid);

struct cached_filelist {
	struct cached_filelist *next, *prev;
	char *path;
	struct hl_filelist_hdr *fh;
	u_int32_t fhlen;
	int completing;
	int listed;
	char **filter_argv;
};

struct output_functions {
	void (*init)(int argc, char **argv);
	void (*loop)(void);
	void (*cleanup)(void);
	void (*status)(void);
	void (*clear)(struct htlc_conn *htlc, struct hx_chat *chat);
	void (*mode_underline)(void);
	void (*mode_clear)(void);
	void (*chat)(struct htlc_conn *htlc, u_int32_t cid, char *chat, u_int16_t len);
	void (*chat_subject)(struct htlc_conn *htlc, u_int32_t cid, const char *subject);
	void (*chat_password)(struct htlc_conn *htlc, u_int32_t cid, const u_int8_t *pass);
	void (*chat_invite)(struct htlc_conn *htlc, u_int32_t cid, u_int32_t uid, const char *name);
	void (*chat_delete)(struct htlc_conn *htlc, struct hx_chat *chat);
	void (*msg)(struct htlc_conn *htlc, u_int32_t uid, const char *name, const char *msgbuf, u_int16_t msglen);
	void (*agreement)(struct htlc_conn *htlc, const char *agreement, u_int16_t len);
	void (*news_file)(struct htlc_conn *htlc, const char *news, u_int16_t len);
	void (*news_post)(struct htlc_conn *htlc, const char *news, u_int16_t len);
	void (*news_dirlist)(struct htlc_conn *htlc, struct news_folder *dir);
	void (*news_catlist)(struct htlc_conn *htlc, struct news_group *cat);
	void (*news_thread)(struct htlc_conn *htlc, struct news_post *post);
	void (*user_info)(struct htlc_conn *htlc, u_int32_t uid, const char *nam, const char *info, u_int16_t len);
	void (*user_create)(struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user,
			    const char *nam, u_int16_t icon, u_int16_t color);
	void (*user_delete)(struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user);
	void (*user_change)(struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user,
			    const char *nam, u_int16_t icon, u_int16_t color);
	void (*user_list)(struct htlc_conn *htlc, struct hx_chat *chat);
	void (*users_clear)(struct htlc_conn *htlc, struct hx_chat *chat);
	void (*file_list)(struct htlc_conn *htlc, struct cached_filelist *cfl);
	void (*file_info)(struct htlc_conn *htlc, const char *icon, const char *type, const char *crea, u_int32_t size, const char *name, const char *created, const char *modified, const char *comment);
	void (*file_preview)(struct htlc_conn *htlc, const char *filename, const char *type);
	void (*file_update)(struct htxf_conn *htxf);
	void (*tracker_server_create)(struct htlc_conn *htlc, const char *addrstr, u_int16_t port, u_int16_t nusers,
				      const char *nam, const char *desc);
	void (*task_update)(struct htlc_conn *htlc, struct task *tsk);
	void (*on_connect)(struct htlc_conn *htlc);
	void (*on_disconnect)(struct htlc_conn *htlc);
};

extern struct output_functions hx_output;

/* there is always a hx_tty_output for the console */
extern struct output_functions hx_tty_output;

extern char *g_encoding;
extern char *g_server_encoding;

#endif /* ndef __hxd_HX_H */
