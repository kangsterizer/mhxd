#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "conf.h"
#include "xmalloc.h"
#include "sys_deps.h"
#include "getline.h"
#include <stdio.h>

extern void hxd_log (const char *fmt, ...);

struct type {
	char *str;
	int len;
	int type;
};

struct type types[] = {
	{"int", 3, TYPE_INT},
	{"oint", 4, TYPE_INT_OCTAL},
	{"bool", 4, TYPE_BOOL},
	{"str", 3, TYPE_STR},
	{"strarr", 6, TYPE_STR_ARRAY},
	{"enum", 4, TYPE_ENUM},
	{"include", 7, TYPE_INCLUDE},
	{"table", 5, TYPE_TABLE}
};
#define NTYPES (sizeof(types)/sizeof(struct type))

static int
chrexpand (char *str, int len)
{
	char *p;
	int off;

	p = str;
	off = 1;
	switch (p[1]) {
		case 'r':
			p[1] = '\r';
			break;
		case 'n':
			p[1] = '\n';
			break;
		case 't':
			p[1] = '\t';
			break;
		case 'a':
			p[1] = '\a';
			break;
		case 'b':
			p[1] = '\b';
			break;
		case 'v':
			p[1] = '\v';
			break;
		case 'f':
			p[1] = '\f';
			break;
		case 'x':
			while (isxdigit(p[off+1]) && off < 3)
				off++;
			p[off] = (char)strtoul(p+2, 0, 16);
			break;
		default:
			if (!isdigit(p[1]) || p[1] >= '8')
				break;
			while ((isdigit(p[off+1]) && p[off+1] < '8') && off < 3)
				off++;
			p[off] = (char)strtoul(p+2, 0, 8);
			break;
	}
	len -= off;
	memcpy(p, p+off, len);
	p[len] = 0;

	return off;
}

static struct wanted *
conf_read_recursive (const char *filename, struct wanted *wanted_top, int toplevel, int recurs)
{
	int fd, linen, linelen, l, lookfor_type, comment, nbs = 0, inquotes = 0;
	char *line, *p, *xp;
	char *bakyp, *yp, *zp;
	struct wanted *wanted, *w, *last_wanted;
	struct wanted *wp;
	int want_type = 0;
	unsigned int i;
	int type;
	struct enumarray *enumarr, *en;
	struct tablearray *ta;
	int last_enum;
	int isx, x, y;
	unsigned int nenum;
	getline_t *gl;
	char *bbuf = 0, *begin = 0;
	int bbuflen = 0, bbufsize = 0, bbuf_copy = 0;

#define MAX_RECURS 16
	if (recurs > MAX_RECURS) {
		/* hxd_log("max recursion depth (%u) reached\n", MAX_RECURS); */
		return 0;
	}

	fd = SYS_open(filename, O_RDONLY, 0);
	if (fd < 0)
		return 0;
	linen = 1;
	lookfor_type = 0;
	if (!wanted_top) {
		wanted_top = xmalloc(sizeof(struct wanted));
		memset(wanted_top, 0, sizeof(struct wanted));
	}
	gl = getline_open(fd);
	last_wanted = w = wanted = wanted_top;
	if (toplevel)
		want_type = 1;
	for (;;) {
		line = getline_line(gl, &linelen);
		if (!line)
			break;
		/* no multiline comments */
		comment = 1;
		/* no multiline quotes */
		inquotes = 0;
		/* no multiline escapes */
		nbs = 0;
		linen++;
		if (bbuf_copy) {
			if (bbuflen >= bbufsize) {
				bbufsize += 1024;
				bbuf = xrealloc(bbuf, bbufsize);
			}
			bbuf[bbuflen] = '\n';
			bbuflen++;
		}
		for (p = line; *p; p++) {
			if (comment == 2)
				continue;
			/* count number of backslashes */
			if (*p == '\\') {
				if (*(p-1)=='\\')
					nbs++;
				else
					nbs = 1;
				linelen -= chrexpand(p, (line+linelen)-p);
				p--;
				continue;
			}
			nbs = 0;
			if (bbuf_copy) {
				if (bbuflen >= bbufsize) {
					bbufsize += 1024;
					bbuf = xrealloc(bbuf, bbufsize);
				}
				bbuf[bbuflen] = *p;
				bbuflen++;
			}
			if (isspace(*p)) {
				continue;
			}
			if (*p == '#' && comment == 1 && !inquotes) {
				comment = 2;
				continue;
			}
			if (*p == '"') {
				if (nbs%2!=1)
					inquotes = !inquotes;
			}
			if (w->type && *p == ';' && !inquotes && nbs%2!=1) {
				unsigned int len;
				char *bakp = p;

				bbuf_copy = 0;
				p = bbuf;
				while (isspace(*p))
					p++;
				begin = p;
				if (begin[0] == '"')
					begin++;
				/* skip ';' */
				p = bbuf+bbuflen-2;
				while (isspace(*p) && p > begin)
					p--;
				if (*p == '"')
					len = p - begin;
				else
					len = bbuf+bbuflen-1 - begin;
				p = bakp;
				w->valstr = xmalloc(len+1);
				memcpy(w->valstr, begin, len);
				w->valstr[len] = 0;
				w->flags |= WANTED_FLAG_ALLOCED_VALSTR;
				switch (w->type) {
					case 0:
						break;
					case TYPE_INT:
						w->val.i = strtol(w->valstr, 0, 0);
						break;
					case TYPE_INT_OCTAL:
						w->val.i = strtol(w->valstr, 0, 8);
						break;
					case TYPE_INCLUDE:
						w->val.s = w->valstr;
						break;
					case TYPE_STR:
						w->val.s = w->valstr;
						break;
					case TYPE_STR_ARRAY:
						{
						char **arr;
						char *beg;
						unsigned int nstrs;
						int xinquotes, xnbs;

						arr = 0;
						nstrs = 0;
						xinquotes = 0;
						xnbs = 0;
						xp = bbuf;
						while (isspace(*xp))
							xp++;
						for (beg = xp; xp < bbuf+bbuflen; xp++) {
#if 0
							/* count number of backslashes */
							if (*xp == '\\') {
								if (*(xp-1)=='\\')
									xnbs++;
								else
									xnbs = 1;
								continue;
							}
#endif
							xnbs = 0;
							if (*xp == '"') {
								if (xnbs%2!=1)
									xinquotes = !xinquotes;
								continue;
							}
							if ((*xp == ',' || *xp == ';') && !xinquotes && xnbs%2!=1) {
								char *bakxp = xp;
								xp--;
								while (isspace(*xp))
									xp--;
								if (*xp == '"')
									*xp = 0;
								xp = bakxp;
								*xp = 0;
								xp++;
								if (*beg == '"')
									beg++;
								arr = xrealloc(arr, sizeof(char *)*(nstrs+1));
								arr[nstrs] = xstrdup(beg);
								while (isspace(*xp))
									xp++;
								beg = xp;
								xp--;
								nstrs++;
							}
						}
						arr = (char **)xrealloc(arr, sizeof(char *)*(nstrs+1));
						arr[nstrs] = 0;
						w->val.sa = arr;
						w->flags |= WANTED_FLAG_ALLOCED_VAL;
						}
						break;
					case TYPE_BOOL:
						begin = bbuf;
						while (begin < bbuf+bbuflen && isspace(*begin))
							begin++;
						if (begin == bbuf+bbuflen)
							break;
						if (isdigit(*(w->valstr)))
							w->val.i = strtol(begin, 0, 0);
						else if (!strcmp(begin, "true")
							 || !strcmp(w->valstr, "on")
							 || !strcmp(w->valstr, "ja")
							 || !strcmp(w->valstr, "yes")
							 || !strcmp(w->valstr, "si")
							 || !strcmp(w->valstr, "hai")
							 || !strcmp(w->valstr, "oui"))
							w->val.i = 1;
						else
							w->val.i = 0;
						break;
					case TYPE_ENUM:
						for (i = 0; w->ext.enuma[i].sym; i++) {
							if (!strcmp(w->valstr, w->ext.enuma[i].sym)) {
								w->val.i = i+1;
								break;
							}
						}
						break;
					case TYPE_TABLE:
						xp = bbuf;
						isx = 0;
						i = 0;
						x = 0;
						ta = 0;
						while (xp < bbuf+bbuflen) {
							while (xp < bbuf+bbuflen && isspace(*xp))
								xp++;
							if (*xp == '#')
								goto skipline;
							if (xp >= bbuf+bbuflen)
								break;
							isx = !isx;
							if (isx)
								x = strtoul(xp, 0, 0);
							else {
								y = strtoul(xp, 0, 0);

								ta = xrealloc(ta, (i+1)*sizeof(struct tablearray));
								ta[i].i[0] = x;
								ta[i].i[1] = y;
								i++;
							}
							while (xp < bbuf+bbuflen && !isspace(*xp)) {
								xp++;
								if (*xp == '#') {
skipline:
									while (xp < bbuf+bbuflen && *xp != '\n')
										xp++;
									break;
								}
							}
						}
						w->val.ta = ta;
						w->ext.nta = i;
						if (ta)
							w->flags |= WANTED_FLAG_ALLOCED_VAL;
						break;
				}
				lookfor_type = 0;
				inquotes = 0;
				nbs = 0;
				if (w->type == TYPE_INCLUDE) {
					wp = w;
					w = conf_read_recursive(w->val.s, 0, 1, recurs+1);
					if (!w) {
						w = wp;
						continue;
					}
					if (!w->subnext) {
						w = wp;
						continue;
					}
					memcpy(wp, w->subnext, sizeof(struct wanted));
					w = wp;
					for (wp = w; wp; wp = wp->next) {
						if (!wanted->subtail->next) {
							if (wanted->subtail != wp)
								wanted->subtail->next = wp;
						}
						if (!wp->prev) {
							if (wanted->subtail != wp)
								wp->prev = wanted->subtail;
						}
						wanted->subtail = wp;
						w = wp;
					}
				}
				continue;
			} else if (*p == '{') {
				want_type = 1;
				continue;
			} else if (*p == '}') {
				/*if (wanted->prev)*/
				w->next = 0;
				w = wanted = wanted_top;
				want_type = 0;
				while (*(p+1) == ';')
					p++;
				continue;
			}
			if (lookfor_type)
				continue;
			type = 0;
			enumarr = 0;
			nenum = 0;
			if (want_type) {
				for (i = 0; i < NTYPES; i++) {
					l = types[i].len;
					if (!memcmp(p, types[i].str, l) && (isspace(*(p+l)) || *(p+l)=='{' || *(p+l)==0))
						goto foundtype;
				}
				/* hxd_log("line %d: no such type\n", line); */
				goto nottype;
foundtype:
				type = types[i].type;
				p += types[i].len;
				/* this should be a new function */
				if (type == TYPE_ENUM) {
					/* read until '}' */
					bbuflen = 0;
					for (;;) {
						while (*p) {
							if (bbuflen >= bbufsize) {
								bbufsize += 1024;
								bbuf = xrealloc(bbuf, bbufsize);
							}
							bbuf[bbuflen] = *p;
							bbuflen++;
							if (*p == '}') {
								break;
							}
							p++;
						}
						if (*p == '}') {
							p++;
							break;
						}
						p = getline_line(gl, &linelen);
						if (!p)
							goto done;
					}
					last_enum = 0;
					nenum = 0;
					xp = strchr(bbuf, '{');
					if (!xp)
						goto notenum;
					xp++;
					while (isspace(*xp))
						xp++;
					while (*xp != '}' && !last_enum) {
						yp = strchr(xp, '=');
						if (!yp)
							goto notenum;
						zp = yp+1;
						while (*zp && (*zp != ',' && *zp != '}'))
							zp++;
						if (!*zp)
							goto notenum;
						if (*zp == '}')
							last_enum = 1;
						zp--;
						while (isspace(*zp))
							zp--;
						zp++;
						*zp = 0;
						enumarr = xrealloc(enumarr, sizeof(struct enumarray)*(nenum+2));
						en = &enumarr[nenum];
						bakyp = yp+1;
						yp--;
						while (isspace(*yp))
							yp--;
						*(yp+1) = 0;
						yp = bakyp;
						while (isspace(*yp))
							yp++;
						en->sym = xstrdup(xp);
						en->valstr = xstrdup(yp);
						nenum++;
						xp = zp + 1;
						while (isspace(*xp))
							xp++;
					}
					goto isenum;
notenum:
					type = TYPE_INT;
				}
isenum:
				if (nenum) {
					en = &enumarr[nenum];
					en->sym = 0;
					en->valstr = 0;
				}
				while (isspace(*p))
					p++;
			}
nottype:
			wp = w;
			if ((!want_type && !wanted->type) || (want_type && wanted->type)) {
				for (w = wanted; w; w = w->next) {
					if (!w->str)
						continue;
					l = strlen(w->str);
					if (!memcmp(p, w->str, l)
					    && (isspace(*(p+l)) || *(p+l)=='{' || *(p+l)==0)) {
						goto found;
					}
				}
			}
			if (want_type && type == 0) {
				w = wp;
				continue;
			}
			w = xmalloc(sizeof(struct wanted));
			memset(w, 0, sizeof(struct wanted));
			w->type = type;
			if (type == TYPE_ENUM) {
				w->ext.enuma = enumarr;
				w->flags |= WANTED_FLAG_ALLOCED_ENUMARR;
			}
found:
			l = 0;
			while (p[l] && !isspace(p[l]) && p[l]!='{')
				 l++;
			if (!w->str) {
				w->str = xmalloc(l+1);
				memcpy(w->str, p, l);
				w->str[l] = 0;
			}
			if (w->type == 0) {
				if (!last_wanted->next)
					last_wanted->next = w;
				if (!w->prev)
					w->prev = last_wanted;
				last_wanted = w;
				if (w->subnext)
					wanted = w->subnext;
				else
					wanted = w;
				wanted->subtail = 0;
			} else {
				comment = 0;
				lookfor_type = 1;
				if (wanted->subtail) {
					if (!wanted->subtail->next) {
						wanted->subtail->next = w;
					}
					if (!w->prev)
						w->prev = wanted->subtail;
					wanted->subtail = w;
				} else {
					if (!wanted->subnext)
						wanted->subnext = w;
					wanted->subtail = w;
					if (!w->prev)
						w->prev = wanted;
				}
			}
			p += l;
			while (*p && isspace(*p))
				p++;
			bbuf_copy = 1;
			bbuflen = 0;
			p--;
		}
	}
done:
	close(fd);
	getline_close(gl);
	if (bbuf)
		xfree(bbuf);

	return wanted_top;
}

struct wanted *
conf_read (const char *filename, struct wanted *wanted_top)
{
	return conf_read_recursive(filename, wanted_top, 0, 0);
}

void
conf_wanted_to_config (void *__cfgp, struct wanted *wanted_top)
{
	unsigned int i;
	struct wanted *w, *ws;
	u_int8_t *cfgp, *p;
	char **sa, *sp;
	conf_table_t *t;

	cfgp = (u_int8_t *)__cfgp;
	for (w = wanted_top->next; w; w = w->next) {
		for (ws = w->subnext; ws; ws = ws->next) {
			if (ws->soff < 0)
				continue;
			p = cfgp + ws->soff;
			switch (ws->type) {
				case TYPE_INT:
				case TYPE_INT_OCTAL:
				case TYPE_BOOL:
					*((int *)p) = ws->val.i;
					break;
				case TYPE_INCLUDE:
				case TYPE_STR:
					sp = *((char **)p);
					if (sp)
						xfree(sp);
					if (ws->val.s)
						*((char **)p) = xstrdup(ws->val.s);
					else
						*((char **)p) = 0;
					break;
				case TYPE_STR_ARRAY:
					sa = *((char ***)p);
					if (sa) {
						for (i = 0; sa[i]; i++)
							xfree(sa[i]);
						xfree(sa);
					}
					i = 0;
					if (ws->val.sa)
						for (i = 0; ws->val.sa[i]; i++)
							;
					sa = xmalloc(sizeof(char *)*(i+1));
					if (ws->val.sa)
						for (i = 0; ws->val.sa[i]; i++)
							sa[i] = xstrdup(ws->val.sa[i]);
					sa[i] = 0;
					*((char ***)p) = sa;
					break;
				case TYPE_ENUM:
					*((int *)p) = ws->val.i;
					break;
				case TYPE_TABLE:
					t = (conf_table_t *)p;
					if (!t)
						break;
					if (t->ta) {
						xfree(t->ta);
						t->ta = 0;
					}
					if (ws->val.ta) {
						t->nta = ws->ext.nta;
						if (t->nta) {
							t->ta = xmalloc(t->nta*sizeof(struct tablearray));
							memcpy(t->ta, ws->val.ta, t->nta*sizeof(struct tablearray));
						}
					} else {
						t->nta = 0;
					}
					break;
				default:
					break;
			}
		}
	}
}

void
conf_config_free (void *__cfgp, struct wanted *wanted_top)
{
	unsigned int i;
	struct wanted *w, *ws;
	u_int8_t *cfgp, *p;
	u_int8_t *s, **sa;
	struct tablearray *ta;

	cfgp = (u_int8_t *)__cfgp;
	for (w = wanted_top->next; w; w = w->next) {
		for (ws = w->subnext; ws; ws = ws->next) {
			p = cfgp + ws->soff;
			switch (ws->type) {
				case TYPE_INCLUDE:
				case TYPE_STR:
					s = *((u_int8_t **)p);
					if (s) {
						xfree(s);
						*((char **)p) = 0;
					}
					break;
				case TYPE_STR_ARRAY:
					sa = *((u_int8_t ***)p);
					if (sa) {
						for (i = 0; sa[i]; i++)
							xfree(sa[i]);
						xfree(sa);
						*((char ***)p) = 0;
					}
					break;
				case TYPE_TABLE:
					ta = *((struct tablearray **)p);
					if (ta) {
						xfree(ta);
						*((struct tablearray **)p) = 0;
					}
					break;
				default:
					break;
			}
		}
	}
}

void
conf_wanted_freevars (struct wanted *wanted_top)
{
	unsigned int i;
	struct wanted *w, *ws, *next, *subnext;

	for (w = wanted_top->next; w; w = next) {
		for (ws = w->subnext; ws; ws = subnext) {
			if ((ws->flags & WANTED_FLAG_ALLOCED_VAL)) {
				switch (ws->type) {
					case TYPE_INCLUDE:
					case TYPE_STR:
						if (ws->val.s && ws->val.s != ws->valstr) {
							xfree(ws->val.s);
							ws->val.s = 0;
						}
						break;
					case TYPE_STR_ARRAY:
						if (ws->val.sa) {
							for (i = 0; ws->val.sa[i]; i++)
								xfree(ws->val.sa[i]);
							xfree(ws->val.sa);
							ws->val.sa = 0;
						}
						break;
					case TYPE_TABLE:
						if (ws->val.ta) {
							xfree(ws->val.ta);
							ws->val.ta = 0;
						}
						break;
					default:
						break;
				}
			}
			if ((ws->flags & WANTED_FLAG_ALLOCED_VALSTR) && ws->valstr) {
				xfree(ws->valstr);
				ws->valstr = 0;
			}
			subnext = ws->next;
		}
		next = w->next;
	}
}

void
conf_wanted_free (struct wanted *wanted_top)
{
	unsigned int i;
	struct wanted *w, *ws, *next, *subnext;

	for (w = wanted_top->next; w; w = next) {
		for (ws = w->subnext; ws; ws = subnext) {
			if ((ws->flags & WANTED_FLAG_ALLOCED_VAL)) {
				switch (ws->type) {
					case TYPE_INCLUDE:
					case TYPE_STR:
						if (ws->val.s) {
							xfree(ws->val.s);
							ws->val.s = 0;
						}
						break;
					case TYPE_STR_ARRAY:
						if (ws->val.sa) {
							for (i = 0; ws->val.sa[i]; i++)
								xfree(ws->val.sa[i]);
							xfree(ws->val.sa);
							ws->val.sa = 0;
						}
						break;
					case TYPE_TABLE:
						if (ws->val.ta) {
							xfree(ws->val.ta);
							ws->val.ta = 0;
						}
						break;
					default:
						break;
				}
			}
			if ((ws->flags & WANTED_FLAG_ALLOCED_ENUMARR) && ws->ext.enuma) {
				for (i = 0; ws->ext.enuma[i].sym; i++) {
					xfree(ws->ext.enuma[i].sym);
					xfree(ws->ext.enuma[i].valstr);
				}
				if (ws->ext.enuma)
					xfree(ws->ext.enuma);
			}
			if ((ws->flags & WANTED_FLAG_ALLOCED_VALSTR) && ws->valstr)
				xfree(ws->valstr);
			subnext = ws->next;
			if ((ws->flags & WANTED_FLAG_ALLOCED))
				xfree(ws);
		}
		next = w->next;
		if ((w->flags & WANTED_FLAG_ALLOCED))
			xfree(w);
	}
	xfree(wanted_top);
}
