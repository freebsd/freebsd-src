/*
 * Copyright (c) 2000 Dag-Erling Coïdan Smørgrav
 * Copyright (c) 1999 Pierre Beyssac
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *
 *	@(#)procfs_status.c	8.4 (Berkeley) 6/15/94
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/blist.h>
#include <sys/conf.h>
#include <sys/dkstat.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_zone.h>
#include <vm/swap_pager.h>

#include <sys/exec.h>
#include <sys/user.h>
#include <sys/vmmeter.h>

#include <machine/clock.h>

#ifdef __alpha__
#include <machine/alpha_cpu.h>
#include <machine/cpuconf.h>
#include <machine/rpb.h>
extern int ncpus;
#endif /* __alpha__ */

#ifdef __i386__
#include <machine/cputypes.h>
#include <machine/md_var.h>
#endif /* __i386__ */

#include <sys/socket.h>
#include <net/if.h>

#include <compat/linux/linux_mib.h>
#include <fs/pseudofs/pseudofs.h>

extern struct cdevsw *cdevsw[];

/*
 * Various conversion macros
 */
#define T2J(x) (((x) * 100UL) / (stathz ? stathz : hz))	/* ticks to jiffies */
#define T2S(x) ((x) / (stathz ? stathz : hz))		/* ticks to seconds */
#define B2K(x) ((x) >> 10)				/* bytes to kbytes */
#define B2P(x) ((x) >> PAGE_SHIFT)			/* bytes to pages */
#define P2B(x) ((x) << PAGE_SHIFT)			/* pages to bytes */
#define P2K(x) ((x) << (PAGE_SHIFT - 10))		/* pages to kbytes */

/*
 * Filler function for proc/meminfo
 */
static int
linprocfs_domeminfo(PFS_FILL_ARGS)
{
	unsigned long memtotal;		/* total memory in bytes */
	unsigned long memused;		/* used memory in bytes */
	unsigned long memfree;		/* free memory in bytes */
	unsigned long memshared;	/* shared memory ??? */
	unsigned long buffers, cached;	/* buffer / cache memory ??? */
	u_quad_t swaptotal;		/* total swap space in bytes */
	u_quad_t swapused;		/* used swap space in bytes */
	u_quad_t swapfree;		/* free swap space in bytes */
	vm_object_t object;

	memtotal = physmem * PAGE_SIZE;
	/*
	 * The correct thing here would be:
	 *
	memfree = cnt.v_free_count * PAGE_SIZE;
	memused = memtotal - memfree;
	 *
	 * but it might mislead linux binaries into thinking there
	 * is very little memory left, so we cheat and tell them that
	 * all memory that isn't wired down is free.
	 */
	memused = cnt.v_wire_count * PAGE_SIZE;
	memfree = memtotal - memused;
	if (swapblist == NULL) {
		swaptotal = 0;
		swapfree = 0;
	} else {
		swaptotal = (u_quad_t)swapblist->bl_blocks * 1024; /* XXX why 1024? */
		swapfree = (u_quad_t)swapblist->bl_root->u.bmu_avail * PAGE_SIZE;
	}
	swapused = swaptotal - swapfree;
	memshared = 0;
	TAILQ_FOREACH(object, &vm_object_list, object_list)
		if (object->shadow_count > 1)
			memshared += object->resident_page_count;
	memshared *= PAGE_SIZE;
	/*
	 * We'd love to be able to write:
	 *
	buffers = bufspace;
	 *
	 * but bufspace is internal to vfs_bio.c and we don't feel
	 * like unstaticizing it just for linprocfs's sake.
	 */
	buffers = 0;
	cached = cnt.v_cache_count * PAGE_SIZE;

	sbuf_printf(sb,
	    "	     total:    used:	free:  shared: buffers:	 cached:\n"
	    "Mem:  %lu %lu %lu %lu %lu %lu\n"
	    "Swap: %llu %llu %llu\n"
	    "MemTotal: %9lu kB\n"
	    "MemFree:  %9lu kB\n"
	    "MemShared:%9lu kB\n"
	    "Buffers:  %9lu kB\n"
	    "Cached:   %9lu kB\n"
	    "SwapTotal:%9llu kB\n"
	    "SwapFree: %9llu kB\n",
	    memtotal, memused, memfree, memshared, buffers, cached,
	    swaptotal, swapused, swapfree,
	    B2K(memtotal), B2K(memfree),
	    B2K(memshared), B2K(buffers), B2K(cached),
	    B2K(swaptotal), B2K(swapfree));

	return (0);
}

#ifdef __alpha__
/*
 * Filler function for proc/cpuinfo (Alpha version)
 */
static int
linprocfs_docpuinfo(PFS_FILL_ARGS)
{
	u_int64_t type, major;
	struct pcs *pcsp;
	const char *model, *sysname;

	static const char *cpuname[] = {
		"EV3", "EV4", "Simulate", "LCA4", "EV5", "EV45", "EV56",
		"EV6", "PCA56", "PCA57", "EV67", "EV68CB", "EV68AL"
	};

	pcsp = LOCATE_PCS(hwrpb, hwrpb->rpb_primary_cpu_id);
	type = pcsp->pcs_proc_type;
	major = (type & PCS_PROC_MAJOR) >> PCS_PROC_MAJORSHIFT;
	if (major < sizeof(cpuname)/sizeof(char *)) {
		model = cpuname[major - 1];
	} else {
		model = "unknown";
	}
	
	sysname = alpha_dsr_sysname();
	    
	sbuf_printf(sb,
	    "cpu\t\t\t: Alpha\n"
	    "cpu model\t\t: %s\n"
	    "cpu variation\t\t: %ld\n"
	    "cpu revision\t\t: %ld\n"
	    "cpu serial number\t: %s\n"
	    "system type\t\t: %s\n"
	    "system variation\t: %s\n"
	    "system revision\t\t: %ld\n"
	    "system serial number\t: %s\n"
	    "cycle frequency [Hz]\t: %lu\n"
	    "timer frequency [Hz]\t: %lu\n"
	    "page size [bytes]\t: %ld\n"
	    "phys. address bits\t: %ld\n"
	    "max. addr. space #\t: %ld\n"
	    "BogoMIPS\t\t: %lu.%02lu\n"
	    "kernel unaligned acc\t: %ld (pc=%lx,va=%lx)\n"
	    "user unaligned acc\t: %ld (pc=%lx,va=%lx)\n"
	    "platform string\t\t: %s\n"
	    "cpus detected\t\t: %d\n"
	    ,
	    model,
	    pcsp->pcs_proc_var,
	    *(int *)hwrpb->rpb_revision,
	    " ",
	    " ",
	    "0",
	    0,
	    " ",
	    hwrpb->rpb_cc_freq,
	    hz,
	    hwrpb->rpb_page_size,
	    hwrpb->rpb_phys_addr_size,
	    hwrpb->rpb_max_asn,
	    0, 0,
	    0, 0, 0,
	    0, 0, 0,
	    sysname,
	    ncpus);
	return (0);
}
#endif /* __alpha__ */

#ifdef __i386__
/*
 * Filler function for proc/cpuinfo (i386 version)
 */
static int
linprocfs_docpuinfo(PFS_FILL_ARGS)
{
	int class, i, fqmhz, fqkhz;

	/*
	 * We default the flags to include all non-conflicting flags,
	 * and the Intel versions of conflicting flags.
	 */
	static char *flags[] = {
		"fpu",	    "vme",     "de",	   "pse",      "tsc",
		"msr",	    "pae",     "mce",	   "cx8",      "apic",
		"sep",	    "sep",     "mtrr",	   "pge",      "mca",
		"cmov",	    "pat",     "pse36",	   "pn",       "b19",
		"b20",	    "b21",     "mmxext",   "mmx",      "fxsr",
		"xmm",	    "b26",     "b27",	   "b28",      "b29",
		"3dnowext", "3dnow"
	};

	switch (cpu_class) {
	case CPUCLASS_286:
		class = 2;
		break;
	case CPUCLASS_386:
		class = 3;
		break;
	case CPUCLASS_486:
		class = 4;
		break;
	case CPUCLASS_586:
		class = 5;
		break;
	case CPUCLASS_686:
		class = 6;
		break;
	default:
		class = 0;
		break;
	}

	sbuf_printf(sb,
	    "processor\t: %d\n"
	    "vendor_id\t: %.20s\n"
	    "cpu family\t: %d\n"
	    "model\t\t: %d\n"
	    "stepping\t: %d\n",
	    0, cpu_vendor, class, cpu, cpu_id & 0xf);

	sbuf_cat(sb,
	    "flags\t\t:");

	if (!strcmp(cpu_vendor, "AuthenticAMD") && (class < 6)) {
		flags[16] = "fcmov";
	} else if (!strcmp(cpu_vendor, "CyrixInstead")) {
		flags[24] = "cxmmx";
	}
	
	for (i = 0; i < 32; i++)
		if (cpu_feature & (1 << i))
			sbuf_printf(sb, " %s", flags[i]);
	sbuf_cat(sb, "\n");
	if (class >= 5) {
		fqmhz = (tsc_freq + 4999) / 1000000;
		fqkhz = ((tsc_freq + 4999) / 10000) % 100;
		sbuf_printf(sb,
		    "cpu MHz\t\t: %d.%02d\n"
		    "bogomips\t: %d.%02d\n",
		    fqmhz, fqkhz, fqmhz, fqkhz);
	}

	return (0);
}
#endif /* __i386__ */

/*
 * Filler function for proc/stat
 */
static int
linprocfs_dostat(PFS_FILL_ARGS)
{
	sbuf_printf(sb,
	    "cpu %ld %ld %ld %ld\n"
	    "disk 0 0 0 0\n"
	    "page %u %u\n"
	    "swap %u %u\n"
	    "intr %u\n"
	    "ctxt %u\n"
	    "btime %ld\n",
	    T2J(cp_time[CP_USER]),
	    T2J(cp_time[CP_NICE]),
	    T2J(cp_time[CP_SYS] /*+ cp_time[CP_INTR]*/),
	    T2J(cp_time[CP_IDLE]),
	    cnt.v_vnodepgsin,
	    cnt.v_vnodepgsout,
	    cnt.v_swappgsin,
	    cnt.v_swappgsout,
	    cnt.v_intr,
	    cnt.v_swtch,
	    boottime.tv_sec);
	return (0);
}

/*
 * Filler function for proc/uptime
 */
static int
linprocfs_douptime(PFS_FILL_ARGS)
{
	struct timeval tv;

	getmicrouptime(&tv);
	sbuf_printf(sb, "%ld.%02ld %ld.%02ld\n",
	    tv.tv_sec, tv.tv_usec / 10000,
	    T2S(cp_time[CP_IDLE]), T2J(cp_time[CP_IDLE]) % 100);
	return (0);
}

/*
 * Filler function for proc/version
 */
static int
linprocfs_doversion(PFS_FILL_ARGS)
{
	sbuf_printf(sb,
	    "%s version %s (des@freebsd.org) (gcc version " __VERSION__ ")"
	    " #4 Sun Dec 18 04:30:00 CET 1977\n",
	    linux_get_osname(curp),
	    linux_get_osrelease(curp));
	return (0);
}

/*
 * Filler function for proc/loadavg
 */
static int
linprocfs_doloadavg(PFS_FILL_ARGS)
{
	sbuf_printf(sb,
	    "%d.%02d %d.%02d %d.%02d %d/%d %d\n",
	    (int)(averunnable.ldavg[0] / averunnable.fscale),
	    (int)(averunnable.ldavg[0] * 100 / averunnable.fscale % 100),
	    (int)(averunnable.ldavg[1] / averunnable.fscale),
	    (int)(averunnable.ldavg[1] * 100 / averunnable.fscale % 100),
	    (int)(averunnable.ldavg[2] / averunnable.fscale),
	    (int)(averunnable.ldavg[2] * 100 / averunnable.fscale % 100),
	    1,				/* number of running tasks */
	    nprocs,			/* number of tasks */
	    lastpid			/* the last pid */
	);
	
	return (0);
}

/*
 * Filler function for proc/pid/stat
 */
static int
linprocfs_doprocstat(PFS_FILL_ARGS)
{
	struct kinfo_proc kp;

	fill_kinfo_proc(p, &kp);
	sbuf_printf(sb, "%d", p->p_pid);
#define PS_ADD(name, fmt, arg) sbuf_printf(sb, " " fmt, arg)
	PS_ADD("comm",		"(%s)",	p->p_comm);
	PS_ADD("statr",		"%c",	'0'); /* XXX */
	PROC_LOCK(p);
	PS_ADD("ppid",		"%d",	p->p_pptr ? p->p_pptr->p_pid : 0);
	PROC_UNLOCK(p);
	PS_ADD("pgrp",		"%d",	p->p_pgid);
	PS_ADD("session",	"%d",	p->p_session->s_sid);
	PS_ADD("tty",		"%d",	0); /* XXX */
	PS_ADD("tpgid",		"%d",	0); /* XXX */
	PS_ADD("flags",		"%u",	0); /* XXX */
	PS_ADD("minflt",	"%u",	0); /* XXX */
	PS_ADD("cminflt",	"%u",	0); /* XXX */
	PS_ADD("majflt",	"%u",	0); /* XXX */
	PS_ADD("cminflt",	"%u",	0); /* XXX */
	PS_ADD("utime",		"%d",	0); /* XXX */
	PS_ADD("stime",		"%d",	0); /* XXX */
	PS_ADD("cutime",	"%d",	0); /* XXX */
	PS_ADD("cstime",	"%d",	0); /* XXX */
	PS_ADD("counter",	"%d",	0); /* XXX */
	PS_ADD("priority",	"%d",	0); /* XXX */
	PS_ADD("timeout",	"%u",	0); /* XXX */
	PS_ADD("itrealvalue",	"%u",	0); /* XXX */
	PS_ADD("starttime",	"%d",	0); /* XXX */
	PS_ADD("vsize",		"%u",	kp.ki_size);
	PS_ADD("rss",		"%u",	P2K(kp.ki_rssize));
	PS_ADD("rlim",		"%u",	0); /* XXX */
	PS_ADD("startcode",	"%u",	(unsigned)0);
	PS_ADD("endcode",	"%u",	0); /* XXX */
	PS_ADD("startstack",	"%u",	0); /* XXX */
	PS_ADD("esp",		"%u",	0); /* XXX */
	PS_ADD("eip",		"%u",	0); /* XXX */
	PS_ADD("signal",	"%d",	0); /* XXX */
	PS_ADD("blocked",	"%d",	0); /* XXX */
	PS_ADD("sigignore",	"%d",	0); /* XXX */
	PS_ADD("sigcatch",	"%d",	0); /* XXX */
	PS_ADD("wchan",		"%u",	0); /* XXX */
	PS_ADD("nswap",		"%lu",	(long unsigned)0); /* XXX */
	PS_ADD("cnswap",	"%lu",	(long unsigned)0); /* XXX */
	PS_ADD("exitsignal",	"%d",	0); /* XXX */
	PS_ADD("processor",	"%d",	0); /* XXX */
#undef PS_ADD
	sbuf_putc(sb, '\n');
	
	return (0);
}

/*
 * Map process state to descriptive letter. Note that this does not
 * quite correspond to what Linux outputs, but it's close enough.
 */
static char *state_str[] = {
	"? (unknown)",
	"I (idle)",
	"R (running)",
	"S (sleeping)",
	"T (stopped)",
	"Z (zombie)",
	"W (waiting)",
	"M (mutex)"
};

/*
 * Filler function for proc/pid/status
 */
static int
linprocfs_doprocstatus(PFS_FILL_ARGS)
{
	struct kinfo_proc kp;
	char *state;
	segsz_t lsize;
	int i;

	mtx_lock_spin(&sched_lock);
	if (p->p_stat > sizeof state_str / sizeof *state_str)
		state = state_str[0];
	else
		state = state_str[(int)p->p_stat];
	mtx_unlock_spin(&sched_lock);

	fill_kinfo_proc(p, &kp);
	sbuf_printf(sb, "Name:\t%s\n",		p->p_comm); /* XXX escape */
	sbuf_printf(sb, "State:\t%s\n",		state);

	/*
	 * Credentials
	 */
	sbuf_printf(sb, "Pid:\t%d\n",		p->p_pid);
	PROC_LOCK(p);
	sbuf_printf(sb, "PPid:\t%d\n",		p->p_pptr ?
						p->p_pptr->p_pid : 0);
	sbuf_printf(sb, "Uid:\t%d %d %d %d\n",	p->p_ucred->cr_ruid,
						p->p_ucred->cr_uid,
						p->p_ucred->cr_svuid,
						/* FreeBSD doesn't have fsuid */
						p->p_ucred->cr_uid);
	sbuf_printf(sb, "Gid:\t%d %d %d %d\n",	p->p_ucred->cr_rgid,
						p->p_ucred->cr_gid,
						p->p_ucred->cr_svgid,
						/* FreeBSD doesn't have fsgid */
						p->p_ucred->cr_gid);
	sbuf_cat(sb, "Groups:\t");
	for (i = 0; i < p->p_ucred->cr_ngroups; i++)
		sbuf_printf(sb, "%d ",		p->p_ucred->cr_groups[i]);
	PROC_UNLOCK(p);
	sbuf_putc(sb, '\n');
	
	/*
	 * Memory
	 *
	 * While our approximation of VmLib may not be accurate (I
	 * don't know of a simple way to verify it, and I'm not sure
	 * it has much meaning anyway), I believe it's good enough.
	 *
	 * The same code that could (I think) accurately compute VmLib
	 * could also compute VmLck, but I don't really care enough to
	 * implement it. Submissions are welcome.
	 */
	sbuf_printf(sb, "VmSize:\t%8u kB\n",	B2K(kp.ki_size));
	sbuf_printf(sb, "VmLck:\t%8u kB\n",	P2K(0)); /* XXX */
	sbuf_printf(sb, "VmRss:\t%8u kB\n",	P2K(kp.ki_rssize));
	sbuf_printf(sb, "VmData:\t%8u kB\n",	P2K(kp.ki_dsize));
	sbuf_printf(sb, "VmStk:\t%8u kB\n",	P2K(kp.ki_ssize));
	sbuf_printf(sb, "VmExe:\t%8u kB\n",	P2K(kp.ki_tsize));
	lsize = B2P(kp.ki_size) - kp.ki_dsize -
	    kp.ki_ssize - kp.ki_tsize - 1;
	sbuf_printf(sb, "VmLib:\t%8u kB\n",	P2K(lsize));

	/*
	 * Signal masks
	 *
	 * We support up to 128 signals, while Linux supports 32,
	 * but we only define 32 (the same 32 as Linux, to boot), so
	 * just show the lower 32 bits of each mask. XXX hack.
	 *
	 * NB: on certain platforms (Sparc at least) Linux actually
	 * supports 64 signals, but this code is a long way from
	 * running on anything but i386, so ignore that for now.
	 */
	PROC_LOCK(p);
	sbuf_printf(sb, "SigPnd:\t%08x\n",	p->p_siglist.__bits[0]);
	/*
	 * I can't seem to find out where the signal mask is in
	 * relation to struct proc, so SigBlk is left unimplemented.
	 */
	sbuf_printf(sb, "SigBlk:\t%08x\n",	0); /* XXX */
	sbuf_printf(sb, "SigIgn:\t%08x\n",	p->p_sigignore.__bits[0]);
	sbuf_printf(sb, "SigCgt:\t%08x\n",	p->p_sigcatch.__bits[0]);
	PROC_UNLOCK(p);
	
	/*
	 * Linux also prints the capability masks, but we don't have
	 * capabilities yet, and when we do get them they're likely to
	 * be meaningless to Linux programs, so we lie. XXX
	 */
	sbuf_printf(sb, "CapInh:\t%016x\n",	0);
	sbuf_printf(sb, "CapPrm:\t%016x\n",	0);
	sbuf_printf(sb, "CapEff:\t%016x\n",	0);
	
	return (0);
}

/*
 * Filler function for proc/self
 */
static int
linprocfs_doselflink(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "%ld", (long)curp->p_pid);
	return (0);
}

/*
 * Filler function for proc/pid/cmdline
 */
static int
linprocfs_doproccmdline(PFS_FILL_ARGS)
{
	struct ps_strings pstr;
	int error, i;

	/*
	 * If we are using the ps/cmdline caching, use that.  Otherwise
	 * revert back to the old way which only implements full cmdline
	 * for the currept process and just p->p_comm for all other
	 * processes.
	 * Note that if the argv is no longer available, we deliberately
	 * don't fall back on p->p_comm or return an error: the authentic
	 * Linux behaviour is to return zero-length in this case.
	 */

	if (p->p_args && (ps_argsopen || !p_can(curp, p, P_CAN_SEE, NULL))) {
		sbuf_bcpy(sb, p->p_args->ar_args, p->p_args->ar_length);
	} else if (p != curp) {
		sbuf_printf(sb, "%.*s", MAXCOMLEN, p->p_comm);
	} else {
		error = copyin((void*)PS_STRINGS, &pstr, sizeof(pstr));
		if (error)
			return (error);
		for (i = 0; i < pstr.ps_nargvstr; i++) {
			sbuf_copyin(sb, pstr.ps_argvstr[i], 0);
			sbuf_printf(sb, "%c", '\0');
		}
	}

	return (0);
}

/*
 * Filler function for proc/pid/exe
 */
static int
linprocfs_doexelink(PFS_FILL_ARGS)
{
	char *fullpath = "unknown";
	char *freepath = NULL;

	textvp_fullpath(p, &fullpath, &freepath);
	sbuf_printf(sb, "%s", fullpath);
	if (freepath)
		free(freepath, M_TEMP);
	return (0);
}

/*
 * Filler function for proc/net/dev
 */
static int
linprocfs_donetdev(PFS_FILL_ARGS)
{
	struct ifnet *ifp;
	int eth_index = 0;

	sbuf_printf(sb,
	    "Inter-|   Receive					     "
	    "	      |	 Transmit\n"
	    " face |bytes    packets errs drop fifo frame compressed "
	    "multicast|bytes	packets errs drop fifo colls carrier "
	    "compressed\n");

	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if (strcmp(ifp->if_name, "lo") == 0) {
			sbuf_printf(sb, "%6.6s:", ifp->if_name);
		} else {
			sbuf_printf(sb, "%5.5s%d:", "eth", eth_index);
			eth_index++;
		}
		sbuf_printf(sb,
		    "%8lu %7lu %4lu %4lu %4lu %5lu %10lu %9lu "
		    "%8lu %7lu %4lu %4lu %4lu %5lu %7lu %10lu\n",
		    0, 0, 0, 0, 0, 0, 0, 0,
		    0, 0, 0, 0, 0, 0, 0, 0);
	}
	
	return (0);
}

/*
 * Filler function for proc/devices
 */
static int
linprocfs_dodevices(PFS_FILL_ARGS)
{
	int i;

	sbuf_printf(sb, "Character devices:\n");

	for (i = 0; i < NUMCDEVSW; i++)
		if (cdevsw[i] != NULL)
			sbuf_printf(sb, "%3d %s\n", i, cdevsw[i]->d_name);

	sbuf_printf(sb, "\nBlock devices:\n");
	
	return (0);
}

/*
 * Filler function for proc/cmdline
 */
static int
linprocfs_docmdline(PFS_FILL_ARGS)
{
	sbuf_printf(sb, "BOOT_IMAGE=%s", kernelname);
	sbuf_printf(sb, " ro root=302\n");
	return (0);
}

/*
 * Directory structure
 */

static struct pfs_node linprocfs_proc_nodes[] = {
	PFS_THIS,
	PFS_PARENT,
	/*	    name	flags uid  gid	mode  data */
        PFS_FILE(   "cmdline",	0,    0,   0,	0444, linprocfs_doproccmdline),
	PFS_SYMLINK("exe",	0,    0,   0,	0444, linprocfs_doexelink),
     /* PFS_FILE(   "mem",	0,    0,   0,	0444, procfs_domem), */
	PFS_FILE(   "stat",	0,    0,   0,	0444, linprocfs_doprocstat),
	PFS_FILE(   "status",	0,    0,   0,	0444, linprocfs_doprocstatus),
	PFS_LASTNODE
};

static struct pfs_node linprocfs_net_nodes[] = {
	PFS_THIS,
	PFS_PARENT,
	/*	    name	flags uid  gid	mode  data */
	PFS_FILE(   "dev",	0,    0,   0,	0444, linprocfs_donetdev),
	PFS_LASTNODE
};

static struct pfs_node linprocfs_root_nodes[] = {
	PFS_THIS,
	PFS_PARENT,
	/*	    name	flags uid  gid	mode  data */
	PFS_FILE(   "cmdline",	0,    0,   0,	0444, linprocfs_docmdline),
	PFS_FILE(   "cpuinfo",	0,    0,   0,	0444, linprocfs_docpuinfo),
	PFS_FILE(   "devices",	0,    0,   0,	0444, linprocfs_dodevices),
	PFS_FILE(   "loadavg",	0,    0,   0,	0444, linprocfs_doloadavg),
	PFS_FILE(   "meminfo",	0,    0,   0,	0444, linprocfs_domeminfo),
	PFS_FILE(   "stat",	0,    0,   0,	0444, linprocfs_dostat),
	PFS_FILE(   "uptime",	0,    0,   0,	0444, linprocfs_douptime),
	PFS_FILE(   "version",	0,    0,   0,	0444, linprocfs_doversion),
	PFS_DIR(    "net",	0,    0,   0,	0555, linprocfs_net_nodes),
	PFS_PROCDIR(		0,    0,   0,	0555, linprocfs_proc_nodes),
	PFS_SYMLINK("self",	0,    0,   0,	0555, linprocfs_doselflink),
	PFS_LASTNODE
};

static struct pfs_node linprocfs_root =
	PFS_ROOT(linprocfs_root_nodes);

PSEUDOFS(linprocfs, linprocfs_root);
MODULE_DEPEND(linprocfs, linux, 1, 1, 1);
MODULE_DEPEND(linprocfs, procfs, 1, 1, 1);
