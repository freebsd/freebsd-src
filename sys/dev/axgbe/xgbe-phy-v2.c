/*
 * AMD 10Gb Ethernet driver
 *
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
 *
 * This file is available to you under your choice of the following two
 * licenses:
 *
 * License 1: GPLv2
 *
 * This file is free software; you may copy, redistribute and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or (at
 * your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *     The Synopsys DWC ETHER XGMAC Software Driver and documentation
 *     (hereinafter "Software") is an unsupported proprietary work of Synopsys,
 *     Inc. unless otherwise expressly agreed to in writing between Synopsys
 *     and you.
 *
 *     The Software IS NOT an item of Licensed Software or Licensed Product
 *     under any End User Software License Agreement or Agreement for Licensed
 *     Product with Synopsys or any supplement thereto.  Permission is hereby
 *     granted, free of charge, to any person obtaining a copy of this software
 *     annotated with this license and the Software, to deal in the Software
 *     without restriction, including without limitation the rights to use,
 *     copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 *     of the Software, and to permit persons to whom the Software is furnished
 *     to do so, subject to the following conditions:
 *
 *     The above copyright notice and this permission notice shall be included
 *     in all copies or substantial portions of the Software.
 *
 *     THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS"
 *     BASIS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *     TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *     PARTICULAR PURPOSE ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS
 *     BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *     CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *     SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *     INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *     ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *     THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * License 2: Modified BSD
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Advanced Micro Devices, Inc. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *     The Synopsys DWC ETHER XGMAC Software Driver and documentation
 *     (hereinafter "Software") is an unsupported proprietary work of Synopsys,
 *     Inc. unless otherwise expressly agreed to in writing between Synopsys
 *     and you.
 *
 *     The Software IS NOT an item of Licensed Software or Licensed Product
 *     under any End User Software License Agreement or Agreement for Licensed
 *     Product with Synopsys or any supplement thereto.  Permission is hereby
 *     granted, free of charge, to any person obtaining a copy of this software
 *     annotated with this license and the Software, to deal in the Software
 *     without restriction, including without limitation the rights to use,
 *     copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 *     of the Software, and to permit persons to whom the Software is furnished
 *     to do so, subject to the following conditions:
 *
 *     The above copyright notice and this permission notice shall be included
 *     in all copies or substantial portions of the Software.
 *
 *     THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS"
 *     BASIS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *     TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *     PARTICULAR PURPOSE ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS
 *     BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *     CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *     SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *     INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *     ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *     THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "xgbe.h"
#include "xgbe-common.h"

struct mtx xgbe_phy_comm_lock;

#define XGBE_PHY_PORT_SPEED_100		BIT(0)
#define XGBE_PHY_PORT_SPEED_1000	BIT(1)
#define XGBE_PHY_PORT_SPEED_2500	BIT(2)
#define XGBE_PHY_PORT_SPEED_10000	BIT(3)

#define XGBE_MUTEX_RELEASE		0x80000000

#define XGBE_SFP_DIRECT			7
#define GPIO_MASK_WIDTH			4

/* I2C target addresses */
#define XGBE_SFP_SERIAL_ID_ADDRESS	0x50
#define XGBE_SFP_DIAG_INFO_ADDRESS	0x51
#define XGBE_SFP_PHY_ADDRESS		0x56
#define XGBE_GPIO_ADDRESS_PCA9555	0x20

/* SFP sideband signal indicators */
#define XGBE_GPIO_NO_TX_FAULT		BIT(0)
#define XGBE_GPIO_NO_RATE_SELECT	BIT(1)
#define XGBE_GPIO_NO_MOD_ABSENT		BIT(2)
#define XGBE_GPIO_NO_RX_LOS		BIT(3)

/* Rate-change complete wait/retry count */
#define XGBE_RATECHANGE_COUNT		500

/* CDR delay values for KR support (in usec) */
#define XGBE_CDR_DELAY_INIT		10000
#define XGBE_CDR_DELAY_INC		10000
#define XGBE_CDR_DELAY_MAX		100000

/* RRC frequency during link status check */
#define XGBE_RRC_FREQUENCY		10

/* SFP port max PHY probe retries */
#define XGBE_SFP_PHY_RETRY_MAX		5

enum xgbe_port_mode {
	XGBE_PORT_MODE_RSVD = 0,
	XGBE_PORT_MODE_BACKPLANE,
	XGBE_PORT_MODE_BACKPLANE_2500,
	XGBE_PORT_MODE_1000BASE_T,
	XGBE_PORT_MODE_1000BASE_X,
	XGBE_PORT_MODE_NBASE_T,
	XGBE_PORT_MODE_10GBASE_T,
	XGBE_PORT_MODE_10GBASE_R,
	XGBE_PORT_MODE_SFP,
	XGBE_PORT_MODE_MAX,
};

enum xgbe_conn_type {
	XGBE_CONN_TYPE_NONE = 0,
	XGBE_CONN_TYPE_SFP,
	XGBE_CONN_TYPE_MDIO,
	XGBE_CONN_TYPE_RSVD1,
	XGBE_CONN_TYPE_BACKPLANE,
	XGBE_CONN_TYPE_MAX,
};

/* SFP/SFP+ related definitions */
enum xgbe_sfp_comm {
	XGBE_SFP_COMM_DIRECT = 0,
	XGBE_SFP_COMM_PCA9545,
};

enum xgbe_sfp_cable {
	XGBE_SFP_CABLE_UNKNOWN = 0,
	XGBE_SFP_CABLE_ACTIVE,
	XGBE_SFP_CABLE_PASSIVE,
};

enum xgbe_sfp_base {
	XGBE_SFP_BASE_UNKNOWN = 0,
	XGBE_SFP_BASE_PX,
	XGBE_SFP_BASE_BX10,
	XGBE_SFP_BASE_100_FX,
	XGBE_SFP_BASE_100_LX10,
	XGBE_SFP_BASE_100_BX,
	XGBE_SFP_BASE_1000_T,
	XGBE_SFP_BASE_1000_SX,
	XGBE_SFP_BASE_1000_LX,
	XGBE_SFP_BASE_1000_CX,
	XGBE_SFP_BASE_1000_BX,
	XGBE_SFP_BASE_10000_SR,
	XGBE_SFP_BASE_10000_LR,
	XGBE_SFP_BASE_10000_LRM,
	XGBE_SFP_BASE_10000_ER,
	XGBE_SFP_BASE_10000_CR,
};

enum xgbe_sfp_speed {
	XGBE_SFP_SPEED_UNKNOWN = 0,
	XGBE_SFP_SPEED_100,
	XGBE_SFP_SPEED_100_1000,
	XGBE_SFP_SPEED_1000,
	XGBE_SFP_SPEED_10000,
	XGBE_SFP_SPEED_25000,
};

/* SFP Serial ID Base ID values relative to an offset of 0 */
#define XGBE_SFP_BASE_ID			0
#define XGBE_SFP_ID_SFP				0x03

#define XGBE_SFP_BASE_EXT_ID			1
#define XGBE_SFP_EXT_ID_SFP			0x04

#define XGBE_SFP_BASE_CV			2
#define XGBE_SFP_BASE_CV_CP			0x21

#define XGBE_SFP_BASE_10GBE_CC			3
#define XGBE_SFP_BASE_10GBE_CC_SR		BIT(4)
#define XGBE_SFP_BASE_10GBE_CC_LR		BIT(5)
#define XGBE_SFP_BASE_10GBE_CC_LRM		BIT(6)
#define XGBE_SFP_BASE_10GBE_CC_ER		BIT(7)

#define XGBE_SFP_BASE_1GBE_CC			6
#define XGBE_SFP_BASE_1GBE_CC_SX		BIT(0)
#define XGBE_SFP_BASE_1GBE_CC_LX		BIT(1)
#define XGBE_SFP_BASE_1GBE_CC_CX		BIT(2)
#define XGBE_SFP_BASE_1GBE_CC_T			BIT(3)
#define XGBE_SFP_BASE_100M_CC_LX10		BIT(4)
#define XGBE_SFP_BASE_100M_CC_FX		BIT(5)
#define XGBE_SFP_BASE_CC_BX10			BIT(6)
#define XGBE_SFP_BASE_CC_PX			BIT(7)

#define XGBE_SFP_BASE_CABLE			8
#define XGBE_SFP_BASE_CABLE_PASSIVE		BIT(2)
#define XGBE_SFP_BASE_CABLE_ACTIVE		BIT(3)

#define XGBE_SFP_BASE_BR			12
#define XGBE_SFP_BASE_BR_100M_MIN		0x1
#define XGBE_SFP_BASE_BR_100M_MAX		0x2
#define XGBE_SFP_BASE_BR_1GBE_MIN		0x0a
#define XGBE_SFP_BASE_BR_1GBE_MAX		0x0d
#define XGBE_SFP_BASE_BR_10GBE_MIN		0x64
#define XGBE_SFP_BASE_BR_10GBE_MAX		0x68
#define XGBE_SFP_BASE_BR_25GBE			0xFF

/* Single mode, length of fiber in units of km */
#define XGBE_SFP_BASE_SM_LEN_KM			14
#define XGBE_SFP_BASE_SM_LEN_KM_MIN		0x0A

/* Single mode, length of fiber in units of 100m */
#define XGBE_SFP_BASE_SM_LEN_100M		15
#define XGBE_SFP_BASE_SM_LEN_100M_MIN		0x64

#define XGBE_SFP_BASE_CU_CABLE_LEN		18

#define XGBE_SFP_BASE_VENDOR_NAME		20
#define XGBE_SFP_BASE_VENDOR_NAME_LEN		16
#define XGBE_SFP_BASE_VENDOR_PN			40
#define XGBE_SFP_BASE_VENDOR_PN_LEN		16
#define XGBE_SFP_BASE_VENDOR_REV		56
#define XGBE_SFP_BASE_VENDOR_REV_LEN		4

/*
 * Optical specification compliance - denotes wavelength
 * for optical tranceivers
 */
#define XGBE_SFP_BASE_OSC			60
#define XGBE_SFP_BASE_OSC_LEN			2
#define XGBE_SFP_BASE_OSC_1310			0x051E

#define XGBE_SFP_BASE_CC			63

/* SFP Serial ID Extended ID values relative to an offset of 64 */
#define XGBE_SFP_BASE_VENDOR_SN			4
#define XGBE_SFP_BASE_VENDOR_SN_LEN		16

#define XGBE_SFP_EXTD_OPT1			1
#define XGBE_SFP_EXTD_OPT1_RX_LOS		BIT(1)
#define XGBE_SFP_EXTD_OPT1_TX_FAULT		BIT(3)

#define XGBE_SFP_EXTD_DIAG			28
#define XGBE_SFP_EXTD_DIAG_ADDR_CHANGE		BIT(2)

#define XGBE_SFP_EXTD_SFF_8472			30

#define XGBE_SFP_EXTD_CC			31

struct xgbe_sfp_eeprom {
	uint8_t base[64];
	uint8_t extd[32];
	uint8_t vendor[32];
};

#define XGBE_SFP_DIAGS_SUPPORTED(_x)			\
	((_x)->extd[XGBE_SFP_EXTD_SFF_8472] &&		\
	 !((_x)->extd[XGBE_SFP_EXTD_DIAG] & XGBE_SFP_EXTD_DIAG_ADDR_CHANGE))

#define XGBE_SFP_EEPROM_BASE_LEN		256
#define XGBE_SFP_EEPROM_DIAG_LEN		256
#define XGBE_SFP_EEPROM_MAX			(XGBE_SFP_EEPROM_BASE_LEN +	\
					 	XGBE_SFP_EEPROM_DIAG_LEN)

#define XGBE_BEL_FUSE_VENDOR			"BEL-FUSE        "
#define XGBE_BEL_FUSE_PARTNO			"1GBT-SFP06      "

struct xgbe_sfp_ascii {
	union {
		char vendor[XGBE_SFP_BASE_VENDOR_NAME_LEN + 1];
		char partno[XGBE_SFP_BASE_VENDOR_PN_LEN + 1];
		char rev[XGBE_SFP_BASE_VENDOR_REV_LEN + 1];
		char serno[XGBE_SFP_BASE_VENDOR_SN_LEN + 1];
	} u;
};

/* MDIO PHY reset types */
enum xgbe_mdio_reset {
	XGBE_MDIO_RESET_NONE = 0,
	XGBE_MDIO_RESET_I2C_GPIO,
	XGBE_MDIO_RESET_INT_GPIO,
	XGBE_MDIO_RESET_MAX,
};

/* Re-driver related definitions */
enum xgbe_phy_redrv_if {
	XGBE_PHY_REDRV_IF_MDIO = 0,
	XGBE_PHY_REDRV_IF_I2C,
	XGBE_PHY_REDRV_IF_MAX,
};

enum xgbe_phy_redrv_model {
	XGBE_PHY_REDRV_MODEL_4223 = 0,
	XGBE_PHY_REDRV_MODEL_4227,
	XGBE_PHY_REDRV_MODEL_MAX,
};

enum xgbe_phy_redrv_mode {
	XGBE_PHY_REDRV_MODE_CX = 5,
	XGBE_PHY_REDRV_MODE_SR = 9,
};

#define XGBE_PHY_REDRV_MODE_REG	0x12b0

/* PHY related configuration information */
struct xgbe_phy_data {
	enum xgbe_port_mode port_mode;

	unsigned int port_id;

	unsigned int port_speeds;

	enum xgbe_conn_type conn_type;

	enum xgbe_mode cur_mode;
	enum xgbe_mode start_mode;

	unsigned int rrc_count;

	unsigned int mdio_addr;

	/* SFP Support */
	enum xgbe_sfp_comm sfp_comm;
	unsigned int sfp_mux_address;
	unsigned int sfp_mux_channel;

	unsigned int sfp_gpio_address;
	unsigned int sfp_gpio_mask;
	unsigned int sfp_gpio_inputs;
	unsigned int sfp_gpio_rx_los;
	unsigned int sfp_gpio_tx_fault;
	unsigned int sfp_gpio_mod_absent;
	unsigned int sfp_gpio_rate_select;

	unsigned int sfp_rx_los;
	unsigned int sfp_tx_fault;
	unsigned int sfp_mod_absent;
	unsigned int sfp_changed;
	unsigned int sfp_phy_avail;
	unsigned int sfp_cable_len;
	enum xgbe_sfp_base sfp_base;
	enum xgbe_sfp_cable sfp_cable;
	enum xgbe_sfp_speed sfp_speed;
	struct xgbe_sfp_eeprom sfp_eeprom;

	/* External PHY support */
	enum xgbe_mdio_mode phydev_mode;
	uint32_t phy_id;
	int phydev;
	enum xgbe_mdio_reset mdio_reset;
	unsigned int mdio_reset_addr;
	unsigned int mdio_reset_gpio;
	int sfp_phy_retries;

	/* Re-driver support */
	unsigned int redrv;
	unsigned int redrv_if;
	unsigned int redrv_addr;
	unsigned int redrv_lane;
	unsigned int redrv_model;

	/* KR AN support */
	unsigned int phy_cdr_notrack;
	unsigned int phy_cdr_delay;

	uint8_t port_sfp_inputs;
};

static enum xgbe_an_mode xgbe_phy_an_mode(struct xgbe_prv_data *pdata);
static int xgbe_phy_reset(struct xgbe_prv_data *pdata);
static int axgbe_ifmedia_upd(struct ifnet *ifp);
static void axgbe_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr);

static int
xgbe_phy_i2c_xfer(struct xgbe_prv_data *pdata, struct xgbe_i2c_op *i2c_op)
{
	return (pdata->i2c_if.i2c_xfer(pdata, i2c_op));
}

static int
xgbe_phy_redrv_write(struct xgbe_prv_data *pdata, unsigned int reg,
    unsigned int val)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	struct xgbe_i2c_op i2c_op;
	__be16 *redrv_val;
	uint8_t redrv_data[5], csum;
	unsigned int i, retry;
	int ret;

	/* High byte of register contains read/write indicator */
	redrv_data[0] = ((reg >> 8) & 0xff) << 1;
	redrv_data[1] = reg & 0xff;
	redrv_val = (__be16 *)&redrv_data[2];
	*redrv_val = cpu_to_be16(val);

	/* Calculate 1 byte checksum */
	csum = 0;
	for (i = 0; i < 4; i++) {
		csum += redrv_data[i];
		if (redrv_data[i] > csum)
			csum++;
	}
	redrv_data[4] = ~csum;

	retry = 1;
again1:
	i2c_op.cmd = XGBE_I2C_CMD_WRITE;
	i2c_op.target = phy_data->redrv_addr;
	i2c_op.len = sizeof(redrv_data);
	i2c_op.buf = redrv_data;
	ret = xgbe_phy_i2c_xfer(pdata, &i2c_op);
	if (ret) {
		if ((ret == -EAGAIN) && retry--)
			goto again1;

		return (ret);
	}

	retry = 1;
again2:
	i2c_op.cmd = XGBE_I2C_CMD_READ;
	i2c_op.target = phy_data->redrv_addr;
	i2c_op.len = 1;
	i2c_op.buf = redrv_data;
	ret = xgbe_phy_i2c_xfer(pdata, &i2c_op);
	if (ret) {
		if ((ret == -EAGAIN) && retry--)
			goto again2;

		return (ret);
	}

	if (redrv_data[0] != 0xff) {
		axgbe_error("Redriver write checksum error\n");
		ret = -EIO;
	}

	return (ret);
}

static int
xgbe_phy_i2c_write(struct xgbe_prv_data *pdata, unsigned int target, void *val,
    unsigned int val_len)
{
	struct xgbe_i2c_op i2c_op;
	int retry, ret;

	retry = 1;
again:
	/* Write the specfied register */
	i2c_op.cmd = XGBE_I2C_CMD_WRITE;
	i2c_op.target = target;
	i2c_op.len = val_len;
	i2c_op.buf = val;
	ret = xgbe_phy_i2c_xfer(pdata, &i2c_op);
	if ((ret == -EAGAIN) && retry--)
		goto again;

	return (ret);
}

static int
xgbe_phy_i2c_read(struct xgbe_prv_data *pdata, unsigned int target, void *reg,
    unsigned int reg_len, void *val, unsigned int val_len)
{
	struct xgbe_i2c_op i2c_op;
	int retry, ret;

	axgbe_printf(3, "%s: target 0x%x reg_len %d val_len %d\n", __func__,
	    target, reg_len, val_len);
	retry = 1;
again1:
	/* Set the specified register to read */
	i2c_op.cmd = XGBE_I2C_CMD_WRITE;
	i2c_op.target = target;
	i2c_op.len = reg_len;
	i2c_op.buf = reg;
	ret = xgbe_phy_i2c_xfer(pdata, &i2c_op);
	axgbe_printf(3, "%s: ret1 %d retry %d\n", __func__, ret, retry);
	if (ret) {
		if ((ret == -EAGAIN) && retry--)
			goto again1;

		return (ret);
	}

	retry = 1;
again2:
	/* Read the specfied register */
	i2c_op.cmd = XGBE_I2C_CMD_READ;
	i2c_op.target = target;
	i2c_op.len = val_len;
	i2c_op.buf = val;
	ret = xgbe_phy_i2c_xfer(pdata, &i2c_op);
	axgbe_printf(3, "%s: ret2 %d retry %d\n", __func__, ret, retry);
	if ((ret == -EAGAIN) && retry--)
		goto again2;

	return (ret);
}

static int
xgbe_phy_sfp_put_mux(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	struct xgbe_i2c_op i2c_op;
	uint8_t mux_channel;

	if (phy_data->sfp_comm == XGBE_SFP_COMM_DIRECT)
		return (0);

	/* Select no mux channels */
	mux_channel = 0;
	i2c_op.cmd = XGBE_I2C_CMD_WRITE;
	i2c_op.target = phy_data->sfp_mux_address;
	i2c_op.len = sizeof(mux_channel);
	i2c_op.buf = &mux_channel;

	return (xgbe_phy_i2c_xfer(pdata, &i2c_op));
}

static int
xgbe_phy_sfp_get_mux(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	struct xgbe_i2c_op i2c_op;
	uint8_t mux_channel;

	if (phy_data->sfp_comm == XGBE_SFP_COMM_DIRECT)
		return (0);

	/* Select desired mux channel */
	mux_channel = 1 << phy_data->sfp_mux_channel;
	i2c_op.cmd = XGBE_I2C_CMD_WRITE;
	i2c_op.target = phy_data->sfp_mux_address;
	i2c_op.len = sizeof(mux_channel);
	i2c_op.buf = &mux_channel;

	return (xgbe_phy_i2c_xfer(pdata, &i2c_op));
}

static void
xgbe_phy_put_comm_ownership(struct xgbe_prv_data *pdata)
{
	mtx_unlock(&xgbe_phy_comm_lock);
}

static int
xgbe_phy_get_comm_ownership(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	unsigned long timeout;
	unsigned int mutex_id;

	/* The I2C and MDIO/GPIO bus is multiplexed between multiple devices,
	 * the driver needs to take the software mutex and then the hardware
	 * mutexes before being able to use the busses.
	 */
	mtx_lock(&xgbe_phy_comm_lock);

	/* Clear the mutexes */
	XP_IOWRITE(pdata, XP_I2C_MUTEX, XGBE_MUTEX_RELEASE);
	XP_IOWRITE(pdata, XP_MDIO_MUTEX, XGBE_MUTEX_RELEASE);

	/* Mutex formats are the same for I2C and MDIO/GPIO */
	mutex_id = 0;
	XP_SET_BITS(mutex_id, XP_I2C_MUTEX, ID, phy_data->port_id);
	XP_SET_BITS(mutex_id, XP_I2C_MUTEX, ACTIVE, 1);

	timeout = ticks + (5 * hz);
	while (ticks < timeout) {
		/* Must be all zeroes in order to obtain the mutex */
		if (XP_IOREAD(pdata, XP_I2C_MUTEX) ||
		    XP_IOREAD(pdata, XP_MDIO_MUTEX)) {
			DELAY(200);
			continue;
		}

		/* Obtain the mutex */
		XP_IOWRITE(pdata, XP_I2C_MUTEX, mutex_id);
		XP_IOWRITE(pdata, XP_MDIO_MUTEX, mutex_id);

		return (0);
	}

	mtx_unlock(&xgbe_phy_comm_lock);

	axgbe_error("unable to obtain hardware mutexes\n");

	return (-ETIMEDOUT);
}

static int
xgbe_phy_mdio_mii_write(struct xgbe_prv_data *pdata, int addr, int reg,
    uint16_t val)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	if (reg & MII_ADDR_C45) {
		if (phy_data->phydev_mode != XGBE_MDIO_MODE_CL45)
			return (-ENOTSUP);
	} else {
		if (phy_data->phydev_mode != XGBE_MDIO_MODE_CL22)
			return (-ENOTSUP);
	}

	return (pdata->hw_if.write_ext_mii_regs(pdata, addr, reg, val));
}

static int
xgbe_phy_i2c_mii_write(struct xgbe_prv_data *pdata, int reg, uint16_t val)
{
	__be16 *mii_val;
	uint8_t mii_data[3];
	int ret;

	ret = xgbe_phy_sfp_get_mux(pdata);
	if (ret)
		return (ret);

	mii_data[0] = reg & 0xff;
	mii_val = (__be16 *)&mii_data[1];
	*mii_val = cpu_to_be16(val);

	ret = xgbe_phy_i2c_write(pdata, XGBE_SFP_PHY_ADDRESS,
				 mii_data, sizeof(mii_data));

	xgbe_phy_sfp_put_mux(pdata);

	return (ret);
}

int
xgbe_phy_mii_write(struct xgbe_prv_data *pdata, int addr, int reg, uint16_t val)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	int ret;

	axgbe_printf(3, "%s: addr %d reg %d val %#x\n", __func__, addr, reg, val);
	ret = xgbe_phy_get_comm_ownership(pdata);
	if (ret)
		return (ret);

	if (phy_data->conn_type == XGBE_CONN_TYPE_SFP)
		ret = xgbe_phy_i2c_mii_write(pdata, reg, val);
	else if (phy_data->conn_type & XGBE_CONN_TYPE_MDIO)
		ret = xgbe_phy_mdio_mii_write(pdata, addr, reg, val);
	else
		ret = -ENOTSUP;

	xgbe_phy_put_comm_ownership(pdata);

	return (ret);
}

static int
xgbe_phy_mdio_mii_read(struct xgbe_prv_data *pdata, int addr, int reg)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	if (reg & MII_ADDR_C45) {
		if (phy_data->phydev_mode != XGBE_MDIO_MODE_CL45)
			return (-ENOTSUP);
	} else {
		if (phy_data->phydev_mode != XGBE_MDIO_MODE_CL22)
			return (-ENOTSUP);
	}

	return (pdata->hw_if.read_ext_mii_regs(pdata, addr, reg));
}

static int
xgbe_phy_i2c_mii_read(struct xgbe_prv_data *pdata, int reg)
{
	__be16 mii_val;
	uint8_t mii_reg;
	int ret;

	ret = xgbe_phy_sfp_get_mux(pdata);
	if (ret)
		return (ret);

	mii_reg = reg;
	ret = xgbe_phy_i2c_read(pdata, XGBE_SFP_PHY_ADDRESS,
				&mii_reg, sizeof(mii_reg),
				&mii_val, sizeof(mii_val));
	if (!ret)
		ret = be16_to_cpu(mii_val);

	xgbe_phy_sfp_put_mux(pdata);

	return (ret);
}

int
xgbe_phy_mii_read(struct xgbe_prv_data *pdata, int addr, int reg)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	int ret;

	axgbe_printf(3, "%s: addr %d reg %d\n", __func__, addr, reg);
	ret = xgbe_phy_get_comm_ownership(pdata);
	if (ret)
		return (ret);

	if (phy_data->conn_type == XGBE_CONN_TYPE_SFP)
		ret = xgbe_phy_i2c_mii_read(pdata, reg);
	else if (phy_data->conn_type & XGBE_CONN_TYPE_MDIO)
		ret = xgbe_phy_mdio_mii_read(pdata, addr, reg);
	else
		ret = -ENOTSUP;

	xgbe_phy_put_comm_ownership(pdata);

	return (ret);
}

static void
xgbe_phy_sfp_phy_settings(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	if (!phy_data->sfp_mod_absent && !phy_data->sfp_changed)
		return;

	XGBE_ZERO_SUP(&pdata->phy);

	if (phy_data->sfp_mod_absent) {
		pdata->phy.speed = SPEED_UNKNOWN;
		pdata->phy.duplex = DUPLEX_UNKNOWN;
		pdata->phy.autoneg = AUTONEG_ENABLE;
		pdata->phy.pause_autoneg = AUTONEG_ENABLE;

		XGBE_SET_SUP(&pdata->phy, Autoneg);
		XGBE_SET_SUP(&pdata->phy, Pause);
		XGBE_SET_SUP(&pdata->phy, Asym_Pause);
		XGBE_SET_SUP(&pdata->phy, TP);
		XGBE_SET_SUP(&pdata->phy, FIBRE);

		XGBE_LM_COPY(&pdata->phy, advertising, &pdata->phy, supported);

		return;
	}

	switch (phy_data->sfp_base) {
	case XGBE_SFP_BASE_100_FX:
	case XGBE_SFP_BASE_100_LX10:
	case XGBE_SFP_BASE_100_BX:
		pdata->phy.speed = SPEED_100;
		pdata->phy.duplex = DUPLEX_FULL;
		pdata->phy.autoneg = AUTONEG_DISABLE;
		pdata->phy.pause_autoneg = AUTONEG_DISABLE;
		break;
	case XGBE_SFP_BASE_1000_T:
	case XGBE_SFP_BASE_1000_SX:
	case XGBE_SFP_BASE_1000_LX:
	case XGBE_SFP_BASE_1000_CX:
		pdata->phy.speed = SPEED_UNKNOWN;
		pdata->phy.duplex = DUPLEX_UNKNOWN;
		pdata->phy.autoneg = AUTONEG_ENABLE;
		pdata->phy.pause_autoneg = AUTONEG_ENABLE;
		XGBE_SET_SUP(&pdata->phy, Autoneg);
		XGBE_SET_SUP(&pdata->phy, Pause);
		XGBE_SET_SUP(&pdata->phy, Asym_Pause);
		if (phy_data->sfp_base == XGBE_SFP_BASE_1000_T) {
			if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_100)
				XGBE_SET_SUP(&pdata->phy, 100baseT_Full);
			if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_1000)
				XGBE_SET_SUP(&pdata->phy, 1000baseT_Full);
		} else {
			if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_1000)
				XGBE_SET_SUP(&pdata->phy, 1000baseX_Full);
		}
		break;
	case XGBE_SFP_BASE_1000_BX:
	case XGBE_SFP_BASE_PX:
		pdata->phy.speed = SPEED_1000;
		pdata->phy.duplex = DUPLEX_FULL;
		pdata->phy.autoneg = AUTONEG_DISABLE;
		pdata->phy.pause_autoneg = AUTONEG_DISABLE;
		break;
	case XGBE_SFP_BASE_10000_SR:
	case XGBE_SFP_BASE_10000_LR:
	case XGBE_SFP_BASE_10000_LRM:
	case XGBE_SFP_BASE_10000_ER:
	case XGBE_SFP_BASE_10000_CR:
		pdata->phy.speed = SPEED_10000;
		pdata->phy.duplex = DUPLEX_FULL;
		pdata->phy.autoneg = AUTONEG_DISABLE;
		pdata->phy.pause_autoneg = AUTONEG_DISABLE;
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_10000) {
			switch (phy_data->sfp_base) {
			case XGBE_SFP_BASE_10000_SR:
				XGBE_SET_SUP(&pdata->phy, 10000baseSR_Full);
				break;
			case XGBE_SFP_BASE_10000_LR:
				XGBE_SET_SUP(&pdata->phy, 10000baseLR_Full);
				break;
			case XGBE_SFP_BASE_10000_LRM:
				XGBE_SET_SUP(&pdata->phy, 10000baseLRM_Full);
				break;
			case XGBE_SFP_BASE_10000_ER:
				XGBE_SET_SUP(&pdata->phy, 10000baseER_Full);
				break;
			case XGBE_SFP_BASE_10000_CR:
				XGBE_SET_SUP(&pdata->phy, 10000baseCR_Full);
				break;
			default:
				break;
			}
		}
		break;
	default:
		pdata->phy.speed = SPEED_UNKNOWN;
		pdata->phy.duplex = DUPLEX_UNKNOWN;
		pdata->phy.autoneg = AUTONEG_DISABLE;
		pdata->phy.pause_autoneg = AUTONEG_DISABLE;
		break;
	}

	switch (phy_data->sfp_base) {
	case XGBE_SFP_BASE_1000_T:
	case XGBE_SFP_BASE_1000_CX:
	case XGBE_SFP_BASE_10000_CR:
		XGBE_SET_SUP(&pdata->phy, TP);
		break;
	default:
		XGBE_SET_SUP(&pdata->phy, FIBRE);
		break;
	}

	XGBE_LM_COPY(&pdata->phy, advertising, &pdata->phy, supported);

	axgbe_printf(1, "%s: link speed %d spf_base 0x%x pause_autoneg %d "
	    "advert 0x%x support 0x%x\n", __func__, pdata->phy.speed,
	    phy_data->sfp_base, pdata->phy.pause_autoneg,
	    pdata->phy.advertising, pdata->phy.supported);
}

static bool
xgbe_phy_sfp_bit_rate(struct xgbe_sfp_eeprom *sfp_eeprom,
    enum xgbe_sfp_speed sfp_speed)
{
	uint8_t *sfp_base, min, max;

	sfp_base = sfp_eeprom->base;

	switch (sfp_speed) {
	case XGBE_SFP_SPEED_100:
		min = XGBE_SFP_BASE_BR_100M_MIN;
		max = XGBE_SFP_BASE_BR_100M_MAX;
		break;
	case XGBE_SFP_SPEED_1000:
		min = XGBE_SFP_BASE_BR_1GBE_MIN;
		max = XGBE_SFP_BASE_BR_1GBE_MAX;
		break;
	case XGBE_SFP_SPEED_10000:
		min = XGBE_SFP_BASE_BR_10GBE_MIN;
		max = XGBE_SFP_BASE_BR_10GBE_MAX;
		break;
	case XGBE_SFP_SPEED_25000:
		min = XGBE_SFP_BASE_BR_25GBE;
		max = XGBE_SFP_BASE_BR_25GBE;
		break;
	default:
		return (false);
	}

	return ((sfp_base[XGBE_SFP_BASE_BR] >= min) &&
		(sfp_base[XGBE_SFP_BASE_BR] <= max));
}

static void
xgbe_phy_free_phy_device(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	if (phy_data->phydev)
		phy_data->phydev = 0;

	if (pdata->axgbe_miibus != NULL) {
		device_delete_child(pdata->dev, pdata->axgbe_miibus);
		pdata->axgbe_miibus = NULL;
	}
}

static bool
xgbe_phy_finisar_phy_quirks(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	unsigned int phy_id = phy_data->phy_id;

	if (phy_data->port_mode != XGBE_PORT_MODE_SFP)
		return (false);

	if ((phy_id & 0xfffffff0) != 0x01ff0cc0)
		return (false);

	/* Enable Base-T AN */
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x16, 0x0001);
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x00, 0x9140);
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x16, 0x0000);

	/* Enable SGMII at 100Base-T/1000Base-T Full Duplex */
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x1b, 0x9084);
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x09, 0x0e00);
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x00, 0x8140);
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x04, 0x0d01);
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x00, 0x9140);

	axgbe_printf(3, "Finisar PHY quirk in place\n");

	return (true);
}

static bool
xgbe_phy_belfuse_phy_quirks(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	struct xgbe_sfp_eeprom *sfp_eeprom = &phy_data->sfp_eeprom;
	unsigned int phy_id = phy_data->phy_id;
	int reg;

	if (phy_data->port_mode != XGBE_PORT_MODE_SFP)
		return (false);

	if (memcmp(&sfp_eeprom->base[XGBE_SFP_BASE_VENDOR_NAME],
		   XGBE_BEL_FUSE_VENDOR, XGBE_SFP_BASE_VENDOR_NAME_LEN))
		return (false);

	/* For Bel-Fuse, use the extra AN flag */
	pdata->an_again = 1;

	if (memcmp(&sfp_eeprom->base[XGBE_SFP_BASE_VENDOR_PN],
		   XGBE_BEL_FUSE_PARTNO, XGBE_SFP_BASE_VENDOR_PN_LEN))
		return (false);

	if ((phy_id & 0xfffffff0) != 0x03625d10)
		return (false);

	/* Disable RGMII mode */
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x18, 0x7007);
	reg = xgbe_phy_mii_read(pdata, phy_data->mdio_addr, 0x18);
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x18, reg & ~0x0080);

	/* Enable fiber register bank */
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x1c, 0x7c00);
	reg = xgbe_phy_mii_read(pdata, phy_data->mdio_addr, 0x1c);
	reg &= 0x03ff;
	reg &= ~0x0001;
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x1c, 0x8000 | 0x7c00 |
	    reg | 0x0001);

	/* Power down SerDes */
	reg = xgbe_phy_mii_read(pdata, phy_data->mdio_addr, 0x00);
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x00, reg | 0x00800);

	/* Configure SGMII-to-Copper mode */
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x1c, 0x7c00);
	reg = xgbe_phy_mii_read(pdata, phy_data->mdio_addr, 0x1c);
	reg &= 0x03ff;
	reg &= ~0x0006;
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x1c, 0x8000 | 0x7c00 |
	    reg | 0x0004);

	/* Power up SerDes */
	reg = xgbe_phy_mii_read(pdata, phy_data->mdio_addr, 0x00);
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x00, reg & ~0x00800);

	/* Enable copper register bank */
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x1c, 0x7c00);
	reg = xgbe_phy_mii_read(pdata, phy_data->mdio_addr, 0x1c);
	reg &= 0x03ff;
	reg &= ~0x0001;
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x1c, 0x8000 | 0x7c00 |
	    reg);

	/* Power up SerDes */
	reg = xgbe_phy_mii_read(pdata, phy_data->mdio_addr, 0x00);
	xgbe_phy_mii_write(pdata, phy_data->mdio_addr, 0x00, reg & ~0x00800);

	axgbe_printf(3, "BelFuse PHY quirk in place\n");

	return (true);
}

static void
xgbe_phy_external_phy_quirks(struct xgbe_prv_data *pdata)
{
	if (xgbe_phy_belfuse_phy_quirks(pdata))
		return;

	if (xgbe_phy_finisar_phy_quirks(pdata))
		return;
}

static int
xgbe_get_phy_id(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	uint32_t oui, model, phy_id1, phy_id2;
	int phy_reg;

	phy_reg = xgbe_phy_mii_read(pdata, phy_data->mdio_addr, 0x02);
	if (phy_reg < 0)
		return (-EIO);

	phy_id1 = (phy_reg & 0xffff);
	phy_data->phy_id = (phy_reg & 0xffff) << 16;

	phy_reg = xgbe_phy_mii_read(pdata, phy_data->mdio_addr, 0x03);
	if (phy_reg < 0)
		return (-EIO);

	phy_id2 = (phy_reg & 0xffff);
	phy_data->phy_id |= (phy_reg & 0xffff);

	oui = MII_OUI(phy_id1, phy_id2);
	model = MII_MODEL(phy_id2);

	axgbe_printf(2, "%s: phy_id1: 0x%x phy_id2: 0x%x oui: %#x model %#x\n",
	    __func__, phy_id1, phy_id2, oui, model);

	return (0);
}

static int
xgbe_phy_find_phy_device(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	int ret;

	axgbe_printf(2, "%s: phydev %d phydev_mode %d sfp_phy_avail %d phy_id "
	    "0x%08x\n", __func__, phy_data->phydev, phy_data->phydev_mode,
	    phy_data->sfp_phy_avail, phy_data->phy_id);

	/* If we already have a PHY, just return */
	if (phy_data->phydev) {
		axgbe_printf(3, "%s: phy present already\n", __func__);
		return (0);
	}

	/* Clear the extra AN flag */
	pdata->an_again = 0;

	/* Check for the use of an external PHY */
	if (phy_data->phydev_mode == XGBE_MDIO_MODE_NONE) {
		axgbe_printf(3, "%s: phydev_mode %d\n", __func__,
		    phy_data->phydev_mode);
		return (0);
	}

	/* For SFP, only use an external PHY if available */
	if ((phy_data->port_mode == XGBE_PORT_MODE_SFP) &&
	    !phy_data->sfp_phy_avail) {
		axgbe_printf(3, "%s: port_mode %d avail %d\n", __func__,
		    phy_data->port_mode, phy_data->sfp_phy_avail);
		return (0);
	}

	/* Set the proper MDIO mode for the PHY */
	ret = pdata->hw_if.set_ext_mii_mode(pdata, phy_data->mdio_addr,
	    phy_data->phydev_mode);
	if (ret) {
		axgbe_error("mdio port/clause not compatible (%u/%u) ret %d\n",
		    phy_data->mdio_addr, phy_data->phydev_mode, ret);
		return (ret);
	}

	ret = xgbe_get_phy_id(pdata);
	if (ret)
		return (ret);
	axgbe_printf(2, "Get phy_id 0x%08x\n", phy_data->phy_id);

	phy_data->phydev = 1;
	xgbe_phy_external_phy_quirks(pdata);

	return (0);
}

static void
xgbe_phy_sfp_external_phy(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	int ret;

	axgbe_printf(3, "%s: sfp_changed: 0x%x\n", __func__,
	    phy_data->sfp_changed);
	if (!phy_data->sfp_phy_retries && !phy_data->sfp_changed)
		return;

	phy_data->sfp_phy_avail = 0;

	if (phy_data->sfp_base != XGBE_SFP_BASE_1000_T)
		return;

	/* Check access to the PHY by reading CTRL1 */
	ret = xgbe_phy_i2c_mii_read(pdata, MII_BMCR);
	if (ret < 0) {
		phy_data->sfp_phy_retries++;
		if (phy_data->sfp_phy_retries >= XGBE_SFP_PHY_RETRY_MAX)
			phy_data->sfp_phy_retries = 0;
		axgbe_printf(1, "%s: ext phy fail %d. retrying.\n", __func__, ret);
		return;
	}

	/* Successfully accessed the PHY */
	phy_data->sfp_phy_avail = 1;
	axgbe_printf(3, "Successfully accessed External PHY\n");

	/* Attach external PHY to the miibus */
	ret = mii_attach(pdata->dev, &pdata->axgbe_miibus, pdata->netdev,
		(ifm_change_cb_t)axgbe_ifmedia_upd,
		(ifm_stat_cb_t)axgbe_ifmedia_sts, BMSR_DEFCAPMASK,
		pdata->mdio_addr, MII_OFFSET_ANY, MIIF_FORCEANEG);

	if (ret) {
		axgbe_error("mii attach failed with err=(%d)\n", ret);
	}
}

static bool
xgbe_phy_check_sfp_rx_los(struct xgbe_phy_data *phy_data)
{
	uint8_t *sfp_extd = phy_data->sfp_eeprom.extd;

	if (!(sfp_extd[XGBE_SFP_EXTD_OPT1] & XGBE_SFP_EXTD_OPT1_RX_LOS))
		return (false);

	if (phy_data->sfp_gpio_mask & XGBE_GPIO_NO_RX_LOS)
		return (false);

	if (phy_data->sfp_gpio_inputs & (1 << phy_data->sfp_gpio_rx_los))
		return (true);

	return (false);
}

static bool
xgbe_phy_check_sfp_tx_fault(struct xgbe_phy_data *phy_data)
{
	uint8_t *sfp_extd = phy_data->sfp_eeprom.extd;

	if (!(sfp_extd[XGBE_SFP_EXTD_OPT1] & XGBE_SFP_EXTD_OPT1_TX_FAULT))
		return (false);

	if (phy_data->sfp_gpio_mask & XGBE_GPIO_NO_TX_FAULT)
		return (false);

	if (phy_data->sfp_gpio_inputs & (1 << phy_data->sfp_gpio_tx_fault))
		return (true);

	return (false);
}

static bool
xgbe_phy_check_sfp_mod_absent(struct xgbe_phy_data *phy_data)
{
	if (phy_data->sfp_gpio_mask & XGBE_GPIO_NO_MOD_ABSENT)
		return (false);

	if (phy_data->sfp_gpio_inputs & (1 << phy_data->sfp_gpio_mod_absent))
		return (true);

	return (false);
}

static void
xgbe_phy_sfp_parse_eeprom(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	struct xgbe_sfp_eeprom *sfp_eeprom = &phy_data->sfp_eeprom;
	uint8_t *sfp_base;
	uint16_t wavelen = 0;

	sfp_base = sfp_eeprom->base;

	if (sfp_base[XGBE_SFP_BASE_ID] != XGBE_SFP_ID_SFP) {
		axgbe_error("base id %d\n", sfp_base[XGBE_SFP_BASE_ID]);
		return;
	}

	if (sfp_base[XGBE_SFP_BASE_EXT_ID] != XGBE_SFP_EXT_ID_SFP) {
		axgbe_error("base id %d\n", sfp_base[XGBE_SFP_BASE_EXT_ID]);
		return;
	}

	/* Update transceiver signals (eeprom extd/options) */
	phy_data->sfp_tx_fault = xgbe_phy_check_sfp_tx_fault(phy_data);
	phy_data->sfp_rx_los = xgbe_phy_check_sfp_rx_los(phy_data);

	/* Assume ACTIVE cable unless told it is PASSIVE */
	if (sfp_base[XGBE_SFP_BASE_CABLE] & XGBE_SFP_BASE_CABLE_PASSIVE) {
		phy_data->sfp_cable = XGBE_SFP_CABLE_PASSIVE;
		phy_data->sfp_cable_len = sfp_base[XGBE_SFP_BASE_CU_CABLE_LEN];
	} else
		phy_data->sfp_cable = XGBE_SFP_CABLE_ACTIVE;

	wavelen = (sfp_base[XGBE_SFP_BASE_OSC] << 8) | sfp_base[XGBE_SFP_BASE_OSC + 1];

	/*
	 * Determine the type of SFP. Certain 10G SFP+ modules read as
	 * 1000BASE-CX. To prevent 10G DAC cables to be recognized as
	 * 1G, we first check if it is a DAC and the bitrate is 10G.
	 * If it's greater than 10G, we assume the DAC is capable
	 * of multiple bitrates, set the MAC to 10G and hope for the best.
	 */
	if (((sfp_base[XGBE_SFP_BASE_CV] & XGBE_SFP_BASE_CV_CP) ||
		(phy_data->sfp_cable == XGBE_SFP_CABLE_PASSIVE)) &&
		(xgbe_phy_sfp_bit_rate(sfp_eeprom, XGBE_SFP_SPEED_10000) ||
		xgbe_phy_sfp_bit_rate(sfp_eeprom, XGBE_SFP_SPEED_25000)))
		phy_data->sfp_base = XGBE_SFP_BASE_10000_CR;
	else if (sfp_base[XGBE_SFP_BASE_10GBE_CC] & XGBE_SFP_BASE_10GBE_CC_SR)
		phy_data->sfp_base = XGBE_SFP_BASE_10000_SR;
	else if (sfp_base[XGBE_SFP_BASE_10GBE_CC] & XGBE_SFP_BASE_10GBE_CC_LR)
		phy_data->sfp_base = XGBE_SFP_BASE_10000_LR;
	else if (sfp_base[XGBE_SFP_BASE_10GBE_CC] & XGBE_SFP_BASE_10GBE_CC_LRM)
		phy_data->sfp_base = XGBE_SFP_BASE_10000_LRM;
	else if (sfp_base[XGBE_SFP_BASE_10GBE_CC] & XGBE_SFP_BASE_10GBE_CC_ER)
		phy_data->sfp_base = XGBE_SFP_BASE_10000_ER;
	else if (sfp_base[XGBE_SFP_BASE_1GBE_CC] & XGBE_SFP_BASE_1GBE_CC_SX)
		phy_data->sfp_base = XGBE_SFP_BASE_1000_SX;
	else if (sfp_base[XGBE_SFP_BASE_1GBE_CC] & XGBE_SFP_BASE_1GBE_CC_LX)
		phy_data->sfp_base = XGBE_SFP_BASE_1000_LX;
	else if (sfp_base[XGBE_SFP_BASE_1GBE_CC] & XGBE_SFP_BASE_1GBE_CC_CX)
		phy_data->sfp_base = XGBE_SFP_BASE_1000_CX;
	else if (sfp_base[XGBE_SFP_BASE_1GBE_CC] & XGBE_SFP_BASE_1GBE_CC_T)
		phy_data->sfp_base = XGBE_SFP_BASE_1000_T;
	else if (sfp_base[XGBE_SFP_BASE_1GBE_CC] & XGBE_SFP_BASE_100M_CC_LX10)
		phy_data->sfp_base = XGBE_SFP_BASE_100_LX10;
	else if (sfp_base[XGBE_SFP_BASE_1GBE_CC] & XGBE_SFP_BASE_100M_CC_FX)
		phy_data->sfp_base = XGBE_SFP_BASE_100_FX;
	else if (sfp_base[XGBE_SFP_BASE_1GBE_CC] & XGBE_SFP_BASE_CC_BX10) {
		/* BX10 can be either 100 or 1000 */
		if (xgbe_phy_sfp_bit_rate(sfp_eeprom, XGBE_SFP_SPEED_100)) {
			phy_data->sfp_base = XGBE_SFP_BASE_100_BX;
		} else {
			/* default to 1000 */
			phy_data->sfp_base = XGBE_SFP_BASE_1000_BX;
		}
	} else if (sfp_base[XGBE_SFP_BASE_1GBE_CC] & XGBE_SFP_BASE_CC_PX)
		phy_data->sfp_base = XGBE_SFP_BASE_PX;
	else if (xgbe_phy_sfp_bit_rate(sfp_eeprom, XGBE_SFP_SPEED_1000)
			&& (sfp_base[XGBE_SFP_BASE_SM_LEN_KM] >= XGBE_SFP_BASE_SM_LEN_KM_MIN
			|| sfp_base[XGBE_SFP_BASE_SM_LEN_100M] >= XGBE_SFP_BASE_SM_LEN_100M_MIN)
			&& wavelen >= XGBE_SFP_BASE_OSC_1310)
		phy_data->sfp_base = XGBE_SFP_BASE_1000_BX;
	else if (xgbe_phy_sfp_bit_rate(sfp_eeprom, XGBE_SFP_SPEED_100)
			&& (sfp_base[XGBE_SFP_BASE_SM_LEN_KM] >= XGBE_SFP_BASE_SM_LEN_KM_MIN
			|| sfp_base[XGBE_SFP_BASE_SM_LEN_100M] >= XGBE_SFP_BASE_SM_LEN_100M_MIN)
			&& wavelen >= XGBE_SFP_BASE_OSC_1310)
		phy_data->sfp_base = XGBE_SFP_BASE_100_BX;

	switch (phy_data->sfp_base) {
	case XGBE_SFP_BASE_100_FX:
	case XGBE_SFP_BASE_100_LX10:
	case XGBE_SFP_BASE_100_BX:
		phy_data->sfp_speed = XGBE_SFP_SPEED_100;
	case XGBE_SFP_BASE_1000_T:
		phy_data->sfp_speed = XGBE_SFP_SPEED_100_1000;
		break;
	case XGBE_SFP_BASE_PX:
	case XGBE_SFP_BASE_1000_SX:
	case XGBE_SFP_BASE_1000_LX:
	case XGBE_SFP_BASE_1000_CX:
	case XGBE_SFP_BASE_1000_BX:
		phy_data->sfp_speed = XGBE_SFP_SPEED_1000;
		break;
	case XGBE_SFP_BASE_10000_SR:
	case XGBE_SFP_BASE_10000_LR:
	case XGBE_SFP_BASE_10000_LRM:
	case XGBE_SFP_BASE_10000_ER:
	case XGBE_SFP_BASE_10000_CR:
		phy_data->sfp_speed = XGBE_SFP_SPEED_10000;
		break;
	default:
		break;
	}
	axgbe_printf(3, "%s: sfp_base: 0x%x sfp_speed: 0x%x sfp_cable: 0x%x "
	    "rx_los 0x%x tx_fault 0x%x\n", __func__, phy_data->sfp_base,
	    phy_data->sfp_speed, phy_data->sfp_cable, phy_data->sfp_rx_los,
	    phy_data->sfp_tx_fault);
}

static void
xgbe_phy_sfp_eeprom_info(struct xgbe_prv_data *pdata,
    struct xgbe_sfp_eeprom *sfp_eeprom)
{
	struct xgbe_sfp_ascii sfp_ascii;
	char *sfp_data = (char *)&sfp_ascii;

	axgbe_printf(0, "SFP detected:\n");
	memcpy(sfp_data, &sfp_eeprom->base[XGBE_SFP_BASE_VENDOR_NAME],
	       XGBE_SFP_BASE_VENDOR_NAME_LEN);
	sfp_data[XGBE_SFP_BASE_VENDOR_NAME_LEN] = '\0';
	axgbe_printf(0, "  vendor:	 %s\n",
	    sfp_data);

	memcpy(sfp_data, &sfp_eeprom->base[XGBE_SFP_BASE_VENDOR_PN],
	       XGBE_SFP_BASE_VENDOR_PN_LEN);
	sfp_data[XGBE_SFP_BASE_VENDOR_PN_LEN] = '\0';
	axgbe_printf(0, "  part number:    %s\n",
	    sfp_data);

	memcpy(sfp_data, &sfp_eeprom->base[XGBE_SFP_BASE_VENDOR_REV],
	       XGBE_SFP_BASE_VENDOR_REV_LEN);
	sfp_data[XGBE_SFP_BASE_VENDOR_REV_LEN] = '\0';
	axgbe_printf(0, "  revision level: %s\n",
	    sfp_data);

	memcpy(sfp_data, &sfp_eeprom->extd[XGBE_SFP_BASE_VENDOR_SN],
	       XGBE_SFP_BASE_VENDOR_SN_LEN);
	sfp_data[XGBE_SFP_BASE_VENDOR_SN_LEN] = '\0';
	axgbe_printf(0, "  serial number:  %s\n",
	    sfp_data);
}

static bool
xgbe_phy_sfp_verify_eeprom(uint8_t cc_in, uint8_t *buf, unsigned int len)
{
	uint8_t cc;

	for (cc = 0; len; buf++, len--)
		cc += *buf;

	return ((cc == cc_in) ? true : false);
}

static void
dump_sfp_eeprom(struct xgbe_prv_data *pdata, uint8_t *sfp_base)
{
	axgbe_printf(3, "sfp_base[XGBE_SFP_BASE_ID]     : 0x%04x\n",
	    sfp_base[XGBE_SFP_BASE_ID]);
	axgbe_printf(3, "sfp_base[XGBE_SFP_BASE_EXT_ID] : 0x%04x\n",
	    sfp_base[XGBE_SFP_BASE_EXT_ID]);
	axgbe_printf(3, "sfp_base[XGBE_SFP_BASE_CABLE]  : 0x%04x\n",
	    sfp_base[XGBE_SFP_BASE_CABLE]);
}

static int
xgbe_phy_sfp_read_eeprom(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	struct xgbe_sfp_eeprom sfp_eeprom, *eeprom;
	uint8_t eeprom_addr, *base;
	int ret;

	ret = xgbe_phy_sfp_get_mux(pdata);
	if (ret) {
		axgbe_error("I2C error setting SFP MUX\n");
		return (ret);
	}

	/* Read the SFP serial ID eeprom */
	eeprom_addr = 0;
	ret = xgbe_phy_i2c_read(pdata, XGBE_SFP_SERIAL_ID_ADDRESS,
	    &eeprom_addr, sizeof(eeprom_addr),
	    &sfp_eeprom, sizeof(sfp_eeprom));

	if (ret) {
		axgbe_error("I2C error reading SFP EEPROM\n");
		goto put;
	}

	eeprom = &sfp_eeprom;
	base = eeprom->base;
	dump_sfp_eeprom(pdata, base);

	/* Validate the contents read */
	if (!xgbe_phy_sfp_verify_eeprom(sfp_eeprom.base[XGBE_SFP_BASE_CC],
	    sfp_eeprom.base, sizeof(sfp_eeprom.base) - 1)) {
		axgbe_error("verify eeprom base failed\n");
		ret = -EINVAL;
		goto put;
	}

	if (!xgbe_phy_sfp_verify_eeprom(sfp_eeprom.extd[XGBE_SFP_EXTD_CC],
	    sfp_eeprom.extd, sizeof(sfp_eeprom.extd) - 1)) {
		axgbe_error("verify eeprom extd failed\n");
		ret = -EINVAL;
		goto put;
	}

	/* Check for an added or changed SFP */
	if (memcmp(&phy_data->sfp_eeprom, &sfp_eeprom, sizeof(sfp_eeprom))) {
		phy_data->sfp_changed = 1;

		xgbe_phy_sfp_eeprom_info(pdata, &sfp_eeprom);

		memcpy(&phy_data->sfp_eeprom, &sfp_eeprom, sizeof(sfp_eeprom));

		xgbe_phy_free_phy_device(pdata);
	} else
		phy_data->sfp_changed = 0;

put:
	xgbe_phy_sfp_put_mux(pdata);

	return (ret);
}

static void
xgbe_phy_sfp_signals(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	uint8_t gpio_reg, gpio_ports[2];
	int ret, prev_sfp_inputs = phy_data->port_sfp_inputs;
	int shift = GPIO_MASK_WIDTH * (3 - phy_data->port_id);

	/* Read the input port registers */
	axgbe_printf(3, "%s: befor sfp_mod:%d sfp_gpio_address:0x%x\n",
	    __func__, phy_data->sfp_mod_absent, phy_data->sfp_gpio_address);

	gpio_reg = 0;
	ret = xgbe_phy_i2c_read(pdata, phy_data->sfp_gpio_address, &gpio_reg,
	    sizeof(gpio_reg), gpio_ports, sizeof(gpio_ports));
	if (ret) {
		axgbe_error("%s: I2C error reading SFP GPIO addr:0x%x\n",
		    __func__, phy_data->sfp_gpio_address);
		return;
	}

	phy_data->sfp_gpio_inputs = (gpio_ports[1] << 8) | gpio_ports[0];
	phy_data->port_sfp_inputs = (phy_data->sfp_gpio_inputs >> shift) & 0x0F;

	if (prev_sfp_inputs != phy_data->port_sfp_inputs)
		axgbe_printf(0, "%s: port_sfp_inputs: 0x%0x\n", __func__,
		    phy_data->port_sfp_inputs);

	phy_data->sfp_mod_absent = xgbe_phy_check_sfp_mod_absent(phy_data);

	axgbe_printf(3, "%s: after sfp_mod:%d sfp_gpio_inputs:0x%x\n",
	    __func__, phy_data->sfp_mod_absent, phy_data->sfp_gpio_inputs);
}

static void
xgbe_phy_sfp_mod_absent(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	xgbe_phy_free_phy_device(pdata);

	phy_data->sfp_mod_absent = 1;
	phy_data->sfp_phy_avail = 0;
	memset(&phy_data->sfp_eeprom, 0, sizeof(phy_data->sfp_eeprom));
}

static void
xgbe_phy_sfp_reset(struct xgbe_phy_data *phy_data)
{
	phy_data->sfp_rx_los = 0;
	phy_data->sfp_tx_fault = 0;
	phy_data->sfp_mod_absent = 1;
	phy_data->sfp_base = XGBE_SFP_BASE_UNKNOWN;
	phy_data->sfp_cable = XGBE_SFP_CABLE_UNKNOWN;
	phy_data->sfp_speed = XGBE_SFP_SPEED_UNKNOWN;
}

static void
xgbe_phy_sfp_detect(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	int ret, prev_sfp_state = phy_data->sfp_mod_absent;

	/* Reset the SFP signals and info */
	xgbe_phy_sfp_reset(phy_data);

	ret = xgbe_phy_get_comm_ownership(pdata);
	if (ret)
		return;

	/* Read the SFP signals and check for module presence */
	xgbe_phy_sfp_signals(pdata);
	if (phy_data->sfp_mod_absent) {
		if (prev_sfp_state != phy_data->sfp_mod_absent)
			axgbe_error("%s: mod absent\n", __func__);
		xgbe_phy_sfp_mod_absent(pdata);
		goto put;
	}

	ret = xgbe_phy_sfp_read_eeprom(pdata);
	if (ret) {
		/* Treat any error as if there isn't an SFP plugged in */
		axgbe_error("%s: eeprom read failed\n", __func__);
		xgbe_phy_sfp_reset(phy_data);
		xgbe_phy_sfp_mod_absent(pdata);
		goto put;
	}

	xgbe_phy_sfp_parse_eeprom(pdata);

	xgbe_phy_sfp_external_phy(pdata);

put:
	xgbe_phy_sfp_phy_settings(pdata);

	axgbe_printf(3, "%s: phy speed: 0x%x duplex: 0x%x autoneg: 0x%x "
	    "pause_autoneg: 0x%x\n", __func__, pdata->phy.speed,
	    pdata->phy.duplex, pdata->phy.autoneg, pdata->phy.pause_autoneg);

	xgbe_phy_put_comm_ownership(pdata);
}

static int
xgbe_phy_module_eeprom(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	uint8_t eeprom_addr, eeprom_data[XGBE_SFP_EEPROM_MAX];
	struct xgbe_sfp_eeprom *sfp_eeprom;
	int ret;

	if (phy_data->port_mode != XGBE_PORT_MODE_SFP) {
		ret = -ENXIO;
		goto done;
	}

	if (phy_data->sfp_mod_absent) {
		ret = -EIO;
		goto done;
	}

	ret = xgbe_phy_get_comm_ownership(pdata);
	if (ret) {
		ret = -EIO;
		goto done;
	}

	ret = xgbe_phy_sfp_get_mux(pdata);
	if (ret) {
		axgbe_error("I2C error setting SFP MUX\n");
		ret = -EIO;
		goto put_own;
	}

	/* Read the SFP serial ID eeprom */
	eeprom_addr = 0;
	ret = xgbe_phy_i2c_read(pdata, XGBE_SFP_SERIAL_ID_ADDRESS,
				&eeprom_addr, sizeof(eeprom_addr),
				eeprom_data, XGBE_SFP_EEPROM_BASE_LEN);
	if (ret) {
		axgbe_error("I2C error reading SFP EEPROM\n");
		ret = -EIO;
		goto put_mux;
	}

	sfp_eeprom = (struct xgbe_sfp_eeprom *)eeprom_data;

	if (XGBE_SFP_DIAGS_SUPPORTED(sfp_eeprom)) {
		/* Read the SFP diagnostic eeprom */
		eeprom_addr = 0;
		ret = xgbe_phy_i2c_read(pdata, XGBE_SFP_DIAG_INFO_ADDRESS,
					&eeprom_addr, sizeof(eeprom_addr),
					eeprom_data + XGBE_SFP_EEPROM_BASE_LEN,
					XGBE_SFP_EEPROM_DIAG_LEN);
		if (ret) {
			axgbe_error("I2C error reading SFP DIAGS\n");
			ret = -EIO;
			goto put_mux;
		}
	}

put_mux:
	xgbe_phy_sfp_put_mux(pdata);

put_own:
	xgbe_phy_put_comm_ownership(pdata);

done:
	return (ret);
}

static int
xgbe_phy_module_info(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	if (phy_data->port_mode != XGBE_PORT_MODE_SFP)
		return (-ENXIO);

	if (phy_data->sfp_mod_absent)
		return (-EIO);

	return (0);
}

static void
xgbe_phy_phydev_flowctrl(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	pdata->phy.tx_pause = 0;
	pdata->phy.rx_pause = 0;

	if (!phy_data->phydev)
		return;

	if (pdata->phy.pause)
		XGBE_SET_LP_ADV(&pdata->phy, Pause);

	if (pdata->phy.asym_pause)
		XGBE_SET_LP_ADV(&pdata->phy, Asym_Pause);

	axgbe_printf(1, "%s: pause tx/rx %d/%d\n", __func__,
	    pdata->phy.tx_pause, pdata->phy.rx_pause);
}

static enum xgbe_mode
xgbe_phy_an37_sgmii_outcome(struct xgbe_prv_data *pdata)
{
	enum xgbe_mode mode;

	XGBE_SET_LP_ADV(&pdata->phy, Autoneg);
	XGBE_SET_LP_ADV(&pdata->phy, TP);

	axgbe_printf(1, "%s: pause_autoneg %d\n", __func__,
	    pdata->phy.pause_autoneg);

	/* Use external PHY to determine flow control */
	if (pdata->phy.pause_autoneg)
		xgbe_phy_phydev_flowctrl(pdata);

	switch (pdata->an_status & XGBE_SGMII_AN_LINK_SPEED) {
	case XGBE_SGMII_AN_LINK_SPEED_100:
		if (pdata->an_status & XGBE_SGMII_AN_LINK_DUPLEX) {
			XGBE_SET_LP_ADV(&pdata->phy, 100baseT_Full);
			mode = XGBE_MODE_SGMII_100;
		} else {
			/* Half-duplex not supported */
			XGBE_SET_LP_ADV(&pdata->phy, 100baseT_Half);
			mode = XGBE_MODE_UNKNOWN;
		}
		break;
	case XGBE_SGMII_AN_LINK_SPEED_1000:
	default:
		/* Default to 1000 */
		if (pdata->an_status & XGBE_SGMII_AN_LINK_DUPLEX) {
			XGBE_SET_LP_ADV(&pdata->phy, 1000baseT_Full);
			mode = XGBE_MODE_SGMII_1000;
		} else {
			/* Half-duplex not supported */
			XGBE_SET_LP_ADV(&pdata->phy, 1000baseT_Half);
			mode = XGBE_MODE_SGMII_1000;
		}
		break;
	}

	return (mode);
}

static enum xgbe_mode
xgbe_phy_an37_outcome(struct xgbe_prv_data *pdata)
{
	enum xgbe_mode mode;
	unsigned int ad_reg, lp_reg;

	XGBE_SET_LP_ADV(&pdata->phy, Autoneg);
	XGBE_SET_LP_ADV(&pdata->phy, FIBRE);

	/* Compare Advertisement and Link Partner register */
	ad_reg = XMDIO_READ(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_ADVERTISE);
	lp_reg = XMDIO_READ(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_LP_ABILITY);
	if (lp_reg & 0x100)
		XGBE_SET_LP_ADV(&pdata->phy, Pause);
	if (lp_reg & 0x80)
		XGBE_SET_LP_ADV(&pdata->phy, Asym_Pause);

	axgbe_printf(1, "%s: pause_autoneg %d ad_reg 0x%x lp_reg 0x%x\n",
	    __func__, pdata->phy.pause_autoneg, ad_reg, lp_reg);

	if (pdata->phy.pause_autoneg) {
		/* Set flow control based on auto-negotiation result */
		pdata->phy.tx_pause = 0;
		pdata->phy.rx_pause = 0;

		if (ad_reg & lp_reg & 0x100) {
			pdata->phy.tx_pause = 1;
			pdata->phy.rx_pause = 1;
		} else if (ad_reg & lp_reg & 0x80) {
			if (ad_reg & 0x100)
				pdata->phy.rx_pause = 1;
			else if (lp_reg & 0x100)
				pdata->phy.tx_pause = 1;
		}
	}

	axgbe_printf(1, "%s: pause tx/rx %d/%d\n", __func__, pdata->phy.tx_pause,
	    pdata->phy.rx_pause);

	if (lp_reg & 0x20)
		XGBE_SET_LP_ADV(&pdata->phy, 1000baseX_Full);

	/* Half duplex is not supported */
	ad_reg &= lp_reg;
	mode = (ad_reg & 0x20) ? XGBE_MODE_X : XGBE_MODE_UNKNOWN;

	return (mode);
}

static enum xgbe_mode
xgbe_phy_an73_redrv_outcome(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	enum xgbe_mode mode;
	unsigned int ad_reg, lp_reg;

	XGBE_SET_LP_ADV(&pdata->phy, Autoneg);
	XGBE_SET_LP_ADV(&pdata->phy, Backplane);

	axgbe_printf(1, "%s: pause_autoneg %d\n", __func__,
	    pdata->phy.pause_autoneg);

	/* Use external PHY to determine flow control */
	if (pdata->phy.pause_autoneg)
		xgbe_phy_phydev_flowctrl(pdata);

	/* Compare Advertisement and Link Partner register 2 */
	ad_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 1);
	lp_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_LPA + 1);
	if (lp_reg & 0x80)
		XGBE_SET_LP_ADV(&pdata->phy, 10000baseKR_Full);
	if (lp_reg & 0x20)
		XGBE_SET_LP_ADV(&pdata->phy, 1000baseKX_Full);

	ad_reg &= lp_reg;
	if (ad_reg & 0x80) {
		switch (phy_data->port_mode) {
		case XGBE_PORT_MODE_BACKPLANE:
			mode = XGBE_MODE_KR;
			break;
		default:
			mode = XGBE_MODE_SFI;
			break;
		}
	} else if (ad_reg & 0x20) {
		switch (phy_data->port_mode) {
		case XGBE_PORT_MODE_BACKPLANE:
			mode = XGBE_MODE_KX_1000;
			break;
		case XGBE_PORT_MODE_1000BASE_X:
			mode = XGBE_MODE_X;
			break;
		case XGBE_PORT_MODE_SFP:
			switch (phy_data->sfp_base) {
			case XGBE_SFP_BASE_1000_T:
				if ((phy_data->phydev) &&
				    (pdata->phy.speed == SPEED_100))
					mode = XGBE_MODE_SGMII_100;
				else
					mode = XGBE_MODE_SGMII_1000;
				break;
			case XGBE_SFP_BASE_1000_SX:
			case XGBE_SFP_BASE_1000_LX:
			case XGBE_SFP_BASE_1000_CX:
			default:
				mode = XGBE_MODE_X;
				break;
			}
			break;
		default:
			if ((phy_data->phydev) &&
			    (pdata->phy.speed == SPEED_100))
				mode = XGBE_MODE_SGMII_100;
			else
				mode = XGBE_MODE_SGMII_1000;
			break;
		}
	} else {
		mode = XGBE_MODE_UNKNOWN;
	}

	/* Compare Advertisement and Link Partner register 3 */
	ad_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 2);
	lp_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_LPA + 2);
	if (lp_reg & 0xc000)
		XGBE_SET_LP_ADV(&pdata->phy, 10000baseR_FEC);

	return (mode);
}

static enum xgbe_mode
xgbe_phy_an73_outcome(struct xgbe_prv_data *pdata)
{
	enum xgbe_mode mode;
	unsigned int ad_reg, lp_reg;

	XGBE_SET_LP_ADV(&pdata->phy, Autoneg);
	XGBE_SET_LP_ADV(&pdata->phy, Backplane);

	/* Compare Advertisement and Link Partner register 1 */
	ad_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE);
	lp_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_LPA);
	if (lp_reg & 0x400)
		XGBE_SET_LP_ADV(&pdata->phy, Pause);
	if (lp_reg & 0x800)
		XGBE_SET_LP_ADV(&pdata->phy, Asym_Pause);

	axgbe_printf(1, "%s: pause_autoneg %d ad_reg 0x%x lp_reg 0x%x\n",
	    __func__, pdata->phy.pause_autoneg, ad_reg, lp_reg);

	if (pdata->phy.pause_autoneg) {
		/* Set flow control based on auto-negotiation result */
		pdata->phy.tx_pause = 0;
		pdata->phy.rx_pause = 0;

		if (ad_reg & lp_reg & 0x400) {
			pdata->phy.tx_pause = 1;
			pdata->phy.rx_pause = 1;
		} else if (ad_reg & lp_reg & 0x800) {
			if (ad_reg & 0x400)
				pdata->phy.rx_pause = 1;
			else if (lp_reg & 0x400)
				pdata->phy.tx_pause = 1;
		}
	}

	axgbe_printf(1, "%s: pause tx/rx %d/%d\n", __func__, pdata->phy.tx_pause,
	    pdata->phy.rx_pause);

	/* Compare Advertisement and Link Partner register 2 */
	ad_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 1);
	lp_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_LPA + 1);
	if (lp_reg & 0x80)
		XGBE_SET_LP_ADV(&pdata->phy, 10000baseKR_Full);
	if (lp_reg & 0x20)
		XGBE_SET_LP_ADV(&pdata->phy, 1000baseKX_Full);

	ad_reg &= lp_reg;
	if (ad_reg & 0x80)
		mode = XGBE_MODE_KR;
	else if (ad_reg & 0x20)
		mode = XGBE_MODE_KX_1000;
	else
		mode = XGBE_MODE_UNKNOWN;

	/* Compare Advertisement and Link Partner register 3 */
	ad_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 2);
	lp_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_LPA + 2);
	if (lp_reg & 0xc000)
		XGBE_SET_LP_ADV(&pdata->phy, 10000baseR_FEC);

	return (mode);
}

static enum xgbe_mode
xgbe_phy_an_outcome(struct xgbe_prv_data *pdata)
{
	switch (pdata->an_mode) {
	case XGBE_AN_MODE_CL73:
		return (xgbe_phy_an73_outcome(pdata));
	case XGBE_AN_MODE_CL73_REDRV:
		return (xgbe_phy_an73_redrv_outcome(pdata));
	case XGBE_AN_MODE_CL37:
		return (xgbe_phy_an37_outcome(pdata));
	case XGBE_AN_MODE_CL37_SGMII:
		return (xgbe_phy_an37_sgmii_outcome(pdata));
	default:
		return (XGBE_MODE_UNKNOWN);
	}
}

static void
xgbe_phy_an_advertising(struct xgbe_prv_data *pdata, struct xgbe_phy *dphy)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	XGBE_LM_COPY(dphy, advertising, &pdata->phy, advertising);

	/* Without a re-driver, just return current advertising */
	if (!phy_data->redrv)
		return;

	/* With the KR re-driver we need to advertise a single speed */
	XGBE_CLR_ADV(dphy, 1000baseKX_Full);
	XGBE_CLR_ADV(dphy, 10000baseKR_Full);

	/* Advertise FEC support is present */
	if (pdata->fec_ability & MDIO_PMA_10GBR_FECABLE_ABLE)
		XGBE_SET_ADV(dphy, 10000baseR_FEC);

	switch (phy_data->port_mode) {
	case XGBE_PORT_MODE_BACKPLANE:
		XGBE_SET_ADV(dphy, 10000baseKR_Full);
		break;
	case XGBE_PORT_MODE_BACKPLANE_2500:
		XGBE_SET_ADV(dphy, 1000baseKX_Full);
		break;
	case XGBE_PORT_MODE_1000BASE_T:
	case XGBE_PORT_MODE_1000BASE_X:
	case XGBE_PORT_MODE_NBASE_T:
		XGBE_SET_ADV(dphy, 1000baseKX_Full);
		break;
	case XGBE_PORT_MODE_10GBASE_T:
		if ((phy_data->phydev) &&
		    (pdata->phy.speed == SPEED_10000))
			XGBE_SET_ADV(dphy, 10000baseKR_Full);
		else
			XGBE_SET_ADV(dphy, 1000baseKX_Full);
		break;
	case XGBE_PORT_MODE_10GBASE_R:
		XGBE_SET_ADV(dphy, 10000baseKR_Full);
		break;
	case XGBE_PORT_MODE_SFP:
		switch (phy_data->sfp_base) {
		case XGBE_SFP_BASE_1000_T:
		case XGBE_SFP_BASE_1000_SX:
		case XGBE_SFP_BASE_1000_LX:
		case XGBE_SFP_BASE_1000_CX:
			XGBE_SET_ADV(dphy, 1000baseKX_Full);
			break;
		default:
			XGBE_SET_ADV(dphy, 10000baseKR_Full);
			break;
		}
		break;
	default:
		XGBE_SET_ADV(dphy, 10000baseKR_Full);
		break;
	}
}

static int
xgbe_phy_an_config(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	int ret;

	ret = xgbe_phy_find_phy_device(pdata);
	if (ret)
		return (ret);

	axgbe_printf(2, "%s: find_phy_device return %s.\n", __func__,
	    ret ? "Failure" : "Success");

	if (!phy_data->phydev)
		return (0);

	return (ret);
}

static enum xgbe_an_mode
xgbe_phy_an_sfp_mode(struct xgbe_phy_data *phy_data)
{
	switch (phy_data->sfp_base) {
	case XGBE_SFP_BASE_1000_T:
		return (XGBE_AN_MODE_CL37_SGMII);
	case XGBE_SFP_BASE_1000_SX:
	case XGBE_SFP_BASE_1000_LX:
	case XGBE_SFP_BASE_1000_CX:
		return (XGBE_AN_MODE_CL37);
	default:
		return (XGBE_AN_MODE_NONE);
	}
}

static enum xgbe_an_mode
xgbe_phy_an_mode(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	/* A KR re-driver will always require CL73 AN */
	if (phy_data->redrv)
		return (XGBE_AN_MODE_CL73_REDRV);

	switch (phy_data->port_mode) {
	case XGBE_PORT_MODE_BACKPLANE:
		return (XGBE_AN_MODE_CL73);
	case XGBE_PORT_MODE_BACKPLANE_2500:
		return (XGBE_AN_MODE_NONE);
	case XGBE_PORT_MODE_1000BASE_T:
		return (XGBE_AN_MODE_CL37_SGMII);
	case XGBE_PORT_MODE_1000BASE_X:
		return (XGBE_AN_MODE_CL37);
	case XGBE_PORT_MODE_NBASE_T:
		return (XGBE_AN_MODE_CL37_SGMII);
	case XGBE_PORT_MODE_10GBASE_T:
		return (XGBE_AN_MODE_CL73);
	case XGBE_PORT_MODE_10GBASE_R:
		return (XGBE_AN_MODE_NONE);
	case XGBE_PORT_MODE_SFP:
		return (xgbe_phy_an_sfp_mode(phy_data));
	default:
		return (XGBE_AN_MODE_NONE);
	}
}

static int
xgbe_phy_set_redrv_mode_mdio(struct xgbe_prv_data *pdata,
    enum xgbe_phy_redrv_mode mode)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	uint16_t redrv_reg, redrv_val;

	redrv_reg = XGBE_PHY_REDRV_MODE_REG + (phy_data->redrv_lane * 0x1000);
	redrv_val = (uint16_t)mode;

	return (pdata->hw_if.write_ext_mii_regs(pdata, phy_data->redrv_addr,
	    redrv_reg, redrv_val));
}

static int
xgbe_phy_set_redrv_mode_i2c(struct xgbe_prv_data *pdata,
    enum xgbe_phy_redrv_mode mode)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	unsigned int redrv_reg;
	int ret;

	/* Calculate the register to write */
	redrv_reg = XGBE_PHY_REDRV_MODE_REG + (phy_data->redrv_lane * 0x1000);

	ret = xgbe_phy_redrv_write(pdata, redrv_reg, mode);

	return (ret);
}

static void
xgbe_phy_set_redrv_mode(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	enum xgbe_phy_redrv_mode mode;
	int ret;

	if (!phy_data->redrv)
		return;

	mode = XGBE_PHY_REDRV_MODE_CX;
	if ((phy_data->port_mode == XGBE_PORT_MODE_SFP) &&
	    (phy_data->sfp_base != XGBE_SFP_BASE_1000_CX) &&
	    (phy_data->sfp_base != XGBE_SFP_BASE_10000_CR))
		mode = XGBE_PHY_REDRV_MODE_SR;

	ret = xgbe_phy_get_comm_ownership(pdata);
	if (ret)
		return;

	axgbe_printf(2, "%s: redrv_if set: %d\n", __func__, phy_data->redrv_if);
	if (phy_data->redrv_if)
		xgbe_phy_set_redrv_mode_i2c(pdata, mode);
	else
		xgbe_phy_set_redrv_mode_mdio(pdata, mode);

	xgbe_phy_put_comm_ownership(pdata);
}

static void
xgbe_phy_pll_ctrl(struct xgbe_prv_data *pdata, bool enable)
{
	XMDIO_WRITE_BITS(pdata, MDIO_MMD_PMAPMD, MDIO_VEND2_PMA_MISC_CTRL0,
					XGBE_PMA_PLL_CTRL_MASK,
					enable ? XGBE_PMA_PLL_CTRL_ENABLE
					       : XGBE_PMA_PLL_CTRL_DISABLE);
	DELAY(200);
}

static void
xgbe_phy_perform_ratechange(struct xgbe_prv_data *pdata, unsigned int cmd,
    unsigned int sub_cmd)
{
	unsigned int s0 = 0;
	unsigned int wait;

	xgbe_phy_pll_ctrl(pdata, false);

	/* Log if a previous command did not complete */
	if (XP_IOREAD_BITS(pdata, XP_DRIVER_INT_RO, STATUS))
		axgbe_error("firmware mailbox not ready for command\n");

	/* Construct the command */
	XP_SET_BITS(s0, XP_DRIVER_SCRATCH_0, COMMAND, cmd);
	XP_SET_BITS(s0, XP_DRIVER_SCRATCH_0, SUB_COMMAND, sub_cmd);

	/* Issue the command */
	XP_IOWRITE(pdata, XP_DRIVER_SCRATCH_0, s0);
	XP_IOWRITE(pdata, XP_DRIVER_SCRATCH_1, 0);
	XP_IOWRITE_BITS(pdata, XP_DRIVER_INT_REQ, REQUEST, 1);

	/* Wait for command to complete */
	wait = XGBE_RATECHANGE_COUNT;
	while (wait--) {
		if (!XP_IOREAD_BITS(pdata, XP_DRIVER_INT_RO, STATUS)) {
			axgbe_printf(3, "%s: Rate change done\n", __func__);
			goto reenable_pll;
		}

		DELAY(2000);
	}

	axgbe_printf(3, "firmware mailbox command did not complete\n");

reenable_pll:
	xgbe_phy_pll_ctrl(pdata, true);
}

static void
xgbe_phy_rrc(struct xgbe_prv_data *pdata)
{
	/* Receiver Reset Cycle */
	xgbe_phy_perform_ratechange(pdata, 5, 0);

	axgbe_printf(3, "receiver reset complete\n");
}

static void
xgbe_phy_power_off(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	/* Power off */
	xgbe_phy_perform_ratechange(pdata, 0, 0);

	phy_data->cur_mode = XGBE_MODE_UNKNOWN;

	axgbe_printf(3, "phy powered off\n");
}

static void
xgbe_phy_sfi_mode(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	xgbe_phy_set_redrv_mode(pdata);

	/* 10G/SFI */
	axgbe_printf(3, "%s: cable %d len %d\n", __func__, phy_data->sfp_cable,
	    phy_data->sfp_cable_len);

	if (phy_data->sfp_cable != XGBE_SFP_CABLE_PASSIVE)
		xgbe_phy_perform_ratechange(pdata, 3, 0);
	else {
		if (phy_data->sfp_cable_len <= 1)
			xgbe_phy_perform_ratechange(pdata, 3, 1);
		else if (phy_data->sfp_cable_len <= 3)
			xgbe_phy_perform_ratechange(pdata, 3, 2);
		else
			xgbe_phy_perform_ratechange(pdata, 3, 3);
	}

	phy_data->cur_mode = XGBE_MODE_SFI;

	axgbe_printf(3, "10GbE SFI mode set\n");
}

static void
xgbe_phy_x_mode(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	xgbe_phy_set_redrv_mode(pdata);

	/* 1G/X */
	xgbe_phy_perform_ratechange(pdata, 1, 3);

	phy_data->cur_mode = XGBE_MODE_X;

	axgbe_printf(3, "1GbE X mode set\n");
}

static void
xgbe_phy_sgmii_1000_mode(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	xgbe_phy_set_redrv_mode(pdata);

	/* 1G/SGMII */
	xgbe_phy_perform_ratechange(pdata, 1, 2);

	phy_data->cur_mode = XGBE_MODE_SGMII_1000;

	axgbe_printf(2, "1GbE SGMII mode set\n");
}

static void
xgbe_phy_sgmii_100_mode(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	xgbe_phy_set_redrv_mode(pdata);

	/* 100M/SGMII */
	xgbe_phy_perform_ratechange(pdata, 1, 1);

	phy_data->cur_mode = XGBE_MODE_SGMII_100;

	axgbe_printf(3, "100MbE SGMII mode set\n");
}

static void
xgbe_phy_kr_mode(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	xgbe_phy_set_redrv_mode(pdata);

	/* 10G/KR */
	xgbe_phy_perform_ratechange(pdata, 4, 0);

	phy_data->cur_mode = XGBE_MODE_KR;

	axgbe_printf(3, "10GbE KR mode set\n");
}

static void
xgbe_phy_kx_2500_mode(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	xgbe_phy_set_redrv_mode(pdata);

	/* 2.5G/KX */
	xgbe_phy_perform_ratechange(pdata, 2, 0);

	phy_data->cur_mode = XGBE_MODE_KX_2500;

	axgbe_printf(3, "2.5GbE KX mode set\n");
}

static void
xgbe_phy_kx_1000_mode(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	xgbe_phy_set_redrv_mode(pdata);

	/* 1G/KX */
	xgbe_phy_perform_ratechange(pdata, 1, 3);

	phy_data->cur_mode = XGBE_MODE_KX_1000;

	axgbe_printf(3, "1GbE KX mode set\n");
}

static enum xgbe_mode
xgbe_phy_cur_mode(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	return (phy_data->cur_mode);
}

static enum xgbe_mode
xgbe_phy_switch_baset_mode(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	/* No switching if not 10GBase-T */
	if (phy_data->port_mode != XGBE_PORT_MODE_10GBASE_T)
		return (xgbe_phy_cur_mode(pdata));

	switch (xgbe_phy_cur_mode(pdata)) {
	case XGBE_MODE_SGMII_100:
	case XGBE_MODE_SGMII_1000:
		return (XGBE_MODE_KR);
	case XGBE_MODE_KR:
	default:
		return (XGBE_MODE_SGMII_1000);
	}
}

static enum xgbe_mode
xgbe_phy_switch_bp_2500_mode(struct xgbe_prv_data *pdata)
{
	return (XGBE_MODE_KX_2500);
}

static enum xgbe_mode
xgbe_phy_switch_bp_mode(struct xgbe_prv_data *pdata)
{
	/* If we are in KR switch to KX, and vice-versa */
	switch (xgbe_phy_cur_mode(pdata)) {
	case XGBE_MODE_KX_1000:
		return (XGBE_MODE_KR);
	case XGBE_MODE_KR:
	default:
		return (XGBE_MODE_KX_1000);
	}
}

static enum xgbe_mode
xgbe_phy_switch_mode(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	switch (phy_data->port_mode) {
	case XGBE_PORT_MODE_BACKPLANE:
		return (xgbe_phy_switch_bp_mode(pdata));
	case XGBE_PORT_MODE_BACKPLANE_2500:
		return (xgbe_phy_switch_bp_2500_mode(pdata));
	case XGBE_PORT_MODE_1000BASE_T:
	case XGBE_PORT_MODE_NBASE_T:
	case XGBE_PORT_MODE_10GBASE_T:
		return (xgbe_phy_switch_baset_mode(pdata));
	case XGBE_PORT_MODE_1000BASE_X:
	case XGBE_PORT_MODE_10GBASE_R:
	case XGBE_PORT_MODE_SFP:
		/* No switching, so just return current mode */
		return (xgbe_phy_cur_mode(pdata));
	default:
		return (XGBE_MODE_UNKNOWN);
	}
}

static enum xgbe_mode
xgbe_phy_get_basex_mode(struct xgbe_phy_data *phy_data, int speed)
{
	switch (speed) {
	case SPEED_1000:
		return (XGBE_MODE_X);
	case SPEED_10000:
		return (XGBE_MODE_KR);
	default:
		return (XGBE_MODE_UNKNOWN);
	}
}

static enum xgbe_mode
xgbe_phy_get_baset_mode(struct xgbe_phy_data *phy_data, int speed)
{
	switch (speed) {
	case SPEED_100:
		return (XGBE_MODE_SGMII_100);
	case SPEED_1000:
		return (XGBE_MODE_SGMII_1000);
	case SPEED_2500:
		return (XGBE_MODE_KX_2500);
	case SPEED_10000:
		return (XGBE_MODE_KR);
	default:
		return (XGBE_MODE_UNKNOWN);
	}
}

static enum xgbe_mode
xgbe_phy_get_sfp_mode(struct xgbe_phy_data *phy_data, int speed)
{
	switch (speed) {
	case SPEED_100:
		return (XGBE_MODE_SGMII_100);
	case SPEED_1000:
		if (phy_data->sfp_base == XGBE_SFP_BASE_1000_T)
			return (XGBE_MODE_SGMII_1000);
		else
			return (XGBE_MODE_X);
	case SPEED_10000:
	case SPEED_UNKNOWN:
		return (XGBE_MODE_SFI);
	default:
		return (XGBE_MODE_UNKNOWN);
	}
}

static enum xgbe_mode
xgbe_phy_get_bp_2500_mode(int speed)
{
	switch (speed) {
	case SPEED_2500:
		return (XGBE_MODE_KX_2500);
	default:
		return (XGBE_MODE_UNKNOWN);
	}
}

static enum xgbe_mode
xgbe_phy_get_bp_mode(int speed)
{
	switch (speed) {
	case SPEED_1000:
		return (XGBE_MODE_KX_1000);
	case SPEED_10000:
		return (XGBE_MODE_KR);
	default:
		return (XGBE_MODE_UNKNOWN);
	}
}

static enum xgbe_mode
xgbe_phy_get_mode(struct xgbe_prv_data *pdata, int speed)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	switch (phy_data->port_mode) {
	case XGBE_PORT_MODE_BACKPLANE:
		return (xgbe_phy_get_bp_mode(speed));
	case XGBE_PORT_MODE_BACKPLANE_2500:
		return (xgbe_phy_get_bp_2500_mode(speed));
	case XGBE_PORT_MODE_1000BASE_T:
	case XGBE_PORT_MODE_NBASE_T:
	case XGBE_PORT_MODE_10GBASE_T:
		return (xgbe_phy_get_baset_mode(phy_data, speed));
	case XGBE_PORT_MODE_1000BASE_X:
	case XGBE_PORT_MODE_10GBASE_R:
		return (xgbe_phy_get_basex_mode(phy_data, speed));
	case XGBE_PORT_MODE_SFP:
		return (xgbe_phy_get_sfp_mode(phy_data, speed));
	default:
		return (XGBE_MODE_UNKNOWN);
	}
}

static void
xgbe_phy_set_mode(struct xgbe_prv_data *pdata, enum xgbe_mode mode)
{
	switch (mode) {
	case XGBE_MODE_KX_1000:
		xgbe_phy_kx_1000_mode(pdata);
		break;
	case XGBE_MODE_KX_2500:
		xgbe_phy_kx_2500_mode(pdata);
		break;
	case XGBE_MODE_KR:
		xgbe_phy_kr_mode(pdata);
		break;
	case XGBE_MODE_SGMII_100:
		xgbe_phy_sgmii_100_mode(pdata);
		break;
	case XGBE_MODE_SGMII_1000:
		xgbe_phy_sgmii_1000_mode(pdata);
		break;
	case XGBE_MODE_X:
		xgbe_phy_x_mode(pdata);
		break;
	case XGBE_MODE_SFI:
		xgbe_phy_sfi_mode(pdata);
		break;
	default:
		break;
	}
}

static void
xgbe_phy_get_type(struct xgbe_prv_data *pdata, struct ifmediareq * ifmr)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	switch (pdata->phy.speed) {
	case SPEED_10000:
		if (phy_data->port_mode == XGBE_PORT_MODE_BACKPLANE)
			ifmr->ifm_active |= IFM_10G_KR;
		else if(phy_data->port_mode == XGBE_PORT_MODE_10GBASE_T)
			ifmr->ifm_active |= IFM_10G_T;
		else if(phy_data->port_mode == XGBE_PORT_MODE_10GBASE_R)
			ifmr->ifm_active |= IFM_10G_KR;
		else if(phy_data->port_mode == XGBE_PORT_MODE_SFP)
			ifmr->ifm_active |= IFM_10G_SFI;
		else
			ifmr->ifm_active |= IFM_OTHER;
		break;
	case SPEED_2500:
		if (phy_data->port_mode == XGBE_PORT_MODE_BACKPLANE_2500)
			ifmr->ifm_active |= IFM_2500_KX;
		else
			ifmr->ifm_active |= IFM_OTHER;
		break;
	case SPEED_1000:
		if (phy_data->port_mode == XGBE_PORT_MODE_BACKPLANE)
			ifmr->ifm_active |= IFM_1000_KX;
		else if(phy_data->port_mode == XGBE_PORT_MODE_1000BASE_T)
			ifmr->ifm_active |= IFM_1000_T;
#if 0
		else if(phy_data->port_mode == XGBE_PORT_MODE_1000BASE_X)
		      ifmr->ifm_active |= IFM_1000_SX;
		      ifmr->ifm_active |= IFM_1000_LX;
		      ifmr->ifm_active |= IFM_1000_CX;
#endif
		else if(phy_data->port_mode == XGBE_PORT_MODE_SFP)
			ifmr->ifm_active |= IFM_1000_SGMII;
		else
			ifmr->ifm_active |= IFM_OTHER;
		break;
	case SPEED_100:
		if(phy_data->port_mode == XGBE_PORT_MODE_NBASE_T)
			ifmr->ifm_active |= IFM_100_T;
		else if(phy_data->port_mode == XGBE_PORT_MODE_SFP)
			ifmr->ifm_active |= IFM_100_SGMII;
		else
			ifmr->ifm_active |= IFM_OTHER;
		break;
	default:
		ifmr->ifm_active |= IFM_OTHER;
		axgbe_printf(1, "Unknown mode detected\n");
		break;
	}
}

static bool
xgbe_phy_check_mode(struct xgbe_prv_data *pdata, enum xgbe_mode mode,
    bool advert)
{

	if (pdata->phy.autoneg == AUTONEG_ENABLE)
		return (advert);
	else {
		enum xgbe_mode cur_mode;

		cur_mode = xgbe_phy_get_mode(pdata, pdata->phy.speed);
		if (cur_mode == mode)
			return (true);
	}

	return (false);
}

static bool
xgbe_phy_use_basex_mode(struct xgbe_prv_data *pdata, enum xgbe_mode mode)
{

	switch (mode) {
	case XGBE_MODE_X:
		return (xgbe_phy_check_mode(pdata, mode, XGBE_ADV(&pdata->phy,
		    1000baseX_Full)));
	case XGBE_MODE_KR:
		return (xgbe_phy_check_mode(pdata, mode, XGBE_ADV(&pdata->phy,
		    10000baseKR_Full)));
	default:
		return (false);
	}
}

static bool
xgbe_phy_use_baset_mode(struct xgbe_prv_data *pdata, enum xgbe_mode mode)
{

	axgbe_printf(3, "%s: check mode %d\n", __func__, mode);
	switch (mode) {
	case XGBE_MODE_SGMII_100:
		return (xgbe_phy_check_mode(pdata, mode, XGBE_ADV(&pdata->phy,
		    100baseT_Full)));
	case XGBE_MODE_SGMII_1000:
		return (xgbe_phy_check_mode(pdata, mode, XGBE_ADV(&pdata->phy,
		    1000baseT_Full)));
	case XGBE_MODE_KX_2500:
		return (xgbe_phy_check_mode(pdata, mode, XGBE_ADV(&pdata->phy,
		    2500baseT_Full)));
	case XGBE_MODE_KR:
		return (xgbe_phy_check_mode(pdata, mode, XGBE_ADV(&pdata->phy,
		    10000baseT_Full)));
	default:
		return (false);
	}
}

static bool
xgbe_phy_use_sfp_mode(struct xgbe_prv_data *pdata, enum xgbe_mode mode)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	switch (mode) {
	case XGBE_MODE_X:
		if (phy_data->sfp_base == XGBE_SFP_BASE_1000_T)
			return (false);
		return (xgbe_phy_check_mode(pdata, mode,
		    XGBE_ADV(&pdata->phy, 1000baseX_Full)));
	case XGBE_MODE_SGMII_100:
		if (phy_data->sfp_base != XGBE_SFP_BASE_1000_T)
			return (false);
		return (xgbe_phy_check_mode(pdata, mode,
		    XGBE_ADV(&pdata->phy, 100baseT_Full)));
	case XGBE_MODE_SGMII_1000:
		if (phy_data->sfp_base != XGBE_SFP_BASE_1000_T)
			return (false);
		return (xgbe_phy_check_mode(pdata, mode,
		    XGBE_ADV(&pdata->phy, 1000baseT_Full)));
	case XGBE_MODE_SFI:
		if (phy_data->sfp_mod_absent)
			return (true);
		return (xgbe_phy_check_mode(pdata, mode,
		    XGBE_ADV(&pdata->phy, 10000baseSR_Full)  ||
		    XGBE_ADV(&pdata->phy, 10000baseLR_Full)  ||
		    XGBE_ADV(&pdata->phy, 10000baseLRM_Full) ||
		    XGBE_ADV(&pdata->phy, 10000baseER_Full)  ||
		    XGBE_ADV(&pdata->phy, 10000baseCR_Full)));
	default:
		return (false);
	}
}

static bool
xgbe_phy_use_bp_2500_mode(struct xgbe_prv_data *pdata, enum xgbe_mode mode)
{

	switch (mode) {
	case XGBE_MODE_KX_2500:
		return (xgbe_phy_check_mode(pdata, mode,
		    XGBE_ADV(&pdata->phy, 2500baseX_Full)));
	default:
		return (false);
	}
}

static bool
xgbe_phy_use_bp_mode(struct xgbe_prv_data *pdata, enum xgbe_mode mode)
{

	switch (mode) {
	case XGBE_MODE_KX_1000:
		return (xgbe_phy_check_mode(pdata, mode,
		    XGBE_ADV(&pdata->phy, 1000baseKX_Full)));
	case XGBE_MODE_KR:
		return (xgbe_phy_check_mode(pdata, mode,
		    XGBE_ADV(&pdata->phy, 10000baseKR_Full)));
	default:
		return (false);
	}
}

static bool
xgbe_phy_use_mode(struct xgbe_prv_data *pdata, enum xgbe_mode mode)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	switch (phy_data->port_mode) {
	case XGBE_PORT_MODE_BACKPLANE:
		return (xgbe_phy_use_bp_mode(pdata, mode));
	case XGBE_PORT_MODE_BACKPLANE_2500:
		return (xgbe_phy_use_bp_2500_mode(pdata, mode));
	case XGBE_PORT_MODE_1000BASE_T:
		axgbe_printf(3, "use_mode %s\n",
		    xgbe_phy_use_baset_mode(pdata, mode) ? "found" : "Not found");
	case XGBE_PORT_MODE_NBASE_T:
	case XGBE_PORT_MODE_10GBASE_T:
		return (xgbe_phy_use_baset_mode(pdata, mode));
	case XGBE_PORT_MODE_1000BASE_X:
	case XGBE_PORT_MODE_10GBASE_R:
		return (xgbe_phy_use_basex_mode(pdata, mode));
	case XGBE_PORT_MODE_SFP:
		return (xgbe_phy_use_sfp_mode(pdata, mode));
	default:
		return (false);
	}
}

static bool
xgbe_phy_valid_speed_basex_mode(struct xgbe_phy_data *phy_data, int speed)
{

	switch (speed) {
	case SPEED_1000:
		return (phy_data->port_mode == XGBE_PORT_MODE_1000BASE_X);
	case SPEED_10000:
		return (phy_data->port_mode == XGBE_PORT_MODE_10GBASE_R);
	default:
		return (false);
	}
}

static bool
xgbe_phy_valid_speed_baset_mode(struct xgbe_phy_data *phy_data, int speed)
{

	switch (speed) {
	case SPEED_100:
	case SPEED_1000:
		return (true);
	case SPEED_2500:
		return (phy_data->port_mode == XGBE_PORT_MODE_NBASE_T);
	case SPEED_10000:
		return (phy_data->port_mode == XGBE_PORT_MODE_10GBASE_T);
	default:
		return (false);
	}
}

static bool
xgbe_phy_valid_speed_sfp_mode(struct xgbe_phy_data *phy_data, int speed)
{

	switch (speed) {
	case SPEED_100:
		return ((phy_data->sfp_speed == XGBE_SFP_SPEED_100) ||
			(phy_data->sfp_speed == XGBE_SFP_SPEED_100_1000));
	case SPEED_1000:
		return ((phy_data->sfp_speed == XGBE_SFP_SPEED_100_1000) ||
		    (phy_data->sfp_speed == XGBE_SFP_SPEED_1000));
	case SPEED_10000:
		return (phy_data->sfp_speed == XGBE_SFP_SPEED_10000);
	default:
		return (false);
	}
}

static bool
xgbe_phy_valid_speed_bp_2500_mode(int speed)
{

	switch (speed) {
	case SPEED_2500:
		return (true);
	default:
		return (false);
	}
}

static bool
xgbe_phy_valid_speed_bp_mode(int speed)
{

	switch (speed) {
	case SPEED_1000:
	case SPEED_10000:
		return (true);
	default:
		return (false);
	}
}

static bool
xgbe_phy_valid_speed(struct xgbe_prv_data *pdata, int speed)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	switch (phy_data->port_mode) {
	case XGBE_PORT_MODE_BACKPLANE:
		return (xgbe_phy_valid_speed_bp_mode(speed));
	case XGBE_PORT_MODE_BACKPLANE_2500:
		return (xgbe_phy_valid_speed_bp_2500_mode(speed));
	case XGBE_PORT_MODE_1000BASE_T:
	case XGBE_PORT_MODE_NBASE_T:
	case XGBE_PORT_MODE_10GBASE_T:
		return (xgbe_phy_valid_speed_baset_mode(phy_data, speed));
	case XGBE_PORT_MODE_1000BASE_X:
	case XGBE_PORT_MODE_10GBASE_R:
		return (xgbe_phy_valid_speed_basex_mode(phy_data, speed));
	case XGBE_PORT_MODE_SFP:
		return (xgbe_phy_valid_speed_sfp_mode(phy_data, speed));
	default:
		return (false);
	}
}

static int
xgbe_upd_link(struct xgbe_prv_data *pdata)
{
	int reg;

	axgbe_printf(2, "%s: Link %d\n", __func__, pdata->phy.link);
	reg = xgbe_phy_mii_read(pdata, pdata->mdio_addr, MII_BMSR);
	reg = xgbe_phy_mii_read(pdata, pdata->mdio_addr, MII_BMSR);
	if (reg < 0)
		return (reg);

	if ((reg & BMSR_LINK) == 0)
		pdata->phy.link = 0;
	else
		pdata->phy.link = 1;

	axgbe_printf(2, "Link: %d updated reg %#x\n", pdata->phy.link, reg);
	return (0);
}

static int
xgbe_phy_read_status(struct xgbe_prv_data *pdata)
{
	int common_adv_gb = 0;
	int common_adv;
	int lpagb = 0;
	int adv, lpa;
	int ret;

	ret = xgbe_upd_link(pdata);
	if (ret) {
		axgbe_printf(2, "Link Update return %d\n", ret);
		return (ret);
	}	

	if (AUTONEG_ENABLE == pdata->phy.autoneg) {
		if (pdata->phy.supported == SUPPORTED_1000baseT_Half || 
		    pdata->phy.supported == SUPPORTED_1000baseT_Full) {
			lpagb = xgbe_phy_mii_read(pdata, pdata->mdio_addr,
			    MII_100T2SR);
			if (lpagb < 0)
				return (lpagb);

			adv = xgbe_phy_mii_read(pdata, pdata->mdio_addr,
			    MII_100T2CR);
			if (adv < 0)
				return (adv);

			if (lpagb & GTSR_MAN_MS_FLT) {
				if (adv & GTCR_MAN_MS)
					axgbe_printf(2, "Master/Slave Resolution "
					    "failed, maybe conflicting manual settings\n");
				else
					axgbe_printf(2, "Master/Slave Resolution failed\n");
				return (-ENOLINK);
			}

			if (pdata->phy.supported == SUPPORTED_1000baseT_Half) 
				XGBE_SET_ADV(&pdata->phy, 1000baseT_Half); 
			else if (pdata->phy.supported == SUPPORTED_1000baseT_Full) 
				XGBE_SET_ADV(&pdata->phy, 1000baseT_Full); 

			common_adv_gb = lpagb & adv << 2;
		}

		lpa = xgbe_phy_mii_read(pdata, pdata->mdio_addr, MII_ANLPAR);
		if (lpa < 0)
			return (lpa);

		if (pdata->phy.supported == SUPPORTED_Autoneg) 
			XGBE_SET_ADV(&pdata->phy, Autoneg);
 
		adv = xgbe_phy_mii_read(pdata, pdata->mdio_addr, MII_ANAR);
		if (adv < 0)
			return (adv);

		common_adv = lpa & adv;

		pdata->phy.speed = SPEED_10;
		pdata->phy.duplex = DUPLEX_HALF;
		pdata->phy.pause = 0;
		pdata->phy.asym_pause = 0;

		axgbe_printf(2, "%s: lpa %#x adv %#x common_adv_gb %#x "
		    "common_adv %#x\n", __func__, lpa, adv, common_adv_gb,
		    common_adv);
		if (common_adv_gb & (GTSR_LP_1000TFDX | GTSR_LP_1000THDX)) {
			axgbe_printf(2, "%s: SPEED 1000\n", __func__);
			pdata->phy.speed = SPEED_1000;

			if (common_adv_gb & GTSR_LP_1000TFDX)
				pdata->phy.duplex = DUPLEX_FULL;
		} else if (common_adv & (ANLPAR_TX_FD | ANLPAR_TX)) {
			axgbe_printf(2, "%s: SPEED 100\n", __func__);
			pdata->phy.speed = SPEED_100;

			if (common_adv & ANLPAR_TX_FD)
				pdata->phy.duplex = DUPLEX_FULL;
		} else
			if (common_adv & ANLPAR_10_FD)
				pdata->phy.duplex = DUPLEX_FULL;

		if (pdata->phy.duplex == DUPLEX_FULL) {
			pdata->phy.pause = lpa & ANLPAR_FC ? 1 : 0;
			pdata->phy.asym_pause = lpa & LPA_PAUSE_ASYM ? 1 : 0;
		}
	} else {
		int bmcr = xgbe_phy_mii_read(pdata, pdata->mdio_addr, MII_BMCR);
		if (bmcr < 0)
			return (bmcr);

		if (bmcr & BMCR_FDX)
			pdata->phy.duplex = DUPLEX_FULL;
		else
			pdata->phy.duplex = DUPLEX_HALF;

		if (bmcr & BMCR_SPEED1)
			pdata->phy.speed = SPEED_1000;
		else if (bmcr & BMCR_SPEED100)
			pdata->phy.speed = SPEED_100;
		else
			pdata->phy.speed = SPEED_10;

		pdata->phy.pause = 0;
		pdata->phy.asym_pause = 0;
		axgbe_printf(2, "%s: link speed %#x duplex %#x media %#x "
		    "autoneg %#x\n", __func__, pdata->phy.speed,
		    pdata->phy.duplex, pdata->phy.link, pdata->phy.autoneg);
	}	
		
	return (0);
}

static void
xgbe_rrc(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	int ret;

	if (phy_data->rrc_count++ > XGBE_RRC_FREQUENCY) {
		axgbe_printf(1, "ENTERED RRC: rrc_count: %d\n",
			phy_data->rrc_count);
		phy_data->rrc_count = 0;
		if (pdata->link_workaround) {
			ret = xgbe_phy_reset(pdata);
			if (ret)
				axgbe_error("Error resetting phy\n");
		} else
			xgbe_phy_rrc(pdata);
	}
}

static int
xgbe_phy_link_status(struct xgbe_prv_data *pdata, int *an_restart)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	struct mii_data *mii = NULL;
	unsigned int reg;
	int ret;

	*an_restart = 0;

	if (phy_data->port_mode == XGBE_PORT_MODE_SFP) {
		/* Check SFP signals */
		axgbe_printf(3, "%s: calling phy detect\n", __func__);
		xgbe_phy_sfp_detect(pdata);

		if (phy_data->sfp_changed) {
			axgbe_printf(1, "%s: SFP changed observed\n", __func__);
			*an_restart = 1;
			return (0);
		}

		if (phy_data->sfp_mod_absent || phy_data->sfp_rx_los) {
			axgbe_printf(1, "%s: SFP absent 0x%x & sfp_rx_los 0x%x\n",
			    __func__, phy_data->sfp_mod_absent,
			    phy_data->sfp_rx_los);

			if (!phy_data->sfp_mod_absent) {
				xgbe_rrc(pdata);
			}

			return (0);
		}
	}

	if (phy_data->phydev || phy_data->port_mode != XGBE_PORT_MODE_SFP) {
		if (pdata->axgbe_miibus == NULL) {
			axgbe_printf(1, "%s: miibus not initialized", __func__);
			goto mdio_read;
		}

		mii = device_get_softc(pdata->axgbe_miibus);
		mii_tick(mii);

		ret = xgbe_phy_read_status(pdata);
		if (ret) {
			axgbe_error("Link: Read status returned %d\n", ret);
			return (0);
		}

		axgbe_printf(2, "%s: link speed %#x duplex %#x media %#x "
		    "autoneg %#x\n", __func__, pdata->phy.speed,
		    pdata->phy.duplex, pdata->phy.link, pdata->phy.autoneg);
		ret = xgbe_phy_mii_read(pdata, pdata->mdio_addr, MII_BMSR);
		ret = (ret < 0) ? ret : (ret & BMSR_ACOMP);
		axgbe_printf(2, "Link: BMCR returned %d\n", ret);
		if ((pdata->phy.autoneg == AUTONEG_ENABLE) && !ret)
			return (0);

		if (pdata->phy.link)
			return (1);

		xgbe_rrc(pdata);
	}

mdio_read:

	/* Link status is latched low, so read once to clear
	 * and then read again to get current state
	 */
	reg = XMDIO_READ(pdata, MDIO_MMD_PCS, MDIO_STAT1);
	reg = XMDIO_READ(pdata, MDIO_MMD_PCS, MDIO_STAT1);
	axgbe_printf(1, "%s: link_status reg: 0x%x\n", __func__, reg);
	if (reg & MDIO_STAT1_LSTATUS)
		return (1);

	/* No link, attempt a receiver reset cycle */
	xgbe_rrc(pdata);

	return (0);
}

static void
xgbe_phy_sfp_gpio_setup(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	phy_data->sfp_gpio_address = XGBE_GPIO_ADDRESS_PCA9555 +
	    XP_GET_BITS(pdata->pp3, XP_PROP_3, GPIO_ADDR);
	phy_data->sfp_gpio_mask = XP_GET_BITS(pdata->pp3, XP_PROP_3,
	    GPIO_MASK);
	phy_data->sfp_gpio_rx_los = XP_GET_BITS(pdata->pp3, XP_PROP_3,
	    GPIO_RX_LOS);
	phy_data->sfp_gpio_tx_fault = XP_GET_BITS(pdata->pp3, XP_PROP_3,
	    GPIO_TX_FAULT);
	phy_data->sfp_gpio_mod_absent = XP_GET_BITS(pdata->pp3, XP_PROP_3,
	    GPIO_MOD_ABS);
	phy_data->sfp_gpio_rate_select = XP_GET_BITS(pdata->pp3, XP_PROP_3,
	    GPIO_RATE_SELECT);

	DBGPR("SFP: gpio_address=%#x\n", phy_data->sfp_gpio_address);
	DBGPR("SFP: gpio_mask=%#x\n",	phy_data->sfp_gpio_mask);
	DBGPR("SFP: gpio_rx_los=%u\n", phy_data->sfp_gpio_rx_los);
	DBGPR("SFP: gpio_tx_fault=%u\n", phy_data->sfp_gpio_tx_fault);
	DBGPR("SFP: gpio_mod_absent=%u\n",
	    phy_data->sfp_gpio_mod_absent);
	DBGPR("SFP: gpio_rate_select=%u\n",
	    phy_data->sfp_gpio_rate_select);
}

static void
xgbe_phy_sfp_comm_setup(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	unsigned int mux_addr_hi, mux_addr_lo;

	mux_addr_hi = XP_GET_BITS(pdata->pp4, XP_PROP_4, MUX_ADDR_HI);
	mux_addr_lo = XP_GET_BITS(pdata->pp4, XP_PROP_4, MUX_ADDR_LO);
	if (mux_addr_lo == XGBE_SFP_DIRECT)
		return;

	phy_data->sfp_comm = XGBE_SFP_COMM_PCA9545;
	phy_data->sfp_mux_address = (mux_addr_hi << 2) + mux_addr_lo;
	phy_data->sfp_mux_channel = XP_GET_BITS(pdata->pp4, XP_PROP_4,
						MUX_CHAN);

	DBGPR("SFP: mux_address=%#x\n", phy_data->sfp_mux_address);
	DBGPR("SFP: mux_channel=%u\n", phy_data->sfp_mux_channel);
}

static void
xgbe_phy_sfp_setup(struct xgbe_prv_data *pdata)
{
	xgbe_phy_sfp_comm_setup(pdata);
	xgbe_phy_sfp_gpio_setup(pdata);
}

static int
xgbe_phy_int_mdio_reset(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	unsigned int ret;

	ret = pdata->hw_if.set_gpio(pdata, phy_data->mdio_reset_gpio);
	if (ret)
		return (ret);

	ret = pdata->hw_if.clr_gpio(pdata, phy_data->mdio_reset_gpio);

	return (ret);
}

static int
xgbe_phy_i2c_mdio_reset(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	uint8_t gpio_reg, gpio_ports[2], gpio_data[3];
	int ret;

	/* Read the output port registers */
	gpio_reg = 2;
	ret = xgbe_phy_i2c_read(pdata, phy_data->mdio_reset_addr,
				&gpio_reg, sizeof(gpio_reg),
				gpio_ports, sizeof(gpio_ports));
	if (ret)
		return (ret);

	/* Prepare to write the GPIO data */
	gpio_data[0] = 2;
	gpio_data[1] = gpio_ports[0];
	gpio_data[2] = gpio_ports[1];

	/* Set the GPIO pin */
	if (phy_data->mdio_reset_gpio < 8)
		gpio_data[1] |= (1 << (phy_data->mdio_reset_gpio % 8));
	else
		gpio_data[2] |= (1 << (phy_data->mdio_reset_gpio % 8));

	/* Write the output port registers */
	ret = xgbe_phy_i2c_write(pdata, phy_data->mdio_reset_addr,
				 gpio_data, sizeof(gpio_data));
	if (ret)
		return (ret);

	/* Clear the GPIO pin */
	if (phy_data->mdio_reset_gpio < 8)
		gpio_data[1] &= ~(1 << (phy_data->mdio_reset_gpio % 8));
	else
		gpio_data[2] &= ~(1 << (phy_data->mdio_reset_gpio % 8));

	/* Write the output port registers */
	ret = xgbe_phy_i2c_write(pdata, phy_data->mdio_reset_addr,
				 gpio_data, sizeof(gpio_data));

	return (ret);
}

static int
xgbe_phy_mdio_reset(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	int ret;

	if (phy_data->conn_type != XGBE_CONN_TYPE_MDIO)
		return (0);

	ret = xgbe_phy_get_comm_ownership(pdata);
	if (ret)
		return (ret);

	if (phy_data->mdio_reset == XGBE_MDIO_RESET_I2C_GPIO)
		ret = xgbe_phy_i2c_mdio_reset(pdata);
	else if (phy_data->mdio_reset == XGBE_MDIO_RESET_INT_GPIO)
		ret = xgbe_phy_int_mdio_reset(pdata);

	xgbe_phy_put_comm_ownership(pdata);

	return (ret);
}

static bool
xgbe_phy_redrv_error(struct xgbe_phy_data *phy_data)
{
	if (!phy_data->redrv)
		return (false);

	if (phy_data->redrv_if >= XGBE_PHY_REDRV_IF_MAX)
		return (true);

	switch (phy_data->redrv_model) {
	case XGBE_PHY_REDRV_MODEL_4223:
		if (phy_data->redrv_lane > 3)
			return (true);
		break;
	case XGBE_PHY_REDRV_MODEL_4227:
		if (phy_data->redrv_lane > 1)
			return (true);
		break;
	default:
		return (true);
	}

	return (false);
}

static int
xgbe_phy_mdio_reset_setup(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	if (phy_data->conn_type != XGBE_CONN_TYPE_MDIO)
		return (0);

	phy_data->mdio_reset = XP_GET_BITS(pdata->pp3, XP_PROP_3, MDIO_RESET);
	switch (phy_data->mdio_reset) {
	case XGBE_MDIO_RESET_NONE:
	case XGBE_MDIO_RESET_I2C_GPIO:
	case XGBE_MDIO_RESET_INT_GPIO:
		break;
	default:
		axgbe_error("unsupported MDIO reset (%#x)\n",
		    phy_data->mdio_reset);
		return (-EINVAL);
	}

	if (phy_data->mdio_reset == XGBE_MDIO_RESET_I2C_GPIO) {
		phy_data->mdio_reset_addr = XGBE_GPIO_ADDRESS_PCA9555 +
		    XP_GET_BITS(pdata->pp3, XP_PROP_3, MDIO_RESET_I2C_ADDR);
		phy_data->mdio_reset_gpio = XP_GET_BITS(pdata->pp3, XP_PROP_3,
		    MDIO_RESET_I2C_GPIO);
	} else if (phy_data->mdio_reset == XGBE_MDIO_RESET_INT_GPIO)
		phy_data->mdio_reset_gpio = XP_GET_BITS(pdata->pp3, XP_PROP_3,
		    MDIO_RESET_INT_GPIO);

	return (0);
}

static bool
xgbe_phy_port_mode_mismatch(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	switch (phy_data->port_mode) {
	case XGBE_PORT_MODE_BACKPLANE:
		if ((phy_data->port_speeds & XGBE_PHY_PORT_SPEED_1000) ||
		    (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_10000))
			return (false);
		break;
	case XGBE_PORT_MODE_BACKPLANE_2500:
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_2500)
			return (false);
		break;
	case XGBE_PORT_MODE_1000BASE_T:
		if ((phy_data->port_speeds & XGBE_PHY_PORT_SPEED_100) ||
		    (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_1000))
			return (false);
		break;
	case XGBE_PORT_MODE_1000BASE_X:
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_1000)
			return (false);
		break;
	case XGBE_PORT_MODE_NBASE_T:
		if ((phy_data->port_speeds & XGBE_PHY_PORT_SPEED_100) ||
		    (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_1000) ||
		    (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_2500))
			return (false);
		break;
	case XGBE_PORT_MODE_10GBASE_T:
		if ((phy_data->port_speeds & XGBE_PHY_PORT_SPEED_100) ||
		    (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_1000) ||
		    (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_10000))
			return (false);
		break;
	case XGBE_PORT_MODE_10GBASE_R:
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_10000)
			return (false);
		break;
	case XGBE_PORT_MODE_SFP:
		if ((phy_data->port_speeds & XGBE_PHY_PORT_SPEED_100) ||
		    (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_1000) ||
		    (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_10000))
			return (false);
		break;
	default:
		break;
	}

	return (true);
}

static bool
xgbe_phy_conn_type_mismatch(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	switch (phy_data->port_mode) {
	case XGBE_PORT_MODE_BACKPLANE:
	case XGBE_PORT_MODE_BACKPLANE_2500:
		if (phy_data->conn_type == XGBE_CONN_TYPE_BACKPLANE)
			return (false);
		break;
	case XGBE_PORT_MODE_1000BASE_T:
	case XGBE_PORT_MODE_1000BASE_X:
	case XGBE_PORT_MODE_NBASE_T:
	case XGBE_PORT_MODE_10GBASE_T:
	case XGBE_PORT_MODE_10GBASE_R:
		if (phy_data->conn_type == XGBE_CONN_TYPE_MDIO)
			return (false);
		break;
	case XGBE_PORT_MODE_SFP:
		if (phy_data->conn_type == XGBE_CONN_TYPE_SFP)
			return (false);
		break;
	default:
		break;
	}

	return (true);
}

static bool
xgbe_phy_port_enabled(struct xgbe_prv_data *pdata)
{

	if (!XP_GET_BITS(pdata->pp0, XP_PROP_0, PORT_SPEEDS))
		return (false);
	if (!XP_GET_BITS(pdata->pp0, XP_PROP_0, CONN_TYPE))
		return (false);

	return (true);
}

static void
xgbe_phy_cdr_track(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	axgbe_printf(2, "%s: an_cdr_workaround %d phy_cdr_notrack %d\n",
	    __func__, pdata->sysctl_an_cdr_workaround, phy_data->phy_cdr_notrack);

	if (!pdata->sysctl_an_cdr_workaround)
		return;

	if (!phy_data->phy_cdr_notrack)
		return;

	DELAY(phy_data->phy_cdr_delay + 500);

	XMDIO_WRITE_BITS(pdata, MDIO_MMD_PMAPMD, MDIO_VEND2_PMA_CDR_CONTROL,
	    XGBE_PMA_CDR_TRACK_EN_MASK, XGBE_PMA_CDR_TRACK_EN_ON);

	phy_data->phy_cdr_notrack = 0;

	axgbe_printf(2, "CDR TRACK DONE\n");
}

static void
xgbe_phy_cdr_notrack(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	axgbe_printf(2, "%s: an_cdr_workaround %d phy_cdr_notrack %d\n",
	    __func__, pdata->sysctl_an_cdr_workaround, phy_data->phy_cdr_notrack);

	if (!pdata->sysctl_an_cdr_workaround)
		return;

	if (phy_data->phy_cdr_notrack)
		return;

	XMDIO_WRITE_BITS(pdata, MDIO_MMD_PMAPMD, MDIO_VEND2_PMA_CDR_CONTROL,
	    XGBE_PMA_CDR_TRACK_EN_MASK, XGBE_PMA_CDR_TRACK_EN_OFF);

	xgbe_phy_rrc(pdata);

	phy_data->phy_cdr_notrack = 1;
}

static void
xgbe_phy_kr_training_post(struct xgbe_prv_data *pdata)
{
	if (!pdata->sysctl_an_cdr_track_early)
		xgbe_phy_cdr_track(pdata);
}

static void
xgbe_phy_kr_training_pre(struct xgbe_prv_data *pdata)
{
	if (pdata->sysctl_an_cdr_track_early)
		xgbe_phy_cdr_track(pdata);
}

static void
xgbe_phy_an_post(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	switch (pdata->an_mode) {
	case XGBE_AN_MODE_CL73:
	case XGBE_AN_MODE_CL73_REDRV:
		if (phy_data->cur_mode != XGBE_MODE_KR)
			break;

		xgbe_phy_cdr_track(pdata);

		switch (pdata->an_result) {
		case XGBE_AN_READY:
		case XGBE_AN_COMPLETE:
			break;
		default:
			if (phy_data->phy_cdr_delay < XGBE_CDR_DELAY_MAX)
				phy_data->phy_cdr_delay += XGBE_CDR_DELAY_INC;
			else
				phy_data->phy_cdr_delay = XGBE_CDR_DELAY_INIT;
			break;
		}
		break;
	default:
		break;
	}
}

static void
xgbe_phy_an_pre(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	switch (pdata->an_mode) {
	case XGBE_AN_MODE_CL73:
	case XGBE_AN_MODE_CL73_REDRV:
		if (phy_data->cur_mode != XGBE_MODE_KR)
			break;

		xgbe_phy_cdr_notrack(pdata);
		break;
	default:
		break;
	}
}

static void
xgbe_phy_stop(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;

	/* If we have an external PHY, free it */
	xgbe_phy_free_phy_device(pdata);

	/* Reset SFP data */
	xgbe_phy_sfp_reset(phy_data);
	xgbe_phy_sfp_mod_absent(pdata);

	/* Reset CDR support */
	xgbe_phy_cdr_track(pdata);

	/* Power off the PHY */
	xgbe_phy_power_off(pdata);

	/* Stop the I2C controller */
	pdata->i2c_if.i2c_stop(pdata);
}

static int
xgbe_phy_start(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	int ret;

	axgbe_printf(2, "%s: redrv %d redrv_if %d start_mode %d\n", __func__,
	    phy_data->redrv, phy_data->redrv_if, phy_data->start_mode);

	/* Start the I2C controller */
	ret = pdata->i2c_if.i2c_start(pdata);
	if (ret) {
		axgbe_error("%s: impl i2c start ret %d\n", __func__, ret);
		return (ret);
	}

	/* Set the proper MDIO mode for the re-driver */
	if (phy_data->redrv && !phy_data->redrv_if) {
		ret = pdata->hw_if.set_ext_mii_mode(pdata, phy_data->redrv_addr,
		    XGBE_MDIO_MODE_CL22);
		if (ret) {
			axgbe_error("redriver mdio port not compatible (%u)\n",
			    phy_data->redrv_addr);
			return (ret);
		}
	}

	/* Start in highest supported mode */
	xgbe_phy_set_mode(pdata, phy_data->start_mode);

	/* Reset CDR support */
	xgbe_phy_cdr_track(pdata);

	/* After starting the I2C controller, we can check for an SFP */
	switch (phy_data->port_mode) {
	case XGBE_PORT_MODE_SFP:
		axgbe_printf(3, "%s: calling phy detect\n", __func__);
		xgbe_phy_sfp_detect(pdata);
		break;
	default:
		break;
	}

	/* If we have an external PHY, start it */
	ret = xgbe_phy_find_phy_device(pdata);
	if (ret) {
		axgbe_error("%s: impl find phy dev ret %d\n", __func__, ret);
		goto err_i2c;
	}

	axgbe_printf(3, "%s: impl return success\n", __func__);
	return (0);

err_i2c:
	pdata->i2c_if.i2c_stop(pdata);

	return (ret);
}

static int
xgbe_phy_reset(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data = pdata->phy_data;
	enum xgbe_mode cur_mode;
	int ret;

	/* Reset by power cycling the PHY */
	cur_mode = phy_data->cur_mode;
	xgbe_phy_power_off(pdata);
	xgbe_phy_set_mode(pdata, cur_mode);

	axgbe_printf(3, "%s: mode %d\n", __func__, cur_mode);
	if (!phy_data->phydev) {
		axgbe_printf(1, "%s: no phydev\n", __func__);
		return (0);
	}

	/* Reset the external PHY */
	ret = xgbe_phy_mdio_reset(pdata);
	if (ret) {
		axgbe_error("%s: mdio reset %d\n", __func__, ret);
		return (ret);
	}

	axgbe_printf(3, "%s: return success\n", __func__);

	return (0);
}

static void
axgbe_ifmedia_sts(if_t ifp, struct ifmediareq *ifmr)
{
	struct axgbe_if_softc *sc;
	struct xgbe_prv_data *pdata;
	struct mii_data *mii;

	sc = if_getsoftc(ifp);
	pdata = &sc->pdata;

	axgbe_printf(2, "%s: Invoked\n", __func__);
	mtx_lock_spin(&pdata->mdio_mutex);
	mii = device_get_softc(pdata->axgbe_miibus);
	axgbe_printf(2, "%s: media_active %#x media_status %#x\n", __func__,
	    mii->mii_media_active, mii->mii_media_status);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	mtx_unlock_spin(&pdata->mdio_mutex);
}

static int
axgbe_ifmedia_upd(if_t ifp)
{
	struct xgbe_prv_data *pdata;
	struct axgbe_if_softc *sc;
	struct mii_data *mii;
	struct mii_softc *miisc;
	int ret;

	sc = if_getsoftc(ifp);
	pdata = &sc->pdata;

	axgbe_printf(2, "%s: Invoked\n", __func__);
	mtx_lock_spin(&pdata->mdio_mutex);
	mii = device_get_softc(pdata->axgbe_miibus);
	LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
		PHY_RESET(miisc);
	ret = mii_mediachg(mii);
	mtx_unlock_spin(&pdata->mdio_mutex);

	return (ret);
}

static void
xgbe_phy_exit(struct xgbe_prv_data *pdata)
{
	if (pdata->axgbe_miibus != NULL)
		device_delete_child(pdata->dev, pdata->axgbe_miibus);

	/* free phy_data structure */
	free(pdata->phy_data, M_AXGBE);
}

static int
xgbe_phy_init(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy_data *phy_data;
	int ret;

	/* Initialize the global lock */
	if (!mtx_initialized(&xgbe_phy_comm_lock))
		mtx_init(&xgbe_phy_comm_lock, "xgbe phy common lock", NULL, MTX_DEF);

	/* Check if enabled */
	if (!xgbe_phy_port_enabled(pdata)) {
		axgbe_error("device is not enabled\n");
		return (-ENODEV);
	}

	/* Initialize the I2C controller */
	ret = pdata->i2c_if.i2c_init(pdata);
	if (ret)
		return (ret);

	phy_data = malloc(sizeof(*phy_data), M_AXGBE, M_WAITOK | M_ZERO);
	if (!phy_data)
		return (-ENOMEM);
	pdata->phy_data = phy_data;

	phy_data->port_mode = XP_GET_BITS(pdata->pp0, XP_PROP_0, PORT_MODE);
	phy_data->port_id = XP_GET_BITS(pdata->pp0, XP_PROP_0, PORT_ID);
	phy_data->port_speeds = XP_GET_BITS(pdata->pp0, XP_PROP_0, PORT_SPEEDS);
	phy_data->conn_type = XP_GET_BITS(pdata->pp0, XP_PROP_0, CONN_TYPE);
	phy_data->mdio_addr = XP_GET_BITS(pdata->pp0, XP_PROP_0, MDIO_ADDR);

	pdata->mdio_addr = phy_data->mdio_addr;
	DBGPR("port mode=%u\n", phy_data->port_mode);
	DBGPR("port id=%u\n", phy_data->port_id);
	DBGPR("port speeds=%#x\n", phy_data->port_speeds);
	DBGPR("conn type=%u\n", phy_data->conn_type);
	DBGPR("mdio addr=%u\n", phy_data->mdio_addr);

	phy_data->redrv = XP_GET_BITS(pdata->pp4, XP_PROP_4, REDRV_PRESENT);
	phy_data->redrv_if = XP_GET_BITS(pdata->pp4, XP_PROP_4, REDRV_IF);
	phy_data->redrv_addr = XP_GET_BITS(pdata->pp4, XP_PROP_4, REDRV_ADDR);
	phy_data->redrv_lane = XP_GET_BITS(pdata->pp4, XP_PROP_4, REDRV_LANE);
	phy_data->redrv_model = XP_GET_BITS(pdata->pp4, XP_PROP_4, REDRV_MODEL);
	
	if (phy_data->redrv) {
		DBGPR("redrv present\n");
		DBGPR("redrv i/f=%u\n", phy_data->redrv_if);
		DBGPR("redrv addr=%#x\n", phy_data->redrv_addr);
		DBGPR("redrv lane=%u\n", phy_data->redrv_lane);
		DBGPR("redrv model=%u\n", phy_data->redrv_model);
	}

	DBGPR("%s: redrv addr=%#x redrv i/f=%u\n", __func__,
	    phy_data->redrv_addr, phy_data->redrv_if);
	/* Validate the connection requested */
	if (xgbe_phy_conn_type_mismatch(pdata)) {
		axgbe_error("phy mode/connection mismatch "
		    "(%#x/%#x)\n", phy_data->port_mode, phy_data->conn_type);
		return (-EINVAL);
	}

	/* Validate the mode requested */
	if (xgbe_phy_port_mode_mismatch(pdata)) {
		axgbe_error("phy mode/speed mismatch "
		    "(%#x/%#x)\n", phy_data->port_mode, phy_data->port_speeds);
		return (-EINVAL);
	}

	/* Check for and validate MDIO reset support */
	ret = xgbe_phy_mdio_reset_setup(pdata);
	if (ret) {
		axgbe_error("%s, mdio_reset_setup ret %d\n", __func__, ret);
		return (ret);
	}

	/* Validate the re-driver information */
	if (xgbe_phy_redrv_error(phy_data)) {
		axgbe_error("phy re-driver settings error\n");
		return (-EINVAL);
	}
	pdata->kr_redrv = phy_data->redrv;

	/* Indicate current mode is unknown */
	phy_data->cur_mode = XGBE_MODE_UNKNOWN;

	/* Initialize supported features. Current code does not support ethtool */
	XGBE_ZERO_SUP(&pdata->phy);

	DBGPR("%s: port mode %d\n", __func__, phy_data->port_mode);
	switch (phy_data->port_mode) {
	/* Backplane support */
	case XGBE_PORT_MODE_BACKPLANE:
		XGBE_SET_SUP(&pdata->phy, Autoneg);
		XGBE_SET_SUP(&pdata->phy, Pause);
		XGBE_SET_SUP(&pdata->phy, Asym_Pause);
		XGBE_SET_SUP(&pdata->phy, Backplane);
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_1000) {
			XGBE_SET_SUP(&pdata->phy, 1000baseKX_Full);
			phy_data->start_mode = XGBE_MODE_KX_1000;
		}
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_10000) {
			XGBE_SET_SUP(&pdata->phy, 10000baseKR_Full);
			if (pdata->fec_ability & MDIO_PMA_10GBR_FECABLE_ABLE)
				XGBE_SET_SUP(&pdata->phy, 10000baseR_FEC);
			phy_data->start_mode = XGBE_MODE_KR;
		}

		phy_data->phydev_mode = XGBE_MDIO_MODE_NONE;
		break;
	case XGBE_PORT_MODE_BACKPLANE_2500:
		XGBE_SET_SUP(&pdata->phy, Pause);
		XGBE_SET_SUP(&pdata->phy, Asym_Pause);
		XGBE_SET_SUP(&pdata->phy, Backplane);
		XGBE_SET_SUP(&pdata->phy, 2500baseX_Full);
		phy_data->start_mode = XGBE_MODE_KX_2500;

		phy_data->phydev_mode = XGBE_MDIO_MODE_NONE;
		break;

	/* MDIO 1GBase-T support */
	case XGBE_PORT_MODE_1000BASE_T:
		XGBE_SET_SUP(&pdata->phy, Autoneg);
		XGBE_SET_SUP(&pdata->phy, Pause);
		XGBE_SET_SUP(&pdata->phy, Asym_Pause);
		XGBE_SET_SUP(&pdata->phy, TP);
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_100) {
			XGBE_SET_SUP(&pdata->phy, 100baseT_Full);
			phy_data->start_mode = XGBE_MODE_SGMII_100;
		}
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_1000) {
			XGBE_SET_SUP(&pdata->phy, 1000baseT_Full);
			phy_data->start_mode = XGBE_MODE_SGMII_1000;
		}

		phy_data->phydev_mode = XGBE_MDIO_MODE_CL22;
		break;

	/* MDIO Base-X support */
	case XGBE_PORT_MODE_1000BASE_X:
		XGBE_SET_SUP(&pdata->phy, Autoneg);
		XGBE_SET_SUP(&pdata->phy, Pause);
		XGBE_SET_SUP(&pdata->phy, Asym_Pause);
		XGBE_SET_SUP(&pdata->phy, FIBRE);
		XGBE_SET_SUP(&pdata->phy, 1000baseX_Full);
		phy_data->start_mode = XGBE_MODE_X;

		phy_data->phydev_mode = XGBE_MDIO_MODE_CL22;
		break;

	/* MDIO NBase-T support */
	case XGBE_PORT_MODE_NBASE_T:
		XGBE_SET_SUP(&pdata->phy, Autoneg);
		XGBE_SET_SUP(&pdata->phy, Pause);
		XGBE_SET_SUP(&pdata->phy, Asym_Pause);
		XGBE_SET_SUP(&pdata->phy, TP);
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_100) {
			XGBE_SET_SUP(&pdata->phy, 100baseT_Full);
			phy_data->start_mode = XGBE_MODE_SGMII_100;
		}
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_1000) {
			XGBE_SET_SUP(&pdata->phy, 1000baseT_Full);
			phy_data->start_mode = XGBE_MODE_SGMII_1000;
		}
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_2500) {
			XGBE_SET_SUP(&pdata->phy, 2500baseT_Full);
			phy_data->start_mode = XGBE_MODE_KX_2500;
		}

		phy_data->phydev_mode = XGBE_MDIO_MODE_CL45;
		break;

	/* 10GBase-T support */
	case XGBE_PORT_MODE_10GBASE_T:
		XGBE_SET_SUP(&pdata->phy, Autoneg);
		XGBE_SET_SUP(&pdata->phy, Pause);
		XGBE_SET_SUP(&pdata->phy, Asym_Pause);
		XGBE_SET_SUP(&pdata->phy, TP);
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_100) {
			XGBE_SET_SUP(&pdata->phy, 100baseT_Full);
			phy_data->start_mode = XGBE_MODE_SGMII_100;
		}
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_1000) {
			XGBE_SET_SUP(&pdata->phy, 1000baseT_Full);
			phy_data->start_mode = XGBE_MODE_SGMII_1000;
		}
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_10000) {
			XGBE_SET_SUP(&pdata->phy, 10000baseT_Full);
			phy_data->start_mode = XGBE_MODE_KR;
		}

		phy_data->phydev_mode = XGBE_MDIO_MODE_CL45;
		break;

	/* 10GBase-R support */
	case XGBE_PORT_MODE_10GBASE_R:
		XGBE_SET_SUP(&pdata->phy, Autoneg);
		XGBE_SET_SUP(&pdata->phy, Pause);
		XGBE_SET_SUP(&pdata->phy, Asym_Pause);
		XGBE_SET_SUP(&pdata->phy, FIBRE);
		XGBE_SET_SUP(&pdata->phy, 10000baseSR_Full);
		XGBE_SET_SUP(&pdata->phy, 10000baseLR_Full);
		XGBE_SET_SUP(&pdata->phy, 10000baseLRM_Full);
		XGBE_SET_SUP(&pdata->phy, 10000baseER_Full);
		if (pdata->fec_ability & MDIO_PMA_10GBR_FECABLE_ABLE)
			XGBE_SET_SUP(&pdata->phy, 10000baseR_FEC);
		phy_data->start_mode = XGBE_MODE_SFI;

		phy_data->phydev_mode = XGBE_MDIO_MODE_NONE;
		break;

	/* SFP support */
	case XGBE_PORT_MODE_SFP:
		XGBE_SET_SUP(&pdata->phy, Autoneg);
		XGBE_SET_SUP(&pdata->phy, Pause);
		XGBE_SET_SUP(&pdata->phy, Asym_Pause);
		XGBE_SET_SUP(&pdata->phy, TP);
		XGBE_SET_SUP(&pdata->phy, FIBRE);
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_100)
			phy_data->start_mode = XGBE_MODE_SGMII_100;
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_1000)
			phy_data->start_mode = XGBE_MODE_SGMII_1000;
		if (phy_data->port_speeds & XGBE_PHY_PORT_SPEED_10000)
			phy_data->start_mode = XGBE_MODE_SFI;

		phy_data->phydev_mode = XGBE_MDIO_MODE_CL22;

		xgbe_phy_sfp_setup(pdata);
		DBGPR("%s: start %d mode %d adv 0x%x\n", __func__,
		    phy_data->start_mode, phy_data->phydev_mode,
		    pdata->phy.advertising);
		break;
	default:
		return (-EINVAL);
	}

	axgbe_printf(2, "%s: start %d mode %d adv 0x%x\n", __func__,
	    phy_data->start_mode, phy_data->phydev_mode, pdata->phy.advertising);

	DBGPR("%s: conn type %d mode %d\n", __func__,
	    phy_data->conn_type, phy_data->phydev_mode);
	if ((phy_data->conn_type & XGBE_CONN_TYPE_MDIO) &&
	    (phy_data->phydev_mode != XGBE_MDIO_MODE_NONE)) {
		ret = pdata->hw_if.set_ext_mii_mode(pdata, phy_data->mdio_addr,
		    phy_data->phydev_mode);
		if (ret) {
			axgbe_error("mdio port/clause not compatible (%d/%u)\n",
			    phy_data->mdio_addr, phy_data->phydev_mode);
			return (-EINVAL);
		}
	}

	if (phy_data->redrv && !phy_data->redrv_if) {
		ret = pdata->hw_if.set_ext_mii_mode(pdata, phy_data->redrv_addr,
		    XGBE_MDIO_MODE_CL22);
		if (ret) {
			axgbe_error("redriver mdio port not compatible (%u)\n",
			    phy_data->redrv_addr);
			return (-EINVAL);
		}
	}

	phy_data->phy_cdr_delay = XGBE_CDR_DELAY_INIT;

	if (phy_data->port_mode != XGBE_PORT_MODE_SFP) {
		ret = mii_attach(pdata->dev, &pdata->axgbe_miibus, pdata->netdev,
		    (ifm_change_cb_t)axgbe_ifmedia_upd,
		    (ifm_stat_cb_t)axgbe_ifmedia_sts, BMSR_DEFCAPMASK,
		    pdata->mdio_addr, MII_OFFSET_ANY, MIIF_FORCEANEG);

		if (ret){
			axgbe_printf(2, "mii attach failed with err=(%d)\n", ret);
			return (-EINVAL);
		}
	}

	DBGPR("%s: return success\n", __func__);

	return (0);
}

void
xgbe_init_function_ptrs_phy_v2(struct xgbe_phy_if *phy_if)
{
	struct xgbe_phy_impl_if *phy_impl = &phy_if->phy_impl;

	phy_impl->init			= xgbe_phy_init;
	phy_impl->exit			= xgbe_phy_exit;

	phy_impl->reset			= xgbe_phy_reset;
	phy_impl->start			= xgbe_phy_start;
	phy_impl->stop			= xgbe_phy_stop;

	phy_impl->link_status		= xgbe_phy_link_status;

	phy_impl->valid_speed		= xgbe_phy_valid_speed;

	phy_impl->use_mode		= xgbe_phy_use_mode;
	phy_impl->set_mode		= xgbe_phy_set_mode;
	phy_impl->get_mode		= xgbe_phy_get_mode;
	phy_impl->switch_mode		= xgbe_phy_switch_mode;
	phy_impl->cur_mode		= xgbe_phy_cur_mode;
	phy_impl->get_type		= xgbe_phy_get_type;

	phy_impl->an_mode		= xgbe_phy_an_mode;

	phy_impl->an_config		= xgbe_phy_an_config;

	phy_impl->an_advertising	= xgbe_phy_an_advertising;

	phy_impl->an_outcome		= xgbe_phy_an_outcome;

	phy_impl->an_pre		= xgbe_phy_an_pre;
	phy_impl->an_post		= xgbe_phy_an_post;

	phy_impl->kr_training_pre	= xgbe_phy_kr_training_pre;
	phy_impl->kr_training_post	= xgbe_phy_kr_training_post;

	phy_impl->module_info		= xgbe_phy_module_info;
	phy_impl->module_eeprom		= xgbe_phy_module_eeprom;
}
