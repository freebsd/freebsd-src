/* 
 * evaluate the dc language, from a FILE* or a string
 *
 * Copyright (C) 1994 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can either send email to this
 * program's author (see below) or write to: The Free Software Foundation,
 * Inc.; 675 Mass Ave. Cambridge, MA 02139, USA.
 */

/* This is the only module which knows about the dc input language */

#include "config.h"

#include <stdio.h>
#include <string.h>	
#include "dc.h"
#include "dc-proto.h"

typedef enum {
	DC_OKAY,		/* no further intervention needed for this command */
	DC_EATONE,		/* caller needs to eat the lookahead char */
	DC_QUIT,		/* quit out of unwind_depth levels of evaluation */

	/* with the following return values, the caller does not have to 
	 * fret about rescan_stdin's value
	 */
	DC_INT,			/* caller needs to parse a dc_num from input stream */
	DC_STR,			/* caller needs to parse a dc_str from input stream */
	DC_SYSTEM,		/* caller needs to run a system() on next input line */
	DC_COMMENT,		/* caller needs to skip to the next input line */

	DC_EOF_ERROR	/* unexpected end of input; abort current eval */
} dc_status;

static int dc_ibase=10;		/* input base, 2 <= dc_ibase <= DC_IBASE_MAX */
static int dc_obase=10;		/* output base, 2 <= dc_obase */
static int dc_scale=0;		/* scale (see user documentaton) */

/* forward reference */
static dc_status dc_evalstr DC_PROTO((dc_data));

/* for Quitting evaluations */
static int unwind_depth=0;

/* if true, active Quit will not exit program */
static dc_boolean unwind_noexit=DC_FALSE;

/* if true, stdin has been mucked with, dc_evalfile() needs to resyncronize */
static dc_boolean rescan_stdin=DC_FALSE;


/* input_fil and input_str are passed as arguments to dc_getnum */

/* used by the input_* functions: */
static FILE *input_fil_fp;
static const char *input_str_string;

/* Since we have a need for two characters of pushback, and
 * ungetc() only guarantees one, we place the second pushback here
 */
static int input_pushback;

/* passed as an argument to dc_getnum */
static int
input_fil DC_DECLVOID()
{
	if (input_pushback != EOF){
		int c = input_pushback;
		input_pushback = EOF;
		return c;
	}
	return getc(input_fil_fp);
}

/* passed as an argument to dc_getnum */
static int
input_str DC_DECLVOID()
{
	if (!*input_str_string)
		return EOF;
	return *input_str_string++;
}



/* takes a string and evals it; frees the string when done */
/* Wrapper around dc_evalstr to avoid duplicating the free call
 * at all possible return points.
 */
static dc_status
dc_eval_and_free_str DC_DECLARG((string))
	dc_data string DC_DECLEND
{
	dc_status status;

	status = dc_evalstr(string);
	if (string.dc_type == DC_STRING)
		dc_free_str(&string.v.string);
	return status;
}


/* dc_func does the grunt work of figuring out what each input
 * character means; used by both dc_evalstr and dc_evalfile
 *
 * c -> the "current" input character under consideration
 * peekc -> the lookahead input character
 */
static dc_status
dc_func DC_DECLARG((c, peekc))
	int c DC_DECLSEP
	int peekc DC_DECLEND
{
	/* we occasionally need these for temporary data */
	/* Despite the GNU coding standards, it is much easier
	 * to have these decared once here, since this function
	 * is just one big switch statement.
	 */
	dc_data datum;
	int tmpint;

	switch (c){
	case '_': case '.':
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
	case '8': case '9': case 'A': case 'B':
	case 'C': case 'D': case 'E': case 'F':
		return DC_INT;
	case ' ':
	case '\t':
	case '\n':
		/* standard command separators */
		break;

	case '+':	/* add top two stack elements */
		dc_binop(dc_add, dc_scale);
		break;
	case '-':	/* subtract top two stack elements */
		dc_binop(dc_sub, dc_scale);
		break;
	case '*':	/* multiply top two stack elements */
		dc_binop(dc_mul, dc_scale);
		break;
	case '/':	/* divide top two stack elements */
		dc_binop(dc_div, dc_scale);
		break;
	case '%':
		/* take the remainder from  division of the top two stack elements */
		dc_binop(dc_rem, dc_scale);
		break;
	case '^':	/* exponientiation of the top two stack elements */
		dc_binop(dc_exp, dc_scale);
		break;
	case '<':
		/* eval register named by peekc if
		 * less-than holds for top two stack elements
		 */
		if (peekc == EOF)
			return DC_EOF_ERROR;
		if (dc_cmpop() <  0)
			if (dc_register_get(peekc, &datum) == DC_SUCCESS)
				if (dc_eval_and_free_str(datum) == DC_QUIT)
					return DC_QUIT;
		return DC_EATONE;
	case '=':
		/* eval register named by peekc if
		 * equal-to holds for top two stack elements
		 */
		if (peekc == EOF)
			return DC_EOF_ERROR;
		if (dc_cmpop() == 0)
			if (dc_register_get(peekc, &datum) == DC_SUCCESS)
				if (dc_eval_and_free_str(datum) == DC_QUIT)
					return DC_QUIT;
		return DC_EATONE;
	case '>':
		/* eval register named by peekc if
		 * greater-than holds for top two stack elements
		 */
		if (peekc == EOF)
			return DC_EOF_ERROR;
		if (dc_cmpop()  > 0)
			if (dc_register_get(peekc, &datum) == DC_SUCCESS)
				if (dc_eval_and_free_str(datum) == DC_QUIT)
					return DC_QUIT;
		return DC_EATONE;
	case '?':	/* read a lnie from standard-input and eval it */
		for (c=peekc; c=='\n'; c=getc(stdin))
			;
		ungetc(c, stdin);
		if (dc_eval_and_free_str(dc_readstring(stdin, '\n', '\n')) == DC_QUIT)
			return DC_QUIT;
		rescan_stdin = DC_TRUE;
		return DC_OKAY;
	case '[':	/* read to balancing ']' into a dc_str */
		return DC_STR;
	case '!':	/* read to newline and call system() on resulting string */
		return DC_SYSTEM;
	case '#':	/* comment; skip remainder of current line */
		return DC_COMMENT;

	case 'c':	/* clear whole stack */
		dc_clear_stack();
		break;
	case 'd':	/* duplicate the datum on the top of stack */
		if (dc_top_of_stack(&datum) == DC_SUCCESS)
			dc_push(dc_dup(datum));
		break;
	case 'f':	/* print list of all stack items */
		dc_printall(dc_obase);
		break;
	case 'i':	/* set input base to value on top of stack */
		if (dc_pop(&datum) == DC_SUCCESS){
			tmpint = 0;
			if (datum.dc_type == DC_NUMBER)
				tmpint = dc_num2int(datum.v.number, DC_TRUE);
			if ( ! (2 <= tmpint  &&  tmpint <= DC_IBASE_MAX) )
				fprintf(stderr,
						"%s: input base must be a number \
between 2 and %d (inclusive)\n",
						progname, DC_IBASE_MAX);
			else
				dc_ibase = tmpint;
		}
		break;
	case 'k':	/* set scale to value on top of stack */
		if (dc_pop(&datum) == DC_SUCCESS){
			tmpint = -1;
			if (datum.dc_type == DC_NUMBER)
				tmpint = dc_num2int(datum.v.number, DC_TRUE);
			if ( ! (tmpint >= 0) )
				fprintf(stderr,
						"%s: scale must be a nonnegative number\n",
						progname);
			else
				dc_scale = tmpint;
		}
		break;
	case 'l':	/* "load" -- push value on top of register stack named
				 * by peekc onto top of evaluation stack; does not
				 * modify the register stack
				 */
		if (peekc == EOF)
			return DC_EOF_ERROR;
		if (dc_register_get(peekc, &datum) == DC_SUCCESS)
			dc_push(datum);
		return DC_EATONE;
	case 'o':	/* set output base to value on top of stack */
		if (dc_pop(&datum) == DC_SUCCESS){
			tmpint = 0;
			if (datum.dc_type == DC_NUMBER)
				tmpint = dc_num2int(datum.v.number, DC_TRUE);
			if ( ! (tmpint > 1) )
				fprintf(stderr,
						"%s: output base must be a number greater than 1\n",
						progname);
			else
				dc_obase = tmpint;
		}
		break;
	case 'p':	/* print the datum on the top of stack */
		if (dc_top_of_stack(&datum) == DC_SUCCESS)
			dc_print(datum, dc_obase);
		break;
	case 'q':	/* quit two levels of evaluation, posibly exiting program */
		unwind_depth = 2;
		unwind_noexit = DC_FALSE;
		return DC_QUIT;
	case 's':	/* "store" -- replace top of register stack named
				 * by peekc with the value popped from the top
				 * of the evaluation stack
				 */
		if (peekc == EOF)
			return DC_EOF_ERROR;
		if (dc_pop(&datum) == DC_SUCCESS)
			dc_register_set(peekc, datum);
		return DC_EATONE;
	case 'v':	/* replace top of stack with its square root */
		if (dc_pop(&datum) == DC_SUCCESS){
			dc_num tmpnum;
			if (datum.dc_type != DC_NUMBER){
				fprintf(stderr,
						"%s: square root of nonnumeric attempted\n",
						progname);
			}else if (dc_sqrt(datum.v.number, dc_scale, &tmpnum) == DC_SUCCESS){
				dc_free_num(&datum.v.number);
				datum.v.number = tmpnum;
				dc_push(datum);
			}
		}
		break;
	case 'x':	/* eval the datum popped from top of stack */
		if (dc_pop(&datum) == DC_SUCCESS){
			if (datum.dc_type == DC_STRING){
				if (dc_eval_and_free_str(datum) == DC_QUIT)
					return DC_QUIT;
			}else if (datum.dc_type == DC_NUMBER){
				dc_push(datum);
			}else{
				dc_garbage("at top of stack", -1);
			}
		}
		break;
	case 'z':	/* push the current stack depth onto the top of stack */
		dc_push(dc_int2data(dc_tell_stackdepth()));
		break;

	case 'I':	/* push the current input base onto the stack */
		dc_push(dc_int2data(dc_ibase));
		break;
	case 'K':	/* push the current scale onto the stack */
		dc_push(dc_int2data(dc_scale));
		break;
	case 'L':	/* pop a value off of register stack named by peekc
				 * and push it onto the evaluation stack
				 */
		if (peekc == EOF)
			return DC_EOF_ERROR;
		if (dc_register_pop(peekc, &datum) == DC_SUCCESS)
			dc_push(datum);
		return DC_EATONE;
	case 'O':	/* push the current output base onto the stack */
		dc_push(dc_int2data(dc_obase));
		break;
	case 'P':	/* print the value popped off of top-of-stack;
				 * do not add a trailing newline
				 */
		if (dc_pop(&datum) == DC_SUCCESS){
			if (datum.dc_type == DC_STRING)
				dc_out_str(datum.v.string, DC_FALSE, DC_TRUE);
			else if (datum.dc_type == DC_NUMBER)
				dc_out_num(datum.v.number, dc_obase, DC_FALSE, DC_TRUE);
			else
				dc_garbage("at top of stack", -1);
		}
		break;
	case 'Q':	/* quit out of top-of-stack nested evals;
				 * pops value from stack;
				 * does not exit program (stops short if necessary)
				 */
		if (dc_pop(&datum) == DC_SUCCESS){
			unwind_depth = 0;
			unwind_noexit = DC_TRUE;
			if (datum.dc_type == DC_NUMBER)
				unwind_depth = dc_num2int(datum.v.number, DC_TRUE);
			if (unwind_depth > 0)
				return DC_QUIT;
			fprintf(stderr,
					"%s: Q command requires a positive number\n",
					progname);
		}
		break;
	case 'S':	/* pop a value off of the evaluation stack
				 * and push it onto the register stack named by peekc
				 */
		if (peekc == EOF)
			return DC_EOF_ERROR;
		if (dc_pop(&datum) == DC_SUCCESS)
			dc_register_push(peekc, datum);
		return DC_EATONE;
	case 'X':	/* replace the number on top-of-stack with its scale factor */
		if (dc_pop(&datum) == DC_SUCCESS){
			tmpint = 0;
			if (datum.dc_type == DC_NUMBER)
				tmpint = dc_tell_scale(datum.v.number, DC_TRUE);
			dc_push(dc_int2data(tmpint));
		}
		break;
	case 'Z':	/* replace the datum on the top-of-stack with its length */
		if (dc_pop(&datum) == DC_SUCCESS)
			dc_push(dc_int2data(dc_tell_length(datum, DC_TRUE)));
		break;

	case ':':	/* store into array */
		if (peekc == EOF)
			return DC_EOF_ERROR;
		if (dc_pop(&datum) == DC_SUCCESS){
			tmpint = -1;
			if (datum.dc_type == DC_NUMBER)
				tmpint = dc_num2int(datum.v.number, DC_TRUE);
			if (dc_pop(&datum) == DC_SUCCESS){
				if (tmpint < 0)
					fprintf(stderr,
							"%s: array index must be a nonnegative integer\n",
							progname);
				else
					dc_array_set(peekc, tmpint, datum);
			}
		}
		return DC_EATONE;
	case ';':	/* retreive from array */
		if (peekc == EOF)
			return DC_EOF_ERROR;
		if (dc_pop(&datum) == DC_SUCCESS){
			tmpint = -1;
			if (datum.dc_type == DC_NUMBER)
				tmpint = dc_num2int(datum.v.number, DC_TRUE);
			if (tmpint < 0)
				fprintf(stderr,
						"%s: array index must be a nonnegative integer\n",
						progname);
			else
				dc_push(dc_array_get(peekc, tmpint));
		}
		return DC_EATONE;

	default:	/* What did that user mean? */
		fprintf(stderr, "%s: ", progname);
		dc_show_id(stdout, c, " unimplemented\n");
		break;
	}
	return DC_OKAY;
}


/* takes a string and evals it */
static dc_status
dc_evalstr DC_DECLARG((string))
	dc_data string DC_DECLEND
{
	const char *s;
	const char *end;
	const char *p;
	size_t len;
	int c;
	int peekc;
	int count;

	if (string.dc_type != DC_STRING){
		fprintf(stderr,
				"%s: eval called with non-string argument\n",
				progname);
		return DC_OKAY;
	}
	s = dc_str2charp(string.v.string);
	end = s + dc_strlen(string.v.string);
	while (s < end){
		c = *(const unsigned char *)s++;
		peekc = EOF;
		if (s < end)
			peekc = *(const unsigned char *)s;
		switch (dc_func(c, peekc)){
		case DC_OKAY:
			break;
		case DC_EATONE:
			if (peekc != EOF)
				++s;
			break;
		case DC_QUIT:
			if (unwind_depth > 0){
				--unwind_depth;
				return DC_QUIT;
			}
			return DC_OKAY;

		case DC_INT:
			input_str_string = s - 1;
			dc_push(dc_getnum(input_str, dc_ibase, &peekc));
			s = input_str_string;
			if (peekc != EOF)
				--s;
			break;
		case DC_STR:
			count = 1;
			for (p=s; p<end && count>0; ++p)
				if (*p == ']')
					--count;
				else if (*p == '[')
					++count;
			len = p - s;
			dc_push(dc_makestring(s, len-1));
			s = p;
			break;
		case DC_SYSTEM:
			s = dc_system(s);
		case DC_COMMENT:
			s = memchr(s, '\n', (size_t)(end-s));
			if (!s)
				s = end;
			++s;
			break;

		case DC_EOF_ERROR:
			fprintf(stderr, "%s: unexpected EOS\n", progname);
			return DC_OKAY;
		}
	}
	return DC_OKAY;
}


/* This is the main function of the whole DC program.
 * Reads the file described by fp, calls dc_func to do
 * the dirty work, and takes care of dc_func's shortcomings.
 */
int
dc_evalfile DC_DECLARG((fp))
	FILE *fp DC_DECLEND
{
	int c;
	int peekc;
	dc_data datum;

	for (c=getc(fp); c!=EOF; c=peekc){
		peekc = getc(fp);
		rescan_stdin = DC_FALSE;
		switch (dc_func(c, peekc)){
		case DC_OKAY:
			if (rescan_stdin == DC_TRUE  &&  fp == stdin)
				peekc = getc(fp);
			break;
		case DC_EATONE:
			peekc = getc(fp);
			break;
		case DC_QUIT:
			if (unwind_noexit != DC_TRUE)
				return DC_SUCCESS;
			fprintf(stderr,
					"%s: Q command argument exceeded string execution depth\n",
					progname);
			if (rescan_stdin == DC_TRUE  &&  fp == stdin)
				peekc = getc(fp);
			break;

		case DC_INT:
			input_fil_fp = fp;
			input_pushback = c;
			ungetc(peekc, fp);
			dc_push(dc_getnum(input_fil, dc_ibase, &peekc));
			break;
		case DC_STR:
			ungetc(peekc, fp);
			datum = dc_readstring(fp, '[', ']');
			dc_push(datum);
			peekc = getc(fp);
			break;
		case DC_SYSTEM:
			ungetc(peekc, fp);
			datum = dc_readstring(stdin, '\n', '\n');
			(void)dc_system(dc_str2charp(datum.v.string));
			dc_free_str(&datum.v.string);
			peekc = getc(fp);
			break;
		case DC_COMMENT:
			while (peekc!=EOF && peekc!='\n')
				peekc = getc(fp);
			if (peekc != EOF)
				peekc = getc(fp);
			break;

		case DC_EOF_ERROR:
			fprintf(stderr, "%s: unexpected EOF\n", progname);
			return DC_FAIL;
		}
	}
	return DC_SUCCESS;
}
