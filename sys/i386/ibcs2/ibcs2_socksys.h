/*-
 * Copyright (c) 1994 Mostyn Lewis
 * All rights reserved.
 *
 * This software is based on code which is:
 * Copyright (c) 1994  Mike Jagdis (jaggy@purplet.demon.co.uk)
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
 *    derived from this software withough specific prior written permission
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
 *	$Id: ibcs2_socksys.h,v 1.2 1994/10/13 23:10:58 sos Exp $
 */

#define	SS_DEBUG

struct ss_call {
	int	arg[7];
};

/* Alien socket */
struct alien_sockaddr {
	unsigned short	sa_family;	/* address family, AF_xxx       */
	char		sa_data[14];	/* 14 bytes of protocol address */
};

struct alien_in_addr {
        unsigned long int	s_addr;
};

#define	__ALIEN_SOCK_SIZE__	16	/* sizeof(struct alien_sockaddr)*/
struct alien_sockaddr_in { 
  short int		sin_family;	/* Address family               */
  unsigned short int	sin_port;	/* Port number                  */
  struct alien_in_addr	sin_addr;	/* Internet address             */ 
  unsigned char         __filling[__ALIEN_SOCK_SIZE__ - sizeof(short int) -
                        sizeof(unsigned short int) - sizeof(struct alien_in_addr)];
};

struct sgdomarg {
	char *name;
	int namelen;
};

struct lstatarg {
	char *fname;
	void *statb;
};

struct socknewproto {
	int	family;	/* address family (AF_INET, etc.) */
	int	type;	/* protocol type (SOCK_STREAM, etc.) */
	int	proto;	/* per family proto number */
	dev_t	dev;	/* major/minor to use (must be a clone) */
	int	flags;	/* protosw flags */
};

/* System type ordinals */
#define	SS_FREEBSD	0
#define	SS_SYSVR4	1
#define	SS_SYSVR3	2
#define	SS_SCO_32	3
#define	SS_WYSE_321	4
#define	SS_ISC		5
#define	SS_LINUX	6


/* Socksys macros */
#define	IOCTL(cmd) \
  if(error = ss_IOCTL(fp, cmd, arg, p))\
	return(error);
#define	SYSCALL(number,conv_arg,indicator) \
  if(error = ss_SYSCALL(number,conv_arg,indicator,arg,p,retval))\
	return(error);
#define	SYSCALL_N(number,conv_arg,indicator) \
  arg = (caddr_t)(((int *)arg) - 1);\
  if(error = ss_SYSCALL(number,conv_arg,indicator,arg,p,retval))\
	return(error);
#define	SYSCALLX(number,arg)	(*sysent[number].sy_call)(p, (caddr_t)arg, retval)
#define	SYSCALL_RETURN(number)	SYSCALL(number) ; IBCS2_MAGIC_RETURN

/* Socksys commands */
#define  CMD_SO_ACCEPT		1
#define  CMD_SO_BIND		2
#define  CMD_SO_CONNECT		3
#define  CMD_SO_GETPEERNAME	4
#define  CMD_SO_GETSOCKNAME	5
#define  CMD_SO_GETSOCKOPT	6
#define  CMD_SO_LISTEN		7
#define  CMD_SO_RECV		8
#define  CMD_SO_RECVFROM	9
#define  CMD_SO_SEND		10
#define  CMD_SO_SENDTO		11
#define  CMD_SO_SETSOCKOPT	12
#define  CMD_SO_SHUTDOWN	13
#define  CMD_SO_SOCKET		14
#define  CMD_SO_SELECT		15
#define  CMD_SO_GETIPDOMAIN	16
#define  CMD_SO_SETIPDOMAIN	17
#define  CMD_SO_ADJTIME		18
#define  CMD_SO_SETREUID	19
#define  CMD_SO_SETREGID	20
#define  CMD_SO_GETTIME		21
#define  CMD_SO_SETTIME		22
#define  CMD_SO_GETITIMER	23
#define  CMD_SO_SETITIMER	24

#define  CMD_SO_SS_DEBUG	255

/* socksys ioctls */
#define	SS_IOCPARM_MASK		0x7f		/* parameters must be < 128 bytes */
#define	SS_IOC_VOID		0x20000000	/* no parameters */
#define	SS_IOC_OUT		0x40000000	/* copy out parameters */
#define	SS_IOC_IN		0x80000000	/* copy in parameters */
#define	SS_IOC_INOUT		(SS_IOC_IN|SS_IOC_OUT)

#define	SS_IO(x,y)		(SS_IOC_VOID|(x<<8)|y)
#define	SS_IOR(x,y,t)		(SS_IOC_OUT|((sizeof(t)&SS_IOCPARM_MASK)<<16)|(x<<8)|y)
#define	SS_IOW(x,y,t)		(SS_IOC_IN|((sizeof(t)&SS_IOCPARM_MASK)<<16)|(x<<8)|y)
#define	SS_IOWR(x,y,t)		(SS_IOC_INOUT|((sizeof(t)&SS_IOCPARM_MASK)<<16)|(x<<8)|y)

#define SS_SIOCSHIWAT		SS_IOW ('S', 1, int) /* set high watermark */
#define SS_SIOCGHIWAT		SS_IOR ('S', 2, int) /* get high watermark */
#define SS_SIOCSLOWAT		SS_IOW ('S', 3, int) /* set low watermark */
#define SS_SIOCGLOWAT		SS_IOR ('S', 4, int) /* get low watermark */
#define SS_SIOCATMARK		SS_IOR ('S', 5, int) /* at oob mark? */
#define SS_SIOCSPGRP		SS_IOW ('S', 6, int) /* set process group */
#define SS_SIOCGPGRP		SS_IOR ('S', 7, int) /* get process group */
#define SS_FIONREAD		SS_IOR ('S', 8, int)
#define SS_FIONBIO		SS_IOW ('S', 9, int)
#define SS_FIOASYNC		SS_IOW ('S', 10, int)
#define SS_SIOCPROTO		SS_IOW ('S', 11, struct socknewproto) /* link proto */
#define SS_SIOCGETNAME		SS_IOR ('S', 12, struct sockaddr) /* getsockname */
#define SS_SIOCGETPEER		SS_IOR ('S', 13,struct sockaddr) /* getpeername */
#define SS_IF_UNITSEL		SS_IOW ('S', 14, int)/* set unit number */
#define SS_SIOCXPROTO		SS_IO  ('S', 15)     /* empty proto table */

#define	SS_SIOCADDRT		SS_IOW ('R', 9, struct ortentry) /* add route */
#define	SS_SIOCDELRT		SS_IOW ('R', 10, struct ortentry)/* delete route */

#define	SS_SIOCSIFADDR		SS_IOW ('I', 11, struct ifreq)/* set ifnet address */
#define	SS_SIOCGIFADDR		SS_IOWR('I', 12, struct ifreq)/* get ifnet address */
#define	SS_SIOCSIFDSTADDR	SS_IOW ('I', 13, struct ifreq)/* set p-p address */
#define	SS_SIOCGIFDSTADDR	SS_IOWR('I', 14,struct ifreq) /* get p-p address */
#define	SS_SIOCSIFFLAGS		SS_IOW ('I', 15, struct ifreq)/* set ifnet flags */
#define	SS_SIOCGIFFLAGS		SS_IOWR('I', 16, struct ifreq)/* get ifnet flags */
#define	SS_SIOCGIFCONF		SS_IOWR('I', 17, struct ifconf)/* get ifnet list */

#define	SS_SIOCSIFMTU		SS_IOW ('I', 21, struct ifreq)/* get if_mtu */
#define	SS_SIOCGIFMTU		SS_IOWR('I', 22, struct ifreq)/* set if_mtu */

#define SS_SIOCIFDETACH		SS_IOW ('I', 26, struct ifreq)/* detach interface */
#define SS_SIOCGENPSTATS	SS_IOWR('I', 27, struct ifreq)/* get ENP stats */

#define SS_SIOCX25XMT		SS_IOWR('I', 29, struct ifreq)/* start a slp proc in x25if */
#define SS_SIOCX25RCV		SS_IOWR('I', 30, struct ifreq)/* start a slp proc in x25if */
#define SS_SIOCX25TBL		SS_IOWR('I', 31, struct ifreq)/* xfer lun table to kernel */

#define	SS_SIOCGIFBRDADDR	SS_IOWR('I', 32, struct ifreq)/* get broadcast addr */
#define	SS_SIOCSIFBRDADDR	SS_IOW ('I', 33, struct ifreq)/* set broadcast addr */
#define	SS_SIOCGIFNETMASK	SS_IOWR('I', 34, struct ifreq)/* get net addr mask */
#define	SS_SIOCSIFNETMASK	SS_IOW ('I', 35, struct ifreq)/* set net addr mask */
#define	SS_SIOCGIFMETRIC	SS_IOWR('I', 36, struct ifreq)/* get IF metric */
#define	SS_SIOCSIFMETRIC	SS_IOW ('I', 37, struct ifreq)/* set IF metric */

#define	SS_SIOCSARP		SS_IOW ('I', 38, struct arpreq)/* set arp entry */
#define	SS_SIOCGARP		SS_IOWR('I', 39, struct arpreq)/* get arp entry */
#define	SS_SIOCDARP		SS_IOW ('I', 40, struct arpreq)/* delete arp entry */

#define SS_SIOCSIFNAME		SS_IOW ('I', 41, struct ifreq)/* set interface name */
#define	SS_SIOCGIFONEP		SS_IOWR('I', 42, struct ifreq)/* get 1-packet parms */
#define	SS_SIOCSIFONEP		SS_IOW ('I', 43, struct ifreq)/* set 1-packet parms */

#define SS_SIOCGENADDR		SS_IOWR('I', 65, struct ifreq)/* Get ethernet addr */

#define SS_SIOCSOCKSYS		SS_IOW ('I', 66, struct ss_call)/* ss syscall */


/*
 *	NFS/NIS has a pseudo device called /dev/nfsd which may accept ioctl
 *	calls. /dev/nfsd is linked to /dev/socksys.
 */

#define NIOCNFSD	1
#define NIOCOLDGETFH	2
#define NIOCASYNCD	3
#define NIOCSETDOMNAM	4
#define NIOCGETDOMNAM	5
#define NIOCCLNTHAND	6
#define NIOCEXPORTFS	7
#define NIOCGETFH	8
#define NIOCLSTAT	9


/*
 *	noso
 */

#define	SO_ORDREL	0xff02
#define	SO_IMASOCKET	0xff03
#define	SO_PROTOTYPE	0xff04
/* Check below */
#define SO_NO_CHECK	11
#define SO_PRIORITY	12

/*
 *	convert
 */

/* Structure conversion indicators */

#define	SS_STRUCT_ARPREQ	1
#define	SS_STRUCT_IFCONF	2
#define	SS_STRUCT_IFREQ		3
#define	SS_STRUCT_ORTENTRY	4
#define	SS_STRUCT_SOCKADDR	5
#define	SS_STRUCT_SOCKNEWPROTO	6

#define	SS_ALIEN_TO_NATIVE	1
#define	SS_NATIVE_TO_ALIEN	2

struct whatever {
	int from, to;
	unsigned char *conversion;
	unsigned char all_the_same;
	struct whatever *more;
};


extern struct whatever *af_whatevers[];
extern struct whatever *type_whatevers[];
extern struct whatever *sopt_whatevers[];
extern struct whatever *struct_whatevers[];

extern int ss_convert(struct whatever **what, int *this, int otherwise);
extern int ss_convert_struct(char *alien, int indicator, int direction);

/*
 *	convert af
 */


static struct whatever af_whatevers_all[] =  {
	{ 0, 2, NULL, 0, 0 },
	{ -1 }
};


struct whatever *af_whatevers[] = {
	NULL,			/* FreeBSD */
	af_whatevers_all,	/* SysVR4 */
	af_whatevers_all,	/* SysVR3 */
	af_whatevers_all,	/* SCO 3.2.[24] */
	af_whatevers_all,	/* Wyse Unix V/386 3.2.1 */
	af_whatevers_all,	/* ISC */
	af_whatevers_all	/* Linux */
};

/*
 *	convert sopt
 */

static struct whatever sopt_whatevers_all[] =  {
	{ 0x0001, 0x0001, (char *)SO_DEBUG, 0, 0 },
	{ 0x0002, 0x0002, (char *)SO_ACCEPTCONN, 0, 0 },
	{ 0x0004, 0x0004, (char *)SO_REUSEADDR, 0, 0 },
	{ 0x0008, 0x0008, (char *)SO_KEEPALIVE, 0, 0 },
	{ 0x0010, 0x0010, (char *)SO_DONTROUTE, 0, 0 },
	{ 0x0020, 0x0020, (char *)SO_BROADCAST, 0, 0 },
	{ 0x0040, 0x0040, (char *)SO_USELOOPBACK, 0, 0 },
	{ 0x0080, 0x0080, (char *)SO_LINGER, 0, 0 },
	{ 0x0100, 0x0100, (char *)SO_OOBINLINE, 0, 0 },
	{ 0x0200, 0x0200, (char *)SO_ORDREL, 0, 0 },
	{ 0x0400, 0x0400, (char *)SO_IMASOCKET, 0, 0 },
	{ 0x1001, 0x1001, (char *)SO_SNDBUF, 0, 0 },
	{ 0x1002, 0x1001, (char *)SO_RCVBUF, 0, 0 },
	{ 0x1003, 0x1001, (char *)SO_SNDLOWAT, 0, 0 },
	{ 0x1004, 0x1001, (char *)SO_RCVLOWAT, 0, 0 },
	{ 0x1005, 0x1001, (char *)SO_SNDTIMEO, 0, 0 },
	{ 0x1006, 0x1001, (char *)SO_RCVTIMEO, 0, 0 },
	{ 0x1007, 0x1001, (char *)SO_ERROR, 0, 0 },
	{ 0x1008, 0x1001, (char *)SO_TYPE, 0, 0 },
	{ 0x1009, 0x1001, (char *)SO_PROTOTYPE, 0, 0 },
	{ -1 }
};


struct whatever *sopt_whatevers[] = {
	NULL,			/* FreeBSD */
	sopt_whatevers_all,	/* SysVR4 */
	sopt_whatevers_all,	/* SysVR3 */
	sopt_whatevers_all,	/* SCO 3.2.[24] */
	sopt_whatevers_all,	/* Wyse Unix V/386 3.2.1 */
	sopt_whatevers_all,	/* ISC */
	sopt_whatevers_all	/* Linux */
};

/*
 *	convert struct
 */

static struct whatever struct_whatever_typeI_ranges[] = {
	{ 11, 16, (char *)SS_STRUCT_IFREQ	, 1, 0 }, /* OK */
	{ 17, 17, (char *)SS_STRUCT_IFCONF	, 1, 0 }, /* OK */
	{ 21, 22, (char *)SS_STRUCT_IFREQ	, 1, 0 }, /* SIZE OK */
	{ 26, 27, (char *)SS_STRUCT_IFREQ	, 1, 0 }, /* SIZE OK */
	{ 29, 37, (char *)SS_STRUCT_IFREQ	, 1, 0 }, /* SIZE OK */
	{ 38, 40, (char *)SS_STRUCT_ARPREQ	, 1, 0 }, /* OK */
	{ 41, 43, (char *)SS_STRUCT_IFREQ	, 1, 0 }, /* SIZE OK */
	{ 65, 65, (char *)SS_STRUCT_IFREQ	, 1, 0 }, /* SIZE OK */
	{ -1 }
};

static struct whatever struct_whatever_typeR_ranges[] = {
	{ 9,  10, (char *)SS_STRUCT_ORTENTRY	, 1, 0 }, /* SIZE OK */
	{ -1 }
};

static struct whatever struct_whatever_typeS_ranges[] = {
	{ 1,  10, 0				, 1, 0 },
	{ 11, 11, (char *)SS_STRUCT_SOCKNEWPROTO, 1, 0 }, /* NO SUPPORT */
	{ 12, 13, (char *)SS_STRUCT_SOCKADDR	, 1, 0 }, /* len and family */
	{ 14, 15, 0				, 1, 0 },
	{ -1 }
};

static struct whatever struct_whatevers_all[] = {
	{ 'I', 'I', 0, 0, struct_whatever_typeI_ranges },
	{ 'R', 'R', 0, 0, struct_whatever_typeR_ranges },
	{ 'S', 'S', 0, 0, struct_whatever_typeS_ranges },
	{ -1 }
};

struct whatever *struct_whatevers[] = {
	struct_whatevers_all,	/* FreeBSD */
	struct_whatevers_all,	/* SysVR4 */
	struct_whatevers_all,	/* SysVR3 */
	struct_whatevers_all,	/* SCO 3.2.[24] */
	struct_whatevers_all,	/* Wyse Unix V/386 3.2.1 */
	struct_whatevers_all,	/* ISC */
	struct_whatevers_all	/* Linux */
};

int ss_struct_native_sizes[] = {
	sizeof(struct arpreq),
	sizeof(struct ifconf),
	sizeof(struct ifreq),
	sizeof(struct rtentry),
	sizeof(struct sockaddr),
	sizeof(struct socknewproto)
};

/*
 *	convert type
 */

static char type_conversion_SysVr4_range1[] = {
	SOCK_DGRAM,
	SOCK_STREAM,
	0,
	SOCK_RAW,
	SOCK_RDM,
	SOCK_SEQPACKET
};

static struct whatever type_whatevers_SysVr4[] = {
	{ 1, 6, type_conversion_SysVr4_range1, 0 },
	{ -1 }
};

struct whatever *type_whatevers[] = {
	NULL,			/* FreeBSD */
	type_whatevers_SysVr4,	/* SysVR4 */
	NULL,			/* SysVR3 */
	NULL,			/* SCO 3.2.[24] */
	NULL,			/* Wyse Unix V/386 3.2.1 */
	NULL,			/* ISC */
	NULL			/* Linux */
};
