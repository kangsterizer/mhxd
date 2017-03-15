#include <stdio.h>
#include "sha.h"

static void
hash2str (char *str, u_int8_t *hash, int len)
{
	int i, c;

	for (i = 0; i < len; i++) {
		c = hash[i] >> 4;
		c = c > 9 ? c - 0xa + 'a' : c + '0';
		str[i*2 + 0] = c;
		c = hash[i] & 0xf;
		c = c > 9 ? c - 0xa + 'a' : c + '0';
		str[i*2 + 1] = c;
	}
}

void
hmac_sha (u_int8_t *md, u_int8_t *key, u_int32_t keylen, u_int8_t *text, u_int32_t textlen)
{
	struct sha_ctx ctx;
	u_int8_t k_ipad[64], k_opad[64];
	unsigned int i;

	if (keylen > 64) {
		sha_init(&ctx);
		sha_update(&ctx, key, keylen);
		sha_final(md, &ctx);
		key = md;
		keylen = 20;
	}

	memcpy(k_ipad, key, keylen);
	memcpy(k_opad, key, keylen);
	memset(k_ipad+keylen, 0, 64 - keylen);
	memset(k_opad+keylen, 0, 64 - keylen);

	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	sha_init(&ctx);
	sha_update(&ctx, k_ipad, 64);
	sha_update(&ctx, text, textlen);
	sha_final(md, &ctx);

	sha_init(&ctx);
	sha_update(&ctx, k_opad, 64);
	sha_update(&ctx, md, 20);
	sha_final(md, &ctx);
}

int
main (int argc, char **argv)
{
	char md[20], str[40], *password = argv[1] ? argv[1] : "apassword";
	char *data = argc > 2 ? argv[2] : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	hmac_sha(md, password, strlen(password), data, strlen(data));
	hash2str(str, md, 20);
	printf("%40.40s\n", str);

	return 0;
}
