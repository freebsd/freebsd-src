/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2013-2015 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

/*
 * This list data structure is a verbatim copy from wayland-util.h from the
 * Wayland project; except that wl_ prefix has been removed.
 */


/**
 * Doubly linked list implementation. This struct is used for both the list
 * nodes and the list head. Use like this:
 *
 * @code
 *
 *	struct foo {
 *	   struct list list_of_bars; // the list head
 *	};
 *
 *	struct bar {
 *	   struct list link; // links between the bars
 *	};
 *
 *      struct foo *f = zalloc(sizeof *f);
 *      struct bar *b = make_some_bar();
 *
 *	list_init(&f->list_of_bars);
 *	list_append(&f->list_of_bars, &b->link);
 *	list_remove(&b->link);
 * @endcode
 */
struct list {
	struct list *prev;
	struct list *next;
};

/**
 * Initialize a list head. This function *must* be called once for each list
 * head. This function *must not* be called for a node to be added to a
 * list.
 */
void list_init(struct list *list);

/**
 * Insert an element at the front of the list
 */
void list_insert(struct list *list, struct list *elm);
/**
 * Append an element to the  back of the list
 */
void list_append(struct list *list, struct list *elm);

/**
 * Remove an element from list.
 *
 * Removing a list element is only possible once, the caller must track
 * whether the list node has already been removed.
 *
 */
void list_remove(struct list *elm);
/**
 * Returns true if the given list head is an empty list.
 */
bool list_empty(const struct list *list);

/**
 * Return the 'type' parent container struct of 'ptr' of which
 * 'member' is our 'ptr' field. For example:
 *
 * @code
 *     struct foo {			// the parent container struct
 *         uint32_t a;
 *         struct bar bar_member;	// the member field
 *     };
 *
 *     struct foo *f = zalloc(sizeof *f);
 *     struct bar *b = &f->bar_member;
 *     struct foo *f2 = container_of(b, struct foo, bar_member);
 *
 *     assert(f == f2);
 * @endcode
 */
#define container_of(ptr, type, member)					\
	(__typeof__(type) *)((char *)(ptr) -				\
		 offsetof(__typeof__(type), member))

/**
 * Given a list 'head', return the first entry of type 'pos' that has a
 * member 'link'.
 *
 * The 'pos' argument is solely used to determine the type be returned and
 * not modified otherwise. It is common to use the same pointer that the
 * return value of list_first_entry() is assigned to, for example:
 *
 * @code
 *     struct foo {
 *        struct list list_of_bars;
 *     };
 *
 *     struct bar {
 *         struct list link;
 *     }
 *
 *     struct foo *f = get_a_foo();
 *     struct bar *b = 0;  // initialize to avoid static analysis errors
 *     b = list_first_entry(&f->list_of_bars, b, link);
 * @endcode
 */
#define list_first_entry(head, pointer_of_type, member)				\
	container_of((head)->next, __typeof__(*pointer_of_type), member)

/**
 * Given a list 'head', return the first entry of type 'container_type' that
 * has a member 'link'.
 *
 * @code
 *     struct foo {
 *        struct list list_of_bars;
 *     };
 *
 *     struct bar {
 *         struct list link;
 *     }
 *
 *     struct foo *f = get_a_foo();
 *     struct bar *b = list_first_entry(&f->list_of_bars, struct bar, link);
 * @endcode
 */
#define list_first_entry_by_type(head, container_type, member)		\
	container_of((head)->next, container_type, member)

/**
 * Iterate through the list.
 *
 * @code
 *     struct foo *f =  get_a_foo();
 *     struct bar *element;
 *     list_for_each(element, &f->list_of_bars, link) {
 *     }
 * @endcode
 *
 * If a list node needs to be removed during iteration, use
 * list_for_each_safe().
 */
#define list_for_each(pos, head, member)				\
	for (pos = list_first_entry_by_type(head, __typeof__(*pos), member); \
	     &pos->member != (head);					\
	     pos = list_first_entry_by_type(&pos->member, __typeof__(*pos), member))

/**
 * Iterate through the list. Equivalent to list_for_each() but allows
 * calling list_remove() on the element.
 *
 * @code
 *     struct foo *f =  get_a_foo();
 *     struct bar *element;
 *     list_for_each(element, tmp, &f->list_of_bars, link) {
 *          list_remove(&element->link);
 *     }
 * @endcode
 */
#define list_for_each_safe(pos, head, member)				\
	pos = list_first_entry_by_type(head, __typeof__(*pos), member);			\
	for (__typeof__(pos) _tmp = list_first_entry_by_type(&pos->member, __typeof__(*_tmp), member); \
	     &pos->member != (head);					\
	     pos = _tmp,						\
	     _tmp = list_first_entry_by_type(&pos->member, __typeof__(*_tmp), member))
