/*
 * Copyright (c) 1980, 1986, 1991, 1993
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
static const char copyright[] =
"@(#) Copyright (c) 1980, 1986, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)vmstat.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id: vmstat.c,v 1.20 1997/10/10 14:08:07 phk Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/signal.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

#include <vm/vm_param.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

struct nlist namelist[] = {
#define	X_CPTIME	0
	{ "_cp_time" },
#define	X_DK_NDRIVE	1
	{ "_dk_ndrive" },
#define X_SUM		2
	{ "_cnt" },
#define	X_BOOTTIME	3
	{ "_boottime" },
#define	X_DKXFER	4
	{ "_dk_xfer" },
#define X_HZ		5
	{ "_hz" },
#define X_STATHZ	6
	{ "_stathz" },
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
#define	X_KMEMSTATISTICS	12
	{ "_kmemstatistics" },
#define	X_KMEMBUCKETS	13
	{ "_bucket" },
#ifdef notyet
#define	X_DEFICIT	14
	{ "_deficit" },
#define	X_FORKSTAT	15
	{ "_forkstat" },
#define X_REC		16
	{ "_rectime" },
#define X_PGIN		17
	{ "_pgintime" },
#define	X_XSTATS	18
	{ "_xstats" },
#define X_END		19
#else
#define X_END		14
#endif
#if defined(hp300) || defined(luna68k)
#define	X_HPDINIT	(X_END)
	{ "_hp_dinit" },
#endif
#if defined(i386)
#define X_DK_NAMES	(X_END)
	{ "_dk_names" },
#endif
#ifdef mips
#define	X_SCSI_DINIT	(X_END)
	{ "_scsi_dinit" },
#endif
#ifdef tahoe
#define	X_VBDINIT	(X_END)
	{ "_vbdinit" },
#define	X_CKEYSTATS	(X_END+1)
	{ "_ckeystats" },
#define	X_DKEYSTATS	(X_END+2)
	{ "_dkeystats" },
#endif
#ifdef vax
#define X_MBDINIT	(X_END)
	{ "_mbdinit" },
#define X_UBDINIT	(X_END+1)
	{ "_ubdinit" },
#endif
	{ "" },
};

struct _disk {
	long time[CPUSTATES];
	long *xfer;
} cur, last;

struct	vmmeter sum, osum;
char	**dr_name;
int	*dr_select, dk_ndrive, ndrives;

int	winlines = 20;

kvm_t *kd;

#define	FORKSTAT	0x01
#define	INTRSTAT	0x02
#define	MEMSTAT		0x04
#define	SUMSTAT		0x08
#define	TIMESTAT	0x10
#define	VMSTAT		0x20

#include "names.c"			/* disk names -- machine dependent */

void	cpustats(), dkstats(), dointr(), domem(), dosum();
void	dovmstat(), kread(), usage();
#ifdef notyet
void	dotimes(), doforkst();
#endif
void printhdr __P((void));

int
main(argc, argv)
	register int argc;
	register char **argv;
{
	register int c, todo;
	u_int interval;
	int reps;
	char *memf, *nlistf;
        char errbuf[_POSIX2_LINE_MAX];

	memf = nlistf = NULL;
	interval = reps = todo = 0;
	while ((c = getopt(argc, argv, "c:fiM:mN:stw:")) != -1) {
		switch (c) {
		case 'c':
			reps = atoi(optarg);
			break;
		case 'f':
#ifdef notyet
			todo |= FORKSTAT;
#else
			errx(EX_USAGE, "sorry, -f is not (re)implemented yet");
#endif
			break;
		case 'i':
			todo |= INTRSTAT;
			break;
		case 'M':
			memf = optarg;
			break;
		case 'm':
			todo |= MEMSTAT;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 's':
			todo |= SUMSTAT;
			break;
		case 't':
#ifdef notyet
			todo |= TIMESTAT;
#else
			errx(EX_USAGE, "sorry, -t is not (re)implemented yet");
#endif
			break;
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

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (nlistf != NULL || memf != NULL)
		setgid(getgid());

	kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf);
	if (kd == 0) 
		errx(1, "kvm_openfiles: %s", errbuf);

	if ((c = kvm_nlist(kd, namelist)) != 0) {
		if (c > 0) {
			warnx("undefined symbols:");
			for (c = 0;
			    c < sizeof(namelist)/sizeof(namelist[0]); c++)
				if (namelist[c].n_type == 0)
					fprintf(stderr, " %s",
					    namelist[c].n_name);
			(void)fputc('\n', stderr);
		} else
			warnx("kvm_nlist: %s", kvm_geterr(kd));
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

#ifdef notyet
	if (todo & FORKSTAT)
		doforkst();
#endif
	if (todo & MEMSTAT)
		domem();
	if (todo & SUMSTAT)
		dosum();
#ifdef notyet
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
	if (dk_ndrive < 0)
		errx(1, "dk_ndrive %d", dk_ndrive);
	dr_select = calloc((size_t)dk_ndrive, sizeof(int));
	dr_name = calloc((size_t)dk_ndrive, sizeof(char *));
	for (i = 0; i < dk_ndrive; i++)
		dr_name[i] = NULL;
	cur.xfer = calloc((size_t)dk_ndrive, sizeof(long));
	last.xfer = calloc((size_t)dk_ndrive, sizeof(long));
	if (!read_names())
		exit (1);
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

long
getuptime()
{
	static time_t now, boottime;
	time_t uptime;

	if (boottime == 0)
		kread(X_BOOTTIME, &boottime, sizeof(boottime));
	(void)time(&now);
	uptime = now - boottime;
	if (uptime <= 0 || uptime > 60*60*24*365*10)
		errx(1, "time makes no sense; namelist must be wrong");
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
	int mib[2], size;

	uptime = getuptime();
	halfuptime = uptime / 2;
	(void)signal(SIGCONT, needhdr);

	if (namelist[X_STATHZ].n_type != 0 && namelist[X_STATHZ].n_value != 0)
		kread(X_STATHZ, &hz, sizeof(hz));
	if (!hz)
		kread(X_HZ, &hz, sizeof(hz));

	for (hdrcnt = 1;;) {
		if (!--hdrcnt)
			printhdr();
		kread(X_CPTIME, cur.time, sizeof(cur.time));
		kread(X_DKXFER, cur.xfer, sizeof(*cur.xfer) * dk_ndrive);
		kread(X_SUM, &sum, sizeof(sum));
		size = sizeof(total);
		mib[0] = CTL_VM;
		mib[1] = VM_METER;
		if (sysctl(mib, 2, &total, &size, NULL, 0) < 0) {
			printf("Can't get kerninfo: %s\n", strerror(errno));
			bzero(&total, sizeof(total));
		}
		(void)printf("%2d%2d%2d",
		    total.t_rq - 1, total.t_dw + total.t_pw, total.t_sw);
#define pgtok(a) ((a) * sum.v_page_size >> 10)
#define	rate(x)	(((x) + halfuptime) / uptime)	/* round */
		(void)printf("%8ld%6ld ",
		    (long)pgtok(total.t_avm), (long)pgtok(total.t_free));
		(void)printf("%4lu ", rate(sum.v_vm_faults - osum.v_vm_faults));
		(void)printf("%3lu ",
		    rate(sum.v_reactivated - osum.v_reactivated));
		(void)printf("%3lu ", rate(sum.v_swapin + sum.v_vnodein -
		    (osum.v_swapin + osum.v_vnodein)));
		(void)printf("%3lu ", rate(sum.v_swapout + sum.v_vnodeout -
		    (osum.v_swapout + osum.v_vnodeout)));
		(void)printf("%3lu ", rate(sum.v_tfree - osum.v_tfree));
		(void)printf("%3lu ", rate(sum.v_pdpages - osum.v_pdpages));
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
		uptime = interval;
		/*
		 * We round upward to avoid losing low-frequency events
		 * (i.e., >= 1 per interval but < 1 per second).
		 */
		if (interval != 1)
			halfuptime = (uptime + 1) / 2;
		else
			halfuptime = 0;
		(void)sleep(interval);
	}
}

void
printhdr()
{
	register int i;

	(void)printf(" procs      memory     page%*s", 20, "");
	if (ndrives > 1)
		(void)printf("disks %*s  faults      cpu\n",
		   ndrives * 3 - 6, "");
	else
		(void)printf("%*s  faults      cpu\n", ndrives * 3, "");
	(void)printf(" r b w     avm   fre  flt  re  pi  po  fr  sr ");
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

#ifdef notyet
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

long
pct(top, bot)
	long top, bot;
{
	long ans;

	if (bot == 0)
		return(0);
	ans = (quad_t)top * 100 / bot;
	return (ans);
}

#define	PCT(top, bot) pct((long)(top), (long)(bot))

#if defined(tahoe)
#include <machine/cpu.h>
#endif

void
dosum()
{
	struct nchstats nchstats;
	long nchtotal;
#if defined(tahoe)
	struct keystats keystats;
#endif

	kread(X_SUM, &sum, sizeof(sum));
	(void)printf("%9u cpu context switches\n", sum.v_swtch);
	(void)printf("%9u device interrupts\n", sum.v_intr);
	(void)printf("%9u software interrupts\n", sum.v_soft);
#ifdef vax
	(void)printf("%9u pseudo-dma dz interrupts\n", sum.v_pdma);
#endif
	(void)printf("%9u traps\n", sum.v_trap);
	(void)printf("%9u system calls\n", sum.v_syscall);
	(void)printf("%9u swap pager pageins\n", sum.v_swapin);
	(void)printf("%9u swap pager pages paged in\n", sum.v_swappgsin);
	(void)printf("%9u swap pager pageouts\n", sum.v_swapout);
	(void)printf("%9u swap pager pages paged out\n", sum.v_swappgsout);
	(void)printf("%9u vnode pager pageins\n", sum.v_vnodein);
	(void)printf("%9u vnode pager pages paged in\n", sum.v_vnodepgsin);
	(void)printf("%9u vnode pager pageouts\n", sum.v_vnodeout);
	(void)printf("%9u vnode pager pages paged out\n", sum.v_vnodepgsout);
	(void)printf("%9u page daemon wakeups\n", sum.v_pdwakeups);
	(void)printf("%9u pages examined by the page daemon\n", sum.v_pdpages);
	(void)printf("%9u pages reactivated\n", sum.v_reactivated);
	(void)printf("%9u copy-on-write faults\n", sum.v_cow_faults);
	(void)printf("%9u zero fill pages zeroed\n", sum.v_zfod);
	(void)printf("%9u intransit blocking page faults\n", sum.v_intrans);
	(void)printf("%9u total VM faults taken\n", sum.v_vm_faults);
	(void)printf("%9u pages freed\n", sum.v_tfree);
	(void)printf("%9u pages freed by daemon\n", sum.v_dfree);
	(void)printf("%9u pages freed by exiting processes\n", sum.v_pfree);
	(void)printf("%9u pages active\n", sum.v_active_count);
	(void)printf("%9u pages inactive\n", sum.v_inactive_count);
	(void)printf("%9u pages in VM cache\n", sum.v_cache_count);
	(void)printf("%9u pages wired down\n", sum.v_wire_count);
	(void)printf("%9u pages free\n", sum.v_free_count);
	(void)printf("%9u bytes per page\n", sum.v_page_size);
	kread(X_NCHSTATS, &nchstats, sizeof(nchstats));
	nchtotal = nchstats.ncs_goodhits + nchstats.ncs_neghits +
	    nchstats.ncs_badhits + nchstats.ncs_falsehits +
	    nchstats.ncs_miss + nchstats.ncs_long;
	(void)printf("%9ld total name lookups\n", nchtotal);
	(void)printf(
	    "%9s cache hits (%ld%% pos + %ld%% neg) system %ld%% per-directory\n",
	    "", PCT(nchstats.ncs_goodhits, nchtotal),
	    PCT(nchstats.ncs_neghits, nchtotal),
	    PCT(nchstats.ncs_pass2, nchtotal));
	(void)printf("%9s deletions %ld%%, falsehits %ld%%, toolong %ld%%\n", "",
	    PCT(nchstats.ncs_badhits, nchtotal),
	    PCT(nchstats.ncs_falsehits, nchtotal),
	    PCT(nchstats.ncs_long, nchtotal));
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

#ifdef notyet
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
	(void)printf("%2.0f ", (cur.time[CP_USER] + cur.time[CP_NICE]) * pct);
	(void)printf("%2.0f ", (cur.time[CP_SYS] + cur.time[CP_INTR]) * pct);
	(void)printf("%2.0f", cur.time[CP_IDLE] * pct);
}

void
dointr()
{
	register long *intrcnt, inttotal, uptime;
	register int nintr, inamlen;
	register char *intrname;

	uptime = getuptime();
	nintr = namelist[X_EINTRCNT].n_value - namelist[X_INTRCNT].n_value;
	inamlen =
	    namelist[X_EINTRNAMES].n_value - namelist[X_INTRNAMES].n_value;
	intrcnt = malloc((size_t)nintr);
	intrname = malloc((size_t)inamlen);
	if (intrcnt == NULL || intrname == NULL)
		errx(1, "malloc");
	kread(X_INTRCNT, intrcnt, (size_t)nintr);
	kread(X_INTRNAMES, intrname, (size_t)inamlen);
	(void)printf("interrupt      total      rate\n");
	inttotal = 0;
	nintr /= sizeof(long);
	while (--nintr >= 0) {
		if (*intrcnt)
			(void)printf("%-12s %8ld %8ld\n", intrname,
			    *intrcnt, *intrcnt / uptime);
		intrname += strlen(intrname) + 1;
		inttotal += *intrcnt++;
	}
	(void)printf("Total        %8ld %8ld\n", inttotal, inttotal / uptime);
}

void
domem()
{
	register struct kmembuckets *kp;
	register struct malloc_type *ks;
	register int i, j;
	int len, size, first, nkms;
	long totuse = 0, totfree = 0, totreq = 0;
	char *name;
	struct malloc_type kmemstats[200],*kmsp;
	char *kmemnames[200];
	char buf[1024];
	struct kmembuckets buckets[MINBUCKET + 16];

	kread(X_KMEMBUCKETS, buckets, sizeof(buckets));
	kread(X_KMEMSTATISTICS, &kmsp, sizeof(kmsp));
	for (nkms=0; nkms < 200 && kmsp; nkms++) {
		if (sizeof(kmemstats[0]) != kvm_read(kd, (u_long)kmsp,
		    &kmemstats[nkms], sizeof(kmemstats[0])))
			err(1,"kvm_read(%08x)", (u_long)kmsp);
		if (sizeof(buf) !=  kvm_read(kd, 
	            (u_long)kmemstats[nkms].ks_shortdesc, buf, sizeof(buf)))
			err(1,"kvm_read(%08x)", 
			    (u_long)kmemstats[nkms].ks_shortdesc);
		kmemstats[nkms].ks_shortdesc = strdup(buf);
		kmsp = kmemstats[nkms].ks_next;
	}
	(void)printf("Memory statistics by bucket size\n");
	(void)printf(
	    "Size   In Use   Free   Requests  HighWater  Couldfree\n");
	for (i = MINBUCKET, kp = &buckets[i]; i < MINBUCKET + 16; i++, kp++) {
		if (kp->kb_calls == 0)
			continue;
		size = 1 << i;
		if(size < 1024)
			(void)printf("%4d",size);
		else
			(void)printf("%3dK",size>>10);
		(void)printf(" %8ld %6ld %10ld %7ld %10ld\n",
			kp->kb_total - kp->kb_totalfree,
			kp->kb_totalfree, kp->kb_calls,
			kp->kb_highwat, kp->kb_couldfree);
		totfree += size * kp->kb_totalfree;
	}

	(void)printf("\nMemory usage type by bucket size\n");
	(void)printf("Size  Type(s)\n");
	kp = &buckets[MINBUCKET];
	for (j =  1 << MINBUCKET; j < 1 << (MINBUCKET + 16); j <<= 1, kp++) {
		if (kp->kb_calls == 0)
			continue;
		first = 1;
		len = 8;
		for (i = 0, ks = &kmemstats[0]; i < nkms; i++, ks++) {
			if (ks->ks_calls == 0)
				continue;
			if ((ks->ks_size & j) == 0)
				continue;
			name = ks->ks_shortdesc;
			len += 2 + strlen(name);
			if (first && j < 1024)
				printf("%4d  %s", j, name);
			else if (first)
				printf("%3dK  %s", j>>10, name);
			else
				printf(",");
			if (len >= 79) {
				printf("\n\t ");
				len = 10 + strlen(name);
			}
			if (!first)
				printf(" %s", name);
			first = 0;
		}
		printf("\n");
	}

	(void)printf(
	    "\nMemory statistics by type                          Type  Kern\n");
	(void)printf(
"        Type  InUse MemUse HighUse  Limit Requests Limit Limit Size(s)\n");
	for (i = 0, ks = &kmemstats[0]; i < nkms; i++, ks++) {
		if (ks->ks_calls == 0)
			continue;
		(void)printf("%13s%6ld%6ldK%7ldK%6ldK%9ld%5u%6u",
		    ks->ks_shortdesc,
		    ks->ks_inuse, (ks->ks_memuse + 1023) / 1024,
		    (ks->ks_maxused + 1023) / 1024,
		    (ks->ks_limit + 1023) / 1024, ks->ks_calls,
		    ks->ks_limblocks, ks->ks_mapblocks);
		first = 1;
		for (j =  1 << MINBUCKET; j < 1 << (MINBUCKET + 16); j <<= 1) {
			if ((ks->ks_size & j) == 0)
				continue;
			if (first)
				printf("  ");
			else
				printf(",");
			if(j<1024)
				printf("%d",j);
			else
				printf("%dK",j>>10);
			first = 0;
		}
		printf("\n");
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

	if (namelist[nlx].n_type == 0 || namelist[nlx].n_value == 0) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		errx(1, "symbol %s not defined", sym);
	}
	if (kvm_read(kd, namelist[nlx].n_value, addr, size) != size) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		errx(1, "%s: %s", sym, kvm_geterr(kd));
	}
}

void
usage()
{
	(void)fprintf(stderr,
"usage: vmstat [-ims] [-c count] [-M core] [-N system] [-w wait] [disks]\n");
	exit(1);
}
