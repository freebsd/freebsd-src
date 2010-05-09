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

#include <linux/types.h>
#include <linux/device.h>
#include <linux/kernel.h>

#include "vnic_main.h"

cycles_t vnic_recv_ref;

/*
 * TODO: Statistics reporting for control path, data path,
 *       RDMA times, IOs etc
 *
 */
static ssize_t show_lifetime(struct device *dev,
			     struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, stat_info);
	cycles_t time = get_cycles() - vnic->statistics.start_time;

	return sprintf(buf, "%llu\n", (unsigned long long)time);
}

static DEVICE_ATTR(lifetime, S_IRUGO, show_lifetime, NULL);

static ssize_t show_conntime(struct device *dev,
			     struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, stat_info);

	if (vnic->statistics.conn_time)
		return sprintf(buf, "%llu\n",
			   (unsigned long long)vnic->statistics.conn_time);
	return 0;
}

static DEVICE_ATTR(connection_time, S_IRUGO, show_conntime, NULL);

static ssize_t show_disconnects(struct device *dev,
				struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, stat_info);
	u32 num;

	if (vnic->statistics.disconn_ref)
		num = vnic->statistics.disconn_num + 1;
	else
		num = vnic->statistics.disconn_num;

	return sprintf(buf, "%d\n", num);
}

static DEVICE_ATTR(disconnects, S_IRUGO, show_disconnects, NULL);

static ssize_t show_total_disconn_time(struct device *dev,
				       struct device_attribute *dev_attr,
				       char *buf)
{
	struct dev_info *info = container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, stat_info);
	cycles_t time;

	if (vnic->statistics.disconn_ref)
		time = vnic->statistics.disconn_time +
		       get_cycles() - vnic->statistics.disconn_ref;
	else
		time = vnic->statistics.disconn_time;

	return sprintf(buf, "%llu\n", (unsigned long long)time);
}

static DEVICE_ATTR(total_disconn_time, S_IRUGO, show_total_disconn_time, NULL);

static ssize_t show_carrier_losses(struct device *dev,
				   struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, stat_info);
	u32 num;

	if (vnic->statistics.carrier_ref)
		num = vnic->statistics.carrier_off_num + 1;
	else
		num = vnic->statistics.carrier_off_num;

	return sprintf(buf, "%d\n", num);
}

static DEVICE_ATTR(carrier_losses, S_IRUGO, show_carrier_losses, NULL);

static ssize_t show_total_carr_loss_time(struct device *dev,
					 struct device_attribute *dev_attr,
					 char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, stat_info);
	cycles_t time;

	if (vnic->statistics.carrier_ref)
		time = vnic->statistics.carrier_off_time +
		       get_cycles() - vnic->statistics.carrier_ref;
	else
		time = vnic->statistics.carrier_off_time;

	return sprintf(buf, "%llu\n", (unsigned long long)time);
}

static DEVICE_ATTR(total_carrier_loss_time, S_IRUGO,
			 show_total_carr_loss_time, NULL);

static ssize_t show_total_recv_time(struct device *dev,
				    struct device_attribute *dev_attr,
				    char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, stat_info);

	return sprintf(buf, "%llu\n",
		       (unsigned long long)vnic->statistics.recv_time);
}

static DEVICE_ATTR(total_recv_time, S_IRUGO, show_total_recv_time, NULL);

static ssize_t show_recvs(struct device *dev,
			  struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, stat_info);

	return sprintf(buf, "%d\n", vnic->statistics.recv_num);
}

static DEVICE_ATTR(recvs, S_IRUGO, show_recvs, NULL);

static ssize_t show_multicast_recvs(struct device *dev,
				    struct device_attribute *dev_attr,
				    char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, stat_info);

	return sprintf(buf, "%d\n", vnic->statistics.multicast_recv_num);
}

static DEVICE_ATTR(multicast_recvs, S_IRUGO, show_multicast_recvs, NULL);

static ssize_t show_total_xmit_time(struct device *dev,
				    struct device_attribute *dev_attr,
				    char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, stat_info);

	return sprintf(buf, "%llu\n",
		       (unsigned long long)vnic->statistics.xmit_time);
}

static DEVICE_ATTR(total_xmit_time, S_IRUGO, show_total_xmit_time, NULL);

static ssize_t show_xmits(struct device *dev,
			  struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, stat_info);

	return sprintf(buf, "%d\n", vnic->statistics.xmit_num);
}

static DEVICE_ATTR(xmits, S_IRUGO, show_xmits, NULL);

static ssize_t show_failed_xmits(struct device *dev,
				 struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, stat_info);

	return sprintf(buf, "%d\n", vnic->statistics.xmit_fail);
}

static DEVICE_ATTR(failed_xmits, S_IRUGO, show_failed_xmits, NULL);

static struct attribute *vnic_stats_attrs[] = {
	&dev_attr_lifetime.attr,
	&dev_attr_xmits.attr,
	&dev_attr_total_xmit_time.attr,
	&dev_attr_failed_xmits.attr,
	&dev_attr_recvs.attr,
	&dev_attr_multicast_recvs.attr,
	&dev_attr_total_recv_time.attr,
	&dev_attr_connection_time.attr,
	&dev_attr_disconnects.attr,
	&dev_attr_total_disconn_time.attr,
	&dev_attr_carrier_losses.attr,
	&dev_attr_total_carrier_loss_time.attr,
	NULL
};

struct attribute_group vnic_stats_attr_group = {
	.attrs = vnic_stats_attrs,
};
