/*********************************************************************
 *                
 * Filename:      smc-ircc.h
 * Version:       0.3
 * Description:   Definitions for the SMC IrCC chipset
 * Status:        Experimental.
 * Author:        Thomas Davis (tadavis@jps.net)
 *
 *     Copyright (c) 1999-2000, Dag Brattli <dagb@cs.uit.no>
 *     Copyright (c) 1998-1999, Thomas Davis (tadavis@jps.net>
 *     All Rights Reserved
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License 
 *     along with this program; if not, write to the Free Software 
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *     MA 02111-1307 USA
 *
 ********************************************************************/

#ifndef SMC_IRCC_H
#define SMC_IRCC_H

#include <linux/spinlock.h>
#include <linux/pm.h>

#include <net/irda/irport.h>

/* DMA modes needed */
#define DMA_TX_MODE                0x08    /* Mem to I/O, ++, demand. */
#define DMA_RX_MODE                0x04    /* I/O to mem, ++, demand. */

/* Master Control Register */
#define IRCC_MASTER                0x07
#define   IRCC_MASTER_POWERDOWN	   0x80
#define   IRCC_MASTER_RESET        0x40
#define   IRCC_MASTER_INT_EN       0x20
#define   IRCC_MASTER_ERROR_RESET  0x10

/* Register block 0 */

/* Interrupt Identification */
#define IRCC_IIR                   0x01
#define   IRCC_IIR_ACTIVE_FRAME    0x80
#define   IRCC_IIR_EOM             0x40
#define   IRCC_IIR_RAW_MODE        0x20
#define   IRCC_IIR_FIFO		   0x10

/* Interrupt Enable */
#define IRCC_IER                   0x02
#define   IRCC_IER_ACTIVE_FRAME	   0x80
#define   IRCC_IER_EOM 		   0x40
#define   IRCC_IER_RAW_MODE        0x20
#define   IRCC_IER_FIFO		   0x10

/* Line Status Register */
#define IRCC_LSR                   0x03
#define   IRCC_LSR_UNDERRUN        0x80
#define   IRCC_LSR_OVERRUN         0x40
#define   IRCC_LSR_FRAME_ERROR     0x20
#define   IRCC_LSR_SIZE_ERROR      0x10
#define   IRCC_LSR_CRC_ERROR       0x80
#define   IRCC_LSR_FRAME_ABORT 	   0x40

/* Line Control Register A */
#define IRCC_LCR_A                 0x04
#define   IRCC_LCR_A_FIFO_RESET    0x80
#define   IRCC_LCR_A_FAST          0x40
#define   IRCC_LCR_A_GP_DATA       0x20
#define   IRCC_LCR_A_RAW_TX        0x10
#define   IRCC_LCR_A_RAW_RX        0x08
#define   IRCC_LCR_A_ABORT         0x04
#define   IRCC_LCR_A_DATA_DONE     0x02

/* Line Control Register B */
#define IRCC_LCR_B                 0x05
#define   IRCC_LCR_B_SCE_DISABLED  0x00
#define   IRCC_LCR_B_SCE_TRANSMIT  0x40
#define   IRCC_LCR_B_SCE_RECEIVE   0x80
#define   IRCC_LCR_B_SCE_UNDEFINED 0xc0
#define   IRCC_LCR_B_SIP_ENABLE	   0x20
#define   IRCC_LCR_B_BRICK_WALL    0x10

/* Bus Status Register */
#define IRCC_BSR                   0x06
#define   IRCC_BSR_NOT_EMPTY	   0x80
#define   IRCC_BSR_FIFO_FULL	   0x40
#define   IRCC_BSR_TIMEOUT	   0x20

/* Register block 1 */

#define IRCC_FIFO_THRESHOLD	   0x02

#define IRCC_SCE_CFGA	           0x00
#define   IRCC_CFGA_AUX_IR	   0x80
#define   IRCC_CFGA_HALF_DUPLEX	   0x04
#define   IRCC_CFGA_TX_POLARITY	   0x02
#define   IRCC_CFGA_RX_POLARITY	   0x01

#define   IRCC_CFGA_COM		   0x00
#define   IRCC_CFGA_IRDA_SIR_A	   0x08
#define   IRCC_CFGA_ASK_SIR	   0x10
#define   IRCC_CFGA_IRDA_SIR_B	   0x18
#define   IRCC_CFGA_IRDA_HDLC 	   0x20
#define   IRCC_CFGA_IRDA_4PPM 	   0x28
#define   IRCC_CFGA_CONSUMER	   0x30
#define   IRCC_CFGA_RAW_IR	   0x38
#define   IRCC_CFGA_OTHER          0x40

#define IRCC_IR_HDLC               0x04
#define IRCC_IR_4PPM               0x01
#define IRCC_IR_CONSUMER           0x02

#define IRCC_SCE_CFGB	           0x01
#define IRCC_CFGB_LOOPBACK         0x20
#define IRCC_CFGB_LPBCK_TX_CRC	   0x10
#define IRCC_CFGB_NOWAIT	   0x08
#define IRCC_CFGB_STRING_MOVE	   0x04
#define IRCC_CFGB_DMA_BURST 	   0x02
#define IRCC_CFGB_DMA_ENABLE	   0x01

#define IRCC_CFGB_MUX_COM          0x00
#define IRCC_CFGB_MUX_IR           0x40
#define IRCC_CFGB_MUX_AUX          0x80
#define IRCC_CFGB_MUX_INACTIVE	   0xc0

/* Register block 3 - Identification Registers! */
#define IRCC_ID_HIGH	           0x00   /* 0x10 */
#define IRCC_ID_LOW	           0x01   /* 0xB8 */
#define IRCC_CHIP_ID 	           0x02   /* 0xF1 */
#define IRCC_VERSION	           0x03   /* 0x01 */
#define IRCC_INTERFACE	           0x04   /* low 4 = DMA, high 4 = IRQ */

/* Register block 4 - IrDA */
#define IRCC_CONTROL               0x00
#define IRCC_BOF_COUNT_LO          0x01 /* Low byte */
#define IRCC_BOF_COUNT_HI          0x00 /* High nibble (bit 0-3) */
#define IRCC_BRICKWALL_CNT_LO      0x02 /* Low byte */
#define IRCC_BRICKWALL_CNT_HI      0x03 /* High nibble (bit 4-7) */
#define IRCC_TX_SIZE_LO            0x04 /* Low byte */
#define IRCC_TX_SIZE_HI            0x03 /* High nibble (bit 0-3) */
#define IRCC_RX_SIZE_HI            0x05 /* High nibble (bit 0-3) */
#define IRCC_RX_SIZE_LO            0x06 /* Low byte */

#define IRCC_1152                  0x80
#define IRCC_CRC                   0x40

/* Private data for each instance */
struct ircc_cb {
	struct net_device *netdev;     /* Yes! we are some kind of netdevice */
	struct irlap_cb    *irlap; /* The link layer we are binded to */
	
	chipio_t *io;               /* IrDA controller information */
	iobuff_t tx_buff;          /* Transmit buffer */
	iobuff_t rx_buff;          /* Receive buffer */

	struct irport_cb *irport;

	spinlock_t lock;           /* For serializing operations */
	
	__u32 new_speed;
	__u32 flags;               /* Interface flags */

	int tx_buff_offsets[10];   /* Offsets between frames in tx_buff */
	int tx_len;                /* Number of frames in tx_buff */

	struct pm_dev *pmdev;
};

#endif /* SMC_IRCC_H */
