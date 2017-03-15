struct hxd_config hxd_config_default;
#define CONFSOFF(_x) (((char *)&hxd_config_default._x)-((char *)&hxd_config_default))
struct wanted hxd_wanted_t0s0;
struct wanted hxd_wanted_t1s0;
struct wanted hxd_wanted_t1s1;
struct wanted hxd_wanted_t1s2;
struct wanted hxd_wanted_t1s3;
struct wanted hxd_wanted_t1s4;
struct wanted hxd_wanted_t1s5;
struct wanted hxd_wanted_t1s6;
struct wanted hxd_wanted_t1s7;
struct wanted hxd_wanted_t1s8;
struct wanted hxd_wanted_t1s9;
struct wanted hxd_wanted_t1s10;
struct wanted hxd_wanted_t1s11;
struct wanted hxd_wanted_t1s12;
struct wanted hxd_wanted_t1s13;
struct wanted hxd_wanted_t1s14;
struct wanted hxd_wanted_t1s15;
struct wanted hxd_wanted_t2s0;
struct wanted hxd_wanted_t2s1;
struct wanted hxd_wanted_t2s2;
struct wanted hxd_wanted_t2s3;
struct wanted hxd_wanted_t3s0;
struct wanted hxd_wanted_t3s1;
struct wanted hxd_wanted_t3s2;
struct wanted hxd_wanted_t4s0;
struct wanted hxd_wanted_t4s1;
struct wanted hxd_wanted_t4s2;
struct wanted hxd_wanted_t4s3;
struct wanted hxd_wanted_t4s4;
struct wanted hxd_wanted_t4s5;
struct wanted hxd_wanted_t4s6;
struct wanted hxd_wanted_t5s0;
struct wanted hxd_wanted_t5s1;
struct wanted hxd_wanted_t5s2;
struct wanted hxd_wanted_t5s3;
struct wanted hxd_wanted_t6s0;
struct wanted hxd_wanted_t6s1;
struct wanted hxd_wanted_t6s2;
struct wanted hxd_wanted_t6s3;
struct wanted hxd_wanted_t6s4;
struct wanted hxd_wanted_t6s5;
struct wanted hxd_wanted_t6s6;
struct wanted hxd_wanted_t6s7;
struct wanted hxd_wanted_t6s8;
struct wanted hxd_wanted_t6s9;
struct wanted hxd_wanted_t7s0;
struct wanted hxd_wanted_t7s1;
struct wanted hxd_wanted_t7s2;
struct wanted hxd_wanted_t7s3;
struct wanted hxd_wanted_t7s4;
struct wanted hxd_wanted_t7s5;
struct wanted hxd_wanted_t7s6;
struct wanted hxd_wanted_t8s0;
struct wanted hxd_wanted_t8s1;
struct wanted hxd_wanted_t8s2;
struct wanted hxd_wanted_t8s3;
struct wanted hxd_wanted_t8s4;
struct wanted hxd_wanted_t8s5;
struct wanted hxd_wanted_t8s6;
struct wanted hxd_wanted_t8s7;
struct wanted hxd_wanted_t8s8;
struct wanted hxd_wanted_t8s9;
struct wanted hxd_wanted_t8s10;
struct wanted hxd_wanted_t8s11;
struct wanted hxd_wanted_t8s12;
struct wanted hxd_wanted_t8s13;
struct wanted hxd_wanted_t8s14;
struct wanted hxd_wanted_t8s15;
struct wanted hxd_wanted_t9s0;
struct wanted hxd_wanted_t9s1;
struct wanted hxd_wanted_t9s2;
struct wanted hxd_wanted_t9s3;
struct wanted hxd_wanted_t9s4;
struct wanted hxd_wanted_t9s5;
struct wanted hxd_wanted_t9s6;
struct wanted hxd_wanted_t9s7;
struct wanted hxd_wanted_t10s0;
struct wanted hxd_wanted_t10s1;
struct wanted hxd_wanted_t10s2;
struct wanted hxd_wanted_t11s0;
struct wanted hxd_wanted_t11s1;
struct wanted hxd_wanted_t11s2;
struct wanted hxd_wanted_t11s3;
struct wanted hxd_wanted_t11s4;
struct wanted hxd_wanted_t11s5;
struct wanted hxd_wanted_t11s6;
struct wanted hxd_wanted_t11s7;
struct wanted hxd_wanted_t12s0;
struct wanted hxd_wanted_t12s1;
struct wanted hxd_wanted_t12s2;
struct wanted hxd_wanted_t12s3;
struct wanted hxd_wanted_t12s4;
struct wanted hxd_wanted_t13s0;
struct wanted hxd_wanted_t13s1;
struct wanted hxd_wanted_t13s2;
struct wanted hxd_wanted_t13s3;
struct wanted hxd_wanted_t13s4;
struct wanted hxd_wanted_t13s5;
struct wanted hxd_wanted_t13s6;
struct wanted hxd_wanted_t13s7;
struct wanted hxd_wanted_t13s8;
struct wanted hxd_wanted_t14s0;
struct wanted hxd_wanted_t14s1;
struct wanted hxd_wanted_t14s2;
struct wanted hxd_wanted_t14s3;
struct wanted hxd_wanted_t15s0;
struct wanted hxd_wanted_t15s1;
struct wanted hxd_wanted_t0s0 = {
&hxd_wanted_t1s0, 0, 0, 0,
0, 0, 0,
0, 0, {0}, {0} };
struct wanted hxd_wanted_t1s0 = {
&hxd_wanted_t2s0, &hxd_wanted_t0s0, &hxd_wanted_t1s1, &hxd_wanted_t1s15,
"options", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t1s1 = {
&hxd_wanted_t1s2, &hxd_wanted_t1s0, 0, 0,
"port", 0, TYPE_INT, CONFSOFF(options.port),
0, {(void *)5500}, {0} };
char *hxd_wanted_t1s2_sa[] = {
	"0.0.0.0",
	0
};
struct wanted hxd_wanted_t1s2 = {
&hxd_wanted_t1s3, &hxd_wanted_t1s1, 0, 0,
"addresses", 0, TYPE_STR_ARRAY, CONFSOFF(options.addresses),
0, {hxd_wanted_t1s2_sa}, {0} };
struct wanted hxd_wanted_t1s3 = {
&hxd_wanted_t1s4, &hxd_wanted_t1s2, 0, 0,
"unix_address", 0, TYPE_STR, CONFSOFF(options.unix_address),
0, {"/tmp/.hxd.sock"}, {0} };
struct wanted hxd_wanted_t1s4 = {
&hxd_wanted_t1s5, &hxd_wanted_t1s3, 0, 0,
"version", 0, TYPE_INT, CONFSOFF(options.version),
0, {(void *)0}, {0} };
struct wanted hxd_wanted_t1s5 = {
&hxd_wanted_t1s6, &hxd_wanted_t1s4, 0, 0,
"away_time", 0, TYPE_INT, CONFSOFF(options.away_time),
0, {(void *)600}, {0} };
struct wanted hxd_wanted_t1s6 = {
&hxd_wanted_t1s7, &hxd_wanted_t1s5, 0, 0,
"ban_time", 0, TYPE_INT, CONFSOFF(options.ban_time),
0, {(void *)1800}, {0} };
struct wanted hxd_wanted_t1s7 = {
&hxd_wanted_t1s8, &hxd_wanted_t1s6, 0, 0,
"ignore_commands", 0, TYPE_BOOL, CONFSOFF(options.ignore_commands),
0, {(void *)0}, {0} };
struct wanted hxd_wanted_t1s8 = {
&hxd_wanted_t1s9, &hxd_wanted_t1s7, 0, 0,
"self_info", 0, TYPE_BOOL, CONFSOFF(options.self_info),
0, {(void *)1}, {0} };
struct wanted hxd_wanted_t1s9 = {
&hxd_wanted_t1s10, &hxd_wanted_t1s8, 0, 0,
"kick_transients", 0, TYPE_BOOL, CONFSOFF(options.kick_transients),
0, {(void *)1}, {0} };
struct wanted hxd_wanted_t1s10 = {
&hxd_wanted_t1s11, &hxd_wanted_t1s9, 0, 0,
"gid", 0, TYPE_INT, CONFSOFF(options.gid),
0, {(void *)-1}, {0} };
struct wanted hxd_wanted_t1s11 = {
&hxd_wanted_t1s12, &hxd_wanted_t1s10, 0, 0,
"uid", 0, TYPE_INT, CONFSOFF(options.uid),
0, {(void *)-1}, {0} };
struct wanted hxd_wanted_t1s12 = {
&hxd_wanted_t1s13, &hxd_wanted_t1s11, 0, 0,
"detach", 0, TYPE_BOOL, CONFSOFF(options.detach),
0, {(void *)0}, {0} };
struct wanted hxd_wanted_t1s13 = {
&hxd_wanted_t1s14, &hxd_wanted_t1s12, 0, 0,
"ident", 0, TYPE_BOOL, CONFSOFF(options.ident),
0, {(void *)1}, {0} };
struct wanted hxd_wanted_t1s14 = {
&hxd_wanted_t1s15, &hxd_wanted_t1s13, 0, 0,
"hldump", 0, TYPE_BOOL, CONFSOFF(options.hldump),
0, {(void *)0}, {0} };
char *hxd_wanted_t1s15_sa[] = {
	"mail",
	0
};
struct wanted hxd_wanted_t1s15 = {
0, &hxd_wanted_t1s14, 0, 0,
"exclude", 0, TYPE_STR_ARRAY, CONFSOFF(options.exclude),
0, {hxd_wanted_t1s15_sa}, {0} };
struct wanted hxd_wanted_t2s0 = {
&hxd_wanted_t3s0, &hxd_wanted_t1s0, &hxd_wanted_t2s1, &hxd_wanted_t2s3,
"text", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t2s1 = {
&hxd_wanted_t2s2, &hxd_wanted_t2s0, 0, 0,
"server_encoding", 0, TYPE_STR, CONFSOFF(text.server_encoding),
0, {""}, {0} };
struct wanted hxd_wanted_t2s2 = {
&hxd_wanted_t2s3, &hxd_wanted_t2s1, 0, 0,
"client_encoding", 0, TYPE_STR, CONFSOFF(text.client_encoding),
0, {""}, {0} };
struct wanted hxd_wanted_t2s3 = {
0, &hxd_wanted_t2s2, 0, 0,
"convert_mac", 0, TYPE_BOOL, CONFSOFF(text.convert_mac),
0, {(void *)0}, {0} };
struct wanted hxd_wanted_t3s0 = {
&hxd_wanted_t4s0, &hxd_wanted_t2s0, &hxd_wanted_t3s1, &hxd_wanted_t3s2,
"modules", 0, 0, 0, 0, {0}, {0} };
char *hxd_wanted_t3s1_sa[] = {
	"hotline",
	"irc",
	0
};
struct wanted hxd_wanted_t3s1 = {
&hxd_wanted_t3s2, &hxd_wanted_t3s0, 0, 0,
"load", 0, TYPE_STR_ARRAY, CONFSOFF(modules.load),
0, {hxd_wanted_t3s1_sa}, {0} };
char *hxd_wanted_t3s2_sa[] = {
	"",
	0
};
struct wanted hxd_wanted_t3s2 = {
0, &hxd_wanted_t3s1, 0, 0,
"reload", 0, TYPE_STR_ARRAY, CONFSOFF(modules.reload),
0, {hxd_wanted_t3s2_sa}, {0} };
struct wanted hxd_wanted_t4s0 = {
&hxd_wanted_t5s0, &hxd_wanted_t3s0, &hxd_wanted_t4s1, &hxd_wanted_t4s6,
"operation", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t4s1 = {
&hxd_wanted_t4s2, &hxd_wanted_t4s0, 0, 0,
"hfs", 0, TYPE_BOOL, CONFSOFF(operation.hfs),
0, {(void *)1}, {0} };
struct wanted hxd_wanted_t4s2 = {
&hxd_wanted_t4s3, &hxd_wanted_t4s1, 0, 0,
"nospam", 0, TYPE_BOOL, CONFSOFF(operation.nospam),
0, {(void *)1}, {0} };
struct wanted hxd_wanted_t4s3 = {
&hxd_wanted_t4s4, &hxd_wanted_t4s2, 0, 0,
"commands", 0, TYPE_BOOL, CONFSOFF(operation.commands),
0, {(void *)1}, {0} };
struct wanted hxd_wanted_t4s4 = {
&hxd_wanted_t4s5, &hxd_wanted_t4s3, 0, 0,
"exec", 0, TYPE_BOOL, CONFSOFF(operation.exec),
0, {(void *)1}, {0} };
struct wanted hxd_wanted_t4s5 = {
&hxd_wanted_t4s6, &hxd_wanted_t4s4, 0, 0,
"tracker_register", 0, TYPE_BOOL, CONFSOFF(operation.tracker_register),
0, {(void *)1}, {0} };
struct wanted hxd_wanted_t4s6 = {
0, &hxd_wanted_t4s5, 0, 0,
"tnews", 0, TYPE_BOOL, CONFSOFF(operation.tnews),
0, {(void *)1}, {0} };
struct wanted hxd_wanted_t5s0 = {
&hxd_wanted_t6s0, &hxd_wanted_t4s0, &hxd_wanted_t5s1, &hxd_wanted_t5s3,
"banner", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t5s1 = {
&hxd_wanted_t5s2, &hxd_wanted_t5s0, 0, 0,
"type", 0, TYPE_STR, CONFSOFF(banner.type),
0, {""}, {0} };
struct wanted hxd_wanted_t5s2 = {
&hxd_wanted_t5s3, &hxd_wanted_t5s1, 0, 0,
"file", 0, TYPE_STR, CONFSOFF(banner.file),
0, {""}, {0} };
struct wanted hxd_wanted_t5s3 = {
0, &hxd_wanted_t5s2, 0, 0,
"url", 0, TYPE_STR, CONFSOFF(banner.url),
0, {""}, {0} };
struct wanted hxd_wanted_t6s0 = {
&hxd_wanted_t7s0, &hxd_wanted_t5s0, &hxd_wanted_t6s1, &hxd_wanted_t6s9,
"limits", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t6s1 = {
&hxd_wanted_t6s2, &hxd_wanted_t6s0, 0, 0,
"total_downloads", 0, TYPE_INT, CONFSOFF(limits.total_downloads),
0, {(void *)20}, {0} };
struct wanted hxd_wanted_t6s2 = {
&hxd_wanted_t6s3, &hxd_wanted_t6s1, 0, 0,
"total_uploads", 0, TYPE_INT, CONFSOFF(limits.total_uploads),
0, {(void *)20}, {0} };
struct wanted hxd_wanted_t6s3 = {
&hxd_wanted_t6s4, &hxd_wanted_t6s2, 0, 0,
"queue_size", 0, TYPE_INT, CONFSOFF(limits.queue_size),
0, {(void *)40}, {0} };
struct wanted hxd_wanted_t6s4 = {
&hxd_wanted_t6s5, &hxd_wanted_t6s3, 0, 0,
"individual_downloads", 0, TYPE_INT, CONFSOFF(limits.individual_downloads),
0, {(void *)1}, {0} };
struct wanted hxd_wanted_t6s5 = {
&hxd_wanted_t6s6, &hxd_wanted_t6s4, 0, 0,
"individual_uploads", 0, TYPE_INT, CONFSOFF(limits.individual_uploads),
0, {(void *)1}, {0} };
struct wanted hxd_wanted_t6s6 = {
&hxd_wanted_t6s7, &hxd_wanted_t6s5, 0, 0,
"out_Bps", 0, TYPE_INT, CONFSOFF(limits.out_Bps),
0, {(void *)0}, {0} };
struct wanted hxd_wanted_t6s7 = {
&hxd_wanted_t6s8, &hxd_wanted_t6s6, 0, 0,
"uploader_out_Bps", 0, TYPE_INT, CONFSOFF(limits.uploader_out_Bps),
0, {(void *)0}, {0} };
struct wanted hxd_wanted_t6s8 = {
&hxd_wanted_t6s9, &hxd_wanted_t6s7, 0, 0,
"total_exec", 0, TYPE_INT, CONFSOFF(limits.total_exec),
0, {(void *)3}, {0} };
struct wanted hxd_wanted_t6s9 = {
0, &hxd_wanted_t6s8, 0, 0,
"individual_exec", 0, TYPE_INT, CONFSOFF(limits.individual_exec),
0, {(void *)1}, {0} };
struct wanted hxd_wanted_t7s0 = {
&hxd_wanted_t8s0, &hxd_wanted_t6s0, &hxd_wanted_t7s1, &hxd_wanted_t7s6,
"tracker", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t7s1 = {
&hxd_wanted_t7s2, &hxd_wanted_t7s0, 0, 0,
"name", 0, TYPE_STR, CONFSOFF(tracker.name),
0, {"name"}, {0} };
struct wanted hxd_wanted_t7s2 = {
&hxd_wanted_t7s3, &hxd_wanted_t7s1, 0, 0,
"description", 0, TYPE_STR, CONFSOFF(tracker.description),
0, {"description"}, {0} };
char *hxd_wanted_t7s3_sa[] = {
	"127.0.0.1",
	0
};
struct wanted hxd_wanted_t7s3 = {
&hxd_wanted_t7s4, &hxd_wanted_t7s2, 0, 0,
"trackers", 0, TYPE_STR_ARRAY, CONFSOFF(tracker.trackers),
0, {hxd_wanted_t7s3_sa}, {0} };
struct wanted hxd_wanted_t7s4 = {
&hxd_wanted_t7s5, &hxd_wanted_t7s3, 0, 0,
"interval", 0, TYPE_INT, CONFSOFF(tracker.interval),
0, {(void *)240}, {0} };
struct wanted hxd_wanted_t7s5 = {
&hxd_wanted_t7s6, &hxd_wanted_t7s4, 0, 0,
"nusers", 0, TYPE_INT, CONFSOFF(tracker.nusers),
0, {(void *)-1}, {0} };
struct wanted hxd_wanted_t7s6 = {
0, &hxd_wanted_t7s5, 0, 0,
"password", 0, TYPE_STR, CONFSOFF(tracker.password),
0, {""}, {0} };
struct wanted hxd_wanted_t8s0 = {
&hxd_wanted_t9s0, &hxd_wanted_t7s0, &hxd_wanted_t8s1, &hxd_wanted_t8s15,
"paths", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t8s1 = {
&hxd_wanted_t8s2, &hxd_wanted_t8s0, 0, 0,
"files", 0, TYPE_STR, CONFSOFF(paths.files),
0, {"./files"}, {0} };
struct wanted hxd_wanted_t8s2 = {
&hxd_wanted_t8s3, &hxd_wanted_t8s1, 0, 0,
"accounts", 0, TYPE_STR, CONFSOFF(paths.accounts),
0, {"./accounts"}, {0} };
struct wanted hxd_wanted_t8s3 = {
&hxd_wanted_t8s4, &hxd_wanted_t8s2, 0, 0,
"exec", 0, TYPE_STR, CONFSOFF(paths.exec),
0, {"./exec"}, {0} };
struct wanted hxd_wanted_t8s4 = {
&hxd_wanted_t8s5, &hxd_wanted_t8s3, 0, 0,
"news", 0, TYPE_STR, CONFSOFF(paths.news),
0, {"./news"}, {0} };
struct wanted hxd_wanted_t8s5 = {
&hxd_wanted_t8s6, &hxd_wanted_t8s4, 0, 0,
"newsdir", 0, TYPE_STR, CONFSOFF(paths.newsdir),
0, {"./newsdir"}, {0} };
struct wanted hxd_wanted_t8s6 = {
&hxd_wanted_t8s7, &hxd_wanted_t8s5, 0, 0,
"agreement", 0, TYPE_STR, CONFSOFF(paths.agreement),
0, {"./agreement"}, {0} };
struct wanted hxd_wanted_t8s7 = {
&hxd_wanted_t8s8, &hxd_wanted_t8s6, 0, 0,
"log", 0, TYPE_STR, CONFSOFF(paths.log),
0, {"./log"}, {0} };
struct wanted hxd_wanted_t8s8 = {
&hxd_wanted_t8s9, &hxd_wanted_t8s7, 0, 0,
"banlist", 0, TYPE_STR, CONFSOFF(paths.banlist),
0, {"./banlist"}, {0} };
struct wanted hxd_wanted_t8s9 = {
&hxd_wanted_t8s10, &hxd_wanted_t8s8, 0, 0,
"tracker_banlist", 0, TYPE_STR, CONFSOFF(paths.tracker_banlist),
0, {"./tracker_banlist"}, {0} };
struct wanted hxd_wanted_t8s10 = {
&hxd_wanted_t8s11, &hxd_wanted_t8s9, 0, 0,
"hldump", 0, TYPE_STR, CONFSOFF(paths.hldump),
0, {"./hldump"}, {0} };
struct wanted hxd_wanted_t8s11 = {
&hxd_wanted_t8s12, &hxd_wanted_t8s10, 0, 0,
"avlist", 0, TYPE_STR, CONFSOFF(paths.avlist),
0, {"./etc/AppleVolumes.system"}, {0} };
struct wanted hxd_wanted_t8s12 = {
&hxd_wanted_t8s13, &hxd_wanted_t8s11, 0, 0,
"luser", 0, TYPE_STR, CONFSOFF(paths.luser),
0, {"login"}, {0} };
struct wanted hxd_wanted_t8s13 = {
&hxd_wanted_t8s14, &hxd_wanted_t8s12, 0, 0,
"nuser", 0, TYPE_STR, CONFSOFF(paths.nuser),
0, {"./etc/newuser"}, {0} };
struct wanted hxd_wanted_t8s14 = {
&hxd_wanted_t8s15, &hxd_wanted_t8s13, 0, 0,
"duser", 0, TYPE_STR, CONFSOFF(paths.duser),
0, {"./etc/rmuser"}, {0} };
struct wanted hxd_wanted_t8s15 = {
0, &hxd_wanted_t8s14, 0, 0,
"modules", 0, TYPE_STR, CONFSOFF(paths.modules),
0, {"./lib"}, {0} };
struct wanted hxd_wanted_t9s0 = {
&hxd_wanted_t10s0, &hxd_wanted_t8s0, &hxd_wanted_t9s1, &hxd_wanted_t9s7,
"permissions", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t9s1 = {
&hxd_wanted_t9s2, &hxd_wanted_t9s0, 0, 0,
"umask", 0, TYPE_INT_OCTAL, CONFSOFF(permissions.umask),
0, {(void *)0}, {0} };
struct wanted hxd_wanted_t9s2 = {
&hxd_wanted_t9s3, &hxd_wanted_t9s1, 0, 0,
"files", 0, TYPE_INT_OCTAL, CONFSOFF(permissions.files),
0, {(void *)416}, {0} };
struct wanted hxd_wanted_t9s3 = {
&hxd_wanted_t9s4, &hxd_wanted_t9s2, 0, 0,
"directories", 0, TYPE_INT_OCTAL, CONFSOFF(permissions.directories),
0, {(void *)488}, {0} };
struct wanted hxd_wanted_t9s4 = {
&hxd_wanted_t9s5, &hxd_wanted_t9s3, 0, 0,
"account_files", 0, TYPE_INT_OCTAL, CONFSOFF(permissions.account_files),
0, {(void *)384}, {0} };
struct wanted hxd_wanted_t9s5 = {
&hxd_wanted_t9s6, &hxd_wanted_t9s4, 0, 0,
"account_directories", 0, TYPE_INT_OCTAL, CONFSOFF(permissions.account_directories),
0, {(void *)448}, {0} };
struct wanted hxd_wanted_t9s6 = {
&hxd_wanted_t9s7, &hxd_wanted_t9s5, 0, 0,
"log_files", 0, TYPE_INT_OCTAL, CONFSOFF(permissions.log_files),
0, {(void *)384}, {0} };
struct wanted hxd_wanted_t9s7 = {
0, &hxd_wanted_t9s6, 0, 0,
"news_files", 0, TYPE_INT_OCTAL, CONFSOFF(permissions.news_files),
0, {(void *)416}, {0} };
struct wanted hxd_wanted_t10s0 = {
&hxd_wanted_t11s0, &hxd_wanted_t9s0, &hxd_wanted_t10s1, &hxd_wanted_t10s2,
"files", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t10s1 = {
&hxd_wanted_t10s2, &hxd_wanted_t10s0, 0, 0,
"comment", 0, TYPE_STR, CONFSOFF(files.comment),
0, {"ftp.microsoft.com"}, {0} };
struct enumarray hxd_enum_t10s2[] = {
	{"cap", "HFS_FORK_CAP"},
	{"double", "HFS_FORK_DOUBLE"},
	{"netatalk", "HFS_FORK_NETATALK"},
	{0, 0}
};
struct wanted hxd_wanted_t10s2 = {
0, &hxd_wanted_t10s1, 0, 0,
"fork", 0, TYPE_ENUM, CONFSOFF(files.fork),
0, {(void *)1}, {hxd_enum_t10s2} };
struct wanted hxd_wanted_t11s0 = {
&hxd_wanted_t12s0, &hxd_wanted_t10s0, &hxd_wanted_t11s1, &hxd_wanted_t11s7,
"strings", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t11s1 = {
&hxd_wanted_t11s2, &hxd_wanted_t11s0, 0, 0,
"news_divider", 0, TYPE_STR, CONFSOFF(strings.news_divider),
0, {"_________________________________________________________"}, {0} };
struct wanted hxd_wanted_t11s2 = {
&hxd_wanted_t11s3, &hxd_wanted_t11s1, 0, 0,
"news_format", 0, TYPE_STR, CONFSOFF(strings.news_format),
0, {"From %s - %s"}, {0} };
struct wanted hxd_wanted_t11s3 = {
&hxd_wanted_t11s4, &hxd_wanted_t11s2, 0, 0,
"news_time_format", 0, TYPE_STR, CONFSOFF(strings.news_time_format),
0, {"\n[%c]"}, {0} };
struct wanted hxd_wanted_t11s4 = {
&hxd_wanted_t11s5, &hxd_wanted_t11s3, 0, 0,
"chat_format", 0, TYPE_STR, CONFSOFF(strings.chat_format),
0, {"\r%13.13s:  %s"}, {0} };
struct wanted hxd_wanted_t11s5 = {
&hxd_wanted_t11s6, &hxd_wanted_t11s4, 0, 0,
"chat_opt_format", 0, TYPE_STR, CONFSOFF(strings.chat_opt_format),
0, {"\r *** %s %s"}, {0} };
struct wanted hxd_wanted_t11s6 = {
&hxd_wanted_t11s7, &hxd_wanted_t11s5, 0, 0,
"download_info", 0, TYPE_STR, CONFSOFF(strings.download_info),
0, {"--------------------------------\rDownload:"}, {0} };
struct wanted hxd_wanted_t11s7 = {
0, &hxd_wanted_t11s6, 0, 0,
"upload_info", 0, TYPE_STR, CONFSOFF(strings.upload_info),
0, {" Uploads:"}, {0} };
struct wanted hxd_wanted_t12s0 = {
&hxd_wanted_t13s0, &hxd_wanted_t11s0, &hxd_wanted_t12s1, &hxd_wanted_t12s4,
"sql", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t12s1 = {
&hxd_wanted_t12s2, &hxd_wanted_t12s0, 0, 0,
"host", 0, TYPE_STR, CONFSOFF(sql.host),
0, {"localhost"}, {0} };
struct wanted hxd_wanted_t12s2 = {
&hxd_wanted_t12s3, &hxd_wanted_t12s1, 0, 0,
"user", 0, TYPE_STR, CONFSOFF(sql.user),
0, {"hxd"}, {0} };
struct wanted hxd_wanted_t12s3 = {
&hxd_wanted_t12s4, &hxd_wanted_t12s2, 0, 0,
"pass", 0, TYPE_STR, CONFSOFF(sql.pass),
0, {"hxd"}, {0} };
struct wanted hxd_wanted_t12s4 = {
0, &hxd_wanted_t12s3, 0, 0,
"data", 0, TYPE_STR, CONFSOFF(sql.data),
0, {"hxd"}, {0} };
struct wanted hxd_wanted_t13s0 = {
&hxd_wanted_t14s0, &hxd_wanted_t12s0, &hxd_wanted_t13s1, &hxd_wanted_t13s8,
"spam", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t13s1 = {
&hxd_wanted_t13s2, &hxd_wanted_t13s0, 0, 0,
"conn_max", 0, TYPE_INT, CONFSOFF(spam.conn_max),
0, {(void *)5}, {0} };
struct wanted hxd_wanted_t13s2 = {
&hxd_wanted_t13s3, &hxd_wanted_t13s1, 0, 0,
"reconn_time", 0, TYPE_INT, CONFSOFF(spam.reconn_time),
0, {(void *)2}, {0} };
char *hxd_wanted_t13s3_sa[] = {
	"message delivered by",
	"message was delivered by",
	"all new hotline mass",
	0
};
struct wanted hxd_wanted_t13s3 = {
&hxd_wanted_t13s4, &hxd_wanted_t13s2, 0, 0,
"messagebot", 0, TYPE_STR_ARRAY, CONFSOFF(spam.messagebot),
0, {hxd_wanted_t13s3_sa}, {0} };
struct wanted hxd_wanted_t13s4 = {
&hxd_wanted_t13s5, &hxd_wanted_t13s3, 0, 0,
"spam_max", 0, TYPE_INT, CONFSOFF(spam.spam_max),
0, {(void *)100}, {0} };
struct wanted hxd_wanted_t13s5 = {
&hxd_wanted_t13s6, &hxd_wanted_t13s4, 0, 0,
"spam_time", 0, TYPE_INT, CONFSOFF(spam.spam_time),
0, {(void *)5}, {0} };
struct tablearray hxd_wanted_t13s6_ta[] = {
	{{0, 10}},
	{{101, 20}},
	{{103, 20}},
	{{105, 2}},
	{{108, 2}},
	{{110, 1}},
	{{112, 10}},
	{{113, 8}},
	{{114, 2}},
	{{115, 2}},
	{{116, 1}},
	{{120, 2}},
	{{121, 4}},
	{{200, 3}},
	{{202, 3}},
	{{203, 3}},
	{{204, 7}},
	{{205, 7}},
	{{206, 7}},
	{{207, 7}},
	{{208, 7}},
	{{209, 7}},
	{{210, 3}},
	{{213, 3}},
	{{214, 1}},
	{{300, 20}},
	{{303, 1}},
	{{304, 20}},
	{{348, 20}},
	{{349, 20}},
	{{350, 20}},
	{{351, 20}},
	{{352, 10}},
	{{353, 20}},
	{{355, 20}},
	{{1862, 20}},
	{{1863, 1}},
};
struct wanted hxd_wanted_t13s6 = {
&hxd_wanted_t13s7, &hxd_wanted_t13s5, 0, 0,
"spamconf", 0, TYPE_TABLE, CONFSOFF(spam.spamconf),
0, {hxd_wanted_t13s6_ta}, {(void *)37} };
struct wanted hxd_wanted_t13s7 = {
&hxd_wanted_t13s8, &hxd_wanted_t13s6, 0, 0,
"chat_max", 0, TYPE_INT, CONFSOFF(spam.chat_max),
0, {(void *)20}, {0} };
struct wanted hxd_wanted_t13s8 = {
0, &hxd_wanted_t13s7, 0, 0,
"chat_time", 0, TYPE_INT, CONFSOFF(spam.chat_time),
0, {(void *)5}, {0} };
struct wanted hxd_wanted_t14s0 = {
&hxd_wanted_t15s0, &hxd_wanted_t13s0, &hxd_wanted_t14s1, &hxd_wanted_t14s3,
"cipher", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t14s1 = {
&hxd_wanted_t14s2, &hxd_wanted_t14s0, 0, 0,
"egd_path", 0, TYPE_STR, CONFSOFF(cipher.egd_path),
0, {"./entropy"}, {0} };
char *hxd_wanted_t14s2_sa[] = {
	"RC4",
	"BLOWFISH",
	"IDEA",
	0
};
struct wanted hxd_wanted_t14s2 = {
&hxd_wanted_t14s3, &hxd_wanted_t14s1, 0, 0,
"ciphers", 0, TYPE_STR_ARRAY, CONFSOFF(cipher.ciphers),
0, {hxd_wanted_t14s2_sa}, {0} };
struct wanted hxd_wanted_t14s3 = {
0, &hxd_wanted_t14s2, 0, 0,
"cipher_only", 0, TYPE_BOOL, CONFSOFF(cipher.cipher_only),
0, {(void *)0}, {0} };
struct wanted hxd_wanted_t15s0 = {
0, &hxd_wanted_t14s0, &hxd_wanted_t15s1, &hxd_wanted_t15s1,
"irc", 0, 0, 0, 0, {0}, {0} };
struct wanted hxd_wanted_t15s1 = {
0, &hxd_wanted_t15s0, 0, 0,
"hostname", 0, TYPE_STR, CONFSOFF(irc.hostname),
0, {"chatonly.org"}, {0} };
