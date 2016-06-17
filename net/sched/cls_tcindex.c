/*
 * net/sched/cls_tcindex.c	Packet classifier for skb->tc_index
 *
 * Written 1998,1999 by Werner Almesberger, EPFL ICA
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <net/ip.h>
#include <net/pkt_sched.h>
#include <net/route.h>


/*
 * Not quite sure if we need all the xchgs Alexey uses when accessing things.
 * Can always add them later ... :)
 */

/*
 * Passing parameters to the root seems to be done more awkwardly than really
 * necessary. At least, u32 doesn't seem to use such dirty hacks. To be
 * verified. FIXME.
 */

#define PERFECT_HASH_THRESHOLD	64	/* use perfect hash if not bigger */
#define DEFAULT_HASH_SIZE	64	/* optimized for diffserv */


#if 1 /* control */
#define DPRINTK(format,args...) printk(KERN_DEBUG format,##args)
#else
#define DPRINTK(format,args...)
#endif

#if 0 /* data */
#define D2PRINTK(format,args...) printk(KERN_DEBUG format,##args)
#else
#define D2PRINTK(format,args...)
#endif


#define	PRIV(tp)	((struct tcindex_data *) (tp)->root)


struct tcindex_filter_result {
	struct tcf_police *police;
	struct tcf_result res;
};

struct tcindex_filter {
	__u16 key;
	struct tcindex_filter_result result;
	struct tcindex_filter *next;
};


struct tcindex_data {
	struct tcindex_filter_result *perfect; /* perfect hash; NULL if none */
	struct tcindex_filter **h; /* imperfect hash; only used if !perfect;
				      NULL if unused */
	__u16 mask;		/* AND key with mask */
	int shift;		/* shift ANDed key to the right */
	int hash;		/* hash table size; 0 if undefined */
	int alloc_hash;		/* allocated size */
	int fall_through;	/* 0: only classify if explicit match */
};


static struct tcindex_filter_result *lookup(struct tcindex_data *p,__u16 key)
{
	struct tcindex_filter *f;

	if (p->perfect)
		return p->perfect[key].res.class ? p->perfect+key : NULL;
	if (!p->h)
		return NULL;
	for (f = p->h[key % p->hash]; f; f = f->next) {
		if (f->key == key)
			return &f->result;
	}
	return NULL;
}


static int tcindex_classify(struct sk_buff *skb, struct tcf_proto *tp,
			  struct tcf_result *res)
{
	struct tcindex_data *p = PRIV(tp);
	struct tcindex_filter_result *f;

	D2PRINTK("tcindex_classify(skb %p,tp %p,res %p),p %p\n",skb,tp,res,p);

	f = lookup(p,(skb->tc_index & p->mask) >> p->shift);
	if (!f) {
		if (!p->fall_through)
			return -1;
		res->classid = TC_H_MAKE(TC_H_MAJ(tp->q->handle),
		    (skb->tc_index& p->mask) >> p->shift);
		res->class = 0;
		D2PRINTK("alg 0x%x\n",res->classid);
		return 0;
	}
	*res = f->res;
	D2PRINTK("map 0x%x\n",res->classid);
#ifdef CONFIG_NET_CLS_POLICE
	if (f->police) {
		int result;

		result = tcf_police(skb,f->police);
		D2PRINTK("police %d\n",res);
		return result;
	}
#endif
	return 0;
}


static unsigned long tcindex_get(struct tcf_proto *tp, u32 handle)
{
	struct tcindex_data *p = PRIV(tp);
	struct tcindex_filter_result *r;

	DPRINTK("tcindex_get(tp %p,handle 0x%08x)\n",tp,handle);
	if (p->perfect && handle >= p->alloc_hash)
		return 0;
	r = lookup(PRIV(tp),handle);
	return r && r->res.class ? (unsigned long) r : 0;
}


static void tcindex_put(struct tcf_proto *tp, unsigned long f)
{
	DPRINTK("tcindex_put(tp %p,f 0x%lx)\n",tp,f);
}


static int tcindex_init(struct tcf_proto *tp)
{
	struct tcindex_data *p;

	DPRINTK("tcindex_init(tp %p)\n",tp);
	MOD_INC_USE_COUNT;
	p = kmalloc(sizeof(struct tcindex_data),GFP_KERNEL);
	if (!p) {
		MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}
	tp->root = p;
	p->perfect = NULL;
	p->h = NULL;
	p->hash = 0;
	p->mask = 0xffff;
	p->shift = 0;
	p->fall_through = 1;
	return 0;
}


static int tcindex_delete(struct tcf_proto *tp, unsigned long arg)
{
	struct tcindex_data *p = PRIV(tp);
	struct tcindex_filter_result *r = (struct tcindex_filter_result *) arg;
	struct tcindex_filter *f = NULL;
	unsigned long cl;

	DPRINTK("tcindex_delete(tp %p,arg 0x%lx),p %p,f %p\n",tp,arg,p,f);
	if (p->perfect) {
		if (!r->res.class)
			return -ENOENT;
	} else {
		int i;
		struct tcindex_filter **walk = NULL;

		for (i = 0; i < p->hash; i++)
			for (walk = p->h+i; *walk; walk = &(*walk)->next)
				if (&(*walk)->result == r)
					goto found;
		return -ENOENT;

found:
		f = *walk;
		tcf_tree_lock(tp); 
		*walk = f->next;
		tcf_tree_unlock(tp);
	}
	cl = __cls_set_class(&r->res.class,0);
	if (cl)
		tp->q->ops->cl_ops->unbind_tcf(tp->q,cl);
#ifdef CONFIG_NET_CLS_POLICE
	tcf_police_release(r->police);
#endif
	if (f)
		kfree(f);
	return 0;
}


/*
 * There are no parameters for tcindex_init, so we overload tcindex_change
 */


static int tcindex_change(struct tcf_proto *tp,unsigned long base,u32 handle,
    struct rtattr **tca,unsigned long *arg)
{
	struct tcindex_filter_result new_filter_result = {
		NULL,		/* no policing */
		{ 0,0 },	/* no classification */
	};
	struct rtattr *opt = tca[TCA_OPTIONS-1];
	struct rtattr *tb[TCA_TCINDEX_MAX];
	struct tcindex_data *p = PRIV(tp);
	struct tcindex_filter *f;
	struct tcindex_filter_result *r = (struct tcindex_filter_result *) *arg;
	struct tcindex_filter **walk;
	int hash,shift;
	__u16 mask;

	DPRINTK("tcindex_change(tp %p,handle 0x%08x,tca %p,arg %p),opt %p,"
	    "p %p,r %p\n",tp,handle,tca,arg,opt,p,r);
	if (arg)
		DPRINTK("*arg = 0x%lx\n",*arg);
	if (!opt)
		return 0;
	if (rtattr_parse(tb,TCA_TCINDEX_MAX,RTA_DATA(opt),RTA_PAYLOAD(opt)) < 0)
		return -EINVAL;
	if (!tb[TCA_TCINDEX_HASH-1]) {
		hash = p->hash;
	} else {
		if (RTA_PAYLOAD(tb[TCA_TCINDEX_HASH-1]) < sizeof(int))
			return -EINVAL;
		hash = *(int *) RTA_DATA(tb[TCA_TCINDEX_HASH-1]);
	}
	if (!tb[TCA_TCINDEX_MASK-1]) {
		mask = p->mask;
	} else {
		if (RTA_PAYLOAD(tb[TCA_TCINDEX_MASK-1]) < sizeof(__u16))
			return -EINVAL;
		mask = *(__u16 *) RTA_DATA(tb[TCA_TCINDEX_MASK-1]);
	}
	if (!tb[TCA_TCINDEX_SHIFT-1])
		shift = p->shift;
	else {
		if (RTA_PAYLOAD(tb[TCA_TCINDEX_SHIFT-1]) < sizeof(__u16))
			return -EINVAL;
		shift = *(int *) RTA_DATA(tb[TCA_TCINDEX_SHIFT-1]);
	}
	if (p->perfect && hash <= (mask >> shift))
		return -EBUSY;
	if (p->perfect && hash > p->alloc_hash)
		return -EBUSY;
	if (p->h && hash != p->alloc_hash)
		return -EBUSY;
	p->hash = hash;
	p->mask = mask;
	p->shift = shift;
	if (tb[TCA_TCINDEX_FALL_THROUGH-1]) {
		if (RTA_PAYLOAD(tb[TCA_TCINDEX_FALL_THROUGH-1]) < sizeof(int))
			return -EINVAL;
		p->fall_through =
		    *(int *) RTA_DATA(tb[TCA_TCINDEX_FALL_THROUGH-1]);
	}
	DPRINTK("classid/police %p/%p\n",tb[TCA_TCINDEX_CLASSID-1],
	    tb[TCA_TCINDEX_POLICE-1]);
	if (!tb[TCA_TCINDEX_CLASSID-1] && !tb[TCA_TCINDEX_POLICE-1])
		return 0;
	if (!hash) {
		if ((mask >> shift) < PERFECT_HASH_THRESHOLD) {
			p->hash = (mask >> shift)+1;
		} else {
			p->hash = DEFAULT_HASH_SIZE;
		}
	}
	if (!p->perfect && !p->h) {
		p->alloc_hash = p->hash;
		DPRINTK("hash %d mask %d\n",p->hash,p->mask);
		if (p->hash > (mask >> shift)) {
			p->perfect = kmalloc(p->hash*
			    sizeof(struct tcindex_filter_result),GFP_KERNEL);
			if (!p->perfect)
				return -ENOMEM;
			memset(p->perfect, 0,
			       p->hash * sizeof(struct tcindex_filter_result));
		} else {
			p->h = kmalloc(p->hash*sizeof(struct tcindex_filter *),
			    GFP_KERNEL);
			if (!p->h)
				return -ENOMEM;
			memset(p->h, 0, p->hash*sizeof(struct tcindex_filter *));
		}
	}
	/*
	 * Note: this could be as restrictive as
	 * if (handle & ~(mask >> shift))
	 * but then, we'd fail handles that may become valid after some
	 * future mask change. While this is extremely unlikely to ever
	 * matter, the check below is safer (and also more
	 * backwards-compatible).
	 */
	if (p->perfect && handle >= p->alloc_hash)
		return -EINVAL;
	if (p->perfect) {
		r = p->perfect+handle;
	} else {
		r = lookup(p,handle);
		DPRINTK("r=%p\n",r);
		if (!r)
			r = &new_filter_result;
	}
	DPRINTK("r=%p\n",r);
	if (tb[TCA_TCINDEX_CLASSID-1]) {
		unsigned long cl = cls_set_class(tp,&r->res.class,0);

		if (cl)
			tp->q->ops->cl_ops->unbind_tcf(tp->q,cl);
		r->res.classid = *(__u32 *) RTA_DATA(tb[TCA_TCINDEX_CLASSID-1]);
		r->res.class = tp->q->ops->cl_ops->bind_tcf(tp->q,base,
							    r->res.classid);
		if (!r->res.class) {
			r->res.classid = 0;
			return -ENOENT;
		}
        }
#ifdef CONFIG_NET_CLS_POLICE
	{
		struct tcf_police *police;

		police = tb[TCA_TCINDEX_POLICE-1] ?
		    tcf_police_locate(tb[TCA_TCINDEX_POLICE-1],NULL) : NULL;
		tcf_tree_lock(tp);
		police = xchg(&r->police,police);
		tcf_tree_unlock(tp);
		tcf_police_release(police);
	}
#endif
	if (r != &new_filter_result)
		return 0;
	f = kmalloc(sizeof(struct tcindex_filter),GFP_KERNEL);
	if (!f)
		return -ENOMEM;
	f->key = handle;
	f->result = new_filter_result;
	f->next = NULL;
	for (walk = p->h+(handle % p->hash); *walk; walk = &(*walk)->next)
		/* nothing */;
	wmb();
	*walk = f;
	return 0;
}


static void tcindex_walk(struct tcf_proto *tp, struct tcf_walker *walker)
{
	struct tcindex_data *p = PRIV(tp);
	struct tcindex_filter *f,*next;
	int i;

	DPRINTK("tcindex_walk(tp %p,walker %p),p %p\n",tp,walker,p);
	if (p->perfect) {
		for (i = 0; i < p->hash; i++) {
			if (!p->perfect[i].res.class)
				continue;
			if (walker->count >= walker->skip) {
				if (walker->fn(tp,
				    (unsigned long) (p->perfect+i), walker)
				     < 0) {
					walker->stop = 1;
					return;
				}
			}
			walker->count++;
		}
	}
	if (!p->h)
		return;
	for (i = 0; i < p->hash; i++) {
		for (f = p->h[i]; f; f = next) {
			next = f->next;
			if (walker->count >= walker->skip) {
				if (walker->fn(tp,(unsigned long) &f->result,
				    walker) < 0) {
					walker->stop = 1;
					return;
				}
			}
			walker->count++;
		}
	}
}


static int tcindex_destroy_element(struct tcf_proto *tp,
    unsigned long arg, struct tcf_walker *walker)
{
	return tcindex_delete(tp,arg);
}


static void tcindex_destroy(struct tcf_proto *tp)
{
	struct tcindex_data *p = PRIV(tp);
	struct tcf_walker walker;

	DPRINTK("tcindex_destroy(tp %p),p %p\n",tp,p);
	walker.count = 0;
	walker.skip = 0;
	walker.fn = &tcindex_destroy_element;
	tcindex_walk(tp,&walker);
	if (p->perfect)
		kfree(p->perfect);
	if (p->h)
		kfree(p->h);
	kfree(p);
	tp->root = NULL;
	MOD_DEC_USE_COUNT;
}


static int tcindex_dump(struct tcf_proto *tp, unsigned long fh,
    struct sk_buff *skb, struct tcmsg *t)
{
	struct tcindex_data *p = PRIV(tp);
	struct tcindex_filter_result *r = (struct tcindex_filter_result *) fh;
	unsigned char *b = skb->tail;
	struct rtattr *rta;

	DPRINTK("tcindex_dump(tp %p,fh 0x%lx,skb %p,t %p),p %p,r %p,b %p\n",
	    tp,fh,skb,t,p,r,b);
	DPRINTK("p->perfect %p p->h %p\n",p->perfect,p->h);
	rta = (struct rtattr *) b;
	RTA_PUT(skb,TCA_OPTIONS,0,NULL);
	if (!fh) {
		t->tcm_handle = ~0; /* whatever ... */
		RTA_PUT(skb,TCA_TCINDEX_HASH,sizeof(p->hash),&p->hash);
		RTA_PUT(skb,TCA_TCINDEX_MASK,sizeof(p->mask),&p->mask);
		RTA_PUT(skb,TCA_TCINDEX_SHIFT,sizeof(p->shift),&p->shift);
		RTA_PUT(skb,TCA_TCINDEX_FALL_THROUGH,sizeof(p->fall_through),
		    &p->fall_through);
	} else {
		if (p->perfect) {
			t->tcm_handle = r-p->perfect;
		} else {
			struct tcindex_filter *f;
			int i;

			t->tcm_handle = 0;
			for (i = 0; !t->tcm_handle && i < p->hash; i++) {
				for (f = p->h[i]; !t->tcm_handle && f;
				     f = f->next) {
					if (&f->result == r)
						t->tcm_handle = f->key;
				}
			}
		}
		DPRINTK("handle = %d\n",t->tcm_handle);
		if (r->res.class)
			RTA_PUT(skb, TCA_TCINDEX_CLASSID, 4, &r->res.classid);
#ifdef CONFIG_NET_CLS_POLICE
		if (r->police) {
			struct rtattr *p_rta = (struct rtattr *) skb->tail;

			RTA_PUT(skb,TCA_TCINDEX_POLICE,0,NULL);
			if (tcf_police_dump(skb,r->police) < 0)
				goto rtattr_failure;
			p_rta->rta_len = skb->tail-(u8 *) p_rta;
		}
#endif
	}
	rta->rta_len = skb->tail-b;
	return skb->len;

rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

struct tcf_proto_ops cls_tcindex_ops = {
	NULL,
	"tcindex",
	tcindex_classify,
	tcindex_init,
	tcindex_destroy,

	tcindex_get,
	tcindex_put,
	tcindex_change,
	tcindex_delete,
	tcindex_walk,
	tcindex_dump
};


#ifdef MODULE
int init_module(void)
{
	return register_tcf_proto_ops(&cls_tcindex_ops);
}

void cleanup_module(void) 
{
	unregister_tcf_proto_ops(&cls_tcindex_ops);
}
#endif
MODULE_LICENSE("GPL");
