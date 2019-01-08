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

#ifndef __DAPL_BPOOL_H__
#define __DAPL_BPOOL_H__

#include "dapl_proto.h"

typedef struct Bpool_tag Bpool;
/*
 * struct Bpool
 */

struct Bpool_tag
{
    unsigned char  *alloc_ptr;
    DAT_UINT32      alloc_size;
    DAT_PZ_HANDLE   pz_handle;
    DAT_COUNT       seg_size;
    DAT_COUNT       num_segs;		/* num segments */
    unsigned char  *buffer_start;	/* Start of buffer area */
    DAT_VLEN        buffer_size;	/* Size of data buffer (rounded) */
    DAT_VADDR	    reg_addr;		/* start of registered area */
    DAT_VLEN        reg_size;		/* size of registered area */
    DAT_EP_HANDLE   ep_handle;		/* EP area is registered to */
    DAT_LMR_HANDLE  lmr_handle;		/* local access */
    DAT_LMR_CONTEXT lmr_context;
    DAT_LMR_TRIPLET*tripl_start;	/* local IOV */
    DAT_BOOLEAN     enable_rdma_write;	/* remote access */
    DAT_BOOLEAN     enable_rdma_read;
    DAT_RMR_HANDLE  rmr_handle;
    DAT_RMR_CONTEXT rmr_context;
    DAT_EVD_HANDLE  rmr_evd_handle;
};
#endif
