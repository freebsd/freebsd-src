/* netfilter.c: look after the filters for various protocols. 
 * Heavily influenced by the old firewall.c by David Bonn and Alan Cox.
 *
 * Thanks to Rob `CmdrTaco' Malda for not influencing this code in any
 * way.
 *
 * Rusty Russell (C)2000 -- This code is GPL.
 *
 * February 2000: Modified by James Morris to have 1 queue per protocol.
 * 15-Mar-2000:   Added NF_REPEAT --RR.
 */
#include <linux/config.h>
#include <linux/netfilter.h>
#include <net/protocol.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/brlock.h>
#include <linux/inetdevice.h>
#include <net/sock.h>
#include <net/route.h>
#include <linux/ip.h>

#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>

/* In this code, we can be waiting indefinitely for userspace to
 * service a packet if a hook returns NF_QUEUE.  We could keep a count
 * of skbuffs queued for userspace, and not deregister a hook unless
 * this is zero, but that sucks.  Now, we simply check when the
 * packets come back: if the hook is gone, the packet is discarded. */
#ifdef CONFIG_NETFILTER_DEBUG
#define NFDEBUG(format, args...)  printk(format , ## args)
#else
#define NFDEBUG(format, args...)
#endif

/* Sockopts only registered and called from user context, so
   BR_NETPROTO_LOCK would be overkill.  Also, [gs]etsockopt calls may
   sleep. */
static DECLARE_MUTEX(nf_sockopt_mutex);

struct list_head nf_hooks[NPROTO][NF_MAX_HOOKS];
static LIST_HEAD(nf_sockopts);

/* 
 * A queue handler may be registered for each protocol.  Each is protected by
 * long term mutex.  The handler must provide an an outfn() to accept packets
 * for queueing and must reinject all packets it receives, no matter what.
 */
static struct nf_queue_handler_t {
	nf_queue_outfn_t outfn;
	void *data;
} queue_handler[NPROTO];

int nf_register_hook(struct nf_hook_ops *reg)
{
	struct list_head *i;

	br_write_lock_bh(BR_NETPROTO_LOCK);
	for (i = nf_hooks[reg->pf][reg->hooknum].next; 
	     i != &nf_hooks[reg->pf][reg->hooknum]; 
	     i = i->next) {
		if (reg->priority < ((struct nf_hook_ops *)i)->priority)
			break;
	}
	list_add(&reg->list, i->prev);
	br_write_unlock_bh(BR_NETPROTO_LOCK);
	return 0;
}

void nf_unregister_hook(struct nf_hook_ops *reg)
{
	br_write_lock_bh(BR_NETPROTO_LOCK);
	list_del(&reg->list);
	br_write_unlock_bh(BR_NETPROTO_LOCK);
}

/* Do exclusive ranges overlap? */
static inline int overlap(int min1, int max1, int min2, int max2)
{
	return max1 > min2 && min1 < max2;
}

/* Functions to register sockopt ranges (exclusive). */
int nf_register_sockopt(struct nf_sockopt_ops *reg)
{
	struct list_head *i;
	int ret = 0;

	if (down_interruptible(&nf_sockopt_mutex) != 0)
		return -EINTR;

	for (i = nf_sockopts.next; i != &nf_sockopts; i = i->next) {
		struct nf_sockopt_ops *ops = (struct nf_sockopt_ops *)i;
		if (ops->pf == reg->pf
		    && (overlap(ops->set_optmin, ops->set_optmax, 
				reg->set_optmin, reg->set_optmax)
			|| overlap(ops->get_optmin, ops->get_optmax, 
				   reg->get_optmin, reg->get_optmax))) {
			NFDEBUG("nf_sock overlap: %u-%u/%u-%u v %u-%u/%u-%u\n",
				ops->set_optmin, ops->set_optmax, 
				ops->get_optmin, ops->get_optmax, 
				reg->set_optmin, reg->set_optmax,
				reg->get_optmin, reg->get_optmax);
			ret = -EBUSY;
			goto out;
		}
	}

	list_add(&reg->list, &nf_sockopts);
out:
	up(&nf_sockopt_mutex);
	return ret;
}

void nf_unregister_sockopt(struct nf_sockopt_ops *reg)
{
	/* No point being interruptible: we're probably in cleanup_module() */
 restart:
	down(&nf_sockopt_mutex);
	if (reg->use != 0) {
		/* To be woken by nf_sockopt call... */
		/* FIXME: Stuart Young's name appears gratuitously. */
		set_current_state(TASK_UNINTERRUPTIBLE);
		reg->cleanup_task = current;
		up(&nf_sockopt_mutex);
		schedule();
		goto restart;
	}
	list_del(&reg->list);
	up(&nf_sockopt_mutex);
}

#ifdef CONFIG_NETFILTER_DEBUG
#include <net/ip.h>
#include <net/route.h>
#include <net/tcp.h>
#include <linux/netfilter_ipv4.h>

static void debug_print_hooks_ip(unsigned int nf_debug)
{
	if (nf_debug & (1 << NF_IP_PRE_ROUTING)) {
		printk("PRE_ROUTING ");
		nf_debug ^= (1 << NF_IP_PRE_ROUTING);
	}
	if (nf_debug & (1 << NF_IP_LOCAL_IN)) {
		printk("LOCAL_IN ");
		nf_debug ^= (1 << NF_IP_LOCAL_IN);
	}
	if (nf_debug & (1 << NF_IP_FORWARD)) {
		printk("FORWARD ");
		nf_debug ^= (1 << NF_IP_FORWARD);
	}
	if (nf_debug & (1 << NF_IP_LOCAL_OUT)) {
		printk("LOCAL_OUT ");
		nf_debug ^= (1 << NF_IP_LOCAL_OUT);
	}
	if (nf_debug & (1 << NF_IP_POST_ROUTING)) {
		printk("POST_ROUTING ");
		nf_debug ^= (1 << NF_IP_POST_ROUTING);
	}
	if (nf_debug)
		printk("Crap bits: 0x%04X", nf_debug);
	printk("\n");
}

void nf_dump_skb(int pf, struct sk_buff *skb)
{
	printk("skb: pf=%i %s dev=%s len=%u\n", 
	       pf,
	       skb->sk ? "(owned)" : "(unowned)",
	       skb->dev ? skb->dev->name : "(no dev)",
	       skb->len);
	switch (pf) {
	case PF_INET: {
		const struct iphdr *ip = skb->nh.iph;
		__u32 *opt = (__u32 *) (ip + 1);
		int opti;
		__u16 src_port = 0, dst_port = 0;

		if (ip->protocol == IPPROTO_TCP
		    || ip->protocol == IPPROTO_UDP) {
			struct tcphdr *tcp=(struct tcphdr *)((__u32 *)ip+ip->ihl);
			src_port = ntohs(tcp->source);
			dst_port = ntohs(tcp->dest);
		}
	
		printk("PROTO=%d %u.%u.%u.%u:%hu %u.%u.%u.%u:%hu"
		       " L=%hu S=0x%2.2hX I=%hu F=0x%4.4hX T=%hu",
		       ip->protocol, NIPQUAD(ip->saddr),
		       src_port, NIPQUAD(ip->daddr),
		       dst_port,
		       ntohs(ip->tot_len), ip->tos, ntohs(ip->id),
		       ntohs(ip->frag_off), ip->ttl);

		for (opti = 0; opti < (ip->ihl - sizeof(struct iphdr) / 4); opti++)
			printk(" O=0x%8.8X", *opt++);
		printk("\n");
	}
	}
}

void nf_debug_ip_local_deliver(struct sk_buff *skb)
{
	/* If it's a loopback packet, it must have come through
	 * NF_IP_LOCAL_OUT, NF_IP_RAW_INPUT, NF_IP_PRE_ROUTING and
	 * NF_IP_LOCAL_IN.  Otherwise, must have gone through
	 * NF_IP_RAW_INPUT and NF_IP_PRE_ROUTING.  */
	if (!skb->dev) {
		printk("ip_local_deliver: skb->dev is NULL.\n");
	}
	else if (strcmp(skb->dev->name, "lo") == 0) {
		if (skb->nf_debug != ((1 << NF_IP_LOCAL_OUT)
				      | (1 << NF_IP_POST_ROUTING)
				      | (1 << NF_IP_PRE_ROUTING)
				      | (1 << NF_IP_LOCAL_IN))) {
			printk("ip_local_deliver: bad loopback skb: ");
			debug_print_hooks_ip(skb->nf_debug);
			nf_dump_skb(PF_INET, skb);
		}
	}
	else {
		if (skb->nf_debug != ((1<<NF_IP_PRE_ROUTING)
				      | (1<<NF_IP_LOCAL_IN))) {
			printk("ip_local_deliver: bad non-lo skb: ");
			debug_print_hooks_ip(skb->nf_debug);
			nf_dump_skb(PF_INET, skb);
		}
	}
}

void nf_debug_ip_loopback_xmit(struct sk_buff *newskb)
{
	if (newskb->nf_debug != ((1 << NF_IP_LOCAL_OUT)
				 | (1 << NF_IP_POST_ROUTING))) {
		printk("ip_dev_loopback_xmit: bad owned skb = %p: ", 
		       newskb);
		debug_print_hooks_ip(newskb->nf_debug);
		nf_dump_skb(PF_INET, newskb);
	}
	/* Clear to avoid confusing input check */
	newskb->nf_debug = 0;
}

void nf_debug_ip_finish_output2(struct sk_buff *skb)
{
	/* If it's owned, it must have gone through the
	 * NF_IP_LOCAL_OUT and NF_IP_POST_ROUTING.
	 * Otherwise, must have gone through
	 * NF_IP_PRE_ROUTING, NF_IP_FORWARD and NF_IP_POST_ROUTING.
	 */
	if (skb->sk) {
		if (skb->nf_debug != ((1 << NF_IP_LOCAL_OUT)
				      | (1 << NF_IP_POST_ROUTING))) {
			printk("ip_finish_output: bad owned skb = %p: ", skb);
			debug_print_hooks_ip(skb->nf_debug);
			nf_dump_skb(PF_INET, skb);
		}
	} else {
		if (skb->nf_debug != ((1 << NF_IP_PRE_ROUTING)
				      | (1 << NF_IP_FORWARD)
				      | (1 << NF_IP_POST_ROUTING))) {
			/* Fragments, entunnelled packets, TCP RSTs
                           generated by ipt_REJECT will have no
                           owners, but still may be local */
			if (skb->nf_debug != ((1 << NF_IP_LOCAL_OUT)
					      | (1 << NF_IP_POST_ROUTING))){
				printk("ip_finish_output:"
				       " bad unowned skb = %p: ",skb);
				debug_print_hooks_ip(skb->nf_debug);
				nf_dump_skb(PF_INET, skb);
			}
		}
	}
}
#endif /*CONFIG_NETFILTER_DEBUG*/

/* Call get/setsockopt() */
static int nf_sockopt(struct sock *sk, int pf, int val, 
		      char *opt, int *len, int get)
{
	struct list_head *i;
	struct nf_sockopt_ops *ops;
	int ret;

	if (down_interruptible(&nf_sockopt_mutex) != 0)
		return -EINTR;

	for (i = nf_sockopts.next; i != &nf_sockopts; i = i->next) {
		ops = (struct nf_sockopt_ops *)i;
		if (ops->pf == pf) {
			if (get) {
				if (val >= ops->get_optmin
				    && val < ops->get_optmax) {
					ops->use++;
					up(&nf_sockopt_mutex);
					ret = ops->get(sk, val, opt, len);
					goto out;
				}
			} else {
				if (val >= ops->set_optmin
				    && val < ops->set_optmax) {
					ops->use++;
					up(&nf_sockopt_mutex);
					ret = ops->set(sk, val, opt, *len);
					goto out;
				}
			}
		}
	}
	up(&nf_sockopt_mutex);
	return -ENOPROTOOPT;
	
 out:
	down(&nf_sockopt_mutex);
	ops->use--;
	if (ops->cleanup_task)
		wake_up_process(ops->cleanup_task);
	up(&nf_sockopt_mutex);
	return ret;
}

int nf_setsockopt(struct sock *sk, int pf, int val, char *opt,
		  int len)
{
	return nf_sockopt(sk, pf, val, opt, &len, 0);
}

int nf_getsockopt(struct sock *sk, int pf, int val, char *opt, int *len)
{
	return nf_sockopt(sk, pf, val, opt, len, 1);
}

static unsigned int nf_iterate(struct list_head *head,
			       struct sk_buff **skb,
			       int hook,
			       const struct net_device *indev,
			       const struct net_device *outdev,
			       struct list_head **i,
			       int (*okfn)(struct sk_buff *))
{
	for (*i = (*i)->next; *i != head; *i = (*i)->next) {
		struct nf_hook_ops *elem = (struct nf_hook_ops *)*i;
		switch (elem->hook(hook, skb, indev, outdev, okfn)) {
		case NF_QUEUE:
			return NF_QUEUE;

		case NF_STOLEN:
			return NF_STOLEN;

		case NF_DROP:
			return NF_DROP;

		case NF_REPEAT:
			*i = (*i)->prev;
			break;

#ifdef CONFIG_NETFILTER_DEBUG
		case NF_ACCEPT:
			break;

		default:
			NFDEBUG("Evil return from %p(%u).\n", 
				elem->hook, hook);
#endif
		}
	}
	return NF_ACCEPT;
}

int nf_register_queue_handler(int pf, nf_queue_outfn_t outfn, void *data)
{      
	int ret;

	br_write_lock_bh(BR_NETPROTO_LOCK);
	if (queue_handler[pf].outfn)
		ret = -EBUSY;
	else {
		queue_handler[pf].outfn = outfn;
		queue_handler[pf].data = data;
		ret = 0;
	}
	br_write_unlock_bh(BR_NETPROTO_LOCK);

	return ret;
}

/* The caller must flush their queue before this */
int nf_unregister_queue_handler(int pf)
{
	br_write_lock_bh(BR_NETPROTO_LOCK);
	queue_handler[pf].outfn = NULL;
	queue_handler[pf].data = NULL;
	br_write_unlock_bh(BR_NETPROTO_LOCK);
	return 0;
}

/* 
 * Any packet that leaves via this function must come back 
 * through nf_reinject().
 */
static void nf_queue(struct sk_buff *skb, 
		     struct list_head *elem, 
		     int pf, unsigned int hook,
		     struct net_device *indev,
		     struct net_device *outdev,
		     int (*okfn)(struct sk_buff *))
{
	int status;
	struct nf_info *info;

	if (!queue_handler[pf].outfn) {
		kfree_skb(skb);
		return;
	}

	info = kmalloc(sizeof(*info), GFP_ATOMIC);
	if (!info) {
		if (net_ratelimit())
			printk(KERN_ERR "OOM queueing packet %p\n",
			       skb);
		kfree_skb(skb);
		return;
	}

	*info = (struct nf_info) { 
		(struct nf_hook_ops *)elem, pf, hook, indev, outdev, okfn };

	/* Bump dev refs so they don't vanish while packet is out */
	if (indev) dev_hold(indev);
	if (outdev) dev_hold(outdev);

	status = queue_handler[pf].outfn(skb, info, queue_handler[pf].data);
	if (status < 0) {
		/* James M doesn't say fuck enough. */
		if (indev) dev_put(indev);
		if (outdev) dev_put(outdev);
		kfree(info);
		kfree_skb(skb);
		return;
	}
}

int nf_hook_slow(int pf, unsigned int hook, struct sk_buff *skb,
		 struct net_device *indev,
		 struct net_device *outdev,
		 int (*okfn)(struct sk_buff *))
{
	struct list_head *elem;
	unsigned int verdict;
	int ret = 0;

	/* This stopgap cannot be removed until all the hooks are audited. */
	if (skb_is_nonlinear(skb) && skb_linearize(skb, GFP_ATOMIC) != 0) {
		kfree_skb(skb);
		return -ENOMEM;
	}
	if (skb->ip_summed == CHECKSUM_HW) {
		if (outdev == NULL) {
			skb->ip_summed = CHECKSUM_NONE;
		} else {
			skb_checksum_help(skb);
		}
	}

	/* We may already have this, but read-locks nest anyway */
	br_read_lock_bh(BR_NETPROTO_LOCK);

#ifdef CONFIG_NETFILTER_DEBUG
	if (skb->nf_debug & (1 << hook)) {
		printk("nf_hook: hook %i already set.\n", hook);
		nf_dump_skb(pf, skb);
	}
	skb->nf_debug |= (1 << hook);
#endif

	elem = &nf_hooks[pf][hook];
	verdict = nf_iterate(&nf_hooks[pf][hook], &skb, hook, indev,
			     outdev, &elem, okfn);
	if (verdict == NF_QUEUE) {
		NFDEBUG("nf_hook: Verdict = QUEUE.\n");
		nf_queue(skb, elem, pf, hook, indev, outdev, okfn);
	}

	switch (verdict) {
	case NF_ACCEPT:
		ret = okfn(skb);
		break;

	case NF_DROP:
		kfree_skb(skb);
		ret = -EPERM;
		break;
	}

	br_read_unlock_bh(BR_NETPROTO_LOCK);
	return ret;
}

void nf_reinject(struct sk_buff *skb, struct nf_info *info,
		 unsigned int verdict)
{
	struct list_head *elem = &info->elem->list;
	struct list_head *i;

	/* We don't have BR_NETPROTO_LOCK here */
	br_read_lock_bh(BR_NETPROTO_LOCK);
	for (i = nf_hooks[info->pf][info->hook].next; i != elem; i = i->next) {
		if (i == &nf_hooks[info->pf][info->hook]) {
			/* The module which sent it to userspace is gone. */
			NFDEBUG("%s: module disappeared, dropping packet.\n",
			         __FUNCTION__);
			verdict = NF_DROP;
			break;
		}
	}

	/* Continue traversal iff userspace said ok... */
	if (verdict == NF_REPEAT) {
		elem = elem->prev;
		verdict = NF_ACCEPT;
	}

	if (verdict == NF_ACCEPT) {
		verdict = nf_iterate(&nf_hooks[info->pf][info->hook],
				     &skb, info->hook, 
				     info->indev, info->outdev, &elem,
				     info->okfn);
	}

	switch (verdict) {
	case NF_ACCEPT:
		info->okfn(skb);
		break;

	case NF_QUEUE:
		nf_queue(skb, elem, info->pf, info->hook, 
			 info->indev, info->outdev, info->okfn);
		break;

	case NF_DROP:
		kfree_skb(skb);
		break;
	}
	br_read_unlock_bh(BR_NETPROTO_LOCK);

	/* Release those devices we held, or Alexey will kill me. */
	if (info->indev) dev_put(info->indev);
	if (info->outdev) dev_put(info->outdev);
	
	kfree(info);
	return;
}

#ifdef CONFIG_INET
/* route_me_harder function, used by iptable_nat, iptable_mangle + ip_queue */
int ip_route_me_harder(struct sk_buff **pskb)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	struct rtable *rt;
	struct rt_key key = {};
	struct dst_entry *odst;
	unsigned int hh_len;

	/* some non-standard hacks like ipt_REJECT.c:send_reset() can cause
	 * packets with foreign saddr to be appear on the NF_IP_LOCAL_OUT hook.
	 */
	if (inet_addr_type(iph->saddr) == RTN_LOCAL) {
		key.dst = iph->daddr;
		key.src = iph->saddr;
		key.oif = (*pskb)->sk ? (*pskb)->sk->bound_dev_if : 0;
		key.tos = RT_TOS(iph->tos);
#ifdef CONFIG_IP_ROUTE_FWMARK
		key.fwmark = (*pskb)->nfmark;
#endif
		if (ip_route_output_key(&rt, &key) != 0)
			return -1;

		/* Drop old route. */
		dst_release((*pskb)->dst);
		(*pskb)->dst = &rt->u.dst;
	} else {
		/* non-local src, find valid iif to satisfy
		 * rp-filter when calling ip_route_input. */
		key.dst = iph->saddr;
		if (ip_route_output_key(&rt, &key) != 0)
			return -1;

		odst = (*pskb)->dst;
		if (ip_route_input(*pskb, iph->daddr, iph->saddr,
		                   RT_TOS(iph->tos), rt->u.dst.dev) != 0) {
			dst_release(&rt->u.dst);
			return -1;
		}
		dst_release(&rt->u.dst);
		dst_release(odst);
	}
	
	if ((*pskb)->dst->error)
		return -1;

	/* Change in oif may mean change in hh_len. */
	hh_len = (*pskb)->dst->dev->hard_header_len;
	if (skb_headroom(*pskb) < hh_len) {
		struct sk_buff *nskb;

		nskb = skb_realloc_headroom(*pskb, hh_len);
		if (!nskb)
			return -1;
		if ((*pskb)->sk)
			skb_set_owner_w(nskb, (*pskb)->sk);
		kfree_skb(*pskb);
		*pskb = nskb;
	}

	return 0;
}
#endif /*CONFIG_INET*/

/* This does not belong here, but ipt_REJECT needs it if connection
   tracking in use: without this, connection may not be in hash table,
   and hence manufactured ICMP or RST packets will not be associated
   with it. */
void (*ip_ct_attach)(struct sk_buff *, struct nf_ct_info *);

void __init netfilter_init(void)
{
	int i, h;

	for (i = 0; i < NPROTO; i++) {
		for (h = 0; h < NF_MAX_HOOKS; h++)
			INIT_LIST_HEAD(&nf_hooks[i][h]);
	}
}
