/*
 * net/sched/cls_route.c	ROUTE4 classifier.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 */

#include <linux/module.h>
#include <linux/config.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/bitops.h>
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
#include <linux/if_ether.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/notifier.h>
#include <net/ip.h>
#include <net/route.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/pkt_sched.h>

/*
   1. For now we assume that route tags < 256.
      It allows to use direct table lookups, instead of hash tables.
   2. For now we assume that "from TAG" and "fromdev DEV" statements
      are mutually  exclusive.
   3. "to TAG from ANY" has higher priority, than "to ANY from XXX"
 */

struct route4_fastmap
{
	struct route4_filter	*filter;
	u32			id;
	int			iif;
};

struct route4_head
{
	struct route4_fastmap	fastmap[16];
	struct route4_bucket	*table[256+1];
};

struct route4_bucket
{
	struct route4_filter	*ht[16+16+1];
};

struct route4_filter
{
	struct route4_filter	*next;
	u32			id;
	int			iif;

	struct tcf_result	res;
#ifdef CONFIG_NET_CLS_POLICE
	struct tcf_police	*police;
#endif

	u32			handle;
	struct route4_bucket	*bkt;
};

#define ROUTE4_FAILURE ((struct route4_filter*)(-1L))

static __inline__ int route4_fastmap_hash(u32 id, int iif)
{
	return id&0xF;
}

static void route4_reset_fastmap(struct net_device *dev, struct route4_head *head, u32 id)
{
	spin_lock_bh(&dev->queue_lock);
	memset(head->fastmap, 0, sizeof(head->fastmap));
	spin_unlock_bh(&dev->queue_lock);
}

static void __inline__
route4_set_fastmap(struct route4_head *head, u32 id, int iif,
		   struct route4_filter *f)
{
	int h = route4_fastmap_hash(id, iif);
	head->fastmap[h].id = id;
	head->fastmap[h].iif = iif;
	head->fastmap[h].filter = f;
}

static __inline__ int route4_hash_to(u32 id)
{
	return id&0xFF;
}

static __inline__ int route4_hash_from(u32 id)
{
	return (id>>16)&0xF;
}

static __inline__ int route4_hash_iif(int iif)
{
	return 16 + ((iif>>16)&0xF);
}

static __inline__ int route4_hash_wild(void)
{
	return 32;
}

#ifdef CONFIG_NET_CLS_POLICE
#define IF_ROUTE_POLICE \
if (f->police) { \
	int pol_res = tcf_police(skb, f->police); \
	if (pol_res >= 0) return pol_res; \
	dont_cache = 1; \
	continue; \
} \
if (!dont_cache)
#else
#define IF_ROUTE_POLICE
#endif


static int route4_classify(struct sk_buff *skb, struct tcf_proto *tp,
			   struct tcf_result *res)
{
	struct route4_head *head = (struct route4_head*)tp->root;
	struct dst_entry *dst;
	struct route4_bucket *b;
	struct route4_filter *f;
#ifdef CONFIG_NET_CLS_POLICE
	int dont_cache = 0;
#endif
	u32 id, h;
	int iif;

	if ((dst = skb->dst) == NULL)
		goto failure;

	id = dst->tclassid;
	if (head == NULL)
		goto old_method;

	iif = ((struct rtable*)dst)->key.iif;

	h = route4_fastmap_hash(id, iif);
	if (id == head->fastmap[h].id &&
	    iif == head->fastmap[h].iif &&
	    (f = head->fastmap[h].filter) != NULL) {
		if (f == ROUTE4_FAILURE)
			goto failure;

		*res = f->res;
		return 0;
	}

	h = route4_hash_to(id);

restart:
	if ((b = head->table[h]) != NULL) {
		f = b->ht[route4_hash_from(id)];

		for ( ; f; f = f->next) {
			if (f->id == id) {
				*res = f->res;
				IF_ROUTE_POLICE route4_set_fastmap(head, id, iif, f);
				return 0;
			}
		}

		for (f = b->ht[route4_hash_iif(iif)]; f; f = f->next) {
			if (f->iif == iif) {
				*res = f->res;
				IF_ROUTE_POLICE route4_set_fastmap(head, id, iif, f);
				return 0;
			}
		}

		for (f = b->ht[route4_hash_wild()]; f; f = f->next) {
			*res = f->res;
			IF_ROUTE_POLICE route4_set_fastmap(head, id, iif, f);
			return 0;
		}

	}
	if (h < 256) {
		h = 256;
		id &= ~0xFFFF;
		goto restart;
	}

#ifdef CONFIG_NET_CLS_POLICE
	if (!dont_cache)
#endif
		route4_set_fastmap(head, id, iif, ROUTE4_FAILURE);
failure:
	return -1;

old_method:
	if (id && (TC_H_MAJ(id) == 0 ||
		   !(TC_H_MAJ(id^tp->q->handle)))) {
		res->classid = id;
		res->class = 0;
		return 0;
	}
	return -1;
}

static u32 to_hash(u32 id)
{
	u32 h = id&0xFF;
	if (id&0x8000)
		h += 256;
	return h;
}

static u32 from_hash(u32 id)
{
	id &= 0xFFFF;
	if (id == 0xFFFF)
		return 32;
	if (!(id & 0x8000)) {
		if (id > 255)
			return 256;
		return id&0xF;
	}
	return 16 + (id&0xF);
}

static unsigned long route4_get(struct tcf_proto *tp, u32 handle)
{
	struct route4_head *head = (struct route4_head*)tp->root;
	struct route4_bucket *b;
	struct route4_filter *f;
	unsigned h1, h2;

	if (!head)
		return 0;

	h1 = to_hash(handle);
	if (h1 > 256)
		return 0;

	h2 = from_hash(handle>>16);
	if (h2 > 32)
		return 0;

	if ((b = head->table[h1]) != NULL) {
		for (f = b->ht[h2]; f; f = f->next)
			if (f->handle == handle)
				return (unsigned long)f;
	}
	return 0;
}

static void route4_put(struct tcf_proto *tp, unsigned long f)
{
}

static int route4_init(struct tcf_proto *tp)
{
	MOD_INC_USE_COUNT;
	return 0;
}

static void route4_destroy(struct tcf_proto *tp)
{
	struct route4_head *head = xchg(&tp->root, NULL);
	int h1, h2;

	if (head == NULL) {
		MOD_DEC_USE_COUNT;
		return;
	}

	for (h1=0; h1<=256; h1++) {
		struct route4_bucket *b;

		if ((b = head->table[h1]) != NULL) {
			for (h2=0; h2<=32; h2++) {
				struct route4_filter *f;

				while ((f = b->ht[h2]) != NULL) {
					unsigned long cl;

					b->ht[h2] = f->next;
					if ((cl = __cls_set_class(&f->res.class, 0)) != 0)
						tp->q->ops->cl_ops->unbind_tcf(tp->q, cl);
#ifdef CONFIG_NET_CLS_POLICE
					tcf_police_release(f->police);
#endif
					kfree(f);
				}
			}
			kfree(b);
		}
	}
	kfree(head);
	MOD_DEC_USE_COUNT;
}

static int route4_delete(struct tcf_proto *tp, unsigned long arg)
{
	struct route4_head *head = (struct route4_head*)tp->root;
	struct route4_filter **fp, *f = (struct route4_filter*)arg;
	unsigned h = 0;
	struct route4_bucket *b;
	int i;

	if (!head || !f)
		return -EINVAL;

	h = f->handle;
	b = f->bkt;

	for (fp = &b->ht[from_hash(h>>16)]; *fp; fp = &(*fp)->next) {
		if (*fp == f) {
			unsigned long cl;

			tcf_tree_lock(tp);
			*fp = f->next;
			tcf_tree_unlock(tp);

			route4_reset_fastmap(tp->q->dev, head, f->id);

			if ((cl = cls_set_class(tp, &f->res.class, 0)) != 0)
				tp->q->ops->cl_ops->unbind_tcf(tp->q, cl);

#ifdef CONFIG_NET_CLS_POLICE
			tcf_police_release(f->police);
#endif
			kfree(f);

			/* Strip tree */

			for (i=0; i<=32; i++)
				if (b->ht[i])
					return 0;

			/* OK, session has no flows */
			tcf_tree_lock(tp);
			head->table[to_hash(h)] = NULL;
			tcf_tree_unlock(tp);

			kfree(b);
			return 0;
		}
	}
	return 0;
}

static int route4_change(struct tcf_proto *tp, unsigned long base,
		       u32 handle,
		       struct rtattr **tca,
		       unsigned long *arg)
{
	struct route4_head *head = tp->root;
	struct route4_filter *f, *f1, **ins_f;
	struct route4_bucket *b;
	struct rtattr *opt = tca[TCA_OPTIONS-1];
	struct rtattr *tb[TCA_ROUTE4_MAX];
	unsigned h1, h2;
	int err;

	if (opt == NULL)
		return handle ? -EINVAL : 0;

	if (rtattr_parse(tb, TCA_ROUTE4_MAX, RTA_DATA(opt), RTA_PAYLOAD(opt)) < 0)
		return -EINVAL;

	if ((f = (struct route4_filter*)*arg) != NULL) {
		/* Node exists: adjust only classid */

		if (f->handle != handle && handle)
			return -EINVAL;
		if (tb[TCA_ROUTE4_CLASSID-1]) {
			unsigned long cl;

			f->res.classid = *(u32*)RTA_DATA(tb[TCA_ROUTE4_CLASSID-1]);
			cl = cls_set_class(tp, &f->res.class, tp->q->ops->cl_ops->bind_tcf(tp->q, base, f->res.classid));
			if (cl)
				tp->q->ops->cl_ops->unbind_tcf(tp->q, cl);
		}
#ifdef CONFIG_NET_CLS_POLICE
		if (tb[TCA_ROUTE4_POLICE-1]) {
			struct tcf_police *police = tcf_police_locate(tb[TCA_ROUTE4_POLICE-1], tca[TCA_RATE-1]);

			tcf_tree_lock(tp);
			police = xchg(&f->police, police);
			tcf_tree_unlock(tp);

			tcf_police_release(police);
		}
#endif
		return 0;
	}

	/* Now more serious part... */

	if (head == NULL) {
		head = kmalloc(sizeof(struct route4_head), GFP_KERNEL);
		if (head == NULL)
			return -ENOBUFS;
		memset(head, 0, sizeof(struct route4_head));

		tcf_tree_lock(tp);
		tp->root = head;
		tcf_tree_unlock(tp);
	}

	f = kmalloc(sizeof(struct route4_filter), GFP_KERNEL);
	if (f == NULL)
		return -ENOBUFS;

	memset(f, 0, sizeof(*f));

	err = -EINVAL;
	f->handle = 0x8000;
	if (tb[TCA_ROUTE4_TO-1]) {
		if (handle&0x8000)
			goto errout;
		if (RTA_PAYLOAD(tb[TCA_ROUTE4_TO-1]) < 4)
			goto errout;
		f->id = *(u32*)RTA_DATA(tb[TCA_ROUTE4_TO-1]);
		if (f->id > 0xFF)
			goto errout;
		f->handle = f->id;
	}
	if (tb[TCA_ROUTE4_FROM-1]) {
		u32 sid;
		if (tb[TCA_ROUTE4_IIF-1])
			goto errout;
		if (RTA_PAYLOAD(tb[TCA_ROUTE4_FROM-1]) < 4)
			goto errout;
		sid = (*(u32*)RTA_DATA(tb[TCA_ROUTE4_FROM-1]));
		if (sid > 0xFF)
			goto errout;
		f->handle |= sid<<16;
		f->id |= sid<<16;
	} else if (tb[TCA_ROUTE4_IIF-1]) {
		if (RTA_PAYLOAD(tb[TCA_ROUTE4_IIF-1]) < 4)
			goto errout;
		f->iif = *(u32*)RTA_DATA(tb[TCA_ROUTE4_IIF-1]);
		if (f->iif > 0x7FFF)
			goto errout;
		f->handle |= (f->iif|0x8000)<<16;
	} else
		f->handle |= 0xFFFF<<16;

	if (handle) {
		f->handle |= handle&0x7F00;
		if (f->handle != handle)
			goto errout;
	}

	if (tb[TCA_ROUTE4_CLASSID-1]) {
		if (RTA_PAYLOAD(tb[TCA_ROUTE4_CLASSID-1]) < 4)
			goto errout;
		f->res.classid = *(u32*)RTA_DATA(tb[TCA_ROUTE4_CLASSID-1]);
	}

	h1 = to_hash(f->handle);
	if ((b = head->table[h1]) == NULL) {
		err = -ENOBUFS;
		b = kmalloc(sizeof(struct route4_bucket), GFP_KERNEL);
		if (b == NULL)
			goto errout;
		memset(b, 0, sizeof(*b));

		tcf_tree_lock(tp);
		head->table[h1] = b;
		tcf_tree_unlock(tp);
	}
	f->bkt = b;

	err = -EEXIST;
	h2 = from_hash(f->handle>>16);
	for (ins_f = &b->ht[h2]; (f1=*ins_f) != NULL; ins_f = &f1->next) {
		if (f->handle < f1->handle)
			break;
		if (f1->handle == f->handle)
			goto errout;
	}

	cls_set_class(tp, &f->res.class, tp->q->ops->cl_ops->bind_tcf(tp->q, base, f->res.classid));
#ifdef CONFIG_NET_CLS_POLICE
	if (tb[TCA_ROUTE4_POLICE-1])
		f->police = tcf_police_locate(tb[TCA_ROUTE4_POLICE-1], tca[TCA_RATE-1]);
#endif

	f->next = f1;
	tcf_tree_lock(tp);
	*ins_f = f;
	tcf_tree_unlock(tp);

	route4_reset_fastmap(tp->q->dev, head, f->id);
	*arg = (unsigned long)f;
	return 0;

errout:
	if (f)
		kfree(f);
	return err;
}

static void route4_walk(struct tcf_proto *tp, struct tcf_walker *arg)
{
	struct route4_head *head = tp->root;
	unsigned h, h1;

	if (head == NULL)
		arg->stop = 1;

	if (arg->stop)
		return;

	for (h = 0; h <= 256; h++) {
		struct route4_bucket *b = head->table[h];

		if (b) {
			for (h1 = 0; h1 <= 32; h1++) {
				struct route4_filter *f;

				for (f = b->ht[h1]; f; f = f->next) {
					if (arg->count < arg->skip) {
						arg->count++;
						continue;
					}
					if (arg->fn(tp, (unsigned long)f, arg) < 0) {
						arg->stop = 1;
						break;
					}
					arg->count++;
				}
			}
		}
	}
}

static int route4_dump(struct tcf_proto *tp, unsigned long fh,
		       struct sk_buff *skb, struct tcmsg *t)
{
	struct route4_filter *f = (struct route4_filter*)fh;
	unsigned char	 *b = skb->tail;
	struct rtattr *rta;
	u32 id;

	if (f == NULL)
		return skb->len;

	t->tcm_handle = f->handle;

	rta = (struct rtattr*)b;
	RTA_PUT(skb, TCA_OPTIONS, 0, NULL);

	if (!(f->handle&0x8000)) {
		id = f->id&0xFF;
		RTA_PUT(skb, TCA_ROUTE4_TO, sizeof(id), &id);
	}
	if (f->handle&0x80000000) {
		if ((f->handle>>16) != 0xFFFF)
			RTA_PUT(skb, TCA_ROUTE4_IIF, sizeof(f->iif), &f->iif);
	} else {
		id = f->id>>16;
		RTA_PUT(skb, TCA_ROUTE4_FROM, sizeof(id), &id);
	}
	if (f->res.classid)
		RTA_PUT(skb, TCA_ROUTE4_CLASSID, 4, &f->res.classid);
#ifdef CONFIG_NET_CLS_POLICE
	if (f->police) {
		struct rtattr * p_rta = (struct rtattr*)skb->tail;

		RTA_PUT(skb, TCA_ROUTE4_POLICE, 0, NULL);

		if (tcf_police_dump(skb, f->police) < 0)
			goto rtattr_failure;

		p_rta->rta_len = skb->tail - (u8*)p_rta;
	}
#endif

	rta->rta_len = skb->tail - b;
#ifdef CONFIG_NET_CLS_POLICE
	if (f->police) {
		if (qdisc_copy_stats(skb, &f->police->stats))
			goto rtattr_failure;
	}
#endif
	return skb->len;

rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

struct tcf_proto_ops cls_route4_ops = {
	NULL,
	"route",
	route4_classify,
	route4_init,
	route4_destroy,

	route4_get,
	route4_put,
	route4_change,
	route4_delete,
	route4_walk,
	route4_dump
};

#ifdef MODULE
int init_module(void)
{
	return register_tcf_proto_ops(&cls_route4_ops);
}

void cleanup_module(void)
{
	unregister_tcf_proto_ops(&cls_route4_ops);
}
#endif
MODULE_LICENSE("GPL");
