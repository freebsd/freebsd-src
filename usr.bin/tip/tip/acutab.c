/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)acutab.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include "tipconf.h"
#include "tip.h"

extern int df02_dialer(), df03_dialer(),
	   biz31f_dialer(),
	   biz31w_dialer(),
	   biz22f_dialer(),
	   biz22w_dialer(),
	   ven_dialer(),
	   hay_dialer(),
	   cour_dialer(),
	   multitech_dialer(),
	   t3000_dialer(),
	   v3451_dialer(),
	   v831_dialer(),
	   dn_dialer();

extern void df_disconnect(), df_abort(),
	   biz31_disconnect(), biz31_abort(),
	   biz22_disconnect(), biz22_abort(),
	   ven_disconnect(), ven_abort(),
	   hay_disconnect(), hay_abort(),
	   cour_disconnect(), cour_abort(),
	   multitech_disconnect(), multitech_abort(),
	   t3000_disconnect(), t3000_abort(),
	   v3451_disconnect(), v3451_abort(),
	   v831_disconnect(), v831_abort(),
	   dn_disconnect(), dn_abort();

acu_t acutable[] = {
#if BIZ1031
	"biz31f", biz31f_dialer, biz31_disconnect,	biz31_abort,
	"biz31w", biz31w_dialer, biz31_disconnect,	biz31_abort,
#endif
#if BIZ1022
	"biz22f", biz22f_dialer, biz22_disconnect,	biz22_abort,
	"biz22w", biz22w_dialer, biz22_disconnect,	biz22_abort,
#endif
#if DF02
	"df02",	df02_dialer,	df_disconnect,		df_abort,
#endif
#if DF03
	"df03",	df03_dialer,	df_disconnect,		df_abort,
#endif
#if DN11
	"dn11",	dn_dialer,	dn_disconnect,		dn_abort,
#endif
#if VENTEL
	"ventel",ven_dialer,	ven_disconnect,		ven_abort,
#endif
#if HAYES
	"hayes",hay_dialer,	hay_disconnect,		hay_abort,
#endif
#if COURIER
	"courier",cour_dialer,	cour_disconnect,	cour_abort,
#endif
#if MULTITECH
	"multitech",multitech_dialer,	multitech_disconnect,	multitech_abort,
#endif
#if T3000
	"t3000",t3000_dialer,	t3000_disconnect,	t3000_abort,
#endif
#if V3451
#if !V831
	"vadic",v3451_dialer,	v3451_disconnect,	v3451_abort,
#endif
	"v3451",v3451_dialer,	v3451_disconnect,	v3451_abort,
#endif
#if V831
#if !V3451
	"vadic",v831_dialer,	v831_disconnect,	v831_abort,
#endif
	"v831",v831_dialer,	v831_disconnect,	v831_abort,
#endif
	0,	0,		0,			0
};

