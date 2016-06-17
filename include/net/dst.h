/*
 * net/dst.h	Protocol independent destination cache definitions.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#ifndef _NET_DST_H
#define _NET_DST_H

#include <linux/config.h>
#include <net/neighbour.h>

/*
 * 0 - no debugging messages
 * 1 - rare events and bugs (default)
 * 2 - trace mode.
 */
#define RT_CACHE_DEBUG		0

#define DST_GC_MIN	(HZ/10)
#define DST_GC_INC	(HZ/2)
#define DST_GC_MAX	(120*HZ)

struct sk_buff;

struct dst_entry
{
	struct dst_entry        *next;
	atomic_t		__refcnt;	/* client references	*/
	int			__use;
	struct net_device       *dev;
	int			obsolete;
	int			flags;
#define DST_HOST		1
	unsigned long		lastuse;
	unsigned long		expires;

	unsigned		mxlock;
	unsigned		pmtu;
	unsigned		window;
	unsigned		rtt;
	unsigned		rttvar;
	unsigned		ssthresh;
	unsigned		cwnd;
	unsigned		advmss;
	unsigned		reordering;

	unsigned long		rate_last;	/* rate limiting for ICMP */
	unsigned long		rate_tokens;

	int			error;

	struct neighbour	*neighbour;
	struct hh_cache		*hh;

	int			(*input)(struct sk_buff*);
	int			(*output)(struct sk_buff*);

#ifdef CONFIG_NET_CLS_ROUTE
	__u32			tclassid;
#endif

	struct  dst_ops	        *ops;
		
	char			info[0];
};


struct dst_ops
{
	unsigned short		family;
	unsigned short		protocol;
	unsigned		gc_thresh;

	int			(*gc)(void);
	struct dst_entry *	(*check)(struct dst_entry *, __u32 cookie);
	struct dst_entry *	(*reroute)(struct dst_entry *,
					   struct sk_buff *);
	void			(*destroy)(struct dst_entry *);
	struct dst_entry *	(*negative_advice)(struct dst_entry *);
	void			(*link_failure)(struct sk_buff *);
	int			entry_size;

	atomic_t		entries;
	kmem_cache_t 		*kmem_cachep;
};

#ifdef __KERNEL__

static inline void dst_hold(struct dst_entry * dst)
{
	atomic_inc(&dst->__refcnt);
}

static inline
struct dst_entry * dst_clone(struct dst_entry * dst)
{
	if (dst)
		atomic_inc(&dst->__refcnt);
	return dst;
}

static inline
void dst_release(struct dst_entry * dst)
{
	if (dst)
		atomic_dec(&dst->__refcnt);
}

extern void * dst_alloc(struct dst_ops * ops);
extern void __dst_free(struct dst_entry * dst);
extern void dst_destroy(struct dst_entry * dst);

static inline
void dst_free(struct dst_entry * dst)
{
	if (dst->obsolete > 1)
		return;
	if (!atomic_read(&dst->__refcnt)) {
		dst_destroy(dst);
		return;
	}
	__dst_free(dst);
}

static inline void dst_confirm(struct dst_entry *dst)
{
	if (dst)
		neigh_confirm(dst->neighbour);
}

static inline void dst_negative_advice(struct dst_entry **dst_p)
{
	struct dst_entry * dst = *dst_p;
	if (dst && dst->ops->negative_advice)
		*dst_p = dst->ops->negative_advice(dst);
}

static inline void dst_link_failure(struct sk_buff *skb)
{
	struct dst_entry * dst = skb->dst;
	if (dst && dst->ops && dst->ops->link_failure)
		dst->ops->link_failure(skb);
}

static inline void dst_set_expires(struct dst_entry *dst, int timeout)
{
	unsigned long expires = jiffies + timeout;

	if (expires == 0)
		expires = 1;

	if (dst->expires == 0 || (long)(dst->expires - expires) > 0)
		dst->expires = expires;
}

extern void		dst_init(void);

#endif

#endif /* _NET_DST_H */
