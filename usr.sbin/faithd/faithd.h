/*	$KAME: faithd.h,v 1.8 2001/09/05 03:04:21 itojun Exp $	*/

/*
 * Copyright (C) 1997 and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

extern char logname[];
extern int dflag;

extern void tcp_relay __P((int, int, const char *));
extern void ftp_relay __P((int, int));
extern int ftp_active __P((int, int, int *, int *));
extern int ftp_passive __P((int, int, int *, int *));
extern void rsh_relay __P((int, int));
extern void rsh_dual_relay __P((int, int));
extern void exit_success __P((const char *, ...))
	__attribute__((__format__(__printf__, 1, 2)));
extern void exit_failure __P((const char *, ...))
	__attribute__((__format__(__printf__, 1, 2)));

#define DEFAULT_PORT_NAME	"telnet"
#define DEFAULT_DIR	"/usr/libexec"
#define DEFAULT_NAME	"telnetd"
#define DEFAULT_PATH	(DEFAULT_DIR "/" DEFAULT_NAME)

#define FTP_PORT	21
#define RLOGIN_PORT	513
#define RSH_PORT	514

#define RETURN_SUCCESS	0
#define RETURN_FAILURE	1

#define YES	1
#define NO	0

#define MSS	2048
#define MAXARGV	20

#define NUMPRT	0
#define NUMPRG	1
#define NUMARG	2

#define UC(b)	(((int)b)&0xff)

#define FAITH_TIMEOUT	(30 * 60)	/*second*/
