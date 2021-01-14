/* $NetBSD: lst.c,v 1.102 2020/12/30 10:03:16 rillig Exp $ */

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

MAKE_RCSID("$NetBSD: lst.c,v 1.102 2020/12/30 10:03:16 rillig Exp $");

static ListNode *
LstNodeNew(ListNode *prev, ListNode *next, void *datum)
{
	ListNode *ln = bmake_malloc(sizeof *ln);

	ln->prev = prev;
	ln->next = next;
	ln->datum = datum;

	return ln;
}

/* Create and initialize a new, empty list. */
List *
Lst_New(void)
{
	List *list = bmake_malloc(sizeof *list);
	Lst_Init(list);
	return list;
}

void
Lst_Done(List *list)
{
	ListNode *ln, *next;

	for (ln = list->first; ln != NULL; ln = next) {
		next = ln->next;
		free(ln);
	}
}

void
Lst_DoneCall(List *list, LstFreeProc freeProc)
{
	ListNode *ln, *next;

	for (ln = list->first; ln != NULL; ln = next) {
		next = ln->next;
		freeProc(ln->datum);
		free(ln);
	}
}

/* Free a list and all its nodes. The node data are not freed though. */
void
Lst_Free(List *list)
{

	Lst_Done(list);
	free(list);
}

/*
 * Destroy a list and free all its resources. The freeProc is called with the
 * datum from each node in turn before the node is freed.
 */
void
Lst_Destroy(List *list, LstFreeProc freeProc)
{
	Lst_DoneCall(list, freeProc);
	free(list);
}

/* Insert a new node with the datum before the given node. */
void
Lst_InsertBefore(List *list, ListNode *ln, void *datum)
{
	ListNode *newNode;

	assert(datum != NULL);

	newNode = LstNodeNew(ln->prev, ln, datum);

	if (ln->prev != NULL)
		ln->prev->next = newNode;
	ln->prev = newNode;

	if (ln == list->first)
		list->first = newNode;
}

/* Add a piece of data at the start of the given list. */
void
Lst_Prepend(List *list, void *datum)
{
	ListNode *ln;

	assert(datum != NULL);

	ln = LstNodeNew(NULL, list->first, datum);

	if (list->first == NULL) {
		list->first = ln;
		list->last = ln;
	} else {
		list->first->prev = ln;
		list->first = ln;
	}
}

/* Add a piece of data at the end of the given list. */
void
Lst_Append(List *list, void *datum)
{
	ListNode *ln;

	assert(datum != NULL);

	ln = LstNodeNew(list->last, NULL, datum);

	if (list->last == NULL) {
		list->first = ln;
		list->last = ln;
	} else {
		list->last->next = ln;
		list->last = ln;
	}
}

/*
 * Remove the given node from the given list.
 * The datum stored in the node must be freed by the caller, if necessary.
 */
void
Lst_Remove(List *list, ListNode *ln)
{
	/* unlink it from its neighbors */
	if (ln->next != NULL)
		ln->next->prev = ln->prev;
	if (ln->prev != NULL)
		ln->prev->next = ln->next;

	/* unlink it from the list */
	if (list->first == ln)
		list->first = ln->next;
	if (list->last == ln)
		list->last = ln->prev;
}

/* Replace the datum in the given node with the new datum. */
void
LstNode_Set(ListNode *ln, void *datum)
{
	assert(datum != NULL);

	ln->datum = datum;
}

/*
 * Replace the datum in the given node with NULL.
 * Having NULL values in a list is unusual though.
 */
void
LstNode_SetNull(ListNode *ln)
{
	ln->datum = NULL;
}

/*
 * Return the first node that contains the given datum, or NULL.
 *
 * Time complexity: O(length(list))
 */
ListNode *
Lst_FindDatum(List *list, const void *datum)
{
	ListNode *ln;

	assert(datum != NULL);

	for (ln = list->first; ln != NULL; ln = ln->next)
		if (ln->datum == datum)
			return ln;

	return NULL;
}

/*
 * Move all nodes from src to the end of dst.
 * The source list becomes empty but is not freed.
 */
void
Lst_MoveAll(List *dst, List *src)
{
	if (src->first != NULL) {
		src->first->prev = dst->last;
		if (dst->last != NULL)
			dst->last->next = src->first;
		else
			dst->first = src->first;

		dst->last = src->last;
	}
}

/* Copy the element data from src to the start of dst. */
void
Lst_PrependAll(List *dst, List *src)
{
	ListNode *ln;

	for (ln = src->last; ln != NULL; ln = ln->prev)
		Lst_Prepend(dst, ln->datum);
}

/* Copy the element data from src to the end of dst. */
void
Lst_AppendAll(List *dst, List *src)
{
	ListNode *ln;

	for (ln = src->first; ln != NULL; ln = ln->next)
		Lst_Append(dst, ln->datum);
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
	v->cap = 10;
	v->itemSize = itemSize;
	v->items = bmake_malloc(v->cap * v->itemSize);
}

/*
 * Add space for a new item to the vector and return a pointer to that space.
 * The returned data is valid until the next modifying operation.
 */
void *
Vector_Push(Vector *v)
{
	if (v->len >= v->cap) {
		v->cap *= 2;
		v->items = bmake_realloc(v->items, v->cap * v->itemSize);
	}
	v->len++;
	return Vector_Get(v, v->len - 1);
}

/*
 * Return the pointer to the last item in the vector.
 * The returned data is valid until the next modifying operation.
 */
void *
Vector_Pop(Vector *v)
{
	assert(v->len > 0);
	v->len--;
	return Vector_Get(v, v->len);
}
