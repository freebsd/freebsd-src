/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * No Difference
 * 
 * The user file looks modified judging from its time stamp; however it needn't
 * be.  No_difference() finds out whether it is or not. If it is not, it
 * updates the administration.
 * 
 * returns 0 if no differences are found and non-zero otherwise
 */

#include "cvs.h"

int
No_Difference (file, vers, entries, repository, update_dir)
    char *file;
    Vers_TS *vers;
    List *entries;
    char *repository;
    char *update_dir;
{
    Node *p;
    char tmp[L_tmpnam+1];
    int ret;
    char *ts, *options;
    int retcode = 0;
    char *tocvsPath;

    if (!vers->srcfile || !vers->srcfile->path)
	return (-1);			/* different since we couldn't tell */

    if (vers->entdata && vers->entdata->options)
	options = xstrdup (vers->entdata->options);
    else
	options = xstrdup ("");

    retcode = RCS_checkout (vers->srcfile->path, NULL, vers->vn_user, options,
                            tmpnam (tmp), 0, 0);
    if (retcode == 0)
    {
#if 0
	/* Why would we want to munge the modes?  And only if the timestamps
	   are different?  And even for commands like "cvs status"????  */
	if (!iswritable (file))		/* fix the modes as a side effect */
	    xchmod (file, 1);
#endif

	tocvsPath = wrap_tocvs_process_file (file);

	/* do the byte by byte compare */
	if (xcmp (tocvsPath == NULL ? file : tocvsPath, tmp) == 0)
	{
#if 0
	    /* Why would we want to munge the modes?  And only if the
	       timestamps are different?  And even for commands like
	       "cvs status"????  */
	    if (cvswrite == FALSE)	/* fix the modes as a side effect */
		xchmod (file, 0);
#endif

	    /* no difference was found, so fix the entries file */
	    ts = time_stamp (file);
	    Register (entries, file,
		      vers->vn_user ? vers->vn_user : vers->vn_rcs, ts,
		      options, vers->tag, vers->date, (char *) 0);
#ifdef SERVER_SUPPORT
	    if (server_active)
	    {
		/* We need to update the entries line on the client side.  */
		server_update_entries
		  (file, update_dir, repository, SERVER_UPDATED);
	    }
#endif
	    free (ts);

	    /* update the entdata pointer in the vers_ts structure */
	    p = findnode (entries, file);
	    vers->entdata = (Entnode *) p->data;

	    ret = 0;
	}
	else
	    ret = 1;			/* files were really different */
        if (tocvsPath)
	{
	    /* Need to call unlink myself because the noexec variable
	     * has been set to 1.  */
	    if (trace)
		(void) fprintf (stderr, "%c-> unlink (%s)\n",
#ifdef SERVER_SUPPORT
				(server_active) ? 'S' : ' ',
#else
				' ',
#endif
				tocvsPath);
	    if (unlink (tocvsPath) < 0)
		error (0, errno, "could not remove %s", tocvsPath);
	}
    }
    else
    {
	if (update_dir[0] == '\0')
	    error (0, retcode == -1 ? errno : 0,
		   "could not check out revision %s of %s",
		   vers->vn_user, file);
	else
	    error (0, retcode == -1 ? errno : 0,
		   "could not check out revision %s of %s/%s",
		   vers->vn_user, update_dir, file);
	ret = -1;			/* different since we couldn't tell */
    }

    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> unlink2 (%s)\n",
			(server_active) ? 'S' : ' ', tmp);
#else
	(void) fprintf (stderr, "-> unlink (%s)\n", tmp);
#endif
    if (unlink (tmp) < 0)
	error (0, errno, "could not remove %s", tmp);
    free (options);
    return (ret);
}
