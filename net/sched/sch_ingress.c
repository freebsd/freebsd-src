/* net/sched/sch_ingress.c - Ingress qdisc 
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Authors:     Jamal Hadi Salim 1999
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter.h>
#include <linux/smp.h>
#include <net/pkt_sched.h>
#include <asm/byteorder.h>
#include <asm/uaccess.h>
#include <linux/kmod.h>
#include <linux/stat.h>
#include <linux/interrupt.h>
#include <linux/list.h>


#undef DEBUG_INGRESS

#ifdef DEBUG_INGRESS  /* control */
#define DPRINTK(format,args...) printk(KERN_DEBUG format,##args)
#else
#define DPRINTK(format,args...)
#endif

#if 0  /* data */
#define D2PRINTK(format,args...) printk(KERN_DEBUG format,##args)
#else
#define D2PRINTK(format,args...)
#endif


#define PRIV(sch) ((struct ingress_qdisc_data *) (sch)->data)


/* Thanks to Doron Oz for this hack
*/
static int nf_registered = 0; 

struct ingress_qdisc_data {
	struct Qdisc		*q;
	struct tcf_proto	*filter_list;
};


/* ------------------------- Class/flow operations ------------------------- */


static int ingress_graft(struct Qdisc *sch,unsigned long arg,
    struct Qdisc *new,struct Qdisc **old)
{
#ifdef DEBUG_INGRESS
	struct ingress_qdisc_data *p = PRIV(sch);
#endif

	DPRINTK("ingress_graft(sch %p,[qdisc %p],new %p,old %p)\n",
		sch, p, new, old);
	DPRINTK("\n ingress_graft: You cannot add qdiscs to classes");
        return 1;
}


static struct Qdisc *ingress_leaf(struct Qdisc *sch, unsigned long arg)
{
	return NULL;
}


static unsigned long ingress_get(struct Qdisc *sch,u32 classid)
{
#ifdef DEBUG_INGRESS
	struct ingress_qdisc_data *p = PRIV(sch);
#endif
	DPRINTK("ingress_get(sch %p,[qdisc %p],classid %x)\n", sch, p, classid);
	return TC_H_MIN(classid) + 1;
}


static unsigned long ingress_bind_filter(struct Qdisc *sch,
    unsigned long parent, u32 classid)
{
	return ingress_get(sch, classid);
}


static void ingress_put(struct Qdisc *sch, unsigned long cl)
{
}


static int ingress_change(struct Qdisc *sch, u32 classid, u32 parent,
    struct rtattr **tca, unsigned long *arg)
{
#ifdef DEBUG_INGRESS
	struct ingress_qdisc_data *p = PRIV(sch);
#endif
	DPRINTK("ingress_change(sch %p,[qdisc %p],classid %x,parent %x),"
		"arg 0x%lx\n", sch, p, classid, parent, *arg);
	DPRINTK("No effect. sch_ingress doesn't maintain classes at the moment");
	return 0;
}



static void ingress_walk(struct Qdisc *sch,struct qdisc_walker *walker)
{
#ifdef DEBUG_INGRESS
	struct ingress_qdisc_data *p = PRIV(sch);
#endif
	DPRINTK("ingress_walk(sch %p,[qdisc %p],walker %p)\n", sch, p, walker);
	DPRINTK("No effect. sch_ingress doesn't maintain classes at the moment");
}


static struct tcf_proto **ingress_find_tcf(struct Qdisc *sch,unsigned long cl)
{
	struct ingress_qdisc_data *p = PRIV(sch);

	return &p->filter_list;
}


/* --------------------------- Qdisc operations ---------------------------- */


static int ingress_enqueue(struct sk_buff *skb,struct Qdisc *sch)
{
	struct ingress_qdisc_data *p = PRIV(sch);
	struct tcf_result res;
	int result;

	D2PRINTK("ingress_enqueue(skb %p,sch %p,[qdisc %p])\n", skb, sch, p);
	result = tc_classify(skb, p->filter_list, &res);
	D2PRINTK("result %d class 0x%04x\n", result, res.classid);
	/*
	 * Unlike normal "enqueue" functions, ingress_enqueue returns a
	 * firewall FW_* code.
	 */
#ifdef CONFIG_NET_CLS_POLICE
	switch (result) {
		case TC_POLICE_SHOT:
			result = NF_DROP;
			sch->stats.drops++;
			break;
		case TC_POLICE_RECLASSIFY: /* DSCP remarking here ? */
		case TC_POLICE_OK:
		case TC_POLICE_UNSPEC:
		default:
			sch->stats.packets++;
			sch->stats.bytes += skb->len;
			result = NF_ACCEPT;
			break;
	};
#else
	sch->stats.packets++;
	sch->stats.bytes += skb->len;
#endif

	skb->tc_index = TC_H_MIN(res.classid);
	return result;
}


static struct sk_buff *ingress_dequeue(struct Qdisc *sch)
{
/*
	struct ingress_qdisc_data *p = PRIV(sch);
	D2PRINTK("ingress_dequeue(sch %p,[qdisc %p])\n",sch,PRIV(p));
*/
	return NULL;
}


static int ingress_requeue(struct sk_buff *skb,struct Qdisc *sch)
{
/*
	struct ingress_qdisc_data *p = PRIV(sch);
	D2PRINTK("ingress_requeue(skb %p,sch %p,[qdisc %p])\n",skb,sch,PRIV(p));
*/
	return 0;
}

static unsigned int ingress_drop(struct Qdisc *sch)
{
#ifdef DEBUG_INGRESS
	struct ingress_qdisc_data *p = PRIV(sch);
#endif
	DPRINTK("ingress_drop(sch %p,[qdisc %p])\n", sch, p);
	return 0;
}

static unsigned int
ing_hook(unsigned int hook, struct sk_buff **pskb,
                             const struct net_device *indev,
                             const struct net_device *outdev,
	                     int (*okfn)(struct sk_buff *))
{
	
	struct Qdisc *q;
	struct sk_buff *skb = *pskb;
        struct net_device *dev = skb->dev;
	int fwres=NF_ACCEPT;

	DPRINTK("ing_hook: skb %s dev=%s len=%u\n",
		skb->sk ? "(owned)" : "(unowned)",
		skb->dev ? (*pskb)->dev->name : "(no dev)",
		skb->len);

/* 
revisit later: Use a private since lock dev->queue_lock is also
used on the egress (might slow things for an iota)
*/

	if (dev->qdisc_ingress) {
		spin_lock(&dev->queue_lock);
		if ((q = dev->qdisc_ingress) != NULL)
			fwres = q->enqueue(skb, q);
		spin_unlock(&dev->queue_lock);
        }
			
	return fwres;
}

/* after ipt_filter */
static struct nf_hook_ops ing_ops =
{
	{ NULL, NULL},
	ing_hook,
	PF_INET,
	NF_IP_PRE_ROUTING,
	NF_IP_PRI_FILTER + 1
};

int ingress_init(struct Qdisc *sch,struct rtattr *opt)
{
	struct ingress_qdisc_data *p = PRIV(sch);

	if (!nf_registered) {
		if (nf_register_hook(&ing_ops) < 0) {
			printk("ingress qdisc registration error \n");
			goto error;
			}
		nf_registered++;
	}

	DPRINTK("ingress_init(sch %p,[qdisc %p],opt %p)\n",sch,p,opt);
	memset(p, 0, sizeof(*p));
	p->filter_list = NULL;
	p->q = &noop_qdisc;
	MOD_INC_USE_COUNT;
	return 0;
error:
	return -EINVAL;
}


static void ingress_reset(struct Qdisc *sch)
{
	struct ingress_qdisc_data *p = PRIV(sch);

	DPRINTK("ingress_reset(sch %p,[qdisc %p])\n", sch, p);

/*
#if 0
*/
/* for future use */
	qdisc_reset(p->q);
/*
#endif
*/
}

/* ------------------------------------------------------------- */


/* ------------------------------------------------------------- */

static void ingress_destroy(struct Qdisc *sch)
{
	struct ingress_qdisc_data *p = PRIV(sch);
	struct tcf_proto *tp;

	DPRINTK("ingress_destroy(sch %p,[qdisc %p])\n", sch, p);
	while (p->filter_list) {
		tp = p->filter_list;
		p->filter_list = tp->next;
		tcf_destroy(tp);
	}
	memset(p, 0, sizeof(*p));
	p->filter_list = NULL;

#if 0
/* for future use */
	qdisc_destroy(p->q);
#endif
 
	MOD_DEC_USE_COUNT;

}


static int ingress_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	unsigned char *b = skb->tail;
	struct rtattr *rta;

	rta = (struct rtattr *) b;
	RTA_PUT(skb, TCA_OPTIONS, 0, NULL);
	rta->rta_len = skb->tail - b;
	return skb->len;

rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

static struct Qdisc_class_ops ingress_class_ops =
{
	ingress_graft,			/* graft */
	ingress_leaf,			/* leaf */
	ingress_get,			/* get */
	ingress_put,			/* put */
	ingress_change,			/* change */
	NULL,				/* delete */
	ingress_walk,				/* walk */

	ingress_find_tcf,		/* tcf_chain */
	ingress_bind_filter,		/* bind_tcf */
	ingress_put,			/* unbind_tcf */

	NULL,		/* dump */
};

struct Qdisc_ops ingress_qdisc_ops =
{
	NULL,				/* next */
	&ingress_class_ops,		/* cl_ops */
	"ingress",
	sizeof(struct ingress_qdisc_data),

	ingress_enqueue,		/* enqueue */
	ingress_dequeue,		/* dequeue */
	ingress_requeue,		/* requeue */
	ingress_drop,			/* drop */

	ingress_init,			/* init */
	ingress_reset,			/* reset */
	ingress_destroy,		/* destroy */
	NULL,				/* change */

	ingress_dump,			/* dump */
};


#ifdef MODULE
int init_module(void)
{
	int ret = 0;

	if ((ret = register_qdisc(&ingress_qdisc_ops)) < 0) {
		printk("Unable to register Ingress qdisc\n");
		return ret;
	}

	return ret;
}


void cleanup_module(void) 
{
	unregister_qdisc(&ingress_qdisc_ops);
	if (nf_registered)
		nf_unregister_hook(&ing_ops);
}
#endif
MODULE_LICENSE("GPL");
