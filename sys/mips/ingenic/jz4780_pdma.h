/*-
 * Copyright (c) 2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

/* DMA Channel Registers */
#define	PDMA_DSA(n)	(0x00 + 0x20 * n)	/* Channel n Source Address */
#define	PDMA_DTA(n)	(0x04 + 0x20 * n)	/* Channel n Target Address */
#define	PDMA_DTC(n)	(0x08 + 0x20 * n)	/* Channel n Transfer Count */
#define	PDMA_DRT(n)	(0x0C + 0x20 * n)	/* Channel n Request Source */
#define	PDMA_DCS(n)	(0x10 + 0x20 * n)	/* Channel n Control/Status */
#define	PDMA_DCM(n)	(0x14 + 0x20 * n)	/* Channel n Command */
#define	PDMA_DDA(n)	(0x18 + 0x20 * n)	/* Channel n Descriptor Address */
#define	PDMA_DSD(n)	(0x1C + 0x20 * n)	/* Channel n Stride Difference */

/* Global Control Registers */
#define	PDMA_DMAC	0x1000	/* DMA Control */
#define	PDMA_DIRQP	0x1004	/* DMA Interrupt Pending */
#define	PDMA_DDB	0x1008	/* DMA Doorbell */
#define	PDMA_DDS	0x100C	/* DMA Doorbell Set */
#define	PDMA_DIP	0x1010	/* Descriptor Interrupt Pending */
#define	PDMA_DIC	0x1014	/* Descriptor Interrupt Clear */
#define	PDMA_DMACP	0x101C	/* DMA Channel Programmable */
#define	PDMA_DSIRQP	0x1020	/* Channel soft IRQ to MCU */
#define	PDMA_DSIRQM	0x1024	/* Channel soft IRQ mask */
#define	PDMA_DCIRQP	0x1028	/* Channel IRQ to MCU */
#define	PDMA_DCIRQM	0x102C	/* Channel IRQ to MCU mask */
#define	PDMA_DMCS	0x1030	/* MCU Control and Status */
#define	PDMA_DMNMB	0x1034	/* MCU Normal Mailbox */
#define	PDMA_DMSMB	0x1038	/* MCU Security Mailbox */
#define	PDMA_DMINT	0x103C	/* MCU Interrupt */

struct pdma_hwdesc {
	uint32_t dcm;		/* DMA Channel Command */
	uint32_t dsa;		/* DMA Source Address */
	uint32_t dta;		/* DMA Target Address */
	uint32_t dtc;		/* DMA Transfer Counter */
	uint32_t sd;		/* Stride Address */
	uint32_t drt;		/* DMA Request Type */
	uint32_t reserved[2];
};
