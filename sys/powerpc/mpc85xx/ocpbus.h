/*-
 * Copyright 2006 by Juniper Networks. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_OCP85XX_H_
#define _MACHINE_OCP85XX_H_

/*
 * Configuration control and status registers
 */
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

#define	OCP85XX_TGTIF_PCI0	0
#define	OCP85XX_TGTIF_PCI1	1
#define	OCP85XX_TGTIF_PCI2	2
#define	OCP85XX_TGTIF_PCI3	3
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
#define	OCP85XX_PORDEVSR2	(CCSRBAR_VA + 0xe0014)

/*
 * Status Registers.
 */
#define	OCP85XX_RSTCR		(CCSRBAR_VA + 0xe00b0)

/*
 * OCP Bus Definitions
 */
#define	OCP85XX_I2C0_OFF	0x03000
#define	OCP85XX_I2C1_OFF	0x03100
#define	OCP85XX_I2C_SIZE	0x16
#define	OCP85XX_UART0_OFF	0x04500
#define	OCP85XX_UART1_OFF	0x04600
#define	OCP85XX_UART_SIZE	0x10
#define	OCP85XX_LBC_OFF		0x05000
#define	OCP85XX_LBC_SIZE	0x1000
#define	OCP85XX_PCI0_OFF	0x08000
#define	OCP85XX_PCI1_OFF	0x09000
#define	OCP85XX_PCI2_OFF	0x0A000
#define	OCP85XX_PCI3_OFF	0x0B000
#define	OCP85XX_PCI_SIZE	0x1000
#define	OCP85XX_TSEC0_OFF	0x24000
#define	OCP85XX_TSEC1_OFF	0x25000
#define	OCP85XX_TSEC2_OFF	0x26000
#define	OCP85XX_TSEC3_OFF	0x27000
#define	OCP85XX_TSEC_SIZE	0x1000
#define	OCP85XX_OPENPIC_OFF	0x40000
#define	OCP85XX_OPENPIC_SIZE	0x200B4
#define	OCP85XX_QUICC_OFF	0x80000
#define	OCP85XX_QUICC_SIZE	0x20000
#define	OCP85XX_SEC_OFF		0x30000
#define	OCP85XX_SEC_SIZE	0x10000

/*
 * PIC definitions
 */
#define	ISA_IRQ_START	0
#define	PIC_IRQ_START	(ISA_IRQ_START + 16)

#define	ISA_IRQ(n)	(ISA_IRQ_START + (n))
#define	PIC_IRQ_EXT(n)	(PIC_IRQ_START + (n))
#define	PIC_IRQ_INT(n)	(PIC_IRQ_START + 16 + (n))

#endif /* _MACHINE_OCP85XX_H */
