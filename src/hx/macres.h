#ifndef _MACRES_H
#define _MACRES_H

#include <sys/types.h>
#include <unistd.h>
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "berw.h"

#ifndef PACKED
#ifdef __GNUC__
#define PACKED __attribute__((__packed__))
#else
#define PACKED
#endif
#endif

struct macres_res;

struct macres_res_ref_list {
	int16_t resid PACKED;
	u_int16_t res_map_name_list_name_off PACKED;
	u_int8_t res_attrs PACKED;
	u_int32_t res_data_off PACKED;
	struct macres_res *mr PACKED;
};

typedef struct macres_res_ref_list macres_res_ref_list;

struct macres_res_type_list {
	u_int32_t res_type PACKED;
	u_int32_t num_res_of_type PACKED;
	u_int16_t res_ref_list_off PACKED;
	macres_res_ref_list *cached_ref_list PACKED;
};

typedef struct macres_res_type_list macres_res_type_list;

struct macres_res_map {
	u_int16_t res_type_list_off PACKED;
	u_int16_t res_name_list_off PACKED;
	u_int32_t num_res_types PACKED;
	macres_res_type_list *res_type_list PACKED;
};

typedef struct macres_res_map macres_res_map;

struct macres_file {
	int fd PACKED;
	u_int32_t first_res_off PACKED;
	u_int32_t res_data_len PACKED;
	u_int32_t res_map_off PACKED;
	u_int32_t res_map_len PACKED;
	macres_res_map res_map PACKED;
};

typedef struct macres_file macres_file;

struct macres_res {
	u_int32_t datalen PACKED;
	u_int16_t resid PACKED;
	u_int16_t namelen PACKED;
	u_int8_t *name PACKED;
	u_int8_t data[4] PACKED;
};

typedef struct macres_res macres_res;

extern macres_file *macres_file_new (void);
extern macres_file *macres_file_open (int fd);
extern macres_res *macres_res_read (int fd);
extern u_int32_t macres_file_num_res_of_type (macres_file *mrf, u_int32_t type);
extern macres_res *macres_file_get_nth_res_of_type (macres_file *mrf, u_int32_t type, u_int32_t n);
extern macres_res *macres_file_get_resid_of_type (macres_file *mrf, u_int32_t type, int16_t resid);
extern void macres_file_print (macres_file *mrf);
extern void macres_file_delete (macres_file *mrf);
extern int macres_file_write (int fd, macres_file *mrf);
extern void macres_file_write_setup (macres_file *mrf);
extern int macres_add_resource (macres_file *mrf, u_int32_t type, int16_t resid, const u_int8_t *name, u_int8_t namelen, const u_int8_t *data, u_int32_t datalen);
extern int macres_res_write (int fd, macres_res *mr);

#endif /* _MACRES_H */
