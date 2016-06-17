/* net/atm/signaling.c - ATM signaling */

/* Written 1995-2000 by Werner Almesberger, EPFL LRC/ICA */


#include <linux/errno.h>	/* error codes */
#include <linux/kernel.h>	/* printk */
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/sched.h>	/* jiffies and HZ */
#include <linux/atm.h>		/* ATM stuff */
#include <linux/atmsap.h>
#include <linux/atmsvc.h>
#include <linux/atmdev.h>
#include <linux/bitops.h>

#include "resources.h"
#include "signaling.h"


#undef WAIT_FOR_DEMON		/* #define this if system calls on SVC sockets
				   should block until the demon runs.
				   Danger: may cause nasty hangs if the demon
				   crashes. */

#if 0
#define DPRINTK(format,args...) printk(KERN_DEBUG format,##args)
#else
#define DPRINTK(format,args...)
#endif


struct atm_vcc *sigd = NULL;
static DECLARE_WAIT_QUEUE_HEAD(sigd_sleep);


static void sigd_put_skb(struct sk_buff *skb)
{
#ifdef WAIT_FOR_DEMON
	static unsigned long silence = 0;
	DECLARE_WAITQUEUE(wait,current);

	add_wait_queue(&sigd_sleep,&wait);
	while (!sigd) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		if (time_after(jiffies, silence) || silence == 0) {
			printk(KERN_INFO "atmsvc: waiting for signaling demon "
			    "...\n");
			silence = (jiffies+30*HZ)|1;
		}
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&sigd_sleep,&wait);
#else
	if (!sigd) {
		printk(KERN_WARNING "atmsvc: no signaling demon\n");
		kfree_skb(skb);
		return;
	}
#endif
	atm_force_charge(sigd,skb->truesize);
	skb_queue_tail(&sigd->sk->receive_queue,skb);
	wake_up(&sigd->sleep);
}


static void modify_qos(struct atm_vcc *vcc,struct atmsvc_msg *msg)
{
	struct sk_buff *skb;

	if (test_bit(ATM_VF_RELEASED,&vcc->flags) ||
	    !test_bit(ATM_VF_READY,&vcc->flags))
		return;
	msg->type = as_error;
	if (!vcc->dev->ops->change_qos) msg->reply = -EOPNOTSUPP;
	else {
		/* should lock VCC */
		msg->reply = vcc->dev->ops->change_qos(vcc,&msg->qos,
		    msg->reply);
		if (!msg->reply) msg->type = as_okay;
	}
	/*
	 * Should probably just turn around the old skb. But the, the buffer
	 * space accounting needs to follow the change too. Maybe later.
	 */
	while (!(skb = alloc_skb(sizeof(struct atmsvc_msg),GFP_KERNEL)))
		schedule();
	*(struct atmsvc_msg *) skb_put(skb,sizeof(struct atmsvc_msg)) = *msg;
	sigd_put_skb(skb);
}


static int sigd_send(struct atm_vcc *vcc,struct sk_buff *skb)
{
	struct atmsvc_msg *msg;
	struct atm_vcc *session_vcc;

	msg = (struct atmsvc_msg *) skb->data;
	atomic_sub(skb->truesize, &vcc->sk->wmem_alloc);
	DPRINTK("sigd_send %d (0x%lx)\n",(int) msg->type,
	  (unsigned long) msg->vcc);
	vcc = *(struct atm_vcc **) &msg->vcc;
	switch (msg->type) {
		case as_okay:
			vcc->reply = msg->reply;
			if (!*vcc->local.sas_addr.prv &&
			    !*vcc->local.sas_addr.pub) {
				vcc->local.sas_family = AF_ATMSVC;
				memcpy(vcc->local.sas_addr.prv,
				    msg->local.sas_addr.prv,ATM_ESA_LEN);
				memcpy(vcc->local.sas_addr.pub,
				    msg->local.sas_addr.pub,ATM_E164_LEN+1);
			}
			session_vcc = vcc->session ? vcc->session : vcc;
			if (session_vcc->vpi || session_vcc->vci) break;
			session_vcc->itf = msg->pvc.sap_addr.itf;
			session_vcc->vpi = msg->pvc.sap_addr.vpi;
			session_vcc->vci = msg->pvc.sap_addr.vci;
			if (session_vcc->vpi || session_vcc->vci)
				session_vcc->qos = msg->qos;
			break;
		case as_error:
			clear_bit(ATM_VF_REGIS,&vcc->flags);
			clear_bit(ATM_VF_READY,&vcc->flags);
			vcc->reply = msg->reply;
			vcc->sk->err = -msg->reply;
			break;
		case as_indicate:
			vcc = *(struct atm_vcc **) &msg->listen_vcc;
			DPRINTK("as_indicate!!!\n");
			lock_sock(vcc->sk);
			if (vcc->sk->ack_backlog == vcc->sk->max_ack_backlog) {
				sigd_enq(0,as_reject,vcc,NULL,NULL);
				goto as_indicate_complete;
			}
			vcc->sk->ack_backlog++;
			skb_queue_tail(&vcc->sk->receive_queue,skb);
			if (vcc->callback) {
				DPRINTK("waking vcc->sleep 0x%p\n",
				    &vcc->sleep);
				vcc->callback(vcc);
			}
as_indicate_complete:
			release_sock(vcc->sk);
			return 0;
		case as_close:
			set_bit(ATM_VF_RELEASED,&vcc->flags);
			clear_bit(ATM_VF_READY,&vcc->flags);
			vcc->reply = msg->reply;
			vcc->sk->err = -msg->reply;
			break;
		case as_modify:
			modify_qos(vcc,msg);
			break;
		default:
			printk(KERN_ALERT "sigd_send: bad message type %d\n",
			    (int) msg->type);
			return -EINVAL;
	}
	if (vcc->callback) vcc->callback(vcc);
	dev_kfree_skb(skb);
	return 0;
}


void sigd_enq2(struct atm_vcc *vcc,enum atmsvc_msg_type type,
    struct atm_vcc *listen_vcc,const struct sockaddr_atmpvc *pvc,
    const struct sockaddr_atmsvc *svc,const struct atm_qos *qos,int reply)
{
	struct sk_buff *skb;
	struct atmsvc_msg *msg;

	DPRINTK("sigd_enq %d (0x%p)\n",(int) type,vcc);
	while (!(skb = alloc_skb(sizeof(struct atmsvc_msg),GFP_KERNEL)))
		schedule();
	msg = (struct atmsvc_msg *) skb_put(skb,sizeof(struct atmsvc_msg));
	memset(msg,0,sizeof(*msg));
	msg->type = type;
	*(struct atm_vcc **) &msg->vcc = vcc;
	*(struct atm_vcc **) &msg->listen_vcc = listen_vcc;
	msg->reply = reply;
	if (qos) msg->qos = *qos;
	if (vcc) msg->sap = vcc->sap;
	if (svc) msg->svc = *svc;
	if (vcc) msg->local = vcc->local;
	if (pvc) msg->pvc = *pvc;
	sigd_put_skb(skb);
	if (vcc) set_bit(ATM_VF_REGIS,&vcc->flags);
}


void sigd_enq(struct atm_vcc *vcc,enum atmsvc_msg_type type,
    struct atm_vcc *listen_vcc,const struct sockaddr_atmpvc *pvc,
    const struct sockaddr_atmsvc *svc)
{
	sigd_enq2(vcc,type,listen_vcc,pvc,svc,vcc ? &vcc->qos : NULL,0);
	/* other ISP applications may use "reply" */
}


static void purge_vcc(struct atm_vcc *vcc)
{
	if (vcc->sk->family == PF_ATMSVC &&
	    !test_bit(ATM_VF_META, &vcc->flags)) {
		set_bit(ATM_VF_RELEASED, &vcc->flags);
		vcc->reply = -EUNATCH;
		vcc->sk->err = EUNATCH;
		wake_up(&vcc->sleep);
	}
}


static void sigd_close(struct atm_vcc *vcc)
{
	struct sock *s;

	DPRINTK("sigd_close\n");
	sigd = NULL;
	if (skb_peek(&vcc->sk->receive_queue))
		printk(KERN_ERR "sigd_close: closing with requests pending\n");
	skb_queue_purge(&vcc->sk->receive_queue);

	read_lock(&vcc_sklist_lock);
	for(s = vcc_sklist; s; s = s->next) {
		struct atm_vcc *vcc = s->protinfo.af_atm;

		if (vcc->dev)
			purge_vcc(vcc);
	}
	read_unlock(&vcc_sklist_lock);
}


static struct atmdev_ops sigd_dev_ops = {
	.close =	sigd_close,
	.send =		sigd_send
};


static struct atm_dev sigd_dev = {
	.ops =		&sigd_dev_ops,
	.type =		"sig",
	.number =	999,
	.lock =		SPIN_LOCK_UNLOCKED
};


int sigd_attach(struct atm_vcc *vcc)
{
	if (sigd) return -EADDRINUSE;
	DPRINTK("sigd_attach\n");
	sigd = vcc;
	vcc->dev = &sigd_dev;
	vcc_insert_socket(vcc->sk);
	set_bit(ATM_VF_META,&vcc->flags);
	set_bit(ATM_VF_READY,&vcc->flags);
	wake_up(&sigd_sleep);
	return 0;
}
