/*	$NetBSD: natm_proto.c,v 1.3 1996/09/18 00:56:41 chuck Exp $	*/
/*
 *
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * protocol layer for access to native mode ATM
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>

#include <net/if.h>
#include <net/netisr.h>

#include <netinet/in.h>

#include <netnatm/natm.h>

extern	struct domain natmdomain;

static	void natm_init(void);

static struct protosw natmsw[] = {
{ SOCK_STREAM,	&natmdomain,	PROTO_NATMAAL5, PR_CONNREQUIRED,
  0,	0,	0,	0,
#ifdef FREEBSD_USRREQS
  0,
#else
  natm_usrreq,
#endif
  0,	0,	0,	0,	
#if defined(__NetBSD__) || defined(__OpenBSD__)
	natm5_sysctl
#elif defined(FREEBSD_USRREQS)
        &natm_usrreqs
#endif
},
{ SOCK_DGRAM,	&natmdomain,	PROTO_NATMAAL5,	PR_CONNREQUIRED | PR_ATOMIC,
  0,	0,	0,	0,
#ifdef FREEBSD_USRREQS
  0,
#else
  natm_usrreq,
#endif
  0,	0,	0,	0,	
#if defined(__NetBSD__) || defined(__OpenBSD__)
	natm5_sysctl
#elif defined(FREEBSD_USRREQS)
        &natm_usrreqs
#endif
},
{ SOCK_STREAM,	&natmdomain,	PROTO_NATMAAL0, PR_CONNREQUIRED,
  0,	0,	0,	0,
#ifdef FREEBSD_USRREQS
  0,
#else
  natm_usrreq,
#endif
  0,	0,	0,	0,	
#if defined(__NetBSD__) || defined(__OpenBSD__)
	natm0_sysctl
#elif defined(FREEBSD_USRREQS)
        &natm_usrreqs
#endif
},
};

static struct domain natmdomain =
    { AF_NATM, "natm", natm_init, 0, 0, 
      natmsw, &natmsw[sizeof(natmsw)/sizeof(natmsw[0])], 0,
      0, 0, 0};

static int natmqmaxlen = IFQ_MAXLEN;	/* max # of packets on queue */
static struct ifqueue natmintrq;
#ifdef NATM_STAT
u_int natm_sodropcnt = 0;		/* # mbufs dropped due to full sb */
u_int natm_sodropbytes = 0;		/* # of bytes dropped */
u_int natm_sookcnt = 0;			/* # mbufs ok */
u_int natm_sookbytes = 0;		/* # of bytes ok */
#endif


static void natm_init()

{
  LIST_INIT(&natm_pcbs);
  bzero(&natmintrq, sizeof(natmintrq));
  natmintrq.ifq_maxlen = natmqmaxlen;
  mtx_init(&natmintrq.ifq_mtx, "natm_inq", NULL, MTX_DEF);
  netisr_register(NETISR_NATM, natmintr, &natmintrq);
}

#if defined(__FreeBSD__)
DOMAIN_SET(natm);
#endif
