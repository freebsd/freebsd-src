/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
static char sccsid[] = "@(#)lcmd.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] = "@(#)$FreeBSD$";
#endif /* not lint */

#include "defs.h"
#include "value.h"
#include "lcmd.h"

int l_alias();
int l_close();
int l_cursormodes();
int l_debug();
int l_def_nline();
int l_def_shell();
int l_def_smooth();
int l_echo();
int l_escape();
int l_foreground();
int l_iostat();
int l_label();
int l_list();
int l_select();
int l_smooth();
int l_source();
int l_terse();
int l_time();
int l_unalias();
int l_unset();
int l_variable();
int l_window();
int l_write();

extern struct lcmd_arg arg_alias[];
extern struct lcmd_arg arg_cursormodes[];
extern struct lcmd_arg arg_debug[];
extern struct lcmd_arg arg_echo[];
extern struct lcmd_arg arg_escape[];
extern struct lcmd_arg arg_foreground[];
extern struct lcmd_arg arg_label[];
extern struct lcmd_arg arg_def_nline[];
extern struct lcmd_arg arg_def_shell[];
extern struct lcmd_arg arg_def_smooth[];
extern struct lcmd_arg arg_close[];
extern struct lcmd_arg arg_select[];
extern struct lcmd_arg arg_smooth[];
extern struct lcmd_arg arg_source[];
extern struct lcmd_arg arg_terse[];
extern struct lcmd_arg arg_time[];
extern struct lcmd_arg arg_unalias[];
extern struct lcmd_arg arg_unset[];
extern struct lcmd_arg arg_window[];
extern struct lcmd_arg arg_write[];
struct lcmd_arg arg_null[1] = { { 0 } };

struct lcmd_tab lcmd_tab[] = {
	"alias",		1,	l_alias,	arg_alias,
	"close",		2,	l_close,	arg_close,
	"cursormodes",		2,	l_cursormodes,	arg_cursormodes,
	"debug",		1,	l_debug,	arg_debug,
	"default_nlines",	9,	l_def_nline,	arg_def_nline,
	"default_shell",	10,	l_def_shell,	arg_def_shell,
	"default_smooth",	10,	l_def_smooth,	arg_def_smooth,
	"echo",			2,	l_echo,		arg_echo,
	"escape",		2,	l_escape,	arg_escape,
	"foreground",		1,	l_foreground,	arg_foreground,
	"iostat",		1,	l_iostat,	arg_null,
	"label",		2,	l_label,	arg_label,
	"list",			2,	l_list,		arg_null,
	"nlines",		1,	l_def_nline,	arg_def_nline,
	"select",		2,	l_select,	arg_select,
	"shell",		2,	l_def_shell,	arg_def_shell,
	"smooth",		2,	l_smooth,	arg_smooth,
	"source",		2,	l_source,	arg_source,
	"terse",		2,	l_terse,	arg_terse,
	"time",			2,	l_time,		arg_time,
	"unalias",		3,	l_unalias,	arg_unalias,
	"unset",		3,	l_unset,	arg_unset,
	"variable",		1,	l_variable,	arg_null,
	"window",		2,	l_window,	arg_window,
	"write",		2,	l_write,	arg_write,
	0
};

struct lcmd_tab *
lcmd_lookup(name)
char *name;
{
	register struct lcmd_tab *p;

	for (p = lcmd_tab; p->lc_name != 0; p++)
		if (str_match(name, p->lc_name, p->lc_minlen))
			return p;
	return 0;
}

dosource(filename)
char *filename;
{
	if (cx_beginfile(filename) < 0)
		return -1;
	p_start();
	err_end();
	cx_end();
	return 0;
}

dolongcmd(buffer, arg, narg)
char *buffer;
struct value *arg;
int narg;
{
	if (cx_beginbuf(buffer, arg, narg) < 0)
		return -1;
	p_start();
	err_end();
	cx_end();
	return 0;
}
