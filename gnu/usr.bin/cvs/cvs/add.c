/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Add
 * 
 * Adds a file or directory to the RCS source repository.  For a file,
 * the entry is marked as "needing to be added" in the user's own CVS
 * directory, and really added to the repository when it is committed.
 * For a directory, it is added at the appropriate place in the source
 * repository and a CVS directory is generated within the directory.
 * 
 * The -m option is currently the only supported option.  Some may wish to
 * supply standard "rcs" options here, but I've found that this causes more
 * trouble than anything else.
 * 
 * The user files or directories must already exist.  For a directory, it must
 * not already have a CVS file in it.
 * 
 * An "add" on a file that has been "remove"d but not committed will cause the
 * file to be resurrected.
 */

#include "cvs.h"
#include "save-cwd.h"

#ifndef lint
static const char rcsid[] = "$CVSid: @(#)add.c 1.55 94/10/22 $";
USE(rcsid);
#endif

static int add_directory PROTO((char *repository, char *dir));
static int build_entry PROTO((char *repository, char *user, char *options,
		        char *message, List * entries, char *tag));

static const char *const add_usage[] =
{
    "Usage: %s %s [-k rcs-kflag] [-m message] files...\n",
    "\t-k\tUse \"rcs-kflag\" to add the file with the specified kflag.\n",
    "\t-m\tUse \"message\" for the creation log.\n",
    NULL
};

int
add (argc, argv)
    int argc;
    char **argv;
{
    char *message = NULL;
    char *user;
    int i;
    char *repository;
    int c;
    int err = 0;
    int added_files = 0;
    char *options = NULL;
    List *entries;
    Vers_TS *vers;

    if (argc == 1 || argc == -1)
	usage (add_usage);

    wrap_setup ();

    /* parse args */
    optind = 1;
    while ((c = getopt (argc, argv, "k:m:")) != -1)
    {
	switch (c)
	{
	    case 'k':
		if (options)
		    free (options);
		options = RCS_check_kflag (optarg);
		break;

	    case 'm':
		message = xstrdup (optarg);
		break;
	    case '?':
	    default:
		usage (add_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (argc <= 0)
	usage (add_usage);

    /* find the repository associated with our current dir */
    repository = Name_Repository ((char *) NULL, (char *) NULL);

#ifdef CLIENT_SUPPORT
    if (client_active)
      {
	int i;
	start_server ();
	ign_setup ();
	if (options) send_arg(options);
	option_with_arg ("-m", message);
	for (i = 0; i < argc; ++i)
	  /* FIXME: Does this erroneously call Create_Admin in error
	     conditions which are only detected once the server gets its
	     hands on things?  */
	  if (isdir (argv[i]))
	    {
	      char *tag;
	      char *date;
	      char *rcsdir = xmalloc (strlen (repository)
				      + strlen (argv[i]) + 10);

	      /* before we do anything else, see if we have any
		 per-directory tags */
	      ParseTag (&tag, &date);

	      sprintf (rcsdir, "%s/%s", repository, argv[i]);

	      Create_Admin (argv[i], argv[i], rcsdir, tag, date);

	      if (tag)
		free (tag);
	      if (date)
		free (date);
	      free (rcsdir);
	    }
	send_files (argc, argv, 0, 0);
	if (fprintf (to_server, "add\n") < 0)
	  error (1, errno, "writing to server");
	return get_responses_and_close ();
      }
#endif

    entries = Entries_Open (0);

    /* walk the arg list adding files/dirs */
    for (i = 0; i < argc; i++)
    {
	int begin_err = err;
	int begin_added_files = added_files;

	user = argv[i];
	strip_trailing_slashes (user);
	if (strchr (user, '/') != NULL)
	{
	    error (0, 0,
	     "cannot add files with '/' in their name; %s not added", user);
	    err++;
	    continue;
	}

	vers = Version_TS (repository, options, (char *) NULL, (char *) NULL,
			   user, 0, 0, entries, (List *) NULL);
	if (vers->vn_user == NULL)
	{
	    /* No entry available, ts_rcs is invalid */
	    if (vers->vn_rcs == NULL)
	    {
		/* There is no RCS file either */
		if (vers->ts_user == NULL)
		{
		    /* There is no user file either */
		    error (0, 0, "nothing known about %s", user);
		    err++;
		}
		else if (!isdir (user) || wrap_name_has (user, WRAP_TOCVS))
		{
		    /*
		     * See if a directory exists in the repository with
		     * the same name.  If so, blow this request off.
		     */
		    char dname[PATH_MAX];
		    (void) sprintf (dname, "%s/%s", repository, user);
		    if (isdir (dname))
		    {
			error (0, 0,
			       "cannot add file `%s' since the directory",
			       user);
			error (0, 0, "`%s' already exists in the repository",
			       dname);
			error (1, 0, "illegal filename overlap");
		    }

		    /* There is a user file, so build the entry for it */
		    if (build_entry (repository, user, vers->options,
				     message, entries, vers->tag) != 0)
			err++;
		    else 
		    {
			added_files++;
			if (!quiet)
			{
#ifdef DEATH_SUPPORT
			    if (vers->tag)
				error (0, 0, "\
scheduling %s `%s' for addition on branch `%s'",
				       (wrap_name_has (user, WRAP_TOCVS)
					? "wrapper"
					: "file"),
				       user, vers->tag);
			    else
#endif /* DEATH_SUPPORT */
			    error (0, 0, "scheduling %s `%s' for addition",
				   (wrap_name_has (user, WRAP_TOCVS)
				    ? "wrapper"
				    : "file"),
				   user);
			}
		    }
		}
	    }
#ifdef DEATH_SUPPORT
	    else if (RCS_isdead (vers->srcfile, vers->vn_rcs))
	    {
		if (isdir (user) && !wrap_name_has (user, WRAP_TOCVS))
		{
		    error (0, 0, "the directory `%s' cannot be added because a file of the", user);
		    error (1, 0, "same name already exists in the repository.");
		}
		else
		{
		    if (vers->tag)
			error (0, 0, "file `%s' will be added on branch `%s' from version %s",
			       user, vers->tag, vers->vn_rcs);
		    else
			error (0, 0, "version %s of `%s' will be resurrected",
			       vers->vn_rcs, user);
		    Register (entries, user, "0", vers->ts_user, NULL,
			      vers->tag, NULL, NULL);
		    ++added_files;
		}
	    }
#endif /* DEATH_SUPPORT */
	    else
	    {
		/*
		 * There is an RCS file already, so somebody else must've
		 * added it
		 */
		error (0, 0, "%s added independently by second party", user);
		err++;
	    }
	}
	else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
	{

	    /*
	     * An entry for a new-born file, ts_rcs is dummy, but that is
	     * inappropriate here
	     */
	    error (0, 0, "%s has already been entered", user);
	    err++;
	}
	else if (vers->vn_user[0] == '-')
	{
	    /* An entry for a removed file, ts_rcs is invalid */
	    if (vers->ts_user == NULL)
	    {
		/* There is no user file (as it should be) */
		if (vers->vn_rcs == NULL)
		{

		    /*
		     * There is no RCS file, so somebody else must've removed
		     * it from under us
		     */
		    error (0, 0,
			   "cannot resurrect %s; RCS file removed by second party", user);
		    err++;
		}
		else
		{

		    /*
		     * There is an RCS file, so remove the "-" from the
		     * version number and restore the file
		     */
		    char *tmp = xmalloc (strlen (user) + 50);

		    (void) strcpy (tmp, vers->vn_user + 1);
		    (void) strcpy (vers->vn_user, tmp);
		    (void) sprintf (tmp, "Resurrected %s", user);
		    Register (entries, user, vers->vn_user, tmp, vers->options,
			      vers->tag, vers->date, vers->ts_conflict);
		    free (tmp);

		    /* XXX - bugs here; this really resurrect the head */
		    /* Note that this depends on the Register above actually
		       having written Entries, or else it won't really
		       check the file out.  */
		    if (update (2, argv + i - 1) == 0)
		    {
			error (0, 0, "%s, version %s, resurrected", user,
			       vers->vn_user);
		    }
		    else
		    {
			error (0, 0, "could not resurrect %s", user);
			err++;
		    }
		}
	    }
	    else
	    {
		/* The user file shouldn't be there */
		error (0, 0, "%s should be removed and is still there (or is back again)", user);
		err++;
	    }
	}
	else
	{
	    /* A normal entry, ts_rcs is valid, so it must already be there */
	    error (0, 0, "%s already exists, with version number %s", user,
		   vers->vn_user);
	    err++;
	}
	freevers_ts (&vers);

	/* passed all the checks.  Go ahead and add it if its a directory */
	if (begin_err == err
	    && isdir (user)
	    && !wrap_name_has (user, WRAP_TOCVS))
	{
	    err += add_directory (repository, user);
	    continue;
	}
#ifdef SERVER_SUPPORT
	if (server_active && begin_added_files != added_files)
	    server_checked_in (user, ".", repository);
#endif
    }
    if (added_files)
	error (0, 0, "use 'cvs commit' to add %s permanently",
	       (added_files == 1) ? "this file" : "these files");

    Entries_Close (entries);

    if (message)
	free (message);

    return (err);
}

/*
 * The specified user file is really a directory.  So, let's make sure that
 * it is created in the RCS source repository, and that the user's directory
 * is updated to include a CVS directory.
 * 
 * Returns 1 on failure, 0 on success.
 */
static int
add_directory (repository, dir)
    char *repository;
    char *dir;
{
    char rcsdir[PATH_MAX];
    struct saved_cwd cwd;
    char message[PATH_MAX + 100];
    char *tag, *date;

    if (strchr (dir, '/') != NULL)
    {
	error (0, 0,
	       "directory %s not added; must be a direct sub-directory", dir);
	return (1);
    }
    if (strcmp (dir, CVSADM) == 0)
    {
	error (0, 0, "cannot add a `%s' directory", CVSADM);
	return (1);
    }

    /* before we do anything else, see if we have any per-directory tags */
    ParseTag (&tag, &date);

    /* now, remember where we were, so we can get back */
    if (save_cwd (&cwd))
	return (1);
    if (chdir (dir) < 0)
    {
	error (0, errno, "cannot chdir to %s", dir);
	return (1);
    }
#ifdef SERVER_SUPPORT
    if (!server_active && isfile (CVSADM))
#else
    if (isfile (CVSADM))
#endif
    {
	error (0, 0, "%s/%s already exists", dir, CVSADM);
	goto out;
    }

    (void) sprintf (rcsdir, "%s/%s", repository, dir);
    if (isfile (rcsdir) && !isdir (rcsdir))
    {
	error (0, 0, "%s is not a directory; %s not added", rcsdir, dir);
	goto out;
    }

    /* setup the log message */
    (void) sprintf (message, "Directory %s added to the repository\n", rcsdir);
    if (tag)
    {
	(void) strcat (message, "--> Using per-directory sticky tag `");
	(void) strcat (message, tag);
	(void) strcat (message, "'\n");
    }
    if (date)
    {
	(void) strcat (message, "--> Using per-directory sticky date `");
	(void) strcat (message, date);
	(void) strcat (message, "'\n");
    }

    if (!isdir (rcsdir))
    {
	mode_t omask;
	Node *p;
	List *ulist;

#if 0
	char line[MAXLINELEN];

	(void) printf ("Add directory %s to the repository (y/n) [n] ? ",
		       rcsdir);
	(void) fflush (stdout);
	clearerr (stdin);
	if (fgets (line, sizeof (line), stdin) == NULL ||
	    (line[0] != 'y' && line[0] != 'Y'))
	{
	    error (0, 0, "directory %s not added", rcsdir);
	    goto out;
	}
#endif

	omask = umask (cvsumask);
	if (CVS_MKDIR (rcsdir, 0777) < 0)
	{
	    error (0, errno, "cannot mkdir %s", rcsdir);
	    (void) umask (omask);
	    goto out;
	}
	(void) umask (omask);

	/*
	 * Set up an update list with a single title node for Update_Logfile
	 */
	ulist = getlist ();
	p = getnode ();
	p->type = UPDATE;
	p->delproc = update_delproc;
	p->key = xstrdup ("- New directory");
	p->data = (char *) T_TITLE;
	(void) addnode (ulist, p);
	Update_Logfile (rcsdir, message, (char *) NULL, (FILE *) NULL, ulist);
	dellist (&ulist);
    }

#ifdef SERVER_SUPPORT
    if (!server_active)
	Create_Admin (".", dir, rcsdir, tag, date);
#else
    Create_Admin (".", dir, rcsdir, tag, date);
#endif
    if (tag)
	free (tag);
    if (date)
	free (date);

    (void) printf ("%s", message);
out:
    if (restore_cwd (&cwd, NULL))
      exit (1);
    free_cwd (&cwd);
    return (0);
}

/*
 * Builds an entry for a new file and sets up "CVS/file",[pt] by
 * interrogating the user.  Returns non-zero on error.
 */
static int
build_entry (repository, user, options, message, entries, tag)
    char *repository;
    char *user;
    char *options;
    char *message;
    List *entries;
    char *tag;
{
    char fname[PATH_MAX];
    char line[MAXLINELEN];
    FILE *fp;

#ifndef DEATH_SUPPORT
 /* when using the rcs death support, this case is not a problem. */
    /*
     * There may be an old file with the same name in the Attic! This is,
     * perhaps, an awkward place to check for this, but other places are
     * equally awkward.
     */
    (void) sprintf (fname, "%s/%s/%s%s", repository, CVSATTIC, user, RCSEXT);
    if (isreadable (fname))
    {
	error (0, 0, "there is an old file %s already in %s/%s", user,
	       repository, CVSATTIC);
	return (1);
    }
#endif /* no DEATH_SUPPORT */

    if (noexec)
	return (0);

    /*
     * The requested log is read directly from the user and stored in the
     * file user,t.  If the "message" argument is set, use it as the
     * initial creation log (which typically describes the file).
     */
    (void) sprintf (fname, "%s/%s%s", CVSADM, user, CVSEXT_LOG);
    fp = open_file (fname, "w+");
    if (message && fputs (message, fp) == EOF)
	    error (1, errno, "cannot write to %s", fname);
    if (fclose(fp) == EOF)
        error(1, errno, "cannot close %s", fname);

    /*
     * Create the entry now, since this allows the user to interrupt us above
     * without needing to clean anything up (well, we could clean up the
     * ,t file, but who cares).
     */
    (void) sprintf (line, "Initial %s", user);
    Register (entries, user, "0", line, options, tag, (char *) 0, (char *) 0);
    return (0);
}
