/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id$");

/*
 * Run as the user in `pwd'
 */

int
changeuser(POP *p, struct passwd *pwd)
{
    if(setgid(pwd->pw_gid) < 0) {
	pop_log (p, POP_PRIORITY,
		 "Unable to change to gid %u: %s",
		 (unsigned)pwd->pw_gid,
		 strerror(errno));
	return pop_msg (p, POP_FAILURE,
			"Unable to change gid");
    }
    if(setuid(pwd->pw_uid) < 0) {
	pop_log (p, POP_PRIORITY,
		 "Unable to change to uid %u: %s",
		 (unsigned)pwd->pw_uid,
		 strerror(errno));
	return pop_msg (p, POP_FAILURE,
			"Unable to change uid");
    }
#ifdef DEBUG
    if(p->debug)
	pop_log(p, POP_DEBUG,"uid = %u, gid = %u",
	       (unsigned)getuid(),
	       (unsigned)getgid());
#endif /* DEBUG */
    return POP_SUCCESS;
}

/*
 *  dropcopy:   Make a temporary copy of the user's mail drop and
 *  save a stream pointer for it.
 */

int
pop_dropcopy(POP *p, struct passwd *pwp)
{
    int                     mfd;                    /*  File descriptor for
                                                        the user's maildrop */
    int                     dfd;                    /*  File descriptor for
                                                        the SERVER maildrop */
    FILE		    *tf;		    /*  The temp file */
    char		    template[POP_TMPSIZE];  /*  Temp name holder */
    char                    buffer[BUFSIZ];         /*  Read buffer */
    long                    offset;                 /*  Old/New boundary */
    int                     nchar;                  /*  Bytes written/read */
    int                     tf_fd;                  /*  fd for temp file */
    int			    ret;

    /*  Create a temporary maildrop into which to copy the updated maildrop */
    snprintf(p->temp_drop, sizeof(p->temp_drop), POP_DROP,p->user);

#ifdef DEBUG
    if(p->debug)
        pop_log(p,POP_DEBUG,"Creating temporary maildrop '%s'",
            p->temp_drop);
#endif /* DEBUG */

    /* Here we work to make sure the user doesn't cause us to remove or
     * write over existing files by limiting how much work we do while
     * running as root.
     */

    strlcpy(template, POP_TMPDROP, sizeof(template));
    if ((tf_fd = mkstemp(template)) < 0 ||
	(tf = fdopen(tf_fd, "w+")) == NULL) {
        pop_log(p,POP_PRIORITY,
            "Unable to create temporary temporary maildrop '%s': %s",template,
		strerror(errno));
        return pop_msg(p,POP_FAILURE,
		"System error, can't create temporary file.");
    }

    /* Now give this file to the user	*/
    chown(template, pwp->pw_uid, pwp->pw_gid);
    chmod(template, 0600);

    /* Now link this file to the temporary maildrop.  If this fails it
     * is probably because the temporary maildrop already exists.  If so,
     * this is ok.  We can just go on our way, because by the time we try
     * to write into the file we will be running as the user.
     */
    link(template,p->temp_drop);
    fclose(tf);
    unlink(template);

    ret = changeuser(p, pwp);
    if (ret != POP_SUCCESS)
	return ret;

    /* Open for append,  this solves the crash recovery problem */
    if ((dfd = open(p->temp_drop,O_RDWR|O_APPEND|O_CREAT,0600)) == -1){
        pop_log(p,POP_PRIORITY,
            "Unable to open temporary maildrop '%s': %s",p->temp_drop,
		strerror(errno));
        return pop_msg(p,POP_FAILURE,
		"System error, can't open temporary file, do you own it?");
    }

    /*  Lock the temporary maildrop */
    if ( flock (dfd, (LOCK_EX | LOCK_NB)) == -1 )
    switch(errno) {
        case EWOULDBLOCK:
            return pop_msg(p,POP_FAILURE,
                 "%sMaildrop lock busy!  Is another session active?",
			   (p->flags & POP_FLAG_CAPA) ? "[IN-USE] " : "");
            /* NOTREACHED */
        default:
            return pop_msg(p,POP_FAILURE,"flock: '%s': %s", p->temp_drop,
		strerror(errno));
            /* NOTREACHED */
        }

    /* May have grown or shrunk between open and lock! */
    offset = lseek(dfd,0, SEEK_END);

    /*  Open the user's maildrop, If this fails,  no harm in assuming empty */
    if ((mfd = open(p->drop_name,O_RDWR)) > 0) {

        /*  Lock the maildrop */
        if (flock (mfd, LOCK_EX) == -1) {
            close(mfd) ;
            return pop_msg(p,POP_FAILURE, "flock: '%s': %s", p->temp_drop,
		strerror(errno));
        }

        /*  Copy the actual mail drop into the temporary mail drop */
        while ( (nchar=read(mfd,buffer,BUFSIZ)) > 0 )
            if ( nchar != write(dfd,buffer,nchar) ) {
                nchar = -1 ;
                break ;
            }

        if ( nchar != 0 ) {
            /* Error adding new mail.  Truncate to original size,
               and leave the maildrop as is.  The user will not
               see the new mail until the error goes away.
               Should let them process the current backlog,  in case
               the error is a quota problem requiring deletions! */
            ftruncate(dfd,(int)offset) ;
        } else {
            /* Mail transferred!  Zero the mail drop NOW,  that we
               do not have to do gymnastics to figure out what's new
               and what is old later */
            ftruncate(mfd,0) ;
        }

        /*  Close the actual mail drop */
        close (mfd);
    }

    /*  Acquire a stream pointer for the temporary maildrop */
    if ( (p->drop = fdopen(dfd,"a+")) == NULL ) {
        close(dfd) ;
        return pop_msg(p,POP_FAILURE,"Cannot assign stream for %s",
            p->temp_drop);
    }

    rewind (p->drop);

    return(POP_SUCCESS);
}
