/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Scott Long
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
 * Thunderbolt 3 register definitions
 */

/* $FreeBSD$ */

#ifndef _NHI_REG_H
#define _NHI_REG_H

/* Some common definitions */
#define TBT_SEC_NONE		0x00
#define TBT_SEC_USER		0x01
#define TBT_SEC_SECURE		0x02
#define TBT_SEC_DP		0x03

#define GENMASK(h, l)	 (((~0U) >> (31 - (h))) ^ ((~0U) >> (31 - (l)) >> 1))

/* PCI Vendor and Device ID's */
#define	VENDOR_INTEL		0x8086
#define DEVICE_AR_2C_NHI	0x1575
#define DEVICE_AR_DP_B_NHI	0x1577
#define	DEVICE_AR_DP_C_NHI	0x15d2
#define	DEVICE_AR_LP_NHI	0x15bf
#define DEVICE_ICL_NHI_0	0x8a17
#define DEVICE_ICL_NHI_1	0x8a0d

#define VENDOR_AMD		0x1022
#define DEVICE_PINK_SARDINE_0	0x1668
#define DEVICE_PINK_SARDINE_1	0x1669

/* * * MMIO Registers
 * * Ring buffer registers
 *
 * 32 transmit and receive rings are available, with Ring 0 being the most
 * important one.  The ring descriptors are 16 bytes each, and each set of
 * TX and RX descriptors are packed together.  There are only definitions
 * for the Ring 0 addresses, others can be directly computed.
 */
#define NHI_TX_RING_ADDR_LO		0x00000
#define NHI_TX_RING_ADDR_HI		0x00004
#define NHI_TX_RING_PICI		0x00008
#define TX_RING_CI_MASK			GENMASK(15, 0)
#define TX_RING_PI_SHIFT		16
#define NHI_TX_RING_SIZE		0x0000c

#define NHI_RX_RING_ADDR_LO		0x08000
#define NHI_RX_RING_ADDR_HI		0x08004
#define NHI_RX_RING_PICI		0x08008
#define RX_RING_CI_MASK			GENMASK(15, 0)
#define RX_RING_PI_SHIFT		16
#define NHI_RX_RING_SIZE		0x0800c
#define RX_RING_BUF_SIZE_SHIFT		16

/*
 * One 32-bit status register encodes one status bit per ring indicates that
 * the watermark from the control descriptor has been reached.
 */
#define NHI_RX_RING_STATUS		0x19400

/*
 * TX and RX Tables.  These are 32 byte control fields for each ring.
 * Only 8 bytes are controllable by the host software, the rest are a
 * shadow copy by the controller of the current packet that's being
 * processed.
 */
#define NHI_TX_RING_TABLE_BASE0		0x19800
#define TX_TABLE_INTERVAL_MASK		GENMASK(23,0) /* Isoch interval 256ns */
#define TX_TABLE_ITE			(1 << 27) /* Isoch tx enable */
#define TX_TABLE_E2E			(1 << 28) /* End-to-end flow control */
#define TX_TABLE_NS			(1 << 29) /* PCIe No Snoop */
#define TX_TABLE_RAW			(1 << 30) /* Raw (1)/frame(0) mode */
#define TX_TABLE_VALID			(1 << 31) /* Table entry is valid */
#define NHI_TX_RING_TABLE_TIMESTAMP	0x19804

#define NHI_RX_RING_TABLE_BASE0		0x29800
#define RX_TABLE_TX_E2E_HOPID_SHIFT	(1 << 12)
#define RX_TABLE_E2E			(1 << 28) /* End-to-end flow control */
#define RX_TABLE_NS			(1 << 29) /* PCIe No Snoop */
#define RX_TABLE_RAW			(1 << 30) /* Raw (1)/frame(0) mode */
#define RX_TABLE_VALID			(1 << 31) /* Table entry is valid */
#define NHI_RX_RING_TABLE_BASE1		0x29804
#define RX_TABLE_EOF_MASK		(1 << 0)
#define RX_TABLE_SOF_MASK		(1 << 16)

/* * Interrupt Control/Status Registers
 * Interrupt Status Register (ISR)
 * Interrupt status for RX, TX, and Nearly Empty events, one bit per
 * MSI-X vector.  Clear on read.
 * Only 12 bits per operation, instead of 16?  I guess it relates to the
 * number paths, advertised in the HOST_CAPS register, which is wired to
 * 0x0c for Alpine Ridge.
 */
#define NHI_ISR0			0x37800
#define ISR0_TX_DESC_SHIFT		0
#define ISR0_RX_DESC_SHIFT		12
#define ISR0_RX_EMPTY_SHIFT		24
#define NHI_ISR1			0x37804
#define ISR1_RX_EMPTY_SHIFT		0

/* * Interrupt Status Clear, corresponds to ISR0/ISR1. Write Only */
#define NHI_ISC0			0x37808
#define NHI_ISC1			0x3780c

/* * Interrupt Status Set, corresponds to ISR0/ISR1.  Write Only */
#define NHI_ISS0			0x37810
#define NHI_ISS1			0x37814

/* * Interrupt Mask, corresponds to ISR0/ISR1.  Read-Write */
#define NHI_IMR0			0x38200
#define NHI_IMR1			0x38204
#define IMR_TX_OFFSET	0
#define IMR_RX_OFFSET	12
#define IMR_NE_OFFSET	24

/* * Interrupt Mask Clear, corresponds to ISR0/ISR1.  Write-only */
#define NHI_IMC0			0x38208
#define NHI_IMC1			0x3820c

/* * Interrupt Mask Set, corresponds to ISR0/ISR1.  Write-only */
#define NHI_IMS0			0x38210
#define NHI_IMS1			0x38214

/*
 *   Interrupt Throttle Rate.  One 32 bit register per interrupt,
 *   16 registers for the 16 MSI-X interrupts.  Interval is in 256ns
 *   increments.
 */
#define NHI_ITR0			0x38c00
#define ITR_INTERVAL_SHIFT		0
#define ITR_COUNTER_SHIFT		16

/*
 *   Interrupt Vector Allocation.
 *   There are 12 4-bit descriptors for TX, 12 4-bit descriptors for RX,
 *   and 12 4-bit descriptors for Nearly Empty.  Each descriptor holds
 *   the numerical value of the MSI-X vector that will receive the
 *   corresponding interrupt.
 *   Bits 0-31 of IVR0 and 0-15 of IVR1 are for TX
 *   Bits 16-31 of IVR1 and 0-31 of IVR2 are for RX
 *   Bits 0-31 of IVR3 and 0-15 of IVR4 are for Nearly Empty
 */
#define NHI_IVR0			0x38c40
#define NHI_IVR1			0x38c44
#define NHI_IVR2			0x38c48
#define NHI_IVR3			0x38c4c
#define NHI_IVR4			0x38c50
#define IVR_TX_OFFSET	0
#define IVR_RX_OFFSET	12
#define IVR_NE_OFFSET	24

/* Native Host Interface Control registers */
#define NHI_HOST_CAPS			0x39640
#define	GET_HOST_CAPS_PATHS(val)	((val) & 0x3f)

/*
 * This definition comes from the Linux driver.  In the USB4 spec, this
 * register is named Host Interface Control, and the Interrupt Autoclear bit
 * is at bit17, not bit2.  The Linux driver doesn't seem to acknowledge this.
 */
#define NHI_DMA_MISC			0x39864
#define DMA_MISC_INT_AUTOCLEAR		(1 << 2)

/* Thunderbolt firmware mailbox registers */
#define TBT_INMAILDATA			0x39900

#define TBT_INMAILCMD			0x39904
#define INMAILCMD_CMD_MASK		0xff
#define INMAILCMD_SAVE_CONNECTED	0x05
#define INMAILCMD_DISCONNECT_PCIE	0x06
#define INMAILCMD_DRIVER_UNLOAD_DISCONNECT	0x07
#define INMAILCMD_DISCONNECT_PORTA		0x10
#define INMAILCMD_DISCONNECT_PORTB		0x11
#define INMAILCMD_SETMODE_CERT_TB_1ST_DEPTH	0x20
#define INMAILCMD_SETMODE_ANY_TB_1ST_DEPTH	0x21
#define INMAILCMD_SETMODE_CERT_TB_ANY_DEPTH	0x22
#define INMAILCMD_SETMODE_ANY_TB_ANY_DEPTH	0x23
#define INMAILCMD_CIO_RESET		0xf0
#define INMAILCMD_ERROR			(1 << 30)
#define INMAILCMD_OPREQ			(1 << 31)

#define TBT_OUTMAILCMD			0x3990c
#define OUTMAILCMD_STATUS_BUSY		(1 << 12)
#define	OUTMAILCMD_OPMODE_MASK		0xf00
#define	OUTMAILCMD_OPMODE_SAFE		0x000
#define	OUTMAILCMD_OPMODE_AUTH		0x100
#define	OUTMAILCMD_OPMODE_ENDPOINT	0x200
#define	OUTMAILCMD_OPMODE_CM_FULL	0x300

#define TBT_FW_STATUS			0x39944
#define FWSTATUS_ENABLE			(1 << 0)
#define FWSTATUS_INVERT			(1 << 1)
#define FWSTATUS_START			(1 << 2)
#define FWSTATUS_CIO_RESET		(1 << 30)
#define FWSTATUS_CM_READY		(1 << 31)

/*
 *  Link Controller (LC) registers.  These are in the Vendor Specific
 *  Extended Capability registers in PCICFG.
 */
#define AR_LC_MBOX_OUT			0x4c
#define ICL_LC_MBOX_OUT			0xf0
#define LC_MBOXOUT_VALID		(1 << 0)
#define LC_MBOXOUT_CMD_SHIFT		1
#define LC_MBOXOUT_CMD_MASK		(0x7f << LC_MBOXOUT_CMD_SHIFT)
#define LC_MBOXOUT_CMD_GO2SX		(0x02 << LC_MBOXOUT_CMD_SHIFT)
#define LC_MBOXOUT_CMD_GO2SX_NOWAKE	(0x03 << LC_MBOXOUT_CMD_SHIFT)
#define LC_MBOXOUT_CMD_SXEXIT_TBT	(0x04 << LC_MBOXOUT_CMD_SHIFT)
#define LC_MBOXOUT_CMD_SXEXIT_NOTBT	(0x05 << LC_MBOXOUT_CMD_SHIFT)
#define LC_MBOXOUT_CMD_OS_UP		(0x06 << LC_MBOXOUT_CMD_SHIFT)
#define LC_MBOXOUT_DATA_SHIFT	8
#define SET_LC_MBOXOUT_DATA(val)	((val) << LC_MBOXOUT_DATA_SHIFT)

#define AR_LC_MBOX_IN			0x48
#define ICL_LC_MBOX_IN			0xec
#define LC_MBOXIN_DONE			(1 << 0)
#define LC_MBOXIN_CMD_SHIFT		1
#define LC_MBOXIN_CMD_MASK		(0x7f << LC_MBOXIN_CMD_SHIFT)
#define LC_MBOXIN_DATA_SHIFT		8
#define GET_LC_MBOXIN_DATA(val)		((val) >> LC_MBOXIN_DATA_SHIFT)

/* Other Vendor Specific registers */
#define AR_VSCAP_1C		0x1c
#define AR_VSCAP_B0		0xb0

#define ICL_VSCAP_9		0xc8
#define ICL_VSCAP9_FWREADY	(1 << 31)
#define ICL_VSCAP_10		0xcc
#define ICL_VSCAP_11		0xd0
#define ICL_VSCAP_22		0xfc
#define ICL_VSCAP22_FORCEPWR	(1 << 1)

/* * Data structures
 * Transmit buffer descriptor, 12.3.1.  Must be aligned on a 4byte boundary.
 */
struct nhi_tx_buffer_desc {
	uint32_t			addr_lo;
	uint32_t			addr_hi;
	uint16_t			eof_len;
#define TX_BUFFER_DESC_LEN_MASK		0xfff
#define TX_BUFFER_DESC_EOF_SHIFT	12
	uint8_t				flags_sof;
#define TX_BUFFER_DESC_SOF_MASK		0xf
#define TX_BUFFER_DESC_IDE		(1 << 4) /* Isoch DMA enable */
#define TX_BUFFER_DESC_DONE		(1 << 5) /* Descriptor Done */
#define TX_BUFFER_DESC_RS		(1 << 6) /* Request Status/Done */
#define TX_BUFFER_DESC_IE		(1 << 7) /* Interrupt Enable */
	uint8_t				offset;
	uint32_t			payload_time;
} __packed;

/*
 * Receive buffer descriptor, 12.4.1.  4 byte aligned.  This goes into
 * the descriptor ring, but changes into the _post form when the
 * controller uses it.
 */
struct nhi_rx_buffer_desc {
	uint32_t			addr_lo;
	uint32_t			addr_hi;
	uint16_t			reserved0;
	uint8_t				flags;
#define RX_BUFFER_DESC_RS		(1 << 6) /* Request Status/Done */
#define RX_BUFFER_DESC_IE		(1 << 7) /* Interrupt Enable */
	uint8_t				offset;
	uint32_t			reserved1;
} __packed;

/*
 * Receive buffer descriptor, after the controller fills it in
 */
struct nhi_rx_post_desc {
	uint32_t			addr_lo;
	uint32_t			addr_hi;
	uint16_t			eof_len;
#define RX_BUFFER_DESC_LEN_MASK		0xfff
#define RX_BUFFER_DESC_EOF_SHIFT	12
	uint8_t				flags_sof;
#define RX_BUFFER_DESC_SOF_MASK		0xf
#define RX_BUFFER_DESC_CRC_ERROR	(1 << 4) /* CRC error (frame mode) */
#define RX_BUFFER_DESC_DONE		(1 << 5) /* Descriptor Done */
#define RX_BUFFER_DESC_OVERRUN		(1 << 6) /* Buffer overrun */
#define RX_BUFFER_DESC_IE		(1 << 7) /* Interrupt Enable */
	uint8_t				offset;
	uint32_t			payload_time;
} __packed;

union nhi_ring_desc {
	struct nhi_tx_buffer_desc	tx;
	struct nhi_rx_buffer_desc	rx;
	struct nhi_rx_post_desc		rxpost;
	uint32_t			dword[4];
};

/* Protocol Defined Field (PDF) */
#define PDF_READ		0x01
#define PDF_WRITE		0x02
#define PDF_NOTIFY		0x03
#define PDF_NOTIFY_ACK		0x04
#define PDF_HOTPLUG		0x05
#define PDF_XDOMAIN_REQ		0x06
#define PDF_XDOMAIN_RESP	0x07
/* Thunderbolt-only */
#define PDF_CM_EVENT		0x0a
#define PDF_CM_REQ		0x0b
#define PDF_CM_RESP		0x0c

#endif /* _NHI_REG_H */
