/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _LINUX_LIST_H_
#define _LINUX_LIST_H_

/*
 * Since LIST_HEAD conflicts with the linux definition we must include any
 * FreeBSD header which requires it here so it is resolved with the correct
 * definition prior to the undef.
 */
#include <linux/types.h>

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/cpuset.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/socket.h>
#include <sys/mbuf.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>

#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include <vm/vm.h>
#include <vm/vm_object.h>

#define	prefetch(x)

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

static inline void
INIT_LIST_HEAD(struct list_head *list)
{

	list->next = list->prev = list;
}
 
static inline int
list_empty(const struct list_head *head)
{

	return (head->next == head);
}

static inline void
list_del(struct list_head *entry)
{

	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
}

static inline void
_list_add(struct list_head *new, struct list_head *prev,
    struct list_head *next)
{

	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void
list_del_init(struct list_head *entry)
{	

	list_del(entry);
	INIT_LIST_HEAD(entry);
}

#define	list_entry(ptr, type, field)	container_of(ptr, type, field)

#define list_first_entry(ptr, type, member) \
        list_entry((ptr)->next, type, member)

#define	list_for_each(p, head)						\
	for (p = (head)->next; p != (head); p = p->next)

#define	list_for_each_safe(p, n, head)					\
	for (p = (head)->next, n = p->next; p != (head); p = n, n = p->next)

#define list_for_each_entry(p, h, field)				\
	for (p = list_entry((h)->next, typeof(*p), field); &p->field != (h); \
	    p = list_entry(p->field.next, typeof(*p), field))

#define list_for_each_entry_safe(p, n, h, field)			\
	for (p = list_entry((h)->next, typeof(*p), field), 		\
	    n = list_entry(p->field.next, typeof(*p), field); &p->field != (h);\
	    p = n, n = list_entry(n->field.next, typeof(*n), field))

#define	list_for_each_entry_reverse(p, h, field)			\
	for (p = list_entry((h)->prev, typeof(*p), field); &p->field != (h); \
	    p = list_entry(p->field.prev, typeof(*p), field))

#define	list_for_each_prev(p, h) for (p = (h)->prev; p != (h); p = p->prev)

static inline void
list_add(struct list_head *new, struct list_head *head)
{

	_list_add(new, head, head->next);
}

static inline void
list_add_tail(struct list_head *new, struct list_head *head)
{

	_list_add(new, head->prev, head);
}

static inline void
list_move(struct list_head *list, struct list_head *head)
{

	list_del(list);
	list_add(list, head);
}

static inline void
list_move_tail(struct list_head *entry, struct list_head *head)
{

	list_del(entry);
	list_add_tail(entry, head);
}

static inline void
_list_splice(const struct list_head *list, struct list_head *prev,  
    struct list_head *next)
{
	struct list_head *first;
	struct list_head *last;

	if (list_empty(list))
		return;
	first = list->next;
	last = list->prev;
	first->prev = prev;
	prev->next = first;
	last->next = next;
	next->prev = last;
}

static inline void
list_splice(const struct list_head *list, struct list_head *head)
{

	_list_splice(list, head, head->next);
} 

static inline void
list_splice_tail(struct list_head *list, struct list_head *head)
{

	_list_splice(list, head->prev, head);
}
 
static inline void
list_splice_init(struct list_head *list, struct list_head *head)
{

	_list_splice(list, head, head->next);
	INIT_LIST_HEAD(list);   
}
 
static inline void
list_splice_tail_init(struct list_head *list, struct list_head *head)
{

	_list_splice(list, head->prev, head);
	INIT_LIST_HEAD(list);
}

#undef LIST_HEAD
#define LIST_HEAD(name)	struct list_head name = { &(name), &(name) }


struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};

#define	HLIST_HEAD_INIT { }
#define	HLIST_HEAD(name) struct hlist_head name = HLIST_HEAD_INIT
#define	INIT_HLIST_HEAD(head) (head)->first = NULL
#define	INIT_HLIST_NODE(node)						\
do {									\
	(node)->next = NULL;						\
	(node)->pprev = NULL;						\
} while (0)

static inline int
hlist_unhashed(const struct hlist_node *h)
{

	return !h->pprev;
}

static inline int
hlist_empty(const struct hlist_head *h)
{

	return !h->first;
}

static inline void
hlist_del(struct hlist_node *n)
{

        if (n->next)
                n->next->pprev = n->pprev;
        *n->pprev = n->next;
}

static inline void
hlist_del_init(struct hlist_node *n)
{

	if (hlist_unhashed(n))
		return;
	hlist_del(n);
	INIT_HLIST_NODE(n);
}

static inline void
hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{

	n->next = h->first;
	if (h->first)
		h->first->pprev = &n->next;
	h->first = n;
	n->pprev = &h->first;
}

static inline void
hlist_add_before(struct hlist_node *n, struct hlist_node *next)
{

	n->pprev = next->pprev;
	n->next = next;
	next->pprev = &n->next;
	*(n->pprev) = n;
}
 
static inline void
hlist_add_after(struct hlist_node *n, struct hlist_node *next)
{

	next->next = n->next;
	n->next = next;
	next->pprev = &n->next;
	if (next->next)
		next->next->pprev = &next->next;
}
 
static inline void
hlist_move_list(struct hlist_head *old, struct hlist_head *new)
{

	new->first = old->first;
	if (new->first)
		new->first->pprev = &new->first;
	old->first = NULL;
}
 
#define	hlist_entry(ptr, type, field)	container_of(ptr, type, field)

#define	hlist_for_each(p, head)						\
	for (p = (head)->first; p; p = p->next)

#define	hlist_for_each_safe(p, n, head)					\
	for (p = (head)->first; p && ({ n = p->next; 1; }); p = n)

#define	hlist_for_each_entry(tp, p, head, field)			\
	for (p = (head)->first;						\
	    p ? (tp = hlist_entry(p, typeof(*tp), field)): NULL; p = p->next)
 
#define hlist_for_each_entry_continue(tp, p, field)			\
	for (p = (p)->next;						\
	    p ? (tp = hlist_entry(p, typeof(*tp), field)): NULL; p = p->next)

#define	hlist_for_each_entry_from(tp, p, field)				\
	for (; p ? (tp = hlist_entry(p, typeof(*tp), field)): NULL; p = p->next)

#define	hlist_for_each_entry_safe(tp, p, n, head, field)		\
	for (p = (head)->first;	p ?					\
	    (n = p->next) | (tp = hlist_entry(p, typeof(*tp), field)) :	\
	    NULL; p = n)

#endif /* _LINUX_LIST_H_ */
