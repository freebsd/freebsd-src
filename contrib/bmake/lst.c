/* $NetBSD: lst.c,v 1.91 2020/10/28 02:43:16 rillig Exp $ */

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "make.h"

MAKE_RCSID("$NetBSD: lst.c,v 1.91 2020/10/28 02:43:16 rillig Exp $");

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#endif

static ListNode *
LstNodeNew(ListNode *prev, ListNode *next, void *datum)
{
    ListNode *node = bmake_malloc(sizeof *node);
    node->prev = prev;
    node->next = next;
    node->datum = datum;
    return node;
}

/* Create and initialize a new, empty list. */
List *
Lst_New(void)
{
    List *list = bmake_malloc(sizeof *list);

    list->first = NULL;
    list->last = NULL;

    return list;
}

/* Free a list and all its nodes. The node data are not freed though. */
void
Lst_Free(List *list)
{
    ListNode *node;
    ListNode *next;

    for (node = list->first; node != NULL; node = next) {
	next = node->next;
	free(node);
    }

    free(list);
}

/* Destroy a list and free all its resources. The freeProc is called with the
 * datum from each node in turn before the node is freed. */
void
Lst_Destroy(List *list, LstFreeProc freeProc)
{
    ListNode *node;
    ListNode *next;

    for (node = list->first; node != NULL; node = next) {
	next = node->next;
	freeProc(node->datum);
	free(node);
    }

    free(list);
}

/*
 * Functions to modify a list
 */

/* Insert a new node with the datum before the given node. */
void
Lst_InsertBefore(List *list, ListNode *node, void *datum)
{
    ListNode *newNode;

    assert(datum != NULL);

    newNode = LstNodeNew(node->prev, node, datum);

    if (node->prev != NULL)
	node->prev->next = newNode;
    node->prev = newNode;

    if (node == list->first)
	list->first = newNode;
}

/* Add a piece of data at the start of the given list. */
void
Lst_Prepend(List *list, void *datum)
{
    ListNode *node;

    assert(datum != NULL);

    node = LstNodeNew(NULL, list->first, datum);

    if (list->first == NULL) {
	list->first = node;
	list->last = node;
    } else {
	list->first->prev = node;
	list->first = node;
    }
}

/* Add a piece of data at the end of the given list. */
void
Lst_Append(List *list, void *datum)
{
    ListNode *node;

    assert(datum != NULL);

    node = LstNodeNew(list->last, NULL, datum);

    if (list->last == NULL) {
	list->first = node;
	list->last = node;
    } else {
	list->last->next = node;
	list->last = node;
    }
}

/* Remove the given node from the given list.
 * The datum stored in the node must be freed by the caller, if necessary. */
void
Lst_Remove(List *list, ListNode *node)
{
    /* unlink it from its neighbors */
    if (node->next != NULL)
	node->next->prev = node->prev;
    if (node->prev != NULL)
	node->prev->next = node->next;

    /* unlink it from the list */
    if (list->first == node)
	list->first = node->next;
    if (list->last == node)
	list->last = node->prev;
}

/* Replace the datum in the given node with the new datum. */
void
LstNode_Set(ListNode *node, void *datum)
{
    assert(datum != NULL);

    node->datum = datum;
}

/* Replace the datum in the given node to NULL.
 * Having NULL values in a list is unusual though. */
void
LstNode_SetNull(ListNode *node)
{
    node->datum = NULL;
}

/*
 * Functions for entire lists
 */

/* Return the first node that contains the given datum, or NULL. */
ListNode *
Lst_FindDatum(List *list, const void *datum)
{
    ListNode *node;

    assert(datum != NULL);

    for (node = list->first; node != NULL; node = node->next)
	if (node->datum == datum)
	    return node;

    return NULL;
}

int
Lst_ForEachUntil(List *list, LstActionUntilProc proc, void *procData)
{
    ListNode *node;
    int result = 0;

    for (node = list->first; node != NULL; node = node->next) {
	result = proc(node->datum, procData);
	if (result != 0)
	    break;
    }
    return result;
}

/* Move all nodes from list2 to the end of list1.
 * List2 is destroyed and freed. */
void
Lst_MoveAll(List *list1, List *list2)
{
    if (list2->first != NULL) {
	list2->first->prev = list1->last;
	if (list1->last != NULL)
	    list1->last->next = list2->first;
	else
	    list1->first = list2->first;

	list1->last = list2->last;
    }
    free(list2);
}

/* Copy the element data from src to the start of dst. */
void
Lst_PrependAll(List *dst, List *src)
{
    ListNode *node;
    for (node = src->last; node != NULL; node = node->prev)
	Lst_Prepend(dst, node->datum);
}

/* Copy the element data from src to the end of dst. */
void
Lst_AppendAll(List *dst, List *src)
{
    ListNode *node;
    for (node = src->first; node != NULL; node = node->next)
	Lst_Append(dst, node->datum);
}

/*
 * for using the list as a queue
 */

/* Add the datum to the tail of the given list. */
void
Lst_Enqueue(List *list, void *datum)
{
    Lst_Append(list, datum);
}

/* Remove and return the datum at the head of the given list. */
void *
Lst_Dequeue(List *list)
{
    void *datum = list->first->datum;
    Lst_Remove(list, list->first);
    assert(datum != NULL);	/* since NULL would mean end of the list */
    return datum;
}

void
Vector_Init(Vector *v, size_t itemSize)
{
    v->len = 0;
    v->priv_cap = 10;
    v->itemSize = itemSize;
    v->items = bmake_malloc(v->priv_cap * v->itemSize);
}

/* Add space for a new item to the vector and return a pointer to that space.
 * The returned data is valid until the next modifying operation. */
void *
Vector_Push(Vector *v)
{
    if (v->len >= v->priv_cap) {
	v->priv_cap *= 2;
	v->items = bmake_realloc(v->items, v->priv_cap * v->itemSize);
    }
    v->len++;
    return Vector_Get(v, v->len - 1);
}

/* Return the pointer to the last item in the vector.
 * The returned data is valid until the next modifying operation. */
void *
Vector_Pop(Vector *v)
{
    assert(v->len > 0);
    v->len--;
    return Vector_Get(v, v->len);
}

void
Vector_Done(Vector *v)
{
    free(v->items);
}
