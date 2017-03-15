/*
 * Receive functions for the hotline protocol.
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include "sys_net.h"
#if !defined(__WIN32__)
#include <arpa/inet.h>
#endif
#include <stdlib.h>
#include "hlserver.h"
#include "rcv.h"
#include "xmalloc.h"
#include "spam.h"
#include "commands.h"
#include "snd.h"

#define MAX_HOTLINE_PACKET_LEN 0x40000

void
htlc_set_name (htlc_t *htlc, const char *name)
{
	if (hxd_cfg.text.convert_mac) {
		unsigned int i, j;
		for (i = 0, j = 0; i < sizeof(htlc->name)-1; i++, j++) {
			switch (name[j]) {
				case 0xfd:
					if (i + 3 >= sizeof(htlc->name))
						goto def;
					memcpy(&htlc->name[i], "(C)", 3);
					i += 2;
					break;
				case 0xfe:
					if (i + 4 >= sizeof(htlc->name))
						goto def;
					memcpy(&htlc->name[i], "[TM]", 4);
					i += 3;
					break;
				case 0xff:
					if (i + 3 >= sizeof(htlc->name))
						goto def;
					memcpy(&htlc->name[i], "...", 3);
					i += 2;
					break;
				case 0:
					htlc->name[i] = 0;
					return;
				default:
def:
					htlc->name[i] = name[j];
					break;
			}
		}
		htlc->name[i] = 0;
	} else {
		size_t len = strlen(name);
		if (len >= sizeof(htlc->name))
			len = sizeof(htlc->name)-1;
		memcpy(htlc->name, name, len);
		htlc->name[len] = 0;
	}
}

int
away_timer (struct htlc_conn *htlc)
{
	toggle_away(htlc);
	htlc->flags.away = AWAY_INTERRUPTABLE;

	return 0;
}

void
test_away (struct htlc_conn *htlc)
{
	if (htlc->flags.in_ping) {
		htlc->flags.in_ping = 0;
		return;
	}
	
	/* we have a valid transaction, update idle time */
	/* if (htlc->flags.away != AWAY_PERM) */
		gettimeofday(&htlc->idle_tv, 0);
		
	if (htlc->flags.away == AWAY_INTERRUPTED)
		htlc->flags.away = 0;
	if (htlc->flags.away == AWAY_INTERRUPTABLE) {
		toggle_away(htlc);
		htlc->flags.away = AWAY_INTERRUPTED;
	}
	if (hxd_cfg.options.away_time && htlc->flags.away != AWAY_PERM) {
		timer_delete_ptr(htlc);
		timer_add_secs(hxd_cfg.options.away_time, away_timer, htlc);
	}
}

#if defined(CONFIG_HLDUMP)
extern void hldump_packet (void *pktbuf, u_int32_t pktbuflen);

static void
rcv_hldump (struct htlc_conn *htlc)
{
	if (hxd_cfg.options.hldump)
		hldump_packet(htlc->in.buf, htlc->in.pos);
	if (!htlc->real_rcv)
		snd_errorstr(htlc, "Transaction rejected. (Unknown or non-authorised)");
	else
		htlc->real_rcv(htlc);
}
#endif

/* called by either rcv_login or rcv_agreementagree or rcv_user_change */
void
user_loginupdate (struct htlc_conn *htlc)
{
	u_int16_t nlen;
	u_int8_t ubuf[SIZEOF_HL_USERLIST_HDR + 32];
	struct hl_userlist_hdr *uh;
	struct {
		u_int32_t hi, lo PACKED;
	} fakeaccess;

	nlen = (u_int16_t)strlen(htlc->name);
	if (hxd_cfg.options.version != 0) {
		uh = (struct hl_userlist_hdr *)(ubuf - SIZEOF_HL_DATA_HDR);
		uh->uid = htons(mangle_uid(htlc));
		uh->icon = htons(htlc->icon);
		uh->color = htons(htlc->color);
		uh->nlen = htons(nlen);
		memcpy(uh->name, htlc->name, nlen);
		/* everything enabled */
		fakeaccess.hi = htonl(0xfff3cfef);
		fakeaccess.lo = htonl(0xff800000);
#if 1
		hlwrite(htlc, HTLS_HDR_USER_SELFINFO, 0, 2,
			HTLS_DATA_ACCESS, 8, &fakeaccess,
			HTLS_DATA_USER_LIST, (SIZEOF_HL_USERLIST_HDR - SIZEOF_HL_DATA_HDR) + nlen, ubuf);
#else
		hlwrite(htlc, HTLS_HDR_USER_SELFINFO, 0, 1,
			HTLS_DATA_ACCESS, 8, &fakeaccess);
#endif
	}

	/* enforce access file defaults set for login  */
	if (htlc->defaults.has_default_color)
					 htlc->color = htlc->defaults.color;
		  if (htlc->defaults.has_default_icon)
					 htlc->icon = htlc->defaults.icon;

#if defined(CONFIG_EXEC)
	/* exec at login */
	cmd_exec(htlc, 0, hxd_cfg.paths.luser);
#endif
	/* login invisibility */
	if (htlc->defaults.invisibility && htlc->access_extra.user_visibility) {
		 htlc->flags.visible = 0;
		 return;
	}

	snd_user_join(htlc);
}

static void
finish_login (struct htlc_conn *htlc)
{
	int err;
	u_int8_t name[32];
	char abuf[HOSTLEN+1];

	/* access was zeroed in rcv_login */
	if ((err = account_read(htlc->login, 0, name, &htlc->access))) {
		inaddr2str(abuf, &htlc->sockaddr);
		hxd_log("%s@%s:%u -- error reading account %s: %s", htlc->userid, abuf, ntohs(htlc->sockaddr.SIN_PORT), htlc->login, strerror(err));
		htlc_close(htlc);
		return;
	}
	account_get_access_extra(htlc);
	/* can_login should be 0, but just in case ... */
	htlc->access_extra.can_login = 0;
	/*
	if (htlc->access.disconnect_users)
		htlc->color = 2;
	else
		htlc->color = 0;
	*/
	htlc->flags.in_login = 0;
}

void
rcv_agreementagree (struct htlc_conn *htlc)
{
	u_int16_t nlen = 0;
	u_int8_t given_name[32];
	u_int16_t icon = 0;

	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_NAME:
				nlen = dh_len > 31 ? 31 : dh_len;
				memcpy(given_name, dh_data, nlen);
				given_name[nlen] = 0;
				break;
			case HTLC_DATA_ICON:
				dh_getint(icon);
				break;
			case HTLC_DATA_BAN:
				break;
		}
	dh_end()

	/* ack agreementagree */
	hlwrite(htlc, HTLS_HDR_TASK, 0, 0);

	if (htlc->flags.in_login)
		finish_login(htlc);

	if (htlc->access.use_any_name && nlen)
		htlc_set_name(htlc, given_name);
	if (icon)
		htlc->icon = icon;

	if (check_banlist(htlc))
		return;

	user_loginupdate(htlc);

	htlc->access_extra.can_agree = 0;

	/* Banner stuff here */
	if (hxd_cfg.banner.type[0]) {
		u_int8_t banner_type[4];
		size_t len;

		len = strlen(hxd_cfg.banner.type);
		if (len > 4)
			len = 4;
		banner_type[1] = ' ';
		banner_type[2] = ' ';
		banner_type[3] = ' ';
		memcpy(banner_type, hxd_cfg.banner.type, len);
		if (hxd_cfg.banner.url[0]) {
			hlwrite(htlc, HTLS_HDR_BANNER, 0, 2,
				HTLS_DATA_BANNER_TYPE, 4, banner_type,
				HTLS_DATA_BANNER_URL, strlen(hxd_cfg.banner.url), hxd_cfg.banner.url);
		} else {
			hlwrite(htlc, HTLS_HDR_BANNER, 0, 1,
				HTLS_DATA_BANNER_TYPE, 4, banner_type);
		}
	}
	htlc->access_extra.banner_get = 1;
}

extern u_int32_t htxf_ref_new (struct htlc_conn *htlc);
extern struct htxf_conn *htxf_new (struct htlc_conn *htlc, int put);

void
rcv_banner_get (struct htlc_conn *htlc)
{
	u_int16_t size16;
	u_int32_t ref, size;
	struct stat sb;
	int siz;
	struct SOCKADDR_IN lsaddr;
	struct htxf_conn *htxf;
	unsigned int i;

	for (i = 0; i < HTXF_GET_MAX; i++)
		if (!htlc->htxf_out[i])
			break;
	if (i == HTXF_GET_MAX) {
		snd_strerror(htlc, EAGAIN);
		return;
	}
	htlc->access_extra.banner_get = 0;
	if (!hxd_cfg.banner.file[0])
		return;

	if (stat(hxd_cfg.banner.file, &sb)) {
		snd_strerror(htlc, errno);
		return;
	}
	siz = sizeof(struct SOCKADDR_IN);
	if (getsockname(htlc->fd, (struct sockaddr *)&lsaddr, &siz)) {
		hxd_log("rcv_file_get: getsockname: %s", strerror(errno));
		snd_strerror(htlc, errno);
		return;
	}
	ref = htxf_ref_new(htlc);
	ref = htonl(ref);
	size = sb.st_size;
	size16 = htons((u_int16_t)size);
	hlwrite(htlc, HTLS_HDR_TASK, 0, 2,
		HTLS_DATA_HTXF_SIZE, sizeof(size16), &size16,
		HTLS_DATA_HTXF_REF, sizeof(ref), &ref);

	htxf = htxf_new(htlc, 0);
	htxf->type = HTXF_TYPE_BANNER;
	htxf->data_size = size;
	htxf->ref = ref;
	htxf->preview = 1;
	htxf->sockaddr = htlc->sockaddr;
	htxf->listen_sockaddr = lsaddr;
	htxf->listen_sockaddr.SIN_PORT = htons(ntohs(htxf->listen_sockaddr.SIN_PORT) + 1);
	strlcpy(htxf->path, hxd_cfg.banner.file, sizeof(htxf->path));
	htlc->nr_gets++;
	nr_gets++;
}

void
rcv_ping (struct htlc_conn *htlc)
{
	/* pong */
	hlwrite(htlc, HTLS_HDR_TASK, 0, 0);
}

void user_kick (struct htlc_conn *htlcp, time_t ban, struct htlc_conn *htlc,
		const char *fmt, ...);

void
rcv_hdr (struct htlc_conn *htlc)
{
	struct hl_hdr *h = (struct hl_hdr *)htlc->in.buf;
	u_int32_t type, len;

	type = ntohl(h->type);
	len = ntohl(h->len);
	if (len < 2)
		len = 0;
	else
		len = (len > MAX_HOTLINE_PACKET_LEN ? MAX_HOTLINE_PACKET_LEN : len) - 2;
	htlc->replytrans = ntohl(h->trans);
	htlc->rcv = 0;
#ifdef CONFIG_CIPHER
	if ((type >> 24) & 0xff) {
		/*hxd_log("changing decode key %u %x", (type >> 24), type);*/
		cipher_change_decode_key(htlc, type);
		type &= 0xffffff;
	}
#endif

#if defined(CONFIG_NOSPAM)
	if (hxd_cfg.operation.nospam) {
		if (!htlc->access_extra.can_login) {
			spam_update(htlc, type);
			if (htlc->spam_points >= htlc->spam_max
				 && !htlc->access_extra.can_spam) {
				user_kick(htlc, hxd_cfg.options.ban_time, htlc, "spam_max exceeded: %u >= %u, last transaction: 0x%x", htlc->spam_points, htlc->spam_max, type);
				return;
			}
		}
	}
#endif
	/*
	 * Big switch for different packet types.
	 * Ordered according to expected usage.
	 */
	switch (type) {
		case HTLC_HDR_CHAT:
			if (htlc->access.send_chat)
				htlc->rcv = rcv_chat;
			break;
		case HTLC_HDR_MSG:
			/*
			 * Old clients cannot set the send messages bit
			 * when editing accounts. So we allow the extra access
			 * to override.
			 */
			if (htlc->access.send_msgs || htlc->access_extra.msg)
				htlc->rcv = rcv_msg;
			break;
		case HTLC_HDR_USER_CHANGE:
			if (htlc->access.use_any_name && !htlc->access_extra.name_lock)
				htlc->rcv = rcv_user_change;
			break;
		case HTLC_HDR_ICON_SET:
			htlc->rcv = rcv_icon_set;
			break;
		case HTLC_HDR_ICON_GET:
			htlc->flags.in_ping = 1;
			htlc->rcv = rcv_icon_get;
			break;
		case HTLC_HDR_USER_GETINFO:
			/* if (htlc->access.get_user_info) */
			if (!htlc->access_extra.can_login)
				htlc->rcv = rcv_user_getinfo;
			break;
		case HTLC_HDR_USER_KICK:
			if (htlc->access.disconnect_users)
				htlc->rcv = rcv_user_kick;
			break;
		case HTLC_HDR_FILE_LIST:
			if (htlc->access_extra.file_list)
				htlc->rcv = rcv_file_list;
			break;
		case HTLC_HDR_FILE_GET:
			if (htlc->access.download_files)
				htlc->rcv = rcv_file_get;
			break;
		case HTLC_HDR_FILE_PUT:
			if (htlc->access.upload_files)
				htlc->rcv = rcv_file_put;
			break;
		case HTLC_HDR_KILLDOWNLOAD:
			//htlc->rcv = rcv_killdownload;
			break;
		case HTLC_HDR_FILE_GETINFO:
			if (htlc->access_extra.file_getinfo)
				htlc->rcv = rcv_file_getinfo;
			break;
		case HTLC_HDR_FILE_SETINFO:
			if (htlc->access.rename_files	||
				 htlc->access.rename_folders ||
				 htlc->access.comment_files  ||
				 htlc->access.comment_folders)
				htlc->rcv = rcv_file_setinfo;
			break;
		case HTLC_HDR_FILE_DELETE:
			if (htlc->access.delete_files ||
				 htlc->access.delete_folders)
				htlc->rcv = rcv_file_delete;
			break;
		case HTLC_HDR_FILE_MOVE:
			if (htlc->access.move_files ||
				 htlc->access.move_folders)
				htlc->rcv = rcv_file_move;
			break;
		case HTLC_HDR_FILE_MKDIR:
			if (htlc->access.create_folders)
				htlc->rcv = rcv_file_mkdir;
			break;
		case HTLC_HDR_FILE_SYMLINK:
			if (htlc->access.make_aliases)
				htlc->rcv = rcv_file_symlink;
			break;
#ifdef CONFIG_HOPE
		case HTLC_HDR_FILE_HASH:
			if (htlc->access_extra.file_hash)
				htlc->rcv = rcv_file_hash;
			break;
#endif
		case HTLC_HDR_CHAT_CREATE:
			if (htlc->access.create_pchats)
				htlc->rcv = rcv_chat_create;
			break;
		case HTLC_HDR_CHAT_INVITE:
			if (htlc->access_extra.chat_private)
				htlc->rcv = rcv_chat_invite;
			break;
		case HTLC_HDR_CHAT_JOIN:
			if (htlc->access_extra.chat_private)
				htlc->rcv = rcv_chat_join;
			break;
		case HTLC_HDR_CHAT_PART:
			if (htlc->access_extra.chat_private)
				htlc->rcv = rcv_chat_part;
			break;
		case HTLC_HDR_CHAT_DECLINE:
			if (htlc->access_extra.chat_private)
				htlc->rcv = rcv_chat_decline;
			break;
		case HTLC_HDR_CHAT_SUBJECT:
			if (htlc->access_extra.chat_private)
				htlc->rcv = rcv_chat_subject;
			break;
		case HTLC_HDR_NEWS_LISTCATEGORY:
			if (htlc->access.read_news)
				htlc->rcv = rcv_news_listcat;
			break;
		case HTLC_HDR_NEWS_LISTDIR:
			if (htlc->access.read_news)
				htlc->rcv = rcv_news_listdir;
			break;
		case HTLC_HDR_NEWS_GETTHREAD:
			if (htlc->access.read_news)
				htlc->rcv = rcv_news_get_thread;
			break;
		case HTLC_HDR_NEWS_POSTTHREAD:
			if (htlc->access.post_news)
				htlc->rcv = rcv_news_post_thread;
			break;
		case HTLC_HDR_NEWSFILE_POST:
			if (htlc->access.post_news)
				htlc->rcv = rcv_news_post;
			break;
		case HTLC_HDR_NEWS_DELETETHREAD:
			if (htlc->access.delete_articles)
				htlc->rcv = rcv_news_delete_thread;
			break;
		case HTLC_HDR_NEWS_DELETE:
			if (htlc->access.delete_news_bundles
				 || htlc->access.delete_categories)
				htlc->rcv = rcv_news_delete;
			break;
		case HTLC_HDR_NEWS_MKDIR:
			if (htlc->access.create_news_bundles)
				htlc->rcv = rcv_news_mkdir;
			break;
		case HTLC_HDR_NEWS_MKCATEGORY:
			if (htlc->access.create_categories)
				htlc->rcv = rcv_news_mkcategory;
			break;
		case HTLC_HDR_ACCOUNT_READ:
			if (htlc->access.read_users)
				htlc->rcv = rcv_account_read;
			break;
		case HTLC_HDR_ACCOUNT_MODIFY:
			if (htlc->access.modify_users)
				htlc->rcv = rcv_account_modify;
			break;
		case HTLC_HDR_ACCOUNT_CREATE:
			if (htlc->access.create_users)
				htlc->rcv = rcv_account_create;
			break;
		case HTLC_HDR_ACCOUNT_DELETE:
			if (htlc->access.delete_users)
				htlc->rcv = rcv_account_delete;
			break;
		case HTLC_HDR_MSG_BROADCAST:
			if (htlc->access.can_broadcast)
				htlc->rcv = rcv_msg_broadcast;
			break;
		case HTLC_HDR_USER_GETLIST:
			if (htlc->access_extra.user_getlist) {
				if (htlc->flags.is_hl || htlc->flags.is_heidrun)
					htlc->flags.in_ping = 1;
				htlc->rcv = rcv_user_getlist;
			}
			break;
		case HTLC_HDR_ICON_GETLIST:
			htlc->rcv = rcv_icon_getlist;
			break;
		case HTLC_HDR_NEWSFILE_GET:
			if (htlc->access.read_news)
				htlc->rcv = rcv_news_getfile;
			break;
		case HTLC_HDR_LOGIN:
			if (htlc->access_extra.can_login)
				htlc->rcv = rcv_login;
			break;
		case HTLC_HDR_AGREEMENTAGREE:
			if (htlc->access_extra.can_agree)
				htlc->rcv = rcv_agreementagree;
			break;
		case HTLC_HDR_BANNER_GET:
			if (htlc->access_extra.banner_get)
				htlc->rcv = rcv_banner_get;
			break;
		case HTLC_HDR_FILE_GETFOLDER:
			if (htlc->access.download_folders)
				htlc->rcv = rcv_folder_get;
			break;
		case HTLC_HDR_FILE_PUTFOLDER:
			if (htlc->access.upload_folders)
				htlc->rcv = rcv_folder_put;
			break;
		case HTLC_HDR_PING:
			if (htlc->access_extra.can_ping) {
				htlc->flags.in_ping=1;
				htlc->rcv = rcv_ping;
			}
			break;
		default:
			hxd_log("%s:%s:%u - unknown header type %x",
				htlc->name, htlc->login, htlc->uid, type);
			break;
	}
#if defined(CONFIG_HLDUMP)
	htlc->real_rcv = htlc->rcv;
	htlc->rcv = rcv_hldump;
#else
	if (!htlc->rcv)
		snd_errorstr(htlc, "Transaction rejected. (Unknown or non-authorised)");
#endif
	if (len) {
		qbuf_set(&htlc->in, htlc->in.pos, len);
	} else {
		int fd = htlc->fd;
		if (htlc->rcv)
			htlc->rcv(htlc);
		if (!hxd_files[fd].conn.htlc)
			return;
		test_away(htlc);
		htlc->rcv = rcv_hdr;
		qbuf_set(&htlc->in, 0, SIZEOF_HL_HDR);
	}
}

void chat_sendf (struct htlc_chat *chat, struct htlc_conn *htlc, const char *fmt, ...);

void
user_kick (struct htlc_conn *htlcp, time_t ban, struct htlc_conn *htlc,
		const char *fmt, ...)
{
	char abuf[HOSTLEN+1];
	char message[256];
	int len;
	va_list ap;

	if (fmt) {
		va_start(ap, fmt);
		len = vsnprintf(message, sizeof(message), fmt, ap);
		va_end(ap);
	} else {
		len = 0;
	}
	inaddr2str(abuf, &htlcp->sockaddr);
	hxd_log("%s@%s:%u - %s:%u:%u:%s %sed %s:%u:%u:%s - %s",
		htlc->userid, abuf, ntohs(htlc->sockaddr.SIN_PORT),
		htlc->name, htlc->icon, htlc->uid, htlc->login,
		ban?"bann":"kick",
		htlcp->name, htlcp->icon, htlcp->uid, htlcp->login, message);
	if (len)
			 chat_sendf(0, htlc, "\r<%s has been %sed by %s: %s>", htlcp->name, ban?"bann":"kick", htlc->name, message);
	else
		chat_sendf(0, htlc, "\r<%s has been %sed by %s>", htlcp->name, ban?"bann":"kick", htlc->name);

#if defined(CONFIG_SQL)
	if (ban)
		sql_user_ban(htlcp->name, abuf, htlcp->login, htlc->name, htlc->login);
	else
		sql_user_kick(htlcp->name, abuf, htlcp->login, htlc->name, htlc->login);
#endif
	if (ban) {
		char *name, *login, *userid;
#if 1
		name = "*";
		userid = "*";
		if (htlcp->login[0]) login = htlcp->login; else login = "*";
#else
		if (htlcp->name[0] && 0) name = htlcp->name; else name = "*";
		if (htlcp->login[0]) login = htlcp->login; else login = "*";
		if (htlcp->userid[0] && 0) userid = htlcp->userid; else userid = "*";
#endif
		addto_banlist(ban, name, login, userid, abuf, htlc->name);
	}
	htlc_flush_close(htlcp);
}

void
rcv_user_kick (struct htlc_conn *htlc)
{
	u_int32_t uid = 0, ban = 0;
	struct htlc_conn *htlcp;
	char buf[64];

	dh_start(htlc)
		if (dh_type == HTLC_DATA_BAN) {
			dh_getint(ban);
			continue;
		}
		if (dh_type != HTLC_DATA_UID)
			continue;
		dh_getint(uid);
	dh_end()
	if ((htlcp = isclient(uid))) {
		if (!htlcp->access.cant_be_disconnected) {
			user_kick(htlcp, ban ? hxd_cfg.options.ban_time : 0, htlc, "");
			if (htlcp != htlc)
				hlwrite(htlc, HTLS_HDR_TASK, 0, 0);
			return;
		} else {
			int len = snprintf(buf, sizeof(buf), "%s cannot be disconnected", htlcp->name);
			hlwrite(htlc, HTLS_HDR_TASK, 1, 1, HTLS_DATA_TASKERROR, len, buf);
		}
	} else {
		snd_errorstr(htlc, "Cannot kick the specified client because it does not exist.");
	}
}

static inline void
dirmask (char *dst, char *src, char *mask)
{
#if 0
	while (*mask && *src && *mask++ == *src++) ;
	strcpy(dst, src);
#else
	char *p = strrchr(src, '/');
	if (!p)
		p = src;
	else
		p++;
	strcpy(dst, p);
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

	
/*	if (d)
		snprintf(buf, 32, "%u day%s %u:%u:%u", d, d == 1 ? "" : "s", h, m, s);
	else
		snprintf(buf, 32, "%u:%u:%u", h, m, s);
		*/
	/* kang's sucky but nice display:p */
	if (d)
		snprintf(buf, 32, "%u day%s %u hr%s %u min%s %u sec%s", 
				d, d == 1 ? "" : "s",\
				h, h == 1 ? "" : "s",\
				m, m == 1 ? "" : "s",\
				s, s == 1 ? "" : "s");
	else if (h)
		snprintf(buf, 32, "%u hr%s %u min%s %u sec%s",
				h, h == 1 ? "" : "s",\
				m, m == 1 ? "" : "s",\
				s, s == 1 ? "" : "s");
	else if (m)
		snprintf(buf, 32, "%u min%s %u sec%s",
				m, m == 1 ? "" : "s",\
				s, s == 1 ? "" : "s");
	else if (s > 1)
		snprintf(buf, 32, "%u sec%s",
				s, s == 1 ? "" : "s");
	else
		snprintf(buf, 32, "none.");
}

#ifdef CONFIG_CIPHER
static char *cipher_names[] = {"none", "rc4", "blowfish", "idea"};
#endif
#ifdef CONFIG_COMPRESS
static char *compress_names[] = {"none", "gzip"};
#endif

void
rcv_user_getinfo (struct htlc_conn *htlc)
{
	u_int32_t uid = 0;
	struct htlc_conn *htlcp;
	u_int8_t infobuf[8192];
	char abuf[HOSTLEN+1];
	ssize_t len;
	struct htxf_conn *htxf;
	u_int8_t in_name[MAXPATHLEN], out_name[MAXPATHLEN];
	u_int16_t i;
	struct timeval now;
	float speed, elapsed;
	int seconds, minutes, hours, days;
	char *version;
	char tstr[32];
	time_t ts;

	dh_start(htlc)
		if (dh_type != HTLC_DATA_UID)
			continue;
		dh_getint(uid);
	dh_end()

	htlcp = isclient(uid);
	if (uid == 1 && !htlcp) {
		htlc->flags.is_frogblast = 1;
		htlc->flags.in_ping = 1;
	}
	test_away(htlc);

	if (!htlc->access.get_user_info && (!hxd_cfg.options.self_info || uid != htlc->uid)) {
		snd_errorstr(htlc, "You are not allowed to get user information");
		return;
	}

	if (!uid || !htlcp) {
		snd_errorstr(htlc, "Cannot get info for the specified client because it does not exist.");
		return;
	}

	gettimeofday(&now, 0);

	inaddr2str(abuf, &htlcp->sockaddr);
	len = snprintf(infobuf, sizeof(infobuf),
"    name: %s\r"
"   login: %s\r"
" address: %s:%u\r"
"    host: %s\r"
"  userid: %s\r",
			htlcp->name, htlcp->login, abuf, ntohs(htlcp->sockaddr.SIN_PORT),
			htlcp->host, htlcp->userid);

	if (htlcp->flags.is_hl)
		version = "1.5+";
	else if (htlcp->clientversion == 185)
		version = "1.8.5 compatible";
	else if (htlcp->flags.is_heidrun)
		version = "Heidrun";
	else if (htlcp->flags.is_frogblast)
		version = "Frogblast";
	else if (htlcp->flags.is_tide)
		version = "Panorama";
	else if (htlcp->flags.is_aniclient)
		version = "AniClient";
	else if (htlcp->flags.is_irc)
		version = "IRC Client";
	else if (htlcp->flags.is_kdx)
		version = "KDX Client";
	else
		version = "1.2.3 compatible";

	len += snprintf(infobuf+len, sizeof(infobuf)-len,
" version: %s [%u]\r",
			version, htlcp->clientversion);

	len += snprintf(infobuf+len, sizeof(infobuf)-len,
"     uid: %u\r"
"   color: %u\r"
"    icon: %u\r",
			htlcp->uid, htlcp->color, htlcp->icon);

#ifdef CONFIG_CIPHER
	len += snprintf(infobuf+len, sizeof(infobuf)-len,
"  cipher: %s/%s\r",
			cipher_names[htlcp->cipher_encode_type],
			cipher_names[htlcp->cipher_decode_type]);
#endif

#ifdef CONFIG_COMPRESS
	len += snprintf(infobuf+len, sizeof(infobuf)-len,
"compress: %s/%s\r",
			compress_names[htlcp->compress_encode_type],
			compress_names[htlcp->compress_decode_type]);
#endif

	ts = tv_secdiff(&htlcp->login_tv, &now);
	human_time(ts, tstr);
	len += snprintf(infobuf+len, sizeof(infobuf)-len,
"  online: %s\r",
			tstr);
	ts = tv_secdiff(&htlcp->idle_tv, &now);
	human_time(ts, tstr);
	len += snprintf(infobuf+len, sizeof(infobuf)-len, 
"    idle: %s\r",
			tstr);

	len += snprintf(infobuf+len, sizeof(infobuf)-len, "%s\r", hxd_cfg.strings.download_info);
	mutex_lock(&htlc->htxf_mutex);
	for (i = 0; i < HTXF_GET_MAX; i++) {
		char *spd_units = "B";
		char *pos_units = "k";
		char *size_units = pos_units;
		float tot_pos, tot_size;

		htxf = htlcp->htxf_out[i];
		if (!htxf)
			continue;
		dirmask(out_name, htxf->path, htlcp->rootdir);
#if defined(CONFIG_HTXF_QUEUE)
		if (htxf->queue_pos) {
			len += snprintf(infobuf+len, sizeof(infobuf)-len,
"%s\r"
"queued at position #%d\r",
					out_name, htxf->queue_pos);
			continue;
		}
#endif
		tot_pos = (float)(htxf->data_pos + htxf->rsrc_pos);
		tot_size = (float)(htxf->data_size + htxf->rsrc_size);
		elapsed = (float)(now.tv_sec - htxf->start.tv_sec);
		speed = (float)(htxf->total_pos / elapsed);
		seconds = (int)((tot_size - tot_pos) / speed);
		minutes = seconds / 60;
		seconds %= 60;
		hours = minutes / 60;
		minutes %= 60;
		days = hours / 24;
		hours %= 24;
		if (speed >= 1024.0){
			speed /= 1024.0;
			spd_units = "kB";
		}
		if (tot_pos >= 1048576.0) {
			tot_pos /= 1048576.0;
			pos_units = "M";
		} else
			tot_pos /= 1024.0;
		if (tot_size >= 1048576.0) {
			tot_size /= 1048576.0;
			size_units = "M";
		} else
			tot_size /= 1024.0;
		len += snprintf(infobuf+len, sizeof(infobuf)-len,
				"%s\r%.2f%sB of %.2f%sB, SPD: %.2f%s/s, ETA: ",
				out_name, tot_pos, pos_units, tot_size, size_units, speed, spd_units);
		if (days > 0)
			len += snprintf(infobuf+len, sizeof(infobuf)-len, 
					"%dd%d:%02d:%02ds\r", days, hours, minutes, seconds);
		else if (hours > 0)
			len += snprintf(infobuf+len, sizeof(infobuf)-len, 
					"%d:%02d:%02ds\r", hours, minutes, seconds);
		else if (minutes > 0)
			len += snprintf(infobuf+len, sizeof(infobuf)-len, 
					"%d:%02ds\r", minutes, seconds);
		else
			len += snprintf(infobuf+len, sizeof(infobuf)-len, 
					"%d seconds\r", seconds);
	}
	len += snprintf(infobuf+len, sizeof(infobuf)-len, "%s\r", hxd_cfg.strings.upload_info);
	for (i = 0; i < HTXF_PUT_MAX; i++) {
		char *spd_units = "B";
		char *pos_units = "k";
		char *size_units = pos_units;
		float tot_pos, tot_size;

		htxf = htlcp->htxf_in[i];
		if (!htxf)
			continue;
		dirmask(in_name, htxf->path, htlcp->rootdir);
		tot_pos = (float)(htxf->data_pos + htxf->rsrc_pos);
		tot_size = (float)(htxf->data_size + htxf->rsrc_size);
		elapsed = (float)(now.tv_sec - htxf->start.tv_sec);
		speed = (float)(htxf->total_pos / elapsed);
		seconds = (int)((tot_size - tot_pos) / speed);
		minutes = seconds / 60;
		seconds %= 60;
		hours = minutes / 60;
		minutes %= 60;
		days = hours / 24;
		hours %= 24;
		if (speed >= 1024.0){
			speed /= 1024.0;
			spd_units="kB";
		}
		if (tot_pos >= 1048576.0) {
			tot_pos /= 1048576.0;
			pos_units = "M";
		} else
			tot_pos /= 1024.0;
		if (tot_size >= 1048576.0) {
			tot_size /= 1048576.0;
			size_units = "M";
		} else
			tot_size /= 1024.0;
		len += snprintf(infobuf+len, sizeof(infobuf)-len,
				"%s\r%.2f%sB of %.2f%sB, SPD: %.2f%s/s, ETA: ",
				in_name, tot_pos, pos_units, tot_size, size_units, speed, spd_units);
		if (days > 0)
			len += snprintf(infobuf+len, sizeof(infobuf)-len,
					"%dd%d:%02d:%02ds\r", days, hours, minutes, seconds);
		else if (hours > 0)
			len += snprintf(infobuf+len, sizeof(infobuf)-len,
					"%d:%02d:%02ds\r", hours, minutes, seconds);
		else if (minutes > 0)
			len += snprintf(infobuf+len, sizeof(infobuf)-len,
					"%d:%02ds\r", minutes, seconds);
		else
			len += snprintf(infobuf+len, sizeof(infobuf)-len,
					"%d seconds\r", seconds);
	}
	mutex_unlock(&htlc->htxf_mutex);

	if (len < 0)
		len = sizeof(infobuf);
	len--;
	infobuf[len] = 0;
	hlwrite(htlc, HTLS_HDR_TASK, 0, 2,
		HTLS_DATA_USER_INFO, (u_int16_t)len, infobuf,
		HTLS_DATA_NAME, strlen(htlcp->name), htlcp->name);
}

void
rcv_news_getfile (struct htlc_conn *htlc)
{
	news_send_file(htlc);
}

void
rcv_news_post (struct htlc_conn *htlc)
{
	struct htlc_conn *htlcp;
	u_int8_t buf[0xffff];
	u_int16_t len;
	struct tm tm;
	time_t t;

	hlwrite(htlc, HTLS_HDR_TASK, 0, 0);
	t = time(0);
	localtime_r(&t, &tm);
	dh_start(htlc)
		if (dh_type != HTLC_DATA_NEWSFILE_POST)
			continue;
		len = snprintf(buf, sizeof(buf), hxd_cfg.strings.news_format, htlc->name, htlc->login);
		len += strftime(buf+len, sizeof(buf)-len, hxd_cfg.strings.news_time_format, &tm);
		len += snprintf(buf+len, sizeof(buf)-len, "\r\r%.*s\r%s\r",
			dh_len, dh_data, hxd_cfg.strings.news_divider);
		for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
			if (!htlcp->access.read_news)
				continue;
			if (strcmp(htlc->newsfile, htlcp->newsfile))
				continue;
			hlwrite(htlcp, HTLS_HDR_NEWSFILE_POST, 0, 1,
				HTLS_DATA_NEWS, len, buf);
		}
		CR2LF(buf, len);
		news_save_post(htlc->newsfile, buf, len);
	dh_end()
}

#if defined(CONFIG_HOPE)
static int
valid_macalg (const char *macalg)
{
	if (strcmp(macalg, "HMAC-SHA1") && strcmp(macalg, "HMAC-MD5")
		 && strcmp(macalg, "SHA1") && strcmp(macalg, "MD5"))
		return 0;
	else
		return 1;
}

#if defined(CONFIG_CIPHER)
static int
valid_cipher (const char *cipheralg)
{
	unsigned int i;

	for (i = 0; hxd_cfg.cipher.ciphers[i]; i++) {
		if (!strcmp(hxd_cfg.cipher.ciphers[i], cipheralg))
			return 1;
	}

	return 0;
}
#endif

#if defined(CONFIG_COMPRESS)
static int
valid_compress (const char *compressalg)
{
	if (!strcmp(compressalg, "GZIP"))
		return 1;

	return 0;
}
#endif

static u_int8_t *
list_n (u_int8_t *list, u_int16_t listlen, unsigned int n)
{
	unsigned int i;
	u_int16_t pos = 1;
	u_int8_t *p = list + 2;

	for (i = 0; ; i++) {
		if (pos + *p > listlen)
			return 0;
		if (i == n)
			return p;
		pos += *p+1;
		p += *p+1;
	}
}
#endif

extern char *resolve (struct SOCKADDR_IN *saddr);

static thread_return_type
resolve_htlc_address (void *__htlc)
{
	struct htlc_conn *htlc = (struct htlc_conn *)__htlc;
	char *hn;

	hn = resolve(&htlc->sockaddr);
	if (hn)
		snprintf(htlc->host, sizeof(htlc->host), "%s", hn);
	mutex_unlock(&htlc->htxf_mutex);

	return 0;
}

void
rcv_login (struct htlc_conn *htlc)
{
	u_int16_t plen = 0, llen = 0, nlen = 0, uid16;
	u_int16_t clientversion = 0, serverversion, bannerid;
	int agreement_sent, got_name = 0;
	u_int32_t icon = 0;
	char abuf[HOSTLEN+1];
	char login[32], name[32], given_name[32], password[32], given_password[32];
	int err;
	char *hn;
#if defined(CONFIG_HTXF_PTHREAD) || defined(CONFIG_HTXF_CLONE)
	hxd_thread_t tid;
	void *stack;
#endif
#ifdef CONFIG_HOPE
	u_int8_t given_password_mac[20], given_login_mac[20];
	u_int16_t password_mac_len = 0, login_mac_len = 0;
	u_int16_t macalglen = 0;
	u_int8_t *in_mal = 0;
	u_int16_t in_mal_len = 0;
	u_int8_t *in_checksum_al = 0;
	u_int16_t in_checksum_al_len = 0;
#ifdef CONFIG_CIPHER
	u_int8_t *in_cipher_al = 0;
	u_int16_t in_cipher_al_len = 0;
	u_int8_t cipheralg[32];
	u_int16_t cipheralglen = 0;
	u_int8_t *in_ciphermode_l = 0;
	u_int16_t in_ciphermode_l_len = 0;
	u_int8_t *ivec;
	u_int16_t ivec_len;
#endif
#ifdef CONFIG_COMPRESS
	u_int8_t *in_compress_al = 0;
	u_int16_t in_compress_al_len = 0;
	u_int8_t compressalg[32];
	u_int16_t compressalglen = 0;
#endif
#endif

	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_NAME:
				nlen = dh_len > 31 ? 31 : dh_len;
				memcpy(given_name, dh_data, nlen);
				given_name[nlen] = 0;
				got_name = 1;
				break;
			case HTLC_DATA_ICON:
				dh_getint(icon);
				break;
			case HTLC_DATA_CLIENTVERSION:
				dh_getint(clientversion);
				break;
			case HTLC_DATA_LOGIN:
				llen = dh_len > 31 ? 31 : dh_len;
				if (llen == 1 && !dh_data[0]) {
					login[0] = 0;
					break;
				} else
#ifdef CONFIG_HOPE
				if (htlc->macalg[0]) {
					if (llen > 20)
						break;
					login_mac_len = llen;
					memcpy(given_login_mac, dh_data, login_mac_len);
				} else
#endif
					hl_decode(login, dh_data, llen);
				login[llen] = 0;
				break;
			case HTLC_DATA_PASSWORD:
				plen = dh_len > 31 ? 31 : dh_len;
#ifdef CONFIG_HOPE
				if (htlc->macalg[0]) {
					if (plen > 20)
						break;
					password_mac_len = plen;
					memcpy(given_password_mac, dh_data, password_mac_len);
					break;
				}
#endif
				hl_decode(given_password, dh_data, plen);
				given_password[plen] = 0;
				break;
#ifdef CONFIG_HOPE
			case HTLC_DATA_MAC_ALG:
				in_mal = dh_data;
				in_mal_len = dh_len;
				break;
#ifdef CONFIG_CIPHER
			case HTLC_DATA_CIPHER_ALG:
			case HTLS_DATA_CIPHER_ALG:
				in_cipher_al = dh_data;
				in_cipher_al_len = dh_len;
				break;
			case HTLC_DATA_CIPHER_IVEC:
			case HTLS_DATA_CIPHER_IVEC:
				ivec = dh_data;
				ivec_len = dh_len;
				break;
			case HTLC_DATA_CIPHER_MODE:
			case HTLS_DATA_CIPHER_MODE:
				in_ciphermode_l = dh_data;
				in_ciphermode_l_len = dh_len;
				break;
#endif
#ifdef CONFIG_COMPRESS
			case HTLC_DATA_COMPRESS_ALG:
			case HTLS_DATA_COMPRESS_ALG:
				in_compress_al = dh_data;
				in_compress_al_len = dh_len;
				break;
#endif
			case HTLC_DATA_CHECKSUM_ALG:
			case HTLS_DATA_CHECKSUM_ALG:
				in_checksum_al = dh_data;
				in_checksum_al_len = dh_len;
				break;
#endif
		}
	dh_end()

	if (llen == 1 && !login[0]) {
#ifdef CONFIG_HOPE
#ifdef CONFIG_CIPHER
		u_int8_t cipheralglist[64];
		u_int16_t cipheralglistlen;
#endif
#ifdef CONFIG_COMPRESS
		u_int8_t compressalglist[64];
		u_int16_t compressalglistlen;
#endif
		u_int16_t hc;
		u_int8_t macalglist[64];
		u_int16_t macalglistlen;
		u_int8_t sessionkey[64];
		struct SOCKADDR_IN saddr;
		int x;

#if defined(CONFIG_IPV6)
		if (random_bytes(sessionkey+18, 46) != 46) {
#else
		if (random_bytes(sessionkey+6, 58) != 58) {
#endif
			hlwrite(htlc, HTLS_HDR_TASK, 1, 0);
			htlc_close(htlc);
			return;
		}

		x = sizeof(saddr);
		if (getsockname(htlc->fd, (struct sockaddr *)&saddr, &x)) {
			hxd_log("login: getsockname: %s", strerror(errno));
			saddr.SIN_ADDR.s_addr = 0;
		}
		/* store in network order */
#if defined(CONFIG_IPV6)
		memcpy(sessionkey, &saddr.SIN_ADDR.s6_addr, 16);
		*((u_int16_t *)(sessionkey + 16)) = saddr.SIN_PORT;
#else
		*((u_int32_t *)sessionkey) = saddr.SIN_ADDR.s_addr;
		*((u_int16_t *)(sessionkey + 4)) = saddr.SIN_PORT;
#endif

		if (in_mal_len) {
			unsigned int i, len;
			u_int8_t *map, ma[32];

			for (i = 0; ; i++) {
				map = list_n(in_mal, in_mal_len, i);
				if (!map)
					break;
				len = *map >= sizeof(ma) ? sizeof(ma)-1 : *map;
				memcpy(ma, map+1, len);
				ma[len] = 0;
				if (valid_macalg(ma)) {
					macalglen = len;
					strcpy(htlc->macalg, ma);
					break;
				}
			}
		}

		if (!macalglen) {
			macalglen = 0;
			macalglistlen = 0;
		} else {
			macalglist[0] = 0;
			macalglist[1] = 1;
			macalglist[2] = macalglen;
			memcpy(macalglist+3, htlc->macalg, macalglist[2]);
			macalglistlen = 3 + macalglen;
		}
		hc = 3;
#ifdef CONFIG_COMPRESS
		if (in_compress_al_len) {
			unsigned int i, len;
			u_int8_t *cap, ca[32];

			for (i = 0; ; i++) {
				cap = list_n(in_compress_al, in_compress_al_len, i);
				if (!cap)
					break;
				len = *cap > sizeof(ca)-1 ? sizeof(ca)-1 : *cap;
				memcpy(ca, cap+1, len);
				ca[len] = 0;
				if (valid_compress(ca)) {
					compressalglen = len;
					strcpy(compressalg, ca);
					break;
				}
			}
		}
		if (!compressalglen) {
			compressalglistlen = 0;
		} else {
			compressalglist[0] = 0;
			compressalglist[1] = 1;
			compressalglist[2] = compressalglen;
			memcpy(compressalglist+3, compressalg, compressalglist[2]);
			compressalglistlen = 3 + compressalglen;
		}
		hc += 2;
#endif /* compress */
#ifdef CONFIG_CIPHER
		cipheralg[0] = 0;
		if (in_cipher_al_len) {
			unsigned int i, len;
			u_int8_t *cap, ca[32];

			for (i = 0; ; i++) {
				cap = list_n(in_cipher_al, in_cipher_al_len, i);
				if (!cap)
					break;
				len = *cap > sizeof(ca)-1 ? sizeof(ca)-1 : *cap;
				memcpy(ca, cap+1, len);
				ca[len] = 0;
				if (valid_cipher(ca)) {
					cipheralglen = len;
					strcpy(cipheralg, ca);
					break;
				}
			}
		}
		if (!cipheralglen || !strcmp(cipheralg, "NONE")) {
			cipheralglistlen = 0;
			if (hxd_cfg.cipher.cipher_only) {
				hlwrite(htlc, HTLS_HDR_TASK, 1, 0);
				htlc_flush_close(htlc);
				return;
			}
		} else {
			cipheralglist[0] = 0;
			cipheralglist[1] = 1;
			cipheralglist[2] = cipheralglen;
			memcpy(cipheralglist+3, cipheralg, cipheralglist[2]);
			cipheralglistlen = 3 + cipheralglen;
		}
		hc += 2;
#endif /* cipher */
		hlwrite(htlc, HTLS_HDR_TASK, 0, hc,
			HTLS_DATA_LOGIN, macalglen, htlc->macalg,
			HTLS_DATA_MAC_ALG, macalglistlen, macalglist,
#ifdef CONFIG_CIPHER
			HTLS_DATA_CIPHER_ALG, cipheralglistlen, cipheralglist,
			HTLC_DATA_CIPHER_ALG, cipheralglistlen, cipheralglist,
#endif
#ifdef CONFIG_COMPRESS
			HTLS_DATA_COMPRESS_ALG, compressalglistlen, compressalglist,
			HTLC_DATA_COMPRESS_ALG, compressalglistlen, compressalglist,
#endif
			HTLS_DATA_SESSIONKEY, 64, sessionkey);

		memcpy(htlc->sessionkey, sessionkey, 64);
		htlc->sklen = 64;
		return;
#else /* if not HOPE */
		hlwrite(htlc, HTLS_HDR_TASK, 1, 0);
		return;
#endif
	}
	inaddr2str(abuf, &htlc->sockaddr);

#ifdef CONFIG_HOPE
	if (login_mac_len) {
		DIR *dir;
		struct dirent *de;
		u_int8_t real_login_mac[20];
		int len;

		len = hmac_xxx(real_login_mac, "", 0, htlc->sessionkey, htlc->sklen, htlc->macalg);
		if (len && !memcmp(real_login_mac, given_login_mac, len)) {
			llen = 0;
			goto guest_login;
		}
		dir = opendir(hxd_cfg.paths.accounts);
		if (!dir) {
			hlwrite(htlc, HTLS_HDR_TASK, 1, 0);
			return;
		}
		while ((de = readdir(dir))) {
			len = hmac_xxx(real_login_mac, de->d_name, strlen(de->d_name), htlc->sessionkey, htlc->sklen, htlc->macalg);
			if (len && !memcmp(real_login_mac, given_login_mac, len)) {
				llen = strlen(de->d_name) > 31 ? 31 : strlen(de->d_name);
				memcpy(login, de->d_name, llen);
				login[llen] = 0;
				break;
			}
		}
		closedir(dir);
		if (!de) {
			hxd_log("%s@%s:%u -- could not find login", htlc->userid, abuf, ntohs(htlc->sockaddr.SIN_PORT));
			htlc_close(htlc);
			return;
		}
	} else {
		if (hxd_cfg.cipher.cipher_only) {
			htlc_close(htlc);
			return;
		}
	}
guest_login:
#endif /* hope */

	if (!llen) {
		strcpy(login, "guest");
	}

	if (!account_trusted(login, htlc->userid, abuf)) {
		hxd_log("%s@%s:%u -- not trusted with login %s", htlc->userid, abuf, ntohs(htlc->sockaddr.SIN_PORT), login);
		htlc_close(htlc);
		return;
	}
	if ((err = account_read(login, password, name, &htlc->access))) {
		hxd_log("%s@%s:%u -- error reading account %s: %s", htlc->userid, abuf, ntohs(htlc->sockaddr.SIN_PORT), login, strerror(err));
		htlc_close(htlc);
		return;
	}
#ifdef CONFIG_HOPE
	if (password_mac_len) {
		u_int8_t real_password_mac[20];
		unsigned int maclen;

		maclen = hmac_xxx(real_password_mac, password, strlen(password),
					 htlc->sessionkey, htlc->sklen, htlc->macalg);
		if (!maclen || memcmp(real_password_mac, given_password_mac, maclen)) {
			hxd_log("%s@%s:%u -- wrong password for %s",
				htlc->userid, abuf, ntohs(htlc->sockaddr.SIN_PORT), login);
			htlc_close(htlc);
			return;
		}
#ifdef CONFIG_COMPRESS
		if (in_compress_al_len) {
			htlc->compress_encode_type = COMPRESS_GZIP;
			compress_encode_init(htlc);
			htlc->compress_decode_type = COMPRESS_GZIP;
			compress_decode_init(htlc);
		}
#endif
#ifdef CONFIG_CIPHER
		if (in_cipher_al_len) {
			unsigned int i, len;
			u_int8_t *cap, ca[32];

			for (i = 0; ; i++) {
				cap = list_n(in_cipher_al, in_cipher_al_len, i);
				if (!cap)
					break;
				len = *cap >= sizeof(ca) ? sizeof(ca)-1 : *cap;
				memcpy(ca, cap+1, len);
				ca[len] = 0;
				if (valid_cipher(ca)) {
					cipheralglen = len;
					strcpy(cipheralg, ca);
					break;
				}
			}
			if (!cipheralglen) {
				hxd_log("%s@%s:%u - %s - bad cipher alg",
					htlc->userid, abuf, ntohs(htlc->sockaddr.SIN_PORT), login);
				htlc_close(htlc);
				return;
			}
			/* in server, encode key is first */
			maclen = hmac_xxx(htlc->cipher_encode_key, password, strlen(password),
						 real_password_mac, maclen, htlc->macalg);
			htlc->cipher_encode_keylen = maclen;
			maclen = hmac_xxx(htlc->cipher_decode_key, password, strlen(password),
						 htlc->cipher_encode_key, maclen, htlc->macalg);
			htlc->cipher_decode_keylen = maclen;
			if (!strcmp(cipheralg, "RC4")) {
				htlc->cipher_encode_type = CIPHER_RC4;
				htlc->cipher_decode_type = CIPHER_RC4;
			} else if (!strcmp(cipheralg, "BLOWFISH")) {
				htlc->cipher_encode_type = CIPHER_BLOWFISH;
				htlc->cipher_decode_type = CIPHER_BLOWFISH;
			} else if (!strcmp(cipheralg, "IDEA")) {
				htlc->cipher_encode_type = CIPHER_IDEA;
				htlc->cipher_decode_type = CIPHER_IDEA;
			}
			cipher_encode_init(htlc);
			cipher_decode_init(htlc);
		}
#endif /* cipher */
	} else {
#endif /* hope */
		if (strlen(password) != plen || (plen && strcmp(password, given_password))) {
			hxd_log("%s@%s:%u -- wrong password for %s", htlc->userid, abuf,
				ntohs(htlc->sockaddr.SIN_PORT), login);
			htlc_close(htlc);
			return;
		}
#ifdef CONFIG_HOPE
	}
#endif

	if (htlc->access.use_any_name && nlen) {
		htlc_set_name(htlc, given_name);
	} else {
		nlen = strlen(name);
		htlc_set_name(htlc, name);
	}
	strcpy(htlc->login, login);
	htlc->clientversion = clientversion;
	htlc->serverversion = hxd_cfg.options.version;
	if (check_banlist(htlc))
		return;

	/* user has successfully logged in */
	/* resolve the ip address and store the hostname */
#if defined(CONFIG_HTXF_PTHREAD) || defined(CONFIG_HTXF_CLONE)
	/* XXX prevent htlc_close from freeing htlc */
	mutex_lock(&htlc->htxf_mutex);
	err = hxd_thread_create(&tid, &stack, resolve_htlc_address, htlc);
	if (err) {
		mutex_unlock(&htlc->htxf_mutex);
#if !defined(__WIN32__)
		alarm(10);
#endif
		snprintf(htlc->host, sizeof(htlc->host), "%s", resolve(&htlc->sockaddr));
		hn = resolve(&htlc->sockaddr);
		if (hn)
			snprintf(htlc->host, sizeof(htlc->host), "%s", hn);
	} else {
#if defined(CONFIG_HTXF_PTHREAD)
		pthread_detach(tid);
#endif
	}
#else
	alarm(10);
	hn = resolve(&htlc->sockaddr);
	if (hn)
		snprintf(htlc->host, sizeof(htlc->host), "%s", hn);
#endif

	htlc->icon = (u_int16_t)icon;
	htlc->uid = uid_assign(htlc);

	account_get_access_extra(htlc);
	/* can_login should be 0, but just in case ... */
	htlc->access_extra.can_login = 0;
	
	if (htlc->access.disconnect_users)
		htlc->color = 2;
	else
		htlc->color = 0;

	/* determine what kind of client we have here */
	if (!got_name) {
		/* hlcomm logs in with no name and no icon */
		htlc->flags.is_hl = 1;
	} else if (htlc->icon == 42) {
		/* panorama logs in with name and icon 42 */
		htlc->flags.is_tide = 1;
		htlc_set_name(htlc, given_name);
	}

	hxd_log("%s@%s:%u - %s:%u:%u:%s - name:icon:uid:login",
		htlc->userid, abuf, ntohs(htlc->sockaddr.SIN_PORT),
		htlc->name, htlc->icon, htlc->uid, htlc->login);

#if defined(CONFIG_SQL)
	sql_add_user(htlc->userid, htlc->name, abuf, ntohs(htlc->sockaddr.SIN_PORT),
			  htlc->login, htlc->uid, htlc->icon, htlc->color);
#endif

	account_getconf(htlc);
	serverversion = htons(hxd_cfg.options.version);
	bannerid = 0;
	uid16 = htons(htlc->uid);
	if (hxd_cfg.options.version == 0) {
		/* Do not send server version/queue/name */
		hlwrite(htlc, HTLS_HDR_TASK, 0, 1,
			HTLS_DATA_UID, 2, &uid16);
	} else {
#if 1
		hlwrite(htlc, HTLS_HDR_TASK, 0, 4,
			HTLS_DATA_UID, 2, &uid16,
			HTLS_DATA_SERVERVERSION, 2, &serverversion,
			HTLS_DATA_BANNERID, 2, &bannerid,
			HTLS_DATA_SERVERNAME, strlen(hxd_cfg.tracker.name), hxd_cfg.tracker.name);
#else
		hlwrite(htlc, HTLS_HDR_TASK, 0, 2,
			HTLS_DATA_SERVERVERSION, 2, &serverversion,
			HTLS_DATA_BANNERID, 2, &bannerid);
#endif
	}

	agreement_sent = 0;
	if (!htlc->access.dont_show_agreement) {
		if (!agreement_send_file(htlc))
			agreement_sent = 1;
	}
	if (clientversion >= 150) {
		if (!agreement_sent)
			hlwrite(htlc, HTLS_HDR_AGREEMENT, 0, 1,
				HTLS_DATA_NOAGREEMENT, 2, "\0\1");
		if (got_name) {
			htlc->flags.is_heidrun = 1;
		} else {
			/*
			 * Zero access to make sure they send a name
			 * in rcv_agreementagree or rcv_user_change
			 */
			memset(&htlc->access, 0, sizeof(htlc->access));
			memset(&htlc->access_extra, 0, sizeof(htlc->access_extra));
			htlc->access_extra.can_agree = 1;
			htlc->access.use_any_name = 1;
			htlc->flags.in_login = 1;
		}
		htlc->access_extra.can_ping=1;
	}
	if (clientversion < 150 || got_name)
		user_loginupdate(htlc);
#ifdef CONFIG_TRACKER_REGISTER
	/* update user count */
	if (hxd_cfg.operation.tracker_register)
		tracker_register_timer(0);
#endif
}

static char *
cr_lf_strchr (char *ptr)
{
	register char *p;

	for (p = ptr; *p; p++)
		if (*p == '\r' || *p == '\n')
			return p;

	return 0;
}

static u_int8_t *
cr_strtok_r (u_int8_t *ptr, u_int8_t **state_ptr)
{
	u_int8_t *p, *ret;

	if (ptr) {
		if (!(p = cr_lf_strchr(ptr)))
			return 0;
	} else if (!*state_ptr) {
		return 0;
	} else if (!(p = cr_lf_strchr(*state_ptr))) {
		p = *state_ptr;
		*state_ptr = 0;
		return p;
	}
	*p = 0;
	ret = *state_ptr ? *state_ptr : ptr;
	*state_ptr = &(p[1]);

	return ret;
}

void
chat_sendf (struct htlc_chat *chat, struct htlc_conn *htlc, const char *fmt, ...)
{
	char buf[4096];
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	snd_chat(chat, htlc, buf, len);
}

#define MAX_CHAT 65536
u_int8_t big_chatbuf[MAX_CHAT];

void
hxd_rcv_chat (struct htlc_conn *htlc, struct htlc_chat *chat,
			u_int16_t style, u_int16_t away, u_int8_t *chatbuf, u_int16_t len)
{
	u_int8_t *fmt = hxd_cfg.strings.chat_format;
	u_int8_t *buf = big_chatbuf;
	u_int8_t *p, *last = 0;
	int r;

	if (style == 1)
		fmt = hxd_cfg.strings.chat_opt_format;
	if (away && htlc->flags.away != AWAY_INTERRUPTED) {
		toggle_away(htlc);
		if (!htlc->flags.away)
			htlc->flags.away = AWAY_PERM;
		else
			htlc->flags.away = 0;
		if (hxd_cfg.options.away_time) {
			timer_delete_ptr(htlc);
			if (!htlc->flags.away)
				timer_add_secs(hxd_cfg.options.away_time, away_timer, htlc);
		}
	}
	if (hxd_cfg.operation.commands) {
		if (chatbuf[0] == '/') {
			command_chat(htlc, chat ? chat->ref : 0, &chatbuf[1]);
			return;
		}
	}

#if defined(CONFIG_NOSPAM)
	if ((loopZ_timeval.tv_sec - htlc->spam_chat_time) >= htlc->spam_chat_time_limit) {
		htlc->spam_chat_lines = 0;
		htlc->spam_chat_time = loopZ_timeval.tv_sec;
	}
#endif
	if (!(p = cr_strtok_r(chatbuf, &last)))
		p = chatbuf;
	len = 0;
	do {
#if defined(CONFIG_NOSPAM)
		if (hxd_cfg.operation.nospam) {
			if (!htlc->access_extra.can_spam
				 && htlc->spam_chat_lines >= htlc->spam_chat_max) {
				len = sprintf(buf, "\r *** %s was kicked for chat spamming", htlc->name);
				htlc_close(htlc);
				break;
			}
		}
		htlc->spam_chat_lines++;
#endif
		r = snprintf(buf+len, MAX_CHAT-len, fmt, htlc->name, p);
		if (r == -1) {
			r = MAX_CHAT-len;
			len += r;
			break;
		}
		len += r;
	} while ((p = cr_strtok_r(0, &last)));

	snd_chat(chat, htlc, buf, len);
}

void
rcv_chat (struct htlc_conn *htlc)
{
	u_int32_t chatref = 0, away = 0, style = 0;
	struct htlc_chat *chat = 0;
	u_int16_t len = 0;
	u_int8_t chatbuf[4096];

	dh_start(htlc)
		switch (dh_type) {
		case HTLC_DATA_STYLE:
			dh_getint(style);
			break;
		case HTLC_DATA_CHAT_ID:
			dh_getint(chatref);
			break;
		case HTLC_DATA_CHAT:
			len = dh_len > 2048 ? 2048 : dh_len;
			memcpy(chatbuf, dh_data, len);
			break;
		case HTLC_DATA_CHAT_AWAY:	
			dh_getint(away);
			break;

		}
	dh_end()

	if (chatref) {
		chat = chat_lookup_ref(chatref);
		if (!chat || !chat_isset(htlc, chat, 0))
			return;
	}
	chatbuf[len] = 0;

	hxd_rcv_chat(htlc, chat, style, away, chatbuf, len);
}

void
hxd_rcv_msg (struct htlc_conn *htlc, struct htlc_conn *htlcp, u_int8_t *msg, u_int16_t msglen)
{
#if defined(CONFIG_NOSPAM)
	if (hxd_cfg.operation.nospam && !strcmp(htlc->login, "guest")) {
		if (check_messagebot(msg)) {
#if defined(CONFIG_SQL)
			char abuf[HOSTLEN+1];
			inaddr2str(abuf, &htlc->sockaddr);
			sql_user_kick("messagebot", abuf, "guest", "hxd", "hxd");
#endif
			htlc_close(htlc);
			return;
		}
	}
#endif

	snd_msg(htlc, htlcp, msg, msglen);
}

void
rcv_msg (struct htlc_conn *htlc)
{
	u_int32_t uid = 0;
	u_int16_t msglen = 0;
	u_int8_t *msg = 0;
	struct htlc_conn *htlcp;

	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_UID:
				dh_getint(uid);
				break;
			case HTLC_DATA_MSG:
				msglen = dh_len;
				msg = dh_data;
				break;
		}
	dh_end()

	if (!msg)
		return;
	msg[msglen] = 0;

	if (uid && msglen) {
		htlcp = isclient(uid);
		if (htlcp)
			hxd_rcv_msg(htlc, htlcp, msg, msglen);
		else
			snd_errorstr(htlc, "Cannot message the specified client because it does not exist.");
	} else {
		snd_errorstr(htlc, "You provided an empty message or the client you are trying to message does not exist.");
	}
}

void
rcv_msg_broadcast (struct htlc_conn *htlc)
{
	u_int16_t msglen = 0, uid;
	u_int8_t msgbuf[4096];
	struct htlc_conn *htlcp;

	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_MSG:
				msglen = dh_len > sizeof(msgbuf) ? sizeof(msgbuf) : dh_len;
				memcpy(msgbuf, dh_data, msglen);
				break;
		}
	dh_end()
	if (!msglen)
		return;
	uid = htons(mangle_uid(htlc));
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (htlcp->access_extra.can_login)
			continue;
		hlwrite(htlcp, HTLS_HDR_MSG_BROADCAST, 0, 3,
			HTLS_DATA_UID, sizeof(uid), &uid,
			HTLS_DATA_MSG, msglen, msgbuf,
			HTLS_DATA_NAME, strlen(htlc->name), htlc->name);
	}
	/*
	 * TODO: send task complete message after completing write to others?
	 * Allow different task completion models that can be requested by
	 * the client?
	 * 1. Report success if no errors were found in request packet.
	 * 2. Report success when data has been written to all clients.
	 * 3. Report partial success when data has been written to each client.
	 * 4. More detail?
	 */
	hlwrite(htlc, HTLS_HDR_TASK, 0, 0);
}

void
rcv_icon_get (struct htlc_conn *htlc)
{
	u_int16_t uid = 0;
	u_int16_t out_uid = 0;
	struct htlc_conn *htlcp;
	
	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_UID:
				dh_getint(uid);
				out_uid = htons(uid);
				break;
		}
	dh_end()

	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if(uid == htlcp->uid) {
			hlwrite(htlc, HTLS_HDR_TASK, 0, 2,
				HTLS_DATA_ICON_GIF, htlcp->icon_gif_len, htlcp->icon_gif,
				HTLS_DATA_UID, sizeof(out_uid), &out_uid);
			return;
		}
	}

}

void
rcv_icon_set (struct htlc_conn *htlc)
{
	struct htlc_conn *htlcp;
	u_int16_t uid = 0;
	
	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_ICON_GIF:
				htlc->icon_gif = (u_int8_t *)malloc( dh_len );
				htlc->icon_gif_len = dh_len;
				memcpy(htlc->icon_gif, dh_data, dh_len);
				break;
		}
	dh_end()
	hlwrite(htlc, HTLS_HDR_TASK, 0, 0);

	uid = htons(mangle_uid(htlc));

	/* tell everyone that this guy changed icon*/
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		hlwrite(htlcp, HTLS_HDR_ICON_CHANGE, 0, 1,
			HTLC_DATA_UID, sizeof(uid), &uid);
	}
}

void
rcv_icon_getlist (struct htlc_conn *htlc)
{
	struct htlc_conn *htlcp;
	u_int32_t offset = 0;
	u_int16_t uid = 0, len = 0;
	u_int8_t *buf;

	offset = hlwrite_hdr( htlc, HTLS_HDR_TASK, 0 );
	
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		uid = htons(htlcp->uid);	
		len = htons( htlcp->icon_gif_len);
		buf = (u_int8_t *)xmalloc(4 + htlcp->icon_gif_len);

		memcpy(buf, &uid, 2);
		memcpy(buf+2, &len, 2);
		memcpy(buf+4, htlcp->icon_gif, htlcp->icon_gif_len);
		hlwrite_data(htlc, offset, HTLS_DATA_ICON_LIST, 4+htlcp->icon_gif_len, buf);
		xfree(buf);
	}
	hlwrite_end(htlc, offset);
}

void
rcv_user_change (struct htlc_conn *htlc)
{
	int diff = 0;
	u_int16_t nlen = 0;
	u_int32_t icon = 0;
	u_int8_t given_name[32];
	
	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_NAME:
				nlen = dh_len > 31 ? 31 : dh_len;
				if (strlen(htlc->name) != nlen || memcmp(dh_data, htlc->name, nlen))
					diff = 1;
				memcpy(given_name, dh_data, nlen);
				given_name[nlen] = 0;
				break;
			case HTLC_DATA_ICON:
				dh_getint(icon);
				if (htlc->icon != icon) {
					diff = 1;
					htlc->icon = (u_int16_t)icon;
				}
				break;
		}
	dh_end()

	if (!diff)
		return;

	htlc_set_name(htlc, given_name);

	if (htlc->flags.in_login) {
		finish_login(htlc);
		if (check_banlist(htlc))
			return;
		user_loginupdate(htlc);
		return;
	}

	if (check_banlist(htlc))
		return;

#if defined(CONFIG_SQL)
	sql_modify_user(htlc->name, htlc->icon, htlc->color, htlc->uid);
#endif

	if (!htlc->flags.visible)
		return;

	snd_user_change(htlc);
}

void
rcv_user_getlist (struct htlc_conn *htlc)
{
	struct hl_userlist_hdr *uh;
	u_int8_t uhbuf[sizeof(struct hl_userlist_hdr) + 32];
	u_int16_t nlen, nclients = 0;
	struct htlc_conn *htlcp;
	struct htlc_chat *chat;
	u_int16_t slen;
	u_int32_t hoff;

	hoff = hlwrite_hdr(htlc, HTLS_HDR_TASK, 0);
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (!htlcp->flags.visible)
			continue;
		nlen = strlen(htlcp->name);
		uh = (struct hl_userlist_hdr *)uhbuf;
		uh->uid = htons(htlcp->uid);
		uh->icon = htons(htlcp->icon);
		uh->color = htons(htlcp->color);
		uh->nlen = htons(nlen);
		memcpy(uh->name, htlcp->name, nlen);
		hlwrite_data(htlc, hoff, HTLS_DATA_USER_LIST, 8+nlen, uhbuf+SIZEOF_HL_DATA_HDR);
		nclients++;
	}
	chat = chat_lookup_ref(0);
	slen = strlen(chat->subject);
	hlwrite_data(htlc, hoff, HTLS_DATA_CHAT_SUBJECT, slen, chat->subject);
	hlwrite_end(htlc, hoff);
}
