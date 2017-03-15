#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sha.h"

typedef unsigned char byte;

#define HASH_ALL	0
#define HASH_START	1
#define HASH_CONTINUE	2
#define HASH_END	3

/* Test the SHA output against the test vectors given in FIPS 180-1 */

void sha1HashBuffer (void *hashInfo, byte *outBuffer, byte *inBuffer,
		     int length, int hashState);

char lots_of_a[1000000];

static const struct {
	const char *data;						/* Data to hash */
	const int length;						/* Length of data */
	const byte digest[ SHA_DIGESTSIZE ];	/* Digest of data */
	} digestValues[] = {
	{ "", 0,
	  { 0xDA, 0x39, 0xA3, 0xEE, 0x5E, 0x6B, 0x4B, 0x0D,
		0x32, 0x55, 0xBF, 0xEF, 0x95, 0x60, 0x18, 0x90,
		0xAF, 0xD8, 0x07, 0x09 } },
	{ "abc", 3,
	  { 0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A,
		0xBA, 0x3E, 0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C,
		0x9C, 0xD0, 0xD8, 0x9D } },
	{ "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
	  { 0x84, 0x98, 0x3E, 0x44, 0x1C, 0x3B, 0xD2, 0x6E,
		0xBA, 0xAE, 0x4A, 0xA1, 0xF9, 0x51, 0x29, 0xE5,
		0xE5, 0x46, 0x70, 0xF1 } },
	{ lots_of_a, 1000000,
	  { 0x34, 0xAA, 0x97, 0x3C, 0xD4, 0xC4, 0xDA, 0xA4,
		0xF6, 0x1E, 0xEB, 0x2B, 0xDB, 0xAD, 0x27, 0x31,
		0x65, 0x34, 0x01, 0x6F } },
	{ NULL, 0, { 0 } }
};

void
sha1SelfTest (void)
{
	byte digest[SHA_DIGESTSIZE];
	int i;

	memset(lots_of_a, 'a', 1000000);
	/* Test SHA against values given in FIPS 180-1 */
	for (i = 0; digestValues[i].data != NULL; i++) {
		sha1HashBuffer(NULL, digest, (byte *)digestValues[i].data,
					   digestValues[i].length, HASH_ALL);
		printf("%20.20s\n", digest);
		printf("%20.20s\n", digestValues[i].digest);
		if (memcmp(digest, digestValues[i].digest, SHA_DIGESTSIZE))
			printf("failed %d\n", i);
	}
}

/* Internal API: Hash a single block of memory without the overhead of
   creating an encryption context.  This always uses SHA1 */

void
sha1HashBuffer (void *hashInfo, byte *outBuffer, byte *inBuffer,
		int length, int hashState)
{
	struct sha_ctx *shaInfo = (struct sha_ctx *)hashInfo, shaInfoBuffer;

	/* If the user has left it up to us to allocate the hash context buffer,
	   use the internal buffer */
	if (shaInfo == NULL)
		shaInfo = &shaInfoBuffer;

	if (hashState == HASH_ALL) {
		sha_init(shaInfo);
		sha_update(shaInfo, inBuffer, length);
		sha_final(outBuffer, shaInfo);
	} else {
		switch (hashState) {
			case HASH_START:
				sha_init(shaInfo);
				/* Drop through */
			case HASH_CONTINUE:
				sha_update(shaInfo, inBuffer, length);
				break;
			case HASH_END:
				sha_update(shaInfo, inBuffer, length);
				sha_final(outBuffer, shaInfo);
		}
	}
}

int
main ()
{
	sha1SelfTest();
	return 0;
}
