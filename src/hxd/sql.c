#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "hxd.h"

#if defined(CONFIG_SQL)
#include <mysql.h>

static MYSQL *Db = 0;

void
sql_query (const char *fmt, ...)
{
	va_list ap;
	char buf[16384];

	if (Db) {
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		if (mysql_query(Db, buf) != 0)
			hxd_log("mysql_query() failed: %s", mysql_error(Db));
	}
}

void
init_database (const char *host, const char *user, const char *pass, const char *data)
{
	MYSQL *d;

	Db = mysql_init (Db);
	if (!Db) {
		hxd_log("mysql_init() failed: %s", mysql_error(Db));
		return;
	}

	d = mysql_real_connect(Db, host, user, pass, data, 0, 0, 0); 
	if (!d) {
		hxd_log("mysql_real_connect() failed: %s", mysql_error(Db));
		Db = 0;
		return;
	}

	Db = d; 
}

/* individual functions for different sql activities */

void
sql_init_user_tbl (void)
{
	sql_query("DELETE FROM user");
}

void
sql_add_user (const char *userid, const char *nick, const char *ipaddr, int port, const char *login, int uid, int icon, int color)
{
	char as_login[64], as_nick[64], as_userid[1032];

	mysql_escape_string(as_login, login, strlen(login));
	mysql_escape_string(as_nick, nick, strlen(nick));
	mysql_escape_string(as_userid, userid, strlen(userid));

	sql_query("INSERT INTO user VALUES(NOW(),%d,'%s','%s','%s',%d,%d)",
		  uid,as_login,ipaddr,as_nick,icon,color);
	sql_query("INSERT INTO connections VALUES(NULL,NOW(),'%s','%s','%s',%d,'%s',%d)",
		  as_userid,as_nick,ipaddr,port,as_login,uid);
}

void
sql_modify_user (const char *nick, int icon, int color, int uid)
{
	char as_nick[64];

	mysql_escape_string(as_nick, nick, strlen(nick));

	sql_query("UPDATE user SET nickname='%s',icon=%d,color=%d WHERE socket=%d",
		  as_nick, icon, color, uid);
}

void
sql_delete_user (const char *userid, const char *nick, const char *ipaddr, int port, const char *login, int uid)
{
	char as_login[64], as_nick[64], as_userid[1032];

	mysql_escape_string(as_login, login, strlen(login));
	mysql_escape_string(as_nick, nick, strlen(nick));
	mysql_escape_string(as_userid, userid, strlen(userid));

	sql_query("DELETE FROM user WHERE socket=%d",uid);
	sql_query("INSERT INTO disconnections VALUES(NULL,NOW(),'%s','%s','%s',%d,'%s',%d)",
		  as_userid, as_nick, ipaddr, port, as_login, uid);
}

void
sql_exec (const char *nick, const char *login, const char *ipaddr, const char *cmdpath, const char *thisarg) {
	char as_login[64], as_nick[64], as_cmdpath[255], as_thisarg[30];
	mysql_escape_string(as_login, login, strlen(login));
	mysql_escape_string(as_nick, nick, strlen(nick));
	mysql_escape_string(as_cmdpath, cmdpath, strlen(cmdpath));
	mysql_escape_string(as_thisarg, thisarg, strlen(thisarg));

	sql_query("INSERT INTO exec VALUES(NULL,NOW(),'%s','%s','%s','%s', '%s')",
			as_nick, as_login, ipaddr, as_cmdpath, as_thisarg);
}

void
sql_download (const char *nick, const char *ipaddr, const char *login, const char *path)
{
	char as_login[64], as_nick[64], as_path[MAXPATHLEN*2];

	mysql_escape_string(as_login, login, strlen(login));
	mysql_escape_string(as_nick, nick, strlen(nick));
	mysql_escape_string(as_path, path, strlen(path));	

	sql_query("INSERT INTO download VALUES(NULL,NOW(),'%s','%s','%s','%s')",
		  as_nick, ipaddr, as_login, as_path);
}

void
sql_upload (const char *nick, const char *ipaddr, const char *login, const char *path)
{
	char as_login[64], as_nick[64], as_path[MAXPATHLEN*2];

	mysql_escape_string(as_login, login, strlen(login));
	mysql_escape_string(as_nick, nick, strlen(nick));
	mysql_escape_string(as_path, path, strlen(path));

	sql_query("INSERT INTO upload VALUES(NULL,NOW(),'%s','%s','%s','%s')",
		  as_nick, ipaddr, as_login, as_path);
}

void
sql_user_kick (const char *nick, const char *ipaddr, const char *login, const char *knick, const char *klogin)
{
	char as_login[64], as_nick[64], as_klogin[64], as_knick[64];

	mysql_escape_string(as_login, login, strlen(login));
	mysql_escape_string(as_nick, nick, strlen(nick));
	mysql_escape_string(as_klogin, klogin, strlen(klogin));
	mysql_escape_string(as_knick, knick, strlen(knick));

	sql_query("INSERT INTO kick VALUES(NULL,NOW(),'%s','%s','%s','%s','%s')",
		  as_nick, ipaddr, as_login, as_knick, as_klogin);
}

void
sql_user_ban (const char *nick, const char *ipaddr, const char *login, const char *knick, const char *klogin)
{
	char as_login[64], as_nick[64], as_klogin[64], as_knick[64];

	mysql_escape_string(as_login, login, strlen(login));
	mysql_escape_string(as_nick, nick, strlen(nick));
	mysql_escape_string(as_klogin, klogin, strlen(klogin));
	mysql_escape_string(as_knick, knick, strlen(knick));

	sql_query("INSERT INTO ban VALUES(NULL,NOW(),'%s','%s','%s','%s','%s')",
		  as_nick, ipaddr, as_login, as_knick, as_klogin);
}

void
sql_start (const char *version)
{
	char as_version[20];

	mysql_escape_string(as_version,version,strlen(version));

	sql_query("INSERT INTO start VALUES(NULL,NOW(),'%s')", as_version);
}
#endif
