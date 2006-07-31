/*	$NetBSD: if_bridgevar.h,v 1.4 2003/07/08 07:13:50 itojun Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1999, 2000 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * OpenBSD: if_bridge.h,v 1.14 2001/03/22 03:48:29 jason Exp
 *
 * $FreeBSD$
 */

/*
 * Data structure and control definitions for STP interfaces.
 */

#include <sys/callout.h>
#include <sys/queue.h>

/* STP port states */
#define	BSTP_IFSTATE_DISABLED	0
#define	BSTP_IFSTATE_LISTENING	1
#define	BSTP_IFSTATE_LEARNING	2
#define	BSTP_IFSTATE_FORWARDING	3
#define	BSTP_IFSTATE_BLOCKING	4

#ifdef _KERNEL

/*
 * Spanning tree defaults.
 */
#define	BSTP_DEFAULT_MAX_AGE		(20 * 256)
#define	BSTP_DEFAULT_HELLO_TIME		(2 * 256)
#define	BSTP_DEFAULT_FORWARD_DELAY	(15 * 256)
#define	BSTP_DEFAULT_HOLD_TIME		(1 * 256)
#define	BSTP_DEFAULT_BRIDGE_PRIORITY	0x8000
#define	BSTP_DEFAULT_PORT_PRIORITY	0x80
#define	BSTP_DEFAULT_PATH_COST		55

/* BPDU message types */
#define	BSTP_MSGTYPE_CFG	0x00		/* Configuration */
#define	BSTP_MSGTYPE_TCN	0x80		/* Topology chg notification */

/* BPDU flags */
#define	BSTP_FLAG_TC		0x01		/* Topology change */
#define	BSTP_FLAG_TCA		0x80		/* Topology change ack */

#define	BSTP_MESSAGE_AGE_INCR	(1 * 256)	/* in 256ths of a second */
#define	BSTP_TICK_VAL		(1 * 256)	/* in 256ths of a second */
#define	BSTP_LINK_TIMER		(BSTP_TICK_VAL * 30)

/*
 * Because BPDU's do not make nicely aligned structures, two different
 * declarations are used: bstp_?bpdu (wire representation, packed) and
 * bstp_*_unit (internal, nicely aligned version).
 */

/* configuration bridge protocol data unit */
struct bstp_cbpdu {
	uint8_t		cbu_dsap;		/* LLC: destination sap */
	uint8_t		cbu_ssap;		/* LLC: source sap */
	uint8_t		cbu_ctl;		/* LLC: control */
	uint16_t	cbu_protoid;		/* protocol id */
	uint8_t		cbu_protover;		/* protocol version */
	uint8_t		cbu_bpdutype;		/* message type */
	uint8_t		cbu_flags;		/* flags (below) */

	/* root id */
	uint16_t	cbu_rootpri;		/* root priority */
	uint8_t	cbu_rootaddr[6];	/* root address */

	uint32_t	cbu_rootpathcost;	/* root path cost */

	/* bridge id */
	uint16_t	cbu_bridgepri;		/* bridge priority */
	uint8_t		cbu_bridgeaddr[6];	/* bridge address */

	uint16_t	cbu_portid;		/* port id */
	uint16_t	cbu_messageage;		/* current message age */
	uint16_t	cbu_maxage;		/* maximum age */
	uint16_t	cbu_hellotime;		/* hello time */
	uint16_t	cbu_forwarddelay;	/* forwarding delay */
} __attribute__((__packed__));

/* topology change notification bridge protocol data unit */
struct bstp_tbpdu {
	uint8_t		tbu_dsap;		/* LLC: destination sap */
	uint8_t		tbu_ssap;		/* LLC: source sap */
	uint8_t		tbu_ctl;		/* LLC: control */
	uint16_t	tbu_protoid;		/* protocol id */
	uint8_t		tbu_protover;		/* protocol version */
	uint8_t		tbu_bpdutype;		/* message type */
} __attribute__((__packed__));

/*
 * Timekeeping structure used in spanning tree code.
 */
struct bstp_timer {
	uint16_t	active;
	uint16_t	value;
};

struct bstp_config_unit {
	uint64_t	cu_rootid;
	uint64_t	cu_bridge_id;
	uint32_t	cu_root_path_cost;
	uint16_t	cu_message_age;
	uint16_t	cu_max_age;
	uint16_t	cu_hello_time;
	uint16_t	cu_forward_delay;
	uint16_t	cu_port_id;
	uint8_t		cu_message_type;
	uint8_t		cu_topology_change_acknowledgment;
	uint8_t		cu_topology_change;
};

struct bstp_tcn_unit {
	uint8_t		tu_message_type;
};

/*
 * Bridge interface list entry.
 */
struct bstp_port {
	LIST_ENTRY(bstp_port)	bp_next;
	struct ifnet		*bp_ifp;	/* parent if */
	struct bstp_state	*bp_bs;
	int			bp_active;
	uint64_t		bp_designated_root;
	uint64_t		bp_designated_bridge;
	uint32_t		bp_path_cost;
	uint32_t		bp_designated_cost;
	struct bstp_timer	bp_hold_timer;
	struct bstp_timer	bp_message_age_timer;
	struct bstp_timer	bp_forward_delay_timer;
	struct bstp_config_unit	bp_config_bpdu;
	uint16_t		bp_port_id;
	uint16_t		bp_designated_port;
	uint8_t			bp_state;
	uint8_t			bp_topology_change_acknowledge;
	uint8_t			bp_config_pending;
	uint8_t			bp_change_detection_enabled;
	uint8_t			bp_priority;
	uint32_t		bp_forward_transitions;
};

/*
 * Software state for each bridge STP.
 */
struct bstp_state {
	LIST_ENTRY(bstp_state)	bs_list;
	struct mtx		bs_mtx;
	uint64_t		bs_designated_root;
	uint64_t		bs_bridge_id;
	struct bstp_port	*bs_root_port;
	uint32_t		bs_root_path_cost;
	uint16_t		bs_max_age;
	uint16_t		bs_hello_time;
	uint16_t		bs_forward_delay;
	uint16_t		bs_bridge_max_age;
	uint16_t		bs_bridge_hello_time;
	uint16_t		bs_bridge_forward_delay;
	uint16_t		bs_topology_change_time;
	uint16_t		bs_hold_time;
	uint16_t		bs_bridge_priority;
	uint8_t			bs_topology_change_detected;
	uint8_t			bs_topology_change;
	struct bstp_timer	bs_hello_timer;
	struct bstp_timer	bs_topology_change_timer;
	struct bstp_timer	bs_tcn_timer;
	struct callout		bs_bstpcallout;	/* STP callout */
	struct bstp_timer	bs_link_timer;
	struct timeval		bs_last_tc_time;
	LIST_HEAD(, bstp_port)	bs_bplist;
};

#define BSTP_LOCK_INIT(_bs)	mtx_init(&(_bs)->bs_mtx, "bstp", \
					    NULL, MTX_DEF)
#define BSTP_LOCK_DESTROY(_bs)	mtx_destroy(&(_bs)->bs_mtx)
#define BSTP_LOCK(_bs)		mtx_lock(&(_bs)->bs_mtx)
#define BSTP_UNLOCK(_bs)	mtx_unlock(&(_bs)->bs_mtx)
#define BSTP_LOCK_ASSERT(_bs)	mtx_assert(&(_bs)->bs_mtx, MA_OWNED)

extern const uint8_t bstp_etheraddr[];

extern	void (*bstp_linkstate_p)(struct ifnet *ifp, int state);

void	bstp_attach(struct bstp_state *);
void	bstp_detach(struct bstp_state *);
void	bstp_init(struct bstp_state *);
void	bstp_reinit(struct bstp_state *);
void	bstp_stop(struct bstp_state *);
int	bstp_add(struct bstp_state *, struct bstp_port *, struct ifnet *);
void	bstp_delete(struct bstp_port *);
void	bstp_linkstate(struct ifnet *, int);
struct mbuf *bstp_input(struct bstp_port *, struct ifnet *, struct mbuf *);

#endif /* _KERNEL */
