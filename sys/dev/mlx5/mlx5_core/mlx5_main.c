/*-
 * Copyright (c) 2013-2015, Mellanox Technologies, Ltd.  All rights reserved.
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
 * $FreeBSD$
 */

#define	LINUXKPI_PARAM_PREFIX mlx5_

#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/io-mapping.h>
#include <linux/interrupt.h>
#include <dev/mlx5/driver.h>
#include <dev/mlx5/cq.h>
#include <dev/mlx5/qp.h>
#include <dev/mlx5/srq.h>
#include <linux/delay.h>
#include <dev/mlx5/mlx5_ifc.h>
#include "mlx5_core.h"

MODULE_AUTHOR("Eli Cohen <eli@mellanox.com>");
MODULE_DESCRIPTION("Mellanox Connect-IB, ConnectX-4 core driver");
MODULE_LICENSE("Dual BSD/GPL");
#if (__FreeBSD_version >= 1100000)
MODULE_DEPEND(mlx5, linuxkpi, 1, 1, 1);
#endif
MODULE_VERSION(mlx5, 1);

int mlx5_core_debug_mask;
module_param_named(debug_mask, mlx5_core_debug_mask, int, 0644);
MODULE_PARM_DESC(debug_mask, "debug mask: 1 = dump cmd data, 2 = dump cmd exec time, 3 = both. Default=0");

#define MLX5_DEFAULT_PROF	2
static int prof_sel = MLX5_DEFAULT_PROF;
module_param_named(prof_sel, prof_sel, int, 0444);
MODULE_PARM_DESC(prof_sel, "profile selector. Valid range 0 - 2");

#define NUMA_NO_NODE       -1

struct workqueue_struct *mlx5_core_wq;
static LIST_HEAD(intf_list);
static LIST_HEAD(dev_list);
static DEFINE_MUTEX(intf_mutex);

struct mlx5_device_context {
	struct list_head	list;
	struct mlx5_interface  *intf;
	void		       *context;
};

static struct mlx5_profile profiles[] = {
	[0] = {
		.mask           = 0,
	},
	[1] = {
		.mask		= MLX5_PROF_MASK_QP_SIZE,
		.log_max_qp	= 12,
	},
	[2] = {
		.mask		= MLX5_PROF_MASK_QP_SIZE |
				  MLX5_PROF_MASK_MR_CACHE,
		.log_max_qp	= 17,
		.mr_cache[0]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[1]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[2]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[3]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[4]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[5]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[6]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[7]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[8]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[9]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[10]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[11]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[12]	= {
			.size	= 64,
			.limit	= 32
		},
		.mr_cache[13]	= {
			.size	= 32,
			.limit	= 16
		},
		.mr_cache[14]	= {
			.size	= 16,
			.limit	= 8
		},
		.mr_cache[15]	= {
			.size	= 8,
			.limit	= 4
		},
	},
	[3] = {
		.mask		= MLX5_PROF_MASK_QP_SIZE,
		.log_max_qp	= 17,
	},
};

static int set_dma_caps(struct pci_dev *pdev)
{
	int err;

	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		device_printf((&pdev->dev)->bsddev, "WARN: ""Warning: couldn't set 64-bit PCI DMA mask\n");
		err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			device_printf((&pdev->dev)->bsddev, "ERR: ""Can't set PCI DMA mask, aborting\n");
			return err;
		}
	}

	err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		device_printf((&pdev->dev)->bsddev, "WARN: ""Warning: couldn't set 64-bit consistent PCI DMA mask\n");
		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			device_printf((&pdev->dev)->bsddev, "ERR: ""Can't set consistent PCI DMA mask, aborting\n");
			return err;
		}
	}

	dma_set_max_seg_size(&pdev->dev, 2u * 1024 * 1024 * 1024);
	return err;
}

static int request_bar(struct pci_dev *pdev)
{
	int err = 0;

	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""Missing registers BAR, aborting\n");
		return -ENODEV;
	}

	err = pci_request_regions(pdev, DRIVER_NAME);
	if (err)
		device_printf((&pdev->dev)->bsddev, "ERR: ""Couldn't get PCI resources, aborting\n");

	return err;
}

static void release_bar(struct pci_dev *pdev)
{
	pci_release_regions(pdev);
}

static int mlx5_enable_msix(struct mlx5_core_dev *dev)
{
	struct mlx5_priv *priv = &dev->priv;
	struct mlx5_eq_table *table = &priv->eq_table;
	int num_eqs = 1 << MLX5_CAP_GEN(dev, log_max_eq);
	int nvec;
	int i;

	nvec = MLX5_CAP_GEN(dev, num_ports) * num_online_cpus() +
	       MLX5_EQ_VEC_COMP_BASE;
	nvec = min_t(int, nvec, num_eqs);
	if (nvec <= MLX5_EQ_VEC_COMP_BASE)
		return -ENOMEM;

	priv->msix_arr = kzalloc(nvec * sizeof(*priv->msix_arr), GFP_KERNEL);

	priv->irq_info = kzalloc(nvec * sizeof(*priv->irq_info), GFP_KERNEL);

	for (i = 0; i < nvec; i++)
		priv->msix_arr[i].entry = i;

	nvec = pci_enable_msix_range(dev->pdev, priv->msix_arr,
				     MLX5_EQ_VEC_COMP_BASE + 1, nvec);
	if (nvec < 0)
		return nvec;

	table->num_comp_vectors = nvec - MLX5_EQ_VEC_COMP_BASE;

	return 0;

}

static void mlx5_disable_msix(struct mlx5_core_dev *dev)
{
	struct mlx5_priv *priv = &dev->priv;

	pci_disable_msix(dev->pdev);
	kfree(priv->irq_info);
	kfree(priv->msix_arr);
}

struct mlx5_reg_host_endianess {
	u8	he;
	u8      rsvd[15];
};


#define CAP_MASK(pos, size) ((u64)((1 << (size)) - 1) << (pos))

enum {
	MLX5_CAP_BITS_RW_MASK = CAP_MASK(MLX5_CAP_OFF_CMDIF_CSUM, 2) |
				MLX5_DEV_CAP_FLAG_DCT,
};

static u16 to_fw_pkey_sz(u32 size)
{
	switch (size) {
	case 128:
		return 0;
	case 256:
		return 1;
	case 512:
		return 2;
	case 1024:
		return 3;
	case 2048:
		return 4;
	case 4096:
		return 5;
	default:
		printf("mlx5_core: WARN: ""invalid pkey table size %d\n", size);
		return 0;
	}
}

int mlx5_core_get_caps(struct mlx5_core_dev *dev, enum mlx5_cap_type cap_type,
		       enum mlx5_cap_mode cap_mode)
{
	u8 in[MLX5_ST_SZ_BYTES(query_hca_cap_in)];
	int out_sz = MLX5_ST_SZ_BYTES(query_hca_cap_out);
	void *out, *hca_caps;
	u16 opmod = (cap_type << 1) | (cap_mode & 0x01);
	int err;

	memset(in, 0, sizeof(in));
	out = kzalloc(out_sz, GFP_KERNEL);

	MLX5_SET(query_hca_cap_in, in, opcode, MLX5_CMD_OP_QUERY_HCA_CAP);
	MLX5_SET(query_hca_cap_in, in, op_mod, opmod);
	err = mlx5_cmd_exec(dev, in, sizeof(in), out, out_sz);
	if (err)
		goto query_ex;

	err = mlx5_cmd_status_to_err_v2(out);
	if (err) {
		mlx5_core_warn(dev,
			       "QUERY_HCA_CAP : type(%x) opmode(%x) Failed(%d)\n",
			       cap_type, cap_mode, err);
		goto query_ex;
	}

	hca_caps =  MLX5_ADDR_OF(query_hca_cap_out, out, capability);

	switch (cap_mode) {
	case HCA_CAP_OPMOD_GET_MAX:
		memcpy(dev->hca_caps_max[cap_type], hca_caps,
		       MLX5_UN_SZ_BYTES(hca_cap_union));
		break;
	case HCA_CAP_OPMOD_GET_CUR:
		memcpy(dev->hca_caps_cur[cap_type], hca_caps,
		       MLX5_UN_SZ_BYTES(hca_cap_union));
		break;
	default:
		mlx5_core_warn(dev,
			       "Tried to query dev cap type(%x) with wrong opmode(%x)\n",
			       cap_type, cap_mode);
		err = -EINVAL;
		break;
	}
query_ex:
	kfree(out);
	return err;
}

static int set_caps(struct mlx5_core_dev *dev, void *in, int in_sz)
{
	u32 out[MLX5_ST_SZ_DW(set_hca_cap_out)];
	int err;

	memset(out, 0, sizeof(out));

	MLX5_SET(set_hca_cap_in, in, opcode, MLX5_CMD_OP_SET_HCA_CAP);
	err = mlx5_cmd_exec(dev, in, in_sz, out, sizeof(out));
	if (err)
		return err;

	err = mlx5_cmd_status_to_err_v2(out);

	return err;
}

static int handle_hca_cap(struct mlx5_core_dev *dev)
{
	void *set_ctx = NULL;
	struct mlx5_profile *prof = dev->profile;
	int err = -ENOMEM;
	int set_sz = MLX5_ST_SZ_BYTES(set_hca_cap_in);
	void *set_hca_cap;

	set_ctx = kzalloc(set_sz, GFP_KERNEL);

	err = mlx5_core_get_caps(dev, MLX5_CAP_GENERAL, HCA_CAP_OPMOD_GET_MAX);
	if (err)
		goto query_ex;

	err = mlx5_core_get_caps(dev, MLX5_CAP_GENERAL, HCA_CAP_OPMOD_GET_CUR);
	if (err)
		goto query_ex;

	set_hca_cap = MLX5_ADDR_OF(set_hca_cap_in, set_ctx,
				   capability);
	memcpy(set_hca_cap, dev->hca_caps_cur[MLX5_CAP_GENERAL],
	       MLX5_ST_SZ_BYTES(cmd_hca_cap));

	mlx5_core_dbg(dev, "Current Pkey table size %d Setting new size %d\n",
		      mlx5_to_sw_pkey_sz(MLX5_CAP_GEN(dev, pkey_table_size)),
		      128);
	/* we limit the size of the pkey table to 128 entries for now */
	MLX5_SET(cmd_hca_cap, set_hca_cap, pkey_table_size,
		 to_fw_pkey_sz(128));

	if (prof->mask & MLX5_PROF_MASK_QP_SIZE)
		MLX5_SET(cmd_hca_cap, set_hca_cap, log_max_qp,
			 prof->log_max_qp);

	/* disable cmdif checksum */
	MLX5_SET(cmd_hca_cap, set_hca_cap, cmdif_checksum, 0);

	MLX5_SET(cmd_hca_cap, set_hca_cap, log_uar_page_sz, PAGE_SHIFT - 12);

	err = set_caps(dev, set_ctx, set_sz);

query_ex:
	kfree(set_ctx);
	return err;
}

static int set_hca_ctrl(struct mlx5_core_dev *dev)
{
	struct mlx5_reg_host_endianess he_in;
	struct mlx5_reg_host_endianess he_out;
	int err;

	memset(&he_in, 0, sizeof(he_in));
	he_in.he = MLX5_SET_HOST_ENDIANNESS;
	err = mlx5_core_access_reg(dev, &he_in,  sizeof(he_in),
					&he_out, sizeof(he_out),
					MLX5_REG_HOST_ENDIANNESS, 0, 1);
	return err;
}

static int mlx5_core_enable_hca(struct mlx5_core_dev *dev)
{
	u32 in[MLX5_ST_SZ_DW(enable_hca_in)];
	u32 out[MLX5_ST_SZ_DW(enable_hca_out)];

	memset(in, 0, sizeof(in));
	MLX5_SET(enable_hca_in, in, opcode, MLX5_CMD_OP_ENABLE_HCA);
	memset(out, 0, sizeof(out));
	return mlx5_cmd_exec_check_status(dev, in,  sizeof(in),
					       out, sizeof(out));
}

static int mlx5_core_disable_hca(struct mlx5_core_dev *dev)
{
	u32 in[MLX5_ST_SZ_DW(disable_hca_in)];
	u32 out[MLX5_ST_SZ_DW(disable_hca_out)];

	memset(in, 0, sizeof(in));

	MLX5_SET(disable_hca_in, in, opcode, MLX5_CMD_OP_DISABLE_HCA);
	memset(out, 0, sizeof(out));
	return mlx5_cmd_exec_check_status(dev, in,  sizeof(in),
					       out, sizeof(out));
}

static int mlx5_core_set_issi(struct mlx5_core_dev *dev)
{
	u32 query_in[MLX5_ST_SZ_DW(query_issi_in)];
	u32 query_out[MLX5_ST_SZ_DW(query_issi_out)];
	u32 set_in[MLX5_ST_SZ_DW(set_issi_in)];
	u32 set_out[MLX5_ST_SZ_DW(set_issi_out)];
	int err;
	u32 sup_issi;

	memset(query_in, 0, sizeof(query_in));
	memset(query_out, 0, sizeof(query_out));

	MLX5_SET(query_issi_in, query_in, opcode, MLX5_CMD_OP_QUERY_ISSI);

	err = mlx5_cmd_exec_check_status(dev, query_in, sizeof(query_in),
					 query_out, sizeof(query_out));
	if (err) {
		if (((struct mlx5_outbox_hdr *)query_out)->status ==
		    MLX5_CMD_STAT_BAD_OP_ERR) {
			pr_debug("Only ISSI 0 is supported\n");
			return 0;
		}

		printf("mlx5_core: ERR: ""failed to query ISSI\n");
		return err;
	}

	sup_issi = MLX5_GET(query_issi_out, query_out, supported_issi_dw0);

	if (sup_issi & (1 << 1)) {
		memset(set_in, 0, sizeof(set_in));
		memset(set_out, 0, sizeof(set_out));

		MLX5_SET(set_issi_in, set_in, opcode, MLX5_CMD_OP_SET_ISSI);
		MLX5_SET(set_issi_in, set_in, current_issi, 1);

		err = mlx5_cmd_exec_check_status(dev, set_in, sizeof(set_in),
						 set_out, sizeof(set_out));
		if (err) {
			printf("mlx5_core: ERR: ""failed to set ISSI=1\n");
			return err;
		}

		dev->issi = 1;

		return 0;
	} else if (sup_issi & (1 << 0)) {
		return 0;
	}

	return -ENOTSUPP;
}


int mlx5_vector2eqn(struct mlx5_core_dev *dev, int vector, int *eqn, int *irqn)
{
	struct mlx5_eq_table *table = &dev->priv.eq_table;
	struct mlx5_eq *eq;
	int err = -ENOENT;

	spin_lock(&table->lock);
	list_for_each_entry(eq, &table->comp_eqs_list, list) {
		if (eq->index == vector) {
			*eqn = eq->eqn;
			*irqn = eq->irqn;
			err = 0;
			break;
		}
	}
	spin_unlock(&table->lock);

	return err;
}
EXPORT_SYMBOL(mlx5_vector2eqn);

int mlx5_rename_eq(struct mlx5_core_dev *dev, int eq_ix, char *name)
{
	struct mlx5_priv *priv = &dev->priv;
	struct mlx5_eq_table *table = &priv->eq_table;
	struct mlx5_eq *eq;
	int err = -ENOENT;

	spin_lock(&table->lock);
	list_for_each_entry(eq, &table->comp_eqs_list, list) {
		if (eq->index == eq_ix) {
			int irq_ix = eq_ix + MLX5_EQ_VEC_COMP_BASE;

			snprintf(priv->irq_info[irq_ix].name, MLX5_MAX_IRQ_NAME,
				 "%s-%d", name, eq_ix);

			err = 0;
			break;
		}
	}
	spin_unlock(&table->lock);

	return err;
}

static void free_comp_eqs(struct mlx5_core_dev *dev)
{
	struct mlx5_eq_table *table = &dev->priv.eq_table;
	struct mlx5_eq *eq, *n;

	spin_lock(&table->lock);
	list_for_each_entry_safe(eq, n, &table->comp_eqs_list, list) {
		list_del(&eq->list);
		spin_unlock(&table->lock);
		if (mlx5_destroy_unmap_eq(dev, eq))
			mlx5_core_warn(dev, "failed to destroy EQ 0x%x\n",
				       eq->eqn);
		kfree(eq);
		spin_lock(&table->lock);
	}
	spin_unlock(&table->lock);
}

static int alloc_comp_eqs(struct mlx5_core_dev *dev)
{
	struct mlx5_eq_table *table = &dev->priv.eq_table;
	char name[MLX5_MAX_IRQ_NAME];
	struct mlx5_eq *eq;
	int ncomp_vec;
	int nent;
	int err;
	int i;

	INIT_LIST_HEAD(&table->comp_eqs_list);
	ncomp_vec = table->num_comp_vectors;
	nent = MLX5_COMP_EQ_SIZE;
	for (i = 0; i < ncomp_vec; i++) {
		eq = kzalloc(sizeof(*eq), GFP_KERNEL);

		snprintf(name, MLX5_MAX_IRQ_NAME, "mlx5_comp%d", i);
		err = mlx5_create_map_eq(dev, eq,
					 i + MLX5_EQ_VEC_COMP_BASE, nent, 0,
					 name, &dev->priv.uuari.uars[0]);
		if (err) {
			kfree(eq);
			goto clean;
		}
		mlx5_core_dbg(dev, "allocated completion EQN %d\n", eq->eqn);
		eq->index = i;
		spin_lock(&table->lock);
		list_add_tail(&eq->list, &table->comp_eqs_list);
		spin_unlock(&table->lock);
	}

	return 0;

clean:
	free_comp_eqs(dev);
	return err;
}

static int map_bf_area(struct mlx5_core_dev *dev)
{
	resource_size_t bf_start = pci_resource_start(dev->pdev, 0);
	resource_size_t bf_len = pci_resource_len(dev->pdev, 0);

	dev->priv.bf_mapping = io_mapping_create_wc(bf_start, bf_len);

	return dev->priv.bf_mapping ? 0 : -ENOMEM;
}

static void unmap_bf_area(struct mlx5_core_dev *dev)
{
	if (dev->priv.bf_mapping)
		io_mapping_free(dev->priv.bf_mapping);
}

static inline int fw_initializing(struct mlx5_core_dev *dev)
{
	return ioread32be(&dev->iseg->initializing) >> 31;
}

static int wait_fw_init(struct mlx5_core_dev *dev, u32 max_wait_mili)
{
	u64 end = jiffies + msecs_to_jiffies(max_wait_mili);
	int err = 0;

	while (fw_initializing(dev)) {
		if (time_after(jiffies, end)) {
			err = -EBUSY;
			break;
		}
		msleep(FW_INIT_WAIT_MS);
	}

	return err;
}

static int mlx5_dev_init(struct mlx5_core_dev *dev, struct pci_dev *pdev)
{
	struct mlx5_priv *priv = &dev->priv;
	int err;

	dev->pdev = pdev;
	pci_set_drvdata(dev->pdev, dev);
	strncpy(priv->name, dev_name(&pdev->dev), MLX5_MAX_NAME_LEN);
	priv->name[MLX5_MAX_NAME_LEN - 1] = 0;

	mutex_init(&priv->pgdir_mutex);
	INIT_LIST_HEAD(&priv->pgdir_list);
	spin_lock_init(&priv->mkey_lock);

	priv->numa_node = NUMA_NO_NODE;

	err = pci_enable_device(pdev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""Cannot enable PCI device, aborting\n");
		goto err_dbg;
	}

	err = request_bar(pdev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""error requesting BARs, aborting\n");
		goto err_disable;
	}

	pci_set_master(pdev);

	err = set_dma_caps(pdev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""Failed setting DMA capabilities mask, aborting\n");
		goto err_clr_master;
	}

	dev->iseg = ioremap(pci_resource_start(dev->pdev, 0),
			    sizeof(*dev->iseg));
	if (!dev->iseg) {
		err = -ENOMEM;
		device_printf((&pdev->dev)->bsddev, "ERR: ""Failed mapping initialization segment, aborting\n");
		goto err_clr_master;
	}
	device_printf((&pdev->dev)->bsddev, "INFO: ""firmware version: %d.%d.%d\n", fw_rev_maj(dev), fw_rev_min(dev), fw_rev_sub(dev));

	err = mlx5_cmd_init(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""Failed initializing command interface, aborting\n");
		goto err_unmap;
	}

	err = wait_fw_init(dev, FW_INIT_TIMEOUT_MILI);
	if (err) {
		device_printf((&dev->pdev->dev)->bsddev, "ERR: ""Firmware over %d MS in initializing state, aborting\n", FW_INIT_TIMEOUT_MILI);
		goto err_cmd_cleanup;
	}

	mlx5_pagealloc_init(dev);

	err = mlx5_core_enable_hca(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""enable hca failed\n");
		goto err_pagealloc_cleanup;
	}

	err = mlx5_core_set_issi(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""failed to set issi\n");
		goto err_disable_hca;
	}

	err = mlx5_pagealloc_start(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""mlx5_pagealloc_start failed\n");
		goto err_disable_hca;
	}

	err = mlx5_satisfy_startup_pages(dev, 1);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""failed to allocate boot pages\n");
		goto err_pagealloc_stop;
	}

	err = set_hca_ctrl(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""set_hca_ctrl failed\n");
		goto reclaim_boot_pages;
	}

	err = handle_hca_cap(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""handle_hca_cap failed\n");
		goto reclaim_boot_pages;
	}

	err = mlx5_satisfy_startup_pages(dev, 0);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""failed to allocate init pages\n");
		goto reclaim_boot_pages;
	}

	err = mlx5_cmd_init_hca(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""init hca failed\n");
		goto reclaim_boot_pages;
	}

	mlx5_start_health_poll(dev);

	err = mlx5_query_hca_caps(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""query hca failed\n");
		goto err_stop_poll;
	}

	err = mlx5_query_board_id(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""query board id failed\n");
		goto err_stop_poll;
	}

	err = mlx5_enable_msix(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""enable msix failed\n");
		goto err_stop_poll;
	}

	err = mlx5_eq_init(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""failed to initialize eq\n");
		goto disable_msix;
	}

	err = mlx5_alloc_uuars(dev, &priv->uuari);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""Failed allocating uar, aborting\n");
		goto err_eq_cleanup;
	}

	err = mlx5_start_eqs(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""Failed to start pages and async EQs\n");
		goto err_free_uar;
	}

	err = alloc_comp_eqs(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""Failed to alloc completion EQs\n");
		goto err_stop_eqs;
	}

	if (map_bf_area(dev))
		device_printf((&pdev->dev)->bsddev, "ERR: ""Failed to map blue flame area\n");

	MLX5_INIT_DOORBELL_LOCK(&priv->cq_uar_lock);

	mlx5_init_cq_table(dev);
	mlx5_init_qp_table(dev);
	mlx5_init_srq_table(dev);
	mlx5_init_mr_table(dev);

	return 0;

err_stop_eqs:
	mlx5_stop_eqs(dev);

err_free_uar:
	mlx5_free_uuars(dev, &priv->uuari);

err_eq_cleanup:
	mlx5_eq_cleanup(dev);

disable_msix:
	mlx5_disable_msix(dev);

err_stop_poll:
	mlx5_stop_health_poll(dev);
	if (mlx5_cmd_teardown_hca(dev)) {
		device_printf((&dev->pdev->dev)->bsddev, "ERR: ""tear_down_hca failed, skip cleanup\n");
		return err;
	}

reclaim_boot_pages:
	mlx5_reclaim_startup_pages(dev);

err_pagealloc_stop:
	mlx5_pagealloc_stop(dev);

err_disable_hca:
	mlx5_core_disable_hca(dev);

err_pagealloc_cleanup:
	mlx5_pagealloc_cleanup(dev);
err_cmd_cleanup:
	mlx5_cmd_cleanup(dev);

err_unmap:
	iounmap(dev->iseg);

err_clr_master:
	pci_clear_master(dev->pdev);
	release_bar(dev->pdev);

err_disable:
	pci_disable_device(dev->pdev);

err_dbg:
	return err;
}

static void mlx5_dev_cleanup(struct mlx5_core_dev *dev)
{
	struct mlx5_priv *priv = &dev->priv;

	mlx5_cleanup_mr_table(dev);
	mlx5_cleanup_srq_table(dev);
	mlx5_cleanup_qp_table(dev);
	mlx5_cleanup_cq_table(dev);
	unmap_bf_area(dev);
	free_comp_eqs(dev);
	mlx5_stop_eqs(dev);
	mlx5_free_uuars(dev, &priv->uuari);
	mlx5_eq_cleanup(dev);
	mlx5_disable_msix(dev);
	mlx5_stop_health_poll(dev);
	if (mlx5_cmd_teardown_hca(dev)) {
		device_printf((&dev->pdev->dev)->bsddev, "ERR: ""tear_down_hca failed, skip cleanup\n");
		return;
	}
	mlx5_pagealloc_stop(dev);
	mlx5_reclaim_startup_pages(dev);
	mlx5_core_disable_hca(dev);
	mlx5_pagealloc_cleanup(dev);
	mlx5_cmd_cleanup(dev);
	iounmap(dev->iseg);
	pci_clear_master(dev->pdev);
	release_bar(dev->pdev);
	pci_disable_device(dev->pdev);
}

static void mlx5_add_device(struct mlx5_interface *intf, struct mlx5_priv *priv)
{
	struct mlx5_device_context *dev_ctx;
	struct mlx5_core_dev *dev = container_of(priv, struct mlx5_core_dev, priv);

	dev_ctx = kmalloc(sizeof(*dev_ctx), GFP_KERNEL);

	dev_ctx->intf    = intf;
	dev_ctx->context = intf->add(dev);

	if (dev_ctx->context) {
		spin_lock_irq(&priv->ctx_lock);
		list_add_tail(&dev_ctx->list, &priv->ctx_list);
		spin_unlock_irq(&priv->ctx_lock);
	} else {
		kfree(dev_ctx);
	}
}

static void mlx5_remove_device(struct mlx5_interface *intf, struct mlx5_priv *priv)
{
	struct mlx5_device_context *dev_ctx;
	struct mlx5_core_dev *dev = container_of(priv, struct mlx5_core_dev, priv);

	list_for_each_entry(dev_ctx, &priv->ctx_list, list)
		if (dev_ctx->intf == intf) {
			spin_lock_irq(&priv->ctx_lock);
			list_del(&dev_ctx->list);
			spin_unlock_irq(&priv->ctx_lock);

			intf->remove(dev, dev_ctx->context);
			kfree(dev_ctx);
			return;
		}
}
static int mlx5_register_device(struct mlx5_core_dev *dev)
{
	struct mlx5_priv *priv = &dev->priv;
	struct mlx5_interface *intf;

	mutex_lock(&intf_mutex);
	list_add_tail(&priv->dev_list, &dev_list);
	list_for_each_entry(intf, &intf_list, list)
		mlx5_add_device(intf, priv);
	mutex_unlock(&intf_mutex);

	return 0;
}
static void mlx5_unregister_device(struct mlx5_core_dev *dev)
{
	struct mlx5_priv *priv = &dev->priv;
	struct mlx5_interface *intf;

	mutex_lock(&intf_mutex);
	list_for_each_entry(intf, &intf_list, list)
		mlx5_remove_device(intf, priv);
	list_del(&priv->dev_list);
	mutex_unlock(&intf_mutex);
}

int mlx5_register_interface(struct mlx5_interface *intf)
{
	struct mlx5_priv *priv;

	if (!intf->add || !intf->remove)
		return -EINVAL;

	mutex_lock(&intf_mutex);
	list_add_tail(&intf->list, &intf_list);
	list_for_each_entry(priv, &dev_list, dev_list)
		mlx5_add_device(intf, priv);
	mutex_unlock(&intf_mutex);

	return 0;
}
EXPORT_SYMBOL(mlx5_register_interface);

void mlx5_unregister_interface(struct mlx5_interface *intf)
{
	struct mlx5_priv *priv;

	mutex_lock(&intf_mutex);
	list_for_each_entry(priv, &dev_list, dev_list)
	       mlx5_remove_device(intf, priv);
	list_del(&intf->list);
	mutex_unlock(&intf_mutex);
}
EXPORT_SYMBOL(mlx5_unregister_interface);

void *mlx5_get_protocol_dev(struct mlx5_core_dev *mdev, int protocol)
{
	struct mlx5_priv *priv = &mdev->priv;
	struct mlx5_device_context *dev_ctx;
	unsigned long flags;
	void *result = NULL;

	spin_lock_irqsave(&priv->ctx_lock, flags);

	list_for_each_entry(dev_ctx, &mdev->priv.ctx_list, list)
		if ((dev_ctx->intf->protocol == protocol) &&
		    dev_ctx->intf->get_dev) {
			result = dev_ctx->intf->get_dev(dev_ctx->context);
			break;
		}

	spin_unlock_irqrestore(&priv->ctx_lock, flags);

	return result;
}
EXPORT_SYMBOL(mlx5_get_protocol_dev);

static void mlx5_core_event(struct mlx5_core_dev *dev, enum mlx5_dev_event event,
			    unsigned long param)
{
	struct mlx5_priv *priv = &dev->priv;
	struct mlx5_device_context *dev_ctx;
	unsigned long flags;

	spin_lock_irqsave(&priv->ctx_lock, flags);

	list_for_each_entry(dev_ctx, &priv->ctx_list, list)
		if (dev_ctx->intf->event)
			dev_ctx->intf->event(dev, dev_ctx->context, event, param);

	spin_unlock_irqrestore(&priv->ctx_lock, flags);
}

struct mlx5_core_event_handler {
	void (*event)(struct mlx5_core_dev *dev,
		      enum mlx5_dev_event event,
		      void *data);
};


static int init_one(struct pci_dev *pdev,
		    const struct pci_device_id *id)
{
	struct mlx5_core_dev *dev;
	struct mlx5_priv *priv;
	int err;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	priv = &dev->priv;

	if (prof_sel < 0 || prof_sel >= ARRAY_SIZE(profiles)) {
		printf("mlx5_core: WARN: ""selected profile out of range, selecting default (%d)\n", MLX5_DEFAULT_PROF);
		prof_sel = MLX5_DEFAULT_PROF;
	}
	dev->profile = &profiles[prof_sel];
	dev->event = mlx5_core_event;

	INIT_LIST_HEAD(&priv->ctx_list);
	spin_lock_init(&priv->ctx_lock);
	err = mlx5_dev_init(dev, pdev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""mlx5_dev_init failed %d\n", err);
		goto out;
	}

	err = mlx5_register_device(dev);
	if (err) {
		device_printf((&pdev->dev)->bsddev, "ERR: ""mlx5_register_device failed %d\n", err);
		goto out_init;
	}


	return 0;

out_init:
	mlx5_dev_cleanup(dev);
out:
	kfree(dev);
	return err;
}

static void remove_one(struct pci_dev *pdev)
{
	struct mlx5_core_dev *dev  = pci_get_drvdata(pdev);

	mlx5_unregister_device(dev);
	mlx5_dev_cleanup(dev);
	kfree(dev);
}

static const struct pci_device_id mlx5_core_pci_table[] = {
	{ PCI_VDEVICE(MELLANOX, 4113) }, /* Connect-IB */
	{ PCI_VDEVICE(MELLANOX, 4114) }, /* Connect-IB VF */
	{ PCI_VDEVICE(MELLANOX, 4115) }, /* ConnectX-4 */
	{ PCI_VDEVICE(MELLANOX, 4116) }, /* ConnectX-4 VF */
	{ PCI_VDEVICE(MELLANOX, 4117) }, /* ConnectX-4LX */
	{ PCI_VDEVICE(MELLANOX, 4118) }, /* ConnectX-4LX VF */
	{ PCI_VDEVICE(MELLANOX, 4119) },
	{ PCI_VDEVICE(MELLANOX, 4120) },
	{ PCI_VDEVICE(MELLANOX, 4121) },
	{ PCI_VDEVICE(MELLANOX, 4122) },
	{ PCI_VDEVICE(MELLANOX, 4123) },
	{ PCI_VDEVICE(MELLANOX, 4124) },
	{ PCI_VDEVICE(MELLANOX, 4125) },
	{ PCI_VDEVICE(MELLANOX, 4126) },
	{ PCI_VDEVICE(MELLANOX, 4127) },
	{ PCI_VDEVICE(MELLANOX, 4128) },
	{ PCI_VDEVICE(MELLANOX, 4129) },
	{ PCI_VDEVICE(MELLANOX, 4130) },
	{ PCI_VDEVICE(MELLANOX, 4131) },
	{ PCI_VDEVICE(MELLANOX, 4132) },
	{ PCI_VDEVICE(MELLANOX, 4133) },
	{ PCI_VDEVICE(MELLANOX, 4134) },
	{ PCI_VDEVICE(MELLANOX, 4135) },
	{ PCI_VDEVICE(MELLANOX, 4136) },
	{ PCI_VDEVICE(MELLANOX, 4137) },
	{ PCI_VDEVICE(MELLANOX, 4138) },
	{ PCI_VDEVICE(MELLANOX, 4139) },
	{ PCI_VDEVICE(MELLANOX, 4140) },
	{ PCI_VDEVICE(MELLANOX, 4141) },
	{ PCI_VDEVICE(MELLANOX, 4142) },
	{ PCI_VDEVICE(MELLANOX, 4143) },
	{ PCI_VDEVICE(MELLANOX, 4144) },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, mlx5_core_pci_table);

static struct pci_driver mlx5_core_driver = {
	.name           = DRIVER_NAME,
	.id_table       = mlx5_core_pci_table,
	.probe          = init_one,
	.remove         = remove_one
};

static int __init init(void)
{
	int err;

	mlx5_core_wq = create_singlethread_workqueue("mlx5_core_wq");
	if (!mlx5_core_wq) {
		err = -ENOMEM;
		goto err_debug;
	}
	mlx5_health_init();

	err = pci_register_driver(&mlx5_core_driver);
	if (err)
		goto err_health;


	return 0;

err_health:
	mlx5_health_cleanup();
	destroy_workqueue(mlx5_core_wq);
err_debug:
	return err;
}

static void __exit cleanup(void)
{
	pci_unregister_driver(&mlx5_core_driver);
	mlx5_health_cleanup();
	destroy_workqueue(mlx5_core_wq);
}

module_init(init);
module_exit(cleanup);
