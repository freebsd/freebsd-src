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
#include "getline.h"

static Node *AddEntryNode PROTO((List * list, Entnode *entnode));

static Entnode *fgetentent PROTO((FILE *, char *, int *));
static int   fputentent PROTO((FILE *, Entnode *));

static Entnode *subdir_record PROTO((int, const char *, const char *));

static FILE *entfile;
static char *entfilename;		/* for error messages */

/*
 * Construct an Entnode
 */
static Entnode *Entnode_Create PROTO ((enum ent_type, const char *,
				       const char *, const char *,
				       const char *, const char *,
				       const char *, const char *));

static Entnode *
Entnode_Create(type, user, vn, ts, options, tag, date, ts_conflict)
    enum ent_type type;
    const char *user;
    const char *vn;
    const char *ts;
    const char *options;
    const char *tag;
    const char *date;
    const char *ts_conflict;
{
    Entnode *ent;
    
    /* Note that timestamp and options must be non-NULL */
    ent = (Entnode *) xmalloc (sizeof (Entnode));
    ent->type      = type;
    ent->user      = xstrdup (user);
    ent->version   = xstrdup (vn);
    ent->timestamp = xstrdup (ts ? ts : "");
    ent->options   = xstrdup (options ? options : "");
    ent->tag       = xstrdup (tag);
    ent->date      = xstrdup (date);
    ent->conflict  = xstrdup (ts_conflict);

    return ent;
}

/*
 * Destruct an Entnode
 */
static void Entnode_Destroy PROTO ((Entnode *));

static void
Entnode_Destroy (ent)
    Entnode *ent;
{
    free (ent->user);
    free (ent->version);
    free (ent->timestamp);
    free (ent->options);
    if (ent->tag)
	free (ent->tag);
    if (ent->date)
	free (ent->date);
    if (ent->conflict)
	free (ent->conflict);
    free (ent);
}

/*
 * Write out the line associated with a node of an entries file
 */
static int write_ent_proc PROTO ((Node *, void *));
static int
write_ent_proc (node, closure)
     Node *node;
     void *closure;
{
    Entnode *entnode;

    entnode = (Entnode *) node->data;

    if (closure != NULL && entnode->type != ENT_FILE)
	*(int *) closure = 1;

    if (fputentent(entfile, entnode))
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
    int sawdir;

    sawdir = 0;

    /* open the new one and walk the list writing entries */
    entfilename = CVSADM_ENTBAK;
    entfile = open_file (entfilename, "w+");
    (void) walklist (list, write_ent_proc, (void *) &sawdir);
    if (! sawdir)
    {
	struct stickydirtag *sdtp;

	/* We didn't write out any directories.  Check the list
           private data to see whether subdirectory information is
           known.  If it is, we need to write out an empty D line.  */
	sdtp = (struct stickydirtag *) list->list->data;
	if (sdtp == NULL || sdtp->subdirs)
	    if (fprintf (entfile, "D\n") < 0)
		error (1, errno, "cannot write %s", entfilename);
    }
    if (fclose (entfile) == EOF)
	error (1, errno, "error closing %s", entfilename);

    /* now, atomically (on systems that support it) rename it */
    rename_file (entfilename, CVSADM_ENT);

    /* now, remove the log file */
    unlink_file (CVSADM_ENTLOG);
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
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> Scratch_Entry(%s)\n",
			(server_active) ? 'S' : ' ', fname);
#else
	(void) fprintf (stderr, "-> Scratch_Entry(%s)\n", fname);
#endif

    /* hashlookup to see if it is there */
    if ((node = findnode_fn (list, fname)) != NULL)
    {
	if (!noexec)
	{
	    entfilename = CVSADM_ENTLOG;
	    entfile = open_file (entfilename, "a");

	    if (fprintf (entfile, "R ") < 0)
		error (1, errno, "cannot write %s", entfilename);

	    write_ent_proc (node, NULL);

	    if (fclose (entfile) == EOF)
		error (1, errno, "error closing %s", entfilename);
	}

	delnode (node);			/* delete the node */

#ifdef SERVER_SUPPORT
	if (server_active)
	    server_scratch (fname);
#endif
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
    Entnode *entnode;
    Node *node;

#ifdef SERVER_SUPPORT
    if (server_active)
    {
	server_register (fname, vn, ts, options, tag, date, ts_conflict);
    }
#endif

    if (trace)
    {
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> Register(%s, %s, %s%s%s, %s, %s %s)\n",
			(server_active) ? 'S' : ' ',
			fname, vn, ts ? ts : "",
			ts_conflict ? "+" : "", ts_conflict ? ts_conflict : "",
			options, tag ? tag : "", date ? date : "");
#else
	(void) fprintf (stderr, "-> Register(%s, %s, %s%s%s, %s, %s %s)\n",
			fname, vn, ts ? ts : "",
			ts_conflict ? "+" : "", ts_conflict ? ts_conflict : "",
			options, tag ? tag : "", date ? date : "");
#endif
    }

    entnode = Entnode_Create (ENT_FILE, fname, vn, ts, options, tag, date,
			      ts_conflict);
    node = AddEntryNode (list, entnode);

    if (!noexec)
    {
	entfilename = CVSADM_ENTLOG;
	entfile = open_file (entfilename, "a");

	if (fprintf (entfile, "A ") < 0)
	    error (1, errno, "cannot write %s", entfilename);

	write_ent_proc (node, NULL);

        if (fclose (entfile) == EOF)
	    error (1, errno, "error closing %s", entfilename);
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
    free ((char *) sdtp);
}

static Entnode *
fgetentent(fpin, cmd, sawdir)
    FILE *fpin;
    char *cmd;
    int *sawdir;
{
    Entnode *ent;
    char *line;
    size_t line_chars_allocated;
    register char *cp;
    enum ent_type type;
    char *l, *user, *vn, *ts, *options;
    char *tag_or_date, *tag, *date, *ts_conflict;

    line = NULL;
    line_chars_allocated = 0;

    ent = NULL;
    while (getline (&line, &line_chars_allocated, fpin) > 0)
    {
	l = line;

	/* If CMD is not NULL, we are reading an Entries.Log file.
	   Each line in the Entries.Log file starts with a single
	   character command followed by a space.  For backward
	   compatibility, the absence of a space indicates an add
	   command.  */
	if (cmd != NULL)
	{
	    if (l[1] != ' ')
		*cmd = 'A';
	    else
	    {
		*cmd = l[0];
		l += 2;
	    }
	}

	type = ENT_FILE;

	if (l[0] == 'D')
	{
	    type = ENT_SUBDIR;
	    *sawdir = 1;
	    ++l;
	    /* An empty D line is permitted; it is a signal that this
	       Entries file lists all known subdirectories.  */
	}

	if (l[0] != '/')
	    continue;

	user = l + 1;
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

	if ((ts_conflict = strchr (ts, '+')))
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
	    if (strlen (ts) > 30 && CVS_STAT (user, &sb) == 0)
	    {
		char *c = ctime (&sb.st_mtime);
		
		if (!strncmp (ts + 25, c, 24))
		    ts = time_stamp (user);
		else
		{
		    ts += 24;
		    ts[0] = '*';
		}
	    }
	}

	ent = Entnode_Create (type, user, vn, ts, options, tag, date,
			      ts_conflict);
	break;
    }

    free (line);
    return ent;
}

static int
fputentent(fp, p)
    FILE *fp;
    Entnode *p;
{
    switch (p->type)
    {
    case ENT_FILE:
        break;
    case ENT_SUBDIR:
        if (fprintf (fp, "D") < 0)
	    return 1;
	break;
    }

    if (fprintf (fp, "/%s/%s/%s", p->user, p->version, p->timestamp) < 0)
	return 1;
    if (p->conflict)
    {
	if (fprintf (fp, "+%s", p->conflict) < 0)
	    return 1;
    }
    if (fprintf (fp, "/%s/", p->options) < 0)
	return 1;

    if (p->tag)
    {
	if (fprintf (fp, "T%s\n", p->tag) < 0)
	    return 1;
    }
    else if (p->date)
    {
	if (fprintf (fp, "D%s\n", p->date) < 0)
	    return 1;
    }
    else 
    {
	if (fprintf (fp, "\n") < 0)
	    return 1;
    }

    return 0;
}


/*
 * Read the entries file into a list, hashing on the file name.
 */
List *
Entries_Open (aflag)
    int aflag;
{
    List *entries;
    struct stickydirtag *sdtp = NULL;
    Entnode *ent;
    char *dirtag, *dirdate;
    int dirnonbranch;
    int do_rewrite = 0;
    FILE *fpin;
    int sawdir;

    /* get a fresh list... */
    entries = getlist ();

    /*
     * Parse the CVS/Tag file, to get any default tag/date settings. Use
     * list-private storage to tuck them away for Version_TS().
     */
    ParseTag (&dirtag, &dirdate, &dirnonbranch);
    if (aflag || dirtag || dirdate)
    {
	sdtp = (struct stickydirtag *) xmalloc (sizeof (*sdtp));
	memset ((char *) sdtp, 0, sizeof (*sdtp));
	sdtp->aflag = aflag;
	sdtp->tag = xstrdup (dirtag);
	sdtp->date = xstrdup (dirdate);
	sdtp->nonbranch = dirnonbranch;

	/* feed it into the list-private area */
	entries->list->data = (char *) sdtp;
	entries->list->delproc = freesdt;
    }

    sawdir = 0;

    fpin = CVS_FOPEN (CVSADM_ENT, "r");
    if (fpin == NULL)
	error (0, errno, "cannot open %s for reading", CVSADM_ENT);
    else
    {
	while ((ent = fgetentent (fpin, (char *) NULL, &sawdir)) != NULL) 
	{
	    (void) AddEntryNode (entries, ent);
	}

	fclose (fpin);
    }

    fpin = CVS_FOPEN (CVSADM_ENTLOG, "r");
    if (fpin != NULL) 
    {
	char cmd;
	Node *node;

	while ((ent = fgetentent (fpin, &cmd, &sawdir)) != NULL)
	{
	    switch (cmd)
	    {
	    case 'A':
		(void) AddEntryNode (entries, ent);
		break;
	    case 'R':
		node = findnode_fn (entries, ent->user);
		if (node != NULL)
		    delnode (node);
		Entnode_Destroy (ent);
		break;
	    default:
		/* Ignore unrecognized commands.  */
	        break;
	    }
	}
	do_rewrite = 1;
	fclose (fpin);
    }

    /* Update the list private data to indicate whether subdirectory
       information is known.  Nonexistent list private data is taken
       to mean that it is known.  */
    if (sdtp != NULL)
	sdtp->subdirs = sawdir;
    else if (! sawdir)
    {
	sdtp = (struct stickydirtag *) xmalloc (sizeof (*sdtp));
	memset ((char *) sdtp, 0, sizeof (*sdtp));
	sdtp->subdirs = 0;
	entries->list->data = (char *) sdtp;
	entries->list->delproc = freesdt;
    }

    if (do_rewrite && !noexec)
	write_entries (entries);

    /* clean up and return */
    if (dirtag)
	free (dirtag);
    if (dirdate)
	free (dirdate);
    return (entries);
}

void
Entries_Close(list)
    List *list;
{
    if (list)
    {
	if (!noexec) 
        {
            if (isfile (CVSADM_ENTLOG))
		write_entries (list);
	}
	dellist(&list);
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
    Entnode_Destroy(p);
}

/*
 * Get an Entries file list node, initialize it, and add it to the specified
 * list
 */
static Node *
AddEntryNode (list, entdata)
    List *list;
    Entnode *entdata;
{
    Node *p;

    /* was it already there? */
    if ((p  = findnode_fn (list, entdata->user)) != NULL)
    {
	/* take it out */
	delnode (p);
    }

    /* get a node and fill in the regular stuff */
    p = getnode ();
    p->type = ENTRIES;
    p->delproc = Entries_delproc;

    /* this one gets a key of the name for hashing */
    /* FIXME This results in duplicated data --- the hash package shouldn't
       assume that the key is dynamically allocated.  The user's free proc
       should be responsible for freeing the key. */
    p->key = xstrdup (entdata->user);
    p->data = (char *) entdata;

    /* put the node into the list */
    addnode (list, p);
    return (p);
}

/*
 * Write out/Clear the CVS/Tag file.
 */
void
WriteTag (dir, tag, date, nonbranch, update_dir, repository)
    char *dir;
    char *tag;
    char *date;
    int nonbranch;
    char *update_dir;
    char *repository;
{
    FILE *fout;
    char *tmp;

    if (noexec)
	return;

    tmp = xmalloc ((dir ? strlen (dir) : 0)
		   + sizeof (CVSADM_TAG)
		   + 10);
    if (dir == NULL)
	(void) strcpy (tmp, CVSADM_TAG);
    else
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_TAG);

    if (tag || date)
    {
	fout = open_file (tmp, "w+");
	if (tag)
	{
	    if (nonbranch)
	    {
		if (fprintf (fout, "N%s\n", tag) < 0)
		    error (1, errno, "write to %s failed", tmp);
	    }
	    else
	    {
		if (fprintf (fout, "T%s\n", tag) < 0)
		    error (1, errno, "write to %s failed", tmp);
	    }
	}
	else
	{
	    if (fprintf (fout, "D%s\n", date) < 0)
		error (1, errno, "write to %s failed", tmp);
	}
	if (fclose (fout) == EOF)
	    error (1, errno, "cannot close %s", tmp);
    }
    else
	if (unlink_file (tmp) < 0 && ! existence_error (errno))
	    error (1, errno, "cannot remove %s", tmp);
    free (tmp);
#ifdef SERVER_SUPPORT
    if (server_active)
	server_set_sticky (update_dir, repository, tag, date, nonbranch);
#endif
}

/* Parse the CVS/Tag file for the current directory.

   If it contains a date, sets *DATEP to the date in a newly malloc'd
   string, *TAGP to NULL, and *NONBRANCHP to an unspecified value.

   If it contains a branch tag, sets *TAGP to the tag in a newly
   malloc'd string, *NONBRANCHP to 0, and *DATEP to NULL.

   If it contains a nonbranch tag, sets *TAGP to the tag in a newly
   malloc'd string, *NONBRANCHP to 1, and *DATEP to NULL.

   If it does not exist, or contains something unrecognized by this
   version of CVS, set *DATEP and *TAGP to NULL and *NONBRANCHP to
   an unspecified value.  */
void
ParseTag (tagp, datep, nonbranchp)
    char **tagp;
    char **datep;
    int *nonbranchp;
{
    FILE *fp;

    if (tagp)
	*tagp = (char *) NULL;
    if (datep)
	*datep = (char *) NULL;
    /* Always store a value here, even in the 'D' case where the value
       is unspecified.  Shuts up tools which check for references to
       uninitialized memory.  */
    if (nonbranchp != NULL)
	*nonbranchp = 0;
    fp = CVS_FOPEN (CVSADM_TAG, "r");
    if (fp)
    {
	char *line;
	int line_length;
	size_t line_chars_allocated;

	line = NULL;
	line_chars_allocated = 0;

	if ((line_length = getline (&line, &line_chars_allocated, fp)) > 0)
	{
	    /* Remove any trailing newline.  */
	    if (line[line_length - 1] == '\n')
	        line[--line_length] = '\0';
	    switch (*line)
	    {
		case 'T':
		    if (tagp != NULL)
			*tagp = xstrdup (line + 1);
		    break;
		case 'D':
		    if (datep != NULL)
			*datep = xstrdup (line + 1);
		    break;
		case 'N':
		    if (tagp != NULL)
			*tagp = xstrdup (line + 1);
		    if (nonbranchp != NULL)
			*nonbranchp = 1;
		    break;
		default:
		    /* Silently ignore it; it may have been
		       written by a future version of CVS which extends the
		       syntax.  */
		    break;
	    }
	}
	(void) fclose (fp);
	free (line);
    }
}

/*
 * This is called if all subdirectory information is known, but there
 * aren't any subdirectories.  It records that fact in the list
 * private data.
 */

void
Subdirs_Known (entries)
     List *entries;
{
    struct stickydirtag *sdtp;

    /* If there is no list private data, that means that the
       subdirectory information is known.  */
    sdtp = (struct stickydirtag *) entries->list->data;
    if (sdtp != NULL && ! sdtp->subdirs)
    {
	FILE *fp;

	sdtp->subdirs = 1;
	if (!noexec)
	{
	    /* Create Entries.Log so that Entries_Close will do something.  */
	    fp = open_file (CVSADM_ENTLOG, "a");
	    if (fclose (fp) == EOF)
		error (1, errno, "cannot close %s", CVSADM_ENTLOG);
	}
    }
}

/* Record subdirectory information.  */

static Entnode *
subdir_record (cmd, parent, dir)
     int cmd;
     const char *parent;
     const char *dir;
{
    Entnode *entnode;

    /* None of the information associated with a directory is
       currently meaningful.  */
    entnode = Entnode_Create (ENT_SUBDIR, dir, "", "", "",
			      (char *) NULL, (char *) NULL,
			      (char *) NULL);

    if (!noexec)
    {
	if (parent == NULL)
	    entfilename = CVSADM_ENTLOG;
	else
	{
	    entfilename = xmalloc (strlen (parent)
				   + sizeof CVSADM_ENTLOG
				   + 10);
	    sprintf (entfilename, "%s/%s", parent, CVSADM_ENTLOG);
	}

	entfile = CVS_FOPEN (entfilename, "a");
	if (entfile == NULL)
	{
	    int save_errno = errno;

	    /* It is not an error if there is no CVS administration
               directory.  Permitting this case simplifies some
               calling code.  */

	    if (parent == NULL)
	    {
		if (! isdir (CVSADM))
		    return entnode;
	    }
	    else
	    {
		sprintf (entfilename, "%s/%s", parent, CVSADM);
		if (! isdir (entfilename))
		{
		    free (entfilename);
		    entfilename = NULL;
		    return entnode;
		}
	    }

	    error (1, save_errno, "cannot open %s", entfilename);
	}

	if (fprintf (entfile, "%c ", cmd) < 0)
	    error (1, errno, "cannot write %s", entfilename);

	if (fputentent (entfile, entnode) != 0)
	    error (1, errno, "cannot write %s", entfilename);

	if (fclose (entfile) == EOF)
	    error (1, errno, "error closing %s", entfilename);

	if (parent != NULL)
	{
	    free (entfilename);
	    entfilename = NULL;
	}
    }

    return entnode;
}

/*
 * Record the addition of a new subdirectory DIR in PARENT.  PARENT
 * may be NULL, which means the current directory.  ENTRIES is the
 * current entries list; it may be NULL, which means that it need not
 * be updated.
 */

void
Subdir_Register (entries, parent, dir)
     List *entries;
     const char *parent;
     const char *dir;
{
    Entnode *entnode;

    /* Ignore attempts to register ".".  These can happen in the
       server code.  */
    if (dir[0] == '.' && dir[1] == '\0')
	return;

    entnode = subdir_record ('A', parent, dir);

    if (entries != NULL && (parent == NULL || strcmp (parent, ".") == 0))
	(void) AddEntryNode (entries, entnode);
    else
	Entnode_Destroy (entnode);
}

/*
 * Record the removal of a subdirectory.  The arguments are the same
 * as for Subdir_Register.
 */

void
Subdir_Deregister (entries, parent, dir)
     List *entries;
     const char *parent;
     const char *dir;
{
    Entnode *entnode;

    entnode = subdir_record ('R', parent, dir);
    Entnode_Destroy (entnode);

    if (entries != NULL && (parent == NULL || strcmp (parent, ".") == 0))
    {
	Node *p;

	p = findnode_fn (entries, dir);
	if (p != NULL)
	    delnode (p);
    }
}
