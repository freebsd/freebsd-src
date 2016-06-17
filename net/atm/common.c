/* net/atm/common.c - ATM sockets (common part for PVC and SVC) */

/* Written 1995-2000 by Werner Almesberger, EPFL LRC/ICA */


#include <linux/config.h>
#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/net.h>		/* struct socket, struct net_proto, struct
				   proto_ops */
#include <linux/atm.h>		/* ATM stuff */
#include <linux/atmdev.h>
#include <linux/atmclip.h>	/* CLIP_*ENCAP */
#include <linux/atmarp.h>	/* manifest constants */
#include <linux/sonet.h>	/* for ioctls */
#include <linux/socket.h>	/* SOL_SOCKET */
#include <linux/errno.h>	/* error codes */
#include <linux/capability.h>
#include <linux/mm.h>		/* verify_area */
#include <linux/sched.h>
#include <linux/time.h>		/* struct timeval */
#include <linux/skbuff.h>
#include <linux/bitops.h>
#include <linux/init.h>
#include <net/sock.h>		/* struct sock */

#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/poll.h>
#include <asm/ioctls.h>

#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
#include <linux/atmlec.h>
#include "lec.h"
#include "lec_arpc.h"
struct atm_lane_ops *atm_lane_ops;
static DECLARE_MUTEX(atm_lane_ops_mutex);

void atm_lane_ops_set(struct atm_lane_ops *hook)
{
	down(&atm_lane_ops_mutex);
	atm_lane_ops = hook;
	up(&atm_lane_ops_mutex);
}

int try_atm_lane_ops(void)
{
	down(&atm_lane_ops_mutex);
	if (atm_lane_ops && try_inc_mod_count(atm_lane_ops->owner)) {
		up(&atm_lane_ops_mutex);
		return 1;
	}
	up(&atm_lane_ops_mutex);
	return 0;
}

#if defined(CONFIG_ATM_LANE_MODULE) || defined(CONFIG_ATM_MPOA_MODULE)
EXPORT_SYMBOL(atm_lane_ops);
EXPORT_SYMBOL(try_atm_lane_ops);
EXPORT_SYMBOL(atm_lane_ops_set);
#endif
#endif

#if defined(CONFIG_ATM_MPOA) || defined(CONFIG_ATM_MPOA_MODULE)
#include <linux/atmmpc.h>
#include "mpc.h"
struct atm_mpoa_ops *atm_mpoa_ops;
static DECLARE_MUTEX(atm_mpoa_ops_mutex);

void atm_mpoa_ops_set(struct atm_mpoa_ops *hook)
{
	down(&atm_mpoa_ops_mutex);
	atm_mpoa_ops = hook;
	up(&atm_mpoa_ops_mutex);
}

int try_atm_mpoa_ops(void)
{
	down(&atm_mpoa_ops_mutex);
	if (atm_mpoa_ops && try_inc_mod_count(atm_mpoa_ops->owner)) {
		up(&atm_mpoa_ops_mutex);
		return 1;
	}
	up(&atm_mpoa_ops_mutex);
	return 0;
}
#ifdef CONFIG_ATM_MPOA_MODULE
EXPORT_SYMBOL(atm_mpoa_ops);
EXPORT_SYMBOL(try_atm_mpoa_ops);
EXPORT_SYMBOL(atm_mpoa_ops_set);
#endif
#endif

#if defined(CONFIG_ATM_TCP) || defined(CONFIG_ATM_TCP_MODULE)
#include <linux/atm_tcp.h>
#ifdef CONFIG_ATM_TCP_MODULE
struct atm_tcp_ops atm_tcp_ops;
EXPORT_SYMBOL(atm_tcp_ops);
#endif
#endif

#if defined(CONFIG_ATM_CLIP) || defined(CONFIG_ATM_CLIP_MODULE)
#include <net/atmclip.h>
struct atm_clip_ops *atm_clip_ops;
static DECLARE_MUTEX(atm_clip_ops_mutex);

void atm_clip_ops_set(struct atm_clip_ops *hook)
{
	down(&atm_clip_ops_mutex);
	atm_clip_ops = hook;
	up(&atm_clip_ops_mutex);
}

int try_atm_clip_ops(void)
{
	down(&atm_clip_ops_mutex);
	if (atm_clip_ops && try_inc_mod_count(atm_clip_ops->owner)) {
		up(&atm_clip_ops_mutex);
		return 1;
	}
	up(&atm_clip_ops_mutex);
	return 0;
}

#ifdef CONFIG_ATM_CLIP_MODULE
EXPORT_SYMBOL(atm_clip_ops);
EXPORT_SYMBOL(try_atm_clip_ops);
EXPORT_SYMBOL(atm_clip_ops_set);
#endif
#endif

#if defined(CONFIG_PPPOATM) || defined(CONFIG_PPPOATM_MODULE)
static DECLARE_MUTEX(pppoatm_ioctl_mutex);

static int (*pppoatm_ioctl_hook)(struct atm_vcc *, unsigned int, unsigned long);

void pppoatm_ioctl_set(int (*hook)(struct atm_vcc *, unsigned int, unsigned long))
{
	down(&pppoatm_ioctl_mutex);
	pppoatm_ioctl_hook = hook;
	up(&pppoatm_ioctl_mutex);
}
#ifdef CONFIG_PPPOATM_MODULE
EXPORT_SYMBOL(pppoatm_ioctl_set);
#endif
#endif

#if defined(CONFIG_ATM_BR2684) || defined(CONFIG_ATM_BR2684_MODULE)
static DECLARE_MUTEX(br2684_ioctl_mutex);

static int (*br2684_ioctl_hook)(struct atm_vcc *, unsigned int, unsigned long);
 
void br2684_ioctl_set(int (*hook)(struct atm_vcc *, unsigned int, unsigned long))
{
	down(&br2684_ioctl_mutex);
	br2684_ioctl_hook = hook;
	up(&br2684_ioctl_mutex);
}
#ifdef CONFIG_ATM_BR2684_MODULE
EXPORT_SYMBOL(br2684_ioctl_set);
#endif
#endif

#include "resources.h"		/* atm_find_dev */
#include "common.h"		/* prototypes */
#include "protocols.h"		/* atm_init_<transport> */
#include "addr.h"		/* address registry */
#ifdef CONFIG_ATM_CLIP
#include <net/atmclip.h>	/* for clip_create */
#endif
#include "signaling.h"		/* for WAITING and sigd_attach */


#if 0
#define DPRINTK(format,args...) printk(KERN_DEBUG format,##args)
#else
#define DPRINTK(format,args...)
#endif


struct sock *vcc_sklist;
rwlock_t vcc_sklist_lock = RW_LOCK_UNLOCKED;

void __vcc_insert_socket(struct sock *sk)
{
	sk->next = vcc_sklist;
	if (sk->next)
		vcc_sklist->pprev = &sk->next;
	vcc_sklist = sk;
	sk->pprev = &vcc_sklist;
}

void vcc_insert_socket(struct sock *sk)
{
	write_lock_irq(&vcc_sklist_lock);
	__vcc_insert_socket(sk);
	write_unlock_irq(&vcc_sklist_lock);
}

void vcc_remove_socket(struct sock *sk)
{
	write_lock_irq(&vcc_sklist_lock);
	if (sk->pprev) {
		if (sk->next)
			sk->next->pprev = sk->pprev;
		*sk->pprev = sk->next;
		sk->pprev = NULL;
	}
	write_unlock_irq(&vcc_sklist_lock);
}


static struct sk_buff *alloc_tx(struct atm_vcc *vcc,unsigned int size)
{
	struct sk_buff *skb;

	if (atomic_read(&vcc->sk->wmem_alloc) && !atm_may_send(vcc,size)) {
		DPRINTK("Sorry: wmem_alloc = %d, size = %d, sndbuf = %d\n",
		    atomic_read(&vcc->sk->wmem_alloc),size,vcc->sk->sndbuf);
		return NULL;
	}
	while (!(skb = alloc_skb(size,GFP_KERNEL))) schedule();
	DPRINTK("AlTx %d += %d\n",atomic_read(&vcc->sk->wmem_alloc),skb->truesize);
	atomic_add(skb->truesize, &vcc->sk->wmem_alloc);
	return skb;
}


EXPORT_SYMBOL(vcc_sklist);
EXPORT_SYMBOL(vcc_sklist_lock);
EXPORT_SYMBOL(vcc_insert_socket);
EXPORT_SYMBOL(vcc_remove_socket);

static void vcc_sock_destruct(struct sock *sk)
{
	struct atm_vcc *vcc = sk->protinfo.af_atm;

	if (atomic_read(&vcc->sk->rmem_alloc))
		printk(KERN_DEBUG "vcc_sock_destruct: rmem leakage (%d bytes) detected.\n", atomic_read(&sk->rmem_alloc));

	if (atomic_read(&vcc->sk->wmem_alloc))
		printk(KERN_DEBUG "vcc_sock_destruct: wmem leakage (%d bytes) detected.\n", atomic_read(&sk->wmem_alloc));

	kfree(sk->protinfo.af_atm);

	MOD_DEC_USE_COUNT;
}

int vcc_create(struct socket *sock, int protocol, int family)
{     
	struct sock *sk;
	struct atm_vcc *vcc;

	sock->sk = NULL;
	if (sock->type == SOCK_STREAM)
		return -EINVAL;
	sk = sk_alloc(family, GFP_KERNEL, 1);
	if (!sk)
		return -ENOMEM;
	sock_init_data(NULL, sk);

	vcc = sk->protinfo.af_atm = kmalloc(sizeof(*vcc), GFP_KERNEL);
	if (!vcc) {
		sk_free(sk);
		return -ENOMEM;
	}

	memset(vcc, 0, sizeof(*vcc));
	vcc->sk = sk;

	vcc->dev = NULL;
	vcc->callback = NULL;
	memset(&vcc->local,0,sizeof(struct sockaddr_atmsvc));
	memset(&vcc->remote,0,sizeof(struct sockaddr_atmsvc));
	vcc->qos.txtp.max_sdu = 1 << 16; /* for meta VCs */
	atomic_set(&vcc->sk->wmem_alloc,0);
	atomic_set(&vcc->sk->rmem_alloc,0);
	vcc->push = NULL;
	vcc->pop = NULL;
	vcc->push_oam = NULL;
	vcc->vpi = vcc->vci = 0; /* no VCI/VPI yet */
	vcc->atm_options = vcc->aal_options = 0;
	init_waitqueue_head(&vcc->sleep);
	sk->sleep = &vcc->sleep;
	sk->destruct = vcc_sock_destruct;
	sock->sk = sk;

	MOD_INC_USE_COUNT;

	return 0;
}


static void vcc_destroy_socket(struct sock *sk)
{
	struct atm_vcc *vcc;
	struct sk_buff *skb;

	vcc = sk->protinfo.af_atm;
	clear_bit(ATM_VF_READY, &vcc->flags);
	if (vcc->dev) {
		if (vcc->dev->ops->close)
			vcc->dev->ops->close(vcc);
		if (vcc->push)
			vcc->push(vcc, NULL); /* atmarpd has no push */

		vcc_remove_socket(sk);  /* no more receive */

		while ((skb = skb_dequeue(&vcc->sk->receive_queue))) {
			atm_return(vcc,skb->truesize);
			kfree_skb(skb);
		}

		if (vcc->dev->ops->owner)
			__MOD_DEC_USE_COUNT(vcc->dev->ops->owner);
		atm_dev_put(vcc->dev);
	}
}


int vcc_release(struct socket *sock)
{
	struct sock *sk = sock->sk;

	if (sk) {
		lock_sock(sk);
		vcc_destroy_socket(sock->sk);
		release_sock(sk);
		sock_put(sk);
	}

	return 0;
}


void vcc_release_async(struct atm_vcc *vcc, int reply)
{
	set_bit(ATM_VF_CLOSE, &vcc->flags);
	vcc->reply = reply;
	vcc->sk->err = -reply;
	wake_up(&vcc->sleep);
}


EXPORT_SYMBOL(vcc_release_async);


static int adjust_tp(struct atm_trafprm *tp,unsigned char aal)
{
	int max_sdu;

	if (!tp->traffic_class) return 0;
	switch (aal) {
		case ATM_AAL0:
			max_sdu = ATM_CELL_SIZE-1;
			break;
		case ATM_AAL34:
			max_sdu = ATM_MAX_AAL34_PDU;
			break;
		default:
			printk(KERN_WARNING "ATM: AAL problems ... "
			    "(%d)\n",aal);
			/* fall through */
		case ATM_AAL5:
			max_sdu = ATM_MAX_AAL5_PDU;
	}
	if (!tp->max_sdu) tp->max_sdu = max_sdu;
	else if (tp->max_sdu > max_sdu) return -EINVAL;
	if (!tp->max_cdv) tp->max_cdv = ATM_MAX_CDV;
	return 0;
}


static int __vcc_connect(struct atm_vcc *vcc, struct atm_dev *dev, int vpi,
			 int vci)
{
	int error;

	if ((vpi != ATM_VPI_UNSPEC && vpi != ATM_VPI_ANY &&
	    vpi >> dev->ci_range.vpi_bits) || (vci != ATM_VCI_UNSPEC &&
	    vci != ATM_VCI_ANY && vci >> dev->ci_range.vci_bits))
		return -EINVAL;
	if (vci > 0 && vci < ATM_NOT_RSV_VCI && !capable(CAP_NET_BIND_SERVICE))
		return -EPERM;
	error = 0;
	if (!try_inc_mod_count(dev->ops->owner))
		return -ENODEV;
	vcc->dev = dev;
	vcc_insert_socket(vcc->sk);
	switch (vcc->qos.aal) {
		case ATM_AAL0:
			error = atm_init_aal0(vcc);
			vcc->stats = &dev->stats.aal0;
			break;
		case ATM_AAL34:
			error = atm_init_aal34(vcc);
			vcc->stats = &dev->stats.aal34;
			break;
		case ATM_NO_AAL:
			/* ATM_AAL5 is also used in the "0 for default" case */
			vcc->qos.aal = ATM_AAL5;
			/* fall through */
		case ATM_AAL5:
			error = atm_init_aal5(vcc);
			vcc->stats = &dev->stats.aal5;
			break;
		default:
			error = -EPROTOTYPE;
	}
	if (!error) error = adjust_tp(&vcc->qos.txtp,vcc->qos.aal);
	if (!error) error = adjust_tp(&vcc->qos.rxtp,vcc->qos.aal);
	if (error)
		goto fail;
	DPRINTK("VCC %d.%d, AAL %d\n",vpi,vci,vcc->qos.aal);
	DPRINTK("  TX: %d, PCR %d..%d, SDU %d\n",vcc->qos.txtp.traffic_class,
	    vcc->qos.txtp.min_pcr,vcc->qos.txtp.max_pcr,vcc->qos.txtp.max_sdu);
	DPRINTK("  RX: %d, PCR %d..%d, SDU %d\n",vcc->qos.rxtp.traffic_class,
	    vcc->qos.rxtp.min_pcr,vcc->qos.rxtp.max_pcr,vcc->qos.rxtp.max_sdu);
	if (dev->ops->open) {
		if ((error = dev->ops->open(vcc,vpi,vci)))
			goto fail;
	}
	return 0;

fail:
	vcc_remove_socket(vcc->sk);
	if (dev->ops->owner)
		__MOD_DEC_USE_COUNT(dev->ops->owner);
	/* ensure we get dev module ref count correct */
	vcc->dev = NULL;
	return error;
	
}


int vcc_connect(struct socket *sock, int itf, short vpi, int vci)
{
	struct atm_dev *dev;
	struct atm_vcc *vcc = ATM_SD(sock);
	int error;

	DPRINTK("vcc_connect (vpi %d, vci %d)\n",vpi,vci);
	if (sock->state == SS_CONNECTED)
		return -EISCONN;
	if (sock->state != SS_UNCONNECTED)
		return -EINVAL;
	if (!(vpi || vci))
		return -EINVAL;

	if (vpi != ATM_VPI_UNSPEC && vci != ATM_VCI_UNSPEC)
		clear_bit(ATM_VF_PARTIAL,&vcc->flags);
	else
		if (test_bit(ATM_VF_PARTIAL,&vcc->flags))
			return -EINVAL;
	DPRINTK("vcc_connect (TX: cl %d,bw %d-%d,sdu %d; "
	    "RX: cl %d,bw %d-%d,sdu %d,AAL %s%d)\n",
	    vcc->qos.txtp.traffic_class,vcc->qos.txtp.min_pcr,
	    vcc->qos.txtp.max_pcr,vcc->qos.txtp.max_sdu,
	    vcc->qos.rxtp.traffic_class,vcc->qos.rxtp.min_pcr,
	    vcc->qos.rxtp.max_pcr,vcc->qos.rxtp.max_sdu,
	    vcc->qos.aal == ATM_AAL5 ? "" : vcc->qos.aal == ATM_AAL0 ? "" :
	    " ??? code ",vcc->qos.aal == ATM_AAL0 ? 0 : vcc->qos.aal);
	if (!test_bit(ATM_VF_HASQOS, &vcc->flags))
		return -EBADFD;
	if (vcc->qos.txtp.traffic_class == ATM_ANYCLASS ||
	    vcc->qos.rxtp.traffic_class == ATM_ANYCLASS)
		return -EINVAL;
	if (itf != ATM_ITF_ANY) {
		dev = atm_dev_lookup(itf);
		if (!dev)
			return -ENODEV;
		error = __vcc_connect(vcc, dev, vpi, vci);
		if (error) {
			atm_dev_put(dev);
			return error;
		}
	} else {
		struct list_head *p, *next;

		dev = NULL;
		spin_lock(&atm_dev_lock);
		list_for_each_safe(p, next, &atm_devs) {
			dev = list_entry(p, struct atm_dev, dev_list);
			atm_dev_hold(dev);
			spin_unlock(&atm_dev_lock);
			if (!__vcc_connect(vcc, dev, vpi, vci))
				break;
			atm_dev_put(dev);
			dev = NULL;
			spin_lock(&atm_dev_lock);
		}
		spin_unlock(&atm_dev_lock);
		if (!dev)
			return -ENODEV;
	}
	if (vpi == ATM_VPI_UNSPEC || vci == ATM_VCI_UNSPEC)
		set_bit(ATM_VF_PARTIAL,&vcc->flags);
	if (test_bit(ATM_VF_READY,&ATM_SD(sock)->flags))
		sock->state = SS_CONNECTED;
	return 0;
}


int vcc_recvmsg(struct socket *sock, struct msghdr *msg,
		int size, int flags, struct scm_cookie *scm)
{
 	struct sock *sk = sock->sk;
  	struct atm_vcc *vcc;
  	struct sk_buff *skb;
 	int copied, error = -EINVAL;
 
 	if (sock->state != SS_CONNECTED)
 		return -ENOTCONN;
 	if (flags & ~MSG_DONTWAIT)		/* only handle MSG_DONTWAIT */
 		return -EOPNOTSUPP;
  	vcc = ATM_SD(sock);
 	if (test_bit(ATM_VF_RELEASED,&vcc->flags) ||
 	    test_bit(ATM_VF_CLOSE, &vcc->flags) ||
 	    !test_bit(ATM_VF_READY, &vcc->flags))
 		return 0;
 
 	skb = skb_recv_datagram(sk, flags, flags & MSG_DONTWAIT, &error);
 	if (!skb)
 		return error;
 
 	copied = skb->len; 
 	if (copied > size) {
 		copied = size; 
 		msg->msg_flags |= MSG_TRUNC;
 	}
 
	error = skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);
	if (error)
		 return error;
	sock_recv_timestamp(msg, sk, skb);
	if (vcc->dev->ops->feedback)
		vcc->dev->ops->feedback(vcc, skb, (unsigned long) skb->data,
					(unsigned long) msg->msg_iov->iov_base, copied);
	DPRINTK("RcvM %d -= %d\n", atomic_read(&vcc->sk->rmem_alloc), skb->truesize);
	atm_return(vcc, skb->truesize);
	skb_free_datagram(sk, skb);
	return copied;
}
  
  

int vcc_sendmsg(struct socket *sock, struct msghdr *m, int total_len,
		struct scm_cookie *scm)
{
 	struct sock *sk = sock->sk;
	DECLARE_WAITQUEUE(wait,current);
  	struct atm_vcc *vcc;
  	struct sk_buff *skb;
  	int eff,error;
  	const void *buff;
  	int size;

 	lock_sock(sk);
 	if (sock->state != SS_CONNECTED) {
 		error = -ENOTCONN;
 		goto out;
 	}
 	if (m->msg_name) {
 		error = -EISCONN;
 		goto out;
 	}
 	if (m->msg_iovlen != 1) {
 		error = -ENOSYS; /* fix this later @@@ */
 		goto out;
 	}
  	buff = m->msg_iov->iov_base;
  	size = m->msg_iov->iov_len;
  	vcc = ATM_SD(sock);
 	if (test_bit(ATM_VF_RELEASED, &vcc->flags) ||
 	    test_bit(ATM_VF_CLOSE, &vcc->flags) ||
 	    !test_bit(ATM_VF_READY, &vcc->flags)) {
 		error = -EPIPE;
 		send_sig(SIGPIPE, current, 0);
 		goto out;
 	}
 	if (!size) {
 		error = 0;
 		goto out;
 	}
 	if (size < 0 || size > vcc->qos.txtp.max_sdu) {
 		error = -EMSGSIZE;
 		goto out;
 	}
  	/* verify_area is done by net/socket.c */
  	eff = (size+3) & ~3; /* align to word boundary */
 	add_wait_queue(&vcc->sleep,&wait);
	set_current_state(TASK_INTERRUPTIBLE);
  	error = 0;
  	while (!(skb = alloc_tx(vcc,eff))) {
  		if (m->msg_flags & MSG_DONTWAIT) {
			error = -EAGAIN;
			break;
		}
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
		if (signal_pending(current)) {
			error = -ERESTARTSYS;
			break;
		}
		if (test_bit(ATM_VF_RELEASED,&vcc->flags) ||
		    test_bit(ATM_VF_CLOSE, &vcc->flags) ||
		    !test_bit(ATM_VF_READY, &vcc->flags)) {
			error = -EPIPE;
			send_sig(SIGPIPE, current, 0);
			break;
		}
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&vcc->sleep,&wait);
	if (error)
		goto out;
	skb->dev = NULL; /* for paths shared with net_device interfaces */
	ATM_SKB(skb)->atm_options = vcc->atm_options;
	if (copy_from_user(skb_put(skb,size),buff,size)) {
		kfree_skb(skb);
		error = -EFAULT;
		goto out;
	}
	if (eff != size) memset(skb->data+size,0,eff-size);
	error = vcc->dev->ops->send(vcc,skb);
	error = error ? error : size;
out:
	release_sock(sk);
	return error;
}


unsigned int atm_poll(struct file *file,struct socket *sock,poll_table *wait)
{
	struct atm_vcc *vcc;
	unsigned int mask;

	vcc = ATM_SD(sock);
	poll_wait(file,&vcc->sleep,wait);
	mask = 0;
	if (skb_peek(&vcc->sk->receive_queue))
		mask |= POLLIN | POLLRDNORM;
	if (test_bit(ATM_VF_RELEASED,&vcc->flags) ||
	    test_bit(ATM_VF_CLOSE,&vcc->flags))
		mask |= POLLHUP;
	if (sock->state != SS_CONNECTING) {
		if (vcc->qos.txtp.traffic_class != ATM_NONE &&
		    vcc->qos.txtp.max_sdu+atomic_read(&vcc->sk->wmem_alloc) <= vcc->sk->sndbuf)
			mask |= POLLOUT | POLLWRNORM;
	}
	else if (vcc->reply != WAITING) {
			mask |= POLLOUT | POLLWRNORM;
			if (vcc->reply) mask |= POLLERR;
		}
	return mask;
}


int vcc_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	struct atm_vcc *vcc;
	int error;

	vcc = ATM_SD(sock);
	switch (cmd) {
		case SIOCOUTQ:
			if (sock->state != SS_CONNECTED ||
			    !test_bit(ATM_VF_READY, &vcc->flags)) {
				error =  -EINVAL;
				goto done;
			}
			error =  put_user(vcc->sk->sndbuf-
					  atomic_read(&vcc->sk->wmem_alloc),
					  (int *) arg) ? -EFAULT : 0;
			goto done;
		case SIOCINQ:
			{
				struct sk_buff *skb;

				if (sock->state != SS_CONNECTED) {
					error = -EINVAL;
					goto done;
				}
				skb = skb_peek(&vcc->sk->receive_queue);
				error = put_user(skb ? skb->len : 0,
						 (int *) arg) ? -EFAULT : 0;
				goto done;
			}
		case SIOCGSTAMP: /* borrowed from IP */
			if (!vcc->sk->stamp.tv_sec) {
				error = -ENOENT;
				goto done;
			}
			error = copy_to_user((void *) arg, &vcc->sk->stamp,
					     sizeof(struct timeval)) ? -EFAULT : 0;
			goto done;
		case ATM_SETSC:
			printk(KERN_WARNING "ATM_SETSC is obsolete\n");
			error = 0;
			goto done;
		case ATMSIGD_CTRL:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			/*
			 * The user/kernel protocol for exchanging signalling
			 * info uses kernel pointers as opaque references,
			 * so the holder of the file descriptor can scribble
			 * on the kernel... so we should make sure that we
			 * have the same privledges that /proc/kcore needs
			 */
			if (!capable(CAP_SYS_RAWIO)) {
				error = -EPERM;
				goto done;
			}
			error = sigd_attach(vcc);
			if (!error)
				sock->state = SS_CONNECTED;
			goto done;
#if defined(CONFIG_ATM_CLIP) || defined(CONFIG_ATM_CLIP_MODULE)
		case SIOCMKCLIP:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			if (try_atm_clip_ops()) {
				error = atm_clip_ops->clip_create(arg);
				if (atm_clip_ops->owner)
					__MOD_DEC_USE_COUNT(atm_clip_ops->owner);
			} else
				error = -ENOSYS;
			goto done;
		case ATMARPD_CTRL:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
#if defined(CONFIG_ATM_CLIP_MODULE)
			if (!atm_clip_ops)
				request_module("clip");
#endif
			if (try_atm_clip_ops()) {
				error = atm_clip_ops->atm_init_atmarp(vcc);
				if (atm_clip_ops->owner)
					__MOD_DEC_USE_COUNT(atm_clip_ops->owner);
				if (!error)
					sock->state = SS_CONNECTED;
			} else
				error = -ENOSYS;
			goto done;
		case ATMARP_MKIP:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			if (try_atm_clip_ops()) {
				error = atm_clip_ops->clip_mkip(vcc, arg);
				if (atm_clip_ops->owner)
					__MOD_DEC_USE_COUNT(atm_clip_ops->owner);
			} else
				error = -ENOSYS;
			goto done;
		case ATMARP_SETENTRY:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			if (try_atm_clip_ops()) {
				error = atm_clip_ops->clip_setentry(vcc, arg);
				if (atm_clip_ops->owner)
					__MOD_DEC_USE_COUNT(atm_clip_ops->owner);
			} else
				error = -ENOSYS;
			goto done;
		case ATMARP_ENCAP:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			if (try_atm_clip_ops()) {
				error = atm_clip_ops->clip_encap(vcc, arg);
				if (atm_clip_ops->owner)
					__MOD_DEC_USE_COUNT(atm_clip_ops->owner);
			} else
				error = -ENOSYS;
			goto done;
#endif
#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
                case ATMLEC_CTRL:
                        if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
#if defined(CONFIG_ATM_LANE_MODULE)
                        if (!atm_lane_ops)
				request_module("lec");
#endif
			if (try_atm_lane_ops()) {
				error = atm_lane_ops->lecd_attach(vcc, (int) arg);
				if (atm_lane_ops->owner)
					__MOD_DEC_USE_COUNT(atm_lane_ops->owner);
				if (error >= 0)
					sock->state = SS_CONNECTED;
			} else
				error = -ENOSYS;
			goto done;
                case ATMLEC_MCAST:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			if (try_atm_lane_ops()) {
				error = atm_lane_ops->mcast_attach(vcc, (int) arg);
				if (atm_lane_ops->owner)
					__MOD_DEC_USE_COUNT(atm_lane_ops->owner);
			} else
				error = -ENOSYS;
			goto done;
                case ATMLEC_DATA:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			if (try_atm_lane_ops()) {
				error = atm_lane_ops->vcc_attach(vcc, (void *) arg);
				if (atm_lane_ops->owner)
					__MOD_DEC_USE_COUNT(atm_lane_ops->owner);
			} else
				error = -ENOSYS;
			goto done;
#endif
#if defined(CONFIG_ATM_MPOA) || defined(CONFIG_ATM_MPOA_MODULE)
		case ATMMPC_CTRL:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
#if defined(CONFIG_ATM_MPOA_MODULE)
			if (!atm_mpoa_ops)
                                request_module("mpoa");
#endif
			if (try_atm_mpoa_ops()) {
				error = atm_mpoa_ops->mpoad_attach(vcc, (int) arg);
				if (atm_mpoa_ops->owner)
					__MOD_DEC_USE_COUNT(atm_mpoa_ops->owner);
				if (error >= 0)
					sock->state = SS_CONNECTED;
			} else
				error = -ENOSYS;
			goto done;
		case ATMMPC_DATA:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			if (try_atm_mpoa_ops()) {
				error = atm_mpoa_ops->vcc_attach(vcc, arg);
				if (atm_mpoa_ops->owner)
					__MOD_DEC_USE_COUNT(atm_mpoa_ops->owner);
			} else
				error = -ENOSYS;
			goto done;
#endif
#if defined(CONFIG_ATM_TCP) || defined(CONFIG_ATM_TCP_MODULE)
		case SIOCSIFATMTCP:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			if (!atm_tcp_ops.attach) {
				error = -ENOPKG;
				goto done;
			}
			fops_get(&atm_tcp_ops);
			error = atm_tcp_ops.attach(vcc, (int) arg);
			if (error >= 0)
				sock->state = SS_CONNECTED;
			else
				fops_put(&atm_tcp_ops);
			goto done;
		case ATMTCP_CREATE:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			if (!atm_tcp_ops.create_persistent) {
				error = -ENOPKG;
				goto done;
			}
			error = atm_tcp_ops.create_persistent((int) arg);
			if (error < 0)
				fops_put(&atm_tcp_ops);
			goto done;
		case ATMTCP_REMOVE:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			if (!atm_tcp_ops.remove_persistent) {
				error = -ENOPKG;
				goto done;
			}
			error = atm_tcp_ops.remove_persistent((int) arg);
			fops_put(&atm_tcp_ops);
			goto done;
#endif
		default:
			break;
	}
	error = -ENOIOCTLCMD;
#if defined(CONFIG_PPPOATM) || defined(CONFIG_PPPOATM_MODULE)
	down(&pppoatm_ioctl_mutex);
	if (pppoatm_ioctl_hook)
		error = pppoatm_ioctl_hook(vcc, cmd, arg);
	up(&pppoatm_ioctl_mutex);
	if (error != -ENOIOCTLCMD)
		goto done;
#endif
#if defined(CONFIG_ATM_BR2684) || defined(CONFIG_ATM_BR2684_MODULE)
	down(&br2684_ioctl_mutex);
	if (br2684_ioctl_hook)
		error = br2684_ioctl_hook(vcc, cmd, arg);
	up(&br2684_ioctl_mutex);
	if (error != -ENOIOCTLCMD)
		goto done;
#endif

	error = atm_dev_ioctl(cmd, arg);

done:
	return error;
}


static int atm_change_qos(struct atm_vcc *vcc,struct atm_qos *qos)
{
	int error;

	/*
	 * Don't let the QoS change the already connected AAL type nor the
	 * traffic class.
	 */
	if (qos->aal != vcc->qos.aal ||
	    qos->rxtp.traffic_class != vcc->qos.rxtp.traffic_class ||
	    qos->txtp.traffic_class != vcc->qos.txtp.traffic_class)
		return -EINVAL;
	error = adjust_tp(&qos->txtp,qos->aal);
	if (!error) error = adjust_tp(&qos->rxtp,qos->aal);
	if (error) return error;
	if (!vcc->dev->ops->change_qos) return -EOPNOTSUPP;
	if (vcc->sk->family == AF_ATMPVC)
		return vcc->dev->ops->change_qos(vcc,qos,ATM_MF_SET);
	return svc_change_qos(vcc,qos);
}


static int check_tp(struct atm_trafprm *tp)
{
	/* @@@ Should be merged with adjust_tp */
	if (!tp->traffic_class || tp->traffic_class == ATM_ANYCLASS) return 0;
	if (tp->traffic_class != ATM_UBR && !tp->min_pcr && !tp->pcr &&
	    !tp->max_pcr) return -EINVAL;
	if (tp->min_pcr == ATM_MAX_PCR) return -EINVAL;
	if (tp->min_pcr && tp->max_pcr && tp->max_pcr != ATM_MAX_PCR &&
	    tp->min_pcr > tp->max_pcr) return -EINVAL;
	/*
	 * We allow pcr to be outside [min_pcr,max_pcr], because later
	 * adjustment may still push it in the valid range.
	 */
	return 0;
}


static int check_qos(struct atm_qos *qos)
{
	int error;

	if (!qos->txtp.traffic_class && !qos->rxtp.traffic_class)
                return -EINVAL;
	if (qos->txtp.traffic_class != qos->rxtp.traffic_class &&
	    qos->txtp.traffic_class && qos->rxtp.traffic_class &&
	    qos->txtp.traffic_class != ATM_ANYCLASS &&
	    qos->rxtp.traffic_class != ATM_ANYCLASS) return -EINVAL;
	error = check_tp(&qos->txtp);
	if (error) return error;
	return check_tp(&qos->rxtp);
}

int vcc_setsockopt(struct socket *sock, int level, int optname,
		   char *optval, int optlen)
{
	struct atm_vcc *vcc;
	unsigned long value;
	int error;

	if (__SO_LEVEL_MATCH(optname, level) && optlen != __SO_SIZE(optname))
		return -EINVAL;

	vcc = ATM_SD(sock);
	switch (optname) {
		case SO_ATMQOS:
			{
				struct atm_qos qos;

				if (copy_from_user(&qos,optval,sizeof(qos)))
					return -EFAULT;
				error = check_qos(&qos);
				if (error) return error;
				if (sock->state == SS_CONNECTED)
					return atm_change_qos(vcc,&qos);
				if (sock->state != SS_UNCONNECTED)
					return -EBADFD;
				vcc->qos = qos;
				set_bit(ATM_VF_HASQOS,&vcc->flags);
				return 0;
			}
		case SO_SETCLP:
			if (get_user(value,(unsigned long *) optval))
				return -EFAULT;
			if (value) vcc->atm_options |= ATM_ATMOPT_CLP;
			else vcc->atm_options &= ~ATM_ATMOPT_CLP;
			return 0;
		default:
			if (level == SOL_SOCKET) return -EINVAL;
			break;
	}
	if (!vcc->dev || !vcc->dev->ops->setsockopt) return -EINVAL;
	return vcc->dev->ops->setsockopt(vcc,level,optname,optval,optlen);
}


int vcc_getsockopt(struct socket *sock, int level, int optname,
		   char *optval, int *optlen)
{
	struct atm_vcc *vcc;
	int len;

	if (get_user(len, optlen))
		return -EFAULT;
	if (__SO_LEVEL_MATCH(optname, level) && len != __SO_SIZE(optname))
		return -EINVAL;

	vcc = ATM_SD(sock);
	switch (optname) {
		case SO_ATMQOS:
			if (!test_bit(ATM_VF_HASQOS,&vcc->flags))
				return -EINVAL;
			return copy_to_user(optval,&vcc->qos,sizeof(vcc->qos)) ?
			    -EFAULT : 0;
		case SO_SETCLP:
			return put_user(vcc->atm_options & ATM_ATMOPT_CLP ? 1 :
			  0,(unsigned long *) optval) ? -EFAULT : 0;
		case SO_ATMPVC:
			{
				struct sockaddr_atmpvc pvc;

				if (!vcc->dev ||
				    !test_bit(ATM_VF_ADDR,&vcc->flags))
					return -ENOTCONN;
				pvc.sap_family = AF_ATMPVC;
				pvc.sap_addr.itf = vcc->dev->number;
				pvc.sap_addr.vpi = vcc->vpi;
				pvc.sap_addr.vci = vcc->vci;
				return copy_to_user(optval,&pvc,sizeof(pvc)) ?
				    -EFAULT : 0;
			}
		default:
			if (level == SOL_SOCKET) return -EINVAL;
			break;
	}
	if (!vcc->dev || !vcc->dev->ops->getsockopt) return -EINVAL;
	return vcc->dev->ops->getsockopt(vcc, level, optname, optval, len);
}


#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
struct net_bridge_fdb_entry *(*br_fdb_get_hook)(struct net_bridge *br,
						unsigned char *addr) = NULL;
void (*br_fdb_put_hook)(struct net_bridge_fdb_entry *ent) = NULL;
#if defined(CONFIG_ATM_LANE_MODULE) || defined(CONFIG_BRIDGE_MODULE)
EXPORT_SYMBOL(br_fdb_get_hook);
EXPORT_SYMBOL(br_fdb_put_hook);
#endif /* defined(CONFIG_ATM_LANE_MODULE) || defined(CONFIG_BRIDGE_MODULE) */
#endif /* defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE) */
#endif /* defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE) */


static int __init atm_init(void)
{
	int error;

	if ((error = atmpvc_init()) < 0) {
		printk(KERN_ERR "atmpvc_init() failed with %d\n", error);
		goto failure;
	}
	if ((error = atmsvc_init()) < 0) {
		printk(KERN_ERR "atmsvc_init() failed with %d\n", error);
		goto failure;
	}
#ifdef CONFIG_PROC_FS
        if ((error = atm_proc_init()) < 0) {
		printk(KERN_ERR "atm_proc_init() failed with %d\n",error);
		goto failure;
	}
#endif
	return 0;

failure:
	atmsvc_exit();
	atmpvc_exit();
	return error;
}

static void __exit atm_exit(void)
{
#ifdef CONFIG_PROC_FS
	atm_proc_exit();
#endif
	atmsvc_exit();
	atmpvc_exit();
}

module_init(atm_init);
module_exit(atm_exit);

MODULE_LICENSE("GPL");
