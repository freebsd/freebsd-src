/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
 *
 * Quite extensively rewritten by Poul-Henning Kamp of the FreeBSD
 * project, to make these variables more userfriendly.
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
 *	@(#)kern_sysctl.c	8.4 (Berkeley) 4/14/94
 * $FreeBSD$
 */

#include "opt_posix.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/jail.h>
#include <sys/smp.h>
#include <sys/unistd.h>

SYSCTL_NODE(, 0,	  sysctl, CTLFLAG_RW, 0,
	"Sysctl internal magic");
SYSCTL_NODE(, CTL_KERN,	  kern,   CTLFLAG_RW, 0,
	"High kernel, proc, limits &c");
SYSCTL_NODE(, CTL_VM,	  vm,     CTLFLAG_RW, 0,
	"Virtual memory");
SYSCTL_NODE(, CTL_VFS,	  vfs,     CTLFLAG_RW, 0,
	"File system");
SYSCTL_NODE(, CTL_NET,	  net,    CTLFLAG_RW, 0,
	"Network, (see socket.h)");
SYSCTL_NODE(, CTL_DEBUG,  debug,  CTLFLAG_RW, 0,
	"Debugging");
SYSCTL_NODE(_debug, OID_AUTO,  sizeof,  CTLFLAG_RW, 0,
	"Sizeof various things");
SYSCTL_NODE(, CTL_HW,	  hw,     CTLFLAG_RW, 0,
	"hardware");
SYSCTL_NODE(, CTL_MACHDEP, machdep, CTLFLAG_RW, 0,
	"machine dependent");
SYSCTL_NODE(, CTL_USER,	  user,   CTLFLAG_RW, 0,
	"user-level");
SYSCTL_NODE(, CTL_P1003_1B,  p1003_1b,   CTLFLAG_RW, 0,
	"p1003_1b, (see p1003_1b.h)");

SYSCTL_NODE(, OID_AUTO,  compat, CTLFLAG_RW, 0,
	"Compatibility code");
SYSCTL_NODE(, OID_AUTO, security, CTLFLAG_RW, 0, 
     	"Security");
#ifdef REGRESSION
SYSCTL_NODE(, OID_AUTO, regression, CTLFLAG_RW, 0,
     "Regression test MIB");
#endif

SYSCTL_STRING(_kern, KERN_OSRELEASE, osrelease, CTLFLAG_RD,
    osrelease, 0, "Operating system release");

SYSCTL_INT(_kern, KERN_OSREV, osrevision, CTLFLAG_RD,
    0, BSD, "Operating system revision");

SYSCTL_STRING(_kern, KERN_VERSION, version, CTLFLAG_RD,
    version, 0, "Kernel version");

SYSCTL_STRING(_kern, KERN_OSTYPE, ostype, CTLFLAG_RD,
    ostype, 0, "Operating system type");

extern int osreldate;
SYSCTL_INT(_kern, KERN_OSRELDATE, osreldate, CTLFLAG_RD,
    &osreldate, 0, "Operating system release date");

SYSCTL_INT(_kern, KERN_MAXPROC, maxproc, CTLFLAG_RD,
    &maxproc, 0, "Maximum number of processes");

SYSCTL_INT(_kern, KERN_MAXPROCPERUID, maxprocperuid, CTLFLAG_RW,
    &maxprocperuid, 0, "Maximum processes allowed per userid");

SYSCTL_INT(_kern, OID_AUTO, maxusers, CTLFLAG_RD,
    &maxusers, 0, "Hint for kernel tuning");

SYSCTL_INT(_kern, KERN_ARGMAX, argmax, CTLFLAG_RD,
    0, ARG_MAX, "Maximum bytes of argument to execve(2)");

SYSCTL_INT(_kern, KERN_POSIX1, posix1version, CTLFLAG_RD,
    0, _POSIX_VERSION, "Version of POSIX attempting to comply to");

SYSCTL_INT(_kern, KERN_NGROUPS, ngroups, CTLFLAG_RD,
    0, NGROUPS_MAX, "Maximum number of groups a user can belong to");

SYSCTL_INT(_kern, KERN_JOB_CONTROL, job_control, CTLFLAG_RD,
    0, 1, "Whether job control is available");

#ifdef _POSIX_SAVED_IDS
SYSCTL_INT(_kern, KERN_SAVED_IDS, saved_ids, CTLFLAG_RD,
    0, 1, "Whether saved set-group/user ID is available");
#else
SYSCTL_INT(_kern, KERN_SAVED_IDS, saved_ids, CTLFLAG_RD,
    0, 0, "Whether saved set-group/user ID is available");
#endif

char kernelname[MAXPATHLEN] = "/kernel";	/* XXX bloat */

SYSCTL_STRING(_kern, KERN_BOOTFILE, bootfile, CTLFLAG_RW,
    kernelname, sizeof kernelname, "Name of kernel file booted");

#ifdef SMP
SYSCTL_INT(_hw, HW_NCPU, ncpu, CTLFLAG_RD,
    &mp_ncpus, 0, "Number of active CPUs");
#else
SYSCTL_INT(_hw, HW_NCPU, ncpu, CTLFLAG_RD,
    0, 1, "Number of active CPUs");
#endif

SYSCTL_INT(_hw, HW_BYTEORDER, byteorder, CTLFLAG_RD,
    0, BYTE_ORDER, "System byte order");

SYSCTL_INT(_hw, HW_PAGESIZE, pagesize, CTLFLAG_RD,
    0, PAGE_SIZE, "System memory page size");

static int
sysctl_hw_physmem(SYSCTL_HANDLER_ARGS)
{
	u_long val;

	val = ctob(physmem);
	return (sysctl_handle_long(oidp, &val, 0, req));
}

SYSCTL_PROC(_hw, HW_PHYSMEM, physmem, CTLTYPE_ULONG | CTLFLAG_RD,
	0, 0, sysctl_hw_physmem, "LU", "");

static int
sysctl_hw_usermem(SYSCTL_HANDLER_ARGS)
{
	u_long val;

	val = ctob(physmem - cnt.v_wire_count);
	return (sysctl_handle_long(oidp, &val, 0, req));
}

SYSCTL_PROC(_hw, HW_USERMEM, usermem, CTLTYPE_ULONG | CTLFLAG_RD,
	0, 0, sysctl_hw_usermem, "LU", "");

SYSCTL_ULONG(_hw, OID_AUTO, availpages, CTLFLAG_RD, &physmem, 0, "");

static char	machine_arch[] = MACHINE_ARCH;
SYSCTL_STRING(_hw, HW_MACHINE_ARCH, machine_arch, CTLFLAG_RD,
    machine_arch, 0, "System architecture");

char hostname[MAXHOSTNAMELEN];

static int
sysctl_hostname(SYSCTL_HANDLER_ARGS)
{
	struct prison *pr;
	char tmphostname[MAXHOSTNAMELEN];
	int error;

	pr = req->td->td_ucred->cr_prison;
	if (pr != NULL) {
		if (!jail_set_hostname_allowed && req->newptr)
			return (EPERM);
		/*
		 * Process is in jail, so make a local copy of jail
		 * hostname to get/set so we don't have to hold the jail
		 * mutex during the sysctl copyin/copyout activities.
		 */
		mtx_lock(&pr->pr_mtx);
		bcopy(pr->pr_host, tmphostname, MAXHOSTNAMELEN);
		mtx_unlock(&pr->pr_mtx);

		error = sysctl_handle_string(oidp, tmphostname,
		    sizeof pr->pr_host, req);

		if (req->newptr != NULL && error == 0) {
			/*
			 * Copy the locally set hostname to the jail, if
			 * appropriate.
			 */
			mtx_lock(&pr->pr_mtx);
			bcopy(tmphostname, pr->pr_host, MAXHOSTNAMELEN);
			mtx_unlock(&pr->pr_mtx);
		}
	} else
		error = sysctl_handle_string(oidp,
		    hostname, sizeof hostname, req);
	return (error);
}

SYSCTL_PROC(_kern, KERN_HOSTNAME, hostname,
       CTLTYPE_STRING|CTLFLAG_RW|CTLFLAG_PRISON,
       0, 0, sysctl_hostname, "A", "Hostname");

static int	regression_securelevel_nonmonotonic = 0;

#ifdef REGRESSION
SYSCTL_INT(_regression, OID_AUTO, securelevel_nonmonotonic, CTLFLAG_RW,
    &regression_securelevel_nonmonotonic, 0, "securelevel may be lowered");
#endif

int securelevel = -1;
struct mtx securelevel_mtx;

MTX_SYSINIT(securelevel_lock, &securelevel_mtx, "securelevel mutex lock",
    MTX_DEF);

static int
sysctl_kern_securelvl(SYSCTL_HANDLER_ARGS)
{
	struct prison *pr;
	int error, level;

	pr = req->td->td_ucred->cr_prison;

	/*
	 * If the process is in jail, return the maximum of the global and
	 * local levels; otherwise, return the global level.
	 */
	if (pr != NULL) {
		mtx_lock(&pr->pr_mtx);
		level = imax(securelevel, pr->pr_securelevel);
		mtx_unlock(&pr->pr_mtx);
	} else
		level = securelevel;
	error = sysctl_handle_int(oidp, &level, 0, req);
	if (error || !req->newptr)
		return (error);
	/*
	 * Permit update only if the new securelevel exceeds the
	 * global level, and local level if any.
	 */
	if (pr != NULL) {
		mtx_lock(&pr->pr_mtx);
		if (!regression_securelevel_nonmonotonic &&
		    (level < imax(securelevel, pr->pr_securelevel))) {
			mtx_unlock(&pr->pr_mtx);
			return (EPERM);
		}
		pr->pr_securelevel = level;
		mtx_unlock(&pr->pr_mtx);
	} else {
		mtx_lock(&securelevel_mtx);
		if (!regression_securelevel_nonmonotonic &&
		    (level < securelevel)) {
			mtx_unlock(&securelevel_mtx);
			return (EPERM);
		}
		securelevel = level;
		mtx_unlock(&securelevel_mtx);
	}
	return (error);
}

SYSCTL_PROC(_kern, KERN_SECURELVL, securelevel,
    CTLTYPE_INT|CTLFLAG_RW|CTLFLAG_PRISON, 0, 0, sysctl_kern_securelvl,
    "I", "Current secure level");

char domainname[MAXHOSTNAMELEN];
SYSCTL_STRING(_kern, KERN_NISDOMAINNAME, domainname, CTLFLAG_RW,
    &domainname, sizeof(domainname), "Name of the current YP/NIS domain");

u_long hostid;
SYSCTL_ULONG(_kern, KERN_HOSTID, hostid, CTLFLAG_RW, &hostid, 0, "Host ID");

/*
 * This is really cheating.  These actually live in the libc, something
 * which I'm not quite sure is a good idea anyway, but in order for
 * getnext and friends to actually work, we define dummies here.
 */
SYSCTL_STRING(_user, USER_CS_PATH, cs_path, CTLFLAG_RD,
    "", 0, "PATH that finds all the standard utilities");
SYSCTL_INT(_user, USER_BC_BASE_MAX, bc_base_max, CTLFLAG_RD,
    0, 0, "Max ibase/obase values in bc(1)");
SYSCTL_INT(_user, USER_BC_DIM_MAX, bc_dim_max, CTLFLAG_RD,
    0, 0, "Max array size in bc(1)");
SYSCTL_INT(_user, USER_BC_SCALE_MAX, bc_scale_max, CTLFLAG_RD,
    0, 0, "Max scale value in bc(1)");
SYSCTL_INT(_user, USER_BC_STRING_MAX, bc_string_max, CTLFLAG_RD,
    0, 0, "Max string length in bc(1)");
SYSCTL_INT(_user, USER_COLL_WEIGHTS_MAX, coll_weights_max, CTLFLAG_RD,
    0, 0, "Maximum number of weights assigned to an LC_COLLATE locale entry");
SYSCTL_INT(_user, USER_EXPR_NEST_MAX, expr_nest_max, CTLFLAG_RD, 0, 0, "");
SYSCTL_INT(_user, USER_LINE_MAX, line_max, CTLFLAG_RD,
    0, 0, "Max length (bytes) of a text-processing utility's input line");
SYSCTL_INT(_user, USER_RE_DUP_MAX, re_dup_max, CTLFLAG_RD,
    0, 0, "Maximum number of repeats of a regexp permitted");
SYSCTL_INT(_user, USER_POSIX2_VERSION, posix2_version, CTLFLAG_RD,
    0, 0,
    "The version of POSIX 1003.2 with which the system attempts to comply");
SYSCTL_INT(_user, USER_POSIX2_C_BIND, posix2_c_bind, CTLFLAG_RD,
    0, 0, "Whether C development supports the C bindings option");
SYSCTL_INT(_user, USER_POSIX2_C_DEV, posix2_c_dev, CTLFLAG_RD,
    0, 0, "Whether system supports the C development utilities option");
SYSCTL_INT(_user, USER_POSIX2_CHAR_TERM, posix2_char_term, CTLFLAG_RD,
    0, 0, "");
SYSCTL_INT(_user, USER_POSIX2_FORT_DEV, posix2_fort_dev, CTLFLAG_RD,
    0, 0, "Whether system supports FORTRAN development utilities");
SYSCTL_INT(_user, USER_POSIX2_FORT_RUN, posix2_fort_run, CTLFLAG_RD,
    0, 0, "Whether system supports FORTRAN runtime utilities");
SYSCTL_INT(_user, USER_POSIX2_LOCALEDEF, posix2_localedef, CTLFLAG_RD,
    0, 0, "Whether system supports creation of locales");
SYSCTL_INT(_user, USER_POSIX2_SW_DEV, posix2_sw_dev, CTLFLAG_RD,
    0, 0, "Whether system supports software development utilities");
SYSCTL_INT(_user, USER_POSIX2_UPE, posix2_upe, CTLFLAG_RD,
    0, 0, "Whether system supports the user portability utilities");
SYSCTL_INT(_user, USER_STREAM_MAX, stream_max, CTLFLAG_RD,
    0, 0, "Min Maximum number of streams a process may have open at one time");
SYSCTL_INT(_user, USER_TZNAME_MAX, tzname_max, CTLFLAG_RD,
    0, 0, "Min Maximum number of types supported for timezone names");

#include <sys/vnode.h>
SYSCTL_INT(_debug_sizeof, OID_AUTO, vnode, CTLFLAG_RD,
    0, sizeof(struct vnode), "sizeof(struct vnode)");

SYSCTL_INT(_debug_sizeof, OID_AUTO, proc, CTLFLAG_RD,
    0, sizeof(struct proc), "sizeof(struct proc)");

#include <sys/conf.h>
SYSCTL_INT(_debug_sizeof, OID_AUTO, cdev, CTLFLAG_RD,
    0, sizeof(struct cdev), "sizeof(struct cdev)");

#include <sys/bio.h>
#include <sys/buf.h>
SYSCTL_INT(_debug_sizeof, OID_AUTO, bio, CTLFLAG_RD,
    0, sizeof(struct bio), "sizeof(struct bio)");
SYSCTL_INT(_debug_sizeof, OID_AUTO, buf, CTLFLAG_RD,
    0, sizeof(struct buf), "sizeof(struct buf)");

#include <sys/user.h>
SYSCTL_INT(_debug_sizeof, OID_AUTO, kinfo_proc, CTLFLAG_RD,
    0, sizeof(struct kinfo_proc), "sizeof(struct kinfo_proc)");

SYSCTL_STRING(_kern, OID_AUTO, fallback_elf_brand, CTLFLAG_RD,
    "kern.fallback_elf_brand is deprecated, use kern.elf32.fallback_brand or "
    "kern.elf64.fallback_brand" , 0, "");
