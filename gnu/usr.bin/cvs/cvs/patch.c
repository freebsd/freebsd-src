/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * Patch
 * 
 * Create a Larry Wall format "patch" file between a previous release and the
 * current head of a module, or between two releases.  Can specify the
 * release as either a date or a revision number.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)patch.c 1.50 92/04/10";

#endif

#if __STDC__
static SIGTYPE patch_cleanup (void);
static Dtype patch_dirproc (char *dir, char *repos, char *update_dir);
static int patch_fileproc (char *file, char *update_dir, char *repository,
			   List * entries, List * srcfiles);
static int patch_proc (int *pargc, char *argv[], char *xwhere,
		       char *mwhere, char *mfile, int shorten,
		       int local_specified, char *mname, char *msg);
#else
static int patch_proc ();
static int patch_fileproc ();
static Dtype patch_dirproc ();
static SIGTYPE patch_cleanup ();
#endif				/* __STDC__ */

static int force_tag_match = 1;
static int patch_short = 0;
static int toptwo_diffs = 0;
static int local = 0;
static char *options = NULL;
static char *rev1 = NULL;
static char *rev2 = NULL;
static char *date1 = NULL;
static char *date2 = NULL;
static char *K_flag1 = NULL;
static char *K_flag2 = NULL;
static char tmpfile1[L_tmpnam+1], tmpfile2[L_tmpnam+1], tmpfile3[L_tmpnam+1];
static int unidiff = 0;

static char *patch_usage[] =
{
    "Usage: %s %s [-Qflq] [-c|-u] [-s|-t] [-V %%d]\n",
    "    -r rev|-D date [-r rev2 | -D date2] modules...\n",
    "\t-Q\tReally quiet.\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, not recursive\n",
    "\t-c\tContext diffs (default)\n",
    "\t-u\tUnidiff format.\n",
    "\t-s\tShort patch - one liner per file.\n",
    "\t-t\tTop two diffs - last change made to the file.\n",
    "\t-D date\tDate.\n",
    "\t-r rev\tRevision - symbolic or numeric.\n",
    "\t-V vers\tUse RCS Version \"vers\" for keyword expansion.\n",
    "\t-K key\tUse RCS key -K option on checkout.\n",
    NULL
};

int
patch (argc, argv)
    int argc;
    char *argv[];
{
    register int i;
    int c;
    int err = 0;
    DBM *db;

    if (argc == -1)
	usage (patch_usage);

    optind = 1;
    while ((c = gnu_getopt (argc, argv, "V:k:cuftsQqlRD:r:K:")) != -1)
    {
	switch (c)
	{
	    case 'Q':
		really_quiet = 1;
		/* FALL THROUGH */
	    case 'q':
		quiet = 1;
		break;
	    case 'f':
		force_tag_match = 0;
		break;
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 't':
		toptwo_diffs = 1;
		break;
	    case 's':
		patch_short = 1;
		break;
	    case 'D':
		if (rev2 != NULL || date2 != NULL)
		    error (1, 0,
		       "no more than two revisions/dates can be specified");
		if (rev1 != NULL || date1 != NULL)
		    date2 = Make_Date (optarg);
		else
		    date1 = Make_Date (optarg);
		break;
	    case 'r':
		if (rev2 != NULL || date2 != NULL)
		    error (1, 0,
		       "no more than two revisions/dates can be specified");
		if (rev1 != NULL || date1 != NULL)
		    rev2 = optarg;
		else
		    rev1 = optarg;
		break;
	    case 'k':
		if (options)
		    free (options);
		options = RCS_check_kflag (optarg);
		break;
	    case 'V':
		if (atoi (optarg) <= 0)
		    error (1, 0, "must specify a version number to -V");
		if (options)
		    free (options);
		options = xmalloc (strlen (optarg) + 1 + 2);	/* for the -V */
		(void) sprintf (options, "-V%s", optarg);
		break;
	    case 'u':
		unidiff = 1;		/* Unidiff */
		break;
	    case 'c':			/* Context diff */
		unidiff = 0;
		break;
	    case 'K':
		if (K_flag2 != NULL)
		    error (1, 0, "no more than two -K flags can be specified");
		if (K_flag1 != NULL)
		    K_flag2 = optarg;
		else
		    K_flag1 = optarg;
		break;
	    case '?':
	    default:
		usage (patch_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    /* Sanity checks */
    /* Check for dummy -K flags */
    if (K_flag1 && K_flag1[0] != 'e' && K_flag1[0] != 'i')
	error (1, 0, "-K flag does not start e or i");
    if (K_flag2 && K_flag2[0] != 'e' && K_flag2[0] != 'i')
	error (1, 0, "-K flag does not start e or i");
    if (argc < 1)
	usage (patch_usage);

    if (toptwo_diffs && patch_short)
	error (1, 0, "-t and -s options are mutually exclusive");
    if (toptwo_diffs && (date1 != NULL || date2 != NULL ||
			 rev1 != NULL || rev2 != NULL))
	error (1, 0, "must not specify revisions/dates with -t option!");

    if (!toptwo_diffs && (date1 == NULL && date2 == NULL &&
			  rev1 == NULL && rev2 == NULL))
	error (1, 0, "must specify at least one revision/date!");
    if (date1 != NULL && date2 != NULL)
	if (RCS_datecmp (date1, date2) >= 0)
	    error (1, 0, "second date must come after first date!");

    /* if options is NULL, make it a NULL string */
    if (options == NULL)
	options = xstrdup ("");

    /* clean up if we get a signal */
    (void) SIG_register (SIGHUP, patch_cleanup);
    (void) SIG_register (SIGINT, patch_cleanup);
    (void) SIG_register (SIGQUIT, patch_cleanup);
    (void) SIG_register (SIGPIPE, patch_cleanup);
    (void) SIG_register (SIGTERM, patch_cleanup);

    db = open_module ();
    for (i = 0; i < argc; i++)
	err += do_module (db, argv[i], PATCH, "Patching", patch_proc,
			  (char *) NULL, 0, 0, 0, (char *) NULL);
    close_module (db);
    free (options);
    patch_cleanup ();
    return (err);
}

/*
 * callback proc for doing the real work of patching
 */
/* ARGSUSED */
static char where[PATH_MAX];
static int
patch_proc (pargc, argv, xwhere, mwhere, mfile, shorten, local_specified,
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

    (void) sprintf (repository, "%s/%s", CVSroot, argv[0]);
    (void) strcpy (where, argv[0]);

    /* if mfile isn't null, we need to set up to do only part of the module */
    if (mfile != NULL)
    {
	char *cp;
	char path[PATH_MAX];

	/* if the portion of the module is a path, put the dir part on repos */
	if ((cp = rindex (mfile, '/')) != NULL)
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

    /* cd to the starting repository */
    if (chdir (repository) < 0)
    {
	error (0, errno, "cannot chdir to %s", repository);
	return (1);
    }

    if (force_tag_match)
	which = W_REPOS | W_ATTIC;
    else
	which = W_REPOS;

    /* start the recursion processor */
    err = start_recursion (patch_fileproc, (int (*) ()) NULL, patch_dirproc,
			   (int (*) ()) NULL, *pargc - 1, argv + 1, local,
			   which, 0, 1, where, 1);

    return (err);
}

/*
 * Called to examine a particular RCS file, as appropriate with the options
 * that were set above.
 */
/* ARGSUSED */
static int
patch_fileproc (file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List *entries;
    List *srcfiles;
{
    char *vers_tag, *vers_head;
    char rcsspace[PATH_MAX];
    char *rcs = rcsspace;
    Node *p;
    RCSNode *rcsfile;
    FILE *fp1, *fp2, *fp3;
    int ret = 0;
    int isattic = 0;
    int retcode = 0;
    char file1[PATH_MAX], file2[PATH_MAX], strippath[PATH_MAX];
    char line1[MAXLINELEN], line2[MAXLINELEN];
    char *cp1, *cp2, *commap;
    FILE *fp;


    /* find the parsed rcs file */
    p = findnode (srcfiles, file);
    if (p == NULL)
	return (1);
    rcsfile = (RCSNode *) p->data;
    if ((rcsfile->flags & VALID) && (rcsfile->flags & INATTIC))
	isattic = 1;

    (void) sprintf (rcs, "%s%s", file, RCSEXT);

    /* if vers_head is NULL, may have been removed from the release */
    if (isattic && rev2 == NULL && date2 == NULL)
	vers_head = NULL;
    else
	vers_head = RCS_getversion (rcsfile, rev2, date2, force_tag_match);

    if (toptwo_diffs)
    {
	if (vers_head == NULL)
	    return (1);

	if (!date1)
	    date1 = xmalloc (50);	/* plenty big :-) */
	*date1 = '\0';
	if (RCS_getrevtime (rcsfile, vers_head, date1, 1) == -1)
	{
	    if (!really_quiet)
		error (0, 0, "cannot find date in rcs file %s revision %s",
		       rcs, vers_head);
	    return (1);
	}
    }
    vers_tag = RCS_getversion (rcsfile, rev1, date1, force_tag_match);

    if (vers_tag == NULL && (vers_head == NULL || isattic))
	return (0);			/* nothing known about specified revs */

    if (vers_tag && vers_head && strcmp (vers_head, vers_tag) == 0)
	return (0);			/* not changed between releases */

    if (patch_short)
    {
	(void) printf ("File ");
	if (vers_tag == NULL)
	    (void) printf ("%s is new; current revision %s\n", rcs, vers_head);
	else if (vers_head == NULL)
	    (void) printf ("%s is removed; not included in release %s\n",
			   rcs, rev2 ? rev2 : date2);
	else
	    (void) printf ("%s changed from revision %s to %s\n",
			   rcs, vers_tag, vers_head);
	return (0);
    }
    if ((fp1 = fopen (tmpnam (tmpfile1), "w+")) != NULL)
	(void) fclose (fp1);
    if ((fp2 = fopen (tmpnam (tmpfile2), "w+")) != NULL)
	(void) fclose (fp2);
    if ((fp3 = fopen (tmpnam (tmpfile3), "w+")) != NULL)
	(void) fclose (fp3);
    if (fp1 == NULL || fp2 == NULL || fp3 == NULL)
    {
	error (0, 0, "cannot create temporary files");
	ret = 1;
	goto out;
    }
    if (vers_tag != NULL)
    {
	run_setup ("%s%s %s -p -q -r%s %s%s", Rcsbin, RCS_CO, options,
		   vers_tag, K_flag1 ? "-K" : "", K_flag1 ? K_flag1 : "");
	run_arg (rcsfile->path);
	if ((retcode = run_exec (RUN_TTY, tmpfile1, RUN_TTY, RUN_NORMAL)) != 0)
	{
	    if (!really_quiet)
		error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
		       "co of revision %s in %s failed", vers_tag, rcs);
	    ret = 1;
	    goto out;
	}
    }
    else if (toptwo_diffs)
    {
	ret = 1;
	goto out;
    }
    if (vers_head != NULL)
    {
	run_setup ("%s%s %s -p -q -r%s %s%s", Rcsbin, RCS_CO, options,
		   vers_head, K_flag2 ? "-K" : "", K_flag2 ? K_flag2 : "");
	run_arg (rcsfile->path);
	if ((retcode = run_exec (RUN_TTY, tmpfile2, RUN_TTY, RUN_NORMAL)) != 0)
	{
	    if (!really_quiet)
		error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
		       "co of revision %s in %s failed", vers_head, rcs);
	    ret = 1;
	    goto out;
	}
    }
    run_setup ("%s -%c", DIFF, unidiff ? 'u' : 'c');
    run_arg (tmpfile1);
    run_arg (tmpfile2);
    switch (run_exec (RUN_TTY, tmpfile3, RUN_TTY, RUN_NORMAL))
    {
	case -1:			/* fork/wait failure */
	    error (1, errno, "fork for diff failed on %s", rcs);
	    break;
	case 0:				/* nothing to do */
	    break;
	case 1:
	    /*
	     * The two revisions are really different, so read the first two
	     * lines of the diff output file, and munge them to include more
	     * reasonable file names that "patch" will understand.
	     */
	    fp = open_file (tmpfile3, "r");
	    if (fgets (line1, sizeof (line1), fp) == NULL ||
		fgets (line2, sizeof (line2), fp) == NULL)
	    {
		error (0, errno, "failed to read diff file header %s for %s",
		       tmpfile3, rcs);
		ret = 1;
		(void) fclose (fp);
		goto out;
	    }
	    if (!unidiff)
	    {
		if (strncmp (line1, "*** ", 4) != 0 ||
		    strncmp (line2, "--- ", 4) != 0 ||
		    (cp1 = index (line1, '\t')) == NULL ||
		    (cp2 = index (line2, '\t')) == NULL)
		{
		    error (0, 0, "invalid diff header for %s", rcs);
		    ret = 1;
		    (void) fclose (fp);
		    goto out;
		}
	    }
	    else
	    {
		if (strncmp (line1, "--- ", 4) != 0 ||
		    strncmp (line2, "+++ ", 4) != 0 ||
		    (cp1 = index (line1, '\t')) == NULL ||
		    (cp2 = index (line2, '\t')) == NULL)
		{
		    error (0, 0, "invalid unidiff header for %s", rcs);
		    ret = 1;
		    (void) fclose (fp);
		    goto out;
		}
	    }
	    if (CVSroot != NULL)
		(void) sprintf (strippath, "%s/", CVSroot);
	    else
		(void) strcpy (strippath, REPOS_STRIP);
	    if (strncmp (rcs, strippath, strlen (strippath)) == 0)
		rcs += strlen (strippath);
	    commap = rindex (rcs, ',');
	    *commap = '\0';
	    if (vers_tag != NULL)
	    {
		(void) sprintf (file1, "%s%s%s:%s", update_dir,
				update_dir[0] ? "/" : "", rcs, vers_tag);
	    }
	    else
	    {
		(void) strcpy (file1, DEVNULL);
	    }
	    (void) sprintf (file2, "%s%s%s:%s", update_dir,
			    update_dir[0] ? "/" : "", rcs,
			    vers_head ? vers_head : "removed");
	    if (unidiff)
	    {
		(void) printf ("diff -u %s %s\n", file1, file2);
		(void) printf ("--- %s%s+++ ", file1, cp1);
	    }
	    else
	    {
		(void) printf ("diff -c %s %s\n", file1, file2);
		(void) printf ("*** %s%s--- ", file1, cp1);
	    }

	    if (update_dir[0] != '\0')
		(void) printf ("%s/", update_dir);
	    (void) printf ("%s%s", rcs, cp2);
	    while (fgets (line1, sizeof (line1), fp) != NULL)
		(void) printf ("%s", line1);
	    (void) fclose (fp);
	    break;
	default:
	    error (0, 0, "diff failed for %s", rcs);
    }
  out:
    (void) unlink_file (tmpfile1);
    (void) unlink_file (tmpfile2);
    (void) unlink_file (tmpfile3);
    return (ret);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
patch_dirproc (dir, repos, update_dir)
    char *dir;
    char *repos;
    char *update_dir;
{
    if (!quiet)
	error (0, 0, "Diffing %s", update_dir);
    return (R_PROCESS);
}

/*
 * Clean up temporary files
 */
static SIGTYPE
patch_cleanup ()
{
    if (tmpfile1[0] != '\0')
	(void) unlink_file (tmpfile1);
    if (tmpfile2[0] != '\0')
	(void) unlink_file (tmpfile2);
    if (tmpfile3[0] != '\0')
	(void) unlink_file (tmpfile3);
}
