/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Patch
 * 
 * Create a Larry Wall format "patch" file between a previous release and the
 * current head of a module, or between two releases.  Can specify the
 * release as either a date or a revision number.
 */

#include "cvs.h"
#include "getline.h"

static RETSIGTYPE patch_cleanup PROTO((void));
static Dtype patch_dirproc PROTO ((void *callerdat, char *dir,
				   char *repos, char *update_dir,
				   List *entries));
static int patch_fileproc PROTO ((void *callerdat, struct file_info *finfo));
static int patch_proc PROTO((int *pargc, char **argv, char *xwhere,
		       char *mwhere, char *mfile, int shorten,
		       int local_specified, char *mname, char *msg));

static int force_tag_match = 1;
static int patch_short = 0;
static int toptwo_diffs = 0;
static int local = 0;
static char *options = NULL;
static char *rev1 = NULL;
static int rev1_validated = 0;
static char *rev2 = NULL;
static int rev2_validated = 0;
static char *date1 = NULL;
static char *date2 = NULL;
static char *tmpfile1 = NULL;
static char *tmpfile2 = NULL;
static char *tmpfile3 = NULL;
static int unidiff = 0;

static const char *const patch_usage[] =
{
    "Usage: %s %s [-flR] [-c|-u] [-s|-t] [-V %%d]\n",
    "    -r rev|-D date [-r rev2 | -D date2] modules...\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, not recursive\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-c\tContext diffs (default)\n",
    "\t-u\tUnidiff format.\n",
    "\t-s\tShort patch - one liner per file.\n",
    "\t-t\tTop two diffs - last change made to the file.\n",
    "\t-D date\tDate.\n",
    "\t-r rev\tRevision - symbolic or numeric.\n",
    "\t-V vers\tUse RCS Version \"vers\" for keyword expansion.\n",
    NULL
};

int
patch (argc, argv)
    int argc;
    char **argv;
{
    register int i;
    int c;
    int err = 0;
    DBM *db;

    if (argc == -1)
	usage (patch_usage);

    optind = 1;
    while ((c = getopt (argc, argv, "+V:k:cuftsQqlRD:r:")) != -1)
    {
	switch (c)
	{
	    case 'Q':
	    case 'q':
#ifdef SERVER_SUPPORT
		/* The CVS 1.5 client sends these options (in addition to
		   Global_option requests), so we must ignore them.  */
		if (!server_active)
#endif
		    error (1, 0,
			   "-q or -Q must be specified before \"%s\"",
			   command_name);
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
		/* This option is pretty seriously broken:
		   1.  It is not clear what it does (does it change keyword
		   expansion behavior?  If so, how?  Or does it have
		   something to do with what version of RCS we are using?
		   Or the format we write RCS files in?).
		   2.  Because both it and -k use the options variable,
		   specifying both -V and -k doesn't work.
		   3.  At least as of CVS 1.9, it doesn't work (failed
		   assertion in RCS_checkout where it asserts that options
		   starts with -k).  Few people seem to be complaining.
		   In the future (perhaps the near future), I have in mind
		   removing it entirely, and updating NEWS and cvs.texinfo,
		   but in case it is a good idea to give people more time
		   to complain if they would miss it, I'll just add this
		   quick and dirty error message for now.  */
		error (1, 0,
		       "the -V option is obsolete and should not be used");
#if 0
		if (atoi (optarg) <= 0)
		    error (1, 0, "must specify a version number to -V");
		if (options)
		    free (options);
		options = xmalloc (strlen (optarg) + 1 + 2);	/* for the -V */
		(void) sprintf (options, "-V%s", optarg);
#endif
		break;
	    case 'u':
		unidiff = 1;		/* Unidiff */
		break;
	    case 'c':			/* Context diff */
		unidiff = 0;
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

#ifdef CLIENT_SUPPORT
    if (client_active)
    {
	/* We're the client side.  Fire up the remote server.  */
	start_server ();
	
	ign_setup ();

	if (local)
	    send_arg("-l");
	if (!force_tag_match)
	    send_arg("-f");
	if (toptwo_diffs)
	    send_arg("-t");
	if (patch_short)
	    send_arg("-s");
	if (unidiff)
	    send_arg("-u");

	if (rev1)
	    option_with_arg ("-r", rev1);
	if (date1)
	    client_senddate (date1);
	if (rev2)
	    option_with_arg ("-r", rev2);
	if (date2)
	    client_senddate (date2);
	if (options[0] != '\0')
	    send_arg (options);

	{
	    int i;
	    for (i = 0; i < argc; ++i)
		send_arg (argv[i]);
	}

	send_to_server ("rdiff\012", 0);
        return get_responses_and_close ();
    }
#endif

    /* clean up if we get a signal */
#ifdef SIGHUP
    (void) SIG_register (SIGHUP, patch_cleanup);
#endif
#ifdef SIGINT
    (void) SIG_register (SIGINT, patch_cleanup);
#endif
#ifdef SIGQUIT
    (void) SIG_register (SIGQUIT, patch_cleanup);
#endif
#ifdef SIGPIPE
    (void) SIG_register (SIGPIPE, patch_cleanup);
#endif
#ifdef SIGTERM
    (void) SIG_register (SIGTERM, patch_cleanup);
#endif

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
static int
patch_proc (pargc, argv, xwhere, mwhere, mfile, shorten, local_specified,
	    mname, msg)
    int *pargc;
    char **argv;
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
    char *repository;
    char *where;

    repository = xmalloc (strlen (CVSroot_directory) + strlen (argv[0])
			  + (mfile == NULL ? 0 : strlen (mfile)) + 30);
    (void) sprintf (repository, "%s/%s", CVSroot_directory, argv[0]);
    where = xmalloc (strlen (argv[0]) + (mfile == NULL ? 0 : strlen (mfile))
		     + 10);
    (void) strcpy (where, argv[0]);

    /* if mfile isn't null, we need to set up to do only part of the module */
    if (mfile != NULL)
    {
	char *cp;
	char *path;

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
	path = xmalloc (strlen (repository) + strlen (mfile) + 5);
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
	free (path);
    }

    /* cd to the starting repository */
    if ( CVS_CHDIR (repository) < 0)
    {
	error (0, errno, "cannot chdir to %s", repository);
	free (repository);
	return (1);
    }
    free (repository);

    if (force_tag_match)
	which = W_REPOS | W_ATTIC;
    else
	which = W_REPOS;

    if (rev1 != NULL && !rev1_validated)
    {
	tag_check_valid (rev1, *pargc - 1, argv + 1, local, 0, NULL);
	rev1_validated = 1;
    }
    if (rev2 != NULL && !rev2_validated)
    {
	tag_check_valid (rev2, *pargc - 1, argv + 1, local, 0, NULL);
	rev2_validated = 1;
    }

    /* start the recursion processor */
    err = start_recursion (patch_fileproc, (FILESDONEPROC) NULL, patch_dirproc,
			   (DIRLEAVEPROC) NULL, NULL,
			   *pargc - 1, argv + 1, local,
			   which, 0, 1, where, 1);
    free (where);

    return (err);
}

/*
 * Called to examine a particular RCS file, as appropriate with the options
 * that were set above.
 */
/* ARGSUSED */
static int
patch_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    struct utimbuf t;
    char *vers_tag, *vers_head;
    char *rcs = NULL;
    RCSNode *rcsfile;
    FILE *fp1, *fp2, *fp3;
    int ret = 0;
    int isattic = 0;
    int retcode = 0;
    char *file1;
    char *file2;
    char *strippath;
    char *line1, *line2;
    size_t line1_chars_allocated;
    size_t line2_chars_allocated;
    char *cp1, *cp2;
    FILE *fp;

    line1 = NULL;
    line1_chars_allocated = 0;
    line2 = NULL;
    line2_chars_allocated = 0;

    /* find the parsed rcs file */
    if ((rcsfile = finfo->rcs) == NULL)
    {
	ret = 1;
	goto out2;
    }
    if ((rcsfile->flags & VALID) && (rcsfile->flags & INATTIC))
	isattic = 1;

    rcs = xmalloc (strlen (finfo->file) + sizeof (RCSEXT) + 5);
    (void) sprintf (rcs, "%s%s", finfo->file, RCSEXT);

    /* if vers_head is NULL, may have been removed from the release */
    if (isattic && rev2 == NULL && date2 == NULL)
	vers_head = NULL;
    else
    {
	vers_head = RCS_getversion (rcsfile, rev2, date2, force_tag_match,
				    (int *) NULL);
	if (vers_head != NULL && RCS_isdead (rcsfile, vers_head))
	{
	    free (vers_head);
	    vers_head = NULL;
	}
    }

    if (toptwo_diffs)
    {
	if (vers_head == NULL)
	{
	    ret = 1;
	    goto out2;
	}

	if (!date1)
	    date1 = xmalloc (MAXDATELEN);
	*date1 = '\0';
	if (RCS_getrevtime (rcsfile, vers_head, date1, 1) == -1)
	{
	    if (!really_quiet)
		error (0, 0, "cannot find date in rcs file %s revision %s",
		       rcs, vers_head);
	    ret = 1;
	    goto out2;
	}
    }
    vers_tag = RCS_getversion (rcsfile, rev1, date1, force_tag_match,
			       (int *) NULL);
    if (vers_tag != NULL && RCS_isdead (rcsfile, vers_tag))
    {
        free (vers_tag);
	vers_tag = NULL;
    }

    if (vers_tag == NULL && vers_head == NULL)
    {
	/* Nothing known about specified revs.  */
	ret = 0;
	goto out2;
    }

    if (vers_tag && vers_head && strcmp (vers_head, vers_tag) == 0)
    {
	/* Not changed between releases.  */
	ret = 0;
	goto out2;
    }

    if (patch_short)
    {
	(void) printf ("File %s ", finfo->fullname);
	if (vers_tag == NULL)
	    (void) printf ("is new; current revision %s\n", vers_head);
	else if (vers_head == NULL)
	{
	    (void) printf ("is removed; not included in ");
	    if (rev2 != NULL)
		(void) printf ("release tag %s", rev2);
	    else if (date2 != NULL)
		(void) printf ("release date %s", date2);
	    else
		(void) printf ("current release");
	    (void) printf ("\n");
	}
	else
	    (void) printf ("changed from revision %s to %s\n",
			   vers_tag, vers_head);
	ret = 0;
	goto out2;
    }
    tmpfile1 = cvs_temp_name ();
    if ((fp1 = CVS_FOPEN (tmpfile1, "w+")) != NULL)
	(void) fclose (fp1);
    tmpfile2 = cvs_temp_name ();
    if ((fp2 = CVS_FOPEN (tmpfile2, "w+")) != NULL)
	(void) fclose (fp2);
    tmpfile3 = cvs_temp_name ();
    if ((fp3 = CVS_FOPEN (tmpfile3, "w+")) != NULL)
	(void) fclose (fp3);
    if (fp1 == NULL || fp2 == NULL || fp3 == NULL)
    {
	/* FIXME: should be printing a proper error message, with errno-based
	   message, and the filename which we could not create.  */
	error (0, 0, "cannot create temporary files");
	ret = 1;
	goto out;
    }
    if (vers_tag != NULL)
    {
	retcode = RCS_checkout (rcsfile, (char *) NULL, vers_tag,
				rev1, options, tmpfile1,
				(RCSCHECKOUTPROC) NULL, (void *) NULL);
	if (retcode != 0)
	{
	    if (!really_quiet)
		error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
		       "co of revision %s in %s failed", vers_tag, rcs);
	    ret = 1;
	    goto out;
	}
	memset ((char *) &t, 0, sizeof (t));
	if ((t.actime = t.modtime = RCS_getrevtime (rcsfile, vers_tag,
						    (char *) 0, 0)) != -1)
		(void) utime (tmpfile1, &t);
    }
    else if (toptwo_diffs)
    {
	ret = 1;
	goto out;
    }
    if (vers_head != NULL)
    {
	retcode = RCS_checkout (rcsfile, (char *) NULL, vers_head,
				rev2, options, tmpfile2,
				(RCSCHECKOUTPROC) NULL, (void *) NULL);
	if (retcode != 0)
	{
	    if (!really_quiet)
		error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
		       "co of revision %s in %s failed", vers_head, rcs);
	    ret = 1;
	    goto out;
	}
	if ((t.actime = t.modtime = RCS_getrevtime (rcsfile, vers_head,
						    (char *) 0, 0)) != -1)
		(void) utime (tmpfile2, &t);
    }
    run_setup ("%s -%c", DIFF, unidiff ? 'u' : 'c');
    run_arg (tmpfile1);
    run_arg (tmpfile2);

    switch (run_exec (RUN_TTY, tmpfile3, RUN_TTY, RUN_REALLY))
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

	    /* Output an "Index:" line for patch to use */
	    (void) fflush (stdout);
	    (void) printf ("Index: %s\n", finfo->fullname);
	    (void) fflush (stdout);

	    fp = open_file (tmpfile3, "r");
	    if (getline (&line1, &line1_chars_allocated, fp) < 0 ||
		getline (&line2, &line2_chars_allocated, fp) < 0)
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
		    (cp1 = strchr (line1, '\t')) == NULL ||
		    (cp2 = strchr (line2, '\t')) == NULL)
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
		    (cp1 = strchr (line1, '\t')) == NULL ||
		    (cp2 = strchr  (line2, '\t')) == NULL)
		{
		    error (0, 0, "invalid unidiff header for %s", rcs);
		    ret = 1;
		    (void) fclose (fp);
		    goto out;
		}
	    }
	    if (CVSroot_directory != NULL)
	    {
		strippath = xmalloc (strlen (CVSroot_directory) + 10);
		(void) sprintf (strippath, "%s/", CVSroot_directory);
	    }
	    else
		strippath = xstrdup (REPOS_STRIP);
	    if (strncmp (rcs, strippath, strlen (strippath)) == 0)
		rcs += strlen (strippath);
	    free (strippath);
	    if (vers_tag != NULL)
	    {
		file1 = xmalloc (strlen (finfo->fullname)
				 + strlen (vers_tag)
				 + 10);
		(void) sprintf (file1, "%s:%s", finfo->fullname, vers_tag);
	    }
	    else
	    {
		file1 = xstrdup (DEVNULL);
	    }
	    file2 = xmalloc (strlen (finfo->fullname)
			     + (vers_head != NULL ? strlen (vers_head) : 10)
			     + 10);
	    (void) sprintf (file2, "%s:%s", finfo->fullname,
			    vers_head ? vers_head : "removed");

	    /* Note that this prints "diff" not DIFF.  The format of a diff
	       does not depend on the name of the program which happens to
	       have produced it.  */
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

	    (void) printf ("%s%s", finfo->fullname, cp2);
	    /* spew the rest of the diff out */
	    while (getline (&line1, &line1_chars_allocated, fp) >= 0)
		(void) fputs (line1, stdout);
	    (void) fclose (fp);
	    free (file1);
	    free (file2);
	    break;
	default:
	    error (0, 0, "diff failed for %s", finfo->fullname);
    }
  out:
    if (line1)
        free (line1);
    if (line2)
        free (line2);
    /* FIXME: should be checking for errors.  */
    (void) CVS_UNLINK (tmpfile1);
    (void) CVS_UNLINK (tmpfile2);
    (void) CVS_UNLINK (tmpfile3);
    free (tmpfile1);
    free (tmpfile2);
    free (tmpfile3);
    tmpfile1 = tmpfile2 = tmpfile3 = NULL;

 out2:
    if (rcs != NULL)
	free (rcs);
    return (ret);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
patch_dirproc (callerdat, dir, repos, update_dir, entries)
    void *callerdat;
    char *dir;
    char *repos;
    char *update_dir;
    List *entries;
{
    if (!quiet)
	error (0, 0, "Diffing %s", update_dir);
    return (R_PROCESS);
}

/*
 * Clean up temporary files
 */
static RETSIGTYPE
patch_cleanup ()
{
    if (tmpfile1 != NULL)
    {
	(void) unlink_file (tmpfile1);
	free (tmpfile1);
    }
    if (tmpfile2 != NULL)
    {
	(void) unlink_file (tmpfile2);
	free (tmpfile2);
    }
    if (tmpfile3 != NULL)
    {
	(void) unlink_file (tmpfile3);
	free (tmpfile3);
    }
    tmpfile1 = tmpfile2 = tmpfile3 = NULL;
}
