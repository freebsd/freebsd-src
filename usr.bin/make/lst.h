/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)lst.h	8.2 (Berkeley) 4/28/95
 * $FreeBSD$
 */

#ifndef lst_h_38f3ead1
#define	lst_h_38f3ead1

/*-
 * lst.h --
 *	Header for using the list library
 */

#include "sprite.h"

/*
 * Structure of a list node.
 */
struct LstNode {
	struct LstNode	*prevPtr;   /* previous element in list */
	struct LstNode	*nextPtr;   /* next in list */
	int	useCount:8; /* Count of functions using the node. Node may not
			     * be deleted until count goes to 0 */
 	int	flags:8;    /* Node status flags */
	void	*datum;	    /* datum associated with this element */
};
typedef	struct	LstNode	LstNode;

/*
 * Flags required for synchronization
 */
#define	LN_DELETED  	0x0001      /* List node should be removed when done */

typedef enum {
	LstHead, LstMiddle, LstTail, LstUnknown
} LstWhere;

/*
 * The list itself
 */
struct Lst {
	LstNode  	*firstPtr; /* first node in list */
	LstNode  	*lastPtr;  /* last node in list */
};
typedef	struct	Lst Lst;

typedef	int CompareProc(const void *, const void *);
typedef	int DoProc(void *, void *);
typedef	void *DuplicateProc(void *);
typedef	void FreeProc(void *);

/*
 * NOFREE can be used as the freeProc to Lst_Destroy when the elements are
 *	not to be freed.
 * NOCOPY performs similarly when given as the copyProc to Lst_Duplicate.
 */
#define	NOFREE		((FreeProc *)NULL)
#define	NOCOPY		((DuplicateProc *)NULL)

#define	LST_CONCNEW	0   /* create new LstNode's when using Lst_Concat */
#define	LST_CONCLINK	1   /* relink LstNode's when using Lst_Concat */

/*
 * Creation/destruction functions
 */
/* Create a new list */
#define	Lst_Init(LST)	do {						\
				(LST)->firstPtr = NULL;			\
				(LST)->lastPtr = NULL;			\
			} while (0)
#define	Lst_Initializer(NAME)	{ NULL, NULL }

/* Duplicate an existing list */
void	Lst_Duplicate(Lst *, Lst *, DuplicateProc *);

/* Destroy an old one */
void	Lst_Destroy(Lst *, FreeProc *);

/*
 * Functions to modify a list
 */
/* Insert an element before another */
void		Lst_Insert(Lst *, LstNode *, void *);
/* Insert an element after another */
void		Lst_Append(Lst *, LstNode *, void *);
/* Place an element at the front of a lst. */
#define	Lst_AtFront(LST, D)	(Lst_Insert((LST), Lst_First(LST), (D)))
/* Place an element at the end of a lst. */
#define	Lst_AtEnd(LST, D) 	(Lst_Append((LST), Lst_Last(LST), (D)))
/* Remove an element */
void		Lst_Remove(Lst *, LstNode *);
/* Replace a node with a new value */
#define	Lst_Replace(NODE, D)	(((NODE) == NULL) ? FAILURE : \
				    (((NODE)->datum = (D)), SUCCESS))
/* Concatenate two lists */
void	Lst_Concat(Lst *, Lst *, int);

/*
 * Node-specific functions
 */
/* Return first element in list */
#define	Lst_First(LST)	((Lst_Valid(LST) && !Lst_IsEmpty(LST)) \
			    ? (LST)->firstPtr : NULL)
/* Return last element in list */
#define	Lst_Last(LST)	((Lst_Valid(LST) && !Lst_IsEmpty(LST)) \
			    ? (LST)->lastPtr : NULL)
/* Return successor to given element */
#define	Lst_Succ(NODE)	(((NODE) == NULL) ? NULL : (NODE)->nextPtr)
/* Get datum from LstNode */
#define	Lst_Datum(NODE)	((NODE)->datum)

/*
 * Functions for entire lists
 */
/* Find an element in a list */
#define	Lst_Find(LST, D, FN)	(Lst_FindFrom((LST), Lst_First(LST), (D), (FN)))
/* Find an element starting from somewhere */
LstNode		*Lst_FindFrom(Lst *, LstNode *, const void *, CompareProc *);
/*
 * See if the given datum is on the list. Returns the LstNode containing
 * the datum
 */
LstNode		*Lst_Member(Lst *, void *);
/* Apply a function to all elements of a lst */
void		Lst_ForEach(Lst *, DoProc *, void *);
#define	Lst_ForEach(LST, FN, D)	(Lst_ForEachFrom((LST), Lst_First(LST), \
				    (FN), (D)))
/*
 * Apply a function to all elements of a lst starting from a certain point.
 * If the list is circular, the application will wrap around to the
 * beginning of the list again.
 */
void		Lst_ForEachFrom(Lst *, LstNode *, DoProc *, void *);

/*
 * for using the list as a queue
 */
/* Place an element at tail of queue */
#define	Lst_EnQueue(LST, D)	(Lst_Valid(LST) \
				    ? Lst_Append((LST), Lst_Last(LST), (D)) \
				    : FAILURE)
/* Remove an element from head of queue */
void		*Lst_DeQueue(Lst *);

/*
 * LstValid (L) --
 *	Return TRUE if the list L is valid
 */
#define Lst_Valid(L)	(((L) == NULL) ? FALSE : TRUE)

/*
 * LstNodeValid (LN, L) --
 *	Return TRUE if the LstNode LN is valid with respect to L
 */
#define Lst_NodeValid(LN, L)	(((LN) == NULL) ? FALSE : TRUE)

/*
 * Lst_IsEmpty(L) --
 *	TRUE if the list L is empty.
 */
#define Lst_IsEmpty(L)	(!Lst_Valid(L) || (L)->firstPtr == NULL)

#endif /* lst_h_38f3ead1 */
