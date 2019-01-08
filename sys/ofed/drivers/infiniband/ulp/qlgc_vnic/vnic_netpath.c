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

#include <linux/netdevice.h>
#include <linux/skbuff.h>

#include "vnic_util.h"
#include "vnic_main.h"
#include "vnic_viport.h"
#include "vnic_netpath.h"

static void vnic_npevent_timeout(unsigned long data)
{
	struct netpath *netpath = (struct netpath *)data;

	if (netpath->second_bias)
		vnic_npevent_queue_evt(netpath, VNIC_SECNP_TIMEREXPIRED);
	else
		vnic_npevent_queue_evt(netpath, VNIC_PRINP_TIMEREXPIRED);
}

void netpath_timer(struct netpath *netpath, int timeout)
{
	if (netpath->timer_state == NETPATH_TS_ACTIVE)
		del_timer_sync(&netpath->timer);
	if (timeout) {
		init_timer(&netpath->timer);
		netpath->timer_state = NETPATH_TS_ACTIVE;
		netpath->timer.expires = jiffies + timeout;
		netpath->timer.data = (unsigned long)netpath;
		netpath->timer.function = vnic_npevent_timeout;
		add_timer(&netpath->timer);
	} else
		vnic_npevent_timeout((unsigned long)netpath);
}

void netpath_timer_stop(struct netpath *netpath)
{
	if (netpath->timer_state != NETPATH_TS_ACTIVE)
		return;
	del_timer_sync(&netpath->timer);
	if (netpath->second_bias)
		vnic_npevent_dequeue_evt(netpath, VNIC_SECNP_TIMEREXPIRED);
	else
		vnic_npevent_dequeue_evt(netpath, VNIC_PRINP_TIMEREXPIRED);

	netpath->timer_state = NETPATH_TS_IDLE;
}

void netpath_free(struct netpath *netpath)
{
	if (!netpath->viport)
		return;
	viport_free(netpath->viport);
	netpath->viport = NULL;
	sysfs_remove_group(&netpath->dev_info.dev.kobj,
			   &vnic_path_attr_group);
	device_unregister(&netpath->dev_info.dev);
	wait_for_completion(&netpath->dev_info.released);
}

void netpath_init(struct netpath *netpath, struct vnic *vnic,
		  int second_bias)
{
	netpath->parent = vnic;
	netpath->carrier = 0;
	netpath->viport = NULL;
	netpath->second_bias = second_bias;
	netpath->timer_state = NETPATH_TS_IDLE;
	init_timer(&netpath->timer);
}

const char *netpath_to_string(struct vnic *vnic, struct netpath *netpath)
{
	if (!netpath)
		return "NULL";
	else if (netpath == &vnic->primary_path)
		return "PRIMARY";
	else if (netpath == &vnic->secondary_path)
		return "SECONDARY";
	else
		return "UNKNOWN";
}
