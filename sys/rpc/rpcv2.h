/* $FreeBSD$ */
/*	$OpenBSD: rpcv2.h,v 1.5 2002/06/11 15:45:44 hin Exp $	*/
/*	$NetBSD: rpcv2.h,v 1.8 1996/02/18 11:54:11 fvdl Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)rpcv2.h	8.2 (Berkeley) 3/30/95
 */


#ifndef _NFS_RPCV2_H_
#define _NFS_RPCV2_H_

/*
 * Definitions for Sun RPC Version 2, from
 * "RPC: Remote Procedure Call Protocol Specification" RFC1057
 */

/* Version # */
#define	RPC_VER2	2

/* Authentication */
#define	RPCAUTH_NULL	0
#define	RPCAUTH_UNIX	1
#define	RPCAUTH_SHORT	2
#define RPCAUTH_KERB4	4
#define	RPCAUTH_MAXSIZ	400
#define	RPCVERF_MAXSIZ	12	/* For Kerb, can actually be 400 */
#define	RPCAUTH_UNIXGIDS 16

/* Rpc Constants */

/* msg type */
#define	RPC_CALL	0
#define	RPC_REPLY	1

/* reply stat */
#define	RPC_MSGACCEPTED	0
#define	RPC_MSGDENIED	1

/* accepted stat: */
#define RPC_SUCCESS	0
#define	RPC_PROGUNAVAIL	1
#define	RPC_PROGMISMATCH	2
#define	RPC_PROCUNAVAIL	3
#define	RPC_GARBAGE	4		/* I like this one */
#define RPC_SYSTEMERR	5

/* reject stat */
#define	RPC_MISMATCH	0
#define	RPC_AUTHERR	1

/* Authentication failures */
#define AUTH_OK		0
#define	AUTH_BADCRED	1
#define	AUTH_REJECTCRED	2
#define	AUTH_BADVERF	3
#define	AUTH_REJECTVERF	4
#define	AUTH_TOOWEAK	5		/* Give em wheaties */


/* Sizes of rpc header parts */
#define	RPC_SIZ		24
#define	RPC_REPLYSIZ	28

typedef u_char			NFSKERBKEY_T[2];

#endif
