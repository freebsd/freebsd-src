/*
 * Copyright (c) 2007 Cisco Systems, Inc. All rights reserved.
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

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/sched.h>

#ifdef __linux__
#include <linux/proc_fs.h>
#include <linux/cred.h>
#endif

#include "mlx4_ib.h"

static u32 convert_access(int acc)
{
	return (acc & IB_ACCESS_REMOTE_ATOMIC ? MLX4_PERM_ATOMIC       : 0) |
	       (acc & IB_ACCESS_REMOTE_WRITE  ? MLX4_PERM_REMOTE_WRITE : 0) |
	       (acc & IB_ACCESS_REMOTE_READ   ? MLX4_PERM_REMOTE_READ  : 0) |
	       (acc & IB_ACCESS_LOCAL_WRITE   ? MLX4_PERM_LOCAL_WRITE  : 0) |
	       MLX4_PERM_LOCAL_READ;
}
#ifdef __linux__
static ssize_t shared_mr_proc_read(struct file *file,
			  char __user *buffer,
			  size_t len,
			  loff_t *offset)
{

	return -ENOSYS;

}

static ssize_t shared_mr_proc_write(struct file *file,
			   const char __user *buffer,
			   size_t len,
			   loff_t *offset)
{

	return -ENOSYS;
}

static int shared_mr_mmap(struct file *filep, struct vm_area_struct *vma)
{

	struct proc_dir_entry *pde = PDE(filep->f_path.dentry->d_inode);
	struct mlx4_shared_mr_info *smr_info =
		(struct mlx4_shared_mr_info *)pde->data;

	/* Prevent any mapping not on start of area */
	if (vma->vm_pgoff != 0)
		return -EINVAL;

	return ib_umem_map_to_vma(smr_info->umem,
					vma);

}

static const struct file_operations shared_mr_proc_ops = {
	.owner	= THIS_MODULE,
	.read	= shared_mr_proc_read,
	.write	= shared_mr_proc_write,
	.mmap	= shared_mr_mmap
};

static mode_t convert_shared_access(int acc)
{

	return (acc & IB_ACCESS_SHARED_MR_USER_READ ? S_IRUSR       : 0) |
	       (acc & IB_ACCESS_SHARED_MR_USER_WRITE  ? S_IWUSR : 0) |
	       (acc & IB_ACCESS_SHARED_MR_GROUP_READ   ? S_IRGRP  : 0) |
	       (acc & IB_ACCESS_SHARED_MR_GROUP_WRITE   ? S_IWGRP  : 0) |
	       (acc & IB_ACCESS_SHARED_MR_OTHER_READ   ? S_IROTH  : 0) |
	       (acc & IB_ACCESS_SHARED_MR_OTHER_WRITE   ? S_IWOTH  : 0);

}
#endif
struct ib_mr *mlx4_ib_get_dma_mr(struct ib_pd *pd, int acc)
{
	struct mlx4_ib_mr *mr;
	int err;

	mr = kzalloc(sizeof *mr, GFP_KERNEL);
	if (!mr)
		return ERR_PTR(-ENOMEM);

	err = mlx4_mr_alloc(to_mdev(pd->device)->dev, to_mpd(pd)->pdn, 0,
			    ~0ull, convert_access(acc), 0, 0, &mr->mmr);
	if (err)
		goto err_free;

	err = mlx4_mr_enable(to_mdev(pd->device)->dev, &mr->mmr);
	if (err)
		goto err_mr;

	mr->ibmr.rkey = mr->ibmr.lkey = mr->mmr.key;
	mr->umem = NULL;

	return &mr->ibmr;

err_mr:
	mlx4_mr_free(to_mdev(pd->device)->dev, &mr->mmr);

err_free:
	kfree(mr);

	return ERR_PTR(err);
}

static int mlx4_ib_umem_write_mtt_block(struct mlx4_ib_dev *dev,
						struct mlx4_mtt *mtt,
						u64 mtt_size,
						u64 mtt_shift,
						u64 len,
						u64 cur_start_addr,
						u64 *pages,
						int *start_index,
						int *npages)
{
	int k;
	int err = 0;
	u64 mtt_entries;
	u64 cur_end_addr = cur_start_addr + len;
	u64 cur_end_addr_aligned = 0;

	len += (cur_start_addr & (mtt_size-1ULL));
	cur_end_addr_aligned = round_up(cur_end_addr, mtt_size);
	len += (cur_end_addr_aligned - cur_end_addr);
	if (len & (mtt_size-1ULL)) {
		WARN(1 ,
		"write_block: len %llx is not aligned to mtt_size %llx\n",
			(long long)len, (long long)mtt_size);
		return -EINVAL;
	}


	mtt_entries = (len >> mtt_shift);

	/* Align the MTT start address to
		the mtt_size.
		Required to handle cases when the MR
		starts in the middle of an MTT record.
		Was not required in old code since
		the physical addresses provided by
		the dma subsystem were page aligned,
		which was also the MTT size.
	*/
	cur_start_addr = round_down(cur_start_addr, mtt_size);
	/* A new block is started ...*/
	for (k = 0; k < mtt_entries; ++k) {
		pages[*npages] = cur_start_addr + (mtt_size * k);
		(*npages)++;
		/*
		 * Be friendly to mlx4_write_mtt() and
		 * pass it chunks of appropriate size.
		 */
		if (*npages == PAGE_SIZE / sizeof(u64)) {
			err = mlx4_write_mtt(dev->dev,
					mtt, *start_index,
					*npages, pages);
			if (err)
				return err;

			(*start_index) += *npages;
			*npages = 0;
		}
	}

	return 0;
}

int mlx4_ib_umem_write_mtt(struct mlx4_ib_dev *dev, struct mlx4_mtt *mtt,
			   struct ib_umem *umem)
{
	u64 *pages;
	struct ib_umem_chunk *chunk;
	int j;
	u64 len = 0;
	int err = 0;
	u64 mtt_size;
	u64 cur_start_addr = 0;
	u64 mtt_shift;
	int start_index = 0;
	int npages = 0;

	pages = (u64 *) __get_free_page(GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	mtt_shift = mtt->page_shift;
	mtt_size = 1ULL << mtt_shift;

	list_for_each_entry(chunk, &umem->chunk_list, list)
		for (j = 0; j < chunk->nmap; ++j) {
			if (cur_start_addr + len ==
			    sg_dma_address(&chunk->page_list[j])) {
				/* still the same block */
				len += sg_dma_len(&chunk->page_list[j]);
				continue;
			}
			/* A new block is started ...*/
			/* If len is malaligned, write an extra mtt entry to
			    cover the misaligned area (round up the division)
			*/
			err = mlx4_ib_umem_write_mtt_block(dev,
						mtt, mtt_size, mtt_shift,
						len, cur_start_addr,
						pages,
						&start_index,
						&npages);
			if (err)
				goto out;

			cur_start_addr =
				sg_dma_address(&chunk->page_list[j]);
			len = sg_dma_len(&chunk->page_list[j]);
		}

	/* Handle the last block */
	if (len > 0) {
		/*  If len is malaligned, write an extra mtt entry to cover
		     the misaligned area (round up the division)
		*/
		err = mlx4_ib_umem_write_mtt_block(dev,
						mtt, mtt_size, mtt_shift,
						len, cur_start_addr,
						pages,
						&start_index,
						&npages);
			if (err)
				goto out;
	}


	if (npages)
		err = mlx4_write_mtt(dev->dev, mtt, start_index, npages, pages);

out:
	free_page((unsigned long) pages);
	return err;
}

static inline u64 alignment_of(u64 ptr)
{
	return ilog2(ptr & (~(ptr-1)));
}

static int mlx4_ib_umem_calc_block_mtt(u64 next_block_start,
						u64 current_block_end,
						u64 block_shift)
{
	/* Check whether the alignment of the new block
	     is aligned as well as the previous block.
	     Block address must start with zeros till size of entity_size.
	*/
	if ((next_block_start & ((1ULL << block_shift) - 1ULL)) != 0)
		/* It is not as well aligned as the
		previous block-reduce the mtt size
		accordingly.
		Here we take the last right bit
		which is 1.
		*/
		block_shift = alignment_of(next_block_start);

	/*  Check whether the alignment of the
	     end of previous block - is it aligned
	     as well as the start of the block
	*/
	if (((current_block_end) & ((1ULL << block_shift) - 1ULL)) != 0)
		/* It is not as well aligned as
		the start of the block - reduce the
		mtt size accordingly.
		*/
		block_shift = alignment_of(current_block_end);

	return block_shift;
}

/* Calculate optimal mtt size based on contiguous pages.
* Function will return also the number of pages that are not aligned to the
   calculated mtt_size to be added to total number
    of pages. For that we should check the first chunk length & last chunk
    length and if not aligned to mtt_size we should increment
    the non_aligned_pages number.
    All chunks in the middle already handled as part of mtt shift calculation
    for both their start & end addresses.
*/
int mlx4_ib_umem_calc_optimal_mtt_size(struct ib_umem *umem,
						u64 start_va,
						int *num_of_mtts)
{
	struct ib_umem_chunk *chunk;
	int j;
	u64 block_shift = MLX4_MAX_MTT_SHIFT;
	u64 current_block_len = 0;
	u64 current_block_start = 0;
	u64 misalignment_bits;
	u64 first_block_start = 0;
	u64 last_block_end = 0;
	u64 total_len = 0;
	u64 last_block_aligned_end = 0;
	u64 min_shift = ilog2(umem->page_size);

	list_for_each_entry(chunk, &umem->chunk_list, list) {
		/* Initialization - save the first chunk start as
		    the current_block_start - block means contiguous pages.
		*/
		if (current_block_len == 0 && current_block_start == 0) {
			first_block_start = current_block_start =
				sg_dma_address(&chunk->page_list[0]);
			/* Find the bits that are different between
			    the physical address and the virtual
			    address for the start of the MR.
			*/
			/* umem_get aligned the start_va to a page
			   boundry. Therefore, we need to align the
			   start va to the same boundry */
			/* misalignment_bits is needed to handle the
			   case of a single memory region. In this
			   case, the rest of the logic will not reduce
			   the block size.  If we use a block size
			   which is bigger than the alignment of the
			   misalignment bits, we might use the virtual
			   page number instead of the physical page
			   number, resulting in access to the wrong
			   data. */
			misalignment_bits =
			(start_va & (~(((u64)(umem->page_size))-1ULL)))
						^ current_block_start;
			block_shift = min(alignment_of(misalignment_bits)
				, block_shift);
		}

		/* Go over the scatter entries in the current chunk, check
		     if they continue the previous scatter entry.
		*/
		for (j = 0; j < chunk->nmap; ++j) {
			u64 next_block_start =
				sg_dma_address(&chunk->page_list[j]);
			u64 current_block_end = current_block_start
				+ current_block_len;
			/* If we have a split (non-contig.) between two block*/
			if (current_block_end != next_block_start) {
				block_shift = mlx4_ib_umem_calc_block_mtt(
						next_block_start,
						current_block_end,
						block_shift);

				/* If we reached the minimum shift for 4k
				     page we stop the loop.
				*/
				if (block_shift <= min_shift)
					goto end;

				/* If not saved yet we are in first block -
				     we save the length of first block to
				     calculate the non_aligned_pages number at
				*    the end.
				*/
				total_len += current_block_len;

				/* Start a new block */
				current_block_start = next_block_start;
				current_block_len =
					sg_dma_len(&chunk->page_list[j]);
				continue;
			}
			/* The scatter entry is another part of
			     the current block, increase the block size
			* An entry in the scatter can be larger than
			4k (page) as of dma mapping
			which merge some blocks together.
			*/
			current_block_len +=
				sg_dma_len(&chunk->page_list[j]);
		}
	}

	/* Account for the last block in the total len */
	total_len += current_block_len;
	/* Add to the first block the misalignment that it suffers from.*/
	total_len += (first_block_start & ((1ULL<<block_shift)-1ULL));
	last_block_end = current_block_start+current_block_len;
	last_block_aligned_end = round_up(last_block_end, 1<<block_shift);
	total_len += (last_block_aligned_end - last_block_end);

	WARN((total_len & ((1ULL<<block_shift)-1ULL)),
		" misaligned total length detected (%llu, %llu)!",
		(long long)total_len, (long long)block_shift);

	*num_of_mtts = total_len >> block_shift;
end:
	if (block_shift < min_shift) {
		/* If shift is less than the min we set a WARN and
		     return the min shift.
		*/
		WARN(1,
		"mlx4_ib_umem_calc_optimal_mtt_size - unexpected shift %lld\n",
		(long long)block_shift);

		block_shift = min_shift;
	}
	return block_shift;
}

#ifdef __linux__
static int prepare_shared_mr(struct mlx4_ib_mr *mr, int access_flags, int mr_id)
{
	struct proc_dir_entry *mr_proc_entry;
	mode_t mode = S_IFREG;
	char name_buff[16];

	mode |= convert_shared_access(access_flags);
	sprintf(name_buff, "%X", mr_id);
	mr->smr_info = kmalloc(sizeof(struct mlx4_shared_mr_info), GFP_KERNEL);
	mr->smr_info->mr_id = mr_id;
	mr->smr_info->umem = mr->umem;

	mr_proc_entry = proc_create_data(name_buff, mode,
				mlx4_mrs_dir_entry,
				&shared_mr_proc_ops,
				mr->smr_info);

	if (!mr_proc_entry) {
		pr_err("prepare_shared_mr failed via proc\n");
		kfree(mr->smr_info);
		return -ENODEV;
	}

	current_uid_gid(&(mr_proc_entry->uid), &(mr_proc_entry->gid));
	mr_proc_entry->size = mr->umem->length;
	return 0;

}
static int is_shared_mr(int access_flags)
{
	/* We should check whether IB_ACCESS_SHARED_MR_USER_READ or
	other shared bits were turned on.
	*/
	return !!(access_flags & (IB_ACCESS_SHARED_MR_USER_READ |
				IB_ACCESS_SHARED_MR_USER_WRITE |
				IB_ACCESS_SHARED_MR_GROUP_READ |
				IB_ACCESS_SHARED_MR_GROUP_WRITE |
				IB_ACCESS_SHARED_MR_OTHER_READ |
				IB_ACCESS_SHARED_MR_OTHER_WRITE));

}
#endif

struct ib_mr *mlx4_ib_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
				  u64 virt_addr, int access_flags,
				  struct ib_udata *udata,
				  int mr_id)
{
	struct mlx4_ib_dev *dev = to_mdev(pd->device);
	struct mlx4_ib_mr *mr;
	int shift;
	int err;
	int n;

	mr = kzalloc(sizeof *mr, GFP_KERNEL);
	if (!mr)
		return ERR_PTR(-ENOMEM);

	mr->umem = ib_umem_get(pd->uobject->context, start, length,
			access_flags, 0);
	if (IS_ERR(mr->umem)) {
		err = PTR_ERR(mr->umem);
		goto err_free;
	}

	n = ib_umem_page_count(mr->umem);
	shift = mlx4_ib_umem_calc_optimal_mtt_size(mr->umem, start,
		&n);
	err = mlx4_mr_alloc(dev->dev, to_mpd(pd)->pdn, virt_addr, length,
			 convert_access(access_flags), n, shift, &mr->mmr);
	if (err)
		goto err_umem;

	err = mlx4_ib_umem_write_mtt(dev, &mr->mmr.mtt, mr->umem);
	if (err)
		goto err_mr;

	err = mlx4_mr_enable(dev->dev, &mr->mmr);
	if (err)
		goto err_mr;

	mr->ibmr.rkey = mr->ibmr.lkey = mr->mmr.key;
#ifdef __linux__
	/* Check whether MR should be shared */
	if (is_shared_mr(access_flags)) {
	/* start address and length must be aligned to page size in order
	    to map a full page and preventing leakage of data */
		if (mr->umem->offset || (length & ~PAGE_MASK)) {
		        err = -EINVAL;
		        goto err_mr;
		}

		err = prepare_shared_mr(mr, access_flags, mr_id);
		if (err)
			goto err_mr;
	}
#endif
	return &mr->ibmr;

err_mr:
	mlx4_mr_free(to_mdev(pd->device)->dev, &mr->mmr);

err_umem:
	ib_umem_release(mr->umem);

err_free:
	kfree(mr);

	return ERR_PTR(err);
}


int mlx4_ib_dereg_mr(struct ib_mr *ibmr)
{
	struct mlx4_ib_mr *mr = to_mmr(ibmr);

	mlx4_mr_free(to_mdev(ibmr->device)->dev, &mr->mmr);
	if (mr->smr_info) {
		/* When master/parent shared mr is dereged there is
		no ability to share this mr any more - its mr_id will be
		returned to the kernel as part of ib_uverbs_dereg_mr
		and may be allocated again as part of other reg_mr.
		*/
		char name_buff[16];

		sprintf(name_buff, "%X", mr->smr_info->mr_id);
		/* Remove proc entry is checking internally that no operation
		    was strated on that proc fs file and if in the middle
		    current process will wait till end of operation.
		    That's why no sync mechanism is needed when we release
		    below the shared umem.
		*/
#ifdef __linux__
		remove_proc_entry(name_buff, mlx4_mrs_dir_entry);
		kfree(mr->smr_info);
#endif
	}

	if (mr->umem)
		ib_umem_release(mr->umem);

	kfree(mr);

	return 0;
}

struct ib_mr *mlx4_ib_alloc_fast_reg_mr(struct ib_pd *pd,
					int max_page_list_len)
{
	struct mlx4_ib_dev *dev = to_mdev(pd->device);
	struct mlx4_ib_mr *mr;
	int err;

	mr = kzalloc(sizeof *mr, GFP_KERNEL);
	if (!mr)
		return ERR_PTR(-ENOMEM);

	err = mlx4_mr_alloc(dev->dev, to_mpd(pd)->pdn, 0, 0, 0,
			    max_page_list_len, 0, &mr->mmr);
	if (err)
		goto err_free;

	err = mlx4_mr_enable(dev->dev, &mr->mmr);
	if (err)
		goto err_mr;

	mr->ibmr.rkey = mr->ibmr.lkey = mr->mmr.key;
	mr->umem = NULL;

	return &mr->ibmr;

err_mr:
	mlx4_mr_free(dev->dev, &mr->mmr);

err_free:
	kfree(mr);
	return ERR_PTR(err);
}

struct ib_fast_reg_page_list *mlx4_ib_alloc_fast_reg_page_list(struct ib_device *ibdev,
							       int page_list_len)
{
	struct mlx4_ib_dev *dev = to_mdev(ibdev);
	struct mlx4_ib_fast_reg_page_list *mfrpl;
	int size = page_list_len * sizeof (u64);

	if (page_list_len > MLX4_MAX_FAST_REG_PAGES)
		return ERR_PTR(-EINVAL);

	mfrpl = kmalloc(sizeof *mfrpl, GFP_KERNEL);
	if (!mfrpl)
		return ERR_PTR(-ENOMEM);

	mfrpl->ibfrpl.page_list = kmalloc(size, GFP_KERNEL);
	if (!mfrpl->ibfrpl.page_list)
		goto err_free;

	mfrpl->mapped_page_list = dma_alloc_coherent(&dev->dev->pdev->dev,
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

void mlx4_ib_free_fast_reg_page_list(struct ib_fast_reg_page_list *page_list)
{
	struct mlx4_ib_dev *dev = to_mdev(page_list->device);
	struct mlx4_ib_fast_reg_page_list *mfrpl = to_mfrpl(page_list);
	int size = page_list->max_page_list_len * sizeof (u64);

	dma_free_coherent(&dev->dev->pdev->dev, size, mfrpl->mapped_page_list,
			  mfrpl->map);
	kfree(mfrpl->ibfrpl.page_list);
	kfree(mfrpl);
}

struct ib_fmr *mlx4_ib_fmr_alloc(struct ib_pd *pd, int acc,
				 struct ib_fmr_attr *fmr_attr)
{
	struct mlx4_ib_dev *dev = to_mdev(pd->device);
	struct mlx4_ib_fmr *fmr;
	int err = -ENOMEM;

	fmr = kmalloc(sizeof *fmr, GFP_KERNEL);
	if (!fmr)
		return ERR_PTR(-ENOMEM);

	err = mlx4_fmr_alloc(dev->dev, to_mpd(pd)->pdn, convert_access(acc),
			     fmr_attr->max_pages, fmr_attr->max_maps,
			     fmr_attr->page_shift, &fmr->mfmr);
	if (err)
		goto err_free;

	err = mlx4_fmr_enable(to_mdev(pd->device)->dev, &fmr->mfmr);
	if (err)
		goto err_mr;

	fmr->ibfmr.rkey = fmr->ibfmr.lkey = fmr->mfmr.mr.key;

	return &fmr->ibfmr;

err_mr:
	mlx4_mr_free(to_mdev(pd->device)->dev, &fmr->mfmr.mr);

err_free:
	kfree(fmr);

	return ERR_PTR(err);
}

int mlx4_ib_map_phys_fmr(struct ib_fmr *ibfmr, u64 *page_list,
		      int npages, u64 iova)
{
	struct mlx4_ib_fmr *ifmr = to_mfmr(ibfmr);
	struct mlx4_ib_dev *dev = to_mdev(ifmr->ibfmr.device);

	return mlx4_map_phys_fmr(dev->dev, &ifmr->mfmr, page_list, npages, iova,
				 &ifmr->ibfmr.lkey, &ifmr->ibfmr.rkey);
}

int mlx4_ib_unmap_fmr(struct list_head *fmr_list)
{
	struct ib_fmr *ibfmr;
	int err;
	struct mlx4_dev *mdev = NULL;

	list_for_each_entry(ibfmr, fmr_list, list) {
		if (mdev && to_mdev(ibfmr->device)->dev != mdev)
			return -EINVAL;
		mdev = to_mdev(ibfmr->device)->dev;
	}

	if (!mdev)
		return 0;

	list_for_each_entry(ibfmr, fmr_list, list) {
		struct mlx4_ib_fmr *ifmr = to_mfmr(ibfmr);

		mlx4_fmr_unmap(mdev, &ifmr->mfmr, &ifmr->ibfmr.lkey, &ifmr->ibfmr.rkey);
	}

	/*
	 * Make sure all MPT status updates are visible before issuing
	 * SYNC_TPT firmware command.
	 */
	wmb();

	err = mlx4_SYNC_TPT(mdev);
	if (err)
		pr_warn("SYNC_TPT error %d when "
		       "unmapping FMRs\n", err);

	return 0;
}

int mlx4_ib_fmr_dealloc(struct ib_fmr *ibfmr)
{
	struct mlx4_ib_fmr *ifmr = to_mfmr(ibfmr);
	struct mlx4_ib_dev *dev = to_mdev(ibfmr->device);
	int err;

	err = mlx4_fmr_free(dev->dev, &ifmr->mfmr);

	if (!err)
		kfree(ifmr);

	return err;
}
