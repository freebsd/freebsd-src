/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)kern_xxx.c	8.2 (Berkeley) 11/14/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <vm/vm.h>
#include <sys/sysctl.h>

struct reboot_args {
	int	opt;
};
/* ARGSUSED */
reboot(p, uap, retval)
	struct proc *p;
	struct reboot_args *uap;
	int *retval;
{
	int error;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	boot(uap->opt);
	return (0);
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)

struct gethostname_args {
	char	*hostname;
	u_int	len;
};
/* ARGSUSED */
ogethostname(p, uap, retval)
	struct proc *p;
	struct gethostname_args *uap;
	int *retval;
{
	int name;

	name = KERN_HOSTNAME;
	return (kern_sysctl(&name, 1, uap->hostname, &uap->len, 0, 0));
}

struct sethostname_args {
	char	*hostname;
	u_int	len;
};
/* ARGSUSED */
osethostname(p, uap, retval)
	struct proc *p;
	register struct sethostname_args *uap;
	int *retval;
{
	int name;
	int error;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	name = KERN_HOSTNAME;
	return (kern_sysctl(&name, 1, 0, 0, uap->hostname, uap->len));
}

extern long hostid;

struct gethostid_args {
	int	dummy;
};
/* ARGSUSED */
ogethostid(p, uap, retval)
	struct proc *p;
	struct gethostid_args *uap;
	int *retval;
{

	*(long *)retval = hostid;
	return (0);
}
#endif /* COMPAT_43 || COMPAT_SUNOS */

#ifdef COMPAT_43
struct sethostid_args {
	long	hostid;
};
/* ARGSUSED */
osethostid(p, uap, retval)
	struct proc *p;
	struct sethostid_args *uap;
	int *retval;
{
	int error;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	hostid = uap->hostid;
	return (0);
}

oquota()
{

	return (ENOSYS);
}
#endif /* COMPAT_43 */
