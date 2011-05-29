/*
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
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
 *
 */

#ifndef __OSMT_INFORM__
#define __OSMT_INFORM__

#ifdef OSM_VENDOR_INTF_MTL
#include <vendor/osm_vendor_mlx_inout.h>
#include <ib_mgt.h>
#include "osmt_mtl_regular_qp.h"
#endif

typedef struct _osmt_qp_ctx {
#ifdef OSM_VENDOR_INTF_MTL
	osmt_mtl_mad_res_t qp_bind_hndl;
#endif
	uint8_t *p_send_buf;
	uint8_t *p_recv_buf;
#ifdef OSM_VENDOR_INTF_MTL
	IB_MGT_mad_hndl_t ib_mgt_qp0_handle;
#endif
} osmt_qp_ctx_t;

ib_api_status_t
osmt_bind_inform_qp(IN osmtest_t * const p_osmt, OUT osmt_qp_ctx_t * p_qp_ctx);

void
osmt_unbind_inform_qp(IN osmtest_t * const p_osmt, IN osmt_qp_ctx_t * p_qp_ctx);

ib_api_status_t
osmt_reg_unreg_inform_info(IN osmtest_t * p_osmt,
			   IN osmt_qp_ctx_t * p_qp_ctx,
			   IN ib_inform_info_t * p_inform_info,
			   IN uint8_t reg_flag);

ib_api_status_t
osmt_trap_wait(IN osmtest_t * const p_osmt, IN osmt_qp_ctx_t * p_qp_ctx);

ib_api_status_t
osmt_init_inform_info(IN osmtest_t * const p_osmt, OUT ib_inform_info_t * p_ii);

ib_api_status_t
osmt_init_inform_info_by_trap(IN osmtest_t * const p_osmt,
			      IN ib_net16_t trap_num,
			      OUT ib_inform_info_t * p_ii);

#endif				/* __OSMT_INFORM__ */
