#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#if !defined(__WIN32__)
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/wait.h>
#endif
#include <sys/stat.h>
#include "hlserver.h"
#include "sys_net.h"
#include "rcv.h"
#include "xmalloc.h"
#include "threads.h"
#include "queue.h"
#include "protocols.h"
#include "snd.h"

#if defined(CONFIG_MODULES)
#include "module.h"
#endif

#if defined(CONFIG_NOSPAM)
#include "spam.h"
#endif

#ifdef CONFIG_COMPRESS
#include "compress.h"
#endif

#ifdef SOCKS
#include "socks.h"
#endif

u_int16_t nhtlc_conns = 0;

struct htlc_conn __htlc_list, *htlc_list = &__htlc_list, *htlc_tail = &__htlc_list;
#ifdef CONFIG_EXEC
extern void exec_close_all (struct htlc_conn *htlc);
#endif

#if defined(CONFIG_MODULES)
void
server_modules_load (void)
{
	char *mod;
	void *dh;
	unsigned int i;

	for (i = 0; hxd_cfg.modules.load[i]; i++) {
		mod = hxd_cfg.modules.load[i];
		dh = module_open(mod, 1);
		if (!dh) {
			hxd_log("failed to open module %s: %s", mod, module_error());
			continue;
		}
		hxd_log("loaded module %s", mod);
		module_run(dh);
	}
}

void
server_modules_reload (void)
{
}
#endif

/* returns the htlc_conn associated with uid */
struct htlc_conn *
isclient (u_int16_t uid)
{
	struct htlc_conn *htlcp;

	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (htlcp->uid == uid)
			return htlcp;
	}

	return 0;
}

u_int16_t
uid_assign (struct htlc_conn *htlc __attribute__((__unused__)))
{
	u_int16_t high_uid;
	struct htlc_conn *htlcp;

	high_uid = 0;
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (htlcp->uid > high_uid)
			high_uid = htlcp->uid;
	}

	return high_uid+1;
}

void
htlc_flush_close (struct htlc_conn *htlc)
{
	int fd = htlc->fd;

	if (hxd_files[fd].ready_write)
		hxd_files[fd].ready_write(fd);
	/* htlc_close might be called in htlc_write */
	if (hxd_files[fd].conn.htlc)
		htlc_close(htlc);
}

int
proto_should_reset (struct htlc_conn *htlc)
{
	if (htlc->rcv == protocol_rcv_magic)
		return 0;
	if (!htlc->proto)
		return 0;
	if (htlc->proto->ftab.should_reset)
		return htlc->proto->ftab.should_reset(htlc);

	return 0;
}

void
proto_reset (struct htlc_conn *htlc)
{
	if (!htlc->proto)
		return;
	if (htlc->proto->ftab.reset)
		htlc->proto->ftab.reset(htlc);
}


#define READ_BUFSIZE	1024
extern unsigned int decode (struct htlc_conn *htlc);

static void
htlc_read (int fd)
{
	ssize_t r;
	struct htlc_conn *htlc = hxd_files[fd].conn.htlc;
	struct qbuf *in = &htlc->read_in;
	int do_reset;
	int err;

	if (in->len == 0) {
		if (in->pos >= READ_BUFSIZE) {
			htlc_close(htlc);
			return;
		}
		qbuf_set(in, in->pos, READ_BUFSIZE);
		in->len = 0;
	}
	r = socket_read(fd, &in->buf[in->pos], READ_BUFSIZE-in->len);
	if (r <= 0) {
		err = socket_errno();
#if defined(__WIN32__)
		if (r == 0 || (r < 0 && err != WSAEWOULDBLOCK && err != WSAEINTR)) {
#else
		if (r == 0 || (r < 0 && err != EWOULDBLOCK && err != EINTR)) {
#endif
			/*hxd_log("htlc_read; %d %s", r, strerror(errno));*/
			htlc_close(htlc);
		}
	} else {
		in->len += r;
		for (;;) {
			r = decode(htlc);
			if (!r)
				break;
			if (htlc->rcv) {
				do_reset = proto_should_reset(htlc);
				htlc->rcv(htlc);
				/* htlc->rcv could have called htlc_close */
				if (!hxd_files[fd].conn.htlc)
					return;
				if (do_reset)
					goto reset;
			} else {
reset:
				/* Check idle status after a transaction is completed */
				if (!htlc->access_extra.can_login) {
					if (htlc->flags.is_hl) {
						if (htlc->rcv != rcv_user_getlist)
							test_away(htlc);
					} else if (htlc->flags.is_frogblast) {
						if (htlc->rcv != rcv_user_getinfo)
							test_away(htlc);
					} else {
						test_away(htlc);
					}
				}
				proto_reset(htlc);
			}
		}
	}
}

static void
htlc_write (int fd)
{
	ssize_t r;
	struct htlc_conn *htlc = hxd_files[fd].conn.htlc;
	int err;

	if (htlc->out.len == 0) {
		/*hxd_log("htlc->out.len == 0 but htlc_write was called...");*/
		hxd_fd_clr(fd, FDW);
		return;
	}
	r = socket_write(fd, &htlc->out.buf[htlc->out.pos], htlc->out.len);
	if (r <= 0) {
		err = socket_errno();
#if defined(__WIN32__)
		if (r == 0 || (r < 0 && err != WSAEWOULDBLOCK && err != WSAEINTR)) {
#else
		if (r == 0 || (r < 0 && err != EWOULDBLOCK && err != EINTR)) {
#endif
			/*hxd_log("htlc_write(%u); %d %s", htlc->out.len, r, strerror(errno));*/
			htlc_close(htlc);
		}
	} else {
		htlc->out.pos += r;
		htlc->out.len -= r;
		if (!htlc->out.len) {
			htlc->out.pos = 0;
			htlc->out.len = 0;
			hxd_fd_clr(fd, FDW);
		}
	}
}

extern void ident_close (int fd);

static int
login_timeout (struct htlc_conn *htlc)
{
	if (hxd_cfg.options.ident && htlc->identfd != -1) {
		ident_close(htlc->identfd);
		return 1;
	}
	if (htlc->access_extra.can_login)
		htlc_close(htlc);

	return 0;
}

/*
 * Allocate a new htlc connection and initialize it
 * Suitable to call before checking the banlist
 * Must set htlc->fd and htlc->sockaddr after calling
 */
static struct htlc_conn *
htlc_new (void)
{
	struct htlc_conn *htlc;

	htlc = xmalloc(sizeof(struct htlc_conn));
	memset(htlc, 0, sizeof(struct htlc_conn));
	/* htlc->next = 0; */
	htlc->prev = htlc_tail;
	htlc_tail->next = htlc;
	htlc_tail = htlc;
	nhtlc_conns++;
	htlc->identfd = -1;
	htlc->access_extra.can_login = 1;
	mutex_init(&htlc->htxf_mutex);

	return htlc;
}

/*
 * Call after checking the banlist
 */
static void
htlc_accepted (struct htlc_conn *htlc)
{
	int s = htlc->fd;

	hxd_files[s].ready_read = htlc_read;
	hxd_files[s].ready_write = htlc_write;
	hxd_files[s].conn.htlc = htlc;

	htlc->rcv = protocol_rcv_magic;
	htlc->trans = 1;
	htlc->replytrans = 1;
#if defined(CONFIG_EXEC)
	htlc->exec_limit = hxd_cfg.limits.individual_exec;
#endif
#if defined(CONFIG_NOSPAM)
	htlc->spam_max = hxd_cfg.spam.spam_max;
	htlc->spam_time_limit = hxd_cfg.spam.spam_time;
	htlc->spam_chat_max = hxd_cfg.spam.chat_max;
	htlc->spam_chat_time_limit = hxd_cfg.spam.chat_time;
#endif
	htlc->put_limit = hxd_cfg.limits.individual_uploads > HTXF_PUT_MAX ? HTXF_PUT_MAX : hxd_cfg.limits.individual_uploads;
	htlc->get_limit = hxd_cfg.limits.individual_downloads > HTXF_GET_MAX ? HTXF_GET_MAX : hxd_cfg.limits.individual_downloads;
	htlc->limit_out_Bps = hxd_cfg.limits.out_Bps;
	htlc->limit_uploader_out_Bps = hxd_cfg.limits.uploader_out_Bps;
	mutex_init(&htlc->htxf_mutex);

	gettimeofday(&htlc->login_tv, 0);
	gettimeofday(&htlc->idle_tv, 0);
	 
	htlc->flags.visible = 1;

	timer_add_secs(10, login_timeout, htlc);

	if (hxd_cfg.options.ident && !htlc->flags.sock_unix) {
		start_ident(htlc);
	} else {
		/* qbuf_set(&htlc->in, 0, HTLC_MAGIC_LEN); */
		qbuf_set(&htlc->in, 0, 0);
		hxd_fd_set(s, FDR);
	}
}

#if defined(CONFIG_UNIXSOCKETS)
static void
unix_listen_ready_read (int fd)
{
	int s;
	struct sockaddr_un saddr;
	int siz = sizeof(saddr);
	struct htlc_conn *htlc;

	s = accept(fd, (struct sockaddr *)&saddr, &siz);
	if (s < 0) {
		hxd_log("htls: accept: %s", strerror(errno));
		return;
	}
	nr_open_files++;
	if (nr_open_files >= hxd_open_max) {
		hxd_log("%s: %d >= hxd_open_max (%d)", saddr.sun_path, s, hxd_open_max);
		socket_close(s);
		nr_open_files--;
		return;
	}
	fd_closeonexec(s, 1);
	socket_blocking(s, 0);
	if (high_fd < s)
		high_fd = s;

	htlc = htlc_new();
	htlc->fd = s;
	htlc->usockaddr = saddr;
	htlc->flags.sock_unix = 1;

	hxd_log("%s -- htlc connection accepted", "local socket");

	htlc_accepted(htlc);
}
#endif

static void
listen_ready_read (int fd)
{
	int s;
	struct SOCKADDR_IN saddr;
	int siz = sizeof(saddr);
	char abuf[HOSTLEN+1];
	struct htlc_conn *htlc;
#if defined(CONFIG_NOSPAM)
	int reason;
	char rstr[64];
#endif

	s = accept(fd, (struct sockaddr *)&saddr, &siz);
	if (s < 0) {
		hxd_log("htls: accept: %s", strerror(errno));
		return;
	}
	nr_open_files++;
	inaddr2str(abuf, &saddr);
	if (nr_open_files >= hxd_open_max) {
		hxd_log("%s:%u: %d >= hxd_open_max (%d)", abuf, ntohs(saddr.SIN_PORT), s, hxd_open_max);
		socket_close(s);
		nr_open_files--;
		return;
	}

	fd_closeonexec(s, 1);
	socket_blocking(s, 0);
	if (high_fd < s)
		high_fd = s;

	htlc = htlc_new();
	htlc->fd = s;
	htlc->sockaddr = saddr;

	/* 
	 * Make sure known bad addresses are banned before
	 * they can fill up the connection spam queue.
	 */
	if (check_banlist(htlc))
		return;
#if defined(CONFIG_NOSPAM)
	if (hxd_cfg.operation.nospam)
		reason = check_connections(&saddr);
	else
		reason = 0;
	if (reason) {
		if (reason == 1)
			snprintf(rstr, sizeof(rstr),
				"too many (%d) connections from same address",
				hxd_cfg.spam.conn_max);
		else
			snprintf(rstr, sizeof(rstr),
				"two connections too fast (in under %d seconds)",
				hxd_cfg.spam.reconn_time);
		hxd_log("%s:%u -- htlc connection refused: %s",
			abuf, ntohs(saddr.SIN_PORT), rstr);
		socket_close(s);
		nr_open_files--;
		nhtlc_conns--;
		if (htlc->next)
			htlc->next->prev = htlc->prev;
		if (htlc->prev)
			htlc->prev->next = htlc->next;
		if (htlc_tail == htlc)
			htlc_tail = htlc->prev;
		xfree(htlc);
		return;
	}
#endif
	hxd_log("%s:%u -- htlc connection accepted", abuf, ntohs(saddr.SIN_PORT));

	htlc_accepted(htlc);
}

static hxd_mutex_t resolve_mutex;

char *
resolve (struct SOCKADDR_IN *saddr)
{
	struct hostent *he;

	/* gethostbyaddr and its evil cohorts may not be thread-safe */
	mutex_lock(&resolve_mutex);
	he = gethostbyaddr((char *)&saddr->SIN_ADDR, sizeof(saddr->SIN_ADDR), AFINET);
	mutex_unlock(&resolve_mutex);
	if (he)
		return he->h_name;
	else
		return 0;
}

#if defined(CONFIG_HTXF_PTHREAD)
static void
join_thread (hxd_thread_t tid)
{
	int err;
	thread_return_type retval;

	err = pthread_join(tid, &retval);
	if (err)
		hxd_log("pthread_join error: %s", strerror(err));
}
#endif

void
htlc_close (struct htlc_conn *htlc)
{
	int fd = htlc->fd;
	char abuf[HOSTLEN+1];
	int wr;
	hxd_thread_t tid;
	u_int16_t i;
	int can_login;
#if defined(CONFIG_HTXF_CLONE) || defined(CONFIG_HTXF_FORK)
	int status;
#endif

	socket_close(fd);
	nr_open_files--;
	hxd_fd_clr(fd, FDR|FDW);
	hxd_fd_del(fd);
	memset(&hxd_files[fd], 0, sizeof(struct hxd_file));
	if (htlc->identfd != -1) {
		int ifd = htlc->identfd;
		socket_close(ifd);
		nr_open_files--;
		hxd_fd_clr(ifd, FDR|FDW);
		hxd_fd_del(ifd);
		memset(&hxd_files[ifd], 0, sizeof(struct hxd_file));
	}
	inaddr2str(abuf, &htlc->sockaddr);
	hxd_log("%s@%s:%u - %s:%u:%u:%s - htlc connection closed",
		htlc->userid, abuf, ntohs(htlc->sockaddr.SIN_PORT),
		htlc->name, htlc->icon, htlc->uid, htlc->login);
#if defined(CONFIG_SQL)
	sql_delete_user(htlc->userid, htlc->name, abuf, ntohs(htlc->sockaddr.SIN_PORT), htlc->login, htlc->uid);
#endif
	timer_delete_ptr(htlc);
	if (htlc->next)
		htlc->next->prev = htlc->prev;
	if (htlc->prev)
		htlc->prev->next = htlc->next;
	if (htlc_tail == htlc)
		htlc_tail = htlc->prev;
	can_login = htlc->access_extra.can_login;
	if (!can_login) {
		chat_remove_from_all(htlc);
		snd_user_part(htlc);
	}
	if (htlc->read_in.buf)
		xfree(htlc->read_in.buf);
	if (htlc->in.buf)
		xfree(htlc->in.buf);
	if (htlc->out.buf)
		xfree(htlc->out.buf);
	mutex_lock(&htlc->htxf_mutex);
	for (i = 0; i < HTXF_PUT_MAX; i++) {
		if (htlc->htxf_in[i]) {
			wr = 0;
			tid = htlc->htxf_in[i]->tid;
#if defined(CONFIG_HTXF_PTHREAD)
			if (tid) {
				int err = pthread_cancel(tid);
				if (err)
					hxd_log("pthread_cancel error: %s", strerror(err));
				join_thread(tid);
				wr = (int)tid;
			}
#elif defined(CONFIG_HTXF_CLONE)
			mask_signal(SIG_BLOCK, SIGCHLD);
			if (tid) {
				kill(tid, SIGTERM);
				wr = waitpid(tid, &status, 0);
			}
			mask_signal(SIG_UNBLOCK, SIGCHLD);
			if (htlc->htxf_in[i]->stack)
				xfree(htlc->htxf_in[i]->stack);
#elif defined(CONFIG_HTXF_FORK)
			mask_signal(SIG_BLOCK, SIGCHLD);
			if (tid) {
				kill(tid, SIGTERM);
				wr = waitpid(tid, &status, 0);
			}
			mask_signal(SIG_UNBLOCK, SIGCHLD);
#endif
			xfree(htlc->htxf_in[i]);
			if (wr != 0 && wr != -1)
				nr_puts--;
		}
	}
	for (i = 0; i < HTXF_GET_MAX; i++) {
		if (htlc->htxf_out[i]) {
			wr = 0;
			tid = htlc->htxf_out[i]->tid;
#if defined(CONFIG_HTXF_PTHREAD)
			if (tid) {
				int err = pthread_cancel(tid);
				if (err)
					hxd_log("pthread_cancel error: %s", strerror(err));
				join_thread(tid);
				wr = (int)tid;
			}
#elif defined(CONFIG_HTXF_CLONE)
			mask_signal(SIG_BLOCK, SIGCHLD);
			if (tid) {
				kill(tid, SIGTERM);
				wr = waitpid(tid, &status, 0);
			}
			mask_signal(SIG_UNBLOCK, SIGCHLD);
			if (htlc->htxf_out[i]->stack)
				xfree(htlc->htxf_out[i]->stack);
#elif defined(CONFIG_HTXF_FORK)
			mask_signal(SIG_BLOCK, SIGCHLD);
			if (tid) {
				kill(tid, SIGTERM);
				wr = waitpid(tid, &status, 0);
			}
			mask_signal(SIG_UNBLOCK, SIGCHLD);
#endif
			xfree(htlc->htxf_out[i]);
			if (wr != 0 && wr != -1)
				nr_gets--;
		}
	}
	mutex_unlock(&htlc->htxf_mutex);
#ifdef CONFIG_COMPRESS
	if (htlc->compress_encode_type != COMPRESS_NONE)
		compress_encode_end(htlc);
	if (htlc->compress_decode_type != COMPRESS_NONE)
		compress_decode_end(htlc);
#endif
#ifdef CONFIG_EXEC
	exec_close_all(htlc);
#endif
	if (htlc->icon_gif != NULL)
		xfree(htlc->icon_gif);
	
	xfree(htlc);
	nhtlc_conns--;
	if (!can_login) {
#ifdef CONFIG_TRACKER_REGISTER
		if (hxd_cfg.operation.tracker_register)
			tracker_register_timer(0);
#endif
	}
#if defined(CONFIG_HTXF_QUEUE)
	queue_refresh();
#endif
}

static int
free_htxf (struct htlc_conn *htlc, u_int16_t i, int get)
{
	mutex_lock(&htlc->htxf_mutex);
	if (get) {
		if (!htlc->htxf_out[i])
			return 0;
#if defined(CONFIG_HTXF_PTHREAD)
		join_thread(htlc->htxf_out[i]->tid);
#endif
#if defined(CONFIG_HTXF_CLONE)
		xfree(htlc->htxf_out[i]->stack);
#endif
		xfree(htlc->htxf_out[i]);
		htlc->htxf_out[i] = 0;
		htlc->nr_gets--;
		nr_gets--;
	} else {
		if (!htlc->htxf_in[i])
			return 0;
#if defined(CONFIG_HTXF_PTHREAD)
		join_thread(htlc->htxf_in[i]->tid);
#endif
#if defined(CONFIG_HTXF_CLONE)
		xfree(htlc->htxf_in[i]->stack);
#endif
		xfree(htlc->htxf_in[i]);
		htlc->htxf_in[i] = 0;
		htlc->nr_puts--;
		nr_puts--;
	}
	mutex_unlock(&htlc->htxf_mutex);

#if defined(CONFIG_HTXF_QUEUE)
	queue_refresh();
#endif

	return 0;
}

#if defined(CONFIG_HTXF_PTHREAD)
int
thread_check (void *p __attribute__((__unused__)))
{
	struct htlc_conn *htlcp;
	u_int16_t i;

	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		for (i = 0; i < HTXF_GET_MAX; i++) {
			if (htlcp->htxf_out[i]) {
				if (htlcp->htxf_out[i]->gone) {
					free_htxf(htlcp, i, 1);
				}
			}
		}
		for (i = 0; i < HTXF_PUT_MAX; i++) {
			if (htlcp->htxf_in[i]) {
				if (htlcp->htxf_in[i]->gone) {
					free_htxf(htlcp, i, 0);
				}
			}
		}
	}

	return 0;
}
#endif

void
hlserver_reap_pid (pid_t pid, int status __attribute__((__unused__)))
{
#if defined(CONFIG_HTXF_CLONE) || defined(CONFIG_HTXF_FORK)
	struct htlc_conn *htlcp;
	u_int16_t i;

	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		for (i = 0; i < HTXF_GET_MAX; i++) {
			if (htlcp->htxf_out[i]) {
				if (htlcp->htxf_out[i]->tid == pid) {
					free_htxf(htlcp, i, 0);
					return;
				}
			}
		}
		for (i = 0; i < HTXF_PUT_MAX; i++) {
			if (htlcp->htxf_in[i]) {
				if (htlcp->htxf_in[i]->tid == pid) {
					free_htxf(htlcp, i, 1);
					return;
				}
			}
		}
	}
#endif
}

extern void hotline_protocol_init (void);
extern void irc_protocol_init (void);
extern void kdx_protocol_init (void);

void
hotline_server_init (void)
{
	struct SOCKADDR_IN saddr;
	int x, r, i, listen_sock;
	char *host;
#if defined(CONFIG_UNIXSOCKETS)
	struct sockaddr_un usaddr;
	struct stat sb;
#endif

	mutex_init(&resolve_mutex);

	memset(&__htlc_list, 0, sizeof(__htlc_list));

#if defined(CONFIG_PROTO_HOTLINE)
	hotline_protocol_init();
#endif
#if defined(CONFIG_PROTO_IRC)
	irc_protocol_init();
#endif
#if defined(CONFIG_PROTO_KDX)
	kdx_protocol_init();
#endif

#if defined(CONFIG_HTXF_QUEUE)
	queue_init();
#endif

#if defined(CONFIG_UNIXSOCKETS)
	memset(&usaddr, 0, sizeof(usaddr));
	usaddr.sun_family = AF_UNIX;
	if (strlen(hxd_cfg.options.unix_address) > sizeof(usaddr.sun_path)-1) {
		hxd_log("unix socket path %s is too long", hxd_cfg.options.unix_address);
		exit(1);
	}
	strcpy(usaddr.sun_path, hxd_cfg.options.unix_address);
	listen_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (listen_sock < 0) {
		hxd_log("unix socket() failed");
		exit(1);
	}
	if (!stat(usaddr.sun_path, &sb)) {
		if (unlink(usaddr.sun_path)) {
			hxd_log("unlink(%s) failed", usaddr.sun_path);
			exit(1);
		}
	}
	r = bind(listen_sock, (struct sockaddr *)&usaddr, sizeof(usaddr));
	if (r < 0) {
		hxd_log("unix bind(%s) failed", usaddr.sun_path);
		exit(1);
	}
	r = listen(listen_sock, 5);
	if (r < 0) {
		hxd_log("unix listen() failed: %s", strerror(errno));
		exit(1);
	}
	hxd_files[listen_sock].ready_read = unix_listen_ready_read;
	hxd_fd_set(listen_sock, FDR);
	if (high_fd < listen_sock)
		high_fd = listen_sock;
	fd_closeonexec(listen_sock, 1);
	socket_blocking(listen_sock, 0);
#endif

	for (i = 0; (host = hxd_cfg.options.addresses[i]); i++) {
		memset(&saddr, 0, sizeof(saddr));
#if defined(CONFIG_IPV6)
		if (!inet_pton(AFINET, host, &saddr.SIN_ADDR)) {
#else
		if (!inet_aton(host, &saddr.SIN_ADDR)) {
#endif
			struct hostent *he = gethostbyname(host);
			if (he)
				memcpy(&saddr.SIN_ADDR, he->h_addr_list[0],
				       (size_t)he->h_length > sizeof(saddr.SIN_ADDR) ?
				       sizeof(saddr.SIN_ADDR) : (size_t)he->h_length);
			else {
				hxd_log("%s:%d: could not resolve hostname %s", __FILE__, __LINE__, host);
				exit(1);
			}
		}

		listen_sock = socket(AFINET, SOCK_STREAM, IPPROTO_TCP);
		if (listen_sock < 0) {
#if defined(__WIN32__)
			hxd_log("%s:%d: socket: WSA %d", __FILE__, __LINE__, listen_sock, WSAGetLastError());
#endif
			hxd_log("%s:%d: socket: %s", __FILE__, __LINE__, strerror(errno));
			exit(1);
		}
		nr_open_files++;
		if (nr_open_files >= hxd_open_max) {
			hxd_log("%s:%d: %d >= hxd_open_max (%d)", __FILE__, __LINE__, listen_sock, hxd_open_max);
			socket_close(listen_sock);
			nr_open_files--;
			exit(1);
		}
		x = 1;
		setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (SETSOCKOPT_PTR_CAST_T)&x, sizeof(x));
		saddr.SIN_FAMILY = AFINET;
		saddr.SIN_PORT = htons(hxd_cfg.options.port);
#ifdef CONFIG_EUID
		if (hxd_cfg.options.port <= 1024) {
			r = seteuid(0);
			if (r)
				hxd_log("seteuid(0): %s", strerror(errno));
		}
#endif
		r = bind(listen_sock, (struct sockaddr *)&saddr, sizeof(saddr));
#ifdef CONFIG_EUID
		if (hxd_cfg.options.port <= 1024)
			seteuid(getuid());
#endif
		if (r < 0) {
			char abuf[HOSTLEN+1];

			inaddr2str(abuf, &saddr);
			hxd_log("%s:%d: bind(%s:%u): %s", __FILE__, __LINE__, abuf, ntohs(saddr.SIN_PORT), strerror(errno));
			exit(1);
		}
		if (listen(listen_sock, 5) < 0) {
			hxd_log("%s:%d: listen: %s", __FILE__, __LINE__, strerror(errno));
			exit(1);
		}

		hxd_files[listen_sock].ready_read = listen_ready_read;
		hxd_fd_set(listen_sock, FDR);
		if (high_fd < listen_sock)
			high_fd = listen_sock;
		fd_closeonexec(listen_sock, 1);
		socket_blocking(listen_sock, 0);

		htxf_init(&saddr);
	}
}
