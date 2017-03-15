#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#if !defined(MAXPATHLEN)
#define MAXPATHLEN 4096
#endif

char *
realpath (const char *pathname, char *result)
{
	struct stat sbuf;
	char curpath[MAXPATHLEN], workpath[MAXPATHLEN], linkpath[MAXPATHLEN], namebuf[MAXPATHLEN],
	     *where, *ptr, *last;
	int len;

	if (!result)
		return 0;

	if (!pathname) {
		*result = 0;
		return 0;
	}

	len = strlen(pathname);
	if (len >= MAXPATHLEN) {
		*result = 0;
		return 0;
	}
	memcpy(curpath, pathname, len);
	curpath[len] = 0;

	if (*pathname != '/') {
		if (!getcwd(workpath, MAXPATHLEN)) {
			result[0] = '.'; result[1] = 0;
			return 0;
		}
	} else
		*workpath = 0;

	/* curpath is the path we're still resolving      */
	/* linkpath is the path a symbolic link points to */
	/* workpath is the path we've resolved            */
loop:
	where = curpath;
	while (*where != '\0') {
		if (!strcmp(where, ".")) {
			where++;
			continue;
		}
		/* deal with "./" */
		if (!strncmp(where, "./", 2)) {
			where += 2;
			continue;
		}
		/* deal with "../" */
		if (!strncmp(where, "../", 3)) {
			where += 3;
			ptr = last = workpath;
			while (*ptr != '\0') {
				if (*ptr == '/')
					last = ptr;
				ptr++;
			}
			*last = '\0';
			continue;
		}
		ptr = strchr(where, '/');
		if (!ptr)
			ptr = where + strlen(where) - 1;
		else
			*ptr = '\0';

		strcpy(namebuf, workpath);
		for (last = namebuf; *last; last++);
		if ((last == namebuf) || (*--last != '/'))
			strcat(namebuf, "/");
		strcat(namebuf, where);

		where = ++ptr;
#if !defined(__WIN32__)
		if (lstat(namebuf, &sbuf) == -1) {
			strcpy(result, namebuf);
			return 0;
		}
		/* was IFLNK */
		if ((sbuf.st_mode & S_IFMT) == S_IFLNK) {
			len = readlink(namebuf, linkpath, MAXPATHLEN);
			if (len == 0) {
				strcpy(result, namebuf);
				return 0;
			}
			*(linkpath + len) = '\0';   /* readlink doesn't null-terminate
					             * result */
			if (*linkpath == '/')
				*workpath = '\0';
			if (*where) {
				strcat(linkpath, "/");
				strcat(linkpath, where);
			}
			strcpy(curpath, linkpath);
			goto loop;
		}
#else
		if (stat(namebuf, &sbuf) == -1) {
			strcpy(result, namebuf);
			return 0;
		}
#endif
		if ((sbuf.st_mode & S_IFDIR) == S_IFDIR) {
			strcpy(workpath, namebuf);
			continue;
		}
		if (*where) {
			strcpy(result, namebuf);
			return 0;      /* path/notadir/morepath */
		} else
			strcpy(workpath, namebuf);
	}

	strcpy(result, workpath);

	return result;
}
