/*-
 * Copyright (c) 1994 Søren Schmidt
 * Copyright (c) 1994 Sean Eric Fagan
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
 *	$Id: ibcs2.h,v 1.11 1994/10/13 23:10:58 sos Exp $
 */

/* trace all iBCS2 system calls */
extern int ibcs2_trace;

/* convert signals between bsd & iBCS2 */
extern int bsd_to_ibcs2_signal[];
extern int ibcs2_to_bsd_signal[];
char *ibcs2_sig_to_str(int);

/* iBCS2 type definitions */
typedef char *		ibcs2_caddr_t;
typedef long		ibcs2_daddr_t;
typedef long		ibcs2_off_t;
typedef long		ibcs2_key_t;
typedef unsigned short	ibcs2_uid_t;
typedef unsigned long	ibcs2_x_uid_t;
typedef unsigned short	ibcs2_gid_t;
typedef unsigned long	ibcs2_x_gid_t;
typedef short		ibcs2_nlink_t;
typedef unsigned long	ibcs2_x_nlink_t;
typedef short		ibcs2_dev_t;
typedef long		ibcs2_x_dev_t;
typedef unsigned short	ibcs2_ino_t;
typedef unsigned long	ibcs2_x_ino_t;
typedef unsigned short	ibcs2_mode_t;
typedef unsigned long	ibcs2_x_mode_t;
typedef short		ibcs2_pid_t;
typedef long		ibcs2_x_pid_t;
typedef unsigned int	ibcs2_size_t;
typedef unsigned long	ibcs2_time_t;
typedef struct timespec	ibcs2_timestruc_t;
typedef long		ibcs2_clock_t;
typedef unsigned int	ibcs2_sigset_t;
typedef	void 		(*ibcs2_sig_t) (int);

/* misc defines */
#define UA_ALLOC() \
	(ALIGN(((caddr_t)PS_STRINGS) + sizeof(struct ps_strings)))
#define IBCS2_RETVAL_SIZE	(3 * sizeof(int))
#define IBCS2_MAGIC_IN		0xe215
#define IBCS2_MAGIC_OUT		0x8e11
#define	IBCS2_MAGIC_RETURN	*(((int *)arg) - 3) = IBCS2_MAGIC_OUT; \
				*(((int *)arg) - 2) = retval[0]; \
				*(((int *)arg) - 1) = retval[1]; \
				return(0);

/* iBCS2 signal numbers */
#define IBCS2_SIGHUP	1
#define IBCS2_SIGINT	2
#define IBCS2_SIGQUIT	3
#define IBCS2_SIGILL	4
#define IBCS2_SIGTRAP	5
#define IBCS2_SIGIOT	6
#define IBCS2_SIGABRT	6
#define IBCS2_SIGEMT	7
#define IBCS2_SIGFPE	8
#define IBCS2_SIGKILL	9
#define IBCS2_SIGBUS	10
#define IBCS2_SIGSEGV	11
#define IBCS2_SIGSYS	12
#define IBCS2_SIGPIPE	13
#define IBCS2_SIGALRM	14
#define IBCS2_SIGTERM	15
#define IBCS2_SIGUSR1	16
#define IBCS2_SIGUSR2	17
#define IBCS2_SIGCLD	18
#define IBCS2_SIGCHLD	18
#define IBCS2_SIGPWR	19
#define IBCS2_SIGWINCH	20
#define IBCS2_SIGURG	21
#define IBCS2_SIGPOLL	22
#define IBCS2_SIGIO	22
#define IBCS2_SIGSTOP	23
#define IBCS2_SIGTSTP	24
#define IBCS2_SIGCONT	25
#define IBCS2_SIGTTIN	26
#define IBCS2_SIGTTOU	27
#define IBCS2_SIGVTALRM	28
#define IBCS2_SIGPROF	29
#define IBCS2_SIGGXCPU	30
#define IBCS2_SIGGXFSZ	31
#define IBCS2_NSIG	32
#define IBCS2_SIGMASK	0xFF

#define IBCS2_SA_NOCLDSTOP 0x01
#define	IBCS2_SIG_DFL	(void (*)())0
#define	IBCS2_SIG_IGN	(void (*)())1
#define	IBCS2_SIG_HOLD	(void (*)())2

/* iBCS2 open & fcntl file modes */
#define	IBCS2_RDONLY	0x000
#define IBCS2_WRONLY	0x001
#define IBCS2_RDWR	0x002
#define IBCS2_NDELAY	0x004
#define IBCS2_APPEND	0x008
#define IBCS2_SYNC	0x010
#define IBCS2_NONBLOCK	0x080
#define IBCS2_CREAT	0x100
#define IBCS2_TRUNC	0x200
#define IBCS2_EXCL	0x400
#define IBCS2_NOCTTY	0x800
#define IBCS2_PRIV	0x1000

/* iBCS2 fcntl commands */
#define IBCS2_F_DUPFD	0
#define IBCS2_F_GETFD	1
#define IBCS2_F_SETFD	2
#define IBCS2_F_GETFL	3
#define IBCS2_F_SETFL	4
#define IBCS2_F_GETLK	5
#define IBCS2_F_SETLK	6
#define IBCS2_F_SETLKW	7

#define IBCS2_F_RDLCK	1
#define IBCS2_F_WRLCK	2
#define IBCS2_F_UNLCK	3

/* iBCS2 poll commands */
#define IBCS2_POLLIN  		0x0001
#define IBCS2_POLLPRI 		0x0002
#define IBCS2_POLLOUT 		0x0004
#define IBCS2_POLLERR 		0x0008
#define IBCS2_POLLHUP 		0x0010
#define IBCS2_POLLNVAL		0x0020
#define IBCS2_POLLRDNORM	0x0040
#define IBCS2_POLLWRNORM	0x0004	
#define IBCS2_POLLRDBAND	0x0080
#define IBCS2_POLLWRBAND	0x0100
#define IBCS2_READPOLL	(IBCS2_POLLIN|IBCS2_POLLRDNORM|IBCS2_POLLRDBAND)
#define IBCS2_WRITEPOLL (IBCS2_POLLOUT|IBCS2_POLLWRNORM|IBCS2_POLLWRBAND)

/* iBCS2 termio input modes */
#define	IBCS2_IGNBRK	0x0001
#define	IBCS2_BRKINT	0x0002
#define	IBCS2_IGNPAR	0x0004
#define	IBCS2_PARMRK	0x0008
#define	IBCS2_INPCK	0x0010
#define	IBCS2_ISTRIP	0x0020
#define	IBCS2_INLCR	0x0040
#define	IBCS2_IGNCR	0x0080
#define	IBCS2_ICRNL	0x0100
#define	IBCS2_IUCLC	0x0200
#define	IBCS2_IXON	0x0400
#define	IBCS2_IXANY	0x0800
#define	IBCS2_IXOFF	0x1000
#define IBCS2_DOSMODE	0x8000

/* iBCS2 termio output modes */
#define	IBCS2_OPOST	0x0001
#define	IBCS2_OLCUC	0x0002
#define	IBCS2_ONLCR	0x0004
#define	IBCS2_OCRNL	0x0008
#define	IBCS2_ONOCR	0x0010
#define	IBCS2_ONLRET	0x0020
#define	IBCS2_OFILL	0x0040
#define	IBCS2_OFDEL	0x0080
#define	IBCS2_NL1	0x0100
#define	IBCS2_CR1	0x0200
#define	IBCS2_CR2	0x0400
#define	IBCS2_TAB1	0x0800
#define	IBCS2_TAB2	0x1000
#define	IBCS2_BS1	0x2000
#define	IBCS2_VT1	0x4000
#define	IBCS2_FF1	0x8000

/* iBCS2 termio control modes */
#define	IBCS2_CBAUD	0x000F
#define	IBCS2_B0	0x0
#define	IBCS2_B50	0x0001
#define	IBCS2_B75	0x0002
#define	IBCS2_B110	0x0003
#define	IBCS2_B134	0x0004
#define	IBCS2_B150	0x0005
#define	IBCS2_B200	0x0006
#define	IBCS2_B300	0x0007
#define	IBCS2_B600	0x0008
#define	IBCS2_B1200	0x0009
#define	IBCS2_B1800	0x000A
#define	IBCS2_B2400	0x000B
#define	IBCS2_B4800	0x000C
#define	IBCS2_B9600	0x000D
#define	IBCS2_B19200	0x000E
#define	IBCS2_B38400	0x000F
#define	IBCS2_CSIZE	0x0030
#define	IBCS2_CS5	0x0
#define	IBCS2_CS6	0x0010
#define	IBCS2_CS7	0x0020
#define	IBCS2_CS8	0x0030
#define	IBCS2_CSTOPB	0x0040
#define	IBCS2_CREAD	0x0080
#define	IBCS2_PARENB	0x0100
#define	IBCS2_PARODD	0x0200
#define	IBCS2_HUPCL	0x0400
#define	IBCS2_CLOCAL	0x0800
#define IBCS2_RCV1EN	0x1000
#define	IBCS2_XMT1EN	0x2000
#define	IBCS2_LOBLK	0x4000
#define	IBCS2_XCLUDE	0x8000

/* iBCS2 termio line discipline 0 modes */
#define	IBCS2_ISIG	0x0001
#define	IBCS2_ICANON	0x0002
#define	IBCS2_XCASE	0x0004
#define	IBCS2_ECHO	0x0008
#define	IBCS2_ECHOE	0x0010
#define	IBCS2_ECHOK	0x0020
#define	IBCS2_ECHONL	0x0040
#define	IBCS2_NOFLSH	0x0080

/* iBCS2 control characters */
#define IBCS2_VINTR	0
#define IBCS2_VQUIT	1
#define IBCS2_VERASE	2
#define IBCS2_VKILL	3
#define IBCS2_VEOF	4	/* ICANON */
#define IBCS2_VEOL	5	/* ICANON */
#define IBCS2_VEOL2	6	
#define IBCS2_VSWTCH	7
#define IBCS2_VMIN	4	/* !ICANON */
#define IBCS2_VTIME	5	/* !ICANON */
#define IBCS2_VSUSP	10
#define IBCS2_VSTART	11
#define IBCS2_VSTOP	12
#define IBCS2_NCC	8	/* termio */
#define IBCS2_NCCS	13	/* termios */

/* iBCS2 ulimit commands */
#define IBCS2_GETFSIZE	1
#define IBCS2_SETFSIZE	2
#define IBCS2_GETPSIZE	3
#define IBCS2_GETMOPEN	4

/* iBCS2 emulator trace control */
#define IBCS2_TRACE_FILE	0x00000001
#define IBCS2_TRACE_IOCTL	0x00000002
#define IBCS2_TRACE_ISC		0x00000004
#define IBCS2_TRACE_MISC	0x00000008
#define IBCS2_TRACE_SIGNAL	0x00000010
#define IBCS2_TRACE_STATS	0x00000020
#define IBCS2_TRACE_XENIX	0x00000040
#define IBCS2_TRACE_IOCTLCNV	0x00000080
#define IBCS2_TRACE_COFF	0x01000000
#define IBCS2_TRACE_ELF		0x02000000
#define IBCS2_TRACE_ALL		0x0300007f

#define IBCS2_FP_NO   	0       /* no fp support */
#define IBCS2_FP_SW   	1       /* software emulator */
#define IBCS2_FP_287  	2       /* 80287 FPU */
#define IBCS2_FP_387  	3       /* 80387 FPU */

