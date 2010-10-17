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

#ifndef DAPL_FFT_UTIL_H
#define DAPL_FFT_UTIL_H

#include "dapl_proto.h"
#include "dapl_test_data.h"

#define DT_assert_dat(test_num, x)   if(x) ; \
    else { \
	DT_assert_fail(test_num, #x, __FILE__, __BASE_FILE__, __LINE__); \
	DT_Tdep_PT_Printf(test_num,"Error = %d, %s\n", rc, DT_RetToString(rc)); \
	res = 0; \
	goto cleanup; \
    }

#define DT_assert(test_num, x)   if(x) ; \
    else { \
	DT_assert_fail(test_num, #x, __FILE__, __BASE_FILE__, __LINE__); \
	res = 0; \
	goto cleanup; \
    }

#define DT_assert_clean(test_num, x)  if(x) ; \
    else { \
	DT_assert_fail(test_num, #x, __FILE__, __BASE_FILE__, __LINE__); \
	DT_Tdep_PT_Printf(test_num,"Error = %d, %s\n", rc, DT_RetToString(rc)); \
	return 0; \
    }

typedef struct
{
    DAT_IA_HANDLE ia_handle;
    DAT_PZ_HANDLE pz_handle;
    DAT_PSP_HANDLE psp_handle;
    DAT_EP_HANDLE ep_handle;
    DAT_EVD_HANDLE cr_evd, conn_evd, send_evd, recv_evd;
    DAT_EVENT event;
    DAT_COUNT count;
    DAT_CR_HANDLE cr_handle;
    Bpool *bpool;
    DAT_CR_ARRIVAL_EVENT_DATA cr_stat;
    DAT_EVENT_NUMBER event_num;
    DAT_IA_ADDRESS_PTR remote_netaddr;
    Per_Test_Data_t *pt_ptr;
    bool connected;
} FFT_Connection_t;

typedef enum
{
    QUERY_CNO,
    QUERY_CR,
    QUERY_EP,
    QUERY_EVD,
    QUERY_IA,
    QUERY_LMR,
    QUERY_RMR,
    QUERY_PSP,
    QUERY_RSP,
    QUERY_PZ,
} FFT_query_enum;

typedef struct
{
    int	(*fun) (Params_t *params_ptr, FFT_Cmd_t* cmd);
} FFT_Testfunc_t;

#endif

