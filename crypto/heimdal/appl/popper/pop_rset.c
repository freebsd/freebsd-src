/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id: pop_rset.c,v 1.9 1998/04/23 17:38:08 joda Exp $");

/* 
 *  rset:   Unflag all messages flagged for deletion in a POP maildrop
 */

int
pop_rset (POP *p)
{
    MsgInfoList     *   mp;         /*  Pointer to the message info list */
    int		        i;

    /*  Unmark all the messages */
    for (i = p->msg_count, mp = p->mlp; i > 0; i--, mp++)
        mp->flags &= ~DEL_FLAG;
    
    /*  Reset the messages-deleted and bytes-deleted counters */
    p->msgs_deleted = 0;
    p->bytes_deleted = 0;
    
    /*  Reset the last-message-access flag */
    p->last_msg = 0;

    return (pop_msg(p,POP_SUCCESS,"Maildrop has %u messages (%ld octets)",
		    p->msg_count, p->drop_size));
}
