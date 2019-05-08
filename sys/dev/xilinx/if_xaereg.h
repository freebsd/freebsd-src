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
 *
 * $FreeBSD$
 */

#ifndef _DEV_XILINX_IF_XAE_H_
#define _DEV_XILINX_IF_XAE_H_

#define	XAE_RAF		0x00000 /* Reset and Address Filter RW */
#define	XAE_TPF		0x00004 /* Transmit Pause Frame RW */
#define	XAE_IFGP	0x00008 /* Transmit Inter Frame Gap Adjustment RW */
#define	XAE_IS		0x0000C /* Interrupt Status register RW */
#define	XAE_IP		0x00010 /* Interrupt Pending register RO */
#define	XAE_IE		0x00014 /* Interrupt Enable register RW */
#define	XAE_TTAG	0x00018 /* Transmit VLAN Tag RW */
#define	XAE_RTAG	0x0001C /* Receive VLAN Tag  RW */
#define	XAE_UAWL	0x00020 /* Unicast Address Word Lower RW */
#define	XAE_UAWU	0x00024 /* Unicast Address Word Upper RW */
#define	XAE_TPID0	0x00028 /* VLAN TPID Word 0 RW */
#define	XAE_TPID1	0x0002C /* VLAN TPID Word 1 RW */
#define	XAE_PPST	0x00030 /* PCS PMA Status register RO */
#define	XAE_STATCNT(n)	(0x00200 + 0x8 * (n)) /* Statistics Counters RO */
#define	XAE_RCW0	0x00400 /* Receive Configuration Word 0 Register RW */
#define	XAE_RCW1	0x00404 /* Receive Configuration Word 1 Register RW */
#define	 RCW1_RX	(1 << 28) /* Receive Enable */
#define	XAE_TC		0x00408 /* Transmitter Configuration register RW */
#define	 TC_TX		(1 << 28) /* Transmit Enable */
#define	XAE_FCC		0x0040C /* Flow Control Configuration register RW */
#define	 FCC_FCRX	(1 << 29) /* Flow Control Enable (RX) */
#define	XAE_SPEED	0x00410 /* MAC Speed Configuration Word RW */
#define	 SPEED_CONF_S	30
#define	 SPEED_10	(0 << SPEED_CONF_S)
#define	 SPEED_100	(1 << SPEED_CONF_S)
#define	 SPEED_1000	(2 << SPEED_CONF_S)
#define	XAE_RX_MAXFRAME	0x00414 /* RX Max Frame Configuration RW */
#define	XAE_TX_MAXFRAME	0x00418 /* TX Max Frame Configuration RW */
#define	XAE_TX_TIMESTMP	0x0041C /* TX timestamp adjust control register RW */
#define	XAE_IDENT	0x004F8 /* Identification register RO */
#define	XAE_ABILITY	0x004FC /* Ability register RO */
#define	XAE_MDIO_SETUP	0x00500 /* MDIO Setup register RW */
#define	 MDIO_SETUP_ENABLE	(1 << 6) /* MDIO Enable */
#define	 MDIO_SETUP_CLK_DIV_S	0 /* Clock Divide */
#define	XAE_MDIO_CTRL	0x00504 /* MDIO Control RW */
#define	 MDIO_TX_REGAD_S	16 /* This controls the register address being accessed. */
#define	 MDIO_TX_REGAD_M	(0x1f << MDIO_TX_REGAD_S)
#define	 MDIO_TX_PHYAD_S	24 /* This controls the PHY address being accessed. */
#define	 MDIO_TX_PHYAD_M	(0x1f << MDIO_TX_PHYAD_S)
#define	 MDIO_CTRL_TX_OP_S	14 /* Type of access performed. */
#define	 MDIO_CTRL_TX_OP_M	(0x3 << MDIO_CTRL_TX_OP_S)
#define	 MDIO_CTRL_TX_OP_READ	(0x2 << MDIO_CTRL_TX_OP_S)
#define	 MDIO_CTRL_TX_OP_WRITE	(0x1 << MDIO_CTRL_TX_OP_S)
#define	 MDIO_CTRL_INITIATE	(1 << 11) /* Start an MDIO transfer. */
#define	 MDIO_CTRL_READY	(1 << 7) /* MDIO is ready for a new xfer */
#define	XAE_MDIO_WRITE	0x00508 /* MDIO Write Data RW */
#define	XAE_MDIO_READ	0x0050C /* MDIO Read Data RO */
#define	XAE_INT_STATUS	0x00600 /* Interrupt Status Register RW */
#define	XAE_INT_PEND	0x00610 /* Interrupt Pending Register RO */
#define	XAE_INT_ENABLE	0x00620 /* Interrupt Enable Register RW */
#define	XAE_INT_CLEAR	0x00630 /* Interrupt Clear Register RW */
#define	XAE_UAW0	0x00700 /* Unicast Address Word 0 register (UAW0) RW */
#define	XAE_UAW1	0x00704 /* Unicast Address Word 1 register (UAW1) RW */
#define	XAE_FFC		0x00708 /* Frame Filter Control RW */
#define	 FFC_PM		(1 << 31) /* Promiscuous Mode */
#define	XAE_FFV(n)	(0x00710 + 0x4 * (n)) /* Frame Filter Value RW */
#define	XAE_FFMV(n)	(0x00750 + 0x4 * (n)) /* Frame Filter Mask Value RW */
#define	XAE_TX_VLAN(n)	(0x04000 + 0x4 * (n)) /* Transmit VLAN Data Table RW */
#define	XAE_RX_VLAN(n)	(0x08000 + 0x4 * (n)) /* Receive VLAN Data Table RW */
#define	XAE_AVB(n)	(0x10000 + 0x4 * (n)) /* Ethernet AVB RW */
#define	XAE_MAT(n)	(0x20000 + 0x4 * (n)) /* Multicast Address Table RW */

#define	XAE_MULTICAST_TABLE_SIZE	4

/* RX statistical counters. */
#define	RX_BYTES			0
#define	RX_GOOD_FRAMES			18
#define	RX_FRAME_CHECK_SEQ_ERROR	19
#define	RX_GOOD_MCASTS			21
#define	RX_LEN_OUT_OF_RANGE		23
#define	RX_ALIGNMENT_ERRORS		40

/* TX statistical counters. */
#define	TX_BYTES			1
#define	TX_GOOD_FRAMES			27
#define	TX_GOOD_MCASTS			29
#define	TX_GOOD_UNDERRUN_ERRORS		30
#define	TX_SINGLE_COLLISION_FRAMES	34
#define	TX_MULTI_COLLISION_FRAMES	35
#define	TX_LATE_COLLISIONS		37
#define	TX_EXCESS_COLLISIONS		38

#define	XAE_MAX_COUNTERS		43

#endif	/* _DEV_XILINX_IF_XAE_H_ */
