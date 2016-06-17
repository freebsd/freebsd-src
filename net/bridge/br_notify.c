/*
 *	Device event handling
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	$Id: br_notify.c,v 1.2 2000/02/21 15:51:34 davem Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/if_bridge.h>
#include "br_private.h"

static int br_device_event(struct notifier_block *unused, unsigned long event, void *ptr);

struct notifier_block br_device_notifier =
{
	br_device_event,
	NULL,
	0
};

static int br_device_event(struct notifier_block *unused, unsigned long event, void *ptr)
{
	struct net_device *dev;
	struct net_bridge_port *p;

	dev = ptr;
	p = dev->br_port;

	if (p == NULL)
		return NOTIFY_DONE;

	switch (event)
	{
	case NETDEV_CHANGEADDR:
		read_lock(&p->br->lock);
		br_fdb_changeaddr(p, dev->dev_addr);
		br_stp_recalculate_bridge_id(p->br);
		read_unlock(&p->br->lock);
		break;

	case NETDEV_GOING_DOWN:
		/* extend the protocol to send some kind of notification? */
		break;

	case NETDEV_DOWN:
		if (p->br->dev.flags & IFF_UP) {
			read_lock(&p->br->lock);
			br_stp_disable_port(dev->br_port);
			read_unlock(&p->br->lock);
		}
		break;

	case NETDEV_UP:
		if (p->br->dev.flags & IFF_UP) {
			read_lock(&p->br->lock);
			br_stp_enable_port(dev->br_port);
			read_unlock(&p->br->lock);
		}
		break;

	case NETDEV_UNREGISTER:
		br_del_if(dev->br_port->br, dev);
		break;
	}

	return NOTIFY_DONE;
}
