/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "$CVSid: @(#)classify.c 1.17 94/10/07 $";
USE(rcsid)
#endif

static void sticky_ck PROTO((char *file, int aflag, Vers_TS * vers, List * entries));

/*
 * Classify the state of a file
 */
Ctype
Classify_File (file, tag, date, options, force_tag_match, aflag, repository,
	       entries, srcfiles, versp, update_dir, pipeout)
    char *file;
    char *tag;
    char *date;
    char *options;
    int force_tag_match;
    int aflag;
    char *repository;
    List *entries;
    List *srcfiles;
    Vers_TS **versp;
    char *update_dir;
    int pipeout;
{
    Vers_TS *vers;
    Ctype ret;
    char *fullname;

    fullname = xmalloc (strlen (update_dir) + strlen (file) + 10);
    if (update_dir[0] == '\0')
	strcpy (fullname, file);
    else
	sprintf (fullname, "%s/%s", update_dir, file);

    /* get all kinds of good data about the file */
    vers = Version_TS (repository, options, tag, date, file,
		       force_tag_match, 0, entries, srcfiles);

    if (vers->vn_user == NULL)
    {
	/* No entry available, ts_rcs is invalid */
	if (vers->vn_rcs == NULL)
	{
	    /* there is no RCS file either */
	    if (vers->ts_user == NULL)
	    {
		/* there is no user file */
		if (!force_tag_match || !(vers->tag || vers->date))
		    if (!really_quiet)
			error (0, 0, "nothing known about %s", fullname);
		ret = T_UNKNOWN;
	    }
	    else
	    {
		/* there is a user file */
		if (!force_tag_match || !(vers->tag || vers->date))
		    if (!really_quiet)
			error (0, 0, "use `cvs add' to create an entry for %s",
			       fullname);
		ret = T_UNKNOWN;
	    }
	}
	else
	{
	    /* there is an rcs file */

	    if (vers->ts_user == NULL)
	    {
		/* There is no user file; needs checkout */
		ret = T_CHECKOUT;
	    }
	    else
	    {
		if (pipeout)
		{
		    /*
		     * The user file doesn't necessarily have anything
		     * to do with this.
		     */
		    ret = T_CHECKOUT;
		}
		/*
		 * There is a user file; print a warning and add it to the
		 * conflict list, only if it is indeed different from what we
		 * plan to extract
		 */
		else if (No_Difference (file, vers, entries,
					repository, update_dir))
		{
		    /* the files were different so it is a conflict */
		    if (!really_quiet)
			error (0, 0, "move away %s; it is in the way",
			       fullname);
		    ret = T_CONFLICT;
		}
		else
		    /* since there was no difference, still needs checkout */
		    ret = T_CHECKOUT;
	    }
	}
    }
    else if (strcmp (vers->vn_user, "0") == 0)
    {
	/* An entry for a new-born file; ts_rcs is dummy */

	if (vers->ts_user == NULL)
	{
	    /*
	     * There is no user file, but there should be one; remove the
	     * entry
	     */
	    if (!really_quiet)
		error (0, 0, "warning: new-born %s has disappeared", fullname);
	    ret = T_REMOVE_ENTRY;
	}
	else
	{
	    /* There is a user file */

	    if (vers->vn_rcs == NULL)
		/* There is no RCS file, added file */
		ret = T_ADDED;
	    else
	    {
		/*
		 * There is an RCS file, so someone else must have checked
		 * one in behind our back; conflict
		 */
		if (!really_quiet)
		    error (0, 0,
			"conflict: %s created independently by second party",
			   fullname);
		ret = T_CONFLICT;
	    }
	}
    }
    else if (vers->vn_user[0] == '-')
    {
	/* An entry for a removed file, ts_rcs is invalid */

	if (vers->ts_user == NULL)
	{
	    char tmp[PATH_MAX];

	    /* There is no user file (as it should be) */

	    (void) sprintf (tmp, "-%s", vers->vn_rcs ? vers->vn_rcs : "");

	    if (vers->vn_rcs == NULL)
	    {

		/*
		 * There is no RCS file; this is all-right, but it has been
		 * removed independently by a second party; remove the entry
		 */
		ret = T_REMOVE_ENTRY;
	    }
	    else if (strcmp (tmp, vers->vn_user) == 0)

		/*
		 * The RCS file is the same version as the user file was, and
		 * that's OK; remove it
		 */
		ret = T_REMOVED;
	    else
	    {

		/*
		 * The RCS file is a newer version than the removed user file
		 * and this is definitely not OK; make it a conflict.
		 */
		if (!really_quiet)
		    error (0, 0,
			   "conflict: removed %s was modified by second party",
			   fullname);
		ret = T_CONFLICT;
	    }
	}
	else
	{
	    /* The user file shouldn't be there */
	    if (!really_quiet)
		error (0, 0, "%s should be removed and is still there",
		       fullname);
	    ret = T_REMOVED;
	}
    }
    else
    {
	/* A normal entry, TS_Rcs is valid */
	if (vers->vn_rcs == NULL)
	{
	    /* There is no RCS file */

	    if (vers->ts_user == NULL)
	    {
		/* There is no user file, so just remove the entry */
		if (!really_quiet)
		    error (0, 0, "warning: %s is not (any longer) pertinent",
			   fullname);
		ret = T_REMOVE_ENTRY;
	    }
	    else if (strcmp (vers->ts_user, vers->ts_rcs) == 0)
	    {

		/*
		 * The user file is still unmodified, so just remove it from
		 * the entry list
		 */
		if (!really_quiet)
		    error (0, 0, "%s is no longer in the repository",
			   fullname);
		ret = T_REMOVE_ENTRY;
	    }
	    else
	    {
		/*
		 * The user file has been modified and since it is no longer
		 * in the repository, a conflict is raised
		 */
		if (No_Difference (file, vers, entries,
				   repository, update_dir))
		{
		    /* they are different -> conflict */
		    if (!really_quiet)
			error (0, 0,
	       "conflict: %s is modified but no longer in the repository",
			   fullname);
		    ret = T_CONFLICT;
		}
		else
		{
		    /* they weren't really different */
		    if (!really_quiet)
			error (0, 0,
			       "warning: %s is not (any longer) pertinent",
			       fullname);
		    ret = T_REMOVE_ENTRY;
		}
	    }
	}
	else if (strcmp (vers->vn_rcs, vers->vn_user) == 0)
	{
	    /* The RCS file is the same version as the user file */

	    if (vers->ts_user == NULL)
	    {

		/*
		 * There is no user file, so note that it was lost and
		 * extract a new version
		 */
		if (strcmp (command_name, "update") == 0)
		    if (!really_quiet)
			error (0, 0, "warning: %s was lost", fullname);
		ret = T_CHECKOUT;
	    }
	    else if (strcmp (vers->ts_user, vers->ts_rcs) == 0)
	    {

		/*
		 * The user file is still unmodified, so nothing special at
		 * all to do -- no lists updated, unless the sticky -k option
		 * has changed.  If the sticky tag has changed, we just need
		 * to re-register the entry
		 */
		if (vers->entdata->options &&
		    strcmp (vers->entdata->options, vers->options) != 0)
		    ret = T_CHECKOUT;
		else
		{
		    sticky_ck (file, aflag, vers, entries);
		    ret = T_UPTODATE;
		}
	    }
	    else
	    {

		/*
		 * The user file appears to have been modified, but we call
		 * No_Difference to verify that it really has been modified
		 */
		if (No_Difference (file, vers, entries,
				   repository, update_dir))
		{

		    /*
		     * they really are different; modified if we aren't
		     * changing any sticky -k options, else needs merge
		     */
#ifdef XXX_FIXME_WHEN_RCSMERGE_IS_FIXED
		    if (strcmp (vers->entdata->options ?
			   vers->entdata->options : "", vers->options) == 0)
			ret = T_MODIFIED;
		    else
			ret = T_NEEDS_MERGE;
#else
		    ret = T_MODIFIED;
		    sticky_ck (file, aflag, vers, entries);
#endif
		}
		else
		{
		    /* file has not changed; check out if -k changed */
		    if (strcmp (vers->entdata->options ?
			   vers->entdata->options : "", vers->options) != 0)
		    {
			ret = T_CHECKOUT;
		    }
		    else
		    {

			/*
			 * else -> note that No_Difference will Register the
			 * file already for us, using the new tag/date. This
			 * is the desired behaviour
			 */
			ret = T_UPTODATE;
		    }
		}
	    }
	}
	else
	{
	    /* The RCS file is a newer version than the user file */

	    if (vers->ts_user == NULL)
	    {
		/* There is no user file, so just get it */

		if (strcmp (command_name, "update") == 0)
		    if (!really_quiet)
			error (0, 0, "warning: %s was lost", fullname);
		ret = T_CHECKOUT;
	    }
	    else if (strcmp (vers->ts_user, vers->ts_rcs) == 0)
	    {

		/*
		 * The user file is still unmodified, so just get it as well
		 */
		ret = T_CHECKOUT;
	    }
	    else
	    {
		if (No_Difference (file, vers, entries,
				   repository, update_dir))
		    /* really modified, needs to merge */
		    ret = T_NEEDS_MERGE;
		else
		    /* not really modified, check it out */
		    ret = T_CHECKOUT;
	    }
	}
    }

    /* free up the vers struct, or just return it */
    if (versp != (Vers_TS **) NULL)
	*versp = vers;
    else
	freevers_ts (&vers);

    free (fullname);

    /* return the status of the file */
    return (ret);
}

static void
sticky_ck (file, aflag, vers, entries)
    char *file;
    int aflag;
    Vers_TS *vers;
    List *entries;
{
    if (aflag || vers->tag || vers->date)
    {
	char *enttag = vers->entdata->tag;
	char *entdate = vers->entdata->date;

	if ((enttag && vers->tag && strcmp (enttag, vers->tag)) ||
	    ((enttag && !vers->tag) || (!enttag && vers->tag)) ||
	    (entdate && vers->date && strcmp (entdate, vers->date)) ||
	    ((entdate && !vers->date) || (!entdate && vers->date)))
	{
	    Register (entries, file, vers->vn_user, vers->ts_rcs,
		      vers->options, vers->tag, vers->date, vers->ts_conflict);
	}
    }
}
