/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Rtag
 * 
 * Add or delete a symbolic name to an RCS file, or a collection of RCS files.
 * Uses the modules database, if necessary.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "$CVSid: @(#)rtag.c 1.61 94/09/30 $";
USE(rcsid)
#endif

static Dtype rtag_dirproc PROTO((char *dir, char *repos, char *update_dir));
static int rtag_fileproc PROTO((char *file, char *update_dir,
			  char *repository, List * entries,
			  List * srcfiles));
static int rtag_proc PROTO((int *pargc, char *argv[], char *xwhere,
		      char *mwhere, char *mfile, int shorten,
		      int local_specified, char *mname, char *msg));
static int rtag_delete PROTO((RCSNode *rcsfile));

static char *symtag;
static char *numtag;
static int delete;			/* adding a tag by default */
static int attic_too;			/* remove tag from Attic files */
static int branch_mode;			/* make an automagic "branch" tag */
static char *date;
static int local;			/* recursive by default */
static int force_tag_match = 1;		/* force by default */
static int force_tag_move;              /* don't move existing tags by default */

static char *rtag_usage[] =
{
    "Usage: %s %s [-QaflRnqF] [-b] [-d] [-r tag|-D date] tag modules...\n",
    "\t-Q\tReally quiet.\n",
    "\t-a\tClear tag from removed files that would not otherwise be tagged.\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, not recursive\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-n\tNo execution of 'tag program'\n",
    "\t-q\tSomewhat quiet.\n",
    "\t-d\tDelete the given Tag.\n",
    "\t-b\tMake the tag a \"branch\" tag, allowing concurrent development.\n",
    "\t-[rD]\tExisting tag or Date.\n",
    "\t-F\tMove tag if it already exists\n",	
    NULL
};

int
rtag (argc, argv)
    int argc;
    char *argv[];
{
    register int i;
    int c;
    DBM *db;
    int run_module_prog = 1;
    int err = 0;

    if (argc == -1)
	usage (rtag_usage);

    optind = 1;
    while ((c = getopt (argc, argv, "FanfQqlRdbr:D:")) != -1)
    {
	switch (c)
	{
	    case 'a':
		attic_too = 1;
		break;
	    case 'n':
		run_module_prog = 0;
		break;
	    case 'Q':
		really_quiet = 1;
		/* FALL THROUGH */
	    case 'q':
		quiet = 1;
		break;
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 'd':
		delete = 1;
		break;
	    case 'f':
		force_tag_match = 0;
		break;
	    case 'b':
		branch_mode = 1;
		break;
	    case 'r':
		numtag = optarg;
		break;
	    case 'D':
		if (date)
		    free (date);
		date = Make_Date (optarg);
		break;
	    case 'F':
		force_tag_move = 1;
		break;
	    case '?':
	    default:
		usage (rtag_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;
    if (argc < 2)
	usage (rtag_usage);
    symtag = argv[0];
    argc--;
    argv++;

    if (date && numtag)
	error (1, 0, "-r and -D options are mutually exclusive");
    if (delete && branch_mode)
	error (0, 0, "warning: -b ignored with -d options");
    RCS_check_tag (symtag);

    db = open_module ();
    for (i = 0; i < argc; i++)
    {
	/* XXX last arg should be repository, but doesn't make sense here */
	history_write ('T', (delete ? "D" : (numtag ? numtag : 
		       (date ? date : "A"))), symtag, argv[i], "");
	err += do_module (db, argv[i], TAG, delete ? "Untagging" : "Tagging",
			  rtag_proc, (char *) NULL, 0, 0, run_module_prog,
			  symtag);
    }
    close_module (db);
    return (err);
}

/*
 * callback proc for doing the real work of tagging
 */
/* ARGSUSED */
static int
rtag_proc (pargc, argv, xwhere, mwhere, mfile, shorten, local_specified,
	   mname, msg)
    int *pargc;
    char *argv[];
    char *xwhere;
    char *mwhere;
    char *mfile;
    int shorten;
    int local_specified;
    char *mname;
    char *msg;
{
    int err = 0;
    int which;
    char repository[PATH_MAX];
    char where[PATH_MAX];

    (void) sprintf (repository, "%s/%s", CVSroot, argv[0]);
    (void) strcpy (where, argv[0]);

    /* if mfile isn't null, we need to set up to do only part of the module */
    if (mfile != NULL)
    {
	char *cp;
	char path[PATH_MAX];

	/* if the portion of the module is a path, put the dir part on repos */
	if ((cp = strrchr (mfile, '/')) != NULL)
	{
	    *cp = '\0';
	    (void) strcat (repository, "/");
	    (void) strcat (repository, mfile);
	    (void) strcat (where, "/");
	    (void) strcat (where, mfile);
	    mfile = cp + 1;
	}

	/* take care of the rest */
	(void) sprintf (path, "%s/%s", repository, mfile);
	if (isdir (path))
	{
	    /* directory means repository gets the dir tacked on */
	    (void) strcpy (repository, path);
	    (void) strcat (where, "/");
	    (void) strcat (where, mfile);
	}
	else
	{
	    int i;

	    /* a file means muck argv */
	    for (i = 1; i < *pargc; i++)
		free (argv[i]);
	    argv[1] = xstrdup (mfile);
	    (*pargc) = 2;
	}
    }

    /* chdir to the starting directory */
    if (chdir (repository) < 0)
    {
	error (0, errno, "cannot chdir to %s", repository);
	return (1);
    }

    if (delete || attic_too || (force_tag_match && numtag))
	which = W_REPOS | W_ATTIC;
    else
	which = W_REPOS;

    /* start the recursion processor */
    err = start_recursion (rtag_fileproc, (int (*) ()) NULL, rtag_dirproc,
			   (int (*) ()) NULL, *pargc - 1, argv + 1, local,
			   which, 0, 1, where, 1, 1);

    return (err);
}

/*
 * Called to tag a particular file, as appropriate with the options that were
 * set above.
 */
/* ARGSUSED */
static int
rtag_fileproc (file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List *entries;
    List *srcfiles;
{
    Node *p;
    RCSNode *rcsfile;
    char *version, *rev;
    int retcode = 0;

    /* find the parsed RCS data */
    p = findnode (srcfiles, file);
    if (p == NULL)
	return (1);
    rcsfile = (RCSNode *) p->data;

    /*
     * For tagging an RCS file which is a symbolic link, you'd best be
     * running with RCS 5.6, since it knows how to handle symbolic links
     * correctly without breaking your link!
     */

    if (delete)
	return (rtag_delete (rcsfile));

    /*
     * If we get here, we are adding a tag.  But, if -a was specified, we
     * need to check to see if a -r or -D option was specified.  If neither
     * was specified and the file is in the Attic, remove the tag.
     */
    if (attic_too && (!numtag && !date))
    {
	if ((rcsfile->flags & VALID) && (rcsfile->flags & INATTIC))
	    return (rtag_delete (rcsfile));
    }

    version = RCS_getversion (rcsfile, numtag, date, force_tag_match);
    if (version == NULL)
    {
	/* If -a specified, clean up any old tags */
	if (attic_too)
	    (void) rtag_delete (rcsfile);

	if (!quiet && !force_tag_match)
	{
	    error (0, 0, "cannot find tag `%s' in `%s'",
		   numtag ? numtag : "head", rcsfile->path);
	    return (1);
	}
	return (0);
    }
    if (numtag && isdigit (*numtag) && strcmp (numtag, version) != 0)
    {

	/*
	 * We didn't find a match for the numeric tag that was specified, but
	 * that's OK.  just pass the numeric tag on to rcs, to be tagged as
	 * specified.  Could get here if one tried to tag "1.1.1" and there
	 * was a 1.1.1 branch with some head revision.  In this case, we want
	 * the tag to reference "1.1.1" and not the revision at the head of
	 * the branch.  Use a symbolic tag for that.
	 */
	rev = branch_mode ? RCS_magicrev (rcsfile, version) : numtag;
	run_setup ("%s%s -q -N%s:%s", Rcsbin, RCS, symtag, numtag);
    }
    else
    {
       char *oversion;
       
       /*
	* As an enhancement for the case where a tag is being re-applied to
	* a large body of a module, make one extra call to Version_Number to
	* see if the tag is already set in the RCS file.  If so, check to
	* see if it needs to be moved.  If not, do nothing.  This will
	* likely save a lot of time when simply moving the tag to the
	* "current" head revisions of a module -- which I have found to be a
	* typical tagging operation.
	*/
       rev = branch_mode ? RCS_magicrev (rcsfile, version) : version;
       oversion = RCS_getversion (rcsfile, symtag, (char *) 0, 1);
       if (oversion != NULL)
       {
	  int isbranch = RCS_isbranch (file, symtag, srcfiles);

	  /*
	   * if versions the same and neither old or new are branches don't
	   * have to do anything
	   */
	  if (strcmp (version, oversion) == 0 && !branch_mode && !isbranch)
	  {
	     free (oversion);
	     free (version);
	     return (0);
	  }
	  
	  if (!force_tag_move) {	/* we're NOT going to move the tag */
	     if (update_dir[0])
		(void) printf ("W %s/%s", update_dir, file);
	     else
		(void) printf ("W %s", file);
	     
	     (void) printf (" : %s already exists on %s %s", 
			    symtag, isbranch ? "branch" : "version", oversion);
	     (void) printf (" : NOT MOVING tag to %s %s\n", 
			    branch_mode ? "branch" : "version", rev);
	     free (oversion);
	     free (version);
	     return (0);
	  }
	  free (oversion);
       }
       run_setup ("%s%s -q -N%s:%s", Rcsbin, RCS, symtag, rev);
    }
    run_arg (rcsfile->path);
    if ((retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL)) != 0)
    {
	error (1, retcode == -1 ? errno : 0,
	       "failed to set tag `%s' to revision `%s' in `%s'",
	       symtag, rev, rcsfile->path);
       free (version);
       return (1);
    }
    free (version);
    return (0);
}

/*
 * If -d is specified, "force_tag_match" is set, so that this call to
 * Version_Number() will return a NULL version string if the symbolic
 * tag does not exist in the RCS file.
 * 
 * If the -r flag was used, numtag is set, and we only delete the
 * symtag from files that have numtag.
 * 
 * This is done here because it's MUCH faster than just blindly calling
 * "rcs" to remove the tag... trust me.
 */
static int
rtag_delete (rcsfile)
    RCSNode *rcsfile;
{
    char *version;
    int retcode;

    if (numtag)
    {
	version = RCS_getversion (rcsfile, numtag, (char *) 0, 1);
	if (version == NULL)
	    return (0);
	free (version);
    }

    version = RCS_getversion (rcsfile, symtag, (char *) 0, 1);
    if (version == NULL)
	return (0);
    free (version);

    run_setup ("%s%s -q -N%s", Rcsbin, RCS, symtag);
    run_arg (rcsfile->path);
    if ((retcode = run_exec (RUN_TTY, RUN_TTY, DEVNULL, RUN_NORMAL)) != 0)
    {
	if (!quiet)
	    error (0, retcode == -1 ? errno : 0,
		   "failed to remove tag `%s' from `%s'", symtag,
		   rcsfile->path);
	return (1);
    }
    return (0);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
rtag_dirproc (dir, repos, update_dir)
    char *dir;
    char *repos;
    char *update_dir;
{
    if (!quiet)
	error (0, 0, "%s %s", delete ? "Untagging" : "Tagging", update_dir);
    return (R_PROCESS);
}
