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

#define FXP_VENDORID_INTEL	0x8086
#define FXP_DEVICEID_i82557	0x1229	/* 82557 - 82559 "classic" */
#define FXP_DEVICEID_i82559	0x1030	/* New 82559 device id.. */
#define FXP_DEVICEID_i82559ER	0x1209	/* 82559 for embedded applications */
#define FXP_DEVICEID_i82562	0x2449	/* 82562 PLC devices */

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
	volatile u_int		:8;
	volatile u_int		rx_dma_bytecount:7,
				:1;
	volatile u_int		tx_dma_bytecount:7,
				dma_bce:1;
	volatile u_int		late_scb:1,
				:1,
				tno_int:1,
				ci_int:1,
				:3,
				save_bf:1;
	volatile u_int		disc_short_rx:1,
				underrun_retry:2,
				:5;
	volatile u_int		mediatype:1,
				:7;
	volatile u_int		:8;
	volatile u_int		:3,
				nsai:1,
				preamble_length:2,
				loopback:2;
	volatile u_int		linear_priority:3,
				:5;
	volatile u_int		linear_pri_mode:1,
				:3,
				interfrm_spacing:4;
	volatile u_int		:8;
	volatile u_int		:8;
	volatile u_int		promiscuous:1,
				bcast_disable:1,
				:5,
				crscdt:1;
	volatile u_int		:8;
	volatile u_int		:8;
	volatile u_int		stripping:1,
				padding:1,
				rcv_crc_xfer:1,
				:5;
	volatile u_int		:6,
				force_fdx:1,
				fdx_pin_en:1;
	volatile u_int		:6,
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
	 * The following isn't actually part of the TxCB.
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
/* shift clock */
#define FXP_EEPROM_EESK		0x01
/* chip select */
#define FXP_EEPROM_EECS		0x02
/* data in */
#define FXP_EEPROM_EEDI		0x04
/* data out */
#define FXP_EEPROM_EEDO		0x08

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

/*
 * PHY BMCR Basic Mode Control Register
 * Should probably be in i82555.h or dp83840.h (Intel/National names).
 * (Called "Management Data Interface Control Reg" in some Intel data books).
 * (*) indicates bit ignored in auto negotiation mode.
 */
#define FXP_PHY_BMCR			0x0 
#define FXP_PHY_BMCR_COLTEST		0x0080 /* not on Intel parts */
#define FXP_PHY_BMCR_FULLDUPLEX		0x0100 /* 1 = Fullduplex (*) */
#define FXP_PHY_BMCR_RESTART_NEG	0x0200 /* ==> 1 to restart autoneg */
#define FXP_PHY_BMCR_ISOLATE		0x0400 /* not on Intel parts */
#define FXP_PHY_BMCR_POWERDOWN		0x0800 /* 1 = low power mode */
#define FXP_PHY_BMCR_AUTOEN		0x1000 /* 1 = for auto mode */
#define FXP_PHY_BMCR_SPEED_100M		0x2000 /* 1 = for 100Mb/sec (*) */
#define FXP_PHY_BMCR_LOOPBACK		0x4000 /* 1 = loopback at the PHY */
#define FXP_PHY_BMCR_RESET		0x8000 /* ==> 1 sets to defaults */

/*
 * Basic Mode Status Register (National name)
 * Management Data Interface Status reg. (Intel name)
 * in both Intel and National parts.
 */
#define FXP_PHY_STS			0x1
#define FXP_PHY_STS_EXND		0x0001 /* Extended regs enabled */
#define FXP_PHY_STS_JABR		0x0002 /* Jabber detected */
#define FXP_PHY_STS_LINK_STS		0x0004 /* Link valid */
#define FXP_PHY_STS_CAN_AUTO		0x0008 /* Auto detection available */
#define FXP_PHY_STS_REMT_FAULT		0x0010 /* remote fault detected */
#define FXP_PHY_STS_AUTO_DONE		0x0020 /* auto negotiation completed */
#define FXP_PHY_STS_MGMT_PREAMBLE	0x0040 /* real complicated */
#define FXP_PHY_STS_10HDX_OK		0x0800 /* can do 10Mb HDX */
#define FXP_PHY_STS_10FDX_OK		0x1000 /* can do 10Mb FDX */
#define FXP_PHY_STS_100HDX_OK		0x2000 /* can do 100Mb HDX */
#define FXP_PHY_STS_100FDX_OK		0x4000 /* can do 100Mb FDX */
#define FXP_PHY_STS_100T4_OK		0x8000 /* can do 100bT4 -not Intel */

/*
 * More Phy regs
 */
#define FXP_PHY_ID1			0x2
#define FXP_PHY_ID2			0x3

/*
 * MDI Auto negotiation advertisement register.
 * What we advertise we can do..
 * The same bits are used to indicate the response too.
 */
#define FXP_PHY_ADVRT			0x4
#define FXP_PHY_RMT_ADVRT		0x5 /* what the other end said */
#define FXP_PHY_ADVRT_SELECT		0x001F /* real complicated */
#define FXP_PHY_ADVRT_TECH_AVAIL	0x1FE0 /* can do 10Mb HDX */
#define FXP_PHY_ADVRT_RMT_FAULT		0x2000 /* can do 10Mb FDX */
#define FXP_PHY_ADVRT_ACK		0x4000 /* Acked */
#define FXP_PHY_ADVRT_NXT_PAGE		0x8000 /* can do 100Mb FDX */

/*
 * Phy Unit Status and Control Register (another one)
 * This is not in the National part!
 */
#define FXP_PHY_USC			0x10
#define FXP_PHY_USC_DUPLEX		0x0001 /* in FDX mode */
#define FXP_PHY_USC_SPEED		0x0002 /* 1 = in 100Mb mode */
#define FXP_PHY_USC_POLARITY		0x0100 /* 1 = reverse polarity */
#define FXP_PHY_USC_10_PWRDOWN		0x0200 /* 10Mb PHY powered down */
#define FXP_PHY_USC_100_PWRDOWN		0x0400 /* 100Mb PHY powered down */
#define FXP_PHY_USC_INSYNC		0x0800 /* 100Mb PHY is in sync */
#define FXP_PHY_USC_TX_FLOWCNTRL	0x1000 /* TX FC mode in use */
#define FXP_PHY_USC_PHY_FLOWCNTRL	0x8000 /* PHY FC mode in use */


/*
 * DP83830 PHY, PCS Configuration Register
 * NOT compatible with Intel parts,
 * (where it is the 100BTX premature eof counter).
 */
#define FXP_DP83840_PCR			0x17
#define FXP_DP83840_PCR_LED4_MODE	0x0002	/* 1 = LED4 always = FDX */
#define FXP_DP83840_PCR_F_CONNECT	0x0020	/* 1 = link disconnect bypass */
#define FXP_DP83840_PCR_BIT8		0x0100
#define FXP_DP83840_PCR_BIT10		0x0400

/*
 * DP83830 PHY, Address/status Register
 * NOT compatible with Intel parts,
 * (where it is the 10BT jabber detect counter).
 */
#define FXP_DP83840_PAR			0x19
#define FXP_DP83840_PAR_PHYADDR		0x1F
#define FXP_DP83840_PAR_CON_STATUS	0x20	
#define FXP_DP83840_PAR_SPEED_10	0x40	/* 1 == running at 10 Mb/Sec */

