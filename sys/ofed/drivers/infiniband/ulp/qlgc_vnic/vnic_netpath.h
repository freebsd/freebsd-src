/*
 * Copyright (c) 2006 QLogic, Inc.  All rights reserved.
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
 */

#ifndef VNIC_NETPATH_H_INCLUDED
#define VNIC_NETPATH_H_INCLUDED

#include <linux/spinlock.h>

#include "vnic_sys.h"

struct viport;
struct vnic;

enum netpath_ts {
	NETPATH_TS_IDLE		= 0,
	NETPATH_TS_ACTIVE	= 1,
	NETPATH_TS_EXPIRED	= 2
};

struct netpath {
	int			carrier;
	struct vnic		*parent;
	struct viport		*viport;
	size_t			path_idx;
	unsigned long		connect_time;
	int			second_bias;
	u8			is_primary_path;
	u8 			delay_reconnect;
	struct timer_list	timer;
	enum netpath_ts		timer_state;
	struct dev_info		dev_info;
};

void netpath_init(struct netpath *netpath, struct vnic *vnic,
		  int second_bias);
void netpath_free(struct netpath *netpath);

void netpath_timer(struct netpath *netpath, int timeout);
void netpath_timer_stop(struct netpath *netpath);

const char *netpath_to_string(struct vnic *vnic, struct netpath *netpath);

#define netpath_get_hw_addr(netpath, address)		\
	viport_get_hw_addr((netpath)->viport, address)
#define netpath_is_connected(netpath)			\
	(netpath->state == NETPATH_CONNECTED)
#define netpath_can_tx_csum(netpath)			\
	viport_can_tx_csum(netpath->viport)

#endif	/* VNIC_NETPATH_H_INCLUDED */
