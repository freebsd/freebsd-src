/*
 * Copyright (c) 1980, 1986, 1991 The Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1980, 1986, 1991 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)vmstat.c	5.31 (Berkeley) 7/2/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/signal.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_statistics.h>
#include <time.h>
#include <nlist.h>
#include <kvm.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>

#define NEWVM			/* XXX till old has been updated or purged */
struct nlist nl[] = {
#define	X_CPTIME	0
	{ "_cp_time" },
#define X_TOTAL		1
	{ "_total" },
#define X_SUM		2
	{ "_cnt" },		/* XXX for now that's where it is */
#define	X_BOOTTIME	3
	{ "_boottime" },
#define	X_DKXFER	4
	{ "_dk_xfer" },
#define X_HZ		5
	{ "_hz" },
#define X_PHZ		6
	{ "_phz" },
#define X_NCHSTATS	7
	{ "_nchstats" },
#define	X_INTRNAMES	8
	{ "_intrnames" },
#define	X_EINTRNAMES	9
	{ "_eintrnames" },
#define	X_INTRCNT	10
	{ "_intrcnt" },
#define	X_EINTRCNT	11
	{ "_eintrcnt" },
#define	X_DK_NDRIVE	12
	{ "_dk_ndrive" },
#define	X_KMEMSTAT	13
	{ "_kmemstats" },
#define	X_KMEMBUCKETS	14
	{ "_bucket" },
#define	X_VMSTAT	15
	{ "_vm_stat" },
#ifdef notdef
#define	X_DEFICIT	15
	{ "_deficit" },
#define	X_FORKSTAT	16
	{ "_forkstat" },
#define X_REC		17
	{ "_rectime" },
#define X_PGIN		18
	{ "_pgintime" },
#define	X_XSTATS	19
	{ "_xstats" },
#define X_END		19
#else
#define X_END		15
#endif
#ifdef hp300
#define	X_HPDINIT	(X_END+1)
	{ "_hp_dinit" },
#endif
#ifdef tahoe
#define	X_VBDINIT	(X_END+1)
	{ "_vbdinit" },
#define	X_CKEYSTATS	(X_END+2)
	{ "_ckeystats" },
#define	X_DKEYSTATS	(X_END+3)
	{ "_dkeystats" },
#endif
#ifdef vax
#define X_MBDINIT	(X_END+1)
	{ "_mbdinit" },
#define X_UBDINIT	(X_END+2)
	{ "_ubdinit" },
#endif
#ifdef __386BSD__
#define	X_FREE		(X_END+1)
	{ "_vm_page_free_count" },
#define	X_ACTIVE	(X_END+2)
	{ "_vm_page_active_count" },
#define	X_INACTIVE	(X_END+3)
	{ "_vm_page_inactive_count" },
#define	X_WIRED		(X_END+4)
	{ "_vm_page_wire_count" },
#define	X_PAGESIZE	(X_END+5)
	{ "_page_size" },
#define	X_ISA_BIO	(X_END+6)
	{ "_isa_devtab_bio" },
#endif /* __386BSD__ */
	{ "" },
};

struct _disk {
	long time[CPUSTATES];
	long *xfer;
} cur, last;

struct	vm_statistics vm_stat, ostat;
struct	vmmeter sum, osum;
char	*vmunix = _PATH_UNIX;
char	**dr_name;
int	*dr_select, dk_ndrive, ndrives;
#ifdef __386BSD__
      /* to make up for statistics that don't get updated */
int	size, free_count, active_count, inactive, wired; 
#endif

int	winlines = 20;

#define	FORKSTAT	0x01
#define	INTRSTAT	0x02
#define	MEMSTAT		0x04
#define	SUMSTAT		0x08
#define	TIMESTAT	0x10
#define	VMSTAT		0x20

#include "names.c"			/* disk names -- machine dependent */

void	cpustats(), dkstats(), dointr(), domem(), dosum();
void	dovmstat(), kread(), usage();
#ifdef notdef
void	dotimes(), doforkst();
#endif

main(argc, argv)
	register int argc;
	register char **argv;
{
	extern int optind;
	extern char *optarg;
	register int c, todo;
	u_int interval;
	int reps;
	char *kmem;

	kmem = NULL;
	interval = reps = todo = 0;
	while ((c = getopt(argc, argv, "c:fiM:mN:stw:")) != EOF) {
		switch (c) {
		case 'c':
			reps = atoi(optarg);
			break;
#ifndef notdef
		case 'f':
			todo |= FORKSTAT;
			break;
#endif
		case 'i':
			todo |= INTRSTAT;
			break;
		case 'M':
			kmem = optarg;
			break;
		case 'm':
			todo |= MEMSTAT;
			break;
		case 'N':
			vmunix = optarg;
			break;
		case 's':
			todo |= SUMSTAT;
			break;
#ifndef notdef
		case 't':
			todo |= TIMESTAT;
			break;
#endif
		case 'w':
			interval = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (todo == 0)
		todo = VMSTAT;

	if (kvm_openfiles(vmunix, kmem, NULL) < 0) {
		(void)fprintf(stderr,
		    "vmstat: kvm_openfiles: %s\n", kvm_geterr());
		exit(1);
	}

	if ((c = kvm_nlist(nl)) != 0) {
		if (c > 0) {
			(void)fprintf(stderr,
			    "vmstat: undefined symbols in %s:", vmunix);
			for (c = 0; c < sizeof(nl)/sizeof(nl[0]); c++)
				if (nl[c].n_type == 0)
					fprintf(stderr, " %s", nl[c].n_name);
			(void)fputc('\n', stderr);
		} else
			(void)fprintf(stderr, "vmstat: kvm_nlist: %s\n",
			    kvm_geterr());
		exit(1);
	}

	if (todo & VMSTAT) {
		char **getdrivedata();
		struct winsize winsize;

		argv = getdrivedata(argv);
		winsize.ws_row = 0;
		(void) ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *)&winsize);
		if (winsize.ws_row > 0)
			winlines = winsize.ws_row;

	}

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		interval = atoi(*argv);
		if (*++argv)
			reps = atoi(*argv);
	}
#endif

	if (interval) {
		if (!reps)
			reps = -1;
	} else if (reps)
		interval = 1;

#ifdef notdef
	if (todo & FORKSTAT)
		doforkst();
#endif
	if (todo & MEMSTAT)
		domem();
	if (todo & SUMSTAT)
		dosum();
#ifdef notdef
	if (todo & TIMESTAT)
		dotimes();
#endif
	if (todo & INTRSTAT)
		dointr();
	if (todo & VMSTAT)
		dovmstat(interval, reps);
	exit(0);
}

char **
getdrivedata(argv)
	char **argv;
{
	register int i;
	register char **cp;
	char buf[30];

	kread(X_DK_NDRIVE, &dk_ndrive, sizeof(dk_ndrive));
	if (dk_ndrive <= 0) {
		(void)fprintf(stderr, "vmstat: dk_ndrive %d\n", dk_ndrive);
		exit(1);
	}
	dr_select = calloc((size_t)dk_ndrive, sizeof(int));
	dr_name = calloc((size_t)dk_ndrive, sizeof(char *));
	for (i = 0; i < dk_ndrive; i++)
		dr_name[i] = NULL;
	cur.xfer = calloc((size_t)dk_ndrive, sizeof(long));
	last.xfer = calloc((size_t)dk_ndrive, sizeof(long));
	read_names();
	for (i = 0; i < dk_ndrive; i++)
		if (dr_name[i] == NULL) {
			(void)sprintf(buf, "??%d", i);
			dr_name[i] = strdup(buf);
		}

	/*
	 * Choose drives to be displayed.  Priority goes to (in order) drives
	 * supplied as arguments, default drives.  If everything isn't filled
	 * in and there are drives not taken care of, display the first few
	 * that fit.
	 */
#define BACKWARD_COMPATIBILITY
	for (ndrives = 0; *argv; ++argv) {
#ifdef	BACKWARD_COMPATIBILITY
		if (isdigit(**argv))
			break;
#endif
		for (i = 0; i < dk_ndrive; i++) {
			if (strcmp(dr_name[i], *argv))
				continue;
			dr_select[i] = 1;
			++ndrives;
			break;
		}
	}
	for (i = 0; i < dk_ndrive && ndrives < 4; i++) {
		if (dr_select[i])
			continue;
		for (cp = defdrives; *cp; cp++)
			if (strcmp(dr_name[i], *cp) == 0) {
				dr_select[i] = 1;
				++ndrives;
				break;
			}
	}
	for (i = 0; i < dk_ndrive && ndrives < 4; i++) {
		if (dr_select[i])
			continue;
		dr_select[i] = 1;
		++ndrives;
	}
	return(argv);
}

#ifdef __386BSD__
/* 
 * Make up for the fact that under 0.1, VM doesn't update all of the
 * fields in the statistics structures.
 */

void
fill_in_vm_stat(vm_stat)
        struct vm_statistics *vm_stat;
{
	kread(X_FREE, &vm_stat->free_count, sizeof(vm_stat->free_count));
	kread(X_ACTIVE, &vm_stat->active_count, sizeof(vm_stat->active_count));
	kread(X_INACTIVE, &vm_stat->inactive_count, 
	      sizeof(vm_stat->inactive_count));
	kread(X_WIRED, &vm_stat->wire_count, sizeof(vm_stat->wire_count));
	kread(X_PAGESIZE, &vm_stat->pagesize, sizeof(vm_stat->pagesize));
}
#endif

long
getuptime()
{
	static time_t now, boottime;
	time_t uptime;

	if (boottime == 0)
		kread(X_BOOTTIME, &boottime, sizeof(boottime));
	(void)time(&now);
	uptime = now - boottime;
	if (uptime <= 0 || uptime > 60*60*24*365*10) {
		(void)fprintf(stderr,
		    "vmstat: time makes no sense; namelist must be wrong.\n");
		exit(1);
	}
	return(uptime);
}

int	hz, hdrcnt;

void
dovmstat(interval, reps)
	u_int interval;
	int reps;
{
	struct vmtotal total;
	time_t uptime, halfuptime;
	void needhdr();
#ifndef notdef
	int deficit;
#endif

	uptime = getuptime();
	halfuptime = uptime / 2;
	(void)signal(SIGCONT, needhdr);

	if (nl[X_PHZ].n_type != 0 && nl[X_PHZ].n_value != 0)
		kread(X_PHZ, &hz, sizeof(hz));
	if (!hz)
		kread(X_HZ, &hz, sizeof(hz));

	for (hdrcnt = 1;;) {
		if (!--hdrcnt)
			printhdr();
		kread(X_CPTIME, cur.time, sizeof(cur.time));
		kread(X_DKXFER, cur.xfer, sizeof(*cur.xfer) * dk_ndrive);
		kread(X_SUM, &sum, sizeof(sum));
		kread(X_TOTAL, &total, sizeof(total));
		kread(X_VMSTAT, &vm_stat, sizeof(vm_stat));
#ifdef __386BSD__
		fill_in_vm_stat (&vm_stat);
#endif
#ifdef notdef
		kread(X_DEFICIT, &deficit, sizeof(deficit));
#endif
		(void)printf("%2d %1d %1d ",
		    total.t_rq, total.t_dw + total.t_pw, total.t_sw);
#define pgtok(a) ((a)*NBPG >> 10)
#define	rate(x)	(((x) + halfuptime) / uptime)	/* round */
		(void)printf("%5ld %5ld ",
#ifdef __386BSD__
		    pgtok(vm_stat.active_count), pgtok(vm_stat.free_count));
#else
		    pgtok(total.t_avm), pgtok(total.t_free));
#endif
#ifdef NEWVM
		(void)printf("%4lu ", rate(vm_stat.faults - ostat.faults));
		(void)printf("%3lu ",
		    rate(vm_stat.reactivations - ostat.reactivations));
		(void)printf("%3lu ", rate(vm_stat.pageins - ostat.pageins));
		(void)printf("%3lu %3lu ",
		    rate(vm_stat.pageouts - ostat.pageouts), 0);
#else
		(void)printf("%3lu %2lu ",
		    rate(sum.v_pgrec - (sum.v_xsfrec+sum.v_xifrec) -
		    (osum.v_pgrec - (osum.v_xsfrec+osum.v_xifrec))),
		    rate(sum.v_xsfrec + sum.v_xifrec -
		    osum.v_xsfrec - osum.v_xifrec));
		(void)printf("%3lu ",
		    rate(pgtok(sum.v_pgpgin - osum.v_pgpgin)));
		(void)printf("%3lu %3lu ",
		    rate(pgtok(sum.v_pgpgout - osum.v_pgpgout)),
		    rate(pgtok(sum.v_dfree - osum.v_dfree)));
		(void)printf("%3d ", pgtok(deficit));
#endif
		(void)printf("%3lu ", rate(sum.v_scan - osum.v_scan));
		dkstats();
		(void)printf("%4lu %4lu %3lu ",
		    rate(sum.v_intr - osum.v_intr),
		    rate(sum.v_syscall - osum.v_syscall),
		    rate(sum.v_swtch - osum.v_swtch));
		cpustats();
		(void)printf("\n");
		(void)fflush(stdout);
		if (reps >= 0 && --reps <= 0)
			break;
		osum = sum;
		ostat = vm_stat;
		uptime = interval;
		/*
		 * We round upward to avoid losing low-frequency events
		 * (i.e., >= 1 per interval but < 1 per second).
		 */
		halfuptime = (uptime + 1) / 2;
		(void)sleep(interval);
	}
}

printhdr()
{
	register int i;

	(void)printf(" procs   memory     page%*s", 20, "");
	if (ndrives > 1)
		(void)printf("disks %*s  faults      cpu\n",
		   ndrives * 3 - 6, "");
	else
		(void)printf("%*s  faults      cpu\n", ndrives * 3, "");
#ifndef NEWVM
	(void)printf(" r b w   avm   fre  re at  pi  po  fr  de  sr ");
#else
	(void)printf(" r b w   avm   fre  flt  re  pi  po  fr  sr ");
#endif
	for (i = 0; i < dk_ndrive; i++)
		if (dr_select[i])
			(void)printf("%c%c ", dr_name[i][0],
			    dr_name[i][strlen(dr_name[i]) - 1]);
	(void)printf("  in   sy  cs us sy id\n");
	hdrcnt = winlines - 2;
}

/*
 * Force a header to be prepended to the next output.
 */
void
needhdr()
{

	hdrcnt = 1;
}

#ifdef notdef
void
dotimes()
{
	u_int pgintime, rectime;

	kread(X_REC, &rectime, sizeof(rectime));
	kread(X_PGIN, &pgintime, sizeof(pgintime));
	kread(X_SUM, &sum, sizeof(sum));
	(void)printf("%u reclaims, %u total time (usec)\n",
	    sum.v_pgrec, rectime);
	(void)printf("average: %u usec / reclaim\n", rectime / sum.v_pgrec);
	(void)printf("\n");
	(void)printf("%u page ins, %u total time (msec)\n",
	    sum.v_pgin, pgintime / 10);
	(void)printf("average: %8.1f msec / page in\n",
	    pgintime / (sum.v_pgin * 10.0));
}
#endif

pct(top, bot)
	long top, bot;
{
	if (bot == 0)
		return(0);
	return((top * 100) / bot);
}

#define	PCT(top, bot) pct((long)(top), (long)(bot))

#if defined(tahoe)
#include <machine/cpu.h>
#endif

void
dosum()
{
	struct nchstats nchstats;
#ifndef NEWVM
	struct xstats xstats;
#endif
	long nchtotal;
#if defined(tahoe)
	struct keystats keystats;
#endif

	kread(X_SUM, &sum, sizeof(sum));
#ifdef NEWVM
	kread(X_VMSTAT, &vm_stat, sizeof(vm_stat));
#ifdef __386BSD__
	fill_in_vm_stat(&vm_stat);
#endif
#else
	(void)printf("%9u swap ins\n", sum.v_swpin);
	(void)printf("%9u swap outs\n", sum.v_swpout);
	(void)printf("%9u pages swapped in\n", sum.v_pswpin / CLSIZE);
	(void)printf("%9u pages swapped out\n", sum.v_pswpout / CLSIZE);
	(void)printf("%9u total address trans. faults taken\n", sum.v_faults);
	(void)printf("%9u page ins\n", sum.v_pgin);
	(void)printf("%9u page outs\n", sum.v_pgout);
	(void)printf("%9u pages paged in\n", sum.v_pgpgin);
	(void)printf("%9u pages paged out\n", sum.v_pgpgout);
	(void)printf("%9u sequential process pages freed\n", sum.v_seqfree);
	(void)printf("%9u total reclaims (%d%% fast)\n", sum.v_pgrec,
	    PCT(sum.v_fastpgrec, sum.v_pgrec));
	(void)printf("%9u reclaims from free list\n", sum.v_pgfrec);
	(void)printf("%9u intransit blocking page faults\n", sum.v_intrans);
	(void)printf("%9u zero fill pages created\n", sum.v_nzfod / CLSIZE);
	(void)printf("%9u zero fill page faults\n", sum.v_zfod / CLSIZE);
	(void)printf("%9u executable fill pages created\n",
	    sum.v_nexfod / CLSIZE);
	(void)printf("%9u executable fill page faults\n",
	    sum.v_exfod / CLSIZE);
	(void)printf("%9u swap text pages found in free list\n",
	    sum.v_xsfrec);
	(void)printf("%9u inode text pages found in free list\n",
	    sum.v_xifrec);
	(void)printf("%9u file fill pages created\n", sum.v_nvrfod / CLSIZE);
	(void)printf("%9u file fill page faults\n", sum.v_vrfod / CLSIZE);
	(void)printf("%9u pages examined by the clock daemon\n", sum.v_scan);
	(void)printf("%9u revolutions of the clock hand\n", sum.v_rev);
	(void)printf("%9u pages freed by the clock daemon\n",
	    sum.v_dfree / CLSIZE);
#endif
	(void)printf("%9u cpu context switches\n", sum.v_swtch);
	(void)printf("%9u device interrupts\n", sum.v_intr);
	(void)printf("%9u software interrupts\n", sum.v_soft);
#ifdef vax
	(void)printf("%9u pseudo-dma dz interrupts\n", sum.v_pdma);
#endif
	(void)printf("%9u traps\n", sum.v_trap);
	(void)printf("%9u system calls\n", sum.v_syscall);
#ifdef NEWVM
	(void)printf("%9u bytes per page\n", vm_stat.pagesize);
	(void)printf("%9u pages free\n", vm_stat.free_count);
	(void)printf("%9u pages active\n", vm_stat.active_count);
	(void)printf("%9u pages inactive\n", vm_stat.inactive_count);
	(void)printf("%9u pages wired down\n", vm_stat.wire_count);
	(void)printf("%9u zero-fill pages\n", vm_stat.zero_fill_count);
	(void)printf("%9u pages reactivated\n", vm_stat.reactivations);
	(void)printf("%9u pageins\n", vm_stat.pageins);
	(void)printf("%9u pageouts\n", vm_stat.pageouts);
	(void)printf("%9u VM faults\n", vm_stat.faults);
	(void)printf("%9u copy-on-write faults\n", vm_stat.cow_faults);
	(void)printf("%9u VM object cache lookups\n", vm_stat.lookups);
	(void)printf("%9u VM object hits\n", vm_stat.hits);
#endif

	kread(X_NCHSTATS, &nchstats, sizeof(nchstats));
	nchtotal = nchstats.ncs_goodhits + nchstats.ncs_neghits +
	    nchstats.ncs_badhits + nchstats.ncs_falsehits +
	    nchstats.ncs_miss + nchstats.ncs_long;
	(void)printf("%9ld total name lookups\n", nchtotal);
	(void)printf(
	    "%9s cache hits (%d%% pos + %d%% neg) system %d%% per-process\n",
	    "", PCT(nchstats.ncs_goodhits, nchtotal),
	    PCT(nchstats.ncs_neghits, nchtotal),
	    PCT(nchstats.ncs_pass2, nchtotal));
	(void)printf("%9s deletions %d%%, falsehits %d%%, toolong %d%%\n", "",
	    PCT(nchstats.ncs_badhits, nchtotal),
	    PCT(nchstats.ncs_falsehits, nchtotal),
	    PCT(nchstats.ncs_long, nchtotal));
#ifndef NEWVM
	kread(X_XSTATS, &xstats, sizeof(xstats));
	(void)printf("%9lu total calls to xalloc (cache hits %d%%)\n",
	    xstats.alloc, PCT(xstats.alloc_cachehit, xstats.alloc));
	(void)printf("%9s sticky %lu flushed %lu unused %lu\n", "",
	    xstats.alloc_inuse, xstats.alloc_cacheflush, xstats.alloc_unused);
	(void)printf("%9lu total calls to xfree", xstats.free);
	(void)printf(" (sticky %lu cached %lu swapped %lu)\n",
	    xstats.free_inuse, xstats.free_cache, xstats.free_cacheswap);
#endif
#if defined(tahoe)
	kread(X_CKEYSTATS, &keystats, sizeof(keystats));
	(void)printf("%9d %s (free %d%% norefs %d%% taken %d%% shared %d%%)\n",
	    keystats.ks_allocs, "code cache keys allocated",
	    PCT(keystats.ks_allocfree, keystats.ks_allocs),
	    PCT(keystats.ks_norefs, keystats.ks_allocs),
	    PCT(keystats.ks_taken, keystats.ks_allocs),
	    PCT(keystats.ks_shared, keystats.ks_allocs));
	kread(X_DKEYSTATS, &keystats, sizeof(keystats));
	(void)printf("%9d %s (free %d%% norefs %d%% taken %d%% shared %d%%)\n",
	    keystats.ks_allocs, "data cache keys allocated",
	    PCT(keystats.ks_allocfree, keystats.ks_allocs),
	    PCT(keystats.ks_norefs, keystats.ks_allocs),
	    PCT(keystats.ks_taken, keystats.ks_allocs),
	    PCT(keystats.ks_shared, keystats.ks_allocs));
#endif
}

#ifdef notdef
void
doforkst()
{
	struct forkstat fks;

	kread(X_FORKSTAT, &fks, sizeof(struct forkstat));
	(void)printf("%d forks, %d pages, average %.2f\n",
	    fks.cntfork, fks.sizfork, (double)fks.sizfork / fks.cntfork);
	(void)printf("%d vforks, %d pages, average %.2f\n",
	    fks.cntvfork, fks.sizvfork, (double)fks.sizvfork / fks.cntvfork);
}
#endif

void
dkstats()
{
	register int dn, state;
	double etime;
	long tmp;

	for (dn = 0; dn < dk_ndrive; ++dn) {
		tmp = cur.xfer[dn];
		cur.xfer[dn] -= last.xfer[dn];
		last.xfer[dn] = tmp;
	}
	etime = 0;
	for (state = 0; state < CPUSTATES; ++state) {
		tmp = cur.time[state];
		cur.time[state] -= last.time[state];
		last.time[state] = tmp;
		etime += cur.time[state];
	}
	if (etime == 0)
		etime = 1;
	etime /= hz;
	for (dn = 0; dn < dk_ndrive; ++dn) {
		if (!dr_select[dn])
			continue;
		(void)printf("%2.0f ", cur.xfer[dn] / etime);
	}
}

void
cpustats()
{
	register int state;
	double pct, total;

	total = 0;
	for (state = 0; state < CPUSTATES; ++state)
		total += cur.time[state];
	if (total)
		pct = 100 / total;
	else
		pct = 0;
	(void)printf("%2.0f ",				/* user + nice */
	    (cur.time[0] + cur.time[1]) * pct);
	(void)printf("%2.0f ", cur.time[2] * pct);	/* system */
	(void)printf("%2.0f", cur.time[3] * pct);	/* idle */
}

void
dointr()
{
	register unsigned long *intrcnt, inttotal, uptime;
	register int nintr, inamlen;
	register char *intrname;

	uptime = getuptime();
	nintr = nl[X_EINTRCNT].n_value - nl[X_INTRCNT].n_value;
	inamlen = nl[X_EINTRNAMES].n_value - nl[X_INTRNAMES].n_value;
	intrcnt = malloc((size_t)nintr);
	intrname = malloc((size_t)inamlen);
	if (intrcnt == NULL || intrname == NULL) {
		(void)fprintf(stderr, "vmstat: %s.\n", strerror(errno));
		exit(1);
	}
	kread(X_INTRCNT, intrcnt, (size_t)nintr);
	kread(X_INTRNAMES, intrname, (size_t)inamlen);
	(void)printf("%-12s %10s %8s\n", "interrupt", "count", "rate");
	inttotal = 0;
	nintr /= sizeof(long);
	while (--nintr >= 0) {
		if (*intrcnt)
			(void)printf("%-12s %10lu %8lu\n", intrname,
			    *intrcnt, *intrcnt / uptime);
		intrname += strlen(intrname) + 1;
		inttotal += *intrcnt++;
	}
	(void)printf("%-12s %10lu %8lu\n", "Total", inttotal, inttotal / uptime);
}

/*
 * These names are defined in <sys/malloc.h>.
 */
char *kmemnames[] = INITKMEMNAMES;

void
domem()
{
	register struct kmembuckets *kp;
	register struct kmemstats *ks;
	register int i;
	int size;
	long totuse = 0, totfree = 0, totreq = 0;
	struct kmemstats kmemstats[M_LAST];
	struct kmembuckets buckets[MINBUCKET + 16];

	kread(X_KMEMBUCKETS, buckets, sizeof(buckets));
	(void)printf("Memory statistics by bucket size\n");
	(void)printf(
	    "    Size   In Use   Free   Requests  HighWater  Couldfree\n");
	for (i = MINBUCKET, kp = &buckets[i]; i < MINBUCKET + 16; i++, kp++) {
		if (kp->kb_calls == 0)
			continue;
		size = 1 << i;
		(void)printf("%8d %8ld %6ld %10ld %7ld %10ld\n", size, 
			kp->kb_total - kp->kb_totalfree,
			kp->kb_totalfree, kp->kb_calls,
			kp->kb_highwat, kp->kb_couldfree);
		totfree += size * kp->kb_totalfree;
	}

	kread(X_KMEMSTAT, kmemstats, sizeof(kmemstats));
	(void)printf("\nMemory statistics by type\n");
	(void)printf(
"       Type  In Use   MemUse   HighUse  Limit Requests  TypeLimit KernLimit\n");
	for (i = 0, ks = &kmemstats[0]; i < M_LAST; i++, ks++) {
		if (ks->ks_calls == 0)
			continue;
		(void)printf("%11s %7ld %7ldK %8ldK %5ldK %8ld %6u %9u\n",
		    kmemnames[i] ? kmemnames[i] : "undefined",
		    ks->ks_inuse, (ks->ks_memuse + 1023) / 1024,
		    (ks->ks_maxused + 1023) / 1024,
		    (ks->ks_limit + 1023) / 1024, ks->ks_calls,
		    ks->ks_limblocks, ks->ks_mapblocks);
		totuse += ks->ks_memuse;
		totreq += ks->ks_calls;
	}
	(void)printf("\nMemory Totals:  In Use    Free    Requests\n");
	(void)printf("              %7ldK %6ldK    %8ld\n",
	     (totuse + 1023) / 1024, (totfree + 1023) / 1024, totreq);
}

/*
 * kread reads something from the kernel, given its nlist index.
 */
void
kread(nlx, addr, size)
	int nlx;
	void *addr;
	size_t size;
{
	char *sym;

	if (nl[nlx].n_type == 0 || nl[nlx].n_value == 0) {
		sym = nl[nlx].n_name;
		if (*sym == '_')
			++sym;
		(void)fprintf(stderr,
		    "vmstat: %s: symbol %s not defined\n", vmunix, sym);
		exit(1);
	}
	if (kvm_read((void *)nl[nlx].n_value, addr, size) != size) {
		sym = nl[nlx].n_name;
		if (*sym == '_')
			++sym;
		(void)fprintf(stderr, "vmstat: %s: %s\n", sym, kvm_geterr());
		exit(1);
	}
}

void
usage()
{
	(void)fprintf(stderr,
#ifndef NEWVM
	    "usage: vmstat [-fimst] [-c count] [-M core] \
[-N system] [-w wait] [disks]\n");
#else
	    "usage: vmstat [-ims] [-c count] [-M core] \
[-N system] [-w wait] [disks]\n");
#endif
	exit(1);
}
