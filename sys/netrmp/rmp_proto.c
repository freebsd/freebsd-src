/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: rmp_proto.c 1.3 89/06/07$
 *
 *	From: @(#)rmp_proto.c	7.1 (Berkeley) 5/8/90
 *	$Id: rmp_proto.c,v 1.2 1993/12/19 00:54:08 wollman Exp $
 */

#include "param.h"
#include "systm.h"
#include "socket.h"
#include "protosw.h"
#include "domain.h"

#include "rmp.h"

#ifdef RMP
/*
 * HP Remote Maintenance Protocol (RMP) family: BOOT
 */

extern	struct domain	rmpdomain;
extern	int		raw_usrreq(), rmp_output();

struct protosw rmpsw[] = {
  {	SOCK_RAW,	&rmpdomain,	RMPPROTO_BOOT,	PR_ATOMIC|PR_ADDR,
	0,		rmp_output,	0,		0,
	raw_usrreq,
	0,		0,		0,		0,
  },
};

struct domain rmpdomain = {
	AF_RMP, "RMP", 0, 0, 0, rmpsw, &rmpsw[sizeof(rmpsw)/sizeof(rmpsw[0])]
};

#endif
