/* $NetBSD: awivar.h,v 1.12 2000/07/21 04:48:56 onoe Exp $ */
/* $FreeBSD$ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
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

/* timer values in msec */
#define	AWI_SELFTEST_TIMEOUT	5000
#define	AWI_CMD_TIMEOUT		2000
#define	AWI_LOCKOUT_TIMEOUT	50
#define	AWI_ASCAN_DURATION	100
#define	AWI_ASCAN_WAIT		3000
#define	AWI_PSCAN_DURATION	200
#define	AWI_PSCAN_WAIT		5000
#define	AWI_TRANS_TIMEOUT	2000

#define	AWI_NTXBUFS		4
#define	AWI_MAX_KEYLEN		16

enum awi_status {
	AWI_ST_INIT,
	AWI_ST_SCAN,
	AWI_ST_SETSS,
	AWI_ST_SYNC,
	AWI_ST_AUTH,
	AWI_ST_ASSOC,
	AWI_ST_RUNNING
};

struct awi_bss 
{
	TAILQ_ENTRY(awi_bss)	list;
	u_int8_t	esrc[ETHER_ADDR_LEN];
	u_int8_t	chanset;	/* channel set to use */
	u_int8_t	pattern;	/* hop pattern to use */
	u_int8_t	index;		/* index to use */
	u_int8_t	rssi;		/* strength of this beacon */
	u_int16_t	dwell_time;	/* dwell time */
	u_int8_t	timestamp[8];	/* timestamp of this bss */
	u_int8_t	bssid[ETHER_ADDR_LEN];
	u_int16_t	capinfo;
	u_int32_t	rxtime;		/* unit's local time */
	u_int16_t	interval;	/* beacon interval */
	u_int8_t	txrate;
	u_int8_t	fails;
	u_int8_t	essid[IEEE80211_NWID_LEN + 2];
};

struct awi_wep_algo {
	char		*awa_name;
	int		(*awa_ctxlen) __P((void));
	void		(*awa_setkey) __P((void *, u_char *, int));
	void		(*awa_encrypt) __P((void *, u_char *, u_char *, int));
	void		(*awa_decrypt) __P((void *, u_char *, u_char *, int));
};

struct awi_softc 
{
#ifdef __NetBSD__
	struct device 		sc_dev;
	struct ethercom		sc_ec;
	void			*sc_ih; /* interrupt handler */
#endif
#ifdef __FreeBSD__
#if __FreeBSD__ >= 4
	struct {
		char	dv_xname[64];	/*XXX*/
	}			sc_dev;
#else
	struct device		sc_dev;
#endif
	struct arpcom		sc_ec;
#endif
	struct am79c930_softc 	sc_chip;
	struct ifnet		*sc_ifp;
	int			(*sc_enable) __P((struct awi_softc *));
	void			(*sc_disable) __P((struct awi_softc *));

	struct ifmedia		sc_media;
	enum awi_status		sc_status;
	unsigned int		sc_enabled:1,
				sc_busy:1,
				sc_cansleep:1,
				sc_invalid:1,
				sc_enab_intr:1,
				sc_format_llc:1,
				sc_start_bss:1,
				sc_rawbpf:1,
				sc_no_bssid:1,
				sc_active_scan:1,
				sc_attached:1;	/* attach has succeeded */
	u_int8_t		sc_cmd_inprog;
	int			sc_sleep_cnt;

	int			sc_mgt_timer;

	TAILQ_HEAD(, awi_bss)	sc_scan;
	u_int8_t		sc_scan_cur;
	u_int8_t		sc_scan_min;
	u_int8_t		sc_scan_max;
	u_int8_t		sc_scan_set;
	struct awi_bss		sc_bss;
	u_int8_t		sc_ownssid[IEEE80211_NWID_LEN + 2];
	u_int8_t		sc_ownch;

	int			sc_rx_timer;
	u_int32_t		sc_rxdoff;
	u_int32_t		sc_rxmoff;
	struct mbuf		*sc_rxpend;

	int			sc_tx_timer;
	u_int8_t		sc_tx_rate;
	struct ifqueue		sc_mgtq;
	u_int32_t		sc_txbase;
	u_int32_t		sc_txend;
	u_int32_t		sc_txnext;
	u_int32_t		sc_txdone;

	int			sc_wep_keylen[IEEE80211_WEP_NKID]; /* keylen */
	u_int8_t		sc_wep_key[IEEE80211_WEP_NKID][AWI_MAX_KEYLEN];
	int			sc_wep_defkid;
	void			*sc_wep_ctx;	/* work area */
	struct awi_wep_algo	*sc_wep_algo;

	u_char			sc_banner[AWI_BANNER_LEN];
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
#define awi_read_bytes(sc, off, ptr, len) ((sc)->sc_chip.sc_ops->read_bytes)(&sc->sc_chip, off, ptr, len)

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

/* unalligned little endian access */
#define	LE_READ_2(p)							\
	(((u_int8_t *)(p))[0] | (((u_int8_t *)(p))[1] << 8))
#define	LE_READ_4(p)							\
	(((u_int8_t *)(p))[0] | (((u_int8_t *)(p))[1] << 8) |		\
	 (((u_int8_t *)(p))[2] << 16) | (((u_int8_t *)(p))[3] << 24))
#define	LE_WRITE_2(p, v)						\
	((((u_int8_t *)(p))[0] = ((u_int32_t)(v) & 0xff)),		\
	 (((u_int8_t *)(p))[1] = (((u_int32_t)(v) >> 8) & 0xff)))
#define	LE_WRITE_4(p, v)						\
	((((u_int8_t *)(p))[0] = ((u_int32_t)(v) & 0xff)),		\
	 (((u_int8_t *)(p))[1] = (((u_int32_t)(v) >> 8) & 0xff)),	\
	 (((u_int8_t *)(p))[2] = (((u_int32_t)(v) >> 16) & 0xff)),	\
	 (((u_int8_t *)(p))[3] = (((u_int32_t)(v) >> 24) & 0xff)))

#define	AWI_80211_RATE(rate)	(((rate) & 0x7f) * 5)

int	awi_attach __P((struct awi_softc *));
int	awi_intr __P((void *));
void	awi_reset __P((struct awi_softc *));
#ifdef __NetBSD__
int	awi_activate __P((struct device *, enum devact));
int	awi_detach __P((struct awi_softc *));
void	awi_power __P((struct awi_softc *, int));
#endif

void awi_stop __P((struct awi_softc *sc));
int awi_init __P((struct awi_softc *sc));
int awi_init_region __P((struct awi_softc *));
int awi_wicfg __P((struct ifnet *, u_long, caddr_t));

int awi_wep_setnwkey __P((struct awi_softc *, struct ieee80211_nwkey *));
int awi_wep_getnwkey __P((struct awi_softc *, struct ieee80211_nwkey *));
int awi_wep_getalgo __P((struct awi_softc *));
int awi_wep_setalgo __P((struct awi_softc *, int));
int awi_wep_setkey __P((struct awi_softc *, int, unsigned char *, int));
int awi_wep_getkey __P((struct awi_softc *, int, unsigned char *, int *));
struct mbuf *awi_wep_encrypt __P((struct awi_softc *, struct mbuf *, int));
