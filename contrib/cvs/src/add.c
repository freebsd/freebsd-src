/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
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

#include <assert.h>
#include "cvs.h"
#include "savecwd.h"
#include "fileattr.h"

static int add_directory PROTO ((struct file_info *finfo));
static int build_entry PROTO((const char *repository, const char *user,
                              const char *options, const char *message,
                              List * entries, const char *tag));

static const char *const add_usage[] =
{
    "Usage: %s %s [-k rcs-kflag] [-m message] files...\n",
    "\t-k\tUse \"rcs-kflag\" to add the file with the specified kflag.\n",
    "\t-m\tUse \"message\" for the creation log.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int
add (argc, argv)
    int argc;
    char **argv;
{
    char *message = NULL;
    int i;
    char *repository;
    int c;
    int err = 0;
    int added_files = 0;
    char *options = NULL;
    List *entries;
    Vers_TS *vers;
    struct saved_cwd cwd;
    /* Nonzero if we found a slash, and are thus adding files in a
       subdirectory.  */
    int found_slash = 0;
    size_t cvsroot_len;

    if (argc == 1 || argc == -1)
	usage (add_usage);

    wrap_setup ();

    /* parse args */
    optind = 0;
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

    cvsroot_len = strlen (current_parsed_root->directory);

    /* First some sanity checks.  I know that the CVS case is (sort of)
       also handled by add_directory, but we need to check here so the
       client won't get all confused in send_file_names.  */
    for (i = 0; i < argc; i++)
    {
	int skip_file = 0;

	/* If it were up to me I'd probably make this a fatal error.
	   But some people are really fond of their "cvs add *", and
	   don't seem to object to the warnings.
	   Whatever.  */
	strip_trailing_slashes (argv[i]);
	if (strcmp (argv[i], ".") == 0
	    || strcmp (argv[i], "..") == 0
	    || fncmp (argv[i], CVSADM) == 0)
	{
	    if (!quiet)
		error (0, 0, "cannot add special file `%s'; skipping", argv[i]);
	    skip_file = 1;
	}
	else
	{
	    char *p;
	    p = argv[i];
	    while (*p != '\0')
	    {
		if (ISDIRSEP (*p))
		{
		    found_slash = 1;
		    break;
		}
		++p;
	    }
	}

	if (skip_file)
	{
	    int j;

	    /* FIXME: We don't do anything about free'ing argv[i].  But
	       the problem is that it is only sometimes allocated (see
	       cvsrc.c).  */

	    for (j = i; j < argc - 1; ++j)
		argv[j] = argv[j + 1];
	    --argc;
	    /* Check the new argv[i] again.  */
	    --i;
	    ++err;
	}
    }

#ifdef CLIENT_SUPPORT
    if (current_parsed_root->isremote)
    {
	int j;

	if (argc == 0)
	    /* We snipped out all the arguments in the above sanity
	       check.  We can just forget the whole thing (and we
	       better, because if we fired up the server and passed it
	       nothing, it would spit back a usage message).  */
	    return err;

	start_server ();
	ign_setup ();
	if (options)
	{
	    send_arg (options);
	    free (options);
	}
	option_with_arg ("-m", message);
	send_arg ("--");

	/* If !found_slash, refrain from sending "Directory", for
	   CVS 1.9 compatibility.  If we only tried to deal with servers
	   which are at least CVS 1.9.26 or so, we wouldn't have to
	   special-case this.  */
	if (found_slash)
	{
	    repository = Name_Repository (NULL, NULL);
	    send_a_repository ("", repository, "");
	    free (repository);
	}

	for (j = 0; j < argc; ++j)
	{
	    /* FIXME: Does this erroneously call Create_Admin in error
	       conditions which are only detected once the server gets its
	       hands on things?  */
	    /* FIXME-also: if filenames are case-insensitive on the
	       client, and the directory in the repository already
	       exists and is named "foo", and the command is "cvs add
	       FOO", this call to Create_Admin puts the wrong thing in
	       CVS/Repository and so a subsequent "cvs update" will
	       give an error.  The fix will be to have the server report
	       back what it actually did (e.g. use tagged text for the
	       "Directory %s added" message), and then Create_Admin,
	       which should also fix the error handling concerns.  */

	    if (isdir (argv[j]))
	    {
		char *tag;
		char *date;
		int nonbranch;
		char *rcsdir;
		char *p;
		char *update_dir;
		/* This is some mungeable storage into which we can point
		   with p and/or update_dir.  */
		char *filedir;

		if (save_cwd (&cwd))
		    error_exit ();

		filedir = xstrdup (argv[j]);
                /* Deliberately discard the const below since we know we just
                 * allocated filedir and can do what we like with it.
                 */
		p = (char *)last_component (filedir);
		if (p == filedir)
		{
		    update_dir = "";
		}
		else
		{
		    p[-1] = '\0';
		    update_dir = filedir;
		    if (CVS_CHDIR (update_dir) < 0)
			error (1, errno,
			       "could not chdir to %s", update_dir);
		}

		/* find the repository associated with our current dir */
		repository = Name_Repository (NULL, update_dir);

		/* don't add stuff to Emptydir */
		if (strncmp (repository, current_parsed_root->directory, cvsroot_len) == 0
		    && ISDIRSEP (repository[cvsroot_len])
		    && strncmp (repository + cvsroot_len + 1,
				CVSROOTADM,
				sizeof CVSROOTADM - 1) == 0
		    && ISDIRSEP (repository[cvsroot_len + sizeof CVSROOTADM])
		    && strcmp (repository + cvsroot_len + sizeof CVSROOTADM + 1,
			       CVSNULLREPOS) == 0)
		    error (1, 0, "cannot add to %s", repository);

		/* before we do anything else, see if we have any
		   per-directory tags */
		ParseTag (&tag, &date, &nonbranch);

		rcsdir = xmalloc (strlen (repository) + strlen (p) + 5);
		sprintf (rcsdir, "%s/%s", repository, p);

		Create_Admin (p, argv[j], rcsdir, tag, date,
			      nonbranch, 0, 1);

		if (found_slash)
		    send_a_repository ("", repository, update_dir);

		if (restore_cwd (&cwd, NULL))
		    error_exit ();
		free_cwd (&cwd);

		if (tag)
		    free (tag);
		if (date)
		    free (date);
		free (rcsdir);

		if (p == filedir)
		    Subdir_Register ((List *) NULL, (char *) NULL, argv[j]);
		else
		{
		    Subdir_Register ((List *) NULL, update_dir, p);
		}
		free (repository);
		free (filedir);
	    }
	}
	send_files (argc, argv, 0, 0, SEND_BUILD_DIRS | SEND_NO_CONTENTS);
	send_file_names (argc, argv, SEND_EXPAND_WILD);
	send_to_server ("add\012", 0);
	if (message)
	    free (message);
	return err + get_responses_and_close ();
    }
#endif

    /* walk the arg list adding files/dirs */
    for (i = 0; i < argc; i++)
    {
	int begin_err = err;
#ifdef SERVER_SUPPORT
	int begin_added_files = added_files;
#endif
	struct file_info finfo;
	char *filename, *p;

	memset (&finfo, 0, sizeof finfo);

	if (save_cwd (&cwd))
	    error_exit ();

	finfo.fullname = xstrdup (argv[i]);
	filename = xstrdup (argv[i]);
	/* We know we can discard the const below since we just allocated
	 * filename and can do as we like with it.
         */
	p = (char *)last_component (filename);
	if (p == filename)
	{
	    finfo.update_dir = "";
	    finfo.file = p;
	}
	else
	{
	    p[-1] = '\0';
	    finfo.update_dir = filename;
	    finfo.file = p;
	    if (CVS_CHDIR (finfo.update_dir) < 0)
		error (1, errno, "could not chdir to %s", finfo.update_dir);
	}

	/* Add wrappers for this directory.  They exist only until
	   the next call to wrap_add_file.  */
	wrap_add_file (CVSDOTWRAPPER, 1);

	finfo.rcs = NULL;

	/* Find the repository associated with our current dir.  */
	repository = Name_Repository (NULL, finfo.update_dir);

	/* don't add stuff to Emptydir */
	if (strncmp (repository, current_parsed_root->directory,
                     cvsroot_len) == 0
	    && ISDIRSEP (repository[cvsroot_len])
	    && strncmp (repository + cvsroot_len + 1,
			CVSROOTADM,
			sizeof CVSROOTADM - 1) == 0
	    && ISDIRSEP (repository[cvsroot_len + sizeof CVSROOTADM])
	    && strcmp (repository + cvsroot_len + sizeof CVSROOTADM + 1,
		       CVSNULLREPOS) == 0)
	    error (1, 0, "cannot add to %s", repository);

	entries = Entries_Open (0, NULL);

	finfo.repository = repository;
	finfo.entries = entries;

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
		    error (0, 0, "nothing known about %s", finfo.fullname);
		    err++;
		}
		else if (!isdir (finfo.file)
			 || wrap_name_has (finfo.file, WRAP_TOCVS))
		{
		    /*
		     * See if a directory exists in the repository with
		     * the same name.  If so, blow this request off.
		     */
		    char *dname = xmalloc (strlen (repository)
					   + strlen (finfo.file)
					   + 10);
		    (void) sprintf (dname, "%s/%s", repository, finfo.file);
		    if (isdir (dname))
		    {
			error (0, 0,
			       "cannot add file `%s' since the directory",
			       finfo.fullname);
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
			if (wrap_name_has (finfo.file, WRAP_RCSOPTION))
			{
			    if (vers->options)
				free (vers->options);
			    vers->options = wrap_rcsoption (finfo.file, 1);
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
			if (build_entry (repository, finfo.file, vers->options,
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
					   (wrap_name_has (finfo.file,
							   WRAP_TOCVS)
					    ? "wrapper"
					    : "file"),
					   finfo.fullname, vers->tag);
				else
				    error (0, 0,
					   "scheduling %s `%s' for addition",
					   (wrap_name_has (finfo.file,
							   WRAP_TOCVS)
					    ? "wrapper"
					    : "file"),
					   finfo.fullname);
			    }
			}
		    }
		}
	    }
	    else if (RCS_isdead (vers->srcfile, vers->vn_rcs))
	    {
		if (isdir (finfo.file)
		    && !wrap_name_has (finfo.file, WRAP_TOCVS))
		{
		    error (0, 0, "\
the directory `%s' cannot be added because a file of the", finfo.fullname);
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
			char *timestamp = NULL;
			if (vers->ts_user == NULL)
			{
			    /* If this file does not exist locally, assume that
			     * the last version on the branch is being
			     * resurrected.
			     *
			     * Compute previous revision.  We assume that it
			     * exists and that it is not a revision on the
			     * trunk of the form X.1 (1.1, 2.1, 3.1, ...).  We
			     * also assume that it is not dead, which seems
			     * fair since we know vers->vn_rcs is dead
			     * and we shouldn't see two dead revisions in a
			     * row.
			     */
			    char *prev = previous_rev (vers->srcfile,
			                               vers->vn_rcs);
			    int status;
			    assert (prev != NULL);
			    if (!quiet)
				error (0, 0,
"Resurrecting file `%s' from revision %s.",
			               finfo.fullname, prev);
			    status = RCS_checkout (vers->srcfile, finfo.file,
						   prev, vers->tag,
						   vers->options, RUN_TTY,
			                           NULL, NULL);
			    xchmod (finfo.file, 1);
			    if (status != 0)
			    {
				error (0, 0, "Failed to resurrect revision %s",
				       prev);
				err++;
			    }
			    else
			    {
				/* I don't actually set vers->ts_user here
				 * because it would confuse server_update().
				 */
				timestamp = time_stamp (finfo.file);
				if (!really_quiet)
				    write_letter (&finfo, 'U');
			    }
			    free (prev);
			}
			if (!quiet)
			{
			    if (vers->tag)
				error (0, 0,
"file `%s' will be added on branch `%s' from version %s",
				       finfo.fullname, vers->tag,
				       vers->vn_rcs);
			    else
				/* I'm not sure that mentioning
				   vers->vn_rcs makes any sense here; I
				   can't think of a way to word the
				   message which is not confusing.  */
				error (0, 0,
"Re-adding file `%s' (in place of dead revision %s).",
				       finfo.fullname, vers->vn_rcs);
			}
			Register (entries, finfo.file, "0",
				  timestamp ? timestamp : vers->ts_user,
				  vers->options, vers->tag, vers->date, NULL);
			if (timestamp) free (timestamp);
#ifdef SERVER_SUPPORT
			if (server_active && vers->ts_user == NULL)
			{
			    /* If we resurrected the file from the archive, we
			     * need to tell the client about it.
			     */
			    server_updated (&finfo, vers,
					    SERVER_UPDATED,
					    (mode_t) -1, NULL, NULL);
			    /* This is kinda hacky or, at least, it renders the
			     * name "begin_added_files" obsolete, but we want
			     * the added_files to be counted without triggering
			     * the check that causes server_checked_in() to be
			     * called below since we have already called
			     * server_updated() to complete the resurrection.
			     */
			    ++begin_added_files;
			}
#endif
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
		error (0, 0, "%s added independently by second party",
		       finfo.fullname);
		err++;
	    }
	}
	else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
	{

	    /*
	     * An entry for a new-born file, ts_rcs is dummy, but that is
	     * inappropriate here
	     */
	    if (!quiet)
		error (0, 0, "%s has already been entered", finfo.fullname);
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
cannot resurrect %s; RCS file removed by second party", finfo.fullname);
		    err++;
		}
		else
		{
		    int status;
		    /*
		     * There is an RCS file, so remove the "-" from the
		     * version number and restore the file
		     */
		    char *tmp = xmalloc (strlen (vers->vn_user));
		    (void) strcpy (tmp, vers->vn_user + 1);
		    (void) strcpy (vers->vn_user, tmp);
		    free(tmp);
		    status = RCS_checkout (vers->srcfile, finfo.file,
					   vers->vn_user, vers->tag,
					   vers->options, RUN_TTY,
					   NULL, NULL);
		    xchmod (finfo.file, 1);
		    if (status != 0)
		    {
			error (0, 0, "Failed to resurrect revision %s",
			       vers->vn_user);
			err++;
			tmp = NULL;
		    }
		    else
		    {
			/* I don't actually set vers->ts_user here because it
			 * would confuse server_update().
			 */
			tmp = time_stamp (finfo.file);
			write_letter (&finfo, 'U');
			if (!quiet)
			     error (0, 0, "%s, version %s, resurrected",
			            finfo.fullname, vers->vn_user);
		    }
		    Register (entries, finfo.file, vers->vn_user,
                              tmp, vers->options,
			      vers->tag, vers->date, NULL);
		    if (tmp) free (tmp);
#ifdef SERVER_SUPPORT
		    if (server_active)
		    {
			/* If we resurrected the file from the archive, we
			 * need to tell the client about it.
			 */
			server_updated (&finfo, vers,
					SERVER_UPDATED,
					(mode_t) -1, NULL, NULL);
		    }
		   /* We don't increment added_files here because this isn't
		    * a change that needs to be committed.
		    */
#endif
		}
	    }
	    else
	    {
		/* The user file shouldn't be there */
		error (0, 0, "\
%s should be removed and is still there (or is back again)", finfo.fullname);
		err++;
	    }
	}
	else
	{
	    /* A normal entry, ts_rcs is valid, so it must already be there */
	    if (!quiet)
		error (0, 0, "%s already exists, with version number %s",
			finfo.fullname,
			vers->vn_user);
	    err++;
	}
	freevers_ts (&vers);

	/* passed all the checks.  Go ahead and add it if its a directory */
	if (begin_err == err
	    && isdir (finfo.file)
	    && !wrap_name_has (finfo.file, WRAP_TOCVS))
	{
	    err += add_directory (&finfo);
	}
	else
	{
#ifdef SERVER_SUPPORT
	    if (server_active && begin_added_files != added_files)
		server_checked_in (finfo.file, finfo.update_dir, repository);
#endif
	}
	free (repository);
	Entries_Close (entries);

	if (restore_cwd (&cwd, NULL))
	    error_exit ();
	free_cwd (&cwd);

	/* It's okay to discard the const to free this - we allocated this
	 * above.  The const is for everybody else.
	 */
	free ((char *) finfo.fullname);
	free ((char *) filename);
    }
    if (added_files && !really_quiet)
	error (0, 0, "use '%s commit' to add %s permanently",
	       program_name,
	       (added_files == 1) ? "this file" : "these files");

    if (message)
	free (message);
    if (options)
	free (options);

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
add_directory (finfo)
    struct file_info *finfo;
{
    const char *repository = finfo->repository;
    List *entries = finfo->entries;
    const char *dir = finfo->file;

    char *rcsdir = NULL;
    struct saved_cwd cwd;
    char *message = NULL;
    char *tag, *date;
    int nonbranch;
    char *attrs;

    if (strchr (dir, '/') != NULL)
    {
	/* "Can't happen".  */
	error (0, 0,
	       "directory %s not added; must be a direct sub-directory", dir);
	return (1);
    }
    if (fncmp (dir, CVSADM) == 0)
    {
	error (0, 0, "cannot add a `%s' directory", CVSADM);
	return (1);
    }

    /* before we do anything else, see if we have any per-directory tags */
    ParseTag (&tag, &date, &nonbranch);

    /* Remember the default attributes from this directory, so we can apply
       them to the new directory.  */
    fileattr_startdir (repository);
    attrs = fileattr_getall (NULL);
    fileattr_free ();

    /* now, remember where we were, so we can get back */
    if (save_cwd (&cwd))
	return 1;
    if (CVS_CHDIR (dir) < 0)
    {
	error (0, errno, "cannot chdir to %s", finfo->fullname);
	return 1;
    }
#ifdef SERVER_SUPPORT
    if (!server_active && isfile (CVSADM))
#else
    if (isfile (CVSADM))
#endif
    {
	error (0, 0, "%s/%s already exists", finfo->fullname, CVSADM);
	goto out;
    }

    rcsdir = xmalloc (strlen (repository) + strlen (dir) + 5);
    sprintf (rcsdir, "%s/%s", repository, dir);
    if (isfile (rcsdir) && !isdir (rcsdir))
    {
	error (0, 0, "%s is not a directory; %s not added", rcsdir,
	       finfo->fullname);
	goto out;
    }

    /* setup the log message */
    message = xmalloc (strlen (rcsdir)
		       + 80
		       + (tag == NULL ? 0 : strlen (tag) + 80)
		       + (date == NULL ? 0 : strlen (date) + 80));
    (void) sprintf (message, "Directory %s added to the repository\n",
		    rcsdir);
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

	/* Now set the default file attributes to the ones we inherited
	   from the parent directory.  */
	fileattr_startdir (rcsdir);
	fileattr_setall (NULL, attrs);
	fileattr_write ();
	fileattr_free ();
	if (attrs != NULL)
	    free (attrs);

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
	p->data = li;
	(void) addnode (ulist, p);
	Update_Logfile (rcsdir, message, (FILE *) NULL, ulist);
	dellist (&ulist);
    }

#ifdef SERVER_SUPPORT
    if (!server_active)
#endif
        Create_Admin (".", finfo->fullname, rcsdir, tag, date, nonbranch, 0, 1);
    if (tag)
	free (tag);
    if (date)
	free (date);

    if (restore_cwd (&cwd, NULL))
	error_exit ();
    free_cwd (&cwd);

    Subdir_Register (entries, (char *) NULL, dir);

    if (!really_quiet)
	cvs_output (message, 0);

    free (rcsdir);
    free (message);

    return 0;

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
    const char *repository;
    const char *user;
    const char *options;
    const char *message;
    List *entries;
    const char *tag;
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
