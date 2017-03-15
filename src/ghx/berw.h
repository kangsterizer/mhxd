#ifndef __berw_h
#define __berw_h

static inline u_int32_t
be_read32 (int fd)
{
	u_int32_t word;
 	u_int8_t buf[4];

	read(fd, buf, 4);
	word = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

	return word;
}

static inline u_int32_t
be_read24 (int fd)
{
	u_int32_t word;
 	u_int8_t buf[3];
 
	read(fd, buf, 3);
	word = (buf[0] << 16) | (buf[1] << 8) | buf[2];

	if (buf[0] > 0x7f)
		word |= (0xff << 24);

	return word;
}

static inline u_int16_t
be_read16 (int fd)
{
	u_int16_t word;
	u_int8_t buf[2];

	read(fd, buf, 2);
	word = (buf[0] << 8) | buf[1];

	return word;
}

static inline ssize_t
be_write16 (int fd, u_int16_t word)
{
	u_int8_t buf[2];

	buf[1] = word & 0xff;
	buf[0] = (word & 0xff00) >> 8;
	return write(fd, buf, 2);
}

static inline ssize_t
be_write24 (int fd, u_int32_t word)
{
 	u_int8_t buf[3];
 
	buf[2] = word & 0xff;
	buf[1] = (word & 0xff00) >> 8;
	buf[0] = (word & 0xff0000) >> 16;
	return write(fd, buf, 3);
}

static inline ssize_t
be_write32 (int fd, u_int32_t word)
{
	u_int8_t buf[4];

	buf[3] = word & 0xff;
	buf[2] = (word & 0xff00) >> 8;
	buf[1] = (word & 0xff0000) >> 16;
	buf[0] = (word & 0xff000000) >> 24;
	return write(fd, buf, 4);
}

#endif
