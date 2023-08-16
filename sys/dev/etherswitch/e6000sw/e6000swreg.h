/*-
 * Copyright (c) 2015 Semihalf
 * Copyright (c) 2015 Stormshield
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
 */

#ifndef _E6000SWREG_H_
#define	_E6000SWREG_H_

struct atu_opt {
	uint16_t mac_01;
	uint16_t mac_23;
	uint16_t mac_45;
	uint16_t fid;
};

/*
 * Definitions for the Marvell 88E6000 series Ethernet Switch.
 */

/* Switch IDs. */
#define	MV88E6141	0x3400
#define	MV88E6341	0x3410
#define	MV88E6352	0x3520
#define	MV88E6172	0x1720
#define	MV88E6176	0x1760
#define	MV88E6190	0x1900

#define	MVSWITCH(_sc, id)	((_sc)->swid == (id))
#define	MVSWITCH_MULTICHIP(_sc)	((_sc)->sw_addr != 0)

/*
 * Switch Registers
 */
#define	REG_GLOBAL			0x1b
#define	REG_GLOBAL2			0x1c
#define	REG_PORT(_sc, p)		((MVSWITCH((_sc), MV88E6190) ? 0 : 0x10) + (p))

#define	REG_NUM_MAX			31

/*
 * Per-Port Switch Registers
 */
#define	PORT_STATUS			0x0
#define	PORT_STATUS_SPEED_MASK		0x300
#define	PORT_STATUS_SPEED_10		0
#define	PORT_STATUS_SPEED_100		1
#define	PORT_STATUS_SPEED_1000		2
#define	PORT_STATUS_DUPLEX_MASK		(1 << 10)
#define	PORT_STATUS_LINK_MASK		(1 << 11)
#define	PORT_STATUS_PHY_DETECT_MASK	(1 << 12)

#define	PSC_CONTROL			0x1
#define	PSC_CONTROL_FORCED_SPD		(1 << 13)
#define	PSC_CONTROL_ALT_SPD		(1 << 12)
#define	PSC_CONTROL_EEE_ON		(1 << 9)
#define	PSC_CONTROL_FORCED_EEE		(1 << 8)
#define	PSC_CONTROL_FC_ON		(1 << 7)
#define	PSC_CONTROL_FORCED_FC		(1 << 6)
#define	PSC_CONTROL_LINK_UP		(1 << 5)
#define	PSC_CONTROL_FORCED_LINK		(1 << 4)
#define	PSC_CONTROL_FULLDPX		(1 << 3)
#define	PSC_CONTROL_FORCED_DPX		(1 << 2)
#define	PSC_CONTROL_SPD10G		0x3
#define	PSC_CONTROL_SPD2500		PSC_CONTROL_SPD10G
#define	PSC_CONTROL_SPD1000		0x2
#define	SWITCH_ID			0x3
#define	PORT_CONTROL			0x4
#define	PORT_CONTROL1			0x5
#define	 PORT_CONTROL1_LAG_PORT		(1 << 14)
#define	 PORT_CONTROL1_LAG_ID_MASK	0xf
#define	 PORT_CONTROL1_LAG_ID_SHIFT	8
#define	 PORT_CONTROL1_FID_MASK		0xf
#define	PORT_VLAN_MAP			0x6
#define	PORT_VID			0x7
#define	PORT_CONTROL2			0x8
#define	PORT_ASSOCIATION_VECTOR		0xb
#define	PORT_ATU_CTRL			0xc
#define	RX_COUNTER			0x12
#define	TX_COUNTER			0x13

#define	PORT_MASK(_sc)			0x7f
#define	PORT_VID_DEF_VID		0
#define	PORT_VID_DEF_VID_MASK		0xfff
#define	PORT_VID_PRIORITY_MASK		0xc00

#define	PORT_CONTROL_DISABLED		0
#define	PORT_CONTROL_BLOCKING		1
#define	PORT_CONTROL_LEARNING		2
#define	PORT_CONTROL_FORWARDING		3
#define	PORT_CONTROL_ENABLE		3
#define	PORT_CONTROL_FRAME		0x0300
#define	PORT_CONTROL_EGRESS		0x3000
#define	PORT_CONTROL2_DOT1Q		0x0c00
#define	PORT_CONTROL2_DISC_TAGGED	(1 << 9)
#define	PORT_CONTROL2_DISC_UNTAGGED	(1 << 8)

/* PORT_VLAN fields */
#define	PORT_VLAN_MAP_FID		12
#define	PORT_VLAN_MAP_FID_MASK		0xf000

/*
 * Switch Global Register 1 accessed via REG_GLOBAL_ADDR
 */
#define	SWITCH_GLOBAL_STATUS		0
#define	SWITCH_GLOBAL_STATUS_IR		(1 << 11)
#define	SWITCH_GLOBAL_CONTROL		4
#define	SWITCH_GLOBAL_CONTROL2		28

#define	MONITOR_CONTROL			26

/* VTU operation */
#define	VTU_FID				2
#define	VTU_OPERATION			5
#define	VTU_VID				6
#define	VTU_DATA			7
#define	VTU_DATA2			8

#define	VTU_FID_MASK(_sc)		(MVSWITCH((_sc), MV88E6190) ? 0xfff : 0xff)
#define	VTU_FID_POLICY			(1 << 12)
#define	VTU_PORT_UNMODIFIED		0
#define	VTU_PORT_UNTAGGED		1
#define	VTU_PORT_TAGGED			2
#define	VTU_PORT_DISCARD		3
#define	VTU_PPREG(_sc)			(MVSWITCH((_sc), MV88E6190) ? 8 : 4)
#define	VTU_PORT(_sc, p)		(((p) % VTU_PPREG(_sc)) * (16 / VTU_PPREG(_sc)))
#define	VTU_PORT_MASK			3
#define	VTU_BUSY			(1 << 15)
#define	VTU_VID_VALID			(1 << 12)
#define	VTU_VID_MASK			0xfff

/* VTU opcodes */
#define	VTU_OP_MASK			(7 << 12)
#define	VTU_NOP				(0 << 12)
#define	VTU_FLUSH			(1 << 12)
#define	VTU_LOAD			(3 << 12)
#define	VTU_PURGE			(3 << 12)
#define	VTU_GET_NEXT			(4 << 12)
#define	STU_LOAD			(5 << 12)
#define	STU_PURGE			(5 << 12)
#define	STU_GET_NEXT			(6 << 12)
#define	VTU_GET_VIOLATION_DATA		(7 << 12)
#define	VTU_CLEAR_VIOLATION_DATA	(7 << 12)

/* ATU operation */
#define	ATU_FID				1
#define	ATU_CONTROL			10
#define	ATU_OPERATION			11
#define	ATU_DATA			12
#define	ATU_MAC_ADDR01			13
#define	ATU_MAC_ADDR23			14
#define	ATU_MAC_ADDR45			15

#define	ATU_DATA_LAG                    (1 << 15)
#define	ATU_PORT_MASK(_sc)		(MVSWITCH((_sc), MV88E6190) ? 0xfff0 : 0xff0)
#define	ATU_PORT_SHIFT                  4
#define	ATU_LAG_MASK                    0xf0
#define	ATU_LAG_SHIFT                   4
#define	ATU_STATE_MASK                  0xf
#define	ATU_UNIT_BUSY			(1 << 15)
#define	ENTRY_STATE			0xf

/* ATU_CONTROL fields */
#define	ATU_CONTROL_AGETIME		4
#define	ATU_CONTROL_AGETIME_MASK	0xff0
#define	ATU_CONTROL_LEARN2ALL		3

/* ATU opcode */
#define	ATU_OP_MASK                     (7 << 12)
#define	NO_OPERATION                    (0 << 12)
#define	FLUSH_ALL                       (1 << 12)
#define	FLUSH_NON_STATIC                (2 << 12)
#define	LOAD_FROM_FIB                   (3 << 12)
#define	PURGE_FROM_FIB                  (3 << 12)
#define	GET_NEXT_IN_FIB                 (4 << 12)
#define	FLUSH_ALL_IN_FIB                (5 << 12)
#define	FLUSH_NON_STATIC_IN_FIB         (6 << 12)
#define	GET_VIOLATION_DATA              (7 << 12)
#define	CLEAR_VIOLATION_DATA            (7 << 12)

/* ATU Stats */
#define	COUNT_ALL			(0 << 0)

/*
 * Switch Global Register 2 accessed via REG_GLOBAL2_ADDR
 */
#define	MGMT_EN_2x			2
#define	MGMT_EN_0x			3
#define	SWITCH_MGMT			5
#define	LAG_MASK			7
#define	LAG_MAPPING			8
#define	ATU_STATS			14

#define	MGMT_EN_ALL			0xffff
#define	LAG_UPDATE			(1 << 15)
#define	LAG_MASKNUM_SHIFT		12
#define	LAGID_SHIFT			11

/* SWITCH_MGMT fields */

#define	SWITCH_MGMT_PRI			0
#define	SWITCH_MGMT_PRI_MASK		7
#define	SWITCH_MGMT_RSVD2CPU		3
#define	SWITCH_MGMT_FC_PRI		4
#define	SWITCH_MGMT_FC_PRI_MASK		(7 << 4)
#define	SWITCH_MGMT_FORCEFLOW		7

/* ATU_STATS fields */

#define	ATU_STATS_BIN			14
#define	ATU_STATS_FLAG			12

/* Offset of SMI registers in multi-chip setup. */
#define	SMI_CMD				0
#define	SMI_DATA			1

/*
 * 'Switch Global Registers 2' (REG_GLOBAL2).
 */

/* EEPROM registers */
#define	EEPROM_CMD			0x14
#define	 EEPROM_BUSY			(1 << 15)
#define	 EEPROM_READ_CMD		(4 << 12)
#define	 EEPROM_WRITE_CMD		(3 << 12)
#define	 EEPROM_WRITE_EN		(1 << 10)
#define	 EEPROM_DATA_MASK		0xff
#define	EEPROM_ADDR			0x15

/* PHY registers */
#define	SMI_PHY_CMD_REG			0x18
#define	 SMI_CMD_BUSY			(1 << 15)
#define	 SMI_CMD_MODE_C22		(1 << 12)
#define	 SMI_CMD_C22_WRITE		(1 << 10)
#define	 SMI_CMD_C22_READ		(2 << 10)
#define	 SMI_CMD_OP_C22_WRITE						\
	 (SMI_CMD_C22_WRITE | SMI_CMD_BUSY | SMI_CMD_MODE_C22)
#define	 SMI_CMD_OP_C22_READ						\
	 (SMI_CMD_C22_READ | SMI_CMD_BUSY | SMI_CMD_MODE_C22)
#define	 SMI_CMD_C45			(0 << 12)
#define	 SMI_CMD_C45_ADDR		(0 << 10)
#define	 SMI_CMD_C45_WRITE		(1 << 10)
#define	 SMI_CMD_C45_READ		(3 << 10)
#define	 SMI_CMD_OP_C45_ADDR						\
	 (SMI_CMD_C45_ADDR | SMI_CMD_BUSY | SMI_CMD_C45)
#define	 SMI_CMD_OP_C45_WRITE						\
	 (SMI_CMD_C45_WRITE | SMI_CMD_BUSY | SMI_CMD_C45)
#define	 SMI_CMD_OP_C45_READ						\
	 (SMI_CMD_C45_READ | SMI_CMD_BUSY | SMI_CMD_C45)
#define	 SMI_CMD_DEV_ADDR		5
#define	 SMI_CMD_DEV_ADDR_MASK		0x3e0
#define	 SMI_CMD_REG_ADDR_MASK		0x1f
#define	SMI_PHY_DATA_REG		0x19
#define	 PHY_DATA_MASK			0xffff

#define	PHY_PAGE_REG			22

/*
 * Scratch and Misc register accessed via
 * 'Switch Global Registers' (REG_GLOBAL2)
 */
#define	SCR_AND_MISC_REG		0x1a

#define	SCR_AND_MISC_PTR_CFG		0x7000
#define	SCR_AND_MISC_DATA_CFG_MASK	0xf0

/* SERDES registers. */
#define	E6000SW_SERDES_DEV		4
#define	E6000SW_SERDES_PCS_CTL1		0x1000
#define	E6000SW_SERDES_SGMII_CTL	0x2000
#define	 E6000SW_SERDES_PDOWN		(1 << 11)

#define	E6000SW_NUM_VLANS		128
#define	E6000SW_NUM_LAGMASK		8
#define	E6000SW_NUM_PHY_REGS		29
#define	E6000SW_MAX_PORTS		11
#define	E6000SW_DEFAULT_AGETIME		20
#define	E6000SW_RETRIES			100
#define	E6000SW_SMI_TIMEOUT		16

#endif /* _E6000SWREG_H_ */
