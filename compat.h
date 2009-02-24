/* $OpenBSD: compat.h,v 1.42 2008/09/11 14:22:37 markus Exp $ */

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

#define	SSH_PROTO_UNKNOWN	0x00
#define	SSH_PROTO_1		0x01
#define	SSH_PROTO_1_PREFERRED	0x02
#define	SSH_PROTO_2		0x04

#define SSH_BUG_SIGBLOB		0x00000001
#define SSH_BUG_PKSERVICE	0x00000002
#define SSH_BUG_HMAC		0x00000004
#define SSH_BUG_X11FWD		0x00000008
#define SSH_OLD_SESSIONID	0x00000010
#define SSH_BUG_PKAUTH		0x00000020
#define SSH_BUG_DEBUG		0x00000040
#define SSH_BUG_BANNER		0x00000080
#define SSH_BUG_IGNOREMSG	0x00000100
#define SSH_BUG_PKOK		0x00000200
#define SSH_BUG_PASSWORDPAD	0x00000400
#define SSH_BUG_SCANNER		0x00000800
#define SSH_BUG_BIGENDIANAES	0x00001000
#define SSH_BUG_RSASIGMD5	0x00002000
#define SSH_OLD_DHGEX		0x00004000
#define SSH_BUG_NOREKEY		0x00008000
#define SSH_BUG_HBSERVICE	0x00010000
#define SSH_BUG_OPENFAILURE	0x00020000
#define SSH_BUG_DERIVEKEY	0x00040000
#define SSH_BUG_DUMMYCHAN	0x00100000
#define SSH_BUG_EXTEOF		0x00200000
#define SSH_BUG_PROBE		0x00400000
#define SSH_BUG_FIRSTKEX	0x00800000
#define SSH_OLD_FORWARD_ADDR	0x01000000
#define SSH_BUG_RFWD_ADDR	0x02000000
#define SSH_NEW_OPENSSH		0x04000000

void     enable_compat13(void);
void     enable_compat20(void);
void     compat_datafellows(const char *);
int	 proto_spec(const char *);
char	*compat_cipher_proposal(char *);

extern int compat13;
extern int compat20;
extern int datafellows;
#endif
