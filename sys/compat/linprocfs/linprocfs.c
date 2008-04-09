/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/blist.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/filedesc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/msg.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sbuf.h>
#include <sys/sem.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/tty.h>
#include <sys/user.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <net/if.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/swap_pager.h>

#include <machine/clock.h>

#if defined(__i386__) || defined(__amd64__)
#include <machine/cputypes.h>
#include <machine/md_var.h>
#endif /* __i386__ || __amd64__ */

#include "opt_compat.h"
#ifdef COMPAT_LINUX32				/* XXX */
#include <machine/../linux32/linux.h>
#else
#include <machine/../linux/linux.h>
#endif
#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_util.h>
#include <fs/pseudofs/pseudofs.h>
#include <fs/procfs/procfs.h>

/*
 * Various conversion macros
 */
#define T2J(x) (((x) * 100UL) / (stathz ? stathz : hz))	/* ticks to jiffies */
#define T2S(x) ((x) / (stathz ? stathz : hz))		/* ticks to seconds */
#define B2K(x) ((x) >> 10)				/* bytes to kbytes */
#define B2P(x) ((x) >> PAGE_SHIFT)			/* bytes to pages */
#define P2B(x) ((x) << PAGE_SHIFT)			/* pages to bytes */
#define P2K(x) ((x) << (PAGE_SHIFT - 10))		/* pages to kbytes */

/**
 * @brief Mapping of ki_stat in struct kinfo_proc to the linux state
 *
 * The linux procfs state field displays one of the characters RSDZTW to
 * denote running, sleeping in an interruptible wait, waiting in an
 * uninteruptible disk sleep, a zombie process, process is being traced
 * or stopped, or process is paging respectively.
 *
 * Our struct kinfo_proc contains the variable ki_stat which contains a
 * value out of SIDL, SRUN, SSLEEP, SSTOP, SZOMB, SWAIT and SLOCK.
 *
 * This character array is used with ki_stati-1 as an index and tries to
 * map our states to suitable linux states.
 */
static char linux_state[] = "RRSTZDD";

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
	unsigned long long swaptotal;	/* total swap space in bytes */
	unsigned long long swapused;	/* used swap space in bytes */
	unsigned long long swapfree;	/* free swap space in bytes */
	vm_object_t object;
	int i, j;

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
	swap_pager_status(&i, &j);
	swaptotal = (unsigned long long)i * PAGE_SIZE;
	swapused = (unsigned long long)j * PAGE_SIZE;
	swapfree = swaptotal - swapused;
	memshared = 0;
	mtx_lock(&vm_object_list_mtx);
	TAILQ_FOREACH(object, &vm_object_list, object_list)
		if (object->shadow_count > 1)
			memshared += object->resident_page_count;
	mtx_unlock(&vm_object_list_mtx);
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

#if defined(__i386__) || defined(__amd64__)
/*
 * Filler function for proc/cpuinfo (i386 & amd64 version)
 */
static int
linprocfs_docpuinfo(PFS_FILL_ARGS)
{
	int hw_model[2];
	char model[128];
	size_t size;
	int class, fqmhz, fqkhz;
	int i;

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
#ifdef __i386__
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
#else /* __amd64__ */
	default:
		class = 15;
		break;
#endif
	}

	hw_model[0] = CTL_HW;
	hw_model[1] = HW_MODEL;
	model[0] = '\0';
	size = sizeof(model);
	if (kernel_sysctl(td, hw_model, 2, &model, &size, 0, 0, 0, 0) != 0)
		strcpy(model, "unknown");
	for (i = 0; i < mp_ncpus; ++i) {
		sbuf_printf(sb,
		    "processor\t: %d\n"
		    "vendor_id\t: %.20s\n"
		    "cpu family\t: %d\n"
		    "model\t\t: %d\n"
		    "model name\t: %s\n"
		    "stepping\t: %d\n",
		    i, cpu_vendor, class, cpu, model, cpu_id & 0xf);
		/* XXX per-cpu vendor / class / model / id? */
	}

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
#endif /* __i386__ || __amd64__ */

/*
 * Filler function for proc/mtab
 *
 * This file doesn't exist in Linux' procfs, but is included here so
 * users can symlink /compat/linux/etc/mtab to /proc/mtab
 */
static int
linprocfs_domtab(PFS_FILL_ARGS)
{
	struct nameidata nd;
	struct mount *mp;
	const char *lep;
	char *dlep, *flep, *mntto, *mntfrom, *fstype;
	size_t lep_len;
	int error;

	/* resolve symlinks etc. in the emulation tree prefix */
	NDINIT(&nd, LOOKUP, FOLLOW | MPSAFE, UIO_SYSSPACE, linux_emul_path, td);
	flep = NULL;
	error = namei(&nd);
	VFS_UNLOCK_GIANT(NDHASGIANT(&nd));
	if (error != 0 || vn_fullpath(td, nd.ni_vp, &dlep, &flep) != 0)
		lep = linux_emul_path;
	else
		lep = dlep;
	lep_len = strlen(lep);

	mtx_lock(&mountlist_mtx);
	error = 0;
	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		/* determine device name */
		mntfrom = mp->mnt_stat.f_mntfromname;

		/* determine mount point */
		mntto = mp->mnt_stat.f_mntonname;
		if (strncmp(mntto, lep, lep_len) == 0 &&
		    mntto[lep_len] == '/')
			mntto += lep_len;

		/* determine fs type */
		fstype = mp->mnt_stat.f_fstypename;
		if (strcmp(fstype, pn->pn_info->pi_name) == 0)
			mntfrom = fstype = "proc";
		else if (strcmp(fstype, "procfs") == 0)
			continue;

		if (strcmp(fstype, "linsysfs") == 0) {
			sbuf_printf(sb, "/sys %s sysfs %s", mntto,
			    mp->mnt_stat.f_flags & MNT_RDONLY ? "ro" : "rw");
		} else {
			sbuf_printf(sb, "%s %s %s %s", mntfrom, mntto, fstype,
			    mp->mnt_stat.f_flags & MNT_RDONLY ? "ro" : "rw");
		}
#define ADD_OPTION(opt, name) \
	if (mp->mnt_stat.f_flags & (opt)) sbuf_printf(sb, "," name);
		ADD_OPTION(MNT_SYNCHRONOUS,	"sync");
		ADD_OPTION(MNT_NOEXEC,		"noexec");
		ADD_OPTION(MNT_NOSUID,		"nosuid");
		ADD_OPTION(MNT_UNION,		"union");
		ADD_OPTION(MNT_ASYNC,		"async");
		ADD_OPTION(MNT_SUIDDIR,		"suiddir");
		ADD_OPTION(MNT_NOSYMFOLLOW,	"nosymfollow");
		ADD_OPTION(MNT_NOATIME,		"noatime");
#undef ADD_OPTION
		/* a real Linux mtab will also show NFS options */
		sbuf_printf(sb, " 0 0\n");
	}
	mtx_unlock(&mountlist_mtx);
	if (flep != NULL)
		free(flep, M_TEMP);
	return (error);
}

/*
 * Filler function for proc/stat
 */
static int
linprocfs_dostat(PFS_FILL_ARGS)
{
	struct pcpu *pcpu;
	long cp_time[CPUSTATES];
	long *cp;
	int i;

	read_cpu_time(cp_time);
	sbuf_printf(sb, "cpu %ld %ld %ld %ld\n",
	    T2J(cp_time[CP_USER]),
	    T2J(cp_time[CP_NICE]),
	    T2J(cp_time[CP_SYS] /*+ cp_time[CP_INTR]*/),
	    T2J(cp_time[CP_IDLE]));
	for (i = 0; i <= mp_maxid; ++i) {
		if (CPU_ABSENT(i))
			continue;
		pcpu = pcpu_find(i);
		cp = pcpu->pc_cp_time;
		sbuf_printf(sb, "cpu%d %ld %ld %ld %ld\n", i,
		    T2J(cp[CP_USER]),
		    T2J(cp[CP_NICE]),
		    T2J(cp[CP_SYS] /*+ cp[CP_INTR]*/),
		    T2J(cp[CP_IDLE]));
	}
	sbuf_printf(sb,
	    "disk 0 0 0 0\n"
	    "page %u %u\n"
	    "swap %u %u\n"
	    "intr %u\n"
	    "ctxt %u\n"
	    "btime %lld\n",
	    cnt.v_vnodepgsin,
	    cnt.v_vnodepgsout,
	    cnt.v_swappgsin,
	    cnt.v_swappgsout,
	    cnt.v_intr,
	    cnt.v_swtch,
	    (long long)boottime.tv_sec);
	return (0);
}

/*
 * Filler function for proc/uptime
 */
static int
linprocfs_douptime(PFS_FILL_ARGS)
{
	long cp_time[CPUSTATES];
	struct timeval tv;

	getmicrouptime(&tv);
	read_cpu_time(cp_time);
	sbuf_printf(sb, "%lld.%02ld %ld.%02ld\n",
	    (long long)tv.tv_sec, tv.tv_usec / 10000,
	    T2S(cp_time[CP_IDLE]), T2J(cp_time[CP_IDLE]) % 100);
	return (0);
}

/*
 * Get OS build date
 */
static void
linprocfs_osbuild(struct thread *td, struct sbuf *sb)
{
#if 0
	char osbuild[256];
	char *cp1, *cp2;

	strncpy(osbuild, version, 256);
	osbuild[255] = '\0';
	cp1 = strstr(osbuild, "\n");
	cp2 = strstr(osbuild, ":");
	if (cp1 && cp2) {
		*cp1 = *cp2 = '\0';
		cp1 = strstr(osbuild, "#");
	} else
		cp1 = NULL;
	if (cp1)
		sbuf_printf(sb, "%s%s", cp1, cp2 + 1);
	else
#endif
		sbuf_cat(sb, "#4 Sun Dec 18 04:30:00 CET 1977");
}

/*
 * Get OS builder
 */
static void
linprocfs_osbuilder(struct thread *td, struct sbuf *sb)
{
#if 0
	char builder[256];
	char *cp;

	cp = strstr(version, "\n    ");
	if (cp) {
		strncpy(builder, cp + 5, 256);
		builder[255] = '\0';
		cp = strstr(builder, ":");
		if (cp)
			*cp = '\0';
	}
	if (cp)
		sbuf_cat(sb, builder);
	else
#endif
		sbuf_cat(sb, "des@freebsd.org");
}

/*
 * Filler function for proc/version
 */
static int
linprocfs_doversion(PFS_FILL_ARGS)
{
	char osname[LINUX_MAX_UTSNAME];
	char osrelease[LINUX_MAX_UTSNAME];

	linux_get_osname(td, osname);
	linux_get_osrelease(td, osrelease);
	sbuf_printf(sb, "%s version %s (", osname, osrelease);
	linprocfs_osbuilder(td, sb);
	sbuf_cat(sb, ") (gcc version " __VERSION__ ") ");
	linprocfs_osbuild(td, sb);
	sbuf_cat(sb, "\n");

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
	char state;
	static int ratelimit = 0;

	PROC_LOCK(p);
	fill_kinfo_proc(p, &kp);
	sbuf_printf(sb, "%d", p->p_pid);
#define PS_ADD(name, fmt, arg) sbuf_printf(sb, " " fmt, arg)
	PS_ADD("comm",		"(%s)",	p->p_comm);
	if (kp.ki_stat > sizeof(linux_state)) {
		state = 'R';

		if (ratelimit == 0) {
			printf("linprocfs: don't know how to handle unknown FreeBSD state %d/%zd, mapping to R\n",
			    kp.ki_stat, sizeof(linux_state));
			++ratelimit;
		}
	} else
		state = linux_state[kp.ki_stat - 1];
	PS_ADD("state",		"%c",	state);
	PS_ADD("ppid",		"%d",	p->p_pptr ? p->p_pptr->p_pid : 0);
	PS_ADD("pgrp",		"%d",	p->p_pgid);
	PS_ADD("session",	"%d",	p->p_session->s_sid);
	PROC_UNLOCK(p);
	PS_ADD("tty",		"%d",	0); /* XXX */
	PS_ADD("tpgid",		"%d",	kp.ki_tpgid);
	PS_ADD("flags",		"%u",	0); /* XXX */
	PS_ADD("minflt",	"%lu",	kp.ki_rusage.ru_minflt);
	PS_ADD("cminflt",	"%lu",	kp.ki_rusage_ch.ru_minflt);
	PS_ADD("majflt",	"%lu",	kp.ki_rusage.ru_majflt);
	PS_ADD("cmajflt",	"%lu",	kp.ki_rusage_ch.ru_majflt);
	PS_ADD("utime",		"%ld",	T2J(tvtohz(&kp.ki_rusage.ru_utime)));
	PS_ADD("stime",		"%ld",	T2J(tvtohz(&kp.ki_rusage.ru_stime)));
	PS_ADD("cutime",	"%ld",	T2J(tvtohz(&kp.ki_rusage_ch.ru_utime)));
	PS_ADD("cstime",	"%ld",	T2J(tvtohz(&kp.ki_rusage_ch.ru_stime)));
	PS_ADD("priority",	"%d",	kp.ki_pri.pri_user);
	PS_ADD("nice",		"%d",	kp.ki_nice); /* 19 (nicest) to -19 */
	PS_ADD("0",		"%d",	0); /* removed field */
	PS_ADD("itrealvalue",	"%d",	0); /* XXX */
	/* XXX: starttime is not right, it is the _same_ for _every_ process.
	   It should be the number of jiffies between system boot and process
	   start. */
	PS_ADD("starttime",	"%lu",	T2J(tvtohz(&kp.ki_start)));
	PS_ADD("vsize",		"%ju",	P2K((uintmax_t)kp.ki_size));
	PS_ADD("rss",		"%ju",	(uintmax_t)kp.ki_rssize);
	PS_ADD("rlim",		"%lu",	kp.ki_rusage.ru_maxrss);
	PS_ADD("startcode",	"%u",	(unsigned)0);
	PS_ADD("endcode",	"%u",	0); /* XXX */
	PS_ADD("startstack",	"%u",	0); /* XXX */
	PS_ADD("kstkesp",	"%u",	0); /* XXX */
	PS_ADD("kstkeip",	"%u",	0); /* XXX */
	PS_ADD("signal",	"%u",	0); /* XXX */
	PS_ADD("blocked",	"%u",	0); /* XXX */
	PS_ADD("sigignore",	"%u",	0); /* XXX */
	PS_ADD("sigcatch",	"%u",	0); /* XXX */
	PS_ADD("wchan",		"%u",	0); /* XXX */
	PS_ADD("nswap",		"%lu",	kp.ki_rusage.ru_nswap);
	PS_ADD("cnswap",	"%lu",	kp.ki_rusage_ch.ru_nswap);
	PS_ADD("exitsignal",	"%d",	0); /* XXX */
	PS_ADD("processor",	"%u",	kp.ki_lastcpu);
	PS_ADD("rt_priority",	"%u",	0); /* XXX */ /* >= 2.5.19 */
	PS_ADD("policy",	"%u",	kp.ki_pri.pri_class); /* >= 2.5.19 */
#undef PS_ADD
	sbuf_putc(sb, '\n');

	return (0);
}

/*
 * Filler function for proc/pid/statm
 */
static int
linprocfs_doprocstatm(PFS_FILL_ARGS)
{
	struct kinfo_proc kp;
	segsz_t lsize;

	PROC_LOCK(p);
	fill_kinfo_proc(p, &kp);
	PROC_UNLOCK(p);

	/*
	 * See comments in linprocfs_doprocstatus() regarding the
	 * computation of lsize.
	 */
	/* size resident share trs drs lrs dt */
	sbuf_printf(sb, "%ju ", B2P((uintmax_t)kp.ki_size));
	sbuf_printf(sb, "%ju ", (uintmax_t)kp.ki_rssize);
	sbuf_printf(sb, "%ju ", (uintmax_t)0); /* XXX */
	sbuf_printf(sb, "%ju ",	(uintmax_t)kp.ki_tsize);
	sbuf_printf(sb, "%ju ", (uintmax_t)(kp.ki_dsize + kp.ki_ssize));
	lsize = B2P(kp.ki_size) - kp.ki_dsize -
	    kp.ki_ssize - kp.ki_tsize - 1;
	sbuf_printf(sb, "%ju ", (uintmax_t)lsize);
	sbuf_printf(sb, "%ju\n", (uintmax_t)0); /* XXX */

	return (0);
}

/*
 * Filler function for proc/pid/status
 */
static int
linprocfs_doprocstatus(PFS_FILL_ARGS)
{
	struct kinfo_proc kp;
	char *state;
	segsz_t lsize;
	struct thread *td2;
	struct sigacts *ps;
	int i;

	PROC_LOCK(p);
	td2 = FIRST_THREAD_IN_PROC(p); /* XXXKSE pretend only one thread */

	if (P_SHOULDSTOP(p)) {
		state = "T (stopped)";
	} else {
		PROC_SLOCK(p);
		switch(p->p_state) {
		case PRS_NEW:
			state = "I (idle)";
			break;
		case PRS_NORMAL:
			if (p->p_flag & P_WEXIT) {
				state = "X (exiting)";
				break;
			}
			switch(td2->td_state) {
			case TDS_INHIBITED:
				state = "S (sleeping)";
				break;
			case TDS_RUNQ:
			case TDS_RUNNING:
				state = "R (running)";
				break;
			default:
				state = "? (unknown)";
				break;
			}
			break;
		case PRS_ZOMBIE:
			state = "Z (zombie)";
			break;
		default:
			state = "? (unknown)";
			break;
		}
		PROC_SUNLOCK(p);
	}

	fill_kinfo_proc(p, &kp);
	sbuf_printf(sb, "Name:\t%s\n",		p->p_comm); /* XXX escape */
	sbuf_printf(sb, "State:\t%s\n",		state);

	/*
	 * Credentials
	 */
	sbuf_printf(sb, "Pid:\t%d\n",		p->p_pid);
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
	sbuf_printf(sb, "VmSize:\t%8ju kB\n",	B2K((uintmax_t)kp.ki_size));
	sbuf_printf(sb, "VmLck:\t%8u kB\n",	P2K(0)); /* XXX */
	sbuf_printf(sb, "VmRss:\t%8ju kB\n",	P2K((uintmax_t)kp.ki_rssize));
	sbuf_printf(sb, "VmData:\t%8ju kB\n",	P2K((uintmax_t)kp.ki_dsize));
	sbuf_printf(sb, "VmStk:\t%8ju kB\n",	P2K((uintmax_t)kp.ki_ssize));
	sbuf_printf(sb, "VmExe:\t%8ju kB\n",	P2K((uintmax_t)kp.ki_tsize));
	lsize = B2P(kp.ki_size) - kp.ki_dsize -
	    kp.ki_ssize - kp.ki_tsize - 1;
	sbuf_printf(sb, "VmLib:\t%8ju kB\n",	P2K((uintmax_t)lsize));

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
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	sbuf_printf(sb, "SigIgn:\t%08x\n",	ps->ps_sigignore.__bits[0]);
	sbuf_printf(sb, "SigCgt:\t%08x\n",	ps->ps_sigcatch.__bits[0]);
	mtx_unlock(&ps->ps_mtx);
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
 * Filler function for proc/pid/cwd
 */
static int
linprocfs_doproccwd(PFS_FILL_ARGS)
{
	char *fullpath = "unknown";
	char *freepath = NULL;

	vn_fullpath(td, p->p_fd->fd_cdir, &fullpath, &freepath);
	sbuf_printf(sb, "%s", fullpath);
	if (freepath)
		free(freepath, M_TEMP);
	return (0);
}

/*
 * Filler function for proc/pid/root
 */
static int
linprocfs_doprocroot(PFS_FILL_ARGS)
{
	struct vnode *rvp;
	char *fullpath = "unknown";
	char *freepath = NULL;

	rvp = jailed(p->p_ucred) ? p->p_fd->fd_jdir : p->p_fd->fd_rdir;
	vn_fullpath(td, rvp, &fullpath, &freepath);
	sbuf_printf(sb, "%s", fullpath);
	if (freepath)
		free(freepath, M_TEMP);
	return (0);
}

/*
 * Filler function for proc/pid/cmdline
 */
static int
linprocfs_doproccmdline(PFS_FILL_ARGS)
{
	struct ps_strings pstr;
	char **ps_argvstr;
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

	PROC_LOCK(p);
	if (p->p_args && p_cansee(td, p) == 0) {
		sbuf_bcpy(sb, p->p_args->ar_args, p->p_args->ar_length);
		PROC_UNLOCK(p);
	} else if (p != td->td_proc) {
		PROC_UNLOCK(p);
		sbuf_printf(sb, "%.*s", MAXCOMLEN, p->p_comm);
	} else {
		PROC_UNLOCK(p);
		error = copyin((void *)p->p_sysent->sv_psstrings, &pstr,
		    sizeof(pstr));
		if (error)
			return (error);
		if (pstr.ps_nargvstr > ARG_MAX)
			return (E2BIG);
		ps_argvstr = malloc(pstr.ps_nargvstr * sizeof(char *),
		    M_TEMP, M_WAITOK);
		error = copyin((void *)pstr.ps_argvstr, ps_argvstr,
		    pstr.ps_nargvstr * sizeof(char *));
		if (error) {
			free(ps_argvstr, M_TEMP);
			return (error);
		}
		for (i = 0; i < pstr.ps_nargvstr; i++) {
			sbuf_copyin(sb, ps_argvstr[i], 0);
			sbuf_printf(sb, "%c", '\0');
		}
		free(ps_argvstr, M_TEMP);
	}

	return (0);
}

/*
 * Filler function for proc/pid/environ
 */
static int
linprocfs_doprocenviron(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "doprocenviron\n%c", '\0');
	return (0);
}

/*
 * Filler function for proc/pid/maps
 */
static int
linprocfs_doprocmaps(PFS_FILL_ARGS)
{
	char mebuffer[512];
	vm_map_t map = &p->p_vmspace->vm_map;
	vm_map_entry_t entry, tmp_entry;
	vm_object_t obj, tobj, lobj;
	vm_offset_t saved_end;
	vm_ooffset_t off = 0;
	char *name = "", *freename = NULL;
	size_t len;
	ino_t ino;
	unsigned int last_timestamp;
	int ref_count, shadow_count, flags;
	int error;
	struct vnode *vp;
	struct vattr vat;
	int locked;

	PROC_LOCK(p);
	error = p_candebug(td, p);
	PROC_UNLOCK(p);
	if (error)
		return (error);

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	if (uio->uio_offset != 0)
		return (0);

	error = 0;
	vm_map_lock_read(map);
	for (entry = map->header.next;
	    ((uio->uio_resid > 0) && (entry != &map->header));
	    entry = entry->next) {
		name = "";
		freename = NULL;
		if (entry->eflags & MAP_ENTRY_IS_SUB_MAP)
			continue;
		saved_end = entry->end;
		obj = entry->object.vm_object;
		for (lobj = tobj = obj; tobj; tobj = tobj->backing_object) {
			VM_OBJECT_LOCK(tobj);
			if (lobj != obj)
				VM_OBJECT_UNLOCK(lobj);
			lobj = tobj;
		}
		ino = 0;
		if (lobj) {
			off = IDX_TO_OFF(lobj->size);
			if (lobj->type == OBJT_VNODE) {
				vp = lobj->handle;
				if (vp)
					vref(vp);
			}
			else
				vp = NULL;
			if (lobj != obj)
				VM_OBJECT_UNLOCK(lobj);
			flags = obj->flags;
			ref_count = obj->ref_count;
			shadow_count = obj->shadow_count;
			VM_OBJECT_UNLOCK(obj);
			if (vp) {
				vn_fullpath(td, vp, &name, &freename);
				locked = VFS_LOCK_GIANT(vp->v_mount);
				vn_lock(vp, LK_SHARED | LK_RETRY, td);
				VOP_GETATTR(vp, &vat, td->td_ucred, td);
				ino = vat.va_fileid;
				vput(vp);
				VFS_UNLOCK_GIANT(locked);
			}
		} else {
			flags = 0;
			ref_count = 0;
			shadow_count = 0;
		}

		/*
		 * format:
		 *  start, end, access, offset, major, minor, inode, name.
		 */
		snprintf(mebuffer, sizeof mebuffer,
		    "%08lx-%08lx %s%s%s%s %08lx %02x:%02x %lu%s%s\n",
		    (u_long)entry->start, (u_long)entry->end,
		    (entry->protection & VM_PROT_READ)?"r":"-",
		    (entry->protection & VM_PROT_WRITE)?"w":"-",
		    (entry->protection & VM_PROT_EXECUTE)?"x":"-",
		    "p",
		    (u_long)off,
		    0,
		    0,
		    (u_long)ino,
		    *name ? "     " : "",
		    name
		    );
		if (freename)
			free(freename, M_TEMP);
		len = strlen(mebuffer);
		if (len > uio->uio_resid)
			len = uio->uio_resid; /*
					       * XXX We should probably return
					       * EFBIG here, as in procfs.
					       */
		last_timestamp = map->timestamp;
		vm_map_unlock_read(map);
		error = uiomove(mebuffer, len, uio);
		vm_map_lock_read(map);
		if (error)
			break;
		if (last_timestamp + 1 != map->timestamp) {
			/*
			 * Look again for the entry because the map was
			 * modified while it was unlocked.  Specifically,
			 * the entry may have been clipped, merged, or deleted.
			 */
			vm_map_lookup_entry(map, saved_end - 1, &tmp_entry);
			entry = tmp_entry;
		}
	}
	vm_map_unlock_read(map);

	return (error);
}

/*
 * Filler function for proc/net/dev
 */
static int
linprocfs_donetdev(PFS_FILL_ARGS)
{
	char ifname[16]; /* XXX LINUX_IFNAMSIZ */
	struct ifnet *ifp;

	sbuf_printf(sb, "%6s|%58s|%s\n%6s|%58s|%58s\n",
	    "Inter-", "   Receive", "  Transmit", " face",
	    "bytes    packets errs drop fifo frame compressed",
	    "bytes    packets errs drop fifo frame compressed");

	IFNET_RLOCK();
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		linux_ifname(ifp, ifname, sizeof ifname);
			sbuf_printf(sb, "%6.6s:", ifname);
		sbuf_printf(sb, "%8lu %7lu %4lu %4lu %4lu %5lu %10lu %9lu ",
		    0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL);
		sbuf_printf(sb, "%8lu %7lu %4lu %4lu %4lu %5lu %7lu %10lu\n",
		    0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL);
	}
	IFNET_RUNLOCK();

	return (0);
}

/*
 * Filler function for proc/sys/kernel/osrelease
 */
static int
linprocfs_doosrelease(PFS_FILL_ARGS)
{
	char osrelease[LINUX_MAX_UTSNAME];

	linux_get_osrelease(td, osrelease);
	sbuf_printf(sb, "%s\n", osrelease);

	return (0);
}

/*
 * Filler function for proc/sys/kernel/ostype
 */
static int
linprocfs_doostype(PFS_FILL_ARGS)
{
	char osname[LINUX_MAX_UTSNAME];

	linux_get_osname(td, osname);
	sbuf_printf(sb, "%s\n", osname);

	return (0);
}

/*
 * Filler function for proc/sys/kernel/version
 */
static int
linprocfs_doosbuild(PFS_FILL_ARGS)
{

	linprocfs_osbuild(td, sb);
	sbuf_cat(sb, "\n");
	return (0);
}

/*
 * Filler function for proc/sys/kernel/msgmni
 */
static int
linprocfs_domsgmni(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%d\n", msginfo.msgmni);
	return (0);
}

/*
 * Filler function for proc/sys/kernel/pid_max
 */
static int
linprocfs_dopid_max(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%i\n", PID_MAX);
	return (0);
}

/*
 * Filler function for proc/sys/kernel/sem
 */
static int
linprocfs_dosem(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%d %d %d %d\n", seminfo.semmsl, seminfo.semmns,
	    seminfo.semopm, seminfo.semmni);
	return (0);
}

/*
 * Filler function for proc/scsi/device_info
 */
static int
linprocfs_doscsidevinfo(PFS_FILL_ARGS)
{

	return (0);
}

/*
 * Filler function for proc/scsi/scsi
 */
static int
linprocfs_doscsiscsi(PFS_FILL_ARGS)
{

	return (0);
}

extern struct cdevsw *cdevsw[];

/*
 * Filler function for proc/devices
 */
static int
linprocfs_dodevices(PFS_FILL_ARGS)
{
	char *char_devices;
	sbuf_printf(sb, "Character devices:\n");

	char_devices = linux_get_char_devices();
	sbuf_printf(sb, "%s", char_devices);
	linux_free_get_char_devices(char_devices);

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

#if 0
/*
 * Filler function for proc/modules
 */
static int
linprocfs_domodules(PFS_FILL_ARGS)
{
	struct linker_file *lf;

	TAILQ_FOREACH(lf, &linker_files, link) {
		sbuf_printf(sb, "%-20s%8lu%4d\n", lf->filename,
		    (unsigned long)lf->size, lf->refs);
	}
	return (0);
}
#endif

/*
 * Constructor
 */
static int
linprocfs_init(PFS_INIT_ARGS)
{
	struct pfs_node *root;
	struct pfs_node *dir;

	root = pi->pi_root;

	/* /proc/... */
	pfs_create_file(root, "cmdline", &linprocfs_docmdline,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "cpuinfo", &linprocfs_docpuinfo,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "devices", &linprocfs_dodevices,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "loadavg", &linprocfs_doloadavg,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "meminfo", &linprocfs_domeminfo,
	    NULL, NULL, NULL, PFS_RD);
#if 0
	pfs_create_file(root, "modules", &linprocfs_domodules,
	    NULL, NULL, NULL, PFS_RD);
#endif
	pfs_create_file(root, "mounts", &linprocfs_domtab,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "mtab", &linprocfs_domtab,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_link(root, "self", &procfs_docurproc,
	    NULL, NULL, NULL, 0);
	pfs_create_file(root, "stat", &linprocfs_dostat,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "uptime", &linprocfs_douptime,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "version", &linprocfs_doversion,
	    NULL, NULL, NULL, PFS_RD);

	/* /proc/net/... */
	dir = pfs_create_dir(root, "net", NULL, NULL, NULL, 0);
	pfs_create_file(dir, "dev", &linprocfs_donetdev,
	    NULL, NULL, NULL, PFS_RD);

	/* /proc/<pid>/... */
	dir = pfs_create_dir(root, "pid", NULL, NULL, NULL, PFS_PROCDEP);
	pfs_create_file(dir, "cmdline", &linprocfs_doproccmdline,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_link(dir, "cwd", &linprocfs_doproccwd,
	    NULL, NULL, NULL, 0);
	pfs_create_file(dir, "environ", &linprocfs_doprocenviron,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_link(dir, "exe", &procfs_doprocfile,
	    NULL, &procfs_notsystem, NULL, 0);
	pfs_create_file(dir, "maps", &linprocfs_doprocmaps,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "mem", &procfs_doprocmem,
	    &procfs_attr, &procfs_candebug, NULL, PFS_RDWR|PFS_RAW);
	pfs_create_link(dir, "root", &linprocfs_doprocroot,
	    NULL, NULL, NULL, 0);
	pfs_create_file(dir, "stat", &linprocfs_doprocstat,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "statm", &linprocfs_doprocstatm,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "status", &linprocfs_doprocstatus,
	    NULL, NULL, NULL, PFS_RD);

	/* /proc/scsi/... */
	dir = pfs_create_dir(root, "scsi", NULL, NULL, NULL, 0);
	pfs_create_file(dir, "device_info", &linprocfs_doscsidevinfo,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "scsi", &linprocfs_doscsiscsi,
	    NULL, NULL, NULL, PFS_RD);

	/* /proc/sys/... */
	dir = pfs_create_dir(root, "sys", NULL, NULL, NULL, 0);
	/* /proc/sys/kernel/... */
	dir = pfs_create_dir(dir, "kernel", NULL, NULL, NULL, 0);
	pfs_create_file(dir, "osrelease", &linprocfs_doosrelease,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "ostype", &linprocfs_doostype,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "version", &linprocfs_doosbuild,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "msgmni", &linprocfs_domsgmni,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "pid_max", &linprocfs_dopid_max,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "sem", &linprocfs_dosem,
	    NULL, NULL, NULL, PFS_RD);

	return (0);
}

/*
 * Destructor
 */
static int
linprocfs_uninit(PFS_INIT_ARGS)
{

	/* nothing to do, pseudofs will GC */
	return (0);
}

PSEUDOFS(linprocfs, 1);
MODULE_DEPEND(linprocfs, linux, 1, 1, 1);
MODULE_DEPEND(linprocfs, procfs, 1, 1, 1);
MODULE_DEPEND(linprocfs, sysvmsg, 1, 1, 1);
MODULE_DEPEND(linprocfs, sysvsem, 1, 1, 1);
