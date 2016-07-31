/*
 * Copyright (c) 1985 Sun Microsystems, Inc.
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 * All rights reserved.
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
 *
 *	@(#)indent_globs.h	8.1 (Berkeley) 6/6/93
 * $FreeBSD$
 */

#define BACKSLASH '\\'
#define bufsize 200		/* size of internal buffers */
#define sc_size 5000		/* size of save_com buffer */
#define label_offset 2		/* number of levels a label is placed to left
				 * of code */

#define tabsize 8		/* the size of a tab */
#define tabmask 0177770		/* mask used when figuring length of lines
				 * with tabs */


#define false 0
#define true  1


FILE       *input;		/* the fid for the input file */
FILE       *output;		/* the output file */

#define CHECK_SIZE_CODE \
	if (e_code >= l_code) { \
	    int nsize = l_code-s_code+400; \
	    codebuf = (char *) realloc(codebuf, nsize); \
	    if (codebuf == NULL) \
		err(1, NULL); \
	    e_code = codebuf + (e_code-s_code) + 1; \
	    l_code = codebuf + nsize - 5; \
	    s_code = codebuf + 1; \
	}
#define CHECK_SIZE_COM \
	if (e_com >= l_com) { \
	    int nsize = l_com-s_com+400; \
	    combuf = (char *) realloc(combuf, nsize); \
	    if (combuf == NULL) \
		err(1, NULL); \
	    e_com = combuf + (e_com-s_com) + 1; \
	    last_bl = combuf + (last_bl-s_com) + 1; \
	    l_com = combuf + nsize - 5; \
	    s_com = combuf + 1; \
	}
#define CHECK_SIZE_LAB \
	if (e_lab >= l_lab) { \
	    int nsize = l_lab-s_lab+400; \
	    labbuf = (char *) realloc(labbuf, nsize); \
	    if (labbuf == NULL) \
		err(1, NULL); \
	    e_lab = labbuf + (e_lab-s_lab) + 1; \
	    l_lab = labbuf + nsize - 5; \
	    s_lab = labbuf + 1; \
	}
#define CHECK_SIZE_TOKEN \
	if (e_token >= l_token) { \
	    int nsize = l_token-s_token+400; \
	    tokenbuf = (char *) realloc(tokenbuf, nsize); \
	    if (tokenbuf == NULL) \
		err(1, NULL); \
	    e_token = tokenbuf + (e_token-s_token) + 1; \
	    l_token = tokenbuf + nsize - 5; \
	    s_token = tokenbuf + 1; \
	}

char       *labbuf;		/* buffer for label */
char       *s_lab;		/* start ... */
char       *e_lab;		/* .. and end of stored label */
char       *l_lab;		/* limit of label buffer */

char       *codebuf;		/* buffer for code section */
char       *s_code;		/* start ... */
char       *e_code;		/* .. and end of stored code */
char       *l_code;		/* limit of code section */

char       *combuf;		/* buffer for comments */
char       *s_com;		/* start ... */
char       *e_com;		/* ... and end of stored comments */
char       *l_com;		/* limit of comment buffer */

#define token s_token
char       *tokenbuf;		/* the last token scanned */
char	   *s_token;
char       *e_token;
char	   *l_token;

char       *in_buffer;		/* input buffer */
char	   *in_buffer_limit;	/* the end of the input buffer */
char       *buf_ptr;		/* ptr to next character to be taken from
				 * in_buffer */
char       *buf_end;		/* ptr to first after last char in in_buffer */

char        save_com[sc_size];	/* input text is saved here when looking for
				 * the brace after an if, while, etc */
char       *sc_end;		/* pointer into save_com buffer */

char       *bp_save;		/* saved value of buf_ptr when taking input
				 * from save_com */
char       *be_save;		/* similarly saved value of buf_end */


int         found_err;
int         pointer_as_binop;
int         blanklines_after_declarations;
int         blanklines_before_blockcomments;
int         blanklines_after_procs;
int         blanklines_around_conditional_compilation;
int         swallow_optional_blanklines;
int         n_real_blanklines;
int         prefix_blankline_requested;
int         postfix_blankline_requested;
int         break_comma;	/* when true and not in parens, break after a
				 * comma */
int         btype_2;		/* when true, brace should be on same line as
				 * if, while, etc */
float       case_ind;		/* indentation level to be used for a "case
				 * n:" */
int         code_lines;		/* count of lines with code */
int         had_eof;		/* set to true when input is exhausted */
int         line_no;		/* the current line number. */
int         max_col;		/* the maximum allowable line length */
int         verbose;		/* when true, non-essential error messages are
				 * printed */
int         cuddle_else;	/* true if else should cuddle up to '}' */
int         star_comment_cont;	/* true iff comment continuation lines should
				 * have stars at the beginning of each line. */
int         comment_delimiter_on_blankline;
int         troff;		/* true iff were generating troff input */
int         procnames_start_line;	/* if true, the names of procedures
					 * being defined get placed in column
					 * 1 (ie. a newline is placed between
					 * the type of the procedure and its
					 * name) */
int         proc_calls_space;	/* If true, procedure calls look like:
				 * foo(bar) rather than foo (bar) */
int         format_block_comments;	/* true if comments beginning with
					 * `/ * \n' are to be reformatted */
int         format_col1_comments;	/* If comments which start in column 1
					 * are to be magically reformatted
					 * (just like comments that begin in
					 * later columns) */
int         inhibit_formatting;	/* true if INDENT OFF is in effect */
int         suppress_blanklines;/* set iff following blanklines should be
				 * suppressed */
int         continuation_indent;/* set to the indentation between the edge of
				 * code and continuation lines */
int         lineup_to_parens;	/* if true, continued code within parens will
				 * be lined up to the open paren */
int         Bill_Shannon;	/* true iff a blank should always be inserted
				 * after sizeof */
int         blanklines_after_declarations_at_proctop;	/* This is vaguely
							 * similar to
							 * blanklines_after_decla
							 * rations except that
							 * it only applies to
							 * the first set of
							 * declarations in a
							 * procedure (just after
							 * the first '{') and it
							 * causes a blank line
							 * to be generated even
							 * if there are no
							 * declarations */
int         block_comment_max_col;
int         extra_expression_indent;	/* true if continuation lines from the
					 * expression part of "if(e)",
					 * "while(e)", "for(e;e;e)" should be
					 * indented an extra tab stop so that
					 * they don't conflict with the code
					 * that follows */
int	    function_brace_split;	/* split function declaration and
					 * brace onto separate lines */
int	    use_tabs;			/* set true to use tabs for spacing,
					 * false uses all spaces */
int	    auto_typedefs;		/* set true to recognize identifiers
					 * ending in "_t" like typedefs */

/* -troff font state information */

struct fstate {
    char        font[4];
    char        size;
    int         allcaps:1;
} __aligned(sizeof(int));
char       *chfont(struct fstate *, struct fstate *, char *);

struct fstate
            keywordf,		/* keyword font */
            stringf,		/* string font */
            boxcomf,		/* Box comment font */
            blkcomf,		/* Block comment font */
            scomf,		/* Same line comment font */
            bodyf;		/* major body font */


#define	STACKSIZE 256

struct parser_state {
    int         last_token;
    struct fstate cfont;	/* Current font */
    int         p_stack[STACKSIZE];	/* this is the parsers stack */
    int         il[STACKSIZE];	/* this stack stores indentation levels */
    float       cstk[STACKSIZE];/* used to store case stmt indentation levels */
    int         box_com;	/* set to true when we are in a "boxed"
				 * comment. In that case, the first non-blank
				 * char should be lined up with the / in / followed by * */
    int         comment_delta,
                n_comment_delta;
    int         cast_mask;	/* indicates which close parens close off
				 * casts */
    int         sizeof_mask;	/* indicates which close parens close off
				 * sizeof''s */
    int         block_init;	/* true iff inside a block initialization */
    int         block_init_level;	/* The level of brace nesting in an
					 * initialization */
    int         last_nl;	/* this is true if the last thing scanned was
				 * a newline */
    int         in_or_st;	/* Will be true iff there has been a
				 * declarator (e.g. int or char) and no left
				 * paren since the last semicolon. When true,
				 * a '{' is starting a structure definition or
				 * an initialization list */
    int         bl_line;	/* set to 1 by dump_line if the line is blank */
    int         col_1;		/* set to true if the last token started in
				 * column 1 */
    int         com_col;	/* this is the column in which the current
				 * comment should start */
    int         com_ind;	/* the column in which comments to the right
				 * of code should start */
    int         com_lines;	/* the number of lines with comments, set by
				 * dump_line */
    int         dec_nest;	/* current nesting level for structure or init */
    int         decl_com_ind;	/* the column in which comments after
				 * declarations should be put */
    int         decl_on_line;	/* set to true if this line of code has part
				 * of a declaration on it */
    int         i_l_follow;	/* the level to which ind_level should be set
				 * after the current line is printed */
    int         in_decl;	/* set to true when we are in a declaration
				 * stmt.  The processing of braces is then
				 * slightly different */
    int         in_stmt;	/* set to 1 while in a stmt */
    int         ind_level;	/* the current indentation level */
    int         ind_size;	/* the size of one indentation level */
    int         ind_stmt;	/* set to 1 if next line should have an extra
				 * indentation level because we are in the
				 * middle of a stmt */
    int         last_u_d;	/* set to true after scanning a token which
				 * forces a following operator to be unary */
    int         leave_comma;	/* if true, never break declarations after
				 * commas */
    int         ljust_decl;	/* true if declarations should be left
				 * justified */
    int         out_coms;	/* the number of comments processed, set by
				 * pr_comment */
    int         out_lines;	/* the number of lines written, set by
				 * dump_line */
    int         p_l_follow;	/* used to remember how to indent following
				 * statement */
    int         paren_level;	/* parenthesization level. used to indent
				 * within statements */
    short       paren_indents[20];	/* column positions of each paren */
    int         pcase;		/* set to 1 if the current line label is a
				 * case.  It is printed differently from a
				 * regular label */
    int         search_brace;	/* set to true by parse when it is necessary
				 * to buffer up all info up to the start of a
				 * stmt after an if, while, etc */
    int         unindent_displace;	/* comments not to the right of code
					 * will be placed this many
					 * indentation levels to the left of
					 * code */
    int         use_ff;		/* set to one if the current line should be
				 * terminated with a form feed */
    int         want_blank;	/* set to true when the following token should
				 * be prefixed by a blank. (Said prefixing is
				 * ignored in some cases.) */
    int         else_if;	/* True iff else if pairs should be handled
				 * specially */
    int         decl_indent;	/* column to indent declared identifiers to */
    int         local_decl_indent;	/* like decl_indent but for locals */
    int         its_a_keyword;
    int         sizeof_keyword;
    int         dumped_decl_indent;
    float       case_indent;	/* The distance to indent case labels from the
				 * switch statement */
    int         in_parameter_declaration;
    int         indent_parameters;
    int         tos;		/* pointer to top of stack */
    char        procname[100];	/* The name of the current procedure */
    int         just_saw_decl;
}           ps;

int         ifdef_level;
int	    rparen_count;
struct parser_state state_stack[5];
struct parser_state match_state[5];
