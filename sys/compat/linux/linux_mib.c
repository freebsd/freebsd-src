/*-
 * Copyright (c) 1999 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/jail.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>

#include "opt_compat.h"

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#else
#include <machine/../linux/linux.h>
#endif
#include <compat/linux/linux_mib.h>

struct linux_prison {
	char	pr_osname[LINUX_MAX_UTSNAME];
	char	pr_osrelease[LINUX_MAX_UTSNAME];
	int	pr_oss_version;
	int	pr_use_linux26;	/* flag to determine whether to use 2.6 emulation */
};

static unsigned linux_osd_jail_slot;

SYSCTL_NODE(_compat, OID_AUTO, linux, CTLFLAG_RW, 0,
	    "Linux mode");

static struct mtx osname_lock;
MTX_SYSINIT(linux_osname, &osname_lock, "linux osname", MTX_DEF);

static char	linux_osname[LINUX_MAX_UTSNAME] = "Linux";

static int
linux_sysctl_osname(SYSCTL_HANDLER_ARGS)
{
	char osname[LINUX_MAX_UTSNAME];
	int error;

	linux_get_osname(req->td, osname);
	error = sysctl_handle_string(oidp, osname, LINUX_MAX_UTSNAME, req);
	if (error || req->newptr == NULL)
		return (error);
	error = linux_set_osname(req->td, osname);
	return (error);
}

SYSCTL_PROC(_compat_linux, OID_AUTO, osname,
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE,
	    0, 0, linux_sysctl_osname, "A",
	    "Linux kernel OS name");

static char	linux_osrelease[LINUX_MAX_UTSNAME] = "2.6.16";
static int	linux_use_linux26 = 1;

static int
linux_sysctl_osrelease(SYSCTL_HANDLER_ARGS)
{
	char osrelease[LINUX_MAX_UTSNAME];
	int error;

	linux_get_osrelease(req->td, osrelease);
	error = sysctl_handle_string(oidp, osrelease, LINUX_MAX_UTSNAME, req);
	if (error || req->newptr == NULL)
		return (error);
	error = linux_set_osrelease(req->td, osrelease);
	return (error);
}

SYSCTL_PROC(_compat_linux, OID_AUTO, osrelease,
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE,
	    0, 0, linux_sysctl_osrelease, "A",
	    "Linux kernel OS release");

static int	linux_oss_version = 0x030600;

static int
linux_sysctl_oss_version(SYSCTL_HANDLER_ARGS)
{
	int oss_version;
	int error;

	oss_version = linux_get_oss_version(req->td);
	error = sysctl_handle_int(oidp, &oss_version, 0, req);
	if (error || req->newptr == NULL)
		return (error);
	error = linux_set_oss_version(req->td, oss_version);
	return (error);
}

SYSCTL_PROC(_compat_linux, OID_AUTO, oss_version,
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE,
	    0, 0, linux_sysctl_oss_version, "I",
	    "Linux OSS version");

/*
 * Returns holding the prison mutex if return non-NULL.
 */
static struct linux_prison *
linux_get_prison(struct thread *td, struct prison **prp)
{
	struct prison *pr;
	struct linux_prison *lpr;

	KASSERT(td == curthread, ("linux_get_prison() called on !curthread"));
	*prp = pr = td->td_ucred->cr_prison;
	if (pr == NULL || !linux_osd_jail_slot)
		return (NULL);
	mtx_lock(&pr->pr_mtx);
	lpr = osd_jail_get(pr, linux_osd_jail_slot);
	if (lpr == NULL)
		mtx_unlock(&pr->pr_mtx);
	return (lpr);
}

/*
 * Ensure a prison has its own Linux info.  The prison should be locked on
 * entrance and will be locked on exit (though it may get unlocked in the
 * interrim).
 */
static int
linux_alloc_prison(struct prison *pr, struct linux_prison **lprp)
{
	struct linux_prison *lpr, *nlpr;
	int error;

	/* If this prison already has Linux info, return that. */
	error = 0;
	mtx_assert(&pr->pr_mtx, MA_OWNED);
	lpr = osd_jail_get(pr, linux_osd_jail_slot);
	if (lpr != NULL)
		goto done;
	/*
	 * Allocate a new info record.  Then check again, in case something
	 * changed during the allocation.
	 */
	mtx_unlock(&pr->pr_mtx);
	nlpr = malloc(sizeof(struct linux_prison), M_PRISON, M_WAITOK);
	mtx_lock(&pr->pr_mtx);
	lpr = osd_jail_get(pr, linux_osd_jail_slot);
	if (lpr != NULL) {
		free(nlpr, M_PRISON);
		goto done;
	}
	error = osd_jail_set(pr, linux_osd_jail_slot, nlpr);
	if (error)
		free(nlpr, M_PRISON);
	else {
		lpr = nlpr;
		mtx_lock(&osname_lock);
		strncpy(lpr->pr_osname, linux_osname, LINUX_MAX_UTSNAME);
		strncpy(lpr->pr_osrelease, linux_osrelease, LINUX_MAX_UTSNAME);
		lpr->pr_oss_version = linux_oss_version;
		lpr->pr_use_linux26 = linux_use_linux26;
		mtx_unlock(&osname_lock);
	}
done:
	if (lprp != NULL)
		*lprp = lpr;
	return (error);
}

/*
 * Jail OSD methods for Linux prison data.
 */
static int
linux_prison_create(void *obj, void *data)
{
	int error;
	struct prison *pr = obj;
	struct vfsoptlist *opts = data;

	if (vfs_flagopt(opts, "nolinux", NULL, 0))
		return (0);
	/*
	 * Inherit a prison's initial values from its parent
	 * (different from NULL which also inherits changes).
	 */
	mtx_lock(&pr->pr_mtx);
	error = linux_alloc_prison(pr, NULL);
	mtx_unlock(&pr->pr_mtx);
	return (error);
}

static int
linux_prison_check(void *obj __unused, void *data)
{
	struct vfsoptlist *opts = data;
	char *osname, *osrelease;
	size_t len;
	int error, oss_version;

	/* Check that the parameters are correct. */
	(void)vfs_flagopt(opts, "linux", NULL, 0);
	(void)vfs_flagopt(opts, "nolinux", NULL, 0);
	error = vfs_getopt(opts, "linux.osname", (void **)&osname, &len);
	if (error != ENOENT) {
		if (error != 0)
			return (error);
		if (len == 0 || osname[len - 1] != '\0')
			return (EINVAL);
		if (len > LINUX_MAX_UTSNAME) {
			vfs_opterror(opts, "linux.osname too long");
			return (ENAMETOOLONG);
		}
	}
	error = vfs_getopt(opts, "linux.osrelease", (void **)&osrelease, &len);
	if (error != ENOENT) {
		if (error != 0)
			return (error);
		if (len == 0 || osrelease[len - 1] != '\0')
			return (EINVAL);
		if (len > LINUX_MAX_UTSNAME) {
			vfs_opterror(opts, "linux.osrelease too long");
			return (ENAMETOOLONG);
		}
	}
	error = vfs_copyopt(opts, "linux.oss_version", &oss_version,
	    sizeof(oss_version));
	return (error == ENOENT ? 0 : error);
}

static int
linux_prison_set(void *obj, void *data)
{
	struct linux_prison *lpr;
	struct prison *pr = obj;
	struct vfsoptlist *opts = data;
	char *osname, *osrelease;
	size_t len;
	int error, gotversion, nolinux, oss_version, yeslinux;

	/* Set the parameters, which should be correct. */
	yeslinux = vfs_flagopt(opts, "linux", NULL, 0);
	nolinux = vfs_flagopt(opts, "nolinux", NULL, 0);
	error = vfs_getopt(opts, "linux.osname", (void **)&osname, &len);
	if (error == ENOENT)
		osname = NULL;
	else
		yeslinux = 1;
	error = vfs_getopt(opts, "linux.osrelease", (void **)&osrelease, &len);
	if (error == ENOENT)
		osrelease = NULL;
	else
		yeslinux = 1;
	error = vfs_copyopt(opts, "linux.oss_version", &oss_version,
	    sizeof(oss_version));
	gotversion = error == 0;
	yeslinux |= gotversion;
	if (nolinux) {
		/* "nolinux": inherit the parent's Linux info. */
		mtx_lock(&pr->pr_mtx);
		osd_jail_del(pr, linux_osd_jail_slot);
		mtx_unlock(&pr->pr_mtx);
	} else if (yeslinux) {
		/*
		 * "linux" or "linux.*":
		 * the prison gets its own Linux info.
		 */
		mtx_lock(&pr->pr_mtx);
		error = linux_alloc_prison(pr, &lpr);
		if (error) {
			mtx_unlock(&pr->pr_mtx);
			return (error);
		}
		if (osname)
			strlcpy(lpr->pr_osname, osname, LINUX_MAX_UTSNAME);
		if (osrelease) {
			strlcpy(lpr->pr_osrelease, osrelease,
			    LINUX_MAX_UTSNAME);
			lpr->pr_use_linux26 = strlen(osrelease) >= 3 &&
			    osrelease[2] == '6';
		}
		if (gotversion)
			lpr->pr_oss_version = oss_version;
		mtx_unlock(&pr->pr_mtx);
	}
	return (0);
}

SYSCTL_JAIL_PARAM_NODE(linux, "Jail Linux parameters");
SYSCTL_JAIL_PARAM(, nolinux, CTLTYPE_INT | CTLFLAG_RW,
    "BN", "Jail w/ no Linux parameters");
SYSCTL_JAIL_PARAM_STRING(_linux, osname, CTLFLAG_RW, LINUX_MAX_UTSNAME,
    "Jail Linux kernel OS name");
SYSCTL_JAIL_PARAM_STRING(_linux, osrelease, CTLFLAG_RW, LINUX_MAX_UTSNAME,
    "Jail Linux kernel OS release");
SYSCTL_JAIL_PARAM(_linux, oss_version, CTLTYPE_INT | CTLFLAG_RW,
    "I", "Jail Linux OSS version");

static int
linux_prison_get(void *obj, void *data)
{
	struct linux_prison *lpr;
	struct prison *pr = obj;
	struct vfsoptlist *opts = data;
	int error, i;

	mtx_lock(&pr->pr_mtx);
	/* Tell whether this prison has its own Linux info. */
	lpr = osd_jail_get(pr, linux_osd_jail_slot);
	i = lpr != NULL;
	error = vfs_setopt(opts, "linux", &i, sizeof(i));
	if (error != 0 && error != ENOENT)
		goto done;
	i = !i;
	error = vfs_setopt(opts, "nolinux", &i, sizeof(i));
	if (error != 0 && error != ENOENT)
		goto done;
	/*
	 * It's kind of bogus to give the root info, but leave it to the caller
	 * to check the above flag.
	 */
	if (lpr != NULL) {
		error = vfs_setopts(opts, "linux.osname", lpr->pr_osname);
		if (error != 0 && error != ENOENT)
			goto done;
		error = vfs_setopts(opts, "linux.osrelease", lpr->pr_osrelease);
		if (error != 0 && error != ENOENT)
			goto done;
		error = vfs_setopt(opts, "linux.oss_version",
		    &lpr->pr_oss_version, sizeof(lpr->pr_oss_version));
		if (error != 0 && error != ENOENT)
			goto done;
	} else {
		mtx_lock(&osname_lock);
		error = vfs_setopts(opts, "linux.osname", linux_osname);
		if (error != 0 && error != ENOENT)
			goto done;
		error = vfs_setopts(opts, "linux.osrelease", linux_osrelease);
		if (error != 0 && error != ENOENT)
			goto done;
		error = vfs_setopt(opts, "linux.oss_version",
		    &linux_oss_version, sizeof(linux_oss_version));
		if (error != 0 && error != ENOENT)
			goto done;
		mtx_unlock(&osname_lock);
	}
	error = 0;

 done:
	mtx_unlock(&pr->pr_mtx);
	return (error);
}

static void
linux_prison_destructor(void *data)
{

	free(data, M_PRISON);
}

void
linux_osd_jail_register(void)
{
	struct prison *pr;
	osd_method_t methods[PR_MAXMETHOD] = {
	    [PR_METHOD_CREATE] =	linux_prison_create,
	    [PR_METHOD_GET] =		linux_prison_get,
	    [PR_METHOD_SET] =		linux_prison_set,
	    [PR_METHOD_CHECK] =		linux_prison_check
	};

	linux_osd_jail_slot =
	    osd_jail_register(linux_prison_destructor, methods);
	if (linux_osd_jail_slot > 0) {
		/* Copy the system linux info to any current prisons. */
		sx_xlock(&allprison_lock);
		TAILQ_FOREACH(pr, &allprison, pr_list) {
			mtx_lock(&pr->pr_mtx);
			(void)linux_alloc_prison(pr, NULL);
			mtx_unlock(&pr->pr_mtx);
		}
		sx_xunlock(&allprison_lock);
	}
}

void
linux_osd_jail_deregister(void)
{

	if (linux_osd_jail_slot)
		osd_jail_deregister(linux_osd_jail_slot);
}

void
linux_get_osname(struct thread *td, char *dst)
{
	struct prison *pr;
	struct linux_prison *lpr;

	lpr = linux_get_prison(td, &pr);
	if (lpr != NULL) {
		bcopy(lpr->pr_osname, dst, LINUX_MAX_UTSNAME);
		mtx_unlock(&pr->pr_mtx);
	} else {
		mtx_lock(&osname_lock);
		bcopy(linux_osname, dst, LINUX_MAX_UTSNAME);
		mtx_unlock(&osname_lock);
	}
}

int
linux_set_osname(struct thread *td, char *osname)
{
	struct prison *pr;
	struct linux_prison *lpr;

	lpr = linux_get_prison(td, &pr);
	if (lpr != NULL) {
		strlcpy(lpr->pr_osname, osname, LINUX_MAX_UTSNAME);
		mtx_unlock(&pr->pr_mtx);
	} else {
		mtx_lock(&osname_lock);
		strcpy(linux_osname, osname);
		mtx_unlock(&osname_lock);
	}

	return (0);
}

void
linux_get_osrelease(struct thread *td, char *dst)
{
	struct prison *pr;
	struct linux_prison *lpr;

	lpr = linux_get_prison(td, &pr);
	if (lpr != NULL) {
		bcopy(lpr->pr_osrelease, dst, LINUX_MAX_UTSNAME);
		mtx_unlock(&pr->pr_mtx);
	} else {
		mtx_lock(&osname_lock);
		bcopy(linux_osrelease, dst, LINUX_MAX_UTSNAME);
		mtx_unlock(&osname_lock);
	}
}

int
linux_use26(struct thread *td)
{
	struct prison *pr;
	struct linux_prison *lpr;
	int use26;

	lpr = linux_get_prison(td, &pr);
	if (lpr != NULL) {
		use26 = lpr->pr_use_linux26;
		mtx_unlock(&pr->pr_mtx);
	} else
		use26 = linux_use_linux26;
	return (use26);
}

int
linux_set_osrelease(struct thread *td, char *osrelease)
{
	struct prison *pr;
	struct linux_prison *lpr;

	lpr = linux_get_prison(td, &pr);
	if (lpr != NULL) {
		strlcpy(lpr->pr_osrelease, osrelease, LINUX_MAX_UTSNAME);
		lpr->pr_use_linux26 =
		    strlen(osrelease) >= 3 && osrelease[2] == '6';
		mtx_unlock(&pr->pr_mtx);
	} else {
		mtx_lock(&osname_lock);
		strcpy(linux_osrelease, osrelease);
		linux_use_linux26 =
		    strlen(osrelease) >= 3 && osrelease[2] == '6';
		mtx_unlock(&osname_lock);
	}

	return (0);
}

int
linux_get_oss_version(struct thread *td)
{
	struct prison *pr;
	struct linux_prison *lpr;
	int version;

	lpr = linux_get_prison(td, &pr);
	if (lpr != NULL) {
		version = lpr->pr_oss_version;
		mtx_unlock(&pr->pr_mtx);
	} else
		version = linux_oss_version;
	return (version);
}

int
linux_set_oss_version(struct thread *td, int oss_version)
{
	struct prison *pr;
	struct linux_prison *lpr;

	lpr = linux_get_prison(td, &pr);
	if (lpr != NULL) {
		lpr->pr_oss_version = oss_version;
		mtx_unlock(&pr->pr_mtx);
	} else {
		mtx_lock(&osname_lock);
		linux_oss_version = oss_version;
		mtx_unlock(&osname_lock);
	}

	return (0);
}

#if defined(DEBUG) || defined(KTR)

u_char linux_debug_map[howmany(LINUX_SYS_MAXSYSCALL, sizeof(u_char))];

static int
linux_debug(int syscall, int toggle, int global)
{

	if (global) {
		char c = toggle ? 0 : 0xff;

		memset(linux_debug_map, c, sizeof(linux_debug_map));
		return (0);
	}
	if (syscall < 0 || syscall >= LINUX_SYS_MAXSYSCALL)
		return (EINVAL);
	if (toggle)
		clrbit(linux_debug_map, syscall);
	else
		setbit(linux_debug_map, syscall);
	return (0);
}

/*
 * Usage: sysctl linux.debug=<syscall_nr>.<0/1>
 *
 *    E.g.: sysctl linux.debug=21.0
 *
 * As a special case, syscall "all" will apply to all syscalls globally.
 */
#define LINUX_MAX_DEBUGSTR	16
static int
linux_sysctl_debug(SYSCTL_HANDLER_ARGS)
{
	char value[LINUX_MAX_DEBUGSTR], *p;
	int error, sysc, toggle;
	int global = 0;

	value[0] = '\0';
	error = sysctl_handle_string(oidp, value, LINUX_MAX_DEBUGSTR, req);
	if (error || req->newptr == NULL)
		return (error);
	for (p = value; *p != '\0' && *p != '.'; p++);
	if (*p == '\0')
		return (EINVAL);
	*p++ = '\0';
	sysc = strtol(value, NULL, 0);
	toggle = strtol(p, NULL, 0);
	if (strcmp(value, "all") == 0)
		global = 1;
	error = linux_debug(sysc, toggle, global);
	return (error);
}

SYSCTL_PROC(_compat_linux, OID_AUTO, debug,
            CTLTYPE_STRING | CTLFLAG_RW,
            0, 0, linux_sysctl_debug, "A",
            "Linux debugging control");

#endif /* DEBUG || KTR */
