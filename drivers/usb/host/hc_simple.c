/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*
 * simple generic USB HCD frontend Version 0.9.5 (10/28/2001)
 * for embedded HCs (SL811HS)
 * 
 * USB URB handling, hci_ hcs_
 * URB queueing, qu_
 * Transfer scheduling, sh_
 * 
 *
 *-------------------------------------------------------------------------*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *-------------------------------------------------------------------------*/

/* main lock for urb access */
static spinlock_t usb_urb_lock = SPIN_LOCK_UNLOCKED;

/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/
/* URB HCD API function layer
 * * * */

/***************************************************************************
 * Function Name : hcs_urb_queue
 *
 * This function initializes the urb status and length before queueing the 
 * urb. 
 *
 * Input:  hci = data structure for the host controller
 *         urb = USB request block data structure 
 *
 * Return: 0 
 **************************************************************************/
static inline int hcs_urb_queue (hci_t * hci, struct urb * urb)
{
	int i;

	DBGFUNC ("enter hcs_urb_queue\n");
	if (usb_pipeisoc (urb->pipe)) {
		DBGVERBOSE ("hcs_urb_queue: isoc pipe\n");
		for (i = 0; i < urb->number_of_packets; i++) {
			urb->iso_frame_desc[i].actual_length = 0;
			urb->iso_frame_desc[i].status = -EXDEV;
		}

		/* urb->next hack : 1 .. resub, 0 .. single shot */
		/* urb->interval = urb->next ? 1 : 0; */
	}

	urb->status = -EINPROGRESS;
	urb->actual_length = 0;
	urb->error_count = 0;

	if (usb_pipecontrol (urb->pipe))
		hc_flush_data_cache (hci, urb->setup_packet, 8);
	if (usb_pipeout (urb->pipe))
		hc_flush_data_cache (hci, urb->transfer_buffer,
				     urb->transfer_buffer_length);

	qu_queue_urb (hci, urb);

	return 0;
}

/***************************************************************************
 * Function Name : hcs_return_urb
 *
 * This function the return path of URB back to the USB core. It calls the
 * the urb complete function if exist, and also handles the resubmition of
 * interrupt URBs.
 *
 * Input:  hci = data structure for the host controller
 *         urb = USB request block data structure 
 *         resub_ok = resubmit flag: 1 = submit urb again, 0 = not submit 
 *
 * Return: 0 
 **************************************************************************/
static int hcs_return_urb (hci_t * hci, struct urb * urb, int resub_ok)
{
	struct usb_device *dev = urb->dev;
	int resubmit = 0;

	DBGFUNC ("enter hcs_return_urb, urb pointer = %p, "
		 "transferbuffer point = %p, "
		 " setup packet pointer = %p, context pointer = %p \n",
		 (__u32 *) urb, (__u32 *) urb->transfer_buffer,
		 (__u32 *) urb->setup_packet, (__u32 *) urb->context);
	if (urb_debug)
		urb_print (urb, "RET", usb_pipeout (urb->pipe));

	resubmit = urb->interval && resub_ok;

	urb->dev = urb->hcpriv = NULL;

	if (urb->complete) {
		urb->complete (urb);	/* call complete */
	}

	if (resubmit) {
		/* requeue the URB */
		urb->dev = dev;
		hcs_urb_queue (hci, urb);
	}

	return 0;
}

/***************************************************************************
 * Function Name : hci_submit_urb
 *
 * This function is called by the USB core API when an URB is available to
 * process.  This function does the following
 *
 * 1) Check the validity of the URB
 * 2) Parse the device number from the URB
 * 3) Pass the URB to the root hub routine if its intended for the hub, else
 *    queue the urb for the attached device. 
 *
 * Input: urb = USB request block data structure 
 *
 * Return: 0 if success or error code 
 **************************************************************************/
static int hci_submit_urb (struct urb * urb)
{
	hci_t *hci;
	unsigned int pipe = urb->pipe;
	unsigned long flags;
	int ret;

	DBGFUNC ("enter hci_submit_urb, pipe = 0x%x\n", urb->pipe);
	if (!urb->dev || !urb->dev->bus || urb->hcpriv)
		return -EINVAL;

	if (usb_endpoint_halted
	    (urb->dev, usb_pipeendpoint (pipe), usb_pipeout (pipe))) {
		printk ("hci_submit_urb: endpoint_halted\n");
		return -EPIPE;
	}
	hci = (hci_t *) urb->dev->bus->hcpriv;

	/* a request to the virtual root hub */

	if (usb_pipedevice (pipe) == hci->rh.devnum) {
		if (urb_debug > 1)
			urb_print (urb, "SUB-RH", usb_pipein (pipe));

		return rh_submit_urb (urb);
	}

	/* queue the URB to its endpoint-queue */

	spin_lock_irqsave (&usb_urb_lock, flags);
	ret = hcs_urb_queue (hci, urb);
	if (ret != 0) {
		/* error on return */
		DBGERR
		    ("hci_submit_urb: return err, ret = 0x%x, urb->status = 0x%x\n",
		     ret, urb->status);
	}

	spin_unlock_irqrestore (&usb_urb_lock, flags);

	return ret;

}

/***************************************************************************
 * Function Name : hci_unlink_urb
 *
 * This function mark the URB to unlink
 *
 * Input: urb = USB request block data structure 
 *
 * Return: 0 if success or error code 
 **************************************************************************/
static int hci_unlink_urb (struct urb * urb)
{
	unsigned long flags;
	hci_t *hci;
	DECLARE_WAITQUEUE (wait, current);
	void *comp = NULL;

	DBGFUNC ("enter hci_unlink_urb\n");

	if (!urb)		/* just to be sure */
		return -EINVAL;

	if (!urb->dev || !urb->dev->bus)
		return -ENODEV;

	hci = (hci_t *) urb->dev->bus->hcpriv;

	/* a request to the virtual root hub */
	if (usb_pipedevice (urb->pipe) == hci->rh.devnum) {
		return rh_unlink_urb (urb);
	}

	if (urb_debug)
		urb_print (urb, "UNLINK", 1);

	spin_lock_irqsave (&usb_urb_lock, flags);

	if (!list_empty (&urb->urb_list) && urb->status == -EINPROGRESS) {
		/* URB active? */

		if (urb->
		    transfer_flags & (USB_ASYNC_UNLINK | USB_TIMEOUT_KILLED)) {
			/* asynchron with callback */

			list_del (&urb->urb_list);	/* relink the urb to the del list */
			list_add (&urb->urb_list, &hci->del_list);
			spin_unlock_irqrestore (&usb_urb_lock, flags);

		} else {
			/* synchron without callback */

			add_wait_queue (&hci->waitq, &wait);

			set_current_state (TASK_UNINTERRUPTIBLE);
			comp = urb->complete;
			urb->complete = NULL;

/* --> crash --> */	list_del (&urb->urb_list);	/* relink the urb to the del list */
			list_add (&urb->urb_list, &hci->del_list);

			spin_unlock_irqrestore (&usb_urb_lock, flags);

			schedule_timeout (HZ / 50);

			if (!list_empty (&urb->urb_list))
				list_del (&urb->urb_list);

			urb->complete = comp;
			urb->hcpriv = NULL;
			remove_wait_queue (&hci->waitq, &wait);
		}
	} else {
		/* hcd does not own URB but we keep the driver happy anyway */
		spin_unlock_irqrestore (&usb_urb_lock, flags);

		if (urb->complete && (urb->transfer_flags & USB_ASYNC_UNLINK)) {
			urb->status = -ENOENT;
			urb->actual_length = 0;
			urb->complete (urb);
			urb->status = 0;
		} else {
			urb->status = -ENOENT;
		}
	}

	return 0;
}

/***************************************************************************
 * Function Name : hci_alloc_dev
 *
 * This function allocates private data space for the usb device and 
 * initialize the endpoint descriptor heads.
 *
 * Input: usb_dev = pointer to the usb device 
 *
 * Return: 0 if success or error code 
 **************************************************************************/
static int hci_alloc_dev (struct usb_device *usb_dev)
{
	struct hci_device *dev;
	int i;

	DBGFUNC ("enter hci_alloc_dev\n");
	dev = kmalloc (sizeof (*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	memset (dev, 0, sizeof (*dev));

	for (i = 0; i < 32; i++) {
		INIT_LIST_HEAD (&(dev->ed[i].urb_queue));
		dev->ed[i].pipe_head = NULL;
	}

	usb_dev->hcpriv = dev;

	DBGVERBOSE ("USB HC dev alloc %d bytes\n", sizeof (*dev));

	return 0;

}

/***************************************************************************
 * Function Name : hci_free_dev
 *
 * This function de-allocates private data space for the usb devic
 *
 * Input: usb_dev = pointer to the usb device 
 *
 * Return: 0  
 **************************************************************************/
static int hci_free_dev (struct usb_device *usb_dev)
{
	DBGFUNC ("enter hci_free_dev\n");

	if (usb_dev->hcpriv)
		kfree (usb_dev->hcpriv);

	usb_dev->hcpriv = NULL;

	return 0;
}

/***************************************************************************
 * Function Name : hci_get_current_frame_number
 *
 * This function get the current USB frame number
 *
 * Input: usb_dev = pointer to the usb device 
 *
 * Return: frame number  
 **************************************************************************/
static int hci_get_current_frame_number (struct usb_device *usb_dev)
{
	hci_t *hci = usb_dev->bus->hcpriv;
	DBGFUNC ("enter hci_get_current_frame_number, frame = 0x%x \r\n",
		 hci->frame_number);

	return (hci->frame_number);
}

/***************************************************************************
 * List of all io-functions 
 **************************************************************************/

static struct usb_operations hci_device_operations = {
	allocate:		hci_alloc_dev,
	deallocate:		hci_free_dev,
	get_frame_number:	hci_get_current_frame_number,
	submit_urb:		hci_submit_urb,
	unlink_urb:		hci_unlink_urb,
};

/***************************************************************************
 * URB queueing:
 * 
 * For each type of transfer (INTR, BULK, ISO, CTRL) there is a list of 
 * active URBs.
 * (hci->intr_list, hci->bulk_list, hci->iso_list, hci->ctrl_list)
 * For every endpoint the head URB of the queued URBs is linked to one of 
 * those lists.
 * 
 * The rest of the queued URBs of an endpoint are linked into a 
 * private URB list for each endpoint. (hci_dev->ed [endpoint_io].urb_queue)
 * hci_dev->ed [endpoint_io].pipe_head .. points to the head URB which is 
 * in one of the active URB lists.
 * 
 * The index of an endpoint consists of its number and its direction.
 * 
 * The state of an intr and iso URB is 0. 
 * For ctrl URBs the states are US_CTRL_SETUP, US_CTRL_DATA, US_CTRL_ACK
 * Bulk URBs states are US_BULK and US_BULK0 (with 0-len packet)
 * 
 **************************************************************************/

/***************************************************************************
 * Function Name : qu_urb_timeout
 *
 * This function is called when the URB timeout. The function unlinks the 
 * URB. 
 *
 * Input: lurb: URB 
 *
 * Return: none  
 **************************************************************************/
#ifdef HC_URB_TIMEOUT
static void qu_urb_timeout (unsigned long lurb)
{
	struct urb *urb = (struct urb *) lurb;

	DBGFUNC ("enter qu_urb_timeout\n");
	urb->transfer_flags |= USB_TIMEOUT_KILLED;
	hci_unlink_urb (urb);
}
#endif

/***************************************************************************
 * Function Name : qu_pipeindex
 *
 * This function gets the index of the pipe.   
 *
 * Input: pipe: the urb pipe 
 *
 * Return: index  
 **************************************************************************/
static inline int qu_pipeindex (__u32 pipe)
{
	DBGFUNC ("enter qu_pipeindex\n");
	return (usb_pipeendpoint (pipe) << 1) | (usb_pipecontrol (pipe) ? 0 : usb_pipeout (pipe));
}

/***************************************************************************
 * Function Name : qu_seturbstate
 *
 * This function set the state of the URB.  
 * 
 * control pipe: 3 states -- Setup, data, status
 * interrupt and bulk pipe: 1 state -- data    
 *
 * Input: urb = USB request block data structure 
 *        state = the urb state
 *
 * Return: none  
 **************************************************************************/
static inline void qu_seturbstate (struct urb * urb, int state)
{
	DBGFUNC ("enter qu_seturbstate\n");
	urb->pipe &= ~0x1f;
	urb->pipe |= state & 0x1f;
}

/***************************************************************************
 * Function Name : qu_urbstate
 *
 * This function get the current state of the URB.  
 * 
 * Input: urb = USB request block data structure 
 *
 * Return: none  
 **************************************************************************/
static inline int qu_urbstate (struct urb * urb)
{

	DBGFUNC ("enter qu_urbstate\n");

	return urb->pipe & 0x1f;
}

/***************************************************************************
 * Function Name : qu_queue_active_urb
 *
 * This function adds the urb to the appropriate active urb list and set
 * the urb state.
 * 
 * There are four active lists: isochoronous list, interrupt list, 
 * control list, and bulk list.
 * 
 * Input: hci = data structure for the host controller 
 *        urb = USB request block data structure 
 *        ed = endpoint descriptor
 *
 * Return: none  
 **************************************************************************/
static inline void qu_queue_active_urb (hci_t * hci, struct urb * urb, epd_t * ed)
{
	int urb_state = 0;
	DBGFUNC ("enter qu_queue_active_urb\n");
	switch (usb_pipetype (urb->pipe)) {
	case PIPE_CONTROL:
		list_add (&urb->urb_list, &hci->ctrl_list);
		urb_state = US_CTRL_SETUP;
		break;

	case PIPE_BULK:
		list_add (&urb->urb_list, &hci->bulk_list);
		if ((urb->transfer_flags & USB_ZERO_PACKET)
		    && urb->transfer_buffer_length > 0
		    &&
		    ((urb->transfer_buffer_length %
		      usb_maxpacket (urb->dev, urb->pipe,
				     usb_pipeout (urb->pipe))) == 0)) {
			urb_state = US_BULK0;
		}
		break;

	case PIPE_INTERRUPT:
		urb->start_frame = hci->frame_number;
		list_add (&urb->urb_list, &hci->intr_list);
		break;

	case PIPE_ISOCHRONOUS:
		list_add (&urb->urb_list, &hci->iso_list);
		break;
	}

#ifdef HC_URB_TIMEOUT
	if (urb->timeout) {
		ed->timeout.data = (unsigned long) urb;
		ed->timeout.expires = urb->timeout + jiffies;
		ed->timeout.function = qu_urb_timeout;
		add_timer (&ed->timeout);
	}
#endif

	qu_seturbstate (urb, urb_state);
}

/***************************************************************************
 * Function Name : qu_queue_urb
 *
 * This function adds the urb to the endpoint descriptor list 
 * 
 * Input: hci = data structure for the host controller 
 *        urb = USB request block data structure 
 *
 * Return: none  
 **************************************************************************/
static int qu_queue_urb (hci_t * hci, struct urb * urb)
{
	struct hci_device *hci_dev = usb_to_hci (urb->dev);
	epd_t *ed = &hci_dev->ed[qu_pipeindex (urb->pipe)];

	DBGFUNC ("Enter qu_queue_urb\n");

	/* for ISOC transfers calculate start frame index */

	if (usb_pipeisoc (urb->pipe) && urb->transfer_flags & USB_ISO_ASAP) {
		urb->start_frame = ((ed->pipe_head) ? (ed->last_iso + 1) : hci_get_current_frame_number (urb-> dev) + 1) & 0xffff;
	}

	if (ed->pipe_head) {
		__list_add (&urb->urb_list, ed->urb_queue.prev,
			    &(ed->urb_queue));
	} else {
		ed->pipe_head = urb;
		qu_queue_active_urb (hci, urb, ed);
		if (++hci->active_urbs == 1)
			hc_start_int (hci);
	}

	return 0;
}

/***************************************************************************
 * Function Name : qu_next_urb
 *
 * This function removes the URB from the queue and add the next URB to 
 * active list. 
 * 
 * Input: hci = data structure for the host controller 
 *        urb = USB request block data structure 
 *        resub_ok = resubmit flag
 *
 * Return: pointer to the next urb  
 **************************************************************************/
static struct urb *qu_next_urb (hci_t * hci, struct urb * urb, int resub_ok)
{
	struct hci_device *hci_dev = usb_to_hci (urb->dev);
	epd_t *ed = &hci_dev->ed[qu_pipeindex (urb->pipe)];

	DBGFUNC ("enter qu_next_urb\n");
	list_del (&urb->urb_list);
	INIT_LIST_HEAD (&urb->urb_list);
	if (ed->pipe_head == urb) {

#ifdef HC_URB_TIMEOUT
		if (urb->timeout)
			del_timer (&ed->timeout);
#endif

		if (!--hci->active_urbs)
			hc_stop_int (hci);

		if (!list_empty (&ed->urb_queue)) {
			urb = list_entry (ed->urb_queue.next, struct urb, urb_list);
			list_del (&urb->urb_list);
			INIT_LIST_HEAD (&urb->urb_list);
			ed->pipe_head = urb;
			qu_queue_active_urb (hci, urb, ed);
		} else {
			ed->pipe_head = NULL;
			urb = NULL;
		}
	}
	return urb;
}

/***************************************************************************
 * Function Name : qu_return_urb
 *
 * This function is part of the return path.   
 * 
 * Input: hci = data structure for the host controller 
 *        urb = USB request block data structure 
 *        resub_ok = resubmit flag
 *
 * Return: pointer to the next urb  
 **************************************************************************/
static struct urb *qu_return_urb (hci_t * hci, struct urb * urb, int resub_ok)
{
	struct urb *next_urb;

	DBGFUNC ("enter qu_return_rub\n");
	next_urb = qu_next_urb (hci, urb, resub_ok);
	hcs_return_urb (hci, urb, resub_ok);
	return next_urb;
}

#if 0 /* unused now (hne) */
/***************************************************************************
 * Function Name : sh_scan_iso_urb_list
 *
 * This function goes throught the isochronous urb list and schedule the 
 * the transfer.   
 *
 * Note: This function has not tested yet
 * 
 * Input: hci = data structure for the host controller 
 *        list_lh = pointer to the isochronous list 
 *        frame_number = the frame number 
 *
 * Return: 0 = unsuccessful; 1 = successful  
 **************************************************************************/
static int sh_scan_iso_urb_list (hci_t * hci, struct list_head *list_lh,
				 int frame_number)
{
	struct list_head *lh = list_lh->next;
	struct urb *urb;

	DBGFUNC ("enter sh_scan_iso_urb_list\n");
	hci->td_array->len = 0;

	while (lh != list_lh) {
		urb = list_entry (lh, struct urb, urb_list);
		lh = lh->next;
		if (((frame_number - urb->start_frame) & 0x7ff) <
		    urb->number_of_packets) {
			if (!sh_add_packet (hci, urb)) {
				return 0;
			} else {
				if (((frame_number -
				      urb->start_frame) & 0x7ff) > 0x400) {
					if (qu_urbstate (urb) > 0)
						urb = qu_return_urb (hci, urb, 1);
					else
						urb = qu_next_urb (hci, urb, 1);

					if (lh == list_lh && urb)
						lh = &urb->urb_list;
				}
			}
		}
	}
	return 1;
}
#endif // if0

/***************************************************************************
 * Function Name : sh_scan_urb_list
 *
 * This function goes through the urb list and schedule the 
 * the transaction.   
 * 
 * Input: hci = data structure for the host controller 
 *        list_lh = pointer to the isochronous list 
 *
 * Return: 0 = unsuccessful; 1 = successful  
 **************************************************************************/
static int sh_scan_urb_list (hci_t * hci, struct list_head *list_lh)
{
	struct list_head *lh = NULL;
	struct urb *urb;

	if (list_lh == NULL) {
		DBGERR ("sh_scan_urb_list: error, list_lh == NULL\n");
	}

	DBGFUNC ("enter sh_scan_urb_list: frame# \n");

	list_for_each (lh, list_lh) {
		urb = list_entry (lh, struct urb, urb_list);
		if (urb == NULL)
			return 1;
		if (!usb_pipeint (urb->pipe)
		    || (((hci->frame_number - urb->start_frame)
			 & 0x7ff) >= urb->interval)) {
			DBGVERBOSE ("sh_scan_urb_list !INT: %d fr_no: %d int: %d pint: %d\n",
				    urb->start_frame, hci->frame_number, urb->interval,
				    usb_pipeint (urb->pipe));
			if (!sh_add_packet (hci, urb)) {
				return 0;
			} else {
				DBGVERBOSE ("INT: start: %d fr_no: %d int: %d pint: %d\n",
					    urb->start_frame, hci->frame_number,
					    urb->interval, usb_pipeint (urb->pipe));
				urb->start_frame = hci->frame_number;
				return 0;

			}
		}
	}
	return 1;
}

/***************************************************************************
 * Function Name : sh_shedule_trans
 *
 * This function schedule the USB transaction.
 * This function will process the endpoint in the following order: 
 * interrupt, control, and bulk.    
 * 
 * Input: hci = data structure for the host controller 
 *        isSOF = flag indicate if Start Of Frame has occurred 
 *
 * Return: 0   
 **************************************************************************/
static int sh_schedule_trans (hci_t * hci, int isSOF)
{
	int units_left = 1;
	struct list_head *lh;

	if (hci == NULL) {
		DBGERR ("sh_schedule_trans: hci == NULL\n");
		return 0;
	}
	if (hci->td_array == NULL) {
		DBGERR ("sh_schedule_trans: hci->td_array == NULL\n");
		return 0;
	}

	if (hci->td_array->len != 0) {
		DBGERR ("ERROR: schedule, hci->td_array->len = 0x%x, s/b: 0\n",
			hci->td_array->len);
	}

	/* schedule the next available interrupt transfer or the next
	 * stage of the interrupt transfer */

	if (hci->td_array->len == 0 && !list_empty (&hci->intr_list)) {
		units_left = sh_scan_urb_list (hci, &hci->intr_list);
	}

	/* schedule the next available control transfer or the next
	 * stage of the control transfer */

	if (hci->td_array->len == 0 && !list_empty (&hci->ctrl_list) && units_left > 0) {
		units_left = sh_scan_urb_list (hci, &hci->ctrl_list);
	}

	/* schedule the next available bulk transfer or the next
	 * stage of the bulk transfer */

	if (hci->td_array->len == 0 && !list_empty (&hci->bulk_list) && units_left > 0) {
		sh_scan_urb_list (hci, &hci->bulk_list);

		/* be fair to each BULK URB (move list head around) 
		 * only when the new SOF happens */

		lh = hci->bulk_list.next;
		list_del (&hci->bulk_list);
		list_add (&hci->bulk_list, lh);
	}
	return 0;
}

/***************************************************************************
 * Function Name : sh_add_packet
 *
 * This function forms the packet and transmit the packet. This function
 * will handle all endpoint type: isochoronus, interrupt, control, and 
 * bulk.
 * 
 * Input: hci = data structure for the host controller 
 *        urb = USB request block data structure 
 *
 * Return: 0 = unsucessful; 1 = successful   
 **************************************************************************/
static int sh_add_packet (hci_t * hci, struct urb * urb)
{
	__u8 *data = NULL;
	int len = 0;
	int toggle = 0;
	int maxps = usb_maxpacket (urb->dev, urb->pipe, usb_pipeout (urb->pipe));
	int endpoint = usb_pipeendpoint (urb->pipe);
	int address = usb_pipedevice (urb->pipe);
	int slow = (((urb->pipe) >> 26) & 1);
	int out = usb_pipeout (urb->pipe);
	int pid = 0;
	int ret;
	int i = 0;
	int iso = 0;

	DBGFUNC ("enter sh_add_packet\n");
	if (maxps == 0)
		maxps = 8;

	/* calculate len, toggle bit and add the transaction */
	switch (usb_pipetype (urb->pipe)) {
	case PIPE_ISOCHRONOUS:
		pid = out ? PID_OUT : PID_IN;
		iso = 1;
		i = hci->frame_number - urb->start_frame;
		data = urb->transfer_buffer + urb->iso_frame_desc[i].offset;
		len = urb->iso_frame_desc[i].length;
		break;

	case PIPE_BULK:	/* BULK and BULK0 */
	case PIPE_INTERRUPT:
		pid = out ? PID_OUT : PID_IN;
		len = urb->transfer_buffer_length - urb->actual_length;
		data = urb->transfer_buffer + urb->actual_length;
		toggle = usb_gettoggle (urb->dev, endpoint, out);
		break;

	case PIPE_CONTROL:
		switch (qu_urbstate (urb)) {
		case US_CTRL_SETUP:
			len = 8;
			pid = PID_SETUP;
			data = urb->setup_packet;
			toggle = 0;
			break;

		case US_CTRL_DATA:
			if (!hci->last_packet_nak) {
				/* The last packet received is not a nak:
				 * reset the nak count
				 */

				hci->nakCnt = 0;
			}
			if (urb->transfer_buffer_length != 0) {
				pid = out ? PID_OUT : PID_IN;
				len = urb->transfer_buffer_length - urb->actual_length;
				data = urb->transfer_buffer + urb->actual_length;
				toggle = (urb->actual_length & maxps) ? 0 : 1;
				usb_settoggle (urb->dev,
					       usb_pipeendpoint (urb->pipe),
					       usb_pipeout (urb->pipe), toggle);
				break;
			} else {
				/* correct state and fall through */
				qu_seturbstate (urb, US_CTRL_ACK);
			}

		case US_CTRL_ACK:
			len = 0;

			/* reply in opposite direction */
			pid = !out ? PID_OUT : PID_IN;
			toggle = 1;
			usb_settoggle (urb->dev, usb_pipeendpoint (urb->pipe),
				       usb_pipeout (urb->pipe), toggle);
			break;
		}
	}

	ret =
	    hc_add_trans (hci, len, data, toggle, maxps, slow, endpoint,
			  address, pid, iso, qu_urbstate (urb));

	DBGVERBOSE ("transfer_pa: addr:%d ep:%d pid:%x tog:%x iso:%x sl:%x "
		    "max:%d\n len:%d ret:%d data:%p left:%d\n",
		    address, endpoint, pid, toggle, iso, slow,
		    maxps, len, ret, data, hci->hp.units_left);

	if (ret >= 0) {
		hci->td_array->td[hci->td_array->len].urb = urb;
		hci->td_array->td[hci->td_array->len].len = ret;
		hci->td_array->td[hci->td_array->len].iso_index = i;
		hci->td_array->len++;
		hci->active_trans = 1;
		return 1;
	}
	return 0;
}

/***************************************************************************
 * Function Name : cc_to_error
 *
 * This function maps the SL811HS hardware error code to the linux USB error
 * code.
 * 
 * Input: cc = hardware error code 
 *
 * Return: USB error code   
 **************************************************************************/
static int cc_to_error (int cc)
{
	int errCode = 0;
	if (cc & SL11H_STATMASK_ERROR) {
		errCode |= -EILSEQ;
	} else if (cc & SL11H_STATMASK_OVF) {
		errCode |= -EOVERFLOW;
	} else if (cc & SL11H_STATMASK_STALL) {
		errCode |= -EPIPE;
	}
	return errCode;
}

/***************************************************************************
 * Function Name : sh_done_list
 *
 * This function process the packet when it has done finish transfer.
 * 
 * 1) It handles hardware error
 * 2) It updates the URB state
 * 3) If the USB transaction is complete, it start the return stack path.
 * 
 * Input: hci = data structure for the host controller 
 *        isExcessNak = flag tells if there excess NAK condition occurred 
 *
 * Return:  urb_state or -1 if the transaction has complete   
 **************************************************************************/
static int sh_done_list (hci_t * hci, int *isExcessNak)
{
	int actbytes = 0;
	int active = 0;
	void *data = NULL;
	int cc;
	int maxps;
	int toggle;
	struct urb *urb;
	int urb_state = 0;
	int ret = 1;		/* -1 parse abbort, 1 parse ok, 0 last element */
	int trans = 0;
	int len;
	int iso_index = 0;
	int out;
	int pid = 0;
	int debugLen = 0;

	*isExcessNak = 0;

	DBGFUNC ("enter sh_done_list: td_array->len = 0x%x\n",
		 hci->td_array->len);

	debugLen = hci->td_array->len;
	if (debugLen > 1)
		DBGERR ("sh_done_list: td_array->len = 0x%x > 1\n",
			hci->td_array->len);

	for (trans = 0; ret && trans < hci->td_array->len && trans < MAX_TRANS;
	     trans++) {
		urb = hci->td_array->td[trans].urb;
		/* FIXME: */
		/* +++ I'm sorry, can't handle NULL-Pointers 21.11.2002 (hne) */
		if (!urb) {
			DBGERR ("sh_done_list: urb = NULL\n");
			continue;
		}
		if (!urb->dev || !urb->pipe) {
			if (!urb->dev) DBGERR ("sh_done_list: urb->dev = NULL\n");
			if (!urb->pipe) DBGERR ("sh_done_list: urb->pipe = NULL\n");
			continue;
		}
		/* --- 21.11.2002 (hne) */
		
		len = hci->td_array->td[trans].len;
		out = usb_pipeout (urb->pipe);

		if (usb_pipeisoc (urb->pipe)) {
			iso_index = hci->td_array->td[trans].iso_index;
			data = urb->transfer_buffer + urb->iso_frame_desc[iso_index].offset;
			toggle = 0;
		} else {
			data = urb->transfer_buffer + urb->actual_length;
			/* +++ Crash (hne)  urb->dev == NULL !!! */
			toggle = usb_gettoggle (urb->dev,
						usb_pipeendpoint (urb->pipe),
						usb_pipeout (urb->pipe));
			/* --- Crash (hne)  urb->dev == NULL !!! */

		}
		urb_state = qu_urbstate (urb);
		pid = out ? PID_OUT : PID_IN;
		ret = hc_parse_trans (hci, &actbytes, data, &cc, &toggle, len,
				      pid, urb_state);
		maxps = usb_maxpacket (urb->dev, urb->pipe, usb_pipeout (urb->pipe));

		if (maxps == 0)
			maxps = 8;

		active = (urb_state != US_CTRL_SETUP) && (actbytes && !(actbytes & (maxps - 1)));

		/* If the transfer is not bulk in, then it is necessary to get all
		 * data specify by the urb->transfer_len.
		 */

		if (!(usb_pipebulk (urb->pipe) && usb_pipein (urb->pipe)))
			active = active && (urb->transfer_buffer_length != urb->actual_length + actbytes);

		if (urb->transfer_buffer_length == urb->actual_length + actbytes)
			active = 0;

		if ((cc &
		     (SL11H_STATMASK_ERROR | SL11H_STATMASK_TMOUT |
		      SL11H_STATMASK_OVF | SL11H_STATMASK_STALL))
		    && !(cc & SL11H_STATMASK_NAK)) {
			if (++urb->error_count > 3) {
				DBGERR ("done_list: excessive error: errcount = 0x%x, cc = 0x%x\n",
					urb->error_count, cc);
				urb_state = 0;
				active = 0;
			} else {
				DBGERR ("done_list: packet err, cc=0x%x, "
					" urb->length=0x%x, actual_len=0x%x,"
					" urb_state=0x%x\n",
					cc, urb->transfer_buffer_length,
					urb->actual_length, urb_state);
//			if (cc & SL11H_STATMASK_STALL) {
				/* The USB function is STALLED on a control pipe (0), 
				 * then it needs to send the SETUP command again to 
				 * clear the STALL condition
				 */

//				if (usb_pipeendpoint (urb->pipe) == 0) {
//					urb_state = 2;  
//					active = 0;
//				}
//			} else   
				active = 1;
			}
		} else {
			if (cc & SL11H_STATMASK_NAK) {
				if (hci->nakCnt < 0x10000) {
					hci->nakCnt++;
					hci->last_packet_nak = 1;
					active = 1;
					*isExcessNak = 0;
				} else {
					DBGERR ("done_list: nak count exceed limit\n");
					active = 0;
					*isExcessNak = 1;
					hci->nakCnt = 0;
				}
			} else {
				hci->nakCnt = 0;
				hci->last_packet_nak = 0;
			}

			if (urb_state != US_CTRL_SETUP) {
				/* no error */
				urb->actual_length += actbytes;
				usb_settoggle (urb->dev,
					       usb_pipeendpoint (urb->pipe),
					       usb_pipeout (urb->pipe), toggle);
			}
			if (usb_pipeisoc (urb->pipe)) {
				urb->iso_frame_desc[iso_index].actual_length = actbytes;
				urb->iso_frame_desc[iso_index].status = cc_to_error (cc);
				active = (iso_index < urb->number_of_packets);
			}
		}
		if (!active) {
			if (!urb_state) {
				urb->status = cc_to_error (cc);
				if (urb->status) {
					DBGERR ("error on received packet: urb->status = 0x%x\n",
						urb->status);
				}
				hci->td_array->len = 0;
				qu_return_urb (hci, urb, 1);
				return -1;
			} else {
				/* We do not want to decrement the urb_state if exceeded nak,
				 * because we need to finish the data stage of the control 
				 * packet 
				 */

				if (!(*isExcessNak))
					urb_state--;
				qu_seturbstate (urb, urb_state);
			}
		}
	}

	if (urb_state < 0)
		DBGERR ("ERROR: done_list, urb_state = %d, suppose > 0\n",
			urb_state);
	if (debugLen != hci->td_array->len) {
		DBGERR ("ERROR: done_list, debugLen!= td_array->len,"
			"debugLen = 0x%x, hci->td_array->len = 0x%x\n",
			debugLen, hci->td_array->len);
	}

	hci->td_array->len = 0;

	return urb_state;
}
