#include <string.h>

char *
basename (char *path)
{
	size_t len;

	len = strlen(path);
	while (len--) {
		if (path[len] == '/')
			return path+len+1;
	}

	return path;
}
