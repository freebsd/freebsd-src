/*                  
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *              
 * Modifications to support NetBSD:
 * Copyright (c) 1997 Jason R. Thorpe.  All rights reserved.
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
 * NOTE: Elements are ordered for optimal cacheline behavior, and NOT
 *	 for functional grouping.
 */
struct fxp_softc {
#if defined(__NetBSD__)
	struct device sc_dev;		/* generic device structures */
	void *sc_ih;			/* interrupt handler cookie */
	struct ethercom sc_ethercom;	/* ethernet common part */
#else
	struct arpcom arpcom;		/* per-interface network data */
	struct resource *mem;		/* resource descriptor for registers */
	struct resource *irq;		/* resource descriptor for interrupt */
	void *ih;			/* interrupt handler cookie */
	struct mtx sc_mtx;
#endif /* __NetBSD__ */
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
	int all_mcasts;			/* receive all multicasts */
	struct ifmedia sc_media;	/* media information */
	int phy_primary_addr;		/* address of primary PHY */
	int phy_primary_device;		/* device type of primary PHY */
	int phy_10Mbps_only;		/* PHY is 10Mbps-only device */
	int eeprom_size;		/* size of serial EEPROM */
	int suspended;			/* 0 = normal  1 = suspended (APM) */
	u_int32_t saved_maps[5];	/* pci data */
	u_int32_t saved_biosaddr;
	u_int8_t saved_intline;
	u_int8_t saved_cachelnsz;
	u_int8_t saved_lattimer;
};

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

/* Deal with slight differences in software interfaces. */
#if defined(__NetBSD__)
#define	sc_if			sc_ethercom.ec_if
#define	FXP_FORMAT		"%s"
#define	FXP_ARGS(sc)		(sc)->sc_dev.dv_xname
#define	FXP_INTR_TYPE		int
#define	FXP_IOCTLCMD_TYPE	u_long
#define	FXP_BPFTAP_ARG(ifp)	(ifp)->if_bpf
#define	FXP_SPLVAR(x)		int x;
#define	FXP_LOCK(sc, x)		x = splimp()
#define	FXP_UNLOCK(sc, x)	splx(x)
#else /* __FreeBSD__ */
#define	sc_if			arpcom.ac_if
#define	FXP_FORMAT		"fxp%d"
#define	FXP_ARGS(sc)		(sc)->arpcom.ac_if.if_unit
#define	FXP_INTR_TYPE		void
#define	FXP_IOCTLCMD_TYPE	u_long
#define	FXP_BPFTAP_ARG(ifp)	ifp
#define	FXP_SPLVAR(s)
#define	FXP_LOCK(_sc, x)	mtx_enter(&(_sc)->sc_mtx, MTX_DEF)
#define	FXP_UNLOCK(_sc, x)	mtx_exit(&(_sc)->sc_mtx, MTX_DEF)
#endif /* __NetBSD__ */
