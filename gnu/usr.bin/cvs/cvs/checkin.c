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
static const char rcsid[] = "$CVSid: @(#)checkin.c 1.48 94/10/07 $";
USE(rcsid);
#endif

int
Checkin (type, file, update_dir, repository,
	 rcs, rev, tag, options, message, entries)
    int type;
    char *file;
    char *update_dir;
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
    char *fullname;

    char *tocvsPath = NULL;

    fullname = xmalloc (strlen (update_dir) + strlen (file) + 10);
    if (update_dir[0] == '\0')
	strcpy (fullname, file);
    else
	sprintf (fullname, "%s/%s", update_dir, file);

    (void) printf ("Checking in %s;\n", fullname);
    (void) sprintf (fname, "%s/%s%s", CVSADM, CVSPREFIX, file);

    /*
     * Move the user file to a backup file, so as to preserve its
     * modification times, then place a copy back in the original file name
     * for the checkin and checkout.
     */

    tocvsPath = wrap_tocvs_process_file (fullname);

    if (!noexec)
    {
        if (tocvsPath)
	{
            copy_file (tocvsPath, fname);
	    if (unlink_file_dir (file) < 0)
		if (! existence_error (errno))
		    error (1, errno, "cannot remove %s", file);
	    copy_file (tocvsPath, file);
	}
	else
	{
	    copy_file (file, fname);
	}
    }

    run_setup ("%s%s -f %s%s", Rcsbin, RCS_CI,
	       rev ? "-r" : "", rev ? rev : "");
    run_args ("-m%s", make_message_rcslegal (message));
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
		if (unlink_file (fname) < 0)
		    error (0, errno, "cannot remove %s", fname);
		/* sync up with the time from the RCS file */
		set_time = 1;
	    }

	    wrap_fromcvs_process_file (file);

	    /*
	     * If we want read-only files, muck the permissions here, before
	     * getting the file time-stamp.
	     */
	    if (cvswrite == FALSE)
		xchmod (file, 0);

#ifndef DEATH_SUPPORT
 /* With death_support, files added with tags go into branches immediately. */

	    /* for added files with symbolic tags, need to add the tag too */
	    if (type == 'A' && tag && !isdigit (*tag))
	    {
		(void) RCS_settag(rcs, tag, rev);
	    }
#endif /* No DEATH_SUPPORT */

	    /* re-register with the new data */
	    vers = Version_TS (repository, (char *) NULL, tag, (char *) NULL,
			       file, 1, set_time, entries, (List *) NULL);
	    if (strcmp (vers->options, "-V4") == 0)
		vers->options[0] = '\0';
	    Register (entries, file, vers->vn_rcs, vers->ts_user,
		      vers->options, vers->tag, vers->date, (char *) 0);
	    history_write (type, (char *) 0, vers->vn_rcs, file, repository);
	    freevers_ts (&vers);

	    if (tocvsPath)
		if (unlink_file_dir (tocvsPath) < 0)
		    error (0, errno, "cannot remove %s", tocvsPath);

	    break;

	case -1:			/* fork failed */
	    if (tocvsPath)
		if (unlink_file_dir (tocvsPath) < 0)
		    error (0, errno, "cannot remove %s", tocvsPath);

	    if (!noexec)
		error (1, errno, "could not check in %s -- fork failed",
		       fullname);
	    return (1);

	default:			/* ci failed */

	    /*
	     * The checkin failed, for some unknown reason, so we restore the
	     * original user file, print an error, and return an error
	     */
	    if (tocvsPath)
		if (unlink_file_dir (tocvsPath) < 0)
		    error (0, errno, "cannot remove %s", tocvsPath);

	    if (!noexec)
	    {
		rename_file (fname, file);
		error (0, 0, "could not check in %s", fullname);
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
	(void) RCS_unlock (rcs, NULL, 1);
    }

#ifdef SERVER_SUPPORT
    if (server_active)
    {
	if (set_time)
	    /* Need to update the checked out file on the client side.  */
	    server_updated (file, update_dir, repository, SERVER_UPDATED,
			    NULL, NULL);
	else
	    server_checked_in (file, update_dir, repository);
    }
#endif

    return (0);
}
