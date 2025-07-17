/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2015 - 2023 Intel Corporation
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenFabrics.org BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *    - Redistributions of source code must retain the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *    - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
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

#include "osdep.h"
#include "irdma_hmc.h"
#include "irdma_defs.h"
#include "irdma_type.h"
#include "irdma_protos.h"
#include "irdma_pble.h"

static int add_pble_prm(struct irdma_hmc_pble_rsrc *pble_rsrc);

/**
 * irdma_destroy_pble_prm - destroy prm during module unload
 * @pble_rsrc: pble resources
 */
void
irdma_destroy_pble_prm(struct irdma_hmc_pble_rsrc *pble_rsrc)
{
	struct irdma_chunk *chunk;
	struct irdma_pble_prm *pinfo = &pble_rsrc->pinfo;

	while (!list_empty(&pinfo->clist)) {
		chunk = (struct irdma_chunk *)(&pinfo->clist)->next;
		list_del(&chunk->list);
		if (chunk->type == PBLE_SD_PAGED)
			irdma_pble_free_paged_mem(chunk);
		bitmap_free(chunk->bitmapbuf);
		kfree(chunk->chunkmem.va);
	}
	spin_lock_destroy(&pinfo->prm_lock);
	mutex_destroy(&pble_rsrc->pble_mutex_lock);
}

/**
 * irdma_hmc_init_pble - Initialize pble resources during module load
 * @dev: irdma_sc_dev struct
 * @pble_rsrc: pble resources
 */
int
irdma_hmc_init_pble(struct irdma_sc_dev *dev,
		    struct irdma_hmc_pble_rsrc *pble_rsrc)
{
	struct irdma_hmc_info *hmc_info;
	u32 fpm_idx = 0;
	int status = 0;

	hmc_info = dev->hmc_info;
	pble_rsrc->dev = dev;
	pble_rsrc->fpm_base_addr = hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].base;
	/* Start pble' on 4k boundary */
	if (pble_rsrc->fpm_base_addr & 0xfff)
		fpm_idx = (4096 - (pble_rsrc->fpm_base_addr & 0xfff)) >> 3;
	pble_rsrc->unallocated_pble =
	    hmc_info->hmc_obj[IRDMA_HMC_IW_PBLE].cnt - fpm_idx;
	pble_rsrc->next_fpm_addr = pble_rsrc->fpm_base_addr + (fpm_idx << 3);
	pble_rsrc->pinfo.pble_shift = PBLE_SHIFT;

	mutex_init(&pble_rsrc->pble_mutex_lock);

	spin_lock_init(&pble_rsrc->pinfo.prm_lock);
	INIT_LIST_HEAD(&pble_rsrc->pinfo.clist);
	if (add_pble_prm(pble_rsrc)) {
		irdma_destroy_pble_prm(pble_rsrc);
		status = -ENOMEM;
	}

	return status;
}

/**
 * get_sd_pd_idx -  Returns sd index, pd index and rel_pd_idx from fpm address
 * @pble_rsrc: structure containing fpm address
 * @idx: where to return indexes
 */
static void
get_sd_pd_idx(struct irdma_hmc_pble_rsrc *pble_rsrc,
	      struct sd_pd_idx *idx)
{
	idx->sd_idx = (u32)pble_rsrc->next_fpm_addr / IRDMA_HMC_DIRECT_BP_SIZE;
	idx->pd_idx = (u32)(pble_rsrc->next_fpm_addr / IRDMA_HMC_PAGED_BP_SIZE);
	idx->rel_pd_idx = (idx->pd_idx % IRDMA_HMC_PD_CNT_IN_SD);
}

/**
 * add_sd_direct - add sd direct for pble
 * @pble_rsrc: pble resource ptr
 * @info: page info for sd
 */
static int
add_sd_direct(struct irdma_hmc_pble_rsrc *pble_rsrc,
	      struct irdma_add_page_info *info)
{
	struct irdma_sc_dev *dev = pble_rsrc->dev;
	int ret_code = 0;
	struct sd_pd_idx *idx = &info->idx;
	struct irdma_chunk *chunk = info->chunk;
	struct irdma_hmc_info *hmc_info = info->hmc_info;
	struct irdma_hmc_sd_entry *sd_entry = info->sd_entry;
	u32 offset = 0;

	if (!sd_entry->valid) {
		ret_code = irdma_add_sd_table_entry(dev->hw, hmc_info,
						    info->idx.sd_idx,
						    IRDMA_SD_TYPE_DIRECT,
						    IRDMA_HMC_DIRECT_BP_SIZE);
		if (ret_code)
			return ret_code;

		chunk->type = PBLE_SD_CONTIGOUS;
	}

	offset = idx->rel_pd_idx << HMC_PAGED_BP_SHIFT;
	chunk->size = info->pages << HMC_PAGED_BP_SHIFT;
	chunk->vaddr = (u8 *)sd_entry->u.bp.addr.va + offset;
	chunk->fpm_addr = pble_rsrc->next_fpm_addr;
	irdma_debug(dev, IRDMA_DEBUG_PBLE,
		    "chunk_size[%ld] = 0x%lx vaddr=0x%p fpm_addr = %lx\n",
		    chunk->size, chunk->size, chunk->vaddr, chunk->fpm_addr);

	return 0;
}

/**
 * fpm_to_idx - given fpm address, get pble index
 * @pble_rsrc: pble resource management
 * @addr: fpm address for index
 */
static u32 fpm_to_idx(struct irdma_hmc_pble_rsrc *pble_rsrc, u64 addr){
	u64 idx;

	idx = (addr - (pble_rsrc->fpm_base_addr)) >> 3;

	return (u32)idx;
}

/**
 * add_bp_pages - add backing pages for sd
 * @pble_rsrc: pble resource management
 * @info: page info for sd
 */
static int
add_bp_pages(struct irdma_hmc_pble_rsrc *pble_rsrc,
	     struct irdma_add_page_info *info)
{
	struct irdma_sc_dev *dev = pble_rsrc->dev;
	u8 *addr;
	struct irdma_dma_mem mem;
	struct irdma_hmc_pd_entry *pd_entry;
	struct irdma_hmc_sd_entry *sd_entry = info->sd_entry;
	struct irdma_hmc_info *hmc_info = info->hmc_info;
	struct irdma_chunk *chunk = info->chunk;
	int status = 0;
	u32 rel_pd_idx = info->idx.rel_pd_idx;
	u32 pd_idx = info->idx.pd_idx;
	u32 i;

	if (irdma_pble_get_paged_mem(chunk, info->pages))
		return -ENOMEM;

	status = irdma_add_sd_table_entry(dev->hw, hmc_info, info->idx.sd_idx,
					  IRDMA_SD_TYPE_PAGED,
					  IRDMA_HMC_DIRECT_BP_SIZE);
	if (status)
		goto error;

	addr = chunk->vaddr;
	for (i = 0; i < info->pages; i++) {
		mem.pa = (u64)chunk->dmainfo.dmaaddrs[i];
		mem.size = 4096;
		mem.va = addr;
		pd_entry = &sd_entry->u.pd_table.pd_entry[rel_pd_idx++];
		if (!pd_entry->valid) {
			status = irdma_add_pd_table_entry(dev, hmc_info,
							  pd_idx++, &mem);
			if (status)
				goto error;

			addr += 4096;
		}
	}

	chunk->fpm_addr = pble_rsrc->next_fpm_addr;
	return 0;

error:
	irdma_pble_free_paged_mem(chunk);

	return status;
}

/**
 * irdma_get_type - add a sd entry type for sd
 * @dev: irdma_sc_dev struct
 * @idx: index of sd
 * @pages: pages in the sd
 */
static enum irdma_sd_entry_type
irdma_get_type(struct irdma_sc_dev *dev,
	       struct sd_pd_idx *idx, u32 pages)
{
	enum irdma_sd_entry_type sd_entry_type;

	sd_entry_type = !idx->rel_pd_idx && pages == IRDMA_HMC_PD_CNT_IN_SD ?
	    IRDMA_SD_TYPE_DIRECT : IRDMA_SD_TYPE_PAGED;
	return sd_entry_type;
}

/**
 * add_pble_prm - add a sd entry for pble resoure
 * @pble_rsrc: pble resource management
 */
static int
add_pble_prm(struct irdma_hmc_pble_rsrc *pble_rsrc)
{
	struct irdma_sc_dev *dev = pble_rsrc->dev;
	struct irdma_hmc_sd_entry *sd_entry;
	struct irdma_hmc_info *hmc_info;
	struct irdma_chunk *chunk;
	struct irdma_add_page_info info;
	struct sd_pd_idx *idx = &info.idx;
	int ret_code = 0;
	enum irdma_sd_entry_type sd_entry_type;
	u64 sd_reg_val = 0;
	struct irdma_virt_mem chunkmem;
	u32 pages;

	if (pble_rsrc->unallocated_pble < PBLE_PER_PAGE)
		return -ENOMEM;

	if (pble_rsrc->next_fpm_addr & 0xfff)
		return -EINVAL;

	chunkmem.size = sizeof(*chunk);
	chunkmem.va = kzalloc(chunkmem.size, GFP_KERNEL);
	if (!chunkmem.va)
		return -ENOMEM;

	chunk = chunkmem.va;
	chunk->chunkmem = chunkmem;
	hmc_info = dev->hmc_info;
	chunk->dev = dev;
	chunk->fpm_addr = pble_rsrc->next_fpm_addr;
	get_sd_pd_idx(pble_rsrc, idx);
	sd_entry = &hmc_info->sd_table.sd_entry[idx->sd_idx];
	pages = (idx->rel_pd_idx) ? (IRDMA_HMC_PD_CNT_IN_SD - idx->rel_pd_idx) :
	    IRDMA_HMC_PD_CNT_IN_SD;
	pages = min(pages, pble_rsrc->unallocated_pble >> PBLE_512_SHIFT);
	info.chunk = chunk;
	info.hmc_info = hmc_info;
	info.pages = pages;
	info.sd_entry = sd_entry;
	if (!sd_entry->valid)
		sd_entry_type = irdma_get_type(dev, idx, pages);
	else
		sd_entry_type = sd_entry->entry_type;

	irdma_debug(dev, IRDMA_DEBUG_PBLE,
		    "pages = %d, unallocated_pble[%d] current_fpm_addr = %lx\n",
		    pages, pble_rsrc->unallocated_pble,
		    pble_rsrc->next_fpm_addr);
	irdma_debug(dev, IRDMA_DEBUG_PBLE, "sd_entry_type = %d\n",
		    sd_entry_type);
	if (sd_entry_type == IRDMA_SD_TYPE_DIRECT)
		ret_code = add_sd_direct(pble_rsrc, &info);

	if (ret_code)
		sd_entry_type = IRDMA_SD_TYPE_PAGED;
	else
		pble_rsrc->stats_direct_sds++;

	if (sd_entry_type == IRDMA_SD_TYPE_PAGED) {
		ret_code = add_bp_pages(pble_rsrc, &info);
		if (ret_code)
			goto err_bp_pages;
		else
			pble_rsrc->stats_paged_sds++;
	}

	ret_code = irdma_prm_add_pble_mem(&pble_rsrc->pinfo, chunk);
	if (ret_code)
		goto err_bp_pages;

	pble_rsrc->next_fpm_addr += chunk->size;
	irdma_debug(dev, IRDMA_DEBUG_PBLE,
		    "next_fpm_addr = %lx chunk_size[%lu] = 0x%lx\n",
		    pble_rsrc->next_fpm_addr, chunk->size, chunk->size);
	pble_rsrc->unallocated_pble -= (u32)(chunk->size >> 3);
	sd_reg_val = (sd_entry_type == IRDMA_SD_TYPE_PAGED) ?
	    sd_entry->u.pd_table.pd_page_addr.pa :
	    sd_entry->u.bp.addr.pa;
	if (!sd_entry->valid) {
		ret_code = irdma_hmc_sd_one(dev, hmc_info->hmc_fn_id, sd_reg_val,
					    idx->sd_idx, sd_entry->entry_type, true);
		if (ret_code)
			goto error;
	}

	sd_entry->valid = true;
	list_add(&chunk->list, &pble_rsrc->pinfo.clist);
	return 0;

error:
	bitmap_free(chunk->bitmapbuf);
err_bp_pages:
	kfree(chunk->chunkmem.va);

	return ret_code;
}

/**
 * free_lvl2 - fee level 2 pble
 * @pble_rsrc: pble resource management
 * @palloc: level 2 pble allocation
 */
static void
free_lvl2(struct irdma_hmc_pble_rsrc *pble_rsrc,
	  struct irdma_pble_alloc *palloc)
{
	u32 i;
	struct irdma_pble_level2 *lvl2 = &palloc->level2;
	struct irdma_pble_info *root = &lvl2->root;
	struct irdma_pble_info *leaf = lvl2->leaf;

	for (i = 0; i < lvl2->leaf_cnt; i++, leaf++) {
		if (leaf->addr)
			irdma_prm_return_pbles(&pble_rsrc->pinfo,
					       &leaf->chunkinfo);
		else
			break;
	}

	if (root->addr)
		irdma_prm_return_pbles(&pble_rsrc->pinfo, &root->chunkinfo);

	kfree(lvl2->leafmem.va);
	lvl2->leaf = NULL;
}

/**
 * get_lvl2_pble - get level 2 pble resource
 * @pble_rsrc: pble resource management
 * @palloc: level 2 pble allocation
 */
static int
get_lvl2_pble(struct irdma_hmc_pble_rsrc *pble_rsrc,
	      struct irdma_pble_alloc *palloc)
{
	u32 lf4k, lflast, total, i;
	u32 pblcnt = PBLE_PER_PAGE;
	u64 *addr;
	struct irdma_pble_level2 *lvl2 = &palloc->level2;
	struct irdma_pble_info *root = &lvl2->root;
	struct irdma_pble_info *leaf;
	int ret_code;
	u64 fpm_addr;

	/* number of full 512 (4K) leafs) */
	lf4k = palloc->total_cnt >> 9;
	lflast = palloc->total_cnt % PBLE_PER_PAGE;
	total = (lflast == 0) ? lf4k : lf4k + 1;
	lvl2->leaf_cnt = total;

	lvl2->leafmem.size = (sizeof(*leaf) * total);
	lvl2->leafmem.va = kzalloc(lvl2->leafmem.size, GFP_KERNEL);
	if (!lvl2->leafmem.va)
		return -ENOMEM;

	lvl2->leaf = lvl2->leafmem.va;
	leaf = lvl2->leaf;
	ret_code = irdma_prm_get_pbles(&pble_rsrc->pinfo, &root->chunkinfo,
				       total << 3, &root->addr, &fpm_addr);
	if (ret_code) {
		kfree(lvl2->leafmem.va);
		lvl2->leaf = NULL;
		return -ENOMEM;
	}

	root->idx = fpm_to_idx(pble_rsrc, fpm_addr);
	root->cnt = total;
	addr = root->addr;
	for (i = 0; i < total; i++, leaf++) {
		pblcnt = (lflast && ((i + 1) == total)) ?
		    lflast : PBLE_PER_PAGE;
		ret_code = irdma_prm_get_pbles(&pble_rsrc->pinfo,
					       &leaf->chunkinfo, pblcnt << 3,
					       &leaf->addr, &fpm_addr);
		if (ret_code)
			goto error;

		leaf->idx = fpm_to_idx(pble_rsrc, fpm_addr);

		leaf->cnt = pblcnt;
		*addr = (u64)leaf->idx;
		addr++;
	}

	palloc->level = PBLE_LEVEL_2;
	pble_rsrc->stats_lvl2++;
	return 0;

error:
	free_lvl2(pble_rsrc, palloc);

	return -ENOMEM;
}

/**
 * get_lvl1_pble - get level 1 pble resource
 * @pble_rsrc: pble resource management
 * @palloc: level 1 pble allocation
 */
static int
get_lvl1_pble(struct irdma_hmc_pble_rsrc *pble_rsrc,
	      struct irdma_pble_alloc *palloc)
{
	int ret_code;
	u64 fpm_addr;
	struct irdma_pble_info *lvl1 = &palloc->level1;

	ret_code = irdma_prm_get_pbles(&pble_rsrc->pinfo, &lvl1->chunkinfo,
				       palloc->total_cnt << 3, &lvl1->addr,
				       &fpm_addr);
	if (ret_code)
		return -ENOMEM;

	palloc->level = PBLE_LEVEL_1;
	lvl1->idx = fpm_to_idx(pble_rsrc, fpm_addr);
	lvl1->cnt = palloc->total_cnt;
	pble_rsrc->stats_lvl1++;

	return 0;
}

/**
 * get_lvl1_lvl2_pble - calls get_lvl1 and get_lvl2 pble routine
 * @pble_rsrc: pble resources
 * @palloc: contains all inforamtion regarding pble (idx + pble addr)
 * @lvl: Bitmask for requested pble level
 */
static int
get_lvl1_lvl2_pble(struct irdma_hmc_pble_rsrc *pble_rsrc,
		   struct irdma_pble_alloc *palloc, u8 lvl)
{
	int status = 0;

	status = get_lvl1_pble(pble_rsrc, palloc);
	if (!status || lvl == PBLE_LEVEL_1 || palloc->total_cnt <= PBLE_PER_PAGE)
		return status;

	status = get_lvl2_pble(pble_rsrc, palloc);

	return status;
}

/**
 * irdma_get_pble - allocate pbles from the prm
 * @pble_rsrc: pble resources
 * @palloc: contains all inforamtion regarding pble (idx + pble addr)
 * @pble_cnt: #of pbles requested
 * @lvl: requested pble level mask
 */
int
irdma_get_pble(struct irdma_hmc_pble_rsrc *pble_rsrc,
	       struct irdma_pble_alloc *palloc, u32 pble_cnt,
	       u8 lvl)
{
	int status = 0;
	int max_sds = 0;
	int i;

	palloc->total_cnt = pble_cnt;
	palloc->level = PBLE_LEVEL_0;

	mutex_lock(&pble_rsrc->pble_mutex_lock);

	/*
	 * check first to see if we can get pble's without acquiring additional sd's
	 */
	status = get_lvl1_lvl2_pble(pble_rsrc, palloc, lvl);
	if (!status)
		goto exit;

	max_sds = (palloc->total_cnt >> 18) + 1;
	for (i = 0; i < max_sds; i++) {
		status = add_pble_prm(pble_rsrc);
		if (status)
			break;

		status = get_lvl1_lvl2_pble(pble_rsrc, palloc, lvl);
		/* if level1_only, only go through it once */
		if (!status || lvl == PBLE_LEVEL_1)
			break;
	}

exit:
	if (!status) {
		pble_rsrc->allocdpbles += pble_cnt;
		pble_rsrc->stats_alloc_ok++;
	} else {
		pble_rsrc->stats_alloc_fail++;
	}
	mutex_unlock(&pble_rsrc->pble_mutex_lock);

	return status;
}

/**
 * irdma_free_pble - put pbles back into prm
 * @pble_rsrc: pble resources
 * @palloc: contains all information regarding pble resource being freed
 */
void
irdma_free_pble(struct irdma_hmc_pble_rsrc *pble_rsrc,
		struct irdma_pble_alloc *palloc)
{
	pble_rsrc->freedpbles += palloc->total_cnt;

	if (palloc->level == PBLE_LEVEL_2)
		free_lvl2(pble_rsrc, palloc);
	else
		irdma_prm_return_pbles(&pble_rsrc->pinfo,
				       &palloc->level1.chunkinfo);
	pble_rsrc->stats_alloc_freed++;
}
