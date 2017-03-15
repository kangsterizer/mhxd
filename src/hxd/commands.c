#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/param.h>
#include "hlserver.h"
#include "xmalloc.h"
#include "snd.h"
#include "signal.h"

#define MAX_CHAT 65536
extern u_int8_t big_chatbuf[MAX_CHAT];

int
nick_to_uid (char *nick, int *uid)
{
	int found = 0;
	u_int8_t n = 0;
	char *p = nick;
	struct htlc_conn *htlcp;
	
	while (*p && *p != ' ') {
		p++;
		n++;
	}
	
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (!strncmp(nick, htlcp->name, n)) {
			if (!found) {
				*uid = htlcp->uid;
				found++;
			} else if (strlen(htlcp->name) == n) {
				*uid = htlcp->uid;
				found++;
			}
		}
	}

	if (found)
		return 0;
	else
		return 1;
}

static void
cmd_snd_chat (htlc_t *to, u_int32_t cid, u_int8_t *buf, u_int16_t len)
{
        if (cid) {
                struct htlc_chat *chat = chat_lookup_ref(cid);
                snd_chat_toone(to, chat, 0, buf, len);
        } else {
                snd_chat_toone(to, 0, 0, buf, len);
        }
}


#if defined(CONFIG_EXEC)

struct exec_file {
	struct htlc_conn *htlc;
	u_int32_t cid;
};

fd_set exec_fds;
u_int32_t nr_execs = 0;

static void
exec_close (int fd)
{
	close(fd);
	nr_open_files--;
	if (nr_execs)
		nr_execs--;
	xfree(hxd_files[fd].conn.ptr);
	memset(&hxd_files[fd], 0, sizeof(struct hxd_file));
	FD_CLR(fd, &hxd_rfds);
	FD_CLR(fd, &exec_fds);
	hxd_fd_del(fd);
}

void
exec_close_all (struct htlc_conn *htlc)
{
	struct exec_file *execp;
	int i;

	for (i = 0; i <= high_fd; i++) {
		if (FD_ISSET(i, &exec_fds)) {
			execp = (struct exec_file *)hxd_files[i].conn.ptr;
			if (execp->htlc == htlc)
				exec_close(i);
		}
	}
	htlc->nr_execs = 0;
}

void
exec_init (void)
{
	FD_ZERO(&exec_fds);
}

static void
exec_ready_read (int fd)
{
	struct exec_file *execp = (struct exec_file *)hxd_files[fd].conn.ptr;
	ssize_t r;
	u_int8_t buf[16384];

	r = read(fd, buf, sizeof(buf));
	if (r <= 0) {
		if (execp->htlc->nr_execs)
			execp->htlc->nr_execs--;
		exec_close(fd);
	} else {
		cmd_snd_chat(execp->htlc, execp->cid, buf, r);
	}
}

#if 0
static void
exec_ready_write (int fd)
{
}
#endif

extern void human_time (time_t ts, char *buf);	

void
cmd_exec (struct htlc_conn *htlc, u_int32_t cid, char *command)
{
	int argc, pfds[2], fakepfds[2];
	char *argv[32], myarg[32], *p, *pii, *thisarg, cmdpath[MAXPATHLEN];
	struct exec_file *execp;
	char errstr[64];
	int len, nolog=0, i;
	char *envp[6];
	char rootdir[MAXPATHLEN + 16];
	char accountdir[MAXPATHLEN + 16], account[32 + 16];
	char uptime[32 + 16];
	char version[6 + 16];
	u_int16_t style;
	char abuf[HOSTLEN+1];
	struct timeval now;
	time_t ts; 
	char tstr[32];

	if (htlc->nr_execs >= htlc->exec_limit) {
		style = htons(1);
		if (isclient(htlc->uid)) {
			len = snprintf(errstr, sizeof(errstr),
				"%u command%s at a time, please",
				htlc->exec_limit, htlc->exec_limit == 1 ? "" : "s");
			hlwrite(htlc, HTLS_HDR_MSG, 0, 2,
				HTLS_DATA_STYLE, 2, &style,
				HTLS_DATA_MSG, len, errstr);
		}
		return;
	} else if (nr_execs >= (u_int32_t)hxd_cfg.limits.total_exec) {
		style = htons(1);
		if (isclient(htlc->uid)) {
			len = snprintf(errstr, sizeof(errstr),
				"server is too busy, limit is %u command%s at a time",
				hxd_cfg.limits.total_exec,
				hxd_cfg.limits.total_exec == 1 ? "" : "s");
			hlwrite(htlc, HTLS_HDR_MSG, 0, 2,
				HTLS_DATA_STYLE, 2, &style,
				HTLS_DATA_MSG, len, errstr);
		}
		return;
	}

	while ((p = strstr(command, "../"))) {
		for (pii = p; *pii; pii++)
			*pii = *(pii + 3);
	}

	for (argc = 0, thisarg = p = command; *p && argc < 30; p++) {
		if (isspace(*p)) {
			*p = 0;
			argv[argc++] = thisarg;
			thisarg = p + 1;
		}
	}
	if (thisarg != p)
		argv[argc++] = thisarg;
	argv[argc] = 0;
	snprintf(cmdpath, sizeof(cmdpath), "%s/%s", hxd_cfg.paths.exec, argv[0]);
	snprintf(myarg, 30, thisarg);
	inaddr2str(abuf, &htlc->sockaddr);
	if (strlen(cmdpath) > 0 && strlen(myarg) > 0) {
		for (i = 0; hxd_cfg.options.exclude[i]; i++) {
			if (strstr(argv[0], hxd_cfg.options.exclude[i]))
				nolog = 1;
		}
		if (nolog == 0) {
			hxd_log("%s@%s:%u - %s:%s:%u - exec %s %s",
				htlc->userid, abuf, ntohs(htlc->sockaddr.SIN_PORT),
				htlc->name, htlc->login, htlc->uid, cmdpath, myarg);
#if defined(CONFIG_SQL)
		if (strncmp(cmdpath, "./login", 7 ))
			sql_exec(htlc->name, htlc->login, abuf, cmdpath, myarg);
#endif
		} else {
			hxd_log("%s@%s:%u - %s:%s:%u - exec %s",
				htlc->userid, abuf, ntohs(htlc->sockaddr.SIN_PORT),
				htlc->name,htlc->login,htlc->uid,cmdpath);
#if defined(CONFIG_SQL)
			sql_exec(htlc->name, htlc->login, abuf, cmdpath, " ");
#endif
		}
	}
	
	if (pipe(pfds)) {
		hxd_log("cmd_exec: pipe: %s", strerror(errno));
		return;
	}
	nr_open_files += 2;
	if (nr_open_files >= hxd_open_max) {
		hxd_log("%s:%d: %d >= hxd_open_max (%d)", __FILE__, __LINE__, pfds[0], hxd_open_max);
		close(pfds[0]);
		close(pfds[1]);
		nr_open_files -= 2;
		return;
	}

	switch (fork()) {
		case -1:
			hxd_log("cmd_exec: fork: %s", strerror(errno));
			close(pfds[0]);
			close(pfds[1]);
			nr_open_files -= 2;
			return;
		case 0:
			/* make sure fds 1 and 2 exist for dup2 */
			fakepfds[0] = fakepfds[1] = 0;
			pipe(fakepfds);
			if (pfds[1] != 1) {
				if (dup2(pfds[1], 1) == -1) {
					hxd_log("cmd_exec: dup2(%d,%d): %s",
						pfds[1], 1, strerror(errno));
					_exit(1);
				}
			}
			if (pfds[1] != 2) {
				if (dup2(pfds[1], 2) == -1) {
					hxd_log("cmd_exec: dup2(%d,%d): %s",
						pfds[1], 2, strerror(errno));
					_exit(1);
				}
			}
			close(0);
			if (fakepfds[0] > 2)
				close(fakepfds[0]);
			if (fakepfds[1] > 2)
				close(fakepfds[1]);
#if 0
			{
				int i;

				fprintf(stderr, "executing");
				for (i = 0; i < argc; i++)
					fprintf(stderr, " %s", argv[i]);
			}
#endif
			snprintf(rootdir, sizeof(rootdir), "ROOTDIR=%s", htlc->rootdir);
			snprintf(account, sizeof(account), "ACCOUNT=%s", htlc->login);
			snprintf(accountdir, sizeof(accountdir), "ACCOUNTDIR=%s", hxd_cfg.paths.accounts);
			gettimeofday(&now, 0);
			ts = tv_secdiff(&server_start_time, &now);
			human_time(ts, tstr);
			snprintf(uptime, sizeof(uptime), "UPTIME=%s", tstr);
			snprintf(version, sizeof(version), "VERSION=%s", hxd_version);
			envp[0] = rootdir;
			envp[1] = accountdir;
			envp[2] = account;
			envp[3] = uptime;
			envp[4] = version;
			envp[5] = 0;
			execve(cmdpath, argv, envp);
			fprintf(stderr, "\r%s: %s", argv[0], strerror(errno));
			_exit(1);
		default:
			close(pfds[1]);
			nr_open_files--;
			FD_SET(pfds[0], &exec_fds);
			execp = xmalloc(sizeof(struct exec_file));
			execp->htlc = htlc;
			execp->cid = cid;
			hxd_files[pfds[0]].conn.ptr = (void *)execp;
			hxd_files[pfds[0]].ready_read = exec_ready_read;
			FD_SET(pfds[0], &hxd_rfds);
			if (high_fd < pfds[0])
				high_fd = pfds[0];
			htlc->nr_execs++;
			nr_execs++;
			break;
	}
}
#endif /* ndef CONFIG_EXEC */

static void
cmd_err (struct htlc_conn *htlc, u_int32_t cid, char *cmd, char *errtext)
{
	size_t len = 0;
	char *buf = big_chatbuf;

	if (!strlen(cmd) > 0 || !strlen(errtext) > 0)
		return;

	len = snprintf(buf, MAX_CHAT, "\r%s: %s", cmd, errtext);

	cmd_snd_chat(htlc, cid, buf, len);
}


static void
cmd_denied (struct htlc_conn *htlc, u_int32_t cid, char *cmd)
{
	cmd_err(htlc, cid, cmd, "permission denied");
}
	
void
toggle_away (struct htlc_conn *htlc)
{
	htlc->color = htlc->color & 1 ? htlc->color&~1 : htlc->color|1;
	if (!htlc->flags.visible)
		return;
#if defined(CONFIG_SQL)
	sql_modify_user(htlc->name, htlc->icon, htlc->color, htlc->uid);
#endif
	snd_user_change(htlc);
}

static void
cmd_broadcast (struct htlc_conn *htlc, u_int32_t cid, char *chatbuf)
{
	u_int16_t style, len;
	struct htlc_conn *htlcp;

	if (!htlc->access.can_broadcast) {
		cmd_denied(htlc, cid, "broacast");
		return;
	}

	style = htons(1);
	len = strlen(chatbuf);
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (htlcp->access_extra.can_login)
			continue;
		hlwrite(htlcp, HTLS_HDR_MSG, 0, 2,
			HTLS_DATA_STYLE, 2, &style,
			HTLS_DATA_MSG, len, chatbuf);
	}
}

static void
cmd_users (struct htlc_conn *htlc, u_int32_t cid)
{
	struct htlc_conn *htlcp;
	char *user_list;
	u_int32_t pos = 0, n;
	size_t len = 0;
	u_int32_t uid;

	if (!htlc->access_extra.manage_users) {
		cmd_denied(htlc, cid, "users");
		return;
	}

	/* 
	 * need to know how big the thing is so we can allocate the memory
	 * (reallocating as we go is much slower)
	 */
	len = (42 * nhtlc_conns) + 243;
	user_list = xmalloc(len);
	pos += sprintf(&user_list[pos],
			"(chat 0x0): %u users:\r*** Visible Users ***\r\r", nhtlc_conns);
	n = 0;
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (!htlcp->flags.visible)
			continue;
		if (!n) {
			pos += sprintf(&user_list[pos],
					"uid | nickname                  |  priv  \r");
			pos += sprintf(&user_list[pos],
					"_________________________________________\r");
		}
		pos += sprintf(&user_list[pos], "%-3u | %-25.25s | %4s\r",
			       htlcp->uid, htlcp->name, htlcp->color % 4 > 1 ? "OWNER" : "LUSER");
		n++;
	}

	if (!n)
		pos += sprintf(&user_list[pos], "none.\r");
	pos += sprintf(&user_list[pos], "\r*** Invisible Users ***\r\r");
	
	n = 0;
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (htlcp->flags.visible)
			continue;
		if (!n) {
			pos += sprintf(&user_list[pos],
					"uid | nickname                  |  priv  \r");
			pos += sprintf(&user_list[pos],
					"_________________________________________\r");
		}
		pos += sprintf(&user_list[pos], "%-3u | %-25.25s | %4s\r",
				htlcp->uid, htlcp->name, htlcp->color % 4 > 1 ? "OWNER" : "LUSER");
		n++;
	}

	if (!n)
		pos += sprintf(&user_list[pos], "none.\r");
	uid = htons(mangle_uid(htlc));
	hlwrite(htlc, HTLS_HDR_MSG, 0, 3,
		HTLS_DATA_UID, sizeof(uid), &uid,
		HTLS_DATA_MSG, strlen(user_list), user_list,
		HTLS_DATA_NAME, 3, "hxd");
	/* free the memory used by the userlist */
	xfree(user_list);
}

static void
cmd_visible (struct htlc_conn *htlc, u_int32_t cid)
{
	if (!htlc->access_extra.user_visibility) {
		cmd_denied(htlc, cid, "visible");
		return;
	}
	htlc->flags.visible = !htlc->flags.visible;
	if (!htlc->flags.visible) {
		snd_user_part(htlc);
	} else {
		snd_user_change(htlc);
	}
}

static void
cmd_color (struct htlc_conn *htlc, u_int32_t cid, char *chatbuf)
{
	u_int16_t color = htlc->color;

	if (!htlc->access_extra.user_color) {
		cmd_denied(htlc, cid, "color");
		return;
	}

	htlc->color = strtoul(chatbuf, 0, 0);
#if defined(CONFIG_SQL)
	sql_modify_user(htlc->name, htlc->icon, htlc->color, htlc->uid);
#endif
	if (htlc->color != color && htlc->flags.visible)
		snd_user_change(htlc);
}

extern int access_extra_set (struct extra_access_bits *acc, char *p, int val);
extern int set_access_bit (struct hl_access_bits *acc, char *p, int val);
extern void user_access_update (struct htlc_conn *htlcp);

static void
cmd_access (struct htlc_conn *htlc, u_int32_t cid, char *chatbuf)
{
	char *p, *str;
	u_int32_t uid;
	int val;
	struct htlc_conn *htlcp;
	char errbuf[MAX_CHAT];
	char nickbuf[32];
	int f[2], i = 1;
	char abuf[HOSTLEN+1];

	if (!htlc->access_extra.user_access) {
		cmd_denied(htlc, cid, "access");
		return;
	}

	p = chatbuf;
	uid = atou32(p);
	if (!strncmp(p, "0 ", 2))
		uid = 0;
	else if (!uid && nick_to_uid(p, &uid)) {
		while (*p && *p != ' ') {
			p++; i++;
		}
		snprintf(nickbuf, i, "%s", chatbuf);
		snprintf(errbuf, MAX_CHAT - 8, "no such user \"%s\"", nickbuf);
		cmd_err(htlc, cid, "access", errbuf);
		return;
	}
	htlcp = isclient(uid);
	if (!htlcp) {
		snprintf(errbuf, MAX_CHAT - 8, "no such user (uid:%u)", uid);
		cmd_err(htlc, cid, "access", errbuf);
		return;
	}
	if (!htlcp->access_extra.access_volatile) {
		cmd_err(htlc, cid, "access", "user cannot be modified");
		return;
	}
	while (*p && *p != ' ')
		p++;
	while (*p && *p == ' ')
		p++;

	inaddr2str(abuf, &htlc->sockaddr);
	str = "";
	while (*p) {
		str = p;
		p = strchr(p, '=');
		if (!p)
			break;
		*p = 0;
		p++;
		val = *p == '1' ? 1 : 0;
		p++;
		f[0] = access_extra_set(&htlcp->access_extra, str, val);
		f[1] = set_access_bit(&htlcp->access, str, val);
		if (f[0] && f[1]) {
			snprintf(errbuf, MAX_CHAT - 8, "unknown argument \"%s\"", str);
			cmd_err(htlc, cid, "access", errbuf);
			return;
		}
		while (*p && *p != ' ')
			p++;
		while (*p && *p == ' ')
			p++;
	}
	hxd_log("%s@%s:%u - %s:%u:%u:%s modified access of %s:%u:%u:%s - %s",
				htlc->userid, abuf, ntohs(htlc->sockaddr.SIN_PORT),
				htlc->name, htlc->icon, htlc->uid, htlc->login,
				htlcp->name, htlcp->icon, htlcp->uid, htlcp->login, str);

	user_access_update(htlcp);
	snd_user_change(htlcp);
}

static int
user_0wn (struct htlc_conn *htlc, char *str, char *p)
{
	if (!strncmp(str, "icon", 4)) {
		u_int16_t icon;

		icon = strtoul(p, 0, 0);
		htlc->icon = icon;
		return 1;
	} else if (!strncmp(str, "color", 5)) {
		u_int16_t color;

		color = strtoul(p, 0, 0);
		htlc->color = color;
		return 2;
	} else if (!strncmp(str, "name", 4)) {
		size_t len;

		/* copies the rest of the command into name! */
		len = strlen(p);
		if (len > sizeof(htlc->name)-1)
			len = sizeof(htlc->name)-1;
		memcpy(htlc->name, p, len);
		htlc->name[len] = 0;
		return 3;
	} else if (!strncmp(str, "g0away", 6) || !strncmp(str, "visible", 7)) {
		u_int16_t visible;

		visible = strtoul(p, 0, 0);
		htlc->flags.visible = visible;

		if (!visible) {
	                snd_user_part(htlc);
	        } else {
			snd_user_change(htlc);
		}
		return 4;
	}

	return 0;
}

static void
cmd_0wn (struct htlc_conn *htlc, u_int32_t cid, char *chatbuf)
{
	char *p, *str;
	u_int32_t uid;
	int x, n, i=0;
	struct htlc_conn *htlcp;
	char nickbuf[sizeof(big_chatbuf)];
	char errbuf[sizeof(big_chatbuf)];
	char abuf[HOSTLEN+1];

	if (!htlc->access_extra.user_0wn) {
		cmd_denied(htlc, cid, "0wn");
		return;
	}
	
	p = chatbuf;
	
	uid = atou32(p);
	if (!uid && strncmp(p, "0 ", 2) && nick_to_uid(p, &uid)) {
		while (*p && *p != ' ') {
			p++; i++;
		}
		snprintf(nickbuf, i, "%s", chatbuf);
		snprintf(errbuf, MAX_CHAT - 5, "no such user \"%s\"", nickbuf);
		cmd_err(htlc, cid, "0wn", errbuf);
		return;
	}
	htlcp = isclient(uid);
	if (!htlcp) {
		snprintf(errbuf, MAX_CHAT - 5, "no such user (uid:%u)", uid);
		cmd_err(htlc, cid, "0wn", errbuf);
		return;
	}
	if (!htlcp->access_extra.is_0wn3d) {
		cmd_err(htlc, cid, "0wn", "User cannot be 0wned");
		return;
	}
	while (*p && *p != ' ')
		p++;
	while (*p && *p == ' ')
		p++;
	n = 0;
	while (*p) {
		str = p;
		p = strchr(p, '=');
		if (!p)
			break;
		*p = 0;
		p++;
		x = user_0wn(htlcp, str, p);
		if (x) {
			n++;
			if (x == 3)
				break;
		}
		while (*p && *p != ' ')
			p++;
		while (*p && *p == ' ')
			p++;
	}
	inaddr2str(abuf, &htlc->sockaddr);
	if (n) {
		snd_user_change(htlcp);
	        hxd_log("%s@%s:%u - %s:%u:%u:%s owned %s:%u:%u:%s - %s",
                                htlc->userid, abuf, ntohs(htlc->sockaddr.SIN_PORT),
                                htlc->name, htlc->icon, htlc->uid, htlc->login,
                                htlcp->name, htlcp->icon, htlcp->uid, htlcp->login, str);
	}
}

static void
cmd_mon (struct htlc_conn *htlc, u_int32_t cid, char *chatbuf)
{
	struct htlc_conn *htlcp;
	u_int32_t uid;

	if (!htlc->access.disconnect_users) {
		cmd_denied(htlc, cid, "mon");
		return;
	}

	uid = atou32(chatbuf);
	htlcp = isclient(uid);
	if (!htlcp)
		return;
	if (!htlcp->access_extra.access_volatile)
		return;
	htlcp->access.send_msgs = 1;
}

static void
cmd_version (struct htlc_conn *htlc, u_int32_t cid)
{
	char version[256];
	int len;

	len = snprintf(version, sizeof(version),
"\rhxd release %s\r"
"Developers:\r"
" hxd: rorschach, ran@fortyoz.org\r"
" kxd: kang, kang@insecure.ws\r"
"shxd: devin teske, devinteske@hotmail.com\r"
"Contributions to kxd: weejay, SaraH, silver:box, Schub, Philou\r"
"Contributions to hxd: FORTYoz, mishan\r",
			hxd_version);

	cmd_snd_chat(htlc, cid, version, len);
}

static void
cmd_me (struct htlc_conn *htlc, u_int32_t cid, char *chatbuf)
{
	struct htlc_chat *chat = 0;
	size_t len = 0;
	char *buf = big_chatbuf;

	if (!strlen(chatbuf) > 0)
		return;

	len = snprintf(buf, MAX_CHAT, hxd_cfg.strings.chat_opt_format,
			htlc->name, chatbuf);

	if (cid) {
		chat = chat_lookup_ref(cid);
		if (!chat || !chat_isset(htlc, chat, 0))
			return;
		snd_chat(chat, htlc, buf, len);
	} else {
		snd_chat(0, htlc, buf, len);
	}
}

extern void user_kick (struct htlc_conn *htlcp, time_t ban, struct htlc_conn *htlc, 
		       const char *fmt, ...);
		
static void
cmd_kick (struct htlc_conn *htlc, u_int32_t cid, char *chatbuf, int ban)
{
	char *p, *str;
	u_int32_t uid;
	struct htlc_conn *htlcp;
	int n, i = 1;
	char errbuf[sizeof(big_chatbuf)];
	char nickbuf[sizeof(big_chatbuf)];

	if (!htlc->access.disconnect_users) {
		if (!ban)
			cmd_denied(htlc, cid, "kick");
		else
			cmd_denied(htlc, cid, "ban");
		return;
	}

	p = chatbuf;
	uid = atou32(p);
	if (!uid && strncmp(p, "0 ", 2) && nick_to_uid(p, &uid)) {
		while (*p && *p != ' ') {
			p++;
			i++;
		}
		snprintf(nickbuf, i, "%s", chatbuf);
		snprintf(errbuf, MAX_CHAT - 7, "no such user \"%s\"", nickbuf);
		if (!ban)
			cmd_err(htlc, cid, "kick", errbuf);
		else
			cmd_err(htlc, cid, "ban", errbuf);
		return;
	}
	htlcp = isclient(uid);
	if (!htlcp) {
		snprintf(errbuf, MAX_CHAT - 7, "no such user (uid:%u)", uid);
		cmd_err(htlc, cid, "kick", errbuf);
		return;
	}

	if(ban)
		ban = hxd_cfg.options.ban_time;

	n = 1;
	while (*p && *p != ' ') {
		p++;
		n++;
	}

	if ((htlcp = isclient(uid))) {
		if (htlcp->access.cant_be_disconnected) {
			snprintf(errbuf, MAX_CHAT - 7, "%s cannot be disconnected.", htlcp->name);
			cmd_err(htlc, cid, "kick", errbuf);
			return;
		}
		str = &chatbuf[n];

		
		if (strlen(p))
			user_kick(htlcp, ban, htlc, str);
		else
			user_kick(htlcp, ban, htlcp, " ");
	}
}


static void
cmd_alrt (struct htlc_conn *htlc, u_int32_t cid, char *chatbuf)
{
	char *p, *str;
	u_int32_t uid;
	u_int16_t style, len;
	struct htlc_conn *htlcp;
	int n, i = 1;
	char errbuf[sizeof(big_chatbuf)];
	char nickbuf[sizeof(big_chatbuf)];

	if (!htlc->access.can_broadcast) {
		cmd_denied(htlc, cid, "alert");
		return;
	}
	
	p = chatbuf;
	uid = atou32(p);

	if (!uid && strncmp(p, "0 ", 2) && nick_to_uid(p, &uid)) {
		while (*p && *p != ' ') {
			p++;
			i++;
		}
		snprintf(nickbuf, i, "%s", chatbuf);
		snprintf(errbuf, MAX_CHAT - 7, "no such user \"%s\"", nickbuf);
		cmd_err(htlc, cid, "alert", errbuf);
		return;
	}
	htlcp = isclient(uid);
	if (!htlcp) {
		snprintf(errbuf, MAX_CHAT - 7, "no such user (uid:%u)", uid);
		cmd_err(htlc, cid, "alert", errbuf);
		return;
	}
	n = 1;
	while (*p && *p != ' ') {
		p++;
		n++;
	}

	if (!strlen(p) > 0)
		return;
	style = htons(1);
	str = &chatbuf[n];
	len = strlen(str);
	if (isclient(htlcp->uid) != 0) {
		hlwrite(htlcp, HTLS_HDR_MSG, 0, 2,
			HTLS_DATA_STYLE, 2, &style,
			HTLS_DATA_MSG, len, str);
	}
}

static void
cmd_notfound (struct htlc_conn *htlc, u_int32_t cid, char *chatbuf)
{
	char *p, buf[256];
	int len;

	p = strchr(chatbuf, ' ');
	if (p)
		len = p - chatbuf;
	else
		len = strlen(chatbuf);
	len = snprintf(buf, sizeof(buf), "\rcommand not found: %.*s", len, chatbuf);
	if (len == -1)
		len = sizeof(buf);
	cmd_snd_chat(htlc, cid, buf, len);
}

/* chatbuf points to the auto array in rcv_chat + 1 */
void
command_chat (struct htlc_conn *htlc, u_int32_t cid, char *chatbuf)
{
	if (!chatbuf[0])
		return;
	switch (chatbuf[0]) {
		case '0':
			if (!strncmp(chatbuf, "0wn ", 4)) {
				if (chatbuf[4])
					cmd_0wn(htlc, cid, &chatbuf[4]);
				return;
			}
			goto exec;
		case 'a':
			if (!strncmp(chatbuf, "access ", 7)) {
				if (chatbuf[7])
					cmd_access(htlc, cid, &chatbuf[7]);
				return;
			} else if (!strncmp(chatbuf, "away", 4)) {
				if (htlc->flags.away == AWAY_INTERRUPTED)
					return;
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
				return;
			} else if (!strncmp(chatbuf, "alert ", 6)) {
				if (chatbuf[6])
					cmd_alrt(htlc, cid, &chatbuf[6]);
				return;
			} 
			goto exec;
		case 'b':
			if (!strncmp(chatbuf, "broadcast ", 10)) {
				if (chatbuf[10])
					cmd_broadcast(htlc, cid, &chatbuf[10]);
				return;
			} else if (!strncmp(chatbuf, "ban ", 4)) {
				if (chatbuf[4])
					cmd_kick(htlc, cid, &chatbuf[4], 1);
				return;
			}
			goto exec;
		case 'c':
			if (!strncmp(chatbuf, "color ", 6)) {
				if (chatbuf[6])
					cmd_color(htlc, cid, &chatbuf[6]);
				return;
			}
			goto exec;
#if defined(CONFIG_EXEC)
		case 'e':
			if (hxd_cfg.operation.exec) {
				if (!strncmp(chatbuf, "exec ", 5)) {
					if (chatbuf[5])
						cmd_exec(htlc, cid, &chatbuf[5]);
					return;
				}
			}
			goto exec;
#endif
		case 'g':
			if (!strncmp(chatbuf, "g0away", 6)) {
				cmd_visible(htlc, cid);
				return;
			}
			goto exec;
		case 'k':
			if (!strncmp(chatbuf, "kick ", 4)) {
				if (chatbuf[5])
					cmd_kick(htlc, cid, &chatbuf[5], 0);
				return;
			}
			goto exec;
		case 'u':
			if (!strncmp(chatbuf, "users", 5)) {
				cmd_users(htlc, cid);
				return;
			}
			goto exec;
		case 'v':
			if (!strncmp(chatbuf, "visible", 7)) {
				cmd_visible(htlc, cid);
				return;
			} else if (!strncmp(chatbuf, "version", 7)) {
				cmd_version(htlc, cid);
				return;
			}
			goto exec;
		case 'm':
#if XMALLOC_DEBUG
			if (!strncmp(chatbuf, "maltbl", 6) && htlc->access_extra.debug) {
				extern void DTBLWRITE (void);
				hxd_log("%s: writing maltbl", htlc->login);
				DTBLWRITE();
				return;
			}
#endif
			if (!strncmp(chatbuf, "me ", 3)) {
				if (chatbuf[3])
					cmd_me(htlc, cid, &chatbuf[3]);
				return;
			} else if (!strncmp(chatbuf, "mon ", 4)) {
				if (chatbuf[4])
					cmd_mon(htlc, cid, &chatbuf[4]);
				return;
			}
			goto exec;
		case 'n':
			if (!strncmp(chatbuf, "nick ", 5) && (htlc->access.use_any_name && !htlc->access_extra.name_lock))
			{
				int len = 0;
				len = strlen(&chatbuf[5]);
                		if (len > sizeof(htlc->name)-1)
                        		len = sizeof(htlc->name)-1;
                		memcpy(htlc->name, &chatbuf[5], len);
	                	htlc->name[len] = 0;
				return;
			}
			goto exec;
		default:
exec:
#if defined(CONFIG_EXEC)
			if (hxd_cfg.operation.exec) {
				cmd_exec(htlc, cid, chatbuf);
			} else 
#endif
				cmd_notfound(htlc, cid, chatbuf);
			break;
	}
}
