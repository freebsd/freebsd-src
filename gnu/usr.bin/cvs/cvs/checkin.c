/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Check In
 * 
 * Does a very careful checkin of the file "user", and tries not to spoil its
 * modification time (to avoid needless recompilations). When RCS ID keywords
 * get expanded on checkout, however, the modification time is updated and
 * there is no good way to get around this.
 * 
 * Returns non-zero on error.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "$CVSid: @(#)checkin.c 1.48 94/10/07 $";
USE(rcsid)
#endif

int
Checkin (type, file, repository, rcs, rev, tag, options, message, entries)
    int type;
    char *file;
    char *repository;
    char *rcs;
    char *rev;
    char *tag;
    char *options;
    char *message;
    List *entries;
{
    char fname[PATH_MAX];
    Vers_TS *vers;
    int set_time;

    (void) printf ("Checking in %s;\n", file);
    (void) sprintf (fname, "%s/%s%s", CVSADM, CVSPREFIX, file);

    /*
     * Move the user file to a backup file, so as to preserve its
     * modification times, then place a copy back in the original file name
     * for the checkin and checkout.
     */
    if (!noexec)
	copy_file (file, fname);

    run_setup ("%s%s -f %s%s", Rcsbin, RCS_CI,
	       rev ? "-r" : "", rev ? rev : "");
    run_args ("-m%s", (*message == '\0' || strcmp(message, "\n") == 0) ?
	"*** empty log message ***\n" : message);
    run_arg (rcs);

    switch (run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL))
    {
	case 0:			/* everything normal */

	    /*
	     * The checkin succeeded, so now check the new file back out and
	     * see if it matches exactly with the one we checked in. If it
	     * does, just move the original user file back, thus preserving
	     * the modes; otherwise, we have no recourse but to leave the
	     * newly checkout file as the user file and remove the old
	     * original user file.
	     */

	    if (strcmp (options, "-V4") == 0) /* upgrade to V5 now */
		options[0] = '\0';
	    run_setup ("%s%s -q %s %s%s", Rcsbin, RCS_CO, options,
		       rev ? "-r" : "", rev ? rev : "");
	    run_arg (rcs);
	    (void) run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
	    xchmod (file, 1);
	    if (xcmp (file, fname) == 0)
	    {
		rename_file (fname, file);
		/* the time was correct, so leave it alone */
		set_time = 0;
	    }
	    else
	    {
		(void) unlink_file (fname);
		/* sync up with the time from the RCS file */
		set_time = 1;
	    }

	    /*
	     * If we want read-only files, muck the permissions here, before
	     * getting the file time-stamp.
	     */
	    if (cvswrite == FALSE)
		xchmod (file, 0);

	    /* for added files with symbolic tags, need to add the tag too */
	    if (type == 'A' && tag && !isdigit (*tag))
	    {
		run_setup ("%s%s -q -N%s:%s", Rcsbin, RCS, tag, rev);
		run_arg (rcs);
		(void) run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
	    }

	    /* re-register with the new data */
	    vers = Version_TS (repository, (char *) NULL, tag, (char *) NULL,
			       file, 1, set_time, entries, (List *) NULL);
	    if (strcmp (vers->options, "-V4") == 0)
		vers->options[0] = '\0';
	    Register (entries, file, vers->vn_rcs, vers->ts_user,
		      vers->options, vers->tag, vers->date, (char *) 0);
	    history_write (type, (char *) 0, vers->vn_rcs, file, repository);
	    freevers_ts (&vers);
	    break;

	case -1:			/* fork failed */
	    if (!noexec)
		error (1, errno, "could not check in %s -- fork failed", file);
	    return (1);

	default:			/* ci failed */

	    /*
	     * The checkin failed, for some unknown reason, so we restore the
	     * original user file, print an error, and return an error
	     */
	    if (!noexec)
	    {
		rename_file (fname, file);
		error (0, 0, "could not check in %s", file);
	    }
	    return (1);
    }

    /*
     * When checking in a specific revision, we may have locked the wrong
     * branch, so to be sure, we do an extra unlock here before
     * returning.
     */
    if (rev)
    {
	run_setup ("%s%s -q -u", Rcsbin, RCS);
	run_arg (rcs);
	(void) run_exec (RUN_TTY, RUN_TTY, DEVNULL, RUN_NORMAL);
    }

    return (0);
}
