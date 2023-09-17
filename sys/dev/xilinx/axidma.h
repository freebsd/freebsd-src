/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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
 */

#ifndef _DEV_XILINX_AXIDMA_H_
#define _DEV_XILINX_AXIDMA_H_

#define	AXI_DMACR(n)		(0x00 + 0x30 * (n)) /* DMA Control register */
#define	 DMACR_RS		(1 << 0) /* Run / Stop. */
#define	 DMACR_RESET		(1 << 2) /* Soft reset the AXI DMA core. */
#define	 DMACR_IOC_IRQEN	(1 << 12) /* Interrupt on Complete (IOC) Interrupt Enable. */
#define	 DMACR_DLY_IRQEN	(1 << 13) /* Interrupt on Delay Timer Interrupt Enable. */
#define	 DMACR_ERR_IRQEN	(1 << 14) /* Interrupt on Error Interrupt Enable. */
#define	AXI_DMASR(n)		(0x04 + 0x30 * (n)) /* DMA Status register */
#define	 DMASR_HALTED		(1 << 0)
#define	 DMASR_IDLE		(1 << 1)
#define	 DMASR_SGINCLD		(1 << 3) /* Scatter Gather Enabled */
#define	 DMASR_DMAINTERR	(1 << 4) /* DMA Internal Error. */
#define	 DMASR_DMASLVERR	(1 << 5) /* DMA Slave Error. */
#define	 DMASR_DMADECOREERR	(1 << 6) /* Decode Error. */
#define	 DMASR_SGINTERR		(1 << 8) /* Scatter Gather Internal Error. */
#define	 DMASR_SGSLVERR		(1 << 9) /* Scatter Gather Slave Error. */
#define	 DMASR_SGDECERR		(1 << 10) /* Scatter Gather Decode Error. */
#define	 DMASR_IOC_IRQ		(1 << 12) /* Interrupt on Complete. */
#define	 DMASR_DLY_IRQ		(1 << 13) /* Interrupt on Delay. */
#define	 DMASR_ERR_IRQ		(1 << 14) /* Interrupt on Error. */
#define	AXI_CURDESC(n)		(0x08 + 0x30 * (n)) /* Current Descriptor Pointer. Lower 32 bits of the address. */
#define	AXI_CURDESC_MSB(n)	(0x0C + 0x30 * (n)) /* Current Descriptor Pointer. Upper 32 bits of address. */
#define	AXI_TAILDESC(n)		(0x10 + 0x30 * (n)) /* Tail Descriptor Pointer. Lower 32 bits. */
#define	AXI_TAILDESC_MSB(n)	(0x14 + 0x30 * (n)) /* Tail Descriptor Pointer. Upper 32 bits of address. */
#define	AXI_SG_CTL		0x2C /* Scatter/Gather User and Cache */

#define	AXIDMA_NCHANNELS	2
#define	AXIDMA_DESCS_NUM	512
#define	AXIDMA_TX_CHAN		0
#define	AXIDMA_RX_CHAN		1

struct axidma_desc {
	uint32_t next;
	uint32_t reserved1;
	uint32_t phys;
	uint32_t reserved2;
	uint32_t reserved3;
	uint32_t reserved4;
	uint32_t control;
#define	BD_CONTROL_TXSOF	(1 << 27) /* Start of Frame. */
#define	BD_CONTROL_TXEOF	(1 << 26) /* End of Frame. */
#define	BD_CONTROL_LEN_S	0	/* Buffer Length. */
#define	BD_CONTROL_LEN_M	(0x3ffffff << BD_CONTROL_LEN_S)
	uint32_t status;
#define	BD_STATUS_CMPLT		(1 << 31)
#define	BD_STATUS_TRANSFERRED_S	0
#define	BD_STATUS_TRANSFERRED_M	(0x7fffff << BD_STATUS_TRANSFERRED_S)
	uint32_t app0;
	uint32_t app1;
	uint32_t app2;
	uint32_t app3;
	uint32_t app4;
	uint32_t reserved[3];
};

struct axidma_fdt_data {
	int id;
};

#endif /* !_DEV_XILINX_AXIDMA_H_ */
