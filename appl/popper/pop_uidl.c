/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <popper.h>
RCSID("$Id$");

#ifdef UIDL
/*
 *  uidl:   Uidl the contents of a POP maildrop
 */

int
pop_uidl (POP *p)
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
        return (pop_msg(p,POP_SUCCESS,"%u %s",msg_num,mp->msg_id));
    }

    /*  Display the entire list of messages */
    pop_msg(p,POP_SUCCESS,
	    "%d messages (%ld octets)",
            p->msg_count-p->msgs_deleted,
	    p->drop_size-p->bytes_deleted);

    /*  Loop through the message information list.  Skip deleted messages */
    for (i = p->msg_count, mp = p->mlp; i > 0; i--, mp++) {
        if (!(mp->flags & DEL_FLAG))
            fprintf(p->output,"%u %s\r\n",mp->number,mp->msg_id);
    }

    /*  "." signals the end of a multi-line transmission */
    fprintf(p->output,".\r\n");
    fflush(p->output);

    return(POP_SUCCESS);
}
#endif /* UIDL */
