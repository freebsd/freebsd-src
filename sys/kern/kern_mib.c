/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 *
 *	@(#)kern_sysctl.c	8.4 (Berkeley) 4/14/94
 */

#include <sys/cdefs.h>
#include "opt_posix.h"
#include "opt_config.h"

#include <sys/param.h>
#include <sys/boot.h>
#include <sys/elf.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysent.h>
#include <sys/vmmeter.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/unistd.h>

SYSCTL_ROOT_NODE(0, sysctl, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Sysctl internal magic");
SYSCTL_ROOT_NODE(CTL_KERN, kern, CTLFLAG_RW | CTLFLAG_CAPRD | CTLFLAG_MPSAFE, 0,
    "High kernel, proc, limits &c");
SYSCTL_ROOT_NODE(CTL_VM, vm, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Virtual memory");
SYSCTL_ROOT_NODE(CTL_VFS, vfs, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "File system");
SYSCTL_ROOT_NODE(CTL_NET, net, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Network, (see socket.h)");
SYSCTL_ROOT_NODE(CTL_DEBUG, debug, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Debugging");
SYSCTL_NODE(_debug, OID_AUTO,  sizeof,  CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Sizeof various things");
SYSCTL_ROOT_NODE(CTL_HW, hw, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "hardware");
SYSCTL_ROOT_NODE(CTL_MACHDEP, machdep, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "machine dependent");
SYSCTL_NODE(_machdep, OID_AUTO, mitigations, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Machine dependent platform mitigations.");
SYSCTL_ROOT_NODE(CTL_USER, user, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "user-level");
SYSCTL_ROOT_NODE(CTL_P1003_1B, p1003_1b, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "p1003_1b, (see p1003_1b.h)");

SYSCTL_ROOT_NODE(OID_AUTO, compat, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Compatibility code");
SYSCTL_ROOT_NODE(OID_AUTO, security, CTLFLAG_RW | CTLFLAG_MPSAFE, 0, 
    "Security");
#ifdef REGRESSION
SYSCTL_ROOT_NODE(OID_AUTO, regression, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Regression test MIB");
#endif

SYSCTL_STRING(_kern, OID_AUTO, ident, CTLFLAG_RD,
    kern_ident, 0, "Kernel identifier");

SYSCTL_INT(_kern, KERN_OSREV, osrevision, CTLFLAG_RD | CTLFLAG_CAPRD,
    SYSCTL_NULL_INT_PTR, BSD, "Operating system revision");

SYSCTL_STRING(_kern, KERN_VERSION, version, CTLFLAG_RD,
    version, 0, "Kernel version");

SYSCTL_STRING(_kern, OID_AUTO, compiler_version, CTLFLAG_RD,
    compiler_version, 0, "Version of compiler used to compile kernel");

SYSCTL_STRING(_kern, KERN_OSTYPE, ostype, CTLFLAG_RD | CTLFLAG_CAPRD,
    ostype, 0, "Operating system type");

SYSCTL_INT(_kern, KERN_MAXPROC, maxproc, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &maxproc, 0, "Maximum number of processes");

SYSCTL_INT(_kern, KERN_MAXPROCPERUID, maxprocperuid, CTLFLAG_RW,
    &maxprocperuid, 0, "Maximum processes allowed per userid");

SYSCTL_INT(_kern, OID_AUTO, maxusers, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &maxusers, 0, "Hint for kernel tuning");

SYSCTL_INT(_kern, KERN_ARGMAX, argmax, CTLFLAG_RD|CTLFLAG_CAPRD,
    SYSCTL_NULL_INT_PTR, ARG_MAX, "Maximum bytes of argument to execve(2)");

SYSCTL_INT(_kern, KERN_POSIX1, posix1version, CTLFLAG_RD|CTLFLAG_CAPRD,
    SYSCTL_NULL_INT_PTR, _POSIX_VERSION, "Version of POSIX attempting to comply to");

SYSCTL_INT(_kern, KERN_NGROUPS, ngroups, CTLFLAG_RDTUN |
    CTLFLAG_NOFETCH | CTLFLAG_CAPRD, &ngroups_max, 0,
    "Maximum number of supplemental groups a user can belong to");

SYSCTL_INT(_kern, KERN_JOB_CONTROL, job_control, CTLFLAG_RD|CTLFLAG_CAPRD,
    SYSCTL_NULL_INT_PTR, 1, "Whether job control is available");

#ifdef _POSIX_SAVED_IDS
SYSCTL_INT(_kern, KERN_SAVED_IDS, saved_ids, CTLFLAG_RD|CTLFLAG_CAPRD,
    SYSCTL_NULL_INT_PTR, 1, "Whether saved set-group/user ID is available");
#else
SYSCTL_INT(_kern, KERN_SAVED_IDS, saved_ids, CTLFLAG_RD|CTLFLAG_CAPRD,
    SYSCTL_NULL_INT_PTR, 0, "Whether saved set-group/user ID is available");
#endif

char kernelname[MAXPATHLEN] = PATH_KERNEL;	/* XXX bloat */

SYSCTL_STRING(_kern, KERN_BOOTFILE, bootfile, CTLFLAG_RW,
    kernelname, sizeof kernelname, "Name of kernel file booted");

#ifdef COMPAT_FREEBSD12
static int
sysctl_maxphys(SYSCTL_HANDLER_ARGS)
{
	u_long lvalue;
	int ivalue;

	lvalue = maxphys;
	if (sizeof(int) == sizeof(u_long) || req->oldlen >= sizeof(u_long))
		return (sysctl_handle_long(oidp, &lvalue, 0, req));
	if (lvalue > INT_MAX)
		return (sysctl_handle_long(oidp, &lvalue, 0, req));
	ivalue = lvalue;
	return (sysctl_handle_int(oidp, &ivalue, 0, req));
}
SYSCTL_PROC(_kern, KERN_MAXPHYS, maxphys, CTLTYPE_LONG | CTLFLAG_RDTUN |
    CTLFLAG_NOFETCH | CTLFLAG_CAPRD | CTLFLAG_MPSAFE,
    NULL, 0, sysctl_maxphys, "UL", "Maximum block I/O access size");
#else
SYSCTL_ULONG(_kern, KERN_MAXPHYS, maxphys,
    CTLFLAG_RDTUN | CTLFLAG_NOFETCH | CTLFLAG_CAPRD,
    &maxphys, 0, "Maximum block I/O access size");
#endif

SYSCTL_INT(_hw, HW_NCPU, ncpu, CTLFLAG_RD|CTLFLAG_CAPRD,
    &mp_ncpus, 0, "Number of active CPUs");

SYSCTL_INT(_hw, HW_BYTEORDER, byteorder, CTLFLAG_RD|CTLFLAG_CAPRD,
    SYSCTL_NULL_INT_PTR, BYTE_ORDER, "System byte order");

SYSCTL_INT(_hw, HW_PAGESIZE, pagesize, CTLFLAG_RD|CTLFLAG_CAPRD,
    SYSCTL_NULL_INT_PTR, PAGE_SIZE, "System memory page size");

static int
sysctl_kern_arnd(SYSCTL_HANDLER_ARGS)
{
	char buf[256];
	size_t len;

	len = MIN(req->oldlen, sizeof(buf));
	read_random(buf, len);
	return (SYSCTL_OUT(req, buf, len));
}

SYSCTL_PROC(_kern, KERN_ARND, arandom,
    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_MPSAFE | CTLFLAG_CAPRD, NULL, 0,
    sysctl_kern_arnd, "", "arc4rand");

static int
sysctl_hw_physmem(SYSCTL_HANDLER_ARGS)
{
	u_long val, p;

	p = SIZE_T_MAX >> PAGE_SHIFT;
	if (physmem < p)
		p = physmem;
	val = ctob(p);
	return (sysctl_handle_long(oidp, &val, 0, req));
}
SYSCTL_PROC(_hw, HW_PHYSMEM, physmem,
    CTLTYPE_ULONG | CTLFLAG_RD | CTLFLAG_MPSAFE, 0, 0,
    sysctl_hw_physmem, "LU",
    "Amount of physical memory (in bytes)");

static int
sysctl_hw_realmem(SYSCTL_HANDLER_ARGS)
{
	u_long val, p;

	p = SIZE_T_MAX >> PAGE_SHIFT;
	if (realmem < p)
		p = realmem;
	val = ctob(p);
	return (sysctl_handle_long(oidp, &val, 0, req));
}
SYSCTL_PROC(_hw, HW_REALMEM, realmem,
    CTLTYPE_ULONG | CTLFLAG_RD | CTLFLAG_MPSAFE, 0, 0,
    sysctl_hw_realmem, "LU",
    "Amount of memory (in bytes) reported by the firmware");

static int
sysctl_hw_usermem(SYSCTL_HANDLER_ARGS)
{
	u_long val, p, p1;

	p1 = physmem - vm_wire_count();
	p = SIZE_T_MAX >> PAGE_SHIFT;
	if (p1 < p)
		p = p1;
	val = ctob(p);
	return (sysctl_handle_long(oidp, &val, 0, req));
}
SYSCTL_PROC(_hw, HW_USERMEM, usermem,
    CTLTYPE_ULONG | CTLFLAG_RD | CTLFLAG_MPSAFE, 0, 0,
    sysctl_hw_usermem, "LU",
    "Amount of memory (in bytes) which is not wired");

SYSCTL_LONG(_hw, OID_AUTO, availpages, CTLFLAG_RD, &physmem, 0,
    "Amount of physical memory (in pages)");

u_long pagesizes[MAXPAGESIZES] = { PAGE_SIZE };

static int
sysctl_hw_pagesizes(SYSCTL_HANDLER_ARGS)
{
	int error;
	size_t len;
#ifdef SCTL_MASK32
	int i;
	uint32_t pagesizes32[MAXPAGESIZES];

	if (req->flags & SCTL_MASK32) {
		/*
		 * Recreate the "pagesizes" array with 32-bit elements.
		 * Truncate any page size greater than UINT32_MAX to zero,
		 * which assumes that page sizes are powers of two.
		 */
		for (i = 0; i < MAXPAGESIZES; i++)
			pagesizes32[i] = (uint32_t)pagesizes[i];

		len = sizeof(pagesizes32);
		if (len > req->oldlen && req->oldptr != NULL)
			len = req->oldlen;
		error = SYSCTL_OUT(req, pagesizes32, len);
	} else
#endif
	{
		len = sizeof(pagesizes);
		if (len > req->oldlen && req->oldptr != NULL)
			len = req->oldlen;
		error = SYSCTL_OUT(req, pagesizes, len);
	}
	return (error);
}
SYSCTL_PROC(_hw, OID_AUTO, pagesizes,
    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_hw_pagesizes, "S,pagesizes",
    "Supported page sizes");

int adaptive_machine_arch = 1;
SYSCTL_INT(_debug, OID_AUTO, adaptive_machine_arch, CTLFLAG_RW,
    &adaptive_machine_arch, 1,
    "Adapt reported machine architecture to the ABI of the binary");

static const char *
proc_machine_arch(struct proc *p)
{

	if (p->p_sysent->sv_machine_arch != NULL)
		return (p->p_sysent->sv_machine_arch(p));
#ifdef COMPAT_FREEBSD32
	if (SV_PROC_FLAG(p, SV_ILP32))
		return (MACHINE_ARCH32);
#endif
	return (MACHINE_ARCH);
}

static int
sysctl_hw_machine_arch(SYSCTL_HANDLER_ARGS)
{
	const char *machine_arch;

	if (adaptive_machine_arch)
		machine_arch = proc_machine_arch(curproc);
	else
		machine_arch = MACHINE_ARCH;
	return (SYSCTL_OUT(req, machine_arch, strlen(machine_arch) + 1));
}
SYSCTL_PROC(_hw, HW_MACHINE_ARCH, machine_arch, CTLTYPE_STRING | CTLFLAG_RD |
    CTLFLAG_CAPRD | CTLFLAG_MPSAFE, NULL, 0, sysctl_hw_machine_arch, "A",
    "System architecture");

#ifdef COMPAT_FREEBSD32
#include <compat/freebsd32/freebsd32_util.h>
#endif

static int
sysctl_kern_supported_archs(SYSCTL_HANDLER_ARGS)
{
	const char *supported_archs;

	supported_archs =
#ifdef COMPAT_FREEBSD32
	    compat_freebsd_32bit ? MACHINE_ARCH " " MACHINE_ARCH32 :
#endif
	    MACHINE_ARCH;
	return (SYSCTL_OUT(req, supported_archs, strlen(supported_archs) + 1));
}
SYSCTL_PROC(_kern, OID_AUTO, supported_archs, CTLFLAG_RD | CTLFLAG_MPSAFE |
    CTLFLAG_CAPRD | CTLTYPE_STRING, NULL, 0, sysctl_kern_supported_archs, "A",
    "Supported architectures for binaries");

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

	/*
	 * Make a local copy of hostname to get/set so we don't have to hold
	 * the jail mutex during the sysctl copyin/copyout activities.
	 */
	pr = req->td->td_ucred->cr_prison;
	mtx_lock(&pr->pr_mtx);
	bcopy((char *)pr + pr_offset, tmpname, len);
	mtx_unlock(&pr->pr_mtx);

	error = sysctl_handle_string(oidp, tmpname, len, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	/*
	 * Copy the locally set hostname to all jails that share
	 * this host info.
	 */
	sx_slock(&allprison_lock);
	if (!(pr->pr_allow & PR_ALLOW_SET_HOSTNAME))
		error = EPERM;
	else {
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
	}
	sx_sunlock(&allprison_lock);
	return (error);
}

SYSCTL_PROC(_kern, KERN_HOSTNAME, hostname,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_CAPRD | CTLFLAG_MPSAFE,
    (void *)(offsetof(struct prison, pr_hostname)), MAXHOSTNAMELEN,
    sysctl_hostname, "A", "Hostname");
SYSCTL_PROC(_kern, KERN_NISDOMAINNAME, domainname,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_CAPRD | CTLFLAG_MPSAFE,
    (void *)(offsetof(struct prison, pr_domainname)), MAXHOSTNAMELEN,
    sysctl_hostname, "A", "Name of the current YP/NIS domain");
SYSCTL_PROC(_kern, KERN_HOSTUUID, hostuuid,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_CAPRD | CTLFLAG_MPSAFE,
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
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE, 0, 0,
    sysctl_kern_securelvl, "I",
    "Current secure level");

#ifdef INCLUDE_CONFIG_FILE
/* Actual kernel configuration options. */
extern char kernconfstring[];

SYSCTL_STRING(_kern, OID_AUTO, conftxt, CTLFLAG_RD,
    kernconfstring, 0, "Kernel configuration file");
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
	mtx_lock(&pr->pr_mtx);
	tmpid = pr->pr_hostid;
	mtx_unlock(&pr->pr_mtx);

	error = sysctl_handle_long(oidp, &tmpid, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	sx_slock(&allprison_lock);
	if (!(pr->pr_allow & PR_ALLOW_SET_HOSTNAME))
		error = EPERM;
	else {
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
	}
	sx_sunlock(&allprison_lock);
	return (error);
}

SYSCTL_PROC(_kern, KERN_HOSTID, hostid,
    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE | CTLFLAG_CAPRD,
    NULL, 0, sysctl_hostid, "LU", "Host ID");

static struct mtx bootid_lk;
MTX_SYSINIT(bootid_lock, &bootid_lk, "bootid generator lock", MTX_DEF);

static int
sysctl_bootid(SYSCTL_HANDLER_ARGS)
{
	static uint8_t boot_id[16];
	static bool initialized = false;

	mtx_lock(&bootid_lk);
	if (!initialized) {
		if (!is_random_seeded()) {
			mtx_unlock(&bootid_lk);
			return (ENXIO);
		}
		arc4random_buf(boot_id, sizeof(boot_id));
		initialized = true;
	}
	mtx_unlock(&bootid_lk);

	return (SYSCTL_OUT(req, boot_id, sizeof(boot_id)));
}
SYSCTL_PROC(_kern, OID_AUTO, boot_id,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_MPSAFE | CTLFLAG_CAPRD,
    NULL, 0, sysctl_bootid, "", "Random boot ID");

/*
 * The osrelease string is copied from the global (osrelease in vers.c) into
 * prison0 by a sysinit and is inherited by child jails if not changed at jail
 * creation, so we always return the copy from the current prison data.
 */
static int
sysctl_osrelease(SYSCTL_HANDLER_ARGS)
{
	struct prison *pr;

	pr = req->td->td_ucred->cr_prison;
	return (SYSCTL_OUT(req, pr->pr_osrelease, strlen(pr->pr_osrelease) + 1));

}

SYSCTL_PROC(_kern, KERN_OSRELEASE, osrelease,
    CTLTYPE_STRING | CTLFLAG_CAPRD | CTLFLAG_RD | CTLFLAG_MPSAFE,
    NULL, 0, sysctl_osrelease, "A", "Operating system release");

/*
 * The osreldate number is copied from the global (osreldate in vers.c) into
 * prison0 by a sysinit and is inherited by child jails if not changed at jail
 * creation, so we always return the value from the current prison data.
 */
static int
sysctl_osreldate(SYSCTL_HANDLER_ARGS)
{
	struct prison *pr;

	pr = req->td->td_ucred->cr_prison;
	return (SYSCTL_OUT(req, &pr->pr_osreldate, sizeof(pr->pr_osreldate)));

}

/*
 * NOTICE: The *userland* release date is available in
 * /usr/include/osreldate.h
 */
SYSCTL_PROC(_kern, KERN_OSRELDATE, osreldate,
    CTLTYPE_INT | CTLFLAG_CAPRD | CTLFLAG_RD | CTLFLAG_MPSAFE,
    NULL, 0, sysctl_osreldate, "I", "Kernel release date");

/*
 * The build-id is copied from the ELF section .note.gnu.build-id.  The linker
 * script defines two variables to expose the beginning and end.  LLVM
 * currently uses a SHA-1 hash, but other formats can be supported by checking
 * the length of the section.
 */

extern char __build_id_start[];
extern char __build_id_end[];

#define	BUILD_ID_HEADER_LEN	0x10
#define	BUILD_ID_HASH_MAXLEN	0x14

static int
sysctl_build_id(SYSCTL_HANDLER_ARGS)
{
	uintptr_t sectionlen = (uintptr_t)(__build_id_end - __build_id_start);
	int hashlen;
	char buf[2*BUILD_ID_HASH_MAXLEN+1];

	/*
	 * The ELF note section has a four byte length for the vendor name,
	 * four byte length for the value, and a four byte vendor specific
	 * type.  The name for the build id is "GNU\0".  We skip the first 16
	 * bytes to read the build hash.  We will return the remaining bytes up
	 * to 20 (SHA-1) hash size.  If the hash happens to be a custom number
	 * of bytes we will pad the value with zeros, as the section should be
	 * four byte aligned.
	 */
	if (sectionlen <= BUILD_ID_HEADER_LEN ||
	    sectionlen > (BUILD_ID_HEADER_LEN + BUILD_ID_HASH_MAXLEN)) {
		return (ENOENT);
	}

	hashlen = sectionlen - BUILD_ID_HEADER_LEN;
	for (int i = 0; i < hashlen; i++) {
		uint8_t c = __build_id_start[i+BUILD_ID_HEADER_LEN];
		snprintf(&buf[2*i], 3, "%02x", c);
	}

	return (SYSCTL_OUT(req, buf, strlen(buf) + 1));
}

SYSCTL_PROC(_kern, OID_AUTO, build_id,
    CTLTYPE_STRING | CTLFLAG_CAPRD | CTLFLAG_RD | CTLFLAG_MPSAFE,
    NULL, 0, sysctl_build_id, "A", "Operating system build-id");

SYSCTL_NODE(_kern, OID_AUTO, features, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Kernel Features");

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

#ifdef COMPAT_FREEBSD8
FEATURE(compat_freebsd8, "Compatible with FreeBSD 8");
#endif

#ifdef COMPAT_FREEBSD9
FEATURE(compat_freebsd9, "Compatible with FreeBSD 9");
#endif

#ifdef COMPAT_FREEBSD10
FEATURE(compat_freebsd10, "Compatible with FreeBSD 10");
#endif

#ifdef COMPAT_FREEBSD11
FEATURE(compat_freebsd11, "Compatible with FreeBSD 11");
#endif

#ifdef COMPAT_FREEBSD12
FEATURE(compat_freebsd12, "Compatible with FreeBSD 12");
#endif

#ifdef COMPAT_FREEBSD13
FEATURE(compat_freebsd13, "Compatible with FreeBSD 13");
#endif

/*
 * This is really cheating.  These actually live in the libc, something
 * which I'm not quite sure is a good idea anyway, but in order for
 * getnext and friends to actually work, we define dummies here.
 *
 * XXXRW: These probably should be CTLFLAG_CAPRD.
 */
SYSCTL_STRING(_user, USER_CS_PATH, cs_path, CTLFLAG_RD,
    "", 0, "PATH that finds all the standard utilities");
SYSCTL_INT(_user, USER_BC_BASE_MAX, bc_base_max, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Max ibase/obase values in bc(1)");
SYSCTL_INT(_user, USER_BC_DIM_MAX, bc_dim_max, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Max array size in bc(1)");
SYSCTL_INT(_user, USER_BC_SCALE_MAX, bc_scale_max, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Max scale value in bc(1)");
SYSCTL_INT(_user, USER_BC_STRING_MAX, bc_string_max, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Max string length in bc(1)");
SYSCTL_INT(_user, USER_COLL_WEIGHTS_MAX, coll_weights_max, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Maximum number of weights assigned to an LC_COLLATE locale entry");
SYSCTL_INT(_user, USER_EXPR_NEST_MAX, expr_nest_max, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "");
SYSCTL_INT(_user, USER_LINE_MAX, line_max, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Max length (bytes) of a text-processing utility's input line");
SYSCTL_INT(_user, USER_RE_DUP_MAX, re_dup_max, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Maximum number of repeats of a regexp permitted");
SYSCTL_INT(_user, USER_POSIX2_VERSION, posix2_version, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0,
    "The version of POSIX 1003.2 with which the system attempts to comply");
SYSCTL_INT(_user, USER_POSIX2_C_BIND, posix2_c_bind, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Whether C development supports the C bindings option");
SYSCTL_INT(_user, USER_POSIX2_C_DEV, posix2_c_dev, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Whether system supports the C development utilities option");
SYSCTL_INT(_user, USER_POSIX2_CHAR_TERM, posix2_char_term, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "");
SYSCTL_INT(_user, USER_POSIX2_FORT_DEV, posix2_fort_dev, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Whether system supports FORTRAN development utilities");
SYSCTL_INT(_user, USER_POSIX2_FORT_RUN, posix2_fort_run, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Whether system supports FORTRAN runtime utilities");
SYSCTL_INT(_user, USER_POSIX2_LOCALEDEF, posix2_localedef, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Whether system supports creation of locales");
SYSCTL_INT(_user, USER_POSIX2_SW_DEV, posix2_sw_dev, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Whether system supports software development utilities");
SYSCTL_INT(_user, USER_POSIX2_UPE, posix2_upe, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Whether system supports the user portability utilities");
SYSCTL_INT(_user, USER_STREAM_MAX, stream_max, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Min Maximum number of streams a process may have open at one time");
SYSCTL_INT(_user, USER_TZNAME_MAX, tzname_max, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, 0, "Min Maximum number of types supported for timezone names");

static char localbase[MAXPATHLEN] = "";

SYSCTL_STRING(_user, USER_LOCALBASE, localbase, CTLFLAG_RWTUN,
    localbase, sizeof(localbase), "Prefix used to install and locate add-on packages");

#include <sys/vnode.h>
SYSCTL_INT(_debug_sizeof, OID_AUTO, vnode, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, sizeof(struct vnode), "sizeof(struct vnode)");

SYSCTL_INT(_debug_sizeof, OID_AUTO, proc, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, sizeof(struct proc), "sizeof(struct proc)");

static int
sysctl_kern_pid_max(SYSCTL_HANDLER_ARGS)
{
	int error, pm;

	pm = pid_max;
	error = sysctl_handle_int(oidp, &pm, 0, req);
	if (error || !req->newptr)
		return (error);
	sx_xlock(&proctree_lock);
	sx_xlock(&allproc_lock);

	/*
	 * Only permit the values less then PID_MAX.
	 * As a safety measure, do not allow to limit the pid_max too much.
	 */
	if (pm < 300 || pm > PID_MAX)
		error = EINVAL;
	else
		pid_max = pm;
	sx_xunlock(&allproc_lock);
	sx_xunlock(&proctree_lock);
	return (error);
}
SYSCTL_PROC(_kern, OID_AUTO, pid_max, CTLTYPE_INT |
    CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE,
    0, 0, sysctl_kern_pid_max, "I", "Maximum allowed pid");

#include <sys/bio.h>
#include <sys/buf.h>
SYSCTL_INT(_debug_sizeof, OID_AUTO, bio, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, sizeof(struct bio), "sizeof(struct bio)");
SYSCTL_INT(_debug_sizeof, OID_AUTO, buf, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, sizeof(struct buf), "sizeof(struct buf)");

#include <sys/user.h>
SYSCTL_INT(_debug_sizeof, OID_AUTO, kinfo_proc, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, sizeof(struct kinfo_proc), "sizeof(struct kinfo_proc)");

/* Used by kernel debuggers. */
const int pcb_size = sizeof(struct pcb);
SYSCTL_INT(_debug_sizeof, OID_AUTO, pcb, CTLFLAG_RD,
    SYSCTL_NULL_INT_PTR, sizeof(struct pcb), "sizeof(struct pcb)");

/* XXX compatibility, remove for 6.0 */
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
SYSCTL_INT(_kern, OID_AUTO, fallback_elf_brand, CTLFLAG_RW,
    &__elfN(fallback_brand), sizeof(__elfN(fallback_brand)),
    "compatibility for kern.fallback_elf_brand");
