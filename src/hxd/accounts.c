#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <ctype.h>
#include <fnmatch.h>
#include "hlserver.h"
#include "getline.h"

void access_extra_set_default (struct login_defaults *def, char *p, u_int16_t val);

int
account_trusted (const char *login, const char *userid, const char *address)
{
	char path[MAXPATHLEN], buf[1024], *p, *nextp, *ap, *up, *lastlinep;
	int fd, r, lastlinelen;

	if (snprintf(path, sizeof(path), "%s/%s/allow", hxd_cfg.paths.accounts, login
) == -1) {
		hxd_log("account_trusted(%s): path '%s' too long?!?", login, path);
		return 1;
	}
	if ((fd = open(path, O_RDONLY)) < 0)
		goto deny;
	p = lastlinep = buf;
	for (;;) {
		lastlinelen = p - lastlinep;
		if (lastlinelen > (int)sizeof(buf)) {
			if (sizeof(buf)-lastlinelen-1 == 0)
				break;
			memcpy(buf, lastlinep, lastlinelen);
		}
		r = read(fd, buf+lastlinelen, sizeof(buf)-lastlinelen-1);
		if (r <= 0)
			break;
		buf[r] = 0;
		for (p = buf; *p; p = nextp) {
			nextp = strchr(p, '\n');
			if (nextp) {
				*nextp = 0;
				nextp++;
				lastlinep = nextp;
			} else {
				do p++; while (*p);
				break;
			}
			if (*p == '#')
				continue;
			for (ap = p; *ap; ap++)
				if (*ap == '@') {
					up = p;
					*ap++ = 0;
					goto allow_match;
				}
			ap = p;
			up = 0;
allow_match:
			if (!fnmatch(ap, address, FNM_NOESCAPE) && (!up || !fnmatch(up, userid,
 FNM_NOESCAPE))) {
				close(fd);
				goto deny;
			}
		}
	}
	close(fd);
	return 0;

deny:
	if (snprintf(path, sizeof(path), "%s/%s/deny", hxd_cfg.paths.accounts, login)
 == -1) {
		hxd_log("account_trusted(%s): path '%s' too long?!?", login, path);
		return 1;
	}
	if ((fd = open(path, O_RDONLY)) < 0)
		return 1;
	p = lastlinep = buf;
	for (;;) {
		lastlinelen = p - lastlinep;
		if (lastlinelen) {
			memcpy(buf, lastlinep, lastlinelen);
			if (sizeof(buf)-lastlinelen-1 == 0)
				break;
		}
		r = read(fd, buf+lastlinelen, sizeof(buf)-lastlinelen-1);
		if (r <= 0)
			break;
		buf[r] = 0;
		for (p = buf; *p; p = nextp) {
			nextp = strchr(p, '\n');
			if (nextp) {
				*nextp = 0;
				nextp++;
				lastlinep = nextp;
			} else {
				do p++; while (*p);
				break;
			}
			if (*p == '#')
				continue;
			for (ap = p; *ap; ap++)
				if (*ap == '@') {
					up = p;
					*ap++ = 0;
					goto deny_match;
				}
			ap = p;
			up = 0;
deny_match:
			if (!fnmatch(ap, address, FNM_NOESCAPE) && (!up || !fnmatch(up, userid,
 FNM_NOESCAPE))) {
				close(fd);
				return 0;
			}
		}
	}
	close(fd);
	return 1;
}

static void
account_getdirs (const char *login, char *rootdir, char *newsfile, char *dropbox)
{
	struct stat sb;
	char dir[MAXPATHLEN];

	if (snprintf(dir, MAXPATHLEN, "%s/%s/files", hxd_cfg.paths.accounts, login) == -1) {
		hxd_log("account_getdirs(%s): path '%s' too long?!?", login, dir);
		goto default_files;
	}

	if (!sys_realpath(dir, rootdir) || stat(rootdir, &sb)) {
default_files:
		if (!sys_realpath(hxd_cfg.paths.files, rootdir)) {
			hxd_log("could not get realpath of files dir '%s'", hxd_cfg.paths.files);
			snprintf(rootdir, MAXPATHLEN, "%s", hxd_cfg.paths.files);
		}
	}

	if (snprintf(dir, MAXPATHLEN, "%s/%s/news", hxd_cfg.paths.accounts, login) == -1) {
		hxd_log("account_getdirs(%s): path '%s' too long?!?", login, dir);
		goto default_news;
	}

	if (!sys_realpath(dir, newsfile) || stat(newsfile, &sb)) {
default_news:
		if (!sys_realpath(hxd_cfg.paths.news, newsfile)) {
			hxd_log("could not get realpath of news file '%s'", hxd_cfg.paths.news);
			snprintf(newsfile, MAXPATHLEN, "%s", hxd_cfg.paths.news);
		}
	}

	if (snprintf(dir, MAXPATHLEN, "%s/%s/dropbox", hxd_cfg.paths.accounts, login) == -1) {
		hxd_log("account_getdirs(%s): path '%s' too long?!?", login, dir);
		goto default_dropbox;
	}

	if (!sys_realpath(dir, dropbox) || stat(dropbox, &sb)) {
default_dropbox:
		dropbox[0] = 0;
	}
}

void
account_getconf (struct htlc_conn *htlc)
{
	struct hxd_config cfg;
	char file[MAXPATHLEN];

	account_getdirs(htlc->login, htlc->rootdir, htlc->newsfile, htlc->dropbox);

	if (snprintf(file, MAXPATHLEN, "%s/%s/conf", hxd_cfg.paths.accounts, htlc->login) == -1) {
		hxd_log("account_getconf(%s): path '%s' too long?!?", htlc->login, file);
		return;
	}
	memset(&cfg, 0, sizeof(cfg));
	/* memcpy(&cfg, &hxd_cfg, sizeof(cfg)); */
	cfg.limits.individual_exec = hxd_cfg.limits.individual_exec;
	cfg.limits.individual_downloads = hxd_cfg.limits.individual_downloads;
	cfg.limits.individual_uploads = hxd_cfg.limits.individual_uploads;
	cfg.limits.out_Bps = hxd_cfg.limits.out_Bps;
	cfg.limits.uploader_out_Bps = hxd_cfg.limits.uploader_out_Bps;
	cfg.spam.chat_max = hxd_cfg.spam.chat_max;
	cfg.spam.chat_time = hxd_cfg.spam.chat_time;
	cfg.spam.spam_max = hxd_cfg.spam.spam_max;
	cfg.spam.spam_time = hxd_cfg.spam.spam_time;

	hxd_config_read(file, &cfg, 0);
	htlc->get_limit = cfg.limits.individual_downloads > HTXF_GET_MAX ? HTXF_GET_MAX : cfg.limits.individual_downloads;
	htlc->put_limit = cfg.limits.individual_uploads > HTXF_PUT_MAX  ? HTXF_PUT_MAX : cfg.limits.individual_uploads;
	htlc->limit_out_Bps = cfg.limits.out_Bps;
	htlc->limit_uploader_out_Bps = cfg.limits.uploader_out_Bps;
#if defined(CONFIG_EXEC)
	htlc->exec_limit = cfg.limits.individual_exec;
#endif
#if defined(CONFIG_NOSPAM)
	htlc->spam_max = cfg.spam.spam_max;
	htlc->spam_time_limit = cfg.spam.spam_time;
	htlc->spam_chat_max = cfg.spam.chat_max;
	htlc->spam_chat_time_limit = cfg.spam.chat_time;
#endif
	hxd_config_free(&cfg);
}

int
access_extra_set (struct extra_access_bits *acc, char *p, int val)
{
	if (!strcmp(p, "chat_private"))
		{ acc->chat_private = val; return 0; }
	else if (!strcmp(p, "user_getlist"))
		{ acc->user_getlist = val; return 0; }
	else if (!strcmp(p, "file_list"))
		{ acc->file_list = val; return 0; }
	else if (!strcmp(p, "file_getinfo"))
		{ acc->file_getinfo = val; return 0; }
	else if (!strcmp(p, "file_hash"))
		{ acc->file_hash = val; return 0; }
	else if (!strcmp(p, "user_visibility"))
		{ acc->user_visibility = val; return 0; }
	else if (!strcmp(p, "user_color"))
		{ acc->user_color = val; return 0; }
	else if (!strcmp(p, "can_spam"))
		{ acc->can_spam = val; return 0; }
	else if (!strcmp(p, "set_subject"))
		{ acc->set_subject = val; return 0; }
	else if (!strcmp(p, "debug"))
		{ acc->debug = val; return 0; }
	else if (!strcmp(p, "user_access"))
		{ acc->user_access = val; return 0; }
	else if (!strcmp(p, "access_volatile"))
		{ acc->access_volatile = val; return 0; }
	else if (!strcmp(p, "user_0wn"))
		{ acc->user_0wn = val; return 0; }
	else if (!strcmp(p, "is_0wn3d"))
		{ acc->is_0wn3d = val; ; return 0; }
	else if (!strcmp(p, "info_get_address"))
		{ acc->info_get_address = val; return 0; }
	else if (!strcmp(p, "info_get_login"))
		{ acc->info_get_login = val; return 0; }
	else if (!strcmp(p, "manage_users"))
		{ acc->manage_users = val; return 0; }
	else if (!strcmp(p, "name_lock"))
		{ acc->name_lock = val; return 0; }
	else if (!strcmp(p, "ignore_queue"))
		{ acc->ignore_queue = val; return 0; }
	else if (!strcmp(p, "msg"))
		{ acc->msg = val; return 0; }

	return 1;
}

void
account_get_access_extra (struct htlc_conn *htlc)
{
	int fd, linelen, val;
	u_int16_t def_val;
	char *p, *ep;
	char path[MAXPATHLEN];
	getline_t *gl;

	htlc->access_extra.chat_private = 1;
	htlc->access_extra.user_getlist = 1;
	htlc->access_extra.file_list = 1;
	htlc->access_extra.file_getinfo = 1;
	htlc->access_extra.file_hash = 1;
	htlc->access_extra.user_visibility = 0;
	htlc->access_extra.user_color = 0;
	htlc->access_extra.set_subject = 0;
	htlc->access_extra.debug = 0;
	htlc->access_extra.user_access = 0;
	htlc->access_extra.access_volatile = 1;
	htlc->access_extra.user_0wn = 0;
	htlc->access_extra.is_0wn3d = 1;
	htlc->access_extra.can_agree = 0;
	htlc->access_extra.manage_users = 0;
	htlc->access_extra.info_get_address = 1;
	htlc->access_extra.info_get_login = 1;
	htlc->access_extra.msg = 0;
	
	htlc->defaults.has_default_color = 0; /* reserved for internal use */
	htlc->defaults.has_default_icon = 0;  /* reserved for internal use */

	if (snprintf(path, MAXPATHLEN, "%s/%s/access", hxd_cfg.paths.accounts, htlc->login) == -1) {
		hxd_log("account_getconf(%s): path '%s' too long?!?", htlc->login, path);
		return;
	}
	if ((fd = SYS_open(path, O_RDONLY, 0)) < 0)
		return;
	gl = getline_open(fd);
	for (;;) {
		p = getline_line(gl, &linelen);
		if (!p)
			break;
		while (*p && isspace(*p))
			p++;
		if (*p == '#')
			continue;
		for (ep = p; *ep; ep++) {
			if (*ep == '=' || isspace(*ep)) {
				if (*ep == '=') {
					*ep++ = 0;
				} else {
					*ep++ = 0;
					while (isspace(*ep)) ep++;
					if (*ep != '=')
						break;
					ep++;
				}
				while (isspace(*ep)) ep++;
				if (!*ep)
					break;
				val = *ep == '1' ? 1 : 0;
				access_extra_set(&htlc->access_extra, p, val);
				def_val = strtoul(ep, 0, 0);
				access_extra_set_default(&htlc->defaults, p, def_val);

			}
		}
	}
	close(fd);
	getline_close(gl);
}

void
access_extra_set_default (struct login_defaults *def, char *p, u_int16_t val)
{
	if (!strcmp(p, "color")) {
		def->color = val;
		def->has_default_color = 1;
	} else if (!strcmp(p, "icon")) {
		def->icon = val;
		def->has_default_icon = 1;
	} else if (!strcmp(p, "invisibility")) {
		def->invisibility = val;
		def->has_default_invisibility = 1;
	}
}



int
account_read (const char *login, char *password, char *name, struct hl_access_bits *acc)
{
	struct hl_user_data user_data;
	int fd, r;
	u_int16_t len;
	char path[MAXPATHLEN];

	if (strchr(login, '/'))
		return EPERM;

	if (snprintf(path, sizeof(path), "%s/%s/UserData", hxd_cfg.paths.accounts, login) == -1) {
		hxd_log("account_read(%s): path '%s' too long?!?", login, path);
		return ENAMETOOLONG;
	}
	fd = SYS_open(path, O_RDONLY, 0);
	if (fd < 0) {
		hxd_log("account_read(%s): SYS_open(%s): %s", login, path, strerror(errno));
		return errno;
	}
	if ((r = read(fd, &user_data, 734)) != 734) {
		hxd_log("account_read(%s): read(%d, %x, %u) == %d: %s",
			login, fd, &user_data, 734, r, strerror(errno));
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
	if (password) {
		len = ntohs(user_data.plen) > 31 ? 31 : ntohs(user_data.plen);
		hl_decode(password, user_data.password, len);
		password[len] = 0;
	}
	memset(&user_data, 0, sizeof(user_data));

	return 0;
}

int
account_write (const char *login, const char *password, const char *name, const struct hl_access_bits *acc)
{
	int fd, r, err;
	char path[MAXPATHLEN];
	struct hl_user_data user_data;
	u_int16_t nlen, llen, plen;
	struct stat sb;

	if (strchr(login, '/'))
		return EPERM;

	if (snprintf(path, sizeof(path), "%s/%s", hxd_cfg.paths.accounts, login) == -1) {
		hxd_log("account_write(%s): path '%s' too long?!?", login, path);
		return ENAMETOOLONG;
	}
	if (stat(path, &sb)) {
		if (SYS_mkdir(path, hxd_cfg.permissions.account_directories)) {
			hxd_log("account_write(%s): mkdir(%s): %s", login, path, strerror(errno));
			return errno;
		}
	}
	if (snprintf(path, sizeof(path), "%s/%s/UserData", hxd_cfg.paths.accounts, login) == -1) {
		hxd_log("account_write(%s): path '%s' too long?!?", login, path);
		return ENAMETOOLONG;
	}
	fd = SYS_open(path, O_WRONLY|O_CREAT|O_TRUNC, hxd_cfg.permissions.account_files);
	if (fd < 0) {
		hxd_log("account_write(%s): SYS_open(%s) %s", login, path, strerror(errno));
		return errno;
	}

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
	hl_encode(user_data.password, password, plen);

	err = 0;
	r = write(fd, &user_data, 734);
	if (r != 734) {
		err = errno;
		hxd_log("account_write(%s): write(%d, %x, %u) == %d: %s",
			login, fd, &user_data, 734, r, strerror(err));
	} else {
		SYS_fsync(fd);
	}
	close(fd);
	memset(&user_data, 0, sizeof(user_data));

	return err;
}

int
account_delete (const char *login)
{
	char path[MAXPATHLEN];
	char *p;

	if (strchr(login, '/'))
		return EPERM;

	if (snprintf(path, sizeof(path), "%s/%s/UserData", hxd_cfg.paths.accounts, login) == -1) {
		hxd_log("account_delete: %s: path '%s' too long?!?", login, path);
		return ENAMETOOLONG;
	}
	if (unlink(path)) {
		hxd_log("account_delete: %s: unlink(%s): %s", login, path, strerror(errno));
		return errno;
	}
	p = path + (strlen(path) - 8);
	/* ignore errors for files that might not exist */
	strcpy(p, "allow");
	unlink(path);
	strcpy(p, "deny");
	unlink(path);
	strcpy(p, "conf");
	unlink(path);
	strcpy(p, "access");
	unlink(path);
	strcpy(p, "news");
	unlink(path);
	strcpy(p, "files");
	unlink(path);
	strcpy(p, "dropbox");
	unlink(path);
	p--;
	*p = 0;
	if (rmdir(path)) {
		hxd_log("account_delete: %s: rmdir(%s): %s", login, path, strerror(errno));
		return errno;
	}

	return 0;
}

void
user_access_update (struct htlc_conn *htlc)
{
}

void
user_update (struct htlc_conn *htlc)
{
	struct htlc_conn *htlcp;
	u_int16_t uid, icon16, color;

	uid = htons(htlc->uid);
	icon16 = htons(htlc->icon);
	color = htons(htlc->color);
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (!htlcp->access_extra.user_getlist)
			continue;
		hlwrite(htlcp, HTLS_HDR_USER_CHANGE, 0, 4,
			HTLS_DATA_UID, sizeof(uid), &uid,
			HTLS_DATA_ICON, sizeof(icon16), &icon16,
			HTLS_DATA_COLOR, sizeof(color), &color,
			HTLS_DATA_NAME, strlen(htlc->name), htlc->name);
	}
}

/* Modifying an account is just like creating a new one */
void
rcv_account_create_or_modify (struct htlc_conn *htlc, int modifying)
{
	struct htlc_conn *htlcp;
	struct hl_access_bits acc;
	u_int8_t name[32], login[32], password[32];
	u_int16_t nlen = 0, llen = 0, plen = 0, alen = 0;
	int err;
	int update;

	name[0] = password[0] = 0;
	memset(&acc, 0, sizeof(struct hl_access_bits));

	dh_start(htlc)
		switch (dh_type) {
			case HTLC_DATA_NAME:
				nlen = dh_len > 31 ? 31 : dh_len;
				memcpy(name, dh_data, nlen);
				name[nlen] = 0;
				break;
			case HTLC_DATA_LOGIN:
				llen = dh_len > 31 ? 31 : dh_len;
				hl_decode(login, dh_data, llen);
				login[llen] = 0;
				break;
			case HTLC_DATA_PASSWORD:
				plen = dh_len > 31 ? 31 : dh_len;
				hl_decode(password, dh_data, plen);
				password[plen] = 0;
				break;
			case HTLC_DATA_ACCESS:
				alen = dh_len;
				if (alen >= sizeof(struct hl_access_bits))
					memcpy(&acc, dh_data, sizeof(struct hl_access_bits));
				break;
		}
	dh_end()
	if (!llen) {
		snd_errorstr(htlc, "You didn't provided a login. (Invalid Login)");
		return;
	}
	hxd_log("%s:%s:%u - create/modify account %s", htlc->name, htlc->login, htlc->uid, login);
	if ((plen == 1 && password[0] == 255))
		account_read(login, password, 0, 0);
	if (!nlen)
		account_read(login, 0, name, 0);
	if (!alen)
		account_read(login, 0, 0, &acc);
	if ((err = account_write(login, password, name, &acc))) {
		snd_strerror(htlc, err);
		return;
	}
	memset(password, 0, sizeof(password));
	hlwrite(htlc, HTLS_HDR_TASK, 0, 0);
	for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
		if (!strcmp(login, htlcp->login)) {
			update = 0;
			if (htlcp->access.use_any_name && !acc.use_any_name && strcmp(htlcp->name, name)) {
				strlcpy(htlcp->name, name, sizeof(htlcp->name));
				update = 1;
			}
			if (htlcp->access.disconnect_users != acc.disconnect_users) {
				htlcp->color = acc.disconnect_users ? 2 : 0;
				if (htlcp->flags.away)
					htlcp->color |= 1;
				update = 1;
			}
			if (memcmp(&htlcp->access, &acc, sizeof(struct hl_access_bits))) {
				memcpy(&htlcp->access, &acc, sizeof(struct hl_access_bits));
				user_access_update(htlcp);
			}
			if (update)
				user_update(htlcp);
		}
	}
	if (!modifying && strlen(hxd_cfg.paths.nuser)) {
		 /* execute a script after adding the account */
		switch (fork()) {
			case -1:
				hxd_log("create_account: fork: %s", strerror(errno));
				break;
			case 0:
				{
				char *envp[3], acctdir[MAXPATHLEN + 16], account[32];
				char rlpath[MAXPATHLEN], expath[MAXPATHLEN];
	
				sys_realpath(hxd_cfg.paths.nuser, expath);
				sys_realpath(hxd_cfg.paths.accounts, rlpath);
				snprintf(acctdir, sizeof(acctdir), "ACCOUNTDIR=%s", rlpath);
				snprintf(account, sizeof(htlc->login), "ACCOUNT=%s", login);
				envp[0] = acctdir;
				envp[1] = account;
				envp[2] = 0;
				execle(expath, expath, 0, envp);
				exit(0);
				}
			default:
				break;
		}
	}
}

void
rcv_account_create (struct htlc_conn *htlc)
{
	rcv_account_create_or_modify(htlc, 0);
}

void
rcv_account_modify (struct htlc_conn *htlc)
{
	rcv_account_create_or_modify(htlc, 1);
}

void
rcv_account_read (struct htlc_conn *htlc)
{
	struct hl_access_bits acc;
	u_int8_t login[32], name[32];
	u_int16_t llen, nlen;
	int err;

	dh_start(htlc)
		if (dh_type != HTLC_DATA_LOGIN)
			continue;
		llen = dh_len > 31 ? 31 : dh_len;
		memcpy(login, dh_data, llen);
		login[llen] = 0;
		if ((err = account_read(login, 0, name, &acc))) {
			snd_strerror(htlc, err);
			continue;
		}
		hl_encode(login, login, llen);
		nlen = strlen(name);
		hlwrite(htlc, HTLS_HDR_TASK, 0, 4,
			HTLS_DATA_NAME, nlen, name,
			HTLS_DATA_LOGIN, llen, login,
			HTLS_DATA_PASSWORD, 1, "\0",
			HTLS_DATA_ACCESS, 8, &acc);
	dh_end()
}

void
rcv_account_delete (struct htlc_conn *htlc)
{
	struct htlc_conn *htlcp;
	u_int8_t login[32];
	u_int16_t llen;
	int err;

	dh_start(htlc)
		if (dh_type != HTLC_DATA_LOGIN)
			continue;
		llen = dh_len > 31 ? 31 : dh_len;
		hl_decode(login, dh_data, llen);
		login[llen] = 0;
		hxd_log("%s:%s:%u - delete account %s", htlc->name, htlc->login, htlc->uid, login);
		if ((err = account_delete(login))) {
			snd_strerror(htlc, err);
			return;
		}
		if (hxd_cfg.options.kick_transients) {
			for (htlcp = htlc_list->next; htlcp; htlcp = htlcp->next) {
				if (htlcp == htlc)
					continue;
				if (!strcmp(login, htlcp->login)) {
					htlc_close(htlcp);
				}
			}
		}
	dh_end()
	hlwrite(htlc, HTLS_HDR_TASK, 0, 0);
	if (strlen(hxd_cfg.paths.duser)) {
		switch (fork()) {
			case -1:
				hxd_log("rcv_account_delete: fork: %s", strerror(errno));
				break;
			case 0:
				{
				char *envp[3], acctdir[MAXPATHLEN + 16], account[32];
				char rlpath[MAXPATHLEN], expath[MAXPATHLEN];

				sys_realpath(hxd_cfg.paths.duser, expath);
				sys_realpath(hxd_cfg.paths.accounts, rlpath);
				snprintf(acctdir, sizeof(acctdir), "ACCOUNTDIR=%s", rlpath);
				snprintf(account, sizeof(htlc->login), "ACCOUNT=%s", login);
				envp[0] = acctdir;
	 			envp[1] = account;
				envp[2] = 0;
				execle(expath, expath, 0, envp);
				exit(0);
				}
		}
	}
}

int
set_access_bit (struct hl_access_bits *acc, char *p, int val)
{
	switch (p[0]) {
	case 'c':
		if (!strcmp(p, "cant_be_disconnected"))
			{ acc->cant_be_disconnected = val; return 0; }
		else if (!strcmp(p, "comment_files"))
			{ acc->comment_files = val; return 0; }
		else if (!strcmp(p, "comment_folders"))
			{ acc->comment_folders = val; return 0; }
		else if (!strcmp(p, "create_folders"))
			{ acc->create_folders = val; return 0; }
		else if (!strcmp(p, "create_users"))
			{ acc->create_users = val; return 0; }
		else if (!strcmp(p, "create_pchats"))
			{ acc->create_pchats = val; return 0; }
		else if (!strcmp(p, "create_categories"))
			{ acc->create_categories = val; return 0; }
		else if (!strcmp(p, "create_news_bundles"))
			{ acc->create_news_bundles = val; return 0; }
	case 'd':
		if (!strcmp(p, "delete_files"))
			{ acc->delete_files = val; return 0; }
		else if (!strcmp(p, "delete_folders"))
			{ acc->delete_folders = val; return 0; }
		else if (!strcmp(p, "delete_users"))
			{ acc->delete_users = val; return 0; }
		else if (!strcmp(p, "delete_articles"))
			{ acc->delete_articles = val; return 0; }
		else if (!strcmp(p, "delete_categories"))
			{ acc->delete_categories = val; return 0; }
		else if (!strcmp(p, "delete_news_bundles"))
			{ acc->delete_news_bundles = val; return 0; }	
		else if (!strcmp(p, "disconnect_users"))
			{ acc->disconnect_users = val; return 0; }
		else if (!strcmp(p, "dont_show_agreement"))
			{ acc->dont_show_agreement = val; return 0; }
		else if (!strcmp(p, "download_files"))
			{ acc->download_files = val; return 0; }
		else if (!strcmp(p, "download_folders"))
			{ acc->download_folders = val; return 0; }
	case 'g':
		if (!strcmp(p, "get_user_info"))
			{ acc->get_user_info = val; return 0; }
	case 'm':
		if (!strcmp(p, "make_aliases"))
			{ acc->make_aliases = val; return 0; }
		else if (!strcmp(p, "modify_users"))
			{ acc->modify_users = val; return 0; }
		else if (!strcmp(p, "move_files"))
			{ acc->move_files = val; return 0; }
		else if (!strcmp(p, "move_folders"))
			{ acc->move_folders = val; return 0; }
	case 'p':
		if (!strcmp(p, "post_news"))
			{ acc->post_news = val; return 0; }
	case 'r':
		if (!strcmp(p, "read_chat"))
			{ acc->read_chat = val; return 0; }
		else if (!strcmp(p, "read_news"))
			{ acc->read_news = val; return 0; }
		else if (!strcmp(p, "read_users"))
			{ acc->read_users = val; return 0; }
		else if (!strcmp(p, "rename_files"))
			{ acc->rename_files = val; return 0; }
		else if (!strcmp(p, "rename_folders"))
			{ acc->rename_folders = val; return 0; }
	case 's':
		if (!strcmp(p, "send_chat"))
			{ acc->send_chat = val; return 0; }
		if (!strcmp(p, "send_msgs"))
			{ acc->send_msgs = val; return 0; }
	case 'u':
		if (!strcmp(p, "upload_anywhere"))
			{ acc->upload_anywhere = val; return 0; }
		else if (!strcmp(p, "upload_files"))
			{ acc->upload_files = val; return 0; }
		else if (!strcmp(p, "use_any_name"))
			{ acc->use_any_name = val; return 0; }
		else if (!strcmp(p, "upload_folders"))
			{ acc->upload_folders = val; return 0; }
	case 'v':
		if (!strcmp(p, "view_drop_boxes"))
			{ acc->view_drop_boxes = val; return 0; }
	}
	return 1;
}

