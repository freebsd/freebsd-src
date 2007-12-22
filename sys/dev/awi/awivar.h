/* $NetBSD: awivar.h,v 1.20 2004/01/15 09:39:15 onoe Exp $ */
/* $FreeBSD$ */

/*-
 * Copyright (c) 1999,2000,2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_IC_AWIVAR_H
#define	_DEV_IC_AWIVAR_H

/* timer values in msec */
#define	AWI_SELFTEST_TIMEOUT	5000
#define	AWI_CMD_TIMEOUT		2000
#define	AWI_LOCKOUT_TIMEOUT	50
#define	AWI_ASCAN_DURATION	100
#define	AWI_ASCAN_WAIT		3000
#define	AWI_PSCAN_DURATION	200
#define	AWI_PSCAN_WAIT		5000
#define	AWI_TRANS_TIMEOUT	5000

#define	AWI_NTXBUFS		4

enum awi_sub_state {
	AWI_ST_NONE,
	AWI_ST_SCAN_INIT,
	AWI_ST_SCAN_SETMIB,
	AWI_ST_SCAN_SCCMD,
	AWI_ST_SUB_INIT,
	AWI_ST_SUB_SETSS,
	AWI_ST_SUB_SYNC
};

#define	AWI_WAIT		0		/* must wait for completion */
#define	AWI_NOWAIT		1		/* do not wait */

struct awi_chanset {
	u_int8_t	cs_type;
	u_int8_t	cs_region;
	u_int8_t	cs_min;
	u_int8_t	cs_max;
	u_int8_t	cs_def;
};

struct awi_softc {
#ifdef __NetBSD__
	struct device		sc_dev;
	void			(*sc_power)(struct awi_softc *, int);
#endif
#ifdef __FreeBSD__
	struct arpcom		sc_arp;
	device_t		sc_dev;
#endif
	struct am79c930_softc 	sc_chip;
	struct ieee80211com	sc_ic;
	u_char			sc_banner[AWI_BANNER_LEN];
	int			(*sc_enable)(struct awi_softc *);
	void			(*sc_disable)(struct awi_softc *);

	int			(*sc_newstate)(struct ieee80211com *,
				    enum ieee80211_state, int);
	void			(*sc_recv_mgmt)(struct ieee80211com *,
				    struct mbuf *, struct ieee80211_node *,
				    int, int, int, u_int32_t);
	int			(*sc_send_mgmt)(struct ieee80211com *,
				    struct ieee80211_node *, int, int);

	void			*sc_sdhook;	/* shutdown hook */
	void			*sc_powerhook;	/* power management hook */
	unsigned int		sc_attached:1,
				sc_enabled:1,
				sc_busy:1,
				sc_cansleep:1,
				sc_enab_intr:1,
				sc_adhoc_ap:1,
				sc_invalid:1;
	enum ieee80211_state	sc_nstate;
	enum awi_sub_state	sc_substate;
	int			sc_sleep_cnt;
	u_int8_t		sc_cmd_inprog;
	u_int8_t		sc_cur_chan;

	int			sc_rx_timer;
	u_int32_t		sc_rxdoff;
	u_int32_t		sc_rxmoff;
	struct mbuf		*sc_rxpend;

	int			sc_tx_timer;
	u_int32_t		sc_txbase;
	u_int32_t		sc_txend;
	u_int32_t		sc_txnext;
	u_int32_t		sc_txdone;

	struct awi_mib_local	sc_mib_local;
	struct awi_mib_addr	sc_mib_addr;
	struct awi_mib_mac	sc_mib_mac;
	struct awi_mib_stat	sc_mib_stat;
	struct awi_mib_mgt	sc_mib_mgt;
	struct awi_mib_phy	sc_mib_phy;
};

#define awi_read_1(sc, off) ((sc)->sc_chip.sc_ops->read_1)(&sc->sc_chip, off)
#define awi_read_2(sc, off) ((sc)->sc_chip.sc_ops->read_2)(&sc->sc_chip, off)
#define awi_read_4(sc, off) ((sc)->sc_chip.sc_ops->read_4)(&sc->sc_chip, off)
#define awi_read_bytes(sc, off, ptr, len) \
	((sc)->sc_chip.sc_ops->read_bytes)(&sc->sc_chip, off, ptr, len)

#define awi_write_1(sc, off, val) \
	((sc)->sc_chip.sc_ops->write_1)(&sc->sc_chip, off, val)
#define awi_write_2(sc, off, val) \
	((sc)->sc_chip.sc_ops->write_2)(&sc->sc_chip, off, val)
#define awi_write_4(sc, off, val) \
	((sc)->sc_chip.sc_ops->write_4)(&sc->sc_chip, off, val)
#define awi_write_bytes(sc, off, ptr, len) \
	((sc)->sc_chip.sc_ops->write_bytes)(&sc->sc_chip, off, ptr, len)

#define awi_drvstate(sc, state) \
	awi_write_1(sc, AWI_DRIVERSTATE, \
	    ((state) | AWI_DRV_AUTORXLED|AWI_DRV_AUTOTXLED))

int	awi_attach(struct awi_softc *);
int	awi_detach(struct awi_softc *);
#ifdef __NetBSD__
int	awi_activate(struct device *, enum devact);
void	awi_power(int, void *);
#endif
void	awi_shutdown(void *);
int	awi_intr(void *);

#endif /* _DEV_IC_AWIVAR_H */
