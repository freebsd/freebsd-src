/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id: pop_last.c,v 1.6 1996/10/28 16:25:28 assar Exp $");

/* 
 *  last:   Display the last message touched in a POP session
 */

int
pop_last (POP *p)
{
    return (pop_msg(p,POP_SUCCESS,"%u is the last message seen.",p->last_msg));
}
