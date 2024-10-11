/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2023 Dmitry Salychev
 * Copyright © 2022 Mathew McBride
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
/*
 * The DPAA2 Network Interface (DPNI) driver.
 *
 * The DPNI object is a network interface that is configurable to support a wide
 * range of features from a very basic Ethernet interface up to a
 * high-functioning network interface. The DPNI supports features that are
 * expected by standard network stacks, from basic features to offloads.
 *
 * DPNIs work with Ethernet traffic, starting with the L2 header. Additional
 * functions are provided for standard network protocols (L2, L3, L4, etc.).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/taskqueue.h>
#include <sys/sysctl.h>
#include <sys/buf_ring.h>
#include <sys/smp.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/atomic.h>
#include <machine/vmparam.h>

#include <net/ethernet.h>
#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <dev/pci/pcivar.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mdio/mdio.h>

#include "opt_acpi.h"
#include "opt_platform.h"

#include "pcib_if.h"
#include "pci_if.h"
#include "miibus_if.h"
#include "memac_mdio_if.h"

#include "dpaa2_types.h"
#include "dpaa2_mc.h"
#include "dpaa2_mc_if.h"
#include "dpaa2_mcp.h"
#include "dpaa2_swp.h"
#include "dpaa2_swp_if.h"
#include "dpaa2_cmd_if.h"
#include "dpaa2_ni.h"
#include "dpaa2_channel.h"
#include "dpaa2_buf.h"

#define BIT(x)			(1ul << (x))
#define WRIOP_VERSION(x, y, z)	((x) << 10 | (y) << 5 | (z) << 0)
#define ARRAY_SIZE(a)		(sizeof(a) / sizeof((a)[0]))

/* Frame Dequeue Response status bits. */
#define IS_NULL_RESPONSE(stat)	((((stat) >> 4) & 1) == 0)

#define	ALIGN_UP(x, y)		roundup2((x), (y))
#define	ALIGN_DOWN(x, y)	rounddown2((x), (y))
#define CACHE_LINE_ALIGN(x)	ALIGN_UP((x), CACHE_LINE_SIZE)

#define DPNI_LOCK(__sc) do {			\
	mtx_assert(&(__sc)->lock, MA_NOTOWNED);	\
	mtx_lock(&(__sc)->lock);		\
} while (0)
#define	DPNI_UNLOCK(__sc) do {			\
	mtx_assert(&(__sc)->lock, MA_OWNED);	\
	mtx_unlock(&(__sc)->lock);		\
} while (0)
#define	DPNI_LOCK_ASSERT(__sc) do {		\
	mtx_assert(&(__sc)->lock, MA_OWNED);	\
} while (0)

#define DPAA2_TX_RING(sc, chan, tc) \
	(&(sc)->channels[(chan)]->txc_queue.tx_rings[(tc)])

MALLOC_DEFINE(M_DPAA2_TXB, "dpaa2_txb", "DPAA2 DMA-mapped buffer (Tx)");

/*
 * How many times channel cleanup routine will be repeated if the RX or TX
 * budget was depleted.
 */
#define DPAA2_CLEAN_BUDGET	64 /* sysctl(9)? */
/* TX/RX budget for the channel cleanup task */
#define DPAA2_TX_BUDGET		128 /* sysctl(9)? */
#define DPAA2_RX_BUDGET		256 /* sysctl(9)? */

#define DPNI_IRQ_INDEX		0 /* Index of the only DPNI IRQ. */
#define DPNI_IRQ_LINK_CHANGED	1 /* Link state changed */
#define DPNI_IRQ_EP_CHANGED	2 /* DPAA2 endpoint dis/connected */

/* Default maximum RX frame length w/o CRC. */
#define	DPAA2_ETH_MFL		(ETHER_MAX_LEN_JUMBO + ETHER_VLAN_ENCAP_LEN - \
    ETHER_CRC_LEN)

/* Minimally supported version of the DPNI API. */
#define DPNI_VER_MAJOR		7
#define DPNI_VER_MINOR		0

/* Rx/Tx buffers configuration. */
#define BUF_ALIGN_V1		256 /* WRIOP v1.0.0 limitation */
#define BUF_ALIGN		64
#define BUF_SWA_SIZE		64  /* SW annotation size */
#define BUF_RX_HWA_SIZE		64  /* HW annotation size */
#define BUF_TX_HWA_SIZE		128 /* HW annotation size */

#define DPAA2_RX_BUFRING_SZ	(4096u)
#define DPAA2_RXE_BUFRING_SZ	(1024u)
#define DPAA2_TXC_BUFRING_SZ	(4096u)
#define DPAA2_TX_SEGLIMIT	(16u) /* arbitrary number */
#define DPAA2_TX_SEG_SZ		(PAGE_SIZE)
#define DPAA2_TX_SEGS_MAXSZ	(DPAA2_TX_SEGLIMIT * DPAA2_TX_SEG_SZ)
#define DPAA2_TX_SGT_SZ		(PAGE_SIZE) /* bytes */

/* Size of a buffer to keep a QoS table key configuration. */
#define ETH_QOS_KCFG_BUF_SIZE	(PAGE_SIZE)

/* Required by struct dpni_rx_tc_dist_cfg::key_cfg_iova */
#define DPAA2_CLASSIFIER_DMA_SIZE (PAGE_SIZE)

/* Buffers layout options. */
#define BUF_LOPT_TIMESTAMP	0x1
#define BUF_LOPT_PARSER_RESULT	0x2
#define BUF_LOPT_FRAME_STATUS	0x4
#define BUF_LOPT_PRIV_DATA_SZ	0x8
#define BUF_LOPT_DATA_ALIGN	0x10
#define BUF_LOPT_DATA_HEAD_ROOM	0x20
#define BUF_LOPT_DATA_TAIL_ROOM	0x40

#define DPAA2_NI_BUF_ADDR_MASK	(0x1FFFFFFFFFFFFul) /* 49-bit addresses max. */
#define DPAA2_NI_BUF_CHAN_MASK	(0xFu)
#define DPAA2_NI_BUF_CHAN_SHIFT	(60)
#define DPAA2_NI_BUF_IDX_MASK	(0x7FFFu)
#define DPAA2_NI_BUF_IDX_SHIFT	(49)
#define DPAA2_NI_TX_IDX_MASK	(0x7u)
#define DPAA2_NI_TX_IDX_SHIFT	(57)
#define DPAA2_NI_TXBUF_IDX_MASK	(0xFFu)
#define DPAA2_NI_TXBUF_IDX_SHIFT (49)

#define DPAA2_NI_FD_FMT_MASK	(0x3u)
#define DPAA2_NI_FD_FMT_SHIFT	(12)
#define DPAA2_NI_FD_ERR_MASK	(0xFFu)
#define DPAA2_NI_FD_ERR_SHIFT	(0)
#define DPAA2_NI_FD_SL_MASK	(0x1u)
#define DPAA2_NI_FD_SL_SHIFT	(14)
#define DPAA2_NI_FD_LEN_MASK	(0x3FFFFu)
#define DPAA2_NI_FD_OFFSET_MASK (0x0FFFu)

/* Enables TCAM for Flow Steering and QoS look-ups. */
#define DPNI_OPT_HAS_KEY_MASKING 0x10

/* Unique IDs for the supported Rx classification header fields. */
#define DPAA2_ETH_DIST_ETHDST	BIT(0)
#define DPAA2_ETH_DIST_ETHSRC	BIT(1)
#define DPAA2_ETH_DIST_ETHTYPE	BIT(2)
#define DPAA2_ETH_DIST_VLAN	BIT(3)
#define DPAA2_ETH_DIST_IPSRC	BIT(4)
#define DPAA2_ETH_DIST_IPDST	BIT(5)
#define DPAA2_ETH_DIST_IPPROTO	BIT(6)
#define DPAA2_ETH_DIST_L4SRC	BIT(7)
#define DPAA2_ETH_DIST_L4DST	BIT(8)
#define DPAA2_ETH_DIST_ALL	(~0ULL)

/* L3-L4 network traffic flow hash options. */
#define	RXH_L2DA		(1 << 1)
#define	RXH_VLAN		(1 << 2)
#define	RXH_L3_PROTO		(1 << 3)
#define	RXH_IP_SRC		(1 << 4)
#define	RXH_IP_DST		(1 << 5)
#define	RXH_L4_B_0_1		(1 << 6) /* src port in case of TCP/UDP/SCTP */
#define	RXH_L4_B_2_3		(1 << 7) /* dst port in case of TCP/UDP/SCTP */
#define	RXH_DISCARD		(1 << 31)

/* Default Rx hash options, set during attaching. */
#define DPAA2_RXH_DEFAULT	(RXH_IP_SRC | RXH_IP_DST | RXH_L4_B_0_1 | RXH_L4_B_2_3)

MALLOC_DEFINE(M_DPAA2_NI, "dpaa2_ni", "DPAA2 Network Interface");

/*
 * DPAA2 Network Interface resource specification.
 *
 * NOTE: Don't forget to update macros in dpaa2_ni.h in case of any changes in
 *       the specification!
 */
struct resource_spec dpaa2_ni_spec[] = {
	/*
	 * DPMCP resources.
	 *
	 * NOTE: MC command portals (MCPs) are used to send commands to, and
	 *	 receive responses from, the MC firmware. One portal per DPNI.
	 */
	{ DPAA2_DEV_MCP, DPAA2_NI_MCP_RID(0),   RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	/*
	 * DPIO resources (software portals).
	 *
	 * NOTE: One per running core. While DPIOs are the source of data
	 *	 availability interrupts, the DPCONs are used to identify the
	 *	 network interface that has produced ingress data to that core.
	 */
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(0),    RF_ACTIVE | RF_SHAREABLE },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(1),    RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(2),    RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(3),    RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(4),    RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(5),    RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(6),    RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(7),    RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(8),    RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(9),    RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(10),   RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(11),   RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(12),   RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(13),   RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(14),   RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	{ DPAA2_DEV_IO,  DPAA2_NI_IO_RID(15),   RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	/*
	 * DPBP resources (buffer pools).
	 *
	 * NOTE: One per network interface.
	 */
	{ DPAA2_DEV_BP,  DPAA2_NI_BP_RID(0),   RF_ACTIVE },
	/*
	 * DPCON resources (channels).
	 *
	 * NOTE: One DPCON per core where Rx or Tx confirmation traffic to be
	 *	 distributed to.
	 * NOTE: Since it is necessary to distinguish between traffic from
	 *	 different network interfaces arriving on the same core, the
	 *	 DPCONs must be private to the DPNIs.
	 */
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(0),   RF_ACTIVE },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(1),   RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(2),   RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(3),   RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(4),   RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(5),   RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(6),   RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(7),   RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(8),   RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(9),   RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(10),  RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(11),  RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(12),  RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(13),  RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(14),  RF_ACTIVE | RF_OPTIONAL },
	{ DPAA2_DEV_CON, DPAA2_NI_CON_RID(15),  RF_ACTIVE | RF_OPTIONAL },

	RESOURCE_SPEC_END
};

/* Supported header fields for Rx hash distribution key */
static const struct dpaa2_eth_dist_fields dist_fields[] = {
	{
		/* L2 header */
		.rxnfc_field = RXH_L2DA,
		.cls_prot = NET_PROT_ETH,
		.cls_field = NH_FLD_ETH_DA,
		.id = DPAA2_ETH_DIST_ETHDST,
		.size = 6,
	}, {
		.cls_prot = NET_PROT_ETH,
		.cls_field = NH_FLD_ETH_SA,
		.id = DPAA2_ETH_DIST_ETHSRC,
		.size = 6,
	}, {
		/* This is the last ethertype field parsed:
		 * depending on frame format, it can be the MAC ethertype
		 * or the VLAN etype.
		 */
		.cls_prot = NET_PROT_ETH,
		.cls_field = NH_FLD_ETH_TYPE,
		.id = DPAA2_ETH_DIST_ETHTYPE,
		.size = 2,
	}, {
		/* VLAN header */
		.rxnfc_field = RXH_VLAN,
		.cls_prot = NET_PROT_VLAN,
		.cls_field = NH_FLD_VLAN_TCI,
		.id = DPAA2_ETH_DIST_VLAN,
		.size = 2,
	}, {
		/* IP header */
		.rxnfc_field = RXH_IP_SRC,
		.cls_prot = NET_PROT_IP,
		.cls_field = NH_FLD_IP_SRC,
		.id = DPAA2_ETH_DIST_IPSRC,
		.size = 4,
	}, {
		.rxnfc_field = RXH_IP_DST,
		.cls_prot = NET_PROT_IP,
		.cls_field = NH_FLD_IP_DST,
		.id = DPAA2_ETH_DIST_IPDST,
		.size = 4,
	}, {
		.rxnfc_field = RXH_L3_PROTO,
		.cls_prot = NET_PROT_IP,
		.cls_field = NH_FLD_IP_PROTO,
		.id = DPAA2_ETH_DIST_IPPROTO,
		.size = 1,
	}, {
		/* Using UDP ports, this is functionally equivalent to raw
		 * byte pairs from L4 header.
		 */
		.rxnfc_field = RXH_L4_B_0_1,
		.cls_prot = NET_PROT_UDP,
		.cls_field = NH_FLD_UDP_PORT_SRC,
		.id = DPAA2_ETH_DIST_L4SRC,
		.size = 2,
	}, {
		.rxnfc_field = RXH_L4_B_2_3,
		.cls_prot = NET_PROT_UDP,
		.cls_field = NH_FLD_UDP_PORT_DST,
		.id = DPAA2_ETH_DIST_L4DST,
		.size = 2,
	},
};

static struct dpni_stat {
	int	 page;
	int	 cnt;
	char	*name;
	char	*desc;
} dpni_stat_sysctls[DPAA2_NI_STAT_SYSCTLS] = {
	/* PAGE, COUNTER, NAME, DESCRIPTION */
	{  0, 0, "in_all_frames",	"All accepted ingress frames" },
	{  0, 1, "in_all_bytes",	"Bytes in all accepted ingress frames" },
	{  0, 2, "in_multi_frames",	"Multicast accepted ingress frames" },
	{  1, 0, "eg_all_frames",	"All egress frames transmitted" },
	{  1, 1, "eg_all_bytes",	"Bytes in all frames transmitted" },
	{  1, 2, "eg_multi_frames",	"Multicast egress frames transmitted" },
	{  2, 0, "in_filtered_frames",	"All ingress frames discarded due to "
	   				"filtering" },
	{  2, 1, "in_discarded_frames",	"All frames discarded due to errors" },
	{  2, 2, "in_nobuf_discards",	"Discards on ingress side due to buffer "
	   				"depletion in DPNI buffer pools" },
};

struct dpaa2_ni_rx_ctx {
	struct mbuf	*head;
	struct mbuf	*tail;
	int		 cnt;
	bool		 last;
};

/* Device interface */
static int dpaa2_ni_probe(device_t);
static int dpaa2_ni_attach(device_t);
static int dpaa2_ni_detach(device_t);

/* DPAA2 network interface setup and configuration */
static int dpaa2_ni_setup(device_t);
static int dpaa2_ni_setup_channels(device_t);
static int dpaa2_ni_bind(device_t);
static int dpaa2_ni_setup_rx_dist(device_t);
static int dpaa2_ni_setup_irqs(device_t);
static int dpaa2_ni_setup_msi(struct dpaa2_ni_softc *);
static int dpaa2_ni_setup_if_caps(struct dpaa2_ni_softc *);
static int dpaa2_ni_setup_if_flags(struct dpaa2_ni_softc *);
static int dpaa2_ni_setup_sysctls(struct dpaa2_ni_softc *);
static int dpaa2_ni_setup_dma(struct dpaa2_ni_softc *);

/* Tx/Rx flow configuration */
static int dpaa2_ni_setup_rx_flow(device_t, struct dpaa2_ni_fq *);
static int dpaa2_ni_setup_tx_flow(device_t, struct dpaa2_ni_fq *);
static int dpaa2_ni_setup_rx_err_flow(device_t, struct dpaa2_ni_fq *);

/* Configuration subroutines */
static int dpaa2_ni_set_buf_layout(device_t);
static int dpaa2_ni_set_pause_frame(device_t);
static int dpaa2_ni_set_qos_table(device_t);
static int dpaa2_ni_set_mac_addr(device_t);
static int dpaa2_ni_set_hash(device_t, uint64_t);
static int dpaa2_ni_set_dist_key(device_t, enum dpaa2_ni_dist_mode, uint64_t);

/* Frame descriptor routines */
static int dpaa2_ni_build_fd(struct dpaa2_ni_softc *, struct dpaa2_ni_tx_ring *,
    struct dpaa2_buf *, bus_dma_segment_t *, int, struct dpaa2_fd *);
static int dpaa2_ni_fd_err(struct dpaa2_fd *);
static uint32_t dpaa2_ni_fd_data_len(struct dpaa2_fd *);
static int dpaa2_ni_fd_format(struct dpaa2_fd *);
static bool dpaa2_ni_fd_short_len(struct dpaa2_fd *);
static int dpaa2_ni_fd_offset(struct dpaa2_fd *);

/* Various subroutines */
static int dpaa2_ni_cmp_api_version(struct dpaa2_ni_softc *, uint16_t, uint16_t);
static int dpaa2_ni_prepare_key_cfg(struct dpkg_profile_cfg *, uint8_t *);

/* Network interface routines */
static void dpaa2_ni_init(void *);
static int  dpaa2_ni_transmit(if_t , struct mbuf *);
static void dpaa2_ni_qflush(if_t );
static int  dpaa2_ni_ioctl(if_t , u_long, caddr_t);
static int  dpaa2_ni_update_mac_filters(if_t );
static u_int dpaa2_ni_add_maddr(void *, struct sockaddr_dl *, u_int);

/* Interrupt handlers */
static void dpaa2_ni_intr(void *);

/* MII handlers */
static void dpaa2_ni_miibus_statchg(device_t);
static int  dpaa2_ni_media_change(if_t );
static void dpaa2_ni_media_status(if_t , struct ifmediareq *);
static void dpaa2_ni_media_tick(void *);

/* Tx/Rx routines. */
static int dpaa2_ni_rx_cleanup(struct dpaa2_channel *);
static int dpaa2_ni_tx_cleanup(struct dpaa2_channel *);
static void dpaa2_ni_tx(struct dpaa2_ni_softc *, struct dpaa2_channel *,
    struct dpaa2_ni_tx_ring *, struct mbuf *);
static void dpaa2_ni_cleanup_task(void *, int);

/* Tx/Rx subroutines */
static int dpaa2_ni_consume_frames(struct dpaa2_channel *, struct dpaa2_ni_fq **,
    uint32_t *);
static int dpaa2_ni_rx(struct dpaa2_channel *, struct dpaa2_ni_fq *,
    struct dpaa2_fd *, struct dpaa2_ni_rx_ctx *);
static int dpaa2_ni_rx_err(struct dpaa2_channel *, struct dpaa2_ni_fq *,
    struct dpaa2_fd *);
static int dpaa2_ni_tx_conf(struct dpaa2_channel *, struct dpaa2_ni_fq *,
    struct dpaa2_fd *);

/* sysctl(9) */
static int dpaa2_ni_collect_stats(SYSCTL_HANDLER_ARGS);
static int dpaa2_ni_collect_buf_num(SYSCTL_HANDLER_ARGS);
static int dpaa2_ni_collect_buf_free(SYSCTL_HANDLER_ARGS);

static int
dpaa2_ni_probe(device_t dev)
{
	/* DPNI device will be added by a parent resource container itself. */
	device_set_desc(dev, "DPAA2 Network Interface");
	return (BUS_PROBE_DEFAULT);
}

static int
dpaa2_ni_attach(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	device_t mcp_dev;
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_devinfo *mcp_dinfo;
	struct dpaa2_cmd cmd;
	uint16_t rc_token, ni_token;
	if_t ifp;
	char tq_name[32];
	int error;

	sc->dev = dev;
	sc->ifp = NULL;
	sc->miibus = NULL;
	sc->mii = NULL;
	sc->media_status = 0;
	sc->if_flags = 0;
	sc->link_state = LINK_STATE_UNKNOWN;
	sc->buf_align = 0;

	/* For debug purposes only! */
	sc->rx_anomaly_frames = 0;
	sc->rx_single_buf_frames = 0;
	sc->rx_sg_buf_frames = 0;
	sc->rx_enq_rej_frames = 0;
	sc->rx_ieoi_err_frames = 0;
	sc->tx_single_buf_frames = 0;
	sc->tx_sg_frames = 0;

	DPAA2_ATOMIC_XCHG(&sc->buf_num, 0);
	DPAA2_ATOMIC_XCHG(&sc->buf_free, 0);

	sc->rxd_dmat = NULL;
	sc->qos_dmat = NULL;

	sc->qos_kcfg.dmap = NULL;
	sc->qos_kcfg.paddr = 0;
	sc->qos_kcfg.vaddr = NULL;

	sc->rxd_kcfg.dmap = NULL;
	sc->rxd_kcfg.paddr = 0;
	sc->rxd_kcfg.vaddr = NULL;

	sc->mac.dpmac_id = 0;
	sc->mac.phy_dev = NULL;
	memset(sc->mac.addr, 0, ETHER_ADDR_LEN);

	error = bus_alloc_resources(sc->dev, dpaa2_ni_spec, sc->res);
	if (error) {
		device_printf(dev, "%s: failed to allocate resources: "
		    "error=%d\n", __func__, error);
		goto err_exit;
	}

	/* Obtain MC portal. */
	mcp_dev = (device_t) rman_get_start(sc->res[DPAA2_NI_MCP_RID(0)]);
	mcp_dinfo = device_get_ivars(mcp_dev);
	dinfo->portal = mcp_dinfo->portal;

	mtx_init(&sc->lock, device_get_nameunit(dev), "dpaa2_ni", MTX_DEF);

	/* Allocate network interface */
	ifp = if_alloc(IFT_ETHER);
	sc->ifp = ifp;
	if_initname(ifp, DPAA2_NI_IFNAME, device_get_unit(sc->dev));

	if_setsoftc(ifp, sc);
	if_setflags(ifp, IFF_SIMPLEX | IFF_MULTICAST | IFF_BROADCAST);
	if_setinitfn(ifp, dpaa2_ni_init);
	if_setioctlfn(ifp, dpaa2_ni_ioctl);
	if_settransmitfn(ifp, dpaa2_ni_transmit);
	if_setqflushfn(ifp, dpaa2_ni_qflush);

	if_setcapabilities(ifp, IFCAP_VLAN_MTU | IFCAP_HWCSUM | IFCAP_JUMBO_MTU);
	if_setcapenable(ifp, if_getcapabilities(ifp));

	DPAA2_CMD_INIT(&cmd);

	/* Open resource container and network interface object. */
	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	bzero(tq_name, sizeof(tq_name));
	snprintf(tq_name, sizeof(tq_name), "%s_tqbp", device_get_nameunit(dev));

	/*
	 * XXX-DSL: Release new buffers on Buffer Pool State Change Notification
	 *          (BPSCN) returned as a result to the VDQ command instead.
	 *          It is similar to CDAN processed in dpaa2_io_intr().
	 */
	/* Create a taskqueue thread to release new buffers to the pool. */
	sc->bp_taskq = taskqueue_create(tq_name, M_WAITOK,
	    taskqueue_thread_enqueue, &sc->bp_taskq);
	taskqueue_start_threads(&sc->bp_taskq, 1, PI_NET, "%s", tq_name);

	/* sc->cleanup_taskq = taskqueue_create("dpaa2_ch cleanup", M_WAITOK, */
	/*     taskqueue_thread_enqueue, &sc->cleanup_taskq); */
	/* taskqueue_start_threads(&sc->cleanup_taskq, 1, PI_NET, */
	/*     "dpaa2_ch cleanup"); */

	error = dpaa2_ni_setup(dev);
	if (error) {
		device_printf(dev, "%s: failed to setup DPNI: error=%d\n",
		    __func__, error);
		goto close_ni;
	}
	error = dpaa2_ni_setup_channels(dev);
	if (error) {
		device_printf(dev, "%s: failed to setup QBMan channels: "
		    "error=%d\n", __func__, error);
		goto close_ni;
	}

	error = dpaa2_ni_bind(dev);
	if (error) {
		device_printf(dev, "%s: failed to bind DPNI: error=%d\n",
		    __func__, error);
		goto close_ni;
	}
	error = dpaa2_ni_setup_irqs(dev);
	if (error) {
		device_printf(dev, "%s: failed to setup IRQs: error=%d\n",
		    __func__, error);
		goto close_ni;
	}
	error = dpaa2_ni_setup_sysctls(sc);
	if (error) {
		device_printf(dev, "%s: failed to setup sysctls: error=%d\n",
		    __func__, error);
		goto close_ni;
	}

	ether_ifattach(sc->ifp, sc->mac.addr);
	callout_init(&sc->mii_callout, 0);

	return (0);

close_ni:
	DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (ENXIO);
}

static void
dpaa2_ni_fixed_media_status(if_t ifp, struct ifmediareq* ifmr)
{
	struct dpaa2_ni_softc *sc = if_getsoftc(ifp);

	DPNI_LOCK(sc);
	ifmr->ifm_count = 0;
	ifmr->ifm_mask = 0;
	ifmr->ifm_status = IFM_AVALID | IFM_ACTIVE;
	ifmr->ifm_current = ifmr->ifm_active =
	    sc->fixed_ifmedia.ifm_cur->ifm_media;

	/*
	 * In non-PHY usecases, we need to signal link state up, otherwise
	 * certain things requiring a link event (e.g async DHCP client) from
	 * devd do not happen.
	 */
	if (if_getlinkstate(ifp) == LINK_STATE_UNKNOWN) {
		if_link_state_change(ifp, LINK_STATE_UP);
	}

	/*
	 * TODO: Check the status of the link partner (DPMAC, DPNI or other) and
	 * reset if down. This is different to the DPAA2_MAC_LINK_TYPE_PHY as
	 * the MC firmware sets the status, instead of us telling the MC what
	 * it is.
	 */
	DPNI_UNLOCK(sc);

	return;
}

static void
dpaa2_ni_setup_fixed_link(struct dpaa2_ni_softc *sc)
{
	/*
	 * FIXME: When the DPNI is connected to a DPMAC, we can get the
	 * 'apparent' speed from it.
	 */
	sc->fixed_link = true;

	ifmedia_init(&sc->fixed_ifmedia, 0, dpaa2_ni_media_change,
		     dpaa2_ni_fixed_media_status);
	ifmedia_add(&sc->fixed_ifmedia, IFM_ETHER | IFM_1000_T, 0, NULL);
	ifmedia_set(&sc->fixed_ifmedia, IFM_ETHER | IFM_1000_T);
}

static int
dpaa2_ni_detach(device_t dev)
{
	/* TBD */
	return (0);
}

/**
 * @brief Configure DPAA2 network interface object.
 */
static int
dpaa2_ni_setup(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_ep_desc ep1_desc, ep2_desc; /* endpoint descriptors */
	struct dpaa2_cmd cmd;
	uint8_t eth_bca[ETHER_ADDR_LEN]; /* broadcast physical address */
	uint16_t rc_token, ni_token, mac_token;
	struct dpaa2_mac_attr attr;
	enum dpaa2_mac_link_type link_type;
	uint32_t link;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	/* Check if we can work with this DPNI object. */
	error = DPAA2_CMD_NI_GET_API_VERSION(dev, child, &cmd, &sc->api_major,
	    &sc->api_minor);
	if (error) {
		device_printf(dev, "%s: failed to get DPNI API version\n",
		    __func__);
		goto close_ni;
	}
	if (dpaa2_ni_cmp_api_version(sc, DPNI_VER_MAJOR, DPNI_VER_MINOR) < 0) {
		device_printf(dev, "%s: DPNI API version %u.%u not supported, "
		    "need >= %u.%u\n", __func__, sc->api_major, sc->api_minor,
		    DPNI_VER_MAJOR, DPNI_VER_MINOR);
		error = ENODEV;
		goto close_ni;
	}

	/* Reset the DPNI object. */
	error = DPAA2_CMD_NI_RESET(dev, child, &cmd);
	if (error) {
		device_printf(dev, "%s: failed to reset DPNI: id=%d\n",
		    __func__, dinfo->id);
		goto close_ni;
	}

	/* Obtain attributes of the DPNI object. */
	error = DPAA2_CMD_NI_GET_ATTRIBUTES(dev, child, &cmd, &sc->attr);
	if (error) {
		device_printf(dev, "%s: failed to obtain DPNI attributes: "
		    "id=%d\n", __func__, dinfo->id);
		goto close_ni;
	}
	if (bootverbose) {
		device_printf(dev, "\toptions=0x%#x queues=%d tx_channels=%d "
		    "wriop_version=%#x\n", sc->attr.options, sc->attr.num.queues,
		    sc->attr.num.channels, sc->attr.wriop_ver);
		device_printf(dev, "\ttraffic classes: rx=%d tx=%d "
		    "cgs_groups=%d\n", sc->attr.num.rx_tcs, sc->attr.num.tx_tcs,
		    sc->attr.num.cgs);
		device_printf(dev, "\ttable entries: mac=%d vlan=%d qos=%d "
		    "fs=%d\n", sc->attr.entries.mac, sc->attr.entries.vlan,
		    sc->attr.entries.qos, sc->attr.entries.fs);
		device_printf(dev, "\tkey sizes: qos=%d fs=%d\n",
		    sc->attr.key_size.qos, sc->attr.key_size.fs);
	}

	/* Configure buffer layouts of the DPNI queues. */
	error = dpaa2_ni_set_buf_layout(dev);
	if (error) {
		device_printf(dev, "%s: failed to configure buffer layout\n",
		    __func__);
		goto close_ni;
	}

	/* Configure DMA resources. */
	error = dpaa2_ni_setup_dma(sc);
	if (error) {
		device_printf(dev, "%s: failed to setup DMA\n", __func__);
		goto close_ni;
	}

	/* Setup link between DPNI and an object it's connected to. */
	ep1_desc.obj_id = dinfo->id;
	ep1_desc.if_id = 0; /* DPNI has the only endpoint */
	ep1_desc.type = dinfo->dtype;

	error = DPAA2_CMD_RC_GET_CONN(dev, child, DPAA2_CMD_TK(&cmd, rc_token),
	    &ep1_desc, &ep2_desc, &link);
	if (error) {
		device_printf(dev, "%s: failed to obtain an object DPNI is "
		    "connected to: error=%d\n", __func__, error);
	} else {
		device_printf(dev, "connected to %s (id=%d)\n",
		    dpaa2_ttos(ep2_desc.type), ep2_desc.obj_id);

		error = dpaa2_ni_set_mac_addr(dev);
		if (error) {
			device_printf(dev, "%s: failed to set MAC address: "
			    "error=%d\n", __func__, error);
		}

		if (ep2_desc.type == DPAA2_DEV_MAC) {
			/*
			 * This is the simplest case when DPNI is connected to
			 * DPMAC directly.
			 */
			sc->mac.dpmac_id = ep2_desc.obj_id;

			link_type = DPAA2_MAC_LINK_TYPE_NONE;

			/*
			 * Need to determine if DPMAC type is PHY (attached to
			 * conventional MII PHY) or FIXED (usually SFP/SerDes,
			 * link state managed by MC firmware).
			 */
			error = DPAA2_CMD_MAC_OPEN(sc->dev, child,
			    DPAA2_CMD_TK(&cmd, rc_token), sc->mac.dpmac_id,
			    &mac_token);
			/*
			 * Under VFIO, the DPMAC might be sitting in another
			 * container (DPRC) we don't have access to.
			 * Assume DPAA2_MAC_LINK_TYPE_FIXED if this is
			 * the case.
			 */
			if (error) {
				device_printf(dev, "%s: failed to open "
				    "connected DPMAC: %d (assuming in other DPRC)\n", __func__,
				    sc->mac.dpmac_id);
				link_type = DPAA2_MAC_LINK_TYPE_FIXED;
			} else {
				error = DPAA2_CMD_MAC_GET_ATTRIBUTES(dev, child,
				    &cmd, &attr);
				if (error) {
					device_printf(dev, "%s: failed to get "
					    "DPMAC attributes: id=%d, "
					    "error=%d\n", __func__, dinfo->id,
					    error);
				} else {
					link_type = attr.link_type;
				}
			}
			DPAA2_CMD_MAC_CLOSE(dev, child, &cmd);

			if (link_type == DPAA2_MAC_LINK_TYPE_FIXED) {
				device_printf(dev, "connected DPMAC is in FIXED "
				    "mode\n");
				dpaa2_ni_setup_fixed_link(sc);
			} else if (link_type == DPAA2_MAC_LINK_TYPE_PHY) {
				device_printf(dev, "connected DPMAC is in PHY "
				    "mode\n");
				error = DPAA2_MC_GET_PHY_DEV(dev,
				    &sc->mac.phy_dev, sc->mac.dpmac_id);
				if (error == 0) {
					error = MEMAC_MDIO_SET_NI_DEV(
					    sc->mac.phy_dev, dev);
					if (error != 0) {
						device_printf(dev, "%s: failed "
						    "to set dpni dev on memac "
						    "mdio dev %s: error=%d\n",
						    __func__,
						    device_get_nameunit(
						    sc->mac.phy_dev), error);
					}
				}
				if (error == 0) {
					error = MEMAC_MDIO_GET_PHY_LOC(
					    sc->mac.phy_dev, &sc->mac.phy_loc);
					if (error == ENODEV) {
						error = 0;
					}
					if (error != 0) {
						device_printf(dev, "%s: failed "
						    "to get phy location from "
						    "memac mdio dev %s: error=%d\n",
						    __func__, device_get_nameunit(
						    sc->mac.phy_dev), error);
					}
				}
				if (error == 0) {
					error = mii_attach(sc->mac.phy_dev,
					    &sc->miibus, sc->ifp,
					    dpaa2_ni_media_change,
					    dpaa2_ni_media_status,
					    BMSR_DEFCAPMASK, sc->mac.phy_loc,
					    MII_OFFSET_ANY, 0);
					if (error != 0) {
						device_printf(dev, "%s: failed "
						    "to attach to miibus: "
						    "error=%d\n",
						    __func__, error);
					}
				}
				if (error == 0) {
					sc->mii = device_get_softc(sc->miibus);
				}
			} else {
				device_printf(dev, "%s: DPMAC link type is not "
				    "supported\n", __func__);
			}
		} else if (ep2_desc.type == DPAA2_DEV_NI ||
			   ep2_desc.type == DPAA2_DEV_MUX ||
			   ep2_desc.type == DPAA2_DEV_SW) {
			dpaa2_ni_setup_fixed_link(sc);
		}
	}

	/* Select mode to enqueue frames. */
	/* ... TBD ... */

	/*
	 * Update link configuration to enable Rx/Tx pause frames support.
	 *
	 * NOTE: MC may generate an interrupt to the DPMAC and request changes
	 *       in link configuration. It might be necessary to attach miibus
	 *       and PHY before this point.
	 */
	error = dpaa2_ni_set_pause_frame(dev);
	if (error) {
		device_printf(dev, "%s: failed to configure Rx/Tx pause "
		    "frames\n", __func__);
		goto close_ni;
	}

	/* Configure ingress traffic classification. */
	error = dpaa2_ni_set_qos_table(dev);
	if (error) {
		device_printf(dev, "%s: failed to configure QoS table: "
		    "error=%d\n", __func__, error);
		goto close_ni;
	}

	/* Add broadcast physical address to the MAC filtering table. */
	memset(eth_bca, 0xff, ETHER_ADDR_LEN);
	error = DPAA2_CMD_NI_ADD_MAC_ADDR(dev, child, DPAA2_CMD_TK(&cmd,
	    ni_token), eth_bca);
	if (error) {
		device_printf(dev, "%s: failed to add broadcast physical "
		    "address to the MAC filtering table\n", __func__);
		goto close_ni;
	}

	/* Set the maximum allowed length for received frames. */
	error = DPAA2_CMD_NI_SET_MFL(dev, child, &cmd, DPAA2_ETH_MFL);
	if (error) {
		device_printf(dev, "%s: failed to set maximum length for "
		    "received frames\n", __func__);
		goto close_ni;
	}

	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
	return (0);

close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

/**
 * @brief Сonfigure QBMan channels and register data availability notifications.
 */
static int
dpaa2_ni_setup_channels(device_t dev)
{
	device_t iodev, condev, bpdev;
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	uint32_t i, num_chan;
	int error;

	/* Calculate number of the channels based on the allocated resources */
	for (i = 0; i < DPAA2_NI_IO_RES_NUM; i++) {
		if (!sc->res[DPAA2_NI_IO_RID(i)]) {
			break;
		}
	}
	num_chan = i;
	for (i = 0; i < DPAA2_NI_CON_RES_NUM; i++) {
		if (!sc->res[DPAA2_NI_CON_RID(i)]) {
			break;
		}
	}
	num_chan = i < num_chan ? i : num_chan;
	sc->chan_n = num_chan > DPAA2_MAX_CHANNELS
	    ? DPAA2_MAX_CHANNELS : num_chan;
	sc->chan_n = sc->chan_n > sc->attr.num.queues
	    ? sc->attr.num.queues : sc->chan_n;

	KASSERT(sc->chan_n > 0u, ("%s: positive number of channels expected: "
	    "chan_n=%d", __func__, sc->chan_n));

	device_printf(dev, "channels=%d\n", sc->chan_n);

	for (i = 0; i < sc->chan_n; i++) {
		iodev = (device_t)rman_get_start(sc->res[DPAA2_NI_IO_RID(i)]);
		condev = (device_t)rman_get_start(sc->res[DPAA2_NI_CON_RID(i)]);
		/* Only one buffer pool available at the moment */
		bpdev = (device_t)rman_get_start(sc->res[DPAA2_NI_BP_RID(0)]);

		error = dpaa2_chan_setup(dev, iodev, condev, bpdev,
		    &sc->channels[i], i, dpaa2_ni_cleanup_task);
		if (error != 0) {
			device_printf(dev, "%s: dpaa2_chan_setup() failed: "
			    "error=%d, chan_id=%d\n", __func__, error, i);
			return (error);
		}
	}

	/* There is exactly one Rx error queue per network interface */
	error = dpaa2_chan_setup_fq(dev, sc->channels[0], DPAA2_NI_QUEUE_RX_ERR);
	if (error != 0) {
		device_printf(dev, "%s: failed to prepare RxError queue: "
		    "error=%d\n", __func__, error);
		return (error);
	}

	return (0);
}

/**
 * @brief Bind DPNI to DPBPs, DPIOs, frame queues and channels.
 */
static int
dpaa2_ni_bind(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	device_t bp_dev;
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_devinfo *bp_info;
	struct dpaa2_cmd cmd;
	struct dpaa2_ni_pools_cfg pools_cfg;
	struct dpaa2_ni_err_cfg err_cfg;
	struct dpaa2_channel *chan;
	uint16_t rc_token, ni_token;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	/* Select buffer pool (only one available at the moment). */
	bp_dev = (device_t) rman_get_start(sc->res[DPAA2_NI_BP_RID(0)]);
	bp_info = device_get_ivars(bp_dev);

	/* Configure buffers pool. */
	pools_cfg.pools_num = 1;
	pools_cfg.pools[0].bp_obj_id = bp_info->id;
	pools_cfg.pools[0].backup_flag = 0;
	pools_cfg.pools[0].buf_sz = sc->buf_sz;
	error = DPAA2_CMD_NI_SET_POOLS(dev, child, &cmd, &pools_cfg);
	if (error) {
		device_printf(dev, "%s: failed to set buffer pools\n", __func__);
		goto close_ni;
	}

	/* Setup ingress traffic distribution. */
	error = dpaa2_ni_setup_rx_dist(dev);
	if (error && error != EOPNOTSUPP) {
		device_printf(dev, "%s: failed to setup ingress traffic "
		    "distribution\n", __func__);
		goto close_ni;
	}
	if (bootverbose && error == EOPNOTSUPP) {
		device_printf(dev, "Ingress traffic distribution not "
		    "supported\n");
	}

	/* Configure handling of error frames. */
	err_cfg.err_mask = DPAA2_NI_FAS_RX_ERR_MASK;
	err_cfg.set_err_fas = false;
	err_cfg.action = DPAA2_NI_ERR_DISCARD;
	error = DPAA2_CMD_NI_SET_ERR_BEHAVIOR(dev, child, &cmd, &err_cfg);
	if (error) {
		device_printf(dev, "%s: failed to set errors behavior\n",
		    __func__);
		goto close_ni;
	}

	/* Configure channel queues to generate CDANs. */
	for (uint32_t i = 0; i < sc->chan_n; i++) {
		chan = sc->channels[i];

		/* Setup Rx flows. */
		for (uint32_t j = 0; j < chan->rxq_n; j++) {
			error = dpaa2_ni_setup_rx_flow(dev, &chan->rx_queues[j]);
			if (error) {
				device_printf(dev, "%s: failed to setup Rx "
				    "flow: error=%d\n", __func__, error);
				goto close_ni;
			}
		}

		/* Setup Tx flow. */
		error = dpaa2_ni_setup_tx_flow(dev, &chan->txc_queue);
		if (error) {
			device_printf(dev, "%s: failed to setup Tx "
			    "flow: error=%d\n", __func__, error);
			goto close_ni;
		}
	}

	/* Configure RxError queue to generate CDAN. */
	error = dpaa2_ni_setup_rx_err_flow(dev, &sc->rxe_queue);
	if (error) {
		device_printf(dev, "%s: failed to setup RxError flow: "
		    "error=%d\n", __func__, error);
		goto close_ni;
	}

	/*
	 * Get the Queuing Destination ID (QDID) that should be used for frame
	 * enqueue operations.
	 */
	error = DPAA2_CMD_NI_GET_QDID(dev, child, &cmd, DPAA2_NI_QUEUE_TX,
	    &sc->tx_qdid);
	if (error) {
		device_printf(dev, "%s: failed to get Tx queuing destination "
		    "ID\n", __func__);
		goto close_ni;
	}

	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
	return (0);

close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

/**
 * @brief Setup ingress traffic distribution.
 *
 * NOTE: Ingress traffic distribution is valid only when DPNI_OPT_NO_FS option
 *	 hasn't been set for DPNI and a number of DPNI queues > 1.
 */
static int
dpaa2_ni_setup_rx_dist(device_t dev)
{
	/*
	 * Have the interface implicitly distribute traffic based on the default
	 * hash key.
	 */
	return (dpaa2_ni_set_hash(dev, DPAA2_RXH_DEFAULT));
}

static int
dpaa2_ni_setup_rx_flow(device_t dev, struct dpaa2_ni_fq *fq)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_devinfo *con_info;
	struct dpaa2_cmd cmd;
	struct dpaa2_ni_queue_cfg queue_cfg = {0};
	uint16_t rc_token, ni_token;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	/* Obtain DPCON associated with the FQ's channel. */
	con_info = device_get_ivars(fq->chan->con_dev);

	queue_cfg.type = DPAA2_NI_QUEUE_RX;
	queue_cfg.tc = fq->tc;
	queue_cfg.idx = fq->flowid;
	error = DPAA2_CMD_NI_GET_QUEUE(dev, child, &cmd, &queue_cfg);
	if (error) {
		device_printf(dev, "%s: failed to obtain Rx queue "
		    "configuration: tc=%d, flowid=%d\n", __func__, queue_cfg.tc,
		    queue_cfg.idx);
		goto close_ni;
	}

	fq->fqid = queue_cfg.fqid;

	queue_cfg.dest_id = con_info->id;
	queue_cfg.dest_type = DPAA2_NI_DEST_DPCON;
	queue_cfg.priority = 1;
	queue_cfg.user_ctx = (uint64_t)(uintmax_t) fq;
	queue_cfg.options =
	    DPAA2_NI_QUEUE_OPT_USER_CTX |
	    DPAA2_NI_QUEUE_OPT_DEST;
	error = DPAA2_CMD_NI_SET_QUEUE(dev, child, &cmd, &queue_cfg);
	if (error) {
		device_printf(dev, "%s: failed to update Rx queue "
		    "configuration: tc=%d, flowid=%d\n", __func__, queue_cfg.tc,
		    queue_cfg.idx);
		goto close_ni;
	}

	if (bootverbose) {
		device_printf(dev, "RX queue idx=%d, tc=%d, chan=%d, fqid=%d, "
		    "user_ctx=%#jx\n", fq->flowid, fq->tc, fq->chan->id,
		    fq->fqid, (uint64_t) fq);
	}

	(void)DPAA2_CMD_NI_CLOSE(dev, child, &cmd);
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
	return (0);

close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

static int
dpaa2_ni_setup_tx_flow(device_t dev, struct dpaa2_ni_fq *fq)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_channel *ch = fq->chan;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_devinfo *con_info;
	struct dpaa2_ni_queue_cfg queue_cfg = {0};
	struct dpaa2_ni_tx_ring *tx;
	struct dpaa2_buf *buf;
	struct dpaa2_cmd cmd;
	uint32_t tx_rings_n = 0;
	uint16_t rc_token, ni_token;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	/* Obtain DPCON associated with the FQ's channel. */
	con_info = device_get_ivars(fq->chan->con_dev);

	KASSERT(sc->attr.num.tx_tcs <= DPAA2_MAX_TCS,
	    ("%s: too many Tx traffic classes: tx_tcs=%d\n", __func__,
	    sc->attr.num.tx_tcs));
	KASSERT(DPAA2_NI_BUFS_PER_TX <= DPAA2_NI_MAX_BPTX,
	    ("%s: too many Tx buffers (%d): max=%d\n", __func__,
	    DPAA2_NI_BUFS_PER_TX, DPAA2_NI_MAX_BPTX));

	/* Setup Tx rings. */
	for (int i = 0; i < sc->attr.num.tx_tcs; i++) {
		queue_cfg.type = DPAA2_NI_QUEUE_TX;
		queue_cfg.tc = i;
		queue_cfg.idx = fq->flowid;
		queue_cfg.chan_id = fq->chan->id;

		error = DPAA2_CMD_NI_GET_QUEUE(dev, child, &cmd, &queue_cfg);
		if (error) {
			device_printf(dev, "%s: failed to obtain Tx queue "
			    "configuration: tc=%d, flowid=%d\n", __func__,
			    queue_cfg.tc, queue_cfg.idx);
			goto close_ni;
		}

		tx = &fq->tx_rings[i];
		tx->fq = fq;
		tx->fqid = queue_cfg.fqid;
		tx->txid = tx_rings_n;

		if (bootverbose) {
			device_printf(dev, "TX queue idx=%d, tc=%d, chan=%d, "
			    "fqid=%d\n", fq->flowid, i, fq->chan->id,
			    queue_cfg.fqid);
		}

		mtx_init(&tx->lock, "dpaa2_tx_ring", NULL, MTX_DEF);

		/* Allocate Tx ring buffer. */
		tx->br = buf_ring_alloc(DPAA2_TX_BUFRING_SZ, M_DEVBUF, M_NOWAIT,
		    &tx->lock);
		if (tx->br == NULL) {
			device_printf(dev, "%s: failed to setup Tx ring buffer"
			    " (2) fqid=%d\n", __func__, tx->fqid);
			goto close_ni;
		}

		/* Configure Tx buffers */
		for (uint64_t j = 0; j < DPAA2_NI_BUFS_PER_TX; j++) {
			buf = malloc(sizeof(struct dpaa2_buf), M_DPAA2_TXB,
			    M_WAITOK);
			/* Keep DMA tag and Tx ring linked to the buffer */
			DPAA2_BUF_INIT_TAGOPT(buf, ch->tx_dmat, tx);

			buf->sgt = malloc(sizeof(struct dpaa2_buf), M_DPAA2_TXB,
			    M_WAITOK);
			/* Link SGT to DMA tag and back to its Tx buffer */
			DPAA2_BUF_INIT_TAGOPT(buf->sgt, ch->sgt_dmat, buf);

			error = dpaa2_buf_seed_txb(dev, buf);

			/* Add Tx buffer to the ring */
			buf_ring_enqueue(tx->br, buf);
		}

		tx_rings_n++;
	}

	/* All Tx queues which belong to the same flowid have the same qdbin. */
	fq->tx_qdbin = queue_cfg.qdbin;

	queue_cfg.type = DPAA2_NI_QUEUE_TX_CONF;
	queue_cfg.tc = 0; /* ignored for TxConf queue */
	queue_cfg.idx = fq->flowid;
	error = DPAA2_CMD_NI_GET_QUEUE(dev, child, &cmd, &queue_cfg);
	if (error) {
		device_printf(dev, "%s: failed to obtain TxConf queue "
		    "configuration: tc=%d, flowid=%d\n", __func__, queue_cfg.tc,
		    queue_cfg.idx);
		goto close_ni;
	}

	fq->fqid = queue_cfg.fqid;

	queue_cfg.dest_id = con_info->id;
	queue_cfg.dest_type = DPAA2_NI_DEST_DPCON;
	queue_cfg.priority = 0;
	queue_cfg.user_ctx = (uint64_t)(uintmax_t) fq;
	queue_cfg.options =
	    DPAA2_NI_QUEUE_OPT_USER_CTX |
	    DPAA2_NI_QUEUE_OPT_DEST;
	error = DPAA2_CMD_NI_SET_QUEUE(dev, child, &cmd, &queue_cfg);
	if (error) {
		device_printf(dev, "%s: failed to update TxConf queue "
		    "configuration: tc=%d, flowid=%d\n", __func__, queue_cfg.tc,
		    queue_cfg.idx);
		goto close_ni;
	}

	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
	return (0);

close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

static int
dpaa2_ni_setup_rx_err_flow(device_t dev, struct dpaa2_ni_fq *fq)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_devinfo *con_info;
	struct dpaa2_ni_queue_cfg queue_cfg = {0};
	struct dpaa2_cmd cmd;
	uint16_t rc_token, ni_token;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	/* Obtain DPCON associated with the FQ's channel. */
	con_info = device_get_ivars(fq->chan->con_dev);

	queue_cfg.type = DPAA2_NI_QUEUE_RX_ERR;
	queue_cfg.tc = fq->tc; /* ignored */
	queue_cfg.idx = fq->flowid; /* ignored */
	error = DPAA2_CMD_NI_GET_QUEUE(dev, child, &cmd, &queue_cfg);
	if (error) {
		device_printf(dev, "%s: failed to obtain RxErr queue "
		    "configuration\n", __func__);
		goto close_ni;
	}

	fq->fqid = queue_cfg.fqid;

	queue_cfg.dest_id = con_info->id;
	queue_cfg.dest_type = DPAA2_NI_DEST_DPCON;
	queue_cfg.priority = 1;
	queue_cfg.user_ctx = (uint64_t)(uintmax_t) fq;
	queue_cfg.options =
	    DPAA2_NI_QUEUE_OPT_USER_CTX |
	    DPAA2_NI_QUEUE_OPT_DEST;
	error = DPAA2_CMD_NI_SET_QUEUE(dev, child, &cmd, &queue_cfg);
	if (error) {
		device_printf(dev, "%s: failed to update RxErr queue "
		    "configuration\n", __func__);
		goto close_ni;
	}

	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
	return (0);

close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

/**
 * @brief Configure DPNI object to generate interrupts.
 */
static int
dpaa2_ni_setup_irqs(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_cmd cmd;
	uint16_t rc_token, ni_token;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	/* Configure IRQs. */
	error = dpaa2_ni_setup_msi(sc);
	if (error) {
		device_printf(dev, "%s: failed to allocate MSI\n", __func__);
		goto close_ni;
	}
	if ((sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->irq_rid[0], RF_ACTIVE | RF_SHAREABLE)) == NULL) {
		device_printf(dev, "%s: failed to allocate IRQ resource\n",
		    __func__);
		goto close_ni;
	}
	if (bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, dpaa2_ni_intr, sc, &sc->intr)) {
		device_printf(dev, "%s: failed to setup IRQ resource\n",
		    __func__);
		goto close_ni;
	}

	error = DPAA2_CMD_NI_SET_IRQ_MASK(dev, child, &cmd, DPNI_IRQ_INDEX,
	    DPNI_IRQ_LINK_CHANGED | DPNI_IRQ_EP_CHANGED);
	if (error) {
		device_printf(dev, "%s: failed to set DPNI IRQ mask\n",
		    __func__);
		goto close_ni;
	}

	error = DPAA2_CMD_NI_SET_IRQ_ENABLE(dev, child, &cmd, DPNI_IRQ_INDEX,
	    true);
	if (error) {
		device_printf(dev, "%s: failed to enable DPNI IRQ\n", __func__);
		goto close_ni;
	}

	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
	return (0);

close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

/**
 * @brief Allocate MSI interrupts for DPNI.
 */
static int
dpaa2_ni_setup_msi(struct dpaa2_ni_softc *sc)
{
	int val;

	val = pci_msi_count(sc->dev);
	if (val < DPAA2_NI_MSI_COUNT)
		device_printf(sc->dev, "MSI: actual=%d, expected=%d\n", val,
		    DPAA2_IO_MSI_COUNT);
	val = MIN(val, DPAA2_NI_MSI_COUNT);

	if (pci_alloc_msi(sc->dev, &val) != 0)
		return (EINVAL);

	for (int i = 0; i < val; i++)
		sc->irq_rid[i] = i + 1;

	return (0);
}

/**
 * @brief Update DPNI according to the updated interface capabilities.
 */
static int
dpaa2_ni_setup_if_caps(struct dpaa2_ni_softc *sc)
{
	const bool en_rxcsum = if_getcapenable(sc->ifp) & IFCAP_RXCSUM;
	const bool en_txcsum = if_getcapenable(sc->ifp) & IFCAP_TXCSUM;
	device_t pdev = device_get_parent(sc->dev);
	device_t dev = sc->dev;
	device_t child = dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_cmd cmd;
	uint16_t rc_token, ni_token;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	/* Setup checksums validation. */
	error = DPAA2_CMD_NI_SET_OFFLOAD(dev, child, &cmd,
	    DPAA2_NI_OFL_RX_L3_CSUM, en_rxcsum);
	if (error) {
		device_printf(dev, "%s: failed to %s L3 checksum validation\n",
		    __func__, en_rxcsum ? "enable" : "disable");
		goto close_ni;
	}
	error = DPAA2_CMD_NI_SET_OFFLOAD(dev, child, &cmd,
	    DPAA2_NI_OFL_RX_L4_CSUM, en_rxcsum);
	if (error) {
		device_printf(dev, "%s: failed to %s L4 checksum validation\n",
		    __func__, en_rxcsum ? "enable" : "disable");
		goto close_ni;
	}

	/* Setup checksums generation. */
	error = DPAA2_CMD_NI_SET_OFFLOAD(dev, child, &cmd,
	    DPAA2_NI_OFL_TX_L3_CSUM, en_txcsum);
	if (error) {
		device_printf(dev, "%s: failed to %s L3 checksum generation\n",
		    __func__, en_txcsum ? "enable" : "disable");
		goto close_ni;
	}
	error = DPAA2_CMD_NI_SET_OFFLOAD(dev, child, &cmd,
	    DPAA2_NI_OFL_TX_L4_CSUM, en_txcsum);
	if (error) {
		device_printf(dev, "%s: failed to %s L4 checksum generation\n",
		    __func__, en_txcsum ? "enable" : "disable");
		goto close_ni;
	}

	(void)DPAA2_CMD_NI_CLOSE(dev, child, &cmd);
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
	return (0);

close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

/**
 * @brief Update DPNI according to the updated interface flags.
 */
static int
dpaa2_ni_setup_if_flags(struct dpaa2_ni_softc *sc)
{
	const bool en_promisc = if_getflags(sc->ifp) & IFF_PROMISC;
	const bool en_allmulti = if_getflags(sc->ifp) & IFF_ALLMULTI;
	device_t pdev = device_get_parent(sc->dev);
	device_t dev = sc->dev;
	device_t child = dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_cmd cmd;
	uint16_t rc_token, ni_token;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	error = DPAA2_CMD_NI_SET_MULTI_PROMISC(dev, child, &cmd,
	    en_promisc ? true : en_allmulti);
	if (error) {
		device_printf(dev, "%s: failed to %s multicast promiscuous "
		    "mode\n", __func__, en_allmulti ? "enable" : "disable");
		goto close_ni;
	}

	error = DPAA2_CMD_NI_SET_UNI_PROMISC(dev, child, &cmd, en_promisc);
	if (error) {
		device_printf(dev, "%s: failed to %s unicast promiscuous mode\n",
		    __func__, en_promisc ? "enable" : "disable");
		goto close_ni;
	}

	(void)DPAA2_CMD_NI_CLOSE(dev, child, &cmd);
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
	return (0);

close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

static int
dpaa2_ni_setup_sysctls(struct dpaa2_ni_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *node, *node2;
	struct sysctl_oid_list *parent, *parent2;
	char cbuf[128];
	int i;

	ctx = device_get_sysctl_ctx(sc->dev);
	parent = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev));

	/* Add DPNI statistics. */
	node = SYSCTL_ADD_NODE(ctx, parent, OID_AUTO, "stats",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "DPNI Statistics");
	parent = SYSCTL_CHILDREN(node);
	for (i = 0; i < DPAA2_NI_STAT_SYSCTLS; ++i) {
		SYSCTL_ADD_PROC(ctx, parent, i, dpni_stat_sysctls[i].name,
		    CTLTYPE_U64 | CTLFLAG_RD, sc, 0, dpaa2_ni_collect_stats,
		    "IU", dpni_stat_sysctls[i].desc);
	}
	SYSCTL_ADD_UQUAD(ctx, parent, OID_AUTO, "rx_anomaly_frames",
	    CTLFLAG_RD, &sc->rx_anomaly_frames,
	    "Rx frames in the buffers outside of the buffer pools");
	SYSCTL_ADD_UQUAD(ctx, parent, OID_AUTO, "rx_single_buf_frames",
	    CTLFLAG_RD, &sc->rx_single_buf_frames,
	    "Rx frames in single buffers");
	SYSCTL_ADD_UQUAD(ctx, parent, OID_AUTO, "rx_sg_buf_frames",
	    CTLFLAG_RD, &sc->rx_sg_buf_frames,
	    "Rx frames in scatter/gather list");
	SYSCTL_ADD_UQUAD(ctx, parent, OID_AUTO, "rx_enq_rej_frames",
	    CTLFLAG_RD, &sc->rx_enq_rej_frames,
	    "Enqueue rejected by QMan");
	SYSCTL_ADD_UQUAD(ctx, parent, OID_AUTO, "rx_ieoi_err_frames",
	    CTLFLAG_RD, &sc->rx_ieoi_err_frames,
	    "QMan IEOI error");
	SYSCTL_ADD_UQUAD(ctx, parent, OID_AUTO, "tx_single_buf_frames",
	    CTLFLAG_RD, &sc->tx_single_buf_frames,
	    "Tx single buffer frames");
	SYSCTL_ADD_UQUAD(ctx, parent, OID_AUTO, "tx_sg_frames",
	    CTLFLAG_RD, &sc->tx_sg_frames,
	    "Tx S/G frames");

	SYSCTL_ADD_PROC(ctx, parent, OID_AUTO, "buf_num",
	    CTLTYPE_U32 | CTLFLAG_RD, sc, 0, dpaa2_ni_collect_buf_num,
	    "IU", "number of Rx buffers in the buffer pool");
	SYSCTL_ADD_PROC(ctx, parent, OID_AUTO, "buf_free",
	    CTLTYPE_U32 | CTLFLAG_RD, sc, 0, dpaa2_ni_collect_buf_free,
	    "IU", "number of free Rx buffers in the buffer pool");

 	parent = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev));

	/* Add channels statistics. */
	node = SYSCTL_ADD_NODE(ctx, parent, OID_AUTO, "channels",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "DPNI Channels");
	parent = SYSCTL_CHILDREN(node);
	for (int i = 0; i < sc->chan_n; i++) {
		snprintf(cbuf, sizeof(cbuf), "%d", i);

		node2 = SYSCTL_ADD_NODE(ctx, parent, OID_AUTO, cbuf,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "DPNI Channel");
		parent2 = SYSCTL_CHILDREN(node2);

		SYSCTL_ADD_UQUAD(ctx, parent2, OID_AUTO, "tx_frames",
		    CTLFLAG_RD, &sc->channels[i]->tx_frames,
		    "Tx frames counter");
		SYSCTL_ADD_UQUAD(ctx, parent2, OID_AUTO, "tx_dropped",
		    CTLFLAG_RD, &sc->channels[i]->tx_dropped,
		    "Tx dropped counter");
	}

	return (0);
}

static int
dpaa2_ni_setup_dma(struct dpaa2_ni_softc *sc)
{
	device_t dev = sc->dev;
	int error;

	KASSERT((sc->buf_align == BUF_ALIGN) || (sc->buf_align == BUF_ALIGN_V1),
	    ("unexpected buffer alignment: %d\n", sc->buf_align));

	/* DMA tag for Rx distribution key. */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),
	    PAGE_SIZE, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* low restricted addr */
	    BUS_SPACE_MAXADDR,		/* high restricted addr */
	    NULL, NULL,			/* filter, filterarg */
	    DPAA2_CLASSIFIER_DMA_SIZE, 1, /* maxsize, nsegments */
	    DPAA2_CLASSIFIER_DMA_SIZE, 0, /* maxsegsize, flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->rxd_dmat);
	if (error) {
		device_printf(dev, "%s: failed to create DMA tag for Rx "
		    "distribution key\n", __func__);
		return (error);
	}

	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),
	    PAGE_SIZE, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* low restricted addr */
	    BUS_SPACE_MAXADDR,		/* high restricted addr */
	    NULL, NULL,			/* filter, filterarg */
	    ETH_QOS_KCFG_BUF_SIZE, 1,	/* maxsize, nsegments */
	    ETH_QOS_KCFG_BUF_SIZE, 0,	/* maxsegsize, flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->qos_dmat);
	if (error) {
		device_printf(dev, "%s: failed to create DMA tag for QoS key\n",
		    __func__);
		return (error);
	}

	return (0);
}

/**
 * @brief Configure buffer layouts of the different DPNI queues.
 */
static int
dpaa2_ni_set_buf_layout(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_ni_buf_layout buf_layout = {0};
	struct dpaa2_cmd cmd;
	uint16_t rc_token, ni_token;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(sc->dev, "%s: failed to open DPMAC: id=%d, "
		    "error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	/*
	 * Select Rx/Tx buffer alignment. It's necessary to ensure that the
	 * buffer size seen by WRIOP is a multiple of 64 or 256 bytes depending
	 * on the WRIOP version.
	 */
	sc->buf_align = (sc->attr.wriop_ver == WRIOP_VERSION(0, 0, 0) ||
	    sc->attr.wriop_ver == WRIOP_VERSION(1, 0, 0))
	    ? BUF_ALIGN_V1 : BUF_ALIGN;

	/*
	 * We need to ensure that the buffer size seen by WRIOP is a multiple
	 * of 64 or 256 bytes depending on the WRIOP version.
	 */
	sc->buf_sz = ALIGN_DOWN(DPAA2_RX_BUF_SIZE, sc->buf_align);

	if (bootverbose) {
		device_printf(dev, "Rx/Tx buffers: size=%d, alignment=%d\n",
		    sc->buf_sz, sc->buf_align);
	}

	/*
	 *    Frame Descriptor       Tx buffer layout
	 *
	 *                ADDR -> |---------------------|
	 *                        | SW FRAME ANNOTATION | BUF_SWA_SIZE bytes
	 *                        |---------------------|
	 *                        | HW FRAME ANNOTATION | BUF_TX_HWA_SIZE bytes
	 *                        |---------------------|
	 *                        |    DATA HEADROOM    |
	 *       ADDR + OFFSET -> |---------------------|
	 *                        |                     |
	 *                        |                     |
	 *                        |     FRAME DATA      |
	 *                        |                     |
	 *                        |                     |
	 *                        |---------------------|
	 *                        |    DATA TAILROOM    |
	 *                        |---------------------|
	 *
	 * NOTE: It's for a single buffer frame only.
	 */
	buf_layout.queue_type = DPAA2_NI_QUEUE_TX;
	buf_layout.pd_size = BUF_SWA_SIZE;
	buf_layout.pass_timestamp = true;
	buf_layout.pass_frame_status = true;
	buf_layout.options =
	    BUF_LOPT_PRIV_DATA_SZ |
	    BUF_LOPT_TIMESTAMP | /* requires 128 bytes in HWA */
	    BUF_LOPT_FRAME_STATUS;
	error = DPAA2_CMD_NI_SET_BUF_LAYOUT(dev, child, &cmd, &buf_layout);
	if (error) {
		device_printf(dev, "%s: failed to set Tx buffer layout\n",
		    __func__);
		goto close_ni;
	}

	/* Tx-confirmation buffer layout */
	buf_layout.queue_type = DPAA2_NI_QUEUE_TX_CONF;
	buf_layout.options =
	    BUF_LOPT_TIMESTAMP |
	    BUF_LOPT_FRAME_STATUS;
	error = DPAA2_CMD_NI_SET_BUF_LAYOUT(dev, child, &cmd, &buf_layout);
	if (error) {
		device_printf(dev, "%s: failed to set TxConf buffer layout\n",
		    __func__);
		goto close_ni;
	}

	/*
	 * Driver should reserve the amount of space indicated by this command
	 * as headroom in all Tx frames.
	 */
	error = DPAA2_CMD_NI_GET_TX_DATA_OFF(dev, child, &cmd, &sc->tx_data_off);
	if (error) {
		device_printf(dev, "%s: failed to obtain Tx data offset\n",
		    __func__);
		goto close_ni;
	}

	if (bootverbose) {
		device_printf(dev, "Tx data offset=%d\n", sc->tx_data_off);
	}
	if ((sc->tx_data_off % 64) != 0) {
		device_printf(dev, "Tx data offset (%d) is not a multiplication "
		    "of 64 bytes\n", sc->tx_data_off);
	}

	/*
	 *    Frame Descriptor       Rx buffer layout
	 *
	 *                ADDR -> |---------------------|
	 *                        | SW FRAME ANNOTATION | BUF_SWA_SIZE bytes
	 *                        |---------------------|
	 *                        | HW FRAME ANNOTATION | BUF_RX_HWA_SIZE bytes
	 *                        |---------------------|
	 *                        |    DATA HEADROOM    | OFFSET-BUF_RX_HWA_SIZE
	 *       ADDR + OFFSET -> |---------------------|
	 *                        |                     |
	 *                        |                     |
	 *                        |     FRAME DATA      |
	 *                        |                     |
	 *                        |                     |
	 *                        |---------------------|
	 *                        |    DATA TAILROOM    | 0 bytes
	 *                        |---------------------|
	 *
	 * NOTE: It's for a single buffer frame only.
	 */
	buf_layout.queue_type = DPAA2_NI_QUEUE_RX;
	buf_layout.pd_size = BUF_SWA_SIZE;
	buf_layout.fd_align = sc->buf_align;
	buf_layout.head_size = sc->tx_data_off - BUF_RX_HWA_SIZE - BUF_SWA_SIZE;
	buf_layout.tail_size = 0;
	buf_layout.pass_frame_status = true;
	buf_layout.pass_parser_result = true;
	buf_layout.pass_timestamp = true;
	buf_layout.options =
	    BUF_LOPT_PRIV_DATA_SZ |
	    BUF_LOPT_DATA_ALIGN |
	    BUF_LOPT_DATA_HEAD_ROOM |
	    BUF_LOPT_DATA_TAIL_ROOM |
	    BUF_LOPT_FRAME_STATUS |
	    BUF_LOPT_PARSER_RESULT |
	    BUF_LOPT_TIMESTAMP;
	error = DPAA2_CMD_NI_SET_BUF_LAYOUT(dev, child, &cmd, &buf_layout);
	if (error) {
		device_printf(dev, "%s: failed to set Rx buffer layout\n",
		    __func__);
		goto close_ni;
	}

	error = 0;
close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

/**
 * @brief Enable Rx/Tx pause frames.
 *
 * NOTE: DPNI stops sending when a pause frame is received (Rx frame) or DPNI
 *       itself generates pause frames (Tx frame).
 */
static int
dpaa2_ni_set_pause_frame(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_ni_link_cfg link_cfg = {0};
	struct dpaa2_cmd cmd;
	uint16_t rc_token, ni_token;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(sc->dev, "%s: failed to open DPMAC: id=%d, "
		    "error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	error = DPAA2_CMD_NI_GET_LINK_CFG(dev, child, &cmd, &link_cfg);
	if (error) {
		device_printf(dev, "%s: failed to obtain link configuration: "
		    "error=%d\n", __func__, error);
		goto close_ni;
	}

	/* Enable both Rx and Tx pause frames by default. */
	link_cfg.options |= DPAA2_NI_LINK_OPT_PAUSE;
	link_cfg.options &= ~DPAA2_NI_LINK_OPT_ASYM_PAUSE;

	error = DPAA2_CMD_NI_SET_LINK_CFG(dev, child, &cmd, &link_cfg);
	if (error) {
		device_printf(dev, "%s: failed to set link configuration: "
		    "error=%d\n", __func__, error);
		goto close_ni;
	}

	sc->link_options = link_cfg.options;
	error = 0;
close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

/**
 * @brief Configure QoS table to determine the traffic class for the received
 * frame.
 */
static int
dpaa2_ni_set_qos_table(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_ni_qos_table tbl;
	struct dpaa2_buf *buf = &sc->qos_kcfg;
	struct dpaa2_cmd cmd;
	uint16_t rc_token, ni_token;
	int error;

	if (sc->attr.num.rx_tcs == 1 ||
	    !(sc->attr.options & DPNI_OPT_HAS_KEY_MASKING)) {
		if (bootverbose) {
			device_printf(dev, "Ingress traffic classification is "
			    "not supported\n");
		}
		return (0);
	}

	/*
	 * Allocate a buffer visible to the device to hold the QoS table key
	 * configuration.
	 */

	if (__predict_true(buf->dmat == NULL)) {
		buf->dmat = sc->qos_dmat;
	}

	error = bus_dmamem_alloc(buf->dmat, (void **)&buf->vaddr,
	    BUS_DMA_ZERO | BUS_DMA_COHERENT, &buf->dmap);
	if (error) {
		device_printf(dev, "%s: failed to allocate a buffer for QoS key "
		    "configuration\n", __func__);
		goto err_exit;
	}

	error = bus_dmamap_load(buf->dmat, buf->dmap, buf->vaddr,
	    ETH_QOS_KCFG_BUF_SIZE, dpaa2_dmamap_oneseg_cb, &buf->paddr,
	    BUS_DMA_NOWAIT);
	if (error) {
		device_printf(dev, "%s: failed to map QoS key configuration "
		    "buffer into bus space\n", __func__);
		goto err_exit;
	}

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(sc->dev, "%s: failed to open DPMAC: id=%d, "
		    "error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	tbl.default_tc = 0;
	tbl.discard_on_miss = false;
	tbl.keep_entries = false;
	tbl.kcfg_busaddr = buf->paddr;
	error = DPAA2_CMD_NI_SET_QOS_TABLE(dev, child, &cmd, &tbl);
	if (error) {
		device_printf(dev, "%s: failed to set QoS table\n", __func__);
		goto close_ni;
	}

	error = DPAA2_CMD_NI_CLEAR_QOS_TABLE(dev, child, &cmd);
	if (error) {
		device_printf(dev, "%s: failed to clear QoS table\n", __func__);
		goto close_ni;
	}

	error = 0;
close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

static int
dpaa2_ni_set_mac_addr(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	if_t ifp = sc->ifp;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_cmd cmd;
	struct ether_addr rnd_mac_addr;
	uint16_t rc_token, ni_token;
	uint8_t mac_addr[ETHER_ADDR_LEN];
	uint8_t dpni_mac_addr[ETHER_ADDR_LEN];
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(sc->dev, "%s: failed to open DPMAC: id=%d, "
		    "error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	/*
	 * Get the MAC address associated with the physical port, if the DPNI is
	 * connected to a DPMAC directly associated with one of the physical
	 * ports.
	 */
	error = DPAA2_CMD_NI_GET_PORT_MAC_ADDR(dev, child, &cmd, mac_addr);
	if (error) {
		device_printf(dev, "%s: failed to obtain the MAC address "
		    "associated with the physical port\n", __func__);
		goto close_ni;
	}

	/* Get primary MAC address from the DPNI attributes. */
	error = DPAA2_CMD_NI_GET_PRIM_MAC_ADDR(dev, child, &cmd, dpni_mac_addr);
	if (error) {
		device_printf(dev, "%s: failed to obtain primary MAC address\n",
		    __func__);
		goto close_ni;
	}

	if (!ETHER_IS_ZERO(mac_addr)) {
		/* Set MAC address of the physical port as DPNI's primary one. */
		error = DPAA2_CMD_NI_SET_PRIM_MAC_ADDR(dev, child, &cmd,
		    mac_addr);
		if (error) {
			device_printf(dev, "%s: failed to set primary MAC "
			    "address\n", __func__);
			goto close_ni;
		}
		for (int i = 0; i < ETHER_ADDR_LEN; i++) {
			sc->mac.addr[i] = mac_addr[i];
		}
	} else if (ETHER_IS_ZERO(dpni_mac_addr)) {
		/* Generate random MAC address as DPNI's primary one. */
		ether_gen_addr(ifp, &rnd_mac_addr);
		for (int i = 0; i < ETHER_ADDR_LEN; i++) {
			mac_addr[i] = rnd_mac_addr.octet[i];
		}

		error = DPAA2_CMD_NI_SET_PRIM_MAC_ADDR(dev, child, &cmd,
		    mac_addr);
		if (error) {
			device_printf(dev, "%s: failed to set random primary "
			    "MAC address\n", __func__);
			goto close_ni;
		}
		for (int i = 0; i < ETHER_ADDR_LEN; i++) {
			sc->mac.addr[i] = mac_addr[i];
		}
	} else {
		for (int i = 0; i < ETHER_ADDR_LEN; i++) {
			sc->mac.addr[i] = dpni_mac_addr[i];
		}
	}

	error = 0;
close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

static void
dpaa2_ni_miibus_statchg(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_mac_link_state mac_link = { 0 };
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_cmd cmd;
	uint16_t rc_token, mac_token;
	int error, link_state;

	if (sc->fixed_link || sc->mii == NULL) {
		return;
	}
	if ((if_getdrvflags(sc->ifp) & IFF_DRV_RUNNING) == 0) {
		/*
		 * We will receive calls and adjust the changes but
		 * not have setup everything (called before dpaa2_ni_init()
		 * really).  This will then setup the link and internal
		 * sc->link_state and not trigger the update once needed,
		 * so basically dpmac never knows about it.
		 */
		return;
	}

	/*
	 * Note: ifp link state will only be changed AFTER we are called so we
	 * cannot rely on ifp->if_linkstate here.
	 */
	if (sc->mii->mii_media_status & IFM_AVALID) {
		if (sc->mii->mii_media_status & IFM_ACTIVE) {
			link_state = LINK_STATE_UP;
		} else {
			link_state = LINK_STATE_DOWN;
		}
	} else {
		link_state = LINK_STATE_UNKNOWN;
	}

	if (link_state != sc->link_state) {
		sc->link_state = link_state;

		DPAA2_CMD_INIT(&cmd);

		error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id,
		    &rc_token);
		if (error) {
			device_printf(dev, "%s: failed to open resource "
			    "container: id=%d, error=%d\n", __func__, rcinfo->id,
			    error);
			goto err_exit;
		}
		error = DPAA2_CMD_MAC_OPEN(dev, child, &cmd, sc->mac.dpmac_id,
		    &mac_token);
		if (error) {
			device_printf(sc->dev, "%s: failed to open DPMAC: "
			    "id=%d, error=%d\n", __func__, sc->mac.dpmac_id,
			    error);
			goto close_rc;
		}

		if (link_state == LINK_STATE_UP ||
		    link_state == LINK_STATE_DOWN) {
			/* Update DPMAC link state. */
			mac_link.supported = sc->mii->mii_media.ifm_media;
			mac_link.advert = sc->mii->mii_media.ifm_media;
			mac_link.rate = 1000; /* TODO: Where to get from? */	/* ifmedia_baudrate? */
			mac_link.options =
			    DPAA2_MAC_LINK_OPT_AUTONEG |
			    DPAA2_MAC_LINK_OPT_PAUSE;
			mac_link.up = (link_state == LINK_STATE_UP) ? true : false;
			mac_link.state_valid = true;

			/* Inform DPMAC about link state. */
			error = DPAA2_CMD_MAC_SET_LINK_STATE(dev, child, &cmd,
			    &mac_link);
			if (error) {
				device_printf(sc->dev, "%s: failed to set DPMAC "
				    "link state: id=%d, error=%d\n", __func__,
				    sc->mac.dpmac_id, error);
			}
		}
		(void)DPAA2_CMD_MAC_CLOSE(dev, child, &cmd);
		(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd,
		    rc_token));
	}

	return;

close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return;
}

/**
 * @brief Callback function to process media change request.
 */
static int
dpaa2_ni_media_change_locked(struct dpaa2_ni_softc *sc)
{

	DPNI_LOCK_ASSERT(sc);
	if (sc->mii) {
		mii_mediachg(sc->mii);
		sc->media_status = sc->mii->mii_media.ifm_media;
	} else if (sc->fixed_link) {
		if_printf(sc->ifp, "%s: can't change media in fixed mode\n",
		    __func__);
	}

	return (0);
}

static int
dpaa2_ni_media_change(if_t ifp)
{
	struct dpaa2_ni_softc *sc = if_getsoftc(ifp);
	int error;

	DPNI_LOCK(sc);
	error = dpaa2_ni_media_change_locked(sc);
	DPNI_UNLOCK(sc);
	return (error);
}

/**
 * @brief Callback function to process media status request.
 */
static void
dpaa2_ni_media_status(if_t ifp, struct ifmediareq *ifmr)
{
	struct dpaa2_ni_softc *sc = if_getsoftc(ifp);

	DPNI_LOCK(sc);
	if (sc->mii) {
		mii_pollstat(sc->mii);
		ifmr->ifm_active = sc->mii->mii_media_active;
		ifmr->ifm_status = sc->mii->mii_media_status;
	}
	DPNI_UNLOCK(sc);
}

/**
 * @brief Callout function to check and update media status.
 */
static void
dpaa2_ni_media_tick(void *arg)
{
	struct dpaa2_ni_softc *sc = (struct dpaa2_ni_softc *) arg;

	/* Check for media type change */
	if (sc->mii) {
		mii_tick(sc->mii);
		if (sc->media_status != sc->mii->mii_media.ifm_media) {
			printf("%s: media type changed (ifm_media=%x)\n",
			    __func__, sc->mii->mii_media.ifm_media);
			dpaa2_ni_media_change(sc->ifp);
		}
	}

	/* Schedule another timeout one second from now */
	callout_reset(&sc->mii_callout, hz, dpaa2_ni_media_tick, sc);
}

static void
dpaa2_ni_init(void *arg)
{
	struct dpaa2_ni_softc *sc = (struct dpaa2_ni_softc *) arg;
	if_t ifp = sc->ifp;
	device_t pdev = device_get_parent(sc->dev);
	device_t dev = sc->dev;
	device_t child = dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_cmd cmd;
	uint16_t rc_token, ni_token;
	int error;

	DPNI_LOCK(sc);
	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0) {
		DPNI_UNLOCK(sc);
		return;
	}
	DPNI_UNLOCK(sc);

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	error = DPAA2_CMD_NI_ENABLE(dev, child, &cmd);
	if (error) {
		device_printf(dev, "%s: failed to enable DPNI: error=%d\n",
		    __func__, error);
	}

	DPNI_LOCK(sc);
	/* Announce we are up and running and can queue packets. */
	if_setdrvflagbits(ifp, IFF_DRV_RUNNING, IFF_DRV_OACTIVE);

	if (sc->mii) {
		/*
		 * mii_mediachg() will trigger a call into
		 * dpaa2_ni_miibus_statchg() to setup link state.
		 */
		dpaa2_ni_media_change_locked(sc);
	}
	callout_reset(&sc->mii_callout, hz, dpaa2_ni_media_tick, sc);

	DPNI_UNLOCK(sc);

	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
	return;

close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return;
}

static int
dpaa2_ni_transmit(if_t ifp, struct mbuf *m)
{
	struct dpaa2_ni_softc *sc = if_getsoftc(ifp);
	struct dpaa2_channel *ch;
	uint32_t fqid;
	bool found = false;
	int chidx = 0, error;

	if (__predict_false(!(if_getdrvflags(ifp) & IFF_DRV_RUNNING))) {
		return (0);
	}

	if (M_HASHTYPE_GET(m) != M_HASHTYPE_NONE) {
		fqid = m->m_pkthdr.flowid;
		for (int i = 0; i < sc->chan_n; i++) {
			ch = sc->channels[i];
			for (int j = 0; j < ch->rxq_n; j++) {
				if (fqid == ch->rx_queues[j].fqid) {
					chidx = ch->flowid;
					found = true;
					break;
				}
			}
			if (found) {
				break;
			}
		}
	}

	ch = sc->channels[chidx];
	error = buf_ring_enqueue(ch->xmit_br, m);
	if (__predict_false(error != 0)) {
		m_freem(m);
	} else {
		taskqueue_enqueue(ch->cleanup_tq, &ch->cleanup_task);
	}

	return (error);
}

static void
dpaa2_ni_qflush(if_t ifp)
{
	/* TODO: Find a way to drain Tx queues in QBMan. */
	if_qflush(ifp);
}

static int
dpaa2_ni_ioctl(if_t ifp, u_long c, caddr_t data)
{
	struct dpaa2_ni_softc *sc = if_getsoftc(ifp);
	struct ifreq *ifr = (struct ifreq *) data;
	device_t pdev = device_get_parent(sc->dev);
	device_t dev = sc->dev;
	device_t child = dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_cmd cmd;
	uint32_t changed = 0;
	uint16_t rc_token, ni_token;
	int mtu, error, rc = 0;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	switch (c) {
	case SIOCSIFMTU:
		DPNI_LOCK(sc);
		mtu = ifr->ifr_mtu;
		if (mtu < ETHERMIN || mtu > ETHERMTU_JUMBO) {
			DPNI_UNLOCK(sc);
			error = EINVAL;
			goto close_ni;
		}
		if_setmtu(ifp, mtu);
		DPNI_UNLOCK(sc);

		/* Update maximum frame length. */
		mtu += ETHER_HDR_LEN;
		if (if_getcapenable(ifp) & IFCAP_VLAN_MTU)
			mtu += ETHER_VLAN_ENCAP_LEN;
		error = DPAA2_CMD_NI_SET_MFL(dev, child, &cmd, mtu);
		if (error) {
			device_printf(dev, "%s: failed to update maximum frame "
			    "length: error=%d\n", __func__, error);
			goto close_ni;
		}
		break;
	case SIOCSIFCAP:
		changed = if_getcapenable(ifp) ^ ifr->ifr_reqcap;
		if (changed & IFCAP_HWCSUM) {
			if ((ifr->ifr_reqcap & changed) & IFCAP_HWCSUM) {
				if_setcapenablebit(ifp, IFCAP_HWCSUM, 0);
			} else {
				if_setcapenablebit(ifp, 0, IFCAP_HWCSUM);
			}
		}
		rc = dpaa2_ni_setup_if_caps(sc);
		if (rc) {
			printf("%s: failed to update iface capabilities: "
			    "error=%d\n", __func__, rc);
			rc = ENXIO;
		}
		break;
	case SIOCSIFFLAGS:
		DPNI_LOCK(sc);
		if (if_getflags(ifp) & IFF_UP) {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
				changed = if_getflags(ifp) ^ sc->if_flags;
				if (changed & IFF_PROMISC ||
				    changed & IFF_ALLMULTI) {
					rc = dpaa2_ni_setup_if_flags(sc);
				}
			} else {
				DPNI_UNLOCK(sc);
				dpaa2_ni_init(sc);
				DPNI_LOCK(sc);
			}
		} else if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			/* FIXME: Disable DPNI. See dpaa2_ni_init(). */
		}

		sc->if_flags = if_getflags(ifp);
		DPNI_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		DPNI_LOCK(sc);
		if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			DPNI_UNLOCK(sc);
			rc = dpaa2_ni_update_mac_filters(ifp);
			if (rc) {
				device_printf(dev, "%s: failed to update MAC "
				    "filters: error=%d\n", __func__, rc);
			}
			DPNI_LOCK(sc);
		}
		DPNI_UNLOCK(sc);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		if (sc->mii)
			rc = ifmedia_ioctl(ifp, ifr, &sc->mii->mii_media, c);
		else if(sc->fixed_link) {
			rc = ifmedia_ioctl(ifp, ifr, &sc->fixed_ifmedia, c);
		}
		break;
	default:
		rc = ether_ioctl(ifp, c, data);
		break;
	}

	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
	return (rc);

close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

static int
dpaa2_ni_update_mac_filters(if_t ifp)
{
	struct dpaa2_ni_softc *sc = if_getsoftc(ifp);
	struct dpaa2_ni_mcaddr_ctx ctx;
	device_t pdev = device_get_parent(sc->dev);
	device_t dev = sc->dev;
	device_t child = dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_cmd cmd;
	uint16_t rc_token, ni_token;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	/* Remove all multicast MAC filters. */
	error = DPAA2_CMD_NI_CLEAR_MAC_FILTERS(dev, child, &cmd, false, true);
	if (error) {
		device_printf(dev, "%s: failed to clear multicast MAC filters: "
		    "error=%d\n", __func__, error);
		goto close_ni;
	}

	ctx.ifp = ifp;
	ctx.error = 0;
	ctx.nent = 0;

	if_foreach_llmaddr(ifp, dpaa2_ni_add_maddr, &ctx);

	error = ctx.error;
close_ni:
	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (error);
}

static u_int
dpaa2_ni_add_maddr(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	struct dpaa2_ni_mcaddr_ctx *ctx = arg;
	struct dpaa2_ni_softc *sc = if_getsoftc(ctx->ifp);
	device_t pdev = device_get_parent(sc->dev);
	device_t dev = sc->dev;
	device_t child = dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_cmd cmd;
	uint16_t rc_token, ni_token;
	int error;

	if (ctx->error != 0) {
		return (0);
	}

	if (ETHER_IS_MULTICAST(LLADDR(sdl))) {
		DPAA2_CMD_INIT(&cmd);

		error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id,
		    &rc_token);
		if (error) {
			device_printf(dev, "%s: failed to open resource "
			    "container: id=%d, error=%d\n", __func__, rcinfo->id,
			    error);
			return (0);
		}
		error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id,
		    &ni_token);
		if (error) {
			device_printf(dev, "%s: failed to open network interface: "
			    "id=%d, error=%d\n", __func__, dinfo->id, error);
			(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd,
			    rc_token));
			return (0);
		}

		ctx->error = DPAA2_CMD_NI_ADD_MAC_ADDR(dev, child, &cmd,
		    LLADDR(sdl));

		(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd,
		    ni_token));
		(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd,
		    rc_token));

		if (ctx->error != 0) {
			device_printf(dev, "%s: can't add more then %d MAC "
			    "addresses, switching to the multicast promiscuous "
			    "mode\n", __func__, ctx->nent);

			/* Enable multicast promiscuous mode. */
			DPNI_LOCK(sc);
			if_setflagbits(ctx->ifp, IFF_ALLMULTI, 0);
			sc->if_flags |= IFF_ALLMULTI;
			ctx->error = dpaa2_ni_setup_if_flags(sc);
			DPNI_UNLOCK(sc);

			return (0);
		}
		ctx->nent++;
	}

	return (1);
}

static void
dpaa2_ni_intr(void *arg)
{
	struct dpaa2_ni_softc *sc = (struct dpaa2_ni_softc *) arg;
	device_t pdev = device_get_parent(sc->dev);
	device_t dev = sc->dev;
	device_t child = dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_cmd cmd;
	uint32_t status = ~0u; /* clear all IRQ status bits */
	uint16_t rc_token, ni_token;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	error = DPAA2_CMD_NI_GET_IRQ_STATUS(dev, child, &cmd, DPNI_IRQ_INDEX,
	    &status);
	if (error) {
		device_printf(sc->dev, "%s: failed to obtain IRQ status: "
		    "error=%d\n", __func__, error);
	}

	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return;
}

/**
 * @brief Execute channel's Rx/Tx routines.
 *
 * NOTE: Should not be re-entrant for the same channel. It is achieved by
 *       enqueuing the cleanup routine on a single-threaded taskqueue.
 */
static void
dpaa2_ni_cleanup_task(void *arg, int count)
{
	struct dpaa2_channel *ch = (struct dpaa2_channel *)arg;
	struct dpaa2_ni_softc *sc = device_get_softc(ch->ni_dev);
	int error, rxc, txc;

	for (int i = 0; i < DPAA2_CLEAN_BUDGET; i++) {
		rxc  = dpaa2_ni_rx_cleanup(ch);
		txc  = dpaa2_ni_tx_cleanup(ch);

		if (__predict_false((if_getdrvflags(sc->ifp) &
		    IFF_DRV_RUNNING) == 0)) {
			return;
		}

		if ((txc != DPAA2_TX_BUDGET) && (rxc != DPAA2_RX_BUDGET)) {
			break;
		}
	}

	/* Re-arm channel to generate CDAN */
	error = DPAA2_SWP_CONF_WQ_CHANNEL(ch->io_dev, &ch->ctx);
	if (error != 0) {
		panic("%s: failed to rearm channel: chan_id=%d, error=%d\n",
		    __func__, ch->id, error);
	}
}

/**
 * @brief Poll frames from a specific channel when CDAN is received.
 */
static int
dpaa2_ni_rx_cleanup(struct dpaa2_channel *ch)
{
	struct dpaa2_io_softc *iosc = device_get_softc(ch->io_dev);
	struct dpaa2_swp *swp = iosc->swp;
	struct dpaa2_ni_fq *fq;
	struct dpaa2_buf *buf = &ch->store;
	int budget = DPAA2_RX_BUDGET;
	int error, consumed = 0;

	do {
		error = dpaa2_swp_pull(swp, ch->id, buf, DPAA2_ETH_STORE_FRAMES);
		if (error) {
			device_printf(ch->ni_dev, "%s: failed to pull frames: "
			    "chan_id=%d, error=%d\n", __func__, ch->id, error);
			break;
		}
		error = dpaa2_ni_consume_frames(ch, &fq, &consumed);
		if (error == ENOENT || error == EALREADY) {
			break;
		}
		if (error == ETIMEDOUT) {
			device_printf(ch->ni_dev, "%s: timeout to consume "
			    "frames: chan_id=%d\n", __func__, ch->id);
		}
	} while (--budget);

	return (DPAA2_RX_BUDGET - budget);
}

static int
dpaa2_ni_tx_cleanup(struct dpaa2_channel *ch)
{
	struct dpaa2_ni_softc *sc = device_get_softc(ch->ni_dev);
	struct dpaa2_ni_tx_ring *tx = &ch->txc_queue.tx_rings[0];
	struct mbuf *m = NULL;
	int budget = DPAA2_TX_BUDGET;

	do {
		mtx_assert(&ch->xmit_mtx, MA_NOTOWNED);
		mtx_lock(&ch->xmit_mtx);
		m = buf_ring_dequeue_sc(ch->xmit_br);
		mtx_unlock(&ch->xmit_mtx);

		if (__predict_false(m == NULL)) {
			/* TODO: Do not give up easily */
			break;
		} else {
			dpaa2_ni_tx(sc, ch, tx, m);
		}
	} while (--budget);

	return (DPAA2_TX_BUDGET - budget);
}

static void
dpaa2_ni_tx(struct dpaa2_ni_softc *sc, struct dpaa2_channel *ch,
    struct dpaa2_ni_tx_ring *tx, struct mbuf *m)
{
	device_t dev = sc->dev;
	struct dpaa2_ni_fq *fq = tx->fq;
	struct dpaa2_buf *buf, *sgt;
	struct dpaa2_fd fd;
	struct mbuf *md;
	bus_dma_segment_t segs[DPAA2_TX_SEGLIMIT];
	int rc, nsegs;
	int error;

	mtx_assert(&tx->lock, MA_NOTOWNED);
	mtx_lock(&tx->lock);
	buf = buf_ring_dequeue_sc(tx->br);
	mtx_unlock(&tx->lock);
	if (__predict_false(buf == NULL)) {
		/* TODO: Do not give up easily */
		m_freem(m);
		return;
	} else {
		DPAA2_BUF_ASSERT_TXREADY(buf);
		buf->m = m;
		sgt = buf->sgt;
	}

#if defined(INVARIANTS)
	struct dpaa2_ni_tx_ring *btx = (struct dpaa2_ni_tx_ring *)buf->opt;
	KASSERT(buf->opt == tx, ("%s: unexpected Tx ring", __func__));
	KASSERT(btx->fq->chan == ch, ("%s: unexpected channel", __func__));
#endif /* INVARIANTS */

	error = bus_dmamap_load_mbuf_sg(buf->dmat, buf->dmap, m, segs, &nsegs,
	    BUS_DMA_NOWAIT);
	if (__predict_false(error != 0)) {
		/* Too many fragments, trying to defragment... */
		md = m_collapse(m, M_NOWAIT, DPAA2_TX_SEGLIMIT);
		if (md == NULL) {
			device_printf(dev, "%s: m_collapse() failed\n", __func__);
			fq->chan->tx_dropped++;
			goto err;
		}

		buf->m = m = md;
		error = bus_dmamap_load_mbuf_sg(buf->dmat, buf->dmap, m, segs,
		    &nsegs, BUS_DMA_NOWAIT);
		if (__predict_false(error != 0)) {
			device_printf(dev, "%s: bus_dmamap_load_mbuf_sg() "
			    "failed: error=%d\n", __func__, error);
			fq->chan->tx_dropped++;
			goto err;
		}
	}

	error = dpaa2_ni_build_fd(sc, tx, buf, segs, nsegs, &fd);
	if (__predict_false(error != 0)) {
		device_printf(dev, "%s: failed to build frame descriptor: "
		    "error=%d\n", __func__, error);
		fq->chan->tx_dropped++;
		goto err_unload;
	}

	/* TODO: Enqueue several frames in a single command */
	for (int i = 0; i < DPAA2_NI_ENQUEUE_RETRIES; i++) {
		/* TODO: Return error codes instead of # of frames */
		rc = DPAA2_SWP_ENQ_MULTIPLE_FQ(fq->chan->io_dev, tx->fqid, &fd, 1);
		if (rc == 1) {
			break;
		}
	}

	bus_dmamap_sync(buf->dmat, buf->dmap, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sgt->dmat, sgt->dmap, BUS_DMASYNC_PREWRITE);

	if (rc != 1) {
		fq->chan->tx_dropped++;
		goto err_unload;
	} else {
		fq->chan->tx_frames++;
	}
	return;

err_unload:
	bus_dmamap_unload(buf->dmat, buf->dmap);
	if (sgt->paddr != 0) {
		bus_dmamap_unload(sgt->dmat, sgt->dmap);
	}
err:
	m_freem(buf->m);
	buf_ring_enqueue(tx->br, buf);
}

static int
dpaa2_ni_consume_frames(struct dpaa2_channel *chan, struct dpaa2_ni_fq **src,
    uint32_t *consumed)
{
	struct dpaa2_ni_fq *fq = NULL;
	struct dpaa2_dq *dq;
	struct dpaa2_fd *fd;
	struct dpaa2_ni_rx_ctx ctx = {
		.head = NULL,
		.tail = NULL,
		.cnt = 0,
		.last = false
	};
	int rc, frames = 0;

	do {
		rc = dpaa2_chan_next_frame(chan, &dq);
		if (rc == EINPROGRESS) {
			if (dq != NULL && !IS_NULL_RESPONSE(dq->fdr.desc.stat)) {
				fd = &dq->fdr.fd;
				fq = (struct dpaa2_ni_fq *) dq->fdr.desc.fqd_ctx;

				switch (fq->type) {
				case DPAA2_NI_QUEUE_RX:
					(void)dpaa2_ni_rx(chan, fq, fd, &ctx);
					break;
				case DPAA2_NI_QUEUE_RX_ERR:
					(void)dpaa2_ni_rx_err(chan, fq, fd);
					break;
				case DPAA2_NI_QUEUE_TX_CONF:
					(void)dpaa2_ni_tx_conf(chan, fq, fd);
					break;
				default:
					panic("%s: unknown queue type (1)",
					    __func__);
				}
				frames++;
			}
		} else if (rc == EALREADY || rc == ENOENT) {
			if (dq != NULL && !IS_NULL_RESPONSE(dq->fdr.desc.stat)) {
				fd = &dq->fdr.fd;
				fq = (struct dpaa2_ni_fq *) dq->fdr.desc.fqd_ctx;

				switch (fq->type) {
				case DPAA2_NI_QUEUE_RX:
					/*
					 * Last VDQ response (mbuf) in a chain
					 * obtained from the Rx queue.
					 */
					ctx.last = true;
					(void)dpaa2_ni_rx(chan, fq, fd, &ctx);
					break;
				case DPAA2_NI_QUEUE_RX_ERR:
					(void)dpaa2_ni_rx_err(chan, fq, fd);
					break;
				case DPAA2_NI_QUEUE_TX_CONF:
					(void)dpaa2_ni_tx_conf(chan, fq, fd);
					break;
				default:
					panic("%s: unknown queue type (2)",
					    __func__);
				}
				frames++;
			}
			break;
		} else {
			panic("%s: should not reach here: rc=%d", __func__, rc);
		}
	} while (true);

	KASSERT(chan->store_idx < chan->store_sz, ("%s: store_idx(%d) >= "
	    "store_sz(%d)", __func__, chan->store_idx, chan->store_sz));

	/*
	 * VDQ operation pulls frames from a single queue into the store.
	 * Return the frame queue and a number of consumed frames as an output.
	 */
	if (src != NULL) {
		*src = fq;
	}
	if (consumed != NULL) {
		*consumed = frames;
	}

	return (rc);
}

/**
 * @brief Receive frames.
 */
static int
dpaa2_ni_rx(struct dpaa2_channel *ch, struct dpaa2_ni_fq *fq, struct dpaa2_fd *fd,
    struct dpaa2_ni_rx_ctx *ctx)
{
	bus_addr_t paddr = (bus_addr_t)fd->addr;
	struct dpaa2_fa *fa = (struct dpaa2_fa *)PHYS_TO_DMAP(paddr);
	struct dpaa2_buf *buf = fa->buf;
	struct dpaa2_channel *bch = (struct dpaa2_channel *)buf->opt;
	struct dpaa2_ni_softc *sc = device_get_softc(bch->ni_dev);
	struct dpaa2_bp_softc *bpsc;
	struct mbuf *m;
	device_t bpdev;
	bus_addr_t released[DPAA2_SWP_BUFS_PER_CMD];
	void *buf_data;
	int buf_len, error, released_n = 0;

	KASSERT(fa->magic == DPAA2_MAGIC, ("%s: wrong magic", __func__));
	/*
	 * NOTE: Current channel might not be the same as the "buffer" channel
	 * and it's fine. It must not be NULL though.
	 */
	KASSERT(bch != NULL, ("%s: buffer channel is NULL", __func__));

	if (__predict_false(paddr != buf->paddr)) {
		panic("%s: unexpected physical address: fd(%#jx) != buf(%#jx)",
		    __func__, paddr, buf->paddr);
	}

	switch (dpaa2_ni_fd_err(fd)) {
	case 1: /* Enqueue rejected by QMan */
		sc->rx_enq_rej_frames++;
		break;
	case 2: /* QMan IEOI error */
		sc->rx_ieoi_err_frames++;
		break;
	default:
		break;
	}
	switch (dpaa2_ni_fd_format(fd)) {
	case DPAA2_FD_SINGLE:
		sc->rx_single_buf_frames++;
		break;
	case DPAA2_FD_SG:
		sc->rx_sg_buf_frames++;
		break;
	default:
		break;
	}

	mtx_assert(&bch->dma_mtx, MA_NOTOWNED);
	mtx_lock(&bch->dma_mtx);

	bus_dmamap_sync(buf->dmat, buf->dmap, BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(buf->dmat, buf->dmap);
	m = buf->m;
	buf_len = dpaa2_ni_fd_data_len(fd);
	buf_data = (uint8_t *)buf->vaddr + dpaa2_ni_fd_offset(fd);
	/* Prepare buffer to be re-cycled */
	buf->m = NULL;
	buf->paddr = 0;
	buf->vaddr = NULL;
	buf->seg.ds_addr = 0;
	buf->seg.ds_len = 0;
	buf->nseg = 0;

	mtx_unlock(&bch->dma_mtx);

	m->m_flags |= M_PKTHDR;
	m->m_data = buf_data;
	m->m_len = buf_len;
	m->m_pkthdr.len = buf_len;
	m->m_pkthdr.rcvif = sc->ifp;
	m->m_pkthdr.flowid = fq->fqid;
	M_HASHTYPE_SET(m, M_HASHTYPE_OPAQUE);

	if (ctx->head == NULL) {
		KASSERT(ctx->tail == NULL, ("%s: tail already given?", __func__));
		ctx->head = m;
		ctx->tail = m;
	} else {
		KASSERT(ctx->head != NULL, ("%s: head is NULL", __func__));
		ctx->tail->m_nextpkt = m;
		ctx->tail = m;
	}
	ctx->cnt++;

	if (ctx->last) {
		ctx->tail->m_nextpkt = NULL;
		if_input(sc->ifp, ctx->head);
	}

	/* Keep the buffer to be recycled */
	ch->recycled[ch->recycled_n++] = buf;

	/* Re-seed and release recycled buffers back to the pool */
	if (ch->recycled_n == DPAA2_SWP_BUFS_PER_CMD) {
		/* Release new buffers to the pool if needed */
		taskqueue_enqueue(sc->bp_taskq, &ch->bp_task);

		for (int i = 0; i < ch->recycled_n; i++) {
			buf = ch->recycled[i];
			bch = (struct dpaa2_channel *)buf->opt;

			mtx_assert(&bch->dma_mtx, MA_NOTOWNED);
			mtx_lock(&bch->dma_mtx);
			error = dpaa2_buf_seed_rxb(sc->dev, buf,
			    DPAA2_RX_BUF_SIZE, &bch->dma_mtx);
			mtx_unlock(&bch->dma_mtx);

			if (__predict_false(error != 0)) {
				/* TODO: What else to do with the buffer? */
				panic("%s: failed to recycle buffer: error=%d",
				    __func__, error);
			}

			/* Prepare buffer to be released in a single command */
			released[released_n++] = buf->paddr;
		}

		/* There's only one buffer pool for now */
		bpdev = (device_t)rman_get_start(sc->res[DPAA2_NI_BP_RID(0)]);
		bpsc = device_get_softc(bpdev);

		error = DPAA2_SWP_RELEASE_BUFS(ch->io_dev, bpsc->attr.bpid,
		    released, released_n);
		if (__predict_false(error != 0)) {
			device_printf(sc->dev, "%s: failed to release buffers "
			    "to the pool: error=%d\n", __func__, error);
			return (error);
		}
		ch->recycled_n = 0;
	}

	return (0);
}

/**
 * @brief Receive Rx error frames.
 */
static int
dpaa2_ni_rx_err(struct dpaa2_channel *ch, struct dpaa2_ni_fq *fq,
    struct dpaa2_fd *fd)
{
	bus_addr_t paddr = (bus_addr_t)fd->addr;
	struct dpaa2_fa *fa = (struct dpaa2_fa *)PHYS_TO_DMAP(paddr);
	struct dpaa2_buf *buf = fa->buf;
	struct dpaa2_channel *bch = (struct dpaa2_channel *)buf->opt;
	struct dpaa2_ni_softc *sc = device_get_softc(bch->ni_dev);
	device_t bpdev;
	struct dpaa2_bp_softc *bpsc;
	int error;

	KASSERT(fa->magic == DPAA2_MAGIC, ("%s: wrong magic", __func__));
	/*
	 * NOTE: Current channel might not be the same as the "buffer" channel
	 * and it's fine. It must not be NULL though.
	 */
	KASSERT(bch != NULL, ("%s: buffer channel is NULL", __func__));

	if (__predict_false(paddr != buf->paddr)) {
		panic("%s: unexpected physical address: fd(%#jx) != buf(%#jx)",
		    __func__, paddr, buf->paddr);
	}

	/* There's only one buffer pool for now */
	bpdev = (device_t)rman_get_start(sc->res[DPAA2_NI_BP_RID(0)]);
	bpsc = device_get_softc(bpdev);

	/* Release buffer to QBMan buffer pool */
	error = DPAA2_SWP_RELEASE_BUFS(ch->io_dev, bpsc->attr.bpid, &paddr, 1);
	if (error != 0) {
		device_printf(sc->dev, "%s: failed to release frame buffer to "
		    "the pool: error=%d\n", __func__, error);
		return (error);
	}

	return (0);
}

/**
 * @brief Receive Tx confirmation frames.
 */
static int
dpaa2_ni_tx_conf(struct dpaa2_channel *ch, struct dpaa2_ni_fq *fq,
    struct dpaa2_fd *fd)
{
	bus_addr_t paddr = (bus_addr_t)fd->addr;
	struct dpaa2_fa *fa = (struct dpaa2_fa *)PHYS_TO_DMAP(paddr);
	struct dpaa2_buf *buf = fa->buf;
	struct dpaa2_buf *sgt = buf->sgt;
	struct dpaa2_ni_tx_ring *tx = (struct dpaa2_ni_tx_ring *)buf->opt;
	struct dpaa2_channel *bch = tx->fq->chan;

	KASSERT(fa->magic == DPAA2_MAGIC, ("%s: wrong magic", __func__));
	KASSERT(tx != NULL, ("%s: Tx ring is NULL", __func__));
	KASSERT(sgt != NULL, ("%s: S/G table is NULL", __func__));
	/*
	 * NOTE: Current channel might not be the same as the "buffer" channel
	 * and it's fine. It must not be NULL though.
	 */
	KASSERT(bch != NULL, ("%s: buffer channel is NULL", __func__));

	if (paddr != buf->paddr) {
		panic("%s: unexpected physical address: fd(%#jx) != buf(%#jx)",
		    __func__, paddr, buf->paddr);
	}

	mtx_assert(&bch->dma_mtx, MA_NOTOWNED);
	mtx_lock(&bch->dma_mtx);

	bus_dmamap_sync(buf->dmat, buf->dmap, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(sgt->dmat, sgt->dmap, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(buf->dmat, buf->dmap);
	bus_dmamap_unload(sgt->dmat, sgt->dmap);
	m_freem(buf->m);
	buf->m = NULL;
	buf->paddr = 0;
	buf->vaddr = NULL;
	sgt->paddr = 0;

	mtx_unlock(&bch->dma_mtx);

	/* Return Tx buffer back to the ring */
	buf_ring_enqueue(tx->br, buf);

	return (0);
}

/**
 * @brief Compare versions of the DPAA2 network interface API.
 */
static int
dpaa2_ni_cmp_api_version(struct dpaa2_ni_softc *sc, uint16_t major,
    uint16_t minor)
{
	if (sc->api_major == major) {
		return sc->api_minor - minor;
	}
	return sc->api_major - major;
}

/**
 * @brief Build a DPAA2 frame descriptor.
 */
static int
dpaa2_ni_build_fd(struct dpaa2_ni_softc *sc, struct dpaa2_ni_tx_ring *tx,
    struct dpaa2_buf *buf, bus_dma_segment_t *segs, int nsegs, struct dpaa2_fd *fd)
{
	struct dpaa2_buf *sgt = buf->sgt;
	struct dpaa2_sg_entry *sge;
	struct dpaa2_fa *fa;
	int i, error;

	KASSERT(nsegs <= DPAA2_TX_SEGLIMIT, ("%s: too many segments", __func__));
	KASSERT(buf->opt != NULL, ("%s: no Tx ring?", __func__));
	KASSERT(sgt != NULL, ("%s: no S/G table?", __func__));
	KASSERT(sgt->vaddr != NULL, ("%s: no S/G vaddr?", __func__));

	memset(fd, 0, sizeof(*fd));

	/* Populate and map S/G table */
	if (__predict_true(nsegs <= DPAA2_TX_SEGLIMIT)) {
		sge = (struct dpaa2_sg_entry *)sgt->vaddr + sc->tx_data_off;
		for (i = 0; i < nsegs; i++) {
			sge[i].addr = (uint64_t)segs[i].ds_addr;
			sge[i].len = (uint32_t)segs[i].ds_len;
			sge[i].offset_fmt = 0u;
		}
		sge[i-1].offset_fmt |= 0x8000u; /* set final entry flag */

		KASSERT(sgt->paddr == 0, ("%s: paddr(%#jx) != 0", __func__,
		    sgt->paddr));

		error = bus_dmamap_load(sgt->dmat, sgt->dmap, sgt->vaddr,
		    DPAA2_TX_SGT_SZ, dpaa2_dmamap_oneseg_cb, &sgt->paddr,
		    BUS_DMA_NOWAIT);
		if (__predict_false(error != 0)) {
			device_printf(sc->dev, "%s: bus_dmamap_load() failed: "
			    "error=%d\n", __func__, error);
			return (error);
		}

		buf->paddr = sgt->paddr;
		buf->vaddr = sgt->vaddr;
		sc->tx_sg_frames++; /* for sysctl(9) */
	} else {
		return (EINVAL);
	}

	fa = (struct dpaa2_fa *)sgt->vaddr;
	fa->magic = DPAA2_MAGIC;
	fa->buf = buf;

	fd->addr = buf->paddr;
	fd->data_length = (uint32_t)buf->m->m_pkthdr.len;
	fd->bpid_ivp_bmt = 0;
	fd->offset_fmt_sl = 0x2000u | sc->tx_data_off;
	fd->ctrl = 0x00800000u;

	return (0);
}

static int
dpaa2_ni_fd_err(struct dpaa2_fd *fd)
{
	return ((fd->ctrl >> DPAA2_NI_FD_ERR_SHIFT) & DPAA2_NI_FD_ERR_MASK);
}

static uint32_t
dpaa2_ni_fd_data_len(struct dpaa2_fd *fd)
{
	if (dpaa2_ni_fd_short_len(fd)) {
		return (fd->data_length & DPAA2_NI_FD_LEN_MASK);
	}
	return (fd->data_length);
}

static int
dpaa2_ni_fd_format(struct dpaa2_fd *fd)
{
	return ((enum dpaa2_fd_format)((fd->offset_fmt_sl >>
	    DPAA2_NI_FD_FMT_SHIFT) & DPAA2_NI_FD_FMT_MASK));
}

static bool
dpaa2_ni_fd_short_len(struct dpaa2_fd *fd)
{
	return (((fd->offset_fmt_sl >> DPAA2_NI_FD_SL_SHIFT)
	    & DPAA2_NI_FD_SL_MASK) == 1);
}

static int
dpaa2_ni_fd_offset(struct dpaa2_fd *fd)
{
	return (fd->offset_fmt_sl & DPAA2_NI_FD_OFFSET_MASK);
}

/**
 * @brief Collect statistics of the network interface.
 */
static int
dpaa2_ni_collect_stats(SYSCTL_HANDLER_ARGS)
{
	struct dpaa2_ni_softc *sc = (struct dpaa2_ni_softc *) arg1;
	struct dpni_stat *stat = &dpni_stat_sysctls[oidp->oid_number];
	device_t pdev = device_get_parent(sc->dev);
	device_t dev = sc->dev;
	device_t child = dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_cmd cmd;
	uint64_t cnt[DPAA2_NI_STAT_COUNTERS];
	uint64_t result = 0;
	uint16_t rc_token, ni_token;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open resource container: "
		    "id=%d, error=%d\n", __func__, rcinfo->id, error);
		goto exit;
	}
	error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id, &ni_token);
	if (error) {
		device_printf(dev, "%s: failed to open network interface: "
		    "id=%d, error=%d\n", __func__, dinfo->id, error);
		goto close_rc;
	}

	error = DPAA2_CMD_NI_GET_STATISTICS(dev, child, &cmd, stat->page, 0, cnt);
	if (!error) {
		result = cnt[stat->cnt];
	}

	(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, ni_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
exit:
	return (sysctl_handle_64(oidp, &result, 0, req));
}

static int
dpaa2_ni_collect_buf_num(SYSCTL_HANDLER_ARGS)
{
	struct dpaa2_ni_softc *sc = (struct dpaa2_ni_softc *) arg1;
	uint32_t buf_num = DPAA2_ATOMIC_READ(&sc->buf_num);

	return (sysctl_handle_32(oidp, &buf_num, 0, req));
}

static int
dpaa2_ni_collect_buf_free(SYSCTL_HANDLER_ARGS)
{
	struct dpaa2_ni_softc *sc = (struct dpaa2_ni_softc *) arg1;
	uint32_t buf_free = DPAA2_ATOMIC_READ(&sc->buf_free);

	return (sysctl_handle_32(oidp, &buf_free, 0, req));
}

static int
dpaa2_ni_set_hash(device_t dev, uint64_t flags)
{
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	uint64_t key = 0;
	int i;

	if (!(sc->attr.num.queues > 1)) {
		return (EOPNOTSUPP);
	}

	for (i = 0; i < ARRAY_SIZE(dist_fields); i++) {
		if (dist_fields[i].rxnfc_field & flags) {
			key |= dist_fields[i].id;
		}
	}

	return (dpaa2_ni_set_dist_key(dev, DPAA2_NI_DIST_MODE_HASH, key));
}

/**
 * @brief Set Rx distribution (hash or flow classification) key flags is a
 * combination of RXH_ bits.
 */
static int
dpaa2_ni_set_dist_key(device_t dev, enum dpaa2_ni_dist_mode type, uint64_t flags)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpkg_profile_cfg cls_cfg;
	struct dpkg_extract *key;
	struct dpaa2_buf *buf = &sc->rxd_kcfg;
	struct dpaa2_cmd cmd;
	uint16_t rc_token, ni_token;
	int i, error = 0;

	if (__predict_true(buf->dmat == NULL)) {
		buf->dmat = sc->rxd_dmat;
	}

	memset(&cls_cfg, 0, sizeof(cls_cfg));

	/* Configure extracts according to the given flags. */
	for (i = 0; i < ARRAY_SIZE(dist_fields); i++) {
		key = &cls_cfg.extracts[cls_cfg.num_extracts];

		if (!(flags & dist_fields[i].id)) {
			continue;
		}

		if (cls_cfg.num_extracts >= DPKG_MAX_NUM_OF_EXTRACTS) {
			device_printf(dev, "%s: failed to add key extraction "
			    "rule\n", __func__);
			return (E2BIG);
		}

		key->type = DPKG_EXTRACT_FROM_HDR;
		key->extract.from_hdr.prot = dist_fields[i].cls_prot;
		key->extract.from_hdr.type = DPKG_FULL_FIELD;
		key->extract.from_hdr.field = dist_fields[i].cls_field;
		cls_cfg.num_extracts++;
	}

	error = bus_dmamem_alloc(buf->dmat, (void **)&buf->vaddr,
	    BUS_DMA_ZERO | BUS_DMA_COHERENT, &buf->dmap);
	if (error != 0) {
		device_printf(dev, "%s: failed to allocate a buffer for Rx "
		    "traffic distribution key configuration\n", __func__);
		return (error);
	}

	error = dpaa2_ni_prepare_key_cfg(&cls_cfg, (uint8_t *)buf->vaddr);
	if (error != 0) {
		device_printf(dev, "%s: failed to prepare key configuration: "
		    "error=%d\n", __func__, error);
		return (error);
	}

	/* Prepare for setting the Rx dist. */
	error = bus_dmamap_load(buf->dmat, buf->dmap, buf->vaddr,
	    DPAA2_CLASSIFIER_DMA_SIZE, dpaa2_dmamap_oneseg_cb, &buf->paddr,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->dev, "%s: failed to map a buffer for Rx "
		    "traffic distribution key configuration\n", __func__);
		return (error);
	}

	if (type == DPAA2_NI_DIST_MODE_HASH) {
		DPAA2_CMD_INIT(&cmd);

		error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id,
		    &rc_token);
		if (error) {
			device_printf(dev, "%s: failed to open resource "
			    "container: id=%d, error=%d\n", __func__, rcinfo->id,
			    error);
			goto err_exit;
		}
		error = DPAA2_CMD_NI_OPEN(dev, child, &cmd, dinfo->id,
		    &ni_token);
		if (error) {
			device_printf(dev, "%s: failed to open network "
			    "interface: id=%d, error=%d\n", __func__, dinfo->id,
			    error);
			goto close_rc;
		}

		error = DPAA2_CMD_NI_SET_RX_TC_DIST(dev, child, &cmd,
		    sc->attr.num.queues, 0, DPAA2_NI_DIST_MODE_HASH, buf->paddr);
		if (error != 0) {
			device_printf(dev, "%s: failed to set distribution mode "
			    "and size for the traffic class\n", __func__);
		}

		(void)DPAA2_CMD_NI_CLOSE(dev, child, DPAA2_CMD_TK(&cmd,
		    ni_token));
close_rc:
		(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd,
		    rc_token));
	}

err_exit:
	return (error);
}

/**
 * @brief Prepares extract parameters.
 *
 * cfg:		Defining a full Key Generation profile.
 * key_cfg_buf:	Zeroed 256 bytes of memory before mapping it to DMA.
 */
static int
dpaa2_ni_prepare_key_cfg(struct dpkg_profile_cfg *cfg, uint8_t *key_cfg_buf)
{
	struct dpni_ext_set_rx_tc_dist *dpni_ext;
	struct dpni_dist_extract *extr;
	int i, j;

	if (cfg->num_extracts > DPKG_MAX_NUM_OF_EXTRACTS)
		return (EINVAL);

	dpni_ext = (struct dpni_ext_set_rx_tc_dist *) key_cfg_buf;
	dpni_ext->num_extracts = cfg->num_extracts;

	for (i = 0; i < cfg->num_extracts; i++) {
		extr = &dpni_ext->extracts[i];

		switch (cfg->extracts[i].type) {
		case DPKG_EXTRACT_FROM_HDR:
			extr->prot = cfg->extracts[i].extract.from_hdr.prot;
			extr->efh_type =
			    cfg->extracts[i].extract.from_hdr.type & 0x0Fu;
			extr->size = cfg->extracts[i].extract.from_hdr.size;
			extr->offset = cfg->extracts[i].extract.from_hdr.offset;
			extr->field = cfg->extracts[i].extract.from_hdr.field;
			extr->hdr_index =
				cfg->extracts[i].extract.from_hdr.hdr_index;
			break;
		case DPKG_EXTRACT_FROM_DATA:
			extr->size = cfg->extracts[i].extract.from_data.size;
			extr->offset =
				cfg->extracts[i].extract.from_data.offset;
			break;
		case DPKG_EXTRACT_FROM_PARSE:
			extr->size = cfg->extracts[i].extract.from_parse.size;
			extr->offset =
				cfg->extracts[i].extract.from_parse.offset;
			break;
		default:
			return (EINVAL);
		}

		extr->num_of_byte_masks = cfg->extracts[i].num_of_byte_masks;
		extr->extract_type = cfg->extracts[i].type & 0x0Fu;

		for (j = 0; j < DPKG_NUM_OF_MASKS; j++) {
			extr->masks[j].mask = cfg->extracts[i].masks[j].mask;
			extr->masks[j].offset =
				cfg->extracts[i].masks[j].offset;
		}
	}

	return (0);
}

static device_method_t dpaa2_ni_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpaa2_ni_probe),
	DEVMETHOD(device_attach,	dpaa2_ni_attach),
	DEVMETHOD(device_detach,	dpaa2_ni_detach),

	/* mii via memac_mdio */
	DEVMETHOD(miibus_statchg,	dpaa2_ni_miibus_statchg),

	DEVMETHOD_END
};

static driver_t dpaa2_ni_driver = {
	"dpaa2_ni",
	dpaa2_ni_methods,
	sizeof(struct dpaa2_ni_softc),
};

DRIVER_MODULE(miibus, dpaa2_ni, miibus_driver, 0, 0);
DRIVER_MODULE(dpaa2_ni, dpaa2_rc, dpaa2_ni_driver, 0, 0);

MODULE_DEPEND(dpaa2_ni, miibus, 1, 1, 1);
#ifdef DEV_ACPI
MODULE_DEPEND(dpaa2_ni, memac_mdio_acpi, 1, 1, 1);
#endif
#ifdef FDT
MODULE_DEPEND(dpaa2_ni, memac_mdio_fdt, 1, 1, 1);
#endif
