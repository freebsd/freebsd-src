/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * $FreeBSD: src/usr.sbin/pim6sd/var.h,v 1.1.2.1 2000/07/15 07:36:37 kris Exp $
 */
/* YIPS @(#)$Id: var.h,v 1.1 1999/10/29 09:04:54 jinmei Exp $ */

#if !defined(_VAR_H_)
#define _VAR_H_

#include <sys/socket.h>

#define MAX3(a,b,c) (a > b ? (a > c ? a : c) : (b > c ? b : c))

#define CALLOC(size, cast) (cast)calloc(1, (size))

#define ISSET(exp, bit) (((exp) & (bit)) == (bit))

#define ATOX(c) \
    (isdigit(c) ? (c - '0') : (isupper(c) ? (c - 'A' + 10) : (c - 'a' + 10) ))

#define LALIGN(a) \
    ((a) > 0 ? ((a) &~ (sizeof(long) - 1)) : sizeof(long))

#define RNDUP(a) \
    ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

#define BUFADDRSIZE 128
#define INET_NTOP(addr, buf) \
	inet_ntop(((struct sockaddr *)(addr))->sa_family, _INADDRBYSA(addr), buf, sizeof(buf))

#define GETNAMEINFO(x, y, z) \
	getnameinfo((x), (x)->sa_len, (y), sizeof(y), (z), sizeof(z), \
		NI_NUMERICHOST | NI_NUMERICSERV)

#define ARRAYSIZE(a) (sizeof(a)/sizeof(a[0]))
#endif /*!defined(_VAR_H_)*/
