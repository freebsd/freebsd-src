/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
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
static const char rcsid[] = "$CVSid: @(#)diff.c 1.61 94/10/22 $";
USE(rcsid);
#endif

static Dtype diff_dirproc PROTO((char *dir, char *pos_repos, char *update_dir));
static int diff_filesdoneproc PROTO((int err, char *repos, char *update_dir));
static int diff_dirleaveproc PROTO((char *dir, int err, char *update_dir));
static int diff_file_nodiff PROTO((char *file, char *repository, List *entries,
			     List *srcfiles, Vers_TS *vers));
static int diff_fileproc PROTO((char *file, char *update_dir, char *repository,
			  List * entries, List * srcfiles));
static void diff_mark_errors PROTO((int err));

static char *diff_rev1, *diff_rev2;
static char *diff_date1, *diff_date2;
static char *use_rev1, *use_rev2;

#ifdef SERVER_SUPPORT
/* Revision of the user file, if it is unchanged from something in the
   repository and we want to use that fact.  */
static char *user_file_rev;
#endif

static char *options;
static char opts[PATH_MAX];
static int diff_errors;
static int empty_files = 0;

static const char *const diff_usage[] =
{
    "Usage: %s %s [-lN] [rcsdiff-options]\n",
#ifdef CVS_DIFFDATE
    "    [[-r rev1 | -D date1] [-r rev2 | -D date2]] [files...] \n",
#else
    "    [-r rev1 [-r rev2]] [files...] \n",
#endif
    "\t-l\tLocal directory only, not recursive\n",
    "\t-D d1\tDiff revision for date against working file.\n",
    "\t-D d2\tDiff rev1/date1 against date2.\n",
    "\t-N\tinclude diffs for added and removed files.\n",
    "\t-r rev1\tDiff revision for rev1 against working file.\n",
    "\t-r rev2\tDiff rev1/date1 against rev2.\n",
    NULL
};

int
diff (argc, argv)
    int argc;
    char **argv;
{
    char tmp[50];
    int c, err = 0;
    int local = 0;
    int which;

    if (argc == -1)
	usage (diff_usage);

    /*
     * Note that we catch all the valid arguments here, so that we can
     * intercept the -r arguments for doing revision diffs; and -l/-R for a
     * non-recursive/recursive diff.
     */
#ifdef SERVER_SUPPORT
    /* Need to be able to do this command more than once (according to
       the protocol spec, even if the current client doesn't use it).  */
    opts[0] = '\0';
#endif
    optind = 1;
    while ((c = getopt (argc, argv,
		   "abcdefhilnpqtuw0123456789BHNQRTC:D:F:I:L:V:k:r:")) != -1)
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
	    case 'N':
		empty_files = 1;
		break;
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

#ifdef CLIENT_SUPPORT
    if (client_active) {
	/* We're the client side.  Fire up the remote server.  */
	start_server ();
	
	ign_setup ();

	if (local)
	    send_arg("-l");
	if (empty_files)
	    send_arg("-N");
	send_option_string (opts);
	if (diff_rev1)
	    option_with_arg ("-r", diff_rev1);
	if (diff_date1)
	    client_senddate (diff_date1);
	if (diff_rev2)
	    option_with_arg ("-r", diff_rev2);
	if (diff_date2)
	    client_senddate (diff_date2);

#if 0
/* FIXME:  We shouldn't have to send current files to diff two revs, but it
   doesn't work yet and I haven't debugged it.  So send the files --
   it's slower but it works.  gnu@cygnus.com  Apr94  */

/* Idea: often times the changed region of a file is relatively small.
   It would be cool if the client could just divide the file into 4k
   blocks or whatever and send hash values for the blocks.  Send hash
   values for blocks aligned with the beginning of the file and the
   end of the file.  Then the server can tell how much of the head and
   tail of the file is unchanged.  Well, hash collisions will screw
   things up, but MD5 has 128 bits of hash value...  */

	/* Send the current files unless diffing two revs from the archive */
	if (diff_rev2 == NULL && diff_date2 == NULL)
	    send_files (argc, argv, local);
	else
	    send_file_names (argc, argv);
#else
	send_files (argc, argv, local, 0);
#endif

	if (fprintf (to_server, "diff\n") < 0)
	    error (1, errno, "writing to server");
        err = get_responses_and_close ();
	free (options);
	return (err);
    }
#endif

    which = W_LOCAL;
    if (diff_rev2 != NULL || diff_date2 != NULL)
	which |= W_REPOS | W_ATTIC;

    wrap_setup ();

    /* start the recursion processor */
    err = start_recursion (diff_fileproc, diff_filesdoneproc, diff_dirproc,
			   diff_dirleaveproc, argc, argv, local,
			   which, 0, 1, (char *) NULL, 1, 0);

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
    enum {
	DIFF_ERROR,
	DIFF_ADDED,
	DIFF_REMOVED,
	DIFF_NEITHER
    } empty_file = DIFF_NEITHER;
    char tmp[L_tmpnam+1];
    char *tocvsPath;
    char fname[PATH_MAX];

#ifdef SERVER_SUPPORT
    user_file_rev = 0;
#endif
    vers = Version_TS (repository, (char *) NULL, (char *) NULL, (char *) NULL,
		       file, 1, 0, entries, srcfiles);

    if (diff_rev2 != NULL || diff_date2 != NULL)
    {
	/* Skip all the following checks regarding the user file; we're
	   not using it.  */
    }
    else if (vers->vn_user == NULL)
    {
	error (0, 0, "I know nothing about %s", file);
	freevers_ts (&vers);
	diff_mark_errors (err);
	return (err);
    }
    else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
    {
	if (empty_files)
	    empty_file = DIFF_ADDED;
	else
	{
	    error (0, 0, "%s is a new entry, no comparison available", file);
	    freevers_ts (&vers);
	    diff_mark_errors (err);
	    return (err);
	}
    }
    else if (vers->vn_user[0] == '-')
    {
	if (empty_files)
	    empty_file = DIFF_REMOVED;
	else
	{
	    error (0, 0, "%s was removed, no comparison available", file);
	    freevers_ts (&vers);
	    diff_mark_errors (err);
	    return (err);
	}
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
#ifdef SERVER_SUPPORT
	    else if (!strcmp (vers->ts_user, vers->ts_rcs)) 
	    {
		/* The user file matches some revision in the repository
		   Diff against the repository (for remote CVS, we might not
		   have a copy of the user file around).  */
		user_file_rev = vers->vn_user;
	    }
#endif
	}
    }

    if (empty_file == DIFF_NEITHER && diff_file_nodiff (file, repository, entries, srcfiles, vers))
    {
	freevers_ts (&vers);
	return (0);
    }

#ifdef DEATH_SUPPORT
    /* FIXME: Check whether use_rev1 and use_rev2 are dead and deal
       accordingly.  */
#endif

    /* Output an "Index:" line for patch to use */
    (void) fflush (stdout);
    if (update_dir[0])
	(void) printf ("Index: %s/%s\n", update_dir, file);
    else
	(void) printf ("Index: %s\n", file);
    (void) fflush (stdout);

    tocvsPath = wrap_tocvs_process_file(file);
    if (tocvsPath)
    {
	/* Backup the current version of the file to CVS/,,filename */
	sprintf(fname,"%s/%s%s",CVSADM, CVSPREFIX, file);
	if (unlink_file_dir (fname) < 0)
	    if (! existence_error (errno))
		error (1, errno, "cannot remove %s", file);
	rename_file (file, fname);
	/* Copy the wrapped file to the current directory then go to work */
	copy_file (tocvsPath, file);
    }

    if (empty_file == DIFF_ADDED || empty_file == DIFF_REMOVED)
    {
	(void) printf ("===================================================================\nRCS file: %s\n",
		       file);
	(void) printf ("diff -N %s\n", file);

	if (empty_file == DIFF_ADDED)
	{
	    run_setup ("%s %s %s %s", DIFF, opts, DEVNULL, file);
	}
	else
	{
	    /*
	     * FIXME: Should be setting use_rev1 using the logic in
	     * diff_file_nodiff, and using that revision.  This code
	     * is broken for "cvs diff -N -r foo".
	     */
	    run_setup ("%s%s -p -q %s -r%s", Rcsbin, RCS_CO,
		       *options ? options : vers->options, vers->vn_rcs);
	    run_arg (vers->srcfile->path);
	    if (run_exec (RUN_TTY, tmpnam (tmp), RUN_TTY, RUN_REALLY) == -1)
	    {
		(void) unlink (tmp);
		error (1, errno, "fork failed during checkout of %s",
		       vers->srcfile->path);
	    }

	    run_setup ("%s %s %s %s", DIFF, opts, tmp, DEVNULL);
	}
    }
    else
    {
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
    }

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

    if (tocvsPath)
    {
	if (unlink_file_dir (file) < 0)
	    if (! existence_error (errno))
		error (1, errno, "cannot remove %s", file);

	rename_file (fname,file);
	if (unlink_file (tocvsPath) < 0)
	    error (1, errno, "cannot remove %s", file);
    }

    if (empty_file == DIFF_REMOVED)
	(void) unlink (tmp);

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
 *
 * Don't try to diff directories that don't exist! -- DW
 */
/* ARGSUSED */
static Dtype
diff_dirproc (dir, pos_repos, update_dir)
    char *dir;
    char *pos_repos;
    char *update_dir;
{
    /* XXX - check for dirs we don't want to process??? */

    /* YES ... for instance dirs that don't exist!!! -- DW */
    if (!isdir (dir) )
      return (R_SKIP_ALL);
  
    if (!quiet)
	error (0, 0, "Diffing %s", update_dir);
    return (R_PROCESS);
}

/*
 * Concoct the proper exit status - done with files
 */
/* ARGSUSED */
static int
diff_filesdoneproc (err, repos, update_dir)
    int err;
    char *repos;
    char *update_dir;
{
    return (diff_errors);
}

/*
 * Concoct the proper exit status - leaving directories
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
	    use_rev1 = xstrdup (vers->srcfile->head);
	else
	{
	    xvers = Version_TS (repository, (char *) NULL, diff_rev1,
				diff_date1, file, 1, 0, entries, srcfiles);
	    if (xvers->vn_rcs == NULL)
	    {
		/* Don't gripe if it doesn't exist, just ignore! */
		if (! isfile (file))
                  /* null statement */ ;
		else if (diff_rev1)
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
	    use_rev2 = xstrdup (vers->srcfile->head);
	else
	{
	    xvers = Version_TS (repository, (char *) NULL, diff_rev2,
				diff_date2, file, 1, 0, entries, srcfiles);
	    if (xvers->vn_rcs == NULL)
	    {
		/* Don't gripe if it doesn't exist, just ignore! */
		if (! isfile (file))
                  /* null statement */ ;
		else if (diff_rev1)
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
	if (use_rev1 && use_rev2) {
	    return (strcmp (use_rev1, use_rev2) == 0);
	} else {
	    error(0, 0, "No HEAD revision for file %s", file);
	    return (1);
	}
    }
#ifdef SERVER_SUPPORT
    if (user_file_rev) 
    {
        /* drop user_file_rev into first unused use_rev */
        if (!use_rev1) 
	  use_rev1 = xstrdup (user_file_rev);
	else if (!use_rev2)
	  use_rev2 = xstrdup (user_file_rev);
	/* and if not, it wasn't needed anyhow */
	user_file_rev = 0;
    }

    /* now, see if we really need to do the diff */
    if (use_rev1 && use_rev2) 
    {
	return (strcmp (use_rev1, use_rev2) == 0);
    }
#endif /* SERVER_SUPPORT */
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
