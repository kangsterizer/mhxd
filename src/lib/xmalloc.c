#include "xmalloc.h"
#include <stdlib.h>
#include <string.h>

extern void hxd_log (const char *fmt, ...);

#if XMALLOC_DEBUG
struct mal_ent {
	void *ptr;
	size_t size;
	void *return_addr;
};

#define HASHSIZE	256
#define ADDRSIZE	sizeof(unsigned long)

struct mal_ent *mal_hash_tbl[HASHSIZE];
size_t mal_tbl_size[HASHSIZE];
size_t mal_nent[HASHSIZE];
struct mal_ent *mal_tbl = 0;
size_t total_allocated = 0;
size_t total_reallocated = 0;
size_t total_freed = 0;

void
DDUP (size_t hi __attribute__((__unused__)), size_t i __attribute__((__unused__)))
{
}

static inline void
DDEL (void *ptr)
{
	unsigned long addr = (unsigned long)ptr;
	unsigned long hi = (addr & ((HASHSIZE*ADDRSIZE)-1)) / ADDRSIZE;
	struct mal_ent *mal_tbl;
	size_t nent, tbl_size, i;

	mal_tbl = mal_hash_tbl[hi];
	tbl_size = mal_tbl_size[hi];
	for (i = 0; i < tbl_size; i++) {
		if (mal_tbl[i].ptr == ptr) {
			mal_tbl[i].ptr = 0;
			mal_tbl[i].size = 0;
			break;
		}
	}
	if (i == tbl_size) {
		hxd_log("DDEL: no %p", ptr);
		return;
	} else {
		total_freed++;
	}
	nent = mal_nent[hi];
	nent--;
	mal_nent[hi] = nent;
}

static inline void
DNEW (void *ptr, size_t size, void *oldptr, void *retaddr)
{
	unsigned long addr = (unsigned long)ptr;
	unsigned long hi = (addr & ((HASHSIZE*ADDRSIZE)-1)) / ADDRSIZE;
	struct mal_ent *mal_tbl;
	size_t i, nent, tbl_size;

	if (oldptr && ptr != oldptr)
		DDEL(oldptr);
	mal_tbl = mal_hash_tbl[hi];
	nent = mal_nent[hi];
	tbl_size = mal_tbl_size[hi];
	if ((nent+1) > tbl_size) {
		tbl_size = (nent+1);
		mal_tbl_size[hi] = tbl_size;
		mal_tbl = realloc(mal_tbl, tbl_size*sizeof(struct mal_ent));
		mal_tbl[nent].ptr = 0;
		mal_tbl[nent].size = 0;
		if (!mal_tbl) {
			hxd_log("DNEW: could not allocate %lu bytes",
				(unsigned long)tbl_size*sizeof(struct mal_ent));
			exit(111);
		}
		mal_hash_tbl[hi] = mal_tbl;
	}
	for (i = 0; i < tbl_size; i++) {
		if (mal_tbl[i].ptr == ptr) {
			if (!oldptr) {
				hxd_log("DNEW: duplicate ptr %p %u %u",
					ptr, size, mal_tbl[i].size);
				DDUP(hi, i);
			}
			break;
		}
	}
	if (i == tbl_size) {
		for (i = 0; i < nent; i++) {
			if (!mal_tbl[i].ptr)
				break;
		}
		nent++;
		total_allocated++;
	} else {
		total_reallocated++;
	}
	mal_tbl[i].ptr = ptr;
	mal_tbl[i].size = size;
	mal_tbl[i].return_addr = retaddr;
	mal_nent[hi] = nent;
}

void
DTBLINIT (void)
{
	memset(mal_hash_tbl, 0, sizeof(mal_hash_tbl));
	memset(mal_tbl_size, 0, sizeof(mal_tbl_size));
	memset(mal_nent, 0, sizeof(mal_nent));
}

#include <stdio.h>

void
DTBLWRITE (void)
{
	FILE *fp;
	size_t i, j, nent, tbl_size, total_nent = 0, total_size = 0;
	int first = 1;

#if defined(__GLIBC__)
	malloc_stats();
#endif

	fp = fopen("maltbl", "w");
	if (!fp)
		return;
second_pass:
	for (i = 0; i < HASHSIZE; i++) {
		mal_tbl = mal_hash_tbl[i];
		if (!mal_tbl)
			continue;
		nent = mal_nent[i];
		if (!nent)
			continue;
		if (first)
			total_nent += nent;
		tbl_size = mal_tbl_size[i];
		for (j = 0; j < tbl_size; j++) {
			if (!mal_tbl[j].ptr)
				continue;
			if (first)
				total_size += mal_tbl[j].size;
			else
				fprintf(fp, "%p  %u  from %p\n",
					mal_tbl[j].ptr, mal_tbl[j].size,
					mal_tbl[j].return_addr);
		}
	}
	if (first) {
		first = 0;
		fprintf(fp, "    total entries: %u\n", total_nent);
		fprintf(fp, "       total size: %u\n", total_size);
		fprintf(fp, "  total allocated: %u\n", total_allocated);
		fprintf(fp, "total reallocated: %u\n", total_reallocated);
		fprintf(fp, "      total freed: %u\n", total_freed);
		goto second_pass;
	}
	fflush(fp);
	fclose(fp);
}
#else
#define DNEW(ptr,size,re,reta)
#define DDEL(ptr)
#endif

static void
xdie (size_t size)
{
	hxd_log("xmalloc: could not allocate %lu bytes", (unsigned long)size);
#if XMALLOC_DEBUG
	DTBLWRITE();
#endif
	abort();
	exit(111);
}

void *
xmalloc (size_t size)
{
	void *p;

	if (!(p = malloc(size)))
		xdie(size);
	DNEW(p, size, 0, __builtin_return_address(0));

	return p;
}

void *
xrealloc (void *ptr, size_t size)
{
	void *p;

	p = ptr ? realloc(ptr, size) : malloc(size);
	if (!p)
		xdie(size);
	DNEW(p, size, ptr, __builtin_return_address(0));

	return p;
}

void
xfree (void *ptr)
{
	if (ptr)
		free(ptr);
	DDEL(ptr);
}

char *
xstrdup (const char *str)
{
	char *p;
	size_t size;

	if (!str)
		return 0;
	size = strlen(str) + 1;
	if (!(p = malloc(size)))
		xdie(size);
	DNEW(p, size, 0, __builtin_return_address(0));
	strcpy(p, str);

	return p;
}
