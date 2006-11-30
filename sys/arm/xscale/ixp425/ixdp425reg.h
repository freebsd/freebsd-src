/*	$NetBSD: ixdp425reg.h,v 1.6 2005/12/11 12:17:09 christos Exp $ */
/*
 * Copyright (c) 2003
 *	Ichiro FUKUHARA <ichiro@ichiro.org>.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ichiro FUKUHARA.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ICHIRO FUKUHARA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ICHIRO FUKUHARA OR THE VOICES IN HIS HEAD BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $FreeBSD$ */
#ifndef	_IXDP425REG_H_
#define	_IXDP425REG_H_
/* GPIOs */
#define	GPIO_PCI_CLK	14
#define	GPIO_PCI_RESET	13
#define	GPIO_PCI_INTA	11
#define	GPIO_PCI_INTB	10
#define	GPIO_PCI_INTC	9
#define	GPIO_PCI_INTD	8
#define	GPIO_I2C_SDA	7
#define  GPIO_I2C_SDA_BIT	(1U << GPIO_I2C_SDA)
#define	GPIO_I2C_SCL	6
#define	  GPIO_I2C_SCL_BIT	(1U << GPIO_I2C_SCL)
/* Interrupt */
#define	PCI_INT_A	IXP425_INT_GPIO_11
#define	PCI_INT_B	IXP425_INT_GPIO_10
#define	PCI_INT_C	IXP425_INT_GPIO_9
#define	PCI_INT_D	IXP425_INT_GPIO_8
#endif /* _IXDP425REG_H_ */
