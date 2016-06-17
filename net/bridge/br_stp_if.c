/*
 *	Spanning tree protocol; interface code
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	$Id: br_stp_if.c,v 1.4 2001/04/14 21:14:39 davem Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/if_bridge.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include "br_private.h"
#include "br_private_stp.h"

__u16 br_make_port_id(struct net_bridge_port *p)
{
	return (p->priority << 8) | p->port_no;
}

/* called under bridge lock */
void br_init_port(struct net_bridge_port *p)
{
	p->port_id = br_make_port_id(p);
	br_become_designated_port(p);
	p->state = BR_STATE_BLOCKING;
	p->topology_change_ack = 0;
	p->config_pending = 0;
	br_timer_clear(&p->message_age_timer);
	br_timer_clear(&p->forward_delay_timer);
	br_timer_clear(&p->hold_timer);
}

/* called under bridge lock */
void br_stp_enable_bridge(struct net_bridge *br)
{
	struct net_bridge_port *p;
	struct timer_list *timer = &br->tick;

	init_timer(timer);
	timer->data = (unsigned long) br;
	timer->function = br_tick;
	timer->expires = jiffies + 1;
	add_timer(timer);

	br_timer_set(&br->hello_timer, jiffies);
	br_config_bpdu_generation(br);

	p = br->port_list;
	while (p != NULL) {
		if (p->dev->flags & IFF_UP)
			br_stp_enable_port(p);

		p = p->next;
	}

	br_timer_set(&br->gc_timer, jiffies);
}

/* called under bridge lock */
void br_stp_disable_bridge(struct net_bridge *br)
{
	struct net_bridge_port *p;

	br->topology_change = 0;
	br->topology_change_detected = 0;
	br_timer_clear(&br->hello_timer);
	br_timer_clear(&br->topology_change_timer);
	br_timer_clear(&br->tcn_timer);
	br_timer_clear(&br->gc_timer);
	br_fdb_cleanup(br);

	p = br->port_list;
	while (p != NULL) {
		if (p->state != BR_STATE_DISABLED)
			br_stp_disable_port(p);

		p = p->next;
	}

	del_timer(&br->tick);
}

/* called under bridge lock */
void br_stp_enable_port(struct net_bridge_port *p)
{
	br_init_port(p);
	br_port_state_selection(p->br);
}

/* called under bridge lock */
void br_stp_disable_port(struct net_bridge_port *p)
{
	struct net_bridge *br;
	int wasroot;

	br = p->br;
	printk(KERN_INFO "%s: port %i(%s) entering %s state\n",
	       br->dev.name, p->port_no, p->dev->name, "disabled");

	wasroot = br_is_root_bridge(br);
	br_become_designated_port(p);
	p->state = BR_STATE_DISABLED;
	p->topology_change_ack = 0;
	p->config_pending = 0;
	br_timer_clear(&p->message_age_timer);
	br_timer_clear(&p->forward_delay_timer);
	br_timer_clear(&p->hold_timer);
	br_configuration_update(br);
	br_port_state_selection(br);

	if (br_is_root_bridge(br) && !wasroot)
		br_become_root_bridge(br);
}

/* called under bridge lock */
static void br_stp_change_bridge_id(struct net_bridge *br, unsigned char *addr)
{
	unsigned char oldaddr[6];
	struct net_bridge_port *p;
	int wasroot;

	wasroot = br_is_root_bridge(br);

	memcpy(oldaddr, br->bridge_id.addr, ETH_ALEN);
	memcpy(br->bridge_id.addr, addr, ETH_ALEN);
	memcpy(br->dev.dev_addr, addr, ETH_ALEN);

	p = br->port_list;
	while (p != NULL) {
		if (!memcmp(p->designated_bridge.addr, oldaddr, ETH_ALEN))
			memcpy(p->designated_bridge.addr, addr, ETH_ALEN);

		if (!memcmp(p->designated_root.addr, oldaddr, ETH_ALEN))
			memcpy(p->designated_root.addr, addr, ETH_ALEN);

		p = p->next;
	}

	br_configuration_update(br);
	br_port_state_selection(br);
	if (br_is_root_bridge(br) && !wasroot)
		br_become_root_bridge(br);
}

static unsigned char br_mac_zero[6] = {0,0,0,0,0,0};

/* called under bridge lock */
void br_stp_recalculate_bridge_id(struct net_bridge *br)
{
	unsigned char *addr;
	struct net_bridge_port *p;

	addr = br_mac_zero;

	p = br->port_list;
	while (p != NULL) {
		if (addr == br_mac_zero ||
		    memcmp(p->dev->dev_addr, addr, ETH_ALEN) < 0)
			addr = p->dev->dev_addr;

		p = p->next;
	}

	if (memcmp(br->bridge_id.addr, addr, ETH_ALEN))
		br_stp_change_bridge_id(br, addr);
}

/* called under bridge lock */
void br_stp_set_bridge_priority(struct net_bridge *br, int newprio)
{
	struct net_bridge_port *p;
	int wasroot;

	wasroot = br_is_root_bridge(br);

	p = br->port_list;
	while (p != NULL) {
		if (p->state != BR_STATE_DISABLED &&
		    br_is_designated_port(p)) {
			p->designated_bridge.prio[0] = (newprio >> 8) & 0xFF;
			p->designated_bridge.prio[1] = newprio & 0xFF;
		}

		p = p->next;
	}

	br->bridge_id.prio[0] = (newprio >> 8) & 0xFF;
	br->bridge_id.prio[1] = newprio & 0xFF;
	br_configuration_update(br);
	br_port_state_selection(br);
	if (br_is_root_bridge(br) && !wasroot)
		br_become_root_bridge(br);
}

/* called under bridge lock */
void br_stp_set_port_priority(struct net_bridge_port *p, int newprio)
{
	__u16 new_port_id;

	p->priority = newprio & 0xFF;
	new_port_id = br_make_port_id(p);

	if (br_is_designated_port(p))
		p->designated_port = new_port_id;

	p->port_id = new_port_id;
	if (!memcmp(&p->br->bridge_id, &p->designated_bridge, 8) &&
	    p->port_id < p->designated_port) {
		br_become_designated_port(p);
		br_port_state_selection(p->br);
	}
}

/* called under bridge lock */
void br_stp_set_path_cost(struct net_bridge_port *p, int path_cost)
{
	p->path_cost = path_cost;
	br_configuration_update(p->br);
	br_port_state_selection(p->br);
}
