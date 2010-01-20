/*
 * Copyright (C) 2004-2005 The Free Software Foundation, Inc.
 *
 * Portions Copyright (C) 2004-2005 Derek Price, Ximbiot <http://ximbiot.com>,
 *                                  and others.
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 *
 * This module uses the hash.c module to implement a stack.
 */

#include "cvs.h"
#include <assert.h>



static void
do_push (stack, elem, isstring)
    List *stack;
    void *elem;
    int isstring;
{
    Node *p = getnode();

    if (isstring)
	p->key = elem;
    else
	p->data = elem;

    addnode(stack, p);
}



void
push (stack, elem)
    List *stack;
    void *elem;
{
    do_push (stack, elem, 0);
}



void
push_string (stack, elem)
    List *stack;
    char *elem;
{
    do_push (stack, elem, 1);
}



static void *
do_pop (stack, isstring)
    List *stack;
    int isstring;
{
    void *elem;

    if (isempty (stack)) return NULL;

    if (isstring)
    {
	elem = stack->list->prev->key;
	stack->list->prev->key = NULL;
    }
    else
    {
	elem = stack->list->prev->data;
	stack->list->prev->data = NULL;
    }

    delnode (stack->list->prev);
    return elem;
}



void *
pop (stack)
    List *stack;
{
    return do_pop (stack, 0);
}



char *
pop_string (stack)
    List *stack;
{
    return do_pop (stack, 1);
}



static void
do_unshift (stack, elem, isstring)
    List *stack;
    void *elem;
    int isstring;
{
    Node *p = getnode();

    if (isstring)
	p->key = elem;
    else
	p->data = elem;

    addnode_at_front(stack, p);
}



void
unshift (stack, elem)
    List *stack;
    void *elem;
{
    do_unshift (stack, elem, 0);
}



void
unshift_string (stack, elem)
    List *stack;
    char *elem;
{
    do_unshift (stack, elem, 1);
}



static void *
do_shift (stack, isstring)
    List *stack;
    int isstring;
{
    void *elem;

    if (isempty (stack)) return NULL;

    if (isstring)
    {
	elem = stack->list->next->key;
	stack->list->next->key = NULL;
    }
    else
    {
	elem = stack->list->next->data;
	stack->list->next->data = NULL;
    }
    delnode (stack->list->next);
    return elem;
}



void *
shift (stack)
    List *stack;
{
    return do_shift (stack, 0);
}



char *
shift_string (stack)
    List *stack;
{
    return do_shift (stack, 1);
}



int
isempty (stack)
    List *stack;
{
    if (stack->list == stack->list->next)
	return 1;
    return 0;
}
