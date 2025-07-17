/*-
 * Copyright (c) 2013-2021, Mellanox Technologies, Ltd.  All rights reserved.
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

#include <linux/interrupt.h>
#include <linux/module.h>
#include <dev/mlx5/port.h>
#include <dev/mlx5/mlx5_ifc.h>
#include <dev/mlx5/mlx5_fpga/core.h>
#include <dev/mlx5/mlx5_core/mlx5_core.h>
#include <dev/mlx5/mlx5_core/eswitch.h>
#include <dev/mlx5/mlx5_accel/ipsec.h>

#ifdef  RSS
#include <net/rss_config.h>
#include <netinet/in_rss.h>
#endif

enum {
	MLX5_EQE_SIZE		= sizeof(struct mlx5_eqe),
	MLX5_EQE_OWNER_INIT_VAL	= 0x1,
};

enum {
	MLX5_NUM_SPARE_EQE	= 0x80,
	MLX5_NUM_ASYNC_EQE	= 0x100,
	MLX5_NUM_CMD_EQE	= 32,
};

enum {
	MLX5_EQ_DOORBEL_OFFSET	= 0x40,
};

#define MLX5_ASYNC_EVENT_MASK ((1ull << MLX5_EVENT_TYPE_PATH_MIG)	    | \
			       (1ull << MLX5_EVENT_TYPE_COMM_EST)	    | \
			       (1ull << MLX5_EVENT_TYPE_SQ_DRAINED)	    | \
			       (1ull << MLX5_EVENT_TYPE_CQ_ERROR)	    | \
			       (1ull << MLX5_EVENT_TYPE_WQ_CATAS_ERROR)	    | \
			       (1ull << MLX5_EVENT_TYPE_PATH_MIG_FAILED)    | \
			       (1ull << MLX5_EVENT_TYPE_WQ_INVAL_REQ_ERROR) | \
			       (1ull << MLX5_EVENT_TYPE_WQ_ACCESS_ERROR)    | \
			       (1ull << MLX5_EVENT_TYPE_PORT_CHANGE)	    | \
			       (1ull << MLX5_EVENT_TYPE_SRQ_CATAS_ERROR)    | \
			       (1ull << MLX5_EVENT_TYPE_SRQ_LAST_WQE)	    | \
			       (1ull << MLX5_EVENT_TYPE_SRQ_RQ_LIMIT)	    | \
			       (1ull << MLX5_EVENT_TYPE_NIC_VPORT_CHANGE))

struct map_eq_in {
	u64	mask;
	u32	reserved;
	u32	unmap_eqn;
};

struct cre_des_eq {
	u8	reserved[15];
	u8	eqn;
};

/*Function prototype*/
static void mlx5_port_module_event(struct mlx5_core_dev *dev,
				   struct mlx5_eqe *eqe);
static void mlx5_port_general_notification_event(struct mlx5_core_dev *dev,
						 struct mlx5_eqe *eqe);

static int mlx5_cmd_destroy_eq(struct mlx5_core_dev *dev, u8 eqn)
{
	u32 in[MLX5_ST_SZ_DW(destroy_eq_in)] = {0};
	u32 out[MLX5_ST_SZ_DW(destroy_eq_out)] = {0};

	MLX5_SET(destroy_eq_in, in, opcode, MLX5_CMD_OP_DESTROY_EQ);
	MLX5_SET(destroy_eq_in, in, eq_number, eqn);

	return mlx5_cmd_exec(dev, in, sizeof(in), out, sizeof(out));
}

static struct mlx5_eqe *get_eqe(struct mlx5_eq *eq, u32 entry)
{
	return mlx5_buf_offset(&eq->buf, entry * MLX5_EQE_SIZE);
}

static struct mlx5_eqe *next_eqe_sw(struct mlx5_eq *eq)
{
	struct mlx5_eqe *eqe = get_eqe(eq, eq->cons_index & (eq->nent - 1));

	return ((eqe->owner & 1) ^ !!(eq->cons_index & eq->nent)) ? NULL : eqe;
}

static const char *eqe_type_str(u8 type)
{
	switch (type) {
	case MLX5_EVENT_TYPE_COMP:
		return "MLX5_EVENT_TYPE_COMP";
	case MLX5_EVENT_TYPE_PATH_MIG:
		return "MLX5_EVENT_TYPE_PATH_MIG";
	case MLX5_EVENT_TYPE_COMM_EST:
		return "MLX5_EVENT_TYPE_COMM_EST";
	case MLX5_EVENT_TYPE_SQ_DRAINED:
		return "MLX5_EVENT_TYPE_SQ_DRAINED";
	case MLX5_EVENT_TYPE_SRQ_LAST_WQE:
		return "MLX5_EVENT_TYPE_SRQ_LAST_WQE";
	case MLX5_EVENT_TYPE_SRQ_RQ_LIMIT:
		return "MLX5_EVENT_TYPE_SRQ_RQ_LIMIT";
	case MLX5_EVENT_TYPE_CQ_ERROR:
		return "MLX5_EVENT_TYPE_CQ_ERROR";
	case MLX5_EVENT_TYPE_WQ_CATAS_ERROR:
		return "MLX5_EVENT_TYPE_WQ_CATAS_ERROR";
	case MLX5_EVENT_TYPE_PATH_MIG_FAILED:
		return "MLX5_EVENT_TYPE_PATH_MIG_FAILED";
	case MLX5_EVENT_TYPE_WQ_INVAL_REQ_ERROR:
		return "MLX5_EVENT_TYPE_WQ_INVAL_REQ_ERROR";
	case MLX5_EVENT_TYPE_WQ_ACCESS_ERROR:
		return "MLX5_EVENT_TYPE_WQ_ACCESS_ERROR";
	case MLX5_EVENT_TYPE_SRQ_CATAS_ERROR:
		return "MLX5_EVENT_TYPE_SRQ_CATAS_ERROR";
	case MLX5_EVENT_TYPE_INTERNAL_ERROR:
		return "MLX5_EVENT_TYPE_INTERNAL_ERROR";
	case MLX5_EVENT_TYPE_PORT_CHANGE:
		return "MLX5_EVENT_TYPE_PORT_CHANGE";
	case MLX5_EVENT_TYPE_GPIO_EVENT:
		return "MLX5_EVENT_TYPE_GPIO_EVENT";
	case MLX5_EVENT_TYPE_CODING_PORT_MODULE_EVENT:
		return "MLX5_EVENT_TYPE_PORT_MODULE_EVENT";
	case MLX5_EVENT_TYPE_TEMP_WARN_EVENT:
		return "MLX5_EVENT_TYPE_TEMP_WARN_EVENT";
	case MLX5_EVENT_TYPE_REMOTE_CONFIG:
		return "MLX5_EVENT_TYPE_REMOTE_CONFIG";
	case MLX5_EVENT_TYPE_DB_BF_CONGESTION:
		return "MLX5_EVENT_TYPE_DB_BF_CONGESTION";
	case MLX5_EVENT_TYPE_STALL_EVENT:
		return "MLX5_EVENT_TYPE_STALL_EVENT";
	case MLX5_EVENT_TYPE_CMD:
		return "MLX5_EVENT_TYPE_CMD";
	case MLX5_EVENT_TYPE_PAGE_REQUEST:
		return "MLX5_EVENT_TYPE_PAGE_REQUEST";
	case MLX5_EVENT_TYPE_NIC_VPORT_CHANGE:
		return "MLX5_EVENT_TYPE_NIC_VPORT_CHANGE";
	case MLX5_EVENT_TYPE_FPGA_ERROR:
		return "MLX5_EVENT_TYPE_FPGA_ERROR";
	case MLX5_EVENT_TYPE_FPGA_QP_ERROR:
		return "MLX5_EVENT_TYPE_FPGA_QP_ERROR";
	case MLX5_EVENT_TYPE_CODING_DCBX_CHANGE_EVENT:
		return "MLX5_EVENT_TYPE_CODING_DCBX_CHANGE_EVENT";
	case MLX5_EVENT_TYPE_CODING_GENERAL_NOTIFICATION_EVENT:
		return "MLX5_EVENT_TYPE_CODING_GENERAL_NOTIFICATION_EVENT";
	case MLX5_EVENT_TYPE_OBJECT_CHANGE:
		return "MLX5_EVENT_TYPE_OBJECT_CHANGE";
	default:
		return "Unrecognized event";
	}
}

static enum mlx5_dev_event port_subtype_event(u8 subtype)
{
	switch (subtype) {
	case MLX5_PORT_CHANGE_SUBTYPE_DOWN:
		return MLX5_DEV_EVENT_PORT_DOWN;
	case MLX5_PORT_CHANGE_SUBTYPE_ACTIVE:
		return MLX5_DEV_EVENT_PORT_UP;
	case MLX5_PORT_CHANGE_SUBTYPE_INITIALIZED:
		return MLX5_DEV_EVENT_PORT_INITIALIZED;
	case MLX5_PORT_CHANGE_SUBTYPE_LID:
		return MLX5_DEV_EVENT_LID_CHANGE;
	case MLX5_PORT_CHANGE_SUBTYPE_PKEY:
		return MLX5_DEV_EVENT_PKEY_CHANGE;
	case MLX5_PORT_CHANGE_SUBTYPE_GUID:
		return MLX5_DEV_EVENT_GUID_CHANGE;
	case MLX5_PORT_CHANGE_SUBTYPE_CLIENT_REREG:
		return MLX5_DEV_EVENT_CLIENT_REREG;
	}
	return -1;
}

static enum mlx5_dev_event dcbx_subevent(u8 subtype)
{
	switch (subtype) {
	case MLX5_DCBX_EVENT_SUBTYPE_ERROR_STATE_DCBX:
		return MLX5_DEV_EVENT_ERROR_STATE_DCBX;
	case MLX5_DCBX_EVENT_SUBTYPE_REMOTE_CONFIG_CHANGE:
		return MLX5_DEV_EVENT_REMOTE_CONFIG_CHANGE;
	case MLX5_DCBX_EVENT_SUBTYPE_LOCAL_OPER_CHANGE:
		return MLX5_DEV_EVENT_LOCAL_OPER_CHANGE;
	case MLX5_DCBX_EVENT_SUBTYPE_REMOTE_CONFIG_APP_PRIORITY_CHANGE:
		return MLX5_DEV_EVENT_REMOTE_CONFIG_APPLICATION_PRIORITY_CHANGE;
	}
	return -1;
}

static void eq_update_ci(struct mlx5_eq *eq, int arm)
{
	__be32 __iomem *addr = eq->doorbell + (arm ? 0 : 2);
	u32 val = (eq->cons_index & 0xffffff) | (eq->eqn << 24);
	__raw_writel((__force u32) cpu_to_be32(val), addr);
	/* We still want ordering, just not swabbing, so add a barrier */
	mb();
}

static void
mlx5_temp_warning_event(struct mlx5_core_dev *dev, struct mlx5_eqe *eqe)
{

	mlx5_core_warn(dev,
	    "High temperature on sensors with bit set %#jx %#jx\n",
	    (uintmax_t)be64_to_cpu(eqe->data.temp_warning.sensor_warning_msb),
	    (uintmax_t)be64_to_cpu(eqe->data.temp_warning.sensor_warning_lsb));
}

static int mlx5_eq_int(struct mlx5_core_dev *dev, struct mlx5_eq *eq)
{
	struct mlx5_eqe *eqe;
	int eqes_found = 0;
	int set_ci = 0;
	u32 cqn;
	u32 rsn;
	u8 port;

	while ((eqe = next_eqe_sw(eq))) {
		/*
		 * Make sure we read EQ entry contents after we've
		 * checked the ownership bit.
		 */
		atomic_thread_fence_acq();

		mlx5_core_dbg(eq->dev, "eqn %d, eqe type %s\n",
			      eq->eqn, eqe_type_str(eqe->type));

		if (dev->priv.eq_table.cb != NULL &&
		    dev->priv.eq_table.cb(dev, eqe->type, &eqe->data)) {
			/* FALLTHROUGH */
		} else switch (eqe->type) {
		case MLX5_EVENT_TYPE_COMP:
			mlx5_cq_completion(dev, eqe);
			break;

		case MLX5_EVENT_TYPE_PATH_MIG:
		case MLX5_EVENT_TYPE_COMM_EST:
		case MLX5_EVENT_TYPE_SQ_DRAINED:
		case MLX5_EVENT_TYPE_SRQ_LAST_WQE:
		case MLX5_EVENT_TYPE_WQ_CATAS_ERROR:
		case MLX5_EVENT_TYPE_PATH_MIG_FAILED:
		case MLX5_EVENT_TYPE_WQ_INVAL_REQ_ERROR:
		case MLX5_EVENT_TYPE_WQ_ACCESS_ERROR:
			rsn = be32_to_cpu(eqe->data.qp_srq.qp_srq_n) & 0xffffff;
			mlx5_core_dbg(dev, "event %s(%d) arrived on resource 0x%x\n",
				      eqe_type_str(eqe->type), eqe->type, rsn);
			mlx5_rsc_event(dev, rsn, eqe->type);
			break;

		case MLX5_EVENT_TYPE_SRQ_RQ_LIMIT:
		case MLX5_EVENT_TYPE_SRQ_CATAS_ERROR:
			rsn = be32_to_cpu(eqe->data.qp_srq.qp_srq_n) & 0xffffff;
			mlx5_core_dbg(dev, "SRQ event %s(%d): srqn 0x%x\n",
				      eqe_type_str(eqe->type), eqe->type, rsn);
			mlx5_srq_event(dev, rsn, eqe->type);
			break;

		case MLX5_EVENT_TYPE_CMD:
			if (dev->state != MLX5_DEVICE_STATE_INTERNAL_ERROR) {
				mlx5_cmd_comp_handler(dev, be32_to_cpu(eqe->data.cmd.vector),
				    MLX5_CMD_MODE_EVENTS);
			}
			break;

		case MLX5_EVENT_TYPE_PORT_CHANGE:
			port = (eqe->data.port.port >> 4) & 0xf;
			switch (eqe->sub_type) {
			case MLX5_PORT_CHANGE_SUBTYPE_DOWN:
			case MLX5_PORT_CHANGE_SUBTYPE_ACTIVE:
			case MLX5_PORT_CHANGE_SUBTYPE_LID:
			case MLX5_PORT_CHANGE_SUBTYPE_PKEY:
			case MLX5_PORT_CHANGE_SUBTYPE_GUID:
			case MLX5_PORT_CHANGE_SUBTYPE_CLIENT_REREG:
			case MLX5_PORT_CHANGE_SUBTYPE_INITIALIZED:
				if (dev->event)
					dev->event(dev, port_subtype_event(eqe->sub_type),
						   (unsigned long)port);
				break;
			default:
				mlx5_core_warn(dev, "Port event with unrecognized subtype: port %d, sub_type %d\n",
					       port, eqe->sub_type);
			}
			break;

		case MLX5_EVENT_TYPE_CODING_DCBX_CHANGE_EVENT:
			port = (eqe->data.port.port >> 4) & 0xf;
			switch (eqe->sub_type) {
			case MLX5_DCBX_EVENT_SUBTYPE_ERROR_STATE_DCBX:
			case MLX5_DCBX_EVENT_SUBTYPE_REMOTE_CONFIG_CHANGE:
			case MLX5_DCBX_EVENT_SUBTYPE_LOCAL_OPER_CHANGE:
			case MLX5_DCBX_EVENT_SUBTYPE_REMOTE_CONFIG_APP_PRIORITY_CHANGE:
				if (dev->event)
					dev->event(dev,
						   dcbx_subevent(eqe->sub_type),
						   0);
				break;
			default:
				mlx5_core_warn(dev,
					       "dcbx event with unrecognized subtype: port %d, sub_type %d\n",
					       port, eqe->sub_type);
			}
			break;

		case MLX5_EVENT_TYPE_CODING_GENERAL_NOTIFICATION_EVENT:
			mlx5_port_general_notification_event(dev, eqe);
			break;

		case MLX5_EVENT_TYPE_CQ_ERROR:
			cqn = be32_to_cpu(eqe->data.cq_err.cqn) & 0xffffff;
			mlx5_core_warn(dev, "CQ error on CQN 0x%x, syndrom 0x%x\n",
				       cqn, eqe->data.cq_err.syndrome);
			mlx5_cq_event(dev, cqn, eqe->type);
			break;

		case MLX5_EVENT_TYPE_PAGE_REQUEST:
			{
				u16 func_id = be16_to_cpu(eqe->data.req_pages.func_id);
				s32 npages = be32_to_cpu(eqe->data.req_pages.num_pages);

				mlx5_core_dbg(dev, "page request for func 0x%x, npages %d\n",
					      func_id, npages);
				mlx5_core_req_pages_handler(dev, func_id, npages);
			}
			break;

		case MLX5_EVENT_TYPE_CODING_PORT_MODULE_EVENT:
			mlx5_port_module_event(dev, eqe);
			break;

		case MLX5_EVENT_TYPE_NIC_VPORT_CHANGE:
			{
				struct mlx5_eqe_vport_change *vc_eqe =
						&eqe->data.vport_change;
				u16 vport_num = be16_to_cpu(vc_eqe->vport_num);

				if (dev->event)
					dev->event(dev,
					     MLX5_DEV_EVENT_VPORT_CHANGE,
					     (unsigned long)vport_num);
			}
			if (dev->priv.eswitch != NULL)
				mlx5_eswitch_vport_event(dev->priv.eswitch,
				    eqe);
			break;

		case MLX5_EVENT_TYPE_FPGA_ERROR:
		case MLX5_EVENT_TYPE_FPGA_QP_ERROR:
			mlx5_fpga_event(dev, eqe->type, &eqe->data.raw);
			break;
		case MLX5_EVENT_TYPE_TEMP_WARN_EVENT:
			mlx5_temp_warning_event(dev, eqe);
			break;

		case MLX5_EVENT_TYPE_OBJECT_CHANGE:
			mlx5_object_change_event(dev, eqe);
			break;

		default:
			mlx5_core_warn(dev, "Unhandled event 0x%x on EQ 0x%x\n",
				       eqe->type, eq->eqn);
			break;
		}

		++eq->cons_index;
		eqes_found = 1;
		++set_ci;

		/* The HCA will think the queue has overflowed if we
		 * don't tell it we've been processing events.  We
		 * create our EQs with MLX5_NUM_SPARE_EQE extra
		 * entries, so we must update our consumer index at
		 * least that often.
		 */
		if (unlikely(set_ci >= MLX5_NUM_SPARE_EQE)) {
			eq_update_ci(eq, 0);
			set_ci = 0;
		}
	}

	eq_update_ci(eq, 1);

	return eqes_found;
}

static irqreturn_t mlx5_msix_handler(int irq, void *eq_ptr)
{
	struct mlx5_eq *eq = eq_ptr;
	struct mlx5_core_dev *dev = eq->dev;

	/* check if IRQs are not disabled */
	if (likely(dev->priv.disable_irqs == 0))
		mlx5_eq_int(dev, eq);

	/* MSI-X vectors always belong to us */
	return IRQ_HANDLED;
}

static void init_eq_buf(struct mlx5_eq *eq)
{
	struct mlx5_eqe *eqe;
	int i;

	for (i = 0; i < eq->nent; i++) {
		eqe = get_eqe(eq, i);
		eqe->owner = MLX5_EQE_OWNER_INIT_VAL;
	}
}

int mlx5_create_map_eq(struct mlx5_core_dev *dev, struct mlx5_eq *eq, u8 vecidx,
		       int nent, u64 mask)
{
	u32 out[MLX5_ST_SZ_DW(create_eq_out)] = {0};
	struct mlx5_priv *priv = &dev->priv;
	__be64 *pas;
	void *eqc;
	int inlen;
	u32 *in;
	int err;

	eq->nent = roundup_pow_of_two(nent + MLX5_NUM_SPARE_EQE);
	eq->cons_index = 0;
	err = mlx5_buf_alloc(dev, eq->nent * MLX5_EQE_SIZE, 2 * PAGE_SIZE,
			     &eq->buf);
	if (err)
		return err;

	init_eq_buf(eq);

	inlen = MLX5_ST_SZ_BYTES(create_eq_in) +
		MLX5_FLD_SZ_BYTES(create_eq_in, pas[0]) * eq->buf.npages;
	in = mlx5_vzalloc(inlen);
	if (!in) {
		err = -ENOMEM;
		goto err_buf;
	}

	pas = (__be64 *)MLX5_ADDR_OF(create_eq_in, in, pas);
	mlx5_fill_page_array(&eq->buf, pas);

	MLX5_SET(create_eq_in, in, opcode, MLX5_CMD_OP_CREATE_EQ);
	MLX5_SET64(create_eq_in, in, event_bitmask, mask);

	eqc = MLX5_ADDR_OF(create_eq_in, in, eq_context_entry);
	MLX5_SET(eqc, eqc, log_eq_size, ilog2(eq->nent));
	MLX5_SET(eqc, eqc, uar_page, priv->uar->index);
	MLX5_SET(eqc, eqc, intr, vecidx);
	MLX5_SET(eqc, eqc, log_page_size,
		 eq->buf.page_shift - MLX5_ADAPTER_PAGE_SHIFT);

	err = mlx5_cmd_exec(dev, in, inlen, out, sizeof(out));
	if (err)
		goto err_in;

	eq->eqn = MLX5_GET(create_eq_out, out, eq_number);
	eq->irqn = vecidx;
	eq->dev = dev;
	eq->doorbell = priv->uar->map + MLX5_EQ_DOORBEL_OFFSET;
	err = request_irq(priv->msix_arr[vecidx].vector, mlx5_msix_handler, 0,
			  "mlx5_core", eq);
	if (err)
		goto err_eq;
#ifdef RSS
	if (vecidx >= MLX5_EQ_VEC_COMP_BASE) {
		u8 bucket = vecidx - MLX5_EQ_VEC_COMP_BASE;
		err = bind_irq_to_cpu(priv->msix_arr[vecidx].vector,
				      rss_getcpu(bucket % rss_getnumbuckets()));
		if (err)
			goto err_irq;
	}
#else
	if (0)
		goto err_irq;
#endif


	/* EQs are created in ARMED state
	 */
	eq_update_ci(eq, 1);

	kvfree(in);
	return 0;

err_irq:
	free_irq(priv->msix_arr[vecidx].vector, eq);

err_eq:
	mlx5_cmd_destroy_eq(dev, eq->eqn);

err_in:
	kvfree(in);

err_buf:
	mlx5_buf_free(dev, &eq->buf);
	return err;
}
EXPORT_SYMBOL_GPL(mlx5_create_map_eq);

int mlx5_destroy_unmap_eq(struct mlx5_core_dev *dev, struct mlx5_eq *eq)
{
	int err;

	free_irq(dev->priv.msix_arr[eq->irqn].vector, eq);
	err = mlx5_cmd_destroy_eq(dev, eq->eqn);
	if (err)
		mlx5_core_warn(dev, "failed to destroy a previously created eq: eqn %d\n",
			       eq->eqn);
	mlx5_buf_free(dev, &eq->buf);

	return err;
}
EXPORT_SYMBOL_GPL(mlx5_destroy_unmap_eq);

int mlx5_eq_init(struct mlx5_core_dev *dev)
{
	int err;

	spin_lock_init(&dev->priv.eq_table.lock);

	err = 0;

	return err;
}


void mlx5_eq_cleanup(struct mlx5_core_dev *dev)
{
}

int mlx5_start_eqs(struct mlx5_core_dev *dev)
{
	struct mlx5_eq_table *table = &dev->priv.eq_table;
	u64 async_event_mask = MLX5_ASYNC_EVENT_MASK;
	int err;

	if (MLX5_CAP_GEN(dev, port_module_event))
		async_event_mask |= (1ull <<
				     MLX5_EVENT_TYPE_CODING_PORT_MODULE_EVENT);

	if (MLX5_CAP_GEN(dev, nic_vport_change_event))
		async_event_mask |= (1ull <<
				     MLX5_EVENT_TYPE_NIC_VPORT_CHANGE);

	if (MLX5_CAP_GEN(dev, dcbx))
		async_event_mask |= (1ull <<
				     MLX5_EVENT_TYPE_CODING_DCBX_CHANGE_EVENT);

	if (MLX5_CAP_GEN(dev, fpga))
		async_event_mask |= (1ull << MLX5_EVENT_TYPE_FPGA_ERROR) |
				    (1ull << MLX5_EVENT_TYPE_FPGA_QP_ERROR);

	if (MLX5_CAP_GEN(dev, temp_warn_event))
		async_event_mask |= (1ull << MLX5_EVENT_TYPE_TEMP_WARN_EVENT);

	if (MLX5_CAP_GEN(dev, general_notification_event)) {
		async_event_mask |= (1ull <<
		    MLX5_EVENT_TYPE_CODING_GENERAL_NOTIFICATION_EVENT);
	}

	if (mlx5_ipsec_device_caps(dev) & MLX5_IPSEC_CAP_PACKET_OFFLOAD)
		async_event_mask |=
			(1ull << MLX5_EVENT_TYPE_OBJECT_CHANGE);

	err = mlx5_create_map_eq(dev, &table->cmd_eq, MLX5_EQ_VEC_CMD,
				 MLX5_NUM_CMD_EQE, 1ull << MLX5_EVENT_TYPE_CMD);
	if (err) {
		mlx5_core_warn(dev, "failed to create cmd EQ %d\n", err);
		return err;
	}

	mlx5_cmd_use_events(dev);

	err = mlx5_create_map_eq(dev, &table->async_eq, MLX5_EQ_VEC_ASYNC,
				 MLX5_NUM_ASYNC_EQE, async_event_mask);
	if (err) {
		mlx5_core_warn(dev, "failed to create async EQ %d\n", err);
		goto err1;
	}

	err = mlx5_create_map_eq(dev, &table->pages_eq,
				 MLX5_EQ_VEC_PAGES,
				 /* TODO: sriov max_vf + */ 1,
				 1 << MLX5_EVENT_TYPE_PAGE_REQUEST);
	if (err) {
		mlx5_core_warn(dev, "failed to create pages EQ %d\n", err);
		goto err2;
	}

	return err;

err2:
	mlx5_destroy_unmap_eq(dev, &table->async_eq);

err1:
	mlx5_cmd_use_polling(dev);
	mlx5_destroy_unmap_eq(dev, &table->cmd_eq);
	return err;
}

int mlx5_stop_eqs(struct mlx5_core_dev *dev)
{
	struct mlx5_eq_table *table = &dev->priv.eq_table;
	int err;

	err = mlx5_destroy_unmap_eq(dev, &table->pages_eq);
	if (err)
		return err;

	mlx5_destroy_unmap_eq(dev, &table->async_eq);
	mlx5_cmd_use_polling(dev);

	err = mlx5_destroy_unmap_eq(dev, &table->cmd_eq);
	if (err)
		mlx5_cmd_use_events(dev);

	return err;
}

int mlx5_core_eq_query(struct mlx5_core_dev *dev, struct mlx5_eq *eq,
		       u32 *out, int outlen)
{
	u32 in[MLX5_ST_SZ_DW(query_eq_in)] = {0};

	memset(out, 0, outlen);
	MLX5_SET(query_eq_in, in, opcode, MLX5_CMD_OP_QUERY_EQ);
	MLX5_SET(query_eq_in, in, eq_number, eq->eqn);

	return mlx5_cmd_exec(dev, in, sizeof(in), out, outlen);
}
EXPORT_SYMBOL_GPL(mlx5_core_eq_query);

static const char *mlx5_port_module_event_error_type_to_string(u8 error_type)
{
	switch (error_type) {
	case MLX5_MODULE_EVENT_ERROR_POWER_BUDGET_EXCEEDED:
		return "Power budget exceeded";
	case MLX5_MODULE_EVENT_ERROR_LONG_RANGE_FOR_NON_MLNX_CABLE_MODULE:
		return "Long Range for non MLNX cable";
	case MLX5_MODULE_EVENT_ERROR_BUS_STUCK:
		return "Bus stuck(I2C or data shorted)";
	case MLX5_MODULE_EVENT_ERROR_NO_EEPROM_RETRY_TIMEOUT:
		return "No EEPROM/retry timeout";
	case MLX5_MODULE_EVENT_ERROR_ENFORCE_PART_NUMBER_LIST:
		return "Enforce part number list";
	case MLX5_MODULE_EVENT_ERROR_UNSUPPORTED_CABLE:
		return "Unknown identifier";
	case MLX5_MODULE_EVENT_ERROR_HIGH_TEMPERATURE:
		return "High Temperature";
	case MLX5_MODULE_EVENT_ERROR_CABLE_IS_SHORTED:
		return "Bad or shorted cable/module";
	case MLX5_MODULE_EVENT_ERROR_PMD_TYPE_NOT_ENABLED:
		return "PMD type is not enabled";
	case MLX5_MODULE_EVENT_ERROR_LASTER_TEC_FAILURE:
		return "Laster_TEC_failure";
	case MLX5_MODULE_EVENT_ERROR_HIGH_CURRENT:
		return "High_current";
	case MLX5_MODULE_EVENT_ERROR_HIGH_VOLTAGE:
		return "High_voltage";
	case MLX5_MODULE_EVENT_ERROR_PCIE_SYS_POWER_SLOT_EXCEEDED:
		return "pcie_system_power_slot_Exceeded";
	case MLX5_MODULE_EVENT_ERROR_HIGH_POWER:
		return "High_power";
	case MLX5_MODULE_EVENT_ERROR_MODULE_STATE_MACHINE_FAULT:
		return "Module_state_machine_fault";
	default:
		return "Unknown error type";
	}
}

unsigned int mlx5_query_module_status(struct mlx5_core_dev *dev, int module_num)
{
	if (module_num != dev->module_num)
		return 0;		/* module num doesn't equal to what FW reported */
	return dev->module_status;
}

static void mlx5_port_module_event(struct mlx5_core_dev *dev,
				   struct mlx5_eqe *eqe)
{
	unsigned int module_num;
	unsigned int module_status;
	unsigned int error_type;
	struct mlx5_eqe_port_module_event *module_event_eqe;

	module_event_eqe = &eqe->data.port_module_event;

	module_num = (unsigned int)module_event_eqe->module;
	module_status = (unsigned int)module_event_eqe->module_status &
	    PORT_MODULE_EVENT_MODULE_STATUS_MASK;
	error_type = (unsigned int)module_event_eqe->error_type &
	    PORT_MODULE_EVENT_ERROR_TYPE_MASK;

	if (module_status < MLX5_MODULE_STATUS_NUM)
		dev->priv.pme_stats.status_counters[module_status]++;
	switch (module_status) {
	case MLX5_MODULE_STATUS_PLUGGED_ENABLED:
		mlx5_core_info(dev,
		    "Module %u, status: plugged and enabled\n",
		    module_num);
		break;

	case MLX5_MODULE_STATUS_UNPLUGGED:
		mlx5_core_info(dev,
		    "Module %u, status: unplugged\n", module_num);
		break;

	case MLX5_MODULE_STATUS_ERROR:
		mlx5_core_err(dev,
		    "Module %u, status: error, %s (%d)\n",
		    module_num,
		    mlx5_port_module_event_error_type_to_string(error_type),
		    error_type);
		if (error_type < MLX5_MODULE_EVENT_ERROR_NUM)
			dev->priv.pme_stats.error_counters[error_type]++;
		break;

	default:
		mlx5_core_info(dev,
		    "Module %u, unknown status %d\n", module_num, module_status);
	}
	/* store module status */
	dev->module_status = module_status;
	dev->module_num = module_num;
}

static void mlx5_port_general_notification_event(struct mlx5_core_dev *dev,
						 struct mlx5_eqe *eqe)
{
	u8 port = (eqe->data.port.port >> 4) & 0xf;

	switch (eqe->sub_type) {
	case MLX5_GEN_EVENT_SUBTYPE_DELAY_DROP_TIMEOUT:
		break;
	case MLX5_GEN_EVENT_SUBTYPE_PCI_POWER_CHANGE_EVENT:
		mlx5_trigger_health_watchdog(dev);
		break;
	default:
		mlx5_core_warn(dev,
			       "general event with unrecognized subtype: port %d, sub_type %d\n",
			       port, eqe->sub_type);
		break;
	}
}

void
mlx5_disable_interrupts(struct mlx5_core_dev *dev)
{
	int nvec = dev->priv.eq_table.num_comp_vectors + MLX5_EQ_VEC_COMP_BASE;
	int x;

	for (x = 0; x != nvec; x++)
		disable_irq(dev->priv.msix_arr[x].vector);
}

void
mlx5_poll_interrupts(struct mlx5_core_dev *dev)
{
	struct mlx5_eq *eq;

	if (unlikely(dev->priv.disable_irqs != 0))
		return;

	mlx5_eq_int(dev, &dev->priv.eq_table.cmd_eq);
	mlx5_eq_int(dev, &dev->priv.eq_table.async_eq);
	mlx5_eq_int(dev, &dev->priv.eq_table.pages_eq);

	list_for_each_entry(eq, &dev->priv.eq_table.comp_eqs_list, list)
		mlx5_eq_int(dev, eq);
}
