/*-
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
 *
 * $FreeBSD$
 */

/*-
 * lst.c --
 *	Routines to maintain a linked list of objects.
 */

#include <stdio.h>
#include <stdlib.h>

#include "lst.h"
#include "make.h"
#include "util.h"

/*-
 *-----------------------------------------------------------------------
 * Lst_Append --
 *	Create a new node and add it to the given list after the given node.
 *
 * Arguments:
 *	l	affected list
 *	ln	node after which to append the datum
 *	d	said datum
 *
 * Side Effects:
 *	A new LstNode is created and linked in to the List. The lastPtr
 *	field of the List will be altered if ln is the last node in the
 *	list. lastPtr and firstPtr will alter if the list was empty and
 *	ln was NULL.
 *
 *-----------------------------------------------------------------------
 */
void
Lst_Append(Lst *list, LstNode *ln, void *d)
{
    LstNode *nLNode;

    nLNode = emalloc(sizeof(*nLNode));
    nLNode->datum = d;
    nLNode->useCount = nLNode->flags = 0;

    if (ln == NULL) {
	nLNode->nextPtr = nLNode->prevPtr = NULL;
	list->firstPtr = list->lastPtr = nLNode;
    } else {
	nLNode->prevPtr = ln;
	nLNode->nextPtr = ln->nextPtr;

	ln->nextPtr = nLNode;
	if (nLNode->nextPtr != NULL) {
	    nLNode->nextPtr->prevPtr = nLNode;
	}

	if (ln == list->lastPtr) {
	    list->lastPtr = nLNode;
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * Lst_Concat --
 *	Concatenate two lists. New elements are created to hold the data
 *	elements, if specified, but the elements themselves are not copied.
 *	If the elements should be duplicated to avoid confusion with another
 *	list, the Lst_Duplicate function should be called first.
 *
 * Results:
 *	SUCCESS if all went well. FAILURE otherwise.
 *
 * Arguments:
 *	list1	The list to which list2 is to be appended
 *	list2	The list to append to list1
 *	flags	LST_CONCNEW if LstNode's should be duplicated
 *		LST_CONCLINK if should just be relinked
 *
 * Side Effects:
 *	New elements are created and appended the the first list.
 *-----------------------------------------------------------------------
 */
void
Lst_Concat(Lst *list1, Lst *list2, int flags)
{
    LstNode *ln;	/* original LstNode */
    LstNode *nln;	/* new LstNode */
    LstNode *last;	/* the last element in the list. Keeps
			 * bookkeeping until the end */

    if (list2->firstPtr == NULL)
	return;

    if (flags == LST_CONCLINK) {
	/*
	 * Link the first element of the second list to the last element of the
	 * first list. If the first list isn't empty, we then link the
	 * last element of the list to the first element of the second list
	 * The last element of the second list, if it exists, then becomes
	 * the last element of the first list.
	 */
	list2->firstPtr->prevPtr = list1->lastPtr;
	if (list1->lastPtr != NULL)
	    list1->lastPtr->nextPtr = list2->firstPtr;
	else
	    list1->firstPtr = list2->firstPtr;
	list1->lastPtr = list2->lastPtr;

	Lst_Init(list2);
    } else {
	/*
	 * The loop simply goes through the entire
	 * second list creating new LstNodes and filling in the nextPtr, and
	 * prevPtr to fit into list1 and its datum field from the
	 * datum field of the corresponding element in list2. The 'last' node
	 * follows the last of the new nodes along until the entire list2 has
	 * been appended. Only then does the bookkeeping catch up with the
	 * changes. During the first iteration of the loop, if 'last' is NULL,
	 * the first list must have been empty so the newly-created node is
	 * made the first node of the list.
	 */
	for (last = list1->lastPtr, ln = list2->firstPtr;
	     ln != NULL;
	     ln = ln->nextPtr)
	{
	    nln = emalloc(sizeof(*nln));
	    nln->datum = ln->datum;
	    if (last != NULL) {
		last->nextPtr = nln;
	    } else {
		list1->firstPtr = nln;
	    }
	    nln->prevPtr = last;
	    nln->flags = nln->useCount = 0;
	    last = nln;
	}

	/*
	 * Finish bookkeeping. The last new element becomes the last element
	 * of list one.
	 */
	list1->lastPtr = last;
	last->nextPtr = NULL;
    }
}

/*-
 *-----------------------------------------------------------------------
 * Lst_DeQueue --
 *	Remove and return the datum at the head of the given list.
 *
 * Results:
 *	The datum in the node at the head or (ick) NULL if the list
 *	is empty.
 *
 * Side Effects:
 *	The head node is removed from the list.
 *
 *-----------------------------------------------------------------------
 */
void *
Lst_DeQueue(Lst *l)
{
    void *rd;
    LstNode *tln;

    tln = Lst_First(l);
    if (tln == NULL) {
	return (NULL);
    }

    rd = tln->datum;
    Lst_Remove(l, tln);
    return (rd);
}

/*-
 *-----------------------------------------------------------------------
 * Lst_Destroy --
 *	Destroy a list and free all its resources. If the freeProc is
 *	given, it is called with the datum from each node in turn before
 *	the node is freed.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The given list is freed in its entirety.
 *
 *-----------------------------------------------------------------------
 */
void
Lst_Destroy(Lst *list, FreeProc *freeProc)
{
    LstNode *ln;

    if (list->firstPtr == NULL)
	return;

    if (freeProc != NOFREE) {
	while ((ln = list->firstPtr) != NULL) {
	    list->firstPtr = ln->nextPtr;
	    (*freeProc)(ln->datum);
	    free(ln);
	}
    } else {
	while ((ln = list->firstPtr) != NULL) {
	    list->firstPtr = ln->nextPtr;
	    free(ln);
	}
    }
    list->lastPtr = NULL;
}

/*-
 *-----------------------------------------------------------------------
 * Lst_Duplicate --
 *	Duplicate an entire list. If a function to copy a void * is
 *	given, the individual client elements will be duplicated as well.
 *
 * Arguments:
 *	dst	the destination list (initialized)
 *	src	the list to duplicate
 *	copyProc A function to duplicate each void
 *
 *-----------------------------------------------------------------------
 */
void
Lst_Duplicate(Lst *dst, Lst *src, DuplicateProc *copyProc)
{
    LstNode *ln;

    ln = src->firstPtr;
    while (ln != NULL) {
	if (copyProc != NOCOPY)
	    Lst_AtEnd(dst, (*copyProc)(ln->datum));
	else
	    Lst_AtEnd(dst, ln->datum);
	ln = ln->nextPtr;
    }
}

/*-
 *-----------------------------------------------------------------------
 * Lst_FindFrom --
 *	Search for a node starting and ending with the given one on the
 *	given list using the passed datum and comparison function to
 *	determine when it has been found.
 *
 * Results:
 *	The found node or NULL
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
LstNode *
Lst_FindFrom(Lst *l, LstNode *ln, const void *d, CompareProc *cProc)
{
    LstNode *tln;
    Boolean	found = FALSE;

    if (!Lst_Valid(l) || Lst_IsEmpty(l) || !Lst_NodeValid(ln, l)) {
	return (NULL);
    }

    tln = ln;

    do {
	if ((*cProc)(tln->datum, d) == 0) {
	    found = TRUE;
	    break;
	} else {
	    tln = tln->nextPtr;
	}
    } while (tln != ln && tln != NULL);

    if (found) {
	return (tln);
    } else {
	return (NULL);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Lst_ForEachFrom --
 *	Apply the given function to each element of the given list. The
 *	function should return 0 if traversal should continue and non-
 *	zero if it should abort.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Only those created by the passed-in function.
 *
 *-----------------------------------------------------------------------
 */
void
Lst_ForEachFrom(Lst *list, LstNode *ln, DoProc *proc, void *d)
{
    LstNode	*next;
    Boolean	done;
    int		result;

    if (!Lst_Valid(list) || Lst_IsEmpty(list)) {
	return;
    }

    do {
	/*
	 * Take care of having the current element deleted out from under
	 * us.
	 */

	next = ln->nextPtr;

	ln->useCount++;
	result = (*proc)(ln->datum, d);
	ln->useCount--;

	/*
	 * We're done with the traversal if
	 *  - nothing's been added after the current node and
	 *  - the next node to examine is the first in the queue or
	 *    doesn't exist.
	 */
	done = (next == ln->nextPtr &&
		(next == NULL || next == list->firstPtr));

	next = ln->nextPtr;

	if (ln->flags & LN_DELETED) {
	    free(ln);
	}
	ln = next;
    } while (!result && !Lst_IsEmpty(list) && !done);
}

/*-
 *-----------------------------------------------------------------------
 * Lst_Insert --
 *	Insert a new node with the given piece of data before the given
 *	node in the given list.
 *
 * Parameters:
 *	l	list to manipulate
 *	ln	node before which to insert d
 *	d	datum to be inserted
 *
 * Side Effects:
 *	the firstPtr field will be changed if ln is the first node in the
 *	list.
 *
 *-----------------------------------------------------------------------
 */
void
Lst_Insert(Lst *list, LstNode *ln, void *d)
{
    LstNode *nLNode;	/* new lnode for d */

    nLNode = emalloc(sizeof(*nLNode));
    nLNode->datum = d;
    nLNode->useCount = nLNode->flags = 0;

    if (ln == NULL) {
	nLNode->prevPtr = nLNode->nextPtr = NULL;
	list->firstPtr = list->lastPtr = nLNode;
    } else {
	nLNode->prevPtr = ln->prevPtr;
	nLNode->nextPtr = ln;

	if (nLNode->prevPtr != NULL) {
	    nLNode->prevPtr->nextPtr = nLNode;
	}
	ln->prevPtr = nLNode;

	if (ln == list->firstPtr) {
	    list->firstPtr = nLNode;
	}
    }
}

LstNode *
Lst_Member(Lst *list, void *d)
{
    LstNode *lNode;

    lNode = list->firstPtr;
    if (lNode == NULL) {
	return (NULL);
    }

    do {
	if (lNode->datum == d) {
	    return (lNode);
	}
	lNode = lNode->nextPtr;
    } while (lNode != NULL && lNode != list->firstPtr);

    return (NULL);
}

/*-
 *-----------------------------------------------------------------------
 * Lst_Remove --
 *	Remove the given node from the given list.
 *
 * Results:
 *	SUCCESS or FAILURE.
 *
 * Side Effects:
 *	The list's firstPtr will be set to NULL if ln is the last
 *	node on the list. firsPtr and lastPtr will be altered if ln is
 *	either the first or last node, respectively, on the list.
 *
 *-----------------------------------------------------------------------
 */
void
Lst_Remove(Lst *list, LstNode *ln)
{
    /*
     * unlink it from the list
     */
    if (ln->nextPtr != NULL)
	/* unlink from the backward chain */
	ln->nextPtr->prevPtr = ln->prevPtr;
    else
	/* this was the last element */
	list->lastPtr = ln->prevPtr;

    if (ln->prevPtr != NULL)
	/* unlink from the forward chain */
	ln->prevPtr->nextPtr = ln->nextPtr;
    else
	/* this was the first element */
	list->firstPtr = ln->nextPtr;

    /*
     * note that the datum is unmolested. The caller must free it as
     * necessary and as expected.
     */
    if (ln->useCount == 0) {
	free(ln);
    } else {
printf("USE COUNT %d\n", ln->useCount);
	ln->flags |= LN_DELETED;
    }
}
