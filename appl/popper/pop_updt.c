/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id$");

static const char standard_error[] =
    "Error error updating primary drop. Mailbox unchanged";

/*
 *  updt:   Apply changes to a user's POP maildrop
 */

int
pop_updt (POP *p)
{
    FILE                *   md;                     /*  Stream pointer for
                                                        the user's maildrop */
    int                     mfd;                    /*  File descriptor for
                                                        above */
    char                    buffer[BUFSIZ];         /*  Read buffer */

    MsgInfoList         *   mp;                     /*  Pointer to message
                                                        info list */
    int		            msg_num;                /*  Current message
                                                        counter */
    int			    status_written;         /*  Status header field
                                                        written */
    int                     nchar;                  /* Bytes read/written */

    long                    offset;                 /* New mail offset */

    int			    blank_line;

#ifdef DEBUG
    if (p->debug) {
        pop_log(p,POP_DEBUG,"Performing maildrop update...");
        pop_log(p,POP_DEBUG,"Checking to see if all messages were deleted");
    }
#endif /* DEBUG */

    if(IS_MAILDIR(p))
	return pop_maildir_update(p);

    if (p->msgs_deleted == p->msg_count) {
        /* Truncate before close, to avoid race condition,  DO NOT UNLINK!
           Another process may have opened,  and not yet tried to lock */
        ftruncate ((int)fileno(p->drop),0);
        fclose(p->drop) ;
        return (POP_SUCCESS);
    }

#ifdef DEBUG
    if (p->debug)
        pop_log(p,POP_DEBUG,"Opening mail drop \"%s\"",p->drop_name);
#endif /* DEBUG */

    /*  Open the user's real maildrop */
    if ((mfd = open(p->drop_name,O_RDWR|O_CREAT,0600)) == -1 ||
        (md = fdopen(mfd,"r+")) == NULL) {
        return pop_msg(p,POP_FAILURE,standard_error);
    }

    /*  Lock the user's real mail drop */
    if ( flock(mfd, LOCK_EX) == -1 ) {
        fclose(md) ;
        return pop_msg(p,POP_FAILURE, "flock: '%s': %s", p->temp_drop,
		       strerror(errno));
    }

    /* Go to the right places */
    offset = lseek((int)fileno(p->drop),0,SEEK_END) ;

    /*  Append any messages that may have arrived during the session
        to the temporary maildrop */
    while ((nchar=read(mfd,buffer,BUFSIZ)) > 0)
        if ( nchar != write((int)fileno(p->drop),buffer,nchar) ) {
            nchar = -1;
            break ;
        }
    if ( nchar != 0 ) {
        fclose(md) ;
        ftruncate((int)fileno(p->drop),(int)offset) ;
        fclose(p->drop) ;
        return pop_msg(p,POP_FAILURE,standard_error);
    }

    rewind(md);
    lseek(mfd,0,SEEK_SET);
    ftruncate(mfd,0) ;

    /* Synch stdio and the kernel for the POP drop */
    rewind(p->drop);
    lseek((int)fileno(p->drop),0,SEEK_SET);

    /*  Transfer messages not flagged for deletion from the temporary
        maildrop to the new maildrop */
#ifdef DEBUG
    if (p->debug)
        pop_log(p,POP_DEBUG,"Creating new maildrop \"%s\" from \"%s\"",
                p->drop_name,p->temp_drop);
#endif /* DEBUG */

    for (msg_num = 0; msg_num < p->msg_count; ++msg_num) {

        int doing_body;

        /*  Get a pointer to the message information list */
        mp = &p->mlp[msg_num];

        if (mp->flags & DEL_FLAG) {
#ifdef DEBUG
            if(p->debug)
                pop_log(p,POP_DEBUG,
                    "Message %d flagged for deletion.",mp->number);
#endif /* DEBUG */
            continue;
        }

        fseek(p->drop,mp->offset,0);

#ifdef DEBUG
        if(p->debug)
            pop_log(p,POP_DEBUG,"Copying message %d.",mp->number);
#endif /* DEBUG */
	blank_line = 1;
        for(status_written = doing_body = 0 ;
            fgets(buffer,MAXMSGLINELEN,p->drop);) {

            if (doing_body == 0) { /* Header */

                /*  Update the message status */
                if (strncasecmp(buffer,"Status:",7) == 0) {
                    if (mp->flags & RETR_FLAG)
                        fputs("Status: RO\n",md);
                    else
                        fputs(buffer, md);
                    status_written++;
                    continue;
                }
                /*  A blank line signals the end of the header. */
                if (*buffer == '\n') {
                    doing_body = 1;
                    if (status_written == 0) {
                        if (mp->flags & RETR_FLAG)
                            fputs("Status: RO\n\n",md);
                        else
                            fputs("Status: U\n\n",md);
                    }
                    else fputs ("\n", md);
                    continue;
                }
                /*  Save another header line */
                fputs (buffer, md);
            }
            else { /* Body */
                if (blank_line && strncmp(buffer,"From ",5) == 0) break;
                fputs (buffer, md);
		blank_line = (*buffer == '\n');
            }
        }
    }

    /* flush and check for errors now!  The new mail will writen
       without stdio,  since we need not separate messages */

    fflush(md) ;
    if (ferror(md)) {
        ftruncate(mfd,0) ;
        fclose(md) ;
        fclose(p->drop) ;
        return pop_msg(p,POP_FAILURE,standard_error);
    }

    /* Go to start of new mail if any */
    lseek((int)fileno(p->drop),offset,SEEK_SET);

    while((nchar=read((int)fileno(p->drop),buffer,BUFSIZ)) > 0)
        if ( nchar != write(mfd,buffer,nchar) ) {
            nchar = -1;
            break ;
        }
    if ( nchar != 0 ) {
        ftruncate(mfd,0) ;
        fclose(md) ;
        fclose(p->drop) ;
        return pop_msg(p,POP_FAILURE,standard_error);
    }

    /*  Close the maildrop and empty temporary maildrop */
    fclose(md);
    ftruncate((int)fileno(p->drop),0);
    fclose(p->drop);

    return(pop_quit(p));
}
