/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
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
 *	$Id$
 */

/*-
 * lst.h --
 *	Header for using the list library
 */
#ifndef _LST_H_
#define _LST_H_

#include	<sprite.h>
#include	<sys/param.h>
#if __STDC__
#include	<stdlib.h>
#endif

/*
 * basic typedef. This is what the Lst_ functions handle
 */

typedef	struct	Lst	*Lst;
typedef	struct	LstNode	*LstNode;

#define	NILLST		((Lst) NIL)
#define	NILLNODE	((LstNode) NIL)

/*
 * NOFREE can be used as the freeProc to Lst_Destroy when the elements are
 *	not to be freed.
 * NOCOPY performs similarly when given as the copyProc to Lst_Duplicate.
 */
#define NOFREE		((void (*) __P((ClientData))) 0)
#define NOCOPY		((ClientData (*) __P((ClientData))) 0)

#define LST_CONCNEW	0   /* create new LstNode's when using Lst_Concat */
#define LST_CONCLINK	1   /* relink LstNode's when using Lst_Concat */

/*
 * Creation/destruction functions
 */
/* Create a new list */
Lst		Lst_Init __P((Boolean));
/* Duplicate an existing list */
Lst		Lst_Duplicate __P((Lst, ClientData (*)(ClientData)));
/* Destroy an old one */
void		Lst_Destroy __P((Lst, void (*)(ClientData)));
/* True if list is empty */
Boolean		Lst_IsEmpty __P((Lst));

/*
 * Functions to modify a list
 */
/* Insert an element before another */
ReturnStatus	Lst_Insert __P((Lst, LstNode, ClientData));
/* Insert an element after another */
ReturnStatus	Lst_Append __P((Lst, LstNode, ClientData));
/* Place an element at the front of a lst. */
ReturnStatus	Lst_AtFront __P((Lst, ClientData));
/* Place an element at the end of a lst. */
ReturnStatus	Lst_AtEnd __P((Lst, ClientData));
/* Remove an element */
ReturnStatus	Lst_Remove __P((Lst, LstNode));
/* Replace a node with a new value */
ReturnStatus	Lst_Replace __P((LstNode, ClientData));
/* Concatenate two lists */
ReturnStatus	Lst_Concat __P((Lst, Lst, int));

/*
 * Node-specific functions
 */
/* Return first element in list */
LstNode		Lst_First __P((Lst));
/* Return last element in list */
LstNode		Lst_Last __P((Lst));
/* Return successor to given element */
LstNode		Lst_Succ __P((LstNode));
/* Get datum from LstNode */
ClientData	Lst_Datum __P((LstNode));

/*
 * Functions for entire lists
 */
/* Find an element in a list */
LstNode		Lst_Find __P((Lst, ClientData,
			      int (*)(ClientData, ClientData)));
/* Find an element starting from somewhere */
LstNode		Lst_FindFrom __P((Lst, LstNode, ClientData,
				  int (*cProc)(ClientData, ClientData)));
/*
 * See if the given datum is on the list. Returns the LstNode containing
 * the datum
 */
LstNode		Lst_Member __P((Lst, ClientData));
/* Apply a function to all elements of a lst */
void		Lst_ForEach __P((Lst, int (*)(ClientData, ClientData),
				 ClientData));
/*
 * Apply a function to all elements of a lst starting from a certain point.
 * If the list is circular, the application will wrap around to the
 * beginning of the list again.
 */
void		Lst_ForEachFrom __P((Lst, LstNode,
				     int (*)(ClientData, ClientData),
				     ClientData));
/*
 * these functions are for dealing with a list as a table, of sorts.
 * An idea of the "current element" is kept and used by all the functions
 * between Lst_Open() and Lst_Close().
 */
/* Open the list */
ReturnStatus	Lst_Open __P((Lst));
/* Next element please */
LstNode		Lst_Next __P((Lst));
/* Done yet? */
Boolean		Lst_IsAtEnd __P((Lst));
/* Finish table access */
void		Lst_Close __P((Lst));

/*
 * for using the list as a queue
 */
/* Place an element at tail of queue */
ReturnStatus	Lst_EnQueue __P((Lst, ClientData));
/* Remove an element from head of queue */
ClientData	Lst_DeQueue __P((Lst));

#endif /* _LST_H_ */
