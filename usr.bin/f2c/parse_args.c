/****************************************************************
Copyright 1990, 1994-5 by AT&T, Lucent Technologies and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T, Bell Laboratories,
Lucent or Bellcore or any of their entities not be used in
advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

AT&T, Lucent and Bellcore disclaim all warranties with regard to
this software, including all implied warranties of
merchantability and fitness.  In no event shall AT&T, Lucent or
Bellcore be liable for any special, indirect or consequential
damages or any damages whatsoever resulting from loss of use,
data or profits, whether in an action of contract, negligence or
other tortious action, arising out of or in connection with the
use or performance of this software.
****************************************************************/

/* parse_args

	This function will parse command line input into appropriate data
   structures, output error messages when appropriate and provide some
   minimal type conversion.

	Input to the function consists of the standard   argc,argv
   values, and a table which directs the parser.  Each table entry has the
   following components:

	prefix -- the (optional) switch character string, e.g. "-" "/" "="
	switch -- the command string, e.g. "o" "data" "file" "F"
	flags -- control flags, e.g.   CASE_INSENSITIVE, REQUIRED_PREFIX
	arg_count -- number of arguments this command requires, e.g. 0 for
		     booleans, 1 for filenames, INFINITY for input files
	result_type -- how to interpret the switch arguments, e.g. STRING,
		       CHAR, FILE, OLD_FILE, NEW_FILE
	result_ptr -- pointer to storage for the result, be it a table or
		      a string or whatever
	table_size -- if the arguments fill a table, the maximum number of
		      entries; if there are no arguments, the value to
		      load into the result storage

	Although the table can be used to hold a list of filenames, only
   scalar values (e.g. pointers) can be stored in the table.  No vector
   processing will be done, only pointers to string storage will be moved.

	An example entry, which could be used to parse input filenames, is:

	"-", "o", 0, oo, OLD_FILE, infilenames, INFILE_TABLE_SIZE

*/

#include <stdio.h>
#ifndef NULL
/* ANSI C */
#include <stddef.h>
#endif
#ifdef KR_headers
extern double atof();
#else
#include "stdlib.h"
#include "string.h"
#endif
#include "parse.h"
#include <math.h>	     /* For atof */
#include <ctype.h>

#define MAX_INPUT_SIZE 1000

#define arg_prefix(x) ((x).prefix)
#define arg_string(x) ((x).string)
#define arg_flags(x) ((x).flags)
#define arg_count(x) ((x).count)
#define arg_result_type(x) ((x).result_type)
#define arg_result_ptr(x) ((x).result_ptr)
#define arg_table_size(x) ((x).table_size)

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
typedef int boolean;


static char *this_program = "";

static int arg_parse Argdcl((char*, arg_info*));
static char *lower_string Argdcl((char*, char*));
static int match Argdcl((char*, char*, arg_info*, boolean));
static int put_one_arg Argdcl((int, char*, char**, char*, char*));
extern int badargs;


 boolean
#ifdef KR_headers
parse_args(argc, argv, table, entries, others, other_count)
	int argc;
	char **argv;
	arg_info *table;
	int entries;
	char **others;
	int other_count;
#else
parse_args(int argc, char **argv, arg_info *table, int entries, char **others, int other_count)
#endif
{
    boolean result;

    if (argv)
	this_program = argv[0];

/* Check the validity of the table and its parameters */

    result = arg_verify (argv, table, entries);

/* Initialize the storage values */

    init_store (table, entries);

    if (result) {
	boolean use_prefix = TRUE;
	char *argv0;

	argc--;
	argv0 = *++argv;
	while (argc) {
	    int index, length;

	    index = match_table (*argv, table, entries, use_prefix, &length);
	    if (index < 0) {

/* The argument doesn't match anything in the table */

		if (others) {

		    if (*argv > argv0)
			*--*argv = '-';	/* complain at invalid flag */

		    if (other_count > 0) {
			*others++ = *argv;
			other_count--;
		    } else {
			fprintf (stderr, "%s:  too many parameters: ",
				this_program);
			fprintf (stderr, "'%s' ignored\n", *argv);
			badargs++;
		    } /* else */
		} /* if (others) */
		argv0 = *++argv;
		argc--;
	    } else {

/* A match was found */

		if (length >= strlen (*argv)) {
		    argc--;
		    argv0 = *++argv;
		    use_prefix = TRUE;
		} else {
		    (*argv) += length;
		    use_prefix = FALSE;
		} /* else */

/* Parse any necessary arguments */

		if (arg_count (table[index]) != P_NO_ARGS) {

/* Now   length   will be used to store the number of parsed characters */

		    length = arg_parse(*argv, &table[index]);
		    if (*argv == NULL)
			argc = 0;
		    else if (length >= strlen (*argv)) {
			argc--;
			argv0 = *++argv;
			use_prefix = TRUE;
		    } else {
			(*argv) += length;
			use_prefix = FALSE;
		    } /* else */
		} /* if (argv_count != P_NO_ARGS) */
		  else
		    *arg_result_ptr(table[index]) =
			    arg_table_size(table[index]);
	    } /* else */
	} /* while (argc) */
    } /* if (result) */

    return result;
} /* parse_args */


 boolean
#ifdef KR_headers
arg_verify(argv, table, entries)
	char **argv;
	arg_info *table;
	int entries;
#else
arg_verify(char **argv, arg_info *table, int entries)
#endif
{
    int i;
    char *this_program = "";

    if (argv)
	this_program = argv[0];

    for (i = 0; i < entries; i++) {
	arg_info *arg = &table[i];

/* Check the argument flags */

	if (arg_flags (*arg) & ~(P_CASE_INSENSITIVE | P_REQUIRED_PREFIX)) {
	    fprintf (stderr, "%s [arg_verify]:  too many ", this_program);
	    fprintf (stderr, "flags in entry %d:  '%x' (hex)\n", i,
		    arg_flags (*arg));
	    badargs++;
	} /* if */

/* Check the argument count */

	{ int count = arg_count (*arg);

	    if (count != P_NO_ARGS && count != P_ONE_ARG && count !=
		    P_INFINITE_ARGS) {
		fprintf (stderr, "%s [arg_verify]:  invalid ", this_program);
		fprintf (stderr, "argument count in entry %d:  '%d'\n", i,
			count);
		badargs++;
	    } /* if count != P_NO_ARGS ... */

/* Check the result field; want to be able to store results */

	      else
		if (arg_result_ptr (*arg) == (int *) NULL) {
		    fprintf (stderr, "%s [arg_verify]:  ", this_program);
		    fprintf (stderr, "no argument storage given for ");
		    fprintf (stderr, "entry %d\n", i);
		    badargs++;
		} /* if arg_result_ptr */
	}

/* Check the argument type */

	{ int type = arg_result_type (*arg);

	    if (type < P_STRING || type > P_DOUBLE) {
		    fprintf(stderr,
			"%s [arg_verify]:  bad arg type in entry %d:  '%d'\n",
			this_program, i, type);
		    badargs++;
		    }
	}

/* Check table size */

	{ int size = arg_table_size (*arg);

	    if (arg_count (*arg) == P_INFINITE_ARGS && size < 1) {
		fprintf (stderr, "%s [arg_verify]:  bad ", this_program);
		fprintf (stderr, "table size in entry %d:  '%d'\n", i,
			size);
		badargs++;
	    } /* if (arg_count == P_INFINITE_ARGS && size < 1) */
	}

    } /* for i = 0 */

    return TRUE;
} /* arg_verify */


/* match_table -- returns the index of the best entry matching the input,
   -1 if no match.  The best match is the one of longest length which
   appears lowest in the table.  The length of the match will be returned
   in   length   ONLY IF a match was found.   */

 int
#ifdef KR_headers
match_table(norm_input, table, entries, use_prefix, length)
	register char *norm_input;
	arg_info *table;
	int entries;
	boolean use_prefix;
	int *length;
#else
match_table(register char *norm_input, arg_info *table, int entries, boolean use_prefix, int *length)
#endif
{
    char low_input[MAX_INPUT_SIZE];
    register int i;
    int best_index = -1, best_length = 0;

/* FUNCTION BODY */

    (void) lower_string (low_input, norm_input);

    for (i = 0; i < entries; i++) {
	int this_length = match(norm_input, low_input, &table[i], use_prefix);

	if (this_length > best_length) {
	    best_index = i;
	    best_length = this_length;
	} /* if (this_length > best_length) */
    } /* for (i = 0) */

    if (best_index > -1 && length != (int *) NULL)
	*length = best_length;

    return best_index;
} /* match_table */


/* match -- takes an input string and table entry, and returns the length
   of the longer match.

	0 ==> input doesn't match

   For example:

	INPUT	PREFIX	STRING	RESULT
----------------------------------------------------------------------
	"abcd"	"-"	"d"	0
	"-d"	"-"	"d"	2    (i.e. "-d")
	"dout"	"-"	"d"	1    (i.e. "d")
	"-d"	""	"-d"	2    (i.e. "-d")
	"dd"	"d"	"d"	2	<= here's the weird one
*/

 static int
#ifdef KR_headers
match(norm_input, low_input, entry, use_prefix)
	char *norm_input;
	char *low_input;
	arg_info *entry;
	boolean use_prefix;
#else
match(char *norm_input, char *low_input, arg_info *entry, boolean use_prefix)
#endif
{
    char *norm_prefix = arg_prefix (*entry);
    char *norm_string = arg_string (*entry);
    boolean prefix_match = FALSE, string_match = FALSE;
    int result = 0;

/* Buffers for the lowercased versions of the strings being compared.
   These are used when the switch is to be case insensitive */

    static char low_prefix[MAX_INPUT_SIZE];
    static char low_string[MAX_INPUT_SIZE];
    int prefix_length = strlen (norm_prefix);
    int string_length = strlen (norm_string);

/* Pointers for the required strings (lowered or nonlowered) */

    register char *input, *prefix, *string;

/* FUNCTION BODY */

/* Use the appropriate strings to handle case sensitivity */

    if (arg_flags (*entry) & P_CASE_INSENSITIVE) {
	input = low_input;
	prefix = lower_string (low_prefix, norm_prefix);
	string = lower_string (low_string, norm_string);
    } else {
	input = norm_input;
	prefix = norm_prefix;
	string = norm_string;
    } /* else */

/* First, check the string formed by concatenating the prefix onto the
   switch string, but only when the prefix is not being ignored */

    if (use_prefix && prefix != NULL && *prefix != '\0')
	 prefix_match = (strncmp (input, prefix, prefix_length) == 0) &&
		(strncmp (input + prefix_length, string, string_length) == 0);

/* Next, check just the switch string, if that's allowed */

    if (!use_prefix && (arg_flags (*entry) & P_REQUIRED_PREFIX) == 0)
	string_match = strncmp (input, string, string_length) == 0;

    if (prefix_match)
	result = prefix_length + string_length;
    else if (string_match)
	result = string_length;

    return result;
} /* match */


 static char *
#ifdef KR_headers
lower_string(dest, src)
	char *dest;
	char *src;
#else
lower_string(char *dest, char *src)
#endif
{
    char *result = dest;
    register int c;

    if (dest == NULL || src == NULL)
	result = NULL;
    else
	while (*dest++ = (c = *src++) >= 'A' && c <= 'Z' ? tolower(c) : c);

    return result;
} /* lower_string */


/* arg_parse -- returns the number of characters parsed for this entry */

 static int
#ifdef KR_headers
arg_parse(str, entry)
	char *str;
	arg_info *entry;
#else
arg_parse(char *str, arg_info *entry)
#endif
{
    int length = 0;

    if (arg_count (*entry) == P_ONE_ARG) {
	char **store = (char **) arg_result_ptr (*entry);

	length = put_one_arg (arg_result_type (*entry), str, store,
		arg_prefix (*entry), arg_string (*entry));

    } /* if (arg_count == P_ONE_ARG) */
      else { /* Must be a table of arguments */
	char **store = (char **) arg_result_ptr (*entry);

	if (store) {
	    while (*store)
		store++;

	    length = put_one_arg(arg_result_type (*entry), str, store++,
		    arg_prefix (*entry), arg_string (*entry));

	    *store = (char *) NULL;
	} /* if (store) */
    } /* else */

    return length;
} /* arg_parse */


 static int
#ifdef KR_headers
put_one_arg(type, str, store, prefix, string)
	int type;
	char *str;
	char **store;
	char *prefix;
	char *string;
#else
put_one_arg(int type, char *str, char **store, char *prefix, char *string)
#endif
{
    int length = 0;
    long L;

    if (store) {
	switch (type) {
	    case P_STRING:
	    case P_FILE:
	    case P_OLD_FILE:
	    case P_NEW_FILE:
		if (str == NULL) {
			fprintf(stderr, "%s: Missing argument after '%s%s'\n",
				this_program, prefix, string);
			length = 0;
			badargs++;
			}
		else
			length = strlen(*store = str);
		break;
	    case P_CHAR:
		*((char *) store) = *str;
		length = 1;
		break;
	    case P_SHORT:
		L = atol(str);
		*(short *)store = (short) L;
		if (L != *(short *)store) {
		    fprintf(stderr,
	"%s%s parameter '%ld' is not a SHORT INT (truncating to %d)\n",
			    prefix, string, L, *(short *)store);
		    badargs++;
		    }
		length = strlen (str);
		break;
	    case P_INT:
		L = atol(str);
		*(int *)store = (int)L;
		if (L != *(int *)store) {
		    fprintf(stderr,
	"%s%s parameter '%ld' is not an INT (truncating to %d)\n",
			    prefix, string, L, *(int *)store);
		    badargs++;
		    }
		length = strlen (str);
		break;
	    case P_LONG:
		*(long *)store = atol(str);
		length = strlen (str);
		break;
	    case P_FLOAT:
		*((float *) store) = (float) atof(str);
		length = strlen (str);
		break;
	    case P_DOUBLE:
		*((double *) store) = (double) atof(str);
		length = strlen (str);
		break;
	    default:
		fprintf (stderr, "put_one_arg:  bad type '%d'\n", type);
		badargs++;
		break;
	} /* switch */
    } /* if (store) */

    return length;
} /* put_one_arg */


 void
#ifdef KR_headers
init_store(table, entries)
	arg_info *table;
	int entries;
#else
init_store(arg_info *table, int entries)
#endif
{
    int index;

    for (index = 0; index < entries; index++)
	if (arg_count (table[index]) == P_INFINITE_ARGS) {
	    char **place = (char **) arg_result_ptr (table[index]);

	    if (place)
		*place = (char *) NULL;
	} /* if arg_count == P_INFINITE_ARGS */

} /* init_store */
