/* drivers/atm/atmtcp.c - ATM over TCP "device" driver */

/* Written 1997-2000 by Werner Almesberger, EPFL LRC/ICA */


#include <linux/module.h>
#include <linux/wait.h>
#include <linux/atmdev.h>
#include <linux/atm_tcp.h>
#include <linux/bitops.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>


extern int atm_init_aal5(struct atm_vcc *vcc); /* "raw" AAL5 transport */


#define PRIV(dev) ((struct atmtcp_dev_data *) ((dev)->dev_data))


struct atmtcp_dev_data {
	struct atm_vcc *vcc;	/* control VCC; NULL if detached */
	int persist;		/* non-zero if persistent */
};


#define DEV_LABEL    "atmtcp"

#define MAX_VPI_BITS  8	/* simplifies life */
#define MAX_VCI_BITS 16


/*
 * Hairy code ahead: the control VCC may be closed while we're still
 * waiting for an answer, so we need to re-validate out_vcc every once
 * in a while.
 */


static int atmtcp_send_control(struct atm_vcc *vcc,int type,
    const struct atmtcp_control *msg,int flag)
{
	DECLARE_WAITQUEUE(wait,current);
	struct atm_vcc *out_vcc;
	struct sk_buff *skb;
	struct atmtcp_control *new_msg;
	int old_test;
	int error = 0;

	out_vcc = PRIV(vcc->dev) ? PRIV(vcc->dev)->vcc : NULL;
	if (!out_vcc) return -EUNATCH;
	skb = alloc_skb(sizeof(*msg),GFP_KERNEL);
	if (!skb) return -ENOMEM;
	mb();
	out_vcc = PRIV(vcc->dev) ? PRIV(vcc->dev)->vcc : NULL;
	if (!out_vcc) {
		dev_kfree_skb(skb);
		return -EUNATCH;
	}
	atm_force_charge(out_vcc,skb->truesize);
	new_msg = (struct atmtcp_control *) skb_put(skb,sizeof(*new_msg));
	*new_msg = *msg;
	new_msg->hdr.length = ATMTCP_HDR_MAGIC;
	new_msg->type = type;
	memset(&new_msg->vcc,0,sizeof(atm_kptr_t));
	*(struct atm_vcc **) &new_msg->vcc = vcc;
	old_test = test_bit(flag,&vcc->flags);
	out_vcc->push(out_vcc,skb);
	add_wait_queue(&vcc->sleep,&wait);
	while (test_bit(flag,&vcc->flags) == old_test) {
		mb();
		out_vcc = PRIV(vcc->dev) ? PRIV(vcc->dev)->vcc : NULL;
		if (!out_vcc) {
			error = -EUNATCH;
			break;
		}
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&vcc->sleep,&wait);
	return error;
}


static int atmtcp_recv_control(const struct atmtcp_control *msg)
{
	struct atm_vcc *vcc = *(struct atm_vcc **) &msg->vcc;

	vcc->vpi = msg->addr.sap_addr.vpi;
	vcc->vci = msg->addr.sap_addr.vci;
	vcc->qos = msg->qos;
	vcc->reply = msg->result;
	switch (msg->type) {
	    case ATMTCP_CTRL_OPEN:
		change_bit(ATM_VF_READY,&vcc->flags);
		break;
	    case ATMTCP_CTRL_CLOSE:
		change_bit(ATM_VF_ADDR,&vcc->flags);
		break;
	    default:
		printk(KERN_ERR "atmtcp_recv_control: unknown type %d\n",
		    msg->type);
		return -EINVAL;
	}
	wake_up(&vcc->sleep);
	return 0;
}


static void atmtcp_v_dev_close(struct atm_dev *dev)
{
	/* Nothing.... Isn't this simple :-)  -- REW */
}


static int atmtcp_v_open(struct atm_vcc *vcc,short vpi,int vci)
{
	struct atmtcp_control msg;
	int error;

	memset(&msg,0,sizeof(msg));
	msg.addr.sap_family = AF_ATMPVC;
	msg.hdr.vpi = htons(vpi);
	msg.addr.sap_addr.vpi = vpi;
	msg.hdr.vci = htons(vci);
	msg.addr.sap_addr.vci = vci;
	error = atm_find_ci(vcc,&msg.addr.sap_addr.vpi,&msg.addr.sap_addr.vci);
	if (error) return error;
	if (vpi == ATM_VPI_UNSPEC || vci == ATM_VCI_UNSPEC) return 0;
	msg.type = ATMTCP_CTRL_OPEN;
	msg.qos = vcc->qos;
	set_bit(ATM_VF_ADDR,&vcc->flags);
	clear_bit(ATM_VF_READY,&vcc->flags); /* just in case ... */
	error = atmtcp_send_control(vcc,ATMTCP_CTRL_OPEN,&msg,ATM_VF_READY);
	if (error) return error;
	return vcc->reply;
}


static void atmtcp_v_close(struct atm_vcc *vcc)
{
	struct atmtcp_control msg;

	memset(&msg,0,sizeof(msg));
	msg.addr.sap_family = AF_ATMPVC;
	msg.addr.sap_addr.vpi = vcc->vpi;
	msg.addr.sap_addr.vci = vcc->vci;
	clear_bit(ATM_VF_READY,&vcc->flags);
	(void) atmtcp_send_control(vcc,ATMTCP_CTRL_CLOSE,&msg,ATM_VF_ADDR);
}


static int atmtcp_v_ioctl(struct atm_dev *dev,unsigned int cmd,void *arg)
{
	struct atm_cirange ci;
	struct atm_vcc *vcc;
	struct sock *s;

	if (cmd != ATM_SETCIRANGE) return -ENOIOCTLCMD;
	if (copy_from_user(&ci,(void *) arg,sizeof(ci))) return -EFAULT;
	if (ci.vpi_bits == ATM_CI_MAX) ci.vpi_bits = MAX_VPI_BITS;
	if (ci.vci_bits == ATM_CI_MAX) ci.vci_bits = MAX_VCI_BITS;
	if (ci.vpi_bits > MAX_VPI_BITS || ci.vpi_bits < 0 ||
	    ci.vci_bits > MAX_VCI_BITS || ci.vci_bits < 0) return -EINVAL;
	read_lock(&vcc_sklist_lock);
	for (s = vcc_sklist; s; s = s->next) {
		vcc = s->protinfo.af_atm;
		if (vcc->dev != dev)
			continue;
		if ((vcc->vpi >> ci.vpi_bits) ||
		    (vcc->vci >> ci.vci_bits)) {
			read_unlock(&vcc_sklist_lock);
			return -EBUSY;
		}
	}
	read_unlock(&vcc_sklist_lock);
	dev->ci_range = ci;
	return 0;
}


static int atmtcp_v_send(struct atm_vcc *vcc,struct sk_buff *skb)
{
	struct atmtcp_dev_data *dev_data;
	struct atm_vcc *out_vcc=NULL; /* Initializer quietens GCC warning */
	struct sk_buff *new_skb;
	struct atmtcp_hdr *hdr;
	int size;

	if (vcc->qos.txtp.traffic_class == ATM_NONE) {
		if (vcc->pop) vcc->pop(vcc,skb);
		else dev_kfree_skb(skb);
		return -EINVAL;
	}
	dev_data = PRIV(vcc->dev);
	if (dev_data) out_vcc = dev_data->vcc;
	if (!dev_data || !out_vcc) {
		if (vcc->pop) vcc->pop(vcc,skb);
		else dev_kfree_skb(skb);
		if (dev_data) return 0;
		atomic_inc(&vcc->stats->tx_err);
		return -ENOLINK;
	}
	size = skb->len+sizeof(struct atmtcp_hdr);
	new_skb = atm_alloc_charge(out_vcc,size,GFP_ATOMIC);
	if (!new_skb) {
		if (vcc->pop) vcc->pop(vcc,skb);
		else dev_kfree_skb(skb);
		atomic_inc(&vcc->stats->tx_err);
		return -ENOBUFS;
	}
	hdr = (void *) skb_put(new_skb,sizeof(struct atmtcp_hdr));
	hdr->vpi = htons(vcc->vpi);
	hdr->vci = htons(vcc->vci);
	hdr->length = htonl(skb->len);
	memcpy(skb_put(new_skb,skb->len),skb->data,skb->len);
	if (vcc->pop) vcc->pop(vcc,skb);
	else dev_kfree_skb(skb);
	out_vcc->push(out_vcc,new_skb);
	atomic_inc(&vcc->stats->tx);
	atomic_inc(&out_vcc->stats->rx);
	return 0;
}


static int atmtcp_v_proc(struct atm_dev *dev,loff_t *pos,char *page)
{
	struct atmtcp_dev_data *dev_data = PRIV(dev);

	if (*pos) return 0;
	if (!dev_data->persist) return sprintf(page,"ephemeral\n");
	return sprintf(page,"persistent, %sconnected\n",
	    dev_data->vcc ? "" : "dis");
}


static void atmtcp_c_close(struct atm_vcc *vcc)
{
	struct atm_dev *atmtcp_dev;
	struct atmtcp_dev_data *dev_data;
	struct sock *s;
	struct atm_vcc *walk;

	atmtcp_dev = (struct atm_dev *) vcc->dev_data;
	dev_data = PRIV(atmtcp_dev);
	dev_data->vcc = NULL;
	if (dev_data->persist) return;
	PRIV(atmtcp_dev) = NULL;
	kfree(dev_data);
	shutdown_atm_dev(atmtcp_dev);
	vcc->dev_data = NULL;
	read_lock(&vcc_sklist_lock);
	for (s = vcc_sklist; s; s = s->next) {
		walk = s->protinfo.af_atm;
		if (walk->dev != atmtcp_dev)
			continue;
		wake_up(&walk->sleep);
	}
	read_unlock(&vcc_sklist_lock);
}


static int atmtcp_c_send(struct atm_vcc *vcc,struct sk_buff *skb)
{
	struct atm_dev *dev;
	struct atmtcp_hdr *hdr;
	struct sock *s;
	struct atm_vcc *out_vcc = NULL;
	struct sk_buff *new_skb;
	int result = 0;

	if (!skb->len) return 0;
	dev = vcc->dev_data;
	hdr = (struct atmtcp_hdr *) skb->data;
	if (hdr->length == ATMTCP_HDR_MAGIC) {
		result = atmtcp_recv_control(
		    (struct atmtcp_control *) skb->data);
		goto done;
	}
	read_lock(&vcc_sklist_lock);
	for (s = vcc_sklist; s; s = s->next) {
		out_vcc = s->protinfo.af_atm;
		if (out_vcc->dev != dev)
			continue;
		if (out_vcc->vpi == ntohs(hdr->vpi) &&
		    out_vcc->vci == ntohs(hdr->vci) &&
		    out_vcc->qos.rxtp.traffic_class != ATM_NONE)
			break;
	}
	read_unlock(&vcc_sklist_lock);
	if (!out_vcc) {
		atomic_inc(&vcc->stats->tx_err);
		goto done;
	}
	skb_pull(skb,sizeof(struct atmtcp_hdr));
	new_skb = atm_alloc_charge(out_vcc,skb->len,GFP_KERNEL);
	if (!new_skb) {
		result = -ENOBUFS;
		goto done;
	}
	new_skb->stamp = xtime;
	memcpy(skb_put(new_skb,skb->len),skb->data,skb->len);
	out_vcc->push(out_vcc,new_skb);
	atomic_inc(&vcc->stats->tx);
	atomic_inc(&out_vcc->stats->rx);
done:
	if (vcc->pop) vcc->pop(vcc,skb);
	else dev_kfree_skb(skb);
	return result;
}


/*
 * Device operations for the virtual ATM devices created by ATMTCP.
 */


static struct atmdev_ops atmtcp_v_dev_ops = {
	.dev_close	= atmtcp_v_dev_close,
	.open		= atmtcp_v_open,
	.close		= atmtcp_v_close,
	.ioctl		= atmtcp_v_ioctl,
	.send		= atmtcp_v_send,
	.proc_read	= atmtcp_v_proc,
	.owner		= THIS_MODULE
};


/*
 * Device operations for the ATMTCP control device.
 */


static struct atmdev_ops atmtcp_c_dev_ops = {
	.close		= atmtcp_c_close,
	.send		= atmtcp_c_send
};


static struct atm_dev atmtcp_control_dev = {
	.ops		= &atmtcp_c_dev_ops,
	.type		= "atmtcp",
	.number		= 999,
	.lock		= SPIN_LOCK_UNLOCKED
};


static int atmtcp_create(int itf,int persist,struct atm_dev **result)
{
	struct atmtcp_dev_data *dev_data;
	struct atm_dev *dev;

	dev_data = kmalloc(sizeof(*dev_data),GFP_KERNEL);
	if (!dev_data)
		return -ENOMEM;

	dev = atm_dev_register(DEV_LABEL,&atmtcp_v_dev_ops,itf,NULL);
	if (!dev) {
		kfree(dev_data);
		return itf == -1 ? -ENOMEM : -EBUSY;
	}
	dev->ci_range.vpi_bits = MAX_VPI_BITS;
	dev->ci_range.vci_bits = MAX_VCI_BITS;
	PRIV(dev) = dev_data;
	PRIV(dev)->vcc = NULL;
	PRIV(dev)->persist = persist;
	if (result) *result = dev;
	return 0;
}


int atmtcp_attach(struct atm_vcc *vcc,int itf)
{
	struct atm_dev *dev;

	dev = NULL;
	if (itf != -1) dev = atm_dev_lookup(itf);
	if (dev) {
		if (dev->ops != &atmtcp_v_dev_ops) {
			atm_dev_put(dev);
			return -EMEDIUMTYPE;
		}
		if (PRIV(dev)->vcc) return -EBUSY;
	}
	else {
		int error;

		error = atmtcp_create(itf,0,&dev);
		if (error) return error;
	}
	PRIV(dev)->vcc = vcc;
	vcc->dev = &atmtcp_control_dev;
	vcc_insert_socket(vcc->sk);
	set_bit(ATM_VF_META,&vcc->flags);
	set_bit(ATM_VF_READY,&vcc->flags);
	vcc->dev_data = dev;
	(void) atm_init_aal5(vcc); /* @@@ losing AAL in transit ... */
	vcc->stats = &atmtcp_control_dev.stats.aal5;
	return dev->number;
}


int atmtcp_create_persistent(int itf)
{
	return atmtcp_create(itf,1,NULL);
}


int atmtcp_remove_persistent(int itf)
{
	struct atm_dev *dev;
	struct atmtcp_dev_data *dev_data;

	dev = atm_dev_lookup(itf);
	if (!dev) return -ENODEV;
	if (dev->ops != &atmtcp_v_dev_ops) {
		atm_dev_put(dev);
		return -EMEDIUMTYPE;
	}
	dev_data = PRIV(dev);
	if (!dev_data->persist) return 0;
	dev_data->persist = 0;
	if (PRIV(dev)->vcc) return 0;
	kfree(dev_data);
	atm_dev_put(dev);
	shutdown_atm_dev(dev);
	return 0;
}


#ifdef MODULE

int init_module(void)
{
	atm_tcp_ops.attach = atmtcp_attach;
	atm_tcp_ops.create_persistent = atmtcp_create_persistent;
	atm_tcp_ops.remove_persistent = atmtcp_remove_persistent;
	return 0;
}


void cleanup_module(void)
{
	atm_tcp_ops.attach = NULL;
	atm_tcp_ops.create_persistent = NULL;
	atm_tcp_ops.remove_persistent = NULL;
}

MODULE_LICENSE("GPL");
#else

struct atm_tcp_ops atm_tcp_ops = {
	atmtcp_attach,			/* attach */
	atmtcp_create_persistent,	/* create_persistent */
	atmtcp_remove_persistent	/* remove_persistent */
};

#endif
