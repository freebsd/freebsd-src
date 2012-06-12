/*-
 * Copyright (c) 2012 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef _DEV_IOC4_REG_H_
#define	_DEV_IOC4_REG_H_

#define	IOC4_CTL_ERR_LO		0x0000
#define	IOC4_CTL_ERR_HI		0x0004
#define	IOC4_CTL_UART_INT	0x0008
#define	IOC4_CTL_MISC_INT	0x000c
#define	IOC4_CTL_UART_INT_SET	0x0010
#define	IOC4_CTL_MISC_INT_SET	0x0014
#define	IOC4_CTL_UART_INT_CLR	0x0018
#define	IOC4_CTL_MISC_INT_CLR	0x001c
#define	IOC4_CTL_CR		0x0020
#define	IOC4_CTL_PCI		0x0024
#define	IOC4_CTL_EXTINT		0x0028

#define	IOC4_CTL_GPIO_SET	0x0030
#define	IOC4_CTL_GPIO_CLR	0x0034
#define	IOC4_CTL_GPIO_DATA	0x0038

#define	IOC4_GPIO_0		0x0040
#define	IOC4_GPIO_1		0x0044
#define	IOC4_GPIO_2		0x0048
#define	IOC4_GPIO_3		0x004C
#define	IOC4_GPIO_4		0x0050
#define	IOC4_GPIO_5		0x0054
#define	IOC4_GPIO_6		0x0058
#define	IOC4_GPIO_7		0x005C

#define	IOC4_ATA_BASE		0x0100
#define	IOC4_ATA_SIZE		0x0100

#define	IOC4_KBD_BASE		0x0200
#define	IOC4_KBD_SIZE		0x0020

#define	IOC4_UART_BASE		0x0300
#define	IOC4_UART_SIZE		0x0100

#define	IOC4_UART_DMA_SIZE	(7 * 4)
#define	IOC4_UART_DMA(x)	(0x0310 + (x) * IOC4_UART_DMA_SIZE)
#define	IOC4_UART_REG_SIZE	8
#define	IOC4_UART_REG(x)	(0x0380 + (x) * IOC4_UART_REG_SIZE)

#endif /* _DEV_IOC4_REG_H_ */
