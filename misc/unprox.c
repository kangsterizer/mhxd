#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

int
main (int argc, char **argv)
{
	struct sockaddr_un saddr;
	int siz = sizeof(saddr);
	int s, r;
	unsigned char buf[4096];
	fd_set rfds;
//FILE *fp=fopen("/tmp/perr","a");

	saddr.sun_family = AF_UNIX;
	strcpy(saddr.sun_path, "/tmp/..../wtf");
	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		perror("socket");
		exit(1);
	}
	r = connect(s, (struct sockaddr *)&saddr, siz);
	if (r < 0) {
		perror("socket");
		exit(1);
	}

	for (;;) {
		FD_SET(0, &rfds);
		FD_SET(s, &rfds);
		r = select(s+1, &rfds, 0, 0, 0);
		if (r < 0)
			break;
		if (FD_ISSET(s, &rfds)) {
			r = read(s, buf, sizeof(buf));
//fprintf(fp,"read(%d)==%d: %s\n",s,r, strerror(errno));
			if (r <= 0)
				break;
			r = write(1, buf, r);
//fprintf(fp,"write(1)==%d: %s\n",r, strerror(errno));
			if (r <= 0)
				break;
		}
		if (FD_ISSET(0, &rfds)) {
			r = read(0, buf, sizeof(buf));
//fprintf(fp,"read(0)==%d: %s\n",r, strerror(errno));
			if (r <= 0)
				break;
			r = write(s, buf, r);
//fprintf(fp,"write(%d)==%d: %s\n",s,r,strerror(errno));
			if (r <= 0)
				break;
		}
	}
//fclose(fp);

	return 0;
}
