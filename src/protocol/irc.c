#include "hlserver.h"
#include "hxd_rcv.h"
#include "chatproto.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "xmalloc.h"
#include "protocols.h"
#include "irc.h"

extern void user_loginupdate (htlc_t *htlc);

/* this function remove any non-standard character in nicknames
 * and puts a ~ in front of hotline users, making their nick both unique to irc
 * and easily identifiable */
/*static char *
satanize_nick (htlc_t *htlc)
{
	char *final;
go up to first strange char, add UID
	return final;

}
*/
static void *
cr_to_lf ( char *str )
{
	u_int8_t i = 0;
	while (str[i] != 0) {
		if (str[i] == '\r') {
			str[i] = '\n';
		}
		i++;
	}
	return;
}


static char *
read_file (int fd, size_t max, size_t *lenp)
{
	char *__news_buf = 0;
	size_t __news_len = 0;
#define BLOCKSIZE	4096
	char *buf = __news_buf;
	size_t len = __news_len;
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
		if (r <= 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			break;
		}
		pos += r;
		max -= r;
		if (r != (ssize_t)rn || !max)
			break;
	}
	if (lenp)
		*lenp = pos;
	__news_buf = buf;
	__news_len = len;

	return buf;
}


static void
do_command (htlc_t *htlc, char *linep)
{
	u_int8_t *p, *cmd, *arg, *arg2;
	char *agbuf;
	char nick_list[255];
	u_int32_t fd, len;
	//struct hl_userlist_hdr *uh;
	struct htlc_conn *htlcp;
	
	u_int8_t ircops=0, users=0, invisible=0;

	/* update idle time for irc */
	gettimeofday(&htlc->idle_tv, 0);
	 
	cmd = linep;
	for (p = linep; *p; p++) {
		if (isspace(*p))
			break;
	}
	if (!*p)
		return;
	*p = 0;
	p++;
	while (isspace(*p))
		p++;
	arg = p;
	if (!*p)
		return;

	switch (cmd[0]) {
		case 'J':
			if (!strncmp(cmd, "JOIN", 4)) {
				//irc_join(htlc, cmd, arg);
			}
			break;
		case 'N':
			if (!strncmp(cmd, "NICK", 4)) {
				htlc->access.read_chat = 1;
				htlc->access.send_chat = 1;
				htlc->access.send_msgs = 1;
				snd_user_change(htlc, arg);
			}
			break;
		
		case 'P':
			if (!strncmp(cmd, "PRIVMSG", 7)) {
				arg2 = arg;
				while (*arg2 && !isspace(*arg2))
					arg2++;
				*arg2 = 0;
				arg2++;
				arg2++;

				if (*arg == '#') {
					hxd_rcv_chat(htlc, 0, 0, 0, arg2, strlen(arg2));
				} else {
					htlc_t *htlcp;
					for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next)
						if (!strcmp(htlcp->name, arg))
							break;
					
				if (!htlcp)
					return;
				hxd_rcv_msg(htlc, htlcp, arg2, strlen(arg2));
				}
	
			} else if (!strncmp(cmd, "PING", 4)) {
				cp_snd_linef(htlc, ":%s PONG %s :%s", getHost(htlc), hxd_cfg.irc.hostname, htlc->name);
			}
			break;
			
		case 'Q':
			if (!strncmp(cmd, "QUIT", 4)) {
				snd_user_part(htlc, htlc);
				htlc_close(htlc);
			}
			break;
		case 'U':
			if (!strncmp(cmd, "USER", 4)) {
				if (!htlc->access_extra.can_login)
					return;
                                htlc->access.read_chat = 1;
                                htlc->access.send_chat = 1;
                                htlc->access.send_msgs = 1;
				arg2 = arg;
                                while (*arg2 && (*arg2 != ':')){
                                        arg2++;
				}
                                *arg2 = 0;
                                arg2++;

                                snd_user_change(htlc, arg2);

                                hxd_log("IRC> %s@%s logged in", htlc->name, htlc->userid);
                                htlc->access_extra.can_login = 0;
                                htlc->uid = uid_assign(htlc);
                                user_loginupdate(htlc);

                                /*starts login emulation in IRC*/

                                for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
                                        if(htlcp->access.disconnect_users)
                                                ircops++;
                                        if(!htlcp->flags.visible)
                                                invisible++;
                                        users++;
                                }

                                cp_snd_linef(htlc,
"NOTICE AUTH :*** Looking up your hostname...\r\n"
"NOTICE AUTH :*** Checking Ident\r\n");
                                if(!htlc->userid) {
                                        strlcpy(htlc->userid, "~", 2);
                                        cp_snd_linef(htlc, "NOTICE AUTH :*** No Ident response");
                                }

                                cp_snd_numeric(htlc, IRC_RPL_WELCOME, "Welcome to %s %s\r\n",
				  hxd_cfg.tracker.name, htlc->name);
                                cp_snd_numeric(htlc, IRC_RPL_MYINFO, "This is an experimental mhxd server");
                                cp_snd_numeric(htlc, IRC_RPL_MYINFO, "WALLCHOPS KNOCK EXCEPTS INVEX MODES=4 MAXCHANNELS=65335 MAXBANS=65335 MAXTARGETS=4 NICKLEN=32 TOPICLEN=300 KICKLEN=300 :are maybe supported by this server");
                                cp_snd_numeric(htlc, IRC_RPL_MYINFO, "CHANTYPES=# PREFIX=(ohv)@%%+ CHANMODES=eIb,k,l,imnpstZ NETWORK=%s CASEMAPPING=rfc1459 CALLERID :are maybe supported by this server", "Hotline");

                                cp_snd_numeric(htlc,IRC_RPL_MYINFO,
"There are %u users and %u invisible on 1 servers\r\n"
"%u :IRC Operators online\r\n"
"<unknown> :channels formed\r\n"
"I have %u clients and 1 servers\r\n"
"Current local  users: %u  Max: 65335\r\n"
"Current global users: %u  Max: 65335",
users, invisible, ircops, users, users, users);


                                /* We send agreement as message of the day */
                                if ((fd = SYS_open(hxd_cfg.paths.agreement, O_RDONLY, 0)) > 0) {
                                        agbuf = read_file(fd, 0xffff, &len);
                                	cp_snd_numeric(htlc, IRC_RPL_MOTDSTART, "- Message of the Day -");
                                	cp_snd_numeric(htlc, IRC_RPL_MOTD, "- By entering this server, you agree the following statements -");
                                        cp_snd_numeric(htlc, IRC_RPL_MOTD, agbuf);
                                	cp_snd_numeric(htlc, IRC_RPL_ENDOFMOTD, "End of /MOTD command.");
				}
                                else {
                                        agbuf = 0;
					cp_snd_numeric(htlc, IRC_ERR_NOMOTD, "Agreement File is missing");
				}
                                close(fd);
                                //cp_snd_linef(htlc, ":%s MODE %s :+ph", arg, arg);
                                cp_snd_linef(htlc, "PING :%s", getHost(htlc));
				irc_join(htlc, "/JOIN", "#public");
                                cp_snd_linef(htlc, "*** You were forced to join #public.");

			}
			break;
		case 'W':
			if(!strncmp(cmd, "WHO", 3)) {
				len = 0;
                		for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next)
                         		if (htlcp->flags.visible){
                                    		len += snprintf(nick_list+len, 1+sizeof(htlcp->name), "%s ", htlcp->name);
                                    		//hxd_log("list from who: %s: fulllen: %x, added: %s", nick_list, len, htlcp->name);
                         		}

                 	cp_snd_linef(htlc, ":%s %s %s = %s :@%s",
                    		hxd_cfg.irc.hostname, IRC_RPL_NAMREPLY, htlc->name, arg, nick_list);
                 	cp_snd_linef(htlc, ":%s %s %s %s :End of /NAMES list.",
                 	hxd_cfg.irc.hostname, IRC_RPL_ENDOFNAMES, htlc->name, arg);
			}
			break;
		default:
			cp_snd_numeric(htlc, IRC_ERR_UNKNOWNCOMMAND, "Unknown command");
			hxd_log("IRC> unknown command: %s arg: %s", cmd, arg);
			break;
	}
}

static void
rcv_chat (htlc_t *htlc)
{
	struct qbuf *in = &htlc->in;
	u_int8_t *p, *linep;
	u_int32_t pos;

	linep = in->buf;
	for (p = in->buf+1; p < (in->buf+in->pos); p++) {
#if 0
		/* RFC/irssi behavior */
		if (*(p-1) == '\r' && *p == '\n') {
			*(p-1) = 0;
#else
		/* Handle mirc/bitchx as well */
		if (*p == '\n') {
			if (*(p-1) == '\r')
				*(p-1) = 0;
			else
				*p = 0;
#endif
			do_command(htlc, linep);
			linep = p+1;
		}
	}
	pos = p - linep;
	qbuf_set(in, pos, 0);
}

static int
rcv_magic (htlc_t *htlc)
{
	struct qbuf *in = &htlc->in;

	if (in->pos >= 4
	    && (   !memcmp(in->buf, "NICK", 4)
		|| !memcmp(in->buf, "USER", 4)
		|| !memcmp(in->buf, "PASS", 4))) {
		htlc->proto = irc_proto;
		htlc->flags.is_irc = 1;
		htlc->rcv = rcv_chat;
		if (in->pos > 4)
			rcv_chat(htlc);
		return 1;
	}
	if (in->pos >= 4 || (in->buf[0] != 'N' && in->buf[0] != 'P' && in->buf[0] != 'U'))
		return -1;

	return 0;
}

static void
cp_snd_line (htlc_t *htlc, u_int8_t *buf, u_int16_t len)
{
	struct qbuf *out = &htlc->out;
	qbuf_add(out, buf, len);
	qbuf_add(out, "\r\n", 2);
	if (htlc->wfd)
		hxd_fd_set(htlc->wfd, FDW);
	else
		hxd_fd_set(htlc->fd, FDW);
}

static void
cp_snd_linef (htlc_t *htlc, const char *fmt, ...)
{
	char buf[4096];
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	cp_snd_line(htlc, buf, len);
}

static void
cp_snd_numeric (htlc_t *htlc, const char numeric[3], const char *fmt, ...)
{
	char buf [4096];
	int len;
	va_list ap;
	
	len = snprintf(buf, sizeof(buf), ":%s %s %s :", hxd_cfg.irc.hostname, numeric, htlc->name); 
	va_start(ap, fmt);
	len += vsnprintf(buf+len, sizeof(buf), fmt, ap);
	va_end(ap);
	
	cp_snd_linef(htlc, buf, len);
}

static void
snd_msg (htlc_t *htlc, htlc_t *htlcp, u_int8_t *buf, u_int16_t len)
{
	char host[255];
	char abuf[HOSTLEN+1];

	inaddr2str(abuf, &htlcp->sockaddr);
	if (!htlc->host)
		snprintf(host, 255, abuf);
	else
		snprintf(host, 255, htlc->host);
	cp_snd_linef(htlcp, ":%s!%s@%s PRIVMSG %s :%.*s", htlc->name, htlc->userid, host, htlcp->name, len, buf);
}

static void
snd_chat (htlc_t *htlcp, struct htlc_chat *chat, htlc_t *htlc, const u_int8_t *buf, u_int16_t len)
{
	u_int8_t *name, *p;

	/* in IRC, we don't send back the chat to the sender */
	if (htlcp == htlc)
		return;

	p = buf;
	while (isspace(*p))
		p++;
	name = p;
	for (; p < buf+len;p++) {
		if (*p == ':')
			break;
	}
	*p = 0;
	//buf[len] = 0;

	if(!htlc)
		cp_snd_linef(htlcp, ":%s!%s@%s PRIVMSG %s %s", "servermsg", "hxd", "localhost", "#public", (p+2));
	else
		cp_snd_linef(htlcp, ":%s!%s@%s PRIVMSG %s %s", htlc->name, htlc->userid, getHost(htlc), "#public", p+2);
	//*p = ':';
}

/* IRC pchat/channel joining procedure */
static void
irc_join (htlc_t *htlc, char *cmd, char *arg)
{
	struct htlc_chat *chat;
	char nick_list[255];
	htlc_t *htlcp;
	int len=0;

        cp_snd_linef(htlc, ":%s!%s@%s JOIN :%s", htlc->name, htlc->userid, getHost(htlc), arg);
	if (!strncmp(arg, "#public", 7))
        	chat = chat_lookup_ref(0);
	else {
		cp_snd_numeric(htlc, IRC_ERR_NOSUCHCHANNEL, "No such channel");
	}
	
		
  /*      XXX SUBJECT/TOPIC if (strlen(chat->subject))
        	cp_snd_numeric(htlc, "%s", chat->subject);
        else
        	cp_snd_numeric(htlc, "No Subject.");
*/                //cp_snd_linef(htlc, ":%s MODE %s +nt", hxd_cfg.irc.hostname, arg);
		len = 0;
                for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next)
                         if (htlcp->flags.visible){
				    len += snprintf(nick_list+len, 1+sizeof(htlcp->name), "%s ", htlcp->name);
                                    //hxd_log("list from join: %s: fulllen: %x, added: %s", nick_list, len, htlcp->name);
                         }
                 
                 cp_snd_linef(htlc, ":%s %s %s = %s :@%s", hxd_cfg.irc.hostname, IRC_RPL_NAMREPLY, htlc->name, arg, nick_list);
                 cp_snd_linef(htlc, ":%s %s %s %s :End of /NAMES list.", hxd_cfg.irc.hostname, IRC_RPL_ENDOFNAMES, htlc->name, arg);
		 return;
}

/* getHost(htlc)
 * returns hostname or ip if not found */
char *
getHost (htlc_t *htlc)
{
	char abuf[HOSTLEN+1];
	if(!htlc)
		return "localhost";

	if (!strlen(htlc->host)){
		inaddr2str(abuf, &htlc->sockaddr);
		}
	else
		snprintf(abuf, HOSTLEN+1, htlc->host);

	return (char *)&abuf;
}

static void
snd_user_part (struct htlc_conn *to, struct htlc_conn *parting)
{
	hxd_log("part: %s:%s", to->name, parting->name);
	cp_snd_linef(to, ":%s QUIT :<< %s has left >>", parting->name, parting->name);
	return;
}

static void
snd_user_change (htlc_t *htlc, char *arg)
{
        u_int16_t uid;
        u_int16_t nlen;

        uid = htons(mangle_uid(htlc));
        nlen = strlen(arg);

	cp_snd_linef(htlc, ":%s!%s@%s NICK :%s", htlc->name, htlc->userid, getHost(htlc), arg);
	strlcpy(htlc->name, arg, 32);
	hxd_log("IRC> user change %s, %s", htlc->name, arg);
	return;
}

static void
snd_error (htlc_t *htlc, const u_int8_t *str, u_int16_t len)
{
	cp_snd_numeric(htlc, IRC_ERR_UNAVAILRESOURCE, htlc->name, str);
	if(len)
		return;
	return;
}

struct protocol_function irc_functions[] = {
	{ RCV_MAGIC, { rcv_magic } },
	{ SND_CHAT, { snd_chat } },
	{ SND_MSG, { snd_msg } },
	{ SND_ERROR, { snd_error } },
	{ SND_USER_PART, { snd_user_part } },
	{ SND_USER_CHANGE, { snd_user_change } }
};
#define nirc_functions (sizeof(irc_functions)/sizeof(struct protocol_function))

#ifdef MODULE
#define irc_protocol_init module_init
#endif

void
irc_protocol_init (void)
{
	irc_proto = protocol_register("irc");
	if (!irc_proto)
		return;
	protocol_register_functions(irc_proto, irc_functions, nirc_functions);
}
