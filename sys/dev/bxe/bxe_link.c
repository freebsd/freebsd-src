/*-
 * Copyright (c) 2007-2010 Broadcom Corporation. All rights reserved.
 *
 *    Gary Zambrano <zambrano@broadcom.com>
 *    David Christensen <davidch@broadcom.com>
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
 * 3. Neither the name of Broadcom Corporation nor the name of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written consent.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bxe.h"
#include "bxe_include.h"
#include "if_bxe.h"

void bxe_write_dmae(struct bxe_softc *, bus_addr_t, uint32_t, uint32_t);
void bxe_read_dmae(struct bxe_softc *, uint32_t, uint32_t);
int bxe_set_gpio(struct bxe_softc *, int, uint32_t, uint8_t);
int bxe_get_gpio(struct bxe_softc *, int, uint8_t);
int bxe_set_spio(struct bxe_softc *, int, uint32_t);
int bxe_set_gpio_int(struct bxe_softc *, int, uint32_t, uint8_t);
int bxe_fw_command(struct bxe_softc *, uint32_t);

#ifdef BXE_DEBUG
extern uint32_t
bxe_reg_read32	(struct bxe_softc *, bus_size_t);
extern void
bxe_reg_write32	(struct bxe_softc *, bus_size_t, uint32_t);
#endif

#undef msleep
#define msleep(m) DELAY(m * 1000)

#define	EMAC_RX_MODE_KEEP_MAC_CONTROL	(1L<<3)
#define	EMAC_RX_MODE_KEEP_VLAN_TAG		(1L<<10)
#define	MDIO_PMA_REG_8481_LED1_MASK		0xa82c
#define	MDIO_PMA_REG_8481_LED2_MASK		0xa82f
#define	MDIO_PMA_REG_8481_LED3_MASK		0xa832
/*
 * [RW 27] 0 - must be active for Everest A0; 1- for Everest B0 when latch
 * logic for interrupts must be used. Enable per bit of interrupt of
 * ~latch_status.latch_status.
 */
#define	NIG_REG_LATCH_BC_0					0x16210
/*
 * [RW 27] Latch for each interrupt from Unicore.b[0]
 * status_emac0_misc_mi_int; b[1] status_emac0_misc_mi_complete;
 * b[2]status_emac0_misc_cfg_change; b[3]status_emac0_misc_link_status;
 * b[4]status_emac0_misc_link_change; b[5]status_emac0_misc_attn;
 * b[6]status_serdes0_mac_crs; b[7]status_serdes0_autoneg_complete;
 * b[8]status_serdes0_fiber_rxact; b[9]status_serdes0_link_status;
 * b[10]status_serdes0_mr_page_rx; b[11]status_serdes0_cl73_an_complete;
 * b[12]status_serdes0_cl73_mr_page_rx; b[13]status_serdes0_rx_sigdet;
 * b[14]status_xgxs0_remotemdioreq; b[15]status_xgxs0_link10g;
 * b[16]status_xgxs0_autoneg_complete; b[17]status_xgxs0_fiber_rxact;
 * b[21:18]status_xgxs0_link_status; b[22]status_xgxs0_mr_page_rx;
 * b[23]status_xgxs0_cl73_an_complete; b[24]status_xgxs0_cl73_mr_page_rx;
 * b[25]status_xgxs0_rx_sigdet; b[26]status_xgxs0_mac_crs
 */
#define	NIG_REG_LATCH_STATUS_0					0x18000

#define	ETH_HLEN			14
#define	ETH_OVREHEAD			(ETH_HLEN + 8)/* 8 for CRC + VLAN*/
#define	ETH_MIN_PACKET_SIZE		60
#define	ETH_MAX_PACKET_SIZE		1500
#define	ETH_MAX_JUMBO_PACKET_SIZE	9600
#define	MDIO_ACCESS_TIMEOUT		1000
#define	BMAC_CONTROL_RX_ENABLE		2

struct bxe_image_header {
	uint32_t	magic;
#define	FILE_MAGIC			0x669955aa
	uint32_t	version;
#define	FORMAT_VERSION_2		0x2
	uint32_t	type;
#define	IMAGE_HDR_TYPE_BCM8073		0x33373038
#define	IMAGE_HDR_TYPE_BCM8726		0x36323738
#define	IMAGE_HDR_TYPE_BCM8727		0x37323738
#define	IMAGE_HDR_TYPE_BCM8481		0x31383438
#define	IMAGE_HDR_TYPE_SFX7101		0x68706673
	uint32_t	image_info;
	uint32_t	byte_cnt;
};

/*
 * Shortcut definitions
 */

#define	NIG_LATCH_BC_ENABLE_MI_INT		0

#define	NIG_STATUS_EMAC0_MI_INT			\
	NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_EMAC0_MISC_MI_INT
#define	NIG_STATUS_XGXS0_LINK10G		\
	NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK10G
#define	NIG_STATUS_XGXS0_LINK_STATUS		\
	NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK_STATUS
#define	NIG_STATUS_XGXS0_LINK_STATUS_SIZE	\
	NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK_STATUS_SIZE
#define	NIG_STATUS_SERDES0_LINK_STATUS		\
	NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_SERDES0_LINK_STATUS
#define	NIG_MASK_MI_INT				\
	NIG_MASK_INTERRUPT_PORT0_REG_MASK_EMAC0_MISC_MI_INT
#define	NIG_MASK_XGXS0_LINK10G			\
	NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK10G
#define	NIG_MASK_XGXS0_LINK_STATUS		\
	NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK_STATUS
#define	NIG_MASK_SERDES0_LINK_STATUS		\
	NIG_MASK_INTERRUPT_PORT0_REG_MASK_SERDES0_LINK_STATUS

#define	MDIO_AN_CL73_OR_37_COMPLETE					\
	(MDIO_GP_STATUS_TOP_AN_STATUS1_CL73_AUTONEG_COMPLETE |		\
	MDIO_GP_STATUS_TOP_AN_STATUS1_CL37_AUTONEG_COMPLETE)

#define	XGXS_RESET_BITS							\
	(MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_RSTB_HW |	\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_IDDQ |		\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_PWRDWN |		\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_PWRDWN_SD |	\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_TXD_FIFO_RSTB)

#define	SERDES_RESET_BITS						\
	(MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_RSTB_HW |	\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_IDDQ |		\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_PWRDWN |	\
	MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_PWRDWN_SD)

#define	AUTONEG_CL37		SHARED_HW_CFG_AN_ENABLE_CL37
#define	AUTONEG_CL73		SHARED_HW_CFG_AN_ENABLE_CL73
#define	AUTONEG_BAM 		SHARED_HW_CFG_AN_ENABLE_BAM
#define	AUTONEG_PARALLEL	SHARED_HW_CFG_AN_ENABLE_PARALLEL_DETECTION
#define	AUTONEG_SGMII_FIBER_AUTODET	\
	SHARED_HW_CFG_AN_EN_SGMII_FIBER_AUTO_DETECT
#define	AUTONEG_REMOTE_PHY  	SHARED_HW_CFG_AN_ENABLE_REMOTE_PHY

#define	GP_STATUS_PAUSE_RSOLUTION_TXSIDE	\
	MDIO_GP_STATUS_TOP_AN_STATUS1_PAUSE_RSOLUTION_TXSIDE
#define	GP_STATUS_PAUSE_RSOLUTION_RXSIDE	\
	MDIO_GP_STATUS_TOP_AN_STATUS1_PAUSE_RSOLUTION_RXSIDE
#define	GP_STATUS_SPEED_MASK		\
	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_MASK
#define	GP_STATUS_10M		MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10M
#define	GP_STATUS_100M		MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_100M
#define	GP_STATUS_1G		MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_1G
#define	GP_STATUS_2_5G		MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_2_5G
#define	GP_STATUS_5G		MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_5G
#define	GP_STATUS_6G		MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_6G
#define	GP_STATUS_10G_HIG	\
	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_HIG
#define	GP_STATUS_10G_CX4	\
	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_CX4
#define	GP_STATUS_12G_HIG	\
	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12G_HIG
#define	GP_STATUS_12_5G		MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12_5G
#define	GP_STATUS_13G		MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_13G
#define	GP_STATUS_15G		MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_15G
#define	GP_STATUS_16G		MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_16G
#define	GP_STATUS_1G_KX		MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_1G_KX
#define	GP_STATUS_10G_KX4	\
	MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_KX4

#define	LINK_10THD  		LINK_STATUS_SPEED_AND_DUPLEX_10THD
#define	LINK_10TFD  		LINK_STATUS_SPEED_AND_DUPLEX_10TFD
#define	LINK_100TXHD		LINK_STATUS_SPEED_AND_DUPLEX_100TXHD
#define	LINK_100T4  		LINK_STATUS_SPEED_AND_DUPLEX_100T4
#define	LINK_100TXFD		LINK_STATUS_SPEED_AND_DUPLEX_100TXFD
#define	LINK_1000THD		LINK_STATUS_SPEED_AND_DUPLEX_1000THD
#define	LINK_1000TFD		LINK_STATUS_SPEED_AND_DUPLEX_1000TFD
#define	LINK_1000XFD		LINK_STATUS_SPEED_AND_DUPLEX_1000XFD
#define	LINK_2500THD		LINK_STATUS_SPEED_AND_DUPLEX_2500THD
#define	LINK_2500TFD		LINK_STATUS_SPEED_AND_DUPLEX_2500TFD
#define	LINK_2500XFD		LINK_STATUS_SPEED_AND_DUPLEX_2500XFD
#define	LINK_10GTFD 		LINK_STATUS_SPEED_AND_DUPLEX_10GTFD
#define	LINK_10GXFD 		LINK_STATUS_SPEED_AND_DUPLEX_10GXFD
#define	LINK_12GTFD 		LINK_STATUS_SPEED_AND_DUPLEX_12GTFD
#define	LINK_12GXFD 		LINK_STATUS_SPEED_AND_DUPLEX_12GXFD
#define	LINK_12_5GTFD		LINK_STATUS_SPEED_AND_DUPLEX_12_5GTFD
#define	LINK_12_5GXFD		LINK_STATUS_SPEED_AND_DUPLEX_12_5GXFD
#define	LINK_13GTFD 		LINK_STATUS_SPEED_AND_DUPLEX_13GTFD
#define	LINK_13GXFD 		LINK_STATUS_SPEED_AND_DUPLEX_13GXFD
#define	LINK_15GTFD 		LINK_STATUS_SPEED_AND_DUPLEX_15GTFD
#define	LINK_15GXFD 		LINK_STATUS_SPEED_AND_DUPLEX_15GXFD
#define	LINK_16GTFD 		LINK_STATUS_SPEED_AND_DUPLEX_16GTFD
#define	LINK_16GXFD 		LINK_STATUS_SPEED_AND_DUPLEX_16GXFD

#define	PHY_XGXS_FLAG				0x1
#define	PHY_SGMII_FLAG				0x2
#define	PHY_SERDES_FLAG				0x4

#define	SFP_EEPROM_CON_TYPE_ADDR		0x2
#define	SFP_EEPROM_CON_TYPE_VAL_LC		0x7
#define	SFP_EEPROM_CON_TYPE_VAL_COPPER		0x21

#define	SFP_EEPROM_COMP_CODE_ADDR		0x3
#define	SFP_EEPROM_COMP_CODE_SR_MASK		(1 << 4)
#define	SFP_EEPROM_COMP_CODE_LR_MASK		(1 << 5)
#define	SFP_EEPROM_COMP_CODE_LRM_MASK		(1 << 6)

#define	SFP_EEPROM_FC_TX_TECH_ADDR		0x8
#define	SFP_EEPROM_FC_TX_TECH_BITMASK_COPPER_PASSIVE	0x4
#define	SFP_EEPROM_FC_TX_TECH_BITMASK_COPPER_ACTIVE	0x8

#define	SFP_EEPROM_OPTIONS_ADDR 		0x40
#define	SFP_EEPROM_OPTIONS_LINEAR_RX_OUT_MASK	0x1
#define	SFP_EEPROM_OPTIONS_SIZE 		2

#define	EDC_MODE_LINEAR				0x0022
#define	EDC_MODE_LIMITING			0x0044
#define	EDC_MODE_PASSIVE_DAC			0x0055

/*
 * 8073 Download definitions.
 */
/* spi Parameters.*/
#define	SPI_CTRL_1_L	0xC000
#define	SPI_CTRL_1_H	0xC002
#define	SPI_CTRL_2_L	0xC400
#define	SPI_CTRL_2_H	0xC402
#define	SPI_TXFIFO	0xD000
#define	SPI_RXFIFO	0xD400

/* Input Command Messages.*/
/*
 * Write CPU/SPI Control Regs, followed by Count And
 * CPU/SPI Controller Reg add/data pairs.
 */
#define	WR_CPU_CTRL_REGS	0x11
/*
 * Read CPU/SPI Control Regs, followed by Count and
 * CPU/SPI Controller Register Add.
 */
#define	RD_CPU_CTRL_REGS	0xEE
/*
 * Write CPU/SPI Control Regs Continously, followed by
 * Count and CPU/SPI Controller Reg addr and data's.
 */
#define	WR_CPU_CTRL_FIFO	0x66
/* Output Command Messages.*/
#define	DONE			0x4321

/* SPI Controller Commands (known As messages).*/
#define	MSGTYPE_HWR		0x40
#define	MSGTYPE_HRD		0x80
#define	WRSR_OPCODE		0x01
#define	WR_OPCODE		0x02
#define	RD_OPCODE		0x03
#define	WRDI_OPCODE		0x04
#define	RDSR_OPCODE		0x05
#define	WREN_OPCODE		0x06
#define	WR_BLOCK_SIZE		0x40 /* Maximum 64 Bytes Writes.*/

#define	BUF_SIZE_BCM		0x4000 /* Code Size is 16k bytes.*/
#define	UPGRADE_TIMEOUT_BCM	1000

/*
 * INTERFACE
 */
#define	CL45_WR_OVER_CL22(_sc, _port, _phy_addr, _bank, _addr, _val)	\
	bxe_cl45_write(_sc, _port, 0, _phy_addr, DEFAULT_PHY_DEV_ADDR,	\
	(_bank + (_addr & 0xf)), _val)

#define	CL45_RD_OVER_CL22(_sc, _port, _phy_addr, _bank, _addr, _val)	\
	bxe_cl45_read(_sc, _port, 0, _phy_addr, DEFAULT_PHY_DEV_ADDR,	\
	(_bank + (_addr & 0xf)), _val)

static void
bxe_set_serdes_access(struct link_params *params)
{
	struct bxe_softc *sc;
	uint32_t emac_base;

	sc = params->sc;
	emac_base = (params->port) ? GRCBASE_EMAC1 : GRCBASE_EMAC0;

	/* Set Clause 22 */
	REG_WR(sc, NIG_REG_SERDES0_CTRL_MD_ST + params->port * 0x10, 1);
	REG_WR(sc, emac_base + EMAC_REG_EMAC_MDIO_COMM, 0x245f8000);
	DELAY(500);
	REG_WR(sc, emac_base + EMAC_REG_EMAC_MDIO_COMM, 0x245d000f);
	DELAY(500);
	 /* Set Clause 45 */
	REG_WR(sc, NIG_REG_SERDES0_CTRL_MD_ST + params->port * 0x10, 0);
}

static void
bxe_set_phy_mdio(struct link_params *params, uint8_t phy_flags)
{
	struct bxe_softc *sc;

	sc = params->sc;
	if (phy_flags & PHY_XGXS_FLAG) {
		REG_WR(sc, NIG_REG_XGXS0_CTRL_MD_ST + params->port * 0x18, 0);
		REG_WR(sc, NIG_REG_XGXS0_CTRL_MD_DEVAD + params->port * 0x18,
		    DEFAULT_PHY_DEV_ADDR);
	} else {
		bxe_set_serdes_access(params);

		REG_WR(sc, NIG_REG_SERDES0_CTRL_MD_DEVAD + params->port * 0x10,
		    DEFAULT_PHY_DEV_ADDR);
	}
}

static uint32_t
bxe_bits_en(struct bxe_softc *sc, uint32_t reg, uint32_t bits)
{
	uint32_t val;

	val = REG_RD(sc, reg);
	val |= bits;
	REG_WR(sc, reg, val);
	return (val);
}

static uint32_t
bxe_bits_dis(struct bxe_softc *sc, uint32_t reg, uint32_t bits)
{
	uint32_t val;

	val = REG_RD(sc, reg);
	val &= ~bits;
	REG_WR(sc, reg, val);
	return (val);
}

static void
bxe_emac_init(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint32_t emac_base, val;
	uint16_t timeout;
	uint8_t port;

	/* reset and unreset the emac core */
	sc = params->sc;
	port = params->port;
	emac_base = port ? GRCBASE_EMAC1 : GRCBASE_EMAC0;

	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_CLEAR,
	    (MISC_REGISTERS_RESET_REG_2_RST_EMAC0_HARD_CORE << port));
	DELAY(5);
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_SET,
	    (MISC_REGISTERS_RESET_REG_2_RST_EMAC0_HARD_CORE << port));

	/* Init emac - use read-modify-write. */
	/* self clear reset */
	val = REG_RD(sc, emac_base + EMAC_REG_EMAC_MODE);
	EMAC_WR(sc, EMAC_REG_EMAC_MODE, (val | EMAC_MODE_RESET));

	timeout = 200;
	do {
		val = REG_RD(sc, emac_base + EMAC_REG_EMAC_MODE);
		DBPRINT(sc, BXE_VERBOSE_PHY, "EMAC reset reg is %u\n", val);
		if (!timeout) {
			DBPRINT(sc, BXE_VERBOSE_PHY, "EMAC timeout!\n");
			return;
		}
		timeout--;
	} while (val & EMAC_MODE_RESET);

	/* Set mac address. */
	val = ((params->mac_addr[0] << 8) | params->mac_addr[1]);
	EMAC_WR(sc, EMAC_REG_EMAC_MAC_MATCH, val);

	val = ((params->mac_addr[2] << 24) | (params->mac_addr[3] << 16) |
	    (params->mac_addr[4] << 8) | params->mac_addr[5]);
	EMAC_WR(sc, EMAC_REG_EMAC_MAC_MATCH + 4, val);
}

static uint8_t
bxe_emac_enable(struct link_params *params, struct link_vars *vars, uint8_t lb)
{
	struct bxe_softc *sc;
	uint32_t emac_base, ser_lane, val;
	uint8_t port;

	sc = params->sc;
	DBPRINT(sc, BXE_VERBOSE_PHY, "enabling EMAC\n");

	port = params->port;
	emac_base = port ? GRCBASE_EMAC1 : GRCBASE_EMAC0;

	/* enable emac and not bmac */
	REG_WR(sc, NIG_REG_EGRESS_EMAC0_PORT + port * 4, 1);

	if (vars->phy_flags & PHY_XGXS_FLAG) {
		ser_lane = ((params->lane_config &
		    PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MASK) >>
		    PORT_HW_CFG_LANE_SWAP_CFG_MASTER_SHIFT);

		DBPRINT(sc, BXE_VERBOSE_PHY, "XGXS\n");
		/* select the master lanes (out of 0-3) */
		REG_WR(sc, NIG_REG_XGXS_LANE_SEL_P0 + port * 4, ser_lane);
		/* select XGXS */
		REG_WR(sc, NIG_REG_XGXS_SERDES0_MODE_SEL + port * 4, 1);

	} else { /* SerDes */
		DBPRINT(sc, BXE_VERBOSE_PHY, "SerDes\n");
		/* select SerDes */
		REG_WR(sc, NIG_REG_XGXS_SERDES0_MODE_SEL + port * 4, 0);
	}

	bxe_bits_en(sc, emac_base + EMAC_REG_EMAC_RX_MODE, EMAC_RX_MODE_RESET);
	bxe_bits_en(sc, emac_base + EMAC_REG_EMAC_TX_MODE, EMAC_TX_MODE_RESET);

	/* pause enable/disable */
	bxe_bits_dis(sc, emac_base + EMAC_REG_EMAC_RX_MODE,
	    EMAC_RX_MODE_FLOW_EN);
	if (vars->flow_ctrl & FLOW_CTRL_RX)
		bxe_bits_en(sc, emac_base + EMAC_REG_EMAC_RX_MODE,
		    EMAC_RX_MODE_FLOW_EN);

	bxe_bits_dis(sc, emac_base + EMAC_REG_EMAC_TX_MODE,
	    (EMAC_TX_MODE_EXT_PAUSE_EN | EMAC_TX_MODE_FLOW_EN));
	if (vars->flow_ctrl & FLOW_CTRL_TX)
		bxe_bits_en(sc, emac_base + EMAC_REG_EMAC_TX_MODE,
		    (EMAC_TX_MODE_EXT_PAUSE_EN | EMAC_TX_MODE_FLOW_EN));

	/* KEEP_VLAN_TAG, promiscuous */
	val = REG_RD(sc, emac_base + EMAC_REG_EMAC_RX_MODE);
	val |= EMAC_RX_MODE_KEEP_VLAN_TAG | EMAC_RX_MODE_PROMISCUOUS;

	EMAC_WR(sc, EMAC_REG_EMAC_RX_MODE, val);

	/* Set Loopback */
	val = REG_RD(sc, emac_base + EMAC_REG_EMAC_MODE);
	if (lb)
		val |= 0x810;
	else
		val &= ~0x810;
	EMAC_WR(sc, EMAC_REG_EMAC_MODE, val);

	/* enable emac */
	REG_WR(sc, NIG_REG_NIG_EMAC0_EN + port * 4, 1);

	/* Enable emac for jumbo packets. */
	EMAC_WR(sc, EMAC_REG_EMAC_RX_MTU_SIZE, (EMAC_RX_MTU_SIZE_JUMBO_ENA |
	    (ETH_MAX_JUMBO_PACKET_SIZE + ETH_OVREHEAD)));

	/* strip CRC */
	REG_WR(sc, NIG_REG_NIG_INGRESS_EMAC0_NO_CRC + port * 4, 0x1);

	/* Disable the NIG in/out to the bmac. */
	REG_WR(sc, NIG_REG_BMAC0_IN_EN + port * 4, 0x0);
	REG_WR(sc, NIG_REG_BMAC0_PAUSE_OUT_EN + port * 4, 0x0);
	REG_WR(sc, NIG_REG_BMAC0_OUT_EN + port * 4, 0x0);

	/* Enable the NIG in/out to the emac. */
	REG_WR(sc, NIG_REG_EMAC0_IN_EN + port * 4, 0x1);
	val = 0;
	if (vars->flow_ctrl & FLOW_CTRL_TX)
		val = 1;

	REG_WR(sc, NIG_REG_EMAC0_PAUSE_OUT_EN + port * 4, val);
	REG_WR(sc, NIG_REG_EGRESS_EMAC0_OUT_EN + port * 4, 0x1);

	REG_WR(sc, NIG_REG_BMAC0_REGS_OUT_EN + port * 4, 0x0);

	vars->mac_type = MAC_TYPE_EMAC;
	return (0);
}

static uint8_t
bxe_bmac1_enable(struct link_params *params, struct link_vars *vars,
    uint8_t is_lb)
{
	struct bxe_softc *sc;
	uint32_t bmac_addr, wb_data[2], val;
	uint8_t port;

	sc = params->sc;
	DBPRINT(sc, BXE_VERBOSE_PHY, "Enabling BigMAC1\n");

	port = params->port;
	bmac_addr = port ? NIG_REG_INGRESS_BMAC1_MEM :
	    NIG_REG_INGRESS_BMAC0_MEM;

	/* XGXS control */
	wb_data[0] = 0x3c;
	wb_data[1] = 0;
	REG_WR_DMAE(sc, bmac_addr + BIGMAC_REGISTER_BMAC_XGXS_CONTROL, wb_data,
	    2);

	/* tx MAC SA */
	wb_data[0] = ((params->mac_addr[2] << 24) |
	    (params->mac_addr[3] << 16) | (params->mac_addr[4] << 8) |
	    params->mac_addr[5]);
	wb_data[1] = ((params->mac_addr[0] << 8) | params->mac_addr[1]);
	REG_WR_DMAE(sc, bmac_addr + BIGMAC_REGISTER_TX_SOURCE_ADDR, wb_data, 2);

	/* tx control */
	val = 0xc0;
	if (vars->flow_ctrl & FLOW_CTRL_TX)
		val |= 0x800000;
	wb_data[0] = val;
	wb_data[1] = 0;
	REG_WR_DMAE(sc, bmac_addr + BIGMAC_REGISTER_TX_CONTROL, wb_data, 2);

	/* mac control */
	val = 0x3;
	if (is_lb) {
		val |= 0x4;
		DBPRINT(sc, BXE_VERBOSE_PHY, "enable bmac loopback\n");
	}
	wb_data[0] = val;
	wb_data[1] = 0;
	REG_WR_DMAE(sc, bmac_addr + BIGMAC_REGISTER_BMAC_CONTROL, wb_data, 2);

	/* set rx mtu */
	wb_data[0] = ETH_MAX_JUMBO_PACKET_SIZE + ETH_OVREHEAD;
	wb_data[1] = 0;
	REG_WR_DMAE(sc, bmac_addr + BIGMAC_REGISTER_RX_MAX_SIZE, wb_data, 2);

	/* rx control set to don't strip crc */
	val = 0x14;
	if (vars->flow_ctrl & FLOW_CTRL_RX)
		val |= 0x20;

	wb_data[0] = val;
	wb_data[1] = 0;
	REG_WR_DMAE(sc, bmac_addr + BIGMAC_REGISTER_RX_CONTROL, wb_data, 2);

	/* set tx mtu */
	wb_data[0] = ETH_MAX_JUMBO_PACKET_SIZE + ETH_OVREHEAD;
	wb_data[1] = 0;
	REG_WR_DMAE(sc, bmac_addr + BIGMAC_REGISTER_TX_MAX_SIZE,
			wb_data, 2);

	/* set cnt max size */
	wb_data[0] = ETH_MAX_JUMBO_PACKET_SIZE + ETH_OVREHEAD;
	wb_data[1] = 0;
	REG_WR_DMAE(sc, bmac_addr + BIGMAC_REGISTER_CNT_MAX_SIZE, wb_data, 2);

	/* configure safc */
	wb_data[0] = 0x1000200;
	wb_data[1] = 0;
	REG_WR_DMAE(sc, bmac_addr + BIGMAC_REGISTER_RX_LLFC_MSG_FLDS, wb_data,
	    2);

	return (0);
}

static uint8_t
bxe_bmac_enable(struct link_params *params, struct link_vars *vars,
    uint8_t is_lb)
{
	struct bxe_softc *sc;
	uint32_t val;
	uint8_t rc, port;

	sc = params->sc;
	port = params->port;

	/* reset and unreset the BigMac */
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_CLEAR,
	    (MISC_REGISTERS_RESET_REG_2_RST_BMAC0 << port));
	msleep(1);

	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_SET,
	    (MISC_REGISTERS_RESET_REG_2_RST_BMAC0 << port));

	/* enable access for bmac registers */
	REG_WR(sc, NIG_REG_BMAC0_REGS_OUT_EN + port * 4, 0x1);

	rc = bxe_bmac1_enable(params, vars, is_lb);

	REG_WR(sc, NIG_REG_XGXS_SERDES0_MODE_SEL + port * 4, 0x1);
	REG_WR(sc, NIG_REG_XGXS_LANE_SEL_P0 + port * 4, 0x0);
	REG_WR(sc, NIG_REG_EGRESS_EMAC0_PORT + port * 4, 0x0);
	val = 0;
	if (vars->flow_ctrl & FLOW_CTRL_TX)
		val = 1;
	REG_WR(sc, NIG_REG_BMAC0_PAUSE_OUT_EN + port * 4, val);
	REG_WR(sc, NIG_REG_EGRESS_EMAC0_OUT_EN + port * 4, 0x0);
	REG_WR(sc, NIG_REG_EMAC0_IN_EN + port * 4, 0x0);
	REG_WR(sc, NIG_REG_EMAC0_PAUSE_OUT_EN + port * 4, 0x0);
	REG_WR(sc, NIG_REG_BMAC0_IN_EN + port * 4, 0x1);
	REG_WR(sc, NIG_REG_BMAC0_OUT_EN + port * 4, 0x1);

	vars->mac_type = MAC_TYPE_BMAC;
	return (rc);
}

static void
bxe_phy_deassert(struct link_params *params, uint8_t phy_flags)
{
	struct bxe_softc *sc;
	uint32_t val;

	sc = params->sc;
	if (phy_flags & PHY_XGXS_FLAG) {
		DBPRINT(sc, BXE_VERBOSE_PHY, "bxe_phy_deassert:XGXS\n");
		val = XGXS_RESET_BITS;

	} else { /* SerDes */
		DBPRINT(sc, BXE_VERBOSE_PHY, "bxe_phy_deassert:SerDes\n");
		val = SERDES_RESET_BITS;
	}

	val = val << (params->port * 16);

	/* reset and unreset the SerDes/XGXS */
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_3_CLEAR, val);
	DELAY(500);
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_3_SET, val);
	bxe_set_phy_mdio(params, phy_flags);
}

void
bxe_link_status_update(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint8_t link_10g, port;

	sc = params->sc;
	port = params->port;

	if (params->switch_cfg == SWITCH_CFG_1G)
		vars->phy_flags = PHY_SERDES_FLAG;
	else
		vars->phy_flags = PHY_XGXS_FLAG;
	vars->link_status = REG_RD(sc, params->shmem_base +
	    offsetof(struct shmem_region, port_mb[port].link_status));

	vars->link_up = (vars->link_status & LINK_STATUS_LINK_UP);

	if (vars->link_up) {
		DBPRINT(sc, BXE_VERBOSE_PHY, "phy link up\n");

		vars->phy_link_up = 1;
		vars->duplex = DUPLEX_FULL;
		switch (vars->link_status & LINK_STATUS_SPEED_AND_DUPLEX_MASK) {
		case LINK_10THD:
			vars->duplex = DUPLEX_HALF;
			/* FALLTHROUGH */
		case LINK_10TFD:
			vars->line_speed = SPEED_10;
			break;
		case LINK_100TXHD:
			vars->duplex = DUPLEX_HALF;
			/* FALLTHROUGH */
		case LINK_100T4:
		case LINK_100TXFD:
			vars->line_speed = SPEED_100;
			break;
		case LINK_1000THD:
			vars->duplex = DUPLEX_HALF;
			/* FALLTHROUGH */
		case LINK_1000TFD:
			vars->line_speed = SPEED_1000;
			break;
		case LINK_2500THD:
			vars->duplex = DUPLEX_HALF;
			/* FALLTHROUGH */
		case LINK_2500TFD:
			vars->line_speed = SPEED_2500;
			break;
		case LINK_10GTFD:
			vars->line_speed = SPEED_10000;
			break;
		case LINK_12GTFD:
			vars->line_speed = SPEED_12000;
			break;
		case LINK_12_5GTFD:
			vars->line_speed = SPEED_12500;
			break;
		case LINK_13GTFD:
			vars->line_speed = SPEED_13000;
			break;
		case LINK_15GTFD:
			vars->line_speed = SPEED_15000;
			break;
		case LINK_16GTFD:
			vars->line_speed = SPEED_16000;
			break;
		default:
			break;
		}

		if (vars->link_status & LINK_STATUS_TX_FLOW_CONTROL_ENABLED)
			vars->flow_ctrl |= FLOW_CTRL_TX;
		else
			vars->flow_ctrl &= ~FLOW_CTRL_TX;

		if (vars->link_status & LINK_STATUS_RX_FLOW_CONTROL_ENABLED)
			vars->flow_ctrl |= FLOW_CTRL_RX;
		else
			vars->flow_ctrl &= ~FLOW_CTRL_RX;

		if (vars->phy_flags & PHY_XGXS_FLAG) {
			if (vars->line_speed &&
			    ((vars->line_speed == SPEED_10) ||
			    (vars->line_speed == SPEED_100))) {
				vars->phy_flags |= PHY_SGMII_FLAG;
			} else
				vars->phy_flags &= ~PHY_SGMII_FLAG;
		}

		/* Anything 10 and over uses the bmac. */
		link_10g = ((vars->line_speed == SPEED_10000) ||
		    (vars->line_speed == SPEED_12000) ||
		    (vars->line_speed == SPEED_12500) ||
		    (vars->line_speed == SPEED_13000) ||
		    (vars->line_speed == SPEED_15000) ||
		    (vars->line_speed == SPEED_16000));
		if (link_10g)
			vars->mac_type = MAC_TYPE_BMAC;
		else
			vars->mac_type = MAC_TYPE_EMAC;

	} else {
		/* link down */
		DBPRINT(sc, BXE_VERBOSE_PHY, "phy link down\n");

		vars->phy_link_up = 0;

		vars->line_speed = 0;
		vars->duplex = DUPLEX_FULL;
		vars->flow_ctrl = FLOW_CTRL_NONE;

		/* Indicate no mac active. */
		vars->mac_type = MAC_TYPE_NONE;
	}

	DBPRINT(sc, BXE_VERBOSE_PHY, "link_status 0x%x phy_link_up %x\n",
	    vars->link_status, vars->phy_link_up);
	DBPRINT(sc, BXE_VERBOSE_PHY, "line_speed %x duplex %x flow_ctrl 0x%x\n",
	    vars->line_speed, vars->duplex, vars->flow_ctrl);
}

static void
bxe_update_mng(struct link_params *params, uint32_t link_status)
{
	struct bxe_softc *sc;

	sc = params->sc;
	REG_WR(sc, params->shmem_base + offsetof(struct shmem_region,
	    port_mb[params->port].link_status), link_status);
}

static void
bxe_bmac_rx_disable(struct bxe_softc *sc, uint32_t chip_id, uint8_t port)
{
	uint32_t bmac_addr, wb_data[2];
	uint32_t nig_bmac_enable;

	bmac_addr = port ? NIG_REG_INGRESS_BMAC1_MEM :
	    NIG_REG_INGRESS_BMAC0_MEM;
	nig_bmac_enable = REG_RD(sc, NIG_REG_BMAC0_REGS_OUT_EN + port * 4);

	/* Only if the bmac is out of reset */
	if (REG_RD(sc, MISC_REG_RESET_REG_2) &
	    (MISC_REGISTERS_RESET_REG_2_RST_BMAC0 << port) && nig_bmac_enable) {
		REG_RD_DMAE(sc, bmac_addr + BIGMAC_REGISTER_BMAC_CONTROL,
		    wb_data, 2);
		wb_data[0] &= ~BMAC_CONTROL_RX_ENABLE;
		REG_WR_DMAE(sc, bmac_addr + BIGMAC_REGISTER_BMAC_CONTROL,
		    wb_data, 2);

		msleep(1);
	}
}

static uint8_t
bxe_pbf_update(struct link_params *params, uint32_t flow_ctrl,
    uint32_t line_speed)
{
	struct bxe_softc *sc;
	uint32_t count, crd, init_crd;
	uint32_t thresh;
	uint8_t port;

	sc = params->sc;
	port = params->port;

	/* Disable port. */
	REG_WR(sc, PBF_REG_DISABLE_NEW_TASK_PROC_P0 + port * 4, 0x1);

	/* Wait for init credit. */
	init_crd = REG_RD(sc, PBF_REG_P0_INIT_CRD + port * 4);
	crd = REG_RD(sc, PBF_REG_P0_CREDIT + port * 8);
	DBPRINT(sc, BXE_VERBOSE_PHY, "init_crd 0x%x crd 0x%x\n", init_crd, crd);

	count = 1000;
	while ((init_crd != crd) && count) {
		msleep(5);

		crd = REG_RD(sc, PBF_REG_P0_CREDIT + port * 8);
		count--;
	}
	crd = REG_RD(sc, PBF_REG_P0_CREDIT + port*8);
	if (init_crd != crd) {
		DBPRINT(sc, BXE_VERBOSE_PHY, "BUG! init_crd 0x%x != crd 0x%x\n",
		    init_crd, crd);
		return (-EINVAL);
	}

	if (flow_ctrl & FLOW_CTRL_RX || line_speed == SPEED_10 ||
	    line_speed == SPEED_100 || line_speed == SPEED_1000 ||
	    line_speed == SPEED_2500) {
		REG_WR(sc, PBF_REG_P0_PAUSE_ENABLE + port * 4, 1);
		/* Update threshold. */
		REG_WR(sc, PBF_REG_P0_ARB_THRSH + port * 4, 0);
		/* Update init credit. */
		init_crd = 778; 	/* (800-18-4) */
	} else {
		thresh = (ETH_MAX_JUMBO_PACKET_SIZE + ETH_OVREHEAD) / 16;
		REG_WR(sc, PBF_REG_P0_PAUSE_ENABLE + port * 4, 0);
		/* Update threshold. */
		REG_WR(sc, PBF_REG_P0_ARB_THRSH + port * 4, thresh);
		/* Update init credit. */
		switch (line_speed) {
		case SPEED_10000:
			init_crd = thresh + 553 - 22;
			break;
		case SPEED_12000:
			init_crd = thresh + 664 - 22;
			break;
		case SPEED_13000:
			init_crd = thresh + 742 - 22;
			break;
		case SPEED_16000:
			init_crd = thresh + 778 - 22;
			break;
		default:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "Invalid line_speed 0x%x\n", line_speed);
			return (-EINVAL);
		}
	}
	REG_WR(sc, PBF_REG_P0_INIT_CRD + port * 4, init_crd);
	DBPRINT(sc, BXE_VERBOSE_PHY, "PBF updated to speed %d credit %d\n",
	    line_speed, init_crd);

	/* Probe the credit changes. */
	REG_WR(sc, PBF_REG_INIT_P0 + port * 4, 0x1);
	msleep(5);
	REG_WR(sc, PBF_REG_INIT_P0 + port * 4, 0x0);

	/* Enable port. */
	REG_WR(sc, PBF_REG_DISABLE_NEW_TASK_PROC_P0 + port * 4, 0x0);
	return (0);
}

static uint32_t
bxe_get_emac_base(struct bxe_softc *sc, uint32_t ext_phy_type, uint8_t port)
{
	uint32_t emac_base;

	switch (ext_phy_type) {
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
		/* All MDC/MDIO is directed through single EMAC. */
		if (REG_RD(sc, NIG_REG_PORT_SWAP))
			emac_base = GRCBASE_EMAC0;
		else
			emac_base = GRCBASE_EMAC1;
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
		emac_base = (port) ? GRCBASE_EMAC0 : GRCBASE_EMAC1;
		break;
	default:
		emac_base = (port) ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
		break;
	}
	return (emac_base);
}

uint8_t
bxe_cl45_write(struct bxe_softc *sc, uint8_t port, uint32_t ext_phy_type,
    uint8_t phy_addr, uint8_t devad, uint16_t reg, uint16_t val)
{
	uint32_t mdio_ctrl, saved_mode, tmp;
	uint8_t i, rc;

	rc = 0;
	mdio_ctrl = bxe_get_emac_base(sc, ext_phy_type, port);

	/*
	 * Set clause 45 mode, slow down the MDIO clock to 2.5MHz
	 * (a value of 49==0x31) and make sure that the AUTO poll is off.
	 */

	saved_mode = REG_RD(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE);
	tmp = saved_mode & ~(EMAC_MDIO_MODE_AUTO_POLL |
	    EMAC_MDIO_MODE_CLOCK_CNT);
	tmp |= (EMAC_MDIO_MODE_CLAUSE_45 |
	    (49 << EMAC_MDIO_MODE_CLOCK_CNT_BITSHIFT));
	REG_WR(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE, tmp);
	REG_RD(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE);
	DELAY(40);

	/* address */

	tmp = ((phy_addr << 21) | (devad << 16) | reg |
	    EMAC_MDIO_COMM_COMMAND_ADDRESS | EMAC_MDIO_COMM_START_BUSY);
	REG_WR(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM, tmp);

	for (i = 0; i < 50; i++) {
		DELAY(10);

		tmp = REG_RD(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM);
		if (!(tmp & EMAC_MDIO_COMM_START_BUSY)) {
			DELAY(5);
			break;
		}
	}
	if (tmp & EMAC_MDIO_COMM_START_BUSY) {
		DBPRINT(sc, BXE_VERBOSE_PHY, "write phy register failed\n");
		rc = -EINVAL;
	} else {
		/* data */
		tmp = ((phy_addr << 21) | (devad << 16) | val |
		    EMAC_MDIO_COMM_COMMAND_WRITE_45 |
		    EMAC_MDIO_COMM_START_BUSY);
		REG_WR(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM, tmp);

		for (i = 0; i < 50; i++) {
			DELAY(10);

			tmp = REG_RD(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM);
			if (!(tmp & EMAC_MDIO_COMM_START_BUSY)) {
				DELAY(5);
				break;
			}
		}
		if (tmp & EMAC_MDIO_COMM_START_BUSY) {
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "write phy register failed\n");
			rc = -EINVAL;
		}
	}

	/* Restore the saved mode. */
	REG_WR(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE, saved_mode);

	return (rc);
}

uint8_t
bxe_cl45_read(struct bxe_softc *sc, uint8_t port, uint32_t ext_phy_type,
    uint8_t phy_addr, uint8_t devad, uint16_t reg, uint16_t *ret_val)
{
	uint32_t mdio_ctrl, saved_mode, val;
	uint16_t i;
	uint8_t rc;

	rc = 0;
	mdio_ctrl = bxe_get_emac_base(sc, ext_phy_type, port);

	/*
	 * set clause 45 mode, slow down the MDIO clock to 2.5MHz
	 * (a value of 49==0x31) and make sure that the AUTO poll is off.
	 */
	saved_mode = REG_RD(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE);
	val = saved_mode & ((EMAC_MDIO_MODE_AUTO_POLL |
	    EMAC_MDIO_MODE_CLOCK_CNT));
	val |= (EMAC_MDIO_MODE_CLAUSE_45 |
	    (49L << EMAC_MDIO_MODE_CLOCK_CNT_BITSHIFT));
	REG_WR(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE, val);
	REG_RD(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE);
	DELAY(40);

	/* address */
	val = ((phy_addr << 21) | (devad << 16) | reg |
	    EMAC_MDIO_COMM_COMMAND_ADDRESS | EMAC_MDIO_COMM_START_BUSY);
	REG_WR(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM, val);

	for (i = 0; i < 50; i++) {
		DELAY(10);

		val = REG_RD(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM);
		if (!(val & EMAC_MDIO_COMM_START_BUSY)) {
			DELAY(5);
			break;
		}
	}
	if (val & EMAC_MDIO_COMM_START_BUSY) {
		DBPRINT(sc, BXE_VERBOSE_PHY, "read phy register failed\n");

		*ret_val = 0;
		rc = -EINVAL;
	} else {
		/* data */
		val = ((phy_addr << 21) | (devad << 16) |
		    EMAC_MDIO_COMM_COMMAND_READ_45 | EMAC_MDIO_COMM_START_BUSY);
		REG_WR(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM, val);

		for (i = 0; i < 50; i++) {
			DELAY(10);

			val = REG_RD(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_COMM);
			if (!(val & EMAC_MDIO_COMM_START_BUSY)) {
				*ret_val = (uint16_t)(val &
				    EMAC_MDIO_COMM_DATA);
				break;
			}
		}
		if (val & EMAC_MDIO_COMM_START_BUSY) {
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "read phy register failed\n");

			*ret_val = 0;
			rc = -EINVAL;
		}
	}

	/* Restore the saved mode. */
	REG_WR(sc, mdio_ctrl + EMAC_REG_EMAC_MDIO_MODE, saved_mode);

	return (rc);
}

static void
bxe_set_aer_mmd(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint32_t ser_lane;
	uint16_t offset;

	sc = params->sc;
	ser_lane = ((params->lane_config &
	    PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MASK) >>
	    PORT_HW_CFG_LANE_SWAP_CFG_MASTER_SHIFT);

	offset = (vars->phy_flags & PHY_XGXS_FLAG) ?
	    (params->phy_addr + ser_lane) : 0;

	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_AER_BLOCK, MDIO_AER_BLOCK_AER_REG, 0x3800 + offset);
}

static void
bxe_set_master_ln(struct link_params *params)
{
	struct bxe_softc *sc;
	uint16_t new_master_ln, ser_lane;

	sc = params->sc;
	ser_lane = ((params->lane_config &
	    PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MASK) >>
	    PORT_HW_CFG_LANE_SWAP_CFG_MASTER_SHIFT);

	/* Set the master_ln for AN. */
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_XGXS_BLOCK2, MDIO_XGXS_BLOCK2_TEST_MODE_LANE,
	    &new_master_ln);

	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_XGXS_BLOCK2 , MDIO_XGXS_BLOCK2_TEST_MODE_LANE,
	    (new_master_ln | ser_lane));
}

static uint8_t
bxe_reset_unicore(struct link_params *params)
{
	struct bxe_softc *sc;
	uint16_t mii_control;
	uint16_t i;

	sc = params->sc;
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_MII_CONTROL,
	    &mii_control);

	/* Reset the unicore. */
	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_MII_CONTROL,
	    (mii_control | MDIO_COMBO_IEEO_MII_CONTROL_RESET));
	if (params->switch_cfg == SWITCH_CFG_1G)
		bxe_set_serdes_access(params);

	/* Wait for the reset to self clear. */
	for (i = 0; i < MDIO_ACCESS_TIMEOUT; i++) {
		DELAY(5);

		/* The reset erased the previous bank value. */
		CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_MII_CONTROL,
		    &mii_control);

		if (!(mii_control & MDIO_COMBO_IEEO_MII_CONTROL_RESET)) {
			DELAY(5);
			return (0);
		}
	}

	DBPRINT(sc, BXE_VERBOSE_PHY, "BUG! XGXS is still in reset!\n");
	return (-EINVAL);
}

static void
bxe_set_swap_lanes(struct link_params *params)
{
	struct bxe_softc *sc;
	uint16_t ser_lane, rx_lane_swap, tx_lane_swap;

	sc = params->sc;
	/*
	 * Each two bits represents a lane number:
	 *  No swap is 0123 => 0x1b no need to enable the swap.
	 */

	ser_lane = ((params->lane_config &
	    PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MASK) >>
	    PORT_HW_CFG_LANE_SWAP_CFG_MASTER_SHIFT);
	rx_lane_swap = ((params->lane_config &
	    PORT_HW_CFG_LANE_SWAP_CFG_RX_MASK) >>
	    PORT_HW_CFG_LANE_SWAP_CFG_RX_SHIFT);
	tx_lane_swap = ((params->lane_config &
	    PORT_HW_CFG_LANE_SWAP_CFG_TX_MASK) >>
	    PORT_HW_CFG_LANE_SWAP_CFG_TX_SHIFT);

	if (rx_lane_swap != 0x1b) {
		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_XGXS_BLOCK2, MDIO_XGXS_BLOCK2_RX_LN_SWAP,
		    (rx_lane_swap | MDIO_XGXS_BLOCK2_RX_LN_SWAP_ENABLE |
		    MDIO_XGXS_BLOCK2_RX_LN_SWAP_FORCE_ENABLE));
	} else {
		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_XGXS_BLOCK2, MDIO_XGXS_BLOCK2_RX_LN_SWAP, 0);
	}

	if (tx_lane_swap != 0x1b) {
		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_XGXS_BLOCK2, MDIO_XGXS_BLOCK2_TX_LN_SWAP,
		    (tx_lane_swap | MDIO_XGXS_BLOCK2_TX_LN_SWAP_ENABLE));
	} else {
		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_XGXS_BLOCK2, MDIO_XGXS_BLOCK2_TX_LN_SWAP, 0);
	}
}

static void
bxe_set_parallel_detection(struct link_params *params, uint8_t phy_flags)
{
	struct bxe_softc *sc;
	uint16_t control2;

	sc = params->sc;
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_SERDES_DIGITAL, MDIO_SERDES_DIGITAL_A_1000X_CONTROL2,
	    &control2);
	if (params->speed_cap_mask & PORT_HW_CFG_SPEED_CAPABILITY_D0_1G)
		control2 |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL2_PRL_DT_EN;
	else
		control2 &= ~MDIO_SERDES_DIGITAL_A_1000X_CONTROL2_PRL_DT_EN;

	DBPRINT(sc, BXE_VERBOSE_PHY, "%s(): params->speed_cap_mask = 0x%x, "
	    "control2 = 0x%x\n", __FUNCTION__, params->speed_cap_mask, control2);

	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_SERDES_DIGITAL, MDIO_SERDES_DIGITAL_A_1000X_CONTROL2,
	    control2);

	if ((phy_flags & PHY_XGXS_FLAG) && (params->speed_cap_mask &
	    PORT_HW_CFG_SPEED_CAPABILITY_D0_10G)) {
		DBPRINT(sc, BXE_VERBOSE_PHY, "%s(): XGXS\n", __FUNCTION__);

		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_10G_PARALLEL_DETECT,
		    MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_LINK,
		    MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_LINK_CNT);

		CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_10G_PARALLEL_DETECT,
		    MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_CONTROL, &control2);

		control2 |=
		    MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_CONTROL_PARDET10G_EN;

		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_10G_PARALLEL_DETECT,
		    MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_CONTROL, control2);

		/* Disable parallel detection of HiG. */
		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_XGXS_BLOCK2,
		    MDIO_XGXS_BLOCK2_UNICORE_MODE_10G,
		    MDIO_XGXS_BLOCK2_UNICORE_MODE_10G_CX4_XGXS |
		    MDIO_XGXS_BLOCK2_UNICORE_MODE_10G_HIGIG_XGXS);
	}
}

static void
bxe_set_autoneg(struct link_params *params, struct link_vars *vars,
    uint8_t enable_cl73)
{
	struct bxe_softc *sc;
	uint16_t reg_val;

	sc = params->sc;
	/* CL37 Autoneg */
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_MII_CONTROL, &reg_val);

	/* CL37 Autoneg Enabled */
	if (vars->line_speed == SPEED_AUTO_NEG)
		reg_val |= MDIO_COMBO_IEEO_MII_CONTROL_AN_EN;
	else /* CL37 Autoneg Disabled */
		reg_val &= ~(MDIO_COMBO_IEEO_MII_CONTROL_AN_EN |
		    MDIO_COMBO_IEEO_MII_CONTROL_RESTART_AN);

	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_MII_CONTROL, reg_val);

	/* Enable/Disable Autodetection */
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_SERDES_DIGITAL, MDIO_SERDES_DIGITAL_A_1000X_CONTROL1,
	    &reg_val);
	reg_val &= ~(MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_SIGNAL_DETECT_EN |
	    MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_INVERT_SIGNAL_DETECT);
	reg_val |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_FIBER_MODE;

	if (vars->line_speed == SPEED_AUTO_NEG)
		reg_val |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_AUTODET;
	else
		reg_val &= ~MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_AUTODET;

	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_SERDES_DIGITAL, MDIO_SERDES_DIGITAL_A_1000X_CONTROL1,
	    reg_val);

	/* Enable TetonII and BAM autoneg. */
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_BAM_NEXT_PAGE, MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL,
	    &reg_val);
	if (vars->line_speed == SPEED_AUTO_NEG) {
		/* Enable BAM aneg Mode and TetonII aneg Mode. */
		reg_val |= (MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL_BAM_MODE |
		    MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL_TETON_AN);
	} else {
		/* TetonII and BAM Autoneg Disabled. */
		reg_val &= ~(MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL_BAM_MODE |
		    MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL_TETON_AN);
	}
	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_BAM_NEXT_PAGE, MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL,
	    reg_val);

	if (enable_cl73) {
		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_CL73_USERB0, MDIO_CL73_USERB0_CL73_UCTRL,
		    0xe);

		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_CL73_USERB0, MDIO_CL73_USERB0_CL73_BAM_CTRL1,
		    MDIO_CL73_USERB0_CL73_BAM_CTRL1_BAM_EN |
		    MDIO_CL73_USERB0_CL73_BAM_CTRL1_BAM_STATION_MNGR_EN |
		    MDIO_CL73_USERB0_CL73_BAM_CTRL1_BAM_NP_AFTER_BP_EN);

		/* Set the CL73 AN speed. */
		CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_CL73_IEEEB1, MDIO_CL73_IEEEB1_AN_ADV2,
		    &reg_val);
		if (params->speed_cap_mask &
		    PORT_HW_CFG_SPEED_CAPABILITY_D0_10G)
			reg_val |= MDIO_CL73_IEEEB1_AN_ADV2_ADVR_10G_KX4;
		if (params->speed_cap_mask &
		    PORT_HW_CFG_SPEED_CAPABILITY_D0_1G)
			reg_val |= MDIO_CL73_IEEEB1_AN_ADV2_ADVR_1000M_KX;

		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_CL73_IEEEB1, MDIO_CL73_IEEEB1_AN_ADV2,
		    reg_val);

		/* CL73 Autoneg Enabled. */
		reg_val = MDIO_CL73_IEEEB0_CL73_AN_CONTROL_AN_EN;

	} else /* CL73 Autoneg Disabled */
		reg_val = 0;

	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_CL73_IEEEB0, MDIO_CL73_IEEEB0_CL73_AN_CONTROL,
	    reg_val);
}

/* Program SerDes, forced speed. */
static void
bxe_program_serdes(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint16_t reg_val;

	sc = params->sc;
	/* Program duplex, disable autoneg and sgmii.*/
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_MII_CONTROL, &reg_val);
	reg_val &= ~(MDIO_COMBO_IEEO_MII_CONTROL_FULL_DUPLEX |
	    MDIO_COMBO_IEEO_MII_CONTROL_AN_EN |
	    MDIO_COMBO_IEEO_MII_CONTROL_MAN_SGMII_SP_MASK);
	if (params->req_duplex == DUPLEX_FULL)
		reg_val |= MDIO_COMBO_IEEO_MII_CONTROL_FULL_DUPLEX;
	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_MII_CONTROL, reg_val);

	/*
	 * Program speed
	 * - needed only if the speed is greater than 1G (2.5G or 10G).
	 */
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_SERDES_DIGITAL, MDIO_SERDES_DIGITAL_MISC1, &reg_val);
	/* Clearing the speed value before setting the right speed. */
	DBPRINT(sc, BXE_VERBOSE_PHY, "MDIO_REG_BANK_SERDES_DIGITAL = 0x%x\n",
	    reg_val);

	reg_val &= ~(MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_MASK |
	    MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_SEL);

	if (!((vars->line_speed == SPEED_1000) ||
	    (vars->line_speed == SPEED_100) ||
	    (vars->line_speed == SPEED_10))) {
		reg_val |= (MDIO_SERDES_DIGITAL_MISC1_REFCLK_SEL_156_25M |
		    MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_SEL);
		if (vars->line_speed == SPEED_10000)
			reg_val |=
			    MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_10G_CX4;
		if (vars->line_speed == SPEED_13000)
			reg_val |=
			    MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_13G;
	}

	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_SERDES_DIGITAL, MDIO_SERDES_DIGITAL_MISC1, reg_val);
}

static void
bxe_set_brcm_cl37_advertisment(struct link_params *params)
{
	struct bxe_softc *sc;
	uint16_t val;

	sc = params->sc;
	val = 0;

	/* Configure the 48 bits for BAM AN. */

	/* Set extended capabilities. */
	if (params->speed_cap_mask & PORT_HW_CFG_SPEED_CAPABILITY_D0_2_5G)
		val |= MDIO_OVER_1G_UP1_2_5G;
	if (params->speed_cap_mask & PORT_HW_CFG_SPEED_CAPABILITY_D0_10G)
		val |= MDIO_OVER_1G_UP1_10G;
	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_OVER_1G, MDIO_OVER_1G_UP1, val);

	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_OVER_1G, MDIO_OVER_1G_UP3, 0x400);
}

static void
bxe_calc_ieee_aneg_adv(struct link_params *params, uint16_t *ieee_fc)
{

	*ieee_fc = MDIO_COMBO_IEEE0_AUTO_NEG_ADV_FULL_DUPLEX;
	/*
	 * Resolve pause mode and advertisement.
	 * Please refer to Table 28B-3 of the 802.3ab-1999 spec.
	 */

	switch (params->req_flow_ctrl) {
	case FLOW_CTRL_AUTO:
		if (params->req_fc_auto_adv == FLOW_CTRL_BOTH) {
			*ieee_fc |= MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH;
		} else {
			*ieee_fc |=
			    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC;
		}
		break;
	case FLOW_CTRL_TX:
		*ieee_fc |= MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC;
		break;
	case FLOW_CTRL_RX:
	case FLOW_CTRL_BOTH:
		*ieee_fc |= MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH;
		break;
	case FLOW_CTRL_NONE:
	default:
		*ieee_fc |= MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_NONE;
		break;
	}
}

static void
bxe_set_ieee_aneg_advertisment(struct link_params *params, uint16_t ieee_fc)
{
	struct bxe_softc *sc;
	uint16_t val;

	sc = params->sc;

	/* For AN, we are always publishing full duplex. */

	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_AUTO_NEG_ADV, ieee_fc);
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_CL73_IEEEB1, MDIO_CL73_IEEEB1_AN_ADV1, &val);
	val &= ~MDIO_CL73_IEEEB1_AN_ADV1_PAUSE_BOTH;
	val |= ((ieee_fc<<3) & MDIO_CL73_IEEEB1_AN_ADV1_PAUSE_MASK);
	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_CL73_IEEEB1, MDIO_CL73_IEEEB1_AN_ADV1, val);
}

static void
bxe_restart_autoneg(struct link_params *params, uint8_t enable_cl73)
{
	struct bxe_softc *sc;
	uint16_t mii_control;

	sc = params->sc;
	DBPRINT(sc, BXE_VERBOSE_PHY, "bxe_restart_autoneg\n");
	/* Enable and restart BAM/CL37 aneg. */

	if (enable_cl73) {
		CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_CL73_IEEEB0, MDIO_CL73_IEEEB0_CL73_AN_CONTROL,
		    &mii_control);

		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_CL73_IEEEB0, MDIO_CL73_IEEEB0_CL73_AN_CONTROL,
		    (mii_control | MDIO_CL73_IEEEB0_CL73_AN_CONTROL_AN_EN |
		    MDIO_CL73_IEEEB0_CL73_AN_CONTROL_RESTART_AN));
	} else {
		CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_MII_CONTROL,
		    &mii_control);
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "bxe_restart_autoneg mii_control before = 0x%x\n",
		    mii_control);
		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_MII_CONTROL,
		    (mii_control | MDIO_COMBO_IEEO_MII_CONTROL_AN_EN |
		    MDIO_COMBO_IEEO_MII_CONTROL_RESTART_AN));
	}
}

static void
bxe_initialize_sgmii_process(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint16_t control1, mii_control;

	sc = params->sc;
	/* In SGMII mode, the unicore is always slave. */

	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_SERDES_DIGITAL, MDIO_SERDES_DIGITAL_A_1000X_CONTROL1,
	    &control1);
	control1 |= MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_INVERT_SIGNAL_DETECT;
	/* set sgmii mode (and not fiber) */
	control1 &= ~(MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_FIBER_MODE |
	    MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_AUTODET |
	    MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_MSTR_MODE);
	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_SERDES_DIGITAL, MDIO_SERDES_DIGITAL_A_1000X_CONTROL1,
	    control1);

	/* if forced speed */
	if (!(vars->line_speed == SPEED_AUTO_NEG)) {
		/* Set speed, disable autoneg. */
		CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_MII_CONTROL,
		    &mii_control);
		mii_control &= ~(MDIO_COMBO_IEEO_MII_CONTROL_AN_EN |
		    MDIO_COMBO_IEEO_MII_CONTROL_MAN_SGMII_SP_MASK|
		    MDIO_COMBO_IEEO_MII_CONTROL_FULL_DUPLEX);

		switch (vars->line_speed) {
		case SPEED_100:
			mii_control |=
			    MDIO_COMBO_IEEO_MII_CONTROL_MAN_SGMII_SP_100;
			break;
		case SPEED_1000:
			mii_control |=
			    MDIO_COMBO_IEEO_MII_CONTROL_MAN_SGMII_SP_1000;
			break;
		case SPEED_10:
			/* There is nothing to set for 10M. */
			break;
		default:
			/* Invalid speed for SGMII. */
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "Invalid line_speed 0x%x\n", vars->line_speed);
			break;
		}

		/* Setting the full duplex. */
		if (params->req_duplex == DUPLEX_FULL)
			mii_control |= MDIO_COMBO_IEEO_MII_CONTROL_FULL_DUPLEX;
		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_MII_CONTROL,
		    mii_control);
	} else { /* AN mode */
		/* Enable and restart AN. */
		bxe_restart_autoneg(params, 0);
	}
}

/*
 * Link management.
 */
static void
bxe_pause_resolve(struct link_vars *vars, uint32_t pause_result)
{

						/*  LD	    LP	 */
	switch (pause_result) { 		/* ASYM P ASYM P */
	case 0xb:       			/*   1  0   1  1 */
		vars->flow_ctrl = FLOW_CTRL_TX;
		break;
	case 0xe:       			/*   1  1   1  0 */
		vars->flow_ctrl = FLOW_CTRL_RX;
		break;
	case 0x5:       			/*   0  1   0  1 */
	case 0x7:       			/*   0  1   1  1 */
	case 0xd:       			/*   1  1   0  1 */
	case 0xf:       			/*   1  1   1  1 */
		vars->flow_ctrl = FLOW_CTRL_BOTH;
		break;
	default:
		break;
	}
	if (pause_result & (1<<0))
		vars->link_status |= LINK_STATUS_LINK_PARTNER_SYMMETRIC_PAUSE;
	if (pause_result & (1<<1))
		vars->link_status |= LINK_STATUS_LINK_PARTNER_ASYMMETRIC_PAUSE;
}

static uint8_t
bxe_ext_phy_resolve_fc(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint8_t ext_phy_addr;
	uint16_t ld_pause;   /* local */
	uint16_t lp_pause;   /* link partner */
	uint16_t pause_result;
	uint8_t port, ret;

	sc = params->sc;
	ret = 0;
	port = params->port;
	/* Read twice. */
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	if (vars->autoneg & AUTO_NEG_COMPLETE) {
		ret = 1;
		bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr,
		    MDIO_AN_DEVAD, MDIO_AN_REG_ADV_PAUSE, &ld_pause);
		bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr,
		    MDIO_AN_DEVAD, MDIO_AN_REG_LP_AUTO_NEG, &lp_pause);
		pause_result = (ld_pause & MDIO_AN_REG_ADV_PAUSE_MASK) >> 8;
		pause_result |= (lp_pause & MDIO_AN_REG_ADV_PAUSE_MASK) >> 10;
		DBPRINT(sc, BXE_VERBOSE_PHY, "Ext PHY pause result 0x%x \n",
		    pause_result);
		bxe_pause_resolve(vars, pause_result);
		if (vars->flow_ctrl == FLOW_CTRL_NONE && ext_phy_type ==
		    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073) {
			bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr,
			    MDIO_AN_DEVAD, MDIO_AN_REG_CL37_FC_LD, &ld_pause);

			bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr,
			    MDIO_AN_DEVAD, MDIO_AN_REG_CL37_FC_LP, &lp_pause);
			pause_result = (ld_pause &
			    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH) >> 5;
			pause_result |= (lp_pause &
			    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH) >> 7;

			bxe_pause_resolve(vars, pause_result);
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "Ext PHY CL37 pause result 0x%x \n", pause_result);
		}
	}
	return (ret);
}

uint8_t bxe_direct_parallel_detect_used(struct link_params *);

uint8_t
bxe_direct_parallel_detect_used(struct link_params *params)
{
	struct bxe_softc *sc;
	uint16_t pd_10g, status2_1000x;

	sc = params->sc;
	if (params->req_line_speed != SPEED_AUTO_NEG)
		return (0);
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_SERDES_DIGITAL, MDIO_SERDES_DIGITAL_A_1000X_STATUS2,
	    &status2_1000x);
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_SERDES_DIGITAL, MDIO_SERDES_DIGITAL_A_1000X_STATUS2,
	    &status2_1000x);
	if (status2_1000x & MDIO_SERDES_DIGITAL_A_1000X_STATUS2_AN_DISABLED) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "1G parallel detect link on port %d\n", params->port);
		return (1);
	}
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_10G_PARALLEL_DETECT,
	    MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_STATUS, &pd_10g);
	if (pd_10g & MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_STATUS_PD_LINK) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "10G parallel detect link on port %d\n", params->port);
		return (1);
	}
	return (0);
}

static void
bxe_flow_ctrl_resolve(struct link_params *params, struct link_vars *vars,
    uint32_t gp_status)
{
	struct bxe_softc *sc;
	uint16_t ld_pause;   /* local driver */
	uint16_t lp_pause;   /* link partner */
	uint16_t pause_result;

	sc = params->sc;
	vars->flow_ctrl = FLOW_CTRL_NONE;

	/* Resolve from gp_status in case of AN complete and not sgmii. */
	if (params->req_flow_ctrl != FLOW_CTRL_AUTO)
                vars->flow_ctrl = params->req_flow_ctrl;
	else if (params->req_line_speed != SPEED_AUTO_NEG)
		vars->flow_ctrl = params->req_fc_auto_adv;
	else if (XGXS_EXT_PHY_TYPE(params->ext_phy_config) !=
	    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT)
		bxe_ext_phy_resolve_fc(params, vars);
	else if ((gp_status & MDIO_AN_CL73_OR_37_COMPLETE) &&
	    (!(vars->phy_flags & PHY_SGMII_FLAG))) {
		if (bxe_direct_parallel_detect_used(params)) {
			vars->flow_ctrl = params->req_fc_auto_adv;
			return;
		}
		if ((gp_status &
		    (MDIO_GP_STATUS_TOP_AN_STATUS1_CL73_AUTONEG_COMPLETE |
		    MDIO_GP_STATUS_TOP_AN_STATUS1_CL73_MR_LP_NP_AN_ABLE)) ==
		    (MDIO_GP_STATUS_TOP_AN_STATUS1_CL73_AUTONEG_COMPLETE |
		    MDIO_GP_STATUS_TOP_AN_STATUS1_CL73_MR_LP_NP_AN_ABLE)) {
			CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
			    MDIO_REG_BANK_CL73_IEEEB1, MDIO_CL73_IEEEB1_AN_ADV1,
			    &ld_pause);
			CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
			    MDIO_REG_BANK_CL73_IEEEB1,
			    MDIO_CL73_IEEEB1_AN_LP_ADV1, &lp_pause);
			pause_result = (ld_pause &
			    MDIO_CL73_IEEEB1_AN_ADV1_PAUSE_MASK) >> 8;
			pause_result |= (lp_pause &
			    MDIO_CL73_IEEEB1_AN_LP_ADV1_PAUSE_MASK) >> 10;
			DBPRINT(sc, BXE_VERBOSE_PHY, "pause_result CL73 0x%x\n",
			    pause_result);
		} else {
			CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
			    MDIO_REG_BANK_COMBO_IEEE0,
			    MDIO_COMBO_IEEE0_AUTO_NEG_ADV, &ld_pause);
			CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
			    MDIO_REG_BANK_COMBO_IEEE0,
			    MDIO_COMBO_IEEE0_AUTO_NEG_LINK_PARTNER_ABILITY1,
			    &lp_pause);
			pause_result = (ld_pause &
			    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_MASK) >> 5;
			pause_result |= (lp_pause &
			    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_MASK)>>7;
			DBPRINT(sc, BXE_VERBOSE_PHY, "pause_result CL37 0x%x\n",
			    pause_result);
		}
		bxe_pause_resolve(vars, pause_result);
	}

	DBPRINT(sc, BXE_VERBOSE_PHY, "%s(): flow_ctrl 0x%x\n",
	    __FUNCTION__, vars->flow_ctrl);
}

static void
bxe_check_fallback_to_cl37(struct link_params *params)
{
	struct bxe_softc *sc;
	uint16_t rx_status, ustat_val, cl37_fsm_recieved;

	sc = params->sc;

	DBPRINT(sc, BXE_VERBOSE_PHY, "%s(): IEEE 802.3 Clause 37 Fallback\n",
	     __FUNCTION__);

	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr, MDIO_REG_BANK_RX0,
	    MDIO_RX0_RX_STATUS, &rx_status);
	if ((rx_status & MDIO_RX0_RX_STATUS_SIGDET) !=
	    (MDIO_RX0_RX_STATUS_SIGDET)) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "No signal detected. Restoring CL73."
		    "rx_status(0x80b0) = 0x%x\n", rx_status);
		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
		    MDIO_REG_BANK_CL73_IEEEB0, MDIO_CL73_IEEEB0_CL73_AN_CONTROL,
		    MDIO_CL73_IEEEB0_CL73_AN_CONTROL_AN_EN);
		return;
	}
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_CL73_USERB0, MDIO_CL73_USERB0_CL73_USTAT1,
	    &ustat_val);
	if ((ustat_val & (MDIO_CL73_USERB0_CL73_USTAT1_LINK_STATUS_CHECK |
	    MDIO_CL73_USERB0_CL73_USTAT1_AN_GOOD_CHECK_BAM37)) !=
	    (MDIO_CL73_USERB0_CL73_USTAT1_LINK_STATUS_CHECK |
	    MDIO_CL73_USERB0_CL73_USTAT1_AN_GOOD_CHECK_BAM37)) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "CL73 state-machine is not stable. "
		    "ustat_val(0x8371) = 0x%x\n", ustat_val);
		return;
	}
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_REMOTE_PHY, MDIO_REMOTE_PHY_MISC_RX_STATUS,
	    &cl37_fsm_recieved);
	if ((cl37_fsm_recieved &
	    (MDIO_REMOTE_PHY_MISC_RX_STATUS_CL37_FSM_RECEIVED_OVER1G_MSG |
	    MDIO_REMOTE_PHY_MISC_RX_STATUS_CL37_FSM_RECEIVED_BRCM_OUI_MSG)) !=
	    (MDIO_REMOTE_PHY_MISC_RX_STATUS_CL37_FSM_RECEIVED_OVER1G_MSG |
	    MDIO_REMOTE_PHY_MISC_RX_STATUS_CL37_FSM_RECEIVED_BRCM_OUI_MSG)) {
		DBPRINT(sc, BXE_VERBOSE_PHY, "No CL37 FSM were received. "
		    "misc_rx_status(0x8330) = 0x%x\n", cl37_fsm_recieved);
		return;
	}
	CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_CL73_IEEEB0, MDIO_CL73_IEEEB0_CL73_AN_CONTROL, 0);
	bxe_restart_autoneg(params, 0);

	DBPRINT(sc, BXE_INFO, "%s(): Disabling CL73 and restarting CL37 "
	    "autoneg\n", __FUNCTION__);
}

static void
bxe_an_resolve(struct link_params *params, struct link_vars *vars,
    uint32_t gp_status)

{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint16_t val;
	uint8_t ext_phy_addr;

	sc = params->sc;
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);

	switch (ext_phy_type) {
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT:
		if (gp_status & MDIO_AN_CL73_OR_37_COMPLETE) {
			vars->autoneg |= AUTO_NEG_COMPLETE;
			vars->link_status |=
			    LINK_STATUS_AUTO_NEGOTIATE_COMPLETE;
		}

		if (bxe_direct_parallel_detect_used(params)) {
			vars->autoneg |= AUTO_NEG_PARALLEL_DETECTION_USED;
			vars->link_status |=
			    LINK_STATUS_PARALLEL_DETECTION_USED;
		}
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM84823:
		if (vars->line_speed < SPEED_10000) {
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_AN_DEVAD,
			    MDIO_AN_REG_8481_LEGACY_MII_STATUS, &val);
			if (val & (1 << 5)) {
				vars->autoneg |= AUTO_NEG_COMPLETE;
				vars->link_status |=
				    LINK_STATUS_AUTO_NEGOTIATE_COMPLETE;
			}
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_AN_DEVAD,
			    MDIO_AN_REG_8481_LEGACY_AN_EXPANSION, &val);
			if ((val & (1 << 0)) == 0) {
				vars->autoneg |=
				    AUTO_NEG_PARALLEL_DETECTION_USED;
				vars->link_status |=
					LINK_STATUS_PARALLEL_DETECTION_USED;
			}
			break;
		};
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
		bxe_cl45_read(sc, params->port, ext_phy_type, ext_phy_addr,
		    MDIO_AN_DEVAD, MDIO_AN_REG_STATUS, &val);
		bxe_cl45_read(sc, params->port, ext_phy_type, ext_phy_addr,
		    MDIO_AN_DEVAD, MDIO_AN_REG_STATUS, &val);
		if (val & (1 << 5)) {
			vars->autoneg |= AUTO_NEG_COMPLETE;
			vars->link_status |=
			    LINK_STATUS_AUTO_NEGOTIATE_COMPLETE;
		}
		if ((val & (1 << 0)) == 0) {
			vars->autoneg |= AUTO_NEG_PARALLEL_DETECTION_USED;
			vars->link_status |=
			    LINK_STATUS_PARALLEL_DETECTION_USED;
		}
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
		vars->autoneg |= AUTO_NEG_COMPLETE;
		vars->link_status |= LINK_STATUS_AUTO_NEGOTIATE_COMPLETE;
		break;
	default:
		break;
	}
	DBPRINT(sc, BXE_VERBOSE_PHY, "AN result for port %d: 0x%x\n",
	    params->port, vars->autoneg);
}

static uint8_t
bxe_link_settings_status(struct link_params *params, struct link_vars *vars,
    uint32_t gp_status, uint8_t ext_phy_link_up)
{
	struct bxe_softc *sc;
	uint16_t new_line_speed;

	sc = params->sc;
	vars->link_status = 0;
	vars->autoneg = 0;
	if (params->req_line_speed == SPEED_AUTO_NEG) {
		vars->link_status |= LINK_STATUS_AUTO_NEGOTIATE_ENABLED;
		vars->autoneg |= AUTO_NEG_ENABLED;
	}
	if (gp_status & MDIO_GP_STATUS_TOP_AN_STATUS1_LINK_STATUS) {
		DBPRINT(sc, BXE_VERBOSE_PHY, "phy link up gp_status=0x%x\n",
			 gp_status);

		vars->phy_link_up = 1;
		vars->link_status |= LINK_STATUS_LINK_UP;

		if (gp_status & MDIO_GP_STATUS_TOP_AN_STATUS1_DUPLEX_STATUS)
			vars->duplex = DUPLEX_FULL;
		else
			vars->duplex = DUPLEX_HALF;
		if (params->req_line_speed == SPEED_AUTO_NEG)
			bxe_an_resolve(params, vars, gp_status);

		bxe_flow_ctrl_resolve(params, vars, gp_status);

		switch (gp_status & GP_STATUS_SPEED_MASK) {
		case GP_STATUS_10M:
			new_line_speed = SPEED_10;
			if (vars->duplex == DUPLEX_FULL)
				vars->link_status |= LINK_10TFD;
			else
				vars->link_status |= LINK_10THD;
			break;
		case GP_STATUS_100M:
			new_line_speed = SPEED_100;
			if (vars->duplex == DUPLEX_FULL)
				vars->link_status |= LINK_100TXFD;
			else
				vars->link_status |= LINK_100TXHD;
			break;
		case GP_STATUS_1G:
		case GP_STATUS_1G_KX:
			new_line_speed = SPEED_1000;
			if (vars->duplex == DUPLEX_FULL)
				vars->link_status |= LINK_1000TFD;
			else
				vars->link_status |= LINK_1000THD;
			break;
		case GP_STATUS_2_5G:
			new_line_speed = SPEED_2500;
			if (vars->duplex == DUPLEX_FULL)
				vars->link_status |= LINK_2500TFD;
			else
				vars->link_status |= LINK_2500THD;
			break;
		case GP_STATUS_5G:
		case GP_STATUS_6G:
			DBPRINT(sc, BXE_VERBOSE_PHY,
				 "link speed unsupported  gp_status 0x%x\n",
				  gp_status);
			return (-EINVAL);
		case GP_STATUS_10G_KX4:
		case GP_STATUS_10G_HIG:
		case GP_STATUS_10G_CX4:
			new_line_speed = SPEED_10000;
			vars->link_status |= LINK_10GTFD;
			break;
		case GP_STATUS_12G_HIG:
			new_line_speed = SPEED_12000;
			vars->link_status |= LINK_12GTFD;
			break;
		case GP_STATUS_12_5G:
			new_line_speed = SPEED_12500;
			vars->link_status |= LINK_12_5GTFD;
			break;
		case GP_STATUS_13G:
			new_line_speed = SPEED_13000;
			vars->link_status |= LINK_13GTFD;
			break;
		case GP_STATUS_15G:
			new_line_speed = SPEED_15000;
			vars->link_status |= LINK_15GTFD;
			break;
		case GP_STATUS_16G:
			new_line_speed = SPEED_16000;
			vars->link_status |= LINK_16GTFD;
			break;
		default:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "link speed unsupported gp_status 0x%x\n",
			    gp_status);
			return (-EINVAL);
		}

		/*
		 * Upon link speed change set the NIG into drain mode.
		 * Comes to deals with possible FIFO glitch due to clk change
		 * when speed is decreased without link down indicator.
		 */
		if (new_line_speed != vars->line_speed) {
			if (XGXS_EXT_PHY_TYPE(params->ext_phy_config) !=
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT &&
			    ext_phy_link_up) {
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "Internal link speed %d is different "
				    "than the external link speed %d\n",
				    new_line_speed, vars->line_speed);
				vars->phy_link_up = 0;
				return (0);
			}
			REG_WR(sc, NIG_REG_EGRESS_DRAIN0_MODE +
			    params->port * 4, 0);
			msleep(1);
		}
		vars->line_speed = new_line_speed;

		if (vars->flow_ctrl & FLOW_CTRL_TX)
			vars->link_status |=
			    LINK_STATUS_TX_FLOW_CONTROL_ENABLED;

		if (vars->flow_ctrl & FLOW_CTRL_RX)
			vars->link_status |=
			    LINK_STATUS_RX_FLOW_CONTROL_ENABLED;

		if (!ext_phy_link_up)
			vars->link_status = 0;
	} else { /* link_down */
		DBPRINT(sc, BXE_VERBOSE_PHY, "phy link down\n");

		vars->phy_link_up = 0;

		vars->duplex = DUPLEX_FULL;
		vars->flow_ctrl = FLOW_CTRL_NONE;
		vars->autoneg = AUTO_NEG_DISABLED;
		vars->mac_type = MAC_TYPE_NONE;
		if ((params->req_line_speed == SPEED_AUTO_NEG) &&
		    ((XGXS_EXT_PHY_TYPE(params->ext_phy_config) ==
		    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT))) {
			bxe_check_fallback_to_cl37(params);
		}
	}
	DBPRINT(sc, BXE_VERBOSE_PHY,
	    "gp_status 0x%x  phy_link_up %x line_speed %x \n", gp_status,
	    vars->phy_link_up, vars->line_speed);
	DBPRINT(sc, BXE_VERBOSE_PHY,
	    "duplex %x  flow_ctrl 0x%x autoneg 0x%x\n", vars->duplex,
	    vars->flow_ctrl, vars->autoneg);
	DBPRINT(sc, BXE_VERBOSE_PHY, "link_status 0x%x\n", vars->link_status);

	return (0);
}

static void
bxe_set_gmii_tx_driver(struct link_params *params)
{
	struct bxe_softc *sc;
	uint16_t lp_up2;
	uint16_t tx_driver;
	uint16_t bank;

	sc = params->sc;
	/* read precomp */
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_OVER_1G, MDIO_OVER_1G_LP_UP2, &lp_up2);

	/* bits [10:7] at lp_up2, positioned at [15:12] */
	lp_up2 = (((lp_up2 & MDIO_OVER_1G_LP_UP2_PREEMPHASIS_MASK) >>
	    MDIO_OVER_1G_LP_UP2_PREEMPHASIS_SHIFT) <<
	    MDIO_TX0_TX_DRIVER_PREEMPHASIS_SHIFT);

	if (lp_up2 == 0)
		return;

	for (bank = MDIO_REG_BANK_TX0; bank <= MDIO_REG_BANK_TX3;
	    bank += (MDIO_REG_BANK_TX1 - MDIO_REG_BANK_TX0)) {
		CL45_RD_OVER_CL22(sc, params->port, params->phy_addr, bank,
		    MDIO_TX0_TX_DRIVER, &tx_driver);

		/* Replace tx_driver bits [15:12] */
		if (lp_up2 != (tx_driver &
		    MDIO_TX0_TX_DRIVER_PREEMPHASIS_MASK)) {
			tx_driver &= ~MDIO_TX0_TX_DRIVER_PREEMPHASIS_MASK;
			tx_driver |= lp_up2;
			CL45_WR_OVER_CL22(sc, params->port, params->phy_addr,
			    bank, MDIO_TX0_TX_DRIVER, tx_driver);
		}
	}
}

static uint8_t
bxe_emac_program(struct link_params *params, uint32_t line_speed,
    uint32_t duplex)
{
	struct bxe_softc *sc;
	uint16_t mode;
	uint8_t port;

	sc = params->sc;
	port = params->port;
	mode = 0;

	DBPRINT(sc, BXE_VERBOSE_PHY, "setting link speed & duplex\n");
	bxe_bits_dis(sc, GRCBASE_EMAC0 + port * 0x400 + EMAC_REG_EMAC_MODE,
	    (EMAC_MODE_25G_MODE | EMAC_MODE_PORT_MII_10M |
	    EMAC_MODE_HALF_DUPLEX));
	switch (line_speed) {
	case SPEED_10:
		mode |= EMAC_MODE_PORT_MII_10M;
		break;
	case SPEED_100:
		mode |= EMAC_MODE_PORT_MII;
		break;
	case SPEED_1000:
		mode |= EMAC_MODE_PORT_GMII;
		break;
	case SPEED_2500:
		mode |= (EMAC_MODE_25G_MODE | EMAC_MODE_PORT_GMII);
		break;
	default:
		/* 10G not valid for EMAC */
		DBPRINT(sc, BXE_VERBOSE_PHY, "Invalid line_speed 0x%x\n",
		    line_speed);
		return (-EINVAL);
	}

	if (duplex == DUPLEX_HALF)
		mode |= EMAC_MODE_HALF_DUPLEX;
	bxe_bits_en(sc, GRCBASE_EMAC0 + port * 0x400 + EMAC_REG_EMAC_MODE,
	    mode);

	bxe_set_led(params, LED_MODE_OPER, line_speed);
	return (0);
}

/*
 * External Phy section
 */
void
bxe_ext_phy_hw_reset(struct bxe_softc *sc, uint8_t port)
{

	bxe_set_gpio(sc, MISC_REGISTERS_GPIO_1, MISC_REGISTERS_GPIO_OUTPUT_LOW,
	    port);
	msleep(1);
	bxe_set_gpio(sc, MISC_REGISTERS_GPIO_1, MISC_REGISTERS_GPIO_OUTPUT_HIGH,
	    port);
}

static void
bxe_ext_phy_reset(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint8_t ext_phy_addr;

	sc = params->sc;
	DBPRINT(sc, BXE_VERBOSE_PHY, "Port %x: bxe_ext_phy_reset\n",
	    params->port);
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	/*
	 * The PHY reset is controled by GPIO 1.
	 * Give it 1ms of reset pulse.
	 */
	if (vars->phy_flags & PHY_XGXS_FLAG) {
		switch (ext_phy_type) {
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT:
			DBPRINT(sc, BXE_VERBOSE_PHY, "XGXS Direct\n");
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8706:
			DBPRINT(sc, BXE_VERBOSE_PHY, "XGXS 8705/8706\n");

			/* Restore normal power mode*/
			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_2,
			    MISC_REGISTERS_GPIO_OUTPUT_HIGH, params->port);

			/* HW reset */
			bxe_ext_phy_hw_reset(sc, params->port);

			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_CTRL, 0xa040);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
			/* Restore normal power mode*/
			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_2,
			    MISC_REGISTERS_GPIO_OUTPUT_HIGH, params->port);

			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_1,
			    MISC_REGISTERS_GPIO_OUTPUT_HIGH, params->port);

			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_CTRL,
			    1 << 15);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
			DBPRINT(sc, BXE_VERBOSE_PHY, "XGXS 8072\n");

			/*
			 * Unset Low Power Mode and SW reset.
			 * Restore normal power mode.
			 */
			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_2,
			    MISC_REGISTERS_GPIO_OUTPUT_HIGH, params->port);

			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_CTRL,
			    1 << 15);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
			DBPRINT(sc, BXE_VERBOSE_PHY, "XGXS 8073\n");

			/* Restore normal power mode. */
			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_2,
			    MISC_REGISTERS_GPIO_OUTPUT_HIGH, params->port);

			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_1,
			    MISC_REGISTERS_GPIO_OUTPUT_HIGH, params->port);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
			DBPRINT(sc, BXE_VERBOSE_PHY, "XGXS SFX7101\n");

			/* Restore normal power mode. */
			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_2,
			    MISC_REGISTERS_GPIO_OUTPUT_HIGH, params->port);

			/* HW reset */
			bxe_ext_phy_hw_reset(sc, params->port);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481:
			/* Restore normal power mode. */
			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_2,
			    MISC_REGISTERS_GPIO_OUTPUT_HIGH, params->port);

			/* HW reset */
			bxe_ext_phy_hw_reset(sc, params->port);

			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_CTRL,
			    1 << 15);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM84823:
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "XGXS PHY Failure detected\n");
			break;

		default:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "BAD XGXS ext_phy_config 0x%x\n",
			    params->ext_phy_config);
			break;
		}

	} else { /* SerDes */
		ext_phy_type = SERDES_EXT_PHY_TYPE(params->ext_phy_config);
		switch (ext_phy_type) {
		case PORT_HW_CFG_SERDES_EXT_PHY_TYPE_DIRECT:
			DBPRINT(sc, BXE_VERBOSE_PHY, "SerDes Direct\n");
			break;
		case PORT_HW_CFG_SERDES_EXT_PHY_TYPE_BCM5482:
			DBPRINT(sc, BXE_VERBOSE_PHY, "SerDes 5482\n");
			bxe_ext_phy_hw_reset(sc, params->port);
			break;
		default:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "BAD SerDes ext_phy_config 0x%x\n",
			    params->ext_phy_config);
			break;
		}
	}
}

static void
bxe_save_spirom_version(struct bxe_softc *sc, uint8_t port, uint32_t shmem_base,
    uint32_t spirom_ver)
{

	DBPRINT(sc, BXE_VERBOSE_PHY, "FW version 0x%x:0x%x\n",
	    (uint16_t)(spirom_ver >> 16), (uint16_t)spirom_ver);
	REG_WR(sc, shmem_base + offsetof(struct shmem_region,
	    port_mb[port].ext_phy_fw_version), spirom_ver);
}

static void
bxe_save_bcm_spirom_ver(struct bxe_softc *sc, uint8_t port,
    uint32_t ext_phy_type, uint8_t ext_phy_addr, uint32_t shmem_base)
{
	uint16_t fw_ver1, fw_ver2;
	uint8_t status;

	status = 0;
	status = bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr,
	    MDIO_PMA_DEVAD, MDIO_PMA_REG_ROM_VER1, &fw_ver1);
	status |= bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr,
	    MDIO_PMA_DEVAD, MDIO_PMA_REG_ROM_VER2, &fw_ver2);
	bxe_save_spirom_version(sc, port, shmem_base,
	    (uint32_t)(fw_ver1 << 16 | fw_ver2));

	if (status)
		BXE_PRINTF("Reading the external PHY ROM failed. Status:0x%x\n",
		    status);
}

static void
bxe_save_8481_spirom_version(struct bxe_softc *sc, uint8_t port,
    uint8_t ext_phy_addr, uint32_t shmem_base)
{
	uint16_t val, fw_ver1, fw_ver2, cnt;
	/*
	 * For the 32 bits registers in 8481, access via MDIO2ARM interface.
	 * (1) set register 0xc200_0014(SPI_BRIDGE_CTRL_2) to 0x03000000.
	 */
	bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
	    ext_phy_addr, MDIO_PMA_DEVAD, 0xA819, 0x0014);
	bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
	    ext_phy_addr, MDIO_PMA_DEVAD, 0xA81A, 0xc200);
	bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
	    ext_phy_addr, MDIO_PMA_DEVAD, 0xA81B, 0x0000);
	bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
	    ext_phy_addr, MDIO_PMA_DEVAD, 0xA81C, 0x0300);
	bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
	    ext_phy_addr, MDIO_PMA_DEVAD, 0xA817, 0x0009);

	for (cnt = 0; cnt < 100; cnt++) {
		bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
		    ext_phy_addr, MDIO_PMA_DEVAD, 0xA818, &val);
		if (val & 1)
			break;
		DELAY(5);
	}
	if (cnt == 100) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "Unable to read 8481 phy fw version(1)\n");
		bxe_save_spirom_version(sc, port, shmem_base, 0);
		return;
	}

	/* 2) read register 0xc200_0000 (SPI_FW_STATUS). */
	bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
	    ext_phy_addr, MDIO_PMA_DEVAD, 0xA819, 0x0000);
	bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
	    ext_phy_addr, MDIO_PMA_DEVAD, 0xA81A, 0xc200);
	bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
	    ext_phy_addr, MDIO_PMA_DEVAD, 0xA817, 0x000A);
	for (cnt = 0; cnt < 100; cnt++) {
		bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
		    ext_phy_addr, MDIO_PMA_DEVAD, 0xA818, &val);
		if (val & 1)
			break;
		DELAY(5);
	}
	if (cnt == 100) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "Unable to read 8481 phy fw version(2)\n");
		bxe_save_spirom_version(sc, port, shmem_base, 0);
		return;
	}

	/* Lower 16 bits of the register SPI_FW_STATUS. */
	bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
	    ext_phy_addr, MDIO_PMA_DEVAD, 0xA81B, &fw_ver1);
	/* Upper 16 bits of register SPI_FW_STATUS. */
	bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481,
	    ext_phy_addr, MDIO_PMA_DEVAD, 0xA81C, &fw_ver2);

	bxe_save_spirom_version(sc, port, shmem_base, (fw_ver2<<16) | fw_ver1);
}

static void
bxe_bcm8072_external_rom_boot(struct link_params *params)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint8_t ext_phy_addr, port;

	sc = params->sc;
	port = params->port;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	/* Need to wait 200ms after reset. */
	msleep(200);
	/*
	 * Boot port from external ROM.
	 * Set ser_boot_ctl bit in the MISC_CTRL1 register.
	 */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_MISC_CTRL1, 0x0001);

	/* Reset internal microprocessor */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_GEN_CTRL, MDIO_PMA_REG_GEN_CTRL_ROM_RESET_INTERNAL_MP);
	/* Set micro reset = 0. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_GEN_CTRL, MDIO_PMA_REG_GEN_CTRL_ROM_MICRO_RESET);
	/* Reset internal microprocessor. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_GEN_CTRL, MDIO_PMA_REG_GEN_CTRL_ROM_RESET_INTERNAL_MP);
	/* Wait for 100ms for code download via SPI port. */
	msleep(100);

	/* Clear ser_boot_ctl bit. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_MISC_CTRL1, 0x0000);
	/* Wait 100ms. */
	msleep(100);

	bxe_save_bcm_spirom_ver(sc, port, ext_phy_type, ext_phy_addr,
	    params->shmem_base);
}

/* This is only required for 8073A1, version 102 only. */
static uint8_t
bxe_8073_is_snr_needed(struct link_params *params)
{
	struct bxe_softc *sc;
	uint8_t ext_phy_addr;
	uint16_t val;

	sc = params->sc;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);

	/* Read 8073 HW revision. */
	bxe_cl45_read(sc, params->port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_8073_CHIP_REV, &val);

	if (val != 1) {
		/* No need to workaround in 8073 A1. */
		return (0);
	}

	bxe_cl45_read(sc, params->port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_ROM_VER2, &val);

	/* SNR should be applied only for version 0x102. */
	if (val != 0x102)
		return (0);

	return (1);
}

static uint8_t
bxe_bcm8073_xaui_wa(struct link_params *params)
{
	struct bxe_softc *sc;
	uint16_t cnt, cnt1, val;
	uint8_t ext_phy_addr;

	sc = params->sc;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);

	bxe_cl45_read(sc, params->port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_8073_CHIP_REV, &val);

	if (val > 0) {
		/* No need to workaround in 8073 A1. */
		return (0);
	}
	/* XAUI workaround in 8073 A0: */

	/*
	 * After loading the boot ROM and restarting Autoneg,
	 * poll Dev1, Reg $C820:
	 */

	for (cnt = 0; cnt < 1000; cnt++) {
		bxe_cl45_read(sc, params->port,
		    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073, ext_phy_addr,
		    MDIO_PMA_DEVAD, MDIO_PMA_REG_8073_SPEED_LINK_STATUS, &val);
		/*
		 * If bit [14] = 0 or bit [13] = 0, continue on with
		 * system initialization (XAUI work-around not required,
		 * as these bits indicate 2.5G or 1G link up).
		 */
		if (!(val & (1<<14)) || !(val & (1<<13))) {
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "XAUI work-around not required\n");
			return (0);
		} else if (!(val & (1<<15))) {
			DBPRINT(sc, BXE_VERBOSE_PHY, "clc bit 15 went off\n");
			/*
			 * If bit 15 is 0, then poll Dev1, Reg $C841 until
			 * it's MSB (bit 15) goes to 1 (indicating that the
			 * XAUI workaround has completed), then continue on
			 * with system initialization.
			 */
			for (cnt1 = 0; cnt1 < 1000; cnt1++) {
				bxe_cl45_read(sc, params->port,
				    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_8073_XAUI_WA, &val);
				if (val & (1<<15)) {
					DBPRINT(sc, BXE_VERBOSE_PHY,
					    "XAUI workaround has completed\n");
					return (0);
				 }
				 msleep(3);
			}
			break;
		}
		msleep(3);
	}
	DBPRINT(sc, BXE_VERBOSE_PHY, "Warning: XAUI work-around timeout !!!\n");
	return (-EINVAL);
}

static void
bxe_bcm8073_bcm8727_external_rom_boot(struct bxe_softc *sc, uint8_t port,
    uint8_t ext_phy_addr, uint32_t ext_phy_type, uint32_t shmem_base)
{

	/* Boot port from external ROM.  */
	/* EDC grst */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_GEN_CTRL, 0x0001);

	/* ucode reboot and rst. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_GEN_CTRL, 0x008c);

	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_MISC_CTRL1, 0x0001);

	/* Reset internal microprocessor. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_GEN_CTRL, MDIO_PMA_REG_GEN_CTRL_ROM_MICRO_RESET);

	/* Release srst bit. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_GEN_CTRL, MDIO_PMA_REG_GEN_CTRL_ROM_RESET_INTERNAL_MP);

	/* Wait for 100ms for code download via SPI port. */
	msleep(100);

	/* Clear ser_boot_ctl bit. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_MISC_CTRL1, 0x0000);

	bxe_save_bcm_spirom_ver(sc, port, ext_phy_type, ext_phy_addr,
	    shmem_base);
}

static void
bxe_bcm8073_external_rom_boot(struct bxe_softc *sc, uint8_t port,
    uint8_t ext_phy_addr, uint32_t shmem_base)
{

	bxe_bcm8073_bcm8727_external_rom_boot(sc, port, ext_phy_addr,
	    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073, shmem_base);
}

static void
bxe_bcm8727_external_rom_boot(struct bxe_softc *sc, uint8_t port,
    uint8_t ext_phy_addr, uint32_t shmem_base)
{

	bxe_bcm8073_bcm8727_external_rom_boot(sc, port, ext_phy_addr,
	    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727, shmem_base);
}

static void
bxe_bcm8726_external_rom_boot(struct link_params *params)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint8_t ext_phy_addr, port;

	sc = params->sc;
	port = params->port;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	/* Need to wait 100ms after reset. */
	msleep(100);

	/* Set serial boot control for external load. */

	/* Micro controller re-boot. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_GEN_CTRL, 0x018B);

	/* Set soft reset. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_GEN_CTRL, MDIO_PMA_REG_GEN_CTRL_ROM_MICRO_RESET);

	/* Set PLL register value to be same like in P13 ver. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_MISC_CTRL1, 0x0001);

	/*
	 * Clear soft reset.
	 * Will automatically reset micro-controller re-boot.
	 */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_GEN_CTRL, MDIO_PMA_REG_GEN_CTRL_ROM_RESET_INTERNAL_MP);

	/* Wait for 150ms for microcode load. */
	msleep(150);

	/* Disable serial boot control, tristates pins SS_N, SCK, MOSI, MISO. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_MISC_CTRL1, 0x0000);

	msleep(200);
	bxe_save_bcm_spirom_ver(sc, port, ext_phy_type, ext_phy_addr,
	    params->shmem_base);
}

static void
bxe_sfp_set_transmitter(struct bxe_softc *sc, uint8_t port,
    uint32_t ext_phy_type, uint8_t ext_phy_addr, uint8_t tx_en)
{
	uint16_t val;

	DBPRINT(sc, BXE_VERBOSE_PHY,
	    "Setting transmitter tx_en=%x for port %x\n", tx_en, port);
	/* Disable/Enable transmitter ( TX laser of the SFP+ module.). */
	bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_PHY_IDENTIFIER, &val);

	if (tx_en)
		val &= ~(1<<15);
	else
		val |= (1<<15);

	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_PHY_IDENTIFIER, val);
}

static uint8_t
bxe_8726_read_sfp_module_eeprom(struct link_params *params, uint16_t addr,
    uint8_t byte_cnt, uint8_t *o_buf)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint16_t i, val;
	uint8_t ext_phy_addr, port;

	sc = params->sc;
	val = 0;
	port = params->port;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	if (byte_cnt > 16) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "Reading from eeprom is is limited to 0xf\n");
		return (-EINVAL);
	}
	/* Set the read command byte count. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_SFP_TWO_WIRE_BYTE_CNT, (byte_cnt | 0xa000));

	/* Set the read command address. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_SFP_TWO_WIRE_MEM_ADDR, addr);

	/* Activate read command. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_SFP_TWO_WIRE_CTRL, 0x2c0f);

	/* Wait up to 500us for command complete status. */
	for (i = 0; i < 100; i++) {
		bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr,
		    MDIO_PMA_DEVAD, MDIO_PMA_REG_SFP_TWO_WIRE_CTRL, &val);
		if ((val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK) ==
		    MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_COMPLETE)
			break;
		DELAY(5);
	}

	if ((val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK) !=
	    MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_COMPLETE) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "Got bad status 0x%x when reading from SFP+ EEPROM\n",
		    (val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK));
		return (-EINVAL);
	}

	/* Read the buffer. */
	for (i = 0; i < byte_cnt; i++) {
		bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr,
		    MDIO_PMA_DEVAD, MDIO_PMA_REG_8726_TWO_WIRE_DATA_BUF + i,
		    &val);
		o_buf[i] = (uint8_t)(val &
		    MDIO_PMA_REG_8726_TWO_WIRE_DATA_MASK);
	}

	for (i = 0; i < 100; i++) {
		bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr,
		    MDIO_PMA_DEVAD, MDIO_PMA_REG_SFP_TWO_WIRE_CTRL, &val);
		if ((val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK) ==
		    MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_IDLE)
			return (0);
		msleep(1);
	}
	return (-EINVAL);
}

static uint8_t
bxe_8727_read_sfp_module_eeprom(struct link_params *params, uint16_t addr,
    uint8_t byte_cnt, uint8_t *o_buf)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint16_t val, i;
	uint8_t ext_phy_addr, port;

	sc = params->sc;
	port = params->port;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	if (byte_cnt > 16) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "Reading from eeprom is is limited to 0xf\n");
		return (-EINVAL);
	}

	/* Need to read from 1.8000 to clear it. */
	bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_SFP_TWO_WIRE_CTRL,
	    &val);

	/* Set the read command byte count. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_SFP_TWO_WIRE_BYTE_CNT,
	    ((byte_cnt < 2) ? 2 : byte_cnt));

	/* Set the read command address. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_SFP_TWO_WIRE_MEM_ADDR, addr);
	/* Set the destination address. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    0x8004, MDIO_PMA_REG_8727_TWO_WIRE_DATA_BUF);

	/* Activate read command. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_SFP_TWO_WIRE_CTRL, 0x8002);
	/*
	 * Wait appropriate time for two-wire command to finish before
	 * polling the status register.
	 */
	msleep(1);

	/* Wait up to 500us for command complete status. */
	for (i = 0; i < 100; i++) {
		bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr,
		    MDIO_PMA_DEVAD, MDIO_PMA_REG_SFP_TWO_WIRE_CTRL, &val);
		if ((val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK) ==
		    MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_COMPLETE)
			break;
		DELAY(5);
	}

	if ((val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK) !=
	    MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_COMPLETE) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "Got bad status 0x%x when reading from SFP+ EEPROM\n",
		    (val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK));
		return (-EINVAL);
	}

	/* Read the buffer. */
	for (i = 0; i < byte_cnt; i++) {
		bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr,
		    MDIO_PMA_DEVAD, MDIO_PMA_REG_8727_TWO_WIRE_DATA_BUF + i,
		    &val);
		o_buf[i] = (uint8_t)(val &
		    MDIO_PMA_REG_8727_TWO_WIRE_DATA_MASK);
	}

	for (i = 0; i < 100; i++) {
		bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr,
		    MDIO_PMA_DEVAD, MDIO_PMA_REG_SFP_TWO_WIRE_CTRL, &val);
		if ((val & MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK) ==
		    MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_IDLE)
			return (0);
		msleep(1);
	}

	return (-EINVAL);
}

uint8_t
bxe_read_sfp_module_eeprom(struct link_params *params, uint16_t addr,
    uint8_t byte_cnt, uint8_t *o_buf)
{
	uint32_t ext_phy_type;

	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);
	if (ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726)
		return (bxe_8726_read_sfp_module_eeprom(params, addr, byte_cnt,
		    o_buf));
	else if (ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727)
		return (bxe_8727_read_sfp_module_eeprom(params, addr, byte_cnt,
		    o_buf));
	return (-EINVAL);
}

static uint8_t
bxe_get_edc_mode(struct link_params *params, uint16_t *edc_mode)
{
#ifdef BXE_DEBUG
	struct bxe_softc *sc = params->sc;
#endif
	uint8_t copper_module_type;
	uint8_t options[SFP_EEPROM_OPTIONS_SIZE];
	uint8_t val, check_limiting_mode;

	check_limiting_mode = 0;
	*edc_mode = EDC_MODE_LIMITING;

	/* First check for copper cable. */
	if (bxe_read_sfp_module_eeprom(params, SFP_EEPROM_CON_TYPE_ADDR, 1,
	    &val) != 0) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "Failed to read from SFP+ module EEPROM\n");
		return (-EINVAL);
	}

	switch (val) {
	case SFP_EEPROM_CON_TYPE_VAL_COPPER:
		/*
		 * Check if its active cable( includes SFP+ module)
		 * of passive cable.
		 */
		if (bxe_read_sfp_module_eeprom(params,
		    SFP_EEPROM_FC_TX_TECH_ADDR, 1, &copper_module_type) != 0) {
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "Failed to read copper-cable-type"
			    " from SFP+ EEPROM\n");
			return (-EINVAL);
		}

		if (copper_module_type &
		    SFP_EEPROM_FC_TX_TECH_BITMASK_COPPER_ACTIVE) {
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "Active Copper cable detected\n");
			check_limiting_mode = 1;
		} else if (copper_module_type &
		    SFP_EEPROM_FC_TX_TECH_BITMASK_COPPER_PASSIVE) {
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "Passive Copper cable detected\n");
			*edc_mode = EDC_MODE_PASSIVE_DAC;
		} else {
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "Unknown copper-cable-type 0x%x !!!\n",
			    copper_module_type);
			return (-EINVAL);
		}
		break;
	case SFP_EEPROM_CON_TYPE_VAL_LC:
		DBPRINT(sc, BXE_VERBOSE_PHY, "Optic module detected\n");
		check_limiting_mode = 1;
		break;
	default:
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "Unable to determine module type 0x%x !!!\n", val);
		return (-EINVAL);
	}

	if (check_limiting_mode) {
		if (bxe_read_sfp_module_eeprom(params, SFP_EEPROM_OPTIONS_ADDR,
		    SFP_EEPROM_OPTIONS_SIZE, options) != 0) {
			DBPRINT(sc, BXE_VERBOSE_PHY, "Failed to read Option"
			    " field from module EEPROM\n");
			return (-EINVAL);
		}
		if ((options[0] & SFP_EEPROM_OPTIONS_LINEAR_RX_OUT_MASK))
			*edc_mode = EDC_MODE_LINEAR;
		else
			*edc_mode = EDC_MODE_LIMITING;
	}
	DBPRINT(sc, BXE_VERBOSE_PHY, "EDC mode is set to 0x%x\n", *edc_mode);
	return (0);
}

/*
 * This function read the relevant field from the module ( SFP+ ),
 * and verify it is compliant with this board.
 */
static uint8_t bxe_verify_sfp_module(struct link_params *params)
{
	struct bxe_softc *sc = params->sc;
	uint32_t val;
	/* uint32_t fw_resp; */
	char vendor_name[SFP_EEPROM_VENDOR_NAME_SIZE + 1];
	char vendor_pn[SFP_EEPROM_PART_NO_SIZE + 1];

	sc = params->sc;
	val = REG_RD(sc, params->shmem_base + offsetof(struct shmem_region,
	    dev_info.port_feature_config[params->port].config));
	if ((val & PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_MASK) ==
	    PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_NO_ENFORCEMENT) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "NOT enforcing module verification\n");
		return (0);
	}

	/* Ask the FW to validate the module. */
	if (!(params->feature_config_flags &
	    FEATURE_CONFIG_BC_SUPPORTS_OPT_MDL_VRFY)) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "FW does not support OPT MDL verification\n");
		return (-EINVAL);
	}
	/* Format the warning message. */
	if (bxe_read_sfp_module_eeprom(params, SFP_EEPROM_VENDOR_NAME_ADDR,
	    SFP_EEPROM_VENDOR_NAME_SIZE, (uint8_t *)vendor_name))
		vendor_name[0] = '\0';
	else
		vendor_name[SFP_EEPROM_VENDOR_NAME_SIZE] = '\0';
	if (bxe_read_sfp_module_eeprom(params, SFP_EEPROM_PART_NO_ADDR,
	    SFP_EEPROM_PART_NO_SIZE, (uint8_t *)vendor_pn))
		vendor_pn[0] = '\0';
	else
		vendor_pn[SFP_EEPROM_PART_NO_SIZE] = '\0';

	printf("Warning: Unqualified SFP+ module detected on %s, "
	    "Port %d from %s part number %s\n", sc->name, params->port,
	    vendor_name, vendor_pn);
	return (-EINVAL);
}

static uint8_t
bxe_bcm8726_set_limiting_mode(struct link_params *params, uint16_t edc_mode)
{
	struct bxe_softc *sc;
	uint16_t cur_limiting_mode;
	uint8_t ext_phy_addr, port;

	sc = params->sc;
	port = params->port;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);

	bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_ROM_VER2,
	    &cur_limiting_mode);
	DBPRINT(sc, BXE_VERBOSE_PHY, "Current Limiting mode is 0x%x\n",
	    cur_limiting_mode);

	if (edc_mode == EDC_MODE_LIMITING) {
		DBPRINT(sc, BXE_VERBOSE_PHY, "Setting LIMITING MODE\n");
		bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726,
		    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_ROM_VER2,
		    EDC_MODE_LIMITING);
	} else { /* LRM mode ( default )*/

		DBPRINT(sc, BXE_VERBOSE_PHY, "Setting LRM MODE\n");

		/*
		 * Changing to LRM mode takes quite few seconds.
		 * So do it only if current mode is limiting
		 * ( default is LRM ).
		 */
		if (cur_limiting_mode != EDC_MODE_LIMITING)
			return (0);

		bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726,
		    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_LRM_MODE, 0);
		bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726,
		    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_ROM_VER2, 0x128);
		bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726,
		    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_MISC_CTRL0,
		    0x4008);
		bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726,
		    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_LRM_MODE,
		    0xaaaa);
	}
	return (0);
}

static uint8_t
bxe_bcm8727_set_limiting_mode(struct link_params *params, uint16_t edc_mode)
{
	struct bxe_softc *sc;
	uint8_t ext_phy_addr, port;
	uint16_t phy_identifier;
	uint16_t rom_ver2_val;

	sc = params->sc;
	port = params->port;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);

	bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_PHY_IDENTIFIER,
	    &phy_identifier);

	bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_PHY_IDENTIFIER,
	    (phy_identifier & ~(1 << 9)));

	bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_ROM_VER2, &rom_ver2_val);
	/* Keep the MSB 8-bits, and set the LSB 8-bits with the edc_mode. */
	bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_ROM_VER2,
	    (rom_ver2_val & 0xff00) | (edc_mode & 0x00ff));

	bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_PHY_IDENTIFIER,
	    (phy_identifier | (1<<9)));

	return (0);
}

static uint8_t
bxe_wait_for_sfp_module_initialized(struct link_params *params)
{
	struct bxe_softc *sc;
	uint16_t timeout;
	uint8_t val;

	sc = params->sc;
	/*
	 * Initialization time after hot-plug may take up to 300ms for some
	 * phys type ( e.g. JDSU ).
	 */
	for (timeout = 0; timeout < 60; timeout++) {
		if (bxe_read_sfp_module_eeprom(params, 1, 1, &val)
		    == 0) {
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "SFP+ module initialization took %d ms\n",
			    timeout * 5);
			return (0);
		}
		msleep(5);
	}
	return (-EINVAL);
}

/* Make sure GPIOs are not using for LED mode. */
static void
bxe_8727_power_module(struct bxe_softc *sc, struct link_params *params,
    uint8_t ext_phy_addr, uint8_t is_power_up)
{
	uint16_t val;
	uint8_t port;

	port = params->port;
	/*
	 * In the GPIO register, bit 4 is use to detemine if the GPIOs are
	 * operating as INPUT or as OUTPUT. Bit 1 is for input, and 0 for
	 * output
	 * Bits 0-1 determine the gpios value for OUTPUT in case bit 4 val is 0
	 * Bits 8-9 determine the gpios value for INPUT in case bit 4 val is 1
	 * where the 1st bit is the over-current(only input), and 2nd bit is
	 * for power( only output ).
	 */

	/*
	 * In case of NOC feature is disabled and power is up, set GPIO control
	 *  as input to enable listening of over-current indication.
	 */

	if (!(params->feature_config_flags & FEATURE_CONFIG_BCM8727_NOC) &&
	    is_power_up)
		val = (1<<4);
	else
		/*
		 * Set GPIO control to OUTPUT, and set the power bit
		 * to according to the is_power_up.
		 */
		val = ((!(is_power_up)) << 1);

	bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_8727_GPIO_CTRL, val);
}

static uint8_t
bxe_sfp_module_detection(struct link_params *params)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type, val;
	uint16_t edc_mode;
	uint8_t ext_phy_addr, rc;

	sc = params->sc;
	rc = 0;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);
	val = REG_RD(sc, params->shmem_base + offsetof(struct shmem_region,
	    dev_info.port_feature_config[params->port].config));

	DBPRINT(sc, BXE_VERBOSE_PHY,
	    "SFP+ module plugged in/out detected on port %d\n", params->port);

	if (bxe_get_edc_mode(params, &edc_mode) != 0) {
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "Failed to get valid module type\n");
		return (-EINVAL);
	} else if (bxe_verify_sfp_module(params) != 0) {
		/* Check SFP+ module compatibility. */
		DBPRINT(sc, BXE_VERBOSE_PHY, "Module verification failed!!\n");
		rc = -EINVAL;
		/* Turn on fault module-detected led. */
		bxe_set_gpio(sc, MISC_REGISTERS_GPIO_0,
		    MISC_REGISTERS_GPIO_HIGH, params->port);
		if ((ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727) &&
		    ((val & PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_MASK) ==
		    PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_POWER_DOWN)) {
			/* Shutdown SFP+ module. */
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "Shutdown SFP+ module!!\n");
			bxe_8727_power_module(sc, params, ext_phy_addr, 0);
			return (rc);
		}
	} else {
		/* Turn off fault module-detected led. */
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "Turn off fault module-detected led\n");
		bxe_set_gpio(sc, MISC_REGISTERS_GPIO_0, MISC_REGISTERS_GPIO_LOW,
		    params->port);
	}

	/* Power up the SFP module. */
	if (ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727)
		bxe_8727_power_module(sc, params, ext_phy_addr, 1);

	/*
	 * Check and set limiting mode / LRM mode on 8726.
	 * On 8727 it is done automatically.
	 */
	if (ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726)
		bxe_bcm8726_set_limiting_mode(params, edc_mode);
	else
		bxe_bcm8727_set_limiting_mode(params, edc_mode);
	/*
	 * Enable transmit for this module if the module is approved, or
	 * if unapproved modules should also enable the Tx laser.
	 */
	if (rc == 0 || (val & PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_MASK) !=
	    PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_DISABLE_TX_LASER)
		bxe_sfp_set_transmitter(sc, params->port, ext_phy_type,
		    ext_phy_addr, 1);
	else
		bxe_sfp_set_transmitter(sc, params->port, ext_phy_type,
		    ext_phy_addr, 0);

	return (rc);
}

void
bxe_handle_module_detect_int(struct link_params *params)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type, gpio_val, val;
	uint8_t ext_phy_addr, port;

	sc = params->sc;
	port = params->port;

	/* Set valid module led off. */
	bxe_set_gpio(sc, MISC_REGISTERS_GPIO_0, MISC_REGISTERS_GPIO_HIGH,
	    params->port);

	/* Get current gpio val refelecting module plugged in / out. */
	gpio_val = bxe_get_gpio(sc,  MISC_REGISTERS_GPIO_3, port);

	/* Call the handling function in case module is detected. */
	if (gpio_val == 0) {
		bxe_set_gpio_int(sc, MISC_REGISTERS_GPIO_3,
		    MISC_REGISTERS_GPIO_INT_OUTPUT_CLR, port);

		if (bxe_wait_for_sfp_module_initialized(params) == 0)
			bxe_sfp_module_detection(params);
		else
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "SFP+ module is not initialized\n");
	} else {
		ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
		ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);
		val = REG_RD(sc, params->shmem_base +
		    offsetof(struct shmem_region,
		    dev_info.port_feature_config[params->port].config));

		bxe_set_gpio_int(sc, MISC_REGISTERS_GPIO_3,
		    MISC_REGISTERS_GPIO_INT_OUTPUT_SET, port);
		/* Module was plugged out. */
		/* Disable transmit for this module. */
		if ((val & PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_MASK) ==
		    PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_DISABLE_TX_LASER)
			bxe_sfp_set_transmitter(sc, params->port, ext_phy_type,
			    ext_phy_addr, 0);
	}
}

static void
bxe_bcm807x_force_10G(struct link_params *params)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint8_t ext_phy_addr, port;

	sc = params->sc;
	port = params->port;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	/* Force KR or KX. */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_CTRL, 0x2040);
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_10G_CTRL2, 0x000b);
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_PMA_DEVAD,
	    MDIO_PMA_REG_BCM_CTRL, 0x0000);
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_AN_DEVAD,
	    MDIO_AN_REG_CTRL, 0x0000);
}

static void
bxe_bcm8073_set_xaui_low_power_mode(struct link_params *params)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint16_t val;
	uint8_t ext_phy_addr, port;

	sc = params->sc;
	port = params->port;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	bxe_cl45_read(sc, params->port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_8073_CHIP_REV, &val);

	if (val == 0) {
		/* Mustn't set low power mode in 8073 A0. */
		return;
	}

	/* Disable PLL sequencer (use read-modify-write to clear bit 13). */
	bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    MDIO_XS_PLL_SEQUENCER, &val);
	val &= ~(1 << 13);
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    MDIO_XS_PLL_SEQUENCER, val);

	/* PLL controls */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    0x805E, 0x1077);
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    0x805D, 0x0000);
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    0x805C, 0x030B);
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    0x805B, 0x1240);
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    0x805A, 0x2490);

	/* Tx Controls */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    0x80A7, 0x0C74);
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    0x80A6, 0x9041);
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    0x80A5, 0x4640);

	/* Rx Controls */
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    0x80FE, 0x01C4);
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    0x80FD, 0x9249);
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    0x80FC, 0x2015);

	/* Enable PLL sequencer (use read-modify-write to set bit 13). */
	bxe_cl45_read(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    MDIO_XS_PLL_SEQUENCER, &val);
	val |= (1 << 13);
	bxe_cl45_write(sc, port, ext_phy_type, ext_phy_addr, MDIO_XS_DEVAD,
	    MDIO_XS_PLL_SEQUENCER, val);
}

static void
bxe_8073_set_pause_cl37(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint16_t cl37_val;
	uint8_t ext_phy_addr;

	sc = params->sc;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	bxe_cl45_read(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_AN_DEVAD, MDIO_AN_REG_CL37_FC_LD, &cl37_val);

	cl37_val &= ~MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH;
	/* Please refer to Table 28B-3 of 802.3ab-1999 spec. */

	if ((vars->ieee_fc & MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_SYMMETRIC) ==
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_SYMMETRIC)
		cl37_val |=  MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_SYMMETRIC;
	if ((vars->ieee_fc & MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC) ==
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC)
		cl37_val |=  MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC;
	if ((vars->ieee_fc & MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH) ==
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH)
		cl37_val |= MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH;
	DBPRINT(sc, BXE_VERBOSE_PHY, "Ext phy AN advertize cl37 0x%x\n",
	    cl37_val);

	bxe_cl45_write(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_AN_DEVAD, MDIO_AN_REG_CL37_FC_LD, cl37_val);
	msleep(500);
}

static void
bxe_ext_phy_set_pause(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint16_t val;
	uint8_t ext_phy_addr;

	sc = params->sc;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	/* Read modify write pause advertizing. */
	bxe_cl45_read(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_AN_DEVAD, MDIO_AN_REG_ADV_PAUSE, &val);

	val &= ~MDIO_AN_REG_ADV_PAUSE_BOTH;

	/* Please refer to Table 28B-3 of 802.3ab-1999 spec. */

	if ((vars->ieee_fc & MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC) ==
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC)
		val |=  MDIO_AN_REG_ADV_PAUSE_ASYMMETRIC;
	if ((vars->ieee_fc & MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH) ==
	    MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH)
		val |= MDIO_AN_REG_ADV_PAUSE_PAUSE;
	DBPRINT(sc, BXE_VERBOSE_PHY, "Ext phy AN advertize 0x%x\n", val);
	bxe_cl45_write(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_AN_DEVAD, MDIO_AN_REG_ADV_PAUSE, val);
}

static void
bxe_set_preemphasis(struct link_params *params)
{
	struct bxe_softc *sc = params->sc;
	uint16_t bank, i;

	sc = params->sc;
	for (bank = MDIO_REG_BANK_RX0, i = 0; bank <= MDIO_REG_BANK_RX3;
	    bank += (MDIO_REG_BANK_RX1-MDIO_REG_BANK_RX0), i++) {
		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr, bank,
		    MDIO_RX0_RX_EQ_BOOST, params->xgxs_config_rx[i]);
	}

	for (bank = MDIO_REG_BANK_TX0, i = 0; bank <= MDIO_REG_BANK_TX3;
	    bank += (MDIO_REG_BANK_TX1 - MDIO_REG_BANK_TX0), i++) {
		CL45_WR_OVER_CL22(sc, params->port, params->phy_addr, bank,
		    MDIO_TX0_TX_DRIVER, params->xgxs_config_tx[i]);
	}
}

static void
bxe_8481_set_led4(struct link_params *params, uint32_t ext_phy_type,
    uint8_t ext_phy_addr)
{
	struct bxe_softc *sc;

	sc = params->sc;
	/* PHYC_CTL_LED_CTL */

	/* Enable continous signal to go active on link. */
	bxe_cl45_write(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_PMA_DEVAD, MDIO_PMA_REG_8481_LINK_SIGNAL, 0xa482);

	/* Unmask LED4 for 10G link. */
	bxe_cl45_write(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_PMA_DEVAD, MDIO_PMA_REG_8481_SIGNAL_MASK, (1 << 6));
	/* Unmask LED4 for 10G link. */
	bxe_cl45_write(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_AN_DEVAD, 0xFFFB, 0xFFFD);
}

static void
bxe_8481_set_legacy_led_mode(struct link_params *params, uint32_t ext_phy_type,
    uint8_t ext_phy_addr)
{
	struct bxe_softc *sc;

	sc = params->sc;
	/*
	 * LED1 (10G Link): Disable LED1 when 10/100/1000 link.
	 * LED2 (1G/100/10 Link): Enable LED2 when 10/100/1000 link).
	 */
	bxe_cl45_write(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_AN_DEVAD, MDIO_AN_REG_8481_LEGACY_SHADOW, (1<<15) |
	    (0xd << 10) | (0xc<<4) | 0xe);
}

static void
bxe_8481_set_10G_led_mode(struct link_params *params, uint32_t ext_phy_type,
    uint8_t ext_phy_addr)
{
	struct bxe_softc *sc;
	uint16_t val1;

	sc = params->sc;
	/*
	 * LED1 (10G Link)
	 * Enable continuse based on source 7(10G-link).
	 */
	bxe_cl45_read(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_PMA_DEVAD, MDIO_PMA_REG_8481_LINK_SIGNAL, &val1);
	/* Set bit 2 to 0, and bits [1:0] to 10. */
	val1 &= ~((1<<0) | (1<<2) | (1<<7)); /* Clear bits 0,2,7*/
	val1 |= ((1<<1) | (1<<6)); /* Set bit 1, 6 */

	bxe_cl45_write(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_PMA_DEVAD, MDIO_PMA_REG_8481_LINK_SIGNAL, val1);

	/* Unmask LED1 for 10G link. */
	bxe_cl45_read(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_PMA_DEVAD, MDIO_PMA_REG_8481_LED1_MASK, &val1);
	/* Set bit 2 to 0, and bits [1:0] to 10. */
	val1 |= (1<<7);
	bxe_cl45_write(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_PMA_DEVAD, MDIO_PMA_REG_8481_LED1_MASK, val1);

	/*
	 * LED2 (1G/100/10G Link).
	 * Mask LED2 for 10G link.
	 */
	bxe_cl45_write(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_PMA_DEVAD, MDIO_PMA_REG_8481_LED2_MASK, 0);

	/* Unmask LED3 for 10G link. */
	bxe_cl45_write(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_PMA_DEVAD, MDIO_PMA_REG_8481_LED3_MASK, 0x6);
	bxe_cl45_write(sc, params->port, ext_phy_type, ext_phy_addr,
	    MDIO_PMA_DEVAD, MDIO_PMA_REG_8481_LED3_BLINK, 0);
}

static void
bxe_init_internal_phy(struct link_params *params, struct link_vars *vars,
    uint8_t enable_cl73)
{
	struct bxe_softc *sc;

	sc = params->sc;
	if (!(vars->phy_flags & PHY_SGMII_FLAG)) {
		if ((XGXS_EXT_PHY_TYPE(params->ext_phy_config) ==
		    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT) &&
		    (params->feature_config_flags &
		    FEATURE_CONFIG_OVERRIDE_PREEMPHASIS_ENABLED))
			bxe_set_preemphasis(params);

		/* Forced speed requested? */
		if (vars->line_speed != SPEED_AUTO_NEG ||
		    ((XGXS_EXT_PHY_TYPE(params->ext_phy_config) ==
		    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT) &&
		    params->loopback_mode == LOOPBACK_EXT)) {
			DBPRINT(sc, BXE_VERBOSE_PHY, "%s(): Not SGMII, no AN\n",
			    __FUNCTION__);

			/* Disable autoneg. */
			bxe_set_autoneg(params, vars, 0);

			/* Program speed and duplex. */
			bxe_program_serdes(params, vars);

		} else { /* AN_mode */
			DBPRINT(sc, BXE_VERBOSE_PHY, "not SGMII, AN\n");

			/* AN enabled. */
			bxe_set_brcm_cl37_advertisment(params);

			/* Program duplex & pause advertisement (for aneg). */
			bxe_set_ieee_aneg_advertisment(params, vars->ieee_fc);

			/* Enable autoneg. */
			bxe_set_autoneg(params, vars, enable_cl73);

			/* Enable and restart AN. */
			bxe_restart_autoneg(params, enable_cl73);
		}

	} else { /* SGMII mode */
		DBPRINT(sc, BXE_VERBOSE_PHY, "SGMII\n");

		bxe_initialize_sgmii_process(params, vars);
	}
}

static uint8_t
bxe_ext_phy_init(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint16_t cnt, ctrl, reg, val;
	uint16_t fw_ver1, fw_ver2;
	uint16_t lasi_ctrl_val, rx_alarm_ctrl_val, tmp1;
	uint16_t mod_abs, phy_ver;
	uint16_t autoneg_val, an_1000_val, an_10_100_val;
	uint16_t autoneg_ctrl, pma_ctrl;
	uint8_t ext_phy_addr, i, rc;

	sc = params->sc;
	ctrl = 0;
	val = 0;
	rc = 0;

	if (vars->phy_flags & PHY_XGXS_FLAG) {
		ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);

		ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);
		/*
		 * Make sure that the soft reset is off (expect for the 8072:
		 * due to the lock, it will be done inside the specific
		 * handling).
		 */
		if ((ext_phy_type != PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT) &&
		    (ext_phy_type != PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE) &&
		    (ext_phy_type != PORT_HW_CFG_XGXS_EXT_PHY_TYPE_NOT_CONN) &&
		    (ext_phy_type != PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072) &&
		    (ext_phy_type != PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073)) {
			/* Wait for soft reset to get cleared upto 1 sec. */
			for (cnt = 0; cnt < 1000; cnt++) {
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_CTRL, &ctrl);
				if (!(ctrl & (1<<15)))
					break;
				msleep(1);
			}
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "control reg 0x%x (after %d ms)\n", ctrl, cnt);
		}

		switch (ext_phy_type) {
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT:
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705:
			DBPRINT(sc, BXE_VERBOSE_PHY, "XGXS 8705\n");

			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_MISC_CTRL, 0x8288);
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_PHY_IDENTIFIER, 0x7fbf);
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_CMU_PLL_BYPASS, 0x0100);
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_WIS_DEVAD,
			    MDIO_WIS_REG_LASI_CNTL, 0x1);

			/* BCM8705 doesn't have microcode, hence the 0. */
			bxe_save_spirom_version(sc, params->port,
			    params->shmem_base, 0);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8706:
			/* Wait until fw is loaded. */
			for (cnt = 0; cnt < 100; cnt++) {
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_ROM_VER1, &val);
				if (val)
					break;
				msleep(10);
			}
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "XGXS 8706 is initialized after %d ms\n", cnt);
			if ((params->feature_config_flags &
			    FEATURE_CONFIG_OVERRIDE_PREEMPHASIS_ENABLED)) {
				for (i = 0; i < 4; i++) {
					reg = MDIO_XS_8706_REG_BANK_RX0 +
					    i * (MDIO_XS_8706_REG_BANK_RX1 -
					    MDIO_XS_8706_REG_BANK_RX0);
					bxe_cl45_read(sc, params->port,
					    ext_phy_type, ext_phy_addr,
					    MDIO_XS_DEVAD, reg, &val);
					/* Clear first 3 bits of the control. */
					val &= ~0x7;
					/*
					 * Set control bits according to
					 * configuation.
					 */
					val |= (params->xgxs_config_rx[i] &
					    0x7);
					DBPRINT(sc, BXE_VERBOSE_PHY,
		"Setting RX Equalizer to BCM8706 reg 0x%x <-- val 0x%x\n",
					    reg, val);
					bxe_cl45_write(sc, params->port,
					    ext_phy_type, ext_phy_addr,
					    MDIO_XS_DEVAD, reg, val);
				}
			}
			/* Force speed */
			if (params->req_line_speed == SPEED_10000) {
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "XGXS 8706 force 10Gbps\n");

				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_DIGITAL_CTRL, 0x400);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_LASI_CTRL, 1);
			} else {
				/*
				 * Force 1Gbps using autoneg with 1G
				 * advertisment.
				 */

				/* Allow CL37 through CL73. */
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "XGXS 8706 AutoNeg\n");
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_CL37_CL73, 0x040c);

				/* Enable Full-Duplex advertisment on CL37. */
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_CL37_FC_LP, 0x0020);
				/* Enable CL37 AN. */
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_CL37_AN, 0x1000);
				/* 1G support */
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_ADV, (1 << 5));

				/* Enable clause 73 AN. */
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_CTRL, 0x1200);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_RX_ALARM_CTRL, 0x0400);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_LASI_CTRL, 0x0004);

			}
			bxe_save_bcm_spirom_ver(sc, params->port, ext_phy_type,
			    ext_phy_addr, params->shmem_base);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
			DBPRINT(sc, BXE_VERBOSE_PHY, "Initializing BCM8726\n");
			bxe_bcm8726_external_rom_boot(params);

			/*
			 * Need to call module detected on initialization since
			 * the module detection triggered by actual module
			 * insertion might occur before driver is loaded, and
			 * when driver is loaded, it reset all registers,
			 * including the transmitter.
			 */
			bxe_sfp_module_detection(params);

			/* Set Flow control */
			bxe_ext_phy_set_pause(params, vars);
			if (params->req_line_speed == SPEED_1000) {
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "Setting 1G force\n");
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_CTRL, 0x40);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_10G_CTRL2, 0xD);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_LASI_CTRL, 0x5);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_RX_ALARM_CTRL, 0x400);
			} else if ((params->req_line_speed == SPEED_AUTO_NEG) &&
			    ((params->speed_cap_mask &
			    PORT_HW_CFG_SPEED_CAPABILITY_D0_1G))) {
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "Setting 1G clause37 \n");
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_ADV, 0x20);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_CL37_CL73, 0x040c);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_CL37_FC_LD, 0x0020);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_CL37_AN, 0x1000);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_CTRL, 0x1200);

				/*
				 * Enable RX-ALARM control to receive
				 * interrupt for 1G speed change.
				 */
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_LASI_CTRL, 0x4);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_RX_ALARM_CTRL, 0x400);

			} else { /* Default 10G. Set only LASI control */
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_LASI_CTRL, 1);
			}

			/* Set TX PreEmphasis if needed. */
			if ((params->feature_config_flags &
			    FEATURE_CONFIG_OVERRIDE_PREEMPHASIS_ENABLED)) {
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "Setting TX_CTRL1 0x%x, TX_CTRL2 0x%x\n",
				    params->xgxs_config_tx[0],
				    params->xgxs_config_tx[1]);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_8726_TX_CTRL1,
				    params->xgxs_config_tx[0]);

				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_8726_TX_CTRL2,
				    params->xgxs_config_tx[1]);
			}
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
			if (ext_phy_type ==
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072) {
				rx_alarm_ctrl_val = 0x400;
				lasi_ctrl_val = 0x0004;
			} else {
				rx_alarm_ctrl_val = (1<<2);
				lasi_ctrl_val = 0x0004;
			}

			/* Enable LASI. */
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_RX_ALARM_CTRL, rx_alarm_ctrl_val);

			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_LASI_CTRL, lasi_ctrl_val);

			bxe_8073_set_pause_cl37(params, vars);

			if (ext_phy_type ==
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072)
				bxe_bcm8072_external_rom_boot(params);
			else {
				/*
				 * In case of 8073 with long xaui lines,
				 * don't set the 8073 xaui low power.
				 */
				bxe_bcm8073_set_xaui_low_power_mode(params);
			}
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_M8051_MSGOUT_REG, &tmp1);

			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_RX_ALARM, &tmp1);

			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "Before rom RX_ALARM(port1):0x%x\n", tmp1);

			/*
			 * If this is forced speed, set to KR or KX
			 * (all other are not supported).
			 */
			if (params->loopback_mode == LOOPBACK_EXT) {
				bxe_bcm807x_force_10G(params);
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "Forced speed 10G on 807X\n");
				break;
			} else {
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_BCM_CTRL, 0x0002);
			}
			if (params->req_line_speed != SPEED_AUTO_NEG) {
				if (params->req_line_speed == SPEED_10000)
					val = (1 << 7);
				else if (params->req_line_speed ==
					   SPEED_2500) {
					val = (1 << 5);
					/*
					 * Note that 2.5G works only
					 * when used with 1G advertisment.
					 */
				} else
					val = (1 << 5);
			} else {
				val = 0;
				if (params->speed_cap_mask &
				    PORT_HW_CFG_SPEED_CAPABILITY_D0_10G)
					val |= (1 << 7);

				/*
				 * Note that 2.5G works only when
				 * used with 1G advertisment.
				 */
				if (params->speed_cap_mask &
				    (PORT_HW_CFG_SPEED_CAPABILITY_D0_1G |
				    PORT_HW_CFG_SPEED_CAPABILITY_D0_2_5G))
					val |= (1 << 5);
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "807x autoneg val = 0x%x\n", val);
			}

			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_AN_DEVAD, MDIO_AN_REG_ADV, val);
			if (ext_phy_type ==
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073) {
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_8073_2_5G, &tmp1);

				if (((params->speed_cap_mask &
				    PORT_HW_CFG_SPEED_CAPABILITY_D0_2_5G) &&
				    (params->req_line_speed ==
				    SPEED_AUTO_NEG)) ||
				    (params->req_line_speed == SPEED_2500)) {
					/* Allow 2.5G for A1 and above. */
					bxe_cl45_read(sc, params->port,
					  PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
					    ext_phy_addr, MDIO_PMA_DEVAD,
					    MDIO_PMA_REG_8073_CHIP_REV,
					    &phy_ver);
					DBPRINT(sc, BXE_VERBOSE_PHY,
					    "Add 2.5G\n");
					if (phy_ver > 0)
						tmp1 |= 1;
					else
						tmp1 &= 0xfffe;
				} else {
					DBPRINT(sc, BXE_VERBOSE_PHY,
					    "Disable 2.5G\n");
					tmp1 &= 0xfffe;
				}

				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_8073_2_5G, tmp1);
			}

			/* Add support for CL37 (passive mode) II. */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_AN_DEVAD,
			    MDIO_AN_REG_CL37_FC_LD, &tmp1);

			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_AN_DEVAD, MDIO_AN_REG_CL37_FC_LD,
			    (tmp1 | ((params->req_duplex == DUPLEX_FULL) ?
			    0x20 : 0x40)));

			/* Add support for CL37 (passive mode) III. */
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_AN_DEVAD, MDIO_AN_REG_CL37_AN,
			    0x1000);

			if (ext_phy_type ==
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073) {
				/*
				 * The SNR will improve about 2db by changing
				 * BW and FEE main tap. Rest commands are
				 * executed after link is up.
				 */
				/*
				 * Change FFE main cursor to 5 in EDC register.
				 */
				if (bxe_8073_is_snr_needed(params))
					bxe_cl45_write(sc, params->port,
					    ext_phy_type, ext_phy_addr,
					    MDIO_PMA_DEVAD,
					    MDIO_PMA_REG_EDC_FFE_MAIN, 0xFB0C);

				/*
				 * Enable FEC (Forware Error Correction)
				 * Request in the AN.
				 */
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_ADV2, &tmp1);
				tmp1 |= (1 << 15);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_ADV2, tmp1);
			}

			bxe_ext_phy_set_pause(params, vars);

			/* Restart autoneg. */
			msleep(500);
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_AN_DEVAD, MDIO_AN_REG_CTRL,
			    0x1200);
			DBPRINT(sc, BXE_VERBOSE_PHY, "807x Autoneg Restart: "
			    "Advertise 1G=%x, 10G=%x\n",
			    ((val & (1 << 5)) > 0), ((val & (1 << 7)) > 0));
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
			/* Enable PMD link, MOD_ABS_FLT, and 1G link alarm. */
			rx_alarm_ctrl_val = (1 << 2) | (1 << 5);
			lasi_ctrl_val = 0x0004;

			DBPRINT(sc, BXE_VERBOSE_PHY, "Initializing BCM8727\n");
			/* Enable LASI. */
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_RX_ALARM_CTRL, rx_alarm_ctrl_val);

			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_LASI_CTRL, lasi_ctrl_val);

			/*
			 * Initially configure  MOD_ABS to interrupt when
			 * module is presence( bit 8).
			 */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_PHY_IDENTIFIER, &mod_abs);
			/*
			 * Set EDC off by setting OPTXLOS signal input to low
			 * (bit 9). When the EDC is off it locks onto a
			 * reference clock and avoids becoming 'lost'.
			 */
			mod_abs &= ~((1 << 8) | (1 << 9));
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_PHY_IDENTIFIER, mod_abs);

			/* Make MOD_ABS give interrupt on change. */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_8727_PCS_OPT_CTRL, &val);
			val |= (1 << 12);
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_8727_PCS_OPT_CTRL, val);

			/*
			 * Set 8727 GPIOs to input to allow reading from the
			 * 8727 GPIO0 status which reflect SFP+ module
			 * over-current.
			 */
			bxe_cl45_read(sc, params->port,
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727, ext_phy_addr,
			    MDIO_PMA_DEVAD, MDIO_PMA_REG_8727_PCS_OPT_CTRL,
			    &val);
			val &= 0xff8f; /* Reset bits 4-6 */
			bxe_cl45_write(sc, params->port,
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727, ext_phy_addr,
			    MDIO_PMA_DEVAD, MDIO_PMA_REG_8727_PCS_OPT_CTRL,
			    val);

			bxe_8727_power_module(sc, params, ext_phy_addr, 1);
			bxe_bcm8073_set_xaui_low_power_mode(params);

			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_M8051_MSGOUT_REG, &tmp1);

			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_RX_ALARM, &tmp1);

			/* Set option 1G speed. */
			if (params->req_line_speed == SPEED_1000) {
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "Setting 1G force\n");
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_CTRL, 0x40);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_10G_CTRL2, 0xD);
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_10G_CTRL2, &tmp1);
				DBPRINT(sc, BXE_VERBOSE_PHY, "1.7 = 0x%x \n",
				    tmp1);
			} else if ((params->req_line_speed == SPEED_AUTO_NEG) &&
			    ((params->speed_cap_mask &
			    PORT_HW_CFG_SPEED_CAPABILITY_D0_1G))) {
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "Setting 1G clause37 \n");
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_PMA_REG_8727_MISC_CTRL, 0);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_CL37_AN, 0x1300);
			} else {
				/*
				 * Since the 8727 has only single reset pin,
				 * need to set the 10G registers although it
				 * is default.
				 */
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_CTRL, 0x0020);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    0x7, 0x0100);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_CTRL, 0x2040);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_10G_CTRL2, 0x0008);
			}

			/*
			 * Set 2-wire transfer rate to 400Khz since 100Khz
			 * is not operational.
			 */
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_8727_TWO_WIRE_SLAVE_ADDR, 0xa101);

			/* Set TX PreEmphasis if needed. */
			if ((params->feature_config_flags &
			    FEATURE_CONFIG_OVERRIDE_PREEMPHASIS_ENABLED)) {
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "Setting TX_CTRL1 0x%x, TX_CTRL2 0x%x\n",
				    params->xgxs_config_tx[0],
				    params->xgxs_config_tx[1]);
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_8727_TX_CTRL1,
				    params->xgxs_config_tx[0]);

				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_8727_TX_CTRL2,
				    params->xgxs_config_tx[1]);
			}
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "Setting the SFX7101 LASI indication\n");

			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_LASI_CTRL, 0x1);
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "Setting the SFX7101 LED to blink on traffic\n");
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_7107_LED_CNTL, (1 << 3));

			bxe_ext_phy_set_pause(params, vars);
			/* Restart autoneg. */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_AN_DEVAD, MDIO_AN_REG_CTRL,
			    &val);
			val |= 0x200;
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_AN_DEVAD, MDIO_AN_REG_CTRL, val);

			/* Save spirom version. */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_7101_VER1, &fw_ver1);

			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_7101_VER2, &fw_ver2);

			bxe_save_spirom_version(params->sc, params->port,
			    params->shmem_base,
			    (uint32_t)(fw_ver1 << 16 | fw_ver2));
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM84823:
			/*
			 * This phy uses the NIG latch mechanism since link
			 * indication arrives through its LED4 and not via
			 * its LASI signal, so we get steady signal
			 * instead of clear on read.
			 */
			bxe_bits_en(sc, NIG_REG_LATCH_BC_0 + params->port * 4,
			    1 << NIG_LATCH_BC_ENABLE_MI_INT);
			bxe_cl45_write(sc, params->port,
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481, ext_phy_addr,
			    MDIO_PMA_DEVAD, MDIO_PMA_REG_CTRL, 0x0000);

			bxe_8481_set_led4(params, ext_phy_type, ext_phy_addr);
			if (params->req_line_speed == SPEED_AUTO_NEG) {
				/* Set 1000 speed advertisement. */
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_8481_1000T_CTRL, &an_1000_val);

				bxe_ext_phy_set_pause(params, vars);
				if (params->speed_cap_mask &
				    PORT_HW_CFG_SPEED_CAPABILITY_D0_1G) {
					an_1000_val |= (1 << 8);
					if (params->req_duplex == DUPLEX_FULL)
						an_1000_val |= (1 << 9);
					DBPRINT(sc, BXE_VERBOSE_PHY,
					    "Advertising 1G\n");
				} else
					an_1000_val &= ~((1 << 8) | (1 << 9));

				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_8481_1000T_CTRL, an_1000_val);

				/* Set 100 speed advertisement. */
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_8481_LEGACY_AN_ADV,
				    &an_10_100_val);

				if (params->speed_cap_mask &
				  (PORT_HW_CFG_SPEED_CAPABILITY_D0_100M_FULL |
				  PORT_HW_CFG_SPEED_CAPABILITY_D0_100M_HALF)) {
					an_10_100_val |= (1 << 7);
					if (params->req_duplex == DUPLEX_FULL)
						an_10_100_val |= (1 << 8);
					DBPRINT(sc, BXE_VERBOSE_PHY,
					    "Advertising 100M\n");
				} else
					an_10_100_val &= ~((1 << 7) | (1 << 8));

				/* Set 10 speed advertisement. */
				if (params->speed_cap_mask &
				    (PORT_HW_CFG_SPEED_CAPABILITY_D0_10M_FULL |
				    PORT_HW_CFG_SPEED_CAPABILITY_D0_10M_HALF)) {
					an_10_100_val |= (1 << 5);
					if (params->req_duplex == DUPLEX_FULL)
						an_10_100_val |= (1 << 6);
					DBPRINT(sc, BXE_VERBOSE_PHY,
					    "Advertising 10M\n");
				} else
					an_10_100_val &= ~((1 << 5) | (1 << 6));

				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_8481_LEGACY_AN_ADV,
				    an_10_100_val);

				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_8481_LEGACY_MII_CTRL,
				    &autoneg_val);

				/* Disable forced speed. */
				autoneg_val &= ~(1 << 6 | 1 << 13);

				/*
				 * Enable autoneg and restart autoneg
				 * for legacy speeds.
				 */
				autoneg_val |= (1 << 9 | 1 << 12);

				if (params->req_duplex == DUPLEX_FULL)
					autoneg_val |= (1 << 8);
				else
					autoneg_val &= ~(1 << 8);

				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_8481_LEGACY_MII_CTRL,
				    autoneg_val);

				if (params->speed_cap_mask &
				    PORT_HW_CFG_SPEED_CAPABILITY_D0_10G) {
					DBPRINT(sc, BXE_VERBOSE_PHY,
					    "Advertising 10G\n");
					/* Restart autoneg for 10G */
					bxe_cl45_write(sc, params->port,
					    ext_phy_type, ext_phy_addr,
					    MDIO_AN_DEVAD,
					    MDIO_AN_REG_CTRL, val);
				}
			} else {
				/* Force speed */
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_8481_LEGACY_MII_CTRL,
				    &autoneg_ctrl);

				/* Disable autoneg. */
				autoneg_ctrl &= ~(1 << 12);

				/* Set 1000 force. */
				switch (params->req_line_speed) {
				case SPEED_10000:
					DBPRINT(sc, BXE_VERBOSE_PHY,
					    "Unable to set 10G force !\n");
					break;
				case SPEED_1000:
					bxe_cl45_read(sc, params->port,
					    ext_phy_type, ext_phy_addr,
					    MDIO_PMA_DEVAD, MDIO_PMA_REG_CTRL,
					    &pma_ctrl);
					autoneg_ctrl &= ~(1 << 13);
					autoneg_ctrl |= (1 << 6);
					pma_ctrl &= ~(1 << 13);
					pma_ctrl |= (1 << 6);
					DBPRINT(sc, BXE_VERBOSE_PHY,
					    "Setting 1000M force\n");
					bxe_cl45_write(sc, params->port,
					    ext_phy_type, ext_phy_addr,
					    MDIO_PMA_DEVAD, MDIO_PMA_REG_CTRL,
					    pma_ctrl);
					break;
				case SPEED_100:
					autoneg_ctrl |= (1 << 13);
					autoneg_ctrl &= ~(1 << 6);
					DBPRINT(sc, BXE_VERBOSE_PHY,
					    "Setting 100M force\n");
					break;
				case SPEED_10:
					autoneg_ctrl &= ~(1 << 13);
					autoneg_ctrl &= ~(1 << 6);
					DBPRINT(sc, BXE_VERBOSE_PHY,
					    "Setting 10M force\n");
					break;
				}

				/* Duplex mode */
				if (params->req_duplex == DUPLEX_FULL) {
					autoneg_ctrl |= (1 << 8);
					DBPRINT(sc, BXE_VERBOSE_PHY,
					    "Setting full duplex\n");
				} else
					autoneg_ctrl &= ~(1 << 8);

				/* Update autoneg ctrl and pma ctrl. */
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_8481_LEGACY_MII_CTRL,
				    autoneg_ctrl);
			}

			/* Save spirom version. */
			bxe_save_8481_spirom_version(sc, params->port,
			    ext_phy_addr, params->shmem_base);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "XGXS PHY Failure detected 0x%x\n",
			    params->ext_phy_config);
			rc = -EINVAL;
			break;
		default:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "BAD XGXS ext_phy_config 0x%x\n",
			    params->ext_phy_config);
			rc = -EINVAL;
			break;
		}

	} else { /* SerDes */
		ext_phy_type = SERDES_EXT_PHY_TYPE(params->ext_phy_config);
		switch (ext_phy_type) {
		case PORT_HW_CFG_SERDES_EXT_PHY_TYPE_DIRECT:
			DBPRINT(sc, BXE_VERBOSE_PHY, "SerDes Direct\n");
			break;
		case PORT_HW_CFG_SERDES_EXT_PHY_TYPE_BCM5482:
			DBPRINT(sc, BXE_VERBOSE_PHY, "SerDes 5482\n");
			break;
		default:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "BAD SerDes ext_phy_config 0x%x\n",
			    params->ext_phy_config);
			break;
		}
	}
	return (rc);
}

static void
bxe_8727_handle_mod_abs(struct link_params *params)
{
	struct bxe_softc *sc;
	uint32_t val;
	uint16_t mod_abs, rx_alarm_status;
	uint8_t ext_phy_addr;

	sc = params->sc;
	ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
	val = REG_RD(sc, params->shmem_base + offsetof(struct shmem_region,
	    dev_info.port_feature_config[params->port].config));

	bxe_cl45_read(sc, params->port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_PHY_IDENTIFIER,
	    &mod_abs);
	if (mod_abs & (1 << 8)) {
		/* Module is absent. */
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "MOD_ABS indication show module is absent\n");

		/*
		 * 1. Set mod_abs to detect next module presence event
		 * 2. Set EDC off by setting OPTXLOS signal input to low
		 *    (bit 9).
		 *    When the EDC is off it locks onto a reference clock and
		 *    avoids becoming 'lost'.
		 */
		mod_abs &= ~((1 << 8) | (1 << 9));
		bxe_cl45_write(sc, params->port,
		    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727, ext_phy_addr,
		    MDIO_PMA_DEVAD, MDIO_PMA_REG_PHY_IDENTIFIER, mod_abs);

		/*
		 * Clear RX alarm since it stays up as long as
		 * the mod_abs wasn't changed.
		 */
		bxe_cl45_read(sc, params->port,
		    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727, ext_phy_addr,
		    MDIO_PMA_DEVAD, MDIO_PMA_REG_RX_ALARM, &rx_alarm_status);
	} else {
		/* Module is present. */
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "MOD_ABS indication show module is present\n");
		/*
		 * First thing, disable transmitter, and if the
		 * module is ok, the module_detection will enable
		 * it. */

		/*
		 * 1. Set mod_abs to detect next module absent event ( bit 8)
		 * 2. Restore the default polarity of the OPRXLOS signal and
		 *    this signal will then correctly indicate the presence or
		 *    absence of the Rx signal. (bit 9)
		 */
		mod_abs |= ((1 << 8) | (1 << 9));
		bxe_cl45_write(sc, params->port,
		    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727, ext_phy_addr,
		    MDIO_PMA_DEVAD, MDIO_PMA_REG_PHY_IDENTIFIER, mod_abs);

		/*
		 * Clear RX alarm since it stays up as long as the mod_abs
		 * wasn't changed. This is need to be done before calling
		 * the module detection, otherwise it will clear the link
		 * update alarm.
		 */
		bxe_cl45_read(sc, params->port,
		    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727, ext_phy_addr,
		    MDIO_PMA_DEVAD, MDIO_PMA_REG_RX_ALARM, &rx_alarm_status);

		if ((val & PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_MASK) ==
		    PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_DISABLE_TX_LASER)
			bxe_sfp_set_transmitter(sc, params->port,
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727, ext_phy_addr,
			    0);

		if (bxe_wait_for_sfp_module_initialized(params) == 0)
			bxe_sfp_module_detection(params);
		else
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "SFP+ module is not initialized\n");
	}

	DBPRINT(sc, BXE_VERBOSE_PHY, "8727 RX_ALARM_STATUS 0x%x\n",
		 rx_alarm_status);
	/* No need to check link status in case of module plugged in/out. */
}

static uint8_t
bxe_ext_phy_is_link_up(struct link_params *params, struct link_vars *vars,
    uint8_t is_mi_int)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint16_t val1, val2;
	uint16_t rx_sd, pcs_status;
	uint16_t link_status;
	uint16_t rx_alarm_status;
	uint16_t an1000_status;
	uint16_t legacy_status, legacy_speed;
	uint8_t ext_phy_addr, ext_phy_link_up, port;

	sc = params->sc;
	val1 = 0;
	ext_phy_link_up = 0;
	port = params->port;

	if (vars->phy_flags & PHY_XGXS_FLAG) {
		ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
		ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);
		switch (ext_phy_type) {
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT:
			DBPRINT(sc, BXE_VERBOSE_PHY, "XGXS Direct\n");
			ext_phy_link_up = 1;
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705:
			DBPRINT(sc, BXE_VERBOSE_PHY, "XGXS 8705\n");
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_WIS_DEVAD,
			    MDIO_WIS_REG_LASI_STATUS, &val1);
			DBPRINT(sc, BXE_VERBOSE_PHY, "8705 LASI status 0x%x\n",
			    val1);

			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_WIS_DEVAD,
			    MDIO_WIS_REG_LASI_STATUS, &val1);
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "8705 LASI status 0x%x\n", val1);

			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_RX_SD,
			    &rx_sd);

			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, 1, 0xc809, &val1);
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, 1, 0xc809, &val1);

			DBPRINT(sc, BXE_VERBOSE_PHY, "8705 1.c809 val=0x%x\n",
			    val1);
			ext_phy_link_up = ((rx_sd & 0x1) && (val1 & (1 << 9)) &&
			    ((val1 & (1 << 8)) == 0));
			if (ext_phy_link_up)
				vars->line_speed = SPEED_10000;
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8706:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
			DBPRINT(sc, BXE_VERBOSE_PHY, "XGXS 8706/8726\n");
			/* Clear RX Alarm. */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_RX_ALARM,
			    &val2);
			/* Clear LASI indication. */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_LASI_STATUS, &val1);
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_LASI_STATUS, &val2);
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "8706/8726 LASI status 0x%x-->0x%x\n", val1, val2);

			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_RX_SD,
			    &rx_sd);
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PCS_DEVAD, MDIO_PCS_REG_STATUS,
			    &pcs_status);
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_AN_DEVAD,
			    MDIO_AN_REG_LINK_STATUS, &val2);
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_AN_DEVAD,
			    MDIO_AN_REG_LINK_STATUS, &val2);

			DBPRINT(sc, BXE_VERBOSE_PHY, "8706/8726 rx_sd 0x%x "
			    "pcs_status 0x%x 1Gbps link_status 0x%x\n",
			    rx_sd, pcs_status, val2);
			/*
			 * Link is up if both bit 0 of pmd_rx_sd and bit 0
			 * of pcs_status are set, or if the autoneg bit 1
			 * is set.
			 */
			ext_phy_link_up = ((rx_sd & pcs_status & 0x1) ||
			    (val2 & (1 << 1)));
			if (ext_phy_link_up) {
				if (ext_phy_type ==
				     PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726) {
					/*
					 * If transmitter is disabled,
					 * ignore false link up indication.
					 */
					bxe_cl45_read(sc, params->port,
					    ext_phy_type, ext_phy_addr,
					    MDIO_PMA_DEVAD,
					    MDIO_PMA_REG_PHY_IDENTIFIER,
					    &val1);
					if (val1 & (1<<15)) {
						DBPRINT(sc, BXE_VERBOSE_PHY,
						    "Tx is disabled\n");
						ext_phy_link_up = 0;
						break;
					}
				}
				if (val2 & (1 << 1))
					vars->line_speed = SPEED_1000;
				else
					vars->line_speed = SPEED_10000;
			}
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
			link_status = 0;
			/* Check the LASI. */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_RX_ALARM, &rx_alarm_status);

			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "8727 RX_ALARM_STATUS 0x%x\n", rx_alarm_status);

			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_LASI_STATUS, &val1);

			DBPRINT(sc, BXE_VERBOSE_PHY, "8727 LASI status 0x%x\n",
			    val1);

			/* Clear MSG-OUT */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_M8051_MSGOUT_REG, &val1);

			/*
			 * If a module is present and there is need to check
			 * for over current.
			 */
			if (!(params->feature_config_flags &
			    FEATURE_CONFIG_BCM8727_NOC) &&
			    !(rx_alarm_status & (1<<5))) {
				/* Check over-current using 8727 GPIO0 input. */
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_8727_GPIO_CTRL, &val1);

				if ((val1 & (1 << 8)) == 0) {
					DBPRINT(sc, BXE_VERBOSE_PHY,
			"8727 Power fault has been detected on port %d\n",
					    params->port);
					printf(
			"Error: Power fault on %s Port %d has been detected "
			"and the power to that SFP+ module has been removed "
			"to prevent failure of the card. Please remove the "
			"SFP+ module and restart the system to clear this "
			"error.\n",
					    sc->name, params->port);
					/*
					 * Disable all RX_ALARMs except for
					 * mod_abs.
					 */
					bxe_cl45_write(sc, params->port,
					    ext_phy_type, ext_phy_addr,
					    MDIO_PMA_DEVAD,
					    MDIO_PMA_REG_RX_ALARM_CTRL,
					    (1 << 5));

					bxe_cl45_read(sc, params->port,
					    ext_phy_type, ext_phy_addr,
					    MDIO_PMA_DEVAD,
					    MDIO_PMA_REG_PHY_IDENTIFIER, &val1);
					/* Wait for module_absent_event. */
					val1 |= (1 << 8);
					bxe_cl45_write(sc, params->port,
					    ext_phy_type, ext_phy_addr,
					    MDIO_PMA_DEVAD,
					    MDIO_PMA_REG_PHY_IDENTIFIER, val1);
					/* Clear RX alarm. */
					bxe_cl45_read(sc, params->port,
					    ext_phy_type, ext_phy_addr,
					    MDIO_PMA_DEVAD,
					    MDIO_PMA_REG_RX_ALARM,
					    &rx_alarm_status);
					break;
				}
			} /* Over current check */

			/* When module absent bit is set, check module. */
			if (rx_alarm_status & (1 << 5)) {
				bxe_8727_handle_mod_abs(params);
				/*
				 * Enable all mod_abs and link detection bits.
				 */
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_RX_ALARM_CTRL,
				    ((1 << 5) | (1 << 2)));
			}

			/*
			 * If transmitter is disabled, ignore false link
			 * up indication.
			 */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_PHY_IDENTIFIER, &val1);
			if (val1 & (1 << 15)) {
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "Tx is disabled\n");
				ext_phy_link_up = 0;
				break;
			}

			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_8073_SPEED_LINK_STATUS, &link_status);

			/*
			 * Bits 0..2 --> speed detected,
			 * bits 13..15--> link is down
			 */
			if ((link_status & (1 << 2)) &&
			    (!(link_status & (1 << 15)))) {
				ext_phy_link_up = 1;
				vars->line_speed = SPEED_10000;
			} else if ((link_status & (1 << 0)) &&
			    (!(link_status & (1 << 13)))) {
				ext_phy_link_up = 1;
				vars->line_speed = SPEED_1000;
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "port %x: External link up in 1G\n",
				    params->port);
			} else {
				ext_phy_link_up = 0;
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "port %x: External link is down\n",
				    params->port);
			}
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
			link_status = 0;
			an1000_status = 0;
			if (ext_phy_type ==
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072) {
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PCS_DEVAD,
				    MDIO_PCS_REG_LASI_STATUS, &val1);
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PCS_DEVAD,
				    MDIO_PCS_REG_LASI_STATUS, &val2);
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "870x LASI status 0x%x->0x%x\n", val1,
				    val2);
			} else {
				/*
				 * In 8073, port1 is directed through emac0 and
				 * port0 is directed through emac1.
				 */
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_LASI_STATUS, &val1);

				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "8703 LASI status 0x%x\n", val1);
			}

			/* Clear the interrupt LASI status register. */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PCS_DEVAD,
			    MDIO_PCS_REG_STATUS, &val2);
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PCS_DEVAD,
			    MDIO_PCS_REG_STATUS, &val1);
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "807x PCS status 0x%x->0x%x\n", val2, val1);
			/* Clear MSG-OUT. */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_M8051_MSGOUT_REG, &val1);

			/* Check the LASI. */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_RX_ALARM,
			    &val2);

			DBPRINT(sc, BXE_VERBOSE_PHY, "KR 0x9003 0x%x\n", val2);

			/* Check the link status. */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PCS_DEVAD, MDIO_PCS_REG_STATUS,
			    &val2);
			DBPRINT(sc, BXE_VERBOSE_PHY, "KR PCS status 0x%x\n",
			    val2);

			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_STATUS,
			    &val2);
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_STATUS,
			    &val1);
			ext_phy_link_up = ((val1 & 4) == 4);
			DBPRINT(sc, BXE_VERBOSE_PHY, "PMA_REG_STATUS=0x%x\n",
			    val1);
			if (ext_phy_type ==
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073) {
				if (ext_phy_link_up &&
				    ((params->req_line_speed != SPEED_10000))) {
					if (bxe_bcm8073_xaui_wa(params) != 0) {
						ext_phy_link_up = 0;
						break;
					}
				}
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_LINK_STATUS, &an1000_status);
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_LINK_STATUS, &an1000_status);

				/* Check the link status on 1.1.2. */
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_STATUS, &val2);
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_STATUS, &val1);
				DBPRINT(sc, BXE_VERBOSE_PHY,
			"KR PMA status 0x%x->0x%x, an_link_status=0x%x\n",
				    val2, val1, an1000_status);

				ext_phy_link_up = (((val1 & 4) == 4) ||
				    (an1000_status & (1 << 1)));
				if (ext_phy_link_up &&
				    bxe_8073_is_snr_needed(params)) {
					/*
					 * The SNR will improve about 2dbby
					 * changing the BW and FEE main tap.
					 *
					 * The 1st write to change FFE main
					 * tap is set before restart AN.
					 * Change PLL Bandwidth in EDC
					 * register.
					 */
					bxe_cl45_write(sc, port, ext_phy_type,
					    ext_phy_addr, MDIO_PMA_DEVAD,
					    MDIO_PMA_REG_PLL_BANDWIDTH, 0x26BC);

					/*
					 * Change CDR Bandwidth in EDC register.
					  */
					bxe_cl45_write(sc, port, ext_phy_type,
					    ext_phy_addr, MDIO_PMA_DEVAD,
					    MDIO_PMA_REG_CDR_BANDWIDTH, 0x0333);
				}
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_PMA_DEVAD,
				    MDIO_PMA_REG_8073_SPEED_LINK_STATUS,
				    &link_status);

				/*
				 * Bits 0..2 --> speed detected,
				 * bits 13..15--> link is down
				 */
				if ((link_status & (1<<2)) &&
				    (!(link_status & (1<<15)))) {
					ext_phy_link_up = 1;
					vars->line_speed = SPEED_10000;
					DBPRINT(sc, BXE_VERBOSE_PHY,
				"port %x: External link up in 10G\n",
					    params->port);
				} else if ((link_status & (1<<1)) &&
				    (!(link_status & (1<<14)))) {
					ext_phy_link_up = 1;
					vars->line_speed = SPEED_2500;
					DBPRINT(sc, BXE_VERBOSE_PHY,
				"port %x: External link up in 2.5G\n",
					    params->port);
				} else if ((link_status & (1<<0)) &&
				    (!(link_status & (1<<13)))) {
					ext_phy_link_up = 1;
					vars->line_speed = SPEED_1000;
					DBPRINT(sc, BXE_VERBOSE_PHY,
				"port %x: External link up in 1G\n",
					    params->port);
				} else {
					ext_phy_link_up = 0;
					DBPRINT(sc, BXE_VERBOSE_PHY,
				"port %x: External link is down\n",
					    params->port);
				}
			} else {
				/* See if 1G link is up for the 8072. */
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_LINK_STATUS, &an1000_status);
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_LINK_STATUS, &an1000_status);
				if (an1000_status & (1 << 1)) {
					ext_phy_link_up = 1;
					vars->line_speed = SPEED_1000;
					DBPRINT(sc, BXE_VERBOSE_PHY,
				"port %x: External link up in 1G\n",
					    params->port);
				} else if (ext_phy_link_up) {
					ext_phy_link_up = 1;
					vars->line_speed = SPEED_10000;
					DBPRINT(sc, BXE_VERBOSE_PHY,
				"port %x: External link up in 10G\n",
					    params->port);
				}
			}
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_LASI_STATUS, &val2);
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_LASI_STATUS, &val1);
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "10G-base-T LASI status 0x%x->0x%x\n", val2, val1);
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_STATUS, &val2);
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_STATUS, &val1);
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "10G-base-T PMA status 0x%x->0x%x\n", val2, val1);
			ext_phy_link_up = ((val1 & 4) == 4);
			/*
			 * if link is up print the AN outcome of the
			 * SFX7101 PHY.
			 */
			if (ext_phy_link_up) {
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_MASTER_STATUS, &val2);
				vars->line_speed = SPEED_10000;
				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "SFX7101 AN status 0x%x->Master=%x\n",
				    val2, (val2 & (1 << 14)));
			}
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM84823:
			/* Check 10G-BaseT link status. */
			/* Check PMD signal ok. */
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_AN_DEVAD, 0xFFFA, &val1);
			bxe_cl45_read(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD,
			    MDIO_PMA_REG_8481_PMD_SIGNAL, &val2);
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "PMD_SIGNAL 1.a811 = 0x%x\n", val2);

			/* Check link 10G. */
			if (val2 & (1 << 11)) {
				vars->line_speed = SPEED_10000;
				ext_phy_link_up = 1;
				bxe_8481_set_10G_led_mode(params, ext_phy_type,
				    ext_phy_addr);
			} else { /* Check Legacy speed link */
				/*
				 * Enable expansion register 0x42
				 * (Operation mode status).
				 */
				bxe_cl45_write(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_8481_EXPANSION_REG_ACCESS,
				    0xf42);

				/* Get legacy speed operation status. */
				bxe_cl45_read(sc, params->port, ext_phy_type,
				    ext_phy_addr, MDIO_AN_DEVAD,
				    MDIO_AN_REG_8481_EXPANSION_REG_RD_RW,
				    &legacy_status);

				DBPRINT(sc, BXE_VERBOSE_PHY,
				    "Legacy speed status = 0x%x\n",
				    legacy_status);
				ext_phy_link_up = ((legacy_status &
				    (1 << 11)) == (1<<11));
				if (ext_phy_link_up) {
					legacy_speed = (legacy_status &
					    (3 << 9));
					if (legacy_speed == (0 << 9))
						vars->line_speed = SPEED_10;
					else if (legacy_speed == (1 << 9))
						vars->line_speed = SPEED_100;
					else if (legacy_speed == (2 << 9))
						vars->line_speed = SPEED_1000;
					else /* Should not happen */
						vars->line_speed = 0;
					if (legacy_status & (1 << 8))
						vars->duplex = DUPLEX_FULL;
					else
						vars->duplex = DUPLEX_HALF;

					DBPRINT(sc, BXE_VERBOSE_PHY,
				"Link is up in %dMbps, is_duplex_full = %d\n",
					    vars->line_speed,
					    (vars->duplex == DUPLEX_FULL));
					bxe_8481_set_legacy_led_mode(params,
					    ext_phy_type, ext_phy_addr);
				}
			}
			break;
		default:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "BAD XGXS ext_phy_config 0x%x\n",
			    params->ext_phy_config);
			ext_phy_link_up = 0;
			break;
		}
		/* Set SGMII mode for external phy */
		if (ext_phy_type != PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT) {
			if (vars->line_speed < SPEED_1000)
				vars->phy_flags |= PHY_SGMII_FLAG;
			else
				vars->phy_flags &= ~PHY_SGMII_FLAG;
		}

	} else { /* SerDes */
		ext_phy_type = SERDES_EXT_PHY_TYPE(params->ext_phy_config);
		switch (ext_phy_type) {
		case PORT_HW_CFG_SERDES_EXT_PHY_TYPE_DIRECT:
			DBPRINT(sc, BXE_VERBOSE_PHY, "SerDes Direct\n");
			ext_phy_link_up = 1;
			break;
		case PORT_HW_CFG_SERDES_EXT_PHY_TYPE_BCM5482:
			DBPRINT(sc, BXE_VERBOSE_PHY, "SerDes 5482\n");
			ext_phy_link_up = 1;
			break;
		default:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "BAD SerDes ext_phy_config 0x%x\n",
			    params->ext_phy_config);
			ext_phy_link_up = 0;
			break;
		}
	}

	return (ext_phy_link_up);
}

static void
bxe_link_int_enable(struct link_params *params)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type, mask;
	uint8_t port;

	sc = params->sc;
	port = params->port;

	/* Setting the status to report on link up for either XGXS or SerDes. */

	if (params->switch_cfg == SWITCH_CFG_10G) {
		mask = (NIG_MASK_XGXS0_LINK10G | NIG_MASK_XGXS0_LINK_STATUS);
		DBPRINT(sc, BXE_VERBOSE_PHY, "enabled XGXS interrupt\n");
		ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);
		if ((ext_phy_type != PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT) &&
		    (ext_phy_type != PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE) &&
		    (ext_phy_type != PORT_HW_CFG_XGXS_EXT_PHY_TYPE_NOT_CONN)) {
			mask |= NIG_MASK_MI_INT;
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "enabled external phy int\n");
		}
	} else { /* SerDes */
		mask = NIG_MASK_SERDES0_LINK_STATUS;
		DBPRINT(sc, BXE_VERBOSE_PHY, "enabled SerDes interrupt\n");
		ext_phy_type = SERDES_EXT_PHY_TYPE(params->ext_phy_config);
		if ((ext_phy_type != PORT_HW_CFG_SERDES_EXT_PHY_TYPE_DIRECT) &&
		    (ext_phy_type !=
		    PORT_HW_CFG_SERDES_EXT_PHY_TYPE_NOT_CONN)) {
			mask |= NIG_MASK_MI_INT;
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "enabled external phy int\n");
		}
	}
	bxe_bits_en(sc, NIG_REG_MASK_INTERRUPT_PORT0 + port * 4, mask);

	DBPRINT(sc, BXE_VERBOSE_PHY, "port %x, is_xgxs %x, int_status 0x%x\n",
	    port, (params->switch_cfg == SWITCH_CFG_10G), REG_RD(sc,
	    NIG_REG_STATUS_INTERRUPT_PORT0 + port * 4));
	DBPRINT(sc, BXE_VERBOSE_PHY, " int_mask 0x%x, MI_INT %x, SERDES_LINK %x\n",
	    REG_RD(sc, NIG_REG_MASK_INTERRUPT_PORT0 + port * 4),
	    REG_RD(sc, NIG_REG_EMAC0_STATUS_MISC_MI_INT + port * 0x18),
	    REG_RD(sc, NIG_REG_SERDES0_STATUS_LINK_STATUS + port * 0x3c));
	DBPRINT(sc, BXE_VERBOSE_PHY, " 10G %x, XGXS_LINK %x\n",
	    REG_RD(sc, NIG_REG_XGXS0_STATUS_LINK10G + port * 0x68),
	    REG_RD(sc, NIG_REG_XGXS0_STATUS_LINK_STATUS + port * 0x68));
}

static void
bxe_8481_rearm_latch_signal(struct bxe_softc *sc, uint8_t port,
uint8_t is_mi_int)
{
	uint32_t latch_status, is_mi_int_status;

	latch_status = 0;

	/*
	 * Disable the MI INT ( external phy int )
	 * by writing 1 to the status register. Link down indication
	 * is high-active-signal, so in this case we need to write
	 * the status to clear the XOR.
	 */
	/* Read Latched signals. */
	latch_status = REG_RD(sc, NIG_REG_LATCH_STATUS_0 + port * 8);
	is_mi_int_status = REG_RD(sc, NIG_REG_STATUS_INTERRUPT_PORT0 +
	    port * 4);
	DBPRINT(sc, BXE_VERBOSE_PHY,
	    "original_signal = 0x%x, nig_status = 0x%x, latch_status = 0x%x\n",
	    is_mi_int, is_mi_int_status, latch_status);
	/* Handle only those with latched-signal=up. */
	if (latch_status & 1) {
		/* For all latched-signal=up,Write original_signal to status. */
		if (is_mi_int)
			bxe_bits_en(sc, NIG_REG_STATUS_INTERRUPT_PORT0 +
			    port * 4, NIG_STATUS_EMAC0_MI_INT);
		else
			bxe_bits_dis(sc, NIG_REG_STATUS_INTERRUPT_PORT0 +
			    port * 4, NIG_STATUS_EMAC0_MI_INT);
		/* For all latched-signal=up : Re-Arm Latch signals. */
		REG_WR(sc, NIG_REG_LATCH_STATUS_0 + port * 8,
		    (latch_status & 0xfffe) | (latch_status & 1));
	}
}

/*
 * Link management
 */
static void
bxe_link_int_ack(struct link_params *params, struct link_vars *vars,
    uint8_t is_10g, uint8_t is_mi_int)
{
	struct bxe_softc *sc;
	uint32_t ser_lane;
	uint8_t port;

	sc = params->sc;
	port = params->port;

	/*
	 * First reset all status, we assume only one line will be
	 * change at a time.
	 */
	bxe_bits_dis(sc, NIG_REG_STATUS_INTERRUPT_PORT0 + port * 4,
	    (NIG_STATUS_XGXS0_LINK10G | NIG_STATUS_XGXS0_LINK_STATUS |
	     NIG_STATUS_SERDES0_LINK_STATUS));
	if ((XGXS_EXT_PHY_TYPE(params->ext_phy_config) ==
	    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481) ||
	    (XGXS_EXT_PHY_TYPE(params->ext_phy_config) ==
	    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM84823))
		bxe_8481_rearm_latch_signal(sc, port, is_mi_int);
	if (vars->phy_link_up) {
		if (is_10g) {
			/*
			 * Disable the 10G link interrupt by writing 1 to
			 * the status register.
			 */
			DBPRINT(sc, BXE_VERBOSE_PHY, "10G XGXS phy link up\n");
			bxe_bits_en(sc, NIG_REG_STATUS_INTERRUPT_PORT0 +
			    port * 4, NIG_STATUS_XGXS0_LINK10G);
		} else if (params->switch_cfg == SWITCH_CFG_10G) {
			/*
			 * Disable the link interrupt by writing 1 to
			 * the relevant lane in the status register.
			 */
			ser_lane = ((params->lane_config &
			    PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MASK) >>
			    PORT_HW_CFG_LANE_SWAP_CFG_MASTER_SHIFT);

			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "%d speed XGXS phy link up\n", vars->line_speed);
			bxe_bits_en(sc, NIG_REG_STATUS_INTERRUPT_PORT0 +
			    port * 4, ((1 << ser_lane) <<
			    NIG_STATUS_XGXS0_LINK_STATUS_SIZE));
		} else { /* SerDes */
			DBPRINT(sc, BXE_VERBOSE_PHY, "SerDes phy link up\n");
			/*
			 * Disable the link interrupt by writing 1 to
			 * the status register.
			 */
			bxe_bits_en(sc, NIG_REG_STATUS_INTERRUPT_PORT0 +
			    port * 4, NIG_STATUS_SERDES0_LINK_STATUS);
		}

	} else { /* link_down */
	}
}

static uint8_t
bxe_format_ver(uint32_t num, uint8_t *str, uint16_t len)
{
	uint32_t mask;
	uint8_t *str_ptr;
	uint8_t digit, shift;

	str_ptr = str;
	mask = 0xf0000000;
	shift = 8 * 4;
	if (len < 10) {
		/* Need more than 10 chars for this format. */
		*str_ptr = '\0';
		return (-EINVAL);
	}
	while (shift > 0) {
		shift -= 4;
		digit = ((num & mask) >> shift);
		if (digit < 0xa)
			*str_ptr = digit + '0';
		else
			*str_ptr = digit - 0xa + 'a';
		str_ptr++;
		mask = mask >> 4;
		if (shift == 4*4) {
			*str_ptr = ':';
			str_ptr++;
		}
	}
	*str_ptr = '\0';
	return (0);
}

uint8_t
bxe_get_ext_phy_fw_version(struct link_params *params, uint8_t driver_loaded,
    uint8_t *version, uint16_t len)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type, spirom_ver;
	uint8_t status;

	sc = params->sc;
	if (version == NULL || params == NULL)
		return (-EINVAL);

	spirom_ver = REG_RD(sc, params->shmem_base +
	    offsetof(struct shmem_region,
	    port_mb[params->port].ext_phy_fw_version));

	status = 0;
	/* Reset the returned value to zero. */
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);
	switch (ext_phy_type) {
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
		if (len < 5)
			return (-EINVAL);
		version[0] = (spirom_ver & 0xFF);
		version[1] = (spirom_ver & 0xFF00) >> 8;
		version[2] = (spirom_ver & 0xFF0000) >> 16;
		version[3] = (spirom_ver & 0xFF000000) >> 24;
		version[4] = '\0';
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8706:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
		status = bxe_format_ver(spirom_ver, version, len);
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM84823:
		spirom_ver = ((spirom_ver & 0xF80) >> 7) << 16 |
		    (spirom_ver & 0x7F);
		status = bxe_format_ver(spirom_ver, version, len);
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705:
		version[0] = '\0';
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE:
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "bxe_get_ext_phy_fw_version: type is FAILURE!\n");
		status = -EINVAL;
		break;
	default:
		break;
	}
	return (status);
}

static void
bxe_set_xgxs_loopback(struct link_params *params, struct link_vars *vars,
    uint8_t is_10g)
{
	struct bxe_softc *sc;
	uint32_t md_devad;
	uint16_t mii_control;
	uint8_t port;

	sc = params->sc;
	port = params->port;

	if (is_10g) {
		DBPRINT(sc, BXE_VERBOSE_PHY, "XGXS 10G loopback enable\n");

		/* Change the uni_phy_addr in the nig. */
		md_devad = REG_RD(sc, (NIG_REG_XGXS0_CTRL_MD_DEVAD +
		    port * 0x18));

		REG_WR(sc, NIG_REG_XGXS0_CTRL_MD_DEVAD + port * 0x18, 0x5);

		bxe_cl45_write(sc, port, 0, params->phy_addr, 5,
		    (MDIO_REG_BANK_AER_BLOCK + (MDIO_AER_BLOCK_AER_REG & 0xf)),
		    0x2800);

		bxe_cl45_write(sc, port, 0, params->phy_addr, 5,
		    (MDIO_REG_BANK_CL73_IEEEB0 +
		     (MDIO_CL73_IEEEB0_CL73_AN_CONTROL & 0xf)), 0x6041);
		msleep(200);
		/* Set aer mmd back. */
		bxe_set_aer_mmd(params, vars);

		/* and md_devad */
		REG_WR(sc, NIG_REG_XGXS0_CTRL_MD_DEVAD + port * 0x18, md_devad);
	} else {
		DBPRINT(sc, BXE_VERBOSE_PHY, "XGXS 1G loopback enable\n");

		CL45_RD_OVER_CL22(sc, port, params->phy_addr,
		    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_MII_CONTROL,
		    &mii_control);

		CL45_WR_OVER_CL22(sc, port, params->phy_addr,
		    MDIO_REG_BANK_COMBO_IEEE0, MDIO_COMBO_IEEE0_MII_CONTROL,
		    (mii_control | MDIO_COMBO_IEEO_MII_CONTROL_LOOPBACK));
	}
}

static void
bxe_ext_phy_loopback(struct link_params *params)
{
	struct bxe_softc *sc;
	uint8_t ext_phy_addr;
	uint32_t ext_phy_type;

	sc = params->sc;
	if (params->switch_cfg == SWITCH_CFG_10G) {
		ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);
		ext_phy_addr = XGXS_EXT_PHY_ADDR(params->ext_phy_config);
		/* CL37 Autoneg Enabled */
		switch (ext_phy_type) {
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_NOT_CONN:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "ext_phy_loopback: We should not get here\n");
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "ext_phy_loopback: 8705\n");
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8706:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "ext_phy_loopback: 8706\n");
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "PMA/PMD ext_phy_loopback: 8726\n");
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_CTRL,
			    0x0001);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101:
			/* SFX7101_XGXS_TEST1 */
			bxe_cl45_write(sc, params->port, ext_phy_type,
			    ext_phy_addr, MDIO_XS_DEVAD,
			    MDIO_XS_SFX7101_XGXS_TEST1, 0x100);
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "ext_phy_loopback: set ext phy loopback\n");
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
			break;
		} /* switch external PHY type */
	} else {
		/* serdes */
		ext_phy_type = SERDES_EXT_PHY_TYPE(params->ext_phy_config);
		ext_phy_addr = (params->ext_phy_config &
		    PORT_HW_CFG_SERDES_EXT_PHY_ADDR_MASK) >>
		    PORT_HW_CFG_SERDES_EXT_PHY_ADDR_SHIFT;
	}
}

/* Override the led value of the requsted led. */
uint8_t bxe_override_led_value(struct bxe_softc *sc, uint8_t port,
    uint32_t led_idx, uint32_t value)
{
	uint32_t reg_val;
	uint32_t emac_base;

	/* If port 0 then use EMAC0, else use EMAC1. */
	emac_base = (port) ? GRCBASE_EMAC1 : GRCBASE_EMAC0;

	DBPRINT(sc, BXE_VERBOSE_PHY,
	    "bxe_override_led_value() port %x led_idx %d value %d\n", port,
	    led_idx, value);

	switch (led_idx) {
	case 0: /* 10MB led */
		/*
		 * Read the current value of the LED register in the EMAC block.
		 */
		reg_val = REG_RD(sc, emac_base + EMAC_REG_EMAC_LED);
		/* Set the OVERRIDE bit to 1. */
		reg_val |= EMAC_LED_OVERRIDE;
		/*
		 * If value is 1, set the 10M_OVERRIDE bit, otherwise reset it.
		 */
		reg_val = (value == 1) ? (reg_val | EMAC_LED_10MB_OVERRIDE) :
		    (reg_val & ~EMAC_LED_10MB_OVERRIDE);
		REG_WR(sc, emac_base + EMAC_REG_EMAC_LED, reg_val);
		break;
	case 1: /*100MB led    */
		/*
		 * Read the current value of the LED register in the EMAC block.
		 */
		reg_val = REG_RD(sc, emac_base + EMAC_REG_EMAC_LED);
		/* Set the OVERRIDE bit to 1. */
		reg_val |= EMAC_LED_OVERRIDE;
		/*
		 * If value is 1, set the 100M_OVERRIDE bit, otherwise reset it.
		 */
		reg_val = (value == 1) ? (reg_val | EMAC_LED_100MB_OVERRIDE) :
		    (reg_val & ~EMAC_LED_100MB_OVERRIDE);
		REG_WR(sc, emac_base + EMAC_REG_EMAC_LED, reg_val);
		break;
	case 2: /* 1000MB led */
		/*
		 * Read the current value of the LED register in the EMAC block.
		 */
		reg_val = REG_RD(sc, emac_base + EMAC_REG_EMAC_LED);
		/* Set the OVERRIDE bit to 1. */
		reg_val |= EMAC_LED_OVERRIDE;
		/*
		 * If value is 1, set the 1000M_OVERRIDE bit, otherwise reset
		 * it.
		 */
		reg_val = (value == 1) ? (reg_val | EMAC_LED_1000MB_OVERRIDE) :
		    (reg_val & ~EMAC_LED_1000MB_OVERRIDE);
		REG_WR(sc, emac_base + EMAC_REG_EMAC_LED, reg_val);
		break;
	case 3: /* 2500MB led */
		/*
		 * Read the current value of the LED register in the EMAC block.
		 */
		reg_val = REG_RD(sc, emac_base + EMAC_REG_EMAC_LED);
		/* Set the OVERRIDE bit to 1. */
		reg_val |= EMAC_LED_OVERRIDE;
		/*
		 * If value is 1, set the 2500M_OVERRIDE bit, otherwise reset
		 * it.
		 */
		reg_val = (value == 1) ? (reg_val | EMAC_LED_2500MB_OVERRIDE) :
		    (reg_val & ~EMAC_LED_2500MB_OVERRIDE);
		REG_WR(sc, emac_base + EMAC_REG_EMAC_LED, reg_val);
		break;
	case 4: /*10G led */
		if (port == 0)
			REG_WR(sc, NIG_REG_LED_10G_P0, value);
		else
			REG_WR(sc, NIG_REG_LED_10G_P1, value);
		break;
	case 5: /* TRAFFIC led */
		/* Find if the traffic control is via BMAC or EMAC. */
		if (port == 0)
			reg_val = REG_RD(sc, NIG_REG_NIG_EMAC0_EN);
		else
			reg_val = REG_RD(sc, NIG_REG_NIG_EMAC1_EN);

		/*  Override the traffic led in the EMAC. */
		if (reg_val == 1) {
			/*
			 * Read the current value of the LED register in
			 * the EMAC block.
			 */
			reg_val = REG_RD(sc, emac_base + EMAC_REG_EMAC_LED);
			/* Set the TRAFFIC_OVERRIDE bit to 1. */
			reg_val |= EMAC_LED_OVERRIDE;
			/*
			 * If value is 1, set the TRAFFIC bit, otherwise reset
			 * it.
			 */
			reg_val = (value == 1) ? (reg_val | EMAC_LED_TRAFFIC) :
			    (reg_val & ~EMAC_LED_TRAFFIC);
			REG_WR(sc, emac_base + EMAC_REG_EMAC_LED, reg_val);
		} else {
			/* Override the traffic led in the BMAC. */
			REG_WR(sc, NIG_REG_LED_CONTROL_OVERRIDE_TRAFFIC_P0 +
			    port * 4, 1);
			REG_WR(sc, NIG_REG_LED_CONTROL_TRAFFIC_P0 + port * 4,
			    value);
		}
		break;
	default:
		DBPRINT(sc, BXE_VERBOSE_PHY,
	"bxe_override_led_value() unknown led index %d (should be 0-5)\n",
		    led_idx);
		return (-EINVAL);
	}

	return (0);
}

uint8_t
bxe_set_led(struct link_params *params, uint8_t mode, uint32_t speed)
{
	struct bxe_softc *sc;
	uint32_t emac_base, ext_phy_type, tmp;
	uint16_t hw_led_mode;
	uint8_t port, rc;

	sc = params->sc;
	port = params->port;
	hw_led_mode = params->hw_led_mode;
	rc = 0;
	emac_base = port ? GRCBASE_EMAC1 : GRCBASE_EMAC0;
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	switch (mode) {
	case LED_MODE_OFF:
		REG_WR(sc, NIG_REG_LED_10G_P0 + port * 4, 0);
		REG_WR(sc, NIG_REG_LED_MODE_P0 + port * 4,
		    SHARED_HW_CFG_LED_MAC1);
		tmp = EMAC_RD(sc, EMAC_REG_EMAC_LED);
		EMAC_WR(sc, EMAC_REG_EMAC_LED, (tmp | EMAC_LED_OVERRIDE));
		break;
	case LED_MODE_OPER:
		if (ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT) {
			REG_WR(sc, NIG_REG_LED_MODE_P0 + port * 4, 0);
			REG_WR(sc, NIG_REG_LED_10G_P0 + port * 4, 1);
		} else
			REG_WR(sc, NIG_REG_LED_MODE_P0 + port * 4, hw_led_mode);
		REG_WR(sc, NIG_REG_LED_CONTROL_OVERRIDE_TRAFFIC_P0 + port * 4,
		    0);
		/* Set blinking rate to ~15.9Hz. */
		REG_WR(sc, NIG_REG_LED_CONTROL_BLINK_RATE_P0 + port * 4,
		    LED_BLINK_RATE_VAL);
		REG_WR(sc, NIG_REG_LED_CONTROL_BLINK_RATE_ENA_P0 + port * 4, 1);
		tmp = EMAC_RD(sc, EMAC_REG_EMAC_LED);
		EMAC_WR(sc, EMAC_REG_EMAC_LED, (tmp & (~EMAC_LED_OVERRIDE)));

		if (CHIP_IS_E1(sc) && ((speed == SPEED_2500) ||
		    (speed == SPEED_1000) || (speed == SPEED_100) ||
		    (speed == SPEED_10))) {
			/*
			 * On Everest 1 Ax chip versions for speeds less than
			 * 10G LED scheme is different.
			 */
			REG_WR(sc, NIG_REG_LED_CONTROL_OVERRIDE_TRAFFIC_P0 +
			    port * 4, 1);
			REG_WR(sc, NIG_REG_LED_CONTROL_TRAFFIC_P0 + port * 4,
			    0);
			REG_WR(sc, NIG_REG_LED_CONTROL_BLINK_TRAFFIC_P0 +
			    port * 4, 1);
		}
		break;
	default:
		rc = -EINVAL;
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "%s(): Invalid led mode (%d)!\n", __FUNCTION__, mode);
		break;
	}
	return (rc);
}

uint8_t
bxe_test_link(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint16_t gp_status;

	sc = params->sc;
	gp_status = 0;
	CL45_RD_OVER_CL22(sc, params->port, params->phy_addr,
	    MDIO_REG_BANK_GP_STATUS, MDIO_GP_STATUS_TOP_AN_STATUS1, &gp_status);
	/* Link is up only if both local phy and external phy are up. */
	if ((gp_status & MDIO_GP_STATUS_TOP_AN_STATUS1_LINK_STATUS) &&
	    bxe_ext_phy_is_link_up(params, vars, 1))
		return (0);

	return (-EINVAL);
}

static uint8_t
bxe_link_initialize(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint8_t port, rc;
	uint8_t non_ext_phy;

	sc = params->sc;
	port = params->port;
	rc = 0;
	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	/* Activate the external PHY. */
	bxe_ext_phy_reset(params, vars);

	bxe_set_aer_mmd(params, vars);

	if (vars->phy_flags & PHY_XGXS_FLAG)
		bxe_set_master_ln(params);

	rc = bxe_reset_unicore(params);
	/* Reset the SerDes and wait for reset bit return low. */
	if (rc != 0)
		return (rc);

	bxe_set_aer_mmd(params, vars);

	/* Setting the masterLn_def again after the reset. */
	if (vars->phy_flags & PHY_XGXS_FLAG) {
		bxe_set_master_ln(params);
		bxe_set_swap_lanes(params);
	}

	if (vars->phy_flags & PHY_XGXS_FLAG) {
		if ((params->req_line_speed &&
		    ((params->req_line_speed == SPEED_100) ||
		    (params->req_line_speed == SPEED_10))) ||
		    (!params->req_line_speed &&
		    (params->speed_cap_mask >=
		    PORT_HW_CFG_SPEED_CAPABILITY_D0_10M_FULL) &&
		    (params->speed_cap_mask <
		    PORT_HW_CFG_SPEED_CAPABILITY_D0_1G)))
			vars->phy_flags |= PHY_SGMII_FLAG;
		else
			vars->phy_flags &= ~PHY_SGMII_FLAG;
	}
	/*
	 * In case of external phy existance, the line speed would be the
	 * line speed linked up by the external phy. In case it is direct
	 * only, then the line_speed during initialization will be equal
	 * to the req_line_speed.
	 */
	vars->line_speed = params->req_line_speed;

	bxe_calc_ieee_aneg_adv(params, &vars->ieee_fc);

	/* Init ext phy and enable link state int. */
	non_ext_phy = ((ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT) ||
	    (params->loopback_mode == LOOPBACK_XGXS_10));

	if (non_ext_phy ||
	    (ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705) ||
	    (ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8706) ||
	    (ext_phy_type == PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726) ||
	    (params->loopback_mode == LOOPBACK_EXT_PHY)) {
		if (params->req_line_speed == SPEED_AUTO_NEG)
			bxe_set_parallel_detection(params, vars->phy_flags);
		bxe_init_internal_phy(params, vars, 0);
	}

	if (!non_ext_phy)
		rc |= bxe_ext_phy_init(params, vars);

	bxe_bits_dis(sc, NIG_REG_STATUS_INTERRUPT_PORT0 + port * 4,
	    (NIG_STATUS_XGXS0_LINK10G | NIG_STATUS_XGXS0_LINK_STATUS |
	    NIG_STATUS_SERDES0_LINK_STATUS));

	return (rc);
}

uint8_t
bxe_phy_init(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint32_t val;

	sc = params->sc;
	DBPRINT(sc, BXE_VERBOSE_PHY, "Phy Initialization started\n");
	DBPRINT(sc, BXE_VERBOSE_PHY, "req_speed %d, req_flowctrl %d\n",
	    params->req_line_speed, params->req_flow_ctrl);
	vars->link_status = 0;
	vars->phy_link_up = 0;
	vars->link_up = 0;
	vars->line_speed = 0;
	vars->duplex = DUPLEX_FULL;
	vars->flow_ctrl = FLOW_CTRL_NONE;
	vars->mac_type = MAC_TYPE_NONE;

	if (params->switch_cfg ==  SWITCH_CFG_1G)
		vars->phy_flags = PHY_SERDES_FLAG;
	else
		vars->phy_flags = PHY_XGXS_FLAG;

	/* disable attentions */
	bxe_bits_dis(sc, NIG_REG_MASK_INTERRUPT_PORT0 + params->port * 4,
	    (NIG_MASK_XGXS0_LINK_STATUS | NIG_MASK_XGXS0_LINK10G |
	    NIG_MASK_SERDES0_LINK_STATUS | NIG_MASK_MI_INT));

	bxe_emac_init(params, vars);

	if (params->loopback_mode == LOOPBACK_BMAC) {
		vars->link_up = 1;
		vars->line_speed = SPEED_10000;
		vars->duplex = DUPLEX_FULL;
		vars->flow_ctrl = FLOW_CTRL_NONE;
		vars->mac_type = MAC_TYPE_BMAC;

		vars->phy_flags = PHY_XGXS_FLAG;

		bxe_phy_deassert(params, vars->phy_flags);
		/* Set bmac loopback. */
		bxe_bmac_enable(params, vars, 1);

		REG_WR(sc, NIG_REG_EGRESS_DRAIN0_MODE + params->port * 4, 0);
	} else if (params->loopback_mode == LOOPBACK_EMAC) {
		vars->link_up = 1;
		vars->line_speed = SPEED_1000;
		vars->duplex = DUPLEX_FULL;
		vars->flow_ctrl = FLOW_CTRL_NONE;
		vars->mac_type = MAC_TYPE_EMAC;

		vars->phy_flags = PHY_XGXS_FLAG;

		bxe_phy_deassert(params, vars->phy_flags);
		/* Set bmac loopback. */
		bxe_emac_enable(params, vars, 1);
		bxe_emac_program(params, vars->line_speed, vars->duplex);
		REG_WR(sc, NIG_REG_EGRESS_DRAIN0_MODE + params->port * 4, 0);
	} else if ((params->loopback_mode == LOOPBACK_XGXS_10) ||
	    (params->loopback_mode == LOOPBACK_EXT_PHY)) {
		vars->link_up = 1;
		vars->line_speed = SPEED_10000;
		vars->duplex = DUPLEX_FULL;
		vars->flow_ctrl = FLOW_CTRL_NONE;

		vars->phy_flags = PHY_XGXS_FLAG;

		val = REG_RD(sc, NIG_REG_XGXS0_CTRL_PHY_ADDR +
		    params->port * 0x18);
		params->phy_addr = (uint8_t)val;

		bxe_phy_deassert(params, vars->phy_flags);
		bxe_link_initialize(params, vars);

		vars->mac_type = MAC_TYPE_BMAC;

		bxe_bmac_enable(params, vars, 0);

		if (params->loopback_mode == LOOPBACK_XGXS_10) {
			/* Set 10G XGXS loopback. */
			bxe_set_xgxs_loopback(params, vars, 1);
		} else {
			/* Set external phy loopback. */
			bxe_ext_phy_loopback(params);
		}
		REG_WR(sc, NIG_REG_EGRESS_DRAIN0_MODE + params->port * 4, 0);
		bxe_set_led(params, LED_MODE_OPER, vars->line_speed);
	} else {
		/* No loopback. */
		bxe_phy_deassert(params, vars->phy_flags);
		switch (params->switch_cfg) {
		case SWITCH_CFG_1G:
			vars->phy_flags |= PHY_SERDES_FLAG;
			if ((params->ext_phy_config &
			    PORT_HW_CFG_SERDES_EXT_PHY_TYPE_MASK) ==
			    PORT_HW_CFG_SERDES_EXT_PHY_TYPE_BCM5482) {
				vars->phy_flags |= PHY_SGMII_FLAG;
			}

			val = REG_RD(sc, NIG_REG_SERDES0_CTRL_PHY_ADDR +
			    params->port * 0x10);
			params->phy_addr = (uint8_t)val;
			break;
		case SWITCH_CFG_10G:
			vars->phy_flags |= PHY_XGXS_FLAG;
			val = REG_RD(sc, NIG_REG_XGXS0_CTRL_PHY_ADDR +
			    params->port * 0x18);
			params->phy_addr = (uint8_t)val;
			break;
		default:
			DBPRINT(sc, BXE_VERBOSE_PHY, "Invalid switch_cfg\n");
			return (-EINVAL);
		}
		DBPRINT(sc, BXE_VERBOSE_PHY, "Phy address = 0x%x\n",
		    params->phy_addr);

		bxe_link_initialize(params, vars);
		msleep(30);
		bxe_link_int_enable(params);
	}
	return (0);
}

static void
bxe_8726_reset_phy(struct bxe_softc *sc, uint8_t port, uint8_t ext_phy_addr)
{

	DBPRINT(sc, BXE_VERBOSE_PHY, "bxe_8726_reset_phy port %d\n", port);

	/* Set serial boot control for external load. */
	bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726,
	    ext_phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_GEN_CTRL, 0x0001);
}

uint8_t
bxe_link_reset(struct link_params *params, struct link_vars *vars,
    uint8_t reset_ext_phy)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_config, ext_phy_type, val;
	uint8_t ext_phy_addr, port;

	sc = params->sc;
	ext_phy_config = params->ext_phy_config;
	port = params->port;
	ext_phy_type = XGXS_EXT_PHY_TYPE(ext_phy_config);
	val = REG_RD(sc, params->shmem_base + offsetof(struct shmem_region,
	    dev_info.port_feature_config[params->port].config));

	DBPRINT(sc, BXE_INFO, "%s(): Resetting port %d link.\n",
	    __FUNCTION__, port);

	/* Disable attentions. */
	vars->link_status = 0;
	bxe_update_mng(params, vars->link_status);
	bxe_bits_dis(sc, NIG_REG_MASK_INTERRUPT_PORT0 + port * 4,
	    (NIG_MASK_XGXS0_LINK_STATUS | NIG_MASK_XGXS0_LINK10G |
	    NIG_MASK_SERDES0_LINK_STATUS | NIG_MASK_MI_INT));

	/* Activate nig drain. */
	REG_WR(sc, NIG_REG_EGRESS_DRAIN0_MODE + port * 4, 1);

	/* Disable nig egress interface. */
	REG_WR(sc, NIG_REG_BMAC0_OUT_EN + port * 4, 0);
	REG_WR(sc, NIG_REG_EGRESS_EMAC0_OUT_EN + port * 4, 0);

	/* Stop BigMac rx. */
	bxe_bmac_rx_disable(sc, params->chip_id, port);

	/* Disable emac. */
	REG_WR(sc, NIG_REG_NIG_EMAC0_EN + port * 4, 0);

	msleep(10);
	/* The PHY reset is controled by GPIO 1 Hold it as vars low. */
	 /* Clear link led. */
	bxe_set_led(params, LED_MODE_OFF, 0);
	if (reset_ext_phy) {
		switch (ext_phy_type) {
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT:
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072:
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
			/* Disable Transmitter */
			ext_phy_addr =
			    XGXS_EXT_PHY_ADDR(params->ext_phy_config);
			if ((val & PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_MASK) ==
			    PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_DISABLE_TX_LASER)
				bxe_sfp_set_transmitter(sc, port,
				    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
				    ext_phy_addr, 0);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "Setting 8073 port %d into low power mode\n",
			    port);
			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_2,
			    MISC_REGISTERS_GPIO_OUTPUT_LOW, port);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
			ext_phy_addr =
			    XGXS_EXT_PHY_ADDR(params->ext_phy_config);
			/* Set soft reset. */
			bxe_8726_reset_phy(sc, params->port, ext_phy_addr);
			break;
		case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM84823:
			ext_phy_addr =
			    XGXS_EXT_PHY_ADDR(params->ext_phy_config);
			bxe_cl45_write(sc, port,
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481, ext_phy_addr,
			    MDIO_AN_DEVAD, MDIO_AN_REG_CTRL, 0x0000);
			bxe_cl45_write(sc, port,
			    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481, ext_phy_addr,
			    MDIO_PMA_DEVAD, MDIO_PMA_REG_CTRL, 1);
			break;
		default:
			/* HW reset */
			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_1,
			    MISC_REGISTERS_GPIO_OUTPUT_LOW, port);
			bxe_set_gpio(sc, MISC_REGISTERS_GPIO_2,
			    MISC_REGISTERS_GPIO_OUTPUT_LOW, port);
			DBPRINT(sc, BXE_VERBOSE_PHY, "reset external PHY\n");
		}
	}
	/* Reset the SerDes/XGXS. */
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_3_CLEAR,
	    (0x1ff << (port * 16)));

	/* Reset BigMac. */
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_CLEAR,
	    (MISC_REGISTERS_RESET_REG_2_RST_BMAC0 << port));

	/* Disable nig ingress interface. */
	REG_WR(sc, NIG_REG_BMAC0_IN_EN + port * 4, 0);
	REG_WR(sc, NIG_REG_EMAC0_IN_EN + port * 4, 0);
	REG_WR(sc, NIG_REG_BMAC0_OUT_EN + port * 4, 0);
	REG_WR(sc, NIG_REG_EGRESS_EMAC0_OUT_EN + port * 4, 0);
	vars->link_up = 0;
	return (0);
}

static uint8_t
bxe_update_link_down(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint8_t port;

	sc = params->sc;
	port = params->port;

	DBPRINT(sc, BXE_INFO, "Port %x: Link is down\n", port);
	bxe_set_led(params, LED_MODE_OFF, 0);

	/* Indicate no mac active. */
	vars->mac_type = MAC_TYPE_NONE;

	/* Update shared memory. */
	vars->link_status = 0;
	vars->line_speed = 0;
	bxe_update_mng(params, vars->link_status);

	/* Activate nig drain. */
	REG_WR(sc, NIG_REG_EGRESS_DRAIN0_MODE + port * 4, 1);

	/* Disable emac. */
	REG_WR(sc, NIG_REG_NIG_EMAC0_EN + port * 4, 0);

	msleep(10);

	/* Reset BigMac. */
	bxe_bmac_rx_disable(sc, params->chip_id, params->port);
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_CLEAR,
	    (MISC_REGISTERS_RESET_REG_2_RST_BMAC0 << port));
	return (0);
}

static uint8_t
bxe_update_link_up(struct link_params *params, struct link_vars *vars,
    uint8_t link_10g, uint32_t gp_status)
{
	struct bxe_softc *sc;
	uint8_t port, rc;

	sc = params->sc;
	port = params->port;
	rc = 0;

	vars->link_status |= LINK_STATUS_LINK_UP;
	if (link_10g) {
		bxe_bmac_enable(params, vars, 0);
		bxe_set_led(params, LED_MODE_OPER, SPEED_10000);
	} else {
		rc = bxe_emac_program(params, vars->line_speed, vars->duplex);
		bxe_emac_enable(params, vars, 0);
		/* AN complete? */
		if (gp_status & MDIO_AN_CL73_OR_37_COMPLETE) {
			if (!(vars->phy_flags & PHY_SGMII_FLAG))
				bxe_set_gmii_tx_driver(params);
		}
	}

	/* PBF - link up */
	rc |= bxe_pbf_update(params, vars->flow_ctrl, vars->line_speed);

	/* Disable drain. */
	REG_WR(sc, NIG_REG_EGRESS_DRAIN0_MODE + port * 4, 0);

	/* Update shared memory. */
	bxe_update_mng(params, vars->link_status);
	msleep(20);
	return (rc);
}

/*
 * This function should called upon link interrupt.
 * In case vars->link_up, driver needs to
 *	1. Update the pbf
 *	2. Disable drain
 *	3. Update the shared memory
 *	4. Indicate link up
 *	5. Set LEDs
 *  Otherwise,
 *	1. Update shared memory
 *	2. Reset BigMac
 *	3. Report link down
 *	4. Unset LEDs
 */
uint8_t
bxe_link_update(struct link_params *params, struct link_vars *vars)
{
	struct bxe_softc *sc;
	uint32_t ext_phy_type;
	uint16_t gp_status;
	uint8_t link_10g, port;
	uint8_t ext_phy_link_up, rc;
	uint8_t is_mi_int;

	sc = params->sc;
	port = params->port;
	rc = 0;
	is_mi_int = 0;

	DBPRINT(sc, BXE_VERBOSE_PHY, "port %x, XGXS?%x, int_status 0x%x\n",
	    port, (vars->phy_flags & PHY_XGXS_FLAG), REG_RD(sc,
	    NIG_REG_STATUS_INTERRUPT_PORT0 + port * 4));

	is_mi_int = (uint8_t)(REG_RD(sc, NIG_REG_EMAC0_STATUS_MISC_MI_INT +
	    port * 0x18) > 0);
	DBPRINT(sc, BXE_VERBOSE_PHY,
	    "int_mask 0x%x MI_INT %x, SERDES_LINK %x\n", REG_RD(sc,
	    NIG_REG_MASK_INTERRUPT_PORT0 + port * 4), is_mi_int, REG_RD(sc,
	    NIG_REG_SERDES0_STATUS_LINK_STATUS + port * 0x3c));

	DBPRINT(sc, BXE_VERBOSE_PHY, " 10G %x, XGXS_LINK %x\n",
	    REG_RD(sc, NIG_REG_XGXS0_STATUS_LINK10G + port * 0x68),
	    REG_RD(sc, NIG_REG_XGXS0_STATUS_LINK_STATUS + port * 0x68));

	/* Disable emac. */
	REG_WR(sc, NIG_REG_NIG_EMAC0_EN + port * 4, 0);

	ext_phy_type = XGXS_EXT_PHY_TYPE(params->ext_phy_config);

	/* Check external link change only for non-direct. */
	ext_phy_link_up = bxe_ext_phy_is_link_up(params, vars, is_mi_int);

	/* Read gp_status. */
	CL45_RD_OVER_CL22(sc, port, params->phy_addr, MDIO_REG_BANK_GP_STATUS,
	    MDIO_GP_STATUS_TOP_AN_STATUS1, &gp_status);

	rc = bxe_link_settings_status(params, vars, gp_status, ext_phy_link_up);
	if (rc != 0)
		return (rc);

	/* Anything 10 and over uses the bmac. */
	link_10g = ((vars->line_speed == SPEED_10000) ||
	    (vars->line_speed == SPEED_12000) ||
	    (vars->line_speed == SPEED_12500) ||
	    (vars->line_speed == SPEED_13000) ||
	    (vars->line_speed == SPEED_15000) ||
	    (vars->line_speed == SPEED_16000));

	bxe_link_int_ack(params, vars, link_10g, is_mi_int);

	/*
	 * In case external phy link is up, and internal link is down,
	 * not initialized yet probably after link initialization, it
	 * needs to be initialized.
	 * Note that after link down-up as result of cable plug,
	 * the xgxs link would probably become up again without the need
	 * to initialize it.
	 */
	if ((ext_phy_type != PORT_HW_CFG_SERDES_EXT_PHY_TYPE_DIRECT) &&
	    (ext_phy_type != PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705) &&
	    (ext_phy_type != PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8706) &&
	    (ext_phy_type != PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726) &&
	    (ext_phy_link_up && !vars->phy_link_up)) {
			bxe_init_internal_phy(params, vars, 0);
	}

	/* Link is up only if both local phy and external phy are up. */
	vars->link_up = (ext_phy_link_up && vars->phy_link_up);
	if (vars->link_up)
		rc = bxe_update_link_up(params, vars, link_10g, gp_status);
	else
		rc = bxe_update_link_down(params, vars);

	return (rc);
}

static uint8_t
bxe_8073_common_init_phy(struct bxe_softc *sc, uint32_t shmem_base)
{
	uint32_t ext_phy_config;
	uint16_t fw_ver1, val;
	uint8_t ext_phy_addr[PORT_MAX];
	int port;

	/* PART1 - Reset both phys. */
	for (port = PORT_MAX - 1; port >= PORT_0; port--) {
		/* Extract the ext phy address for the port. */
		ext_phy_config = REG_RD(sc, shmem_base +
		    offsetof(struct shmem_region,
		    dev_info.port_hw_config[port].external_phy_config));

		/* Disable attentions. */
		bxe_bits_dis(sc, NIG_REG_MASK_INTERRUPT_PORT0 + port * 4,
		    (NIG_MASK_XGXS0_LINK_STATUS | NIG_MASK_XGXS0_LINK10G |
		    NIG_MASK_SERDES0_LINK_STATUS | NIG_MASK_MI_INT));

		ext_phy_addr[port] = XGXS_EXT_PHY_ADDR(ext_phy_config);

		/*
		 * Need to take the phy out of low power mode in order
		 * to write to access its registers.
		 */
		bxe_set_gpio(sc, MISC_REGISTERS_GPIO_2,
		    MISC_REGISTERS_GPIO_OUTPUT_HIGH, port);

		/* Reset the phy. */
		bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
		    ext_phy_addr[port], MDIO_PMA_DEVAD, MDIO_PMA_REG_CTRL,
		    1 << 15);
	}

	/* Add delay of 150ms after reset. */
	msleep(150);

	/* PART2 - Download firmware to both phys. */
	for (port = PORT_MAX - 1; port >= PORT_0; port--) {
		bxe_bcm8073_external_rom_boot(sc, port, ext_phy_addr[port],
		    shmem_base);
		bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
		    ext_phy_addr[port], MDIO_PMA_DEVAD, MDIO_PMA_REG_ROM_VER1,
		    &fw_ver1);
		if (fw_ver1 == 0 || fw_ver1 == 0x4321) {
			DBPRINT(sc, BXE_VERBOSE_PHY,
			    "bxe_8073_common_init_phy port %x:"
			    "Download failed. fw version = 0x%x\n", port,
			    fw_ver1);
			return (-EINVAL);
		}

		/* Only set bit 10 = 1 (Tx power down). */
		bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
		    ext_phy_addr[port], MDIO_PMA_DEVAD,
		    MDIO_PMA_REG_TX_POWER_DOWN, &val);

		/* Phase1 of TX_POWER_DOWN reset. */
		bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
		    ext_phy_addr[port], MDIO_PMA_DEVAD,
		    MDIO_PMA_REG_TX_POWER_DOWN, (val | 1 << 10));
	}

	/*
	 * Toggle Transmitter: Power down and then up with 600ms
	 * delay between.
	 */
	msleep(600);

	/* PART3 - complete TX_POWER_DOWN process, and set GPIO2 back to low. */
	for (port = PORT_MAX - 1; port >= PORT_0; port--) {
		/* Phase2 of POWER_DOWN_RESET */
		/* Release bit 10 (Release Tx power down). */
		bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
		    ext_phy_addr[port], MDIO_PMA_DEVAD,
		    MDIO_PMA_REG_TX_POWER_DOWN, &val);

		bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
		    ext_phy_addr[port], MDIO_PMA_DEVAD,
		    MDIO_PMA_REG_TX_POWER_DOWN, (val & (~(1 << 10))));
		msleep(15);

		/* Read modify write the SPI-ROM version select register. */
		bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
		    ext_phy_addr[port], MDIO_PMA_DEVAD,
		    MDIO_PMA_REG_EDC_FFE_MAIN, &val);
		bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073,
		    ext_phy_addr[port], MDIO_PMA_DEVAD,
		    MDIO_PMA_REG_EDC_FFE_MAIN, (val | (1 << 12)));

		/* Set GPIO2 back to LOW. */
		bxe_set_gpio(sc, MISC_REGISTERS_GPIO_2,
		     MISC_REGISTERS_GPIO_OUTPUT_LOW, port);
	}
	return (0);
}

static uint8_t
bxe_8727_common_init_phy(struct bxe_softc *sc, uint32_t shmem_base)
{
	uint32_t swap_val, swap_override;
	uint32_t ext_phy_config;
	uint16_t fw_ver1;
	uint8_t ext_phy_addr[PORT_MAX];
	uint8_t port, first_port, i;

	DBPRINT(sc, BXE_VERBOSE_PHY, "Executing BCM8727 common init\n");
	swap_val = REG_RD(sc,  NIG_REG_PORT_SWAP);
	swap_override = REG_RD(sc,  NIG_REG_STRAP_OVERRIDE);

	bxe_ext_phy_hw_reset(sc, 1 ^ (swap_val && swap_override));
	msleep(5);

	if (swap_val && swap_override)
		first_port = PORT_0;
	else
		first_port = PORT_1;
	/* PART1 - Reset both phys. */
	for (i = 0, port = first_port; i < PORT_MAX; i++, port = !port) {
		/* Extract the ext phy address for the port. */
		ext_phy_config = REG_RD(sc, shmem_base +
		    offsetof(struct shmem_region,
		    dev_info.port_hw_config[port].external_phy_config));

		/* Disable attentions. */
		bxe_bits_dis(sc, NIG_REG_MASK_INTERRUPT_PORT0 + port * 4,
		    (NIG_MASK_XGXS0_LINK_STATUS | NIG_MASK_XGXS0_LINK10G |
		    NIG_MASK_SERDES0_LINK_STATUS | NIG_MASK_MI_INT));

		ext_phy_addr[port] = XGXS_EXT_PHY_ADDR(ext_phy_config);

		/* Reset the phy. */
		bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
		    ext_phy_addr[port], MDIO_PMA_DEVAD, MDIO_PMA_REG_CTRL,
		    1 << 15);
	}

	/* Add delay of 150ms after reset. */
	msleep(150);

	/* PART2 - Download firmware to both phys. */
	for (i = 0, port = first_port; i < PORT_MAX; i++, port = !port) {
		bxe_bcm8727_external_rom_boot(sc, port, ext_phy_addr[port],
		    shmem_base);
		bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727,
		    ext_phy_addr[port], MDIO_PMA_DEVAD, MDIO_PMA_REG_ROM_VER1,
		    &fw_ver1);
		if (fw_ver1 == 0 || fw_ver1 == 0x4321) {
			DBPRINT(sc, 1/*BXE_VERBOSE_PHY*/,
			    "bxe_8727_common_init_phy port %x:"
			    "Download failed. fw version = 0x%x\n", port,
			    fw_ver1);
			return (-EINVAL);
		}
	}

	return (0);
}

static uint8_t
bxe_8726_common_init_phy(struct bxe_softc *sc, uint32_t shmem_base)
{
	uint32_t ext_phy_config, val;
	uint8_t ext_phy_addr;
	uint8_t port;

	/* Use port1 because of the static port-swap. */
	/* Enable the module detection interrupt. */
	val = REG_RD(sc, MISC_REG_GPIO_EVENT_EN);
	val |= ((1 << MISC_REGISTERS_GPIO_3) | (1 << (MISC_REGISTERS_GPIO_3 +
	    MISC_REGISTERS_GPIO_PORT_SHIFT)));
	REG_WR(sc, MISC_REG_GPIO_EVENT_EN, val);

	bxe_ext_phy_hw_reset(sc, 1);
	msleep(5);
	for (port = 0; port < PORT_MAX; port++) {
		/* Extract the ext phy address for the port. */
		ext_phy_config = REG_RD(sc, shmem_base +
		    offsetof(struct shmem_region,
		    dev_info.port_hw_config[port].external_phy_config));

		ext_phy_addr = XGXS_EXT_PHY_ADDR(ext_phy_config);
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "8726_common_init : ext_phy_addr = 0x%x\n", ext_phy_addr);

		bxe_8726_reset_phy(sc, port, ext_phy_addr);

		/* Set fault module detected LED on. */
		bxe_set_gpio(sc, MISC_REGISTERS_GPIO_0,
		    MISC_REGISTERS_GPIO_HIGH, port);
	}

	return (0);
}

static uint8_t
bxe_84823_common_init_phy(struct bxe_softc *sc, uint32_t shmem_base)
{

	bxe_ext_phy_hw_reset(sc, 1);
	return (0);
}

uint8_t
bxe_common_init_phy(struct bxe_softc *sc, uint32_t shmem_base)
{
	uint32_t ext_phy_type;
	uint8_t rc;

	DBPRINT(sc, BXE_VERBOSE_PHY, "Begin common phy init\n");

	rc = 0;
	ext_phy_type = 0;
	/* Read the ext_phy_type for arbitrary port(0) */
	ext_phy_type = XGXS_EXT_PHY_TYPE( REG_RD(sc, shmem_base +
	    offsetof(struct shmem_region,
	    dev_info.port_hw_config[0].external_phy_config)));

	switch (ext_phy_type) {
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073:
		rc = bxe_8073_common_init_phy(sc, shmem_base);
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727:
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727_NOC:
		rc = bxe_8727_common_init_phy(sc, shmem_base);
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726:
		/*
		 * GPIO1 affects both ports, so there's need to pull
		 * it for single port alone.
		 */
		rc = bxe_8726_common_init_phy(sc, shmem_base);
		break;
	case PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM84823:
		rc = bxe_84823_common_init_phy(sc, shmem_base);
		break;
	default:
		DBPRINT(sc, BXE_VERBOSE_PHY,
		    "bxe_common_init_phy: ext_phy 0x%x not required\n",
		    ext_phy_type);
		break;
	}

	return (rc);
}

void
bxe_sfx7101_sp_sw_reset(struct bxe_softc *sc, uint8_t port, uint8_t phy_addr)
{
	uint16_t val, cnt;

	bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101, phy_addr,
	    MDIO_PMA_DEVAD, MDIO_PMA_REG_7101_RESET, &val);

	for (cnt = 0; cnt < 10; cnt++) {
		msleep(50);
		/* Writes a self-clearing reset. */
		bxe_cl45_write(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101,
		    phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_7101_RESET,
		    (val | (1 << 15)));
		/* Wait for clear. */
		bxe_cl45_read(sc, port, PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101,
		    phy_addr, MDIO_PMA_DEVAD, MDIO_PMA_REG_7101_RESET, &val);
		if ((val & (1 << 15)) == 0)
			break;
	}
}
