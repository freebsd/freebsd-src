
/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*
 * SL811HS virtual root hub
 *  
 * based on usb-ohci.c by R. Weissgaerber et al.
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

#ifdef DEBUG
#undef DEBUG
#endif
static __u32 getPortStatusAndChange (hci_t * hci);
static void setPortStatus (hci_t * hci, __u16 bitPos);
static void setPortChange (hci_t * hci, __u16 bitPos);
static void clrPortStatus (hci_t * hci, __u16 bitPos);
static void clrPortChange (hci_t * hci, __u16 bitPos);
static int USBReset (hci_t * hci);
static int cc_to_error (int cc);

/*-------------------------------------------------------------------------*
 * Virtual Root Hub 
 *-------------------------------------------------------------------------*/

/* Device descriptor */
static __u8 root_hub_dev_des[] = {
	0x12,			/*  __u8  bLength; */
	0x01,			/*  __u8  bDescriptorType; Device */
	0x10,			/*  __u16 bcdUSB; v1.1 */
	0x01,
	0x09,			/*  __u8  bDeviceClass; HUB_CLASSCODE */
	0x00,			/*  __u8  bDeviceSubClass; */
	0x00,			/*  __u8  bDeviceProtocol; */
	0x08,			/*  __u8  bMaxPacketSize0; 8 Bytes */
	0x00,			/*  __u16 idVendor; */
	0x00,
	0x00,			/*  __u16 idProduct; */
	0x00,
	0x00,			/*  __u16 bcdDevice; */
	0x00,
	0x00,			/*  __u8  iManufacturer; */
	0x02,			/*  __u8  iProduct; */
	0x01,			/*  __u8  iSerialNumber; */
	0x01			/*  __u8  bNumConfigurations; */
};

/* Configuration descriptor */
static __u8 root_hub_config_des[] = {
	0x09,			/*  __u8  bLength; */
	0x02,			/*  __u8  bDescriptorType; Configuration */
	0x19,			/*  __u16 wTotalLength; */
	0x00,
	0x01,			/*  __u8  bNumInterfaces; */
	0x01,			/*  __u8  bConfigurationValue; */
	0x00,			/*  __u8  iConfiguration; */
	0x40,			/*  __u8  bmAttributes; 
				   Bit 7: Bus-powered, 6: Self-powered, 5 Remote-wakwup, 
				   4..0: resvd */
	0x00,			/*  __u8  MaxPower; */

	/* interface */
	0x09,			/*  __u8  if_bLength; */
	0x04,			/*  __u8  if_bDescriptorType; Interface */
	0x00,			/*  __u8  if_bInterfaceNumber; */
	0x00,			/*  __u8  if_bAlternateSetting; */
	0x01,			/*  __u8  if_bNumEndpoints; */
	0x09,			/*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
	0x00,			/*  __u8  if_bInterfaceSubClass; */
	0x00,			/*  __u8  if_bInterfaceProtocol; */
	0x00,			/*  __u8  if_iInterface; */

	/* endpoint */
	0x07,			/*  __u8  ep_bLength; */
	0x05,			/*  __u8  ep_bDescriptorType; Endpoint */
	0x81,			/*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
	0x03,			/*  __u8  ep_bmAttributes; Interrupt */
	0x02,			/*  __u16 ep_wMaxPacketSize; ((MAX_ROOT_PORTS + 1) / 8 */
	0x00,
	0xff			/*  __u8  ep_bInterval; 255 ms */
};

/* [Hub class-specific descriptor is constructed dynamically] */
/* copied from "dynamic source". (hne) */
static __u8 root_hub_hub_des[] =
{
	0x09,			/*  __u8  bLength; */
	0x29,			/*  __u8  bDescriptorType; Hub-descriptor */
	0x01,			/*  __u8  bNbrPorts; */
	0x00,			/* __u16  wHubCharacteristics; */
	0x00,
	0x50,			/*  __u8  bPwrOn2pwrGood; 100ms for port reset */
	0x00,			/*  __u8  bHubContrCurrent; 0 mA */
	0xfc, /* ??? HNE */	/*  __u8  DeviceRemovable; *** 7 Ports max *** */ /* which port is attachable (HNE) */
	0xff			/*  __u8  PortPwrCtrlMask; *** 7 ports max *** */
};


/***************************************************************************
 * Function Name : rh_send_irq
 * 
 * This function examine the port change in the virtual root hub.
 * 
 * Note: This function assumes only one port exist in the root hub.
 *
 * Input:  hci = data structure for the host controller
 *         rh_data = The pointer to port change data
 *         rh_len = length of the data in bytes
 *
 * Return: length of data  
 **************************************************************************/
static int rh_send_irq (hci_t * hci, void *rh_data, int rh_len)
{
	int num_ports;
	int i;
	int ret;
	int len;
	__u8 data[8];

	DBGFUNC ("enter rh_send_irq: \n");

	/* Assuming the root hub has one port.  This value need to change if
	 * there are more than one port for the root hub
	 */

	num_ports = 1;

	/* The root hub status is not implemented, it basically has two fields:
	 *     -- Local Power Status
	 *     -- Over Current Indicator
	 *     -- Local Power Change
	 *     -- Over Current Indicator
	 *
	 * Right now, It is assume the power is good and no changes 
	 */

	*(__u8 *) data = 0;

	ret = *(__u8 *) data;

	/* Has the port status change within the root hub: It checks for
	 *      -- Port Connect Status change
	 *      -- Port Enable Change
	 */

	for (i = 0; i < num_ports; i++) {
		*(__u8 *) (data + (i + 1) / 8) |=
		    (((getPortStatusAndChange (hci) >> 16) & (PORT_CONNECT_STAT | PORT_ENABLE_STAT)) ? 1 : 0) << ((i + 1) % 8);
		ret += *(__u8 *) (data + (i + 1) / 8);

		/* After the port change is read, it should be reset so the next time 
		 * is it doesn't trigger a change again */

	}
	len = i / 8 + 1;

	if (ret > 0) {
		memcpy (rh_data, data, min (len, (int)min (rh_len, (int)sizeof (data))));
		return len;
	}
	return 0;
}

/***************************************************************************
 * Function Name : rh_int_timer_do
 * 
 * This function is called when the timer expires.  It gets the the port 
 * change data and pass along to the upper protocol.
 * 
 * Note:  The virtual root hub interrupt pipe are polled by the timer
 *        every "interval" ms
 *
 * Input:  ptr = ptr to the urb
 *
 * Return: none  
 **************************************************************************/
static void rh_int_timer_do (unsigned long ptr)
{
	int len;
	struct urb *urb = (struct urb *) ptr;
	hci_t *hci = urb->dev->bus->hcpriv;

	DBGFUNC ("enter rh_int_timer_do\n");

	if (hci->rh.send) {
		len = rh_send_irq (hci, urb->transfer_buffer,
				   urb->transfer_buffer_length);
		if (len > 0) {
			urb->actual_length = len;
			if (urb_debug == 2)
				urb_print (urb, "RET-t(rh)",
					   usb_pipeout (urb->pipe));

			if (urb->complete) {
				urb->complete (urb);
			}
		}
	}

	/* re-activate the timer */
	rh_init_int_timer (urb);
}

/***************************************************************************
 * Function Name : rh_init_int_timer
 * 
 * This function creates a timer that act as interrupt pipe in the
 * virtual hub.   
 * 
 * Note:  The virtual root hub's interrupt pipe are polled by the timer
 *        every "interval" ms
 *
 * Input: urb = USB request block 
 *
 * Return: 0  
 **************************************************************************/
static int rh_init_int_timer (struct urb * urb)
{
	hci_t *hci = urb->dev->bus->hcpriv;
	hci->rh.interval = urb->interval;

	init_timer (&hci->rh.rh_int_timer);
	hci->rh.rh_int_timer.function = rh_int_timer_do;
	hci->rh.rh_int_timer.data = (unsigned long) urb;
	hci->rh.rh_int_timer.expires = jiffies + (HZ * (urb->interval < 30 ? 30 : urb->interval)) / 1000;
	add_timer (&hci->rh.rh_int_timer);

	return 0;
}

/*-------------------------------------------------------------------------*/

/* helper macro */
#define OK(x) 			len = (x); break

/***************************************************************************
 * Function Name : rh_submit_urb
 * 
 * This function handles all USB request to the the virtual root hub
 * 
 * Input: urb = USB request block 
 *
 * Return: 0  
 **************************************************************************/
static int rh_submit_urb (struct urb * urb)
{
	struct usb_device *usb_dev = urb->dev;
	hci_t *hci = usb_dev->bus->hcpriv;
	unsigned int pipe = urb->pipe;
	struct usb_ctrlrequest *cmd = (struct usb_ctrlrequest *) urb->setup_packet;
	void *data = urb->transfer_buffer;
	int leni = urb->transfer_buffer_length;
	int len = 0;
	int status = TD_CC_NOERROR;
	__u32 datab[4];
	__u8 *data_buf = (__u8 *) datab;

	__u16 bmRType_bReq;
	__u16 wValue;
	__u16 wIndex;
	__u16 wLength;

	DBGFUNC ("enter rh_submit_urb\n");
	if (usb_pipeint (pipe)) {
		hci->rh.urb = urb;
		hci->rh.send = 1;
		hci->rh.interval = urb->interval;
		rh_init_int_timer (urb);
		urb->status = cc_to_error (TD_CC_NOERROR);

		return 0;
	}

	bmRType_bReq = cmd->bRequestType | (cmd->bRequest << 8);
	wValue = le16_to_cpu (cmd->wValue);
	wIndex = le16_to_cpu (cmd->wIndex);
	wLength = le16_to_cpu (cmd->wLength);

	DBG ("rh_submit_urb, req = %d(%x) len=%d",
	     bmRType_bReq, bmRType_bReq, wLength);

	switch (bmRType_bReq) {
		/* Request Destination:
		   without flags: Device, 
		   RH_INTERFACE: interface, 
		   RH_ENDPOINT: endpoint,
		   RH_CLASS means HUB here, 
		   RH_OTHER | RH_CLASS  almost ever means HUB_PORT here 
		 */

	case RH_GET_STATUS:
		*(__u16 *) data_buf = cpu_to_le16 (1);
		OK (2);

	case RH_GET_STATUS | RH_INTERFACE:
		*(__u16 *) data_buf = cpu_to_le16 (0);
		OK (2);

	case RH_GET_STATUS | RH_ENDPOINT:
		*(__u16 *) data_buf = cpu_to_le16 (0);
		OK (2);

	case RH_GET_STATUS | RH_CLASS:
		*(__u32 *) data_buf = cpu_to_le32 (0);
		OK (4);

	case RH_GET_STATUS | RH_OTHER | RH_CLASS:
		*(__u32 *) data_buf =
		    cpu_to_le32 (getPortStatusAndChange (hci));
		OK (4);

	case RH_CLEAR_FEATURE | RH_ENDPOINT:
		switch (wValue) {
		case (RH_ENDPOINT_STALL):
			OK (0);
		}
		break;

	case RH_CLEAR_FEATURE | RH_CLASS:
		switch (wValue) {
		case RH_C_HUB_LOCAL_POWER:
			OK (0);

		case (RH_C_HUB_OVER_CURRENT):
			/* Over Current Not Implemented */
			OK (0);
		}
		break;

	case RH_CLEAR_FEATURE | RH_OTHER | RH_CLASS:
		switch (wValue) {
		case (RH_PORT_ENABLE):
			clrPortStatus (hci, PORT_ENABLE_STAT);
			OK (0);

		case (RH_PORT_SUSPEND):
			clrPortStatus (hci, PORT_SUSPEND_STAT);
			OK (0);

		case (RH_PORT_POWER):
			clrPortStatus (hci, PORT_POWER_STAT);
			OK (0);

		case (RH_C_PORT_CONNECTION):
			clrPortChange (hci, PORT_CONNECT_STAT);
			OK (0);

		case (RH_C_PORT_ENABLE):
			clrPortChange (hci, PORT_ENABLE_STAT);
			OK (0);

		case (RH_C_PORT_SUSPEND):
			clrPortChange (hci, PORT_SUSPEND_STAT);
			OK (0);

		case (RH_C_PORT_OVER_CURRENT):
			clrPortChange (hci, PORT_OVER_CURRENT_STAT);
			OK (0);

		case (RH_C_PORT_RESET):
			clrPortChange (hci, PORT_RESET_STAT);
			OK (0);
		}
		break;

	case RH_SET_FEATURE | RH_OTHER | RH_CLASS:
		switch (wValue) {
		case (RH_PORT_SUSPEND):
			setPortStatus (hci, PORT_SUSPEND_STAT);
			OK (0);

		case (RH_PORT_RESET):
			setPortStatus (hci, PORT_RESET_STAT);
			// USBReset(hci);
			clrPortChange (hci,
				       PORT_CONNECT_CHANGE | PORT_ENABLE_CHANGE
				       | PORT_SUSPEND_CHANGE |
				       PORT_OVER_CURRENT_CHANGE);
			setPortChange (hci, PORT_RESET_CHANGE);
			clrPortStatus (hci, PORT_RESET_STAT);
			setPortStatus (hci, PORT_ENABLE_STAT);

			OK (0);

		case (RH_PORT_POWER):
			setPortStatus (hci, PORT_POWER_STAT);
			OK (0);

		case (RH_PORT_ENABLE):
			setPortStatus (hci, PORT_ENABLE_STAT);
			OK (0);
		}
		break;

	case RH_SET_ADDRESS:
		hci->rh.devnum = wValue;
		OK (0);

	case RH_GET_DESCRIPTOR:
		DBGVERBOSE ("rh_submit_urb: RH_GET_DESCRIPTOR, wValue = 0x%x\n", wValue);
		switch ((wValue & 0xff00) >> 8) {
		case (0x01):	/* device descriptor */
			len = min (leni, min ((__u16)sizeof (root_hub_dev_des), wLength));
			data_buf = root_hub_dev_des;
			OK (len);

		case (0x02):	/* configuration descriptor */
			len = min (leni, min ((__u16)sizeof (root_hub_config_des), wLength));
			data_buf = root_hub_config_des;
			OK (len);

		case (0x03):	/* string descriptors */
			len = usb_root_hub_string (wValue & 0xff, (int) (long) 0,
						   "SL811HS", data, wLength);
			if (len > 0) {
				data_buf = data;
				OK (min (leni, len));
			}

		default:
			status = SL11H_STATMASK_STALL;
		}
		break;

	case RH_GET_DESCRIPTOR | RH_CLASS:
		len = min_t(unsigned int, leni,
			  min_t(unsigned int, sizeof (root_hub_hub_des), wLength));
		memcpy (data_buf, root_hub_hub_des, len);
		OK (len);

	case RH_GET_CONFIGURATION:
		*(__u8 *) data_buf = 0x01;
		OK (1);

	case RH_SET_CONFIGURATION:
		OK (0);

	default:
		DBGERR ("unsupported root hub command");
		status = SL11H_STATMASK_STALL;
	}

	len = min (len, leni);
	if (data != data_buf)
		memcpy (data, data_buf, len);
	urb->actual_length = len;
	urb->status = cc_to_error (status);

	urb->hcpriv = NULL;
	urb->dev = NULL;
	if (urb->complete) {
		urb->complete (urb);
	}

	return 0;
}

/***************************************************************************
 * Function Name : rh_unlink_urb
 * 
 * This function unlinks the URB 
 * 
 * Input: urb = USB request block 
 *
 * Return: 0  
 **************************************************************************/
static int rh_unlink_urb (struct urb * urb)
{
	hci_t *hci = urb->dev->bus->hcpriv;

	DBGFUNC ("enter rh_unlink_urb\n");
	if (hci->rh.urb == urb) {
		hci->rh.send = 0;
		del_timer (&hci->rh.rh_int_timer);
		hci->rh.urb = NULL;

		urb->hcpriv = NULL;
		/* ---
		usb_put_dev (urb->dev);

		Can not found this function, copy from old source and other source.
		usbnet.c us this to. Its a macro to usb_dec_dev_use. (hne)
		--- */
		usb_dec_dev_use(urb->dev);
		urb->dev = NULL;
		if (urb->transfer_flags & USB_ASYNC_UNLINK) {
			urb->status = -ECONNRESET;
			if (urb->complete) {
				urb->complete (urb);
			}
		} else
			urb->status = -ENOENT;
	}
	return 0;
}

/***************************************************************************
 * Function Name : rh_connect_rh
 * 
 * This function connect the virtual root hub to the USB stack 
 * 
 * Input: urb = USB request block 
 *
 * Return: 0  
 **************************************************************************/
static int rh_connect_rh (hci_t * hci)
{
	struct usb_device *usb_dev;

	hci->rh.devnum = 0;
	usb_dev = usb_alloc_dev (NULL, hci->bus);
	if (!usb_dev)
		return -ENOMEM;

	hci->bus->root_hub = usb_dev;
	usb_connect (usb_dev);
	if (usb_new_device (usb_dev) != 0) {
		usb_free_dev (usb_dev);
		return -ENODEV;
	}

	return 0;
}
