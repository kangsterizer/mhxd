#ifndef __cipher_openssl_h
#define __cipher_openssl_h

#include "config.h"
#include <openssl/rc4.h>
#include <openssl/blowfish.h>
#if !defined(CONFIG_NO_IDEA)
#include <openssl/idea.h>
#endif

#define rc4_prepare_key(key,keylen,state)	RC4_set_key(state,keylen,key)
#define rc4_buffer(buf,len,state)		RC4(state,len,buf,buf)

typedef RC4_KEY rc4_state;

struct blowfish_state {
	BF_KEY state;
	unsigned char ivec[8];
	int num;
};

typedef struct blowfish_state blowfish_state;

#if !defined(CONFIG_NO_IDEA)
struct idea_state {
	IDEA_KEY_SCHEDULE state;
	unsigned char ivec[8];
	int num;
};

typedef struct idea_state idea_state;
#endif

#endif /* __cipher_openssl_h */
