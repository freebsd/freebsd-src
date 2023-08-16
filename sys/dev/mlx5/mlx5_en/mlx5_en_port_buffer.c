/*-
 * Copyright (c) 2018-2019 Mellanox Technologies. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_rss.h"
#include "opt_ratelimit.h"

#include <dev/mlx5/mlx5_en/port_buffer.h>

#define MLX5E_MAX_PORT_MTU  9216

int mlx5e_port_query_buffer(struct mlx5e_priv *priv,
			    struct mlx5e_port_buffer *port_buffer)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	int sz = MLX5_ST_SZ_BYTES(pbmc_reg);
	u32 total_used = 0;
	void *buffer;
	void *out;
	int err;
	int i;

	out = kzalloc(sz, GFP_KERNEL);
	if (!out)
		return -ENOMEM;

	err = mlx5e_port_query_pbmc(mdev, out);
	if (err)
		goto out;

	for (i = 0; i < MLX5E_MAX_BUFFER; i++) {
		buffer = MLX5_ADDR_OF(pbmc_reg, out, buffer[i]);
		port_buffer->buffer[i].lossy =
			MLX5_GET(bufferx_reg, buffer, lossy);
		port_buffer->buffer[i].epsb =
			MLX5_GET(bufferx_reg, buffer, epsb);
		port_buffer->buffer[i].size =
			MLX5_GET(bufferx_reg, buffer, size) << MLX5E_BUFFER_CELL_SHIFT;
		port_buffer->buffer[i].xon =
			MLX5_GET(bufferx_reg, buffer, xon_threshold) << MLX5E_BUFFER_CELL_SHIFT;
		port_buffer->buffer[i].xoff =
			MLX5_GET(bufferx_reg, buffer, xoff_threshold) << MLX5E_BUFFER_CELL_SHIFT;
		total_used += port_buffer->buffer[i].size;

		mlx5e_dbg(HW, priv, "buffer %d: size=%d, xon=%d, xoff=%d, epsb=%d, lossy=%d\n", i,
			  port_buffer->buffer[i].size,
			  port_buffer->buffer[i].xon,
			  port_buffer->buffer[i].xoff,
			  port_buffer->buffer[i].epsb,
			  port_buffer->buffer[i].lossy);
	}

	port_buffer->port_buffer_size =
		MLX5_GET(pbmc_reg, out, port_buffer_size) << MLX5E_BUFFER_CELL_SHIFT;
	port_buffer->spare_buffer_size =
		port_buffer->port_buffer_size - total_used;

	mlx5e_dbg(HW, priv, "total buffer size=%d, spare buffer size=%d\n",
		  port_buffer->port_buffer_size,
		  port_buffer->spare_buffer_size);
out:
	kfree(out);
	return err;
}

static int port_set_buffer(struct mlx5e_priv *priv,
			   struct mlx5e_port_buffer *port_buffer)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	int sz = MLX5_ST_SZ_BYTES(pbmc_reg);
	void *buffer;
	void *in;
	int err;
	int i;

	in = kzalloc(sz, GFP_KERNEL);
	if (!in)
		return -ENOMEM;

	err = mlx5e_port_query_pbmc(mdev, in);
	if (err)
		goto out;

	for (i = 0; i < MLX5E_MAX_BUFFER; i++) {
		buffer = MLX5_ADDR_OF(pbmc_reg, in, buffer[i]);

		MLX5_SET(bufferx_reg, buffer, size,
			 port_buffer->buffer[i].size >> MLX5E_BUFFER_CELL_SHIFT);
		MLX5_SET(bufferx_reg, buffer, lossy,
			 port_buffer->buffer[i].lossy);
		MLX5_SET(bufferx_reg, buffer, xoff_threshold,
			 port_buffer->buffer[i].xoff >> MLX5E_BUFFER_CELL_SHIFT);
		MLX5_SET(bufferx_reg, buffer, xon_threshold,
			 port_buffer->buffer[i].xon >> MLX5E_BUFFER_CELL_SHIFT);
	}

	err = mlx5e_port_set_pbmc(mdev, in);
out:
	kfree(in);
	return err;
}

/* xoff = ((301+2.16 * len [m]) * speed [Gbps] + 2.72 MTU [B]) */
static u32 calculate_xoff(struct mlx5e_priv *priv, unsigned int mtu)
{
	u32 speed;
	u32 xoff;
	int err;

	err = mlx5e_port_linkspeed(priv->mdev, &speed);
	if (err) {
		mlx5_core_warn(priv->mdev, "cannot get port speed\n");
		speed = SPEED_40000;
	}
	speed = max_t(u32, speed, SPEED_40000);
	xoff = (301 + 216 * priv->dcbx.cable_len / 100) * speed / 1000 + 272 * mtu / 100;

	mlx5e_dbg(HW, priv, "%s: xoff=%d\n", __func__, xoff);
	return xoff;
}

static int update_xoff_threshold(struct mlx5e_priv *priv,
    struct mlx5e_port_buffer *port_buffer, u32 xoff)
{
	int i;

	for (i = 0; i < MLX5E_MAX_BUFFER; i++) {
		if (port_buffer->buffer[i].lossy) {
			port_buffer->buffer[i].xoff = 0;
			port_buffer->buffer[i].xon  = 0;
			continue;
		}

		if (port_buffer->buffer[i].size <
		    (xoff + MLX5E_MAX_PORT_MTU + (1 << MLX5E_BUFFER_CELL_SHIFT))) {
			mlx5_en_info(priv->ifp,
	"non-lossy buffer %d size %d less than xoff threshold %d\n",
			    i, port_buffer->buffer[i].size,
			    xoff + MLX5E_MAX_PORT_MTU +
			    (1 << MLX5E_BUFFER_CELL_SHIFT));
			return -ENOMEM;
		}

		port_buffer->buffer[i].xoff = port_buffer->buffer[i].size - xoff;
		port_buffer->buffer[i].xon  = 
			port_buffer->buffer[i].xoff - MLX5E_MAX_PORT_MTU;
	}

	return 0;
}

/**
 * update_buffer_lossy()
 *   mtu: device's MTU
 *   pfc_en: <input> current pfc configuration
 *   buffer: <input> current prio to buffer mapping
 *   xoff:   <input> xoff value
 *   port_buffer: <output> port receive buffer configuration
 *   change: <output>
 *
 *   Update buffer configuration based on pfc configuration and priority
 *   to buffer mapping.
 *   Buffer's lossy bit is changed to:
 *     lossless if there is at least one PFC enabled priority mapped to this buffer
 *     lossy if all priorities mapped to this buffer are PFC disabled
 *
 *   Return:
 *     Return 0 if no error.
 *     Set change to true if buffer configuration is modified.
 */
static int update_buffer_lossy(struct mlx5e_priv *priv, unsigned int mtu,
			       u8 pfc_en, u8 *buffer, u32 xoff,
			       struct mlx5e_port_buffer *port_buffer,
			       bool *change)
{
	bool changed = false;
	u8 lossy_count;
	u8 prio_count;
	u8 lossy;
	int prio;
	int err;
	int i;

	for (i = 0; i < MLX5E_MAX_BUFFER; i++) {
		prio_count = 0;
		lossy_count = 0;

		for (prio = 0; prio < MLX5E_MAX_PRIORITY; prio++) {
			if (buffer[prio] != i)
				continue;

			prio_count++;
			lossy_count += !(pfc_en & (1 << prio));
		}

		if (lossy_count == prio_count)
			lossy = 1;
		else /* lossy_count < prio_count */
			lossy = 0;

		if (lossy != port_buffer->buffer[i].lossy) {
			port_buffer->buffer[i].lossy = lossy;
			changed = true;
		}
	}

	if (changed) {
		err = update_xoff_threshold(priv, port_buffer, xoff);
		if (err)
			return err;

		*change = true;
	}

	return 0;
}

int mlx5e_port_manual_buffer_config(struct mlx5e_priv *priv,
				    u32 change, unsigned int mtu,
				    struct ieee_pfc *pfc,
				    u32 *buffer_size,
				    u8 *prio2buffer)
{
	struct mlx5e_port_buffer port_buffer;
	u32 xoff = calculate_xoff(priv, mtu);
	bool update_prio2buffer = false;
	u8 buffer[MLX5E_MAX_PRIORITY];
	bool update_buffer = false;
	u32 total_used = 0;
	u8 curr_pfc_en;
	int err;
	int i;

	mlx5e_dbg(HW, priv, "%s: change=%x\n", __func__, change);

	err = mlx5e_port_query_buffer(priv, &port_buffer);
	if (err)
		return err;

	if (change & MLX5E_PORT_BUFFER_CABLE_LEN) {
		update_buffer = true;
		err = update_xoff_threshold(priv, &port_buffer, xoff);
		if (err)
			return err;
	}

	if (change & MLX5E_PORT_BUFFER_PFC) {
		err = mlx5e_port_query_priority2buffer(priv->mdev, buffer);
		if (err)
			return err;

		priv->sw_is_port_buf_owner = true;
		err = update_buffer_lossy(priv, mtu, pfc->pfc_en, buffer, xoff,
					  &port_buffer, &update_buffer);
		if (err)
			return err;
	}

	if (change & MLX5E_PORT_BUFFER_PRIO2BUFFER) {
		update_prio2buffer = true;
		err = mlx5_query_port_pfc(priv->mdev, &curr_pfc_en, NULL);
		if (err)
			return err;

		err = update_buffer_lossy(priv, mtu, curr_pfc_en, prio2buffer, xoff,
					  &port_buffer, &update_buffer);
		if (err)
			return err;
	}

	if (change & MLX5E_PORT_BUFFER_SIZE) {
		for (i = 0; i < MLX5E_MAX_BUFFER; i++) {
			mlx5e_dbg(HW, priv, "%s: buffer[%d]=%d\n", __func__, i, buffer_size[i]);
			if (!port_buffer.buffer[i].lossy && !buffer_size[i]) {
				mlx5e_dbg(HW, priv, "%s: lossless buffer[%d] size cannot be zero\n",
					  __func__, i);
				return -EINVAL;
			}

			port_buffer.buffer[i].size = buffer_size[i];
			total_used += buffer_size[i];
		}

		mlx5e_dbg(HW, priv, "%s: total buffer requested=%d\n", __func__, total_used);

		if (total_used > port_buffer.port_buffer_size)
			return -EINVAL;

		update_buffer = true;
		err = update_xoff_threshold(priv, &port_buffer, xoff);
		if (err)
			return err;
	}

	/* Need to update buffer configuration if xoff value is changed */
	if (!update_buffer && xoff != priv->dcbx.xoff) {
		update_buffer = true;
		err = update_xoff_threshold(priv, &port_buffer, xoff);
		if (err)
			return err;
	}
	priv->dcbx.xoff = xoff;

	/* Apply the settings */
	if (update_buffer) {
		priv->sw_is_port_buf_owner = true;
		err = port_set_buffer(priv, &port_buffer);
		if (err)
			return err;
	}

	if (update_prio2buffer) {
		priv->sw_is_port_buf_owner = true;
		err = mlx5e_port_set_priority2buffer(priv->mdev, prio2buffer);
	}

	return err;
}
