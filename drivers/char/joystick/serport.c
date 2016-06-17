/*
 * $Id: serport.c,v 1.7 2001/05/25 19:00:27 jdeneux Exp $
 *
 *  Copyright (c) 1999-2001 Vojtech Pavlik
 *
 *  Sponsored by SuSE
 */

/*
 * This is a module that converts a tty line into a much simpler
 * 'serial io port' abstraction that the input device drivers use.
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
 *  Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@ucw.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */

#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/tty.h>

struct serport {
	struct tty_struct *tty;
	wait_queue_head_t wait;
	struct serio serio;
};

/*
 * Callback functions from the serio code.
 */

static int serport_serio_write(struct serio *serio, unsigned char data)
{
	struct serport *serport = serio->driver;
	return -(serport->tty->driver.write(serport->tty, 0, &data, 1) != 1);
}

static int serport_serio_open(struct serio *serio)
{
        return 0;
}

static void serport_serio_close(struct serio *serio)
{
	struct serport *serport = serio->driver;
	wake_up_interruptible(&serport->wait);
}

/*
 * serport_ldisc_open() is the routine that is called upon setting our line
 * discipline on a tty. It looks for the Mag, and if found, registers
 * it as a joystick device.
 */

static int serport_ldisc_open(struct tty_struct *tty)
{
	struct serport *serport;

	MOD_INC_USE_COUNT;

	if (!(serport = kmalloc(sizeof(struct serport), GFP_KERNEL))) {
		MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}

	memset(serport, 0, sizeof(struct serport));

	serport->tty = tty;
	tty->disc_data = serport;

	serport->serio.type = SERIO_RS232;
	serport->serio.write = serport_serio_write;
	serport->serio.open = serport_serio_open;
	serport->serio.close = serport_serio_close;
	serport->serio.driver = serport;

	init_waitqueue_head(&serport->wait);

	return 0;
}

/*
 * serport_ldisc_close() is the opposite of serport_ldisc_open()
 */

static void serport_ldisc_close(struct tty_struct *tty)
{
	struct serport *serport = (struct serport*) tty->disc_data;
	kfree(serport);
	MOD_DEC_USE_COUNT;
}

/*
 * serport_ldisc_receive() is called by the low level tty driver when characters
 * are ready for us. We forward the characters, one by one to the 'interrupt'
 * routine.
 */

static void serport_ldisc_receive(struct tty_struct *tty, const unsigned char *cp, char *fp, int count)
{
	struct serport *serport = (struct serport*) tty->disc_data;
	int i;
	for (i = 0; i < count; i++)
		if (serport->serio.dev)
			serport->serio.dev->interrupt(&serport->serio, cp[i], 0);
}

/*
 * serport_ldisc_room() reports how much room we do have for receiving data.
 * Although we in fact have infinite room, we need to specify some value
 * here, and 256 seems to be reasonable.
 */

static int serport_ldisc_room(struct tty_struct *tty)
{
	return 256;
}

/*
 * serport_ldisc_read() just waits indefinitely if everything goes well. 
 * However, when the serio driver closes the serio port, it finishes,
 * returning 0 characters.
 */

static ssize_t serport_ldisc_read(struct tty_struct * tty, struct file * file, unsigned char * buf, size_t nr)
{
	struct serport *serport = (struct serport*) tty->disc_data;
	DECLARE_WAITQUEUE(wait, current);
	char name[32];

#ifdef CONFIG_DEVFS_FS
	sprintf(name, tty->driver.name, MINOR(tty->device) - tty->driver.minor_start);
#else
	sprintf(name, "%s%d", tty->driver.name, MINOR(tty->device) - tty->driver.minor_start);
#endif

	serio_register_port(&serport->serio);

	printk(KERN_INFO "serio%d: Serial port %s\n", serport->serio.number, name);

	add_wait_queue(&serport->wait, &wait);
	current->state = TASK_INTERRUPTIBLE;

	while(serport->serio.type && !signal_pending(current)) schedule();

	current->state = TASK_RUNNING;
	remove_wait_queue(&serport->wait, &wait);

	serio_unregister_port(&serport->serio);

	return 0;
}

/*
 * serport_ldisc_ioctl() allows to set the port protocol, and device ID
 */

static int serport_ldisc_ioctl(struct tty_struct * tty, struct file * file, unsigned int cmd, unsigned long arg)
{
	struct serport *serport = (struct serport*) tty->disc_data;
	
	switch (cmd) {
		case SPIOCSTYPE:
			return get_user(serport->serio.type, (unsigned long *) arg);
	}

	return -EINVAL;
}

/*
 * The line discipline structure.
 */

static struct tty_ldisc serport_ldisc = {
	name:		"input",
	open:		serport_ldisc_open,
	close:		serport_ldisc_close,
	read:		serport_ldisc_read,
	ioctl:		serport_ldisc_ioctl,
	receive_buf:	serport_ldisc_receive,
	receive_room:	serport_ldisc_room,
};

/*
 * The functions for insering/removing us as a module.
 */

int __init serport_init(void)
{
        if (tty_register_ldisc(N_MOUSE, &serport_ldisc)) {
                printk(KERN_ERR "serport.c: Error registering line discipline.\n");
		return -ENODEV;
	}

	return  0;
}

void __exit serport_exit(void)
{
	tty_register_ldisc(N_MOUSE, NULL);
}

module_init(serport_init);
module_exit(serport_exit);

MODULE_LICENSE("GPL");
