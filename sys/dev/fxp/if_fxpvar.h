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
#define mtx_init(a, b, c, d)
#define mtx_destroy(a)
struct mtx { int dummy; };
#else
#define	FXP_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	FXP_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#endif

#ifdef __alpha__
#undef vtophys
#define vtophys(va)	alpha_XXX_dmamap((vm_offset_t)(va))
#endif /* __alpha__ */

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
	struct mbuf *rfa_headm;		/* first mbuf in receive frame area */
	struct mbuf *rfa_tailm;		/* last mbuf in receive frame area */
	struct fxp_cb_tx *cbl_first;	/* first active TxCB in list */
	int tx_queued;			/* # of active TxCB's */
	int need_mcsetup;		/* multicast filter needs programming */
	struct fxp_cb_tx *cbl_last;	/* last active TxCB in list */
	struct fxp_stats *fxp_stats;	/* Pointer to interface stats */
	int rx_idle_secs;		/* # of seconds RX has been idle */
	struct callout_handle stat_ch;	/* Handle for canceling our stat timeout */
	struct fxp_cb_tx *cbl_base;	/* base of TxCB list */
	struct fxp_cb_mcs *mcsp;	/* Pointer to mcast setup descriptor */
	struct ifmedia sc_media;	/* media information */
	device_t miibus;
	device_t dev;
	struct sysctl_ctx_list sysctl_ctx;
	struct sysctl_oid *sysctl_tree;
	int tunable_int_delay;		/* interrupt delay value for ucode */
	int tunable_bundle_max;		/* max # frames per interrupt (ucode) */
	int eeprom_size;		/* size of serial EEPROM */
	int suspended;			/* 0 = normal  1 = suspended (APM) */
	int cu_resume_bug;
	int revision;
	int flags;
	u_int32_t saved_maps[5];	/* pci data */
	u_int32_t saved_biosaddr;
	u_int8_t saved_intline;
	u_int8_t saved_cachelnsz;
	u_int8_t saved_lattimer;
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

#define	FXP_UNIT(_sc)		(_sc)->arpcom.ac_if.if_unit
