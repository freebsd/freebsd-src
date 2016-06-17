/*
 *	AX.25 release 037
 *
 *	This code REQUIRES 2.1.15 or higher/ NET3.038
 *
 *	This module:
 *		This module is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	History
 *	AX.25 036	Jonathan(G4KLX)	Split from af_ax25.c.
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <net/ax25.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/netfilter.h>
#include <linux/sysctl.h>
#include <net/ip.h>
#include <net/arp.h>

/*
 *	Callsign/UID mapper. This is in kernel space for security on multi-amateur machines.
 */

static ax25_uid_assoc *ax25_uid_list;

int ax25_uid_policy = 0;

ax25_address *ax25_findbyuid(uid_t uid)
{
	ax25_uid_assoc *ax25_uid;

	for (ax25_uid = ax25_uid_list; ax25_uid != NULL; ax25_uid = ax25_uid->next) {
		if (ax25_uid->uid == uid)
			return &ax25_uid->call;
	}

	return NULL;
}

int ax25_uid_ioctl(int cmd, struct sockaddr_ax25 *sax)
{
	ax25_uid_assoc *s, *ax25_uid;
	unsigned long flags;

	switch (cmd) {
		case SIOCAX25GETUID:
			for (ax25_uid = ax25_uid_list; ax25_uid != NULL; ax25_uid = ax25_uid->next) {
				if (ax25cmp(&sax->sax25_call, &ax25_uid->call) == 0)
					return ax25_uid->uid;
			}
			return -ENOENT;

		case SIOCAX25ADDUID:
			if (!capable(CAP_NET_ADMIN))
				return -EPERM;
			if (ax25_findbyuid(sax->sax25_uid))
				return -EEXIST;
			if (sax->sax25_uid == 0)
				return -EINVAL;
			if ((ax25_uid = kmalloc(sizeof(*ax25_uid), GFP_KERNEL)) == NULL)
				return -ENOMEM;
			ax25_uid->uid  = sax->sax25_uid;
			ax25_uid->call = sax->sax25_call;
			save_flags(flags); cli();
			ax25_uid->next = ax25_uid_list;
			ax25_uid_list  = ax25_uid;
			restore_flags(flags);
			return 0;

		case SIOCAX25DELUID:
			if (!capable(CAP_NET_ADMIN))
				return -EPERM;
			for (ax25_uid = ax25_uid_list; ax25_uid != NULL; ax25_uid = ax25_uid->next) {
				if (ax25cmp(&sax->sax25_call, &ax25_uid->call) == 0)
					break;
			}
			if (ax25_uid == NULL)
				return -ENOENT;
			save_flags(flags); cli();
			if ((s = ax25_uid_list) == ax25_uid) {
				ax25_uid_list = s->next;
				restore_flags(flags);
				kfree(ax25_uid);
				return 0;
			}
			while (s != NULL && s->next != NULL) {
				if (s->next == ax25_uid) {
					s->next = ax25_uid->next;
					restore_flags(flags);
					kfree(ax25_uid);
					return 0;
				}
				s = s->next;
			}
			restore_flags(flags);
			return -ENOENT;

		default:
			return -EINVAL;
	}

	return -EINVAL;	/*NOTREACHED */
}

int ax25_uid_get_info(char *buffer, char **start, off_t offset, int length)
{
	ax25_uid_assoc *pt;
	int len     = 0;
	off_t pos   = 0;
	off_t begin = 0;

	cli();

	len += sprintf(buffer, "Policy: %d\n", ax25_uid_policy);

	for (pt = ax25_uid_list; pt != NULL; pt = pt->next) {
		len += sprintf(buffer + len, "%6d %s\n", pt->uid, ax2asc(&pt->call));

		pos = begin + len;

		if (pos < offset) {
			len = 0;
			begin = pos;
		}

		if (pos > offset + length)
			break;
	}

	sti();

	*start = buffer + (offset - begin);
	len   -= offset - begin;

	if (len > length) len = length;

	return len;
}

/*
 *	Free all memory associated with UID/Callsign structures.
 */
void __exit ax25_uid_free(void)
{
	ax25_uid_assoc *s, *ax25_uid = ax25_uid_list;

	while (ax25_uid != NULL) {
		s        = ax25_uid;
		ax25_uid = ax25_uid->next;

		kfree(s);
	}
}
