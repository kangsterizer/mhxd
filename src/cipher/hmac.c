#include "hxd.h"
#include "sha.h"
#include "md5.h"
#include <string.h>

u_int16_t
hmac_xxx (u_int8_t *md, u_int8_t *key, u_int32_t keylen, u_int8_t *text, u_int32_t textlen, u_int8_t *macalg)
{
	u_int8_t mdkey[20];
	u_int8_t k_ipad[64], k_opad[64];
	unsigned int i;

	if (!strcmp(macalg, "SHA1")) {
		struct sha_ctx ctx;

		sha_init(&ctx);
		sha_update(&ctx, key, keylen);
		sha_update(&ctx, text, textlen);
		sha_final(md, &ctx);

		return 20;
	} else if (!strcmp(macalg, "MD5")) {
		struct md5_ctx ctx;

		md5_init_ctx(&ctx);
		md5_process_bytes(key, keylen, &ctx);
		md5_process_bytes(text, textlen, &ctx);
		md5_finish_ctx(&ctx, md);

		return 16;
	}

	if (keylen > 64) {
		if (!strcmp(macalg, "HMAC-SHA1")) {
			struct sha_ctx ctx;

			sha_init(&ctx);
			sha_update(&ctx, key, keylen);
			sha_final(mdkey, &ctx);
			keylen = 20;
		} else if (!strcmp(macalg, "HMAC-MD5")) {
			struct md5_ctx ctx;

			md5_init_ctx(&ctx);
			md5_process_bytes(key, keylen, &ctx);
			md5_finish_ctx(&ctx, mdkey);
			keylen = 16;
		} else if (!strncmp(macalg, "HMAC-HAVAL", 10)) {
			return 0;
		} else {
			return 0;
		}
		key = mdkey;
	}

	memcpy(k_ipad, key, keylen);
	memcpy(k_opad, key, keylen);
	memset(k_ipad+keylen, 0, 64 - keylen);
	memset(k_opad+keylen, 0, 64 - keylen);

	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	if (!strcmp(macalg, "HMAC-SHA1")) {
		struct sha_ctx ctx;

		sha_init(&ctx);
		sha_update(&ctx, k_ipad, 64);
		sha_update(&ctx, text, textlen);
		sha_final(md, &ctx);

		sha_init(&ctx);
		sha_update(&ctx, k_opad, 64);
		sha_update(&ctx, md, 20);
		sha_final(md, &ctx);

		return 20;
	} else if (!strcmp(macalg, "HMAC-MD5")) {
		struct md5_ctx ctx;

		md5_init_ctx(&ctx);
		md5_process_bytes(k_ipad, 64, &ctx);
		md5_process_bytes(text, textlen, &ctx);
		md5_finish_ctx(&ctx, md);

		md5_init_ctx(&ctx);
		md5_process_bytes(k_opad, 64, &ctx);
		md5_process_bytes(md, 16, &ctx);
		md5_finish_ctx(&ctx, md);

		return 16;
	} else if (!strncmp(macalg, "HMAC-HAVAL", 10)) {
		return 0;
	}

	return 0;
}
