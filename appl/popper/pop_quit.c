/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id$");

/*
 *  quit:   Terminate a POP session
 */

int
pop_quit (POP *p)
{
    /*  Release the message information list */
    if (p->mlp) free (p->mlp);

    return(POP_SUCCESS);
}
