/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
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
 */

/*
 *  FileName :    xgehal-regs.h
 *
 *  Description:  Xframe mem-mapped register space
 *
 *  Created:      14 May 2004
 */

#ifndef XGE_HAL_REGS_H
#define XGE_HAL_REGS_H

typedef struct {

/* General Control-Status Registers */
	u64 general_int_status;
#define XGE_HAL_GEN_INTR_TXPIC             BIT(0)
#define XGE_HAL_GEN_INTR_TXDMA             BIT(1)
#define XGE_HAL_GEN_INTR_TXMAC             BIT(2)
#define XGE_HAL_GEN_INTR_TXXGXS            BIT(3)
#define XGE_HAL_GEN_INTR_TXTRAFFIC         BIT(8)
#define XGE_HAL_GEN_INTR_RXPIC             BIT(32)
#define XGE_HAL_GEN_INTR_RXDMA             BIT(33)
#define XGE_HAL_GEN_INTR_RXMAC             BIT(34)
#define XGE_HAL_GEN_INTR_MC                BIT(35)
#define XGE_HAL_GEN_INTR_RXXGXS            BIT(36)
#define XGE_HAL_GEN_INTR_RXTRAFFIC         BIT(40)
#define XGE_HAL_GEN_ERROR_INTR             (XGE_HAL_GEN_INTR_TXPIC  | \
					 XGE_HAL_GEN_INTR_RXPIC  | \
					 XGE_HAL_GEN_INTR_TXDMA  | \
					 XGE_HAL_GEN_INTR_RXDMA  | \
					 XGE_HAL_GEN_INTR_TXMAC  | \
					 XGE_HAL_GEN_INTR_RXMAC  | \
					 XGE_HAL_GEN_INTR_TXXGXS | \
					 XGE_HAL_GEN_INTR_RXXGXS | \
					 XGE_HAL_GEN_INTR_MC)

	u64 general_int_mask;

	u8 unused0[0x100 - 0x10];

	u64 sw_reset;

/* XGXS must be removed from reset only once. */
#define XGE_HAL_SW_RESET_XENA              vBIT(0xA5,0,8)
#define XGE_HAL_SW_RESET_FLASH             vBIT(0xA5,8,8)
#define XGE_HAL_SW_RESET_EOI               vBIT(0xA5,16,8)
#define XGE_HAL_SW_RESET_XGXS              vBIT(0xA5,24,8)
#define XGE_HAL_SW_RESET_ALL               (XGE_HAL_SW_RESET_XENA  | \
					    XGE_HAL_SW_RESET_FLASH | \
					    XGE_HAL_SW_RESET_EOI | \
					    XGE_HAL_SW_RESET_XGXS)

/* The SW_RESET register must read this value after a successful reset. */
#if defined(XGE_OS_HOST_BIG_ENDIAN) && !defined(XGE_OS_PIO_LITTLE_ENDIAN)
#define XGE_HAL_SW_RESET_RAW_VAL_XENA			0xA500000000ULL
#define XGE_HAL_SW_RESET_RAW_VAL_HERC			0xA5A500000000ULL
#else
#define XGE_HAL_SW_RESET_RAW_VAL_XENA			0xA5000000ULL
#define XGE_HAL_SW_RESET_RAW_VAL_HERC			0xA5A50000ULL
#endif


	u64 adapter_status;
#define XGE_HAL_ADAPTER_STATUS_TDMA_READY          BIT(0)
#define XGE_HAL_ADAPTER_STATUS_RDMA_READY          BIT(1)
#define XGE_HAL_ADAPTER_STATUS_PFC_READY           BIT(2)
#define XGE_HAL_ADAPTER_STATUS_TMAC_BUF_EMPTY      BIT(3)
#define XGE_HAL_ADAPTER_STATUS_PIC_QUIESCENT       BIT(5)
#define XGE_HAL_ADAPTER_STATUS_RMAC_REMOTE_FAULT   BIT(6)
#define XGE_HAL_ADAPTER_STATUS_RMAC_LOCAL_FAULT    BIT(7)
#define XGE_HAL_ADAPTER_STATUS_RMAC_PCC_IDLE       vBIT(0xFF,8,8)
#define XGE_HAL_ADAPTER_STATUS_RMAC_PCC_4_IDLE     vBIT(0x0F,8,8)
#define XGE_HAL_ADAPTER_PCC_ENABLE_FOUR            vBIT(0x0F,0,8)

#define XGE_HAL_ADAPTER_STATUS_RC_PRC_QUIESCENT    vBIT(0xFF,16,8)
#define XGE_HAL_ADAPTER_STATUS_MC_DRAM_READY       BIT(24)
#define XGE_HAL_ADAPTER_STATUS_MC_QUEUES_READY     BIT(25)
#define XGE_HAL_ADAPTER_STATUS_M_PLL_LOCK          BIT(30)
#define XGE_HAL_ADAPTER_STATUS_P_PLL_LOCK          BIT(31)

	u64 adapter_control;
#define XGE_HAL_ADAPTER_CNTL_EN                    BIT(7)
#define XGE_HAL_ADAPTER_EOI_TX_ON                  BIT(15)
#define XGE_HAL_ADAPTER_LED_ON                     BIT(23)
#define XGE_HAL_ADAPTER_UDPI(val)                  vBIT(val,36,4)
#define XGE_HAL_ADAPTER_WAIT_INT                   BIT(48)
#define XGE_HAL_ADAPTER_ECC_EN                     BIT(55)

	u64 serr_source;
#define XGE_HAL_SERR_SOURCE_PIC	                BIT(0)
#define XGE_HAL_SERR_SOURCE_TXDMA               BIT(1)
#define XGE_HAL_SERR_SOURCE_RXDMA               BIT(2)
#define XGE_HAL_SERR_SOURCE_MAC			BIT(3)
#define XGE_HAL_SERR_SOURCE_MC			BIT(4)
#define XGE_HAL_SERR_SOURCE_XGXS			 BIT(5)
#define XGE_HAL_SERR_SOURCE_ANY		(XGE_HAL_SERR_SOURCE_PIC   | \
					 XGE_HAL_SERR_SOURCE_TXDMA | \
					 XGE_HAL_SERR_SOURCE_RXDMA | \
					 XGE_HAL_SERR_SOURCE_MAC   | \
					 XGE_HAL_SERR_SOURCE_MC    | \
					 XGE_HAL_SERR_SOURCE_XGXS)

	u64	pci_info;
#define XGE_HAL_PCI_INFO			vBIT(0xF,0,4)
#define XGE_HAL_PCI_32_BIT			BIT(8)

	u8 unused0_1[0x160 - 0x128];
 
	u64 ric_status;

	u8  unused0_2[0x558 - 0x168];

	u64 mbist_status;

	u8  unused0_3[0x800 - 0x560];

/* PCI-X Controller registers */
	u64 pic_int_status;
	u64 pic_int_mask;
#define XGE_HAL_PIC_INT_TX                     BIT(0)
#define XGE_HAL_PIC_INT_FLSH                   BIT(1)
#define XGE_HAL_PIC_INT_MDIO                   BIT(2)
#define XGE_HAL_PIC_INT_IIC                    BIT(3)
#define XGE_HAL_PIC_INT_MISC                   BIT(4)
#define XGE_HAL_PIC_INT_RX                     BIT(32)

	u64 txpic_int_reg;
#define XGE_HAL_TXPIC_INT_SCHED_INTR            BIT(42)
	u64 txpic_int_mask;
#define XGE_HAL_PCIX_INT_REG_ECC_SG_ERR                BIT(0)
#define XGE_HAL_PCIX_INT_REG_ECC_DB_ERR                BIT(1)
#define XGE_HAL_PCIX_INT_REG_FLASHR_R_FSM_ERR          BIT(8)
#define XGE_HAL_PCIX_INT_REG_FLASHR_W_FSM_ERR          BIT(9)
#define XGE_HAL_PCIX_INT_REG_INI_TX_FSM_SERR           BIT(10)
#define XGE_HAL_PCIX_INT_REG_INI_TXO_FSM_ERR           BIT(11)
#define XGE_HAL_PCIX_INT_REG_TRT_FSM_SERR              BIT(13)
#define XGE_HAL_PCIX_INT_REG_SRT_FSM_SERR              BIT(14)
#define XGE_HAL_PCIX_INT_REG_PIFR_FSM_SERR             BIT(15)
#define XGE_HAL_PCIX_INT_REG_WRC_TX_SEND_FSM_SERR      BIT(21)
#define XGE_HAL_PCIX_INT_REG_RRC_TX_REQ_FSM_SERR       BIT(23)
#define XGE_HAL_PCIX_INT_REG_INI_RX_FSM_SERR           BIT(48)
#define XGE_HAL_PCIX_INT_REG_RA_RX_FSM_SERR            BIT(50)
/*
#define XGE_HAL_PCIX_INT_REG_WRC_RX_SEND_FSM_SERR      BIT(52)
#define XGE_HAL_PCIX_INT_REG_RRC_RX_REQ_FSM_SERR       BIT(54)
#define XGE_HAL_PCIX_INT_REG_RRC_RX_SPLIT_FSM_SERR     BIT(58)
*/
	u64 txpic_alarms;
	u64 rxpic_int_reg;
#define XGE_HAL_RX_PIC_INT_REG_SPDM_READY               BIT(0)
#define XGE_HAL_RX_PIC_INT_REG_SPDM_OVERWRITE_ERR       BIT(44)
#define XGE_HAL_RX_PIC_INT_REG_SPDM_PERR                BIT(55)
	u64 rxpic_int_mask;
	u64 rxpic_alarms;

	u64 flsh_int_reg;
	u64 flsh_int_mask;
#define XGE_HAL_PIC_FLSH_INT_REG_CYCLE_FSM_ERR          BIT(63)
#define XGE_HAL_PIC_FLSH_INT_REG_ERR                    BIT(62)
	u64 flash_alarms;

	u64 mdio_int_reg;
	u64 mdio_int_mask;
#define XGE_HAL_MDIO_INT_REG_MDIO_BUS_ERR              BIT(0)
#define XGE_HAL_MDIO_INT_REG_DTX_BUS_ERR               BIT(8)
#define XGE_HAL_MDIO_INT_REG_LASI                      BIT(39)
	u64 mdio_alarms;

	u64 iic_int_reg;
	u64 iic_int_mask;
#define XGE_HAL_IIC_INT_REG_BUS_FSM_ERR                BIT(4)
#define XGE_HAL_IIC_INT_REG_BIT_FSM_ERR                BIT(5)
#define XGE_HAL_IIC_INT_REG_CYCLE_FSM_ERR              BIT(6)
#define XGE_HAL_IIC_INT_REG_REQ_FSM_ERR                BIT(7)
#define XGE_HAL_IIC_INT_REG_ACK_ERR                    BIT(8)
	u64 iic_alarms;

	u64 msi_pending_reg;

	u64 misc_int_reg;
#define XGE_HAL_MISC_INT_REG_DP_ERR_INT			BIT(0)
#define XGE_HAL_MISC_INT_REG_LINK_DOWN_INT		BIT(1)
#define XGE_HAL_MISC_INT_REG_LINK_UP_INT		BIT(2)
	u64 misc_int_mask;
	u64 misc_alarms;

	u64 msi_triggered_reg;

	u64 xfp_gpio_int_reg;
	u64 xfp_gpio_int_mask;
	u64 xfp_alarms;

	u8  unused5[0x8E0 - 0x8C8];

	u64 tx_traffic_int;
#define XGE_HAL_TX_TRAFFIC_INT_n(n)                     BIT(n)
	u64 tx_traffic_mask;

	u64 rx_traffic_int;
#define XGE_HAL_RX_TRAFFIC_INT_n(n)                     BIT(n)
	u64 rx_traffic_mask;

/* PIC Control registers */
	u64 pic_control;
#define XGE_HAL_PIC_CNTL_RX_ALARM_MAP_1                BIT(0)
#define XGE_HAL_PIC_CNTL_ONE_SHOT_TINT                 BIT(1)
#define XGE_HAL_PIC_CNTL_SHARED_SPLITS(n)              vBIT(n,11,4)

	u64 swapper_ctrl;
#define XGE_HAL_SWAPPER_CTRL_PIF_R_FE                  BIT(0)
#define XGE_HAL_SWAPPER_CTRL_PIF_R_SE                  BIT(1)
#define XGE_HAL_SWAPPER_CTRL_PIF_W_FE                  BIT(8)
#define XGE_HAL_SWAPPER_CTRL_PIF_W_SE                  BIT(9)
#define XGE_HAL_SWAPPER_CTRL_RTH_FE                    BIT(10)
#define XGE_HAL_SWAPPER_CTRL_RTH_SE                    BIT(11)
#define XGE_HAL_SWAPPER_CTRL_TXP_FE                    BIT(16)
#define XGE_HAL_SWAPPER_CTRL_TXP_SE                    BIT(17)
#define XGE_HAL_SWAPPER_CTRL_TXD_R_FE                  BIT(18)
#define XGE_HAL_SWAPPER_CTRL_TXD_R_SE                  BIT(19)
#define XGE_HAL_SWAPPER_CTRL_TXD_W_FE                  BIT(20)
#define XGE_HAL_SWAPPER_CTRL_TXD_W_SE                  BIT(21)
#define XGE_HAL_SWAPPER_CTRL_TXF_R_FE                  BIT(22)
#define XGE_HAL_SWAPPER_CTRL_TXF_R_SE                  BIT(23)
#define XGE_HAL_SWAPPER_CTRL_RXD_R_FE                  BIT(32)
#define XGE_HAL_SWAPPER_CTRL_RXD_R_SE                  BIT(33)
#define XGE_HAL_SWAPPER_CTRL_RXD_W_FE                  BIT(34)
#define XGE_HAL_SWAPPER_CTRL_RXD_W_SE                  BIT(35)
#define XGE_HAL_SWAPPER_CTRL_RXF_W_FE                  BIT(36)
#define XGE_HAL_SWAPPER_CTRL_RXF_W_SE                  BIT(37)
#define XGE_HAL_SWAPPER_CTRL_XMSI_FE                   BIT(40)
#define XGE_HAL_SWAPPER_CTRL_XMSI_SE                   BIT(41)
#define XGE_HAL_SWAPPER_CTRL_STATS_FE                  BIT(48)
#define XGE_HAL_SWAPPER_CTRL_STATS_SE                  BIT(49)

	u64 pif_rd_swapper_fb;
#define XGE_HAL_IF_RD_SWAPPER_FB   0x0123456789ABCDEFULL

	u64 scheduled_int_ctrl;
#define XGE_HAL_SCHED_INT_CTRL_TIMER_EN                BIT(0)
#define XGE_HAL_SCHED_INT_CTRL_ONE_SHOT                BIT(1)
#define XGE_HAL_SCHED_INT_CTRL_INT2MSI(val)	     vBIT(val,10,6)
#define XGE_HAL_SCHED_INT_PERIOD(val)		     vBIT(val,32,32)
#define XGE_HAL_SCHED_INT_PERIOD_MASK		     0xFFFFFFFF00000000ULL


	u64 txreqtimeout;
#define XGE_HAL_TXREQTO_VAL(val)		vBIT(val,0,32)
#define XGE_HAL_TXREQTO_EN			BIT(63)

	u64 statsreqtimeout;
#define XGE_HAL_STATREQTO_VAL(n)                  TBD
#define XGE_HAL_STATREQTO_EN                      BIT(63)

	u64 read_retry_delay;
	u64 read_retry_acceleration;
	u64 write_retry_delay;
	u64 write_retry_acceleration;

	u64 xmsi_control;
#define XGE_HAL_XMSI_EN				BIT(0)
#define XGE_HAL_XMSI_DIS_TINT_SERR		BIT(1)
#define XGE_HAL_XMSI_BYTE_COUNT(val)		vBIT(val,13,3)

	u64 xmsi_access;
#define XGE_HAL_XMSI_WR_RDN			BIT(7)
#define XGE_HAL_XMSI_STROBE			BIT(15)
#define XGE_HAL_XMSI_NO(val)			vBIT(val,26,6)

	u64 xmsi_address;
	u64 xmsi_data;

	u64 rx_mat;
#define XGE_HAL_SET_RX_MAT(ring, msi)	vBIT(msi, (8 * ring), 8)

	u8 unused6[0x8];

	u64 tx_mat[8];
#define XGE_HAL_SET_TX_MAT(fifo, msi)	vBIT(msi, (8 * fifo), 8)

	u64 xmsi_mask_reg;

	/* Automated statistics collection */
	u64 stat_byte_cnt;
	u64 stat_cfg;
#define XGE_HAL_STAT_CFG_STAT_EN           BIT(0)
#define XGE_HAL_STAT_CFG_ONE_SHOT_EN       BIT(1)
#define XGE_HAL_STAT_CFG_STAT_NS_EN        BIT(8)
#define XGE_HAL_STAT_CFG_STAT_RO           BIT(9)
#define XGE_HAL_XENA_PER_SEC	           0x208d5
#define XGE_HAL_SET_UPDT_PERIOD(n)	   vBIT(n,32,32)

	u64 stat_addr;

	/* General Configuration */
	u64 mdio_control;
#define XGE_HAL_MDIO_CONTROL_MMD_INDX_ADDR(n)	vBIT(n,0,16)
#define XGE_HAL_MDIO_CONTROL_MMD_DEV_ADDR(n)	vBIT(n,19,5)
#define XGE_HAL_MDIO_CONTROL_MMD_PRT_ADDR(n)	vBIT(n,27,5)
#define XGE_HAL_MDIO_CONTROL_MMD_DATA(n)	vBIT(n,32,16)
#define XGE_HAL_MDIO_CONTROL_MMD_CTRL(n)	vBIT(n,56,4)
#define XGE_HAL_MDIO_CONTROL_MMD_OP(n)		vBIT(n,60,2)
#define XGE_HAL_MDIO_CONTROL_MMD_DATA_GET(n)	((n>>16)&0xFFFF)
#define XGE_HAL_MDIO_MMD_PMA_DEV_ADDR		0x01
#define XGE_HAL_MDIO_DOM_REG_ADDR		0xA100
#define XGE_HAL_MDIO_ALARM_FLAGS_ADDR		0xA070
#define XGE_HAL_MDIO_WARN_FLAGS_ADDR		0xA074
#define XGE_HAL_MDIO_CTRL_START			0xE
#define XGE_HAL_MDIO_OP_ADDRESS			0x0
#define XGE_HAL_MDIO_OP_WRITE			0x1
#define XGE_HAL_MDIO_OP_READ			0x3
#define XGE_HAL_MDIO_OP_READ_POST_INCREMENT	0x2
#define XGE_HAL_MDIO_ALARM_TEMPHIGH		0x0080
#define XGE_HAL_MDIO_ALARM_TEMPLOW		0x0040
#define XGE_HAL_MDIO_ALARM_BIASHIGH		0x0008
#define XGE_HAL_MDIO_ALARM_BIASLOW		0x0004
#define XGE_HAL_MDIO_ALARM_POUTPUTHIGH		0x0002
#define XGE_HAL_MDIO_ALARM_POUTPUTLOW		0x0001
#define XGE_HAL_MDIO_WARN_TEMPHIGH		0x0080
#define XGE_HAL_MDIO_WARN_TEMPLOW		0x0040
#define XGE_HAL_MDIO_WARN_BIASHIGH		0x0008
#define XGE_HAL_MDIO_WARN_BIASLOW		0x0004
#define XGE_HAL_MDIO_WARN_POUTPUTHIGH		0x0002
#define XGE_HAL_MDIO_WARN_POUTPUTLOW		0x0001

	u64 dtx_control;

	u64 i2c_control;
#define XGE_HAL_I2C_CONTROL_DEV_ID(id)		vBIT(id,1,3)
#define XGE_HAL_I2C_CONTROL_ADDR(addr)		vBIT(addr,5,11)
#define XGE_HAL_I2C_CONTROL_BYTE_CNT(cnt)	vBIT(cnt,22,2)
#define XGE_HAL_I2C_CONTROL_READ		BIT(24)
#define XGE_HAL_I2C_CONTROL_NACK		BIT(25)
#define XGE_HAL_I2C_CONTROL_CNTL_START		vBIT(0xE,28,4)
#define XGE_HAL_I2C_CONTROL_CNTL_END(val)	(val & vBIT(0x1,28,4))
#define XGE_HAL_I2C_CONTROL_GET_DATA(val)	(u32)(val & 0xFFFFFFFF)
#define XGE_HAL_I2C_CONTROL_SET_DATA(val)	vBIT(val,32,32)

	u64 beacon_control;
	u64 misc_control;
#define XGE_HAL_MISC_CONTROL_LINK_STABILITY_PERIOD(val)	vBIT(val,29,3)
#define XGE_HAL_MISC_CONTROL_EXT_REQ_EN     BIT(1)
#define XGE_HAL_MISC_CONTROL_LINK_FAULT		BIT(0)

	u64 xfb_control;
	u64 gpio_control;
#define XGE_HAL_GPIO_CTRL_GPIO_0           	BIT(8)

	u64 txfifo_dw_mask;
	u64 split_table_line_no;
	u64 sc_timeout;
	u64 pic_control_2;
#define XGE_HAL_TXD_WRITE_BC(n)                 vBIT(n, 13, 3)
	u64 ini_dperr_ctrl;
	u64 wreq_split_mask;
	u64 qw_per_rxd;
	u8  unused7[0x300 - 0x250];

	u64 pic_status;
	u64 txp_status;
	u64 txp_err_context;
	u64 spdm_bir_offset;
#define XGE_HAL_SPDM_PCI_BAR_NUM(spdm_bir_offset)	\
				(u8)(spdm_bir_offset >> 61)
#define XGE_HAL_SPDM_PCI_BAR_OFFSET(spdm_bir_offset) \
				(u32)((spdm_bir_offset >> 32) & 0x1FFFFFFF)
	u64 spdm_overwrite;
#define XGE_HAL_SPDM_OVERWRITE_ERR_SPDM_ENTRY(spdm_overwrite)  \
				(u8)((spdm_overwrite >> 48) & 0xff)
#define XGE_HAL_SPDM_OVERWRITE_ERR_SPDM_DW(spdm_overwrite)  \
				(u8)((spdm_overwrite >> 40) & 0x3)
#define XGE_HAL_SPDM_OVERWRITE_ERR_SPDM_LINE(spdm_overwrite)  \
				(u8)((spdm_overwrite >> 32) & 0x7)
	u64 cfg_addr_on_dperr;
	u64 pif_addr_on_dperr;
	u64 tags_in_use;
	u64 rd_req_types;
	u64 split_table_line;
	u64 unxp_split_add_ph;
	u64 unexp_split_attr_ph;
	u64 split_message;
	u64 spdm_structure;
#define XGE_HAL_SPDM_MAX_ENTRIES(spdm_structure)  (u16)(spdm_structure >> 48)
#define XGE_HAL_SPDM_INT_QW_PER_ENTRY(spdm_structure)  \
				(u8)((spdm_structure >> 40) & 0xff)
#define XGE_HAL_SPDM_PCI_QW_PER_ENTRY(spdm_structure)  \
				(u8)((spdm_structure >> 32) & 0xff)

	u64 txdw_ptr_cnt_0;
	u64 txdw_ptr_cnt_1;
	u64 txdw_ptr_cnt_2;
	u64 txdw_ptr_cnt_3;
	u64 txdw_ptr_cnt_4;
	u64 txdw_ptr_cnt_5;
	u64 txdw_ptr_cnt_6;
	u64 txdw_ptr_cnt_7;
	u64 rxdw_cnt_ring_0;
	u64 rxdw_cnt_ring_1;
	u64 rxdw_cnt_ring_2;
	u64 rxdw_cnt_ring_3;
	u64 rxdw_cnt_ring_4;
	u64 rxdw_cnt_ring_5;
	u64 rxdw_cnt_ring_6;
	u64 rxdw_cnt_ring_7;

	u8  unused8[0x410];

/* TxDMA registers */
	u64 txdma_int_status;
	u64 txdma_int_mask;
#define XGE_HAL_TXDMA_PFC_INT			BIT(0)
#define XGE_HAL_TXDMA_TDA_INT			BIT(1)
#define XGE_HAL_TXDMA_PCC_INT			BIT(2)
#define XGE_HAL_TXDMA_TTI_INT			BIT(3)
#define XGE_HAL_TXDMA_LSO_INT			BIT(4)
#define XGE_HAL_TXDMA_TPA_INT			BIT(5)
#define XGE_HAL_TXDMA_SM_INT			BIT(6)
	u64 pfc_err_reg;
#define XGE_HAL_PFC_ECC_SG_ERR			BIT(7)
#define XGE_HAL_PFC_ECC_DB_ERR			BIT(15)
#define XGE_HAL_PFC_SM_ERR_ALARM		BIT(23)
#define XGE_HAL_PFC_MISC_0_ERR			BIT(31)
#define XGE_HAL_PFC_MISC_1_ERR			BIT(32)
#define XGE_HAL_PFC_PCIX_ERR			BIT(39)
	u64 pfc_err_mask;
	u64 pfc_err_alarm;

	u64 tda_err_reg;
#define XGE_HAL_TDA_Fn_ECC_SG_ERR		vBIT(0xff,0,8)
#define XGE_HAL_TDA_Fn_ECC_DB_ERR		vBIT(0xff,8,8)
#define XGE_HAL_TDA_SM0_ERR_ALARM		BIT(22)
#define XGE_HAL_TDA_SM1_ERR_ALARM		BIT(23)
#define XGE_HAL_TDA_PCIX_ERR			BIT(39)
	u64 tda_err_mask;
	u64 tda_err_alarm;

	u64 pcc_err_reg;
#define XGE_HAL_PCC_FB_ECC_SG_ERR		vBIT(0xFF,0,8)
#define XGE_HAL_PCC_TXB_ECC_SG_ERR		vBIT(0xFF,8,8)
#define XGE_HAL_PCC_FB_ECC_DB_ERR		vBIT(0xFF,16, 8)
#define XGE_HAL_PCC_TXB_ECC_DB_ERR		vBIT(0xff,24,8)
#define XGE_HAL_PCC_SM_ERR_ALARM		vBIT(0xff,32,8)
#define XGE_HAL_PCC_WR_ERR_ALARM		vBIT(0xff,40,8)
#define XGE_HAL_PCC_N_SERR			vBIT(0xff,48,8)
#define XGE_HAL_PCC_ENABLE_FOUR			vBIT(0x0F,0,8)
#define XGE_HAL_PCC_6_COF_OV_ERR		BIT(56)
#define XGE_HAL_PCC_7_COF_OV_ERR		BIT(57)
#define XGE_HAL_PCC_6_LSO_OV_ERR		BIT(58)
#define XGE_HAL_PCC_7_LSO_OV_ERR		BIT(59)
	u64 pcc_err_mask;
	u64 pcc_err_alarm;

	u64 tti_err_reg;
#define XGE_HAL_TTI_ECC_SG_ERR			BIT(7)
#define XGE_HAL_TTI_ECC_DB_ERR			BIT(15)
#define XGE_HAL_TTI_SM_ERR_ALARM		BIT(23)
	u64 tti_err_mask;
	u64 tti_err_alarm;

	u64 lso_err_reg;
#define XGE_HAL_LSO6_SEND_OFLOW			BIT(12)
#define XGE_HAL_LSO7_SEND_OFLOW			BIT(13)
#define XGE_HAL_LSO6_ABORT			BIT(14)
#define XGE_HAL_LSO7_ABORT			BIT(15)
#define XGE_HAL_LSO6_SM_ERR_ALARM		BIT(22)
#define XGE_HAL_LSO7_SM_ERR_ALARM		BIT(23)
	u64 lso_err_mask;
	u64 lso_err_alarm;

	u64 tpa_err_reg;
#define XGE_HAL_TPA_TX_FRM_DROP			BIT(7)
#define XGE_HAL_TPA_SM_ERR_ALARM		BIT(23)
	u64 tpa_err_mask;
	u64 tpa_err_alarm;

	u64 sm_err_reg;
#define XGE_HAL_SM_SM_ERR_ALARM			BIT(15)
	u64 sm_err_mask;
	u64 sm_err_alarm;

	u8 unused9[0x100 - 0xB8];

/* TxDMA arbiter */
	u64 tx_dma_wrap_stat;

/* Tx FIFO controller */
#define XGE_HAL_X_MAX_FIFOS                        8
#define XGE_HAL_X_FIFO_MAX_LEN                     0x1FFF	/*8191 */
	u64 tx_fifo_partition_0;
#define XGE_HAL_TX_FIFO_PARTITION_EN               BIT(0)
#define XGE_HAL_TX_FIFO_PARTITION_0_PRI(val)       vBIT(val,5,3)
#define XGE_HAL_TX_FIFO_PARTITION_0_LEN(val)       vBIT(val,19,13)
#define XGE_HAL_TX_FIFO_PARTITION_1_PRI(val)       vBIT(val,37,3)
#define XGE_HAL_TX_FIFO_PARTITION_1_LEN(val)       vBIT(val,51,13  )

	u64 tx_fifo_partition_1;
#define XGE_HAL_TX_FIFO_PARTITION_2_PRI(val)       vBIT(val,5,3)
#define XGE_HAL_TX_FIFO_PARTITION_2_LEN(val)       vBIT(val,19,13)
#define XGE_HAL_TX_FIFO_PARTITION_3_PRI(val)       vBIT(val,37,3)
#define XGE_HAL_TX_FIFO_PARTITION_3_LEN(val)       vBIT(val,51,13)

	u64 tx_fifo_partition_2;
#define XGE_HAL_TX_FIFO_PARTITION_4_PRI(val)       vBIT(val,5,3)
#define XGE_HAL_TX_FIFO_PARTITION_4_LEN(val)       vBIT(val,19,13)
#define XGE_HAL_TX_FIFO_PARTITION_5_PRI(val)       vBIT(val,37,3)
#define XGE_HAL_TX_FIFO_PARTITION_5_LEN(val)       vBIT(val,51,13)

	u64 tx_fifo_partition_3;
#define XGE_HAL_TX_FIFO_PARTITION_6_PRI(val)       vBIT(val,5,3)
#define XGE_HAL_TX_FIFO_PARTITION_6_LEN(val)       vBIT(val,19,13)
#define XGE_HAL_TX_FIFO_PARTITION_7_PRI(val)       vBIT(val,37,3)
#define XGE_HAL_TX_FIFO_PARTITION_7_LEN(val)       vBIT(val,51,13)

#define XGE_HAL_TX_FIFO_PARTITION_PRI_0            0	/* highest */
#define XGE_HAL_TX_FIFO_PARTITION_PRI_1            1
#define XGE_HAL_TX_FIFO_PARTITION_PRI_2            2
#define XGE_HAL_TX_FIFO_PARTITION_PRI_3            3
#define XGE_HAL_TX_FIFO_PARTITION_PRI_4            4
#define XGE_HAL_TX_FIFO_PARTITION_PRI_5            5
#define XGE_HAL_TX_FIFO_PARTITION_PRI_6            6
#define XGE_HAL_TX_FIFO_PARTITION_PRI_7            7	/* lowest */

	u64 tx_w_round_robin_0;
	u64 tx_w_round_robin_1;
	u64 tx_w_round_robin_2;
	u64 tx_w_round_robin_3;
	u64 tx_w_round_robin_4;

	u64 tti_command_mem;
#define XGE_HAL_TTI_CMD_MEM_WE                     BIT(7)
#define XGE_HAL_TTI_CMD_MEM_STROBE_NEW_CMD         BIT(15)
#define XGE_HAL_TTI_CMD_MEM_STROBE_BEING_EXECUTED  BIT(15)
#define XGE_HAL_TTI_CMD_MEM_OFFSET(n)              vBIT(n,26,6)

	u64 tti_data1_mem;
#define XGE_HAL_TTI_DATA1_MEM_TX_TIMER_VAL(n)      vBIT(n,6,26)
#define XGE_HAL_TTI_DATA1_MEM_TX_TIMER_AC_CI(n)    vBIT(n,38,2)
#define XGE_HAL_TTI_DATA1_MEM_TX_TIMER_AC_EN       BIT(38)
#define XGE_HAL_TTI_DATA1_MEM_TX_TIMER_CI_EN       BIT(39)
#define XGE_HAL_TTI_DATA1_MEM_TX_URNG_A(n)         vBIT(n,41,7)
#define XGE_HAL_TTI_DATA1_MEM_TX_URNG_B(n)         vBIT(n,49,7)
#define XGE_HAL_TTI_DATA1_MEM_TX_URNG_C(n)         vBIT(n,57,7)

	u64 tti_data2_mem;
#define XGE_HAL_TTI_DATA2_MEM_TX_UFC_A(n)          vBIT(n,0,16)
#define XGE_HAL_TTI_DATA2_MEM_TX_UFC_B(n)          vBIT(n,16,16)
#define XGE_HAL_TTI_DATA2_MEM_TX_UFC_C(n)          vBIT(n,32,16)
#define XGE_HAL_TTI_DATA2_MEM_TX_UFC_D(n)          vBIT(n,48,16)

/* Tx Protocol assist */
	u64 tx_pa_cfg;
#define XGE_HAL_TX_PA_CFG_IGNORE_FRM_ERR           BIT(1)
#define XGE_HAL_TX_PA_CFG_IGNORE_SNAP_OUI          BIT(2)
#define XGE_HAL_TX_PA_CFG_IGNORE_LLC_CTRL          BIT(3)
#define XGE_HAL_TX_PA_CFG_IGNORE_L2_ERR		 BIT(6)

/* Recent add, used only debug purposes. */
	u64 pcc_enable;
	
	u64 pfc_monitor_0;
	u64 pfc_monitor_1;
	u64 pfc_monitor_2;
	u64 pfc_monitor_3;
	u64 txd_ownership_ctrl;
	u64 pfc_read_cntrl;
	u64 pfc_read_data;
	
	u8  unused10[0x1700 - 0x11B0];
	
	u64 txdma_debug_ctrl;

	u8 unused11[0x1800 - 0x1708];

/* RxDMA Registers */
	u64 rxdma_int_status;
#define XGE_HAL_RXDMA_RC_INT                   BIT(0)
#define XGE_HAL_RXDMA_RPA_INT                  BIT(1)
#define XGE_HAL_RXDMA_RDA_INT                  BIT(2)
#define XGE_HAL_RXDMA_RTI_INT                  BIT(3)

	u64 rxdma_int_mask;
#define XGE_HAL_RXDMA_INT_RC_INT_M             BIT(0)
#define XGE_HAL_RXDMA_INT_RPA_INT_M            BIT(1)
#define XGE_HAL_RXDMA_INT_RDA_INT_M            BIT(2)
#define XGE_HAL_RXDMA_INT_RTI_INT_M            BIT(3)

	u64 rda_err_reg;
#define XGE_HAL_RDA_RXDn_ECC_SG_ERR		vBIT(0xFF,0,8)
#define XGE_HAL_RDA_RXDn_ECC_DB_ERR		vBIT(0xFF,8,8)
#define XGE_HAL_RDA_FRM_ECC_SG_ERR		BIT(23)
#define XGE_HAL_RDA_FRM_ECC_DB_N_AERR		BIT(31)
#define XGE_HAL_RDA_SM1_ERR_ALARM		BIT(38)
#define XGE_HAL_RDA_SM0_ERR_ALARM		BIT(39)
#define XGE_HAL_RDA_MISC_ERR			BIT(47)
#define XGE_HAL_RDA_PCIX_ERR			BIT(55)
#define XGE_HAL_RDA_RXD_ECC_DB_SERR		BIT(63)
	u64 rda_err_mask;
	u64 rda_err_alarm;

	u64 rc_err_reg;
#define XGE_HAL_RC_PRCn_ECC_SG_ERR		vBIT(0xFF,0,8)
#define XGE_HAL_RC_PRCn_ECC_DB_ERR		vBIT(0xFF,8,8)
#define XGE_HAL_RC_FTC_ECC_SG_ERR		BIT(23)
#define XGE_HAL_RC_FTC_ECC_DB_ERR		BIT(31)
#define XGE_HAL_RC_PRCn_SM_ERR_ALARM		vBIT(0xFF,32,8)
#define XGE_HAL_RC_FTC_SM_ERR_ALARM		BIT(47)
#define XGE_HAL_RC_RDA_FAIL_WR_Rn		vBIT(0xFF,48,8)
	u64 rc_err_mask;
	u64 rc_err_alarm;

	u64 prc_pcix_err_reg;
#define XGE_HAL_PRC_PCI_AB_RD_Rn		vBIT(0xFF,0,8)
#define XGE_HAL_PRC_PCI_DP_RD_Rn		vBIT(0xFF,8,8)
#define XGE_HAL_PRC_PCI_AB_WR_Rn		vBIT(0xFF,16,8)
#define XGE_HAL_PRC_PCI_DP_WR_Rn		vBIT(0xFF,24,8)
#define XGE_HAL_PRC_PCI_AB_F_WR_Rn		vBIT(0xFF,32,8)
#define XGE_HAL_PRC_PCI_DP_F_WR_Rn		vBIT(0xFF,40,8)
	u64 prc_pcix_err_mask;
	u64 prc_pcix_err_alarm;

	u64 rpa_err_reg;
#define XGE_HAL_RPA_ECC_SG_ERR			BIT(7)
#define XGE_HAL_RPA_ECC_DB_ERR			BIT(15)
#define XGE_HAL_RPA_FLUSH_REQUEST		BIT(22)
#define XGE_HAL_RPA_SM_ERR_ALARM		BIT(23)
#define XGE_HAL_RPA_CREDIT_ERR			BIT(31)
	u64 rpa_err_mask;
	u64 rpa_err_alarm;

	u64 rti_err_reg;
#define XGE_HAL_RTI_ECC_SG_ERR			BIT(7)
#define XGE_HAL_RTI_ECC_DB_ERR			BIT(15)
#define XGE_HAL_RTI_SM_ERR_ALARM		BIT(23)
	u64 rti_err_mask;
	u64 rti_err_alarm;

	u8 unused12[0x100 - 0x88];

/* DMA arbiter */
	u64 rx_queue_priority;
#define XGE_HAL_RX_QUEUE_0_PRIORITY(val)       vBIT(val,5,3)
#define XGE_HAL_RX_QUEUE_1_PRIORITY(val)       vBIT(val,13,3)
#define XGE_HAL_RX_QUEUE_2_PRIORITY(val)       vBIT(val,21,3)
#define XGE_HAL_RX_QUEUE_3_PRIORITY(val)       vBIT(val,29,3)
#define XGE_HAL_RX_QUEUE_4_PRIORITY(val)       vBIT(val,37,3)
#define XGE_HAL_RX_QUEUE_5_PRIORITY(val)       vBIT(val,45,3)
#define XGE_HAL_RX_QUEUE_6_PRIORITY(val)       vBIT(val,53,3)
#define XGE_HAL_RX_QUEUE_7_PRIORITY(val)       vBIT(val,61,3)

#define XGE_HAL_RX_QUEUE_PRI_0                 0	/* highest */
#define XGE_HAL_RX_QUEUE_PRI_1                 1
#define XGE_HAL_RX_QUEUE_PRI_2                 2
#define XGE_HAL_RX_QUEUE_PRI_3                 3
#define XGE_HAL_RX_QUEUE_PRI_4                 4
#define XGE_HAL_RX_QUEUE_PRI_5                 5
#define XGE_HAL_RX_QUEUE_PRI_6                 6
#define XGE_HAL_RX_QUEUE_PRI_7                 7	/* lowest */

	u64 rx_w_round_robin_0;
	u64 rx_w_round_robin_1;
	u64 rx_w_round_robin_2;
	u64 rx_w_round_robin_3;
	u64 rx_w_round_robin_4;

	/* Per-ring controller regs */
#define XGE_HAL_RX_MAX_RINGS                8
	u64 prc_rxd0_n[XGE_HAL_RX_MAX_RINGS];
	u64 prc_ctrl_n[XGE_HAL_RX_MAX_RINGS];
#define XGE_HAL_PRC_CTRL_RC_ENABLED                    BIT(7)
#define XGE_HAL_PRC_CTRL_RING_MODE                     (BIT(14)|BIT(15))
#define XGE_HAL_PRC_CTRL_RING_MODE_1                   vBIT(0,14,2)
#define XGE_HAL_PRC_CTRL_RING_MODE_3                   vBIT(1,14,2)
#define XGE_HAL_PRC_CTRL_RING_MODE_5                   vBIT(2,14,2)
#define XGE_HAL_PRC_CTRL_RING_MODE_x                   vBIT(3,14,2)
#define XGE_HAL_PRC_CTRL_NO_SNOOP(n)                   vBIT(n,22,2)
#define XGE_HAL_PRC_CTRL_RTH_DISABLE                   BIT(31)
#define XGE_HAL_PRC_CTRL_BIMODAL_INTERRUPT             BIT(37)
#define XGE_HAL_PRC_CTRL_GROUP_READS                   BIT(38)
#define XGE_HAL_PRC_CTRL_RXD_BACKOFF_INTERVAL(val)     vBIT(val,40,24)

	u64 prc_alarm_action;
#define XGE_HAL_PRC_ALARM_ACTION_RR_R0_STOP            BIT(3)
#define XGE_HAL_PRC_ALARM_ACTION_RW_R0_STOP            BIT(7)
#define XGE_HAL_PRC_ALARM_ACTION_RR_R1_STOP            BIT(11)
#define XGE_HAL_PRC_ALARM_ACTION_RW_R1_STOP            BIT(15)
#define XGE_HAL_PRC_ALARM_ACTION_RR_R2_STOP            BIT(19)
#define XGE_HAL_PRC_ALARM_ACTION_RW_R2_STOP            BIT(23)
#define XGE_HAL_PRC_ALARM_ACTION_RR_R3_STOP            BIT(27)
#define XGE_HAL_PRC_ALARM_ACTION_RW_R3_STOP            BIT(31)
#define XGE_HAL_PRC_ALARM_ACTION_RR_R4_STOP            BIT(35)
#define XGE_HAL_PRC_ALARM_ACTION_RW_R4_STOP            BIT(39)
#define XGE_HAL_PRC_ALARM_ACTION_RR_R5_STOP            BIT(43)
#define XGE_HAL_PRC_ALARM_ACTION_RW_R5_STOP            BIT(47)
#define XGE_HAL_PRC_ALARM_ACTION_RR_R6_STOP            BIT(51)
#define XGE_HAL_PRC_ALARM_ACTION_RW_R6_STOP            BIT(55)
#define XGE_HAL_PRC_ALARM_ACTION_RR_R7_STOP            BIT(59)
#define XGE_HAL_PRC_ALARM_ACTION_RW_R7_STOP            BIT(63)

/* Receive traffic interrupts */
	u64 rti_command_mem;
#define XGE_HAL_RTI_CMD_MEM_WE                          BIT(7)
#define XGE_HAL_RTI_CMD_MEM_STROBE                      BIT(15)
#define XGE_HAL_RTI_CMD_MEM_STROBE_NEW_CMD              BIT(15)
#define XGE_HAL_RTI_CMD_MEM_STROBE_CMD_BEING_EXECUTED   BIT(15)
#define XGE_HAL_RTI_CMD_MEM_OFFSET(n)                   vBIT(n,29,3)

	u64 rti_data1_mem;
#define XGE_HAL_RTI_DATA1_MEM_RX_TIMER_VAL(n)      vBIT(n,3,29)
#define XGE_HAL_RTI_DATA1_MEM_RX_TIMER_AC_EN       BIT(38)
#define XGE_HAL_RTI_DATA1_MEM_RX_TIMER_CI_EN       BIT(39)
#define XGE_HAL_RTI_DATA1_MEM_RX_URNG_A(n)         vBIT(n,41,7)
#define XGE_HAL_RTI_DATA1_MEM_RX_URNG_B(n)         vBIT(n,49,7)
#define XGE_HAL_RTI_DATA1_MEM_RX_URNG_C(n)         vBIT(n,57,7)

	u64 rti_data2_mem;
#define XGE_HAL_RTI_DATA2_MEM_RX_UFC_A(n)          vBIT(n,0,16)
#define XGE_HAL_RTI_DATA2_MEM_RX_UFC_B(n)          vBIT(n,16,16)
#define XGE_HAL_RTI_DATA2_MEM_RX_UFC_C(n)          vBIT(n,32,16)
#define XGE_HAL_RTI_DATA2_MEM_RX_UFC_D(n)          vBIT(n,48,16)

	u64 rx_pa_cfg;
#define XGE_HAL_RX_PA_CFG_IGNORE_FRM_ERR           BIT(1)
#define XGE_HAL_RX_PA_CFG_IGNORE_SNAP_OUI          BIT(2)
#define XGE_HAL_RX_PA_CFG_IGNORE_LLC_CTRL          BIT(3)
#define XGE_HAL_RX_PA_CFG_SCATTER_MODE(n)          vBIT(n,6,1)
#define XGE_HAL_RX_PA_CFG_STRIP_VLAN_TAG_MODE(n)   vBIT(n,15,1)

	u8 unused13_0[0x8];

	u64 ring_bump_counter1;
	u64 ring_bump_counter2;
#define XGE_HAL_RING_BUMP_CNT(i, val) (u16)(val >> (48 - (16 * (i % 4))))

	u8 unused13[0x700 - 0x1f0];

	u64 rxdma_debug_ctrl;

	u8 unused14[0x2000 - 0x1f08];

/* Media Access Controller Register */
	u64 mac_int_status;
	u64 mac_int_mask;
#define XGE_HAL_MAC_INT_STATUS_TMAC_INT            BIT(0)
#define XGE_HAL_MAC_INT_STATUS_RMAC_INT            BIT(1)

	u64 mac_tmac_err_reg;
#define XGE_HAL_TMAC_ECC_DB_ERR			BIT(15) 
#define XGE_HAL_TMAC_TX_BUF_OVRN		BIT(23)
#define XGE_HAL_TMAC_TX_CRI_ERR		   	BIT(31)
#define XGE_HAL_TMAC_TX_SM_ERR			BIT(39)
	u64 mac_tmac_err_mask;
	u64 mac_tmac_err_alarm;

	u64 mac_rmac_err_reg;
#define XGE_HAL_RMAC_RX_BUFF_OVRN		BIT(0)
#define XGE_HAL_RMAC_RTH_SPDM_ECC_SG_ERR	BIT(0)
#define XGE_HAL_RMAC_RTS_ECC_DB_ERR		BIT(0)
#define XGE_HAL_RMAC_ECC_DB_ERR			BIT(0)
#define XGE_HAL_RMAC_RTH_SPDM_ECC_DB_ERR	BIT(0)
#define XGE_HAL_RMAC_LINK_STATE_CHANGE_INT	BIT(0)
#define XGE_HAL_RMAC_RX_SM_ERR			BIT(39)
	u64 mac_rmac_err_mask;
	u64 mac_rmac_err_alarm;

	u8 unused15[0x100 - 0x40];

	u64 mac_cfg;
#define XGE_HAL_MAC_CFG_TMAC_ENABLE             BIT(0)
#define XGE_HAL_MAC_CFG_RMAC_ENABLE             BIT(1)
#define XGE_HAL_MAC_CFG_LAN_NOT_WAN             BIT(2)
#define XGE_HAL_MAC_CFG_TMAC_LOOPBACK           BIT(3)
#define XGE_HAL_MAC_CFG_TMAC_APPEND_PAD         BIT(4)
#define XGE_HAL_MAC_CFG_RMAC_STRIP_FCS          BIT(5)
#define XGE_HAL_MAC_CFG_RMAC_STRIP_PAD          BIT(6)
#define XGE_HAL_MAC_CFG_RMAC_PROM_ENABLE        BIT(7)
#define XGE_HAL_MAC_RMAC_DISCARD_PFRM           BIT(8)
#define XGE_HAL_MAC_RMAC_BCAST_ENABLE           BIT(9)
#define XGE_HAL_MAC_RMAC_ALL_ADDR_ENABLE        BIT(10)
#define XGE_HAL_MAC_RMAC_INVLD_IPG_THR(val)     vBIT(val,16,8)

	u64 tmac_avg_ipg;
#define XGE_HAL_TMAC_AVG_IPG(val)           vBIT(val,0,8)

	u64 rmac_max_pyld_len;
#define XGE_HAL_RMAC_MAX_PYLD_LEN(val)      vBIT(val,2,14)

	u64 rmac_err_cfg;
#define XGE_HAL_RMAC_ERR_FCS                    BIT(0)
#define XGE_HAL_RMAC_ERR_FCS_ACCEPT             BIT(1)
#define XGE_HAL_RMAC_ERR_TOO_LONG               BIT(1)
#define XGE_HAL_RMAC_ERR_TOO_LONG_ACCEPT        BIT(1)
#define XGE_HAL_RMAC_ERR_RUNT                   BIT(2)
#define XGE_HAL_RMAC_ERR_RUNT_ACCEPT            BIT(2)
#define XGE_HAL_RMAC_ERR_LEN_MISMATCH           BIT(3)
#define XGE_HAL_RMAC_ERR_LEN_MISMATCH_ACCEPT    BIT(3)

	u64 rmac_cfg_key;
#define XGE_HAL_RMAC_CFG_KEY(val)               vBIT(val,0,16)

#define XGE_HAL_MAX_MAC_ADDRESSES               64
#define XGE_HAL_MAC_MC_ALL_MC_ADDR_OFFSET       63
#define XGE_HAL_MAX_MAC_ADDRESSES_HERC          256
#define XGE_HAL_MAC_MC_ALL_MC_ADDR_OFFSET_HERC  255

	u64 rmac_addr_cmd_mem;
#define XGE_HAL_RMAC_ADDR_CMD_MEM_WE                    BIT(7)
#define XGE_HAL_RMAC_ADDR_CMD_MEM_RD                    0
#define XGE_HAL_RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD        BIT(15)
#define XGE_HAL_RMAC_ADDR_CMD_MEM_STROBE_CMD_EXECUTING  BIT(15)
#define XGE_HAL_RMAC_ADDR_CMD_MEM_OFFSET(n)             vBIT(n,26,6)

	u64 rmac_addr_data0_mem;
#define XGE_HAL_RMAC_ADDR_DATA0_MEM_ADDR(n)    vBIT(n,0,48)
#define XGE_HAL_RMAC_ADDR_DATA0_MEM_USER       BIT(48)

	u64 rmac_addr_data1_mem;
#define XGE_HAL_RMAC_ADDR_DATA1_MEM_MASK(n)    vBIT(n,0,48)

	u8 unused16[0x8];

/*
        u64 rmac_addr_cfg;
#define XGE_HAL_RMAC_ADDR_UCASTn_EN(n)     mBIT(0)_n(n)
#define XGE_HAL_RMAC_ADDR_MCASTn_EN(n)     mBIT(0)_n(n)
#define XGE_HAL_RMAC_ADDR_BCAST_EN         vBIT(0)_48
#define XGE_HAL_RMAC_ADDR_ALL_ADDR_EN      vBIT(0)_49
*/
	u64 tmac_ipg_cfg;

	u64 rmac_pause_cfg;
#define XGE_HAL_RMAC_PAUSE_GEN_EN          BIT(0)
#define XGE_HAL_RMAC_PAUSE_RCV_EN          BIT(1)
#define XGE_HAL_RMAC_PAUSE_HG_PTIME_DEF    vBIT(0xFFFF,16,16)
#define XGE_HAL_RMAC_PAUSE_HG_PTIME(val)    vBIT(val,16,16)

	u64 rmac_red_cfg;

	u64 rmac_red_rate_q0q3;
	u64 rmac_red_rate_q4q7;

	u64 mac_link_util;
#define XGE_HAL_MAC_TX_LINK_UTIL           vBIT(0xFE,1,7)
#define XGE_HAL_MAC_TX_LINK_UTIL_DISABLE   vBIT(0xF, 8,4)
#define XGE_HAL_MAC_TX_LINK_UTIL_VAL( n )  vBIT(n,8,4)
#define XGE_HAL_MAC_RX_LINK_UTIL           vBIT(0xFE,33,7)
#define XGE_HAL_MAC_RX_LINK_UTIL_DISABLE   vBIT(0xF,40,4)
#define XGE_HAL_MAC_RX_LINK_UTIL_VAL( n )  vBIT(n,40,4)

#define XGE_HAL_MAC_LINK_UTIL_DISABLE (XGE_HAL_MAC_TX_LINK_UTIL_DISABLE | \
				       XGE_HAL_MAC_RX_LINK_UTIL_DISABLE)

	u64 rmac_invalid_ipg;

/* rx traffic steering */
#define XGE_HAL_MAC_RTS_FRM_LEN_SET(len)	vBIT(len,2,14)
	u64 rts_frm_len_n[8];

	u64 rts_qos_steering;

#define XGE_HAL_MAX_DIX_MAP                         4
	u64 rts_dix_map_n[XGE_HAL_MAX_DIX_MAP];
#define XGE_HAL_RTS_DIX_MAP_ETYPE(val)             vBIT(val,0,16)
#define XGE_HAL_RTS_DIX_MAP_SCW(val)               BIT(val,21)

	u64 rts_q_alternates;
	u64 rts_default_q;
#define XGE_HAL_RTS_DEFAULT_Q(n)		   vBIT(n,5,3)

	u64 rts_ctrl;
#define XGE_HAL_RTS_CTRL_IGNORE_SNAP_OUI           BIT(2)
#define XGE_HAL_RTS_CTRL_IGNORE_LLC_CTRL           BIT(3)
#define XGE_HAL_RTS_CTRL_ENHANCED_MODE		   BIT(7)

	u64 rts_pn_cam_ctrl;
#define XGE_HAL_RTS_PN_CAM_CTRL_WE                 BIT(7)
#define XGE_HAL_RTS_PN_CAM_CTRL_STROBE_NEW_CMD     BIT(15)
#define XGE_HAL_RTS_PN_CAM_CTRL_STROBE_BEING_EXECUTED   BIT(15)
#define XGE_HAL_RTS_PN_CAM_CTRL_OFFSET(n)          vBIT(n,24,8)
	u64 rts_pn_cam_data;
#define XGE_HAL_RTS_PN_CAM_DATA_TCP_SELECT         BIT(7)
#define XGE_HAL_RTS_PN_CAM_DATA_PORT(val)          vBIT(val,8,16)
#define XGE_HAL_RTS_PN_CAM_DATA_SCW(val)           vBIT(val,24,8)

	u64 rts_ds_mem_ctrl;
#define XGE_HAL_RTS_DS_MEM_CTRL_WE                 BIT(7)
#define XGE_HAL_RTS_DS_MEM_CTRL_STROBE_NEW_CMD     BIT(15)
#define XGE_HAL_RTS_DS_MEM_CTRL_STROBE_CMD_BEING_EXECUTED   BIT(15)
#define XGE_HAL_RTS_DS_MEM_CTRL_OFFSET(n)          vBIT(n,26,6)
	u64 rts_ds_mem_data;
#define XGE_HAL_RTS_DS_MEM_DATA(n)                 vBIT(n,0,8)

	u8  unused16_1[0x308 - 0x220];
	
	u64 rts_vid_mem_ctrl;
	u64 rts_vid_mem_data;
	u64 rts_p0_p3_map;
	u64 rts_p4_p7_map;
	u64 rts_p8_p11_map;
	u64 rts_p12_p15_map;

	u64 rts_mac_cfg;
#define XGE_HAL_RTS_MAC_SECT0_EN                    BIT(0)
#define XGE_HAL_RTS_MAC_SECT1_EN                    BIT(1)
#define XGE_HAL_RTS_MAC_SECT2_EN                    BIT(2)
#define XGE_HAL_RTS_MAC_SECT3_EN                    BIT(3)
#define XGE_HAL_RTS_MAC_SECT4_EN                    BIT(4)
#define XGE_HAL_RTS_MAC_SECT5_EN                    BIT(5)
#define XGE_HAL_RTS_MAC_SECT6_EN                    BIT(6)
#define XGE_HAL_RTS_MAC_SECT7_EN                    BIT(7)

	u8 unused16_2[0x380 - 0x340];

	u64 rts_rth_cfg;
#define XGE_HAL_RTS_RTH_EN                         BIT(3)
#define XGE_HAL_RTS_RTH_BUCKET_SIZE(n)             vBIT(n,4,4)
#define XGE_HAL_RTS_RTH_ALG_SEL_MS                 BIT(11)
#define XGE_HAL_RTS_RTH_TCP_IPV4_EN                BIT(15)
#define XGE_HAL_RTS_RTH_UDP_IPV4_EN                BIT(19)
#define XGE_HAL_RTS_RTH_IPV4_EN                    BIT(23)
#define XGE_HAL_RTS_RTH_TCP_IPV6_EN                BIT(27)
#define XGE_HAL_RTS_RTH_UDP_IPV6_EN                BIT(31)
#define XGE_HAL_RTS_RTH_IPV6_EN                    BIT(35)
#define XGE_HAL_RTS_RTH_TCP_IPV6_EX_EN             BIT(39)
#define XGE_HAL_RTS_RTH_UDP_IPV6_EX_EN             BIT(43)
#define XGE_HAL_RTS_RTH_IPV6_EX_EN                 BIT(47)

	u64 rts_rth_map_mem_ctrl;
#define XGE_HAL_RTS_RTH_MAP_MEM_CTRL_WE            BIT(7)
#define XGE_HAL_RTS_RTH_MAP_MEM_CTRL_STROBE        BIT(15)
#define XGE_HAL_RTS_RTH_MAP_MEM_CTRL_OFFSET(n)     vBIT(n,24,8)

	u64 rts_rth_map_mem_data;
#define XGE_HAL_RTS_RTH_MAP_MEM_DATA_ENTRY_EN      BIT(3)
#define XGE_HAL_RTS_RTH_MAP_MEM_DATA(n)            vBIT(n,5,3)

	u64 rts_rth_spdm_mem_ctrl;
#define XGE_HAL_RTS_RTH_SPDM_MEM_CTRL_STROBE       BIT(15)
#define XGE_HAL_RTS_RTH_SPDM_MEM_CTRL_LINE_SEL(n)  vBIT(n,21,3)
#define XGE_HAL_RTS_RTH_SPDM_MEM_CTRL_OFFSET(n)    vBIT(n,24,8)

	u64 rts_rth_spdm_mem_data;

	u64 rts_rth_jhash_cfg;
#define XGE_HAL_RTS_RTH_JHASH_GOLDEN(n)            vBIT(n,0,32)
#define XGE_HAL_RTS_RTH_JHASH_INIT_VAL(n)          vBIT(n,32,32)

	u64 rts_rth_hash_mask[5]; /* rth mask's 0...4 */
	u64 rts_rth_hash_mask_5;
#define XGE_HAL_RTH_HASH_MASK_5(n)                 vBIT(n,0,32)

	u64 rts_rth_status;
#define XGE_HAL_RTH_STATUS_SPDM_USE_L4             BIT(3)

	u8  unused17[0x400 - 0x3E8];
	
	u64 rmac_red_fine_q0q3;
	u64 rmac_red_fine_q4q7;    
	u64 rmac_pthresh_cross;
	u64 rmac_rthresh_cross;
	u64 rmac_pnum_range[32];

	u64 rmac_mp_crc_0;
	u64 rmac_mp_mask_a_0;
	u64 rmac_mp_mask_b_0;
	
	u64 rmac_mp_crc_1;
	u64 rmac_mp_mask_a_1;
	u64 rmac_mp_mask_b_1;
	
	u64 rmac_mp_crc_2;
	u64 rmac_mp_mask_a_2;
	u64 rmac_mp_mask_b_2;
	
	u64 rmac_mp_crc_3;
	u64 rmac_mp_mask_a_3;
	u64 rmac_mp_mask_b_3;
	
	u64 rmac_mp_crc_4;
	u64 rmac_mp_mask_a_4;
	u64 rmac_mp_mask_b_4;
	
	u64 rmac_mp_crc_5;
	u64 rmac_mp_mask_a_5;
	u64 rmac_mp_mask_b_5;
	
	u64 rmac_mp_crc_6;
	u64 rmac_mp_mask_a_6;
	u64 rmac_mp_mask_b_6;

	u64 rmac_mp_crc_7;
	u64 rmac_mp_mask_a_7;
	u64 rmac_mp_mask_b_7;

	u64 mac_ctrl;
	u64 activity_control;
	
	u8  unused17_2[0x700 - 0x5F0];

	u64 mac_debug_ctrl;
#define XGE_HAL_MAC_DBG_ACTIVITY_VALUE		   0x411040400000000ULL

	u8 unused18[0x2800 - 0x2708];

/* memory controller registers */
	u64 mc_int_status;
#define XGE_HAL_MC_INT_STATUS_MC_INT               BIT(0)
	u64 mc_int_mask;
#define XGE_HAL_MC_INT_MASK_MC_INT                 BIT(0)

	u64 mc_err_reg;
#define XGE_HAL_MC_ERR_REG_ITQ_ECC_SG_ERR_L        BIT(2) /* non-Xena */
#define XGE_HAL_MC_ERR_REG_ITQ_ECC_SG_ERR_U        BIT(3) /* non-Xena */
#define XGE_HAL_MC_ERR_REG_RLD_ECC_SG_ERR_L        BIT(4) /* non-Xena */
#define XGE_HAL_MC_ERR_REG_RLD_ECC_SG_ERR_U        BIT(5) /* non-Xena */
#define XGE_HAL_MC_ERR_REG_ETQ_ECC_SG_ERR_L        BIT(6)
#define XGE_HAL_MC_ERR_REG_ETQ_ECC_SG_ERR_U        BIT(7)
#define XGE_HAL_MC_ERR_REG_ITQ_ECC_DB_ERR_L        BIT(10) /* non-Xena */
#define XGE_HAL_MC_ERR_REG_ITQ_ECC_DB_ERR_U        BIT(11) /* non-Xena */
#define XGE_HAL_MC_ERR_REG_RLD_ECC_DB_ERR_L        BIT(12) /* non-Xena */
#define XGE_HAL_MC_ERR_REG_RLD_ECC_DB_ERR_U        BIT(13) /* non-Xena */
#define XGE_HAL_MC_ERR_REG_ETQ_ECC_DB_ERR_L        BIT(14)
#define XGE_HAL_MC_ERR_REG_ETQ_ECC_DB_ERR_U        BIT(15)
#define XGE_HAL_MC_ERR_REG_MIRI_ECC_SG_ERR_0       BIT(17)
#define XGE_HAL_MC_ERR_REG_MIRI_ECC_DB_ERR_0       BIT(18) /* Xena: reset */
#define XGE_HAL_MC_ERR_REG_MIRI_ECC_SG_ERR_1       BIT(19)
#define XGE_HAL_MC_ERR_REG_MIRI_ECC_DB_ERR_1       BIT(20) /* Xena: reset */
#define XGE_HAL_MC_ERR_REG_MIRI_CRI_ERR_0          BIT(22)
#define XGE_HAL_MC_ERR_REG_MIRI_CRI_ERR_1          BIT(23)
#define XGE_HAL_MC_ERR_REG_SM_ERR                  BIT(31)
#define XGE_HAL_MC_ERR_REG_PL_LOCK_N               BIT(39)

	u64 mc_err_mask;
	u64 mc_err_alarm;

	u8 unused19[0x100 - 0x28];

/* MC configuration */
	u64 rx_queue_cfg;
#define XGE_HAL_RX_QUEUE_CFG_Q0_SZ(n)              vBIT(n,0,8)
#define XGE_HAL_RX_QUEUE_CFG_Q1_SZ(n)              vBIT(n,8,8)
#define XGE_HAL_RX_QUEUE_CFG_Q2_SZ(n)              vBIT(n,16,8)
#define XGE_HAL_RX_QUEUE_CFG_Q3_SZ(n)              vBIT(n,24,8)
#define XGE_HAL_RX_QUEUE_CFG_Q4_SZ(n)              vBIT(n,32,8)
#define XGE_HAL_RX_QUEUE_CFG_Q5_SZ(n)              vBIT(n,40,8)
#define XGE_HAL_RX_QUEUE_CFG_Q6_SZ(n)              vBIT(n,48,8)
#define XGE_HAL_RX_QUEUE_CFG_Q7_SZ(n)              vBIT(n,56,8)

	u64 mc_rldram_mrs;
#define XGE_HAL_MC_RLDRAM_QUEUE_SIZE_ENABLE	BIT(39)
#define XGE_HAL_MC_RLDRAM_MRS_ENABLE		BIT(47)

	u64 mc_rldram_interleave;

	u64 mc_pause_thresh_q0q3;
	u64 mc_pause_thresh_q4q7;

	u64 mc_red_thresh_q[8];

	u8 unused20[0x200 - 0x168];
	u64 mc_rldram_ref_per;
	u8 unused21[0x220 - 0x208];
	u64 mc_rldram_test_ctrl;
#define XGE_HAL_MC_RLDRAM_TEST_MODE		BIT(47)
#define XGE_HAL_MC_RLDRAM_TEST_WRITE		BIT(7)
#define XGE_HAL_MC_RLDRAM_TEST_GO		BIT(15)
#define XGE_HAL_MC_RLDRAM_TEST_DONE		BIT(23)
#define XGE_HAL_MC_RLDRAM_TEST_PASS		BIT(31)

	u8 unused22[0x240 - 0x228];
	u64 mc_rldram_test_add;
	u8 unused23[0x260 - 0x248];
	u64 mc_rldram_test_d0;
	u8 unused24[0x280 - 0x268];
	u64 mc_rldram_test_d1;
	u8 unused25[0x300 - 0x288];
	u64 mc_rldram_test_d2;
	u8  unused26_1[0x2C00 - 0x2B08];
	u64 mc_rldram_test_read_d0;
	u8  unused26_2[0x20 - 0x8];
	u64 mc_rldram_test_read_d1;
	u8  unused26_3[0x40 - 0x28];
	u64 mc_rldram_test_read_d2;
	u8  unused26_4[0x60 - 0x48];
	u64 mc_rldram_test_add_bkg;
	u8  unused26_5[0x80 - 0x68];
	u64 mc_rldram_test_d0_bkg;
	u8  unused26_6[0xD00 - 0xC88];    
	u64 mc_rldram_test_d1_bkg;
	u8  unused26_7[0x20 - 0x8];
	u64 mc_rldram_test_d2_bkg;
	u8  unused26_8[0x40 - 0x28];
	u64 mc_rldram_test_read_d0_bkg;
	u8  unused26_9[0x60 - 0x48];
	u64 mc_rldram_test_read_d1_bkg;
	u8  unused26_10[0x80 - 0x68];
	u64 mc_rldram_test_read_d2_bkg;
	u8  unused26_11[0xE00 - 0xD88];
	u64 mc_rldram_generation;
	u8  unused26_12[0x20 - 0x8];
	u64 mc_driver;
	u8  unused26_13[0x40 - 0x28];
	u64 mc_rldram_ref_per_herc;
#define XGE_HAL_MC_RLDRAM_SET_REF_PERIOD(n)   vBIT(n, 0, 16)
	u8 unused26_14[0x660 - 0x648];
	u64 mc_rldram_mrs_herc;
#define XGE_HAL_MC_RLDRAM_MRS(n)              vBIT(n, 14, 17)
	u8 unused26_15[0x700 - 0x668];
	u64 mc_debug_ctrl;

	u8 unused27[0x3000 - 0x2f08];

/* XGXG */
	/* XGXS control registers */

	u64 xgxs_int_status;
#define XGE_HAL_XGXS_INT_STATUS_TXGXS              BIT(0)
#define XGE_HAL_XGXS_INT_STATUS_RXGXS              BIT(1)
	u64 xgxs_int_mask;
#define XGE_HAL_XGXS_INT_MASK_TXGXS                BIT(0)
#define XGE_HAL_XGXS_INT_MASK_RXGXS                BIT(1)

	u64 xgxs_txgxs_err_reg;
#define XGE_HAL_TXGXS_ECC_SG_ERR			BIT(7)
#define XGE_HAL_TXGXS_ECC_DB_ERR			BIT(15)
#define XGE_HAL_TXGXS_ESTORE_UFLOW			BIT(31)
#define XGE_HAL_TXGXS_TX_SM_ERR				BIT(39)
	u64 xgxs_txgxs_err_mask;
	u64 xgxs_txgxs_err_alarm;

	u64 xgxs_rxgxs_err_reg;
#define XGE_HAL_RXGXS_ESTORE_OFLOW			BIT(7)
#define XGE_HAL_RXGXS_RX_SM_ERR				BIT(39)
	u64 xgxs_rxgxs_err_mask;
	u64 xgxs_rxgxs_err_alarm;

	u64 spi_err_reg;
	u64 spi_err_mask;
	u64 spi_err_alarm;

	u8 unused28[0x100 - 0x58];

	u64 xgxs_cfg;
	u64 xgxs_status;

	u64 xgxs_cfg_key;
	u64 xgxs_efifo_cfg; /* CHANGED */
	u64 rxgxs_ber_0;    /* CHANGED */
	u64 rxgxs_ber_1;    /* CHANGED */
	
	u64 spi_control;
	u64 spi_data;
	u64 spi_write_protect;
	
	u8  unused29[0x80 - 0x48];
	
	u64 xgxs_cfg_1;
} xge_hal_pci_bar0_t;

/* Using this strcture to calculate offsets */
typedef struct xge_hal_pci_config_le_t {
    u16     vendor_id;              // 0x00
    u16     device_id;              // 0x02

    u16     command;                // 0x04
    u16     status;                 // 0x06

    u8      revision;               // 0x08
    u8      pciClass[3];            // 0x09

    u8      cache_line_size;        // 0x0c
    u8      latency_timer;          // 0x0d
    u8      header_type;            // 0x0e
    u8      bist;                   // 0x0f

    u32     base_addr0_lo;          // 0x10
    u32     base_addr0_hi;          // 0x14

    u32     base_addr1_lo;          // 0x18
    u32     base_addr1_hi;          // 0x1C

    u32     not_Implemented1;       // 0x20
    u32     not_Implemented2;       // 0x24

    u32     cardbus_cis_pointer;    // 0x28

    u16     subsystem_vendor_id;    // 0x2c
    u16     subsystem_id;           // 0x2e

    u32     rom_base;               // 0x30
    u8      capabilities_pointer;   // 0x34
    u8      rsvd_35[3];             // 0x35
    u32     rsvd_38;                // 0x38

    u8      interrupt_line;         // 0x3c
    u8      interrupt_pin;          // 0x3d
    u8      min_grant;              // 0x3e
    u8      max_latency;            // 0x3f

    u8      msi_cap_id;             // 0x40
    u8      msi_next_ptr;           // 0x41
    u16     msi_control;            // 0x42
    u32     msi_lower_address;      // 0x44
    u32     msi_higher_address;     // 0x48
    u16     msi_data;               // 0x4c
    u16     msi_unused;             // 0x4e

    u8      vpd_cap_id;             // 0x50
    u8      vpd_next_cap;           // 0x51
    u16     vpd_addr;               // 0x52
    u32     vpd_data;               // 0x54

    u8      rsvd_b0[8];             // 0x58

    u8      pcix_cap;               // 0x60
    u8      pcix_next_cap;          // 0x61
    u16     pcix_command;           // 0x62

    u32     pcix_status;            // 0x64

    u8      rsvd_b1[XGE_HAL_PCI_XFRAME_CONFIG_SPACE_SIZE-0x68];
} xge_hal_pci_config_le_t;              // 0x100

typedef struct xge_hal_pci_config_t {
#ifdef XGE_OS_HOST_BIG_ENDIAN
    u16     device_id;              // 0x02
    u16     vendor_id;              // 0x00

    u16     status;                 // 0x06
    u16     command;                // 0x04

    u8      pciClass[3];            // 0x09
    u8      revision;               // 0x08

    u8      bist;                   // 0x0f
    u8      header_type;            // 0x0e
    u8      latency_timer;          // 0x0d
    u8      cache_line_size;        // 0x0c

    u32     base_addr0_lo;           // 0x10
    u32     base_addr0_hi;           // 0x14

    u32     base_addr1_lo;          // 0x18
    u32     base_addr1_hi;          // 0x1C

    u32     not_Implemented1;       // 0x20
    u32     not_Implemented2;       // 0x24

    u32     cardbus_cis_pointer;    // 0x28

    u16     subsystem_id;           // 0x2e
    u16     subsystem_vendor_id;    // 0x2c

    u32     rom_base;               // 0x30
    u8      rsvd_35[3];             // 0x35
    u8      capabilities_pointer;   // 0x34
    u32     rsvd_38;                // 0x38

    u8      max_latency;            // 0x3f
    u8      min_grant;              // 0x3e
    u8      interrupt_pin;          // 0x3d
    u8      interrupt_line;         // 0x3c

    u16     msi_control;            // 0x42
    u8      msi_next_ptr;           // 0x41
    u8      msi_cap_id;             // 0x40
    u32     msi_lower_address;      // 0x44
    u32     msi_higher_address;     // 0x48
    u16     msi_unused;             // 0x4e
    u16     msi_data;               // 0x4c

    u16     vpd_addr;               // 0x52
    u8      vpd_next_cap;           // 0x51
    u8      vpd_cap_id;             // 0x50
    u32     vpd_data;               // 0x54

    u8      rsvd_b0[8];             // 0x58

    u16     pcix_command;           // 0x62
    u8      pcix_next_cap;          // 0x61
    u8      pcix_cap;               // 0x60

    u32     pcix_status;            // 0x64
#else
    u16     vendor_id;              // 0x00
    u16     device_id;              // 0x02

    u16     command;                // 0x04
    u16     status;                 // 0x06

    u8      revision;               // 0x08
    u8      pciClass[3];            // 0x09

    u8      cache_line_size;        // 0x0c
    u8      latency_timer;          // 0x0d
    u8      header_type;            // 0x0e
    u8      bist;                   // 0x0f

    u32     base_addr0_lo;          // 0x10
    u32     base_addr0_hi;          // 0x14

    u32     base_addr1_lo;          // 0x18
    u32     base_addr1_hi;          // 0x1C

    u32     not_Implemented1;       // 0x20
    u32     not_Implemented2;       // 0x24

    u32     cardbus_cis_pointer;    // 0x28

    u16     subsystem_vendor_id;    // 0x2c
    u16     subsystem_id;           // 0x2e

    u32     rom_base;               // 0x30
    u8      capabilities_pointer;   // 0x34
    u8      rsvd_35[3];             // 0x35
    u32     rsvd_38;                // 0x38

    u8      interrupt_line;         // 0x3c
    u8      interrupt_pin;          // 0x3d
    u8      min_grant;              // 0x3e
    u8      max_latency;            // 0x3f

    u8      msi_cap_id;             // 0x40
    u8      msi_next_ptr;           // 0x41
    u16     msi_control;            // 0x42
    u32     msi_lower_address;      // 0x44
    u32     msi_higher_address;     // 0x48
    u16     msi_data;               // 0x4c
    u16     msi_unused;             // 0x4e

    u8      vpd_cap_id;             // 0x50
    u8      vpd_next_cap;           // 0x51
    u16     vpd_addr;               // 0x52
    u32     vpd_data;               // 0x54

    u8      rsvd_b0[8];             // 0x58

    u8      pcix_cap;               // 0x60
    u8      pcix_next_cap;          // 0x61
    u16     pcix_command;           // 0x62

    u32     pcix_status;            // 0x64

#endif
    u8      rsvd_b1[XGE_HAL_PCI_XFRAME_CONFIG_SPACE_SIZE-0x68];
} xge_hal_pci_config_t;               // 0x100

#define XGE_HAL_REG_SPACE	sizeof(xge_hal_pci_bar0_t)
#define XGE_HAL_EEPROM_SIZE	(0x01 << 11)

#endif /* XGE_HAL_REGS_H */
