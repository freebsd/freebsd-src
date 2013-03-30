/*-
 * Copyright (c) 2011 Chelsio Communications, Inc.
 * All rights reserved.
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
 *
 * $FreeBSD$
 *
 */

#ifndef __CHELSIO_COMMON_H
#define __CHELSIO_COMMON_H

#include "t4_hw.h"


enum {
	MAX_NPORTS     = 4,     /* max # of ports */
	SERNUM_LEN     = 24,    /* Serial # length */
	EC_LEN         = 16,    /* E/C length */
	ID_LEN         = 16,    /* ID length */
	PN_LEN         = 16,    /* Part Number length */
	MACADDR_LEN    = 12,    /* MAC Address length */
};

enum { MEM_EDC0, MEM_EDC1, MEM_MC, MEM_MC0 = MEM_MC, MEM_MC1 };

enum {
	MEMWIN0_APERTURE = 2048,
	MEMWIN0_BASE     = 0x1b800,
	MEMWIN1_APERTURE = 32768,
	MEMWIN1_BASE     = 0x28000,

	MEMWIN2_APERTURE_T4 = 65536,
	MEMWIN2_BASE_T4     = 0x30000,

	MEMWIN2_APERTURE_T5 = 128 * 1024,
	MEMWIN2_BASE_T5     = 0x60000,
};

enum dev_master { MASTER_CANT, MASTER_MAY, MASTER_MUST };

enum dev_state { DEV_STATE_UNINIT, DEV_STATE_INIT, DEV_STATE_ERR };

enum {
	PAUSE_RX      = 1 << 0,
	PAUSE_TX      = 1 << 1,
	PAUSE_AUTONEG = 1 << 2
};

#define FW_VERSION_MAJOR_T4 1
#define FW_VERSION_MINOR_T4 8
#define FW_VERSION_MICRO_T4 4
#define FW_VERSION_BUILD_T4 0

#define FW_VERSION_MAJOR_T5 0
#define FW_VERSION_MINOR_T5 5
#define FW_VERSION_MICRO_T5 18
#define FW_VERSION_BUILD_T5 0

struct memwin {
	uint32_t base;
	uint32_t aperture;
};

struct port_stats {
	u64 tx_octets;            /* total # of octets in good frames */
	u64 tx_frames;            /* all good frames */
	u64 tx_bcast_frames;      /* all broadcast frames */
	u64 tx_mcast_frames;      /* all multicast frames */
	u64 tx_ucast_frames;      /* all unicast frames */
	u64 tx_error_frames;      /* all error frames */

	u64 tx_frames_64;         /* # of Tx frames in a particular range */
	u64 tx_frames_65_127;
	u64 tx_frames_128_255;
	u64 tx_frames_256_511;
	u64 tx_frames_512_1023;
	u64 tx_frames_1024_1518;
	u64 tx_frames_1519_max;

	u64 tx_drop;              /* # of dropped Tx frames */
	u64 tx_pause;             /* # of transmitted pause frames */
	u64 tx_ppp0;              /* # of transmitted PPP prio 0 frames */
	u64 tx_ppp1;              /* # of transmitted PPP prio 1 frames */
	u64 tx_ppp2;              /* # of transmitted PPP prio 2 frames */
	u64 tx_ppp3;              /* # of transmitted PPP prio 3 frames */
	u64 tx_ppp4;              /* # of transmitted PPP prio 4 frames */
	u64 tx_ppp5;              /* # of transmitted PPP prio 5 frames */
	u64 tx_ppp6;              /* # of transmitted PPP prio 6 frames */
	u64 tx_ppp7;              /* # of transmitted PPP prio 7 frames */

	u64 rx_octets;            /* total # of octets in good frames */
	u64 rx_frames;            /* all good frames */
	u64 rx_bcast_frames;      /* all broadcast frames */
	u64 rx_mcast_frames;      /* all multicast frames */
	u64 rx_ucast_frames;      /* all unicast frames */
	u64 rx_too_long;          /* # of frames exceeding MTU */
	u64 rx_jabber;            /* # of jabber frames */
	u64 rx_fcs_err;           /* # of received frames with bad FCS */
	u64 rx_len_err;           /* # of received frames with length error */
	u64 rx_symbol_err;        /* symbol errors */
	u64 rx_runt;              /* # of short frames */

	u64 rx_frames_64;         /* # of Rx frames in a particular range */
	u64 rx_frames_65_127;
	u64 rx_frames_128_255;
	u64 rx_frames_256_511;
	u64 rx_frames_512_1023;
	u64 rx_frames_1024_1518;
	u64 rx_frames_1519_max;

	u64 rx_pause;             /* # of received pause frames */
	u64 rx_ppp0;              /* # of received PPP prio 0 frames */
	u64 rx_ppp1;              /* # of received PPP prio 1 frames */
	u64 rx_ppp2;              /* # of received PPP prio 2 frames */
	u64 rx_ppp3;              /* # of received PPP prio 3 frames */
	u64 rx_ppp4;              /* # of received PPP prio 4 frames */
	u64 rx_ppp5;              /* # of received PPP prio 5 frames */
	u64 rx_ppp6;              /* # of received PPP prio 6 frames */
	u64 rx_ppp7;              /* # of received PPP prio 7 frames */

	u64 rx_ovflow0;           /* drops due to buffer-group 0 overflows */
	u64 rx_ovflow1;           /* drops due to buffer-group 1 overflows */
	u64 rx_ovflow2;           /* drops due to buffer-group 2 overflows */
	u64 rx_ovflow3;           /* drops due to buffer-group 3 overflows */
	u64 rx_trunc0;            /* buffer-group 0 truncated packets */
	u64 rx_trunc1;            /* buffer-group 1 truncated packets */
	u64 rx_trunc2;            /* buffer-group 2 truncated packets */
	u64 rx_trunc3;            /* buffer-group 3 truncated packets */
};

struct lb_port_stats {
	u64 octets;
	u64 frames;
	u64 bcast_frames;
	u64 mcast_frames;
	u64 ucast_frames;
	u64 error_frames;

	u64 frames_64;
	u64 frames_65_127;
	u64 frames_128_255;
	u64 frames_256_511;
	u64 frames_512_1023;
	u64 frames_1024_1518;
	u64 frames_1519_max;

	u64 drop;

	u64 ovflow0;
	u64 ovflow1;
	u64 ovflow2;
	u64 ovflow3;
	u64 trunc0;
	u64 trunc1;
	u64 trunc2;
	u64 trunc3;
};

struct tp_tcp_stats {
	u32 tcpOutRsts;
	u64 tcpInSegs;
	u64 tcpOutSegs;
	u64 tcpRetransSegs;
};

struct tp_usm_stats {
	u32 frames;
	u32 drops;
	u64 octets;
};

struct tp_fcoe_stats {
	u32 framesDDP;
	u32 framesDrop;
	u64 octetsDDP;
};

struct tp_err_stats {
	u32 macInErrs[4];
	u32 hdrInErrs[4];
	u32 tcpInErrs[4];
	u32 tnlCongDrops[4];
	u32 ofldChanDrops[4];
	u32 tnlTxDrops[4];
	u32 ofldVlanDrops[4];
	u32 tcp6InErrs[4];
	u32 ofldNoNeigh;
	u32 ofldCongDefer;
};

struct tp_proxy_stats {
	u32 proxy[4];
};

struct tp_cpl_stats {
	u32 req[4];
	u32 rsp[4];
};

struct tp_rdma_stats {
	u32 rqe_dfr_mod;
	u32 rqe_dfr_pkt;
};

struct tp_params {
	unsigned int ntxchan;        /* # of Tx channels */
	unsigned int tre;            /* log2 of core clocks per TP tick */
	unsigned int dack_re;        /* DACK timer resolution */
	unsigned int la_mask;        /* what events are recorded by TP LA */
	unsigned short tx_modq[NCHAN];  /* channel to modulation queue map */
};

struct vpd_params {
	unsigned int cclk;
	u8 ec[EC_LEN + 1];
	u8 sn[SERNUM_LEN + 1];
	u8 id[ID_LEN + 1];
	u8 pn[PN_LEN + 1];
	u8 na[MACADDR_LEN + 1];
};

struct pci_params {
	unsigned int vpd_cap_addr;
	unsigned short speed;
	unsigned short width;
};

/*
 * Firmware device log.
 */
struct devlog_params {
	u32 memtype;			/* which memory (EDC0, EDC1, MC) */
	u32 start;			/* start of log in firmware memory */
	u32 size;			/* size of log */
};

struct adapter_params {
	struct tp_params  tp;
	struct vpd_params vpd;
	struct pci_params pci;
	struct devlog_params devlog;

	unsigned int sf_size;             /* serial flash size in bytes */
	unsigned int sf_nsec;             /* # of flash sectors */

	unsigned int fw_vers;
	unsigned int tp_vers;

	unsigned short mtus[NMTUS];
	unsigned short a_wnd[NCCTRL_WIN];
	unsigned short b_wnd[NCCTRL_WIN];

	unsigned int mc_size;		/* MC memory size */
	unsigned int nfilters;		/* size of filter region */

	unsigned int cim_la_size;

	uint8_t nports;		/* # of ethernet ports */
	uint8_t portvec;
	unsigned int chipid:4;	/* chip ID.  T4 = 4, T5 = 5, ... */
	unsigned int rev:4;	/* chip revision */
	unsigned int fpga:1;	/* this is an FPGA */
	unsigned int offload:1;	/* hw is TOE capable, fw has divvied up card
				   resources for TOE operation. */
	unsigned int bypass:1;	/* this is a bypass card */

	unsigned int ofldq_wr_cred;
};

#define CHELSIO_T4		0x4
#define CHELSIO_T5		0x5

struct trace_params {
	u32 data[TRACE_LEN / 4];
	u32 mask[TRACE_LEN / 4];
	unsigned short snap_len;
	unsigned short min_len;
	unsigned char skip_ofst;
	unsigned char skip_len;
	unsigned char invert;
	unsigned char port;
};

struct link_config {
	unsigned short supported;        /* link capabilities */
	unsigned short advertising;      /* advertised capabilities */
	unsigned short requested_speed;  /* speed user has requested */
	unsigned short speed;            /* actual link speed */
	unsigned char  requested_fc;     /* flow control user has requested */
	unsigned char  fc;               /* actual link flow control */
	unsigned char  autoneg;          /* autonegotiating? */
	unsigned char  link_ok;          /* link up? */
};

#include "adapter.h"

#ifndef PCI_VENDOR_ID_CHELSIO
# define PCI_VENDOR_ID_CHELSIO 0x1425
#endif

#define for_each_port(adapter, iter) \
	for (iter = 0; iter < (adapter)->params.nports; ++iter)

static inline int is_offload(const struct adapter *adap)
{
	return adap->params.offload;
}

static inline int chip_id(struct adapter *adap)
{
	return adap->params.chipid;
}

static inline int chip_rev(struct adapter *adap)
{
	return adap->params.rev;
}

static inline int is_t4(struct adapter *adap)
{
	return adap->params.chipid == CHELSIO_T4;
}

static inline int is_t5(struct adapter *adap)
{
	return adap->params.chipid == CHELSIO_T5;
}

static inline int is_fpga(struct adapter *adap)
{
	 return adap->params.fpga;
}

static inline unsigned int core_ticks_per_usec(const struct adapter *adap)
{
	return adap->params.vpd.cclk / 1000;
}

static inline unsigned int us_to_core_ticks(const struct adapter *adap,
					    unsigned int us)
{
	return (us * adap->params.vpd.cclk) / 1000;
}

static inline unsigned int dack_ticks_to_usec(const struct adapter *adap,
					      unsigned int ticks)
{
	return (ticks << adap->params.tp.dack_re) / core_ticks_per_usec(adap);
}

void t4_set_reg_field(struct adapter *adap, unsigned int addr, u32 mask, u32 val);
int t4_wait_op_done_val(struct adapter *adapter, int reg, u32 mask, int polarity,
			int attempts, int delay, u32 *valp);

static inline int t4_wait_op_done(struct adapter *adapter, int reg, u32 mask,
				  int polarity, int attempts, int delay)
{
	return t4_wait_op_done_val(adapter, reg, mask, polarity, attempts,
				   delay, NULL);
}

int t4_wr_mbox_meat(struct adapter *adap, int mbox, const void *cmd, int size,
		    void *rpl, bool sleep_ok);

static inline int t4_wr_mbox(struct adapter *adap, int mbox, const void *cmd,
			     int size, void *rpl)
{
	return t4_wr_mbox_meat(adap, mbox, cmd, size, rpl, true);
}

static inline int t4_wr_mbox_ns(struct adapter *adap, int mbox, const void *cmd,
				int size, void *rpl)
{
	return t4_wr_mbox_meat(adap, mbox, cmd, size, rpl, false);
}

void t4_read_indirect(struct adapter *adap, unsigned int addr_reg,
		      unsigned int data_reg, u32 *vals, unsigned int nregs,
		      unsigned int start_idx);
void t4_write_indirect(struct adapter *adap, unsigned int addr_reg,
		       unsigned int data_reg, const u32 *vals,
		       unsigned int nregs, unsigned int start_idx);

u32 t4_hw_pci_read_cfg4(adapter_t *adapter, int reg);

struct fw_filter_wr;

void t4_intr_enable(struct adapter *adapter);
void t4_intr_disable(struct adapter *adapter);
void t4_intr_clear(struct adapter *adapter);
int t4_slow_intr_handler(struct adapter *adapter);

int t4_hash_mac_addr(const u8 *addr);
int t4_link_start(struct adapter *adap, unsigned int mbox, unsigned int port,
		  struct link_config *lc);
int t4_restart_aneg(struct adapter *adap, unsigned int mbox, unsigned int port);
int t4_seeprom_read(struct adapter *adapter, u32 addr, u32 *data);
int t4_seeprom_write(struct adapter *adapter, u32 addr, u32 data);
int t4_eeprom_ptov(unsigned int phys_addr, unsigned int fn, unsigned int sz);
int t4_seeprom_wp(struct adapter *adapter, int enable);
int t4_read_flash(struct adapter *adapter, unsigned int addr, unsigned int nwords,
		  u32 *data, int byte_oriented);
int t4_load_fw(struct adapter *adapter, const u8 *fw_data, unsigned int size);
int t4_load_boot(struct adapter *adap, u8 *boot_data,
                 unsigned int boot_addr, unsigned int size);
unsigned int t4_flash_cfg_addr(struct adapter *adapter);
int t4_load_cfg(struct adapter *adapter, const u8 *cfg_data, unsigned int size);
int t4_get_fw_version(struct adapter *adapter, u32 *vers);
int t4_get_tp_version(struct adapter *adapter, u32 *vers);
int t4_check_fw_version(struct adapter *adapter);
int t4_init_hw(struct adapter *adapter, u32 fw_params);
int t4_prep_adapter(struct adapter *adapter);
int t4_port_init(struct port_info *p, int mbox, int pf, int vf);
int t4_reinit_adapter(struct adapter *adap);
void t4_fatal_err(struct adapter *adapter);
int t4_set_trace_filter(struct adapter *adapter, const struct trace_params *tp,
			int filter_index, int enable);
void t4_get_trace_filter(struct adapter *adapter, struct trace_params *tp,
			 int filter_index, int *enabled);
int t4_config_rss_range(struct adapter *adapter, int mbox, unsigned int viid,
			int start, int n, const u16 *rspq, unsigned int nrspq);
int t4_config_glbl_rss(struct adapter *adapter, int mbox, unsigned int mode,
		       unsigned int flags);
int t4_config_vi_rss(struct adapter *adapter, int mbox, unsigned int viid,
		     unsigned int flags, unsigned int defq);
int t4_read_rss(struct adapter *adapter, u16 *entries);
void t4_read_rss_key(struct adapter *adapter, u32 *key);
void t4_write_rss_key(struct adapter *adap, const u32 *key, int idx);
void t4_read_rss_pf_config(struct adapter *adapter, unsigned int index, u32 *valp);
void t4_write_rss_pf_config(struct adapter *adapter, unsigned int index, u32 val);
void t4_read_rss_vf_config(struct adapter *adapter, unsigned int index,
			   u32 *vfl, u32 *vfh);
void t4_write_rss_vf_config(struct adapter *adapter, unsigned int index,
			    u32 vfl, u32 vfh);
u32 t4_read_rss_pf_map(struct adapter *adapter);
void t4_write_rss_pf_map(struct adapter *adapter, u32 pfmap);
u32 t4_read_rss_pf_mask(struct adapter *adapter);
void t4_write_rss_pf_mask(struct adapter *adapter, u32 pfmask);
int t4_mps_set_active_ports(struct adapter *adap, unsigned int port_mask);
void t4_pmtx_get_stats(struct adapter *adap, u32 cnt[], u64 cycles[]);
void t4_pmrx_get_stats(struct adapter *adap, u32 cnt[], u64 cycles[]);
void t4_read_cimq_cfg(struct adapter *adap, u16 *base, u16 *size, u16 *thres);
int t4_read_cim_ibq(struct adapter *adap, unsigned int qid, u32 *data, size_t n);
int t4_read_cim_obq(struct adapter *adap, unsigned int qid, u32 *data, size_t n);
int t4_cim_read(struct adapter *adap, unsigned int addr, unsigned int n,
		unsigned int *valp);
int t4_cim_write(struct adapter *adap, unsigned int addr, unsigned int n,
		 const unsigned int *valp);
int t4_cim_ctl_read(struct adapter *adap, unsigned int addr, unsigned int n,
		    unsigned int *valp);
int t4_cim_read_la(struct adapter *adap, u32 *la_buf, unsigned int *wrptr);
void t4_cim_read_pif_la(struct adapter *adap, u32 *pif_req, u32 *pif_rsp,
		unsigned int *pif_req_wrptr, unsigned int *pif_rsp_wrptr);
void t4_cim_read_ma_la(struct adapter *adap, u32 *ma_req, u32 *ma_rsp);
int t4_mc_read(struct adapter *adap, int idx, u32 addr,
	       __be32 *data, u64 *parity);
int t4_edc_read(struct adapter *adap, int idx, u32 addr, __be32 *data, u64 *parity);
int t4_mem_read(struct adapter *adap, int mtype, u32 addr, u32 size,
		__be32 *data);

void t4_get_port_stats(struct adapter *adap, int idx, struct port_stats *p);
void t4_get_port_stats_offset(struct adapter *adap, int idx,
		struct port_stats *stats,
		struct port_stats *offset);
void t4_get_lb_stats(struct adapter *adap, int idx, struct lb_port_stats *p);
void t4_clr_port_stats(struct adapter *adap, int idx);

void t4_read_mtu_tbl(struct adapter *adap, u16 *mtus, u8 *mtu_log);
void t4_read_cong_tbl(struct adapter *adap, u16 incr[NMTUS][NCCTRL_WIN]);
void t4_read_pace_tbl(struct adapter *adap, unsigned int pace_vals[NTX_SCHED]);
void t4_get_tx_sched(struct adapter *adap, unsigned int sched, unsigned int *kbps,
		     unsigned int *ipg);
void t4_tp_wr_bits_indirect(struct adapter *adap, unsigned int addr,
			    unsigned int mask, unsigned int val);
void t4_tp_read_la(struct adapter *adap, u64 *la_buf, unsigned int *wrptr);
void t4_tp_get_err_stats(struct adapter *adap, struct tp_err_stats *st);
void t4_tp_get_proxy_stats(struct adapter *adap, struct tp_proxy_stats *st);
void t4_tp_get_cpl_stats(struct adapter *adap, struct tp_cpl_stats *st);
void t4_tp_get_rdma_stats(struct adapter *adap, struct tp_rdma_stats *st);
void t4_get_usm_stats(struct adapter *adap, struct tp_usm_stats *st);
void t4_tp_get_tcp_stats(struct adapter *adap, struct tp_tcp_stats *v4,
			 struct tp_tcp_stats *v6);
void t4_get_fcoe_stats(struct adapter *adap, unsigned int idx,
		       struct tp_fcoe_stats *st);
void t4_load_mtus(struct adapter *adap, const unsigned short *mtus,
		  const unsigned short *alpha, const unsigned short *beta);

void t4_ulprx_read_la(struct adapter *adap, u32 *la_buf);

int t4_set_sched_bps(struct adapter *adap, int sched, unsigned int kbps);
int t4_set_sched_ipg(struct adapter *adap, int sched, unsigned int ipg);
int t4_set_pace_tbl(struct adapter *adap, const unsigned int *pace_vals,
		    unsigned int start, unsigned int n);
void t4_get_chan_txrate(struct adapter *adap, u64 *nic_rate, u64 *ofld_rate);
int t4_set_filter_mode(struct adapter *adap, unsigned int mode_map);
void t4_mk_filtdelwr(unsigned int ftid, struct fw_filter_wr *wr, int qid);

void t4_wol_magic_enable(struct adapter *adap, unsigned int port, const u8 *addr);
int t4_wol_pat_enable(struct adapter *adap, unsigned int port, unsigned int map,
		      u64 mask0, u64 mask1, unsigned int crc, bool enable);

int t4_fw_hello(struct adapter *adap, unsigned int mbox, unsigned int evt_mbox,
		enum dev_master master, enum dev_state *state);
int t4_fw_bye(struct adapter *adap, unsigned int mbox);
int t4_fw_reset(struct adapter *adap, unsigned int mbox, int reset);
int t4_fw_halt(struct adapter *adap, unsigned int mbox, int force);
int t4_fw_restart(struct adapter *adap, unsigned int mbox, int reset);
int t4_fw_upgrade(struct adapter *adap, unsigned int mbox,
		  const u8 *fw_data, unsigned int size, int force);
int t4_fw_initialize(struct adapter *adap, unsigned int mbox);
int t4_query_params(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int nparams, const u32 *params,
		    u32 *val);
int t4_set_params(struct adapter *adap, unsigned int mbox, unsigned int pf,
		  unsigned int vf, unsigned int nparams, const u32 *params,
		  const u32 *val);
int t4_cfg_pfvf(struct adapter *adap, unsigned int mbox, unsigned int pf,
		unsigned int vf, unsigned int txq, unsigned int txq_eth_ctrl,
		unsigned int rxqi, unsigned int rxq, unsigned int tc,
		unsigned int vi, unsigned int cmask, unsigned int pmask,
		unsigned int exactf, unsigned int rcaps, unsigned int wxcaps);
int t4_alloc_vi_func(struct adapter *adap, unsigned int mbox,
		     unsigned int port, unsigned int pf, unsigned int vf,
		     unsigned int nmac, u8 *mac, unsigned int *rss_size,
		     unsigned int portfunc, unsigned int idstype);
int t4_alloc_vi(struct adapter *adap, unsigned int mbox, unsigned int port,
		unsigned int pf, unsigned int vf, unsigned int nmac, u8 *mac,
		unsigned int *rss_size);
int t4_free_vi(struct adapter *adap, unsigned int mbox,
	       unsigned int pf, unsigned int vf,
	       unsigned int viid);
int t4_set_rxmode(struct adapter *adap, unsigned int mbox, unsigned int viid,
		  int mtu, int promisc, int all_multi, int bcast, int vlanex,
		  bool sleep_ok);
int t4_alloc_mac_filt(struct adapter *adap, unsigned int mbox, unsigned int viid,
		      bool free, unsigned int naddr, const u8 **addr, u16 *idx,
		      u64 *hash, bool sleep_ok);
int t4_change_mac(struct adapter *adap, unsigned int mbox, unsigned int viid,
		  int idx, const u8 *addr, bool persist, bool add_smt);
int t4_set_addr_hash(struct adapter *adap, unsigned int mbox, unsigned int viid,
		     bool ucast, u64 vec, bool sleep_ok);
int t4_enable_vi(struct adapter *adap, unsigned int mbox, unsigned int viid,
		 bool rx_en, bool tx_en);
int t4_identify_port(struct adapter *adap, unsigned int mbox, unsigned int viid,
		     unsigned int nblinks);
int t4_i2c_rd(struct adapter *adap, unsigned int mbox, unsigned int port_id,
	      u8 dev_addr, u8 offset, u8 *valp);
int t4_mdio_rd(struct adapter *adap, unsigned int mbox, unsigned int phy_addr,
	       unsigned int mmd, unsigned int reg, unsigned int *valp);
int t4_mdio_wr(struct adapter *adap, unsigned int mbox, unsigned int phy_addr,
	       unsigned int mmd, unsigned int reg, unsigned int val);
int t4_iq_start_stop(struct adapter *adap, unsigned int mbox, bool start,
		     unsigned int pf, unsigned int vf, unsigned int iqid,
		     unsigned int fl0id, unsigned int fl1id);
int t4_iq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
	       unsigned int vf, unsigned int iqtype, unsigned int iqid,
	       unsigned int fl0id, unsigned int fl1id);
int t4_eth_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		   unsigned int vf, unsigned int eqid);
int t4_ctrl_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int eqid);
int t4_ofld_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int eqid);
int t4_sge_ctxt_rd(struct adapter *adap, unsigned int mbox, unsigned int cid,
		   enum ctxt_type ctype, u32 *data);
int t4_sge_ctxt_rd_bd(struct adapter *adap, unsigned int cid, enum ctxt_type ctype,
		      u32 *data);
int t4_sge_ctxt_flush(struct adapter *adap, unsigned int mbox);
int t4_handle_fw_rpl(struct adapter *adap, const __be64 *rpl);
int t4_fwaddrspace_write(struct adapter *adap, unsigned int mbox, u32 addr, u32 val);
#endif /* __CHELSIO_COMMON_H */
