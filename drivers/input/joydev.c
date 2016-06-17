/*
 * $Id: joydev.c,v 1.19 2001/01/10 19:49:40 vojtech Exp $
 *
 *  Copyright (c) 1999-2000 Vojtech Pavlik 
 *  Copyright (c) 1999 Colin Van Dyke 
 *
 *  Joystick device driver for the input driver suite.
 *
 *  Sponsored by SuSE and Intel
 */

/*
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
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@suse.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */

#include <asm/io.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/joystick.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/smp_lock.h>

#define JOYDEV_MINOR_BASE	0
#define JOYDEV_MINORS		32
#define JOYDEV_BUFFER_SIZE	64

#define MSECS(t)	(1000 * ((t) / HZ) + 1000 * ((t) % HZ) / HZ)

struct joydev {
	int exist;
	int open;
	int minor;
	struct input_handle handle;
	wait_queue_head_t wait;
	devfs_handle_t devfs;
	struct joydev *next;
	struct joydev_list *list;
	struct js_corr corr[ABS_MAX];
	struct JS_DATA_SAVE_TYPE glue;
	int nabs;
	int nkey;
	__u16 keymap[KEY_MAX - BTN_MISC];
	__u16 keypam[KEY_MAX - BTN_MISC];
	__u8 absmap[ABS_MAX];
	__u8 abspam[ABS_MAX];
	__s16 abs[ABS_MAX];
};

struct joydev_list {
	struct js_event buffer[JOYDEV_BUFFER_SIZE];
	int head;
	int tail;
	int startup;
	struct fasync_struct *fasync;
	struct joydev *joydev;
	struct joydev_list *next;
};

static struct joydev *joydev_table[JOYDEV_MINORS];

MODULE_AUTHOR("Vojtech Pavlik <vojtech@suse.cz>");
MODULE_DESCRIPTION("Joystick device driver");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("input/js");

static int joydev_correct(int value, struct js_corr *corr)
{
	switch (corr->type) {
		case JS_CORR_NONE:
			break;
		case JS_CORR_BROKEN:
			value = value > corr->coef[0] ? (value < corr->coef[1] ? 0 :
				((corr->coef[3] * (value - corr->coef[1])) >> 14)) :
				((corr->coef[2] * (value - corr->coef[0])) >> 14);
			break;
		default:
			return 0;
	}

	if (value < -32767) return -32767;
	if (value >  32767) return  32767;

	return value;
}

static void joydev_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
	struct joydev *joydev = handle->private;
	struct joydev_list *list = joydev->list;
	struct js_event event;

	switch (type) {

		case EV_KEY:
			if (code < BTN_MISC || value == 2) return;
			event.type = JS_EVENT_BUTTON;
			event.number = joydev->keymap[code - BTN_MISC];
			event.value = value;
			break;

		case EV_ABS:
			event.type = JS_EVENT_AXIS;
			event.number = joydev->absmap[code];
			event.value = joydev_correct(value, joydev->corr + event.number);
			if (event.value == joydev->abs[event.number]) return;
			joydev->abs[event.number] = event.value;
			break;

		default:
			return;
	}  

	event.time = MSECS(jiffies);

	while (list) {

		memcpy(list->buffer + list->head, &event, sizeof(struct js_event));

		if (list->startup == joydev->nabs + joydev->nkey)
			if (list->tail == (list->head = (list->head + 1) & (JOYDEV_BUFFER_SIZE - 1)))
				list->startup = 0;

		kill_fasync(&list->fasync, SIGIO, POLL_IN);

		list = list->next;
	}

	wake_up_interruptible(&joydev->wait);
}

static int joydev_fasync(int fd, struct file *file, int on)
{
	int retval;
	struct joydev_list *list = file->private_data;
	retval = fasync_helper(fd, file, on, &list->fasync);
	return retval < 0 ? retval : 0;
}

static int joydev_release(struct inode * inode, struct file * file)
{
	struct joydev_list *list = file->private_data;
	struct joydev_list **listptr;

	lock_kernel();
	listptr = &list->joydev->list;
	joydev_fasync(-1, file, 0);

	while (*listptr && (*listptr != list))
		listptr = &((*listptr)->next);
	*listptr = (*listptr)->next;

	if (!--list->joydev->open) {
		if (list->joydev->exist) {
			input_close_device(&list->joydev->handle);
		} else {
			input_unregister_minor(list->joydev->devfs);
			joydev_table[list->joydev->minor] = NULL;
			kfree(list->joydev);
		}
	}

	kfree(list);
	unlock_kernel();

	return 0;
}

static int joydev_open(struct inode *inode, struct file *file)
{
	struct joydev_list *list;
	int i = MINOR(inode->i_rdev) - JOYDEV_MINOR_BASE;

	if (i >= JOYDEV_MINORS || !joydev_table[i])
		return -ENODEV;

	if (!(list = kmalloc(sizeof(struct joydev_list), GFP_KERNEL)))
		return -ENOMEM;
	memset(list, 0, sizeof(struct joydev_list));

	list->joydev = joydev_table[i];
	list->next = joydev_table[i]->list;
	joydev_table[i]->list = list;	

	file->private_data = list;

	if (!list->joydev->open++)
		if (list->joydev->exist)
			input_open_device(&list->joydev->handle);

	return 0;
}

static ssize_t joydev_write(struct file * file, const char * buffer, size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static ssize_t joydev_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	struct joydev_list *list = file->private_data;
	struct joydev *joydev = list->joydev;
	struct input_dev *input = joydev->handle.dev;
	int retval = 0;

	if (count < sizeof(struct js_event))
		return -EINVAL;
	
	if (!joydev->exist)
		return -ENODEV;

	if (count == sizeof(struct JS_DATA_TYPE)) {

		struct JS_DATA_TYPE data;
		int i;

		for (data.buttons = i = 0; i < 32 && i < joydev->nkey; i++)
			data.buttons |= test_bit(joydev->keypam[i], input->key) ? (1 << i) : 0;
		data.x = (joydev->abs[0] / 256 + 128) >> joydev->glue.JS_CORR.x;
		data.y = (joydev->abs[1] / 256 + 128) >> joydev->glue.JS_CORR.y;

		if (copy_to_user(buf, &data, sizeof(struct JS_DATA_TYPE)))
			return -EFAULT;

		list->startup = 0;
		list->tail = list->head;

		return sizeof(struct JS_DATA_TYPE);
	}

	if (list->head == list->tail && list->startup == joydev->nabs + joydev->nkey) {

		add_wait_queue(&list->joydev->wait, &wait);
		current->state = TASK_INTERRUPTIBLE;

		while (list->head == list->tail) {

			if (!joydev->exist) {
                                retval = -ENODEV;
                                break;
                        }
			if (file->f_flags & O_NONBLOCK) {
				retval = -EAGAIN;
				break;
			}
			if (signal_pending(current)) {
				retval = -ERESTARTSYS;
				break;
			}

			schedule();
		}

		current->state = TASK_RUNNING;
		remove_wait_queue(&list->joydev->wait, &wait);
	}

	if (retval)
		return retval;

	while (list->startup < joydev->nabs + joydev->nkey && retval + sizeof(struct js_event) <= count) {

		struct js_event event;

		event.time = MSECS(jiffies);

		if (list->startup < joydev->nkey) {
			event.type = JS_EVENT_BUTTON | JS_EVENT_INIT;
			event.number = list->startup;
			event.value = !!test_bit(joydev->keypam[event.number], input->key);
		} else {
			event.type = JS_EVENT_AXIS | JS_EVENT_INIT;
			event.number = list->startup - joydev->nkey;
			event.value = joydev->abs[event.number];
		}

		if (copy_to_user(buf + retval, &event, sizeof(struct js_event)))
			return -EFAULT;

		list->startup++;
		retval += sizeof(struct js_event);
	}

	while (list->head != list->tail && retval + sizeof(struct js_event) <= count) {

		if (copy_to_user(buf + retval, list->buffer + list->tail, sizeof(struct js_event)))
			return -EFAULT;

		list->tail = (list->tail + 1) & (JOYDEV_BUFFER_SIZE - 1);
		retval += sizeof(struct js_event);
	}

	return retval;
}

/* No kernel lock - fine */
static unsigned int joydev_poll(struct file *file, poll_table *wait)
{
	struct joydev_list *list = file->private_data;
	poll_wait(file, &list->joydev->wait, wait);
	if (list->head != list->tail || list->startup < list->joydev->nabs + list->joydev->nkey)
		return POLLIN | POLLRDNORM;
	return 0;
}

static int joydev_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct joydev_list *list = file->private_data;
	struct joydev *joydev = list->joydev;
	struct input_dev *dev = joydev->handle.dev;
	int i;

	if (!joydev->exist)
		return -ENODEV;

	switch (cmd) {

		case JS_SET_CAL:
			return copy_from_user(&joydev->glue.JS_CORR, (struct JS_DATA_TYPE *) arg,
				sizeof(struct JS_DATA_TYPE)) ? -EFAULT : 0;
		case JS_GET_CAL:
			return copy_to_user((struct JS_DATA_TYPE *) arg, &joydev->glue.JS_CORR,
				sizeof(struct JS_DATA_TYPE)) ? -EFAULT : 0;
		case JS_SET_TIMEOUT:
			return get_user(joydev->glue.JS_TIMEOUT, (int *) arg);
		case JS_GET_TIMEOUT:
			return put_user(joydev->glue.JS_TIMEOUT, (int *) arg);
		case JS_SET_TIMELIMIT:
			return get_user(joydev->glue.JS_TIMELIMIT, (long *) arg);
		case JS_GET_TIMELIMIT:
			return put_user(joydev->glue.JS_TIMELIMIT, (long *) arg);
		case JS_SET_ALL:
			return copy_from_user(&joydev->glue, (struct JS_DATA_SAVE_TYPE *) arg,
						sizeof(struct JS_DATA_SAVE_TYPE)) ? -EFAULT : 0;
		case JS_GET_ALL:
			return copy_to_user((struct JS_DATA_SAVE_TYPE *) arg, &joydev->glue,
						sizeof(struct JS_DATA_SAVE_TYPE)) ? -EFAULT : 0;

		case JSIOCGVERSION:
			return put_user(JS_VERSION, (__u32 *) arg);
		case JSIOCGAXES:
			return put_user(joydev->nabs, (__u8 *) arg);
		case JSIOCGBUTTONS:
			return put_user(joydev->nkey, (__u8 *) arg);
		case JSIOCSCORR:
			return copy_from_user(joydev->corr, (struct js_corr *) arg,
						sizeof(struct js_corr) * joydev->nabs) ? -EFAULT : 0;
		case JSIOCGCORR:
			return copy_to_user((struct js_corr *) arg, joydev->corr,
						sizeof(struct js_corr) * joydev->nabs) ? -EFAULT : 0;
		case JSIOCSAXMAP:
			if (copy_from_user(joydev->abspam, (__u8 *) arg, sizeof(__u8) * ABS_MAX))
				return -EFAULT;
			for (i = 0; i < joydev->nabs; i++) {
				if (joydev->abspam[i] > ABS_MAX) return -EINVAL;
				joydev->absmap[joydev->abspam[i]] = i;
			}
			return 0;
		case JSIOCGAXMAP:
			return copy_to_user((__u8 *) arg, joydev->abspam,
						sizeof(__u8) * ABS_MAX) ? -EFAULT : 0;
		case JSIOCSBTNMAP:
			if (copy_from_user(joydev->keypam, (__u16 *) arg, sizeof(__u16) * (KEY_MAX - BTN_MISC)))
				return -EFAULT;
			for (i = 0; i < joydev->nkey; i++) {
				if (joydev->keypam[i] > KEY_MAX || joydev->keypam[i] < BTN_MISC) return -EINVAL;
				joydev->keymap[joydev->keypam[i] - BTN_MISC] = i;
			}
			return 0;
		case JSIOCGBTNMAP:
			return copy_to_user((__u16 *) arg, joydev->keypam,
						sizeof(__u16) * (KEY_MAX - BTN_MISC)) ? -EFAULT : 0;
		default:
			if ((cmd & ~(_IOC_SIZEMASK << _IOC_SIZESHIFT)) == JSIOCGNAME(0)) {
				int len;
				if (!dev->name) return 0;
				len = strlen(dev->name) + 1;
				if (len > _IOC_SIZE(cmd)) len = _IOC_SIZE(cmd);
				if (copy_to_user((char *) arg, dev->name, len)) return -EFAULT;
				return len;
			}
	}
	return -EINVAL;
}

static struct file_operations joydev_fops = {
	owner:		THIS_MODULE,
	read:		joydev_read,
	write:		joydev_write,
	poll:		joydev_poll,
	open:		joydev_open,
	release:	joydev_release,
	ioctl:		joydev_ioctl,
	fasync:		joydev_fasync,
};

static struct input_handle *joydev_connect(struct input_handler *handler, struct input_dev *dev)
{
	struct joydev *joydev;
	int i, j, t, minor;

	if (!(test_bit(EV_KEY, dev->evbit) && test_bit(EV_ABS, dev->evbit) &&
	     (test_bit(ABS_X, dev->absbit) || test_bit(ABS_Y, dev->absbit)) &&
	     (test_bit(BTN_TRIGGER, dev->keybit) || test_bit(BTN_A, dev->keybit)
		|| test_bit(BTN_1, dev->keybit)))) return NULL; 

	for (minor = 0; minor < JOYDEV_MINORS && joydev_table[minor]; minor++);
	if (minor == JOYDEV_MINORS) {
		printk(KERN_ERR "joydev: no more free joydev devices\n");
		return NULL;
	}

	if (!(joydev = kmalloc(sizeof(struct joydev), GFP_KERNEL)))
		return NULL;
	memset(joydev, 0, sizeof(struct joydev));

	init_waitqueue_head(&joydev->wait);

	joydev->minor = minor;
	joydev_table[minor] = joydev;

	joydev->handle.dev = dev;
	joydev->handle.handler = handler;
	joydev->handle.private = joydev;

	joydev->exist = 1;

	for (i = 0; i < ABS_MAX; i++)
		if (test_bit(i, dev->absbit)) {
			joydev->absmap[i] = joydev->nabs;
			joydev->abspam[joydev->nabs] = i;
			joydev->nabs++;
		}

	for (i = BTN_JOYSTICK - BTN_MISC; i < KEY_MAX - BTN_MISC; i++)
		if (test_bit(i + BTN_MISC, dev->keybit)) {
			joydev->keymap[i] = joydev->nkey;
			joydev->keypam[joydev->nkey] = i + BTN_MISC;
			joydev->nkey++;
		}

	for (i = 0; i < BTN_JOYSTICK - BTN_MISC; i++)
		if (test_bit(i + BTN_MISC, dev->keybit)) {
			joydev->keymap[i] = joydev->nkey;
			joydev->keypam[joydev->nkey] = i + BTN_MISC;
			joydev->nkey++;
		}

	for (i = 0; i < joydev->nabs; i++) {
		j = joydev->abspam[i];
		if (dev->absmax[j] == dev->absmin[j]) {
			joydev->corr[i].type = JS_CORR_NONE;
			continue;
		}
		joydev->corr[i].type = JS_CORR_BROKEN;
		joydev->corr[i].prec = dev->absfuzz[j];
		joydev->corr[i].coef[0] = (dev->absmax[j] + dev->absmin[j]) / 2 - dev->absflat[j];
		joydev->corr[i].coef[1] = (dev->absmax[j] + dev->absmin[j]) / 2 + dev->absflat[j];
		if (!(t = ((dev->absmax[j] - dev->absmin[j]) / 2 - 2 * dev->absflat[j])))
			continue;
		joydev->corr[i].coef[2] = (1 << 29) / t;
		joydev->corr[i].coef[3] = (1 << 29) / t;

		joydev->abs[i] = joydev_correct(dev->abs[j], joydev->corr + i);
	}

	joydev->devfs = input_register_minor("js%d", minor, JOYDEV_MINOR_BASE);

//	printk(KERN_INFO "js%d: Joystick device for input%d\n", minor, dev->number);

	return &joydev->handle;
}

static void joydev_disconnect(struct input_handle *handle)
{
	struct joydev *joydev = handle->private;

	joydev->exist = 0;

	if (joydev->open) {
		input_close_device(handle);	
	} else {
		input_unregister_minor(joydev->devfs);
		joydev_table[joydev->minor] = NULL;
		kfree(joydev);
	}
}

static struct input_handler joydev_handler = {
	event:		joydev_event,
	connect:	joydev_connect,
	disconnect:	joydev_disconnect,
	fops:		&joydev_fops,
	minor:		JOYDEV_MINOR_BASE,
};

static int __init joydev_init(void)
{
	input_register_handler(&joydev_handler);
	return 0;
}

static void __exit joydev_exit(void)
{
	input_unregister_handler(&joydev_handler);
}

module_init(joydev_init);
module_exit(joydev_exit);
