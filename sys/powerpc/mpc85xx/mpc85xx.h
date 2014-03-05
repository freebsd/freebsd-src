/*-
 * Copyright (C) 2008 Semihalf, Rafal Jaworowski
 * Copyright 2006 by Juniper Networks.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef _MPC85XX_H_
#define _MPC85XX_H_

/*
 * Configuration control and status registers
 */
extern vm_offset_t		ccsrbar_va;
#define CCSRBAR_VA		ccsrbar_va
#define	OCP85XX_CCSRBAR		(CCSRBAR_VA + 0x0)
#define	OCP85XX_BPTR		(CCSRBAR_VA + 0x20)

/*
 * E500 Coherency Module registers
 */
#define	OCP85XX_EEBPCR		(CCSRBAR_VA + 0x1010)

/*
 * Local access registers
 */
#define	OCP85XX_LAWBAR(n)	(CCSRBAR_VA + 0xc08 + 0x20 * (n))
#define	OCP85XX_LAWSR(n)	(CCSRBAR_VA + 0xc10 + 0x20 * (n))

#define	OCP85XX_TGTIF_LBC	4
#define	OCP85XX_TGTIF_RAM_INTL	11
#define	OCP85XX_TGTIF_RIO	12
#define	OCP85XX_TGTIF_RAM1	15
#define	OCP85XX_TGTIF_RAM2	22

/*
 * L2 cache registers
 */
#define OCP85XX_L2CTL		(CCSRBAR_VA + 0x20000)

/*
 * Power-On Reset configuration
 */
#define	OCP85XX_PORDEVSR	(CCSRBAR_VA + 0xe000c)
#define OCP85XX_PORDEVSR_IO_SEL	0x00780000
#define OCP85XX_PORDEVSR_IO_SEL_SHIFT 19

#define	OCP85XX_PORDEVSR2	(CCSRBAR_VA + 0xe0014)

/*
 * Status Registers.
 */
#define	OCP85XX_RSTCR		(CCSRBAR_VA + 0xe00b0)

/*
 * Prototypes.
 */
uint32_t ccsr_read4(uintptr_t addr);
void ccsr_write4(uintptr_t addr, uint32_t val);
int law_enable(int trgt, u_long addr, u_long size);
int law_disable(int trgt, u_long addr, u_long size);
int law_getmax(void);
int law_pci_target(struct resource *, int *, int *);

#endif /* _MPC85XX_H_ */
