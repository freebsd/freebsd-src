/*-
 * Copyright (c) 2011 Marcel Moolenaar
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

#ifndef _IA64_SGISN_PCIB_H_
#define	_IA64_SGISN_PCIB_H_

#define	PIC_REG_SIZE		(512 * 1024)

#define	PIC_REG_WGT_ID		0x00000
#define	PIC_REG_WGT_STAT	0x00008
#define	PIC_REG_WGT_ERR_H	0x00010
#define	PIC_REG_WGT_ERR		0x00018
#define	PIC_REG_WGT_CTRL	0x00020
#define	PIC_REG_WGT_REQ_TOUT	0x00028
#define	PIC_REG_WGT_INT_H	0x00030
#define	PIC_REG_WGT_INT		0x00038
#define	PIC_REG_WGT_ERRCMD	0x00040
#define	PIC_REG_WGT_LLP		0x00048
#define	PIC_REG_WGT_TFLUSH	0x00050
#define	PIC_REG_WGT_AUX_ERR	0x00058
#define	PIC_REG_WGT_RSP_H	0x00060
#define	PIC_REG_WGT_RSP		0x00068
#define	PIC_REG_WGT_TSTPIN_CTL	0x00070
#define	PIC_REG_WGT_ADDR_LKERR	0x00078

#define	PIC_REG_DIR_MAP		0x00080
#define	PIC_REG_MAP_FAULT	0x00090
#define	PIC_REG_ARBITRATION	0x000a0
#define	PIC_REG_ATE_PARERR	0x000b0
#define	PIC_REG_BUS_TOUT	0x000c0
#define	PIC_REG_PCI_CFG		0x000c8
#define	PIC_REG_PCI_ERR_H	0x000d0
#define	PIC_REG_PCI_ERR		0x000d8

#define	PIC_REG_INT_STATUS	0x00100
#define	PIC_REG_INT_ENABLE	0x00108
#define	PIC_REG_INT_RSTSTAT	0x00110
#define	PIC_REG_INT_MODE	0x00118
#define	PIC_REG_INT_DEVICE	0x00120
#define	PIC_REG_INT_HOSTERR	0x00128
#define	PIC_REG_INT_ADDR(x)	(0x00130 + (x << 3))
#define	PIC_REG_INT_ERRVIEW	0x00170
#define	PIC_REG_INT_MULTI	0x00178
#define	PIC_REG_INT_FORCE(x)	(0x00180 + (x << 3))
#define	PIC_REG_INT_PIN(x)	(0x001c0 + (x << 3))

#define	PIC_REG_DEVICE(x)	(0x00200 + (x << 3))
#define	PIC_REG_WR_REQ(x)	(0x00240 + (x << 3))
#define	PIC_REG_RRB_MAP(x)	(0x00280 + (x << 3))

#endif /* _IA64_SGISN_PCIB_H_ */
