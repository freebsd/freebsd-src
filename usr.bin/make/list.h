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
 *	@(#)list.h	8.2 (Berkeley) 4/28/95
 */

/*
 * list.h --
 *
 * Structures, macros, and routines exported by the List module.
 */

#ifndef _LIST
#define _LIST

#ifndef _SPRITE
#include "sprite.h"
#endif _SPRITE

/*
 * This module defines the list abstraction, which enables one to link
 * together arbitrary data structures.  Lists are doubly-linked and
 * circular.  A list contains a header followed by its real members, if
 * any.  (An empty list therefore consists of a single element, the
 * header,  whose nextPtr and prevPtr fields point to itself).  To refer
 * to a list as a whole, the user keeps a pointer to the header; that
 * header is initialized by a call to List_Init(), which creates an empty
 * list given a pointer to a List_Links structure (described below).
 * 
 * The links are contained in a two-element structure called List_Links.
 * A list joins List_Links records (that is, each List_Links structure
 * points to other List_Links structures), but if the List_Links is the
 * first field within a larger structure, then the larger structures are
 * effectively linked together as follows:
 * 
 *	      header
 *	  (List_Links)		   first elt.		    second elt.
 *	-----------------	-----------------	----------------- 
 * ..->	|    nextPtr	| ---->	|  List_Links	| ---->	|  List_Links	|----..
 *	| - - - - - - -	|	|		|	|		| 
 * ..--	|    prevPtr	| <----	|		| <----	|		|<---..
 *	-----------------	- ---  ---  ---	-	- ---  ---  ---	-
 *				|    rest of	|	|    rest of	| 
 *				|   structure	|	|   structure	| 
 *				|		|	|		|
 *				|      ...	|	|      ...	| 
 *				-----------------	----------------- 
 * 
 * It is possible to link structures through List_Links fields that are
 * not at the beginning of the larger structure, but it is then necessary
 * to perform pointer arithmetic to find the beginning of the larger
 * structure, given a pointer to some point within it.
 * 
 * A typical structure might be something like:
 * 
 *      typedef struct {
 *                  List_Links links;
 *                  char ch;
 *                  integer flags;
 *      } EditChar;
 *  
 * Before an element is inserted in a list for the first time, it must
 * be initialized by calling the macro List_InitElement().
 */


/*
 * data structure for lists
 */

typedef struct List_Links {
    struct List_Links *prevPtr;
    struct List_Links *nextPtr;
} List_Links;

/*
 * procedures
 */

void	List_Init();    /* initialize a header to a list */
void    List_Insert();  /* insert an element into a list */
void 	List_Remove();  /* remove an element from a list */
void 	List_Move();    /* move an element elsewhere in a list */

/*
 * ----------------------------------------------------------------------------
 *
 * List_InitElement --
 *
 *      Initialize a list element.  Must be called before an element is first
 *	inserted into a list.
 *
 * ----------------------------------------------------------------------------
 */
#define List_InitElement(elementPtr) \
    (elementPtr)->prevPtr = (List_Links *) NIL; \
    (elementPtr)->nextPtr = (List_Links *) NIL;
    
/*
 * Macros for stepping through or selecting parts of lists
 */

/*
 * ----------------------------------------------------------------------------
 *
 * LIST_FORALL --
 *
 *      Macro to loop through a list and perform an operation on each member.
 *
 *      Usage: LIST_FORALL(headerPtr, itemPtr) {
 *                 / * 
 *                   * operation on itemPtr, which points to successive members
 *                   * of the list
 *                   * 
 *                   * It may be appropriate to first assign
 *                   *          foobarPtr = (Foobar *) itemPtr;
 *                   * to refer to the entire Foobar structure.
 *                   * /
 *             }
 *
 *      Note: itemPtr must be a List_Links pointer variable, and headerPtr
 *      must evaluate to a pointer to a List_Links structure.
 *
 * ----------------------------------------------------------------------------
 */

#define LIST_FORALL(headerPtr, itemPtr) \
        for (itemPtr = List_First(headerPtr); \
             !List_IsAtEnd((headerPtr),itemPtr); \
             itemPtr = List_Next(itemPtr))

/*
 * ----------------------------------------------------------------------------
 *
 * List_IsEmpty --
 *
 *      Macro: Boolean value, TRUE if the given list does not contain any
 *      members.
 *
 *      Usage: if (List_IsEmpty(headerPtr)) ...
 *
 * ----------------------------------------------------------------------------
 */

#define List_IsEmpty(headerPtr) \
        ((headerPtr) == (headerPtr)->nextPtr)

/*
 * ----------------------------------------------------------------------------
 *
 * List_IsAtEnd --
 *
 *      Macro: Boolean value, TRUE if itemPtr is after the end of headerPtr
 *      (i.e., itemPtr is the header of the list).
 *
 *      Usage: if (List_IsAtEnd(headerPtr, itemPtr)) ...
 *
 * ----------------------------------------------------------------------------
 */


#define List_IsAtEnd(headerPtr, itemPtr) \
        ((itemPtr) == (headerPtr))


/*
 * ----------------------------------------------------------------------------
 *
 * List_First --
 *
 *      Macro to return the first member in a list, which is the header if
 *      the list is empty.
 *
 *      Usage: firstPtr = List_First(headerPtr);
 *
 * ----------------------------------------------------------------------------
 */

#define List_First(headerPtr) ((headerPtr)->nextPtr)

/*
 * ----------------------------------------------------------------------------
 *
 * List_Last --
 *
 *      Macro to return the last member in a list, which is the header if
 *      the list is empty.
 *
 *      Usage: lastPtr = List_Last(headerPtr);
 *
 * ----------------------------------------------------------------------------
 */

#define List_Last(headerPtr) ((headerPtr)->prevPtr)

/*
 * ----------------------------------------------------------------------------
 *
 * List_Prev --
 *
 *      Macro to return the member preceding the given member in its list.
 *      If the given list member is the first element in the list, List_Prev
 *      returns the list header.
 *
 *      Usage: prevPtr = List_Prev(itemPtr);
 *
 * ----------------------------------------------------------------------------
 */

#define List_Prev(itemPtr) ((itemPtr)->prevPtr)

/*
 * ----------------------------------------------------------------------------
 *
 * List_Next --
 *
 *      Macro to return the member following the given member in its list.
 *      If the given list member is the last element in the list, List_Next
 *      returns the list header.
 *
 *      Usage: nextPtr = List_Next(itemPtr);
 *
 * ----------------------------------------------------------------------------
 */

#define List_Next(itemPtr) ((itemPtr)->nextPtr)


/*
 * ----------------------------------------------------------------------------
 *      The List_Insert procedure takes two arguments.  The first argument
 *      is a pointer to the structure to be inserted into a list, and
 *      the second argument is a pointer to the list member after which
 *      the new element is to be inserted.  Macros are used to determine
 *      which existing member will precede the new one.
 *
 *      The List_Move procedure takes a destination argument with the same
 *      semantics as List_Insert.
 *
 *      The following macros define where to insert the new element
 *      in the list:
 *
 *      LIST_AFTER(itemPtr)     --      insert after itemPtr
 *      LIST_BEFORE(itemPtr)    --      insert before itemPtr
 *      LIST_ATFRONT(headerPtr) --      insert at front of list
 *      LIST_ATREAR(headerPtr)  --      insert at end of list
 *
 *      For example, 
 *
 *              List_Insert(itemPtr, LIST_AFTER(otherPtr));
 *
 *      will insert itemPtr following otherPtr in the list containing otherPtr.
 * ----------------------------------------------------------------------------
 */

#define LIST_AFTER(itemPtr) ((List_Links *) itemPtr)

#define LIST_BEFORE(itemPtr) (((List_Links *) itemPtr)->prevPtr)

#define LIST_ATFRONT(headerPtr) ((List_Links *) headerPtr)

#define LIST_ATREAR(headerPtr) (((List_Links *) headerPtr)->prevPtr)

#endif /* _LIST */
