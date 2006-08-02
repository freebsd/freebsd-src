/*	$NetBSD: bridgestp.c,v 1.5 2003/11/28 08:56:48 keihan Exp $	*/

/*
 * Copyright (c) 2000 Jason L. Wright (jason@thought.net)
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
 *      This product includes software developed by Jason L. Wright
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
 * OpenBSD: bridgestp.c,v 1.5 2001/03/22 03:48:29 jason Exp
 */

/*
 * Implementation of the spanning tree protocol as defined in
 * ISO/IEC Final DIS 15802-3 (IEEE P802.1D/D17), May 25, 1998.
 * (In English: IEEE 802.1D, Draft 17, 1998)
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/callout.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_llc.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <net/bridgestp.h>

const uint8_t bstp_etheraddr[] = { 0x01, 0x80, 0xc2, 0x00, 0x00, 0x00 };

LIST_HEAD(, bstp_state) bstp_list;
static struct mtx	bstp_list_mtx;

static void	bstp_initialize_port(struct bstp_state *,
		    struct bstp_port *);
static void	bstp_ifupdstatus(struct bstp_state *, struct bstp_port *);
static void	bstp_enable_port(struct bstp_state *, struct bstp_port *);
static void	bstp_disable_port(struct bstp_state *,
		    struct bstp_port *);
#ifdef notused
static void	bstp_enable_change_detection(struct bstp_port *);
static void	bstp_disable_change_detection(struct bstp_port *);
#endif /* notused */
static int	bstp_root_bridge(struct bstp_state *bs);
static int	bstp_supersedes_port_info(struct bstp_state *,
		    struct bstp_port *, struct bstp_config_unit *);
static int	bstp_designated_port(struct bstp_state *,
		    struct bstp_port *);
static int	bstp_designated_for_some_port(struct bstp_state *);
static void	bstp_transmit_config(struct bstp_state *,
		    struct bstp_port *);
static void	bstp_transmit_tcn(struct bstp_state *);
static void	bstp_received_config_bpdu(struct bstp_state *,
		    struct bstp_port *, struct bstp_config_unit *);
static void	bstp_received_tcn_bpdu(struct bstp_state *,
		    struct bstp_port *, struct bstp_tcn_unit *);
static void	bstp_record_config_information(struct bstp_state *,
		    struct bstp_port *, struct bstp_config_unit *);
static void	bstp_record_config_timeout_values(struct bstp_state *,
		    struct bstp_config_unit *);
static void	bstp_config_bpdu_generation(struct bstp_state *);
static void	bstp_send_config_bpdu(struct bstp_state *,
		    struct bstp_port *, struct bstp_config_unit *);
static void	bstp_configuration_update(struct bstp_state *);
static void	bstp_root_selection(struct bstp_state *);
static void	bstp_designated_port_selection(struct bstp_state *);
static void	bstp_become_designated_port(struct bstp_state *,
		    struct bstp_port *);
static void	bstp_port_state_selection(struct bstp_state *);
static void	bstp_make_forwarding(struct bstp_state *,
		    struct bstp_port *);
static void	bstp_make_blocking(struct bstp_state *,
		    struct bstp_port *);
static void	bstp_set_port_state(struct bstp_port *, uint8_t);
static void	bstp_update_forward_transitions(struct bstp_port *);
#ifdef notused
static void	bstp_set_bridge_priority(struct bstp_state *, uint64_t);
static void	bstp_set_port_priority(struct bstp_state *,
		    struct bstp_port *, uint16_t);
static void	bstp_set_path_cost(struct bstp_state *,
		    struct bstp_port *, uint32_t);
#endif /* notused */
static void	bstp_topology_change_detection(struct bstp_state *);
static void	bstp_topology_change_acknowledged(struct bstp_state *);
static void	bstp_acknowledge_topology_change(struct bstp_state *,
		    struct bstp_port *);

static void	bstp_enqueue(struct ifnet *, struct mbuf *);
static void	bstp_tick(void *);
static void	bstp_timer_start(struct bstp_timer *, uint16_t);
static void	bstp_timer_stop(struct bstp_timer *);
static int	bstp_timer_expired(struct bstp_timer *, uint16_t);

static void	bstp_hold_timer_expiry(struct bstp_state *,
		    struct bstp_port *);
static void	bstp_message_age_timer_expiry(struct bstp_state *,
		    struct bstp_port *);
static void	bstp_forward_delay_timer_expiry(struct bstp_state *,
		    struct bstp_port *);
static void	bstp_topology_change_timer_expiry(struct bstp_state *);
static void	bstp_tcn_timer_expiry(struct bstp_state *);
static void	bstp_hello_timer_expiry(struct bstp_state *);
static int	bstp_addr_cmp(const uint8_t *, const uint8_t *);

static void
bstp_transmit_config(struct bstp_state *bs, struct bstp_port *bp)
{
	BSTP_LOCK_ASSERT(bs);

	if (bp->bp_hold_timer.active) {
		bp->bp_config_pending = 1;
		return;
	}

	bp->bp_config_bpdu.cu_message_type = BSTP_MSGTYPE_CFG;
	bp->bp_config_bpdu.cu_rootid = bs->bs_designated_root;
	bp->bp_config_bpdu.cu_root_path_cost = bs->bs_root_path_cost;
	bp->bp_config_bpdu.cu_bridge_id = bs->bs_bridge_id;
	bp->bp_config_bpdu.cu_port_id = bp->bp_port_id;

	if (bstp_root_bridge(bs))
		bp->bp_config_bpdu.cu_message_age = 0;
	else
		bp->bp_config_bpdu.cu_message_age =
		    bs->bs_root_port->bp_message_age_timer.value +
		    BSTP_MESSAGE_AGE_INCR;

	bp->bp_config_bpdu.cu_max_age = bs->bs_max_age;
	bp->bp_config_bpdu.cu_hello_time = bs->bs_hello_time;
	bp->bp_config_bpdu.cu_forward_delay = bs->bs_forward_delay;
	bp->bp_config_bpdu.cu_topology_change_acknowledgment
	    = bp->bp_topology_change_acknowledge;
	bp->bp_config_bpdu.cu_topology_change = bs->bs_topology_change;

	if (bp->bp_config_bpdu.cu_message_age < bs->bs_max_age) {
		bp->bp_topology_change_acknowledge = 0;
		bp->bp_config_pending = 0;
		bstp_send_config_bpdu(bs, bp, &bp->bp_config_bpdu);
		bstp_timer_start(&bp->bp_hold_timer, 0);
	}
}

static void
bstp_send_config_bpdu(struct bstp_state *bs, struct bstp_port *bp,
    struct bstp_config_unit *cu)
{
	struct ifnet *ifp;
	struct mbuf *m;
	struct ether_header *eh;
	struct bstp_cbpdu bpdu;

	BSTP_LOCK_ASSERT(bs);

	ifp = bp->bp_ifp;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;

	eh = mtod(m, struct ether_header *);

	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = sizeof(*eh) + sizeof(bpdu);
	m->m_len = m->m_pkthdr.len;

	bpdu.cbu_ssap = bpdu.cbu_dsap = LLC_8021D_LSAP;
	bpdu.cbu_ctl = LLC_UI;
	bpdu.cbu_protoid = htons(0);
	bpdu.cbu_protover = 0;
	bpdu.cbu_bpdutype = cu->cu_message_type;
	bpdu.cbu_flags = (cu->cu_topology_change ? BSTP_FLAG_TC : 0) |
	    (cu->cu_topology_change_acknowledgment ? BSTP_FLAG_TCA : 0);

	bpdu.cbu_rootpri = htons(cu->cu_rootid >> 48);
	bpdu.cbu_rootaddr[0] = cu->cu_rootid >> 40;
	bpdu.cbu_rootaddr[1] = cu->cu_rootid >> 32;
	bpdu.cbu_rootaddr[2] = cu->cu_rootid >> 24;
	bpdu.cbu_rootaddr[3] = cu->cu_rootid >> 16;
	bpdu.cbu_rootaddr[4] = cu->cu_rootid >> 8;
	bpdu.cbu_rootaddr[5] = cu->cu_rootid >> 0;

	bpdu.cbu_rootpathcost = htonl(cu->cu_root_path_cost);

	bpdu.cbu_bridgepri = htons(cu->cu_bridge_id >> 48);
	bpdu.cbu_bridgeaddr[0] = cu->cu_bridge_id >> 40;
	bpdu.cbu_bridgeaddr[1] = cu->cu_bridge_id >> 32;
	bpdu.cbu_bridgeaddr[2] = cu->cu_bridge_id >> 24;
	bpdu.cbu_bridgeaddr[3] = cu->cu_bridge_id >> 16;
	bpdu.cbu_bridgeaddr[4] = cu->cu_bridge_id >> 8;
	bpdu.cbu_bridgeaddr[5] = cu->cu_bridge_id >> 0;

	bpdu.cbu_portid = htons(cu->cu_port_id);
	bpdu.cbu_messageage = htons(cu->cu_message_age);
	bpdu.cbu_maxage = htons(cu->cu_max_age);
	bpdu.cbu_hellotime = htons(cu->cu_hello_time);
	bpdu.cbu_forwarddelay = htons(cu->cu_forward_delay);

	memcpy(eh->ether_shost, IF_LLADDR(ifp), ETHER_ADDR_LEN);
	memcpy(eh->ether_dhost, bstp_etheraddr, ETHER_ADDR_LEN);
	eh->ether_type = htons(sizeof(bpdu));

	memcpy(mtod(m, caddr_t) + sizeof(*eh), &bpdu, sizeof(bpdu));

	bstp_enqueue(ifp, m);
}

static int
bstp_root_bridge(struct bstp_state *bs)
{
	return (bs->bs_designated_root == bs->bs_bridge_id);
}

static int
bstp_supersedes_port_info(struct bstp_state *bs, struct bstp_port *bp,
    struct bstp_config_unit *cu)
{
	if (cu->cu_rootid < bp->bp_designated_root)
		return (1);
	if (cu->cu_rootid > bp->bp_designated_root)
		return (0);

	if (cu->cu_root_path_cost < bp->bp_designated_cost)
		return (1);
	if (cu->cu_root_path_cost > bp->bp_designated_cost)
		return (0);

	if (cu->cu_bridge_id < bp->bp_designated_bridge)
		return (1);
	if (cu->cu_bridge_id > bp->bp_designated_bridge)
		return (0);

	if (bs->bs_bridge_id != cu->cu_bridge_id)
		return (1);
	if (cu->cu_port_id <= bp->bp_designated_port)
		return (1);
	return (0);
}

static void
bstp_record_config_information(struct bstp_state *bs,
    struct bstp_port *bp, struct bstp_config_unit *cu)
{
	BSTP_LOCK_ASSERT(bs);

	bp->bp_designated_root = cu->cu_rootid;
	bp->bp_designated_cost = cu->cu_root_path_cost;
	bp->bp_designated_bridge = cu->cu_bridge_id;
	bp->bp_designated_port = cu->cu_port_id;
	bstp_timer_start(&bp->bp_message_age_timer, cu->cu_message_age);
}

static void
bstp_record_config_timeout_values(struct bstp_state *bs,
    struct bstp_config_unit *config)
{
	BSTP_LOCK_ASSERT(bs);

	bs->bs_max_age = config->cu_max_age;
	bs->bs_hello_time = config->cu_hello_time;
	bs->bs_forward_delay = config->cu_forward_delay;
	bs->bs_topology_change = config->cu_topology_change;
}

static void
bstp_config_bpdu_generation(struct bstp_state *bs)
{
	struct bstp_port *bp;

	BSTP_LOCK_ASSERT(bs);

	LIST_FOREACH(bp, &bs->bs_bplist, bp_next) {
		if (bstp_designated_port(bs, bp) &&
		    (bp->bp_state != BSTP_IFSTATE_DISABLED))
			bstp_transmit_config(bs, bp);
	}
}

static int
bstp_designated_port(struct bstp_state *bs, struct bstp_port *bp)
{
	return ((bp->bp_designated_bridge == bs->bs_bridge_id)
	    && (bp->bp_designated_port == bp->bp_port_id));
}

static void
bstp_transmit_tcn(struct bstp_state *bs)
{
	struct bstp_tbpdu bpdu;
	struct bstp_port *bp = bs->bs_root_port;
	struct ifnet *ifp = bp->bp_ifp;
	struct ether_header *eh;
	struct mbuf *m;

	BSTP_LOCK_ASSERT(bs);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;

	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = sizeof(*eh) + sizeof(bpdu);
	m->m_len = m->m_pkthdr.len;

	eh = mtod(m, struct ether_header *);

	memcpy(eh->ether_shost, IF_LLADDR(ifp), ETHER_ADDR_LEN);
	memcpy(eh->ether_dhost, bstp_etheraddr, ETHER_ADDR_LEN);
	eh->ether_type = htons(sizeof(bpdu));

	bpdu.tbu_ssap = bpdu.tbu_dsap = LLC_8021D_LSAP;
	bpdu.tbu_ctl = LLC_UI;
	bpdu.tbu_protoid = 0;
	bpdu.tbu_protover = 0;
	bpdu.tbu_bpdutype = BSTP_MSGTYPE_TCN;

	memcpy(mtod(m, caddr_t) + sizeof(*eh), &bpdu, sizeof(bpdu));

	bstp_enqueue(ifp, m);
}

static void
bstp_configuration_update(struct bstp_state *bs)
{
	BSTP_LOCK_ASSERT(bs);

	bstp_root_selection(bs);
	bstp_designated_port_selection(bs);
}

static void
bstp_root_selection(struct bstp_state *bs)
{
	struct bstp_port *root_port = NULL, *bp;

	BSTP_LOCK_ASSERT(bs);

	LIST_FOREACH(bp, &bs->bs_bplist, bp_next) {
		if (bstp_designated_port(bs, bp))
			continue;
		if (bp->bp_state == BSTP_IFSTATE_DISABLED)
			continue;
		if (bp->bp_designated_root >= bs->bs_bridge_id)
			continue;
		if (root_port == NULL)
			goto set_port;

		if (bp->bp_designated_root < root_port->bp_designated_root)
			goto set_port;
		if (bp->bp_designated_root > root_port->bp_designated_root)
			continue;

		if ((bp->bp_designated_cost + bp->bp_path_cost) <
		    (root_port->bp_designated_cost + root_port->bp_path_cost))
			goto set_port;
		if ((bp->bp_designated_cost + bp->bp_path_cost) >
		    (root_port->bp_designated_cost + root_port->bp_path_cost))
			continue;

		if (bp->bp_designated_bridge <
		    root_port->bp_designated_bridge)
			goto set_port;
		if (bp->bp_designated_bridge >
		    root_port->bp_designated_bridge)
			continue;

		if (bp->bp_designated_port < root_port->bp_designated_port)
			goto set_port;
		if (bp->bp_designated_port > root_port->bp_designated_port)
			continue;

		if (bp->bp_port_id >= root_port->bp_port_id)
			continue;
set_port:
		root_port = bp;
	}

	bs->bs_root_port = root_port;
	if (root_port == NULL) {
		bs->bs_designated_root = bs->bs_bridge_id;
		bs->bs_root_path_cost = 0;
	} else {
		bs->bs_designated_root = root_port->bp_designated_root;
		bs->bs_root_path_cost = root_port->bp_designated_cost +
		    root_port->bp_path_cost;
	}
}

static void
bstp_designated_port_selection(struct bstp_state *bs)
{
	struct bstp_port *bp;

	BSTP_LOCK_ASSERT(bs);

	LIST_FOREACH(bp, &bs->bs_bplist, bp_next) {
		if (bstp_designated_port(bs, bp))
			goto designated;
		if (bp->bp_designated_root != bs->bs_designated_root)
			goto designated;

		if (bs->bs_root_path_cost < bp->bp_designated_cost)
			goto designated;
		if (bs->bs_root_path_cost > bp->bp_designated_cost)
			continue;

		if (bs->bs_bridge_id < bp->bp_designated_bridge)
			goto designated;
		if (bs->bs_bridge_id > bp->bp_designated_bridge)
			continue;

		if (bp->bp_port_id > bp->bp_designated_port)
			continue;
designated:
		bstp_become_designated_port(bs, bp);
	}
}

static void
bstp_become_designated_port(struct bstp_state *bs, struct bstp_port *bp)
{
	BSTP_LOCK_ASSERT(bs);

	bp->bp_designated_root = bs->bs_designated_root;
	bp->bp_designated_cost = bs->bs_root_path_cost;
	bp->bp_designated_bridge = bs->bs_bridge_id;
	bp->bp_designated_port = bp->bp_port_id;
}

static void
bstp_port_state_selection(struct bstp_state *bs)
{
	struct bstp_port *bp;

	BSTP_LOCK_ASSERT(bs);

	LIST_FOREACH(bp, &bs->bs_bplist, bp_next) {
		if (bp == bs->bs_root_port) {
			bp->bp_config_pending = 0;
			bp->bp_topology_change_acknowledge = 0;
			bstp_make_forwarding(bs, bp);
		} else if (bstp_designated_port(bs, bp)) {
			bstp_timer_stop(&bp->bp_message_age_timer);
			bstp_make_forwarding(bs, bp);
		} else {
			bp->bp_config_pending = 0;
			bp->bp_topology_change_acknowledge = 0;
			bstp_make_blocking(bs, bp);
		}
	}
}

static void
bstp_make_forwarding(struct bstp_state *bs, struct bstp_port *bp)
{
	BSTP_LOCK_ASSERT(bs);

	if (bp->bp_state == BSTP_IFSTATE_BLOCKING) {
		bstp_set_port_state(bp, BSTP_IFSTATE_LISTENING);
		bstp_timer_start(&bp->bp_forward_delay_timer, 0);
	}
}

static void
bstp_make_blocking(struct bstp_state *bs, struct bstp_port *bp)
{
	BSTP_LOCK_ASSERT(bs);

	if ((bp->bp_state != BSTP_IFSTATE_DISABLED) &&
	    (bp->bp_state != BSTP_IFSTATE_BLOCKING)) {
		if ((bp->bp_state == BSTP_IFSTATE_FORWARDING) ||
		    (bp->bp_state == BSTP_IFSTATE_LEARNING)) {
			if (bp->bp_change_detection_enabled) {
				bstp_topology_change_detection(bs);
			}
		}
		bstp_set_port_state(bp, BSTP_IFSTATE_BLOCKING);
		/* XXX bridge_rtdelete(bs, bp->bp_ifp, IFBF_FLUSHDYN); */
		bstp_timer_stop(&bp->bp_forward_delay_timer);
	}
}

static void
bstp_set_port_state(struct bstp_port *bp, uint8_t state)
{
	bp->bp_state = state;
}

static void
bstp_update_forward_transitions(struct bstp_port *bp)
{
	bp->bp_forward_transitions++;
}

static void
bstp_topology_change_detection(struct bstp_state *bs)
{
	BSTP_LOCK_ASSERT(bs);

	if (bstp_root_bridge(bs)) {
		bs->bs_topology_change = 1;
		bstp_timer_start(&bs->bs_topology_change_timer, 0);
	} else if (!bs->bs_topology_change_detected) {
		bstp_transmit_tcn(bs);
		bstp_timer_start(&bs->bs_tcn_timer, 0);
	}
	bs->bs_topology_change_detected = 1;
	getmicrotime(&bs->bs_last_tc_time);
}

static void
bstp_topology_change_acknowledged(struct bstp_state *bs)
{
	BSTP_LOCK_ASSERT(bs);

	bs->bs_topology_change_detected = 0;
	bstp_timer_stop(&bs->bs_tcn_timer);
}

static void
bstp_acknowledge_topology_change(struct bstp_state *bs,
    struct bstp_port *bp)
{
	BSTP_LOCK_ASSERT(bs);

	bp->bp_topology_change_acknowledge = 1;
	bstp_transmit_config(bs, bp);
}

struct mbuf *
bstp_input(struct bstp_port *bp, struct ifnet *ifp, struct mbuf *m)
{
	struct bstp_state *bs = bp->bp_bs;
	struct ether_header *eh;
	struct bstp_tbpdu tpdu;
	struct bstp_cbpdu cpdu;
	struct bstp_config_unit cu;
	struct bstp_tcn_unit tu;
	uint16_t len;

	if (bp->bp_active == 0) {
		m_freem(m);
		return (NULL);
	}

	BSTP_LOCK(bs);

	eh = mtod(m, struct ether_header *);

	len = ntohs(eh->ether_type);
	if (len < sizeof(tpdu))
		goto out;

	m_adj(m, ETHER_HDR_LEN);

	if (m->m_pkthdr.len > len)
		m_adj(m, len - m->m_pkthdr.len);
	if (m->m_len < sizeof(tpdu) &&
	    (m = m_pullup(m, sizeof(tpdu))) == NULL)
		goto out;

	memcpy(&tpdu, mtod(m, caddr_t), sizeof(tpdu));

	if (tpdu.tbu_dsap != LLC_8021D_LSAP ||
	    tpdu.tbu_ssap != LLC_8021D_LSAP ||
	    tpdu.tbu_ctl != LLC_UI)
		goto out;
	if (tpdu.tbu_protoid != 0 || tpdu.tbu_protover != 0)
		goto out;

	switch (tpdu.tbu_bpdutype) {
	case BSTP_MSGTYPE_TCN:
		tu.tu_message_type = tpdu.tbu_bpdutype;
		bstp_received_tcn_bpdu(bs, bp, &tu);
		break;
	case BSTP_MSGTYPE_CFG:
		if (m->m_len < sizeof(cpdu) &&
		    (m = m_pullup(m, sizeof(cpdu))) == NULL)
			goto out;
		memcpy(&cpdu, mtod(m, caddr_t), sizeof(cpdu));

		cu.cu_rootid =
		    (((uint64_t)ntohs(cpdu.cbu_rootpri)) << 48) |
		    (((uint64_t)cpdu.cbu_rootaddr[0]) << 40) |
		    (((uint64_t)cpdu.cbu_rootaddr[1]) << 32) |
		    (((uint64_t)cpdu.cbu_rootaddr[2]) << 24) |
		    (((uint64_t)cpdu.cbu_rootaddr[3]) << 16) |
		    (((uint64_t)cpdu.cbu_rootaddr[4]) << 8) |
		    (((uint64_t)cpdu.cbu_rootaddr[5]) << 0);

		cu.cu_bridge_id =
		    (((uint64_t)ntohs(cpdu.cbu_bridgepri)) << 48) |
		    (((uint64_t)cpdu.cbu_bridgeaddr[0]) << 40) |
		    (((uint64_t)cpdu.cbu_bridgeaddr[1]) << 32) |
		    (((uint64_t)cpdu.cbu_bridgeaddr[2]) << 24) |
		    (((uint64_t)cpdu.cbu_bridgeaddr[3]) << 16) |
		    (((uint64_t)cpdu.cbu_bridgeaddr[4]) << 8) |
		    (((uint64_t)cpdu.cbu_bridgeaddr[5]) << 0);

		cu.cu_root_path_cost = ntohl(cpdu.cbu_rootpathcost);
		cu.cu_message_age = ntohs(cpdu.cbu_messageage);
		cu.cu_max_age = ntohs(cpdu.cbu_maxage);
		cu.cu_hello_time = ntohs(cpdu.cbu_hellotime);
		cu.cu_forward_delay = ntohs(cpdu.cbu_forwarddelay);
		cu.cu_port_id = ntohs(cpdu.cbu_portid);
		cu.cu_message_type = cpdu.cbu_bpdutype;
		cu.cu_topology_change_acknowledgment =
		    (cpdu.cbu_flags & BSTP_FLAG_TCA) ? 1 : 0;
		cu.cu_topology_change =
		    (cpdu.cbu_flags & BSTP_FLAG_TC) ? 1 : 0;
		bstp_received_config_bpdu(bs, bp, &cu);
		break;
	default:
		goto out;
	}

out:
	BSTP_UNLOCK(bs);
	if (m)
		m_freem(m);
	return (NULL);
}

static void
bstp_received_config_bpdu(struct bstp_state *bs, struct bstp_port *bp,
    struct bstp_config_unit *cu)
{
	int root;

	BSTP_LOCK_ASSERT(bs);

	root = bstp_root_bridge(bs);

	if (bp->bp_state != BSTP_IFSTATE_DISABLED) {
		if (bstp_supersedes_port_info(bs, bp, cu)) {
			bstp_record_config_information(bs, bp, cu);
			bstp_configuration_update(bs);
			bstp_port_state_selection(bs);

			if ((bstp_root_bridge(bs) == 0) && root) {
				bstp_timer_stop(&bs->bs_hello_timer);

				if (bs->bs_topology_change_detected) {
					bstp_timer_stop(
					    &bs->bs_topology_change_timer);
					bstp_transmit_tcn(bs);
					bstp_timer_start(&bs->bs_tcn_timer, 0);
				}
			}

			if (bp == bs->bs_root_port) {
				bstp_record_config_timeout_values(bs, cu);
				bstp_config_bpdu_generation(bs);

				if (cu->cu_topology_change_acknowledgment)
					bstp_topology_change_acknowledged(bs);
			}
		} else if (bstp_designated_port(bs, bp))
			bstp_transmit_config(bs, bp);
	}
}

static void
bstp_received_tcn_bpdu(struct bstp_state *bs, struct bstp_port *bp,
    struct bstp_tcn_unit *tcn)
{
	if (bp->bp_state != BSTP_IFSTATE_DISABLED &&
	    bstp_designated_port(bs, bp)) {
		bstp_topology_change_detection(bs);
		bstp_acknowledge_topology_change(bs, bp);
	}
}

static void
bstp_hello_timer_expiry(struct bstp_state *bs)
{
	bstp_config_bpdu_generation(bs);
	bstp_timer_start(&bs->bs_hello_timer, 0);
}

static void
bstp_message_age_timer_expiry(struct bstp_state *bs,
    struct bstp_port *bp)
{
	int root;

	BSTP_LOCK_ASSERT(bs);

	root = bstp_root_bridge(bs);
	bstp_become_designated_port(bs, bp);
	bstp_configuration_update(bs);
	bstp_port_state_selection(bs);

	if ((bstp_root_bridge(bs)) && (root == 0)) {
		bs->bs_max_age = bs->bs_bridge_max_age;
		bs->bs_hello_time = bs->bs_bridge_hello_time;
		bs->bs_forward_delay = bs->bs_bridge_forward_delay;

		bstp_topology_change_detection(bs);
		bstp_timer_stop(&bs->bs_tcn_timer);
		bstp_config_bpdu_generation(bs);
		bstp_timer_start(&bs->bs_hello_timer, 0);
	}
}

static void
bstp_forward_delay_timer_expiry(struct bstp_state *bs,
    struct bstp_port *bp)
{
	if (bp->bp_state == BSTP_IFSTATE_LISTENING) {
		bstp_set_port_state(bp, BSTP_IFSTATE_LEARNING);
		bstp_timer_start(&bp->bp_forward_delay_timer, 0);
	} else if (bp->bp_state == BSTP_IFSTATE_LEARNING) {
		bstp_set_port_state(bp, BSTP_IFSTATE_FORWARDING);
		bstp_update_forward_transitions(bp);
		if (bstp_designated_for_some_port(bs) &&
		    bp->bp_change_detection_enabled)
			bstp_topology_change_detection(bs);
	}
}

static int
bstp_designated_for_some_port(struct bstp_state *bs)
{

	struct bstp_port *bp;

	BSTP_LOCK_ASSERT(bs);

	LIST_FOREACH(bp, &bs->bs_bplist, bp_next) {
		if (bp->bp_designated_bridge == bs->bs_bridge_id)
			return (1);
	}
	return (0);
}

static void
bstp_tcn_timer_expiry(struct bstp_state *bs)
{
	BSTP_LOCK_ASSERT(bs);

	bstp_transmit_tcn(bs);
	bstp_timer_start(&bs->bs_tcn_timer, 0);
}

static void
bstp_topology_change_timer_expiry(struct bstp_state *bs)
{
	BSTP_LOCK_ASSERT(bs);

	bs->bs_topology_change_detected = 0;
	bs->bs_topology_change = 0;
}

static void
bstp_hold_timer_expiry(struct bstp_state *bs, struct bstp_port *bp)
{
	if (bp->bp_config_pending)
		bstp_transmit_config(bs, bp);
}

static int
bstp_addr_cmp(const uint8_t *a, const uint8_t *b)
{
	int i, d;

	for (i = 0, d = 0; i < ETHER_ADDR_LEN && d == 0; i++) {
		d = ((int)a[i]) - ((int)b[i]);
	}

	return (d);
}

void
bstp_reinit(struct bstp_state *bs)
{
	struct bstp_port *bp, *mbp;
	u_char *e_addr;

	BSTP_LOCK(bs);

	mbp = NULL;
	LIST_FOREACH(bp, &bs->bs_bplist, bp_next) {
		bp->bp_port_id = (bp->bp_priority << 8) |
		    (bp->bp_ifp->if_index & 0xff);

		if (mbp == NULL) {
			mbp = bp;
			continue;
		}
		if (bstp_addr_cmp(IF_LLADDR(bp->bp_ifp),
		    IF_LLADDR(mbp->bp_ifp)) < 0) {
			mbp = bp;
			continue;
		}
	}
	if (mbp == NULL) {
		BSTP_UNLOCK(bs);
		bstp_stop(bs);
		return;
	}

	e_addr = IF_LLADDR(mbp->bp_ifp);
	bs->bs_bridge_id =
	    (((uint64_t)bs->bs_bridge_priority) << 48) |
	    (((uint64_t)e_addr[0]) << 40) |
	    (((uint64_t)e_addr[1]) << 32) |
	    (((uint64_t)e_addr[2]) << 24) |
	    (((uint64_t)e_addr[3]) << 16) |
	    (((uint64_t)e_addr[4]) << 8) |
	    (((uint64_t)e_addr[5]));

	bs->bs_designated_root = bs->bs_bridge_id;
	bs->bs_root_path_cost = 0;
	bs->bs_root_port = NULL;

	bs->bs_max_age = bs->bs_bridge_max_age;
	bs->bs_hello_time = bs->bs_bridge_hello_time;
	bs->bs_forward_delay = bs->bs_bridge_forward_delay;
	bs->bs_topology_change_detected = 0;
	bs->bs_topology_change = 0;
	bstp_timer_stop(&bs->bs_tcn_timer);
	bstp_timer_stop(&bs->bs_topology_change_timer);

	if (callout_pending(&bs->bs_bstpcallout) == 0)
		callout_reset(&bs->bs_bstpcallout, hz,
		    bstp_tick, bs);

	LIST_FOREACH(bp, &bs->bs_bplist, bp_next)
		bstp_ifupdstatus(bs, bp);

	getmicrotime(&bs->bs_last_tc_time);
	bstp_port_state_selection(bs);
	bstp_config_bpdu_generation(bs);
	bstp_timer_start(&bs->bs_hello_timer, 0);
	bstp_timer_start(&bs->bs_link_timer, 0);
	BSTP_UNLOCK(bs);
}

static int
bstp_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		mtx_init(&bstp_list_mtx, "bridgestp list", NULL, MTX_DEF);
		LIST_INIT(&bstp_list);
		bstp_linkstate_p = bstp_linkstate;
		break;
	case MOD_UNLOAD:
		mtx_destroy(&bstp_list_mtx);
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t bstp_mod = {
	"bridgestp",
	bstp_modevent,
	0
};

DECLARE_MODULE(bridgestp, bstp_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

void
bstp_attach(struct bstp_state *bs)
{
	BSTP_LOCK_INIT(bs);
	callout_init_mtx(&bs->bs_bstpcallout, &bs->bs_mtx, 0);
	LIST_INIT(&bs->bs_bplist);

	bs->bs_bridge_max_age = BSTP_DEFAULT_MAX_AGE;
	bs->bs_bridge_hello_time = BSTP_DEFAULT_HELLO_TIME;
	bs->bs_bridge_forward_delay = BSTP_DEFAULT_FORWARD_DELAY;
	bs->bs_bridge_priority = BSTP_DEFAULT_BRIDGE_PRIORITY;
	bs->bs_hold_time = BSTP_DEFAULT_HOLD_TIME;

	mtx_lock(&bstp_list_mtx);
	LIST_INSERT_HEAD(&bstp_list, bs, bs_list);
	mtx_unlock(&bstp_list_mtx);
}

void
bstp_detach(struct bstp_state *bs)
{
	KASSERT(LIST_EMPTY(&bs->bs_bplist), ("bstp still active"));

	mtx_lock(&bstp_list_mtx);
	LIST_REMOVE(bs, bs_list);
	mtx_unlock(&bstp_list_mtx);
	BSTP_LOCK_DESTROY(bs);
}

void
bstp_init(struct bstp_state *bs)
{
	callout_reset(&bs->bs_bstpcallout, hz, bstp_tick, bs);
	bstp_reinit(bs);
}

void
bstp_stop(struct bstp_state *bs)
{
	struct bstp_port *bp;

	BSTP_LOCK(bs);

	LIST_FOREACH(bp, &bs->bs_bplist, bp_next) {
		bstp_set_port_state(bp, BSTP_IFSTATE_DISABLED);
		bstp_timer_stop(&bp->bp_hold_timer);
		bstp_timer_stop(&bp->bp_message_age_timer);
		bstp_timer_stop(&bp->bp_forward_delay_timer);
	}

	callout_drain(&bs->bs_bstpcallout);
	callout_stop(&bs->bs_bstpcallout);

	bstp_timer_stop(&bs->bs_topology_change_timer);
	bstp_timer_stop(&bs->bs_tcn_timer);
	bstp_timer_stop(&bs->bs_hello_timer);

	BSTP_UNLOCK(bs);
}

static void
bstp_initialize_port(struct bstp_state *bs, struct bstp_port *bp)
{
	BSTP_LOCK_ASSERT(bs);

	bstp_become_designated_port(bs, bp);
	bstp_set_port_state(bp, BSTP_IFSTATE_BLOCKING);
	bp->bp_topology_change_acknowledge = 0;
	bp->bp_config_pending = 0;
	bp->bp_change_detection_enabled = 1;
	bstp_timer_stop(&bp->bp_message_age_timer);
	bstp_timer_stop(&bp->bp_forward_delay_timer);
	bstp_timer_stop(&bp->bp_hold_timer);
}

static void
bstp_enable_port(struct bstp_state *bs, struct bstp_port *bp)
{
	bstp_initialize_port(bs, bp);
	bstp_port_state_selection(bs);
}

static void
bstp_disable_port(struct bstp_state *bs, struct bstp_port *bp)
{
	int root;

	BSTP_LOCK_ASSERT(bs);

	root = bstp_root_bridge(bs);
	bstp_become_designated_port(bs, bp);
	bstp_set_port_state(bp, BSTP_IFSTATE_DISABLED);
	bp->bp_topology_change_acknowledge = 0;
	bp->bp_config_pending = 0;
	bstp_timer_stop(&bp->bp_message_age_timer);
	bstp_timer_stop(&bp->bp_forward_delay_timer);
	bstp_configuration_update(bs);
	bstp_port_state_selection(bs);
	/* XXX bridge_rtdelete(bs, bp->bp_ifp, IFBF_FLUSHDYN); */

	if (bstp_root_bridge(bs) && (root == 0)) {
		bs->bs_max_age = bs->bs_bridge_max_age;
		bs->bs_hello_time = bs->bs_bridge_hello_time;
		bs->bs_forward_delay = bs->bs_bridge_forward_delay;

		bstp_topology_change_detection(bs);
		bstp_timer_stop(&bs->bs_tcn_timer);
		bstp_config_bpdu_generation(bs);
		bstp_timer_start(&bs->bs_hello_timer, 0);
	}
}

#ifdef notused
static void
bstp_set_bridge_priority(struct bstp_state *bs, uint64_t new_bridge_id)
{
	struct bstp_port *bp;
	int root;

	BSTP_LOCK_ASSERT(bs);

	root = bstp_root_bridge(bs);

	LIST_FOREACH(bp, &bs->bs_bplist, bp_next) {
		if (bstp_designated_port(bs, bp))
			bp->bp_designated_bridge = new_bridge_id;
	}

	bs->bs_bridge_id = new_bridge_id;

	bstp_configuration_update(bs);
	bstp_port_state_selection(bs);

	if (bstp_root_bridge(bs) && (root == 0)) {
		bs->bs_max_age = bs->bs_bridge_max_age;
		bs->bs_hello_time = bs->bs_bridge_hello_time;
		bs->bs_forward_delay = bs->bs_bridge_forward_delay;

		bstp_topology_change_detection(bs);
		bstp_timer_stop(&bs->bs_tcn_timer);
		bstp_config_bpdu_generation(bs);
		bstp_timer_start(&bs->bs_hello_timer, 0);
	}
}

static void
bstp_set_port_priority(struct bstp_state *bs, struct bstp_port *bp,
    uint16_t new_port_id)
{
	if (bstp_designated_port(bs, bp))
		bp->bp_designated_port = new_port_id;

	bp->bp_port_id = new_port_id;

	if ((bs->bs_bridge_id == bp->bp_designated_bridge) &&
	    (bp->bp_port_id < bp->bp_designated_port)) {
		bstp_become_designated_port(bs, bp);
		bstp_port_state_selection(bs);
	}
}

static void
bstp_set_path_cost(struct bstp_state *bs, struct bstp_port *bp,
    uint32_t path_cost)
{
	bp->bp_path_cost = path_cost;
	bstp_configuration_update(bs);
	bstp_port_state_selection(bs);
}

static void
bstp_enable_change_detection(struct bstp_port *bp)
{
	bp->bp_change_detection_enabled = 1;
}

static void
bstp_disable_change_detection(struct bstp_port *bp)
{
	bp->bp_change_detection_enabled = 0;
}
#endif /* notused */

static void
bstp_enqueue(struct ifnet *dst_ifp, struct mbuf *m)
{
	int err = 0;

	IFQ_ENQUEUE(&dst_ifp->if_snd, m, err);

	if ((dst_ifp->if_drv_flags & IFF_DRV_OACTIVE) == 0)
		(*dst_ifp->if_start)(dst_ifp);
}

void
bstp_linkstate(struct ifnet *ifp, int state)
{
	struct bstp_state *bs;
	struct bstp_port *bp;

	/*
	 * It would be nice if the ifnet had a pointer to the bstp_port so we
	 * didnt need to search for it, but that may be an overkill. In reality
	 * this is fast and doesnt get called often.
	 */
	mtx_lock(&bstp_list_mtx);
	LIST_FOREACH(bs, &bstp_list, bs_list) {
		BSTP_LOCK(bs);
		LIST_FOREACH(bp, &bs->bs_bplist, bp_next) {
			if (bp->bp_ifp == ifp) {
				bstp_ifupdstatus(bs, bp);
				/* it only exists once so return */
				BSTP_UNLOCK(bs);
				mtx_unlock(&bstp_list_mtx);
				return;
			}
		}
		BSTP_UNLOCK(bs);
	}
	mtx_unlock(&bstp_list_mtx);
}

static void
bstp_ifupdstatus(struct bstp_state *bs, struct bstp_port *bp)
{
	struct ifnet *ifp = bp->bp_ifp;
	struct ifmediareq ifmr;
	int error = 0;

	BSTP_LOCK_ASSERT(bs);

	bzero((char *)&ifmr, sizeof(ifmr));
	error = (*ifp->if_ioctl)(ifp, SIOCGIFMEDIA, (caddr_t)&ifmr);

	if ((error == 0) && (ifp->if_flags & IFF_UP)) {
		if (ifmr.ifm_status & IFM_ACTIVE) {
			if (bp->bp_state == BSTP_IFSTATE_DISABLED)
				bstp_enable_port(bs, bp);

		} else {
			if (bp->bp_state != BSTP_IFSTATE_DISABLED)
				bstp_disable_port(bs, bp);
		}
		return;
	}

	if (bp->bp_state != BSTP_IFSTATE_DISABLED)
		bstp_disable_port(bs, bp);
}

static void
bstp_tick(void *arg)
{
	struct bstp_state *bs = arg;
	struct bstp_port *bp;

	BSTP_LOCK_ASSERT(bs);

	/* slow timer to catch missed link events */
	if (bstp_timer_expired(&bs->bs_link_timer, BSTP_LINK_TIMER)) {
		LIST_FOREACH(bp, &bs->bs_bplist, bp_next) {
			bstp_ifupdstatus(bs, bp);
		}
		bstp_timer_start(&bs->bs_link_timer, 0);
	}

	if (bstp_timer_expired(&bs->bs_hello_timer, bs->bs_hello_time))
		bstp_hello_timer_expiry(bs);

	if (bstp_timer_expired(&bs->bs_tcn_timer, bs->bs_bridge_hello_time))
		bstp_tcn_timer_expiry(bs);

	if (bstp_timer_expired(&bs->bs_topology_change_timer,
	    bs->bs_topology_change_time))
		bstp_topology_change_timer_expiry(bs);

	LIST_FOREACH(bp, &bs->bs_bplist, bp_next) {
		if (bstp_timer_expired(&bp->bp_message_age_timer,
		    bs->bs_max_age))
			bstp_message_age_timer_expiry(bs, bp);
	}

	LIST_FOREACH(bp, &bs->bs_bplist, bp_next) {
		if (bstp_timer_expired(&bp->bp_forward_delay_timer,
		    bs->bs_forward_delay))
			bstp_forward_delay_timer_expiry(bs, bp);

		if (bstp_timer_expired(&bp->bp_hold_timer,
		    bs->bs_hold_time))
			bstp_hold_timer_expiry(bs, bp);
	}

	callout_reset(&bs->bs_bstpcallout, hz, bstp_tick, bs);
}

static void
bstp_timer_start(struct bstp_timer *t, uint16_t v)
{
	t->value = v;
	t->active = 1;
}

static void
bstp_timer_stop(struct bstp_timer *t)
{
	t->value = 0;
	t->active = 0;
}

static int
bstp_timer_expired(struct bstp_timer *t, uint16_t v)
{
	if (t->active == 0)
		return (0);
	t->value += BSTP_TICK_VAL;
	if (t->value >= v) {
		bstp_timer_stop(t);
		return (1);
	}
	return (0);

}

int
bstp_add(struct bstp_state *bs, struct bstp_port *bp, struct ifnet *ifp)
{
	KASSERT(bp->bp_active == 0, ("already a bstp member"));

	switch (ifp->if_type) {
		case IFT_ETHER:	/* These can do spanning tree. */
			break;
		default:
			/* Nothing else can. */
			return (EINVAL);
	}

	BSTP_LOCK(bs);
	bp->bp_ifp = ifp;
	bp->bp_bs = bs;
	bp->bp_active = 1;
	bp->bp_priority = BSTP_DEFAULT_PORT_PRIORITY;
	bp->bp_path_cost = BSTP_DEFAULT_PATH_COST;

	LIST_INSERT_HEAD(&bs->bs_bplist, bp, bp_next);
	BSTP_UNLOCK(bs);
	bstp_reinit(bs);

	return (0);
}

void
bstp_delete(struct bstp_port *bp)
{
	struct bstp_state *bs = bp->bp_bs;

	KASSERT(bp->bp_active == 1, ("not a bstp member"));

	BSTP_LOCK(bs);
	if (bp->bp_state != BSTP_IFSTATE_DISABLED)
		bstp_disable_port(bs, bp);
	LIST_REMOVE(bp, bp_next);
	BSTP_UNLOCK(bs);
	bp->bp_bs = NULL;
	bp->bp_active = 0;

	bstp_reinit(bs);
}
