#include <popper.h>
RCSID("$Id: pop_xover.c,v 1.4 1998/04/23 17:39:31 joda Exp $");

int
pop_xover (POP *p)
{
#ifdef XOVER
    MsgInfoList         *   mp;         /*  Pointer to message info list */
    int		            i;

    pop_msg(p,POP_SUCCESS,
	    "%d messages (%ld octets)",
            p->msg_count-p->msgs_deleted,
	    p->drop_size-p->bytes_deleted);
    
    /*  Loop through the message information list.  Skip deleted messages */
    for (i = p->msg_count, mp = p->mlp; i > 0; i--, mp++) {
        if (!(mp->flags & DEL_FLAG)) 
            fprintf(p->output,"%u\t%s\t%s\t%s\t%s\t%lu\t%u\r\n",
		    mp->number,
		    mp->subject,
		    mp->from,
		    mp->date, 
		    mp->msg_id,
		    mp->length,
		    mp->lines);
    }

    /*  "." signals the end of a multi-line transmission */
    fprintf(p->output,".\r\n");
    fflush(p->output);

    return(POP_SUCCESS);
#else
    return pop_msg(p, POP_FAILURE, "Command not implemented.");
#endif
}
