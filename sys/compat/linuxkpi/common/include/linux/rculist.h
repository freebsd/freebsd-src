/*-
 * Copyright (c) 2015 François Tigeot
 * Copyright (c) 2016-2020 Mellanox Technologies, Ltd.
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
 *
 * $FreeBSD$
 */

#ifndef _LINUXKPI_LINUX_RCULIST_H_
#define	_LINUXKPI_LINUX_RCULIST_H_

#include <linux/list.h>
#include <linux/rcupdate.h>

#define	list_entry_rcu(ptr, type, member) \
	container_of(READ_ONCE(ptr), type, member)

#define	list_next_rcu(head)	(*((struct list_head **)(&(head)->next)))
#define	list_prev_rcu(head)	(*((struct list_head **)(&(head)->prev)))

#define	list_for_each_entry_rcu(pos, head, member) \
	for (pos = list_entry_rcu((head)->next, typeof(*(pos)), member); \
	     &(pos)->member != (head);					\
	     pos = list_entry_rcu((pos)->member.next, typeof(*(pos)), member))

#define	list_for_each_entry_lockless(pos, head, member) \
	list_for_each_entry_rcu(pos, head, member)

static inline void
linux_list_add_rcu(struct list_head *new, struct list_head *prev,
    struct list_head *next)
{
	new->next = next;
	new->prev = prev;
	rcu_assign_pointer(list_next_rcu(prev), new);
	next->prev = new;
}

static inline void
list_add_rcu(struct list_head *new, struct list_head *head)
{
	linux_list_add_rcu(new, head, head->next);
}

static inline void
list_add_tail_rcu(struct list_head *new, struct list_head *head)
{
	linux_list_add_rcu(new, head->prev, head);
}

static inline void
__list_del_rcu(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	rcu_assign_pointer(list_next_rcu(prev), next);
}

static inline void
__list_del_entry_rcu(struct list_head *entry)
{
	__list_del_rcu(entry->prev, entry->next);
}

static inline void
list_del_rcu(struct list_head *entry)
{
	__list_del_rcu(entry->prev, entry->next);
}

#define	hlist_first_rcu(head)	(*((struct hlist_node **)(&(head)->first)))
#define	hlist_next_rcu(node)	(*((struct hlist_node **)(&(node)->next)))
#define	hlist_pprev_rcu(node)	(*((struct hlist_node **)((node)->pprev)))

static inline void
hlist_add_behind_rcu(struct hlist_node *n, struct hlist_node *prev)
{
	n->next = prev->next;
	n->pprev = &prev->next;
	rcu_assign_pointer(hlist_next_rcu(prev), n);
	if (n->next)
		n->next->pprev = &n->next;
}

#define	hlist_for_each_entry_rcu(pos, head, member)			\
	for (pos = hlist_entry_safe (rcu_dereference_raw(hlist_first_rcu(head)),\
	        typeof(*(pos)), member);				\
	     (pos);							\
	     pos = hlist_entry_safe(rcu_dereference_raw(hlist_next_rcu(	\
			&(pos)->member)), typeof(*(pos)), member))

static inline void
hlist_del_rcu(struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;

	WRITE_ONCE(*pprev, next);
	if (next)
		next->pprev = pprev;
}

static inline void
hlist_add_head_rcu(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node *first = h->first;

	n->next = first;
	n->pprev = &h->first;
	rcu_assign_pointer(hlist_first_rcu(h), n);
	if (first)
		first->pprev = &n->next;
}

static inline void
hlist_del_init_rcu(struct hlist_node *n)
{
	if (!hlist_unhashed(n)) {
		hlist_del_rcu(n);
		n->pprev = NULL;
	}
}

#endif					/* _LINUXKPI_LINUX_RCULIST_H_ */
