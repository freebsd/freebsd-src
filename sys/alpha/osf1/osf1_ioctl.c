/*	$NetBSD: osf1_ioctl.c,v 1.5 1996/10/13 00:46:53 christos Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Additional Copyright (c) 1999 by Andrew Gallatin
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/ioctl_compat.h>
#include <sys/termios.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/sysproto.h>
#include <alpha/osf1/osf1_signal.h>
#include <alpha/osf1/osf1_proto.h>
#include <alpha/osf1/osf1.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <sys/sockio.h>

#include "opt_compat.h"

/*#define	IOCTL_DEBUG*/

int osf1_ioctl_i	__P((struct proc *p, struct ioctl_args *nuap,
			    int cmd, int dir, int len));
int osf1_ioctl_t	__P((struct proc *p, struct ioctl_args *nuap,
			    int cmd, int dir, int len));
int osf1_ioctl_f	__P((struct proc *p, struct ioctl_args *nuap,
			    int cmd, int dir, int len));
int osf1_ioctl_m	__P((struct proc *p, struct ioctl_args *nuap,
			    int cmd, int dir, int len));

int
osf1_ioctl(p, uap)
	struct proc *p;
	struct osf1_ioctl_args *uap;
{
	char *dirstr;
	unsigned int cmd, dir, group, len, op;
	struct ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ a;

	op = uap->com;
	dir = op & OSF1_IOC_DIRMASK;
	group = OSF1_IOCGROUP(op);
	cmd = OSF1_IOCCMD(op);
	len = OSF1_IOCPARM_LEN(op);

	switch (dir) {
	case OSF1_IOC_VOID:
		dir = IOC_VOID;
		dirstr = "none";
		break;

	case OSF1_IOC_OUT:
		dir = IOC_OUT;
		dirstr = "out";
		break;

	case OSF1_IOC_IN:
		dir = IOC_IN;
		dirstr = "in";
		break;

	case OSF1_IOC_INOUT:
		dir = IOC_INOUT;
		dirstr = "in-out";
		break;

	default:
		return (EINVAL);
		break;
	}
#ifdef IOCTL_DEBUG
		uprintf(
		    "OSF/1 IOCTL: group = %c, cmd = %d, len = %d, dir = %s\n",
		    group, cmd, len, dirstr);
#endif

	a.fd = uap->fd;
	a.com = (unsigned long)uap->com;
	bzero(&a.com, sizeof(long));
	a.com = _IOC(dir, group, cmd, len);
	a.data = uap->data;
	switch (group) {
	case 'i':
		return osf1_ioctl_i(p, &a, cmd, dir, len);
	case 't':
		return osf1_ioctl_t(p, &a, cmd, dir, len);
	case 'f':
		return osf1_ioctl_f(p, &a, cmd, dir, len);
	case 'm':
		return osf1_ioctl_m(p, &a, cmd, dir, len);
	case 'S':
		/*
		 * XXX SVR4 Streams IOCTLs are all unimpl.
		 */

#ifndef IOCTL_DEBUG
		return (0);
#endif		
	default:
		printf(
		    "unimplented OSF/1 IOCTL: group = %c, cmd = %d, len = %d, dir = %s\n",
		    group, cmd, len, dirstr);
		return (ENOTTY);
	}
}

/*
 * Structure used to query de and qe for physical addresses.
 */
struct osf1_ifdevea {
	char   ifr_name[IFNAMSIZ];	/* if name, e.g. "en0" */
	u_char default_pa[6];		/* default hardware address */
	u_char current_pa[6];		/* current physical address */
};


int
osf1_ioctl_i(p, uap, cmd, dir, len)
	struct proc *p;
	struct ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap;
	int cmd;
	int dir;
	int len;
{

	switch (cmd) {
	case 20:                        /* OSF/1 OSIOCGIFCONF */
	case 36:                        /* OSF/1  SIOCGIFCONF */
	case 12:			/* OSF/1 SIOCSIFADDR */
	case 14:			/* OSF/1 SIOCSIFDSTADDR */
	case 16:			/* OSF/1 SIOCSIFFLAGS (XXX) */
	case 17:			/* OSF/1 SIOCGIFFLAGS (XXX) */
	case 19:			/* OSF/1 SIOCSIFBRDADDR */
	case 22:			/* OSF/1 SIOCSIFNETMASK */
	case 23:			/* OSF/1 SIOCGIFMETRIC */
	case 24:			/* OSF/1 SIOCSIFMETRIC */
	case 25:			/* OSF/1 SIOCDIFADDR */
	case 33:			/* OSF/1 SIOCGIFADDR */
	case 34:			/* OSF/1 SIOCGIFDSTADDR */
	case 35:			/* OSF/1 SIOCGIFBRDADDR */
	case 37:			/* OSF/1 SIOCGIFNETMASK */
		/* same as in FreeBSD */
		return ioctl(p, uap);
		break;

	case 62:			/* OSF/1 SIOCRPHYSADDR */

	{
		int			ifn, retval;
		struct ifnet		*ifp;
		struct ifaddr		*ifa;
		struct sockaddr_dl	*sdl;
		struct osf1_ifdevea	*ifd = (struct osf1_ifdevea *)uap->data;
		
		/*
		 * Note that we don't actually respect the name in the ifreq
		 * structure, as DU interface names are all different.
		 */
		for (ifn = 0; ifn < if_index; ifn++) {
			ifp = ifnet_addrs[ifn]->ifa_ifp;    /* pointer to interface */
			/* Only look at ether interfaces, exclude alteon nics
			 * because osf/1 doesn't know about most of them.
			 */
			if (ifp->if_type == IFT_ETHER 
			    && strcmp(ifp->if_name, "ti")) {	/* looks good */
				/* walk the address list */
				TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
					if ((sdl = (struct sockaddr_dl *)ifa->ifa_addr)	/* we have an address structure */
					    && (sdl->sdl_family == AF_LINK)		/* it's a link address */
					    && (sdl->sdl_type == IFT_ETHER)) {		/* for an ethernet link */
						retval = copyout(LLADDR(sdl),
						    (caddr_t)&ifd->current_pa,
						    6);
						if (!retval) {
							return(copyout(
							    LLADDR(sdl),
							    (caddr_t)&ifd->default_pa,
							    6));
						}
					}
				}
			}
		}
		return(ENOENT);		/* ??? */
	}


	default:
		printf("osf1_ioctl_i: cmd = %d\n", cmd);
		return (ENOTTY);
	}


}
#ifndef _SGTTYB_
#define _SGTTYB_
struct sgttyb {
	char	sg_ispeed;		/* input speed */
	char	sg_ospeed;		/* output speed */
	char	sg_erase;		/* erase character */
	char	sg_kill;		/* kill character */
	short	sg_flags;		/* mode flags */
};
#endif

int
osf1_ioctl_t(p, uap, cmd, dir, len)
	struct proc *p;
	struct ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap;
	int cmd;
	int dir;
	int len;
{
	int retval;

	switch (cmd) {
#ifdef COMPAT_43
	case 0:				/* OSF/1 COMPAT_43 TIOCGETD  */
	case 1:				/* OSF/1 COMPAT_43 TIOCSETD  */
	case 8:				/* OSF/1 COMPAT_43 TIOCGETP  */
	case 9:				/* OSF/1 COMPAT_43 TIOCSETP  */
	case 10:                        /* OSF/1 COMPAT_43 TIOCSETN */
	case 17:			/* OSF/1 TIOCSETC (XXX) */
	case 18:			/* OSF/1 TIOCGETC (XXX) */
	case 116:			/* OSF/1 TIOCSLTC */
	case 117:			/* OSF/1 TIOCGLTC */
	case 124:			/* OSF/1 TIOCLGET */		
	case 125:			/* OSF/1 TIOCLSET */		
	case 126:			/* OSF/1 TIOCLBIC */		
	case 127:			/* OSF/1 TIOCLBIS */		
#endif
	case 19:			/* OSF/1 TIOCGETA (XXX) */
	case 20:			/* OSF/1 TIOCSETA (XXX) */
	case 21:			/* OSF/1 TIOCSETAW (XXX) */
	case 22:			/* OSF/1 TIOCSETAF (XXX) */
	case 26:			/* OSF/1 TIOCGETD (XXX) */
	case 27:			/* OSF/1 TIOCSETD (XXX) */
	case 97:			/* OSF/1 TIOCSCTTY */
	case 103:			/* OSF/1 TIOCSWINSZ */
	case 104:			/* OSF/1 TIOCGWINSZ */
	case 110:			/* OSF/1 TIOCSTART */
	case 111:			/* OSF/1 TIOCSTOP */
	case 118:			/* OSF/1 TIOCGPGRP */
	case 119:			/* OSF/1 TIOCGPGRP */
		/* same as in FreeBSD */
		break;


	default:
		printf("osf1_ioctl_t: cmd = %d\n", cmd);
		return (ENOTTY);
	}

	retval = ioctl(p, uap);
#if 0
	if (retval)
		printf("osf1_ioctl_t: cmd = %d, com = 0x%lx, retval = %d\n",
		       cmd, uap->com,retval);
#endif
	return retval;
}

/*
 * file locking ioctl's
 */

int
osf1_ioctl_f(p, uap, cmd, dir, len)
	struct proc *p;
	struct ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(int) com;
		syscallarg(caddr_t) data;
	} */ *uap;
	int cmd;
	int dir;
	int len;
{

	switch (cmd) {
	case 1:				/* OSF/1 FIOCLEX (XXX) */
	case 2:				/* OSF/1 FIONCLEX (XXX) */
	case 127:			/* OSF/1 FIONREAD (XXX) */
	case 126:			/* OSF/1 FIONREAD (XXX) */
	case 125:			/* OSF/1 FIOASYNC (XXX) */
	case 124:			/* OSF/1 FIOSETOWN (XXX) */
	case 123:			/* OSF/1 FIOGETOWN (XXX) */
		/* same as in FreeBSD */
		break;
		
	default:
		printf("osf1_ioctl_f: cmd = %d\n", cmd);
		return (ENOTTY);
	}

	return ioctl(p, uap);
}

/*
 * mag tape ioctl's
 */

int
osf1_ioctl_m(p, uap, cmd, dir, len)
	struct proc *p;
	struct ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(int) com;
		syscallarg(caddr_t) data;
	} */ *uap;
	int cmd;
	int dir;
	int len;
{

	switch (cmd) {
	case 1:				/* OSF/1 MTIOCTOP (XXX) */
	case 2:				/* OSF/1 MTIOCGET (XXX) */
		/* same as in FreeBSD */
		break;
		
	default:
		printf("osf1_ioctl_m: cmd = %d\n", cmd);
		return (ENOTTY);
	}

	return ioctl(p, uap);
}
