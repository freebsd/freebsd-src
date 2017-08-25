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


#include <linux/kref.h>
#include <linux/random.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <rdma/ib_umem.h>
#include "mlx5_ib.h"

CTASSERT((uintptr_t)PAGE_MASK > (uintptr_t)PAGE_SIZE);

enum {
	MAX_PENDING_REG_MR = 8,
	MAX_MR_RELEASE_TIMEOUT = (60 * 20) /* Allow release timeout up to 20 min */
};

#define MLX5_UMR_ALIGN 2048

static int mlx5_mr_sysfs_init(struct mlx5_ib_dev *dev);
static void mlx5_mr_sysfs_cleanup(struct mlx5_ib_dev *dev);

static int destroy_mkey(struct mlx5_ib_dev *dev, struct mlx5_ib_mr *mr)
{
	int err = mlx5_core_destroy_mkey(dev->mdev, &mr->mmr);

	return err;
}

static int order2idx(struct mlx5_ib_dev *dev, int order)
{
	struct mlx5_mr_cache *cache = &dev->cache;

	if (order < cache->ent[0].order)
		return 0;
	else
		return order - cache->ent[0].order;
}

static void reg_mr_callback(int status, void *context)
{
	struct mlx5_ib_mr *mr = context;
	struct mlx5_ib_dev *dev = mr->dev;
	struct mlx5_mr_cache *cache = &dev->cache;
	int c = order2idx(dev, mr->order);
	struct mlx5_cache_ent *ent = &cache->ent[c];
	struct mlx5_core_dev *mdev = dev->mdev;
	struct mlx5_core_mr *mmr = &mr->mmr;
	struct mlx5_mr_table *table = &dev->mdev->priv.mr_table;
	unsigned long flags;
	int err;
	u8 key;

	spin_lock_irqsave(&ent->lock, flags);
	ent->pending--;
	spin_unlock_irqrestore(&ent->lock, flags);
	if (status) {
		mlx5_ib_warn(dev, "async reg mr failed. status %d, order %d\n", status, ent->order);
		kfree(mr);
		dev->fill_delay = 1;
		mod_timer(&dev->delay_timer, jiffies + HZ);
		return;
	}

	if (mr->out.hdr.status) {
		mlx5_ib_warn(dev, "failed - status %d, syndorme 0x%x\n",
			     mr->out.hdr.status,
			     be32_to_cpu(mr->out.hdr.syndrome));
		kfree(mr);
		dev->fill_delay = 1;
		mod_timer(&dev->delay_timer, jiffies + HZ);
		return;
	}

	spin_lock_irqsave(&dev->mdev->priv.mkey_lock, flags);
	key = dev->mdev->priv.mkey_key++;
	spin_unlock_irqrestore(&dev->mdev->priv.mkey_lock, flags);
	mmr->key = mlx5_idx_to_mkey(be32_to_cpu(mr->out.mkey) & 0xffffff) | key;
	mlx5_ib_dbg(dev, "callbacked mkey 0x%x created\n",
		    be32_to_cpu(mr->out.mkey));

	cache->last_add = jiffies;

	spin_lock_irqsave(&ent->lock, flags);
	list_add_tail(&mr->list, &ent->head);
	ent->cur++;
	ent->size++;
	spin_unlock_irqrestore(&ent->lock, flags);

	spin_lock_irqsave(&table->lock, flags);
	err = radix_tree_insert(&table->tree, mlx5_mkey_to_idx(mmr->key), mmr);
	spin_unlock_irqrestore(&table->lock, flags);
	if (err) {
		mlx5_ib_warn(dev, "failed radix tree insert of mkey 0x%x, %d\n",
			     mmr->key, err);
		mlx5_core_destroy_mkey(mdev, mmr);
	}
}

static int add_keys(struct mlx5_ib_dev *dev, int c, int num)
{
	struct mlx5_mr_cache *cache = &dev->cache;
	struct mlx5_cache_ent *ent = &cache->ent[c];
	struct mlx5_create_mkey_mbox_in *in;
	struct mlx5_ib_mr *mr;
	int npages = 1 << ent->order;
	int err = 0;
	int i;

	in = kzalloc(sizeof(*in), GFP_KERNEL);
	if (!in)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		if (ent->pending >= MAX_PENDING_REG_MR) {
			err = -EAGAIN;
			break;
		}

		mr = kzalloc(sizeof(*mr), GFP_KERNEL);
		if (!mr) {
			err = -ENOMEM;
			break;
		}
		mr->order = ent->order;
		mr->umred = 1;
		mr->dev = dev;
		in->seg.status = MLX5_MKEY_STATUS_FREE;
		in->seg.xlt_oct_size = cpu_to_be32((npages + 1) / 2);
		in->seg.qpn_mkey7_0 = cpu_to_be32(0xffffff << 8);
		in->seg.flags = MLX5_ACCESS_MODE_MTT | MLX5_PERM_UMR_EN;
		in->seg.log2_page_size = 12;

		spin_lock_irq(&ent->lock);
		ent->pending++;
		spin_unlock_irq(&ent->lock);
		err = mlx5_core_create_mkey(dev->mdev, &mr->mmr, in,
					    sizeof(*in), reg_mr_callback,
					    mr, &mr->out);
		if (err) {
			spin_lock_irq(&ent->lock);
			ent->pending--;
			spin_unlock_irq(&ent->lock);
			mlx5_ib_warn(dev, "create mkey failed %d\n", err);
			kfree(mr);
			break;
		}
	}

	kfree(in);
	return err;
}

static void remove_keys(struct mlx5_ib_dev *dev, int c, int num)
{
	struct mlx5_mr_cache *cache = &dev->cache;
	struct mlx5_cache_ent *ent = &cache->ent[c];
	struct mlx5_ib_mr *mr;
	int err;
	int i;

	for (i = 0; i < num; i++) {
		spin_lock_irq(&ent->lock);
		if (list_empty(&ent->head)) {
			spin_unlock_irq(&ent->lock);
			return;
		}
		mr = list_first_entry(&ent->head, struct mlx5_ib_mr, list);
		list_del(&mr->list);
		ent->cur--;
		ent->size--;
		spin_unlock_irq(&ent->lock);
		err = destroy_mkey(dev, mr);
		if (err)
			mlx5_ib_warn(dev, "failed destroy mkey\n");
		else
			kfree(mr);
	}
}

static int someone_adding(struct mlx5_mr_cache *cache)
{
	int i;

	for (i = 0; i < MAX_MR_CACHE_ENTRIES; i++) {
		if (cache->ent[i].cur < cache->ent[i].limit)
			return 1;
	}

	return 0;
}

static int someone_releasing(struct mlx5_mr_cache *cache)
{
	int i;

	for (i = 0; i < MAX_MR_CACHE_ENTRIES; i++) {
		if (cache->ent[i].cur > 2 * cache->ent[i].limit)
			return 1;
	}

	return 0;
}

static void __cache_work_func(struct mlx5_cache_ent *ent)
{
	struct mlx5_ib_dev *dev = ent->dev;
	struct mlx5_mr_cache *cache = &dev->cache;
	int i = order2idx(dev, ent->order);
	int err;
	s64 dtime;

	if (cache->stopped)
		return;

	ent = &dev->cache.ent[i];
	if (ent->cur < 2 * ent->limit && !dev->fill_delay) {
		err = add_keys(dev, i, 1);
		if (ent->cur < 2 * ent->limit) {
			if (err == -EAGAIN) {
				mlx5_ib_dbg(dev, "returned eagain, order %d\n",
					    i + 2);
				cancel_delayed_work(&ent->dwork);
				if (!queue_delayed_work(cache->wq, &ent->dwork,
							msecs_to_jiffies(3)))
					mlx5_ib_warn(dev, "failed queueing delayed work\n");
			} else if (err) {
				mlx5_ib_warn(dev, "command failed order %d, err %d\n",
					     i + 2, err);
				cancel_delayed_work(&ent->dwork);
				if (!queue_delayed_work(cache->wq, &ent->dwork,
							msecs_to_jiffies(1000)))
					mlx5_ib_warn(dev, "failed queueing delayed work\n");
			} else {
				if (!queue_work(cache->wq, &ent->work))
					mlx5_ib_warn(dev, "failed queueing work\n");
			}
		}
	} else if (ent->cur > 2 * ent->limit) {
		dtime = (cache->last_add + (s64)cache->rel_timeout * HZ) - jiffies;
		if (cache->rel_imm ||
		    (cache->rel_timeout >= 0 && !someone_adding(cache) && dtime <= 0)) {
			remove_keys(dev, i, 1);
			if (ent->cur > ent->limit)
				if (!queue_work(cache->wq, &ent->work))
					mlx5_ib_warn(dev, "failed queueing work\n");
		} else if (cache->rel_timeout >= 0) {
			dtime = max_t(s64, dtime, 0);
			dtime = min_t(s64, dtime, (MAX_MR_RELEASE_TIMEOUT * HZ));
			cancel_delayed_work(&ent->dwork);
			if (!queue_delayed_work(cache->wq, &ent->dwork, dtime))
				mlx5_ib_warn(dev, "failed queueing delayed work\n");
		}
	} else if (cache->rel_imm && !someone_releasing(cache)) {
		cache->rel_imm = 0;
	}
}

static void delayed_cache_work_func(struct work_struct *work)
{
	struct mlx5_cache_ent *ent;

	ent = container_of(work, struct mlx5_cache_ent, dwork.work);
	__cache_work_func(ent);
}

static void cache_work_func(struct work_struct *work)
{
	struct mlx5_cache_ent *ent;

	ent = container_of(work, struct mlx5_cache_ent, work);
	__cache_work_func(ent);
}

static void free_cached_mr(struct mlx5_ib_dev *dev, struct mlx5_ib_mr *mr)
{
	struct mlx5_mr_cache *cache = &dev->cache;
	struct mlx5_cache_ent *ent;
	int shrink = 0;
	int c;

	c = order2idx(dev, mr->order);
	if (c < 0 || c >= MAX_MR_CACHE_ENTRIES) {
		mlx5_ib_warn(dev, "order %d, cache index %d\n", mr->order, c);
		return;
	}
	ent = &cache->ent[c];
	spin_lock_irq(&ent->lock);
	list_add_tail(&mr->list, &ent->head);
	ent->cur++;
	if (ent->cur > 2 * ent->limit)
		shrink = 1;
	spin_unlock_irq(&ent->lock);

	if (shrink)
		if (!queue_work(cache->wq, &ent->work))
			mlx5_ib_warn(dev, "failed queueing work\n");
}

static void clean_keys(struct mlx5_ib_dev *dev, int c)
{
	struct mlx5_mr_cache *cache = &dev->cache;
	struct mlx5_cache_ent *ent = &cache->ent[c];
	struct mlx5_ib_mr *mr;
	int err;

	cancel_delayed_work(&ent->dwork);
	while (1) {
		spin_lock_irq(&ent->lock);
		if (list_empty(&ent->head)) {
			spin_unlock_irq(&ent->lock);
			return;
		}
		mr = list_first_entry(&ent->head, struct mlx5_ib_mr, list);
		list_del(&mr->list);
		ent->cur--;
		ent->size--;
		spin_unlock_irq(&ent->lock);
		err = destroy_mkey(dev, mr);
		if (err)
			mlx5_ib_warn(dev, "failed destroy mkey 0x%x from order %d\n",
				     mr->mmr.key, ent->order);
		else
			kfree(mr);
	}
}

static void delay_time_func(unsigned long ctx)
{
	struct mlx5_ib_dev *dev = (struct mlx5_ib_dev *)ctx;

	dev->fill_delay = 0;
}

enum {
	MLX5_VF_MR_LIMIT	= 2,
};

int mlx5_mr_cache_init(struct mlx5_ib_dev *dev)
{
	struct mlx5_mr_cache *cache = &dev->cache;
	struct mlx5_cache_ent *ent;
	int limit;
	int err;
	int i;

	mutex_init(&dev->slow_path_mutex);
	cache->rel_timeout = 300;
	cache->wq = create_singlethread_workqueue("mkey_cache");
	if (!cache->wq) {
		mlx5_ib_warn(dev, "failed to create work queue\n");
		return -ENOMEM;
	}

	setup_timer(&dev->delay_timer, delay_time_func, (uintptr_t)dev);
	for (i = 0; i < MAX_MR_CACHE_ENTRIES; i++) {
		INIT_LIST_HEAD(&cache->ent[i].head);
		spin_lock_init(&cache->ent[i].lock);

		ent = &cache->ent[i];
		INIT_LIST_HEAD(&ent->head);
		spin_lock_init(&ent->lock);
		ent->order = i + 2;
		ent->dev = dev;

		if (dev->mdev->profile->mask & MLX5_PROF_MASK_MR_CACHE) {
			if (mlx5_core_is_pf(dev->mdev))
				limit = dev->mdev->profile->mr_cache[i].limit;
			else
				limit = MLX5_VF_MR_LIMIT;
		} else {
			limit = 0;
		}

		INIT_WORK(&ent->work, cache_work_func);
		INIT_DELAYED_WORK(&ent->dwork, delayed_cache_work_func);
		ent->limit = limit;
		if (!queue_work(cache->wq, &ent->work))
			mlx5_ib_warn(dev, "failed queueing work\n");
	}

	err = mlx5_mr_sysfs_init(dev);
	if (err)
		mlx5_ib_warn(dev, "failed to init mr cache sysfs\n");

	return 0;
}

static void wait_for_async_commands(struct mlx5_ib_dev *dev)
{
	struct mlx5_mr_cache *cache = &dev->cache;
	struct mlx5_cache_ent *ent;
	int total = 0;
	int i;
	int j;

	for (i = 0; i < MAX_MR_CACHE_ENTRIES; i++) {
		ent = &cache->ent[i];
		for (j = 0 ; j < 1000; j++) {
			if (!ent->pending)
				break;
			msleep(50);
		}
	}
	for (i = 0; i < MAX_MR_CACHE_ENTRIES; i++) {
		ent = &cache->ent[i];
		total += ent->pending;
	}

	if (total)
		mlx5_ib_dbg(dev, "aborted, %d pending requests\n", total);
	else
		mlx5_ib_dbg(dev, "done with all pending requests\n");
}

int mlx5_mr_cache_cleanup(struct mlx5_ib_dev *dev)
{
	int i;

	dev->cache.stopped = 1;
	flush_workqueue(dev->cache.wq);
	mlx5_mr_sysfs_cleanup(dev);

	for (i = 0; i < MAX_MR_CACHE_ENTRIES; i++)
		clean_keys(dev, i);

	destroy_workqueue(dev->cache.wq);
	wait_for_async_commands(dev);
	del_timer_sync(&dev->delay_timer);
	return 0;
}

struct ib_mr *mlx5_ib_get_dma_mr(struct ib_pd *pd, int acc)
{
	struct mlx5_ib_dev *dev = to_mdev(pd->device);
	struct mlx5_core_dev *mdev = dev->mdev;
	struct mlx5_create_mkey_mbox_in *in;
	struct mlx5_mkey_seg *seg;
	struct mlx5_ib_mr *mr;
	int err;

	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr)
		return ERR_PTR(-ENOMEM);

	in = kzalloc(sizeof(*in), GFP_KERNEL);
	if (!in) {
		err = -ENOMEM;
		goto err_free;
	}

	seg = &in->seg;
	seg->flags = convert_access(acc) | MLX5_ACCESS_MODE_PA;
	seg->flags_pd = cpu_to_be32(to_mpd(pd)->pdn | MLX5_MKEY_LEN64);
	seg->qpn_mkey7_0 = cpu_to_be32(0xffffff << 8);
	seg->start_addr = 0;

	err = mlx5_core_create_mkey(mdev, &mr->mmr, in, sizeof(*in), NULL, NULL,
				    NULL);
	if (err)
		goto err_in;

	kfree(in);
	mr->ibmr.lkey = mr->mmr.key;
	mr->ibmr.rkey = mr->mmr.key;
	mr->umem = NULL;

	return &mr->ibmr;

err_in:
	kfree(in);

err_free:
	kfree(mr);

	return ERR_PTR(err);
}

static int get_octo_len(u64 addr, u64 len, u64 page_size)
{
	u64 offset;
	int npages;

	offset = addr & (page_size - 1ULL);
	npages = ALIGN(len + offset, page_size) >> ilog2(page_size);
	return (npages + 1) / 2;
}

void mlx5_umr_cq_handler(struct ib_cq *cq, void *cq_context)
{
	struct mlx5_ib_umr_context *context;
	struct ib_wc wc;
	int err;

	while (1) {
		err = ib_poll_cq(cq, 1, &wc);
		if (err < 0) {
			printf("mlx5_ib: WARN: ""poll cq error %d\n", err);
			return;
		}
		if (err == 0)
			break;

		context = (struct mlx5_ib_umr_context *)(uintptr_t)wc.wr_id;
		context->status = wc.status;
		complete(&context->done);
	}
	ib_req_notify_cq(cq, IB_CQ_NEXT_COMP);
}

static struct mlx5_ib_mr *reg_create(struct ib_pd *pd, u64 virt_addr,
				     u64 length, struct ib_umem *umem,
				     int npages, int page_shift,
				     int access_flags)
{
	struct mlx5_ib_dev *dev = to_mdev(pd->device);
	struct mlx5_create_mkey_mbox_in *in;
	struct mlx5_ib_mr *mr;
	int inlen;
	int err;
	bool pg_cap = !!(MLX5_CAP_GEN(dev->mdev, pg));

	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr)
		return ERR_PTR(-ENOMEM);

	inlen = sizeof(*in) + sizeof(*in->pas) * ((npages + 1) / 2) * 2;
	in = mlx5_vzalloc(inlen);
	if (!in) {
		err = -ENOMEM;
		goto err_1;
	}
	mlx5_ib_populate_pas(dev, umem, page_shift, in->pas,
			     pg_cap ? MLX5_IB_MTT_PRESENT : 0);

	/* The MLX5_MKEY_INBOX_PG_ACCESS bit allows setting the access flags
	 * in the page list submitted with the command. */
	in->flags = pg_cap ? cpu_to_be32(MLX5_MKEY_INBOX_PG_ACCESS) : 0;
	in->seg.flags = convert_access(access_flags) |
		MLX5_ACCESS_MODE_MTT;
	in->seg.flags_pd = cpu_to_be32(to_mpd(pd)->pdn);
	in->seg.start_addr = cpu_to_be64(virt_addr);
	in->seg.len = cpu_to_be64(length);
	in->seg.bsfs_octo_size = 0;
	in->seg.xlt_oct_size = cpu_to_be32(get_octo_len(virt_addr, length, 1 << page_shift));
	in->seg.log2_page_size = page_shift;
	in->seg.qpn_mkey7_0 = cpu_to_be32(0xffffff << 8);
	in->xlat_oct_act_size = cpu_to_be32(get_octo_len(virt_addr, length,
							 1 << page_shift));
	err = mlx5_core_create_mkey(dev->mdev, &mr->mmr, in, inlen, NULL,
				    NULL, NULL);
	if (err) {
		mlx5_ib_warn(dev, "create mkey failed\n");
		goto err_2;
	}
	mr->umem = umem;
	mr->dev = dev;
	kvfree(in);

	mlx5_ib_dbg(dev, "mkey = 0x%x\n", mr->mmr.key);

	return mr;

err_2:
	kvfree(in);

err_1:
	kfree(mr);

	return ERR_PTR(err);
}

enum {
	MLX5_MAX_REG_ORDER = MAX_MR_CACHE_ENTRIES + 1,
	MLX5_MAX_REG_SIZE = 2ul * 1024 * 1024 * 1024,
};

static int clean_mr(struct mlx5_ib_mr *mr)
{
	struct mlx5_ib_dev *dev = to_mdev(mr->ibmr.device);
	int umred = mr->umred;
	int err;
	int i;

	if (!umred) {
		for (i = 0; i < mr->nchild; ++i) {
			free_cached_mr(dev, mr->children[i]);
		}
		kfree(mr->children);

		err = destroy_mkey(dev, mr);
		if (err) {
			mlx5_ib_warn(dev, "failed to destroy mkey 0x%x (%d)\n",
				     mr->mmr.key, err);
			return err;
		}
	}
	return 0;
}

struct ib_mr *mlx5_ib_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
				  u64 virt_addr, int access_flags,
				  struct ib_udata *udata, int mr_id)
{
	struct mlx5_ib_dev *dev = to_mdev(pd->device);
	struct mlx5_ib_mr *mr = NULL;
	struct ib_umem *umem;
	int page_shift;
	int npages;
	int ncont;
	int order;
	int err;

	mlx5_ib_dbg(dev, "start 0x%llx, virt_addr 0x%llx, length 0x%llx, access_flags 0x%x\n",
		    (unsigned long long)start, (unsigned long long)virt_addr,
		    (unsigned long long)length, access_flags);
	umem = ib_umem_get(pd->uobject->context, start, length, access_flags, 0);
	if (IS_ERR(umem)) {
		mlx5_ib_warn(dev, "umem get failed (%ld)\n", PTR_ERR(umem));
		return (void *)umem;
	}

	mlx5_ib_cont_pages(umem, start, &npages, &page_shift, &ncont, &order);
	if (!npages) {
		mlx5_ib_warn(dev, "avoid zero region\n");
		err = -EINVAL;
		goto error;
	}

	mlx5_ib_dbg(dev, "npages %d, ncont %d, order %d, page_shift %d\n",
		    npages, ncont, order, page_shift);

	mutex_lock(&dev->slow_path_mutex);
	mr = reg_create(pd, virt_addr, length, umem, ncont, page_shift, access_flags);
	mutex_unlock(&dev->slow_path_mutex);

	if (IS_ERR(mr)) {
		err = PTR_ERR(mr);
		mr = NULL;
		goto error;
	}

	mlx5_ib_dbg(dev, "mkey 0x%x\n", mr->mmr.key);

	mr->umem = umem;
	mr->npages = npages;
	atomic_add(npages, &dev->mdev->priv.reg_pages);
	mr->ibmr.lkey = mr->mmr.key;
	mr->ibmr.rkey = mr->mmr.key;

	return &mr->ibmr;

error:
	/*
	 * Destroy the umem *before* destroying the MR, to ensure we
	 * will not have any in-flight notifiers when destroying the
	 * MR.
	 *
	 * As the MR is completely invalid to begin with, and this
	 * error path is only taken if we can't push the mr entry into
	 * the pagefault tree, this is safe.
	 */

	ib_umem_release(umem);
	return ERR_PTR(err);
}

CTASSERT(sizeof(((struct ib_phys_buf *)0)->size) == 8);

struct ib_mr *
mlx5_ib_reg_phys_mr(struct ib_pd *pd,
		    struct ib_phys_buf *buffer_list,
		    int num_phys_buf,
		    int access_flags,
		    u64 *virt_addr)
{
	struct mlx5_ib_dev *dev = to_mdev(pd->device);
	struct mlx5_create_mkey_mbox_in *in;
	struct mlx5_ib_mr *mr;
	u64 total_size;
	u32 octo_len;
	bool pg_cap = !!(MLX5_CAP_GEN(dev->mdev, pg));
	unsigned long mask;
	int shift;
	int npages;
	int inlen;
	int err;
	int i, j, n;

	mask = buffer_list[0].addr ^ *virt_addr;
	total_size = 0;
	for (i = 0; i < num_phys_buf; ++i) {
		if (i != 0)
			mask |= buffer_list[i].addr;
		if (i != num_phys_buf - 1)
			mask |= buffer_list[i].addr + buffer_list[i].size;

		total_size += buffer_list[i].size;
	}

	if (mask & ~PAGE_MASK)
		return ERR_PTR(-EINVAL);

	shift = __ffs(mask | 1 << 31);

	buffer_list[0].size += buffer_list[0].addr & ((1ULL << shift) - 1);
	buffer_list[0].addr &= ~0ULL << shift;

	npages = 0;
	for (i = 0; i < num_phys_buf; ++i)
		npages += (buffer_list[i].size + (1ULL << shift) - 1) >> shift;

	if (!npages) {
		mlx5_ib_warn(dev, "avoid zero region\n");
		return ERR_PTR(-EINVAL);
	}

	mr = kzalloc(sizeof *mr, GFP_KERNEL);
	if (!mr)
		return ERR_PTR(-ENOMEM);

	octo_len = get_octo_len(*virt_addr, total_size, 1ULL << shift);
	octo_len = ALIGN(octo_len, 4);

	inlen = sizeof(*in) + (octo_len * 16);
	in = mlx5_vzalloc(inlen);
	if (!in) {
		kfree(mr);
		return ERR_PTR(-ENOMEM);
	}

	n = 0;
	for (i = 0; i < num_phys_buf; ++i) {
		for (j = 0;
		     j < (buffer_list[i].size + (1ULL << shift) - 1) >> shift;
		     ++j) {
			u64 temp = buffer_list[i].addr + ((u64) j << shift);
			if (pg_cap)
				temp |= MLX5_IB_MTT_PRESENT;
			in->pas[n++] = cpu_to_be64(temp);
		}
	}

	/* The MLX5_MKEY_INBOX_PG_ACCESS bit allows setting the access flags
	 * in the page list submitted with the command. */
	in->flags = pg_cap ? cpu_to_be32(MLX5_MKEY_INBOX_PG_ACCESS) : 0;
	in->seg.flags = convert_access(access_flags) |
		MLX5_ACCESS_MODE_MTT;
	in->seg.flags_pd = cpu_to_be32(to_mpd(pd)->pdn);
	in->seg.start_addr = cpu_to_be64(*virt_addr);
	in->seg.len = cpu_to_be64(total_size);
	in->seg.bsfs_octo_size = 0;
	in->seg.xlt_oct_size = cpu_to_be32(octo_len);
	in->seg.log2_page_size = shift;
	in->seg.qpn_mkey7_0 = cpu_to_be32(0xffffff << 8);
	in->xlat_oct_act_size = cpu_to_be32(octo_len);
	err = mlx5_core_create_mkey(dev->mdev, &mr->mmr, in, inlen, NULL,
				    NULL, NULL);
	mr->umem = NULL;
	mr->dev = dev;
	mr->npages = npages;
	mr->ibmr.lkey = mr->mmr.key;
	mr->ibmr.rkey = mr->mmr.key;

	kvfree(in);

	if (err) {
		kfree(mr);
		return ERR_PTR(err);
	}
	return &mr->ibmr;
}

int mlx5_ib_dereg_mr(struct ib_mr *ibmr)
{
	struct mlx5_ib_dev *dev = to_mdev(ibmr->device);
	struct mlx5_ib_mr *mr = to_mmr(ibmr);
	struct ib_umem *umem = mr->umem;
	int npages = mr->npages;
	int umred = mr->umred;
	int err;

	err = clean_mr(mr);
	if (err)
		return err;

	if (umem) {
		ib_umem_release(umem);
		atomic_sub(npages, &dev->mdev->priv.reg_pages);
	}

	if (umred)
		free_cached_mr(dev, mr);
	else
		kfree(mr);

	return 0;
}

int mlx5_ib_destroy_mr(struct ib_mr *ibmr)
{
	struct mlx5_ib_dev *dev = to_mdev(ibmr->device);
	struct mlx5_ib_mr *mr = to_mmr(ibmr);
	int err;

	if (mr->sig) {
		if (mlx5_core_destroy_psv(dev->mdev,
					  mr->sig->psv_memory.psv_idx))
			mlx5_ib_warn(dev, "failed to destroy mem psv %d\n",
				     mr->sig->psv_memory.psv_idx);
		if (mlx5_core_destroy_psv(dev->mdev,
					  mr->sig->psv_wire.psv_idx))
			mlx5_ib_warn(dev, "failed to destroy wire psv %d\n",
				     mr->sig->psv_wire.psv_idx);
		kfree(mr->sig);
	}

	err = destroy_mkey(dev, mr);
	if (err) {
		mlx5_ib_warn(dev, "failed to destroy mkey 0x%x (%d)\n",
			     mr->mmr.key, err);
		return err;
	}

	kfree(mr);

	return err;
}

struct ib_mr *mlx5_ib_alloc_fast_reg_mr(struct ib_pd *pd,
					int max_page_list_len)
{
	struct mlx5_ib_dev *dev = to_mdev(pd->device);
	struct mlx5_create_mkey_mbox_in *in;
	struct mlx5_ib_mr *mr;
	int err;

	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr)
		return ERR_PTR(-ENOMEM);

	in = kzalloc(sizeof(*in), GFP_KERNEL);
	if (!in) {
		err = -ENOMEM;
		goto err_free;
	}

	in->seg.status = MLX5_MKEY_STATUS_FREE;
	in->seg.xlt_oct_size = cpu_to_be32((max_page_list_len + 1) / 2);
	in->seg.qpn_mkey7_0 = cpu_to_be32(0xffffff << 8);
	in->seg.flags = MLX5_PERM_UMR_EN | MLX5_ACCESS_MODE_MTT;
	in->seg.flags_pd = cpu_to_be32(to_mpd(pd)->pdn);
	/*
	 * TBD not needed - issue 197292 */
	in->seg.log2_page_size = PAGE_SHIFT;

	err = mlx5_core_create_mkey(dev->mdev, &mr->mmr, in, sizeof(*in), NULL,
				    NULL, NULL);
	kfree(in);
	if (err) {
		mlx5_ib_warn(dev, "failed create mkey\n");
		goto err_free;
	}

	mr->ibmr.lkey = mr->mmr.key;
	mr->ibmr.rkey = mr->mmr.key;
	mr->umem = NULL;

	return &mr->ibmr;

err_free:
	kfree(mr);
	return ERR_PTR(err);
}

struct ib_fast_reg_page_list *mlx5_ib_alloc_fast_reg_page_list(struct ib_device *ibdev,
							       int page_list_len)
{
	struct mlx5_ib_fast_reg_page_list *mfrpl;
	int size = page_list_len * sizeof(u64);

	mfrpl = kmalloc(sizeof(*mfrpl), GFP_KERNEL);
	if (!mfrpl)
		return ERR_PTR(-ENOMEM);

	mfrpl->ibfrpl.page_list = kmalloc(size, GFP_KERNEL);
	if (!mfrpl->ibfrpl.page_list)
		goto err_free;

	mfrpl->mapped_page_list = dma_alloc_coherent(ibdev->dma_device,
						     size, &mfrpl->map,
						     GFP_KERNEL);
	if (!mfrpl->mapped_page_list)
		goto err_free;

	WARN_ON(mfrpl->map & 0x3f);

	return &mfrpl->ibfrpl;

err_free:
	kfree(mfrpl->ibfrpl.page_list);
	kfree(mfrpl);
	return ERR_PTR(-ENOMEM);
}

void mlx5_ib_free_fast_reg_page_list(struct ib_fast_reg_page_list *page_list)
{
	struct mlx5_ib_fast_reg_page_list *mfrpl = to_mfrpl(page_list);
	struct mlx5_ib_dev *dev = to_mdev(page_list->device);
	int size = page_list->max_page_list_len * sizeof(u64);

	dma_free_coherent(&dev->mdev->pdev->dev, size, mfrpl->mapped_page_list,
			  mfrpl->map);
	kfree(mfrpl->ibfrpl.page_list);
	kfree(mfrpl);
}

struct order_attribute {
	struct attribute attr;
	ssize_t (*show)(struct cache_order *, struct order_attribute *, char *buf);
	ssize_t (*store)(struct cache_order *, struct order_attribute *,
			 const char *buf, size_t count);
};

static ssize_t cur_show(struct cache_order *co, struct order_attribute *oa,
			char *buf)
{
	struct mlx5_ib_dev *dev = co->dev;
	struct mlx5_mr_cache *cache = &dev->cache;
	struct mlx5_cache_ent *ent = &cache->ent[co->index];
	int err;

	err = snprintf(buf, 20, "%d\n", ent->cur);
	return err;
}

static ssize_t limit_show(struct cache_order *co, struct order_attribute *oa,
			  char *buf)
{
	struct mlx5_ib_dev *dev = co->dev;
	struct mlx5_mr_cache *cache = &dev->cache;
	struct mlx5_cache_ent *ent = &cache->ent[co->index];
	int err;

	err = snprintf(buf, 20, "%d\n", ent->limit);
	return err;
}

static ssize_t limit_store(struct cache_order *co, struct order_attribute *oa,
			   const char *buf, size_t count)
{
	struct mlx5_ib_dev *dev = co->dev;
	struct mlx5_mr_cache *cache = &dev->cache;
	struct mlx5_cache_ent *ent = &cache->ent[co->index];
	u32 var;
	int err;

#define	kstrtouint(a,b,c) ({*(c) = strtol(a,0,b); 0;})
#define	kstrtoint(a,b,c) ({*(c) = strtol(a,0,b); 0;})

	if (kstrtouint(buf, 0, &var))
		return -EINVAL;

	if (var > ent->size)
		return -EINVAL;

	ent->limit = var;

	if (ent->cur < ent->limit) {
		err = add_keys(dev, co->index, 2 * ent->limit - ent->cur);
		if (err)
			return err;
	}

	return count;
}

static ssize_t miss_show(struct cache_order *co, struct order_attribute *oa,
			 char *buf)
{
	struct mlx5_ib_dev *dev = co->dev;
	struct mlx5_mr_cache *cache = &dev->cache;
	struct mlx5_cache_ent *ent = &cache->ent[co->index];
	int err;

	err = snprintf(buf, 20, "%d\n", ent->miss);
	return err;
}

static ssize_t miss_store(struct cache_order *co, struct order_attribute *oa,
			  const char *buf, size_t count)
{
	struct mlx5_ib_dev *dev = co->dev;
	struct mlx5_mr_cache *cache = &dev->cache;
	struct mlx5_cache_ent *ent = &cache->ent[co->index];
	u32 var;

	if (kstrtouint(buf, 0, &var))
		return -EINVAL;

	if (var != 0)
		return -EINVAL;

	ent->miss = var;

	return count;
}

static ssize_t size_show(struct cache_order *co, struct order_attribute *oa,
			 char *buf)
{
	struct mlx5_ib_dev *dev = co->dev;
	struct mlx5_mr_cache *cache = &dev->cache;
	struct mlx5_cache_ent *ent = &cache->ent[co->index];
	int err;

	err = snprintf(buf, 20, "%d\n", ent->size);
	return err;
}

static ssize_t size_store(struct cache_order *co, struct order_attribute *oa,
			  const char *buf, size_t count)
{
	struct mlx5_ib_dev *dev = co->dev;
	struct mlx5_mr_cache *cache = &dev->cache;
	struct mlx5_cache_ent *ent = &cache->ent[co->index];
	u32 var;
	int err;

	if (kstrtouint(buf, 0, &var))
		return -EINVAL;

	if (var < ent->limit)
		return -EINVAL;

	if (var > ent->size) {
		do {
			err = add_keys(dev, co->index, var - ent->size);
			if (err && err != -EAGAIN)
				return err;

			usleep_range(3000, 5000);
		} while (err);
	} else if (var < ent->size) {
		remove_keys(dev, co->index, ent->size - var);
	}

	return count;
}

static ssize_t order_attr_show(struct kobject *kobj,
			       struct attribute *attr, char *buf)
{
	struct order_attribute *oa =
		container_of(attr, struct order_attribute, attr);
	struct cache_order *co = container_of(kobj, struct cache_order, kobj);

	if (!oa->show)
		return -EIO;

	return oa->show(co, oa, buf);
}

static ssize_t order_attr_store(struct kobject *kobj,
				struct attribute *attr, const char *buf, size_t size)
{
	struct order_attribute *oa =
		container_of(attr, struct order_attribute, attr);
	struct cache_order *co = container_of(kobj, struct cache_order, kobj);

	if (!oa->store)
		return -EIO;

	return oa->store(co, oa, buf, size);
}

static const struct sysfs_ops order_sysfs_ops = {
	.show = order_attr_show,
	.store = order_attr_store,
};

#define ORDER_ATTR(_name) struct order_attribute order_attr_##_name = \
	__ATTR(_name, 0644, _name##_show, _name##_store)
#define ORDER_ATTR_RO(_name) struct order_attribute order_attr_##_name = \
	__ATTR(_name, 0444, _name##_show, NULL)

static ORDER_ATTR_RO(cur);
static ORDER_ATTR(limit);
static ORDER_ATTR(miss);
static ORDER_ATTR(size);

static struct attribute *order_default_attrs[] = {
	&order_attr_cur.attr,
	&order_attr_limit.attr,
	&order_attr_miss.attr,
	&order_attr_size.attr,
	NULL
};

static struct kobj_type order_type = {
	.sysfs_ops     = &order_sysfs_ops,
	.default_attrs = order_default_attrs
};



struct cache_attribute {
	struct attribute attr;
	ssize_t (*show)(struct mlx5_ib_dev *dev, char *buf);
	ssize_t (*store)(struct mlx5_ib_dev *dev, const char *buf, size_t count);
};

static ssize_t rel_imm_show(struct mlx5_ib_dev *dev, char *buf)
{
	struct mlx5_mr_cache *cache = &dev->cache;
	int err;

	err = snprintf(buf, 20, "%d\n", cache->rel_imm);
	return err;
}

static ssize_t rel_imm_store(struct mlx5_ib_dev *dev, const char *buf, size_t count)
{
	struct mlx5_mr_cache *cache = &dev->cache;
	u32 var;
	int i;
	int found = 0;

	if (kstrtouint(buf, 0, &var))
		return -EINVAL;

	if (var > 1)
		return -EINVAL;

	if (var == cache->rel_imm)
		return count;

	cache->rel_imm = var;
	if (cache->rel_imm == 1) {
		for (i = 0; i < MAX_MR_CACHE_ENTRIES; i++) {
			if (cache->ent[i].cur > 2 * cache->ent[i].limit) {
				queue_work(cache->wq, &cache->ent[i].work);
				found = 1;
			}
		}
		if (!found)
			cache->rel_imm = 0;
	}

	return count;
}
static ssize_t rel_timeout_show(struct mlx5_ib_dev *dev, char *buf)
{
	struct mlx5_mr_cache *cache = &dev->cache;
	int err;

	err = snprintf(buf, 20, "%d\n", cache->rel_timeout);
	return err;
}

static ssize_t rel_timeout_store(struct mlx5_ib_dev *dev, const char *buf, size_t count)
{
	struct mlx5_mr_cache *cache = &dev->cache;
	int var;
	int i;

	if (kstrtoint(buf, 0, &var))
		return -EINVAL;

	if (var < -1 || var > MAX_MR_RELEASE_TIMEOUT)
		return -EINVAL;

	if (var == cache->rel_timeout)
		return count;

	if (cache->rel_timeout == -1 || (var < cache->rel_timeout && var != -1)) {
		cache->rel_timeout = var;
		for (i = 0; i < MAX_MR_CACHE_ENTRIES; i++) {
			if (cache->ent[i].cur > 2 * cache->ent[i].limit)
				queue_work(cache->wq, &cache->ent[i].work);
		}
	} else {
		cache->rel_timeout = var;
	}

	return count;
}

static ssize_t cache_attr_show(struct kobject *kobj,
			       struct attribute *attr, char *buf)
{
	struct cache_attribute *ca =
		container_of(attr, struct cache_attribute, attr);
	struct mlx5_ib_dev *dev = container_of(kobj, struct mlx5_ib_dev, mr_cache);

	if (!ca->show)
		return -EIO;

	return ca->show(dev, buf);
}

static ssize_t cache_attr_store(struct kobject *kobj,
				struct attribute *attr, const char *buf, size_t size)
{
	struct cache_attribute *ca =
		container_of(attr, struct cache_attribute, attr);
	struct mlx5_ib_dev *dev = container_of(kobj, struct mlx5_ib_dev, mr_cache);

	if (!ca->store)
		return -EIO;

	return ca->store(dev, buf, size);
}

static const struct sysfs_ops cache_sysfs_ops = {
	.show = cache_attr_show,
	.store = cache_attr_store,
};

#define CACHE_ATTR(_name) struct cache_attribute cache_attr_##_name = \
	__ATTR(_name, 0644, _name##_show, _name##_store)

static CACHE_ATTR(rel_imm);
static CACHE_ATTR(rel_timeout);

static struct attribute *cache_default_attrs[] = {
	&cache_attr_rel_imm.attr,
	&cache_attr_rel_timeout.attr,
	NULL
};

static struct kobj_type cache_type = {
	.sysfs_ops     = &cache_sysfs_ops,
	.default_attrs = cache_default_attrs
};

static int mlx5_mr_sysfs_init(struct mlx5_ib_dev *dev)
{
	struct mlx5_mr_cache *cache = &dev->cache;
	struct device *device = &dev->ib_dev.dev;
	struct cache_order *co;
	int o;
	int i;
	int err;

	err = kobject_init_and_add(&dev->mr_cache, &cache_type,
				   &device->kobj, "mr_cache");
	if (err)
		return -ENOMEM;

	for (o = 2, i = 0; i < MAX_MR_CACHE_ENTRIES; o++, i++) {
		co = &cache->ent[i].co;
		co->order = o;
		co->index = i;
		co->dev = dev;
		err = kobject_init_and_add(&co->kobj, &order_type,
					   &dev->mr_cache, "%d", o);
		if (err)
			goto err_put;
	}

	return 0;

err_put:
	for (; i >= 0; i--) {
		co = &cache->ent[i].co;
		kobject_put(&co->kobj);
	}
	kobject_put(&dev->mr_cache);

	return err;
}

static void mlx5_mr_sysfs_cleanup(struct mlx5_ib_dev *dev)
{
	struct mlx5_mr_cache *cache = &dev->cache;
	struct cache_order *co;
	int i;

	for (i = MAX_MR_CACHE_ENTRIES - 1; i >= 0; i--) {
		co = &cache->ent[i].co;
		kobject_put(&co->kobj);
	}
	kobject_put(&dev->mr_cache);
}
