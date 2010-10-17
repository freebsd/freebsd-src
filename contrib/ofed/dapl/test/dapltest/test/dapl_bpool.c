/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

#include "dapl_proto.h"

/* flag to prevent usage of Memory Windows as they are not supported in mthca */
static DAT_BOOLEAN enable_memory_windows = DAT_FALSE;

/*****************************************************************************/
/*
 * Allocate buffer pool (data buffers)
 *
 *  Caller wants to allocate <num_seg> buffers of <seg_size> bytes,
 *  with each buffer aligned as requested.  The caller is free to
 *  use the buffers separately, or as one contiguous segment, so
 *  we allocate IOV entries enough to support either usage.
 */
Bpool *DT_BpoolAlloc(Per_Test_Data_t * pt_ptr,
		     DT_Tdep_Print_Head * phead,
		     DAT_IA_HANDLE ia_handle,
		     DAT_PZ_HANDLE pz_handle,
		     DAT_EP_HANDLE ep_handle,
		     DAT_EVD_HANDLE rmr_evd_handle,
		     DAT_COUNT seg_size,
		     DAT_COUNT num_segs,
		     DAT_COUNT alignment,
		     DAT_BOOLEAN enable_rdma_write,
		     DAT_BOOLEAN enable_rdma_read)
{
	char *module = "DT_BpoolAlloc";
	unsigned char *alloc_ptr = 0;
	Bpool *bpool_ptr = 0;
	DAT_COUNT alloc_size;
	DAT_REGION_DESCRIPTION region;
	DAT_RETURN ret;

	/* We'll hand out aligned buffers, compensate here */
	seg_size = DT_RoundSize(seg_size, alignment);
	alloc_size = seg_size * num_segs + alignment;

	alloc_ptr = (unsigned char *)DT_MemListAlloc(pt_ptr, "bpool", BUFF,
						     alloc_size);
	if (!alloc_ptr) {
		DT_Tdep_PT_Printf(phead, "No Memory to create bpool buffer!\n");
		goto err;
	}

	bpool_ptr =
	    (Bpool *) DT_MemListAlloc(pt_ptr, "bpool", BPOOL, sizeof(Bpool)
				      + num_segs * sizeof(DAT_LMR_TRIPLET));
	if (!bpool_ptr) {
		DT_Tdep_PT_Printf(phead, "No Memory to create Bpool!\n");
		goto err;
	}

	bpool_ptr->alloc_ptr = alloc_ptr;
	bpool_ptr->alloc_size = alloc_size;
	bpool_ptr->pz_handle = pz_handle;
	bpool_ptr->num_segs = num_segs;
	bpool_ptr->ep_handle = ep_handle;
	bpool_ptr->buffer_size = seg_size * num_segs;
	bpool_ptr->buffer_start = DT_AlignPtr(alloc_ptr, alignment);
	bpool_ptr->tripl_start = (DAT_LMR_TRIPLET *) (bpool_ptr + 1);
	bpool_ptr->seg_size = seg_size;
	bpool_ptr->enable_rdma_write = enable_rdma_write;
	bpool_ptr->enable_rdma_read = enable_rdma_read;
	bpool_ptr->rmr_evd_handle = rmr_evd_handle;

	DT_Tdep_PT_Debug(3,
			 (phead,
			  "lmr_create    [%p, " F64x "]\n",
			  bpool_ptr->buffer_start, bpool_ptr->buffer_size));

	memset(&region, 0, sizeof(region));
	region.for_va = bpool_ptr->buffer_start;
	ret = DT_Tdep_lmr_create(ia_handle,
				 DAT_MEM_TYPE_VIRTUAL,
				 region,
				 bpool_ptr->buffer_size,
				 pz_handle,
				 DAT_MEM_PRIV_ALL_FLAG,
				 &bpool_ptr->lmr_handle,
				 &bpool_ptr->lmr_context,
				 &bpool_ptr->rmr_context,
				 &bpool_ptr->reg_size, &bpool_ptr->reg_addr);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead, "%s: dat_lmr_create failed %s\n",
				  module, DT_RetToString(ret));
		goto err;
	}
	/* verify that the outputs are reasonable */
	if (((uintptr_t) bpool_ptr->reg_addr >
	     (uintptr_t) bpool_ptr->buffer_start)
	    || (bpool_ptr->reg_size <
		bpool_ptr->buffer_size + ((uintptr_t) bpool_ptr->buffer_start -
					  (uintptr_t) bpool_ptr->reg_addr))) {
		DT_Tdep_PT_Printf(phead,
				  "%s: dat_lmr_create bogus" "in: 0x%p, " F64x
				  " out 0x" F64x ", " F64x "\n", module,
				  bpool_ptr->buffer_start,
				  bpool_ptr->buffer_size, bpool_ptr->reg_addr,
				  bpool_ptr->reg_size);
		goto err;
	}

	DT_Tdep_PT_Debug(3, (phead,
			     "lmr_create OK [0x" F64x ", " F64x ", lctx=%x]\n",
			     bpool_ptr->reg_addr,
			     bpool_ptr->reg_size, bpool_ptr->lmr_context));

	/* Enable RDMA if requested */
	if (enable_memory_windows && (enable_rdma_write || enable_rdma_read)) {
		DAT_LMR_TRIPLET iov;
		DAT_RMR_COOKIE cookie;
		DAT_MEM_PRIV_FLAGS mflags;
		DAT_RMR_BIND_COMPLETION_EVENT_DATA rmr_stat;

		/* create the RMR */
		ret = dat_rmr_create(pz_handle, &bpool_ptr->rmr_handle);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "%s: dat_rmr_create failed %s\n",
					  module, DT_RetToString(ret));
			goto err;
		}

		/* bind the RMR */
		iov.virtual_address = bpool_ptr->reg_addr;
		iov.segment_length = bpool_ptr->reg_size;
		iov.lmr_context = bpool_ptr->lmr_context;
		cookie.as_64 = (DAT_UINT64) 0UL;
		cookie.as_ptr = (DAT_PVOID) (uintptr_t) bpool_ptr->reg_addr;
		mflags = (enable_rdma_write
			  && enable_rdma_read ? DAT_MEM_PRIV_ALL_FLAG
			  : (enable_rdma_write ? DAT_MEM_PRIV_WRITE_FLAG
			     : (enable_rdma_read ? DAT_MEM_PRIV_READ_FLAG :
				0)));

		DT_Tdep_PT_Debug(3, (phead, "rmr_bind [" F64x ", " F64x "]\n",
				     bpool_ptr->reg_addr, bpool_ptr->reg_size));

		ret = dat_rmr_bind(bpool_ptr->rmr_handle,
				   bpool_ptr->lmr_handle,
				   &iov,
				   mflags,
				   DAT_VA_TYPE_VA,
				   bpool_ptr->ep_handle,
				   cookie,
				   DAT_COMPLETION_DEFAULT_FLAG,
				   &bpool_ptr->rmr_context);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead, "%s: dat_rmr_bind failed %s\n",
					  module, DT_RetToString(ret));
			goto err;
		}

		DT_Tdep_PT_Debug(3, (phead, "rmr_bind-wait\n"));

		/* await the bind result */
		if (!DT_rmr_event_wait(phead,
				       bpool_ptr->rmr_evd_handle,
				       &rmr_stat) ||
		    !DT_rmr_check(phead,
				  &rmr_stat,
				  bpool_ptr->rmr_handle,
				  (DAT_PVOID) (uintptr_t) bpool_ptr->reg_addr,
				  "Bpool")) {
			goto err;
		}

		DT_Tdep_PT_Debug(3,
				 (phead, "rmr_bound [OK Rctx=%x]\n",
				  bpool_ptr->rmr_context));
	}

	/*
	 * Finally!  Return the newly created Bpool.
	 */
	return (bpool_ptr);

	/* *********************************
	 * Whoops - clean up and return NULL
	 */
      err:
	if (bpool_ptr) {
		if (bpool_ptr->rmr_handle) {
			ret = dat_rmr_free(bpool_ptr->rmr_handle);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_rmr_free failed %s\n",
						  module, DT_RetToString(ret));
			}
		}
		if (bpool_ptr->lmr_handle) {
			ret = dat_lmr_free(bpool_ptr->lmr_handle);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_lmr_free failed %s\n",
						  module, DT_RetToString(ret));
			}
		}
		DT_MemListFree(pt_ptr, bpool_ptr);
	}
	if (alloc_ptr) {
		DT_MemListFree(pt_ptr, alloc_ptr);
	}

	return (0);
}

/*****************************************************************************/
bool
DT_Bpool_Destroy(Per_Test_Data_t * pt_ptr,
		 DT_Tdep_Print_Head * phead, Bpool * bpool_ptr)
{
	char *module = "DT_Bpool_Destroy";
	bool rval = true;

	if (bpool_ptr) {
		if (bpool_ptr->alloc_ptr) {
			if (bpool_ptr->rmr_handle) {
				DAT_LMR_TRIPLET iov;
				DAT_RMR_COOKIE cookie;
				DAT_RETURN ret;

				iov.virtual_address = bpool_ptr->reg_addr;
				iov.segment_length = 0;	/* un-bind */
				iov.lmr_context = bpool_ptr->lmr_context;
				cookie.as_64 = (DAT_UINT64) 0UL;
				cookie.as_ptr =
				    (DAT_PVOID) (uintptr_t) bpool_ptr->reg_addr;

				/*
				 * Do not attempt to unbind here. The remote node
				 * is going through the same logic and may disconnect
				 * before an unbind completes. Any bind/unbind
				 * operation requires a CONNECTED QP to complete,
				 * a disconnect will cause problems. Unbind is
				 * a simple optimization to allow rebinding of
				 * an RMR, doing an rmr_free will pull the plug
				 * and cleanup properly.
				 */
				ret = dat_rmr_free(bpool_ptr->rmr_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_rmr_free failed %s\n",
							  module,
							  DT_RetToString(ret));
					rval = false;
				}
			}

			if (bpool_ptr->lmr_handle) {
				DAT_RETURN ret =
				    dat_lmr_free(bpool_ptr->lmr_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_lmr_free failed %s\n",
							  module,
							  DT_RetToString(ret));
					rval = false;
				}
			}
			DT_MemListFree(pt_ptr, bpool_ptr->alloc_ptr);
		}
		DT_MemListFree(pt_ptr, bpool_ptr);
	}

	return (rval);
}

/*****************************************************************************/
unsigned char *DT_Bpool_GetBuffer(Bpool * bpool_ptr, int index)
{
	return (bpool_ptr->buffer_start + index * bpool_ptr->seg_size);
}

/*****************************************************************************/
DAT_COUNT DT_Bpool_GetBuffSize(Bpool * bpool_ptr, int index)
{
	return (bpool_ptr->seg_size);
}

/*****************************************************************************/
DAT_LMR_TRIPLET *DT_Bpool_GetIOV(Bpool * bpool_ptr, int index)
{
	return (bpool_ptr->tripl_start + index);
}

/*****************************************************************************/
DAT_LMR_CONTEXT DT_Bpool_GetLMR(Bpool * bpool_ptr, int index)
{
	return (bpool_ptr->lmr_context);
}

/*****************************************************************************/
DAT_RMR_CONTEXT DT_Bpool_GetRMR(Bpool * bpool_ptr, int index)
{
	return (bpool_ptr->rmr_context);
}

/*****************************************************************************/
void DT_Bpool_print(DT_Tdep_Print_Head * phead, Bpool * bpool_ptr)
{
	DT_Tdep_PT_Printf(phead, "BPOOL                %p\n", bpool_ptr);
	DT_Tdep_PT_Printf(phead,
			  "BPOOL alloc_ptr      %p\n", bpool_ptr->alloc_ptr);
	DT_Tdep_PT_Printf(phead,
			  "BPOOL alloc_size     %x\n",
			  (int)bpool_ptr->alloc_size);
	DT_Tdep_PT_Printf(phead,
			  "BPOOL pz_handle      %p\n", bpool_ptr->pz_handle);
	DT_Tdep_PT_Printf(phead,
			  "BPOOL num_segs       %x\n",
			  (int)bpool_ptr->num_segs);
	DT_Tdep_PT_Printf(phead,
			  "BPOOL seg_size       %x\n",
			  (int)bpool_ptr->seg_size);
	DT_Tdep_PT_Printf(phead,
			  "BPOOL tripl_start    %p\n", bpool_ptr->tripl_start);
	DT_Tdep_PT_Printf(phead,
			  "BPOOL buffer_start   %p\n", bpool_ptr->buffer_start);
	DT_Tdep_PT_Printf(phead,
			  "BPOOL buffer_size    %x\n",
			  (int)bpool_ptr->buffer_size);
	DT_Tdep_PT_Printf(phead,
			  "BPOOL rdma_write     %x\n",
			  (int)bpool_ptr->enable_rdma_write);
	DT_Tdep_PT_Printf(phead,
			  "BPOOL rdmaread       %x\n",
			  (int)bpool_ptr->enable_rdma_read);
}
