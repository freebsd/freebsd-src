/*
 * Copyright (c) 1997 Ted Faber
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Ted Faber.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* PCI/CardBus Device IDs */
#define	PCI_DEVICE_ID_PCIC_OZ6729	0x67291217ul
#define	PCI_DEVICE_ID_PCIC_OZ6730	0x673A1217ul
#define	PCI_DEVICE_ID_PCIC_OZ6832	0x68321217ul
#define	PCI_DEVICE_ID_PCIC_CLPD6729	0x11001013ul
#define	PCI_DEVICE_ID_PCIC_CLPD6832	0x11101013ul
#define PCI_DEVICE_ID_PCIC_TI1031	0xac13104cul
#define	PCI_DEVICE_ID_PCIC_TI1130	0xac12104cul
#define	PCI_DEVICE_ID_PCIC_TI1131	0xac15104cul
#define	PCI_DEVICE_ID_PCIC_TI1211	0xac1e104cul
#define	PCI_DEVICE_ID_PCIC_TI1220	0xac17104cul
#define	PCI_DEVICE_ID_PCIC_TI1221	0xac19104cul
#define	PCI_DEVICE_ID_PCIC_TI1225	0xac1c104cul
#define	PCI_DEVICE_ID_PCIC_TI1250	0xac16104cul
#define	PCI_DEVICE_ID_PCIC_TI1251	0xac1d104cul
#define	PCI_DEVICE_ID_PCIC_TI1251B	0xac1f104cul
#define	PCI_DEVICE_ID_PCIC_TI1410	0xac50104cul
#define	PCI_DEVICE_ID_PCIC_TI1420	0xac51104cul
#define	PCI_DEVICE_ID_PCIC_TI1450	0xac1b104cul
#define	PCI_DEVICE_ID_PCIC_TI1451	0xac52104cul
#define	PCI_DEVICE_ID_TOSHIBA_TOPIC95	0x060a1179ul
#define	PCI_DEVICE_ID_TOSHIBA_TOPIC97	0x060f1179ul
#define	PCI_DEVICE_ID_RICOH_RL5C465	0x04651180ul
#define	PCI_DEVICE_ID_RICOH_RL5C466	0x04661180ul
#define	PCI_DEVICE_ID_RICOH_RL5C475	0x04751180ul
#define	PCI_DEVICE_ID_RICOH_RL5C476	0x04761180ul
#define	PCI_DEVICE_ID_RICOH_RL5C478	0x04781180ul
  
/* CL-PD6832 CardBus defines */
#define	CLPD6832_IO_BASE0		0x002c
#define	CLPD6832_IO_LIMIT0		0x0030
#define	CLPD6832_IO_BASE1		0x0034
#define	CLPD6832_IO_LIMIT1		0x0038
#define	CLPD6832_BRIDGE_CONTROL		0x003c
#define	CLPD6832_LEGACY_16BIT_IOADDR	0x0044
#define	CLPD6832_LEGACY_16BIT_IOENABLE	0x0001
#define	CLPD6832_SOCKET	 		0x004c

/* Configuration constants */
#define	CLPD6832_BCR_MGMT_IRQ_ENA	0x08000000
#define	CLPD6832_BCR_ISA_IRQ		0x00800000
#define	CLPD6832_COMMAND_DEFAULTS	0x00000045
#define	CLPD6832_NUM_REGS		2

/* End of CL-PD6832 defines */
/* Texas Instruments PCI-1130/1131 CardBus Controller */
#define TI113X_PCI_SYSTEM_CONTROL	0x80	/* System Control */
#define TI113X_PCI_RETRY_STATUS		0x90	/* Retry Status */
#define TI113X_PCI_CARD_CONTROL		0x91	/* Card Control */
#define TI113X_PCI_DEVICE_CONTROL	0x92	/* Device Control */
#define TI113X_PCI_BUFFER_CONTROL	0x93	/* Buffer Control */
#define TI113X_PCI_SOCKET_DMA0		0x94	/* Socket DMA Register 0 */
#define TI113X_PCI_SOCKET_DMA1		0x98	/* Socket DMA Register 1 */

/* Card control register (TI113X_SYSTEM_CONTROL == 0x80) */
#define TI113X_SYSCNTL_VCC_PROTECT	0x00200000u
#define TI113X_SYSCNTL_CLKRUN_SEL	0x00000080u
#define	TI113X_SYSCNTL_PWRSAVINGS	0x00000040u
#define TI113X_SYSCNTL_KEEP_CLK		0x00000002u
#define TI113X_SYSCNTL_CLKRUN_ENA	0x00000001u

/* Card control register (TI113X_CARD_CONTROL == 0x91) */
#define TI113X_CARDCNTL_RING_ENA	0x80u
#define TI113X_CARDCNTL_ZOOM_VIDEO	0x40u
#define TI113X_CARDCNTL_PCI_IRQ_ENA	0x20u
#define TI113X_CARDCNTL_PCI_IREQ	0x10u
#define TI113X_CARDCNTL_PCI_CSC		0x08u
#define	TI113X_CARDCNTL_MASK		(TI113X_CARDCNTL_PCI_IRQ_ENA | TI113X_CARDCNTL_PCI_IREQ | TI113X_CARDCNTL_PCI_CSC)
#define	TI113X_FUNC0_VALID		TI113X_CARDCNTL_MASK
#define	TI113X_FUNC1_VALID		(TI113X_CARDCNTL_PCI_IREQ | TI113X_CARDCNTL_PCI_CSC)
/* Reserved bit				0x04u */
#define TI113X_CARDCNTL_SPKR_ENA	0x02u
#define TI113X_CARDCNTL_INT		0x01u

/* Device control register (TI113X_DEVICE_CONTROL == 0x92) */
#define	TI113X_DEVCNTL_5V_SOCKET	0x40u
#define	TI113X_DEVCNTL_3V_SOCKET	0x20u
#define	TI113X_DEVCNTL_INTR_MASK	0x06u
#define	TI113X_DEVCNTL_INTR_NONE	0x00u
#define	TI113X_DEVCNTL_INTR_ISA		0x02u
#define	TI113X_DEVCNTL_INTR_SERIAL	0x04u
/* TI12XX specific code */
#define	TI12XX_DEVCNTL_INTR_ALLSERIAL	0x06u
/* Texas Instruments PCI-1130/1131 CardBus Controller */
#define	TI113X_ExCA_IO_OFFSET0		0x36	/* Offset of I/O window */
#define	TI113X_ExCA_IO_OFFSET1		0x38	/* Offset of I/O window */
#define	TI113X_ExCA_MEM_WINDOW_PAGE	0x3C	/* Memory Window Page */

/* sanpei */

/* For Bridge Control register (CB_PCI_BRIDGE_CTRL) */
#define CB_BCR_MASTER_ABORT	0x0020
#define CB_BCR_CB_RESET		0x0040
#define CB_BCR_INT_EXCA		0x0080
#define CB_BCR_WRITE_POST_EN	0x0400
  /* additional bits for Ricoh's cardbus products */
#define CB_BCR_RL_3E0_EN	0x0800
#define CB_BCR_RL_3E2_EN	0x1000

/* PCI Configuration Registers (common) */
#define	CB_PCI_VENDOR_ID	0x00	/* vendor ID */
#define	CB_PCI_DEVICE_ID	0x02	/* device ID */
#define	CB_PCI_COMMAND		0x04	/* PCI command */
#define	CB_PCI_STATUS		0x06	/* PCI status */
#define	CB_PCI_REVISION_ID	0x08	/* PCI revision ID */
#define	CB_PCI_CLASS		0x09	/* PCI class code */
#define	CB_PCI_CACHE_LINE_SIZE	0x0c	/* Cache line size */
#define	CB_PCI_LATENCY		0x0d	/* PCI latency timer */
#define	CB_PCI_HEADER_TYPE	0x0e	/* PCI header type */
#define	CB_PCI_BIST		0x0f	/* Built-in self test */
#define	CB_PCI_SOCKET_BASE	0x10	/* Socket/ExCA base address reg. */
#define	CB_PCI_CB_STATUS	0x16	/* CardBus Status */
#define	CB_PCI_PCI_BUS_NUM	0x18	/* PCI bus number */
#define	CB_PCI_CB_BUS_NUM	0x19	/* CardBus bus number */
#define	CB_PCI_CB_SUB_BUS_NUM	0x1A	/* Subordinate CardBus bus number */
#define	CB_PCI_CB_LATENCY	0x1A	/* CardBus latency timer */
#define	CB_PCI_MEMBASE0		0x1C	/* Memory base register 0 */
#define	CB_PCI_MEMLIMIT0	0x20	/* Memory limit register 0 */
#define	CB_PCI_MEMBASE1		0x24	/* Memory base register 1 */
#define	CB_PCI_MEMLIMIT1	0x28	/* Memory limit register 1 */
#define	CB_PCI_IOBASE0		0x2C	/* I/O base register 0 */
#define	CB_PCI_IOLIMIT0		0x30	/* I/O limit register 0 */
#define	CB_PCI_IOBASE1		0x34	/* I/O base register 1 */
#define	CB_PCI_IOLIMIT1		0x38	/* I/O limit register 1 */
#define	CB_PCI_INT_LINE		0x3C	/* Interrupt Line */
#define	CB_PCI_INT_PIN		0x3D	/* Interrupt Pin */
#define	CB_PCI_BRIDGE_CTRL	0x3E	/* Bridge Control */
#define	CB_PCI_SUBSYS_VENDOR_ID	0x40	/* Subsystem Vendor ID */
#define	CB_PCI_SUBSYS_ID	0x42	/* Subsystem ID */
#define	CB_PCI_LEGACY16_IOADDR	0x44	/* Legacy 16bit I/O address */
#define	CB_PCI_LEGACY16_IOENABLE 0x01	/* Enable Legacy 16bit I/O address */
