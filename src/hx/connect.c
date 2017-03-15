#include "hx.h"
#include "hxd.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include "sys_net.h"
#if !defined(__WIN32__)
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include "xmalloc.h"

#ifdef SOCKS
#include "socks.h"
#endif

#if defined(CONFIG_SOUND)
#include "sound.h"
#endif

extern char hx_hostname[128];
extern u_int16_t g_clientversion;

#if defined(CONFIG_HOPE)
#if defined(CONFIG_CIPHER)
static char *valid_ciphers[] = {"RC4", "BLOWFISH", "IDEA", 0};

static int
valid_cipher (const char *cipheralg)
{
	unsigned int i;

	for (i = 0; valid_ciphers[i]; i++) {
		if (!strcmp(valid_ciphers[i], cipheralg))
			return 1;
	}

	return 0;
}
#endif

#if defined(CONFIG_COMPRESS)
static char *valid_compressors[] = {"GZIP", 0};

static int
valid_compress (const char *compressalg)
{
	unsigned int i;

	for (i = 0; valid_compressors[i]; i++) {
		if (!strcmp(valid_compressors[i], compressalg))
			return 1;
	}

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
#endif /* CONFIG_HOPE */

void
hx_htlc_close (struct htlc_conn *htlc)
{
	int fd = htlc->fd;
	struct hx_chat *chat, *cnext;
	struct hx_user *user, *unext;
	char abuf[HOSTLEN+1];

	inaddr2str(abuf, &htlc->sockaddr);
	hx_printf_prefix(htlc, 0, INFOPREFIX, "%s:%u: connection closed\n", abuf, ntohs(htlc->sockaddr.SIN_PORT));
	socket_close(fd);
	hxd_fd_clr(fd, FDR|FDW);
	hxd_fd_del(fd);
	if (htlc->wfd) {
		close(htlc->wfd);
		hxd_fd_clr(htlc->wfd, FDR|FDW);
		hxd_fd_del(htlc->wfd);
	}
	hx_output.on_disconnect(htlc);
#ifdef CONFIG_CIPHER
	memset(htlc->cipher_encode_key, 0, sizeof(htlc->cipher_encode_key));
	memset(htlc->cipher_decode_key, 0, sizeof(htlc->cipher_decode_key));
	memset(&htlc->cipher_encode_state, 0, sizeof(htlc->cipher_encode_state));
	memset(&htlc->cipher_decode_state, 0, sizeof(htlc->cipher_decode_state));
	htlc->cipher_encode_type = 0;
	htlc->cipher_decode_type = 0;
	htlc->cipher_encode_keylen = 0;
	htlc->cipher_decode_keylen = 0;
#endif
#ifdef CONFIG_COMPRESS
	if (htlc->compress_encode_type != COMPRESS_NONE) {
		hx_printf_prefix(htlc, 0, INFOPREFIX, "GZIP deflate: in: %u  out: %u\n",
				 htlc->gzip_deflate_total_in, htlc->gzip_deflate_total_out);
		compress_encode_end(htlc);
	}
	if (htlc->compress_decode_type != COMPRESS_NONE) {
		hx_printf_prefix(htlc, 0, INFOPREFIX, "GZIP inflate: in: %u  out: %u\n",
				 htlc->gzip_inflate_total_in, htlc->gzip_inflate_total_out);
		compress_decode_end(htlc);
	}
	memset(&htlc->compress_encode_state, 0, sizeof(htlc->compress_encode_state));
	memset(&htlc->compress_decode_state, 0, sizeof(htlc->compress_decode_state));
	htlc->compress_encode_type = 0;
	htlc->compress_decode_type = 0;
	htlc->gzip_deflate_total_in = 0;
	htlc->gzip_deflate_total_out = 0;
	htlc->gzip_inflate_total_in = 0;
	htlc->gzip_inflate_total_out = 0;
#endif
#ifdef CONFIG_HOPE
	memset(htlc->sessionkey, 0, sizeof(htlc->sessionkey));
	htlc->sklen = 0;
#endif
	if (htlc->read_in.buf) {
		xfree(htlc->read_in.buf);
		htlc->read_in.buf = 0;
		htlc->read_in.pos = 0;
		htlc->read_in.len = 0;
	}
	if (htlc->in.buf) {
		xfree(htlc->in.buf);
		htlc->in.buf = 0;
		htlc->in.pos = 0;
		htlc->in.len = 0;
	}
	if (htlc->out.buf) {
		xfree(htlc->out.buf);
		htlc->out.buf = 0;
		htlc->out.pos = 0;
		htlc->out.len = 0;
	}
	memset(&hxd_files[fd], 0, sizeof(struct hxd_file));
	htlc->fd = 0;
	htlc->uid = 0;
	htlc->color = 0;
	memset(htlc->login, 0, sizeof(htlc->login));

	for (chat = htlc->chat_list; chat; chat = cnext) {
		cnext = chat->prev;
		hx_output.users_clear(htlc, chat);
		for (user = chat->user_list->next; user; user = unext) {
			unext = user->next;
			hx_user_delete(&chat->user_tail, user);
		}
		hx_chat_delete(htlc, chat);
	}
	task_delete_all();
}

static void
rcv_task_login (struct htlc_conn *htlc, void *secure)
{
	char abuf[HOSTLEN+1];
	u_int32_t uid;
	u_int16_t icon16;
#ifdef CONFIG_HOPE
	u_int16_t hc;
	u_int8_t *p, *mal = 0;
	u_int16_t mal_len = 0;
	u_int16_t sklen = 0, macalglen = 0, secure_login = 0, secure_password = 0;
	u_int8_t password_mac[20];
	u_int8_t login[32];
	u_int16_t llen, pmaclen;
#ifdef CONFIG_CIPHER
	u_int8_t *s_cipher_al = 0, *c_cipher_al = 0;
	u_int16_t s_cipher_al_len = 0, c_cipher_al_len = 0;
	u_int8_t s_cipheralg[32], c_cipheralg[32];
	u_int16_t s_cipheralglen = 0, c_cipheralglen = 0;
	u_int8_t cipheralglist[64];
	u_int16_t cipheralglistlen;
#endif
#ifdef CONFIG_COMPRESS
	u_int8_t *s_compress_al = 0, *c_compress_al = 0;
	u_int16_t s_compress_al_len = 0, c_compress_al_len = 0;
	u_int8_t s_compressalg[32], c_compressalg[32];
	u_int16_t s_compressalglen = 0, c_compressalglen = 0;
	u_int8_t compressalglist[64];
	u_int16_t compressalglistlen;
#endif

	if (secure) {
		dh_start(htlc)
			switch (dh_type) {
				case HTLS_DATA_LOGIN:
					if (dh_len && dh_len == strlen(htlc->macalg) && !memcmp(htlc->macalg, dh_data, dh_len))
						secure_login = 1;
					break;
				case HTLS_DATA_PASSWORD:
					if (dh_len && dh_len == strlen(htlc->macalg) && !memcmp(htlc->macalg, dh_data, dh_len))
						secure_password = 1;
					break;
				case HTLS_DATA_MAC_ALG:
					mal_len = dh_len;
					mal = dh_data;
					break;
#ifdef CONFIG_CIPHER
				case HTLS_DATA_CIPHER_ALG:
					s_cipher_al_len = dh_len;
					s_cipher_al = dh_data;
					break;
				case HTLC_DATA_CIPHER_ALG:
					c_cipher_al_len = dh_len;
					c_cipher_al = dh_data;
					break;
				case HTLS_DATA_CIPHER_MODE:
					break;
				case HTLC_DATA_CIPHER_MODE:
					break;
				case HTLS_DATA_CIPHER_IVEC:
					break;
				case HTLC_DATA_CIPHER_IVEC:
					break;
#endif
#if defined(CONFIG_COMPRESS)
				case HTLS_DATA_COMPRESS_ALG:
					s_compress_al_len = dh_len;
					s_compress_al = dh_data;
					break;
				case HTLC_DATA_COMPRESS_ALG:
					c_compress_al_len = dh_len;
					c_compress_al = dh_data;
					break;
#endif
				case HTLS_DATA_CHECKSUM_ALG:
					break;
				case HTLC_DATA_CHECKSUM_ALG:
					break;
				case HTLS_DATA_SESSIONKEY:
					sklen = dh_len > sizeof(htlc->sessionkey) ? sizeof(htlc->sessionkey) : dh_len;
					memcpy(htlc->sessionkey, dh_data, sklen);
					htlc->sklen = sklen;
					break;
			}
		dh_end()

		if (!mal_len) {
no_mal:			hx_printf_prefix(htlc, 0, INFOPREFIX, "No macalg from server\n");
			hx_htlc_close(htlc);
			memset(htlc->password, 0, sizeof(htlc->password));
			return;
		}

		p = list_n(mal, mal_len, 0);
		if (!p || !*p)
			goto no_mal;
		macalglen = *p >= sizeof(htlc->macalg) ? sizeof(htlc->macalg)-1 : *p;
		memcpy(htlc->macalg, p+1, macalglen);
		htlc->macalg[macalglen] = 0;

		if (sklen < 32) {
			hx_printf_prefix(htlc, 0, INFOPREFIX,
					 "sessionkey length (%u) not big enough\n", sklen);
			hx_htlc_close(htlc);
			memset(htlc->password, 0, sizeof(htlc->password));
			return;
		}
#ifdef CONFIG_IPV6
		if (memcmp(htlc->sessionkey, &htlc->sockaddr.SIN_ADDR.S_ADDR, 16)
		    || *((u_int16_t *)(htlc->sessionkey + 16)) != htlc->sockaddr.SIN_PORT) {
#else
		if (*((u_int32_t *)(htlc->sessionkey)) != htlc->sockaddr.SIN_ADDR.S_ADDR
		    || *((u_int16_t *)(htlc->sessionkey + 4)) != htlc->sockaddr.SIN_PORT) {
#endif
			char fakeabuf[HOSTLEN+1], realabuf[HOSTLEN+1];
			struct IN_ADDR fakeinaddr;

#ifdef CONFIG_IPV6
			memcpy(&fakeinaddr.S_ADDR, htlc->sessionkey, 16);
			inet_ntop(AFINET, (char *)&fakeinaddr, fakeabuf, sizeof(fakeabuf));
			inet_ntop(AFINET, (char *)&htlc->sockaddr.SIN_ADDR, realabuf, sizeof(realabuf));
#else
			fakeinaddr.S_ADDR = *((u_int32_t *)(htlc->sessionkey));
			inet_ntoa_r(fakeinaddr, fakeabuf, sizeof(fakeabuf));
			inet_ntoa_r(htlc->sockaddr.SIN_ADDR, realabuf, sizeof(realabuf));
#endif
			hx_printf_prefix(htlc, 0, INFOPREFIX, "Server gave wrong address: %s:%u != %s:%u\n",
					 fakeabuf, ntohs(*((u_int16_t *)(htlc->sessionkey + 4))),
					 realabuf, ntohs(htlc->sockaddr.SIN_PORT));
			/* XXX HACK XXX */
			if (htlc->secure == 2) {
				hx_printf_prefix(htlc, 0, INFOPREFIX, "Possible man-in-the-middle attack! Connecting anyway.\n");
			} else {
				hx_printf_prefix(htlc, 0, INFOPREFIX, "Possible man-in-the-middle attack! Closing connection. Use -f to force connect.\n");
				hx_htlc_close(htlc);
				memset(htlc->password, 0, sizeof(htlc->password));
				return;
			}
		}
		if (task_inerror(htlc)) {
			hx_htlc_close(htlc);
			memset(htlc->password, 0, sizeof(htlc->password));
			return;
		}
		task_new(htlc, rcv_task_login, 0, 0, "login");
		icon16 = htons(htlc->icon);
		if (secure_login) {
			llen = hmac_xxx(login, htlc->login, strlen(htlc->login),
					htlc->sessionkey, sklen, htlc->macalg);
			if (!llen) {
				hx_printf_prefix(htlc, 0, INFOPREFIX,
						 "bad HMAC algorithm %s\n", htlc->macalg);
				hx_htlc_close(htlc);
				memset(htlc->password, 0, sizeof(htlc->password));
				return;
			}
		} else {
			llen = strlen(htlc->login);
			hl_encode(login, htlc->login, llen);
			login[llen] = 0;
		}
		pmaclen = hmac_xxx(password_mac, htlc->password, strlen(htlc->password),
				   htlc->sessionkey, sklen, htlc->macalg);
		if (!pmaclen) {	
			hx_printf_prefix(htlc, 0, INFOPREFIX,
					 "bad HMAC algorithm %s\n", htlc->macalg);
			hx_htlc_close(htlc);
			return;
		}
		hc = 4;
#ifdef CONFIG_COMPRESS
		if (!htlc->compressalg[0] || !strcmp(htlc->compressalg, "NONE")) {
			hx_printf_prefix(htlc, 0, INFOPREFIX,
					 "WARNING: this connection is not compressed\n");
			compressalglistlen = 0;
			goto no_compress;
		}
		if (!c_compress_al_len || !s_compress_al_len) {
no_compress_al:		hx_printf_prefix(htlc, 0, INFOPREFIX,
				 "No compress algorithm from server\n");
			hx_htlc_close(htlc);
			return;
		}
		p = list_n(s_compress_al, s_compress_al_len, 0);
		if (!p || !*p)
			goto no_compress_al;
		s_compressalglen = *p >= sizeof(s_compressalg) ? sizeof(s_compressalg)-1 : *p;
		memcpy(s_compressalg, p+1, s_compressalglen);
		s_compressalg[s_compressalglen] = 0;
		p = list_n(c_compress_al, c_compress_al_len, 0);
		if (!p || !*p)
			goto no_compress_al;
		c_compressalglen = *p >= sizeof(c_compressalg) ? sizeof(c_compressalg)-1 : *p;
		memcpy(c_compressalg, p+1, c_compressalglen);
		c_compressalg[c_compressalglen] = 0;
		if (!valid_compress(c_compressalg)) {
			hx_printf_prefix(htlc, 0, INFOPREFIX,
					 "Bad client compress algorithm %s\n", c_compressalg);
			goto ret_badcompress_a;
		} else if (!valid_compress(s_compressalg)) {
			hx_printf_prefix(htlc, 0, INFOPREFIX,
					 "Bad server compress algorithm %s\n", s_compressalg);
ret_badcompress_a:
			compressalglistlen = 0;
			hx_htlc_close(htlc);
			return;
		} else {
			S16HTON(1, compressalglist);
			compressalglistlen = 2;
			compressalglist[compressalglistlen] = s_compressalglen;
			compressalglistlen++;
			memcpy(compressalglist+compressalglistlen, s_compressalg, s_compressalglen);
			compressalglistlen += s_compressalglen;
		}
no_compress:
		hc++;
#endif
#ifdef CONFIG_CIPHER
		if (!htlc->cipheralg[0] || !strcmp(htlc->cipheralg, "NONE")) {
			hx_printf_prefix(htlc, 0, INFOPREFIX,
					 "WARNING: this connection is not encrypted\n");
			cipheralglistlen = 0;
			goto no_cipher;
		}
		if (!c_cipher_al_len || !s_cipher_al_len) {
no_cal:			hx_printf_prefix(htlc, 0, INFOPREFIX,
					 "No cipher algorithm from server\n");
			hx_htlc_close(htlc);
			return;
		}
		p = list_n(s_cipher_al, s_cipher_al_len, 0);
		if (!p || !*p)
			goto no_cal;
		s_cipheralglen = *p >= sizeof(s_cipheralg) ? sizeof(s_cipheralg)-1 : *p;
		memcpy(s_cipheralg, p+1, s_cipheralglen);
		s_cipheralg[s_cipheralglen] = 0;
		p = list_n(c_cipher_al, c_cipher_al_len, 0);
		if (!p || !*p)
			goto no_cal;
		c_cipheralglen = *p >= sizeof(c_cipheralg) ? sizeof(c_cipheralg)-1 : *p;
		memcpy(c_cipheralg, p+1, c_cipheralglen);
		c_cipheralg[c_cipheralglen] = 0;
		if (!valid_cipher(c_cipheralg)) {
			hx_printf_prefix(htlc, 0, INFOPREFIX,
					 "Bad client cipher algorithm %s\n", c_cipheralg);
			goto ret_badca;
		} else if (!valid_cipher(s_cipheralg)) {
			hx_printf_prefix(htlc, 0, INFOPREFIX,
					 "Bad server cipher algorithm %s\n", s_cipheralg);
ret_badca:
			cipheralglistlen = 0;
			hx_htlc_close(htlc);
			return;
		} else {
			S16HTON(1, cipheralglist);
			cipheralglistlen = 2;
			cipheralglist[cipheralglistlen] = s_cipheralglen;
			cipheralglistlen++;
			memcpy(cipheralglist+cipheralglistlen, s_cipheralg, s_cipheralglen);
			cipheralglistlen += s_cipheralglen;
		}

		/* server key first */
		pmaclen = hmac_xxx(htlc->cipher_decode_key, htlc->password, strlen(htlc->password),
				   password_mac, pmaclen, htlc->macalg);
		htlc->cipher_decode_keylen = pmaclen;
		pmaclen = hmac_xxx(htlc->cipher_encode_key, htlc->password, strlen(htlc->password),
				   htlc->cipher_decode_key, pmaclen, htlc->macalg);
		htlc->cipher_encode_keylen = pmaclen;
no_cipher:
		hc++;
#endif
		memset(htlc->password, 0, sizeof(htlc->password));
		hlwrite(htlc, HTLC_HDR_LOGIN, 0, hc,
			HTLC_DATA_LOGIN, llen, login,
			HTLC_DATA_PASSWORD, pmaclen, password_mac,
#ifdef CONFIG_CIPHER
			HTLS_DATA_CIPHER_ALG, cipheralglistlen, cipheralglist,
#endif
#ifdef CONFIG_COMPRESS
			HTLS_DATA_COMPRESS_ALG, compressalglistlen, compressalglist,
#endif
			HTLC_DATA_NAME, strlen(htlc->name), htlc->name,
			HTLC_DATA_ICON, 2, &icon16);
#ifdef CONFIG_COMPRESS
		if (compressalglistlen) {
			hx_printf_prefix(htlc, 0, INFOPREFIX, "compress: server %s client %s\n",
					 c_compressalg, s_compressalg);
			if (c_compress_al_len) {
				htlc->compress_encode_type = COMPRESS_GZIP;
				compress_encode_init(htlc);
			}
			if (s_compress_al_len) {
				htlc->compress_decode_type = COMPRESS_GZIP;
				compress_decode_init(htlc);
			}
		}
#endif
#ifdef CONFIG_CIPHER
		if (cipheralglistlen) {
			hx_printf_prefix(htlc, 0, INFOPREFIX, "cipher: server %s client %s\n",
					 c_cipheralg, s_cipheralg);
			if (!strcmp(s_cipheralg, "RC4"))
				htlc->cipher_decode_type = CIPHER_RC4;
			else if (!strcmp(s_cipheralg, "BLOWFISH"))
				htlc->cipher_decode_type = CIPHER_BLOWFISH;
			else if (!strcmp(s_cipheralg, "IDEA"))
				htlc->cipher_decode_type = CIPHER_IDEA;
			if (!strcmp(c_cipheralg, "RC4"))
				htlc->cipher_encode_type = CIPHER_RC4;
			else if (!strcmp(c_cipheralg, "BLOWFISH"))
				htlc->cipher_encode_type = CIPHER_BLOWFISH;
			else if (!strcmp(c_cipheralg, "IDEA"))
				htlc->cipher_encode_type = CIPHER_IDEA;
			cipher_encode_init(htlc);
			cipher_decode_init(htlc);
		}
#endif
	} else {
#endif /* CONFIG_HOPE */
		inaddr2str(abuf, &htlc->sockaddr);
		hx_printf_prefix(htlc, 0, INFOPREFIX, "%s:%u: login %s\n",
			  abuf, ntohs(htlc->sockaddr.SIN_PORT), task_inerror(htlc) ? "failed?" : "successful");

		if (!task_inerror(htlc)) {
			hx_chat_new(htlc, 0);
			play_sound(snd_login);
			dh_start(htlc)
				switch (dh_type) {
					case HTLS_DATA_UID:
						dh_getint(uid);
						htlc->uid = uid;
						break;
					case HTLS_DATA_SERVERVERSION:
						dh_getint(htlc->serverversion);
						break;
					case HTLS_DATA_BANNERID:
						break;
					case HTLS_DATA_SERVERNAME:
						hx_printf_prefix(htlc, 0, INFOPREFIX, "servername: %.*s\n", dh_len, dh_data);
						break;
				}
			dh_end()
			if (htlc->clientversion >= 150 && htlc->serverversion < 150) {
				icon16 = htons(htlc->icon);
				hlwrite(htlc, HTLC_HDR_USER_CHANGE, 0, 2,
					HTLC_DATA_NAME, strlen(htlc->name), htlc->name,
					HTLC_DATA_ICON, 2, &icon16);
			}
		hx_output.on_connect(htlc);
		}
#ifdef CONFIG_HOPE
	}
#endif
}

extern void htlc_read (int s);
extern void htlc_write_connect (int s);
extern void htlc_write (int s);
extern void hx_rcv_magic (struct htlc_conn *htlc);

#if !defined(__WIN32__)
int
fd_popen (int *pfds, const char *cmd)
{
	char **envp;
	char *argv[32];
	char command[4096];
	char *p, *thisarg;
	int argc;
	unsigned int len;
	int rpfds[2], wpfds[2];

	len = strlen(cmd);
	if (len > sizeof(command)-1)
		len = sizeof(command)-1;
	memcpy(command, cmd, len);
	command[len] = 0;
	for (argc = 0, thisarg = p = command; argc < 30; p++) {
		if (isspace(*p) || *p == 0) {
			int last = *p == 0 ? 1 : 0;
			*p = 0;
			argv[argc] = thisarg;
			argc++;
			if (last)
				break;
			p++;
			while (isspace(*p))
				p++;
			thisarg = p;
		}
	}
	argv[argc] = 0;

	if (pipe(rpfds) || pipe(wpfds)) {
		return -1;
	}
	nr_open_files += 4;
	if (nr_open_files >= hxd_open_max) {
		close(rpfds[0]);
		close(rpfds[1]);
		close(wpfds[0]);
		close(wpfds[1]);
		nr_open_files -= 4;
		return -2;
	}
	switch (fork()) {
		case -1:
			close(rpfds[0]);
			close(rpfds[1]);
			close(wpfds[0]);
			close(wpfds[1]);
			nr_open_files -= 4;
			return -3;
		case 0:
			close(rpfds[0]);
			close(wpfds[1]);
			if (wpfds[0] != 0) {
				if (dup2(wpfds[0], 0) == -1) {
					_exit(1);
				}
			}
			if (rpfds[1] != 1) {
				if (dup2(rpfds[1], 1) == -1) {
				/* || dup2(rpfds[1], 2) == -1) { */
					_exit(1);
				}
			}
#if 0
			{
				int i;

				fprintf(stderr, "executing");
				for (i = 0; i < argc; i++)
					fprintf(stderr, " %s", argv[i]);
			}
#endif
			envp = hxd_environ;
			/* execve(argv[0], argv, envp); */
			execvp(argv[0], argv);
			/* fprintf(stderr, " ERROR %s", strerror(errno)); */
			_exit(1);
		default:
			break;
	}
	close(rpfds[1]);
	close(wpfds[0]);
	pfds[0] = rpfds[0];
	pfds[1] = wpfds[1];

	return 0;
}
#endif /* ndef __WIN32__ */

void
hx_connect (struct htlc_conn *htlc, const char *serverstr, u_int16_t port,
	    const char *name, u_int16_t icon, const char *login, const char *pass,
	    int secure)
{
	int s;
	struct SOCKADDR_IN saddr;
	char abuf[HOSTLEN+1];

#if !defined(__WIN32__)
	if (serverstr[0] == '|' && serverstr[1]) {
		int pfds[2];
		int r;

		if (htlc->fd)
			hx_htlc_close(htlc);
		r = fd_popen(pfds, serverstr+1);
		if (r < 0) {
			hx_printf_prefix(htlc, 0, INFOPREFIX, "fd_popen(%s): %s\n",
					serverstr+1, strerror(errno));
			return;
		}
		hx_printf_prefix(htlc, 0, INFOPREFIX, "connecting through pipe %s\n", serverstr+1);
		htlc->fd = pfds[0];
		htlc->wfd = pfds[1];
		s = htlc->fd;
		fd_blocking(s, 0);
		fd_closeonexec(s, 1);
		hxd_files[s].ready_write = 0;
		hxd_files[s].ready_read = htlc_read;
		hxd_files[s].conn.htlc = htlc;
		hxd_fd_add(s);
		hxd_fd_set(s, FDR);
		if (high_fd < s)
			high_fd = s;
		s = htlc->wfd;
		fd_blocking(s, 0);
		fd_closeonexec(s, 1);
		hxd_files[s].ready_read = 0;
		hxd_files[s].ready_write = htlc_write;
		hxd_files[s].conn.htlc = htlc;
		hxd_fd_add(s);
		hxd_fd_set(s, FDW);
		if (high_fd < s)
			high_fd = s;
		goto is_pipe;
	}
#endif
#if defined(CONFIG_UNIXSOCKETS)
	else if (serverstr[0] == '!' && serverstr[1]) {
		struct sockaddr_un usaddr;

		s = socket(AF_UNIX, SOCK_STREAM, 0);
		if (s < 0) {
			hx_printf_prefix(htlc, 0, INFOPREFIX, "socket: %s\n", strerror(errno));
			return;
		}
		usaddr.sun_family = AF_UNIX;
		snprintf(usaddr.sun_path, sizeof(usaddr.sun_path), "%s", serverstr+1);

		if (htlc->fd)
			hx_htlc_close(htlc);
		if (connect(s, (struct sockaddr *)&usaddr, sizeof(usaddr))) {
			switch (errno) {
				case EINPROGRESS:
					break;
				default:
					hx_printf_prefix(htlc, 0, INFOPREFIX, "connect: %s\n", strerror(errno));
					socket_close(s);
					return;
			}
		}
		htlc->usockaddr = usaddr;
		socket_blocking(s, 0);
		fd_closeonexec(s, 1);
		goto is_unix;
	}
#endif

	s = socket(AFINET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0) {
		hx_printf_prefix(htlc, 0, INFOPREFIX, "socket: %s\n", strerror(errno));
		return;
	}
	if (s >= hxd_open_max) {
		hx_printf_prefix(htlc, 0, INFOPREFIX, "%s:%d: %d >= hxd_open_max (%d)", __FILE__, __LINE__, s, hxd_open_max);
		close(s);
		return;
	}
	socket_blocking(s, 0);
	fd_closeonexec(s, 1);

	if (hx_hostname[0]) {
#ifdef CONFIG_IPV6
		if (!inet_pton(AFINET, hx_hostname, &saddr.SIN_ADDR)) {
#else
		if (!inet_aton(hx_hostname, &saddr.SIN_ADDR)) {
#endif
			struct hostent *he;

			if ((he = gethostbyname(hx_hostname))) {
				size_t len = (unsigned int)he->h_length > sizeof(struct IN_ADDR)
					     ? sizeof(struct IN_ADDR) : (unsigned int)he->h_length;
				memcpy(&saddr.SIN_ADDR, he->h_addr, len);
			} else {
#ifndef HAVE_HSTRERROR
				hx_printf_prefix(htlc, 0, INFOPREFIX, "DNS lookup for %s failed\n", hx_hostname);
#else
				hx_printf_prefix(htlc, 0, INFOPREFIX, "DNS lookup for %s failed: %s\n", hx_hostname, hstrerror(h_errno));
#endif
				socket_close(s);
				return;
			}
		}
		saddr.SIN_PORT = 0;
		saddr.SIN_FAMILY = AFINET;
		if (bind(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
			inaddr2str(abuf, &saddr);
			hx_printf_prefix(htlc, 0, INFOPREFIX, "bind %s (%s): %s\n", hx_hostname, abuf, strerror(errno));
			socket_close(s);
			return;
		}
	}

#ifdef CONFIG_IPV6
	if (!inet_pton(AFINET, serverstr, &saddr.SIN_ADDR)) {
#else
	if (!inet_aton(serverstr, &saddr.SIN_ADDR)) {
#endif
		struct hostent *he;

		if ((he = gethostbyname(serverstr))) {
			size_t len = (unsigned int)he->h_length > sizeof(struct IN_ADDR)
				     ? sizeof(struct IN_ADDR) : (unsigned int)he->h_length;
			memcpy(&saddr.SIN_ADDR, he->h_addr, len);
		} else {
#ifndef HAVE_HSTRERROR
			hx_printf_prefix(htlc, 0, INFOPREFIX, "DNS lookup for %s failed\n", serverstr);
#else
			hx_printf_prefix(htlc, 0, INFOPREFIX, "DNS lookup for %s failed: %s\n", serverstr, hstrerror(h_errno));
#endif
			socket_close(s);
			return;
		}
	}
	saddr.SIN_PORT = htons(port);
	saddr.SIN_FAMILY = AFINET;

	if (htlc->fd)
		hx_htlc_close(htlc);
	if (connect(s, (struct sockaddr *)&saddr, sizeof(saddr))) {
#if !defined(__WIN32__)
		switch (errno) {
			case EINPROGRESS:
				break;
			default:
				hx_printf_prefix(htlc, 0, INFOPREFIX, "connect: %s\n", strerror(errno));
				socket_close(s);
				return;
		}
#else
		int wsaerr;
		wsaerr = WSAGetLastError();
		if (wsaerr != WSAEWOULDBLOCK) {
			hx_printf_prefix(htlc, 0, INFOPREFIX, "connect: WSA error %d\n", wsaerr);
			socket_close(s);
			return;
		}
#endif
	}
	htlc->sockaddr = saddr;
	inaddr2str(abuf, &saddr);
	hx_printf_prefix(htlc, 0, INFOPREFIX, "connecting to %s:%u\n", abuf, ntohs(saddr.SIN_PORT));

is_unix:
	hxd_files[s].ready_read = htlc_read;
	hxd_files[s].ready_write = htlc_write_connect;
	hxd_files[s].conn.htlc = htlc;
	hxd_fd_add(s);
	hxd_fd_set(s, FDR|FDW);
	htlc->fd = s;

is_pipe:
	hx_output.status();

	if (name)
		strlcpy(htlc->name, name, sizeof(htlc->name));
	if (icon)
		htlc->icon = icon;
	if (login)
		strlcpy(htlc->login, login, sizeof(htlc->login));
	if (pass)
		strlcpy(htlc->password, pass, sizeof(htlc->password));
	else
		htlc->password[0] = 0;
	htlc->secure = secure;
	htlc->flags.in_login = 1;
	htlc->rcv = hx_rcv_magic;
	htlc->trans = 1;
	memset(&htlc->in, 0, sizeof(struct qbuf));
	memset(&htlc->out, 0, sizeof(struct qbuf));
	qbuf_set(&htlc->in, 0, HTLS_MAGIC_LEN);
	qbuf_add(&htlc->out, HTLC_MAGIC, HTLC_MAGIC_LEN);
}

void
hx_connect_finish (struct htlc_conn *htlc)
{
	char enclogin[64], encpass[64];
	char abuf[HOSTLEN+1];
	u_int16_t icon16;
	u_int16_t llen, plen;
	int secure;
	u_int8_t *login, *pass;

	secure = htlc->secure;
	login = htlc->login;
	pass = htlc->password;
	inaddr2str(abuf, &htlc->sockaddr);
#ifdef CONFIG_HOPE
	if (secure) {
#ifdef CONFIG_CIPHER
		u_int8_t cipheralglist[64];
		u_int16_t cipheralglistlen;
		u_int8_t cipherlen;
#endif
#ifdef CONFIG_COMPRESS
		u_int8_t compressalglist[64];
		u_int16_t compressalglistlen;
		u_int8_t compresslen;
#endif
		u_int16_t hc;
		u_int8_t macalglist[64];
		u_int16_t macalglistlen;

		abuf[0] = 0;
		task_new(htlc, rcv_task_login, htlc, 0, "login");
		strcpy(htlc->macalg, "HMAC-SHA1");
		S16HTON(2, macalglist);
		macalglistlen = 2;
		macalglist[macalglistlen] = 9;
		macalglistlen++;
		memcpy(macalglist+macalglistlen, htlc->macalg, 9);
		macalglistlen += 9;
		macalglist[macalglistlen] = 8;
		macalglistlen++;
		memcpy(macalglist+macalglistlen, "HMAC-MD5", 8);
		macalglistlen += 8;

		hc = 4;
#ifdef CONFIG_COMPRESS
		if (htlc->compressalg[0]) {
			compresslen = strlen(htlc->compressalg);
			S16HTON(1, compressalglist);
			compressalglistlen = 2;
			compressalglist[compressalglistlen] = compresslen;
			compressalglistlen++;
			memcpy(compressalglist+compressalglistlen, htlc->compressalg, compresslen);
			compressalglistlen += compresslen;
			hc++;
		} else
			compressalglistlen = 0;
#endif
#ifdef CONFIG_CIPHER
		if (htlc->cipheralg[0]) {
			cipherlen = strlen(htlc->cipheralg);
			S16HTON(1, cipheralglist);
			cipheralglistlen = 2;
			cipheralglist[cipheralglistlen] = cipherlen;
			cipheralglistlen++;
			memcpy(cipheralglist+cipheralglistlen, htlc->cipheralg, cipherlen);
			cipheralglistlen += cipherlen;
			hc++;
		} else
			cipheralglistlen = 0;
#endif
		hlwrite(htlc, HTLC_HDR_LOGIN, 0, hc,
			HTLC_DATA_LOGIN, 1, abuf,
			HTLC_DATA_PASSWORD, 1, abuf,
			HTLC_DATA_MAC_ALG, macalglistlen, macalglist,
#ifdef CONFIG_CIPHER
			HTLC_DATA_CIPHER_ALG, cipheralglistlen, cipheralglist,
#endif
#ifdef CONFIG_COMPRESS
			HTLC_DATA_COMPRESS_ALG, compressalglistlen, compressalglist,
#endif
			HTLC_DATA_SESSIONKEY, 0, 0);
		return;
	}
#endif /* HOPE */

	task_new(htlc, rcv_task_login, 0, 0, "login");

	icon16 = htons(htlc->icon);
	if (login) {
		llen = strlen(login);
		if (llen > 32)
			llen = 32;
		hl_encode(enclogin, login, llen);
	} else
		llen = 0;
	htlc->clientversion = g_clientversion;
	if (htlc->clientversion >= 150) {
		if (pass) {
			u_int16_t cv = htons(185);
			plen = strlen(pass);
			if (plen > 32)
				plen = 32;
			hl_encode(encpass, pass, plen);
			hlwrite(htlc, HTLC_HDR_LOGIN, 0, 3,
				HTLC_DATA_LOGIN, llen, enclogin,
				HTLC_DATA_PASSWORD, plen, encpass,
				HTLC_DATA_CLIENTVERSION, 2, &cv);
		} else {
			u_int16_t cv = htons(185);
			hlwrite(htlc, HTLC_HDR_LOGIN, 0, 2,
				HTLC_DATA_LOGIN, llen, enclogin,
				HTLC_DATA_CLIENTVERSION, 2, &cv);
		}
	} else { /* 123 */
		if (pass) {
			plen = strlen(pass);
			if (plen > 32)
				plen = 32;
			hl_encode(encpass, pass, plen);
			hlwrite(htlc, HTLC_HDR_LOGIN, 0, 4,
				HTLC_DATA_ICON, 2, &icon16,
				HTLC_DATA_LOGIN, llen, enclogin,
				HTLC_DATA_PASSWORD, plen, encpass,
				HTLC_DATA_NAME, strlen(htlc->name), htlc->name);
		} else {
			hlwrite(htlc, HTLC_HDR_LOGIN, 0, 3,
				HTLC_DATA_ICON, 2, &icon16,
				HTLC_DATA_LOGIN, llen, enclogin,
				HTLC_DATA_NAME, strlen(htlc->name), htlc->name);
		}
	}
	memset(htlc->password, 0, sizeof(htlc->password));
}
