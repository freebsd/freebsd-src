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

#define WICACHE			/* turn on signal strength cache code */  
#define	MAXWICACHE	10

struct wi_counters {
	u_int32_t		wi_tx_unicast_frames;
	u_int32_t		wi_tx_multicast_frames;
	u_int32_t		wi_tx_fragments;
	u_int32_t		wi_tx_unicast_octets;
	u_int32_t		wi_tx_multicast_octets;
	u_int32_t		wi_tx_deferred_xmits;
	u_int32_t		wi_tx_single_retries;
	u_int32_t		wi_tx_multi_retries;
	u_int32_t		wi_tx_retry_limit;
	u_int32_t		wi_tx_discards;
	u_int32_t		wi_rx_unicast_frames;
	u_int32_t		wi_rx_multicast_frames;
	u_int32_t		wi_rx_fragments;
	u_int32_t		wi_rx_unicast_octets;
	u_int32_t		wi_rx_multicast_octets;
	u_int32_t		wi_rx_fcs_errors;
	u_int32_t		wi_rx_discards_nobuf;
	u_int32_t		wi_tx_discards_wrong_sa;
	u_int32_t		wi_rx_WEP_cant_decrypt;
	u_int32_t		wi_rx_msg_in_msg_frags;
	u_int32_t		wi_rx_msg_in_bad_msg_frags;
};

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
struct wi_key {
	u_int16_t		wi_keylen;
	u_int8_t		wi_keydat[14];
};

struct wi_ltv_keys {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	struct wi_key		wi_keys[4];
};

struct wi_softc	{
	struct arpcom		arpcom;
	struct ifmedia		ifmedia;
	device_t		dev;
	int			wi_unit;
	struct resource *	local;
	int					local_rid;
	struct resource *	iobase;
	int					iobase_rid;
	struct resource *	irq;
	int					irq_rid;
	struct resource *	mem;
	int					mem_rid;
	bus_space_handle_t	wi_localhandle;
	bus_space_tag_t		wi_localtag;
	bus_space_handle_t	wi_bhandle;
	bus_space_tag_t		wi_btag;
	bus_space_handle_t	wi_bmemhandle;
	bus_space_tag_t		wi_bmemtag;
	void *			wi_intrhand;
	int			sc_firmware_type;
#define	WI_LUCENT	0
#define	WI_INTERSIL	1
#define	WI_SYMBOL	2
	int			sc_pri_firmware_ver;	/* Primary firmware */
	int			sc_sta_firmware_ver;	/* Station firmware */
	int			sc_enabled;
	int			wi_io_addr;
	int			wi_tx_data_id;
	int			wi_tx_mgmt_id;
	int			wi_gone;
	int			wi_if_flags;
	u_int16_t		wi_procframe;
	u_int16_t		wi_ptype;
	u_int16_t		wi_portnum;
	u_int16_t		wi_max_data_len;
	u_int16_t		wi_rts_thresh;
	u_int16_t		wi_ap_density;
	u_int16_t		wi_tx_rate;
	u_int16_t		wi_create_ibss;
	u_int16_t		wi_channel;
	u_int16_t		wi_pm_enabled;
	u_int16_t		wi_mor_enabled;
	u_int16_t		wi_max_sleep;
	u_int16_t		wi_authtype;
	u_int16_t		wi_roaming;

	char			wi_node_name[32];
	char			wi_net_name[32];
	char			wi_ibss_name[32];
	u_int8_t		wi_txbuf[1596];
	u_int8_t		wi_scanbuf[1596];
	int			wi_scanbuf_len;
	struct wi_counters	wi_stats;
	int			wi_has_wep;
	int			wi_use_wep;
	int			wi_tx_key;
	struct wi_ltv_keys	wi_keys;
#ifdef WICACHE
	int			wi_sigitems;
	struct wi_sigcache	wi_sigcache[MAXWICACHE];
	int			wi_nextitem;
#endif
	struct callout_handle	wi_stat_ch;
	struct mtx		wi_mtx;
	int			wi_nic_type;
	int			wi_bus_type;	/* Bus attachment type */
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

};

#define	WI_LOCK(_sc)
#define	WI_UNLOCK(_sc)

int wi_generic_attach(device_t);
int wi_generic_detach(device_t);
void wi_shutdown(device_t);
int wi_alloc(device_t, int);
void wi_free(device_t);
extern devclass_t wi_devclass;
