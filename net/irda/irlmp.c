/*********************************************************************
 *                
 * Filename:      irlmp.c
 * Version:       1.0
 * Description:   IrDA Link Management Protocol (LMP) layer                 
 * Status:        Stable.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sun Aug 17 20:54:32 1997
 * Modified at:   Wed Jan  5 11:26:03 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1998-2000 Dag Brattli <dagb@cs.uit.no>, 
 *     All Rights Reserved.
 *     Copyright (c) 2000-2001 Jean Tourrilhes <jt@hpl.hp.com>
 *     
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *
 ********************************************************************/

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/random.h>

#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/timer.h>
#include <net/irda/qos.h>
#include <net/irda/irlap.h>
#include <net/irda/iriap.h>
#include <net/irda/irlmp.h>
#include <net/irda/irlmp_frame.h>

/* Master structure */
struct irlmp_cb *irlmp = NULL;

/* These can be altered by the sysctl interface */
int  sysctl_discovery         = 0;
int  sysctl_discovery_timeout = 3; /* 3 seconds by default */
int  sysctl_discovery_slots   = 6; /* 6 slots by default */
int  sysctl_lap_keepalive_time = LM_IDLE_TIMEOUT * 1000 / HZ;
char sysctl_devname[65];

char *lmp_reasons[] = {
	"ERROR, NOT USED",
	"LM_USER_REQUEST",
	"LM_LAP_DISCONNECT",
	"LM_CONNECT_FAILURE",
	"LM_LAP_RESET",
	"LM_INIT_DISCONNECT",
	"ERROR, NOT USED",
};

__u8 *irlmp_hint_to_service(__u8 *hint);
#ifdef CONFIG_PROC_FS
int irlmp_proc_read(char *buf, char **start, off_t offst, int len);
#endif

/*
 * Function irlmp_init (void)
 *
 *    Create (allocate) the main IrLMP structure
 *
 */
int __init irlmp_init(void)
{
	IRDA_DEBUG(1, "%s()\n", __FUNCTION__);
	/* Initialize the irlmp structure. */
	irlmp = kmalloc( sizeof(struct irlmp_cb), GFP_KERNEL);
	if (irlmp == NULL)
		return -ENOMEM;
	memset(irlmp, 0, sizeof(struct irlmp_cb));
	
	irlmp->magic = LMP_MAGIC;
	spin_lock_init(&irlmp->log_lock);

	irlmp->clients = hashbin_new(HB_GLOBAL);
	irlmp->services = hashbin_new(HB_GLOBAL);
	irlmp->links = hashbin_new(HB_GLOBAL);
	irlmp->unconnected_lsaps = hashbin_new(HB_GLOBAL);
	irlmp->cachelog = hashbin_new(HB_GLOBAL);
	
	irlmp->free_lsap_sel = 0x10; /* Reserved 0x00-0x0f */
#ifdef CONFIG_IRDA_CACHE_LAST_LSAP
	irlmp->cache.valid = FALSE;
#endif
	strcpy(sysctl_devname, "Linux");
	
	/* Do discovery every 3 seconds */
	init_timer(&irlmp->discovery_timer);
   	irlmp_start_discovery_timer(irlmp, sysctl_discovery_timeout*HZ);

	return 0;
}

/*
 * Function irlmp_cleanup (void)
 *
 *    Remove IrLMP layer
 *
 */
void irlmp_cleanup(void) 
{
	/* Check for main structure */
	ASSERT(irlmp != NULL, return;);
	ASSERT(irlmp->magic == LMP_MAGIC, return;);

	del_timer(&irlmp->discovery_timer);
	
	hashbin_delete(irlmp->links, (FREE_FUNC) kfree);
	hashbin_delete(irlmp->unconnected_lsaps, (FREE_FUNC) kfree);
	hashbin_delete(irlmp->clients, (FREE_FUNC) kfree);
	hashbin_delete(irlmp->services, (FREE_FUNC) kfree);
	hashbin_delete(irlmp->cachelog, (FREE_FUNC) kfree);
	
	/* De-allocate main structure */
	kfree(irlmp);
	irlmp = NULL;
}

/*
 * Function irlmp_open_lsap (slsap, notify)
 *
 *   Register with IrLMP and create a local LSAP,
 *   returns handle to LSAP.
 */
struct lsap_cb *irlmp_open_lsap(__u8 slsap_sel, notify_t *notify, __u8 pid)
{
	struct lsap_cb *self;

	ASSERT(notify != NULL, return NULL;);
	ASSERT(irlmp != NULL, return NULL;);
	ASSERT(irlmp->magic == LMP_MAGIC, return NULL;);

	/*  Does the client care which Source LSAP selector it gets?  */
	if (slsap_sel == LSAP_ANY) {
		slsap_sel = irlmp_find_free_slsap();
		if (!slsap_sel)
			return NULL;
	} else if (irlmp_slsap_inuse(slsap_sel))
		return NULL;

	/* Allocate new instance of a LSAP connection */
	self = kmalloc(sizeof(struct lsap_cb), GFP_ATOMIC);
	if (self == NULL) {
		ERROR("%s(), can't allocate memory", __FUNCTION__);
		return NULL;
	}
	memset(self, 0, sizeof(struct lsap_cb));
	
	self->magic = LMP_LSAP_MAGIC;
	self->slsap_sel = slsap_sel;

	/* Fix connectionless LSAP's */
	if (slsap_sel == LSAP_CONNLESS) {
#ifdef CONFIG_IRDA_ULTRA
		self->dlsap_sel = LSAP_CONNLESS;
		self->pid = pid;
#endif /* CONFIG_IRDA_ULTRA */
	} else
		self->dlsap_sel = LSAP_ANY;
	/* self->connected = FALSE; -> already NULL via memset() */

	init_timer(&self->watchdog_timer);

	ASSERT(notify->instance != NULL, return NULL;);
	self->notify = *notify;

	self->lsap_state = LSAP_DISCONNECTED;
	
	/* Insert into queue of unconnected LSAPs */
	hashbin_insert(irlmp->unconnected_lsaps, (irda_queue_t *) self, (int) self, 
		       NULL);
	
	return self;
}

/*
 * Function __irlmp_close_lsap (self)
 *
 *    Remove an instance of LSAP
 */
static void __irlmp_close_lsap(struct lsap_cb *self)
{
	IRDA_DEBUG(4, "%s()\n", __FUNCTION__);

	ASSERT(self != NULL, return;);
	ASSERT(self->magic == LMP_LSAP_MAGIC, return;);

	/*
	 *  Set some of the variables to preset values
	 */
	self->magic = 0;
	del_timer(&self->watchdog_timer); /* Important! */

	if (self->conn_skb)
		dev_kfree_skb(self->conn_skb);

#ifdef CONFIG_IRDA_CACHE_LAST_LSAP
	ASSERT(irlmp != NULL, return;);
	irlmp->cache.valid = FALSE;
#endif
	kfree(self);
}

/*
 * Function irlmp_close_lsap (self)
 *
 *    Close and remove LSAP
 *
 */
void irlmp_close_lsap(struct lsap_cb *self)
{
	struct lap_cb *lap;
	struct lsap_cb *lsap = NULL;

	ASSERT(self != NULL, return;);
	ASSERT(self->magic == LMP_LSAP_MAGIC, return;);

	/*
	 *  Find out if we should remove this LSAP from a link or from the
	 *  list of unconnected lsaps (not associated with a link)
	 */
	lap = self->lap;
	if (lap) {
		ASSERT(lap->magic == LMP_LAP_MAGIC, return;);
		/* We might close a LSAP before it has completed the
		 * connection setup. In those case, higher layers won't
		 * send a proper disconnect request. Harmless, except
		 * that we will forget to close LAP... - Jean II */
		if(self->lsap_state != LSAP_DISCONNECTED) {
			self->lsap_state = LSAP_DISCONNECTED;
			irlmp_do_lap_event(self->lap,
					   LM_LAP_DISCONNECT_REQUEST, NULL);
		}
		/* Now, remove from the link */
		lsap = hashbin_remove(lap->lsaps, (int) self, NULL);
	}
	self->lap = NULL;
	/* Check if we found the LSAP! If not then try the unconnected lsaps */
	if (!lsap) {
		lsap = hashbin_remove(irlmp->unconnected_lsaps, (int) self, 
				      NULL);
	}
	if (!lsap) {
		IRDA_DEBUG(0, "%s(), Looks like somebody has removed me already!\n", __FUNCTION__);
		return;
	}
	__irlmp_close_lsap(self);
}

/*
 * Function irlmp_register_irlap (saddr, notify)
 *
 *    Register IrLAP layer with IrLMP. There is possible to have multiple
 *    instances of the IrLAP layer, each connected to different IrDA ports
 *
 */
void irlmp_register_link(struct irlap_cb *irlap, __u32 saddr, notify_t *notify)
{
	struct lap_cb *lap;

	ASSERT(irlmp != NULL, return;);
	ASSERT(irlmp->magic == LMP_MAGIC, return;);
	ASSERT(notify != NULL, return;);

	/*
	 *  Allocate new instance of a LSAP connection
	 */
	lap = kmalloc(sizeof(struct lap_cb), GFP_KERNEL);
	if (lap == NULL) {
		ERROR("%s(), unable to kmalloc\n", __FUNCTION__);
		return;
	}
	memset(lap, 0, sizeof(struct lap_cb));
	
	lap->irlap = irlap;
	lap->magic = LMP_LAP_MAGIC;
	lap->saddr = saddr;
	lap->daddr = DEV_ADDR_ANY;
	lap->lsaps = hashbin_new(HB_GLOBAL);

	lap->lap_state = LAP_STANDBY;
	
	init_timer(&lap->idle_timer);

	/*
	 *  Insert into queue of LMP links
	 */
	hashbin_insert(irlmp->links, (irda_queue_t *) lap, lap->saddr, NULL);

	/* 
	 *  We set only this variable so IrLAP can tell us on which link the
	 *  different events happened on 
	 */
	irda_notify_init(notify);
	notify->instance = lap;
}

/*
 * Function irlmp_unregister_irlap (saddr)
 *
 *    IrLAP layer has been removed!
 *
 */
void irlmp_unregister_link(__u32 saddr)
{
	struct lap_cb *link;

	IRDA_DEBUG(4, "%s()\n", __FUNCTION__);

	link = hashbin_remove(irlmp->links, saddr, NULL);
	if (link) {
		ASSERT(link->magic == LMP_LAP_MAGIC, return;);

		/* Remove all discoveries discovered at this link */
		irlmp_expire_discoveries(irlmp->cachelog, link->saddr, TRUE);

		del_timer(&link->idle_timer);	

		link->magic = 0;
		kfree(link);
	}
}

/*
 * Function irlmp_connect_request (handle, dlsap, userdata)
 *
 *    Connect with a peer LSAP  
 *
 */
int irlmp_connect_request(struct lsap_cb *self, __u8 dlsap_sel, 
			  __u32 saddr, __u32 daddr, 
			  struct qos_info *qos, struct sk_buff *userdata) 
{
	struct sk_buff *skb = NULL;
	struct lap_cb *lap;
	struct lsap_cb *lsap;
	discovery_t *discovery;

	ASSERT(self != NULL, return -EBADR;);
	ASSERT(self->magic == LMP_LSAP_MAGIC, return -EBADR;);
	
	IRDA_DEBUG(2, "%s(), slsap_sel=%02x, dlsap_sel=%02x, saddr=%08x, daddr=%08x\n", 
		__FUNCTION__, self->slsap_sel, dlsap_sel, saddr, daddr);
	
	if (test_bit(0, &self->connected))
		return -EISCONN;
	
	/* Client must supply destination device address */
	if (!daddr)
		return -EINVAL;
	
	/* Any userdata? */
	if (userdata == NULL) {
		skb = dev_alloc_skb(64);
		if (!skb)
			return -ENOMEM;

		skb_reserve(skb, LMP_MAX_HEADER);
	} else
		skb = userdata;
	
	/* Make room for MUX control header (3 bytes) */
	ASSERT(skb_headroom(skb) >= LMP_CONTROL_HEADER, return -1;);
	skb_push(skb, LMP_CONTROL_HEADER);

	self->dlsap_sel = dlsap_sel;
	
	/*  
	 * Find the link to where we should try to connect since there may
	 * be more than one IrDA port on this machine. If the client has
	 * passed us the saddr (and already knows which link to use), then
	 * we use that to find the link, if not then we have to look in the
	 * discovery log and check if any of the links has discovered a
	 * device with the given daddr 
	 */
	if ((!saddr) || (saddr == DEV_ADDR_ANY)) {
		if (daddr != DEV_ADDR_ANY)
			discovery = hashbin_find(irlmp->cachelog, daddr, NULL);
		else {
			IRDA_DEBUG(2, "%s(), no daddr\n", __FUNCTION__);
			discovery = (discovery_t *) 
				hashbin_get_first(irlmp->cachelog);
		}

		if (discovery) {
			saddr = discovery->saddr;
			daddr = discovery->daddr;
		}
	}
	lap = hashbin_find(irlmp->links, saddr, NULL);	
	if (lap == NULL) {
		IRDA_DEBUG(1, "%s(), Unable to find a usable link!\n", __FUNCTION__);
		return -EHOSTUNREACH;
	}

	/* Check if LAP is disconnected or already connected */
	if (lap->daddr == DEV_ADDR_ANY)
		lap->daddr = daddr;
	else if (lap->daddr != daddr) {
		struct lsap_cb *any_lsap;

		/* Check if some LSAPs are active on this LAP */
		any_lsap = (struct lsap_cb *) hashbin_get_first(lap->lsaps);
		if (any_lsap == NULL) {
			/* No active connection, but LAP hasn't been
			 * disconnected yet (waiting for timeout in LAP).
			 * Maybe we could give LAP a bit of help in this case.
			 */
			IRDA_DEBUG(0, "%s(), sorry, but I'm waiting for LAP to timeout!\n", __FUNCTION__);
			return -EAGAIN;
		}

		/* LAP is already connected to a different node, and LAP
		 * can only talk to one node at a time */
		IRDA_DEBUG(0, "%s(), sorry, but link is busy!\n", __FUNCTION__);
		return -EBUSY;
	}

	self->lap = lap;

	/* 
	 *  Remove LSAP from list of unconnected LSAPs and insert it into the 
	 *  list of connected LSAPs for the particular link 
	 */
	lsap = hashbin_remove(irlmp->unconnected_lsaps, (int) self, NULL);

	ASSERT(lsap != NULL, return -1;);
	ASSERT(lsap->magic == LMP_LSAP_MAGIC, return -1;);
	ASSERT(lsap->lap != NULL, return -1;);
	ASSERT(lsap->lap->magic == LMP_LAP_MAGIC, return -1;);

	hashbin_insert(self->lap->lsaps, (irda_queue_t *) self, (int) self, NULL);

	set_bit(0, &self->connected);	/* TRUE */
	
	/*
	 *  User supplied qos specifications?
	 */
	if (qos)
		self->qos = *qos;
	
	irlmp_do_lsap_event(self, LM_CONNECT_REQUEST, skb);

	return 0;
}

/*
 * Function irlmp_connect_indication (self)
 *
 *    Incoming connection
 *
 */
void irlmp_connect_indication(struct lsap_cb *self, struct sk_buff *skb) 
{
	int max_seg_size;
	int lap_header_size;
	int max_header_size;
	
	ASSERT(self != NULL, return;);
	ASSERT(self->magic == LMP_LSAP_MAGIC, return;);
	ASSERT(skb != NULL, return;);
	ASSERT(self->lap != NULL, return;);

	IRDA_DEBUG(2, "%s(), slsap_sel=%02x, dlsap_sel=%02x\n", 
		__FUNCTION__, self->slsap_sel, self->dlsap_sel);

	/* Note : self->lap is set in irlmp_link_data_indication(),
	 * (case CONNECT_CMD:) because we have no way to set it here.
	 * Similarly, self->dlsap_sel is usually set in irlmp_find_lsap().
	 * Jean II */

	self->qos = *self->lap->qos;

	max_seg_size = self->lap->qos->data_size.value-LMP_HEADER;
	lap_header_size = IRLAP_GET_HEADER_SIZE(self->lap->irlap);
	max_header_size = LMP_HEADER + lap_header_size;

	/* Hide LMP_CONTROL_HEADER header from layer above */
	skb_pull(skb, LMP_CONTROL_HEADER);
	
	if (self->notify.connect_indication)
		self->notify.connect_indication(self->notify.instance, self, 
						&self->qos, max_seg_size, 
						max_header_size, skb);
	else
		dev_kfree_skb(skb);
}

/*
 * Function irlmp_connect_response (handle, userdata)
 *
 *    Service user is accepting connection
 *
 */
int irlmp_connect_response(struct lsap_cb *self, struct sk_buff *userdata) 
{
	ASSERT(self != NULL, return -1;);
	ASSERT(self->magic == LMP_LSAP_MAGIC, return -1;);
	ASSERT(userdata != NULL, return -1;);

	set_bit(0, &self->connected);	/* TRUE */

	IRDA_DEBUG(2, "%s(), slsap_sel=%02x, dlsap_sel=%02x\n", 
		__FUNCTION__, self->slsap_sel, self->dlsap_sel);

	/* Make room for MUX control header (3 bytes) */
	ASSERT(skb_headroom(userdata) >= LMP_CONTROL_HEADER, return -1;);
	skb_push(userdata, LMP_CONTROL_HEADER);
	
	irlmp_do_lsap_event(self, LM_CONNECT_RESPONSE, userdata);

	return 0;
}

/*
 * Function irlmp_connect_confirm (handle, skb)
 *
 *    LSAP connection confirmed peer device!
 */
void irlmp_connect_confirm(struct lsap_cb *self, struct sk_buff *skb) 
{
	int max_header_size;
	int lap_header_size;
	int max_seg_size;

	IRDA_DEBUG(3, "%s()\n", __FUNCTION__);
	
	ASSERT(skb != NULL, return;);
	ASSERT(self != NULL, return;);
	ASSERT(self->magic == LMP_LSAP_MAGIC, return;);	
	ASSERT(self->lap != NULL, return;);

	self->qos = *self->lap->qos;

	max_seg_size    = self->lap->qos->data_size.value-LMP_HEADER;
	lap_header_size = IRLAP_GET_HEADER_SIZE(self->lap->irlap);	
	max_header_size = LMP_HEADER + lap_header_size;

	IRDA_DEBUG(2, "%s(), max_header_size=%d\n",
		__FUNCTION__, max_header_size);

	/* Hide LMP_CONTROL_HEADER header from layer above */
	skb_pull(skb, LMP_CONTROL_HEADER);

	if (self->notify.connect_confirm) {
		self->notify.connect_confirm(self->notify.instance, self,
					     &self->qos, max_seg_size,
					     max_header_size, skb);
	} else
		dev_kfree_skb(skb);
}

/*
 * Function irlmp_dup (orig, instance)
 *
 *    Duplicate LSAP, can be used by servers to confirm a connection on a
 *    new LSAP so it can keep listening on the old one.
 *
 */
struct lsap_cb *irlmp_dup(struct lsap_cb *orig, void *instance) 
{
	struct lsap_cb *new;

	IRDA_DEBUG(1, "%s()\n", __FUNCTION__);

	/* Only allowed to duplicate unconnected LSAP's */
	if (!hashbin_find(irlmp->unconnected_lsaps, (int) orig, NULL)) {
		IRDA_DEBUG(0, "%s(), unable to find LSAP\n", __FUNCTION__);
		return NULL;
	}
	new = kmalloc(sizeof(struct lsap_cb), GFP_ATOMIC);
	if (!new)  {
		IRDA_DEBUG(0, "%s(), unable to kmalloc\n", __FUNCTION__);
		return NULL;
	}
	/* Dup */
	memcpy(new, orig, sizeof(struct lsap_cb));
	new->notify.instance = instance;
	/* new->lap = orig->lap; => done in the memcpy() */
	/* new->slsap_sel = orig->slsap_sel; => done in the memcpy() */

	init_timer(&new->watchdog_timer);
	
	hashbin_insert(irlmp->unconnected_lsaps, (irda_queue_t *) new, (int) new, 
		       NULL);

	/* Make sure that we invalidate the cache */
#ifdef CONFIG_IRDA_CACHE_LAST_LSAP
	irlmp->cache.valid = FALSE;
#endif /* CONFIG_IRDA_CACHE_LAST_LSAP */

	return new;
}

/*
 * Function irlmp_disconnect_request (handle, userdata)
 *
 *    The service user is requesting disconnection, this will not remove the 
 *    LSAP, but only mark it as disconnected
 */
int irlmp_disconnect_request(struct lsap_cb *self, struct sk_buff *userdata) 
{
	struct lsap_cb *lsap;

	ASSERT(self != NULL, return -1;);
	ASSERT(self->magic == LMP_LSAP_MAGIC, return -1;);
	ASSERT(userdata != NULL, return -1;);

	/* Already disconnected ?
	 * There is a race condition between irlmp_disconnect_indication()
	 * and us that might mess up the hashbins below. This fixes it.
	 * Jean II */
	if (! test_and_clear_bit(0, &self->connected)) {
		IRDA_DEBUG(0, "%s(), already disconnected!\n", __FUNCTION__);
		dev_kfree_skb(userdata);
		return -1;
	}

	skb_push(userdata, LMP_CONTROL_HEADER);

	/* 
	 *  Do the event before the other stuff since we must know
	 *  which lap layer that the frame should be transmitted on
	 */
	irlmp_do_lsap_event(self, LM_DISCONNECT_REQUEST, userdata);

	/* 
	 *  Remove LSAP from list of connected LSAPs for the particular link
	 *  and insert it into the list of unconnected LSAPs
	 */
	ASSERT(self->lap != NULL, return -1;);
	ASSERT(self->lap->magic == LMP_LAP_MAGIC, return -1;);
	ASSERT(self->lap->lsaps != NULL, return -1;);

	lsap = hashbin_remove(self->lap->lsaps, (int) self, NULL);

	ASSERT(lsap != NULL, return -1;);
	ASSERT(lsap->magic == LMP_LSAP_MAGIC, return -1;);
	ASSERT(lsap == self, return -1;);

	hashbin_insert(irlmp->unconnected_lsaps, (irda_queue_t *) self, (int) self, 
		       NULL);
	
	/* Reset some values */
	self->dlsap_sel = LSAP_ANY;
	self->lap = NULL;
	
	return 0;
}

/*
 * Function irlmp_disconnect_indication (reason, userdata)
 *
 *    LSAP is being closed!
 */
void irlmp_disconnect_indication(struct lsap_cb *self, LM_REASON reason, 
				 struct sk_buff *userdata) 
{
	struct lsap_cb *lsap;

	IRDA_DEBUG(1, "%s(), reason=%s\n", __FUNCTION__, lmp_reasons[reason]);
	ASSERT(self != NULL, return;);
	ASSERT(self->magic == LMP_LSAP_MAGIC, return;);

	IRDA_DEBUG(3, "%s(), slsap_sel=%02x, dlsap_sel=%02x\n", 
		__FUNCTION__, self->slsap_sel, self->dlsap_sel);

	/* Already disconnected ?
	 * There is a race condition between irlmp_disconnect_request()
	 * and us that might mess up the hashbins below. This fixes it.
	 * Jean II */
	if (! test_and_clear_bit(0, &self->connected)) {
		IRDA_DEBUG(0, "%s(), already disconnected!\n", __FUNCTION__);
		if (userdata)
			dev_kfree_skb(userdata);
		return;
	}

#ifdef CONFIG_IRDA_CACHE_LAST_LSAP
	irlmp->cache.valid = FALSE;
#endif

	/* 
	 *  Remove association between this LSAP and the link it used 
	 */
	ASSERT(self->lap != NULL, return;);
	ASSERT(self->lap->lsaps != NULL, return;);

	lsap = hashbin_remove(self->lap->lsaps, (int) self, NULL);

	ASSERT(lsap != NULL, return;);
	ASSERT(lsap == self, return;);
	hashbin_insert(irlmp->unconnected_lsaps, (irda_queue_t *) lsap, (int) lsap, 
		       NULL);

	self->dlsap_sel = LSAP_ANY;
	self->lap = NULL;
	
	/*
	 *  Inform service user
	 */
	if (self->notify.disconnect_indication)
		self->notify.disconnect_indication(self->notify.instance, 
						   self, reason, userdata);
	else {
		IRDA_DEBUG(0, "%s(), no handler\n", __FUNCTION__);
		if (userdata)
			dev_kfree_skb(userdata);
	}
}

/*
 * Function irlmp_do_expiry (void)
 *
 *    Do a cleanup of the discovery log (remove old entries)
 *
 * Note : separate from irlmp_do_discovery() so that we can handle
 * passive discovery properly.
 */
void irlmp_do_expiry()
{
	struct lap_cb *lap;

	/*
	 * Expire discovery on all links which are *not* connected.
	 * On links which are connected, we can't do discovery
	 * anymore and can't refresh the log, so we freeze the
	 * discovery log to keep info about the device we are
	 * connected to.
	 * This info is mandatory if we want irlmp_connect_request()
	 * to work properly. - Jean II
	 */
	lap = (struct lap_cb *) hashbin_get_first(irlmp->links);
	while (lap != NULL) {
		ASSERT(lap->magic == LMP_LAP_MAGIC, return;);
		
		if (lap->lap_state == LAP_STANDBY) {
			/* Expire discoveries discovered on this link */
			irlmp_expire_discoveries(irlmp->cachelog, lap->saddr,
						 FALSE);
		}
		lap = (struct lap_cb *) hashbin_get_next(irlmp->links);
	}
}

/*
 * Function irlmp_do_discovery (nslots)
 *
 *    Do some discovery on all links
 *
 * Note : log expiry is done above.
 */
void irlmp_do_discovery(int nslots)
{
	struct lap_cb *lap;

	/* Make sure the value is sane */
	if ((nslots != 1) && (nslots != 6) && (nslots != 8) && (nslots != 16)){
		WARNING("%s(), invalid value for number of slots!\n", __FUNCTION__);
		nslots = sysctl_discovery_slots = 8;
	}

	/* Construct new discovery info to be used by IrLAP, */
	irlmp->discovery_cmd.hints.word = irlmp->hints.word;
	
	/* 
	 *  Set character set for device name (we use ASCII), and 
	 *  copy device name. Remember to make room for a \0 at the 
	 *  end
	 */
	irlmp->discovery_cmd.charset = CS_ASCII;
	strncpy(irlmp->discovery_cmd.nickname, sysctl_devname, 
		NICKNAME_MAX_LEN);
	irlmp->discovery_cmd.name_len = strlen(irlmp->discovery_cmd.nickname);
	irlmp->discovery_cmd.nslots = nslots;
	
	/*
	 * Try to send discovery packets on all links
	 */
	lap = (struct lap_cb *) hashbin_get_first(irlmp->links);
	while (lap != NULL) {
		ASSERT(lap->magic == LMP_LAP_MAGIC, return;);
		
		if (lap->lap_state == LAP_STANDBY) {
			/* Try to discover */
			irlmp_do_lap_event(lap, LM_LAP_DISCOVERY_REQUEST, 
					   NULL);
		}
		lap = (struct lap_cb *) hashbin_get_next(irlmp->links);
	}
}

/*
 * Function irlmp_discovery_request (nslots)
 *
 *    Do a discovery of devices in front of the computer
 *
 */
void irlmp_discovery_request(int nslots)
{
	/* Return current cached discovery log */
	irlmp_discovery_confirm(irlmp->cachelog, DISCOVERY_LOG);

	/* 
	 * Start a single discovery operation if discovery is not already
         * running 
	 */
	if (!sysctl_discovery) {
		/* Check if user wants to override the default */
		if (nslots == DISCOVERY_DEFAULT_SLOTS)
			nslots = sysctl_discovery_slots;

		irlmp_do_discovery(nslots);
		/* Note : we never do expiry here. Expiry will run on the
		 * discovery timer regardless of the state of sysctl_discovery
		 * Jean II */
	}
}

/*
 * Function irlmp_get_discoveries (pn, mask, slots)
 *
 *    Return the current discovery log
 *
 */
struct irda_device_info *irlmp_get_discoveries(int *pn, __u16 mask, int nslots)
{
	/* If discovery is not enabled, it's likely that the discovery log
	 * will be empty. So, we trigger a single discovery, so that next
	 * time the user call us there might be some results in the log.
	 * Jean II
	 */
	if (!sysctl_discovery) {
		/* Check if user wants to override the default */
		if (nslots == DISCOVERY_DEFAULT_SLOTS)
			nslots = sysctl_discovery_slots;

		/* Start discovery - will complete sometime later */
		irlmp_do_discovery(nslots);
		/* Note : we never do expiry here. Expiry will run on the
		 * discovery timer regardless of the state of sysctl_discovery
		 * Jean II */
	}

	/* Return current cached discovery log */
	return(irlmp_copy_discoveries(irlmp->cachelog, pn, mask));
}

#if 0
/*
 * Function irlmp_check_services (discovery)
 *
 *    
 *
 */
void irlmp_check_services(discovery_t *discovery)
{
	struct irlmp_client *client;
	__u8 *service_log;
	__u8 service;
	int i = 0;

	IRDA_DEBUG(1, "IrDA Discovered: %s\n", discovery->info);
	IRDA_DEBUG(1, "    Services: ");

	service_log = irlmp_hint_to_service(discovery->hints.byte);
	if (!service_log)
		return;

	/*
	 *  Check all services on the device
	 */
	while ((service = service_log[i++]) != S_END) {
		IRDA_DEBUG( 4, "service=%02x\n", service);
		client = hashbin_find(irlmp->registry, service, NULL);
		if (entry && entry->discovery_callback) {
			IRDA_DEBUG( 4, "discovery_callback!\n");

			entry->discovery_callback(discovery);
		} else {
			/* Don't notify about the ANY service */
			if (service == S_ANY)
				continue;
			/*  
			 * Found no clients for dealing with this service,
			 */
		}
	}
	kfree(service_log);
}
#endif
/*
 * Function irlmp_notify_client (log)
 *
 *    Notify all about discovered devices
 *
 * Clients registered with IrLMP are :
 *	o IrComm
 *	o IrLAN
 *	o Any socket (in any state - ouch, that may be a lot !)
 * The client may have defined a callback to be notified in case of
 * partial/selective discovery based on the hints that it passed to IrLMP.
 */
static inline void
irlmp_notify_client(irlmp_client_t *client,
		    hashbin_t *log, DISCOVERY_MODE mode)
{
	discovery_t *discovery;

	IRDA_DEBUG(3, "%s()\n", __FUNCTION__);
	
	/* Check if client wants or not partial/selective log (optimisation) */
	if (!client->disco_callback)
		return;

	/* 
	 * Now, check all discovered devices (if any), and notify client 
	 * only about the services that the client is interested in 
	 */
	discovery = (discovery_t *) hashbin_get_first(log);
	while (discovery != NULL) {
		IRDA_DEBUG(3, "discovery->daddr = 0x%08x\n", discovery->daddr); 
		
		/* 
		 * Any common hint bits? Remember to mask away the extension
		 * bits ;-)
		 */
		if (client->hint_mask & discovery->hints.word & 0x7f7f)
			client->disco_callback(discovery, mode, client->priv);

		discovery = (discovery_t *) hashbin_get_next(log);
	}
}

/*
 * Function irlmp_discovery_confirm ( self, log)
 *
 *    Some device(s) answered to our discovery request! Check to see which
 *    device it is, and give indication to the client(s)
 * 
 */
void irlmp_discovery_confirm(hashbin_t *log, DISCOVERY_MODE mode) 
{
	irlmp_client_t *client;
	
	IRDA_DEBUG(3, "%s()\n", __FUNCTION__);
	
	ASSERT(log != NULL, return;);
	
	if (!(HASHBIN_GET_SIZE(log)))
		return;
	
	client = (irlmp_client_t *) hashbin_get_first(irlmp->clients);
	while (client != NULL) {
		/* Check if we should notify client */
		irlmp_notify_client(client, log, mode);
			
		client = (irlmp_client_t *) hashbin_get_next(irlmp->clients);
	}
}

/*
 * Function irlmp_discovery_expiry (expiry)
 *
 *	This device is no longer been discovered, and therefore it is beeing
 *	purged from the discovery log. Inform all clients who have
 *	registered for this event...
 * 
 *	Note : called exclusively from discovery.c
 *	Note : as we are currently processing the log, the clients callback
 *	should *NOT* attempt to touch the log now.
 */
void irlmp_discovery_expiry(discovery_t *expiry) 
{
	irlmp_client_t *client;
	
	IRDA_DEBUG(3, "%s()\n", __FUNCTION__);

	ASSERT(expiry != NULL, return;);
	
	client = (irlmp_client_t *) hashbin_get_first(irlmp->clients);
	while (client != NULL) {
		/* Check if we should notify client */
		if ((client->expir_callback) &&
		    (client->hint_mask & expiry->hints.word & 0x7f7f))
			client->expir_callback(expiry, EXPIRY_TIMEOUT,
					       client->priv);

		/* Next client */
		client = (irlmp_client_t *) hashbin_get_next(irlmp->clients);
	}
}

/*
 * Function irlmp_get_discovery_response ()
 *
 *    Used by IrLAP to get the discovery info it needs when answering
 *    discovery requests by other devices.
 */
discovery_t *irlmp_get_discovery_response()
{
	IRDA_DEBUG(4, "%s()\n", __FUNCTION__);

	ASSERT(irlmp != NULL, return NULL;);

	irlmp->discovery_rsp.hints.word = irlmp->hints.word;

	/* 
	 *  Set character set for device name (we use ASCII), and 
	 *  copy device name. Remember to make room for a \0 at the 
	 *  end
	 */
	irlmp->discovery_rsp.charset = CS_ASCII;

	strncpy(irlmp->discovery_rsp.nickname, sysctl_devname, 
		NICKNAME_MAX_LEN);
	irlmp->discovery_rsp.name_len = strlen(irlmp->discovery_rsp.nickname);

	return &irlmp->discovery_rsp;
}

/*
 * Function irlmp_data_request (self, skb)
 *
 *    Send some data to peer device
 *
 */
int irlmp_data_request(struct lsap_cb *self, struct sk_buff *skb) 
{
	ASSERT(self != NULL, return -1;);
	ASSERT(self->magic == LMP_LSAP_MAGIC, return -1;);
	
	/* Make room for MUX header */
	ASSERT(skb_headroom(skb) >= LMP_HEADER, return -1;);
	skb_push(skb, LMP_HEADER);
	
	return irlmp_do_lsap_event(self, LM_DATA_REQUEST, skb);
}

/*
 * Function irlmp_data_indication (handle, skb)
 *
 *    Got data from LAP layer so pass it up to upper layer
 *
 */
void irlmp_data_indication(struct lsap_cb *self, struct sk_buff *skb) 
{
	/* Hide LMP header from layer above */
	skb_pull(skb, LMP_HEADER);

	if (self->notify.data_indication)
		self->notify.data_indication(self->notify.instance, self, skb);
	else
		dev_kfree_skb(skb);
}

/*
 * Function irlmp_udata_request (self, skb)
 *
 *    
 *
 */
int irlmp_udata_request(struct lsap_cb *self, struct sk_buff *skb) 
{
 	IRDA_DEBUG(4, "%s()\n", __FUNCTION__); 

	ASSERT(skb != NULL, return -1;);
	
	/* Make room for MUX header */
	ASSERT(skb_headroom(skb) >= LMP_HEADER, return -1;);
	skb_push(skb, LMP_HEADER);

	return irlmp_do_lsap_event(self, LM_UDATA_REQUEST, skb);
}

/*
 * Function irlmp_udata_indication (self, skb)
 *
 *    Send unreliable data (but still within the connection)
 *
 */
void irlmp_udata_indication(struct lsap_cb *self, struct sk_buff *skb) 
{
 	IRDA_DEBUG(4, "%s()\n", __FUNCTION__); 

	ASSERT(self != NULL, return;);
	ASSERT(self->magic == LMP_LSAP_MAGIC, return;);
	ASSERT(skb != NULL, return;);

	/* Hide LMP header from layer above */
	skb_pull(skb, LMP_HEADER);

	if (self->notify.udata_indication)
		self->notify.udata_indication(self->notify.instance, self, 
					      skb);
	else
		dev_kfree_skb(skb);
}

/*
 * Function irlmp_connless_data_request (self, skb)
 *
 *    
 *
 */
#ifdef CONFIG_IRDA_ULTRA
int irlmp_connless_data_request(struct lsap_cb *self, struct sk_buff *skb) 
{
	struct sk_buff *clone_skb;
	struct lap_cb *lap;

 	IRDA_DEBUG(4, "%s()\n", __FUNCTION__); 

	ASSERT(skb != NULL, return -1;);
	
	/* Make room for MUX and PID header */
	ASSERT(skb_headroom(skb) >= LMP_HEADER+LMP_PID_HEADER, return -1;);
	
	/* Insert protocol identifier */
	skb_push(skb, LMP_PID_HEADER);
	skb->data[0] = self->pid;

	/* Connectionless sockets must use 0x70 */
	skb_push(skb, LMP_HEADER);
	skb->data[0] = skb->data[1] = LSAP_CONNLESS;

	/* Try to send Connectionless  packets out on all links */
	lap = (struct lap_cb *) hashbin_get_first(irlmp->links);
	while (lap != NULL) {
		ASSERT(lap->magic == LMP_LAP_MAGIC, return -1;);

		clone_skb = skb_clone(skb, GFP_ATOMIC);
		if (!clone_skb)
			return -ENOMEM;

		irlap_unitdata_request(lap->irlap, clone_skb);
		
		lap = (struct lap_cb *) hashbin_get_next(irlmp->links);
	}
	dev_kfree_skb(skb);

	return 0;
}
#endif /* CONFIG_IRDA_ULTRA */

/*
 * Function irlmp_connless_data_indication (self, skb)
 *
 *    Receive unreliable data outside any connection. Mostly used by Ultra
 *
 */
#ifdef CONFIG_IRDA_ULTRA
void irlmp_connless_data_indication(struct lsap_cb *self, struct sk_buff *skb) 
{
 	IRDA_DEBUG(4, "%s()\n", __FUNCTION__); 

	ASSERT(self != NULL, return;);
	ASSERT(self->magic == LMP_LSAP_MAGIC, return;);
	ASSERT(skb != NULL, return;);

	/* Hide LMP and PID header from layer above */
	skb_pull(skb, LMP_HEADER+LMP_PID_HEADER);

	if (self->notify.udata_indication)
		self->notify.udata_indication(self->notify.instance, self,
					      skb);
	else
		dev_kfree_skb(skb);
}
#endif /* CONFIG_IRDA_ULTRA */

void irlmp_status_request(void) 
{
	IRDA_DEBUG(0, "%s(), Not implemented\n", __FUNCTION__);
}

/*
 * Propagate status indication from LAP to LSAPs (via LMP)
 * This don't trigger any change of state in lap_cb, lmp_cb or lsap_cb,
 * and the event is stateless, therefore we can bypass both state machines
 * and send the event direct to the LSAP user.
 * Jean II
 */
void irlmp_status_indication(struct lap_cb *self,
			     LINK_STATUS link, LOCK_STATUS lock) 
{
	struct lsap_cb *next;
	struct lsap_cb *curr;

	/* Send status_indication to all LSAPs using this link */
	next = (struct lsap_cb *) hashbin_get_first( self->lsaps);
	while (next != NULL ) {
		curr = next;
		next = (struct lsap_cb *) hashbin_get_next(self->lsaps);

		ASSERT(curr->magic == LMP_LSAP_MAGIC, return;);
		/*
		 *  Inform service user if he has requested it
		 */
		if (curr->notify.status_indication != NULL)
			curr->notify.status_indication(curr->notify.instance, 
						       link, lock);
		else
			IRDA_DEBUG(2, "%s(), no handler\n", __FUNCTION__);
	}
}

/*
 * Receive flow control indication from LAP.
 * LAP want us to send it one more frame. We implement a simple round
 * robin scheduler between the active sockets so that we get a bit of
 * fairness. Note that the round robin is far from perfect, but it's
 * better than nothing.
 * We then poll the selected socket so that we can do synchronous
 * refilling of IrLAP (which allow to minimise the number of buffers).
 * Jean II
 */
void irlmp_flow_indication(struct lap_cb *self, LOCAL_FLOW flow)
{
	struct lsap_cb *next;
	struct lsap_cb *curr;
	int	lsap_todo;

	ASSERT(self->magic == LMP_LAP_MAGIC, return;);
	ASSERT(flow == FLOW_START, return;);

	/* Get the number of lsap. That's the only safe way to know
	 * that we have looped around... - Jean II */
	lsap_todo = HASHBIN_GET_SIZE(self->lsaps);
	IRDA_DEBUG(4, __FUNCTION__ "() : %d lsaps to scan\n", lsap_todo);

	/* Poll lsap in order until the queue is full or until we
	 * tried them all.
	 * Most often, the current LSAP will have something to send,
	 * so we will go through this loop only once. - Jean II */
	while((lsap_todo--) &&
	      (IRLAP_GET_TX_QUEUE_LEN(self->irlap) < LAP_HIGH_THRESHOLD)) {
		/* Try to find the next lsap we should poll. */
		next = self->flow_next;
		if(next != NULL) {
			/* Note that if there is only one LSAP on the LAP
			 * (most common case), self->flow_next is always NULL,
			 * so we always avoid this loop. - Jean II */
			IRDA_DEBUG(4, __FUNCTION__ "() : searching my LSAP\n");

			/* We look again in hashbins, because the lsap
			 * might have gone away... - Jean II */
			curr = (struct lsap_cb *) hashbin_get_first(self->lsaps);
			while((curr != NULL ) && (curr != next))
				curr = (struct lsap_cb *) hashbin_get_next(self->lsaps);
		} else
			curr = NULL;

		/* If we have no lsap, restart from first one */
		if(curr == NULL)
			curr = (struct lsap_cb *) hashbin_get_first(self->lsaps);
		/* Uh-oh... Paranoia */
		if(curr == NULL)
			break;

		/* Next time, we will get the next one (or the first one) */
		self->flow_next = (struct lsap_cb *) hashbin_get_next(self->lsaps);
		IRDA_DEBUG(4, __FUNCTION__ "() : curr is %p, next was %p and is now %p, still %d to go - queue len = %d\n", curr, next, self->flow_next, lsap_todo, IRLAP_GET_TX_QUEUE_LEN(self->irlap));

		/* Inform lsap user that it can send one more packet. */
		if (curr->notify.flow_indication != NULL)
			curr->notify.flow_indication(curr->notify.instance, 
						     curr, flow);
		else
			IRDA_DEBUG(1, __FUNCTION__ "(), no handler\n");
	}
}

/*
 * Function irlmp_hint_to_service (hint)
 *
 *    Returns a list of all servics contained in the given hint bits. This
 *    funtion assumes that the hint bits have the size of two bytes only
 */
__u8 *irlmp_hint_to_service(__u8 *hint)
{
	__u8 *service;
	int i = 0;

	/* 
	 * Allocate array to store services in. 16 entries should be safe 
	 * since we currently only support 2 hint bytes
	 */
	service = kmalloc(16, GFP_ATOMIC);
	if (!service) {
		IRDA_DEBUG(1, "%s(), Unable to kmalloc!\n", __FUNCTION__);
		return NULL;
	}

	if (!hint[0]) {
		IRDA_DEBUG(1, "<None>\n");
		kfree(service);
		return NULL;
	}
	if (hint[0] & HINT_PNP)
		IRDA_DEBUG(1, "PnP Compatible ");
	if (hint[0] & HINT_PDA)
		IRDA_DEBUG(1, "PDA/Palmtop ");
	if (hint[0] & HINT_COMPUTER)
		IRDA_DEBUG(1, "Computer ");
	if (hint[0] & HINT_PRINTER) {
		IRDA_DEBUG(1, "Printer ");
		service[i++] = S_PRINTER;
	}
	if (hint[0] & HINT_MODEM)
		IRDA_DEBUG(1, "Modem ");
	if (hint[0] & HINT_FAX)
		IRDA_DEBUG(1, "Fax ");
	if (hint[0] & HINT_LAN) {
		IRDA_DEBUG(1, "LAN Access ");		
		service[i++] = S_LAN;
	}
	/* 
	 *  Test if extension byte exists. This byte will usually be
	 *  there, but this is not really required by the standard.
	 *  (IrLMP p. 29)
	 */
	if (hint[0] & HINT_EXTENSION) {
		if (hint[1] & HINT_TELEPHONY) {
			IRDA_DEBUG(1, "Telephony ");
			service[i++] = S_TELEPHONY;
		} if (hint[1] & HINT_FILE_SERVER)
			IRDA_DEBUG(1, "File Server ");
		
		if (hint[1] & HINT_COMM) {
			IRDA_DEBUG(1, "IrCOMM ");
			service[i++] = S_COMM;
		}
		if (hint[1] & HINT_OBEX) {
			IRDA_DEBUG(1, "IrOBEX ");
			service[i++] = S_OBEX;
		}
	}
	IRDA_DEBUG(1, "\n");

	/* So that client can be notified about any discovery */
	service[i++] = S_ANY;

	service[i] = S_END;
	
	return service;
}

/*
 * Function irlmp_service_to_hint (service)
 *
 *    Converts a service type, to a hint bit
 *
 *    Returns: a 16 bit hint value, with the service bit set
 */
__u16 irlmp_service_to_hint(int service)
{
	__u16_host_order hint;

	hint.word = 0;

	switch (service) {
	case S_PNP:
		hint.byte[0] |= HINT_PNP;
		break;
	case S_PDA:
		hint.byte[0] |= HINT_PDA;
		break;
	case S_COMPUTER:
		hint.byte[0] |= HINT_COMPUTER;
		break;
	case S_PRINTER:
		hint.byte[0] |= HINT_PRINTER;
		break;
	case S_MODEM:
		hint.byte[0] |= HINT_PRINTER;
		break;
	case S_LAN:
		hint.byte[0] |= HINT_LAN;
		break;
	case S_COMM:
		hint.byte[0] |= HINT_EXTENSION;
		hint.byte[1] |= HINT_COMM;
		break;
	case S_OBEX:
		hint.byte[0] |= HINT_EXTENSION;
		hint.byte[1] |= HINT_OBEX;
		break;
	case S_TELEPHONY:
		hint.byte[0] |= HINT_EXTENSION;
		hint.byte[1] |= HINT_TELEPHONY;
		break;
	case S_ANY:
		hint.word = 0xffff;
		break;
	default:
		IRDA_DEBUG( 1, "%s(), Unknown service!\n", __FUNCTION__);
		break;
	}
	return hint.word;
}

/*
 * Function irlmp_register_service (service)
 *
 *    Register local service with IrLMP
 *
 */
__u32 irlmp_register_service(__u16 hints)
{
	irlmp_service_t *service;
	__u32 handle;

	IRDA_DEBUG(4, "%s(), hints = %04x\n", __FUNCTION__, hints);

	/* Get a unique handle for this service */
	get_random_bytes(&handle, sizeof(handle));
	while (hashbin_find(irlmp->services, handle, NULL) || !handle)
		get_random_bytes(&handle, sizeof(handle));

	irlmp->hints.word |= hints;

	/* Make a new registration */
 	service = kmalloc(sizeof(irlmp_service_t), GFP_ATOMIC);
	if (!service) {
		IRDA_DEBUG(1, "%s(), Unable to kmalloc!\n", __FUNCTION__);
		return 0;
	}
	service->hints = hints;
	hashbin_insert(irlmp->services, (irda_queue_t *) service, handle, NULL);

	return handle;
}

/*
 * Function irlmp_unregister_service (handle)
 *
 *    Unregister service with IrLMP. 
 *
 *    Returns: 0 on success, -1 on error
 */
int irlmp_unregister_service(__u32 handle)
{
	irlmp_service_t *service;
		
 	IRDA_DEBUG(4, "%s()\n", __FUNCTION__);

	if (!handle)
		return -1;
 
	service = hashbin_find(irlmp->services, handle, NULL);
	if (!service) {
		IRDA_DEBUG(1, "%s(), Unknown service!\n", __FUNCTION__);
		return -1;
	}

	service = hashbin_remove(irlmp->services, handle, NULL);
	if (service)
		kfree(service);

	/* Remove old hint bits */
	irlmp->hints.word = 0;

	/* Refresh current hint bits */
        service = (irlmp_service_t *) hashbin_get_first(irlmp->services);
        while (service) {
		irlmp->hints.word |= service->hints;

                service = (irlmp_service_t *)hashbin_get_next(irlmp->services);
        }
	return 0;
}

/*
 * Function irlmp_register_client (hint_mask, callback1, callback2)
 *
 *    Register a local client with IrLMP
 *	First callback is selective discovery (based on hints)
 *	Second callback is for selective discovery expiries
 *
 *    Returns: handle > 0 on success, 0 on error
 */
__u32 irlmp_register_client(__u16 hint_mask, DISCOVERY_CALLBACK1 disco_clb,
			    DISCOVERY_CALLBACK1 expir_clb, void *priv)
{
	irlmp_client_t *client;
	__u32 handle;

	IRDA_DEBUG(1, "%s()\n", __FUNCTION__);
	ASSERT(irlmp != NULL, return 0;);
	
	/* Get a unique handle for this client */
	get_random_bytes(&handle, sizeof(handle));
	while (hashbin_find(irlmp->clients, handle, NULL) || !handle)
		get_random_bytes(&handle, sizeof(handle));

	/* Make a new registration */
 	client = kmalloc(sizeof(irlmp_client_t), GFP_ATOMIC);
	if (!client) {
		IRDA_DEBUG( 1, "%s(), Unable to kmalloc!\n", __FUNCTION__);
		return 0;
	}

	/* Register the details */
	client->hint_mask = hint_mask;
	client->disco_callback = disco_clb;
	client->expir_callback = expir_clb;
	client->priv = priv;

 	hashbin_insert(irlmp->clients, (irda_queue_t *) client, handle, NULL);

	return handle;
}

/*
 * Function irlmp_update_client (handle, hint_mask, callback1, callback2)
 *
 *    Updates specified client (handle) with possibly new hint_mask and
 *    callback
 *
 *    Returns: 0 on success, -1 on error
 */
int irlmp_update_client(__u32 handle, __u16 hint_mask, 
			DISCOVERY_CALLBACK1 disco_clb, 
			DISCOVERY_CALLBACK1 expir_clb, void *priv)
{
	irlmp_client_t *client;

	if (!handle)
		return -1;

	client = hashbin_find(irlmp->clients, handle, NULL);
	if (!client) {
		IRDA_DEBUG(1, "%s(), Unknown client!\n", __FUNCTION__);
		return -1;
	}

	client->hint_mask = hint_mask;
	client->disco_callback = disco_clb;
	client->expir_callback = expir_clb;
	client->priv = priv;
	
	return 0;
}

/*
 * Function irlmp_unregister_client (handle)
 *
 *    Returns: 0 on success, -1 on error
 *
 */
int irlmp_unregister_client(__u32 handle)
{
 	struct irlmp_client *client;
 
 	IRDA_DEBUG(4, "%s()\n", __FUNCTION__);

	if (!handle)
		return -1;
 
	client = hashbin_find(irlmp->clients, handle, NULL);
	if (!client) {
		IRDA_DEBUG(1, "%s(), Unknown client!\n", __FUNCTION__);
		return -1;
	}

	IRDA_DEBUG( 4, "%s(), removing client!\n", __FUNCTION__);
	client = hashbin_remove( irlmp->clients, handle, NULL);
	if (client)
		kfree(client);
	
	return 0;
}

/*
 * Function irlmp_slsap_inuse (slsap)
 *
 *    Check if the given source LSAP selector is in use
 */
int irlmp_slsap_inuse(__u8 slsap_sel)
{
	struct lsap_cb *self;
	struct lap_cb *lap;

	ASSERT(irlmp != NULL, return TRUE;);
	ASSERT(irlmp->magic == LMP_MAGIC, return TRUE;);
	ASSERT(slsap_sel != LSAP_ANY, return TRUE;);

	IRDA_DEBUG(4, "%s()\n", __FUNCTION__);

#ifdef CONFIG_IRDA_ULTRA
	/* Accept all bindings to the connectionless LSAP */
	if (slsap_sel == LSAP_CONNLESS)
		return FALSE;
#endif /* CONFIG_IRDA_ULTRA */

	/* Valid values are between 0 and 127 */
	if (slsap_sel > LSAP_MAX)
		return TRUE;

	/*
	 *  Check if slsap is already in use. To do this we have to loop over
	 *  every IrLAP connection and check every LSAP assosiated with each
	 *  the connection.
	 */
	lap = (struct lap_cb *) hashbin_get_first(irlmp->links);
	while (lap != NULL) {
		ASSERT(lap->magic == LMP_LAP_MAGIC, return TRUE;);

		self = (struct lsap_cb *) hashbin_get_first(lap->lsaps);
		while (self != NULL) {
			ASSERT(self->magic == LMP_LSAP_MAGIC, return TRUE;);

			if ((self->slsap_sel == slsap_sel)) {
				IRDA_DEBUG(4, "Source LSAP selector=%02x in use\n",
				      self->slsap_sel); 
				return TRUE;
			}
			self = (struct lsap_cb*) hashbin_get_next(lap->lsaps);
		}
		lap = (struct lap_cb *) hashbin_get_next(irlmp->links);
	}     
	return FALSE;
}

/*
 * Function irlmp_find_free_slsap ()
 *
 *    Find a free source LSAP to use. This function is called if the service
 *    user has requested a source LSAP equal to LM_ANY
 */
__u8 irlmp_find_free_slsap(void) 
{
	__u8 lsap_sel;
	int wrapped = 0;

	ASSERT(irlmp != NULL, return -1;);
	ASSERT(irlmp->magic == LMP_MAGIC, return -1;);
      
	lsap_sel = irlmp->free_lsap_sel++;
	
	/* Check if the new free lsap is really free */
	while (irlmp_slsap_inuse(irlmp->free_lsap_sel)) {
		irlmp->free_lsap_sel++;

		/* Check if we need to wraparound (0x70-0x7f are reserved) */
		if (irlmp->free_lsap_sel > LSAP_MAX) {
			irlmp->free_lsap_sel = 10;

			/* Make sure we terminate the loop */
			if (wrapped++)
				return 0;
		}
	}
	IRDA_DEBUG(4, "%s(), next free lsap_sel=%02x\n", __FUNCTION__, lsap_sel);
	
	return lsap_sel;
}

/*
 * Function irlmp_convert_lap_reason (lap_reason)
 *
 *    Converts IrLAP disconnect reason codes to IrLMP disconnect reason
 *    codes
 *
 */
LM_REASON irlmp_convert_lap_reason( LAP_REASON lap_reason)
{
	int reason = LM_LAP_DISCONNECT;

	switch (lap_reason) {		
	case LAP_DISC_INDICATION: /* Received a disconnect request from peer */
		IRDA_DEBUG( 1, "%s(), LAP_DISC_INDICATION\n", __FUNCTION__);
		reason = LM_USER_REQUEST;
		break;
	case LAP_NO_RESPONSE:    /* To many retransmits without response */
		IRDA_DEBUG( 1, "%s(), LAP_NO_RESPONSE\n", __FUNCTION__);
		reason = LM_LAP_DISCONNECT;
		break;
	case LAP_RESET_INDICATION:
		IRDA_DEBUG( 1, "%s(), LAP_RESET_INDICATION\n", __FUNCTION__);
		reason = LM_LAP_RESET;
		break;
	case LAP_FOUND_NONE:
	case LAP_MEDIA_BUSY:
	case LAP_PRIMARY_CONFLICT:
		IRDA_DEBUG(1, "%s(), LAP_FOUND_NONE, LAP_MEDIA_BUSY or LAP_PRIMARY_CONFLICT\n", __FUNCTION__);
		reason = LM_CONNECT_FAILURE;
		break;
	default:
		IRDA_DEBUG(1, "%s(), Unknow IrLAP disconnect reason %d!\n", 
			__FUNCTION__, lap_reason);
		reason = LM_LAP_DISCONNECT;
		break;
	}

	return reason;
}	

__u32 irlmp_get_saddr(struct lsap_cb *self)
{
	ASSERT(self != NULL, return 0;);
	ASSERT(self->lap != NULL, return 0;);

	return self->lap->saddr;
}

__u32 irlmp_get_daddr(struct lsap_cb *self)
{
	ASSERT(self != NULL, return 0;);
	ASSERT(self->lap != NULL, return 0;);
	
	return self->lap->daddr;
}

#ifdef CONFIG_PROC_FS
/*
 * Function irlmp_proc_read (buf, start, offset, len, unused)
 *
 *    Give some info to the /proc file system
 *
 */
int irlmp_proc_read(char *buf, char **start, off_t offset, int len)
{
	struct lsap_cb *self;
	struct lap_cb *lap;
	unsigned long flags;

	ASSERT(irlmp != NULL, return 0;);
	
	save_flags( flags);
	cli();

	len = 0;
	
	len += sprintf( buf+len, "Unconnected LSAPs:\n");
	self = (struct lsap_cb *) hashbin_get_first( irlmp->unconnected_lsaps);
	while (self != NULL) {
		ASSERT(self->magic == LMP_LSAP_MAGIC, break;);
		len += sprintf(buf+len, "lsap state: %s, ", 
			       irlsap_state[ self->lsap_state]);
		len += sprintf(buf+len, 
			       "slsap_sel: %#02x, dlsap_sel: %#02x, ",
			       self->slsap_sel, self->dlsap_sel); 
		len += sprintf(buf+len, "(%s)", self->notify.name);
		len += sprintf(buf+len, "\n");

		self = (struct lsap_cb *) hashbin_get_next(
			irlmp->unconnected_lsaps);
 	} 

	len += sprintf(buf+len, "\nRegistred Link Layers:\n");

	lap = (struct lap_cb *) hashbin_get_first(irlmp->links);
	while (lap != NULL) {
		len += sprintf(buf+len, "lap state: %s, ", 
			       irlmp_state[lap->lap_state]);

		len += sprintf(buf+len, "saddr: %#08x, daddr: %#08x, ",
			       lap->saddr, lap->daddr); 
		len += sprintf(buf+len, "num lsaps: %d",
			       HASHBIN_GET_SIZE(lap->lsaps));
		len += sprintf(buf+len, "\n");

		len += sprintf(buf+len, "\n  Connected LSAPs:\n");
		self = (struct lsap_cb *) hashbin_get_first(lap->lsaps);
		while (self != NULL) {
			ASSERT(self->magic == LMP_LSAP_MAGIC, break;);
			len += sprintf(buf+len, "  lsap state: %s, ", 
				       irlsap_state[ self->lsap_state]);
			len += sprintf(buf+len, 
				       "slsap_sel: %#02x, dlsap_sel: %#02x, ",
				       self->slsap_sel, self->dlsap_sel);
			len += sprintf(buf+len, "(%s)", self->notify.name);
			len += sprintf(buf+len, "\n");
			
			self = (struct lsap_cb *) hashbin_get_next( 
				lap->lsaps);
		} 
		len += sprintf(buf+len, "\n");

		lap = (struct lap_cb *) hashbin_get_next(irlmp->links);
 	} 
	restore_flags(flags);
	
	return len;
}

#endif /* PROC_FS */



