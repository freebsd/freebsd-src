/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2015 - 2022 Intel Corporation
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

#ifndef IRDMA_WS_H
#define IRDMA_WS_H

#include "osdep.h"

enum irdma_ws_node_type {
	WS_NODE_TYPE_PARENT,
	WS_NODE_TYPE_LEAF,
};

enum irdma_ws_match_type {
	WS_MATCH_TYPE_VSI,
	WS_MATCH_TYPE_TC,
};

struct irdma_ws_node {
	struct list_head siblings;
	struct list_head child_list_head;
	struct irdma_ws_node *parent;
	u32 l2_sched_node_id;
	u16 index;
	u16 qs_handle;
	u16 vsi_index;
	u8 traffic_class;
	u8 user_pri;
	u8 rel_bw;
	u8 abstraction_layer; /* used for splitting a TC */
	u8 prio_type;
	bool type_leaf:1;
	bool enable:1;
};

struct irdma_sc_vsi;
int irdma_ws_add(struct irdma_sc_vsi *vsi, u8 user_pri);
void irdma_ws_remove(struct irdma_sc_vsi *vsi, u8 user_pri);
void irdma_ws_reset(struct irdma_sc_vsi *vsi);

#endif /* IRDMA_WS_H */
