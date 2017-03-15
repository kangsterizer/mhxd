#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define XMMS_PROTOCOL_VERSION	1

enum {
	CMD_GET_VERSION, CMD_PLAYLIST_ADD, CMD_PLAY, CMD_PAUSE, CMD_STOP,
	CMD_IS_PLAYING, CMD_IS_PAUSED, CMD_GET_PLAYLIST_POS,
	CMD_SET_PLAYLIST_POS, CMD_GET_PLAYLIST_LENGTH, CMD_PLAYLIST_CLEAR,
	CMD_GET_OUTPUT_TIME, CMD_JUMP_TO_TIME, CMD_GET_VOLUME,
	CMD_SET_VOLUME, CMD_GET_SKIN, CMD_SET_SKIN, CMD_GET_PLAYLIST_FILE,
	CMD_GET_PLAYLIST_TITLE, CMD_GET_PLAYLIST_TIME, CMD_GET_INFO,
	CMD_GET_EQ_DATA, CMD_SET_EQ_DATA, CMD_PL_WIN_TOGGLE,
	CMD_EQ_WIN_TOGGLE, CMD_SHOW_PREFS_BOX, CMD_TOGGLE_AOT,
	CMD_SHOW_ABOUT_BOX, CMD_EJECT, CMD_PLAYLIST_PREV, CMD_PLAYLIST_NEXT,
	CMD_PING, CMD_GET_BALANCE, CMD_TOGGLE_REPEAT, CMD_TOGGLE_SHUFFLE,
	CMD_MAIN_WIN_TOGGLE, CMD_PLAYLIST_ADD_URL_STRING,
	CMD_IS_EQ_WIN, CMD_IS_PL_WIN, CMD_IS_MAIN_WIN
};

struct ClientPktHeader {
	u_int16_t version;
	u_int16_t command;
	u_int32_t data_length;
};

struct ServerPktHeader {
	u_int16_t version;
	u_int32_t data_length;
};

extern char hx_user[64];

int
xmms_connect_to_session (int session)
{
	int fd;
	uid_t stored_uid, euid;
	struct sockaddr_un saddr;
	char *tmpdir, *user;

	tmpdir = getenv("TMPDIR");
	if (!tmpdir)
		tmpdir = "/tmp";
	user = hx_user;
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) != -1) {
		saddr.sun_family = AF_UNIX;
		stored_uid = getuid();
		euid = geteuid();
		setuid(euid);
		sprintf(saddr.sun_path, "%s/xmms_%s.%d", tmpdir, user, session);
		setreuid(stored_uid, euid);
		if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) != -1)
			return fd;
	}
	close(fd);

	return -1;
}

static char *
remote_read_packet (int fd, struct ServerPktHeader *pkt_hdr)
{
	char *data = NULL;

	if (read(fd, pkt_hdr, sizeof(struct ServerPktHeader)) == sizeof(struct ServerPktHeader)) {
		if (pkt_hdr->data_length) {
			data = malloc(pkt_hdr->data_length);
			read(fd, data, pkt_hdr->data_length);
		}
	}

	return data;
}

static void
remote_read_ack (int fd)
{
	char *data;
	struct ServerPktHeader pkt_hdr;

	data = remote_read_packet(fd, &pkt_hdr);
	if (data)
		free(data);

}

static void
remote_send_packet (int fd, u_int32_t command, void *data, unsigned int data_length)
{
	struct ClientPktHeader pkt_hdr;

	pkt_hdr.version = XMMS_PROTOCOL_VERSION;
	pkt_hdr.command = command;
	pkt_hdr.data_length = data_length;
	write(fd, &pkt_hdr, sizeof(struct ClientPktHeader));
	if (data_length && data)
		write(fd, data, data_length);
}

static u_int32_t
remote_get_int (int session, int cmd)
{
	struct ServerPktHeader pkt_hdr;
	char *data;
	int fd, ret = 0;

	if ((fd = xmms_connect_to_session(session)) == -1)
		return ret;
	remote_send_packet(fd, cmd, NULL, 0);
	data = remote_read_packet(fd, &pkt_hdr);
	if (data) {
		ret = *((int *) data);
		free(data);
	}
	remote_read_ack(fd);
	close(fd);

	return ret;
}

static char *
remote_get_string_pos(int session, int cmd, u_int32_t pos)
{
	struct ServerPktHeader pkt_hdr;
	char *data;
	int fd;

	if ((fd = xmms_connect_to_session(session)) == -1)
		return NULL;
	remote_send_packet(fd, cmd, &pos, sizeof(u_int32_t));
	data = remote_read_packet(fd, &pkt_hdr);
	remote_read_ack(fd);
	close(fd);

	return data;
}

void
xmms_remote_get_info (int session, int *rate, int *freq, int *nch)
{
	struct ServerPktHeader pkt_hdr;
	int fd;
	char *data;

	if ((fd = xmms_connect_to_session(session)) == -1)
		return;
	remote_send_packet(fd, CMD_GET_INFO, NULL, 0);
	data = remote_read_packet(fd, &pkt_hdr);
	if (data) {
		*rate = ((u_int32_t *)data)[0];
		*freq = ((u_int32_t *)data)[1];
		*nch = ((u_int32_t *)data)[2];
		free(data);
	}
	remote_read_ack(fd);
	close(fd);
}

int
xmms_remote_get_playlist_pos (int session)
{
	return remote_get_int(session, CMD_GET_PLAYLIST_POS);
}

char *
xmms_remote_get_playlist_title (int session, int pos)
{
	return remote_get_string_pos(session, CMD_GET_PLAYLIST_TITLE, pos);
}

#if TEST
int
main (int argc, char **argv)
{
	char *string;
	char buf[1024], *channels;
	int rate, freq, nch;

	string = xmms_remote_get_playlist_title(0, xmms_remote_get_playlist_pos(0));
	if (!string) {
		printf("%s: failed to get XMMS playlist\n", argv[0]);
		return 1;
	}
	xmms_remote_get_info(0, &rate, &freq, &nch);
	rate = rate / 1000;
	if (nch == 2)
		channels = "stereo";
	else
		channels = "mono";

	snprintf(buf, sizeof(buf), "is listening to: %s (%d kbit/s %d Hz %s)",
		 string, rate, freq, channels);
	puts(buf);

	return 0;
}
#endif
