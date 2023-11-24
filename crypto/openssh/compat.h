/* $OpenBSD: compat.h,v 1.62 2023/03/06 12:14:48 dtucker Exp $ */

/*
 * Copyright (c) 1999, 2000, 2001 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef COMPAT_H
#define COMPAT_H

#define SSH_BUG_UTF8TTYMODE	0x00000001
#define SSH_BUG_SIGTYPE		0x00000002
#define SSH_BUG_SIGTYPE74	0x00000004
/* #define unused		0x00000008 */
#define SSH_OLD_SESSIONID	0x00000010
/* #define unused		0x00000020 */
#define SSH_BUG_DEBUG		0x00000040
/* #define unused		0x00000080 */
/* #define unused		0x00000100 */
/* #define unused		0x00000200 */
/* #define unused		0x00000400 */
#define SSH_BUG_SCANNER		0x00000800
/* #define unused		0x00001000 */
/* #define unused		0x00002000 */
#define SSH_OLD_DHGEX		0x00004000
#define SSH_BUG_NOREKEY		0x00008000
/* #define unused		0x00010000 */
/* #define unused		0x00020000 */
/* #define unused		0x00040000 */
/* #define unused		0x00100000 */
#define SSH_BUG_EXTEOF		0x00200000
#define SSH_BUG_PROBE		0x00400000
/* #define unused		0x00800000 */
#define SSH_OLD_FORWARD_ADDR	0x01000000
/* #define unused		0x02000000 */
#define SSH_NEW_OPENSSH		0x04000000
#define SSH_BUG_DYNAMIC_RPORT	0x08000000
#define SSH_BUG_CURVE25519PAD	0x10000000
#define SSH_BUG_HOSTKEYS	0x20000000
#define SSH_BUG_DHGEX_LARGE	0x40000000

struct ssh;

void    compat_banner(struct ssh *, const char *);
char	*compat_kex_proposal(struct ssh *, const char *);
#endif
