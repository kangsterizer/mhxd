#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#if defined(__WIN32__)
#include <winsock.h>
#else
#include <netinet/in.h>
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "macres.h"

macres_res_type_list *
macres_res_type_list_get (macres_res_type_list *typelist, u_int32_t ntypes, u_int32_t type)
{
	u_int32_t i;

	for (i = 0; i < ntypes; i++)
		if (typelist[i].res_type == type)
			return &typelist[i];

	return 0;
}

u_int32_t
macres_file_num_res_of_type (macres_file *mrf, u_int32_t type)
{
	macres_res_type_list *rtl;

	rtl = macres_res_type_list_get(mrf->res_map.res_type_list, mrf->res_map.num_res_types, type);
	if (rtl)
		return rtl->num_res_of_type;

	return 0;
}

macres_res_type_list *
macres_file_read_res_type_list (macres_file *mrf)
{
	int fd = mrf->fd;
	u_int32_t i;
	macres_res_type_list *rtl;

	/* Seek past the 2B type list count field */
	if (lseek(fd, mrf->res_map_off + mrf->res_map.res_type_list_off + 2, SEEK_SET) == -1)
		return 0;

	rtl = malloc(sizeof(macres_res_type_list) * mrf->res_map.num_res_types);
	if (!rtl)
		return 0;
	for (i = 0; i < mrf->res_map.num_res_types; i++) {
		rtl[i].res_type = be_read32(fd);
		rtl[i].num_res_of_type = be_read16(fd) + 1;
		rtl[i].res_ref_list_off = be_read16(fd);
		rtl[i].cached_ref_list = 0;
	}

	return rtl;
}

int
macres_file_read_res_map (macres_file *mrf)
{
	int fd = mrf->fd;

	/* Skip to the resource map */
	if (lseek(fd, mrf->res_map_off, SEEK_SET) == -1)
		return -1;

	/* Jump to the resource type list offset field */
	if (lseek(fd, 24, SEEK_CUR) == -1)
		return -1;
 
	mrf->res_map.res_type_list_off = be_read16(fd);
	mrf->res_map.res_name_list_off = be_read16(fd);
	mrf->res_map.num_res_types = be_read16(fd) + 1;

	mrf->res_map.res_type_list = macres_file_read_res_type_list(mrf);

	return 0;
}

static macres_res_type_list *
macres_res_type_list_new (macres_file *mrf, u_int32_t type)
{
	macres_res_type_list *rtl;
	u_int32_t i;

	i = mrf->res_map.num_res_types;
	mrf->res_map.num_res_types++;
	rtl = mrf->res_map.res_type_list;
	rtl = realloc(rtl, sizeof(macres_res_type_list) * mrf->res_map.num_res_types);
	if (!rtl)
		return 0;
	mrf->res_map.res_type_list = rtl;
	rtl[i].res_type = type;
	rtl[i].num_res_of_type = 0;
	rtl[i].res_ref_list_off = 0;
	rtl[i].cached_ref_list = 0;

	return &rtl[i];
}

macres_res_ref_list *
macres_file_read_res_ref_list (macres_file *mrf, u_int32_t type);

macres_res_ref_list *
macres_res_ref_list_new (macres_file *mrf, u_int32_t type, macres_res_type_list *rtl)
{
	macres_res_ref_list *rl;

	rl = rtl->cached_ref_list;
	if (!rl)
		rl = rtl->cached_ref_list = macres_file_read_res_ref_list(mrf, type);
	rl = realloc(rl, rtl->num_res_of_type * sizeof(macres_res_ref_list));
	if (!rl)
		return 0;
	if (!rtl->cached_ref_list)
		memset(rl, 0, rtl->num_res_of_type*sizeof(macres_res_ref_list));
	rtl->cached_ref_list = rl;

	return rl;
}

macres_res *
macres_res_new (const u_int8_t *name, u_int8_t namelen, const u_int8_t *data, u_int32_t len)
{
	macres_res *mr;

	mr = malloc(sizeof(macres_res) + len);
	if (!mr)
		return 0;
	mr->namelen = namelen;
	if (namelen) {
		mr->name = malloc(namelen+1);
		if (mr->name) {
			memcpy(mr->name, name, namelen);
			mr->name[namelen] = 0;
		} else
			mr->name = 0;
	} else {
		mr->name = 0;
	}
	mr->datalen = len;
	memcpy(mr->data, data, len);

	return mr;
}

int
macres_add_resource (macres_file *mrf, u_int32_t type, int16_t resid,
		     const u_int8_t *name, u_int8_t namelen, const u_int8_t *data, u_int32_t len)
{
	macres_res_type_list *rtl;
	macres_res_ref_list *rl;
	u_int32_t i;

	rtl = macres_res_type_list_get(mrf->res_map.res_type_list, mrf->res_map.num_res_types, type);
	if (!rtl)
		rtl = macres_res_type_list_new(mrf, type);
	i = rtl->num_res_of_type;
	rtl->num_res_of_type++;
	rl = macres_res_ref_list_new(mrf, type, rtl);
	if (!rl)
		return -1;
	rl[i].resid = resid;
	rl[i].res_map_name_list_name_off = 0xffff;
	rl[i].res_attrs = 0;
	rl[i].res_data_off = 0;
	rl[i].mr = macres_res_new(name, namelen, data, len);

	return 0;
}

macres_file *
macres_file_new (void)
{
	macres_file *mrf;

	mrf = malloc(sizeof(macres_file));
	if (!mrf)
		return 0;

	memset(mrf, 0, sizeof(macres_file));

	return mrf;
}

void
macres_file_print (macres_file *mrf)
{
	u_int32_t i;
	u_int8_t buf[4];

	printf("first_res_off: %u\n", mrf->first_res_off);
	printf("  res_map_off: %u\n", mrf->res_map_off);
	printf(" res_data_len: %u\n", mrf->res_data_len);
	printf("  res_map_len: %u\n", mrf->res_map_len);
	printf("res_type_list_off: %u\n", mrf->res_map.res_type_list_off);
	printf("res_name_list_off: %u\n", mrf->res_map.res_name_list_off);
	printf("    num_res_types: %u\n", mrf->res_map.num_res_types);
	printf("resource types:\n");
	for (i = 0; i < mrf->res_map.num_res_types; i++) {
		*((u_int32_t *)buf) = ntohl(mrf->res_map.res_type_list[i].res_type);
		printf("\t%4.4s\n", buf);
	}
}

int
macres_file_write_res_type_list (int fd, macres_file *mrf)
{
	u_int8_t buf[8];
	u_int32_t i;
	macres_res_type_list *rtl;

	/* Seek past the 2B type list count field */
	if (lseek(fd, mrf->res_map_off + mrf->res_map.res_type_list_off + 2, SEEK_SET) == -1)
		return -1;

	rtl = mrf->res_map.res_type_list;
	for (i = 0; i < mrf->res_map.num_res_types; i++) {
		*(u_int32_t *)(buf) = htonl(rtl[i].res_type);
		*(u_int16_t *)(buf+4) = htons(rtl[i].num_res_of_type - 1);
		*(u_int16_t *)(buf+6) = htons(rtl[i].res_ref_list_off);
		if (write(fd, buf, 8) != 8)
			return -1;
	}

	return 0;
}

int
macres_res_ref_list_write (int fd, macres_res_ref_list *rl)
{
	if (be_write16(fd, rl->resid) != 2)
		return -1;
	if (be_write16(fd, rl->res_map_name_list_name_off) != 2)
		return -1;
	if (write(fd, &rl->res_attrs, 1) != 1)
		return -1;
	if (be_write24(fd, rl->res_data_off) != 3)
		return -1;

	/* Skip the reserved handle field */
	if (be_write32(fd, 0) != 4)
		return -1;
	/*lseek(fd, 4, SEEK_CUR);*/

	return 0;
}

int
macres_file_write_res_ref_list (int fd, macres_file *mrf)
{
	u_int32_t i, j, rloff;
	macres_res_ref_list *rl;
	macres_res_type_list *rtl;

	rtl = mrf->res_map.res_type_list;
	for (i = 0; i < mrf->res_map.num_res_types; i++) {
		rl = rtl[i].cached_ref_list;
		if (!rl)
			continue;

		rloff = mrf->res_map_off
			+ mrf->res_map.res_type_list_off
			+ rtl[i].res_ref_list_off;
		if (lseek(fd, rloff, SEEK_SET) == -1)
			return -1;

		for (j = 0; j < rtl->num_res_of_type; j++) {
			macres_res_ref_list_write(fd, &rl[j]);
		}
	}

	return 0;
}

int
macres_file_write_res_map (int fd, macres_file *mrf)
{
	/* Skip to the resource map */
	if (lseek(fd, mrf->res_map_off, SEEK_SET) == -1)
		return -1;

	/* Jump to the resource type list offset field */
	if (lseek(fd, 24, SEEK_CUR) == -1)
		return -1;
 
	be_write16(fd, mrf->res_map.res_type_list_off);
	be_write16(fd, mrf->res_map.res_name_list_off);
	be_write16(fd, mrf->res_map.num_res_types - 1);

	macres_file_write_res_type_list(fd, mrf);

	macres_file_write_res_ref_list(fd, mrf);

	return 0;
}

int
macres_file_write_resources (int fd, macres_file *mrf)
{
	u_int32_t i, j;
	macres_res *mr = 0;
	macres_res_type_list *rtl;
	macres_res_ref_list *rl;

	rtl = mrf->res_map.res_type_list;
	for (i = 0; i < mrf->res_map.num_res_types; i++) {
		rl = rtl[i].cached_ref_list;
		if (!rl)
			continue;
		for (j = 0; j < rtl->num_res_of_type; j++) {
			mr = rl[j].mr;
			if (!mr)
				continue;
			if (lseek(fd, mrf->first_res_off + rl[j].res_data_off, SEEK_SET) == -1)
				return -1;
			macres_res_write(fd, mr);
			if (rl[j].res_map_name_list_name_off != 0xffff) {
				u_int8_t namelen = mr->namelen;
				u_int32_t off = mrf->res_map_off +
						mrf->res_map.res_name_list_off +
						rl[j].res_map_name_list_name_off;

				if (lseek(fd, off, SEEK_SET) != (off_t)off)
					return -1;
				if (write(fd, &namelen, 1) != 1)
					return -1;
				if (namelen) {
					if (mr->name) {
						if (write(fd, mr->name, namelen) != namelen)
							return -1;
					}
				}
			}
		}
	}

	return 0;
}

void
macres_file_write_resources_setup (macres_file *mrf)
{
	u_int32_t i, j, ref_list_off, name_list_off, res_data_off;
	macres_res *mr = 0;
	macres_res_type_list *rtl;
	macres_res_ref_list *rl;

	ref_list_off = 10;
	name_list_off = 0;
	res_data_off = 0;
	rtl = mrf->res_map.res_type_list;
	for (i = 0; i < mrf->res_map.num_res_types; i++) {
		rl = rtl[i].cached_ref_list;
		if (!rl)
			continue;
		rtl[i].res_ref_list_off = ref_list_off;
		for (j = 0; j < rtl[i].num_res_of_type; j++) {
			mr = rl[j].mr;
			if (!mr) {
				mr = macres_file_get_nth_res_of_type(mrf, rtl[i].res_type, j);
				if (!mr)
					continue;
			}
			rl[j].res_data_off = res_data_off;
			ref_list_off += 12;
			res_data_off += 4 + mr->datalen;
			if (mr->namelen) {
				rl[j].res_map_name_list_name_off = name_list_off;
				name_list_off += 1 + mr->namelen;
			}
		}
	}
	mrf->res_data_len = res_data_off;
	mrf->res_map_len = ref_list_off + name_list_off + 28;
	mrf->res_map.res_name_list_off = ref_list_off + 28;
}

void
macres_file_write_setup (macres_file *mrf)
{
	macres_file_write_resources_setup(mrf);

	mrf->first_res_off = 256;
	mrf->res_map_off = mrf->first_res_off + mrf->res_data_len;
	mrf->res_map.res_type_list_off = 28;
}

int
macres_file_write (int fd, macres_file *mrf)
{
	u_int8_t buf[16];

	lseek(fd, 0, SEEK_SET);

	*(u_int32_t *)(buf) = htonl(mrf->first_res_off);
	*(u_int32_t *)(buf+4) = htonl(mrf->res_map_off);
	*(u_int32_t *)(buf+8) = htonl(mrf->res_data_len);
	*(u_int32_t *)(buf+12) = htonl(mrf->res_map_len);
	if (write(fd, buf, 16) != 16)
		return -1;

	if (macres_file_write_res_map(fd, mrf) == -1)
		return -1;

	return macres_file_write_resources(fd, mrf);
}

macres_file *
macres_file_open (int fd)
{
	macres_file *mrf;

	mrf = malloc(sizeof(macres_file));
	if (!mrf)
		return 0;

	mrf->fd = fd;
 
	mrf->first_res_off = be_read32(fd);
	mrf->res_map_off = be_read32(fd);
	mrf->res_data_len = be_read32(fd);
	mrf->res_map_len = be_read32(fd);

	macres_file_read_res_map(mrf);

	return mrf;
}

void
macres_file_delete (macres_file *mrf)
{
	u_int32_t i, j;
	macres_res *mr = 0;
	macres_res_type_list *rtl;
	macres_res_ref_list *rl;

	rtl = mrf->res_map.res_type_list;
	for (i = 0; i < mrf->res_map.num_res_types; i++) {
		rl = rtl[i].cached_ref_list;
		if (!rl)
			continue;
		for (j = 0; j < rtl[i].num_res_of_type; j++) {
			mr = rl[j].mr;
			if (!mr)
				continue;
			if (mr->name)
				free(mr->name);
			free(mr);
		}
		free(rl);
	}
	if (rtl)
		free(rtl);
	free(mrf);
}

macres_res *
macres_res_read (int fd)
{
	u_int32_t len;
	macres_res *mr;

	len = be_read32(fd);
	mr = malloc(sizeof(macres_res) + len);
	if (!mr)
		return 0;
	mr->datalen = len;
	if (read(fd, mr->data, len) != (ssize_t)len) {
		free(mr);
		return 0;
	}

	return mr;
}

int
macres_res_write (int fd, macres_res *mr)
{
	if (be_write32(fd, mr->datalen) != 4)
		return -1;
	if (write(fd, mr->data, mr->datalen) != (ssize_t)mr->datalen)
		return -1;

	return 0;
}

void
macres_res_ref_list_read (int fd, macres_res_ref_list *rl)
{
	rl->resid = be_read16(fd);
	rl->res_map_name_list_name_off = be_read16(fd);
	read(fd, &rl->res_attrs, 1);
	rl->res_data_off = be_read24(fd);
	/* Skip the reserved handle field */
	lseek(fd, 4, SEEK_CUR);
	rl->mr = 0;
}

macres_res_ref_list *
macres_file_read_res_ref_list (macres_file *mrf, u_int32_t type)
{
	int fd = mrf->fd;
	u_int32_t i, found, rloff;
	macres_res_ref_list *rl;
	macres_res_type_list *rtl = 0;

	for (i = 0, found = 0; i < mrf->res_map.num_res_types && !found; i++) {
		rtl = &mrf->res_map.res_type_list[i];
		found = (rtl->res_type == type);
	}
	if (!found)
		return 0;

	rl = malloc(rtl->num_res_of_type * sizeof(macres_res_ref_list));
	if (!rl)
		return 0;

	rloff = mrf->res_map_off
		+ mrf->res_map.res_type_list_off
		+ rtl->res_ref_list_off;
	if (lseek(fd, rloff, SEEK_SET) == -1)
		return 0;

	for (i = 0; i < rtl->num_res_of_type; i++)
		macres_res_ref_list_read(fd, &rl[i]);

	return rl;
}

macres_res *
macres_file_get_nth_res_of_type (macres_file *mrf, u_int32_t type, u_int32_t n)
{
	int fd = mrf->fd;
	macres_res *mr = 0;
	macres_res_type_list *rtl;
	macres_res_ref_list *rl;

	rtl = macres_res_type_list_get(mrf->res_map.res_type_list, mrf->res_map.num_res_types, type);
	if (rtl) {
		rl = rtl->cached_ref_list;
		if (!rl)
			rl = rtl->cached_ref_list = macres_file_read_res_ref_list(mrf, type);
		if (rl) {
			int16_t resid = rl[n].resid;

			if (lseek(fd, mrf->first_res_off + rl[n].res_data_off, SEEK_SET) == -1)
				return 0;
			mr = macres_res_read(fd);
			rl[n].mr = mr;
			if (mr) {
				mr->resid = resid;;
				if (rl[n].res_map_name_list_name_off != 0xffff) {
					u_int8_t namelen;
					u_int32_t off = mrf->res_map_off +
							mrf->res_map.res_name_list_off +
							rl[n].res_map_name_list_name_off;

					if (lseek(fd, off, SEEK_SET) == -1)
						return 0;
					if (read(fd, &namelen, 1) != 1)
						return 0;
					if (namelen) {
						mr->name = malloc(namelen + 1);
						if (mr->name) {
							if (read(fd, mr->name, namelen) != namelen)
								return 0;
							mr->name[namelen] = 0;
						}
					}
					mr->namelen = namelen;
				} else {
					mr->name = 0;
					mr->namelen = 0;
				}
			}
		}
	}

	return mr;
}

macres_res *
macres_file_get_resid_of_type (macres_file *mrf, u_int32_t type, int16_t resid)
{
	int fd = mrf->fd;
	u_int32_t i;
	macres_res *mr = 0;
	macres_res_type_list *rtl;
	macres_res_ref_list *rl;

	rtl = macres_res_type_list_get(mrf->res_map.res_type_list, mrf->res_map.num_res_types, type);
	if (rtl) {
		rl = rtl->cached_ref_list;
		if (!rl)
			rl = rtl->cached_ref_list = macres_file_read_res_ref_list(mrf, type);
		if (!rl)
			return 0;
		for (i = 0; i < rtl->num_res_of_type; i++) {
			if (rl[i].resid != resid)
				continue;
			if (lseek(fd, mrf->first_res_off + rl[i].res_data_off, SEEK_SET) == -1)
				return 0;
			mr = macres_res_read(fd);
			rl[i].mr = mr;
			if (mr) {
				mr->resid = resid;;
				if (rl[i].res_map_name_list_name_off != 0xffff) {
					u_int8_t namelen;
					u_int32_t off = mrf->res_map_off +
							mrf->res_map.res_name_list_off +
							rl[i].res_map_name_list_name_off;

					if (lseek(fd, off, SEEK_SET) == -1)
						return 0;
					if (read(fd, &namelen, 1) != 1)
						return 0;
					if (namelen) {
						mr->name = malloc(namelen + 1);
						if (mr->name) {
							if (read(fd, mr->name, namelen) != namelen)
								return 0;
							mr->name[namelen] = 0;
						}
					}
					mr->namelen = namelen;
				} else {
					mr->name = 0;
					mr->namelen = 0;
				}
			}
			break;
		}
	}

	return mr;
}
