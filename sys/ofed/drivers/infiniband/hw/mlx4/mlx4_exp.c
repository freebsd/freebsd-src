/*
 * Copyright (c) 2006, 2007 Cisco Systems, Inc. All rights reserved.
 * Copyright (c) 2007, 2008 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "mlx4_ib.h"
#include "mlx4_exp.h"
#include <linux/mlx4/qp.h>

int mlx4_ib_exp_query_device(struct ib_device *ibdev,
			     struct ib_exp_device_attr *props)
{
	struct ib_device_attr *base = &props->base;
	struct mlx4_ib_dev *dev = to_mdev(ibdev);
	int ret = mlx4_ib_query_device(ibdev, &props->base);

	props->exp_comp_mask = IB_EXP_DEVICE_ATTR_INLINE_RECV_SZ;
	props->inline_recv_sz = dev->dev->caps.max_rq_sg * sizeof(struct mlx4_wqe_data_seg);
	props->device_cap_flags2 = 0;

	/* move RSS device cap from device_cap to device_cap_flags2 */
	if (base->device_cap_flags & IB_DEVICE_QPG) {
		props->device_cap_flags2 |= IB_EXP_DEVICE_QPG;
		if (base->device_cap_flags & IB_DEVICE_UD_RSS)
			props->device_cap_flags2 |= IB_EXP_DEVICE_UD_RSS;
	}
	base->device_cap_flags &= ~(IB_DEVICE_QPG |
				    IB_DEVICE_UD_RSS |
				    IB_DEVICE_UD_TSS);

	if (base->max_rss_tbl_sz > 0) {
		props->max_rss_tbl_sz = base->max_rss_tbl_sz;
		props->exp_comp_mask |= IB_EXP_DEVICE_ATTR_RSS_TBL_SZ;
	} else {
		props->max_rss_tbl_sz = 0;
		props->exp_comp_mask &= ~IB_EXP_DEVICE_ATTR_RSS_TBL_SZ;
	}

	if (props->device_cap_flags2)
		props->exp_comp_mask |= IB_EXP_DEVICE_ATTR_CAP_FLAGS2;

	return ret;
}

/*
 * Experimental functions
 */
struct ib_qp *mlx4_ib_exp_create_qp(struct ib_pd *pd,
				    struct ib_exp_qp_init_attr *init_attr,
				    struct ib_udata *udata)
{
	int rwqe_size;
	struct ib_qp *qp;
	struct mlx4_ib_qp *mqp;
	int use_inlr;
	struct mlx4_ib_dev *dev;

	if (init_attr->max_inl_recv && !udata)
		return ERR_PTR(-EINVAL);

	use_inlr = mlx4_ib_qp_has_rq((struct ib_qp_init_attr *)init_attr) &&
		   init_attr->max_inl_recv && pd;
	if (use_inlr) {
		rwqe_size = roundup_pow_of_two(max(1U, init_attr->cap.max_recv_sge)) *
					       sizeof(struct mlx4_wqe_data_seg);
		if (rwqe_size < init_attr->max_inl_recv) {
			dev = to_mdev(pd->device);
			init_attr->max_inl_recv = min(init_attr->max_inl_recv,
						      (u32)(dev->dev->caps.max_rq_sg *
						      sizeof(struct mlx4_wqe_data_seg)));
			init_attr->cap.max_recv_sge = roundup_pow_of_two(init_attr->max_inl_recv) /
						      sizeof(struct mlx4_wqe_data_seg);
		}
	} else {
		init_attr->max_inl_recv = 0;
	}
	qp = mlx4_ib_create_qp(pd, (struct ib_qp_init_attr *)init_attr, udata);
	if (IS_ERR(qp))
		return qp;

	if (use_inlr) {
		mqp = to_mqp(qp);
		mqp->max_inlr_data = 1 << mqp->rq.wqe_shift;
		init_attr->max_inl_recv = mqp->max_inlr_data;
	}

	return qp;
}
