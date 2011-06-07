/*
 * Copyright (C) 1986-2005 The Free Software Foundation, Inc.
 *
 * Portions Copyright (C) 1998-2005 Derek Price, Ximbiot <http://ximbiot.com>,
 *                                  and others.
 *
 * Portions Copyright (C) 1992, Brian Berliner and Jeff Polk
 * Portions Copyright (C) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
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

#include <assert.h>
#include "cvs.h"
#include "fileattr.h"
#include "edit.h"

int
Checkin (type, finfo, rev, tag, options, message)
    int type;
    struct file_info *finfo;
    char *rev;
    char *tag;
    char *options;
    char *message;
{
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

    tocvsPath = wrap_tocvs_process_file (finfo->file);
    if (!noexec)
    {
        if (tocvsPath)
	{
	    if (unlink_file_dir (finfo->file) < 0)
		if (! existence_error (errno))
		    error (1, errno, "cannot remove %s", finfo->fullname);
	    rename_file (tocvsPath, finfo->file);
	}
    }

    /* There use to be a check for finfo->rcs == NULL here and then a
     * call to RCS_parse when necessary, but Checkin() isn't called
     * if the RCS file hasn't already been parsed in one of the
     * check functions.
     */
    assert (finfo->rcs != NULL);

    switch (RCS_checkin (finfo->rcs, finfo->file, message, rev, 0,
                         RCS_FLAGS_KEEPFILE))
    {
	case 0:			/* everything normal */

	    /* The checkin succeeded.  If checking the file out again
               would not cause any changes, we are done.  Otherwise,
               we need to check out the file, which will change the
               modification time of the file.

	       The only way checking out the file could cause any
	       changes is if the file contains RCS keywords.  So we if
	       we are not expanding RCS keywords, we are done.  */

	    if (options != NULL
		&& strcmp (options, "-V4") == 0) /* upgrade to V5 now */
		options[0] = '\0';

	    /* FIXME: If PreservePermissions is on, RCS_cmp_file is
               going to call RCS_checkout into a temporary file
               anyhow.  In that case, it would be more efficient to
               call RCS_checkout here, compare the resulting files
               using xcmp, and rename if necessary.  I think this
               should be fixed in RCS_cmp_file.  */
	    if( ( ! preserve_perms
		  && options != NULL
		  && ( strcmp( options, "-ko" ) == 0
		       || strcmp( options, "-kb" ) == 0 ) )
		|| RCS_cmp_file( finfo->rcs, rev, (char **)NULL, (char *)NULL,
	                         options, finfo->file ) == 0 )
	    {
		/* The existing file is correct.  We don't have to do
                   anything.  */
		set_time = 0;
	    }
	    else
	    {
		/* The existing file is incorrect.  We need to check
                   out the correct file contents.  */
		if (RCS_checkout (finfo->rcs, finfo->file, rev, (char *) NULL,
				  options, RUN_TTY, (RCSCHECKOUTPROC) NULL,
				  (void *) NULL) != 0)
		    error (1, 0, "failed when checking out new copy of %s",
			   finfo->fullname);
		xchmod (finfo->file, 1);
		set_time = 1;
	    }

	    wrap_fromcvs_process_file (finfo->file);

	    /*
	     * If we want read-only files, muck the permissions here, before
	     * getting the file time-stamp.
	     */
	    if (!cvswrite || fileattr_get (finfo->file, "_watched"))
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
	    return (1);

	default:			/* ci failed */

	    /* The checkin failed, for some unknown reason, so we
	       print an error, and return an error.  We assume that
	       the original file has not been touched.  */
	    if (tocvsPath)
		if (unlink_file_dir (tocvsPath) < 0)
		    error (0, errno, "cannot remove %s", tocvsPath);

	    if (!noexec)
		error (0, 0, "could not check in %s", finfo->fullname);
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
	RCS_rewrite (finfo->rcs, NULL, NULL);
    }

#ifdef SERVER_SUPPORT
    if (server_active)
    {
	if (set_time)
	    /* Need to update the checked out file on the client side.  */
	    server_updated (finfo, vers, SERVER_UPDATED,
			    (mode_t) -1, (unsigned char *) NULL,
			    (struct buffer *) NULL);
	else
	    server_checked_in (finfo->file, finfo->update_dir, finfo->repository);
    }
    else
#endif
	mark_up_to_date (finfo->file);

    freevers_ts (&vers);
    return 0;
}
