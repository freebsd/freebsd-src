/*
 * Copyright (c) 1985 Sun Microsystems, Inc.
 * Copyright (c) 1976 Board of Trustees of the University of Illinois.
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1985 Sun Microsystems, Inc.\n\
@(#) Copyright (c) 1976 Board of Trustees of the University of Illinois.\n\
@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if 0
#ifndef lint
static char sccsid[] = "@(#)indent.c	5.17 (Berkeley) 6/7/93";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "indent_globs.h"
#include "indent_codes.h"
#include "indent.h"

static void bakcopy(void);

const char *in_name = "Standard Input";	/* will always point to name of input
					 * file */
const char *out_name = "Standard Output";	/* will always point to name
						 * of output file */
char        bakfile[MAXPATHLEN] = "";

int
main(int argc, char **argv)
{

    int         dec_ind;	/* current indentation for declarations */
    int         di_stack[20];	/* a stack of structure indentation levels */
    int         flushed_nl;	/* used when buffering up comments to remember
				 * that a newline was passed over */
    int         force_nl;	/* when true, code must be broken */
    int         hd_type = 0;	/* used to store type of stmt for if (...),
				 * for (...), etc */
    int		i;		/* local loop counter */
    int         scase;		/* set to true when we see a case, so we will
				 * know what to do with the following colon */
    int         sp_sw;		/* when true, we are in the expression of
				 * if(...), while(...), etc. */
    int         squest;		/* when this is positive, we have seen a ?
				 * without the matching : in a <c>?<s>:<s>
				 * construct */
    const char *t_ptr;		/* used for copying tokens */
    int		tabs_to_var;	/* true if using tabs to indent to var name */
    int         type_code;	/* the type of token, returned by lexi */

    int         last_else = 0;	/* true iff last keyword was an else */


    /*-----------------------------------------------*\
    |		      INITIALIZATION		      |
    \*-----------------------------------------------*/

    found_err = 0;

    ps.p_stack[0] = stmt;	/* this is the parser's stack */
    ps.last_nl = true;		/* this is true if the last thing scanned was
				 * a newline */
    ps.last_token = semicolon;
    combuf = (char *) malloc(bufsize);
    if (combuf == NULL)
	err(1, NULL);
    labbuf = (char *) malloc(bufsize);
    if (labbuf == NULL)
	err(1, NULL);
    codebuf = (char *) malloc(bufsize);
    if (codebuf == NULL)
	err(1, NULL);
    tokenbuf = (char *) malloc(bufsize);
    if (tokenbuf == NULL)
	err(1, NULL);
    l_com = combuf + bufsize - 5;
    l_lab = labbuf + bufsize - 5;
    l_code = codebuf + bufsize - 5;
    l_token = tokenbuf + bufsize - 5;
    combuf[0] = codebuf[0] = labbuf[0] = ' ';	/* set up code, label, and
						 * comment buffers */
    combuf[1] = codebuf[1] = labbuf[1] = '\0';
    ps.else_if = 1;		/* Default else-if special processing to on */
    s_lab = e_lab = labbuf + 1;
    s_code = e_code = codebuf + 1;
    s_com = e_com = combuf + 1;
    s_token = e_token = tokenbuf + 1;

    in_buffer = (char *) malloc(10);
    if (in_buffer == NULL)
	err(1, NULL);
    in_buffer_limit = in_buffer + 8;
    buf_ptr = buf_end = in_buffer;
    line_no = 1;
    had_eof = ps.in_decl = ps.decl_on_line = break_comma = false;
    sp_sw = force_nl = false;
    ps.in_or_st = false;
    ps.bl_line = true;
    dec_ind = 0;
    di_stack[ps.dec_nest = 0] = 0;
    ps.want_blank = ps.in_stmt = ps.ind_stmt = false;

    scase = ps.pcase = false;
    squest = 0;
    sc_end = 0;
    bp_save = 0;
    be_save = 0;

    output = 0;
    tabs_to_var = 0;

    /*--------------------------------------------------*\
    |   		COMMAND LINE SCAN		 |
    \*--------------------------------------------------*/

#ifdef undef
    max_col = 78;		/* -l78 */
    lineup_to_parens = 1;	/* -lp */
    ps.ljust_decl = 0;		/* -ndj */
    ps.com_ind = 33;		/* -c33 */
    star_comment_cont = 1;	/* -sc */
    ps.ind_size = 8;		/* -i8 */
    verbose = 0;
    ps.decl_indent = 16;	/* -di16 */
    ps.local_decl_indent = -1;	/* if this is not set to some nonnegative value
				 * by an arg, we will set this equal to
				 * ps.decl_ind */
    ps.indent_parameters = 1;	/* -ip */
    ps.decl_com_ind = 0;	/* if this is not set to some positive value
				 * by an arg, we will set this equal to
				 * ps.com_ind */
    btype_2 = 1;		/* -br */
    cuddle_else = 1;		/* -ce */
    ps.unindent_displace = 0;	/* -d0 */
    ps.case_indent = 0;		/* -cli0 */
    format_block_comments = 1;	/* -fcb */
    format_col1_comments = 1;	/* -fc1 */
    procnames_start_line = 1;	/* -psl */
    proc_calls_space = 0;	/* -npcs */
    comment_delimiter_on_blankline = 1;	/* -cdb */
    ps.leave_comma = 1;		/* -nbc */
#endif

    for (i = 1; i < argc; ++i)
	if (strcmp(argv[i], "-npro") == 0)
	    break;
    set_defaults();
    if (i >= argc)
	set_profile();

    for (i = 1; i < argc; ++i) {

	/*
	 * look thru args (if any) for changes to defaults
	 */
	if (argv[i][0] != '-') {/* no flag on parameter */
	    if (input == NULL) {	/* we must have the input file */
		in_name = argv[i];	/* remember name of input file */
		input = fopen(in_name, "r");
		if (input == NULL)	/* check for open error */
			err(1, "%s", in_name);
		continue;
	    }
	    else if (output == NULL) {	/* we have the output file */
		out_name = argv[i];	/* remember name of output file */
		if (strcmp(in_name, out_name) == 0) {	/* attempt to overwrite
							 * the file */
		    errx(1, "input and output files must be different");
		}
		output = fopen(out_name, "w");
		if (output == NULL)	/* check for create error */
			err(1, "%s", out_name);
		continue;
	    }
	    errx(1, "unknown parameter: %s", argv[i]);
	}
	else
	    set_option(argv[i]);
    }				/* end of for */
    if (input == NULL)
	input = stdin;
    if (output == NULL) {
	if (troff || input == stdin)
	    output = stdout;
	else {
	    out_name = in_name;
	    bakcopy();
	}
    }
    if (ps.com_ind <= 1)
	ps.com_ind = 2;		/* dont put normal comments before column 2 */
    if (troff) {
	if (bodyf.font[0] == 0)
	    parsefont(&bodyf, "R");
	if (scomf.font[0] == 0)
	    parsefont(&scomf, "I");
	if (blkcomf.font[0] == 0)
	    blkcomf = scomf, blkcomf.size += 2;
	if (boxcomf.font[0] == 0)
	    boxcomf = blkcomf;
	if (stringf.font[0] == 0)
	    parsefont(&stringf, "L");
	if (keywordf.font[0] == 0)
	    parsefont(&keywordf, "B");
	writefdef(&bodyf, 'B');
	writefdef(&scomf, 'C');
	writefdef(&blkcomf, 'L');
	writefdef(&boxcomf, 'X');
	writefdef(&stringf, 'S');
	writefdef(&keywordf, 'K');
    }
    if (block_comment_max_col <= 0)
	block_comment_max_col = max_col;
    if (ps.local_decl_indent < 0)	/* if not specified by user, set this */
	ps.local_decl_indent = ps.decl_indent;
    if (ps.decl_com_ind <= 0)	/* if not specified by user, set this */
	ps.decl_com_ind = ps.ljust_decl ? (ps.com_ind <= 10 ? 2 : ps.com_ind - 8) : ps.com_ind;
    if (continuation_indent == 0)
	continuation_indent = ps.ind_size;
    fill_buffer();		/* get first batch of stuff into input buffer */

    parse(semicolon);
    {
	char *p = buf_ptr;
	int col = 1;

	while (1) {
	    if (*p == ' ')
		col++;
	    else if (*p == '\t')
		col = ((col - 1) & ~7) + 9;
	    else
		break;
	    p++;
	}
	if (col > ps.ind_size)
	    ps.ind_level = ps.i_l_follow = col / ps.ind_size;
    }
    if (troff) {
	const char *p = in_name,
	           *beg = in_name;

	while (*p)
	    if (*p++ == '/')
		beg = p;
	fprintf(output, ".Fn \"%s\"\n", beg);
    }
    /*
     * START OF MAIN LOOP
     */

    while (1) {			/* this is the main loop.  it will go until we
				 * reach eof */
	int         is_procname;

	type_code = lexi();	/* lexi reads one token.  The actual
				 * characters read are stored in "token". lexi
				 * returns a code indicating the type of token */
	is_procname = ps.procname[0];

	/*
	 * The following code moves everything following an if (), while (),
	 * else, etc. up to the start of the following stmt to a buffer. This
	 * allows proper handling of both kinds of brace placement.
	 */

	flushed_nl = false;
	while (ps.search_brace) {	/* if we scanned an if(), while(),
					 * etc., we might need to copy stuff
					 * into a buffer we must loop, copying
					 * stuff into save_com, until we find
					 * the start of the stmt which follows
					 * the if, or whatever */
	    switch (type_code) {
	    case newline:
		++line_no;
		flushed_nl = true;
	    case form_feed:
		break;		/* form feeds and newlines found here will be
				 * ignored */

	    case lbrace:	/* this is a brace that starts the compound
				 * stmt */
		if (sc_end == 0) {	/* ignore buffering if a comment wasn't
					 * stored up */
		    ps.search_brace = false;
		    goto check_type;
		}
		if (btype_2) {
		    save_com[0] = '{';	/* we either want to put the brace
					 * right after the if */
		    goto sw_buffer;	/* go to common code to get out of
					 * this loop */
		}
	    case comment:	/* we have a comment, so we must copy it into
				 * the buffer */
		if (!flushed_nl || sc_end != 0) {
		    if (sc_end == 0) {	/* if this is the first comment, we
					 * must set up the buffer */
			save_com[0] = save_com[1] = ' ';
			sc_end = &(save_com[2]);
		    }
		    else {
			*sc_end++ = '\n';	/* add newline between
						 * comments */
			*sc_end++ = ' ';
			--line_no;
		    }
		    *sc_end++ = '/';	/* copy in start of comment */
		    *sc_end++ = '*';

		    for (;;) {	/* loop until we get to the end of the comment */
			*sc_end = *buf_ptr++;
			if (buf_ptr >= buf_end)
			    fill_buffer();

			if (*sc_end++ == '*' && *buf_ptr == '/')
			    break;	/* we are at end of comment */

			if (sc_end >= &(save_com[sc_size])) {	/* check for temp buffer
								 * overflow */
			    diag2(1, "Internal buffer overflow - Move big comment from right after if, while, or whatever");
			    fflush(output);
			    exit(1);
			}
		    }
		    *sc_end++ = '/';	/* add ending slash */
		    if (++buf_ptr >= buf_end)	/* get past / in buffer */
			fill_buffer();
		    break;
		}
	    default:		/* it is the start of a normal statement */
		if (flushed_nl)	/* if we flushed a newline, make sure it is
				 * put back */
		    force_nl = true;
		if ((type_code == sp_paren && *token == 'i'
			&& last_else && ps.else_if)
			|| (type_code == sp_nparen && *token == 'e'
			&& e_code != s_code && e_code[-1] == '}'))
		    force_nl = false;

		if (sc_end == 0) {	/* ignore buffering if comment wasn't
					 * saved up */
		    ps.search_brace = false;
		    goto check_type;
		}
		if (force_nl) {	/* if we should insert a nl here, put it into
				 * the buffer */
		    force_nl = false;
		    --line_no;	/* this will be re-increased when the nl is
				 * read from the buffer */
		    *sc_end++ = '\n';
		    *sc_end++ = ' ';
		    if (verbose && !flushed_nl)	/* print error msg if the line
						 * was not already broken */
			diag2(0, "Line broken");
		    flushed_nl = false;
		}
		for (t_ptr = token; *t_ptr; ++t_ptr)
		    *sc_end++ = *t_ptr;	/* copy token into temp buffer */
		ps.procname[0] = 0;

	sw_buffer:
		ps.search_brace = false;	/* stop looking for start of
						 * stmt */
		bp_save = buf_ptr;	/* save current input buffer */
		be_save = buf_end;
		buf_ptr = save_com;	/* fix so that subsequent calls to
					 * lexi will take tokens out of
					 * save_com */
		*sc_end++ = ' ';/* add trailing blank, just in case */
		buf_end = sc_end;
		sc_end = 0;
		break;
	    }			/* end of switch */
	    if (type_code != 0)	/* we must make this check, just in case there
				 * was an unexpected EOF */
		type_code = lexi();	/* read another token */
	    /* if (ps.search_brace) ps.procname[0] = 0; */
	    if ((is_procname = ps.procname[0]) && flushed_nl
		    && !procnames_start_line && ps.in_decl
		    && type_code == ident)
		flushed_nl = 0;
	}			/* end of while (search_brace) */
	last_else = 0;
check_type:
	if (type_code == 0) {	/* we got eof */
	    if (s_lab != e_lab || s_code != e_code
		    || s_com != e_com)	/* must dump end of line */
		dump_line();
	    if (ps.tos > 1)	/* check for balanced braces */
		diag2(1, "Stuff missing from end of file");

	    if (verbose) {
		printf("There were %d output lines and %d comments\n",
		       ps.out_lines, ps.out_coms);
		printf("(Lines with comments)/(Lines with code): %6.3f\n",
		       (1.0 * ps.com_lines) / code_lines);
	    }
	    fflush(output);
	    exit(found_err);
	}
	if (
		(type_code != comment) &&
		(type_code != newline) &&
		(type_code != preesc) &&
		(type_code != form_feed)) {
	    if (force_nl &&
		    (type_code != semicolon) &&
		    (type_code != lbrace || !btype_2)) {
		/* we should force a broken line here */
		if (verbose && !flushed_nl)
		    diag2(0, "Line broken");
		flushed_nl = false;
		dump_line();
		ps.want_blank = false;	/* dont insert blank at line start */
		force_nl = false;
	    }
	    ps.in_stmt = true;	/* turn on flag which causes an extra level of
				 * indentation. this is turned off by a ; or
				 * '}' */
	    if (s_com != e_com) {	/* the turkey has embedded a comment
					 * in a line. fix it */
		*e_code++ = ' ';
		for (t_ptr = s_com; *t_ptr; ++t_ptr) {
		    CHECK_SIZE_CODE;
		    *e_code++ = *t_ptr;
		}
		*e_code++ = ' ';
		*e_code = '\0';	/* null terminate code sect */
		ps.want_blank = false;
		e_com = s_com;
	    }
	}
	else if (type_code != comment)	/* preserve force_nl thru a comment */
	    force_nl = false;	/* cancel forced newline after newline, form
				 * feed, etc */



	/*-----------------------------------------------------*\
	|	   do switch on type of token scanned		|
	\*-----------------------------------------------------*/
	CHECK_SIZE_CODE;
	switch (type_code) {	/* now, decide what to do with the token */

	case form_feed:	/* found a form feed in line */
	    ps.use_ff = true;	/* a form feed is treated much like a newline */
	    dump_line();
	    ps.want_blank = false;
	    break;

	case newline:
	    if (ps.last_token != comma || ps.p_l_follow > 0
		    || !ps.leave_comma || ps.block_init || !break_comma || s_com != e_com) {
		dump_line();
		ps.want_blank = false;
	    }
	    ++line_no;		/* keep track of input line number */
	    break;

	case lparen:		/* got a '(' or '[' */
	    ++ps.p_l_follow;	/* count parens to make Healy happy */
	    if (ps.want_blank && *token != '[' &&
		    (ps.last_token != ident || proc_calls_space
	      || (ps.its_a_keyword && (!ps.sizeof_keyword || Bill_Shannon))))
		*e_code++ = ' ';
	    if (ps.in_decl && !ps.block_init)
		if (troff && !ps.dumped_decl_indent && !is_procname && ps.last_token == decl) {
		    ps.dumped_decl_indent = 1;
		    sprintf(e_code, "\n.Du %dp+\200p \"%s\"\n", dec_ind * 7, token);
		    e_code += strlen(e_code);
		}
		else {
		    while ((e_code - s_code) < dec_ind) {
			CHECK_SIZE_CODE;
			*e_code++ = ' ';
		    }
		    *e_code++ = token[0];
		}
	    else
		*e_code++ = token[0];
	    ps.paren_indents[ps.p_l_follow - 1] = e_code - s_code;
	    if (sp_sw && ps.p_l_follow == 1 && extra_expression_indent
		    && ps.paren_indents[0] < 2 * ps.ind_size)
		ps.paren_indents[0] = 2 * ps.ind_size;
	    ps.want_blank = false;
	    if (ps.in_or_st && *token == '(' && ps.tos <= 2) {
		/*
		 * this is a kluge to make sure that declarations will be
		 * aligned right if proc decl has an explicit type on it, i.e.
		 * "int a(x) {..."
		 */
		parse(semicolon);	/* I said this was a kluge... */
		ps.in_or_st = false;	/* turn off flag for structure decl or
					 * initialization */
	    }
	    if (ps.sizeof_keyword)
		ps.sizeof_mask |= 1 << ps.p_l_follow;
	    break;

	case rparen:		/* got a ')' or ']' */
	    rparen_count--;
	    if (ps.cast_mask & (1 << ps.p_l_follow) & ~ps.sizeof_mask) {
		ps.last_u_d = true;
		ps.cast_mask &= (1 << ps.p_l_follow) - 1;
		ps.want_blank = false;
	    } else
		ps.want_blank = true;
	    ps.sizeof_mask &= (1 << ps.p_l_follow) - 1;
	    if (--ps.p_l_follow < 0) {
		ps.p_l_follow = 0;
		diag3(0, "Extra %c", *token);
	    }
	    if (e_code == s_code)	/* if the paren starts the line */
		ps.paren_level = ps.p_l_follow;	/* then indent it */

	    *e_code++ = token[0];

	    if (sp_sw && (ps.p_l_follow == 0)) {	/* check for end of if
							 * (...), or some such */
		sp_sw = false;
		force_nl = true;/* must force newline after if */
		ps.last_u_d = true;	/* inform lexi that a following
					 * operator is unary */
		ps.in_stmt = false;	/* dont use stmt continuation
					 * indentation */

		parse(hd_type);	/* let parser worry about if, or whatever */
	    }
	    ps.search_brace = btype_2;	/* this should insure that constructs
					 * such as main(){...} and int[]{...}
					 * have their braces put in the right
					 * place */
	    break;

	case unary_op:		/* this could be any unary operation */
	    if (ps.want_blank)
		*e_code++ = ' ';

	    if (troff && !ps.dumped_decl_indent && ps.in_decl && !is_procname) {
		sprintf(e_code, "\n.Du %dp+\200p \"%s\"\n", dec_ind * 7, token);
		ps.dumped_decl_indent = 1;
		e_code += strlen(e_code);
	    }
	    else {
		const char *res = token;

		if (ps.in_decl && !ps.block_init) {	/* if this is a unary op
							 * in a declaration, we
							 * should indent this
							 * token */
		    for (i = 0; token[i]; ++i);	/* find length of token */
		    while ((e_code - s_code) < (dec_ind - i)) {
			CHECK_SIZE_CODE;
			*e_code++ = ' ';	/* pad it */
		    }
		}
		if (troff && token[0] == '-' && token[1] == '>')
		    res = "\\(->";
		for (t_ptr = res; *t_ptr; ++t_ptr) {
		    CHECK_SIZE_CODE;
		    *e_code++ = *t_ptr;
		}
	    }
	    ps.want_blank = false;
	    break;

	case binary_op:	/* any binary operation */
	    if (ps.want_blank)
		*e_code++ = ' ';
	    {
		const char *res = token;

		if (troff)
		    switch (token[0]) {
		    case '<':
			if (token[1] == '=')
			    res = "\\(<=";
			break;
		    case '>':
			if (token[1] == '=')
			    res = "\\(>=";
			break;
		    case '!':
			if (token[1] == '=')
			    res = "\\(!=";
			break;
		    case '|':
			if (token[1] == '|')
			    res = "\\(br\\(br";
			else if (token[1] == 0)
			    res = "\\(br";
			break;
		    }
		for (t_ptr = res; *t_ptr; ++t_ptr) {
		    CHECK_SIZE_CODE;
		    *e_code++ = *t_ptr;	/* move the operator */
		}
	    }
	    ps.want_blank = true;
	    break;

	case postop:		/* got a trailing ++ or -- */
	    *e_code++ = token[0];
	    *e_code++ = token[1];
	    ps.want_blank = true;
	    break;

	case question:		/* got a ? */
	    squest++;		/* this will be used when a later colon
				 * appears so we can distinguish the
				 * <c>?<n>:<n> construct */
	    if (ps.want_blank)
		*e_code++ = ' ';
	    *e_code++ = '?';
	    ps.want_blank = true;
	    break;

	case casestmt:		/* got word 'case' or 'default' */
	    scase = true;	/* so we can process the later colon properly */
	    goto copy_id;

	case colon:		/* got a ':' */
	    if (squest > 0) {	/* it is part of the <c>?<n>: <n> construct */
		--squest;
		if (ps.want_blank)
		    *e_code++ = ' ';
		*e_code++ = ':';
		ps.want_blank = true;
		break;
	    }
	    if (ps.in_or_st) {
		*e_code++ = ':';
		ps.want_blank = false;
		break;
	    }
	    ps.in_stmt = false;	/* seeing a label does not imply we are in a
				 * stmt */
	    for (t_ptr = s_code; *t_ptr; ++t_ptr)
		*e_lab++ = *t_ptr;	/* turn everything so far into a label */
	    e_code = s_code;
	    *e_lab++ = ':';
	    *e_lab++ = ' ';
	    *e_lab = '\0';

	    force_nl = ps.pcase = scase;	/* ps.pcase will be used by
						 * dump_line to decide how to
						 * indent the label. force_nl
						 * will force a case n: to be
						 * on a line by itself */
	    scase = false;
	    ps.want_blank = false;
	    break;

	case semicolon:	/* got a ';' */
	    ps.in_or_st = false;/* we are not in an initialization or
				 * structure declaration */
	    scase = false;	/* these will only need resetting in an error */
	    squest = 0;
	    if (ps.last_token == rparen && rparen_count == 0)
		ps.in_parameter_declaration = 0;
	    ps.cast_mask = 0;
	    ps.sizeof_mask = 0;
	    ps.block_init = 0;
	    ps.block_init_level = 0;
	    ps.just_saw_decl--;

	    if (ps.in_decl && s_code == e_code && !ps.block_init)
		while ((e_code - s_code) < (dec_ind - 1)) {
		    CHECK_SIZE_CODE;
		    *e_code++ = ' ';
		}

	    ps.in_decl = (ps.dec_nest > 0);	/* if we were in a first level
						 * structure declaration, we
						 * arent any more */

	    if ((!sp_sw || hd_type != forstmt) && ps.p_l_follow > 0) {

		/*
		 * This should be true iff there were unbalanced parens in the
		 * stmt.  It is a bit complicated, because the semicolon might
		 * be in a for stmt
		 */
		diag2(1, "Unbalanced parens");
		ps.p_l_follow = 0;
		if (sp_sw) {	/* this is a check for an if, while, etc. with
				 * unbalanced parens */
		    sp_sw = false;
		    parse(hd_type);	/* dont lose the if, or whatever */
		}
	    }
	    *e_code++ = ';';
	    ps.want_blank = true;
	    ps.in_stmt = (ps.p_l_follow > 0);	/* we are no longer in the
						 * middle of a stmt */

	    if (!sp_sw) {	/* if not if for (;;) */
		parse(semicolon);	/* let parser know about end of stmt */
		force_nl = true;/* force newline after an end of stmt */
	    }
	    break;

	case lbrace:		/* got a '{' */
	    ps.in_stmt = false;	/* dont indent the {} */
	    if (!ps.block_init)
		force_nl = true;/* force other stuff on same line as '{' onto
				 * new line */
	    else if (ps.block_init_level <= 0)
		ps.block_init_level = 1;
	    else
		ps.block_init_level++;

	    if (s_code != e_code && !ps.block_init) {
		if (!btype_2) {
		    dump_line();
		    ps.want_blank = false;
		}
		else if (ps.in_parameter_declaration && !ps.in_or_st) {
		    ps.i_l_follow = 0;
		    if (function_brace_split) {	/* dump the line prior to the
						 * brace ... */
			dump_line();
			ps.want_blank = false;
		    } else	/* add a space between the decl and brace */
			ps.want_blank = true;
		}
	    }
	    if (ps.in_parameter_declaration)
		prefix_blankline_requested = 0;

	    if (ps.p_l_follow > 0) {	/* check for preceding unbalanced
					 * parens */
		diag2(1, "Unbalanced parens");
		ps.p_l_follow = 0;
		if (sp_sw) {	/* check for unclosed if, for, etc. */
		    sp_sw = false;
		    parse(hd_type);
		    ps.ind_level = ps.i_l_follow;
		}
	    }
	    if (s_code == e_code)
		ps.ind_stmt = false;	/* dont put extra indentation on line
					 * with '{' */
	    if (ps.in_decl && ps.in_or_st) {	/* this is either a structure
						 * declaration or an init */
		di_stack[ps.dec_nest++] = dec_ind;
		/* ?		dec_ind = 0; */
	    }
	    else {
		ps.decl_on_line = false;	/* we can't be in the middle of
						 * a declaration, so don't do
						 * special indentation of
						 * comments */
		if (blanklines_after_declarations_at_proctop
			&& ps.in_parameter_declaration)
		    postfix_blankline_requested = 1;
		ps.in_parameter_declaration = 0;
	    }
	    dec_ind = 0;
	    parse(lbrace);	/* let parser know about this */
	    if (ps.want_blank)	/* put a blank before '{' if '{' is not at
				 * start of line */
		*e_code++ = ' ';
	    ps.want_blank = false;
	    *e_code++ = '{';
	    ps.just_saw_decl = 0;
	    break;

	case rbrace:		/* got a '}' */
	    if (ps.p_stack[ps.tos] == decl && !ps.block_init)	/* semicolons can be
								 * omitted in
								 * declarations */
		parse(semicolon);
	    if (ps.p_l_follow) {/* check for unclosed if, for, else. */
		diag2(1, "Unbalanced parens");
		ps.p_l_follow = 0;
		sp_sw = false;
	    }
	    ps.just_saw_decl = 0;
	    ps.block_init_level--;
	    if (s_code != e_code && !ps.block_init) {	/* '}' must be first on
							 * line */
		if (verbose)
		    diag2(0, "Line broken");
		dump_line();
	    }
	    *e_code++ = '}';
	    ps.want_blank = true;
	    ps.in_stmt = ps.ind_stmt = false;
	    if (ps.dec_nest > 0) {	/* we are in multi-level structure
					 * declaration */
		dec_ind = di_stack[--ps.dec_nest];
		if (ps.dec_nest == 0 && !ps.in_parameter_declaration)
		    ps.just_saw_decl = 2;
		ps.in_decl = true;
	    }
	    prefix_blankline_requested = 0;
	    parse(rbrace);	/* let parser know about this */
	    ps.search_brace = cuddle_else && ps.p_stack[ps.tos] == ifhead
		&& ps.il[ps.tos] >= ps.ind_level;
	    if (ps.tos <= 1 && blanklines_after_procs && ps.dec_nest <= 0)
		postfix_blankline_requested = 1;
	    break;

	case swstmt:		/* got keyword "switch" */
	    sp_sw = true;
	    hd_type = swstmt;	/* keep this for when we have seen the
				 * expression */
	    goto copy_id;	/* go move the token into buffer */

	case sp_paren:		/* token is if, while, for */
	    sp_sw = true;	/* the interesting stuff is done after the
				 * expression is scanned */
	    hd_type = (*token == 'i' ? ifstmt :
		       (*token == 'w' ? whilestmt : forstmt));

	    /*
	     * remember the type of header for later use by parser
	     */
	    goto copy_id;	/* copy the token into line */

	case sp_nparen:	/* got else, do */
	    ps.in_stmt = false;
	    if (*token == 'e') {
		if (e_code != s_code && (!cuddle_else || e_code[-1] != '}')) {
		    if (verbose)
			diag2(0, "Line broken");
		    dump_line();/* make sure this starts a line */
		    ps.want_blank = false;
		}
		force_nl = true;/* also, following stuff must go onto new line */
		last_else = 1;
		parse(elselit);
	    }
	    else {
		if (e_code != s_code) {	/* make sure this starts a line */
		    if (verbose)
			diag2(0, "Line broken");
		    dump_line();
		    ps.want_blank = false;
		}
		force_nl = true;/* also, following stuff must go onto new line */
		last_else = 0;
		parse(dolit);
	    }
	    goto copy_id;	/* move the token into line */

	case decl:		/* we have a declaration type (int, register,
				 * etc.) */
	    parse(decl);	/* let parser worry about indentation */
	    if (ps.last_token == rparen && ps.tos <= 1) {
		ps.in_parameter_declaration = 1;
		if (s_code != e_code) {
		    dump_line();
		    ps.want_blank = 0;
		}
	    }
	    if (ps.in_parameter_declaration && ps.indent_parameters && ps.dec_nest == 0) {
		ps.ind_level = ps.i_l_follow = 1;
		ps.ind_stmt = 0;
	    }
	    ps.in_or_st = true;	/* this might be a structure or initialization
				 * declaration */
	    ps.in_decl = ps.decl_on_line = true;
	    if ( /* !ps.in_or_st && */ ps.dec_nest <= 0)
		ps.just_saw_decl = 2;
	    prefix_blankline_requested = 0;
	    for (i = 0; token[i++];);	/* get length of token */

	    if (ps.ind_level == 0 || ps.dec_nest > 0) {
		/* global variable or struct member in local variable */
		dec_ind = ps.decl_indent > 0 ? ps.decl_indent : i;
		tabs_to_var = (use_tabs ? ps.decl_indent > 0 : 0);
	    } else {
		/* local variable */
		dec_ind = ps.local_decl_indent > 0 ? ps.local_decl_indent : i;
		tabs_to_var = (use_tabs ? ps.local_decl_indent > 0 : 0);
	    }
	    goto copy_id;

	case ident:		/* got an identifier or constant */
	    if (ps.in_decl) {	/* if we are in a declaration, we must indent
				 * identifier */
		if (is_procname == 0 || !procnames_start_line) {
		    if (!ps.block_init) {
			if (troff && !ps.dumped_decl_indent) {
			    if (ps.want_blank)
				*e_code++ = ' ';
			    ps.want_blank = false;
			    sprintf(e_code, "\n.De %dp+\200p\n", dec_ind * 7);
			    ps.dumped_decl_indent = 1;
			    e_code += strlen(e_code);
			} else {
			    int cur_dec_ind;
			    int pos, startpos;

			    /*
			     * in order to get the tab math right for
			     * indentations that are not multiples of 8 we
			     * need to modify both startpos and dec_ind
			     * (cur_dec_ind) here by eight minus the
			     * remainder of the current starting column
			     * divided by eight. This seems to be a
			     * properly working fix
			     */
			    startpos = e_code - s_code;
			    cur_dec_ind = dec_ind;
			    pos = startpos;
			    if ((ps.ind_level * ps.ind_size) % 8 != 0) {
				pos += (ps.ind_level * ps.ind_size) % 8;
				cur_dec_ind += (ps.ind_level * ps.ind_size) % 8;
			    }

			    if (tabs_to_var) {
				while ((pos & ~7) + 8 <= cur_dec_ind) {
				    CHECK_SIZE_CODE;
				    *e_code++ = '\t';
				    pos = (pos & ~7) + 8;
				}
			    }
			    while (pos < cur_dec_ind) {
				CHECK_SIZE_CODE;
				*e_code++ = ' ';
				pos++;
			    }
			    if (ps.want_blank && e_code - s_code == startpos)
				*e_code++ = ' ';
			    ps.want_blank = false;
			}
		    }
		} else {
		    if (ps.want_blank)
			*e_code++ = ' ';
		    ps.want_blank = false;
		    if (dec_ind && s_code != e_code) {
			*e_code = '\0';
			dump_line();
		    }
		    dec_ind = 0;
		}
	    }
	    else if (sp_sw && ps.p_l_follow == 0) {
		sp_sw = false;
		force_nl = true;
		ps.last_u_d = true;
		ps.in_stmt = false;
		parse(hd_type);
	    }
    copy_id:
	    if (ps.want_blank)
		*e_code++ = ' ';
	    if (troff && ps.its_a_keyword) {
		e_code = chfont(&bodyf, &keywordf, e_code);
		for (t_ptr = token; *t_ptr; ++t_ptr) {
		    CHECK_SIZE_CODE;
		    *e_code++ = keywordf.allcaps && islower(*t_ptr)
			? toupper(*t_ptr) : *t_ptr;
		}
		e_code = chfont(&keywordf, &bodyf, e_code);
	    }
	    else
		for (t_ptr = token; *t_ptr; ++t_ptr) {
		    CHECK_SIZE_CODE;
		    *e_code++ = *t_ptr;
		}
	    ps.want_blank = true;
	    break;

	case period:		/* treat a period kind of like a binary
				 * operation */
	    *e_code++ = '.';	/* move the period into line */
	    ps.want_blank = false;	/* dont put a blank after a period */
	    break;

	case comma:
	    ps.want_blank = (s_code != e_code);	/* only put blank after comma
						 * if comma does not start the
						 * line */
	    if (ps.in_decl && is_procname == 0 && !ps.block_init)
		while ((e_code - s_code) < (dec_ind - 1)) {
		    CHECK_SIZE_CODE;
		    *e_code++ = ' ';
		}

	    *e_code++ = ',';
	    if (ps.p_l_follow == 0) {
		if (ps.block_init_level <= 0)
		    ps.block_init = 0;
		if (break_comma && (!ps.leave_comma || compute_code_target() + (e_code - s_code) > max_col - 8))
		    force_nl = true;
	    }
	    break;

	case preesc:		/* got the character '#' */
	    if ((s_com != e_com) ||
		    (s_lab != e_lab) ||
		    (s_code != e_code))
		dump_line();
	    *e_lab++ = '#';	/* move whole line to 'label' buffer */
	    {
		int         in_comment = 0;
		int         com_start = 0;
		char        quote = 0;
		int         com_end = 0;

		while (*buf_ptr == ' ' || *buf_ptr == '\t') {
		    buf_ptr++;
		    if (buf_ptr >= buf_end)
			fill_buffer();
		}
		while (*buf_ptr != '\n' || (in_comment && !had_eof)) {
		    CHECK_SIZE_LAB;
		    *e_lab = *buf_ptr++;
		    if (buf_ptr >= buf_end)
			fill_buffer();
		    switch (*e_lab++) {
		    case BACKSLASH:
			if (troff)
			    *e_lab++ = BACKSLASH;
			if (!in_comment) {
			    *e_lab++ = *buf_ptr++;
			    if (buf_ptr >= buf_end)
				fill_buffer();
			}
			break;
		    case '/':
			if (*buf_ptr == '*' && !in_comment && !quote) {
			    in_comment = 1;
			    *e_lab++ = *buf_ptr++;
			    com_start = e_lab - s_lab - 2;
			}
			break;
		    case '"':
			if (quote == '"')
			    quote = 0;
			break;
		    case '\'':
			if (quote == '\'')
			    quote = 0;
			break;
		    case '*':
			if (*buf_ptr == '/' && in_comment) {
			    in_comment = 0;
			    *e_lab++ = *buf_ptr++;
			    com_end = e_lab - s_lab;
			}
			break;
		    }
		}

		while (e_lab > s_lab && (e_lab[-1] == ' ' || e_lab[-1] == '\t'))
		    e_lab--;
		if (e_lab - s_lab == com_end && bp_save == 0) {	/* comment on
								 * preprocessor line */
		    if (sc_end == 0)	/* if this is the first comment, we
					 * must set up the buffer */
			sc_end = &(save_com[0]);
		    else {
			*sc_end++ = '\n';	/* add newline between
						 * comments */
			*sc_end++ = ' ';
			--line_no;
		    }
		    bcopy(s_lab + com_start, sc_end, com_end - com_start);
		    sc_end += com_end - com_start;
		    if (sc_end >= &save_com[sc_size])
			abort();
		    e_lab = s_lab + com_start;
		    while (e_lab > s_lab && (e_lab[-1] == ' ' || e_lab[-1] == '\t'))
			e_lab--;
		    bp_save = buf_ptr;	/* save current input buffer */
		    be_save = buf_end;
		    buf_ptr = save_com;	/* fix so that subsequent calls to
					 * lexi will take tokens out of
					 * save_com */
		    *sc_end++ = ' ';	/* add trailing blank, just in case */
		    buf_end = sc_end;
		    sc_end = 0;
		}
		*e_lab = '\0';	/* null terminate line */
		ps.pcase = false;
	    }

	    if (strncmp(s_lab, "#if", 3) == 0) {
		if (blanklines_around_conditional_compilation) {
		    int c;
		    prefix_blankline_requested++;
		    while ((c = getc(input)) == '\n');
		    ungetc(c, input);
		}
		if ((size_t)ifdef_level < sizeof(state_stack)/sizeof(state_stack[0])) {
		    match_state[ifdef_level].tos = -1;
		    state_stack[ifdef_level++] = ps;
		}
		else
		    diag2(1, "#if stack overflow");
	    }
	    else if (strncmp(s_lab, "#else", 5) == 0)
		if (ifdef_level <= 0)
		    diag2(1, "Unmatched #else");
		else {
		    match_state[ifdef_level - 1] = ps;
		    ps = state_stack[ifdef_level - 1];
		}
	    else if (strncmp(s_lab, "#endif", 6) == 0) {
		if (ifdef_level <= 0)
		    diag2(1, "Unmatched #endif");
		else {
		    ifdef_level--;

#ifdef undef
		    /*
		     * This match needs to be more intelligent before the
		     * message is useful
		     */
		    if (match_state[ifdef_level].tos >= 0
			  && bcmp(&ps, &match_state[ifdef_level], sizeof ps))
			diag2(0, "Syntactically inconsistent #ifdef alternatives");
#endif
		}
		if (blanklines_around_conditional_compilation) {
		    postfix_blankline_requested++;
		    n_real_blanklines = 0;
		}
	    }
	    break;		/* subsequent processing of the newline
				 * character will cause the line to be printed */

	case comment:		/* we have gotten a / followed by * this is a biggie */
	    if (flushed_nl) {	/* we should force a broken line here */
		flushed_nl = false;
		dump_line();
		ps.want_blank = false;	/* dont insert blank at line start */
		force_nl = false;
	    }
	    pr_comment();
	    break;
	}			/* end of big switch stmt */

	*e_code = '\0';		/* make sure code section is null terminated */
	if (type_code != comment && type_code != newline && type_code != preesc)
	    ps.last_token = type_code;
    }				/* end of main while (1) loop */
}

/*
 * copy input file to backup file if in_name is /blah/blah/blah/file, then
 * backup file will be ".Bfile" then make the backup file the input and
 * original input file the output
 */
static void
bakcopy(void)
{
    int         n,
                bakchn;
    char        buff[8 * 1024];
    const char *p;

    /* construct file name .Bfile */
    for (p = in_name; *p; p++);	/* skip to end of string */
    while (p > in_name && *p != '/')	/* find last '/' */
	p--;
    if (*p == '/')
	p++;
    sprintf(bakfile, "%s.BAK", p);

    /* copy in_name to backup file */
    bakchn = creat(bakfile, 0600);
    if (bakchn < 0)
	err(1, "%s", bakfile);
    while ((n = read(fileno(input), buff, sizeof(buff))) > 0)
	if (write(bakchn, buff, n) != n)
	    err(1, "%s", bakfile);
    if (n < 0)
	err(1, "%s", in_name);
    close(bakchn);
    fclose(input);

    /* re-open backup file as the input file */
    input = fopen(bakfile, "r");
    if (input == NULL)
	err(1, "%s", bakfile);
    /* now the original input file will be the output */
    output = fopen(in_name, "w");
    if (output == NULL) {
	unlink(bakfile);
	err(1, "%s", in_name);
    }
}
