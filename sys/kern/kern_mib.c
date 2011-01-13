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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_posix.h"
#include "opt_config.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sbuf.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/jail.h>
#include <sys/smp.h>
#include <sys/sx.h>
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

SYSCTL_STRING(_kern, OID_AUTO, ident, CTLFLAG_RD|CTLFLAG_MPSAFE,
    kern_ident, 0, "Kernel identifier");

SYSCTL_STRING(_kern, KERN_OSRELEASE, osrelease, CTLFLAG_RD|CTLFLAG_MPSAFE,
    osrelease, 0, "Operating system release");

SYSCTL_INT(_kern, KERN_OSREV, osrevision, CTLFLAG_RD,
    0, BSD, "Operating system revision");

SYSCTL_STRING(_kern, KERN_VERSION, version, CTLFLAG_RD|CTLFLAG_MPSAFE,
    version, 0, "Kernel version");

SYSCTL_STRING(_kern, KERN_OSTYPE, ostype, CTLFLAG_RD|CTLFLAG_MPSAFE,
    ostype, 0, "Operating system type");

/*
 * NOTICE: The *userland* release date is available in
 * /usr/include/osreldate.h
 */
SYSCTL_INT(_kern, KERN_OSRELDATE, osreldate, CTLFLAG_RD,
    &osreldate, 0, "Kernel release date");

SYSCTL_INT(_kern, KERN_MAXPROC, maxproc, CTLFLAG_RDTUN,
    &maxproc, 0, "Maximum number of processes");

SYSCTL_INT(_kern, KERN_MAXPROCPERUID, maxprocperuid, CTLFLAG_RW,
    &maxprocperuid, 0, "Maximum processes allowed per userid");

SYSCTL_INT(_kern, OID_AUTO, maxusers, CTLFLAG_RDTUN,
    &maxusers, 0, "Hint for kernel tuning");

SYSCTL_INT(_kern, KERN_ARGMAX, argmax, CTLFLAG_RD,
    0, ARG_MAX, "Maximum bytes of argument to execve(2)");

SYSCTL_INT(_kern, KERN_POSIX1, posix1version, CTLFLAG_RD,
    0, _POSIX_VERSION, "Version of POSIX attempting to comply to");

SYSCTL_INT(_kern, KERN_NGROUPS, ngroups, CTLFLAG_RDTUN,
    &ngroups_max, 0,
    "Maximum number of supplemental groups a user can belong to");

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

SYSCTL_INT(_hw, HW_NCPU, ncpu, CTLFLAG_RD,
    &mp_ncpus, 0, "Number of active CPUs");

SYSCTL_INT(_hw, HW_BYTEORDER, byteorder, CTLFLAG_RD,
    0, BYTE_ORDER, "System byte order");

SYSCTL_INT(_hw, HW_PAGESIZE, pagesize, CTLFLAG_RD,
    0, PAGE_SIZE, "System memory page size");

static int
sysctl_kern_arnd(SYSCTL_HANDLER_ARGS)
{
	char buf[256];
	size_t len;

	len = req->oldlen;
	if (len > sizeof(buf))
		len = sizeof(buf);
	arc4rand(buf, len, 0);
	return (SYSCTL_OUT(req, buf, len));
}

SYSCTL_PROC(_kern, KERN_ARND, arandom,
    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_kern_arnd, "", "arc4rand");

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
sysctl_hw_realmem(SYSCTL_HANDLER_ARGS)
{
	u_long val;
	val = ctob(realmem);
	return (sysctl_handle_long(oidp, &val, 0, req));
}
SYSCTL_PROC(_hw, HW_REALMEM, realmem, CTLTYPE_ULONG | CTLFLAG_RD,
	0, 0, sysctl_hw_realmem, "LU", "");
static int
sysctl_hw_usermem(SYSCTL_HANDLER_ARGS)
{
	u_long val;

	val = ctob(physmem - cnt.v_wire_count);
	return (sysctl_handle_long(oidp, &val, 0, req));
}

SYSCTL_PROC(_hw, HW_USERMEM, usermem, CTLTYPE_ULONG | CTLFLAG_RD,
	0, 0, sysctl_hw_usermem, "LU", "");

SYSCTL_LONG(_hw, OID_AUTO, availpages, CTLFLAG_RD, &physmem, 0, "");

u_long pagesizes[MAXPAGESIZES] = { PAGE_SIZE };

static int
sysctl_hw_pagesizes(SYSCTL_HANDLER_ARGS)
{
	int error;
#ifdef SCTL_MASK32
	int i;
	uint32_t pagesizes32[MAXPAGESIZES];

	if (req->flags & SCTL_MASK32) {
		/*
		 * Recreate the "pagesizes" array with 32-bit elements.  Truncate
		 * any page size greater than UINT32_MAX to zero.
		 */
		for (i = 0; i < MAXPAGESIZES; i++)
			pagesizes32[i] = (uint32_t)pagesizes[i];

		error = SYSCTL_OUT(req, pagesizes32, sizeof(pagesizes32));
	} else
#endif
		error = SYSCTL_OUT(req, pagesizes, sizeof(pagesizes));
	return (error);
}
SYSCTL_PROC(_hw, OID_AUTO, pagesizes, CTLTYPE_ULONG | CTLFLAG_RD,
    NULL, 0, sysctl_hw_pagesizes, "LU", "Supported page sizes");

#ifdef SCTL_MASK32
int adaptive_machine_arch = 1;
SYSCTL_INT(_debug, OID_AUTO, adaptive_machine_arch, CTLFLAG_RW,
    &adaptive_machine_arch, 1,
    "Adapt reported machine architecture to the ABI of the binary");
#endif

static int
sysctl_hw_machine_arch(SYSCTL_HANDLER_ARGS)
{
	int error;
	static const char machine_arch[] = MACHINE_ARCH;
#ifdef SCTL_MASK32
	static const char machine_arch32[] = MACHINE_ARCH32;

	if ((req->flags & SCTL_MASK32) != 0 && adaptive_machine_arch)
		error = SYSCTL_OUT(req, machine_arch32, sizeof(machine_arch32));
	else
#endif
		error = SYSCTL_OUT(req, machine_arch, sizeof(machine_arch));
	return (error);

}
SYSCTL_PROC(_hw, HW_MACHINE_ARCH, machine_arch, CTLTYPE_STRING | CTLFLAG_RD,
    NULL, 0, sysctl_hw_machine_arch, "A", "System architecture");

static int
sysctl_hostname(SYSCTL_HANDLER_ARGS)
{
	struct prison *pr, *cpr;
	size_t pr_offset;
	char tmpname[MAXHOSTNAMELEN];
	int descend, error, len;

	/*
	 * This function can set: hostname domainname hostuuid.
	 * Keep that in mind when comments say "hostname".
	 */
	pr_offset = (size_t)arg1;
	len = arg2;
	KASSERT(len <= sizeof(tmpname),
	    ("length %d too long for %s", len, __func__));

	pr = req->td->td_ucred->cr_prison;
	if (!(pr->pr_allow & PR_ALLOW_SET_HOSTNAME) && req->newptr)
		return (EPERM);
	/*
	 * Make a local copy of hostname to get/set so we don't have to hold
	 * the jail mutex during the sysctl copyin/copyout activities.
	 */
	mtx_lock(&pr->pr_mtx);
	bcopy((char *)pr + pr_offset, tmpname, len);
	mtx_unlock(&pr->pr_mtx);

	error = sysctl_handle_string(oidp, tmpname, len, req);

	if (req->newptr != NULL && error == 0) {
		/*
		 * Copy the locally set hostname to all jails that share
		 * this host info.
		 */
		sx_slock(&allprison_lock);
		while (!(pr->pr_flags & PR_HOST))
			pr = pr->pr_parent;
		mtx_lock(&pr->pr_mtx);
		bcopy(tmpname, (char *)pr + pr_offset, len);
		FOREACH_PRISON_DESCENDANT_LOCKED(pr, cpr, descend)
			if (cpr->pr_flags & PR_HOST)
				descend = 0;
			else
				bcopy(tmpname, (char *)cpr + pr_offset, len);
		mtx_unlock(&pr->pr_mtx);
		sx_sunlock(&allprison_lock);
	}
	return (error);
}

SYSCTL_PROC(_kern, KERN_HOSTNAME, hostname,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE,
    (void *)(offsetof(struct prison, pr_hostname)), MAXHOSTNAMELEN,
    sysctl_hostname, "A", "Hostname");
SYSCTL_PROC(_kern, KERN_NISDOMAINNAME, domainname,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE,
    (void *)(offsetof(struct prison, pr_domainname)), MAXHOSTNAMELEN,
    sysctl_hostname, "A", "Name of the current YP/NIS domain");
SYSCTL_PROC(_kern, KERN_HOSTUUID, hostuuid,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE,
    (void *)(offsetof(struct prison, pr_hostuuid)), HOSTUUIDLEN,
    sysctl_hostname, "A", "Host UUID");

static int	regression_securelevel_nonmonotonic = 0;

#ifdef REGRESSION
SYSCTL_INT(_regression, OID_AUTO, securelevel_nonmonotonic, CTLFLAG_RW,
    &regression_securelevel_nonmonotonic, 0, "securelevel may be lowered");
#endif

static int
sysctl_kern_securelvl(SYSCTL_HANDLER_ARGS)
{
	struct prison *pr, *cpr;
	int descend, error, level;

	pr = req->td->td_ucred->cr_prison;

	/*
	 * Reading the securelevel is easy, since the current jail's level
	 * is known to be at least as secure as any higher levels.  Perform
	 * a lockless read since the securelevel is an integer.
	 */
	level = pr->pr_securelevel;
	error = sysctl_handle_int(oidp, &level, 0, req);
	if (error || !req->newptr)
		return (error);
	/* Permit update only if the new securelevel exceeds the old. */
	sx_slock(&allprison_lock);
	mtx_lock(&pr->pr_mtx);
	if (!regression_securelevel_nonmonotonic &&
	    level < pr->pr_securelevel) {
		mtx_unlock(&pr->pr_mtx);
		sx_sunlock(&allprison_lock);
		return (EPERM);
	}
	pr->pr_securelevel = level;
	/*
	 * Set all child jails to be at least this level, but do not lower
	 * them (even if regression_securelevel_nonmonotonic).
	 */
	FOREACH_PRISON_DESCENDANT_LOCKED(pr, cpr, descend) {
		if (cpr->pr_securelevel < level)
			cpr->pr_securelevel = level;
	}
	mtx_unlock(&pr->pr_mtx);
	sx_sunlock(&allprison_lock);
	return (error);
}

SYSCTL_PROC(_kern, KERN_SECURELVL, securelevel,
    CTLTYPE_INT|CTLFLAG_RW|CTLFLAG_PRISON, 0, 0, sysctl_kern_securelvl,
    "I", "Current secure level");

#ifdef INCLUDE_CONFIG_FILE
/* Actual kernel configuration options. */
extern char kernconfstring[];

static int
sysctl_kern_config(SYSCTL_HANDLER_ARGS)
{
	return (sysctl_handle_string(oidp, kernconfstring,
	    strlen(kernconfstring), req));
}

SYSCTL_PROC(_kern, OID_AUTO, conftxt, CTLTYPE_STRING|CTLFLAG_RW, 
    0, 0, sysctl_kern_config, "", "Kernel configuration file");
#endif

static int
sysctl_hostid(SYSCTL_HANDLER_ARGS)
{
	struct prison *pr, *cpr;
	u_long tmpid;
	int descend, error;

	/*
	 * Like sysctl_hostname, except it operates on a u_long
	 * instead of a string, and is used only for hostid.
	 */
	pr = req->td->td_ucred->cr_prison;
	if (!(pr->pr_allow & PR_ALLOW_SET_HOSTNAME) && req->newptr)
		return (EPERM);
	tmpid = pr->pr_hostid;
	error = sysctl_handle_long(oidp, &tmpid, 0, req);

	if (req->newptr != NULL && error == 0) {
		sx_slock(&allprison_lock);
		while (!(pr->pr_flags & PR_HOST))
			pr = pr->pr_parent;
		mtx_lock(&pr->pr_mtx);
		pr->pr_hostid = tmpid;
		FOREACH_PRISON_DESCENDANT_LOCKED(pr, cpr, descend)
			if (cpr->pr_flags & PR_HOST)
				descend = 0;
			else
				cpr->pr_hostid = tmpid;
		mtx_unlock(&pr->pr_mtx);
		sx_sunlock(&allprison_lock);
	}
	return (error);
}

SYSCTL_PROC(_kern, KERN_HOSTID, hostid,
    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE,
    NULL, 0, sysctl_hostid, "LU", "Host ID");

SYSCTL_NODE(_kern, OID_AUTO, features, CTLFLAG_RD, 0, "Kernel Features");

#ifdef COMPAT_FREEBSD4
FEATURE(compat_freebsd4, "Compatible with FreeBSD 4");
#endif

#ifdef COMPAT_FREEBSD5
FEATURE(compat_freebsd5, "Compatible with FreeBSD 5");
#endif

#ifdef COMPAT_FREEBSD6
FEATURE(compat_freebsd6, "Compatible with FreeBSD 6");
#endif

#ifdef COMPAT_FREEBSD7
FEATURE(compat_freebsd7, "Compatible with FreeBSD 7");
#endif

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

#include <sys/bio.h>
#include <sys/buf.h>
SYSCTL_INT(_debug_sizeof, OID_AUTO, bio, CTLFLAG_RD,
    0, sizeof(struct bio), "sizeof(struct bio)");
SYSCTL_INT(_debug_sizeof, OID_AUTO, buf, CTLFLAG_RD,
    0, sizeof(struct buf), "sizeof(struct buf)");

#include <sys/user.h>
SYSCTL_INT(_debug_sizeof, OID_AUTO, kinfo_proc, CTLFLAG_RD,
    0, sizeof(struct kinfo_proc), "sizeof(struct kinfo_proc)");

/* XXX compatibility, remove for 6.0 */
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
SYSCTL_INT(_kern, OID_AUTO, fallback_elf_brand, CTLFLAG_RW,
    &__elfN(fallback_brand), sizeof(__elfN(fallback_brand)),
    "compatibility for kern.fallback_elf_brand");
