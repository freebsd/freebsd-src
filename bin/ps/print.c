/*-
 * Copyright (c) 1990 The Regents of the University of California.
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

#ifndef lint
static char sccsid[] = "@(#)print.c	5.9 (Berkeley) 7/1/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <math.h>
#include <tzfile.h>
#include <stddef.h>
#include <string.h>
#include "ps.h"

#ifdef SPPWAIT
#define NEWVM
#endif

#ifdef NEWVM
#include <vm/vm.h>
#include <sys/ucred.h>
#include <sys/kinfo_proc.h>
#else
#include <machine/pte.h>
#include <sys/vmparam.h>
#include <sys/vm.h>
#endif

printheader()
{
	register VAR *v;
	register struct varent *vent;

	for (vent = vhead; vent; vent = vent->next) {
		v = vent->var;
		if (v->flag & LJUST) {
			if (vent->next == NULL)	/* last one */
				(void) printf("%s", v->header);
			else
				(void) printf("%-*s", v->width, v->header);
		} else
			(void) printf("%*s", v->width, v->header);
		if (vent->next != NULL)
			(void) putchar(' ');
	}
	(void) putchar('\n');
}

command(k, v, next)
	KINFO *k;
	VAR *v;
{
	extern int termwidth, totwidth;

	if (next == NULL) {
		/* last field */
		if (termwidth == UNLIMITED)
			(void) printf("%s", k->ki_args);
		else {
			register int left = termwidth - (totwidth - v->width);
			register char *cp = k->ki_args;

			if (left < 1) /* already wrapped, just use std width */
				left = v->width;
			while (--left >= 0 && *cp)
				(void) putchar(*cp++);
		}
	} else
		(void) printf("%-*.*s", v->width, v->width, k->ki_args);

}

ucomm(k, v)
	KINFO *k;
	VAR *v;
{
	(void) printf("%-*s", v->width, k->ki_p->p_comm);
}

logname(k, v)
	KINFO *k;
	VAR *v;
{
#ifndef NEWVM
	(void) printf("%-*s", v->width, k->ki_p->p_logname);
#else /* NEWVM */
	(void) printf("%-*s", v->width, k->ki_e->e_login);
#endif /* NEWVM */
}

state(k, v)
	KINFO *k;
	VAR *v;
{
	char buf[16];
	register char *cp = buf;
	register struct proc *p = k->ki_p;
	register flag = p->p_flag;

	switch (p->p_stat) {

	case SSTOP:
		*cp = 'T';
		break;

	case SSLEEP:
		if (flag & SSINTR)	/* interuptable (long) */
			*cp = p->p_slptime >= MAXSLP ? 'I' : 'S';
		else
			*cp = /* (flag & SPAGE) ? 'P' : */ 'D';
		break;

	case SRUN:
	case SIDL:
		*cp = 'R';
		break;

	case SZOMB:
		*cp = 'Z';
		break;

	default:
		*cp = '?';
	}
	cp++;
	if (flag & SLOAD) {
#ifndef NEWVM
		if (p->p_rssize > p->p_maxrss)
			*cp++ = '>';
#endif
	} else
		*cp++ = 'W';
	if (p->p_nice < NZERO)
		*cp++ = '<';
	else if (p->p_nice > NZERO)
		*cp++ = 'N';
#ifndef NEWVM
	if (flag & SUANOM)
		*cp++ = 'A';
	else if (flag & SSEQL)
		*cp++ = 'S';
#endif
	if (flag & STRC)
		*cp++ = 'X';
	if (flag & SWEXIT && p->p_stat != SZOMB)
		*cp++ = 'E';
#ifdef NEWVM
	if (flag & SPPWAIT)
#else
	if (flag & SVFORK)
#endif
		*cp++ = 'V';
#ifdef NEWVM
	if (flag & (SSYS|SLOCK|SKEEP|SPHYSIO))
#else
	if (flag & (SSYS|SLOCK|SULOCK|SKEEP|SPHYSIO))
#endif
		*cp++ = 'L';
	if (flag & SUGID)
		*cp++ = 'U';
	if (k->ki_e->e_flag & EPROC_SLEADER)
		*cp++ = 's';
	if ((flag & SCTTY) && k->ki_e->e_pgid == k->ki_e->e_tpgid)
		*cp++ = '+';
	*cp = '\0';
	(void) printf("%-*s", v->width, buf);
}

pri(k, v)
	KINFO *k;
	VAR *v;
{
	(void) printf("%*d", v->width, k->ki_p->p_pri - PZERO);
}

uname(k, v)
	KINFO *k;
	VAR *v;
{
#ifndef NEWVM
	(void) printf("%-*s", v->width, user_from_uid(k->ki_p->p_uid, 0));
#else /* NEWVM */
	(void) printf("%-*s", v->width,
		user_from_uid(k->ki_e->e_ucred.cr_uid, 0));
#endif /* NEWVM */
}

runame(k, v)
	KINFO *k;
	VAR *v;
{
#ifndef NEWVM
	(void) printf("%-*s", v->width, user_from_uid(k->ki_p->p_ruid, 0));
#else /* NEWVM */
	(void) printf("%-*s", v->width,
		user_from_uid(k->ki_e->e_pcred.p_ruid, 0));
#endif /* NEWVM */
}

tdev(k, v)
	KINFO *k;
	VAR *v;
{
	dev_t dev = k->ki_e->e_tdev;

	if (dev == NODEV)
		(void) printf("%*s", v->width, "??");
	else {
		char buff[16];

		(void) sprintf(buff, "%d/%d", major(dev), minor(dev));
		(void) printf("%*s", v->width, buff);
	}
}

tname(k, v)
	KINFO *k;
	VAR *v;
{
	dev_t dev;
	char *ttname, *devname();

	dev = k->ki_e->e_tdev;
	if (dev == NODEV || (ttname = devname(dev, S_IFCHR)) == NULL)
		(void) printf("%-*s", v->width, "??");
	else {
		if (strncmp(ttname, "tty", 3) == 0 ||
		    strncmp(ttname, "cua", 3) == 0)
			ttname += 3;
		(void) printf("%*.*s%c", v->width-1, v->width-1, ttname,
			k->ki_e->e_flag & EPROC_CTTY ? ' ' : '-');
	}
}

longtname(k, v)
	KINFO *k;
	VAR *v;
{
	dev_t dev;
	char *ttname, *devname();

	dev = k->ki_e->e_tdev;
	if (dev == NODEV || (ttname = devname(dev, S_IFCHR)) == NULL)
		(void) printf("%-*s", v->width, "??");
	else
		(void) printf("%-*s", v->width, ttname);
}

started(k, v)
	KINFO *k;
	VAR *v;
{
	static time_t now;
	struct tm *tp;
	char buf[100];

	if (!k->ki_u) {
		(void) printf("%-*s", v->width, "-");
		return;
	}

	tp = localtime(&k->ki_u->u_start.tv_sec);
	if (!now)
		(void)time(&now);
	if (now - k->ki_u->u_start.tv_sec < 24 * SECSPERHOUR) {
		static char fmt[] = "%l:@M%p";
		fmt[3] = '%';			/* I *hate* SCCS... */
		(void) strftime(buf, sizeof(buf) - 1, fmt, tp);
	} else if (now - k->ki_u->u_start.tv_sec < 7 * SECSPERDAY) {
		static char fmt[] = "%a@I%p";
		fmt[2] = '%';			/* I *hate* SCCS... */
		(void) strftime(buf, sizeof(buf) - 1, fmt, tp);
	} else
		(void) strftime(buf, sizeof(buf) - 1, "%e%b%y", tp);
	(void) printf("%-*s", v->width, buf);
}

lstarted(k, v)
	KINFO *k;
	VAR *v;
{
	char buf[100];

	if (!k->ki_u) {
		(void) printf("%-*s", v->width, "-");
		return;
	}
	(void) strftime(buf, sizeof(buf) -1, "%C",
	    localtime(&k->ki_u->u_start.tv_sec));
	(void) printf("%-*s", v->width, buf);
}

wchan(k, v)
	KINFO *k;
	VAR *v;
{
	if (k->ki_p->p_wchan) {
		if (k->ki_p->p_wmesg)
			(void) printf("%-*.*s", v->width, v->width, k->ki_e->e_wmesg);
		else
			(void) printf("%-*x", v->width,
			    (int)k->ki_p->p_wchan &~ KERNBASE);
	} else
		(void) printf("%-*s", v->width, "-");
}

#define pgtok(a)        (((a)*NBPG)/1024)

vsize(k, v)
	KINFO *k;
	VAR *v;
{
	(void) printf("%*d", v->width,
#ifndef NEWVM
	    pgtok(k->ki_p->p_dsize + k->ki_p->p_ssize + k->ki_e->e_xsize));
#else /* NEWVM */
	    pgtok(k->ki_e->e_vm.vm_dsize + k->ki_e->e_vm.vm_ssize +
		k->ki_e->e_vm.vm_tsize));
#endif /* NEWVM */
}

rssize(k, v)
	KINFO *k;
	VAR *v;
{
#ifndef NEWVM
	(void) printf("%*d", v->width,
	    pgtok(k->ki_p->p_rssize + (k->ki_e->e_xccount ?
	    (k->ki_e->e_xrssize / k->ki_e->e_xccount) : 0)));
#else /* NEWVM */
	/* XXX don't have info about shared */
	(void) printf("%*d", v->width, pgtok(k->ki_e->e_vm.vm_rssize));
#endif /* NEWVM */
}

p_rssize(k, v)		/* doesn't account for text */
	KINFO *k;
	VAR *v;
{
#ifndef NEWVM
	(void) printf("%*d", v->width, pgtok(k->ki_p->p_rssize));
#else /* NEWVM */
	(void) printf("%*d", v->width, pgtok(k->ki_e->e_vm.vm_rssize));
#endif /* NEWVM */
}

cputime(k, v)
	KINFO *k;
	VAR *v;
{
	extern int sumrusage;
	long secs;
	long psecs;	/* "parts" of a second. first micro, then centi */
	char obuff[128];

	if (k->ki_p->p_stat == SZOMB || k->ki_u == NULL) {
		secs = 0;
		psecs = 0;
	} else {
		secs = k->ki_p->p_utime.tv_sec +
			k->ki_p->p_stime.tv_sec;
		psecs = k->ki_p->p_utime.tv_usec +
			k->ki_p->p_stime.tv_usec;
		if (sumrusage) {
			secs += k->ki_u->u_cru.ru_utime.tv_sec +
				k->ki_u->u_cru.ru_stime.tv_sec;
			psecs += k->ki_u->u_cru.ru_utime.tv_usec +
				k->ki_u->u_cru.ru_stime.tv_usec;
		}
		/*
		 * round and scale to 100's
		 */
		psecs = (psecs + 5000) / 10000;
		secs += psecs / 100;
		psecs = psecs % 100;
	}
	(void) sprintf(obuff, "%3ld:%02ld.%02ld", secs/60, secs%60, psecs);
	(void) printf("%*s", v->width, obuff);
}

double
getpcpu(k)
	KINFO *k;
{
	extern fixpt_t ccpu;
	extern int fscale, nlistread, rawcpu;
	struct proc *p;
	static int failure;

	if (!nlistread)
		failure = donlist();
	if (failure)
		return (0.0);

	p = k->ki_p;
#define	fxtofl(fixpt)	((double)(fixpt) / fscale)

	/* XXX - I don't like this */
	if (p->p_time == 0 || (p->p_flag & SLOAD) == 0)
		return (0.0);
	if (rawcpu)
		return (100.0 * fxtofl(p->p_pctcpu));
	return (100.0 * fxtofl(p->p_pctcpu) /
		(1.0 - exp(p->p_time * log(fxtofl(ccpu)))));
}

pcpu(k, v)
	KINFO *k;
	VAR *v;
{
	(void) printf("%*.1f", v->width, getpcpu(k));
}

double
getpmem(k)
	KINFO *k;
{
	extern int mempages, nlistread;
	static int failure;
	struct proc *p;
	struct eproc *e;
	double fracmem;
	int szptudot;

	if (!nlistread)
		failure = donlist();
	if (failure)
		return (0.0);

	p = k->ki_p;
	e = k->ki_e;
	if ((p->p_flag & SLOAD) == 0)
		return (0.0);
#ifndef NEWVM
	szptudot = UPAGES + clrnd(ctopt(p->p_dsize + p->p_ssize + e->e_xsize));
	fracmem = ((float)p->p_rssize + szptudot)/CLSIZE/mempages;
	if (p->p_textp && e->e_xccount)
		fracmem += ((float)e->e_xrssize)/CLSIZE/e->e_xccount/mempages;
#else /* NEWVM */
	/* XXX want pmap ptpages, segtab, etc. (per architecture) */
	szptudot = UPAGES;
	/* XXX don't have info about shared */
	fracmem = ((float)e->e_vm.vm_rssize + szptudot)/CLSIZE/mempages;
#endif /* NEWVM */
	return (100.0 * fracmem);
}

pmem(k, v)
	KINFO *k;
	VAR *v;
{
	(void) printf("%*.1f", v->width, getpmem(k));
}

pagein(k, v)
	KINFO *k;
	VAR *v;
{
	(void) printf("%*d", v->width, k->ki_u ? k->ki_u->u_ru.ru_majflt : 0);
}

maxrss(k, v)
	KINFO *k;
	VAR *v;
{
#ifndef NEWVM	/* not yet */
	if (k->ki_p->p_maxrss != (RLIM_INFINITY/NBPG))
		(void) printf("%*d", v->width, pgtok(k->ki_p->p_maxrss));
	else
#endif /* NEWVM */
		(void) printf("%*s", v->width, "-");
}

tsize(k, v)
	KINFO *k;
	VAR *v;
{
#ifndef NEWVM
	(void) printf("%*d", v->width, pgtok(k->ki_e->e_xsize));
#else /* NEWVM */
	(void) printf("%*d", v->width, pgtok(k->ki_e->e_vm.vm_tsize));
#endif /* NEWVM */
}

#ifndef NEWVM
trss(k, v)
	KINFO *k;
	VAR *v;
{
	(void) printf("%*d", v->width, pgtok(k->ki_e->e_xrssize));
}
#endif /* NEWVM */

/*
 * Generic output routines.  Print fields from various prototype
 * structures.
 */
pvar(k, v)
	KINFO *k;
	VAR *v;
{
	printval((char *)((char *)k->ki_p + v->off), v);
}

evar(k, v)
	KINFO *k;
	VAR *v;
{
	printval((char *)((char *)k->ki_e + v->off), v);
}

uvar(k, v)
	KINFO *k;
	VAR *v;
{
	if (k->ki_u)
		printval((char *)((char *)k->ki_u + v->off), v);
	else
		(void) printf("%*s", v->width, "-");
}

rvar(k, v)
	KINFO *k;
	VAR *v;
{
	if (k->ki_u)
		printval((char *)((char *)(&k->ki_u->u_ru) + v->off), v);
	else
		(void) printf("%*s", v->width, "-");
}

printval(bp, v)
	char *bp;
	VAR *v;
{
	static char ofmt[32] = "%";
	register char *cp = ofmt+1, *fcp = v->fmt;

	if (v->flag & LJUST)
		*cp++ = '-';
	*cp++ = '*';
	while (*cp++ = *fcp++);

	switch (v->type) {
	case CHAR:
		(void) printf(ofmt, v->width, *(char *)bp);
		break;
	case UCHAR:
		(void) printf(ofmt, v->width, *(u_char *)bp);
		break;
	case SHORT:
		(void) printf(ofmt, v->width, *(short *)bp);
		break;
	case USHORT:
		(void) printf(ofmt, v->width, *(u_short *)bp);
		break;
	case LONG:
		(void) printf(ofmt, v->width, *(long *)bp);
		break;
	case ULONG:
		(void) printf(ofmt, v->width, *(u_long *)bp);
		break;
	case KPTR:
		(void) printf(ofmt, v->width, *(u_long *)bp &~ KERNBASE);
		break;
	default:
		err("unknown type %d", v->type);
	}
}
