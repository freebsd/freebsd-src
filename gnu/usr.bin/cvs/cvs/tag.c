/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * Tag
 * 
 * Add or delete a symbolic name to an RCS file, or a collection of RCS files.
 * Uses the checked out revision in the current directory.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)tag.c 1.56 92/03/31";
#endif

#if __STDC__
static Dtype tag_dirproc (char *dir, char *repos, char *update_dir);
static int tag_fileproc (char *file, char *update_dir,
			 char *repository, List * entries,
			 List * srcfiles);
#else
static int tag_fileproc ();
static Dtype tag_dirproc ();
#endif				/* __STDC__ */

static char *symtag;
static int delete;			/* adding a tag by default */
static int branch_mode;			/* make an automagic "branch" tag */
static int local;			/* recursive by default */

static char *tag_usage[] =
{
    "Usage: %s %s [-QlRq] [-b] [-d] tag [files...]\n",
    "\t-Q\tReally quiet.\n",
    "\t-l\tLocal directory only, not recursive.\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-q\tSomewhat quiet.\n",
    "\t-d\tDelete the given Tag.\n",
    "\t-b\tMake the tag a \"branch\" tag, allowing concurrent development.\n",
    NULL
};

int
tag (argc, argv)
    int argc;
    char *argv[];
{
    int c;
    int err = 0;

    if (argc == -1)
	usage (tag_usage);

    optind = 1;
    while ((c = gnu_getopt (argc, argv, "QqlRdb")) != -1)
    {
	switch (c)
	{
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
	    case 'b':
		branch_mode = 1;
		break;
	    case '?':
	    default:
		usage (tag_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (argc == 0)
	usage (tag_usage);
    symtag = argv[0];
    argc--;
    argv++;

    if (delete && branch_mode)
	error (0, 0, "warning: -b ignored with -d options");
    RCS_check_tag (symtag);

    /* start the recursion processor */
    err = start_recursion (tag_fileproc, (int (*) ()) NULL, tag_dirproc,
			   (int (*) ()) NULL, argc, argv, local,
			   W_LOCAL, 0, 1, (char *) NULL, 1);
    return (err);
}

/*
 * Called to tag a particular file (the currently checked out version is
 * tagged with the specified tag - or the specified tag is deleted).
 */
/* ARGSUSED */
static int
tag_fileproc (file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List *entries;
    List *srcfiles;
{
    char *version, *oversion;
    char *rev;
    Vers_TS *vers;
    int retcode = 0;

    vers = Version_TS (repository, (char *) NULL, (char *) NULL, (char *) NULL,
		       file, 0, 0, entries, srcfiles);

    if (delete)
    {

	/*
	 * If -d is specified, "force_tag_match" is set, so that this call to
	 * Version_Number() will return a NULL version string if the symbolic
	 * tag does not exist in the RCS file.
	 * 
	 * This is done here because it's MUCH faster than just blindly calling
	 * "rcs" to remove the tag... trust me.
	 */

	version = RCS_getversion (vers->srcfile, symtag, (char *) NULL, 1);
	if (version == NULL || vers->srcfile == NULL)
	{
	    freevers_ts (&vers);
	    return (0);
	}
	free (version);

	run_setup ("%s%s -q -N%s", Rcsbin, RCS, symtag);
	run_arg (vers->srcfile->path);
	if ((retcode = run_exec (RUN_TTY, RUN_TTY, DEVNULL, RUN_NORMAL)) != 0)
	{
	    if (!quiet)
		error (0, retcode == -1 ? errno : 0,
		       "failed to remove tag %s from %s", symtag,
		       vers->srcfile->path);
	    freevers_ts (&vers);
	    return (1);
	}

	/* warm fuzzies */
	if (!really_quiet)
	{
	    if (update_dir[0])
		(void) printf ("D %s/%s\n", update_dir, file);
	    else
		(void) printf ("D %s\n", file);
	}

	freevers_ts (&vers);
	return (0);
    }

    /*
     * If we are adding a tag, we need to know which version we have checked
     * out and we'll tag that version.
     */
    version = vers->vn_user;
    if (version == NULL)
    {
	freevers_ts (&vers);
	return (0);
    }
    else if (strcmp (version, "0") == 0)
    {
	if (!quiet)
	    error (0, 0, "couldn't tag added but un-commited file `%s'", file);
	freevers_ts (&vers);
	return (0);
    }
    else if (version[0] == '-')
    {
	if (!quiet)
	    error (0, 0, "skipping removed but un-commited file `%s'", file);
	freevers_ts (&vers);
	return (0);
    }
    else if (vers->srcfile == NULL)
    {
	if (!quiet)
	    error (0, 0, "cannot find revision control file for `%s'", file);
	freevers_ts (&vers);
	return (0);
    }

    /*
     * As an enhancement for the case where a tag is being re-applied to a
     * large number of files, make one extra call to Version_Number to see if
     * the tag is already set in the RCS file.  If so, check to see if it
     * needs to be moved.  If not, do nothing.  This will likely save a lot of
     * time when simply moving the tag to the "current" head revisions of a
     * module -- which I have found to be a typical tagging operation.
     */
    oversion = RCS_getversion (vers->srcfile, symtag, (char *) NULL, 1);
    if (oversion != NULL)
    {
	if (strcmp (version, oversion) == 0)
	{
	    free (oversion);
	    freevers_ts (&vers);
	    return (0);
	}
	free (oversion);
    }
    rev = branch_mode ? RCS_magicrev (vers->srcfile, version) : version;
    run_setup ("%s%s -q -N%s:%s", Rcsbin, RCS, symtag, rev);
    run_arg (vers->srcfile->path);
    if ((retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL)) != 0)
    {
	if (!quiet)
	    error (0, retcode == -1 ? errno : 0,
		   "failed to set tag %s to revision %s in %s",
		   symtag, rev, vers->srcfile->path);
	freevers_ts (&vers);
	return (1);
    }

    /* more warm fuzzies */
    if (!really_quiet)
    {
	if (update_dir[0])
	    (void) printf ("T %s/%s\n", update_dir, file);
	else
	    (void) printf ("T %s\n", file);
    }

    freevers_ts (&vers);
    return (0);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
tag_dirproc (dir, repos, update_dir)
    char *dir;
    char *repos;
    char *update_dir;
{
    if (!quiet)
	error (0, 0, "%s %s", delete ? "Untagging" : "Tagging", update_dir);
    return (R_PROCESS);
}
