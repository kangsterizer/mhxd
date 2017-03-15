#ifndef __hxd_conf_h
#define __hxd_conf_h

struct enumarray {
	char *sym;
	char *valstr;
};

struct tablearray {
	int i[2];
};

struct conf_table {
	unsigned int nta;
	struct tablearray *ta;
};

struct wanted {
	struct wanted *next, *prev;
	struct wanted *subnext, *subtail;
	char *str;
	char *valstr;
	unsigned int type;
	int soff;
#define WANTED_FLAG_ALLOCED 1
#define WANTED_FLAG_ALLOCED_ENUMARR 2
#define WANTED_FLAG_ALLOCED_VAL 4
#define WANTED_FLAG_ALLOCED_VALSTR 8
	unsigned int flags;
	union {
		void *p;
		long int i;
		char *s;
		char **sa;
		struct tablearray *ta;
	} val;
	union {
		void *p;
		struct enumarray *enuma;
		unsigned int nta;
		/* struct tablearray *table; */
	} ext;
};

/* exported types */
typedef int conf_int_t;
typedef int conf_oint_t;
typedef int conf_bool_t;
typedef char *conf_str_t;
typedef char **conf_strarr_t;
typedef int conf_enum_t;
typedef struct conf_table conf_table_t;

#define TYPE_INT	1
#define TYPE_INT_OCTAL	2
#define TYPE_BOOL	3
#define TYPE_STR	4
#define TYPE_STR_ARRAY	5
#define TYPE_ENUM	6
#define TYPE_INCLUDE	7
#define TYPE_TABLE	8

extern struct wanted *conf_read (const char *filename, struct wanted *top);
extern void conf_wanted_freevars (struct wanted *top);
extern void conf_wanted_free (struct wanted *top);
extern void conf_config_free (void *cfgp, struct wanted *top);
extern void conf_wanted_to_config (void *cfgp, struct wanted *top);

#endif
