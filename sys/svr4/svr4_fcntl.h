/*
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

#ifndef	_SVR4_FCNTL_H_
#define	_SVR4_FCNTL_H_

#include <svr4/svr4_types.h>
#include <sys/fcntl.h>

#define	SVR4_O_RDONLY		0x0000
#define	SVR4_O_WRONLY		0x0001
#define	SVR4_O_RDWR		0x0002
#define	SVR4_O_ACCMODE		0x0003
#define	SVR4_O_NDELAY		0x0004
#define	SVR4_O_APPEND		0x0008
#define	SVR4_O_SYNC		0x0010
#define	SVR4_O_NONBLOCK		0x0080
#define	SVR4_O_CREAT		0x0100
#define	SVR4_O_TRUNC		0x0200
#define	SVR4_O_EXCL		0x0400
#define	SVR4_O_NOCTTY		0x0800
#define	SVR4_O_PRIV		0x1000


#define	SVR4_FD_CLOEXEC		1

#define	SVR4_F_DUPFD		0
#define	SVR4_F_GETFD		1
#define	SVR4_F_SETFD		2
#define	SVR4_F_GETFL		3
#define	SVR4_F_SETFL		4
#define	SVR4_F_GETLK_SVR3	5
#define	SVR4_F_SETLK		6
#define	SVR4_F_SETLKW		7
#define	SVR4_F_CHKFL		8
#define SVR4_F_DUP2FD		9
#define	SVR4_F_ALLOCSP		10
#define	SVR4_F_FREESP		11

#define SVR4_F_ISSTREAM		13
#define	SVR4_F_GETLK		14
#define	SVR4_F_PRIV		15
#define	SVR4_F_NPRIV		16
#define	SVR4_F_QUOTACTL		17
#define	SVR4_F_BLOCKS		18
#define	SVR4_F_BLKSIZE		19
#define SVR4_F_RSETLK		20
#define SVR4_F_RGETLK		21
#define SVR4_F_RSETLKW		22
#define	SVR4_F_GETOWN		23
#define	SVR4_F_SETOWN		24
#define	SVR4_F_REVOKE		25
#define SVR4_F_HASREMOTELOCKS	26
#define SVR4_F_FREESP64		27

#define SVR4_F_GETLK64		33
#define SVR4_F_SETLK64		34
#define SVR4_F_SETLKW64		35

#define SVR4_F_SHARE		40
#define SVR4_F_UNSHARE		41

#define SVR4_F_CHSIZE_XENIX	0x6000
#define SVR4_F_RDCHK_XENIX	0x6001
#define SVR4_F_LK_UNLCK_XENIX	0x6300
#define SVR4_F_LK_LOCK_XENIX	0x7200
#define SVR4_F_LK_NBLCK_XENIX	0x6200
#define SVR4_F_LK_RLCK_XENIX	0x7100
#define SVR4_F_LK_NBRLCK_XENIX	0x6100

#define SVR4_LK_CMDTYPE(x)   (((x) >> 12) & 0x7)
#define SVR4_LK_LCKTYPE(x)   (((x) >> 8) & 0x7)

#define	SVR4_F_RDLCK	1
#define	SVR4_F_WRLCK	2
#define	SVR4_F_UNLCK	3

struct svr4_flock_svr3 {
	short		l_type;
	short		l_whence;
	svr4_off_t	l_start;
	svr4_off_t	l_len;
	short		l_sysid;
	svr4_o_pid_t	l_pid;
};


struct svr4_flock {
	short		l_type;
	short		l_whence;
	svr4_off_t	l_start;
	svr4_off_t	l_len;
	long		l_sysid;
	svr4_pid_t	l_pid;
	long		pad[4];
};

struct svr4_flock64 {
	short		l_type;
	short		l_whence;
	svr4_off64_t	l_start;
	svr4_off64_t	l_len;
	long		l_sysid;
	svr4_pid_t	l_pid;
	long		pad[4];
};
#endif /* !_SVR4_FCNTL_H_ */
