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
 *	$Id: ibcs2_socksys.c,v 1.1 1994/10/14 08:53:08 sos Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/malloc.h>
#include <sys/un.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <netinet/in.h>
#include <vm/vm.h>
#include <i386/ibcs2/ibcs2.h>
#include <i386/ibcs2/ibcs2_socksys.h>

/* Socksys pseudo driver entry points */

int sockopen (dev_t dev, int mode, int devtype, struct proc *p);
int sockioctl(dev_t dev, int cmd, caddr_t arg, int fflag, struct proc *p);
int sockclose(dev_t dev, int flag, int mode, struct proc *p);

/* Socksys internal functions */

static void put_socket_fops(struct proc *p, int fd);
static int  ss_fop_close(struct file *fp, struct proc *p);
static int  ss_fop_ioctl(struct file*fp, int cmd, caddr_t arg, struct proc *p);
static int  ss_syscall(caddr_t arg, struct proc *p);

/*
 *	This structure is setup on first usage. Its address is planted
 *	into a socket's file structure fileops pointer after a successful
 *	socket creation or accept.
 */
static struct fileops ss_socket_fops = {
	NULL,	/* normal socket read */
	NULL,	/* normal socket write */
	NULL,	/* socksys ioctl */
	NULL,	/* normal socket select */
	NULL,	/* socksys close */
};

static int (*close_s)__P((struct file *fp, struct proc *p));
static int (*ioctl_s)__P((struct file *fp, int cmd, caddr_t data, struct proc *p));

int	ss_debug = 10;

static int
ss_syscall(arg, p)
	caddr_t arg;
	struct proc *p;
{
	int cmd;
	int error;
	int retval[2];

	retval[0] = retval[1] = 0;
	cmd = ((struct ss_call *)arg)->arg[0];

	if(ss_debug) {
	static char *ss_syscall_strings[] = {
		"0?", "accept", "bind", "connect", "getpeername",
		"getsockname", "getsockopt", "listen", "recv(from)",
		"recvfrom", "send(to)", "sendto", "setsockopt", "shutdown",
		"socket", "select", "getipdomain", "setipdomain",
		"adjtime", "setreuid", "setregid", "gettimeofday",
		"settimeofday", "getitimer", "setitimer",
	};

	printf("ss_syscall: [%d] ",p->p_pid);
	if(cmd < 0 || (cmd > CMD_SO_SETITIMER && cmd != CMD_SO_SS_DEBUG) )
		printf("? ");
	else {
		if(cmd == CMD_SO_SS_DEBUG)
			printf("%s ","ss_debug");
		else 
			printf("%s ",ss_syscall_strings[cmd]);
	}
	printf("(%d) <0x%x,0x%x,0x%x,0x%x,0x%x,0x%x>\n",
		cmd,
		((struct ss_call *)arg)->arg[1],
		((struct ss_call *)arg)->arg[2],
		((struct ss_call *)arg)->arg[3],
		((struct ss_call *)arg)->arg[4],
		((struct ss_call *)arg)->arg[5],
		((struct ss_call *)arg)->arg[6]);
	}

	error = 0;

	switch (cmd) {

	case CMD_SO_SS_DEBUG:

		/* ss_debug = ((struct ss_call *)arg)->arg[1]; */
		break;

	case CMD_SO_SOCKET: { /* NO CONV */

		if(ss_debug > 1)
			printf("SO_SOCKET af in %d\n",
				((struct ss_call *)arg)->arg[1]);
		((struct ss_call *)arg)->arg[1] = ss_convert(
			af_whatevers,
			&(((struct ss_call *)arg)->arg[1]),
			0);
		if(ss_debug > 1) {
			printf("SO_SOCKET af out %d\n",
				((struct ss_call *)arg)->arg[1]);

			printf("SO_SOCKET type in %d\n",
				((struct ss_call *)arg)->arg[2]);
		}
		((struct ss_call *)arg)->arg[2] = ss_convert(
			type_whatevers,
			&(((struct ss_call *)arg)->arg[2]),
			0);
		if(ss_debug > 1)
			printf("SO_SOCKET type out %d\n",
				((struct ss_call *)arg)->arg[2]);

		SYSCALL(SYS_socket, 0, 0);

		if(ss_debug)
			printf("ss_syscall: [%d] socket fd=%d\n",
				p->p_pid, retval[0]);
		put_socket_fops(p,retval[0]);

		break;
	}

	case CMD_SO_ACCEPT: { /* CONVERSION in arg 2 */

		SYSCALL(SYS_accept, 2, SS_STRUCT_SOCKADDR);

		if(ss_debug)
			printf("ss_syscall: [%d] accept fd=%d\n",
				p->p_pid, retval[0]);
		put_socket_fops(p,retval[0]);

		break;
	}

	case CMD_SO_BIND:
		SYSCALL(SYS_bind, 2, SS_STRUCT_SOCKADDR);
		break;

	case CMD_SO_CONNECT: {
		struct alien_sockaddr *sa;
		unsigned short family;

		/* Remap any INADDR_ANY (0.0.0.0) to localhost */

		sa = (struct alien_sockaddr *)((struct ss_call *)arg)->arg[1];
		if(error = copyin((caddr_t)&sa->sa_family,
					(caddr_t)&family, sizeof(short)))
			return(error);
		if (family == AF_INET) {
			unsigned long *addr;
			unsigned long saddr;

			addr = &(((struct alien_sockaddr_in *)sa)->sin_addr.s_addr);
			if(error = copyin((caddr_t)addr, (caddr_t)&saddr, sizeof(long)))
				return(error);
			if (saddr == INADDR_ANY) {
				/* 0x0100007f is 127.0.0.1 reversed */
				saddr = 0x0100007f;
				if(error = copyout((caddr_t)&saddr,
						(caddr_t)addr, sizeof(long)))
					return(error);
				if (ss_debug)
					printf("ss_syscall: remapped INADDR_ANY to localhost\n");
			}
		}
		SYSCALL(SYS_connect, 2, SS_STRUCT_SOCKADDR);
		break;
	}

	case CMD_SO_GETPEERNAME:
		SYSCALL(SYS_getpeername, 2, SS_STRUCT_SOCKADDR);
		break;

	case CMD_SO_GETSOCKNAME:
		SYSCALL(SYS_getsockname, 2, SS_STRUCT_SOCKADDR);
		break;

	case CMD_SO_GETSOCKOPT:
		if(error = ss_getsockopt((caddr_t)(((int *)arg) + 1),retval,p))
			return(error);
		break;

	case CMD_SO_LISTEN:
		SYSCALL(SYS_listen, 0, 0);
		break;

	case CMD_SO_RECV:
		((struct ss_call *)arg)->arg[5] = (int)((struct sockaddr *)NULL);
		((struct ss_call *)arg)->arg[6] = 0;
		SYSCALL(SYS_recvfrom, 0, 0);
		break;

	case CMD_SO_RECVFROM:
		SYSCALL(SYS_recvfrom, 5, SS_STRUCT_SOCKADDR);
		break;

	case CMD_SO_SEND:
		((struct ss_call *)arg)->arg[5] = (int)((struct sockaddr *)NULL);
		((struct ss_call *)arg)->arg[6] = 0;
		SYSCALL(SYS_sendto, 0, 0);
		break;

	case CMD_SO_SENDTO:
		SYSCALL(SYS_sendto, 5, SS_STRUCT_SOCKADDR);
		break;

	case CMD_SO_SETSOCKOPT:
		if(error = ss_setsockopt((caddr_t)(((int *)arg) + 1),retval,p))
			return(error);

	case CMD_SO_SHUTDOWN:
		SYSCALL(SYS_shutdown, 0, 0);
		break;

	case CMD_SO_GETIPDOMAIN:
		SYSCALL(SYS_getdomainname, 0, 0);
		break;

	case CMD_SO_SETIPDOMAIN: /* Note check on BSD utsname no change? */
		SYSCALL(SYS_setdomainname, 0, 0);
		break;

	case CMD_SO_SETREUID:
		SYSCALL(126/*SYS_setreuid*/, 0, 0);
		break;

	case CMD_SO_SETREGID:
		SYSCALL(127/*SYS_setregid*/, 0, 0);
		break;

	case CMD_SO_GETTIME:
		SYSCALL(SYS_gettimeofday, 0, 0);
		break;

	case CMD_SO_SETTIME:
		SYSCALL(SYS_settimeofday, 0, 0);
		break;

	case CMD_SO_GETITIMER:
		SYSCALL(SYS_getitimer, 0, 0);
		break;

	case CMD_SO_SETITIMER:
		SYSCALL(SYS_setitimer, 0, 0);
		break;

	case CMD_SO_SELECT:
		SYSCALL(SYS_select, 0, 0);
		break;

	case CMD_SO_ADJTIME:
		SYSCALL(SYS_adjtime, 0, 0);
		break;

	default:
		printf("ss_syscall: default 0x%x\n",cmd);
		return (EINVAL);
	}
	IBCS2_MAGIC_RETURN(arg);
}


static int
ss_fop_ioctl(fp, cmd, arg, p)
	struct file *fp;
	int cmd;
	caddr_t arg;
	struct proc *p;
{
	int error;
	int retval[2];

	if(ss_debug) {
	static char **ioctl_strings;
	int	fd;
	struct filedesc *fdp;
	unsigned int ioctl_type;
	unsigned int ioctl_len;
	char	cmd_type;
	int	cmd_ordinal;

	static char *ioctl_type_strings[] = {
		"0?", "SS_IO", "SS_IOR", "3?", "SS_IOW", "5?", "SS_IOWR"
	};
	static char *ioctl_S_strings[] = {
		"0?", "SIOCSHIWAT", "SIOCGHIWAT", "SIOCSLOWAT", "SIOCGLOWAT",
		"SIOCATMARK", "SIOCSPGRP", "SIOCGPGRP", "FIONREAD",
		"FIONBIO", "FIOASYNC", "SIOCPROTO", "SIOCGETNAME",
		"SIOCGETPEER", "IF_UNITSEL", "SIOCXPROTO"
	};
	static char *ioctl_R_strings[] = {
		"0?", "1?", "2?", "3?", "4?", "5?", "6?", "7?", "8?",
		"SIOCADDRT", "SIOCDELRT"
	};
	static char *ioctl_I_strings[] = {
		"0?", "1?", "2?", "3?", "4?", "5?", "6?", "7?", "8?",
		"9?", "10?", "SIOCSIFADDR", "SIOCGIFADDR", "SIOCSIFDSTADDR",
		"SIOCGIFDSTADDR", "SIOCSIFFLAGS", "SIOCGIFFLAGS",
		"SIOCGIFCONF", "18?", "19?", "20?", "SIOCSIFMTU",
		"SIOCGIFMTU", "23?", "24?", "25?", "SIOCIFDETACH",
		"SIOCGENPSTATS", "28?", "SIOCX25XMT", "SS_SIOCX25RCV",
		"SS_SIOCX25TBL", "SIOCGIFBRDADDR" ,"SIOCSIFBRDADDR",
		"SIOCGIFNETMASK", "SIOCSIFNETMASK", "SIOCGIFMETRIC",
		"SIOCSIFMETRIC", "SIOCSARP", "SIOCGARP", "SIOCDARP",
		"SIOCSIFNAME", "SIOCGIFONEP", "SIOCSIFONEP ",
		"44?", "45?", "46?", "47?", "48?", "49?", "50?", "51?",
		"52?", "53?", "54?", "55?", "56?", "57?", "58?", "59?",
		"60?", "61?", "62?", "63?", "64?", "SIOCGENADDR",
		"SIOCSOCKSYS"
	};

	cmd_type = (cmd >> 8) & 0xff;
	cmd_ordinal = cmd & 0xff;

	switch (cmd_type) {

	case 'S':
		ioctl_strings = ioctl_S_strings;
		if (cmd_ordinal > 15)
			cmd_ordinal = -1;
		break;

	case 'R':
		ioctl_strings = ioctl_R_strings;
		if (cmd_ordinal > 10)
			cmd_ordinal = -1;
		break;

	case 'I':
		ioctl_strings = ioctl_I_strings;
		if (cmd_ordinal > 66)
			cmd_ordinal = -1;
		break;

	default:
		cmd_type = '?';
		break;
	}
	fdp = p->p_fd;
	fd = -1;
	while(++fd < NOFILE)
		if ( fp == fdp->fd_ofiles[fd] )
			break;

	ioctl_type = (0xe0000000 & cmd) >> 29;
	ioctl_len = (cmd >> 16) & SS_IOCPARM_MASK;

	printf("ss_fop_ioctl: [%d] fd=%d ",p->p_pid, fd);
	if(cmd_type != '?'){
		if(cmd_ordinal != -1)
			printf("%s %s('%c',%d,l=%d) ",ioctl_strings[cmd_ordinal],
				ioctl_type_strings[ioctl_type],
				cmd_type,
				cmd_ordinal,
				ioctl_len);
		else {
			cmd_ordinal = cmd & 0xff;
			printf("[unknown ordinal %d] %s('%c',%d,l=%d) ",cmd_ordinal,
				ioctl_type_strings[ioctl_type],
				cmd_type,
				cmd_ordinal,
				ioctl_len);
		}
	}
	else {
		printf("? %s('%c',%d,l=%d) ",
			ioctl_type_strings[ioctl_type],
			cmd_type,
			cmd_ordinal,
			ioctl_len);
	}

	printf("0x%x (0x%x) <0x%x>\n",
		fp, cmd, arg);
	}

	/* No dogs allowed */

	if(*(((int *)arg) - 3) != IBCS2_MAGIC_IN){
		printf("ss_fop_ioctl: bad magic (sys_generic.c has no socksys mods?)\n");
		return(EINVAL);
	}

	if(fp->f_type != DTYPE_SOCKET)
		return (ENOTSOCK);

	retval[0] = retval[1] = 0;


	error = 0;

	switch (cmd) {
	case SS_SIOCSOCKSYS:		/* ss syscall */
		return ss_syscall(arg, p);

	case SS_SIOCSHIWAT:	/* set high watermark */
	case SS_SIOCSLOWAT:	/* set low watermark */
		break;		/* return value of 0 and no error */

	case SS_SIOCGHIWAT:	/* get high watermark */
	case SS_SIOCGLOWAT: 	/* get low watermark */
		break;		/* return value of 0 and no error */

	case SS_SIOCATMARK:	/* at oob mark */
		IOCTL(SIOCATMARK);
		break;

	case SS_SIOCSPGRP:	/* set process group */
		IOCTL(SIOCSPGRP);
		break;
	case SS_SIOCGPGRP:	/* get process group */
		IOCTL(SIOCGPGRP);
		break;

	case FIONREAD:
	case SS_FIONREAD:	/* get # bytes to read */
		IOCTL(FIONREAD);
		break;

	case SS_FIONBIO:	/* set/clear non-blocking i/o */
		IOCTL(FIONBIO);
		break;

	case SS_FIOASYNC:	/* set/clear async i/o */
		IOCTL(FIOASYNC);
		break;

	case SS_SIOCADDRT:	/* add route - uses struct ortentry */
		IOCTL(SIOCADDRT);
		break;

	case SS_SIOCDELRT:	/* delete route - uses struct ortentry */
		IOCTL(SIOCDELRT);
		break;

	case SS_SIOCSIFADDR:	/* set ifnet address */
		IOCTL(SIOCSIFADDR);
		break;

	case SS_SIOCGIFADDR:	/* get ifnet address */
		IOCTL(SIOCGIFADDR);
		break;

	case SS_SIOCSIFDSTADDR:	/* set p-p address */
		IOCTL(SIOCSIFDSTADDR);
		break;

	case SS_SIOCGIFDSTADDR:	/* get p-p address */
		IOCTL(SIOCGIFDSTADDR);
		break;

	case SS_SIOCSIFFLAGS:	/* set ifnet flags */
		IOCTL(SIOCSIFFLAGS);
		break;

	case SS_SIOCGIFFLAGS:	/* get ifnet flags */
		IOCTL(SIOCGIFFLAGS);
		break;

	case SS_SIOCGIFCONF:	/* get ifnet ltst */
		IOCTL(SIOCGIFCONF);
		break;

	case SS_SIOCGIFBRDADDR:	/* get broadcast addr */
		IOCTL(SIOCGIFBRDADDR);
		break;

	case SS_SIOCSIFBRDADDR:	/* set broadcast addr */
		IOCTL(SIOCSIFBRDADDR);
		break;

	case SS_SIOCGIFNETMASK:	/* get net addr mask */
		IOCTL(SIOCGIFNETMASK);
		break;

	case SS_SIOCSIFNETMASK:	/* set net addr mask */
		IOCTL(SIOCSIFNETMASK);
		break;

	case SS_SIOCGIFMETRIC:	/* get IF metric */
		IOCTL(SIOCGIFMETRIC);
		break;

	case SS_SIOCSIFMETRIC:	/* set IF metric */
		IOCTL(SIOCSIFMETRIC);
		break;

/*		FreeBSD 2.0 does not have socket ARPs */

#ifdef	SIOCSARP

	case SS_SIOCSARP:	/* set arp entry */
		IOCTL(SIOCSARP);
		break;

	case SS_SIOCGARP:	/* get arp entry */
		IOCTL(SIOCGARP);
		break;

	case SS_SIOCDARP:	/* delete arp entry */
		IOCTL(SIOCDARP);
		break;

#else	/* SIOCSARP */

	case SS_SIOCSARP:	/* set arp entry */
		return(EINVAL);

	case SS_SIOCGARP:	/* get arp entry */
		return(EINVAL);

	case SS_SIOCDARP:	/* delete arp entry */
		return(EINVAL);

#endif	/* SIOCSARP */

	case SS_SIOCGENADDR:	/* Get ethernet addr XXX */
		return (EINVAL);
/*		return (error = ioctl_s(fp, SIOCGIFHWADDR, arg, p)); */

	case SS_SIOCSIFMTU:	/* get if_mtu */
		IOCTL(SIOCSIFMTU);
		break;

	case SS_SIOCGIFMTU:	/* set if_mtu */
		IOCTL(SIOCGIFMTU);
		break;

	case SS_SIOCGETNAME:	/* getsockname XXX */
		return (EINVAL);
/*		return (ioctl_s(fp, SIOCGIFNAME, arg, p)); MMM */

	case SS_SIOCGETPEER: {	/* getpeername */
		struct moose {
			int	fd;
			caddr_t	asa;
			int	*alen;
			int	compat_43;
		} args;

		struct alien_sockaddr uaddr;
		struct sockaddr nuaddr;
		int nuaddr_len = sizeof(struct sockaddr);
		struct filedesc *fdp;

		if(fp->f_type != DTYPE_SOCKET)
			return (ENOTSOCK);

		bzero((caddr_t)&nuaddr, sizeof(struct sockaddr));
		fdp = p->p_fd;
		args.fd = -1;
		while(++args.fd < NOFILE)
			if ( fp == fdp->fd_ofiles[args.fd] )
				break;
		if(args.fd == NOFILE){
			printf("ss_fop_ioctl: [%d] SS_SIOCGETPEER args.fd > NOFILE\n", p->p_pid);
			return(EBADF);
		}
		args.asa = (caddr_t)&nuaddr;
		args.alen = &nuaddr_len;
		args.compat_43 = 0;
		error = SYSCALLX(SYS_getpeername, &args);
		if(error)
			return(error);

		bzero((caddr_t)&uaddr, sizeof(struct alien_sockaddr));
		uaddr.sa_family = (unsigned short)nuaddr.sa_family;
		bcopy(&nuaddr.sa_data, &uaddr.sa_data, __ALIEN_SOCK_SIZE__ - sizeof(unsigned short));

		error = copyout((caddr_t)&uaddr, (caddr_t)arg, sizeof(struct alien_sockaddr));
		return error;
	}

	default:
		printf("ss_fop_ioctl: [%d] %lx unknown ioctl 0x%x, 0x%lx\n",
				p->p_pid, (unsigned long)fp,
				cmd, (unsigned long)arg);
		return (EINVAL);
	}
	IBCS2_MAGIC_RETURN(arg);
}

int
sockioctl(dev, cmd, arg, fflag, p)
	dev_t dev;
	int cmd;
	caddr_t arg;
	int fflag;
	struct proc *p;
{
	int error;
	int retval[2];

	if(ss_debug) {
	char	cmd_type;
	int	cmd_ordinal;
	static char **ioctl_strings;
	unsigned int ioctl_type;
	unsigned int ioctl_len;

	static char *ioctl_type_strings[] = {
		"NIOCxx", "SS_IO", "SS_IOR", "3?", "SS_IOW", "5?", "SS_IOWR"
	};
	static char *ioctl_S_strings[] = {
		"0?", "SIOCSHIWAT", "SIOCGHIWAT", "SIOCSLOWAT", "SIOCGLOWAT",
		"SIOCATMARK", "SIOCSPGRP", "SIOCGPGRP", "FIONREAD",
		"FIONBIO", "FIOASYNC", "SIOCPROTO", "SIOCGETNAME",
		"SIOCGETPEER", "IF_UNITSEL", "SIOCXPROTO"
	};
	static char *ioctl_R_strings[] = {
		"0?", "1?", "2?", "3?", "4?", "5?", "6?", "7?", "8?",
		"SIOCADDRT", "SIOCDELRT"
	};
	static char *ioctl_I_strings[] = {
		"0?", "1?", "2?", "3?", "4?", "5?", "6?", "7?", "8?",
		"9?", "10?", "SIOCSIFADDR", "SIOCGIFADDR", "SIOCSIFDSTADDR",
		"SIOCGIFDSTADDR", "SIOCSIFFLAGS", "SIOCGIFFLAGS",
		"SIOCGIFCONF", "18?", "19?", "20?", "SIOCSIFMTU",
		"SIOCGIFMTU", "23?", "24?", "25?", "SIOCIFDETACH",
		"SIOCGENPSTATS", "28?", "SIOCX25XMT", "SS_SIOCX25RCV",
		"SS_SIOCX25TBL", "SIOCGIFBRDADDR" ,"SIOCSIFBRDADDR",
		"SIOCGIFNETMASK", "SIOCSIFNETMASK", "SIOCGIFMETRIC",
		"SIOCSIFMETRIC", "SIOCSARP", "SIOCGARP", "SIOCDARP",
		"SIOCSIFNAME", "SIOCGIFONEP", "SIOCSIFONEP ",
		"44?", "45?", "46?", "47?", "48?", "49?", "50?", "51?",
		"52?", "53?", "54?", "55?", "56?", "57?", "58?", "59?",
		"60?", "61?", "62?", "63?", "64?", "SIOCGENADDR",
		"SIOCSOCKSYS"
	};
	static char *ioctl_NIOC_strings[] = {
		"0?", "NIOCNFSD", "NIOCOLDGETFH", "NIOCASYNCD",
		"NIOCSETDOMNAM", "NIOCGETDOMNAM", "NIOCCLNTHAND",
		"NIOCEXPORTFS", "NIOCGETFH", "NIOCLSTAT"
	};

	cmd_ordinal = cmd & 0xff;
	cmd_type = (cmd >> 8) & 0xff;
	switch (cmd_type) {

	case   0:
		ioctl_strings = ioctl_NIOC_strings;
		cmd_type = ' ';
		if (cmd_ordinal > 9)
			cmd_ordinal = -1;
		break;

	case 'S':
		ioctl_strings = ioctl_S_strings;
		if (cmd_ordinal > 15)
			cmd_ordinal = -1;
		break;

	case 'R':
		ioctl_strings = ioctl_R_strings;
		if (cmd_ordinal > 10)
			cmd_ordinal = -1;
		break;

	case 'I':
		ioctl_strings = ioctl_I_strings;
		if (cmd_ordinal > 66)
			cmd_ordinal = -1;
		break;

	default:
		cmd_type = '?';
			break;
		
	}
	ioctl_type = (0xe0000000 & cmd) >> 29;
	ioctl_len = (cmd >> 16) & SS_IOCPARM_MASK;

	printf("sockioctl: [%d] ",p->p_pid);
	if(cmd_type != '?'){
		if(cmd_ordinal != -1)
			printf("%s %s('%c',%d,l=%d) ",ioctl_strings[cmd_ordinal],
				ioctl_type_strings[ioctl_type],
				cmd_type,
				cmd_ordinal,
				ioctl_len);
		else {
			cmd_ordinal = cmd & 0xff;
			printf("[unknown ordinal %d] %s('%c',%d,l=%d) ",cmd_ordinal,
				ioctl_type_strings[ioctl_type],
				cmd_type,
				cmd_ordinal,
				ioctl_len);
		}
	}
	else {
		printf("? %s('%c',%d,l=%d) ",
			ioctl_type_strings[ioctl_type],
			cmd_type,
			cmd_ordinal,
			ioctl_len);
	}

	printf("0x%x (0x%x) <0x%x>\n",
		dev, cmd, arg);
	}

	if(*(((int *)arg) - 3) != IBCS2_MAGIC_IN){
		printf("sockioctl: bad magic (sys_generic.c has no socksys mods?)\n");
		return(EINVAL);
	}

	switch (cmd) {

	case SS_SIOCSOCKSYS:		/* ss syscall */
		return ss_syscall(arg, p);

	/* NIOCxx: These ioctls are really just integers
	 *	   (no other information to go on).
	 */

	case NIOCSETDOMNAM: {
		struct sgdomarg domargs;

		if(error = copyin((caddr_t)*((caddr_t *)arg), (caddr_t)&domargs, sizeof(struct sgdomarg)))
			return(error);

		arg = (caddr_t)&domargs;
		SYSCALL_N(SYS_setdomainname, 0, 0);
		break;
	}

	case NIOCGETDOMNAM: {
		struct sgdomarg domargs;

		if(error = copyin((caddr_t)*((caddr_t *)arg), (caddr_t)&domargs, sizeof(struct sgdomarg)))
			return(error);

		arg = (caddr_t)&domargs;
		SYSCALL_N(SYS_getdomainname, 0, 0);
		break;
	}

	case NIOCLSTAT: {
		struct lstatarg st;

		if(error = copyin((caddr_t)*((caddr_t *)arg), (caddr_t)&st, sizeof(struct lstatarg)))
			return(error);

		/* DO WE HAVE A FOREIGN LSTAT */
/*		return mumbo_lstat(st.fname, st.statb); */
		return (EINVAL);
	}

	case NIOCNFSD:
	case NIOCOLDGETFH:
	case NIOCASYNCD:
	case NIOCCLNTHAND:
	case NIOCEXPORTFS:
	case NIOCGETFH:
		return (EINVAL);


	case SS_IF_UNITSEL:		/* set unit number */
	case SS_SIOCXPROTO:		/* empty proto table */

	case SS_SIOCIFDETACH:		/* detach interface */
	case SS_SIOCGENPSTATS:		/* get ENP stats */

	case SS_SIOCSIFNAME:		/* set interface name */
	case SS_SIOCGIFONEP:		/* get one-packet params */
	case SS_SIOCSIFONEP:		/* set one-packet params */

	case SS_SIOCPROTO:		/* link proto */
	case SS_SIOCX25XMT:
	case SS_SIOCX25RCV:
	case SS_SIOCX25TBL:

		printf("sockioctl: [%d] unsupported ioctl 0x%x , 0x%lx\n",
			p->p_pid,
			cmd, (unsigned long)arg);
		return (EINVAL);

	default:
		printf("sockioctl: [%d] unknown ioctl 0x%x , 0x%lx\n",
			p->p_pid,
			cmd, (unsigned long)arg);
		return (EINVAL);
	}
	IBCS2_MAGIC_RETURN(arg);
}


int sockopen(dev, mode, devtype, p)
	dev_t dev;
	int mode;
	int devtype;
	struct proc *p;
{

	if(ss_debug)
		printf("sockopen: [%d] 0x%x\n", p->p_pid, dev);

	/* minor = 0 is the socksys device itself. No special handling
	 *           will be needed as it is controlled by the application
	 *           via ioctls.
	 */
	if (minor(dev) == 0)
		return 0;

	/* minor = 1 is the spx device. This is the client side of a
	 *           streams pipe to the X server. Under SCO and friends
	 *           the library code messes around setting the connection
	 *           up itself. We do it ourselves - this means we don't
	 *           need to worry about the implementation of the server
	 *           side (/dev/X0R - which must exist but can be a link
	 *           to /dev/null) nor do we need to actually implement
	 *           getmsg/putmsg.
	 */
{ /* SPX */
	int fd, error, args[3];
	int retval[2];
#define SUN_LEN(su) \
        (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path)) + 1
	struct sockaddr_un *Xaddr = (struct sockaddr_un *)UA_ALLOC();
	retval[0] = retval[1] = 0;
	if(ss_debug)
		printf("sockopen: SPX: [%d] opening\n", p->p_pid);

	/* Grab a socket. */
	if(ss_debug)
		printf("sockopen: SPX: [%d] get a unix domain socket\n",
					p->p_pid);
	args[0] = AF_UNIX;
	args[1] = SOCK_STREAM;
	args[2] = 0;
	error = SYSCALLX(SYS_socket, args);
	if (error)
		return error;
	fd = retval[0];
	if(fd < 1) {
		printf("sockopen: SPX: [%d] unexpected fd of %d\n",
			 p->p_pid, fd);
		return(EOPNOTSUPP); /* MRL whatever */
	}

	/* Connect the socket to X. */
	if(ss_debug)
		printf("sockopen: SPX: [%d] connect to /tmp/X11-unix/X0\n",
			p->p_pid);
	args[0] = fd;
	Xaddr->sun_family = AF_UNIX;
	copyout("/tmp/.X11-unix/X0", Xaddr->sun_path, 18);
	Xaddr->sun_len = SUN_LEN(Xaddr);
	args[1] = (int)Xaddr;
	args[2] = sizeof(struct sockaddr_un);
	error = SYSCALLX(SYS_connect, args);
	if (error) {
		(void)SYSCALLX(SYS_close, &fd);
		return error;
	}

	put_socket_fops(p,fd);

	return 0;
} /* SPX */
}


int sockclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	if(ss_debug)
		printf("sockclose: [%d] 0x%x\n", p->p_pid, dev);
	return(0);
}

static
int ss_fop_close(struct file *fp, struct proc *p)
{

int	fd;
struct filedesc *fdp;

	if(ss_debug){
		fdp = p->p_fd;
		fd = -1;
		while(++fd < NOFILE)
				if ( fp == fdp->fd_ofiles[fd] )
					break;
		printf("ss_fop_close: [%d] fd=%d ", p->p_pid, fd);
	}

	if(fp->f_type == DTYPE_SOCKET) {
		if(ss_debug)
			printf("is a socket\n");
		return(close_s(fp, p));
	}
	else {
		if(ss_debug)
			printf("is not a socket\n");
		return(ENOTSOCK);
	}
}

void put_socket_fops(struct proc *p, int fd)
{
struct filedesc *fdp;
struct file *fp;

	fdp = p->p_fd;
	fp = fdp->fd_ofiles[fd];
	if (ss_socket_fops.fo_ioctl != fp->f_ops->fo_ioctl) {
		bcopy(fp->f_ops, &ss_socket_fops, sizeof(struct fileops));
		ioctl_s = ss_socket_fops.fo_ioctl; /* save standard ioctl */
		close_s = ss_socket_fops.fo_close; /* save standard close */
		ss_socket_fops.fo_ioctl = ss_fop_ioctl;
		ss_socket_fops.fo_close = ss_fop_close;
	}
	fp->f_ops = &ss_socket_fops;

	return;
}

int	ss_SYSCALL(n,convert_arg,indicator,arg,p,retval)
	int	n;		/* syscall ordinal */
	int	convert_arg;	/* if not 0, argument to convert */
	int	indicator;	/* type of argument to convert */
	int	*arg;		/* address of alien arg */
	struct	proc *p;
	int	*retval;
{
int	error;
int	rc;

	if(convert_arg){
		if(rc = ss_convert_struct( (caddr_t)*(arg + convert_arg),
					indicator,
					SS_ALIEN_TO_NATIVE))
			return(rc);

		error = (*sysent[n].sy_call)(p, arg + 1, retval);
		rc = ss_convert_struct( (caddr_t)*(arg + convert_arg),
					indicator,
					SS_NATIVE_TO_ALIEN);
		if(ss_debug)
			printf("ss_SYSCALL: [%d] error=%d, rc=%d\n",
						p->p_pid, error, rc);
	}
	else {
		rc = 0;
		error = (*sysent[n].sy_call)(p, arg + 1, retval);
		if(ss_debug)
			printf("ss_SYSCALL: [%d] error=%d\n",p->p_pid, error);
	}

	return(error ? error : rc);
}

int	ss_IOCTL(fp, cmd, arg, p)
	struct file *fp;
	int	cmd;
	int	*arg;		/* address of alien arg */
	struct	proc *p;
{
int	error, rc;
int	these[2];
char	cmd_type;
int	cmd_ordinal;
int	indicator;

	cmd_type = (cmd >> 8) & 0xff;
	cmd_ordinal = cmd & 0xff;
	these[0] = cmd_type;
	these[1] = cmd_ordinal;
	if(ss_debug > 1)
		printf("ss_IOCTL: calling ss_convert with %d(%c) %d\n",
				these[0],these[0],these[1]);
	indicator = ss_convert( struct_whatevers, these, 0);
	if(ss_debug > 1)
		printf("ss_IOCTL: ss_convert returns indicator %d\n",indicator);
	if(indicator){
		error = ss_convert_struct((caddr_t)*(arg + 2),
					  indicator,
					  SS_ALIEN_TO_NATIVE);
		if(ss_debug > 1)
			printf("ss_IOCTL: ss_convert_struct returns %d\n",error);
		if(error)
			return(error);
		/* change len in ioctl now - in the general case */
		error = ioctl_s(fp, cmd, (caddr_t)arg, p);
		rc = ss_convert_struct( (caddr_t)*(arg + 2),
					indicator,
					SS_NATIVE_TO_ALIEN);
		if(ss_debug)
			printf("ss_IOCTL: [%d] error=%d, rc=%d\n",p->p_pid,
						error, rc);
	}
	else {
		rc = 0;
		error = ioctl_s(fp, cmd, (caddr_t)arg, p);
		if(ss_debug)
			printf("ss_IOCTL: [%d] error=%d\n",p->p_pid, error);
	}

	return(error ? error : rc);
}


struct ss_socketopt_args {
        int     s;
	int     level;
	int     name;
	caddr_t val;
	int     valsize;
};

int
ss_setsockopt(arg, ret, p)
	struct ss_socketopt_args  *arg;
	int	*ret;
	struct	proc *p;
{
	int error, optname;
	int retval[2];

	if (arg->level != 0xffff) /* FreeBSD, SCO and ? */
		return (ENOPROTOOPT);

	optname = ss_convert(sopt_whatevers, &arg->name, 0);

	switch (optname) {

	case SO_ACCEPTCONN:
	case SO_BROADCAST:
	case SO_DEBUG:
	case SO_DONTROUTE:
	case SO_LINGER:
	case SO_KEEPALIVE:
	case SO_OOBINLINE:
	case SO_RCVBUF:
	case SO_RCVLOWAT:
	case SO_RCVTIMEO:
	case SO_REUSEADDR:
	case SO_SNDBUF:
	case SO_SNDLOWAT:
	case SO_SNDTIMEO:
	case SO_USELOOPBACK:
		error = SYSCALLX(SYS_setsockopt, arg);
		*ret = retval[0];
		*(ret + 1) = retval[1];
		return(error);

	case SO_ERROR:
	case SO_IMASOCKET:
	case SO_NO_CHECK:
	case SO_ORDREL:
	case SO_PRIORITY:
	case SO_PROTOTYPE:
	case SO_TYPE:
		return (ENOPROTOOPT);

	}

	return (ENOPROTOOPT);
}


int
ss_getsockopt(arg, ret, p)
	struct ss_socketopt_args  *arg;
	int	*ret;
	struct	proc *p;
{
	int error, optname;
	int retval[2];

	if (arg->level != 0xffff) /* FreeBSD, SCO and ? */
		return (ENOPROTOOPT);

	optname = ss_convert(sopt_whatevers, &arg->name, 0);

	switch (optname) {

	case SO_ACCEPTCONN:
	case SO_BROADCAST:
	case SO_DEBUG:
	case SO_DONTROUTE:
	case SO_ERROR:
	case SO_KEEPALIVE:
	case SO_LINGER:
	case SO_OOBINLINE:
	case SO_RCVBUF:
	case SO_RCVLOWAT:
	case SO_RCVTIMEO:
	case SO_REUSEADDR:
	case SO_SNDBUF:
	case SO_SNDLOWAT:
	case SO_SNDTIMEO:
	case SO_TYPE:
	case SO_USELOOPBACK:
		error = SYSCALLX(SYS_getsockopt, arg);
		*ret = retval[0];
		*(ret + 1) = retval[1];
		return(error);


	case SO_PROTOTYPE: {
		int	value = 0;

		error = copyout((caddr_t)&value, (caddr_t)arg->s, sizeof(int));
			return(error);
	}


	case SO_IMASOCKET: {
		int	value = 1;

		error = copyout((caddr_t)&value, (caddr_t)arg->s, sizeof(int));
			return(error);
	}

	case SO_NO_CHECK:
	case SO_ORDREL:
	case SO_PRIORITY:
		return (ENOPROTOOPT);
	}

	return (ENOPROTOOPT);
}
                
#define	SS_CONVERT
int	system_type = SS_FREEBSD; /* FreeBSD */

int
ss_convert(what, this, otherwise)
	struct whatever **what;
	int *this;
	int otherwise;
{
	struct whatever *specific;

	if(!(specific = what[system_type]))
		return *this;

	for (; specific->from != -1; specific++)
		if(specific->from <= *this && *this <= specific->to)
			if(specific->from == specific->to){
				if(specific->more){
					specific = specific->more;
					this++;
					continue;
				}
				else {
					return((int)specific->conversion);
				}
			}
			else {
				return(specific->conversion ? (
				       specific->all_the_same ? (int)specific->conversion : specific->conversion[*this - specific->from] ) : *this);
			}

	return otherwise;
}

/*	Returns	0 - no conversion, no pointer modification
		1 - converted, relevant pointer modification
	       -1 - error
 */
int
ss_convert_struct(alien, indicator, direction)
	char	*alien;
	int	indicator;
	int	direction;
{
int	error, len;

	switch (system_type) {

	case SS_FREEBSD:
		return(0);
	case SS_SYSVR4:
	case SS_SYSVR3:
	case SS_SCO_32:
	case SS_WYSE_321:
	case SS_ISC:
	case SS_LINUX:

		switch(direction){

		case SS_ALIEN_TO_NATIVE:

		error = ss_atn(alien, indicator);
		if(ss_debug > 1)
			printf("ss_convert: ATN ss_atn error %d\n",error);
		return(error);

		case SS_NATIVE_TO_ALIEN:

		error = ss_nta(alien, indicator);
		if(ss_debug > 1)
			printf("ss_convert: NTA ss_nta error %d\n",error);
		return(error);

		}

	default:

	printf("ss_convert_struct: not expecting system_type %d\n", system_type);
	break;

	}
	return(EINVAL);
}

/* note sockaddr_un linux unsigned short fam,  108 path
   BSD uchar , uchar 104 */
int
ss_atn(alien, indicator)
	char	*alien;
	int	indicator;
{
int	error;

	switch (indicator) {

	case SS_STRUCT_ARPREQ:
		/* compatible */
		return(0);

	case SS_STRUCT_IFCONF:
		/* compatible */
		return(0);

	case SS_STRUCT_IFREQ:
		/* length OK - more unions - function dependent */
		return(0);

	case SS_STRUCT_ORTENTRY:
		/* compatible */
		return(0);

	case SS_STRUCT_SOCKADDR:{
		struct native_hdr {
			u_char	len;
			u_char	family;
		};
		union hdr_part {
			struct native_hdr native;
			u_short alien_family;
		} hdr;

		if(error = copyin((caddr_t)alien,(caddr_t)&hdr,sizeof(hdr)))
			return(error);
		if(ss_debug > 1)
			printf("ss_atn:copyin 0x%x\n",hdr.alien_family);

		if( hdr.alien_family < AF_MAX){
			hdr.native.family = hdr.alien_family >> 8; /* 386 endianess */
			/* OR LEN FOM A PARAM ? */
			hdr.native.len = sizeof(struct sockaddr);
		if(ss_debug > 1)
			printf("ss_atn:copyout 0x%x\n",hdr.alien_family);
			error = copyout((caddr_t)&hdr,(caddr_t)alien,sizeof(hdr));
			return(error);
		}
		else {
			printf("ss_atn: sa_family = %d\n", hdr.alien_family);
			return(EINVAL);
		}

	}

	case SS_STRUCT_SOCKNEWPROTO:
		/* don't have */
		printf("ss_atn: not expecting SS_STRUCT_SOCKNEWPROTO\n");
		return(EINVAL);

	default:
		printf("ss_atn: not expecting case %d\n",indicator);
		return(EINVAL);

	}
}

/* note sockaddr_un linux unsigned short fam,  108 path
   BSD uchar , uchar 104 */
int
ss_nta(alien, indicator)
	char	*alien;
	int	indicator;
{
int	error;

	switch (indicator) {

	case SS_STRUCT_ARPREQ:
		/* compatible */
		return(0);

	case SS_STRUCT_IFCONF:
		/* compatible */
		return(0);

	case SS_STRUCT_IFREQ:
		/* length OK - more unions - function dependent */
		return(0);

	case SS_STRUCT_ORTENTRY:
		/* compatible */
		return(0);

	case SS_STRUCT_SOCKADDR:{
		struct native_hdr {
			u_char	len;
			u_char	family;
		};
		union hdr_part {
			struct native_hdr native;
			u_short alien_family;
		} hdr;

		if(error = copyin((caddr_t)alien,(caddr_t)&hdr,sizeof(hdr)))
			return(error);
		if(ss_debug > 1)
			printf("ss_nta:copyin 0x%x\n",hdr.alien_family);
		hdr.alien_family = hdr.native.family;
		if(ss_debug > 1)
			printf("ss_nta:copyout 0x%x\n",hdr.alien_family);
		error = copyout((caddr_t)&hdr,(caddr_t)alien,sizeof(hdr));
		return(error);
	}

	case SS_STRUCT_SOCKNEWPROTO:
		/* don't have */
		printf("ss_nta: not expecting SS_STRUCT_SOCKNEWPROTO\n");
		return(EINVAL);

	default:
		printf("ss_nta: not expecting case %d\n",indicator);
		return(EINVAL);

	}
}
