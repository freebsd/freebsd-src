/*
 * net/sched/cls_api.c	Packet classifier API.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 * Changes:
 *
 * Eduardo J. Blanco <ejbs@netlabs.com.uy> :990222: kmod support
 */

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <net/sock.h>
#include <net/pkt_sched.h>

/* The list of all installed classifier types */

static struct tcf_proto_ops *tcf_proto_base;

/* Protects list of registered TC modules. It is pure SMP lock. */
static rwlock_t cls_mod_lock = RW_LOCK_UNLOCKED;

/* Find classifier type by string name */

struct tcf_proto_ops * tcf_proto_lookup_ops(struct rtattr *kind)
{
	struct tcf_proto_ops *t = NULL;

	if (kind) {
		read_lock(&cls_mod_lock);
		for (t = tcf_proto_base; t; t = t->next) {
			if (rtattr_strcmp(kind, t->kind) == 0)
				break;
		}
		read_unlock(&cls_mod_lock);
	}
	return t;
}

/* Register(unregister) new classifier type */

int register_tcf_proto_ops(struct tcf_proto_ops *ops)
{
	struct tcf_proto_ops *t, **tp;

	write_lock(&cls_mod_lock);
	for (tp = &tcf_proto_base; (t=*tp) != NULL; tp = &t->next) {
		if (strcmp(ops->kind, t->kind) == 0) {
			write_unlock(&cls_mod_lock);
			return -EEXIST;
		}
	}

	ops->next = NULL;
	*tp = ops;
	write_unlock(&cls_mod_lock);
	return 0;
}

int unregister_tcf_proto_ops(struct tcf_proto_ops *ops)
{
	struct tcf_proto_ops *t, **tp;

	write_lock(&cls_mod_lock);
	for (tp = &tcf_proto_base; (t=*tp) != NULL; tp = &t->next)
		if (t == ops)
			break;

	if (!t) {
		write_unlock(&cls_mod_lock);
		return -ENOENT;
	}
	*tp = t->next;
	write_unlock(&cls_mod_lock);
	return 0;
}

static int tfilter_notify(struct sk_buff *oskb, struct nlmsghdr *n,
			  struct tcf_proto *tp, unsigned long fh, int event);


/* Select new prio value from the range, managed by kernel. */

static __inline__ u32 tcf_auto_prio(struct tcf_proto *tp)
{
	u32 first = TC_H_MAKE(0xC0000000U,0U);

	if (tp)
		first = tp->prio-1;

	return first;
}

/* Add/change/delete/get a filter node */

static int tc_ctl_tfilter(struct sk_buff *skb, struct nlmsghdr *n, void *arg)
{
	struct rtattr **tca = arg;
	struct tcmsg *t = NLMSG_DATA(n);
	u32 protocol = TC_H_MIN(t->tcm_info);
	u32 prio = TC_H_MAJ(t->tcm_info);
	u32 nprio = prio;
	u32 parent = t->tcm_parent;
	struct net_device *dev;
	struct Qdisc  *q;
	struct tcf_proto **back, **chain;
	struct tcf_proto *tp = NULL;
	struct tcf_proto_ops *tp_ops;
	struct Qdisc_class_ops *cops;
	unsigned long cl = 0;
	unsigned long fh;
	int err;

	if (prio == 0) {
		/* If no priority is given, user wants we allocated it. */
		if (n->nlmsg_type != RTM_NEWTFILTER || !(n->nlmsg_flags&NLM_F_CREATE))
			return -ENOENT;
		prio = TC_H_MAKE(0x80000000U,0U);
	}

	/* Find head of filter chain. */

	/* Find link */
	if ((dev = __dev_get_by_index(t->tcm_ifindex)) == NULL)
		return -ENODEV;

	/* Find qdisc */
	if (!parent) {
		q = dev->qdisc_sleeping;
		parent = q->handle;
	} else if ((q = qdisc_lookup(dev, TC_H_MAJ(t->tcm_parent))) == NULL)
		return -EINVAL;

	/* Is it classful? */
	if ((cops = q->ops->cl_ops) == NULL)
		return -EINVAL;

	/* Do we search for filter, attached to class? */
	if (TC_H_MIN(parent)) {
		cl = cops->get(q, parent);
		if (cl == 0)
			return -ENOENT;
	}

	/* And the last stroke */
	chain = cops->tcf_chain(q, cl);
	err = -EINVAL;
	if (chain == NULL)
		goto errout;

	/* Check the chain for existence of proto-tcf with this priority */
	for (back = chain; (tp=*back) != NULL; back = &tp->next) {
		if (tp->prio >= prio) {
			if (tp->prio == prio) {
				if (!nprio || (tp->protocol != protocol && protocol))
					goto errout;
			} else
				tp = NULL;
			break;
		}
	}

	if (tp == NULL) {
		/* Proto-tcf does not exist, create new one */

		if (tca[TCA_KIND-1] == NULL || !protocol)
			goto errout;

		err = -ENOENT;
		if (n->nlmsg_type != RTM_NEWTFILTER || !(n->nlmsg_flags&NLM_F_CREATE))
			goto errout;


		/* Create new proto tcf */

		err = -ENOBUFS;
		if ((tp = kmalloc(sizeof(*tp), GFP_KERNEL)) == NULL)
			goto errout;
		tp_ops = tcf_proto_lookup_ops(tca[TCA_KIND-1]);
#ifdef CONFIG_KMOD
		if (tp_ops==NULL && tca[TCA_KIND-1] != NULL) {
			struct rtattr *kind = tca[TCA_KIND-1];
			char module_name[4 + IFNAMSIZ + 1];

			if (RTA_PAYLOAD(kind) <= IFNAMSIZ) {
				sprintf(module_name, "cls_%s", (char*)RTA_DATA(kind));
				request_module (module_name);
				tp_ops = tcf_proto_lookup_ops(kind);
			}
		}
#endif
		if (tp_ops == NULL) {
			err = -EINVAL;
			kfree(tp);
			goto errout;
		}
		memset(tp, 0, sizeof(*tp));
		tp->ops = tp_ops;
		tp->protocol = protocol;
		tp->prio = nprio ? : tcf_auto_prio(*back);
		tp->q = q;
		tp->classify = tp_ops->classify;
		tp->classid = parent;
		err = tp_ops->init(tp);
		if (err) {
			kfree(tp);
			goto errout;
		}
		write_lock(&qdisc_tree_lock);
		spin_lock_bh(&dev->queue_lock);
		tp->next = *back;
		*back = tp;
		spin_unlock_bh(&dev->queue_lock);
		write_unlock(&qdisc_tree_lock);
	} else if (tca[TCA_KIND-1] && rtattr_strcmp(tca[TCA_KIND-1], tp->ops->kind))
		goto errout;

	fh = tp->ops->get(tp, t->tcm_handle);

	if (fh == 0) {
		if (n->nlmsg_type == RTM_DELTFILTER && t->tcm_handle == 0) {
			write_lock(&qdisc_tree_lock);
			spin_lock_bh(&dev->queue_lock);
			*back = tp->next;
			spin_unlock_bh(&dev->queue_lock);
			write_unlock(&qdisc_tree_lock);
			tcf_destroy(tp);
			err = 0;
			goto errout;
		}

		err = -ENOENT;
		if (n->nlmsg_type != RTM_NEWTFILTER || !(n->nlmsg_flags&NLM_F_CREATE))
			goto errout;
	} else {
		switch (n->nlmsg_type) {
		case RTM_NEWTFILTER:	
			err = -EEXIST;
			if (n->nlmsg_flags&NLM_F_EXCL)
				goto errout;
			break;
		case RTM_DELTFILTER:
			err = tp->ops->delete(tp, fh);
			goto errout;
		case RTM_GETTFILTER:
			err = tfilter_notify(skb, n, tp, fh, RTM_NEWTFILTER);
			goto errout;
		default:
			err = -EINVAL;
			goto errout;
		}
	}

	err = tp->ops->change(tp, cl, t->tcm_handle, tca, &fh);
	if (err == 0)
		tfilter_notify(skb, n, tp, fh, RTM_NEWTFILTER);

errout:
	if (cl)
		cops->put(q, cl);
	return err;
}

static int
tcf_fill_node(struct sk_buff *skb, struct tcf_proto *tp, unsigned long fh,
	      u32 pid, u32 seq, unsigned flags, int event)
{
	struct tcmsg *tcm;
	struct nlmsghdr  *nlh;
	unsigned char	 *b = skb->tail;

	nlh = NLMSG_PUT(skb, pid, seq, event, sizeof(*tcm));
	nlh->nlmsg_flags = flags;
	tcm = NLMSG_DATA(nlh);
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = tp->q->dev->ifindex;
	tcm->tcm_parent = tp->classid;
	tcm->tcm_handle = 0;
	tcm->tcm_info = TC_H_MAKE(tp->prio, tp->protocol);
	RTA_PUT(skb, TCA_KIND, IFNAMSIZ, tp->ops->kind);
	if (tp->ops->dump && tp->ops->dump(tp, fh, skb, tcm) < 0)
		goto rtattr_failure;
	nlh->nlmsg_len = skb->tail - b;
	return skb->len;

nlmsg_failure:
rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

static int tfilter_notify(struct sk_buff *oskb, struct nlmsghdr *n,
			  struct tcf_proto *tp, unsigned long fh, int event)
{
	struct sk_buff *skb;
	u32 pid = oskb ? NETLINK_CB(oskb).pid : 0;

	skb = alloc_skb(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!skb)
		return -ENOBUFS;

	if (tcf_fill_node(skb, tp, fh, pid, n->nlmsg_seq, 0, event) <= 0) {
		kfree_skb(skb);
		return -EINVAL;
	}

	return rtnetlink_send(skb, pid, RTMGRP_TC, n->nlmsg_flags&NLM_F_ECHO);
}

struct tcf_dump_args
{
	struct tcf_walker w;
	struct sk_buff *skb;
	struct netlink_callback *cb;
};

static int tcf_node_dump(struct tcf_proto *tp, unsigned long n, struct tcf_walker *arg)
{
	struct tcf_dump_args *a = (void*)arg;

	return tcf_fill_node(a->skb, tp, n, NETLINK_CB(a->cb->skb).pid,
			     a->cb->nlh->nlmsg_seq, NLM_F_MULTI, RTM_NEWTFILTER);
}

static int tc_dump_tfilter(struct sk_buff *skb, struct netlink_callback *cb)
{
	int t;
	int s_t;
	struct net_device *dev;
	struct Qdisc *q;
	struct tcf_proto *tp, **chain;
	struct tcmsg *tcm = (struct tcmsg*)NLMSG_DATA(cb->nlh);
	unsigned long cl = 0;
	struct Qdisc_class_ops *cops;
	struct tcf_dump_args arg;

	if (cb->nlh->nlmsg_len < NLMSG_LENGTH(sizeof(*tcm)))
		return skb->len;
	if ((dev = dev_get_by_index(tcm->tcm_ifindex)) == NULL)
		return skb->len;

	read_lock(&qdisc_tree_lock);
	if (!tcm->tcm_parent)
		q = dev->qdisc_sleeping;
	else
		q = qdisc_lookup(dev, TC_H_MAJ(tcm->tcm_parent));
	if (q == NULL) {
		read_unlock(&qdisc_tree_lock);
		dev_put(dev);
		return skb->len;
	}
	if ((cops = q->ops->cl_ops) == NULL)
		goto errout;
	if (TC_H_MIN(tcm->tcm_parent)) {
		cl = cops->get(q, tcm->tcm_parent);
		if (cl == 0)
			goto errout;
	}
	chain = cops->tcf_chain(q, cl);
	if (chain == NULL)
		goto errout;

	s_t = cb->args[0];

	for (tp=*chain, t=0; tp; tp = tp->next, t++) {
		if (t < s_t) continue;
		if (TC_H_MAJ(tcm->tcm_info) &&
		    TC_H_MAJ(tcm->tcm_info) != tp->prio)
			continue;
		if (TC_H_MIN(tcm->tcm_info) &&
		    TC_H_MIN(tcm->tcm_info) != tp->protocol)
			continue;
		if (t > s_t)
			memset(&cb->args[1], 0, sizeof(cb->args)-sizeof(cb->args[0]));
		if (cb->args[1] == 0) {
			if (tcf_fill_node(skb, tp, 0, NETLINK_CB(cb->skb).pid,
					  cb->nlh->nlmsg_seq, NLM_F_MULTI, RTM_NEWTFILTER) <= 0) {
				break;
			}
			cb->args[1] = 1;
		}
		if (tp->ops->walk == NULL)
			continue;
		arg.w.fn = tcf_node_dump;
		arg.skb = skb;
		arg.cb = cb;
		arg.w.stop = 0;
		arg.w.skip = cb->args[1]-1;
		arg.w.count = 0;
		tp->ops->walk(tp, &arg.w);
		cb->args[1] = arg.w.count+1;
		if (arg.w.stop)
			break;
	}

	cb->args[0] = t;

errout:
	if (cl)
		cops->put(q, cl);

	read_unlock(&qdisc_tree_lock);
	dev_put(dev);
	return skb->len;
}


int __init tc_filter_init(void)
{
	struct rtnetlink_link *link_p = rtnetlink_links[PF_UNSPEC];

	/* Setup rtnetlink links. It is made here to avoid
	   exporting large number of public symbols.
	 */

	if (link_p) {
		link_p[RTM_NEWTFILTER-RTM_BASE].doit = tc_ctl_tfilter;
		link_p[RTM_DELTFILTER-RTM_BASE].doit = tc_ctl_tfilter;
		link_p[RTM_GETTFILTER-RTM_BASE].doit = tc_ctl_tfilter;
		link_p[RTM_GETTFILTER-RTM_BASE].dumpit = tc_dump_tfilter;
	}
#define INIT_TC_FILTER(name) { \
          extern struct tcf_proto_ops cls_##name##_ops; \
          register_tcf_proto_ops(&cls_##name##_ops); \
	}

#ifdef CONFIG_NET_CLS_U32
	INIT_TC_FILTER(u32);
#endif
#ifdef CONFIG_NET_CLS_ROUTE4
	INIT_TC_FILTER(route4);
#endif
#ifdef CONFIG_NET_CLS_FW
	INIT_TC_FILTER(fw);
#endif
#ifdef CONFIG_NET_CLS_RSVP
	INIT_TC_FILTER(rsvp);
#endif
#ifdef CONFIG_NET_CLS_TCINDEX
	INIT_TC_FILTER(tcindex);
#endif
#ifdef CONFIG_NET_CLS_RSVP6
	INIT_TC_FILTER(rsvp6);
#endif
	return 0;
}
