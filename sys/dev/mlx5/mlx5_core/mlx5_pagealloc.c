/*-
 * Copyright (c) 2013-2017, Mellanox Technologies, Ltd.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <dev/mlx5/driver.h>
#include "mlx5_core.h"

CTASSERT((uintptr_t)PAGE_MASK > (uintptr_t)PAGE_SIZE);

struct mlx5_pages_req {
	struct mlx5_core_dev *dev;
	u16	func_id;
	s32	npages;
	struct work_struct work;
};


struct mlx5_manage_pages_inbox {
	struct mlx5_inbox_hdr	hdr;
	__be16			rsvd;
	__be16			func_id;
	__be32			num_entries;
	__be64			pas[0];
};

struct mlx5_manage_pages_outbox {
	struct mlx5_outbox_hdr	hdr;
	__be32			num_entries;
	u8			rsvd[4];
	__be64			pas[0];
};

enum {
	MAX_RECLAIM_TIME_MSECS	= 5000,
};

static void
mlx5_fwp_load_mem_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct mlx5_fw_page *fwp;
	uint8_t owned;

	fwp = (struct mlx5_fw_page *)arg;
	owned = MLX5_DMA_OWNED(fwp->dev);

	if (!owned)
		MLX5_DMA_LOCK(fwp->dev);

	if (error == 0) {
		KASSERT(nseg == 1, ("Number of segments is different from 1"));
		fwp->dma_addr = segs->ds_addr;
		fwp->load_done = MLX5_LOAD_ST_SUCCESS;
	} else {
		fwp->load_done = MLX5_LOAD_ST_FAILURE;
	}
	MLX5_DMA_DONE(fwp->dev);

	if (!owned)
		MLX5_DMA_UNLOCK(fwp->dev);
}

void
mlx5_fwp_flush(struct mlx5_fw_page *fwp)
{
	unsigned num = fwp->numpages;

	while (num--)
		bus_dmamap_sync(fwp[num].dev->cmd.dma_tag, fwp[num].dma_map, BUS_DMASYNC_PREWRITE);
}

void
mlx5_fwp_invalidate(struct mlx5_fw_page *fwp)
{
	unsigned num = fwp->numpages;

	while (num--) {
		bus_dmamap_sync(fwp[num].dev->cmd.dma_tag, fwp[num].dma_map, BUS_DMASYNC_POSTREAD);
		bus_dmamap_sync(fwp[num].dev->cmd.dma_tag, fwp[num].dma_map, BUS_DMASYNC_PREREAD);
	}
}

struct mlx5_fw_page *
mlx5_fwp_alloc(struct mlx5_core_dev *dev, gfp_t flags, unsigned num)
{
	struct mlx5_fw_page *fwp;
	unsigned x;
	int err;

	/* check for special case */
	if (num == 0) {
		fwp = kzalloc(sizeof(*fwp), flags);
		if (fwp != NULL)
			fwp->dev = dev;
		return (fwp);
	}

	/* we need sleeping context for this function */
	if (flags & M_NOWAIT)
		return (NULL);

	fwp = kzalloc(sizeof(*fwp) * num, flags);

	/* serialize loading the DMA map(s) */
	sx_xlock(&dev->cmd.dma_sx);

	for (x = 0; x != num; x++) {
		/* store pointer to MLX5 core device */
		fwp[x].dev = dev;
		/* store number of pages left from the array */
		fwp[x].numpages = num - x;

		/* allocate memory */
		err = bus_dmamem_alloc(dev->cmd.dma_tag, &fwp[x].virt_addr,
		    BUS_DMA_WAITOK | BUS_DMA_COHERENT, &fwp[x].dma_map);
		if (err != 0)
			goto failure;

		/* load memory into DMA */
		MLX5_DMA_LOCK(dev);
		err = bus_dmamap_load(
		    dev->cmd.dma_tag, fwp[x].dma_map, fwp[x].virt_addr,
		    MLX5_ADAPTER_PAGE_SIZE, &mlx5_fwp_load_mem_cb,
		    fwp + x, BUS_DMA_WAITOK | BUS_DMA_COHERENT);

		while (fwp[x].load_done == MLX5_LOAD_ST_NONE)
			MLX5_DMA_WAIT(dev);
		MLX5_DMA_UNLOCK(dev);

		/* check for error */
		if (fwp[x].load_done != MLX5_LOAD_ST_SUCCESS) {
			bus_dmamem_free(dev->cmd.dma_tag, fwp[x].virt_addr,
			    fwp[x].dma_map);
			goto failure;
		}
	}
	sx_xunlock(&dev->cmd.dma_sx);
	return (fwp);

failure:
	while (x--) {
		bus_dmamap_unload(dev->cmd.dma_tag, fwp[x].dma_map);
		bus_dmamem_free(dev->cmd.dma_tag, fwp[x].virt_addr, fwp[x].dma_map);
	}
	sx_xunlock(&dev->cmd.dma_sx);
	return (NULL);
}

void
mlx5_fwp_free(struct mlx5_fw_page *fwp)
{
	struct mlx5_core_dev *dev;
	unsigned num;

	/* be NULL safe */
	if (fwp == NULL)
		return;

	/* check for special case */
	if (fwp->numpages == 0) {
		kfree(fwp);
		return;
	}

	num = fwp->numpages;
	dev = fwp->dev;

	/* serialize unloading the DMA maps */
	sx_xlock(&dev->cmd.dma_sx);
	while (num--) {
		bus_dmamap_unload(dev->cmd.dma_tag, fwp[num].dma_map);
		bus_dmamem_free(dev->cmd.dma_tag, fwp[num].virt_addr, fwp[num].dma_map);
	}
	sx_xunlock(&dev->cmd.dma_sx);

	kfree(fwp);
}

u64
mlx5_fwp_get_dma(struct mlx5_fw_page *fwp, size_t offset)
{
	size_t index = (offset / MLX5_ADAPTER_PAGE_SIZE);
	KASSERT(index < fwp->numpages, ("Invalid offset: %lld", (long long)offset));

	return ((fwp + index)->dma_addr + (offset % MLX5_ADAPTER_PAGE_SIZE));
}

void *
mlx5_fwp_get_virt(struct mlx5_fw_page *fwp, size_t offset)
{
	size_t index = (offset / MLX5_ADAPTER_PAGE_SIZE);
	KASSERT(index < fwp->numpages, ("Invalid offset: %lld", (long long)offset));

	return ((char *)(fwp + index)->virt_addr + (offset % MLX5_ADAPTER_PAGE_SIZE));
}

static int
mlx5_insert_fw_page_locked(struct mlx5_core_dev *dev, struct mlx5_fw_page *nfp)
{
	struct rb_root *root = &dev->priv.page_root;
	struct rb_node **new = &root->rb_node;
	struct rb_node *parent = NULL;
	struct mlx5_fw_page *tfp;

	while (*new) {
		parent = *new;
		tfp = rb_entry(parent, struct mlx5_fw_page, rb_node);
		if (tfp->dma_addr < nfp->dma_addr)
			new = &parent->rb_left;
		else if (tfp->dma_addr > nfp->dma_addr)
			new = &parent->rb_right;
		else
			return (-EEXIST);
	}

	rb_link_node(&nfp->rb_node, parent, new);
	rb_insert_color(&nfp->rb_node, root);
	return (0);
}

static struct mlx5_fw_page *
mlx5_remove_fw_page_locked(struct mlx5_core_dev *dev, bus_addr_t addr)
{
	struct rb_root *root = &dev->priv.page_root;
	struct rb_node *tmp = root->rb_node;
	struct mlx5_fw_page *result = NULL;
	struct mlx5_fw_page *tfp;

	while (tmp) {
		tfp = rb_entry(tmp, struct mlx5_fw_page, rb_node);
		if (tfp->dma_addr < addr) {
			tmp = tmp->rb_left;
		} else if (tfp->dma_addr > addr) {
			tmp = tmp->rb_right;
		} else {
			rb_erase(&tfp->rb_node, &dev->priv.page_root);
			result = tfp;
			break;
		}
	}
	return (result);
}

static int
alloc_4k(struct mlx5_core_dev *dev, u64 *addr, u16 func_id)
{
	struct mlx5_fw_page *fwp;
	int err;

	fwp = mlx5_fwp_alloc(dev, GFP_KERNEL, 1);
	if (fwp == NULL)
		return (-ENOMEM);

	fwp->func_id = func_id;

	MLX5_DMA_LOCK(dev);
	err = mlx5_insert_fw_page_locked(dev, fwp);
	MLX5_DMA_UNLOCK(dev);

	if (err != 0) {
		mlx5_fwp_free(fwp);
	} else {
		/* make sure cached data is cleaned */
		mlx5_fwp_invalidate(fwp);

		/* store DMA address */
		*addr = fwp->dma_addr;
	}
	return (err);
}

static void
free_4k(struct mlx5_core_dev *dev, u64 addr)
{
	struct mlx5_fw_page *fwp;

	MLX5_DMA_LOCK(dev);
	fwp = mlx5_remove_fw_page_locked(dev, addr);
	MLX5_DMA_UNLOCK(dev);

	if (fwp == NULL) {
		mlx5_core_warn(dev, "Cannot free 4K page at 0x%llx\n", (long long)addr);
		return;
	}
	mlx5_fwp_free(fwp);
}

static int mlx5_cmd_query_pages(struct mlx5_core_dev *dev, u16 *func_id,
				s32 *npages, int boot)
{
	u32 in[MLX5_ST_SZ_DW(query_pages_in)];
	u32 out[MLX5_ST_SZ_DW(query_pages_out)];
	int err;

	memset(in, 0, sizeof(in));

	MLX5_SET(query_pages_in, in, opcode, MLX5_CMD_OP_QUERY_PAGES);
	MLX5_SET(query_pages_in, in, op_mod,
		 boot ? MLX5_BOOT_PAGES : MLX5_INIT_PAGES);

	memset(out, 0, sizeof(out));
	err = mlx5_cmd_exec_check_status(dev, in, sizeof(in), out, sizeof(out));
	if (err)
		return err;

	*npages = MLX5_GET(query_pages_out, out, num_pages);
	*func_id = MLX5_GET(query_pages_out, out, function_id);

	return 0;
}

static int give_pages(struct mlx5_core_dev *dev, u16 func_id, int npages,
		      int notify_fail)
{
	struct mlx5_manage_pages_inbox *in;
	struct mlx5_manage_pages_outbox out;
	struct mlx5_manage_pages_inbox *nin;
	int inlen;
	u64 addr;
	int err;
	int i = 0;

	inlen = sizeof(*in) + npages * sizeof(in->pas[0]);
	in = mlx5_vzalloc(inlen);
	if (!in) {
		mlx5_core_warn(dev, "vzalloc failed %d\n", inlen);
		err = -ENOMEM;
		goto out_alloc;
	}
	memset(&out, 0, sizeof(out));

	for (i = 0; i < npages; i++) {
		err = alloc_4k(dev, &addr, func_id);
		if (err)
			goto out_alloc;
		in->pas[i] = cpu_to_be64(addr);
	}

	in->hdr.opcode = cpu_to_be16(MLX5_CMD_OP_MANAGE_PAGES);
	in->hdr.opmod = cpu_to_be16(MLX5_PAGES_GIVE);
	in->func_id = cpu_to_be16(func_id);
	in->num_entries = cpu_to_be32(npages);
	err = mlx5_cmd_exec(dev, in, inlen, &out, sizeof(out));
	if (err) {
		mlx5_core_warn(dev, "func_id 0x%x, npages %d, err %d\n",
			       func_id, npages, err);
		goto out_alloc;
	}
	dev->priv.fw_pages += npages;
	dev->priv.pages_per_func[func_id] += npages;

	if (out.hdr.status) {
		err = mlx5_cmd_status_to_err(&out.hdr);
		if (err) {
			mlx5_core_warn(dev, "func_id 0x%x, npages %d, status %d\n",
				       func_id, npages, out.hdr.status);
			goto out_alloc;
		}
	}

	mlx5_core_dbg(dev, "err %d\n", err);

	goto out_free;

out_alloc:
	if (notify_fail) {
		nin = kzalloc(sizeof(*nin), GFP_KERNEL);
		if (!nin)
			goto out_4k;

		memset(&out, 0, sizeof(out));
		nin->hdr.opcode = cpu_to_be16(MLX5_CMD_OP_MANAGE_PAGES);
		nin->hdr.opmod = cpu_to_be16(MLX5_PAGES_CANT_GIVE);
		nin->func_id = cpu_to_be16(func_id);
		if (mlx5_cmd_exec(dev, nin, sizeof(*nin), &out, sizeof(out)))
			mlx5_core_warn(dev, "page notify failed\n");
		kfree(nin);
	}

out_4k:
	for (i--; i >= 0; i--)
		free_4k(dev, be64_to_cpu(in->pas[i]));
out_free:
	kvfree(in);
	return err;
}

static int reclaim_pages(struct mlx5_core_dev *dev, u32 func_id, int npages,
			 int *nclaimed)
{
	struct mlx5_manage_pages_inbox   in;
	struct mlx5_manage_pages_outbox *out;
	int num_claimed;
	int outlen;
	u64 addr;
	int err;
	int i;

	if (nclaimed)
		*nclaimed = 0;

	memset(&in, 0, sizeof(in));
	outlen = sizeof(*out) + npages * sizeof(out->pas[0]);
	out = mlx5_vzalloc(outlen);
	if (!out)
		return -ENOMEM;

	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_MANAGE_PAGES);
	in.hdr.opmod = cpu_to_be16(MLX5_PAGES_TAKE);
	in.func_id = cpu_to_be16(func_id);
	in.num_entries = cpu_to_be32(npages);
	mlx5_core_dbg(dev, "npages %d, outlen %d\n", npages, outlen);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), out, outlen);
	if (err) {
		mlx5_core_err(dev, "failed reclaiming pages\n");
		goto out_free;
	}

	if (out->hdr.status) {
		err = mlx5_cmd_status_to_err(&out->hdr);
		goto out_free;
	}

	num_claimed = be32_to_cpu(out->num_entries);
	if (nclaimed)
		*nclaimed = num_claimed;

	dev->priv.fw_pages -= num_claimed;
	dev->priv.pages_per_func[func_id] -= num_claimed;
	for (i = 0; i < num_claimed; i++) {
		addr = be64_to_cpu(out->pas[i]);
		free_4k(dev, addr);
	}

out_free:
	kvfree(out);
	return err;
}

static void pages_work_handler(struct work_struct *work)
{
	struct mlx5_pages_req *req = container_of(work, struct mlx5_pages_req, work);
	struct mlx5_core_dev *dev = req->dev;
	int err = 0;

	if (req->npages < 0)
		err = reclaim_pages(dev, req->func_id, -1 * req->npages, NULL);
	else if (req->npages > 0)
		err = give_pages(dev, req->func_id, req->npages, 1);

	if (err)
		mlx5_core_warn(dev, "%s fail %d\n",
			       req->npages < 0 ? "reclaim" : "give", err);

	kfree(req);
}

void mlx5_core_req_pages_handler(struct mlx5_core_dev *dev, u16 func_id,
				 s32 npages)
{
	struct mlx5_pages_req *req;

	req = kzalloc(sizeof(*req), GFP_ATOMIC);
	if (!req) {
		mlx5_core_warn(dev, "failed to allocate pages request\n");
		return;
	}

	req->dev = dev;
	req->func_id = func_id;
	req->npages = npages;
	INIT_WORK(&req->work, pages_work_handler);
	if (!queue_work(dev->priv.pg_wq, &req->work))
		mlx5_core_warn(dev, "failed to queue pages handler work\n");
}

int mlx5_satisfy_startup_pages(struct mlx5_core_dev *dev, int boot)
{
	u16 uninitialized_var(func_id);
	s32 uninitialized_var(npages);
	int err;

	err = mlx5_cmd_query_pages(dev, &func_id, &npages, boot);
	if (err)
		return err;

	mlx5_core_dbg(dev, "requested %d %s pages for func_id 0x%x\n",
		      npages, boot ? "boot" : "init", func_id);

	return give_pages(dev, func_id, npages, 0);
}

enum {
	MLX5_BLKS_FOR_RECLAIM_PAGES = 12
};

s64 mlx5_wait_for_reclaim_vfs_pages(struct mlx5_core_dev *dev)
{
	int end = jiffies + msecs_to_jiffies(MAX_RECLAIM_TIME_MSECS);
	s64 prevpages = 0;
	s64 npages = 0;

	while (!time_after(jiffies, end)) {
		/* exclude own function, VFs only */
		npages = dev->priv.fw_pages - dev->priv.pages_per_func[0];
		if (!npages)
			break;

		if (npages != prevpages)
			end = end + msecs_to_jiffies(100);

		prevpages = npages;
		msleep(1);
	}

	if (npages)
		mlx5_core_warn(dev, "FW did not return all VFs pages, will cause to memory leak\n");

	return -npages;
}

static int optimal_reclaimed_pages(void)
{
	struct mlx5_cmd_prot_block *block;
	struct mlx5_cmd_layout *lay;
	int ret;

	ret = (sizeof(lay->out) + MLX5_BLKS_FOR_RECLAIM_PAGES * sizeof(block->data) -
	       sizeof(struct mlx5_manage_pages_outbox)) /
	       FIELD_SIZEOF(struct mlx5_manage_pages_outbox, pas[0]);

	return ret;
}

int mlx5_reclaim_startup_pages(struct mlx5_core_dev *dev)
{
	int end = jiffies + msecs_to_jiffies(MAX_RECLAIM_TIME_MSECS);
	struct mlx5_fw_page *fwp;
	struct rb_node *p;
	int nclaimed = 0;
	int err;

	do {
		p = rb_first(&dev->priv.page_root);
		if (p) {
			fwp = rb_entry(p, struct mlx5_fw_page, rb_node);
			if (dev->state == MLX5_DEVICE_STATE_INTERNAL_ERROR) {
				--dev->priv.fw_pages;
				free_4k(dev, fwp->dma_addr);
				nclaimed = 1;
			} else {
				err = reclaim_pages(dev, fwp->func_id,
						    optimal_reclaimed_pages(),
						    &nclaimed);
				if (err) {
					mlx5_core_warn(dev, "failed reclaiming pages (%d)\n",
						       err);
					return err;
				}
			}

			if (nclaimed)
				end = jiffies + msecs_to_jiffies(MAX_RECLAIM_TIME_MSECS);
		}
		if (time_after(jiffies, end)) {
			mlx5_core_warn(dev, "FW did not return all pages. giving up...\n");
			break;
		}
	} while (p);

	return 0;
}

void mlx5_pagealloc_init(struct mlx5_core_dev *dev)
{

	dev->priv.page_root = RB_ROOT;
}

void mlx5_pagealloc_cleanup(struct mlx5_core_dev *dev)
{
	/* nothing */
}

int mlx5_pagealloc_start(struct mlx5_core_dev *dev)
{
	dev->priv.pg_wq = create_singlethread_workqueue("mlx5_page_allocator");
	if (!dev->priv.pg_wq)
		return -ENOMEM;

	return 0;
}

void mlx5_pagealloc_stop(struct mlx5_core_dev *dev)
{
	destroy_workqueue(dev->priv.pg_wq);
}
