/*
 * Copyright (c) 2002-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License 
 * as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * 
 * Further, this software is distributed without any warranty that it is 
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy, 
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 */

#ifndef _ASM_IA64_SN_IOC4_H
#define _ASM_IA64_SN_IOC4_H


/*
 * ioc4.h - IOC4 chip header file
 */

/* Notes:
 * The IOC4 chip is a 32-bit PCI device that provides 4 serial ports,
 * an IDE bus interface, a PC keyboard/mouse interface, and a real-time
 * external interrupt interface.
 *
 * It includes an optimized DMA buffer management, and a store-and-forward
 * buffer RAM.
 *
 * All IOC4 registers are 32 bits wide.
 */
typedef uint32_t ioc4reg_t;

/*
 * PCI Configuration Space Register Address Map, use offset from IOC4 PCI
 * configuration base such that this can be used for multiple IOC4s
 */
#define IOC4_PCI_ID		0x0	/* ID */

#define IOC4_VENDOR_ID_NUM	0x10A9
#define IOC4_DEVICE_ID_NUM	0x100A 
#define IOC4_ADDRSPACE_MASK	0xfff00000ULL

#define IOC4_PCI_SCR		0x4 /* Status/Command */
#define IOC4_PCI_REV		0x8 /* Revision */
#define IOC4_PCI_LAT		0xC /* Latency Timer */
#define IOC4_PCI_BAR0		0x10 /* IOC4 base address 0 */
#define IOC4_PCI_SIDV		0x2c /* Subsys ID and vendor */
#define IOC4_PCI_CAP 		0x34 /* Capability pointer */
#define IOC4_PCI_LATGNTINT      0x3c /* Max_lat, min_gnt, int_pin, int_line */

/*
 * PCI Memory Space Map 
 */
#define IOC4_PCI_ERR_ADDR_L     0x000	/* Low Error Address */
#define IOC4_PCI_ERR_ADDR_VLD	        (0x1 << 0)
#define IOC4_PCI_ERR_ADDR_MST_ID_MSK    (0xf << 1)
#define IOC4_PCI_ERR_ADDR_MST_NUM_MSK   (0xe << 1)
#define IOC4_PCI_ERR_ADDR_MST_TYP_MSK   (0x1 << 1)
#define IOC4_PCI_ERR_ADDR_MUL_ERR       (0x1 << 5)
#define IOC4_PCI_ERR_ADDR_ADDR_MSK      (0x3ffffff << 6)

/* Master IDs contained in PCI_ERR_ADDR_MST_ID_MSK */
#define IOC4_MST_ID_S0_TX		0
#define IOC4_MST_ID_S0_RX		1
#define IOC4_MST_ID_S1_TX		2
#define IOC4_MST_ID_S1_RX		3
#define IOC4_MST_ID_S2_TX		4
#define IOC4_MST_ID_S2_RX		5
#define IOC4_MST_ID_S3_TX		6
#define IOC4_MST_ID_S3_RX		7
#define IOC4_MST_ID_ATA 		8

#define IOC4_PCI_ERR_ADDR_H	0x004	/* High Error Address */

#define IOC4_SIO_IR	        0x008	/* SIO Interrupt Register */
#define IOC4_OTHER_IR	        0x00C	/* Other Interrupt Register */

/* These registers are read-only for general kernel code. To modify
 * them use the functions in ioc4.c
 */
#define IOC4_SIO_IES_RO         0x010	/* SIO Interrupt Enable Set Reg */
#define IOC4_OTHER_IES_RO       0x014	/* Other Interrupt Enable Set Reg */
#define IOC4_SIO_IEC_RO         0x018	/* SIO Interrupt Enable Clear Reg */
#define IOC4_OTHER_IEC_RO       0x01C	/* Other Interrupt Enable Clear Reg */

#define IOC4_SIO_CR	        0x020	/* SIO Control Reg */
#define IOC4_INT_OUT	        0x028	/* INT_OUT Reg (realtime interrupt) */
#define IOC4_GPCR_S	        0x030	/* GenericPIO Cntrl Set Register */
#define IOC4_GPCR_C	        0x034	/* GenericPIO Cntrl Clear Register */
#define IOC4_GPDR	        0x038	/* GenericPIO Data Register */
#define IOC4_GPPR_0	        0x040	/* GenericPIO Pin Registers */
#define IOC4_GPPR_OFF	        0x4
#define IOC4_GPPR(x)	        (IOC4_GPPR_0+(x)*IOC4_GPPR_OFF)

/* ATAPI Registers */
#define IOC4_ATA_0              0x100	/* Data w/timing */
#define IOC4_ATA_1              0x104	/* Error/Features w/timing */
#define IOC4_ATA_2              0x108	/* Sector Count w/timing */
#define IOC4_ATA_3              0x10C	/* Sector Number w/timing */
#define IOC4_ATA_4              0x110   /* Cyliner Low w/timing */
#define IOC4_ATA_5              0x114	/* Cylinder High w/timing */
#define IOC4_ATA_6              0x118	/* Device/Head w/timing */
#define IOC4_ATA_7              0x11C	/* Status/Command w/timing */
#define IOC4_ATA_0_AUX          0x120	/* Aux Status/Device Cntrl w/timing */
#define IOC4_ATA_TIMING       	0x140	/* Timing value register 0 */
#define IOC4_ATA_DMA_PTR_L      0x144   /* Low Memory Pointer to DMA List */
#define IOC4_ATA_DMA_PTR_H      0x148   /* High Memory Pointer to DMA List */
#define IOC4_ATA_DMA_ADDR_L     0x14C   /* Low Memory DMA Address */
#define IOC4_ATA_DMA_ADDR_H     0x150   /* High Memory DMA Addresss */
#define IOC4_ATA_BC_DEV         0x154	/* DMA Byte Count at Device */
#define IOC4_ATA_BC_MEM         0x158	/* DMA Byte Count at Memory */
#define IOC4_ATA_DMA_CTRL       0x15C	/* DMA Control/Status */

/* Keyboard and Mouse Registers */
#define IOC4_KM_CSR	        0x200	/* Kbd and Mouse Cntrl/Status Reg */
#define IOC4_K_RD	        0x204	/* Kbd Read Data Register */
#define IOC4_M_RD	        0x208	/* Mouse Read Data Register */
#define IOC4_K_WD	        0x20C	/* Kbd Write Data Register */
#define IOC4_M_WD	        0x210	/* Mouse Write Data Register */

/* Serial Port Registers used for DMA mode serial I/O */
#define IOC4_SBBR01_H	        0x300	/* Serial Port Ring Buffers 
                                           Base Reg High for Channels 0 1*/
#define IOC4_SBBR01_L	        0x304	/* Serial Port Ring Buffers 
                                           Base Reg Low for Channels 0 1 */
#define IOC4_SBBR23_H	        0x308	/* Serial Port Ring Buffers 
                                           Base Reg High for Channels 2 3*/
#define IOC4_SBBR23_L	        0x30C	/* Serial Port Ring Buffers 
                                           Base Reg Low for Channels 2 3 */

#define IOC4_SSCR_0	        0x310	/* Serial Port 0 Control */
#define IOC4_STPIR_0	        0x314	/* Serial Port 0 TX Produce */
#define IOC4_STCIR_0	        0x318	/* Serial Port 0 TX Consume */
#define IOC4_SRPIR_0	        0x31C	/* Serial Port 0 RX Produce */
#define IOC4_SRCIR_0	        0x320	/* Serial Port 0 RX Consume */
#define IOC4_SRTR_0	        0x324	/* Serial Port 0 Receive Timer Reg */
#define IOC4_SHADOW_0		0x328	/* Serial Port 0 16550 Shadow Reg */

#define IOC4_SSCR_1	        0x32C	/* Serial Port 1 Control */
#define IOC4_STPIR_1	        0x330	/* Serial Port 1 TX Produce */
#define IOC4_STCIR_1	        0x334	/* Serial Port 1 TX Consume */
#define IOC4_SRPIR_1	        0x338   /* Serial Port 1 RX Produce */
#define IOC4_SRCIR_1	        0x33C	/* Serial Port 1 RX Consume */
#define IOC4_SRTR_1	        0x340	/* Serial Port 1 Receive Timer Reg */
#define IOC4_SHADOW_1		0x344	/* Serial Port 1 16550 Shadow Reg */

#define IOC4_SSCR_2	        0x348	/* Serial Port 2 Control */
#define IOC4_STPIR_2	        0x34C	/* Serial Port 2 TX Produce */
#define IOC4_STCIR_2	        0x350	/* Serial Port 2 TX Consume */
#define IOC4_SRPIR_2	        0x354	/* Serial Port 2 RX Produce */
#define IOC4_SRCIR_2	        0x358	/* Serial Port 2 RX Consume */
#define IOC4_SRTR_2	        0x35C	/* Serial Port 2 Receive Timer Reg */
#define IOC4_SHADOW_2		0x360	/* Serial Port 2 16550 Shadow Reg */

#define IOC4_SSCR_3	        0x364	/* Serial Port 3 Control */
#define IOC4_STPIR_3	        0x368	/* Serial Port 3 TX Produce */
#define IOC4_STCIR_3	        0x36C	/* Serial Port 3 TX Consume */
#define IOC4_SRPIR_3	        0x370	/* Serial Port 3 RX Produce */
#define IOC4_SRCIR_3	        0x374	/* Serial Port 3 RX Consume */
#define IOC4_SRTR_3	        0x378	/* Serial Port 3 Receive Timer Reg */
#define IOC4_SHADOW_3		0x37C	/* Serial Port 3 16550 Shadow Reg */

#define IOC4_UART0_BASE         0x380   /* UART 0 */
#define IOC4_UART1_BASE         0x388   /* UART 1 */
#define IOC4_UART2_BASE         0x390   /* UART 2 */
#define IOC4_UART3_BASE         0x398   /* UART 3 */

/* Private page address aliases for usermode mapping */
#define IOC4_INT_OUT_P	        0x04000	/* INT_OUT Reg */

#define IOC4_SSCR_0_P	        0x08000 /* Serial Port 0 */
#define IOC4_STPIR_0_P	        0x08004
#define IOC4_STCIR_0_P	        0x08008	/* (read-only) */
#define IOC4_SRPIR_0_P	        0x0800C	/* (read-only) */
#define IOC4_SRCIR_0_P	        0x08010
#define IOC4_SRTR_0_P	        0x08014
#define IOC4_UART_LSMSMCR_0_P   0x08018	/* (read-only) */

#define IOC4_SSCR_1_P	        0x0C000	/* Serial Port 1 */
#define IOC4_STPIR_1_P	        0x0C004
#define IOC4_STCIR_1_P	        0x0C008	/* (read-only) */
#define IOC4_SRPIR_1_P	        0x0C00C	/* (read-only) */
#define IOC4_SRCIR_1_P	        0x0C010
#define IOC4_SRTR_1_P	        0x0C014
#define IOC4_UART_LSMSMCR_1_P   0x0C018	/* (read-only) */

#define IOC4_SSCR_2_P	        0x10000	/* Serial Port 2 */
#define IOC4_STPIR_2_P	        0x10004
#define IOC4_STCIR_2_P	        0x10008	/* (read-only) */
#define IOC4_SRPIR_2_P	        0x1000C	/* (read-only) */
#define IOC4_SRCIR_2_P	        0x10010
#define IOC4_SRTR_2_P	        0x10014
#define IOC4_UART_LSMSMCR_2_P   0x10018	/* (read-only) */

#define IOC4_SSCR_3_P	        0x14000	/* Serial Port 3 */
#define IOC4_STPIR_3_P	        0x14004
#define IOC4_STCIR_3_P	        0x14008	/* (read-only) */
#define IOC4_SRPIR_3_P	        0x1400C	/* (read-only) */
#define IOC4_SRCIR_3_P	        0x14010
#define IOC4_SRTR_3_P	        0x14014
#define IOC4_UART_LSMSMCR_3_P   0x14018	/* (read-only) */

#define IOC4_ALIAS_PAGE_SIZE	0x4000

/* Interrupt types */
typedef enum ioc4_intr_type_e {
    ioc4_sio_intr_type,
    ioc4_other_intr_type,
    ioc4_num_intr_types
} ioc4_intr_type_t;
#define ioc4_first_intr_type    ioc4_sio_intr_type

/* Bitmasks for IOC4_SIO_IR, IOC4_SIO_IEC, and IOC4_SIO_IES  */
#define IOC4_SIO_IR_S0_TX_MT		0x00000001 /* Serial port 0 TX empty */
#define IOC4_SIO_IR_S0_RX_FULL		0x00000002 /* Port 0 RX buf full */
#define IOC4_SIO_IR_S0_RX_HIGH		0x00000004 /* Port 0 RX hiwat */
#define IOC4_SIO_IR_S0_RX_TIMER		0x00000008 /* Port 0 RX timeout */
#define IOC4_SIO_IR_S0_DELTA_DCD	0x00000010 /* Port 0 delta DCD */
#define IOC4_SIO_IR_S0_DELTA_CTS	0x00000020 /* Port 0 delta CTS */
#define IOC4_SIO_IR_S0_INT	        0x00000040 /* Port 0 pass-thru intr */
#define IOC4_SIO_IR_S0_TX_EXPLICIT	0x00000080 /* Port 0 explicit TX thru */
#define IOC4_SIO_IR_S1_TX_MT		0x00000100 /* Serial port 1 */
#define IOC4_SIO_IR_S1_RX_FULL		0x00000200 /* */
#define IOC4_SIO_IR_S1_RX_HIGH		0x00000400 /* */
#define IOC4_SIO_IR_S1_RX_TIMER		0x00000800 /* */
#define IOC4_SIO_IR_S1_DELTA_DCD	0x00001000 /* */
#define IOC4_SIO_IR_S1_DELTA_CTS	0x00002000 /* */
#define IOC4_SIO_IR_S1_INT		0x00004000 /* */
#define IOC4_SIO_IR_S1_TX_EXPLICIT	0x00008000 /* */
#define IOC4_SIO_IR_S2_TX_MT		0x00010000 /* Serial port 2 */
#define IOC4_SIO_IR_S2_RX_FULL		0x00020000 /* */
#define IOC4_SIO_IR_S2_RX_HIGH		0x00040000 /* */
#define IOC4_SIO_IR_S2_RX_TIMER		0x00080000 /* */
#define IOC4_SIO_IR_S2_DELTA_DCD	0x00100000 /* */
#define IOC4_SIO_IR_S2_DELTA_CTS	0x00200000 /* */
#define IOC4_SIO_IR_S2_INT		0x00400000 /* */
#define IOC4_SIO_IR_S2_TX_EXPLICIT	0x00800000 /* */
#define IOC4_SIO_IR_S3_TX_MT		0x01000000 /* Serial port 3 */
#define IOC4_SIO_IR_S3_RX_FULL		0x02000000 /* */
#define IOC4_SIO_IR_S3_RX_HIGH		0x04000000 /* */
#define IOC4_SIO_IR_S3_RX_TIMER		0x08000000 /* */
#define IOC4_SIO_IR_S3_DELTA_DCD	0x10000000 /* */
#define IOC4_SIO_IR_S3_DELTA_CTS	0x20000000 /* */
#define IOC4_SIO_IR_S3_INT		0x40000000 /* */
#define IOC4_SIO_IR_S3_TX_EXPLICIT	0x80000000 /* */

/* Per device interrupt masks */
#define IOC4_SIO_IR_S0		(IOC4_SIO_IR_S0_TX_MT | \
				 IOC4_SIO_IR_S0_RX_FULL | \
				 IOC4_SIO_IR_S0_RX_HIGH | \
				 IOC4_SIO_IR_S0_RX_TIMER | \
				 IOC4_SIO_IR_S0_DELTA_DCD | \
				 IOC4_SIO_IR_S0_DELTA_CTS | \
				 IOC4_SIO_IR_S0_INT | \
				 IOC4_SIO_IR_S0_TX_EXPLICIT)
#define IOC4_SIO_IR_S1		(IOC4_SIO_IR_S1_TX_MT | \
				 IOC4_SIO_IR_S1_RX_FULL | \
				 IOC4_SIO_IR_S1_RX_HIGH | \
				 IOC4_SIO_IR_S1_RX_TIMER | \
				 IOC4_SIO_IR_S1_DELTA_DCD | \
				 IOC4_SIO_IR_S1_DELTA_CTS | \
				 IOC4_SIO_IR_S1_INT | \
				 IOC4_SIO_IR_S1_TX_EXPLICIT)
#define IOC4_SIO_IR_S2		(IOC4_SIO_IR_S2_TX_MT | \
				 IOC4_SIO_IR_S2_RX_FULL | \
				 IOC4_SIO_IR_S2_RX_HIGH | \
				 IOC4_SIO_IR_S2_RX_TIMER | \
				 IOC4_SIO_IR_S2_DELTA_DCD | \
				 IOC4_SIO_IR_S2_DELTA_CTS | \
				 IOC4_SIO_IR_S2_INT | \
				 IOC4_SIO_IR_S2_TX_EXPLICIT)
#define IOC4_SIO_IR_S3		(IOC4_SIO_IR_S3_TX_MT | \
				 IOC4_SIO_IR_S3_RX_FULL | \
				 IOC4_SIO_IR_S3_RX_HIGH | \
				 IOC4_SIO_IR_S3_RX_TIMER | \
				 IOC4_SIO_IR_S3_DELTA_DCD | \
				 IOC4_SIO_IR_S3_DELTA_CTS | \
				 IOC4_SIO_IR_S3_INT | \
				 IOC4_SIO_IR_S3_TX_EXPLICIT)

/* Bitmasks for IOC4_OTHER_IR, IOC4_OTHER_IEC, and IOC4_OTHER_IES  */
#define IOC4_OTHER_IR_ATA_INT           0x00000001 /* ATAPI intr pass-thru */
#define IOC4_OTHER_IR_ATA_MEMERR        0x00000002 /* ATAPI DMA PCI error */
#define IOC4_OTHER_IR_S0_MEMERR         0x00000004 /* Port 0 PCI error */
#define IOC4_OTHER_IR_S1_MEMERR         0x00000008 /* Port 1 PCI error */
#define IOC4_OTHER_IR_S2_MEMERR         0x00000010 /* Port 2 PCI error */
#define IOC4_OTHER_IR_S3_MEMERR         0x00000020 /* Port 3 PCI error */
#define IOC4_OTHER_IR_KBD_INT		0x00000040 /* Kbd/mouse intr */
#define IOC4_OTHER_IR_ATA_DMAINT        0x00000089 /* ATAPI DMA intr */
#define IOC4_OTHER_IR_RT_INT		0x00800000 /* RT output pulse */
#define IOC4_OTHER_IR_GEN_INT1		0x02000000 /* RT input pulse */
#define IOC4_OTHER_IR_GEN_INT_SHIFT	        25

/* Per device interrupt masks */
#define IOC4_OTHER_IR_ATA       (IOC4_OTHER_IR_ATA_INT | \
				 IOC4_OTHER_IR_ATA_MEMERR | \
				 IOC4_OTHER_IR_ATA_DMAINT)
#define IOC4_OTHER_IR_RT	(IOC4_OTHER_IR_RT_INT | IOC4_OTHER_IR_GEN_INT1)

/* Macro to load pending interrupts */
#define IOC4_PENDING_SIO_INTRS(mem)     (PCI_INW(&((mem)->sio_ir)) & \
				         PCI_INW(&((mem)->sio_ies_ro)))
#define IOC4_PENDING_OTHER_INTRS(mem)   (PCI_INW(&((mem)->other_ir)) & \
				         PCI_INW(&((mem)->other_ies_ro)))

/* Bitmasks for IOC4_SIO_CR */
#define IOC4_SIO_SR_CMD_PULSE		0x00000004 /* Byte bus strobe length */
#define IOC4_SIO_CR_CMD_PULSE_SHIFT              0
#define IOC4_SIO_CR_ARB_DIAG		0x00000070 /* Current non-ATA PCI bus
                                                      requester (ro) */
#define IOC4_SIO_CR_ARB_DIAG_TX0	0x00000000
#define IOC4_SIO_CR_ARB_DIAG_RX0	0x00000010
#define IOC4_SIO_CR_ARB_DIAG_TX1	0x00000020
#define IOC4_SIO_CR_ARB_DIAG_RX1	0x00000030
#define IOC4_SIO_CR_ARB_DIAG_TX2	0x00000040
#define IOC4_SIO_CR_ARB_DIAG_RX2	0x00000050
#define IOC4_SIO_CR_ARB_DIAG_TX3	0x00000060
#define IOC4_SIO_CR_ARB_DIAG_RX3	0x00000070
#define IOC4_SIO_CR_SIO_DIAG_IDLE	0x00000080 /* 0 -> active request among
                                                      serial ports (ro) */
#define IOC4_SIO_CR_ATA_DIAG_IDLE	0x00000100 /* 0 -> active request from
                                                      ATA port */
#define IOC4_SIO_CR_ATA_DIAG_ACTIVE     0x00000200 /* 1 -> ATA request is winner */ 

/* Bitmasks for IOC4_INT_OUT */
#define IOC4_INT_OUT_COUNT	        0x0000ffff /* Pulse interval timer */
#define IOC4_INT_OUT_MODE	        0x00070000 /* Mode mask */
#define IOC4_INT_OUT_MODE_0             0x00000000 /* Set output to 0 */
#define IOC4_INT_OUT_MODE_1             0x00040000 /* Set output to 1 */
#define IOC4_INT_OUT_MODE_1PULSE        0x00050000 /* Send 1 pulse */
#define IOC4_INT_OUT_MODE_PULSES        0x00060000 /* Send 1 pulse every interval */
#define IOC4_INT_OUT_MODE_SQW           0x00070000 /* Toggle output every interval */
#define IOC4_INT_OUT_DIAG	        0x40000000 /* Diag mode */
#define IOC4_INT_OUT_INT_OUT            0x80000000 /* Current state of INT_OUT */

/* Time constants for IOC4_INT_OUT */
#define IOC4_INT_OUT_NS_PER_TICK        (15 * 520) /* 15 ns PCI clock, multi=520 */
#define IOC4_INT_OUT_TICKS_PER_PULSE             3 /* Outgoing pulse lasts 3
                                                      ticks */
#define IOC4_INT_OUT_US_TO_COUNT(x)	           /* Convert uS to a count value */ \
	(((x) * 10 + IOC4_INT_OUT_NS_PER_TICK / 200) *	\
	 100 / IOC4_INT_OUT_NS_PER_TICK - 1)
#define IOC4_INT_OUT_COUNT_TO_US(x)	           /* Convert count value to uS */ \
	(((x) + 1) * IOC4_INT_OUT_NS_PER_TICK / 1000)
#define IOC4_INT_OUT_MIN_TICKS                   3 /* Min period is width of
                                                      pulse in "ticks" */
#define IOC4_INT_OUT_MAX_TICKS  IOC4_INT_OUT_COUNT /* Largest possible count */

/* Bitmasks for IOC4_GPCR */
#define IOC4_GPCR_DIR	                0x000000ff /* Tristate pin in or out */
#define IOC4_GPCR_DIR_PIN(x)              (1<<(x)) /* Access one of the DIR bits */
#define IOC4_GPCR_EDGE	                0x0000ff00 /* Extint edge or level
                                                      sensitive */
#define IOC4_GPCR_EDGE_PIN(x)        (1<<((x)+7 )) /* Access one of the EDGE bits */

/* Values for IOC4_GPCR */
#define IOC4_GPCR_INT_OUT_EN            0x00100000 /* Enable INT_OUT to pin 0 */
#define IOC4_GPCR_DIR_SER0_XCVR         0x00000010 /* Port 0 Transceiver select
                                                      enable */
#define IOC4_GPCR_DIR_SER1_XCVR         0x00000020 /* Port 1 Transceiver select
                                                      enable */
#define IOC4_GPCR_DIR_SER2_XCVR         0x00000040 /* Port 2 Transceiver select
                                                      enable */
#define IOC4_GPCR_DIR_SER3_XCVR         0x00000080 /* Port 3 Transceiver select
                                                      enable */

/* Defs for some of the generic I/O pins */
#define IOC4_GPCR_UART0_MODESEL	              0x10 /* Pin is output to port 0
                                                      mode sel */
#define IOC4_GPCR_UART1_MODESEL	              0x20 /* Pin is output to port 1
                                                      mode sel */
#define IOC4_GPCR_UART2_MODESEL	              0x40 /* Pin is output to port 2
                                                      mode sel */
#define IOC4_GPCR_UART3_MODESEL	              0x80 /* Pin is output to port 3
                                                      mode sel */

#define IOC4_GPPR_UART0_MODESEL_PIN	         4 /* GIO pin controlling
                                                      uart 0 mode select */
#define IOC4_GPPR_UART1_MODESEL_PIN	         5 /* GIO pin controlling
                                                      uart 1 mode select */
#define IOC4_GPPR_UART2_MODESEL_PIN	         6 /* GIO pin controlling
                                                      uart 2 mode select */
#define IOC4_GPPR_UART3_MODESEL_PIN	         7 /* GIO pin controlling
                                                      uart 3 mode select */

/* Bitmasks for IOC4_ATA_TIMING */
#define IOC4_ATA_TIMING_ADR_SETUP	0x00000003 /* Clocks of addr set-up */
#define IOC4_ATA_TIMING_PULSE_WIDTH	0x000001f8 /* Clocks of read or write
                                                      pulse width */
#define IOC4_ATA_TIMING_RECOVERY	0x0000fe00 /* Clocks before next read
                                                      or write */
#define IOC4_ATA_TIMING_USE_IORDY	0x00010000 /* PIO uses IORDY */

/* Bitmasks for address list elements pointed to by IOC4_ATA_DMA_PTR_<L|H> */
#define IOC4_ATA_ALE_DMA_ADDRESS        0xfffffffffffffffe

/* Bitmasks for byte count list elements pointed to by IOC4_ATA_DMA_PTR_<L|H> */
#define IOC4_ATA_BCLE_BYTE_COUNT        0x000000000000fffe
#define IOC4_ATA_BCLE_LIST_END          0x0000000080000000

/* Bitmasks for IOC4_ATA_BC_<DEV|MEM> */
#define IOC4_ATA_BC_BYTE_CNT            0x0001fffe /* Byte count */

/* Bitmasks for IOC4_ATA_DMA_CTRL */
#define IOC4_ATA_DMA_CTRL_STRAT		0x00000001 /* 1 -> start DMA engine */
#define IOC4_ATA_DMA_CTRL_STOP		0x00000002 /* 1 -> stop DMA engine */
#define IOC4_ATA_DMA_CTRL_DIR		0x00000004 /* 1 -> ATA bus data copied
                                                      to memory */
#define IOC4_ATA_DMA_CTRL_ACTIVE	0x00000008 /* DMA channel is active */
#define IOC4_ATA_DMA_CTRL_MEM_ERROR	0x00000010 /* DMA engine encountered 
						      a PCI error */
/* Bitmasks for IOC4_KM_CSR */
#define IOC4_KM_CSR_K_WRT_PEND  0x00000001 /* Kbd port xmitting or resetting */
#define IOC4_KM_CSR_M_WRT_PEND  0x00000002 /* Mouse port xmitting or resetting */
#define IOC4_KM_CSR_K_LCB       0x00000004 /* Line Cntrl Bit for last KBD write */
#define IOC4_KM_CSR_M_LCB       0x00000008 /* Same for mouse */
#define IOC4_KM_CSR_K_DATA      0x00000010 /* State of kbd data line */
#define IOC4_KM_CSR_K_CLK       0x00000020 /* State of kbd clock line */
#define IOC4_KM_CSR_K_PULL_DATA 0x00000040 /* Pull kbd data line low */
#define IOC4_KM_CSR_K_PULL_CLK  0x00000080 /* Pull kbd clock line low */
#define IOC4_KM_CSR_M_DATA      0x00000100 /* State of mouse data line */
#define IOC4_KM_CSR_M_CLK       0x00000200 /* State of mouse clock line */
#define IOC4_KM_CSR_M_PULL_DATA 0x00000400 /* Pull mouse data line low */
#define IOC4_KM_CSR_M_PULL_CLK  0x00000800 /* Pull mouse clock line low */
#define IOC4_KM_CSR_EMM_MODE	0x00001000 /* Emulation mode */
#define IOC4_KM_CSR_SIM_MODE	0x00002000 /* Clock X8 */
#define IOC4_KM_CSR_K_SM_IDLE   0x00004000 /* Keyboard is idle */
#define IOC4_KM_CSR_M_SM_IDLE   0x00008000 /* Mouse is idle */
#define IOC4_KM_CSR_K_TO	0x00010000 /* Keyboard trying to send/receive */
#define IOC4_KM_CSR_M_TO        0x00020000 /* Mouse trying to send/receive */
#define IOC4_KM_CSR_K_TO_EN     0x00040000 /* KM_CSR_K_TO + KM_CSR_K_TO_EN =
                                              cause SIO_IR to assert */
#define IOC4_KM_CSR_M_TO_EN	0x00080000 /* KM_CSR_M_TO + KM_CSR_M_TO_EN =
                                              cause SIO_IR to assert */
#define IOC4_KM_CSR_K_CLAMP_ONE	0x00100000 /* Pull K_CLK low after rec. one char */
#define IOC4_KM_CSR_M_CLAMP_ONE	0x00200000 /* Pull M_CLK low after rec. one char */
#define IOC4_KM_CSR_K_CLAMP_THREE \
                           	0x00400000 /* Pull K_CLK low after rec. three chars */
#define IOC4_KM_CSR_M_CLAMP_THREE \
                            	0x00800000 /* Pull M_CLK low after rec. three char */

/* Bitmasks for IOC4_K_RD and IOC4_M_RD */
#define IOC4_KM_RD_DATA_2       0x000000ff /* 3rd char recvd since last read */
#define IOC4_KM_RD_DATA_2_SHIFT          0
#define IOC4_KM_RD_DATA_1       0x0000ff00 /* 2nd char recvd since last read */
#define IOC4_KM_RD_DATA_1_SHIFT          8
#define IOC4_KM_RD_DATA_0	0x00ff0000 /* 1st char recvd since last read */
#define IOC4_KM_RD_DATA_0_SHIFT         16
#define IOC4_KM_RD_FRAME_ERR_2  0x01000000 /* Framing or parity error in byte 2 */
#define IOC4_KM_RD_FRAME_ERR_1  0x02000000 /* Same for byte 1 */
#define IOC4_KM_RD_FRAME_ERR_0  0x04000000 /* Same for byte 0 */

#define IOC4_KM_RD_KBD_MSE      0x08000000 /* 0 if from kbd, 1 if from mouse */
#define IOC4_KM_RD_OFLO	        0x10000000 /* 4th char recvd before this read */
#define IOC4_KM_RD_VALID_2      0x20000000 /* DATA_2 valid */
#define IOC4_KM_RD_VALID_1      0x40000000 /* DATA_1 valid */
#define IOC4_KM_RD_VALID_0      0x80000000 /* DATA_0 valid */
#define IOC4_KM_RD_VALID_ALL    (IOC4_KM_RD_VALID_0 | IOC4_KM_RD_VALID_1 | \
                                 IOC4_KM_RD_VALID_2)

/* Bitmasks for IOC4_K_WD & IOC4_M_WD */
#define IOC4_KM_WD_WRT_DATA     0x000000ff /* Write to keyboard/mouse port */
#define IOC4_KM_WD_WRT_DATA_SHIFT        0

/* Bitmasks for serial RX status byte */
#define IOC4_RXSB_OVERRUN       0x01       /* Char(s) lost */
#define IOC4_RXSB_PAR_ERR	0x02	   /* Parity error */
#define IOC4_RXSB_FRAME_ERR	0x04	   /* Framing error */
#define IOC4_RXSB_BREAK	        0x08	   /* Break character */
#define IOC4_RXSB_CTS	        0x10	   /* State of CTS */
#define IOC4_RXSB_DCD	        0x20	   /* State of DCD */
#define IOC4_RXSB_MODEM_VALID   0x40	   /* DCD, CTS, and OVERRUN are valid */
#define IOC4_RXSB_DATA_VALID    0x80	   /* Data byte, FRAME_ERR PAR_ERR & BREAK valid */

/* Bitmasks for serial TX control byte */
#define IOC4_TXCB_INT_WHEN_DONE 0x20       /* Interrupt after this byte is sent */
#define IOC4_TXCB_INVALID	0x00	   /* Byte is invalid */
#define IOC4_TXCB_VALID	        0x40	   /* Byte is valid */
#define IOC4_TXCB_MCR	        0x80	   /* Data<7:0> to modem control register */
#define IOC4_TXCB_DELAY	        0xc0	   /* Delay data<7:0> mSec */

/* Bitmasks for IOC4_SBBR_L */
#define IOC4_SBBR_L_SIZE	0x00000001 /* 0 == 1KB rings, 1 == 4KB rings */
#define IOC4_SBBR_L_BASE	0xfffff000 /* Lower serial ring base addr */

/* Bitmasks for IOC4_SSCR_<3:0> */
#define IOC4_SSCR_RX_THRESHOLD  0x000001ff /* Hiwater mark */
#define IOC4_SSCR_TX_TIMER_BUSY 0x00010000 /* TX timer in progress */
#define IOC4_SSCR_HFC_EN	0x00020000 /* Hardware flow control enabled */
#define IOC4_SSCR_RX_RING_DCD   0x00040000 /* Post RX record on delta-DCD */
#define IOC4_SSCR_RX_RING_CTS   0x00080000 /* Post RX record on delta-CTS */
#define IOC4_SSCR_DIAG	        0x00200000 /* Bypass clock divider for sim */
#define IOC4_SSCR_RX_DRAIN	0x08000000 /* Drain RX buffer to memory */
#define IOC4_SSCR_DMA_EN	0x10000000 /* Enable ring buffer DMA */
#define IOC4_SSCR_DMA_PAUSE	0x20000000 /* Pause DMA */
#define IOC4_SSCR_PAUSE_STATE   0x40000000 /* Sets when PAUSE takes effect */
#define IOC4_SSCR_RESET	        0x80000000 /* Reset DMA channels */

/* All producer/comsumer pointers are the same bitfield */
#define IOC4_PROD_CONS_PTR_4K   0x00000ff8 /* For 4K buffers */
#define IOC4_PROD_CONS_PTR_1K   0x000003f8 /* For 1K buffers */
#define IOC4_PROD_CONS_PTR_OFF           3

/* Bitmasks for IOC4_STPIR_<3:0> */
/* Reserved for future register definitions */

/* Bitmasks for IOC4_STCIR_<3:0> */
#define IOC4_STCIR_BYTE_CNT     0x0f000000 /* Bytes in unpacker */
#define IOC4_STCIR_BYTE_CNT_SHIFT       24

/* Bitmasks for IOC4_SRPIR_<3:0> */
#define IOC4_SRPIR_BYTE_CNT	0x0f000000 /* Bytes in packer */
#define IOC4_SRPIR_BYTE_CNT_SHIFT       24

/* Bitmasks for IOC4_SRCIR_<3:0> */
#define IOC4_SRCIR_ARM	        0x80000000 /* Arm RX timer */

/* Bitmasks for IOC4_SHADOW_<3:0> */
#define IOC4_SHADOW_DR          0x00000001  /* Data ready */
#define IOC4_SHADOW_OE          0x00000002  /* Overrun error */
#define IOC4_SHADOW_PE          0x00000004  /* Parity error */
#define IOC4_SHADOW_FE          0x00000008  /* Framing error */
#define IOC4_SHADOW_BI          0x00000010  /* Break interrupt */
#define IOC4_SHADOW_THRE        0x00000020  /* Xmit holding register empty */
#define IOC4_SHADOW_TEMT        0x00000040  /* Xmit shift register empty */
#define IOC4_SHADOW_RFCE        0x00000080  /* Char in RX fifo has an error */
#define IOC4_SHADOW_DCTS        0x00010000  /* Delta clear to send */
#define IOC4_SHADOW_DDCD        0x00080000  /* Delta data carrier detect */
#define IOC4_SHADOW_CTS         0x00100000  /* Clear to send */
#define IOC4_SHADOW_DCD         0x00800000  /* Data carrier detect */
#define IOC4_SHADOW_DTR         0x01000000  /* Data terminal ready */
#define IOC4_SHADOW_RTS         0x02000000  /* Request to send */
#define IOC4_SHADOW_OUT1        0x04000000  /* 16550 OUT1 bit */
#define IOC4_SHADOW_OUT2        0x08000000  /* 16550 OUT2 bit */
#define IOC4_SHADOW_LOOP        0x10000000  /* Loopback enabled */

/* Bitmasks for IOC4_SRTR_<3:0> */
#define IOC4_SRTR_CNT	        0x00000fff /* Reload value for RX timer */
#define IOC4_SRTR_CNT_VAL	0x0fff0000 /* Current value of RX timer */
#define IOC4_SRTR_CNT_VAL_SHIFT         16
#define IOC4_SRTR_HZ                 16000 /* SRTR clock frequency */

/* Serial port register map used for DMA and PIO serial I/O */
typedef volatile struct ioc4_serialregs {
    ioc4reg_t		    sscr;
    ioc4reg_t		    stpir;
    ioc4reg_t		    stcir;
    ioc4reg_t		    srpir;
    ioc4reg_t		    srcir;
    ioc4reg_t		    srtr;
    ioc4reg_t		    shadow;
} ioc4_sregs_t;

/* IOC4 UART register map */
typedef volatile struct ioc4_uartregs {
    char                    i4u_lcr;
    union {
        char                    iir;    /* read only */
        char                    fcr;    /* write only */
    } u3;
    union {
        char                    ier;    /* DLAB == 0 */
        char                    dlm;    /* DLAB == 1 */
    } u2;
    union {
        char                    rbr;    /* read only, DLAB == 0 */
        char                    thr;    /* write only, DLAB == 0 */
        char                    dll;    /* DLAB == 1 */
    } u1;
    char                    i4u_scr;
    char                    i4u_msr;
    char                    i4u_lsr;
    char                    i4u_mcr;
} ioc4_uart_t;


#define i4u_rbr u1.rbr
#define i4u_thr u1.thr
#define i4u_dll u1.dll
#define i4u_ier u2.ier
#define i4u_dlm u2.dlm
#define i4u_iir u3.iir
#define i4u_fcr u3.fcr

/* PCI config space register map */
typedef volatile struct ioc4_configregs {
    ioc4reg_t		    pci_id;
    ioc4reg_t		    pci_scr;
    ioc4reg_t		    pci_rev;
    ioc4reg_t		    pci_lat;
    ioc4reg_t		    pci_bar0;
    ioc4reg_t		    pci_bar1;
    ioc4reg_t               pci_bar2_not_implemented;
    ioc4reg_t               pci_cis_ptr_not_implemented;
    ioc4reg_t		    pci_sidv;
    ioc4reg_t		    pci_rom_bar_not_implemented;
    ioc4reg_t		    pci_cap;
    ioc4reg_t		    pci_rsv;
    ioc4reg_t		    pci_latgntint;

    char                    pci_fill1[0x58 - 0x3c - 4];

    ioc4reg_t               pci_pcix;
    ioc4reg_t               pci_pcixstatus;
} ioc4_cfg_t;

/* PCI memory space register map addressed using pci_bar0 */
typedef volatile struct ioc4_memregs {

    /* Miscellaneous IOC4  registers */
    ioc4reg_t		    pci_err_addr_l;
    ioc4reg_t		    pci_err_addr_h;
    ioc4reg_t		    sio_ir;
    ioc4reg_t		    other_ir;

    /* These registers are read-only for general kernel code.  To
     * modify them use the functions in ioc4.c.
     */
    ioc4reg_t		    sio_ies_ro;
    ioc4reg_t		    other_ies_ro;
    ioc4reg_t		    sio_iec_ro;
    ioc4reg_t		    other_iec_ro;
    ioc4reg_t		    sio_cr;
    ioc4reg_t		    misc_fill1;
    ioc4reg_t		    int_out;
    ioc4reg_t		    misc_fill2;
    ioc4reg_t		    gpcr_s;
    ioc4reg_t		    gpcr_c;
    ioc4reg_t		    gpdr;
    ioc4reg_t		    misc_fill3;
    ioc4reg_t		    gppr_0;
    ioc4reg_t		    gppr_1;
    ioc4reg_t		    gppr_2;
    ioc4reg_t		    gppr_3;
    ioc4reg_t		    gppr_4;
    ioc4reg_t		    gppr_5;
    ioc4reg_t		    gppr_6;
    ioc4reg_t		    gppr_7;

    char		    misc_fill4[0x100 - 0x5C - 4];

    /* ATA/ATAP registers */
    ioc4reg_t		    ata_0;
    ioc4reg_t		    ata_1;
    ioc4reg_t		    ata_2;
    ioc4reg_t		    ata_3;
    ioc4reg_t		    ata_4;
    ioc4reg_t		    ata_5;
    ioc4reg_t		    ata_6;
    ioc4reg_t		    ata_7;
    ioc4reg_t		    ata_aux;

    char		    ata_fill1[0x140 - 0x120 - 4];

    ioc4reg_t		    ata_timing;
    ioc4reg_t		    ata_dma_ptr_l;
    ioc4reg_t		    ata_dma_ptr_h;
    ioc4reg_t		    ata_dma_addr_l;
    ioc4reg_t		    ata_dma_addr_h;
    ioc4reg_t		    ata_bc_dev;
    ioc4reg_t		    ata_bc_mem;
    ioc4reg_t		    ata_dma_ctrl;

    char		    ata_fill2[0x200 - 0x15C - 4];

    /* Keyboard and mouse registers */
    ioc4reg_t		    km_csr;
    ioc4reg_t		    k_rd;
    ioc4reg_t		    m_rd;
    ioc4reg_t		    k_wd;
    ioc4reg_t		    m_wd;

    char		    km_fill1[0x300 - 0x210 - 4];

    /* Serial port registers used for DMA serial I/O */
    ioc4reg_t		    sbbr01_l;
    ioc4reg_t		    sbbr01_h;
    ioc4reg_t		    sbbr23_l;
    ioc4reg_t		    sbbr23_h;

    ioc4_sregs_t	    port_0;
    ioc4_sregs_t	    port_1;
    ioc4_sregs_t	    port_2;
    ioc4_sregs_t	    port_3;

    ioc4_uart_t		    uart_0;
    ioc4_uart_t		    uart_1;
    ioc4_uart_t		    uart_2;
    ioc4_uart_t		    uart_3;
} ioc4_mem_t;


/*
 * Bytebus device space
 */
#define IOC4_BYTEBUS_DEV0	0x80000L  /* Addressed using pci_bar0 */ 
#define IOC4_BYTEBUS_DEV1	0xA0000L  /* Addressed using pci_bar0 */
#define IOC4_BYTEBUS_DEV2	0xC0000L  /* Addressed using pci_bar0 */
#define IOC4_BYTEBUS_DEV3	0xE0000L  /* Addressed using pci_bar0 */

/* UART clock speed */
#define IOC4_SER_XIN_CLK        66000000

typedef enum ioc4_subdevs_e {
    ioc4_subdev_generic,
    ioc4_subdev_kbms,
    ioc4_subdev_tty0,
    ioc4_subdev_tty1,
    ioc4_subdev_tty2,
    ioc4_subdev_tty3,
    ioc4_subdev_rt,
    ioc4_nsubdevs
} ioc4_subdev_t;

/* Subdevice disable bits,
 * from the standard INFO_LBL_SUBDEVS
 */
#define IOC4_SDB_TTY0		(1 << ioc4_subdev_tty0)
#define IOC4_SDB_TTY1		(1 << ioc4_subdev_tty1)
#define IOC4_SDB_TTY2		(1 << ioc4_subdev_tty2)
#define IOC4_SDB_TTY3		(1 << ioc4_subdev_tty3)
#define IOC4_SDB_KBMS		(1 << ioc4_subdev_kbms)
#define IOC4_SDB_RT		(1 << ioc4_subdev_rt)
#define IOC4_SDB_GENERIC	(1 << ioc4_subdev_generic)

#define IOC4_ALL_SUBDEVS	((1 << ioc4_nsubdevs) - 1)

#define IOC4_SDB_SERIAL		(IOC4_SDB_TTY0 | IOC4_SDB_TTY1 | IOC4_SDB_TTY2 | IOC4_SDB_TTY3)

#define IOC4_STD_SUBDEVS	IOC4_ALL_SUBDEVS

#define IOC4_INTA_SUBDEVS	(IOC4_SDB_SERIAL | IOC4_SDB_KBMS | IOC4_SDB_RT | IOC4_SDB_GENERIC)

extern int		ioc4_subdev_enabled(vertex_hdl_t, ioc4_subdev_t);
extern void		ioc4_subdev_enables(vertex_hdl_t, uint64_t);
extern void		ioc4_subdev_enable(vertex_hdl_t, ioc4_subdev_t);
extern void		ioc4_subdev_disable(vertex_hdl_t, ioc4_subdev_t);

/* Macros to read and write the SIO_IEC and SIO_IES registers (see the
 * comments in ioc4.c for details on why this is necessary
 */
#define IOC4_W_IES	0
#define IOC4_W_IEC	1
extern void		ioc4_write_ireg(void *, ioc4reg_t, int, ioc4_intr_type_t);

#define IOC4_WRITE_IES(ioc4, val, type)	ioc4_write_ireg(ioc4, val, IOC4_W_IES, type)
#define IOC4_WRITE_IEC(ioc4, val, type)	ioc4_write_ireg(ioc4, val, IOC4_W_IEC, type)

typedef void
ioc4_intr_func_f	(intr_arg_t, ioc4reg_t);

typedef void
ioc4_intr_connect_f	(struct pci_dev *conn_vhdl,
			 ioc4_intr_type_t,
			 ioc4reg_t,
			 ioc4_intr_func_f *,
			 intr_arg_t info,
			 vertex_hdl_t owner_vhdl,
			 vertex_hdl_t intr_dev_vhdl,
			 int (*)(intr_arg_t));

typedef void
ioc4_intr_disconnect_f	(vertex_hdl_t conn_vhdl,
			 ioc4_intr_type_t,
			 ioc4reg_t,
			 ioc4_intr_func_f *,
			 intr_arg_t info,
			 vertex_hdl_t owner_vhdl);

void ioc4_intr_connect(vertex_hdl_t, ioc4_intr_type_t, ioc4reg_t,
		  ioc4_intr_func_f *, intr_arg_t, vertex_hdl_t,
		  vertex_hdl_t);

extern int		ioc4_is_console(vertex_hdl_t conn_vhdl);

extern void		ioc4_mlreset(ioc4_cfg_t *, ioc4_mem_t *);


extern ioc4_mem_t      *ioc4_mem_ptr(void *ioc4_fastinfo);

typedef ioc4_intr_func_f *ioc4_intr_func_t;

#endif				/* _ASM_IA64_SN_IOC4_H */
