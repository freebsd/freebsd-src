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

#ifndef __DAPL_TEST_DATA_H__
#define __DAPL_TEST_DATA_H__

#include "dapl_proto.h"
#include "dapl_bpool.h"
#include "dapl_client_info.h"
#include "dapl_transaction_stats.h"
#include "dapl_memlist.h"
#include "dapl_params.h"
#include "dapl_server_info.h"

/* This lock allows the client side to run
 * in a shell script loop without breaking
 * connections.  Remove it and due to timing
 * problems on the server side occasionally
 * the server will reject connections.
 */
extern	 	DT_Mdep_LockType    g_PerfTestLock;

/*
 * check memory leaking extern int              alloc_count ; extern
 * DT_Mdep_LockType        Alloc_Count_Lock;
 */

typedef struct
{
    int             	NextPortNumber;
    int             	num_clients;
    DT_Mdep_LockType   	num_clients_lock;
    DAT_IA_HANDLE   	ia_handle;
    DAT_PZ_HANDLE   	pz_handle;
    DAT_EVD_HANDLE  	recv_evd_hdl;
    DAT_EVD_HANDLE  	reqt_evd_hdl;
    DAT_EVD_HANDLE  	conn_evd_hdl;
    DAT_EVD_HANDLE  	creq_evd_hdl;
    DAT_EVD_HANDLE  	async_evd_hdl;
    DAT_EVD_HANDLE  	rmr_evd_hdl;
    DAT_EP_HANDLE   	ep_handle;
    DAT_PSP_HANDLE  	psp_handle;
    Bpool          	*bpool;
} Per_Server_Data_t;

typedef struct
{
    DT_Mdep_LockType   	MemListLock;
    MemListEntry_t	*MemListHead;

    DT_Mdep_LockType  	Thread_counter_lock;
    int             	Thread_counter;
    Thread         	*thread;

    bool            	local_is_server;
    Server_Info_t   	Server_Info;
    Client_Info_t   	Client_Info;
    Params_t        	Params;
    DAT_IA_ATTR         ia_attr;
    DAT_PROVIDER_ATTR   provider_attr;
    DAT_EP_ATTR		ep_attr;
    Per_Server_Data_t   *ps_ptr;
    Transaction_Stats_t Client_Stats;

    /* synchronize the server with the server's spawned test thread.
     * That test thread uses a PSP that only one test at a time can
     * use.  If we don't synchronize access between the teardown and
     * creation of that PSP then the client will fail to connect
     * randomly, a symptom that the server is not coordinated with
     * its test threads.  Remove this at your own peril, or if you
     * really want your test client to experience rejection on a
     * random but regular basis.
     */
    DT_WAIT_OBJECT	synch_wait_object;
    int             	Countdown_Counter;

} Per_Test_Data_t;

#endif
