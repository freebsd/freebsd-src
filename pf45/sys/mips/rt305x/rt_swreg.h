/*-
 * Copyright (c) 2010 Aleksandr Rybalko.
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

#ifndef _RT_SWREG_H_
#define _RT_SWREG_H_

/* XXX: must move to config */
#define RT3052F

#define RT_SW_BASE	0x10110000

#define RT_SW_ISR		0x00

#define 	WATCHDOG1_TMR_EXPIRED	(1<<29)
#define 	WATCHDOG0_TMR_EXPIRED	(1<<28)
#define 	HAS_INTRUDER		(1<<27)
#define 	PORT_ST_CHG		(1<<26)
#define 	BC_STORM		(1<<25)
#define 	MUST_DROP_LAN		(1<<24)
#define 	GLOBAL_QUE_FULL		(1<<23)
#define 	LAN_QUE_FULL6		(1<<20)
#define 	LAN_QUE_FULL5		(1<<19)
#define 	LAN_QUE_FULL4		(1<<18)
#define 	LAN_QUE_FULL3		(1<<17)
#define 	LAN_QUE_FULL2		(1<<16)
#define 	LAN_QUE_FULL1		(1<<15)
#define 	LAN_QUE_FULL0		(1<<14)

#define RT_SW_IMR		0x04

#define RT_SW_FCT0		0x08
#define RT_SW_FCT1		0x0c
#define RT_SW_PFC0		0x10
#define RT_SW_PFC1		0x14
#define RT_SW_PFC2		0x18
#define RT_SW_GQS0		0x1c
#define RT_SW_GQS1		0x20
#define RT_SW_ATS		0x24
#define RT_SW_ATS0		0x28
#define RT_SW_ATS1		0x2c
#define RT_SW_ATS2		0x30
#define RT_SW_WMAD0		0x34
#define RT_SW_WMAD1		0x38
#define RT_SW_WMAD2		0x3c
#define RT_SW_PVIDC0		0x40
#define RT_SW_PVIDC1		0x44
#define RT_SW_PVIDC2		0x48
#define RT_SW_PVIDC3		0x4c
#define RT_SW_VID0		0x50
#define RT_SW_VID1		0x54
#define RT_SW_VID2		0x58
#define RT_SW_VID3		0x5c
#define RT_SW_VID4		0x60
#define RT_SW_VID5		0x64
#define RT_SW_VID6		0x68
#define RT_SW_VID7		0x6c
#define RT_SW_VMSC0		0x70
#define RT_SW_VMSC1		0x74
#define RT_SW_VMSC2		0x78
#define RT_SW_VMSC3		0x7c
#define RT_SW_POA		0x80
#define RT_SW_FPA		0x84
#define RT_SW_PTS		0x88
#define RT_SW_SOCPC		0x8c
#define RT_SW_POC0		0x90
#define RT_SW_POC1		0x94
#define RT_SW_POC2		0x98
#define RT_SW_SGC		0x9c
#define RT_SW_STRT		0xa0
#define RT_SW_LEDP0		0xa4
#define RT_SW_LEDP1		0xa8
#define RT_SW_LEDP2		0xac
#define RT_SW_LEDP3		0xb0
#define RT_SW_LEDP4		0xb4
#define RT_SW_WDTR		0xb8
#define RT_SW_DES		0xbc
#define RT_SW_PCR0		0xc0
#define RT_SW_PCR1		0xc4
#define RT_SW_FPA		0xc8
#define RT_SW_FCT2		0xcc
#define RT_SW_QSS0		0xd0

#define RT_SW_QSS1		0xd4
#define RT_SW_DEC		0xd8
#define 	BRIDGE_IPG_SHIFT	24
#define 	DEBUG_SW_PORT_SEL_SHIFT	3
#define 	DEBUG_SW_PORT_SEL_MASK	0x00000038

#define RT_SW_MTI		0xdc
#define 	SKIP_BLOCKS_SHIFT	7
#define 	SKIP_BLOCKS_MASK	0x0000ff80
#define 	SW_RAM_TEST_DONE	(1<<6)
#define 	AT_RAM_TEST_DONE	(1<<5)
#define 	AT_RAM_TEST_FAIL	(1<<4)
#define 	LK_RAM_TEST_DONE	(1<<3)
#define 	LK_RAM_TEST_FAIL	(1<<2)
#define 	DT_RAM_TEST_DONE	(1<<1)
#define 	DT_RAM_TEST_FAIL	(1<<0)

#define RT_SW_PPC		0xe0
#define 	SW2FE_CNT_SHIFT		16
#define 	FE2SW_CNT_SHIFT		0

#define RT_SW_SGC2		0xe4
#define 	FE2SW_WL_FC_EN	(1<<30)
#define 	LAN_PMAP_P0_IS_LAN		(1<<24)
#define 	LAN_PMAP_P1_IS_LAN		(1<<25)
#define 	LAN_PMAP_P2_IS_LAN		(1<<26)
#define 	LAN_PMAP_P3_IS_LAN		(1<<27)
#define 	LAN_PMAP_P4_IS_LAN		(1<<28)
#define 	LAN_PMAP_P5_IS_LAN		(1<<29)
/* Transmit CPU TPID(810x) port bit map */
#define 	TX_CPU_TPID_BIT_MAP_SHIFT	16
#define 	TX_CPU_TPID_BIT_MAP_MASK	0x007f0000
#define 	ARBITER_LAN_EN			(1<<11)
#define 	CPU_TPID_EN			(1<<10)
#define 	P0_DOUBLE_TAG_EN		(1<<0)
#define 	P1_DOUBLE_TAG_EN		(1<<1)
#define 	P2_DOUBLE_TAG_EN		(1<<2)
#define 	P3_DOUBLE_TAG_EN		(1<<3)
#define 	P4_DOUBLE_TAG_EN		(1<<4)
#define 	P5_DOUBLE_TAG_EN		(1<<5)

#define RT_SW_P0PC		0xe8
#define RT_SW_P1PC		0xec
#define RT_SW_P2PC		0xf0
#define RT_SW_P3PC		0xf4
#define RT_SW_P4PC		0xf8
#define RT_SW_P5PC		0xfc
#define 	BAD_PCOUNT_SHIFT	16
#define 	BAD_PCOUNT_MASK		0xffff0000
#define 	GOOD_PCOUNT_SHIFT	0
#define 	GOOD_PCOUNT_MASK	0x0000ffff

#endif /* _RT_SWREG_H_ */
