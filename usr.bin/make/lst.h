/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)lst.h	8.1 (Berkeley) 6/6/93
 */

/*-
 * lst.h --
 *	Header for using the list library
 */
#ifndef _LST_H_
#define _LST_H_

#include	<sprite.h>
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
#define NOFREE		((void (*)()) 0)
#define NOCOPY		((ClientData (*)()) 0)

#define LST_CONCNEW	0   /* create new LstNode's when using Lst_Concat */
#define LST_CONCLINK	1   /* relink LstNode's when using Lst_Concat */

/*
 * Creation/destruction functions
 */
Lst		  Lst_Init();	    	/* Create a new list */
Lst	    	  Lst_Duplicate();  	/* Duplicate an existing list */
void		  Lst_Destroy();	/* Destroy an old one */

int	    	  Lst_Length();	    	/* Find the length of a list */
Boolean		  Lst_IsEmpty();	/* True if list is empty */

/*
 * Functions to modify a list
 */
ReturnStatus	  Lst_Insert();	    	/* Insert an element before another */
ReturnStatus	  Lst_Append();	    	/* Insert an element after another */
ReturnStatus	  Lst_AtFront();    	/* Place an element at the front of
					 * a lst. */
ReturnStatus	  Lst_AtEnd();	    	/* Place an element at the end of a
					 * lst. */
ReturnStatus	  Lst_Remove();	    	/* Remove an element */
ReturnStatus	  Lst_Replace();	/* Replace a node with a new value */
ReturnStatus	  Lst_Move();	    	/* Move an element to another place */
ReturnStatus	  Lst_Concat();	    	/* Concatenate two lists */

/*
 * Node-specific functions
 */
LstNode		  Lst_First();	    	/* Return first element in list */
LstNode		  Lst_Last();	    	/* Return last element in list */
LstNode		  Lst_Succ();	    	/* Return successor to given element */
LstNode		  Lst_Pred();	    	/* Return predecessor to given
					 * element */
ClientData	  Lst_Datum();	    	/* Get datum from LstNode */

/*
 * Functions for entire lists
 */
LstNode		  Lst_Find();	    	/* Find an element in a list */
LstNode		  Lst_FindFrom();	/* Find an element starting from
					 * somewhere */
LstNode	    	  Lst_Member();	    	/* See if the given datum is on the
					 * list. Returns the LstNode containing
					 * the datum */
int	    	  Lst_Index();	    	/* Returns the index of a datum in the
					 * list, starting from 0 */
void		  Lst_ForEach();	/* Apply a function to all elements of
					 * a lst */
void	    	  Lst_ForEachFrom();  	/* Apply a function to all elements of
					 * a lst starting from a certain point.
					 * If the list is circular, the
					 * application will wrap around to the
					 * beginning of the list again. */
/*
 * these functions are for dealing with a list as a table, of sorts.
 * An idea of the "current element" is kept and used by all the functions
 * between Lst_Open() and Lst_Close().
 */
ReturnStatus	  Lst_Open();	    	/* Open the list */
LstNode		  Lst_Prev();	    	/* Previous element */
LstNode		  Lst_Cur();	    	/* The current element, please */
LstNode		  Lst_Next();	    	/* Next element please */
Boolean		  Lst_IsAtEnd();	/* Done yet? */
void		  Lst_Close();	    	/* Finish table access */

/*
 * for using the list as a queue
 */
ReturnStatus	  Lst_EnQueue();	/* Place an element at tail of queue */
ClientData	  Lst_DeQueue();	/* Remove an element from head of
					 * queue */

#endif _LST_H_
