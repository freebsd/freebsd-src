/****************************************************************
Copyright 1990, 1991, 1993 by AT&T Bell Laboratories and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T Bell Laboratories or
Bellcore or any of their entities not be used in advertising or
publicity pertaining to distribution of the software without
specific, written prior permission.

AT&T and Bellcore disclaim all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall AT&T or Bellcore be liable for
any special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.
****************************************************************/

#include "defs.h"
#include "names.h"
#include "output.h"

#define TOO_LONG_INDENT (2 * tab_size)
#define MAX_INDENT 44
#define MIN_INDENT 22
static int last_was_newline = 0;
int indent = 0;
int in_comment = 0;
int in_define = 0;
 extern int gflag1;
 extern char *file_name;

 static int
write_indent(fp, use_indent, extra_indent, start, end)
 FILE *fp;
 int use_indent, extra_indent;
 char *start, *end;
{
    int ind, tab;

    if (gflag1 && last_was_newline)
	fprintf(fp, "#line %ld \"%s\"\n", lineno, infname ? infname : file_name);
    if (in_define == 1) {
	in_define = 2;
	use_indent = 0;
	}
    if (last_was_newline && use_indent) {
	if (*start == '\n') do {
		putc('\n', fp);
		if (++start > end)
			return;
		}
		while(*start == '\n');

	ind = indent <= MAX_INDENT
		? indent
		: MIN_INDENT + indent % (MAX_INDENT - MIN_INDENT);

	tab = ind + extra_indent;

	while (tab > 7) {
	    putc ('\t', fp);
	    tab -= 8;
	} /* while */

	while (tab-- > 0)
	    putc (' ', fp);
    } /* if last_was_newline */

    while (start <= end)
	putc (*start++, fp);
} /* write_indent */


/*VARARGS2*/
int margin_printf (fp, a, b, c, d, e, f, g)
FILE *fp;
char *a;
long b, c, d, e, f, g;
{
    ind_printf (0, fp, a, b, c, d, e, f, g);
} /* margin_printf */

/*VARARGS2*/
int nice_printf (fp, a, b, c, d, e, f, g)
FILE *fp;
char *a;
long b, c, d, e, f, g;
{
    ind_printf (1, fp, a, b, c, d, e, f, g);
} /* nice_printf */


#define  max_line_len c_output_line_length
 		/* 74Number of characters allowed on an output
			           line.  This assumes newlines are handled
			           nicely, i.e. a newline after a full text
			           line on a terminal is ignored */

/* output_buf   holds the text of the next line to be printed.  It gets
   flushed when a newline is printed.   next_slot   points to the next
   available location in the output buffer, i.e. where the next call to
   nice_printf will have its output stored */

static char *output_buf;
static char *next_slot;
static char *string_start;

static char *word_start = NULL;
static int cursor_pos = 0;
static int In_string = 0;

 void
np_init()
{
	next_slot = output_buf = Alloc(MAX_OUTPUT_SIZE);
	memset(output_buf, 0, MAX_OUTPUT_SIZE);
	}

 static char *
adjust_pointer_in_string(pointer)
 register char *pointer;
{
	register char *s, *s1, *se, *s0;

	/* arrange not to break \002 */
	s1 = string_start ? string_start : output_buf;
	for(s = s1; s < pointer; s++) {
		s0 = s1;
		s1 = s;
		if (*s == '\\') {
			se = s++ + 4;
			if (se > pointer)
				break;
			if (*s < '0' || *s > '7')
				continue;
			while(++s < se)
				if (*s < '0' || *s > '7')
					break;
			--s;
			}
		}
	return s0 - 1;
	}

/* ANSI says strcpy's behavior is undefined for overlapping args,
 * so we roll our own fwd_strcpy: */

 static void
fwd_strcpy(t, s)
 register char *t, *s;
{ while(*t++ = *s++); }

/* isident -- true iff character could belong to a unit.  C allows
   letters, numbers and underscores in identifiers.  This also doubles as
   a check for numeric constants, since we include the decimal point and
   minus sign.  The minus has to be here, since the constant "10e-2"
   cannot be broken up.  The '.' also prevents structure references from
   being broken, which is a quite acceptable side effect */

#define isident(x) (Tr[x] & 1)
#define isntident(x) (!Tr[x])

int ind_printf (use_indent, fp, a, b, c, d, e, f, g)
int use_indent;
FILE *fp;
char *a;
long b, c, d, e, f, g;
{
    extern int max_line_len;
    extern FILEP c_file;
    extern char tr_tab[];	/* in output.c */
    register char *Tr = tr_tab;
    int ch, inc, ind;
    static int extra_indent, last_indent, set_cursor = 1;

    cursor_pos += indent - last_indent;
    last_indent = indent;
    sprintf (next_slot, a, b, c, d, e, f, g);

    if (fp != c_file) {
	fprintf (fp,"%s", next_slot);
	return 1;
    } /* if fp != c_file */

    do {
	char *pointer;

/* The   for   loop will parse one output line */

	if (set_cursor) {
		ind = indent <= MAX_INDENT
			? indent
			: MIN_INDENT + indent % (MAX_INDENT - MIN_INDENT);
		cursor_pos = ind + extra_indent;
		set_cursor = 0;
		}
	if (in_comment)
        	for (pointer = next_slot; *pointer && *pointer != '\n' &&
				cursor_pos <= max_line_len; pointer++)
			cursor_pos++;
	else
          for (pointer = next_slot; *pointer && *pointer != '\n' &&
		cursor_pos <= max_line_len; pointer++) {

	    /* Update state variables here */

	    if (In_string) {
		switch(*pointer) {
			case '\\':
				if (++cursor_pos > max_line_len) {
					cursor_pos -= 2;
					--pointer;
					goto overflow;
					}
				++pointer;
				break;
			case '"':
				In_string = 0;
				word_start = 0;
			}
		}
	    else switch (*pointer) {
	        case '"':
			if (cursor_pos + 5 > max_line_len) {
				word_start = 0;
				--pointer;
				goto overflow;
				}
			In_string = 1;
			string_start = word_start = pointer;
		    	break;
	        case '\'':
			if (pointer[1] == '\\')
				if ((ch = pointer[2]) >= '0' && ch <= '7')
					for(inc = 3; pointer[inc] != '\''
						&& ++inc < 5;);
				else
					inc = 3;
			else
				inc = 2;
			/*debug*/ if (pointer[inc] != '\'')
			/*debug*/  fatalstr("Bad character constant %.10s",
					pointer);
			if ((cursor_pos += inc) > max_line_len) {
				cursor_pos -= inc;
				word_start = 0;
				--pointer;
				goto overflow;
				}
			word_start = pointer;
			pointer += inc;
			break;
		case '\t':
		    cursor_pos = 8 * ((cursor_pos + 8) / 8) - 1;
		    break;
		default: {

/* HACK  Assumes that all characters in an atomic C token will be written
   at the same time.  Must check for tokens first, since '-' is considered
   part of an identifier; checking isident first would mean breaking up "->" */

		    if (word_start) {
			if (isntident(*(unsigned char *)pointer))
				word_start = NULL;
			}
		    else if (isident(*(unsigned char *)pointer))
			word_start = pointer;
		    break;
		} /* default */
	    } /* switch */
	    cursor_pos++;
	} /* for pointer = next_slot */
 overflow:
	if (*pointer == '\0') {

/* The output line is not complete, so break out and don't output
   anything.  The current line fragment will be stored in the buffer */

	    next_slot = pointer;
	    break;
	} else {
	    char last_char;
	    int in_string0 = In_string;

/* If the line was too long, move   pointer   back to the character before
   the current word.  This allows line breaking on word boundaries.  Make
   sure that 80 character comment lines get broken up somehow.  We assume
   that any non-string 80 character identifier must be in a comment.
*/

	    if (*pointer == '\n')
		in_define = 0;
	    else if (word_start && word_start > output_buf)
		if (In_string)
			if (string_start && pointer - string_start < 5)
				pointer = string_start - 1;
			else {
				pointer = adjust_pointer_in_string(pointer);
				string_start = 0;
				}
		else if (word_start == string_start
				&& pointer - string_start >= 5) {
			pointer = adjust_pointer_in_string(next_slot);
			In_string = 1;
			string_start = 0;
			}
		else
			pointer = word_start - 1;
	    else if (cursor_pos > max_line_len) {
#ifndef ANSI_Libraries
		extern char *strchr();
#endif
		if (In_string) {
			pointer = adjust_pointer_in_string(pointer);
			if (string_start && pointer > string_start)
				string_start = 0;
			}
		else if (strchr("&*+-/<=>|", *pointer)
			&& strchr("!%&*+-/<=>^|", pointer[-1])) {
			pointer -= 2;
			if (strchr("<>", *pointer)) /* <<=, >>= */
				pointer--;
			}
		else {
			if (word_start)
				while(isident(*(unsigned char *)pointer))
					pointer++;
			pointer--;
			}
		}
	    last_char = *pointer;
	    write_indent(fp, use_indent, extra_indent, output_buf, pointer);
	    next_slot = output_buf;
	    if (In_string && !string_start && Ansi == 1 && last_char != '\n')
		*next_slot++ = '"';
	    fwd_strcpy(next_slot, pointer + 1);

/* insert a line break */

	    if (last_char == '\n') {
		if (In_string)
			last_was_newline = 0;
		else {
			last_was_newline = 1;
			extra_indent = 0;
			}
		}
	    else {
		extra_indent = TOO_LONG_INDENT;
		if (In_string && !string_start) {
			if (Ansi == 1) {
				fprintf(fp, "\"\n");
				use_indent = 1;
				last_was_newline = 1;
				}
			else {
				fprintf(fp, "\\\n");
				last_was_newline = 0;
				}
			In_string = in_string0;
			}
		else {
			if (in_define)
				putc('\\', fp);
			putc ('\n', fp);
			last_was_newline = 1;
			}
	    } /* if *pointer != '\n' */

	    if (In_string && Ansi != 1 && !string_start)
		cursor_pos = 0;
	    else
		set_cursor = 1;

	    string_start = word_start = NULL;

	} /* else */

    } while (*next_slot);

    return 0;
} /* ind_printf */
