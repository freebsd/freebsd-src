/* net/atm/svc.c - ATM SVC sockets */

/* Written 1995-2000 by Werner Almesberger, EPFL LRC/ICA */


#include <linux/string.h>
#include <linux/net.h>		/* struct socket, struct net_proto,
				   struct proto_ops */
#include <linux/errno.h>	/* error codes */
#include <linux/kernel.h>	/* printk */
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/sched.h>	/* jiffies and HZ */
#include <linux/fcntl.h>	/* O_NONBLOCK */
#include <linux/init.h>
#include <linux/atm.h>		/* ATM stuff */
#include <linux/atmsap.h>
#include <linux/atmsvc.h>
#include <linux/atmdev.h>
#include <linux/bitops.h>
#include <net/sock.h>		/* for sock_no_* */
#include <asm/uaccess.h>

#include "resources.h"
#include "common.h"		/* common for PVCs and SVCs */
#include "signaling.h"
#include "addr.h"


#if 0
#define DPRINTK(format,args...) printk(KERN_DEBUG format,##args)
#else
#define DPRINTK(format,args...)
#endif


static int svc_create(struct socket *sock,int protocol);


/*
 * Note: since all this is still nicely synchronized with the signaling demon,
 *       there's no need to protect sleep loops with clis. If signaling is
 *       moved into the kernel, that would change.
 */


void svc_callback(struct atm_vcc *vcc)
{
	wake_up(&vcc->sleep);
}




static int svc_shutdown(struct socket *sock,int how)
{
	return 0;
}


static void svc_disconnect(struct atm_vcc *vcc)
{
	DECLARE_WAITQUEUE(wait,current);
	struct sk_buff *skb;

	DPRINTK("svc_disconnect %p\n",vcc);
	if (test_bit(ATM_VF_REGIS,&vcc->flags)) {
		add_wait_queue(&vcc->sleep,&wait);
		sigd_enq(vcc,as_close,NULL,NULL,NULL);
		while (!test_bit(ATM_VF_RELEASED,&vcc->flags) && sigd) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule();
		}
		remove_wait_queue(&vcc->sleep,&wait);
	}
	/* beware - socket is still in use by atmsigd until the last
	   as_indicate has been answered */
	while ((skb = skb_dequeue(&vcc->sk->receive_queue))) {
		DPRINTK("LISTEN REL\n");
		sigd_enq2(NULL,as_reject,vcc,NULL,NULL,&vcc->qos,0);
		dev_kfree_skb(skb);
	}
	clear_bit(ATM_VF_REGIS,&vcc->flags);
	clear_bit(ATM_VF_RELEASED,&vcc->flags);
	clear_bit(ATM_VF_CLOSE,&vcc->flags);
	/* ... may retry later */
}


static int svc_release(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct atm_vcc *vcc;

	if (sk)  {
		vcc = ATM_SD(sock);
		DPRINTK("svc_release %p\n", vcc);
		clear_bit(ATM_VF_READY, &vcc->flags);
		/* VCC pointer is used as a reference, so we must not free it
		   (thereby subjecting it to re-use) before all pending connections
	           are closed */
		sock_hold(sk);
		vcc_release(sock);
		svc_disconnect(vcc);
		sock_put(sk);
	}
	return 0;
}


static int svc_bind(struct socket *sock,struct sockaddr *sockaddr,
    int sockaddr_len)
{
	DECLARE_WAITQUEUE(wait,current);
	struct sock *sk = sock->sk;
	struct sockaddr_atmsvc *addr;
	struct atm_vcc *vcc;
	int error;

	if (sockaddr_len != sizeof(struct sockaddr_atmsvc))
		return -EINVAL;
	lock_sock(sk);
	if (sock->state == SS_CONNECTED) {
		error = -EISCONN;
		goto out;
	}
	if (sock->state != SS_UNCONNECTED) {
		error = -EINVAL;
		goto out;
	}
	vcc = ATM_SD(sock);
	if (test_bit(ATM_VF_SESSION, &vcc->flags)) {
		error = -EINVAL;
		goto out;
	}
	addr = (struct sockaddr_atmsvc *) sockaddr;
	if (addr->sas_family != AF_ATMSVC) {
		error = -EAFNOSUPPORT;
		goto out;
	}
	clear_bit(ATM_VF_BOUND,&vcc->flags);
	    /* failing rebind will kill old binding */
	/* @@@ check memory (de)allocation on rebind */
	if (!test_bit(ATM_VF_HASQOS,&vcc->flags)) {
		error = -EBADFD;
		goto out;
	}
	vcc->local = *addr;
	vcc->reply = WAITING;
	add_wait_queue(&vcc->sleep,&wait);
	sigd_enq(vcc,as_bind,NULL,NULL,&vcc->local);
	while (vcc->reply == WAITING && sigd) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule();
	}
	remove_wait_queue(&vcc->sleep,&wait);
	clear_bit(ATM_VF_REGIS,&vcc->flags); /* doesn't count */
	if (!sigd) {
		error = -EUNATCH;
		goto out;
	}
        if (!vcc->reply)
		set_bit(ATM_VF_BOUND,&vcc->flags);
	error = vcc->reply;
out:
	release_sock(sk);
	return error;
}


static int svc_connect(struct socket *sock,struct sockaddr *sockaddr,
    int sockaddr_len,int flags)
{
	DECLARE_WAITQUEUE(wait,current);
	struct sock *sk = sock->sk;
	struct sockaddr_atmsvc *addr;
	struct atm_vcc *vcc = ATM_SD(sock);
	int error;

	DPRINTK("svc_connect %p\n",vcc);
	lock_sock(sk);
	if (sockaddr_len != sizeof(struct sockaddr_atmsvc)) {
		error = -EINVAL;
		goto out;
	}

	switch (sock->state) {
	default:
		error = -EINVAL;
		goto out;
	case SS_CONNECTED:
		error = -EISCONN;
		goto out;
	case SS_CONNECTING:
		if (vcc->reply == WAITING) {
			error = -EALREADY;
			goto out;
		}
		sock->state = SS_UNCONNECTED;
		if (vcc->reply) {
			error = vcc->reply;
			goto out;
		}
		break;
	case SS_UNCONNECTED:
		if (test_bit(ATM_VF_SESSION, &vcc->flags)) {
			error = -EINVAL;
			goto out;
		}
		addr = (struct sockaddr_atmsvc *) sockaddr;
		if (addr->sas_family != AF_ATMSVC) {
			error = -EAFNOSUPPORT;
			goto out;
		}
		if (!test_bit(ATM_VF_HASQOS, &vcc->flags)) {
			error = -EBADFD;
			goto out;
		}
		if (vcc->qos.txtp.traffic_class == ATM_ANYCLASS ||
		    vcc->qos.rxtp.traffic_class == ATM_ANYCLASS) {
			error = -EINVAL;
			goto out;
		}
		if (!vcc->qos.txtp.traffic_class &&
		    !vcc->qos.rxtp.traffic_class) {
			error = -EINVAL;
			goto out;
		}
		vcc->remote = *addr;
		vcc->reply = WAITING;
		add_wait_queue(&vcc->sleep,&wait);
		sigd_enq(vcc,as_connect,NULL,NULL,&vcc->remote);
		if (flags & O_NONBLOCK) {
			remove_wait_queue(&vcc->sleep,&wait);
			sock->state = SS_CONNECTING;
			error = -EINPROGRESS;
			goto out;
		}
		error = 0;
		while (vcc->reply == WAITING && sigd) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			if (!signal_pending(current)) continue;
			DPRINTK("*ABORT*\n");
			/*
			 * This is tricky:
			 *   Kernel ---close--> Demon
			 *   Kernel <--close--- Demon
		         * or
			 *   Kernel ---close--> Demon
			 *   Kernel <--error--- Demon
			 * or
			 *   Kernel ---close--> Demon
			 *   Kernel <--okay---- Demon
			 *   Kernel <--close--- Demon
			 */
			sigd_enq(vcc,as_close,NULL,NULL,NULL);
			while (vcc->reply == WAITING && sigd) {
				set_current_state(TASK_UNINTERRUPTIBLE);
				schedule();
			}
			if (!vcc->reply)
				while (!test_bit(ATM_VF_RELEASED,&vcc->flags)
				    && sigd) {
					set_current_state(TASK_UNINTERRUPTIBLE);
					schedule();
				}
			clear_bit(ATM_VF_REGIS,&vcc->flags);
			clear_bit(ATM_VF_RELEASED,&vcc->flags);
			clear_bit(ATM_VF_CLOSE,&vcc->flags);
			    /* we're gone now but may connect later */
			error = -EINTR;
			break;
		}
		remove_wait_queue(&vcc->sleep,&wait);
		if (error)
			goto out;
		if (!sigd) {
			error = -EUNATCH;
			goto out;
		}
		if (vcc->reply) {
			error = vcc->reply;
			goto out;
		}
	}
/*
 * Not supported yet
 *
 * #ifndef CONFIG_SINGLE_SIGITF
 */
	vcc->qos.txtp.max_pcr = SELECT_TOP_PCR(vcc->qos.txtp);
	vcc->qos.txtp.pcr = 0;
	vcc->qos.txtp.min_pcr = 0;
/*
 * #endif
 */
	if (!(error = vcc_connect(sock, vcc->itf, vcc->vpi, vcc->vci)))
		sock->state = SS_CONNECTED;
	else (void) svc_disconnect(vcc);
out:
	release_sock(sk);
	return error;
}


static int svc_listen(struct socket *sock,int backlog)
{
	DECLARE_WAITQUEUE(wait,current);
	struct sock *sk = sock->sk;
	struct atm_vcc *vcc = ATM_SD(sock);
	int error;

	DPRINTK("svc_listen %p\n",vcc);
	lock_sock(sk);
	/* let server handle listen on unbound sockets */
	if (test_bit(ATM_VF_SESSION,&vcc->flags)) {
		error = -EINVAL;
		goto out;
	}
	vcc->reply = WAITING;
	add_wait_queue(&vcc->sleep,&wait);
	sigd_enq(vcc,as_listen,NULL,NULL,&vcc->local);
	while (vcc->reply == WAITING && sigd) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule();
	}
	remove_wait_queue(&vcc->sleep,&wait);
	if (!sigd) {
		error = -EUNATCH;
		goto out;
	}
	set_bit(ATM_VF_LISTEN,&vcc->flags);
	vcc->sk->max_ack_backlog = backlog > 0 ? backlog : ATM_BACKLOG_DEFAULT;
	error = vcc->reply;
out:
	release_sock(sk);
	return error;
}


static int svc_accept(struct socket *sock,struct socket *newsock,int flags)
{
	struct sock *sk = sock->sk;
	struct sk_buff *skb;
	struct atmsvc_msg *msg;
	struct atm_vcc *old_vcc = ATM_SD(sock);
	struct atm_vcc *new_vcc;
	int error;

	lock_sock(sk);

	error = svc_create(newsock,0);
	if (error)
		goto out;

	new_vcc = ATM_SD(newsock);

	DPRINTK("svc_accept %p -> %p\n",old_vcc,new_vcc);
	while (1) {
		DECLARE_WAITQUEUE(wait,current);

		add_wait_queue(&old_vcc->sleep,&wait);
		while (!(skb = skb_dequeue(&old_vcc->sk->receive_queue)) && sigd) {
			if (test_bit(ATM_VF_RELEASED,&old_vcc->flags)) break;
			if (test_bit(ATM_VF_CLOSE,&old_vcc->flags)) {
				error = old_vcc->reply;
				break;
			}
			if (flags & O_NONBLOCK) {
				error = -EAGAIN;
				break;
			}
			release_sock(sk);
			schedule();
			lock_sock(sk);
			if (signal_pending(current)) {
				error = -ERESTARTSYS;
				break;
			}
		}
		remove_wait_queue(&old_vcc->sleep,&wait);
		if (error)
			goto out;
		if (!skb) {
			error = -EUNATCH;
			goto out;
		}
		msg = (struct atmsvc_msg *) skb->data;
		new_vcc->qos = msg->qos;
		set_bit(ATM_VF_HASQOS,&new_vcc->flags);
		new_vcc->remote = msg->svc;
		new_vcc->local = msg->local;
		new_vcc->sap = msg->sap;
		error = vcc_connect(newsock, msg->pvc.sap_addr.itf,
				    msg->pvc.sap_addr.vpi, msg->pvc.sap_addr.vci);
		dev_kfree_skb(skb);
		old_vcc->sk->ack_backlog--;
		if (error) {
			sigd_enq2(NULL,as_reject,old_vcc,NULL,NULL,
			    &old_vcc->qos,error);
			error = error == -EAGAIN ? -EBUSY : error;
			goto out;
		}
		/* wait should be short, so we ignore the non-blocking flag */
		new_vcc->reply = WAITING;
		add_wait_queue(&new_vcc->sleep,&wait);
		sigd_enq(new_vcc,as_accept,old_vcc,NULL,NULL);
		while (new_vcc->reply == WAITING && sigd) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			release_sock(sk);
			schedule();
			lock_sock(sk);
		}
		remove_wait_queue(&new_vcc->sleep,&wait);
		if (!sigd) {
			error = -EUNATCH;
			goto out;
		}
		if (!new_vcc->reply) break;
		if (new_vcc->reply != -ERESTARTSYS) {
			error = new_vcc->reply;
			goto out;
		}
	}
	newsock->state = SS_CONNECTED;
out:
	release_sock(sk);
	return error;
}


static int svc_getname(struct socket *sock,struct sockaddr *sockaddr,
    int *sockaddr_len,int peer)
{
	struct sockaddr_atmsvc *addr;

	*sockaddr_len = sizeof(struct sockaddr_atmsvc);
	addr = (struct sockaddr_atmsvc *) sockaddr;
	memcpy(addr,peer ? &ATM_SD(sock)->remote : &ATM_SD(sock)->local,
	    sizeof(struct sockaddr_atmsvc));
	return 0;
}


int svc_change_qos(struct atm_vcc *vcc,struct atm_qos *qos)
{
	DECLARE_WAITQUEUE(wait,current);

	vcc->reply = WAITING;
	add_wait_queue(&vcc->sleep,&wait);
	sigd_enq2(vcc,as_modify,NULL,NULL,&vcc->local,qos,0);
	while (vcc->reply == WAITING && !test_bit(ATM_VF_RELEASED,&vcc->flags)
	    && sigd) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule();
	}
	remove_wait_queue(&vcc->sleep,&wait);
	if (!sigd) return -EUNATCH;
	return vcc->reply;
}


static int svc_setsockopt(struct socket *sock,int level,int optname,
    char *optval,int optlen)
{
	struct sock *sk = sock->sk;
	struct atm_vcc *vcc;
	int error = 0;

	if (!__SO_LEVEL_MATCH(optname, level) || optname != SO_ATMSAP ||
	    optlen != sizeof(struct atm_sap)) {
		error = vcc_setsockopt(sock, level, optname, optval, optlen);
		goto out;
	}
	vcc = ATM_SD(sock);
	if (copy_from_user(&vcc->sap, optval, optlen)) {
		error = -EFAULT;
		goto out;
	}
	set_bit(ATM_VF_HASSAP, &vcc->flags);
out:
	release_sock(sk);
	return error;
}


static int svc_getsockopt(struct socket *sock,int level,int optname,
    char *optval,int *optlen)
{
	struct sock *sk = sock->sk;
	int error = 0, len;

	lock_sock(sk);
	if (!__SO_LEVEL_MATCH(optname, level) || optname != SO_ATMSAP) {
		error = vcc_getsockopt(sock, level, optname, optval, optlen);
		goto out;
	}
	if (get_user(len, optlen)) {
		error = -EFAULT;
		goto out;
	}
	if (len != sizeof(struct atm_sap)) {
		error = -EINVAL;
		goto out;
	}
	if (copy_to_user(optval, &ATM_SD(sock)->sap, sizeof(struct atm_sap))) {
		error = -EFAULT;
		goto out;
	}
out:
	release_sock(sk);
	return error;
}


static struct proto_ops svc_proto_ops = {
	.family =	PF_ATMSVC,

	.release =	svc_release,
	.bind =		svc_bind,
	.connect =	svc_connect,
	.socketpair =	sock_no_socketpair,
	.accept =	svc_accept,
	.getname =	svc_getname,
	.poll =		atm_poll,
	.ioctl =	vcc_ioctl,
	.listen =	svc_listen,
	.shutdown =	svc_shutdown,
	.setsockopt =	svc_setsockopt,
	.getsockopt =	svc_getsockopt,
	.sendmsg =	vcc_sendmsg,
	.recvmsg =	vcc_recvmsg,
	.mmap =		sock_no_mmap,
	.sendpage =	sock_no_sendpage,
};


static int svc_create(struct socket *sock,int protocol)
{
	int error;

	sock->ops = &svc_proto_ops;
	error = vcc_create(sock, protocol, AF_ATMSVC);
	if (error) return error;
	ATM_SD(sock)->callback = svc_callback;
	ATM_SD(sock)->local.sas_family = AF_ATMSVC;
	ATM_SD(sock)->remote.sas_family = AF_ATMSVC;
	return 0;
}


static struct net_proto_family svc_family_ops = {
	PF_ATMSVC,
	svc_create,
	0,			/* no authentication */
	0,			/* no encryption */
	0			/* no encrypt_net */
};


/*
 *	Initialize the ATM SVC protocol family
 */

int atmsvc_init(void)
{
	return sock_register(&svc_family_ops);
}

void atmsvc_exit(void)
{
	sock_unregister(PF_ATMSVC);
}
