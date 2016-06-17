/*
 *	Definitions for the 'struct sk_buff' memory handlers.
 *
 *	Authors:
 *		Alan Cox, <gw4pts@gw4pts.ampr.org>
 *		Florian La Roche, <rzsfl@rz.uni-sb.de>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */
 
#ifndef _LINUX_SKBUFF_H
#define _LINUX_SKBUFF_H

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/cache.h>

#include <asm/atomic.h>
#include <asm/types.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/highmem.h>

#define HAVE_ALLOC_SKB		/* For the drivers to know */
#define HAVE_ALIGNABLE_SKB	/* Ditto 8)		   */
#define SLAB_SKB 		/* Slabified skbuffs 	   */

#define CHECKSUM_NONE 0
#define CHECKSUM_HW 1
#define CHECKSUM_UNNECESSARY 2

#define SKB_DATA_ALIGN(X)	(((X) + (SMP_CACHE_BYTES-1)) & ~(SMP_CACHE_BYTES-1))
#define SKB_MAX_ORDER(X,ORDER)	(((PAGE_SIZE<<(ORDER)) - (X) - sizeof(struct skb_shared_info))&~(SMP_CACHE_BYTES-1))
#define SKB_MAX_HEAD(X)		(SKB_MAX_ORDER((X),0))
#define SKB_MAX_ALLOC		(SKB_MAX_ORDER(0,2))

/* A. Checksumming of received packets by device.
 *
 *	NONE: device failed to checksum this packet.
 *		skb->csum is undefined.
 *
 *	UNNECESSARY: device parsed packet and wouldbe verified checksum.
 *		skb->csum is undefined.
 *	      It is bad option, but, unfortunately, many of vendors do this.
 *	      Apparently with secret goal to sell you new device, when you
 *	      will add new protocol to your host. F.e. IPv6. 8)
 *
 *	HW: the most generic way. Device supplied checksum of _all_
 *	    the packet as seen by netif_rx in skb->csum.
 *	    NOTE: Even if device supports only some protocols, but
 *	    is able to produce some skb->csum, it MUST use HW,
 *	    not UNNECESSARY.
 *
 * B. Checksumming on output.
 *
 *	NONE: skb is checksummed by protocol or csum is not required.
 *
 *	HW: device is required to csum packet as seen by hard_start_xmit
 *	from skb->h.raw to the end and to record the checksum
 *	at skb->h.raw+skb->csum.
 *
 *	Device must show its capabilities in dev->features, set
 *	at device setup time.
 *	NETIF_F_HW_CSUM	- it is clever device, it is able to checksum
 *			  everything.
 *	NETIF_F_NO_CSUM - loopback or reliable single hop media.
 *	NETIF_F_IP_CSUM - device is dumb. It is able to csum only
 *			  TCP/UDP over IPv4. Sigh. Vendors like this
 *			  way by an unknown reason. Though, see comment above
 *			  about CHECKSUM_UNNECESSARY. 8)
 *
 *	Any questions? No questions, good. 		--ANK
 */

#ifdef __i386__
#define NET_CALLER(arg) (*(((void**)&arg)-1))
#else
#define NET_CALLER(arg) __builtin_return_address(0)
#endif

#ifdef CONFIG_NETFILTER
struct nf_conntrack {
	atomic_t use;
	void (*destroy)(struct nf_conntrack *);
};

struct nf_ct_info {
	struct nf_conntrack *master;
};
#endif

struct sk_buff_head {
	/* These two members must be first. */
	struct sk_buff	* next;
	struct sk_buff	* prev;

	__u32		qlen;
	spinlock_t	lock;
};

struct sk_buff;

#define MAX_SKB_FRAGS 6

typedef struct skb_frag_struct skb_frag_t;

struct skb_frag_struct
{
	struct page *page;
	__u16 page_offset;
	__u16 size;
};

/* This data is invariant across clones and lives at
 * the end of the header data, ie. at skb->end.
 */
struct skb_shared_info {
	atomic_t	dataref;
	unsigned int	nr_frags;
	struct sk_buff	*frag_list;
	skb_frag_t	frags[MAX_SKB_FRAGS];
};

struct sk_buff {
	/* These two members must be first. */
	struct sk_buff	* next;			/* Next buffer in list 				*/
	struct sk_buff	* prev;			/* Previous buffer in list 			*/

	struct sk_buff_head * list;		/* List we are on				*/
	struct sock	*sk;			/* Socket we are owned by 			*/
	struct timeval	stamp;			/* Time we arrived				*/
	struct net_device	*dev;		/* Device we arrived on/are leaving by		*/
	struct net_device	*real_dev;	/* For support of point to point protocols 
						   (e.g. 802.3ad) over bonding, we must save the
						   physical device that got the packet before
						   replacing skb->dev with the virtual device.  */

	/* Transport layer header */
	union
	{
		struct tcphdr	*th;
		struct udphdr	*uh;
		struct icmphdr	*icmph;
		struct igmphdr	*igmph;
		struct iphdr	*ipiph;
		struct spxhdr	*spxh;
		unsigned char	*raw;
	} h;

	/* Network layer header */
	union
	{
		struct iphdr	*iph;
		struct ipv6hdr	*ipv6h;
		struct arphdr	*arph;
		struct ipxhdr	*ipxh;
		unsigned char	*raw;
	} nh;
  
	/* Link layer header */
	union 
	{	
	  	struct ethhdr	*ethernet;
	  	unsigned char 	*raw;
	} mac;

	struct  dst_entry *dst;

	/* 
	 * This is the control buffer. It is free to use for every
	 * layer. Please put your private variables there. If you
	 * want to keep them across layers you have to do a skb_clone()
	 * first. This is owned by whoever has the skb queued ATM.
	 */ 
	char		cb[48];	 

	unsigned int 	len;			/* Length of actual data			*/
 	unsigned int 	data_len;
	unsigned int	csum;			/* Checksum 					*/
	unsigned char 	__unused,		/* Dead field, may be reused			*/
			cloned, 		/* head may be cloned (check refcnt to be sure). */
  			pkt_type,		/* Packet class					*/
  			ip_summed;		/* Driver fed us an IP checksum			*/
	__u32		priority;		/* Packet queueing priority			*/
	atomic_t	users;			/* User count - see datagram.c,tcp.c 		*/
	unsigned short	protocol;		/* Packet protocol from driver. 		*/
	unsigned short	security;		/* Security level of packet			*/
	unsigned int	truesize;		/* Buffer size 					*/

	unsigned char	*head;			/* Head of buffer 				*/
	unsigned char	*data;			/* Data head pointer				*/
	unsigned char	*tail;			/* Tail pointer					*/
	unsigned char 	*end;			/* End pointer					*/

	void 		(*destructor)(struct sk_buff *);	/* Destruct function		*/
#ifdef CONFIG_NETFILTER
	/* Can be used for communication between hooks. */
        unsigned long	nfmark;
	/* Cache info */
	__u32		nfcache;
	/* Associated connection, if any */
	struct nf_ct_info *nfct;
#ifdef CONFIG_NETFILTER_DEBUG
        unsigned int nf_debug;
#endif
#endif /*CONFIG_NETFILTER*/

#if defined(CONFIG_HIPPI)
	union{
		__u32	ifield;
	} private;
#endif

#ifdef CONFIG_NET_SCHED
       __u32           tc_index;               /* traffic control index */
#endif
};

#ifdef __KERNEL__
/*
 *	Handling routines are only of interest to the kernel
 */
#include <linux/slab.h>

#include <asm/system.h>

extern void			__kfree_skb(struct sk_buff *skb);
extern struct sk_buff *		alloc_skb(unsigned int size, int priority);
extern void			kfree_skbmem(struct sk_buff *skb);
extern struct sk_buff *		skb_clone(struct sk_buff *skb, int priority);
extern struct sk_buff *		skb_copy(const struct sk_buff *skb, int priority);
extern struct sk_buff *		pskb_copy(struct sk_buff *skb, int gfp_mask);
extern int			pskb_expand_head(struct sk_buff *skb, int nhead, int ntail, int gfp_mask);
extern struct sk_buff *		skb_realloc_headroom(struct sk_buff *skb, unsigned int headroom);
extern struct sk_buff *		skb_copy_expand(const struct sk_buff *skb, 
						int newheadroom,
						int newtailroom,
						int priority);
extern struct sk_buff *		skb_pad(struct sk_buff *skb, int pad);
#define dev_kfree_skb(a)	kfree_skb(a)
extern void	skb_over_panic(struct sk_buff *skb, int len, void *here);
extern void	skb_under_panic(struct sk_buff *skb, int len, void *here);

/* Internal */
#define skb_shinfo(SKB)		((struct skb_shared_info *)((SKB)->end))

/**
 *	skb_queue_empty - check if a queue is empty
 *	@list: queue head
 *
 *	Returns true if the queue is empty, false otherwise.
 */
 
static inline int skb_queue_empty(struct sk_buff_head *list)
{
	return (list->next == (struct sk_buff *) list);
}

/**
 *	skb_get - reference buffer
 *	@skb: buffer to reference
 *
 *	Makes another reference to a socket buffer and returns a pointer
 *	to the buffer.
 */
 
static inline struct sk_buff *skb_get(struct sk_buff *skb)
{
	atomic_inc(&skb->users);
	return skb;
}

/*
 * If users==1, we are the only owner and are can avoid redundant
 * atomic change.
 */
 
/**
 *	kfree_skb - free an sk_buff
 *	@skb: buffer to free
 *
 *	Drop a reference to the buffer and free it if the usage count has
 *	hit zero.
 */
 
static inline void kfree_skb(struct sk_buff *skb)
{
	if (atomic_read(&skb->users) == 1 || atomic_dec_and_test(&skb->users))
		__kfree_skb(skb);
}

/* Use this if you didn't touch the skb state [for fast switching] */
static inline void kfree_skb_fast(struct sk_buff *skb)
{
	if (atomic_read(&skb->users) == 1 || atomic_dec_and_test(&skb->users))
		kfree_skbmem(skb);	
}

/**
 *	skb_cloned - is the buffer a clone
 *	@skb: buffer to check
 *
 *	Returns true if the buffer was generated with skb_clone() and is
 *	one of multiple shared copies of the buffer. Cloned buffers are
 *	shared data so must not be written to under normal circumstances.
 */

static inline int skb_cloned(struct sk_buff *skb)
{
	return skb->cloned && atomic_read(&skb_shinfo(skb)->dataref) != 1;
}

/**
 *	skb_shared - is the buffer shared
 *	@skb: buffer to check
 *
 *	Returns true if more than one person has a reference to this
 *	buffer.
 */
 
static inline int skb_shared(struct sk_buff *skb)
{
	return (atomic_read(&skb->users) != 1);
}

/** 
 *	skb_share_check - check if buffer is shared and if so clone it
 *	@skb: buffer to check
 *	@pri: priority for memory allocation
 *	
 *	If the buffer is shared the buffer is cloned and the old copy
 *	drops a reference. A new clone with a single reference is returned.
 *	If the buffer is not shared the original buffer is returned. When
 *	being called from interrupt status or with spinlocks held pri must
 *	be GFP_ATOMIC.
 *
 *	NULL is returned on a memory allocation failure.
 */
 
static inline struct sk_buff *skb_share_check(struct sk_buff *skb, int pri)
{
	if (skb_shared(skb)) {
		struct sk_buff *nskb;
		nskb = skb_clone(skb, pri);
		kfree_skb(skb);
		return nskb;
	}
	return skb;
}


/*
 *	Copy shared buffers into a new sk_buff. We effectively do COW on
 *	packets to handle cases where we have a local reader and forward
 *	and a couple of other messy ones. The normal one is tcpdumping
 *	a packet thats being forwarded.
 */
 
/**
 *	skb_unshare - make a copy of a shared buffer
 *	@skb: buffer to check
 *	@pri: priority for memory allocation
 *
 *	If the socket buffer is a clone then this function creates a new
 *	copy of the data, drops a reference count on the old copy and returns
 *	the new copy with the reference count at 1. If the buffer is not a clone
 *	the original buffer is returned. When called with a spinlock held or
 *	from interrupt state @pri must be %GFP_ATOMIC
 *
 *	%NULL is returned on a memory allocation failure.
 */
 
static inline struct sk_buff *skb_unshare(struct sk_buff *skb, int pri)
{
	struct sk_buff *nskb;
	if(!skb_cloned(skb))
		return skb;
	nskb=skb_copy(skb, pri);
	kfree_skb(skb);		/* Free our shared copy */
	return nskb;
}

/**
 *	skb_peek
 *	@list_: list to peek at
 *
 *	Peek an &sk_buff. Unlike most other operations you _MUST_
 *	be careful with this one. A peek leaves the buffer on the
 *	list and someone else may run off with it. You must hold
 *	the appropriate locks or have a private queue to do this.
 *
 *	Returns %NULL for an empty list or a pointer to the head element.
 *	The reference count is not incremented and the reference is therefore
 *	volatile. Use with caution.
 */
 
static inline struct sk_buff *skb_peek(struct sk_buff_head *list_)
{
	struct sk_buff *list = ((struct sk_buff *)list_)->next;
	if (list == (struct sk_buff *)list_)
		list = NULL;
	return list;
}

/**
 *	skb_peek_tail
 *	@list_: list to peek at
 *
 *	Peek an &sk_buff. Unlike most other operations you _MUST_
 *	be careful with this one. A peek leaves the buffer on the
 *	list and someone else may run off with it. You must hold
 *	the appropriate locks or have a private queue to do this.
 *
 *	Returns %NULL for an empty list or a pointer to the tail element.
 *	The reference count is not incremented and the reference is therefore
 *	volatile. Use with caution.
 */

static inline struct sk_buff *skb_peek_tail(struct sk_buff_head *list_)
{
	struct sk_buff *list = ((struct sk_buff *)list_)->prev;
	if (list == (struct sk_buff *)list_)
		list = NULL;
	return list;
}

/**
 *	skb_queue_len	- get queue length
 *	@list_: list to measure
 *
 *	Return the length of an &sk_buff queue. 
 */
 
static inline __u32 skb_queue_len(struct sk_buff_head *list_)
{
	return(list_->qlen);
}

static inline void skb_queue_head_init(struct sk_buff_head *list)
{
	spin_lock_init(&list->lock);
	list->prev = (struct sk_buff *)list;
	list->next = (struct sk_buff *)list;
	list->qlen = 0;
}

/*
 *	Insert an sk_buff at the start of a list.
 *
 *	The "__skb_xxxx()" functions are the non-atomic ones that
 *	can only be called with interrupts disabled.
 */

/**
 *	__skb_queue_head - queue a buffer at the list head
 *	@list: list to use
 *	@newsk: buffer to queue
 *
 *	Queue a buffer at the start of a list. This function takes no locks
 *	and you must therefore hold required locks before calling it.
 *
 *	A buffer cannot be placed on two lists at the same time.
 */	
 
static inline void __skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk)
{
	struct sk_buff *prev, *next;

	newsk->list = list;
	list->qlen++;
	prev = (struct sk_buff *)list;
	next = prev->next;
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
}


/**
 *	skb_queue_head - queue a buffer at the list head
 *	@list: list to use
 *	@newsk: buffer to queue
 *
 *	Queue a buffer at the start of the list. This function takes the
 *	list lock and can be used safely with other locking &sk_buff functions
 *	safely.
 *
 *	A buffer cannot be placed on two lists at the same time.
 */	

static inline void skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk)
{
	unsigned long flags;

	spin_lock_irqsave(&list->lock, flags);
	__skb_queue_head(list, newsk);
	spin_unlock_irqrestore(&list->lock, flags);
}

/**
 *	__skb_queue_tail - queue a buffer at the list tail
 *	@list: list to use
 *	@newsk: buffer to queue
 *
 *	Queue a buffer at the end of a list. This function takes no locks
 *	and you must therefore hold required locks before calling it.
 *
 *	A buffer cannot be placed on two lists at the same time.
 */	
 

static inline void __skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk)
{
	struct sk_buff *prev, *next;

	newsk->list = list;
	list->qlen++;
	next = (struct sk_buff *)list;
	prev = next->prev;
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
}

/**
 *	skb_queue_tail - queue a buffer at the list tail
 *	@list: list to use
 *	@newsk: buffer to queue
 *
 *	Queue a buffer at the tail of the list. This function takes the
 *	list lock and can be used safely with other locking &sk_buff functions
 *	safely.
 *
 *	A buffer cannot be placed on two lists at the same time.
 */	

static inline void skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk)
{
	unsigned long flags;

	spin_lock_irqsave(&list->lock, flags);
	__skb_queue_tail(list, newsk);
	spin_unlock_irqrestore(&list->lock, flags);
}

/**
 *	__skb_dequeue - remove from the head of the queue
 *	@list: list to dequeue from
 *
 *	Remove the head of the list. This function does not take any locks
 *	so must be used with appropriate locks held only. The head item is
 *	returned or %NULL if the list is empty.
 */

static inline struct sk_buff *__skb_dequeue(struct sk_buff_head *list)
{
	struct sk_buff *next, *prev, *result;

	prev = (struct sk_buff *) list;
	next = prev->next;
	result = NULL;
	if (next != prev) {
		result = next;
		next = next->next;
		list->qlen--;
		next->prev = prev;
		prev->next = next;
		result->next = NULL;
		result->prev = NULL;
		result->list = NULL;
	}
	return result;
}

/**
 *	skb_dequeue - remove from the head of the queue
 *	@list: list to dequeue from
 *
 *	Remove the head of the list. The list lock is taken so the function
 *	may be used safely with other locking list functions. The head item is
 *	returned or %NULL if the list is empty.
 */

static inline struct sk_buff *skb_dequeue(struct sk_buff_head *list)
{
	unsigned long flags;
	struct sk_buff *result;

	spin_lock_irqsave(&list->lock, flags);
	result = __skb_dequeue(list);
	spin_unlock_irqrestore(&list->lock, flags);
	return result;
}

/*
 *	Insert a packet on a list.
 */

static inline void __skb_insert(struct sk_buff *newsk,
	struct sk_buff * prev, struct sk_buff *next,
	struct sk_buff_head * list)
{
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
	newsk->list = list;
	list->qlen++;
}

/**
 *	skb_insert	-	insert a buffer
 *	@old: buffer to insert before
 *	@newsk: buffer to insert
 *
 *	Place a packet before a given packet in a list. The list locks are taken
 *	and this function is atomic with respect to other list locked calls
 *	A buffer cannot be placed on two lists at the same time.
 */

static inline void skb_insert(struct sk_buff *old, struct sk_buff *newsk)
{
	unsigned long flags;

	spin_lock_irqsave(&old->list->lock, flags);
	__skb_insert(newsk, old->prev, old, old->list);
	spin_unlock_irqrestore(&old->list->lock, flags);
}

/*
 *	Place a packet after a given packet in a list.
 */

static inline void __skb_append(struct sk_buff *old, struct sk_buff *newsk)
{
	__skb_insert(newsk, old, old->next, old->list);
}

/**
 *	skb_append	-	append a buffer
 *	@old: buffer to insert after
 *	@newsk: buffer to insert
 *
 *	Place a packet after a given packet in a list. The list locks are taken
 *	and this function is atomic with respect to other list locked calls.
 *	A buffer cannot be placed on two lists at the same time.
 */


static inline void skb_append(struct sk_buff *old, struct sk_buff *newsk)
{
	unsigned long flags;

	spin_lock_irqsave(&old->list->lock, flags);
	__skb_append(old, newsk);
	spin_unlock_irqrestore(&old->list->lock, flags);
}

/*
 * remove sk_buff from list. _Must_ be called atomically, and with
 * the list known..
 */
 
static inline void __skb_unlink(struct sk_buff *skb, struct sk_buff_head *list)
{
	struct sk_buff * next, * prev;

	list->qlen--;
	next = skb->next;
	prev = skb->prev;
	skb->next = NULL;
	skb->prev = NULL;
	skb->list = NULL;
	next->prev = prev;
	prev->next = next;
}

/**
 *	skb_unlink	-	remove a buffer from a list
 *	@skb: buffer to remove
 *
 *	Place a packet after a given packet in a list. The list locks are taken
 *	and this function is atomic with respect to other list locked calls
 *	
 *	Works even without knowing the list it is sitting on, which can be 
 *	handy at times. It also means that THE LIST MUST EXIST when you 
 *	unlink. Thus a list must have its contents unlinked before it is
 *	destroyed.
 */

static inline void skb_unlink(struct sk_buff *skb)
{
	struct sk_buff_head *list = skb->list;

	if(list) {
		unsigned long flags;

		spin_lock_irqsave(&list->lock, flags);
		if(skb->list == list)
			__skb_unlink(skb, skb->list);
		spin_unlock_irqrestore(&list->lock, flags);
	}
}

/* XXX: more streamlined implementation */

/**
 *	__skb_dequeue_tail - remove from the tail of the queue
 *	@list: list to dequeue from
 *
 *	Remove the tail of the list. This function does not take any locks
 *	so must be used with appropriate locks held only. The tail item is
 *	returned or %NULL if the list is empty.
 */

static inline struct sk_buff *__skb_dequeue_tail(struct sk_buff_head *list)
{
	struct sk_buff *skb = skb_peek_tail(list); 
	if (skb)
		__skb_unlink(skb, list);
	return skb;
}

/**
 *	skb_dequeue - remove from the head of the queue
 *	@list: list to dequeue from
 *
 *	Remove the head of the list. The list lock is taken so the function
 *	may be used safely with other locking list functions. The tail item is
 *	returned or %NULL if the list is empty.
 */

static inline struct sk_buff *skb_dequeue_tail(struct sk_buff_head *list)
{
	unsigned long flags;
	struct sk_buff *result;

	spin_lock_irqsave(&list->lock, flags);
	result = __skb_dequeue_tail(list);
	spin_unlock_irqrestore(&list->lock, flags);
	return result;
}

static inline int skb_is_nonlinear(const struct sk_buff *skb)
{
	return skb->data_len;
}

static inline unsigned int skb_headlen(const struct sk_buff *skb)
{
	return skb->len - skb->data_len;
}

#define SKB_PAGE_ASSERT(skb) do { if (skb_shinfo(skb)->nr_frags) out_of_line_bug(); } while (0)
#define SKB_FRAG_ASSERT(skb) do { if (skb_shinfo(skb)->frag_list) out_of_line_bug(); } while (0)
#define SKB_LINEAR_ASSERT(skb) do { if (skb_is_nonlinear(skb)) out_of_line_bug(); } while (0)

/*
 *	Add data to an sk_buff
 */
 
static inline unsigned char *__skb_put(struct sk_buff *skb, unsigned int len)
{
	unsigned char *tmp=skb->tail;
	SKB_LINEAR_ASSERT(skb);
	skb->tail+=len;
	skb->len+=len;
	return tmp;
}

/**
 *	skb_put - add data to a buffer
 *	@skb: buffer to use 
 *	@len: amount of data to add
 *
 *	This function extends the used data area of the buffer. If this would
 *	exceed the total buffer size the kernel will panic. A pointer to the
 *	first byte of the extra data is returned.
 */
 
static inline unsigned char *skb_put(struct sk_buff *skb, unsigned int len)
{
	unsigned char *tmp=skb->tail;
	SKB_LINEAR_ASSERT(skb);
	skb->tail+=len;
	skb->len+=len;
	if(skb->tail>skb->end) {
		skb_over_panic(skb, len, current_text_addr());
	}
	return tmp;
}

static inline unsigned char *__skb_push(struct sk_buff *skb, unsigned int len)
{
	skb->data-=len;
	skb->len+=len;
	return skb->data;
}

/**
 *	skb_push - add data to the start of a buffer
 *	@skb: buffer to use 
 *	@len: amount of data to add
 *
 *	This function extends the used data area of the buffer at the buffer
 *	start. If this would exceed the total buffer headroom the kernel will
 *	panic. A pointer to the first byte of the extra data is returned.
 */

static inline unsigned char *skb_push(struct sk_buff *skb, unsigned int len)
{
	skb->data-=len;
	skb->len+=len;
	if(skb->data<skb->head) {
		skb_under_panic(skb, len, current_text_addr());
	}
	return skb->data;
}

static inline char *__skb_pull(struct sk_buff *skb, unsigned int len)
{
	skb->len-=len;
	if (skb->len < skb->data_len)
		out_of_line_bug();
	return 	skb->data+=len;
}

/**
 *	skb_pull - remove data from the start of a buffer
 *	@skb: buffer to use 
 *	@len: amount of data to remove
 *
 *	This function removes data from the start of a buffer, returning
 *	the memory to the headroom. A pointer to the next data in the buffer
 *	is returned. Once the data has been pulled future pushes will overwrite
 *	the old data.
 */

static inline unsigned char * skb_pull(struct sk_buff *skb, unsigned int len)
{	
	if (len > skb->len)
		return NULL;
	return __skb_pull(skb,len);
}

extern unsigned char * __pskb_pull_tail(struct sk_buff *skb, int delta);

static inline char *__pskb_pull(struct sk_buff *skb, unsigned int len)
{
	if (len > skb_headlen(skb) &&
	    __pskb_pull_tail(skb, len-skb_headlen(skb)) == NULL)
		return NULL;
	skb->len -= len;
	return 	skb->data += len;
}

static inline unsigned char * pskb_pull(struct sk_buff *skb, unsigned int len)
{	
	if (len > skb->len)
		return NULL;
	return __pskb_pull(skb,len);
}

static inline int pskb_may_pull(struct sk_buff *skb, unsigned int len)
{
	if (len <= skb_headlen(skb))
		return 1;
	if (len > skb->len)
		return 0;
	return (__pskb_pull_tail(skb, len-skb_headlen(skb)) != NULL);
}

/**
 *	skb_headroom - bytes at buffer head
 *	@skb: buffer to check
 *
 *	Return the number of bytes of free space at the head of an &sk_buff.
 */
 
static inline int skb_headroom(const struct sk_buff *skb)
{
	return skb->data-skb->head;
}

/**
 *	skb_tailroom - bytes at buffer end
 *	@skb: buffer to check
 *
 *	Return the number of bytes of free space at the tail of an sk_buff
 */

static inline int skb_tailroom(const struct sk_buff *skb)
{
	return skb_is_nonlinear(skb) ? 0 : skb->end-skb->tail;
}

/**
 *	skb_reserve - adjust headroom
 *	@skb: buffer to alter
 *	@len: bytes to move
 *
 *	Increase the headroom of an empty &sk_buff by reducing the tail
 *	room. This is only allowed for an empty buffer.
 */

static inline void skb_reserve(struct sk_buff *skb, unsigned int len)
{
	skb->data+=len;
	skb->tail+=len;
}

extern int ___pskb_trim(struct sk_buff *skb, unsigned int len, int realloc);

static inline void __skb_trim(struct sk_buff *skb, unsigned int len)
{
	if (!skb->data_len) {
		skb->len = len;
		skb->tail = skb->data+len;
	} else {
		___pskb_trim(skb, len, 0);
	}
}

/**
 *	skb_trim - remove end from a buffer
 *	@skb: buffer to alter
 *	@len: new length
 *
 *	Cut the length of a buffer down by removing data from the tail. If
 *	the buffer is already under the length specified it is not modified.
 */

static inline void skb_trim(struct sk_buff *skb, unsigned int len)
{
	if (skb->len > len) {
		__skb_trim(skb, len);
	}
}


static inline int __pskb_trim(struct sk_buff *skb, unsigned int len)
{
	if (!skb->data_len) {
		skb->len = len;
		skb->tail = skb->data+len;
		return 0;
	} else {
		return ___pskb_trim(skb, len, 1);
	}
}

static inline int pskb_trim(struct sk_buff *skb, unsigned int len)
{
	if (len < skb->len)
		return __pskb_trim(skb, len);
	return 0;
}

/**
 *	skb_orphan - orphan a buffer
 *	@skb: buffer to orphan
 *
 *	If a buffer currently has an owner then we call the owner's
 *	destructor function and make the @skb unowned. The buffer continues
 *	to exist but is no longer charged to its former owner.
 */


static inline void skb_orphan(struct sk_buff *skb)
{
	if (skb->destructor)
		skb->destructor(skb);
	skb->destructor = NULL;
	skb->sk = NULL;
}

/**
 *	skb_purge - empty a list
 *	@list: list to empty
 *
 *	Delete all buffers on an &sk_buff list. Each buffer is removed from
 *	the list and one reference dropped. This function takes the list
 *	lock and is atomic with respect to other list locking functions.
 */


static inline void skb_queue_purge(struct sk_buff_head *list)
{
	struct sk_buff *skb;
	while ((skb=skb_dequeue(list))!=NULL)
		kfree_skb(skb);
}

/**
 *	__skb_purge - empty a list
 *	@list: list to empty
 *
 *	Delete all buffers on an &sk_buff list. Each buffer is removed from
 *	the list and one reference dropped. This function does not take the
 *	list lock and the caller must hold the relevant locks to use it.
 */


static inline void __skb_queue_purge(struct sk_buff_head *list)
{
	struct sk_buff *skb;
	while ((skb=__skb_dequeue(list))!=NULL)
		kfree_skb(skb);
}

/**
 *	__dev_alloc_skb - allocate an skbuff for sending
 *	@length: length to allocate
 *	@gfp_mask: get_free_pages mask, passed to alloc_skb
 *
 *	Allocate a new &sk_buff and assign it a usage count of one. The
 *	buffer has unspecified headroom built in. Users should allocate
 *	the headroom they think they need without accounting for the
 *	built in space. The built in space is used for optimisations.
 *
 *	%NULL is returned in there is no free memory.
 */
 
static inline struct sk_buff *__dev_alloc_skb(unsigned int length,
					      int gfp_mask)
{
	struct sk_buff *skb;

	skb = alloc_skb(length+16, gfp_mask);
	if (skb)
		skb_reserve(skb,16);
	return skb;
}

/**
 *	dev_alloc_skb - allocate an skbuff for sending
 *	@length: length to allocate
 *
 *	Allocate a new &sk_buff and assign it a usage count of one. The
 *	buffer has unspecified headroom built in. Users should allocate
 *	the headroom they think they need without accounting for the
 *	built in space. The built in space is used for optimisations.
 *
 *	%NULL is returned in there is no free memory. Although this function
 *	allocates memory it can be called from an interrupt.
 */
 
static inline struct sk_buff *dev_alloc_skb(unsigned int length)
{
	return __dev_alloc_skb(length, GFP_ATOMIC);
}

/**
 *	skb_cow - copy header of skb when it is required
 *	@skb: buffer to cow
 *	@headroom: needed headroom
 *
 *	If the skb passed lacks sufficient headroom or its data part
 *	is shared, data is reallocated. If reallocation fails, an error
 *	is returned and original skb is not changed.
 *
 *	The result is skb with writable area skb->head...skb->tail
 *	and at least @headroom of space at head.
 */

static inline int
skb_cow(struct sk_buff *skb, unsigned int headroom)
{
	int delta = (headroom > 16 ? headroom : 16) - skb_headroom(skb);

	if (delta < 0)
		delta = 0;

	if (delta || skb_cloned(skb))
		return pskb_expand_head(skb, (delta+15)&~15, 0, GFP_ATOMIC);
	return 0;
}

/**
 *	skb_padto	- pad an skbuff up to a minimal size
 *	@skb: buffer to pad
 *	@len: minimal length
 *
 *	Pads up a buffer to ensure the trailing bytes exist and are
 *	blanked. If the buffer already contains sufficient data it
 *	is untouched. Returns the buffer, which may be a replacement
 *	for the original, or NULL for out of memory - in which case
 *	the original buffer is still freed.
 */
 
static inline struct sk_buff *skb_padto(struct sk_buff *skb, unsigned int len)
{
	unsigned int size = skb->len;
	if(likely(size >= len))
		return skb;
	return skb_pad(skb, len-size);
}

/**
 *	skb_linearize - convert paged skb to linear one
 *	@skb: buffer to linarize
 *	@gfp: allocation mode
 *
 *	If there is no free memory -ENOMEM is returned, otherwise zero
 *	is returned and the old skb data released.  */
int skb_linearize(struct sk_buff *skb, int gfp);

static inline void *kmap_skb_frag(const skb_frag_t *frag)
{
#ifdef CONFIG_HIGHMEM
	if (in_irq())
		out_of_line_bug();

	local_bh_disable();
#endif
	return kmap_atomic(frag->page, KM_SKB_DATA_SOFTIRQ);
}

static inline void kunmap_skb_frag(void *vaddr)
{
	kunmap_atomic(vaddr, KM_SKB_DATA_SOFTIRQ);
#ifdef CONFIG_HIGHMEM
	local_bh_enable();
#endif
}

#define skb_queue_walk(queue, skb) \
		for (skb = (queue)->next;			\
		     (skb != (struct sk_buff *)(queue));	\
		     skb=skb->next)


extern struct sk_buff *		skb_recv_datagram(struct sock *sk,unsigned flags,int noblock, int *err);
extern unsigned int		datagram_poll(struct file *file, struct socket *sock, struct poll_table_struct *wait);
extern int			skb_copy_datagram(const struct sk_buff *from, int offset, char *to,int size);
extern int			skb_copy_datagram_iovec(const struct sk_buff *from, int offset, struct iovec *to,int size);
extern int			skb_copy_and_csum_datagram(const struct sk_buff *skb, int offset, u8 *to, int len, unsigned int *csump);
extern int			skb_copy_and_csum_datagram_iovec(const struct sk_buff *skb, int hlen, struct iovec *iov);
extern void			skb_free_datagram(struct sock * sk, struct sk_buff *skb);

extern unsigned int		skb_checksum(const struct sk_buff *skb, int offset, int len, unsigned int csum);
extern int			skb_copy_bits(const struct sk_buff *skb, int offset, void *to, int len);
extern unsigned int		skb_copy_and_csum_bits(const struct sk_buff *skb, int offset, u8 *to, int len, unsigned int csum);
extern void			skb_copy_and_csum_dev(const struct sk_buff *skb, u8 *to);

extern void skb_init(void);
extern void skb_add_mtu(int mtu);

#ifdef CONFIG_NETFILTER
static inline void
nf_conntrack_put(struct nf_ct_info *nfct)
{
	if (nfct && atomic_dec_and_test(&nfct->master->use))
		nfct->master->destroy(nfct->master);
}
static inline void
nf_conntrack_get(struct nf_ct_info *nfct)
{
	if (nfct)
		atomic_inc(&nfct->master->use);
}
#endif

#endif	/* __KERNEL__ */
#endif	/* _LINUX_SKBUFF_H */
