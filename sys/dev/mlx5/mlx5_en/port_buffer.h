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

#ifndef __MLX5_EN_PORT_BUFFER_H__
#define __MLX5_EN_PORT_BUFFER_H__

#include <dev/mlx5/mlx5_en/en.h>
#include <dev/mlx5/port.h>

#define MLX5E_MAX_BUFFER 8
#define MLX5E_BUFFER_CELL_SHIFT 7
#define MLX5E_DEFAULT_CABLE_LEN 7 /* 7 meters */

#define MLX5_BUFFER_SUPPORTED(mdev) (MLX5_CAP_GEN(mdev, pcam_reg) && \
				     MLX5_CAP_PCAM_REG(mdev, pbmc) && \
				     MLX5_CAP_PCAM_REG(mdev, pptb))

enum {
	MLX5E_PORT_BUFFER_CABLE_LEN   = BIT(0),
	MLX5E_PORT_BUFFER_PFC         = BIT(1),
	MLX5E_PORT_BUFFER_PRIO2BUFFER = BIT(2),
	MLX5E_PORT_BUFFER_SIZE        = BIT(3),
};

struct mlx5e_bufferx_reg {
	u8   lossy;
	u8   epsb;
	u32  size;
	u32  xoff;
	u32  xon;
};

struct mlx5e_port_buffer {
	u32                       port_buffer_size;
	u32                       spare_buffer_size;
	struct mlx5e_bufferx_reg  buffer[MLX5E_MAX_BUFFER];
};

#define	IEEE_8021QAZ_MAX_TCS	8

struct ieee_pfc {
	__u8	pfc_cap;
	__u8	pfc_en;
	__u8	mbc;
	__u16	delay;
	__u64	requests[IEEE_8021QAZ_MAX_TCS];
	__u64	indications[IEEE_8021QAZ_MAX_TCS];
};

int mlx5e_port_manual_buffer_config(struct mlx5e_priv *priv,
				    u32 change, unsigned int mtu,
				    struct ieee_pfc *pfc,
				    u32 *buffer_size,
				    u8 *prio2buffer);

int mlx5e_port_query_buffer(struct mlx5e_priv *priv,
			    struct mlx5e_port_buffer *port_buffer);
#endif
