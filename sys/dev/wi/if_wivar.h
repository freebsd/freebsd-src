/*
 * Copyright (c) 2002
 *	M Warner Losh <imp@freebsd.org>.  All rights reserved.
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#if 0
#define WICACHE			/* turn on signal strength cache code */  
#define	MAXWICACHE	10
#endif

/*
 * Encryption controls. We can enable or disable encryption as
 * well as specify up to 4 encryption keys. We can also specify
 * which of the four keys will be used for transmit encryption.
 */
#define WI_RID_ENCRYPTION	0xFC20
#define WI_RID_AUTHTYPE		0xFC21
#define WI_RID_DEFLT_CRYPT_KEYS	0xFCB0
#define WI_RID_TX_CRYPT_KEY	0xFCB1
#define WI_RID_WEP_AVAIL	0xFD4F
#define WI_RID_P2_TX_CRYPT_KEY	0xFC23
#define WI_RID_P2_CRYPT_KEY0	0xFC24
#define WI_RID_P2_CRYPT_KEY1	0xFC25
#define WI_RID_MICROWAVE_OVEN	0xFC25
#define WI_RID_P2_CRYPT_KEY2	0xFC26
#define WI_RID_P2_CRYPT_KEY3	0xFC27
#define WI_RID_P2_ENCRYPTION	0xFC28
#define WI_RID_ROAMING_MODE	0xFC2D
#define WI_RID_CUR_TX_RATE	0xFD44 /* current TX rate */

struct wi_softc	{
	struct ieee80211com	sc_ic;
	int			(*sc_newstate)(struct ieee80211com *,
					enum ieee80211_state, int);
	device_t		sc_dev;
#if __FreeBSD_version >= 500000
	struct mtx		sc_mtx;
#endif
	int			sc_unit;
	int			wi_gone;
	int			sc_enabled;
	int			sc_reset;
	int			sc_firmware_type;
#define WI_NOTYPE	0
#define	WI_LUCENT	1
#define	WI_INTERSIL	2
#define	WI_SYMBOL	3
	int			sc_pri_firmware_ver;	/* Primary firmware */
	int			sc_sta_firmware_ver;	/* Station firmware */

	int			wi_bus_type;	/* Bus attachment type */
	struct resource *	local;
	int			local_rid;
	struct resource *	iobase;
	int			iobase_rid;
	struct resource *	irq;
	int			irq_rid;
	struct resource *	mem;
	int			mem_rid;
	bus_space_handle_t	wi_localhandle;
	bus_space_tag_t		wi_localtag;
	bus_space_handle_t	wi_bhandle;
	bus_space_tag_t		wi_btag;
	bus_space_handle_t	wi_bmemhandle;
	bus_space_tag_t		wi_bmemtag;
	void *			wi_intrhand;
	int			wi_io_addr;

	struct bpf_if		*sc_drvbpf;
	int			sc_flags;
	int			sc_if_flags;
	int			sc_bap_id;
	int			sc_bap_off;

	u_int16_t		sc_procframe;
	u_int16_t		sc_portnum;

	/* RSSI interpretation */
	u_int16_t		sc_min_rssi;	/* clamp sc_min_rssi < RSSI */
	u_int16_t		sc_max_rssi;	/* clamp RSSI < sc_max_rssi */
	u_int16_t		sc_dbm_offset;	/* dBm ~ RSSI - sc_dbm_offset */

	u_int16_t		sc_max_datalen;
	u_int16_t		sc_system_scale;
	u_int16_t		sc_cnfauthmode;
	u_int16_t		sc_roaming_mode;
	u_int16_t		sc_microwave_oven;
	u_int16_t		sc_authtype;

	int			sc_nodelen;
	char			sc_nodename[IEEE80211_NWID_LEN];
	char			sc_net_name[IEEE80211_NWID_LEN];

	int			sc_buflen;		/* TX buffer size */
	int			sc_ntxbuf;
#define	WI_NTXBUF	3
	struct {
		int		d_fid;
		int		d_len;
	}			sc_txd[WI_NTXBUF];	/* TX buffers */
	int			sc_txnext;		/* index of next TX */
	int			sc_txcur;		/* index of current TX*/
	int			sc_tx_timer;
	int			sc_scan_timer;
	int			sc_syn_timer;

	struct wi_counters	sc_stats;
	u_int16_t		sc_ibss_port;

#define WI_MAXAPINFO		30
	struct wi_apinfo	sc_aps[WI_MAXAPINFO];
	int			sc_naps;

	struct {
		u_int16_t               wi_sleep;
		u_int16_t               wi_delaysupp;
		u_int16_t               wi_txsupp;
		u_int16_t               wi_monitor;
		u_int16_t               wi_ledtest;
		u_int16_t               wi_ledtest_param0;
		u_int16_t               wi_ledtest_param1;
		u_int16_t               wi_conttx;
		u_int16_t               wi_conttx_param0;
		u_int16_t               wi_contrx;
		u_int16_t               wi_sigstate;
		u_int16_t               wi_sigstate_param0;
		u_int16_t               wi_confbits;
		u_int16_t               wi_confbits_param0;
	} wi_debug;

	int			sc_false_syns;

	u_int16_t		sc_txbuf[IEEE80211_MAX_LEN/2];

	union {
		struct wi_tx_radiotap_header th;
		u_int8_t	pad[64];
	} u_tx_rt;
	union {
		struct wi_rx_radiotap_header th;
		u_int8_t	pad[64];
	} u_rx_rt;
};
#define	sc_if			sc_ic.ic_if
#define	sc_tx_th		u_tx_rt.th
#define	sc_rx_th		u_rx_rt.th

/* maximum consecutive false change-of-BSSID indications */
#define	WI_MAX_FALSE_SYNS		10	

#define	WI_SCAN_INQWAIT			3	/* wait sec before inquire */
#define	WI_SCAN_WAIT			5	/* maximum scan wait */

#define	WI_FLAGS_ATTACHED		0x0001
#define	WI_FLAGS_INITIALIZED		0x0002
#define	WI_FLAGS_OUTRANGE		0x0004
#define	WI_FLAGS_HAS_MOR		0x0010
#define	WI_FLAGS_HAS_ROAMING		0x0020
#define	WI_FLAGS_HAS_DIVERSITY		0x0040
#define	WI_FLAGS_HAS_SYSSCALE		0x0080
#define	WI_FLAGS_BUG_AUTOINC		0x0100
#define	WI_FLAGS_HAS_FRAGTHR		0x0200
#define	WI_FLAGS_HAS_DBMADJUST		0x0400

struct wi_card_ident {
	u_int16_t	card_id;
	char		*card_name;
	u_int8_t	firm_type;
};

#define	WI_PRISM_MIN_RSSI	0x1b
#define	WI_PRISM_MAX_RSSI	0x9a
#define	WI_PRISM_DBM_OFFSET	100 /* XXX */

#define	WI_LUCENT_MIN_RSSI	47
#define	WI_LUCENT_MAX_RSSI	138
#define	WI_LUCENT_DBM_OFFSET	149

#define	WI_RSSI_TO_DBM(sc, rssi) (MIN((sc)->sc_max_rssi, \
    MAX((sc)->sc_min_rssi, (rssi))) - (sc)->sc_dbm_offset)

#if __FreeBSD_version < 500000
/*
 * Various compat hacks/kludges
 */
#define le16toh(x) (x)
#define htole16(x) (x)
#define ifaddr_byindex(idx) ifnet_addrs[(idx) - 1];
#define	WI_LOCK_DECL()		int s
#define	WI_LOCK(_sc)		s = splimp()
#define	WI_UNLOCK(_sc)		splx(s)
#else
#define	WI_LOCK_DECL()
#define	WI_LOCK(_sc) 		mtx_lock(&(_sc)->sc_mtx)
#define	WI_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#endif

int	wi_attach(device_t);
int	wi_detach(device_t);
void	wi_shutdown(device_t);
int	wi_alloc(device_t, int);
void	wi_free(device_t);
extern devclass_t wi_devclass;
void	wi_init(void *);
void	wi_intr(void *);
int	wi_mgmt_xmit(struct wi_softc *, caddr_t, int);
void	wi_stop(struct ifnet *, int);
int	wi_symbol_load_firm(struct wi_softc *, const void *, int, const void *, int);
