/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
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

#ifndef lint
static char rcsid[] = "@(#)no_diff.c 1.35 92/03/31";
#endif

int
No_Difference (file, vers, entries)
    char *file;
    Vers_TS *vers;
    List *entries;
{
    Node *p;
    char tmp[L_tmpnam+1];
    int ret;
    char *ts, *options;
    int retcode = 0;

    if (!vers->srcfile || !vers->srcfile->path)
	return (-1);			/* different since we couldn't tell */

    if (vers->entdata && vers->entdata->options)
	options = xstrdup (vers->entdata->options);
    else
	options = xstrdup ("");

    run_setup ("%s%s -p -q -r%s %s", Rcsbin, RCS_CO,
	       vers->vn_user ? vers->vn_user : "", options);
    run_arg (vers->srcfile->path);
    if ((retcode = run_exec (RUN_TTY, tmpnam (tmp), RUN_TTY, RUN_REALLY)) == 0)
    {
	if (!iswritable (file))		/* fix the modes as a side effect */
	    xchmod (file, 1);

	/* do the byte by byte compare */
	if (xcmp (file, tmp) == 0)
	{
	    if (cvswrite == FALSE)	/* fix the modes as a side effect */
		xchmod (file, 0);

	    /* no difference was found, so fix the entries file */
	    ts = time_stamp (file);
	    Register (entries, file,
		      vers->vn_user ? vers->vn_user : vers->vn_rcs, ts,
		      options, vers->tag, vers->date);
	    free (ts);

	    /* update the entdata pointer in the vers_ts structure */
	    p = findnode (entries, file);
	    vers->entdata = (Entnode *) p->data;

	    ret = 0;
	}
	else
	    ret = 1;			/* files were really different */
    }
    else
    {
	error (0, retcode == -1 ? errno : 0,
	       "could not check out revision %s of %s", vers->vn_user, file);
	ret = -1;			/* different since we couldn't tell */
    }

    if (trace)
	(void) fprintf (stderr, "-> unlink(%s)\n", tmp);
    (void) unlink (tmp);
    free (options);
    return (ret);
}
