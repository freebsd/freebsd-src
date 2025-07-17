/*-
 * Copyright (c) 2023 NVIDIA corporation & affiliates.
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
 *
 */

#include "opt_ipsec.h"

#include <sys/mbuf.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netipsec/keydb.h>
#include <netipsec/ipsec_offload.h>
#include <netipsec/xform.h>
#include <dev/mlx5/qp.h>
#include <dev/mlx5/mlx5_en/en.h>
#include <dev/mlx5/mlx5_accel/ipsec.h>

#define MLX5_IPSEC_METADATA_HANDLE(ipsec_metadata) (ipsec_metadata & 0xFFFFFF)

int
mlx5_accel_ipsec_rx_tag_add(if_t ifp, struct mlx5e_rq_mbuf *mr)
{
	struct mlx5e_priv *priv;
	struct ipsec_accel_in_tag *mtag;

	priv = if_getsoftc(ifp);
	if (priv->ipsec == NULL)
		return (0);
	if (mr->ipsec_mtag != NULL)
		return (0);

	mtag = (struct ipsec_accel_in_tag *)m_tag_get(
	    PACKET_TAG_IPSEC_ACCEL_IN, sizeof(struct ipsec_accel_in_tag) -
	    __offsetof(struct ipsec_accel_in_tag, xh), M_NOWAIT);
	if (mtag == NULL)
		return (-ENOMEM);
	mr->ipsec_mtag = mtag;
	return (0);
}

void
mlx5e_accel_ipsec_handle_rx_cqe(if_t ifp, struct mbuf *mb,
    struct mlx5_cqe64 *cqe, struct mlx5e_rq_mbuf *mr)
{
	struct ipsec_accel_in_tag *mtag;
	u32 drv_spi;

	drv_spi = MLX5_IPSEC_METADATA_HANDLE(be32_to_cpu(cqe->ft_metadata));
	mtag = mr->ipsec_mtag;
	WARN_ON(mtag == NULL);
	if (mtag != NULL) {
		mtag->drv_spi = drv_spi;
		if (ipsec_accel_fill_xh(ifp, drv_spi, &mtag->xh)) {
			m_tag_prepend(mb, &mtag->tag);
			mr->ipsec_mtag = NULL;
		}
	}
}

void
mlx5e_accel_ipsec_handle_tx_wqe(struct mbuf *mb, struct mlx5e_tx_wqe *wqe,
    struct ipsec_accel_out_tag *tag)
{
	wqe->eth.flow_table_metadata = cpu_to_be32(
	    mlx5e_accel_ipsec_get_metadata(tag->drv_spi));
}
