/*
 * Copyright (c) University of British Columbia, 1984
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Laboratory for Computation Vision and the Computer Science Department
 * of the University of British Columbia.
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
 *	from: @(#)ccitt_proto.c	7.5 (Berkeley) 8/30/90
 *	$Id: ccitt_proto.c,v 1.3 1993/12/19 00:52:12 wollman Exp $
 */
#define HDLC
#include "param.h"
#include "systm.h"
#include "socket.h"
#include "protosw.h"
#include "domain.h"

#include "x25.h"

/*
 *	Definitions of protocols supported in the CCITT domain.
 */

#ifdef BSD4_3
extern	struct domain ccittdomain;
#define DOMAIN &ccittdomain
#else
#define DOMAIN PF_CCITT
#endif

#ifdef XE
int	xe_output ();
void xe_ctlinput ();
void xe_init();
void xe_timer();
#endif
#ifdef HDLC
int	hd_output ();
void hd_ctlinput ();
void hd_init ();
void hd_timer ();
#endif
int	pk_usrreq ();
void pk_timer ();
int pk_ctloutput ();
void pk_init ();

struct protosw ccittsw[] = {
#ifdef XE
 {	0,		DOMAIN,		IEEEPROTO_802LLC,0,
	0,		xe_output,	xe_ctlinput,	0,
	0,
	xe_init,	0,	 	xe_timer,	0,
 },
#endif
#ifdef HDLC
 {	0,		DOMAIN,		CCITTPROTO_HDLC,0,
	0,		hd_output,	hd_ctlinput,	0,
	0,
	hd_init,	0,	 	hd_timer,	0,
 },
#endif
 {	SOCK_STREAM,	DOMAIN,		CCITTPROTO_X25,	PR_CONNREQUIRED|PR_ATOMIC|PR_WANTRCVD,
	0,		0,		0,		pk_ctloutput,
	pk_usrreq,
	pk_init,	0,		pk_timer,	0,
 }
};

struct domain ccittdomain =
#ifdef BSD4_3
	{ AF_CCITT, "ccitt", 0, 0, 0, ccittsw,
		&ccittsw[sizeof(ccittsw)/sizeof(ccittsw[0])] };
#else
	{ AF_CCITT, "ccitt", ccittsw, &ccittsw[sizeof(ccittsw)/sizeof(ccittsw[0])] };
#endif
