/*
 * Copyright (c) 1995 Matt Thomas (thomas@lkg.dec.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 * $Id: if_fddi.h,v 1.4 1996/08/01 16:28:26 thomas Exp $
 */

#ifndef _NETINET_IF_FDDI_H_
#define _NETINET_IF_FDDI_H_

/*
 * Structure of an 100Mb/s FDDI header.
 */
struct	fddi_header {
	u_char	fddi_fc;
	u_char	fddi_dhost[6];
	u_char	fddi_shost[6];
};

#define	FDDIIPMTU		4352
#define	FDDIMTU			4470
#define	FDDIMIN			3

#define	FDDIFC_C		0x80	/* 0b10000000 */
#define	FDDIFC_L		0x40	/* 0b01000000 */
#define	FDDIFC_F		0x30	/* 0b00110000 */
#define	FDDIFC_Z		0x0F	/* 0b00001111 */

#define	FDDIFC_LLC_ASYNC	0x50
#define	FDDIFC_LLC_PRIO0	0
#define	FDDIFC_LLC_PRIO1	1
#define	FDDIFC_LLC_PRIO2	2
#define	FDDIFC_LLC_PRIO3	3
#define	FDDIFC_LLC_PRIO4	4
#define	FDDIFC_LLC_PRIO5	5
#define	FDDIFC_LLC_PRIO6	6
#define	FDDIFC_LLC_PRIO7	7
#define FDDIFC_LLC_SYNC         0xd0
#define	FDDIFC_SMT		0x40

#if defined(KERNEL) || defined(_KERNEL)
#define	fddibroadcastaddr	etherbroadcastaddr
#define	fddi_ipmulticast_min	ether_ipmulticast_min
#define	fddi_ipmulticast_max	ether_ipmulticast_max
#define	fddi_addmulti		ether_addmulti
#define	fddi_delmulti		ether_delmulti
#define	fddi_sprintf		ether_sprintf

void    fddi_ifattach __P((struct ifnet *));
void    fddi_input __P((struct ifnet *, struct fddi_header *, struct mbuf *));
int     fddi_output __P((struct ifnet *,
           struct mbuf *, struct sockaddr *, struct rtentry *)); 

#endif

#endif
