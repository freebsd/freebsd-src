/*
 * Copyright (c) 2007, 2008 Chelsio, Inc. All rights reserved.
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
 * $FreeBSD: src/sys/dev/cxgb/ulp/iw_cxgb/iw_cxgb_user.h,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */
#ifndef __IWCH_USER_H__
#define __IWCH_USER_H__

#define IWCH_UVERBS_ABI_VERSION	1

/*
 * Make sure that all structs defined in this file remain laid out so
 * that they pack the same way on 32-bit and 64-bit architectures (to
 * avoid incompatibility between 32-bit userspace and 64-bit kernels).
 * In particular do not use pointer types -- pass pointers in uint64_t
 * instead.
 */
struct iwch_create_cq_req {
	uint64_t user_rptr_addr;
};

struct iwch_create_cq_resp {
	uint64_t key;
	uint32_t cqid;
	uint32_t size_log2;
};

struct iwch_create_qp_resp {
	uint64_t key;
	uint64_t db_key;
	uint32_t qpid;
	uint32_t size_log2;
	uint32_t sq_size_log2;
	uint32_t rq_size_log2;
};

struct iwch_reg_user_mr_resp {
	uint32_t pbl_addr;
};
#endif
