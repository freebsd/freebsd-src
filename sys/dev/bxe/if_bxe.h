/*-
 * Copyright (c) 2007-2011 Broadcom Corporation. All rights reserved.
 *
 *    Gary Zambrano <zambrano@broadcom.com>
 *    David Christensen <davidch@broadcom.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Broadcom Corporation nor the name of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written consent.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

 /*$FreeBSD$*/

#ifndef _IF_BXE_H
#define	_IF_BXE_H

#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/pcpu.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/if_types.h>
#include <net/if_vlan_var.h>
#include <net/zlib.h>
#include <net/bpf.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <machine/resource.h>
#include <machine/in_cksum.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

/*
 * Device identification definitions.
 */
#define	BRCM_VENDORID			0x14E4
#define	BRCM_DEVICEID_BCM57710		0x164E
#define	BRCM_DEVICEID_BCM57711		0x164F
#define	BRCM_DEVICEID_BCM57711E		0x1650

#define PCI_ANY_ID			(u_int16_t) (~0U)


struct bxe_type {
	u_int16_t		bxe_vid;
	u_int16_t		bxe_did;
	u_int16_t		bxe_svid;
	u_int16_t		bxe_sdid;
	char			*bxe_name;
};

#define	STORM_ASSERT_ARRAY_SIZE	50

#define	ATTN_NIG_FOR_FUNC	(1L << 8)
#define	ATTN_SW_TIMER_4_FUNC	(1L << 9)
#define	GPIO_2_FUNC		(1L << 10)
#define	GPIO_3_FUNC		(1L << 11)
#define	GPIO_4_FUNC		(1L << 12)

#define	ATTN_GENERAL_ATTN_1	(1L << 13)
#define	ATTN_GENERAL_ATTN_2	(1L << 14)
#define	ATTN_GENERAL_ATTN_3	(1L << 15)
#define	ATTN_GENERAL_ATTN_4	(1L << 13)
#define	ATTN_GENERAL_ATTN_5	(1L << 14)
#define	ATTN_GENERAL_ATTN_6	(1L << 15)

#define	ATTN_HARD_WIRED_MASK	0xff00


/*
 * Convenience definitions.
 */
#define	BXE_CORE_LOCK_INIT(sc, name)					\
	mtx_init(&(sc->bxe_core_mtx), name,				\
	"BXE Core Lock", MTX_DEF)
#define	BXE_SP_LOCK_INIT(sc, name)					\
	mtx_init(&(sc->bxe_sp_mtx), name,				\
	"BXE Slowpath Lock", MTX_DEF)
#define	BXE_DMAE_LOCK_INIT(sc, name)					\
	mtx_init(&(sc->bxe_dmae_mtx), name,				\
	"BXE DMAE Lock", MTX_DEF)
#define	BXE_PHY_LOCK_INIT(sc, name)					\
	mtx_init(&(sc->port.bxe_phy_mtx), name,				\
	"BXE PHY Lock", MTX_DEF)
#define	BXE_FWMB_LOCK_INIT(sc, name)					\
	mtx_init(&(sc->bxe_fwmb_mtx), name,				\
	"BXE FWMB Lock", MTX_DEF)

#define	BXE_PRINT_LOCK_INIT(sc, name)					\
	mtx_init(&(sc->bxe_print_mtx), name,				\
	"BXE PRINT Lock", MTX_DEF)

#define	BXE_CORE_LOCK(sc)						\
	mtx_lock(&(sc->bxe_core_mtx))
#define	BXE_SP_LOCK(sc)							\
	mtx_lock(&(sc->bxe_sp_mtx))
#define	BXE_FP_LOCK(fp)							\
	mtx_lock(&(fp->mtx))
#define	BXE_DMAE_LOCK(sc)						\
	mtx_lock(&(sc->bxe_dmae_mtx))
#define	BXE_PHY_LOCK(sc)						\
	mtx_lock(&(sc->port.bxe_phy_mtx))
#define	BXE_FWMB_LOCK(sc)						\
	mtx_lock(&(sc->bxe_fwmb_mtx))

#define	BXE_PRINT_LOCK(sc)						\
	mtx_lock(&(sc->bxe_print_mtx))

#define	BXE_CORE_LOCK_ASSERT(sc)					\
	mtx_assert(&(sc->bxe_core_mtx), MA_OWNED)
#define	BXE_SP_LOCK_ASSERT(sc)						\
	mtx_assert(&(sc->bxe_sp_mtx), MA_OWNED)
#define	BXE_FP_LOCK_ASSERT(fp)						\
	mtx_assert(&(fp->mtx), MA_OWNED)
#define	BXE_DMAE_LOCK_ASSERT(sc)					\
	mtx_assert(&(sc->bxe_dmae_mtx), MA_OWNED)
#define	BXE_PHY_LOCK_ASSERT(sc)						\
	mtx_assert(&(sc->port.bxe_phy_mtx), MA_OWNED)

#define	BXE_CORE_UNLOCK(sc)						\
	mtx_unlock(&(sc->bxe_core_mtx))
#define	BXE_SP_UNLOCK(sc)						\
	mtx_unlock(&(sc->bxe_sp_mtx))
#define	BXE_FP_UNLOCK(fp)						\
	mtx_unlock(&(fp->mtx))
#define	BXE_DMAE_UNLOCK(sc)						\
	mtx_unlock(&(sc->bxe_dmae_mtx))
#define	BXE_PHY_UNLOCK(sc)						\
	mtx_unlock(&(sc->port.bxe_phy_mtx))
#define	BXE_FWMB_UNLOCK(sc)						\
	mtx_unlock(&(sc->bxe_fwmb_mtx))

#define	BXE_PRINT_UNLOCK(sc)						\
	mtx_unlock(&(sc->bxe_print_mtx))

#define	BXE_CORE_LOCK_DESTROY(sc)					\
	if (mtx_initialized(&(sc->bxe_core_mtx))) {			\
		mtx_destroy(&(sc->bxe_core_mtx));			\
	}
#define	BXE_SP_LOCK_DESTROY(sc)						\
	if (mtx_initialized(&(sc->bxe_sp_mtx))) {			\
		mtx_destroy(&(sc->bxe_sp_mtx));				\
	}
#define	BXE_DMAE_LOCK_DESTROY(sc)					\
	if (mtx_initialized(&(sc->bxe_dmae_mtx))) {			\
		mtx_destroy(&(sc->bxe_dmae_mtx));			\
	}
#define	BXE_PHY_LOCK_DESTROY(sc)					\
	if (mtx_initialized(&(sc->port.bxe_phy_mtx))) {			\
		mtx_destroy(&(sc->port.bxe_phy_mtx));			\
	}

#define	BXE_FWMB_LOCK_DESTROY(sc)					\
	if (mtx_initialized(&(sc->bxe_fwmb_mtx))) {			\
		mtx_destroy(&(sc->bxe_fwmb_mtx));			\
	}

#define	BXE_PRINT_LOCK_DESTROY(sc)					\
	if (mtx_initialized(&(sc->bxe_print_mtx))) {			\
		mtx_destroy(&(sc->bxe_print_mtx));			\
	}

/* Must be used on a CID before placing it on a HW chain. */
#define	HW_CID(sc, x)							\
	((BP_PORT(sc) << 23) | (BP_E1HVN(sc) << 17) | x)

/* Used on a CID received from the HW. */
#define	SW_CID(x)							\
	(le32toh(x) & (COMMON_RAMROD_ETH_RX_CQE_CID >> 7))

#define	CQE_CMD(x)							\
	(le32toh(x) >> COMMON_RAMROD_ETH_RX_CQE_CMD_ID_SHIFT)

#define	DPM_TRIGGER_TYPE	0x40
#define	DOORBELL(sc, cid, val)	do{					\
	bus_space_write_4(sc->bxe_db_btag, sc->bxe_db_bhandle,		\
	((BCM_PAGE_SIZE * (cid)) + DPM_TRIGGER_TYPE), (uint32_t)val);	\
} while(0)

#if (BUS_SPACE_MAXADDR > 0xFFFFFFFF)
/* Define the macro based on whether CPU is 32 or 64 bit. */
#define	U64_LO(y)		((uint64_t) (y) & 0xFFFFFFFF)
#define	U64_HI(y)		((uint64_t) (y) >> 32)
#else
#define	U64_LO(y)		((uint32_t)y)
#define	U64_HI(y)		(0)
#endif

#define	HILO_U64(hi, lo)	(((uint64_t)hi << 32) + lo)

#define	BXE_HAS_WORK(fp)						\
	(bxe_has_rx_work(fp) || bxe_has_tx_work(fp))

/* Define the page size of the host CPU. */
#define	BCM_PAGE_SHIFT		12
#define	BCM_PAGE_SIZE		(1 << BCM_PAGE_SHIFT)
#define	BCM_PAGE_MASK		(~(BCM_PAGE_SIZE - 1))
#define	BCM_PAGE_ALIGN(addr)	((addr + BCM_PAGE_SIZE - 1) & BCM_PAGE_MASK)

#if BCM_PAGE_SIZE != 4096
#error Page sizes other than 4KB not currently supported!
#endif

/* MC hsi */
#define	PAGES_PER_SGE_SHIFT	0
#define	PAGES_PER_SGE		(1 << PAGES_PER_SGE_SHIFT)
#define	SGE_PAGE_SIZE		PAGE_SIZE
#define	SGE_PAGE_SHIFT		PAGE_SHIFT
#define	SGE_PAGE_ALIGN(addr)	PAGE_ALIGN(addr)

/* SGE ring related macros */
#define	NUM_RX_SGE_PAGES	2
#define	RX_SGE_CNT		(BCM_PAGE_SIZE / sizeof(struct eth_rx_sge))
#define	MAX_RX_SGE_CNT		(RX_SGE_CNT - 2)

/* RX_SGE_CNT is required to be a power of 2 */
#define	RX_SGE_MASK		(RX_SGE_CNT - 1)
#define	TOTAL_RX_SGE		(RX_SGE_CNT * NUM_RX_SGE_PAGES)
#define	MAX_RX_SGE		(TOTAL_RX_SGE - 1)
#define	NEXT_SGE_IDX(x)							\
	((((x) & RX_SGE_MASK) == (MAX_RX_SGE_CNT - 1)) ? (x) + 3 : (x) + 1)
#define	RX_SGE(x)		((x) & MAX_RX_SGE)
#define	RX_SGE_PAGE(x)		(((x) & ~RX_SGE_MASK) >> 9)
#define	RX_SGE_IDX(x)		((x) & RX_SGE_MASK)

/* SGE producer mask related macros. */
/* Number of bits in one sge_mask array element. */
#define	RX_SGE_MASK_ELEM_SZ	64
#define	RX_SGE_MASK_ELEM_SHIFT	6
#define	RX_SGE_MASK_ELEM_MASK	((uint64_t)RX_SGE_MASK_ELEM_SZ - 1)

/*
 * Creates a bitmask of all ones in less significant bits.
 * idx - index of the most significant bit in the created mask.
 */
#define	RX_SGE_ONES_MASK(idx)						\
	(((uint64_t)0x1 << (((idx) & RX_SGE_MASK_ELEM_MASK) + 1)) - 1)
#define	RX_SGE_MASK_ELEM_ONE_MASK	((uint64_t)(~0))

/* Number of uint64_t elements in SGE mask array. */
#define	RX_SGE_MASK_LEN							\
	((NUM_RX_SGE_PAGES * RX_SGE_CNT) / RX_SGE_MASK_ELEM_SZ)
#define	RX_SGE_MASK_LEN_MASK	(RX_SGE_MASK_LEN - 1)
#define	NEXT_SGE_MASK_ELEM(el)	(((el) + 1) & RX_SGE_MASK_LEN_MASK)

/*
 * Transmit Buffer Descriptor (tx_bd) definitions*
 */

/* ToDo: Tune this value based on multi-queue/RSS enable/disable. */
#define	NUM_TX_PAGES		2

#define	TOTAL_TX_BD_PER_PAGE	(BCM_PAGE_SIZE / sizeof(union eth_tx_bd_types))
#define	USABLE_TX_BD_PER_PAGE	(TOTAL_TX_BD_PER_PAGE - 1)
#define	TOTAL_TX_BD		(TOTAL_TX_BD_PER_PAGE * NUM_TX_PAGES)
#define	USABLE_TX_BD		(USABLE_TX_BD_PER_PAGE * NUM_TX_PAGES)
#define	MAX_TX_AVAIL		(USABLE_TX_BD_PER_PAGE * NUM_TX_PAGES - 2)
#define	MAX_TX_BD		(TOTAL_TX_BD - 1)
#define	NEXT_TX_BD(x)							\
	((((x) & USABLE_TX_BD_PER_PAGE) ==				\
	(USABLE_TX_BD_PER_PAGE - 1)) ? (x) + 2 : (x) + 1)
#define	TX_BD(x)		((x) & MAX_TX_BD)
#define	TX_PAGE(x)		(((x) & ~USABLE_TX_BD_PER_PAGE) >> 8)
#define	TX_IDX(x)		((x) & USABLE_TX_BD_PER_PAGE)

/*
 * Receive Buffer Descriptor (rx_bd) definitions*
 */
#define	NUM_RX_PAGES		2

/* 512 (0x200) of 8 byte bds in 4096 byte page. */
#define	TOTAL_RX_BD_PER_PAGE	(BCM_PAGE_SIZE / sizeof(struct eth_rx_bd))

/* 510 (0x1fe) = 512 - 2 */
#define	USABLE_RX_BD_PER_PAGE	(TOTAL_RX_BD_PER_PAGE - 2)

/* 1024 (0x400) */
#define	TOTAL_RX_BD		(TOTAL_RX_BD_PER_PAGE * NUM_RX_PAGES)

/* 1020 (0x3fc) = 1024 - 4 */
#define	USABLE_RX_BD		(USABLE_RX_BD_PER_PAGE * NUM_RX_PAGES)

/* 1023 (0x3ff) = 1024 -1 */
#define	MAX_RX_BD		(TOTAL_RX_BD - 1)

/* 511 (0x1ff) = 512 - 1 */
#define	RX_DESC_MASK		(TOTAL_RX_BD_PER_PAGE - 1)

#define	NEXT_RX_BD(x)							\
	((((x) & RX_DESC_MASK) ==					\
	(USABLE_RX_BD_PER_PAGE - 1)) ? (x) + 3 : (x) + 1)
/* x & 0x3ff */
#define	RX_BD(x)		((x) & MAX_RX_BD)
#define	RX_PAGE(x)		(((x) & ~RX_DESC_MASK) >> 9)
#define	RX_IDX(x)		((x) & RX_DESC_MASK)

/*
 * Receive Completion Queue definitions*
 */

/* CQEs (32 bytes) are 4 times larger than rx_bd's (8 bytes). */
#define	NUM_RCQ_PAGES		(NUM_RX_PAGES * 4)

/* 128 (0x80) */
#define	TOTAL_RCQ_ENTRIES_PER_PAGE (BCM_PAGE_SIZE / sizeof(union eth_rx_cqe))

/* 127 (0x7f)for the next page RCQ bd */
#define	USABLE_RCQ_ENTRIES_PER_PAGE	(TOTAL_RCQ_ENTRIES_PER_PAGE - 1)

/* 1024 (0x400) */
#define	TOTAL_RCQ_ENTRIES	(TOTAL_RCQ_ENTRIES_PER_PAGE * NUM_RCQ_PAGES)

/* 1016 (0x3f8) */
#define	USABLE_RCQ_ENTRIES	(USABLE_RCQ_ENTRIES_PER_PAGE * NUM_RCQ_PAGES)

/* 1023 (0x3ff) */
#define	MAX_RCQ_ENTRIES		(TOTAL_RCQ_ENTRIES - 1)

#define	NEXT_RCQ_IDX(x)							\
	((((x) & USABLE_RCQ_ENTRIES_PER_PAGE) ==			\
	(USABLE_RCQ_ENTRIES_PER_PAGE - 1)) ? (x) + 2 : (x) + 1)
#define	RCQ_ENTRY(x)		((x) & MAX_RCQ_ENTRIES)
#define	RCQ_PAGE(x)		(((x) & ~USABLE_RCQ_ENTRIES_PER_PAGE) >> 7)
#define	RCQ_IDX(x)		((x) & USABLE_RCQ_ENTRIES_PER_PAGE)

/* Slowpath Queue definitions. */
#define	SP_DESC_CNT		(BCM_PAGE_SIZE / sizeof(struct eth_spe))
#define	MAX_SP_DESC_CNT		(SP_DESC_CNT - 1)
#define	NEXT_SPE(x)		(((x) + 1 == (MAX_SP_DESC_CNT)) ? 0 : (x) + 1)

/* This is needed for determening of last_max */
#define	SUB_S16(a, b)		(int16_t)((int16_t)(a) - (int16_t)(b))

#define	__SGE_MASK_SET_BIT(el, bit)	do {				\
	el = ((el) | ((uint64_t)0x1 << (bit)));				\
} while (0)

#define	__SGE_MASK_CLEAR_BIT(el, bit)	do {				\
	el = ((el) & (~((uint64_t)0x1 << (bit))));			\
} while (0)

#define	SGE_MASK_SET_BIT(fp, idx)					\
	__SGE_MASK_SET_BIT(fp->sge_mask[(idx) >> RX_SGE_MASK_ELEM_SHIFT], \
	    ((idx) & RX_SGE_MASK_ELEM_MASK))

#define	SGE_MASK_CLEAR_BIT(fp, idx)					\
	__SGE_MASK_CLEAR_BIT(fp->sge_mask[(idx) >> RX_SGE_MASK_ELEM_SHIFT], \
	    ((idx) & RX_SGE_MASK_ELEM_MASK))

#define	BXE_TX_TIMEOUT			5
#define	BXE_TX_CLEANUP_THRESHOLD	((USABLE_TX_BD * 7 ) / 8)

#define	BXE_DMA_ALIGN			8
#define	BXE_DMA_BOUNDARY		0

/* ToDo: Need to verify the following 3 values. */

/* Reduce from 13 to leave room for the parsing buffer. */
#define	BXE_MAX_SEGMENTS		12
#define BXE_TSO_MAX_SEGMENTS		32
#define	BXE_TSO_MAX_SIZE		(65535 + sizeof(struct ether_vlan_header))
#define	BXE_TSO_MAX_SEG_SIZE	4096

/*
 * Hardware Support For IP and TCP checksum.
 * (Per packet hardware assist capabilites, derived.
 * from CSUM_* in sys/mbuf.h).
*/
#define	BXE_IF_HWASSIST		(CSUM_IP | CSUM_TCP | CSUM_UDP | CSUM_TSO)

/*
 * Per interface capabilities.
 *
 * ToDo: Consider adding IFCAP_WOL_MAGIC, IFCAP_TOE4,
 * IFCAP_TSO6, IFCAP_WOL_UCAST.
 */
#if __FreeBSD_version < 700000
#define	BXE_IF_CAPABILITIES						\
	(IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING | IFCAP_HWCSUM |		\
	IFCAP_JUMBO_MTU)
#else
	/* TSO was introduced in FreeBSD 7 */
#define	BXE_IF_CAPABILITIES						\
	(IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING | IFCAP_HWCSUM |		\
	IFCAP_JUMBO_MTU | IFCAP_TSO4 | IFCAP_VLAN_HWCSUM)
#endif

/* Some typical Ethernet frame sizes */
#define	BXE_MIN_MTU			60
#define	BXE_MIN_ETHER_MTU		64

#define	BXE_STD_MTU			1500
#define	BXE_STD_ETHER_MTU		1518
#define	BXE_STD_ETHER_MTU_VLAN		1522

#define	BXE_JUMBO_MTU			9000
#define	BXE_JUMBO_ETHER_MTU		9018
#define	BXE_JUMBO_ETHER_MTU_VLAN	9022

#define	BXE_BTR				3
#define	MAX_SPQ_PENDING			8

/* Derived E1HVN constants for rate shaping. */
#define	DEF_MIN_RATE			100

/* Resolution of the rate shaping timer - 100 usec */
#define	RS_PERIODIC_TIMEOUT_USEC	100

/*
 * Resolution of fairness algorithm, in usecs.
 * Coefficient for calculating the actual t_fair.
 */
#define	T_FAIR_COEF		10000000

/*
 * Number of bytes in single QM arbitration cycle.
 * Coefficient for calculating the fairness timer.
 */
#define	QM_ARB_BYTES		40000
#define	FAIR_MEM		2

#define	MIN_BXE_BC_VER		0x00040200

#define BXE_BR_SIZE		4096

#define	BXE_NO_RX_FLAGS							\
	(TSTORM_ETH_DROP_FLAGS_DROP_ALL_PACKETS)

#define	BXE_NORMAL_RX_FLAGS						\
	(TSTORM_ETH_DROP_FLAGS_DROP_TCP_CS_ERROR_FLG |			\
	TSTORM_ETH_DROP_FLAGS_DROP_IP_CS_ERROR_FLG |			\
	TSTORM_ETH_DROP_FLAGS_DONT_DROP_MAC_ERR_FLG |			\
	TSTORM_ETH_DROP_FLAGS_DROP_TOO_BIG_PACKETS |			\
	TSTORM_ETH_DROP_FLAGS_DROP_UNMATCH_UNICAST |			\
	TSTORM_ETH_DROP_FLAGS_DROP_UNMATCH_MULTICAST |			\
	TSTORM_ETH_DROP_FLAGS_DONT_DROP_TTL0_FLG)

#define	BXE_ALLMULTI_RX_FLAGS						\
	(TSTORM_ETH_DROP_FLAGS_DROP_TCP_CS_ERROR_FLG |			\
	TSTORM_ETH_DROP_FLAGS_DROP_IP_CS_ERROR_FLG |			\
	 TSTORM_ETH_DROP_FLAGS_DONT_DROP_MAC_ERR_FLG |			\
	 TSTORM_ETH_DROP_FLAGS_DROP_TOO_BIG_PACKETS |			\
	 TSTORM_ETH_DROP_FLAGS_DROP_UNMATCH_UNICAST |			\
	 TSTORM_ETH_DROP_FLAGS_DONT_DROP_TTL0_FLG)

#define	BXE_PROMISC_RX_FLAGS						\
	(TSTORM_ETH_DROP_FLAGS_DONT_DROP_TTL0_FLG)



/*
 * External definitions.
 */
/* FreeBSD multip proc number of active cpus on the system. */
extern int mp_ncpus;

#define	MAX_DYNAMIC_ATTN_GRPS		8

#define	MAC_STX_NA			0xffffffff

/* Attention group wiring. */
struct attn_route {
    uint32_t sig[4];
};

struct regp {
	uint32_t lo;
	uint32_t hi;
};

struct nig_stats {
	uint32_t brb_discard;
	uint32_t brb_packet;
	uint32_t brb_truncate;
	uint32_t flow_ctrl_discard;
	uint32_t flow_ctrl_octets;
	uint32_t flow_ctrl_packet;
	uint32_t mng_discard;
	uint32_t mng_octet_inp;
	uint32_t mng_octet_out;
	uint32_t mng_packet_inp;
	uint32_t mng_packet_out;
	uint32_t pbf_octets;
	uint32_t pbf_packet;
	uint32_t safc_inp;
	uint32_t egress_mac_pkt0_lo;
	uint32_t egress_mac_pkt0_hi;
	uint32_t egress_mac_pkt1_lo;
	uint32_t egress_mac_pkt1_hi;
};

enum bxe_stats_event {
	STATS_EVENT_PMF = 0,
	STATS_EVENT_LINK_UP,
	STATS_EVENT_UPDATE,
	STATS_EVENT_STOP,
	STATS_EVENT_MAX
};

enum bxe_stats_state {
	STATS_STATE_DISABLED = 0,
	STATS_STATE_ENABLED,
	STATS_STATE_MAX
};

struct bxe_eth_stats {
	uint32_t total_bytes_received_hi;
	uint32_t total_bytes_received_lo;
	uint32_t total_bytes_transmitted_hi;
	uint32_t total_bytes_transmitted_lo;
	uint32_t total_unicast_packets_received_hi;
	uint32_t total_unicast_packets_received_lo;
	uint32_t total_multicast_packets_received_hi;
	uint32_t total_multicast_packets_received_lo;
	uint32_t total_broadcast_packets_received_hi;
	uint32_t total_broadcast_packets_received_lo;
	uint32_t total_unicast_packets_transmitted_hi;
	uint32_t total_unicast_packets_transmitted_lo;
	uint32_t total_multicast_packets_transmitted_hi;
	uint32_t total_multicast_packets_transmitted_lo;
	uint32_t total_broadcast_packets_transmitted_hi;
	uint32_t total_broadcast_packets_transmitted_lo;
	uint32_t valid_bytes_received_hi;
	uint32_t valid_bytes_received_lo;
	uint32_t error_bytes_received_hi;
	uint32_t error_bytes_received_lo;
	uint32_t rx_stat_ifhcinbadoctets_hi;
	uint32_t rx_stat_ifhcinbadoctets_lo;
	uint32_t tx_stat_ifhcoutbadoctets_hi;
	uint32_t tx_stat_ifhcoutbadoctets_lo;
	uint32_t rx_stat_dot3statsfcserrors_hi;
	uint32_t rx_stat_dot3statsfcserrors_lo;
	uint32_t rx_stat_dot3statsalignmenterrors_hi;
	uint32_t rx_stat_dot3statsalignmenterrors_lo;
	uint32_t rx_stat_dot3statscarriersenseerrors_hi;
	uint32_t rx_stat_dot3statscarriersenseerrors_lo;
	uint32_t rx_stat_falsecarriererrors_hi;
	uint32_t rx_stat_falsecarriererrors_lo;
	uint32_t rx_stat_etherstatsundersizepkts_hi;
	uint32_t rx_stat_etherstatsundersizepkts_lo;
	uint32_t rx_stat_dot3statsframestoolong_hi;
	uint32_t rx_stat_dot3statsframestoolong_lo;
	uint32_t rx_stat_etherstatsfragments_hi;
	uint32_t rx_stat_etherstatsfragments_lo;
	uint32_t rx_stat_etherstatsjabbers_hi;
	uint32_t rx_stat_etherstatsjabbers_lo;
	uint32_t rx_stat_maccontrolframesreceived_hi;
	uint32_t rx_stat_maccontrolframesreceived_lo;
	uint32_t rx_stat_bmac_xpf_hi;
	uint32_t rx_stat_bmac_xpf_lo;
	uint32_t rx_stat_bmac_xcf_hi;
	uint32_t rx_stat_bmac_xcf_lo;
	uint32_t rx_stat_xoffstateentered_hi;
	uint32_t rx_stat_xoffstateentered_lo;
	uint32_t rx_stat_xonpauseframesreceived_hi;
	uint32_t rx_stat_xonpauseframesreceived_lo;
	uint32_t rx_stat_xoffpauseframesreceived_hi;
	uint32_t rx_stat_xoffpauseframesreceived_lo;
	uint32_t tx_stat_outxonsent_hi;
	uint32_t tx_stat_outxonsent_lo;
	uint32_t tx_stat_outxoffsent_hi;
	uint32_t tx_stat_outxoffsent_lo;
	uint32_t tx_stat_flowcontroldone_hi;
	uint32_t tx_stat_flowcontroldone_lo;
	uint32_t tx_stat_etherstatscollisions_hi;
	uint32_t tx_stat_etherstatscollisions_lo;
	uint32_t tx_stat_dot3statssinglecollisionframes_hi;
	uint32_t tx_stat_dot3statssinglecollisionframes_lo;
	uint32_t tx_stat_dot3statsmultiplecollisionframes_hi;
	uint32_t tx_stat_dot3statsmultiplecollisionframes_lo;
	uint32_t tx_stat_dot3statsdeferredtransmissions_hi;
	uint32_t tx_stat_dot3statsdeferredtransmissions_lo;
	uint32_t tx_stat_dot3statsexcessivecollisions_hi;
	uint32_t tx_stat_dot3statsexcessivecollisions_lo;
	uint32_t tx_stat_dot3statslatecollisions_hi;
	uint32_t tx_stat_dot3statslatecollisions_lo;
	uint32_t tx_stat_etherstatspkts64octets_hi;
	uint32_t tx_stat_etherstatspkts64octets_lo;
	uint32_t tx_stat_etherstatspkts65octetsto127octets_hi;
	uint32_t tx_stat_etherstatspkts65octetsto127octets_lo;
	uint32_t tx_stat_etherstatspkts128octetsto255octets_hi;
	uint32_t tx_stat_etherstatspkts128octetsto255octets_lo;
	uint32_t tx_stat_etherstatspkts256octetsto511octets_hi;
	uint32_t tx_stat_etherstatspkts256octetsto511octets_lo;
	uint32_t tx_stat_etherstatspkts512octetsto1023octets_hi;
	uint32_t tx_stat_etherstatspkts512octetsto1023octets_lo;
	uint32_t tx_stat_etherstatspkts1024octetsto1522octets_hi;
	uint32_t tx_stat_etherstatspkts1024octetsto1522octets_lo;
	uint32_t tx_stat_etherstatspktsover1522octets_hi;
	uint32_t tx_stat_etherstatspktsover1522octets_lo;
	uint32_t tx_stat_bmac_2047_hi;
	uint32_t tx_stat_bmac_2047_lo;
	uint32_t tx_stat_bmac_4095_hi;
	uint32_t tx_stat_bmac_4095_lo;
	uint32_t tx_stat_bmac_9216_hi;
	uint32_t tx_stat_bmac_9216_lo;
	uint32_t tx_stat_bmac_16383_hi;
	uint32_t tx_stat_bmac_16383_lo;
	uint32_t tx_stat_dot3statsinternalmactransmiterrors_hi;
	uint32_t tx_stat_dot3statsinternalmactransmiterrors_lo;
	uint32_t tx_stat_bmac_ufl_hi;
	uint32_t tx_stat_bmac_ufl_lo;
	uint32_t brb_drop_hi;
	uint32_t brb_drop_lo;
	uint32_t brb_truncate_hi;
	uint32_t brb_truncate_lo;
	uint32_t pause_frames_received_hi;
	uint32_t pause_frames_received_lo;
	uint32_t pause_frames_sent_hi;
	uint32_t pause_frames_sent_lo;
	uint32_t jabber_packets_received;

	uint32_t etherstatspkts1024octetsto1522octets_hi;
	uint32_t etherstatspkts1024octetsto1522octets_lo;
	uint32_t etherstatspktsover1522octets_hi;
	uint32_t etherstatspktsover1522octets_lo;

	uint32_t no_buff_discard_hi;
	uint32_t no_buff_discard_lo;

	uint32_t mac_filter_discard;
	uint32_t xxoverflow_discard;
	uint32_t brb_truncate_discard;
	uint32_t mac_discard;

	uint32_t driver_xoff;
	uint32_t rx_err_discard_pkt;
	uint32_t rx_skb_alloc_failed;
	uint32_t hw_csum_err;

	uint32_t nig_timer_max;
};

#define	STATS_OFFSET32(stat_name)					\
	(offsetof(struct bxe_eth_stats, stat_name) / 4)

#define	MAX_CONTEXT			16

union cdu_context {
	struct eth_context eth;
	char pad[1024];
};

/* Load/unload mode. */
#define	LOAD_NORMAL			0
#define	LOAD_OPEN			1
#define	LOAD_DIAG			2
#define	UNLOAD_NORMAL			0
#define	UNLOAD_CLOSE			1

#define	BXE_MAX_POLL_COUNT		1024

struct sw_rx_bd {
   struct mbuf *pmbuf;
};

/*
 * Common data structure.
 * This information is shared across all ports and functions.
 */
struct bxe_common {
	uint32_t	chip_id;
/* chip num:16-31, rev:12-15, metal:4-11, bond_id:0-3 */
#define	CHIP_ID(sc)		(sc->common.chip_id & 0xfffffff0)
#define	CHIP_NUM(sc)		(sc->common.chip_id >> 16)
#define	CHIP_NUM_57710		0x164e
#define	CHIP_NUM_57711		0x164f
#define	CHIP_NUM_57711E		0x1650
#define	CHIP_IS_E1(sc)		(CHIP_NUM(sc) == CHIP_NUM_57710)
#define	CHIP_IS_57711(sc)	(CHIP_NUM(sc) == CHIP_NUM_57711)
#define	CHIP_IS_57711E(sc)	(CHIP_NUM(sc) == CHIP_NUM_57711E)
#define	CHIP_IS_E1H(sc)		(CHIP_IS_57711(sc) || CHIP_IS_57711E(sc))
#define	CHIP_IS_MF_CAP(sc)	(CHIP_IS_57711E(sc))
#define	IS_E1H_OFFSET		CHIP_IS_E1H(sc)

#define	CHIP_REV(sc)		((sc->common.chip_id) & 0x0000f000)
#define	CHIP_REV_Ax		0x00000000
#define	CHIP_REV_Bx		0x00001000
#define	CHIP_REV_Cx		0x00002000

#define	CHIP_METAL(sc)		((sc->common.chip_id) & 0x00000ff0)
#define	CHIP_BOND_ID(sc)	((sc->common.chip_id) & 0x0000000f)

	int		flash_size;
#define	NVRAM_1MB_SIZE		0x20000
#define	NVRAM_TIMEOUT_COUNT	30000
#define	NVRAM_PAGE_SIZE		256

	/* Bootcode shared memory address in BAR memory. */
	uint32_t	shmem_base;
	uint32_t	shmem2_base;

	/* Device configuration read from bootcode shared memory. */
	uint32_t	hw_config;
	uint32_t	bc_ver;

};	/* End struct bxe_common */

/*
 * Port specifc data structure.
 */
struct bxe_port {
	/*
	 * Port Management Function (for 57711E only).
	 * When this field is set the driver instance is
	 * responsible for managing port specifc
	 * configurations such as handling link attentions.
	 */
	uint32_t	pmf;

	/* Ethernet maximum transmission unit. */
	uint16_t	ether_mtu;

	uint32_t	link_config;

	/* Defines the features	supported by the PHY. */
	uint32_t	supported;
#define	SUPPORTED_10baseT_Half		(1 << 1)
#define	SUPPORTED_10baseT_Full		(1 << 2)
#define	SUPPORTED_100baseT_Half		(1 << 3)
#define	SUPPORTED_100baseT_Full		(1 << 4)
#define	SUPPORTED_1000baseT_Half	(1 << 5)
#define	SUPPORTED_1000baseT_Full	(1 << 6)
#define	SUPPORTED_TP			(1 << 7)
#define	SUPPORTED_FIBRE			(1 << 8)
#define	SUPPORTED_Autoneg		(1 << 9)
#define	SUPPORTED_Asym_Pause		(1 << 10)
#define	SUPPORTED_Pause			(1 << 11)
#define	SUPPORTED_2500baseX_Full	(1 << 15)
#define	SUPPORTED_10000baseT_Full	(1 << 16)

	/* Defines the features	advertised by the PHY. */
	uint32_t	advertising;
#define	ADVERTISED_10baseT_Half		(1 << 1)
#define	ADVERTISED_10baseT_Full		(1 << 2)
#define	ADVERTISED_100baseT_Half	(1 << 3)
#define	ADVERTISED_100baseT_Full	(1 << 4)
#define	ADVERTISED_1000baseT_Half	(1 << 5)
#define	ADVERTISED_1000baseT_Full	(1 << 6)
#define	ADVERTISED_TP			(1 << 7)
#define	ADVERTISED_FIBRE		(1 << 8)
#define	ADVERTISED_Autoneg		(1 << 9)
#define	ADVERTISED_Asym_Pause		(1 << 10)
#define	ADVERTISED_Pause		(1 << 11)
#define	ADVERTISED_2500baseX_Full	(1 << 15)
#define	ADVERTISED_10000baseT_Full	(1 << 16)

	uint32_t	phy_addr;

	/* Used to synchronize phy accesses. */
	struct mtx	bxe_phy_mtx;

	/*
	 * MCP scratchpad address for port specific statistics.
	 * The device is responsible for writing statistcss
	 * back to the MCP for use with management firmware such
	 * as UMP/NC-SI.
	 */
	uint32_t	port_stx;

	struct nig_stats	old_nig_stats;
};	/* End struct bxe_port */

/* DMAE command defines */
#define	DMAE_CMD_SRC_PCI		0
#define	DMAE_CMD_SRC_GRC		DMAE_COMMAND_SRC

#define	DMAE_CMD_DST_PCI		(1 << DMAE_COMMAND_DST_SHIFT)
#define	DMAE_CMD_DST_GRC		(2 << DMAE_COMMAND_DST_SHIFT)

#define	DMAE_CMD_C_DST_PCI		0
#define	DMAE_CMD_C_DST_GRC		(1 << DMAE_COMMAND_C_DST_SHIFT)

#define	DMAE_CMD_C_ENABLE		DMAE_COMMAND_C_TYPE_ENABLE

#define	DMAE_CMD_ENDIANITY_NO_SWAP	(0 << DMAE_COMMAND_ENDIANITY_SHIFT)
#define	DMAE_CMD_ENDIANITY_B_SWAP	(1 << DMAE_COMMAND_ENDIANITY_SHIFT)
#define	DMAE_CMD_ENDIANITY_DW_SWAP	(2 << DMAE_COMMAND_ENDIANITY_SHIFT)
#define	DMAE_CMD_ENDIANITY_B_DW_SWAP	(3 << DMAE_COMMAND_ENDIANITY_SHIFT)

#define	DMAE_CMD_PORT_0			0
#define	DMAE_CMD_PORT_1			DMAE_COMMAND_PORT

#define	DMAE_CMD_SRC_RESET		DMAE_COMMAND_SRC_RESET
#define	DMAE_CMD_DST_RESET		DMAE_COMMAND_DST_RESET
#define	DMAE_CMD_E1HVN_SHIFT		DMAE_COMMAND_E1HVN_SHIFT

#define	DMAE_LEN32_RD_MAX		0x80
#define	DMAE_LEN32_WR_MAX(sc)		(CHIP_IS_E1(sc) ? 0x400 : 0x2000)

#define	DMAE_COMP_VAL			0xe0d0d0ae

#define	MAX_DMAE_C			8
#define	MAX_DMAE_C_PER_PORT		8

#define	INIT_DMAE_C(sc)							\
	(BP_PORT(sc) * MAX_DMAE_C_PER_PORT + BP_E1HVN(sc))
#define	PMF_DMAE_C(sc)							\
	(BP_PORT(sc) * MAX_DMAE_C_PER_PORT + E1HVN_MAX)

/*
 * This is the slowpath data structure.  It is mapped into non-paged memory
 * so that the hardware can access it's contents directly and must be page
 * aligned.
 */
struct bxe_slowpath {
	/*
	 * The cdu_context array MUST be the first element in this
	 * structure.  It is used during the leading edge ramrod
	 * operation.
	 */
	union cdu_context	context[MAX_CONTEXT];

	struct eth_stats_query	fw_stats;

	/* Used as a DMA source for MAC configuration. */
	struct mac_configuration_cmd	mac_config;
	struct mac_configuration_cmd	mcast_config;

	/* Used by the DMAE command executer. */
	struct dmae_command	dmae[MAX_DMAE_C];

	/* Statistics completion. */
	uint32_t		stats_comp;

	/* Firmware defined statistics blocks. */
	union mac_stats		mac_stats;
	struct nig_stats	nig_stats;
	struct host_port_stats	port_stats;
	struct host_func_stats	func_stats;
	struct host_func_stats	func_stats_base;

	/* DMAE completion value. */
	uint32_t		wb_comp;
#define	BXE_WB_COMP_VAL		0xe0d0d0ae

	/* DMAE data source/sink. */
	uint32_t		wb_data[4];
};	/* End struct bxe_slowpath */

#define	BXE_SP(sc, var)		(&sc->slowpath->var)
#define	BXE_SP_CHECK(sc, var)	((sc->slowpath) ? (&sc->slowpath->var) : NULL)
#define	BXE_SP_MAPPING(sc, var)						\
	(sc->slowpath_paddr + offsetof(struct bxe_slowpath, var))

union db_prod {
	struct doorbell_set_prod data;
	uint32_t		 raw;
};

struct bxe_q_stats {
	uint32_t total_bytes_received_hi;
	uint32_t total_bytes_received_lo;
	uint32_t total_bytes_transmitted_hi;
	uint32_t total_bytes_transmitted_lo;
	uint32_t total_unicast_packets_received_hi;
	uint32_t total_unicast_packets_received_lo;
	uint32_t total_multicast_packets_received_hi;
	uint32_t total_multicast_packets_received_lo;
	uint32_t total_broadcast_packets_received_hi;
	uint32_t total_broadcast_packets_received_lo;
	uint32_t total_unicast_packets_transmitted_hi;
	uint32_t total_unicast_packets_transmitted_lo;
	uint32_t total_multicast_packets_transmitted_hi;
	uint32_t total_multicast_packets_transmitted_lo;
	uint32_t total_broadcast_packets_transmitted_hi;
	uint32_t total_broadcast_packets_transmitted_lo;
	uint32_t valid_bytes_received_hi;
	uint32_t valid_bytes_received_lo;
	uint32_t error_bytes_received_hi;
	uint32_t error_bytes_received_lo;
	uint32_t etherstatsoverrsizepkts_hi;
	uint32_t etherstatsoverrsizepkts_lo;
	uint32_t no_buff_discard_hi;
	uint32_t no_buff_discard_lo;
	uint32_t driver_xoff;
	uint32_t rx_err_discard_pkt;
	uint32_t rx_skb_alloc_failed;
	uint32_t hw_csum_err;
};

/*
 * This is the fastpath data structure.  There can be up to MAX_CONTEXT
 * instances of the fastpath structure when using RSS/multi-queue.
 */
struct bxe_fastpath {
	/* Pointer back to parent structure. */
	struct bxe_softc	*sc;

	struct mtx		mtx;
	char			mtx_name[16];

	/* Hardware maintained status block. */
	bus_dma_tag_t		status_block_tag;
	bus_dmamap_t		status_block_map;
	struct host_status_block	*status_block;
	bus_addr_t		status_block_paddr;
#ifdef notyet
	/*
	 * In this implementation the doorbell data block
	 * (eth_tx_db_data) is mapped into memory immediately
	 * following the status block and is part of the same
	 * memory allocation.
	 */
	struct eth_tx_db_data	*hw_tx_prods;
	bus_addr_t		tx_prods_paddr;
#endif

	/* Hardware maintained TX buffer descriptor chains. */
	bus_dma_tag_t		tx_bd_chain_tag;
	bus_dmamap_t		tx_bd_chain_map[NUM_TX_PAGES];

	union eth_tx_bd_types	*tx_bd_chain[NUM_TX_PAGES];
	bus_addr_t		tx_bd_chain_paddr[NUM_TX_PAGES];

	/* Bus resource tag for TX mbufs. */
	bus_dma_tag_t		tx_mbuf_tag;
	bus_dmamap_t		tx_mbuf_map[TOTAL_TX_BD];
	struct mbuf		*tx_mbuf_ptr[TOTAL_TX_BD];

	/* Hardware maintained RX buffer descriptor chains. */
	bus_dma_tag_t		rx_bd_chain_tag;
	bus_dmamap_t		rx_bd_chain_map[NUM_RX_PAGES];
	struct eth_rx_bd	*rx_bd_chain[NUM_RX_PAGES];
	bus_addr_t		rx_bd_chain_paddr[NUM_RX_PAGES];

	/* Bus resource tag for RX mbufs. */
	bus_dma_tag_t		rx_mbuf_tag;
	bus_dmamap_t		rx_mbuf_map[TOTAL_RX_BD];
	struct mbuf		*rx_mbuf_ptr[TOTAL_RX_BD];

	/* Hardware maintained Completion Queue (CQ) chains. */
	bus_dma_tag_t		rx_cq_chain_tag;
	bus_dmamap_t		rx_cq_chain_map[NUM_RCQ_PAGES];
	union eth_rx_cqe	*rx_cq_chain[NUM_RCQ_PAGES];
	bus_addr_t		rx_cq_chain_paddr[NUM_RCQ_PAGES];

	/* Ticks until chip reset. */
	int			watchdog_timer;

	/* Taskqueue reqources. */
	struct task		task;
	struct taskqueue	*tq;

	/* Fastpath state. */
	/* ToDo: Why use 'int' here, why not 'uint32_t'? */
	int			state;
#define	BXE_FP_STATE_CLOSED	0x00000
#define	BXE_FP_STATE_IRQ	0x80000
#define	BXE_FP_STATE_OPENING	0x90000
#define	BXE_FP_STATE_OPEN	0xa0000
#define	BXE_FP_STATE_HALTING	0xb0000
#define	BXE_FP_STATE_HALTED	0xc0000

	/* Self-reference back to this fastpath's queue number. */
	uint8_t			index;
#define	FP_IDX(fp)		(fp->index)

	/* Ethernet client ID (each fastpath set of RX/TX/CQE is a client). */
	uint8_t			cl_id;
#define	BP_CL_ID(sc)		(sc->fp[0].cl_id)

	/* Status block number in hardware. */
	uint8_t			sb_id;
#define	FP_SB_ID(fp)		(fp->sb_id)

	/* Class of service. */
	uint8_t			cos;

	union db_prod		tx_db;

	/* Transmit packet producer index (used in eth_tx_bd). */
	uint16_t		tx_pkt_prod;

	/* Transmit packet consumer index. */
	uint16_t		tx_pkt_cons;

	/* Transmit buffer descriptor prod/cons indices. */
	uint16_t		tx_bd_prod;
	uint16_t		tx_bd_cons;

	/* Driver's copy of the fastpath CSTORM/USTORM indices. */
	uint16_t		fp_c_idx;
	uint16_t		fp_u_idx;

	/* Driver's copy of the receive buffer descriptor prod/cons indices. */
	uint16_t		rx_bd_prod;
	uint16_t		rx_bd_cons;

	/* Driver's copy of the receive completion queue prod/cons indices. */
	uint16_t		rx_cq_prod;
	uint16_t		rx_cq_cons;

	/* Pointer to the receive consumer index in the status block. */
	uint16_t		*rx_cq_cons_sb;

	/*
	 * Pointer to the receive buffer descriptor consumer in the
	 * status block.
	 */
	uint16_t		*rx_bd_cons_sb;

	/* Pointer to the transmit consumer in the status block. */
	uint16_t		*tx_cons_sb;

	/* Free/used buffer descriptor counters. */
	uint16_t		used_tx_bd;

	/* Begin: TPA Related data structure. */

	/* Hardware maintained RX Scatter Gather Entry chains. */
	bus_dma_tag_t		rx_sge_chain_tag;
	bus_dmamap_t		rx_sge_chain_map[NUM_RX_SGE_PAGES];
	struct eth_rx_sge	*rx_sge_chain[NUM_RX_SGE_PAGES];
	bus_addr_t		rx_sge_chain_paddr[NUM_RX_SGE_PAGES];

	/* Bus tag for RX SGE bufs. */
	bus_dma_tag_t		rx_sge_buf_tag;
	bus_dmamap_t		rx_sge_buf_map[TOTAL_RX_SGE];
	struct mbuf		*rx_sge_buf_ptr[TOTAL_RX_SGE];

	uint64_t		sge_mask[RX_SGE_MASK_LEN];
	uint16_t		rx_sge_prod;

	/* The last maximal completed SGE. */
	uint16_t		last_max_sge;

	uint16_t		rx_sge_free_idx;

	/* Use the larger supported size for TPA queue length. */
	bus_dmamap_t		tpa_mbuf_map[ETH_MAX_AGGREGATION_QUEUES_E1H];
	struct mbuf		*tpa_mbuf_ptr[ETH_MAX_AGGREGATION_QUEUES_E1H];
	bus_dma_segment_t	tpa_mbuf_segs[ETH_MAX_AGGREGATION_QUEUES_E1H];

	uint8_t			tpa_state[ETH_MAX_AGGREGATION_QUEUES_E1H];
#define	BXE_TPA_STATE_START	1
#define	BXE_TPA_STATE_STOP	2

	uint8_t			segs;
	uint8_t			disable_tpa;
	/* End: TPA related data structure. */

	struct tstorm_per_client_stats	old_tclient;
	struct ustorm_per_client_stats	old_uclient;
	struct xstorm_per_client_stats	old_xclient;
	struct bxe_q_stats	eth_q_stats;

	uint16_t		free_rx_bd;

#if __FreeBSD_version >= 800000
	struct buf_ring		*br;
#endif

	/* Recieve/transmit packet counters. */
	unsigned long		rx_pkts;
	unsigned long		tx_pkts;
	unsigned long		tpa_pkts;
	unsigned long		rx_calls;
	unsigned long		mbuf_alloc_failed;
	unsigned long		mbuf_defrag_attempts;
	unsigned long		mbuf_defrag_failures;
	unsigned long		mbuf_defrag_successes;

	/* Track the number of enqueued mbufs. */
	int			tx_mbuf_alloc;
	int			rx_mbuf_alloc;
	int			sge_mbuf_alloc;
	int			tpa_mbuf_alloc;

	uint64_t		tpa_queue_used;

	unsigned long		null_cqe_flags;
	unsigned long		offload_frames_csum_ip;
	unsigned long		offload_frames_csum_tcp;
	unsigned long		offload_frames_csum_udp;
	unsigned long		offload_frames_tso;
	unsigned long		tx_encap_failures;
	unsigned long		tx_start_called_on_empty_queue;
	unsigned long		tx_queue_too_full;
	unsigned long		tx_dma_mapping_failure;
	unsigned long		window_violation_tso;
	unsigned long		window_violation_std;
	unsigned long		unsupported_tso_request_ipv6;
	unsigned long		unsupported_tso_request_not_tcp;
	unsigned long		tpa_mbuf_alloc_failed;
	unsigned long		tx_chain_lost_mbuf;

	/* FreeBSD interface statistics. */
	unsigned long		soft_rx_errors;
	unsigned long		soft_tx_errors;
	unsigned long		ipackets;
	unsigned long		opackets;

}; /* bxe_fastpath */

/*
 * BXE Device State Data Structure
 */
#define	BXE_STATUS_BLK_SZ						\
	sizeof(struct host_status_block) /* +sizeof(struct eth_tx_db_data) */
#define	BXE_DEF_STATUS_BLK_SZ	sizeof(struct host_def_status_block)
#define	BXE_STATS_BLK_SZ	sizeof(struct bxe_eth_stats)
#define	BXE_SLOWPATH_SZ		sizeof(struct bxe_slowpath)
#define	BXE_SPQ_SZ		BCM_PAGE_SIZE
#define	BXE_TX_CHAIN_PAGE_SZ	BCM_PAGE_SIZE
#define	BXE_RX_CHAIN_PAGE_SZ	BCM_PAGE_SIZE

/* ToDo: Audit this structure for unused varaibles. */
struct bxe_softc {
	struct ifnet		*bxe_ifp;
	int			media;

	/* Parent device handle. */
	device_t		dev;

	/* Driver instance number. */
	u_int8_t		bxe_unit;

	/* FreeBSD network interface media structure. */
	struct ifmedia		bxe_ifmedia;

	/* Bus tag for the bxe controller. */
	bus_dma_tag_t		parent_tag;
	/* OS resources for BAR0 memory. */
	struct resource		*bxe_res;
	bus_space_tag_t		bxe_btag;
	bus_space_handle_t	bxe_bhandle;
	vm_offset_t		bxe_vhandle;

	/* OS resources for BAR2 memory. */

	/* OS resources for BAR1 doorbell memory. */
#define	BXE_DB_SIZE		(16 * 2048)
	struct resource		*bxe_db_res;
	bus_space_tag_t		bxe_db_btag;
	bus_space_handle_t	bxe_db_bhandle;
	vm_offset_t		bxe_db_vhandle;

	/* Driver mutex. */
	struct mtx		bxe_core_mtx;
	struct mtx		bxe_sp_mtx;
	struct mtx		bxe_dmae_mtx;
	struct mtx		bxe_fwmb_mtx;
	struct mtx		bxe_print_mtx;

	/* Per-queue state. */
	/* ToDo: Convert to an array of pointers to conserve memory. */
	struct bxe_fastpath	fp[MAX_CONTEXT];

	int			tx_ring_size;

	/* Legacy interrupt handler resources. */
	struct resource		*bxe_irq_res;
	int			bxe_irq_rid;
	void			*bxe_irq_tag;

	/* MSI-X interrupt handler resources (up to 17 vectors). */
	struct resource		*bxe_msix_res[MAX_CONTEXT + 1];
	int			bxe_msix_rid[MAX_CONTEXT + 1];
	void			*bxe_msix_tag[MAX_CONTEXT + 1];
	int			msix_count;

	/* MSI interrupt handler resources (up to XX vectors). */
#define	BXE_MSI_VECTOR_COUNT	8
	struct resource		*bxe_msi_res[BXE_MSI_VECTOR_COUNT];
	int			bxe_msi_rid[BXE_MSI_VECTOR_COUNT];
	void			*bxe_msi_tag[BXE_MSI_VECTOR_COUNT];
	int			msi_count;

	/* Taskqueue resources. */
	struct task		task;
	struct taskqueue	*tq;
	/* RX Driver parameters*/
	uint32_t		rx_csum;
	int			rx_buf_size;

	/* ToDo: Replace with OS specific defintions. */
#define	ETH_HLEN			14
#define	ETH_OVREHEAD			(ETH_HLEN + 8)	/* 8 for CRC + VLAN */
#define	ETH_MIN_PACKET_SIZE		60
#define	ETH_MAX_PACKET_SIZE		1500
#define	ETH_MAX_JUMBO_PACKET_SIZE	9600

	/* Hardware Maintained Host Default Status Block. */
	bus_dma_tag_t		def_status_block_tag;
	bus_dmamap_t		def_status_block_map;
	struct host_def_status_block	*def_status_block;
	bus_addr_t		def_status_block_paddr;

#define	DEF_SB_ID		16
	uint16_t		def_c_idx;
	uint16_t		def_u_idx;
	uint16_t		def_t_idx;
	uint16_t		def_x_idx;
	uint16_t		def_att_idx;

	uint32_t		attn_state;
	struct attn_route	attn_group[MAX_DYNAMIC_ATTN_GRPS];

	/* H/W maintained statistics block. */
	bus_dma_tag_t		stats_tag;
	bus_dmamap_t		stats_map;
	struct statistics_block	*stats_block;
	bus_addr_t		stats_block_paddr;

	/* H/W maintained slow path. */
	bus_dma_tag_t		slowpath_tag;
	bus_dmamap_t		slowpath_map;
	struct bxe_slowpath	*slowpath;
	bus_addr_t		slowpath_paddr;

 	/* Slow path ring. */
	bus_dma_tag_t		spq_tag;
	bus_dmamap_t		spq_map;
	struct eth_spe		*spq;
	bus_addr_t		spq_paddr;
	uint16_t		spq_prod_idx;
	struct eth_spe		*spq_prod_bd;
	struct eth_spe		*spq_last_bd;
	uint16_t		*dsb_sp_prod;
	uint16_t		*spq_hw_con;
	uint16_t		spq_left;

	/* State information for pending ramrod commands. */
	uint8_t			stats_pending;
	uint8_t			set_mac_pending;

	int			panic;

	/* Device flags. */
	uint32_t		bxe_flags;
#define	BXE_ONE_PORT_FLAG	0x00000004
#define	BXE_NO_WOL_FLAG		0x00000008
#define	BXE_USING_DAC_FLAG	0x00000010
#define	BXE_USING_MSIX_FLAG	0x00000020
#define	BXE_USING_MSI_FLAG	0x00000040
#define	BXE_TPA_ENABLE_FLAG	0x00000080
#define	BXE_NO_MCP_FLAG		0x00000100
#define	BP_NOMCP(sc)		(sc->bxe_flags & BXE_NO_MCP_FLAG)
#define	BXE_SAFC_TX_FLAG	0x00000200

#define	TPA_ENABLED(sc)		(sc->bxe_flags & BXE_TPA_ENABLE_FLAG)

	/* PCI Express function number for the device. */
	int			bxe_func;

/*
 * Ethernet port to PCIe function mapping for
 * 57710 and 57711:
 * +---------------+---------------+-------------+
 * | Ethernet Port | PCIe Function | Virtual NIC |
 * |       0       |       0       |      0      |
 * |       1       |       1       |      0      |
 * +---------------+---------------+-------------+
 *
 * Ethernet port to PCIe function mapping for
 * 57711E:
 * +---------------+---------------+-------------+
 * | Ethernet Port | PCIe Function | Virtual NIC |
 * |       0       |       0       |      1      |
 * |       1       |       1       |      2      |
 * |       0       |       2       |      3      |
 * |       1       |       3       |      4      |
 * |       0       |       4       |      5      |
 * |       1       |       5       |      6      |
 * |       0       |       6       |      7      |
 * |       1       |       7       |      8      |
 * +---------------+---------------+-------------+
 */

#define	BP_PORT(sc)		(sc->bxe_func % PORT_MAX)
#define	BP_FUNC(sc)		(sc->bxe_func)
#define	BP_E1HVN(sc)		(sc->bxe_func >> 1)
#define	BP_L_ID(sc)		(BP_E1HVN(sc) << 2)

	/* PCI Express link information. */
	uint16_t		pcie_link_width;
	uint16_t		pcie_link_speed;
	uint32_t		bxe_cap_flags;
#define	BXE_MSI_CAPABLE_FLAG	0x00000001
#define	BXE_MSIX_CAPABLE_FLAG	0x00000002
#define	BXE_PCIE_CAPABLE_FLAG	0x00000004
	uint16_t		pcie_cap;
	uint16_t		pm_cap;

	/* ToDo: Is this really needed? */
	uint16_t		sp_running;

	/* Driver/firmware synchronization. */
	uint16_t		fw_seq;
	uint16_t		fw_drv_pulse_wr_seq;
	uint32_t		fw_mb;

	/*
	 * MCP scratchpad address for function specific statistics.
	 * The device is responsible for writing statistics back to
	 * the MCP for use with management firmware such as UMP/NC-SI.
	 */
	uint32_t		func_stx;

	struct link_params	link_params;
	struct link_vars	link_vars;

	struct bxe_common	common;
	struct bxe_port		port;

	struct cmng_struct_per_port	cmng;
	uint32_t		vn_wsum;
	uint32_t		cos_wsum;

	uint8_t			ser_lane;
	uint8_t			rx_lane_swap;
	uint8_t			tx_lane_swap;

	uint8_t			wol;

	int			rx_ring_size;

	/* RX/TX Interrupt Coalescing Parameters */
	uint16_t		rx_ticks;
	uint16_t		tx_ticks;

	/* Device State: Used for Driver-FW communication. */
	int			state;
#define	BXE_STATE_CLOSED		0x0
#define	BXE_STATE_OPENING_WAIT4_LOAD	0x1000
#define	BXE_STATE_OPENING_WAIT4_PORT	0x2000
#define	BXE_STATE_OPEN			0x3000
#define	BXE_STATE_CLOSING_WAIT4_HALT	0x4000
#define	BXE_STATE_CLOSING_WAIT4_DELETE	0x5000
#define	BXE_STATE_CLOSING_WAIT4_UNLOAD	0x6000
#define	BXE_STATE_DISABLED		0xD000
#define	BXE_STATE_DIAG			0xE000
#define	BXE_STATE_ERROR			0xF000

/* Driver tunable options. */
	int			int_mode;
	int			multi_mode;
	int			tso_enable;
	int			num_queues;
	int			stats_enable;
	int			mrrs;
	int			dcc_enable;

#define	BXE_NUM_QUEUES(cos)						\
	((bxe_qs_per_cos & (0xff << (cos * 8))) >> (cos * 8))
#define	BXE_MAX_QUEUES(sc)						\
	(IS_E1HMF(sc) ? (MAX_CONTEXT / E1HVN_MAX) : MAX_CONTEXT)


#define	BXE_MAX_COS		3
#define	BXE_MAX_PRIORITY	8
#define	BXE_MAX_ENTRIES_PER_PRI	16

	/* Number of queues per class of service. */
	uint8_t			qs_per_cos[BXE_MAX_COS];

	/* Priority to class of service mapping. */
	uint8_t			pri_map[BXE_MAX_PRIORITY];

	/* min rate per cos */
	uint16_t		cos_min_rate[BXE_MAX_COS];

	/* Class of service to queue mapping. */
	uint8_t			cos_map[BXE_MAX_COS];

	/* Used for multiple function devices. */
	uint32_t		mf_config[E1HVN_MAX];

	/* Outer VLAN tag. */
	uint16_t		e1hov;
#define	IS_E1HOV(sc)		(sc->e1hov != 0)

	uint8_t			e1hmf;
#define	IS_E1HMF(sc)		(sc->e1hmf != 0)

	/* Receive mode settings (i.e promiscuous, multicast, etc.). */
	uint32_t		rx_mode;

#define	BXE_RX_MODE_NONE	0
#define	BXE_RX_MODE_NORMAL	1
#define	BXE_RX_MODE_ALLMULTI	2
#define	BXE_RX_MODE_PROMISC	3
#define	BXE_MAX_MULTICAST	64
#define	BXE_MAX_EMUL_MULTI	16

	uint32_t		rx_mode_cl_mask;

	/* Device name */
	char			*name;

	/* Used to synchronize statistics collection. */
	int			stats_state;
#define	STATS_STATE_DISABLE	0
#define	STATS_STATE_ENABLE	1
#define	STATS_STATE_STOP	2

	int			dmae_ready;

	/* Used by the DMAE command loader. */
	struct dmae_command	stats_dmae;
	struct dmae_command	init_dmae;
	int			executer_idx;

	/* Statistics. */
	uint16_t		stats_counter;

	struct bxe_eth_stats	eth_stats;

	z_streamp		strm;
	bus_dma_tag_t		gunzip_tag;
	bus_dmamap_t		gunzip_map;
	void			*gunzip_buf;
	bus_addr_t		gunzip_mapping;
	int			gunzip_outlen;
#define	FW_BUF_SIZE		0x40000

	struct raw_op		*init_ops;
	/* Init blocks offsets inside init_ops */
	const uint16_t		*init_ops_offsets;
	/* Data blob - has 32 bit granularity */
	const uint32_t		*init_data;
	/* PRAM blobs - raw data */
	const uint8_t		*tsem_int_table_data;
	const uint8_t		*tsem_pram_data;
	const uint8_t		*usem_int_table_data;
	const uint8_t		*usem_pram_data;
	const uint8_t		*xsem_int_table_data;
	const uint8_t		*xsem_pram_data;
	const uint8_t		*csem_int_table_data;
	const uint8_t		*csem_pram_data;
#define	INIT_OPS(sc)			(sc->init_ops)
#define	INIT_OPS_OFFSETS(sc)		(sc->init_ops_offsets)
#define	INIT_DATA(sc)			(sc->init_data)
#define	INIT_TSEM_INT_TABLE_DATA(sc)	(sc->tsem_int_table_data)
#define	INIT_TSEM_PRAM_DATA(sc)		(sc->tsem_pram_data)
#define	INIT_USEM_INT_TABLE_DATA(sc)	(sc->usem_int_table_data)
#define	INIT_USEM_PRAM_DATA(sc)		(sc->usem_pram_data)
#define	INIT_XSEM_INT_TABLE_DATA(sc)	(sc->xsem_int_table_data)
#define	INIT_XSEM_PRAM_DATA(sc)		(sc->xsem_pram_data)
#define	INIT_CSEM_INT_TABLE_DATA(sc)	(sc->csem_int_table_data)
#define	INIT_CSEM_PRAM_DATA(sc)		(sc->csem_pram_data)

	/* OS handle for periodic tick routine. */
	struct callout		bxe_tick_callout;

	uint8_t			pad;

	/* Frame size and mbuf allocation size for RX frames. */
	uint32_t		max_frame_size;
	int			mbuf_alloc_size;

	uint16_t		tx_driver;

	/* Verify bxe_function_init is run before handling interrupts. */
	uint8_t			intr_sem;

#ifdef BXE_DEBUG
	unsigned long		debug_mbuf_sim_alloc_failed;
	unsigned long		debug_mbuf_sim_map_failed;
	unsigned long		debug_received_frame_error;
	unsigned long		debug_memory_allocated;

	/* A buffer for hardware/firmware state information (grcdump). */
	uint32_t		*grcdump_buffer;
#endif

	unsigned long 		tx_start_called_with_link_down;
	unsigned long 		tx_start_called_with_queue_full;
}; /* end of struct bxe_softc */

#define	MDIO_AN_CL73_OR_37_COMPLETE					\
	(MDIO_GP_STATUS_TOP_AN_STATUS1_CL73_AUTONEG_COMPLETE |		\
	MDIO_GP_STATUS_TOP_AN_STATUS1_CL37_AUTONEG_COMPLETE)

#define	GP_STATUS_PAUSE_RSOLUTION_TXSIDE				\
	MDIO_GP_STATUS_TOP_AN_STATUS1_PAUSE_RSOLUTION_TXSIDE
#define	GP_STATUS_PAUSE_RSOLUTION_RXSIDE				\
	MDIO_GP_STATUS_TOP_AN_STATUS1_PAUSE_RSOLUTION_RXSIDE
#define	GP_STATUS_SPEED_MASK						\
	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_MASK
#define	GP_STATUS_10M	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10M
#define	GP_STATUS_100M	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_100M
#define	GP_STATUS_1G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_1G
#define	GP_STATUS_2_5G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_2_5G
#define	GP_STATUS_5G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_5G
#define	GP_STATUS_6G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_6G
#define	GP_STATUS_10G_HIG						\
	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_HIG
#define	GP_STATUS_10G_CX4						\
	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_CX4
#define	GP_STATUS_12G_HIG						\
	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12G_HIG
#define	GP_STATUS_12_5G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12_5G
#define	GP_STATUS_13G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_13G
#define	GP_STATUS_15G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_15G
#define	GP_STATUS_16G	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_16G
#define	GP_STATUS_1G_KX	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_1G_KX
#define	GP_STATUS_10G_KX4						\
	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_KX4

#define	LINK_10THD		LINK_STATUS_SPEED_AND_DUPLEX_10THD
#define	LINK_10TFD		LINK_STATUS_SPEED_AND_DUPLEX_10TFD
#define	LINK_100TXHD		LINK_STATUS_SPEED_AND_DUPLEX_100TXHD
#define	LINK_100T4		LINK_STATUS_SPEED_AND_DUPLEX_100T4
#define	LINK_100TXFD		LINK_STATUS_SPEED_AND_DUPLEX_100TXFD
#define	LINK_1000THD		LINK_STATUS_SPEED_AND_DUPLEX_1000THD
#define	LINK_1000TFD		LINK_STATUS_SPEED_AND_DUPLEX_1000TFD
#define	LINK_1000XFD		LINK_STATUS_SPEED_AND_DUPLEX_1000XFD
#define	LINK_2500THD		LINK_STATUS_SPEED_AND_DUPLEX_2500THD
#define	LINK_2500TFD		LINK_STATUS_SPEED_AND_DUPLEX_2500TFD
#define	LINK_2500XFD		LINK_STATUS_SPEED_AND_DUPLEX_2500XFD
#define	LINK_10GTFD		LINK_STATUS_SPEED_AND_DUPLEX_10GTFD
#define	LINK_10GXFD		LINK_STATUS_SPEED_AND_DUPLEX_10GXFD
#define	LINK_12GTFD		LINK_STATUS_SPEED_AND_DUPLEX_12GTFD
#define	LINK_12GXFD		LINK_STATUS_SPEED_AND_DUPLEX_12GXFD
#define	LINK_12_5GTFD		LINK_STATUS_SPEED_AND_DUPLEX_12_5GTFD
#define	LINK_12_5GXFD		LINK_STATUS_SPEED_AND_DUPLEX_12_5GXFD
#define	LINK_13GTFD		LINK_STATUS_SPEED_AND_DUPLEX_13GTFD
#define	LINK_13GXFD		LINK_STATUS_SPEED_AND_DUPLEX_13GXFD
#define	LINK_15GTFD		LINK_STATUS_SPEED_AND_DUPLEX_15GTFD
#define	LINK_15GXFD		LINK_STATUS_SPEED_AND_DUPLEX_15GXFD
#define	LINK_16GTFD		LINK_STATUS_SPEED_AND_DUPLEX_16GTFD
#define	LINK_16GXFD		LINK_STATUS_SPEED_AND_DUPLEX_16GXFD

#define	MEDIUM_FULL_DUPLEX	0
#define	MEDIUM_HALF_DUPLEX	1

#define	DUPLEX_FULL		0
#define	DUPLEX_HALF		1

#define	SPEED_10		10
#define	SPEED_100		100
#define	SPEED_1000		1000
#define	SPEED_2500		2500
#define	SPEED_10000		10000

#ifdef notyet
#define	NIG_STATUS_XGXS0_LINK10G					\
	NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK10G
#define	NIG_STATUS_XGXS0_LINK_STATUS					\
	NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK_STATUS
#define	NIG_STATUS_XGXS0_LINK_STATUS_SIZE				\
	NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK_STATUS_SIZE
#define	NIG_STATUS_SERDES0_LINK_STATUS					\
	NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_SERDES0_LINK_STATUS
#define	NIG_MASK_MI_INT							\
	NIG_MASK_INTERRUPT_PORT0_REG_MASK_EMAC0_MISC_MI_INT
#define	NIG_MASK_XGXS0_LINK10G						\
	NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK10G
#define	NIG_MASK_XGXS0_LINK_STATUS					\
	NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK_STATUS
#define	NIG_MASK_SERDES0_LINK_STATUS					\
	NIG_MASK_INTERRUPT_PORT0_REG_MASK_SERDES0_LINK_STATUS

#define	XGXS_RESET_BITS							\
	(MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_RSTB_HW |	\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_IDDQ |		\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_PWRDWN |		\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_PWRDWN_SD |	\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_TXD_FIFO_RSTB)

#define	SERDES_RESET_BITS						\
	(MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_RSTB_HW |	\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_IDDQ |		\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_PWRDWN |	\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_PWRDWN_SD)

#define	SFP_EEPROM_CON_TYPE_ADDR		0x2
#define	SFP_EEPROM_CON_TYPE_VAL_LC		0x7
#define	SFP_EEPROM_CON_TYPE_VAL_COPPER		0x21

#define	SFP_EEPROM_FC_TX_TECH_ADDR		0x8
#define	SFP_EEPROM_FC_TX_TECH_BITMASK_COPPER_ACTIVE	0x8
#define	SFP_EEPROM_VENDOR_NAME_SIZE		16
#define	SFP_EEPROM_OPTIONS_LINEAR_RX_OUT_MASK	0x1
#define	SFP_EEPROM_OPTIONS_SIZE			2
#endif

#define	BXE_PMF_LINK_ASSERT						\
	GENERAL_ATTEN_OFFSET(LINK_SYNC_ATTENTION_BIT_FUNC_0 + BP_FUNC(sc))

#define	BXE_MC_ASSERT_BITS						\
	(GENERAL_ATTEN_OFFSET(TSTORM_FATAL_ASSERT_ATTENTION_BIT) |	\
	GENERAL_ATTEN_OFFSET(USTORM_FATAL_ASSERT_ATTENTION_BIT) |	\
	GENERAL_ATTEN_OFFSET(CSTORM_FATAL_ASSERT_ATTENTION_BIT) |	\
	GENERAL_ATTEN_OFFSET(XSTORM_FATAL_ASSERT_ATTENTION_BIT))

#define	BXE_MCP_ASSERT							\
	GENERAL_ATTEN_OFFSET(MCP_FATAL_ASSERT_ATTENTION_BIT)

#define	BXE_GRC_TIMEOUT							\
	GENERAL_ATTEN_OFFSET(LATCHED_ATTN_TIMEOUT_GRC)

#define	BXE_GRC_RSV							\
	(GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCR) |			\
	 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCT) |			\
	 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCN) |			\
	 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCU) |			\
	 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCP) |			\
	 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RSVD_GRC))

#define	HW_INTERRUT_ASSERT_SET_0					\
	(AEU_INPUTS_ATTN_BITS_TSDM_HW_INTERRUPT |			\
	AEU_INPUTS_ATTN_BITS_TCM_HW_INTERRUPT |				\
	AEU_INPUTS_ATTN_BITS_TSEMI_HW_INTERRUPT |			\
	AEU_INPUTS_ATTN_BITS_PBF_HW_INTERRUPT)

#define	HW_PRTY_ASSERT_SET_0						\
	(AEU_INPUTS_ATTN_BITS_BRB_PARITY_ERROR |			\
	AEU_INPUTS_ATTN_BITS_PARSER_PARITY_ERROR |			\
	AEU_INPUTS_ATTN_BITS_TSDM_PARITY_ERROR |			\
	AEU_INPUTS_ATTN_BITS_SEARCHER_PARITY_ERROR |			\
	AEU_INPUTS_ATTN_BITS_TSEMI_PARITY_ERROR)

#define	HW_INTERRUT_ASSERT_SET_1					\
	(AEU_INPUTS_ATTN_BITS_QM_HW_INTERRUPT |				\
	AEU_INPUTS_ATTN_BITS_TIMERS_HW_INTERRUPT |			\
	AEU_INPUTS_ATTN_BITS_XSDM_HW_INTERRUPT |			\
	AEU_INPUTS_ATTN_BITS_XCM_HW_INTERRUPT |				\
	AEU_INPUTS_ATTN_BITS_XSEMI_HW_INTERRUPT |			\
	AEU_INPUTS_ATTN_BITS_USDM_HW_INTERRUPT |			\
	AEU_INPUTS_ATTN_BITS_UCM_HW_INTERRUPT |				\
	AEU_INPUTS_ATTN_BITS_USEMI_HW_INTERRUPT |			\
	AEU_INPUTS_ATTN_BITS_UPB_HW_INTERRUPT |				\
	AEU_INPUTS_ATTN_BITS_CSDM_HW_INTERRUPT |			\
	AEU_INPUTS_ATTN_BITS_CCM_HW_INTERRUPT)

#define	HW_PRTY_ASSERT_SET_1						\
	(AEU_INPUTS_ATTN_BITS_PBCLIENT_PARITY_ERROR |			\
	AEU_INPUTS_ATTN_BITS_QM_PARITY_ERROR |				\
	AEU_INPUTS_ATTN_BITS_XSDM_PARITY_ERROR |			\
	AEU_INPUTS_ATTN_BITS_XSEMI_PARITY_ERROR |			\
	AEU_INPUTS_ATTN_BITS_DOORBELLQ_PARITY_ERROR |			\
    	AEU_INPUTS_ATTN_BITS_VAUX_PCI_CORE_PARITY_ERROR |		\
	AEU_INPUTS_ATTN_BITS_DEBUG_PARITY_ERROR |			\
	AEU_INPUTS_ATTN_BITS_USDM_PARITY_ERROR |			\
	AEU_INPUTS_ATTN_BITS_USEMI_PARITY_ERROR |			\
	AEU_INPUTS_ATTN_BITS_UPB_PARITY_ERROR |				\
	AEU_INPUTS_ATTN_BITS_CSDM_PARITY_ERROR)

#define	HW_INTERRUT_ASSERT_SET_2					\
	(AEU_INPUTS_ATTN_BITS_CSEMI_HW_INTERRUPT |			\
	AEU_INPUTS_ATTN_BITS_CDU_HW_INTERRUPT |				\
	AEU_INPUTS_ATTN_BITS_DMAE_HW_INTERRUPT |			\
	AEU_INPUTS_ATTN_BITS_PXPPCICLOCKCLIENT_HW_INTERRUPT |		\
	AEU_INPUTS_ATTN_BITS_MISC_HW_INTERRUPT)

#define	HW_PRTY_ASSERT_SET_2						\
	(AEU_INPUTS_ATTN_BITS_CSEMI_PARITY_ERROR |			\
	AEU_INPUTS_ATTN_BITS_PXP_PARITY_ERROR |				\
	AEU_INPUTS_ATTN_BITS_PXPPCICLOCKCLIENT_PARITY_ERROR |		\
	AEU_INPUTS_ATTN_BITS_CFC_PARITY_ERROR |				\
	AEU_INPUTS_ATTN_BITS_CDU_PARITY_ERROR |				\
	AEU_INPUTS_ATTN_BITS_IGU_PARITY_ERROR |				\
	AEU_INPUTS_ATTN_BITS_MISC_PARITY_ERROR)

/* Stuff added to make the code fit 80Col. */
#define	CQE_TYPE(cqe_fp_flags)	((cqe_fp_flags) & ETH_FAST_PATH_RX_CQE_TYPE)

#define	TPA_TYPE_START		ETH_FAST_PATH_RX_CQE_START_FLG
#define	TPA_TYPE_END		ETH_FAST_PATH_RX_CQE_END_FLG
#define	TPA_TYPE(cqe_fp_flags)						\
	((cqe_fp_flags) & (TPA_TYPE_START | TPA_TYPE_END))

#define	ETH_RX_ERROR_FLAGS	ETH_FAST_PATH_RX_CQE_PHY_DECODE_ERR_FLG

#define	BXE_IP_CSUM_ERR(cqe)						\
	(!((cqe)->fast_path_cqe.status_flags &				\
	ETH_FAST_PATH_RX_CQE_IP_XSUM_NO_VALIDATION_FLG) &&		\
	((cqe)->fast_path_cqe.type_error_flags &			\
	ETH_FAST_PATH_RX_CQE_IP_BAD_XSUM_FLG))

#define	BXE_L4_CSUM_ERR(cqe)						\
	(!((cqe)->fast_path_cqe.status_flags &				\
	ETH_FAST_PATH_RX_CQE_L4_XSUM_NO_VALIDATION_FLG) &&		\
	((cqe)->fast_path_cqe.type_error_flags &			\
	ETH_FAST_PATH_RX_CQE_L4_BAD_XSUM_FLG))

#define	BXE_RX_CSUM_OK(cqe)						\
	(!(BXE_L4_CSUM_ERR(cqe) || BXE_IP_CSUM_ERR(cqe)))

#define	BXE_RX_SUM_FIX(cqe) 						\
	((le16toh(cqe->fast_path_cqe.pars_flags.flags) &		\
	PARSING_FLAGS_OVER_ETHERNET_PROTOCOL) ==			\
	(1 << PARSING_FLAGS_OVER_ETHERNET_PROTOCOL_SHIFT))

#define	MULTI_FLAGS(sc)							\
	(TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_CAPABILITY |	\
	TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_TCP_CAPABILITY |	\
	TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_CAPABILITY |		\
	TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_TCP_CAPABILITY |	\
	(sc->multi_mode <<						\
	TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_MODE_SHIFT))

#define	MULTI_MASK		0x7f

#define	FP_USB_FUNC_OFF		(2 + 2 * HC_USTORM_SB_NUM_INDICES)
#define	FP_CSB_FUNC_OFF		(2 + 2 * HC_CSTORM_SB_NUM_INDICES)

#define	U_SB_ETH_RX_CQ_INDEX	HC_INDEX_U_ETH_RX_CQ_CONS
#define	U_SB_ETH_RX_BD_INDEX	HC_INDEX_U_ETH_RX_BD_CONS
#define	C_SB_ETH_TX_CQ_INDEX	HC_INDEX_C_ETH_TX_CQ_CONS

#define	DEF_USB_FUNC_OFF	(2 + 2 * HC_USTORM_DEF_SB_NUM_INDICES)
#define	DEF_CSB_FUNC_OFF	(2 + 2 * HC_CSTORM_DEF_SB_NUM_INDICES)
#define	DEF_XSB_FUNC_OFF	(2 + 2 * HC_XSTORM_DEF_SB_NUM_INDICES)
#define	DEF_TSB_FUNC_OFF	(2 + 2 * HC_TSTORM_DEF_SB_NUM_INDICES)

#define	C_DEF_SB_SP_INDEX	HC_INDEX_DEF_C_ETH_SLOW_PATH

#define	BXE_RX_SB_INDEX							\
	&fp->status_block->u_status_block.index_values[U_SB_ETH_RX_CQ_INDEX]

#define	BXE_RX_SB_BD_INDEX						\
	(&fp->status_block->u_status_block.index_values[U_SB_ETH_RX_BD_INDEX])

#define	BXE_TX_SB_INDEX							\
	(&fp->status_block->c_status_block.index_values[C_SB_ETH_TX_CQ_INDEX])

#define	BXE_SP_DSB_INDEX						\
	&sc->def_status_block->c_def_status_block.index_values[C_DEF_SB_SP_INDEX]

#define	BXE_RX_SB_INDEX_NUM						\
	(((U_SB_ETH_RX_CQ_INDEX <<					\
	USTORM_ETH_ST_CONTEXT_CONFIG_CQE_SB_INDEX_NUMBER_SHIFT) &	\
	USTORM_ETH_ST_CONTEXT_CONFIG_CQE_SB_INDEX_NUMBER) |		\
	((U_SB_ETH_RX_BD_INDEX <<					\
	USTORM_ETH_ST_CONTEXT_CONFIG_BD_SB_INDEX_NUMBER_SHIFT) &	\
	USTORM_ETH_ST_CONTEXT_CONFIG_BD_SB_INDEX_NUMBER))

#define	CAM_IS_INVALID(x)						\
	((x)->target_table_entry.flags ==				\
	TSTORM_CAM_TARGET_TABLE_ENTRY_ACTION_TYPE)

#define	CAM_INVALIDATE(x)						\
	((x)->target_table_entry.flags = TSTORM_CAM_TARGET_TABLE_ENTRY_ACTION_TYPE)

/* Number of uint32_t elements in multicast hash array. */
#define	MC_HASH_SIZE			8
#define	MC_HASH_OFFSET(sc, i)						\
	(BAR_TSTORM_INTMEM +						\
	TSTORM_APPROXIMATE_MATCH_MULTICAST_FILTERING_OFFSET(BP_FUNC(sc)) + \
	i * 4)

#define	UINT_MAX			(~0U)

/*
 * PCIE Capability Register Definitions. Need to replace with the system
 * header file later.
 */
#define	PCI_EXP_DEVCTL			8
#define	PCI_EXP_DEVCTL_CERE		0x0001
#define	PCI_EXP_DEVCTL_NFERE		0x0002
#define	PCI_EXP_DEVCTL_FERE		0x0004
#define	PCI_EXP_DEVCTL_URRE		0x0008
#define	PCI_EXP_DEVCTL_RELAX_EN		0x0010
#define	PCI_EXP_DEVCTL_PAYLOAD		0x00e0
#define	PCI_EXP_DEVCTL_EXT_TAG		0x0100
#define	PCI_EXP_DEVCTL_PHANTOM		0x0200
#define	PCI_EXP_DEVCTL_AUX_PME		0x0400
#define	PCI_EXP_DEVCTL_NOSNOOP_EN	0x0800
#define	PCI_EXP_DEVCTL_READRQ		0x7000

/*
 * Return Value for bxe_attach/bxe_detach when device is not found.
 */
/* ToDo: Are these necessary? */
#ifndef	ENODEV
#define	ENODEV	3
#endif

/* Return Vlaue for sp_post */
#ifndef	ESPQOVERFLOW
#define	ESPQOVERFLOW	4
#endif

/* Return Value for bxe_write_phy, bxe_read_phy. */
#ifndef	EBUSY
#define	EBUSY	5
#endif

#ifndef	PCI_EXP_DEVCTL
#define	PCI_EXP_DEVCTL			8	/* Device Control */
#endif

#ifndef	PCI_EXP_DEVCTL_PAYLOAD
#define	PCI_EXP_DEVCTL_PAYLOAD		0x00e0	/* Max_Payload_Size */
#endif

#ifndef	PCI_EXP_DEVCTL_READRQ
#define	PCI_EXP_DEVCTL_READRQ		0x7000	/* Max_Read_Request_Size */
#endif

#if defined(__i386__) || defined(__amd64__)
/* ToDo: Validate this! */
/* 128 byte L1 cache size. */
#define	BXE_RX_ALIGN_SHIFT	7
#else
/* ToDo: Validate this! */
/* 256 byte L1 cache size. */
#define	BXE_RX_ALIGN_SHIFT	8
#endif

#define	BXE_RX_ALIGN		(1 << BXE_RX_ALIGN_SHIFT)

#if __FreeBSD_version < 800054
#if defined(__i386__) || defined(__amd64__)
#define	mb()		__asm volatile("mfence" ::: "memory")
#define	wmb()		__asm volatile("sfence" ::: "memory")
#define	rmb()		__asm volatile("lfence" ::: "memory")
static __inline void
prefetch(void *x)
{
	__asm volatile("prefetcht0 %0" :: "m" (*(unsigned long *)x));
}
#else
#define	mb()
#define	rmb()
#define	wmb()
#define	prefetch()
#endif
#endif

#define	BXE_RX_ALIGN		(1 << BXE_RX_ALIGN_SHIFT)

#define	PAGE_ALIGN(addr)	(((addr) + PAGE_SIZE - 1) & (~PAGE_MASK))

/* External PHY definitions. */
#define	LED_MODE_OFF	0
#define	LED_MODE_OPER	2

#endif /*_IF_BXE_H */

