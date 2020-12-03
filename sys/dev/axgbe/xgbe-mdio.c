/*
 * AMD 10Gb Ethernet driver
 *
 * Copyright (c) 2014-2016,2020 Advanced Micro Devices, Inc.
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
__FBSDID("$FreeBSD$");

#include "xgbe.h"
#include "xgbe-common.h"

static void xgbe_an_state_machine(struct xgbe_prv_data *pdata);

static void
xgbe_an37_clear_interrupts(struct xgbe_prv_data *pdata)
{
	int reg;

	reg = XMDIO_READ(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_STAT);
	reg &= ~XGBE_AN_CL37_INT_MASK;
	XMDIO_WRITE(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_STAT, reg);
}

static void
xgbe_an37_disable_interrupts(struct xgbe_prv_data *pdata)
{
	int reg;

	reg = XMDIO_READ(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_CTRL);
	reg &= ~XGBE_AN_CL37_INT_MASK;
	XMDIO_WRITE(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_CTRL, reg);

	reg = XMDIO_READ(pdata, MDIO_MMD_PCS, MDIO_PCS_DIG_CTRL);
	reg &= ~XGBE_PCS_CL37_BP;
	XMDIO_WRITE(pdata, MDIO_MMD_PCS, MDIO_PCS_DIG_CTRL, reg);
}

static void
xgbe_an37_enable_interrupts(struct xgbe_prv_data *pdata)
{
	int reg;

	reg = XMDIO_READ(pdata, MDIO_MMD_PCS, MDIO_PCS_DIG_CTRL);
	reg |= XGBE_PCS_CL37_BP;
	XMDIO_WRITE(pdata, MDIO_MMD_PCS, MDIO_PCS_DIG_CTRL, reg);

	reg = XMDIO_READ(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_CTRL);
	reg |= XGBE_AN_CL37_INT_MASK;
	XMDIO_WRITE(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_CTRL, reg);
}

static void
xgbe_an73_clear_interrupts(struct xgbe_prv_data *pdata)
{
	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_INT, 0);
}

static void
xgbe_an73_disable_interrupts(struct xgbe_prv_data *pdata)
{
	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_INTMASK, 0);
}

static void
xgbe_an73_enable_interrupts(struct xgbe_prv_data *pdata)
{
	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_INTMASK, XGBE_AN_CL73_INT_MASK);
}

static void
xgbe_an_enable_interrupts(struct xgbe_prv_data *pdata)
{
	switch (pdata->an_mode) {
	case XGBE_AN_MODE_CL73:
	case XGBE_AN_MODE_CL73_REDRV:
		xgbe_an73_enable_interrupts(pdata);
		break;
	case XGBE_AN_MODE_CL37:
	case XGBE_AN_MODE_CL37_SGMII:
		xgbe_an37_enable_interrupts(pdata);
		break;
	default:
		break;
	}
}

static void
xgbe_an_clear_interrupts_all(struct xgbe_prv_data *pdata)
{
	xgbe_an73_clear_interrupts(pdata);
	xgbe_an37_clear_interrupts(pdata);
}

static void
xgbe_kr_mode(struct xgbe_prv_data *pdata)
{
	/* Set MAC to 10G speed */
	pdata->hw_if.set_speed(pdata, SPEED_10000);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, XGBE_MODE_KR);
}

static void
xgbe_kx_2500_mode(struct xgbe_prv_data *pdata)
{
	/* Set MAC to 2.5G speed */
	pdata->hw_if.set_speed(pdata, SPEED_2500);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, XGBE_MODE_KX_2500);
}

static void
xgbe_kx_1000_mode(struct xgbe_prv_data *pdata)
{
	/* Set MAC to 1G speed */
	pdata->hw_if.set_speed(pdata, SPEED_1000);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, XGBE_MODE_KX_1000);
}

static void
xgbe_sfi_mode(struct xgbe_prv_data *pdata)
{
	/* If a KR re-driver is present, change to KR mode instead */
	if (pdata->kr_redrv)
		return (xgbe_kr_mode(pdata));

	/* Set MAC to 10G speed */
	pdata->hw_if.set_speed(pdata, SPEED_10000);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, XGBE_MODE_SFI);
}

static void
xgbe_x_mode(struct xgbe_prv_data *pdata)
{
	/* Set MAC to 1G speed */
	pdata->hw_if.set_speed(pdata, SPEED_1000);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, XGBE_MODE_X);
}

static void
xgbe_sgmii_1000_mode(struct xgbe_prv_data *pdata)
{
	/* Set MAC to 1G speed */
	pdata->hw_if.set_speed(pdata, SPEED_1000);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, XGBE_MODE_SGMII_1000);
}

static void
xgbe_sgmii_100_mode(struct xgbe_prv_data *pdata)
{
	/* Set MAC to 1G speed */
	pdata->hw_if.set_speed(pdata, SPEED_1000);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, XGBE_MODE_SGMII_100);
}

static enum xgbe_mode
xgbe_cur_mode(struct xgbe_prv_data *pdata)
{
	return (pdata->phy_if.phy_impl.cur_mode(pdata));
}

static bool
xgbe_in_kr_mode(struct xgbe_prv_data *pdata)
{
	return (xgbe_cur_mode(pdata) == XGBE_MODE_KR);
}

static void
xgbe_change_mode(struct xgbe_prv_data *pdata, enum xgbe_mode mode)
{
	switch (mode) {
	case XGBE_MODE_KX_1000:
		xgbe_kx_1000_mode(pdata);
		break;
	case XGBE_MODE_KX_2500:
		xgbe_kx_2500_mode(pdata);
		break;
	case XGBE_MODE_KR:
		xgbe_kr_mode(pdata);
		break;
	case XGBE_MODE_SGMII_100:
		xgbe_sgmii_100_mode(pdata);
		break;
	case XGBE_MODE_SGMII_1000:
		xgbe_sgmii_1000_mode(pdata);
		break;
	case XGBE_MODE_X:
		xgbe_x_mode(pdata);
		break;
	case XGBE_MODE_SFI:
		xgbe_sfi_mode(pdata);
		break;
	case XGBE_MODE_UNKNOWN:
		break;
	default:
		axgbe_error("invalid operation mode requested (%u)\n", mode);
	}
}

static void
xgbe_switch_mode(struct xgbe_prv_data *pdata)
{
	xgbe_change_mode(pdata, pdata->phy_if.phy_impl.switch_mode(pdata));
}

static bool
xgbe_set_mode(struct xgbe_prv_data *pdata, enum xgbe_mode mode)
{
	if (mode == xgbe_cur_mode(pdata))
		return (false);

	xgbe_change_mode(pdata, mode);

	return (true);
}

static bool
xgbe_use_mode(struct xgbe_prv_data *pdata, enum xgbe_mode mode)
{
	return (pdata->phy_if.phy_impl.use_mode(pdata, mode));
}

static void
xgbe_an37_set(struct xgbe_prv_data *pdata, bool enable, bool restart)
{
	unsigned int reg;

	reg = XMDIO_READ(pdata, MDIO_MMD_VEND2, MDIO_CTRL1);
	reg &= ~MDIO_VEND2_CTRL1_AN_ENABLE;

	if (enable)
		reg |= MDIO_VEND2_CTRL1_AN_ENABLE;

	if (restart)
		reg |= MDIO_VEND2_CTRL1_AN_RESTART;

	XMDIO_WRITE(pdata, MDIO_MMD_VEND2, MDIO_CTRL1, reg);
}

static void
xgbe_an37_restart(struct xgbe_prv_data *pdata)
{
	xgbe_an37_enable_interrupts(pdata);
	xgbe_an37_set(pdata, true, true);
}

static void
xgbe_an37_disable(struct xgbe_prv_data *pdata)
{
	xgbe_an37_set(pdata, false, false);
	xgbe_an37_disable_interrupts(pdata);
}

static void
xgbe_an73_set(struct xgbe_prv_data *pdata, bool enable, bool restart)
{
	unsigned int reg;

	/* Disable KR training for now */
	reg = XMDIO_READ(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL);
	reg &= ~XGBE_KR_TRAINING_ENABLE;
	XMDIO_WRITE(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL, reg);

	/* Update AN settings */
	reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_CTRL1);
	reg &= ~MDIO_AN_CTRL1_ENABLE;

	if (enable)
		reg |= MDIO_AN_CTRL1_ENABLE;

	if (restart)
		reg |= MDIO_AN_CTRL1_RESTART;

	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_CTRL1, reg);
}

static void
xgbe_an73_restart(struct xgbe_prv_data *pdata)
{
	xgbe_an73_enable_interrupts(pdata);
	xgbe_an73_set(pdata, true, true);
}

static void
xgbe_an73_disable(struct xgbe_prv_data *pdata)
{
	xgbe_an73_set(pdata, false, false);
	xgbe_an73_disable_interrupts(pdata);

	pdata->an_start = 0;
}

static void
xgbe_an_restart(struct xgbe_prv_data *pdata)
{
	if (pdata->phy_if.phy_impl.an_pre)
		pdata->phy_if.phy_impl.an_pre(pdata);

	switch (pdata->an_mode) {
	case XGBE_AN_MODE_CL73:
	case XGBE_AN_MODE_CL73_REDRV:
		xgbe_an73_restart(pdata);
		break;
	case XGBE_AN_MODE_CL37:
	case XGBE_AN_MODE_CL37_SGMII:
		xgbe_an37_restart(pdata);
		break;
	default:
		break;
	}
}

static void
xgbe_an_disable(struct xgbe_prv_data *pdata)
{
	if (pdata->phy_if.phy_impl.an_post)
		pdata->phy_if.phy_impl.an_post(pdata);

	switch (pdata->an_mode) {
	case XGBE_AN_MODE_CL73:
	case XGBE_AN_MODE_CL73_REDRV:
		xgbe_an73_disable(pdata);
		break;
	case XGBE_AN_MODE_CL37:
	case XGBE_AN_MODE_CL37_SGMII:
		xgbe_an37_disable(pdata);
		break;
	default:
		break;
	}
}

static void
xgbe_an_disable_all(struct xgbe_prv_data *pdata)
{
	xgbe_an73_disable(pdata);
	xgbe_an37_disable(pdata);
}

static enum xgbe_an
xgbe_an73_tx_training(struct xgbe_prv_data *pdata, enum xgbe_rx *state)
{
	unsigned int ad_reg, lp_reg, reg;

	*state = XGBE_RX_COMPLETE;

	/* If we're not in KR mode then we're done */
	if (!xgbe_in_kr_mode(pdata))
		return (XGBE_AN_PAGE_RECEIVED);

	/* Enable/Disable FEC */
	ad_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 2);
	lp_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_LPA + 2);

	reg = XMDIO_READ(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_FECCTRL);
	reg &= ~(MDIO_PMA_10GBR_FECABLE_ABLE | MDIO_PMA_10GBR_FECABLE_ERRABLE);
	if ((ad_reg & 0xc000) && (lp_reg & 0xc000))
		reg |= pdata->fec_ability;

	XMDIO_WRITE(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_FECCTRL, reg);

	/* Start KR training */
	if (pdata->phy_if.phy_impl.kr_training_pre)
		pdata->phy_if.phy_impl.kr_training_pre(pdata);

	/* Start KR training */
	reg = XMDIO_READ(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL);
	reg |= XGBE_KR_TRAINING_ENABLE;
	reg |= XGBE_KR_TRAINING_START;
	XMDIO_WRITE(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL, reg);

	if (pdata->phy_if.phy_impl.kr_training_post)
		pdata->phy_if.phy_impl.kr_training_post(pdata);

	return (XGBE_AN_PAGE_RECEIVED);
}

static enum xgbe_an
xgbe_an73_tx_xnp(struct xgbe_prv_data *pdata, enum xgbe_rx *state)
{
	uint16_t msg;

	*state = XGBE_RX_XNP;

	msg = XGBE_XNP_MCF_NULL_MESSAGE;
	msg |= XGBE_XNP_MP_FORMATTED;

	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_XNP + 2, 0);
	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_XNP + 1, 0);
	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_XNP, msg);

	return (XGBE_AN_PAGE_RECEIVED);
}

static enum xgbe_an
xgbe_an73_rx_bpa(struct xgbe_prv_data *pdata, enum xgbe_rx *state)
{
	unsigned int link_support;
	unsigned int reg, ad_reg, lp_reg;

	/* Read Base Ability register 2 first */
	reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_LPA + 1);

	/* Check for a supported mode, otherwise restart in a different one */
	link_support = xgbe_in_kr_mode(pdata) ? 0x80 : 0x20;
	if (!(reg & link_support))
		return (XGBE_AN_INCOMPAT_LINK);

	/* Check Extended Next Page support */
	ad_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE);
	lp_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_LPA);

	return (((ad_reg & XGBE_XNP_NP_EXCHANGE) ||
		(lp_reg & XGBE_XNP_NP_EXCHANGE))
	       ? xgbe_an73_tx_xnp(pdata, state)
	       : xgbe_an73_tx_training(pdata, state));
}

static enum xgbe_an
xgbe_an73_rx_xnp(struct xgbe_prv_data *pdata, enum xgbe_rx *state)
{
	unsigned int ad_reg, lp_reg;

	/* Check Extended Next Page support */
	ad_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_XNP);
	lp_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_LPX);

	return (((ad_reg & XGBE_XNP_NP_EXCHANGE) ||
		(lp_reg & XGBE_XNP_NP_EXCHANGE))
	       ? xgbe_an73_tx_xnp(pdata, state)
	       : xgbe_an73_tx_training(pdata, state));
}

static enum xgbe_an
xgbe_an73_page_received(struct xgbe_prv_data *pdata)
{
	enum xgbe_rx *state;
	unsigned long an_timeout;
	enum xgbe_an ret;

	if (!pdata->an_start) {
		pdata->an_start = ticks;
	} else {
		an_timeout = pdata->an_start +
		    ((uint64_t)XGBE_AN_MS_TIMEOUT * (uint64_t)hz) / 1000ull;
		if ((int)(ticks - an_timeout) > 0) {
			/* Auto-negotiation timed out, reset state */
			pdata->kr_state = XGBE_RX_BPA;
			pdata->kx_state = XGBE_RX_BPA;

			pdata->an_start = ticks;

			axgbe_printf(2, "CL73 AN timed out, resetting state\n");
		}
	}

	state = xgbe_in_kr_mode(pdata) ? &pdata->kr_state : &pdata->kx_state;

	switch (*state) {
	case XGBE_RX_BPA:
		ret = xgbe_an73_rx_bpa(pdata, state);
		break;

	case XGBE_RX_XNP:
		ret = xgbe_an73_rx_xnp(pdata, state);
		break;

	default:
		ret = XGBE_AN_ERROR;
	}

	return (ret);
}

static enum xgbe_an
xgbe_an73_incompat_link(struct xgbe_prv_data *pdata)
{
	/* Be sure we aren't looping trying to negotiate */
	if (xgbe_in_kr_mode(pdata)) {
		pdata->kr_state = XGBE_RX_ERROR;

		if (!(XGBE_ADV(&pdata->phy, 1000baseKX_Full)) &&
		    !(XGBE_ADV(&pdata->phy, 2500baseX_Full)))
			return (XGBE_AN_NO_LINK);

		if (pdata->kx_state != XGBE_RX_BPA)
			return (XGBE_AN_NO_LINK);
	} else {
		pdata->kx_state = XGBE_RX_ERROR;

		if (!(XGBE_ADV(&pdata->phy, 10000baseKR_Full)))
			return (XGBE_AN_NO_LINK);

		if (pdata->kr_state != XGBE_RX_BPA)
			return (XGBE_AN_NO_LINK);
	}

	xgbe_an_disable(pdata);

	xgbe_switch_mode(pdata);

	xgbe_an_restart(pdata);

	return (XGBE_AN_INCOMPAT_LINK);
}

static void
xgbe_an37_isr(struct xgbe_prv_data *pdata)
{
	unsigned int reg;

	/* Disable AN interrupts */
	xgbe_an37_disable_interrupts(pdata);

	/* Save the interrupt(s) that fired */
	reg = XMDIO_READ(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_STAT);
	pdata->an_int = reg & XGBE_AN_CL37_INT_MASK;
	pdata->an_status = reg & ~XGBE_AN_CL37_INT_MASK;

	if (pdata->an_int) {
		/* Clear the interrupt(s) that fired and process them */
		reg &= ~XGBE_AN_CL37_INT_MASK;
		XMDIO_WRITE(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_STAT, reg);

		xgbe_an_state_machine(pdata);
	} else {
		/* Enable AN interrupts */
		xgbe_an37_enable_interrupts(pdata);

		/* Reissue interrupt if status is not clear */
		if (pdata->vdata->irq_reissue_support)
			XP_IOWRITE(pdata, XP_INT_REISSUE_EN, 1 << 3);
	}
}

static void
xgbe_an73_isr(struct xgbe_prv_data *pdata)
{
	/* Disable AN interrupts */
	xgbe_an73_disable_interrupts(pdata);

	/* Save the interrupt(s) that fired */
	pdata->an_int = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_INT);

	if (pdata->an_int) {
		/* Clear the interrupt(s) that fired and process them */
		XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_INT, ~pdata->an_int);

		xgbe_an_state_machine(pdata);
	} else {
		/* Enable AN interrupts */
		xgbe_an73_enable_interrupts(pdata);

		/* Reissue interrupt if status is not clear */
		if (pdata->vdata->irq_reissue_support)
			XP_IOWRITE(pdata, XP_INT_REISSUE_EN, 1 << 3);
	}
}

static void
xgbe_an_isr_task(unsigned long data)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)data;

	axgbe_printf(2, "AN interrupt received\n");

	switch (pdata->an_mode) {
	case XGBE_AN_MODE_CL73:
	case XGBE_AN_MODE_CL73_REDRV:
		xgbe_an73_isr(pdata);
		break;
	case XGBE_AN_MODE_CL37:
	case XGBE_AN_MODE_CL37_SGMII:
		xgbe_an37_isr(pdata);
		break;
	default:
		break;
	}
}

static void
xgbe_an_combined_isr(struct xgbe_prv_data *pdata)
{
	xgbe_an_isr_task((unsigned long)pdata);
}

static const char *
xgbe_state_as_string(enum xgbe_an state)
{
	switch (state) {
	case XGBE_AN_READY:
		return ("Ready");
	case XGBE_AN_PAGE_RECEIVED:
		return ("Page-Received");
	case XGBE_AN_INCOMPAT_LINK:
		return ("Incompatible-Link");
	case XGBE_AN_COMPLETE:
		return ("Complete");
	case XGBE_AN_NO_LINK:
		return ("No-Link");
	case XGBE_AN_ERROR:
		return ("Error");
	default:
		return ("Undefined");
	}
}

static void
xgbe_an37_state_machine(struct xgbe_prv_data *pdata)
{
	enum xgbe_an cur_state = pdata->an_state;

	if (!pdata->an_int)
		return;

	if (pdata->an_int & XGBE_AN_CL37_INT_CMPLT) {
		pdata->an_state = XGBE_AN_COMPLETE;
		pdata->an_int &= ~XGBE_AN_CL37_INT_CMPLT;

		/* If SGMII is enabled, check the link status */
		if ((pdata->an_mode == XGBE_AN_MODE_CL37_SGMII) &&
		    !(pdata->an_status & XGBE_SGMII_AN_LINK_STATUS))
			pdata->an_state = XGBE_AN_NO_LINK;
	}

	axgbe_printf(2, "%s: CL37 AN %s\n", __func__,
	    xgbe_state_as_string(pdata->an_state));

	cur_state = pdata->an_state;

	switch (pdata->an_state) {
	case XGBE_AN_READY:
		break;

	case XGBE_AN_COMPLETE:
		axgbe_printf(2, "Auto negotiation successful\n");
		break;

	case XGBE_AN_NO_LINK:
		break;

	default:
		pdata->an_state = XGBE_AN_ERROR;
	}

	if (pdata->an_state == XGBE_AN_ERROR) {
		axgbe_printf(2, "error during auto-negotiation, state=%u\n",
		    cur_state);

		pdata->an_int = 0;
		xgbe_an37_clear_interrupts(pdata);
	}

	if (pdata->an_state >= XGBE_AN_COMPLETE) {
		pdata->an_result = pdata->an_state;
		pdata->an_state = XGBE_AN_READY;

		if (pdata->phy_if.phy_impl.an_post)
			pdata->phy_if.phy_impl.an_post(pdata);

		axgbe_printf(2, "CL37 AN result: %s\n",
		    xgbe_state_as_string(pdata->an_result));
	}

	axgbe_printf(2, "%s: an_state %d an_int %d an_mode %d an_status %d\n",
	     __func__, pdata->an_state, pdata->an_int, pdata->an_mode,
	     pdata->an_status);

	xgbe_an37_enable_interrupts(pdata);
}

static void
xgbe_an73_state_machine(struct xgbe_prv_data *pdata)
{
	enum xgbe_an cur_state = pdata->an_state;

	if (!pdata->an_int)
		goto out;

next_int:
	if (pdata->an_int & XGBE_AN_CL73_PG_RCV) {
		pdata->an_state = XGBE_AN_PAGE_RECEIVED;
		pdata->an_int &= ~XGBE_AN_CL73_PG_RCV;
	} else if (pdata->an_int & XGBE_AN_CL73_INC_LINK) {
		pdata->an_state = XGBE_AN_INCOMPAT_LINK;
		pdata->an_int &= ~XGBE_AN_CL73_INC_LINK;
	} else if (pdata->an_int & XGBE_AN_CL73_INT_CMPLT) {
		pdata->an_state = XGBE_AN_COMPLETE;
		pdata->an_int &= ~XGBE_AN_CL73_INT_CMPLT;
	} else {
		pdata->an_state = XGBE_AN_ERROR;
	}

again:
	axgbe_printf(2, "CL73 AN %s\n",
	    xgbe_state_as_string(pdata->an_state));

	cur_state = pdata->an_state;

	switch (pdata->an_state) {
	case XGBE_AN_READY:
		pdata->an_supported = 0;
		break;

	case XGBE_AN_PAGE_RECEIVED:
		pdata->an_state = xgbe_an73_page_received(pdata);
		pdata->an_supported++;
		break;

	case XGBE_AN_INCOMPAT_LINK:
		pdata->an_supported = 0;
		pdata->parallel_detect = 0;
		pdata->an_state = xgbe_an73_incompat_link(pdata);
		break;

	case XGBE_AN_COMPLETE:
		pdata->parallel_detect = pdata->an_supported ? 0 : 1;
		axgbe_printf(2, "%s successful\n",
		    pdata->an_supported ? "Auto negotiation"
		    : "Parallel detection");
		break;

	case XGBE_AN_NO_LINK:
		break;

	default:
		pdata->an_state = XGBE_AN_ERROR;
	}

	if (pdata->an_state == XGBE_AN_NO_LINK) {
		pdata->an_int = 0;
		xgbe_an73_clear_interrupts(pdata);
	} else if (pdata->an_state == XGBE_AN_ERROR) {
		axgbe_printf(2,
		    "error during auto-negotiation, state=%u\n",
		    cur_state);

		pdata->an_int = 0;
		xgbe_an73_clear_interrupts(pdata);
	}

	if (pdata->an_state >= XGBE_AN_COMPLETE) {
		pdata->an_result = pdata->an_state;
		pdata->an_state = XGBE_AN_READY;
		pdata->kr_state = XGBE_RX_BPA;
		pdata->kx_state = XGBE_RX_BPA;
		pdata->an_start = 0;

		if (pdata->phy_if.phy_impl.an_post)
			pdata->phy_if.phy_impl.an_post(pdata);

		axgbe_printf(2,  "CL73 AN result: %s\n",
		    xgbe_state_as_string(pdata->an_result));
	}

	if (cur_state != pdata->an_state)
		goto again;

	if (pdata->an_int)
		goto next_int;

out:
	/* Enable AN interrupts on the way out */
	xgbe_an73_enable_interrupts(pdata);
}

static void
xgbe_an_state_machine(struct xgbe_prv_data *pdata)
{
	sx_xlock(&pdata->an_mutex);

	switch (pdata->an_mode) {
	case XGBE_AN_MODE_CL73:
	case XGBE_AN_MODE_CL73_REDRV:
		xgbe_an73_state_machine(pdata);
		break;
	case XGBE_AN_MODE_CL37:
	case XGBE_AN_MODE_CL37_SGMII:
		xgbe_an37_state_machine(pdata);
		break;
	default:
		break;
	}

	/* Reissue interrupt if status is not clear */
	if (pdata->vdata->irq_reissue_support)
		XP_IOWRITE(pdata, XP_INT_REISSUE_EN, 1 << 3);

	sx_xunlock(&pdata->an_mutex);
}

static void
xgbe_an37_init(struct xgbe_prv_data *pdata)
{
	struct xgbe_phy local_phy;
	unsigned int reg;

	pdata->phy_if.phy_impl.an_advertising(pdata, &local_phy);

	axgbe_printf(2, "%s: advertising 0x%x\n", __func__, local_phy.advertising);

	/* Set up Advertisement register */
	reg = XMDIO_READ(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_ADVERTISE);
	if (XGBE_ADV(&local_phy, Pause))
		reg |= 0x100;
	else
		reg &= ~0x100;

	if (XGBE_ADV(&local_phy, Asym_Pause))
		reg |= 0x80;
	else
		reg &= ~0x80;

	/* Full duplex, but not half */
	reg |= XGBE_AN_CL37_FD_MASK;
	reg &= ~XGBE_AN_CL37_HD_MASK;

	axgbe_printf(2, "%s: Writing reg: 0x%x\n", __func__, reg);
	XMDIO_WRITE(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_ADVERTISE, reg);

	/* Set up the Control register */
	reg = XMDIO_READ(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_CTRL);
	axgbe_printf(2, "%s: AN_ADVERTISE reg 0x%x an_mode %d\n", __func__,
	    reg, pdata->an_mode);
	reg &= ~XGBE_AN_CL37_TX_CONFIG_MASK;
	reg &= ~XGBE_AN_CL37_PCS_MODE_MASK;

	switch (pdata->an_mode) {
	case XGBE_AN_MODE_CL37:
		reg |= XGBE_AN_CL37_PCS_MODE_BASEX;
		break;
	case XGBE_AN_MODE_CL37_SGMII:
		reg |= XGBE_AN_CL37_PCS_MODE_SGMII;
		break;
	default:
		break;
	}

	reg |= XGBE_AN_CL37_MII_CTRL_8BIT;
	axgbe_printf(2, "%s: Writing reg: 0x%x\n", __func__, reg);
	XMDIO_WRITE(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_CTRL, reg);

	axgbe_printf(2, "CL37 AN (%s) initialized\n",
	    (pdata->an_mode == XGBE_AN_MODE_CL37) ? "BaseX" : "SGMII");
}

static void
xgbe_an73_init(struct xgbe_prv_data *pdata)
{
	/* 
	 * This local_phy is needed because phy-v2 alters the
	 * advertising flag variable. so phy-v1 an_advertising is just copying
	 */
	struct xgbe_phy local_phy;
	unsigned int reg;

	pdata->phy_if.phy_impl.an_advertising(pdata, &local_phy);

	/* Set up Advertisement register 3 first */
	reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 2);
	if (XGBE_ADV(&local_phy, 10000baseR_FEC))
		reg |= 0xc000;
	else
		reg &= ~0xc000;

	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 2, reg);

	/* Set up Advertisement register 2 next */
	reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 1);
	if (XGBE_ADV(&local_phy, 10000baseKR_Full))
		reg |= 0x80;
	else
		reg &= ~0x80;

	if (XGBE_ADV(&local_phy, 1000baseKX_Full) ||
	    XGBE_ADV(&local_phy, 2500baseX_Full))
		reg |= 0x20;
	else
		reg &= ~0x20;

	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 1, reg);

	/* Set up Advertisement register 1 last */
	reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE);
	if (XGBE_ADV(&local_phy, Pause))
		reg |= 0x400;
	else
		reg &= ~0x400;

	if (XGBE_ADV(&local_phy, Asym_Pause))
		reg |= 0x800;
	else
		reg &= ~0x800;

	/* We don't intend to perform XNP */
	reg &= ~XGBE_XNP_NP_EXCHANGE;

	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE, reg);

	axgbe_printf(2, "CL73 AN initialized\n");
}

static void
xgbe_an_init(struct xgbe_prv_data *pdata)
{
	/* Set up advertisement registers based on current settings */
	pdata->an_mode = pdata->phy_if.phy_impl.an_mode(pdata);
	axgbe_printf(2, "%s: setting up an_mode %d\n", __func__, pdata->an_mode);

	switch (pdata->an_mode) {
	case XGBE_AN_MODE_CL73:
	case XGBE_AN_MODE_CL73_REDRV:
		xgbe_an73_init(pdata);
		break;
	case XGBE_AN_MODE_CL37:
	case XGBE_AN_MODE_CL37_SGMII:
		xgbe_an37_init(pdata);
		break;
	default:
		break;
	}
}

static const char *
xgbe_phy_fc_string(struct xgbe_prv_data *pdata)
{
	if (pdata->tx_pause && pdata->rx_pause)
		return ("rx/tx");
	else if (pdata->rx_pause)
		return ("rx");
	else if (pdata->tx_pause)
		return ("tx");
	else
		return ("off");
}

static const char *
xgbe_phy_speed_string(int speed)
{
	switch (speed) {
	case SPEED_100:
		return ("100Mbps");
	case SPEED_1000:
		return ("1Gbps");
	case SPEED_2500:
		return ("2.5Gbps");
	case SPEED_10000:
		return ("10Gbps");
	case SPEED_UNKNOWN:
		return ("Unknown");
	default:
		return ("Unsupported");
	}
}

static void
xgbe_phy_print_status(struct xgbe_prv_data *pdata)
{
	if (pdata->phy.link)
		axgbe_printf(0,
		    "Link is UP - %s/%s - flow control %s\n",
		    xgbe_phy_speed_string(pdata->phy.speed),
		    pdata->phy.duplex == DUPLEX_FULL ? "Full" : "Half",
		    xgbe_phy_fc_string(pdata));
	else
		axgbe_printf(0, "Link is DOWN\n");
}

static void
xgbe_phy_adjust_link(struct xgbe_prv_data *pdata)
{
	int new_state = 0;

	axgbe_printf(1, "link %d/%d tx %d/%d rx %d/%d speed %d/%d autoneg %d/%d\n",
	    pdata->phy_link, pdata->phy.link,
	    pdata->tx_pause, pdata->phy.tx_pause,
	    pdata->rx_pause, pdata->phy.rx_pause,
	    pdata->phy_speed, pdata->phy.speed,
	    pdata->pause_autoneg, pdata->phy.pause_autoneg);

	if (pdata->phy.link) {
		/* Flow control support */
		pdata->pause_autoneg = pdata->phy.pause_autoneg;

		if (pdata->tx_pause != pdata->phy.tx_pause) {
			new_state = 1;
			axgbe_printf(2, "tx pause %d/%d\n", pdata->tx_pause,
			    pdata->phy.tx_pause);
			pdata->tx_pause = pdata->phy.tx_pause;
			pdata->hw_if.config_tx_flow_control(pdata);
		}

		if (pdata->rx_pause != pdata->phy.rx_pause) {
			new_state = 1;
			axgbe_printf(2, "rx pause %d/%d\n", pdata->rx_pause,
			    pdata->phy.rx_pause);
			pdata->rx_pause = pdata->phy.rx_pause;
			pdata->hw_if.config_rx_flow_control(pdata);
		}

		/* Speed support */
		if (pdata->phy_speed != pdata->phy.speed) {
			new_state = 1;
			pdata->phy_speed = pdata->phy.speed;
		}

		if (pdata->phy_link != pdata->phy.link) {
			new_state = 1;
			pdata->phy_link = pdata->phy.link;
		}
	} else if (pdata->phy_link) {
		new_state = 1;
		pdata->phy_link = 0;
		pdata->phy_speed = SPEED_UNKNOWN;
	}

	axgbe_printf(2, "phy_link %d Link %d new_state %d\n", pdata->phy_link,
	    pdata->phy.link, new_state);

	if (new_state)
		xgbe_phy_print_status(pdata);
}

static bool
xgbe_phy_valid_speed(struct xgbe_prv_data *pdata, int speed)
{
	return (pdata->phy_if.phy_impl.valid_speed(pdata, speed));
}

static int
xgbe_phy_config_fixed(struct xgbe_prv_data *pdata)
{
	enum xgbe_mode mode;

	axgbe_printf(2, "fixed PHY configuration\n");

	/* Disable auto-negotiation */
	xgbe_an_disable(pdata);

	/* Set specified mode for specified speed */
	mode = pdata->phy_if.phy_impl.get_mode(pdata, pdata->phy.speed);
	switch (mode) {
	case XGBE_MODE_KX_1000:
	case XGBE_MODE_KX_2500:
	case XGBE_MODE_KR:
	case XGBE_MODE_SGMII_100:
	case XGBE_MODE_SGMII_1000:
	case XGBE_MODE_X:
	case XGBE_MODE_SFI:
		break;
	case XGBE_MODE_UNKNOWN:
	default:
		return (-EINVAL);
	}

	/* Validate duplex mode */
	if (pdata->phy.duplex != DUPLEX_FULL)
		return (-EINVAL);

	xgbe_set_mode(pdata, mode);

	return (0);
}

static int
__xgbe_phy_config_aneg(struct xgbe_prv_data *pdata, bool set_mode)
{
	int ret;
	unsigned int reg = 0;

	sx_xlock(&pdata->an_mutex);

	set_bit(XGBE_LINK_INIT, &pdata->dev_state);
	pdata->link_check = ticks;

	ret = pdata->phy_if.phy_impl.an_config(pdata);
	if (ret) {
		axgbe_error("%s: an_config fail %d\n", __func__, ret);
		goto out;
	}

	if (pdata->phy.autoneg != AUTONEG_ENABLE) {
		ret = xgbe_phy_config_fixed(pdata);
		if (ret || !pdata->kr_redrv) {
			if (ret)
				axgbe_error("%s: fix conf fail %d\n", __func__, ret);
			goto out;
		}

		axgbe_printf(2, "AN redriver support\n");
	} else
		axgbe_printf(2, "AN PHY configuration\n");

	/* Disable auto-negotiation interrupt */
	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_INTMASK, 0);
	reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_INTMASK);
	axgbe_printf(2, "%s: set_mode %d AN int reg value 0x%x\n", __func__,
	    set_mode, reg);

	/* Clear any auto-negotitation interrupts */
	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_INT, 0);

	/* Start auto-negotiation in a supported mode */
	if (set_mode) {
		/* Start auto-negotiation in a supported mode */
		if (xgbe_use_mode(pdata, XGBE_MODE_KR)) {
			xgbe_set_mode(pdata, XGBE_MODE_KR);
		} else if (xgbe_use_mode(pdata, XGBE_MODE_KX_2500)) {
			xgbe_set_mode(pdata, XGBE_MODE_KX_2500);
		} else if (xgbe_use_mode(pdata, XGBE_MODE_KX_1000)) {
			xgbe_set_mode(pdata, XGBE_MODE_KX_1000);
		} else if (xgbe_use_mode(pdata, XGBE_MODE_SFI)) {
			xgbe_set_mode(pdata, XGBE_MODE_SFI);
		} else if (xgbe_use_mode(pdata, XGBE_MODE_X)) {
			xgbe_set_mode(pdata, XGBE_MODE_X);
		} else if (xgbe_use_mode(pdata, XGBE_MODE_SGMII_1000)) {
			xgbe_set_mode(pdata, XGBE_MODE_SGMII_1000);
		} else if (xgbe_use_mode(pdata, XGBE_MODE_SGMII_100)) {
			xgbe_set_mode(pdata, XGBE_MODE_SGMII_100);
		} else {
			XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_INTMASK, 0x07);
			ret = -EINVAL;
			goto out;
		}
	}

	/* Disable and stop any in progress auto-negotiation */
	xgbe_an_disable_all(pdata);

	/* Clear any auto-negotitation interrupts */
	xgbe_an_clear_interrupts_all(pdata);

	pdata->an_result = XGBE_AN_READY;
	pdata->an_state = XGBE_AN_READY;
	pdata->kr_state = XGBE_RX_BPA;
	pdata->kx_state = XGBE_RX_BPA;

	/* Re-enable auto-negotiation interrupt */
	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_INTMASK, 0x07);
	reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_INTMASK);

	/* Set up advertisement registers based on current settings */
	xgbe_an_init(pdata);

	/* Enable and start auto-negotiation */
	xgbe_an_restart(pdata);

out:
	if (ret) {
		axgbe_printf(0, "%s: set_mode %d AN int reg value 0x%x ret value %d\n",
		   __func__, set_mode, reg, ret);
		set_bit(XGBE_LINK_ERR, &pdata->dev_state);
	} else
		clear_bit(XGBE_LINK_ERR, &pdata->dev_state);

	sx_unlock(&pdata->an_mutex);

	return (ret);
}

static int
xgbe_phy_config_aneg(struct xgbe_prv_data *pdata)
{
	return (__xgbe_phy_config_aneg(pdata, true));
}

static int
xgbe_phy_reconfig_aneg(struct xgbe_prv_data *pdata)
{
	return (__xgbe_phy_config_aneg(pdata, false));
}

static bool
xgbe_phy_aneg_done(struct xgbe_prv_data *pdata)
{
	return (pdata->an_result == XGBE_AN_COMPLETE);
}

static void
xgbe_check_link_timeout(struct xgbe_prv_data *pdata)
{
	unsigned long link_timeout;

	link_timeout = pdata->link_check + (XGBE_LINK_TIMEOUT * hz);
	if ((int)(ticks - link_timeout) > 0) {
		axgbe_printf(2, "AN link timeout\n");
		xgbe_phy_config_aneg(pdata);
	}
}

static enum xgbe_mode
xgbe_phy_status_aneg(struct xgbe_prv_data *pdata)
{
	return (pdata->phy_if.phy_impl.an_outcome(pdata));
}

static void
xgbe_phy_status_result(struct xgbe_prv_data *pdata)
{
	enum xgbe_mode mode;

	XGBE_ZERO_LP_ADV(&pdata->phy);

	if ((pdata->phy.autoneg != AUTONEG_ENABLE) || pdata->parallel_detect)
		mode = xgbe_cur_mode(pdata);
	else
		mode = xgbe_phy_status_aneg(pdata);

	axgbe_printf(3, "%s: xgbe mode %d\n", __func__, mode);
	switch (mode) {
	case XGBE_MODE_SGMII_100:
		pdata->phy.speed = SPEED_100;
		break;
	case XGBE_MODE_X:
	case XGBE_MODE_KX_1000:
	case XGBE_MODE_SGMII_1000:
		pdata->phy.speed = SPEED_1000;
		break;
	case XGBE_MODE_KX_2500:
		pdata->phy.speed = SPEED_2500;
		break;
	case XGBE_MODE_KR:
	case XGBE_MODE_SFI:
		pdata->phy.speed = SPEED_10000;
		break;
	case XGBE_MODE_UNKNOWN:
	default:
		axgbe_printf(1, "%s: unknown mode\n", __func__);
		pdata->phy.speed = SPEED_UNKNOWN;
	}

	pdata->phy.duplex = DUPLEX_FULL;
	axgbe_printf(2, "%s: speed %d duplex %d\n", __func__, pdata->phy.speed,
	    pdata->phy.duplex);

	if (xgbe_set_mode(pdata, mode) && pdata->an_again)
		xgbe_phy_reconfig_aneg(pdata);
}

static void
xgbe_phy_status(struct xgbe_prv_data *pdata)
{
	bool link_aneg;
	int an_restart;

	if (test_bit(XGBE_LINK_ERR, &pdata->dev_state)) {
		axgbe_error("%s: LINK_ERR\n", __func__);
		pdata->phy.link = 0;
		goto adjust_link;
	}

	link_aneg = (pdata->phy.autoneg == AUTONEG_ENABLE);
	axgbe_printf(3, "link_aneg - %d\n", link_aneg);

	/* Get the link status. Link status is latched low, so read
	 * once to clear and then read again to get current state
	 */
	pdata->phy.link = pdata->phy_if.phy_impl.link_status(pdata,
	    &an_restart);

	axgbe_printf(1, "link_status returned Link:%d an_restart:%d aneg:%d\n",
	    pdata->phy.link, an_restart, link_aneg);

	if (an_restart) {
		xgbe_phy_config_aneg(pdata);
		return;
	}

	if (pdata->phy.link) {
		axgbe_printf(2, "Link Active\n");
		if (link_aneg && !xgbe_phy_aneg_done(pdata)) {
			axgbe_printf(1, "phy_link set check timeout\n");
			xgbe_check_link_timeout(pdata);
			return;
		}

		axgbe_printf(2, "%s: Link write phy_status result\n", __func__);
		xgbe_phy_status_result(pdata);

		if (test_bit(XGBE_LINK_INIT, &pdata->dev_state))
			clear_bit(XGBE_LINK_INIT, &pdata->dev_state);

	} else {
		axgbe_printf(2, "Link Deactive\n");
		if (test_bit(XGBE_LINK_INIT, &pdata->dev_state)) {
			axgbe_printf(1, "phy_link not set check timeout\n");
			xgbe_check_link_timeout(pdata);

			if (link_aneg) {
				axgbe_printf(2, "link_aneg case\n");
				return;
			}
		}

		xgbe_phy_status_result(pdata);

	}

adjust_link:
	axgbe_printf(2, "%s: Link %d\n", __func__, pdata->phy.link);
	xgbe_phy_adjust_link(pdata);
}

static void
xgbe_phy_stop(struct xgbe_prv_data *pdata)
{
	axgbe_printf(2, "stopping PHY\n");

	if (!pdata->phy_started)
		return;

	/* Indicate the PHY is down */
	pdata->phy_started = 0;

	/* Disable auto-negotiation */
	xgbe_an_disable_all(pdata);

	pdata->phy_if.phy_impl.stop(pdata);

	pdata->phy.link = 0;

	xgbe_phy_adjust_link(pdata);
}

static int
xgbe_phy_start(struct xgbe_prv_data *pdata)
{
	int ret;

	DBGPR("-->xgbe_phy_start\n");

	ret = pdata->phy_if.phy_impl.start(pdata);
	if (ret) {
		axgbe_error("%s: impl start ret %d\n", __func__, ret);
		return (ret);
	}

	/* Set initial mode - call the mode setting routines
	 * directly to insure we are properly configured
	 */
	if (xgbe_use_mode(pdata, XGBE_MODE_KR)) {
		axgbe_printf(2, "%s: KR\n", __func__);
		xgbe_kr_mode(pdata);
	} else if (xgbe_use_mode(pdata, XGBE_MODE_KX_2500)) {
		axgbe_printf(2, "%s: KX 2500\n", __func__);
		xgbe_kx_2500_mode(pdata);
	} else if (xgbe_use_mode(pdata, XGBE_MODE_KX_1000)) {
		axgbe_printf(2, "%s: KX 1000\n", __func__);
		xgbe_kx_1000_mode(pdata);
	} else if (xgbe_use_mode(pdata, XGBE_MODE_SFI)) {
		axgbe_printf(2, "%s: SFI\n", __func__);
		xgbe_sfi_mode(pdata);
	} else if (xgbe_use_mode(pdata, XGBE_MODE_X)) {
		axgbe_printf(2, "%s: X\n", __func__);
		xgbe_x_mode(pdata);
	} else if (xgbe_use_mode(pdata, XGBE_MODE_SGMII_1000)) {
		axgbe_printf(2, "%s: SGMII 1000\n", __func__);
		xgbe_sgmii_1000_mode(pdata);
	} else if (xgbe_use_mode(pdata, XGBE_MODE_SGMII_100)) {
		axgbe_printf(2, "%s: SGMII 100\n", __func__);
		xgbe_sgmii_100_mode(pdata);
	} else {
		axgbe_error("%s: invalid mode\n", __func__);
		ret = -EINVAL;
		goto err_stop;
	}

	/* Indicate the PHY is up and running */
	pdata->phy_started = 1;

	/* Set up advertisement registers based on current settings */
	xgbe_an_init(pdata);

	/* Enable auto-negotiation interrupts */
	xgbe_an_enable_interrupts(pdata);

	ret = xgbe_phy_config_aneg(pdata);
	if (ret)
		axgbe_error("%s: phy_config_aneg %d\n", __func__, ret);

	return (ret);

err_stop:
	pdata->phy_if.phy_impl.stop(pdata);

	return (ret);
}

static int
xgbe_phy_reset(struct xgbe_prv_data *pdata)
{
	int ret;

	ret = pdata->phy_if.phy_impl.reset(pdata);
	if (ret) {
		axgbe_error("%s: impl phy reset %d\n", __func__, ret);
		return (ret);
	}

	/* Disable auto-negotiation for now */
	xgbe_an_disable_all(pdata);

	/* Clear auto-negotiation interrupts */
	xgbe_an_clear_interrupts_all(pdata);

	return (0);
}

static int
xgbe_phy_best_advertised_speed(struct xgbe_prv_data *pdata)
{

	if (XGBE_ADV(&pdata->phy, 10000baseKR_Full))
		return (SPEED_10000);
	else if (XGBE_ADV(&pdata->phy, 10000baseT_Full))
		return (SPEED_10000);
	else if (XGBE_ADV(&pdata->phy, 2500baseX_Full))
		return (SPEED_2500);
	else if (XGBE_ADV(&pdata->phy, 2500baseT_Full))
		return (SPEED_2500);
	else if (XGBE_ADV(&pdata->phy, 1000baseKX_Full))
		return (SPEED_1000);
	else if (XGBE_ADV(&pdata->phy, 1000baseT_Full))
		return (SPEED_1000);
	else if (XGBE_ADV(&pdata->phy, 100baseT_Full))
		return (SPEED_100);

	return (SPEED_UNKNOWN);
}

static void
xgbe_phy_exit(struct xgbe_prv_data *pdata)
{
	pdata->phy_if.phy_impl.exit(pdata);
}

static int
xgbe_phy_init(struct xgbe_prv_data *pdata)
{
	int ret = 0;

	DBGPR("-->xgbe_phy_init\n");

	sx_init(&pdata->an_mutex, "axgbe AN lock");
	pdata->mdio_mmd = MDIO_MMD_PCS;

	/* Initialize supported features */
	pdata->fec_ability = XMDIO_READ(pdata, MDIO_MMD_PMAPMD,
					MDIO_PMA_10GBR_FECABLE);
	pdata->fec_ability &= (MDIO_PMA_10GBR_FECABLE_ABLE |
			       MDIO_PMA_10GBR_FECABLE_ERRABLE);

	/* Setup the phy (including supported features) */
	ret = pdata->phy_if.phy_impl.init(pdata);
	if (ret)
		return (ret);

	/* Copy supported link modes to advertising link modes */
	XGBE_LM_COPY(&pdata->phy, advertising, &pdata->phy, supported);

	pdata->phy.address = 0;

	if (XGBE_ADV(&pdata->phy, Autoneg)) {
		pdata->phy.autoneg = AUTONEG_ENABLE;
		pdata->phy.speed = SPEED_UNKNOWN;
		pdata->phy.duplex = DUPLEX_UNKNOWN;
	} else {
		pdata->phy.autoneg = AUTONEG_DISABLE;
		pdata->phy.speed = xgbe_phy_best_advertised_speed(pdata);
		pdata->phy.duplex = DUPLEX_FULL;
	}

	pdata->phy.link = 0;

	pdata->phy.pause_autoneg = pdata->pause_autoneg;
	pdata->phy.tx_pause = pdata->tx_pause;
	pdata->phy.rx_pause = pdata->rx_pause;

	/* Fix up Flow Control advertising */
	XGBE_CLR_ADV(&pdata->phy, Pause);
	XGBE_CLR_ADV(&pdata->phy, Asym_Pause);

	if (pdata->rx_pause) {
		XGBE_SET_ADV(&pdata->phy, Pause);
		XGBE_SET_ADV(&pdata->phy, Asym_Pause);
	}

	if (pdata->tx_pause) {
		if (XGBE_ADV(&pdata->phy, Asym_Pause))
			XGBE_CLR_ADV(&pdata->phy, Asym_Pause);
		else
			XGBE_SET_ADV(&pdata->phy, Asym_Pause);
	}

	return (0);
}

void
xgbe_init_function_ptrs_phy(struct xgbe_phy_if *phy_if)
{
	phy_if->phy_init	= xgbe_phy_init;
	phy_if->phy_exit	= xgbe_phy_exit;

	phy_if->phy_reset       = xgbe_phy_reset;
	phy_if->phy_start       = xgbe_phy_start;
	phy_if->phy_stop	= xgbe_phy_stop;

	phy_if->phy_status      = xgbe_phy_status;
	phy_if->phy_config_aneg = xgbe_phy_config_aneg;

	phy_if->phy_valid_speed = xgbe_phy_valid_speed;

	phy_if->an_isr		= xgbe_an_combined_isr;
}
