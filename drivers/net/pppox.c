/** -*- linux-c -*- ***********************************************************
 * Linux PPP over X/Ethernet (PPPoX/PPPoE) Sockets
 *
 * PPPoX --- Generic PPP encapsulation socket family
 * PPPoE --- PPP over Ethernet (RFC 2516)
 *
 *
 * Version:	0.5.2
 *
 * Author:	Michal Ostrowski <mostrows@speakeasy.net>
 *
 * 051000 :	Initialization cleanup
 *
 * License:
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 */

#include <linux/string.h>
#include <linux/module.h>

#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include <linux/netdevice.h>
#include <linux/net.h>
#include <linux/init.h>
#include <linux/if_pppox.h>
#include <net/sock.h>
#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>
#include <linux/ppp_channel.h>

static struct pppox_proto *proto[PX_MAX_PROTO+1];

int register_pppox_proto(int proto_num, struct pppox_proto *pp)
{
	if (proto_num < 0 || proto_num > PX_MAX_PROTO) {
		return -EINVAL;
	}

	if (proto[proto_num])
		return -EALREADY;

	MOD_INC_USE_COUNT;

	proto[proto_num] = pp;
	return 0;
}

void unregister_pppox_proto(int proto_num)
{
	if (proto_num >= 0 && proto_num <= PX_MAX_PROTO) {
	    proto[proto_num] = NULL;
	    MOD_DEC_USE_COUNT;
	}
}

void pppox_unbind_sock(struct sock *sk)
{
	/* Clear connection to ppp device, if attached. */

	if (sk->state & (PPPOX_BOUND|PPPOX_ZOMBIE)) {
		ppp_unregister_channel(&sk->protinfo.pppox->chan);
		sk->state = PPPOX_DEAD;
	}
}

EXPORT_SYMBOL(register_pppox_proto);
EXPORT_SYMBOL(unregister_pppox_proto);
EXPORT_SYMBOL(pppox_unbind_sock);

static int pppox_ioctl(struct socket* sock, unsigned int cmd,
		       unsigned long arg)
{
	struct sock *sk = sock->sk;
	struct pppox_opt *po;
	int err = 0;

	po = sk->protinfo.pppox;

	lock_sock(sk);

	switch (cmd) {
	case PPPIOCGCHAN:{
		int index;
		err = -ENOTCONN;
		if (!(sk->state & PPPOX_CONNECTED))
			break;

		err = -EINVAL;
		index = ppp_channel_index(&po->chan);
		if (put_user(index , (int *) arg))
			break;

		err = 0;
		sk->state |= PPPOX_BOUND;
		break;
	}
	default:
		if (proto[sk->protocol]->ioctl)
			err = (*proto[sk->protocol]->ioctl)(sock, cmd, arg);

		break;
	};

	release_sock(sk);
	return err;
}


static int pppox_create(struct socket *sock, int protocol)
{
	int err = 0;

	if (protocol < 0 || protocol > PX_MAX_PROTO)
	    return -EPROTOTYPE;

	if (proto[protocol] == NULL)
	    return -EPROTONOSUPPORT;

	err = (*proto[protocol]->create)(sock);

	if (err == 0) {
		/* We get to set the ioctl handler. */
		/* For everything else, pppox is just a shell. */
		sock->ops->ioctl = pppox_ioctl;
	}

	return err;
}

static struct net_proto_family pppox_proto_family = {
	PF_PPPOX,
	pppox_create
};

static int __init pppox_init(void)
{
	int err = 0;

	err = sock_register(&pppox_proto_family);

	return err;
}

static void __exit pppox_exit(void)
{
	sock_unregister(PF_PPPOX);
}

module_init(pppox_init);
module_exit(pppox_exit);

MODULE_AUTHOR("Michal Ostrowski <mostrows@speakeasy.net>");
MODULE_DESCRIPTION("PPP over Ethernet driver (generic socket layer)");
MODULE_LICENSE("GPL");
