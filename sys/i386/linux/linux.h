/*-
 * Copyright (c) 1994-1995 Søren Schmidt
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
 *	$Id: linux.h,v 1.2 1995/11/22 07:43:43 bde Exp $
 */

#ifndef _I386_LINUX_LINUX_H_
#define _I386_LINUX_LINUX_H_

typedef unsigned short linux_uid_t;
typedef unsigned short linux_gid_t;
typedef unsigned short linux_dev_t;
typedef unsigned long linux_ino_t;
typedef unsigned short linux_mode_t;
typedef unsigned short linux_nlink_t;
typedef long linux_time_t;
typedef long linux_clock_t;
typedef char * linux_caddr_t;
typedef long linux_off_t;
typedef struct {
	long val[2];
} linux_fsid_t;
typedef int linux_pid_t;
typedef unsigned long linux_sigset_t;
typedef void (*linux_handler_t)(int);
typedef struct {
	void (*sa_handler)(int);
	linux_sigset_t sa_mask;
	unsigned long sa_flags;
	void (*sa_restorer)(void);
} linux_sigaction_t;
typedef int linux_key_t;

extern int bsd_to_linux_signal[];
extern int linux_to_bsd_signal[];

/* misc defines */
#define LINUX_NAME_MAX		255

/* signal numbers */
#define LINUX_SIGHUP		 1
#define LINUX_SIGINT		 2
#define LINUX_SIGQUIT		 3
#define LINUX_SIGILL		 4
#define LINUX_SIGTRAP		 5
#define LINUX_SIGABRT		 6
#define LINUX_SIGIOT		 6
#define LINUX_SIGUNUSED	 	 7
#define LINUX_SIGFPE		 8
#define LINUX_SIGKILL		 9
#define LINUX_SIGUSR1		10
#define LINUX_SIGSEGV		11
#define LINUX_SIGUSR2		12
#define LINUX_SIGPIPE		13
#define LINUX_SIGALRM		14
#define LINUX_SIGTERM		15
#define LINUX_SIGSTKFLT		16
#define LINUX_SIGCHLD		17
#define LINUX_SIGCONT		18
#define LINUX_SIGSTOP		19
#define LINUX_SIGTSTP		20
#define LINUX_SIGTTIN		21
#define LINUX_SIGTTOU		22
#define LINUX_SIGIO		23
#define LINUX_SIGPOLL		LINUX_SIGIO
#define LINUX_SIGURG		LINUX_SIGIO
#define LINUX_SIGXCPU		24
#define LINUX_SIGXFSZ		25
#define LINUX_SIGVTALRM		26
#define LINUX_SIGPROF		27
#define LINUX_SIGWINCH		28
#define LINUX_SIGLOST		29
#define LINUX_SIGPWR		30
#define LINUX_SIGBUS		LINUX_SIGUNUSED
#define LINUX_NSIG		32

/* sigaction flags */
#define LINUX_SA_NOCLDSTOP	0x00000001
#define LINUX_SA_ONSTACK	0x08000000
#define LINUX_SA_RESTART	0x10000000
#define LINUX_SA_INTERRUPT	0x20000000
#define LINUX_SA_NOMASK		0x40000000
#define LINUX_SA_ONESHOT	0x80000000

/* sigprocmask actions */
#define LINUX_SIG_BLOCK		0
#define LINUX_SIG_UNBLOCK	1
#define LINUX_SIG_SETMASK	2

/* termio commands */
#define LINUX_TCGETS		0x5401
#define LINUX_TCSETS		0x5402
#define LINUX_TCSETSW		0x5403
#define LINUX_TCSETSF		0x5404
#define LINUX_TCGETA		0x5405
#define LINUX_TCSETA		0x5406
#define LINUX_TCSETAW		0x5407
#define LINUX_TCSETAF		0x5408
#define LINUX_TCSBRK		0x5409
#define LINUX_TCXONC		0x540A
#define LINUX_TCFLSH		0x540B
#define LINUX_TIOCEXCL		0x540C
#define LINUX_TIOCNXCL		0x540D
#define LINUX_TIOCSCTTY		0x540E
#define LINUX_TIOCGPGRP		0x540F
#define LINUX_TIOCSPGRP		0x5410
#define LINUX_TIOCOUTQ		0x5411
#define LINUX_TIOCSTI		0x5412
#define LINUX_TIOCGWINSZ	0x5413
#define LINUX_TIOCSWINSZ	0x5414
#define LINUX_TIOCMGET		0x5415
#define LINUX_TIOCMBIS		0x5416
#define LINUX_TIOCMBIC		0x5417
#define LINUX_TIOCMSET		0x5418
#define LINUX_TIOCGSOFTCAR	0x5419
#define LINUX_TIOCSSOFTCAR	0x541A
#define LINUX_FIONREAD		0x541B
#define LINUX_TIOCINQ		FIONREAD
#define LINUX_TIOCLINUX		0x541C
#define LINUX_TIOCCONS		0x541D
#define LINUX_TIOCGSERIAL	0x541E
#define LINUX_TIOCSSERIAL	0x541F
#define LINUX_TIOCPKT		0x5420
#define LINUX_FIONBIO		0x5421
#define LINUX_TIOCNOTTY		0x5422
#define LINUX_TIOCSETD		0x5423
#define LINUX_TIOCGETD		0x5424
#define LINUX_TCSBRKP		0x5425
#define LINUX_TIOCTTYGSTRUCT	0x5426
#define LINUX_FIONCLEX		0x5450
#define LINUX_FIOCLEX		0x5451
#define LINUX_FIOASYNC		0x5452
#define LINUX_TIOCSERCONFIG	0x5453
#define LINUX_TIOCSERGWILD	0x5454
#define LINUX_TIOCSERSWILD	0x5455
#define LINUX_TIOCGLCKTRMIOS	0x5456
#define LINUX_TIOCSLCKTRMIOS	0x5457

/* line disciplines */
#define LINUX_N_TTY		0
#define LINUX_N_SLIP		1
#define LINUX_N_MOUSE		2
#define LINUX_N_PPP		3

/* Linux termio c_cc values */
#define LINUX_VINTR		0
#define LINUX_VQUIT		1
#define LINUX_VERASE		2
#define LINUX_VKILL		3
#define LINUX_VEOF		4
#define LINUX_VTIME		5
#define LINUX_VMIN		6
#define LINUX_VSWTC		7
#define LINUX_NCC		8

/* Linux termios c_cc values */
#define LINUX_VSTART		8
#define LINUX_VSTOP		9
#define LINUX_VSUSP 		10
#define LINUX_VEOL		11
#define LINUX_VREPRINT		12
#define LINUX_VDISCARD		13
#define LINUX_VWERASE		14
#define LINUX_VLNEXT		15
#define LINUX_VEOL2		16
#define LINUX_NCCS		17

/* Linux c_iflag masks */
#define LINUX_IGNBRK		0x0000001
#define LINUX_BRKINT		0x0000002
#define LINUX_IGNPAR		0x0000004
#define LINUX_PARMRK		0x0000008
#define LINUX_INPCK		0x0000010
#define LINUX_ISTRIP		0x0000020
#define LINUX_INLCR		0x0000040
#define LINUX_IGNCR		0x0000080
#define LINUX_ICRNL		0x0000100
#define LINUX_IUCLC		0x0000200
#define LINUX_IXON		0x0000400
#define LINUX_IXANY		0x0000800
#define LINUX_IXOFF		0x0001000
#define LINUX_IMAXBEL		0x0002000

/* Linux c_oflag masks */
#define LINUX_OPOST		0x0000001
#define LINUX_OLCUC		0x0000002
#define LINUX_ONLCR		0x0000004
#define LINUX_OCRNL		0x0000008
#define LINUX_ONOCR		0x0000010
#define LINUX_ONLRET		0x0000020
#define LINUX_OFILL		0x0000040
#define LINUX_OFDEL		0x0000080
#define LINUX_NLDLY		0x0000100

#define LINUX_NL0		0x0000000
#define LINUX_NL1		0x0000100
#define LINUX_CRDLY		0x0000600
#define LINUX_CR0		0x0000000
#define LINUX_CR1		0x0000200
#define LINUX_CR2		0x0000400
#define LINUX_CR3		0x0000600
#define LINUX_TABDLY		0x0001800
#define LINUX_TAB0		0x0000000
#define LINUX_TAB1		0x0000800
#define LINUX_TAB2		0x0001000
#define LINUX_TAB3		0x0001800
#define LINUX_XTABS		0x0001800
#define LINUX_BSDLY		0x0002000
#define LINUX_BS0		0x0000000
#define LINUX_BS1		0x0002000
#define LINUX_VTDLY		0x0004000
#define LINUX_VT0		0x0000000
#define LINUX_VT1		0x0004000
#define LINUX_FFDLY		0x0008000
#define LINUX_FF0		0x0000000
#define LINUX_FF1		0x0008000

#define LINUX_CBAUD		0x0000100f
#define LINUX_B0		0x00000000
#define LINUX_B50		0x00000001
#define LINUX_B75		0x00000002
#define LINUX_B110		0x00000003
#define LINUX_B134		0x00000004
#define LINUX_B150		0x00000005
#define LINUX_B200		0x00000006
#define LINUX_B300		0x00000007
#define LINUX_B600		0x00000008
#define LINUX_B1200		0x00000009
#define LINUX_B1800		0x0000000a
#define LINUX_B2400		0x0000000b
#define LINUX_B4800		0x0000000c
#define LINUX_B9600		0x0000000d
#define LINUX_B19200		0x0000000e
#define LINUX_B38400		0x0000000f
#define LINUX_EXTA		LINUX_B19200
#define LINUX_EXTB		LINUX_B38400
#define LINUX_CBAUDEX		0x00001000
#define LINUX_B57600		0x00001001
#define LINUX_B115200		0x00001002

#define LINUX_CSIZE		0x00000030
#define LINUX_CS5		0x00000000
#define LINUX_CS6		0x00000010
#define LINUX_CS7		0x00000020
#define LINUX_CS8		0x00000030
#define LINUX_CSTOPB		0x00000040
#define LINUX_CREAD		0x00000080
#define LINUX_PARENB		0x00000100
#define LINUX_PARODD		0x00000200
#define LINUX_HUPCL		0x00000400
#define LINUX_CLOCAL		0x00000800
#define LINUX_CRTSCTS		0x80000000

/* Linux c_lflag masks */
#define LINUX_ISIG		0x00000001
#define LINUX_ICANON		0x00000002
#define LINUX_XCASE		0x00000004
#define LINUX_ECHO		0x00000008
#define LINUX_ECHOE		0x00000010
#define LINUX_ECHOK		0x00000020
#define LINUX_ECHONL		0x00000040
#define LINUX_NOFLSH		0x00000080
#define LINUX_TOSTOP		0x00000100
#define LINUX_ECHOCTL		0x00000200
#define LINUX_ECHOPRT		0x00000400
#define LINUX_ECHOKE		0x00000800
#define LINUX_FLUSHO		0x00001000
#define LINUX_PENDIN		0x00002000
#define LINUX_IEXTEN		0x00008000

/* open/fcntl flags */
#define LINUX_O_RDONLY		00
#define LINUX_O_WRONLY		01
#define LINUX_O_RDWR		02
#define LINUX_O_CREAT		0100
#define LINUX_O_EXCL		0200
#define LINUX_O_NOCTTY		0400
#define LINUX_O_TRUNC		01000
#define LINUX_O_APPEND		02000
#define LINUX_O_NONBLOCK	04000
#define LINUX_O_NDELAY		LINUX_O_NONBLOCK
#define LINUX_O_SYNC		010000
#define LINUX_FASYNC		020000

/* fcntl flags */
#define LINUX_F_DUPFD		0
#define LINUX_F_GETFD		1
#define LINUX_F_SETFD		2
#define LINUX_F_GETFL		3
#define LINUX_F_SETFL		4
#define LINUX_F_GETLK		5
#define LINUX_F_SETLK		6
#define LINUX_F_SETLKW		7
#define LINUX_F_SETOWN		8
#define LINUX_F_GETOWN		9

#define LINUX_F_RDLCK		0
#define LINUX_F_WRLCK		1
#define LINUX_F_UNLCK		2

/* mmap options */
#define LINUX_MAP_SHARED	0x0001
#define LINUX_MAP_PRIVATE	0x0002
#define LINUX_MAP_FIXED		0x0010
#define LINUX_MAP_ANON		0x0020

/* SystemV ipc defines */
#define LINUX_SEMOP		1
#define LINUX_SEMGET		2
#define LINUX_SEMCTL		3
#define LINUX_MSGSND		11
#define LINUX_MSGRCV		12
#define LINUX_MSGGET		13
#define LINUX_MSGCTL		14
#define LINUX_SHMAT		21
#define LINUX_SHMDT		22
#define LINUX_SHMGET		23
#define LINUX_SHMCTL		24

#define LINUX_IPC_RMID		0
#define LINUX_IPC_SET		1
#define LINUX_IPC_STAT		2
#define LINUX_IPC_INFO		3

#define LINUX_SHM_LOCK		11
#define LINUX_SHM_UNLOCK	12
#define LINUX_SHM_STAT		13
#define LINUX_SHM_INFO		14

#define LINUX_SHM_RDONLY	0x1000
#define LINUX_SHM_RND		0x2000
#define LINUX_SHM_REMAP		0x4000

/* Socket defines */
#define LINUX_SOCKET 		1
#define LINUX_BIND		2
#define LINUX_CONNECT 		3
#define LINUX_LISTEN 		4
#define LINUX_ACCEPT 		5
#define LINUX_GETSOCKNAME	6
#define LINUX_GETPEERNAME	7
#define LINUX_SOCKETPAIR	8
#define LINUX_SEND		9
#define LINUX_RECV		10
#define LINUX_SENDTO 		11
#define LINUX_RECVFROM 		12
#define LINUX_SHUTDOWN 		13
#define LINUX_SETSOCKOPT	14
#define LINUX_GETSOCKOPT	15

#define LINUX_AF_UNSPEC		0
#define LINUX_AF_UNIX		1
#define LINUX_AF_INET		2
#define LINUX_AF_AX25		3
#define LINUX_AF_IPX		4
#define LINUX_AF_APPLETALK	5

#define LINUX_SOL_SOCKET	1
#define LINUX_SOL_IP		0
#define LINUX_SOL_IPX		256
#define LINUX_SOL_AX25		257
#define LINUX_SOL_TCP		6
#define LINUX_SOL_UDP		17

#define LINUX_SO_DEBUG		1
#define LINUX_SO_REUSEADDR	2
#define LINUX_SO_TYPE		3
#define LINUX_SO_ERROR		4
#define LINUX_SO_DONTROUTE	5
#define LINUX_SO_BROADCAST	6
#define LINUX_SO_SNDBUF		7
#define LINUX_SO_RCVBUF		8
#define LINUX_SO_KEEPALIVE	9
#define LINUX_SO_OOBINLINE	10
#define LINUX_SO_NO_CHECK	11
#define LINUX_SO_PRIORITY	12
#define LINUX_SO_LINGER		13

#define LINUX_IP_TOS		1
#define LINUX_IP_TTL		2

/* Sound system defines */
#define LINUX_SNDCTL_DSP_RESET		0x5000
#define LINUX_SNDCTL_DSP_SYNC		0x5001
#define LINUX_SNDCTL_DSP_SPEED		0x5002
#define LINUX_SNDCTL_DSP_STEREO		0x5003
#define LINUX_SNDCTL_DSP_GETBLKSIZE	0x5004
#define LINUX_SNDCTL_DSP_SETBLKSIZE	0x5004
#define LINUX_SNDCTL_DSP_SETFMT		0x5005
#define LINUX_SOUND_PCM_WRITE_CHANNELS	0x5006
#define LINUX_SOUND_PCM_WRITE_FILTER	0x5007
#define LINUX_SNDCTL_DSP_POST		0x5008
#define LINUX_SNDCTL_DSP_SUBDIVIDE	0x5009
#define LINUX_SNDCTL_DSP_SETFRAGMENT	0x500A
#define LINUX_SNDCTL_DSP_GETFMTS	0x500B
#define LINUX_SNDCTL_DSP_GETOSPACE	0x500C
#define LINUX_SNDCTL_DSP_GETISPACE	0x500D
#define LINUX_SNDCTL_DSP_NONBLOCK	0x500E

#ifdef KERNEL
caddr_t	ua_alloc_init __P((int len));
caddr_t	ua_alloc __P((int len));
#endif

#endif /* !_I386_LINUX_LINUX_H_ */
