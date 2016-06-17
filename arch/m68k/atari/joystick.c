/*
 * Atari Joystick Driver for Linux
 * by Robert de Vries (robert@and.nl) 19Jul93
 *
 * 16 Nov 1994 Andreas Schwab
 * Support for three button mouse (shamelessly stolen from MiNT)
 * third button wired to one of the joystick directions on joystick 1
 */

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/major.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/smp_lock.h>

#include <asm/atarikb.h>
#include <asm/atari_joystick.h>
#include <asm/uaccess.h>

#define MAJOR_NR    JOYSTICK_MAJOR

#define	ANALOG_JOY(n)	(!(n & 0x80))
#define	DIGITAL_JOY(n)	(n & 0x80)
#define	DEVICE_NR(n)	(MINOR(n) & 0x7f)


static struct joystick_status joystick[2];
int atari_mouse_buttons; /* for three-button mouse */

void atari_joystick_interrupt(char *buf)
{
    int j;
/*    ikbd_joystick_disable(); */

    j = buf[0] & 0x1;
    joystick[j].dir   = buf[1] & 0xF;
    joystick[j].fire  = (buf[1] & 0x80) >> 7;
    joystick[j].ready = 1;
    wake_up_interruptible(&joystick[j].wait);

    /* For three-button mouse emulation fake a mouse packet */
    if (atari_mouse_interrupt_hook &&
	j == 1 && (buf[1] & 1) != ((atari_mouse_buttons & 2) >> 1))
      {
	char faked_packet[3];

	atari_mouse_buttons = (atari_mouse_buttons & 5) | ((buf[1] & 1) << 1);
	faked_packet[0] = (atari_mouse_buttons & 1) | 
			  (atari_mouse_buttons & 4 ? 2 : 0);
	faked_packet[1] = 0;
	faked_packet[2] = 0;
	atari_mouse_interrupt_hook (faked_packet);
      }

/*    ikbd_joystick_event_on(); */
}

static int release_joystick(struct inode *inode, struct file *file)
{
    int minor = DEVICE_NR(inode->i_rdev);

    lock_kernel();
    joystick[minor].active = 0;
    joystick[minor].ready = 0;

    if ((joystick[0].active == 0) && (joystick[1].active == 0))
	ikbd_joystick_disable();
    unlock_kernel();
    return 0;
}

static int open_joystick(struct inode *inode, struct file *file)
{
    int minor = DEVICE_NR(inode->i_rdev);

    if (!DIGITAL_JOY(inode->i_rdev) || minor > 1)
	return -ENODEV;
    if (joystick[minor].active)
	return -EBUSY;
    joystick[minor].active = 1;
    joystick[minor].ready = 0;
    ikbd_joystick_event_on();
    return 0;
}

static ssize_t write_joystick(struct file *file, const char *buffer,
			      size_t count, loff_t *ppos)
{
    return -EINVAL;
}

static ssize_t read_joystick(struct file *file, char *buffer, size_t count,
			     loff_t *ppos)
{
    struct inode *inode = file->f_dentry->d_inode;
    int minor = DEVICE_NR(inode->i_rdev);

    if (count < 2)
	return -EINVAL;
    if (!joystick[minor].ready)
	return -EAGAIN;
    joystick[minor].ready = 0;
    if (put_user(joystick[minor].fire, buffer++) ||
	put_user(joystick[minor].dir, buffer++))
	return -EFAULT;
    if (count > 2)
	if (clear_user(buffer, count - 2))
	    return -EFAULT;
    return count;
}

static unsigned int joystick_poll(struct file *file, poll_table *wait)
{
    int minor = DEVICE_NR(file->f_dentry->d_inode->i_rdev);

    poll_wait(file, &joystick[minor].wait, wait);
    if (joystick[minor].ready)
	return POLLIN | POLLRDNORM;
    return 0;
}

struct file_operations atari_joystick_fops = {
	read:		read_joystick,
	write:		write_joystick,
	poll:		joystick_poll,
	open:		open_joystick,
	release:	release_joystick,
};

int __init atari_joystick_init(void)
{
    joystick[0].active = joystick[1].active = 0;
    joystick[0].ready = joystick[1].ready = 0;
    init_waitqueue_head(&joystick[0].wait);
    init_waitqueue_head(&joystick[1].wait);

    if (devfs_register_chrdev(MAJOR_NR, "Joystick", &atari_joystick_fops))
	printk("unable to get major %d for joystick devices\n", MAJOR_NR);
    devfs_register_series (NULL, "joysticks/digital%u", 2, DEVFS_FL_DEFAULT,
			   MAJOR_NR, 128, S_IFCHR | S_IRUSR | S_IWUSR,
			   &atari_joystick_fops, NULL);

    return 0;
}
