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

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1986, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static const char sccsid[] = "@(#)vmstat.c	8.1 (Berkeley) 6/6/93";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/signal.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

#include <vm/vm_param.h>

#include <ctype.h>
#include <devstat.h>
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

static char da[] = "da";

static struct nlist namelist[] = {
#define	X_CPTIME	0
	{ "_cp_time" },
#define X_SUM		1
	{ "_cnt" },
#define	X_BOOTTIME	2
	{ "_boottime" },
#define X_HZ		3
	{ "_hz" },
#define X_STATHZ	4
	{ "_stathz" },
#define X_NCHSTATS	5
	{ "_nchstats" },
#define	X_INTRNAMES	6
	{ "_intrnames" },
#define	X_EINTRNAMES	7
	{ "_eintrnames" },
#define	X_INTRCNT	8
	{ "_intrcnt" },
#define	X_EINTRCNT	9
	{ "_eintrcnt" },
#ifdef notyet
#define	X_DEFICIT	10
	{ "_deficit" },
#define X_REC		11
	{ "_rectime" },
#define X_PGIN		12
	{ "_pgintime" },
#define	X_XSTATS	13
	{ "_xstats" },
#define X_END		14
#else
#define X_END		10
#endif
	{ "" },
};

static struct statinfo cur, last;
static int num_devices, maxshowdevs;
static long generation;
static struct device_selection *dev_select;
static int num_selected;
static struct devstat_match *matches;
static int num_matches = 0;
static int num_devices_specified, num_selections;
static long select_generation;
static char **specified_devices;
static devstat_select_mode select_mode;

static struct	vmmeter sum, osum;

static int	winlines = 20;
static int	aflag;
static int	nflag;

static kvm_t   *kd;

#define	FORKSTAT	0x01
#define	INTRSTAT	0x02
#define	MEMSTAT		0x04
#define	SUMSTAT		0x08
#define	TIMESTAT	0x10
#define	VMSTAT		0x20
#define ZMEMSTAT	0x40

static void	cpustats(void);
static void	devstats(void);
static void	doforkst(void);
static void	domem(void);
static void	dointr(void);
static void	dosum(void);
static void	dosysctl(const char *);
static void	dovmstat(unsigned int, int);
static void	dozmem(void);
static void	kread(int, void *, size_t);
static void	needhdr(int);
static void	printhdr(void);
static void	usage(void);

static long	pct(long, long);
static long	getuptime(void);

static char   **getdrivedata(char **);

int
main(int argc, char *argv[])
{
	int c, todo;
	unsigned int interval;
	int reps;
	char *memf, *nlistf;
	char errbuf[_POSIX2_LINE_MAX];

	memf = nlistf = NULL;
	interval = reps = todo = 0;
	maxshowdevs = 2;
	while ((c = getopt(argc, argv, "ac:fiM:mN:n:p:stw:z")) != -1) {
		switch (c) {
		case 'a':
			aflag++;
			break;
		case 'c':
			reps = atoi(optarg);
			break;
		case 'f':
			todo |= FORKSTAT;
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
		case 'n':
			nflag = 1;
			maxshowdevs = atoi(optarg);
			if (maxshowdevs < 0)
				errx(1, "number of devices %d is < 0",
				     maxshowdevs);
			break;
		case 'p':
			if (devstat_buildmatch(optarg, &matches, &num_matches) != 0)
				errx(1, "%s", devstat_errbuf);
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
		case 'z':
			todo |= ZMEMSTAT;
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

	if (memf != NULL) {
		kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf);
		if (kd == NULL)
			errx(1, "kvm_openfiles: %s", errbuf);
	}

	if (kd != NULL && (c = kvm_nlist(kd, namelist)) != 0) {
		if (c > 0) {
			warnx("undefined symbols:");
			for (c = 0;
			     c < (int)(sizeof(namelist)/sizeof(namelist[0]));
			     c++)
				if (namelist[c].n_type == 0)
					(void)fprintf(stderr, " %s",
					    namelist[c].n_name);
			(void)fputc('\n', stderr);
		} else
			warnx("kvm_nlist: %s", kvm_geterr(kd));
		exit(1);
	}

	if (todo & VMSTAT) {
		struct winsize winsize;

		/*
		 * Make sure that the userland devstat version matches the
		 * kernel devstat version.  If not, exit and print a
		 * message informing the user of his mistake.
		 */
		if (devstat_checkversion(NULL) < 0)
			errx(1, "%s", devstat_errbuf);


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

	if (todo & FORKSTAT)
		doforkst();
	if (todo & MEMSTAT)
		domem();
	if (todo & ZMEMSTAT)
		dozmem();
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

static int
mysysctl(const char *name, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	int error;

	error = sysctlbyname(name, oldp, oldlenp, newp, newlen);
	if (error != 0 && errno != ENOMEM)
		err(1, "sysctl(%s)", name);
	return (error);
}

static char **
getdrivedata(char **argv)
{
	if ((num_devices = devstat_getnumdevs(NULL)) < 0)
		errx(1, "%s", devstat_errbuf);

	cur.dinfo = (struct devinfo *)malloc(sizeof(struct devinfo));
	last.dinfo = (struct devinfo *)malloc(sizeof(struct devinfo));
	bzero(cur.dinfo, sizeof(struct devinfo));
	bzero(last.dinfo, sizeof(struct devinfo));

	if (devstat_getdevs(NULL, &cur) == -1)
		errx(1, "%s", devstat_errbuf);

	num_devices = cur.dinfo->numdevs;
	generation = cur.dinfo->generation;

	specified_devices = (char **)malloc(sizeof(char *));
	for (num_devices_specified = 0; *argv; ++argv) {
		if (isdigit(**argv))
			break;
		num_devices_specified++;
		specified_devices = (char **)realloc(specified_devices,
						     sizeof(char *) *
						     num_devices_specified);
		specified_devices[num_devices_specified - 1] = *argv;
	}
	dev_select = NULL;

	if (nflag == 0 && maxshowdevs < num_devices_specified)
			maxshowdevs = num_devices_specified;

	/*
	 * People are generally only interested in disk statistics when
	 * they're running vmstat.  So, that's what we're going to give
	 * them if they don't specify anything by default.  We'll also give
	 * them any other random devices in the system so that we get to
	 * maxshowdevs devices, if that many devices exist.  If the user
	 * specifies devices on the command line, either through a pattern
	 * match or by naming them explicitly, we will give the user only
	 * those devices.
	 */
	if ((num_devices_specified == 0) && (num_matches == 0)) {
		if (devstat_buildmatch(da, &matches, &num_matches) != 0)
			errx(1, "%s", devstat_errbuf);

		select_mode = DS_SELECT_ADD;
	} else
		select_mode = DS_SELECT_ONLY;

	/*
	 * At this point, selectdevs will almost surely indicate that the
	 * device list has changed, so we don't look for return values of 0
	 * or 1.  If we get back -1, though, there is an error.
	 */
	if (devstat_selectdevs(&dev_select, &num_selected, &num_selections,
		       &select_generation, generation, cur.dinfo->devices,
		       num_devices, matches, num_matches, specified_devices,
		       num_devices_specified, select_mode,
		       maxshowdevs, 0) == -1)
		errx(1, "%s", devstat_errbuf);

	return(argv);
}

static long
getuptime(void)
{
	static struct timeval boottime;
	static time_t now;
	time_t uptime;

	if (boottime.tv_sec == 0) {
		if (kd != NULL) {
			kread(X_BOOTTIME, &boottime, sizeof(boottime));
		} else {
			size_t size;

			size = sizeof(boottime);
			mysysctl("kern.boottime", &boottime, &size, NULL, 0);
			if (size != sizeof(boottime))
				errx(1, "kern.boottime size mismatch");
		}
	}
	(void)time(&now);
	uptime = now - boottime.tv_sec;
	if (uptime <= 0 || uptime > 60*60*24*365*10)
		errx(1, "time makes no sense; namelist must be wrong");
	return(uptime);
}

static void
fill_vmmeter(struct vmmeter *vmmp)
{
	if (kd != NULL) {
		kread(X_SUM, vmmp, sizeof(*vmmp));
	} else {
		size_t size = sizeof(unsigned int);
#define GET_VM_STATS(cat, name) \
	mysysctl("vm.stats." #cat "." #name, &vmmp->name, &size, NULL, 0)
		/* sys */
		GET_VM_STATS(sys, v_swtch);
		GET_VM_STATS(sys, v_trap);
		GET_VM_STATS(sys, v_syscall);
		GET_VM_STATS(sys, v_intr);
		GET_VM_STATS(sys, v_soft);

		/* vm */
                GET_VM_STATS(vm, v_vm_faults);
                GET_VM_STATS(vm, v_cow_faults);
                GET_VM_STATS(vm, v_cow_optim);
                GET_VM_STATS(vm, v_zfod);
                GET_VM_STATS(vm, v_ozfod);
                GET_VM_STATS(vm, v_swapin);
                GET_VM_STATS(vm, v_swapout);
                GET_VM_STATS(vm, v_swappgsin);
                GET_VM_STATS(vm, v_swappgsout);
                GET_VM_STATS(vm, v_vnodein);
                GET_VM_STATS(vm, v_vnodeout);
                GET_VM_STATS(vm, v_vnodepgsin);
                GET_VM_STATS(vm, v_vnodepgsout);
                GET_VM_STATS(vm, v_intrans);
                GET_VM_STATS(vm, v_reactivated);
                GET_VM_STATS(vm, v_pdwakeups);
                GET_VM_STATS(vm, v_pdpages);
                GET_VM_STATS(vm, v_dfree);
                GET_VM_STATS(vm, v_pfree);
                GET_VM_STATS(vm, v_tfree);
                GET_VM_STATS(vm, v_page_size);
                GET_VM_STATS(vm, v_page_count);
                GET_VM_STATS(vm, v_free_reserved);
                GET_VM_STATS(vm, v_free_target);
                GET_VM_STATS(vm, v_free_min);
                GET_VM_STATS(vm, v_free_count);
                GET_VM_STATS(vm, v_wire_count);
                GET_VM_STATS(vm, v_active_count);
                GET_VM_STATS(vm, v_inactive_target);
                GET_VM_STATS(vm, v_inactive_count);
                GET_VM_STATS(vm, v_cache_count);
                GET_VM_STATS(vm, v_cache_min);
                GET_VM_STATS(vm, v_cache_max);
                GET_VM_STATS(vm, v_pageout_free_min);
                GET_VM_STATS(vm, v_interrupt_free_min);
                /*GET_VM_STATS(vm, v_free_severe);*/
		GET_VM_STATS(vm, v_forks);
		GET_VM_STATS(vm, v_vforks);
		GET_VM_STATS(vm, v_rforks);
		GET_VM_STATS(vm, v_kthreads);
		GET_VM_STATS(vm, v_forkpages);
		GET_VM_STATS(vm, v_vforkpages);
		GET_VM_STATS(vm, v_rforkpages);
		GET_VM_STATS(vm, v_kthreadpages);
#undef GET_VM_STATS
	}
}

static void
fill_vmtotal(struct vmtotal *vmtp)
{
	if (kd != NULL) {
		/* XXX fill vmtp */
		errx(1, "not implemented");
	} else {
		size_t size = sizeof(*vmtp);
		mysysctl("vm.vmtotal", vmtp, &size, NULL, 0);
		if (size != sizeof(*vmtp))
			errx(1, "vm.total size mismatch");
	}
}

static int hz, hdrcnt;

static void
dovmstat(unsigned int interval, int reps)
{
	struct vmtotal total;
	time_t uptime, halfuptime;
	struct devinfo *tmp_dinfo;
	size_t size;

	uptime = getuptime();
	halfuptime = uptime / 2;
	(void)signal(SIGCONT, needhdr);

	if (kd != NULL) {
		if (namelist[X_STATHZ].n_type != 0 &&
		    namelist[X_STATHZ].n_value != 0)
			kread(X_STATHZ, &hz, sizeof(hz));
		if (!hz)
			kread(X_HZ, &hz, sizeof(hz));
	} else {
		struct clockinfo clockrate;

		size = sizeof(clockrate);
		mysysctl("kern.clockrate", &clockrate, &size, NULL, 0);
		if (size != sizeof(clockrate))
			errx(1, "clockrate size mismatch");
		hz = clockrate.hz;
	}

	for (hdrcnt = 1;;) {
		if (!--hdrcnt)
			printhdr();
		if (kd != NULL) {
			kread(X_CPTIME, cur.cp_time, sizeof(cur.cp_time));
		} else {
			size = sizeof(cur.cp_time);
			mysysctl("kern.cp_time", &cur.cp_time, &size, NULL, 0);
			if (size != sizeof(cur.cp_time))
				errx(1, "cp_time size mismatch");
		}

		tmp_dinfo = last.dinfo;
		last.dinfo = cur.dinfo;
		cur.dinfo = tmp_dinfo;
		last.snap_time = cur.snap_time;

		/*
		 * Here what we want to do is refresh our device stats.
		 * getdevs() returns 1 when the device list has changed.
		 * If the device list has changed, we want to go through
		 * the selection process again, in case a device that we
		 * were previously displaying has gone away.
		 */
		switch (devstat_getdevs(NULL, &cur)) {
		case -1:
			errx(1, "%s", devstat_errbuf);
			break;
		case 1: {
			int retval;

			num_devices = cur.dinfo->numdevs;
			generation = cur.dinfo->generation;

			retval = devstat_selectdevs(&dev_select, &num_selected,
					    &num_selections, &select_generation,
					    generation, cur.dinfo->devices,
					    num_devices, matches, num_matches,
					    specified_devices,
					    num_devices_specified, select_mode,
					    maxshowdevs, 0);
			switch (retval) {
			case -1:
				errx(1, "%s", devstat_errbuf);
				break;
			case 1:
				printhdr();
				break;
			default:
				break;
			}
		}
		default:
			break;
		}

		fill_vmmeter(&sum);
		fill_vmtotal(&total);
		(void)printf("%2d %1d %1d",
		    total.t_rq - 1, total.t_dw + total.t_pw, total.t_sw);
#define vmstat_pgtok(a) ((a) * sum.v_page_size >> 10)
#define	rate(x)	(((x) + halfuptime) / uptime)	/* round */
		(void)printf(" %7ld %6ld ", (long)vmstat_pgtok(total.t_avm),
			     (long)vmstat_pgtok(total.t_free));
		(void)printf("%4lu ",
		    (unsigned long)rate(sum.v_vm_faults - osum.v_vm_faults));
		(void)printf("%3lu ",
		    (unsigned long)rate(sum.v_reactivated - osum.v_reactivated));
		(void)printf("%3lu ",
		    (unsigned long)rate(sum.v_swapin + sum.v_vnodein -
		    (osum.v_swapin + osum.v_vnodein)));
		(void)printf("%3lu ",
		    (unsigned long)rate(sum.v_swapout + sum.v_vnodeout -
		    (osum.v_swapout + osum.v_vnodeout)));
		(void)printf("%3lu ",
		    (unsigned long)rate(sum.v_tfree - osum.v_tfree));
		(void)printf("%3lu ",
		    (unsigned long)rate(sum.v_pdpages - osum.v_pdpages));
		devstats();
		(void)printf("%4lu %4lu %3lu ",
		    (unsigned long)rate(sum.v_intr - osum.v_intr),
		    (unsigned long)rate(sum.v_syscall - osum.v_syscall),
		    (unsigned long)rate(sum.v_swtch - osum.v_swtch));
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

static void
printhdr(void)
{
	int i, num_shown;

	num_shown = (num_selected < maxshowdevs) ? num_selected : maxshowdevs;
	(void)printf(" procs      memory      page%*s", 19, "");
	if (num_shown > 1)
		(void)printf(" disks %*s", num_shown * 4 - 7, "");
	else if (num_shown == 1)
		(void)printf("disk");
	(void)printf("   faults      cpu\n");
	(void)printf(" r b w     avm    fre  flt  re  pi  po  fr  sr ");
	for (i = 0; i < num_devices; i++)
		if ((dev_select[i].selected)
		 && (dev_select[i].selected <= maxshowdevs))
			(void)printf("%c%c%d ", dev_select[i].device_name[0],
				     dev_select[i].device_name[1],
				     dev_select[i].unit_number);
	(void)printf("  in   sy  cs us sy id\n");
	hdrcnt = winlines - 2;
}

/*
 * Force a header to be prepended to the next output.
 */
static void
needhdr(int dummy __unused)
{

	hdrcnt = 1;
}

#ifdef notyet
static void
dotimes(void)
{
	unsigned int pgintime, rectime;

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

static long
pct(long top, long bot)
{
	long ans;

	if (bot == 0)
		return(0);
	ans = (quad_t)top * 100 / bot;
	return (ans);
}

#define	PCT(top, bot) pct((long)(top), (long)(bot))

static void
dosum(void)
{
	struct nchstats lnchstats;
	long nchtotal;

	fill_vmmeter(&sum);
	(void)printf("%9u cpu context switches\n", sum.v_swtch);
	(void)printf("%9u device interrupts\n", sum.v_intr);
	(void)printf("%9u software interrupts\n", sum.v_soft);
	(void)printf("%9u traps\n", sum.v_trap);
	(void)printf("%9u system calls\n", sum.v_syscall);
	(void)printf("%9u kernel threads created\n", sum.v_kthreads);
	(void)printf("%9u  fork() calls\n", sum.v_forks);
	(void)printf("%9u vfork() calls\n", sum.v_vforks);
	(void)printf("%9u rfork() calls\n", sum.v_rforks);
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
	(void)printf("%9u copy-on-write optimized faults\n", sum.v_cow_optim);
	(void)printf("%9u zero fill pages zeroed\n", sum.v_zfod);
	(void)printf("%9u zero fill pages prezeroed\n", sum.v_ozfod);
	(void)printf("%9u intransit blocking page faults\n", sum.v_intrans);
	(void)printf("%9u total VM faults taken\n", sum.v_vm_faults);
	(void)printf("%9u pages affected by kernel thread creation\n", sum.v_kthreadpages);
	(void)printf("%9u pages affected by  fork()\n", sum.v_forkpages);
	(void)printf("%9u pages affected by vfork()\n", sum.v_vforkpages);
	(void)printf("%9u pages affected by rfork()\n", sum.v_rforkpages);
	(void)printf("%9u pages freed\n", sum.v_tfree);
	(void)printf("%9u pages freed by daemon\n", sum.v_dfree);
	(void)printf("%9u pages freed by exiting processes\n", sum.v_pfree);
	(void)printf("%9u pages active\n", sum.v_active_count);
	(void)printf("%9u pages inactive\n", sum.v_inactive_count);
	(void)printf("%9u pages in VM cache\n", sum.v_cache_count);
	(void)printf("%9u pages wired down\n", sum.v_wire_count);
	(void)printf("%9u pages free\n", sum.v_free_count);
	(void)printf("%9u bytes per page\n", sum.v_page_size);
	if (kd != NULL) {
		kread(X_NCHSTATS, &lnchstats, sizeof(lnchstats));
	} else {
		size_t size = sizeof(lnchstats);
		mysysctl("vfs.cache.nchstats", &lnchstats, &size, NULL, 0);
		if (size != sizeof(lnchstats))
			errx(1, "vfs.cache.nchstats size mismatch");
	}
	nchtotal = lnchstats.ncs_goodhits + lnchstats.ncs_neghits +
	    lnchstats.ncs_badhits + lnchstats.ncs_falsehits +
	    lnchstats.ncs_miss + lnchstats.ncs_long;
	(void)printf("%9ld total name lookups\n", nchtotal);
	(void)printf(
	    "%9s cache hits (%ld%% pos + %ld%% neg) system %ld%% per-directory\n",
	    "", PCT(lnchstats.ncs_goodhits, nchtotal),
	    PCT(lnchstats.ncs_neghits, nchtotal),
	    PCT(lnchstats.ncs_pass2, nchtotal));
	(void)printf("%9s deletions %ld%%, falsehits %ld%%, toolong %ld%%\n", "",
	    PCT(lnchstats.ncs_badhits, nchtotal),
	    PCT(lnchstats.ncs_falsehits, nchtotal),
	    PCT(lnchstats.ncs_long, nchtotal));
}

static void
doforkst(void)
{
	fill_vmmeter(&sum);
	(void)printf("%d forks, %d pages, average %.2f\n",
	    sum.v_forks, sum.v_forkpages,
	    sum.v_forks == 0 ? 0.0 :
	    (double)sum.v_forkpages / sum.v_forks);
	(void)printf("%d vforks, %d pages, average %.2f\n",
	    sum.v_vforks, sum.v_vforkpages,
	    sum.v_vforks == 0 ? 0.0 :
	    (double)sum.v_vforkpages / sum.v_vforks);
	(void)printf("%d rforks, %d pages, average %.2f\n",
	    sum.v_rforks, sum.v_rforkpages,
	    sum.v_rforks == 0 ? 0.0 :
	    (double)sum.v_rforkpages / sum.v_rforks);
}

static void
devstats(void)
{
	int dn, state;
	long double transfers_per_second;
	long double busy_seconds;
	long tmp;

	for (state = 0; state < CPUSTATES; ++state) {
		tmp = cur.cp_time[state];
		cur.cp_time[state] -= last.cp_time[state];
		last.cp_time[state] = tmp;
	}

	busy_seconds = cur.snap_time - last.snap_time;

	for (dn = 0; dn < num_devices; dn++) {
		int di;

		if ((dev_select[dn].selected == 0)
		 || (dev_select[dn].selected > maxshowdevs))
			continue;

		di = dev_select[dn].position;

		if (devstat_compute_statistics(&cur.dinfo->devices[di],
		    &last.dinfo->devices[di], busy_seconds,
		    DSM_TRANSFERS_PER_SECOND, &transfers_per_second,
		    DSM_NONE) != 0)
			errx(1, "%s", devstat_errbuf);

		(void)printf("%3.0Lf ", transfers_per_second);
	}
}

static void
cpustats(void)
{
	int state;
	double lpct, total;

	total = 0;
	for (state = 0; state < CPUSTATES; ++state)
		total += cur.cp_time[state];
	if (total)
		lpct = 100.0 / total;
	else
		lpct = 0.0;
	(void)printf("%2.0f ", (cur.cp_time[CP_USER] +
				cur.cp_time[CP_NICE]) * lpct);
	(void)printf("%2.0f ", (cur.cp_time[CP_SYS] +
				cur.cp_time[CP_INTR]) * lpct);
	(void)printf("%2.0f", cur.cp_time[CP_IDLE] * lpct);
}

static void
dointr(void)
{
	unsigned long *intrcnt, uptime;
	uint64_t inttotal;
	size_t clen, inamlen, intrcntlen, istrnamlen;
	unsigned int i, nintr;
	char *intrname, *tintrname;

	uptime = getuptime();
	if (kd != NULL) {
		intrcntlen = namelist[X_EINTRCNT].n_value -
		    namelist[X_INTRCNT].n_value;
		inamlen = namelist[X_EINTRNAMES].n_value -
		    namelist[X_INTRNAMES].n_value;
		if ((intrcnt = malloc(intrcntlen)) == NULL ||
		    (intrname = malloc(inamlen)) == NULL)
			err(1, "malloc()");
		kread(X_INTRCNT, intrcnt, intrcntlen);
		kread(X_INTRNAMES, intrname, inamlen);
	} else {
		mysysctl("hw.intrcnt", NULL, &intrcntlen, NULL, 0);
		fprintf(stderr, "intrcntlen = %lu\n", (unsigned long)intrcntlen);
		if ((intrcnt = malloc(intrcntlen)) == NULL)
			err(1, "calloc()");
		mysysctl("hw.intrcnt", intrcnt, &intrcntlen, NULL, 0);
		for (intrname = NULL, inamlen = 1024; ; inamlen *= 2) {
			if ((intrname = reallocf(intrname, inamlen)) == NULL)
				err(1, "reallocf()");
			if (mysysctl("hw.intrnames",
			    intrname, &inamlen, NULL, 0) == 0)
				break;
		}
	}
	nintr = intrcntlen / sizeof(unsigned long);
	tintrname = intrname;
	istrnamlen = strlen("interrupt");
	for (i = 0; i < nintr; i++) {
		clen = strlen(tintrname);
		if (clen > istrnamlen)
			istrnamlen = clen;
		tintrname += clen + 1;
	}
	(void)printf("%-*s %20s %10s\n", istrnamlen, "interrupt", "total",
	    "rate");
	inttotal = 0;
	for (i = 0; i < nintr; i++) {
		const char *p;
		if (intrname[0] != '\0' && (*intrcnt != 0 || aflag))
			(void)printf("%-*s %20lu %10lu\n", istrnamlen, intrname,
			    *intrcnt, *intrcnt / uptime);
		intrname += strlen(intrname) + 1;
		inttotal += *intrcnt++;
	}
	(void)printf("%-*s %20llu %10llu\n", istrnamlen, "Total",
	    (long long)inttotal, (long long)(inttotal / uptime));
}

static void
domem(void)
{
	if (kd != NULL)
		errx(1, "not implemented");
	dosysctl("kern.malloc");
}

static void
dozmem(void)
{
	if (kd != NULL)
		errx(1, "not implemented");
	dosysctl("vm.zone");
}

static void
dosysctl(const char *name)
{
	char *buf;
	size_t bufsize;

	for (buf = NULL, bufsize = 1024; ; bufsize *= 2) {
		if ((buf = realloc(buf, bufsize)) == NULL)
			err(1, "realloc()");
		if (mysysctl(name, buf, &bufsize, 0, NULL) == 0)
			break;
		bufsize *= 2;
	}
	buf[bufsize] = '\0'; /* play it safe */
	(void)printf("%s\n\n", buf);
	free(buf);
}

/*
 * kread reads something from the kernel, given its nlist index.
 */
static void
kread(int nlx, void *addr, size_t size)
{
	char *sym;

	if (namelist[nlx].n_type == 0 || namelist[nlx].n_value == 0) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		errx(1, "symbol %s not defined", sym);
	}
	if ((size_t)kvm_read(kd, namelist[nlx].n_value, addr, size) != size) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		errx(1, "%s: %s", sym, kvm_geterr(kd));
	}
}

static void
usage(void)
{
	(void)fprintf(stderr, "%s%s",
		"usage: vmstat [-aimsz] [-c count] [-M core] [-N system] [-w wait]\n",
		"              [-n devs] [disks]\n");
	exit(1);
}
