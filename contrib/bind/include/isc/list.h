/*
 * Copyright (c) 1997 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef LIST_H
#define LIST_H 1

#define LIST(type) struct { type *head, *tail; }
#define INIT_LIST(list) \
	do { (list).head = NULL; (list).tail = NULL; } while (0)

#define LINK(type) struct { type *prev, *next; }
#define INIT_LINK(elt, link) \
	do { \
		(elt)->link.prev = (void *)(-1); \
		(elt)->link.next = (void *)(-1); \
	} while (0)
#define LINKED(elt, link) ((elt)->link.prev != (void *)(-1))

#define HEAD(list) ((list).head)
#define TAIL(list) ((list).tail)
#define EMPTY(list) ((list).head == NULL)

#define PREPEND(list, elt, link) \
	do { \
		if ((list).head != NULL) \
			(list).head->link.prev = (elt); \
		else \
			(list).tail = (elt); \
		(elt)->link.prev = NULL; \
		(elt)->link.next = (list).head; \
		(list).head = (elt); \
	} while (0)

#define APPEND(list, elt, link) \
	do { \
		if ((list).tail != NULL) \
			(list).tail->link.next = (elt); \
		else \
			(list).head = (elt); \
		(elt)->link.prev = (list).tail; \
		(elt)->link.next = NULL; \
		(list).tail = (elt); \
	} while (0)

#define UNLINK(list, elt, link) \
	do { \
		if ((elt)->link.next != NULL) \
			(elt)->link.next->link.prev = (elt)->link.prev; \
		else \
			(list).tail = (elt)->link.prev; \
		if ((elt)->link.prev != NULL) \
			(elt)->link.prev->link.next = (elt)->link.next; \
		else \
			(list).head = (elt)->link.next; \
	} while (0)

#define PREV(elt, link) ((elt)->link.prev)
#define NEXT(elt, link) ((elt)->link.next)

#define INSERT_BEFORE(list, before, elt, link) \
	do { \
		if ((before)->link.prev == NULL) \
			PREPEND(list, elt, link); \
		else { \
			(elt)->link.prev = (before)->link.prev; \
			(before)->link.prev = (elt); \
			(elt)->link.prev->link.next = (elt); \
			(elt)->link.next = (before); \
		} \
	} while (0)

#define INSERT_AFTER(list, after, elt, link) \
	do { \
		if ((after)->link.next == NULL) \
			APPEND(list, elt, link); \
		else { \
			(elt)->link.next = (after)->link.next; \
			(after)->link.next = (elt); \
			(elt)->link.next->link.prev = (elt); \
			(elt)->link.prev = (after); \
		} \
	} while (0)

#define ENQUEUE(list, elt, link) APPEND(list, elt, link)
#define DEQUEUE(list, elt, link) UNLINK(list, elt, link)

#endif /* LIST_H */
