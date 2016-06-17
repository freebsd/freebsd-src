/*****************************************************************************/

/*
 *      devio.c  --  User space communication with USB devices.
 *
 *      Copyright (C) 1999-2000  Thomas Sailer (sailer@ife.ee.ethz.ch)
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
 *
 *  $Id: devio.c,v 1.7 2000/02/01 17:28:48 fliegl Exp $
 *
 *  This file implements the usbdevfs/x/y files, where
 *  x is the bus number and y the device number.
 *
 *  It allows user space programs/"drivers" to communicate directly
 *  with USB devices without intervening kernel driver.
 *
 *  Revision history
 *    22.12.1999   0.1   Initial release (split from proc_usb.c)
 *    04.01.2000   0.2   Turned into its own filesystem
 */

/*****************************************************************************/

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/signal.h>
#include <linux/poll.h>
#include <linux/usb.h>
#include <linux/usbdevice_fs.h>
#include <asm/uaccess.h>


struct async {
        struct list_head asynclist;
        struct dev_state *ps;
	struct task_struct *task;
	unsigned int signr;
	unsigned int intf;
	void *userbuffer;
        void *userurb;
        struct urb urb;
};

static loff_t usbdev_lseek(struct file *file, loff_t offset, int orig)
{
	switch (orig) {
	case 0:
		file->f_pos = offset;
		return file->f_pos;

	case 1:
		file->f_pos += offset;
		return file->f_pos;

	case 2:
		return -EINVAL;

	default:
		return -EINVAL;
	}
}

static ssize_t usbdev_read(struct file *file, char * buf, size_t nbytes, loff_t *ppos)
{
	struct dev_state *ps = (struct dev_state *)file->private_data;
	ssize_t ret = 0;
	unsigned len;
	loff_t pos;
	int i;

	pos = *ppos;
	down_read(&ps->devsem);
	if (!ps->dev) {
		ret = -ENODEV;
		goto err;
	} else if (pos < 0) {
		ret = -EINVAL;
		goto err;
	}

	if (pos < sizeof(struct usb_device_descriptor)) {
		len = sizeof(struct usb_device_descriptor) - pos;
		if (len > nbytes)
			len = nbytes;
		if (copy_to_user(buf, ((char *)&ps->dev->descriptor) + pos, len)) {
			ret = -EFAULT;
			goto err;
		}

		*ppos += len;
		buf += len;
		nbytes -= len;
		ret += len;
	}

	pos = sizeof(struct usb_device_descriptor);
	for (i = 0; nbytes && i < ps->dev->descriptor.bNumConfigurations; i++) {
		struct usb_config_descriptor *config =
			(struct usb_config_descriptor *)ps->dev->rawdescriptors[i];
		unsigned int length = le16_to_cpu(config->wTotalLength);

		if (*ppos < pos + length) {
			len = length - (*ppos - pos);
			if (len > nbytes)
				len = nbytes;

			if (copy_to_user(buf,
			    ps->dev->rawdescriptors[i] + (*ppos - pos), len)) {
				ret = -EFAULT;
				goto err;
			}

			*ppos += len;
			buf += len;
			nbytes -= len;
			ret += len;
		}

		pos += length;
	}

err:
	up_read(&ps->devsem);
	return ret;
}

static inline unsigned int ld2(unsigned int x)
{
        unsigned int r = 0;
        
        if (x >= 0x10000) {
                x >>= 16;
                r += 16;
        }
        if (x >= 0x100) {
                x >>= 8;
                r += 8;
        }
        if (x >= 0x10) {
                x >>= 4;
                r += 4;
        }
        if (x >= 4) {
                x >>= 2;
                r += 2;
        }
        if (x >= 2)
                r++;
        return r;
}

/*
 * async list handling
 */

static struct async *alloc_async(unsigned int numisoframes)
{
        unsigned int assize = sizeof(struct async) + numisoframes * sizeof(struct iso_packet_descriptor);
        struct async *as = kmalloc(assize, GFP_KERNEL);
        if (!as)
                return NULL;
        memset(as, 0, assize);
        as->urb.number_of_packets = numisoframes;
        spin_lock_init(&as->urb.lock);
        return as;
}

static void free_async(struct async *as)
{
        if (as->urb.transfer_buffer)
                kfree(as->urb.transfer_buffer);
        if (as->urb.setup_packet)
                kfree(as->urb.setup_packet);
        kfree(as);
}

static inline void async_newpending(struct async *as)
{
        struct dev_state *ps = as->ps;
        unsigned long flags;
        
        spin_lock_irqsave(&ps->lock, flags);
        list_add_tail(&as->asynclist, &ps->async_pending);
        spin_unlock_irqrestore(&ps->lock, flags);
}

static inline void async_removepending(struct async *as)
{
        struct dev_state *ps = as->ps;
        unsigned long flags;
        
        spin_lock_irqsave(&ps->lock, flags);
        list_del_init(&as->asynclist);
        spin_unlock_irqrestore(&ps->lock, flags);
}

static inline struct async *async_getcompleted(struct dev_state *ps)
{
        unsigned long flags;
        struct async *as = NULL;

        spin_lock_irqsave(&ps->lock, flags);
        if (!list_empty(&ps->async_completed)) {
                as = list_entry(ps->async_completed.next, struct async, asynclist);
                list_del_init(&as->asynclist);
        }
        spin_unlock_irqrestore(&ps->lock, flags);
        return as;
}

static inline struct async *async_getpending(struct dev_state *ps, void *userurb)
{
        unsigned long flags;
        struct async *as;

        spin_lock_irqsave(&ps->lock, flags);
	list_for_each_entry(as, &ps->async_pending, asynclist)
		if (as->userurb == userurb) {
			list_del_init(&as->asynclist);
			spin_unlock_irqrestore(&ps->lock, flags);
			return as;
		}
        spin_unlock_irqrestore(&ps->lock, flags);
        return NULL;
}

static void async_completed(struct urb *urb)
{
        struct async *as = (struct async *)urb->context;
        struct dev_state *ps = as->ps;
	struct siginfo sinfo;

        spin_lock(&ps->lock);
        list_move_tail(&as->asynclist, &ps->async_completed);
        spin_unlock(&ps->lock);
        wake_up(&ps->wait);
	if (as->signr) {
		sinfo.si_signo = as->signr;
		sinfo.si_errno = as->urb.status;
		sinfo.si_code = SI_ASYNCIO;
		sinfo.si_addr = as->userurb;
		send_sig_info(as->signr, &sinfo, as->task);
	}
}

static void destroy_async (struct dev_state *ps, struct list_head *list)
{
	struct async *as;
	unsigned long flags;

	spin_lock_irqsave(&ps->lock, flags);
	while (!list_empty(list)) {
		as = list_entry(list->next, struct async, asynclist);
		list_del_init(&as->asynclist);
		spin_unlock_irqrestore(&ps->lock, flags);
		/* usb_unlink_urb calls the completion handler with status == USB_ST_URB_KILLED */
		usb_unlink_urb(&as->urb);
		spin_lock_irqsave(&ps->lock, flags);
	}
	spin_unlock_irqrestore(&ps->lock, flags);
	while ((as = async_getcompleted(ps)))
		free_async(as);
}

static void destroy_async_on_interface (struct dev_state *ps, unsigned int intf)
{
	struct list_head *p, *q, hitlist;
	unsigned long flags;

	INIT_LIST_HEAD(&hitlist);
	spin_lock_irqsave(&ps->lock, flags);
	list_for_each_safe(p, q, &ps->async_pending)
		if (intf == list_entry(p, struct async, asynclist)->intf)
			list_move_tail(p, &hitlist);
	spin_unlock_irqrestore(&ps->lock, flags);
	destroy_async(ps, &hitlist);
}

static inline void destroy_all_async(struct dev_state *ps)
{
	destroy_async(ps, &ps->async_pending);
}

/*
 * interface claims are made only at the request of user level code,
 * which can also release them (explicitly or by closing files).
 * they're also undone when devices disconnect.
 */

static void *driver_probe(struct usb_device *dev, unsigned int intf,
			  const struct usb_device_id *id)
{
	return NULL;
}

static void driver_disconnect(struct usb_device *dev, void *context)
{
	struct dev_state *ps = (struct dev_state *)context;

	if (!ps)
		return;

	/* this waits till synchronous requests complete */
	down_write (&ps->devsem);

	/* prevent new I/O requests */
	ps->dev = 0;
	ps->ifclaimed = 0;

	/* force async requests to complete */
	destroy_all_async (ps);

	up_write (&ps->devsem);
}

struct usb_driver usbdevfs_driver = {
	name:		"usbdevfs",
	probe:		driver_probe,
	disconnect:	driver_disconnect,
};

static int claimintf(struct dev_state *ps, unsigned int intf)
{
	struct usb_device *dev = ps->dev;
	struct usb_interface *iface;
	int err;

	if (intf >= 8*sizeof(ps->ifclaimed) || !dev || intf >= dev->actconfig->bNumInterfaces)
		return -EINVAL;
	/* already claimed */
	if (test_bit(intf, &ps->ifclaimed))
		return 0;
	iface = &dev->actconfig->interface[intf];
	err = -EBUSY;
	lock_kernel();
	if (!usb_interface_claimed(iface)) {
		usb_driver_claim_interface(&usbdevfs_driver, iface, ps);
		set_bit(intf, &ps->ifclaimed);
		err = 0;
	}
	unlock_kernel();
	return err;
}

static int releaseintf(struct dev_state *ps, unsigned int intf)
{
	struct usb_device *dev;
	struct usb_interface *iface;
	int err;

	if (intf >= 8*sizeof(ps->ifclaimed))
		return -EINVAL;
	err = -EINVAL;
	lock_kernel();
	dev = ps->dev;
	if (dev && test_and_clear_bit(intf, &ps->ifclaimed)) {
		iface = &dev->actconfig->interface[intf];
		usb_driver_release_interface(&usbdevfs_driver, iface);
		err = 0;
	}
	unlock_kernel();
	return err;
}

static int checkintf(struct dev_state *ps, unsigned int intf)
{
	if (intf >= 8*sizeof(ps->ifclaimed))
		return -EINVAL;
	if (test_bit(intf, &ps->ifclaimed))
		return 0;
	/* if not yet claimed, claim it for the driver */
	printk(KERN_WARNING "usbdevfs: process %d (%s) did not claim interface %u before use\n",
	       current->pid, current->comm, intf);
	return claimintf(ps, intf);
}

static int findintfep(struct usb_device *dev, unsigned int ep)
{
	unsigned int i, j, e;
        struct usb_interface *iface;
	struct usb_interface_descriptor *alts;
	struct usb_endpoint_descriptor *endpt;

	if (ep & ~(USB_DIR_IN|0xf))
		return -EINVAL;
	for (i = 0; i < dev->actconfig->bNumInterfaces; i++) {
		iface = &dev->actconfig->interface[i];
		for (j = 0; j < iface->num_altsetting; j++) {
                        alts = &iface->altsetting[j];
			for (e = 0; e < alts->bNumEndpoints; e++) {
				endpt = &alts->endpoint[e];
				if (endpt->bEndpointAddress == ep)
					return i;
			}
		}
	}
	return -ENOENT; 
}

static int findintfif(struct usb_device *dev, unsigned int ifn)
{
	unsigned int i, j;
        struct usb_interface *iface;
	struct usb_interface_descriptor *alts;

	if (ifn & ~0xff)
		return -EINVAL;
	for (i = 0; i < dev->actconfig->bNumInterfaces; i++) {
		iface = &dev->actconfig->interface[i];
		for (j = 0; j < iface->num_altsetting; j++) {
                        alts = &iface->altsetting[j];
			if (alts->bInterfaceNumber == ifn)
				return i;
		}
	}
	return -ENOENT; 
}

extern struct list_head usb_driver_list;

#if 0
static int finddriver(struct usb_driver **driver, char *name)
{
	struct list_head *tmp;

	tmp = usb_driver_list.next;
	while (tmp != &usb_driver_list) {
		struct usb_driver *d = list_entry(tmp, struct usb_driver,
							driver_list);

		if (!strcmp(d->name, name)) {
			*driver = d;
			return 0;
		}

		tmp = tmp->next;
	}

	return -EINVAL;
}
#endif

static int check_ctrlrecip(struct dev_state *ps, unsigned int requesttype, unsigned int index)
{
	int ret;

	if (USB_TYPE_VENDOR == (USB_TYPE_MASK & requesttype))
		return 0;

	switch (requesttype & USB_RECIP_MASK) {
	case USB_RECIP_ENDPOINT:
		if ((ret = findintfep(ps->dev, index & 0xff)) < 0)
			return ret;
		if ((ret = checkintf(ps, ret)))
			return ret;
		break;

	case USB_RECIP_INTERFACE:
		if ((ret = findintfif(ps->dev, index & 0xff)) < 0)
			return ret;
		if ((ret = checkintf(ps, ret)))
			return ret;
		break;
	}
	return 0;
}

/*
 * file operations
 */
static int usbdev_open(struct inode *inode, struct file *file)
{
	struct usb_device *dev;
	struct dev_state *ps;
	int ret;

	/* 
	 * no locking necessary here, as both sys_open (actually filp_open)
	 * and the hub thread have the kernel lock
	 * (still acquire the kernel lock for safety)
	 */
	lock_kernel();
	ret = -ENOENT;
	if (ITYPE(inode->i_ino) != IDEVICE)
		goto out;
	dev = inode->u.usbdev_i.p.dev;
	if (!dev)
		goto out;
	ret = -ENOMEM;
	if (!(ps = kmalloc(sizeof(struct dev_state), GFP_KERNEL)))
		goto out;
	ret = 0;
	ps->dev = dev;
	ps->file = file;
	spin_lock_init(&ps->lock);
	INIT_LIST_HEAD(&ps->async_pending);
	INIT_LIST_HEAD(&ps->async_completed);
	init_waitqueue_head(&ps->wait);
	init_rwsem(&ps->devsem);
	ps->discsignr = 0;
	ps->disctask = current;
	ps->disccontext = NULL;
	ps->ifclaimed = 0;
	wmb();
	list_add_tail(&ps->list, &dev->filelist);
	file->private_data = ps;
 out:
	unlock_kernel();
        return ret;
}

static int usbdev_release(struct inode *inode, struct file *file)
{
	struct dev_state *ps = (struct dev_state *)file->private_data;
	unsigned int i;

	lock_kernel();
	list_del_init(&ps->list);
	if (ps->dev) {
		for (i = 0; ps->ifclaimed && i < 8*sizeof(ps->ifclaimed); i++)
			if (test_bit(i, &ps->ifclaimed))
				releaseintf(ps, i);
	}
	unlock_kernel();
	destroy_all_async(ps);
	kfree(ps);
        return 0;
}

static int proc_control(struct dev_state *ps, void *arg)
{
	struct usb_device *dev = ps->dev;
	struct usbdevfs_ctrltransfer ctrl;
	unsigned int tmo;
	unsigned char *tbuf;
	int i, ret;

	if (copy_from_user(&ctrl, (void *)arg, sizeof(ctrl)))
		return -EFAULT;
	if ((ret = check_ctrlrecip(ps, ctrl.requesttype, ctrl.index)))
		return ret;
	if (ctrl.length > PAGE_SIZE)
		return -EINVAL;
	if (!(tbuf = (unsigned char *)__get_free_page(GFP_KERNEL)))
		return -ENOMEM;
	tmo = (ctrl.timeout * HZ + 999) / 1000;
	if (ctrl.requesttype & 0x80) {
		if (ctrl.length && !access_ok(VERIFY_WRITE, ctrl.data, ctrl.length)) {
			free_page((unsigned long)tbuf);
			return -EINVAL;
		}
		i = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0), ctrl.request, ctrl.requesttype,
				       ctrl.value, ctrl.index, tbuf, ctrl.length, tmo);
		if ((i > 0) && ctrl.length) {
			if (copy_to_user(ctrl.data, tbuf, ctrl.length)) {
				free_page((unsigned long)tbuf);
				return -EFAULT;
			}
		}
	} else {
		if (ctrl.length) {
			if (copy_from_user(tbuf, ctrl.data, ctrl.length)) {
				free_page((unsigned long)tbuf);
				return -EFAULT;
			}
		}
		i = usb_control_msg(dev, usb_sndctrlpipe(dev, 0), ctrl.request, ctrl.requesttype,
				       ctrl.value, ctrl.index, tbuf, ctrl.length, tmo);
	}
	free_page((unsigned long)tbuf);
	if (i<0) {
		printk(KERN_DEBUG "usbdevfs: USBDEVFS_CONTROL failed dev %d rqt %u rq %u len %u ret %d\n", 
		       dev->devnum, ctrl.requesttype, ctrl.request, ctrl.length, i);
	}
	return i;
}

static int proc_bulk(struct dev_state *ps, void *arg)
{
	struct usb_device *dev = ps->dev;
	struct usbdevfs_bulktransfer bulk;
	unsigned int tmo, len1, pipe;
	int len2;
	unsigned char *tbuf;
	int i, ret;

	if (copy_from_user(&bulk, (void *)arg, sizeof(bulk)))
		return -EFAULT;
	if ((ret = findintfep(ps->dev, bulk.ep)) < 0)
		return ret;
	if ((ret = checkintf(ps, ret)))
		return ret;
	if (bulk.ep & USB_DIR_IN)
		pipe = usb_rcvbulkpipe(dev, bulk.ep & 0x7f);
	else
		pipe = usb_sndbulkpipe(dev, bulk.ep & 0x7f);
	if (!usb_maxpacket(dev, pipe, !(bulk.ep & USB_DIR_IN)))
		return -EINVAL;
	len1 = bulk.len;
	if (len1 > PAGE_SIZE)
		return -EINVAL;
	if (!(tbuf = (unsigned char *)__get_free_page(GFP_KERNEL)))
		return -ENOMEM;
	tmo = (bulk.timeout * HZ + 999) / 1000;
	if (bulk.ep & 0x80) {
		if (len1 && !access_ok(VERIFY_WRITE, bulk.data, len1)) {
			free_page((unsigned long)tbuf);
			return -EINVAL;
		}
		i = usb_bulk_msg(dev, pipe, tbuf, len1, &len2, tmo);
		if (!i && len2) {
			if (copy_to_user(bulk.data, tbuf, len2)) {
				free_page((unsigned long)tbuf);
				return -EFAULT;
			}
		}
	} else {
		if (len1) {
			if (copy_from_user(tbuf, bulk.data, len1)) {
				free_page((unsigned long)tbuf);
				return -EFAULT;
			}
		}
		i = usb_bulk_msg(dev, pipe, tbuf, len1, &len2, tmo);
	}
	free_page((unsigned long)tbuf);
	if (i < 0) {
		printk(KERN_WARNING "usbdevfs: USBDEVFS_BULK failed dev %d ep 0x%x len %u ret %d\n", 
		       dev->devnum, bulk.ep, bulk.len, i);
		return i;
	}
	return len2;
}

static int proc_resetep(struct dev_state *ps, void *arg)
{
	unsigned int ep;
	int ret;

	if (get_user(ep, (unsigned int *)arg))
		return -EFAULT;
	if ((ret = findintfep(ps->dev, ep)) < 0)
		return ret;
	if ((ret = checkintf(ps, ret)))
		return ret;
	usb_settoggle(ps->dev, ep & 0xf, !(ep & USB_DIR_IN), 0);
	return 0;
}

static int proc_clearhalt(struct dev_state *ps, void *arg)
{
	unsigned int ep;
	int pipe;
	int ret;

	if (get_user(ep, (unsigned int *)arg))
		return -EFAULT;
	if ((ret = findintfep(ps->dev, ep)) < 0)
		return ret;
	if ((ret = checkintf(ps, ret)))
		return ret;
	if (ep & USB_DIR_IN)
                pipe = usb_rcvbulkpipe(ps->dev, ep & 0x7f);
        else
                pipe = usb_sndbulkpipe(ps->dev, ep & 0x7f);

	return usb_clear_halt(ps->dev, pipe);
}
		

static int proc_getdriver(struct dev_state *ps, void *arg)
{
	struct usbdevfs_getdriver gd;
	struct usb_interface *interface;
	int ret;

	if (copy_from_user(&gd, arg, sizeof(gd)))
		return -EFAULT;
	if ((ret = findintfif(ps->dev, gd.interface)) < 0)
		return ret;
	interface = usb_ifnum_to_if(ps->dev, gd.interface);
	if (!interface)
		return -EINVAL;
	if (!interface->driver)
		return -ENODATA;
	strcpy(gd.driver, interface->driver->name);
	if (copy_to_user(arg, &gd, sizeof(gd)))
		return -EFAULT;
	return 0;
}

static int proc_connectinfo(struct dev_state *ps, void *arg)
{
	struct usbdevfs_connectinfo ci;

	ci.devnum = ps->dev->devnum;
	ci.slow = ps->dev->speed == USB_SPEED_LOW;
	if (copy_to_user(arg, &ci, sizeof(ci)))
		return -EFAULT;
	return 0;
}

static int proc_resetdevice(struct dev_state *ps)
{
	int i, ret;

	ret = usb_reset_device(ps->dev);
	if (ret < 0)
		return ret;

	for (i = 0; i < ps->dev->actconfig->bNumInterfaces; i++) {
		struct usb_interface *intf = &ps->dev->actconfig->interface[i];

		/* Don't simulate interfaces we've claimed */
		if (test_bit(i, &ps->ifclaimed))
			continue;

		if (intf->driver) {
			const struct usb_device_id *id;
			down(&intf->driver->serialize);
			intf->driver->disconnect(ps->dev, intf->private_data);
			id = usb_match_id(ps->dev,intf,intf->driver->id_table);
			intf->driver->probe(ps->dev, i, id);
			up(&intf->driver->serialize);
		}
	}

	return 0;
}

static int proc_setintf(struct dev_state *ps, void *arg)
{
	struct usbdevfs_setinterface setintf;
	struct usb_interface *interface;
	int ret;

	if (copy_from_user(&setintf, arg, sizeof(setintf)))
		return -EFAULT;
	if ((ret = findintfif(ps->dev, setintf.interface)) < 0)
		return ret;
	interface = usb_ifnum_to_if(ps->dev, setintf.interface);
	if (!interface)
		return -EINVAL;
	if (interface->driver) {
		if ((ret = checkintf(ps, ret)))
			return ret;
	}
	if (usb_set_interface(ps->dev, setintf.interface, setintf.altsetting))
		return -EINVAL;
	return 0;
}

static int proc_setconfig(struct dev_state *ps, void *arg)
{
	unsigned int u;

	if (get_user(u, (unsigned int *)arg))
		return -EFAULT;
	if (usb_set_configuration(ps->dev, u) < 0)
		return -EINVAL;
	return 0;
}

static int proc_submiturb(struct dev_state *ps, void *arg)
{
	struct usbdevfs_urb uurb;
	struct usbdevfs_iso_packet_desc *isopkt = NULL;
	struct usb_endpoint_descriptor *ep_desc;
	struct async *as;
	struct usb_ctrlrequest *dr = NULL;
	unsigned int u, totlen, isofrmlen;
	int ret;
	int intf = -1;

	if (copy_from_user(&uurb, arg, sizeof(uurb)))
		return -EFAULT;
	if (uurb.flags & ~(USBDEVFS_URB_ISO_ASAP|USBDEVFS_URB_DISABLE_SPD|USBDEVFS_URB_QUEUE_BULK|
			   USB_NO_FSBR|USB_ZERO_PACKET))
		return -EINVAL;
	if (!uurb.buffer)
		return -EINVAL;
	if (uurb.signr != 0 && (uurb.signr < SIGRTMIN || uurb.signr > SIGRTMAX))
		return -EINVAL;
	if (!(uurb.type == USBDEVFS_URB_TYPE_CONTROL && (uurb.endpoint & ~USB_ENDPOINT_DIR_MASK) == 0)) {
		if ((intf = findintfep(ps->dev, uurb.endpoint)) < 0)
			return intf;
		if ((ret = checkintf(ps, intf)))
			return ret;
	}
	switch(uurb.type) {
	case USBDEVFS_URB_TYPE_CONTROL:
		if ((uurb.endpoint & ~USB_ENDPOINT_DIR_MASK) != 0) {
			if (!(ep_desc = usb_epnum_to_ep_desc(ps->dev, uurb.endpoint)))
				return -ENOENT;
			if ((ep_desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_CONTROL)
				return -EINVAL;
		}
		/* min 8 byte setup packet, max arbitrary */
		if (uurb.buffer_length < 8 || uurb.buffer_length > PAGE_SIZE)
			return -EINVAL;
		if (!(dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_KERNEL)))
			return -ENOMEM;
		if (copy_from_user(dr, (unsigned char*)uurb.buffer, 8)) {
			kfree(dr);
			return -EFAULT;
		}
		if (uurb.buffer_length < (le16_to_cpup(&dr->wLength) + 8)) {
			kfree(dr);
			return -EINVAL;
		}
		if ((ret = check_ctrlrecip(ps, dr->bRequestType, le16_to_cpup(&dr->wIndex)))) {
			kfree(dr);
			return ret;
		}
		uurb.endpoint = (uurb.endpoint & ~USB_ENDPOINT_DIR_MASK) | (dr->bRequestType & USB_ENDPOINT_DIR_MASK);
		uurb.number_of_packets = 0;
		uurb.buffer_length = le16_to_cpup(&dr->wLength);
		uurb.buffer += 8;
		if (!access_ok((uurb.endpoint & USB_DIR_IN) ?  VERIFY_WRITE : VERIFY_READ, uurb.buffer, uurb.buffer_length)) {
			kfree(dr);
			return -EFAULT;
		}
		break;

	case USBDEVFS_URB_TYPE_BULK:
		uurb.number_of_packets = 0;
		if (uurb.buffer_length > 16384)
			return -EINVAL;
		if (!access_ok((uurb.endpoint & USB_DIR_IN) ? VERIFY_WRITE : VERIFY_READ, uurb.buffer, uurb.buffer_length))
			return -EFAULT;
		break;

	case USBDEVFS_URB_TYPE_ISO:
		/* arbitrary limit */
		if (uurb.number_of_packets < 1 || uurb.number_of_packets > 128)
			return -EINVAL;
		isofrmlen = sizeof(struct usbdevfs_iso_packet_desc) * uurb.number_of_packets;
		if (!(isopkt = kmalloc(isofrmlen, GFP_KERNEL)))
			return -ENOMEM;
		if (copy_from_user(isopkt, &((struct usbdevfs_urb *)arg)->iso_frame_desc, isofrmlen)) {
			kfree(isopkt);
			return -EFAULT;
		}
		for (totlen = u = 0; u < uurb.number_of_packets; u++) {
			if (isopkt[u].length > 1023) {
				kfree(isopkt);
				return -EINVAL;
			}
			totlen += isopkt[u].length;
		}
		if (totlen > 32768) {
			kfree(isopkt);
			return -EINVAL;
		}
		uurb.buffer_length = totlen;
		break;

	case USBDEVFS_URB_TYPE_INTERRUPT:
		uurb.number_of_packets = 0;
		if (uurb.buffer_length > 16384)
			return -EINVAL;
		if (!access_ok((uurb.endpoint & USB_DIR_IN) ? VERIFY_WRITE : VERIFY_READ, uurb.buffer, uurb.buffer_length))
			return -EFAULT;   
		break;

	default:
		return -EINVAL;
	}
	if (!(as = alloc_async(uurb.number_of_packets))) {
		if (isopkt)
			kfree(isopkt);
		if (dr)
			kfree(dr);
		return -ENOMEM;
	}
	if (!(as->urb.transfer_buffer = kmalloc(uurb.buffer_length, GFP_KERNEL))) {
		if (isopkt)
			kfree(isopkt);
		if (dr)
			kfree(dr);
		free_async(as);
		return -ENOMEM;
	}
        as->urb.next = NULL;
        as->urb.dev = ps->dev;
        as->urb.pipe = (uurb.type << 30) | __create_pipe(ps->dev, uurb.endpoint & 0xf) | (uurb.endpoint & USB_DIR_IN);
        as->urb.transfer_flags = uurb.flags;
	as->urb.transfer_buffer_length = uurb.buffer_length;
	as->urb.setup_packet = (unsigned char*)dr;
	as->urb.start_frame = uurb.start_frame;
	as->urb.number_of_packets = uurb.number_of_packets;
        as->urb.context = as;
        as->urb.complete = async_completed;
	for (totlen = u = 0; u < uurb.number_of_packets; u++) {
		as->urb.iso_frame_desc[u].offset = totlen;
		as->urb.iso_frame_desc[u].length = isopkt[u].length;
		totlen += isopkt[u].length;
	}
	if (isopkt)
		kfree(isopkt);
	as->ps = ps;
        as->userurb = arg;
	if (uurb.endpoint & USB_DIR_IN)
		as->userbuffer = uurb.buffer;
	else
		as->userbuffer = NULL;
	as->signr = uurb.signr;
	as->intf = intf;
	as->task = current;
	if (!(uurb.endpoint & USB_DIR_IN)) {
		if (copy_from_user(as->urb.transfer_buffer, uurb.buffer, as->urb.transfer_buffer_length)) {
			free_async(as);
			return -EFAULT;
		}
	}
        async_newpending(as);
        if ((ret = usb_submit_urb(&as->urb))) {
		printk(KERN_DEBUG "usbdevfs: usb_submit_urb returned %d\n", ret);
                async_removepending(as);
                free_async(as);
                return ret;
        }
        return 0;
}

static int proc_unlinkurb(struct dev_state *ps, void *arg)
{
	struct async *as;

	as = async_getpending(ps, arg);
	if (!as)
		return -EINVAL;
	usb_unlink_urb(&as->urb);
	return 0;
}

static int processcompl(struct async *as)
{
	unsigned int i;

	if (as->userbuffer)
		if (copy_to_user(as->userbuffer, as->urb.transfer_buffer, as->urb.transfer_buffer_length))
			return -EFAULT;
	if (put_user(as->urb.status,
		     &((struct usbdevfs_urb *)as->userurb)->status))
		return -EFAULT;
	if (put_user(as->urb.actual_length,
		     &((struct usbdevfs_urb *)as->userurb)->actual_length))
		return -EFAULT;
	if (put_user(as->urb.error_count,
		     &((struct usbdevfs_urb *)as->userurb)->error_count))
		return -EFAULT;

	if (!(usb_pipeisoc(as->urb.pipe)))
		return 0;
	for (i = 0; i < as->urb.number_of_packets; i++) {
		if (put_user(as->urb.iso_frame_desc[i].actual_length, 
			     &((struct usbdevfs_urb *)as->userurb)->iso_frame_desc[i].actual_length))
			return -EFAULT;
		if (put_user(as->urb.iso_frame_desc[i].status, 
			     &((struct usbdevfs_urb *)as->userurb)->iso_frame_desc[i].status))
			return -EFAULT;
	}
	return 0;
}

static int proc_reapurb(struct dev_state *ps, void *arg)
{
        DECLARE_WAITQUEUE(wait, current);
	struct async *as = NULL;
	void *addr;
	int ret;

	add_wait_queue(&ps->wait, &wait);
	while (ps->dev) {
		__set_current_state(TASK_INTERRUPTIBLE);
		if ((as = async_getcompleted(ps)))
			break;
		if (signal_pending(current))
			break;
		up_read(&ps->devsem);
		schedule();
		down_read(&ps->devsem);
	}
	remove_wait_queue(&ps->wait, &wait);
	set_current_state(TASK_RUNNING);
	if (as) {
		ret = processcompl(as);
		addr = as->userurb;
		free_async(as);
		if (ret)
			return ret;
		if (put_user(addr, (void **)arg))
			return -EFAULT;
		return 0;
	}
	if (signal_pending(current))
		return -EINTR;
	return -EIO;
}

static int proc_reapurbnonblock(struct dev_state *ps, void *arg)
{
	struct async *as;
	void *addr;
	int ret;

	if (!(as = async_getcompleted(ps)))
		return -EAGAIN;
	ret = processcompl(as);
	addr = as->userurb;
	free_async(as);
	if (ret)
		return ret;
	if (put_user(addr, (void **)arg))
		return -EFAULT;
	return 0;
}

static int proc_disconnectsignal(struct dev_state *ps, void *arg)
{
	struct usbdevfs_disconnectsignal ds;

	if (copy_from_user(&ds, arg, sizeof(ds)))
		return -EFAULT;
	if (ds.signr != 0 && (ds.signr < SIGRTMIN || ds.signr > SIGRTMAX))
		return -EINVAL;
	ps->discsignr = ds.signr;
	ps->disccontext = ds.context;
	return 0;
}

static int proc_claiminterface(struct dev_state *ps, void *arg)
{
	unsigned int intf;
	int ret;

	if (get_user(intf, (unsigned int *)arg))
		return -EFAULT;
	if ((ret = findintfif(ps->dev, intf)) < 0)
		return ret;
	return claimintf(ps, ret);
}

static int proc_releaseinterface(struct dev_state *ps, void *arg)
{
	unsigned int intf;
	int ret;

	if (get_user(intf, (unsigned int *)arg))
		return -EFAULT;
	if ((ret = findintfif(ps->dev, intf)) < 0)
		return ret;
	if ((ret = releaseintf(ps, intf)) < 0)
		return ret;
	destroy_async_on_interface (ps, intf);
	return 0;
}

static int proc_ioctl (struct dev_state *ps, void *arg)
{
	struct usbdevfs_ioctl	ctrl;
	int			size;
	void			*buf = 0;
	int			retval = 0;
       struct usb_interface    *ifp = 0;
       struct usb_driver       *driver = 0;

	/* get input parameters and alloc buffer */
	if (copy_from_user(&ctrl, (void *) arg, sizeof (ctrl)))
		return -EFAULT;
	if ((size = _IOC_SIZE (ctrl.ioctl_code)) > 0) {
		if ((buf = kmalloc (size, GFP_KERNEL)) == 0)
			return -ENOMEM;
		if ((_IOC_DIR(ctrl.ioctl_code) & _IOC_WRITE)) {
			if (copy_from_user (buf, ctrl.data, size)) {
				kfree (buf);
				return -EFAULT;
			}
		} else {
			memset (buf, 0, size);
		}
	}

       if (!ps->dev)
               retval = -ENODEV;
       else if (!(ifp = usb_ifnum_to_if (ps->dev, ctrl.ifno)))
               retval = -EINVAL;
       else switch (ctrl.ioctl_code) {

       /* disconnect kernel driver from interface, leaving it unbound.  */
       case USBDEVFS_DISCONNECT:
               driver = ifp->driver;
               if (driver) {
                       down (&driver->serialize);
                       dbg ("disconnect '%s' from dev %d interface %d",
                               driver->name, ps->dev->devnum, ctrl.ifno);
                       driver->disconnect (ps->dev, ifp->private_data);
                       usb_driver_release_interface (driver, ifp);
                       up (&driver->serialize);
               } else
                       retval = -ENODATA;
               break;

       /* let kernel drivers try to (re)bind to the interface */
       case USBDEVFS_CONNECT:
               usb_find_interface_driver_for_ifnum (ps->dev, ctrl.ifno);
               break;

       /* talk directly to the interface's driver */
       default:
               driver = ifp->driver;
               if (driver == 0 || driver->ioctl == 0)
                       retval = -ENOSYS;
		else {
			/* ifno might usefully be passed ... */
                       retval = driver->ioctl (ps->dev, ctrl.ioctl_code, buf);
			/* size = min_t(int, size, retval)? */
               }
	}

	/* cleanup and return */
	if (retval >= 0
			&& (_IOC_DIR (ctrl.ioctl_code) & _IOC_READ) != 0
			&& size > 0
			&& copy_to_user (ctrl.data, buf, size) != 0)
		retval = -EFAULT;
	if (buf != 0)
		kfree (buf);
	return retval;
}

static int usbdev_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct dev_state *ps = (struct dev_state *)file->private_data;
	int ret = -ENOIOCTLCMD;

	if (!(file->f_mode & FMODE_WRITE))
		return -EPERM;
	down_read(&ps->devsem);
	if (!ps->dev) {
		up_read(&ps->devsem);
		return -ENODEV;
	}
	switch (cmd) {
	case USBDEVFS_CONTROL:
		ret = proc_control(ps, (void *)arg);
		if (ret >= 0)
			inode->i_mtime = CURRENT_TIME;
		break;

	case USBDEVFS_BULK:
		ret = proc_bulk(ps, (void *)arg);
		if (ret >= 0)
			inode->i_mtime = CURRENT_TIME;
		break;

	case USBDEVFS_RESETEP:
		ret = proc_resetep(ps, (void *)arg);
		if (ret >= 0)
			inode->i_mtime = CURRENT_TIME;
		break;

	case USBDEVFS_RESET:
		ret = proc_resetdevice(ps);
		break;
	
	case USBDEVFS_CLEAR_HALT:
		ret = proc_clearhalt(ps, (void *)arg);
		if (ret >= 0)
			inode->i_mtime = CURRENT_TIME;
		break;

	case USBDEVFS_GETDRIVER:
		ret = proc_getdriver(ps, (void *)arg);
		break;

	case USBDEVFS_CONNECTINFO:
		ret = proc_connectinfo(ps, (void *)arg);
		break;

	case USBDEVFS_SETINTERFACE:
		ret = proc_setintf(ps, (void *)arg);
		break;

	case USBDEVFS_SETCONFIGURATION:
		ret = proc_setconfig(ps, (void *)arg);
		break;

	case USBDEVFS_SUBMITURB:
		ret = proc_submiturb(ps, (void *)arg);
		if (ret >= 0)
			inode->i_mtime = CURRENT_TIME;
		break;

	case USBDEVFS_DISCARDURB:
		ret = proc_unlinkurb(ps, (void *)arg);
		break;

	case USBDEVFS_REAPURB:
		ret = proc_reapurb(ps, (void *)arg);
		break;

	case USBDEVFS_REAPURBNDELAY:
		ret = proc_reapurbnonblock(ps, (void *)arg);
		break;

	case USBDEVFS_DISCSIGNAL:
		ret = proc_disconnectsignal(ps, (void *)arg);
		break;

	case USBDEVFS_CLAIMINTERFACE:
		ret = proc_claiminterface(ps, (void *)arg);
		break;

	case USBDEVFS_RELEASEINTERFACE:
		ret = proc_releaseinterface(ps, (void *)arg);
		break;

	case USBDEVFS_IOCTL:
		ret = proc_ioctl(ps, (void *) arg);
		break;
	}
	up_read(&ps->devsem);
	if (ret >= 0)
		inode->i_atime = CURRENT_TIME;
	return ret;
}

/* No kernel lock - fine */
static unsigned int usbdev_poll(struct file *file, struct poll_table_struct *wait)
{
	struct dev_state *ps = (struct dev_state *)file->private_data;
        unsigned int mask = 0;

	poll_wait(file, &ps->wait, wait);
	if (file->f_mode & FMODE_WRITE && !list_empty(&ps->async_completed))
		mask |= POLLOUT | POLLWRNORM;
	if (!ps->dev)
		mask |= POLLERR | POLLHUP;
	return mask;
}

struct file_operations usbdevfs_device_file_operations = {
	llseek:		usbdev_lseek,
	read:		usbdev_read,
	poll:		usbdev_poll,
	ioctl:		usbdev_ioctl,
	open:		usbdev_open,
	release:	usbdev_release,
};
