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

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/net.h>

#include "vnic_util.h"
#include "vnic_main.h"
#include "vnic_viport.h"
#include "vnic_netpath.h"
#include "vnic_control.h"
#include "vnic_data.h"
#include "vnic_config.h"
#include "vnic_control_pkt.h"

#define VIPORT_DISCONN_TIMER	10000 	 /* 10 seconds */

#define MAX_RETRY_INTERVAL 	  20000  /* 20 seconds */
#define RETRY_INCREMENT		  5000   /* 5 seconds  */
#define MAX_CONNECT_RETRY_TIMEOUT 600000 /* 10 minutes */

static DECLARE_WAIT_QUEUE_HEAD(viport_queue);
static LIST_HEAD(viport_list);
static DECLARE_COMPLETION(viport_thread_exit);
static spinlock_t viport_list_lock;

static struct task_struct *viport_thread;
static int viport_thread_end;

static void viport_timer(struct viport *viport, int timeout);

struct viport *viport_allocate(struct viport_config *config)
{
	struct viport *viport;

	VIPORT_FUNCTION("viport_allocate()\n");
	viport = kzalloc(sizeof *viport, GFP_KERNEL);
	if (!viport) {
		VIPORT_ERROR("failed allocating viport structure\n");
		return NULL;
	}

	viport->state = VIPORT_DISCONNECTED;
	viport->link_state = LINK_FIRSTCONNECT;
	viport->connect = WAIT;
	viport->new_mtu = 1500;
	viport->new_flags = 0;
	viport->config = config;
	viport->connect = DELAY;
	viport->data.max_mtu = vnic_max_mtu;
	spin_lock_init(&viport->lock);
	init_waitqueue_head(&viport->stats_queue);
	init_waitqueue_head(&viport->disconnect_queue);
	init_waitqueue_head(&viport->reference_queue);
	INIT_LIST_HEAD(&viport->list_ptrs);

	vnic_mc_init(viport);

	return viport;
}

void viport_connect(struct viport *viport, int delay)
{
	VIPORT_FUNCTION("viport_connect()\n");

	if (viport->connect != DELAY)
		viport->connect = (delay) ? DELAY : NOW;
	if (viport->link_state == LINK_FIRSTCONNECT) {
		u32 duration;
		duration = (net_random() & 0x1ff);
		if (!viport->parent->is_primary_path)
			duration += 0x1ff;
		viport->link_state = LINK_RETRYWAIT;
		viport_timer(viport, duration);
	} else
		viport_kick(viport);
}

static void viport_disconnect(struct viport *viport)
{
	VIPORT_FUNCTION("viport_disconnect()\n");
	viport->disconnect = 1;
	viport_failure(viport);
	wait_event(viport->disconnect_queue, viport->disconnect == 0);
}

void viport_free(struct viport *viport)
{
	VIPORT_FUNCTION("viport_free()\n");
	viport_disconnect(viport);	/* NOTE: this can sleep */
	vnic_mc_uninit(viport);
	kfree(viport->config);
	kfree(viport);
}

void viport_set_link(struct viport *viport, u16 flags, u16 mtu)
{
	unsigned long localflags;
	int i;

	VIPORT_FUNCTION("viport_set_link()\n");
	if (mtu > data_max_mtu(&viport->data)) {
		VIPORT_ERROR("configuration error."
			     " mtu of %d unsupported by %s\n", mtu,
			     config_viport_name(viport->config));
		goto failure;
	}

	spin_lock_irqsave(&viport->lock, localflags);
	flags &= IFF_UP | IFF_ALLMULTI | IFF_PROMISC;
	if ((viport->new_flags != flags)
	    || (viport->new_mtu != mtu)) {
		viport->new_flags = flags;
		viport->new_mtu = mtu;
		viport->updates |= NEED_LINK_CONFIG;
		if (viport->features_supported & VNIC_FEAT_INBOUND_IB_MC) {
			if (((viport->mtu <= MCAST_MSG_SIZE) && (mtu >  MCAST_MSG_SIZE)) ||
			    ((viport->mtu >  MCAST_MSG_SIZE) && (mtu <= MCAST_MSG_SIZE))) {
			/*
			 * MTU value will enable/disable the multicast. In
			 * either case, need to send the CMD_CONFIG_ADDRESS2 to
			 * EVIC. Hence, setting the NEED_ADDRESS_CONFIG flag.
			 */
				viport->updates |= NEED_ADDRESS_CONFIG;
				if (mtu <= MCAST_MSG_SIZE) {
				    VIPORT_PRINT("%s: MTU changed; "
						"old:%d new:%d (threshold:%d);"
						" MULTICAST will be enabled.\n",
						config_viport_name(viport->config),
						viport->mtu, mtu,
						(int)MCAST_MSG_SIZE);
				} else {
				    VIPORT_PRINT("%s: MTU changed; "
						"old:%d new:%d (threshold:%d); "
						"MULTICAST will be disabled.\n",
						config_viport_name(viport->config),
						viport->mtu, mtu,
						(int)MCAST_MSG_SIZE);
				}
				/* When we resend these addresses, EVIC will
				 * send mgid=0 back in response. So no need to
				 * shutoff ib_multicast.
				 */
				for (i = MCAST_ADDR_START; i < viport->num_mac_addresses; i++) {
					if (viport->mac_addresses[i].valid)
						viport->mac_addresses[i].operation = VNIC_OP_SET_ENTRY;
				}
			}
		}
		viport_kick(viport);
	}

	spin_unlock_irqrestore(&viport->lock, localflags);
	return;
failure:
	viport_failure(viport);
}

int viport_set_unicast(struct viport *viport, u8 *address)
{
	unsigned long flags;
	int	ret = -1;
	VIPORT_FUNCTION("viport_set_unicast()\n");
	spin_lock_irqsave(&viport->lock, flags);

	if (!viport->mac_addresses)
		goto out;

	if (memcmp(viport->mac_addresses[UNICAST_ADDR].address,
		   address, ETH_ALEN)) {
		memcpy(viport->mac_addresses[UNICAST_ADDR].address,
		       address, ETH_ALEN);
		viport->mac_addresses[UNICAST_ADDR].operation
		    = VNIC_OP_SET_ENTRY;
		viport->updates |= NEED_ADDRESS_CONFIG;
		viport_kick(viport);
	}
	ret = 0;
out:
	spin_unlock_irqrestore(&viport->lock, flags);
	return ret;
}

int viport_set_multicast(struct viport *viport,
			 struct dev_mc_list *mc_list, int mc_count)
{
	u32 old_update_list;
	int i;
	int ret = -1;
	unsigned long flags;

	VIPORT_FUNCTION("viport_set_multicast()\n");
	spin_lock_irqsave(&viport->lock, flags);

	if (!viport->mac_addresses)
		goto out;

	old_update_list = viport->updates;
	if (mc_count > viport->num_mac_addresses - MCAST_ADDR_START)
		viport->updates |= NEED_LINK_CONFIG | MCAST_OVERFLOW;
	else {
		if (mc_count == 0) {
			ret = 0;
			goto out;
		}
		if (viport->updates & MCAST_OVERFLOW) {
			viport->updates &= ~MCAST_OVERFLOW;
			viport->updates |= NEED_LINK_CONFIG;
		}
		for (i = MCAST_ADDR_START; i < mc_count + MCAST_ADDR_START;
						i++, mc_list = mc_list->next) {
			if (viport->mac_addresses[i].valid &&
				!memcmp(viport->mac_addresses[i].address,
						mc_list->dmi_addr, ETH_ALEN))
			continue;
		memcpy(viport->mac_addresses[i].address,
					 mc_list->dmi_addr, ETH_ALEN);
		viport->mac_addresses[i].valid = 1;
		viport->mac_addresses[i].operation = VNIC_OP_SET_ENTRY;
	}
	for (; i < viport->num_mac_addresses; i++) {
		if (!viport->mac_addresses[i].valid)
			continue;
		viport->mac_addresses[i].valid = 0;
		viport->mac_addresses[i].operation = VNIC_OP_SET_ENTRY;
	}
	if (mc_count)
		viport->updates |= NEED_ADDRESS_CONFIG;
	}

	if (viport->updates != old_update_list)
		viport_kick(viport);
	ret = 0;
out:
	spin_unlock_irqrestore(&viport->lock, flags);
	return ret;
}

static inline void viport_disable_multicast(struct viport *viport)
{
	VIPORT_INFO("turned off IB_MULTICAST\n");
	viport->config->control_config.ib_multicast = 0;
	viport->config->control_config.ib_config.conn_data.features_supported &=
				__constant_cpu_to_be32((u32)~VNIC_FEAT_INBOUND_IB_MC);
	viport->link_state = LINK_RESET;
}

void viport_get_stats(struct viport *viport,
		     struct net_device_stats *stats)
{
	unsigned long flags;

	VIPORT_FUNCTION("viport_get_stats()\n");
	/* Reference count has been already incremented indicating
	 * that viport structure is being used, which prevents its
	 * freeing when this task sleeps
	 */
	if (time_after(jiffies,
		(viport->last_stats_time + viport->config->stats_interval))) {

		spin_lock_irqsave(&viport->lock, flags);
		viport->updates |= NEED_STATS;
		spin_unlock_irqrestore(&viport->lock, flags);
		viport_kick(viport);
		wait_event(viport->stats_queue,
			   !(viport->updates & NEED_STATS)
			   || (viport->disconnect == 1));

		if (viport->stats.ethernet_status)
			vnic_link_up(viport->vnic, viport->parent);
		else
			vnic_link_down(viport->vnic, viport->parent);
	}

	stats->rx_packets = be64_to_cpu(viport->stats.if_in_ok);
	stats->tx_packets = be64_to_cpu(viport->stats.if_out_ok);
	stats->rx_bytes   = be64_to_cpu(viport->stats.if_in_octets);
	stats->tx_bytes   = be64_to_cpu(viport->stats.if_out_octets);
	stats->rx_errors  = be64_to_cpu(viport->stats.if_in_errors);
	stats->tx_errors  = be64_to_cpu(viport->stats.if_out_errors);
	stats->rx_dropped = 0;	/* EIOC doesn't track */
	stats->tx_dropped = 0;	/* EIOC doesn't track */
	stats->multicast  = be64_to_cpu(viport->stats.if_in_nucast_pkts);
	stats->collisions = 0;	/* EIOC doesn't track */
}

int viport_xmit_packet(struct viport *viport, struct sk_buff *skb)
{
	int status = -1;
	unsigned long flags;

	VIPORT_FUNCTION("viport_xmit_packet()\n");
	spin_lock_irqsave(&viport->lock, flags);
	if (viport->state == VIPORT_CONNECTED)
		status = data_xmit_packet(&viport->data, skb);
	spin_unlock_irqrestore(&viport->lock, flags);

	return status;
}

void viport_kick(struct viport *viport)
{
	unsigned long flags;

	VIPORT_FUNCTION("viport_kick()\n");
	spin_lock_irqsave(&viport_list_lock, flags);
	if (list_empty(&viport->list_ptrs)) {
		list_add_tail(&viport->list_ptrs, &viport_list);
		wake_up(&viport_queue);
	}
	spin_unlock_irqrestore(&viport_list_lock, flags);
}

void viport_failure(struct viport *viport)
{
	unsigned long flags;

	VIPORT_FUNCTION("viport_failure()\n");
	vnic_stop_xmit(viport->vnic, viport->parent);
	spin_lock_irqsave(&viport_list_lock, flags);
	viport->errored = 1;
	if (list_empty(&viport->list_ptrs)) {
		list_add_tail(&viport->list_ptrs, &viport_list);
		wake_up(&viport_queue);
	}
	spin_unlock_irqrestore(&viport_list_lock, flags);
}

static void viport_timeout(unsigned long data)
{
	struct viport *viport;

	VIPORT_FUNCTION("viport_timeout()\n");
	viport = (struct viport *)data;
	viport->timer_active = 0;
	viport_kick(viport);
}

static void viport_timer(struct viport *viport, int timeout)
{
	VIPORT_FUNCTION("viport_timer()\n");
	if (viport->timer_active)
		del_timer(&viport->timer);
	init_timer(&viport->timer);
	viport->timer.expires = jiffies + timeout;
	viport->timer.data = (unsigned long)viport;
	viport->timer.function = viport_timeout;
	viport->timer_active = 1;
	add_timer(&viport->timer);
}

static void viport_timer_stop(struct viport *viport)
{
	VIPORT_FUNCTION("viport_timer_stop()\n");
	if (viport->timer_active)
		del_timer(&viport->timer);
	viport->timer_active = 0;
}

static int viport_init_mac_addresses(struct viport *viport)
{
	struct vnic_address_op2	*temp;
	unsigned long		flags;
	int			i;

	VIPORT_FUNCTION("viport_init_mac_addresses()\n");
	i = viport->num_mac_addresses * sizeof *temp;
	temp = kzalloc(viport->num_mac_addresses * sizeof *temp,
		       GFP_KERNEL);
	if (!temp) {
		VIPORT_ERROR("failed allocating MAC address table\n");
		return -ENOMEM;
	}

	spin_lock_irqsave(&viport->lock, flags);
	viport->mac_addresses = temp;
	for (i = 0; i < viport->num_mac_addresses; i++) {
		viport->mac_addresses[i].index = cpu_to_be16(i);
		viport->mac_addresses[i].vlan =
				cpu_to_be16(viport->default_vlan);
	}
	memset(viport->mac_addresses[BROADCAST_ADDR].address,
	       0xFF, ETH_ALEN);
	viport->mac_addresses[BROADCAST_ADDR].valid = 1;
	memcpy(viport->mac_addresses[UNICAST_ADDR].address,
	       viport->hw_mac_address, ETH_ALEN);
	viport->mac_addresses[UNICAST_ADDR].valid = 1;

	spin_unlock_irqrestore(&viport->lock, flags);

	return 0;
}

static inline void viport_match_mac_address(struct vnic *vnic,
					    struct viport *viport)
{
	if (vnic && vnic->current_path &&
	    viport == vnic->current_path->viport &&
	    vnic->mac_set &&
	    memcmp(vnic->netdevice->dev_addr, viport->hw_mac_address, ETH_ALEN)) {
		VIPORT_ERROR("*** ERROR MAC address mismatch; "
				"current = %02x:%02x:%02x:%02x:%02x:%02x "
				"From EVIC = %02x:%02x:%02x:%02x:%02x:%02x\n",
				vnic->netdevice->dev_addr[0],
				vnic->netdevice->dev_addr[1],
				vnic->netdevice->dev_addr[2],
				vnic->netdevice->dev_addr[3],
				vnic->netdevice->dev_addr[4],
				vnic->netdevice->dev_addr[5],
				viport->hw_mac_address[0],
				viport->hw_mac_address[1],
				viport->hw_mac_address[2],
				viport->hw_mac_address[3],
				viport->hw_mac_address[4],
				viport->hw_mac_address[5]);
	}
}

static int viport_handle_init_states(struct viport *viport)
{
	enum link_state old_state;

	do {
		switch (old_state = viport->link_state) {
		case LINK_UNINITIALIZED:
			LINK_STATE("state LINK_UNINITIALIZED\n");
			viport->updates = 0;
			spin_lock_irq(&viport_list_lock);
			list_del_init(&viport->list_ptrs);
			spin_unlock_irq(&viport_list_lock);
			if (atomic_read(&viport->reference_count)) {
				wake_up(&viport->stats_queue);
				wait_event(viport->reference_queue,
					 atomic_read(&viport->reference_count) == 0);
			}
			/* No more references to viport structure
			 * so it is safe to delete it by waking disconnect
			 * queue
			 */

			viport->disconnect = 0;
			wake_up(&viport->disconnect_queue);
			break;
		case LINK_INITIALIZE:
			LINK_STATE("state LINK_INITIALIZE\n");
			viport->errored = 0;
			viport->connect = WAIT;
			viport->last_stats_time = 0;
			if (viport->disconnect)
				viport->link_state = LINK_UNINITIALIZED;
			else
				viport->link_state = LINK_INITIALIZECONTROL;
			break;
		case LINK_INITIALIZECONTROL:
			LINK_STATE("state LINK_INITIALIZECONTROL\n");
			viport->pd = ib_alloc_pd(viport->config->ibdev);
			if (IS_ERR(viport->pd))
				viport->link_state = LINK_DISCONNECTED;
			else if (control_init(&viport->control, viport,
					    &viport->config->control_config,
					    viport->pd)) {
				ib_dealloc_pd(viport->pd);
				viport->link_state = LINK_DISCONNECTED;

			} else
				viport->link_state = LINK_INITIALIZEDATA;
			break;
		case LINK_INITIALIZEDATA:
			LINK_STATE("state LINK_INITIALIZEDATA\n");
			if (data_init(&viport->data, viport,
				      &viport->config->data_config,
				      viport->pd))
				viport->link_state = LINK_CLEANUPCONTROL;
			else
				viport->link_state = LINK_CONTROLCONNECT;
			break;
		default:
			return -1;
		}
	} while (viport->link_state != old_state);

	return 0;
}

static int viport_handle_control_states(struct viport *viport)
{
	enum link_state old_state;
	struct vnic *vnic;

	do {
		switch (old_state = viport->link_state) {
		case LINK_CONTROLCONNECT:
			if (vnic_ib_cm_connect(&viport->control.ib_conn))
				viport->link_state = LINK_CLEANUPDATA;
			else
				viport->link_state = LINK_CONTROLCONNECTWAIT;
			break;
		case LINK_CONTROLCONNECTWAIT:
			LINK_STATE("state LINK_CONTROLCONNECTWAIT\n");
			if (control_is_connected(&viport->control))
				viport->link_state = LINK_INITVNICREQ;
			if (viport->errored) {
				viport->errored = 0;
				viport->link_state = LINK_CONTROLDISCONNECT;
			}
			break;
		case LINK_INITVNICREQ:
			LINK_STATE("state LINK_INITVNICREQ\n");
			if (control_init_vnic_req(&viport->control))
				viport->link_state = LINK_RESETCONTROL;
			else
				viport->link_state = LINK_INITVNICRSP;
			break;
		case LINK_INITVNICRSP:
			LINK_STATE("state LINK_INITVNICRSP\n");
			control_process_async(&viport->control);

			if (!control_init_vnic_rsp(&viport->control,
						  &viport->features_supported,
						  viport->hw_mac_address,
						  &viport->num_mac_addresses,
						  &viport->default_vlan)) {
				if (viport_init_mac_addresses(viport))
					viport->link_state =
							LINK_RESETCONTROL;
				else {
					viport->link_state =
							LINK_BEGINDATAPATH;
					/*
					 * Ensure that the current path's MAC
					 * address matches the one returned by
					 * EVIC - we've had cases of mismatch
					 * which then caused havoc.
					 */
					vnic = viport->parent->parent;
					viport_match_mac_address(vnic, viport);
				}
			}

			if (viport->errored) {
				viport->errored = 0;
				viport->link_state = LINK_RESETCONTROL;
			}
			break;
		default:
			return -1;
		}
	} while (viport->link_state != old_state);

	return 0;
}

static int viport_handle_data_states(struct viport *viport)
{
	enum link_state old_state;

	do {
		switch (old_state = viport->link_state) {
		case LINK_BEGINDATAPATH:
			LINK_STATE("state LINK_BEGINDATAPATH\n");
			viport->link_state = LINK_CONFIGDATAPATHREQ;
			break;
		case LINK_CONFIGDATAPATHREQ:
			LINK_STATE("state LINK_CONFIGDATAPATHREQ\n");
			if (control_config_data_path_req(&viport->control,
						data_path_id(&viport->
							     data),
						data_host_pool_max
						(&viport->data),
						data_eioc_pool_max
						(&viport->data)))
				viport->link_state = LINK_RESETCONTROL;
			else
				viport->link_state = LINK_CONFIGDATAPATHRSP;
			break;
		case LINK_CONFIGDATAPATHRSP:
			LINK_STATE("state LINK_CONFIGDATAPATHRSP\n");
			control_process_async(&viport->control);

			if (!control_config_data_path_rsp(&viport->control,
							 data_host_pool
							 (&viport->data),
							 data_eioc_pool
							 (&viport->data),
							 data_host_pool_max
							 (&viport->data),
							 data_eioc_pool_max
							 (&viport->data),
							 data_host_pool_min
							 (&viport->data),
							 data_eioc_pool_min
							 (&viport->data)))
				viport->link_state = LINK_DATACONNECT;

			if (viport->errored) {
				viport->errored = 0;
				viport->link_state = LINK_RESETCONTROL;
			}
			break;
		case LINK_DATACONNECT:
			LINK_STATE("state LINK_DATACONNECT\n");
			if (data_connect(&viport->data))
				viport->link_state = LINK_RESETCONTROL;
			else
				viport->link_state = LINK_DATACONNECTWAIT;
			break;
		case LINK_DATACONNECTWAIT:
			LINK_STATE("state LINK_DATACONNECTWAIT\n");
			control_process_async(&viport->control);
			if (data_is_connected(&viport->data))
				viport->link_state = LINK_XCHGPOOLREQ;

			if (viport->errored) {
				viport->errored = 0;
				viport->link_state = LINK_RESET;
			}
			break;
		default:
			return -1;
		}
	} while (viport->link_state != old_state);

	return 0;
}

static int viport_handle_xchgpool_states(struct viport *viport)
{
	enum link_state old_state;

	do {
		switch (old_state = viport->link_state) {
		case LINK_XCHGPOOLREQ:
			LINK_STATE("state LINK_XCHGPOOLREQ\n");
			if (control_exchange_pools_req(&viport->control,
						       data_local_pool_addr
						       (&viport->data),
						       data_local_pool_rkey
						       (&viport->data)))
				viport->link_state = LINK_RESET;
			else
				viport->link_state = LINK_XCHGPOOLRSP;
			break;
		case LINK_XCHGPOOLRSP:
			LINK_STATE("state LINK_XCHGPOOLRSP\n");
			control_process_async(&viport->control);

			if (!control_exchange_pools_rsp(&viport->control,
						       data_remote_pool_addr
						       (&viport->data),
						       data_remote_pool_rkey
						       (&viport->data)))
				viport->link_state = LINK_INITIALIZED;

			if (viport->errored) {
				viport->errored = 0;
				viport->link_state = LINK_RESET;
			}
			break;
		case LINK_INITIALIZED:
			LINK_STATE("state LINK_INITIALIZED\n");
			viport->state = VIPORT_CONNECTED;
			printk(KERN_INFO PFX
			       "%s: connection established\n",
			       config_viport_name(viport->config));
			data_connected(&viport->data);
			vnic_connected(viport->parent->parent,
				       viport->parent);
			if (viport->features_supported & VNIC_FEAT_INBOUND_IB_MC) {
				printk(KERN_INFO PFX "%s: Supports Inbound IB "
					"Multicast\n",
					config_viport_name(viport->config));
				if (mc_data_init(&viport->mc_data, viport,
						&viport->config->data_config,
						viport->pd)) {
					viport_disable_multicast(viport);
					break;
				}
			}
			spin_lock_irq(&viport->lock);
			viport->mtu = 1500;
			viport->flags = 0;
			if ((viport->mtu != viport->new_mtu) ||
			    (viport->flags != viport->new_flags))
				viport->updates |= NEED_LINK_CONFIG;
			spin_unlock_irq(&viport->lock);
			viport->link_state = LINK_IDLE;
			viport->retry_duration = 0;
			viport->total_retry_duration = 0;
			break;
		default:
			return -1;
		}
	} while (viport->link_state != old_state);

	return 0;
}

static int viport_handle_idle_states(struct viport *viport)
{
	enum link_state old_state;
	int handle_mc_join_compl, handle_mc_join;

	do {
		switch (old_state = viport->link_state) {
		case LINK_IDLE:
			LINK_STATE("state LINK_IDLE\n");
			if (viport->config->hb_interval)
				viport_timer(viport,
					     viport->config->hb_interval);
			viport->link_state = LINK_IDLING;
			break;
		case LINK_IDLING:
			LINK_STATE("state LINK_IDLING\n");
			control_process_async(&viport->control);
			if (viport->errored) {
				viport_timer_stop(viport);
				viport->errored = 0;
				viport->link_state = LINK_RESET;
				break;
			}

			spin_lock_irq(&viport->lock);
			handle_mc_join = (viport->updates & NEED_MCAST_JOIN);
			handle_mc_join_compl =
				      (viport->updates & NEED_MCAST_COMPLETION);
			/*
			 * Turn off both flags, the handler functions will
			 * rearm them if necessary.
			 */
			viport->updates &= ~(NEED_MCAST_JOIN | NEED_MCAST_COMPLETION);

			if (viport->updates & NEED_LINK_CONFIG) {
				viport_timer_stop(viport);
				viport->link_state = LINK_CONFIGLINKREQ;
			} else if (viport->updates & NEED_ADDRESS_CONFIG) {
				viport_timer_stop(viport);
				viport->link_state = LINK_CONFIGADDRSREQ;
			} else if (viport->updates & NEED_STATS) {
				viport_timer_stop(viport);
				viport->link_state = LINK_REPORTSTATREQ;
			} else if (viport->config->hb_interval) {
				if (!viport->timer_active)
					viport->link_state =
						LINK_HEARTBEATREQ;
			}
			spin_unlock_irq(&viport->lock);
			if (handle_mc_join) {
				if (vnic_mc_join(viport))
					viport_disable_multicast(viport);
			}
			if (handle_mc_join_compl)
				vnic_mc_join_handle_completion(viport);

			break;
		default:
			return -1;
		}
	} while (viport->link_state != old_state);

	return 0;
}

static int viport_handle_config_states(struct viport *viport)
{
	enum link_state old_state;
	int res;

	do {
		switch (old_state = viport->link_state) {
		case LINK_CONFIGLINKREQ:
			LINK_STATE("state LINK_CONFIGLINKREQ\n");
			spin_lock_irq(&viport->lock);
			viport->updates &= ~NEED_LINK_CONFIG;
			viport->flags = viport->new_flags;
			if (viport->updates & MCAST_OVERFLOW)
				viport->flags |= IFF_ALLMULTI;
			viport->mtu = viport->new_mtu;
			spin_unlock_irq(&viport->lock);
			if (control_config_link_req(&viport->control,
						    viport->flags,
						    viport->mtu))
				viport->link_state = LINK_RESET;
			else
				viport->link_state = LINK_CONFIGLINKRSP;
			break;
		case LINK_CONFIGLINKRSP:
			LINK_STATE("state LINK_CONFIGLINKRSP\n");
			control_process_async(&viport->control);

			if (!control_config_link_rsp(&viport->control,
						    &viport->flags,
						    &viport->mtu))
				viport->link_state = LINK_IDLE;

			if (viport->errored) {
				viport->errored = 0;
				viport->link_state = LINK_RESET;
			}
			break;
		case LINK_CONFIGADDRSREQ:
			LINK_STATE("state LINK_CONFIGADDRSREQ\n");

			spin_lock_irq(&viport->lock);
			res = control_config_addrs_req(&viport->control,
						       viport->mac_addresses,
						       viport->
						       num_mac_addresses);

			if (res > 0) {
				viport->updates &= ~NEED_ADDRESS_CONFIG;
				viport->link_state = LINK_CONFIGADDRSRSP;
			} else if (res == 0)
				viport->link_state = LINK_CONFIGADDRSRSP;
			else
				viport->link_state = LINK_RESET;
			spin_unlock_irq(&viport->lock);
			break;
		case LINK_CONFIGADDRSRSP:
			LINK_STATE("state LINK_CONFIGADDRSRSP\n");
			control_process_async(&viport->control);

			if (!control_config_addrs_rsp(&viport->control))
				viport->link_state = LINK_IDLE;

			if (viport->errored) {
				viport->errored = 0;
				viport->link_state = LINK_RESET;
			}
			break;
		default:
			return -1;
		}
	} while (viport->link_state != old_state);

	return 0;
}

static int viport_handle_stat_states(struct viport *viport)
{
	enum link_state old_state;

	do {
		switch (old_state = viport->link_state) {
		case LINK_REPORTSTATREQ:
			LINK_STATE("state LINK_REPORTSTATREQ\n");
			if (control_report_statistics_req(&viport->control))
				viport->link_state = LINK_RESET;
			else
				viport->link_state = LINK_REPORTSTATRSP;
			break;
		case LINK_REPORTSTATRSP:
			LINK_STATE("state LINK_REPORTSTATRSP\n");
			control_process_async(&viport->control);

			spin_lock_irq(&viport->lock);
			if (control_report_statistics_rsp(&viport->control,
						  &viport->stats) == 0) {
				viport->updates &= ~NEED_STATS;
				viport->last_stats_time = jiffies;
				wake_up(&viport->stats_queue);
				viport->link_state = LINK_IDLE;
			}

			spin_unlock_irq(&viport->lock);

			if (viport->errored) {
				viport->errored = 0;
				viport->link_state = LINK_RESET;
			}
			break;
		default:
			return -1;
		}
	} while (viport->link_state != old_state);

	return 0;
}

static int viport_handle_heartbeat_states(struct viport *viport)
{
	enum link_state old_state;

	do {
		switch (old_state = viport->link_state) {
		case LINK_HEARTBEATREQ:
			LINK_STATE("state LINK_HEARTBEATREQ\n");
			if (control_heartbeat_req(&viport->control,
						  viport->config->hb_timeout))
				viport->link_state = LINK_RESET;
			else
				viport->link_state = LINK_HEARTBEATRSP;
			break;
		case LINK_HEARTBEATRSP:
			LINK_STATE("state LINK_HEARTBEATRSP\n");
			control_process_async(&viport->control);

			if (!control_heartbeat_rsp(&viport->control))
				viport->link_state = LINK_IDLE;

			if (viport->errored) {
				viport->errored = 0;
				viport->link_state = LINK_RESET;
			}
			break;
		default:
			return -1;
		}
	} while (viport->link_state != old_state);

	return 0;
}

static int viport_handle_reset_states(struct viport *viport)
{
	enum link_state old_state;
	int handle_mc_join_compl = 0, handle_mc_join = 0;

	do {
		switch (old_state = viport->link_state) {
		case LINK_RESET:
			LINK_STATE("state LINK_RESET\n");
			viport->errored = 0;
			spin_lock_irq(&viport->lock);
			viport->state = VIPORT_DISCONNECTED;
			/*
			 * Turn off both flags, the handler functions will
			 * rearm them if necessary
			 */
			viport->updates &= ~(NEED_MCAST_JOIN | NEED_MCAST_COMPLETION);

			spin_unlock_irq(&viport->lock);
			vnic_link_down(viport->vnic, viport->parent);
			printk(KERN_INFO PFX
			       "%s: connection lost\n",
			       config_viport_name(viport->config));
			if (handle_mc_join) {
				if (vnic_mc_join(viport))
					viport_disable_multicast(viport);
			}
			if (handle_mc_join_compl)
				vnic_mc_join_handle_completion(viport);
			if (viport->features_supported & VNIC_FEAT_INBOUND_IB_MC) {
				vnic_mc_leave(viport);
				vnic_mc_data_cleanup(&viport->mc_data);
			}

			if (control_reset_req(&viport->control))
				viport->link_state = LINK_DATADISCONNECT;
			else
				viport->link_state = LINK_RESETRSP;
			break;
		case LINK_RESETRSP:
			LINK_STATE("state LINK_RESETRSP\n");
			control_process_async(&viport->control);

			if (!control_reset_rsp(&viport->control))
				viport->link_state = LINK_DATADISCONNECT;

			if (viport->errored) {
				viport->errored = 0;
				viport->link_state = LINK_DATADISCONNECT;
			}
			break;
		case LINK_RESETCONTROL:
			LINK_STATE("state LINK_RESETCONTROL\n");
			if (control_reset_req(&viport->control))
				viport->link_state = LINK_CONTROLDISCONNECT;
			else
				viport->link_state = LINK_RESETCONTROLRSP;
			break;
		case LINK_RESETCONTROLRSP:
			LINK_STATE("state LINK_RESETCONTROLRSP\n");
			control_process_async(&viport->control);

			if (!control_reset_rsp(&viport->control))
				viport->link_state = LINK_CONTROLDISCONNECT;

			if (viport->errored) {
				viport->errored = 0;
				viport->link_state = LINK_CONTROLDISCONNECT;
			}
			break;
		default:
			return -1;
		}
	} while (viport->link_state != old_state);

	return 0;
}

static int viport_handle_disconn_states(struct viport *viport)
{
	enum link_state old_state;

	do {
		switch (old_state = viport->link_state) {
		case LINK_DATADISCONNECT:
			LINK_STATE("state LINK_DATADISCONNECT\n");
			data_disconnect(&viport->data);
			viport->link_state = LINK_CONTROLDISCONNECT;
			break;
		case LINK_CONTROLDISCONNECT:
			LINK_STATE("state LINK_CONTROLDISCONNECT\n");
			viport->link_state = LINK_CLEANUPDATA;
			break;
		case LINK_CLEANUPDATA:
			LINK_STATE("state LINK_CLEANUPDATA\n");
			data_cleanup(&viport->data);
			viport->link_state = LINK_CLEANUPCONTROL;
			break;
		case LINK_CLEANUPCONTROL:
			LINK_STATE("state LINK_CLEANUPCONTROL\n");
			spin_lock_irq(&viport->lock);
			kfree(viport->mac_addresses);
			viport->mac_addresses = NULL;
			spin_unlock_irq(&viport->lock);
			control_cleanup(&viport->control);
			ib_dealloc_pd(viport->pd);
			viport->link_state = LINK_DISCONNECTED;
			break;
		case LINK_DISCONNECTED:
			LINK_STATE("state LINK_DISCONNECTED\n");
			vnic_disconnected(viport->parent->parent,
					  viport->parent);
			if (viport->disconnect != 0)
				viport->link_state = LINK_UNINITIALIZED;
			else if (viport->retry == 1) {
				viport->retry = 0;
			/*
			 * Check if the initial retry interval has crossed
			 * 20 seconds.
			 * The retry interval is initially 5 seconds which
			 * is incremented by 5. Once it is 20 the interval
			 * is fixed to 20 seconds till 10 minutes,
			 * after which retrying is stopped
			 */
				if (viport->retry_duration  < MAX_RETRY_INTERVAL)
					viport->retry_duration +=
								RETRY_INCREMENT;

				viport->total_retry_duration +=
							 viport->retry_duration;

				if (viport->total_retry_duration >=
					MAX_CONNECT_RETRY_TIMEOUT) {
					viport->link_state = LINK_UNINITIALIZED;
					printk("Timed out after retrying"
					       " for retry_duration %d msecs\n"
						, viport->total_retry_duration);
				} else {
					viport->connect = DELAY;
					viport->link_state = LINK_RETRYWAIT;
				}
				viport_timer(viport,
				     msecs_to_jiffies(viport->retry_duration));
			} else {
				u32 duration = 5000 + ((net_random()) & 0x1FF);
				if (!viport->parent->is_primary_path)
					duration += 0x1ff;
				viport_timer(viport,
					     msecs_to_jiffies(duration));
				viport->connect = DELAY;
				viport->link_state = LINK_RETRYWAIT;
			}
			break;
		case LINK_RETRYWAIT:
			LINK_STATE("state LINK_RETRYWAIT\n");
			viport->stats.ethernet_status = 0;
			viport->updates = 0;
			wake_up(&viport->stats_queue);
			if (viport->disconnect != 0) {
				viport_timer_stop(viport);
				viport->link_state = LINK_UNINITIALIZED;
			} else if (viport->connect == DELAY) {
				if (!viport->timer_active)
					viport->link_state = LINK_INITIALIZE;
			} else if (viport->connect == NOW) {
				viport_timer_stop(viport);
				viport->link_state = LINK_INITIALIZE;
			}
			break;
		case LINK_FIRSTCONNECT:
			viport->stats.ethernet_status = 0;
			viport->updates = 0;
			wake_up(&viport->stats_queue);
			if (viport->disconnect != 0) {
				viport_timer_stop(viport);
				viport->link_state = LINK_UNINITIALIZED;
			}

			break;
		default:
			return -1;
		}
	} while (viport->link_state != old_state);

	return 0;
}

static int viport_statemachine(void *context)
{
	struct viport *viport;
	enum link_state old_link_state;

	VIPORT_FUNCTION("viport_statemachine()\n");
	while (!viport_thread_end || !list_empty(&viport_list)) {
		wait_event_interruptible(viport_queue,
					 !list_empty(&viport_list)
					 || viport_thread_end);
		spin_lock_irq(&viport_list_lock);
		if (list_empty(&viport_list)) {
			spin_unlock_irq(&viport_list_lock);
			continue;
		}
		viport = list_entry(viport_list.next, struct viport,
				    list_ptrs);
		list_del_init(&viport->list_ptrs);
		spin_unlock_irq(&viport_list_lock);

		do {
			old_link_state = viport->link_state;

			/*
			 * Optimize for the state machine steady state
			 * by checking for the most common states first.
			 *
			 */
			if (viport_handle_idle_states(viport) == 0)
				break;
			if (viport_handle_heartbeat_states(viport) == 0)
				break;
			if (viport_handle_stat_states(viport) == 0)
				break;
			if (viport_handle_config_states(viport) == 0)
				break;

			if (viport_handle_init_states(viport) == 0)
				break;
			if (viport_handle_control_states(viport) == 0)
				break;
			if (viport_handle_data_states(viport) == 0)
				break;
			if (viport_handle_xchgpool_states(viport) == 0)
				break;
			if (viport_handle_reset_states(viport) == 0)
				break;
			if (viport_handle_disconn_states(viport) == 0)
				break;
		} while (viport->link_state != old_link_state);
	}

	complete_and_exit(&viport_thread_exit, 0);
}

int viport_start(void)
{
	VIPORT_FUNCTION("viport_start()\n");

	spin_lock_init(&viport_list_lock);
	viport_thread = kthread_run(viport_statemachine, NULL,
					"qlgc_vnic_viport_s_m");
	if (IS_ERR(viport_thread)) {
		printk(KERN_WARNING PFX "Could not create viport_thread;"
		       " error %d\n", (int) PTR_ERR(viport_thread));
		viport_thread = NULL;
		return 1;
	}

	return 0;
}

void viport_cleanup(void)
{
	VIPORT_FUNCTION("viport_cleanup()\n");
	if (viport_thread) {
		viport_thread_end = 1;
		wake_up(&viport_queue);
		wait_for_completion(&viport_thread_exit);
		viport_thread = NULL;
	}
}
