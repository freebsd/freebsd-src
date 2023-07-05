/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
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

#include "opt_inet.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/blist.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/msg.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/resourcevar.h>
#include <sys/resource.h>
#include <sys/sbuf.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/tty.h>
#include <sys/user.h>
#include <sys/uuid.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>
#include <sys/bus.h>
#include <sys/uio.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>

#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/route_ctl.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/swap_pager.h>

#include <machine/clock.h>

#include <geom/geom.h>
#include <geom/geom_int.h>

#if defined(__i386__) || defined(__amd64__)
#include <machine/cputypes.h>
#include <machine/md_var.h>
#endif /* __i386__ || __amd64__ */

#include <compat/linux/linux.h>
#include <compat/linux/linux_common.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_util.h>
#include <fs/pseudofs/pseudofs.h>
#include <fs/procfs/procfs.h>

/*
 * Various conversion macros
 */
#define T2J(x) ((long)(((x) * 100ULL) / (stathz ? stathz : hz)))	/* ticks to jiffies */
#define T2CS(x) ((unsigned long)(((x) * 100ULL) / (stathz ? stathz : hz)))	/* ticks to centiseconds */
#define T2S(x) ((x) / (stathz ? stathz : hz))		/* ticks to seconds */
#define B2K(x) ((x) >> 10)				/* bytes to kbytes */
#define B2P(x) ((x) >> PAGE_SHIFT)			/* bytes to pages */
#define P2B(x) ((x) << PAGE_SHIFT)			/* pages to bytes */
#define P2K(x) ((x) << (PAGE_SHIFT - 10))		/* pages to kbytes */
#define TV2J(x)	((x)->tv_sec * 100UL + (x)->tv_usec / 10000)

/**
 * @brief Mapping of ki_stat in struct kinfo_proc to the linux state
 *
 * The linux procfs state field displays one of the characters RSDZTW to
 * denote running, sleeping in an interruptible wait, waiting in an
 * uninterruptible disk sleep, a zombie process, process is being traced
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
	unsigned long memfree;		/* free memory in bytes */
	unsigned long cached;		/* page cache */
	unsigned long buffers;		/* buffer cache */
	unsigned long long swaptotal;	/* total swap space in bytes */
	unsigned long long swapused;	/* used swap space in bytes */
	unsigned long long swapfree;	/* free swap space in bytes */
	size_t sz;
	int error, i, j;

	memtotal = physmem * PAGE_SIZE;
	memfree = (unsigned long)vm_free_count() * PAGE_SIZE;
	swap_pager_status(&i, &j);
	swaptotal = (unsigned long long)i * PAGE_SIZE;
	swapused = (unsigned long long)j * PAGE_SIZE;
	swapfree = swaptotal - swapused;

	/*
	 * This value may exclude wired pages, but we have no good way of
	 * accounting for that.
	 */
	cached =
	    (vm_active_count() + vm_inactive_count() + vm_laundry_count()) *
	    PAGE_SIZE;

	sz = sizeof(buffers);
	error = kernel_sysctlbyname(curthread, "vfs.bufspace", &buffers, &sz,
	    NULL, 0, 0, 0);
	if (error != 0)
		buffers = 0;

	sbuf_printf(sb,
	    "MemTotal: %9lu kB\n"
	    "MemFree:  %9lu kB\n"
	    "Buffers:  %9lu kB\n"
	    "Cached:   %9lu kB\n"
	    "SwapTotal:%9llu kB\n"
	    "SwapFree: %9llu kB\n",
	    B2K(memtotal), B2K(memfree), B2K(buffers),
	    B2K(cached), B2K(swaptotal), B2K(swapfree));

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
	uint64_t freq;
	size_t size;
	u_int cache_size[4];
	u_int regs[4] = { 0 };
	int fqmhz, fqkhz;
	int i, j;

	/*
	 * We default the flags to include all non-conflicting flags,
	 * and the Intel versions of conflicting flags.
	 */
	static char *cpu_feature_names[] = {
		/*  0 */ "fpu", "vme", "de", "pse",
		/*  4 */ "tsc", "msr", "pae", "mce",
		/*  8 */ "cx8", "apic", "", "sep",
		/* 12 */ "mtrr", "pge", "mca", "cmov",
		/* 16 */ "pat", "pse36", "pn", "clflush",
		/* 20 */ "", "dts", "acpi", "mmx",
		/* 24 */ "fxsr", "sse", "sse2", "ss",
		/* 28 */ "ht", "tm", "ia64", "pbe"
	};

	static char *amd_feature_names[] = {
		/*  0 */ "", "", "", "",
		/*  4 */ "", "", "", "",
		/*  8 */ "", "", "", "syscall",
		/* 12 */ "", "", "", "",
		/* 16 */ "", "", "", "mp",
		/* 20 */ "nx", "", "mmxext", "",
		/* 24 */ "", "fxsr_opt", "pdpe1gb", "rdtscp",
		/* 28 */ "", "lm", "3dnowext", "3dnow"
	};

	static char *cpu_feature2_names[] = {
		/*  0 */ "pni", "pclmulqdq", "dtes64", "monitor",
		/*  4 */ "ds_cpl", "vmx", "smx", "est",
		/*  8 */ "tm2", "ssse3", "cid", "sdbg",
		/* 12 */ "fma", "cx16", "xtpr", "pdcm",
		/* 16 */ "", "pcid", "dca", "sse4_1",
		/* 20 */ "sse4_2", "x2apic", "movbe", "popcnt",
		/* 24 */ "tsc_deadline_timer", "aes", "xsave", "",
		/* 28 */ "avx", "f16c", "rdrand", "hypervisor"
	};

	static char *amd_feature2_names[] = {
		/*  0 */ "lahf_lm", "cmp_legacy", "svm", "extapic",
		/*  4 */ "cr8_legacy", "abm", "sse4a", "misalignsse",
		/*  8 */ "3dnowprefetch", "osvw", "ibs", "xop",
		/* 12 */ "skinit", "wdt", "", "lwp",
		/* 16 */ "fma4", "tce", "", "nodeid_msr",
		/* 20 */ "", "tbm", "topoext", "perfctr_core",
		/* 24 */ "perfctr_nb", "", "bpext", "ptsc",
		/* 28 */ "perfctr_llc", "mwaitx", "", ""
	};

	static char *cpu_stdext_feature_names[] = {
		/*  0 */ "fsgsbase", "tsc_adjust", "sgx", "bmi1",
		/*  4 */ "hle", "avx2", "", "smep",
		/*  8 */ "bmi2", "erms", "invpcid", "rtm",
		/* 12 */ "cqm", "", "mpx", "rdt_a",
		/* 16 */ "avx512f", "avx512dq", "rdseed", "adx",
		/* 20 */ "smap", "avx512ifma", "", "clflushopt",
		/* 24 */ "clwb", "intel_pt", "avx512pf", "avx512er",
		/* 28 */ "avx512cd", "sha_ni", "avx512bw", "avx512vl"
	};

	static char *cpu_stdext_feature2_names[] = {
		/*  0 */ "prefetchwt1", "avx512vbmi", "umip", "pku",
		/*  4 */ "ospke", "waitpkg", "avx512_vbmi2", "",
		/*  8 */ "gfni", "vaes", "vpclmulqdq", "avx512_vnni",
		/* 12 */ "avx512_bitalg", "", "avx512_vpopcntdq", "",
		/* 16 */ "", "", "", "",
		/* 20 */ "", "", "rdpid", "",
		/* 24 */ "", "cldemote", "", "movdiri",
		/* 28 */ "movdir64b", "enqcmd", "sgx_lc", ""
	};

	static char *cpu_stdext_feature3_names[] = {
		/*  0 */ "", "", "avx512_4vnniw", "avx512_4fmaps",
		/*  4 */ "fsrm", "", "", "",
		/*  8 */ "avx512_vp2intersect", "", "md_clear", "",
		/* 12 */ "", "", "", "",
		/* 16 */ "", "", "pconfig", "",
		/* 20 */ "", "", "", "",
		/* 24 */ "", "", "ibrs", "stibp",
		/* 28 */ "flush_l1d", "arch_capabilities", "core_capabilities", "ssbd"
	};

	static char *cpu_stdext_feature_l1_names[] = {
		/*  0 */ "xsaveopt", "xsavec", "xgetbv1", "xsaves",
		/*  4 */ "xfd"
	};

	static char *power_flags[] = {
		"ts",           "fid",          "vid",
		"ttp",          "tm",           "stc",
		"100mhzsteps",  "hwpstate",     "",
		"cpb",          "eff_freq_ro",  "proc_feedback",
		"acc_power",
	};

	hw_model[0] = CTL_HW;
	hw_model[1] = HW_MODEL;
	model[0] = '\0';
	size = sizeof(model);
	if (kernel_sysctl(td, hw_model, 2, &model, &size, 0, 0, 0, 0) != 0)
		strcpy(model, "unknown");
#ifdef __i386__
	switch (cpu_vendor_id) {
	case CPU_VENDOR_AMD:
		if (cpu_class < CPUCLASS_686)
			cpu_feature_names[16] = "fcmov";
		break;
	case CPU_VENDOR_CYRIX:
		cpu_feature_names[24] = "cxmmx";
		break;
	}
#endif
	if (cpu_exthigh >= 0x80000006)
		do_cpuid(0x80000006, cache_size);
	else
		memset(cache_size, 0, sizeof(cache_size));
	for (i = 0; i < mp_ncpus; ++i) {
		fqmhz = 0;
		fqkhz = 0;
		freq = atomic_load_acq_64(&tsc_freq);
		if (freq != 0) {
			fqmhz = (freq + 4999) / 1000000;
			fqkhz = ((freq + 4999) / 10000) % 100;
		}
		sbuf_printf(sb,
		    "processor\t: %d\n"
		    "vendor_id\t: %.20s\n"
		    "cpu family\t: %u\n"
		    "model\t\t: %u\n"
		    "model name\t: %s\n"
		    "stepping\t: %u\n"
		    "cpu MHz\t\t: %d.%02d\n"
		    "cache size\t: %d KB\n"
		    "physical id\t: %d\n"
		    "siblings\t: %d\n"
		    "core id\t\t: %d\n"
		    "cpu cores\t: %d\n"
		    "apicid\t\t: %d\n"
		    "initial apicid\t: %d\n"
		    "fpu\t\t: %s\n"
		    "fpu_exception\t: %s\n"
		    "cpuid level\t: %d\n"
		    "wp\t\t: %s\n",
		    i, cpu_vendor, CPUID_TO_FAMILY(cpu_id),
		    CPUID_TO_MODEL(cpu_id), model, cpu_id & CPUID_STEPPING,
		    fqmhz, fqkhz,
		    (cache_size[2] >> 16), 0, mp_ncpus, i, mp_ncpus,
		    i, i, /*cpu_id & CPUID_LOCAL_APIC_ID ??*/
		    (cpu_feature & CPUID_FPU) ? "yes" : "no", "yes",
		    CPUID_TO_FAMILY(cpu_id), "yes");
		sbuf_cat(sb, "flags\t\t:");
		for (j = 0; j < nitems(cpu_feature_names); j++)
			if (cpu_feature & (1 << j) &&
			    cpu_feature_names[j][0] != '\0')
				sbuf_printf(sb, " %s", cpu_feature_names[j]);
		for (j = 0; j < nitems(amd_feature_names); j++)
			if (amd_feature & (1 << j) &&
			    amd_feature_names[j][0] != '\0')
				sbuf_printf(sb, " %s", amd_feature_names[j]);
		for (j = 0; j < nitems(cpu_feature2_names); j++)
			if (cpu_feature2 & (1 << j) &&
			    cpu_feature2_names[j][0] != '\0')
				sbuf_printf(sb, " %s", cpu_feature2_names[j]);
		for (j = 0; j < nitems(amd_feature2_names); j++)
			if (amd_feature2 & (1 << j) &&
			    amd_feature2_names[j][0] != '\0')
				sbuf_printf(sb, " %s", amd_feature2_names[j]);
		for (j = 0; j < nitems(cpu_stdext_feature_names); j++)
			if (cpu_stdext_feature & (1 << j) &&
			    cpu_stdext_feature_names[j][0] != '\0')
				sbuf_printf(sb, " %s",
				    cpu_stdext_feature_names[j]);
		if (tsc_is_invariant)
			sbuf_cat(sb, " constant_tsc");
		for (j = 0; j < nitems(cpu_stdext_feature2_names); j++)
			if (cpu_stdext_feature2 & (1 << j) &&
			    cpu_stdext_feature2_names[j][0] != '\0')
				sbuf_printf(sb, " %s",
				    cpu_stdext_feature2_names[j]);
		for (j = 0; j < nitems(cpu_stdext_feature3_names); j++)
			if (cpu_stdext_feature3 & (1 << j) &&
			    cpu_stdext_feature3_names[j][0] != '\0')
				sbuf_printf(sb, " %s",
				    cpu_stdext_feature3_names[j]);
		if ((cpu_feature2 & CPUID2_XSAVE) != 0) {
			cpuid_count(0xd, 0x1, regs);
			for (j = 0; j < nitems(cpu_stdext_feature_l1_names); j++)
				if (regs[0] & (1 << j) &&
				    cpu_stdext_feature_l1_names[j][0] != '\0')
					sbuf_printf(sb, " %s",
					    cpu_stdext_feature_l1_names[j]);
		}
		sbuf_cat(sb, "\n");
		sbuf_printf(sb,
		    "bugs\t\t: %s\n"
		    "bogomips\t: %d.%02d\n"
		    "clflush size\t: %d\n"
		    "cache_alignment\t: %d\n"
		    "address sizes\t: %d bits physical, %d bits virtual\n",
#if defined(I586_CPU) && !defined(NO_F00F_HACK)
		    (has_f00f_bug) ? "Intel F00F" : "",
#else
		    "",
#endif
		    fqmhz * 2, fqkhz,
		    cpu_clflush_line_size, cpu_clflush_line_size,
		    cpu_maxphyaddr,
		    (cpu_maxphyaddr > 32) ? 48 : 0);
		sbuf_cat(sb, "power management: ");
		for (j = 0; j < nitems(power_flags); j++)
			if (amd_pminfo & (1 << j))
				sbuf_printf(sb, " %s", power_flags[j]);
		sbuf_cat(sb, "\n\n");

		/* XXX per-cpu vendor / class / model / id? */
	}
	sbuf_cat(sb, "\n");

	return (0);
}
#else
/* ARM64TODO: implement non-stubbed linprocfs_docpuinfo */
static int
linprocfs_docpuinfo(PFS_FILL_ARGS)
{
	int i;

	for (i = 0; i < mp_ncpus; ++i) {
		sbuf_printf(sb,
		    "processor\t: %d\n"
		    "BogoMIPS\t: %d.%02d\n",
		    i, 0, 0);
		sbuf_cat(sb, "Features\t: ");
		sbuf_cat(sb, "\n");
		sbuf_printf(sb,
		    "CPU implementer\t: \n"
		    "CPU architecture: \n"
		    "CPU variant\t: 0x%x\n"
		    "CPU part\t: 0x%x\n"
		    "CPU revision\t: %d\n",
		    0, 0, 0);
		sbuf_cat(sb, "\n");
	}

	return (0);
}
#endif /* __i386__ || __amd64__ */

static const char *path_slash_sys = "/sys";
static const char *fstype_sysfs = "sysfs";

static int
_mtab_helper(const struct pfs_node *pn, const struct statfs *sp,
    const char **mntfrom, const char **mntto, const char **fstype)
{
	/* determine device name */
	*mntfrom = sp->f_mntfromname;

	/* determine mount point */
	*mntto = sp->f_mntonname;

	/* determine fs type */
	*fstype = sp->f_fstypename;
	if (strcmp(*fstype, pn->pn_info->pi_name) == 0)
		*mntfrom = *fstype = "proc";
	else if (strcmp(*fstype, "procfs") == 0)
		return (ECANCELED);

	if (strcmp(*fstype, "autofs") == 0) {
		/*
		 * FreeBSD uses eg "map -hosts", whereas Linux
		 * expects just "-hosts".
		 */
		if (strncmp(*mntfrom, "map ", 4) == 0)
			*mntfrom += 4;
	}

	if (strcmp(*fstype, "linsysfs") == 0) {
		*mntfrom = path_slash_sys;
		*fstype = fstype_sysfs;
	} else {
		/* For Linux msdosfs is called vfat */
		if (strcmp(*fstype, "msdosfs") == 0)
			*fstype = "vfat";
	}
	return (0);
}

static void
_sbuf_mntoptions_helper(struct sbuf *sb, uint64_t f_flags)
{
	sbuf_cat(sb, (f_flags & MNT_RDONLY) ? "ro" : "rw");
#define ADD_OPTION(opt, name) \
	if (f_flags & (opt)) sbuf_cat(sb, "," name);
	ADD_OPTION(MNT_SYNCHRONOUS,	"sync");
	ADD_OPTION(MNT_NOEXEC,		"noexec");
	ADD_OPTION(MNT_NOSUID,		"nosuid");
	ADD_OPTION(MNT_UNION,		"union");
	ADD_OPTION(MNT_ASYNC,		"async");
	ADD_OPTION(MNT_SUIDDIR,		"suiddir");
	ADD_OPTION(MNT_NOSYMFOLLOW,	"nosymfollow");
	ADD_OPTION(MNT_NOATIME,		"noatime");
#undef ADD_OPTION
}

/*
 * Filler function for proc/mtab and proc/<pid>/mounts.
 *
 * /proc/mtab doesn't exist in Linux' procfs, but is included here so
 * users can symlink /compat/linux/etc/mtab to /proc/mtab
 */
static int
linprocfs_domtab(PFS_FILL_ARGS)
{
	const char *mntto, *mntfrom, *fstype;
	char *dlep, *flep;
	struct vnode *vp;
	struct pwd *pwd;
	size_t lep_len;
	int error;
	struct statfs *buf, *sp;
	size_t count;

	/*
	 * Resolve emulation tree prefix
	 */
	flep = NULL;
	pwd = pwd_hold(td);
	vp = pwd->pwd_adir;
	error = vn_fullpath_global(vp, &dlep, &flep);
	pwd_drop(pwd);
	if (error != 0)
		return (error);
	lep_len = strlen(dlep);

	buf = NULL;
	error = kern_getfsstat(td, &buf, SIZE_T_MAX, &count,
	    UIO_SYSSPACE, MNT_WAIT);
	if (error != 0) {
		free(buf, M_TEMP);
		free(flep, M_TEMP);
		return (error);
	}

	for (sp = buf; count > 0; sp++, count--) {
		error = _mtab_helper(pn, sp, &mntfrom, &mntto, &fstype);
		if (error != 0) {
			MPASS(error == ECANCELED);
			continue;
		}

		/* determine mount point */
		if (strncmp(mntto, dlep, lep_len) == 0 && mntto[lep_len] == '/')
			mntto += lep_len;

		sbuf_printf(sb, "%s %s %s ", mntfrom, mntto, fstype);
		_sbuf_mntoptions_helper(sb, sp->f_flags);
		/* a real Linux mtab will also show NFS options */
		sbuf_printf(sb, " 0 0\n");
	}

	free(buf, M_TEMP);
	free(flep, M_TEMP);
	return (error);
}

static int
linprocfs_doprocmountinfo(PFS_FILL_ARGS)
{
	const char *mntfrom, *mntto, *fstype;
	char *dlep, *flep;
	struct statfs *buf, *sp;
	size_t count, lep_len;
	struct vnode *vp;
	struct pwd *pwd;
	int error;

	/*
	 * Resolve emulation tree prefix
	 */
	flep = NULL;
	pwd = pwd_hold(td);
	vp = pwd->pwd_adir;
	error = vn_fullpath_global(vp, &dlep, &flep);
	pwd_drop(pwd);
	if (error != 0)
		return (error);
	lep_len = strlen(dlep);

	buf = NULL;
	error = kern_getfsstat(td, &buf, SIZE_T_MAX, &count,
	    UIO_SYSSPACE, MNT_WAIT);
	if (error != 0)
		goto out;

	for (sp = buf; count > 0; sp++, count--) {
		error = _mtab_helper(pn, sp, &mntfrom, &mntto, &fstype);
		if (error != 0) {
			MPASS(error == ECANCELED);
			continue;
		}

		if (strncmp(mntto, dlep, lep_len) == 0 && mntto[lep_len] == '/')
			mntto += lep_len;
#if 0
		/*
		 * If the prefix is a chroot, and this mountpoint is not under
		 * the prefix, we should skip it.  Leave it for now for
		 * consistency with procmtab above.
		 */
		else
			continue;
#endif

		/*
		 * (1) mount id
		 *
		 * (2) parent mount id -- we don't have this cheaply, so
		 * provide a dummy value
		 *
		 * (3) major:minor -- ditto
		 *
		 * (4) root filesystem mount -- probably a namespaces thing
		 *
		 * (5) mountto path
		 */
		sbuf_printf(sb, "%u 0 0:0 / %s ",
		    sp->f_fsid.val[0] ^ sp->f_fsid.val[1], mntto);
		/* (6) mount options */
		_sbuf_mntoptions_helper(sb, sp->f_flags);
		/*
		 * (7) zero or more optional fields -- again, namespace related
		 *
		 * (8) End of variable length fields separator ("-")
		 *
		 * (9) fstype
		 *
		 * (10) mount from
		 *
		 * (11) "superblock" options -- like (6), but different
		 * semantics in Linux
		 */
		sbuf_printf(sb, " - %s %s %s\n", fstype, mntfrom,
		    (sp->f_flags & MNT_RDONLY) ? "ro" : "rw");
	}

	error = 0;
out:
	free(buf, M_TEMP);
	free(flep, M_TEMP);
	return (error);
}

/*
 * Filler function for proc/partitions
 */
static int
linprocfs_dopartitions(PFS_FILL_ARGS)
{
	struct g_class *cp;
	struct g_geom *gp;
	struct g_provider *pp;
	int major, minor;

	g_topology_lock();
	sbuf_printf(sb, "major minor  #blocks  name rio rmerge rsect "
	    "ruse wio wmerge wsect wuse running use aveq\n");

	LIST_FOREACH(cp, &g_classes, class) {
		if (strcmp(cp->name, "DISK") == 0 ||
		    strcmp(cp->name, "PART") == 0)
			LIST_FOREACH(gp, &cp->geom, geom) {
				LIST_FOREACH(pp, &gp->provider, provider) {
					if (linux_driver_get_major_minor(
					    pp->name, &major, &minor) != 0) {
						major = 0;
						minor = 0;
					}
					sbuf_printf(sb, "%d %d %lld %s "
					    "%d %d %d %d %d "
					     "%d %d %d %d %d %d\n",
					     major, minor,
					     (long long)pp->mediasize, pp->name,
					     0, 0, 0, 0, 0,
					     0, 0, 0, 0, 0, 0);
				}
			}
	}
	g_topology_unlock();

	return (0);
}

/*
 * Filler function for proc/stat
 *
 * Output depends on kernel version:
 *
 * v2.5.40 <=
 *   user nice system idle
 * v2.5.41
 *   user nice system idle iowait
 * v2.6.11
 *   user nice system idle iowait irq softirq steal
 * v2.6.24
 *   user nice system idle iowait irq softirq steal guest
 * v2.6.33 >=
 *   user nice system idle iowait irq softirq steal guest guest_nice
 */
static int
linprocfs_dostat(PFS_FILL_ARGS)
{
	struct pcpu *pcpu;
	long cp_time[CPUSTATES];
	long *cp;
	struct timeval boottime;
	int i;
	char *zero_pad;
	bool has_intr = true;

	if (linux_kernver(td) >= LINUX_KERNVER(2,6,33)) {
		zero_pad = " 0 0 0 0\n";
	} else if (linux_kernver(td) >= LINUX_KERNVER(2,6,24)) {
		zero_pad = " 0 0 0\n";
	} else if (linux_kernver(td) >= LINUX_KERNVER(2,6,11)) {
		zero_pad = " 0 0\n";
	} else if (linux_kernver(td) >= LINUX_KERNVER(2,5,41)) {
		has_intr = false;
		zero_pad = " 0\n";
	} else {
		has_intr = false;
		zero_pad = "\n";
	}

	read_cpu_time(cp_time);
	getboottime(&boottime);
	/* Parameters common to all versions */
	sbuf_printf(sb, "cpu %lu %lu %lu %lu",
	    T2J(cp_time[CP_USER]),
	    T2J(cp_time[CP_NICE]),
	    T2J(cp_time[CP_SYS]),
	    T2J(cp_time[CP_IDLE]));

	/* Print interrupt stats if available */
	if (has_intr) {
		sbuf_printf(sb, " 0 %lu", T2J(cp_time[CP_INTR]));
	}

	/* Pad out remaining fields depending on version */
	sbuf_printf(sb, "%s", zero_pad);

	CPU_FOREACH(i) {
		pcpu = pcpu_find(i);
		cp = pcpu->pc_cp_time;
		sbuf_printf(sb, "cpu%d %lu %lu %lu %lu", i,
		    T2J(cp[CP_USER]),
		    T2J(cp[CP_NICE]),
		    T2J(cp[CP_SYS]),
		    T2J(cp[CP_IDLE]));

		if (has_intr) {
			sbuf_printf(sb, " 0 %lu", T2J(cp[CP_INTR]));
		}

		sbuf_printf(sb, "%s", zero_pad);
	}
	sbuf_printf(sb,
	    "disk 0 0 0 0\n"
	    "page %ju %ju\n"
	    "swap %ju %ju\n"
	    "intr %ju\n"
	    "ctxt %ju\n"
	    "btime %lld\n",
	    (uintmax_t)VM_CNT_FETCH(v_vnodepgsin),
	    (uintmax_t)VM_CNT_FETCH(v_vnodepgsout),
	    (uintmax_t)VM_CNT_FETCH(v_swappgsin),
	    (uintmax_t)VM_CNT_FETCH(v_swappgsout),
	    (uintmax_t)VM_CNT_FETCH(v_intr),
	    (uintmax_t)VM_CNT_FETCH(v_swtch),
	    (long long)boottime.tv_sec);
	return (0);
}

static int
linprocfs_doswaps(PFS_FILL_ARGS)
{
	struct xswdev xsw;
	uintmax_t total, used;
	int n;
	char devname[SPECNAMELEN + 1];

	sbuf_printf(sb, "Filename\t\t\t\tType\t\tSize\tUsed\tPriority\n");
	for (n = 0; ; n++) {
		if (swap_dev_info(n, &xsw, devname, sizeof(devname)) != 0)
			break;
		total = (uintmax_t)xsw.xsw_nblks * PAGE_SIZE / 1024;
		used  = (uintmax_t)xsw.xsw_used * PAGE_SIZE / 1024;

		/*
		 * The space and not tab after the device name is on
		 * purpose.  Linux does so.
		 */
		sbuf_printf(sb, "/dev/%-34s unknown\t\t%jd\t%jd\t-1\n",
		    devname, total, used);
	}
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
	sbuf_printf(sb, "%lld.%02ld %ld.%02lu\n",
	    (long long)tv.tv_sec, tv.tv_usec / 10000,
	    T2S(cp_time[CP_IDLE] / mp_ncpus),
	    T2CS(cp_time[CP_IDLE] / mp_ncpus) % 100);
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

static int
linprocfs_get_tty_nr(struct proc *p)
{
	struct session *sp;
	const char *ttyname;
	int error, major, minor, nr;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	sx_assert(&proctree_lock, SX_LOCKED);

	if ((p->p_flag & P_CONTROLT) == 0)
		return (-1);

	sp = p->p_pgrp->pg_session;
	if (sp == NULL)
		return (-1);

	ttyname = devtoname(sp->s_ttyp->t_dev);
	error = linux_driver_get_major_minor(ttyname, &major, &minor);
	if (error != 0)
		return (-1);

	nr = makedev(major, minor);
	return (nr);
}

/*
 * Filler function for proc/pid/stat
 */
static int
linprocfs_doprocstat(PFS_FILL_ARGS)
{
	struct kinfo_proc kp;
	struct timeval boottime;
	char state;
	static int ratelimit = 0;
	int tty_nr;
	vm_offset_t startcode, startdata;

	getboottime(&boottime);
	sx_slock(&proctree_lock);
	PROC_LOCK(p);
	fill_kinfo_proc(p, &kp);
	tty_nr = linprocfs_get_tty_nr(p);
	sx_sunlock(&proctree_lock);
	if (p->p_vmspace) {
	   startcode = (vm_offset_t)p->p_vmspace->vm_taddr;
	   startdata = (vm_offset_t)p->p_vmspace->vm_daddr;
	} else {
	   startcode = 0;
	   startdata = 0;
	}
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
	PS_ADD("tty",		"%d",	tty_nr);
	PS_ADD("tpgid",		"%d",	kp.ki_tpgid);
	PS_ADD("flags",		"%u",	0); /* XXX */
	PS_ADD("minflt",	"%lu",	kp.ki_rusage.ru_minflt);
	PS_ADD("cminflt",	"%lu",	kp.ki_rusage_ch.ru_minflt);
	PS_ADD("majflt",	"%lu",	kp.ki_rusage.ru_majflt);
	PS_ADD("cmajflt",	"%lu",	kp.ki_rusage_ch.ru_majflt);
	PS_ADD("utime",		"%ld",	TV2J(&kp.ki_rusage.ru_utime));
	PS_ADD("stime",		"%ld",	TV2J(&kp.ki_rusage.ru_stime));
	PS_ADD("cutime",	"%ld",	TV2J(&kp.ki_rusage_ch.ru_utime));
	PS_ADD("cstime",	"%ld",	TV2J(&kp.ki_rusage_ch.ru_stime));
	PS_ADD("priority",	"%d",	kp.ki_pri.pri_user);
	PS_ADD("nice",		"%d",	kp.ki_nice); /* 19 (nicest) to -19 */
	PS_ADD("0",		"%d",	0); /* removed field */
	PS_ADD("itrealvalue",	"%d",	0); /* XXX */
	PS_ADD("starttime",	"%lu",	TV2J(&kp.ki_start) - TV2J(&boottime));
	PS_ADD("vsize",		"%ju",	(uintmax_t)kp.ki_size);
	PS_ADD("rss",		"%ju",	(uintmax_t)kp.ki_rssize);
	PS_ADD("rlim",		"%lu",	kp.ki_rusage.ru_maxrss);
	PS_ADD("startcode",	"%ju",	(uintmax_t)startcode);
	PS_ADD("endcode",	"%ju",	(uintmax_t)startdata);
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

	sx_slock(&proctree_lock);
	PROC_LOCK(p);
	fill_kinfo_proc(p, &kp);
	PROC_UNLOCK(p);
	sx_sunlock(&proctree_lock);

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
	l_sigset_t siglist, sigignore, sigcatch;
	int i;

	sx_slock(&proctree_lock);
	PROC_LOCK(p);
	td2 = FIRST_THREAD_IN_PROC(p);

	if (P_SHOULDSTOP(p)) {
		state = "T (stopped)";
	} else {
		switch(p->p_state) {
		case PRS_NEW:
			state = "I (idle)";
			break;
		case PRS_NORMAL:
			if (p->p_flag & P_WEXIT) {
				state = "X (exiting)";
				break;
			}
			switch(TD_GET_STATE(td2)) {
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
	}

	fill_kinfo_proc(p, &kp);
	sx_sunlock(&proctree_lock);

	sbuf_printf(sb, "Name:\t%s\n",		p->p_comm); /* XXX escape */
	sbuf_printf(sb, "State:\t%s\n",		state);

	/*
	 * Credentials
	 */
	sbuf_printf(sb, "Tgid:\t%d\n",		p->p_pid);
	sbuf_printf(sb, "Pid:\t%d\n",		p->p_pid);
	sbuf_printf(sb, "PPid:\t%d\n",		kp.ki_ppid );
	sbuf_printf(sb, "TracerPid:\t%d\n",	kp.ki_tracer );
	sbuf_printf(sb, "Uid:\t%d\t%d\t%d\t%d\n", p->p_ucred->cr_ruid,
						p->p_ucred->cr_uid,
						p->p_ucred->cr_svuid,
						/* FreeBSD doesn't have fsuid */
						p->p_ucred->cr_uid);
	sbuf_printf(sb, "Gid:\t%d\t%d\t%d\t%d\n", p->p_ucred->cr_rgid,
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
	sbuf_printf(sb, "VmRSS:\t%8ju kB\n",	P2K((uintmax_t)kp.ki_rssize));
	sbuf_printf(sb, "VmData:\t%8ju kB\n",	P2K((uintmax_t)kp.ki_dsize));
	sbuf_printf(sb, "VmStk:\t%8ju kB\n",	P2K((uintmax_t)kp.ki_ssize));
	sbuf_printf(sb, "VmExe:\t%8ju kB\n",	P2K((uintmax_t)kp.ki_tsize));
	lsize = B2P(kp.ki_size) - kp.ki_dsize -
	    kp.ki_ssize - kp.ki_tsize - 1;
	sbuf_printf(sb, "VmLib:\t%8ju kB\n",	P2K((uintmax_t)lsize));

	/*
	 * Signal masks
	 */
	PROC_LOCK(p);
	bsd_to_linux_sigset(&p->p_siglist, &siglist);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	bsd_to_linux_sigset(&ps->ps_sigignore, &sigignore);
	bsd_to_linux_sigset(&ps->ps_sigcatch, &sigcatch);
	mtx_unlock(&ps->ps_mtx);
	PROC_UNLOCK(p);

	sbuf_printf(sb, "SigPnd:\t%016jx\n",	siglist.__mask);
	/*
	 * XXX. SigBlk - target thread's signal mask, td_sigmask.
	 * To implement SigBlk pseudofs should support proc/tid dir entries.
	 */
	sbuf_printf(sb, "SigBlk:\t%016x\n",	0);
	sbuf_printf(sb, "SigIgn:\t%016jx\n",	sigignore.__mask);
	sbuf_printf(sb, "SigCgt:\t%016jx\n",	sigcatch.__mask);

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
	struct pwd *pwd;
	char *fullpath = "unknown";
	char *freepath = NULL;

	pwd = pwd_hold_proc(p);
	vn_fullpath(pwd->pwd_cdir, &fullpath, &freepath);
	sbuf_printf(sb, "%s", fullpath);
	if (freepath)
		free(freepath, M_TEMP);
	pwd_drop(pwd);
	return (0);
}

/*
 * Filler function for proc/pid/root
 */
static int
linprocfs_doprocroot(PFS_FILL_ARGS)
{
	struct pwd *pwd;
	struct vnode *vp;
	char *fullpath = "unknown";
	char *freepath = NULL;

	pwd = pwd_hold_proc(p);
	vp = jailed(p->p_ucred) ? pwd->pwd_jdir : pwd->pwd_rdir;
	vn_fullpath(vp, &fullpath, &freepath);
	sbuf_printf(sb, "%s", fullpath);
	if (freepath)
		free(freepath, M_TEMP);
	pwd_drop(pwd);
	return (0);
}

/*
 * Filler function for proc/pid/cmdline
 */
static int
linprocfs_doproccmdline(PFS_FILL_ARGS)
{
	int ret;

	PROC_LOCK(p);
	if ((ret = p_cansee(td, p)) != 0) {
		PROC_UNLOCK(p);
		return (ret);
	}

	/*
	 * Mimic linux behavior and pass only processes with usermode
	 * address space as valid.  Return zero silently otherwize.
	 */
	if (p->p_vmspace == &vmspace0) {
		PROC_UNLOCK(p);
		return (0);
	}
	if (p->p_args != NULL) {
		sbuf_bcpy(sb, p->p_args->ar_args, p->p_args->ar_length);
		PROC_UNLOCK(p);
		return (0);
	}

	if ((p->p_flag & P_SYSTEM) != 0) {
		PROC_UNLOCK(p);
		return (0);
	}

	PROC_UNLOCK(p);

	ret = proc_getargv(td, p, sb);
	return (ret);
}

/*
 * Filler function for proc/pid/environ
 */
static int
linprocfs_doprocenviron(PFS_FILL_ARGS)
{

	/*
	 * Mimic linux behavior and pass only processes with usermode
	 * address space as valid.  Return zero silently otherwize.
	 */
	if (p->p_vmspace == &vmspace0)
		return (0);

	return (proc_getenvv(td, p, sb));
}

static char l32_map_str[] = "%08lx-%08lx %s%s%s%s %08lx %02x:%02x %lu%s%s\n";
static char l64_map_str[] = "%016lx-%016lx %s%s%s%s %08lx %02x:%02x %lu%s%s\n";
static char vdso_str[] = "      [vdso]";
static char stack_str[] = "      [stack]";

/*
 * Filler function for proc/pid/maps
 */
static int
linprocfs_doprocmaps(PFS_FILL_ARGS)
{
	struct vmspace *vm;
	vm_map_t map;
	vm_map_entry_t entry, tmp_entry;
	vm_object_t obj, tobj, lobj;
	vm_offset_t e_start, e_end;
	vm_ooffset_t off;
	vm_prot_t e_prot;
	unsigned int last_timestamp;
	char *name = "", *freename = NULL;
	const char *l_map_str;
	ino_t ino;
	int error;
	struct vnode *vp;
	struct vattr vat;
	bool private;

	PROC_LOCK(p);
	error = p_candebug(td, p);
	PROC_UNLOCK(p);
	if (error)
		return (error);

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	error = 0;
	vm = vmspace_acquire_ref(p);
	if (vm == NULL)
		return (ESRCH);

	if (SV_CURPROC_FLAG(SV_LP64))
		l_map_str = l64_map_str;
	else
		l_map_str = l32_map_str;
	map = &vm->vm_map;
	vm_map_lock_read(map);
	VM_MAP_ENTRY_FOREACH(entry, map) {
		name = "";
		freename = NULL;
		/*
		 * Skip printing of the guard page of the stack region, as
		 * it confuses glibc pthread_getattr_np() method, where both
		 * the base address and size of the stack of the initial thread
		 * are calculated.
		 */
		if ((entry->eflags & (MAP_ENTRY_IS_SUB_MAP | MAP_ENTRY_GUARD)) != 0)
			continue;
		e_prot = entry->protection;
		e_start = entry->start;
		e_end = entry->end;
		obj = entry->object.vm_object;
		off = entry->offset;
		for (lobj = tobj = obj; tobj != NULL;
		    lobj = tobj, tobj = tobj->backing_object) {
			VM_OBJECT_RLOCK(tobj);
			off += lobj->backing_object_offset;
			if (lobj != obj)
				VM_OBJECT_RUNLOCK(lobj);
		}
		private = (entry->eflags & MAP_ENTRY_COW) != 0 || obj == NULL ||
		    (obj->flags & OBJ_ANON) != 0;
		last_timestamp = map->timestamp;
		vm_map_unlock_read(map);
		ino = 0;
		if (lobj) {
			vp = vm_object_vnode(lobj);
			if (vp != NULL)
				vref(vp);
			if (lobj != obj)
				VM_OBJECT_RUNLOCK(lobj);
			VM_OBJECT_RUNLOCK(obj);
			if (vp != NULL) {
				vn_fullpath(vp, &name, &freename);
				vn_lock(vp, LK_SHARED | LK_RETRY);
				VOP_GETATTR(vp, &vat, td->td_ucred);
				ino = vat.va_fileid;
				vput(vp);
			} else if (SV_PROC_ABI(p) == SV_ABI_LINUX) {
				/*
				 * sv_shared_page_base pointed out to the
				 * FreeBSD sharedpage, PAGE_SIZE is a size
				 * of it. The vDSO page is above.
				 */
				if (e_start == p->p_sysent->sv_shared_page_base +
				    PAGE_SIZE)
					name = vdso_str;
				if (e_end == p->p_sysent->sv_usrstack)
					name = stack_str;
			}
		}

		/*
		 * format:
		 *  start, end, access, offset, major, minor, inode, name.
		 */
		error = sbuf_printf(sb, l_map_str,
		    (u_long)e_start, (u_long)e_end,
		    (e_prot & VM_PROT_READ)?"r":"-",
		    (e_prot & VM_PROT_WRITE)?"w":"-",
		    (e_prot & VM_PROT_EXECUTE)?"x":"-",
		    private ? "p" : "s",
		    (u_long)off,
		    0,
		    0,
		    (u_long)ino,
		    *name ? "     " : " ",
		    name
		    );
		if (freename)
			free(freename, M_TEMP);
		vm_map_lock_read(map);
		if (error == -1) {
			error = 0;
			break;
		}
		if (last_timestamp != map->timestamp) {
			/*
			 * Look again for the entry because the map was
			 * modified while it was unlocked.  Specifically,
			 * the entry may have been clipped, merged, or deleted.
			 */
			vm_map_lookup_entry(map, e_end - 1, &tmp_entry);
			entry = tmp_entry;
		}
	}
	vm_map_unlock_read(map);
	vmspace_free(vm);

	return (error);
}

/*
 * Filler function for proc/pid/mem
 */
static int
linprocfs_doprocmem(PFS_FILL_ARGS)
{
	ssize_t resid;
	int error;

	resid = uio->uio_resid;
	error = procfs_doprocmem(PFS_FILL_ARGNAMES);

	if (uio->uio_rw == UIO_READ && resid != uio->uio_resid)
		return (0);

	if (error == EFAULT)
		error = EIO;

	return (error);
}

/*
 * Filler function for proc/net/dev
 */
static int
linprocfs_donetdev_cb(if_t ifp, void *arg)
{
	char ifname[LINUX_IFNAMSIZ];
	struct sbuf *sb = arg;

	if (ifname_bsd_to_linux_ifp(ifp, ifname, sizeof(ifname)) <= 0)
		return (ENODEV);

	sbuf_printf(sb, "%6.6s: ", ifname);
	sbuf_printf(sb, "%7ju %7ju %4ju %4ju %4lu %5lu %10lu %9ju ",
	    (uintmax_t)if_getcounter(ifp, IFCOUNTER_IBYTES),
	    (uintmax_t)if_getcounter(ifp, IFCOUNTER_IPACKETS),
	    (uintmax_t)if_getcounter(ifp, IFCOUNTER_IERRORS),
	    (uintmax_t)if_getcounter(ifp, IFCOUNTER_IQDROPS),
						/* rx_missed_errors */
	    0UL,				/* rx_fifo_errors */
	    0UL,				/* rx_length_errors +
						 * rx_over_errors +
						 * rx_crc_errors +
						 * rx_frame_errors */
	    0UL,				/* rx_compressed */
	    (uintmax_t)if_getcounter(ifp, IFCOUNTER_IMCASTS));
						/* XXX-BZ rx only? */
	sbuf_printf(sb, "%8ju %7ju %4ju %4ju %4lu %5ju %7lu %10lu\n",
	    (uintmax_t)if_getcounter(ifp, IFCOUNTER_OBYTES),
	    (uintmax_t)if_getcounter(ifp, IFCOUNTER_OPACKETS),
	    (uintmax_t)if_getcounter(ifp, IFCOUNTER_OERRORS),
	    (uintmax_t)if_getcounter(ifp, IFCOUNTER_OQDROPS),
	    0UL,				/* tx_fifo_errors */
	    (uintmax_t)if_getcounter(ifp, IFCOUNTER_COLLISIONS),
	    0UL,				/* tx_carrier_errors +
						 * tx_aborted_errors +
						 * tx_window_errors +
						 * tx_heartbeat_errors*/
	    0UL);				/* tx_compressed */
	return (0);
}

static int
linprocfs_donetdev(PFS_FILL_ARGS)
{
	struct epoch_tracker et;

	sbuf_printf(sb, "%6s|%58s|%s\n"
	    "%6s|%58s|%58s\n",
	    "Inter-", "   Receive", "  Transmit",
	    " face",
	    "bytes    packets errs drop fifo frame compressed multicast",
	    "bytes    packets errs drop fifo colls carrier compressed");

	CURVNET_SET(TD_TO_VNET(curthread));
	NET_EPOCH_ENTER(et);
	if_foreach(linprocfs_donetdev_cb, sb);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();

	return (0);
}

struct walkarg {
	struct sbuf *sb;
};

static int
linux_route_print(struct rtentry *rt, void *vw)
{
#ifdef INET
	struct walkarg *w = vw;
	struct route_nhop_data rnd;
	struct in_addr dst, mask;
	struct nhop_object *nh;
	char ifname[16];
	uint32_t scopeid = 0;
	uint32_t gw = 0;
	uint32_t linux_flags = 0;

	rt_get_inet_prefix_pmask(rt, &dst, &mask, &scopeid);

	rt_get_rnd(rt, &rnd);

	/* select only first route in case of multipath */
	nh = nhop_select_func(rnd.rnd_nhop, 0);

	if (ifname_bsd_to_linux_ifp(nh->nh_ifp, ifname, sizeof(ifname)) <= 0)
		return (ENODEV);

	gw = (nh->nh_flags & NHF_GATEWAY)
		? nh->gw4_sa.sin_addr.s_addr : 0;

	linux_flags = RTF_UP |
		(nhop_get_rtflags(nh) & (RTF_GATEWAY | RTF_HOST));

	sbuf_printf(w->sb,
		"%s\t"
		"%08X\t%08X\t%04X\t"
		"%d\t%u\t%d\t"
		"%08X\t%d\t%u\t%u",
		ifname,
		dst.s_addr, gw, linux_flags,
		0, 0, rnd.rnd_weight,
		mask.s_addr, nh->nh_mtu, 0, 0);

	sbuf_printf(w->sb, "\n\n");
#endif
	return (0);
}

/*
 * Filler function for proc/net/route
 */
static int
linprocfs_donetroute(PFS_FILL_ARGS)
{
	struct epoch_tracker et;
	struct walkarg w = {
		.sb = sb
	};
	uint32_t fibnum = curthread->td_proc->p_fibnum;

	sbuf_printf(w.sb, "%-127s\n", "Iface\tDestination\tGateway "
               "\tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU"
               "\tWindow\tIRTT");

	CURVNET_SET(TD_TO_VNET(curthread));
	NET_EPOCH_ENTER(et);
	rib_walk(fibnum, AF_INET, false, linux_route_print, &w);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();

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
 * Filler function for proc/sys/kernel/msgmax
 */
static int
linprocfs_domsgmax(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%d\n", msginfo.msgmax);
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
 * Filler function for proc/sys/kernel/msgmnb
 */
static int
linprocfs_domsgmnb(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%d\n", msginfo.msgmnb);
	return (0);
}

/*
 * Filler function for proc/sys/kernel/ngroups_max
 *
 * Note that in Linux it defaults to 65536, not 1023.
 */
static int
linprocfs_dongroups_max(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%d\n", ngroups_max);
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
 * Filler function for proc/sys/kernel/shmall
 */
static int
linprocfs_doshmall(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%lu\n", shminfo.shmall);
	return (0);
}

/*
 * Filler function for proc/sys/kernel/shmmax
 */
static int
linprocfs_doshmmax(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%lu\n", shminfo.shmmax);
	return (0);
}

/*
 * Filler function for proc/sys/kernel/shmmni
 */
static int
linprocfs_doshmmni(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%lu\n", shminfo.shmmni);
	return (0);
}

/*
 * Filler function for proc/sys/kernel/tainted
 */
static int
linprocfs_dotainted(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "0\n");
	return (0);
}

/*
 * Filler function for proc/sys/vm/min_free_kbytes
 *
 * This mirrors the approach in illumos to return zero for reads. Effectively,
 * it says, no memory is kept in reserve for "atomic allocations". This class
 * of allocation can be used at times when a thread cannot be suspended.
 */
static int
linprocfs_dominfree(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%d\n", 0);
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

/*
 * Filler function for proc/filesystems
 */
static int
linprocfs_dofilesystems(PFS_FILL_ARGS)
{
	struct vfsconf *vfsp;

	vfsconf_slock();
	TAILQ_FOREACH(vfsp, &vfsconf, vfc_list) {
		if (vfsp->vfc_flags & VFCF_SYNTHETIC)
			sbuf_printf(sb, "nodev");
		sbuf_printf(sb, "\t%s\n", vfsp->vfc_name);
	}
	vfsconf_sunlock();
	return(0);
}

/*
 * Filler function for proc/modules
 */
static int
linprocfs_domodules(PFS_FILL_ARGS)
{
#if 0
	struct linker_file *lf;

	TAILQ_FOREACH(lf, &linker_files, link) {
		sbuf_printf(sb, "%-20s%8lu%4d\n", lf->filename,
		    (unsigned long)lf->size, lf->refs);
	}
#endif
	return (0);
}

/*
 * Filler function for proc/pid/fd
 */
static int
linprocfs_dofdescfs(PFS_FILL_ARGS)
{

	if (p == curproc)
		sbuf_printf(sb, "/dev/fd");
	else
		sbuf_printf(sb, "unknown");
	return (0);
}

/*
 * Filler function for proc/pid/limits
 */
static const struct linux_rlimit_ident {
	const char	*desc;
	const char	*unit;
	unsigned int	rlim_id;
} linux_rlimits_ident[] = {
	{ "Max cpu time",	"seconds",	RLIMIT_CPU },
	{ "Max file size", 	"bytes",	RLIMIT_FSIZE },
	{ "Max data size",	"bytes", 	RLIMIT_DATA },
	{ "Max stack size",	"bytes", 	RLIMIT_STACK },
	{ "Max core file size",  "bytes",	RLIMIT_CORE },
	{ "Max resident set",	"bytes",	RLIMIT_RSS },
	{ "Max processes",	"processes",	RLIMIT_NPROC },
	{ "Max open files",	"files",	RLIMIT_NOFILE },
	{ "Max locked memory",	"bytes",	RLIMIT_MEMLOCK },
	{ "Max address space",	"bytes",	RLIMIT_AS },
	{ "Max file locks",	"locks",	LINUX_RLIMIT_LOCKS },
	{ "Max pending signals", "signals",	LINUX_RLIMIT_SIGPENDING },
	{ "Max msgqueue size",	"bytes",	LINUX_RLIMIT_MSGQUEUE },
	{ "Max nice priority", 		"",	LINUX_RLIMIT_NICE },
	{ "Max realtime priority",	"",	LINUX_RLIMIT_RTPRIO },
	{ "Max realtime timeout",	"us",	LINUX_RLIMIT_RTTIME },
	{ 0, 0, 0 }
};

static int
linprocfs_doproclimits(PFS_FILL_ARGS)
{
	const struct linux_rlimit_ident *li;
	struct plimit *limp;
	struct rlimit rl;
	ssize_t size;
	int res, error;

	error = 0;

	PROC_LOCK(p);
	limp = lim_hold(p->p_limit);
	PROC_UNLOCK(p);
	size = sizeof(res);
	sbuf_printf(sb, "%-26s%-21s%-21s%-21s\n", "Limit", "Soft Limit",
			"Hard Limit", "Units");
	for (li = linux_rlimits_ident; li->desc != NULL; ++li) {
		switch (li->rlim_id)
		{
		case LINUX_RLIMIT_LOCKS:
			/* FALLTHROUGH */
		case LINUX_RLIMIT_RTTIME:
			rl.rlim_cur = RLIM_INFINITY;
			break;
		case LINUX_RLIMIT_SIGPENDING:
			error = kernel_sysctlbyname(td,
			    "kern.sigqueue.max_pending_per_proc",
			    &res, &size, 0, 0, 0, 0);
			if (error != 0)
				goto out;
			rl.rlim_cur = res;
			rl.rlim_max = res;
			break;
		case LINUX_RLIMIT_MSGQUEUE:
			error = kernel_sysctlbyname(td,
			    "kern.ipc.msgmnb", &res, &size, 0, 0, 0, 0);
			if (error != 0)
				goto out;
			rl.rlim_cur = res;
			rl.rlim_max = res;
			break;
		case LINUX_RLIMIT_NICE:
			/* FALLTHROUGH */
		case LINUX_RLIMIT_RTPRIO:
			rl.rlim_cur = 0;
			rl.rlim_max = 0;
			break;
		default:
			rl = limp->pl_rlimit[li->rlim_id];
			break;
		}
		if (rl.rlim_cur == RLIM_INFINITY)
			sbuf_printf(sb, "%-26s%-21s%-21s%-10s\n",
			    li->desc, "unlimited", "unlimited", li->unit);
		else
			sbuf_printf(sb, "%-26s%-21llu%-21llu%-10s\n",
			    li->desc, (unsigned long long)rl.rlim_cur,
			    (unsigned long long)rl.rlim_max, li->unit);
	}
out:
	lim_free(limp);
	return (error);
}

/*
 * The point of the following two functions is to work around
 * an assertion in Chromium; see kern/240991 for details.
 */
static int
linprocfs_dotaskattr(PFS_ATTR_ARGS)
{

	vap->va_nlink = 3;
	return (0);
}

/*
 * Filler function for proc/<pid>/task/.dummy
 */
static int
linprocfs_dotaskdummy(PFS_FILL_ARGS)
{

	return (0);
}

/*
 * Filler function for proc/sys/kernel/random/uuid
 */
static int
linprocfs_douuid(PFS_FILL_ARGS)
{
	struct uuid uuid;

	kern_uuidgen(&uuid, 1);
	sbuf_printf_uuid(sb, &uuid);
	sbuf_printf(sb, "\n");
	return(0);
}

/*
 * Filler function for proc/sys/kernel/random/boot_id
 */
static int
linprocfs_doboot_id(PFS_FILL_ARGS)
{
       static bool firstboot = 1;
       static struct uuid uuid;

       if (firstboot) {
               kern_uuidgen(&uuid, 1);
               firstboot = 0;
       }
       sbuf_printf_uuid(sb, &uuid);
       sbuf_printf(sb, "\n");
       return(0);
}

/*
 * Filler function for proc/pid/auxv
 */
static int
linprocfs_doauxv(PFS_FILL_ARGS)
{
	struct sbuf *asb;
	off_t buflen, resid;
	int error;

	/*
	 * Mimic linux behavior and pass only processes with usermode
	 * address space as valid. Return zero silently otherwise.
	 */
	if (p->p_vmspace == &vmspace0)
		return (0);

	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0 || uio->uio_resid < 0)
		return (EINVAL);

	asb = sbuf_new_auto();
	if (asb == NULL)
		return (ENOMEM);
	error = proc_getauxv(td, p, asb);
	if (error == 0)
		error = sbuf_finish(asb);

	resid = sbuf_len(asb) - uio->uio_offset;
	if (resid > uio->uio_resid)
		buflen = uio->uio_resid;
	else
		buflen = resid;
	if (buflen > IOSIZE_MAX)
		return (EINVAL);
	if (buflen > maxphys)
		buflen = maxphys;
	if (resid <= 0)
		return (0);

	if (error == 0)
		error = uiomove(sbuf_data(asb) + uio->uio_offset, buflen, uio);
	sbuf_delete(asb);
	return (error);
}

/*
 * Filler function for proc/self/oom_score_adj
 */
static int
linprocfs_do_oom_score_adj(PFS_FILL_ARGS)
{
	struct linux_pemuldata *pem;
	long oom;

	pem = pem_find(p);
	if (pem == NULL || uio == NULL)
		return (EOPNOTSUPP);
	if (uio->uio_rw == UIO_READ) {
		sbuf_printf(sb, "%d\n", pem->oom_score_adj);
	} else {
		sbuf_trim(sb);
		sbuf_finish(sb);
		oom = strtol(sbuf_data(sb), NULL, 10);
		if (oom < LINUX_OOM_SCORE_ADJ_MIN ||
		    oom > LINUX_OOM_SCORE_ADJ_MAX)
			return (EINVAL);
		pem->oom_score_adj = oom;
	}
	return (0);
}

/*
 * Filler function for proc/sys/vm/max_map_count
 *
 * Maximum number of active map areas, on Linux this limits the number
 * of vmaps per mm struct. We don't limit mappings, return a suitable
 * large value.
 */
static int
linprocfs_domax_map_cnt(PFS_FILL_ARGS)
{

	sbuf_printf(sb, "%d\n", INT32_MAX);
	return (0);
}

/*
 * Constructor
 */
static int
linprocfs_init(PFS_INIT_ARGS)
{
	struct pfs_node *root;
	struct pfs_node *dir;
	struct pfs_node *sys;

	root = pi->pi_root;

	/* /proc/... */
	pfs_create_file(root, "cmdline", &linprocfs_docmdline,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "cpuinfo", &linprocfs_docpuinfo,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "devices", &linprocfs_dodevices,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "filesystems", &linprocfs_dofilesystems,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "loadavg", &linprocfs_doloadavg,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "meminfo", &linprocfs_domeminfo,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "modules", &linprocfs_domodules,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "mounts", &linprocfs_domtab,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "mtab", &linprocfs_domtab,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "partitions", &linprocfs_dopartitions,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_link(root, "self", &procfs_docurproc,
	    NULL, NULL, NULL, 0);
	pfs_create_file(root, "stat", &linprocfs_dostat,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "swaps", &linprocfs_doswaps,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "uptime", &linprocfs_douptime,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(root, "version", &linprocfs_doversion,
	    NULL, NULL, NULL, PFS_RD);

	/* /proc/bus/... */
	dir = pfs_create_dir(root, "bus", NULL, NULL, NULL, 0);
	dir = pfs_create_dir(dir, "pci", NULL, NULL, NULL, 0);
	dir = pfs_create_dir(dir, "devices", NULL, NULL, NULL, 0);

	/* /proc/net/... */
	dir = pfs_create_dir(root, "net", NULL, NULL, NULL, 0);
	pfs_create_file(dir, "dev", &linprocfs_donetdev,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "route", &linprocfs_donetroute,
	    NULL, NULL, NULL, PFS_RD);

	/* /proc/<pid>/... */
	dir = pfs_create_dir(root, "pid", NULL, NULL, NULL, PFS_PROCDEP);
	pfs_create_file(dir, "cmdline", &linprocfs_doproccmdline,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_link(dir, "cwd", &linprocfs_doproccwd,
	    NULL, NULL, NULL, 0);
	pfs_create_file(dir, "environ", &linprocfs_doprocenviron,
	    NULL, &procfs_candebug, NULL, PFS_RD);
	pfs_create_link(dir, "exe", &procfs_doprocfile,
	    NULL, &procfs_notsystem, NULL, 0);
	pfs_create_file(dir, "maps", &linprocfs_doprocmaps,
	    NULL, NULL, NULL, PFS_RD | PFS_AUTODRAIN);
	pfs_create_file(dir, "mem", &linprocfs_doprocmem,
	    procfs_attr_rw, &procfs_candebug, NULL, PFS_RDWR | PFS_RAW);
	pfs_create_file(dir, "mountinfo", &linprocfs_doprocmountinfo,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "mounts", &linprocfs_domtab,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_link(dir, "root", &linprocfs_doprocroot,
	    NULL, NULL, NULL, 0);
	pfs_create_file(dir, "stat", &linprocfs_doprocstat,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "statm", &linprocfs_doprocstatm,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "status", &linprocfs_doprocstatus,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_link(dir, "fd", &linprocfs_dofdescfs,
	    NULL, NULL, NULL, 0);
	pfs_create_file(dir, "auxv", &linprocfs_doauxv,
	    NULL, &procfs_candebug, NULL, PFS_RD|PFS_RAWRD);
	pfs_create_file(dir, "limits", &linprocfs_doproclimits,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "oom_score_adj", &linprocfs_do_oom_score_adj,
	    procfs_attr_rw, &procfs_candebug, NULL, PFS_RDWR);

	/* /proc/<pid>/task/... */
	dir = pfs_create_dir(dir, "task", linprocfs_dotaskattr, NULL, NULL, 0);
	pfs_create_file(dir, ".dummy", &linprocfs_dotaskdummy,
	    NULL, NULL, NULL, PFS_RD);

	/* /proc/scsi/... */
	dir = pfs_create_dir(root, "scsi", NULL, NULL, NULL, 0);
	pfs_create_file(dir, "device_info", &linprocfs_doscsidevinfo,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "scsi", &linprocfs_doscsiscsi,
	    NULL, NULL, NULL, PFS_RD);

	/* /proc/sys/... */
	sys = pfs_create_dir(root, "sys", NULL, NULL, NULL, 0);

	/* /proc/sys/kernel/... */
	dir = pfs_create_dir(sys, "kernel", NULL, NULL, NULL, 0);
	pfs_create_file(dir, "osrelease", &linprocfs_doosrelease,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "ostype", &linprocfs_doostype,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "version", &linprocfs_doosbuild,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "msgmax", &linprocfs_domsgmax,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "msgmni", &linprocfs_domsgmni,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "msgmnb", &linprocfs_domsgmnb,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "ngroups_max", &linprocfs_dongroups_max,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "pid_max", &linprocfs_dopid_max,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "sem", &linprocfs_dosem,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "shmall", &linprocfs_doshmall,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "shmmax", &linprocfs_doshmmax,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "shmmni", &linprocfs_doshmmni,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "tainted", &linprocfs_dotainted,
	    NULL, NULL, NULL, PFS_RD);

	/* /proc/sys/kernel/random/... */
	dir = pfs_create_dir(dir, "random", NULL, NULL, NULL, 0);
	pfs_create_file(dir, "uuid", &linprocfs_douuid,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "boot_id", &linprocfs_doboot_id,
	    NULL, NULL, NULL, PFS_RD);

	/* /proc/sys/vm/.... */
	dir = pfs_create_dir(sys, "vm", NULL, NULL, NULL, 0);
	pfs_create_file(dir, "min_free_kbytes", &linprocfs_dominfree,
	    NULL, NULL, NULL, PFS_RD);
	pfs_create_file(dir, "max_map_count", &linprocfs_domax_map_cnt,
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

PSEUDOFS(linprocfs, 1, VFCF_JAIL);
#if defined(__aarch64__) || defined(__amd64__)
MODULE_DEPEND(linprocfs, linux_common, 1, 1, 1);
#else
MODULE_DEPEND(linprocfs, linux, 1, 1, 1);
#endif
MODULE_DEPEND(linprocfs, procfs, 1, 1, 1);
MODULE_DEPEND(linprocfs, sysvmsg, 1, 1, 1);
MODULE_DEPEND(linprocfs, sysvsem, 1, 1, 1);
MODULE_DEPEND(linprocfs, sysvshm, 1, 1, 1);
