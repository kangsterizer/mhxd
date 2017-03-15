#include "hx.h"
#include "xmalloc.h"
#include <string.h>

struct hx_user *
hx_user_new (struct hx_user **utailp)
{
	struct hx_user *user, *tail = *utailp;

	user = xmalloc(sizeof(struct hx_user));
	memset(user, 0, sizeof(struct hx_user));

	user->next = 0;
	user->prev = tail;
	user->flags = 0;
	tail->next = user;
	tail = user;
	*utailp = tail;

	return user;
}

void
hx_user_delete (struct hx_user **utailp, struct hx_user *user)
{
	if (user->next)
		user->next->prev = user->prev;
	if (user->prev)
		user->prev->next = user->next;
	if (*utailp == user)
		*utailp = user->prev;
	xfree(user);
}

struct hx_user *
hx_user_with_uid (struct hx_user *ulist, u_int32_t uid)
{
	struct hx_user *userp;

	for (userp = ulist->next; userp; userp = userp->next)
		if (userp->uid == uid)
			return userp;

	return 0;
}

struct hx_user *
hx_user_with_name (struct hx_user *ulist, u_int8_t *name)
{
	int ulen, uplen, nlen;
	struct hx_user *userp, *user = 0;

	nlen = strlen(name);
	for (userp = ulist->next; userp; userp = userp->next) {
		if (!strncmp(userp->name, name, nlen)) {
			if (user) {
				ulen = strlen(user->name);
				uplen = strlen(userp->name);
				if (!((uplen == nlen && ulen != nlen) || (ulen == nlen && uplen != nlen)))
					return 0;
			}
			user = userp;
		}
	}

	return user;
}
