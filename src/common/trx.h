#ifndef __trx_h
#define __trx_h

struct track_field {
	u_int8_t type;
	u_int8_t len;
	u_int8_t data[1];
};

struct tracker_udp_packet {
	u_int16_t version;
	u_int8_t uidlen;
	u_int8_t uniqueid[16];
	u_int8_t ntf;
	struct track_field tf[1];
};

#define TRXL_MAGIC	"TRXL\0\1"
#define TRXL_MAGIC_LEN	6

#define TRXR_MAGIC	"TRXR\0\1"
#define TRXR_MAGIC_LEN	6

#define TRXR_HDR_SET_STATIC	0x5401
#define TRXR_HDR_SET_DYNAMIC	0x5402

#endif /* __trx_h */
