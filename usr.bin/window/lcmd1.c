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
static char sccsid[] = "@(#)lcmd1.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] = "@(#)$FreeBSD$";
#endif /* not lint */

#include "defs.h"
#include <string.h> /* System string definitions. */
#include "mystring.h" /* Local string definitions. */
#include "value.h"
#include "lcmd.h"
#include "var.h"

struct lcmd_arg arg_window[] = {
	{ "row",	1,	ARG_NUM },
	{ "column",	1,	ARG_NUM },
	{ "nrows",	2,	ARG_NUM },
	{ "ncols",	2,	ARG_NUM },
	{ "nlines",	2,	ARG_NUM },
	{ "label",	1,	ARG_STR },
	{ "pty",	1,	ARG_ANY },
	{ "frame",	1,	ARG_ANY },
	{ "mapnl",	1,	ARG_ANY },
	{ "keepopen",	1,	ARG_ANY },
	{ "smooth",	1,	ARG_ANY },
	{ "shell",	1,	ARG_STR|ARG_LIST },
	0
};

l_window(v, a)
struct value *v;
register struct value *a;
{
	struct ww *w;
	int col, row, ncol, nrow, id, nline;
	char *label;
	char haspty, hasframe, mapnl, keepopen, smooth;
	char *shf, **sh;
	char *argv[sizeof default_shell / sizeof *default_shell];
	register char **pp;

	if ((id = findid()) < 0)
		return;
	row = a->v_type == V_ERR ? 1 : a->v_num;
	a++;
	col = a->v_type == V_ERR ? 0 : a->v_num;
	a++;
	nrow = a->v_type == V_ERR ? wwnrow - row : a->v_num;
	a++;
	ncol = a->v_type == V_ERR ? wwncol - col : a->v_num;
	a++;
	nline = a->v_type == V_ERR ? default_nline : a->v_num;
	a++;
	label = a->v_type == V_ERR ? 0 : a->v_str;
	if ((haspty = vtobool(++a, 1, -1)) < 0)
		return;
	if ((hasframe = vtobool(++a, 1, -1)) < 0)
		return;
	if ((mapnl = vtobool(++a, !haspty, -1)) < 0)
		return;
	if ((keepopen = vtobool(++a, 0, -1)) < 0)
		return;
	if ((smooth = vtobool(++a, default_smooth, -1)) < 0)
		return;
	if ((++a)->v_type != V_ERR) {
		for (pp = argv; a->v_type != V_ERR &&
		     pp < &argv[sizeof argv/sizeof *argv-1]; pp++, a++)
			*pp = a->v_str;
		*pp = 0;
		shf = *(sh = argv);
		if (*sh = rindex(shf, '/'))
			(*sh)++;
		else
			*sh = shf;
	} else {
		sh = default_shell;
		shf = default_shellfile;
	}
	if ((w = openwin(id, row, col, nrow, ncol, nline, label, haspty,
	    hasframe, shf, sh)) == 0)
		return;
	w->ww_mapnl = mapnl;
	w->ww_keepopen = keepopen;
	w->ww_noupdate = !smooth;
	v->v_type = V_NUM;
	v->v_num = id + 1;
}

struct lcmd_arg arg_def_nline[] = {
	{ "nlines",	1,	ARG_NUM },
	0
};

l_def_nline(v, a)
register struct value *v, *a;
{
	v->v_num = default_nline;
	v->v_type = V_NUM;
	if (a->v_type != V_ERR)
		default_nline = a->v_num;
}

struct lcmd_arg arg_smooth[] = {
	{ "window",	1,	ARG_NUM },
	{ "flag",	1,	ARG_ANY },
	0
};

l_smooth(v, a)
register struct value *v, *a;
{
	struct ww *w;

	v->v_type = V_NUM;
	v->v_num = 0;
	if ((w = vtowin(a++, selwin)) == 0)
		return;
	v->v_num = !w->ww_noupdate;
	w->ww_noupdate = !vtobool(a, v->v_num, v->v_num);
}

struct lcmd_arg arg_def_smooth[] = {
	{ "flag",	1,	ARG_ANY },
	0
};

l_def_smooth(v, a)
register struct value *v, *a;
{
	v->v_type = V_NUM;
	v->v_num = default_smooth;
	default_smooth = vtobool(a, v->v_num, v->v_num);
}

struct lcmd_arg arg_select[] = {
	{ "window",	1,	ARG_NUM },
	0
};

l_select(v, a)
register struct value *v, *a;
{
	struct ww *w;

	v->v_type = V_NUM;
	v->v_num = selwin ? selwin->ww_id + 1 : -1;
	if (a->v_type == V_ERR)
		return;
	if ((w = vtowin(a, (struct ww *)0)) == 0)
		return;
	setselwin(w);
}

struct lcmd_arg arg_debug[] = {
	{ "flag",	1,	ARG_ANY },
	0
};

l_debug(v, a)
register struct value *v, *a;
{
	v->v_type = V_NUM;
	v->v_num = debug;
	debug = vtobool(a, debug, debug);
}

struct lcmd_arg arg_escape[] = {
	{ "escapec",	1,	ARG_STR },
	0
};

l_escape(v, a)
register struct value *v, *a;
{
	char buf[2];

	buf[0] = escapec;
	buf[1] = 0;
	if ((v->v_str = str_cpy(buf)) == 0) {
		error("Out of memory.");
		return;
	}
	v->v_type = V_STR;
	if (a->v_type != V_ERR)
		setescape(a->v_str);
}

struct lcmd_arg arg_label[] = {
	{ "window",	1,	ARG_NUM },
	{ "label",	1,	ARG_STR },
	0
};

/*ARGSUSED*/
l_label(v, a)
struct value *v;
register struct value *a;
{
	struct ww *w;

	if ((w = vtowin(a, selwin)) == 0)
		return;
	if ((++a)->v_type != V_ERR && setlabel(w, a->v_str) < 0)
		error("Out of memory.");
	reframe();
}

struct lcmd_arg arg_foreground[] = {
	{ "window",	1,	ARG_NUM },
	{ "flag",	1,	ARG_ANY },
	0
};

l_foreground(v, a)
register struct value *v, *a;
{
	struct ww *w;
	char flag;

	if ((w = vtowin(a, selwin)) == 0)
		return;
	v->v_type = V_NUM;
	v->v_num = isfg(w);
	flag = vtobool(++a, v->v_num, v->v_num);
	if (flag == v->v_num)
		return;
	deletewin(w);
	addwin(w, flag);
	reframe();
}

struct lcmd_arg arg_terse[] = {
	{ "flag",	1,	ARG_ANY },
	0
};

l_terse(v, a)
register struct value *v, *a;
{
	v->v_type = V_NUM;
	v->v_num = terse;
	setterse(vtobool(a, terse, terse));
}

struct lcmd_arg arg_source[] = {
	{ "filename",	1,	ARG_STR },
	0
};

l_source(v, a)
register struct value *v, *a;
{
	v->v_type = V_NUM;
	if (a->v_type != V_ERR && dosource(a->v_str) < 0) {
		error("Can't open %s.", a->v_str);
		v->v_num = -1;
	} else
		v->v_num = 0;
}

struct lcmd_arg arg_write[] = {
	{ "window",	1,	ARG_NUM },
	{ "",		0,	ARG_ANY|ARG_LIST },
	0
};

/*ARGSUSED*/
l_write(v, a)
struct value *v;
register struct value *a;
{
	char buf[20];
	struct ww *w;

	if ((w = vtowin(a++, selwin)) == 0)
		return;
	while (a->v_type != V_ERR) {
		if (a->v_type == V_NUM) {
			(void) sprintf(buf, "%d", a->v_num);
			(void) write(w->ww_pty, buf, strlen(buf));
		} else
			(void) write(w->ww_pty, a->v_str, strlen(a->v_str));
		if ((++a)->v_type != V_ERR)
			(void) write(w->ww_pty, " ", 1);
	}
}

struct lcmd_arg arg_close[] = {
	{ "window",	1,	ARG_ANY|ARG_LIST },
	0
};

/*ARGSUSED*/
l_close(v, a)
struct value *v;
register struct value *a;
{
	struct ww *w;

	if (a->v_type == V_STR && str_match(a->v_str, "all", 3))
		closewin((struct ww *)0);
	else
		for (; a->v_type != V_ERR; a++)
			if ((w = vtowin(a, (struct ww *)0)) != 0)
				closewin(w);
}

struct lcmd_arg arg_cursormodes[] = {
	{ "modes",	1,	ARG_NUM },
	0
};

l_cursormodes(v, a)
register struct value *v, *a;
{

	v->v_type = V_NUM;
	v->v_num = wwcursormodes;
	if (a->v_type != V_ERR)
		wwsetcursormodes(a->v_num);
}

struct lcmd_arg arg_unset[] = {
	{ "variable",	1,	ARG_ANY },
	0
};

l_unset(v, a)
register struct value *v, *a;
{
	v->v_type = V_NUM;
	switch (a->v_type) {
	case V_ERR:
		v->v_num = -1;
		return;
	case V_NUM:
		if ((a->v_str = str_itoa(a->v_num)) == 0) {
			error("Out of memory.");
			v->v_num = -1;
			return;
		}
		a->v_type = V_STR;
		break;
	}
	v->v_num = var_unset(a->v_str);
}

struct ww *
vtowin(v, w)
register struct value *v;
struct ww *w;
{
	switch (v->v_type) {
	case V_ERR:
		if (w != 0)
			return w;
		error("No window specified.");
		return 0;
	case V_STR:
		error("%s: No such window.", v->v_str);
		return 0;
	}
	if (v->v_num < 1 || v->v_num > NWINDOW
	    || (w = window[v->v_num - 1]) == 0) {
		error("%d: No such window.", v->v_num);
		return 0;
	}
	return w;
}

vtobool(v, def, err)
register struct value *v;
char def, err;
{
	switch (v->v_type) {
	case V_NUM:
		return v->v_num != 0;
	case V_STR:
		if (str_match(v->v_str, "true", 1)
		    || str_match(v->v_str, "on", 2)
		    || str_match(v->v_str, "yes", 1))
			return 1;
		else if (str_match(v->v_str, "false", 1)
		    || str_match(v->v_str, "off", 2)
		    || str_match(v->v_str, "no", 1))
			return 0;
		else {
			error("%s: Illegal boolean value.", v->v_str);
			return err;
		}
		/*NOTREACHED*/
	case V_ERR:
		return def;
	}
	/*NOTREACHED*/
}
