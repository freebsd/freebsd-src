/*
 * Copyright (C) 2000
 * Dr. Duncan McLennan Barclay, dmlb@ragnet.demon.co.uk.
 *
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DUNCAN BARCLAY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DUNCAN BARCLAY OR CONTRIBUTORS BE LIABLE
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

/*	$NetBSD: if_rayreg.h,v 1.1 2000/01/23 23:59:22 chopps Exp $	*/
/* 
 * Copyright (c) 2000 Christian E. Hopps
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

/*
 * CCR registers, appearing in the attribute memory space
 */
#define RAY_CCR		0xf00			/* CCR register offset */
#define RAY_COR		(RAY_CCR + 0x00)	/* config option register */
#define	RAY_CCSR	(RAY_CCR + 0x01)	/* config/status register */
#define	RAY_PIN		(RAY_CCR + 0x02)	/* not used by hw */
#define	RAY_SOCKETCOPY	(RAY_CCR + 0x03)	/* not used by hw */
#define	RAY_HCSIR	(RAY_CCR + 0x05)	/* HCS intr register */
#define	RAY_ECFIR	(RAY_CCR + 0x06)	/* ECF intr register */
/*
 * We don't seem to be able to access these in a simple manner
 */
#define	RAY_AR0		(RAY_CCR + 0x08)	/* authorization register 0 (unused) */
#define	RAY_AR1		(RAY_CCR + 0x09)	/* authorization register 1 (unused) */
#define	RAY_PMR		(RAY_CCR + 0x0a)	/* program mode register (unused) */
#define	RAY_TMR		(RAY_CCR + 0x0b)	/* pc test mode register (unused) */
#define	RAY_FCWR	(RAY_CCR + 0x10)	/* frequency control word register */
#define RAY_TMC1	(RAY_CCR + 0x14)	/* test mode control 1 (unused) */
#define RAY_TMC2	(RAY_CCR + 0x15)	/* test mode control 1 (unused) */
#define RAY_TMC3	(RAY_CCR + 0x16)	/* test mode control 1 (unused) */
#define RAY_TMC4	(RAY_CCR + 0x17)	/* test mode control 1 (unused) */

/*
 * COR register bits
 */
#define	RAY_COR_CFG_NUM		0x01	/* currently ignored and set */
#define RAY_COR_CFG_MASK	0x3f	/* mask for function */
#define	RAY_COR_LEVEL_IRQ	0x40	/* currently ignored and set */
#define	RAY_COR_RESET		0x80	/* soft-reset the card */
#define RAY_COR_DEFAULT		(RAY_COR_CFG_NUM | RAY_COR_LEVEL_IRQ)

/*
 * CCS register bits
 */
#define	RAY_CCS_NORMAL		0x00	/* normal operation */
#define	RAY_CCS_IRQ		0x02	/* interrupt pending */
#define	RAY_CCS_POWER_DOWN	0x04	/* when written powers down card */

/*
 * HCSIR bits
 *
 * the host can only clear this bit.
 */
#define	RAY_HCSIR_IRQ		0x01	/* indicates an interrupt */

/*
 * ECFIR values
 */
#define	RAY_ECFIR_IRQ		0x01	/* interrupt the card */

/*
 * AR0 values
 * used for testing/programming the card (unused)
 */
#define	RAY_AR0_ON		0x57

/*
 * AR1 values
 * used for testing/programming the card (unused)
 */
#define	RAY_AR1_ON		0x82

/*
 * PMR bits 
 * these are used to program the card (unused)
 */
#define	RAY_PMR_NORMAL		0x00	/* normal operation */
#define	RAY_PMR_PC2PM		0x02	/* grant access to firmware flash */
#define	RAY_PMR_PC2CAL		0x10	/* read access to the A/D modem inp */
#define	RAY_PMR_MLSE		0x20	/* read access to the MSLE prom */

/*
 * TMR bits
 * get access to test modes (unused)
 */
#define	RAY_TMR_NORMAL		0x00	/* normal operation */
#define	RAY_TMR_TEST		0x08	/* test mode */

/*
 * FCWR -- frequency control word, values from [0x02,0xA6] map to
 * RF frequency values.
 */

/*
 * 48k of memory
 */
#define	RAY_SRAM_MEM_BASE	0
#define	RAY_SRAM_MEM_SIZE	0xc000

/*
 * offsets into shared ram
 */
#define	RAY_SCB_BASE		0x0000	/* cfg/status/ctl area */
#define	RAY_STATUS_BASE		0x0100
#define	RAY_HOST_TO_ECF_BASE	0x0200
#define	RAY_ECF_TO_HOST_BASE	0x0300
#define	RAY_CCS_BASE		0x0400
#define	RAY_RCS_BASE		0x0800
#define	RAY_APOINT_TIM_BASE	0x0c00
#define	RAY_SSID_LIST_BASE	0x0d00
#define	RAY_TX_BASE		0x1000
#define	RAY_TX_SIZE		0x7000
#define	RAY_TX_END		0x8000
#define	RAY_RX_BASE		0x8000
#define	RAY_RX_END		0xc000
#define	RAY_RX_MASK		0x3fff

/*
 * Startup reporting stucture
 */
struct ray_ecf_startup_v4 {
    u_int8_t	e_status;
    u_int8_t	e_station_addr[ETHER_ADDR_LEN];
    u_int8_t	e_prg_cksum;
    u_int8_t	e_cis_cksum;
    u_int8_t	e_resv0[7];
    u_int8_t	e_japan_callsign[12];
};
struct ray_ecf_startup_v5 {
    u_int8_t	e_status;
    u_int8_t	e_station_addr[ETHER_ADDR_LEN];
    u_int8_t	e_resv0;
    u_int8_t	e_rates[8];
    u_int8_t	e_japan_callsign[12];
    u_int8_t	e_prg_cksum;
    u_int8_t	e_cis_cksum;
    u_int8_t	e_fw_build_string;
    u_int8_t	e_fw_build;
    u_int8_t	e_fw_resv;
    u_int8_t	e_asic_version;
    u_int8_t	e_tibsize;
    u_int8_t	e_resv1[29];
};

/*
 * Startup status word result codes
 */
#define	RAY_ECFS_RESERVED0		0x01
#define	RAY_ECFS_PROC_SELF_TEST		0x02
#define	RAY_ECFS_PROG_MEM_CHECKSUM	0x04
#define	RAY_ECFS_DATA_MEM_TEST		0x08
#define	RAY_ECFS_RX_CALIBRATION		0x10
#define	RAY_ECFS_FW_VERSION_COMPAT	0x20
#define	RAY_ECFS_RERSERVED1		0x40
#define	RAY_ECFS_TEST_COMPLETE		0x80
#define	RAY_ECFS_CARD_OK		RAY_ECFS_TEST_COMPLETE
#define RAY_ECFS_PRINTFB	\
	"\020"			\
	"\001RESERVED0"		\
	"\002PROC_SELF_TEST"	\
	"\003PROG_MEM_CHECKSUM"	\
	"\004DATA_MEM_TEST"	\
	"\005RX_CALIBRATION"	\
	"\006FW_VERSION_COMPAT"	\
	"\007RERSERVED1"	\
	"\010TEST_COMPLETE"

/*
 * Firmware build codes
 */
#define	RAY_ECFS_BUILD_4		0x55
#define	RAY_ECFS_BUILD_5		0x5

/*
 * System Control Block
 */
#define	RAY_SCB_CCSI		0x00	/* host CCS index */
#define	RAY_SCB_RCSI		0x01	/* ecf RCS index */

/*
 * command control structures (for CCSR commands)
 */

/*
 * commands for CCSR
 */
#define	RAY_CMD_DOWNLOAD_PARAMS	0x01	/* download start params */
#define	RAY_CMD_UPDATE_PARAMS	0x02	/* update params */
#define	RAY_CMD_REPORT_PARAMS	0x03	/* report params */
#define	RAY_CMD_UPDATE_MCAST	0x04	/* update mcast list */
#define	RAY_CMD_UPDATE_APM	0x05	/* update power saving mode */
#define	RAY_CMD_START_NET	0x06
#define	RAY_CMD_JOIN_NET	0x07
#define	RAY_CMD_START_ASSOC	0x08
#define	RAY_CMD_TX_REQ		0x09
#define	RAY_CMD_TEST_MEM	0x0a
#define	RAY_CMD_SHUTDOWN	0x0b
#define	RAY_CMD_DUMP_MEM	0x0c
#define	RAY_CMD_START_TIMER	0x0d
#define	RAY_CMD_MAX		0x0e

/*
 * unsolicted commands from the ECF
 */
#define	RAY_ECMD_RX_DONE		0x80	/* process rx packet */
#define	RAY_ECMD_REJOIN_DONE		0x81	/* rejoined the network */
#define	RAY_ECMD_ROAM_START		0x82	/* romaining started */
#define	RAY_ECMD_JAPAN_CALL_SIGNAL	0x83	/* japan test thing */


/*
 * Configure/status/control memory
 */
struct ray_csc {
    u_int8_t	csc_mrxo_own;		/* 0 ECF writes, 1 host write */
    u_int8_t	csc_mrxc_own;		/* 0 ECF writes, 1 host write */
    u_int8_t	csc_rxhc_own;		/* 0 ECF writes, 1 host write */
    u_int8_t	csc_resv;
    u_int16_t	csc_mrx_overflow;	/* ECF incs on rx overflow */
    u_int16_t	csc_mrx_cksum;		/* ECF incs on cksum error */
    u_int16_t	csc_rx_hcksum;		/* ECF incs on header cksum error */
    u_int8_t	csc_rx_noise;		/* average RSL measuremant */
};

/*
 * CCS area
 */
#define	RAY_CCS_LINK_NULL	0xff
#define	RAY_CCS_SIZE		16

#define	RAY_CCS_TX_FIRST	0
#define	RAY_CCS_TX_LAST		13
#define	RAY_CCS_NTX		(RAY_CCS_TX_LAST - RAY_CCS_TX_FIRST + 1)
#define	RAY_TX_BUF_SIZE		2048
#define	RAY_CCS_CMD_FIRST	14
#define	RAY_CCS_CMD_LAST	63
#define	RAY_CCS_NCMD		(RAY_CCS_CMD_LAST - RAY_CCS_CMD_FIRST + 1)
#define	RAY_CCS_LAST		63

#define	RAY_CCS_INDEX(ccs)	(((ccs) - RAY_CCS_BASE) / RAY_CCS_SIZE)
#define	RAY_CCS_ADDRESS(i)	(RAY_CCS_BASE + (i) * RAY_CCS_SIZE)

/*
 * RCS area
 */
#define	RAY_RCS_FIRST	64
#define	RAY_RCS_LAST	127

/*
 * CCS commands
 */
struct ray_cmd {
    u_int8_t	c_status;		/* ccs generic header */
    u_int8_t	c_cmd;			/* " */
    u_int8_t	c_link;			/* " */
};

#define	RAY_CCS_STATUS_FREE		0x0
#define	RAY_CCS_STATUS_BUSY		0x1
#define	RAY_CCS_STATUS_COMPLETE		0x2
#define	RAY_CCS_STATUS_FAIL		0x3
#define RAY_CCS_STATUS_STRINGS {	\
    "free",				\
    "busy",				\
    "complete",				\
    "fail"				\
}

/* RAY_CMD_UPDATE_PARAMS */
struct ray_cmd_update {
    u_int8_t	c_status;		/* ccs generic header */
    u_int8_t	c_cmd;			/* " */
    u_int8_t	c_link;			/* " */
    u_int8_t	c_paramid;
    u_int8_t	c_nparam;
    u_int8_t	c_failcause;
};

/* RAY_CMD_REPORT_PARAMS */
struct ray_cmd_report {
    u_int8_t	c_status;		/* ccs generic header */
    u_int8_t	c_cmd;			/* " */
    u_int8_t	c_link;			/* " */
    u_int8_t	c_paramid;
    u_int8_t	c_nparam;
    u_int8_t	c_failcause;
    u_int8_t	c_len;
};

/* RAY_CMD_UPDATE_MCAST */
struct ray_cmd_update_mcast {
    u_int8_t	c_status;		/* ccs generic header */
    u_int8_t	c_cmd;			/* " */
    u_int8_t	c_link;			/* " */
    u_int8_t	c_nmcast;
};

/* RAY_CMD_UPDATE_APM */
struct ray_cmd_udpate_apm {
    u_int8_t	c_status;		/* ccs generic header */
    u_int8_t	c_cmd;			/* " */
    u_int8_t	c_link;			/* " */
    u_int8_t	c_mode;
};

/* RAY_CMD_START_NET and RAY_CMD_JOIN_NET */
struct ray_cmd_net {
    u_int8_t	c_status;		/* ccs generic header */
    u_int8_t	c_cmd;			/* " */
    u_int8_t	c_link;			/* " */
    u_int8_t	c_upd_param;
    u_int8_t	c_bss_id[ETHER_ADDR_LEN];
    u_int8_t	c_inited;
    u_int8_t	c_def_txrate;
    u_int8_t	c_encrypt;
};
/* Parameters passed in HOST_TO_ECF section when c_upd_param is set in
 * ray_cmd_net. */
struct ray_net_params {
    u_int8_t	p_net_type;
    u_int8_t	p_ssid[32];
    u_int8_t	p_privacy_must_start;
    u_int8_t	p_privacy_can_join;
};

/* RAY_CMD_START_ASSOC */
struct ray_cmd_update_assoc {
    u_int8_t	c_status;		/* ccs generic header */
    u_int8_t	c_cmd;			/* " */
    u_int8_t	c_link;			/* " */
    u_int8_t	c_astatus;
    u_int8_t	c_aid[2];
};

/* RAY_CMD_TX_REQ */
struct ray_cmd_tx {
    u_int8_t	c_status;		/* ccs generic header */
    u_int8_t	c_cmd;			/* " */
    u_int8_t	c_link;			/* " */
    u_int8_t	c_bufp[2];
    u_int8_t	c_len[2];
    u_int8_t	c_resv[5];
    u_int8_t	c_tx_rate;
    u_int8_t	c_apm_mode;
    u_int8_t	c_nretry;
    u_int8_t	c_antenna;
};
struct ray_cmd_tx_4 {
    u_int8_t	c_status;		/* ccs generic header */
    u_int8_t	c_cmd;			/* " */
    u_int8_t	c_link;			/* " */
    u_int8_t	c_bufp[2];
    u_int8_t	c_len[2];
    u_int8_t	c_addr[ETHER_ADDR_LEN];
    u_int8_t	c_apm_mode;
    u_int8_t	c_nretry;
    u_int8_t	c_antenna;
};

/* RAY_CMD_DUMP_MEM */
struct ray_cmd_dump_mem {
    u_int8_t	c_status;		/* ccs generic header */
    u_int8_t	c_cmd;			/* " */
    u_int8_t	c_link;			/* " */
    u_int8_t	c_memtype;
    u_int8_t	c_memp[2];
    u_int8_t	c_len;
};

/* RAY_CMD_START_TIMER */
struct ray_cmd_start_timer {
    u_int8_t	c_status;		/* ccs generic header */
    u_int8_t	c_cmd;			/* " */
    u_int8_t	c_link;			/* " */
    u_int8_t	c_duration[2];
};

/* RAY_ECMD_RX_DONE */
struct ray_cmd_rx {
    u_int8_t	c_status;		/* ccs generic header */
    u_int8_t	c_cmd;			/* " */
    u_int8_t	c_link;			/* " */
    u_int8_t	c_bufp[2];		/* buffer pointer */
    u_int8_t	c_len[2];		/* length */
    u_int8_t	c_siglev;		/* signal level */
    u_int8_t	c_nextfrag;		/* next fragment in packet */
    u_int8_t	c_pktlen[2];		/* total packet length */
    u_int8_t	c_antenna;		/* ant. with best reception */
    u_int8_t	c_updbss;		/* only 1 for beacon messages */
};

/*
 * Transmit scratch space and phy header structures
 */
struct ray_tx_tib {
    u_int8_t	t_ccs_index;
    u_int8_t	t_psm;
    u_int8_t	t_pass_fail;
    u_int8_t	t_retry_count;
    u_int8_t	t_max_retries;
    u_int8_t	t_frags_remaining;
    u_int8_t	t_no_rb;
    u_int8_t	t_rts_reqd;
    u_int8_t	t_csma_tx_cntrl_2;
    u_int8_t	t_sifs_tx_cntrl_2;
    u_int8_t	t_tx_dma_addr_1[2];
    u_int8_t	t_tx_dma_addr_2[2];
    u_int8_t	t_var_dur_2mhz[2];
    u_int8_t	t_var_dur_1mhz[2];
    u_int8_t	t_max_dur_2mhz[2];
    u_int8_t	t_max_dur_1mhz[2];
    u_int8_t	t_hdr_len;
    u_int8_t	t_max_frag_len[2];
    u_int8_t	t_var_len[2];
    u_int8_t	t_phy_hdr_4;
    u_int8_t	t_mac_hdr_1;
    u_int8_t	t_mac_hdr_2;
    u_int8_t	t_sid[2];
};

struct ray_tx_phy_header {
    u_int8_t	t_sfd[2];
    u_int8_t	t_hdr_3;
    u_int8_t	t_hdr_4;
};
