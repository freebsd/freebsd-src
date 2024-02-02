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

#define XGBE_ABORT_COUNT	500
#define XGBE_DISABLE_COUNT	1000

#define XGBE_STD_SPEED		1

#define XGBE_INTR_RX_FULL	BIT(IC_RAW_INTR_STAT_RX_FULL_INDEX)
#define XGBE_INTR_TX_EMPTY	BIT(IC_RAW_INTR_STAT_TX_EMPTY_INDEX)
#define XGBE_INTR_TX_ABRT	BIT(IC_RAW_INTR_STAT_TX_ABRT_INDEX)
#define XGBE_INTR_STOP_DET	BIT(IC_RAW_INTR_STAT_STOP_DET_INDEX)
#define XGBE_DEFAULT_INT_MASK	(XGBE_INTR_RX_FULL  |	\
				 XGBE_INTR_TX_EMPTY |	\
				 XGBE_INTR_TX_ABRT  |	\
				 XGBE_INTR_STOP_DET)

#define XGBE_I2C_READ		BIT(8)
#define XGBE_I2C_STOP		BIT(9)

static int
xgbe_i2c_abort(struct xgbe_prv_data *pdata)
{
	unsigned int wait = XGBE_ABORT_COUNT;

	/* Must be enabled to recognize the abort request */
	XI2C_IOWRITE_BITS(pdata, IC_ENABLE, EN, 1);

	/* Issue the abort */
	XI2C_IOWRITE_BITS(pdata, IC_ENABLE, ABORT, 1);

	while (wait--) {
		if (!XI2C_IOREAD_BITS(pdata, IC_ENABLE, ABORT))
			return (0);

		DELAY(500);
	}

	return (-EBUSY);
}

static int
xgbe_i2c_set_enable(struct xgbe_prv_data *pdata, bool enable)
{
	unsigned int wait = XGBE_DISABLE_COUNT;
	unsigned int mode = enable ? 1 : 0;

	while (wait--) {
		XI2C_IOWRITE_BITS(pdata, IC_ENABLE, EN, mode);
		if (XI2C_IOREAD_BITS(pdata, IC_ENABLE_STATUS, EN) == mode)
			return (0);

		DELAY(100);
	}

	return (-EBUSY);
}

static int
xgbe_i2c_disable(struct xgbe_prv_data *pdata)
{
	unsigned int ret;

	ret = xgbe_i2c_set_enable(pdata, false);
	if (ret) {
		/* Disable failed, try an abort */
		ret = xgbe_i2c_abort(pdata);
		if (ret) {
			axgbe_error("%s: i2c_abort %d\n", __func__, ret);
			return (ret);
		}

		/* Abort succeeded, try to disable again */
		ret = xgbe_i2c_set_enable(pdata, false);
	}

	axgbe_printf(3, "%s: final i2c_disable %d\n", __func__, ret);
	return (ret);
}

static int
xgbe_i2c_enable(struct xgbe_prv_data *pdata)
{
	return (xgbe_i2c_set_enable(pdata, true));
}

static void
xgbe_i2c_clear_all_interrupts(struct xgbe_prv_data *pdata)
{
	XI2C_IOREAD(pdata, IC_CLR_INTR);
}

static void
xgbe_i2c_disable_interrupts(struct xgbe_prv_data *pdata)
{
	XI2C_IOWRITE(pdata, IC_INTR_MASK, 0);
}

static void
xgbe_i2c_enable_interrupts(struct xgbe_prv_data *pdata)
{
	XI2C_IOWRITE(pdata, IC_INTR_MASK, XGBE_DEFAULT_INT_MASK);
}

static void
xgbe_i2c_write(struct xgbe_prv_data *pdata)
{
	struct xgbe_i2c_op_state *state = &pdata->i2c.op_state;
	unsigned int tx_slots, cmd;

	/* Configured to never receive Rx overflows, so fill up Tx fifo */
	tx_slots = pdata->i2c.tx_fifo_size - XI2C_IOREAD(pdata, IC_TXFLR);
	axgbe_printf(3, "%s: tx_slots %d tx_len %d\n", __func__, tx_slots,
	    state->tx_len);

	while (tx_slots && state->tx_len) {
		if (state->op->cmd == XGBE_I2C_CMD_READ)
			cmd = XGBE_I2C_READ;
		else
			cmd = *state->tx_buf++;

		axgbe_printf(3, "%s: cmd %d tx_len %d\n", __func__, cmd,
		    state->tx_len);

		if (state->tx_len == 1)
			XI2C_SET_BITS(cmd, IC_DATA_CMD, STOP, 1);

		XI2C_IOWRITE(pdata, IC_DATA_CMD, cmd);

		tx_slots--;
		state->tx_len--;
	}

	/* No more Tx operations, so ignore TX_EMPTY and return */
	if (!state->tx_len)
		XI2C_IOWRITE_BITS(pdata, IC_INTR_MASK, TX_EMPTY, 0);
}

static void
xgbe_i2c_read(struct xgbe_prv_data *pdata)
{
	struct xgbe_i2c_op_state *state = &pdata->i2c.op_state;
	unsigned int rx_slots;

	/* Anything to be read? */
	axgbe_printf(3, "%s: op cmd %d\n", __func__, state->op->cmd);
	if (state->op->cmd != XGBE_I2C_CMD_READ)
		return;

	rx_slots = XI2C_IOREAD(pdata, IC_RXFLR);
	axgbe_printf(3, "%s: rx_slots %d rx_len %d\n", __func__, rx_slots,
	    state->rx_len);

	while (rx_slots && state->rx_len) {
		*state->rx_buf++ = XI2C_IOREAD(pdata, IC_DATA_CMD);
		state->rx_len--;
		rx_slots--;
	}
}

static void
xgbe_i2c_clear_isr_interrupts(struct xgbe_prv_data *pdata, unsigned int isr)
{
	struct xgbe_i2c_op_state *state = &pdata->i2c.op_state;

	if (isr & XGBE_INTR_TX_ABRT) {
		state->tx_abort_source = XI2C_IOREAD(pdata, IC_TX_ABRT_SOURCE);
		XI2C_IOREAD(pdata, IC_CLR_TX_ABRT);
	}

	if (isr & XGBE_INTR_STOP_DET)
		XI2C_IOREAD(pdata, IC_CLR_STOP_DET);
}

static void
xgbe_i2c_isr(void *data)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)data;
	struct xgbe_i2c_op_state *state = &pdata->i2c.op_state;
	unsigned int isr;

	isr = XI2C_IOREAD(pdata, IC_RAW_INTR_STAT);
	axgbe_printf(3, "%s: isr 0x%x\n", __func__, isr);
	if (!isr)
		goto reissue_check;

	axgbe_printf(3, "%s: I2C interrupt status=%#010x\n", __func__, isr);

	xgbe_i2c_clear_isr_interrupts(pdata, isr);

	if (isr & XGBE_INTR_TX_ABRT) {
		axgbe_printf(1, "%s: I2C TX_ABRT received (%#010x) for target "
		    "%#04x\n", __func__, state->tx_abort_source,
		    state->op->target);

		xgbe_i2c_disable_interrupts(pdata);

		state->ret = -EIO;
		goto out;
	}

	/* Check for data in the Rx fifo */
	xgbe_i2c_read(pdata);

	/* Fill up the Tx fifo next */
	xgbe_i2c_write(pdata);

out:
	/* Complete on an error or STOP condition */
	axgbe_printf(3, "%s: ret %d stop %d\n", __func__, state->ret,
	    XI2C_GET_BITS(isr, IC_RAW_INTR_STAT, STOP_DET));

	if (state->ret || XI2C_GET_BITS(isr, IC_RAW_INTR_STAT, STOP_DET))
		pdata->i2c_complete = true;

	return;

reissue_check:
	/* Reissue interrupt if status is not clear */
	if (pdata->vdata->irq_reissue_support)
		XP_IOWRITE(pdata, XP_INT_REISSUE_EN, 1 << 2);
}

static void
xgbe_i2c_set_mode(struct xgbe_prv_data *pdata)
{
	unsigned int reg;

	reg = XI2C_IOREAD(pdata, IC_CON);
	XI2C_SET_BITS(reg, IC_CON, MASTER_MODE, 1);
	XI2C_SET_BITS(reg, IC_CON, SLAVE_DISABLE, 1);
	XI2C_SET_BITS(reg, IC_CON, RESTART_EN, 1);
	XI2C_SET_BITS(reg, IC_CON, SPEED, XGBE_STD_SPEED);
	XI2C_SET_BITS(reg, IC_CON, RX_FIFO_FULL_HOLD, 1);
	XI2C_IOWRITE(pdata, IC_CON, reg);
}

static void
xgbe_i2c_get_features(struct xgbe_prv_data *pdata)
{
	struct xgbe_i2c *i2c = &pdata->i2c;
	unsigned int reg;

	reg = XI2C_IOREAD(pdata, IC_COMP_PARAM_1);
	i2c->max_speed_mode = XI2C_GET_BITS(reg, IC_COMP_PARAM_1,
					    MAX_SPEED_MODE);
	i2c->rx_fifo_size = XI2C_GET_BITS(reg, IC_COMP_PARAM_1,
					  RX_BUFFER_DEPTH);
	i2c->tx_fifo_size = XI2C_GET_BITS(reg, IC_COMP_PARAM_1,
					  TX_BUFFER_DEPTH);

	axgbe_printf(3, "%s: I2C features: %s=%u, %s=%u, %s=%u\n", __func__,
	    "MAX_SPEED_MODE", i2c->max_speed_mode,
	    "RX_BUFFER_DEPTH", i2c->rx_fifo_size,
	    "TX_BUFFER_DEPTH", i2c->tx_fifo_size);
}

static void
xgbe_i2c_set_target(struct xgbe_prv_data *pdata, unsigned int addr)
{
	XI2C_IOWRITE(pdata, IC_TAR, addr);
}

static void
xgbe_i2c_combined_isr(struct xgbe_prv_data *pdata)
{
	xgbe_i2c_isr(pdata);
}

static int
xgbe_i2c_xfer(struct xgbe_prv_data *pdata, struct xgbe_i2c_op *op)
{
	struct xgbe_i2c_op_state *state = &pdata->i2c.op_state;
	unsigned long timeout;
	int ret;

	mtx_lock(&pdata->i2c_mutex);

	axgbe_printf(3, "i2c xfer started ---->>>\n");

	ret = xgbe_i2c_disable(pdata);
	if (ret) {
		axgbe_error("failed to disable i2c master\n");
		goto out;
	}

	xgbe_i2c_set_target(pdata, op->target);

	memset(state, 0, sizeof(*state));
	state->op = op;
	state->tx_len = op->len;
	state->tx_buf = op->buf;
	state->rx_len = op->len;
	state->rx_buf = op->buf;

	xgbe_i2c_clear_all_interrupts(pdata);
	ret = xgbe_i2c_enable(pdata);
	if (ret) {
		axgbe_error("failed to enable i2c master\n");
		goto out;
	}

	/* Enabling the interrupts will cause the TX FIFO empty interrupt to
	 * fire and begin to process the command via the ISR.
	 */
	xgbe_i2c_enable_interrupts(pdata);

	timeout = ticks + (20 * hz);
	while (ticks < timeout) {

		if (!pdata->i2c_complete) {
			DELAY(200);
			continue;
		}

		axgbe_printf(1, "%s: I2C OP complete\n", __func__);
		break;
	}

	if ((ticks >= timeout) && !pdata->i2c_complete) {
		axgbe_error("%s: operation timed out\n", __func__);
		ret = -ETIMEDOUT;
		goto disable;
	}

	ret = state->ret;
	axgbe_printf(3, "%s: i2c xfer ret %d abrt_source 0x%x\n", __func__,
	    ret, state->tx_abort_source);
	if (ret) {
		axgbe_printf(1, "%s: i2c xfer ret %d abrt_source 0x%x\n", __func__,
		    ret, state->tx_abort_source);
		if (state->tx_abort_source & IC_TX_ABRT_7B_ADDR_NOACK)
			ret = -ENOTCONN;
		else if (state->tx_abort_source & IC_TX_ABRT_ARB_LOST)
			ret = -EAGAIN;
	}

	axgbe_printf(3, "i2c xfer finished ---->>>\n");

disable:
	pdata->i2c_complete = false;
	xgbe_i2c_disable_interrupts(pdata);
	xgbe_i2c_disable(pdata);

out:
	mtx_unlock(&pdata->i2c_mutex);
	return (ret);
}

static void
xgbe_i2c_stop(struct xgbe_prv_data *pdata)
{
	if (!pdata->i2c.started)
		return;

	axgbe_printf(3, "stopping I2C\n");

	pdata->i2c.started = 0;

	xgbe_i2c_disable_interrupts(pdata);
	xgbe_i2c_disable(pdata);
	xgbe_i2c_clear_all_interrupts(pdata);
}

static int
xgbe_i2c_start(struct xgbe_prv_data *pdata)
{
	if (pdata->i2c.started)
		return (0);

	pdata->i2c.started = 1;

	return (0);
}

static int
xgbe_i2c_init(struct xgbe_prv_data *pdata)
{
	int ret;

	/* initialize lock for i2c */
	mtx_init(&pdata->i2c_mutex, "xgbe i2c mutex lock", NULL, MTX_DEF);
	pdata->i2c_complete = false;

	xgbe_i2c_disable_interrupts(pdata);

	ret = xgbe_i2c_disable(pdata);
	if (ret) {
		axgbe_error("failed to disable i2c master\n");
		return (ret);
	}

	xgbe_i2c_get_features(pdata);

	xgbe_i2c_set_mode(pdata);

	xgbe_i2c_clear_all_interrupts(pdata);

	xgbe_dump_i2c_registers(pdata);

	return (0);
}

void
xgbe_init_function_ptrs_i2c(struct xgbe_i2c_if *i2c_if)
{
	i2c_if->i2c_init		= xgbe_i2c_init;

	i2c_if->i2c_start		= xgbe_i2c_start;
	i2c_if->i2c_stop		= xgbe_i2c_stop;

	i2c_if->i2c_xfer		= xgbe_i2c_xfer;

	i2c_if->i2c_isr			= xgbe_i2c_combined_isr;
}
