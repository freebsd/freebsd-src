/*                  
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *              
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:             
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.  
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
 *
 * $FreeBSD$
 */

/*
 * Misc. defintions for the Intel EtherExpress Pro/100B PCI Fast
 * Ethernet driver
 */

/*
 * Number of transmit control blocks. This determines the number
 * of transmit buffers that can be chained in the CB list.
 * This must be a power of two.
 */
#define FXP_NTXCB       128

/*
 * Size of the TxCB list.
 */
#define FXP_TXCB_SZ	(FXP_NTXCB * sizeof(struct fxp_cb_tx))

/*
 * Macro to obtain the DMA address of a virtual address in the
 * TxCB list based on the base DMA address of the TxCB list.
 */
#define FXP_TXCB_DMA_ADDR(sc, addr)					\
	(sc->fxp_desc.cbl_addr + (uintptr_t)addr -			\
	(uintptr_t)sc->fxp_desc.cbl_list)

/*
 * Number of completed TX commands at which point an interrupt
 * will be generated to garbage collect the attached buffers.
 * Must be at least one less than FXP_NTXCB, and should be
 * enough less so that the transmitter doesn't becomes idle
 * during the buffer rundown (which would reduce performance).
 */
#define FXP_CXINT_THRESH 120

/*
 * TxCB list index mask. This is used to do list wrap-around.
 */
#define FXP_TXCB_MASK   (FXP_NTXCB - 1)

/*
 * Number of receive frame area buffers. These are large so chose
 * wisely.
 */
#ifdef DEVICE_POLLING
#define FXP_NRFABUFS	192
#else
#define FXP_NRFABUFS    64
#endif

/*
 * Maximum number of seconds that the receiver can be idle before we
 * assume it's dead and attempt to reset it by reprogramming the
 * multicast filter. This is part of a work-around for a bug in the
 * NIC. See fxp_stats_update().
 */
#define FXP_MAX_RX_IDLE 15

/*
 * Default maximum time, in microseconds, that an interrupt may be delayed
 * in an attempt to coalesce interrupts.  This is only effective if the Intel 
 * microcode is loaded, and may be changed via either loader tunables or
 * sysctl.  See also the CPUSAVER_DWORD entry in rcvbundl.h.
 */
#define TUNABLE_INT_DELAY 1000

/*
 * Default number of packets that will be bundled, before an interrupt is 
 * generated.  This is only effective if the Intel microcode is loaded, and
 * may be changed via either loader tunables or sysctl.  This may not be 
 * present in all microcode revisions, see also the CPUSAVER_BUNDLE_MAX_DWORD
 * entry in rcvbundl.h.
 */
#define TUNABLE_BUNDLE_MAX 6

#if __FreeBSD_version < 500000
#define	FXP_LOCK(_sc)
#define	FXP_UNLOCK(_sc)
#define	INTR_MPSAFE		0
#define mtx_owned(a)		0
#define mtx_assert(a, b)
#define mtx_init(a, b, c, d)
#define mtx_destroy(a)
struct mtx { int dummy; };
#else
#define	FXP_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	FXP_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#endif

/*
 * Structures to handle TX and RX descriptors.
 */
struct fxp_rx {
	struct fxp_rx *rx_next;
	struct mbuf *rx_mbuf;
	bus_dmamap_t rx_map;
	u_int32_t rx_addr;
};

struct fxp_tx {
	struct fxp_tx *tx_next;
	struct fxp_cb_tx *tx_cb;
	struct mbuf *tx_mbuf;
	bus_dmamap_t tx_map;
};

struct fxp_desc_list {
	struct fxp_rx rx_list[FXP_NRFABUFS];
	struct fxp_tx tx_list[FXP_NTXCB];
	struct fxp_tx mcs_tx;
	struct fxp_rx *rx_head;
	struct fxp_rx *rx_tail;
	struct fxp_tx *tx_first;
	struct fxp_tx *tx_last;
	struct fxp_rfa *rfa_list;
	struct fxp_cb_tx *cbl_list;
	u_int32_t cbl_addr;
	bus_dma_tag_t rx_tag;
};

/*
 * NOTE: Elements are ordered for optimal cacheline behavior, and NOT
 *	 for functional grouping.
 */
struct fxp_softc {
	struct arpcom arpcom;		/* per-interface network data */
	struct resource *mem;		/* resource descriptor for registers */
	int rtp;			/* register resource type */
	int rgd;			/* register descriptor in use */
	struct resource *irq;		/* resource descriptor for interrupt */
	void *ih;			/* interrupt handler cookie */
	struct mtx sc_mtx;
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	bus_dma_tag_t fxp_mtag;		/* bus DMA tag for mbufs */
	bus_dma_tag_t fxp_stag;		/* bus DMA tag for stats */
	bus_dmamap_t fxp_smap;		/* bus DMA map for stats */
	bus_dma_tag_t cbl_tag;		/* DMA tag for the TxCB list */
	bus_dmamap_t cbl_map;		/* DMA map for the TxCB list */
	bus_dma_tag_t mcs_tag;		/* DMA tag for the multicast setup */
	bus_dmamap_t mcs_map;		/* DMA map for the multicast setup */
	bus_dmamap_t spare_map;		/* spare DMA map */
	struct fxp_desc_list fxp_desc;	/* descriptors management struct */
	int tx_queued;			/* # of active TxCB's */
	int need_mcsetup;		/* multicast filter needs programming */
	struct fxp_stats *fxp_stats;	/* Pointer to interface stats */
	u_int32_t stats_addr;		/* DMA address of the stats structure */
	int rx_idle_secs;		/* # of seconds RX has been idle */
	struct callout stat_ch;		/* stat callout */
	struct fxp_cb_mcs *mcsp;	/* Pointer to mcast setup descriptor */
	u_int32_t mcs_addr;		/* DMA address of the multicast cmd */
	struct ifmedia sc_media;	/* media information */
	device_t miibus;
	device_t dev;
	struct sysctl_ctx_list sysctl_ctx;
	struct sysctl_oid *sysctl_tree;
	int tunable_int_delay;		/* interrupt delay value for ucode */
	int tunable_bundle_max;		/* max # frames per interrupt (ucode) */
	int eeprom_size;		/* size of serial EEPROM */
	int suspended;			/* 0 = normal  1 = suspended or dead */
	int cu_resume_bug;
	int revision;
	int flags;
	u_int32_t saved_maps[5];	/* pci data */
	u_int32_t saved_biosaddr;
	u_int8_t saved_intline;
	u_int8_t saved_cachelnsz;
	u_int8_t saved_lattimer;
	u_int8_t rfa_size;
        u_int32_t tx_cmd;
};

#define FXP_FLAG_MWI_ENABLE	0x0001	/* MWI enable */
#define FXP_FLAG_READ_ALIGN	0x0002	/* align read access with cacheline */
#define FXP_FLAG_WRITE_ALIGN	0x0004	/* end write on cacheline */
#define FXP_FLAG_EXT_TXCB	0x0008	/* enable use of extended TXCB */
#define FXP_FLAG_SERIAL_MEDIA	0x0010	/* 10Mbps serial interface */
#define FXP_FLAG_LONG_PKT_EN	0x0020	/* enable long packet reception */
#define FXP_FLAG_ALL_MCAST	0x0040	/* accept all multicast frames */
#define FXP_FLAG_CU_RESUME_BUG	0x0080	/* requires workaround for CU_RESUME */
#define FXP_FLAG_UCODE		0x0100	/* ucode is loaded */
#define FXP_FLAG_DEFERRED_RNR	0x0200	/* DEVICE_POLLING deferred RNR */
#define FXP_FLAG_EXT_RFA	0x0400	/* extended RFDs for csum offload */

/* Macros to ease CSR access. */
#define	CSR_READ_1(sc, reg)						\
	bus_space_read_1((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_READ_2(sc, reg)						\
	bus_space_read_2((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_READ_4(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_WRITE_1(sc, reg, val)					\
	bus_space_write_1((sc)->sc_st, (sc)->sc_sh, (reg), (val))
#define	CSR_WRITE_2(sc, reg, val)					\
	bus_space_write_2((sc)->sc_st, (sc)->sc_sh, (reg), (val))
#define	CSR_WRITE_4(sc, reg, val)					\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))

#define	sc_if			arpcom.ac_if
