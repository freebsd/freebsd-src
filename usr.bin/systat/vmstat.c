/*-
 * Copyright (c) 1983, 1989, 1992, 1993
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
#if 0
static char sccsid[] = "@(#)vmstat.c	8.2 (Berkeley) 1/12/94";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * Cursed vmstat -- from Robert Elz.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/sysctl.h>
#include <sys/dkstat.h>
#include <sys/vmmeter.h>

#include <vm/vm_param.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <langinfo.h>
#include <nlist.h>
#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#include <devstat.h>
#include "systat.h"
#include "extern.h"
#include "devs.h"

static struct Info {
	long	time[CPUSTATES];
	u_int v_swtch;		/* context switches */
	u_int v_trap;		/* calls to trap */
	u_int v_syscall;	/* calls to syscall() */
	u_int v_intr;		/* device interrupts */
	u_int v_soft;		/* software interrupts */
	/*
	 * Virtual memory activity.
	 */
	u_int v_vm_faults;	/* number of address memory faults */
	u_int v_cow_faults;	/* number of copy-on-writes */
	u_int v_zfod;		/* pages zero filled on demand */
	u_int v_ozfod;		/* optimized zero fill pages */
	u_int v_swapin;		/* swap pager pageins */
	u_int v_swapout;	/* swap pager pageouts */
	u_int v_swappgsin;	/* swap pager pages paged in */
	u_int v_swappgsout;	/* swap pager pages paged out */
	u_int v_vnodein;	/* vnode pager pageins */
	u_int v_vnodeout;	/* vnode pager pageouts */
	u_int v_vnodepgsin;	/* vnode_pager pages paged in */
	u_int v_vnodepgsout;	/* vnode pager pages paged out */
	u_int v_intrans;	/* intransit blocking page faults */
	u_int v_reactivated;	/* number of pages reactivated from free list */
	u_int v_pdwakeups;	/* number of times daemon has awaken from sleep */
	u_int v_pdpages;	/* number of pages analyzed by daemon */

	u_int v_dfree;		/* pages freed by daemon */
	u_int v_pfree;		/* pages freed by exiting processes */
	u_int v_tfree;		/* total pages freed */
	/*
	 * Distribution of page usages.
	 */
	u_int v_page_size;	/* page size in bytes */
	u_int v_free_count;	/* number of pages free */
	u_int v_wire_count;	/* number of pages wired down */
	u_int v_active_count;	/* number of pages active */
	u_int v_inactive_count;	/* number of pages inactive */
	u_int v_cache_count;	/* number of pages on buffer cache queue */
	struct	vmtotal Total;
	struct	nchstats nchstats;
	long	nchcount;
	long	*intrcnt;
	int	bufspace;
	int	desiredvnodes;
	long	numvnodes;
	long	freevnodes;
	int	numdirtybuffers;
} s, s1, s2, z;

struct statinfo cur, last, run;

#define	total s.Total
#define	nchtotal s.nchstats
#define	oldnchtotal s1.nchstats

static	enum state { BOOT, TIME, RUN } state = TIME;

static void allocinfo __P((struct Info *));
static void copyinfo __P((struct Info *, struct Info *));
static float cputime __P((int));
static void dinfo __P((int, int, struct statinfo *, struct statinfo *));
static void getinfo __P((struct Info *, enum state));
static void putint __P((int, int, int, int));
static void putfloat __P((double, int, int, int, int, int));
static void putlongdouble __P((long double, int, int, int, int, int));
static int ucount __P((void));

static	int ncpu;
static	int ut;
static	char buf[26];
static	time_t t;
static	double etime;
static	int nintr;
static	long *intrloc;
static	char **intrname;
static	int nextintsrow;
static  int extended_vm_stats;

struct	utmp utmp;


WINDOW *
openkre()
{

	ut = open(_PATH_UTMP, O_RDONLY);
	if (ut < 0)
		error("No utmp");
	return (stdscr);
}

void
closekre(w)
	WINDOW *w;
{

	(void) close(ut);
	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
}

/*
 * These constants define where the major pieces are laid out
 */
#define STATROW		 0	/* uses 1 row and 68 cols */
#define STATCOL		 2
#define MEMROW		 2	/* uses 4 rows and 31 cols */
#define MEMCOL		 0
#define PAGEROW		 2	/* uses 4 rows and 26 cols */
#define PAGECOL		46
#define INTSROW		 6	/* uses all rows to bottom and 17 cols */
#define INTSCOL		61
#define PROCSROW	 7	/* uses 2 rows and 20 cols */
#define PROCSCOL	 0
#define GENSTATROW	 7	/* uses 2 rows and 30 cols */
#define GENSTATCOL	20
#define VMSTATROW	 6	/* uses 17 rows and 12 cols */
#define VMSTATCOL	48
#define GRAPHROW	10	/* uses 3 rows and 51 cols */
#define GRAPHCOL	 0
#define NAMEIROW	14	/* uses 3 rows and 38 cols */
#define NAMEICOL	 0
#define DISKROW		18	/* uses 5 rows and 50 cols (for 9 drives) */
#define DISKCOL		 0

#define	DRIVESPACE	 7	/* max # for space */

#define	MAXDRIVES	DRIVESPACE	 /* max # to display */

int
initkre()
{
	char *intrnamebuf, *cp;
	int i;

	if ((num_devices = getnumdevs()) < 0) {
		warnx("%s", devstat_errbuf);
		return(0);
	}

	cur.dinfo = (struct devinfo *)malloc(sizeof(struct devinfo));
	last.dinfo = (struct devinfo *)malloc(sizeof(struct devinfo));
	run.dinfo = (struct devinfo *)malloc(sizeof(struct devinfo));
	bzero(cur.dinfo, sizeof(struct devinfo));
	bzero(last.dinfo, sizeof(struct devinfo));
	bzero(run.dinfo, sizeof(struct devinfo));

	if (dsinit(MAXDRIVES, &cur, &last, &run) != 1)
		return(0);

	if (nintr == 0) {
		GETSYSCTL("hw.nintr", nintr);
		intrloc = calloc(nintr, sizeof (long));
		intrname = calloc(nintr, sizeof (long));
		intrnamebuf = sysctl_dynread("hw.intrnames", NULL);
		if (intrnamebuf == NULL || intrname == NULL || 
		    intrloc == NULL) {
			error("Out of memory");
			if (intrnamebuf)
				free(intrnamebuf);
			if (intrname)
				free(intrname);
			if (intrloc)
				free(intrloc);
			nintr = 0;
			return(0);
		}
		for (cp = intrnamebuf, i = 0; i < nintr; i++) {
			intrname[i] = cp;
			cp += strlen(cp) + 1;
		}
		nextintsrow = INTSROW + 2;
		allocinfo(&s);
		allocinfo(&s1);
		allocinfo(&s2);
		allocinfo(&z);
	}
	getinfo(&s2, RUN);
	copyinfo(&s2, &s1);
	return(1);
}

void
fetchkre()
{
	time_t now;
	struct tm *tp;
	static int d_first = -1;

	if (d_first < 0)
		d_first = (*nl_langinfo(D_MD_ORDER) == 'd');

	time(&now);
	tp = localtime(&now);
	(void) strftime(buf, sizeof(buf),
			d_first ? "%e %b %R" : "%b %e %R", tp);
	getinfo(&s, state);
}

void
labelkre()
{
	register int i, j;

	clear();
	mvprintw(STATROW, STATCOL + 4, "users    Load");
	mvprintw(MEMROW, MEMCOL, "Mem:KB    REAL            VIRTUAL");
	mvprintw(MEMROW + 1, MEMCOL, "        Tot   Share      Tot    Share");
	mvprintw(MEMROW + 2, MEMCOL, "Act");
	mvprintw(MEMROW + 3, MEMCOL, "All");

	mvprintw(MEMROW + 1, MEMCOL + 41, "Free");

	mvprintw(PAGEROW, PAGECOL,     "        VN PAGER  SWAP PAGER ");
	mvprintw(PAGEROW + 1, PAGECOL, "        in  out     in  out ");
	mvprintw(PAGEROW + 2, PAGECOL, "count");
	mvprintw(PAGEROW + 3, PAGECOL, "pages");

	mvprintw(INTSROW, INTSCOL + 3, " Interrupts");
	mvprintw(INTSROW + 1, INTSCOL + 9, "total");

	mvprintw(VMSTATROW + 1, VMSTATCOL + 10, "cow");
	mvprintw(VMSTATROW + 2, VMSTATCOL + 10, "wire");
	mvprintw(VMSTATROW + 3, VMSTATCOL + 10, "act");
	mvprintw(VMSTATROW + 4, VMSTATCOL + 10, "inact");
	mvprintw(VMSTATROW + 5, VMSTATCOL + 10, "cache");
	mvprintw(VMSTATROW + 6, VMSTATCOL + 10, "free");
	mvprintw(VMSTATROW + 7, VMSTATCOL + 10, "daefr");
	mvprintw(VMSTATROW + 8, VMSTATCOL + 10, "prcfr");
	mvprintw(VMSTATROW + 9, VMSTATCOL + 10, "react");
	mvprintw(VMSTATROW + 10, VMSTATCOL + 10, "pdwake");
	mvprintw(VMSTATROW + 11, VMSTATCOL + 10, "pdpgs");
	mvprintw(VMSTATROW + 12, VMSTATCOL + 10, "intrn");
	mvprintw(VMSTATROW + 13, VMSTATCOL + 10, "buf");
	mvprintw(VMSTATROW + 14, VMSTATCOL + 10, "dirtybuf");

	mvprintw(VMSTATROW + 15, VMSTATCOL + 10, "desiredvnodes");
	mvprintw(VMSTATROW + 16, VMSTATCOL + 10, "numvnodes");
	mvprintw(VMSTATROW + 17, VMSTATCOL + 10, "freevnodes");

	mvprintw(GENSTATROW, GENSTATCOL, "  Csw  Trp  Sys  Int  Sof  Flt");

	mvprintw(GRAPHROW, GRAPHCOL,
		"  . %%Sys    . %%Intr   . %%User   . %%Nice   . %%Idle");
	mvprintw(PROCSROW, PROCSCOL, "Proc:r  p  d  s  w");
	mvprintw(GRAPHROW + 1, GRAPHCOL,
		"|    |    |    |    |    |    |    |    |    |    |");

	mvprintw(NAMEIROW, NAMEICOL, "Namei         Name-cache    Dir-cache");
	mvprintw(NAMEIROW + 1, NAMEICOL,
		"    Calls     hits    %%     hits    %%");
	mvprintw(DISKROW, DISKCOL, "Disks");
	mvprintw(DISKROW + 1, DISKCOL, "KB/t");
	mvprintw(DISKROW + 2, DISKCOL, "tps");
	mvprintw(DISKROW + 3, DISKCOL, "MB/s");
	mvprintw(DISKROW + 4, DISKCOL, "%% busy");
	/*
	 * For now, we don't support a fourth disk statistic.  So there's
	 * no point in providing a label for it.  If someone can think of a
	 * fourth useful disk statistic, there is room to add it.
	 */
	/* mvprintw(DISKROW + 4, DISKCOL, " msps"); */
	j = 0;
	for (i = 0; i < num_devices && j < MAXDRIVES; i++)
		if (dev_select[i].selected) {
			char tmpstr[80];
			sprintf(tmpstr, "%s%d", dev_select[i].device_name,
				dev_select[i].unit_number);
			mvprintw(DISKROW, DISKCOL + 5 + 6 * j,
				" %5.5s", tmpstr);
			j++;
		}

	if (j <= 4) {
		/*
		 * room for extended VM stats
		 */
		mvprintw(VMSTATROW + 11, VMSTATCOL - 6, "zfod");
		mvprintw(VMSTATROW + 12, VMSTATCOL - 6, "ofod");
		mvprintw(VMSTATROW + 13, VMSTATCOL - 6, "%%slo-z");
		mvprintw(VMSTATROW + 14, VMSTATCOL - 6, "tfree");
		extended_vm_stats = 1;
	} else {
		extended_vm_stats = 0;
		mvprintw(VMSTATROW + 0, VMSTATCOL + 10, "zfod");
	}

	for (i = 0; i < nintr; i++) {
		if (intrloc[i] == 0)
			continue;
		mvprintw(intrloc[i], INTSCOL + 9, "%-10.10s", intrname[i]);
	}
}

#define X(fld)	{t=s.fld[i]; s.fld[i]-=s1.fld[i]; if(state==TIME) s1.fld[i]=t;}
#define Q(fld)	{t=cur.fld[i]; cur.fld[i]-=last.fld[i]; if(state==TIME) last.fld[i]=t;}
#define Y(fld)	{t = s.fld; s.fld -= s1.fld; if(state == TIME) s1.fld = t;}
#define Z(fld)	{t = s.nchstats.fld; s.nchstats.fld -= s1.nchstats.fld; \
	if(state == TIME) s1.nchstats.fld = t;}
#define PUTRATE(fld, l, c, w) \
	Y(fld); \
	putint((int)((float)s.fld/etime + 0.5), l, c, w)
#define MAXFAIL 5

static	char cpuchar[CPUSTATES] = { '=' , '+', '>', '-', ' ' };
static	char cpuorder[CPUSTATES] = { CP_SYS, CP_INTR, CP_USER, CP_NICE,
				     CP_IDLE };

void
showkre()
{
	float f1, f2;
	int psiz, inttotal;
	int i, l, c;
	static int failcnt = 0;

	etime = 0;
	for(i = 0; i < CPUSTATES; i++) {
		X(time);
		Q(cp_time);
		etime += s.time[i];
	}
	if (etime < 5.0) {	/* < 5 ticks - ignore this trash */
		if (failcnt++ >= MAXFAIL) {
			clear();
			mvprintw(2, 10, "The alternate system clock has died!");
			mvprintw(3, 10, "Reverting to ``pigs'' display.");
			move(CMDLINE, 0);
			refresh();
			failcnt = 0;
			sleep(5);
			command("pigs");
		}
		return;
	}
	failcnt = 0;
	etime /= hertz;
	etime /= ncpu;
	inttotal = 0;
	for (i = 0; i < nintr; i++) {
		if (s.intrcnt[i] == 0)
			continue;
		if (intrloc[i] == 0) {
			if (nextintsrow == LINES)
				continue;
			intrloc[i] = nextintsrow++;
			mvprintw(intrloc[i], INTSCOL + 9, "%-10.10s",
				intrname[i]);
		}
		X(intrcnt);
		l = (int)((float)s.intrcnt[i]/etime + 0.5);
		inttotal += l;
		putint(l, intrloc[i], INTSCOL + 2, 6);
	}
	putint(inttotal, INTSROW + 1, INTSCOL + 2, 6);
	Z(ncs_goodhits); Z(ncs_badhits); Z(ncs_miss);
	Z(ncs_long); Z(ncs_pass2); Z(ncs_2passes); Z(ncs_neghits);
	s.nchcount = nchtotal.ncs_goodhits + nchtotal.ncs_badhits +
	    nchtotal.ncs_miss + nchtotal.ncs_long + nchtotal.ncs_neghits;
	if (state == TIME)
		s1.nchcount = s.nchcount;

	psiz = 0;
	f2 = 0.0;
	for (c = 0; c < CPUSTATES; c++) {
		i = cpuorder[c];
		f1 = cputime(i);
		f2 += f1;
		l = (int) ((f2 + 1.0) / 2.0) - psiz;
		if (f1 > 99.9)
			f1 = 99.9;	/* no room to display 100.0 */
		putfloat(f1, GRAPHROW, GRAPHCOL + 10 * c, 4, 1, 0);
		move(GRAPHROW + 2, psiz);
		psiz += l;
		while (l-- > 0)
			addch(cpuchar[c]);
	}

	putint(ucount(), STATROW, STATCOL, 3);
	putfloat(avenrun[0], STATROW, STATCOL + 17, 6, 2, 0);
	putfloat(avenrun[1], STATROW, STATCOL + 23, 6, 2, 0);
	putfloat(avenrun[2], STATROW, STATCOL + 29, 6, 2, 0);
	mvaddstr(STATROW, STATCOL + 53, buf);
#define pgtokb(pg)	((pg) * s.v_page_size / 1024)
	putint(pgtokb(total.t_arm), MEMROW + 2, MEMCOL + 3, 8);
	putint(pgtokb(total.t_armshr), MEMROW + 2, MEMCOL + 11, 8);
	putint(pgtokb(total.t_avm), MEMROW + 2, MEMCOL + 19, 9);
	putint(pgtokb(total.t_avmshr), MEMROW + 2, MEMCOL + 28, 9);
	putint(pgtokb(total.t_rm), MEMROW + 3, MEMCOL + 3, 8);
	putint(pgtokb(total.t_rmshr), MEMROW + 3, MEMCOL + 11, 8);
	putint(pgtokb(total.t_vm), MEMROW + 3, MEMCOL + 19, 9);
	putint(pgtokb(total.t_vmshr), MEMROW + 3, MEMCOL + 28, 9);
	putint(pgtokb(total.t_free), MEMROW + 2, MEMCOL + 37, 8);
	putint(total.t_rq - 1, PROCSROW + 1, PROCSCOL + 3, 3);
	putint(total.t_pw, PROCSROW + 1, PROCSCOL + 6, 3);
	putint(total.t_dw, PROCSROW + 1, PROCSCOL + 9, 3);
	putint(total.t_sl, PROCSROW + 1, PROCSCOL + 12, 3);
	putint(total.t_sw, PROCSROW + 1, PROCSCOL + 15, 3);
	if (extended_vm_stats == 0) {
		PUTRATE(v_zfod, VMSTATROW + 0, VMSTATCOL + 4, 5);
	}
	PUTRATE(v_cow_faults, VMSTATROW + 1, VMSTATCOL + 3, 6);
	putint(pgtokb(s.v_wire_count), VMSTATROW + 2, VMSTATCOL, 9);
	putint(pgtokb(s.v_active_count), VMSTATROW + 3, VMSTATCOL, 9);
	putint(pgtokb(s.v_inactive_count), VMSTATROW + 4, VMSTATCOL, 9);
	putint(pgtokb(s.v_cache_count), VMSTATROW + 5, VMSTATCOL, 9);
	putint(pgtokb(s.v_free_count), VMSTATROW + 6, VMSTATCOL, 9);
	PUTRATE(v_dfree, VMSTATROW + 7, VMSTATCOL, 9);
	PUTRATE(v_pfree, VMSTATROW + 8, VMSTATCOL, 9);
	PUTRATE(v_reactivated, VMSTATROW + 9, VMSTATCOL, 9);
	PUTRATE(v_pdwakeups, VMSTATROW + 10, VMSTATCOL, 9);
	PUTRATE(v_pdpages, VMSTATROW + 11, VMSTATCOL, 9);
	PUTRATE(v_intrans, VMSTATROW + 12, VMSTATCOL, 9);

	if (extended_vm_stats) {
	    PUTRATE(v_zfod, VMSTATROW + 11, VMSTATCOL - 16, 9);
	    PUTRATE(v_ozfod, VMSTATROW + 12, VMSTATCOL - 16, 9);
	    putint(
		((s.v_ozfod < s.v_zfod) ?
		    s.v_ozfod * 100 / s.v_zfod : 
		    0
		),
		VMSTATROW + 13, 
		VMSTATCOL - 16,
		9
	    );
	    PUTRATE(v_tfree, VMSTATROW + 14, VMSTATCOL - 16, 9);
	}

	putint(s.bufspace/1024, VMSTATROW + 13, VMSTATCOL, 9);
	putint(s.numdirtybuffers, VMSTATROW + 14, VMSTATCOL, 9);
	putint(s.desiredvnodes, VMSTATROW + 15, VMSTATCOL, 9);
	putint(s.numvnodes, VMSTATROW + 16, VMSTATCOL, 9);
	putint(s.freevnodes, VMSTATROW + 17, VMSTATCOL, 9);
	PUTRATE(v_vnodein, PAGEROW + 2, PAGECOL + 5, 5);
	PUTRATE(v_vnodeout, PAGEROW + 2, PAGECOL + 10, 5);
	PUTRATE(v_swapin, PAGEROW + 2, PAGECOL + 17, 5);
	PUTRATE(v_swapout, PAGEROW + 2, PAGECOL + 22, 5);
	PUTRATE(v_vnodepgsin, PAGEROW + 3, PAGECOL + 5, 5);
	PUTRATE(v_vnodepgsout, PAGEROW + 3, PAGECOL + 10, 5);
	PUTRATE(v_swappgsin, PAGEROW + 3, PAGECOL + 17, 5);
	PUTRATE(v_swappgsout, PAGEROW + 3, PAGECOL + 22, 5);
	PUTRATE(v_swtch, GENSTATROW + 1, GENSTATCOL, 5);
	PUTRATE(v_trap, GENSTATROW + 1, GENSTATCOL + 5, 5);
	PUTRATE(v_syscall, GENSTATROW + 1, GENSTATCOL + 10, 5);
	PUTRATE(v_intr, GENSTATROW + 1, GENSTATCOL + 15, 5);
	PUTRATE(v_soft, GENSTATROW + 1, GENSTATCOL + 20, 5);
	PUTRATE(v_vm_faults, GENSTATROW + 1, GENSTATCOL + 25, 5);
	mvprintw(DISKROW, DISKCOL + 5, "                              ");
	for (i = 0, c = 0; i < num_devices && c < MAXDRIVES; i++)
		if (dev_select[i].selected) {
			char tmpstr[80];
			sprintf(tmpstr, "%s%d", dev_select[i].device_name,
				dev_select[i].unit_number);
			mvprintw(DISKROW, DISKCOL + 5 + 6 * c,
				" %5.5s", tmpstr);
			switch(state) {
			case TIME:
				dinfo(i, ++c, &cur, &last);
				break;
			case RUN:
				dinfo(i, ++c, &cur, &run);
				break;
			case BOOT:
				dinfo(i, ++c, &cur, NULL);
				break;
			}
		}
	putint(s.nchcount, NAMEIROW + 2, NAMEICOL, 9);
	putint((nchtotal.ncs_goodhits + nchtotal.ncs_neghits),
	   NAMEIROW + 2, NAMEICOL + 9, 9);
#define nz(x)	((x) ? (x) : 1)
	putfloat((nchtotal.ncs_goodhits+nchtotal.ncs_neghits) *
	   100.0 / nz(s.nchcount),
	   NAMEIROW + 2, NAMEICOL + 19, 4, 0, 1);
	putint(nchtotal.ncs_pass2, NAMEIROW + 2, NAMEICOL + 23, 9);
	putfloat(nchtotal.ncs_pass2 * 100.0 / nz(s.nchcount),
	   NAMEIROW + 2, NAMEICOL + 33, 4, 0, 1);
#undef nz
}

int
cmdkre(cmd, args)
	char *cmd, *args;
{
	int retval;

	if (prefix(cmd, "run")) {
		retval = 1;
		copyinfo(&s2, &s1);
		switch (getdevs(&run)) {
		case -1:
			errx(1, "%s", devstat_errbuf);
			break;
		case 1:
			num_devices = run.dinfo->numdevs;
			generation = run.dinfo->generation;
			retval = dscmd("refresh", NULL, MAXDRIVES, &cur);
			if (retval == 2)
				labelkre();
			break;
		default:
			break;
		}
		state = RUN;
		return (retval);
	}
	if (prefix(cmd, "boot")) {
		state = BOOT;
		copyinfo(&z, &s1);
		return (1);
	}
	if (prefix(cmd, "time")) {
		state = TIME;
		return (1);
	}
	if (prefix(cmd, "zero")) {
		retval = 1;
		if (state == RUN) {
			getinfo(&s1, RUN);
			switch (getdevs(&run)) {
			case -1:
				errx(1, "%s", devstat_errbuf);
				break;
			case 1:
				num_devices = run.dinfo->numdevs;
				generation = run.dinfo->generation;
				retval = dscmd("refresh",NULL, MAXDRIVES, &cur);
				if (retval == 2)
					labelkre();
				break;
			default:
				break;
			}
		}
		return (retval);
	}
	retval = dscmd(cmd, args, MAXDRIVES, &cur);

	if (retval == 2)
		labelkre();

	return(retval);
}

/* calculate number of users on the system */
static int
ucount()
{
	register int nusers = 0;

	if (ut < 0)
		return (0);
	while (read(ut, &utmp, sizeof(utmp)))
		if (utmp.ut_name[0] != '\0')
			nusers++;

	lseek(ut, 0L, L_SET);
	return (nusers);
}

static float
cputime(indx)
	int indx;
{
	double t;
	register int i;

	t = 0;
	for (i = 0; i < CPUSTATES; i++)
		t += s.time[i];
	if (t == 0.0)
		t = 1.0;
	return (s.time[indx] * 100.0 / t);
}

static void
putint(n, l, c, w)
	int n, l, c, w;
{
	char b[128];

	move(l, c);
	if (n == 0) {
		while (w-- > 0)
			addch(' ');
		return;
	}
	snprintf(b, sizeof(b), "%*d", w, n);
	if (strlen(b) > w) {
		while (w-- > 0)
			addch('*');
		return;
	}
	addstr(b);
}

static void
putfloat(f, l, c, w, d, nz)
	double f;
	int l, c, w, d, nz;
{
	char b[128];

	move(l, c);
	if (nz && f == 0.0) {
		while (--w >= 0)
			addch(' ');
		return;
	}
	snprintf(b, sizeof(b), "%*.*f", w, d, f);
	if (strlen(b) > w)
		snprintf(b, sizeof(b), "%*.0f", w, f);
	if (strlen(b) > w) {
		while (--w >= 0)
			addch('*');
		return;
	}
	addstr(b);
}

static void
putlongdouble(f, l, c, w, d, nz)
	long double f;
	int l, c, w, d, nz;
{
	char b[128];

	move(l, c);
	if (nz && f == 0.0) {
		while (--w >= 0)
			addch(' ');
		return;
	}
	sprintf(b, "%*.*Lf", w, d, f);
	if (strlen(b) > w)
		sprintf(b, "%*.0Lf", w, f);
	if (strlen(b) > w) {
		while (--w >= 0)
			addch('*');
		return;
	}
	addstr(b);
}

static void
getinfo(s, st)
	struct Info *s;
	enum state st;
{
	struct devinfo *tmp_dinfo;
	size_t size;
	int mib[2];

	GETSYSCTL("kern.cp_time", s->time);
	GETSYSCTL("kern.cp_time", cur.cp_time);
	GETSYSCTL("vm.stats.sys.v_swtch", s->v_swtch);
	GETSYSCTL("vm.stats.sys.v_trap", s->v_trap);
	GETSYSCTL("vm.stats.sys.v_syscall", s->v_syscall);
	GETSYSCTL("vm.stats.sys.v_intr", s->v_intr);
	GETSYSCTL("vm.stats.sys.v_soft", s->v_soft);
	GETSYSCTL("vm.stats.vm.v_vm_faults", s->v_vm_faults);
	GETSYSCTL("vm.stats.vm.v_cow_faults", s->v_cow_faults);
	GETSYSCTL("vm.stats.vm.v_zfod", s->v_zfod);
	GETSYSCTL("vm.stats.vm.v_ozfod", s->v_ozfod);
	GETSYSCTL("vm.stats.vm.v_swapin", s->v_swapin);
	GETSYSCTL("vm.stats.vm.v_swapout", s->v_swapout);
	GETSYSCTL("vm.stats.vm.v_swappgsin", s->v_swappgsin);
	GETSYSCTL("vm.stats.vm.v_swappgsout", s->v_swappgsout);
	GETSYSCTL("vm.stats.vm.v_vnodein", s->v_vnodein);
	GETSYSCTL("vm.stats.vm.v_vnodeout", s->v_vnodeout);
	GETSYSCTL("vm.stats.vm.v_vnodepgsin", s->v_vnodepgsin);
	GETSYSCTL("vm.stats.vm.v_vnodepgsout", s->v_vnodepgsout);
	GETSYSCTL("vm.stats.vm.v_intrans", s->v_intrans);
	GETSYSCTL("vm.stats.vm.v_reactivated", s->v_reactivated);
	GETSYSCTL("vm.stats.vm.v_pdwakeups", s->v_pdwakeups);
	GETSYSCTL("vm.stats.vm.v_pdpages", s->v_pdpages);
	GETSYSCTL("vm.stats.vm.v_dfree", s->v_dfree);
	GETSYSCTL("vm.stats.vm.v_pfree", s->v_pfree);
	GETSYSCTL("vm.stats.vm.v_tfree", s->v_tfree);
	GETSYSCTL("vm.stats.vm.v_page_size", s->v_page_size);
	GETSYSCTL("vm.stats.vm.v_free_count", s->v_free_count);
	GETSYSCTL("vm.stats.vm.v_wire_count", s->v_wire_count);
	GETSYSCTL("vm.stats.vm.v_active_count", s->v_active_count);
	GETSYSCTL("vm.stats.vm.v_inactive_count", s->v_inactive_count);
	GETSYSCTL("vm.stats.vm.v_cache_count", s->v_cache_count);
	GETSYSCTL("vfs.bufspace", s->bufspace);
	GETSYSCTL("kern.maxvnodes", s->desiredvnodes);
	GETSYSCTL("debug.numvnodes", s->numvnodes);
	GETSYSCTL("debug.freevnodes", s->freevnodes);
	GETSYSCTL("vfs.cache.nchstats", s->nchstats);
	GETSYSCTL("vfs.numdirtybuffers", s->numdirtybuffers);
	getsysctl("hw.intrcnt", s->intrcnt, nintr * LONG);

	size = sizeof(s->Total);
	mib[0] = CTL_VM;
	mib[1] = VM_METER;
	if (sysctl(mib, 2, &s->Total, &size, NULL, 0) < 0) {
		error("Can't get kernel info: %s\n", strerror(errno));
		bzero(&s->Total, sizeof(s->Total));
	}
	size = sizeof(ncpu);
	if (sysctlbyname("hw.ncpu", &ncpu, &size, NULL, 0) < 0 ||
	    size != sizeof(ncpu))
		ncpu = 1;

	tmp_dinfo = last.dinfo;
	last.dinfo = cur.dinfo;
	cur.dinfo = tmp_dinfo;

	last.busy_time = cur.busy_time;
	switch (getdevs(&cur)) {
	case -1:
		errx(1, "%s", devstat_errbuf);
		break;
	case 1:
		num_devices = cur.dinfo->numdevs;
		generation = cur.dinfo->generation;
		cmdkre("refresh", NULL);
		break;
	default:
		break;
	}
}

static void
allocinfo(s)
	struct Info *s;
{

	s->intrcnt = (long *) calloc(nintr, sizeof(long));
	if (s->intrcnt == NULL)
		errx(2, "out of memory");
}

static void
copyinfo(from, to)
	register struct Info *from, *to;
{
	long *intrcnt;

	/*
	 * time, wds, seek, and xfer are malloc'd so we have to
	 * save the pointers before the structure copy and then
	 * copy by hand.
	 */
	intrcnt = to->intrcnt;
	*to = *from;

	bcopy(from->intrcnt, to->intrcnt = intrcnt, nintr * sizeof (int));
}

static void
dinfo(dn, c, now, then)
	int dn, c;
	struct statinfo *now, *then;
{
	long double transfers_per_second;
	long double kb_per_transfer, mb_per_second;
	long double elapsed_time, device_busy;
	int di;

	di = dev_select[dn].position;

	elapsed_time = compute_etime(now->busy_time, then ?
				     then->busy_time :
				     now->dinfo->devices[di].dev_creation_time);

	device_busy =  compute_etime(now->dinfo->devices[di].busy_time, then ?
				     then->dinfo->devices[di].busy_time :
				     now->dinfo->devices[di].dev_creation_time);

	if (compute_stats(&now->dinfo->devices[di], then ?
			  &then->dinfo->devices[di] : NULL, elapsed_time,
			  NULL, NULL, NULL,
			  &kb_per_transfer, &transfers_per_second,
			  &mb_per_second, NULL, NULL) != 0)
		errx(1, "%s", devstat_errbuf);

	if ((device_busy == 0) && (transfers_per_second > 5))
		/* the device has been 100% busy, fake it because
		 * as long as the device is 100% busy the busy_time
		 * field in the devstat struct is not updated */
		device_busy = elapsed_time;
	if (device_busy > elapsed_time)
		/* this normally happens after one or more periods
		 * where the device has been 100% busy, correct it */
		device_busy = elapsed_time;

	c = DISKCOL + c * 6;
	putlongdouble(kb_per_transfer, DISKROW + 1, c, 5, 2, 0);
	putlongdouble(transfers_per_second, DISKROW + 2, c, 5, 0, 0);
	putlongdouble(mb_per_second, DISKROW + 3, c, 5, 2, 0);
	putlongdouble(device_busy * 100 / elapsed_time, DISKROW + 4, c, 5, 0, 0);
}
