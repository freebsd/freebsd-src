/*-
 * Copyright (c) 2011-2012 Stefan Bethke.
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

#ifndef _DEV_ETHERSWITCH_RTL8366RBVAR_H_
#define	_DEV_ETHERSWITCH_RTL8366RBVAR_H_

#define RTL8366RB_IIC_ADDR	0xa8
#define RTL_IICBUS_TIMEOUT	100	/* us */
#define RTL_IICBUS_READ		1
#define	RTL_IICBUS_WRITE	0
/* number of times to try and select the chip on the I2C bus */
#define RTL_IICBUS_RETRIES	3
#define RTL_IICBUS_RETRY_SLEEP	(hz/1000)

/* Register definitions */

/* Switch Global Configuration */
#define RTL8366RB_SGCR				0x0000
#define RTL8366RB_SGCR_EN_BC_STORM_CTRL		0x0001
#define RTL8366RB_SGCR_MAX_LENGTH_MASK		0x0030
#define RTL8366RB_SGCR_MAX_LENGTH_1522		0x0000
#define RTL8366RB_SGCR_MAX_LENGTH_1536		0x0010
#define RTL8366RB_SGCR_MAX_LENGTH_1552		0x0020
#define RTL8366RB_SGCR_MAX_LENGTH_9216		0x0030
#define RTL8366RB_SGCR_EN_VLAN			0x2000
#define RTL8366RB_SGCR_EN_VLAN_4KTB		0x4000
#define RTL8366RB_SGCR_EN_QOS			0x8000

/* Port Enable Control: DISABLE_PORT[5:0] */
#define RTL8366RB_PECR				0x0001

/* Switch Security Control 0: DIS_LEARN[5:0] */
#define RTL8366RB_SSCR0				0x0002

/* Switch Security Control 1: DIS_AGE[5:0] */
#define RTL8366RB_SSCR1				0x0003

/* Switch Security Control 2 */
#define RTL8366RB_SSCR2				0x0004
#define RTL8366RB_SSCR2_DROP_UNKNOWN_DA		0x0001

/* Port Link Status: two ports per register */
#define RTL8366RB_PLSR_BASE			0x0014
#define RTL8366RB_PLSR_SPEED_MASK	0x03
#define RTL8366RB_PLSR_SPEED_10		0x00
#define RTL8366RB_PLSR_SPEED_100	0x01
#define RTL8366RB_PLSR_SPEED_1000	0x02
#define RTL8366RB_PLSR_FULLDUPLEX	0x04
#define RTL8366RB_PLSR_LINK		0x10
#define RTL8366RB_PLSR_TXPAUSE		0x20
#define RTL8366RB_PLSR_RXPAUSE		0x40
#define RTL8366RB_PLSR_NO_AUTO		0x80

/* VLAN Member Configuration, 3 registers per VLAN */
#define RTL8366RB_VMCR_BASE			0x0020
#define RTL8366RB_VMCR_MULT		3
#define RTL8366RB_VMCR_DOT1Q_REG	0
#define RTL8366RB_VMCR_DOT1Q_VID_SHIFT	0
#define RTL8366RB_VMCR_DOT1Q_VID_MASK	0x0fff
#define RTL8366RB_VMCR_DOT1Q_PCP_SHIFT	12
#define RTL8366RB_VMCR_DOT1Q_PCP_MASK	0x7000
#define RTL8366RB_VMCR_MU_REG		1
#define RTL8366RB_VMCR_MU_MEMBER_SHIFT	0
#define RTL8366RB_VMCR_MU_MEMBER_MASK	0x00ff
#define RTL8366RB_VMCR_MU_UNTAG_SHIFT	8
#define RTL8366RB_VMCR_MU_UNTAG_MASK	0xff00
#define RTL8366RB_VMCR_FID_REG		2
#define RTL8366RB_VMCR_FID_FID_SHIFT	0
#define RTL8366RB_VMCR_FID_FID_MASK	0x0007
#define RTL8366RB_VMCR(_reg, _vlan) \
	(RTL8366RB_VMCR_BASE + _reg + _vlan * RTL8366RB_VMCR_MULT)
/* VLAN Identifier */
#define RTL8366RB_VMCR_VID(_r) \
	(_r[RTL8366RB_VMCR_DOT1Q_REG] & RTL8366RB_VMCR_DOT1Q_VID_MASK)
/* Priority Code Point */
#define RTL8366RB_VMCR_PCP(_r) \
	((_r[RTL8366RB_VMCR_DOT1Q_REG] & RTL8366RB_VMCR_DOT1Q_PCP_MASK) \
	>> RTL8366RB_VMCR_DOT1Q_PCP_SHIFT)
/* Member ports */
#define RTL8366RB_VMCR_MEMBER(_r) \
	(_r[RTL8366RB_VMCR_MU_REG] & RTL8366RB_VMCR_MU_MEMBER_MASK)
/* Untagged ports */
#define RTL8366RB_VMCR_UNTAG(_r) \
	((_r[RTL8366RB_VMCR_MU_REG] & RTL8366RB_VMCR_MU_UNTAG_MASK) \
	>> RTL8366RB_VMCR_MU_UNTAG_SHIFT)
/* Forwarding ID */
#define RTL8366RB_VMCR_FID(_r) \
	(_r[RTL8366RB_VMCR_FID_REG] & RTL8366RB_VMCR_FID_FID_MASK)

/*
 * Port VLAN Control, 4 ports per register
 * Determines the VID for untagged ingress frames through
 * index into VMC.
 */
#define RTL8366RB_PVCR_BASE			0x0063
#define RTL8366RB_PVCR_PORT_SHIFT	4
#define RTL8366RB_PVCR_PORT_PERREG	(16 / RTL8366RB_PVCR_PORT_SHIFT)
#define RTL8366RB_PVCR_PORT_MASK	0x000f
#define RTL8366RB_PVCR_REG(_port) \
	(RTL8366RB_PVCR_BASE + _port / (RTL8366RB_PVCR_PORT_PERREG))
#define RTL8366RB_PVCR_VAL(_port, _pvlan) \
	((_pvlan & RTL8366RB_PVCR_PORT_MASK) << \
	((_port % RTL8366RB_PVCR_PORT_PERREG) * RTL8366RB_PVCR_PORT_SHIFT))
#define RTL8366RB_PVCR_GET(_port, _val) \
	(((_val) >> ((_port % RTL8366RB_PVCR_PORT_PERREG) * RTL8366RB_PVCR_PORT_SHIFT)) & RTL8366RB_PVCR_PORT_MASK)

/* Reset Control */
#define RTL8366RB_RCR				0x0100
#define RTL8366RB_RCR_HARD_RESET	0x0001
#define RTL8366RB_RCR_SOFT_RESET	0x0002

/* Chip Version Control: CHIP_VER[3:0] */
#define RTL8366RB_CVCR				0x050A
/* Chip Identifier */
#define RTL8366RB_CIR				0x0509
#define RTL8366RB_CIR_ID8366RB		0x5937

/* VLAN Ingress Control 2: [5:0] */
#define RTL8366RB_VIC2R				0x037f

/* MIB registers */
#define RTL8366RB_MCNT_BASE			0x1000
#define RTL8366RB_MCTLR				0x13f0
#define RTL8366RB_MCTLR_BUSY		0x0001
#define RTL8366RB_MCTLR_RESET		0x0002
#define RTL8366RB_MCTLR_RESET_PORT_MASK	0x00fc
#define RTL8366RB_MCTLR_RESET_ALL	0x0800

#define RTL8366RB_MCNT(_port, _r) \
	(RTL8366RB_MCNT_BASE + 0x50 * (_port) + (_r))
#define RTL8366RB_MCTLR_RESET_PORT(_p) \
	(1 << ((_p) + 2))

/* PHY Access Control */
#define RTL8366RB_PACR				0x8000
#define RTL8366RB_PACR_WRITE		0x0000
#define RTL8366RB_PACR_READ			0x0001

/* PHY Access Data */
#define	RTL8366RB_PADR				0x8002

#define RTL8366RB_PHYREG(phy, page, reg) \
	(RTL8366RB_PACR | (1 << (((phy) & 0x1f) + 9)) | (((page) & 0xf) << 5) | ((reg) & 0x1f))

/* general characteristics of the chip */
#define	RTL8366RB_CPU_PORT			5
#define	RTL8366RB_NUM_PORTS			6
#define	RTL8366RB_NUM_PHYS			(RTL8366RB_NUM_PORTS-1)
#define	RTL8366RB_NUM_VLANS			16
#define	RTL8366RB_NUM_PHY_REG			32

#endif
