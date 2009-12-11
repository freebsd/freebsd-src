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

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#ifdef lint
static const char sccsid[] = "@(#)vmstat.c	8.2 (Berkeley) 1/12/94";
#endif

/*
 * Cursed vmstat -- from Robert Elz.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

#include <vm/vm_param.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <langinfo.h>
#include <nlist.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define	_ULOG_POSIX_NAMES
#include <ulog.h>
#include <unistd.h>
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
	long	bufspace;
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

static void allocinfo(struct Info *);
static void copyinfo(struct Info *, struct Info *);
static float cputime(int);
static void dinfo(int, int, struct statinfo *, struct statinfo *);
static void getinfo(struct Info *);
static void putint(int, int, int, int);
static void putfloat(double, int, int, int, int, int);
static void putlongdouble(long double, int, int, int, int, int);
static int ucount(void);

static	int ncpu;
static	char buf[26];
static	time_t t;
static	double etime;
static	int nintr;
static	long *intrloc;
static	char **intrname;
static	int nextintsrow;

WINDOW *
openkre(void)
{

	return (stdscr);
}

void
closekre(WINDOW *w)
{

	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
}

/*
 * These constants define where the major pieces are laid out
 */
#define STATROW		 0	/* uses 1 row and 67 cols */
#define STATCOL		 0
#define MEMROW		 2	/* uses 4 rows and 45 cols */
#define MEMCOL		 0
#define PAGEROW		 2	/* uses 4 rows and 30 cols */
#define PAGECOL		47
#define INTSROW		 6	/* uses all rows to bottom and 16 cols */
#define INTSCOL		64
#define PROCSROW	 6	/* uses 3 rows and 19 cols */
#define PROCSCOL	 0
#define GENSTATROW	 7	/* uses 2 rows and 29 cols */
#define GENSTATCOL	21
#define VMSTATROW	 7	/* uses 17 rows and 12-14 cols */
#define VMSTATCOL	49	/* actually 50-51 for some fields */
#define GRAPHROW	10	/* uses 3 rows and 49-51 cols */
#define GRAPHCOL	 0
#define VNSTATROW	13	/* uses 4 rows and 13 columns */
#define VNSTATCOL	35
#define NAMEIROW	14	/* uses 3 rows and 32 cols */
#define NAMEICOL	 0
#define DISKROW		18	/* uses 5 rows and 47 cols (for 7 drives) */
#define DISKCOL		 0

#define	DRIVESPACE	 7	/* max # for space */

#define	MAXDRIVES	DRIVESPACE	 /* max # to display */

int
initkre(void)
{
	char *cp, *cp1, *cp2, *intrnamebuf, *nextcp;
	int i;
	size_t sz;

	if ((num_devices = devstat_getnumdevs(NULL)) < 0) {
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
		if (sysctlbyname("hw.intrcnt", NULL, &sz, NULL, 0) == -1) {
			error("sysctl(hw.intrcnt...) failed: %s",
			      strerror(errno));
			return (0);
		}
		nintr = sz / sizeof(u_long);
		intrloc = calloc(nintr, sizeof (long));
		intrname = calloc(nintr, sizeof (char *));
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
			nextcp = cp + strlen(cp) + 1;

			/* Discard trailing spaces. */
			for (cp1 = nextcp - 1; cp1 > cp && *(cp1 - 1) == ' '; )
				*--cp1 = '\0';

			/* Convert "irqN: name" to "name irqN". */
			if (strncmp(cp, "irq", 3) == 0) {
				cp1 = cp + 3;
				while (isdigit((u_char)*cp1))
					cp1++;
				if (cp1 != cp && *cp1 == ':' &&
				    *(cp1 + 1) == ' ') {
					*cp1 = '\0';
					cp1 = cp1 + 2;
					cp2 = strdup(cp);
					bcopy(cp1, cp, strlen(cp1) + 1);
					strcat(cp, " ");
					strcat(cp, cp2);
					free(cp2);
				}
			}

			/*
			 * Convert "name irqN" to "name N" if the former is
			 * longer than the field width.
			 */
			if ((cp1 = strstr(cp, "irq")) != NULL &&
			    strlen(cp) > 10)
				bcopy(cp1 + 3, cp1, strlen(cp1 + 3) + 1);

			intrname[i] = cp;
			cp = nextcp;
		}
		nextintsrow = INTSROW + 2;
		allocinfo(&s);
		allocinfo(&s1);
		allocinfo(&s2);
		allocinfo(&z);
	}
	getinfo(&s2);
	copyinfo(&s2, &s1);
	return(1);
}

void
fetchkre(void)
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
	getinfo(&s);
}

void
labelkre(void)
{
	int i, j;

	clear();
	mvprintw(STATROW, STATCOL + 6, "users    Load");
	mvprintw(MEMROW, MEMCOL, "Mem:KB    REAL            VIRTUAL");
	mvprintw(MEMROW + 1, MEMCOL, "        Tot   Share      Tot    Share");
	mvprintw(MEMROW + 2, MEMCOL, "Act");
	mvprintw(MEMROW + 3, MEMCOL, "All");

	mvprintw(MEMROW + 1, MEMCOL + 41, "Free");

	mvprintw(PAGEROW, PAGECOL,     "         VN PAGER   SWAP PAGER");
	mvprintw(PAGEROW + 1, PAGECOL, "         in   out     in   out");
	mvprintw(PAGEROW + 2, PAGECOL, "count");
	mvprintw(PAGEROW + 3, PAGECOL, "pages");

	mvprintw(INTSROW, INTSCOL + 1, "Interrupts");
	mvprintw(INTSROW + 1, INTSCOL + 6, "total");

	mvprintw(VMSTATROW, VMSTATCOL + 9, "cow");
	mvprintw(VMSTATROW + 1, VMSTATCOL + 9, "zfod");
	mvprintw(VMSTATROW + 2, VMSTATCOL + 9, "ozfod");
	mvprintw(VMSTATROW + 3, VMSTATCOL + 9 - 1, "%%ozfod");
	mvprintw(VMSTATROW + 4, VMSTATCOL + 9, "daefr");
	mvprintw(VMSTATROW + 5, VMSTATCOL + 9, "prcfr");
	mvprintw(VMSTATROW + 6, VMSTATCOL + 9, "totfr");
	mvprintw(VMSTATROW + 7, VMSTATCOL + 9, "react");
	mvprintw(VMSTATROW + 8, VMSTATCOL + 9, "pdwak");
	mvprintw(VMSTATROW + 9, VMSTATCOL + 9, "pdpgs");
	mvprintw(VMSTATROW + 10, VMSTATCOL + 9, "intrn");
	mvprintw(VMSTATROW + 11, VMSTATCOL + 9, "wire");
	mvprintw(VMSTATROW + 12, VMSTATCOL + 9, "act");
	mvprintw(VMSTATROW + 13, VMSTATCOL + 9, "inact");
	mvprintw(VMSTATROW + 14, VMSTATCOL + 9, "cache");
	mvprintw(VMSTATROW + 15, VMSTATCOL + 9, "free");
	if (LINES - 1 > VMSTATROW + 16)
		mvprintw(VMSTATROW + 16, VMSTATCOL + 9, "buf");

	mvprintw(GENSTATROW, GENSTATCOL, " Csw  Trp  Sys  Int  Sof  Flt");

	mvprintw(GRAPHROW, GRAPHCOL,
		"  . %%Sys    . %%Intr   . %%User   . %%Nice   . %%Idle");
	mvprintw(PROCSROW, PROCSCOL, "Proc:");
	mvprintw(PROCSROW + 1, PROCSCOL, "  r   p   d   s   w");
	mvprintw(GRAPHROW + 1, GRAPHCOL,
		"|    |    |    |    |    |    |    |    |    |    |");

	mvprintw(VNSTATROW, VNSTATCOL + 8, "dtbuf");
	mvprintw(VNSTATROW + 1, VNSTATCOL + 8, "desvn");
	mvprintw(VNSTATROW + 2, VNSTATCOL + 8, "numvn");
	mvprintw(VNSTATROW + 3, VNSTATCOL + 8, "frevn");

	mvprintw(NAMEIROW, NAMEICOL, "Namei     Name-cache   Dir-cache");
	mvprintw(NAMEIROW + 1, NAMEICOL,
		"   Calls    hits   %%    hits   %%");
	mvprintw(DISKROW, DISKCOL, "Disks");
	mvprintw(DISKROW + 1, DISKCOL, "KB/t");
	mvprintw(DISKROW + 2, DISKCOL, "tps");
	mvprintw(DISKROW + 3, DISKCOL, "MB/s");
	mvprintw(DISKROW + 4, DISKCOL, "%%busy");
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

	for (i = 0; i < nintr; i++) {
		if (intrloc[i] == 0)
			continue;
		mvprintw(intrloc[i], INTSCOL + 6, "%-10.10s", intrname[i]);
	}
}

#define X(fld)	{t=s.fld[i]; s.fld[i]-=s1.fld[i]; if(state==TIME) s1.fld[i]=t;}
#define Q(fld)	{t=cur.fld[i]; cur.fld[i]-=last.fld[i]; if(state==TIME) last.fld[i]=t;}
#define Y(fld)	{t = s.fld; s.fld -= s1.fld; if(state == TIME) s1.fld = t;}
#define Z(fld)	{t = s.nchstats.fld; s.nchstats.fld -= s1.nchstats.fld; \
	if(state == TIME) s1.nchstats.fld = t;}
#define PUTRATE(fld, l, c, w) \
do { \
	Y(fld); \
	putint((int)((float)s.fld/etime + 0.5), l, c, w); \
} while (0)
#define MAXFAIL 5

static	char cpuchar[CPUSTATES] = { '=' , '+', '>', '-', ' ' };
static	char cpuorder[CPUSTATES] = { CP_SYS, CP_INTR, CP_USER, CP_NICE,
				     CP_IDLE };

void
showkre(void)
{
	float f1, f2;
	int psiz, inttotal;
	int i, l, lc;
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
			mvprintw(intrloc[i], INTSCOL + 6, "%-10.10s",
				intrname[i]);
		}
		X(intrcnt);
		l = (int)((float)s.intrcnt[i]/etime + 0.5);
		inttotal += l;
		putint(l, intrloc[i], INTSCOL, 5);
	}
	putint(inttotal, INTSROW + 1, INTSCOL, 5);
	Z(ncs_goodhits); Z(ncs_badhits); Z(ncs_miss);
	Z(ncs_long); Z(ncs_pass2); Z(ncs_2passes); Z(ncs_neghits);
	s.nchcount = nchtotal.ncs_goodhits + nchtotal.ncs_badhits +
	    nchtotal.ncs_miss + nchtotal.ncs_long + nchtotal.ncs_neghits;
	if (state == TIME)
		s1.nchcount = s.nchcount;

	psiz = 0;
	f2 = 0.0;
	for (lc = 0; lc < CPUSTATES; lc++) {
		i = cpuorder[lc];
		f1 = cputime(i);
		f2 += f1;
		l = (int) ((f2 + 1.0) / 2.0) - psiz;
		putfloat(f1, GRAPHROW, GRAPHCOL + 10 * lc, 4, 1, 0);
		move(GRAPHROW + 2, psiz);
		psiz += l;
		while (l-- > 0)
			addch(cpuchar[lc]);
	}

	putint(ucount(), STATROW, STATCOL, 5);
	putfloat(avenrun[0], STATROW, STATCOL + 20, 5, 2, 0);
	putfloat(avenrun[1], STATROW, STATCOL + 26, 5, 2, 0);
	putfloat(avenrun[2], STATROW, STATCOL + 32, 5, 2, 0);
	mvaddstr(STATROW, STATCOL + 55, buf);
#define pgtokb(pg)	((pg) * (s.v_page_size / 1024))
	putint(pgtokb(total.t_arm), MEMROW + 2, MEMCOL + 4, 7);
	putint(pgtokb(total.t_armshr), MEMROW + 2, MEMCOL + 12, 7);
	putint(pgtokb(total.t_avm), MEMROW + 2, MEMCOL + 20, 8);
	putint(pgtokb(total.t_avmshr), MEMROW + 2, MEMCOL + 29, 8);
	putint(pgtokb(total.t_rm), MEMROW + 3, MEMCOL + 4, 7);
	putint(pgtokb(total.t_rmshr), MEMROW + 3, MEMCOL + 12, 7);
	putint(pgtokb(total.t_vm), MEMROW + 3, MEMCOL + 20, 8);
	putint(pgtokb(total.t_vmshr), MEMROW + 3, MEMCOL + 29, 8);
	putint(pgtokb(total.t_free), MEMROW + 2, MEMCOL + 38, 7);
	putint(total.t_rq - 1, PROCSROW + 2, PROCSCOL, 3);
	putint(total.t_pw, PROCSROW + 2, PROCSCOL + 4, 3);
	putint(total.t_dw, PROCSROW + 2, PROCSCOL + 8, 3);
	putint(total.t_sl, PROCSROW + 2, PROCSCOL + 12, 3);
	putint(total.t_sw, PROCSROW + 2, PROCSCOL + 16, 3);
	PUTRATE(v_cow_faults, VMSTATROW, VMSTATCOL + 2, 8 - 2);
	PUTRATE(v_zfod, VMSTATROW + 1, VMSTATCOL + 2, 8 - 2);
	PUTRATE(v_ozfod, VMSTATROW + 2, VMSTATCOL, 8);
	putint(s.v_zfod != 0 ? (int)(s.v_ozfod * 100.0 / s.v_zfod) : 0,
	    VMSTATROW + 3, VMSTATCOL + 1, 8 - 1);
	PUTRATE(v_dfree, VMSTATROW + 4, VMSTATCOL + 2, 8 - 2);
	PUTRATE(v_pfree, VMSTATROW + 5, VMSTATCOL + 2, 8 - 2);
	PUTRATE(v_tfree, VMSTATROW + 6, VMSTATCOL, 8);
	PUTRATE(v_reactivated, VMSTATROW + 7, VMSTATCOL, 8);
	PUTRATE(v_pdwakeups, VMSTATROW + 8, VMSTATCOL, 8);
	PUTRATE(v_pdpages, VMSTATROW + 9, VMSTATCOL, 8);
	PUTRATE(v_intrans, VMSTATROW + 10, VMSTATCOL, 8);
	putint(pgtokb(s.v_wire_count), VMSTATROW + 11, VMSTATCOL, 8);
	putint(pgtokb(s.v_active_count), VMSTATROW + 12, VMSTATCOL, 8);
	putint(pgtokb(s.v_inactive_count), VMSTATROW + 13, VMSTATCOL, 8);
	putint(pgtokb(s.v_cache_count), VMSTATROW + 14, VMSTATCOL, 8);
	putint(pgtokb(s.v_free_count), VMSTATROW + 15, VMSTATCOL, 8);
	if (LINES - 1 > VMSTATROW + 16)
		putint(s.bufspace / 1024, VMSTATROW + 16, VMSTATCOL, 8);
	PUTRATE(v_vnodein, PAGEROW + 2, PAGECOL + 6, 5);
	PUTRATE(v_vnodeout, PAGEROW + 2, PAGECOL + 12, 5);
	PUTRATE(v_swapin, PAGEROW + 2, PAGECOL + 19, 5);
	PUTRATE(v_swapout, PAGEROW + 2, PAGECOL + 25, 5);
	PUTRATE(v_vnodepgsin, PAGEROW + 3, PAGECOL + 6, 5);
	PUTRATE(v_vnodepgsout, PAGEROW + 3, PAGECOL + 12, 5);
	PUTRATE(v_swappgsin, PAGEROW + 3, PAGECOL + 19, 5);
	PUTRATE(v_swappgsout, PAGEROW + 3, PAGECOL + 25, 5);
	PUTRATE(v_swtch, GENSTATROW + 1, GENSTATCOL, 4);
	PUTRATE(v_trap, GENSTATROW + 1, GENSTATCOL + 5, 4);
	PUTRATE(v_syscall, GENSTATROW + 1, GENSTATCOL + 10, 4);
	PUTRATE(v_intr, GENSTATROW + 1, GENSTATCOL + 15, 4);
	PUTRATE(v_soft, GENSTATROW + 1, GENSTATCOL + 20, 4);
	PUTRATE(v_vm_faults, GENSTATROW + 1, GENSTATCOL + 25, 4);
	for (i = 0, lc = 0; i < num_devices && lc < MAXDRIVES; i++)
		if (dev_select[i].selected) {
			switch(state) {
			case TIME:
				dinfo(i, ++lc, &cur, &last);
				break;
			case RUN:
				dinfo(i, ++lc, &cur, &run);
				break;
			case BOOT:
				dinfo(i, ++lc, &cur, NULL);
				break;
			}
		}
	putint(s.numdirtybuffers, VNSTATROW, VNSTATCOL, 7);
	putint(s.desiredvnodes, VNSTATROW + 1, VNSTATCOL, 7);
	putint(s.numvnodes, VNSTATROW + 2, VNSTATCOL, 7);
	putint(s.freevnodes, VNSTATROW + 3, VNSTATCOL, 7);
	putint(s.nchcount, NAMEIROW + 2, NAMEICOL, 8);
	putint((nchtotal.ncs_goodhits + nchtotal.ncs_neghits),
	   NAMEIROW + 2, NAMEICOL + 9, 7);
#define nz(x)	((x) ? (x) : 1)
	putfloat((nchtotal.ncs_goodhits+nchtotal.ncs_neghits) *
	   100.0 / nz(s.nchcount),
	   NAMEIROW + 2, NAMEICOL + 17, 3, 0, 1);
	putint(nchtotal.ncs_pass2, NAMEIROW + 2, NAMEICOL + 21, 7);
	putfloat(nchtotal.ncs_pass2 * 100.0 / nz(s.nchcount),
	   NAMEIROW + 2, NAMEICOL + 29, 3, 0, 1);
#undef nz
}

int
cmdkre(const char *cmd, const char *args)
{
	int retval;

	if (prefix(cmd, "run")) {
		retval = 1;
		copyinfo(&s2, &s1);
		switch (devstat_getdevs(NULL, &run)) {
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
			getinfo(&s1);
			switch (devstat_getdevs(NULL, &run)) {
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
ucount(void)
{
	int nusers = 0;
	struct utmpx *ut;

	setutxent();
	while ((ut = getutxent()) != NULL)
		if (ut->ut_type == USER_PROCESS)
			nusers++;
	endutxent();

	return (nusers);
}

static float
cputime(int indx)
{
	double lt;
	int i;

	lt = 0;
	for (i = 0; i < CPUSTATES; i++)
		lt += s.time[i];
	if (lt == 0.0)
		lt = 1.0;
	return (s.time[indx] * 100.0 / lt);
}

static void
putint(int n, int l, int lc, int w)
{
	int snr;
	char b[128];

	move(l, lc);
#ifdef DEBUG
		while (w-- > 0)
			addch('*');
		return;
#endif
	if (n == 0) {
		while (w-- > 0)
			addch(' ');
		return;
	}
	snr = snprintf(b, sizeof(b), "%*d", w, n);
	if (snr != w)
		snr = snprintf(b, sizeof(b), "%*dk", w - 1, n / 1000);
	if (snr != w)
		snr = snprintf(b, sizeof(b), "%*dM", w - 1, n / 1000000);
	if (snr != w) {
		while (w-- > 0)
			addch('*');
		return;
	}
	addstr(b);
}

static void
putfloat(double f, int l, int lc, int w, int d, int nz)
{
	int snr;
	char b[128];

	move(l, lc);
#ifdef DEBUG
		while (--w >= 0)
			addch('*');
		return;
#endif
	if (nz && f == 0.0) {
		while (--w >= 0)
			addch(' ');
		return;
	}
	snr = snprintf(b, sizeof(b), "%*.*f", w, d, f);
	if (snr != w)
		snr = snprintf(b, sizeof(b), "%*.0f", w, f);
	if (snr != w) {
		while (--w >= 0)
			addch('*');
		return;
	}
	addstr(b);
}

static void
putlongdouble(long double f, int l, int lc, int w, int d, int nz)
{
	int snr;
	char b[128];

	move(l, lc);
#ifdef DEBUG
		while (--w >= 0)
			addch('*');
		return;
#endif
	if (nz && f == 0.0) {
		while (--w >= 0)
			addch(' ');
		return;
	}
	snr = snprintf(b, sizeof(b), "%*.*Lf", w, d, f);
	if (snr != w)
		snr = snprintf(b, sizeof(b), "%*.0Lf", w, f);
	if (snr != w) {
		while (--w >= 0)
			addch('*');
		return;
	}
	addstr(b);
}

static void
getinfo(struct Info *ls)
{
	struct devinfo *tmp_dinfo;
	size_t size;
	int mib[2];

	GETSYSCTL("kern.cp_time", ls->time);
	GETSYSCTL("kern.cp_time", cur.cp_time);
	GETSYSCTL("vm.stats.sys.v_swtch", ls->v_swtch);
	GETSYSCTL("vm.stats.sys.v_trap", ls->v_trap);
	GETSYSCTL("vm.stats.sys.v_syscall", ls->v_syscall);
	GETSYSCTL("vm.stats.sys.v_intr", ls->v_intr);
	GETSYSCTL("vm.stats.sys.v_soft", ls->v_soft);
	GETSYSCTL("vm.stats.vm.v_vm_faults", ls->v_vm_faults);
	GETSYSCTL("vm.stats.vm.v_cow_faults", ls->v_cow_faults);
	GETSYSCTL("vm.stats.vm.v_zfod", ls->v_zfod);
	GETSYSCTL("vm.stats.vm.v_ozfod", ls->v_ozfod);
	GETSYSCTL("vm.stats.vm.v_swapin", ls->v_swapin);
	GETSYSCTL("vm.stats.vm.v_swapout", ls->v_swapout);
	GETSYSCTL("vm.stats.vm.v_swappgsin", ls->v_swappgsin);
	GETSYSCTL("vm.stats.vm.v_swappgsout", ls->v_swappgsout);
	GETSYSCTL("vm.stats.vm.v_vnodein", ls->v_vnodein);
	GETSYSCTL("vm.stats.vm.v_vnodeout", ls->v_vnodeout);
	GETSYSCTL("vm.stats.vm.v_vnodepgsin", ls->v_vnodepgsin);
	GETSYSCTL("vm.stats.vm.v_vnodepgsout", ls->v_vnodepgsout);
	GETSYSCTL("vm.stats.vm.v_intrans", ls->v_intrans);
	GETSYSCTL("vm.stats.vm.v_reactivated", ls->v_reactivated);
	GETSYSCTL("vm.stats.vm.v_pdwakeups", ls->v_pdwakeups);
	GETSYSCTL("vm.stats.vm.v_pdpages", ls->v_pdpages);
	GETSYSCTL("vm.stats.vm.v_dfree", ls->v_dfree);
	GETSYSCTL("vm.stats.vm.v_pfree", ls->v_pfree);
	GETSYSCTL("vm.stats.vm.v_tfree", ls->v_tfree);
	GETSYSCTL("vm.stats.vm.v_page_size", ls->v_page_size);
	GETSYSCTL("vm.stats.vm.v_free_count", ls->v_free_count);
	GETSYSCTL("vm.stats.vm.v_wire_count", ls->v_wire_count);
	GETSYSCTL("vm.stats.vm.v_active_count", ls->v_active_count);
	GETSYSCTL("vm.stats.vm.v_inactive_count", ls->v_inactive_count);
	GETSYSCTL("vm.stats.vm.v_cache_count", ls->v_cache_count);
	GETSYSCTL("vfs.bufspace", ls->bufspace);
	GETSYSCTL("kern.maxvnodes", ls->desiredvnodes);
	GETSYSCTL("vfs.numvnodes", ls->numvnodes);
	GETSYSCTL("vfs.freevnodes", ls->freevnodes);
	GETSYSCTL("vfs.cache.nchstats", ls->nchstats);
	GETSYSCTL("vfs.numdirtybuffers", ls->numdirtybuffers);
	getsysctl("hw.intrcnt", ls->intrcnt, nintr * sizeof(u_long));

	size = sizeof(ls->Total);
	mib[0] = CTL_VM;
	mib[1] = VM_TOTAL;
	if (sysctl(mib, 2, &ls->Total, &size, NULL, 0) < 0) {
		error("Can't get kernel info: %s\n", strerror(errno));
		bzero(&ls->Total, sizeof(ls->Total));
	}
	size = sizeof(ncpu);
	if (sysctlbyname("hw.ncpu", &ncpu, &size, NULL, 0) < 0 ||
	    size != sizeof(ncpu))
		ncpu = 1;

	tmp_dinfo = last.dinfo;
	last.dinfo = cur.dinfo;
	cur.dinfo = tmp_dinfo;

	last.snap_time = cur.snap_time;
	switch (devstat_getdevs(NULL, &cur)) {
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
allocinfo(struct Info *ls)
{

	ls->intrcnt = (long *) calloc(nintr, sizeof(long));
	if (ls->intrcnt == NULL)
		errx(2, "out of memory");
}

static void
copyinfo(struct Info *from, struct Info *to)
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
dinfo(int dn, int lc, struct statinfo *now, struct statinfo *then)
{
	long double transfers_per_second;
	long double kb_per_transfer, mb_per_second;
	long double elapsed_time, device_busy;
	int di;

	di = dev_select[dn].position;

	if (then != NULL) {
		/* Calculate relative to previous sample */
		elapsed_time = now->snap_time - then->snap_time;
	} else {
		/* Calculate relative to device creation */
	        elapsed_time = now->snap_time - devstat_compute_etime(
		    &now->dinfo->devices[di].creation_time, NULL);
	}

	if (devstat_compute_statistics(&now->dinfo->devices[di], then ?
	    &then->dinfo->devices[di] : NULL, elapsed_time,
	    DSM_KB_PER_TRANSFER, &kb_per_transfer,
	    DSM_TRANSFERS_PER_SECOND, &transfers_per_second,
	    DSM_MB_PER_SECOND, &mb_per_second,
	    DSM_BUSY_PCT, &device_busy,
	    DSM_NONE) != 0)
		errx(1, "%s", devstat_errbuf);

	lc = DISKCOL + lc * 6;
	putlongdouble(kb_per_transfer, DISKROW + 1, lc, 5, 2, 0);
	putlongdouble(transfers_per_second, DISKROW + 2, lc, 5, 0, 0);
	putlongdouble(mb_per_second, DISKROW + 3, lc, 5, 2, 0);
	putlongdouble(device_busy, DISKROW + 4, lc, 5, 0, 0);
}
