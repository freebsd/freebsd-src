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
#include <sys/blist.h>
#include <sys/dkstat.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/vnode.h>
#include <sys/lock.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/swap_pager.h>
#include <sys/vmmeter.h>
#include <sys/exec.h>

#include <machine/clock.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>

#include <i386/linux/linprocfs/linprocfs.h>

/*
 * Various conversion macros
 */
#define T2J(x) (((x) * 100UL) / (stathz ? stathz : hz))	/* ticks to jiffies */
#define T2S(x) ((x) / (stathz ? stathz : hz))		/* ticks to seconds */
#define B2K(x) ((x) >> 10)				/* bytes to kbytes */
#define P2B(x) ((x) << PAGE_SHIFT)			/* pages to bytes */
#define P2K(x) ((x) << (PAGE_SHIFT - 10))		/* pages to kbytes */

int
linprocfs_domeminfo(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	char *ps;
	int xlen;
	char psbuf[512];		/* XXX - conservative */
	unsigned long memtotal;		/* total memory in bytes */
	unsigned long memused;		/* used memory in bytes */
	unsigned long memfree;		/* free memory in bytes */
	unsigned long memshared;	/* shared memory ??? */
	unsigned long buffers, cached;	/* buffer / cache memory ??? */
	unsigned long long swaptotal;	/* total swap space in bytes */
	unsigned long long swapused;		/* used swap space in bytes */
	unsigned long long swapfree;		/* free swap space in bytes */
	vm_object_t object;

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

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
		swaptotal = (unsigned long long) swapblist->bl_blocks * 1024ULL; /* XXX why 1024? */
		swapfree = (unsigned long long) swapblist->bl_root->u.bmu_avail * PAGE_SIZE;
	}
	swapused = swaptotal - swapfree;
	memshared = 0;
	for (object = TAILQ_FIRST(&vm_object_list); object != NULL;
	    object = TAILQ_NEXT(object, object_list))
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

	ps = psbuf;
	ps += sprintf(ps,
		"        total:    used:    free:  shared: buffers:  cached:\n"
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

	xlen = ps - psbuf;
	xlen -= uio->uio_offset;
	ps = psbuf + uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	return (xlen <= 0 ? 0 : uiomove(ps, xlen, uio));
}

int
linprocfs_docpuinfo(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	char *ps;
	int xlen;
	char psbuf[512];		/* XXX - conservative */
	int class;
        int i;
#if 0
	extern char *cpu_model;		/* Yuck */
#endif
        /* We default the flags to include all non-conflicting flags,
           and the Intel versions of conflicting flags.  Note the space
           before each name; that is significant, and should be 
           preserved. */

        static char *flags[] = {
		"fpu",      "vme",     "de",       "pse",      "tsc",
		"msr",      "pae",     "mce",      "cx8",      "apic",
		"sep",      "sep",     "mtrr",     "pge",      "mca",
		"cmov",     "pat",     "pse36",    "pn",       "b19",
		"b20",      "b21",     "mmxext",   "mmx",      "fxsr",
		"xmm",      "b26",     "b27",      "b28",      "b29",
		"3dnowext", "3dnow"
	};

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

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

	ps = psbuf;
	ps += sprintf(ps,
			"processor\t: %d\n"
			"vendor_id\t: %.20s\n"
			"cpu family\t: %d\n"
			"model\t\t: %d\n"
			"stepping\t: %d\n",
			0, cpu_vendor, class, cpu, cpu_id & 0xf);

        ps += sprintf(ps,
                        "flags\t\t:");

        if (!strcmp(cpu_vendor, "AuthenticAMD") && (class < 6)) {
		flags[16] = "fcmov";
        } else if (!strcmp(cpu_vendor, "CyrixInstead")) {
		flags[24] = "cxmmx";
        }
        
        for (i = 0; i < 32; i++)
		if (cpu_feature & (1 << i))
			ps += sprintf(ps, " %s", flags[i]);
	ps += sprintf(ps, "\n");
        if (class >= 5) {
		ps += sprintf(ps,
			"cpu MHz\t\t: %d.%02d\n"
			"bogomips\t: %d.%02d\n",
                        (tsc_freq + 4999) / 1000000,
                        ((tsc_freq + 4999) / 10000) % 100,
                        (tsc_freq + 4999) / 1000000,
                        ((tsc_freq + 4999) / 10000) % 100);
        }
        
	xlen = ps - psbuf;
	xlen -= uio->uio_offset;
	ps = psbuf + uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	return (xlen <= 0 ? 0 : uiomove(ps, xlen, uio));
}

int
linprocfs_dostat(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
        char *ps;
	char psbuf[512];
	int xlen;

	ps = psbuf;
	ps += sprintf(ps,
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
	xlen = ps - psbuf;
	xlen -= uio->uio_offset;
	ps = psbuf + uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	return (xlen <= 0 ? 0 : uiomove(ps, xlen, uio));
}

int
linprocfs_douptime(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	char *ps;
	int xlen;
	char psbuf[64];
	struct timeval tv;

	getmicrouptime(&tv);
	ps = psbuf;
	ps += sprintf(ps, "%ld.%02ld %ld.%02ld\n",
		      tv.tv_sec, tv.tv_usec / 10000,
		      T2S(cp_time[CP_IDLE]), T2J(cp_time[CP_IDLE]) % 100);
	xlen = ps - psbuf;
	xlen -= uio->uio_offset;
	ps = psbuf + uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	return (xlen <= 0 ? 0 : uiomove(ps, xlen, uio));
}

int
linprocfs_doversion(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
        char *ps;
	int xlen;

	ps = version; /* XXX not entirely correct */
	for (xlen = 0; ps[xlen] != '\n'; ++xlen)
		/* nothing */ ;
	++xlen;
	xlen -= uio->uio_offset;
	ps += uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	return (xlen <= 0 ? 0 : uiomove(ps, xlen, uio));
}

int
linprocfs_doprocstat(curp, p, pfs, uio)
    	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	char *ps, psbuf[1024];
	int xlen;

	ps = psbuf;
	ps += sprintf(ps, "%d", p->p_pid);
#define PS_ADD(name, fmt, arg) ps += sprintf(ps, " " fmt, arg)
	PS_ADD("comm",		"(%s)",	p->p_comm);
	PS_ADD("statr",		"%c",	'0'); /* XXX */
	PS_ADD("ppid",		"%d",	p->p_pptr ? p->p_pptr->p_pid : 0);
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
	PS_ADD("vsize",		"%u",	0); /* XXX */
	PS_ADD("rss",		"%u",	0); /* XXX */
	PS_ADD("rlim",		"%u",	0); /* XXX */
	PS_ADD("startcode",	"%u",	0); /* XXX */
	PS_ADD("endcode",	"%u",	0); /* XXX */
	PS_ADD("startstack",	"%u",	0); /* XXX */
	PS_ADD("kstkesp",	"%u",	0); /* XXX */
	PS_ADD("kstkeip",	"%u",	0); /* XXX */
	PS_ADD("signal",	"%d",	0); /* XXX */
	PS_ADD("blocked",	"%d",	0); /* XXX */
	PS_ADD("sigignore",	"%d",	0); /* XXX */
	PS_ADD("sigcatch",	"%d",	0); /* XXX */
	PS_ADD("wchan",		"%u",	0); /* XXX */
#undef PS_ADD
	ps += sprintf(ps, "\n");
	
	xlen = ps - psbuf;
	xlen -= uio->uio_offset;
	ps = psbuf + uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	return (xlen <= 0 ? 0 : uiomove(ps, xlen, uio));
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

int
linprocfs_doprocstatus(curp, p, pfs, uio)
    	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	char *ps, psbuf[1024];
	char *state;
	int i, xlen;

	ps = psbuf;

	if (p->p_stat > sizeof state_str / sizeof *state_str)
		state = state_str[0];
	else
		state = state_str[(int)p->p_stat];

#define PS_ADD ps += sprintf
	PS_ADD(ps, "Name:\t%s\n",	  p->p_comm); /* XXX escape */
	PS_ADD(ps, "State:\t%s\n",	  state);

	/*
	 * Credentials
	 */
	PS_ADD(ps, "Pid:\t%d\n",	  p->p_pid);
	PS_ADD(ps, "PPid:\t%d\n",	  p->p_pptr ? p->p_pptr->p_pid : 0);
	PS_ADD(ps, "Uid:\t%d %d %d %d\n", p->p_cred->p_ruid,
		                          p->p_ucred->cr_uid,
		                          p->p_cred->p_svuid,
		                          /* FreeBSD doesn't have fsuid */
		                          p->p_ucred->cr_uid);
	PS_ADD(ps, "Gid:\t%d %d %d %d\n", p->p_cred->p_rgid,
		                          p->p_ucred->cr_gid,
		                          p->p_cred->p_svgid,
		                          /* FreeBSD doesn't have fsgid */
		                          p->p_ucred->cr_gid);
	PS_ADD(ps, "Groups:\t");
	for (i = 0; i < p->p_ucred->cr_ngroups; i++)
		PS_ADD(ps, "%d ", p->p_ucred->cr_groups[i]);
	PS_ADD(ps, "\n");
	
	/*
	 * Memory
	 */
	PS_ADD(ps, "VmSize:\t%8u kB\n",	  B2K(p->p_vmspace->vm_map.size));
	PS_ADD(ps, "VmLck:\t%8u kB\n",    P2K(0)); /* XXX */
	/* XXX vm_rssize seems to always be zero, how can this be? */
	PS_ADD(ps, "VmRss:\t%8u kB\n",    P2K(p->p_vmspace->vm_rssize));
	PS_ADD(ps, "VmData:\t%8u kB\n",   P2K(p->p_vmspace->vm_dsize));
	PS_ADD(ps, "VmStk:\t%8u kB\n",    P2K(p->p_vmspace->vm_ssize));
	PS_ADD(ps, "VmExe:\t%8u kB\n",    P2K(p->p_vmspace->vm_tsize));
	PS_ADD(ps, "VmLib:\t%8u kB\n",    P2K(0)); /* XXX */

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
	PS_ADD(ps, "SigPnd:\t%08x\n",	  p->p_siglist.__bits[0]);
	PS_ADD(ps, "SigBlk:\t%08x\n",	  0); /* XXX */
	PS_ADD(ps, "SigIgn:\t%08x\n",	  p->p_sigignore.__bits[0]);
	PS_ADD(ps, "SigCgt:\t%08x\n",	  p->p_sigcatch.__bits[0]);
	
	/*
	 * Linux also prints the capability masks, but we don't have
	 * capabilities yet, and when we do get them they're likely to
	 * be meaningless to Linux programs, so we lie. XXX
	 */
	PS_ADD(ps, "CapInh:\t%016x\n",	  0);
	PS_ADD(ps, "CapPrm:\t%016x\n",	  0);
	PS_ADD(ps, "CapEff:\t%016x\n",	  0);
#undef PS_ADD
	
	xlen = ps - psbuf;
	xlen -= uio->uio_offset;
	ps = psbuf + uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	return (xlen <= 0 ? 0 : uiomove(ps, xlen, uio));
}
