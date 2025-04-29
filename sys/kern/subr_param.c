/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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

#include <sys/cdefs.h>
#include "opt_param.h"
#include "opt_msgbuf.h"
#include "opt_maxphys.h"
#include "opt_maxusers.h"

#include <sys/param.h>
#include <sys/_maxphys.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/msgbuf.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

/*
 * System parameter formulae.
 */

#ifndef HZ
#  define	HZ 1000
#  ifndef HZ_VM
#    define	HZ_VM 100
#  endif
#else
#  ifndef HZ_VM
#    define	HZ_VM HZ
#  endif
#endif
#define	NPROC (20 + 16 * maxusers)
#ifndef NBUF
#define NBUF 0
#endif
#ifndef MAXFILES
#define	MAXFILES (40 + 32 * maxusers)
#endif

static int sysctl_kern_vm_guest(SYSCTL_HANDLER_ARGS);

int	hz;				/* system clock's frequency */
int	tick;				/* usec per tick (1000000 / hz) */
time_t	tick_seconds_max;		/* max hz * seconds an integer can hold */
struct bintime tick_bt;			/* bintime per tick (1s / hz) */
sbintime_t tick_sbt;
int	maxusers;			/* base tunable */
int	maxproc;			/* maximum # of processes */
int	maxprocperuid;			/* max # of procs per user */
int	maxfiles;			/* sys. wide open files limit */
int	maxfilesperproc;		/* per-proc open files limit */
int	msgbufsize;			/* size of kernel message buffer */
int	nbuf;				/* number of bcache bufs */
int	bio_transient_maxcnt;
int	ngroups_max;			/* max # groups per process */
int	nswbuf;
pid_t	pid_max = PID_MAX;
u_long	maxswzone;			/* max swmeta KVA storage */
u_long	maxbcache;			/* max buffer cache KVA storage */
u_long	maxpipekva;			/* Limit on pipe KVA */
u_long	maxphys;			/* max raw I/O transfer size */
int	vm_guest = VM_GUEST_NO;		/* Running as virtual machine guest? */
u_long	maxtsiz;			/* max text size */
u_long	dfldsiz;			/* initial data size limit */
u_long	maxdsiz;			/* max data size */
u_long	dflssiz;			/* initial stack size limit */
u_long	maxssiz;			/* max stack size */
u_long	sgrowsiz;			/* amount to grow stack */

SYSCTL_INT(_kern, OID_AUTO, hz, CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &hz, 0,
    "Number of clock ticks per second");
SYSCTL_INT(_kern, OID_AUTO, hz_max, CTLFLAG_RD, SYSCTL_NULL_INT_PTR, HZ_MAXIMUM,
    "Maximum hz value supported");
SYSCTL_INT(_kern, OID_AUTO, hz_min, CTLFLAG_RD, SYSCTL_NULL_INT_PTR, HZ_MINIMUM,
    "Minimum hz value supported");
SYSCTL_INT(_kern, OID_AUTO, nbuf, CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &nbuf, 0,
    "Number of buffers in the buffer cache");
SYSCTL_INT(_kern, OID_AUTO, nswbuf, CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &nswbuf, 0,
    "Number of swap buffers");
SYSCTL_INT(_kern, OID_AUTO, msgbufsize, CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &msgbufsize, 0,
    "Size of the kernel message buffer");
SYSCTL_LONG(_kern, OID_AUTO, maxswzone, CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &maxswzone, 0,
    "Maximum memory for swap metadata");
SYSCTL_LONG(_kern, OID_AUTO, maxbcache, CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &maxbcache, 0,
    "Maximum value of vfs.maxbufspace");
SYSCTL_INT(_kern, OID_AUTO, bio_transient_maxcnt, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &bio_transient_maxcnt, 0,
    "Maximum number of transient BIOs mappings");
SYSCTL_ULONG(_kern, OID_AUTO, maxtsiz, CTLFLAG_RWTUN | CTLFLAG_NOFETCH, &maxtsiz, 0,
    "Maximum text size");
SYSCTL_ULONG(_kern, OID_AUTO, dfldsiz, CTLFLAG_RWTUN | CTLFLAG_NOFETCH, &dfldsiz, 0,
    "Initial data size limit");
SYSCTL_ULONG(_kern, OID_AUTO, maxdsiz, CTLFLAG_RWTUN | CTLFLAG_NOFETCH, &maxdsiz, 0,
    "Maximum data size");
SYSCTL_ULONG(_kern, OID_AUTO, dflssiz, CTLFLAG_RWTUN | CTLFLAG_NOFETCH, &dflssiz, 0,
    "Initial stack size limit");
SYSCTL_ULONG(_kern, OID_AUTO, maxssiz, CTLFLAG_RWTUN | CTLFLAG_NOFETCH, &maxssiz, 0,
    "Maximum stack size");
SYSCTL_ULONG(_kern, OID_AUTO, sgrowsiz, CTLFLAG_RWTUN | CTLFLAG_NOFETCH, &sgrowsiz, 0,
    "Amount to grow stack on a stack fault");
SYSCTL_PROC(_kern, OID_AUTO, vm_guest,
    CTLFLAG_RD | CTLTYPE_STRING | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_kern_vm_guest, "A",
    "Virtual machine guest detected?");

/*
 * The elements of this array are ordered based upon the values of the
 * corresponding enum VM_GUEST members.
 */
static const char *const vm_guest_sysctl_names[] = {
	[VM_GUEST_NO] = "none",
	[VM_GUEST_VM] = "generic",
	[VM_GUEST_XEN] = "xen",
	[VM_GUEST_HV] = "hv",
	[VM_GUEST_VMWARE] = "vmware",
	[VM_GUEST_KVM] = "kvm",
	[VM_GUEST_BHYVE] = "bhyve",
	[VM_GUEST_VBOX] = "vbox",
	[VM_GUEST_PARALLELS] = "parallels",
	[VM_GUEST_NVMM] = "nvmm",
};
_Static_assert(nitems(vm_guest_sysctl_names) == VM_GUEST_LAST,
    "new vm guest type not added to vm_guest_sysctl_names");

/*
 * Boot time overrides that are not scaled against main memory
 */
void
init_param1(void)
{

	TSENTER();

	/*
	 * arm64 and riscv currently hard-code the thread0 kstack size
	 * to KSTACK_PAGES, ignoring the tunable.
	 */
	TUNABLE_INT_FETCH("kern.kstack_pages", &kstack_pages);

	hz = -1;
	TUNABLE_INT_FETCH("kern.hz", &hz);
	if (hz == -1)
		hz = vm_guest > VM_GUEST_NO ? HZ_VM : HZ;

	/* range check the "hz" value */
	if (__predict_false(hz < HZ_MINIMUM))
		hz = HZ_MINIMUM;
	else if (__predict_false(hz > HZ_MAXIMUM))
		hz = HZ_MAXIMUM;

	tick = 1000000 / hz;
	tick_sbt = SBT_1S / hz;
	tick_bt = sbttobt(tick_sbt);
	tick_seconds_max = INT_MAX / hz;

	/*
	 * Arrange for ticks to wrap 10 minutes after boot to help catch
	 * sign problems sooner.
	 */
	ticksl = INT_MAX - (hz * 10 * 60);

	vn_lock_pair_pause_max = hz / 100;
	if (vn_lock_pair_pause_max == 0)
		vn_lock_pair_pause_max = 1;

#ifdef VM_SWZONE_SIZE_MAX
	maxswzone = VM_SWZONE_SIZE_MAX;
#endif
	TUNABLE_LONG_FETCH("kern.maxswzone", &maxswzone);
#ifdef VM_BCACHE_SIZE_MAX
	maxbcache = VM_BCACHE_SIZE_MAX;
#endif
	TUNABLE_LONG_FETCH("kern.maxbcache", &maxbcache);
	msgbufsize = MSGBUF_SIZE;
	TUNABLE_INT_FETCH("kern.msgbufsize", &msgbufsize);

	maxtsiz = MAXTSIZ;
	TUNABLE_ULONG_FETCH("kern.maxtsiz", &maxtsiz);
	dfldsiz = DFLDSIZ;
	TUNABLE_ULONG_FETCH("kern.dfldsiz", &dfldsiz);
	maxdsiz = MAXDSIZ;
	TUNABLE_ULONG_FETCH("kern.maxdsiz", &maxdsiz);
	dflssiz = DFLSSIZ;
	TUNABLE_ULONG_FETCH("kern.dflssiz", &dflssiz);
	maxssiz = MAXSSIZ;
	TUNABLE_ULONG_FETCH("kern.maxssiz", &maxssiz);
	sgrowsiz = SGROWSIZ;
	TUNABLE_ULONG_FETCH("kern.sgrowsiz", &sgrowsiz);

	/*
	 * Let the administrator set {NGROUPS_MAX}.
	 *
	 * Values less than NGROUPS_MAX would violate POSIX/SuS (see the
	 * specification for <limits.h>, paragraph "Runtime Increasable
	 * Values").
	 *
	 * On the other hand, INT_MAX would result in an overflow for the common
	 * 'ngroups_max + 1' computation (to obtain the size of the internal
	 * groups array, its first element being reserved for the effective
	 * GID).  Also, the number of allocated bytes for the group array must
	 * not overflow on 32-bit machines.  For all these reasons, we limit the
	 * number of supplementary groups to some very high number that we
	 * expect will never be reached in all practical uses and ensures we
	 * avoid the problems just exposed, even if 'gid_t' was to be enlarged
	 * by a magnitude.
	 */
	ngroups_max = NGROUPS_MAX;
	TUNABLE_INT_FETCH("kern.ngroups", &ngroups_max);
	if (ngroups_max < NGROUPS_MAX)
		ngroups_max = NGROUPS_MAX;
	else {
		const int ngroups_max_max = (1 << 24) - 1;

		if (ngroups_max > ngroups_max_max)
			ngroups_max = ngroups_max_max;
	}

	/*
	 * Only allow to lower the maximal pid.
	 * Prevent setting up a non-bootable system if pid_max is too low.
	 */
	TUNABLE_INT_FETCH("kern.pid_max", &pid_max);
	if (pid_max > PID_MAX)
		pid_max = PID_MAX;
	else if (pid_max < 300)
		pid_max = 300;

	TUNABLE_INT_FETCH("vfs.unmapped_buf_allowed", &unmapped_buf_allowed);
	TSEXIT();
}

/*
 * Boot time overrides that are scaled against main memory
 */
void
init_param2(long physpages)
{

	TSENTER();
	/* Base parameters */
	maxusers = MAXUSERS;
	TUNABLE_INT_FETCH("kern.maxusers", &maxusers);
	if (maxusers == 0) {
		maxusers = physpages / (2 * 1024 * 1024 / PAGE_SIZE);
		if (maxusers < 32)
			maxusers = 32;
#ifdef VM_MAX_AUTOTUNE_MAXUSERS
                if (maxusers > VM_MAX_AUTOTUNE_MAXUSERS)
                        maxusers = VM_MAX_AUTOTUNE_MAXUSERS;
#endif
                /*
                 * Scales down the function in which maxusers grows once
                 * we hit 384.
                 */
                if (maxusers > 384)
                        maxusers = 384 + ((maxusers - 384) / 8);
        }

	/*
	 * The following can be overridden after boot via sysctl.  Note:
	 * unless overridden, these macros are ultimately based on maxusers.
	 * Limit maxproc so that kmap entries cannot be exhausted by
	 * processes.
	 */
	maxproc = NPROC;
	TUNABLE_INT_FETCH("kern.maxproc", &maxproc);
	if (maxproc > (physpages / 12))
		maxproc = physpages / 12;
	if (maxproc > pid_max)
		maxproc = pid_max;
	maxprocperuid = (maxproc * 9) / 10;

	/*
	 * The default limit for maxfiles is 1/12 of the number of
	 * physical page but not less than 16 times maxusers.
	 * At most it can be 1/6 the number of physical pages.
	 */
	maxfiles = imax(MAXFILES, physpages / 8);
	TUNABLE_INT_FETCH("kern.maxfiles", &maxfiles);
	if (maxfiles > (physpages / 4))
		maxfiles = physpages / 4;
	maxfilesperproc = (maxfiles / 10) * 9;
	TUNABLE_INT_FETCH("kern.maxfilesperproc", &maxfilesperproc);

	/*
	 * Cannot be changed after boot.
	 */
	nbuf = NBUF;
	TUNABLE_INT_FETCH("kern.nbuf", &nbuf);
	TUNABLE_INT_FETCH("kern.bio_transient_maxcnt", &bio_transient_maxcnt);
	maxphys = MAXPHYS;
	TUNABLE_ULONG_FETCH("kern.maxphys", &maxphys);
	if (maxphys == 0) {
		maxphys = MAXPHYS;
	} else if (__bitcountl(maxphys) != 1) {	/* power of two */
		if (flsl(maxphys) == NBBY * sizeof(maxphys))
			maxphys = MAXPHYS;
		else
			maxphys = 1UL << flsl(maxphys);
	}
	if (maxphys < PAGE_SIZE)
		maxphys = MAXPHYS;

	/*
	 * Physical buffers are pre-allocated buffers (struct buf) that
	 * are used as temporary holders for I/O, such as paging I/O.
	 */
	TUNABLE_INT_FETCH("kern.nswbuf", &nswbuf);

	/*
	 * The default for maxpipekva is min(1/64 of the kernel address space,
	 * max(1/64 of main memory, 512KB)).  See sys_pipe.c for more details.
	 */
	maxpipekva = ptoa(physpages / 64);
	TUNABLE_LONG_FETCH("kern.ipc.maxpipekva", &maxpipekva);
	if (maxpipekva < 512 * 1024)
		maxpipekva = 512 * 1024;
	if (maxpipekva > (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) / 64)
		maxpipekva = (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) /
		    64;
	TSEXIT();
}

/*
 * Sysctl stringifying handler for kern.vm_guest.
 */
static int
sysctl_kern_vm_guest(SYSCTL_HANDLER_ARGS)
{
	return (SYSCTL_OUT_STR(req, vm_guest_sysctl_names[vm_guest]));
}
