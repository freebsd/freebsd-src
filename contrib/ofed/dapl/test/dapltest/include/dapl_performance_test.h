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

#ifndef __DAPL_PERFORMANCE_TEST_H__
#define __DAPL_PERFORMANCE_TEST_H__

#include "dapl_proto.h"
#include "dapl_tdep.h"
#include "dapl_common.h"
#include "dapl_test_data.h"
#include "dapl_performance_cmd.h"


#define DT_PERF_SYNC_SEND_BUFFER_ID 	0
#define DT_PERF_SYNC_RECV_BUFFER_ID  	1
#define DT_PERF_SYNC_BUFF_SIZE       	sizeof (RemoteMemoryInfo)
#define DT_PERF_DFLT_EVD_LENGTH		8


typedef struct
{
    DT_Transfer_Type        transfer_type;
    DAT_UINT32              num_segs;
    DAT_UINT32              seg_size;
    Bpool                   *bp;

    /* RDMA info */
    DAT_RMR_CONTEXT         Rdma_Context;
    DAT_VADDR               Rdma_Address;
} Performance_Test_Op_t;

typedef struct
{
    DAT_EP_HANDLE           ep_handle;
    DAT_EP_ATTR		    ep_attr;
    DAT_CONN_QUAL           port;
    DAT_COUNT               pipeline_len;
    Bpool                   *bp;
    Performance_Test_Op_t   op;
} Performance_Ep_Context_t;

typedef struct
{
    Per_Test_Data_t         	*pt_ptr;
    Performance_Cmd_t           *cmd;
    DAT_IA_ADDRESS_PTR      	remote_ia_addr;
    DAT_BOOLEAN             	is_remote_little_endian;
    DAT_CONN_QUAL           	base_port;
    DAT_IA_ATTR			ia_attr;
    DAT_IA_HANDLE           	ia_handle;
    DAT_PZ_HANDLE           	pz_handle;
    DAT_CNO_HANDLE          	cno_handle;
    DAT_COUNT               	reqt_evd_length;
    DAT_EVD_HANDLE          	reqt_evd_hdl;	/* request+rmr  */
    DAT_COUNT               	recv_evd_length;
    DAT_EVD_HANDLE          	recv_evd_hdl;	/* receive	*/
    DAT_COUNT               	conn_evd_length;
    DAT_EVD_HANDLE          	conn_evd_hdl;	/* connect	*/
    DAT_COUNT               	creq_evd_length;
    DAT_EVD_HANDLE          	creq_evd_hdl;	/* "" request   */
    Performance_Ep_Context_t 	ep_context;
} Performance_Test_t;

#endif
