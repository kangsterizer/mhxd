
/*===========================================================================*/

/*
 *  Copyright (C) 1997 Jason Hutchens
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE.  See the Gnu Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*===========================================================================*/

/*
 *	$Id: hal.c,v 1.2 2003/10/10 16:44:03 ror Exp $
 *
 *	File:		megahal.c
 *
 *	Program:	MegaHAL v8
 *
 *	Purpose:	To simulate a natural language conversation with a psychotic
 *			computer.  This is achieved by learning from the user's
 *			input using a third-order Markov model on the word level.
 *			Words are considered to be sequences of characters separated
 *			by whitespace and punctuation.  Replies are generated
 *			randomly based on a keyword, and they are scored using
 *			measures of surprise.
 *
 *	Author:		Mr. Jason L. Hutchens
 *
 *	WWW:		http://ciips.ee.uwa.edu.au/~hutch/hal/
 *
 *	E-Mail:		hutch@ciips.ee.uwa.edu.au
 *
 *	Contact:	The Centre for Intelligent Information Processing Systems
 *			Department of Electrical and Electronic Engineering
 *			The University of Western Australia
 *			AUSTRALIA 6907
 *
 *	Phone:		+61-8-9380-3856
 *
 *	Facsimile:	+61-8-9380-1168
 *
 *	Notes:		This file is best viewed with tabstops set to three spaces.
 */

/*===========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include "hx.h"
#include "xmalloc.h"

#if defined(__WIN32__)
#define random() rand()
#define srandom(x) srand(x)
#endif

/*===========================================================================*/

#define P_THINK		40
#define D_KEY		100000
#define V_KEY		50000
#define D_THINK		500000
#define V_THINK		250000

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define COOKIE "MegaHALv8"

/*===========================================================================*/

typedef enum {FALSE, TRUE} bool;

typedef struct {
	u_int8_t length;
	char *word;
} STRING;

typedef struct {
	u_int32_t size;
	STRING *entry;
	u_int16_t *index;
} DICTIONARY;

typedef struct {
	u_int16_t size;
	STRING *from;
	STRING *to;
} SWAP;

typedef struct NODE {
	u_int16_t symbol;
	u_int32_t usage;
	u_int16_t count;
	u_int16_t branch;
	struct NODE **tree;
} TREE;

typedef struct {
	u_int8_t order;
	TREE *forward;
	TREE *backward;
	TREE **context;
	DICTIONARY *dictionary;
} MODEL;

/*===========================================================================*/

void add_aux (MODEL *, DICTIONARY *, STRING);
void add_key (MODEL *, DICTIONARY *, STRING);
void add_node (TREE *, TREE *, int);
void add_swap (SWAP *, char *, char *);
TREE *add_symbol (TREE *, u_int16_t);
u_int16_t add_word (DICTIONARY *, STRING);
int babble (MODEL *, DICTIONARY *, DICTIONARY *);
bool boundary (char *, unsigned int);
void capitalize (char *);
void delay (struct htlc_conn *, char *);
bool dissimilar (DICTIONARY *, DICTIONARY *);
void error (const char *, const char *, ...);
float evaluate_reply (MODEL *, DICTIONARY *, DICTIONARY *);
void exithal (void);
TREE *find_symbol (TREE *, int);
TREE *find_symbol_add (TREE *, int);
u_int16_t find_word (DICTIONARY *, STRING);
void free_dictionary (DICTIONARY *);
char *generate_reply (MODEL *, DICTIONARY *);
void initialize_context (MODEL *);
void initialize_dictionary (DICTIONARY *);
void initialize_error (const char *);
DICTIONARY *initialize_list (const char *);
void initialize_status (const char *);
SWAP *initialize_swap (const char *);
void learn (MODEL *, DICTIONARY *);
void load_dictionary (FILE *, DICTIONARY *);
bool load_model (const char *, MODEL *);
void load_tree (FILE *, TREE *);
void load_word (FILE *, DICTIONARY *);
void make_greeting (DICTIONARY *);
DICTIONARY *make_keywords (MODEL *, DICTIONARY *);
char *make_output (DICTIONARY *);
void make_words (char *, DICTIONARY *);
DICTIONARY *new_dictionary (void);
MODEL *new_model (int);
TREE *new_node (void);
SWAP *new_swap (void);
void print_header (FILE *);
DICTIONARY *reply (MODEL *, DICTIONARY *);
void save_dictionary (FILE *, DICTIONARY *);
void save_model (const char *, MODEL *);
void save_tree (FILE *, TREE *);
void save_word (FILE *, STRING);
int search_dictionary (DICTIONARY *, STRING, bool *);
int search_node (TREE *, int, bool *);
int seed (MODEL *, DICTIONARY *);
void show_dictionary (DICTIONARY *);
void status (const char *, ...);
void train (MODEL *, const char *);
void update_context (MODEL *, int);
void update_model (MODEL *, int);
void upper (char *);
int wordcmp (STRING, STRING);
bool word_exists (DICTIONARY *, STRING);
void write_input (char *, char *);
void write_output (struct htlc_conn *, char *);

/*===========================================================================*/
#define ORDER	5
#define TIMEOUT	2

int hal_active = 0;
bool used_key, typing_delay = TRUE;
DICTIONARY *g_words = 0, *g_ban = 0, *g_aux = 0, *g_grt = 0;
MODEL *g_model = 0;
SWAP *g_swp = 0;
FILE *errorfp, *statusfp;

#include "getopt.h"

static struct option hal_opts[] = {
	{"delay",	0, 0,	'd'},
	{"quit",	0, 0,	'q'},
	{"restart",	0, 0,	'r'},
	{"save",	0, 0,	's'},
	{0, 0, 0, 0}
};

void
cmd_hal (int argc, char **argv, char *str, struct htlc_conn *htlc, struct hx_chat *chat)
{
	int longind, o;
	struct opt_r opt;

	if (argc < 2) {
usage:		hx_printf(htlc, chat, "usage: %s [-sdrq] [--save] [--delay] [--restart] [--quit]\n", argv[0]);
		return;
	}

	opt.ind = 0;
	opt.err_printf = 0;
	while ((o = getopt_long_r(argc, argv, "dqrs", hal_opts, &longind, &opt)) != EOF) {
		if (o == 0)
			o = hal_opts[longind].val;
		switch (o) {
			case 's':
				if (g_model) {
					save_model("megahal.brn", g_model);
					make_greeting(g_words);
				}
				break;
			case 'd':
				typing_delay = typing_delay == TRUE ? FALSE : TRUE;
				break;
			case 'r':
				hal_active = 1;
				break;
			case 'q':
				if (g_model)
					save_model("megahal.brn", g_model);
				exithal();
				break;
			default:
				goto usage;
		}
	}
}

void
hal_rcv (struct htlc_conn *htlc, char *input_p, char *user)
{
	char *output, *input = xstrdup(input_p);

	if (!g_words) {
		errorfp = stderr;
		statusfp = stdout;
		/*
		 *	Create a dictionary which will be used to hold the segmented
		 *	version of the user's input.
		 */
		g_words = new_dictionary();

		/*
		 *	Do some initialisation 
		 */
		initialize_error("megahal.log");
		srandom(time(0) + clock() + getpid());

		/*
		 *	Create a language model.
		 */
		g_model = new_model(ORDER);

		/*
		 *	Train the model on a text if one exists
		 */
		if (load_model("megahal.brn", g_model) == FALSE)
			train(g_model, "megahal.trn");

		/*
		 *	Read a dictionary containing banned keywords, auxiliary keywords,
		 *	greeting keywords and swap keywords
		 */
		g_ban = initialize_list("megahal.ban");
		g_aux = initialize_list("megahal.aux");
		g_grt = initialize_list("megahal.grt");
		g_swp = initialize_swap("megahal.swp");

		initialize_status("megahal.txt");
	}

	write_input(input, user);
	upper(input);
	make_words(input, g_words);
	learn(g_model, g_words);
	output = generate_reply(g_model, g_words);
	write_output(htlc, output);
	xfree(output);
	xfree(input);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	exithal
 *
 *	Purpose:	Terminate the program.
 */
void
exithal (void)
{
	hal_active = 0;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	initialize_error
 *
 *	Purpose:	Close the current error file pointer, and open a new one.
 */
void
initialize_error (const char *filename)
{
	if (errorfp != stderr)
		fclose(errorfp);
	if (!filename || !(errorfp = fopen(filename, "a"))) {
		errorfp = stderr;
		return;
	}

	print_header(errorfp);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	error
 *
 *	Purpose:	Print the specified message to the error file.
 */
void
error (const char *title, const char *fmt, ...)
{
	va_list argp;

	fprintf(errorfp, "%s: ", title);
	va_start(argp, fmt);
	vfprintf(errorfp, fmt, argp);
	va_end(argp);
	fprintf(errorfp, ".\n");
	fflush(errorfp);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	initialize_status
 *
 *	Purpose:	Close the current status file pointer, and open a new one.
 */
void
initialize_status (const char *filename)
{
	if (statusfp != stdout)
		fclose(statusfp);
	if (!filename || !(statusfp = fopen(filename, "a"))) {
		statusfp = stdout;
		return;
	}

	print_header(statusfp);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	status
 *
 *	Purpose		Print the specified message to the status file.
 */
void
status (const char *fmt, ...)
{
	va_list argp;

	va_start(argp, fmt);
	vfprintf(statusfp, fmt, argp);
	va_end(argp);
	fflush(statusfp);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	print_header
 *
 *	Purpose:	Display a copyright message and timestamp.
 */
void
print_header (FILE *fp)
{
	time_t t;
	char timestamp[1024];
	struct tm *local;

	t = time(0);
	local = localtime(&t);
	strftime(timestamp, 1024, "Start at: [%Y/%m/%d %H:%M:%S]", local);

	fprintf(fp,	"(c)1998 Cambridge Center For Behavioral Studies all rights reserved\n"
			"[MegaHALv8][Jason Hutchens]\n"
			"%s\n", timestamp);
	fflush(fp);
}

/*---------------------------------------------------------------------------*/

/*
 *    Function:   write_output
 *
 *    Purpose:    Display the output string.
 */
void
write_output (struct htlc_conn *htlc, char *output)
{
	time_t t;
	char timestamp[1024];
	struct tm *local;

	capitalize(output);
	t = time(0);
	local = localtime(&t);
	strftime(timestamp, 1024, "HAL[%H:%M:%S]", local);

	delay(htlc, output);
 
	status("%s %s\n", timestamp, output);
}
 
/*---------------------------------------------------------------------------*/

/*
 *	Function:	capitalize
 *
 *	Purpose:	Convert a string to look nice.
 */
void
capitalize (char *str)
{
	register unsigned int i;
	bool start = TRUE;

	for (i = 0; i < 3 && str[i]; i++)
		if (isalpha(str[i])) {
			if (start == TRUE)
				str[i] = toupper(str[i]);
			else
				str[i] = tolower(str[i]);
			start = FALSE;
		}

	for (i = 3; str[i]; i++) {
		if (isalpha(str[i])) {
			if (start == TRUE)
				str[i] = toupper(str[i]);
			else
				str[i] = tolower(str[i]);
			start = FALSE;
		}
		if (isspace(str[i])) {
			switch (str[i - 1]) {
				case '?':
				case '.':
				case '!':
					start = TRUE;
			}
		} else {
			switch (str[i]) {
				case '"':
					start = TRUE;
			}
		}
	}
}
 
/*---------------------------------------------------------------------------*/

/*
 *	Function:	upper
 *
 *	Purpose:	Convert a string to its uppercase representation.
 */
void
upper (char *str)
{
	register unsigned int i;

	for (i = 0; str[i]; i++)
		str[i] = toupper(str[i]);
}
 
/*---------------------------------------------------------------------------*/

/*
 *	Function:	write_input
 *
 *	Purpose:	Log the user's input
 */
void
write_input (char *input, char *user)
{
	time_t t;
	char timestamp[1024], tmp[1024];
	struct tm *local;

	t = time(0);
	local = localtime(&t);
	strftime(tmp, 1024, "[%H:%M:%S]", local);
	sprintf(timestamp, "%s%s", user, tmp);

	status("%s %s\n", timestamp, input);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	add_word
 *
 *	Purpose:	Add a word to a dictionary, and return the identifier
 *			assigned to the word.  If the word already exists in
 *			the dictionary, then return its current identifier
 *			without adding it again.
 */
u_int16_t
add_word (DICTIONARY *dictionary, STRING word)
{
	register int i;
	int position;
	bool found;

	/* 
	 *	If the word's already in the dictionary, there is no need to add it
	 */
	position = search_dictionary(dictionary, word, &found);
	if (found == TRUE)
		goto succeed;

	/* 
	 *	Increase the number of words in the dictionary
	 */
	dictionary->size++;

	/*
	 *	Allocate one more entry for the word index
	 */
	dictionary->index = xrealloc(dictionary->index, 2 * dictionary->size);

	/*
	 *	Allocate one more entry for the word array
	 */
	dictionary->entry = xrealloc(dictionary->entry, sizeof(STRING) * dictionary->size);

	/*
	 *	Copy the new word into the word array
	 */
	dictionary->entry[dictionary->size - 1].length = word.length;
	dictionary->entry[dictionary->size - 1].word = xmalloc(word.length);
	memcpy(dictionary->entry[dictionary->size - 1].word, word.word, word.length);

	/*
	 *	Shuffle the word index to keep it sorted alphabetically
	 */
	for (i = dictionary->size - 1; i > position; i--)
		dictionary->index[i] = dictionary->index[i - 1];

	/*
	 *	Copy the new symbol identifier into the word index
	 */
	dictionary->index[position] = dictionary->size - 1;

succeed:
	return dictionary->index[position];
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	search_dictionary
 *
 *	Purpose:	Search the dictionary for the specified word, returning its
 *			position in the index if found, or the position where it
 *			should be inserted otherwise.
 */
int
search_dictionary (DICTIONARY *dictionary, STRING word, bool *find)
{
	int position, min, max, middle, compar;

	/*
	 *	If the dictionary is empty, then obviously the word won't be found
	 */
	if (!dictionary->size) {
		position = 0;
		goto notfound;
	}

	/*
	 *	Initialize the lower and upper bounds of the search
	 */
	min = 0;
	max = dictionary->size - 1;
	/*
	 *	Search repeatedly, halving the search space each time, until either
	 *	the entry is found, or the search space becomes empty
	 */
	while (TRUE) {
		/*
		 *	See whether the middle element of the search space is greater
		 *	than, equal to, or less than the element being searched for.
		 */
		middle = (min + max) / 2;
		compar = wordcmp(word, dictionary->entry[dictionary->index[middle]]);
		/*
		 *	If it is equal then we have found the element.  Otherwise we
		 *	can halve the search space accordingly.
		 */
		if (!compar) {
			position = middle;
			goto found;
		} else if (compar > 0) {
			if (max == middle) {
				position = middle + 1;
				goto notfound;
			}
			min = middle + 1;
		} else {
			if (min == middle) {
				position = middle;
				goto notfound;
			}
			max = middle - 1;
		}
	}

found:
	*find = TRUE;

	return position;

notfound:
	*find = FALSE;

	return position;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	find_word
 *
 *	Purpose:	Return the symbol corresponding to the word specified.
 *			We assume that the word with index zero is equal to a
 *			NULL word, indicating an error condition.
 */
u_int16_t
find_word (DICTIONARY *dictionary, STRING word)
{
	int position;
	bool found;

	position = search_dictionary(dictionary, word, &found);

	if (found == TRUE)
		return dictionary->index[position];
	else
		return 0;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	wordcmp
 *
 *	Purpose:	Compare two words, and return an integer indicating whether
 *			the first word is less than, equal to or greater than the
 *			second word.
 */
int
wordcmp (STRING word1, STRING word2)
{
	register int i;
	int bound;

	bound = MIN(word1.length,word2.length);

	for (i = 0; i < bound; i++)
		if (word1.word[i] != word2.word[i])
			return (int)(word1.word[i] - word2.word[i]);

	if (word1.length < word2.length)
		return -1;
	if (word1.length > word2.length)
		return 1;

	return 0;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	free_dictionary
 *
 *	Purpose:	Release the memory consumed by the dictionary.
 */
void
free_dictionary (DICTIONARY *dictionary)
{
	register u_int32_t i;

	if (!dictionary->size)
		return;
	for (i = 0; i < dictionary->size; i++)
		xfree(dictionary->entry[i].word);
	xfree(dictionary->entry);
	dictionary->entry = 0;
	xfree(dictionary->index);
	dictionary->index = 0;
	dictionary->size = 0;

	initialize_dictionary(dictionary);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	initialize_dictionary
 *
 *	Purpose:	Add dummy words to the dictionary.
 */
void
initialize_dictionary (DICTIONARY *dictionary)
{
	char err[] = "<ERROR>", fin[] = "<FIN>";
	STRING word, end;

	word.word = err;
	word.length = 7;
	end.word = fin;
	end.length = 5;

	add_word(dictionary, word);
	add_word(dictionary, end);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	new_dictionary
 *
 *	Purpose:	Allocate room for a new dictionary.
 */
DICTIONARY *
new_dictionary (void)
{
	DICTIONARY *dictionary;

	dictionary = xmalloc(sizeof(*dictionary));

	dictionary->size = 0;
	dictionary->index = 0;
	dictionary->entry = 0;

	initialize_dictionary(dictionary);

	return dictionary;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	save_dictionary
 *
 *	Purpose:	Save a dictionary to the specified file.
 */
void
save_dictionary (FILE *fp, DICTIONARY *dictionary)
{
	register u_int32_t i;

	fwrite(&(dictionary->size), sizeof(dictionary->size), 1, fp);
	for (i = 0; i < dictionary->size; i++)
		save_word(fp, dictionary->entry[i]);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	load_dictionary
 *
 *	Purpose:	Load a dictionary from the specified file.
 */
void
load_dictionary (FILE *fp, DICTIONARY *dictionary)
{
	register u_int32_t i;
	u_int32_t size;

	fread(&size, sizeof(size), 1, fp);
	for (i = 0; i < size; i++)
		load_word(fp, dictionary);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	save_word
 *
 *	Purpose:	Save a dictionary word to a file.
 */
void
save_word (FILE *fp, STRING word)
{
	fwrite(&(word.length), sizeof(word.length), 1, fp);
	fwrite(word.word, sizeof(*(word.word)), word.length, fp);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	load_word
 *
 *	Purpose:	Load a dictionary word from a file.
 */
void
load_word (FILE *fp, DICTIONARY *dictionary)
{
	char buf[0xff];
	STRING word;

	fread(&(word.length), sizeof(word.length), 1, fp);
	word.word = buf;
	fread(word.word, sizeof(*(word.word)), word.length, fp);
	add_word(dictionary, word);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	new_node
 *
 *	Purpose:	Allocate a new node for the n-gram tree, and initialise
 *			its contents to sensible values.
 */
TREE *
new_node (void)
{
	TREE *node;

	/*
	 *	Allocate memory for the new node
	 */
	node = xmalloc(sizeof(*node));

	/*
	 *	Initialise the contents of the node
	 */
	node->symbol = 0;
	node->usage = 0;
	node->count = 0;
	node->branch = 0;
	node->tree = 0;

	return node;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	new_model
 *
 *	Purpose:	Create and initialise a new ngram model.
 */
MODEL *
new_model (int order)
{
	MODEL *model;

	model = xmalloc(sizeof(*model));

	model->order = order;
	model->forward = new_node();
	model->backward = new_node();
	model->context = xmalloc(sizeof(TREE *) * (order + 2));
	initialize_context(model);
	model->dictionary = new_dictionary();

	return model;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	update_model
 *
 *	Purpose:	Update the model with the specified symbol.
 */
void
update_model (MODEL *model, int symbol)
{
	register u_int16_t i;

	/*
	 *	Update all of the models in the current context with the specified
	 *	symbol.
	 */
	for (i = model->order + 1; i > 0; i--)
		if (model->context[i - 1])
			model->context[i] = add_symbol(model->context[i - 1], (u_int16_t)symbol);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	update_context
 *
 *	Purpose:	Update the context of the model without adding the symbol.
 */
void
update_context (MODEL *model, int symbol)
{
	register u_int16_t i;

	for (i = model->order + 1; i > 0; i--)
		if (model->context[i - 1])
			model->context[i] = find_symbol(model->context[i - 1], symbol);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	add_symbol
 *
 *	Purpose:	Update the statistics of the specified tree with the
 *			specified symbol, which may mean growing the tree if the
 *			symbol hasn't been seen in this context before.
 */
TREE *
add_symbol (TREE *tree, u_int16_t symbol)
{
	TREE *node = 0;

	/*
	 *	Search for the symbol in the subtree of the tree node.
	 */
	node = find_symbol_add(tree, symbol);

	/*
	 *Increment the symbol counts
	 */
	if (node->count < 65535) {
		node->count++;
		tree->usage++;
	}

	return node;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	find_symbol
 *
 *	Purpose:	Return a pointer to the child node, if one exists, which
 *			contains the specified symbol.
 */
TREE *
find_symbol (TREE *node, int symbol)
{
	register int i;
	TREE *found = 0;
	bool found_symbol = FALSE;

	/* 
	 *	Perform a binary search for the symbol.
	 */
	i = search_node(node, symbol, &found_symbol);
	if (found_symbol == TRUE)
		found = node->tree[i];

	return found;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	find_symbol_add
 *
 *	Purpose:	This function is conceptually similar to find_symbol,
 *			apart from the fact that if the symbol is not found,
 *			a new node is automatically allocated and added to the
 *			tree.
 */
TREE *
find_symbol_add (TREE *node, int symbol)
{
	register int i;
	TREE *found = 0;
	bool found_symbol = FALSE;

	/* 
	 *	Perform a binary search for the symbol.  If the symbol isn't found,
	 *	attach a new sub-node to the tree node so that it remains sorted.
	 */
	i = search_node(node, symbol, &found_symbol);
	if (found_symbol == TRUE) {
		found = node->tree[i];
	} else {
		found = new_node();
		found->symbol = symbol;
		add_node(node, found, i);
	}

	return found;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	add_node
 *
 *	Purpose:	Attach a new child node to the sub-tree of the tree
 *			specified.
 */
void
add_node (TREE *tree, TREE *node, int position)
{
	register int i;

	/*
	 *	Allocate room for one more child node, which may mean allocating
	 *	the sub-tree from scratch.
	 */
	tree->tree = xrealloc(tree->tree, sizeof(TREE *) * (tree->branch + 1));

	/*
	 *	Shuffle the nodes down so that we can insert the new node at the
	 *	subtree index given by position.
	 */
	for (i = tree->branch; i > position; i--)
		tree->tree[i] = tree->tree[i - 1];

	/*
	 *	Add the new node to the sub-tree.
	 */
	tree->tree[position] = node;
	tree->branch++;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	search_node
 *
 *	Purpose:	Perform a binary search for the specified symbol on the
 *			subtree of the given node.  Return the position of the
 *			child node in the subtree if the symbol was found, or the
 *			position where it should be inserted to keep the subtree
 *			sorted if it wasn't.
 */
int
search_node (TREE *node, int symbol, bool *found_symbol)
{
	register int position;
	int min, max, middle, compar;

	/*
	 *	Handle the special case where the subtree is empty.
	 */ 
	if (!node->branch) {
		position = 0;
		goto notfound;
	}

	/*
	 *	Perform a binary search on the subtree.
	 */
	min = 0;
	max = node->branch - 1;
	while (TRUE) {
		middle = (min + max) / 2;
		compar = symbol-node->tree[middle]->symbol;
		if (!compar) {
			position = middle;
			goto found;
		} else if (compar > 0) {
			if (max == middle) {
				position = middle + 1;
				goto notfound;
			}
			min = middle + 1;
		} else {
			if (min == middle) {
				position = middle;
				goto notfound;
			}
			max = middle - 1;
		}
	}

found:
	*found_symbol = TRUE;

	return position;

notfound:
	*found_symbol = FALSE;

	return position;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	initialize_context
 *
 *	Purpose:	Set the context of the model to a default value.
 */
void
initialize_context (MODEL *model)
{
	register u_int16_t i;

	for (i = 0; i <= model->order; i++)
		model->context[i] = 0;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	learn
 *
 *	Purpose:	Learn from the user's input.
 */
void
learn (MODEL *model, DICTIONARY *words)
{
	register int i;
	u_int16_t symbol;

	/*
	 *	We only learn from inputs which are long enough
	 */
	if (words->size <= model->order)
		return;

	/*
	 *	Train the model in the forwards direction.  Start by initializing
	 *	the context of the model.
	 */
	initialize_context(model);
	model->context[0] = model->forward;
	for (i = 0; i < (signed)words->size; i++) {
		/*
		 *	Add the symbol to the model's dictionary if necessary, and then
		 *	update the forward model accordingly.
		 */
		symbol = add_word(model->dictionary, words->entry[i]);
		update_model(model, symbol);
	}
	/*
	 *	Add the sentence-terminating symbol.
	 */
	update_model(model, 1);

	/*
	 *	Train the model in the backwards direction.  Start by initializing
	 *	the context of the model.
	 */
	initialize_context(model);
	model->context[0] = model->backward;
	for (i = words->size - 1; i >= 0; --i) {
		/*
		 *	Find the symbol in the model's dictionary, and then update
		 *	the backward model accordingly.
		 */
		symbol = find_word(model->dictionary, words->entry[i]);
		update_model(model, symbol);
	}
	/*
	 *	Add the sentence-terminating symbol.
	 */
	update_model(model, 1);

	return;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	train
 *
 *	Purpose:	Infer a MegaHAL brain from the contents of a text file.
 */
void
train (MODEL *model, const char *filename)
{
	FILE *fp;
	char buffer[1024];
	DICTIONARY *words = 0;

	if (!filename || !(fp = fopen(filename, "r")))
		return;

	words = new_dictionary();

	while (fgets(buffer, sizeof(buffer), fp)) {
		if (buffer[0] == '#')
			continue;
		buffer[strlen(buffer) - 1] = 0;
		upper(buffer);
		make_words(buffer, words);
		learn(model, words);
	}
	fclose(fp);

	xfree(words->entry);
	xfree(words);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	show_dictionary
 *
 *	Purpose:	Display the dictionary for training purposes.
 */
void
show_dictionary (DICTIONARY *dictionary)
{
	register u_int32_t i;
	FILE *fp;

	if (!(fp = fopen("megahal.dic", "w"))) {
		error("show_dictionary", "Unable to open file");
		return;
	}

	for (i = 0; i < dictionary->size; i++)
		fprintf(fp, "%.*s\n", dictionary->entry[i].length, dictionary->entry[i].word);

	fclose(fp);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	save_model
 *
 *	Purpose:	Save the current state to a MegaHAL brain file.
 */
void
save_model (const char *filename, MODEL *model)
{
	FILE *fp;

	show_dictionary(model->dictionary);

	if (!filename || !(fp = fopen(filename, "w"))) {
		error("save_model", "Unable to open file `%s'", filename);
		return;
	}

	fwrite(COOKIE, sizeof(char), sizeof(COOKIE) - 1, fp);
	fwrite(&(model->order), sizeof(model->order), 1, fp);
	save_tree(fp, model->forward);
	save_tree(fp, model->backward);
	save_dictionary(fp, model->dictionary);

	fclose(fp);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	save_tree
 *
 *	Purpose:	Save a tree structure to the specified file.
 */
void
save_tree (FILE *fp, TREE *node)
{
	register u_int16_t i;

	fwrite(&(node->symbol), sizeof(node->symbol), 1, fp);
	fwrite(&(node->usage), sizeof(node->usage), 1, fp);
	fwrite(&(node->count), sizeof(node->count), 1, fp);
	fwrite(&(node->branch), sizeof(node->branch), 1, fp);

	for (i = 0; i < node->branch; i++)
		save_tree(fp, node->tree[i]);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	load_tree
 *
 *	Purpose:	Load a tree structure from the specified file.
 */
void
load_tree (FILE *fp, TREE *node)
{
	register u_int16_t i;

	fread(&(node->symbol), sizeof(node->symbol), 1, fp);
	fread(&(node->usage), sizeof(node->usage), 1, fp);
	fread(&(node->count), sizeof(node->count), 1, fp);
	fread(&(node->branch), sizeof(node->branch), 1, fp);

	if (!node->branch)
		return;

	node->tree = xmalloc(sizeof(TREE *) * node->branch);

	for (i = 0; i < node->branch; i++) {
		node->tree[i] = new_node();
		load_tree(fp, node->tree[i]);
	}
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	load_model
 *
 *	Purpose:	Load a model into memory.
 */
bool
load_model (const char *filename, MODEL *model)
{
	FILE *fp;
	char cookie[sizeof(COOKIE)];

	if (!filename || !(fp = fopen(filename, "r"))) {
		error("load_model", "Unable to open file `%s'", filename);
		return FALSE;
	}

	fread(cookie, sizeof(*cookie), sizeof(COOKIE) - 1, fp);
	if (memcmp(cookie, COOKIE, sizeof(COOKIE) - 1)) {
		error("load_model", "File `%s' is not a MegaHAL brain (bad cookie %.*s)",
			filename, sizeof(COOKIE) - 1, cookie);
		fclose(fp);
		return FALSE;
	}

	fread(&(model->order), sizeof(model->order), 1, fp);
	load_tree(fp, model->forward);
	load_tree(fp, model->backward);
	load_dictionary(fp, model->dictionary);

	fclose(fp);

	return TRUE;
}

static char period_str[] = ".";

/*---------------------------------------------------------------------------*/

/*
 *    Function:   make_words
 *
 *    Purpose:    Break a string into an array of words.
 */
void
make_words (char *input, DICTIONARY *words)
{
	unsigned int offset = 0;

	/*
	 *	Clear the entries in the dictionary
	 */
	words->size = 0;

	/*
	 *	If the string is empty then do nothing, for it contains no words.
	 */
	if (!input[0])
		return;

	/*
	 *	Loop forever.
	 */
	for (;;) {
		/*
		 *	If the current character is of the same type as the previous
		 *	character, then include it in the word.  Otherwise, terminate
		 *	the current word.
		 */
		if (boundary(input, offset)) {
			/*
			 *	Add the word to the dictionary
			 */
			words->entry = xrealloc(words->entry, (words->size + 1) * sizeof(STRING));
			words->entry[words->size].length = offset;
			words->entry[words->size].word = input;
			words->size++;
			if (offset == strlen(input))
				break;
			input += offset;
			offset = 0;
		} else {
			offset++;
		}
	}

	/*
	 *	If the last word isn't punctuation, then replace it with a
	 *	full-stop character.
	 */
	if (isalnum(words->entry[words->size - 1].word[0])) {
		words->entry = xrealloc(words->entry, (words->size + 1) * sizeof(STRING));
		words->entry[words->size].length = 1;
		words->entry[words->size].word = period_str;
		words->size++;
	} else if (!strchr("!.?", words->entry[words->size - 1].word[0])) {
		words->entry[words->size - 1].length = 1;
		words->entry[words->size - 1].word = period_str;
	}
}
 
/*---------------------------------------------------------------------------*/ 
/*
 *	Function:	boundary
 *
 *	Purpose:	Return whether or not a word boundary exists in a string
 *			at the specified location.
 */
bool
boundary (char *str, unsigned int position)
{
	if (!position)
		return FALSE;

	if (position == strlen(str))
		return TRUE;

	if (
		str[position] == '\'' &&
		isalpha(str[position - 1]) &&
		isalpha(str[position + 1])
	)
		return FALSE;

	if (
		position > 1 &&
		str[position - 1] == '\'' &&
		isalpha(str[position - 2]) &&
		isalpha(str[position])
	)
		return FALSE;

	if (
		isalpha(str[position]) &&
		!isalpha(str[position - 1])
	)
		return TRUE;
	
	if (
		!isalpha(str[position]) &&
		isalpha(str[position - 1])
	)
		return TRUE;
	
	if (isdigit(str[position]) != isdigit(str[position - 1]))
		return TRUE;

	return FALSE;
}
 
/*---------------------------------------------------------------------------*/ 
/*
 *	Function:	make_greeting
 *
 *	Purpose:	Put some special words into the dictionary so that the
 *			program will respond as if to a new judge.
 */
void
make_greeting (DICTIONARY *words)
{
	words->size = 0;
	if (g_grt->size > 2)
		add_word(words, g_grt->entry[random() % (g_grt->size - 2) + 2]);
}
 
/*---------------------------------------------------------------------------*/ 
/*
 *    Function:   generate_reply
 *
 *    Purpose:    Take a string of user input and return a string of output
 *                which may vaguely be construed as containing a reply to
 *                whatever is in the input string.
 */
char *
generate_reply (MODEL *model, DICTIONARY *words)
{
	static DICTIONARY *dummy = 0;
	DICTIONARY *replywords, *keywords;
	float surprise, max_surprise;
	char *output = 0;
	int count, basetime;

	/*
	 *	Create an array of keywords from the words in the user's input
	 */
	keywords = make_keywords(model, words);

	/*
	 *	Make sure some sort of reply exists
	 */
	if (!dummy)
		dummy = new_dictionary();
	replywords = reply(model, dummy);
	if (dissimilar(words, replywords) == TRUE)
		output = make_output(replywords);

	/*
	 *	Loop for the specified waiting period, generating and evaluating
	 *	replies
	 */
	max_surprise = (float)-1.0;
	count = 0;
	basetime = time(0);
	do {
		replywords = reply(model, keywords);
		surprise = evaluate_reply(model, keywords, replywords);
		++count;
		if ((surprise > max_surprise) && (dissimilar(words, replywords) == TRUE)) {
			max_surprise = surprise;
			output = make_output(replywords);
		}
	} while ((time(0) - basetime) < TIMEOUT);

	/*
	 *	Return the best answer we generated
	 */
	return output ? output : xstrdup("I forgot what i was gonna say!");
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	dissimilar
 *
 *	Purpose:	Return TRUE or FALSE depending on whether the dictionaries
 *			are the same or not.
 */
bool
dissimilar (DICTIONARY *words1, DICTIONARY *words2)
{
	register unsigned int i;

	if (words1->size != words2->size)
		return TRUE;
	for (i = 0; i < words1->size; i++)
		if (wordcmp(words1->entry[i], words2->entry[i]))
			return TRUE;

	return FALSE;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	make_keywords
 *
 *	Purpose:	Put all the interesting words from the user's input into
 *			a keywords dictionary, which will be used when generating
 *			a reply.
 */
DICTIONARY *
make_keywords (MODEL *model, DICTIONARY *words)
{
	static DICTIONARY *keys = 0;
	register unsigned int i, j;
	int c;

	if (!keys)
		keys = new_dictionary();
	else
		free_dictionary(keys);

	for (i = 0; i < words->size; i++) {
		/*
		 *	Find the symbol ID of the word.  If it doesn't exist in
		 *	the model, or if it begins with a non-alphanumeric
		 *	character, or if it is in the exclusion array, then
		 *	skip over it.
		 */
		c = 0;
		for (j = 0; j < g_swp->size; j++)
			if (!wordcmp(g_swp->from[j], words->entry[i])) {
				add_key(model, keys, g_swp->to[j]);
				c = 1;
			}
		if (!c)
			add_key(model, keys, words->entry[i]);
	}

	if (keys->size > 2)
		for (i = 0; i < words->size; i++) {
			c = 0;
			for (j = 0; j < g_swp->size; j++)
				if (!wordcmp(g_swp->from[j], words->entry[i])) {
					add_aux(model, keys, g_swp->to[j]);
					c = 1;
				}
			if (!c)
				add_aux(model, keys, words->entry[i]);
	}

	return keys;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	add_key
 *
 *	Purpose:	Add a word to the keyword dictionary.
 */
void
add_key (MODEL *model, DICTIONARY *keys, STRING word)
{
	int symbol;

	symbol = find_word(model->dictionary, word);
	if (!symbol)
		return;
	if (!isalnum(word.word[0]))
		return;
	symbol = find_word(g_ban, word);
	if (symbol)
		return;
	symbol = find_word(g_aux, word);
	if (symbol)
		return;

	add_word(keys, word);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	add_aux
 *
 *	Purpose:	Add an auxilliary keyword to the keyword dictionary.
 */
void
add_aux (MODEL *model, DICTIONARY *keys, STRING word)
{
	int symbol;

	symbol = find_word(model->dictionary, word);
	if (!symbol)
		return;
	if (!isalnum(word.word[0]))
		return;
	symbol = find_word(g_aux, word);
	if (!symbol)
		return;
	add_word(keys, word);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	reply
 *
 *	Purpose:	Generate a dictionary of reply words appropriate to the
 *			given dictionary of keywords.
 */
DICTIONARY *
reply (MODEL *model, DICTIONARY *keys)
{
	DICTIONARY *replies;
	register int i;
	int symbol;
	bool start = TRUE;

	replies = new_dictionary();
	replies->size = 0;

	/*
	 *	Start off by making sure that the model's context is empty.
	 */
	initialize_context(model);
	model->context[0] = model->forward;
	used_key = FALSE;

	/*
	 *	Generate the reply in the forward direction.
	 */
	while (TRUE) {
		/*
		 *	Get a random symbol from the current context.
		 */
		if (start == TRUE)
			symbol = seed(model, keys);
		else
			symbol = babble(model, keys, replies);
		if (!symbol || symbol == 1)
			break;
		start = FALSE;

		/*
		 *	Append the symbol to the reply dictionary.
		 */
		replies->entry = xrealloc(replies->entry, (replies->size + 1) * sizeof(STRING));

		replies->entry[replies->size].length =
			model->dictionary->entry[symbol].length;
		replies->entry[replies->size].word =
			model->dictionary->entry[symbol].word;
		replies->size++;

		/*
		 *	Extend the current context of the model with the current symbol.
		 */
		update_context(model, symbol);
	}

	/*
	 *	Start off by making sure that the model's context is empty.
	 */
	initialize_context(model);
	model->context[0] = model->backward;

	/*
	 *	Re-create the context of the model from the current reply
	 *	dictionary so that we can generate backwards to reach the
	 *	beginning of the string.
	 */
	if (replies->size > 0)
		for (i = MIN(replies->size - 1, model->order); i >= 0; --i) {
			symbol = find_word(model->dictionary, replies->entry[i]);
			update_context(model, symbol);
		}

	/*
	 *	Generate the reply in the backward direction.
	 */
	while (TRUE) {
		/*
		 *	Get a random symbol from the current context.
		 */
		symbol = babble(model, keys, replies);
		if (!symbol || symbol == 1)
			break;

		/*
		 *	Prepend the symbol to the reply dictionary.
		 */
		replies->entry = xrealloc(replies->entry, (replies->size + 1) * sizeof(STRING));

		/*
		 *	Shuffle everything up for the prepend.
		 */
		for (i = replies->size; i > 0; --i) {
			replies->entry[i].length = replies->entry[i - 1].length;
			replies->entry[i].word = replies->entry[i - 1].word;
		}

		replies->entry[0].length = model->dictionary->entry[symbol].length;
		replies->entry[0].word = model->dictionary->entry[symbol].word;
		replies->size++;

		/*
		 *	Extend the current context of the model with the current symbol.
		 */
		update_context(model, symbol);
	}

	return replies;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	evaluate_reply
 *
 *	Purpose:	Measure the average surprise of keywords relative to the
 *			language model.
 */
float
evaluate_reply (MODEL *model, DICTIONARY *keys, DICTIONARY *words)
{
	register unsigned int j;
	register int i;
	int symbol, count, num = 0;
	float probability, entropy = (float)0.0;
	TREE *node;

	if (words->size <= 0)
		return (float)0.0;
	initialize_context(model);
	model->context[0] = model->forward;
	for (i = 0; i < (signed)words->size; i++) {
		symbol = find_word(model->dictionary, words->entry[i]);
		if (find_word(keys, words->entry[i])) {
			probability = (float)0.0;
			count = 0;
			++num;
			for (j = 0; j < model->order; j++)
				if (model->context[j]) {
					node = find_symbol(model->context[j], symbol);
					probability += (float)(node->count) /
						(float)(model->context[j]->usage);
					++count;
				}
			if (count > 0.0)
				entropy -= (float)log(probability / (float)count);
		}
		update_context(model, symbol);
	}

	initialize_context(model);
	model->context[0] = model->backward;
	for (i = words->size - 1; i >= 0; --i) {
		symbol = find_word(model->dictionary, words->entry[i]);
		if (find_word(keys, words->entry[i])) {
			probability = (float)0.0;
			count = 0;
			++num;
			for (j = 0; j < model->order; j++)
				if (model->context[j]) {
					node = find_symbol(model->context[j], symbol);
					probability += (float)(node->count) /
						(float)(model->context[j]->usage);
					++count;
				}
			if (count > 0.0)
				entropy -= (float)log(probability / (float)count);
		}
		update_context(model, symbol);
	}

	if (num >= 8)
		entropy /= (float)sqrt(num - 1);
	if (num >= 16)
		entropy /= (float)num;

	return entropy;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	make_output
 *
 *	Purpose:	Generate a string from the dictionary of reply words.
 */
char *
make_output (DICTIONARY *words)
{
	char *output = 0;
	register unsigned int i, len;

	if (!words->size)
		return xstrdup("I am utterly speechless!");

	len = 1;
	for (i = 0; i < words->size; i++)
		len += words->entry[i].length;
	output = xmalloc(len + 1);
	len = 0;
	for (i = 0; i < words->size; i++) {
		memcpy(&(output[len]), words->entry[i].word, words->entry[i].length);
		len += words->entry[i].length;
	}
	output[len] = 0;

	return output;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	babble
 *
 *	Purpose:	Return a random symbol from the current context, or a
 *			zero symbol identifier if we've reached either the
 *			start or end of the sentence.  Select the symbol based
 *			on probabilities, favouring keywords.  In all cases,
 *			use the longest available context to choose the symbol.
 */
int
babble (MODEL *model, DICTIONARY *keys, DICTIONARY *words)
{
	TREE *node = 0;
	register int i;
	int count, symbol = 0;

	/*
	 *	Select the longest available context.
	 */
	for (i = 0; i <= model->order; i++)
		if (model->context[i])
			node = model->context[i];

	if (!node || !node->branch)
		return 0;

	/*
	 *	Choose a symbol at random from this context.
	 */
	i = random() % node->branch;
	count = random() % node->usage;
	while (count >= 0) {
		/*
		 *	If the symbol occurs as a keyword, then use it.  Only use an
		 *	auxilliary keyword if a normal keyword has already been used.
		 */
		symbol = node->tree[i]->symbol;

		if (
			(find_word(keys, model->dictionary->entry[symbol])) &&
			(used_key == TRUE ||
			(!find_word(g_aux, model->dictionary->entry[symbol]))) &&
			(word_exists(words, model->dictionary->entry[symbol]) == FALSE)
		) {
			used_key = TRUE;
			break;
		}
		count -= node->tree[i]->count;
		i = i >= (node->branch - 1) ? 0 : i + 1;
	}

	return symbol;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	word_exists
 *
 *	Purpose:	A silly brute-force searcher for the reply string.
 */
bool
word_exists (DICTIONARY *dictionary, STRING word)
{
	register u_int32_t i;

	for (i = 0; i < dictionary->size; i++)
		if (!wordcmp(dictionary->entry[i], word))
			return TRUE;

	return FALSE;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	seed
 *
 *	Purpose:	Seed the reply by guaranteeing that it contains a
 *			keyword, if one exists.
 */
int
seed (MODEL *model, DICTIONARY *keys)
{
	register unsigned int i, stop;
	int symbol;

	if (!model->context[0]->branch)
		symbol = 0;
	else
		symbol = random() % model->context[0]->branch;

	if (keys->size > 2) {
		do {
			i = random() % keys->size;
		} while (i < 2);
		stop = i;
		while (TRUE) {
			if (
				(find_word(model->dictionary, keys->entry[i])) &&
				(!find_word(g_aux, keys->entry[i]))
			) {
				symbol = find_word(model->dictionary, keys->entry[i]);
				return symbol;
			}
			i++;
			if (i == keys->size)
				i = 2;
			if (i == stop)
				return symbol;
		}
	}

	return symbol;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	new_swap
 *
 *	Purpose:	Allocate a new swap structure.
 */
SWAP *
new_swap (void)
{
	SWAP *list;

	list = xmalloc(sizeof(*list));
	list->size = 0;
	list->from = 0;
	list->to = 0;

	return list;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	add_swap
 *
 *	Purpose:	Add a new entry to the swap structure.
 */
void
add_swap (SWAP *list, char *s, char *d)
{
	list->size++;

	list->from = xrealloc(list->from, sizeof(STRING) * (list->size));
	list->to = xrealloc(list->to, sizeof(STRING) * (list->size));

	list->from[list->size - 1].length = strlen(s);
	list->from[list->size - 1].word = xstrdup(s);
	list->to[list->size - 1].length = strlen(d);
	list->to[list->size - 1].word = xstrdup(d);
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	initialize_swap
 *
 *	Purpose:	Read a swap structure from a file.
 */
SWAP *
initialize_swap (const char *filename)
{
	SWAP *list;
	FILE *fp;
	char buffer[1024], *from, *to;

	list = new_swap();

	if (!filename || !(fp = fopen(filename, "r")))
		return list;

	while (fgets(buffer, sizeof(buffer), fp)) {
		if (buffer[0] == '#')
			continue;
		from = strtok(buffer, "\t ");
		to = strtok(0, "\t \n#");
		add_swap(list, from, to);
	}

	fclose(fp);

	return list;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	initialize_list
 *
 *	Purpose:	Read a dictionary from a file.
 */
DICTIONARY
*initialize_list (const char *filename)
{
	DICTIONARY *list;
	FILE *fp;
	STRING word;
	char *string, buffer[1024];

	list = new_dictionary();

	if (!filename || !(fp = fopen(filename, "r")))
		return list;

	while (fgets(buffer, sizeof(buffer), fp)) {
		if (buffer[0] == '#')
			continue;
		string = strtok(buffer, "\t \n#");
		if (string && string[0]) {
			word.length = strlen(string);
			word.word = xstrdup(buffer);
			add_word(list, word);
		}
	}

	fclose(fp);

	return list;
}

/*---------------------------------------------------------------------------*/

/*
 *	Function:	delay
 *
 *	Purpose:	Display the string to stdout as if it was typed by a human.
 */
void
delay (struct htlc_conn *htlc, char *str)
{
	register int i;
	register char *out;

	/*
	 *	Don't simulate typing if the feature is turned off
	 */
	if (typing_delay == FALSE) {
		hx_send_chat(htlc, 0, str, strlen(str), 0);
		return;
	}

	out = xmalloc(strlen(str) + 1);
	for (i = 0; str[i]; i++) {
		/*
		 *	Standard keyboard delay
		 */
#if defined(__WIN32__)
		sleep((D_KEY + random() % V_KEY - random() % V_KEY)/1000000+1);
#else
		usleep(D_KEY + random() % V_KEY - random() % V_KEY);
#endif
		out[i] = str[i];

		/*
		 *	A random thinking delay
		 */
		if ((!isalnum(str[i])) && ((random() % 100) < P_THINK))
#if defined(__WIN32__)
			sleep((D_THINK + random() % V_THINK - random() % V_THINK)/1000000+1);
#else
			usleep(D_THINK + random() % V_THINK - random() % V_THINK);
#endif
	}
	out[i] = 0;
	hx_send_chat(htlc, 0, out, i, 0);
	xfree(out);
}

/*===========================================================================*/

/*
 *	Revision 1.8  1997/12/24 03:17:01  hutch
 *	More bug fixes, and hopefully the final contest version!
 *
 *	Revision 1.7  1997/12/22  13:18:09  hutch
 *	A few more bug fixes, and non-repeating implemented.
 *
 *	Revision 1.6  1997/12/22 04:27:04  hutch
 *	A few minor bug fixes.
 *
 *	Revision 1.5  1997/12/15 04:35:59  hutch
 *	Final Loebner version!
 *
 *	Revision 1.4  1997/12/11 05:45:29  hutch
 *	the almost finished version.
 *
 *	Revision 1.3  1997/12/10 09:08:09  hutch
 *	Now Loebner complient (tm)
 *
 *	Revision 1.2  1997/12/08 06:22:32  hutch
 *	Tidied up.
 *
 *	Revision 1.1  1997/12/05  07:11:44  hutch
 *	Initial revision
 *
 *	Revision 1.7  1997/12/04 07:07:13  hutch
 *	Added load and save functions, and tidied up some code/
 *
 *	Revision 1.6  1997/12/02 08:34:47  hutch
 *	Added the ban, aux and swp functions.
 *
 *	Revision 1.5  1997/12/02 06:03:04  hutch
 *	Updated to use a special terminating symbol, and to store only
 *	branches of maximum depth, as they are the only ones used in
 *	the reply.
 *
 *	Revision 1.4  1997/10/28 09:23:12  hutch
 *	MegaHAL is babbling nicely, but without keywords.
 *
 *	Revision 1.3  1997/10/15  09:04:03  hutch
 *	MegaHAL can parrot back whatever the user says.
 *
 *	Revision 1.2  1997/07/21 04:03:28  hutch
 *	Fully working.
 *
 *	Revision 1.1  1997/07/15 01:55:25  hutch
 *	Initial revision
 *
 *	Revision 1.1  1997/07/15 01:54:21  hutch
 *	Initial revision
 */

/*===========================================================================*/

