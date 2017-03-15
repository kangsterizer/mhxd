#include "hx.h"
#include "hxd.h"
#include <string.h>
#include <time.h>
#include "xmalloc.h"
#include "news.h"

#if defined(CONFIG_ICONV)
#include "conv.h"
#endif

void
output_news_dirlist_tty (struct htlc_conn *htlc, struct news_folder *dir)
{
	struct folder_item *item;
	u_int32_t i;

	for (i = 0; i < dir->num_entries; i++) {
		item = dir->entry[i];
#if defined(CONFIG_ICONV)
#endif
		hx_printf(htlc, 0, "%u  %s%s (%u)\n", item->type, item->name,
				(item->type==1||item->type==2)?"/":"", item->count);
	}
}

void
output_news_catlist_tty (struct htlc_conn *htlc, struct news_group *cat)
{
	struct news_item *item, *parent;
	u_int32_t i, j;

	for (i = 0; i < cat->post_count; i++) {
		item = &cat->posts[i];
		item->depth = 0;
	}
	for (i = 0; i < cat->post_count; i++) {
		item = &cat->posts[i];
		parent = 0;
		if (!item)
			continue;
		for (j = 0; j < cat->post_count; j++) {
			if (cat->posts[j].postid == item->parentid) {
				parent = &cat->posts[j];
				item->depth = parent->depth+1;
				break;
			}
		}
#if defined(CONFIG_ICONV)
#endif
		hx_printf(htlc, 0, "%5u | %-31s | ", item->postid, item->sender);
		for (j = 0; j < item->depth; j++) {
			hx_printf(htlc, 0, "  ");
		}
		hx_printf(htlc, 0, "%s\n", item->subject);
	}
}

void
output_news_thread_tty (struct htlc_conn *htlc, struct news_post *post)
{
	struct news_item *item;

	item = post->item;
	hx_printf(htlc, 0, "%5u | %s | %s\n", item->postid, item->sender, item->subject);
	hx_printf(htlc, 0, "%s\n", post->buf);
}

extern void mac_to_tm (struct tm *tm, u_int8_t *date);

void
rcv_task_tnews_cat_list (struct htlc_conn *htlc, char *path)
{
	struct news_group *group = 0;
	struct news_item *ni;
	unsigned char *ptr;
	u_int32_t p, j;

#define get_pstring(_s, _p)		\
do {					\
	u_int8_t _l = *_p;		\
	if (!_l) _s = 0; else {		\
		_s = xmalloc(_l+1);	\
		memcpy(_s, _p+1, _l);	\
		_s[_l] = 0;		\
		_p += _l;		\
	}				\
	_p++;				\
} while (0)

	dh_start(htlc)
		switch (dh_type) {
		case HTLS_DATA_TASKERROR:
			return;
		case HTLS_DATA_NEWS_CATLIST:
			group = xmalloc(sizeof(struct news_group));
			group->path = path;
			ptr = dh_data+4;
			L32NTOH(group->post_count, ptr); ptr += 4;
			ptr += *ptr;
			ptr += 2;
			group->posts = xmalloc(sizeof(struct news_item)*group->post_count);
			for (p = 0, ni = group->posts; p < group->post_count; p++, ni++) {
				L32NTOH(ni->postid, ptr); ptr += 4;
				L16NTOH(ni->date.base_year, ptr); ptr += 2;
				L16NTOH(ni->date.pad, ptr); ptr += 2;
				L32NTOH(ni->date.seconds, ptr); ptr += 4;
				L32NTOH(ni->parentid, ptr); ptr += 8;
				L16NTOH(ni->partcount, ptr); ptr += 2;
				get_pstring(ni->subject, ptr);
				get_pstring(ni->sender, ptr);
				ni->parts = xmalloc(sizeof(struct news_parts)*ni->partcount);
				ni->size = 0;
				for (j = 0; j < ni->partcount; j++) {
					get_pstring(ni->parts[j].mime_type, ptr);
					L16NTOH(ni->parts[j].size, ptr); ptr += 2;
					ni->size += ni->parts[j].size;
				}
				ni->group = group;
			}
			break;
		}
	dh_end()

	if (group)
		hx_output.news_catlist(htlc, group);
#if 0
	/* xfree(path); */
	if (group) {
		for (p = 0, ni = group->posts; p < group->post_count; p++, ni++) {
			if (ni->subject)
				xfree(ni->subject);
			if (ni->sender)
				xfree(ni->sender);
			if (ni->parts)
				xfree(ni->parts);
		}
		xfree(group->posts);
		xfree(group);
	}
#endif
}

void
rcv_task_tnews_dir_list (struct htlc_conn *htlc, char *path)
{
	struct hl_newslist_category_hdr *ch;
	u_int8_t *name;
	u_int16_t nlen;
	struct news_folder *folder = 0;
	struct folder_item *item;
	unsigned int num = 0;

	folder = xmalloc(sizeof(struct news_folder));
	folder->entry = xmalloc(sizeof(struct folder_item));
	folder->path = path;
	folder->num_entries = 0;
	dh_start(htlc)
		switch (dh_type) {
		case HTLS_DATA_TASKERROR:
			xfree(folder->entry);
			xfree(folder);
			return;
		case HTLS_DATA_NEWS_DIRLIST:
			num++;
			folder->entry = xrealloc(folder->entry, sizeof(struct folder_item *)*num);
			item = xmalloc(sizeof(struct folder_item));
			folder->entry[num-1] = item;
			item->type = dh_data[0];
			item->count = 0;
			nlen = dh_len-1;
			name = dh_data+1;
			item->name = xmalloc(nlen+1);
			memcpy(item->name, name, nlen);
			item->name[nlen] = 0;
			break;
		case HTLS_DATA_NEWS_DIRLIST_EXTENDED:
			num++;
			folder->entry = xrealloc(folder->entry, sizeof(struct folder_item *)*num);
			item = xmalloc(sizeof(struct folder_item));
			folder->entry[num-1] = item;
			L16NTOH(item->type, dh_data);
			L16NTOH(item->count, dh_data+2);
			if (item->type & 1) {
				ch = (struct hl_newslist_category_hdr *)dh;
				nlen = ch->namelen;
				name = ch->name;
			} else {
				nlen = *(dh_data+4);
				name = dh_data+5;
			}
			item->name = xmalloc(nlen+1);
			memcpy(item->name, name, nlen);
			item->name[nlen] = 0;
			break;
		default:
			break;
		}
	dh_end()

	folder->num_entries = num;
	hx_output.news_dirlist(htlc, folder);

#if 0
	/* xfree(folder->path); */
	for (num = 0; num < folder->num_entries; num++) {
		xfree(folder->entry[num]->name);
		xfree(folder->entry[num]);
	}
	xfree(folder->entry);
	xfree(folder);
#endif
}

void
rcv_task_tnews_post_get (struct htlc_conn *htlc, struct news_item *item)
{
	struct news_post *post;
	u_int32_t tid, nexttid, prevtid, parenttid, nextsubtid;

	post = 0;
	if (!item)
		item = xmalloc(sizeof(struct news_item));
	dh_start(htlc)
		switch(dh_type) {
		case HTLS_DATA_NEWS_POST:
			post = xmalloc(sizeof(struct news_post));
			post->buf = xmalloc(dh_len+1);
			memcpy(post->buf, dh_data, dh_len);
			post->buf[dh_len] = 0;
			post->buflen = dh_len;
			CR2LF(post->buf, dh_len);
			strip_ansi(post->buf, dh_len);
			break;
		case HTLS_DATA_NEWS_PREVTHREADID:
			dh_getint(prevtid);
			item->previd = prevtid;
			break;
		case HTLS_DATA_NEWS_NEXTTHREADID:
			dh_getint(nexttid);
			item->nextid = nexttid;
			break;
		case HTLS_DATA_NEWS_PARENTTHREADID:
			dh_getint(parenttid);
			item->parentid = parenttid;
			break;
		case HTLS_DATA_NEWS_NEXTSUBTHREADID:
			dh_getint(nextsubtid);
			item->nextsubid = nextsubtid;
			break;
		case HTLS_DATA_NEWS_THREADID:
			dh_getint(tid);
			item->postid = tid;
			break;
		case HTLS_DATA_NEWS_SUBJECT:
			item->subject = xmalloc(dh_len+1);
			memcpy(item->subject, dh_data, dh_len);
			item->subject[dh_len] = 0;
			break;
		case HTLS_DATA_NEWS_POSTER:
			item->sender = xmalloc(dh_len+1);
			memcpy(item->sender, dh_data, dh_len);
			item->sender[dh_len] = 0;
			break;
		case HTLS_DATA_NEWS_MIMETYPE:
			break;
		case HTLS_DATA_NEWS_DATE:
			break;
		} 
	dh_end()

	if (!post)
		return;

	post->item = item;
	hx_output.news_thread(htlc, post);
	xfree(post->buf);
	xfree(post);
}

void
hx_tnews_list (struct htlc_conn *htlc, const char *path, int is_cat)
{
	struct hldir *hldir;
	u_int32_t type;
	void (*tfn)();

	if (!htlc->fd)
		return;
	if (is_cat) {
		type = HTLC_HDR_NEWS_LISTCATEGORY;
		tfn = rcv_task_tnews_cat_list;
	} else {
		type = HTLC_HDR_NEWS_LISTDIR;
		tfn = rcv_task_tnews_dir_list;
	}
	task_new(htlc, tfn, xstrdup(path), 0, "tnews_list");
	if (!strcmp(path, "/"))
		hlwrite(htlc, type, 0, 0);
	else {
		hldir = path_to_hldir(path, 0);
		hlwrite(htlc, type, 0, 1,
			HTLC_DATA_NEWS_DIR, hldir->dirlen, hldir->dir);
		xfree(hldir);
	}
}

#if 0
static void
rcv_task_tnews_get (struct htlc_conn *htlc)
{
	u_int32_t prevtid, nexttid, parenttid, nextsubtid;
	u_int8_t slen, plen, mlen, subject[256], poster[256], mimetype[256];
	char datestr[64];
	struct tm tm;

	dh_start(htlc)
		switch (dh_type) {
			case HTLS_DATA_NEWS_POST:
				CR2LF(dh_data, dh_len);
				hx_printf(htlc, 0, "NEWS: post data:\n%.*s\n", dh_len, dh_data);
				break;
			case HTLS_DATA_NEWS_PREVTHREADID:
				dh_getint(prevtid);
				break;
			case HTLS_DATA_NEWS_NEXTTHREADID:
				dh_getint(nexttid);
				break;
			case HTLS_DATA_NEWS_PARENTTHREADID:
				dh_getint(parenttid);
				break;
			case HTLS_DATA_NEWS_NEXTSUBTHREADID:
				dh_getint(nextsubtid);
				break;
			case HTLS_DATA_NEWS_SUBJECT:
				slen = dh_len > 255 ? 255 : dh_len;
				memcpy(subject, dh_data, slen);
				subject[slen] = 0;
				hx_printf(htlc, 0, "subject: %s\n", subject);
				break;
			case HTLS_DATA_NEWS_POSTER:
				plen = dh_len > 255 ? 255 : dh_len;
				memcpy(poster, dh_data, plen);
				poster[plen] = 0;
				hx_printf(htlc, 0, "poster: %s\n", poster);
				break;
			case HTLS_DATA_NEWS_MIMETYPE:
				mlen = dh_len > 255 ? 255 : dh_len;
				memcpy(mimetype, dh_data, mlen);
				mimetype[mlen] = 0;
				hx_printf(htlc, 0, "mimetype: %s\n", mimetype);
				break;
			case HTLS_DATA_NEWS_DATE:
				mac_to_tm(&tm, dh_data);
				strftime(datestr, sizeof(datestr)-1, hx_timeformat, &tm);
				hx_printf(htlc, 0, "date: %s\n", datestr);
				break;
			default:
				hx_printf(htlc, 0, "NEWS: %04x %04x\n", dh_type, dh_len);
				break;
		}
	dh_end()
}
#endif

void
hx_tnews_get (struct htlc_conn *htlc, const char *path, u_int32_t tid, const u_int8_t *mt, void *item)
{
	u_int16_t mtlen;
	u_int32_t ntid;
	struct hldir *hldir;

	if (!htlc->fd)
		return;
	task_new(htlc, rcv_task_tnews_post_get, item, 0, "tnews_get");
	hldir = path_to_hldir(path, 0);
	mtlen = strlen(mt);
	ntid = htonl(tid);
	hlwrite(htlc, HTLC_HDR_NEWS_GETTHREAD, 0, 3,
		HTLC_DATA_NEWS_DIR, hldir->dirlen, hldir->dir,
		HTLC_DATA_NEWS_THREADID, sizeof(ntid), &ntid,
		HTLC_DATA_NEWS_MIMETYPE, mtlen, mt);
	xfree(hldir);
}

void
hx_tnews_post (struct htlc_conn *htlc, char *path, char *subject,
	       u_int32_t threadid, char *text)
{
	struct hldir *hldir;
	u_int32_t parent = 0;
	u_int32_t ntid;
	char *subj, *post;
	size_t subj_len, post_len, len;

	hldir = path_to_hldir(path, 0);
	task_new(htlc, 0, 0, 0, "tnews_post");
	ntid = htonl(threadid);
#if defined(CONFIG_ICONV)
{
	char *subj_e, *post_e;

	len = strlen(subject);
	subj_len = convbuf(g_server_encoding, g_encoding, subject, len, &subj_e);
	if (subj_len) {
		subj = subj_e;
	} else {
		subj = subject;
		subj_len = strlen(subject);
	}
	len = strlen(text);
	post_len = convbuf(g_server_encoding, g_encoding, text, len, &post_e);
	if (post_len) {
		post = post_e;
	} else {
		post = text;
		post_len = strlen(text);
	}
}
#else
	subj = subject;
	subj_len = strlen(subject);
	post = text;
	post_len = strlen(text);
#endif
	hlwrite(htlc, HTLC_HDR_NEWS_POSTTHREAD, 0, 6,
		HTLC_DATA_NEWS_DIR, hldir->dirlen, hldir->dir, 
		HTLC_DATA_NEWS_PARENTTHREADID, 4, &parent,
		HTLC_DATA_NEWS_MIMETYPE, 10, "text/plain", 
		HTLC_DATA_NEWS_SUBJECT, subj_len, subj,
		HTLC_DATA_NEWS_POST, post_len, post,
		HTLC_DATA_NEWS_THREADID, 4, &ntid);
	xfree(hldir);
#if defined(CONFIG_ICONV)
	if (subj != subject)
		xfree(subj);
	if (post != text)
		xfree(post);
#endif
}


static void
rcv_task_news_file (struct htlc_conn *htlc)
{
	dh_start(htlc)
		if (dh_type != HTLS_DATA_NEWS)
			continue;
		htlc->news_len = dh_len;
		htlc->news_buf = xrealloc(htlc->news_buf, htlc->news_len + 1);
		memcpy(htlc->news_buf, dh_data, htlc->news_len);
		CR2LF(htlc->news_buf, htlc->news_len);
		strip_ansi(htlc->news_buf, htlc->news_len);
		htlc->news_buf[htlc->news_len] = 0;
	dh_end()
	hx_output.news_file(htlc, htlc->news_buf, htlc->news_len);
}

void
hx_get_news (struct htlc_conn *htlc)
{
	if (!htlc->fd)
		return;
	task_new(htlc, rcv_task_news_file, 0, 0, "news");
	hlwrite(htlc, HTLC_HDR_NEWSFILE_GET, 0, 0);
}

void
hx_post_news (struct htlc_conn *htlc, const char *news, u_int16_t len)
{
	char *post = (char *)news;
#if defined(CONFIG_ICONV)
	char *post_e;
	size_t post_len;

	post_len = convbuf(g_server_encoding, g_encoding, news, len, &post_e);
	if (post_len) {
		post = post_e;
		len = post_len;
	} else {
		post_len = len;
	}
#endif
	task_new(htlc, 0, 0, 0, "post");
	hlwrite(htlc, HTLC_HDR_NEWSFILE_POST, 0, 1,
		HTLC_DATA_NEWSFILE_POST, len, post);
#if defined(CONFIG_ICONV)
	if (post != news)
		xfree(post);
#endif
}
