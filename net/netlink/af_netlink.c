/*
 * NETLINK      Kernel-user communication protocol.
 *
 * 		Authors:	Alan Cox <alan@redhat.com>
 * 				Alexey Kuznetsov <kuznet@ms2.inr.ac.ru>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 * 
 * Tue Jun 26 14:36:48 MEST 2001 Herbert "herp" Rosmanith
 *                               added netlink_proto_exit
 *
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/socket.h>
#include <linux/un.h>
#include <linux/fcntl.h>
#include <linux/termios.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/proc_fs.h>
#include <linux/smp_lock.h>
#include <linux/notifier.h>
#include <net/sock.h>
#include <net/scm.h>

#define Nprintk(a...)

#if defined(CONFIG_NETLINK_DEV) || defined(CONFIG_NETLINK_DEV_MODULE)
#define NL_EMULATE_DEV
#endif

struct netlink_opt
{
	u32			pid;
	unsigned		groups;
	u32			dst_pid;
	unsigned		dst_groups;
	unsigned long		state;
	int			(*handler)(int unit, struct sk_buff *skb);
	wait_queue_head_t	wait;
	struct netlink_callback	*cb;
	spinlock_t		cb_lock;
	void			(*data_ready)(struct sock *sk, int bytes);
};

static struct sock *nl_table[MAX_LINKS];
static DECLARE_WAIT_QUEUE_HEAD(nl_table_wait);
static unsigned nl_nonroot[MAX_LINKS];

#ifdef NL_EMULATE_DEV
static struct socket *netlink_kernel[MAX_LINKS];
#endif

static int netlink_dump(struct sock *sk);
static void netlink_destroy_callback(struct netlink_callback *cb);

atomic_t netlink_sock_nr;

static rwlock_t nl_table_lock = RW_LOCK_UNLOCKED;
static atomic_t nl_table_users = ATOMIC_INIT(0);

static struct notifier_block *netlink_chain;

static void netlink_sock_destruct(struct sock *sk)
{
	skb_queue_purge(&sk->receive_queue);

	if (!sk->dead) {
		printk("Freeing alive netlink socket %p\n", sk);
		return;
	}
	BUG_TRAP(atomic_read(&sk->rmem_alloc)==0);
	BUG_TRAP(atomic_read(&sk->wmem_alloc)==0);
	BUG_TRAP(sk->protinfo.af_netlink->cb==NULL);

	kfree(sk->protinfo.af_netlink);

	atomic_dec(&netlink_sock_nr);
#ifdef NETLINK_REFCNT_DEBUG
	printk(KERN_DEBUG "NETLINK %p released, %d are still alive\n", sk, atomic_read(&netlink_sock_nr));
#endif
}

/* This lock without WQ_FLAG_EXCLUSIVE is good on UP and it is _very_ bad on SMP.
 * Look, when several writers sleep and reader wakes them up, all but one
 * immediately hit write lock and grab all the cpus. Exclusive sleep solves
 * this, _but_ remember, it adds useless work on UP machines.
 */

static void netlink_table_grab(void)
{
	write_lock_bh(&nl_table_lock);

	if (atomic_read(&nl_table_users)) {
		DECLARE_WAITQUEUE(wait, current);

		add_wait_queue_exclusive(&nl_table_wait, &wait);
		for(;;) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			if (atomic_read(&nl_table_users) == 0)
				break;
			write_unlock_bh(&nl_table_lock);
			schedule();
			write_lock_bh(&nl_table_lock);
		}

		__set_current_state(TASK_RUNNING);
		remove_wait_queue(&nl_table_wait, &wait);
	}
}

static __inline__ void netlink_table_ungrab(void)
{
	write_unlock_bh(&nl_table_lock);
	wake_up(&nl_table_wait);
}

static __inline__ void
netlink_lock_table(void)
{
	/* read_lock() synchronizes us to netlink_table_grab */

	read_lock(&nl_table_lock);
	atomic_inc(&nl_table_users);
	read_unlock(&nl_table_lock);
}

static __inline__ void
netlink_unlock_table(void)
{
	if (atomic_dec_and_test(&nl_table_users))
		wake_up(&nl_table_wait);
}

static __inline__ struct sock *netlink_lookup(int protocol, u32 pid)
{
	struct sock *sk;

	read_lock(&nl_table_lock);
	for (sk=nl_table[protocol]; sk; sk=sk->next) {
		if (sk->protinfo.af_netlink->pid == pid) {
			sock_hold(sk);
			read_unlock(&nl_table_lock);
			return sk;
		}
	}

	read_unlock(&nl_table_lock);
	return NULL;
}

extern struct proto_ops netlink_ops;

static int netlink_insert(struct sock *sk, u32 pid)
{
	int err = -EADDRINUSE;
	struct sock *osk;

	netlink_table_grab();
	for (osk=nl_table[sk->protocol]; osk; osk=osk->next) {
		if (osk->protinfo.af_netlink->pid == pid)
			break;
	}
	if (osk == NULL) {
		err = -EBUSY;
		if (sk->protinfo.af_netlink->pid == 0) {
			sk->protinfo.af_netlink->pid = pid;
			sk->next = nl_table[sk->protocol];
			nl_table[sk->protocol] = sk;
			sock_hold(sk);
			err = 0;
		}
	}
	netlink_table_ungrab();
	return err;
}

static void netlink_remove(struct sock *sk)
{
	struct sock **skp;

	netlink_table_grab();
	for (skp = &nl_table[sk->protocol]; *skp; skp = &((*skp)->next)) {
		if (*skp == sk) {
			*skp = sk->next;
			__sock_put(sk);
			break;
		}
	}
	netlink_table_ungrab();
}

static int netlink_create(struct socket *sock, int protocol)
{
	struct sock *sk;

	sock->state = SS_UNCONNECTED;

	if (sock->type != SOCK_RAW && sock->type != SOCK_DGRAM)
		return -ESOCKTNOSUPPORT;

	if (protocol<0 || protocol >= MAX_LINKS)
		return -EPROTONOSUPPORT;

	sock->ops = &netlink_ops;

	sk = sk_alloc(PF_NETLINK, GFP_KERNEL, 1);
	if (!sk)
		return -ENOMEM;

	sock_init_data(sock,sk);

	sk->protinfo.af_netlink = kmalloc(sizeof(struct netlink_opt), GFP_KERNEL);
	if (sk->protinfo.af_netlink == NULL) {
		sk_free(sk);
		return -ENOMEM;
	}
	memset(sk->protinfo.af_netlink, 0, sizeof(struct netlink_opt));

	spin_lock_init(&sk->protinfo.af_netlink->cb_lock);
	init_waitqueue_head(&sk->protinfo.af_netlink->wait);
	sk->destruct = netlink_sock_destruct;
	atomic_inc(&netlink_sock_nr);

	sk->protocol=protocol;
	return 0;
}

static int netlink_release(struct socket *sock)
{
	struct sock *sk = sock->sk;

	if (!sk)
		return 0;

	netlink_remove(sk);

	spin_lock(&sk->protinfo.af_netlink->cb_lock);
	if (sk->protinfo.af_netlink->cb) {
		sk->protinfo.af_netlink->cb->done(sk->protinfo.af_netlink->cb);
		netlink_destroy_callback(sk->protinfo.af_netlink->cb);
		sk->protinfo.af_netlink->cb = NULL;
		__sock_put(sk);
	}
	spin_unlock(&sk->protinfo.af_netlink->cb_lock);

	/* OK. Socket is unlinked, and, therefore,
	   no new packets will arrive */

	sock_orphan(sk);
	sock->sk = NULL;
	wake_up_interruptible_all(&sk->protinfo.af_netlink->wait);

	skb_queue_purge(&sk->write_queue);

	if (sk->protinfo.af_netlink->pid && !sk->protinfo.af_netlink->groups) {
		struct netlink_notify n = { protocol:sk->protocol,
		                            pid:sk->protinfo.af_netlink->pid };
		notifier_call_chain(&netlink_chain, NETLINK_URELEASE, &n);
	}	
	
	sock_put(sk);
	return 0;
}

static int netlink_autobind(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct sock *osk;
	s32 pid = current->pid;
	int err;

retry:
	netlink_table_grab();
	for (osk=nl_table[sk->protocol]; osk; osk=osk->next) {
		if (osk->protinfo.af_netlink->pid == pid) {
			/* Bind collision, search negative pid values. */
			if (pid > 0)
				pid = -4096;
			pid--;
			netlink_table_ungrab();
			goto retry;
		}
	}
	netlink_table_ungrab();

	err = netlink_insert(sk, pid);
	if (err == -EADDRINUSE)
		goto retry;
	sk->protinfo.af_netlink->groups = 0;
	return 0;
}

static inline int netlink_capable(struct socket *sock, unsigned flag) 
{ 
	return (nl_nonroot[sock->sk->protocol] & flag) || capable(CAP_NET_ADMIN);
} 

static int netlink_bind(struct socket *sock, struct sockaddr *addr, int addr_len)
{
	struct sock *sk = sock->sk;
	int err;
	struct sockaddr_nl *nladdr=(struct sockaddr_nl *)addr;
	
	if (nladdr->nl_family != AF_NETLINK)
		return -EINVAL;

	/* Only superuser is allowed to listen multicasts */
	if (nladdr->nl_groups && !netlink_capable(sock, NL_NONROOT_RECV))
		return -EPERM;

	if (sk->protinfo.af_netlink->pid) {
		if (nladdr->nl_pid != sk->protinfo.af_netlink->pid)
			return -EINVAL;
		sk->protinfo.af_netlink->groups = nladdr->nl_groups;
		return 0;
	}

	if (nladdr->nl_pid == 0) {
		err = netlink_autobind(sock);
		if (err == 0)
			sk->protinfo.af_netlink->groups = nladdr->nl_groups;
		return err;
	}

	err = netlink_insert(sk, nladdr->nl_pid);
	if (err == 0)
		sk->protinfo.af_netlink->groups = nladdr->nl_groups;
	return err;
}

static int netlink_connect(struct socket *sock, struct sockaddr *addr,
			   int alen, int flags)
{
	int err = 0;
	struct sock *sk = sock->sk;
	struct sockaddr_nl *nladdr=(struct sockaddr_nl*)addr;

	if (addr->sa_family == AF_UNSPEC) {
		sk->protinfo.af_netlink->dst_pid = 0;
		sk->protinfo.af_netlink->dst_groups = 0;
		return 0;
	}
	if (addr->sa_family != AF_NETLINK)
		return -EINVAL;

	/* Only superuser is allowed to send multicasts */
	if (nladdr->nl_groups && !netlink_capable(sock, NL_NONROOT_SEND))
		return -EPERM;

	if (!sk->protinfo.af_netlink->pid)
		err = netlink_autobind(sock);

	if (err == 0) {
		sk->protinfo.af_netlink->dst_pid = nladdr->nl_pid;
		sk->protinfo.af_netlink->dst_groups = nladdr->nl_groups;
	}

	return 0;
}

static int netlink_getname(struct socket *sock, struct sockaddr *addr, int *addr_len, int peer)
{
	struct sock *sk = sock->sk;
	struct sockaddr_nl *nladdr=(struct sockaddr_nl *)addr;
	
	nladdr->nl_family = AF_NETLINK;
	nladdr->nl_pad = 0;
	*addr_len = sizeof(*nladdr);

	if (peer) {
		nladdr->nl_pid = sk->protinfo.af_netlink->dst_pid;
		nladdr->nl_groups = sk->protinfo.af_netlink->dst_groups;
	} else {
		nladdr->nl_pid = sk->protinfo.af_netlink->pid;
		nladdr->nl_groups = sk->protinfo.af_netlink->groups;
	}
	return 0;
}

static void netlink_overrun(struct sock *sk)
{
	if (!test_and_set_bit(0, &sk->protinfo.af_netlink->state)) {
		sk->err = ENOBUFS;
		sk->error_report(sk);
	}
}

int netlink_unicast(struct sock *ssk, struct sk_buff *skb, u32 pid, int nonblock)
{
	struct sock *sk;
	int len = skb->len;
	int protocol = ssk->protocol;
	long timeo;
        DECLARE_WAITQUEUE(wait, current);

	timeo = sock_sndtimeo(ssk, nonblock);

retry:
	sk = netlink_lookup(protocol, pid);
	if (sk == NULL)
		goto no_dst;

	/* Don't bother queuing skb if kernel socket has no input function */
	if (sk->protinfo.af_netlink->pid == 0 &&
	    !sk->protinfo.af_netlink->data_ready)
		goto no_dst;

#ifdef NL_EMULATE_DEV
	if (sk->protinfo.af_netlink->handler) {
		skb_orphan(skb);
		len = sk->protinfo.af_netlink->handler(protocol, skb);
		sock_put(sk);
		return len;
	}
#endif

	if (atomic_read(&sk->rmem_alloc) > sk->rcvbuf ||
	    test_bit(0, &sk->protinfo.af_netlink->state)) {
		if (!timeo) {
			if (ssk->protinfo.af_netlink->pid == 0)
				netlink_overrun(sk);
			sock_put(sk);
			kfree_skb(skb);
			return -EAGAIN;
		}

		__set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&sk->protinfo.af_netlink->wait, &wait);

		if ((atomic_read(&sk->rmem_alloc) > sk->rcvbuf ||
		    test_bit(0, &sk->protinfo.af_netlink->state)) &&
		    !sk->dead)
			timeo = schedule_timeout(timeo);

		__set_current_state(TASK_RUNNING);
		remove_wait_queue(&sk->protinfo.af_netlink->wait, &wait);
		sock_put(sk);

		if (signal_pending(current)) {
			kfree_skb(skb);
			return sock_intr_errno(timeo);
		}
		goto retry;
	}

	skb_orphan(skb);
	skb_set_owner_r(skb, sk);
	skb_queue_tail(&sk->receive_queue, skb);
	sk->data_ready(sk, len);
	sock_put(sk);
	return len;

no_dst:
	kfree_skb(skb);
	return -ECONNREFUSED;
}

static __inline__ int netlink_broadcast_deliver(struct sock *sk, struct sk_buff *skb)
{
#ifdef NL_EMULATE_DEV
	if (sk->protinfo.af_netlink->handler) {
		skb_orphan(skb);
		sk->protinfo.af_netlink->handler(sk->protocol, skb);
		return 0;
	} else
#endif
	if (atomic_read(&sk->rmem_alloc) <= sk->rcvbuf &&
	    !test_bit(0, &sk->protinfo.af_netlink->state)) {
                skb_orphan(skb);
		skb_set_owner_r(skb, sk);
		skb_queue_tail(&sk->receive_queue, skb);
		sk->data_ready(sk, skb->len);
		return 0;
	}
	return -1;
}

void netlink_broadcast(struct sock *ssk, struct sk_buff *skb, u32 pid,
		       u32 group, int allocation)
{
	struct sock *sk;
	struct sk_buff *skb2 = NULL;
	int protocol = ssk->protocol;
	int failure = 0;

	/* While we sleep in clone, do not allow to change socket list */

	netlink_lock_table();

	for (sk = nl_table[protocol]; sk; sk = sk->next) {
		if (ssk == sk)
			continue;

		if (sk->protinfo.af_netlink->pid == pid ||
		    !(sk->protinfo.af_netlink->groups&group))
			continue;

		if (failure) {
			netlink_overrun(sk);
			continue;
		}

		sock_hold(sk);
		if (skb2 == NULL) {
			if (atomic_read(&skb->users) != 1) {
				skb2 = skb_clone(skb, allocation);
			} else {
				skb2 = skb;
				atomic_inc(&skb->users);
			}
		}
		if (skb2 == NULL) {
			netlink_overrun(sk);
			/* Clone failed. Notify ALL listeners. */
			failure = 1;
		} else if (netlink_broadcast_deliver(sk, skb2)) {
			netlink_overrun(sk);
		} else
			skb2 = NULL;
		sock_put(sk);
	}

	netlink_unlock_table();

	if (skb2)
		kfree_skb(skb2);
	kfree_skb(skb);
}

void netlink_set_err(struct sock *ssk, u32 pid, u32 group, int code)
{
	struct sock *sk;
	int protocol = ssk->protocol;

	read_lock(&nl_table_lock);
	for (sk = nl_table[protocol]; sk; sk = sk->next) {
		if (ssk == sk)
			continue;

		if (sk->protinfo.af_netlink->pid == pid ||
		    !(sk->protinfo.af_netlink->groups&group))
			continue;

		sk->err = code;
		sk->error_report(sk);
	}
	read_unlock(&nl_table_lock);
}

static inline void netlink_rcv_wake(struct sock *sk)
{
	if (skb_queue_len(&sk->receive_queue) == 0)
		clear_bit(0, &sk->protinfo.af_netlink->state);
	if (!test_bit(0, &sk->protinfo.af_netlink->state))
		wake_up_interruptible(&sk->protinfo.af_netlink->wait);
}

static int netlink_sendmsg(struct socket *sock, struct msghdr *msg, int len,
			   struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	struct sockaddr_nl *addr=msg->msg_name;
	u32 dst_pid;
	u32 dst_groups;
	struct sk_buff *skb;
	int err;

	if (msg->msg_flags&MSG_OOB)
		return -EOPNOTSUPP;

	if (msg->msg_namelen) {
		if (addr->nl_family != AF_NETLINK)
			return -EINVAL;
		dst_pid = addr->nl_pid;
		dst_groups = addr->nl_groups;
		if (dst_groups && !netlink_capable(sock, NL_NONROOT_SEND))
			return -EPERM;
	} else {
		dst_pid = sk->protinfo.af_netlink->dst_pid;
		dst_groups = sk->protinfo.af_netlink->dst_groups;
	}

	if (!sk->protinfo.af_netlink->pid) {
		err = netlink_autobind(sock);
		if (err)
			goto out;
	}

	err = -EMSGSIZE;
	if ((unsigned)len > sk->sndbuf-32)
		goto out;
	err = -ENOBUFS;
	skb = alloc_skb(len, GFP_KERNEL);
	if (skb==NULL)
		goto out;

	NETLINK_CB(skb).pid = sk->protinfo.af_netlink->pid;
	NETLINK_CB(skb).groups = sk->protinfo.af_netlink->groups;
	NETLINK_CB(skb).dst_pid = dst_pid;
	NETLINK_CB(skb).dst_groups = dst_groups;
	memcpy(NETLINK_CREDS(skb), &scm->creds, sizeof(struct ucred));

	/* What can I do? Netlink is asynchronous, so that
	   we will have to save current capabilities to
	   check them, when this message will be delivered
	   to corresponding kernel module.   --ANK (980802)
	 */
	NETLINK_CB(skb).eff_cap = current->cap_effective;

	err = -EFAULT;
	if (memcpy_fromiovec(skb_put(skb,len), msg->msg_iov, len)) {
		kfree_skb(skb);
		goto out;
	}

	if (dst_groups) {
		atomic_inc(&skb->users);
		netlink_broadcast(sk, skb, dst_pid, dst_groups, GFP_KERNEL);
	}
	err = netlink_unicast(sk, skb, dst_pid, msg->msg_flags&MSG_DONTWAIT);

out:
	return err;
}

static int netlink_recvmsg(struct socket *sock, struct msghdr *msg, int len,
			   int flags, struct scm_cookie *scm)
{
	struct sock *sk = sock->sk;
	int noblock = flags&MSG_DONTWAIT;
	int copied;
	struct sk_buff *skb;
	int err;

	if (flags&MSG_OOB)
		return -EOPNOTSUPP;

	copied = 0;

	skb = skb_recv_datagram(sk,flags,noblock,&err);
	if (skb==NULL)
		goto out;

	msg->msg_namelen = 0;

	copied = skb->len;
	if (len < copied) {
		msg->msg_flags |= MSG_TRUNC;
		copied = len;
	}

	skb->h.raw = skb->data;
	err = skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);

	if (msg->msg_name) {
		struct sockaddr_nl *addr = (struct sockaddr_nl*)msg->msg_name;
		addr->nl_family = AF_NETLINK;
		addr->nl_pad    = 0;
		addr->nl_pid	= NETLINK_CB(skb).pid;
		addr->nl_groups	= NETLINK_CB(skb).dst_groups;
		msg->msg_namelen = sizeof(*addr);
	}

	scm->creds = *NETLINK_CREDS(skb);
	skb_free_datagram(sk, skb);

	if (sk->protinfo.af_netlink->cb
	    && atomic_read(&sk->rmem_alloc) <= sk->rcvbuf/2)
		netlink_dump(sk);

out:
	netlink_rcv_wake(sk);
	return err ? : copied;
}

void netlink_data_ready(struct sock *sk, int len)
{
	if (sk->protinfo.af_netlink->data_ready)
		sk->protinfo.af_netlink->data_ready(sk, len);
	netlink_rcv_wake(sk);
}

/*
 *	We export these functions to other modules. They provide a 
 *	complete set of kernel non-blocking support for message
 *	queueing.
 */

struct sock *
netlink_kernel_create(int unit, void (*input)(struct sock *sk, int len))
{
	struct socket *sock;
	struct sock *sk;

	if (unit<0 || unit>=MAX_LINKS)
		return NULL;

	if (!(sock = sock_alloc())) 
		return NULL;

	sock->type = SOCK_RAW;

	if (netlink_create(sock, unit) < 0) {
		sock_release(sock);
		return NULL;
	}
	sk = sock->sk;
	sk->data_ready = netlink_data_ready;
	if (input)
		sk->protinfo.af_netlink->data_ready = input;

	netlink_insert(sk, 0);
	return sk;
}

void netlink_set_nonroot(int protocol, unsigned flags)
{ 
	if ((unsigned)protocol < MAX_LINKS) 
		nl_nonroot[protocol] = flags;
} 

static void netlink_destroy_callback(struct netlink_callback *cb)
{
	if (cb->skb)
		kfree_skb(cb->skb);
	kfree(cb);
}

/*
 * It looks a bit ugly.
 * It would be better to create kernel thread.
 */

static int netlink_dump(struct sock *sk)
{
	struct netlink_callback *cb;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int len;
	
	skb = sock_rmalloc(sk, NLMSG_GOODSIZE, 0, GFP_KERNEL);
	if (!skb)
		return -ENOBUFS;

	spin_lock(&sk->protinfo.af_netlink->cb_lock);

	cb = sk->protinfo.af_netlink->cb;
	if (cb == NULL) {
		spin_unlock(&sk->protinfo.af_netlink->cb_lock);
		kfree_skb(skb);
		return -EINVAL;
	}

	len = cb->dump(skb, cb);

	if (len > 0) {
		spin_unlock(&sk->protinfo.af_netlink->cb_lock);
		skb_queue_tail(&sk->receive_queue, skb);
		sk->data_ready(sk, len);
		return 0;
	}

	nlh = __nlmsg_put(skb, NETLINK_CB(cb->skb).pid, cb->nlh->nlmsg_seq, NLMSG_DONE, sizeof(int));
	nlh->nlmsg_flags |= NLM_F_MULTI;
	memcpy(NLMSG_DATA(nlh), &len, sizeof(len));
	skb_queue_tail(&sk->receive_queue, skb);
	sk->data_ready(sk, skb->len);

	cb->done(cb);
	sk->protinfo.af_netlink->cb = NULL;
	spin_unlock(&sk->protinfo.af_netlink->cb_lock);

	netlink_destroy_callback(cb);
	sock_put(sk);
	return 0;
}

int netlink_dump_start(struct sock *ssk, struct sk_buff *skb,
		       struct nlmsghdr *nlh,
		       int (*dump)(struct sk_buff *skb, struct netlink_callback*),
		       int (*done)(struct netlink_callback*))
{
	struct netlink_callback *cb;
	struct sock *sk;

	cb = kmalloc(sizeof(*cb), GFP_KERNEL);
	if (cb == NULL)
		return -ENOBUFS;

	memset(cb, 0, sizeof(*cb));
	cb->dump = dump;
	cb->done = done;
	cb->nlh = nlh;
	atomic_inc(&skb->users);
	cb->skb = skb;

	sk = netlink_lookup(ssk->protocol, NETLINK_CB(skb).pid);
	if (sk == NULL) {
		netlink_destroy_callback(cb);
		return -ECONNREFUSED;
	}
	/* A dump is in progress... */
	spin_lock(&sk->protinfo.af_netlink->cb_lock);
	if (sk->protinfo.af_netlink->cb) {
		spin_unlock(&sk->protinfo.af_netlink->cb_lock);
		netlink_destroy_callback(cb);
		sock_put(sk);
		return -EBUSY;
	}
	sk->protinfo.af_netlink->cb = cb;
	spin_unlock(&sk->protinfo.af_netlink->cb_lock);

	netlink_dump(sk);
	return 0;
}

void netlink_ack(struct sk_buff *in_skb, struct nlmsghdr *nlh, int err)
{
	struct sk_buff *skb;
	struct nlmsghdr *rep;
	struct nlmsgerr *errmsg;
	int size;

	if (err == 0)
		size = NLMSG_SPACE(sizeof(struct nlmsgerr));
	else
		size = NLMSG_SPACE(4 + NLMSG_ALIGN(nlh->nlmsg_len));

	skb = alloc_skb(size, GFP_KERNEL);
	if (!skb) {
		struct sock *sk;

		sk = netlink_lookup(in_skb->sk->protocol,
				    NETLINK_CB(in_skb).pid);
		if (sk) {
			sk->err = ENOBUFS;
			sk->error_report(sk);
			sock_put(sk);
		}
	}

	rep = __nlmsg_put(skb, NETLINK_CB(in_skb).pid, nlh->nlmsg_seq,
			  NLMSG_ERROR, sizeof(struct nlmsgerr));
	errmsg = NLMSG_DATA(rep);
	errmsg->error = err;
	memcpy(&errmsg->msg, nlh, err ? nlh->nlmsg_len : sizeof(struct nlmsghdr));
	netlink_unicast(in_skb->sk, skb, NETLINK_CB(in_skb).pid, MSG_DONTWAIT);
}


#ifdef NL_EMULATE_DEV

static rwlock_t nl_emu_lock = RW_LOCK_UNLOCKED;

/*
 *	Backward compatibility.
 */	
 
int netlink_attach(int unit, int (*function)(int, struct sk_buff *skb))
{
	struct sock *sk = netlink_kernel_create(unit, NULL);
	if (sk == NULL)
		return -ENOBUFS;
	sk->protinfo.af_netlink->handler = function;
	write_lock_bh(&nl_emu_lock);
	netlink_kernel[unit] = sk->socket;
	write_unlock_bh(&nl_emu_lock);
	return 0;
}

void netlink_detach(int unit)
{
	struct socket *sock;

	write_lock_bh(&nl_emu_lock);
	sock = netlink_kernel[unit];
	netlink_kernel[unit] = NULL;
	write_unlock_bh(&nl_emu_lock);

	sock_release(sock);
}

int netlink_post(int unit, struct sk_buff *skb)
{
	struct socket *sock;

	read_lock(&nl_emu_lock);
	sock = netlink_kernel[unit];
	if (sock) {
		struct sock *sk = sock->sk;
		memset(skb->cb, 0, sizeof(skb->cb));
		sock_hold(sk);
		read_unlock(&nl_emu_lock);

		netlink_broadcast(sk, skb, 0, ~0, GFP_ATOMIC);

		sock_put(sk);
		return 0;
	}
	read_unlock(&nl_emu_lock);
	return -EUNATCH;
}

#endif


#ifdef CONFIG_PROC_FS
static int netlink_read_proc(char *buffer, char **start, off_t offset,
			     int length, int *eof, void *data)
{
	off_t pos=0;
	off_t begin=0;
	int len=0;
	int i;
	struct sock *s;
	
	len+= sprintf(buffer,"sk       Eth Pid    Groups   "
		      "Rmem     Wmem     Dump     Locks\n");
	
	for (i=0; i<MAX_LINKS; i++) {
		read_lock(&nl_table_lock);
		for (s = nl_table[i]; s; s = s->next) {
			len+=sprintf(buffer+len,"%p %-3d %-6d %08x %-8d %-8d %p %d",
				     s,
				     s->protocol,
				     s->protinfo.af_netlink->pid,
				     s->protinfo.af_netlink->groups,
				     atomic_read(&s->rmem_alloc),
				     atomic_read(&s->wmem_alloc),
				     s->protinfo.af_netlink->cb,
				     atomic_read(&s->refcnt)
				     );

			buffer[len++]='\n';
		
			pos=begin+len;
			if(pos<offset) {
				len=0;
				begin=pos;
			}
			if(pos>offset+length) {
				read_unlock(&nl_table_lock);
				goto done;
			}
		}
		read_unlock(&nl_table_lock);
	}
	*eof = 1;

done:
	*start=buffer+(offset-begin);
	len-=(offset-begin);
	if(len>length)
		len=length;
	if(len<0)
		len=0;
	return len;
}
#endif

int netlink_register_notifier(struct notifier_block *nb)
{
	return notifier_chain_register(&netlink_chain, nb);
}

int netlink_unregister_notifier(struct notifier_block *nb)
{
	return notifier_chain_unregister(&netlink_chain, nb);
}
                
struct proto_ops netlink_ops = {
	family:		PF_NETLINK,

	release:	netlink_release,
	bind:		netlink_bind,
	connect:	netlink_connect,
	socketpair:	sock_no_socketpair,
	accept:		sock_no_accept,
	getname:	netlink_getname,
	poll:		datagram_poll,
	ioctl:		sock_no_ioctl,
	listen:		sock_no_listen,
	shutdown:	sock_no_shutdown,
	setsockopt:	sock_no_setsockopt,
	getsockopt:	sock_no_getsockopt,
	sendmsg:	netlink_sendmsg,
	recvmsg:	netlink_recvmsg,
	mmap:		sock_no_mmap,
	sendpage:	sock_no_sendpage,
};

struct net_proto_family netlink_family_ops = {
	PF_NETLINK,
	netlink_create
};

static int __init netlink_proto_init(void)
{
	struct sk_buff *dummy_skb;

	if (sizeof(struct netlink_skb_parms) > sizeof(dummy_skb->cb)) {
		printk(KERN_CRIT "netlink_init: panic\n");
		return -1;
	}
	sock_register(&netlink_family_ops);
#ifdef CONFIG_PROC_FS
	create_proc_read_entry("net/netlink", 0, 0, netlink_read_proc, NULL);
#endif
	return 0;
}

static void __exit netlink_proto_exit(void)
{
       sock_unregister(PF_NETLINK);
       remove_proc_entry("net/netlink", NULL);
}

module_init(netlink_proto_init);
module_exit(netlink_proto_exit);
