/*
 * Copyright (c) 1995, David Greenman
 * Copyright (c) 2001 Jonathan Lemon <jlemon@freebsd.org>
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

#define FXP_VENDORID_INTEL	0x8086

#define FXP_PCI_MMBA	0x10
#define FXP_PCI_IOBA	0x14

/*
 * Control/status registers.
 */
#define	FXP_CSR_SCB_RUSCUS	0	/* scb_rus/scb_cus (1 byte) */
#define	FXP_CSR_SCB_STATACK	1	/* scb_statack (1 byte) */
#define	FXP_CSR_SCB_COMMAND	2	/* scb_command (1 byte) */
#define	FXP_CSR_SCB_INTRCNTL	3	/* scb_intrcntl (1 byte) */
#define	FXP_CSR_SCB_GENERAL	4	/* scb_general (4 bytes) */
#define	FXP_CSR_PORT		8	/* port (4 bytes) */
#define	FXP_CSR_FLASHCONTROL	12	/* flash control (2 bytes) */
#define	FXP_CSR_EEPROMCONTROL	14	/* eeprom control (2 bytes) */
#define	FXP_CSR_MDICONTROL	16	/* mdi control (4 bytes) */
#define	FXP_CSR_FLOWCONTROL	0x19	/* flow control (2 bytes) */

/*
 * FOR REFERENCE ONLY, the old definition of FXP_CSR_SCB_RUSCUS:
 *
 *	volatile u_int8_t	:2,
 *				scb_rus:4,
 *				scb_cus:2;
 */

#define FXP_PORT_SOFTWARE_RESET		0
#define FXP_PORT_SELFTEST		1
#define FXP_PORT_SELECTIVE_RESET	2
#define FXP_PORT_DUMP			3

#define FXP_SCB_RUS_IDLE		0
#define FXP_SCB_RUS_SUSPENDED		1
#define FXP_SCB_RUS_NORESOURCES		2
#define FXP_SCB_RUS_READY		4
#define FXP_SCB_RUS_SUSP_NORBDS		9
#define FXP_SCB_RUS_NORES_NORBDS	10
#define FXP_SCB_RUS_READY_NORBDS	12

#define FXP_SCB_CUS_IDLE		0
#define FXP_SCB_CUS_SUSPENDED		1
#define FXP_SCB_CUS_ACTIVE		2

#define FXP_SCB_INTR_DISABLE		0x01	/* Disable all interrupts */
#define FXP_SCB_INTR_SWI		0x02	/* Generate SWI */
#define FXP_SCB_INTMASK_FCP		0x04
#define FXP_SCB_INTMASK_ER		0x08
#define FXP_SCB_INTMASK_RNR		0x10
#define FXP_SCB_INTMASK_CNA		0x20
#define FXP_SCB_INTMASK_FR		0x40
#define FXP_SCB_INTMASK_CXTNO		0x80

#define FXP_SCB_STATACK_FCP		0x01	/* Flow Control Pause */
#define FXP_SCB_STATACK_ER		0x02	/* Early Receive */
#define FXP_SCB_STATACK_SWI		0x04
#define FXP_SCB_STATACK_MDI		0x08
#define FXP_SCB_STATACK_RNR		0x10
#define FXP_SCB_STATACK_CNA		0x20
#define FXP_SCB_STATACK_FR		0x40
#define FXP_SCB_STATACK_CXTNO		0x80

#define FXP_SCB_COMMAND_CU_NOP		0x00
#define FXP_SCB_COMMAND_CU_START	0x10
#define FXP_SCB_COMMAND_CU_RESUME	0x20
#define FXP_SCB_COMMAND_CU_DUMP_ADR	0x40
#define FXP_SCB_COMMAND_CU_DUMP		0x50
#define FXP_SCB_COMMAND_CU_BASE		0x60
#define FXP_SCB_COMMAND_CU_DUMPRESET	0x70

#define FXP_SCB_COMMAND_RU_NOP		0
#define FXP_SCB_COMMAND_RU_START	1
#define FXP_SCB_COMMAND_RU_RESUME	2
#define FXP_SCB_COMMAND_RU_ABORT	4
#define FXP_SCB_COMMAND_RU_LOADHDS	5
#define FXP_SCB_COMMAND_RU_BASE		6
#define FXP_SCB_COMMAND_RU_RBDRESUME	7

/*
 * Command block definitions
 */
struct fxp_cb_nop {
	void *fill[2];
	volatile u_int16_t cb_status;
	volatile u_int16_t cb_command;
	volatile u_int32_t link_addr;
};
struct fxp_cb_ias {
	void *fill[2];
	volatile u_int16_t cb_status;
	volatile u_int16_t cb_command;
	volatile u_int32_t link_addr;
	volatile u_int8_t macaddr[6];
};
/* I hate bit-fields :-( */
struct fxp_cb_config {
	void *fill[2];
	volatile u_int16_t	cb_status;
	volatile u_int16_t	cb_command;
	volatile u_int32_t	link_addr;
	volatile u_int		byte_count:6,
				:2;
	volatile u_int		rx_fifo_limit:4,
				tx_fifo_limit:3,
				:1;
	volatile u_int8_t	adaptive_ifs;
	volatile u_int		mwi_enable:1,			/* 8,9 */
				type_enable:1,			/* 8,9 */
				read_align_en:1,		/* 8,9 */
				end_wr_on_cl:1,			/* 8,9 */
				:4;
	volatile u_int		rx_dma_bytecount:7,
				:1;
	volatile u_int		tx_dma_bytecount:7,
				dma_mbce:1;
	volatile u_int		late_scb:1,			/* 7 */
				direct_dma_dis:1,		/* 8,9 */
				tno_int_or_tco_en:1,		/* 7,9 */
				ci_int:1,
				ext_txcb_dis:1,			/* 8,9 */
				ext_stats_dis:1,		/* 8,9 */
				keep_overrun_rx:1,
				save_bf:1;
	volatile u_int		disc_short_rx:1,
				underrun_retry:2,
				:3,
				two_frames:1,			/* 8,9 */
				dyn_tbd:1;			/* 8,9 */
	volatile u_int		mediatype:1,			/* 7 */
				:6,
				csma_dis:1;			/* 8,9 */
	volatile u_int		tcp_udp_cksum:1,		/* 9 */
				:3,
				vlan_tco:1,			/* 8,9 */
				link_wake_en:1,			/* 8,9 */
				arp_wake_en:1,			/* 8 */
				mc_wake_en:1;			/* 8 */
	volatile u_int		:3,
				nsai:1,
				preamble_length:2,
				loopback:2;
	volatile u_int		linear_priority:3,		/* 7 */
				:5;
	volatile u_int		linear_pri_mode:1,		/* 7 */
				:3,
				interfrm_spacing:4;
	volatile u_int		:8;
	volatile u_int		:8;
	volatile u_int		promiscuous:1,
				bcast_disable:1,
				wait_after_win:1,		/* 8,9 */
				:1,
				ignore_ul:1,			/* 8,9 */
				crc16_en:1,			/* 9 */
				:1,
				crscdt:1;
	volatile u_int		fc_delay_lsb:8;			/* 8,9 */
	volatile u_int		fc_delay_msb:8;			/* 8,9 */
	volatile u_int		stripping:1,
				padding:1,
				rcv_crc_xfer:1,
				long_rx_en:1,			/* 8,9 */
				pri_fc_thresh:3,		/* 8,9 */
				:1;
	volatile u_int		ia_wake_en:1,			/* 8 */
				magic_pkt_dis:1,		/* 8,9,!9ER */
				tx_fc_dis:1,			/* 8,9 */
				rx_fc_restop:1,			/* 8,9 */
				rx_fc_restart:1,		/* 8,9 */
				fc_filter:1,			/* 8,9 */
				force_fdx:1,
				fdx_pin_en:1;
	volatile u_int		:5,
				pri_fc_loc:1,			/* 8,9 */
				multi_ia:1,
				:1;
	volatile u_int		:3,
				mc_all:1,
				:4;
};

#define MAXMCADDR 80
struct fxp_cb_mcs {
	struct fxp_cb_tx *next;
	struct mbuf *mb_head;
	volatile u_int16_t cb_status;
	volatile u_int16_t cb_command;
	volatile u_int32_t link_addr;
	volatile u_int16_t mc_cnt;
	volatile u_int8_t mc_addr[MAXMCADDR][6];
};

/*
 * Number of DMA segments in a TxCB. Note that this is carefully
 * chosen to make the total struct size an even power of two. It's
 * critical that no TxCB be split across a page boundry since
 * no attempt is made to allocate physically contiguous memory.
 * 
 */
#ifdef __alpha__ /* XXX - should be conditional on pointer size */
#define FXP_NTXSEG      28
#else
#define FXP_NTXSEG      29
#endif

struct fxp_tbd {
	volatile u_int32_t tb_addr;
	volatile u_int32_t tb_size;
};
struct fxp_cb_tx {
	struct fxp_cb_tx *next;
	struct mbuf *mb_head;
	volatile u_int16_t cb_status;
	volatile u_int16_t cb_command;
	volatile u_int32_t link_addr;
	volatile u_int32_t tbd_array_addr;
	volatile u_int16_t byte_count;
	volatile u_int8_t tx_threshold;
	volatile u_int8_t tbd_number;
	/*
	 * The following structure isn't actually part of the TxCB,
	 * unless the extended TxCB feature is being used.  In this
	 * case, the first two elements of the structure below are 
	 * fetched along with the TxCB.
	 */
	volatile struct fxp_tbd tbd[FXP_NTXSEG];
};

/*
 * Control Block (CB) definitions
 */

/* status */
#define FXP_CB_STATUS_OK	0x2000
#define FXP_CB_STATUS_C		0x8000
/* commands */
#define FXP_CB_COMMAND_NOP	0x0
#define FXP_CB_COMMAND_IAS	0x1
#define FXP_CB_COMMAND_CONFIG	0x2
#define FXP_CB_COMMAND_MCAS	0x3
#define FXP_CB_COMMAND_XMIT	0x4
#define FXP_CB_COMMAND_RESRV	0x5
#define FXP_CB_COMMAND_DUMP	0x6
#define FXP_CB_COMMAND_DIAG	0x7
/* command flags */
#define FXP_CB_COMMAND_SF	0x0008	/* simple/flexible mode */
#define FXP_CB_COMMAND_I	0x2000	/* generate interrupt on completion */
#define FXP_CB_COMMAND_S	0x4000	/* suspend on completion */
#define FXP_CB_COMMAND_EL	0x8000	/* end of list */

/*
 * RFA definitions
 */

struct fxp_rfa {
	volatile u_int16_t rfa_status;
	volatile u_int16_t rfa_control;
        volatile u_int8_t link_addr[4];
        volatile u_int8_t rbd_addr[4];
	volatile u_int16_t actual_size;
	volatile u_int16_t size;
};
#define FXP_RFA_STATUS_RCOL	0x0001	/* receive collision */
#define FXP_RFA_STATUS_IAMATCH	0x0002	/* 0 = matches station address */
#define FXP_RFA_STATUS_S4	0x0010	/* receive error from PHY */
#define FXP_RFA_STATUS_TL	0x0020	/* type/length */
#define FXP_RFA_STATUS_FTS	0x0080	/* frame too short */
#define FXP_RFA_STATUS_OVERRUN	0x0100	/* DMA overrun */
#define FXP_RFA_STATUS_RNR	0x0200	/* no resources */
#define FXP_RFA_STATUS_ALIGN	0x0400	/* alignment error */
#define FXP_RFA_STATUS_CRC	0x0800	/* CRC error */
#define FXP_RFA_STATUS_OK	0x2000	/* packet received okay */
#define FXP_RFA_STATUS_C	0x8000	/* packet reception complete */
#define FXP_RFA_CONTROL_SF	0x08	/* simple/flexible memory mode */
#define FXP_RFA_CONTROL_H	0x10	/* header RFD */
#define FXP_RFA_CONTROL_S	0x4000	/* suspend after reception */
#define FXP_RFA_CONTROL_EL	0x8000	/* end of list */

/*
 * Statistics dump area definitions
 */
struct fxp_stats {
	volatile u_int32_t tx_good;
	volatile u_int32_t tx_maxcols;
	volatile u_int32_t tx_latecols;
	volatile u_int32_t tx_underruns;
	volatile u_int32_t tx_lostcrs;
	volatile u_int32_t tx_deffered;
	volatile u_int32_t tx_single_collisions;
	volatile u_int32_t tx_multiple_collisions;
	volatile u_int32_t tx_total_collisions;
	volatile u_int32_t rx_good;
	volatile u_int32_t rx_crc_errors;
	volatile u_int32_t rx_alignment_errors;
	volatile u_int32_t rx_rnr_errors;
	volatile u_int32_t rx_overrun_errors;
	volatile u_int32_t rx_cdt_errors;
	volatile u_int32_t rx_shortframes;
	volatile u_int32_t completion_status;
};
#define FXP_STATS_DUMP_COMPLETE	0xa005
#define FXP_STATS_DR_COMPLETE	0xa007
	
/*
 * Serial EEPROM control register bits
 */
#define FXP_EEPROM_EESK		0x01 		/* shift clock */
#define FXP_EEPROM_EECS		0x02 		/* chip select */
#define FXP_EEPROM_EEDI		0x04 		/* data in */
#define FXP_EEPROM_EEDO		0x08 		/* data out */

/*
 * Serial EEPROM opcodes, including start bit
 */
#define FXP_EEPROM_OPC_ERASE	0x4
#define FXP_EEPROM_OPC_WRITE	0x5
#define FXP_EEPROM_OPC_READ	0x6

/*
 * Management Data Interface opcodes
 */
#define FXP_MDI_WRITE		0x1
#define FXP_MDI_READ		0x2

/*
 * PHY device types
 */
#define FXP_PHY_DEVICE_MASK	0x3f00
#define FXP_PHY_SERIAL_ONLY	0x8000
#define FXP_PHY_NONE		0
#define FXP_PHY_82553A		1
#define FXP_PHY_82553C		2
#define FXP_PHY_82503		3
#define FXP_PHY_DP83840		4
#define FXP_PHY_80C240		5
#define FXP_PHY_80C24		6
#define FXP_PHY_82555		7
#define FXP_PHY_DP83840A	10
#define FXP_PHY_82555B		11
