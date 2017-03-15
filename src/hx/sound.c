#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "hx.h"
#include "hxd.h"
#include "macres.h"
#include "xmalloc.h"
#include "sound.h"

struct sound_event sounds[] = { 
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0 },
};

unsigned int nsounds = 9;
/* Does not compile with tcc: (sizeof(sounds) / sizeof(struct sound_event)); */

struct snd_resource_header {
	u_int16_t format_type;
	u_int16_t num_data_types;
	u_int16_t sampled_sound_data;
	u_int32_t init_options;
};

struct snd_header {
	u_int32_t sample_ptr;
	u_int32_t length;
	u_int32_t sample_rate;
	u_int32_t loop_start;
	u_int32_t loop_end;
	u_int8_t encode; /* 0xfe -> compressed ; 0xff -> extended */
	u_int8_t base_freq;
	u_int8_t sample_data[1];
};

struct extended_snd_header {
	u_int32_t sample_ptr;
	u_int32_t num_channels;
	u_int32_t sample_rate;
	u_int32_t loop_start;
	u_int32_t loop_end;
	u_int8_t encode;
	u_int8_t base_freq;
	u_int32_t num_frames;
	u_int32_t aiff_sample_rate[2];
	u_int32_t marker_chunk;
	u_int32_t instrument_chunks;
	u_int32_t aes_recording;
	u_int16_t sample_size;
	u_int16_t future_use[4];
	u_int8_t sample_data[0];
};

struct compressed_snd_header {
	u_int32_t sample_ptr;
	u_int32_t length;
	u_int32_t sample_rate;
	u_int32_t loopstart;
	u_int32_t loopend;
	u_int8_t encode;
	u_int8_t base_freq;
	u_int32_t num_frames;
	u_int32_t aiff_sample_rate[2];
	u_int32_t marker_chunk;
	u_int32_t format;
	u_int32_t future_use_2;
	u_int32_t state_vars;
	u_int32_t left_over_block_ptr;
	u_int16_t compression_id;
	u_int16_t packet_size;
	u_int16_t synth_id;
	u_int16_t sample_size;
	u_int8_t sample_data[0];
};

/*
struct snd_res {
	struct snd_resource_header srh;
	u_int16_t num_sound_commands;
	u_int16_t sound_commands[...];
	u_int16_t param1; = 0
	u_int32_t param2; = offset to sound header (20 bytes)
	struct snd_header ssh;
};
*/

int sound_on = 0;
char *sound_stdin_filename = "-";
char default_player[MAXPATHLEN] =
#ifdef DEFAULT_SND_PLAYER
	DEFAULT_SND_PLAYER;
#else
	"play";
#endif

#define TYPE_snd	0x736e6420

void
load_sndset (struct htlc_conn *htlc, struct hx_chat *chat, const char *filename)
{
	macres_file *mrf;
	macres_res *mr;
	u_int8_t *buf;
	size_t buflen;
	struct snd_header *ssh;
	struct extended_snd_header *essh;
	u_int8_t *sample_data;
	u_int16_t nsc;
	u_int32_t sample_rate;
	u_int32_t param2;
	u_int32_t nchannels, nframes;
	u_int16_t sample_size;
	u_int32_t datalen;
	int16_t id;
	int si;
	int fd;
	char command[256];
	char path[MAXPATHLEN];

	expand_tilde(path, filename);
	fd = SYS_open(path, O_RDONLY, 0);
	if (fd < 0) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "load_sndset; %s: %s", path, strerror(errno));
		return;
	}
	mrf = macres_file_open(fd);
	if (!mrf) {
		hx_printf_prefix(htlc, chat, INFOPREFIX, "load_sndset; %s: %s", path, strerror(errno));
		return;
	}
	for (id = 128; id <= 136; id++) {
		si = id - 128;
		mr = macres_file_get_resid_of_type(mrf, TYPE_snd, id);
		if (!mr) {
			hx_printf_prefix(htlc, chat, INFOPREFIX, "load_sndset: no resid %u in %s\n", id, path);
			continue;
		}
		buf = mr->data;
		buflen = mr->datalen;
		nsc = ntohs(*(u_int16_t *)(buf+10));
		param2 = ntohl(*(u_int32_t *)(buf+12+2*nsc+2));
		ssh = (struct snd_header *)(buf+param2);
		sample_rate = ntohl(ssh->sample_rate) >> 16;
		if (ssh->encode == 0xff) {
			essh = (struct extended_snd_header *)ssh;
			nchannels = ntohl(essh->num_channels);
			nframes = ntohl(essh->num_frames) >> 16;
			sample_size = ntohs(essh->sample_size);
			sample_data = essh->sample_data;
			if (sample_size == 16) {
				u_int16_t *p;
				for (p = (u_int16_t *)sample_data; p < (u_int16_t *)(buf+buflen); p++)
					*p = ntohs(*p);
			}
		} else {
			nframes = ntohl(ssh->length);
			nchannels = 1;
			sample_size = 8;
			sample_data = ssh->sample_data;
		}
		snprintf(command, sizeof(command),
			 "play -t raw -f %c -%c -c %u -r %u",
			 sample_size == 8 ? 'u' : 's', sample_size == 16 ? 'w' : 'b',
			 nchannels, sample_rate);
		sounds[si].player = xstrdup(command);
		if (sounds[si].sound && sounds[si].sound != sound_stdin_filename)
			xfree(sounds[si].sound);
		sounds[si].sound = sound_stdin_filename;
		if (sounds[si].data)
			xfree(sounds[si].data);
		datalen = nframes*nchannels*sample_size>>3;
		sounds[si].data = xmalloc(datalen);
		sounds[si].datalen = datalen;
		memcpy(sounds[si].data, sample_data, datalen);
	}
	close(fd);
	macres_file_delete(mrf);
}

void
play_sound (snd_events event)
{
	char buff[2 * MAXPATHLEN + 20];
	pid_t pid;
	int pfds[2], pfds_ok = 0;

	if (!sound_on || !sounds[event].on)
		return;
#if !defined(__WIN32__)
	if (sounds[event].sound) {
		sprintf(buff, "%s %s &>/dev/null", 
			sounds[event].player ? sounds[event].player : default_player, 
			sounds[event].sound);
		if (sounds[event].data) {
			if (!pipe(pfds))
				pfds_ok = 1;
		}
		pid = fork();
		if (pid == -1)
			return;
		sounds[event].pid = pid;
		if (pid == 0) {
			char *argv[4];

			if (pfds_ok) {
				if (pfds[0] != 0)
					dup2(pfds[0], 0);
				close(pfds[1]);
			}
			argv[0] = "sh";
			argv[1] = "-c";
			argv[2] = buff;
			argv[3] = 0;
			execve("/bin/sh", argv, hxd_environ);
			_exit(127);
		}
		if (pfds_ok) {
			write(pfds[1], sounds[event].data, sounds[event].datalen);
			close(pfds[1]);
			close(pfds[0]);
		}
	} else 
#endif
		if (sounds[event].beep)
			write(1, "\a", 1);
}
