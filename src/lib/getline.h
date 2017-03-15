#ifndef __getline_h
#define __getline_h

struct getline {
	int fd;
	int pos, len, size;
	char buf[4]; /* really bigger */
};

typedef struct getline getline_t;

extern char *getline_line (getline_t *gl, int *linelenp);
extern getline_t *getline_open (int fd);
extern void getline_close (getline_t *gl);

#endif
