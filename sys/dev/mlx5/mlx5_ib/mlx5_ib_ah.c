/*-
 * Copyright (c) 2013-2020, Mellanox Technologies, Ltd.  All rights reserved.
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

#include <dev/mlx5/mlx5_ib/mlx5_ib.h>

static void create_ib_ah(struct mlx5_ib_dev *dev,
				  struct mlx5_ib_ah *ah,
				  struct rdma_ah_attr *ah_attr)
{
	if (rdma_ah_get_ah_flags(ah_attr) & IB_AH_GRH) {
		const struct ib_global_route *grh = rdma_ah_read_grh(ah_attr);

		memcpy(ah->av.rgid, &grh->dgid, 16);
		ah->av.grh_gid_fl = cpu_to_be32(grh->flow_label |
						(1 << 30) |
						grh->sgid_index << 20);
		ah->av.hop_limit = grh->hop_limit;
		ah->av.tclass = grh->traffic_class;
	}

	ah->av.stat_rate_sl = (rdma_ah_get_static_rate(ah_attr) << 4);

	if (ah_attr->type == RDMA_AH_ATTR_TYPE_ROCE) {
		memcpy(ah->av.rmac, ah_attr->roce.dmac,
		       sizeof(ah_attr->roce.dmac));
		ah->av.udp_sport =
			mlx5_get_roce_udp_sport(dev, ah_attr->grh.sgid_attr);
		ah->av.stat_rate_sl |= (rdma_ah_get_sl(ah_attr) & 0x7) << 1;
	} else {
		ah->av.rlid = cpu_to_be16(rdma_ah_get_dlid(ah_attr));
		ah->av.fl_mlid = rdma_ah_get_path_bits(ah_attr) & 0x7f;
		ah->av.stat_rate_sl |= (rdma_ah_get_sl(ah_attr) & 0xf);
	}
}

int mlx5_ib_create_ah(struct ib_ah *ibah, struct rdma_ah_attr *ah_attr,
		      u32 flags, struct ib_udata *udata)

{
	struct mlx5_ib_ah *ah = to_mah(ibah);
	struct mlx5_ib_dev *dev = to_mdev(ibah->device);
	enum rdma_ah_attr_type ah_type = ah_attr->type;

	if ((ah_type == RDMA_AH_ATTR_TYPE_ROCE) &&
	    !(rdma_ah_get_ah_flags(ah_attr) & IB_AH_GRH))
		return -EINVAL;

	if (ah_type == RDMA_AH_ATTR_TYPE_ROCE && udata) {
		int err;
		struct mlx5_ib_create_ah_resp resp = {};
		u32 min_resp_len = offsetof(typeof(resp), dmac) +
				   sizeof(resp.dmac);

		if (udata->outlen < min_resp_len)
			return -EINVAL;

		resp.response_length = min_resp_len;

		memcpy(resp.dmac, ah_attr->roce.dmac, ETH_ALEN);
		err = ib_copy_to_udata(udata, &resp, resp.response_length);
		if (err)
			return err;
	}

	create_ib_ah(dev, ah, ah_attr);
	return 0;
}

int mlx5_ib_query_ah(struct ib_ah *ibah, struct rdma_ah_attr *ah_attr)
{
	struct mlx5_ib_ah *ah = to_mah(ibah);
	u32 tmp;

	memset(ah_attr, 0, sizeof(*ah_attr));
	ah_attr->type = ibah->type;

	tmp = be32_to_cpu(ah->av.grh_gid_fl);
	if (tmp & (1 << 30)) {
		rdma_ah_set_grh(ah_attr, NULL,
				tmp & 0xfffff,
				(tmp >> 20) & 0xff,
				ah->av.hop_limit,
				ah->av.tclass);
		rdma_ah_set_dgid_raw(ah_attr, ah->av.rgid);
	}
	rdma_ah_set_dlid(ah_attr, be16_to_cpu(ah->av.rlid));
	rdma_ah_set_static_rate(ah_attr, ah->av.stat_rate_sl >> 4);
	rdma_ah_set_sl(ah_attr, ah->av.stat_rate_sl & 0xf);

	return 0;
}

void mlx5_ib_destroy_ah(struct ib_ah *ah, u32 flags)
{
	return;
}
