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

#ifndef I40IW_HW_H
#define I40IW_HW_H

enum i40iw_device_caps_const {
	I40IW_MAX_WQ_FRAGMENT_COUNT		= 3,
	I40IW_MAX_SGE_RD			= 1,
	I40IW_MAX_PUSH_PAGE_COUNT		= 0,
	I40IW_MAX_INLINE_DATA_SIZE		= 48,
	I40IW_MAX_IRD_SIZE			= 64,
	I40IW_MAX_ORD_SIZE			= 64,
	I40IW_MAX_WQ_ENTRIES			= 2048,
	I40IW_MAX_WQE_SIZE_RQ			= 128,
	I40IW_MAX_PDS				= 32768,
	I40IW_MAX_STATS_COUNT			= 16,
	I40IW_MAX_CQ_SIZE			= 1048575,
	I40IW_MAX_OUTBOUND_MSG_SIZE		= 2147483647,
	I40IW_MAX_INBOUND_MSG_SIZE		= 2147483647,
	I40IW_MIN_WQ_SIZE			= 4 /* WQEs */,
};

#define I40IW_QP_WQE_MIN_SIZE   32
#define I40IW_QP_WQE_MAX_SIZE   128
#define I40IW_MAX_RQ_WQE_SHIFT  2
#define I40IW_MAX_QUANTA_PER_WR 2

#define I40IW_QP_SW_MAX_SQ_QUANTA 2048
#define I40IW_QP_SW_MAX_RQ_QUANTA 16384
#define I40IW_QP_SW_MAX_WQ_QUANTA 2048
#endif /* I40IW_HW_H */
