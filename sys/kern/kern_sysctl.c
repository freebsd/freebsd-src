/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
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
 * $Id: kern_sysctl.c,v 1.25.4.7 1996/09/19 08:18:03 pst Exp $
 */

/*
 * sysctl system call.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/unistd.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <vm/vm.h>
#include <sys/sysctl.h>

#ifdef DEBUG
static sysctlfn debug_sysctl;
#endif

/*
 * Locking and stats
 */
static struct sysctl_lock {
	int	sl_lock;
	int	sl_want;
	int	sl_locked;
} memlock;

struct sysctl_args {
	int	*name;
	u_int	namelen;
	void	*old;
	size_t	*oldlenp;
	void	*new;
	size_t	newlen;
};

int
__sysctl(p, uap, retval)
	struct proc *p;
	register struct sysctl_args *uap;
	int *retval;
{
	int error, dolock = 1;
	u_int savelen = 0, oldlen = 0;
	sysctlfn *fn;
	int name[CTL_MAXNAME];

	if (uap->new != NULL && (error = suser(p->p_ucred, &p->p_acflag)))
		return (error);
	/*
	 * all top-level sysctl names are non-terminal
	 */
	if (uap->namelen > CTL_MAXNAME || uap->namelen < 2)
		return (EINVAL);
 	error = copyin(uap->name, &name, uap->namelen * sizeof(int));
 	if (error)
		return (error);

	switch (name[0]) {
	case CTL_KERN:
		fn = kern_sysctl;
		if (name[1] != KERN_VNODE)      /* XXX */
			dolock = 0;
		break;
	case CTL_HW:
		fn = hw_sysctl;
		break;
	case CTL_VM:
		fn = vm_sysctl;
		break;
	case CTL_NET:
		fn = net_sysctl;
		break;
	case CTL_FS:
		fn = fs_sysctl;
		break;
	case CTL_MACHDEP:
		fn = cpu_sysctl;
		break;
#ifdef DEBUG
	case CTL_DEBUG:
		fn = debug_sysctl;
		break;
#endif
	default:
		return (EOPNOTSUPP);
	}

	if (uap->oldlenp &&
	    (error = copyin(uap->oldlenp, &oldlen, sizeof(oldlen))))
		return (error);
	if (uap->old != NULL) {
		if (!useracc(uap->old, oldlen, B_WRITE))
			return (EFAULT);
		while (memlock.sl_lock) {
			memlock.sl_want = 1;
			(void) tsleep((caddr_t)&memlock, PRIBIO+1, "sysctl", 0);
			memlock.sl_locked++;
		}
		memlock.sl_lock = 1;
		if (dolock)
			vslock(uap->old, oldlen);
		savelen = oldlen;
	}
	error = (*fn)(name + 1, uap->namelen - 1, uap->old, &oldlen,
	    uap->new, uap->newlen, p);
	if (uap->old != NULL) {
		if (dolock)
			vsunlock(uap->old, savelen, B_WRITE);
		memlock.sl_lock = 0;
		if (memlock.sl_want) {
			memlock.sl_want = 0;
			wakeup((caddr_t)&memlock);
		}
	}
	if (error)
		return (error);
	if (uap->oldlenp)
		error = copyout(&oldlen, uap->oldlenp, sizeof(oldlen));
	*retval = oldlen;
	return (0);
}

/*
 * Attributes stored in the kernel.
 */
char hostname[MAXHOSTNAMELEN];
int hostnamelen;
char domainname[MAXHOSTNAMELEN];
int domainnamelen;
long hostid;
int securelevel = -1;
char kernelname[MAXPATHLEN] = "/kernel";	/* XXX bloat */
extern int vfs_update_wakeup;
extern int vfs_update_interval;
extern int osreldate;
extern int exec_ps_strings;
extern int exec_usrstack;
extern int somaxconn;
extern u_long sb_max;
extern u_long sb_efficiency;
extern int sominqueue;

/*
 * kernel related system variables.
 */
int
kern_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	int error, level, inthostid, intsb_max, intsb_efficiency;
	dev_t ndumpdev;

	/* all sysctl names at this level are terminal */
	if (namelen != 1 && !(name[0] == KERN_PROC || name[0] == KERN_PROF
			      || name[0] == KERN_NTP_PLL))
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case KERN_OSTYPE:
		return (sysctl_rdstring(oldp, oldlenp, newp, ostype));
	case KERN_OSRELEASE:
		return (sysctl_rdstring(oldp, oldlenp, newp, osrelease));
	case KERN_OSREV:
		return (sysctl_rdint(oldp, oldlenp, newp, BSD));
	case KERN_VERSION:
		return (sysctl_rdstring(oldp, oldlenp, newp, version));
	case KERN_OSRELDATE:
		return (sysctl_rdint(oldp, oldlenp, newp, osreldate));
	case KERN_BOOTFILE:
		return (sysctl_string(oldp, oldlenp, newp, newlen,
				      kernelname, sizeof kernelname));
	case KERN_MAXVNODES:
		return(sysctl_int(oldp, oldlenp, newp, newlen, &desiredvnodes));
	case KERN_MAXPROC:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &maxproc));
	case KERN_MAXPROCPERUID:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &maxprocperuid));
	case KERN_MAXFILES:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &maxfiles));
	case KERN_MAXFILESPERPROC:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &maxfilesperproc));
	case KERN_UPDATEINTERVAL:
		/*
		 * NB: this simple-minded approach only works because
		 * `tsleep' takes a timeout argument of 0 as meaning
		 * `no timeout'.
		 */
		error = sysctl_int(oldp, oldlenp, newp, newlen,
				   &vfs_update_interval);
		if(!error) {
			wakeup(&vfs_update_wakeup);
		}
		return error;
	case KERN_ARGMAX:
		return (sysctl_rdint(oldp, oldlenp, newp, ARG_MAX));
	case KERN_SECURELVL:
		level = securelevel;
		if ((error = sysctl_int(oldp, oldlenp, newp, newlen, &level)) ||
		    newp == NULL)
			return (error);
		if (level < securelevel)
			return (EPERM);
		securelevel = level;
		return (0);
	case KERN_HOSTNAME:
		error = sysctl_string(oldp, oldlenp, newp, newlen,
		    hostname, sizeof(hostname));
		if (newp)
			if (error == 0 || error == ENOMEM)
				hostnamelen = newlen;
		return (error);
	case KERN_DOMAINNAME:
		error = sysctl_string(oldp, oldlenp, newp, newlen,
		    domainname, sizeof(domainname));
		if (newp)
			if (error == 0 || error == ENOMEM)
				domainnamelen = newlen;
		return (error);
	case KERN_HOSTID:
		inthostid = hostid;  /* XXX assumes sizeof long <= sizeof int */
		error =  sysctl_int(oldp, oldlenp, newp, newlen, &inthostid);
		hostid = inthostid;
		return (error);
	case KERN_CLOCKRATE:
		return (sysctl_clockrate(oldp, oldlenp));
	case KERN_BOOTTIME:
		return (sysctl_rdstruct(oldp, oldlenp, newp, &boottime,
		    sizeof(struct timeval)));
	case KERN_VNODE:
		return (sysctl_vnode(oldp, oldlenp));
	case KERN_PROC:
		return (sysctl_doproc(name + 1, namelen - 1, oldp, oldlenp));
	case KERN_FILE:
		return (sysctl_file(oldp, oldlenp));
#ifdef GPROF
	case KERN_PROF:
		return (sysctl_doprof(name + 1, namelen - 1, oldp, oldlenp,
		    newp, newlen));
#endif
	case KERN_POSIX1:
		return (sysctl_rdint(oldp, oldlenp, newp, _POSIX_VERSION));
	case KERN_NGROUPS:
		return (sysctl_rdint(oldp, oldlenp, newp, NGROUPS_MAX));
	case KERN_JOB_CONTROL:
		return (sysctl_rdint(oldp, oldlenp, newp, 1));
	case KERN_SAVED_IDS:
#ifdef _POSIX_SAVED_IDS
		return (sysctl_rdint(oldp, oldlenp, newp, 1));
#else
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
#endif
	case KERN_NTP_PLL:
		return (ntp_sysctl(name + 1, namelen - 1, oldp, oldlenp,
				   newp, newlen, p));
	case KERN_DUMPDEV:
		ndumpdev = dumpdev;
		error = sysctl_struct(oldp, oldlenp, newp, newlen, &ndumpdev,
				      sizeof ndumpdev);
		if (!error && ndumpdev != dumpdev) {
			error = setdumpdev(ndumpdev);
		}
		return error;
	case KERN_SOMAXCONN:
		return(sysctl_int(oldp, oldlenp, newp, newlen, &somaxconn));
	case KERN_MAXSOCKBUF:
		intsb_max = sb_max;  /* XXX assumes sizeof long <= sizeof int */
		error = sysctl_int(oldp, oldlenp, newp, newlen, &intsb_max);
		sb_max = intsb_max;
		return (error);
	case KERN_PS_STRINGS:
		return(sysctl_int(oldp, oldlenp, newp, newlen, &exec_ps_strings));
	case KERN_USRSTACK:
		return(sysctl_int(oldp, oldlenp, newp, newlen, &exec_usrstack));
	case KERN_SOCKBUF_WASTE:
		intsb_efficiency = sb_efficiency;  /* XXX assumes sizeof long <= sizeof int */
		error = sysctl_int(oldp, oldlenp, newp, newlen, &intsb_efficiency);
		sb_efficiency = intsb_efficiency;
		return (error);
	case KERN_SOMINQUEUE:
		return(sysctl_int(oldp, oldlenp, newp, newlen, &sominqueue));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * hardware related system variables.
 */
int
hw_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	/* almost all sysctl names at this level are terminal */
	if (namelen != 1 && name[0] != HW_DEVCONF)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case HW_MACHINE:
		return (sysctl_rdstring(oldp, oldlenp, newp, machine));
	case HW_MODEL:
		return (sysctl_rdstring(oldp, oldlenp, newp, cpu_model));
	case HW_NCPU:
		return (sysctl_rdint(oldp, oldlenp, newp, 1));	/* XXX */
	case HW_BYTEORDER:
		return (sysctl_rdint(oldp, oldlenp, newp, BYTE_ORDER));
	case HW_PHYSMEM:
		return (sysctl_rdint(oldp, oldlenp, newp, ctob(physmem)));
	case HW_USERMEM:
		return (sysctl_rdint(oldp, oldlenp, newp,
		    ctob(physmem - cnt.v_wire_count)));
	case HW_PAGESIZE:
		return (sysctl_rdint(oldp, oldlenp, newp, PAGE_SIZE));
	case HW_FLOATINGPT:
		return (sysctl_rdint(oldp, oldlenp, newp, hw_float));
	case HW_DEVCONF:
		return (dev_sysctl(name + 1, namelen - 1, oldp, oldlenp,
				   newp, newlen, p));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

#ifdef DEBUG
/*
 * Debugging related system variables.
 */
struct ctldebug debug0, debug1, debug2, debug3, debug4;
struct ctldebug debug5, debug6, debug7, debug8, debug9;
struct ctldebug debug10, debug11, debug12, debug13, debug14;
struct ctldebug debug15, debug16, debug17, debug18, debug19;
static struct ctldebug *debugvars[CTL_DEBUG_MAXID] = {
	&debug0, &debug1, &debug2, &debug3, &debug4,
	&debug5, &debug6, &debug7, &debug8, &debug9,
	&debug10, &debug11, &debug12, &debug13, &debug14,
	&debug15, &debug16, &debug17, &debug18, &debug19,
};
static int
debug_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	struct ctldebug *cdp;

	/* all sysctl names at this level are name and field */
	if (namelen != 2)
		return (ENOTDIR);		/* overloaded */
	cdp = debugvars[name[0]];
	if (cdp->debugname == 0)
		return (EOPNOTSUPP);
	switch (name[1]) {
	case CTL_DEBUG_NAME:
		return (sysctl_rdstring(oldp, oldlenp, newp, cdp->debugname));
	case CTL_DEBUG_VALUE:
		return (sysctl_int(oldp, oldlenp, newp, newlen, cdp->debugvar));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}
#endif /* DEBUG */

/*
 * Validate parameters and get old / set new parameters
 * for an integer-valued sysctl function.
 */
int
sysctl_int(oldp, oldlenp, newp, newlen, valp)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	int *valp;
{
	int error = 0;

	if (oldp && *oldlenp < sizeof(int))
		return (ENOMEM);
	if (newp && newlen != sizeof(int))
		return (EINVAL);
	*oldlenp = sizeof(int);
	if (oldp)
		error = copyout(valp, oldp, sizeof(int));
	if (error == 0 && newp)
		error = copyin(newp, valp, sizeof(int));
	return (error);
}

/*
 * As above, but read-only.
 */
int
sysctl_rdint(oldp, oldlenp, newp, val)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	int val;
{
	int error = 0;

	if (oldp && *oldlenp < sizeof(int))
		return (ENOMEM);
	if (newp)
		return (EPERM);
	*oldlenp = sizeof(int);
	if (oldp)
		error = copyout((caddr_t)&val, oldp, sizeof(int));
	return (error);
}

/*
 * Validate parameters and get old / set new parameters
 * for a string-valued sysctl function.
 */
int
sysctl_string(oldp, oldlenp, newp, newlen, str, maxlen)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	char *str;
	int maxlen;
{
	int len, error = 0, rval = 0;

	len = strlen(str) + 1;
	if (oldp && *oldlenp < len) {
		len = *oldlenp;
		rval = ENOMEM;
	}
	if (newp && newlen >= maxlen)
		return (EINVAL);
	if (oldp) {
		*oldlenp = len;
		error = copyout(str, oldp, len);
		if (error)
			rval = error;
	}
	if ((error == 0 || error == ENOMEM) && newp) {
		error = copyin(newp, str, newlen);
		if (error)
			rval = error;
		str[newlen] = 0;
	}
	return (rval);
}

/*
 * As above, but read-only.
 */
int
sysctl_rdstring(oldp, oldlenp, newp, str)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	char *str;
{
	int len, error = 0, rval = 0;

	len = strlen(str) + 1;
	if (oldp && *oldlenp < len) {
		len = *oldlenp;
		rval = ENOMEM;
	}
	if (newp)
		return (EPERM);
	*oldlenp = len;
	if (oldp)
		error = copyout(str, oldp, len);
		if (error)
			rval = error;
	return (rval);
}

/*
 * Validate parameters and get old / set new parameters
 * for a structure oriented sysctl function.
 */
int
sysctl_struct(oldp, oldlenp, newp, newlen, sp, len)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	void *sp;
	int len;
{
	int error = 0;

	if (oldp && *oldlenp < len)
		return (ENOMEM);
	if (newp && newlen > len)
		return (EINVAL);
	if (oldp) {
		*oldlenp = len;
		error = copyout(sp, oldp, len);
	}
	if (error == 0 && newp)
		error = copyin(newp, sp, len);
	return (error);
}

/*
 * Validate parameters and get old parameters
 * for a structure oriented sysctl function.
 */
int
sysctl_rdstruct(oldp, oldlenp, newp, sp, len)
	void *oldp;
	size_t *oldlenp;
	void *newp, *sp;
	int len;
{
	int error = 0;

	if (oldp && *oldlenp < len)
		return (ENOMEM);
	if (newp)
		return (EPERM);
	*oldlenp = len;
	if (oldp)
		error = copyout(sp, oldp, len);
	return (error);
}

/*
 * Get file structures.
 */
int
sysctl_file(where, sizep)
	char *where;
	size_t *sizep;
{
	int buflen, error;
	struct file *fp;
	char *start = where;

	buflen = *sizep;
	if (where == NULL) {
		/*
		 * overestimate by 10 files
		 */
		*sizep = sizeof(filehead) + (nfiles + 10) * sizeof(struct file);
		return (0);
	}

	/*
	 * first copyout filehead
	 */
	if (buflen < sizeof(filehead)) {
		*sizep = 0;
		return (0);
	}
	error = copyout((caddr_t)&filehead, where, sizeof(filehead));
	if (error)
		return (error);
	buflen -= sizeof(filehead);
	where += sizeof(filehead);

	/*
	 * followed by an array of file structures
	 */
	for (fp = filehead; fp != NULL; fp = fp->f_filef) {
		if (buflen < sizeof(struct file)) {
			*sizep = where - start;
			return (ENOMEM);
		}
		error = copyout((caddr_t)fp, where, sizeof (struct file));
		if (error)
			return (error);
		buflen -= sizeof(struct file);
		where += sizeof(struct file);
	}
	*sizep = where - start;
	return (0);
}

/*
 * try over estimating by 5 procs
 */
#define KERN_PROCSLOP	(5 * sizeof (struct kinfo_proc))

int
sysctl_doproc(name, namelen, where, sizep)
	int *name;
	u_int namelen;
	char *where;
	size_t *sizep;
{
	register struct proc *p;
	register struct kinfo_proc *dp;
	register int needed;
	int buflen;
	int doingzomb;
	struct eproc eproc;
	int error = 0;

	if (namelen != 2 && !(namelen == 1 && name[0] == KERN_PROC_ALL))
		return (EINVAL);
restart:
	doingzomb = 0;
	p = (struct proc *)allproc;
	if (where != NULL)
		buflen = *sizep;
	else
		buflen = 0;
	dp = (struct kinfo_proc *)where;
	needed = 0;
again:
	for (; p != NULL; p = p->p_next) {
		/*
		 * Skip embryonic processes.
		 */
		if (p->p_stat == SIDL)
			continue;
		/*
		 * TODO - make more efficient (see notes below).
		 * do by session.
		 */
		switch (name[0]) {

		case KERN_PROC_PID:
			/* could do this with just a lookup */
			if (p->p_pid != (pid_t)name[1])
				continue;
			break;

		case KERN_PROC_PGRP:
			/* could do this by traversing pgrp */
			if (p->p_pgrp == NULL || p->p_pgrp->pg_id != (pid_t)name[1])
				continue;
			break;

		case KERN_PROC_TTY:
			if ((p->p_flag & P_CONTROLT) == 0 ||
			    p->p_session == NULL ||
			    p->p_session->s_ttyp == NULL ||
			    p->p_session->s_ttyp->t_dev != (dev_t)name[1])
				continue;
			break;

		case KERN_PROC_UID:
			if (p->p_ucred == NULL || p->p_ucred->cr_uid != (uid_t)name[1])
				continue;
			break;

		case KERN_PROC_RUID:
			if (p->p_ucred == NULL || p->p_cred->p_ruid != (uid_t)name[1])
				continue;
			break;
		}
		if (buflen >= sizeof(struct kinfo_proc)) {
			pid_t pid = p->p_pid;

			fill_eproc(p, &eproc);
			error = copyout((caddr_t)p, &dp->kp_proc,
			    sizeof(struct proc));
			if (error)
				return (error);
			/*
			 * Since copyout can block, our cached struct proc * may
			 * no longer be valid. For allproc, restart from the
			 * beginning if the process no longer exists. For
			 * zombproc, just stop if it's gone.
			 */
			if (!doingzomb) {
				if (pid && (pfind(pid) != p))
					goto restart;
			} else {
				if (zpfind(pid) != p)
					break;
			}
			error = copyout((caddr_t)&eproc, &dp->kp_eproc,
			    sizeof(eproc));
			if (error)
				return (error);
			if (!doingzomb) {
				if (pid && (pfind(pid) != p))
					goto restart;
			} else {
				if (zpfind(pid) != p)
					break;
			}
			dp++;
			buflen -= sizeof(struct kinfo_proc);
		}
		needed += sizeof(struct kinfo_proc);
	}
	if (doingzomb == 0) {
		p = zombproc;
		doingzomb++;
		goto again;
	}
	if (where != NULL) {
		*sizep = (caddr_t)dp - where;
		if (needed > *sizep)
			return (ENOMEM);
	} else {
		needed += KERN_PROCSLOP;
		*sizep = needed;
	}
	return (0);
}

/*
 * Fill in an eproc structure for the specified process.
 */
void
fill_eproc(p, ep)
	register struct proc *p;
	register struct eproc *ep;
{
	register struct tty *tp;

	bzero(ep, sizeof(*ep));

	ep->e_paddr = p;
	if (p->p_cred) {
		ep->e_pcred = *p->p_cred;
		if (p->p_ucred)
			ep->e_ucred = *p->p_ucred;
	}
	if (p->p_stat != SIDL && p->p_stat != SZOMB && p->p_vmspace != NULL) {
		register struct vmspace *vm = p->p_vmspace;

#ifdef pmap_resident_count
		ep->e_vm.vm_rssize = pmap_resident_count(&vm->vm_pmap); /*XXX*/
#else
		ep->e_vm.vm_rssize = vm->vm_rssize;
#endif
		ep->e_vm.vm_tsize = vm->vm_tsize;
		ep->e_vm.vm_dsize = vm->vm_dsize;
		ep->e_vm.vm_ssize = vm->vm_ssize;
#ifndef sparc
		ep->e_vm.vm_pmap = vm->vm_pmap;
#endif
	}
	if (p->p_pptr)
		ep->e_ppid = p->p_pptr->p_pid;
	if (p->p_pgrp) {
		ep->e_pgid = p->p_pgrp->pg_id;
		ep->e_jobc = p->p_pgrp->pg_jobc;
		ep->e_sess = p->p_pgrp->pg_session;

		if (ep->e_sess) {
			bcopy(ep->e_sess->s_login, ep->e_login, sizeof(ep->e_login));
			if (ep->e_sess->s_ttyvp)
				ep->e_flag = EPROC_CTTY;
			if (p->p_session && SESS_LEADER(p))
				ep->e_flag |= EPROC_SLEADER;
		}
	}
	if ((p->p_flag & P_CONTROLT) &&
	    (ep->e_sess != NULL) &&
	    ((tp = ep->e_sess->s_ttyp) != NULL)) {
		ep->e_tdev = tp->t_dev;
		ep->e_tpgid = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
		ep->e_tsess = tp->t_session;
	} else
		ep->e_tdev = NODEV;
	if (p->p_wmesg) {
		strncpy(ep->e_wmesg, p->p_wmesg, WMESGLEN);
		ep->e_wmesg[WMESGLEN] = 0;
	}
}

#ifdef COMPAT_43
#include <sys/socket.h>
#define	KINFO_PROC		(0<<8)
#define	KINFO_RT		(1<<8)
#define	KINFO_VNODE		(2<<8)
#define	KINFO_FILE		(3<<8)
#define	KINFO_METER		(4<<8)
#define	KINFO_LOADAVG		(5<<8)
#define	KINFO_CLOCKRATE		(6<<8)

/* Non-standard BSDI extension - only present on their 4.3 net-2 releases */
#define	KINFO_BSDI_SYSINFO	(101<<8)

/*
 * XXX this is bloat, but I hope it's better here than on the potentially
 * limited kernel stack...  -Peter
 */

struct {
	int	bsdi_machine;		/* "i386" on BSD/386 */
/*      ^^^ this is an offset to the string, relative to the struct start */
	char	*pad0;
	long	pad1;
	long	pad2;
	long	pad3;
	u_long	pad4;
	u_long	pad5;
	u_long	pad6;

	int	bsdi_ostype;		/* "BSD/386" on BSD/386 */
	int	bsdi_osrelease;		/* "1.1" on BSD/386 */
	long	pad7;
	long	pad8;
	char	*pad9;

	long	pad10;
	long	pad11;
	int	pad12;
	long	pad13;
	quad_t	pad14;
	long	pad15;

	struct	timeval pad16;
	/* we dont set this, because BSDI's uname used gethostname() instead */
	int	bsdi_hostname;		/* hostname on BSD/386 */

	/* the actual string data is appended here */

} bsdi_si;
/*
 * this data is appended to the end of the bsdi_si structure during copyout.
 * The "char *" offsets are relative to the base of the bsdi_si struct.
 * This contains "FreeBSD\02.0-BUILT-nnnnnn\0i386\0", and these strings
 * should not exceed the length of the buffer here... (or else!! :-)
 */
char bsdi_strings[80];	/* It had better be less than this! */

struct getkerninfo_args {
	int	op;
	char	*where;
	int	*size;
	int	arg;
};

int
ogetkerninfo(p, uap, retval)
	struct proc *p;
	register struct getkerninfo_args *uap;
	int *retval;
{
	int error, name[5];
	u_int size;

	if (uap->size &&
	    (error = copyin((caddr_t)uap->size, (caddr_t)&size, sizeof(size))))
		return (error);

	switch (uap->op & 0xff00) {

	case KINFO_RT:
		name[0] = PF_ROUTE;
		name[1] = 0;
		name[2] = (uap->op & 0xff0000) >> 16;
		name[3] = uap->op & 0xff;
		name[4] = uap->arg;
		error = net_sysctl(name, 5, uap->where, &size, NULL, 0, p);
		break;

	case KINFO_VNODE:
		name[0] = KERN_VNODE;
		error = kern_sysctl(name, 1, uap->where, &size, NULL, 0, p);
		break;

	case KINFO_PROC:
		name[0] = KERN_PROC;
		name[1] = uap->op & 0xff;
		name[2] = uap->arg;
		error = kern_sysctl(name, 3, uap->where, &size, NULL, 0, p);
		break;

	case KINFO_FILE:
		name[0] = KERN_FILE;
		error = kern_sysctl(name, 1, uap->where, &size, NULL, 0, p);
		break;

	case KINFO_METER:
		name[0] = VM_METER;
		error = vm_sysctl(name, 1, uap->where, &size, NULL, 0, p);
		break;

	case KINFO_LOADAVG:
		name[0] = VM_LOADAVG;
		error = vm_sysctl(name, 1, uap->where, &size, NULL, 0, p);
		break;

	case KINFO_CLOCKRATE:
		name[0] = KERN_CLOCKRATE;
		error = kern_sysctl(name, 1, uap->where, &size, NULL, 0, p);
		break;

	case KINFO_BSDI_SYSINFO: {
		/*
		 * this is pretty crude, but it's just enough for uname()
		 * from BSDI's 1.x libc to work.
		 *
		 * In particular, it doesn't return the same results when
		 * the supplied buffer is too small.  BSDI's version apparently
		 * will return the amount copied, and set the *size to how
		 * much was needed.  The emulation framework here isn't capable
		 * of that, so we just set both to the amount copied.
		 * BSDI's 2.x product apparently fails with ENOMEM in this
		 * scenario.
		 */

		u_int needed;
		u_int left;
		char *s;

		bzero((char *)&bsdi_si, sizeof(bsdi_si));
		bzero(bsdi_strings, sizeof(bsdi_strings));

		s = bsdi_strings;

		bsdi_si.bsdi_ostype = (s - bsdi_strings) + sizeof(bsdi_si);
		strcpy(s, ostype);
		s += strlen(s) + 1;

		bsdi_si.bsdi_osrelease = (s - bsdi_strings) + sizeof(bsdi_si);
		strcpy(s, osrelease);
		s += strlen(s) + 1;

		bsdi_si.bsdi_machine = (s - bsdi_strings) + sizeof(bsdi_si);
		strcpy(s, machine);
		s += strlen(s) + 1;

		needed = sizeof(bsdi_si) + (s - bsdi_strings);

		if (uap->where == NULL) {
			/* process is asking how much buffer to supply.. */
			size = needed;
			error = 0;
			break;
		}


		/* if too much buffer supplied, trim it down */
		if (size > needed)
			size = needed;

		/* how much of the buffer is remaining */
		left = size;

		if ((error = copyout((char *)&bsdi_si, uap->where, left)) != 0)
			break;

		/* is there any point in continuing? */
		if (left > sizeof(bsdi_si)) {
			left -= sizeof(bsdi_si);
			error = copyout(&bsdi_strings,
					uap->where + sizeof(bsdi_si), left);
		}
		break;
	}

	default:
		return (EOPNOTSUPP);
	}
	if (error)
		return (error);
	*retval = size;
	if (uap->size)
		error = copyout((caddr_t)&size, (caddr_t)uap->size,
		    sizeof(size));
	return (error);
}
#endif /* COMPAT_43 */
