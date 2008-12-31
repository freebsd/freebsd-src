/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
 * All rights reserved.
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
 *
 * $FreeBSD: src/sys/dev/nxge/include/xge-list.h,v 1.1.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef XGE_LIST_H
#define XGE_LIST_H

#include <dev/nxge/include/xge-debug.h>

__EXTERN_BEGIN_DECLS

/**
 * struct xge_list_t - List item.
 * @prev: Previous list item.
 * @next: Next list item.
 *
 * Item of a bi-directional linked list.
 */
typedef struct xge_list_t {
	struct xge_list_t* prev;
	struct xge_list_t* next;
} xge_list_t;

/**
 * xge_list_init - Initialize linked list.
 * header: first element of the list (head)
 *
 * Initialize linked list.
 * See also: xge_list_t{}.
 */
static inline void xge_list_init (xge_list_t *header)
{
	header->next = header;
	header->prev = header;
}

/**
 * xge_list_is_empty - Is the list empty?
 * header: first element of the list (head)
 *
 * Determine whether the bi-directional list is empty. Return '1' in
 * case of 'empty'.
 * See also: xge_list_t{}.
 */
static inline int xge_list_is_empty(xge_list_t *header)
{
	xge_assert(header != NULL);

	return header->next == header;
}

/**
 * xge_list_first_get - Return the first item from the linked list.
 * header: first element of the list (head)
 *
 * Returns the next item from the header.
 * Returns NULL if the next item is header itself
 * See also: xge_list_remove(), xge_list_insert(), xge_list_t{}.
 */
static inline xge_list_t *xge_list_first_get(xge_list_t *header)
{
	xge_assert(header != NULL);
	xge_assert(header->next != NULL);
	xge_assert(header->prev != NULL);

	if(header->next == header)
	    return NULL;
	else
	    return header->next;
}

/**
 * xge_list_remove - Remove the specified item from the linked list.
 * item: element of the list
 *
 * Remove item from a list.
 * See also: xge_list_insert(), xge_list_t{}.
 */
static inline void xge_list_remove(xge_list_t *item)
{
	xge_assert(item != NULL);
	xge_assert(item->next != NULL);
	xge_assert(item->prev != NULL);

	item->next->prev = item->prev;
	item->prev->next = item->next;
#ifdef XGE_DEBUG_ASSERT
	item->next = item->prev = NULL;
#endif
}

/**
 * xge_list_insert - Insert a new item after the specified item.
 * new_item: new element of the list
 * prev_item: element of the list after which the new element is
 *             inserted
 *
 * Insert new item (new_item) after given item (prev_item).
 * See also: xge_list_remove(), xge_list_insert_before(), xge_list_t{}.
 */
static inline void xge_list_insert (xge_list_t *new_item,
	                xge_list_t *prev_item)
{
	xge_assert(new_item  != NULL);
	xge_assert(prev_item != NULL);
	xge_assert(prev_item->next != NULL);

	new_item->next = prev_item->next;
	new_item->prev = prev_item;
	prev_item->next->prev = new_item;
	prev_item->next = new_item;
}

/**
 * xge_list_insert_before - Insert a new item before the specified item.
 * new_item: new element of the list
 * next_item: element of the list after which the new element is inserted
 *
 * Insert new item (new_item) before given item (next_item).
 */
static inline void xge_list_insert_before (xge_list_t *new_item,
	                       xge_list_t *next_item)
{
	xge_assert(new_item  != NULL);
	xge_assert(next_item != NULL);
	xge_assert(next_item->next != NULL);

	new_item->next = next_item;
	new_item->prev = next_item->prev;
	next_item->prev->next = new_item;
	next_item->prev = new_item;
}

#define xge_list_for_each(_p, _h) \
	for (_p = (_h)->next, xge_os_prefetch(_p->next); _p != (_h); \
	    _p = _p->next, xge_os_prefetch(_p->next))

#define xge_list_for_each_safe(_p, _n, _h) \
	    for (_p = (_h)->next, _n = _p->next; _p != (_h); \
	            _p = _n, _n = _p->next)

#ifdef __GNUC__
/**
 * xge_container_of - Given a member, return the containing structure.
 * @ptr:    the pointer to the member.
 * @type:   the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 *
 * Cast a member of a structure out to the containing structure.
 */
#define xge_container_of(ptr, type, member) ({          \
	     __typeof( ((type *)0)->member ) *__mptr = (ptr);   \
	    (type *)(void *)( (char *)__mptr - ((size_t) &((type *)0)->member) );})
#else
/* type unsafe version */
#define xge_container_of(ptr, type, member) \
	            ((type*)(void*)((char*)(ptr) - ((size_t) &((type *)0)->member)))
#endif

/**
 * xge_offsetof - Offset of the member in the containing structure.
 * @t:  struct name.
 * @m:  the name of the member within the struct.
 *
 * Return the offset of the member @m in the structure @t.
 */
#define xge_offsetof(t, m) ((size_t) (&((t *)0)->m))

__EXTERN_END_DECLS

#endif /* XGE_LIST_H */
