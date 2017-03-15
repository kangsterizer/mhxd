/* History.c -- standalone history library */

/* Copyright (C) 1989, 1992 Free Software Foundation, Inc.

   This file contains the GNU History Library (the Library), a set of
   routines for managing the text of previously typed lines.

   The Library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   The Library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   59 Temple Place, Suite 330, Boston, MA 02111 USA. */

/* The goal is to make the implementation transparent, so that you
   don't have to know what data types are used, just what functions
   you can call.  I think I have done that. */

#if defined (HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "history.h"

#include "xmalloc.h"

/* The number of slots to increase the_history by. */
#define DEFAULT_HISTORY_GROW_SIZE 50

/* **************************************************************** */
/*								    */
/*			History Functions			    */
/*								    */
/* **************************************************************** */

struct history {
	/* An array of HIST_ENTRY.  This is where we store the history. */
	HIST_ENTRY **the_history;

	/* Non-zero means that we have enforced a limit on the amount of
	   history that we save. */
	int history_stifled;

	/* If HISTORY_STIFLED is non-zero, then this is the maximum number of
	   entries to remember. */
	int max_input_history;

	/* The current location of the interactive history pointer.  Just makes
	   life easier for outside callers. */
	int history_offset;

	/* The number of strings currently stored in the history list. */
	int history_length;

	/* The current number of slots allocated to the input_history. */
	int history_size;

	/* The logical `base' of the history array.  It defaults to 1. */
	int history_base;
};

void *
history_new (void)
{
	struct history *history;

	history = xmalloc(sizeof(struct history));
	memset(history, 0, sizeof(struct history));
	history->history_base = 1;

	return (void *)history;
}

/* Return the current HISTORY_STATE of the history. */
HISTORY_STATE *
history_get_history_state (__history)
     void *__history;
{
  struct history *history = (struct history *)__history;
  HISTORY_STATE *state;

  state = (HISTORY_STATE *)xmalloc (sizeof (HISTORY_STATE));
  state->entries = history->the_history;
  state->offset = history->history_offset;
  state->length = history->history_length;
  state->size = history->history_size;
  state->flags = 0;
  if (history->history_stifled)
    state->flags |= HS_STIFLED;

  return (state);
}

/* Set the state of the current history array to STATE. */
void
history_set_history_state (__history, state)
     void *__history;
     HISTORY_STATE *state;
{
  struct history *history = (struct history *)__history;

  history->the_history = state->entries;
  history->history_offset = state->offset;
  history->history_length = state->length;
  history->history_size = state->size;
  if (state->flags & HS_STIFLED)
    history->history_stifled = 1;
}

/* Begin a session in which the history functions might be used.  This
   initializes interactive variables. */
void
using_history (__history)
     void *__history;
{
  struct history *history = (struct history *)__history;

  history->history_offset = history->history_length;
}

/* Return the number of bytes that the primary history entries are using.
   This just adds up the lengths of the_history->lines. */
int
history_total_bytes (__history)
     void *__history;
{
  struct history *history = (struct history *)__history;
  register int i, result;

  result = 0;

  for (i = 0; history->the_history && history->the_history[i]; i++)
    result += strlen (history->the_history[i]->line);

  return (result);
}

/* Returns the magic number which says what history element we are
   looking at now.  In this implementation, it returns history_offset. */
int
where_history (__history)
     void *__history;
{
  struct history *history = (struct history *)__history;

  return (history->history_offset);
}

/* Make the current history item be the one at POS, an absolute index.
   Returns zero if POS is out of range, else non-zero. */
int
history_set_pos (__history, pos)
     void *__history;
     int pos;
{
  struct history *history = (struct history *)__history;

  if (pos > history->history_length || pos < 0 || !history->the_history)
    return (0);
  history->history_offset = pos;
  return (1);
}
 
/* Return the current history array.  The caller has to be carefull, since this
   is the actual array of data, and could be bashed or made corrupt easily.
   The array is terminated with a NULL pointer. */
HIST_ENTRY **
history_list (__history)
     void *__history;
{
  struct history *history = (struct history *)__history;

  return (history->the_history);
}

/* Return the history entry at the current position, as determined by
   history_offset.  If there is no entry there, return a NULL pointer. */
HIST_ENTRY *
current_history (__history)
     void *__history;
{
  struct history *history = (struct history *)__history;

  return ((history->history_offset == history->history_length) || history->the_history == 0)
		? (HIST_ENTRY *)NULL
		: history->the_history[history->history_offset];
}

/* Back up history_offset to the previous history entry, and return
   a pointer to that entry.  If there is no previous entry then return
   a NULL pointer. */
HIST_ENTRY *
previous_history (__history)
     void *__history;
{
  struct history *history = (struct history *)__history;

  return history->history_offset ? history->the_history[--history->history_offset] : (HIST_ENTRY *)NULL;
}

/* Move history_offset forward to the next history entry, and return
   a pointer to that entry.  If there is no next entry then return a
   NULL pointer. */
HIST_ENTRY *
next_history (__history)
     void *__history;
{
  struct history *history = (struct history *)__history;

  return (history->history_offset == history->history_length) ? (HIST_ENTRY *)NULL : history->the_history[++history->history_offset];
}

/* Return the history entry which is logically at OFFSET in the history array.
   OFFSET is relative to history_base. */
HIST_ENTRY *
history_get (__history, offset)
     void *__history;
     int offset;
{
  struct history *history = (struct history *)__history;
  int local_index;

  local_index = offset - history->history_base;
  return (local_index >= history->history_length || local_index < 0 || !history->the_history)
		? (HIST_ENTRY *)NULL
		: history->the_history[local_index];
}

/* Place STRING at the end of the history list.  The data field
   is  set to NULL. */
void
add_history (__history, string)
     void *__history;
     char *string;
{
  struct history *history = (struct history *)__history;
  HIST_ENTRY *temp;

  if (history->history_stifled && (history->history_length == history->max_input_history))
    {
      register int i;

      /* If the history is stifled, and history_length is zero,
	 and it equals max_input_history, we don't save items. */
      if (history->history_length == 0)
	return;

      /* If there is something in the slot, then remove it. */
      if (history->the_history[0])
	{
	  xfree (history->the_history[0]->line);
	  xfree (history->the_history[0]);
	}

      /* Copy the rest of the entries, moving down one slot. */
      for (i = 0; i < history->history_length; i++)
	history->the_history[i] = history->the_history[i + 1];

      history->history_base++;
    }
  else
    {
      if (history->history_size == 0)
	{
	  history->history_size = DEFAULT_HISTORY_GROW_SIZE;
	  history->the_history = (HIST_ENTRY **)xmalloc (history->history_size * sizeof (HIST_ENTRY *));
	  history->history_length = 1;
	}
      else
	{
	  if (history->history_length == (history->history_size - 1))
	    {
	      history->history_size += DEFAULT_HISTORY_GROW_SIZE;
	      history->the_history = (HIST_ENTRY **)
		xrealloc (history->the_history, history->history_size * sizeof (HIST_ENTRY *));
	    }
	  history->history_length++;
	}
    }

  temp = (HIST_ENTRY *)xmalloc (sizeof (HIST_ENTRY));
  temp->line = xstrdup (string);
  temp->data = (char *)NULL;

  history->the_history[history->history_length] = (HIST_ENTRY *)NULL;
  history->the_history[history->history_length - 1] = temp;
}

/* Make the history entry at WHICH have LINE and DATA.  This returns
   the old entry so you can dispose of the data.  In the case of an
   invalid WHICH, a NULL pointer is returned. */
HIST_ENTRY *
replace_history_entry (__history, which, line, data)
     void *__history;
     int which;
     char *line;
     histdata_t data;
{
  struct history *history = (struct history *)__history;
  HIST_ENTRY *temp = (HIST_ENTRY *)xmalloc (sizeof (HIST_ENTRY));
  HIST_ENTRY *old_value;

  if (which >= history->history_length)
    return ((HIST_ENTRY *)NULL);

  old_value = history->the_history[which];

  temp->line = xstrdup (line);
  temp->data = data;
  history->the_history[which] = temp;

  return (old_value);
}

/* Remove history element WHICH from the history.  The removed
   element is returned to you so you can free the line, data,
   and containing structure. */
HIST_ENTRY *
remove_history (__history, which)
     void *__history;
     int which;
{
  struct history *history = (struct history *)__history;
  HIST_ENTRY *return_value;

  if (which >= history->history_length || !history->history_length)
    return_value = (HIST_ENTRY *)NULL;
  else
    {
      register int i;
      return_value = history->the_history[which];

      for (i = which; i < history->history_length; i++)
	history->the_history[i] = history->the_history[i + 1];

      history->history_length--;
    }

  return (return_value);
}

/* Stifle the history list, remembering only MAX number of lines. */
void
stifle_history (__history, max)
     void *__history;
     int max;
{
  struct history *history = (struct history *)__history;

  if (max < 0)
    max = 0;

  if (history->history_length > max)
    {
      register int i, j;

      /* This loses because we cannot free the data. */
      for (i = 0, j = history->history_length - max; i < j; i++)
	{
	  xfree (history->the_history[i]->line);
	  xfree (history->the_history[i]);
	}

      history->history_base = i;
      for (j = 0, i = history->history_length - max; j < max; i++, j++)
	history->the_history[j] = history->the_history[i];
      history->the_history[j] = (HIST_ENTRY *)NULL;
      history->history_length = j;
    }

  history->history_stifled = 1;
  history->max_input_history = max;
}

/* Stop stifling the history.  This returns the previous amount the 
   history was stifled by.  The value is positive if the history was
   stifled,  negative if it wasn't. */
int
unstifle_history (__history)
     void *__history;
{
  struct history *history = (struct history *)__history;

  if (history->history_stifled)
    {
      history->history_stifled = 0;
      return (-history->max_input_history);
    }

  return (history->max_input_history);
}

int
history_is_stifled (__history)
     void *__history;
{
  struct history *history = (struct history *)__history;

  return (history->history_stifled);
}

void
clear_history (__history)
     void *__history;
{
  struct history *history = (struct history *)__history;
  register int i;

  /* This loses because we cannot free the data. */
  for (i = 0; i < history->history_length; i++)
    {
      xfree (history->the_history[i]->line);
      xfree (history->the_history[i]);
      history->the_history[i] = (HIST_ENTRY *)NULL;
    }

  history->history_offset = history->history_length = 0;
}
