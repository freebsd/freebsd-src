/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Entries file to Files file
 * 
 * Creates the file Files containing the names that comprise the project, from
 * the Entries file.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "$CVSid: @(#)entries.c 1.44 94/10/07 $";
USE(rcsid)
#endif

static Node *AddEntryNode PROTO((List * list, char *name, char *version,
			   char *timestamp, char *options, char *tag,
			   char *date, char *conflict));

static FILE *entfile;
static char *entfilename;		/* for error messages */

/*
 * Write out the line associated with a node of an entries file
 */
static int
write_ent_proc (node, closure)
     Node *node;
     void *closure;
{
    Entnode *p;

    p = (Entnode *) node->data;
    if (fprintf (entfile, "/%s/%s/%s", node->key, p->version,
		 p->timestamp) == EOF)
	error (1, errno, "cannot write %s", entfilename);
    if (p->conflict)
    {
	if (fprintf (entfile, "+%s", p->conflict) == EOF)
	    error (1, errno, "cannot write %s", entfilename);
    }
    if (fprintf (entfile, "/%s/", p->options) == EOF)
	error (1, errno, "cannot write %s", entfilename);

    if (p->tag)
    {
	if (fprintf (entfile, "T%s\n", p->tag) == EOF)
	    error (1, errno, "cannot write %s", entfilename);
    }
    else if (p->date)
    {
	if (fprintf (entfile, "D%s\n", p->date) == EOF)
	    error (1, errno, "cannot write %s", entfilename);
    }
    else if (fprintf (entfile, "\n") == EOF)
	error (1, errno, "cannot write %s", entfilename);
    return (0);
}

/*
 * write out the current entries file given a list,  making a backup copy
 * first of course
 */
static void
write_entries (list)
    List *list;
{
    /* open the new one and walk the list writing entries */
    entfilename = CVSADM_ENTBAK;
    entfile = open_file (entfilename, "w+");
    (void) walklist (list, write_ent_proc, NULL);
    if (fclose (entfile) == EOF)
	error (1, errno, "error closing %s", entfilename);

    /* now, atomically (on systems that support it) rename it */
    rename_file (entfilename, CVSADM_ENT);
}

/*
 * Removes the argument file from the Entries file if necessary.
 */
void
Scratch_Entry (list, fname)
    List *list;
    char *fname;
{
    Node *node;

    if (trace)
	(void) fprintf (stderr, "-> Scratch_Entry(%s)\n", fname);

    /* hashlookup to see if it is there */
    if ((node = findnode (list, fname)) != NULL)
    {
	delnode (node);			/* delete the node */
	if (!noexec)
	    write_entries (list);	/* re-write the file */
    }
}

/*
 * Enters the given file name/version/time-stamp into the Entries file,
 * removing the old entry first, if necessary.
 */
void
Register (list, fname, vn, ts, options, tag, date, ts_conflict)
    List *list;
    char *fname;
    char *vn;
    char *ts;
    char *options;
    char *tag;
    char *date;
    char *ts_conflict;
{
    int should_write_file = !noexec;
    Node *node;

    if (trace)
    {
	(void) fprintf (stderr, "-> Register(%s, %s, %s%s%s, %s, %s %s)\n",
			fname, vn, ts,
			ts_conflict ? "+" : "", ts_conflict ? ts_conflict : "",
			options, tag ? tag : "",	date ? date : "");
    }

    /* was it already there? */
    if ((node = findnode (list, fname)) != NULL)
    {
	/* take it out */
	delnode (node);

	/* add the new one and re-write the file */
	(void) AddEntryNode (list, fname, vn, ts, options, tag,
			     date, ts_conflict);

	if (should_write_file)
	    write_entries (list);
    }
    else
    {
	/* add the new one */
	node = AddEntryNode (list, fname, vn, ts, options, tag,
			     date, ts_conflict);

	if (should_write_file)
	{
	    /* append it to the end */
	    entfilename = CVSADM_ENT;
	    entfile = open_file (entfilename, "a");
	    (void) write_ent_proc (node, NULL);
	    if (fclose (entfile) == EOF)
		error (1, errno, "error closing %s", entfilename);
	}
    }
}

/*
 * Node delete procedure for list-private sticky dir tag/date info
 */
static void
freesdt (p)
    Node *p;
{
    struct stickydirtag *sdtp;

    sdtp = (struct stickydirtag *) p->data;
    if (sdtp->tag)
	free (sdtp->tag);
    if (sdtp->date)
	free (sdtp->date);
    if (sdtp->options)
	free (sdtp->options);
    free ((char *) sdtp);
}

/*
 * Read the entries file into a list, hashing on the file name.
 */
List *
ParseEntries (aflag)
    int aflag;
{
    List *entries;
    char line[MAXLINELEN];
    char *cp, *user, *vn, *ts, *options;
    char *tag_or_date, *tag, *date, *ts_conflict;
    char *dirtag, *dirdate;
    int lineno = 0;
    int do_rewrite = 0;
    FILE *fpin;

    vn = ts = options = tag = date = ts_conflict = 0;

    /* get a fresh list... */
    entries = getlist ();

    /*
     * Parse the CVS/Tag file, to get any default tag/date settings. Use
     * list-private storage to tuck them away for Version_TS().
     */
    ParseTag (&dirtag, &dirdate);
    if (aflag || dirtag || dirdate)
    {
	struct stickydirtag *sdtp;

	sdtp = (struct stickydirtag *) xmalloc (sizeof (*sdtp));
	memset ((char *) sdtp, 0, sizeof (*sdtp));
	sdtp->aflag = aflag;
	sdtp->tag = xstrdup (dirtag);
	sdtp->date = xstrdup (dirdate);

	/* feed it into the list-private area */
	entries->list->data = (char *) sdtp;
	entries->list->delproc = freesdt;
    }

  again:
    fpin = fopen (CVSADM_ENT, "r");
    if (fpin == NULL)
	error (0, errno, "cannot open %s for reading", CVSADM_ENT);
    else
    {
	while (fgets (line, sizeof (line), fpin) != NULL)
	{
	    lineno++;
	    if (line[0] == '/')
	    {
		user = line + 1;
		if ((cp = strchr (user, '/')) == NULL)
		    continue;
		*cp++ = '\0';
		vn = cp;
		if ((cp = strchr (vn, '/')) == NULL)
		    continue;
		*cp++ = '\0';
		ts = cp;
		if ((cp = strchr (ts, '/')) == NULL)
		    continue;
		*cp++ = '\0';
		options = cp;
		if ((cp = strchr (options, '/')) == NULL)
		    continue;
		*cp++ = '\0';
		tag_or_date = cp;
		if ((cp = strchr (tag_or_date, '\n')) == NULL)
		    continue;
		*cp = '\0';
		tag = (char *) NULL;
		date = (char *) NULL;
		if (*tag_or_date == 'T')
		    tag = tag_or_date + 1;
		else if (*tag_or_date == 'D')
		    date = tag_or_date + 1;

		if (ts_conflict = strchr (ts, '+'))
		    *ts_conflict++ = '\0';

		/*
		 * XXX - Convert timestamp from old format to new format.
		 *
		 * If the timestamp doesn't match the file's current
		 * mtime, we'd have to generate a string that doesn't
		 * match anyways, so cheat and base it on the existing
		 * string; it doesn't have to match the same mod time.
		 *
		 * For an unmodified file, write the correct timestamp.
		 */
		{
		    struct stat sb;
		    if (strlen (ts) > 30 && stat (user, &sb) == 0)
		    {
			extern char *ctime ();
			char *c = ctime (&sb.st_mtime);

			if (!strncmp (ts + 25, c, 24))
			    ts = time_stamp (user);
			else
			{
			    ts += 24;
			    ts[0] = '*';
			}
			do_rewrite = 1;
		    }
		}

		(void) AddEntryNode (entries, user, vn, ts, options, tag,
				     date, ts_conflict);
	    }
	    else
	    {
		/* try conversion only on first line */
		if (lineno == 1)
		{
		    (void) fclose (fpin);
		    check_entries ((char *) NULL);
		    goto again;
		}
	    }
	}
    }

    if (do_rewrite && !noexec)
	write_entries (entries);

    /* clean up and return */
    if (fpin)
	(void) fclose (fpin);
    if (dirtag)
	free (dirtag);
    if (dirdate)
	free (dirdate);
    return (entries);
}

/*
 * Look at the entries file to determine if it is in the old entries format.
 * If so, convert it to the new format.
 */
void
check_entries (dir)
    char *dir;
{
    FILE *fpin, *fpout;
    char tmp[MAXLINELEN];
    char line[MAXLINELEN];
    char entname[MAXLINELEN];
    char entbak[MAXLINELEN];
    char *cp, *user, *rev, *ts, *opt;

    if (dir != NULL)
    {
	(void) sprintf (entname, "%s/%s", dir, CVSADM_ENT);
	(void) sprintf (entbak, "%s/%s", dir, CVSADM_ENTBAK);
    }
    else
    {
	(void) strcpy (entname, CVSADM_ENT);
	(void) strcpy (entbak, CVSADM_ENTBAK);
    }

    fpin = open_file (entname, "r");
    if (fgets (line, sizeof (line), fpin) == NULL)
    {
	(void) fclose (fpin);
	return;
    }
    (void) fclose (fpin);
    if (line[0] != '/')
    {
	rename_file (entname, entbak);
	fpin = open_file (entbak, "r");
	fpout = open_file (entname, "w+");
	while (fgets (line, sizeof (line), fpin) != NULL)
	{
	    if (line[0] == '/')
	    {
		if (fputs (line, fpout) == EOF)
		    error (1, errno, "cannot write %s", CVSADM_ENT);
		continue;
	    }
	    rev = line;
	    if ((ts = strchr (line, '|')) == NULL)
		continue;
	    *ts++ = '\0';
	    if ((user = strrchr (ts, ' ')) == NULL)
		continue;
	    *user++ = '\0';
	    if ((cp = strchr (user, '|')) == NULL)
		continue;
	    *cp = '\0';
	    opt = "";
#ifdef HAVE_RCS5
#ifdef HAD_RCS4
	    opt = "-V4";
#endif
#endif
	    if (fprintf (fpout, "/%s/%s/%s/%s/\n", user, rev, ts, opt) == EOF)
		error (1, errno, "cannot write %s", CVSADM_ENT);
	}
	(void) fclose (fpin);
	if (fclose (fpout) == EOF)
	    error (1, errno, "cannot close %s", entname);

	/* clean up any old Files or Mod files */
	if (dir != NULL)
	    (void) sprintf (tmp, "%s/%s", dir, CVSADM_FILE);
	else
	    (void) strcpy (tmp, CVSADM_FILE);
	if (isfile (tmp))
	    (void) unlink (tmp);

	if (dir != NULL)
	    (void) sprintf (tmp, "%s/%s", dir, CVSADM_MOD);
	else
	    (void) strcpy (tmp, CVSADM_MOD);
	if (isfile (tmp))
	    (void) unlink (tmp);
    }
}

/*
 * Free up the memory associated with the data section of an ENTRIES type
 * node
 */
static void
Entries_delproc (node)
    Node *node;
{
    Entnode *p;

    p = (Entnode *) node->data;
    free (p->version);
    free (p->timestamp);
    free (p->options);
    if (p->tag)
	free (p->tag);
    if (p->date)
	free (p->date);
    if (p->conflict)
	free (p->conflict);
    free ((char *) p);
}

/*
 * Get an Entries file list node, initialize it, and add it to the specified
 * list
 */
static Node *
AddEntryNode (list, name, version, timestamp, options, tag, date, conflict)
    List *list;
    char *name;
    char *version;
    char *timestamp;
    char *options;
    char *tag;
    char *date;
    char *conflict;
{
    Node *p;
    Entnode *entdata;

    /* get a node and fill in the regular stuff */
    p = getnode ();
    p->type = ENTRIES;
    p->delproc = Entries_delproc;

    /* this one gets a key of the name for hashing */
    p->key = xstrdup (name);

    /* malloc the data parts and fill them in */
    p->data = xmalloc (sizeof (Entnode));
    entdata = (Entnode *) p->data;
    entdata->version = xstrdup (version);
    entdata->timestamp = xstrdup (timestamp);
    entdata->options = xstrdup (options);
    if (entdata->options == NULL)
	entdata->options = xstrdup ("");/* must be non-NULL */
    entdata->conflict = xstrdup (conflict);
    entdata->tag = xstrdup (tag);
    entdata->date = xstrdup (date);

    /* put the node into the list */
    if (addnode (list, p) != 0)
	error (0, 0, "Duplicate filename in entries file (%s) -- ignored",
	       name);

    return (p);
}

/*
 * Write out/Clear the CVS/Tag file.
 */
void
WriteTag (dir, tag, date)
    char *dir;
    char *tag;
    char *date;
{
    FILE *fout;
    char tmp[PATH_MAX];

    if (noexec)
	return;

    if (dir == NULL)
	(void) strcpy (tmp, CVSADM_TAG);
    else
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_TAG);

    if (tag || date)
    {
	fout = open_file (tmp, "w+");
	if (tag)
	{
	    if (fprintf (fout, "T%s\n", tag) == EOF)
		error (1, errno, "write to %s failed", tmp);
	}
	else
	{
	    if (fprintf (fout, "D%s\n", date) == EOF)
		error (1, errno, "write to %s failed", tmp);
	}
	if (fclose (fout) == EOF)
	    error (1, errno, "cannot close %s", tmp);
    }
    else
	if (unlink_file (tmp) < 0 && errno != ENOENT)
	    error (1, errno, "cannot remove %s", tmp);
}

/*
 * Parse the CVS/Tag file for the current directory.
 */
void
ParseTag (tagp, datep)
    char **tagp;
    char **datep;
{
    FILE *fp;
    char line[MAXLINELEN];
    char *cp;

    if (tagp)
	*tagp = (char *) NULL;
    if (datep)
	*datep = (char *) NULL;
    fp = fopen (CVSADM_TAG, "r");
    if (fp)
    {
	if (fgets (line, sizeof (line), fp) != NULL)
	{
	    if ((cp = strrchr (line, '\n')) != NULL)
		*cp = '\0';
	    if (*line == 'T' && tagp)
		*tagp = xstrdup (line + 1);
	    else if (*line == 'D' && datep)
		*datep = xstrdup (line + 1);
	}
	(void) fclose (fp);
    }
}
