#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "conf.h"
#include "xmalloc.h"
#include "sys_deps.h"
#include "getopt.h"

char *prefix = "";
int dynamic_confsoff = 0;

int
strunexpand (char *str, int slen, char *buf, int blen)
{
	char *p, *bp, c;

	for (p = str, bp = buf; p < str+slen && bp < buf+blen; p++, bp++) {
		if (isgraph(*p) || *p == ' '
		    /* && (*p != '\'' && *p != '"' && *p != '?' && *p != '*') */
				) {
			*bp = *p;
			continue;
		}
		*bp++ = '\\';
		switch (*p) {
			case '\r':
				*bp = 'r';
				break;
			case '\n':
				*bp = 'n';
				break;
			case '\t':
				*bp = 't';
				break;
#if 0
			case ' ':
			case '\'':
			case '"':
			case '*':
			case '?':
				*bp = *p;
				break;
#endif
			default:
				*bp++ = 'x';
				c = (*p >> 4) & 0xf;
				c = c > 9 ? c - 0xa + 'a' : c + '0';
				*bp++ = c;
				c = *p & 0xf;
				c = c > 9 ? c - 0xa + 'a' : c + '0';
				*bp = c;
		}
	}
	if (bp == buf+blen)
		bp--;
	*bp = 0;
	blen = bp - buf;

	return blen;
}

void
print_config_file (struct wanted *wanted_top)
{
	unsigned int i;
	struct wanted *w, *ws;
	char estr[1024];

	for (w = wanted_top->next; w; w = w->next) {
		printf("%s {\n", w->str);
		for (ws = w->subnext; ws; ws = ws->next) {
			printf("\t%s ", ws->str);
			switch (ws->type) {
				case TYPE_INT:
					printf("%ld", ws->val.i);
					break;
				case TYPE_INT_OCTAL:
					printf("0%lo", (unsigned long int)ws->val.i);
					break;
				case TYPE_BOOL:
					printf("%ld", ws->val.i);
					break;
				case TYPE_INCLUDE:
				case TYPE_STR:
					strunexpand(ws->val.s, strlen(ws->val.s), estr, sizeof(estr));
					printf("\"%s\"", estr);
					break;
				case TYPE_STR_ARRAY:
					strunexpand(ws->val.sa[0], strlen(ws->val.sa[0]), estr, sizeof(estr));
					printf("\"%s\"", ws->val.sa[0]);
					for (i = 1; ws->val.sa[i]; i++) {
						strunexpand(ws->val.sa[i], strlen(ws->val.sa[i]), estr, sizeof(estr));
						printf(", \"%s\"", estr);
					}
					break;
				case TYPE_ENUM:
					printf("%s", ws->valstr);
					break;
			}
			printf(";\n");
		}
		printf("};\n\n");
	}
}

void
print_config_struct (struct wanted *wanted_top)
{
	struct wanted *w, *ws;

	printf("struct %sconfig {\n", prefix);
	for (w = wanted_top->next; w; w = w->next) {
		printf("\tstruct %sconfig_%s {\n", prefix, w->str);
		for (ws = w->subnext; ws; ws = ws->next) {
			if (ws->type == TYPE_ENUM) {
				unsigned int i;

				for (i = 0; ws->ext.enuma[i].valstr; i++)
					printf("#define %s %u\n", ws->ext.enuma[i].valstr, i+1);
			}
			printf("\t\t");
			switch (ws->type) {
				case TYPE_INT:
					printf("conf_int_t");
					break;
				case TYPE_INT_OCTAL:
					printf("conf_oint_t");
					break;
				case TYPE_BOOL:
					printf("conf_bool_t");
					break;
				case TYPE_STR:
					printf("conf_str_t");
					break;
				case TYPE_INCLUDE:
					printf("conf_str_t");
					break;
				case TYPE_STR_ARRAY:
					printf("conf_strarr_t");
					break;
				case TYPE_ENUM:
					printf("conf_enum_t");
					break;
				case TYPE_TABLE:
					printf("conf_table_t");
					break;
			}
			printf(" %s;\n", ws->str);
		}
		printf("\t} %s;\n", w->str);
	}
	printf("};\n");
}

void
print_config_wanted (struct wanted *wanted_top)
{
	struct wanted *w, *ws;
	unsigned int tind = 0, sind;
	char nextstr[256], prevstr[256], subnextstr[256], subtailstr[256], typestr[16];
	char enumstr[256], soffstr[256];
	char estr[1024];

	printf("struct %sconfig %sconfig_default;\n", prefix, prefix);
	printf("#define CONFSOFF(_x) (((char *)&%sconfig_default._x)-((char *)&%sconfig_default))\n", prefix, prefix);
	printf("struct wanted %swanted_t0s0;\n", prefix);
	for (w = wanted_top->next; w; w = w->next) {
		tind++;
		sind = 0;
		printf("struct wanted %swanted_t%us%u;\n", prefix, tind, sind);
		for (ws = w->subnext; ws; ws = ws->next) {
			sind++;
			printf("struct wanted %swanted_t%us%u;\n", prefix, tind, sind);
		}
	}
	tind = 0;
	printf("struct wanted %swanted_t0s0 = {\n"
		"&%swanted_t1s0, 0, 0, 0,\n"
		"0, 0, 0,\n"
		"0, 0, {0}, {0} };\n",
		prefix, prefix);
	for (w = wanted_top->next; w; w = w->next) {
		tind++;
		sind = 0;
		if (w->next)
			snprintf(nextstr, sizeof(nextstr), "&%swanted_t%us0", prefix, tind+1);
		else
			strcpy(nextstr, "0");
		if (w->prev)
			snprintf(prevstr, sizeof(prevstr), "&%swanted_t%us0", prefix, tind-1);
		else
			strcpy(prevstr, "0");
		if (w->subnext) {
			snprintf(subnextstr, sizeof(subnextstr), "&%swanted_t%us1", prefix, tind);
			for (ws = w->subnext; ws; ws = ws->next)
				sind++;
			snprintf(subtailstr, sizeof(subtailstr), "&%swanted_t%us%u", prefix, tind, sind);
		} else {
			strcpy(prevstr, "0");
			strcpy(subtailstr, "0");
		}
		sind = 0;
		printf("struct wanted %swanted_t%us%u = {\n"
			"%s, %s, %s, %s,\n"
			"\"%s\", 0, 0, 0, 0, {0}, {0} };\n",
			prefix, tind, sind,
			nextstr, prevstr, subnextstr, subtailstr, w->str);
		for (ws = w->subnext; ws; ws = ws->next) {
			sind++;
			if (ws->next)
				snprintf(nextstr, sizeof(nextstr), "&%swanted_t%us%u", prefix, tind, sind+1);
			else
				strcpy(nextstr, "0");
			if (ws->prev)
				snprintf(prevstr, sizeof(prevstr), "&%swanted_t%us%u", prefix, tind, sind-1);
			else
				strcpy(prevstr, "0");
			strcpy(subnextstr, "0");
			strcpy(subtailstr, "0");
			switch (ws->type) {
				case TYPE_INT:
					strcpy(typestr, "TYPE_INT");
					break;
				case TYPE_INT_OCTAL:
					strcpy(typestr, "TYPE_INT_OCTAL");
					break;
				case TYPE_BOOL:
					strcpy(typestr, "TYPE_BOOL");
					break;
				case TYPE_INCLUDE:
					strcpy(typestr, "TYPE_INCLUDE");
					break;
				case TYPE_STR:
					strcpy(typestr, "TYPE_STR");
					break;
				case TYPE_STR_ARRAY:
					strcpy(typestr, "TYPE_STR_ARRAY");
					break;
				case TYPE_ENUM:
					strcpy(typestr, "TYPE_ENUM");
					break;
				case TYPE_TABLE:
					strcpy(typestr, "TYPE_TABLE");
					break;
			}
			if (dynamic_confsoff)
				strcpy(soffstr, "0");
			else
				snprintf(soffstr, sizeof(soffstr), "CONFSOFF(%s.%s)",w->str,ws->str);
			if (ws->type == TYPE_ENUM) {
				unsigned int i;

				snprintf(enumstr, sizeof(enumstr), "%senum_t%us%u", prefix, tind, sind);
				printf("struct enumarray %s[] = {\n", enumstr);
				for (i = 0; ws->ext.enuma[i].valstr; i++) {
					printf("\t{\"%s\", \"%s\"},\n", ws->ext.enuma[i].sym, ws->ext.enuma[i].valstr);
				}
				printf("\t{0, 0}\n};\n");
			} else if (ws->type == TYPE_TABLE) {
				snprintf(enumstr, sizeof(enumstr), "(void *)%u", ws->ext.nta);
			} else {
				strcpy(enumstr, "0");
			}
			if (ws->type == TYPE_STR_ARRAY) {
				unsigned int i;

				printf("char *%swanted_t%us%u_sa[] = {\n", prefix, tind, sind);
				for (i = 0; ws->val.sa[i]; i++) {
					strunexpand(ws->val.sa[i], strlen(ws->val.sa[i]), estr, sizeof(estr));
					printf("\t\"%s\",\n", estr);
				}
				printf("\t0\n};\n");
			} else if (ws->type == TYPE_TABLE) {
				unsigned int i;

				printf("struct tablearray %swanted_t%us%u_ta[] = {\n", prefix, tind, sind);
				for (i = 0; i < ws->ext.nta; i++) {
					printf("\t{{%d, %d}},\n", ws->val.ta[i].i[0], ws->val.ta[i].i[1]);
				}
				printf("};\n");
			}
			printf("struct wanted %swanted_t%us%u = {\n"
				"%s, %s, %s, %s,\n"
				"\"%s\", 0, %s, %s,\n"
				"0, {",
				prefix, tind, sind,
				nextstr, prevstr, subnextstr, subtailstr, ws->str, typestr, soffstr);
			switch (ws->type) {
				case TYPE_INT:
				case TYPE_INT_OCTAL:
				case TYPE_BOOL:
					printf("(void *)%ld", ws->val.i);
					break;
				case TYPE_INCLUDE:
				case TYPE_STR:
					strunexpand(ws->val.s, strlen(ws->val.s), estr, sizeof(estr));
					printf("\"%s\"", estr);
					break;
				case TYPE_STR_ARRAY:
					printf("%swanted_t%us%u_sa", prefix, tind, sind);
					break;
				case TYPE_ENUM:
					printf("(void *)%ld", ws->val.i);
					break;
				case TYPE_TABLE:
					printf("%swanted_t%us%u_ta", prefix, tind, sind);
					break;
			}
			printf("}, {%s} };\n", enumstr);
		}
	}
	if (dynamic_confsoff) {
		printf("void\n%swanted_init (void)\n{\n", prefix);
		tind = 0;
		for (w = wanted_top->next; w; w = w->next) {
			tind++;
			sind = 0;
			for (ws = w->subnext; ws; ws = ws->next) {
				sind++;
				printf("%swanted_t%us%u.soff = CONFSOFF(%s.%s);\n",
					prefix, tind, sind, w->str, ws->str);
			}
		}
		printf("}\n");
	}
}

void
print_config_header (struct wanted *wanted_top)
{
	printf("#ifndef __%s_config_h\n"
	       "#define __%s_config_h\n\n#include \"conf.h\"\n\n", prefix, prefix);
	print_config_struct(wanted_top);
	printf("\n#endif\n");
}


struct option genconf_opts[] = {
	{"prefix", 1, 0, 'p'},
	{"dynamic", 0, 0, 'd'},
	{"file", 0, 0, 'f'},
	{"header", 0, 0, 'h'},
	{"struct", 0, 0, 's'},
	{"wanted", 0, 0, 'w'},
	{0, 0, 0, 0}
};

void
err_printf (const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

int
main (int argc, char **argv)
{
	struct wanted *wantedp;
	char *fname;
	int o, longind;
	struct opt_r opt;
	int pfile = 0, pstruct = 0, pwanted = 0, pheader = 0;

	opt.err_printf = err_printf;
	opt.ind = 0;
	while ((o = getopt_long_r(argc, argv, "p:dfhsw", genconf_opts, &longind, &opt)) != EOF) {
		if (o == 0)
			o = genconf_opts[longind].val;
		switch (o) {
			case 'p':
				prefix = opt.arg;
				break;
			case 'd':
				dynamic_confsoff = 1;
				break;
			case 'f':
				pfile = 1;
				break;
			case 's':
				pstruct = 1;
				break;
			case 'w':
				pwanted = 1;
				break;
			case 'h':
				pheader = 1;
				break;
			default:
				break;
		}
	}
	fname = argv[opt.ind] ? argv[opt.ind] : "hxd.conf.in";
	wantedp = conf_read(fname, 0);
	if (!wantedp) {
		fprintf(stderr, "error reading %s\n", fname);
		return 1;
	}
	if (pfile)
		print_config_file(wantedp);
	if (pstruct)
		print_config_struct(wantedp);
	if (pheader)
		print_config_header(wantedp);
	if (pwanted)
		print_config_wanted(wantedp);
	conf_wanted_free(wantedp);

	return 0;
}

#include <stdarg.h>

/* for xmalloc.c */
void
hxd_log (const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
