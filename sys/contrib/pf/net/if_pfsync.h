/*	$OpenBSD: if_pfsync.h,v 1.2 2002/12/11 18:31:26 mickey Exp $	*/

/*
 * Copyright (c) 2001 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _NET_IF_PFSYNC_H_
#define _NET_IF_PFSYNC_H_

#ifdef _KERNEL
struct pfsync_softc {
	struct ifnet	sc_if;

	struct timeout	sc_tmo;
	struct mbuf	*sc_mbuf;	/* current cummulative mbuf */
	struct pf_state	*sc_ptr;	/* current ongoing state */
	int		 sc_count;	/* number of states in one mtu */
};
#endif

struct pfsync_header {
	u_int8_t version;
#define	PFSYNC_VERSION	1
	u_int8_t af;
	u_int8_t action;
#define	PFSYNC_ACT_CLR	0
#define	PFSYNC_ACT_INS	1
#define	PFSYNC_ACT_UPD	2
#define	PFSYNC_ACT_DEL	3
#define	PFSYNC_ACT_MAX	4
	u_int8_t count;
};

#define PFSYNC_HDRLEN	sizeof(struct pfsync_header)
#define	PFSYNC_ACTIONS \
	"CLR ST", "INS ST", "UPD ST", "DEL ST"

#define pf_state_peer_hton(s,d) do {		\
	(d)->seqlo = htonl((s)->seqlo);		\
	(d)->seqhi = htonl((s)->seqhi);		\
	(d)->seqdiff = htonl((s)->seqdiff);	\
	(d)->max_win = htons((s)->max_win);	\
	(d)->state = (s)->state;		\
} while (0)

#define pf_state_peer_ntoh(s,d) do {		\
	(d)->seqlo = ntohl((s)->seqlo);		\
	(d)->seqhi = ntohl((s)->seqhi);		\
	(d)->seqdiff = ntohl((s)->seqdiff);	\
	(d)->max_win = ntohs((s)->max_win);	\
	(d)->state = (s)->state;		\
} while (0)

#ifdef _KERNEL
int pfsync_clear_state(struct pf_state *);
int pfsync_pack_state(u_int8_t, struct pf_state *);
#define pfsync_insert_state(st)	pfsync_pack_state(PFSYNC_ACT_INS, (st))
#define pfsync_update_state(st)	pfsync_pack_state(PFSYNC_ACT_UPD, (st))
#define pfsync_delete_state(st)	pfsync_pack_state(PFSYNC_ACT_DEL, (st))
#endif

#endif /* _NET_IF_PFSYNC_H_ */
