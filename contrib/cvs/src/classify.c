/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 */

#include "cvs.h"

static void sticky_ck PROTO ((struct file_info *finfo, int aflag,
			      Vers_TS * vers));

/*
 * Classify the state of a file
 */
Ctype
Classify_File (finfo, tag, date, options, force_tag_match, aflag, versp,
	       pipeout)
    struct file_info *finfo;
    char *tag;
    char *date;

    /* Keyword expansion options.  Can be either NULL or "" to
       indicate none are specified here.  */
    char *options;

    int force_tag_match;
    int aflag;
    Vers_TS **versp;
    int pipeout;
{
    Vers_TS *vers;
    Ctype ret;

    /* get all kinds of good data about the file */
    vers = Version_TS (finfo, options, tag, date,
		       force_tag_match, 0);

    if (vers->vn_user == NULL)
    {
	/* No entry available, ts_rcs is invalid */
	if (vers->vn_rcs == NULL)
	{
	    /* there is no RCS file either */
	    if (vers->ts_user == NULL)
	    {
		/* there is no user file */
		/* FIXME: Why do we skip this message if vers->tag or
		   vers->date is set?  It causes "cvs update -r tag98 foo"
		   to silently do nothing, which is seriously confusing
		   behavior.  "cvs update foo" gives this message, which
		   is what I would expect.  */
		if (!force_tag_match || !(vers->tag || vers->date))
		    if (!really_quiet)
			error (0, 0, "nothing known about %s", finfo->fullname);
		ret = T_UNKNOWN;
	    }
	    else
	    {
		/* there is a user file */
		/* FIXME: Why do we skip this message if vers->tag or
		   vers->date is set?  It causes "cvs update -r tag98 foo"
		   to silently do nothing, which is seriously confusing
		   behavior.  "cvs update foo" gives this message, which
		   is what I would expect.  */
		if (!force_tag_match || !(vers->tag || vers->date))
		    if (!really_quiet)
			error (0, 0, "use `%s add' to create an entry for %s",
			       program_name, finfo->fullname);
		ret = T_UNKNOWN;
	    }
	}
	else if (RCS_isdead (vers->srcfile, vers->vn_rcs))
	{
	    /* there is an RCS file, but it's dead */
	    if (vers->ts_user == NULL)
		ret = T_UPTODATE;
	    else
	    {
		error (0, 0, "use `%s add' to create an entry for %s",
		       program_name, finfo->fullname);
		ret = T_UNKNOWN;
	    }
	}
	else if (!pipeout && vers->ts_user && No_Difference (finfo, vers))
	{
	    /* the files were different so it is a conflict */
	    if (!really_quiet)
		error (0, 0, "move away %s; it is in the way",
		       finfo->fullname);
	    ret = T_CONFLICT;
	}
	else
	    /* no user file or no difference, just checkout */
	    ret = T_CHECKOUT;
    }
    else if (strcmp (vers->vn_user, "0") == 0)
    {
	/* An entry for a new-born file; ts_rcs is dummy */

	if (vers->ts_user == NULL)
	{
	    if (pipeout)
	    {
		ret = T_CHECKOUT;
	    }
	    else
	    {
		/*
		 * There is no user file, but there should be one; remove the
		 * entry
		 */
		if (!really_quiet)
		    error (0, 0, "warning: new-born %s has disappeared",
			   finfo->fullname);
		ret = T_REMOVE_ENTRY;
	    }
	}
	else if (vers->vn_rcs == NULL ||
		 RCS_isdead (vers->srcfile, vers->vn_rcs))
	    /* No RCS file or RCS file revision is dead  */
	    ret = T_ADDED;
	else
	{
	    if (pipeout)
	    {
		ret = T_CHECKOUT;
	    }
	    else
	    {
		if (vers->srcfile->flags & INATTIC
		    && vers->srcfile->flags & VALID)
		{
		    /* This file has been added on some branch other than
		       the one we are looking at.  In the branch we are
		       looking at, the file was already valid.  */
		    if (!really_quiet)
			error (0, 0,
			   "conflict: %s has been added, but already exists",
			       finfo->fullname);
		}
		else
		{
		    /*
		     * There is an RCS file, so someone else must have checked
		     * one in behind our back; conflict
		     */
		    if (!really_quiet)
			error (0, 0,
			   "conflict: %s created independently by second party",
			       finfo->fullname);
		}
		ret = T_CONFLICT;
	    }
	}
    }
    else if (vers->vn_user[0] == '-')
    {
	/* An entry for a removed file, ts_rcs is invalid */

	if (vers->ts_user == NULL)
	{
	    /* There is no user file (as it should be) */

	    if (vers->vn_rcs == NULL
		|| RCS_isdead (vers->srcfile, vers->vn_rcs))
	    {

		/*
		 * There is no RCS file; this is all-right, but it has been
		 * removed independently by a second party; remove the entry
		 */
		ret = T_REMOVE_ENTRY;
	    }
	    else if (strcmp (vers->vn_rcs, vers->vn_user + 1) == 0)
		/*
		 * The RCS file is the same version as the user file was, and
		 * that's OK; remove it
		 */
		ret = T_REMOVED;
	    else if (pipeout)
		/*
		 * The RCS file doesn't match the user's file, but it doesn't
		 * matter in this case
		 */
		ret = T_NEEDS_MERGE;
	    else
	    {

		/*
		 * The RCS file is a newer version than the removed user file
		 * and this is definitely not OK; make it a conflict.
		 */
		if (!really_quiet)
		    error (0, 0,
			   "conflict: removed %s was modified by second party",
			   finfo->fullname);
		ret = T_CONFLICT;
	    }
	}
	else
	{
	    /* The user file shouldn't be there */
	    if (!really_quiet)
		error (0, 0, "%s should be removed and is still there",
		       finfo->fullname);
	    ret = T_REMOVED;
	}
    }
    else
    {
	/* A normal entry, TS_Rcs is valid */
	if (vers->vn_rcs == NULL || RCS_isdead (vers->srcfile, vers->vn_rcs))
	{
	    /* There is no RCS file */

	    if (vers->ts_user == NULL)
	    {
		/* There is no user file, so just remove the entry */
		if (!really_quiet)
		    error (0, 0, "warning: %s is not (any longer) pertinent",
			   finfo->fullname);
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
			   finfo->fullname);
		ret = T_REMOVE_ENTRY;
	    }
	    else if (No_Difference (finfo, vers))
	    {
		/* they are different -> conflict */
		if (!really_quiet)
		    error (0, 0,
	       "conflict: %s is modified but no longer in the repository",
			   finfo->fullname);
		ret = T_CONFLICT;
	    }
	    else
	    {
		/* they weren't really different */
		if (!really_quiet)
		    error (0, 0,
			   "warning: %s is not (any longer) pertinent",
			   finfo->fullname);
		ret = T_REMOVE_ENTRY;
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
		/* Comparing the cvs_cmd_name against "update", in
		   addition to being an ugly way to operate, means
		   that this message does not get printed by the
		   server.  That might be considered just a straight
		   bug, although there is one subtlety: that case also
		   gets hit when a patch fails and the client fetches
		   a file.  I'm not sure there is currently any way
		   for the server to distinguish those two cases.  */
		if (strcmp (cvs_cmd_name, "update") == 0)
		    if (!really_quiet)
			error (0, 0, "warning: %s was lost", finfo->fullname);
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
		/* TODO: decide whether we need to check file permissions
		   for a mismatch, and return T_CONFLICT if so. */
		if (vers->entdata->options &&
		    strcmp (vers->entdata->options, vers->options) != 0)
		    ret = T_CHECKOUT;
		else
		{
		    sticky_ck (finfo, aflag, vers);
		    ret = T_UPTODATE;
		}
	    }
	    else if (No_Difference (finfo, vers))
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
		sticky_ck (finfo, aflag, vers);
#endif
	    }
	    else if (strcmp (vers->entdata->options ?
		       vers->entdata->options : "", vers->options) != 0)
	    {
		/* file has not changed; check out if -k changed */
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
	else
	{
	    /* The RCS file is a newer version than the user file */

	    if (vers->ts_user == NULL)
	    {
		/* There is no user file, so just get it */

		/* See comment at other "update" compare, for more
		   thoughts on this comparison.  */
		if (strcmp (cvs_cmd_name, "update") == 0)
		    if (!really_quiet)
			error (0, 0, "warning: %s was lost", finfo->fullname);
		ret = T_CHECKOUT;
	    }
	    else if (strcmp (vers->ts_user, vers->ts_rcs) == 0)
	    {

		/*
		 * The user file is still unmodified, so just get it as well
		 */
		if (strcmp (vers->entdata->options ?
			    vers->entdata->options : "", vers->options) != 0
		    || (vers->srcfile != NULL
			&& (vers->srcfile->flags & INATTIC) != 0))
		    ret = T_CHECKOUT;
		else
		    ret = T_PATCH;
	    }
	    else if (No_Difference (finfo, vers))
		/* really modified, needs to merge */
		ret = T_NEEDS_MERGE;
	    else if ((strcmp (vers->entdata->options ?
			      vers->entdata->options : "", vers->options)
		      != 0)
		     || (vers->srcfile != NULL
		         && (vers->srcfile->flags & INATTIC) != 0))
		/* not really modified, check it out */
		ret = T_CHECKOUT;
	    else
		ret = T_PATCH;
	}
    }

    /* free up the vers struct, or just return it */
    if (versp != (Vers_TS **) NULL)
	*versp = vers;
    else
	freevers_ts (&vers);

    /* return the status of the file */
    return (ret);
}

static void
sticky_ck (finfo, aflag, vers)
    struct file_info *finfo;
    int aflag;
    Vers_TS *vers;
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
	    Register (finfo->entries, finfo->file, vers->vn_user, vers->ts_rcs,
		      vers->options, vers->tag, vers->date, vers->ts_conflict);

#ifdef SERVER_SUPPORT
	    if (server_active)
	    {
		/* We need to update the entries line on the client side.
		   It is possible we will later update it again via
		   server_updated or some such, but that is OK.  */
		server_update_entries
		  (finfo->file, finfo->update_dir, finfo->repository,
		   strcmp (vers->ts_rcs, vers->ts_user) == 0 ?
		   SERVER_UPDATED : SERVER_MERGED);
	    }
#endif
	}
    }
}
