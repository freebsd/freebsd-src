/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id: pop_send.c,v 1.25 1999/03/05 14:14:28 joda Exp $");

/*
 *  sendline:   Send a line of a multi-line response to a client.
 */
static int
pop_sendline(POP *p, char *buffer)
{
    char        *   bp;

    /*  Byte stuff lines that begin with the termination octet */
    if (*buffer == POP_TERMINATE) 
      fputc(POP_TERMINATE,p->output);

    /*  Look for a <NL> in the buffer */
    if ((bp = strchr(buffer, '\n')))
      *bp = 0;

    /*  Send the line to the client */
    fputs(buffer,p->output);

#ifdef DEBUG
    if(p->debug)
      pop_log(p,POP_DEBUG,"Sending line \"%s\"",buffer);
#endif /* DEBUG */

    /*  Put a <CR><NL> if a newline was removed from the buffer */
    if (bp)
      fputs ("\r\n",p->output);
    return bp != NULL;
}

/* 
 *  send:   Send the header and a specified number of lines 
 *          from a mail message to a POP client.
 */

int
pop_send(POP *p)
{
    MsgInfoList         *   mp;         /*  Pointer to message info list */
    int		            msg_num;
    int			    msg_lines;
    char                    buffer[MAXMSGLINELEN];
#ifdef RETURN_PATH_HANDLING
    char		*   return_path_adr;
    char		*   return_path_end;
    int			    return_path_sent;
    int			    return_path_linlen;
#endif
    int			sent_nl = 0;

    /*  Convert the first parameter into an integer */
    msg_num = atoi(p->pop_parm[1]);

    /*  Is requested message out of range? */
    if ((msg_num < 1) || (msg_num > p->msg_count))
        return (pop_msg (p,POP_FAILURE,"Message %d does not exist.",msg_num));

    /*  Get a pointer to the message in the message list */
    mp = &p->mlp[msg_num-1];

    /*  Is the message flagged for deletion? */
    if (mp->flags & DEL_FLAG)
        return (pop_msg (p,POP_FAILURE,
			 "Message %d has been deleted.",msg_num));

    /*  If this is a TOP command, get the number of lines to send */
    if (strcmp(p->pop_command, "top") == 0) {
        /*  Convert the second parameter into an integer */
        msg_lines = atoi(p->pop_parm[2]);
    }
    else {
        /*  Assume that a RETR (retrieve) command was issued */
        msg_lines = -1;
        /*  Flag the message as retreived */
        mp->flags |= RETR_FLAG;
    }
    
    /*  Display the number of bytes in the message */
    pop_msg(p, POP_SUCCESS, "%ld octets", mp->length);

    if(IS_MAILDIR(p)) {
	int e = pop_maildir_open(p, mp);
	if(e != POP_SUCCESS)
	    return e;
    }

    /*  Position to the start of the message */
    fseek(p->drop, mp->offset, 0);

    return_path_sent = 0;

    if(!IS_MAILDIR(p)) {
	/*  Skip the first line (the sendmail "From" line) */
	fgets (buffer,MAXMSGLINELEN,p->drop);

#ifdef RETURN_PATH_HANDLING
	if (strncmp(buffer,"From ",5) == 0) {
	    return_path_linlen = strlen(buffer);
	    for (return_path_adr = buffer+5;
		 (*return_path_adr == ' ' || *return_path_adr == '\t') &&
		     return_path_adr < buffer + return_path_linlen;
		 return_path_adr++)
		;
	    if (return_path_adr < buffer + return_path_linlen) {
		if ((return_path_end = strchr(return_path_adr, ' ')) != NULL)
		    *return_path_end = '\0';
		if (strlen(return_path_adr) != 0 && *return_path_adr != '\n') {
		    static char tmpbuf[MAXMSGLINELEN + 20];
		    if (snprintf (tmpbuf,
				  sizeof(tmpbuf),
				  "Return-Path: %s\n",
				  return_path_adr) < MAXMSGLINELEN) {
			pop_sendline (p,tmpbuf);
			if (hangup)
			    return pop_msg (p, POP_FAILURE,
					    "SIGHUP or SIGPIPE flagged");
			return_path_sent++;
		    }
		}
	    }
	}
#endif
    }

    /*  Send the header of the message followed by a blank line */
    while (fgets(buffer,MAXMSGLINELEN,p->drop)) {
#ifdef RETURN_PATH_HANDLING
	/* Don't send existing Return-Path-header if already sent own */
	if (!return_path_sent || strncasecmp(buffer, "Return-Path:", 12) != 0)
#endif
	    sent_nl = pop_sendline (p,buffer);
        /*  A single newline (blank line) signals the 
            end of the header.  sendline() converts this to a NULL, 
            so that's what we look for. */
        if (*buffer == 0) break;
        if (hangup)
	    return (pop_msg (p,POP_FAILURE,"SIGHUP or SIGPIPE flagged"));
    }
    /*  Send the message body */
    {
	int blank_line = 1;
	while (fgets(buffer, MAXMSGLINELEN-1, p->drop)) {
	    /*  Look for the start of the next message */
	    if (!IS_MAILDIR(p) && blank_line && strncmp(buffer,"From ",5) == 0)
		break;
	    blank_line = (strncmp(buffer, "\n", 1) == 0);
	    /*  Decrement the lines sent (for a TOP command) */
	    if (msg_lines >= 0 && msg_lines-- == 0) break;
	    sent_nl = pop_sendline(p,buffer);
	    if (hangup)
		return (pop_msg (p,POP_FAILURE,"SIGHUP or SIGPIPE flagged"));
	}
	/* add missing newline at end */
	if(!sent_nl)
	    fputs("\r\n", p->output);
	/* some pop-clients want a blank line at the end of the
           message, we always add one here, but what the heck -- in
           outer (white) space, no one can hear you scream */
	if(IS_MAILDIR(p))
	    fputs("\r\n", p->output);
    }
    /*  "." signals the end of a multi-line transmission */
    fputs(".\r\n",p->output);
    fflush(p->output);

    return(POP_SUCCESS);
}
