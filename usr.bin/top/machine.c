/*
 * top - a top users display for Unix
 *
 * SYNOPSIS:  For FreeBSD-2.x and later
 *
 * DESCRIPTION:
 * Originally written for BSD4.4 system by Christos Zoulas.
 * Ported to FreeBSD 2.x by Steven Wallace && Wolfram Schneider
 * Order support hacked in from top-3.5beta6/machine/m_aix41.c
 *   by Monte Mitzelfelt (for latest top see http://www.groupsys.com/topinfo/)
 *
 * This is the machine-dependent module for FreeBSD 2.2
 * Works for:
 *	FreeBSD 2.2.x, 3.x, 4.x, and probably FreeBSD 2.1.x
 *
 * LIBS: -lkvm
 *
 * AUTHOR:  Christos Zoulas <christos@ee.cornell.edu>
 *          Steven Wallace  <swallace@freebsd.org>
 *          Wolfram Schneider <wosch@FreeBSD.org>
 *          Thomas Moestl <tmoestl@gmx.net>
 *
 * $FreeBSD$
 */


#include <sys/time.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/param.h>

#include "os.h"
#include <stdio.h>
#include <nlist.h>
#include <math.h>
#include <kvm.h>
#include <pwd.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/vmmeter.h>
#include <sys/resource.h>
#include <sys/rtprio.h>

/* Swap */
#include <stdlib.h>

#include <unistd.h>
#include <osreldate.h> /* for changes in kernel structures */

#include "top.h"
#include "machine.h"
#include "screen.h"
#include "utils.h"

static void getsysctl(char *, void *, size_t);

#define GETSYSCTL(name, var) getsysctl(name, &(var), sizeof(var))

extern char* printable(char *);
int swapmode(int *retavail, int *retfree);
static int smpmode;
static int namelength;
static int cmdlengthdelta;

/* Prototypes for top internals */
void quit(int);

/* get_process_info passes back a handle.  This is what it looks like: */

struct handle
{
    struct kinfo_proc **next_proc;	/* points to next valid proc pointer */
    int remaining;		/* number of pointers remaining */
};

/* declarations for load_avg */
#include "loadavg.h"

/* define what weighted cpu is.  */
#define weighted_cpu(pct, pp) ((pp)->ki_swtime == 0 ? 0.0 : \
			 ((pct) / (1.0 - exp((pp)->ki_swtime * logcpu))))

/* what we consider to be process size: */
#define PROCSIZE(pp) ((pp)->ki_size / 1024)

/* definitions for indices in the nlist array */

/*
 *  These definitions control the format of the per-process area
 */

static char smp_header[] =
  "  PID %-*.*s PRI NICE   SIZE    RES STATE  C   TIME   WCPU    CPU COMMAND";

#define smp_Proc_format \
	"%5d %-*.*s %3d %4d%7s %6s %-6.6s %1x%7s %5.2f%% %5.2f%% %.*s"

static char up_header[] =
  "  PID %-*.*s PRI NICE   SIZE    RES STATE    TIME   WCPU    CPU COMMAND";

#define up_Proc_format \
	"%5d %-*.*s %3d %4d%7s %6s %-6.6s%.0d%7s %5.2f%% %5.2f%% %.*s"



/* process state names for the "STATE" column of the display */
/* the extra nulls in the string "run" are for adding a slash and
   the processor number when needed */

char *state_abbrev[] =
{
    "", "START", "RUN\0\0\0", "SLEEP", "STOP", "ZOMB", "WAIT", "LOCK"
};


static kvm_t *kd;

/* values that we stash away in _init and use in later routines */

static double logcpu;

/* these are retrieved from the kernel in _init */

static load_avg  ccpu;

/* these are used in the get_ functions */

static int lastpid;

/* these are for calculating cpu state percentages */

static long cp_time[CPUSTATES];
static long cp_old[CPUSTATES];
static long cp_diff[CPUSTATES];

/* these are for detailing the process states */

int process_states[8];
char *procstatenames[] = {
    "", " starting, ", " running, ", " sleeping, ", " stopped, ",
    " zombie, ", " waiting, ", " lock, ",
    NULL
};

/* these are for detailing the cpu states */

int cpu_states[CPUSTATES];
char *cpustatenames[] = {
    "user", "nice", "system", "interrupt", "idle", NULL
};

/* these are for detailing the memory statistics */

int memory_stats[7];
char *memorynames[] = {
    "K Active, ", "K Inact, ", "K Wired, ", "K Cache, ", "K Buf, ", "K Free",
    NULL
};

int swap_stats[7];
char *swapnames[] = {
/*   0           1            2           3            4       5 */
    "K Total, ", "K Used, ", "K Free, ", "% Inuse, ", "K In, ", "K Out",
    NULL
};


/* these are for keeping track of the proc array */

static int nproc;
static int onproc = -1;
static int pref_len;
static struct kinfo_proc *pbase;
static struct kinfo_proc **pref;

/* these are for getting the memory statistics */

static int pageshift;		/* log base 2 of the pagesize */

/* define pagetok in terms of pageshift */

#define pagetok(size) ((size) << pageshift)

/* useful externals */
long percentages();

#ifdef ORDER
/* sorting orders. first is default */
char *ordernames[] = {
    "cpu", "size", "res", "time", "pri", NULL
};
#endif

int
machine_init(statics)

struct statics *statics;

{
    register int pagesize;
    size_t modelen;
    struct passwd *pw;

    modelen = sizeof(smpmode);
    if ((sysctlbyname("machdep.smp_active", &smpmode, &modelen, NULL, 0) < 0 &&
         sysctlbyname("kern.smp.active", &smpmode, &modelen, NULL, 0) < 0) ||
	modelen != sizeof(smpmode))
	    smpmode = 0;

    while ((pw = getpwent()) != NULL) {
	if (strlen(pw->pw_name) > namelength)
	    namelength = strlen(pw->pw_name);
    }
    if (namelength < 8)
	namelength = 8;
    if (smpmode && namelength > 13)
	namelength = 13;
    else if (namelength > 15)
	namelength = 15;

    if ((kd = kvm_open("/dev/null", "/dev/null", "/dev/null", O_RDONLY, "kvm_open")) == NULL)
	return -1;

    GETSYSCTL("kern.ccpu", ccpu);

    /* this is used in calculating WCPU -- calculate it ahead of time */
    logcpu = log(loaddouble(ccpu));

    pbase = NULL;
    pref = NULL;
    nproc = 0;
    onproc = -1;
    /* get the page size with "getpagesize" and calculate pageshift from it */
    pagesize = getpagesize();
    pageshift = 0;
    while (pagesize > 1)
    {
	pageshift++;
	pagesize >>= 1;
    }

    /* we only need the amount of log(2)1024 for our conversion */
    pageshift -= LOG1024;

    /* fill in the statics information */
    statics->procstate_names = procstatenames;
    statics->cpustate_names = cpustatenames;
    statics->memory_names = memorynames;
    statics->swap_names = swapnames;
#ifdef ORDER
    statics->order_names = ordernames;
#endif

    /* all done! */
    return(0);
}

char *format_header(uname_field)

register char *uname_field;

{
    static char Header[128];

    snprintf(Header, sizeof(Header), smpmode ? smp_header : up_header,
	     namelength, namelength, uname_field);

    cmdlengthdelta = strlen(Header) - 7;

    return Header;
}

static int swappgsin = -1;
static int swappgsout = -1;
extern struct timeval timeout;

void
get_system_info(si)

struct system_info *si;

{
    long total;
    struct loadavg sysload;
    int mib[2];
    struct timeval boottime;
    size_t bt_size;

    /* get the cp_time array */
    GETSYSCTL("kern.cp_time", cp_time);
    GETSYSCTL("vm.loadavg", sysload);
    GETSYSCTL("kern.lastpid", lastpid);

    /* convert load averages to doubles */
    {
	register int i;
	register double *infoloadp;

	infoloadp = si->load_avg;
	for (i = 0; i < 3; i++)
	{
#ifdef notyet
	    *infoloadp++ = ((double) sysload.ldavg[i]) / sysload.fscale;
#endif
	    *infoloadp++ = loaddouble(sysload.ldavg[i]);
	}
    }

    /* convert cp_time counts to percentages */
    total = percentages(CPUSTATES, cpu_states, cp_time, cp_old, cp_diff);

    /* sum memory & swap statistics */
    {
	static unsigned int swap_delay = 0;
	static int swapavail = 0;
	static int swapfree = 0;
	static int bufspace = 0;
	static int nspgsin, nspgsout;

	GETSYSCTL("vfs.bufspace", bufspace);
	GETSYSCTL("vm.stats.vm.v_active_count", memory_stats[0]);
	GETSYSCTL("vm.stats.vm.v_inactive_count", memory_stats[1]);
	GETSYSCTL("vm.stats.vm.v_wire_count", memory_stats[2]);
	GETSYSCTL("vm.stats.vm.v_cache_count", memory_stats[3]);
	GETSYSCTL("vm.stats.vm.v_free_count", memory_stats[5]);
	GETSYSCTL("vm.stats.vm.v_swappgsin", nspgsin);
	GETSYSCTL("vm.stats.vm.v_swappgsout", nspgsout);
	/* convert memory stats to Kbytes */
	memory_stats[0] = pagetok(memory_stats[0]);
	memory_stats[1] = pagetok(memory_stats[1]);
	memory_stats[2] = pagetok(memory_stats[2]);
	memory_stats[3] = pagetok(memory_stats[3]);
	memory_stats[4] = bufspace / 1024;
	memory_stats[5] = pagetok(memory_stats[5]);
	memory_stats[6] = -1;

	/* first interval */
        if (swappgsin < 0) {
	    swap_stats[4] = 0;
	    swap_stats[5] = 0;
	} 

	/* compute differences between old and new swap statistic */
	else {
	    swap_stats[4] = pagetok(((nspgsin - swappgsin)));
	    swap_stats[5] = pagetok(((nspgsout - swappgsout)));
	}

        swappgsin = nspgsin;
	swappgsout = nspgsout;

	/* call CPU heavy swapmode() only for changes */
        if (swap_stats[4] > 0 || swap_stats[5] > 0 || swap_delay == 0) {
	    swap_stats[3] = swapmode(&swapavail, &swapfree);
	    swap_stats[0] = swapavail;
	    swap_stats[1] = swapavail - swapfree;
	    swap_stats[2] = swapfree;
	}
        swap_delay = 1;
	swap_stats[6] = -1;
    }

    /* set arrays and strings */
    si->cpustates = cpu_states;
    si->memory = memory_stats;
    si->swap = swap_stats;


    if(lastpid > 0) {
	si->last_pid = lastpid;
    } else {
	si->last_pid = -1;
    }

    /*
     * Print how long system has been up.
     * (Found by looking getting "boottime" from the kernel)
     */
    mib[0] = CTL_KERN;
    mib[1] = KERN_BOOTTIME;
    bt_size = sizeof(boottime);
    if (sysctl(mib, 2, &boottime, &bt_size, NULL, 0) != -1 &&
	boottime.tv_sec != 0) {
	si->boottime = boottime;
    } else {
	si->boottime.tv_sec = -1;
    }
}

static struct handle handle;

caddr_t get_process_info(si, sel, compare)

struct system_info *si;
struct process_select *sel;
int (*compare)();

{
    register int i;
    register int total_procs;
    register int active_procs;
    register struct kinfo_proc **prefp;
    register struct kinfo_proc *pp;

    /* these are copied out of sel for speed */
    int show_idle;
    int show_self;
    int show_system;
    int show_uid;
    int show_command;

    
    pbase = kvm_getprocs(kd, sel->thread ? KERN_PROC_ALL : KERN_PROC_PROC,
	0, &nproc);
    if (nproc > onproc)
	pref = (struct kinfo_proc **) realloc(pref, sizeof(struct kinfo_proc *)
		* (onproc = nproc));
    if (pref == NULL || pbase == NULL) {
	(void) fprintf(stderr, "top: Out of memory.\n");
	quit(23);
    }
    /* get a pointer to the states summary array */
    si->procstates = process_states;

    /* set up flags which define what we are going to select */
    show_idle = sel->idle;
    show_self = sel->self;
    show_system = sel->system;
    show_uid = sel->uid != -1;
    show_command = sel->command != NULL;

    /* count up process states and get pointers to interesting procs */
    total_procs = 0;
    active_procs = 0;
    memset((char *)process_states, 0, sizeof(process_states));
    prefp = pref;
    for (pp = pbase, i = 0; i < nproc; pp++, i++)
    {
	/*
	 *  Place pointers to each valid proc structure in pref[].
	 *  Process slots that are actually in use have a non-zero
	 *  status field.  Processes with P_SYSTEM set are system
	 *  processes---these get ignored unless show_sysprocs is set.
	 */
	if (pp->ki_stat != 0 &&
	    (show_self != pp->ki_pid) &&
	    (show_system || ((pp->ki_flag & P_SYSTEM) == 0)))
	{
	    total_procs++;
	    process_states[(unsigned char) pp->ki_stat]++;
	    if ((pp->ki_stat != SZOMB) &&
		(show_idle || (pp->ki_pctcpu != 0) || 
		 (pp->ki_stat == SRUN)) &&
		(!show_uid || pp->ki_ruid == (uid_t)sel->uid))
	    {
		*prefp++ = pp;
		active_procs++;
	    }
	}
    }

    /* if requested, sort the "interesting" processes */
    if (compare != NULL)
    {
	qsort((char *)pref, active_procs, sizeof(struct kinfo_proc *), compare);
    }

    /* remember active and total counts */
    si->p_total = total_procs;
    si->p_active = pref_len = active_procs;

    /* pass back a handle */
    handle.next_proc = pref;
    handle.remaining = active_procs;
    return((caddr_t)&handle);
}

char fmt[128];		/* static area where result is built */

char *format_next_process(handle, get_userid)

caddr_t handle;
char *(*get_userid)();

{
    register struct kinfo_proc *pp;
    register long cputime;
    register double pct;
    struct handle *hp;
    char status[16];
    int state;

    /* find and remember the next proc structure */
    hp = (struct handle *)handle;
    pp = *(hp->next_proc++);
    hp->remaining--;
    
    /* get the process's command name */
    if ((pp->ki_sflag & PS_INMEM) == 0) {
	/*
	 * Print swapped processes as <pname>
	 */
	char *comm = pp->ki_comm;
#define COMSIZ sizeof(pp->ki_comm)
	char buf[COMSIZ];
	(void) strncpy(buf, comm, COMSIZ);
	comm[0] = '<';
	(void) strncpy(&comm[1], buf, COMSIZ - 2);
	comm[COMSIZ - 2] = '\0';
	(void) strncat(comm, ">", COMSIZ - 1);
	comm[COMSIZ - 1] = '\0';
    }

    /*
     * Convert the process's runtime from microseconds to seconds.  This
     * time includes the interrupt time although that is not wanted here.
     * ps(1) is similarly sloppy.
     */
    cputime = (pp->ki_runtime + 500000) / 1000000;

    /* calculate the base for cpu percentages */
    pct = pctdouble(pp->ki_pctcpu);

    /* generate "STATE" field */
    switch (state = pp->ki_stat) {
	case SRUN:
	    if (smpmode && pp->ki_oncpu != 0xff)
		sprintf(status, "CPU%d", pp->ki_oncpu);
	    else
		strcpy(status, "RUN");
	    break;
	case SLOCK:
	    if (pp->ki_kiflag & KI_LOCKBLOCK) {
		sprintf(status, "*%.6s", pp->ki_lockname);
	        break;
	    }
	    /* fall through */
	case SSLEEP:
	    if (pp->ki_wmesg != NULL) {
		sprintf(status, "%.6s", pp->ki_wmesg);
		break;
	    }
	    /* FALLTHROUGH */
	default:

	    if (state >= 0 &&
	        state < sizeof(state_abbrev) / sizeof(*state_abbrev))
		    sprintf(status, "%.6s", state_abbrev[(unsigned char) state]);
	    else
		    sprintf(status, "?%5d", state);
	    break;
    }

    /* format this entry */
    sprintf(fmt,
	    smpmode ? smp_Proc_format : up_Proc_format,
	    pp->ki_pid,
	    namelength, namelength,
	    (*get_userid)(pp->ki_ruid),
	    pp->ki_pri.pri_level - PZERO,

	    /*
	     * normal time      -> nice value -20 - +20 
	     * real time 0 - 31 -> nice value -52 - -21
	     * idle time 0 - 31 -> nice value +21 - +52
	     */
	    (pp->ki_pri.pri_class ==  PRI_TIMESHARE ? 
	    	pp->ki_nice - NZERO : 
	    	(PRI_IS_REALTIME(pp->ki_pri.pri_class) ?
		    (PRIO_MIN - 1 - (PRI_MAX_REALTIME - pp->ki_pri.pri_level)) :
		    (PRIO_MAX + 1 + pp->ki_pri.pri_level - PRI_MIN_IDLE))), 
	    format_k2(PROCSIZE(pp)),
	    format_k2(pagetok(pp->ki_rssize)),
	    status,
	    smpmode ? pp->ki_lastcpu : 0,
	    format_time(cputime),
	    100.0 * weighted_cpu(pct, pp),
	    100.0 * pct,
	    screen_width > cmdlengthdelta ?
		screen_width - cmdlengthdelta :
		0,
	    printable(pp->ki_comm));

    /* return the result */
    return(fmt);
}

static void getsysctl (name, ptr, len)

char *name;
void *ptr;
size_t len;

{
    size_t nlen = len;
    if (sysctlbyname(name, ptr, &nlen, NULL, 0) == -1) {
	    fprintf(stderr, "top: sysctl(%s...) failed: %s\n", name,
		strerror(errno));
	    quit(23);
    }
    if (nlen != len) {
	    fprintf(stderr, "top: sysctl(%s...) expected %lu, got %lu\n", name,
		(unsigned long)len, (unsigned long)nlen);
	    quit(23);
    }
}

/* comparison routines for qsort */

/*
 *  proc_compare - comparison function for "qsort"
 *	Compares the resource consumption of two processes using five
 *  	distinct keys.  The keys (in descending order of importance) are:
 *  	percent cpu, cpu ticks, state, resident set size, total virtual
 *  	memory usage.  The process states are ordered as follows (from least
 *  	to most important):  WAIT, zombie, sleep, stop, start, run.  The
 *  	array declaration below maps a process state index into a number
 *  	that reflects this ordering.
 */

static unsigned char sorted_state[] =
{
    0,	/* not used		*/
    3,	/* sleep		*/
    1,	/* ABANDONED (WAIT)	*/
    6,	/* run			*/
    5,	/* start		*/
    2,	/* zombie		*/
    4	/* stop			*/
};
 

#define ORDERKEY_PCTCPU \
  if (lresult = (long) p2->ki_pctcpu - (long) p1->ki_pctcpu, \
     (result = lresult > 0 ? 1 : lresult < 0 ? -1 : 0) == 0)

#define ORDERKEY_CPTICKS \
  if ((result = p2->ki_runtime > p1->ki_runtime ? 1 : \
                p2->ki_runtime < p1->ki_runtime ? -1 : 0) == 0)

#define ORDERKEY_STATE \
  if ((result = sorted_state[(unsigned char) p2->ki_stat] - \
                sorted_state[(unsigned char) p1->ki_stat]) == 0)

#define ORDERKEY_PRIO \
  if ((result = p2->ki_pri.pri_level - p1->ki_pri.pri_level) == 0)

#define ORDERKEY_RSSIZE \
  if ((result = p2->ki_rssize - p1->ki_rssize) == 0) 

#define ORDERKEY_MEM \
  if ( (result = PROCSIZE(p2) - PROCSIZE(p1)) == 0 )

/* compare_cpu - the comparison function for sorting by cpu percentage */

int
#ifdef ORDER
compare_cpu(pp1, pp2)
#else
proc_compare(pp1, pp2)
#endif

struct proc **pp1;
struct proc **pp2;

{
    register struct kinfo_proc *p1;
    register struct kinfo_proc *p2;
    register int result;
    register pctcpu lresult;

    /* remove one level of indirection */
    p1 = *(struct kinfo_proc **) pp1;
    p2 = *(struct kinfo_proc **) pp2;

    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ;

    return(result);
}

#ifdef ORDER
/* compare routines */
int compare_size(), compare_res(), compare_time(), compare_prio();

int (*proc_compares[])() = {
    compare_cpu,
    compare_size,
    compare_res,
    compare_time,
    compare_prio,
    NULL
};

/* compare_size - the comparison function for sorting by total memory usage */

int
compare_size(pp1, pp2)

struct proc **pp1;
struct proc **pp2;

{
    register struct kinfo_proc *p1;
    register struct kinfo_proc *p2;
    register int result;
    register pctcpu lresult;

    /* remove one level of indirection */
    p1 = *(struct kinfo_proc **) pp1;
    p2 = *(struct kinfo_proc **) pp2;

    ORDERKEY_MEM
    ORDERKEY_RSSIZE
    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ;

    return(result);
}

/* compare_res - the comparison function for sorting by resident set size */

int
compare_res(pp1, pp2)

struct proc **pp1;
struct proc **pp2;

{
    register struct kinfo_proc *p1;
    register struct kinfo_proc *p2;
    register int result;
    register pctcpu lresult;

    /* remove one level of indirection */
    p1 = *(struct kinfo_proc **) pp1;
    p2 = *(struct kinfo_proc **) pp2;

    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ORDERKEY_PCTCPU
    ORDERKEY_CPTICKS
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ;

    return(result);
}

/* compare_time - the comparison function for sorting by total cpu time */

int
compare_time(pp1, pp2)

struct proc **pp1;
struct proc **pp2;

{
    register struct kinfo_proc *p1;
    register struct kinfo_proc *p2;
    register int result;
    register pctcpu lresult;
  
    /* remove one level of indirection */
    p1 = *(struct kinfo_proc **) pp1;
    p2 = *(struct kinfo_proc **) pp2;

    ORDERKEY_CPTICKS
    ORDERKEY_PCTCPU
    ORDERKEY_STATE
    ORDERKEY_PRIO
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ;

      return(result);
  }
  
/* compare_prio - the comparison function for sorting by cpu percentage */

int
compare_prio(pp1, pp2)

struct proc **pp1;
struct proc **pp2;

{
    register struct kinfo_proc *p1;
    register struct kinfo_proc *p2;
    register int result;
    register pctcpu lresult;

    /* remove one level of indirection */
    p1 = *(struct kinfo_proc **) pp1;
    p2 = *(struct kinfo_proc **) pp2;

    ORDERKEY_PRIO
    ORDERKEY_CPTICKS
    ORDERKEY_PCTCPU
    ORDERKEY_STATE
    ORDERKEY_RSSIZE
    ORDERKEY_MEM
    ;

    return(result);
}
#endif

/*
 * proc_owner(pid) - returns the uid that owns process "pid", or -1 if
 *		the process does not exist.
 *		It is EXTREMLY IMPORTANT that this function work correctly.
 *		If top runs setuid root (as in SVR4), then this function
 *		is the only thing that stands in the way of a serious
 *		security problem.  It validates requests for the "kill"
 *		and "renice" commands.
 */

int proc_owner(pid)

int pid;

{
    register int cnt;
    register struct kinfo_proc **prefp;
    register struct kinfo_proc *pp;

    prefp = pref;
    cnt = pref_len;
    while (--cnt >= 0)
    {
	pp = *prefp++;	
	if (pp->ki_pid == (pid_t)pid)
	{
	    return((int)pp->ki_ruid);
	}
    }
    return(-1);
}

int
swapmode(retavail, retfree)
	int *retavail;
	int *retfree;
{
	int n;
	int pagesize = getpagesize();
	struct kvm_swap swapary[1];

	*retavail = 0;
	*retfree = 0;

#define CONVERT(v)	((quad_t)(v) * pagesize / 1024)

	n = kvm_getswapinfo(kd, swapary, 1, 0);
	if (n < 0 || swapary[0].ksw_total == 0)
		return(0);

	*retavail = CONVERT(swapary[0].ksw_total);
	*retfree = CONVERT(swapary[0].ksw_total - swapary[0].ksw_used);

	n = (int)((double)swapary[0].ksw_used * 100.0 /
	    (double)swapary[0].ksw_total);
	return(n);
}

