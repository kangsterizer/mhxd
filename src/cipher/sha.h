/* 
 * $Id: sha.h,v 1.1.1.1 2003/06/03 07:19:44 ror Exp $
 */

#include <sys/types.h>

/* The SHA block size and message digest sizes, in bytes */

#define SHA_DATASIZE    64
#define SHA_DATALEN     16
#define SHA_DIGESTSIZE  20
#define SHA_DIGESTLEN    5
/* The structure for storing SHA info */

struct sha_ctx {
  u_int32_t digest[SHA_DIGESTLEN];  /* Message digest */
  u_int32_t count_l, count_h;       /* 64-bit block count */
  u_int8_t block[SHA_DATASIZE];     /* SHA data buffer */
  int index;                        /* index into buffer */
};

extern void sha_init(struct sha_ctx *ctx);
extern void sha_update(struct sha_ctx *ctx, u_int8_t *buffer, u_int32_t len);
extern void sha_final(u_int8_t *s, struct sha_ctx *ctx);
extern void sha_digest(struct sha_ctx *ctx, u_int8_t *s);
extern void sha_copy(struct sha_ctx *dest, struct sha_ctx *src);
extern int sha_fd(int fd, size_t maxlen, u_int8_t *s);
