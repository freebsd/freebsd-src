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
 */

#if 0
#ifndef lint
static char sccsid[] = "@(#)args.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Argument scanning and profile reading code.  Default parameters are set
 * here as well.
 */

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "indent_globs.h"
#include "indent.h"

/* profile types */
#define	PRO_SPECIAL	1	/* special case */
#define	PRO_BOOL	2	/* boolean */
#define	PRO_INT		3	/* integer */
#define PRO_FONT	4	/* troff font */

/* profile specials for booleans */
#define	ON		1	/* turn it on */
#define	OFF		0	/* turn it off */

/* profile specials for specials */
#define	IGN		1	/* ignore it */
#define	CLI		2	/* case label indent (float) */
#define	STDIN		3	/* use stdin */
#define	KEY		4	/* type (keyword) */

static void scan_profile(FILE *);

const char *option_source = "?";

/*
 * N.B.: because of the way the table here is scanned, options whose names are
 * substrings of other options must occur later; that is, with -lp vs -l, -lp
 * must be first.  Also, while (most) booleans occur more than once, the last
 * default value is the one actually assigned.
 */
struct pro {
    const char *p_name;		/* name, e.g. -bl, -cli */
    int         p_type;		/* type (int, bool, special) */
    int         p_default;	/* the default value (if int) */
    int         p_special;	/* depends on type */
    int        *p_obj;		/* the associated variable */
}           pro[] = {

    {"T", PRO_SPECIAL, 0, KEY, 0},
    {"bacc", PRO_BOOL, false, ON, &blanklines_around_conditional_compilation},
    {"badp", PRO_BOOL, false, ON, &blanklines_after_declarations_at_proctop},
    {"bad", PRO_BOOL, false, ON, &blanklines_after_declarations},
    {"bap", PRO_BOOL, false, ON, &blanklines_after_procs},
    {"bbb", PRO_BOOL, false, ON, &blanklines_before_blockcomments},
    {"bc", PRO_BOOL, true, OFF, &ps.leave_comma},
    {"bl", PRO_BOOL, true, OFF, &btype_2},
    {"br", PRO_BOOL, true, ON, &btype_2},
    {"bs", PRO_BOOL, false, ON, &Bill_Shannon},
    {"cdb", PRO_BOOL, true, ON, &comment_delimiter_on_blankline},
    {"cd", PRO_INT, 0, 0, &ps.decl_com_ind},
    {"ce", PRO_BOOL, true, ON, &cuddle_else},
    {"ci", PRO_INT, 0, 0, &continuation_indent},
    {"cli", PRO_SPECIAL, 0, CLI, 0},
    {"c", PRO_INT, 33, 0, &ps.com_ind},
    {"di", PRO_INT, 16, 0, &ps.decl_indent},
    {"dj", PRO_BOOL, false, ON, &ps.ljust_decl},
    {"d", PRO_INT, 0, 0, &ps.unindent_displace},
    {"eei", PRO_BOOL, false, ON, &extra_expression_indent},
    {"ei", PRO_BOOL, true, ON, &ps.else_if},
    {"fbc", PRO_FONT, 0, 0, (int *) &blkcomf},
    {"fbs", PRO_BOOL, true, ON, &function_brace_split},
    {"fbx", PRO_FONT, 0, 0, (int *) &boxcomf},
    {"fb", PRO_FONT, 0, 0, (int *) &bodyf},
    {"fc1", PRO_BOOL, true, ON, &format_col1_comments},
    {"fcb", PRO_BOOL, true, ON, &format_block_comments},
    {"fc", PRO_FONT, 0, 0, (int *) &scomf},
    {"fk", PRO_FONT, 0, 0, (int *) &keywordf},
    {"fs", PRO_FONT, 0, 0, (int *) &stringf},
    {"ip", PRO_BOOL, true, ON, &ps.indent_parameters},
    {"i", PRO_INT, 8, 0, &ps.ind_size},
    {"lc", PRO_INT, 0, 0, &block_comment_max_col},
    {"ldi", PRO_INT, -1, 0, &ps.local_decl_indent},
    {"lp", PRO_BOOL, true, ON, &lineup_to_parens},
    {"l", PRO_INT, 78, 0, &max_col},
    {"nbacc", PRO_BOOL, false, OFF, &blanklines_around_conditional_compilation},
    {"nbadp", PRO_BOOL, false, OFF, &blanklines_after_declarations_at_proctop},
    {"nbad", PRO_BOOL, false, OFF, &blanklines_after_declarations},
    {"nbap", PRO_BOOL, false, OFF, &blanklines_after_procs},
    {"nbbb", PRO_BOOL, false, OFF, &blanklines_before_blockcomments},
    {"nbc", PRO_BOOL, true, ON, &ps.leave_comma},
    {"nbs", PRO_BOOL, false, OFF, &Bill_Shannon},
    {"ncdb", PRO_BOOL, true, OFF, &comment_delimiter_on_blankline},
    {"nce", PRO_BOOL, true, OFF, &cuddle_else},
    {"ndj", PRO_BOOL, false, OFF, &ps.ljust_decl},
    {"neei", PRO_BOOL, false, OFF, &extra_expression_indent},
    {"nei", PRO_BOOL, true, OFF, &ps.else_if},
    {"nfbs", PRO_BOOL, true, OFF, &function_brace_split},
    {"nfc1", PRO_BOOL, true, OFF, &format_col1_comments},
    {"nfcb", PRO_BOOL, true, OFF, &format_block_comments},
    {"nip", PRO_BOOL, true, OFF, &ps.indent_parameters},
    {"nlp", PRO_BOOL, true, OFF, &lineup_to_parens},
    {"npcs", PRO_BOOL, false, OFF, &proc_calls_space},
    {"npro", PRO_SPECIAL, 0, IGN, 0},
    {"npsl", PRO_BOOL, true, OFF, &procnames_start_line},
    {"nps", PRO_BOOL, false, OFF, &pointer_as_binop},
    {"nsc", PRO_BOOL, true, OFF, &star_comment_cont},
    {"nsob", PRO_BOOL, false, OFF, &swallow_optional_blanklines},
    {"nut", PRO_BOOL, true, OFF, &use_tabs},
    {"nv", PRO_BOOL, false, OFF, &verbose},
    {"pcs", PRO_BOOL, false, ON, &proc_calls_space},
    {"psl", PRO_BOOL, true, ON, &procnames_start_line},
    {"ps", PRO_BOOL, false, ON, &pointer_as_binop},
    {"sc", PRO_BOOL, true, ON, &star_comment_cont},
    {"sob", PRO_BOOL, false, ON, &swallow_optional_blanklines},
    {"st", PRO_SPECIAL, 0, STDIN, 0},
    {"troff", PRO_BOOL, false, ON, &troff},
    {"ut", PRO_BOOL, true, ON, &use_tabs},
    {"v", PRO_BOOL, false, ON, &verbose},
    /* whew! */
    {0, 0, 0, 0, 0}
};

/*
 * set_profile reads $HOME/.indent.pro and ./.indent.pro and handles arguments
 * given in these files.
 */
void
set_profile(void)
{
    FILE *f;
    char fname[PATH_MAX];
    static char prof[] = ".indent.pro";

    snprintf(fname, sizeof(fname), "%s/%s", getenv("HOME"), prof);
    if ((f = fopen(option_source = fname, "r")) != NULL) {
	scan_profile(f);
	(void) fclose(f);
    }
    if ((f = fopen(option_source = prof, "r")) != NULL) {
	scan_profile(f);
	(void) fclose(f);
    }
    option_source = "Command line";
}

static void
scan_profile(FILE *f)
{
    int		comment, i;
    char	*p;
    char        buf[BUFSIZ];

    while (1) {
	p = buf;
	comment = 0;
	while ((i = getc(f)) != EOF) {
	    if (i == '*' && !comment && p > buf && p[-1] == '/') {
		comment = p - buf;
		*p++ = i;
	    } else if (i == '/' && comment && p > buf && p[-1] == '*') {
		p = buf + comment - 1;
		comment = 0;
	    } else if (isspace(i)) {
		if (p > buf && !comment)
		    break;
	    } else {
		*p++ = i;
	    }
	}
	if (p != buf) {
	    *p++ = 0;
	    if (verbose)
		printf("profile: %s\n", buf);
	    set_option(buf);
	}
	else if (i == EOF)
	    return;
    }
}

const char	*param_start;

static int
eqin(const char *s1, const char *s2)
{
    while (*s1) {
	if (*s1++ != *s2++)
	    return (false);
    }
    param_start = s2;
    return (true);
}

/*
 * Set the defaults.
 */
void
set_defaults(void)
{
    struct pro *p;

    /*
     * Because ps.case_indent is a float, we can't initialize it from the
     * table:
     */
    ps.case_indent = 0.0;	/* -cli0.0 */
    for (p = pro; p->p_name; p++)
	if (p->p_type != PRO_SPECIAL && p->p_type != PRO_FONT)
	    *p->p_obj = p->p_default;
}

void
set_option(char *arg)
{
    struct pro *p;

    arg++;			/* ignore leading "-" */
    for (p = pro; p->p_name; p++)
	if (*p->p_name == *arg && eqin(p->p_name, arg))
	    goto found;
    errx(1, "%s: unknown parameter \"%s\"", option_source, arg - 1);
found:
    switch (p->p_type) {

    case PRO_SPECIAL:
	switch (p->p_special) {

	case IGN:
	    break;

	case CLI:
	    if (*param_start == 0)
		goto need_param;
	    ps.case_indent = atof(param_start);
	    break;

	case STDIN:
	    if (input == 0)
		input = stdin;
	    if (output == 0)
		output = stdout;
	    break;

	case KEY:
	    if (*param_start == 0)
		goto need_param;
	    {
		char *str = strdup(param_start);
		if (str == NULL)
			err(1, NULL);
		addkey(str, 4);
	    }
	    break;

	default:
	    errx(1, "set_option: internal error: p_special %d", p->p_special);
	}
	break;

    case PRO_BOOL:
	if (p->p_special == OFF)
	    *p->p_obj = false;
	else
	    *p->p_obj = true;
	break;

    case PRO_INT:
	if (!isdigit(*param_start)) {
    need_param:
	    errx(1, "%s: ``%s'' requires a parameter", option_source, arg - 1);
	}
	*p->p_obj = atoi(param_start);
	break;

    case PRO_FONT:
	parsefont((struct fstate *) p->p_obj, param_start);
	break;

    default:
	errx(1, "set_option: internal error: p_type %d", p->p_type);
    }
}
