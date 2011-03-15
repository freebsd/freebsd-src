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

 /*$FreeBSD$*/

#ifndef BXE_LINK_H
#define	BXE_LINK_H

/*
 * Defines
 */
#define	DEFAULT_PHY_DEV_ADDR	3



#define	FLOW_CTRL_AUTO		PORT_FEATURE_FLOW_CONTROL_AUTO
#define	FLOW_CTRL_TX		PORT_FEATURE_FLOW_CONTROL_TX
#define	FLOW_CTRL_RX		PORT_FEATURE_FLOW_CONTROL_RX
#define	FLOW_CTRL_BOTH		PORT_FEATURE_FLOW_CONTROL_BOTH
#define	FLOW_CTRL_NONE		PORT_FEATURE_FLOW_CONTROL_NONE

#define	SPEED_AUTO_NEG		0
#define	SPEED_12000		12000
#define	SPEED_12500		12500
#define	SPEED_13000		13000
#define	SPEED_15000		15000
#define	SPEED_16000		16000

#define	SFP_EEPROM_VENDOR_NAME_ADDR	0x14
#define	SFP_EEPROM_VENDOR_NAME_SIZE	16
#define	SFP_EEPROM_VENDOR_OUI_ADDR	0x25
#define	SFP_EEPROM_VENDOR_OUI_SIZE	3
#define	SFP_EEPROM_PART_NO_ADDR 	0x28
#define	SFP_EEPROM_PART_NO_SIZE		16
#define	PWR_FLT_ERR_MSG_LEN		250

/*
 * Structs
 */
/* Inputs parameters to the CLC. */
struct link_params {
	uint8_t port;
	/* Default / User Configuration */
	uint8_t loopback_mode;
#define	LOOPBACK_NONE			0
#define	LOOPBACK_EMAC			1
#define	LOOPBACK_BMAC			2
#define	LOOPBACK_XGXS_10		3
#define	LOOPBACK_EXT_PHY		4
#define	LOOPBACK_EXT 			5

	uint16_t req_duplex;
	uint16_t req_flow_ctrl;
	/* Should be set to TX / BOTH when req_flow_ctrl is set to AUTO. */
	uint16_t req_fc_auto_adv;
	/* Also determine AutoNeg. */
	uint16_t req_line_speed;

	/* Device parameters */
	uint8_t mac_addr[6];

	/* shmem parameters */
	uint32_t shmem_base;
	uint32_t speed_cap_mask;
	uint32_t switch_cfg;
#define	SWITCH_CFG_1G		PORT_FEATURE_CON_SWITCH_1G_SWITCH
#define	SWITCH_CFG_10G		PORT_FEATURE_CON_SWITCH_10G_SWITCH
#define	SWITCH_CFG_AUTO_DETECT	PORT_FEATURE_CON_SWITCH_AUTO_DETECT

	/* Part of the hw_config read from the shmem. */
	uint16_t hw_led_mode;

	/* phy_addr populated by the CLC. */
	uint8_t phy_addr;
	/* uint8_t reserved1; */

	uint32_t lane_config;
	uint32_t ext_phy_config;
#define	XGXS_EXT_PHY_TYPE(ext_phy_config)				\
	((ext_phy_config) & PORT_HW_CFG_XGXS_EXT_PHY_TYPE_MASK)
#define	XGXS_EXT_PHY_ADDR(ext_phy_config)				\
	(((ext_phy_config) & PORT_HW_CFG_XGXS_EXT_PHY_ADDR_MASK) >>	\
	    PORT_HW_CFG_XGXS_EXT_PHY_ADDR_SHIFT)
#define	SERDES_EXT_PHY_TYPE(ext_phy_config)				\
	((ext_phy_config) & PORT_HW_CFG_SERDES_EXT_PHY_TYPE_MASK)

	/* Phy register parameter */
	uint32_t chip_id;

	uint16_t xgxs_config_rx[4]; /* preemphasis values for the rx side */
	uint16_t xgxs_config_tx[4]; /* preemphasis values for the tx side */

	uint32_t feature_config_flags;
#define	FEATURE_CONFIG_OVERRIDE_PREEMPHASIS_ENABLED	(1<<0)
#define	FEATURE_CONFIG_PFC_ENABLED			(1<<1)
#define	FEATURE_CONFIG_BC_SUPPORTS_OPT_MDL_VRFY		(1<<2)
#define	FEATURE_CONFIG_BCM8727_NOC			(1<<3)

	/* Device pointer passed to all callback functions. */
	struct bxe_softc *sc;
};

/* Output parameters */
struct link_vars {
	uint8_t phy_flags;

	uint8_t mac_type;
#define	MAC_TYPE_NONE		0
#define	MAC_TYPE_EMAC		1
#define	MAC_TYPE_BMAC		2

	/* Internal phy link indication. */
	uint8_t phy_link_up;
	uint8_t link_up;

	uint16_t line_speed;
	uint16_t duplex;

	uint16_t flow_ctrl;
	uint16_t ieee_fc;

	uint32_t autoneg;
#define	AUTO_NEG_DISABLED			0x0
#define	AUTO_NEG_ENABLED			0x1
#define	AUTO_NEG_COMPLETE			0x2
#define	AUTO_NEG_PARALLEL_DETECTION_USED	0x4

	/* The same definitions as the shmem parameter. */
	uint32_t link_status;
};

/*
 * Functions
 */

/* Initialize the phy. */
uint8_t bxe_phy_init(struct link_params *input, struct link_vars *output);

/*
 * Reset the link. Should be called when driver or interface goes down
 * Before calling phy firmware upgrade, the reset_ext_phy should be set
 * to 0.
 */
uint8_t bxe_link_reset(struct link_params *params, struct link_vars *vars,
    uint8_t reset_ext_phy);

/* bxe_link_update should be called upon link interrupt */
uint8_t bxe_link_update(struct link_params *input, struct link_vars *output);

/*
 * Use the following cl45 functions to read/write from external_phy
 * In order to use it to read/write internal phy registers, use
 * DEFAULT_PHY_DEV_ADDR as devad, and (_bank + (_addr & 0xf)) as
 * Use ext_phy_type of 0 in case of cl22 over cl45
 * the register.
 */
uint8_t bxe_cl45_read(struct bxe_softc *sc, uint8_t port, uint32_t ext_phy_type,
    uint8_t phy_addr, uint8_t devad, uint16_t reg, uint16_t *ret_val);

uint8_t bxe_cl45_write(struct bxe_softc *sc, uint8_t port,
    uint32_t ext_phy_type, uint8_t phy_addr, uint8_t devad, uint16_t reg,
    uint16_t val);

/*
 * Reads the link_status from the shmem, and update the link vars accordingly.
 */
void bxe_link_status_update(struct link_params *input,
    struct link_vars *output);
/* Returns string representing the fw_version of the external phy. */
uint8_t bxe_get_ext_phy_fw_version(struct link_params *params,
    uint8_t driver_loaded, uint8_t *version, uint16_t len);

/*
 * Set/Unset the led
 * Basically, the CLC takes care of the led for the link, but in case one needs
 * to set/unset the led unnaturally, set the "mode" to LED_MODE_OPER to blink
 * the led, and LED_MODE_OFF to set the led off.
 */
uint8_t bxe_set_led(struct link_params *params, uint8_t mode, uint32_t speed);
#define	LED_MODE_OFF	0
#define	LED_MODE_OPER 	2

uint8_t bxe_override_led_value(struct bxe_softc *sc, uint8_t port,
    uint32_t led_idx, uint32_t value);

/*
 * bxe_handle_module_detect_int should be called upon module detection
 * interrupt.
 */
void bxe_handle_module_detect_int(struct link_params *params);

/*
 * Get the actual link status. In case it returns 0, link is up, otherwise
 * link is down.
 */
uint8_t bxe_test_link(struct link_params *input, struct link_vars *vars);

/* One-time initialization for external phy after power up. */
uint8_t bxe_common_init_phy(struct bxe_softc *sc, uint32_t shmem_base);

/* Reset the external PHY using GPIO. */
void bxe_ext_phy_hw_reset(struct bxe_softc *sc, uint8_t port);

void bxe_sfx7101_sp_sw_reset(struct bxe_softc *sc, uint8_t port,
    uint8_t phy_addr);

uint8_t bxe_read_sfp_module_eeprom(struct link_params *params, uint16_t addr,
    uint8_t byte_cnt, uint8_t *o_buf);

#endif /* BXE_LINK_H */
