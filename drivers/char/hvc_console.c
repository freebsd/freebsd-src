/*
 * Copyright (C) 2001 Anton Blanchard <anton@au.ibm.com>, IBM
 * Copyright (C) 2001 Paul Mackerras <paulus@au.ibm.com>, IBM
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/major.h>
#include <linux/kernel.h>
#include <linux/sysrq.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/sched.h>
#include <linux/kbd_kern.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>

extern int hvc_count(int *);
extern int hvc_get_chars(int index, char *buf, int count);
extern int hvc_put_chars(int index, const char *buf, int count);

#define HVC_MAJOR	229
#define HVC_MINOR	0

#define MAX_NR_HVC_CONSOLES	4

#define TIMEOUT		((HZ + 99) / 100)

struct tty_driver hvc_driver;
static int hvc_refcount;
static struct tty_struct *hvc_table[MAX_NR_HVC_CONSOLES];
static struct termios *hvc_termios[MAX_NR_HVC_CONSOLES];
static struct termios *hvc_termios_locked[MAX_NR_HVC_CONSOLES];
static int hvc_offset;
#ifdef CONFIG_MAGIC_SYSRQ
static int sysrq_pressed;
#endif

#define N_OUTBUF	16

#define __ALIGNED__	__attribute__((__aligned__(8)))

struct hvc_struct {
	spinlock_t lock;
	int index;
	struct tty_struct *tty;
	unsigned int count;
	int do_wakeup;
	char outbuf[N_OUTBUF] __ALIGNED__;
	int n_outbuf;
};

struct hvc_struct hvc_struct[MAX_NR_HVC_CONSOLES];

static int hvc_open(struct tty_struct *tty, struct file * filp)
{
	int line = MINOR(tty->device) - tty->driver.minor_start;
	struct hvc_struct *hp;
	unsigned long flags;

	if (line < 0 || line >= MAX_NR_HVC_CONSOLES)
		return -ENODEV;
	hp = &hvc_struct[line];

	tty->driver_data = hp;
	spin_lock_irqsave(&hp->lock, flags);
	hp->tty = tty;
	hp->count++;
	spin_unlock_irqrestore(&hp->lock, flags);

	return 0;
}

static void hvc_close(struct tty_struct *tty, struct file * filp)
{
	struct hvc_struct *hp = tty->driver_data;
	unsigned long flags;

	if (tty_hung_up_p(filp))
		return;
	spin_lock_irqsave(&hp->lock, flags);
	if (--hp->count == 0)
		hp->tty = NULL;
	else if (hp->count < 0)
		printk(KERN_ERR "hvc_close %lu: oops, count is %d\n",
		       hp - hvc_struct, hp->count);
	spin_unlock_irqrestore(&hp->lock, flags);
}

static void hvc_hangup(struct tty_struct *tty)
{
	struct hvc_struct *hp = tty->driver_data;

	hp->count = 0;
	hp->tty = NULL;
}

/* called with hp->lock held */
static void hvc_push(struct hvc_struct *hp)
{
	int n;

	n = hvc_put_chars(hp->index + hvc_offset, hp->outbuf, hp->n_outbuf);
	if (n <= 0) {
		if (n == 0)
			return;
		/* throw away output on error; this happens when
		   there is no session connected to the vterm. */
		hp->n_outbuf = 0;
	} else
		hp->n_outbuf -= n;
	if (hp->n_outbuf > 0)
		memmove(hp->outbuf, hp->outbuf + n, hp->n_outbuf);
	else
		hp->do_wakeup = 1;
}

static int hvc_write(struct tty_struct *tty, int from_user,
		     const unsigned char *buf, int count)
{
	struct hvc_struct *hp = tty->driver_data;
	char *p;
	int todo, written = 0;
	unsigned long flags;

	spin_lock_irqsave(&hp->lock, flags);
	while (count > 0 && (todo = N_OUTBUF - hp->n_outbuf) > 0) {
		if (todo > count)
			todo = count;
		p = hp->outbuf + hp->n_outbuf;
		if (from_user) {
			todo -= copy_from_user(p, buf, todo);
			if (todo == 0) {
				if (written == 0)
					written = -EFAULT;
				break;
			}
		} else
			memcpy(p, buf, todo);
		count -= todo;
		buf += todo;
		hp->n_outbuf += todo;
		written += todo;
		hvc_push(hp);
	}
	spin_unlock_irqrestore(&hp->lock, flags);

	return written;
}

static int hvc_write_room(struct tty_struct *tty)
{
	struct hvc_struct *hp = tty->driver_data;

	return N_OUTBUF - hp->n_outbuf;
}

static int hvc_chars_in_buffer(struct tty_struct *tty)
{
	struct hvc_struct *hp = tty->driver_data;

	return hp->n_outbuf;
}

static void hvc_poll(int index)
{
	struct hvc_struct *hp = &hvc_struct[index];
	struct tty_struct *tty;
	int i, n;
	char buf[16] __ALIGNED__;
	unsigned long flags;

	spin_lock_irqsave(&hp->lock, flags);

	if (hp->n_outbuf > 0)
		hvc_push(hp);

	tty = hp->tty;
	if (tty) {
		for (;;) {
			if (TTY_FLIPBUF_SIZE - tty->flip.count < sizeof(buf))
				break;
			n = hvc_get_chars(index + hvc_offset, buf, sizeof(buf));
			if (n <= 0)
				break;
			for (i = 0; i < n; ++i) {
#ifdef CONFIG_MAGIC_SYSRQ		/* Handle the SysRq Hack */
				if (buf[i] == '\x0f') {	/* ^O -- should support a sequence */
					sysrq_pressed = 1;
					continue;
				} else if (sysrq_pressed) {
					handle_sysrq(buf[i], NULL, NULL, tty);
					sysrq_pressed = 0;
					continue;
				}
#endif
				tty_insert_flip_char(tty, buf[i], 0);
			}
		}
		if (tty->flip.count)
			tty_schedule_flip(tty);

		if (hp->do_wakeup) {
			hp->do_wakeup = 0;
			if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP))
			    && tty->ldisc.write_wakeup)
				(tty->ldisc.write_wakeup)(tty);
			wake_up_interruptible(&tty->write_wait);
		}
	}

	spin_unlock_irqrestore(&hp->lock, flags);
}

int khvcd(void *unused)
{
	int i;

	daemonize();
	reparent_to_init();
	strcpy(current->comm, "khvcd");
	sigfillset(&current->blocked);

	for (;;) {
		for (i = 0; i < MAX_NR_HVC_CONSOLES; ++i)
			hvc_poll(i);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(TIMEOUT);
	}
}

int __init hvc_init(void)
{
	int i;

	memset(&hvc_driver, 0, sizeof(struct tty_driver));

	hvc_driver.magic = TTY_DRIVER_MAGIC;
	hvc_driver.driver_name = "hvc";
#ifdef CONFIG_DEVFS_FS
	hvc_driver.name = "hvc/%d";
#else
	hvc_driver.name = "hvc";
#endif
	hvc_driver.major = HVC_MAJOR;
	hvc_driver.minor_start = HVC_MINOR;
	hvc_driver.num = hvc_count(&hvc_offset);
	if (hvc_driver.num > MAX_NR_HVC_CONSOLES)
		hvc_driver.num = MAX_NR_HVC_CONSOLES;
	hvc_driver.type = TTY_DRIVER_TYPE_SYSTEM;
	hvc_driver.init_termios = tty_std_termios;
	hvc_driver.flags = TTY_DRIVER_REAL_RAW;
	hvc_driver.refcount = &hvc_refcount;
	hvc_driver.table = hvc_table;
	hvc_driver.termios = hvc_termios;
	hvc_driver.termios_locked = hvc_termios_locked;

	hvc_driver.open = hvc_open;
	hvc_driver.close = hvc_close;
	hvc_driver.write = hvc_write;
	hvc_driver.hangup = hvc_hangup;
	hvc_driver.write_room = hvc_write_room;
	hvc_driver.chars_in_buffer = hvc_chars_in_buffer;

	for (i = 0; i < hvc_driver.num; i++) {
		hvc_struct[i].lock = SPIN_LOCK_UNLOCKED;
		hvc_struct[i].index = i;
		tty_register_devfs(&hvc_driver, 0, hvc_driver.minor_start + i);
	}

	if (tty_register_driver(&hvc_driver))
		panic("Couldn't register hvc console driver\n");

	if (hvc_driver.num > 0)
		kernel_thread(khvcd, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);

	return 0;
}

static void __exit hvc_exit(void)
{
}

void hvc_console_print(struct console *co, const char *b, unsigned count)
{
	char c[16] __ALIGNED__;
	unsigned i, n;
	int r, donecr = 0;

	i = n = 0;
	while (count > 0 || i > 0) {
		if (count > 0 && i < sizeof(c)) {
			if (b[n] == '\n' && !donecr) {
				c[i++] = '\r';
				donecr = 1;
			} else {
				c[i++] = b[n++];
				donecr = 0;
				--count;
			}
		} else {
			r = hvc_put_chars(co->index + hvc_offset, c, i);
			if (r < 0) {
				/* throw away chars on error */
				i = 0;
			} else if (r > 0) {
				i -= r;
				if (i > 0)
					memmove(c, c+r, i);
			}
		}
	}
}

static kdev_t hvc_console_device(struct console *c)
{
	return MKDEV(HVC_MAJOR, HVC_MINOR + c->index);
}

int hvc_wait_for_keypress(struct console *co)
{
	char c[16] __ALIGNED__;

	while (hvc_get_chars(co->index, &c[0], 1) < 1)
		;
	return 0;
}

static int __init hvc_console_setup(struct console *co, char *options)
{
	if (co->index < 0 || co->index >= MAX_NR_HVC_CONSOLES
	    || co->index >= hvc_count(&hvc_offset))
		return -1;
	return 0;
}

struct console hvc_con_driver = {
	name:		"hvc",
	write:		hvc_console_print,
	device:		hvc_console_device,
	setup:		hvc_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

int __init hvc_console_init(void)
{
	register_console(&hvc_con_driver);
	return 0;
}

module_init(hvc_init);
module_exit(hvc_exit);
