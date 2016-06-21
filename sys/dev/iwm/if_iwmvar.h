/*	$OpenBSD: if_iwmvar.h,v 1.7 2015/03/02 13:51:10 jsg Exp $	*/
/*	$FreeBSD$ */

/*
 * Copyright (c) 2014 genua mbh <info@genua.de>
 * Copyright (c) 2014 Fixup Software Ltd.
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

/*-
 * Based on BSD-licensed source modules in the Linux iwlwifi driver,
 * which were used as the reference documentation for this implementation.
 *
 * Driver version we are currently based off of is
 * Linux 3.14.3 (tag id a2df521e42b1d9a23f620ac79dbfe8655a8391dd)
 *
 ***********************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2013 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2013 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2007-2010 Damien Bergamini <damien.bergamini@free.fr>
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

struct iwm_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint64_t	wr_tsft;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	int8_t		wr_dbm_antsignal;
	int8_t		wr_dbm_antnoise;
} __packed;

#define IWM_RX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_TSFT) |				\
	 (1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL) |			\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTNOISE))

struct iwm_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed;

#define IWM_TX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))


#define IWM_UCODE_SECT_MAX 6
#define IWM_FWDMASEGSZ (192*1024)
/* sanity check value */
#define IWM_FWMAXSIZE (2*1024*1024)

/*
 * fw_status is used to determine if we've already parsed the firmware file
 *
 * In addition to the following, status < 0 ==> -error
 */
#define IWM_FW_STATUS_NONE		0
#define IWM_FW_STATUS_INPROGRESS	1
#define IWM_FW_STATUS_DONE		2

enum iwm_ucode_type {
	IWM_UCODE_TYPE_INIT,
	IWM_UCODE_TYPE_REGULAR,
	IWM_UCODE_TYPE_WOW,
	IWM_UCODE_TYPE_MAX
};

struct iwm_fw_info {
	const struct firmware *fw_fp;
	int fw_status;

	struct iwm_fw_sects {
		struct iwm_fw_onesect {
			const void *fws_data;
			uint32_t fws_len;
			uint32_t fws_devoff;
		} fw_sect[IWM_UCODE_SECT_MAX];
		size_t fw_totlen;
		int fw_count;
	} fw_sects[IWM_UCODE_TYPE_MAX];
};

struct iwm_nvm_data {
	int n_hw_addrs;
	uint8_t hw_addr[IEEE80211_ADDR_LEN];

	uint8_t calib_version;
	uint16_t calib_voltage;

	uint16_t raw_temperature;
	uint16_t kelvin_temperature;
	uint16_t kelvin_voltage;
	uint16_t xtal_calib[2];

	int sku_cap_band_24GHz_enable;
	int sku_cap_band_52GHz_enable;
	int sku_cap_11n_enable;
	int sku_cap_amt_enable;
	int sku_cap_ipan_enable;

	uint8_t radio_cfg_type;
	uint8_t radio_cfg_step;
	uint8_t radio_cfg_dash;
	uint8_t radio_cfg_pnum;
	uint8_t valid_tx_ant, valid_rx_ant;
#define IWM_NUM_CHANNELS	39

	uint16_t nvm_ch_flags[IWM_NUM_CHANNELS];

	uint16_t nvm_version;
	uint8_t max_tx_pwr_half_dbm;
};

/* max bufs per tfd the driver will use */
#define IWM_MAX_CMD_TBS_PER_TFD 2

struct iwm_rx_packet;
struct iwm_host_cmd {
	const void *data[IWM_MAX_CMD_TBS_PER_TFD];
	struct iwm_rx_packet *resp_pkt;
	unsigned long _rx_page_addr;
	uint32_t _rx_page_order;
	int handler_status;

	uint32_t flags;
	uint16_t len[IWM_MAX_CMD_TBS_PER_TFD];
	uint8_t dataflags[IWM_MAX_CMD_TBS_PER_TFD];
	uint8_t id;
};

/*
 * DMA glue is from iwn
 */

typedef caddr_t iwm_caddr_t;
typedef void *iwm_hookarg_t;

struct iwm_dma_info {
	bus_dma_tag_t		tag;
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		paddr;
	void 			*vaddr;
	bus_size_t		size;
};

#define IWM_TX_RING_COUNT	256
#define IWM_TX_RING_LOMARK	192
#define IWM_TX_RING_HIMARK	224

struct iwm_tx_data {
	bus_dmamap_t		map;
	bus_addr_t		cmd_paddr;
	bus_addr_t		scratch_paddr;
	struct mbuf		*m;
	struct iwm_node 	*in;
	int			done;
};

struct iwm_tx_ring {
	struct iwm_dma_info	desc_dma;
	struct iwm_dma_info	cmd_dma;
	struct iwm_tfd		*desc;
	struct iwm_device_cmd	*cmd;
	bus_dma_tag_t		data_dmat;
	struct iwm_tx_data	data[IWM_TX_RING_COUNT];
	int			qid;
	int			queued;
	int			cur;
};

#define IWM_RX_RING_COUNT	256
#define IWM_RBUF_COUNT		(IWM_RX_RING_COUNT + 32)
/* Linux driver optionally uses 8k buffer */
#define IWM_RBUF_SIZE		4096

#define	IWM_MAX_SCATTER		20

struct iwm_softc;
struct iwm_rbuf {
	struct iwm_softc	*sc;
	void			*vaddr;
	bus_addr_t		paddr;
};

struct iwm_rx_data {
	struct mbuf	*m;
	bus_dmamap_t	map;
	int		wantresp;
};

struct iwm_rx_ring {
	struct iwm_dma_info	desc_dma;
	struct iwm_dma_info	stat_dma;
	struct iwm_dma_info	buf_dma;
	uint32_t		*desc;
	struct iwm_rb_status	*stat;
	struct iwm_rx_data	data[IWM_RX_RING_COUNT];
	bus_dmamap_t		spare_map;	/* for iwm_rx_addbuf() */
	bus_dma_tag_t           data_dmat;
	int			cur;
};

struct iwm_ucode_status {
	uint32_t uc_error_event_table;
	uint32_t uc_log_event_table;

	int uc_ok;
	int uc_intr;
};

#define IWM_CMD_RESP_MAX PAGE_SIZE

#define IWM_OTP_LOW_IMAGE_SIZE 2048

#define IWM_MVM_TE_SESSION_PROTECTION_MAX_TIME_MS 500
#define IWM_MVM_TE_SESSION_PROTECTION_MIN_TIME_MS 400

/*
 * Command headers are in iwl-trans.h, which is full of all
 * kinds of other junk, so we just replicate the structures here.
 * First the software bits:
 */
enum IWM_CMD_MODE {
	IWM_CMD_SYNC		= 0,
	IWM_CMD_ASYNC		= (1 << 0),
	IWM_CMD_WANT_SKB	= (1 << 1),
	IWM_CMD_SEND_IN_RFKILL	= (1 << 2),
};
enum iwm_hcmd_dataflag {
	IWM_HCMD_DFL_NOCOPY     = (1 << 0),
	IWM_HCMD_DFL_DUP        = (1 << 1),
};

/*
 * iwlwifi/iwl-phy-db
 */

#define IWM_NUM_PAPD_CH_GROUPS	4
#define IWM_NUM_TXP_CH_GROUPS	9

struct iwm_phy_db_entry {
	uint16_t size;
	uint8_t *data;
};

struct iwm_phy_db {
	struct iwm_phy_db_entry	cfg;
	struct iwm_phy_db_entry	calib_nch;
	struct iwm_phy_db_entry	calib_ch_group_papd[IWM_NUM_PAPD_CH_GROUPS];
	struct iwm_phy_db_entry	calib_ch_group_txp[IWM_NUM_TXP_CH_GROUPS];
};

struct iwm_int_sta {
	uint32_t sta_id;
	uint32_t tfd_queue_msk;
};

struct iwm_mvm_phy_ctxt {
	uint16_t id;
	uint16_t color;
	uint32_t ref;
	struct ieee80211_channel *channel;
};

struct iwm_bf_data {
	int bf_enabled;		/* filtering	*/
	int ba_enabled;		/* abort	*/
	int ave_beacon_signal;
	int last_cqm_event;
};

struct iwm_vap {
	struct ieee80211vap	iv_vap;
	int			is_uploaded;

	int			(*iv_newstate)(struct ieee80211vap *,
				    enum ieee80211_state, int);
};
#define IWM_VAP(_vap)		((struct iwm_vap *)(_vap))

struct iwm_node {
	struct ieee80211_node	in_ni;
	struct iwm_mvm_phy_ctxt	*in_phyctxt;

	/* status "bits" */
	int			in_assoc;

	struct iwm_lq_cmd	in_lq;

	uint8_t			in_ridx[IEEE80211_RATE_MAXSIZE];
};
#define IWM_NODE(_ni)		((struct iwm_node *)(_ni))

#define IWM_STATION_ID 0

#define	IWM_DEFAULT_MACID	0
#define	IWM_DEFAULT_COLOR	0
#define	IWM_DEFAULT_TSFID	0

#define IWM_ICT_SIZE		4096
#define IWM_ICT_COUNT		(IWM_ICT_SIZE / sizeof (uint32_t))
#define IWM_ICT_PADDR_SHIFT	12

struct iwm_softc {
	device_t		sc_dev;
	uint32_t		sc_debug;

	struct mtx		sc_mtx;
	struct mbufq		sc_snd;
	struct ieee80211com	sc_ic;

	int			sc_flags;
#define IWM_FLAG_USE_ICT	(1 << 0)
#define IWM_FLAG_HW_INITED	(1 << 1)
#define IWM_FLAG_STOPPED	(1 << 2)
#define IWM_FLAG_RFKILL		(1 << 3)
#define IWM_FLAG_BUSY		(1 << 4)
#define	IWM_FLAG_DORESUME	(1 << 5)

	struct intr_config_hook sc_preinit_hook;
	struct callout		sc_watchdog_to;
	struct callout		sc_led_blink_to;

	struct task		init_task;

	struct resource		*sc_irq;
	struct resource		*sc_mem;
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	bus_size_t		sc_sz;
	bus_dma_tag_t		sc_dmat;
	void			*sc_ih;

	/* TX scheduler rings. */
	struct iwm_dma_info	sched_dma;
	uint32_t		sched_base;

	/* TX/RX rings. */
	struct iwm_tx_ring	txq[IWM_MVM_MAX_QUEUES];
	struct iwm_rx_ring	rxq;
	int			qfullmsk;

	int			sc_sf_state;

	/* ICT table. */
	struct iwm_dma_info	ict_dma;
	int			ict_cur;

	int			sc_hw_rev;
	int			sc_hw_id;

	struct iwm_dma_info	kw_dma;
	struct iwm_dma_info	fw_dma;

	int			sc_fw_chunk_done;
	int			sc_init_complete;

	struct iwm_ucode_status	sc_uc;
	enum iwm_ucode_type	sc_uc_current;
	int			sc_fwver;

	int			sc_capaflags;
	int			sc_capa_max_probe_len;

	int			sc_intmask;

	/*
	 * So why do we need a separate stopped flag and a generation?
	 * the former protects the device from issuing commands when it's
	 * stopped (duh).  The latter protects against race from a very
	 * fast stop/unstop cycle where threads waiting for responses do
	 * not have a chance to run in between.  Notably: we want to stop
	 * the device from interrupt context when it craps out, so we
	 * don't have the luxury of waiting for quiescense.
	 */
	int			sc_generation;

	const char		*sc_fwname;
	bus_size_t		sc_fwdmasegsz;
	struct iwm_fw_info	sc_fw;
	int			sc_fw_phy_config;
	struct iwm_tlv_calib_ctrl sc_default_calib[IWM_UCODE_TYPE_MAX];

	struct iwm_nvm_data	sc_nvm;
	struct iwm_phy_db	sc_phy_db;

	struct iwm_bf_data	sc_bf;

	int			sc_tx_timer;

	struct iwm_scan_cmd	*sc_scan_cmd;
	size_t			sc_scan_cmd_len;
	int			sc_scan_last_antenna;
	int			sc_scanband;

	int			sc_fixed_ridx;

	int			sc_staid;
	int			sc_nodecolor;

	uint8_t			sc_cmd_resp[IWM_CMD_RESP_MAX];
	int			sc_wantresp;

	struct taskqueue	*sc_tq;
	struct task		sc_es_task;

	struct iwm_rx_phy_info	sc_last_phy_info;
	int			sc_ampdu_ref;

	struct iwm_int_sta	sc_aux_sta;

	/* phy contexts.  we only use the first one */
	struct iwm_mvm_phy_ctxt	sc_phyctxt[IWM_NUM_PHY_CTX];

	struct iwm_notif_statistics sc_stats;
	int			sc_noise;

	int			host_interrupt_operation_mode;

	caddr_t			sc_drvbpf;

	struct iwm_rx_radiotap_header sc_rxtap;
	struct iwm_tx_radiotap_header sc_txtap;

	int			sc_max_rssi;
};

#define IWM_LOCK_INIT(_sc) \
	mtx_init(&(_sc)->sc_mtx, device_get_nameunit((_sc)->sc_dev), \
	    MTX_NETWORK_LOCK, MTX_DEF);
#define	IWM_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	IWM_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define IWM_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_mtx)
