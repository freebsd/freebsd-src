/*-
 * Copyright (c) 1990, 1993, 1994
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

#if 0
#ifndef lint
static char sccsid[] = "@(#)print.c	8.6 (Berkeley) 4/16/94";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/stat.h>

#include <sys/mac.h>
#include <sys/user.h>
#include <sys/sysctl.h>

#include <err.h>
#include <grp.h>
#include <langinfo.h>
#include <locale.h>
#include <math.h>
#include <nlist.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "ps.h"

#define	ps_pgtok(a)	(((a) * getpagesize()) / 1024)

void
printheader(void)
{
	VAR *v;
	struct varent *vent;
	int allempty;

	allempty = 1;
	for (vent = vhead; vent; vent = vent->next)
		if (*vent->var->header != '\0') {
			allempty = 0;
			break;
		}
	if (allempty)
		return;
	for (vent = vhead; vent; vent = vent->next) {
		v = vent->var;
		if (v->flag & LJUST) {
			if (vent->next == NULL)	/* last one */
				(void)printf("%s", v->header);
			else
				(void)printf("%-*s", v->width, v->header);
		} else
			(void)printf("%*s", v->width, v->header);
		if (vent->next != NULL)
			(void)putchar(' ');
	}
	(void)putchar('\n');
}

void
arguments(KINFO *k, VARENT *ve)
{
	VAR *v;
	int left;
	char *cp, *vis_args;

	v = ve->var;

	if ((vis_args = malloc(strlen(k->ki_args) * 4 + 1)) == NULL)
		errx(1, "malloc failed");
	strvis(vis_args, k->ki_args, VIS_TAB | VIS_NL | VIS_NOSLASH);

	if (ve->next == NULL) {
		/* last field */
		if (termwidth == UNLIMITED) {
			(void)printf("%s", vis_args);
		} else {
			left = termwidth - (totwidth - v->width);
			if (left < 1) /* already wrapped, just use std width */
				left = v->width;
			for (cp = vis_args; --left >= 0 && *cp != '\0';)
				(void)putchar(*cp++);
		}
	} else {
		(void)printf("%-*.*s", v->width, v->width, vis_args);
	}
	free(vis_args);
}

void
command(KINFO *k, VARENT *ve)
{
	VAR *v;
	int left;
	char *cp, *vis_env, *vis_args;

	v = ve->var;

	if (cflag) {
		if (ve->next == NULL)	/* last field, don't pad */
			(void)printf("%s", k->ki_p->ki_comm);
		else
			(void)printf("%-*s", v->width, k->ki_p->ki_comm);
		return;
	}

	if ((vis_args = malloc(strlen(k->ki_args) * 4 + 1)) == NULL)
		errx(1, "malloc failed");
	strvis(vis_args, k->ki_args, VIS_TAB | VIS_NL | VIS_NOSLASH);
	if (k->ki_env) {
		if ((vis_env = malloc(strlen(k->ki_env) * 4 + 1)) == NULL)
			errx(1, "malloc failed");
		strvis(vis_env, k->ki_env, VIS_TAB | VIS_NL | VIS_NOSLASH);
	} else
		vis_env = NULL;

	if (ve->next == NULL) {
		/* last field */
		if (termwidth == UNLIMITED) {
			if (vis_env)
				(void)printf("%s ", vis_env);
			(void)printf("%s", vis_args);
		} else {
			left = termwidth - (totwidth - v->width);
			if (left < 1) /* already wrapped, just use std width */
				left = v->width;
			if ((cp = vis_env) != NULL) {
				while (--left >= 0 && *cp)
					(void)putchar(*cp++);
				if (--left >= 0)
					putchar(' ');
			}
			for (cp = vis_args; --left >= 0 && *cp != '\0';)
				(void)putchar(*cp++);
		}
	} else
		/* XXX env? */
		(void)printf("%-*.*s", v->width, v->width, vis_args);
	free(vis_args);
	if (vis_env != NULL)
		free(vis_env);
}

void
ucomm(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s", v->width, k->ki_p->ki_comm);
}

void
logname(KINFO *k, VARENT *ve)
{
	VAR *v;
	char *s;

	v = ve->var;
	(void)printf("%-*s", v->width, (s = k->ki_p->ki_login, *s) ? s : "-");
}

void
state(KINFO *k, VARENT *ve)
{
	int flag, sflag, tdflags;
	char *cp;
	VAR *v;
	char buf[16];

	v = ve->var;
	flag = k->ki_p->ki_flag;
	sflag = k->ki_p->ki_sflag;
	tdflags = k->ki_p->ki_tdflags;	/* XXXKSE */
	cp = buf;

	switch (k->ki_p->ki_stat) {

	case SSTOP:
		*cp = 'T';
		break;

	case SSLEEP:
		if (tdflags & TDF_SINTR)	/* interruptable (long) */
			*cp = k->ki_p->ki_slptime >= MAXSLP ? 'I' : 'S';
		else
			*cp = 'D';
		break;

	case SRUN:
	case SIDL:
		*cp = 'R';
		break;

	case SWAIT:
		*cp = 'W';
		break;

	case SLOCK:
		*cp = 'L';
		break;

	case SZOMB:
		*cp = 'Z';
		break;

	default:
		*cp = '?';
	}
	cp++;
	if (!(sflag & PS_INMEM))
		*cp++ = 'W';
	if (k->ki_p->ki_nice < NZERO)
		*cp++ = '<';
	else if (k->ki_p->ki_nice > NZERO)
		*cp++ = 'N';
	if (flag & P_TRACED)
		*cp++ = 'X';
	if (flag & P_WEXIT && k->ki_p->ki_stat != SZOMB)
		*cp++ = 'E';
	if (flag & P_PPWAIT)
		*cp++ = 'V';
	if ((flag & P_SYSTEM) || k->ki_p->ki_lock > 0)
		*cp++ = 'L';
	if (k->ki_p->ki_kiflag & KI_SLEADER)
		*cp++ = 's';
	if ((flag & P_CONTROLT) && k->ki_p->ki_pgid == k->ki_p->ki_tpgid)
		*cp++ = '+';
	if (flag & P_JAILED)
		*cp++ = 'J';
	*cp = '\0';
	(void)printf("%-*s", v->width, buf);
}

void
pri(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*d", v->width, k->ki_p->ki_pri.pri_level - PZERO);
}

void
uname(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s", v->width, user_from_uid(k->ki_p->ki_uid, 0));
}

int
s_uname(KINFO *k)
{
	    return (strlen(user_from_uid(k->ki_p->ki_uid, 0)));
}

void
rgroupname(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s", v->width, group_from_gid(k->ki_p->ki_rgid, 0));
}

int
s_rgroupname(KINFO *k)
{
	return (strlen(group_from_gid(k->ki_p->ki_rgid, 0)));
}

void
runame(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s", v->width, user_from_uid(k->ki_p->ki_ruid, 0));
}

int
s_runame(KINFO *k)
{
	    return (strlen(user_from_uid(k->ki_p->ki_ruid, 0)));
}


void
tdev(KINFO *k, VARENT *ve)
{
	VAR *v;
	dev_t dev;
	char buff[16];

	v = ve->var;
	dev = k->ki_p->ki_tdev;
	if (dev == NODEV)
		(void)printf("%*s", v->width, "??");
	else {
		(void)snprintf(buff, sizeof(buff),
		    "%d/%d", major(dev), minor(dev));
		(void)printf("%*s", v->width, buff);
	}
}

void
tname(KINFO *k, VARENT *ve)
{
	VAR *v;
	dev_t dev;
	char *ttname;

	v = ve->var;
	dev = k->ki_p->ki_tdev;
	if (dev == NODEV || (ttname = devname(dev, S_IFCHR)) == NULL)
		(void)printf("%*s ", v->width-1, "??");
	else {
		if (strncmp(ttname, "tty", 3) == 0 ||
		    strncmp(ttname, "cua", 3) == 0)
			ttname += 3;
		(void)printf("%*.*s%c", v->width-1, v->width-1, ttname,
			k->ki_p->ki_kiflag & KI_CTTY ? ' ' : '-');
	}
}

void
longtname(KINFO *k, VARENT *ve)
{
	VAR *v;
	dev_t dev;
	char *ttname;

	v = ve->var;
	dev = k->ki_p->ki_tdev;
	if (dev == NODEV || (ttname = devname(dev, S_IFCHR)) == NULL)
		(void)printf("%-*s", v->width, "??");
	else
		(void)printf("%-*s", v->width, ttname);
}

void
started(KINFO *k, VARENT *ve)
{
	VAR *v;
	time_t then;
	struct tm *tp;
	char buf[100];
	static int  use_ampm = -1;

	v = ve->var;
	if (!k->ki_valid) {
		(void)printf("%-*s", v->width, "-");
		return;
	}

	if (use_ampm < 0)
		use_ampm = (*nl_langinfo(T_FMT_AMPM) != '\0');

	then = k->ki_p->ki_start.tv_sec;
	tp = localtime(&then);
	if (now - k->ki_p->ki_start.tv_sec < 24 * 3600) {
		(void)strftime(buf, sizeof(buf) - 1,
		use_ampm ? "%l:%M%p" : "%k:%M  ", tp);
	} else if (now - k->ki_p->ki_start.tv_sec < 7 * 86400) {
		(void)strftime(buf, sizeof(buf) - 1,
		use_ampm ? "%a%I%p" : "%a%H  ", tp);
	} else
		(void)strftime(buf, sizeof(buf) - 1, "%e%b%y", tp);
	(void)printf("%-*s", v->width, buf);
}

void
lstarted(KINFO *k, VARENT *ve)
{
	VAR *v;
	time_t then;
	char buf[100];

	v = ve->var;
	if (!k->ki_valid) {
		(void)printf("%-*s", v->width, "-");
		return;
	}
	then = k->ki_p->ki_start.tv_sec;
	(void)strftime(buf, sizeof(buf) -1, "%c", localtime(&then));
	(void)printf("%-*s", v->width, buf);
}

void
lockname(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	if (k->ki_p->ki_kiflag & KI_LOCKBLOCK) {
		if (k->ki_p->ki_lockname[0] != 0)
			(void)printf("%-*.*s", v->width, v->width,
				      k->ki_p->ki_lockname);
		else
			(void)printf("%-*s", v->width, "???");
	} else
		(void)printf("%-*s", v->width, "-");
}

void
wchan(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	if (k->ki_p->ki_wchan) {
		if (k->ki_p->ki_wmesg[0] != 0)
			(void)printf("%-*.*s", v->width, v->width,
				      k->ki_p->ki_wmesg);
		else
			(void)printf("%-*lx", v->width,
			    (long)k->ki_p->ki_wchan);
	} else {
		(void)printf("%-*s", v->width, "-");
	}
}

void
mwchan(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	if (k->ki_p->ki_wchan) {
		if (k->ki_p->ki_wmesg[0] != 0)
			(void)printf("%-*.*s", v->width, v->width,
				      k->ki_p->ki_wmesg);
		else
			(void)printf("%-*lx", v->width,
			    (long)k->ki_p->ki_wchan);
	} else if (k->ki_p->ki_kiflag & KI_LOCKBLOCK) {
		if (k->ki_p->ki_lockname[0]) {
			(void)printf("%-*.*s", v->width, v->width,
			    k->ki_p->ki_lockname);
		} else {
			(void)printf("%-*s", v->width, "???");
		}
	} else {
		(void)printf("%-*s", v->width, "-");
	}
}

void
vsize(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*lu", v->width, (u_long)(k->ki_p->ki_size / 1024));
}

void
cputime(KINFO *k, VARENT *ve)
{
	VAR *v;
	long secs;
	long psecs;	/* "parts" of a second. first micro, then centi */
	char obuff[128];
	static char decimal_point = 0;

	if (!decimal_point)
		decimal_point = localeconv()->decimal_point[0];
	v = ve->var;
	if (k->ki_p->ki_stat == SZOMB || !k->ki_valid) {
		secs = 0;
		psecs = 0;
	} else {
		/*
		 * This counts time spent handling interrupts.  We could
		 * fix this, but it is not 100% trivial (and interrupt
		 * time fractions only work on the sparc anyway).	XXX
		 */
		secs = k->ki_p->ki_runtime / 1000000;
		psecs = k->ki_p->ki_runtime % 1000000;
		if (sumrusage) {
			secs += k->ki_p->ki_childtime.tv_sec;
			psecs += k->ki_p->ki_childtime.tv_usec;
		}
		/*
		 * round and scale to 100's
		 */
		psecs = (psecs + 5000) / 10000;
		secs += psecs / 100;
		psecs = psecs % 100;
	}
	(void)snprintf(obuff, sizeof(obuff),
	    "%3ld:%02ld%c%02ld", secs/60, secs%60, decimal_point, psecs);
	(void)printf("%*s", v->width, obuff);
}

void
elapsed(KINFO *k, VARENT *ve)
{
	VAR *v;
	time_t secs;
	char obuff[128];

	v = ve->var;

	secs = now - k->ki_p->ki_start.tv_sec;
	(void)snprintf(obuff, sizeof(obuff), "%3ld:%02ld", (long)secs/60,
	    (long)secs%60);
	(void)printf("%*s", v->width, obuff);
}

double
getpcpu(const KINFO *k)
{
	static int failure;

	if (!nlistread)
		failure = donlist();
	if (failure)
		return (0.0);

#define	fxtofl(fixpt)	((double)(fixpt) / fscale)

	/* XXX - I don't like this */
	if (k->ki_p->ki_swtime == 0 || (k->ki_p->ki_sflag & PS_INMEM) == 0)
		return (0.0);
	if (rawcpu)
		return (100.0 * fxtofl(k->ki_p->ki_pctcpu));
	return (100.0 * fxtofl(k->ki_p->ki_pctcpu) /
		(1.0 - exp(k->ki_p->ki_swtime * log(fxtofl(ccpu)))));
}

void
pcpu(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*.1f", v->width, getpcpu(k));
}

static double
getpmem(KINFO *k)
{
	static int failure;
	double fracmem;

	if (!nlistread)
		failure = donlist();
	if (failure)
		return (0.0);

	if ((k->ki_p->ki_sflag & PS_INMEM) == 0)
		return (0.0);
	/* XXX want pmap ptpages, segtab, etc. (per architecture) */
	/* XXX don't have info about shared */
	fracmem = ((float)k->ki_p->ki_rssize)/mempages;
	return (100.0 * fracmem);
}

void
pmem(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*.1f", v->width, getpmem(k));
}

void
pagein(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	(void)printf("%*ld", v->width,
	    k->ki_valid ? k->ki_p->ki_rusage.ru_majflt : 0);
}

/* ARGSUSED */
void
maxrss(KINFO *k __unused, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	/* XXX not yet */
	(void)printf("%*s", v->width, "-");
}

void
priorityr(KINFO *k, VARENT *ve)
{
	VAR *v;
	struct priority *lpri;
	char str[8];
	unsigned class, level;
 
	v = ve->var;
	lpri = (struct priority *) ((char *)k + v->off);
	class = lpri->pri_class;
	level = lpri->pri_level;
	switch (class) {
	case PRI_REALTIME:
		snprintf(str, sizeof(str), "real:%u", level);
		break;
	case PRI_TIMESHARE:
		strncpy(str, "normal", sizeof(str));
		break;
	case PRI_IDLE:
		snprintf(str, sizeof(str), "idle:%u", level);
		break;
	default:
		snprintf(str, sizeof(str), "%u:%u", class, level);
		break;
	}
	str[sizeof(str) - 1] = '\0';
	(void)printf("%*s", v->width, str);
}

/*
 * Generic output routines.  Print fields from various prototype
 * structures.
 */
static void
printval(void *bp, VAR *v)
{
	static char ofmt[32] = "%";
	const char *fcp;
	char *cp;

	cp = ofmt + 1;
	fcp = v->fmt;
	if (v->flag & LJUST)
		*cp++ = '-';
	*cp++ = '*';
	while ((*cp++ = *fcp++));

	switch (v->type) {
	case CHAR:
		(void)printf(ofmt, v->width, *(char *)bp);
		break;
	case UCHAR:
		(void)printf(ofmt, v->width, *(u_char *)bp);
		break;
	case SHORT:
		(void)printf(ofmt, v->width, *(short *)bp);
		break;
	case USHORT:
		(void)printf(ofmt, v->width, *(u_short *)bp);
		break;
	case INT:
		(void)printf(ofmt, v->width, *(int *)bp);
		break;
	case UINT:
		(void)printf(ofmt, v->width, *(u_int *)bp);
		break;
	case LONG:
		(void)printf(ofmt, v->width, *(long *)bp);
		break;
	case ULONG:
		(void)printf(ofmt, v->width, *(u_long *)bp);
		break;
	case KPTR:
		(void)printf(ofmt, v->width, *(u_long *)bp);
		break;
	case PGTOK:
		(void)printf(ofmt, v->width, ps_pgtok(*(u_long *)bp));
		break;
	default:
		errx(1, "unknown type %d", v->type);
	}
}

void
kvar(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	printval((char *)((char *)k->ki_p + v->off), v);
}

void
rvar(KINFO *k, VARENT *ve)
{
	VAR *v;

	v = ve->var;
	if (k->ki_valid)
		printval((char *)((char *)(&k->ki_p->ki_rusage) + v->off), v);
	else
		(void)printf("%*s", v->width, "-");
}

void
label(KINFO *k, VARENT *ve)
{
	char *string;
	mac_t proclabel;
	int error;
	VAR *v;

	v = ve->var;
	string = NULL;

	if (mac_prepare_process_label(&proclabel) == -1) {
		perror("mac_prepare_process_label");
		goto out;
	}

	error = mac_get_pid(k->ki_p->ki_pid, proclabel);
	if (error == 0) {
		if (mac_to_text(proclabel, &string) == -1)
			string = NULL;
	}
	mac_free(proclabel);

out:
	if (string != NULL) {
		(void)printf("%-*s", v->width, string);
		free(string);
	} else
		(void)printf("%-*s", v->width, "");
	return;
}

int
s_label(KINFO *k)
{
	char *string = NULL;
	mac_t proclabel;
	int error, size = 0;

	if (mac_prepare_process_label(&proclabel) == -1) {
		perror("mac_prepare_process_label");
		return (0);
	}
	error = mac_get_pid(k->ki_p->ki_pid, proclabel);
	if (error == 0 && mac_to_text(proclabel, &string) == 0) {
		size = strlen(string);
		free(string);
	}
	mac_free(proclabel);
	return (size);
}
