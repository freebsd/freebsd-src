/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 */

#include "cvs.h"

#ifdef SERVER_SUPPORT
static void time_stamp_server PROTO((char *, Vers_TS *, Entnode *));
#endif

/*
 * Fill in and return a Vers_TS structure "user" is the name of the local
 * file; entries is the entries file - preparsed for our pleasure. rcs is
 * the current source control file - preparsed for our pleasure.
 */
Vers_TS *
Version_TS (finfo, options, tag, date, force_tag_match, set_time)
    struct file_info *finfo;
    char *options;
    char *tag;
    char *date;
    int force_tag_match;
    int set_time;
{
    Node *p;
    RCSNode *rcsdata;
    Vers_TS *vers_ts;
    struct stickydirtag *sdtp;
    Entnode *entdata;

    /* get a new Vers_TS struct */
    vers_ts = (Vers_TS *) xmalloc (sizeof (Vers_TS));
    memset ((char *) vers_ts, 0, sizeof (*vers_ts));

    /*
     * look up the entries file entry and fill in the version and timestamp
     * if entries is NULL, there is no entries file so don't bother trying to
     * look it up (used by checkout -P)
     */
    if (finfo->entries == NULL)
    {
	sdtp = NULL;
	p = NULL;
    }
    else
    {
	p = findnode_fn (finfo->entries, finfo->file);
	sdtp = (struct stickydirtag *) finfo->entries->list->data; /* list-private */
    }

    entdata = NULL;
    if (p != NULL)
    {
	entdata = (Entnode *) p->data;

#ifdef SERVER_SUPPORT
	/* An entries line with "D" in the timestamp indicates that the
	   client sent Is-modified without sending Entry.  So we want to
	   use the entries line for the sole purpose of telling
	   time_stamp_server what is up; we don't want the rest of CVS
	   to think there is an entries line.  */
	if (strcmp (entdata->timestamp, "D") != 0)
#endif
	{
	    vers_ts->vn_user = xstrdup (entdata->version);
	    vers_ts->ts_rcs = xstrdup (entdata->timestamp);
	    vers_ts->ts_conflict = xstrdup (entdata->conflict);
	    if (!tag)
	    {
		if (!(sdtp && sdtp->aflag))
		    vers_ts->tag = xstrdup (entdata->tag);
	    }
	    if (!date)
	    {
		if (!(sdtp && sdtp->aflag))
		    vers_ts->date = xstrdup (entdata->date);
	    }
	    if (!options || (options && *options == '\0'))
	    {
		if (!(sdtp && sdtp->aflag))
		    vers_ts->options = xstrdup (entdata->options);
	    }
	    vers_ts->entdata = entdata;
	}
    }

    /*
     * -k options specified on the command line override (and overwrite)
     * options stored in the entries file
     */
    if (options)
	vers_ts->options = xstrdup (options);
    else if (!vers_ts->options)
    {
	if (finfo->rcs != NULL)
	{
	    /* If no keyword expansion was specified on command line,
	       use whatever was in the rcs file (if there is one).  This
	       is how we, if we are the server, tell the client whether
	       a file is binary.  */
	    char *rcsexpand = RCS_getexpand (finfo->rcs);
	    if (rcsexpand != NULL)
	    {
		vers_ts->options = xmalloc (strlen (rcsexpand) + 3);
		strcpy (vers_ts->options, "-k");
		strcat (vers_ts->options, rcsexpand);
	    }
	}
    }
    if (!vers_ts->options)
	vers_ts->options = xstrdup ("");

    /*
     * if tags were specified on the command line, they override what is in
     * the Entries file
     */
    if (tag || date)
    {
	vers_ts->tag = xstrdup (tag);
	vers_ts->date = xstrdup (date);
    }
    else if (!vers_ts->entdata && (sdtp && sdtp->aflag == 0))
    {
	if (!vers_ts->tag)
	{
	    vers_ts->tag = xstrdup (sdtp->tag);
	    vers_ts->nonbranch = sdtp->nonbranch;
	}
	if (!vers_ts->date)
	    vers_ts->date = xstrdup (sdtp->date);
    }

    /* Now look up the info on the source controlled file */
    if (finfo->rcs != NULL)
    {
	rcsdata = finfo->rcs;
	rcsdata->refcount++;
    }
    else if (finfo->repository != NULL)
	rcsdata = RCS_parse (finfo->file, finfo->repository);
    else
	rcsdata = NULL;

    if (rcsdata != NULL)
    {
	/* squirrel away the rcsdata pointer for others */
	vers_ts->srcfile = rcsdata;

	if (vers_ts->tag && strcmp (vers_ts->tag, TAG_BASE) == 0)
	{
	    vers_ts->vn_rcs = xstrdup (vers_ts->vn_user);
	    vers_ts->vn_tag = xstrdup (vers_ts->vn_user);
	}
	else
	{
	    int simple;

	    vers_ts->vn_rcs = RCS_getversion (rcsdata, vers_ts->tag,
					      vers_ts->date, force_tag_match,
					      &simple);
	    if (vers_ts->vn_rcs == NULL)
		vers_ts->vn_tag = NULL;
	    else if (simple)
		vers_ts->vn_tag = xstrdup (vers_ts->tag);
	    else
		vers_ts->vn_tag = xstrdup (vers_ts->vn_rcs);
	}

	/*
	 * If the source control file exists and has the requested revision,
	 * get the Date the revision was checked in.  If "user" exists, set
	 * its mtime.
	 */
	if (set_time)
	{
#ifdef SERVER_SUPPORT
	    if (server_active)
		server_modtime (finfo, vers_ts);
	    else
#endif
	    {
		struct utimbuf t;

		memset (&t, 0, sizeof (t));
		if (vers_ts->vn_rcs)
		{
		    t.modtime =
			RCS_getrevtime (rcsdata, vers_ts->vn_rcs, 0, 0);
		    if (t.modtime != (time_t) -1)
		    {
			t.actime = t.modtime;
			(void) utime (finfo->file, &t);
		    }
		}
	    }
	}
    }

    /* get user file time-stamp in ts_user */
    if (finfo->entries != (List *) NULL)
    {
#ifdef SERVER_SUPPORT
	if (server_active)
	    time_stamp_server (finfo->file, vers_ts, entdata);
	else
#endif
	    vers_ts->ts_user = time_stamp (finfo->file);
    }

    return (vers_ts);
}

#ifdef SERVER_SUPPORT

/* Set VERS_TS->TS_USER to time stamp for FILE.  */

/* Separate these out to keep the logic below clearer.  */
#define mark_lost(V)		((V)->ts_user = 0)
#define mark_unchanged(V)	((V)->ts_user = xstrdup ((V)->ts_rcs))

static void
time_stamp_server (file, vers_ts, entdata)
    char *file;
    Vers_TS *vers_ts;
    Entnode *entdata;
{
    struct stat sb;
    char *cp;

    if ( CVS_STAT (file, &sb) < 0)
    {
	if (! existence_error (errno))
	    error (1, errno, "cannot stat temp file");

	/* Missing file means lost or unmodified; check entries
	   file to see which.

	   XXX FIXME - If there's no entries file line, we
	   wouldn't be getting the file at all, so consider it
	   lost.  I don't know that that's right, but it's not
	   clear to me that either choice is.  Besides, would we
	   have an RCS string in that case anyways?  */
	if (entdata == NULL)
	    mark_lost (vers_ts);
	else if (entdata->timestamp
		 && entdata->timestamp[0] == '=')
	    mark_unchanged (vers_ts);
	else if (entdata->timestamp != NULL
		 && (entdata->timestamp[0] == 'M'
		     || entdata->timestamp[0] == 'D')
		 && entdata->timestamp[1] == '\0')
	    vers_ts->ts_user = xstrdup ("Is-modified");
	else
	    mark_lost (vers_ts);
    }
    else if (sb.st_mtime == 0)
    {
	/* We shouldn't reach this case any more!  */
	abort ();
    }
    else
    {
        struct tm *tm_p;
        struct tm local_tm;

	vers_ts->ts_user = xmalloc (25);
	/* We want to use the same timestamp format as is stored in the
	   st_mtime.  For unix (and NT I think) this *must* be universal
	   time (UT), so that files don't appear to be modified merely
	   because the timezone has changed.  For VMS, or hopefully other
	   systems where gmtime returns NULL, the modification time is
	   stored in local time, and therefore it is not possible to cause
	   st_mtime to be out of sync by changing the timezone.  */
	tm_p = gmtime (&sb.st_mtime);
	if (tm_p)
	{
	    memcpy (&local_tm, tm_p, sizeof (local_tm));
	    cp = asctime (&local_tm);	/* copy in the modify time */
	}
	else
	    cp = ctime (&sb.st_mtime);

	cp[24] = 0;
	(void) strcpy (vers_ts->ts_user, cp);
    }
}

#endif /* SERVER_SUPPORT */
/*
 * Gets the time-stamp for the file "file" and returns it in space it
 * allocates
 */
char *
time_stamp (file)
    char *file;
{
    struct stat sb;
    char *cp;
    char *ts;

    if ( CVS_STAT (file, &sb) < 0)
    {
	ts = NULL;
    }
    else
    {
	struct tm *tm_p;
        struct tm local_tm;
	ts = xmalloc (25);
	/* We want to use the same timestamp format as is stored in the
	   st_mtime.  For unix (and NT I think) this *must* be universal
	   time (UT), so that files don't appear to be modified merely
	   because the timezone has changed.  For VMS, or hopefully other
	   systems where gmtime returns NULL, the modification time is
	   stored in local time, and therefore it is not possible to cause
	   st_mtime to be out of sync by changing the timezone.  */
	tm_p = gmtime (&sb.st_mtime);
	if (tm_p)
	{
	    memcpy (&local_tm, tm_p, sizeof (local_tm));
	    cp = asctime (&local_tm);	/* copy in the modify time */
	}
	else
	    cp = ctime(&sb.st_mtime);

	cp[24] = 0;
	(void) strcpy (ts, cp);
    }

    return (ts);
}

/*
 * free up a Vers_TS struct
 */
void
freevers_ts (versp)
    Vers_TS **versp;
{
    if ((*versp)->srcfile)
	freercsnode (&((*versp)->srcfile));
    if ((*versp)->vn_user)
	free ((*versp)->vn_user);
    if ((*versp)->vn_rcs)
	free ((*versp)->vn_rcs);
    if ((*versp)->vn_tag)
	free ((*versp)->vn_tag);
    if ((*versp)->ts_user)
	free ((*versp)->ts_user);
    if ((*versp)->ts_rcs)
	free ((*versp)->ts_rcs);
    if ((*versp)->options)
	free ((*versp)->options);
    if ((*versp)->tag)
	free ((*versp)->tag);
    if ((*versp)->date)
	free ((*versp)->date);
    if ((*versp)->ts_conflict)
	free ((*versp)->ts_conflict);
    free ((char *) *versp);
    *versp = (Vers_TS *) NULL;
}
