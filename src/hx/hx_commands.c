#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#if !defined(__WIN32__)
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <fnmatch.h>
#include <ctype.h>
#include "hxd.h"
#include "hx.h"
#include "xmalloc.h"
#include "getopt.h"

#ifdef SOCKS
#include "socks.h"
#endif

#ifdef CONFIG_HOPE
#include "sha.h"
#include "md5.h"
#endif

#if defined(CONFIG_HFS)
#include "hfs.h"
#endif

#if defined(CONFIG_SOUND)
#include "sound.h"
#endif

#if defined(CONFIG_ICONV)
#include "conv.h"
#endif

#if defined(CONFIG_TRANSLATE)
#include "translate.h"
#endif

#if defined(CONFIG_HAL)
extern void cmd_hal (int argc, char **argv, char *str, struct htlc_conn *htlc);
#endif

struct htlc_conn hx_htlc;

extern struct hx_chat *tty_chat_front;

void away_log (const char *fmt, ...);

char hx_hostname[128];

u_int8_t dir_char = '/';

#define DIRCHAR			'/'
#define UNKNOWN_TYPECREA	"TEXTR*ch"

char g_multikick[33] = "\0";

static void
user_print (struct htlc_conn *htlc, struct hx_chat *chat, char *str, unsigned int flags)
{
	struct hx_user *ulist;
	struct hx_user *userp;

	ulist = chat->user_list;
	if (flags & USER_FLAG_IGNORE)
		hx_printf_prefix(htlc, chat, INFOPREFIX, "ignored users:\n");
	else if (flags & USER_FLAG_TRANSLATE)
		hx_printf_prefix(htlc, chat, INFOPREFIX, "translated users:\n");
	else
		hx_printf_prefix(htlc, chat, INFOPREFIX, "(chat 0x%x): %u users:\n", chat->cid, chat->nusers);
	hx_output.mode_underline();
	if (flags & USER_FLAG_TRANSLATE)
		hx_printf(htlc, chat, "   uid | nickname                        |  icon | level | stat | from_to\n");
	else
		hx_printf(htlc, chat, "   uid | nickname                        |  icon | level | stat |\n");
	hx_output.mode_clear();
	for (userp = ulist->next; userp; userp = userp->next) {
		if (str && !strstr(userp->name, str))
			continue;
		if (flags & USER_FLAG_IGNORE && !USER_IGNORE(userp))
			continue;
#if defined(CONFIG_TRANSLATE)
		if (flags & USER_FLAG_TRANSLATE && !USER_TRANSLATE(userp))
			continue;
		if (flags & USER_FLAG_TRANSLATE)
			hx_printf(htlc, chat, " %5u | %s%-31s" DEFAULT " | %5u | %5s | %4s | %s\n",
				  userp->uid,
				  colorstr(userp->color),
				  userp->name, userp->icon,
				  userp->color % 4 > 1 ? "ADMIN" : "USER", userp->color % 2 ? "AWAY" : "\0",
				  userp->trans_fromto);
		else
#endif
			hx_printf(htlc, chat, " %5u | %s%-31s" DEFAULT " | %5u | %5s | %4s |\n",
				  userp->uid,
				  colorstr(userp->color),
				  userp->name, userp->icon,
				  userp->color % 4 > 1 ? "ADMIN" : "USER", userp->color % 2 ? "AWAY" : "\0");
	}
}

struct uesp_fn {
	void *uesp;
	void (*fn)(void *, const char *, const char *, const char *, const struct hl_access_bits *);
};

static void
rcv_task_user_open (struct htlc_conn *htlc, struct uesp_fn *uespfn)
{
	char name[32], login[32], pass[32];
	u_int16_t nlen = 0, llen = 0, plen = 0;
	struct hl_access_bits *access = 0;

	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_NAME:
				nlen = dh_len >= sizeof(name) ? sizeof(name)-1 : dh_len;
				memcpy(name, dh_data, nlen);
				break;
			case HTLS_DATA_LOGIN:
				llen = dh_len >= sizeof(login) ? sizeof(login)-1 : dh_len;
				hl_decode(login, dh_data, llen);
				break;
			case HTLS_DATA_PASSWORD:
				plen = dh_len >= sizeof(pass) ? sizeof(pass)-1 : dh_len;
				if (plen > 1 && dh_data[0])
					hl_decode(pass, dh_data, plen);
				else
					pass[0] = 0;
				break;
			case HTLS_DATA_ACCESS:
				if (dh_len >= 8)
					access = (struct hl_access_bits *)dh_data;
				break;
		}
	dh_end()
	name[nlen] = 0;
	login[llen] = 0;
	pass[plen] = 0;
	if (access)
		uespfn->fn(uespfn->uesp, name, login, pass, access);
	xfree(uespfn);
}

void
hx_useredit_create (struct htlc_conn *htlc, const char *login, const char *pass, const char *name, const struct hl_access_bits *access)
{
	char elogin[32], epass[32];
	u_int16_t llen, plen;

	llen = strlen(login);
	hl_encode(elogin, login, llen);
	if (!*pass) {
		plen = 1;
		epass[0] = 0;
	} else {
		plen = strlen(pass);
		hl_encode(epass, pass, plen);
	}
	task_new(htlc, 0, 0, 0, "user create");
	hlwrite(htlc, HTLC_HDR_ACCOUNT_MODIFY, 0, 4,
		HTLC_DATA_LOGIN, llen, elogin,
		HTLC_DATA_PASSWORD, plen, epass,
		HTLC_DATA_NAME, strlen(name), name,
		HTLC_DATA_ACCESS, 8, access);
}

void
hx_useredit_delete (struct htlc_conn *htlc, const char *login)
{
	char elogin[32];
	u_int16_t llen;

	llen = strlen(login);
	hl_encode(elogin, login, llen);
	task_new(htlc, 0, 0, 0, "user delete");
	hlwrite(htlc, HTLC_HDR_ACCOUNT_DELETE, 0, 1,
		HTLC_DATA_LOGIN, llen, elogin);
}

void
hx_useredit_open (struct htlc_conn *htlc, const char *login, void (*fn)(void *, const char *, const char *, const char *, const struct hl_access_bits *), void *uesp)
{
	struct uesp_fn *uespfn;

	uespfn = xmalloc(sizeof(struct uesp_fn));
	uespfn->uesp = uesp;
	uespfn->fn = fn;
	task_new(htlc, rcv_task_user_open, uespfn, 0, "user open");
	hlwrite(htlc, HTLC_HDR_ACCOUNT_READ, 0, 1,
		HTLC_DATA_LOGIN, strlen(login), login);
}

static void
rcv_task_msg (struct htlc_conn *htlc, char *msg_buf)
{
	if (msg_buf) {
		hx_printf(htlc, 0, "%s\n", msg_buf);
		xfree(msg_buf);
	}
}

#define COMMAND(x) static void			\
cmd_##x (int argc __attribute__((__unused__)),	\
	char **argv __attribute__((__unused__)),\
	char *str __attribute__((__unused__)),	\
	struct htlc_conn *htlc __attribute__((__unused__)),\
	struct hx_chat *chat __attribute__((__unused__)))

COMMAND(tasks)
{
	task_print_list(htlc, chat);
}

extern void hx_banner_get (struct htlc_conn *htlc, const char *lpath);

COMMAND(banner)
{
	hx_banner_get(htlc, argc < 2 ? "banner" : argv[1]);
}

COMMAND(broadcast)
{
	char *msg;
	u_int16_t len;

	if (argc < 1) {
usage:		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <msg>\n", argv[0]);
		return;
	}
	msg = str + (cmd_arg(1, str) >> 16);
	if (!*msg)
		goto usage;
	len = strlen(msg);
	task_new(htlc, 0, 0, 0, "broadcast");
	hlwrite(htlc, HTLC_HDR_MSG_BROADCAST, 0, 1,
		HTLC_DATA_MSG, len, msg);
}

void hx_send_msg (struct htlc_conn *htlc, u_int32_t uid, const char *msg, u_int16_t len, void *data);

static void
err_printf (const char *fmt, ...)
{
}

#if defined(CONFIG_TRANSLATE)
struct fromto {
	char fromto[6];
	char *longname;
};

struct fromto babel_fromto[] = {
	{ "en_zh", "English to Chinese" },
	{ "en_fr", "English to French" },
	{ "en_de", "English to German" },
	{ "en_it", "English to Italian" },
	{ "en_ja", "English to Japanese" },
	{ "en_ko", "English to Korean" },
	{ "en_pt", "English to Portuguese" },
	{ "en_es", "English to Spanish" },
	{ "zh_en", "Chinese to English" },
	{ "fr_en", "French to English" },
	{ "fr_de", "French to German" },
	{ "de_en", "German to English" },
	{ "de_fr", "German to French" },
	{ "it_en", "Italian to English" },
	{ "ja_en", "Japanese to English" },
	{ "ko_en", "Korean to English" },
	{ "pt_en", "Portuguese to English" },
	{ "ru_en", "Russian to English" },
	{ "es_en", "Spanish to English" }
};
#define NFROMTO (int)(sizeof(babel_fromto)/sizeof(struct fromto))

struct option translate_opts[] = {
	{"chat", 1, 0, 'c'},
	{"output", 0, 0, 'o'},
	{0, 0, 0, 0}
};

COMMAND(translate)
{
	u_int32_t uid, cid = 0;
	char *name, *fromto;
	struct hx_user *user = 0;
	struct hx_chat *hchat;
	struct opt_r opt;
	int i, o, longind;
	unsigned int output_to_chat = 0;

	opt.err_printf = err_printf;
	opt.ind = 0;
	while ((o = getopt_long_r(argc, argv, "c:o", translate_opts, &longind, &opt)) != EOF) {
		if (o == 0)
			o = translate_opts[longind].val;
		switch (o) {
			case 'c':
				cid = atou32(opt.arg);
				break;
			case 'o':
				output_to_chat = !output_to_chat;
				break;
			default:
				break;
		}
	}
	hchat = hx_chat_with_cid(htlc, cid);
	if (!hchat) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: null chat, not connected?\n", argv[0]);
		return;
	}
	if (!argv[opt.ind]) {
		user_print(htlc, hchat, 0, USER_FLAG_TRANSLATE);
		return;
	}
	if (argv[opt.ind] && !argv[opt.ind+1]) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage %s [-c cid] [-o] <from_to> [users]\n", argv[0]);
		goto help;
	}
	fromto = argv[opt.ind];
	for (i = 0; i < NFROMTO; i++) {
		if (!strcmp(fromto, babel_fromto[i].fromto))
			goto ok;
	}
help:
	hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: fromto must be one of:\n", argv[0]);
	for (i = 0; i < NFROMTO; i++)
		hx_printf(htlc, chat, "%s  %s\n", babel_fromto[i].fromto, babel_fromto[i].longname);
	return;
ok:
	for (i = opt.ind+1; argv[i]; i++) {
		name = argv[i];
		if (*name == '*' && *(name+1)==0) {
			for (user = hchat->user_list->next; user; user = user->next) {
				USER_TOGGLE(user, USER_FLAG_TRANSLATE);
				if (output_to_chat)
					TRANS_TOGGLE(user, TRANS_FLAG_OUTPUT);
				strcpy(user->trans_fromto, fromto);
			}
			hchat->translate_flags ^= 1;
			hchat->translate_flags ^= (output_to_chat<<1);
			strcpy(hchat->translate_fromto, fromto);
			hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: all users are now %s\n",
					 argv[0], USER_TRANSLATE(hchat->user_list->next) ? "translated" : "untranslated");
			break;
		}
		uid = atou32(name);
		if (!uid) {
			user = hx_user_with_name(hchat->user_list, name);
			if (!user) {
				hx_printf_prefix(htlc, chat, INFOPREFIX,
						 "%s: no such nickname %s\n", argv[0], name);
				continue;
			}
		} else {
			user = hx_user_with_uid(hchat->user_list, uid);
			if (!user) {
				hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: no such uid %d\n", argv[0], uid);
				continue;
			}
		}
		USER_TOGGLE(user, USER_FLAG_TRANSLATE);
		if (output_to_chat)
			TRANS_TOGGLE(user, TRANS_FLAG_OUTPUT);
		strcpy(user->trans_fromto, fromto);
		hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: user %s (%u) is now %s\n",
				 argv[0], user->name, user->uid, USER_TRANSLATE(user) ? "translated" : "untranslated");
	}
}
#endif

struct option ignore_opts[] = {
	{"chat", 1, 0, 'c'},
	{0, 0, 0, 0}
};

COMMAND(ignore)
{
	u_int32_t uid, cid = 0;
	char *name;
	struct hx_user *user = 0;
	struct hx_chat *hchat;
	struct opt_r opt;
	int i, o, longind;

	opt.err_printf = err_printf;
	opt.ind = 0;
	while ((o = getopt_long_r(argc, argv, "c:", ignore_opts, &longind, &opt)) != EOF) {
		if (o == 0)
			o = ignore_opts[longind].val;
		switch (o) {
			case 'c':
				cid = atou32(opt.arg);
				break;
			default:
				break;
		}
	}
	hchat = hx_chat_with_cid(htlc, cid);
	if (!hchat) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: null chat, not connected?\n", argv[0]);
		return;
	}
	if (!argv[opt.ind]) {
		user_print(htlc, hchat, 0, USER_FLAG_IGNORE);
		return;
	}
	for (i = opt.ind; argv[i]; i++) {
		name = argv[i];
		if (*name == '*' && *(name+1)==0) {
			for (user = hchat->user_list->next; user; user = user->next) {
				USER_TOGGLE(user, USER_FLAG_IGNORE);
			}
			hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: all users are now %s\n",
					 argv[0], USER_TRANSLATE(hchat->user_list->next) ? "ignored" : "unignored");
			break;
		}
		uid = atou32(name);
		if (!uid) {
			user = hx_user_with_name(hchat->user_list, name);
			if (!user) {
				hx_printf_prefix(htlc, chat, INFOPREFIX,
						 "%s: no such nickname %s\n", argv[0], name);
				continue;
			}
		} else {
			user = hx_user_with_uid(hchat->user_list, uid);
			if (!user) {
				hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: no such uid %d\n", argv[0], uid);
				continue;
			}
		}
		USER_TOGGLE(user, USER_FLAG_IGNORE);
		hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: user %s (%u) is now %s\n",
				 argv[0], user->name, user->uid, USER_TRANSLATE(user) ? "ignored" : "unignored");
	}
}

COMMAND(mkick)
{
	if( argc < 3 )
		 hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <nick> <uid>\n", argv[0]);
	else
		 if( atoi(argv[2]) < USHRT_MAX )
			 hx_set_multikick( htlc, argv[1], atoi(argv[2]));
}

COMMAND(unmkick)
{
	hx_unset_multikick();
}

COMMAND(msg)
{
	u_int32_t uid;
	char *name, *msg, *buf;
	struct hx_user *user = 0;
	size_t buflen;

	name = argv[1];
	if (!name)
		goto usage;
	msg = str + (cmd_arg(2, str) >> 16);
	if (!*msg) {
usage:		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage %s <uid> <msg>\n", argv[0]);
		return;
	}
	uid = atou32(name);
	if (!uid) {
		struct hx_chat *chat = hx_chat_with_cid(htlc, 0);

		if (!chat) {
			hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: null chat, not connected?\n", argv[0]);
			return;
		}
		user = hx_user_with_name(chat->user_list, name);
		if (!user) {
			hx_printf_prefix(htlc, chat, INFOPREFIX,
					 "%s: no such nickname %s\n", argv[0], name);
			return;
		}
		uid = user->uid;
	}
	strlcpy(last_msg_nick, name, 32);
	if (!user) {
		struct hx_chat *chat = hx_chat_with_cid(htlc, 0);

		if (!chat) {
			hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: null chat, not connected?\n", argv[0]);
			return;
		}
		user = hx_user_with_uid(chat->user_list, uid);
	}
	if (user) {
		char *col = colorstr(user->color);

		buflen = strlen(msg) + strlen(user->name) + 64 + strlen(col) + strlen(DEFAULT);
		buf = xmalloc(buflen);
		snprintf(buf, buflen, "%s[%s%s%s(%s%u%s)]%s-> %s",
			 WHITE, col, user->name, WHITE, ORANGE,
			 uid, WHITE, DEFAULT, msg);
	} else {
		buflen = strlen(msg) + 64;
		buf = xmalloc(buflen);
		snprintf(buf, buflen, "%s[(%s%u%s)]%s-> %s", WHITE, ORANGE, uid, WHITE, DEFAULT, msg);
	}

	hx_send_msg(htlc, uid, msg, strlen(msg), buf);
}

void
hx_change_name_icon (struct htlc_conn *htlc, const char *name, u_int16_t icon)
{
	u_int16_t icon16, len;

	if (name) {
		len = strlen(name);
		if (len > 31)
			len = 31;
		memcpy(htlc->name, name, len);
		htlc->name[len] = 0;
	} else {
		len = strlen(htlc->name);
	}
	if (icon)
		htlc->icon = icon;
	icon16 = htons(htlc->icon);
	hlwrite(htlc, HTLC_HDR_USER_CHANGE, 0, 2,
		HTLC_DATA_ICON, 2, &icon16,
		HTLC_DATA_NAME, len, htlc->name);
	hx_output.status();
}

struct option nick_opts[] = {
	{"icon", 1, 0, 'i'},
	{0, 0, 0, 0}
};

COMMAND(nick)
{
	u_int16_t icon = 0;
	char *name;
	struct opt_r opt;
	int o, longind;

	opt.err_printf = err_printf;
	opt.ind = 0;
	while ((o = getopt_long_r(argc, argv, "i:", nick_opts, &longind, &opt)) != EOF) {
		if (o == 0)
			o = nick_opts[longind].val;
		switch (o) {
			case 'i':
				icon = atou32(opt.arg);
				break;
			default:
				break;
		}
	}
	name = argv[opt.ind];
	if (!name) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <nickname> [-i icon]\n", argv[0]);
		return;
	}
	hx_change_name_icon(htlc, name, icon);
}

COMMAND(icon)
{
	u_int16_t icon;

	if (!argv[1]) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <icon>\n", argv[0]);
		return;
	}
	icon = atou16(argv[1]);
	hx_change_name_icon(htlc, 0, icon);
}

COMMAND(quit)
{
	hx_exit(0);
}

static void rcv_task_user_list (struct htlc_conn *htlc, struct hx_chat *chat, int text);

void
hx_get_user_list (struct htlc_conn *htlc, int text)
{
	struct hx_chat *chat;

	if (!htlc->fd)
		return;
	chat = hx_chat_with_cid(htlc, 0);
	if (!chat)
		chat = hx_chat_new(htlc, 0);
	task_new(htlc, rcv_task_user_list, chat, text, "user_list");
	hlwrite(htlc, HTLC_HDR_USER_GETLIST, 0, 0);
}

static struct option server_opts[] = {
	{"login",	1, 0,	'l'},
	{"password",	1, 0,	'p'},
	{"nickname",	1, 0,	'n'},
	{"icon",	1, 0,	'i'},
	{"cipher",	1, 0,	'c'},
	{"old", 	0, 0,	'o'},
	{"secure",	0, 0,	's'},
	{"force",	0, 0,	'f'},
	{"zip",		1, 0,	'z'},
	{0, 0, 0, 0}
};

extern int16_t g_default_secure;

COMMAND(server)
{
	u_int16_t port = 0, icon = 0;
	char *serverstr = 0, *portstr = 0, *login = 0, *pass = 0, *name = 0;
	char *cipher = 0, *compress = 0;
	struct opt_r opt;
	int o, longind;
	int secure = (int)g_default_secure;

	opt.err_printf = err_printf;
	opt.ind = 0;
	while ((o = getopt_long_r(argc, argv, "l:p:n:i:c:z:osf", server_opts, &longind, &opt)) != EOF) {
		if (o == 0)
			o = server_opts[longind].val;
		switch (o) {
			case 'l':
				login = opt.arg;
				break;
			case 'p':
				pass = opt.arg;
				break;
			case 'n':
				name = opt.arg;
				break;
			case 'i':
				icon = atou16(opt.arg);
				break;
			case 'c':
				cipher = opt.arg;
				break;
			case 'o':
				secure = 0;
				break;
			case 's':
				secure = 1;
				break;
			case 'f':
				secure = 2;
				break;
			case 'z':
				compress = opt.arg;
				break;
			default:
				goto usage;
		}
	}

#ifdef CONFIG_CIPHER
	if (cipher)
		strlcpy(htlc->cipheralg, cipher, sizeof(htlc->cipheralg));
#endif
#ifdef CONFIG_COMPRESS
	if (compress)
		strlcpy(htlc->compressalg, compress, sizeof(htlc->compressalg));
#endif

	if (opt.ind < argc) {
		serverstr = argv[opt.ind];
		if (opt.ind + 1 < argc)
			portstr = argv[opt.ind + 1];
		else {
#ifndef CONFIG_IPV6 /* since IPv6 addresses use ':', we surely can't do this! */
			char *p = strchr(serverstr, ':');
#else
			char *p = strchr(serverstr, '/');
#endif
			if (p) {
				*p = 0;
				portstr = p + 1;
			}
		}
	}
	if (!serverstr) {
usage:
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s [OPTIONS] <server address>[:][port]\n"
				"  -l, --login <login>\n"
				"  -p, --password <password>\n"
				"  -n, --nickname <nickname>\n"
				"  -i, --icon <icon>\n"
				"  -c, --cipher {RC4, BLOWFISH, IDEA, NONE}\n"
				"  -z, --zip {GZIP, NONE}\n"
				"  -o, --old  [not secure]\n"
				"  -f, --force [not secure]\n"
				"  -s, --secure\n"
				, argv[0]);
		return;
	}

	if (portstr)
		port = atou16(portstr);
	if (!port)
		port = HTLS_TCPPORT;

	hx_connect(htlc, serverstr, port, name, icon, login, pass, secure);
}

COMMAND(news)
{
	if ((argv[1] && ((argv[1][0] == '-' && argv[1][1] == 'r') || !strcmp(argv[1], "--reload")))
	    || !htlc->news_len) {
		hx_get_news(htlc);
	} else {
		hx_output.news_file(htlc, htlc->news_buf, htlc->news_len);
	}
}

COMMAND(nls)
{
	int i;

	if (argc < 2)
		hx_tnews_list(htlc, "/", 0);
	for (i = 1; i < argc; i++) {
		hx_tnews_list(htlc, argv[i], 0);
	}
}

COMMAND(nlsc)
{
	int i;

	if (argc < 2)
		hx_tnews_list(htlc, "/", 0);
	for (i = 1; i < argc; i++) {
		hx_tnews_list(htlc, argv[i], 1);
	}
}

COMMAND(nget)
{
	int i;

	if (argc < 3) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <newsfile> <tid>\n", argv[0]);
		return;
	}
	for (i = 1; i+1 < argc; i += 2) {
		struct news_item *item = xmalloc(sizeof(struct news_item));
		item->postid = atou32(argv[i+1]);
		hx_tnews_get(htlc, argv[i], item->postid, "text/plain", item);
	}
}

static void
rcv_task_user_list (struct htlc_conn *htlc, struct hx_chat *chat, int text)
{
	struct hl_userlist_hdr *uh;
	struct hx_user *user;
	u_int16_t uid, nlen, slen = 0;

	dh_start(htlc)
		if (dh_type == HTLS_DATA_USER_LIST) {
			uh = (struct hl_userlist_hdr *)dh;
			L16NTOH(uid, &uh->uid);
			user = hx_user_with_uid(chat->user_list, uid);
			if (!user) {
				user = hx_user_new(&chat->user_tail);
				chat->nusers++;
			}
			user->uid = uid;
			L16NTOH(user->icon, &uh->icon);
			L16NTOH(user->color, &uh->color);
			L16NTOH(nlen, &uh->nlen);
			if (nlen > 31)
				nlen = 31;
			memcpy(user->name, uh->name, nlen);
			strip_ansi(user->name, nlen);
			user->name[nlen] = 0;
			if (!htlc->uid && !strcmp(user->name, htlc->name) && user->icon == htlc->icon) {
				htlc->uid = user->uid;
				htlc->color = user->color;
				hx_output.status();
			}
		} else if (dh_type == HTLS_DATA_CHAT_SUBJECT) {
			slen = dh_len > 255 ? 255 : dh_len;
			memcpy(chat->subject, dh_data, slen);
			chat->subject[slen] = 0;
		}
	dh_end()
	hx_output.user_list(htlc, chat);
	if (slen)
		hx_output.chat_subject(htlc, chat->cid, chat->subject);
	if (text)
		user_print(htlc, chat, 0, 0);
}

static void
rcv_task_user_list_switch (struct htlc_conn *htlc, struct hx_chat *chat)
{
	if (task_inerror(htlc)) {
		hx_chat_delete(htlc, chat);
		return;
	}
	rcv_task_user_list(htlc, chat, 1);
}

static struct option who_opts[] = {
	{"chat",	1, 0,	'c'},
	{"reload",	0, 0,	'r'},
	{0, 0, 0, 0}
};

COMMAND(who)
{
	u_int32_t cid = 0;
	int reload = 0;
	struct opt_r opt;
	int o, longind;

	if (chat)
		cid = chat->cid;
	opt.err_printf = err_printf;
	opt.ind = 0;
	while ((o = getopt_long_r(argc, argv, "c:r", who_opts, &longind, &opt)) != EOF) {
		if (o == 0)
			o = who_opts[longind].val;
		switch (o) {
			case 'c':
				cid = atou32(opt.arg);
				break;
			case 'r':
				reload = 1;
				break;
		}
	}
	if (htlc->flags.in_login) {
		reload = 1;
		htlc->flags.in_login = 0;
	}
	if (!htlc->chat_list)
		return;
	if (!cid && !reload)
		reload = htlc->chat_list->user_tail == htlc->chat_list->user_list;
	if (reload) {
		chat = hx_chat_with_cid(htlc, 0);
		if (!chat)
			chat = hx_chat_new(htlc, 0);
		task_new(htlc, rcv_task_user_list, chat, 1, "who");
		hlwrite(htlc, HTLC_HDR_USER_GETLIST, 0, 0);
	} else {
		chat = hx_chat_with_cid(htlc, cid);
		if (!chat)
			hx_printf_prefix(htlc, chat, INFOPREFIX, "no such chat 0xx\n", cid);
		else
			user_print(htlc, chat, argv[opt.ind], 0);
	}
}

COMMAND(me)
{
	char *p;

	for (p = str; *p && *p != ' '; p++)
		;
	if (!*p || !(*++p))
		return;
	if (chat)
		hx_send_chat(htlc, chat->cid, p, strlen(p), 1);
	else
		hx_send_chat(htlc, 0, p, strlen(p), 1);
}

COMMAND(chats)
{
	struct hx_chat *chatp;

	hx_output.mode_underline();
	hx_printf(htlc, chat, "        cid | nusers | password | subject\n");
	hx_output.mode_clear();
	for (chatp = htlc->chat_list; chatp; chatp = chatp->prev)
		if (!chatp->prev)
			break;
	for (; chatp; chatp = chatp->next)
		hx_printf(htlc, chat, " 0x%08x | %6u | %8s | %s\n",
			  chatp->cid, chatp->nusers, chatp->password, chatp->subject);
}

extern void hx_rcv_user_change (struct htlc_conn *htlc);

void
hx_chat_user (struct htlc_conn *htlc, u_int32_t uid)
{
	uid = htonl(uid);
	task_new(htlc, hx_rcv_user_change, 0, 1, "chat");
	hlwrite(htlc, HTLC_HDR_CHAT_CREATE, 0, 1,
		HTLC_DATA_UID, 4, &uid);
}

COMMAND(chat)
{
	u_int32_t uid;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <uid>\n", argv[0]);
		return;
	}
	uid = atou32(argv[1]);
	if (!uid) {
		struct hx_chat *chat = hx_chat_with_cid(htlc, 0);
		struct hx_user *user;

		if (!chat) {
			hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: null chat, not connected?\n", argv[0]);
			return;
		}
		user = hx_user_with_name(chat->user_list, argv[1]);
		if (!user) {
			hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: no such nickname %s\n", argv[0], argv[1]);
			return;
		}
		uid = user->uid;
	}
	hx_chat_user(htlc, uid);
}

void
hx_chat_invite (struct htlc_conn *htlc, u_int32_t cid, u_int32_t uid)
{
	cid = htonl(cid);
	uid = htonl(uid);
	task_new(htlc, 0, 0, 1, "invite");
	hlwrite(htlc, HTLC_HDR_CHAT_INVITE, 0, 2,
		HTLC_DATA_CHAT_ID, 4, &cid,
		HTLC_DATA_UID, 4, &uid);
}

COMMAND(invite)
{
	u_int32_t uid, cid;

	if (argc < 3) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <chat> <uid>\n", argv[0]);
		return;
	}
	cid = atou32(argv[1]);
	uid = atou32(argv[2]);
	if (!uid) {
		struct hx_chat *chat = hx_chat_with_cid(htlc, 0);
		struct hx_user *user;

		if (!chat) {
			hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: null chat, not connected?\n", argv[0]);
			return;
		}
		user = hx_user_with_name(chat->user_list, argv[2]);
		if (!user) {
			hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: no such nickname\n", argv[2]);
			return;
		}
		uid = user->uid;
	}
	hx_chat_invite(htlc, cid, uid);
}

void
hx_chat_join (struct htlc_conn *htlc, u_int32_t cid, u_int8_t *pass, u_int16_t passlen)
{
	struct hx_chat *chat;

	chat = hx_chat_with_cid(htlc, cid);
	if (!chat) {
		chat = hx_chat_new(htlc, cid);
		cid = htonl(cid);
		task_new(htlc, rcv_task_user_list_switch, chat, 1, "join");
		if (passlen)
			hlwrite(htlc, HTLC_HDR_CHAT_JOIN, 0, 2,
				HTLC_DATA_CHAT_ID, 4, &cid,
				HTLC_DATA_PASSWORD, passlen, pass);
		else
			hlwrite(htlc, HTLC_HDR_CHAT_JOIN, 0, 1,
				HTLC_DATA_CHAT_ID, 4, &cid);
	}
	tty_chat_front = chat;
}

COMMAND(join)
{
	u_int32_t cid;
	u_int8_t *pass;
	u_int16_t passlen;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <chat> [pass]\n", argv[0]);
		return;
	}
	cid = atou32(argv[1]);
	pass = argv[2];
	if (pass)
		passlen = strlen(pass);
	else
		passlen = 0;
	hx_chat_join(htlc, cid, pass, passlen);
}

void
hx_chat_part (struct htlc_conn *htlc, struct hx_chat *chat)
{
	u_int32_t cid;

	cid = htonl(chat->cid);
	hlwrite(htlc, HTLC_HDR_CHAT_PART, 0, 1,
		HTLC_DATA_CHAT_ID, 4, &cid);
	hx_chat_delete(htlc, chat);
}

COMMAND(part)
{
	u_int32_t cid;

	if (argv[1]) {
		cid = atou32(argv[1]);
		chat = hx_chat_with_cid(htlc, cid);
	}
	if (!chat) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: no such chat %s\n", argv[0], argv[1] ? argv[1] : "(front)");
		return;
	}
	if (chat->cid == 0) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "not can part from server chat\n");
		return;
	}
	hx_chat_part(htlc, chat);
}

void
hx_set_subject (struct htlc_conn *htlc, u_int32_t cid, const char *subject)
{
	cid = htonl(cid);
	hlwrite(htlc, HTLC_HDR_CHAT_SUBJECT, 0, 2,
		HTLC_DATA_CHAT_ID, 4, &cid,
		HTLC_DATA_CHAT_SUBJECT, strlen(subject), subject);
}

void
hx_set_password (struct htlc_conn *htlc, u_int32_t cid, const char *pass)
{
	cid = htonl(cid);
	hlwrite(htlc, HTLC_HDR_CHAT_SUBJECT, 0, 2,
		HTLC_DATA_CHAT_ID, 4, &cid,
		HTLC_DATA_PASSWORD, strlen(pass), pass);
}

COMMAND(subject)
{
	u_int32_t cid;
	u_int16_t len;
	char *s;

	if (argc < 2) {
usage:		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <subject>\n", argv[0]);
		return;
	}
	if (chat)
		cid = chat->cid;
	else
		cid = 0;
	s = str + (cmd_arg(1, str) >> 16);
	if (!*s)
		goto usage;
	len = strlen(s);
	hx_set_subject(htlc, cid, s);
}

COMMAND(password)
{
	u_int32_t cid;
	u_int16_t len;
	char *s;

	if (argc < 2) {
usage:		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <pass>\n", argv[0]);
		return;
	}
	if (chat) {
		cid = chat->cid;
		s = str + (cmd_arg(1, str) >> 16);
		if (!*s)
			goto usage;
		len = strlen(s);
		hx_set_password(htlc, cid, s);
	}
}

static void
rcv_task_kick (struct htlc_conn *htlc)
{
	if (task_inerror(htlc))
		return;
	hx_printf_prefix(htlc, 0, INFOPREFIX, "kick successful\n");
}

void
hx_kick_user (struct htlc_conn *htlc, u_int32_t uid, u_int16_t ban)
{
	uid = htonl(uid);
	task_new(htlc, rcv_task_kick, 0, 1, "kick");
	if (ban) {
		ban = htons(ban);
		hlwrite(htlc, HTLC_HDR_USER_KICK, 0, 2,
			HTLC_DATA_BAN, 2, &ban,
			HTLC_DATA_UID, 4, &uid);
	} else {
		hlwrite(htlc, HTLC_HDR_USER_KICK, 0, 1,
			HTLC_DATA_UID, 4, &uid);
	}
}

struct option kick_opts[] = {
	{"ban", 0, 0, 'b'},
	{0, 0, 0, 0}
};

COMMAND(kick)
{
	u_int32_t uid;
	struct opt_r opt;
	int i, ban = 0, o, longind;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s [-b, --ban] <uid>\n", argv[0]);
		return;
	}
	opt.err_printf = err_printf;
	opt.ind = 0;
	while ((o = getopt_long_r(argc, argv, "b", kick_opts, &longind, &opt)) != EOF) {
		if (o == 0)
			o = kick_opts[longind].val;
		switch (o) {
			case 'b':
				ban = 1;
				break;
			default:
				break;
		}
	}
	for (i = opt.ind; i < argc; i++) {
		uid = atou32(argv[i]);
		if (!uid) {
			struct hx_chat *chat = hx_chat_with_cid(htlc, 0);
			struct hx_user *user;

			if (!chat) {
				hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: null chat, not connected?\n", argv[0]);
				return;
			}
			user = hx_user_with_name(chat->user_list, argv[i]);
			if (!user) {
				hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: no such nickname %s\n", argv[0], argv[i]);
				return;
			}
			uid = user->uid;
		}
		hx_kick_user(htlc, uid, ban);
	}
}

COMMAND(post)
{
	char *p;

	for (p = str; *p && *p != ' '; p++) ;
	if (!*p || !(*++p))
		return;
	hx_post_news(htlc, p, strlen(p));
}

COMMAND(close)
{
	if (htlc->fd)
		hx_htlc_close(htlc);
}

static void
rcv_task_user_info (struct htlc_conn *htlc, u_int32_t uid, int text)
{
	u_int16_t ilen = 0, nlen = 0;
	u_int8_t info[4096 + 1], name[32];

	name[0] = 0;
	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_USER_INFO:
				ilen = dh_len > 4096 ? 4096 : dh_len;
				memcpy(info, dh_data, ilen);
				info[ilen] = 0;
				break;
			case HTLS_DATA_NAME:
				nlen = dh_len > 31 ? 31 : dh_len;
				memcpy(name, dh_data, nlen);
				name[nlen] = 0;
				strip_ansi(name, nlen);
				break;
		}
	dh_end()
	if (ilen) {
		CR2LF(info, ilen);
		strip_ansi(info, ilen);
		if (text)
			hx_tty_output.user_info(htlc, uid, name, info, ilen);
		else
			hx_output.user_info(htlc, uid, name, info, ilen);
	}
}

void
hx_get_user_info (struct htlc_conn *htlc, u_int32_t uid, int text)
{
	task_new(htlc, rcv_task_user_info, (void *)uid, text, "info");
	uid = htonl(uid);
	hlwrite(htlc, HTLC_HDR_USER_GETINFO, 0, 1,
		HTLC_DATA_UID, 4, &uid);
}

COMMAND(info)
{
	u_int32_t uid;
	int i;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <uid>\n", argv[0]);
		return;
	}
	for (i = 1; i < argc; i++) { 
		uid = atou32(argv[i]);
		if (!uid) {
			struct hx_chat *chat = hx_chat_with_cid(htlc, 0);
			struct hx_user *user;

			if (!chat) {
				hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: null chat, not connected?\n", argv[0]);
				return;
			}
			user = hx_user_with_name(chat->user_list, argv[i]);
			if (!user) {
				hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: no such nickname %s\n", argv[0], argv[i]);
				return;
			}
			uid = user->uid;
		}
		hx_get_user_info(htlc, uid, 1);
	}
}

void
hx_load (struct htlc_conn *htlc, struct hx_chat *chat, const char *path)
{
	char buf[4096];
	int fd;
	size_t j;
	int comment;

	if ((fd = SYS_open(path, O_RDONLY, 0)) < 0) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "load: %s: %s\n", path, strerror(errno));
		return;
	}
	j = 0;
	comment = 0;
	while (j < sizeof(buf) && read(fd, &buf[j], 1) > 0) {
		if (j == 0 && buf[j] == '#') {
			comment = 1;
		} else if (buf[j] == '\n') {
			if (comment) {
				comment = 0;
			} else if (j) {
				buf[j] = 0;
				hx_command(htlc, chat, buf);
			}
			j = 0;
		} else
			j++;
	}
	close(fd);
}

COMMAND(load)
{
	char path[MAXPATHLEN];
	int i;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <file1> [file2...]\n", argv[0]);
		return;
	}
	for (i = 1; i < argc; i++) {
		expand_tilde(path, argv[i]);
		hx_load(htlc, chat, path);
	}
}

static char *
read_file (int fd, size_t max, size_t *lenp)
{
#define BLOCKSIZE	4096
	char *buf = 0;
	size_t len = 0;
	size_t pos = 0;
	size_t rn;
	ssize_t r;

	for (;;) {
		if (pos+BLOCKSIZE > len) {
			len += BLOCKSIZE;
			buf = xrealloc(buf, len);
		}
		rn = max > BLOCKSIZE ? BLOCKSIZE : max;
		r = read(fd, buf+pos, rn);
		if (r <= 0)
			break;
		pos += r;
		max -= r;
		if (r != (ssize_t)rn || !max)
			break;
	}
	if (*lenp)
		*lenp = pos;

	return buf;
}

struct option type_opts[] = {
	{"chat", 0, 0, 'c'},
	{"news", 0, 0, 'n'},
	{"user", 0, 0, 'u'},
	{0, 0, 0, 0}
};

COMMAND(type)
{
#define MAXCHATSIZ	2048
	char path[MAXPATHLEN], *buf;
	struct opt_r opt;
	int i, fd, agin, o, longind;
	size_t buflen, max;
	u_int32_t cid = 0, uid = 0, news = 0;
	struct hx_user *user = 0;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s [-n, --news] [-c cid] [-u uid] <file1> [file2...]\n", argv[0]);
		return;
	}
	if (chat)
		cid = htonl(chat->cid);
	else
		chat = 0;
	opt.err_printf = err_printf;
	opt.ind = 0;
	while ((o = getopt_long_r(argc, argv, "c:u:n", type_opts, &longind, &opt)) != EOF) {
		if (o == 0)
			o = type_opts[longind].val;
		switch (o) {
			case 'c':
				cid = htonl(atou32(opt.arg));
				break;
			case 'n':
				news = 1;
				break;
			case 'u':
				uid = atou32(opt.arg);
				if (!uid) {
					struct hx_chat *chat = hx_chat_with_cid(htlc, 0);

					if (!chat) {
						hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: null chat, not connected?\n", argv[0]);
						return;
					}
					user = hx_user_with_name(chat->user_list, opt.arg);
					if (!user) {
						hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: no such nickname %s\n", argv[0], opt.arg);
						return;
					}
					uid = user->uid;
				}
				break;
			default:
				break;
		}
	}
	if (news || uid)
		max = 0xffff;
	else
		max = MAXCHATSIZ;
	for (i = opt.ind; i < argc; i++) {
		expand_tilde(path, argv[i]);
		if ((fd = SYS_open(path, O_RDONLY, 0)) < 0) {
			hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: %s: %s\n", argv[0], argv[i], strerror(errno));
			continue;
		}
agin_l:
		buf = read_file(fd, max, &buflen);
		LF2CR(buf, buflen);
		if ((news || uid) || buflen != MAXCHATSIZ)
			agin = 0;
		else {
			char *p;

			buf[buflen] = 0;
			p = strrchr(buf, '\r');
			if (p) {
				buflen = p - buf;
				lseek(fd, -(MAXCHATSIZ - buflen - 1), SEEK_CUR);
				agin = 1;
			} else {
				agin = 0;
			}
		}
		if (buf[buflen - 1] == '\r')
			buflen--;
		if (news) {
			task_new(htlc, 0, 0, 0, "type");
			hlwrite(htlc, HTLC_HDR_NEWSFILE_POST, 0, 1,
				HTLC_DATA_NEWS_POST, buflen, buf);
		} else if (uid) {
			size_t msg_buflen;
			char *msg_buf;

			if (!user) {
				struct hx_chat *chat = hx_chat_with_cid(htlc, 0);

				if (!chat) {
					hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: null chat, not connected?\n", argv[0]);
					return;
				}
				user = hx_user_with_uid(chat->user_list, uid);
			}
			if (user) {
				char *col = colorstr(user->color);
				msg_buflen = strlen(path) + strlen(user->name) + 32 + strlen(col) + strlen(DEFAULT);
				msg_buf = xmalloc(msg_buflen);
				snprintf(msg_buf, msg_buflen, "[%s%s%s(%u)]->file> %s", col, user->name, DEFAULT, uid, path);
			} else {
				msg_buflen = strlen(path) + 32;
				msg_buf = xmalloc(msg_buflen);
				snprintf(msg_buf, msg_buflen, "[(%u)]->file> %s", uid, path);
			}
			task_new(htlc, rcv_task_msg, msg_buf, 1, "type");
			uid = htonl(uid);
			hlwrite(htlc, HTLC_HDR_MSG, 0, 2,
				HTLC_DATA_UID, 4, &uid,
				HTLC_DATA_MSG, buflen, buf);
		} else if (cid)
			hlwrite(htlc, HTLC_HDR_CHAT, 0, 2,
				HTLC_DATA_CHAT, buflen, buf,
				HTLC_DATA_CHAT_ID, 4, &cid);
		else
			hlwrite(htlc, HTLC_HDR_CHAT, 0, 1,
				HTLC_DATA_CHAT, buflen, buf);
		xfree(buf);
		if (agin)
			goto agin_l;
		close(fd);
	}
}

extern void hx_dirchar_change (struct htlc_conn *htlc, u_int8_t dc);

COMMAND(dirchar)
{
	if (!argv[1]) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "%c\n", dir_char);
		return;
	}
	hx_dirchar_change(htlc, argv[1][0]);
}

extern char **glob_remote (char *path, int *npaths, u_int32_t type);

static struct option ls_opts[] = {
	{"name",	0, 0,	'n'},
	{"reload",	0, 0,	'r'},
	{"recursive",	0, 0,	'R'},
	{0, 0, 0, 0}
};

void
hx_file_list (struct htlc_conn *htlc, const char *argpath, int reload, int recurs)
{
	char *path, **paths, buf[4096];
	int i, npaths;

	if (*argpath == dir_char)
		strcpy(buf, argpath);
	else
		snprintf(buf, sizeof(buf), "%s%c%s", htlc->rootdir, dir_char, argpath);
	path = buf;
	for (i = strlen(path)-1; i > 1 && path[i] == dir_char; i--)
		path[i] = 0;
	paths = glob_remote(path, &npaths, 0);
	for (i = 0; i < npaths; i++) {
		path = paths[i];
		hx_list_dir(htlc, path, reload, recurs, 1);
		xfree(path);
	}
	xfree(paths);
}

COMMAND(ls)
{
	int o, longind, i, reload = 0, recurs = 0, byname = 0;
	struct opt_r opt;

	opt.err_printf = err_printf;
	opt.ind = 0;
	while ((o = getopt_long_r(argc, argv, "rR", ls_opts, &longind, &opt)) != EOF) {
		if (o == 0)
			o = server_opts[longind].val;
		switch (o) {
			case 'R':
				recurs = 1;
				break;
			case 'n':
				byname = 1;
				break;
			case 'r':
				reload = 1;
				break;
		}
	}
	if (opt.ind == argc)
		hx_file_list(htlc, htlc->rootdir, reload, recurs);
	for (i = opt.ind; i < argc; i++)
		hx_file_list(htlc, argv[i], reload, recurs);
}

COMMAND(cd)
{
	int len;

	if (argc < 2) {
		hx_printf(htlc, chat, "usage: %s <directory>\n", argv[0]);
		return;
	}
	if (argv[1][0] == dir_char) {
		len = strlen(argv[1]) > MAXPATHLEN - 1 ? MAXPATHLEN - 1 : strlen(argv[1]);
		memcpy(htlc->rootdir, argv[1], len);
		htlc->rootdir[len] = 0;
	} else {
		char buf[MAXPATHLEN];

		len = snprintf(buf, MAXPATHLEN, "%s%c%s", htlc->rootdir, dir_char, argv[1]);
		strcpy(htlc->rootdir, buf);
	}
	if (htlc->rootdir[len-1] == dir_char)
		htlc->rootdir[len-1] = 0;
}

COMMAND(pwd)
{
	hx_printf_prefix(htlc, chat, INFOPREFIX, "%s%c\n", htlc->rootdir, dir_char);
}

COMMAND(lcd)
{
	char buf[MAXPATHLEN];

	expand_tilde(buf, argv[1] ? argv[1] : "~");
	if (chdir(buf))
		hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: %s: %s\n", argv[0], buf, strerror(errno));
}

COMMAND(lpwd)
{
	char buf[MAXPATHLEN];

	if (!getcwd(buf, sizeof(buf) - 1))
		hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: getcwd: %s\n", argv[0], strerror(errno));
	else {
		buf[sizeof(buf) - 1] = 0;
		hx_printf_prefix(htlc, chat, INFOPREFIX, "%s\n", buf);
	}
}

COMMAND(mkdir)
{
	char *path, **paths, buf[MAXPATHLEN];
	int i, j, npaths;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <directory> [dir2...]\n", argv[0]);
		return;
	}
	for (i = 1; i < argc; i++) {
		path = argv[i];
		if (*path == dir_char)
			strcpy(buf, path);
		else
			snprintf(buf, MAXPATHLEN, "%s%c%s", htlc->rootdir, dir_char, path);
		path = buf;
		paths = glob_remote(path, &npaths, 0);
		for (j = 0; j < npaths; j++) {
			path = paths[j];
			hx_mkdir(htlc, path);
			xfree(path);
		}
		xfree(paths);
	}
}

COMMAND(rm)
{
	char *path, **paths, buf[MAXPATHLEN];
	int i, j, npaths;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <file> [file2...]\n", argv[0]);
		return;
	}
	for (i = 1; i < argc; i++) {
		path = argv[i];
		if (*path == dir_char)
			strcpy(buf, path);
		else
			snprintf(buf, MAXPATHLEN, "%s%c%s", htlc->rootdir, dir_char, path);
		path = buf;
		paths = glob_remote(path, &npaths, 0);
		for (j = 0; j < npaths; j++) {
			path = paths[j];
			hx_file_delete(htlc, path, 1);
			xfree(path);
		}
		xfree(paths);
	}
}

COMMAND(ln)
{
	char *src_path, *dst_path, **paths;
	char src_buf[MAXPATHLEN], dst_buf[MAXPATHLEN];
	int i, j, npaths;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <source file> <dest file>\n", argv[0]);
		return;
	}
	dst_path = argv[--argc];
	if (*dst_path == dir_char)
		strcpy(dst_buf, dst_path);
	else
		snprintf(dst_buf, MAXPATHLEN, "%s%c%s", htlc->rootdir, dir_char, dst_path);
	dst_path = dst_buf;
	for (i = 1; i < argc; i++) {
		src_path = argv[i];
		if (*src_path == dir_char)
			strcpy(src_buf, src_path);
		else
			snprintf(src_buf, MAXPATHLEN, "%s%c%s", htlc->rootdir, dir_char, src_path);
		src_path = src_buf;
		paths = glob_remote(src_path, &npaths, 0);
		for (j = 0; j < npaths; j++) {
			src_path = paths[j];
			hx_file_link(htlc, src_path, dst_path);
			xfree(src_path);
		}
		xfree(paths);
	}
}

COMMAND(mv)
{
	char *src_path, *dst_path, **paths;
	char src_buf[MAXPATHLEN], dst_buf[MAXPATHLEN];
	int i, j, npaths;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <source file> <dest file>\n", argv[0]);
		return;
	}
	dst_path = argv[--argc];
	if (*dst_path == dir_char)
		strcpy(dst_buf, dst_path);
	else
		snprintf(dst_buf, MAXPATHLEN, "%s%c%s", htlc->rootdir, dir_char, dst_path);
	dst_path = dst_buf;
	for (i = 1; i < argc; i++) {
		src_path = argv[i];
		if (*src_path == dir_char)
			strcpy(src_buf, src_path);
		else
			snprintf(src_buf, MAXPATHLEN, "%s%c%s", htlc->rootdir, dir_char, src_path);
		src_path = src_buf;
		paths = glob_remote(src_path, &npaths, 0);
		for (j = 0; j < npaths; j++) {
			src_path = paths[j];
			hx_file_move(htlc, src_path, dst_path);
			xfree(src_path);
		}
		xfree(paths);
	}
}

COMMAND(finfo)
{
	char *path, **paths, buf[MAXPATHLEN];
	int i, j, npaths;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <file> [file2...]\n", argv[0]);
		return;
	}
	for (i = 1; i < argc; i++) {
		path = argv[i];
		if (*path == dir_char)
			strcpy(buf, path);
		else
			snprintf(buf, MAXPATHLEN, "%s%c%s", htlc->rootdir, dir_char, path);
		path = buf;
		paths = glob_remote(path, &npaths, 0);
		for (j = 0; j < npaths; j++) {
			path = paths[j];
			hx_get_file_info(htlc, path, 1);
			xfree(path);
		}
		xfree(paths);
	}
}

struct exec_info {
	struct htlc_conn *htlc;
	u_int32_t cid;
	int output;
};

static void
exec_close (int fd)
{
	close(fd);
	xfree(hxd_files[fd].conn.ptr);
	memset(&hxd_files[fd], 0, sizeof(struct hxd_file));
	hxd_fd_clr(fd, FDR);
	hxd_fd_del(fd);
}

static void
exec_ready_read (int fd)
{
	ssize_t r;
	u_int8_t buf[0x4000];

	r = read(fd, buf, sizeof(buf) - 1);
	if (r == 0 || (r < 0 && errno != EINTR)) {
		exec_close(fd);
	} else {
		struct htlc_conn *htlc;
		struct exec_info *ex;

		buf[r] = 0;
		ex = (struct exec_info *)hxd_files[fd].conn.ptr;
		htlc = ex->htlc;
		if (ex->output) {
			LF2CR(buf, r);
			if (buf[r - 1] == '\r') {
				r--;
				buf[r] = 0;
			}
			hx_send_chat(htlc, ex->cid, buf, r, 0);
		} else {
			hx_printf(htlc, 0, "%s", buf);
		}
	}
}

COMMAND(exec)
{
	int pfds[2];
	char *p, *av[4];
	int output_to = 0;
	struct exec_info *ex;

#if defined(__WIN32__)
	hx_printf_prefix(htlc, chat, INFOPREFIX, "sorry, no exec on windows\n", argv[0]);
#else
	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s [-o] <command>\n", argv[0]);
		return;
	}
	p = str;
find_cmd_arg:
	for (; *p && *p != ' '; p++) ;
	if (!*p || !(*++p))
		return;
	if (*p == '-' && *(p + 1) == 'o') {
		output_to = 1;
		goto find_cmd_arg;
	}
	if (pipe(pfds)) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: pipe: %s\n", argv[0], strerror(errno));
		return;
	}
	if (pfds[0] >= hxd_open_max) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "%s:%d: %d >= hxd_open_max (%d)\n", __FILE__, __LINE__, pfds[0], hxd_open_max);
		close(pfds[0]);
		close(pfds[1]);
		return;
	}
	switch (fork()) {
		case -1:
			hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: fork: %s\n", argv[0], strerror(errno));
			close(pfds[0]);
			close(pfds[1]);
			return;
		case 0:
			close(pfds[0]);
			av[0] = "/bin/sh";
			av[1] = "-c";
			av[2] = p;
			av[3] = 0;
			if (pfds[1] != 1) {
				if (dup2(pfds[1], 1) == -1) {
					hx_printf_prefix(htlc, chat, INFOPREFIX,
							"%s: dup2(%d,%d): %s\n",
							argv[0], pfds[1], 1, strerror(errno));
					_exit(1);
				}
			}
			if (pfds[1] != 2) {
				if (dup2(pfds[1], 2) == -1) {
					hx_printf_prefix(htlc, chat, INFOPREFIX,
							"%s: dup2(%d,%d): %s\n",
							argv[0], pfds[1], 2, strerror(errno));
					_exit(1);
				}
			}
			close(0);
			execve("/bin/sh", av, hxd_environ);
			hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: execve: %s\n", argv[0], strerror(errno));
			_exit(127);
		default:
			close(pfds[1]);
			ex = xmalloc(sizeof(struct exec_info));
			ex->htlc = htlc;
			if (chat)
				ex->cid = chat->cid;
			else
				ex->cid = 0;
			ex->output = output_to;
			hxd_files[pfds[0]].conn.ptr = ex;
			hxd_files[pfds[0]].ready_read = exec_ready_read;
			hxd_fd_add(pfds[0]);
			hxd_fd_set(pfds[0], FDR);
			break;
	}
#endif
}

/* Converts seconds to days/hours/minutes */
void
human_time (time_t ts, char *buf)
{
	unsigned int m, h, d, s;

	s = (unsigned int)ts;
	m = s/60;
	m %= 60;
	h = s/3600;
	h %= 24;
	d = s/86400;
	s %= 60;
	if (d)
		snprintf(buf, 32, "%u day%s %u:%u:%u", d, d == 1 ? "" : "s", h, m, s);
	else
		snprintf(buf, 32, "%u:%u:%u", h, m, s);
}

extern struct htxf_conn **xfers;
extern int nxfers;

static void
xfers_print (struct htlc_conn *htlc, struct hx_chat *chat, int argc, char **argv)
{
	struct htxf_conn *htxfp;
	int i, j;
	char humanbuf[LONGEST_HUMAN_READABLE*3+3], *posstr, *sizestr, *bpsstr;
	char etastr[32];

	hx_output.mode_underline();
	hx_printf(htlc, chat, " #   type   pid  queue  file\n");
	hx_output.mode_clear();
	mutex_lock(&htlc->htxf_mutex);
	for (i = 0; i < nxfers; i++) {
		struct timeval now;
		time_t sdiff, usdiff, Bps, eta;

		if (argc > 1) {
			for (j = 1; j < argc; j++)
				if (i == (int)atou32(argv[j]))
					goto want_this;
			continue;
		}
want_this:
		htxfp = xfers[i];
		gettimeofday(&now, 0);
		sdiff = now.tv_sec - htxfp->start.tv_sec;
		usdiff = now.tv_usec - htxfp->start.tv_usec;
		if (!sdiff)
			sdiff = 1;
		Bps = htxfp->total_pos / sdiff;
		if (!Bps)	
			Bps = 1;
		posstr = human_size(htxfp->total_pos, humanbuf);
		sizestr = human_size(htxfp->total_size, humanbuf+LONGEST_HUMAN_READABLE+1);
		bpsstr = human_size(Bps, humanbuf+LONGEST_HUMAN_READABLE*2+2);
		eta = (htxfp->total_size - htxfp->total_pos) / Bps
		    + ((htxfp->total_size - htxfp->total_pos) % Bps) / Bps;
		human_time(eta, etastr);
		hx_printf(htlc, chat, "[%d]  %s  %5u  %5d  %s/%s  %s/s  ETA: %s  %s\n", i,
			  htxfp->type & XFER_GET ? "get" : "put",
			  htxfp->tid,
			  htxfp->queue_pos,
			  posstr, sizestr, bpsstr, etastr, htxfp->path);
	}
	mutex_unlock(&htlc->htxf_mutex);
}

COMMAND(xfers)
{
	xfers_print(htlc, chat, argc, argv);
}

extern void xfer_delete_all (void);

COMMAND(xfkill)
{
	int i;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <xfer number> | *\n", argv[0]);
		return;
	}
	if (argv[1][0] == '*') {
		mutex_lock(&htlc->htxf_mutex);
		xfer_delete_all();
		mutex_unlock(&htlc->htxf_mutex);
		return;
	}
	mutex_lock(&htlc->htxf_mutex);
	for (i = 1; i < argc; i++) {
		int x = atou32(argv[i]);
		if (x < nxfers)
			xfer_delete(xfers[x]);
	}
	mutex_unlock(&htlc->htxf_mutex);
}

COMMAND(xfgo)
{
	int i;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <xfer number> | *\n", argv[0]);
		return;
	}
	if (argv[1][0] == '*') {
		mutex_lock(&htlc->htxf_mutex);
		for (i = 0; i < nxfers; i++)
			xfer_go(xfers[i]);
		mutex_unlock(&htlc->htxf_mutex);
		return;
	}
	mutex_lock(&htlc->htxf_mutex);
	for (i = 1; i < argc; i++) {
		int x = atou32(argv[i]);
		if (x < nxfers)
			xfer_go(xfers[x]);
	}
	mutex_unlock(&htlc->htxf_mutex);
}

extern void hx_file_get (struct htlc_conn *htlc, char *rpath, int recurs, int retry, int preview, char **filter);
extern void hx_file_put (struct htlc_conn *htlc, char *lpath);

static char *tr_argv[] = { PATH_TR, "\r", "\n", 0 };

struct option get_opts[] = {
	{"recursive", 0, 0, 'R'},
	{"keeptrying", 0, 0, 'k'},
	{"preview", 0, 0, 'p'},
	{"tr", 0, 0, 't'},
	{0, 0, 0, 0}
};

COMMAND(get)
{
	char *rpath;
	int i, o, longind, recurs = 0, retry = 0, preview = 0;
	char **filter = 0;
	struct opt_r opt;

	if (argc < 2) {
usage:		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s [-Rktp] <file> [file2...]\n", argv[0]);
		return;
	}
	opt.err_printf = err_printf;
	opt.ind = 0;
	while ((o = getopt_long_r(argc, argv, "Rkpt", get_opts, &longind, &opt)) != EOF) {
		if (o == 0)
			o = get_opts[longind].val;
		switch (o) {
			case 'R':
				recurs = 1;
				break;
			case 'k':
				retry = 0xffffffff;
				break;
			case 't':
				filter = tr_argv;
				break;
			case 'p':
				preview = 1;
				break;
			default:
				hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: unknown option '%c'\n", argv[0], o);
				goto usage;
				break;
		}
	}
	for (i = opt.ind; i < argc; i++) {
		rpath = argv[i];
		hx_file_get(htlc, rpath, recurs, retry, preview, filter);
	}
}

COMMAND(put)
{
	char *lpath;
	int i;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <file> [file2...]\n", argv[0]);
		return;
	}
	for (i = 1; i < argc; i++) {
		lpath = argv[i];
		hx_file_put(htlc, lpath);
	}
}

COMMAND(clear)
{
	hx_output.clear(htlc, chat);
}

COMMAND(save)
{
	if (!argv[1])
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <file>\n", argv[0]);
	else
		hx_save(htlc, chat, argv[1]);
}

static int away_log_fd = -1;

void
away_log (const char *fmt, ...)
{
	va_list ap;
	va_list save;
	char *buf;
	size_t mal_len, stamp_len;
	int len;
	time_t t;
	struct tm tm;

	if (away_log_fd == -1)
		return;
	__va_copy(save, ap);
	mal_len = 256;
	buf = xmalloc(mal_len);
	time(&t);
	localtime_r(&t, &tm);
	stamp_len = strftime(buf, mal_len, "[%H:%M:%S %m/%d/%Y] ", &tm);
	for (;;) {
		va_start(ap, fmt);
		len = vsnprintf(buf + stamp_len, mal_len - stamp_len, fmt, ap);
		va_end(ap);
		if (len != -1)
			break;
		mal_len <<= 1;
		buf = xrealloc(buf, mal_len);
		__va_copy(ap, save);
	}
	write(away_log_fd, buf, stamp_len + len);
	xfree(buf);
}

COMMAND(away)
{
	int f;
	char *file = "hx.away";
	char *msg;
	char buf[4096];
	u_int16_t style, away;
	int len;

	for (msg = str; *msg && *msg != ' '; msg++) ;
	if (!*msg || !*++msg)
		msg = away_log_fd == -1 ? "asf" : "fsa";
	len = snprintf(buf, sizeof(buf), away_log_fd == -1 ? "is away : %s" : "is back : %s", msg);
	style = htons(1);
	away = htons(away_log_fd == -1 ? 1 : 2);
	if (chat && chat->cid) {
		u_int32_t cid = htonl(chat->cid);
		hlwrite(htlc, HTLC_HDR_CHAT, 0, 4,
			HTLC_DATA_STYLE, 2, &style,
			HTLC_DATA_CHAT_AWAY, 2, &away,
			HTLC_DATA_CHAT, len, buf,
			HTLC_DATA_CHAT_ID, 4, &cid);
	} else
		hlwrite(htlc, HTLC_HDR_CHAT, 0, 3,
			HTLC_DATA_STYLE, 2, &style,
			HTLC_DATA_CHAT_AWAY, 2, &away,
			HTLC_DATA_CHAT, len, buf);
	if (away_log_fd == -1) {
		f = SYS_open(file, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
		away_log_fd = f;
	} else {
		SYS_fsync(away_log_fd);
		close(away_log_fd);
		away_log_fd = -1;
	}
}

#if defined(CONFIG_HOPE)
extern char *dirchar_basename (const char *rpath);

extern void hx_file_hash (struct htlc_conn *htlc, const char *path, u_int32_t data_size, u_int32_t rsrc_size);

COMMAND(hash)
{
	char **paths, *lpath, *rpath, *rfile, buf[MAXPATHLEN];
	int i, j, npaths;
	u_int32_t data_size, rsrc_size;
	struct stat sb;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <file> [file2...]\n", argv[0]);
		return;
	}
	for (i = 1; i < argc; i++) {
		rpath = argv[i];
		if (rpath[0] != dir_char) {
			snprintf(buf, MAXPATHLEN, "%s%c%s", htlc->rootdir, dir_char, rpath);
			rpath = buf;
		}
		paths = glob_remote(rpath, &npaths, 0);
		for (j = 0; j < npaths; j++) {
			rpath = paths[j];
			rfile = dirchar_basename(rpath);
			lpath = rfile;
			data_size = 0xffffffff;
			rsrc_size = 0xffffffff;
			if (!stat(lpath, &sb))
				data_size = sb.st_size;
			rsrc_size = resource_len(lpath);
			hx_file_hash(htlc, rpath, data_size, rsrc_size);
			xfree(rpath);
		}
		xfree(paths);
	}
}
#endif

COMMAND(hostname)
{
	if (argv[1] && argv[1][0] != '#') {
		if (strlen(argv[1]) >= sizeof(hx_hostname)) {
			hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: '%s' is too long\n", argv[0], argv[1]);
		} else {
			strcpy(hx_hostname, argv[1]);
			hx_printf_prefix(htlc, chat, INFOPREFIX, "local hostname is now [%s]\n", hx_hostname);
		}
		return;
	}

	{
#if !defined(__linux__)
		char hostname[256];

#if defined(HAVE_GETHOSTNAME)
		if (gethostname(hostname, sizeof(hostname)))
#endif
			strcpy(hostname, "localhost");
		hx_printf_prefix(htlc, chat, INFOPREFIX, "local hostname is [%s]\n", hx_hostname ? hx_hostname : hostname);
#else
		char comm[200];
		FILE *fp;
		char *p = NULL, *q, *hname;
		unsigned long ip;
		struct hostent *he;
		unsigned int i = 0;
		struct IN_ADDR inaddr;

		if (!(fp = popen("/sbin/ifconfig -a", "r"))) {
			hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: /sbin/ifconfig: %s", argv[0], strerror(errno));
			return;
		}
		hx_printf_prefix(htlc, chat, INFOPREFIX, "looking for hostnames with ifconfig\n");
		while ((fgets(comm, 200, fp))) {
#ifdef CONFIG_IPV6
			if ((p = strstr(comm, "inet addr")) || (p = strstr(comm, "inet6 addr"))) {
#else
				if ((p = strstr(comm, "inet addr"))) {
#endif
				p += 10;
				q = strchr(p, ' ');
				*q = 0;
				if ((p && !*p) || (p && !strcmp(p, "127.0.0.1"))) continue;
				ip = inet_addr(p);
				i++;
				/* inaddr.S_ADDR = ip; */
#ifdef CONFIG_IPV6
				inet_ntop(AFINET, (char *)&inaddr, comm, sizeof(comm));
#else
				inet_ntoa_r(inaddr, comm, sizeof(comm));
#endif
				if ((he = gethostbyaddr((char *)&ip, sizeof(ip), AFINET)))
					hname = he->h_name;
				else
					hname = comm;
				hx_printf(htlc, chat, " [%d] %s (%s)\n", i, hname, comm);
				if (argv[1] && argv[1][0] == '#' && i == atou32(argv[1]+1)) {
					if (strlen(hname) >= sizeof(hx_hostname)) {
						hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: '%s' is too long\n", argv[0], hname);
					} else {
						strcpy(hx_hostname, hname);
						hx_printf_prefix(htlc, chat, INFOPREFIX, "local hostname is now [%s]\n", hx_hostname);
					}
					break;
				}
			}
		}
		fclose(fp);
#endif
	}
}

COMMAND(tracker)
{
	u_int16_t port;
	int i;

	if (argc < 2) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <server> [port]\n", argv[0]);
		return;
	}

	for (i = 1; i < argc; i++) {
		if (argv[i + 1] && isdigit(argv[i + 1][0]))
			port = atou32(argv[++i]);
		else
			port = HTRK_TCPPORT;
		hx_tracker_list(htlc, chat, argv[i], port);
	}
}

#if defined(CONFIG_XMMS)
extern char *xmms_remote_get_playlist_title (int, int);
extern int xmms_remote_get_playlist_pos (int);
extern void xmms_remote_get_info (int, int *, int *, int *);

COMMAND(trackname)
{
	char *string;
	char buf[1024], *channels;
	int rate, freq, nch;
	u_int16_t style;

	string = xmms_remote_get_playlist_title(0, xmms_remote_get_playlist_pos(0));
	if (!string) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "%s: failed to get XMMS playlist\n", argv[0]);
		return;
	}
	xmms_remote_get_info(0, &rate, &freq, &nch);
	rate = rate / 1000;
	if (nch == 2)
		channels = "stereo";
	else
		channels = "mono";

	snprintf(buf, sizeof(buf), "is listening to: %s (%d kbit/s %d Hz %s)",
		 string, rate, freq, channels);
	style = htons(1);
	if (chat && chat->cid) {
		u_int32_t cid = htonl(chat->cid);
		hlwrite(htlc, HTLC_HDR_CHAT, 0, 3,
			HTLC_DATA_STYLE, 2, &style,
			HTLC_DATA_CHAT, strlen(buf), buf,
			HTLC_DATA_CHAT_ID, 4, &cid);
	} else {
		hlwrite(htlc, HTLC_HDR_CHAT, 0, 2,
			HTLC_DATA_STYLE, 2, &style,
			HTLC_DATA_CHAT, strlen(buf), buf);
	}
	free(string);
}
#endif

#if defined(CONFIG_SOUND)

extern int sound_on;
extern char *sound_stdin_filename;
extern char default_player[MAXPATHLEN];
extern void load_sndset (struct htlc_conn *htlc, struct hx_chat *chat, const char *filename);

static const char *event_name[] = { "news", "chat", "join", "part", "message",
				    "login", "error", "file_done", "chat_invite" };

COMMAND(snd)
{
	struct opt_r opt;
	char *p = 0, *s = 0, *sndset = 0;
	int i, plen, slen, on = 1, ok = 0, beep = 0;

        opt.err_printf = err_printf;
        opt.ind = 0;
        while ((i = getopt_r(argc, argv, "bd:p:s:S:01oO", &opt)) != EOF) {
                switch (i) {
			case 'b':
				beep = 1;
				break;
			case 'd':
				strlcpy(default_player, opt.arg, MAXPATHLEN);
				ok = 1;
				break;
			case 'p':
				p = opt.arg;
				break;
			case 's':
        	                s = opt.arg;
        	                break;
			case 'S':
				sndset = opt.arg;
        	                break;
			case '0':
				on = 0;
				break;
			case '1':
				on = 1;
				break;
			case 'o':
				sound_on = 0;
				ok = 1;
				break;
			case 'O':
				sound_on = 1;
				ok = 1;
				break;
			default:
				goto usage;
		}
	}
	if (sndset) {
		load_sndset(htlc, chat, sndset);
		ok = 1;
	}
	if (argc - opt.ind != 1) {
		if (ok && argc - opt.ind == 0)
			return;
usage:		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s [OPTIONS] <event>\n"
				     "  -b beep if sound cannot be played\n"
				     "  -d <default player> (%s)\n"
				     "  -p <player>\n"
				     "  -s <sound file>\n"
				     "  -S <sound set>\n"
				     "  -0 turn sound for <event> off\n"
				     "  -1 turn sound for <event> on\n"
				     "  -o turn all sound off\n"
				     "  -O turn all sound on\n"
				     "  events are: news, chat, join, part, message,\n"
				     "              login, error, file_done, chat_invite\n"
				     "  beep (pc speaker) can be on or off\n"
			  , argv[0], default_player);
                return;
	}
	for (i = 0; i < (int)nsounds; i++) {
		if (!strcmp(argv[opt.ind], event_name[i]))
			break;
	}
	if (i == (int)nsounds)
		goto usage;
	opt.ind++;
	sounds[i].beep = beep;
	slen = s ? strlen(s) : 0;
	plen = p ? strlen(p) : 0;
	if (slen > MAXPATHLEN || plen > MAXPATHLEN) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "file path too long.");
		return;
	}
	if (s) {
		/* free soundset data */
		if (sounds[i].data) {
			xfree(sounds[i].data);
			sounds[i].data = 0;
		}
		if (sounds[i].sound && sounds[i].sound != sound_stdin_filename) 
			xfree(sounds[i].sound);
		sounds[i].sound = xmalloc(slen+1);
		strcpy(sounds[i].sound, s);
	}
	if (p) {
		if (sounds[i].player) 
			xfree(sounds[i].player);
		sounds[i].player = xmalloc(plen+1);
		strcpy(sounds[i].player, p);
	}
	sounds[i].on = on;
}
#endif

extern int chat_colorz;

COMMAND(colorz)
{
	chat_colorz = !chat_colorz;
}

struct variable *variable_tail = 0;

static void
var_set_strs (struct variable *var, const char *varnam, const char *value)
{
	unsigned int i;

	for (i = 0; i < var->nstrs; i++) {
		if (!strcmp(var->namstr[i], varnam)) {
			xfree(var->namstr[i]);
			xfree(var->valstr[i]);
			goto not_new;
		}
	}
	i = var->nstrs;
	var->nstrs++;
	var->namstr = xrealloc(var->namstr, sizeof(char *) * var->nstrs);
	var->valstr = xrealloc(var->valstr, sizeof(char *) * var->nstrs);
not_new:
	var->namstr[i] = xstrdup(varnam);
	var->valstr[i] = xstrdup(value);
}

struct variable *
variable_add (void *ptr, void (*set_fn)(), const char *nam)
{
	struct variable *var;
	int len;
	char *valstr, valbuf[32];

	len = strlen(nam) + 1;
	var = xmalloc(sizeof(struct variable)+len);
	strcpy(var->nam, nam);
	var->set_fn = set_fn;
	var->ptr = ptr;
	var->namstr = 0;
	var->valstr = 0;
	var->nstrs = 0;
	var->prev = variable_tail;
	variable_tail = var;
	valstr = "";
	if (set_fn == set_bool) {
		snprintf(valbuf, sizeof(valbuf), "%d", *((int *)ptr));
		valstr = valbuf;
	} else if (set_fn == set_int16) {
		snprintf(valbuf, sizeof(valbuf), "%u", *((u_int16_t *)ptr));
		valstr = valbuf;
	} else if (set_fn == set_float) {
		snprintf(valbuf, sizeof(valbuf), "%g", *((float *)ptr));
		valstr = valbuf;
	} else if (set_fn == set_string) {
		valstr = *((char **)ptr);
	}
	var_set_strs(var, nam, valstr);

	return var;
}

void
set_bool (int *boolp, const char *str)
{
	*boolp = *str == '1' ? 1 : 0;
}

void
set_int16 (u_int16_t *intp, const char *str)
{
	*intp = atou16(str);
}

void
set_float (float *floatp, const char *str)
{
	*floatp = atof(str);
}

void
set_string (char **strp, const char *str)
{
	*strp= xstrdup(str);
}

void
variable_set (struct htlc_conn *htlc, struct hx_chat *chat, const char *varnam, const char *value)
{
	struct variable *var;
	int found = 0;

	for (var = variable_tail; var; var = var->prev) {
		if (!fnmatch(var->nam, varnam, 0)) {
			if (var->set_fn)
				var->set_fn(var->ptr, value, varnam);
			var_set_strs(var, varnam, value);
			found = 1;
		}
	}
	if (!found) {
		/*hx_printf_prefix(htlc, chat, INFOPREFIX, "no variable named %s\n", varnam);*/
		hx_printf_prefix(htlc, chat, INFOPREFIX, "adding variable %s\n", varnam);
		var = variable_add(0, 0, varnam);
		var_set_strs(var, varnam, value);
	}
}

COMMAND(set)
{
	struct variable *var;
	unsigned int i, found = 0;
	char *varnam, *vararg;

	if (argc < 1) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "usage: %s <varnam> [value]\n", argv[0]);
		return;
	}
	varnam = argv[1];
	if (argc == 1) {
		for (var = variable_tail; var; var = var->prev) {
			for (i = 0; i < var->nstrs; i++) {
				hx_printf_prefix(htlc, chat, INFOPREFIX, "%s = %s\n",
						 var->namstr[i], var->valstr[i]);
			}
		}
	} else if (argc == 2) {
		for (var = variable_tail; var; var = var->prev) {
			if (!fnmatch(var->nam, varnam, 0)) {
				found = 1;
				for (i = 0; i < var->nstrs; i++) {
					hx_printf_prefix(htlc, chat, INFOPREFIX, "%s = %s\n",
							 var->namstr[i], var->valstr[i]);
				}
			}
		}
		if (!found)
			hx_printf_prefix(htlc, chat, INFOPREFIX, "no variable named %s\n", varnam);
	} else {
		vararg = argv[2];
		variable_set(htlc, chat, varnam, vararg);
	}
}

extern char g_hxvars_path[MAXPATHLEN];

void
hx_savevars (void)
{
	struct variable *var;
	char buf[4096];
	ssize_t len, r;
	int fd;
	unsigned int i;
	char path[MAXPATHLEN];

	expand_tilde(path, g_hxvars_path);
	fd = SYS_open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd < 0) {
err:
		hx_printf_prefix(&hx_htlc, 0, INFOPREFIX, "ERROR saving vars to %s: %s\n", path, strerror(errno));
		return;
	}
	for (var = variable_tail; var; var = var->prev) {
		if (!var->nstrs)
			continue;
		for (i = 0; i < var->nstrs; i++) {
			if (!var->valstr[i][0])
				continue;
			len = snprintf(buf, sizeof(buf), "%s='%s'\n",
				       var->namstr[i], var->valstr[i]);
			r = write(fd, buf, len);
			if (r != len) {
				close(fd);
				goto err;
			}
		}
	}
	close(fd);
}

COMMAND(savevars)
{
	hx_savevars();
}

struct hx_command {
	char *name;
	void (*fun)();
};

static struct hx_command *commands, *last_command;

COMMAND(help)
{
	struct hx_command *cmd;
	unsigned int i;
	int r;
	char buf[2048], *bufp = buf;

	for (i = 0, cmd = commands; cmd <= last_command; cmd++) {
		i++;
		r = snprintf(bufp, sizeof(buf) - (bufp - buf) - 1, "  %-8s", cmd->name);
		if (r == -1 || (sizeof(buf) - 1) <= (unsigned)(bufp + r - buf))
			break;
		bufp += r;
		if (!(i % 5))
			*bufp++ = '\n';
	}
	*bufp = 0;
	hx_printf_prefix(htlc, chat, INFOPREFIX, "commands:\n%s%c", buf, (i % 5) ? '\n' : 0);
}

COMMAND(version)
{
	hx_printf_prefix(htlc, chat, INFOPREFIX, "hx version %s\n", hxd_version);
}

COMMAND(echo)
{
	char const *p;

	for (p = str; *p; p++)
		if (p > str+4)
			break;
	hx_printf(htlc, chat, "%s\n", p);
}

#if XMALLOC_DEBUG
COMMAND(maltbl)
{
	extern void DTBLWRITE (void);
	DTBLWRITE();
}
#endif

static struct hx_command __commands[] = {
	{ "away",	cmd_away },
	{ "banner",	cmd_banner },
	{ "broadcast",	cmd_broadcast },
	{ "cd",		cmd_cd },
	{ "chat",	cmd_chat },
	{ "chats",	cmd_chats },
	{ "clear",	cmd_clear },
	{ "close",	cmd_close },
	{ "colorz",	cmd_colorz },
	{ "dirchar",	cmd_dirchar },
	{ "echo",	cmd_echo },
	{ "exec",	cmd_exec },
	{ "finfo",	cmd_finfo },
	{ "get",	cmd_get },
#if defined(CONFIG_HAL)
	{ "hal",	cmd_hal },
#endif
#if defined(CONFIG_HOPE)
	{ "hash",	cmd_hash },
#endif
	{ "help",	cmd_help },
	{ "hostname",	cmd_hostname },
	{ "icon",	cmd_icon },
	{ "ignore",	cmd_ignore },
	{ "info",	cmd_info },
	{ "invite",	cmd_invite },
	{ "join",	cmd_join },
	{ "kick",	cmd_kick },
	{ "lcd",	cmd_lcd },
	{ "leave",	cmd_part },
	{ "ln",		cmd_ln },
	{ "load",	cmd_load },
	{ "lpwd",	cmd_lpwd },
	{ "ls",		cmd_ls },
	{ "mkick",	cmd_mkick },
	{ "msg",	cmd_msg },
	{ "me",		cmd_me },
	{ "mkdir",	cmd_mkdir },
	{ "mv",		cmd_mv },
#if XMALLOC_DEBUG
	{ "maltbl",	cmd_maltbl },
#endif
	{ "news",	cmd_news },
	{ "nls",	cmd_nls },
	{ "nlsc",	cmd_nlsc },
	{ "nget",	cmd_nget },
	{ "nick",	cmd_nick },
	{ "part",	cmd_part },
	{ "password",	cmd_password },
	{ "post",	cmd_post },
	{ "put",	cmd_put },
	{ "pwd",	cmd_pwd },
	{ "quit",	cmd_quit },
	{ "rm",		cmd_rm },
	{ "save",	cmd_save },
	{ "savevars",	cmd_savevars },
	{ "server",	cmd_server },
	{ "set",	cmd_set },
#if defined(CONFIG_SOUND)
	{ "snd",	cmd_snd },
#endif
	{ "subject",	cmd_subject },
	{ "tasks",	cmd_tasks },
#if defined(CONFIG_TRANSLATE)
	{ "translate",	cmd_translate },
#endif
	{ "tracker",	cmd_tracker },
#if defined(CONFIG_XMMS)
	{ "trackname",  cmd_trackname },
#endif		
	{ "type",	cmd_type },
	{ "unignore",	cmd_ignore },
	{ "unmkick",	cmd_unmkick },
#if defined(CONFIG_TRANSLATE)
	{ "untranslate",cmd_translate },
#endif
	{ "version",	cmd_version },
	{ "who",	cmd_who },
	{ "xfers",	cmd_xfers },
	{ "xfgo",	cmd_xfgo },
	{ "xfkill",	cmd_xfkill }
};

static struct hx_command
	*commands = __commands,
	*last_command = __commands + sizeof(__commands) / sizeof(struct hx_command) - 1;

static short command_hash[26];

void
gen_command_hash (void)
{
	int i, n;
	struct hx_command *cmd;

	cmd = commands;
	for (n = 0, i = 0; i < 26; i++) {
		if (cmd->name[0] == i + 'a') {
			command_hash[i] = n;
			do {
				if (++cmd > last_command) {
					for (i++; i < 26; i++)
						command_hash[i] = -1;
					return;
				}
				n++;
			} while (cmd->name[0] == i + 'a');
		} else
			command_hash[i] = -1;
	}
}

int
expand_command (char *cmdname, int len, char *cmdbuf)
{
	struct hx_command *cmd;
	char *ename = 0;
	int elen = 0, ambig = 0;

	for (cmd = commands; cmd <= last_command; cmd++) {
		if (!strncmp(cmd->name, cmdname, len)) {
			if (ename) {
				if ((int)strlen(cmd->name) < elen)
					elen = strlen(cmd->name);
				for (elen = len; cmd->name[elen] == ename[elen]; elen++)
					;
				ambig = 1;
			} else {
				ename = cmd->name;
				elen = strlen(ename);
			}
		}
	}

	if (ename)
		strncpy(cmdbuf, ename, elen);
	else
		ambig = -1;
	cmdbuf[elen] = 0;

	return ambig;
}

u_int32_t
cmd_arg (int argn, char *str)
{
	char *p, *cur;
	char c, quote = 0;
	int argc = 0;
	u_int32_t offset = 0, length = 0;

	p = str;
	while (isspace(*p)) p++;
	for (cur = p; (c = *p); ) {
		if (c == '\'' || c == '"') {
			if (quote == c) {
				argc++;
				if (argn == argc) {
					p++;
					while (isspace(*p)) p++;
					offset = p - str;
					p--;
				} else if (argn + 1 == argc) {
					length = p - (str + offset);
					break;
				} else {
					p++;
					while (isspace(*p)) p++;
					p--;
				}
				quote = 0;
				cur = ++p;
			} else if (!quote) {
				quote = c;
				cur = ++p;
			}
		} else if (!quote && isspace(c)) {
			argc++;
			if (argn == argc) {
				p++;
				while (isspace(*p)) p++;
				offset = p - str;
				p--;
			} else if (argn + 1 == argc) {
				length = p - (str + offset);
				break;
			} else {
				p++;
				while (isspace(*p)) p++;
				p--;
			}
			cur = ++p;
		} else if (c == '\\' && *(p+1) == ' ') {
			p += 2;
		} else p++;
	}
	if (p != cur)
		argc++;
	if (argn == argc && 0 && argn != 1) {
		cur--;
		offset = cur - str;
		length = strlen(cur);
	} else if (argn + 1 == argc) {
		length = p - (str + offset);
	}

	return (offset << 16) | (length & 0xffff);
}

#define killspace(s) while (isspace(*(s))) strcpy((s), (s)+1)
#define add_arg(s)				\
	do {					\
		argv[argc++] = s;		\
		if ((argc % 16) == 0) {		\
			if (argc == 16) {	\
				argv = xmalloc(sizeof(char *) * (argc + 16));	\
				memcpy(argv, auto_argv, sizeof(char *) * 16);	\
			} else {						\
				argv = xrealloc(argv, sizeof(char *) * (argc + 16));\
			}			\
		}				\
	} while (0)

static void
variable_command (struct htlc_conn *htlc, struct hx_chat *chat, char *str, char *p)
{
	int quote;
	char *start;

	quote = 0;
	*p++ = 0;
	start = p;
	for (; *p; p++) {
		if (*p == '\'') {
			if (quote == 1) {
				*p = 0;
				if (start)
					variable_set(htlc, chat, str, start);
				str = p+1;
				start = 0;
			} else if (!quote) {
				quote = 1;
				start = p+1;
			}
		} else if (*p == '"') {
			if (quote == 2) {
				*p = 0;
				if (start)
					variable_set(htlc, chat, str, start);
				str = p+1;
				start = 0;
			} else if (!quote) {
				quote = 2;
				start = p+1;
			}
		} else if (isspace(*p)) {
			if (!quote) {
				*p = 0;
				if (start)
					variable_set(htlc, chat, str, start);
				str = p+1;
				start = 0;
			}
		} else if (*p == '\\') {
			chrexpand(p, strlen(p));
		}
	}
	if (start && start != p)
		variable_set(htlc, chat, str, start);
}

void
hx_command (struct htlc_conn *htlc, struct hx_chat *chat, char *str)
{
	int i;
	struct hx_command *cmd;
	char *p;

	for (p = str; *p && !isspace(*p); p++) {
		if (*p == '=') {
			variable_command(htlc, chat, str, p);
			return;
		}
	}
	if (*str < 'a' || *str > 'z')
		goto notfound;
	i = *str - 'a';
	if (command_hash[i] == -1)
		goto notfound;
	cmd = commands + command_hash[i];
	do {
		if (!strncmp(str, cmd->name, p - str) && cmd->fun) {
			char *cur, *s;
			char c, quote = 0;
			char *auto_argv[16], **argv = auto_argv;
			int argc = 0;

			s = xstrdup(str);
			killspace(s);
			for (p = cur = s; (c = *p); p++) {
				if (c == '\'' || c == '"') {
					if (quote == c) {
						*p = 0;
						add_arg(cur);
						killspace(p+1);
						quote = 0;
						cur = p+1;
					} else if (!quote) {
						quote = c;
						cur = p+1;
					}
				} else if (!quote && isspace(c)) {
					*p = 0;
					add_arg(cur);
					killspace(p+1);
					cur = p+1;
				} else if (c == '\\') {
					chrexpand(p, strlen(p));
				}
			}
			if (p != cur)
				add_arg(cur);
			argv[argc] = 0;

			cmd->fun(argc, argv, str, htlc, chat);
			xfree(s);
			if (argv != auto_argv)
				xfree(argv);
			return;
		}
		cmd++;
	} while (cmd <= last_command && cmd->name[0] == *str);

notfound:
	hx_printf_prefix(htlc, chat, INFOPREFIX, "%.*s: command not found\n", p - str, str);
}

static char *
colorz (const char *str, size_t len)
{
	register const char *p, *end;
	register char *b;
	static unsigned short asf = 31;
	register unsigned short col = asf;
	char *buf;

	buf = malloc(strlen(str)*6+16);
	if (!buf)
		return 0;
	b = buf;
	strcpy(b, "\033[1m");
	b += 4;
	end = str + len;
	for (p = str; p < end; p++) {
		if (isspace(*p)) {
			*b++ = *p;
			continue;
		}
		b += sprintf(b, "\033[%um%c", col++, *p);
		if (col == 37)
			col = 31;
	}
	strcpy(b, "\033[0m");
	b += 4;
	asf = col;

	return buf;
}

void
hx_send_msg (struct htlc_conn *htlc, u_int32_t uid, const char *msg, u_int16_t len, void *data)
{
#if defined(CONFIG_ICONV)
	char *out_p;
	size_t out_len;
#endif
	uid = htonl(uid);
	task_new(htlc, rcv_task_msg, data, (data ? 1 : 0), data ? data : "msg");

#if defined(CONFIG_ICONV)
	out_len = convbuf(g_server_encoding, g_encoding, msg, len, &out_p);
	if (out_len) {
		hlwrite(htlc, HTLC_HDR_MSG, 0, 2,
			HTLC_DATA_UID, 4, &uid,
			HTLC_DATA_MSG, out_len, out_p);
		xfree(out_p);
	} else
#endif
		hlwrite(htlc, HTLC_HDR_MSG, 0, 2,
			HTLC_DATA_UID, 4, &uid,
			HTLC_DATA_MSG, len, msg);
}

void
hx_send_chat (struct htlc_conn *htlc, u_int32_t cid, const char *str, size_t len,
		u_int16_t opt)
{
	char *s;
	u_int16_t style;

#if defined(CONFIG_ICONV)
{
	char *out_p;
	size_t out_len;

	out_len = convbuf(g_server_encoding, g_encoding, str, len, &out_p);
	if (out_len) {
		s = out_p;
		len = out_len;
	} else {
		s = (char *)str;
	}
	if (chat_colorz) {
		char *cs = colorz(s, len);
		if (cs) {
			s = cs;
			len = strlen(cs);
		}
	}
}
#else
	if (chat_colorz) {
		s = colorz(str, len);
		if (!s)
			s = (char *)str;
		else
			len = strlen(s);
	} else
		s = (char *)str;
#endif

	style = htons(opt);
	if (cid) {
		cid = htonl(cid);
		if (style) {
			hlwrite(htlc, HTLC_HDR_CHAT, 0, 3,
				HTLC_DATA_STYLE, 2, &style,
				HTLC_DATA_CHAT, len, s,
				HTLC_DATA_CHAT_ID, 4, &cid);
		} else {
			hlwrite(htlc, HTLC_HDR_CHAT, 0, 2,
				HTLC_DATA_CHAT, len, s,
				HTLC_DATA_CHAT_ID, 4, &cid);
		}
	} else {
		if (style) {
			hlwrite(htlc, HTLC_HDR_CHAT, 0, 2,
				HTLC_DATA_STYLE, 2, &style,
				HTLC_DATA_CHAT, len, s);
		} else {
			hlwrite(htlc, HTLC_HDR_CHAT, 0, 1,
				HTLC_DATA_CHAT, len, s);
		}
	}

	if (s != str)
#if defined(CONFIG_ICONV)
		xfree(s);
#else
		free(s);
#endif
}

int hx_multikick(struct htlc_conn *htlc, const char *nam, u_int32_t uid)
{
    if(strstr(g_multikick, nam)) hx_kick_user(htlc, uid, 0);
    return 0;
}

int hx_set_multikick(struct htlc_conn *htlc, const char *nick, u_int32_t uid )
{

    if(strlen(nick) < 33) strcpy(g_multikick, nick);
    if(uid != 0) hx_kick_user(htlc, uid, 0);
    return 0;
}
int hx_unset_multikick( void )
{
    strcpy( g_multikick, "\0" );
    return 0;
}

