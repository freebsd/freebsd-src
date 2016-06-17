/*****************************************************************************/
/*
 *      auerchain.c  --  Auerswald PBX/System Telephone chained urb support.
 *
 *      Copyright (C) 2002  Wolfgang Mües (wolfgang@iksw-muees.de)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 /*****************************************************************************/

#undef DEBUG			/* include debug macros until it's done */
#include <linux/usb.h>
#include "auerchain.h"
#include <linux/slab.h>

/* completion function for chained urbs */
static void auerchain_complete(struct urb *urb)
{
	unsigned long flags;
	int result;

	/* get pointer to element and to chain */
	struct auerchainelement *acep =
	    (struct auerchainelement *) urb->context;
	struct auerchain *acp = acep->chain;

	/* restore original entries in urb */
	urb->context = acep->context;
	urb->complete = acep->complete;

	dbg("auerchain_complete called");

	/* call original completion function
	   NOTE: this function may lead to more urbs submitted into the chain.
	   (no chain lock at calling complete()!)
	   acp->active != NULL is protecting us against recursion. */
	urb->complete(urb);

	/* detach element from chain data structure */
	spin_lock_irqsave(&acp->lock, flags);
	if (acp->active != acep)	/* paranoia debug check */
		dbg("auerchain_complete: completion on non-active element called!");
	else
		acp->active = NULL;

	/* add the used chain element to the list of free elements */
	list_add_tail(&acep->list, &acp->free_list);
	acep = NULL;

	/* is there a new element waiting in the chain? */
	if (!acp->active && !list_empty(&acp->waiting_list)) {
		/* yes: get the entry */
		struct list_head *tmp = acp->waiting_list.next;
		list_del(tmp);
		acep = list_entry(tmp, struct auerchainelement, list);
		acp->active = acep;
	}
	spin_unlock_irqrestore(&acp->lock, flags);

	/* submit the new urb */
	if (acep) {
		urb = acep->urbp;
		dbg("auerchain_complete: submitting next urb from chain");
		urb->status = 0;	/* needed! */
		result = usb_submit_urb(urb);

		/* check for submit errors */
		if (result) {
			urb->status = result;
			dbg("auerchain_complete: usb_submit_urb with error code %d", result);
			/* and do error handling via *this* completion function (recursive) */
			auerchain_complete(urb);
		}
	} else {
		/* simple return without submitting a new urb.
		   The empty chain is detected with acp->active == NULL. */
	};
}

/* submit function for chained urbs
   this function may be called from completion context or from user space!
   early = 1 -> submit in front of chain
*/
int auerchain_submit_urb_list(struct auerchain *acp, struct urb *urb,
			      int early)
{
	int result;
	unsigned long flags;
	struct auerchainelement *acep = NULL;

	dbg("auerchain_submit_urb called");

	/* try to get a chain element */
	spin_lock_irqsave(&acp->lock, flags);
	if (!list_empty(&acp->free_list)) {
		/* yes: get the entry */
		struct list_head *tmp = acp->free_list.next;
		list_del(tmp);
		acep = list_entry(tmp, struct auerchainelement, list);
	}
	spin_unlock_irqrestore(&acp->lock, flags);

	/* if no chain element available: return with error */
	if (!acep) {
		return -ENOMEM;
	}

	/* fill in the new chain element values */
	acep->chain = acp;
	acep->context = urb->context;
	acep->complete = urb->complete;
	acep->urbp = urb;
	INIT_LIST_HEAD(&acep->list);

	/* modify urb */
	urb->context = acep;
	urb->complete = auerchain_complete;
	urb->status = -EINPROGRESS;	/* usb_submit_urb does this, too */

	/* add element to chain - or start it immediately */
	spin_lock_irqsave(&acp->lock, flags);
	if (acp->active) {
		/* there is traffic in the chain, simple add element to chain */
		if (early) {
			dbg("adding new urb to head of chain");
			list_add(&acep->list, &acp->waiting_list);
		} else {
			dbg("adding new urb to end of chain");
			list_add_tail(&acep->list, &acp->waiting_list);
		}
		acep = NULL;
	} else {
		/* the chain is empty. Prepare restart */
		acp->active = acep;
	}
	/* Spin has to be removed before usb_submit_urb! */
	spin_unlock_irqrestore(&acp->lock, flags);

	/* Submit urb if immediate restart */
	if (acep) {
		dbg("submitting urb immediate");
		urb->status = 0;	/* needed! */
		result = usb_submit_urb(urb);
		/* check for submit errors */
		if (result) {
			urb->status = result;
			dbg("auerchain_submit_urb: usb_submit_urb with error code %d", result);
			/* and do error handling via completion function */
			auerchain_complete(urb);
		}
	}

	return 0;
}

/* submit function for chained urbs
   this function may be called from completion context or from user space!
*/
int auerchain_submit_urb(struct auerchain *acp, struct urb *urb)
{
	return auerchain_submit_urb_list(acp, urb, 0);
}

/* cancel an urb which is submitted to the chain
   the result is 0 if the urb is cancelled, or -EINPROGRESS if
   USB_ASYNC_UNLINK is set and the function is successfully started.
*/
int auerchain_unlink_urb(struct auerchain *acp, struct urb *urb)
{
	unsigned long flags;
	struct urb *urbp;
	struct auerchainelement *acep;
	struct list_head *tmp;

	dbg("auerchain_unlink_urb called");

	/* search the chain of waiting elements */
	spin_lock_irqsave(&acp->lock, flags);
	list_for_each(tmp, &acp->waiting_list) {
		acep = list_entry(tmp, struct auerchainelement, list);
		if (acep->urbp == urb) {
			list_del(tmp);
			urb->context = acep->context;
			urb->complete = acep->complete;
			list_add_tail(&acep->list, &acp->free_list);
			spin_unlock_irqrestore(&acp->lock, flags);
			dbg("unlink waiting urb");
			urb->status = -ENOENT;
			urb->complete(urb);
			return 0;
		}
	}
	/* not found. */
	spin_unlock_irqrestore(&acp->lock, flags);

	/* get the active urb */
	acep = acp->active;
	if (acep) {
		urbp = acep->urbp;

		/* check if we have to cancel the active urb */
		if (urbp == urb) {
			/* note that there is a race condition between the check above
			   and the unlink() call because of no lock. This race is harmless,
			   because the usb module will detect the unlink() after completion.
			   We can't use the acp->lock here because the completion function
			   wants to grab it.
			 */
			dbg("unlink active urb");
			return usb_unlink_urb(urbp);
		}
	}

	/* not found anyway
	   ... is some kind of success
	 */
	dbg("urb to unlink not found in chain");
	return 0;
}

/* cancel all urbs which are in the chain.
   this function must not be called from interrupt or completion handler.
*/
void auerchain_unlink_all(struct auerchain *acp)
{
	unsigned long flags;
	struct urb *urbp;
	struct auerchainelement *acep;

	dbg("auerchain_unlink_all called");

	/* clear the chain of waiting elements */
	spin_lock_irqsave(&acp->lock, flags);
	while (!list_empty(&acp->waiting_list)) {
		/* get the next entry */
		struct list_head *tmp = acp->waiting_list.next;
		list_del(tmp);
		acep = list_entry(tmp, struct auerchainelement, list);
		urbp = acep->urbp;
		urbp->context = acep->context;
		urbp->complete = acep->complete;
		list_add_tail(&acep->list, &acp->free_list);
		spin_unlock_irqrestore(&acp->lock, flags);
		dbg("unlink waiting urb");
		urbp->status = -ENOENT;
		urbp->complete(urbp);
		spin_lock_irqsave(&acp->lock, flags);
	}
	spin_unlock_irqrestore(&acp->lock, flags);

	/* clear the active urb */
	acep = acp->active;
	if (acep) {
		urbp = acep->urbp;
		urbp->transfer_flags &= ~USB_ASYNC_UNLINK;
		dbg("unlink active urb");
		usb_unlink_urb(urbp);
	}
}


/* free the chain.
   this function must not be called from interrupt or completion handler.
*/
void auerchain_free(struct auerchain *acp)
{
	unsigned long flags;
	struct auerchainelement *acep;

	dbg("auerchain_free called");

	/* first, cancel all pending urbs */
	auerchain_unlink_all(acp);

	/* free the elements */
	spin_lock_irqsave(&acp->lock, flags);
	while (!list_empty(&acp->free_list)) {
		/* get the next entry */
		struct list_head *tmp = acp->free_list.next;
		list_del(tmp);
		spin_unlock_irqrestore(&acp->lock, flags);
		acep = list_entry(tmp, struct auerchainelement, list);
		kfree(acep);
		spin_lock_irqsave(&acp->lock, flags);
	}
	spin_unlock_irqrestore(&acp->lock, flags);
}


/* Init the chain control structure */
void auerchain_init(struct auerchain *acp)
{
	/* init the chain data structure */
	acp->active = NULL;
	spin_lock_init(&acp->lock);
	INIT_LIST_HEAD(&acp->waiting_list);
	INIT_LIST_HEAD(&acp->free_list);
}

/* setup a chain.
   It is assumed that there is no concurrency while setting up the chain
   requirement: auerchain_init()
*/
int auerchain_setup(struct auerchain *acp, unsigned int numElements)
{
	struct auerchainelement *acep;

	dbg("auerchain_setup called with %d elements", numElements);

	/* fill the list of free elements */
	for (; numElements; numElements--) {
		acep =
		    (struct auerchainelement *)
		    kmalloc(sizeof(struct auerchainelement), GFP_KERNEL);
		if (!acep)
			goto ac_fail;
		memset(acep, 0, sizeof(struct auerchainelement));
		INIT_LIST_HEAD(&acep->list);
		list_add_tail(&acep->list, &acp->free_list);
	}
	return 0;

      ac_fail:	/* free the elements */
	while (!list_empty(&acp->free_list)) {
		/* get the next entry */
		struct list_head *tmp = acp->free_list.next;
		list_del(tmp);
		acep = list_entry(tmp, struct auerchainelement, list);
		kfree(acep);
	}
	return -ENOMEM;
}


/* completion handler for synchronous chained URBs */
static void auerchain_blocking_completion(struct urb *urb)
{
	struct auerchain_chs *pchs = (struct auerchain_chs *) urb->context;
	pchs->done = 1;
	wmb();
	wake_up(&pchs->wqh);
}


/* Starts chained urb and waits for completion or timeout */
static int auerchain_start_wait_urb(struct auerchain *acp, struct urb *urb,
				    int timeout, int *actual_length)
{
	DECLARE_WAITQUEUE(wait, current);
	struct auerchain_chs chs;
	int status;

	dbg("auerchain_start_wait_urb called");
	init_waitqueue_head(&chs.wqh);
	chs.done = 0;

	set_current_state(TASK_UNINTERRUPTIBLE);
	add_wait_queue(&chs.wqh, &wait);
	urb->context = &chs;
	status = auerchain_submit_urb(acp, urb);
	if (status) {
		/* something went wrong */
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&chs.wqh, &wait);
		return status;
	}

	while (timeout && !chs.done) {
		timeout = schedule_timeout(timeout);
		set_current_state(TASK_UNINTERRUPTIBLE);
		rmb();
	}

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&chs.wqh, &wait);

	if (!timeout && !chs.done) {
		if (urb->status != -EINPROGRESS) {	/* No callback?!! */
			dbg("auerchain_start_wait_urb: raced timeout");
			status = urb->status;
		} else {
			dbg("auerchain_start_wait_urb: timeout");
			auerchain_unlink_urb(acp, urb);	/* remove urb safely */
			status = -ETIMEDOUT;
		}
	} else
		status = urb->status;

	if (actual_length)
		*actual_length = urb->actual_length;

	return status;
}


/* auerchain_control_msg - Builds a control urb, sends it off and waits for completion
   acp: pointer to the auerchain
   dev: pointer to the usb device to send the message to
   pipe: endpoint "pipe" to send the message to
   request: USB message request value
   requesttype: USB message request type value
   value: USB message value
   index: USB message index value
   data: pointer to the data to send
   size: length in bytes of the data to send
   timeout: time to wait for the message to complete before timing out (if 0 the wait is forever)

   This function sends a simple control message to a specified endpoint
   and waits for the message to complete, or timeout.

   If successful, it returns the transfered length, othwise a negative error number.

   Don't use this function from within an interrupt context, like a
   bottom half handler.  If you need a asyncronous message, or need to send
   a message from within interrupt context, use auerchain_submit_urb()
*/
int auerchain_control_msg(struct auerchain *acp, struct usb_device *dev,
			  unsigned int pipe, __u8 request,
			  __u8 requesttype, __u16 value, __u16 index,
			  void *data, __u16 size, int timeout)
{
	int ret;
	struct usb_ctrlrequest *dr;
	struct urb *urb;
	int length;

	dbg("auerchain_control_msg");
	dr = (struct usb_ctrlrequest *)
	    kmalloc(sizeof(struct usb_ctrlrequest), GFP_KERNEL);
	if (!dr)
		return -ENOMEM;
	urb = usb_alloc_urb(0);
	if (!urb) {
		kfree(dr);
		return -ENOMEM;
	}

	dr->bRequestType = requesttype;
	dr->bRequest = request;
	dr->wValue = cpu_to_le16(value);
	dr->wIndex = cpu_to_le16(index);
	dr->wLength = cpu_to_le16(size);

	FILL_CONTROL_URB(urb, dev, pipe, (unsigned char *) dr, data, size,	/* build urb */
			 (usb_complete_t) auerchain_blocking_completion,
			 0);
	ret = auerchain_start_wait_urb(acp, urb, timeout, &length);

	usb_free_urb(urb);
	kfree(dr);

	if (ret < 0)
		return ret;
	else
		return length;
}
