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
static char sccsid[] = "@(#)lcmd2.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include "defs.h"
#include "string.h"
#include "value.h"
#include "var.h"
#include "lcmd.h"
#include "alias.h"
#include <sys/types.h>
#include <sys/resource.h>

/*ARGSUSED*/
l_iostat(v, a)
struct value *v, *a;
{
	register struct ww *w;

	if ((w = openiwin(16, "IO Statistics")) == 0) {
		error("Can't open statistics window: %s.", wwerror());
		return;
	}
	wwprintf(w, "ttflush\twrite\terror\tzero\tchar\n");
	wwprintf(w, "%d\t%d\t%d\t%d\t%d\n",
		wwnflush, wwnwr, wwnwre, wwnwrz, wwnwrc);
	wwprintf(w, "token\tuse\tbad\tsaving\ttotal\tbaud\n");
	wwprintf(w, "%d\t%d\t%d\t%d\t%d\t%d/%d (%.1f/%.1f)\n",
		wwntokdef, wwntokuse, wwntokbad, wwntoksave, wwntokc,
		wwntokc - wwntoksave ?
			(int) ((float) wwbaud * wwntokc /
					(wwntokc - wwntoksave)) :
			wwbaud,
		wwnwrc ? (int) ((float) wwbaud * (wwnwrc + wwntoksave) /
					wwnwrc) :
			wwbaud,
		wwntokc - wwntoksave ?
			(float) wwntokc / (wwntokc - wwntoksave) : 1.0,
		wwnwrc ? (float) (wwnwrc + wwntoksave) / wwnwrc : 1.0);
	wwprintf(w, "wwwrite\tattempt\tchar\n");
	wwprintf(w, "%d\t%d\t%d\n",
		wwnwwr, wwnwwra, wwnwwrc);
	wwprintf(w, "wwupdat\tline\tmiss\tscan\tclreol\tclreos\tmiss\tline\n");
	wwprintf(w, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
		wwnupdate, wwnupdline, wwnupdmiss, wwnupdscan, wwnupdclreol,
		wwnupdclreos, wwnupdclreosmiss, wwnupdclreosline);
	wwprintf(w, "select\terror\tzero\n");
	wwprintf(w, "%d\t%d\t%d\n",
		wwnselect, wwnselecte, wwnselectz);
	wwprintf(w, "read\terror\tzero\tchar\tack\tnack\tstat\terrorc\n");
	wwprintf(w, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
		wwnread, wwnreade, wwnreadz, wwnreadc, wwnreadack, wwnreadnack,
		wwnreadstat, wwnreadec);
	wwprintf(w, "ptyread\terror\tzero\tcontrol\tdata\tchar\n");
	wwprintf(w, "%d\t%d\t%d\t%d\t%d\t%d\n",
		wwnwread, wwnwreade, wwnwreadz,
		wwnwreadp, wwnwreadd, wwnwreadc);
	waitnl(w);
	closeiwin(w);
}

struct lcmd_arg arg_time[] = {
	{ "who",	1,	ARG_STR },
	0
};

/*ARGSUSED*/
l_time(v, a)
struct value *v;
register struct value *a;
{
	register struct ww *w;
	struct rusage rusage;
	struct timeval timeval;
	char *strtime();

	if ((w = openiwin(8, "Timing and Resource Usage")) == 0) {
		error("Can't open time window: %s.", wwerror());
		return;
	}

	(void) gettimeofday(&timeval, (struct timezone *)0);
	timeval.tv_sec -= starttime.tv_sec;
	if ((timeval.tv_usec -= starttime.tv_usec) < 0) {
		timeval.tv_sec--;
		timeval.tv_usec += 1000000;
	}
	(void) getrusage(a->v_type == V_STR
			&& str_match(a->v_str, "children", 1)
		? RUSAGE_CHILDREN : RUSAGE_SELF, &rusage);

	wwprintf(w, "%-15s %-15s %-15s\n",
		"time", "utime", "stime");
	wwprintf(w, "%-15s ", strtime(&timeval));
	wwprintf(w, "%-15s ", strtime(&rusage.ru_utime));
	wwprintf(w, "%-15s\n", strtime(&rusage.ru_stime));
	wwprintf(w, "%-15s %-15s %-15s %-15s\n",
		"maxrss", "ixrss", "idrss", "isrss");
	wwprintf(w, "%-15ld %-15ld %-15ld %-15ld\n",
		rusage.ru_maxrss, rusage.ru_ixrss,
		rusage.ru_idrss, rusage.ru_isrss);
	wwprintf(w, "%-7s %-7s %-7s %-7s %-7s %-7s %-7s %-7s %-7s %-7s\n",
		"minflt", "majflt", "nswap", "inblk", "oublk",
		"msgsnd", "msgrcv", "nsigs", "nvcsw", "nivcsw");
	wwprintf(w, "%-7ld %-7ld %-7ld %-7ld %-7ld %-7ld %-7ld %-7ld %-7ld %-7ld\n",
		rusage.ru_minflt, rusage.ru_majflt, rusage.ru_nswap,
		rusage.ru_inblock, rusage.ru_oublock,
		rusage.ru_msgsnd, rusage.ru_msgrcv, rusage.ru_nsignals,
		rusage.ru_nvcsw, rusage.ru_nivcsw);

	waitnl(w);
	closeiwin(w);
}

char *
strtime(t)
register struct timeval *t;
{
	char fill = 0;
	static char buf[20];
	register char *p = buf;

	if (t->tv_sec > 60*60) {
		(void) sprintf(p, "%ld:", t->tv_sec / (60*60));
		while (*p++)
			;
		p--;
		t->tv_sec %= 60*60;
		fill++;
	}
	if (t->tv_sec > 60) {
		(void) sprintf(p, fill ? "%02ld:" : "%ld:", t->tv_sec / 60);
		while (*p++)
			;
		p--;
		t->tv_sec %= 60;
		fill++;
	}
	(void) sprintf(p, fill ? "%02ld.%02d" : "%ld.%02ld",
		t->tv_sec, t->tv_usec / 10000);
	return buf;
}

/*ARGSUSED*/
l_list(v, a)
struct value *v, *a;
{
	register struct ww *w, *wp;
	register i;
	int n;

	for (n = 0, i = 0; i < NWINDOW; i++)
		if (window[i] != 0)
			n++;
	if (n == 0) {
		error("No windows.");
		return;
	}
	if ((w = openiwin(n + 2, "Windows")) == 0) {
		error("Can't open listing window: %s.", wwerror());
		return;
	}
	for (i = 0; i < NWINDOW; i++) {
		if ((wp = window[i]) == 0)
			continue;
		wwprintf(w, "%c %c %-13s %-.*s\n",
			wp == selwin ? '+' : (wp == lastselwin ? '-' : ' '),
			i + '1',
			wp->ww_state == WWS_HASPROC ? "" : "(No process)",
			wwncol - 20,
			wp->ww_label ? wp->ww_label : "(No label)");
	}
	waitnl(w);
	closeiwin(w);
}

/*ARGSUSED*/
l_variable(v, a)
struct value *v, *a;
{
	register struct ww *w;
	int printvar();

	if ((w = openiwin(wwnrow - 3, "Variables")) == 0) {
		error("Can't open variable window: %s.", wwerror());
		return;
	}
	if (var_walk(printvar, (int)w) >= 0)
		waitnl(w);
	closeiwin(w);
}

printvar(w, r)
register struct ww *w;
register struct var *r;
{
	if (more(w, 0) == 2)
		return -1;
	wwprintf(w, "%16s    ", r->r_name);
	switch (r->r_val.v_type) {
	case V_STR:
		wwprintf(w, "%s\n", r->r_val.v_str);
		break;
	case V_NUM:
		wwprintf(w, "%d\n", r->r_val.v_num);
		break;
	case V_ERR:
		wwprintf(w, "ERROR\n");
		break;
	}
	return 0;
}

struct lcmd_arg arg_def_shell[] = {
	{ "",	0,		ARG_ANY|ARG_LIST },
	0
};

l_def_shell(v, a)
	struct value *v, *a;
{
	register char **pp;
	register struct value *vp;

	if (a->v_type == V_ERR) {
		if ((v->v_str = str_cpy(default_shellfile)) != 0)
			v->v_type = V_STR;
		return;
	}
	if (v->v_str = default_shellfile) {
		v->v_type = V_STR;
		for (pp = default_shell + 1; *pp; pp++) {
			str_free(*pp);
			*pp = 0;
		}
	}
	for (pp = default_shell, vp = a;
	     vp->v_type != V_ERR &&
	     pp < &default_shell[sizeof default_shell/sizeof *default_shell-1];
	     pp++, vp++)
		if ((*pp = vp->v_type == V_STR ?
		     str_cpy(vp->v_str) : str_itoa(vp->v_num)) == 0) {
			/* just leave default_shell[] the way it is */
			p_memerror();
			break;
		}
	if (default_shellfile = *default_shell)
		if (*default_shell = rindex(default_shellfile, '/'))
			(*default_shell)++;
		else
			*default_shell = default_shellfile;
}

struct lcmd_arg arg_alias[] = {
	{ "",	0,		ARG_STR },
	{ "",	0,		ARG_STR|ARG_LIST },
	0
};

l_alias(v, a)
	struct value *v, *a;
{
	if (a->v_type == V_ERR) {
		register struct ww *w;
		int printalias();

		if ((w = openiwin(wwnrow - 3, "Aliases")) == 0) {
			error("Can't open alias window: %s.", wwerror());
			return;
		}
		if (alias_walk(printalias, (int)w) >= 0)
			waitnl(w);
		closeiwin(w);
	} else {
		register struct alias *ap = 0;

		if (ap = alias_lookup(a->v_str)) {
			if ((v->v_str = str_cpy(ap->a_buf)) == 0) {
				p_memerror();
				return;
			}
			v->v_type = V_STR;
		}
		if (a[1].v_type == V_STR) {
			register struct value *vp;
			register char *p, *q;
			char *str;
			register n;

			for (n = 0, vp = a + 1; vp->v_type != V_ERR; vp++, n++)
				for (p = vp->v_str; *p; p++, n++)
					;
			if ((str = str_alloc(n)) == 0) {
				p_memerror();
				return;
			}
			for (q = str, vp = a + 1; vp->v_type != V_ERR;
			     vp++, q[-1] = ' ')
				for (p = vp->v_str; *q++ = *p++;)
					;
			q[-1] = 0;
			if ((ap = alias_set(a[0].v_str, (char *)0)) == 0) {
				p_memerror();
				str_free(str);
				return;
			}
			ap->a_buf = str;
		}
	}
}

printalias(w, a)
register struct ww *w;
register struct alias *a;
{
	if (more(w, 0) == 2)
		return -1;
	wwprintf(w, "%16s    %s\n", a->a_name, a->a_buf);
	return 0;
}

struct lcmd_arg arg_unalias[] = {
	{ "alias",	1,	ARG_STR },
	0
};

l_unalias(v, a)
struct value *v, *a;
{
	if (a->v_type == ARG_STR)
		v->v_num = alias_unset(a->v_str);
	v->v_type = V_NUM;
}

struct lcmd_arg arg_echo[] = {
	{ "window",	1,	ARG_NUM },
	{ "",		0,	ARG_ANY|ARG_LIST },
	0
};

/*ARGSUSED*/
l_echo(v, a)
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
			(void) wwwrite(w, buf, strlen(buf));
		} else
			(void) wwwrite(w, a->v_str, strlen(a->v_str));
		if ((++a)->v_type != V_ERR)
			(void) wwwrite(w, " ", 1);
	}
	(void) wwwrite(w, "\r\n", 2);
}
