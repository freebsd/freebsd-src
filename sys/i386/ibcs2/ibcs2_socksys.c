/*
 * Copyright (c) 1994, 1995 Scott Bartram
 * Copyright (c) 1994 Arne H Juul
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <i386/ibcs2/ibcs2_socksys.h>
#include <i386/ibcs2/ibcs2_util.h>

/* Local structures */
struct getipdomainname_args {
        char    *ipdomainname;
        int     len;
};

struct setipdomainname_args {
        char    *ipdomainname;
        int     len;
};

/* Local prototypes */
static int ibcs2_getipdomainname(struct thread *,
				      struct getipdomainname_args *);
static int ibcs2_setipdomainname(struct thread *,
				      struct setipdomainname_args *);

/*
 * iBCS2 socksys calls.
 */

int
ibcs2_socksys(td, uap)
	register struct thread *td;
	register struct ibcs2_socksys_args *uap;
{
	int error;
	int realargs[7]; /* 1 for command, 6 for recvfrom */
	void *passargs;

	/*
	 * SOCKET should only be legal on /dev/socksys.
	 * GETIPDOMAINNAME should only be legal on /dev/socksys ?
	 * The others are (and should be) only legal on sockets.
	 */

	if ((error = copyin(uap->argsp, (caddr_t)realargs, sizeof(realargs))) != 0)
		return error;
	DPRINTF(("ibcs2_socksys: %08x %08x %08x %08x %08x %08x %08x\n",
	       realargs[0], realargs[1], realargs[2], realargs[3], 
	       realargs[4], realargs[5], realargs[6]));

	passargs = (void *)(realargs + 1);
	switch (realargs[0]) {
	case SOCKSYS_ACCEPT:
		return accept(td, passargs);
	case SOCKSYS_BIND:
		return bind(td, passargs);
	case SOCKSYS_CONNECT:
		return connect(td, passargs);
	case SOCKSYS_GETPEERNAME:
		return getpeername(td, passargs);
	case SOCKSYS_GETSOCKNAME:
		return getsockname(td, passargs);
	case SOCKSYS_GETSOCKOPT:
		return getsockopt(td, passargs);
	case SOCKSYS_LISTEN:
		return listen(td, passargs);
	case SOCKSYS_RECV:
		realargs[5] = realargs[6] = 0;
		/* FALLTHROUGH */
	case SOCKSYS_RECVFROM:
		return recvfrom(td, passargs);
	case SOCKSYS_SEND:
		realargs[5] = realargs[6] = 0;
		/* FALLTHROUGH */
	case SOCKSYS_SENDTO:
		return sendto(td, passargs);
	case SOCKSYS_SETSOCKOPT:
		return setsockopt(td, passargs);
	case SOCKSYS_SHUTDOWN:
		return shutdown(td, passargs);
	case SOCKSYS_SOCKET:
		return socket(td, passargs);
	case SOCKSYS_SELECT:
		return select(td, passargs);
	case SOCKSYS_GETIPDOMAIN:
		return ibcs2_getipdomainname(td, passargs);
	case SOCKSYS_SETIPDOMAIN:
		return ibcs2_setipdomainname(td, passargs);
	case SOCKSYS_ADJTIME:
		return adjtime(td, passargs);
	case SOCKSYS_SETREUID:
		return setreuid(td, passargs);
	case SOCKSYS_SETREGID:
		return setregid(td, passargs);
	case SOCKSYS_GETTIME:
		return gettimeofday(td, passargs);
	case SOCKSYS_SETTIME:
		return settimeofday(td, passargs);
	case SOCKSYS_GETITIMER:
		return getitimer(td, passargs);
	case SOCKSYS_SETITIMER:
		return setitimer(td, passargs);

	default:
		printf("socksys unknown %08x %08x %08x %08x %08x %08x %08x\n",
                       realargs[0], realargs[1], realargs[2], realargs[3], 
                       realargs[4], realargs[5], realargs[6]);
		return EINVAL;
	}
	/* NOTREACHED */
}

/* ARGSUSED */
static int
ibcs2_getipdomainname(td, uap)
        struct thread *td;
        struct getipdomainname_args *uap;
{
	char hname[MAXHOSTNAMELEN], *dptr;
	int len;

	/* Get the domain name */
	getcredhostname(td->td_ucred, hname, sizeof(hname));

	dptr = index(hname, '.');
	if ( dptr )
		dptr++;
	else
		/* Make it effectively an empty string */
		dptr = hname + strlen(hname);
	
	len = strlen(dptr) + 1;
	if ((u_int)uap->len > len + 1)
		uap->len = len + 1;
	return (copyout((caddr_t)dptr, (caddr_t)uap->ipdomainname, uap->len));
}

/* ARGSUSED */
static int
ibcs2_setipdomainname(td, uap)
        struct thread *td;
        struct setipdomainname_args *uap;
{
	char hname[MAXHOSTNAMELEN], *ptr;
	int error, sctl[2], hlen;

	if ((error = suser(td)))
		return (error);

	/* W/out a hostname a domain-name is nonsense */
	if ( strlen(hostname) == 0 )
		return EINVAL;

	/* Get the host's unqualified name (strip off the domain) */
	snprintf(hname, sizeof(hname), "%s", hostname);
	ptr = index(hname, '.');
	if ( ptr != NULL ) {
		ptr++;
		*ptr = '\0';
	} else
		strcat(hname, ".");

	/* Set ptr to the end of the string so we can append to it */
	hlen = strlen(hname);
	ptr = hname + hlen;
        if ((u_int)uap->len > (sizeof (hname) - hlen - 1))
                return EINVAL;

	/* Append the ipdomain to the end */
	error = copyin((caddr_t)uap->ipdomainname, ptr, uap->len);
	if (error)
		return (error);

	/* 'sethostname' with the new information */
	sctl[0] = CTL_KERN;
        sctl[1] = KERN_HOSTNAME;
 	hlen = strlen(hname) + 1;
        return (kernel_sysctl(td, sctl, 2, 0, 0, hname, hlen, 0));
}
