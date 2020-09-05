/* $NetBSD: lst.c,v 1.60 2020/08/31 05:56:02 rillig Exp $ */

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#endif

#include "make.h"

#ifndef MAKE_NATIVE
static char rcsid[] = "$NetBSD: lst.c,v 1.60 2020/08/31 05:56:02 rillig Exp $";
#else
#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: lst.c,v 1.60 2020/08/31 05:56:02 rillig Exp $");
#endif /* not lint */
#endif

struct ListNode {
    struct ListNode *prev;	/* previous element in list */
    struct ListNode *next;	/* next in list */
    uint8_t useCount;		/* Count of functions using the node.
				 * node may not be deleted until count
				 * goes to 0 */
    Boolean deleted;		/* List node should be removed when done */
    union {
	void *datum;		/* datum associated with this element */
	const GNode *gnode;	/* alias, just for debugging */
	const char *str;	/* alias, just for debugging */
    };
};

typedef enum {
    Head, Middle, Tail, Unknown
} Where;

struct List {
    LstNode first;		/* first node in list */
    LstNode last;		/* last node in list */

    /* fields for sequential access */
    Boolean isOpen;		/* true if list has been Lst_Open'ed */
    Where lastAccess;		/* Where in the list the last access was */
    LstNode curr;		/* current node, if open. NULL if
				 * *just* opened */
    LstNode prev;		/* Previous node, if open. Used by Lst_Remove */
};

/* Allocate and initialize a list node.
 *
 * The fields 'prev' and 'next' must be initialized by the caller.
 */
static LstNode
LstNodeNew(void *datum)
{
    LstNode node = bmake_malloc(sizeof *node);
    node->useCount = 0;
    node->deleted = FALSE;
    node->datum = datum;
    return node;
}

static Boolean
LstIsEmpty(Lst list)
{
    return list->first == NULL;
}

/* Create and initialize a new, empty list. */
Lst
Lst_Init(void)
{
    Lst list = bmake_malloc(sizeof *list);

    list->first = NULL;
    list->last = NULL;
    list->isOpen = FALSE;
    list->lastAccess = Unknown;

    return list;
}

/* Duplicate an entire list, usually by copying the datum pointers.
 * If copyProc is given, that function is used to create the new datum from the
 * old datum, usually by creating a copy of it. */
Lst
Lst_Copy(Lst list, LstCopyProc copyProc)
{
    Lst newList;
    LstNode node;

    assert(list != NULL);

    newList = Lst_Init();

    for (node = list->first; node != NULL; node = node->next) {
	void *datum = copyProc != NULL ? copyProc(node->datum) : node->datum;
	Lst_Append(newList, datum);
    }

    return newList;
}

/* Free a list and all its nodes. The list data itself are not freed though. */
void
Lst_Free(Lst list)
{
    LstNode node;
    LstNode next;

    assert(list != NULL);

    for (node = list->first; node != NULL; node = next) {
	next = node->next;
	free(node);
    }

    free(list);
}

/* Destroy a list and free all its resources. The freeProc is called with the
 * datum from each node in turn before the node is freed. */
void
Lst_Destroy(Lst list, LstFreeProc freeProc)
{
    LstNode node;
    LstNode next;

    assert(list != NULL);
    assert(freeProc != NULL);

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

/* Insert a new node with the given piece of data before the given node in the
 * given list. */
void
Lst_InsertBefore(Lst list, LstNode node, void *datum)
{
    LstNode newNode;

    assert(list != NULL);
    assert(!LstIsEmpty(list));
    assert(node != NULL);
    assert(datum != NULL);

    newNode = LstNodeNew(datum);
    newNode->prev = node->prev;
    newNode->next = node;

    if (node->prev != NULL) {
	node->prev->next = newNode;
    }
    node->prev = newNode;

    if (node == list->first) {
	list->first = newNode;
    }
}

/* Add a piece of data at the start of the given list. */
void
Lst_Prepend(Lst list, void *datum)
{
    LstNode node;

    assert(list != NULL);
    assert(datum != NULL);

    node = LstNodeNew(datum);
    node->prev = NULL;
    node->next = list->first;

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
Lst_Append(Lst list, void *datum)
{
    LstNode node;

    assert(list != NULL);
    assert(datum != NULL);

    node = LstNodeNew(datum);
    node->prev = list->last;
    node->next = NULL;

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
Lst_Remove(Lst list, LstNode node)
{
    assert(list != NULL);
    assert(node != NULL);

    /*
     * unlink it from the list
     */
    if (node->next != NULL) {
	node->next->prev = node->prev;
    }
    if (node->prev != NULL) {
	node->prev->next = node->next;
    }

    /*
     * if either the first or last of the list point to this node,
     * adjust them accordingly
     */
    if (list->first == node) {
	list->first = node->next;
    }
    if (list->last == node) {
	list->last = node->prev;
    }

    /*
     * Sequential access stuff. If the node we're removing is the current
     * node in the list, reset the current node to the previous one. If the
     * previous one was non-existent (prev == NULL), we set the
     * end to be Unknown, since it is.
     */
    if (list->isOpen && list->curr == node) {
	list->curr = list->prev;
	if (list->curr == NULL) {
	    list->lastAccess = Unknown;
	}
    }

    /*
     * note that the datum is unmolested. The caller must free it as
     * necessary and as expected.
     */
    if (node->useCount == 0) {
	free(node);
    } else {
	node->deleted = TRUE;
    }
}

/* Replace the datum in the given node with the new datum. */
void
LstNode_Set(LstNode node, void *datum)
{
    assert(node != NULL);
    assert(datum != NULL);

    node->datum = datum;
}

/* Replace the datum in the given node to NULL. */
void
LstNode_SetNull(LstNode node)
{
    assert(node != NULL);

    node->datum = NULL;
}


/*
 * Node-specific functions
 */

/* Return the first node from the given list, or NULL if the list is empty. */
LstNode
Lst_First(Lst list)
{
    assert(list != NULL);

    return list->first;
}

/* Return the last node from the given list, or NULL if the list is empty. */
LstNode
Lst_Last(Lst list)
{
    assert(list != NULL);

    return list->last;
}

/* Return the successor to the given node on its list, or NULL. */
LstNode
LstNode_Next(LstNode node)
{
    assert(node != NULL);

    return node->next;
}

/* Return the predecessor to the given node on its list, or NULL. */
LstNode
LstNode_Prev(LstNode node)
{
    assert(node != NULL);
    return node->prev;
}

/* Return the datum stored in the given node. */
void *
LstNode_Datum(LstNode node)
{
    assert(node != NULL);
    return node->datum;
}


/*
 * Functions for entire lists
 */

/* Return TRUE if the given list is empty. */
Boolean
Lst_IsEmpty(Lst list)
{
    assert(list != NULL);

    return LstIsEmpty(list);
}

/* Return the first node from the list for which the match function returns
 * TRUE, or NULL if none of the nodes matched. */
LstNode
Lst_Find(Lst list, LstFindProc match, const void *matchArgs)
{
    return Lst_FindFrom(list, Lst_First(list), match, matchArgs);
}

/* Return the first node from the list, starting at the given node, for which
 * the match function returns TRUE, or NULL if none of the nodes matches.
 *
 * The start node may be NULL, in which case nothing is found. This allows
 * for passing Lst_First or LstNode_Next as the start node. */
LstNode
Lst_FindFrom(Lst list, LstNode node, LstFindProc match, const void *matchArgs)
{
    LstNode tln;

    assert(list != NULL);
    assert(match != NULL);

    for (tln = node; tln != NULL; tln = tln->next) {
	if (match(tln->datum, matchArgs))
	    return tln;
    }

    return NULL;
}

/* Return the first node that contains the given datum, or NULL. */
LstNode
Lst_FindDatum(Lst list, const void *datum)
{
    LstNode node;

    assert(list != NULL);
    assert(datum != NULL);

    for (node = list->first; node != NULL; node = node->next) {
	if (node->datum == datum) {
	    return node;
	}
    }

    return NULL;
}

/* Apply the given function to each element of the given list. The function
 * should return 0 if traversal should continue and non-zero if it should
 * abort. */
int
Lst_ForEach(Lst list, LstActionProc proc, void *procData)
{
    if (LstIsEmpty(list))
	return 0;		/* XXX: Document what this value means. */
    return Lst_ForEachFrom(list, Lst_First(list), proc, procData);
}

/* Apply the given function to each element of the given list, starting from
 * the given node. The function should return 0 if traversal should continue,
 * and non-zero if it should abort. */
int
Lst_ForEachFrom(Lst list, LstNode node,
		 LstActionProc proc, void *procData)
{
    LstNode tln = node;
    LstNode next;
    Boolean done;
    int result;

    assert(list != NULL);
    assert(node != NULL);
    assert(proc != NULL);

    do {
	/*
	 * Take care of having the current element deleted out from under
	 * us.
	 */

	next = tln->next;

	/*
	 * We're done with the traversal if
	 *  - the next node to examine doesn't exist and
	 *  - nothing's been added after the current node (check this
	 *    after proc() has been called).
	 */
	done = next == NULL;

	tln->useCount++;
	result = (*proc)(tln->datum, procData);
	tln->useCount--;

	/*
	 * Now check whether a node has been added.
	 * Note: this doesn't work if this node was deleted before
	 *       the new node was added.
	 */
	if (next != tln->next) {
	    next = tln->next;
	    done = 0;
	}

	if (tln->deleted) {
	    free((char *)tln);
	}
	tln = next;
    } while (!result && !LstIsEmpty(list) && !done);

    return result;
}

/* Move all nodes from list2 to the end of list1.
 * List2 is destroyed and freed. */
void
Lst_MoveAll(Lst list1, Lst list2)
{
    assert(list1 != NULL);
    assert(list2 != NULL);

    if (list2->first != NULL) {
	list2->first->prev = list1->last;
	if (list1->last != NULL) {
	    list1->last->next = list2->first;
	} else {
	    list1->first = list2->first;
	}
	list1->last = list2->last;
    }
    free(list2);
}

/* Copy the element data from src to the start of dst. */
void
Lst_PrependAll(Lst dst, Lst src)
{
    LstNode node;
    for (node = src->last; node != NULL; node = node->prev)
	Lst_Prepend(dst, node->datum);
}

/* Copy the element data from src to the end of dst. */
void
Lst_AppendAll(Lst dst, Lst src)
{
    LstNode node;
    for (node = src->first; node != NULL; node = node->next)
	Lst_Append(dst, node->datum);
}

/*
 * these functions are for dealing with a list as a table, of sorts.
 * An idea of the "current element" is kept and used by all the functions
 * between Lst_Open() and Lst_Close().
 *
 * The sequential functions access the list in a slightly different way.
 * CurPtr points to their idea of the current node in the list and they
 * access the list based on it.
 */

/* Open a list for sequential access. A list can still be searched, etc.,
 * without confusing these functions. */
void
Lst_Open(Lst list)
{
    assert(list != NULL);
    assert(!list->isOpen);

    list->isOpen = TRUE;
    list->lastAccess = LstIsEmpty(list) ? Head : Unknown;
    list->curr = NULL;
}

/* Return the next node for the given list, or NULL if the end has been
 * reached. */
LstNode
Lst_Next(Lst list)
{
    LstNode node;

    assert(list != NULL);
    assert(list->isOpen);

    list->prev = list->curr;

    if (list->curr == NULL) {
	if (list->lastAccess == Unknown) {
	    /*
	     * If we're just starting out, lastAccess will be Unknown.
	     * Then we want to start this thing off in the right
	     * direction -- at the start with lastAccess being Middle.
	     */
	    list->curr = node = list->first;
	    list->lastAccess = Middle;
	} else {
	    node = NULL;
	    list->lastAccess = Tail;
	}
    } else {
	node = list->curr->next;
	list->curr = node;

	if (node == list->first || node == NULL) {
	    /*
	     * If back at the front, then we've hit the end...
	     */
	    list->lastAccess = Tail;
	} else {
	    /*
	     * Reset to Middle if gone past first.
	     */
	    list->lastAccess = Middle;
	}
    }

    return node;
}

/* Close a list which was opened for sequential access. */
void
Lst_Close(Lst list)
{
    assert(list != NULL);
    assert(list->isOpen);

    list->isOpen = FALSE;
    list->lastAccess = Unknown;
}


/*
 * for using the list as a queue
 */

/* Add the datum to the tail of the given list. */
void
Lst_Enqueue(Lst list, void *datum)
{
    Lst_Append(list, datum);
}

/* Remove and return the datum at the head of the given list. */
void *
Lst_Dequeue(Lst list)
{
    void *datum;

    assert(list != NULL);
    assert(!LstIsEmpty(list));

    datum = list->first->datum;
    Lst_Remove(list, list->first);
    assert(datum != NULL);
    return datum;
}
