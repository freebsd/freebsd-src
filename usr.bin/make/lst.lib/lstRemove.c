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
 * @(#)lstRemove.c	8.1 (Berkeley) 6/6/93
 */

#ifndef lint
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#endif /* not lint */

/*-
 * LstRemove.c --
 *	Remove an element from a list
 */

#include "make.h"
#include "lst.h"

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
ReturnStatus
Lst_Remove(Lst list, LstNode ln)
{

    if (!Lst_Valid (list) || !Lst_NodeValid (ln, list)) {
	    return (FAILURE);
    }

    /*
     * unlink it from the list
     */
    if (ln->nextPtr != NULL) {
	ln->nextPtr->prevPtr = ln->prevPtr;
    }
    if (ln->prevPtr != NULL) {
	ln->prevPtr->nextPtr = ln->nextPtr;
    }

    /*
     * if either the firstPtr or lastPtr of the list point to this node,
     * adjust them accordingly
     */
    if (list->firstPtr == ln) {
	list->firstPtr = ln->nextPtr;
    }
    if (list->lastPtr == ln) {
	list->lastPtr = ln->prevPtr;
    }

    /*
     * Sequential access stuff. If the node we're removing is the current
     * node in the list, reset the current node to the previous one. If the
     * previous one was non-existent (prevPtr == NULL), we set the
     * end to be Unknown, since it is.
     */
    if (list->isOpen && (list->curPtr == ln)) {
	list->curPtr = list->prevPtr;
	if (list->curPtr == NULL) {
	    list->atEnd = LstUnknown;
	}
    }

    /*
     * the only way firstPtr can still point to ln is if ln is the last
     * node on the list (the list is circular, so ln->nextptr == ln in
     * this case). The list is, therefore, empty and is marked as such
     */
    if (list->firstPtr == ln) {
	list->firstPtr = NULL;
    }

    /*
     * note that the datum is unmolested. The caller must free it as
     * necessary and as expected.
     */
    if (ln->useCount == 0) {
	free(ln);
    } else {
	ln->flags |= LN_DELETED;
    }

    return (SUCCESS);
}
