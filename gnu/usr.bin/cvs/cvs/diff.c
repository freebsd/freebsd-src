/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * Difference
 * 
 * Run diff against versions in the repository.  Options that are specified are
 * passed on directly to "rcsdiff".
 * 
 * Without any file arguments, runs diff against all the currently modified
 * files.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)diff.c 1.52 92/04/10";
#endif

#if __STDC__
static Dtype diff_dirproc (char *dir, char *pos_repos, char *update_dir);
static int diff_dirleaveproc (char *dir, int err, char *update_dir);
static int diff_file_nodiff (char *file, char *repository, List *entries,
			     List *srcfiles, Vers_TS *vers);
static int diff_fileproc (char *file, char *update_dir, char *repository,
			  List * entries, List * srcfiles);
static void diff_mark_errors (int err);
#else
static int diff_fileproc ();
static Dtype diff_dirproc ();
static int diff_dirleaveproc ();
static int diff_file_nodiff ();
static void diff_mark_errors ();
#endif				/* __STDC__ */

static char *diff_rev1, *diff_rev2;
static char *diff_date1, *diff_date2;
static char *use_rev1, *use_rev2;
static char *options;
static char opts[PATH_MAX];
static int diff_errors;

static char *diff_usage[] =
{
    "Usage: %s %s [-l] [rcsdiff-options]\n",
#ifdef CVS_DIFFDATE
    "    [[-r rev1 | -D date1] [-r rev2 | -D date2]] [files...] \n",
#else
    "    [-r rev1 [-r rev2]] [files...] \n",
#endif
    "\t-l\tLocal directory only, not recursive\n",
    "\t-D d1\tDiff revision for date against working file.\n",
    "\t-D d2\tDiff rev1/date1 against date2.\n",
    "\t-r rev1\tDiff revision for rev1 against working file.\n",
    "\t-r rev2\tDiff rev1/date1 against rev2.\n",
    NULL
};

int
diff (argc, argv)
    int argc;
    char *argv[];
{
    char tmp[50];
    int c, err = 0;
    int local = 0;

    if (argc == -1)
	usage (diff_usage);

    /*
     * Note that we catch all the valid arguments here, so that we can
     * intercept the -r arguments for doing revision diffs; and -l/-R for a
     * non-recursive/recursive diff.
     */
    optind = 1;
    while ((c = gnu_getopt (argc, argv,
		   "abcdefhilnpqtuw0123456789BHQRTC:D:F:I:L:V:k:r:")) != -1)
    {
	switch (c)
	{
	    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	    case 'h': case 'i': case 'n': case 'p': case 't': case 'u':
	    case 'w': case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9': case 'B':
	    case 'H': case 'T': case 'Q':
		(void) sprintf (tmp, " -%c", (char) c);
		(void) strcat (opts, tmp);
		if (c == 'Q')
		{
		    quiet = 1;
		    really_quiet = 1;
		    c = 'q';
		}
		break;
	    case 'C': case 'F': case 'I': case 'L': case 'V':
#ifndef CVS_DIFFDATE
	    case 'D':
#endif
		(void) sprintf (tmp, " -%c%s", (char) c, optarg);
		(void) strcat (opts, tmp);
		break;
	    case 'R':
		local = 0;
		break;
	    case 'l':
		local = 1;
		break;
	    case 'q':
		quiet = 1;
		break;
	    case 'k':
		if (options)
		    free (options);
		options = RCS_check_kflag (optarg);
		break;
	    case 'r':
		if (diff_rev2 != NULL || diff_date2 != NULL)
		    error (1, 0,
		       "no more than two revisions/dates can be specified");
		if (diff_rev1 != NULL || diff_date1 != NULL)
		    diff_rev2 = optarg;
		else
		    diff_rev1 = optarg;
		break;
#ifdef CVS_DIFFDATE
	    case 'D':
		if (diff_rev2 != NULL || diff_date2 != NULL)
		    error (1, 0,
		       "no more than two revisions/dates can be specified");
		if (diff_rev1 != NULL || diff_date1 != NULL)
		    diff_date2 = Make_Date (optarg);
		else
		    diff_date1 = Make_Date (optarg);
		break;
#endif
	    case '?':
	    default:
		usage (diff_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    /* make sure options is non-null */
    if (!options)
	options = xstrdup ("");

    /* start the recursion processor */
    err = start_recursion (diff_fileproc, (int (*) ()) NULL, diff_dirproc,
			   diff_dirleaveproc, argc, argv, local,
			   W_LOCAL, 0, 1, (char *) NULL, 1);

    /* clean up */
    free (options);
    return (err);
}

/*
 * Do a file diff
 */
/* ARGSUSED */
static int
diff_fileproc (file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List *entries;
    List *srcfiles;
{
    int status, err = 2;		/* 2 == trouble, like rcsdiff */
    Vers_TS *vers;

    vers = Version_TS (repository, (char *) NULL, (char *) NULL, (char *) NULL,
		       file, 1, 0, entries, srcfiles);

    if (vers->vn_user == NULL)
    {
	error (0, 0, "I know nothing about %s", file);
	freevers_ts (&vers);
	diff_mark_errors (err);
	return (err);
    }
    else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
    {
	error (0, 0, "%s is a new entry, no comparison available", file);
	freevers_ts (&vers);
	diff_mark_errors (err);
	return (err);
    }
    else if (vers->vn_user[0] == '-')
    {
	error (0, 0, "%s was removed, no comparison available", file);
	freevers_ts (&vers);
	diff_mark_errors (err);
	return (err);
    }
    else
    {
	if (vers->vn_rcs == NULL && vers->srcfile == NULL)
	{
	    error (0, 0, "cannot find revision control file for %s", file);
	    freevers_ts (&vers);
	    diff_mark_errors (err);
	    return (err);
	}
	else
	{
	    if (vers->ts_user == NULL)
	    {
		error (0, 0, "cannot find %s", file);
		freevers_ts (&vers);
		diff_mark_errors (err);
		return (err);
	    }
	}
    }

    if (diff_file_nodiff (file, repository, entries, srcfiles, vers))
    {
	freevers_ts (&vers);
	return (0);
    }

    (void) fflush (stdout);
    if (use_rev2)
    {
	run_setup ("%s%s %s %s -r%s -r%s", Rcsbin, RCS_DIFF,
		   opts, *options ? options : vers->options,
		   use_rev1, use_rev2);
    }
    else
    {
	run_setup ("%s%s %s %s -r%s", Rcsbin, RCS_DIFF, opts,
		   *options ? options : vers->options, use_rev1);
    }
    run_arg (vers->srcfile->path);

    switch ((status = run_exec (RUN_TTY, RUN_TTY, RUN_TTY,
	RUN_REALLY|RUN_COMBINED)))
    {
	case -1:			/* fork failed */
	    error (1, errno, "fork failed during rcsdiff of %s",
		   vers->srcfile->path);
	case 0:				/* everything ok */
	    err = 0;
	    break;
	default:			/* other error */
	    err = status;
	    break;
    }

    (void) fflush (stdout);
    freevers_ts (&vers);
    diff_mark_errors (err);
    return (err);
}

/*
 * Remember the exit status for each file.
 */
static void
diff_mark_errors (err)
    int err;
{
    if (err > diff_errors)
	diff_errors = err;
}

/*
 * Print a warm fuzzy message when we enter a dir
 */
/* ARGSUSED */
static Dtype
diff_dirproc (dir, pos_repos, update_dir)
    char *dir;
    char *pos_repos;
    char *update_dir;
{
    /* XXX - check for dirs we don't want to process??? */
    if (!quiet)
	error (0, 0, "Diffing %s", update_dir);
    return (R_PROCESS);
}

/*
 * Concoct the proper exit status.
 */
/* ARGSUSED */
static int
diff_dirleaveproc (dir, err, update_dir)
    char *dir;
    int err;
    char *update_dir;
{
    return (diff_errors);
}

/*
 * verify that a file is different 0=same 1=different
 */
static int
diff_file_nodiff (file, repository, entries, srcfiles, vers)
    char *file;
    char *repository;
    List *entries;
    List *srcfiles;
    Vers_TS *vers;
{
    Vers_TS *xvers;
    char tmp[L_tmpnam+1];

    /* free up any old use_rev* variables and reset 'em */
    if (use_rev1)
	free (use_rev1);
    if (use_rev2)
	free (use_rev2);
    use_rev1 = use_rev2 = (char *) NULL;

    if (diff_rev1 || diff_date1)
    {
	/* special handling for TAG_HEAD */
	if (diff_rev1 && strcmp (diff_rev1, TAG_HEAD) == 0)
	    use_rev1 = xstrdup (vers->vn_rcs);
	else
	{
	    xvers = Version_TS (repository, (char *) NULL, diff_rev1,
				diff_date1, file, 1, 0, entries, srcfiles);
	    if (xvers->vn_rcs == NULL)
	    {
		if (diff_rev1)
		    error (0, 0, "tag %s is not in file %s", diff_rev1, file);
		else
		    error (0, 0, "no revision for date %s in file %s",
			   diff_date1, file);
		return (1);
	    }
	    use_rev1 = xstrdup (xvers->vn_rcs);
	    freevers_ts (&xvers);
	}
    }
    if (diff_rev2 || diff_date2)
    {
	/* special handling for TAG_HEAD */
	if (diff_rev2 && strcmp (diff_rev2, TAG_HEAD) == 0)
	    use_rev2 = xstrdup (vers->vn_rcs);
	else
	{
	    xvers = Version_TS (repository, (char *) NULL, diff_rev2,
				diff_date2, file, 1, 0, entries, srcfiles);
	    if (xvers->vn_rcs == NULL)
	    {
		if (diff_rev1)
		    error (0, 0, "tag %s is not in file %s", diff_rev2, file);
		else
		    error (0, 0, "no revision for date %s in file %s",
			   diff_date2, file);
		return (1);
	    }
	    use_rev2 = xstrdup (xvers->vn_rcs);
	    freevers_ts (&xvers);
	}

	/* now, see if we really need to do the diff */
	return (strcmp (use_rev1, use_rev2) == 0);
    }
    if (use_rev1 == NULL || strcmp (use_rev1, vers->vn_user) == 0)
    {
	if (strcmp (vers->ts_rcs, vers->ts_user) == 0 &&
	    (!(*options) || strcmp (options, vers->options) == 0))
	{
	    return (1);
	}
	if (use_rev1 == NULL)
	    use_rev1 = xstrdup (vers->vn_user);
    }

    /*
     * with 0 or 1 -r option specified, run a quick diff to see if we
     * should bother with it at all.
     */
    run_setup ("%s%s -p -q %s -r%s", Rcsbin, RCS_CO,
	       *options ? options : vers->options, use_rev1);
    run_arg (vers->srcfile->path);
    switch (run_exec (RUN_TTY, tmpnam (tmp), RUN_TTY, RUN_REALLY))
    {
	case 0:				/* everything ok */
	    if (xcmp (file, tmp) == 0)
	    {
		(void) unlink (tmp);
		return (1);
	    }
	    break;
	case -1:			/* fork failed */
	    (void) unlink (tmp);
	    error (1, errno, "fork failed during checkout of %s",
		   vers->srcfile->path);
	default:
	    break;
    }
    (void) unlink (tmp);
    return (0);
}
