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
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/tty.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <compat/ibcs2/ibcs2_socksys.h>
#include <compat/ibcs2/ibcs2_util.h>

/*
 * iBCS2 socksys calls.
 */

struct ibcs2_socksys_args {
	int     fd;
	int     magic;
	caddr_t argsp;
};

int
ibcs2_socksys(p, uap, retval)
	register struct proc *p;
	register struct ibcs2_socksys_args *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	int error;
	int realargs[7]; /* 1 for command, 6 for recvfrom */
      
	/*
	 * SOCKET should only be legal on /dev/socksys.
	 * GETIPDOMAINNAME should only be legal on /dev/socksys ?
	 * The others are (and should be) only legal on sockets.
	 */

	if (error = copyin(uap->argsp, (caddr_t)realargs, sizeof(realargs)))
		return error;
	DPRINTF(("ibcs2_socksys: %08x %08x %08x %08x %08x %08x %08x\n",
	       realargs[0], realargs[1], realargs[2], realargs[3], 
	       realargs[4], realargs[5], realargs[6]));
	switch (realargs[0]) {
	case SOCKSYS_ACCEPT:
		return accept(p, realargs+1, retval);
	case SOCKSYS_BIND:
		return bind(p, realargs+1, retval);
	case SOCKSYS_CONNECT:
		return connect(p, realargs+1, retval);
	case SOCKSYS_GETPEERNAME:
		return getpeername(p, realargs+1, retval);
	case SOCKSYS_GETSOCKNAME:
		return getsockname(p, realargs+1, retval);
	case SOCKSYS_GETSOCKOPT:
		return getsockopt(p, realargs+1, retval);
	case SOCKSYS_LISTEN:
		return listen(p, realargs+1, retval);
	case SOCKSYS_RECV:
		realargs[5] = realargs[6] = 0;
		/* FALLTHROUGH */
	case SOCKSYS_RECVFROM:
		return recvfrom(p, realargs+1, retval);
	case SOCKSYS_SEND:
		realargs[5] = realargs[6] = 0;
		/* FALLTHROUGH */
	case SOCKSYS_SENDTO:
		return sendto(p, realargs+1, retval);
	case SOCKSYS_SETSOCKOPT:
		return setsockopt(p, realargs+1, retval);
	case SOCKSYS_SHUTDOWN:
		return shutdown(p, realargs+1, retval);
	case SOCKSYS_SOCKET:
		return socket(p, realargs+1, retval);
	case SOCKSYS_SELECT:
		return select(p, realargs+1, retval);
	case SOCKSYS_GETIPDOMAIN:
		return compat_09_getdomainname(p, realargs + 1, retval);
	case SOCKSYS_SETIPDOMAIN:
		return compat_09_setdomainname(p, realargs + 1, retval);
	case SOCKSYS_ADJTIME:
		return adjtime(p, realargs + 1, retval);
	case SOCKSYS_SETREUID:
		return compat_43_setreuid(p, realargs + 1, retval);
	case SOCKSYS_SETREGID:
		return compat_43_setregid(p, realargs + 1, retval);
	case SOCKSYS_GETTIME:
		return gettimeofday(p, realargs + 1, retval);
	case SOCKSYS_SETTIME:
		return settimeofday(p, realargs + 1, retval);
	case SOCKSYS_GETITIMER:
		return getitimer(p, realargs + 1, retval);
	case SOCKSYS_SETITIMER:
		return setitimer(p, realargs + 1, retval);

	default:
		printf("socksys unknown %08x %08x %08x %08x %08x %08x %08x\n",
                       realargs[0], realargs[1], realargs[2], realargs[3], 
                       realargs[4], realargs[5], realargs[6]);
		return EINVAL;
	}
	/* NOTREACHED */
}
