/*-
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
 */

#if 0
#ifndef lint
static char sccsid[] = "@(#)lexi.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Here we have the token scanner for indent.  It scans off one token and puts
 * it in the global variable "token".  It returns a code, indicating the type
 * of token scanned.
 */

#include <err.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "indent_globs.h"
#include "indent_codes.h"
#include "indent.h"

#define alphanum 1
#define opchar 3

struct templ {
    const char *rwd;
    int         rwcode;
};

struct templ specials[1000] =
{
    {"switch", 7},
    {"case", 8},
    {"break", 9},
    {"struct", 3},
    {"union", 3},
    {"enum", 3},
    {"default", 8},
    {"int", 4},
    {"char", 4},
    {"float", 4},
    {"double", 4},
    {"long", 4},
    {"short", 4},
    {"typedef", 4},
    {"unsigned", 4},
    {"register", 4},
    {"static", 4},
    {"global", 4},
    {"extern", 4},
    {"void", 4},
    {"const", 4},
    {"volatile", 4},
    {"goto", 9},
    {"return", 9},
    {"if", 5},
    {"while", 5},
    {"for", 5},
    {"else", 6},
    {"do", 6},
    {"sizeof", 2},
    {"offsetof", 1},
    {0, 0}
};

char        chartype[128] =
{				/* this is used to facilitate the decision of
				 * what type (alphanumeric, operator) each
				 * character is */
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 3, 0, 0, 1, 3, 3, 0,
    0, 0, 3, 3, 0, 3, 0, 3,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 0, 0, 3, 3, 3, 3,
    0, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 0, 3, 1,
    0, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 3, 0, 3, 0
};

int
lexi(void)
{
    int         unary_delim;	/* this is set to 1 if the current token
				 * forces a following operator to be unary */
    static int  last_code;	/* the last token type returned */
    static int  l_struct;	/* set to 1 if the last token was 'struct' */
    int         code;		/* internal code to be returned */
    char        qchar;		/* the delimiter character for a string */

    e_token = s_token;		/* point to start of place to save token */
    unary_delim = false;
    ps.col_1 = ps.last_nl;	/* tell world that this token started in
				 * column 1 iff the last thing scanned was nl */
    ps.last_nl = false;

    while (*buf_ptr == ' ' || *buf_ptr == '\t') {	/* get rid of blanks */
	ps.col_1 = false;	/* leading blanks imply token is not in column
				 * 1 */
	if (++buf_ptr >= buf_end)
	    fill_buffer();
    }

    /* Scan an alphanumeric token */
    if (chartype[(int)*buf_ptr] == alphanum || (buf_ptr[0] == '.' && isdigit(buf_ptr[1]))) {
	/*
	 * we have a character or number
	 */
	const char *j;		/* used for searching thru list of
				 *
				 * reserved words */
	struct templ *p;

	if (isdigit(*buf_ptr) || (buf_ptr[0] == '.' && isdigit(buf_ptr[1]))) {
	    int         seendot = 0,
	                seenexp = 0,
			seensfx = 0;
	    if (*buf_ptr == '0' &&
		    (buf_ptr[1] == 'x' || buf_ptr[1] == 'X')) {
		*e_token++ = *buf_ptr++;
		*e_token++ = *buf_ptr++;
		while (isxdigit(*buf_ptr)) {
		    CHECK_SIZE_TOKEN;
		    *e_token++ = *buf_ptr++;
		}
	    }
	    else
		while (1) {
		    if (*buf_ptr == '.') {
			if (seendot)
			    break;
			else
			    seendot++;
		    }
		    CHECK_SIZE_TOKEN;
		    *e_token++ = *buf_ptr++;
		    if (!isdigit(*buf_ptr) && *buf_ptr != '.') {
			if ((*buf_ptr != 'E' && *buf_ptr != 'e') || seenexp)
			    break;
			else {
			    seenexp++;
			    seendot++;
			    CHECK_SIZE_TOKEN;
			    *e_token++ = *buf_ptr++;
			    if (*buf_ptr == '+' || *buf_ptr == '-')
				*e_token++ = *buf_ptr++;
			}
		    }
		}
	    while (1) {
		if (!(seensfx & 1) && (*buf_ptr == 'U' || *buf_ptr == 'u')) {
		    CHECK_SIZE_TOKEN;
		    *e_token++ = *buf_ptr++;
		    seensfx |= 1;
		    continue;
		}
		if (!(seensfx & 2) && (strchr("fFlL", *buf_ptr) != NULL)) {
		    CHECK_SIZE_TOKEN;
		    if (buf_ptr[1] == buf_ptr[0])
		        *e_token++ = *buf_ptr++;
		    *e_token++ = *buf_ptr++;
		    seensfx |= 2;
		    continue;
		}
		break;
	    }
	}
	else
	    while (chartype[(int)*buf_ptr] == alphanum || *buf_ptr == BACKSLASH) {
		/* fill_buffer() terminates buffer with newline */
		if (*buf_ptr == BACKSLASH) {
		    if (*(buf_ptr + 1) == '\n') {
			buf_ptr += 2;
			if (buf_ptr >= buf_end)
			    fill_buffer();
			} else
			    break;
		}
		CHECK_SIZE_TOKEN;
		/* copy it over */
		*e_token++ = *buf_ptr++;
		if (buf_ptr >= buf_end)
		    fill_buffer();
	    }
	*e_token++ = '\0';
	while (*buf_ptr == ' ' || *buf_ptr == '\t') {	/* get rid of blanks */
	    if (++buf_ptr >= buf_end)
		fill_buffer();
	}
	ps.keyword = 0;
	if (l_struct && !ps.p_l_follow) {
				/* if last token was 'struct' and we're not
				 * in parentheses, then this token
				 * should be treated as a declaration */
	    l_struct = false;
	    last_code = ident;
	    ps.last_u_d = true;
	    return (decl);
	}
	ps.last_u_d = l_struct;	/* Operator after identifier is binary
				 * unless last token was 'struct' */
	l_struct = false;
	last_code = ident;	/* Remember that this is the code we will
				 * return */

	if (auto_typedefs) {
	    const char *q = s_token;
	    size_t q_len = strlen(q);
	    /* Check if we have an "_t" in the end */
	    if (q_len > 2 &&
	        (strcmp(q + q_len - 2, "_t") == 0)) {
	        ps.keyword = 4;	/* a type name */
		ps.last_u_d = true;
	        goto found_auto_typedef;
	    }
	}

	/*
	 * This loop will check if the token is a keyword.
	 */
	for (p = specials; (j = p->rwd) != NULL; p++) {
	    const char *q = s_token;	/* point at scanned token */
	    if (*j++ != *q++ || *j++ != *q++)
		continue;	/* This test depends on the fact that
				 * identifiers are always at least 1 character
				 * long (ie. the first two bytes of the
				 * identifier are always meaningful) */
	    if (q[-1] == 0)
		break;		/* If its a one-character identifier */
	    while (*q++ == *j)
		if (*j++ == 0)
		    goto found_keyword;	/* I wish that C had a multi-level
					 * break... */
	}
	if (p->rwd) {		/* we have a keyword */
    found_keyword:
	    ps.keyword = p->rwcode;
	    ps.last_u_d = true;
	    switch (p->rwcode) {
	    case 7:		/* it is a switch */
		return (swstmt);
	    case 8:		/* a case or default */
		return (casestmt);

	    case 3:		/* a "struct" */
		/*
		 * Next time around, we will want to know that we have had a
		 * 'struct'
		 */
		l_struct = true;
		/* FALLTHROUGH */

	    case 4:		/* one of the declaration keywords */
	    found_auto_typedef:
		if (ps.p_l_follow) {
		    /* inside parens: cast, param list, offsetof or sizeof */
		    ps.cast_mask |= (1 << ps.p_l_follow) & ~ps.not_cast_mask;
		    break;
		}
		last_code = decl;
		return (decl);

	    case 5:		/* if, while, for */
		return (sp_paren);

	    case 6:		/* do, else */
		return (sp_nparen);

	    default:		/* all others are treated like any other
				 * identifier */
		return (ident);
	    }			/* end of switch */
	}			/* end of if (found_it) */
	if (*buf_ptr == '(' && ps.tos <= 1 && ps.ind_level == 0) {
	    char *tp = buf_ptr;
	    while (tp < buf_end)
		if (*tp++ == ')' && (*tp == ';' || *tp == ','))
		    goto not_proc;
	    strncpy(ps.procname, token, sizeof ps.procname - 1);
	    ps.in_parameter_declaration = 1;
	    rparen_count = 1;
    not_proc:;
	}
	/*
	 * The following hack attempts to guess whether or not the current
	 * token is in fact a declaration keyword -- one that has been
	 * typedefd
	 */
	if (((*buf_ptr == '*' && buf_ptr[1] != '=') || isalpha(*buf_ptr) || *buf_ptr == '_')
		&& !ps.p_l_follow
	        && !ps.block_init
		&& (ps.last_token == rparen || ps.last_token == semicolon ||
		    ps.last_token == decl ||
		    ps.last_token == lbrace || ps.last_token == rbrace)) {
	    ps.keyword = 4;	/* a type name */
	    ps.last_u_d = true;
	    last_code = decl;
	    return decl;
	}
	if (last_code == decl)	/* if this is a declared variable, then
				 * following sign is unary */
	    ps.last_u_d = true;	/* will make "int a -1" work */
	last_code = ident;
	return (ident);		/* the ident is not in the list */
    }				/* end of procesing for alpanum character */

    /* Scan a non-alphanumeric token */

    *e_token++ = *buf_ptr;		/* if it is only a one-character token, it is
				 * moved here */
    *e_token = '\0';
    if (++buf_ptr >= buf_end)
	fill_buffer();

    switch (*token) {
    case '\n':
	unary_delim = ps.last_u_d;
	ps.last_nl = true;	/* remember that we just had a newline */
	code = (had_eof ? 0 : newline);

	/*
	 * if data has been exhausted, the newline is a dummy, and we should
	 * return code to stop
	 */
	break;

    case '\'':			/* start of quoted character */
    case '"':			/* start of string */
	qchar = *token;
	if (troff) {
	    e_token[-1] = '`';
	    if (qchar == '"')
		*e_token++ = '`';
	    e_token = chfont(&bodyf, &stringf, e_token);
	}
	do {			/* copy the string */
	    while (1) {		/* move one character or [/<char>]<char> */
		if (*buf_ptr == '\n') {
		    diag2(1, "Unterminated literal");
		    goto stop_lit;
		}
		CHECK_SIZE_TOKEN;	/* Only have to do this once in this loop,
					 * since CHECK_SIZE guarantees that there
					 * are at least 5 entries left */
		*e_token = *buf_ptr++;
		if (buf_ptr >= buf_end)
		    fill_buffer();
		if (*e_token == BACKSLASH) {	/* if escape, copy extra char */
		    if (*buf_ptr == '\n')	/* check for escaped newline */
			++line_no;
		    if (troff) {
			*++e_token = BACKSLASH;
			if (*buf_ptr == BACKSLASH)
			    *++e_token = BACKSLASH;
		    }
		    *++e_token = *buf_ptr++;
		    ++e_token;	/* we must increment this again because we
				 * copied two chars */
		    if (buf_ptr >= buf_end)
			fill_buffer();
		}
		else
		    break;	/* we copied one character */
	    }			/* end of while (1) */
	} while (*e_token++ != qchar);
	if (troff) {
	    e_token = chfont(&stringf, &bodyf, e_token - 1);
	    if (qchar == '"')
		*e_token++ = '\'';
	}
stop_lit:
	code = ident;
	break;

    case ('('):
    case ('['):
	unary_delim = true;
	code = lparen;
	break;

    case (')'):
    case (']'):
	code = rparen;
	break;

    case '#':
	unary_delim = ps.last_u_d;
	code = preesc;
	break;

    case '?':
	unary_delim = true;
	code = question;
	break;

    case (':'):
	code = colon;
	unary_delim = true;
	break;

    case (';'):
	unary_delim = true;
	code = semicolon;
	break;

    case ('{'):
	unary_delim = true;

	/*
	 * if (ps.in_or_st) ps.block_init = 1;
	 */
	/* ?	code = ps.block_init ? lparen : lbrace; */
	code = lbrace;
	break;

    case ('}'):
	unary_delim = true;
	/* ?	code = ps.block_init ? rparen : rbrace; */
	code = rbrace;
	break;

    case 014:			/* a form feed */
	unary_delim = ps.last_u_d;
	ps.last_nl = true;	/* remember this so we can set 'ps.col_1'
				 * right */
	code = form_feed;
	break;

    case (','):
	unary_delim = true;
	code = comma;
	break;

    case '.':
	unary_delim = false;
	code = period;
	break;

    case '-':
    case '+':			/* check for -, +, --, ++ */
	code = (ps.last_u_d ? unary_op : binary_op);
	unary_delim = true;

	if (*buf_ptr == token[0]) {
	    /* check for doubled character */
	    *e_token++ = *buf_ptr++;
	    /* buffer overflow will be checked at end of loop */
	    if (last_code == ident || last_code == rparen) {
		code = (ps.last_u_d ? unary_op : postop);
		/* check for following ++ or -- */
		unary_delim = false;
	    }
	}
	else if (*buf_ptr == '=')
	    /* check for operator += */
	    *e_token++ = *buf_ptr++;
	else if (*buf_ptr == '>') {
	    /* check for operator -> */
	    *e_token++ = *buf_ptr++;
	    if (!pointer_as_binop) {
		unary_delim = false;
		code = unary_op;
		ps.want_blank = false;
	    }
	}
	break;			/* buffer overflow will be checked at end of
				 * switch */

    case '=':
	if (ps.in_or_st)
	    ps.block_init = 1;
#ifdef undef
	if (chartype[*buf_ptr] == opchar) {	/* we have two char assignment */
	    e_token[-1] = *buf_ptr++;
	    if ((e_token[-1] == '<' || e_token[-1] == '>') && e_token[-1] == *buf_ptr)
		*e_token++ = *buf_ptr++;
	    *e_token++ = '=';	/* Flip =+ to += */
	    *e_token = 0;
	}
#else
	if (*buf_ptr == '=') {/* == */
	    *e_token++ = '=';	/* Flip =+ to += */
	    buf_ptr++;
	    *e_token = 0;
	}
#endif
	code = binary_op;
	unary_delim = true;
	break;
	/* can drop thru!!! */

    case '>':
    case '<':
    case '!':			/* ops like <, <<, <=, !=, etc */
	if (*buf_ptr == '>' || *buf_ptr == '<' || *buf_ptr == '=') {
	    *e_token++ = *buf_ptr;
	    if (++buf_ptr >= buf_end)
		fill_buffer();
	}
	if (*buf_ptr == '=')
	    *e_token++ = *buf_ptr++;
	code = (ps.last_u_d ? unary_op : binary_op);
	unary_delim = true;
	break;

    default:
	if (token[0] == '/' && *buf_ptr == '*') {
	    /* it is start of comment */
	    *e_token++ = '*';

	    if (++buf_ptr >= buf_end)
		fill_buffer();

	    code = comment;
	    unary_delim = ps.last_u_d;
	    break;
	}
	while (*(e_token - 1) == *buf_ptr || *buf_ptr == '=') {
	    /*
	     * handle ||, &&, etc, and also things as in int *****i
	     */
	    *e_token++ = *buf_ptr;
	    if (++buf_ptr >= buf_end)
		fill_buffer();
	}
	code = (ps.last_u_d ? unary_op : binary_op);
	unary_delim = true;


    }				/* end of switch */
    if (code != newline) {
	l_struct = false;
	last_code = code;
    }
    if (buf_ptr >= buf_end)	/* check for input buffer empty */
	fill_buffer();
    ps.last_u_d = unary_delim;
    *e_token = '\0';		/* null terminate the token */
    return (code);
}

/*
 * Add the given keyword to the keyword table, using val as the keyword type
 */
void
addkey(char *key, int val)
{
    struct templ *p = specials;
    while (p->rwd)
	if (p->rwd[0] == key[0] && strcmp(p->rwd, key) == 0)
	    return;
	else
	    p++;
    if (p >= specials + sizeof specials / sizeof specials[0])
	return;			/* For now, table overflows are silently
				 * ignored */
    p->rwd = key;
    p->rwcode = val;
    p[1].rwd = NULL;
    p[1].rwcode = 0;
}
