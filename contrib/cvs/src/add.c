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
#include "savecwd.h"

static int add_directory PROTO((char *repository, List *, char *dir));
static int build_entry PROTO((char *repository, char *user, char *options,
		        char *message, List * entries, char *tag));

static const char *const add_usage[] =
{
    "Usage: %s %s [-k rcs-kflag] [-m message] files...\n",
    "\t-k\tUse \"rcs-kflag\" to add the file with the specified kflag.\n",
    "\t-m\tUse \"message\" for the creation log.\n",
    NULL
};

static char *combine_dir PROTO ((char *, char *));

/* Given a directory DIR and a subdirectory within it, SUBDIR, combine
   the two into a new directory name.  Returns a newly malloc'd string.
   For now this is a fairly simple affair, but perhaps it will want
   to have grander ambitions in the context of VMS or others (or perhaps
   not, perhaps that should all be hidden inside CVS_FOPEN and libc and so
   on, and CVS should just see foo/bar/baz style pathnames).  */
static char *
combine_dir (dir, subdir)
    char *dir;
    char *subdir;
{
    char *retval;
    size_t dir_len;

    dir_len = strlen (dir);
    retval = xmalloc (dir_len + strlen (subdir) + 10);
    if (dir_len >= 2
	&& dir[dir_len - 1] == '.'
	&& ISDIRSEP (dir[dir_len - 2]))
    {
	/* The dir name has an extraneous "." at the end.
	   I'm not completely sure that this is the best place
	   to strip it off--it is possible that Name_Repository
	   should do so, or it shouldn't be in the CVS/Repository
	   file in the first place.  Fixing it here seems like
	   a safe, small change, but I'm not sure it catches
	   all the cases.  */
	strncpy (retval, dir, dir_len - 2);
	retval[dir_len - 2] = '\0';
    }
    else
    {
	strcpy (retval, dir);
    }
    strcat (retval, "/");
    strcat (retval, subdir);
    return retval;
}

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
    while ((c = getopt (argc, argv, "+k:m:")) != -1)
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
		int nonbranch;
		char *rcsdir;

		/* before we do anything else, see if we have any
		   per-directory tags */
		ParseTag (&tag, &date, &nonbranch);

		rcsdir = combine_dir (repository, argv[i]);

		strip_trailing_slashes (argv[i]);

		Create_Admin (argv[i], argv[i], rcsdir, tag, date, nonbranch);

		if (tag)
		    free (tag);
		if (date)
		    free (date);
		free (rcsdir);

		if (strchr (argv[i], '/') == NULL)
		    Subdir_Register ((List *) NULL, (char *) NULL, argv[i]);
		else
		{
		    char *cp, *b;

		    cp = xstrdup (argv[i]);
		    b = strrchr (cp, '/');
		    *b++ = '\0';
		    Subdir_Register ((List *) NULL, cp, b);
		    free (cp);
		}
	    }
	send_file_names (argc, argv, SEND_EXPAND_WILD);
	/* FIXME: should be able to pass SEND_NO_CONTENTS, I think.  */
	send_files (argc, argv, 0, 0, SEND_BUILD_DIRS);
	send_to_server ("add\012", 0);
	if (message)
	    free (message);
	free (repository);
	return get_responses_and_close ();
    }
#endif

    entries = Entries_Open (0);

    /* walk the arg list adding files/dirs */
    for (i = 0; i < argc; i++)
    {
	int begin_err = err;
#ifdef SERVER_SUPPORT
	int begin_added_files = added_files;
#endif
	struct file_info finfo;

	user = argv[i];
	strip_trailing_slashes (user);
	if (strchr (user, '/') != NULL)
	{
	    error (0, 0,
	     "cannot add files with '/' in their name; %s not added", user);
	    err++;
	    continue;
	}

	memset (&finfo, 0, sizeof finfo);
	finfo.file = user;
	finfo.update_dir = "";
	finfo.fullname = user;
	finfo.repository = repository;
	finfo.entries = entries;
	finfo.rcs = NULL;

	/* We pass force_tag_match as 1.  If the directory has a
           sticky branch tag, and there is already an RCS file which
           does not have that tag, then the head revision is
           meaningless to us.  */
	vers = Version_TS (&finfo, options, NULL, NULL, 1, 0);
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
		    char *dname = xmalloc (strlen (repository) + strlen (user)
					   + 10);
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
		    free (dname);

		    if (vers->options == NULL || *vers->options == '\0')
		    {
			/* No options specified on command line (or in
			   rcs file if it existed, e.g. the file exists
			   on another branch).  Check for a value from
			   the wrapper stuff.  */
			if (wrap_name_has (user, WRAP_RCSOPTION))
			{
			    if (vers->options)
				free (vers->options);
			    vers->options = wrap_rcsoption (user, 1);
			}
		    }

		    if (vers->nonbranch)
		    {
			error (0, 0,
			       "cannot add file on non-branch tag %s",
			       vers->tag);
			++err;
		    }
		    else
		    {
			/* There is a user file, so build the entry for it */
			if (build_entry (repository, user, vers->options,
					 message, entries, vers->tag) != 0)
			    err++;
			else
			{
			    added_files++;
			    if (!quiet)
			    {
				if (vers->tag)
				    error (0, 0, "\
scheduling %s `%s' for addition on branch `%s'",
					   (wrap_name_has (user, WRAP_TOCVS)
					    ? "wrapper"
					    : "file"),
					   user, vers->tag);
				else
				    error (0, 0,
					   "scheduling %s `%s' for addition",
					   (wrap_name_has (user, WRAP_TOCVS)
					    ? "wrapper"
					    : "file"),
					   user);
			    }
			}
		    }
		}
	    }
	    else if (RCS_isdead (vers->srcfile, vers->vn_rcs))
	    {
		if (isdir (user) && !wrap_name_has (user, WRAP_TOCVS))
		{
		    error (0, 0, "\
the directory `%s' cannot be added because a file of the", user);
		    error (1, 0, "\
same name already exists in the repository.");
		}
		else
		{
		    if (vers->nonbranch)
		    {
			error (0, 0,
			       "cannot add file on non-branch tag %s",
			       vers->tag);
			++err;
		    }
		    else
		    {
			if (vers->tag)
			    error (0, 0, "\
file `%s' will be added on branch `%s' from version %s",
				   user, vers->tag, vers->vn_rcs);
			else
			    /* I'm not sure that mentioning
			       vers->vn_rcs makes any sense here; I
			       can't think of a way to word the
			       message which is not confusing.  */
			    error (0, 0, "\
re-adding file %s (in place of dead revision %s)",
				   user, vers->vn_rcs);
			Register (entries, user, "0", vers->ts_user, NULL,
				  vers->tag, NULL, NULL);
			++added_files;
		    }
		}
	    }
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
		    error (0, 0, "\
cannot resurrect %s; RCS file removed by second party", user);
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
		error (0, 0, "\
%s should be removed and is still there (or is back again)", user);
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
	    err += add_directory (repository, entries, user);
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
    free (repository);

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
add_directory (repository, entries, dir)
    char *repository;
    List *entries;
    char *dir;
{
    char *rcsdir = NULL;
    struct saved_cwd cwd;
    char *message = NULL;
    char *tag, *date;
    int nonbranch;

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
    ParseTag (&tag, &date, &nonbranch);

    /* now, remember where we were, so we can get back */
    if (save_cwd (&cwd))
	return (1);
    if ( CVS_CHDIR (dir) < 0)
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

    rcsdir = combine_dir (repository, dir);
    if (isfile (rcsdir) && !isdir (rcsdir))
    {
	error (0, 0, "%s is not a directory; %s not added", rcsdir, dir);
	goto out;
    }

    /* setup the log message */
    message = xmalloc (strlen (rcsdir) + 80);
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
	struct logfile_info *li;

	/* There used to be some code here which would prompt for
	   whether to add the directory.  The details of that code had
	   bitrotted, but more to the point it can't work
	   client/server, doesn't ask in the right way for GUIs, etc.
	   A better way of making it harder to accidentally add
	   directories would be to have to add and commit directories
	   like for files.  The code was #if 0'd at least since CVS 1.5.  */

	if (!noexec)
	{
	    omask = umask (cvsumask);
	    if (CVS_MKDIR (rcsdir, 0777) < 0)
	    {
		error (0, errno, "cannot mkdir %s", rcsdir);
		(void) umask (omask);
		goto out;
	    }
	    (void) umask (omask);
	}

	/*
	 * Set up an update list with a single title node for Update_Logfile
	 */
	ulist = getlist ();
	p = getnode ();
	p->type = UPDATE;
	p->delproc = update_delproc;
	p->key = xstrdup ("- New directory");
	li = (struct logfile_info *) xmalloc (sizeof (struct logfile_info));
	li->type = T_TITLE;
	li->tag = xstrdup (tag);
	li->rev_old = li->rev_new = NULL;
	p->data = (char *) li;
	(void) addnode (ulist, p);
	Update_Logfile (rcsdir, message, (FILE *) NULL, ulist);
	dellist (&ulist);
    }

#ifdef SERVER_SUPPORT
    if (!server_active)
	Create_Admin (".", dir, rcsdir, tag, date, nonbranch);
#else
    Create_Admin (".", dir, rcsdir, tag, date, nonbranch);
#endif
    if (tag)
	free (tag);
    if (date)
	free (date);

    if (restore_cwd (&cwd, NULL))
	error_exit ();
    free_cwd (&cwd);

    Subdir_Register (entries, (char *) NULL, dir);

    (void) printf ("%s", message);
    free (rcsdir);
    free (message);

    return (0);

out:
    if (restore_cwd (&cwd, NULL))
	error_exit ();
    free_cwd (&cwd);
    if (rcsdir != NULL)
	free (rcsdir);
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
    char *fname;
    char *line;
    FILE *fp;

    if (noexec)
	return (0);

    /*
     * The requested log is read directly from the user and stored in the
     * file user,t.  If the "message" argument is set, use it as the
     * initial creation log (which typically describes the file).
     */
    fname = xmalloc (strlen (user) + 80);
    (void) sprintf (fname, "%s/%s%s", CVSADM, user, CVSEXT_LOG);
    fp = open_file (fname, "w+");
    if (message && fputs (message, fp) == EOF)
	    error (1, errno, "cannot write to %s", fname);
    if (fclose(fp) == EOF)
        error(1, errno, "cannot close %s", fname);
    free (fname);

    /*
     * Create the entry now, since this allows the user to interrupt us above
     * without needing to clean anything up (well, we could clean up the
     * ,t file, but who cares).
     */
    line = xmalloc (strlen (user) + 20);
    (void) sprintf (line, "Initial %s", user);
    Register (entries, user, "0", line, options, tag, (char *) 0, (char *) 0);
    free (line);
    return (0);
}
