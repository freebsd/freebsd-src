/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id: pop_list.c,v 1.10 1998/04/23 17:37:47 joda Exp $");

/* 
 *  list:   List the contents of a POP maildrop
 */

int
pop_list (POP *p)
{
    MsgInfoList         *   mp;         /*  Pointer to message info list */
    int		            i;
    int			    msg_num;

    /*  Was a message number provided? */
    if (p->parm_count > 0) {
        msg_num = atoi(p->pop_parm[1]);

        /*  Is requested message out of range? */
        if ((msg_num < 1) || (msg_num > p->msg_count))
            return (pop_msg (p,POP_FAILURE,
                "Message %d does not exist.",msg_num));

        /*  Get a pointer to the message in the message list */
        mp = &p->mlp[msg_num-1];

        /*  Is the message already flagged for deletion? */
        if (mp->flags & DEL_FLAG)
            return (pop_msg (p,POP_FAILURE,
                "Message %d has been deleted.",msg_num));

        /*  Display message information */
        return (pop_msg(p,POP_SUCCESS,"%d %ld",msg_num,mp->length));
    }
    
    /*  Display the entire list of messages */
    pop_msg(p,POP_SUCCESS,
	    "%d messages (%ld octets)",
            p->msg_count-p->msgs_deleted,
	    p->drop_size-p->bytes_deleted);

    /*  Loop through the message information list.  Skip deleted messages */
    for (i = p->msg_count, mp = p->mlp; i > 0; i--, mp++) {
        if (!(mp->flags & DEL_FLAG)) 
            fprintf(p->output,"%u %lu\r\n",mp->number,mp->length);
    }

    /*  "." signals the end of a multi-line transmission */
    fprintf(p->output,".\r\n");
    fflush(p->output);

    return(POP_SUCCESS);
}
