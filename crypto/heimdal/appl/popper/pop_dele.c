/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id: pop_dele.c,v 1.10 1999/08/12 11:35:26 joda Exp $");

/* 
 *  dele:   Delete a message from the POP maildrop
 */
int
pop_dele (POP *p)
{
    MsgInfoList     *   mp;         /*  Pointer to message info list */
    int                 msg_num;

    /*  Convert the message number parameter to an integer */
    msg_num = atoi(p->pop_parm[1]);

    /*  Is requested message out of range? */
    if ((msg_num < 1) || (msg_num > p->msg_count))
        return (pop_msg (p,POP_FAILURE,"Message %d does not exist.",msg_num));

    /*  Get a pointer to the message in the message list */
    mp = &(p->mlp[msg_num-1]);

    /*  Is the message already flagged for deletion? */
    if (mp->flags & DEL_FLAG)
        return (pop_msg (p,POP_FAILURE,"Message %d has already been deleted.",
            msg_num));

    /*  Flag the message for deletion */
    mp->flags |= DEL_FLAG;

#ifdef DEBUG
    if(p->debug)
        pop_log(p, POP_DEBUG,
		"Deleting message %u at offset %ld of length %ld\n",
		mp->number, mp->offset, mp->length);
#endif /* DEBUG */

    /*  Update the messages_deleted and bytes_deleted counters */
    p->msgs_deleted++;
    p->bytes_deleted += mp->length;

    /*  Update the last-message-accessed number if it is lower than 
        the deleted message */
    if (p->last_msg < msg_num) p->last_msg = msg_num;

    return (pop_msg (p,POP_SUCCESS,"Message %d has been deleted.",msg_num));
}

#ifdef XDELE
/* delete a range of messages */
int
pop_xdele(POP *p)
{
    MsgInfoList     *   mp;         /*  Pointer to message info list */

    int msg_min, msg_max;
    int i;


    msg_min = atoi(p->pop_parm[1]);
    if(p->parm_count == 1)
	msg_max = msg_min;
    else
	msg_max = atoi(p->pop_parm[2]);

    if (msg_min < 1)
        return (pop_msg (p,POP_FAILURE,"Message %d does not exist.",msg_min));
    if(msg_max > p->msg_count)
        return (pop_msg (p,POP_FAILURE,"Message %d does not exist.",msg_max));
    for(i = msg_min; i <= msg_max; i++) {

	/*  Get a pointer to the message in the message list */
	mp = &(p->mlp[i - 1]);

	/*  Is the message already flagged for deletion? */
	if (mp->flags & DEL_FLAG)
	    continue; /* no point in returning error */
	/*  Flag the message for deletion */
	mp->flags |= DEL_FLAG;
	
#ifdef DEBUG
	if(p->debug)
	    pop_log(p, POP_DEBUG,
		    "Deleting message %u at offset %ld of length %ld\n",
		    mp->number, mp->offset, mp->length);
#endif /* DEBUG */
	
	/*  Update the messages_deleted and bytes_deleted counters */
	p->msgs_deleted++;
	p->bytes_deleted += mp->length;
    }

    /*  Update the last-message-accessed number if it is lower than 
	the deleted message */
    if (p->last_msg < msg_max) p->last_msg = msg_max;
    
    return (pop_msg (p,POP_SUCCESS,"Messages %d-%d has been deleted.",
		     msg_min, msg_max));
    
}
#endif /* XDELE */
