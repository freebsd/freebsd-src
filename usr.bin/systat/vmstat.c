/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
#include <libutil.h>
#include <nlist.h>
#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>
#include "systat.h"
#include "extern.h"
#include "devs.h"

static struct Info {
	long	time[CPUSTATES];
	uint64_t v_swtch;	/* context switches */
	uint64_t v_trap;	/* calls to trap */
	uint64_t v_syscall;	/* calls to syscall() */
	uint64_t v_intr;	/* device interrupts */
	uint64_t v_soft;	/* software interrupts */
	/*
	 * Virtual memory activity.
	 */
	uint64_t v_vm_faults;	/* number of address memory faults */
	uint64_t v_io_faults;	/* page faults requiring I/O */
	uint64_t v_cow_faults;	/* number of copy-on-writes */
	uint64_t v_zfod;	/* pages zero filled on demand */
	uint64_t v_ozfod;	/* optimized zero fill pages */
	uint64_t v_swapin;	/* swap pager pageins */
	uint64_t v_swapout;	/* swap pager pageouts */
	uint64_t v_swappgsin;	/* swap pager pages paged in */
	uint64_t v_swappgsout;	/* swap pager pages paged out */
	uint64_t v_vnodein;	/* vnode pager pageins */
	uint64_t v_vnodeout;	/* vnode pager pageouts */
	uint64_t v_vnodepgsin;	/* vnode_pager pages paged in */
	uint64_t v_vnodepgsout;	/* vnode pager pages paged out */
	uint64_t v_intrans;	/* intransit blocking page faults */
	uint64_t v_reactivated;	/* number of pages reactivated by pagedaemon */
	uint64_t v_pdwakeups;	/* number of times daemon has awaken from sleep */
	uint64_t v_pdpages;	/* number of pages analyzed by daemon */

	uint64_t v_dfree;	/* pages freed by daemon */
	uint64_t v_pfree;	/* pages freed by exiting processes */
	uint64_t v_tfree;	/* total pages freed */
	/*
	 * Distribution of page usages.
	 */
	u_int v_free_count;	/* number of pages free */
	u_int v_wire_count;	/* number of pages wired down */
	u_int v_active_count;	/* number of pages active */
	u_int v_inactive_count;	/* number of pages inactive */
	u_int v_laundry_count;	/* number of pages in laundry queue */
	u_long v_kmem_map_size;	/* Current kmem allocation size */
	struct	vmtotal Total;
	struct	nchstats nchstats;
	long	nchcount;
	long	*intrcnt;
	long	bufspace;
	u_long	maxvnodes;
	long	numvnodes;
	long	freevnodes;
	int	numdirtybuffers;
} s, s1, s2, z;
static u_long kmem_size;
static u_int v_page_count;


#define	total s.Total
#define	nchtotal s.nchstats
#define	oldnchtotal s1.nchstats

static	enum state { BOOT, TIME, RUN } state = TIME;
enum divisor { IEC = 0, SI = HN_DIVISOR_1000 };

static void allocinfo(struct Info *);
static void copyinfo(struct Info *, struct Info *);
static float cputime(int);
static void do_putuint64(uint64_t, int, int, int, int);
static void getinfo(struct Info *);
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
#define PAGEROW		 1	/* uses 4 rows and 30 cols */
#define PAGECOL		47
#define INTSROW		 5	/* uses all rows to bottom and 16 cols */
#define INTSCOL		64
#define PROCSROW	 6	/* uses 3 rows and 20 cols */
#define PROCSCOL	 0
#define GENSTATROW	 7	/* uses 2 rows and 29 cols */
#define GENSTATCOL	22
#define VMSTATROW	 5	/* uses 17 rows and 12-14 cols */
#define VMSTATCOL	52
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

	if (dsinit(MAXDRIVES) != 1)
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
					sz = strlen(cp);
					*cp1 = '\0';
					cp1 = cp1 + 2;
					cp2 = strdup(cp);
					bcopy(cp1, cp, sz - (cp1 - cp) + 1);
					if (sz <= 10 + 4) {
						strcat(cp, " ");
						strcat(cp, cp2 + 3);
					}
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
	GETSYSCTL("vm.kmem_size", kmem_size);
	GETSYSCTL("vm.stats.vm.v_page_count", v_page_count);
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
			d_first ? "%e %b %T" : "%b %e %T", tp);
	getinfo(&s);
}

void
labelkre(void)
{
	int i;

	clear();
	mvprintw(STATROW, STATCOL + 6, "users    Load");
	mvprintw(STATROW + 1, STATCOL + 3, "Mem usage:    %%Phy   %%Kmem");
	mvprintw(MEMROW, MEMCOL, "Mem:      REAL           VIRTUAL");
	mvprintw(MEMROW + 1, MEMCOL, "       Tot   Share     Tot    Share");
	mvprintw(MEMROW + 2, MEMCOL, "Act");
	mvprintw(MEMROW + 3, MEMCOL, "All");

	mvprintw(MEMROW + 1, MEMCOL + 40, "Free");

	mvprintw(PAGEROW, PAGECOL,     "         VN PAGER   SWAP PAGER");
	mvprintw(PAGEROW + 1, PAGECOL, "         in   out     in   out");
	mvprintw(PAGEROW + 2, PAGECOL, "count");
	mvprintw(PAGEROW + 3, PAGECOL, "pages");

	mvprintw(INTSROW, INTSCOL + 1, "Interrupts");
	mvprintw(INTSROW + 1, INTSCOL + 6, "total");

	mvprintw(VMSTATROW, VMSTATCOL + 6, "ioflt");
	mvprintw(VMSTATROW + 1, VMSTATCOL + 6, "cow");
	mvprintw(VMSTATROW + 2, VMSTATCOL + 6, "zfod");
	mvprintw(VMSTATROW + 3, VMSTATCOL + 6, "ozfod");
	mvprintw(VMSTATROW + 4, VMSTATCOL + 6 - 1, "%%ozfod");
	mvprintw(VMSTATROW + 5, VMSTATCOL + 6, "daefr");
	mvprintw(VMSTATROW + 6, VMSTATCOL + 6, "prcfr");
	mvprintw(VMSTATROW + 7, VMSTATCOL + 6, "totfr");
	mvprintw(VMSTATROW + 8, VMSTATCOL + 6, "react");
	mvprintw(VMSTATROW + 9, VMSTATCOL + 6, "pdwak");
	mvprintw(VMSTATROW + 10, VMSTATCOL + 6, "pdpgs");
	mvprintw(VMSTATROW + 11, VMSTATCOL + 6, "intrn");
	mvprintw(VMSTATROW + 12, VMSTATCOL + 6, "wire");
	mvprintw(VMSTATROW + 13, VMSTATCOL + 6, "act");
	mvprintw(VMSTATROW + 14, VMSTATCOL + 6, "inact");
	mvprintw(VMSTATROW + 15, VMSTATCOL + 6, "laund");
	mvprintw(VMSTATROW + 16, VMSTATCOL + 6, "free");
	if (LINES - 1 > VMSTATROW + 17)
		mvprintw(VMSTATROW + 17, VMSTATCOL + 6, "buf");

	mvprintw(GENSTATROW, GENSTATCOL, " Csw  Trp  Sys  Int  Sof  Flt");

	mvprintw(GRAPHROW, GRAPHCOL,
		"  . %%Sys    . %%Intr   . %%User   . %%Nice   . %%Idle");
	mvprintw(PROCSROW, PROCSCOL, "Proc:");
	mvprintw(PROCSROW + 1, PROCSCOL, "  r   p   d    s   w");
	mvprintw(GRAPHROW + 1, GRAPHCOL,
		"|    |    |    |    |    |    |    |    |    |    |");

	mvprintw(VNSTATROW, VNSTATCOL + 8, "dtbuf");
	mvprintw(VNSTATROW + 1, VNSTATCOL + 8, "maxvn");
	mvprintw(VNSTATROW + 2, VNSTATCOL + 8, "numvn");
	mvprintw(VNSTATROW + 3, VNSTATCOL + 8, "frevn");

	mvprintw(NAMEIROW, NAMEICOL, "Namei     Name-cache   Dir-cache");
	mvprintw(NAMEIROW + 1, NAMEICOL,
		"   Calls    hits   %%    hits   %%");
	dslabel(MAXDRIVES, DISKCOL, DISKROW);

	for (i = 0; i < nintr; i++) {
		if (intrloc[i] == 0)
			continue;
		mvprintw(intrloc[i], INTSCOL + 6, "%-10.10s", intrname[i]);
	}
}

#define X(fld)	{t=s.fld[i]; s.fld[i]-=s1.fld[i]; if(state==TIME) s1.fld[i]=t;}
#define Q(fld)	{t=cur_dev.fld[i]; cur_dev.fld[i]-=last_dev.fld[i]; if(state==TIME) last_dev.fld[i]=t;}
#define Y(fld)	{t = s.fld; s.fld -= s1.fld; if(state == TIME) s1.fld = t;}
#define Z(fld)	{t = s.nchstats.fld; s.nchstats.fld -= s1.nchstats.fld; \
	if(state == TIME) s1.nchstats.fld = t;}
#define PUTRATE(fld, l, c, w) \
do { \
	Y(fld); \
	sysputwuint64(wnd, l, c, w, (s.fld/etime + 0.5), 0); \
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
		X(intrcnt);
		l = (int)((float)s.intrcnt[i]/etime + 0.5);
		inttotal += l;
		if (intrloc[i] == 0) {
			if (nextintsrow == LINES)
				continue;
			intrloc[i] = nextintsrow++;
			mvprintw(intrloc[i], INTSCOL + 6, "%-10.10s",
				intrname[i]);
		}
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
	putfloat(100.0 * (v_page_count - total.t_free) / v_page_count,
	   STATROW + 1, STATCOL + 15, 2, 0, 1);
	putfloat(100.0 * s.v_kmem_map_size / kmem_size,
	   STATROW + 1, STATCOL + 22, 2, 0, 1);

	sysputpage(wnd, MEMROW + 2, MEMCOL + 4, 6, total.t_arm, 0);
	sysputpage(wnd, MEMROW + 2, MEMCOL + 12, 6, total.t_armshr, 0);
	sysputpage(wnd, MEMROW + 2, MEMCOL + 20, 6, total.t_avm, 0);
	sysputpage(wnd, MEMROW + 2, MEMCOL + 29, 6, total.t_avmshr, 0);
	sysputpage(wnd, MEMROW + 3, MEMCOL + 4, 6, total.t_rm, 0);
	sysputpage(wnd, MEMROW + 3, MEMCOL + 12, 6, total.t_rmshr, 0);
	sysputpage(wnd, MEMROW + 3, MEMCOL + 20, 6, total.t_vm, 0);
	sysputpage(wnd, MEMROW + 3, MEMCOL + 29, 6, total.t_vmshr, 0);
	sysputpage(wnd, MEMROW + 2, MEMCOL + 38, 6, total.t_free, 0);
	putint(total.t_rq - 1, PROCSROW + 2, PROCSCOL, 3);
	putint(total.t_pw, PROCSROW + 2, PROCSCOL + 4, 3);
	putint(total.t_dw, PROCSROW + 2, PROCSCOL + 8, 3);
	putint(total.t_sl, PROCSROW + 2, PROCSCOL + 12, 4);
	putint(total.t_sw, PROCSROW + 2, PROCSCOL + 17, 3);
	PUTRATE(v_io_faults, VMSTATROW, VMSTATCOL, 5);
	PUTRATE(v_cow_faults, VMSTATROW + 1, VMSTATCOL, 5);
	PUTRATE(v_zfod, VMSTATROW + 2, VMSTATCOL, 5);
	PUTRATE(v_ozfod, VMSTATROW + 3, VMSTATCOL, 5);
	putint(s.v_zfod != 0 ? (int)(s.v_ozfod * 100.0 / s.v_zfod) : 0,
	    VMSTATROW + 4, VMSTATCOL, 5);
	PUTRATE(v_dfree, VMSTATROW + 5, VMSTATCOL, 5);
	PUTRATE(v_pfree, VMSTATROW + 6, VMSTATCOL, 5);
	PUTRATE(v_tfree, VMSTATROW + 7, VMSTATCOL, 5);
	PUTRATE(v_reactivated, VMSTATROW + 8, VMSTATCOL, 5);
	PUTRATE(v_pdwakeups, VMSTATROW + 9, VMSTATCOL, 5);
	PUTRATE(v_pdpages, VMSTATROW + 10, VMSTATCOL, 5);
	PUTRATE(v_intrans, VMSTATROW + 11, VMSTATCOL, 5);
	sysputpage(wnd, VMSTATROW + 12, VMSTATCOL, 5, s.v_wire_count, 0);
	sysputpage(wnd, VMSTATROW + 13, VMSTATCOL, 5, s.v_active_count, 0);
	sysputpage(wnd, VMSTATROW + 14, VMSTATCOL, 5, s.v_inactive_count, 0);
	sysputpage(wnd, VMSTATROW + 15, VMSTATCOL, 5, s.v_laundry_count, 0);
	sysputpage(wnd, VMSTATROW + 16, VMSTATCOL, 5, s.v_free_count, 0);
	if (LINES - 1 > VMSTATROW + 17)
		sysputuint64(wnd, VMSTATROW + 17, VMSTATCOL, 5, s.bufspace, 0);
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
	switch(state) {
	case TIME:
		dsshow(MAXDRIVES, DISKCOL, DISKROW, &cur_dev, &last_dev);
		break;
	case RUN:
		dsshow(MAXDRIVES, DISKCOL, DISKROW, &cur_dev, &run_dev);
		break;
	case BOOT:
		dsshow(MAXDRIVES, DISKCOL, DISKROW, &cur_dev, NULL);
		break;
	}
	putint(s.numdirtybuffers, VNSTATROW, VNSTATCOL, 7);
	putint(s.maxvnodes, VNSTATROW + 1, VNSTATCOL, 7);
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
		switch (devstat_getdevs(NULL, &run_dev)) {
		case -1:
			errx(1, "%s", devstat_errbuf);
			break;
		case 1:
			num_devices = run_dev.dinfo->numdevs;
			generation = run_dev.dinfo->generation;
			retval = dscmd("refresh", NULL, MAXDRIVES, &cur_dev);
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
			switch (devstat_getdevs(NULL, &run_dev)) {
			case -1:
				errx(1, "%s", devstat_errbuf);
				break;
			case 1:
				num_devices = run_dev.dinfo->numdevs;
				generation = run_dev.dinfo->generation;
				retval = dscmd("refresh",NULL, MAXDRIVES, &cur_dev);
				if (retval == 2)
					labelkre();
				break;
			default:
				break;
			}
		}
		return (retval);
	}
	retval = dscmd(cmd, args, MAXDRIVES, &cur_dev);

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

void
putint(int n, int l, int lc, int w)
{

	do_putuint64(n, l, lc, w, SI);
}

static void
do_putuint64(uint64_t n, int l, int lc, int w, int div)
{
	int snr;
	char b[128];
	char lbuf[128];

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
	snr = snprintf(b, sizeof(b), "%*ju", w, (uintmax_t)n);
	if (snr != w) {
		humanize_number(lbuf, w, n, "", HN_AUTOSCALE,
		    HN_NOSPACE | HN_DECIMAL | div);
		snr = snprintf(b, sizeof(b), "%*s", w, lbuf);
	}
	if (snr != w) {
		while (w-- > 0)
			addch('*');
		return;
	}
	addstr(b);
}

void
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
	if (snr != w)
		snr = snprintf(b, sizeof(b), "%*.0fk", w - 1, f / 1000);
	if (snr != w)
		snr = snprintf(b, sizeof(b), "%*.0fM", w - 1, f / 1000000);
	if (snr != w) {
		while (--w >= 0)
			addch('*');
		return;
	}
	addstr(b);
}

void
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
	if (snr != w)
		snr = snprintf(b, sizeof(b), "%*.0Lfk", w - 1, f / 1000);
	if (snr != w)
		snr = snprintf(b, sizeof(b), "%*.0LfM", w - 1, f / 1000000);
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
	GETSYSCTL("kern.cp_time", cur_dev.cp_time);
	GETSYSCTL("vm.stats.sys.v_swtch", ls->v_swtch);
	GETSYSCTL("vm.stats.sys.v_trap", ls->v_trap);
	GETSYSCTL("vm.stats.sys.v_syscall", ls->v_syscall);
	GETSYSCTL("vm.stats.sys.v_intr", ls->v_intr);
	GETSYSCTL("vm.stats.sys.v_soft", ls->v_soft);
	GETSYSCTL("vm.stats.vm.v_vm_faults", ls->v_vm_faults);
	GETSYSCTL("vm.stats.vm.v_io_faults", ls->v_io_faults);
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
	GETSYSCTL("vm.stats.vm.v_free_count", ls->v_free_count);
	GETSYSCTL("vm.stats.vm.v_wire_count", ls->v_wire_count);
	GETSYSCTL("vm.stats.vm.v_active_count", ls->v_active_count);
	GETSYSCTL("vm.stats.vm.v_inactive_count", ls->v_inactive_count);
	GETSYSCTL("vm.stats.vm.v_laundry_count", ls->v_laundry_count);
	GETSYSCTL("vfs.bufspace", ls->bufspace);
	GETSYSCTL("kern.maxvnodes", ls->maxvnodes);
	GETSYSCTL("vfs.numvnodes", ls->numvnodes);
	GETSYSCTL("vfs.freevnodes", ls->freevnodes);
	GETSYSCTL("vfs.cache.nchstats", ls->nchstats);
	GETSYSCTL("vfs.numdirtybuffers", ls->numdirtybuffers);
	GETSYSCTL("vm.kmem_map_size", ls->v_kmem_map_size);
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

	tmp_dinfo = last_dev.dinfo;
	last_dev.dinfo = cur_dev.dinfo;
	cur_dev.dinfo = tmp_dinfo;

	last_dev.snap_time = cur_dev.snap_time;
	dsgetinfo(&cur_dev);
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
