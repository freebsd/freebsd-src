/*******************************************************************************

  
  Copyright(c) 1999 - 2004 Intel Corporation. All rights reserved.
  
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
  
  Contact Information:
  Linux NICS <linux.nics@intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
*******************************************************************************/

#ifndef _E100_INC_
#define _E100_INC_

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/reboot.h>
#include <asm/io.h>
#include <asm/unaligned.h>
#include <asm/processor.h>
#include <linux/ethtool.h>
#include <linux/inetdevice.h>
#include <linux/bitops.h>

#include <linux/if.h>
#include <asm/uaccess.h>
#include <linux/ip.h>
#include <linux/if_vlan.h>
#include <linux/mii.h>

#define E100_CABLE_UNKNOWN	0
#define E100_CABLE_OK		1		
#define E100_CABLE_OPEN_NEAR	2	/* Open Circuit Near End  */
#define E100_CABLE_OPEN_FAR	3	/* Open Circuit Far End   */
#define E100_CABLE_SHORT_NEAR	4	/* Short Circuit Near End */
#define E100_CABLE_SHORT_FAR	5	/* Short Circuit Far End  */

#define E100_REGS_LEN 2
/*
 *  Configure parameters for buffers per controller.
 *  If the machine this is being used on is a faster machine (i.e. > 150MHz)
 *  and running on a 10MBS network then more queueing of data occurs. This
 *  may indicate the some of the numbers below should be adjusted.  Here are
 *  some typical numbers:
 *                             MAX_TCB 64
 *                             MAX_RFD 64
 *  The default numbers give work well on most systems tests so no real
 *  adjustments really need to take place.  Also, if the machine is connected
 *  to a 100MBS network the numbers described above can be lowered from the
 *  defaults as considerably less data will be queued.
 */

#define TX_FRAME_CNT   8	/* consecutive transmit frames per interrupt */
/* TX_FRAME_CNT must be less than MAX_TCB    */

#define E100_DEFAULT_TCB   64
#define E100_MIN_TCB       2*TX_FRAME_CNT + 3	/* make room for at least 2 interrupts */
#define E100_MAX_TCB       1024

#define E100_DEFAULT_RFD   64
#define E100_MIN_RFD       8
#define E100_MAX_RFD       1024

#define E100_DEFAULT_XSUM         true
#define E100_DEFAULT_BER          ZLOCK_MAX_ERRORS
#define E100_DEFAULT_SPEED_DUPLEX 0
#define E100_DEFAULT_FC           0
#define E100_DEFAULT_IFS          true
#define E100_DEFAULT_UCODE        true

#define TX_THRSHLD     8

/* IFS parameters */
#define MIN_NUMBER_OF_TRANSMITS_100 1000
#define MIN_NUMBER_OF_TRANSMITS_10  100

#define E100_MAX_NIC 16

#define E100_MAX_SCB_WAIT	100	/* Max udelays in wait_scb */
#define E100_MAX_CU_IDLE_WAIT	50	/* Max udelays in wait_cus_idle */

/* HWI feature related constant */
#define HWI_REGISTER_GRANULARITY        80	/* register granularity = 80 Cm */
#define HWI_NEAR_END_BOUNDARY           1000	/* Near end is defined as < 10 meters */

/* CPUSAVER_BUNDLE_MAX: Sets the maximum number of frames that will be bundled.
 * In some situations, such as the TCP windowing algorithm, it may be
 * better to limit the growth of the bundle size than let it go as
 * high as it can, because that could cause too much added latency.
 * The default is six, because this is the number of packets in the
 * default TCP window size.  A value of 1 would make CPUSaver indicate
 * an interrupt for every frame received.  If you do not want to put
 * a limit on the bundle size, set this value to xFFFF.
 */
#define E100_DEFAULT_CPUSAVER_BUNDLE_MAX	6
#define E100_DEFAULT_CPUSAVER_INTERRUPT_DELAY	0x600
#define E100_DEFAULT_BUNDLE_SMALL_FR		false

/* end of configurables */

/* ====================================================================== */
/*                                hw                                      */
/* ====================================================================== */

/* timeout for command completion */
#define E100_CMD_WAIT   100	/* iterations */

struct driver_stats {
	struct net_device_stats net_stats;

	unsigned long tx_late_col;
	unsigned long tx_ok_defrd;
	unsigned long tx_one_retry;
	unsigned long tx_mt_one_retry;
	unsigned long rcv_cdt_frames;
	unsigned long xmt_fc_pkts;
	unsigned long rcv_fc_pkts;
	unsigned long rcv_fc_unsupported;
	unsigned long xmt_tco_pkts;
	unsigned long rcv_tco_pkts;
	unsigned long rx_intr_pkts;
};

/* TODO: kill me when we can do C99 */
#define false		(0)
#define true		(1)

/* Changed for 82558 and 82559 enhancements */
/* defines for 82558/9 flow control CSR values */
#define DFLT_FC_THLD       0x00	/* Rx FIFO threshold of 0.5KB free  */
#define DFLT_FC_CMD        0x00	/* FC Command in CSR */

/* ====================================================================== */
/*                              equates                                   */
/* ====================================================================== */

/*
 * These are general purpose defines 
 */

/* Bit Mask definitions */
#define BIT_0       0x0001
#define BIT_1       0x0002
#define BIT_2       0x0004
#define BIT_3       0x0008
#define BIT_4       0x0010
#define BIT_5       0x0020
#define BIT_6       0x0040
#define BIT_7       0x0080
#define BIT_8       0x0100
#define BIT_9       0x0200
#define BIT_10      0x0400
#define BIT_11      0x0800
#define BIT_12      0x1000
#define BIT_13      0x2000
#define BIT_14      0x4000
#define BIT_15      0x8000
#define BIT_28      0x10000000

#define BIT_0_2     0x0007
#define BIT_0_3     0x000F
#define BIT_0_4     0x001F
#define BIT_0_5     0x003F
#define BIT_0_6     0x007F
#define BIT_0_7     0x00FF
#define BIT_0_8     0x01FF
#define BIT_0_13    0x3FFF
#define BIT_0_15    0xFFFF
#define BIT_1_2     0x0006
#define BIT_1_3     0x000E
#define BIT_2_5     0x003C
#define BIT_3_4     0x0018
#define BIT_4_5     0x0030
#define BIT_4_6     0x0070
#define BIT_4_7     0x00F0
#define BIT_5_7     0x00E0
#define BIT_5_12    0x1FE0
#define BIT_5_15    0xFFE0
#define BIT_6_7     0x00c0
#define BIT_7_11    0x0F80
#define BIT_8_10    0x0700
#define BIT_9_13    0x3E00
#define BIT_12_15   0xF000
#define BIT_8_15    0xFF00

#define BIT_16_20   0x001F0000
#define BIT_21_25   0x03E00000
#define BIT_26_27   0x0C000000

/* Transmit Threshold related constants */
#define DEFAULT_TX_PER_UNDERRUN         20000

#define MAX_MULTICAST_ADDRS             64
#define MAX_FILTER                      16

#define FULL_DUPLEX      2
#define HALF_DUPLEX      1

/*
 * These defines are specific to the 82557 
 */

/* E100 PORT functions -- lower 4 bits */
#define PORT_SOFTWARE_RESET         0
#define PORT_SELFTEST               1
#define PORT_SELECTIVE_RESET        2
#define PORT_DUMP                   3

/* SCB Status Word bit definitions */
/* Interrupt status/ack fields */
/* ER and FCP interrupts for 82558 masks  */
#define SCB_STATUS_ACK_MASK        BIT_8_15	/* Status Mask */
#define SCB_STATUS_ACK_CX          BIT_15	/* CU Completed Action Cmd */
#define SCB_STATUS_ACK_FR          BIT_14	/* RU Received A Frame */
#define SCB_STATUS_ACK_CNA         BIT_13	/* CU Became Inactive (IDLE) */
#define SCB_STATUS_ACK_RNR         BIT_12	/* RU Became Not Ready */
#define SCB_STATUS_ACK_MDI         BIT_11	/* MDI read or write done */
#define SCB_STATUS_ACK_SWI         BIT_10	/* S/W generated interrupt */
#define SCB_STATUS_ACK_ER          BIT_9	/* Early Receive */
#define SCB_STATUS_ACK_FCP         BIT_8	/* Flow Control Pause */

/*- CUS Fields */
#define SCB_CUS_MASK            (BIT_6 | BIT_7)	/* CUS 2-bit Mask */
#define SCB_CUS_IDLE            0	/* CU Idle */
#define SCB_CUS_SUSPEND         BIT_6	/* CU Suspended */
#define SCB_CUS_ACTIVE          BIT_7	/* CU Active */

/*- RUS Fields */
#define SCB_RUS_IDLE            0	/* RU Idle */
#define SCB_RUS_MASK            BIT_2_5	/* RUS 3-bit Mask */
#define SCB_RUS_SUSPEND         BIT_2	/* RU Suspended */
#define SCB_RUS_NO_RESOURCES    BIT_3	/* RU Out Of Resources */
#define SCB_RUS_READY           BIT_4	/* RU Ready */
#define SCB_RUS_SUSP_NO_RBDS    (BIT_2 | BIT_5)	/* RU No More RBDs */
#define SCB_RUS_NO_RBDS         (BIT_3 | BIT_5)	/* RU No More RBDs */
#define SCB_RUS_READY_NO_RBDS   (BIT_4 | BIT_5)	/* RU Ready, No RBDs */

/* SCB Command Word bit definitions */
/*- CUC fields */
/* Changing mask to 4 bits */
#define SCB_CUC_MASK            BIT_4_7	/* CUC 4-bit Mask */
#define SCB_CUC_NOOP            0
#define SCB_CUC_START           BIT_4	/* CU Start */
#define SCB_CUC_RESUME          BIT_5	/* CU Resume */
#define SCB_CUC_UNKNOWN         BIT_7	/* CU unknown command */
/* Changed for 82558 enhancements */
#define SCB_CUC_STATIC_RESUME   (BIT_5 | BIT_7)	/* 82558/9 Static Resume */
#define SCB_CUC_DUMP_ADDR       BIT_6	/* CU Dump Counters Address */
#define SCB_CUC_DUMP_STAT       (BIT_4 | BIT_6)	/* CU Dump stat. counters */
#define SCB_CUC_LOAD_BASE       (BIT_5 | BIT_6)	/* Load the CU base */
/* Below was defined as BIT_4_7 */
#define SCB_CUC_DUMP_RST_STAT   BIT_4_6	/* CU Dump & reset statistics cntrs */

/*- RUC fields */
#define SCB_RUC_MASK            BIT_0_2	/* RUC 3-bit Mask */
#define SCB_RUC_START           BIT_0	/* RU Start */
#define SCB_RUC_RESUME          BIT_1	/* RU Resume */
#define SCB_RUC_ABORT           BIT_2	/* RU Abort */
#define SCB_RUC_LOAD_HDS        (BIT_0 | BIT_2)	/* Load RFD Header Data Size */
#define SCB_RUC_LOAD_BASE       (BIT_1 | BIT_2)	/* Load the RU base */
#define SCB_RUC_RBD_RESUME      BIT_0_2	/* RBD resume */

/* Interrupt fields (assuming byte addressing) */
#define SCB_INT_MASK            BIT_0	/* Mask interrupts */
#define SCB_SOFT_INT            BIT_1	/* Generate a S/W interrupt */
/*  Specific Interrupt Mask Bits (upper byte of SCB Command word) */
#define SCB_FCP_INT_MASK        BIT_2	/* Flow Control Pause */
#define SCB_ER_INT_MASK         BIT_3	/* Early Receive */
#define SCB_RNR_INT_MASK        BIT_4	/* RU Not Ready */
#define SCB_CNA_INT_MASK        BIT_5	/* CU Not Active */
#define SCB_FR_INT_MASK         BIT_6	/* Frame Received */
#define SCB_CX_INT_MASK         BIT_7	/* CU eXecution w/ I-bit done */
#define SCB_BACHELOR_INT_MASK   BIT_2_7	/* 82558 interrupt mask bits */

#define SCB_GCR2_EEPROM_ACCESS_SEMAPHORE BIT_7

/* EEPROM bit definitions */
/*- EEPROM control register bits */
#define EEPROM_FLAG_ASF  0x8000
#define EEPROM_FLAG_GCL  0x4000

#define EN_TRNF          0x10	/* Enable turnoff */
#define EEDO             0x08	/* EEPROM data out */
#define EEDI             0x04	/* EEPROM data in (set for writing data) */
#define EECS             0x02	/* EEPROM chip select (1=hi, 0=lo) */
#define EESK             0x01	/* EEPROM shift clock (1=hi, 0=lo) */

/*- EEPROM opcodes */
#define EEPROM_READ_OPCODE          06
#define EEPROM_WRITE_OPCODE         05
#define EEPROM_ERASE_OPCODE         07
#define EEPROM_EWEN_OPCODE          19	/* Erase/write enable */
#define EEPROM_EWDS_OPCODE          16	/* Erase/write disable */

/*- EEPROM data locations */
#define EEPROM_NODE_ADDRESS_BYTE_0      0
#define EEPROM_COMPATIBILITY_WORD       3
#define EEPROM_PWA_NO                   8
#define EEPROM_ID_WORD			0x0A
#define EEPROM_CONFIG_ASF		0x0D
#define EEPROM_SMBUS_ADDR		0x90

#define EEPROM_SUM                      0xbaba

// Zero Locking Algorithm definitions:
#define ZLOCK_ZERO_MASK		0x00F0
#define ZLOCK_MAX_READS		50	
#define ZLOCK_SET_ZERO		0x2010
#define ZLOCK_MAX_SLEEP		300 * HZ	
#define ZLOCK_MAX_ERRORS	300

/* E100 Action Commands */
#define CB_IA_ADDRESS           1
#define CB_CONFIGURE            2
#define CB_MULTICAST            3
#define CB_TRANSMIT             4
#define CB_LOAD_MICROCODE       5
#define CB_LOAD_FILTER		8
#define CB_MAX_NONTX_CMD        9
#define CB_IPCB_TRANSMIT        9

/* Pre-defined Filter Bits */
#define CB_FILTER_EL            0x80000000
#define CB_FILTER_FIX           0x40000000
#define CB_FILTER_ARP           0x08000000
#define CB_FILTER_IA_MATCH      0x02000000

/* Command Block (CB) Field Definitions */
/*- CB Command Word */
#define CB_EL_BIT           BIT_15	/* CB EL Bit */
#define CB_S_BIT            BIT_14	/* CB Suspend Bit */
#define CB_I_BIT            BIT_13	/* CB Interrupt Bit */
#define CB_TX_SF_BIT        BIT_3	/* TX CB Flexible Mode */
#define CB_CMD_MASK         BIT_0_3	/* CB 4-bit CMD Mask */
#define CB_CID_DEFAULT      (0x1f << 8)	/* CB 5-bit CID (max value) */

/*- CB Status Word */
#define CB_STATUS_MASK          BIT_12_15	/* CB Status Mask (4-bits) */
#define CB_STATUS_COMPLETE      BIT_15	/* CB Complete Bit */
#define CB_STATUS_OK            BIT_13	/* CB OK Bit */
#define CB_STATUS_VLAN          BIT_12 /* CB Valn detected Bit */
#define CB_STATUS_FAIL          BIT_11	/* CB Fail (F) Bit */

/*misc command bits */
#define CB_TX_EOF_BIT           BIT_15	/* TX CB/TBD EOF Bit */

/* Config params */
#define CB_CFIG_BYTE_COUNT          22	/* 22 config bytes */
#define CB_CFIG_D102_BYTE_COUNT    10

/* Receive Frame Descriptor Fields */

/*- RFD Status Bits */
#define RFD_RECEIVE_COLLISION   BIT_0	/* Collision detected on Receive */
#define RFD_IA_MATCH            BIT_1	/* Indv Address Match Bit */
#define RFD_RX_ERR              BIT_4	/* RX_ERR pin on Phy was set */
#define RFD_FRAME_TOO_SHORT     BIT_7	/* Receive Frame Short */
#define RFD_DMA_OVERRUN         BIT_8	/* Receive DMA Overrun */
#define RFD_NO_RESOURCES        BIT_9	/* No Buffer Space */
#define RFD_ALIGNMENT_ERROR     BIT_10	/* Alignment Error */
#define RFD_CRC_ERROR           BIT_11	/* CRC Error */
#define RFD_STATUS_OK           BIT_13	/* RFD OK Bit */
#define RFD_STATUS_COMPLETE     BIT_15	/* RFD Complete Bit */

/*- RFD Command Bits*/
#define RFD_EL_BIT      BIT_15	/* RFD EL Bit */
#define RFD_S_BIT       BIT_14	/* RFD Suspend Bit */
#define RFD_H_BIT       BIT_4	/* Header RFD Bit */
#define RFD_SF_BIT      BIT_3	/* RFD Flexible Mode */

/*- RFD misc bits*/
#define RFD_EOF_BIT         BIT_15	/* RFD End-Of-Frame Bit */
#define RFD_F_BIT           BIT_14	/* RFD Buffer Fetch Bit */
#define RFD_ACT_COUNT_MASK  BIT_0_13	/* RFD Actual Count Mask */

/* Receive Buffer Descriptor Fields*/
#define RBD_EOF_BIT             BIT_15	/* RBD End-Of-Frame Bit */
#define RBD_F_BIT               BIT_14	/* RBD Buffer Fetch Bit */
#define RBD_ACT_COUNT_MASK      BIT_0_13	/* RBD Actual Count Mask */

#define SIZE_FIELD_MASK     BIT_0_13	/* Size of the associated buffer */
#define RBD_EL_BIT          BIT_15	/* RBD EL Bit */

/* Self Test Results*/
#define CB_SELFTEST_FAIL_BIT        BIT_12
#define CB_SELFTEST_DIAG_BIT        BIT_5
#define CB_SELFTEST_REGISTER_BIT    BIT_3
#define CB_SELFTEST_ROM_BIT         BIT_2

#define CB_SELFTEST_ERROR_MASK ( \
                CB_SELFTEST_FAIL_BIT | CB_SELFTEST_DIAG_BIT | \
                CB_SELFTEST_REGISTER_BIT | CB_SELFTEST_ROM_BIT)

/* adapter vendor & device ids */
#define PCI_OHIO_BOARD   0x10f0	/* subdevice ID, Ohio dual port nic */

/* Values for PCI_REV_ID_REGISTER values */
#define D101A4_REV_ID      4	/* 82558 A4 stepping */
#define D101B0_REV_ID      5	/* 82558 B0 stepping */
#define D101MA_REV_ID      8	/* 82559 A0 stepping */
#define D101S_REV_ID      9	/* 82559S A-step */
#define D102_REV_ID      12
#define D102C_REV_ID     13	/* 82550 step C */
#define D102E_REV_ID     15

/* ############Start of 82555 specific defines################## */

#define PHY_82555_LED_SWITCH_CONTROL    	0x1b	/* 82555 led switch control register */

/* 82555 led switch control reg. opcodes */
#define PHY_82555_LED_NORMAL_CONTROL    0	// control back to the 8255X
#define PHY_82555_LED_DRIVER_CONTROL    BIT_2	// the driver is in control
#define PHY_82555_LED_OFF               BIT_2	// activity LED is off
#define PHY_82555_LED_ON_559           (BIT_0 | BIT_2)	// activity LED is on for 559 and later
#define PHY_82555_LED_ON_PRE_559       (BIT_0 | BIT_1 | BIT_2)	// activity LED is on for 558 and before

// Describe the state of the phy led.
// needed for the function : 'e100_blink_timer'
enum led_state_e {
	LED_OFF = 0,
	LED_ON,
};

/* ############End of 82555 specific defines##################### */

#define RFD_PARSE_BIT			BIT_3
#define RFD_TCP_PACKET			0x00
#define RFD_UDP_PACKET			0x01
#define TCPUDP_CHECKSUM_BIT_VALID	BIT_4
#define TCPUDP_CHECKSUM_VALID		BIT_5
#define CHECKSUM_PROTOCOL_MASK		0x03

#define VLAN_SIZE   4
#define CHKSUM_SIZE 2
#define RFD_DATA_SIZE (ETH_FRAME_LEN + CHKSUM_SIZE + VLAN_SIZE)

/* Bits for bdp->flags */
#define DF_LINK_FC_CAP     0x00000001	/* Link is flow control capable */
#define DF_CSUM_OFFLOAD    0x00000002
#define DF_UCODE_LOADED    0x00000004
#define USE_IPCB           0x00000008	/* set if using ipcb for transmits */
#define IS_BACHELOR        0x00000010	/* set if 82558 or newer board */
#define IS_ICH             0x00000020
#define DF_SPEED_FORCED    0x00000040	/* set if speed is forced */
#define LED_IS_ON	   0x00000080	/* LED is turned ON by the driver */
#define DF_LINK_FC_TX_ONLY 0x00000100	/* Received PAUSE frames are honored*/

typedef struct net_device_stats net_dev_stats_t;

/* needed macros */
/* These macros use the bdp pointer. If you use them it better be defined */
#define PREV_TCB_USED(X)  ((X).tail ? (X).tail - 1 : bdp->params.TxDescriptors - 1)
#define NEXT_TCB_TOUSE(X) ((((X) + 1) >= bdp->params.TxDescriptors) ? 0 : (X) + 1)
#define TCB_TO_USE(X)     ((X).tail)
#define TCBS_AVAIL(X)     (NEXT_TCB_TOUSE( NEXT_TCB_TOUSE((X).tail)) != (X).head)

#define RFD_POINTER(skb,bdp)      ((rfd_t *) (((unsigned char *)((skb)->data))-((bdp)->rfd_size)))
#define SKB_RFD_STATUS(skb,bdp)   ((RFD_POINTER((skb),(bdp)))->rfd_header.cb_status)

/* ====================================================================== */
/*                              82557                                     */
/* ====================================================================== */

/* Changed for 82558 enhancement */
typedef struct _d101_scb_ext_t {
	u32 scb_rx_dma_cnt;	/* Rx DMA byte count */
	u8 scb_early_rx_int;	/* Early Rx DMA byte count */
	u8 scb_fc_thld;	/* Flow Control threshold */
	u8 scb_fc_xon_xoff;	/* Flow Control XON/XOFF values */
	u8 scb_pmdr;	/* Power Mgmt. Driver Reg */
} d101_scb_ext __attribute__ ((__packed__));

/* Changed for 82559 enhancement */
typedef struct _d101m_scb_ext_t {
	u32 scb_rx_dma_cnt;	/* Rx DMA byte count */
	u8 scb_early_rx_int;	/* Early Rx DMA byte count */
	u8 scb_fc_thld;	/* Flow Control threshold */
	u8 scb_fc_xon_xoff;	/* Flow Control XON/XOFF values */
	u8 scb_pmdr;	/* Power Mgmt. Driver Reg */
	u8 scb_gen_ctrl;	/* General Control */
	u8 scb_gen_stat;	/* General Status */
	u16 scb_reserved;	/* Reserved */
	u32 scb_function_event;	/* Cardbus Function Event */
	u32 scb_function_event_mask;	/* Cardbus Function Mask */
	u32 scb_function_present_state;	/* Cardbus Function state */
	u32 scb_force_event;	/* Cardbus Force Event */
} d101m_scb_ext __attribute__ ((__packed__));

/* Changed for 82550 enhancement */
typedef struct _d102_scb_ext_t {
	u32 scb_rx_dma_cnt;	/* Rx DMA byte count */
	u8 scb_early_rx_int;	/* Early Rx DMA byte count */
	u8 scb_fc_thld;	/* Flow Control threshold */
	u8 scb_fc_xon_xoff;	/* Flow Control XON/XOFF values */
	u8 scb_pmdr;	/* Power Mgmt. Driver Reg */
	u8 scb_gen_ctrl;	/* General Control */
	u8 scb_gen_stat;	/* General Status */
	u8 scb_gen_ctrl2;
	u8 scb_reserved;	/* Reserved */
	u32 scb_scheduling_reg;
	u32 scb_reserved2;
	u32 scb_function_event;	/* Cardbus Function Event */
	u32 scb_function_event_mask;	/* Cardbus Function Mask */
	u32 scb_function_present_state;	/* Cardbus Function state */
	u32 scb_force_event;	/* Cardbus Force Event */
} d102_scb_ext __attribute__ ((__packed__));

/*
 * 82557 status control block. this will be memory mapped & will hang of the
 * the bdp, which hangs of the bdp. This is the brain of it.
 */
typedef struct _scb_t {
	u16 scb_status;	/* SCB Status register */
	u8 scb_cmd_low;	/* SCB Command register (low byte) */
	u8 scb_cmd_hi;	/* SCB Command register (high byte) */
	u32 scb_gen_ptr;	/* SCB General pointer */
	u32 scb_port;	/* PORT register */
	u16 scb_flsh_cntrl;	/* Flash Control register */
	u16 scb_eprm_cntrl;	/* EEPROM control register */
	u32 scb_mdi_cntrl;	/* MDI Control Register */
	/* Changed for 82558 enhancement */
	union {
		u32 scb_rx_dma_cnt;	/* Rx DMA byte count */
		d101_scb_ext d101_scb;	/* 82558/9 specific fields */
		d101m_scb_ext d101m_scb;	/* 82559 specific fields */
		d102_scb_ext d102_scb;
	} scb_ext;
} scb_t __attribute__ ((__packed__));

/* Self test
 * This is used to dump results of the self test 
 */
typedef struct _self_test_t {
	u32 st_sign;	/* Self Test Signature */
	u32 st_result;	/* Self Test Results */
} self_test_t __attribute__ ((__packed__));

/* 
 *  Statistical Counters 
 */
/* 82557 counters */
typedef struct _basic_cntr_t {
	u32 xmt_gd_frames;	/* Good frames transmitted */
	u32 xmt_max_coll;	/* Fatal frames -- had max collisions */
	u32 xmt_late_coll;	/* Fatal frames -- had a late coll. */
	u32 xmt_uruns;	/* Xmit underruns (fatal or re-transmit) */
	u32 xmt_lost_crs;	/* Frames transmitted without CRS */
	u32 xmt_deferred;	/* Deferred transmits */
	u32 xmt_sngl_coll;	/* Transmits that had 1 and only 1 coll. */
	u32 xmt_mlt_coll;	/* Transmits that had multiple coll. */
	u32 xmt_ttl_coll;	/* Transmits that had 1+ collisions. */
	u32 rcv_gd_frames;	/* Good frames received */
	u32 rcv_crc_errs;	/* Aligned frames that had a CRC error */
	u32 rcv_algn_errs;	/* Receives that had alignment errors */
	u32 rcv_rsrc_err;	/* Good frame dropped cuz no resources */
	u32 rcv_oruns;	/* Overrun errors - bus was busy */
	u32 rcv_err_coll;	/* Received frms. that encountered coll. */
	u32 rcv_shrt_frames;	/* Received frames that were to short */
} basic_cntr_t;

/* 82558 extended statistic counters */
typedef struct _ext_cntr_t {
	u32 xmt_fc_frames;
	u32 rcv_fc_frames;
	u32 rcv_fc_unsupported;
} ext_cntr_t;

/* 82559 TCO statistic counters */
typedef struct _tco_cntr_t {
	u16 xmt_tco_frames;
	u16 rcv_tco_frames;
} tco_cntr_t;

/* Structures to access thet physical dump area */
/* Use one of these types, according to the statisitcal counters mode,
   to cast the pointer to the physical dump area and access the cmd_complete
   DWORD. */

/* 557-mode : only basic counters + cmd_complete */
typedef struct _err_cntr_557_t {
	basic_cntr_t basic_stats;
	u32 cmd_complete;
} err_cntr_557_t;

/* 558-mode : basic + extended counters + cmd_complete */
typedef struct _err_cntr_558_t {
	basic_cntr_t basic_stats;
	ext_cntr_t extended_stats;
	u32 cmd_complete;
} err_cntr_558_t;

/* 559-mode : basic + extended + TCO counters + cmd_complete */
typedef struct _err_cntr_559_t {
	basic_cntr_t basic_stats;
	ext_cntr_t extended_stats;
	tco_cntr_t tco_stats;
	u32 cmd_complete;
} err_cntr_559_t;

/* This typedef defines the struct needed to hold the largest number of counters */
typedef err_cntr_559_t max_counters_t;

/* Different statistical-counters mode the controller may be in */
typedef enum _stat_mode_t {
	E100_BASIC_STATS = 0,	/* 82557 stats : 16 counters / 16 dw */
	E100_EXTENDED_STATS,	/* 82558 stats : 19 counters / 19 dw */
	E100_TCO_STATS		/* 82559 stats : 21 counters / 20 dw */
} stat_mode_t;

/* dump statistical counters complete codes */
#define DUMP_STAT_COMPLETED	0xA005
#define DUMP_RST_STAT_COMPLETED	0xA007

/* Command Block (CB) Generic Header Structure*/
typedef struct _cb_header_t {
	u16 cb_status;	/* Command Block Status */
	u16 cb_cmd;	/* Command Block Command */
	u32 cb_lnk_ptr;	/* Link To Next CB */
} cb_header_t __attribute__ ((__packed__));

//* Individual Address Command Block (IA_CB)*/
typedef struct _ia_cb_t {
	cb_header_t ia_cb_hdr;
	u8 ia_addr[ETH_ALEN];
} ia_cb_t __attribute__ ((__packed__));

/* Configure Command Block (CONFIG_CB)*/
typedef struct _config_cb_t {
	cb_header_t cfg_cbhdr;
	u8 cfg_byte[CB_CFIG_BYTE_COUNT + CB_CFIG_D102_BYTE_COUNT];
} config_cb_t __attribute__ ((__packed__));

/* MultiCast Command Block (MULTICAST_CB)*/
typedef struct _multicast_cb_t {
	cb_header_t mc_cbhdr;
	u16 mc_count;	/* Number of multicast addresses */
	u8 mc_addr[(ETH_ALEN * MAX_MULTICAST_ADDRS)];
} mltcst_cb_t __attribute__ ((__packed__));

#define UCODE_MAX_DWORDS	134
/* Load Microcode Command Block (LOAD_UCODE_CB)*/
typedef struct _load_ucode_cb_t {
	cb_header_t load_ucode_cbhdr;
	u32 ucode_dword[UCODE_MAX_DWORDS];
} load_ucode_cb_t __attribute__ ((__packed__));

/* Load Programmable Filter Data*/
typedef struct _filter_cb_t {
	cb_header_t filter_cb_hdr;
	u32 filter_data[MAX_FILTER];
} filter_cb_t __attribute__ ((__packed__));

/* NON_TRANSMIT_CB -- Generic Non-Transmit Command Block 
 */
typedef struct _nxmit_cb_t {
	union {
		config_cb_t config;
		ia_cb_t setup;
		load_ucode_cb_t load_ucode;
		mltcst_cb_t multicast;
		filter_cb_t filter;
	} ntcb;
} nxmit_cb_t __attribute__ ((__packed__));

/*Block for queuing for postponed execution of the non-transmit commands*/
typedef struct _nxmit_cb_entry_t {
	struct list_head list_elem;
	nxmit_cb_t *non_tx_cmd;
	dma_addr_t dma_addr;
	unsigned long expiration_time;
} nxmit_cb_entry_t;

/* States for postponed non tx commands execution */
typedef enum _non_tx_cmd_state_t {
	E100_NON_TX_IDLE = 0,	/* No queued NON-TX commands */
	E100_WAIT_TX_FINISH,	/* Wait for completion of the TX activities */
	E100_WAIT_NON_TX_FINISH	/* Wait for completion of the non TX command */
} non_tx_cmd_state_t;

/* some defines for the ipcb */
#define IPCB_IP_CHECKSUM_ENABLE 	BIT_4
#define IPCB_TCPUDP_CHECKSUM_ENABLE	BIT_5
#define IPCB_TCP_PACKET 		BIT_6
#define IPCB_LARGESEND_ENABLE 		BIT_7
#define IPCB_HARDWAREPARSING_ENABLE	BIT_0
#define IPCB_INSERTVLAN_ENABLE 		BIT_1
#define IPCB_IP_ACTIVATION_DEFAULT      IPCB_HARDWAREPARSING_ENABLE

/* Transmit Buffer Descriptor (TBD)*/
typedef struct _tbd_t {
	u32 tbd_buf_addr;	/* Physical Transmit Buffer Address */
	u16 tbd_buf_cnt;	/* Actual Count Of Bytes */
	u16 padd;
} tbd_t __attribute__ ((__packed__));

/* d102 specific fields */
typedef struct _tcb_ipcb_t {
	u16 schedule_low;
	u8 ip_schedule;
	u8 ip_activation_high;
	u16 vlan;
	u8 ip_header_offset;
	u8 tcp_header_offset;
	union {
		u32 sec_rec_phys_addr;
		u32 tbd_zero_address;
	} tbd_sec_addr;
	union {
		u16 sec_rec_size;
		u16 tbd_zero_size;
	} tbd_sec_size;
	u16 total_tcp_payload;
} tcb_ipcb_t __attribute__ ((__packed__));

#define E100_TBD_ARRAY_SIZE (2+MAX_SKB_FRAGS)

/* Transmit Command Block (TCB)*/
struct _tcb_t {
	cb_header_t tcb_hdr;
	u32 tcb_tbd_ptr;	/* TBD address */
	u16 tcb_cnt;	/* Data Bytes In TCB past header */
	u8 tcb_thrshld;	/* TX Threshold for FIFO Extender */
	u8 tcb_tbd_num;

	union {
		tcb_ipcb_t ipcb;	/* d102 ipcb fields */
		tbd_t tbd_array[E100_TBD_ARRAY_SIZE];
	} tcbu;

	/* From here onward we can dump anything we want as long as the
	 * size of the total structure is a multiple of a paragraph
	 * boundary ( i.e. -16 bit aligned ).
	 */
	tbd_t *tbd_ptr;

	u32 tcb_tbd_dflt_ptr;	/* TBD address for non-segmented packet */
	u32 tcb_tbd_expand_ptr;	/* TBD address for segmented packet */

	struct sk_buff *tcb_skb;	/* the associated socket buffer */
	dma_addr_t tcb_phys;	/* phys addr of the TCB */
} __attribute__ ((__packed__));

#define _TCB_T_
typedef struct _tcb_t tcb_t;

/* Receive Frame Descriptor (RFD) - will be using the simple model*/
struct _rfd_t {
	/* 8255x */
	cb_header_t rfd_header;
	u32 rfd_rbd_ptr;	/* Receive Buffer Descriptor Addr */
	u16 rfd_act_cnt;	/* Number Of Bytes Received */
	u16 rfd_sz;	/* Number Of Bytes In RFD */
	/* D102 aka Gamla */
	u16 vlanid;
	u8 rcvparserstatus;
	u8 reserved;
	u16 securitystatus;
	u8 checksumstatus;
	u8 zerocopystatus;
	u8 pad[8];	/* data should be 16 byte aligned */
	u8 data[RFD_DATA_SIZE];

} __attribute__ ((__packed__));

#define _RFD_T_
typedef struct _rfd_t rfd_t;

/* Receive Buffer Descriptor (RBD)*/
typedef struct _rbd_t {
	u16 rbd_act_cnt;	/* Number Of Bytes Received */
	u16 rbd_filler;
	u32 rbd_lnk_addr;	/* Link To Next RBD */
	u32 rbd_rcb_addr;	/* Receive Buffer Address */
	u16 rbd_sz;	/* Receive Buffer Size */
	u16 rbd_filler1;
} rbd_t __attribute__ ((__packed__));

/*
 * This structure is used to maintain a FIFO access to a resource that is 
 * maintained as a circular queue. The resource to be maintained is pointed
 * to by the "data" field in the structure below. In this driver the TCBs', 
 * TBDs' & RFDs' are maintained  as a circular queue & are managed thru this
 * structure.
 */
typedef struct _buf_pool_t {
	unsigned int head;	/* index to first used resource */
	unsigned int tail;	/* index to last used resource */
	void *data;		/* points to resource pool */
} buf_pool_t;

/*Rx skb holding structure*/
struct rx_list_elem {
	struct list_head list_elem;
	dma_addr_t dma_addr;
	struct sk_buff *skb;
};

enum next_cu_cmd_e { RESUME_NO_WAIT = 0, RESUME_WAIT, START_WAIT };
enum zlock_state_e { ZLOCK_INITIAL, ZLOCK_READING, ZLOCK_SLEEPING };
enum tx_queue_stop_type { LONG_STOP = 0, SHORT_STOP };

/* 64 bit aligned size */
#define E100_SIZE_64A(X) ((sizeof(X) + 7) & ~0x7)

typedef struct _bd_dma_able_t {
	char selftest[E100_SIZE_64A(self_test_t)];
	char stats_counters[E100_SIZE_64A(max_counters_t)];
} bd_dma_able_t;

/* bit masks for bool parameters */
#define PRM_XSUMRX       0x00000001
#define PRM_UCODE        0x00000002
#define PRM_FC           0x00000004
#define PRM_IFS          0x00000008
#define PRM_BUNDLE_SMALL 0x00000010

struct cfg_params {
	int e100_speed_duplex;
	int RxDescriptors;
	int TxDescriptors;
	int IntDelay;
	int BundleMax;
	int ber;
	u32 b_params;
};
struct ethtool_lpbk_data{
        dma_addr_t dma_handle;
        tcb_t *tcb;
        rfd_t *rfd;

};

struct e100_private {
	struct vlan_group *vlgrp;
	u32 flags;		/* board management flags */
	u32 tx_per_underrun;	/* number of good tx frames per underrun */
	unsigned int tx_count;	/* count of tx frames, so we can request an interrupt */
	u8 tx_thld;		/* stores transmit threshold */
	u16 eeprom_size;
	u32 pwa_no;		/* PWA: xxxxxx-0xx */
	u8 perm_node_address[ETH_ALEN];
	struct list_head active_rx_list;	/* list of rx buffers */
	struct list_head rx_struct_pool;	/* pool of rx buffer struct headers */
	u16 rfd_size;			/* size of the adapter's RFD struct */
	int skb_req;			/* number of skbs neede by the adapter */
	u8 intr_mask;			/* mask for interrupt status */

	void *dma_able;			/* dma allocated structs */
	dma_addr_t dma_able_phys;
	self_test_t *selftest;		/* pointer to self test area */
	dma_addr_t selftest_phys;	/* phys addr of selftest */
	max_counters_t *stats_counters;	/* pointer to stats table */
	dma_addr_t stat_cnt_phys;	/* phys addr of stat counter area */

	stat_mode_t stat_mode;	/* statistics mode: extended, TCO, basic */
	scb_t *scb;		/* memory mapped ptr to 82557 scb */

	tcb_t *last_tcb;	/* pointer to last tcb sent */
	buf_pool_t tcb_pool;	/* adapter's TCB array */
	dma_addr_t tcb_phys;	/* phys addr of start of TCBs */

	u16 cur_line_speed;
	u16 cur_dplx_mode;

	struct net_device *device;
	struct pci_dev *pdev;
	struct driver_stats drv_stats;

	u8 rev_id;		/* adapter PCI revision ID */

	unsigned int phy_addr;	/* address of PHY component */
	unsigned int PhyId;	/* ID of PHY component */
	unsigned int PhyState;	/* state for the fix squelch algorithm */
	unsigned int PhyDelay;	/* delay for the fix squelch algorithm */

	/* Lock defintions for the driver */
	spinlock_t bd_lock;		/* board lock */
	spinlock_t bd_non_tx_lock;	/* Non transmit command lock  */
	spinlock_t config_lock;		/* config block lock */
	spinlock_t mdi_access_lock;	/* mdi lock */

	struct timer_list watchdog_timer;	/* watchdog timer id */

	/* non-tx commands parameters */
	struct timer_list nontx_timer_id;	/* non-tx timer id */
	struct list_head non_tx_cmd_list;
	non_tx_cmd_state_t non_tx_command_state;
	nxmit_cb_entry_t *same_cmd_entry[CB_MAX_NONTX_CMD];

	enum next_cu_cmd_e next_cu_cmd;

	/* Zero Locking Algorithm data members */
	enum zlock_state_e zlock_state;
	u8 zlock_read_data[16];	/* number of times each value 0-15 was read */
	u16 zlock_read_cnt;	/* counts number of reads */
	ulong zlock_sleep_cnt;	/* keeps track of "sleep" time */

	u8 config[CB_CFIG_BYTE_COUNT + CB_CFIG_D102_BYTE_COUNT];

	/* IFS params */
	u8 ifs_state;
	u8 ifs_value;

	struct cfg_params params;	/* adapter's command line parameters */

	u32 speed_duplex_caps;	/* adapter's speed/duplex capabilities */

	/* WOL params for ethtool */
	u32 wolsupported;
	u32 wolopts;
	u16 ip_lbytes;
	struct ethtool_lpbk_data loopback;
	struct timer_list blink_timer;	/* led blink timer id */

#ifdef CONFIG_PM
	u32 pci_state[16];
#endif
#ifdef E100_CU_DEBUG	
	u8 last_cmd;
	u8 last_sub_cmd;
#endif	
};

#define E100_AUTONEG        0
#define E100_SPEED_10_HALF  1
#define E100_SPEED_10_FULL  2
#define E100_SPEED_100_HALF 3
#define E100_SPEED_100_FULL 4

/********* function prototypes *************/
extern int e100_open(struct net_device *);
extern int e100_close(struct net_device *);
extern void e100_isolate_driver(struct e100_private *bdp);
extern unsigned char e100_hw_init(struct e100_private *);
extern void e100_sw_reset(struct e100_private *bdp, u32 reset_cmd);
extern u8 e100_start_cu(struct e100_private *bdp, tcb_t *tcb);
extern void e100_free_non_tx_cmd(struct e100_private *bdp,
				 nxmit_cb_entry_t *non_tx_cmd);
extern nxmit_cb_entry_t *e100_alloc_non_tx_cmd(struct e100_private *bdp);
extern unsigned char e100_exec_non_cu_cmd(struct e100_private *bdp,
					  nxmit_cb_entry_t *cmd);
extern unsigned char e100_selftest(struct e100_private *bdp, u32 *st_timeout,
				   u32 *st_result);
extern unsigned char e100_get_link_state(struct e100_private *bdp);
extern unsigned char e100_wait_scb(struct e100_private *bdp);

extern void e100_deisolate_driver(struct e100_private *bdp, u8 full_reset);
extern unsigned char e100_configure_device(struct e100_private *bdp);
#ifdef E100_CU_DEBUG
extern unsigned char e100_cu_unknown_state(struct e100_private *bdp);
#endif

#define ROM_TEST_FAIL		0x01
#define REGISTER_TEST_FAIL	0x02
#define SELF_TEST_FAIL		0x04
#define TEST_TIMEOUT		0x08

enum test_offsets {
	test_link,
	test_eeprom,
	test_self_test,
	test_loopback_mac,
	test_loopback_phy,
	cable_diag,
	max_test_res,  /* must be last */
};

#endif
