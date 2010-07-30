/*-
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
 *	@(#)param.c	8.3 (Berkeley) 8/20/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_param.h"
#include "opt_maxusers.h"

#include <sys/limits.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <vm/vm_param.h>

/*
 * System parameter formulae.
 */

#ifndef HZ
#  if defined(__mips__) || defined(__arm__)
#    define	HZ 100
#  else
#    define	HZ 1000
#  endif
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
#define	MAXFILES (maxproc * 2)
#endif

static int sysctl_kern_vm_guest(SYSCTL_HANDLER_ARGS);

int	hz;
int	tick;
int	maxusers;			/* base tunable */
int	maxproc;			/* maximum # of processes */
int	maxprocperuid;			/* max # of procs per user */
int	maxfiles;			/* sys. wide open files limit */
int	maxfilesperproc;		/* per-proc open files limit */
int	ncallout;			/* maximum # of timer events */
int	nbuf;
int	ngroups_max;			/* max # groups per process */
int	nswbuf;
long	maxswzone;			/* max swmeta KVA storage */
long	maxbcache;			/* max buffer cache KVA storage */
long	maxpipekva;			/* Limit on pipe KVA */
int 	vm_guest;			/* Running as virtual machine guest? */
u_long	maxtsiz;			/* max text size */
u_long	dfldsiz;			/* initial data size limit */
u_long	maxdsiz;			/* max data size */
u_long	dflssiz;			/* initial stack size limit */
u_long	maxssiz;			/* max stack size */
u_long	sgrowsiz;			/* amount to grow stack */

SYSCTL_INT(_kern, OID_AUTO, hz, CTLFLAG_RDTUN, &hz, 0,
    "Number of clock ticks per second");
SYSCTL_INT(_kern, OID_AUTO, ncallout, CTLFLAG_RDTUN, &ncallout, 0,
    "Number of pre-allocated timer events");
SYSCTL_INT(_kern, OID_AUTO, nbuf, CTLFLAG_RDTUN, &nbuf, 0,
    "Number of buffers in the buffer cache");
SYSCTL_INT(_kern, OID_AUTO, nswbuf, CTLFLAG_RDTUN, &nswbuf, 0,
    "Number of swap buffers");
SYSCTL_LONG(_kern, OID_AUTO, maxswzone, CTLFLAG_RDTUN, &maxswzone, 0,
    "Maximum memory for swap metadata");
SYSCTL_LONG(_kern, OID_AUTO, maxbcache, CTLFLAG_RDTUN, &maxbcache, 0,
    "Maximum value of vfs.maxbufspace");
SYSCTL_ULONG(_kern, OID_AUTO, maxtsiz, CTLFLAG_RDTUN, &maxtsiz, 0,
    "Maximum text size");
SYSCTL_ULONG(_kern, OID_AUTO, dfldsiz, CTLFLAG_RDTUN, &dfldsiz, 0,
    "Initial data size limit");
SYSCTL_ULONG(_kern, OID_AUTO, maxdsiz, CTLFLAG_RDTUN, &maxdsiz, 0,
    "Maximum data size");
SYSCTL_ULONG(_kern, OID_AUTO, dflssiz, CTLFLAG_RDTUN, &dflssiz, 0,
    "Initial stack size limit");
SYSCTL_ULONG(_kern, OID_AUTO, maxssiz, CTLFLAG_RDTUN, &maxssiz, 0,
    "Maximum stack size");
SYSCTL_ULONG(_kern, OID_AUTO, sgrowsiz, CTLFLAG_RDTUN, &sgrowsiz, 0,
    "Amount to grow stack on a stack fault");
SYSCTL_PROC(_kern, OID_AUTO, vm_guest, CTLFLAG_RD | CTLTYPE_STRING,
    NULL, 0, sysctl_kern_vm_guest, "A",
    "Virtual machine guest detected? (none|generic|xen)");

/*
 * These have to be allocated somewhere; allocating
 * them here forces loader errors if this file is omitted
 * (if they've been externed everywhere else; hah!).
 */
struct	buf *swbuf;

/*
 * The elements of this array are ordered based upon the values of the
 * corresponding enum VM_GUEST members.
 */
static const char *const vm_guest_sysctl_names[] = {
	"none",
	"generic",
	"xen",
	NULL
};

#ifndef XEN
static const char *const vm_bnames[] = {
	"QEMU",				/* QEMU */
	"Plex86",			/* Plex86 */
	"Bochs",			/* Bochs */
	NULL
};

static const char *const vm_pnames[] = {
	"VMware Virtual Platform",	/* VMWare VM */
	"Virtual Machine",		/* Microsoft VirtualPC */
	"VirtualBox",			/* Sun xVM VirtualBox */
	"Parallels Virtual Platform",	/* Parallels VM */
	NULL
};


/*
 * Detect known Virtual Machine hosts by inspecting the emulated BIOS.
 */
static enum VM_GUEST
detect_virtual(void)
{
	char *sysenv;
	int i;

	sysenv = getenv("smbios.bios.vendor");
	if (sysenv != NULL) {
		for (i = 0; vm_bnames[i] != NULL; i++)
			if (strcmp(sysenv, vm_bnames[i]) == 0) {
				freeenv(sysenv);
				return (VM_GUEST_VM);
			}
		freeenv(sysenv);
	}
	sysenv = getenv("smbios.system.product");
	if (sysenv != NULL) {
		for (i = 0; vm_pnames[i] != NULL; i++)
			if (strcmp(sysenv, vm_pnames[i]) == 0) {
				freeenv(sysenv);
				return (VM_GUEST_VM);
			}
		freeenv(sysenv);
	}
	return (VM_GUEST_NO);
}
#endif

/*
 * Boot time overrides that are not scaled against main memory
 */
void
init_param1(void)
{
#ifndef XEN
	vm_guest = detect_virtual();
#else
	vm_guest = VM_GUEST_XEN;
#endif
	hz = -1;
	TUNABLE_INT_FETCH("kern.hz", &hz);
	if (hz == -1)
		hz = vm_guest > VM_GUEST_NO ? HZ_VM : HZ;
	tick = 1000000 / hz;

#ifdef VM_SWZONE_SIZE_MAX
	maxswzone = VM_SWZONE_SIZE_MAX;
#endif
	TUNABLE_LONG_FETCH("kern.maxswzone", &maxswzone);
#ifdef VM_BCACHE_SIZE_MAX
	maxbcache = VM_BCACHE_SIZE_MAX;
#endif
	TUNABLE_LONG_FETCH("kern.maxbcache", &maxbcache);

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
	 * Let the administrator set {NGROUPS_MAX}, but disallow values
	 * less than NGROUPS_MAX which would violate POSIX.1-2008 or
	 * greater than INT_MAX-1 which would result in overflow.
	 */
	ngroups_max = NGROUPS_MAX;
	TUNABLE_INT_FETCH("kern.ngroups", &ngroups_max);
	if (ngroups_max < NGROUPS_MAX)
		ngroups_max = NGROUPS_MAX;
}

/*
 * Boot time overrides that are scaled against main memory
 */
void
init_param2(long physpages)
{

	/* Base parameters */
	maxusers = MAXUSERS;
	TUNABLE_INT_FETCH("kern.maxusers", &maxusers);
	if (maxusers == 0) {
		maxusers = physpages / (2 * 1024 * 1024 / PAGE_SIZE);
		if (maxusers < 32)
			maxusers = 32;
		if (maxusers > 384)
			maxusers = 384;
	}

	/*
	 * The following can be overridden after boot via sysctl.  Note:
	 * unless overriden, these macros are ultimately based on maxusers.
	 */
	maxproc = NPROC;
	TUNABLE_INT_FETCH("kern.maxproc", &maxproc);
	/*
	 * Limit maxproc so that kmap entries cannot be exhausted by
	 * processes.
	 */
	if (maxproc > (physpages / 12))
		maxproc = physpages / 12;
	maxfiles = MAXFILES;
	TUNABLE_INT_FETCH("kern.maxfiles", &maxfiles);
	maxprocperuid = (maxproc * 9) / 10;
	maxfilesperproc = (maxfiles * 9) / 10;
	
	/*
	 * Cannot be changed after boot.
	 */
	nbuf = NBUF;
	TUNABLE_INT_FETCH("kern.nbuf", &nbuf);

	ncallout = 16 + maxproc + maxfiles;
	TUNABLE_INT_FETCH("kern.ncallout", &ncallout);
}

/*
 * Boot time overrides that are scaled against the kmem map
 */
void
init_param3(long kmempages)
{

	/*
	 * The default for maxpipekva is max(5% of the kmem map, 512KB).
	 * See sys_pipe.c for more details.
	 */
	maxpipekva = (kmempages / 20) * PAGE_SIZE;
	if (maxpipekva < 512 * 1024)
		maxpipekva = 512 * 1024;
	TUNABLE_LONG_FETCH("kern.ipc.maxpipekva", &maxpipekva);
}

/*
 * Sysctl stringiying handler for kern.vm_guest.
 */
static int
sysctl_kern_vm_guest(SYSCTL_HANDLER_ARGS)
{
	return (SYSCTL_OUT(req, vm_guest_sysctl_names[vm_guest], 
	    strlen(vm_guest_sysctl_names[vm_guest])));
}
