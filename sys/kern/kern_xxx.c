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
 * $Id: kern_xxx.c,v 1.17 1995/11/12 06:43:03 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <vm/vm.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <sys/signalvar.h>

/* This implements a "TEXT_SET" for cleanup functions */

static void
dummy_cleanup() {}
TEXT_SET(cleanup_set, dummy_cleanup);

typedef void (*cleanup_func_t)(void);
extern const struct linker_set cleanup_set;
static const cleanup_func_t *cleanups =
        (const cleanup_func_t *)&cleanup_set.ls_items[0];

#ifndef _SYS_SYSPROTO_H_
struct reboot_args {
	int	opt;
};
#endif
/* ARGSUSED */
int
reboot(p, uap, retval)
	struct proc *p;
	struct reboot_args *uap;
	int *retval;
{
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)))
		return (error);

	if (!uap->opt & RB_NOSYNC) {
		printf("\ncleaning up... ");
                while(*cleanups) {
                        (**cleanups++)();
                }
	}

	boot(uap->opt);
	return (0);
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)

#ifndef _SYS_SYSPROTO_H_
struct gethostname_args {
	char	*hostname;
	u_int	len;
};
#endif
/* ARGSUSED */
int
ogethostname(p, uap, retval)
	struct proc *p;
	struct gethostname_args *uap;
	int *retval;
{
	int name[2];

	name[0] = CTL_KERN;
	name[1] = KERN_HOSTNAME;
	return (userland_sysctl(p, name, 2, uap->hostname, &uap->len, 
		1, 0, 0, 0));
}

#ifndef _SYS_SYSPROTO_H_
struct sethostname_args {
	char	*hostname;
	u_int	len;
};
#endif
/* ARGSUSED */
int
osethostname(p, uap, retval)
	struct proc *p;
	register struct sethostname_args *uap;
	int *retval;
{
	int name[2];
	int error;

	name[0] = CTL_KERN;
	name[1] = KERN_HOSTNAME;
	if ((error = suser(p->p_ucred, &p->p_acflag)))
		return (error);
	return (userland_sysctl(p, name, 2, 0, 0, 0,
		uap->hostname, uap->len, 0));
}

#ifndef _SYS_SYSPROTO_H_
struct ogethostid_args {
	int	dummy;
};
#endif
/* ARGSUSED */
int
ogethostid(p, uap, retval)
	struct proc *p;
	struct ogethostid_args *uap;
	int *retval;
{

	*(long *)retval = hostid;
	return (0);
}
#endif /* COMPAT_43 || COMPAT_SUNOS */

#ifdef COMPAT_43
#ifndef _SYS_SYSPROTO_H_
struct osethostid_args {
	long	hostid;
};
#endif
/* ARGSUSED */
int
osethostid(p, uap, retval)
	struct proc *p;
	struct osethostid_args *uap;
	int *retval;
{
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)))
		return (error);
	hostid = uap->hostid;
	return (0);
}

int
oquota(p, uap, retval)
	struct proc *p;
	struct oquota_args *uap;
	int *retval;
{

	return (ENOSYS);
}
#endif /* COMPAT_43 */

void
shutdown_nice(void)
{
	/* Send a signal to init(8) and have it shutdown the world */
	if (initproc != NULL) {
		psignal(initproc, SIGINT);
	} else {
		/* No init(8) running, so simply reboot */
		boot(RB_NOSYNC);
	}
	return;
}


#ifndef _SYS_SYSPROTO_H_
struct uname_args {
        struct utsname  *name;
};
#endif

/* ARGSUSED */
int
uname(p, uap, retval)
	struct proc *p;
	struct uname_args *uap;
	int *retval;
{
	int name[2], len, rtval, junk;
	char *s, *us;

	name[0] = CTL_KERN;
	name[1] = KERN_OSTYPE;
	len = sizeof uap->name->sysname;
	rtval = userland_sysctl(p, name, 2, uap->name->sysname, &len, 
		1, 0, 0, 0);
	if( rtval) return rtval;
	subyte( uap->name->sysname + sizeof(uap->name->sysname) - 1, 0);

	name[1] = KERN_HOSTNAME;
	len = sizeof uap->name->nodename;
	rtval = userland_sysctl(p, name, 2, uap->name->nodename, &len, 
		1, 0, 0, 0);
	if( rtval) return rtval;
	subyte( uap->name->nodename + sizeof(uap->name->nodename) - 1, 0);

	name[1] = KERN_OSRELEASE;
	len = sizeof uap->name->release;
	rtval = userland_sysctl(p, name, 2, uap->name->release, &len, 
		1, 0, 0, 0);
	if( rtval) return rtval;
	subyte( uap->name->release + sizeof(uap->name->release) - 1, 0);

/*
	name = KERN_VERSION;
	len = sizeof uap->name->version;
	rtval = userland_sysctl(p, name, 2, uap->name->version, &len, 
		1, 0, 0, 0);
	if( rtval) return rtval;
	subyte( uap->name->version + sizeof(uap->name->version) - 1, 0);
*/

/*
 * this stupid hackery to make the version field look like FreeBSD 1.1
 */
	for(s = version; *s && *s != '#'; s++);

	for(us = uap->name->version; *s && *s != ':'; s++) {
		rtval = subyte( us++, *s);
		if( rtval)
			return rtval;
	}
	rtval = subyte( us++, 0);
	if( rtval)
		return rtval;

	name[1] = HW_MACHINE;
	len = sizeof uap->name->machine;
	rtval = userland_sysctl(p, name, 2, uap->name->machine, &len, 
		1, 0, 0, 0);
	if( rtval) return rtval;
	subyte( uap->name->machine + sizeof(uap->name->machine) - 1, 0);

	return 0;
}

#ifndef _SYS_SYSPROTO_H_
struct getdomainname_args {
        char    *domainname;
        int     len;
};
#endif

/* ARGSUSED */
int
getdomainname(p, uap, retval)
        struct proc *p;
        struct getdomainname_args *uap;
        int *retval;
{
	if ((u_int)uap->len > domainnamelen + 1)
		uap->len = domainnamelen + 1;
	return (copyout((caddr_t)domainname, (caddr_t)uap->domainname, uap->len));
}

#ifndef _SYS_SYSPROTO_H_
struct setdomainname_args {
        char    *domainname;
        int     len;
};
#endif

/* ARGSUSED */
int
setdomainname(p, uap, retval)
        struct proc *p;
        struct setdomainname_args *uap;
        int *retval;
{
        int error;

        if ((error = suser(p->p_ucred, &p->p_acflag)))
                return (error);
        if ((u_int)uap->len > sizeof (domainname) - 1)
                return EINVAL;
        domainnamelen = uap->len;
        error = copyin((caddr_t)uap->domainname, domainname, uap->len);
        domainname[domainnamelen] = 0;
        return (error);
}

