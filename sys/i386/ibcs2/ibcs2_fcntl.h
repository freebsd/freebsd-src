/*	$NetBSD: ibcs2_fcntl.h,v 1.2 1994/10/26 02:52:54 cgd Exp $	*/

/*
 * Copyright (c) 1994 Scott Bartram
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Scott Bartram.
 * 4. The name of the author may not be used to endorse or promote products
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

#ifndef _IBCS2_FCNTL_H
#define _IBCS2_FCNTL_H 1

#include <compat/ibcs2/ibcs2_types.h>

#define IBCS2_O_RDONLY		0x0000
#define IBCS2_O_WRONLY		0x0001
#define IBCS2_O_RDWR		0x0002
#define IBCS2_O_NDELAY		0x0004
#define IBCS2_O_APPEND		0x0008
#define IBCS2_O_SYNC		0x0010
#define IBCS2_O_NONBLOCK	0x0080
#define IBCS2_O_CREAT		0x0100
#define IBCS2_O_TRUNC		0x0200
#define IBCS2_O_EXCL		0x0400
#define IBCS2_O_NOCTTY		0x0800

#define IBCS2_F_DUPFD         0
#define IBCS2_F_GETFD         1
#define IBCS2_F_SETFD         2
#define IBCS2_F_GETFL         3
#define IBCS2_F_SETFL         4
#define IBCS2_F_GETLK         5
#define IBCS2_F_SETLK         6
#define IBCS2_F_SETLKW        7

struct ibcs2_flock {
        short   	l_type;
        short   	l_whence;
        ibcs2_off_t	l_start;
        ibcs2_off_t	l_len;
        short   	l_sysid;
        ibcs2_pid_t	l_pid;
};
#define ibcs2_flock_len	(sizeof(struct ibcs2_flock))

#define IBCS2_F_RDLCK		1
#define IBCS2_F_WRLCK		2
#define IBCS2_F_UNLCK		3

#define IBCS2_O_ACCMODE		3
#define IBCS2_FD_CLOEXEC	1

#endif /* _IBCS2_FCNTL_H */
