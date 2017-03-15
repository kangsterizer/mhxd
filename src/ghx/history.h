/* History.h -- the names of functions that you can call in history. */
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

#ifndef _HISTORY_H_
#define _HISTORY_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __P
#ifdef __STDC__
#define __P(x) x
#else
#define __P(x) ()
#endif
#endif

#ifdef __STDC__
typedef void *histdata_t;
#else
typedef char *histdata_t;
#endif

/* The structure used to store a history entry. */
typedef struct _hist_entry {
  char *line;
  histdata_t data;
} HIST_ENTRY;

/* A structure used to pass the current state of the history stuff around. */
typedef struct _hist_state {
  HIST_ENTRY **entries;		/* Pointer to the entries themselves. */
  int offset;			/* The location pointer within this array. */
  int length;			/* Number of elements within this array. */
  int size;			/* Number of slots allocated to this array. */
  int flags;
} HISTORY_STATE;

/* Flag values for the `flags' member of HISTORY_STATE. */
#define HS_STIFLED	0x01

/* Initialization and state management. */

extern void *history_new __P((void));

/* Begin a session in which the history functions might be used.  This
   just initializes the interactive variables. */
extern void using_history __P((void *));

/* Return the current HISTORY_STATE of the history. */
extern HISTORY_STATE *history_get_history_state __P((void *));

/* Set the state of the current history array to STATE. */
extern void history_set_history_state __P((void *, HISTORY_STATE *));

/* Manage the history list. */

/* Place STRING at the end of the history list.
   The associated data field (if any) is set to NULL. */
extern void add_history __P((void *, char *));

/* A reasonably useless function, only here for completeness.  WHICH
   is the magic number that tells us which element to delete.  The
   elements are numbered from 0. */
extern HIST_ENTRY *remove_history __P((void *, int));

/* Make the history entry at WHICH have LINE and DATA.  This returns
   the old entry so you can dispose of the data.  In the case of an
   invalid WHICH, a NULL pointer is returned. */
extern HIST_ENTRY *replace_history_entry __P((void *, int, char *, histdata_t));

/* Clear the history list and start over. */
extern void clear_history __P((void *));

/* Stifle the history list, remembering only MAX number of entries. */
extern void stifle_history __P((void *, int));

/* Stop stifling the history.  This returns the previous amount the
   history was stifled by.  The value is positive if the history was
   stifled, negative if it wasn't. */
extern int unstifle_history __P((void *));

/* Return 1 if the history is stifled, 0 if it is not. */
extern int history_is_stifled __P((void *));

/* Information about the history list. */

/* Return a NULL terminated array of HIST_ENTRY which is the current input
   history.  Element 0 of this list is the beginning of time.  If there
   is no history, return NULL. */
extern HIST_ENTRY **history_list __P((void *));

/* Returns the number which says what history element we are now
   looking at.  */
extern int where_history __P((void *));
  
/* Return the history entry at the current position, as determined by
   history_offset.  If there is no entry there, return a NULL pointer. */
extern HIST_ENTRY *current_history __P((void *));

/* Return the history entry which is logically at OFFSET in the history
   array.  OFFSET is relative to history_base. */
extern HIST_ENTRY *history_get __P((void *, int));

/* Return the number of bytes that the primary history entries are using.
   This just adds up the lengths of the_history->lines. */
extern int history_total_bytes __P((void *));

/* Moving around the history list. */

/* Set the position in the history list to POS. */
extern int history_set_pos __P((void *, int));

/* Back up history_offset to the previous history entry, and return
   a pointer to that entry.  If there is no previous entry, return
   a NULL pointer. */
extern HIST_ENTRY *previous_history __P((void *));

/* Move history_offset forward to the next item in the input_history,
   and return the a pointer to that entry.  If there is no next entry,
   return a NULL pointer. */
extern HIST_ENTRY *next_history __P((void *));

#ifdef __cplusplus
}
#endif

#endif /* !_HISTORY_H_ */
