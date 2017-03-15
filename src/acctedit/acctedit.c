#include <sys/types.h>
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_NCURSES_H)
#include <ncurses.h>
#else
#include <curses.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "sys_deps.h"
#include "sys_net.h"
#include "hotline.h"

struct word {
	char *name;
	char *word;
	u_int8_t len;
};

struct dictionary {
	u_int32_t size;
	struct word *words;
};

void
edit_word (struct dictionary *dict, u_int32_t word)
{
	int c, wpos = strlen(dict->words[word].word), pos = wpos + 10, li;

	li = word;
	move(li, pos);
	refresh();
	for (;;) {
		switch ((c = getch())) {
			case KEY_BACKSPACE:
				if (wpos) {
					if (wpos-- == dict->words[word].len)
						dict->words[word].word[wpos] = 0;
					else
						strcpy(&dict->words[word].word[wpos],
							&dict->words[word].word[wpos + 1]);
					dict->words[word].len--;
					mvdelch(li, --pos);
				}
				break;
			case '\n':
			case '\r':
				return;
			default:
				if (c < 0x100) {
					dict->words[word].word[wpos++] = c;
					dict->words[word].len++;
					dict->words[word].word[wpos] = 0;
					mvaddch(li, pos++, c);
				}
		}
		refresh();
	}
}

void
clear_word (struct dictionary *dict, u_int32_t word)
{
	int li;
	char buf[1024];

	li = word;
	move(li, 0);
	clrtoeol();
	snprintf(buf, sizeof(buf), "%8s: %s", dict->words[word].name, dict->words[word].word);
	addstr(buf);
}

void
hilite_word (struct dictionary *dict, u_int32_t word)
{
	int len, li, y, x;
	char buf[1024];

	getyx(stdscr, y, x);
	li = word;
	move(li, 0);
	if ((len = snprintf(buf, sizeof(buf), "%8s: %s",
		dict->words[word].name, dict->words[word].word)) != -1) {
		while (len < x - 1)
			buf[len++] = ' ';
		buf[len] = 0;
	}
	standout();
	addstr(buf);
	standend();
}

void
draw_dict (struct dictionary *dict)
{
	register u_int32_t i;

	clear();
	for (i = 0; i < dict->size; i++)
		clear_word(dict, i);
	refresh();
}

struct access_name {
	int bitno;
	char *name;
} access_names[] = {
	{ 1, "upload files" },
	{ 2, "download files" },
	{ 0, "delete files" },
	{ 3, "rename files" },
	{ 4, "move files" },
	{ 5, "create folders" },
	{ 6, "delete folders" },
	{ 7, "rename folders" },
	{ 8, "move folders" },
	{ 9, "read chat" },
	{ 10, "send chat" },
	{ 11, "create private chats" },
	{ 14, "create users" },
	{ 15, "delete users" },
	{ 16, "read users" },
	{ 17, "modify users" },
	{ 20, "read news" },
	{ 21, "post news" },
	{ 22, "disconnect users" },
	{ 23, "not be disconnected" },
	{ 24, "get user info" },
	{ 25, "upload anywhere" },
	{ 26, "use any name" },
	{ 27, "not be shown agreement" },
	{ 28, "comment files" },
	{ 29, "comment folders" },
	{ 30, "view drop boxes" },
	{ 31, "make aliases" },
	{ 32, "broadcast" },
	{ 33, "delete articles" },
	{ 34, "create categories" },
	{ 35, "delete categories" },
	{ 36, "create news bundles" },
	{ 37, "delete news bundles" },
	{ 38, "upload folders" },
	{ 39, "download folders" },
	{ 40, "send messages" },
};

int
test_bit (void *bufp, int bitno)
{
	unsigned char *buf = (unsigned char *)bufp;
	unsigned char c, m;

	c = buf[bitno / 8];
	bitno = bitno % 8;
	bitno = 7 - bitno;
	if (!bitno)
		m = 1;
	else {
		m = 2;
		while (--bitno)
			m *= 2;
	}

	return c & m;
}

void
inverse_bit (void *bufp, int bitno)
{
	unsigned char *buf = (unsigned char *)bufp;
	unsigned char *p, c, m;

	p = &buf[bitno / 8];
	c = *p;
	bitno = bitno % 8;
	bitno = 7 - bitno;
	if (!bitno)
		m = 1;
	else {
		m = 2;
		while (--bitno)
			m *= 2;
	}
	if (c & m)
		*p = c & ~m;
	else
		*p = c | m;
}

void
set_bit (void *buf, int bitno)
{
	if (!test_bit(buf, bitno))
		inverse_bit(buf, bitno);
}

void
clr_bit (void *buf, int bitno)
{
	if (test_bit(buf, bitno))
		inverse_bit(buf, bitno);
}

void
clear_acc (void *acc, int n)
{
	move(3+n/2, !(n % 2) ? 2 : 33);
	addch(test_bit(acc, access_names[n].bitno) ? '*' : ' ');
}

void
hilite_acc (void *acc, int n)
{
	move(3+n/2, !(n % 2) ? 2 : 33);
	standout();
	addch(test_bit(acc, access_names[n].bitno) ? '*' : ' ');
	standend();
}

void
draw_access (void *acc)
{
	unsigned int i, len, li, x;
	char buf[128];

	getyx(stdscr, li, x);
	move(++li, 0);
	for (i = 0; i < sizeof(access_names) / sizeof(struct access_name); i += 2) {
		len = sprintf(buf, " [%c] Can %s", test_bit(acc, access_names[i].bitno) ? '*' : ' ', access_names[i].name);
		if (i + 1 < sizeof(access_names) / sizeof(struct access_name)) {
			for (; len < 32; len++)
				buf[len] = ' ';
			sprintf(buf+len, "[%c] Can %s", test_bit(acc, access_names[i+1].bitno) ? '*' : ' ', access_names[i+1].name);
		}
		addstr(buf);
		move(++li, 0);
	}
}

struct hl_access_bits acc;
char login[32], password[32], name[32];

struct word words[] = {
	{ "name", name, 0 },
	{ "login", login, 0 },
	{ "password", password, 0 }
};

struct dictionary dict = {
	3,
	words
};

int curword = 0;
int curacc = -1;

void
hl_code (void *__dst, const void *__src, size_t len)
{
	u_int8_t *dst = (u_int8_t *)__dst, *src = (u_int8_t *)__src;

	for (; len; len--)
		*dst++ = ~*src++;
}

int
account_read (const char *path, char *login, char *password, char *name, struct hl_access_bits *acc)
{
	struct hl_user_data user_data;
	int fd, r;
	u_int16_t len;

	fd = SYS_open(path, O_RDONLY, 0);
	if (fd < 0)
		return errno;
	if ((r = read(fd, &user_data, 734)) != 734) {
		close(fd);
		return errno;
	}
	close(fd);

	if (acc)
		*acc = user_data.access;
	if (name) {
		len = ntohs(user_data.nlen) > 31 ? 31 : ntohs(user_data.nlen);
		memcpy(name, user_data.name, len);
		name[len] = 0;
	}
	if (login) {
		len = ntohs(user_data.llen) > 31 ? 31 : ntohs(user_data.llen);
		memcpy(login, user_data.login, len);
		login[len] = 0;
	}
	if (password) {
		len = ntohs(user_data.plen) > 31 ? 31 : ntohs(user_data.plen);
		hl_code(password, user_data.password, len);
		password[len] = 0;
	}

	return 0;
}

int
account_write (const char *path, const char *login, const char *password, const char *name, const struct hl_access_bits *acc)
{
	int fd, r;
	struct hl_user_data user_data;
	u_int16_t nlen, llen, plen;

	fd = SYS_open(path, O_WRONLY|O_CREAT|O_TRUNC, 0600);
	if (fd < 0)
		return errno;

	if ((nlen = strlen(name)) > 134) nlen = 134;
	if ((llen = strlen(login)) > 34) llen = 34;
	if ((plen = strlen(password)) > 32) plen = 32;

	memset(&user_data, 0, sizeof(user_data));
	user_data.magic = htonl(0x00010000);
	memcpy(&user_data.access, acc, sizeof(user_data.access));
	user_data.nlen = htons(nlen);
	memcpy(user_data.name, name, nlen);
	user_data.llen = htons(llen);
	memcpy(user_data.login, login, llen);
	user_data.plen = htons(plen);
	hl_code(user_data.password, password, plen);

	if ((r = write(fd, &user_data, 734)) != 734) {
		close(fd);
		return errno;
	}
	SYS_fsync(fd);

	close(fd);

	return 0;
}

void
edit_acct (const char *filename)
{
	if (account_read(filename, login, password, name, &acc)) {
		perror(login);
		exit(1);
	}

	words[0].len = strlen(name);
	words[1].len = strlen(login);
	words[2].len = strlen(password);
	draw_dict(&dict);

	draw_access(&acc);

	for (;;) {
		int x, y;

		switch (getch()) {
			case KEY_DOWN:
				if (curword == -1) {
					if ((unsigned)curacc + 2 < sizeof(access_names) / sizeof(struct access_name)) {
						clear_acc(&acc, curacc);
						curacc += 2;
						hilite_acc(&acc, curacc);
					}
					break;
				}
				if ((unsigned)curword < dict.size - 1) {
					clear_word(&dict, curword);
					hilite_word(&dict, ++curword);
				} else {
					clear_word(&dict, curword);
					curword = -1;
					curacc = 0;
					hilite_acc(&acc, 0);
				}
				break;
			case KEY_UP:
				if (curword == -1 && curacc > 1) {
					clear_acc(&acc, curacc);
					curacc -= 2;
					hilite_acc(&acc, curacc);
				} else if (curword) {
					if (curword == -1) {
						clear_acc(&acc, curacc);
						curacc = -1;
						curword = 2;
					} else
						clear_word(&dict, curword--);
					hilite_word(&dict, curword);
				}
				break;
			case KEY_LEFT:
				if (curacc != -1 && curacc) {
					clear_acc(&acc, curacc);
					curacc--;
					hilite_acc(&acc, curacc);
				}
				break;
			case KEY_RIGHT:
				if (curacc == -1)
					break;
				if ((unsigned)curacc + 1 < sizeof(access_names) / sizeof(struct access_name)) {
					clear_acc(&acc, curacc);
					curacc++;
					hilite_acc(&acc, curacc);
				}
				break;
			case '\n':
			case '\r':
				if (curword != -1) {
					clear_word(&dict, curword);
					edit_word(&dict, curword);
					hilite_word(&dict, curword);
				} else if (curacc != -1) {
					inverse_bit(&acc, access_names[curacc].bitno);
					hilite_acc(&acc, curacc);
				}
				break;
			case 's':
				getmaxyx(stdscr, y, x);
				move(y-1, 0);
				addstr("saving ");
				addstr(filename);
				addstr(" ... ");
				if (account_write(filename, login, password, name, &acc)) {
					addstr("error: ");
					addstr((char *)strerror(errno));
				} else
					addstr("done.");
				break;
			case 'q':
				return;
				break;
				
		}
		refresh();
	}
}

int *
get_bits (const char *arg)
{
	char const *p, *lastp;
	char buf[32];
	unsigned int nbits, len;
	int *arr;

	nbits = 0;
	arr = 0;
	lastp = arg;
	for (p = arg; ; p++) {
		if (*p == ',' || *p == 0) {
			arr = realloc(arr, (nbits+2)*sizeof(int));
			if (!arr)
				return 0;
			len = p - lastp;
			if (len > sizeof(buf)-1)
				len = sizeof(buf)-1;
			memcpy(buf, lastp, len);
			buf[len] = 0;
			lastp = p+1;
			arr[nbits] = strtoul(buf, 0, 0);
			nbits++;
			if (*p == 0)
				break;
		}
	}
	arr[nbits] = 0;

	return arr;
}

int
main (int argc, char **argv)
{
	char *filename;
	int i, j;
	int *cbits = 0, *sbits = 0, *tbits = 0;
	struct opt_r opt;

	opt.err_printf = 0;
	opt.ind = 0;
	while ((i = getopt_r(argc, argv, "c:s:t:", &opt)) != EOF) {
		switch (i) {
			case 'c':
				cbits = get_bits(opt.arg);
				break;
			case 's':
				sbits = get_bits(opt.arg);
				break;
			case 't':
				tbits = get_bits(opt.arg);
				break;
		}
	}

	if (!argv[opt.ind]) {
		fprintf(stderr, "usage: %s [-[cst] bit1,bit2,...] <UserData>\n  c: clear, s: set, t: test\n", argv[0]);
		exit(1);
	}

	if (cbits || sbits || tbits) {
		int retcode = 0;
		for (i = opt.ind; i < argc; i++) {
			filename = argv[i];
			if (account_read(filename, login, password, name, &acc)) {
				fprintf(stderr, "error reading %s: %s\n", filename, strerror(errno));
				continue;
			}
			if (cbits) {
				for (j = 0; cbits[j]; j++) {
					clr_bit(&acc, cbits[j]);
				}
			}
			if (sbits) {
				for (j = 0; sbits[j]; j++) {
					set_bit(&acc, sbits[j]);
				}
			}
			if (tbits) {
				for (j = 0; tbits[j]; j++) {
					if (!test_bit(&acc, tbits[j]))
						retcode = 1;
				}
			}
			if (cbits || sbits) {
				if (account_write(filename, login, password, name, &acc)) {
					fprintf(stderr, "error writing %s: %s\n", filename, strerror(errno));
				}
			}
		}
		if (cbits)
			free(cbits);
		if (sbits)
			free(sbits);
		if (sbits)
			free(tbits);
		exit(retcode);
	}

	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	for (i = opt.ind; i < argc; i++) {
		filename = argv[i];
		edit_acct(filename);
	}

	reset_shell_mode();

	return 0;
}
