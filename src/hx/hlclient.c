#include <sys/types.h>
#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#if !defined(__WIN32__)
#include <pwd.h>
#endif
#include <ctype.h>
#include <locale.h>
#include <dirent.h>
#include "hxd.h"
#include "hx.h"
#include "getopt.h"
#include "xmalloc.h"

#define SYSTEM_HXRC_PATH	"/etc/hxrc"

char last_msg_nick[32];

char *g_info_prefix;
char *g_user_colors[4];

#define DEFAULT_TIMEFORMAT "%c"
char *hx_timeformat;

#if defined(CONFIG_JAPANESE)
#define DEFAULT_ENCODING "EUC-JP"
#define DEFAULT_SERVER_ENCODING "SHIFT-JIS"
#else
#define DEFAULT_ENCODING "ISO-8859-1"
#define DEFAULT_SERVER_ENCODING "ISO-8859-1"
#endif
char *g_encoding;
char *g_server_encoding;

int hist_size;

char hx_homedir[MAXPATHLEN];
char hx_user[64];
char g_hxvars_path[MAXPATHLEN];

u_int16_t tty_show_user_changes = 1;
int tty_show_user_joins = 1;
int tty_show_user_parts = 1;
int tty_chat_pretty = 1;
int g_strip_ansi = 0;
int chat_colorz = 0;
int g_auto_nick_complete = 0;
#if defined(CONFIG_HLDUMP)
int g_hldump = 0;
#endif
u_int16_t g_clientversion = 0;
u_int16_t g_default_secure = 1;

static void
init_vars (void)
{
#if defined(CONFIG_HLDUMP)
	variable_add(&g_hldump, set_bool, "hldump");
#endif
	g_info_prefix = xstrdup(DEFAULT_INFOPREFIX);
	variable_add(&g_info_prefix, set_string, "infoprefix");
	g_user_colors[0] = xstrdup(WHITE_BOLD);
	g_user_colors[1] = xstrdup(WHITE);
	g_user_colors[2] = xstrdup(RED_BOLD);
	g_user_colors[3] = xstrdup(RED);
	variable_add(&tty_show_user_changes, set_int16, "tty_show_user_changes");
	variable_add(&tty_show_user_joins, set_bool, "tty_show_user_joins");
	variable_add(&tty_show_user_parts, set_bool, "tty_show_user_parts");
	variable_add(&tty_chat_pretty, set_bool, "tty_chat_pretty");
	variable_add(&g_strip_ansi, set_bool, "strip_ansi");
	variable_add(&chat_colorz, set_bool, "chat_colorz");
	variable_add(&g_auto_nick_complete, set_bool, "auto_nick_complete");
	hx_timeformat = xstrdup(DEFAULT_TIMEFORMAT);
	variable_add(&hx_timeformat, set_string, "timeformat");
	g_encoding = xstrdup(DEFAULT_ENCODING);
	variable_add(&g_encoding, set_string, "encoding");
	g_server_encoding = xstrdup(DEFAULT_SERVER_ENCODING);
	variable_add(&g_server_encoding, set_string, "server_encoding");
	variable_add(&g_clientversion, set_int16, "clientversion");
	variable_add(&g_default_secure, set_int16, "default_secure");
}

void
expand_tilde (char *buf, const char *str)
{
	const char *p = str;
	int len;

	if (*p == '~') {
		p++;
		if (*p == '/' || !*p) {
			len = strlen(hx_homedir);
			strcpy(&(buf[len]), p);
			memcpy(buf, hx_homedir, len);
		} else {
#if !defined(__WIN32__)
			char *rest;
			struct passwd *pwe;

			if ((rest = strchr(p, '/')))
				*rest++ = 0;
			if ((pwe = getpwnam(p))) {
				len = strlen(pwe->pw_dir);
				if (rest) {
					strcpy(&(buf[len + 1]), rest);
					buf[len] = '/';
				}
				memcpy(buf, pwe->pw_dir, len);
			}
#endif
		}
	} else
		strcpy(buf, str);
}

static int
last_msg (char *buffer, int point)
{
	char buf[128], nick[128];
	int len;

	if (!*last_msg_nick)
		return point;

	strunexpand(last_msg_nick, strlen(last_msg_nick), nick, sizeof(nick));
	len = sprintf(buf, "/msg %.120s ", nick);
	buffer[strlen(buffer)+len] = 0;
	memmove(buffer+len, buffer, strlen(buffer));
	memcpy(buffer, buf, len);

	return len + point;
}

static void
similar_str (const char *txt1, int len1, const char *txt2, int len2,
	     int *pos1, int *pos2, int *max)
{
	char *p, *q;
	char *end1 = (char *) txt1 + len1;
	char *end2 = (char *) txt2 + len2;
	int l;

	*max = 0;
	for (p = (char *) txt1; p < end1; p++) {
		for (q = (char *) txt2; q < end2; q++) {
			for (l = 0; (p + l < end1) && (q + l < end2) && (tolower(p[l]) == tolower(q[l]));
				l++);
			if (l > *max) {
				*max = l;
				*pos1 = p - txt1;
				*pos2 = q - txt2;
			}
		}
	}
}

static int
similar (const char *txt1, int len1, const char *txt2, int len2)
{
	int sum;
	int pos1, pos2, max;

	similar_str(txt1, len1, txt2, len2, &pos1, &pos2, &max);
	if ((sum = max)) {
		if (pos1 && pos2)
			sum += similar(txt1, pos1, txt2, pos2);
		if ((pos1 + max < len1) && (pos2 + max < len2))
			sum += similar(txt1 + pos1 + max, len1 - pos1 - max,
				       txt2 + pos2 + max, len2 - pos2 -max);
	}

	return sum;
}

static int
nick_complete (struct htlc_conn *htlc, char *buffer)
{
	char nick[64];
	char *matching_nick, *bp, *cp;
	struct hx_chat *chat;
	struct hx_user *userp, *best_user;
	int i, best, cur, real_rllen, rllen, nlen, maxlen;
	char *p;

	for (p = buffer; *p; p++) {
		if (*p == ' ' && *(p-1) != '\\')
			return 0;
	}
	matching_nick = nick;
	real_rllen = strlen(buffer);
	rllen = real_rllen;
	best = 0;
	best_user = 0;
	maxlen = 0;
	chat = hx_chat_with_cid(htlc, 0);
	for (userp = chat->user_list->next; userp; userp = userp->next) {
		nlen = strlen(userp->name);
		if (nlen < real_rllen)
			rllen = nlen;
		else
			nlen = rllen = real_rllen;
		cur = similar(buffer, rllen, userp->name, nlen);
		if (!cur)
			continue;
		if (cur > best) {
			best = cur;
			best_user = userp;
			maxlen = strlen(best_user->name);
		} else if (cur == best) {
			bp = best_user->name;
			cp = userp->name;
			maxlen = 0;
			while (*bp && *bp == *cp) {
				bp++;
				cp++;
				maxlen++;
			}
		}
	}

	if (!best_user)
		return 0;
	strncpy(matching_nick, best_user->name, maxlen);
	matching_nick[maxlen] = 0;
	while (matching_nick[0] == ' ')
		matching_nick++;

	for (i = maxlen; i >= 0; i--) {
		if (matching_nick[i-1] != ' ')
			break;
		else
			matching_nick[i-1] = '\0';    
	}

	strcat(matching_nick, ": ");
	strcpy(buffer, matching_nick);

	return strlen(matching_nick);
}

char **
filename_complete (char *text)
{
	DIR *dir;
	struct dirent *de;
	char *filename;
	char *dirname = 0;
	char *users_dirname = 0;
	size_t filename_len;
	size_t d_namlen;
	size_t dirlen;
	char *temp;
	char **paths = 0;
	int npaths = 0;

	filename = xstrdup(text);
	if (*text == 0)
		text = ".";
	dirname = xstrdup(text);

	temp = strrchr(dirname, '/');
	if (temp) {
		strcpy(filename, ++temp);
		*temp = '\0';
	} else {
		dirname[0] = '.';
		dirname[1] = '\0';
	}

	/* Save the version of the dir that the user typed. */
	users_dirname = xstrdup(dirname);
	if (*dirname == '~') {
		temp = xmalloc(MAXPATHLEN);
		expand_tilde(temp, dirname);
		xfree(dirname);
		dirname = temp;
	}

	filename_len = strlen(filename);
	dir = opendir(dirname);
	if (!dir)
		goto ret_nodir;

	while ((de = readdir(dir))) {
		d_namlen = strlen(de->d_name);
		/* Special case for no filename.
		   All entries except "." and ".." match. */
		if (filename_len == 0) {
			if (de->d_name[0] != '.' ||
			    (de->d_name[1] &&
			    (de->d_name[1] != '.' || de->d_name[2])))
			goto match;
		} else {
			/* Otherwise, if these match up to the length of filename, then
			   it is a match. */
			if ((de->d_name[0] == filename[0]) &&
			    (d_namlen >= filename_len)
			    && (strncmp (filename, de->d_name, filename_len) == 0))
				goto match;
		}
		continue;
match:
		/* dirname && (strcmp (dirname, ".") != 0) */
		if (dirname && (dirname[0] != '.' || dirname[1])) {
			if (*users_dirname == '~') {
				dirlen = strlen(dirname);
				temp = xmalloc(2 + dirlen + d_namlen);
				strcpy(temp, dirname);
				/* Canonicalization cuts off any final slash present.  We
				   may need to add it back. */
				if (dirname[dirlen - 1] != '/') {
					temp[dirlen++] = '/';
					temp[dirlen] = '\0';
				} else {
					dirlen = strlen(users_dirname);
					temp = xmalloc(1 + dirlen + d_namlen);
					strcpy(temp, users_dirname);
				}
				strcpy(temp + dirlen, de->d_name);
			} else {
				dirlen = strlen(users_dirname);
				temp = xmalloc(1 + dirlen + d_namlen);
				strcpy(temp, users_dirname);
				strcpy(temp+dirlen, de->d_name);
			}
		} else {
			temp = xstrdup(de->d_name);
		}
		paths = xrealloc(paths, sizeof(char *) * (npaths+1));
		paths[npaths] = temp;
		npaths++;
	}
	paths = xrealloc(paths, sizeof(char *) * (npaths+5));
	paths[npaths] = 0;

	closedir(dir);

ret_nodir:
	if (dirname)
		xfree(dirname);
	if (filename)
		xfree(filename);
	if (users_dirname)
		xfree(users_dirname);

	return paths;
}

static char *
expand_user_name (struct htlc_conn *htlc, char *name, unsigned int nlen)
{
	struct hx_chat *chat;
	struct hx_user *userp, *user = 0;

	chat = hx_chat_with_cid(htlc, 0);
	for (userp = chat->user_list->next; userp; userp = userp->next) {
		if (!strncmp(userp->name, name, nlen)) {
			if (user)
				return 0;
			user = userp;
		}
	}

	return user ? user->name : 0;
}

int
hotline_client_tab (struct htlc_conn *htlc, struct hx_chat *chat,
		    char *buffer, int point)
{
	if (*buffer == '/') {
		char cmdbuf[16], *cmd = buffer + 1;
		int i, l, srlp, len;

		for (l = 0; cmd[l]; l++) {
			if (cmd[l] == ' ')
				break;
		}
		i = expand_command(cmd, l, 1+cmdbuf);
		if (i >= 0 && ((int)strlen(1+cmdbuf) != l || !cmd[l])) {
			srlp = point;
			cmdbuf[0] = '/';
			if (!i) {
				i = strlen(cmdbuf);
				cmdbuf[i] = ' ';
				i++;
				cmdbuf[i] = 0;
				if (cmd[l] && cmd[l+1])
					l++;
			} else {
				i = strlen(cmdbuf);
			}
			len = strlen(buffer+1+l);
			memmove(buffer+i, buffer+1+l, len);
			buffer[i+len] = 0;
			memcpy(buffer, cmdbuf, i);
			point = srlp + ((i - 1) - l);
		}
		cmd = buffer + 1;
		if (!(cmd_arg(1, cmd) & 0xffff))
			return point;
		if (!strncmp(cmd, "cd", 2)
		    || !strncmp(cmd, "ln", 2)
		    || !strncmp(cmd, "ls", 2)
		    || !strncmp(cmd, "mv", 2)
		    || !strncmp(cmd, "rm", 2)
		    || (!strncmp(cmd, "get", 3) && cmd[3] == ' ')
		    || !strncmp(cmd, "hash", 4)
		    || !strncmp(cmd, "finfo", 5)) {
			int argn, ambig;
			u_int16_t off, len, orig_len;
			u_int32_t ol;
			char path[MAXPATHLEN], xxxbuf[0x1000];

			argn = 1;
			while ((ol = cmd_arg(argn++, cmd))) {
				off = ol >> 16, orig_len = ol & 0xffff;
				if ((point-1) >= off && (point-1) <= off+orig_len)
					goto ok;
			}
			return point;
ok:
			orig_len &= 0x0fff;
			memcpy(xxxbuf, cmd+off, orig_len);
			len = strexpand(xxxbuf, orig_len);
			ambig = expand_path(htlc, chat, xxxbuf, len, path);
			if (ambig >= 0) {
				char *p, buf[MAXPATHLEN * 3 + 8];

				p = buf;
				len = strunexpand(path, strlen(path), p, sizeof(buf) - (p - buf));
				if (!ambig) {
					p += len;
					*p++ = ' ';
					*p = 0;
					len++;
				}
				if (len > orig_len)
					memmove(buffer+1+off+orig_len, buffer+1+off+len, len-orig_len);
				memcpy(buffer+1+off, buf, len);
				buffer[1+off+len] = 0;
				point = 1 + off + len;
			}
		} else if (!strncmp(cmd, "msg", 3)
			 || !strncmp(cmd, "info", 4)
			 || !strncmp(cmd, "kick", 4)
			 || !strncmp(cmd, "chat", 4)
			 || !strncmp(cmd, "invite", 3)
			 || !strncmp(cmd, "ignore", 2)
			 || !strncmp(cmd, "unignore", 2)) {
			u_int32_t ol = cmd_arg(1, cmd);
			int off = ol >> 16, orig_len = ol & 0xffff;
			char *n, xxxbuf[128];

			if (!off)
				return point;
			len = orig_len & 0x7f;
			memcpy(xxxbuf, cmd+off, len);
			len = strexpand(xxxbuf, len);
			n = expand_user_name(htlc, xxxbuf, len);
			if (n) {
				char *p, buf[120];

				p = buf + sprintf(buf, "/%.*s ", off-1, cmd);
				len = strunexpand(n, strlen(n), p, sizeof(buf) - (p - buf));
				p += strlen(p);
				*p++ = ' ';
				*p = 0;
				point = p - buf;
				buffer[strlen(buffer+1+off+orig_len)+point] = 0;
				memmove(buffer+point, buffer+1+off+orig_len, strlen(buffer+1+off+orig_len));
				memcpy(buffer, buf, point);
			}
		} else if (!strncmp(cmd, "exec", 4)
			   || !strncmp(cmd, "lcd", 3)
			   || !strncmp(cmd, "load", 4)
			   || !strncmp(cmd, "put", 3)
			   || !strncmp(cmd, "type", 4)) {
			int argn, ambig;
			u_int16_t off, len, orig_len;
			u_int32_t ol;
			char path[MAXPATHLEN], xxxbuf[0x1000];
			char **paths, **pathsp, *pp, *p;

			argn = 1;
			while ((ol = cmd_arg(argn++, cmd))) {
				off = ol >> 16, orig_len = ol & 0xffff;
				if ((point-1) >= off && (point-1) <= off+orig_len)
					goto l_ok;
			}
			return point;
l_ok:
			orig_len &= 0x0fff;
			memcpy(xxxbuf, cmd+off, orig_len);
			len = strexpand(xxxbuf, orig_len);
			xxxbuf[len] = 0;
			paths = filename_complete(xxxbuf);
			ambig = -1;
			path[0] = 0;
			if (paths) {
				for (pathsp = paths; *pathsp; pathsp++) {
					if (!path[0]) {
						strcpy(path, *pathsp);
						ambig = 0;
					} else
						ambig = 1;
					for (pp = path, p = *pathsp; *p; p++, pp++) {
						if (*pp != *p) {
							*pp = 0;
							break;
						}
					}
				}
			}
			if (ambig >= 0) {
				char buf[MAXPATHLEN * 3 + 8];

				p = buf;
				len = strunexpand(path, strlen(path), p, sizeof(buf) - (p - buf));
				if (!ambig) {
					p += len;
					*p++ = ' ';
					*p = 0;
					len++;
				}
				if (len > orig_len)
					memmove(buffer+1+off+orig_len, buffer+1+off+len, len-orig_len);
				memcpy(buffer+1+off, buf, len);
				buffer[1+off+len] = 0;
				point = 1 + off + len;
			}
		}
	} else if (!buffer[0]) {
		point = last_msg(buffer, point);
	} else {
		point = nick_complete(htlc, buffer);
	}

	return point;
}

void
hotline_client_input (struct htlc_conn *htlc, struct hx_chat *chat, char *str)
{
	if (*str) {
		if (*str == '/' && *++str && *str != '/') {
			hx_command(htlc, chat, str);
		} else {
			if (g_auto_nick_complete) {
				size_t len;
				char *p;

				p = strchr(str, ':');
				if (p && (len=p-str) < 32 && *(p+1)==' ') {
					size_t nlen, clen;
					char *s, nick[48];

					memcpy(nick, str, len);
					nick[len] = 0;
					nlen = nick_complete(htlc, nick);
					if (!nlen)
						goto normal;
					p += 2;
					clen = strlen(p);
					s = xmalloc(clen+nlen+1);
					memcpy(s, nick, nlen);
					memcpy(s+nlen, p, clen);
					s[clen+nlen] = 0;
					if (chat)
						hx_send_chat(htlc, chat->cid, s, clen+nlen, 0);
					else
						hx_send_chat(htlc, 0, s, clen+nlen, 0);
					xfree(s);
					return;
				}
			}
normal:
			if (chat)
				hx_send_chat(htlc, chat->cid, str, strlen(str), 0);
			else
				hx_send_chat(htlc, 0, str, strlen(str), 0);
		}
	}
}

void
chrexpand (char *str, int len)
{
	char *p;
	int off;

	p = str;
	if (*p != '\\')
		return;
	off = 1;
	switch (p[1]) {
		case 'r':
			p[1] = '\r';
			break;
		case 'n':
			p[1] = '\n';
			break;
		case 't':
			p[1] = '\t';
			break;
		case 'x':
			while (isxdigit(p[off+1]) && off < 3)
				off++;
			p[off] = (char)strtoul(p+2, 0, 16);
			break;
		default:
			if (!isdigit(p[1]) || p[1] >= '8')
				break;
			while ((isdigit(p[off+1]) && p[off+1] < '8') && off < 3)
				off++;
			p[off] = (char)strtoul(p+2, 0, 8);
			break;
	}
	len -= off;
	memcpy(p, p+off, len);
	p[len] = 0;
}

int
strexpand (char *str, int len)
{
	char *p;
	int off;

	for (p = str; p < str+len; p++) {
		if (*p != '\\')
			continue;
		off = 1;
		switch (p[1]) {
			case 'r':
				p[1] = '\r';
				break;
			case 'n':
				p[1] = '\n';
				break;
			case 't':
				p[1] = '\t';
				break;
			case 'x':
				while (isxdigit(p[off+1]) && off < 3)
					off++;
				p[off] = (char)strtoul(p+2, 0, 16);
				break;
			default:
				if (!isdigit(p[1]) || p[1] >= '8')
					break;
				while ((isdigit(p[off+1]) && p[off+1] < '8') && off < 3)
					off++;
				p[off] = (char)strtoul(p+2, 0, 8);
				break;
		}
		len -= off;
		memcpy(p, p+off, str+len - p);
		str[len] = 0;
	}

	return len;
}

int
strunexpand (char *str, int slen, char *buf, int blen)
{
	char *p, *bp, c;

	for (p = str, bp = buf; p < str+slen && bp < buf+blen; p++, bp++) {
		if (isgraph(*p) && (*p != '\'' && *p != '"' && *p != '?' && *p != '*')) {
			*bp = *p;
			continue;
		}
		*bp++ = '\\';
		switch (*p) {
			case '\r':
				*bp = 'r';
				break;
			case '\n':
				*bp = 'n';
				break;
			case '\t':
				*bp = 't';
				break;
			case ' ':
			case '\'':
			case '"':
			case '*':
			case '?':
				*bp = *p;
				break;
			default:
				*bp++ = 'x';
				c = (*p >> 4) & 0xf;
				c = c > 9 ? c - 0xa + 'a' : c + '0';
				*bp++ = c;
				c = *p & 0xf;
				c = c > 9 ? c - 0xa + 'a' : c + '0';
				*bp = c;
		}
	}
	if (bp == buf+blen)
		bp--;
	*bp = 0;
	blen = bp - buf;

	return blen;
}

/* fileutils-4.0/lib/human.c */
static const char human_suffixes[] = {
	'B',	/* Bytes */
	'K',	/* Kilo */
	'M',	/* Mega */
	'G',	/* Giga */
	'T',	/* Tera */
	'P',	/* Peta */
	'E',	/* Exa */
	'Z',	/* Zetta */
	'Y'	/* Yotta */
};

/* Convert N to a human readable format in BUF.

   N is expressed in units of FROM_BLOCK_SIZE.  FROM_BLOCK_SIZE must
   be positive.

   If OUTPUT_BLOCK_SIZE is positive, use units of OUTPUT_BLOCK_SIZE in
   the output number.  OUTPUT_BLOCK_SIZE must be a multiple of
   FROM_BLOCK_SIZE or vice versa.

   If OUTPUT_BLOCK_SIZE is negative, use a format like "127k" if
   possible, using powers of -OUTPUT_BLOCK_SIZE; otherwise, use
   ordinary decimal format.  Normally -OUTPUT_BLOCK_SIZE is either
   1000 or 1024; it must be at least 2.  Most people visually process
   strings of 3-4 digits effectively, but longer strings of digits are
   more prone to misinterpretation.  Hence, converting to an
   abbreviated form usually improves readability.  Use a suffix
   indicating which power is being used.  For example, assuming
   -OUTPUT_BLOCK_SIZE is 1024, 8500 would be converted to 8.3k,
   133456345 to 127M, 56990456345 to 53G, and so on.  Numbers smaller
   than -OUTPUT_BLOCK_SIZE aren't modified.  */

static char *
human_readable (u_int32_t n, char *buf,
		int from_block_size, int output_block_size)
{
  u_int32_t amt;
  unsigned int base;
  int to_block_size;
  unsigned int tenths;
  unsigned int power = 0;
  char *p;

  /* 0 means adjusted N == AMT.TENTHS;
     1 means AMT.TENTHS < adjusted N < AMT.TENTHS + 0.05;
     2 means adjusted N == AMT.TENTHS + 0.05;
     3 means AMT.TENTHS + 0.05 < adjusted N < AMT.TENTHS + 0.1.  */
  unsigned int rounding;

  if (output_block_size < 0)
    {
      base = -output_block_size;
      to_block_size = 1;
    }
  else
    {
      base = 0;
      to_block_size = output_block_size;
    }

  p = buf + LONGEST_HUMAN_READABLE;
  *p = '\0';

  /* Adjust AMT out of FROM_BLOCK_SIZE units and into TO_BLOCK_SIZE units.  */

  if (to_block_size <= from_block_size)
    {
      int multiplier = from_block_size / to_block_size;
      amt = n * multiplier;
      tenths = rounding = 0;

      if (amt / multiplier != n)
	{
	  /* Overflow occurred during multiplication.  We should use
	     multiple precision arithmetic here, but we'll be lazy and
	     resort to floating point.  This can yield answers that
	     are slightly off.  In practice it is quite rare to
	     overflow uintmax_t, so this is good enough for now.  */

	  double damt = n * (double) multiplier;

	  if (! base)
	    sprintf (buf, "%.0f", damt);
	  else
	    {
	      double e = 1;
	      power = 0;

	      do
		{
		  e *= base;
		  power++;
		}
	      while (e * base <= damt && power < sizeof(human_suffixes) - 1);

	      damt /= e;

	      sprintf (buf, "%.1f%c", damt, human_suffixes[power]);
	      if (4 < strlen (buf))
		sprintf (buf, "%.0f%c", damt, human_suffixes[power]);
	    }

	  return buf;
	}
    }
  else
    {
      unsigned int divisor = to_block_size / from_block_size;
      unsigned int r10 = (n % divisor) * 10;
      unsigned int r2 = (r10 % divisor) * 2;
      amt = n / divisor;
      tenths = r10 / divisor;
      rounding = r2 < divisor ? 0 < r2 : 2 + (divisor < r2);
    }


  /* Use power of BASE notation if adjusted AMT is large enough.  */

  if (base && base <= amt)
    {
      power = 0;

      do
	{
	  unsigned int r10 = (amt % base) * 10 + tenths;
	  unsigned int r2 = (r10 % base) * 2 + (rounding >> 1);
	  amt /= base;
	  tenths = r10 / base;
	  rounding = (r2 < base
		      ? 0 < r2 + rounding
		      : 2 + (base < r2 + rounding));
	  power++;
	}
      while (base <= amt && power < sizeof(human_suffixes) - 1);

      *--p = human_suffixes[power];

      if (amt < 10)
	{
	  tenths += 2 < rounding + (tenths & 1);

	  if (tenths == 10)
	    {
	      amt++;
	      tenths = 0;
	    }

	  if (amt < 10)
	    {
	      *--p = '0' + tenths;
	      *--p = '.';
	      tenths = 0;
	    }
	}
    } else if (base > amt) {
		*--p = human_suffixes[0];
	}

  if (5 < tenths + (2 < rounding + (amt & 1)))
    {
      amt++;

      if (amt == base && power < sizeof(human_suffixes) - 1)
	{
	  *p = human_suffixes[power + 1];
	  *--p = '0';
	  *--p = '.';
	  amt = 1;
	}
    }

  do
    *--p = '0' + (int) (amt % 10);
  while ((amt /= 10) != 0);

  return p;
}

char *
human_size (u_int32_t size, char *buf)
{
	return human_readable(size, buf, 1, -1024);
}

void
hx_exit (int stat)
{
	hx_savevars();
	hx_output.cleanup();
	exit(stat);
}

extern struct output_functions hx_gtk_output;
extern struct output_functions hx_tty_output;

struct output_functions hx_output;

void
hotline_client_init (int argc, char **argv)
{
	char *home, *user;
	struct passwd *pwe;
	int load_hxrc = 1;
	char *load_this = 0;
	struct opt_r opt;
	int i;
	char *hsp;

	opt.err_printf = hxd_log;
	opt.ind = 0;
	while ((i = getopt_r(argc, argv, "dql:", &opt)) != EOF) {
		switch (i) {
			/* dumb tty option: handled by tty module */
			case 'd':
				break;
			case 'l':
				load_this = opt.arg;
				break;
			case 'q':
				load_hxrc = 0;
				break;
			default:
				break;
		}
	}

	(void)setlocale(LC_ALL, "");

	hsp = getenv("HISTSIZE");
	if (hsp)
		hist_size = strtoul(hsp, 0, 0);
	else
		hist_size = 500;

	home = getenv("HOME");
	user = getenv("USER");
	if (!home || !user) {
#if defined(__WIN32__)
		home = "C:\\";
		user = "hx";
#else
		pwe = getpwuid(getuid());
		if (!pwe) {
			hxd_log("getpwuid: %s", strerror(errno));
		} else {
			if (!home)
				home = pwe->pw_dir;
			if (!user)	
				user = pwe->pw_name;
		}
#endif
	}
	strlcpy(hx_homedir, home ? home : ".", sizeof(hx_homedir));
	strlcpy(hx_user, user ? user : ".", sizeof(hx_user));
	hx_user[sizeof(hx_user)-1] = 0;
#if defined(ONLY_GTK)
	strcpy(g_hxvars_path, "~/.ghxvars");
#else
	strcpy(g_hxvars_path, "~/.hxvars");
#endif

	memset(&hx_htlc, 0, sizeof(struct htlc_conn));
	mutex_init(&hx_htlc.htxf_mutex);
	hx_htlc.icon = 500;
	if (user)
		strlcpy(hx_htlc.name, user, 32);
	else
		strcpy(hx_htlc.name, "Evaluation 0wn3r");

	gen_command_hash();

	last_msg_nick[0] = 0;

	init_vars();

#if defined(ONLY_GTK)
	hx_output = hx_gtk_output;
#else
	hx_output = hx_tty_output;
#endif

	hx_output.init(argc, argv);

	hx_output.status();

	if (load_hxrc) {
		char path[MAXPATHLEN];

		if (!access(SYSTEM_HXRC_PATH, R_OK))
			hx_load(&hx_htlc, 0, SYSTEM_HXRC_PATH);
		if (!load_this) {
#if defined(ONLY_GTK)
			expand_tilde(path, "~/.ghxrc");
#else
			expand_tilde(path, "~/.hxrc");
#endif
			if (!access(path, R_OK))
				hx_load(&hx_htlc, 0, path);
		} else
			hx_load(&hx_htlc, 0, load_this);
		expand_tilde(path, g_hxvars_path);
		if (!access(path, R_OK))
			hx_load(&hx_htlc, 0, path);
	}
}
