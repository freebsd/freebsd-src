/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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


#ifndef _ACT8846_REG_H_
#define	 _ACT8846_REG_H_


/* ACT8846 registers. */
#define	ACT8846_SYS0		0x00
#define	ACT8846_SYS1		0x01
#define	ACT8846_REG1_CTRL	0x12
#define	ACT8846_REG2_VSET0	0x20
#define	ACT8846_REG2_VSET1	0x21
#define	ACT8846_REG2_CTRL	0x22
#define	ACT8846_REG3_VSET0	0x30
#define	ACT8846_REG3_VSET1	0x31
#define	ACT8846_REG3_CTRL	0x32
#define	ACT8846_REG4_VSET0	0x40
#define	ACT8846_REG4_VSET1	0x41
#define	ACT8846_REG4_CTRL	0x42
#define	ACT8846_REG5_VSET	0x50
#define	ACT8846_REG5_CTRL	0x51
#define	ACT8846_REG6_VSET	0x58
#define	ACT8846_REG6_CTRL	0x59
#define	ACT8846_REG7_VSET	0x60
#define	ACT8846_REG7_CTRL	0x61
#define	ACT8846_REG8_VSET	0x68
#define	ACT8846_REG8_CTRL	0x69
#define	ACT8846_REG9_VSET	0x70
#define	ACT8846_REG9_CTRL	0x71
#define	ACT8846_REG10_VSET	0x80
#define	ACT8846_REG10_CTRL	0x81
#define	ACT8846_REG11_VSET	0x90
#define	ACT8846_REG11_CTRL	0x91
#define	ACT8846_REG12_VSET	0xa0
#define	ACT8846_REG12_CTRL	0xa1
#define	ACT8846_REG13_CTRL	0xb1
#define	ACT8846_PB0		0xc0
#define	ACT8846_PB1		0xc1
#define	ACT8846_PB2		0xc2
#define	ACT8846_PB3		0xc3
#define	ACT8846_PB4		0xc4
#define	ACT8846_GPIO6		0xe3
#define	ACT8846_GPIO5		0xe4
#define	ACT8846_GPIO3		0xf4
#define	ACT8846_GPIO4		0xf5

/* Common REGxx_CTRL bits */
#define	ACT8846_CTRL_ENA	0x80
#define	ACT8846_CTRL_OK		0x01

/* Common REGxx_VSEL bits */
#define	ACT8846_VSEL_MASK	0x3f


#endif /* _ACT8846_REG_H_ */
