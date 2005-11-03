/*-
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
 * $FreeBSD: src/sys/compat/svr4/svr4_wait.h,v 1.4 2005/01/05 22:34:37 imp Exp $
 */

#ifndef	_SVR4_WAIT_H_
#define	_SVR4_WAIT_H_


#define SVR4_P_PID	0
#define SVR4_P_PPID	1
#define SVR4_P_PGID	2
#define SVR4_P_SID	3
#define SVR4_P_CID	4
#define SVR4_P_UID	5
#define SVR4_P_GID	6
#define SVR4_P_ALL	7

#define SVR4_WEXITED	0x01
#define SVR4_WTRAPPED	0x02
#define SVR4_WSTOPPED	0x04
#define SVR4_WCONTINUED	0x08
#define SVR4_WUNDEF1	0x10
#define SVR4_WUNDEF2	0x20
#define SVR4_WNOHANG	0x40
#define SVR4_WNOWAIT	0x80

#define SVR4_WOPTMASK   (SVR4_WEXITED|SVR4_WTRAPPED|SVR4_WSTOPPED|\
			 SVR4_WCONTINUED|SVR4_WNOHANG|SVR4_WNOWAIT)

#endif /* !_SVR4_WAIT_H_ */
