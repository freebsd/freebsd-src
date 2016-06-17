/*
 * NETLINK	An implementation of a loadable kernel mode driver providing
 *		multiple kernel/user space bidirectional communications links.
 *
 * 		Author: 	Alan Cox <alan@redhat.com>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 * 
 *	Now netlink devices are emulated on the top of netlink sockets
 *	by compatibility reasons. Remove this file after a period. --ANK
 *
 */

#include <linux/module.h>

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/smp_lock.h>

#include <asm/bitops.h>
#include <asm/system.h>
#include <asm/uaccess.h>

static long open_map;
static struct socket *netlink_user[MAX_LINKS];

/*
 *	Device operations
 */
 
static unsigned int netlink_poll(struct file *file, poll_table * wait)
{
	struct socket *sock = netlink_user[MINOR(file->f_dentry->d_inode->i_rdev)];

	if (sock->ops->poll==NULL)
		return 0;
	return sock->ops->poll(file, sock, wait);
}

/*
 *	Write a message to the kernel side of a communication link
 */
 
static ssize_t netlink_write(struct file * file, const char * buf,
			     size_t count, loff_t *pos)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct socket *sock = netlink_user[MINOR(inode->i_rdev)];
	struct msghdr msg;
	struct iovec iov;

	iov.iov_base = (void*)buf;
	iov.iov_len = count;
	msg.msg_name=NULL;
	msg.msg_namelen=0;
	msg.msg_controllen=0;
	msg.msg_flags=0;
	msg.msg_iov=&iov;
	msg.msg_iovlen=1;

	return sock_sendmsg(sock, &msg, count);
}

/*
 *	Read a message from the kernel side of the communication link
 */

static ssize_t netlink_read(struct file * file, char * buf,
			    size_t count, loff_t *pos)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct socket *sock = netlink_user[MINOR(inode->i_rdev)];
	struct msghdr msg;
	struct iovec iov;

	iov.iov_base = buf;
	iov.iov_len = count;
	msg.msg_name=NULL;
	msg.msg_namelen=0;
	msg.msg_controllen=0;
	msg.msg_flags=0;
	msg.msg_iov=&iov;
	msg.msg_iovlen=1;
	if (file->f_flags&O_NONBLOCK)
		msg.msg_flags=MSG_DONTWAIT;

	return sock_recvmsg(sock, &msg, count, msg.msg_flags);
}

static int netlink_open(struct inode * inode, struct file * file)
{
	unsigned int minor = MINOR(inode->i_rdev);
	struct socket *sock;
	struct sockaddr_nl nladdr;
	int err;

	if (minor>=MAX_LINKS)
		return -ENODEV;
	if (test_and_set_bit(minor, &open_map))
		return -EBUSY;

	err = sock_create(PF_NETLINK, SOCK_RAW, minor, &sock);
	if (err < 0)
		goto out;

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_groups = ~0;
	if ((err = sock->ops->bind(sock, (struct sockaddr*)&nladdr, sizeof(nladdr))) < 0) {
		sock_release(sock);
		goto out;
	}

	netlink_user[minor] = sock;
	return 0;

out:
	clear_bit(minor, &open_map);
	return err;
}

static int netlink_release(struct inode * inode, struct file * file)
{
	unsigned int minor = MINOR(inode->i_rdev);
	struct socket *sock;

	sock = netlink_user[minor];
	netlink_user[minor] = NULL;
	clear_bit(minor, &open_map);
	sock_release(sock);
	return 0;
}


static int netlink_ioctl(struct inode *inode, struct file *file,
		    unsigned int cmd, unsigned long arg)
{
	unsigned int minor = MINOR(inode->i_rdev);
	int retval = 0;

	if (minor >= MAX_LINKS)
		return -ENODEV;
	switch ( cmd ) {
		default:
			retval = -EINVAL;
	}
	return retval;
}


static struct file_operations netlink_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		netlink_read,
	write:		netlink_write,
	poll:		netlink_poll,
	ioctl:		netlink_ioctl,
	open:		netlink_open,
	release:	netlink_release,
};

static devfs_handle_t devfs_handle;

static void __init make_devfs_entries (const char *name, int minor)
{
	devfs_register (devfs_handle, name, DEVFS_FL_DEFAULT,
			NETLINK_MAJOR, minor,
			S_IFCHR | S_IRUSR | S_IWUSR,
			&netlink_fops, NULL);
}

int __init init_netlink(void)
{
	if (devfs_register_chrdev(NETLINK_MAJOR,"netlink", &netlink_fops)) {
		printk(KERN_ERR "netlink: unable to get major %d\n", NETLINK_MAJOR);
		return -EIO;
	}
	devfs_handle = devfs_mk_dir (NULL, "netlink", NULL);
	/*  Someone tell me the official names for the uppercase ones  */
	make_devfs_entries ("route", 0);
	make_devfs_entries ("skip", 1);
	make_devfs_entries ("usersock", 2);
	make_devfs_entries ("fwmonitor", 3);
	make_devfs_entries ("tcpdiag", 4);
	make_devfs_entries ("arpd", 8);
	make_devfs_entries ("route6", 11);
	make_devfs_entries ("ip6_fw", 13);
	make_devfs_entries ("dnrtmsg", 13);
	devfs_register_series (devfs_handle, "tap%u", 16, DEVFS_FL_DEFAULT,
			       NETLINK_MAJOR, 16,
			       S_IFCHR | S_IRUSR | S_IWUSR,
			       &netlink_fops, NULL);
	return 0;
}

#ifdef MODULE

MODULE_LICENSE("GPL");

int init_module(void)
{
	printk(KERN_INFO "Network Kernel/User communications module 0.04\n");
	return init_netlink();
}

void cleanup_module(void)
{
	devfs_unregister (devfs_handle);
	devfs_unregister_chrdev(NETLINK_MAJOR, "netlink");
}

#endif
