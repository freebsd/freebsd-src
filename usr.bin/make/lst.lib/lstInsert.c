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
 * @(#)lstInsert.c	8.1 (Berkeley) 6/6/93
 */

#ifndef lint
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#endif /* not lint */

/*-
 * LstInsert.c --
 *	Insert a new datum before an old one
 */

#include <stdio.h>

#include "make.h"
#include "util.h"
#include "lst.h"

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
