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
 * @(#)lstConcat.c	8.1 (Berkeley) 6/6/93
 */

#ifndef lint
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.bin/make/lst.lib/lstConcat.c,v 1.11 2002/10/09 02:00:22 jmallett Exp $");
#endif /* not lint */

/*-
 * listConcat.c --
 *	Function to concatentate two lists.
 */

#include    "lstInt.h"

/*-
 *-----------------------------------------------------------------------
 * Lst_Concat --
 *	Concatenate two lists. New elements are created to hold the data
 *	elements, if specified, but the elements themselves are not copied.
 *	If the elements should be duplicated to avoid confusion with another
 *	list, the Lst_Duplicate function should be called first.
 *	If LST_CONCLINK is specified, the second list is destroyed since
 *	its pointers have been corrupted and the list is no longer useable.
 *
 * Results:
 *	SUCCESS if all went well. FAILURE otherwise.
 *
 * Side Effects:
 *	New elements are created and appended the the first list.
 *-----------------------------------------------------------------------
 */
ReturnStatus
Lst_Concat (l1, l2, flags)
    Lst    	  	l1; 	/* The list to which l2 is to be appended */
    Lst    	  	l2; 	/* The list to append to l1 */
    int	   	  	flags;  /* LST_CONCNEW if LstNode's should be duplicated
				 * LST_CONCLINK if should just be relinked */
{
    register ListNode  	ln;     /* original LstNode */
    register ListNode  	nln;    /* new LstNode */
    register ListNode  	last;   /* the last element in the list. Keeps
				 * bookkeeping until the end */
    register List 	list1 = (List)l1;
    register List 	list2 = (List)l2;

    if (!LstValid (l1) || !LstValid (l2)) {
	return (FAILURE);
    }

    if (flags == LST_CONCLINK) {
	if (list2->firstPtr != NULL) {
	    /*
	     * We set the nextPtr of the
	     * last element of list two to be NULL to make the loop easier and
	     * so we don't need an extra case should the first list turn
	     * out to be non-circular -- the final element will already point
	     * to NULL space and the first element will be untouched if it
	     * existed before and will also point to NULL space if it didn't.
	     */
	    list2->lastPtr->nextPtr = NULL;
	    /*
	     * So long as the second list isn't empty, we just link the
	     * first element of the second list to the last element of the
	     * first list. If the first list isn't empty, we then link the
	     * last element of the list to the first element of the second list
	     * The last element of the second list, if it exists, then becomes
	     * the last element of the first list.
	     */
	    list2->firstPtr->prevPtr = list1->lastPtr;
	    if (list1->lastPtr != NULL) {
 		list1->lastPtr->nextPtr = list2->firstPtr;
	    } else {
		list1->firstPtr = list2->firstPtr;
	    }
	    list1->lastPtr = list2->lastPtr;
	}
	if (list1->isCirc && list1->firstPtr != NULL) {
	    /*
	     * If the first list is supposed to be circular and it is (now)
	     * non-empty, we must make sure it's circular by linking the
	     * first element to the last and vice versa
	     */
	    list1->firstPtr->prevPtr = list1->lastPtr;
	    list1->lastPtr->nextPtr = list1->firstPtr;
	}
	free (l2);
    } else if (list2->firstPtr != NULL) {
	/*
	 * We set the nextPtr of the last element of list 2 to be NULL to make
	 * the loop less difficult. The loop simply goes through the entire
	 * second list creating new LstNodes and filling in the nextPtr, and
	 * prevPtr to fit into l1 and its datum field from the
	 * datum field of the corresponding element in l2. The 'last' node
	 * follows the last of the new nodes along until the entire l2 has
	 * been appended. Only then does the bookkeeping catch up with the
	 * changes. During the first iteration of the loop, if 'last' is NULL,
	 * the first list must have been empty so the newly-created node is
	 * made the first node of the list.
	 */
	list2->lastPtr->nextPtr = NULL;
	for (last = list1->lastPtr, ln = list2->firstPtr;
	     ln != NULL;
	     ln = ln->nextPtr)
	{
	    PAlloc (nln, ListNode);
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

	/*
	 * The circularity of both list one and list two must be corrected
	 * for -- list one because of the new nodes added to it; list two
	 * because of the alteration of list2->lastPtr's nextPtr to ease the
	 * above for loop.
	 */
	if (list1->isCirc) {
	    list1->lastPtr->nextPtr = list1->firstPtr;
	    list1->firstPtr->prevPtr = list1->lastPtr;
	} else {
	    last->nextPtr = NULL;
	}

	if (list2->isCirc) {
	    list2->lastPtr->nextPtr = list2->firstPtr;
	}
    }

    return (SUCCESS);
}

