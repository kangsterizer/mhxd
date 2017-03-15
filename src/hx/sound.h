#ifndef __hx_SOUND_H
#define __hx_SOUND_H

typedef enum { snd_news, snd_chat, snd_join, snd_part, snd_message, snd_login,
	       snd_error, snd_file_done, snd_chat_invite } snd_events;

extern void play_sound (snd_events event);

struct sound_event {
	pid_t pid;
	char *player;
	char *sound;
	char *data;
	unsigned int datalen;
	short on, beep;
};

extern struct sound_event sounds[];
extern unsigned int nsounds;

#endif
