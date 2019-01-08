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

#ifndef __DAPL_TRANSACTION_TEST_H__
#define __DAPL_TRANSACTION_TEST_H__

#include "dapl_proto.h"
#include "dapl_common.h"
#include "dapl_test_data.h"
#include "dapl_transaction_cmd.h"

typedef struct
{
    DAT_BOOLEAN             server_initiated;
    DT_Transfer_Type        transfer_type;
    DAT_UINT32              num_segs;
    DAT_UINT32              seg_size;
    DAT_BOOLEAN             reap_send_on_recv;
    Bpool                   *bp;

    /* RDMA info */
    DAT_RMR_CONTEXT         Rdma_Context;
    DAT_VADDR               Rdma_Address;
} Transaction_Test_Op_t;

typedef struct
{
    DAT_EP_HANDLE           ep_handle;
    DAT_EP_ATTR		    ep_attr;
    DAT_CONN_QUAL           ia_port;
    Bpool                   *bp;
    Transaction_Test_Op_t   op[ MAX_OPS ];
    DAT_RSP_HANDLE          rsp_handle;
    DAT_PSP_HANDLE          psp_handle;
    DAT_EVD_HANDLE          recv_evd_hdl;   /* receive	    */
    DAT_EVD_HANDLE          reqt_evd_hdl;   /* request+rmr  */
    DAT_EVD_HANDLE          conn_evd_hdl;   /* connect	    */
    DAT_EVD_HANDLE          creq_evd_hdl;   /* "" request   */

} Ep_Context_t;

typedef struct
{
    unsigned int            stat_bytes_send;
    unsigned int            stat_bytes_recv;
    unsigned int            stat_bytes_rdma_read;
    unsigned int            stat_bytes_rdma_write;
    unsigned int            start_time;
    unsigned int            end_time;
} Transaction_Test_Stats_t;

typedef struct
{
    /* This group set up by DT_Transaction_Create_Test()   */
    DAT_BOOLEAN             is_server;
    DAT_BOOLEAN             remote_is_little_endian;
    Per_Test_Data_t         *pt_ptr;
    DAT_IA_HANDLE           ia_handle;
    Transaction_Cmd_t       *cmd;
    DAT_IA_ADDRESS_PTR      remote_ia_addr;
    DAT_CONN_QUAL           base_port;
    DAT_TIMEOUT             time_out;
    DAT_COUNT               evd_length;
    Thread                  *thread;

    /* This group set up by each thread in DT_Transaction_Main() */
    DAT_PZ_HANDLE           pz_handle;
    Ep_Context_t            *ep_context;

    /* Statistics set by DT_Transaction_Run() */
    Transaction_Test_Stats_t 	stats;
} Transaction_Test_t;

#endif
