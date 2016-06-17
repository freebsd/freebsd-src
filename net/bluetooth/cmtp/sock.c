/* 
   CMTP implementation for Linux Bluetooth stack (BlueZ).
   Copyright (C) 2002-2003 Marcel Holtmann <marcel@holtmann.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation;

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
   IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
   CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES 
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN 
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF 
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS, 
   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS 
   SOFTWARE IS DISCLAIMED.
*/

#include <linux/config.h>
#include <linux/module.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/ioctl.h>
#include <linux/file.h>
#include <net/sock.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include "cmtp.h"

#ifndef CONFIG_BLUEZ_CMTP_DEBUG
#undef  BT_DBG
#define BT_DBG(D...)
#endif

static int cmtp_sock_release(struct socket *sock)
{
	struct sock *sk = sock->sk;

	BT_DBG("sock %p sk %p", sock, sk);

	if (!sk)
		return 0;

	sock_orphan(sk);
	sock_put(sk);

	MOD_DEC_USE_COUNT;
	return 0;
}

static int cmtp_sock_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	struct cmtp_connadd_req ca;
	struct cmtp_conndel_req cd;
	struct cmtp_connlist_req cl;
	struct cmtp_conninfo ci;
	struct socket *nsock;
	int err;

	BT_DBG("cmd %x arg %lx", cmd, arg);

	switch (cmd) {
	case CMTPCONNADD:
		if (!capable(CAP_NET_ADMIN))
			return -EACCES;

		if (copy_from_user(&ca, (void *) arg, sizeof(ca)))
			return -EFAULT;

		nsock = sockfd_lookup(ca.sock, &err);
		if (!nsock)
			return err;

		if (nsock->sk->state != BT_CONNECTED) {
			fput(nsock->file);
			return -EBADFD;
		}

		err = cmtp_add_connection(&ca, nsock);
		if (!err) {
			if (copy_to_user((void *) arg, &ca, sizeof(ca)))
				err = -EFAULT;
		} else
			fput(nsock->file);

		return err;

	case CMTPCONNDEL:
		if (!capable(CAP_NET_ADMIN))
			return -EACCES;

		if (copy_from_user(&cd, (void *) arg, sizeof(cd)))
			return -EFAULT;

		return cmtp_del_connection(&cd);

	case CMTPGETCONNLIST:
		if (copy_from_user(&cl, (void *) arg, sizeof(cl)))
			return -EFAULT;

		if (cl.cnum <= 0)
			return -EINVAL;

		err = cmtp_get_connlist(&cl);
		if (!err && copy_to_user((void *) arg, &cl, sizeof(cl)))
			return -EFAULT;

		return err;

	case CMTPGETCONNINFO:
		if (copy_from_user(&ci, (void *) arg, sizeof(ci)))
			return -EFAULT;

		err = cmtp_get_conninfo(&ci);
		if (!err && copy_to_user((void *) arg, &ci, sizeof(ci)))
			return -EFAULT;

		return err;
	}

	return -EINVAL;
}

static struct proto_ops cmtp_sock_ops = {
	family:		PF_BLUETOOTH,
	release:	cmtp_sock_release,
	ioctl:		cmtp_sock_ioctl,
	bind:		sock_no_bind,
	getname:	sock_no_getname,
	sendmsg:	sock_no_sendmsg,
	recvmsg:	sock_no_recvmsg,
	poll:		sock_no_poll,
	listen:		sock_no_listen,
	shutdown:	sock_no_shutdown,
	setsockopt:	sock_no_setsockopt,
	getsockopt:	sock_no_getsockopt,
	connect:	sock_no_connect,
	socketpair:	sock_no_socketpair,
	accept:		sock_no_accept,
	mmap:		sock_no_mmap
};

static int cmtp_sock_create(struct socket *sock, int protocol)
{
	struct sock *sk;

	BT_DBG("sock %p", sock);

	if (sock->type != SOCK_RAW)
		return -ESOCKTNOSUPPORT;

	sock->ops = &cmtp_sock_ops;

	if (!(sk = sk_alloc(PF_BLUETOOTH, GFP_KERNEL, 1)))
		return -ENOMEM;

	MOD_INC_USE_COUNT;

	sock->state = SS_UNCONNECTED;
	sock_init_data(sock, sk);

	sk->destruct = NULL;
	sk->protocol = protocol;

	return 0;
}

static struct net_proto_family cmtp_sock_family_ops = {
	family:		PF_BLUETOOTH,
	create:		cmtp_sock_create
};

int cmtp_init_sockets(void)
{
	int err;

	if ((err = bluez_sock_register(BTPROTO_CMTP, &cmtp_sock_family_ops))) {
		BT_ERR("Can't register CMTP socket layer (%d)", err);
		return err;
	}

	return 0;
}

void cmtp_cleanup_sockets(void)
{
	int err;

	if ((err = bluez_sock_unregister(BTPROTO_CMTP)))
		BT_ERR("Can't unregister CMTP socket layer (%d)", err);

	return;
}
