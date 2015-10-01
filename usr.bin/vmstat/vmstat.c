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

#if 0
#ifndef lint
static char sccsid[] = "@(#)vmstat.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/signal.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/user.h>
#include <sys/vmmeter.h>
#include <sys/pcpu.h>

#include <vm/vm_param.h>

#include <ctype.h>
#include <devstat.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <kvm.h>
#include <limits.h>
#include <memstat.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>
#include <libutil.h>

static char da[] = "da";

static struct nlist namelist[] = {
#define X_SUM		0
	{ "_cnt" },
#define X_HZ		1
	{ "_hz" },
#define X_STATHZ	2
	{ "_stathz" },
#define X_NCHSTATS	3
	{ "_nchstats" },
#define	X_INTRNAMES	4
	{ "_intrnames" },
#define	X_SINTRNAMES	5
	{ "_sintrnames" },
#define	X_INTRCNT	6
	{ "_intrcnt" },
#define	X_SINTRCNT	7
	{ "_sintrcnt" },
#ifdef notyet
#define	X_DEFICIT	XXX
	{ "_deficit" },
#define X_REC		XXX
	{ "_rectime" },
#define X_PGIN		XXX
	{ "_pgintime" },
#define	X_XSTATS	XXX
	{ "_xstats" },
#define X_END		XXX
#else
#define X_END		8
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

#define	VMSTAT_DEFAULT_LINES	20	/* Default number of `winlines'. */
volatile sig_atomic_t wresized;		/* Tty resized, when non-zero. */
static int winlines = VMSTAT_DEFAULT_LINES; /* Current number of tty rows. */

static int	aflag;
static int	nflag;
static int	Pflag;
static int	hflag;

static kvm_t   *kd;

#define	FORKSTAT	0x01
#define	INTRSTAT	0x02
#define	MEMSTAT		0x04
#define	SUMSTAT		0x08
#define	TIMESTAT	0x10
#define	VMSTAT		0x20
#define ZMEMSTAT	0x40
#define	OBJSTAT		0x80

static void	cpustats(void);
static void	pcpustats(int, u_long, int);
static void	devstats(void);
static void	doforkst(void);
static void	dointr(void);
static void	doobjstat(void);
static void	dosum(void);
static void	dovmstat(unsigned int, int);
static void	domemstat_malloc(void);
static void	domemstat_zone(void);
static void	kread(int, void *, size_t);
static void	kreado(int, void *, size_t, size_t);
static char    *kgetstr(const char *);
static void	needhdr(int);
static void	needresize(int);
static void	doresize(void);
static void	printhdr(int, u_long);
static void	usage(void);

static long	pct(long, long);
static long	getuptime(void);

static char   **getdrivedata(char **);

int
main(int argc, char *argv[])
{
	int c, todo;
	unsigned int interval;
	float f;
	int reps;
	char *memf, *nlistf;
	char errbuf[_POSIX2_LINE_MAX];

	memf = nlistf = NULL;
	interval = reps = todo = 0;
	maxshowdevs = 2;
	hflag = isatty(1);
	while ((c = getopt(argc, argv, "ac:fhHiM:mN:n:oPp:stw:z")) != -1) {
		switch (c) {
		case 'a':
			aflag++;
			break;
		case 'c':
			reps = atoi(optarg);
			break;
		case 'P':
			Pflag++;
			break;
		case 'f':
			todo |= FORKSTAT;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'H':
			hflag = 0;
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
		case 'o':
			todo |= OBJSTAT;
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
			/* Convert to milliseconds. */
			f = atof(optarg);
			interval = f * 1000;
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
	if (kd && Pflag)
		errx(1, "Cannot use -P with crash dumps");

	if (todo & VMSTAT) {
		/*
		 * Make sure that the userland devstat version matches the
		 * kernel devstat version.  If not, exit and print a
		 * message informing the user of his mistake.
		 */
		if (devstat_checkversion(NULL) < 0)
			errx(1, "%s", devstat_errbuf);


		argv = getdrivedata(argv);
	}

	if (*argv) {
		f = atof(*argv);
		interval = f * 1000;
		if (*++argv)
			reps = atoi(*argv);
	}

	if (interval) {
		if (!reps)
			reps = -1;
	} else if (reps)
		interval = 1 * 1000;

	if (todo & FORKSTAT)
		doforkst();
	if (todo & MEMSTAT)
		domemstat_malloc();
	if (todo & ZMEMSTAT)
		domemstat_zone();
	if (todo & SUMSTAT)
		dosum();
	if (todo & OBJSTAT)
		doobjstat();
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

	cur.dinfo = (struct devinfo *)calloc(1, sizeof(struct devinfo));
	last.dinfo = (struct devinfo *)calloc(1, sizeof(struct devinfo));

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
	struct timespec sp;

	(void)clock_gettime(CLOCK_MONOTONIC, &sp);

	return(sp.tv_sec);
}

static void
fill_pcpu(struct pcpu ***pcpup, int* maxcpup)
{
	struct pcpu **pcpu;
	
	int maxcpu, i;

	*pcpup = NULL;
	
	if (kd == NULL)
		return;

	maxcpu = kvm_getmaxcpu(kd);
	if (maxcpu < 0)
		errx(1, "kvm_getmaxcpu: %s", kvm_geterr(kd));

	pcpu = calloc(maxcpu, sizeof(struct pcpu *));
	if (pcpu == NULL)
		err(1, "calloc");

	for (i = 0; i < maxcpu; i++) {
		pcpu[i] = kvm_getpcpu(kd, i);
		if (pcpu[i] == (struct pcpu *)-1)
			errx(1, "kvm_getpcpu: %s", kvm_geterr(kd));
	}

	*maxcpup = maxcpu;
	*pcpup = pcpu;
}

static void
free_pcpu(struct pcpu **pcpu, int maxcpu)
{
	int i;

	for (i = 0; i < maxcpu; i++)
		free(pcpu[i]);
	free(pcpu);
}

static void
fill_vmmeter(struct vmmeter *vmmp)
{
	struct pcpu **pcpu;
	int maxcpu, i;

	if (kd != NULL) {
		kread(X_SUM, vmmp, sizeof(*vmmp));
		fill_pcpu(&pcpu, &maxcpu);
		for (i = 0; i < maxcpu; i++) {
			if (pcpu[i] == NULL)
				continue;
#define ADD_FROM_PCPU(i, name) \
			vmmp->name += pcpu[i]->pc_cnt.name
			ADD_FROM_PCPU(i, v_swtch);
			ADD_FROM_PCPU(i, v_trap);
			ADD_FROM_PCPU(i, v_syscall);
			ADD_FROM_PCPU(i, v_intr);
			ADD_FROM_PCPU(i, v_soft);
			ADD_FROM_PCPU(i, v_vm_faults);
			ADD_FROM_PCPU(i, v_io_faults);
			ADD_FROM_PCPU(i, v_cow_faults);
			ADD_FROM_PCPU(i, v_cow_optim);
			ADD_FROM_PCPU(i, v_zfod);
			ADD_FROM_PCPU(i, v_ozfod);
			ADD_FROM_PCPU(i, v_swapin);
			ADD_FROM_PCPU(i, v_swapout);
			ADD_FROM_PCPU(i, v_swappgsin);
			ADD_FROM_PCPU(i, v_swappgsout);
			ADD_FROM_PCPU(i, v_vnodein);
			ADD_FROM_PCPU(i, v_vnodeout);
			ADD_FROM_PCPU(i, v_vnodepgsin);
			ADD_FROM_PCPU(i, v_vnodepgsout);
			ADD_FROM_PCPU(i, v_intrans);
			ADD_FROM_PCPU(i, v_tfree);
			ADD_FROM_PCPU(i, v_forks);
			ADD_FROM_PCPU(i, v_vforks);
			ADD_FROM_PCPU(i, v_rforks);
			ADD_FROM_PCPU(i, v_kthreads);
			ADD_FROM_PCPU(i, v_forkpages);
			ADD_FROM_PCPU(i, v_vforkpages);
			ADD_FROM_PCPU(i, v_rforkpages);
			ADD_FROM_PCPU(i, v_kthreadpages);
#undef ADD_FROM_PCPU
		}
		free_pcpu(pcpu, maxcpu);
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
		GET_VM_STATS(vm, v_io_faults);
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
		GET_VM_STATS(vm, v_tcached);
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

/* Determine how many cpu columns, and what index they are in kern.cp_times */
static int
getcpuinfo(u_long *maskp, int *maxidp)
{
	int maxcpu;
	int maxid;
	int ncpus;
	int i, j;
	int empty;
	size_t size;
	long *times;
	u_long mask;

	if (kd != NULL)
		errx(1, "not implemented");
	mask = 0;
	ncpus = 0;
	size = sizeof(maxcpu);
	mysysctl("kern.smp.maxcpus", &maxcpu, &size, NULL, 0);
	if (size != sizeof(maxcpu))
		errx(1, "sysctl kern.smp.maxcpus");
	size = sizeof(long) * maxcpu * CPUSTATES;
	times = malloc(size);
	if (times == NULL)
		err(1, "malloc %zd bytes", size);
	mysysctl("kern.cp_times", times, &size, NULL, 0);
	maxid = (size / CPUSTATES / sizeof(long)) - 1;
	for (i = 0; i <= maxid; i++) {
		empty = 1;
		for (j = 0; empty && j < CPUSTATES; j++) {
			if (times[i * CPUSTATES + j] != 0)
				empty = 0;
		}
		if (!empty) {
			mask |= (1ul << i);
			ncpus++;
		}
	}
	if (maskp)
		*maskp = mask;
	if (maxidp)
		*maxidp = maxid;
	return (ncpus);
}


static void
prthuman(u_int64_t val, int size)
{
	char buf[10];
	int flags;

	if (size < 5 || size > 9)
		errx(1, "doofus");
	flags = HN_B | HN_NOSPACE | HN_DECIMAL;
	humanize_number(buf, size, val, "", HN_AUTOSCALE, flags);
	printf("%*s", size, buf);
}

static int hz, hdrcnt;

static long *cur_cp_times;
static long *last_cp_times;
static size_t size_cp_times;

static void
dovmstat(unsigned int interval, int reps)
{
	struct vmtotal total;
	time_t uptime, halfuptime;
	struct devinfo *tmp_dinfo;
	size_t size;
	int ncpus, maxid;
	u_long cpumask;
	int rate_adj;

	uptime = getuptime();
	halfuptime = uptime / 2;
	rate_adj = 1;
	ncpus = 1;
	maxid = 0;

	/*
	 * If the user stops the program (control-Z) and then resumes it,
	 * print out the header again.
	 */
	(void)signal(SIGCONT, needhdr);

	/*
	 * If our standard output is a tty, then install a SIGWINCH handler
	 * and set wresized so that our first iteration through the main
	 * vmstat loop will peek at the terminal's current rows to find out
	 * how many lines can fit in a screenful of output.
	 */
	if (isatty(fileno(stdout)) != 0) {
		wresized = 1;
		(void)signal(SIGWINCH, needresize);
	} else {
		wresized = 0;
		winlines = VMSTAT_DEFAULT_LINES;
	}

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

	if (Pflag) {
		ncpus = getcpuinfo(&cpumask, &maxid);
		size_cp_times = sizeof(long) * (maxid + 1) * CPUSTATES;
		cur_cp_times = calloc(1, size_cp_times);
		last_cp_times = calloc(1, size_cp_times);
	}
	for (hdrcnt = 1;;) {
		if (!--hdrcnt)
			printhdr(maxid, cpumask);
		if (kd != NULL) {
			if (kvm_getcptime(kd, cur.cp_time) < 0)
				errx(1, "kvm_getcptime: %s", kvm_geterr(kd));
		} else {
			size = sizeof(cur.cp_time);
			mysysctl("kern.cp_time", &cur.cp_time, &size, NULL, 0);
			if (size != sizeof(cur.cp_time))
				errx(1, "cp_time size mismatch");
		}
		if (Pflag) {
			size = size_cp_times;
			mysysctl("kern.cp_times", cur_cp_times, &size, NULL, 0);
			if (size != size_cp_times)
				errx(1, "cp_times mismatch");
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
				printhdr(maxid, cpumask);
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
#define vmstat_pgtok(a) ((a) * (sum.v_page_size >> 10))
#define	rate(x)	(((x) * rate_adj + halfuptime) / uptime)	/* round */
		if (hflag) {
			printf(" ");
			prthuman(total.t_avm * (u_int64_t)sum.v_page_size, 7);
			printf(" ");
			prthuman(total.t_free * (u_int64_t)sum.v_page_size, 6);
			printf(" ");
		} else {
			printf(" %7d ", vmstat_pgtok(total.t_avm));
			printf(" %6d ", vmstat_pgtok(total.t_free));
		}
		(void)printf("%5lu ",
		    (unsigned long)rate(sum.v_vm_faults - osum.v_vm_faults));
		(void)printf("%3lu ",
		    (unsigned long)rate(sum.v_reactivated - osum.v_reactivated));
		(void)printf("%3lu ",
		    (unsigned long)rate(sum.v_swapin + sum.v_vnodein -
		    (osum.v_swapin + osum.v_vnodein)));
		(void)printf("%3lu ",
		    (unsigned long)rate(sum.v_swapout + sum.v_vnodeout -
		    (osum.v_swapout + osum.v_vnodeout)));
		(void)printf("%5lu ",
		    (unsigned long)rate(sum.v_tfree - osum.v_tfree));
		(void)printf("%3lu ",
		    (unsigned long)rate(sum.v_pdpages - osum.v_pdpages));
		devstats();
		(void)printf("%4lu %4lu %4lu",
		    (unsigned long)rate(sum.v_intr - osum.v_intr),
		    (unsigned long)rate(sum.v_syscall - osum.v_syscall),
		    (unsigned long)rate(sum.v_swtch - osum.v_swtch));
		if (Pflag)
			pcpustats(ncpus, cpumask, maxid);
		else
			cpustats();
		(void)printf("\n");
		(void)fflush(stdout);
		if (reps >= 0 && --reps <= 0)
			break;
		osum = sum;
		uptime = interval;
		rate_adj = 1000;
		/*
		 * We round upward to avoid losing low-frequency events
		 * (i.e., >= 1 per interval but < 1 per millisecond).
		 */
		if (interval != 1)
			halfuptime = (uptime + 1) / 2;
		else
			halfuptime = 0;
		(void)usleep(interval * 1000);
	}
}

static void
printhdr(int maxid, u_long cpumask)
{
	int i, num_shown;

	num_shown = (num_selected < maxshowdevs) ? num_selected : maxshowdevs;
	(void)printf(" procs      memory      page%*s", 19, "");
	if (num_shown > 1)
		(void)printf(" disks %*s", num_shown * 4 - 7, "");
	else if (num_shown == 1)
		(void)printf("disk");
	(void)printf("   faults         ");
	if (Pflag) {
		for (i = 0; i <= maxid; i++) {
			if (cpumask & (1ul << i))
				printf("cpu%-2d    ", i);
		}
		printf("\n");
	} else
		printf("cpu\n");
	(void)printf(" r b w     avm    fre   flt  re  pi  po    fr  sr ");
	for (i = 0; i < num_devices; i++)
		if ((dev_select[i].selected)
		 && (dev_select[i].selected <= maxshowdevs))
			(void)printf("%c%c%d ", dev_select[i].device_name[0],
				     dev_select[i].device_name[1],
				     dev_select[i].unit_number);
	(void)printf("  in   sy   cs");
	if (Pflag) {
		for (i = 0; i <= maxid; i++) {
			if (cpumask & (1ul << i))
				printf(" us sy id");
		}
		printf("\n");
	} else
		printf(" us sy id\n");
	if (wresized != 0)
		doresize();
	hdrcnt = winlines;
}

/*
 * Force a header to be prepended to the next output.
 */
static void
needhdr(int dummy __unused)
{

	hdrcnt = 1;
}

/*
 * When the terminal is resized, force an update of the maximum number of rows
 * printed between each header repetition.  Then force a new header to be
 * prepended to the next output.
 */
void
needresize(int signo)
{

	wresized = 1;
	hdrcnt = 1;
}

/*
 * Update the global `winlines' count of terminal rows.
 */
void
doresize(void)
{
	int status;
	struct winsize w;

	for (;;) {
		status = ioctl(fileno(stdout), TIOCGWINSZ, &w);
		if (status == -1 && errno == EINTR)
			continue;
		else if (status == -1)
			err(1, "ioctl");
		if (w.ws_row > 3)
			winlines = w.ws_row - 3;
		else
			winlines = VMSTAT_DEFAULT_LINES;
		break;
	}

	/*
	 * Inhibit doresize() calls until we are rescheduled by SIGWINCH.
	 */
	wresized = 0;
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
	(void)printf("%9u page faults requiring I/O\n", sum.v_io_faults);
	(void)printf("%9u pages affected by kernel thread creation\n", sum.v_kthreadpages);
	(void)printf("%9u pages affected by  fork()\n", sum.v_forkpages);
	(void)printf("%9u pages affected by vfork()\n", sum.v_vforkpages);
	(void)printf("%9u pages affected by rfork()\n", sum.v_rforkpages);
	(void)printf("%9u pages cached\n", sum.v_tcached);
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
	(void)printf("%u forks, %u pages, average %.2f\n",
	    sum.v_forks, sum.v_forkpages,
	    sum.v_forks == 0 ? 0.0 :
	    (double)sum.v_forkpages / sum.v_forks);
	(void)printf("%u vforks, %u pages, average %.2f\n",
	    sum.v_vforks, sum.v_vforkpages,
	    sum.v_vforks == 0 ? 0.0 :
	    (double)sum.v_vforkpages / sum.v_vforks);
	(void)printf("%u rforks, %u pages, average %.2f\n",
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
percent(double pct, int *over)
{
	char buf[10];
	int l;

	l = snprintf(buf, sizeof(buf), "%.0f", pct);
	if (l == 1 && *over) {
		printf("%s",  buf);
		(*over)--;
	} else
		printf("%2s", buf);
	if (l > 2)
		(*over)++;
}

static void
cpustats(void)
{
	int state, over;
	double lpct, total;

	total = 0;
	for (state = 0; state < CPUSTATES; ++state)
		total += cur.cp_time[state];
	if (total)
		lpct = 100.0 / total;
	else
		lpct = 0.0;
	over = 0;
	printf(" ");
	percent((cur.cp_time[CP_USER] + cur.cp_time[CP_NICE]) * lpct, &over);
	printf(" ");
	percent((cur.cp_time[CP_SYS] + cur.cp_time[CP_INTR]) * lpct, &over);
	printf(" ");
	percent(cur.cp_time[CP_IDLE] * lpct, &over);
}

static void
pcpustats(int ncpus, u_long cpumask, int maxid)
{
	int state, i;
	double lpct, total;
	long tmp;
	int over;

	/* devstats does this for cp_time */
	for (i = 0; i <= maxid; i++) {
		if ((cpumask & (1ul << i)) == 0)
			continue;
		for (state = 0; state < CPUSTATES; ++state) {
			tmp = cur_cp_times[i * CPUSTATES + state];
			cur_cp_times[i * CPUSTATES + state] -= last_cp_times[i * CPUSTATES + state];
			last_cp_times[i * CPUSTATES + state] = tmp;
		}
	}

	over = 0;
	for (i = 0; i <= maxid; i++) {
		if ((cpumask & (1ul << i)) == 0)
			continue;
		total = 0;
		for (state = 0; state < CPUSTATES; ++state)
			total += cur_cp_times[i * CPUSTATES + state];
		if (total)
			lpct = 100.0 / total;
		else
			lpct = 0.0;
		printf(" ");
		percent((cur_cp_times[i * CPUSTATES + CP_USER] +
			 cur_cp_times[i * CPUSTATES + CP_NICE]) * lpct, &over);
		printf(" ");
		percent((cur_cp_times[i * CPUSTATES + CP_SYS] +
			 cur_cp_times[i * CPUSTATES + CP_INTR]) * lpct, &over);
		printf(" ");
		percent(cur_cp_times[i * CPUSTATES + CP_IDLE] * lpct, &over);
	}
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
		kread(X_SINTRCNT, &intrcntlen, sizeof(intrcntlen));
		kread(X_SINTRNAMES, &inamlen, sizeof(inamlen));
		if ((intrcnt = malloc(intrcntlen)) == NULL ||
		    (intrname = malloc(inamlen)) == NULL)
			err(1, "malloc()");
		kread(X_INTRCNT, intrcnt, intrcntlen);
		kread(X_INTRNAMES, intrname, inamlen);
	} else {
		for (intrcnt = NULL, intrcntlen = 1024; ; intrcntlen *= 2) {
			if ((intrcnt = reallocf(intrcnt, intrcntlen)) == NULL)
				err(1, "reallocf()");
			if (mysysctl("hw.intrcnt",
			    intrcnt, &intrcntlen, NULL, 0) == 0)
				break;
		}
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
	(void)printf("%-*s %20s %10s\n", (int)istrnamlen, "interrupt", "total",
	    "rate");
	inttotal = 0;
	for (i = 0; i < nintr; i++) {
		if (intrname[0] != '\0' && (*intrcnt != 0 || aflag))
			(void)printf("%-*s %20lu %10lu\n", (int)istrnamlen,
			    intrname, *intrcnt, *intrcnt / uptime);
		intrname += strlen(intrname) + 1;
		inttotal += *intrcnt++;
	}
	(void)printf("%-*s %20" PRIu64 " %10" PRIu64 "\n", (int)istrnamlen,
	    "Total", inttotal, inttotal / uptime);
}

static void
domemstat_malloc(void)
{
	struct memory_type_list *mtlp;
	struct memory_type *mtp;
	int error, first, i;

	mtlp = memstat_mtl_alloc();
	if (mtlp == NULL) {
		warn("memstat_mtl_alloc");
		return;
	}
	if (kd == NULL) {
		if (memstat_sysctl_malloc(mtlp, 0) < 0) {
			warnx("memstat_sysctl_malloc: %s",
			    memstat_strerror(memstat_mtl_geterror(mtlp)));
			return;
		}
	} else {
		if (memstat_kvm_malloc(mtlp, kd) < 0) {
			error = memstat_mtl_geterror(mtlp);
			if (error == MEMSTAT_ERROR_KVM)
				warnx("memstat_kvm_malloc: %s",
				    kvm_geterr(kd));
			else
				warnx("memstat_kvm_malloc: %s",
				    memstat_strerror(error));
		}
	}
	printf("%13s %5s %6s %7s %8s  Size(s)\n", "Type", "InUse", "MemUse",
	    "HighUse", "Requests");
	for (mtp = memstat_mtl_first(mtlp); mtp != NULL;
	    mtp = memstat_mtl_next(mtp)) {
		if (memstat_get_numallocs(mtp) == 0 &&
		    memstat_get_count(mtp) == 0)
			continue;
		printf("%13s %5" PRIu64 " %5" PRIu64 "K %7s %8" PRIu64 "  ",
		    memstat_get_name(mtp), memstat_get_count(mtp),
		    (memstat_get_bytes(mtp) + 1023) / 1024, "-",
		    memstat_get_numallocs(mtp));
		first = 1;
		for (i = 0; i < 32; i++) {
			if (memstat_get_sizemask(mtp) & (1 << i)) {
				if (!first)
					printf(",");
				printf("%d", 1 << (i + 4));
				first = 0;
			}
		}
		printf("\n");
	}
	memstat_mtl_free(mtlp);
}

static void
domemstat_zone(void)
{
	struct memory_type_list *mtlp;
	struct memory_type *mtp;
	char name[MEMTYPE_MAXNAME + 1];
	int error;

	mtlp = memstat_mtl_alloc();
	if (mtlp == NULL) {
		warn("memstat_mtl_alloc");
		return;
	}
	if (kd == NULL) {
		if (memstat_sysctl_uma(mtlp, 0) < 0) {
			warnx("memstat_sysctl_uma: %s",
			    memstat_strerror(memstat_mtl_geterror(mtlp)));
			return;
		}
	} else {
		if (memstat_kvm_uma(mtlp, kd) < 0) {
			error = memstat_mtl_geterror(mtlp);
			if (error == MEMSTAT_ERROR_KVM)
				warnx("memstat_kvm_uma: %s",
				    kvm_geterr(kd));
			else
				warnx("memstat_kvm_uma: %s",
				    memstat_strerror(error));
		}
	}
	printf("%-20s %6s %6s %8s %8s %8s %4s %4s\n\n", "ITEM", "SIZE",
	    "LIMIT", "USED", "FREE", "REQ", "FAIL", "SLEEP");
	for (mtp = memstat_mtl_first(mtlp); mtp != NULL;
	    mtp = memstat_mtl_next(mtp)) {
		strlcpy(name, memstat_get_name(mtp), MEMTYPE_MAXNAME);
		strcat(name, ":");
		printf("%-20s %6" PRIu64 ", %6" PRIu64 ",%8" PRIu64 ",%8" PRIu64
		    ",%8" PRIu64 ",%4" PRIu64 ",%4" PRIu64 "\n", name,
		    memstat_get_size(mtp), memstat_get_countlimit(mtp),
		    memstat_get_count(mtp), memstat_get_free(mtp),
		    memstat_get_numallocs(mtp), memstat_get_failures(mtp),
		    memstat_get_sleeps(mtp));
	}
	memstat_mtl_free(mtlp);
	printf("\n");
}

static void
display_object(struct kinfo_vmobject *kvo)
{
	const char *str;

	printf("%5jd ", (uintmax_t)kvo->kvo_resident);
	printf("%5jd ", (uintmax_t)kvo->kvo_active);
	printf("%5jd ", (uintmax_t)kvo->kvo_inactive);
	printf("%3d ", kvo->kvo_ref_count);
	printf("%3d ", kvo->kvo_shadow_count);
	switch (kvo->kvo_memattr) {
#ifdef VM_MEMATTR_UNCACHEABLE
	case VM_MEMATTR_UNCACHEABLE:
		str = "UC";
		break;
#endif
#ifdef VM_MEMATTR_WRITE_COMBINING
	case VM_MEMATTR_WRITE_COMBINING:
		str = "WC";
		break;
#endif
#ifdef VM_MEMATTR_WRITE_THROUGH
	case VM_MEMATTR_WRITE_THROUGH:
		str = "WT";
		break;
#endif
#ifdef VM_MEMATTR_WRITE_PROTECTED
	case VM_MEMATTR_WRITE_PROTECTED:
		str = "WP";
		break;
#endif
#ifdef VM_MEMATTR_WRITE_BACK
	case VM_MEMATTR_WRITE_BACK:
		str = "WB";
		break;
#endif
#ifdef VM_MEMATTR_WEAK_UNCACHEABLE
	case VM_MEMATTR_WEAK_UNCACHEABLE:
		str = "UC-";
		break;
#endif
#ifdef VM_MEMATTR_WB_WA
	case VM_MEMATTR_WB_WA:
		str = "WB";
		break;
#endif
#ifdef VM_MEMATTR_NOCACHE
	case VM_MEMATTR_NOCACHE:
		str = "NC";
		break;
#endif
#ifdef VM_MEMATTR_DEVICE
	case VM_MEMATTR_DEVICE:
		str = "DEV";
		break;
#endif
#ifdef VM_MEMATTR_CACHEABLE
	case VM_MEMATTR_CACHEABLE:
		str = "C";
		break;
#endif
#ifdef VM_MEMATTR_PREFETCHABLE
	case VM_MEMATTR_PREFETCHABLE:
		str = "PRE";
		break;
#endif
	default:
		str = "??";
		break;
	}
	printf("%-3s ", str);
	switch (kvo->kvo_type) {
	case KVME_TYPE_NONE:
		str = "--";
		break;
	case KVME_TYPE_DEFAULT:
		str = "df";
		break;
	case KVME_TYPE_VNODE:
		str = "vn";
		break;
	case KVME_TYPE_SWAP:
		str = "sw";
		break;
	case KVME_TYPE_DEVICE:
		str = "dv";
		break;
	case KVME_TYPE_PHYS:
		str = "ph";
		break;
	case KVME_TYPE_DEAD:
		str = "dd";
		break;
	case KVME_TYPE_SG:
		str = "sg";
		break;
	case KVME_TYPE_UNKNOWN:
	default:
		str = "??";
		break;
	}
	printf("%-2s ", str);
	printf("%-s\n", kvo->kvo_path);
}

static void
doobjstat(void)
{
	struct kinfo_vmobject *kvo;
	int cnt, i;

	kvo = kinfo_getvmobject(&cnt);
	if (kvo == NULL) {
		warn("Failed to fetch VM object list");
		return;
	}
	printf("%5s %5s %5s %3s %3s %3s %2s %s\n", "RES", "ACT", "INACT",
	    "REF", "SHD", "CM", "TP", "PATH");
	for (i = 0; i < cnt; i++)
		display_object(&kvo[i]);
	free(kvo);
}

/*
 * kread reads something from the kernel, given its nlist index.
 */
static void
kreado(int nlx, void *addr, size_t size, size_t offset)
{
	const char *sym;

	if (namelist[nlx].n_type == 0 || namelist[nlx].n_value == 0) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		errx(1, "symbol %s not defined", sym);
	}
	if ((size_t)kvm_read(kd, namelist[nlx].n_value + offset, addr,
	    size) != size) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		errx(1, "%s: %s", sym, kvm_geterr(kd));
	}
}

static void
kread(int nlx, void *addr, size_t size)
{
	kreado(nlx, addr, size, 0);
}

static char *
kgetstr(const char *strp)
{
	int n = 0, size = 1;
	char *ret = NULL;

	do {
		if (size == n + 1) {
			ret = realloc(ret, size);
			if (ret == NULL)
				err(1, "%s: realloc", __func__);
			size *= 2;
		}
		if (kvm_read(kd, (u_long)strp + n, &ret[n], 1) != 1)
			errx(1, "%s: %s", __func__, kvm_geterr(kd));
	} while (ret[n++] != '\0');
	return (ret);
}

static void
usage(void)
{
	(void)fprintf(stderr, "%s%s",
		"usage: vmstat [-afHhimoPsz] [-M core [-N system]] [-c count] [-n devs]\n",
		"              [-p type,if,pass] [-w wait] [disks] [wait [count]]\n");
	exit(1);
}
