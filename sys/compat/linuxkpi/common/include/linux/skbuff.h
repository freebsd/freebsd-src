/*-
 * Copyright (c) 2020-2025 The FreeBSD Foundation
 * Copyright (c) 2021-2025 Bjoern A. Zeeb
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * NOTE: this socket buffer compatibility code is highly EXPERIMENTAL.
 *       Do not rely on the internals of this implementation.  They are highly
 *       likely to change as we will improve the integration to FreeBSD mbufs.
 */

#ifndef	_LINUXKPI_LINUX_SKBUFF_H
#define	_LINUXKPI_LINUX_SKBUFF_H

#include <linux/kernel.h>
#include <linux/page.h>
#include <linux/dma-mapping.h>
#include <linux/netdev_features.h>
#include <linux/list.h>
#include <linux/gfp.h>
#include <linux/compiler.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/compiler.h>

/*
 * At least the net/intel-irdma-kmod port pulls this header in; likely through
 * if_ether.h (see PR289268).  This means we no longer can rely on
 * IEEE80211_DEBUG (opt_wlan.h) to automatically set SKB_DEBUG.
 */
/* #define	SKB_DEBUG */

#ifdef SKB_DEBUG
#define	DSKB_TODO	0x01
#define	DSKB_IMPROVE	0x02
#define	DSKB_TRACE	0x10
#define	DSKB_TRACEX	0x20
extern int linuxkpi_debug_skb;

#define	SKB_TODO()							\
    if (linuxkpi_debug_skb & DSKB_TODO)					\
	printf("SKB_TODO %s:%d\n", __func__, __LINE__)
#define	SKB_IMPROVE(...)						\
    if (linuxkpi_debug_skb & DSKB_IMPROVE)				\
	printf("SKB_IMPROVE %s:%d\n", __func__, __LINE__)
#define	SKB_TRACE(_s)							\
    if (linuxkpi_debug_skb & DSKB_TRACE)				\
	printf("SKB_TRACE %s:%d %p\n", __func__, __LINE__, _s)
#define	SKB_TRACE2(_s, _p)						\
    if (linuxkpi_debug_skb & DSKB_TRACE)				\
	printf("SKB_TRACE %s:%d %p, %p\n", __func__, __LINE__, _s, _p)
#define	SKB_TRACE_FMT(_s, _fmt, ...)					\
   if (linuxkpi_debug_skb & DSKB_TRACE)					\
	printf("SKB_TRACE %s:%d %p " _fmt "\n", __func__, __LINE__, _s,	\
	    __VA_ARGS__)
#else
#define	SKB_TODO()		do { } while(0)
#define	SKB_IMPROVE(...)	do { } while(0)
#define	SKB_TRACE(_s)		do { } while(0)
#define	SKB_TRACE2(_s, _p)	do { } while(0)
#define	SKB_TRACE_FMT(_s, ...)	do { } while(0)
#endif

enum sk_buff_pkt_type {
	PACKET_BROADCAST,
	PACKET_MULTICAST,
	PACKET_OTHERHOST,
};

struct skb_shared_hwtstamps {
	ktime_t			hwtstamp;
};

#define	NET_SKB_PAD		max(CACHE_LINE_SIZE, 32)
#define	SKB_DATA_ALIGN(_x)	roundup2(_x, CACHE_LINE_SIZE)

struct sk_buff_head {
		/* XXX TODO */
	union {
		struct {
			struct sk_buff		*next;
			struct sk_buff		*prev;
		};
		struct sk_buff_head_l {
			struct sk_buff		*next;
			struct sk_buff		*prev;
		} list;
	};
	size_t			qlen;
	spinlock_t		lock;
};

enum sk_checksum_flags {
	CHECKSUM_NONE			= 0x00,
	CHECKSUM_UNNECESSARY		= 0x01,
	CHECKSUM_PARTIAL		= 0x02,
	CHECKSUM_COMPLETE		= 0x03,
};

struct skb_frag {
		/* XXX TODO */
	struct page		*page;		/* XXX-BZ These three are a wild guess so far! */
	off_t			offset;
	size_t			size;
};
typedef	struct skb_frag	skb_frag_t;

enum skb_shared_info_gso_type {
	SKB_GSO_TCPV4,
	SKB_GSO_TCPV6,
};

struct skb_shared_info {
	enum skb_shared_info_gso_type	gso_type;
	uint16_t			gso_size;
	uint16_t			nr_frags;
	struct sk_buff			*frag_list;
	skb_frag_t			frags[64];	/* XXX TODO, 16xpage? */
};

struct sk_buff {
	/* XXX TODO */
	union {
		/* struct sk_buff_head */
		struct {
			struct sk_buff		*next;
			struct sk_buff		*prev;
		};
		struct list_head	list;
	};

	uint8_t			*head;			/* Head of buffer. */
	uint8_t			*data;			/* Head of data. */
	uint8_t			*tail;			/* End of data. */
	uint8_t			*end;			/* End of buffer. */

	uint32_t		len;		/* ? */
	uint32_t		data_len;	/* ? If we have frags? */
	union {
		__wsum			csum;
		struct {
			uint16_t	csum_offset;
			uint16_t	csum_start;
		};
	};
	uint16_t		protocol;
	uint8_t			ip_summed;		/* 2 bit only. */
	/* uint8_t */

	/* "Scratch" area for layers to store metadata. */
	/* ??? I see sizeof() operations so probably an array. */
	uint8_t			cb[64] __aligned(CACHE_LINE_SIZE);

	struct skb_shared_info	*shinfo	__aligned(CACHE_LINE_SIZE);

	uint32_t		truesize;	/* The total size of all buffers, incl. frags. */
	uint32_t		priority;
	uint16_t		qmap;		/* queue mapping */
	uint16_t		_flags;		/* Internal flags. */
#define	_SKB_FLAGS_SKBEXTFRAG	0x0001
	uint16_t		l3hdroff;	/* network header offset from *head */
	uint16_t		l4hdroff;	/* transport header offset from *head */
	uint16_t		mac_header;	/* offset of mac_header */
	uint16_t		mac_len;	/* Link-layer header length. */
	enum sk_buff_pkt_type	pkt_type;
	refcount_t		refcnt;

	struct net_device	*dev;
	void			*sk;		/* XXX net/sock.h? */

	/* FreeBSD specific bandaid (see linuxkpi_kfree_skb). */
	void			*m;
	void(*m_free_func)(void *);

	/* Force padding to CACHE_LINE_SIZE. */
	uint8_t			__scratch[0] __aligned(CACHE_LINE_SIZE);
};

/* -------------------------------------------------------------------------- */

struct sk_buff *linuxkpi_alloc_skb(size_t, gfp_t);
struct sk_buff *linuxkpi_dev_alloc_skb(size_t, gfp_t);
struct sk_buff *linuxkpi_build_skb(void *, size_t);
void linuxkpi_kfree_skb(struct sk_buff *);

struct sk_buff *linuxkpi_skb_copy(const struct sk_buff *, gfp_t);

/* -------------------------------------------------------------------------- */

static inline struct sk_buff *
alloc_skb(size_t size, gfp_t gfp)
{
	struct sk_buff *skb;

	skb = linuxkpi_alloc_skb(size, gfp);
	SKB_TRACE(skb);
	return (skb);
}

static inline struct sk_buff *
__dev_alloc_skb(size_t len, gfp_t gfp)
{
	struct sk_buff *skb;

	skb = linuxkpi_dev_alloc_skb(len, gfp);
	SKB_IMPROVE();
	SKB_TRACE(skb);
	return (skb);
}

static inline struct sk_buff *
dev_alloc_skb(size_t len)
{
	struct sk_buff *skb;

	skb = __dev_alloc_skb(len, GFP_NOWAIT);
	SKB_IMPROVE();
	SKB_TRACE(skb);
	return (skb);
}

static inline void
kfree_skb(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	linuxkpi_kfree_skb(skb);
}

static inline void
consume_skb(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	kfree_skb(skb);
}

static inline void
dev_kfree_skb(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	kfree_skb(skb);
}

static inline void
dev_kfree_skb_any(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	dev_kfree_skb(skb);
}

static inline void
dev_kfree_skb_irq(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	SKB_IMPROVE("Do we have to defer this?");
	dev_kfree_skb(skb);
}

static inline struct sk_buff *
build_skb(void *data, unsigned int fragsz)
{
	struct sk_buff *skb;

	skb = linuxkpi_build_skb(data, fragsz);
	SKB_TRACE(skb);
	return (skb);
}

/* -------------------------------------------------------------------------- */

static inline bool
skb_is_nonlinear(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	return ((skb->data_len > 0) ? true : false);
}

/* Add headroom; cannot do once there is data in there. */
static inline void
skb_reserve(struct sk_buff *skb, size_t len)
{
	SKB_TRACE(skb);
#if 0
	/* Apparently it is allowed to call skb_reserve multiple times in a row. */
	KASSERT(skb->data == skb->head, ("%s: skb %p not empty head %p data %p "
	    "tail %p\n", __func__, skb, skb->head, skb->data, skb->tail));
#else
	KASSERT(skb->len == 0 && skb->data == skb->tail, ("%s: skb %p not "
	    "empty head %p data %p tail %p len %u\n", __func__, skb,
	    skb->head, skb->data, skb->tail, skb->len));
#endif
	skb->data += len;
	skb->tail += len;
}

/*
 * Remove headroom; return new data pointer; basically make space at the
 * front to copy data in (manually).
 */
static inline void *
__skb_push(struct sk_buff *skb, size_t len)
{
	SKB_TRACE(skb);
	KASSERT(((skb->data - len) >= skb->head), ("%s: skb %p (data %p - "
	    "len %zu) < head %p\n", __func__, skb, skb->data, len, skb->data));
	skb->len  += len;
	skb->data -= len;
	return (skb->data);
}

static inline void *
skb_push(struct sk_buff *skb, size_t len)
{

	SKB_TRACE(skb);
	return (__skb_push(skb, len));
}

/*
 * Length of the data on the skb (without any frags)???
 */
static inline size_t
skb_headlen(struct sk_buff *skb)
{

	SKB_TRACE(skb);
	return (skb->len - skb->data_len);
}


/* Return the end of data (tail pointer). */
static inline uint8_t *
skb_tail_pointer(struct sk_buff *skb)
{

	SKB_TRACE(skb);
	return (skb->tail);
}

/* Return number of bytes available at end of buffer. */
static inline unsigned int
skb_tailroom(struct sk_buff *skb)
{

	SKB_TRACE(skb);
	KASSERT((skb->end - skb->tail) >= 0, ("%s: skb %p tailroom < 0, "
	    "end %p tail %p\n", __func__, skb, skb->end, skb->tail));
	if (unlikely(skb_is_nonlinear(skb)))
		return (0);
	return (skb->end - skb->tail);
}

/* Return number of bytes available at the beginning of buffer. */
static inline unsigned int
skb_headroom(const struct sk_buff *skb)
{
	SKB_TRACE(skb);
	KASSERT((skb->data - skb->head) >= 0, ("%s: skb %p headroom < 0, "
	    "data %p head %p\n", __func__, skb, skb->data, skb->head));
	return (skb->data - skb->head);
}


/*
 * Remove tailroom; return the old tail pointer; basically make space at
 * the end to copy data in (manually).  See also skb_put_data() below.
 */
static inline void *
__skb_put(struct sk_buff *skb, size_t len)
{
	void *s;

	SKB_TRACE(skb);
	KASSERT(((skb->tail + len) <= skb->end), ("%s: skb %p (tail %p + "
	    "len %zu) > end %p, head %p data %p len %u\n", __func__,
	    skb, skb->tail, len, skb->end, skb->head, skb->data, skb->len));

	s = skb_tail_pointer(skb);
	if (len == 0)
		return (s);
	skb->tail += len;
	skb->len += len;
#ifdef SKB_DEBUG
	if (linuxkpi_debug_skb & DSKB_TRACEX)
	printf("%s: skb %p (%u) head %p data %p tail %p end %p, s %p len %zu\n",
	    __func__, skb, skb->len, skb->head, skb->data, skb->tail, skb->end,
	    s, len);
#endif
	return (s);
}

static inline void *
skb_put(struct sk_buff *skb, size_t len)
{

	SKB_TRACE(skb);
	return (__skb_put(skb, len));
}

/* skb_put() + copying data in. */
static inline void *
skb_put_data(struct sk_buff *skb, const void *buf, size_t len)
{
	void *s;

	SKB_TRACE2(skb, buf);
	s = skb_put(skb, len);
	if (len == 0)
		return (s);
	memcpy(s, buf, len);
	return (s);
}

/* skb_put() + filling with zeros. */
static inline void *
skb_put_zero(struct sk_buff *skb, size_t len)
{
	void *s;

	SKB_TRACE(skb);
	s = skb_put(skb, len);
	memset(s, '\0', len);
	return (s);
}

/*
 * Remove len bytes from beginning of data.
 *
 * XXX-BZ ath10k checks for !NULL conditions so I assume this doesn't panic;
 * we return the advanced data pointer so we don't have to keep a temp, correct?
 */
static inline void *
skb_pull(struct sk_buff *skb, size_t len)
{

	SKB_TRACE(skb);
#if 0	/* Apparently this doesn't barf... */
	KASSERT(skb->len >= len, ("%s: skb %p skb->len %u < len %u, data %p\n",
	    __func__, skb, skb->len, len, skb->data));
#endif
	if (skb->len < len)
		return (NULL);
	skb->len -= len;
	skb->data += len;
	return (skb->data);
}

/* Reduce skb data to given length or do nothing if smaller already. */
static inline void
__skb_trim(struct sk_buff *skb, unsigned int len)
{

	SKB_TRACE(skb);
	if (skb->len < len)
		return;

	skb->len = len;
	skb->tail = skb->data + skb->len;
}

static inline void
skb_trim(struct sk_buff *skb, unsigned int len)
{

	return (__skb_trim(skb, len));
}

static inline struct skb_shared_info *
skb_shinfo(struct sk_buff *skb)
{

	SKB_TRACE(skb);
	return (skb->shinfo);
}

static inline void
skb_add_rx_frag(struct sk_buff *skb, int fragno, struct page *page,
    off_t offset, size_t size, unsigned int truesize)
{
	struct skb_shared_info *shinfo;

	SKB_TRACE(skb);
#ifdef SKB_DEBUG
	if (linuxkpi_debug_skb & DSKB_TRACEX)
	printf("%s: skb %p head %p data %p tail %p end %p len %u fragno %d "
	    "page %#jx offset %ju size %zu truesize %u\n", __func__,
	    skb, skb->head, skb->data, skb->tail, skb->end, skb->len, fragno,
	    (uintmax_t)(uintptr_t)linux_page_address(page), (uintmax_t)offset,
	    size, truesize);
#endif

	shinfo = skb_shinfo(skb);
	KASSERT(fragno >= 0 && fragno < nitems(shinfo->frags), ("%s: skb %p "
	    "fragno %d too big\n", __func__, skb, fragno));
	shinfo->frags[fragno].page = page;
	shinfo->frags[fragno].offset = offset;
	shinfo->frags[fragno].size = size;
	shinfo->nr_frags = fragno + 1;
        skb->len += size;
	skb->data_len += size;
        skb->truesize += truesize;
}

/* -------------------------------------------------------------------------- */

#define	skb_queue_walk(_q, skb)						\
	for ((skb) = (_q)->next; (skb) != (struct sk_buff *)(_q);	\
	    (skb) = (skb)->next)

#define	skb_queue_walk_safe(_q, skb, tmp)				\
	for ((skb) = (_q)->next, (tmp) = (skb)->next;			\
	    (skb) != (struct sk_buff *)(_q); (skb) = (tmp), (tmp) = (skb)->next)

#define	skb_list_walk_safe(_q, skb, tmp)				\
	for ((skb) = (_q), (tmp) = ((skb) != NULL) ? (skb)->next ? NULL; \
	    ((skb) != NULL);						\
	    (skb) = (tmp), (tmp) = ((skb) != NULL) ? (skb)->next ? NULL)

static inline bool
skb_queue_empty(const struct sk_buff_head *q)
{
	SKB_TRACE(q);
	return (q->next == (const struct sk_buff *)q);
}

static inline bool
skb_queue_empty_lockless(const struct sk_buff_head *q)
{
	SKB_TRACE(q);
	return (READ_ONCE(q->next) == (const struct sk_buff *)q);
}

static inline void
__skb_queue_head_init(struct sk_buff_head *q)
{
	SKB_TRACE(q);
	q->prev = q->next = (struct sk_buff *)q;
	q->qlen = 0;
}

static inline void
skb_queue_head_init(struct sk_buff_head *q)
{
	SKB_TRACE(q);
	__skb_queue_head_init(q);
	spin_lock_init(&q->lock);
}

static inline void
__skb_insert(struct sk_buff *new, struct sk_buff *prev, struct sk_buff *next,
    struct sk_buff_head *q)
{

	SKB_TRACE_FMT(new, "prev %p next %p q %p", prev, next, q);
	WRITE_ONCE(new->prev, prev);
	WRITE_ONCE(new->next, next);
	WRITE_ONCE(((struct sk_buff_head_l *)next)->prev, new);
	WRITE_ONCE(((struct sk_buff_head_l *)prev)->next, new);
	WRITE_ONCE(q->qlen, q->qlen + 1);
}

static inline void
__skb_queue_after(struct sk_buff_head *q, struct sk_buff *skb,
    struct sk_buff *new)
{

	SKB_TRACE_FMT(q, "skb %p new %p", skb, new);
	__skb_insert(new, skb, ((struct sk_buff_head_l *)skb)->next, q);
}

static inline void
__skb_queue_before(struct sk_buff_head *q, struct sk_buff *skb,
    struct sk_buff *new)
{

	SKB_TRACE_FMT(q, "skb %p new %p", skb, new);
	__skb_insert(new, skb->prev, skb, q);
}

static inline void
__skb_queue_tail(struct sk_buff_head *q, struct sk_buff *new)
{

	SKB_TRACE2(q, new);
	__skb_queue_before(q, (struct sk_buff *)q, new);
}

static inline void
skb_queue_tail(struct sk_buff_head *q, struct sk_buff *new)
{
	unsigned long flags;

	SKB_TRACE2(q, new);
	spin_lock_irqsave(&q->lock, flags);
	__skb_queue_tail(q, new);
	spin_unlock_irqrestore(&q->lock, flags);
}

static inline struct sk_buff *
skb_peek(const struct sk_buff_head *q)
{
	struct sk_buff *skb;

	skb = q->next;
	SKB_TRACE2(q, skb);
	if (skb == (const struct sk_buff *)q)
		return (NULL);
	return (skb);
}

static inline struct sk_buff *
skb_peek_tail(const struct sk_buff_head *q)
{
	struct sk_buff *skb;

	skb = READ_ONCE(q->prev);
	SKB_TRACE2(q, skb);
	if (skb == (const struct sk_buff *)q)
		return (NULL);
	return (skb);
}

static inline void
__skb_unlink(struct sk_buff *skb, struct sk_buff_head *q)
{
	struct sk_buff *p, *n;

	SKB_TRACE2(skb, q);

	WRITE_ONCE(q->qlen, q->qlen - 1);
	p = skb->prev;
	n = skb->next;
	WRITE_ONCE(n->prev, p);
	WRITE_ONCE(p->next, n);
	skb->prev = skb->next = NULL;
}

static inline void
skb_unlink(struct sk_buff *skb, struct sk_buff_head *q)
{
	unsigned long flags;

	SKB_TRACE2(skb, q);
	spin_lock_irqsave(&q->lock, flags);
	__skb_unlink(skb, q);
	spin_unlock_irqrestore(&q->lock, flags);
}

static inline struct sk_buff *
__skb_dequeue(struct sk_buff_head *q)
{
	struct sk_buff *skb;

	skb = skb_peek(q);
	if (skb != NULL)
		__skb_unlink(skb, q);
	SKB_TRACE2(q, skb);
	return (skb);
}

static inline struct sk_buff *
skb_dequeue(struct sk_buff_head *q)
{
	unsigned long flags;
	struct sk_buff *skb;

	spin_lock_irqsave(&q->lock, flags);
	skb = __skb_dequeue(q);
	spin_unlock_irqrestore(&q->lock, flags);
	SKB_TRACE2(q, skb);
	return (skb);
}

static inline struct sk_buff *
__skb_dequeue_tail(struct sk_buff_head *q)
{
	struct sk_buff *skb;

	skb = skb_peek_tail(q);
	if (skb != NULL)
		__skb_unlink(skb, q);
	SKB_TRACE2(q, skb);
	return (skb);
}

static inline struct sk_buff *
skb_dequeue_tail(struct sk_buff_head *q)
{
	unsigned long flags;
	struct sk_buff *skb;

	spin_lock_irqsave(&q->lock, flags);
	skb = __skb_dequeue_tail(q);
	spin_unlock_irqrestore(&q->lock, flags);
	SKB_TRACE2(q, skb);
	return (skb);
}

static inline void
__skb_queue_head(struct sk_buff_head *q, struct sk_buff *skb)
{

	SKB_TRACE2(q, skb);
	__skb_queue_after(q, (struct sk_buff *)q, skb);
}

static inline void
skb_queue_head(struct sk_buff_head *q, struct sk_buff *skb)
{
	unsigned long flags;

	SKB_TRACE2(q, skb);
	spin_lock_irqsave(&q->lock, flags);
	__skb_queue_head(q, skb);
	spin_unlock_irqrestore(&q->lock, flags);
}

static inline uint32_t
skb_queue_len(const struct sk_buff_head *q)
{

	SKB_TRACE(q);
	return (q->qlen);
}

static inline uint32_t
skb_queue_len_lockless(const struct sk_buff_head *q)
{

	SKB_TRACE(q);
	return (READ_ONCE(q->qlen));
}

static inline void
___skb_queue_splice(const struct sk_buff_head *from,
    struct sk_buff *p, struct sk_buff *n)
{
	struct sk_buff *b, *e;

	b = from->next;
	e = from->prev;

	WRITE_ONCE(b->prev, p);
	WRITE_ONCE(((struct sk_buff_head_l *)p)->next, b);
	WRITE_ONCE(e->next, n);
	WRITE_ONCE(((struct sk_buff_head_l *)n)->prev, e);
}

static inline void
skb_queue_splice_init(struct sk_buff_head *from, struct sk_buff_head *to)
{

	SKB_TRACE2(from, to);

	if (skb_queue_empty(from))
		return;

	___skb_queue_splice(from, (struct sk_buff *)to, to->next);
	to->qlen += from->qlen;
	__skb_queue_head_init(from);
}

static inline void
skb_queue_splice_tail_init(struct sk_buff_head *from, struct sk_buff_head *to)
{

	SKB_TRACE2(from, to);

	if (skb_queue_empty(from))
		return;

	___skb_queue_splice(from, to->prev, (struct sk_buff *)to);
	to->qlen += from->qlen;
	__skb_queue_head_init(from);
}


static inline void
__skb_queue_purge(struct sk_buff_head *q)
{
	struct sk_buff *skb;

	SKB_TRACE(q);
        while ((skb = __skb_dequeue(q)) != NULL)
		kfree_skb(skb);
	WARN_ONCE(skb_queue_len(q) != 0, "%s: queue %p not empty: %u",
	    __func__, q, skb_queue_len(q));
}

static inline void
skb_queue_purge(struct sk_buff_head *q)
{
	struct sk_buff_head _q;
	unsigned long flags;

	SKB_TRACE(q);

	if (skb_queue_empty_lockless(q))
		return;

	__skb_queue_head_init(&_q);
	spin_lock_irqsave(&q->lock, flags);
	skb_queue_splice_init(q, &_q);
	spin_unlock_irqrestore(&q->lock, flags);
	__skb_queue_purge(&_q);
}

static inline struct sk_buff *
skb_queue_prev(struct sk_buff_head *q, struct sk_buff *skb)
{

	SKB_TRACE2(q, skb);
	/* XXX what is the q argument good for? */
	return (skb->prev);
}

/* -------------------------------------------------------------------------- */

static inline struct sk_buff *
skb_copy(const struct sk_buff *skb, gfp_t gfp)
{
	struct sk_buff *new;

	new = linuxkpi_skb_copy(skb, gfp);
	SKB_TRACE2(skb, new);
	return (new);
}

static inline uint16_t
skb_checksum(struct sk_buff *skb, int offs, size_t len, int x)
{
	SKB_TRACE(skb);
	SKB_TODO();
	return (0xffff);
}

static inline int
skb_checksum_start_offset(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	SKB_TODO();
	return (-1);
}

static inline dma_addr_t
skb_frag_dma_map(struct device *dev, const skb_frag_t *frag, int x,
    size_t fragsz, enum dma_data_direction dir)
{
	SKB_TRACE2(frag, dev);
	SKB_TODO();
	return (-1);
}

static inline size_t
skb_frag_size(const skb_frag_t *frag)
{
	SKB_TRACE(frag);
	return (frag->size);
}

#define	skb_walk_frags(_skb, _frag)					\
	for ((_frag) = (_skb); false; (_frag)++)

static inline void
skb_checksum_help(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	SKB_TODO();
}

static inline bool
skb_ensure_writable(struct sk_buff *skb, size_t off)
{
	SKB_TRACE(skb);
	SKB_TODO();
	return (false);
}

static inline void *
skb_frag_address(const skb_frag_t *frag)
{
	SKB_TRACE(frag);
	return (page_address(frag->page + frag->offset));
}

static inline void
skb_free_frag(void *frag)
{

	page_frag_free(frag);
}

static inline struct sk_buff *
skb_gso_segment(struct sk_buff *skb, netdev_features_t netdev_flags)
{
	SKB_TRACE(skb);
	SKB_TODO();
	return (NULL);
}

static inline bool
skb_is_gso(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	SKB_IMPROVE("Really a TODO but get it away from logging");
	return (false);
}

static inline void
skb_mark_not_on_list(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	skb->next = NULL;
}

static inline void
skb_reset_transport_header(struct sk_buff *skb)
{

	SKB_TRACE(skb);
	skb->l4hdroff = skb->data - skb->head;
}

static inline uint8_t *
skb_transport_header(struct sk_buff *skb)
{

	SKB_TRACE(skb);
        return (skb->head + skb->l4hdroff);
}

static inline uint8_t *
skb_network_header(struct sk_buff *skb)
{

	SKB_TRACE(skb);
        return (skb->head + skb->l3hdroff);
}

static inline int
__skb_linearize(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	SKB_TODO();
	return (-ENXIO);
}

static inline int
skb_linearize(struct sk_buff *skb)
{
	return (skb_is_nonlinear(skb) ? __skb_linearize(skb) : 0);
}

static inline int
pskb_expand_head(struct sk_buff *skb, int x, int len, gfp_t gfp)
{
	SKB_TRACE(skb);
	SKB_TODO();
	return (-ENXIO);
}

/* Not really seen this one but need it as symmetric accessor function. */
static inline void
skb_set_queue_mapping(struct sk_buff *skb, uint16_t qmap)
{

	SKB_TRACE_FMT(skb, "qmap %u", qmap);
	skb->qmap = qmap;
}

static inline uint16_t
skb_get_queue_mapping(struct sk_buff *skb)
{

	SKB_TRACE_FMT(skb, "qmap %u", skb->qmap);
	return (skb->qmap);
}

static inline void
skb_copy_header(struct sk_buff *to, const struct sk_buff *from)
{
	SKB_TRACE2(to, from);
	SKB_TODO();
}

static inline bool
skb_header_cloned(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	SKB_TODO();
	return (true);
}

static inline uint8_t *
skb_mac_header(const struct sk_buff *skb)
{
	SKB_TRACE(skb);
	return (skb->head + skb->mac_header);
}

static inline void
skb_reset_mac_header(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	skb->mac_header = skb->data - skb->head;
}

static inline void
skb_set_mac_header(struct sk_buff *skb, const size_t len)
{
	SKB_TRACE(skb);
	skb_reset_mac_header(skb);
	skb->mac_header += len;
}

static inline struct skb_shared_hwtstamps *
skb_hwtstamps(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	SKB_TODO();
	return (NULL);
}

static inline void
skb_orphan(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	SKB_TODO();
}

static inline __wsum
csum_unfold(__sum16 sum)
{
	return (sum);
}

static __inline void
skb_postpush_rcsum(struct sk_buff *skb, const void *data, size_t len)
{
	SKB_TODO();
}

static inline void
skb_reset_tail_pointer(struct sk_buff *skb)
{

	SKB_TRACE(skb);
#ifdef SKB_DOING_OFFSETS_US_NOT
	skb->tail = (uint8_t *)(uintptr_t)(skb->data - skb->head);
#endif
	skb->tail = skb->data;
	SKB_TRACE(skb);
}

static inline struct sk_buff *
skb_get(struct sk_buff *skb)
{

	SKB_TRACE(skb);
	refcount_inc(&skb->refcnt);
	return (skb);
}

static inline struct sk_buff *
skb_realloc_headroom(struct sk_buff *skb, unsigned int headroom)
{

	SKB_TODO();
	return (NULL);
}

static inline void
skb_copy_from_linear_data(const struct sk_buff *skb, void *dst, size_t len)
{

	SKB_TRACE(skb);
	/* Let us just hope the destination has len space ... */
	memcpy(dst, skb->data, len);
}

static inline int
skb_pad(struct sk_buff *skb, int pad)
{

	SKB_TRACE(skb);
	SKB_TODO();
	return (-1);
}

static inline void
skb_list_del_init(struct sk_buff *skb)
{

	SKB_TRACE(skb);
	__list_del_entry(&skb->list);
	skb_mark_not_on_list(skb);
}

static inline void
napi_consume_skb(struct sk_buff *skb, int budget)
{

	SKB_TRACE(skb);
	SKB_TODO();
}

static inline struct sk_buff *
napi_build_skb(void *data, size_t len)
{

	SKB_TODO();
	return (NULL);
}

static inline uint32_t
skb_get_hash(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	SKB_TODO();
	return (0);
}

static inline void
skb_mark_for_recycle(struct sk_buff *skb)
{
	SKB_TRACE(skb);
	/* page_pool */
	SKB_TODO();
}

static inline int
skb_cow_head(struct sk_buff *skb, unsigned int headroom)
{
	SKB_TRACE(skb);
	SKB_TODO();
	return (-1);
}

/* Misplaced here really but sock comes from skbuff. */
#define	sk_pacing_shift_update(sock, n)

#define	SKB_WITH_OVERHEAD(_s)						\
	(_s) - ALIGN(sizeof(struct skb_shared_info), CACHE_LINE_SIZE)

#endif	/* _LINUXKPI_LINUX_SKBUFF_H */
