#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#if !defined(__WIN32__)
#include <termios.h>
#include <sys/ioctl.h>
#endif
#include <ctype.h>
#include "hxd.h"
#include "hx.h"
#include "xmalloc.h"
#include "getopt.h"

#if defined(CONFIG_ICONV)
#include "conv.h"
#endif

struct screen {
	char *buf;
	size_t size;
	int li, page_lines;
};

static struct screen __scr = {0, 0, 0, 0}, *scr_front = &__scr;

static int last_co = 0;

#if defined(ONLY_GTK) || defined(__WIN32__)
#undef USE_READLINE
#else
#define USE_READLINE	1
#endif

#if defined(__WIN32__) && !defined(ONLY_GTK)
void
hx_printf (struct htlc_conn *htlc, struct hx_chat *chat, const char *fmt, ...)
{
}
void
hx_printf_prefix (struct htlc_conn *htlc, struct hx_chat *chat, const char *prefix, const char *fmt, ...)
{
}
char *
colorstr (u_int16_t color)
{
	char *col;

	col = g_user_colors[color % 4];

	return col;
}
void
hx_save (struct htlc_conn *htlc, struct hx_chat *chat, const char *filename)
{
	int f;
	ssize_t r;
	size_t pos, len;
	char path[MAXPATHLEN];

	expand_tilde(path, filename);
	f = SYS_open(path, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
	if (f < 0) {
		hx_printf(htlc, chat, "save: %s: %s\n", path, strerror(errno));
		return;
	}
	pos = 0;
	len = scr_front->size;
	while (len) {
		r = write(f, scr_front->buf + pos, len);
		if (r <= 0)
			break;
		pos += r;
		len -= r;
	}
	SYS_fsync(f);
	close(f);
	hx_printf_prefix(htlc, chat, INFOPREFIX, "%d bytes written to %s\n", pos, path);
}
#endif

struct hx_chat *tty_chat_front = 0;

extern int hx_multikick (struct htlc_conn *htlc, const char *nam, u_int32_t uid);

#if USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>

#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#if !defined(__NetBSD__)
#define HAVE_TERMCAP_PC_BC_UP 1
#endif
#endif

#ifndef HAVE_TCGETATTR
#define tcgetattr(fd,tty)
#endif
#ifndef HAVE_TCSETATTR
#define tcsetattr(fd,opt,tty)
#endif

#define tty_fd 0

int LI = 24, CO = 80;
char term_name[256] = "vt100", *HO, *ME, *MR, *US, *CL, *CE, *CM, *TCS, *SF, *SR, *DC, *IC;
static char termcap[2048], tbuf[2048];
static struct termios old_tty, new_tty;
static int dumb = 0;

static char term_flush_buf[32];
static int term_flush_len = 0;

static int status_lines;
static int line_input;
static int line_status;
static int line_output;

char *
colorstr (u_int16_t color)
{
	char *col;

	col = g_user_colors[color % 4];

	return col;
}

static void
term_flush (void)
{
	write(1, term_flush_buf, term_flush_len);
	term_flush_len = 0;
}

static int
term_putc (int c)
{
	if (term_flush_len == sizeof(term_flush_buf)) {
		term_flush();
		term_flush_len = 0;
	}
	term_flush_buf[term_flush_len] = (char)c;
	term_flush_len++;

	return c;
}

static void
term_reset (void)
{
	if (dumb)
		return;
	tcsetattr(tty_fd, TCSADRAIN, &old_tty);
	tputs(tgoto(TCS, -1, 0), 0, term_putc);
	tputs(tgoto(CM, 0, LI), 0, term_putc);
	term_flush();
}

static void
term_goto_line (int line)
{
	if (dumb)
		return;
	tputs(tgoto(CM, rl_point % CO, line), 0, term_putc);
	term_flush();
}

static void
term_ce (void)
{
	if (dumb)
		return;
	tputs(CE, 0, term_putc);
	term_flush();
}

static void
term_mode_clear (void)
{
	if (dumb)
		return;
	tputs(ME, 0, term_putc);
	term_flush();
}

static void
term_mode_underline (void)
{
	if (dumb)
		return;
	tputs(US, 0, term_putc);
	term_flush();
}

static void term_status (void);

static char current_time_str[16];

static int
term_clock_timer (char *time_str)
{
	time_t t;
	struct tm tm;

	time(&t);
	if (localtime_r(&t, &tm))
		strftime(time_str, sizeof(current_time_str), "%H:%M", &tm);
	term_status();

	timer_add_secs(60 - tm.tm_sec, term_clock_timer, time_str);

	return 0;
}

static void
term_status (void)
{
	int len;
	char buf[256];
	char addr[HOSTLEN+1];
	char *col;

	if (dumb)
		return;
	col = colorstr(hx_htlc.color);
	tputs(tgoto(CM, 0, line_status), 0, term_putc);
	term_flush();
	inaddr2str(addr, &hx_htlc.sockaddr);
#if 0
	len = sprintf(buf, "[ %s ... hx version %s]\n[%s%s%s<%u>(%u) @ %s:%u]",
		current_time_str, version,
		col, hx_htlc.name, DEFAULT, hx_htlc.icon, hx_htlc.uid,
		addr, ntohs(hx_htlc.sockaddr.SIN_PORT));
#else
	len = sprintf(buf, "%s[%s HX] %s<%u>(%u) @ %s:%u",
		      BLUE_BACK, getenv("USER"), hx_htlc.name, hx_htlc.icon, 
		      hx_htlc.uid, addr, ntohs(hx_htlc.sockaddr.SIN_PORT));
	len += sprintf(buf+len, "%*s%s", CO-len+6, " ", DEFAULT);
#endif
	write(1, buf, len);
	tputs(CE, 0, term_putc);
	tputs(tgoto(CM, rl_point % CO, line_input), 0, term_putc);
	term_flush();
}

static RETSIGTYPE
term_sigwinch (int sig __attribute__((__unused__)))
{
	struct winsize win_size;
	char LINES[16], COLUMNS[16];

	if (ioctl(1, TIOCGWINSZ, &win_size))
		goto ret;

	LI = win_size.ws_row;
	CO = win_size.ws_col;
	line_input = LI - 1;
	line_status = LI - (status_lines + 1);
	line_output = LI - (status_lines + 2);
#if defined(HAVE_PUTENV)
	sprintf(LINES, "LINES=%.4u", win_size.ws_row);
	putenv(LINES);
	sprintf(COLUMNS, "COLUMNS=%.4u", win_size.ws_col);
	putenv(COLUMNS);
#endif

	tputs(tgoto(TCS, line_output, 0), 0, term_putc);
	term_flush();
ret:
	{}
}

static RETSIGTYPE
term_sigtstp (int sig __attribute__((__unused__)))
{
	tcgetattr(tty_fd, &new_tty);
	term_reset();
	write(1, "\r", 1);
	raise(SIGSTOP);
}

extern RETSIGTYPE sig_chld (int sig);

static RETSIGTYPE
term_sigcont (int sig __attribute__((__unused__)))
{
	tcsetattr(tty_fd, TCSADRAIN, &new_tty);
	tputs(tgoto(TCS, line_output, 0), 0, term_putc);
	term_status();
	tputs(tgoto(CM, 0, line_input), 0, term_putc);
	term_flush();
	rl_on_new_line();
	rl_redisplay();
	sig_chld(0);
}

static void
term_install_signal_handlers (void)
{
	struct sigaction act;

	act.sa_flags = 0;
	sigfillset(&act.sa_mask);

	act.sa_handler = term_sigcont;
	sigaction(SIGINT, &act, 0);
	sigaction(SIGCONT, &act, 0);

	act.sa_handler = term_sigtstp;
	sigaction(SIGTSTP, &act, 0);

	act.sa_handler = term_sigwinch;
	sigaction(SIGWINCH, &act, 0);
}

static int
term_init ()
{
	char *ep, *bp = tbuf;
	int i;

	if ((ep = getenv("TERM")))
		strlcpy(term_name, ep, sizeof(term_name));
	if (dumb)
		goto want_dumb;
	i = tgetent(termcap, term_name);
	if (i <= 0) {
		if (i == -1) {
			write(1, "tgetent had trouble accessing the termcap database\n", 51);
		} else {
			write(1, "tgetent did not find an entry for '", 25);
			write(1, term_name, strlen(term_name));
			write(1, "'\n", 2);
		}
want_dumb:
		write(1, "running in dumb terminal mode\n", 31);
		goto ret;
	}
	if (!(ep = getenv("COLUMNS")) || !(CO = strtol(ep, 0, 0)))
		if ((CO = tgetnum("co")) == -1)
			CO = 80;
	if (!(ep = getenv("LINES")) || !(LI = strtol(ep, 0, 0)))
		if ((LI = tgetnum("li")) == -1)
			LI = 24;

	line_input = LI - 1;
	status_lines = 1;
	line_status = LI - (status_lines + 1);
	line_output = LI - (status_lines + 2);

	HO = tgetstr("ho", &bp);
	ME = tgetstr("me", &bp);
	MR = tgetstr("mr", &bp);
	US = tgetstr("us", &bp);
	CL = tgetstr("cl", &bp);
	CM = tgetstr("cm", &bp);
	CE = tgetstr("ce", &bp);
	TCS = tgetstr("cs", &bp);
	DC = tgetstr("dc", &bp);
	IC = tgetstr("ic", &bp);
	SF = tgetstr("sf", &bp);
	SR = tgetstr("sr", &bp);

#ifdef HAVE_TERMCAP_PC_BC_UP
	ep = tgetstr("pc", &bp);
	PC = ep ? *ep : 0;
	BC = tgetstr("le", &bp);
	UP = tgetstr("up", &bp);
#endif

	tcgetattr(tty_fd, &old_tty);
	new_tty = old_tty;
	new_tty.c_cc[VMIN] = 1;
	new_tty.c_cc[VTIME] = 0;
	new_tty.c_lflag &= ~ICANON;
	new_tty.c_iflag &= ~IXON;
	tcsetattr(tty_fd, TCSADRAIN, &new_tty);

	tputs(tgoto(TCS, line_output, 0), 0, term_putc);
	tputs(CL, 0, term_putc);
	term_clock_timer(current_time_str);

ret:
	term_install_signal_handlers();

	return tty_fd;
}

static void
term_puts (char *buf, size_t len)
{
	if (!dumb) {
		tputs(tgoto(CM, last_co, line_output), 0, term_putc);
		if (!last_co)
			term_putc('\n');
		term_flush();
		if (buf[len - 1] == '\n') {
			last_co = 0;
			len--;
		} else last_co += len;
	}
	write(1, buf, len);
	if (!dumb) {
		tputs(tgoto(CM, rl_point % CO, line_input), 0, term_putc);
		term_flush();
	}
}

static int
lines (char *buf, size_t size)
{
	char *p, *end = buf + size;
	int l = 0;

	for (p = buf; p < end; p++) {
		if (*p == '\n')
			l++;
	}

	return l;
}

static char *
line (struct screen *scr, int line)
{
	char *p, *end = scr->buf + scr->size;
	int l = 0;

	if (!line)
		return scr->buf;
	for (p = scr->buf; p < end; p++) {
		if (*p == '\n') {
			l++;
			if (l == line)
				return (p + 1) >= end ? 0 : (p + 1);
		}
	}
	if (l == line - 1)
		return p;

	return 0;
}

static void
draw (char *ln0, size_t len)
{
	tputs(CL, 0, term_putc);
	term_status();
	tputs(tgoto(CM, 0, 0), 0, term_putc);
	term_flush();
	write(1, ln0, len);
	tputs(tgoto(CM, 0, line_input), 0, term_putc);
	term_flush();
	rl_on_new_line();
	rl_redisplay();
}

static int
redraw ()
{
	struct screen *scr = scr_front;
	char *ln0 = 0, *ln1 = 0;

	if (!scr->size)
		goto cl;
	if (scr->li > scr->page_lines)
		ln0 = line(scr, (scr->li - scr->page_lines));
	else
		ln0 = scr->buf;
	ln1 = line(scr, scr->li + 1);
	if (!ln1)
		ln1 = scr->buf + scr->size;

cl:
	tputs(CL, 0, term_putc);
	term_status();
	tputs(tgoto(CM, 0, 0), 0, term_putc);
	term_flush();
	if (ln0)
		write(1, ln0, (ln1 - 1) - ln0);
	tputs(tgoto(CM, 0, line_input), 0, term_putc);
	term_flush();
	rl_on_new_line();
	rl_redisplay();

	return 0;
}

static void
line_up ()
{
	struct screen *scr = scr_front;
	char *ln0, *ln1;
	int li0;

	if (scr->li < (scr->page_lines + 1))
		return;
	li0 = scr->li - (scr->page_lines + 1);
	ln0 = line(scr, li0);
	ln1 = line(scr, li0 + 1);
	if (!ln0 || !ln1)
		return;
	scr->li = li0 + scr->page_lines;
	tputs(tgoto(CM, 0, 0), 0, term_putc);
	tputs(SR, 0, term_putc);
	term_flush();
	write(1, ln0, (ln1 - 1) - ln0);
	tputs(tgoto(CM, rl_point % CO, line_input), 0, term_putc);
	term_flush();
}

static void
line_down ()
{
	struct screen *scr = scr_front;
	char *ln0, *ln1;
	int lis = lines(scr->buf, scr->size);

	if (scr->li + 1 > lis)
		return;
	ln0 = line(scr, scr->li);
	if (!ln0)
		return;
	ln1 = line(scr, scr->li + 1);
	if (!ln1)
		ln1 = scr->buf + scr->size;
	scr->li = scr->li + 1;
	if (scr->li > lis) {
		if (scr->li - 1 != lis)
			return;
		scr->li = lis;
	}
	tputs(tgoto(CM, 0, line_output), 0, term_putc);
	tputs(SF, 0, term_putc);
	term_flush();
	write(1, ln0, (ln1 - 1) - ln0);
	tputs(tgoto(CM, rl_point % CO, line_input), 0, term_putc);
	term_flush();
}

static void
page_up ()
{
	struct screen *scr = scr_front;
	char *ln0, *ln1;
	int li0;

	li0 = (scr->li > (scr->page_lines * 2)) ? (scr->li - (scr->page_lines * 2)) : 0;
	ln0 = line(scr, li0);
	ln1 = line(scr, li0 + scr->page_lines);
	if (!ln0 || !ln1)
		return;
	scr->li = li0 + scr->page_lines;
	draw(ln0, (ln1 - 1) - ln0);
}

static void
page_down ()
{
	struct screen *scr = scr_front;
	char *ln0, *ln1;
	int lis = lines(scr->buf, scr->size);

	if (scr->li + scr->page_lines > lis)
		ln0 = line(scr, lis - scr->page_lines);
	else
		ln0 = line(scr, scr->li);
	if (!ln0)
		return;
	ln1 = line(scr, scr->li + scr->page_lines);
	if (!ln1) {
		ln1 = scr->buf + scr->size;
		scr->li = lis;
	} else
		scr->li = scr->li + scr->page_lines;
	draw(ln0, (ln1 - 1) - ln0);
}

static void
home ()
{
	struct screen *scr = scr_front;
	char *ln0, *ln1;

	ln0 = line(scr, 0);
	ln1 = line(scr, scr->page_lines);
	if (!ln0 || !ln1)
		return;
	if (scr->li == scr->page_lines)
		return;
	scr->li = scr->page_lines;
	draw(ln0, (ln1 - 1) - ln0);
}

static void
end ()
{
	struct screen *scr = scr_front;
	char *ln0, *ln1;
	int lis = lines(scr->buf, scr->size);

	if (lis < scr->page_lines)
		return;
	ln0 = line(scr, lis - scr->page_lines);
	if (!ln0)
		return;
	ln1 = scr->buf + scr->size;
	scr->li = lis;
	draw(ln0, (ln1 - 1) - ln0);
}

void
hx_printf (struct htlc_conn *htlc, struct hx_chat *chat, const char *fmt, ...)
{
	va_list ap;
	va_list save;
	struct screen *scr = scr_front;
	char *buf;
	size_t mal_len;
	int len;

	__va_copy(save, ap);
	mal_len = 256;
	buf = scr->buf;
	for (;;) {
		buf = xrealloc(buf, scr->size + mal_len);
		va_start(ap, fmt);
		len = vsnprintf(buf + scr->size, mal_len, fmt, ap);
		va_end(ap);
		if (len != -1)
			break;
		__va_copy(ap, save);
		mal_len <<= 1;
	}
	scr->buf = buf;
	if (scr->page_lines != line_status)
		scr->page_lines = line_status;
	if (scr->li == lines(scr->buf, scr->size)) {
		scr->li = lines(scr->buf, scr->size + len);
		term_puts(buf + scr->size, len);
	}
	scr->size += len;
}

void
hx_printf_prefix (struct htlc_conn *htlc, struct hx_chat *chat, const char *prefix, const char *fmt, ...)
{
	va_list ap;
	va_list save;
	struct screen *scr = scr_front;
	char *buf;
	size_t plen, mal_len;
	int len;

	__va_copy(save, ap);
	if (prefix)
		plen = strlen(prefix);
	else
		plen = 0;
	mal_len = 256;
	buf = scr->buf;
	buf = xrealloc(buf, scr->size + plen + mal_len);
	if (prefix)
		strcpy(buf + scr->size, prefix);
	for (;;) {
		va_start(ap, fmt);
		len = vsnprintf(buf + scr->size + plen, mal_len - plen, fmt, ap);
		va_end(ap);
		if (len != -1)
			break;
		__va_copy(ap, save);
		mal_len <<= 1;
		buf = xrealloc(buf, scr->size + mal_len + plen);
	}
	len += plen;
	scr->buf = buf;
	if (scr->page_lines != line_status)
		scr->page_lines = line_status;
	if (scr->li == lines(scr->buf, scr->size)) {
		scr->li = lines(scr->buf, scr->size + len);
		term_puts(buf + scr->size, len);
	}
	scr->size += len;
}

static void
term_clear (struct htlc_conn *htlc, struct hx_chat *chat)
{
	if (scr_front->buf)
		xfree(scr_front->buf);
	memset(scr_front, 0, sizeof(struct screen));
	redraw();
}

void
hx_save (struct htlc_conn *htlc, struct hx_chat *chat, const char *filename)
{
	int f;
	ssize_t r;
	size_t pos, len;
	char path[MAXPATHLEN];

	expand_tilde(path, filename);
	f = SYS_open(path, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
	if (f < 0) {
		hx_printf(htlc, chat, "save: %s: %s\n", path, strerror(errno));
		return;
	}
	pos = 0;
	len = scr_front->size;
	while (len) {
		r = write(f, scr_front->buf + pos, len);
		if (r <= 0)
			break;
		pos += r;
		len -= r;
	}
	SYS_fsync(f);
	close(f);
	hx_printf_prefix(htlc, chat, INFOPREFIX, "%d bytes written to %s\n", pos, path);
}

static void
hotline_client_read_tty (int fd)
{
	char c;

	if (read(fd, &c, 1) <= 0) {
		FD_CLR(fd, &hxd_rfds);
		return;
	}
	rl_pending_input = c;
	rl_callback_read_char();
}

static void
term_rl_input (char *line)
{
	if (!line || *line==0)
		return;
	add_history(line);
	using_history();
	hotline_client_input(&hx_htlc, tty_chat_front, line);
	rl_point = 0;
	rl_end = 0;
	term_goto_line(line_input);
	term_ce();
}

static void
last_msg (void)
{
	char buf[128], nick[128];
	int len, srlp;

	if (!*last_msg_nick)
		return;

	strunexpand(last_msg_nick, strlen(last_msg_nick), nick, sizeof(nick));
	len = sprintf(buf, "/msg %.120s ", nick);
	srlp = rl_point;
	rl_point = 0;
	rl_insert_text(buf);
	rl_point = len + srlp;
}

static int
tab (int count, int key)
{
	char *buf = xmalloc(rl_end + 4096);
	int point;

	strcpy(buf, rl_line_buffer);
	point = hotline_client_tab(&hx_htlc, tty_chat_front, buf, rl_point);
	rl_delete_text(0, rl_end);
	rl_point = 0;
	rl_insert_text(buf);
	rl_point = point;
	xfree(buf);

	return 0;
}

static int redraw();
static void line_up(), line_down(), page_up(), page_down(), home(), end();

static void
term_rl_init (void)
{
	Keymap kmap = rl_get_keymap();

	rl_bind_key(0xc, redraw);
	rl_bind_key('\t', tab);
	rl_generic_bind(ISFUNC, "\033\t", (char *)last_msg, kmap);
	rl_generic_bind(ISFUNC, "\033[1~", (char *)home, kmap);
	rl_generic_bind(ISFUNC, "\033[2~", (char *)line_up, kmap);
	rl_generic_bind(ISFUNC, "\033[3~", (char *)line_down, kmap);
	rl_generic_bind(ISFUNC, "\033[4~", (char *)end, kmap);
	rl_generic_bind(ISFUNC, "\033[5~", (char *)page_up, kmap);
	rl_generic_bind(ISFUNC, "\033[6~", (char *)page_down, kmap);
}

static void
init (int argc, char **argv)
{
	int fd;
	struct opt_r opt;
	int i;

	opt.err_printf = hxd_log;
	opt.ind = 0;
	while ((i = getopt_r(argc, argv, "dql:", &opt)) != EOF) {
		switch (i) {
			case 'd':
				dumb = 1;
				break;
			/* hx options */
			case 'q':
			case 'l':
				break;
			default:
				break;
		}
	}

	fd = term_init();
	if (fd == -1)
		return;
	if (fd >= hxd_open_max) {
		hxd_log("%s:%d: %d >= hxd_open_max (%d)", __FILE__, __LINE__, fd, hxd_open_max);
		return;
	}
	hxd_files[fd].ready_read = hotline_client_read_tty;
	hxd_fd_set(fd, FDR);

	rl_readline_name = "hx";
	rl_callback_handler_install("", term_rl_input);

	stifle_history(hist_size);

	term_rl_init();
}

static void
cleanup ()
{
	term_reset();
}
#endif /* USE_READLINE */

static size_t
find_nick (char *chatbuf, char **chatstart, char *nickp)
{
	char *nick, *p;
	size_t nlen;

	p = strstr(chatbuf, " - ");
	if (!p) {
		p = strstr(chatbuf, ":  ");
		if (!p) {
			p = strstr(chatbuf, " < ");
			if (!p) {
				*chatstart = chatbuf;
				return 0;
			}
		}
	}
	*chatstart = p + 3;
	nick = chatbuf;
	while (*nick == ' ')
		nick++;
	nlen = p - nick;
	if (nlen > 31)
		nlen = 31;
	memcpy(nickp, nick, nlen);
	nickp[nlen] = 0;

	return nlen;
}

static void
output_chat_str (struct htlc_conn *htlc, struct hx_chat *chat, char *chatbuf, u_int16_t chatlen)
{
	char *p, *tmpstr, hlnick[8];
	int i;
	u_int32_t cid;

	if (chat)
		cid = chat->cid;
	else
		cid = 0;

#if defined(CONFIG_ICONV)
{
	char *out_p;
	size_t out_len;

	out_len = convbuf(g_encoding, g_server_encoding, chatbuf, chatlen, &out_p);
	if (!out_len)
		return;
	if (cid)
		hx_printf(htlc, chat, "0x%08x: %.*s\n", cid, out_len, out_p);
	else
		hx_printf(htlc, chat, "%.*s\n", out_len, out_p);
	xfree(out_p);
	return;
}
#endif

	if (!tty_chat_pretty) {
normal:
		if (cid)
			hx_printf(htlc, chat, "0x%08x: %s\n", cid, chatbuf);
		else
			hx_printf(htlc, chat, "%s\n", chatbuf);
		return;
	}

	chatbuf[chatlen] = '\0';
	if (!memcmp(chatbuf, " \xa5->", 4) || !memcmp(chatbuf, " ***", 4)) {
		p = &chatbuf[4];
		while (p[0] == ' ' && p[1] == ' ')
			p++;
		if (cid)
			hx_printf(htlc, chat, "0x%08x: %s\xf0%s%s\n", cid, ORANGE, DEFAULT, p);
		else
			hx_printf(htlc, chat, "%s\xf0%s%s\n", ORANGE, DEFAULT, p);
	} else {
		char nick[32];

		if (!find_nick(chatbuf, &tmpstr, nick))
			goto normal;
		for (i = 0; i < 3; i++)
			hlnick[i] = tolower(hx_htlc.name[i]);
		hlnick[3] = 0;
		if (strcasestr(tmpstr, hlnick)) {
			if (cid)
				hx_printf(htlc, chat, "0x%08x: %s<%s%s%s>%s %s\n",
					  cid, BLUE, WHITE_BOLD, nick, BLUE, DEFAULT, tmpstr);
			else
				hx_printf(htlc, chat, "%s<%s%s%s>%s %s\n",
					  BLUE, WHITE_BOLD, nick, BLUE, DEFAULT, tmpstr);
		} else {
			if (cid)
				hx_printf(htlc, chat, "0x%08x: %s<%s%s%s>%s %s\n",
					  cid, BLUE, WHITE, nick, BLUE, DEFAULT, tmpstr);
			else
				hx_printf(htlc, chat, "%s<%s%s%s>%s %s\n",
					  BLUE, WHITE, nick, BLUE, DEFAULT, tmpstr);
		}
	}
}

static void
output_chat (struct htlc_conn *htlc, u_int32_t cid, char *chatbuf, u_int16_t chatlen)
{
	char *lastp, *p;
	int len;
	struct hx_chat *chat;

	chat = hx_chat_with_cid(htlc, cid);
	p = lastp = chatbuf;
	for (len = 0; *p; p++, len++) {
		if (*p == '\n') {
			p++;
			output_chat_str(htlc, chat, lastp, len);
			lastp = p;
			len = 0;
		} 
	}
	if (p != lastp)
		output_chat_str(htlc, chat, lastp, len);
}

static void
chat_subject (struct htlc_conn *htlc, u_int32_t cid, const char *subject)
{
	struct hx_chat *chat;

	chat = hx_chat_with_cid(htlc, cid);
	hx_printf_prefix(htlc, chat, INFOPREFIX, "chat 0x%x subject: %s\n", cid, subject);
}

static void
chat_password (struct htlc_conn *htlc, u_int32_t cid, const u_int8_t *pass)
{
	struct hx_chat *chat;

	chat = hx_chat_with_cid(htlc, cid);
	hx_printf_prefix(htlc, chat, INFOPREFIX, "chat 0x%x password: %s\n", cid, pass);
}

static void
chat_invite (struct htlc_conn *htlc, u_int32_t cid, u_int32_t uid, const char *name)
{
	struct hx_chat *chat;

	chat = hx_chat_with_cid(htlc, cid);
	hx_printf_prefix(htlc, chat, INFOPREFIX, "[%s(%u)] invites you to chat 0x%08x\n", name, uid, cid);
}

#if USE_READLINE
static void
chat_delete (struct htlc_conn *htlc, struct hx_chat *chat)
{
	if (chat == tty_chat_front)
		tty_chat_front = hx_chat_with_cid(htlc, 0);
}
#endif

static void
output_msg (struct htlc_conn *htlc, u_int32_t uid, const char *name, const char *msgbuf, u_int16_t msglen)
{
	char *col;
	struct hx_user *user;
	struct hx_chat *chat;
#if defined(CONFIG_ICONV)
	char *out_p;
	size_t out_len;
#endif

	chat = hx_chat_with_cid(htlc, 0);
	user = hx_user_with_uid(chat->user_list, uid);
	if (user) {
		col = colorstr(user->color);
	} else {
		name = "SERVER";
		col = colorstr(2);
	}
#if defined(CONFIG_ICONV)
	out_len = convbuf(g_encoding, g_server_encoding, msgbuf, msglen, &out_p);
	if (out_len) {
		hx_printf(htlc, 0, "%s[%s%s%s(%s%u%s)]%s: %.*s\n",
			  WHITE, col, name, WHITE, ORANGE,
			  uid, WHITE, DEFAULT, out_len, out_p);
		xfree(out_p);
	} else
#endif
	hx_printf(htlc, 0, "%s[%s%s%s(%s%u%s)]%s: %.*s\n",
		  WHITE, col, name, WHITE, ORANGE,
		  uid, WHITE, DEFAULT, msglen, msgbuf);
}

static void
output_agreement (struct htlc_conn *htlc, const char *agreement, u_int16_t len)
{
#if defined(CONFIG_ICONV)
	char *out_p;
	size_t out_len;

	out_len = convbuf(g_encoding, g_server_encoding, agreement, len, &out_p);
	if (out_len) {
		hx_printf_prefix(htlc, 0, INFOPREFIX, "agreement:\n%.*s\n", out_len, out_p);
		xfree(out_p);
	} else
#endif
		hx_printf_prefix(htlc, 0, INFOPREFIX, "agreement:\n%.*s\n", len, agreement);
}

static int
get_nposts (const char *news)
{
	char const *p;
	char *div = "\n_________________________________________________\n";
	int divlen = sizeof(div);
	int nposts = 0;

	for (p = news; ; p += divlen) {
		p = strstr(p, div);
		if (!p)
			break;
		nposts++;
	}

	return nposts;
}

static void
output_news_file (struct htlc_conn *htlc, const char *news, u_int16_t len)
{
	int nposts;

	if (task_inerror(htlc))
		return;
	nposts = get_nposts(news);
	hx_printf_prefix(htlc, 0, INFOPREFIX, "news: %d post%s:\n",
			nposts, nposts == 1 ? "" : "s");
#if defined(CONFIG_ICONV)
	char *out_p;
	size_t out_len;

	out_len = convbuf(g_encoding, g_server_encoding, news, len, &out_p);
	if (out_len) {
		hx_printf(htlc, 0, "%.*s\n", out_len, out_p);
		xfree(out_p);
	} else
#endif
		hx_printf(htlc, 0, "%s\n", news);
}

static void
output_news_post (struct htlc_conn *htlc, const char *news, u_int16_t len)
{
#if defined(CONFIG_ICONV)
	char *out_p;
	size_t out_len;

	out_len = convbuf(g_encoding, g_server_encoding, news, len, &out_p);
	if (out_len) {
		hx_printf_prefix(htlc, 0, INFOPREFIX, "news posted:\n%.*s\n", out_len, out_p);
		xfree(out_p);
	} else
#endif
		hx_printf_prefix(htlc, 0, INFOPREFIX, "news posted:\n%.*s\n", len, news);
}

static void
user_list (struct htlc_conn *htlc, struct hx_chat *chat)
{
}

static void
user_create (struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user,
	     const char *nam, u_int16_t icon, u_int16_t color)
{
	char *col;

	col = colorstr(color);
	if (chat->cid)
		hx_printf_prefix(htlc, 0, INFOPREFIX, "chat 0x%08x join: %s%s%s [%u:%u:%u]\n",
				 chat->cid, col, nam, DEFAULT, user->uid, icon, color);
	else
	{
		hx_printf_prefix(htlc, 0, INFOPREFIX, "join: %s%s%s [%u:%u:%u]\n",
				 col, nam, DEFAULT, user->uid, icon, color);
		hx_multikick(htlc, nam, user->uid);
	}
}

static void
user_delete (struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user)
{
	char *col = colorstr(user->color);

	if (chat->cid)
		hx_printf_prefix(htlc, 0, INFOPREFIX, "chat 0x%08x parts: %s%s%s [%u:%u:%u]\n",
		  chat->cid, col, user->name, DEFAULT, user->uid, user->icon, user->color);
	else
		hx_printf_prefix(htlc, 0, INFOPREFIX, "parts: %s%s%s [%u:%u:%u]\n",
		  col, user->name, DEFAULT, user->uid, user->icon, user->color);
}

static void
users_clear (struct htlc_conn *htlc, struct hx_chat *chat)
{
	htlc = NULL;
	chat = NULL;
}

static void
user_change (struct htlc_conn *htlc, struct hx_chat *chat, struct hx_user *user,
	     const char *nam, u_int16_t icon, u_int16_t color)
{
	char *col0, *col1;

	col0 = colorstr(user->color);
	if (user->color == color)
		col1 = col0;
	else
		col1 = colorstr(color);
	if (chat->cid)
		hx_printf_prefix(htlc, 0, INFOPREFIX,
		"chat 0x%08x %s%s%s [%u:%u:%u] is now known as %s%s%s [%u:%u:%u]\n",
		chat->cid, col0, user->name, DEFAULT, user->uid, user->icon, user->color,
		col1, nam, DEFAULT, user->uid, icon, color);
	else
		hx_printf_prefix(htlc, 0, INFOPREFIX,
		"%s%s%s [%u:%u:%u] is now known as %s%s%s [%u:%u:%u]\n",
		col0, user->name, DEFAULT, user->uid, user->icon, user->color,
		col1, nam, DEFAULT, user->uid, icon, color);
}

static void
output_user_info (struct htlc_conn *htlc, u_int32_t uid, const char *nam, const char *info, u_int16_t len)
{
	hx_printf_prefix(htlc, 0, INFOPREFIX, "info for %s (%u):\n%s\n", nam, uid, info);
}

static void
output_file_list (struct htlc_conn *htlc, struct cached_filelist *cfl)
{
	char buf[4096];
	u_int16_t i, bpos;
	u_int16_t fnlen;
	struct hl_filelist_hdr *fh;

	for (fh = cfl->fh; (u_int32_t)((char *)fh - (char *)cfl->fh) < cfl->fhlen;
	     fh = fh + ntohs(fh->len) + SIZEOF_HL_DATA_HDR) {
		fnlen = ntohs(fh->fnlen);
#if !defined(CONFIG_ICONV)
		for (i = 0, bpos = 0; i < fnlen && bpos < sizeof(buf); i++) {
			if (!isgraph(fh->fname[i])) {
				buf[bpos++] = '\\';
				switch (fh->fname[i]) {
					case ' ':
						buf[bpos++] = ' ';
						break;
					case '\r':
						buf[bpos++] = 'r';
						break;
					case '\n':
						buf[bpos++] = 'n';
						break;
					case '\t':
						buf[bpos++] = 't';
						break;
					default:
						sprintf(&(buf[bpos]), "x%2.2x", fh->fname[i]);
						bpos += 3;
				}
			} else
				buf[bpos++] = fh->fname[i];
		}
#else
		memcpy(buf, fh->fname, fnlen);
		bpos = fnlen;
#endif
		buf[bpos] = 0;
		hx_printf(htlc, 0, "%4.4s/%4.4s %10u %s\n",
			(char *)&fh->fcreator, (char *)&fh->ftype, ntohl(fh->fsize), buf);
	}
}

static void
file_info (struct htlc_conn *htlc, const char *icon, const char *type, const char *crea,
	   u_int32_t size, const char *name, const char *created, const char *modified,
	   const char *comment)
{
	hx_printf(htlc, 0, "\
    icon: %4.4s\n\
    type: %s\n\
 creator: %s\n\
    size: %u\n\
    name: %s\n\
 created: %s\n\
modified: %s\n\
 comment: %s\n", icon, type, crea, size, name, created, modified, comment);
}

static void
file_preview (struct htlc_conn *htlc, const char *filename, const char *type)
{
}

#if !USE_READLINE
static int CO = 80;
#endif

static void
tracker_server_create (struct htlc_conn *htlc, const char *addrstr, u_int16_t port, u_int16_t nusers,
		       const char *nam, const char *desc)
{
	unsigned int c, j, x;

	hx_printf(htlc, 0, "%16s:%-5u | %5u | %s\n",
		addrstr, port, nusers, nam);
	for (j = x = 0;;) {
		j += CO - 33;
		if (j < 512) {
			c = desc[j];
		} else
			c = 0;
		hx_printf(htlc, 0, "                                 %.*s\n", j, desc+x);
		if (!c)
			break;
		x = j;
	}
}

extern void output_news_dirlist_tty (struct htlc_conn *htlc, struct news_folder *news);
extern void output_news_catlist_tty (struct htlc_conn *htlc, struct news_group *group);
extern void output_news_thread_tty (struct htlc_conn *htlc, struct news_post *post);

#if USE_READLINE
static void
file_update (struct htxf_conn *htxf)
{
	htxf = NULL;
}

static void
task_update (struct htlc_conn *htlc, struct task *tsk)
{
	htlc = NULL;
	tsk = NULL;
}

static void
on_connect (struct htlc_conn *htlc)
{
	hx_get_user_list(htlc, 1);
}

static void
on_disconnect (struct htlc_conn *htlc)
{
	htlc = NULL;
}

static void
loop (void)
{
}

struct output_functions hx_tty_output = {
	init,
	loop,
	cleanup,
	term_status,
	term_clear,
	term_mode_underline,
	term_mode_clear,
	output_chat,
	chat_subject,
	chat_password,
	chat_invite,
	chat_delete,
	output_msg,
	output_agreement,
	output_news_file,
	output_news_post,
	output_news_dirlist_tty,
	output_news_catlist_tty,
	output_news_thread_tty,
	output_user_info,
	user_create,
	user_delete,
	user_change,
	user_list,
	users_clear,
	output_file_list,
	file_info,
	file_preview,
	file_update,
	tracker_server_create,
	task_update,
	on_connect,
	on_disconnect
};
#else
struct output_functions hx_tty_output = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	output_chat,
	chat_subject,
	chat_password,
	chat_invite,
	0,
	output_msg,
	output_agreement,
	output_news_file,
	output_news_post,
	output_news_dirlist_tty,
	output_news_catlist_tty,
	output_news_thread_tty,
	output_user_info,
	user_create,
	user_delete,
	user_change,
	user_list,
	users_clear,
	output_file_list,
	file_info,
	file_preview,
	0,
	tracker_server_create,
	0,
	0,
	0
};
#endif
