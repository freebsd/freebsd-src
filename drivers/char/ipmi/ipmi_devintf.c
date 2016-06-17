/*
 * ipmi_devintf.c
 *
 * Linux device interface for the IPMI message handler.
 *
 * Author: MontaVista Software, Inc.
 *         Corey Minyard <minyard@mvista.com>
 *         source@mvista.com
 *
 * Copyright 2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <asm/system.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/ipmi.h>
#include <asm/semaphore.h>
#include <linux/init.h>

struct ipmi_file_private
{
	ipmi_user_t          user;
	spinlock_t           recv_msg_lock;
	struct list_head     recv_msgs;
	struct file          *file;
	struct fasync_struct *fasync_queue;
	wait_queue_head_t    wait;
	struct semaphore     recv_sem;
};

static void file_receive_handler(struct ipmi_recv_msg *msg,
				 void                 *handler_data)
{
	struct ipmi_file_private *priv = handler_data;
	int                      was_empty;
	unsigned long            flags;

	spin_lock_irqsave(&(priv->recv_msg_lock), flags);

	was_empty = list_empty(&(priv->recv_msgs));
	list_add_tail(&(msg->link), &(priv->recv_msgs));

	if (was_empty) {
		wake_up_interruptible(&priv->wait);
		kill_fasync(&priv->fasync_queue, SIGIO, POLL_IN);
	}

	spin_unlock_irqrestore(&(priv->recv_msg_lock), flags);
}

static unsigned int ipmi_poll(struct file *file, poll_table *wait)
{
	struct ipmi_file_private *priv = file->private_data;
	unsigned int             mask = 0;
	unsigned long            flags;

	poll_wait(file, &priv->wait, wait);

	spin_lock_irqsave(&priv->recv_msg_lock, flags);

	if (! list_empty(&(priv->recv_msgs)))
		mask |= (POLLIN | POLLRDNORM);

	spin_unlock_irqrestore(&priv->recv_msg_lock, flags);

	return mask;
}

static int ipmi_fasync(int fd, struct file *file, int on)
{
	struct ipmi_file_private *priv = file->private_data;
	int                      result;

	result = fasync_helper(fd, file, on, &priv->fasync_queue);

	return (result);
}

static struct ipmi_user_hndl ipmi_hndlrs =
{
	ipmi_recv_hndl : file_receive_handler
};

static int ipmi_open(struct inode *inode, struct file *file)
{
	int                      if_num = minor(inode->i_rdev);
	int                      rv;
	struct ipmi_file_private *priv;


	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->file = file;

	rv = ipmi_create_user(if_num,
			      &ipmi_hndlrs,
			      priv,
			      &(priv->user));
	if (rv) {
		kfree(priv);
		return rv;
	}

	file->private_data = priv;

	spin_lock_init(&(priv->recv_msg_lock));
	INIT_LIST_HEAD(&(priv->recv_msgs));
	init_waitqueue_head(&priv->wait);
	priv->fasync_queue = NULL;
	sema_init(&(priv->recv_sem), 1);

	return 0;
}

static int ipmi_release(struct inode *inode, struct file *file)
{
	struct ipmi_file_private *priv = file->private_data;
	int                      rv;

	rv = ipmi_destroy_user(priv->user);
	if (rv)
		return rv;

	ipmi_fasync (-1, file, 0);

	/* FIXME - free the messages in the list. */
	kfree(priv);

	return 0;
}

static int ipmi_ioctl(struct inode  *inode,
		      struct file   *file,
		      unsigned int  cmd,
		      unsigned long data)
{
	int                      rv = -EINVAL;
	struct ipmi_file_private *priv = file->private_data;

	switch (cmd) 
	{
	case IPMICTL_SEND_COMMAND:
	{
		struct ipmi_req    req;
		struct ipmi_addr   addr;
		unsigned char msgdata[IPMI_MAX_MSG_LENGTH];

		if (copy_from_user(&req, (void *) data, sizeof(req))) {
			rv = -EFAULT;
			break;
		}

		if (req.addr_len > sizeof(struct ipmi_addr))
		{
			rv = -EINVAL;
			break;
		}

		if (copy_from_user(&addr, req.addr, req.addr_len)) {
			rv = -EFAULT;
			break;
		}

		rv = ipmi_validate_addr(&addr, req.addr_len);
		if (rv)
			break;

		if (req.msg.data != NULL) {
			if (req.msg.data_len > IPMI_MAX_MSG_LENGTH) {
				rv = -EMSGSIZE;
				break;
			}

			if (copy_from_user(&msgdata,
					   req.msg.data,
					   req.msg.data_len))
			{
				rv = -EFAULT;
				break;
			}
		} else {
			req.msg.data_len = 0;
		}

		req.msg.data = msgdata;

		rv = ipmi_request(priv->user,
				  &addr,
				  req.msgid,
				  &(req.msg),
				  0);
		break;
	}

	case IPMICTL_RECEIVE_MSG:
	case IPMICTL_RECEIVE_MSG_TRUNC:
	{
		struct ipmi_recv      rsp;
		int              addr_len;
		struct list_head *entry;
		struct ipmi_recv_msg  *msg;
		unsigned long    flags;
		

		rv = 0;
		if (copy_from_user(&rsp, (void *) data, sizeof(rsp))) {
			rv = -EFAULT;
			break;
		}

		/* We claim a semaphore because we don't want two
                   users getting something from the queue at a time.
                   Since we have to release the spinlock before we can
                   copy the data to the user, it's possible another
                   user will grab something from the queue, too.  Then
                   the messages might get out of order if something
                   fails and the message gets put back onto the
                   queue.  This semaphore prevents that problem. */
		down(&(priv->recv_sem));

		/* Grab the message off the list. */
		spin_lock_irqsave(&(priv->recv_msg_lock), flags);
		if (list_empty(&(priv->recv_msgs))) {
			spin_unlock_irqrestore(&(priv->recv_msg_lock), flags);
			rv = -EAGAIN;
			goto recv_err;
		}
		entry = priv->recv_msgs.next;
		msg = list_entry(entry, struct ipmi_recv_msg, link);
		list_del(entry);
		spin_unlock_irqrestore(&(priv->recv_msg_lock), flags);

		addr_len = ipmi_addr_length(msg->addr.addr_type);
		if (rsp.addr_len < addr_len)
		{
			rv = -EINVAL;
			goto recv_putback_on_err;
		}

		if (copy_to_user(rsp.addr, &(msg->addr), addr_len)) {
			rv = -EFAULT;
			goto recv_putback_on_err;
		}
		rsp.addr_len = addr_len;

		rsp.recv_type = msg->recv_type;
		rsp.msgid = msg->msgid;
		rsp.msg.netfn = msg->msg.netfn;
		rsp.msg.cmd = msg->msg.cmd;

		if (msg->msg.data_len > 0) {
			if (rsp.msg.data_len < msg->msg.data_len) {
				rv = -EMSGSIZE;
				if (cmd == IPMICTL_RECEIVE_MSG_TRUNC) {
					msg->msg.data_len = rsp.msg.data_len;
				} else {
					goto recv_putback_on_err;
				}
			}

			if (copy_to_user(rsp.msg.data,
					 msg->msg.data,
					 msg->msg.data_len))
			{
				rv = -EFAULT;
				goto recv_putback_on_err;
			}
			rsp.msg.data_len = msg->msg.data_len;
		} else {
			rsp.msg.data_len = 0;
		}

		if (copy_to_user((void *) data, &rsp, sizeof(rsp))) {
			rv = -EFAULT;
			goto recv_putback_on_err;
		}

		up(&(priv->recv_sem));
		ipmi_free_recv_msg(msg);
		break;

	recv_putback_on_err:
		/* If we got an error, put the message back onto
		   the head of the queue. */
		spin_lock_irqsave(&(priv->recv_msg_lock), flags);
		list_add(entry, &(priv->recv_msgs));
		spin_unlock_irqrestore(&(priv->recv_msg_lock), flags);
		up(&(priv->recv_sem));
		break;

	recv_err:
		up(&(priv->recv_sem));
		break;
	}

	case IPMICTL_REGISTER_FOR_CMD:
	{
		struct ipmi_cmdspec val;

		if (copy_from_user(&val, (void *) data, sizeof(val))) {
			rv = -EFAULT;
			break;
		}

		rv = ipmi_register_for_cmd(priv->user, val.netfn, val.cmd);
		break;
	}

	case IPMICTL_UNREGISTER_FOR_CMD:
	{
		struct ipmi_cmdspec   val;

		if (copy_from_user(&val, (void *) data, sizeof(val))) {
			rv = -EFAULT;
			break;
		}

		rv = ipmi_unregister_for_cmd(priv->user, val.netfn, val.cmd);
		break;
	}

	case IPMICTL_SET_GETS_EVENTS_CMD:
	{
		int val;

		if (copy_from_user(&val, (void *) data, sizeof(val))) {
			rv = -EFAULT;
			break;
		}

		rv = ipmi_set_gets_events(priv->user, val);
		break;
	}

	case IPMICTL_SET_MY_ADDRESS_CMD:
	{
		unsigned int val;

		if (copy_from_user(&val, (void *) data, sizeof(val))) {
			rv = -EFAULT;
			break;
		}

		ipmi_set_my_address(priv->user, val);
		rv = 0;
		break;
	}

	case IPMICTL_GET_MY_ADDRESS_CMD:
	{
		unsigned int val;

		val = ipmi_get_my_address(priv->user);

		if (copy_to_user((void *) data, &val, sizeof(val))) {
			rv = -EFAULT;
			break;
		}
		rv = 0;
		break;
	}

	case IPMICTL_SET_MY_LUN_CMD:
	{
		unsigned int val;

		if (copy_from_user(&val, (void *) data, sizeof(val))) {
			rv = -EFAULT;
			break;
		}

		ipmi_set_my_LUN(priv->user, val);
		rv = 0;
		break;
	}

	case IPMICTL_GET_MY_LUN_CMD:
	{
		unsigned int val;

		val = ipmi_get_my_LUN(priv->user);

		if (copy_to_user((void *) data, &val, sizeof(val))) {
			rv = -EFAULT;
			break;
		}
		rv = 0;
		break;
	}

	}
  
	return rv;
}


static struct file_operations ipmi_fops = {
	owner:   THIS_MODULE,
	ioctl:   ipmi_ioctl,
	open:    ipmi_open,
	release: ipmi_release,
	fasync:  ipmi_fasync,
	poll:    ipmi_poll
};

#define DEVICE_NAME     "ipmidev"

static int ipmi_major = 0;
MODULE_PARM(ipmi_major, "i");

static devfs_handle_t devfs_handle;

#define MAX_DEVICES 10
static devfs_handle_t handles[MAX_DEVICES];

static void ipmi_new_smi(int if_num)
{
	char name[2];

	if (if_num > MAX_DEVICES)
		return;

	name[0] = if_num + '0';
	name[1] = '\0';

	handles[if_num] = devfs_register(devfs_handle, name, DEVFS_FL_NONE,
					 ipmi_major, if_num,
					 S_IFCHR | S_IRUSR | S_IWUSR,
					 &ipmi_fops, NULL);
}

static void ipmi_smi_gone(int if_num)
{
	if (if_num > MAX_DEVICES)
		return;

	devfs_unregister(handles[if_num]);
}

static struct ipmi_smi_watcher smi_watcher =
{
	new_smi  : ipmi_new_smi,
	smi_gone : ipmi_smi_gone
};

static __init int init_ipmi_devintf(void)
{
	int rv;

	if (ipmi_major < 0)
		return -EINVAL;

	rv = register_chrdev(ipmi_major, DEVICE_NAME, &ipmi_fops);
	if (rv < 0) {
		printk(KERN_ERR "ipmi: can't get major %d\n", ipmi_major);
		return rv;
	}

	if (ipmi_major == 0) {
		ipmi_major = rv;
	}

	devfs_handle = devfs_mk_dir(NULL, DEVICE_NAME, NULL);

	rv = ipmi_smi_watcher_register(&smi_watcher);
	if (rv) {
		unregister_chrdev(ipmi_major, DEVICE_NAME);
		printk(KERN_WARNING "ipmi: can't register smi watcher");
		return rv;
	}

	printk(KERN_INFO "ipmi: device interface at char major %d\n",
	       ipmi_major);

	return 0;
}
module_init(init_ipmi_devintf);

static __exit void cleanup_ipmi(void)
{
	ipmi_smi_watcher_unregister(&smi_watcher);
	devfs_unregister(devfs_handle);
	unregister_chrdev(ipmi_major, DEVICE_NAME);
}
module_exit(cleanup_ipmi);
#ifndef MODULE
static __init int ipmi_setup (char *str)
{
	int x;

	if (get_option (&str, &x)) {
		/* ipmi=x sets the major number to x. */
		ipmi_major = x;
	} else if (!strcmp(str, "off")) {
		ipmi_major = -1;
	}

	return 1;
}
#endif

__setup("ipmi=", ipmi_setup);
MODULE_LICENSE("GPL");
