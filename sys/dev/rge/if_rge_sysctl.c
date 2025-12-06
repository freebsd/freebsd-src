/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Adrian Chadd <adrian@FreeBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/endian.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_media.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/mii/mii.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "if_rge_vendor.h"
#include "if_rgereg.h"
#include "if_rgevar.h"
#include "if_rge_debug.h"
#include "if_rge_sysctl.h"

static void
rge_sysctl_drv_stats_attach(struct rge_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);

	/* Create stats node */
	tree = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "drv_stats",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "driver statistics");
	child = SYSCTL_CHILDREN(tree);

	/* Driver stats */
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "transmit_call_cnt", CTLFLAG_RD,
	    &sc->sc_drv_stats.transmit_call_cnt, "Calls to rge_transmit");
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "transmit_stopped_cnt",
	    CTLFLAG_RD, &sc->sc_drv_stats.transmit_stopped_cnt,
	        "rge_transmit calls to a stopped interface");
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "transmit_full_cnt",
	    CTLFLAG_RD, &sc->sc_drv_stats.transmit_full_cnt,
	        "rge_transmit calls to a full tx queue");
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "transmit_queued_cnt",
	    CTLFLAG_RD, &sc->sc_drv_stats.transmit_queued_cnt,
	        "rge_transmit calls which queued a frame");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "intr_cnt",
	    CTLFLAG_RD, &sc->sc_drv_stats.intr_cnt,
	        "incoming interrupts");
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "intr_system_errcnt",
	    CTLFLAG_RD, &sc->sc_drv_stats.intr_system_err_cnt,
	        "INTR_SYSTEM_ERR interrupt leading to a hardware reset");
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rxeof_cnt",
	    CTLFLAG_RD, &sc->sc_drv_stats.rxeof_cnt,
	        "calls to rxeof() to process RX frames");
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "txeof_cnt",
	    CTLFLAG_RD, &sc->sc_drv_stats.txeof_cnt,
	        "calls to rxeof() to process TX frame completions");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "link_state_change_cnt",
	    CTLFLAG_RD, &sc->sc_drv_stats.link_state_change_cnt,
	        "link state changes");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "tx_task_cnt",
	    CTLFLAG_RD, &sc->sc_drv_stats.tx_task_cnt,
	        "calls to tx_task task to send queued frames");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "recv_input_cnt",
	    CTLFLAG_RD, &sc->sc_drv_stats.recv_input_cnt,
	        "calls to if_input to process frames");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rx_desc_err_multidesc",
	    CTLFLAG_RD, &sc->sc_drv_stats.rx_desc_err_multidesc,
	        "multi-descriptor RX frames (unsupported, so dropped)");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "tx_watchdog_timeout_cnt",
	    CTLFLAG_RD, &sc->sc_drv_stats.tx_watchdog_timeout_cnt,
	        "TX watchdog timeouts");

	/* TX encap counters */

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "tx_encap_cnt",
	    CTLFLAG_RD, &sc->sc_drv_stats.tx_encap_cnt, "calls to rge_encap()");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "tx_encap_refrag_cnt",
	    CTLFLAG_RD, &sc->sc_drv_stats.tx_encap_refrag_cnt,
	    "How often rge_encap() has re-linearised TX mbufs");

	/* TX checksum counters */

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "tx_encap_err_toofrag",
	    CTLFLAG_RD, &sc->sc_drv_stats.tx_encap_err_toofrag,
	    "How often rge_encap() failed to defrag a TX mbuf");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "tx_offload_ip_csum_set",
	    CTLFLAG_RD, &sc->sc_drv_stats.tx_offload_ip_csum_set,
	    "Number of frames with TX'ed with IPv4 checksum offload set");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "tx_offload_tcp_csum_set",
	    CTLFLAG_RD, &sc->sc_drv_stats.tx_offload_tcp_csum_set,
	    "Number of frames TX'ed with TCP checksum offload set");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "tx_offload_udp_csum_set",
	    CTLFLAG_RD, &sc->sc_drv_stats.tx_offload_udp_csum_set,
	    "Number of frames TX'ed with UDP checksum offload set");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "tx_offload_vlan_tag_set",
	    CTLFLAG_RD, &sc->sc_drv_stats.tx_offload_vlan_tag_set,
	    "Number of frames TX'ed with VLAN offload tag set");

	/* RX counters */
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rx_ether_csum_err",
	    CTLFLAG_RD, &sc->sc_drv_stats.rx_ether_csum_err,
	    "Number of frames RX'ed with invalid ethernet CRC");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rx_offload_vlan_tag",
	    CTLFLAG_RD, &sc->sc_drv_stats.rx_offload_vlan_tag,
	    "Number of frames RX'ed with offload VLAN tag");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rx_jumbo_frag",
	    CTLFLAG_RD, &sc->sc_drv_stats.rx_desc_jumbo_frag,
	    "Number of descriptors RX'ed as part of a multi-descriptor frame");

	/* RX checksum offload counters */
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rx_offload_csum_ipv4_exists",
	    CTLFLAG_RD, &sc->sc_drv_stats.rx_offload_csum_ipv4_exists,
	    "Number of frames RX'ed with IPv4 checksum offload set");
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rx_offload_csum_ipv4_valid",
	    CTLFLAG_RD, &sc->sc_drv_stats.rx_offload_csum_ipv4_valid,
	    "Number of frames RX'ed with IPv4 checksum offload valid");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rx_offload_csum_tcp_exists",
	    CTLFLAG_RD, &sc->sc_drv_stats.rx_offload_csum_tcp_exists,
	    "Number of frames RX'ed with TCP checksum offload set");
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rx_offload_csum_tcp_valid",
	    CTLFLAG_RD, &sc->sc_drv_stats.rx_offload_csum_tcp_valid,
	    "Number of frames RX'ed with TCP checksum offload valid");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rx_offload_csum_udp_exists",
	    CTLFLAG_RD, &sc->sc_drv_stats.rx_offload_csum_udp_exists,
	    "Number of frames RX'ed with UDP checksum offload set");
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rx_offload_csum_udp_valid",
	    CTLFLAG_RD, &sc->sc_drv_stats.rx_offload_csum_udp_valid,
	    "Number of frames RX'ed with UDP checksum offload valid");
}

static void
rge_sysctl_mac_stats_attach(struct rge_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);
	struct rge_mac_stats *ss = &sc->sc_mac_stats;

	/* Create stats node */
	tree = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "mac_stats",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "mac statistics");
	child = SYSCTL_CHILDREN(tree);

	/* MAC statistics */
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rge_tx_ok", CTLFLAG_RD,
	    &ss->lcl_stats.rge_tx_ok, "");
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rge_rx_ok", CTLFLAG_RD,
	    &ss->lcl_stats.rge_rx_ok, "");

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rge_tx_er", CTLFLAG_RD,
	    &ss->lcl_stats.rge_tx_er, "");
	/* uint32_t rge_rx_er */

	/* uint16_t rge_miss_pkt */
	/* uint16_t rge_fae */
	/* uint32_t rge_tx_1col */
	/* uint32_t rge_tx_mcol */

	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rge_rx_ok_phy", CTLFLAG_RD,
	    &ss->lcl_stats.rge_rx_ok_phy, "");
	SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, "rge_rx_ok_brd", CTLFLAG_RD,
	    &ss->lcl_stats.rge_rx_ok_brd, "");

	/* uint32_t rge_rx_ok_mul */
	/* uint16_t rge_tx_abt */
	/* uint16_t rge_tx_undrn */
}

void
rge_sysctl_attach(struct rge_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);

	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "debug", CTLFLAG_RW, &sc->sc_debug, 0,
	    "control debugging printfs");

	/* Stats */
	rge_sysctl_drv_stats_attach(sc);
	rge_sysctl_mac_stats_attach(sc);
}
