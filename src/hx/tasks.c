#include "hx.h"
#include "hxd.h"
#include "xmalloc.h"

#if defined(CONFIG_SOUND)
#include "sound.h"
#endif

struct task __task_list = {0, 0, 0, 0, 0, 0, 0, 0, 0}, *task_tail = &__task_list;
struct task *task_list = &__task_list;

struct task *
task_new (struct htlc_conn *htlc, void (*rcv)(), void *ptr, int text, const char *str)
{
	struct task *tsk;

	tsk = xmalloc(sizeof(struct task));
	tsk->trans = htlc->trans;
	tsk->text = text;
	if (str)
		tsk->str = xstrdup(str);
	else
		tsk->str = 0;
	tsk->ptr = ptr;
	tsk->rcv = rcv;

	tsk->next = 0;
	tsk->prev = task_tail;
	task_tail->next = tsk;
	task_tail = tsk;

	tsk->pos = 0;
	tsk->len = 1;
	hx_output.task_update(htlc, tsk);

	return tsk;
}

void
task_delete (struct task *tsk)
{
	if (tsk->next)
		tsk->next->prev = tsk->prev;
	if (tsk->prev)
		tsk->prev->next = tsk->next;
	if (task_tail == tsk)
		task_tail = tsk->prev;
	if (tsk->str)
		xfree(tsk->str);
	xfree(tsk);
}

void
task_delete_all (void)
{
	struct task *tsk, *tsknext;

	for (tsk = task_list->next; tsk; tsk = tsknext) {
		tsknext = tsk->next;
		task_delete(tsk);
	}
}

struct task *
task_with_trans (u_int32_t trans)
{
	struct task *tsk;

	for (tsk = task_list->next; tsk; tsk = tsk->next)
		if (tsk->trans == trans)
			return tsk;

	return 0;
}

void
task_error (struct htlc_conn *htlc)
{
	struct hl_hdr *h = (struct hl_hdr *)htlc->in.buf;
	u_int32_t trans;
	struct task *tsk;
	char *str;

	trans = ntohl(h->trans);
	tsk = task_with_trans(trans);
	if (tsk && tsk->str)
		str = tsk->str;
	else
		str = "";
	dh_start(htlc)
		if (dh_type == HTLS_DATA_TASKERROR) {
			CR2LF(dh_data, dh_len);
			strip_ansi(dh_data, dh_len);
			hx_printf_prefix(htlc, 0, INFOPREFIX, "task 0x%08x (%s) error: %.*s\n",
					 trans, str, dh_len, dh_data);
			play_sound(snd_error);
		}
	dh_end()
}

void
task_tasks_update (struct htlc_conn *htlc)
{
	struct task *tsk;

	for (tsk = task_list->next; tsk; tsk = tsk->next)
		hx_output.task_update(htlc, tsk);
}

void
task_print_list (struct htlc_conn *htlc, struct hx_chat *chat)
{
	struct task *tsk;
	struct qbuf *in = &htlc->read_in;

	hx_printf_prefix(htlc, chat, INFOPREFIX, "in qbuf: %u/%u\n", in->pos, in->len);
	hx_output.mode_underline();
	hx_printf(htlc, chat, "        tid | text | str\n");
	hx_output.mode_clear();
	for (tsk = task_list->next; tsk; tsk = tsk->next)
		hx_printf(htlc, chat, " 0x%08x | %6u | %s\n", tsk->trans, tsk->text, tsk->str);
}
