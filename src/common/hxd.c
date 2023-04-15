#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#if !defined(__WIN32__)
#include <sys/wait.h>
#include <sys/ioctl.h>
#endif
#include "xmalloc.h"
#include "hxd.h"
#include "hxd_config.h"
#if !defined(_SC_OPEN_MAX) && defined(HAVE_GETRLIMIT)
#include <sys/time.h>
#include <sys/resource.h>
#endif

#if defined(ONLY_CLIENT)
#include "hx.h"
#undef CONFIG_HOTLINE_SERVER
#undef CONFIG_TRACKER_REGISTER
#undef CONFIG_TRACKER_SERVER
#elif defined(ONLY_SERVER)
#include "hlserver.h"
#undef CONFIG_HOTLINE_CLIENT
#undef CONFIG_TRACKER_SERVER
#elif defined(ONLY_TRACKER)
#undef CONFIG_HOTLINE_SERVER
#undef CONFIG_TRACKER_REGISTER
#undef CONFIG_HOTLINE_CLIENT
#endif

#if defined(CONFIG_MODULES)
#include "module.h"
#endif

#if defined(CONFIG_HOTLINE_SERVER)

#if defined(CONFIG_NOSPAM)
#include "spam.h"
#endif
#if defined(CONFIG_EXEC)
extern void exec_init (void);
#endif

#endif

#if defined(CONFIG_TRACKER_SERVER)
#define _PATH_HXD_CONF "hxtrackd.conf"
#else
#define _PATH_HXD_CONF "hxd.conf"
#endif

extern void hotline_server_init (void);
extern void hotline_client_init (int argc, char **argv);
extern void tracker_server_init (void);
extern void tracker_register_init (void);

char **hxd_environ = 0;

int hxd_open_max = 0;
int nr_open_files = 3; /* 3 ? */

struct hxd_file *hxd_files = 0;

pid_t hxd_pid;

int high_fd = 0;

fd_set hxd_rfds, hxd_wfds;

struct timeval server_start_time;

#define QBUF_SIZE_LIMIT	0x1000

void
qbuf_set (struct qbuf *q, u_int32_t pos, u_int32_t len)
{
	u_int32_t size = q->pos + q->len;
	/* if the size was very large, reallocate */
	int need_realloc = (size < pos + len) || (size > QBUF_SIZE_LIMIT);

	q->pos = pos;
	q->len = len;
	if (need_realloc)
		q->buf = xrealloc(q->buf, q->pos + q->len + 1); /* +1 for null */
}

void
qbuf_add (struct qbuf *q, void *buf, u_int32_t len)
{
	size_t pos = q->pos + q->len;

	qbuf_set(q, q->pos, q->len + len);
	memcpy(&q->buf[pos], buf, len);
}

void
inaddr2str (char abuf[HOSTLEN+1], struct SOCKADDR_IN *sa)
{
#ifdef CONFIG_IPV6
	inet_ntop(AFINET, (char *)&sa->SIN_ADDR, abuf, HOSTLEN+1);
#else
	inet_ntoa(sa->SIN_ADDR, abuf, 16);
#endif
}

#if defined(CONFIG_HOTLINE_SERVER) || defined(CONFIG_TRACKER_SERVER)
/* default is stderr (2) */
static int log_fd = 2;
#endif

void
hxd_log (const char *fmt, ...)
{
	va_list ap;
	char buf[2048];
	int len;
	time_t t;
	struct tm tm;

#if !defined(CONFIG_HOTLINE_CLIENT)
	if (log_fd < 0)
		return;
#endif
	time(&t);
	localtime_r(&t, &tm);
	strftime(buf, 23, "[%d/%m/%Y:%H:%M:%S]\t", &tm);
	va_start(ap, fmt);
	len = vsnprintf(&buf[22], sizeof(buf) - 24, fmt, ap);
	va_end(ap);
	if (len == -1)
		len = sizeof(buf) - 24;
	len += 22;
	buf[len++] = '\n';
#if defined(CONFIG_HOTLINE_CLIENT)
	buf[len] = 0;
	hx_printf_prefix(&hx_htlc, 0, INFOPREFIX, "%s", buf);
#else
	write(log_fd, buf, len);
	SYS_fsync(log_fd);
#endif
}

static int last_signal = 0;

#if !defined(__WIN32__)
extern void hlserver_reap_pid (pid_t pid, int status);
extern void hlclient_reap_pid (pid_t pid, int status);

#if !defined(ONLY_GTK)
static int
chld_wait (void *p __attribute__((__unused__)))
{
	int status;
	pid_t pid;

#ifndef WAIT_ANY
#define WAIT_ANY -1
#endif

	for (;;) {
		pid = waitpid(WAIT_ANY, &status, WNOHANG);
		if (pid < 0) {
			if (errno == EINTR)
				continue;
			return 0;
		}
		if (!pid)
			return 0;
#ifdef CONFIG_HOTLINE_SERVER
		hlserver_reap_pid(pid, status);
#endif
#ifdef CONFIG_HOTLINE_CLIENT
		hlclient_reap_pid(pid, status);
#endif
	}

	return 0;
}
#endif // not GTK

RETSIGTYPE
sig_chld (int sig)
{
	last_signal = sig;
}

static RETSIGTYPE
sig_bus (int sig __attribute__((__unused__)))
{
	hxd_log("\n\
caught SIGBUS -- mail ran@krazynet.com with output from:\n\
$ gcc -v hxd.c\n\
$ cc -v hxd.c\n\
$ gdb hxd core\n\
(gdb) backtrace\n\
and any other information you think is useful");
	abort();
}

#if defined(CONFIG_HTXF_PTHREAD) && defined(CONFIG_HOTLINE_SERVER)
extern int thread_check (void *p);

static int
thread_check_continuous (void *p)
{
	thread_check(p);
	return 1;
}
#endif

static RETSIGTYPE
sig_alrm (int sig)
{
	last_signal = sig;
}
#endif /* not WIN32 */

#if defined(CONFIG_CIPHER) && !defined(CONFIG_TRACKER_SERVER)
#include "cipher.h"

#if USE_OPENSSL
#include <openssl/rand.h>

#if defined(CONFIG_HOTLINE_CLIENT)
static char *egd_path = 0;

static void
set_egd_path (char **egd_pathp, const char *str)
{
	int r;

	if (*egd_pathp)
		xfree(*egd_pathp);
	*egd_pathp = xstrdup(str);
#if USE_OLD_OPENSSL
	r = -1;
#else
	r = RAND_egd(str);
#endif
	if (r == -1) {
		hx_printf_prefix(&hx_htlc, 0, INFOPREFIX,
				 "failed to get entropy from egd socket %s\n", str);
	}
}
#endif
#endif // OPENSSL

static void
cipher_init (void)
{
#if USE_OPENSSL
#if defined(CONFIG_HOTLINE_CLIENT)
	variable_add(&egd_path, set_egd_path, "egd_path");
#else
	int r;

#if USE_OLD_OPENSSL
	r = -1;
#else
	r = RAND_egd(hxd_cfg.cipher.egd_path);
#endif
	if (r == -1) {
		/*hxd_log("failed to get entropy from egd socket %s", hxd_cfg.cipher.egd_path);*/
	}
#endif
#else
	srand(getpid()*clock());
#endif
}
#endif /* CONFIG_CIPHER */

#if !defined(ONLY_GTK)
struct timer {
	struct timer *next;
	struct timeval add_tv;
	struct timeval tv;
	int (*fn)();
	void *ptr;
	u_int8_t expire;
};

static struct timer *timer_list = 0;

void
timer_add (struct timeval *tv, int (*fn)(), void *ptr)
{
	struct timer *timer, *timerp;

	timer = xmalloc(sizeof(struct timer));
	timer->add_tv = *tv;
	timer->tv = *tv;
	timer->fn = fn;
	timer->ptr = ptr;

	timer->expire = 0;

	if (!timer_list || (timer_list->tv.tv_sec > timer->tv.tv_sec
			    || (timer_list->tv.tv_sec == timer->tv.tv_sec && timer_list->tv.tv_usec > timer->tv.tv_usec))) {
		timer->next = timer_list;
		timer_list = timer;
		return;
	}
	for (timerp = timer_list; timerp; timerp = timerp->next) {
		if (!timerp->next || (timerp->next->tv.tv_sec > timer->tv.tv_sec
				      || (timerp->next->tv.tv_sec == timer->tv.tv_sec && timerp->next->tv.tv_usec > timer->tv.tv_usec))) {
			timer->next = timerp->next;
			timerp->next = timer;
			return;
		}
	}
}

void
timer_delete_ptr (void *ptr)
{
	struct timer *timerp, *next;

	if (!timer_list)
		return;
	while (timer_list->ptr == ptr) {
		next = timer_list->next;
		xfree(timer_list);
		timer_list = next;
		if (!timer_list)
			return;
	}
	for (timerp = timer_list; timerp->next; timerp = next) {
		next = timerp->next;
		if (next->ptr == ptr) {
			next = timerp->next->next;
			xfree(timerp->next);
			timerp->next = next;
			next = timerp;
		}
	}
}

void
timer_add_secs (time_t secs, int (*fn)(), void *ptr)
{
	struct timeval tv;
	tv.tv_sec = secs;
	tv.tv_usec = 0;
	timer_add(&tv, fn, ptr);
}

time_t
tv_secdiff (struct timeval *tv0, struct timeval *tv1)
{
	time_t ts;

	ts = tv1->tv_sec - tv0->tv_sec;
	if (tv1->tv_usec > tv0->tv_usec) {
		ts += 1;
		if (tv1->tv_usec - tv0->tv_usec >= 500000)
			ts += 1;
	} else if (tv0->tv_usec - tv1->tv_usec > 500000) {
		ts -= 1;
	}

	return ts;
}

static void
timer_check (struct timeval *before, struct timeval *after)
{
	struct timer *timer, *next, *prev;
	time_t secdiff, usecdiff;

	secdiff = after->tv_sec - before->tv_sec;
	if (before->tv_usec > after->tv_usec) {
		secdiff--;
		usecdiff = 1000000 - (before->tv_usec - after->tv_usec);
	} else {
		usecdiff = after->tv_usec - before->tv_usec;
	}
	for (timer = timer_list; timer; timer = timer->next) {
		if (secdiff > timer->tv.tv_sec
		    || (secdiff == timer->tv.tv_sec && usecdiff >= timer->tv.tv_usec)) {
			timer->expire = 1;
			timer->tv.tv_sec = timer->add_tv.tv_sec
					 - (secdiff - timer->tv.tv_sec);
			if (usecdiff > (timer->tv.tv_usec + timer->add_tv.tv_usec)) {
				timer->tv.tv_sec -= 1;
				timer->tv.tv_usec = 1000000 - timer->add_tv.tv_usec
						  + timer->tv.tv_usec - usecdiff;
			} else {
				timer->tv.tv_usec = timer->add_tv.tv_usec
						  + timer->tv.tv_usec - usecdiff;
			}
		} else {
			timer->tv.tv_sec -= secdiff;
			if (usecdiff > timer->tv.tv_usec) {
				timer->tv.tv_sec -= 1;
				timer->tv.tv_usec = 1000000 - (usecdiff - timer->tv.tv_usec);
			} else
				timer->tv.tv_usec -= usecdiff;
		}
	}

	prev = 0;
	for (timer = timer_list; timer; timer = next) {
		next = timer->next;
		if (timer->expire) {
			int keep;
			int (*fn)() = timer->fn, *ptr = timer->ptr;

			if (prev)
				prev->next = next;
			if (timer == timer_list)
				timer_list = next;
			keep = fn(ptr);
			if (keep)
				timer_add(&timer->add_tv, fn, ptr);
			xfree(timer);
			next = timer_list;
		} else {
			prev = timer;
		}
	}
}

void
hxd_fd_add (int fd)
{
	if (high_fd < fd)
		high_fd = fd;
}

void
hxd_fd_del (int fd)
{
	if (high_fd == fd) {
		for (fd--; fd && !FD_ISSET(fd, &hxd_rfds); fd--)
			;
		high_fd = fd;
	}
}

void
hxd_fd_set (int fd, int rw)
{
	if (rw & FDR)
		FD_SET(fd, &hxd_rfds);
	if (rw & FDW)
		FD_SET(fd, &hxd_wfds);
}

void
hxd_fd_clr (int fd, int rw)
{
	if (rw & FDR)
		FD_CLR(fd, &hxd_rfds);
	if (rw & FDW)
		FD_CLR(fd, &hxd_wfds);
}

#if defined(CONFIG_HOTLINE_SERVER) || defined(CONFIG_TRACKER_SERVER)

#if defined(CONFIG_HOTLINE_SERVER) && defined(CONFIG_HFS)
#include "hfs.h"
#endif

extern void hldump_init (const char *path);
extern void read_banlist (void);
extern void tracker_read_banlist (void);

static char *hxdconf, *pidfile;

static int
read_config_file (void *ptr)
{
	if (ptr)
		hxd_log("SIGHUP: rereading config file");

	hxd_config_read(hxdconf, &hxd_cfg, 1);

	umask(hxd_cfg.permissions.umask);
#if defined(CONFIG_HOTLINE_SERVER)
#if defined(CONFIG_NOSPAM)
	spam_sort();
#endif
	{
		struct htlc_conn *htlcp;
		for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
			if (htlcp->access_extra.can_login)
				continue;
			account_getconf(htlcp);
		}
	}
	read_banlist();
#if defined(CONFIG_HLDUMP)
	hldump_init(hxd_cfg.paths.hldump);
#endif
#if defined(CONFIG_HFS)
	read_applevolume(hxd_cfg.paths.avlist);
	hfs_set_config(hxd_cfg.files.fork, hxd_cfg.permissions.files,
		       hxd_cfg.permissions.directories, hxd_cfg.files.comment);
#endif
#ifdef CONFIG_CIPHER
	cipher_init();
#endif
#endif
#if defined(CONFIG_TRACKER_SERVER)
	tracker_read_banlist();
#endif
#if defined(CONFIG_TRACKER_REGISTER)
	tracker_register_init();
#endif
#if defined(CONFIG_HOTLINE_SERVER)
#if defined(CONFIG_SQL)
	init_database(hxd_cfg.sql.host, hxd_cfg.sql.user, hxd_cfg.sql.pass, hxd_cfg.sql.data);
#endif
#if defined(CONFIG_MODULES)
	if (!ptr) {
		module_init();
		server_modules_load();
	} else {
		server_modules_reload();
	}
#endif
#endif /* server */

	return 0;
}

static RETSIGTYPE
sig_hup (int sig)
{
	last_signal = sig;
}
#endif

static void loopZ (void) __attribute__((__noreturn__));

struct timeval loopZ_timeval;
	
static void
loopZ (void)
{
	fd_set rfds, wfds;
	struct timeval before, tv;

	gettimeofday(&tv, 0);
	for (;;) {
		register int n, i;

		if (timer_list) {
			gettimeofday(&before, 0);
			timer_check(&tv, &before);
			if (timer_list)
				tv = timer_list->tv;
		}
		rfds = hxd_rfds;
		wfds = hxd_wfds;
		n = select(high_fd + 1, &rfds, &wfds, 0, timer_list ? &tv : 0);
		if (n < 0) {
			if (errno != EINTR) {
				hxd_log("loopZ: select: %s", strerror(errno));
				exit(1);
			}
			switch (last_signal) {
				case SIGCHLD:
					timer_add_secs(0, chld_wait, 0);
					break;
				case SIGHUP:
#if defined(CONFIG_HOTLINE_SERVER) || defined(CONFIG_TRACKER_SERVER)
					timer_add_secs(0, read_config_file, (void *)1);
#endif
					break;
				case SIGALRM:
#if defined(CONFIG_HTXF_PTHREAD) && defined(CONFIG_HOTLINE_SERVER)
					timer_add_secs(0, thread_check, 0);
#endif
					break;
				default:
					break;
			}
			last_signal = 0;
		}
		gettimeofday(&tv, 0);
		loopZ_timeval = tv;
		if (timer_list) {
			timer_check(&before, &tv);
		}
		if (n <= 0)
			continue;
		for (i = 0; i < high_fd + 1; i++) {
			if (FD_ISSET(i, &rfds) && FD_ISSET(i, &hxd_rfds)) {
				if (hxd_files[i].ready_read)
					hxd_files[i].ready_read(i);
				n--;
				if (!n)
					break;
			}
			if (FD_ISSET(i, &wfds) && FD_ISSET(i, &hxd_wfds)) {
				if (hxd_files[i].ready_write)
					hxd_files[i].ready_write(i);
				n--;
				if (!n)
					break;
			}
		}
	}
}
#endif /* ndef ONLY_GTK */

/* this is just a test timer */
#if 0
static struct timeval tv1, tv2, tv3, ctv1,ctv2,ctv3;
int
tfn (struct timeval *tv)
{
	struct timeval *ctv;
	time_t s, us, secdiff, usecdiff;

	hxd_log("timer: %u, %u", tv->tv_sec, tv->tv_usec);
	if (tv==&tv1)ctv=&ctv1;else if (tv==&tv2)ctv=&ctv2;else if (tv==&tv3)ctv=&ctv3;
	s = ctv->tv_sec;
	us = ctv->tv_usec;
	gettimeofday(ctv,0);
	secdiff = ctv->tv_sec - s;
	if (us > ctv->tv_usec) {
		secdiff--;
		usecdiff = 1000000 - (us - ctv->tv_usec);
	} else {
		usecdiff = ctv->tv_usec - us;
	}
	hxd_log("real: %u, %u",secdiff,usecdiff);
	return 1;
}
void tfark () __attribute__((__constructor__));
void tfark ()
{
	tv1.tv_sec = 2;
	tv1.tv_usec = 100000;
	gettimeofday(&ctv1,0);
	timer_add(&tv1, tfn, &tv1);
	tv2.tv_sec = 1;
	tv2.tv_usec = 700000;
	gettimeofday(&ctv2,0);
	timer_add(&tv2, tfn, &tv2);
	tv3.tv_sec = 4;
	tv3.tv_usec = 000000;
	gettimeofday(&ctv3,0);
	timer_add(&tv3, tfn, &tv3);
}
#endif

static RETSIGTYPE
sig_fpe (int sig, int fpe)
{
	hxd_log("SIGFPE (%d): %d", sig, fpe);
	abort();
}

static void
signal_init (void)
{
#if !defined(__WIN32__)
	struct sigaction act;

	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGPIPE, &act, 0);
#if defined(CONFIG_HOTLINE_SERVER) || defined(CONFIG_TRACKER_SERVER)
	act.sa_handler = sig_hup;
	sigaction(SIGHUP, &act, 0);
#else
	/* sigaction(SIGHUP, &act, 0); */
#endif
	act.sa_handler = (RETSIGTYPE (*)(int))sig_fpe;
	sigaction(SIGFPE, &act, 0);
	act.sa_handler = sig_bus;
	sigaction(SIGBUS, &act, 0);
	act.sa_handler = sig_alrm;
	sigaction(SIGALRM, &act, 0);
	act.sa_handler = sig_chld;
	act.sa_flags |= SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, 0);
#else
	signal(SIGINT, SIG_DFL);
	signal(SIGFPE, sig_fpe);
#endif
}

int
get_open_max (void)
{
	int om;

#if defined(_SC_OPEN_MAX)
	om = sysconf(_SC_OPEN_MAX);
#elif defined(RLIMIT_NOFILE)
	{
		struct rlimit rlimit;

		if (getrlimit(RLIMIT_NOFILE, &rlimit)) {
			hxd_log("main: getrlimit: %s", strerror(errno));
			exit(1);
		}
		om = rlimit.rlim_max;
	}
#elif defined(HAVE_GETDTABLESIZE)
	om = getdtablesize();
#elif defined(OPEN_MAX)
	om = OPEN_MAX;
#else
	om = sizeof(fd_set)*8;
#endif

	if (om > (int)(FD_SETSIZE*sizeof(int)*8))
		om = (int)(FD_SETSIZE*sizeof(int)*8);

#if defined(__WIN32__)
	om = 4096;
#endif

	return om;
}

int
main (int argc __attribute__((__unused__)), char **argv __attribute__((__unused__)), char **envp)
{
#if defined(__WIN32__)
	/* init winsock */
	WSADATA wsadata;

	WSAStartup(1, &wsadata);
#endif
	hxd_pid = getpid();
#ifdef CONFIG_EUID
	seteuid(getuid());
#endif
#if XMALLOC_DEBUG
	DTBLINIT();
#endif

	hxd_open_max = get_open_max();
	hxd_files = xmalloc(hxd_open_max * sizeof(struct hxd_file));
	memset(hxd_files, 0, hxd_open_max * sizeof(struct hxd_file));
	FD_ZERO(&hxd_rfds);
	FD_ZERO(&hxd_wfds);

#if defined(CONFIG_EXEC) && defined(CONFIG_HOTLINE_SERVER)
	exec_init();
#endif

	hxd_environ = envp;

#if !defined(__WIN32__)
	signal_init();
#endif

#if defined(HAVE_LIBHPPA)
	allow_unaligned_data_access();
#endif

#if defined(CONFIG_HOTLINE_CLIENT)
#if defined(CONFIG_CIPHER)
	cipher_init();
#endif
	hotline_client_init(argc, argv);
#else
	close(0);
	close(1);
#endif
#if defined(CONFIG_HOTLINE_SERVER) || defined(CONFIG_TRACKER_SERVER)
	{
		int i, port = 0, detach = 0, pidset = 0;

		for (i = 1; i < argc; i++)
			if (argv[i][0] == '-') {
				if (argv[i][1] == 'f')
					hxdconf = argv[i+1];
				else if (argv[i][1] == 'd')
					detach = !detach;
				else if (argv[i][1] == 'p' && argv[i+1])
					port = atou16(argv[i+1]);
				if (argv[i][1] == 'o') {
					pidfile = argv[i+1];
					pidset = !pidset;
				}
			}
		if (!hxdconf)
			hxdconf = _PATH_HXD_CONF;
		hxd_config_init(&hxd_cfg);
		read_config_file(0);
		if (port)
			hxd_cfg.options.port = port;
		if (!hxd_cfg.options.detach)
			hxd_cfg.options.detach = detach;
		else
			hxd_cfg.options.detach = !detach;
#if !defined(__WIN32__)
		if (hxd_cfg.options.gid > 0) {
			if (setgid(hxd_cfg.options.gid)) {
				hxd_log("setgid(%d): %s", hxd_cfg.options.gid, strerror(errno));
				exit(1);
			}
		}
		if (hxd_cfg.options.uid > 0) {
			if (setuid(hxd_cfg.options.uid)) {
				hxd_log("setuid(%d)", hxd_cfg.options.uid, strerror(errno));
				exit(1);
			}
		}
		if (hxd_cfg.options.detach) {
			switch (fork()) {
				case 0:
#ifdef TIOCNOTTY
					if ((i = SYS_open("/dev/tty", O_RDWR, 0)) >= 0) {
						(void)ioctl(i, TIOCNOTTY, 0);
						close(i);
					}
#endif
					hxd_pid = getpid();
					setpgid(0, hxd_pid);
					break;
				case -1:
					hxd_log("could not detach; fork: %s", strerror(errno));
					exit(1);
				default:
					_exit(0);
			}
		}
#endif /* not WIN32 */
		if (pidset) {
        		 FILE *pidf = fopen(pidfile, "w+");
		         fprintf(pidf, "%d", getpid());
		         fclose(pidf);
      		}
		if (hxd_cfg.paths.log[0] == '-' && !hxd_cfg.paths.log[1]) {
			log_fd = 2;
		} else {
			log_fd = SYS_open(hxd_cfg.paths.log, O_WRONLY|O_CREAT|O_APPEND, hxd_cfg.permissions.log_files);
			if (log_fd < 0)
				log_fd = 2;
			else
				close(2);
		}
	}
#endif

#if defined(CONFIG_HOTLINE_SERVER)
#if defined(CONFIG_SQL)
	sql_init_user_tbl();
	sql_start(hxd_version);
#endif
	hxd_log("hxd version %s started, hxd_open_max = %d", hxd_version, hxd_open_max);
	hotline_server_init();
	gettimeofday(&server_start_time, 0);
#if defined(CONFIG_TRACKER_REGISTER)
	tracker_register_timer(0);
#endif
#if defined(CONFIG_HTXF_PTHREAD)
	timer_add_secs(5, thread_check_continuous, 0);
#endif
#endif

#if defined(CONFIG_TRACKER_SERVER)
	hxd_log("hxtrackd version %s started, hxd_open_max = %d", hxd_version, hxd_open_max);
	tracker_server_init();
#endif

#if defined(CONFIG_HOTLINE_CLIENT)
	hx_output.loop();
#endif

#if !defined(ONLY_GTK)
	loopZ();
#endif

	return 0;
}
