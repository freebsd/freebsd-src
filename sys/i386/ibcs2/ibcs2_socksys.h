/*-
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
 * $FreeBSD: src/sys/i386/ibcs2/ibcs2_socksys.h,v 1.9 2005/01/06 23:22:04 imp Exp $
 */

#ifndef	_I386_IBCS2_IBCS2_SOCKSYS_H_
#define	_I386_IBCS2_IBCS2_SOCKSYS_H_

#include <sys/ioccom.h>

#include <i386/ibcs2/ibcs2_types.h>

#define SOCKSYS_ACCEPT		1
#define SOCKSYS_BIND		2
#define SOCKSYS_CONNECT		3
#define SOCKSYS_GETPEERNAME	4
#define SOCKSYS_GETSOCKNAME	5
#define SOCKSYS_GETSOCKOPT	6
#define SOCKSYS_LISTEN		7
#define SOCKSYS_RECV		8
#define SOCKSYS_RECVFROM	9
#define SOCKSYS_SEND		10
#define SOCKSYS_SENDTO		11
#define SOCKSYS_SETSOCKOPT	12
#define SOCKSYS_SHUTDOWN	13
#define SOCKSYS_SOCKET		14
#define SOCKSYS_SELECT		15
#define SOCKSYS_GETIPDOMAIN	16
#define SOCKSYS_SETIPDOMAIN	17
#define SOCKSYS_ADJTIME		18
#define SOCKSYS_SETREUID	19
#define SOCKSYS_SETREGID	20
#define SOCKSYS_GETTIME		21
#define SOCKSYS_SETTIME		22
#define SOCKSYS_GETITIMER	23
#define SOCKSYS_SETITIMER	24

#define IBCS2_SIOCSHIWAT	_IOW('S', 1, int)	
#define IBCS2_SIOCGHIWAT	_IOR('S', 2, int)	
#define IBCS2_SIOCSLOWAT	_IOW('S', 3, int)	
#define IBCS2_SIOCGLOWAT	_IOR('S', 4, int)	
#define IBCS2_SIOCATMARK	_IOR('S', 5, int)	
#define IBCS2_SIOCSPGRP		_IOW('S', 6, int)	
#define IBCS2_SIOCGPGRP		_IOR('S', 7, int)	
#define IBCS2_FIONREAD		_IOR('S', 8, int)	
#define IBCS2_FIONBIO		_IOW('S', 9, int)	
#define IBCS2_FIOASYNC		_IOW('S', 10, int)	
#define IBCS2_SIOCPROTO		_IOW('S', 11, struct socknewproto)	
#define IBCS2_SIOCGETNAME	_IOR('S', 12, struct sockaddr)	
#define IBCS2_SIOCGETPEER	_IOR('S', 13, struct sockaddr)	
#define IBCS2_IF_UNITSEL	_IOW('S', 14, int)	
#define IBCS2_SIOCXPROTO	_IO('S', 15)	

#define IBCS2_SIOCADDRT		_IOW('R', 9, struct rtentry)	
#define IBCS2_SIOCDELRT		_IOW('R', 10, struct rtentry)	

#define IBCS2_SIOCSIFADDR	_IOW('I', 11, struct ifreq)	
#define IBCS2_SIOCGIFADDR	_IOWR('I', 12, struct ifreq)	
#define IBCS2_SIOCSIFDSTADDR	_IOW('I', 13, struct ifreq)	
#define IBCS2_SIOCGIFDSTADDR	_IOWR('I', 14, struct ifreq)	
#define IBCS2_SIOCSIFFLAGS	_IOW('I', 15, struct ifreq)	
#define IBCS2_SIOCGIFFLAGS	_IOWR('I', 16, struct ifreq)	
#define IBCS2_SIOCGIFCONF	_IOWR('I', 17, struct ifconf)	
#define IBCS2_SIOCSIFMTU	_IOW('I', 21, struct ifreq)	
#define IBCS2_SIOCGIFMTU	_IOWR('I', 22, struct ifreq)	
#define IBCS2_SIOCIFDETACH	_IOW('I', 26, struct ifreq)	
#define IBCS2_SIOCGENPSTATS	_IOWR('I', 27, struct ifreq)	
#define IBCS2_SIOCX25XMT	_IOWR('I', 29, struct ifreq)
#define IBCS2_SIOCX25RCV	_IOWR('I', 30, struct ifreq)
#define IBCS2_SIOCX25TBL	_IOWR('I', 31, struct ifreq)
#define IBCS2_SIOCGIFBRDADDR	_IOWR('I', 32, struct ifreq)	
#define IBCS2_SIOCSIFBRDADDR	_IOW('I', 33, struct ifreq)	
#define IBCS2_SIOCGIFNETMASK	_IOWR('I', 34, struct ifreq)	
#define IBCS2_SIOCSIFNETMASK	_IOW('I', 35, struct ifreq)	
#define IBCS2_SIOCGIFMETRIC	_IOWR('I', 36, struct ifreq)	
#define IBCS2_SIOCSIFMETRIC	_IOW('I', 37, struct ifreq)	
#define IBCS2_SIOCSARP		_IOW('I', 38, struct arpreq)	
#define IBCS2_SIOCGARP		_IOWR('I', 39, struct arpreq)	
#define IBCS2_SIOCDARP		_IOW('I', 40, struct arpreq)	
#define IBCS2_SIOCSIFNAME	_IOW('I', 41, struct ifreq)	
#define IBCS2_SIOCGIFONEP	_IOWR('I', 42, struct ifreq)	
#define IBCS2_SIOCSIFONEP	_IOW('I', 43, struct ifreq)	
#define IBCS2_SIOCGENADDR	_IOWR('I', 65, struct ifreq)	
#define IBCS2_SIOCSOCKSYS	_IOW('I', 66, struct socksysreq)	

struct socksysreq {
	int realargs[7];
};

struct socknewproto {
	int family;
	int type;
	int proto;
	ibcs2_dev_t dev;
	int flags;
};

struct ibcs2_socksys_args {
	int     fd;
	int     magic;
	caddr_t argsp;
};

int ibcs2_socksys(struct thread *, struct ibcs2_socksys_args *);

#endif /* !_I386_IBCS2_IBCS2_SOCKSYS_H_ */
