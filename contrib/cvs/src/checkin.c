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
#include "fileattr.h"
#include "edit.h"

int
Checkin (type, finfo, rcs, rev, tag, options, message)
    int type;
    struct file_info *finfo;
    char *rcs;
    char *rev;
    char *tag;
    char *options;
    char *message;
{
    char *fname;
    Vers_TS *vers;
    int set_time;
    char *tocvsPath = NULL;

    /* Hmm.  This message goes to stdout and the "foo,v  <--  foo"
       message from "ci" goes to stderr.  This doesn't make a whole
       lot of sense, but making everything go to stdout can only be
       gracefully achieved once RCS_checkin is librarified.  */
    cvs_output ("Checking in ", 0);
    cvs_output (finfo->fullname, 0);
    cvs_output (";\n", 0);

    fname = xmalloc (strlen (finfo->file) + 80);
    (void) sprintf (fname, "%s/%s%s", CVSADM, CVSPREFIX, finfo->file);

    /*
     * Move the user file to a backup file, so as to preserve its
     * modification times, then place a copy back in the original file name
     * for the checkin and checkout.
     */

    tocvsPath = wrap_tocvs_process_file (finfo->file);

    if (!noexec)
    {
        if (tocvsPath)
	{
            copy_file (tocvsPath, fname);
	    if (unlink_file_dir (finfo->file) < 0)
		if (! existence_error (errno))
		    error (1, errno, "cannot remove %s", finfo->fullname);
	    copy_file (tocvsPath, finfo->file);
	}
	else
	{
	    copy_file (finfo->file, fname);
	}
    }

    switch (RCS_checkin (rcs, NULL, message, rev, 0))
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

	    /* Reparse the RCS file, so that we can safely call
               RCS_checkout.  FIXME: We could probably calculate
               all the changes.  */
	    freercsnode (&finfo->rcs);
	    finfo->rcs = RCS_parse (finfo->file, finfo->repository);

	    /* FIXME: should be checking for errors.  */
	    (void) RCS_checkout (finfo->rcs, finfo->file, rev,
				 (char *) NULL, options, RUN_TTY,
				 (RCSCHECKOUTPROC) NULL, (void *) NULL);

	    xchmod (finfo->file, 1);
	    if (xcmp (finfo->file, fname) == 0)
	    {
		rename_file (fname, finfo->file);
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

	    wrap_fromcvs_process_file (finfo->file);

	    /*
	     * If we want read-only files, muck the permissions here, before
	     * getting the file time-stamp.
	     */
	    if (cvswrite == FALSE || fileattr_get (finfo->file, "_watched"))
		xchmod (finfo->file, 0);

	    /* Re-register with the new data.  */
	    vers = Version_TS (finfo, NULL, tag, NULL, 1, set_time);
	    if (strcmp (vers->options, "-V4") == 0)
		vers->options[0] = '\0';
	    Register (finfo->entries, finfo->file, vers->vn_rcs, vers->ts_user,
		      vers->options, vers->tag, vers->date, (char *) 0);
	    history_write (type, NULL, vers->vn_rcs,
			   finfo->file, finfo->repository);

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
		       finfo->fullname);
	    free (fname);
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
		rename_file (fname, finfo->file);
		error (0, 0, "could not check in %s", finfo->fullname);
	    }
	    free (fname);
	    return (1);
    }

    /*
     * When checking in a specific revision, we may have locked the wrong
     * branch, so to be sure, we do an extra unlock here before
     * returning.
     */
    if (rev)
    {
	(void) RCS_unlock (finfo->rcs, NULL, 1);
    }

#ifdef SERVER_SUPPORT
    if (server_active)
    {
	if (set_time)
	    /* Need to update the checked out file on the client side.  */
	    server_updated (finfo, vers, SERVER_UPDATED,
			    NULL, NULL);
	else
	    server_checked_in (finfo->file, finfo->update_dir, finfo->repository);
    }
    else
#endif
	mark_up_to_date (finfo->file);

    freevers_ts (&vers);
    free (fname);
    return (0);
}
