#ifndef __hxd__config_h
#define __hxd__config_h

#include "conf.h"

struct hxd_config {
	struct hxd_config_options {
		conf_int_t port;
		conf_strarr_t addresses;
		conf_str_t unix_address;
		conf_int_t version;
		conf_int_t away_time;
		conf_int_t ban_time;
		conf_bool_t ignore_commands;
		conf_bool_t self_info;
		conf_bool_t kick_transients;
		conf_int_t gid;
		conf_int_t uid;
		conf_bool_t detach;
		conf_bool_t ident;
		conf_bool_t hldump;
		conf_strarr_t exclude;
	} options;
	struct hxd_config_text {
		conf_str_t server_encoding;
		conf_str_t client_encoding;
		conf_bool_t convert_mac;
	} text;
	struct hxd_config_modules {
		conf_strarr_t load;
		conf_strarr_t reload;
	} modules;
	struct hxd_config_operation {
		conf_bool_t hfs;
		conf_bool_t nospam;
		conf_bool_t commands;
		conf_bool_t exec;
		conf_bool_t tracker_register;
		conf_bool_t tnews;
	} operation;
	struct hxd_config_banner {
		conf_str_t type;
		conf_str_t file;
		conf_str_t url;
	} banner;
	struct hxd_config_limits {
		conf_int_t total_downloads;
		conf_int_t total_uploads;
		conf_int_t queue_size;
		conf_int_t individual_downloads;
		conf_int_t individual_uploads;
		conf_int_t out_Bps;
		conf_int_t uploader_out_Bps;
		conf_int_t total_exec;
		conf_int_t individual_exec;
	} limits;
	struct hxd_config_tracker {
		conf_str_t name;
		conf_str_t description;
		conf_strarr_t trackers;
		conf_int_t interval;
		conf_int_t nusers;
		conf_str_t password;
	} tracker;
	struct hxd_config_paths {
		conf_str_t files;
		conf_str_t accounts;
		conf_str_t exec;
		conf_str_t news;
		conf_str_t newsdir;
		conf_str_t agreement;
		conf_str_t log;
		conf_str_t banlist;
		conf_str_t tracker_banlist;
		conf_str_t hldump;
		conf_str_t avlist;
		conf_str_t luser;
		conf_str_t nuser;
		conf_str_t duser;
		conf_str_t modules;
	} paths;
	struct hxd_config_permissions {
		conf_oint_t umask;
		conf_oint_t files;
		conf_oint_t directories;
		conf_oint_t account_files;
		conf_oint_t account_directories;
		conf_oint_t log_files;
		conf_oint_t news_files;
	} permissions;
	struct hxd_config_files {
		conf_str_t comment;
#define HFS_FORK_CAP 1
#define HFS_FORK_DOUBLE 2
#define HFS_FORK_NETATALK 3
		conf_enum_t fork;
	} files;
	struct hxd_config_strings {
		conf_str_t news_divider;
		conf_str_t news_format;
		conf_str_t news_time_format;
		conf_str_t chat_format;
		conf_str_t chat_opt_format;
		conf_str_t download_info;
		conf_str_t upload_info;
	} strings;
	struct hxd_config_sql {
		conf_str_t host;
		conf_str_t user;
		conf_str_t pass;
		conf_str_t data;
	} sql;
	struct hxd_config_spam {
		conf_int_t conn_max;
		conf_int_t reconn_time;
		conf_strarr_t messagebot;
		conf_int_t spam_max;
		conf_int_t spam_time;
		conf_table_t spamconf;
		conf_int_t chat_max;
		conf_int_t chat_time;
	} spam;
	struct hxd_config_cipher {
		conf_str_t egd_path;
		conf_strarr_t ciphers;
		conf_bool_t cipher_only;
	} cipher;
	struct hxd_config_irc {
		conf_str_t hostname;
	} irc;
};

#endif
