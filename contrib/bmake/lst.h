/*	$NetBSD: lst.h,v 1.95 2021/01/03 21:12:03 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * All rights reserved.
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
 *
 *	from: @(#)lst.h	8.1 (Berkeley) 6/6/93
 */

/*
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	from: @(#)lst.h	8.1 (Berkeley) 6/6/93
 */

/* Doubly-linked lists of arbitrary pointers. */

#ifndef MAKE_LST_H
#define MAKE_LST_H

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

/* A doubly-linked list of pointers. */
typedef struct List List;
/* A single node in the doubly-linked list. */
typedef struct ListNode ListNode;

struct ListNode {
	ListNode *prev;		/* previous node in list, or NULL */
	ListNode *next;		/* next node in list, or NULL */
	void *datum;		/* datum associated with this element */
};

struct List {
	ListNode *first;
	ListNode *last;
};

/* Free the datum of a node, called before freeing the node itself. */
typedef void LstFreeProc(void *);

/* Create or destroy a list */

/* Create a new list. */
List *Lst_New(void);
/* Free the list nodes, but not the list itself. */
void Lst_Done(List *);
/* Free the list nodes, freeing the node data using the given function. */
void Lst_DoneCall(List *, LstFreeProc);
/* Free the list, leaving the node data unmodified. */
void Lst_Free(List *);
/* Free the list, freeing the node data using the given function. */
void Lst_Destroy(List *, LstFreeProc);

#define LST_INIT { NULL, NULL }

/* Initialize a list, without memory allocation. */
MAKE_INLINE void
Lst_Init(List *list)
{
    list->first = NULL;
    list->last = NULL;
}

/* Get information about a list */

MAKE_INLINE Boolean
Lst_IsEmpty(List *list)
{ return list->first == NULL; }

/* Find the first node that contains the given datum, or NULL. */
ListNode *Lst_FindDatum(List *, const void *);

/* Modify a list */

/* Insert a datum before the given node. */
void Lst_InsertBefore(List *, ListNode *, void *);
/* Place a datum at the front of the list. */
void Lst_Prepend(List *, void *);
/* Place a datum at the end of the list. */
void Lst_Append(List *, void *);
/* Remove the node from the list. */
void Lst_Remove(List *, ListNode *);
void Lst_PrependAll(List *, List *);
void Lst_AppendAll(List *, List *);
void Lst_MoveAll(List *, List *);

/* Node-specific functions */

/* Replace the value of the node. */
void LstNode_Set(ListNode *, void *);
/* Set the value of the node to NULL. Having NULL in a list is unusual. */
void LstNode_SetNull(ListNode *);

/* Using the list as a queue */

/* Add a datum at the tail of the queue. */
MAKE_INLINE void
Lst_Enqueue(List *list, void *datum) {
	Lst_Append(list, datum);
}

/* Remove the head node of the queue and return its datum. */
void *Lst_Dequeue(List *);

/*
 * A vector is an ordered collection of items, allowing for fast indexed
 * access.
 */
typedef struct Vector {
	void *items;		/* memory holding the items */
	size_t itemSize;	/* size of a single item */
	size_t len;		/* number of actually usable elements */
	size_t cap;		/* capacity */
} Vector;

void Vector_Init(Vector *, size_t);

/*
 * Return the pointer to the given item in the vector.
 * The returned data is valid until the next modifying operation.
 */
MAKE_INLINE void *
Vector_Get(Vector *v, size_t i)
{
	unsigned char *items = v->items;
	return items + i * v->itemSize;
}

void *Vector_Push(Vector *);
void *Vector_Pop(Vector *);

MAKE_INLINE void
Vector_Done(Vector *v) {
	free(v->items);
}

#endif /* MAKE_LST_H */
