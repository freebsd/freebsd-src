/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Create Version
 * 
 * "checkout" creates a "version" of an RCS repository.  This version is owned
 * totally by the user and is actually an independent copy, to be dealt with
 * as seen fit.  Once "checkout" has been called in a given directory, it
 * never needs to be called again.  The user can keep up-to-date by calling
 * "update" when he feels like it; this will supply him with a merge of his
 * own modifications and the changes made in the RCS original.  See "update"
 * for details.
 * 
 * "checkout" can be given a list of directories or files to be updated and in
 * the case of a directory, will recursivley create any sub-directories that
 * exist in the repository.
 * 
 * When the user is satisfied with his own modifications, the present version
 * can be committed by "commit"; this keeps the present version in tact,
 * usually.
 * 
 * The call is cvs checkout [options] <module-name>...
 * 
 * "checkout" creates a directory ./CVS, in which it keeps its administration,
 * in two files, Repository and Entries. The first contains the name of the
 * repository.  The second contains one line for each registered file,
 * consisting of the version number it derives from, its time stamp at
 * derivation time and its name.  Both files are normal files and can be
 * edited by the user, if necessary (when the repository is moved, e.g.)
 */

#include "cvs.h"

static char *findslash PROTO((char *start, char *p));
static int checkout_proc PROTO((int *pargc, char **argv, char *where,
		          char *mwhere, char *mfile, int shorten,
		          int local_specified, char *omodule,
		          char *msg));
static int safe_location PROTO((void));

static const char *const checkout_usage[] =
{
    "Usage:\n  %s %s [-ANPRcflnps] [-r rev | -D date] [-d dir]\n",
    "    [-j rev1] [-j rev2] [-k kopt] modules...\n",
    "\t-A\tReset any sticky tags/date/kopts.\n",
    "\t-N\tDon't shorten module paths if -d specified.\n",
    "\t-P\tPrune empty directories.\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-c\t\"cat\" the module database.\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, not recursive\n",
    "\t-n\tDo not run module program (if any).\n",
    "\t-p\tCheck out files to standard output (avoids stickiness).\n",
    "\t-s\tLike -c, but include module status.\n",
    "\t-r rev\tCheck out revision or tag. (implies -P) (is sticky)\n",
    "\t-D date\tCheck out revisions as of date. (implies -P) (is sticky)\n",
    "\t-d dir\tCheck out into dir instead of module name.\n",
    "\t-k kopt\tUse RCS kopt -k option on checkout.\n",
    "\t-j rev\tMerge in changes made between current revision and rev.\n",
    NULL
};

static const char *const export_usage[] =
{
    "Usage: %s %s [-NRfln] [-r rev | -D date] [-d dir] [-k kopt] module...\n",
    "\t-N\tDon't shorten module paths if -d specified.\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, not recursive\n",
    "\t-R\tProcess directories recursively (default).\n",
    "\t-n\tDo not run module program (if any).\n",
    "\t-r rev\tExport revision or tag.\n",
    "\t-D date\tExport revisions as of date.\n",
    "\t-d dir\tExport into dir instead of module name.\n",
    "\t-k kopt\tUse RCS kopt -k option on checkout.\n",
    NULL
};

static int checkout_prune_dirs;
static int force_tag_match = 1;
static int pipeout;
static int aflag;
static char *options = NULL;
static char *tag = NULL;
static int tag_validated = 0;
static char *date = NULL;
static char *join_rev1 = NULL;
static char *join_rev2 = NULL;
static int join_tags_validated = 0;
static char *preload_update_dir = NULL;
static char *history_name = NULL;

int
checkout (argc, argv)
    int argc;
    char **argv;
{
    int i;
    int c;
    DBM *db;
    int cat = 0, err = 0, status = 0;
    int run_module_prog = 1;
    int local = 0;
    int shorten = -1;
    char *where = NULL;
    char *valid_options;
    const char *const *valid_usage;
    enum mtype m_type;

    /*
     * A smaller subset of options are allowed for the export command, which
     * is essentially like checkout, except that it hard-codes certain
     * options to be default (like -kv) and takes care to remove the CVS
     * directory when it has done its duty
     */
    if (strcmp (command_name, "export") == 0)
    {
        m_type = EXPORT;
	valid_options = "+Nnk:d:flRQqr:D:";
	valid_usage = export_usage;
    }
    else
    {
        m_type = CHECKOUT;
	valid_options = "+ANnk:d:flRpQqcsr:D:j:P";
	valid_usage = checkout_usage;
    }

    if (argc == -1)
	usage (valid_usage);

    ign_setup ();
    wrap_setup ();

    optind = 0;
    while ((c = getopt (argc, argv, valid_options)) != -1)
    {
	switch (c)
	{
	    case 'A':
		aflag = 1;
		break;
	    case 'N':
		shorten = 0;
		break;
	    case 'k':
		if (options)
		    free (options);
		options = RCS_check_kflag (optarg);
		break;
	    case 'n':
		run_module_prog = 0;
		break;
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
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 'P':
		checkout_prune_dirs = 1;
		break;
	    case 'p':
		pipeout = 1;
		run_module_prog = 0;	/* don't run module prog when piping */
		noexec = 1;		/* so no locks will be created */
		break;
	    case 'c':
		cat = 1;
		break;
	    case 'd':
		where = optarg;
		if (shorten == -1)
		    shorten = 1;
		break;
	    case 's':
		status = 1;
		break;
	    case 'f':
		force_tag_match = 0;
		break;
	    case 'r':
		tag = optarg;
		checkout_prune_dirs = 1;
		break;
	    case 'D':
		date = Make_Date (optarg);
		checkout_prune_dirs = 1;
		break;
	    case 'j':
		if (join_rev2)
		    error (1, 0, "only two -j options can be specified");
		if (join_rev1)
		    join_rev2 = optarg;
		else
		    join_rev1 = optarg;
		break;
	    case '?':
	    default:
		usage (valid_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (shorten == -1)
	shorten = 0;

    if ((!(cat + status) && argc == 0) || ((cat + status) && argc != 0))
	usage (valid_usage);

    if (where && pipeout)
	error (1, 0, "-d and -p are mutually exclusive");

    if (strcmp (command_name, "export") == 0)
    {
	if (!tag && !date)
	{
	    error (0, 0, "must specify a tag or date");
	    usage (valid_usage);
	}
	if (tag && isdigit (tag[0]))
	    error (1, 0, "tag `%s' must be a symbolic tag", tag);
/*
 * mhy 950615: -kv doesn't work for binaries with RCS keywords.
 * Instead use the default provided in the RCS file (-ko for binaries).
 */
#if 0
	if (!options)
	  options = RCS_check_kflag ("v");/* -kv is default */
#endif
    }

    if (!safe_location()) {
        error(1, 0, "Cannot check out files into the repository itself");
    }

#ifdef CLIENT_SUPPORT
    if (client_active)
    {
	int expand_modules;

	start_server ();

	ign_setup ();
	
	/* We have to expand names here because the "expand-modules"
           directive to the server has the side-effect of having the
           server send the check-in and update programs for the
           various modules/dirs requested.  If we turn this off and
           simply request the names of the modules and directories (as
           below in !expand_modules), those files (CVS/Checking.prog
           or CVS/Update.prog) don't get created.  Grrr.  */
	
	expand_modules = (!cat && !status && !pipeout
			  && supported_request ("expand-modules"));
	
	if (expand_modules)
	{
	    /* This is done here because we need to read responses
               from the server before we send the command checkout or
               export files. */

	    client_expand_modules (argc, argv, local);
	}

	if (!run_module_prog) send_arg ("-n");
	if (local) send_arg ("-l");
	if (pipeout) send_arg ("-p");
	if (!force_tag_match) send_arg ("-f");
	if (aflag)
	    send_arg("-A");
	if (!shorten)
	    send_arg("-N");
	if (checkout_prune_dirs && strcmp (command_name, "export") != 0)
	    send_arg("-P");
	client_prune_dirs = checkout_prune_dirs;
	if (cat)
	    send_arg("-c");
	if (where != NULL)
	{
	    option_with_arg ("-d", where);
	}
	if (status)
	    send_arg("-s");
	/* Why not send -k for export?  This would appear to make
	   remote export differ from local export.  FIXME.  */
	if (strcmp (command_name, "export") != 0
	    && options != NULL
	    && options[0] != '\0')
	    send_arg (options);
	option_with_arg ("-r", tag);
	if (date)
	    client_senddate (date);
	if (join_rev1 != NULL)
	    option_with_arg ("-j", join_rev1);
	if (join_rev2 != NULL)
	    option_with_arg ("-j", join_rev2);

	if (expand_modules)
	{
	    client_send_expansions (local, where, 1);
	}
	else
	{
	    int i;
	    for (i = 0; i < argc; ++i)
		send_arg (argv[i]);
	    client_nonexpanded_setup ();
	}

	send_to_server (strcmp (command_name, "export") == 0 ?
                        "export\012" : "co\012",
                        0);

	return get_responses_and_close ();
    }
#endif /* CLIENT_SUPPORT */

    if (cat || status)
    {
	cat_module (status);
	return (0);
    }
    db = open_module ();

    /*
     * if we have more than one argument and where was specified, we make the
     * where, cd into it, and try to shorten names as much as possible.
     * Otherwise, we pass the where as a single argument to do_module.
     */
    if (argc > 1 && where != NULL)
    {
	(void) CVS_MKDIR (where, 0777);
	if ( CVS_CHDIR (where) < 0)
	    error (1, errno, "cannot chdir to %s", where);
	preload_update_dir = xstrdup (where);
	where = (char *) NULL;
	if (!isfile (CVSADM))
	{
	    char *repository;

	    repository = xmalloc (strlen (CVSroot_directory) + 80);
	    (void) sprintf (repository, "%s/%s/%s", CVSroot_directory,
			    CVSROOTADM, CVSNULLREPOS);
	    if (!isfile (repository))
	    {
		mode_t omask;
		omask = umask (cvsumask);
		(void) CVS_MKDIR (repository, 0777);
		(void) umask (omask);
	    }

	    /* I'm not sure whether this check is redundant.  */
	    if (!isdir (repository))
		error (1, 0, "there is no repository %s", repository);

	    Create_Admin (".", preload_update_dir, repository,
			  (char *) NULL, (char *) NULL, 0);
	    if (!noexec)
	    {
		FILE *fp;

		fp = open_file (CVSADM_ENTSTAT, "w+");
		if (fclose(fp) == EOF)
		    error(1, errno, "cannot close %s", CVSADM_ENTSTAT);
#ifdef SERVER_SUPPORT
		if (server_active)
		    server_set_entstat (preload_update_dir, repository);
#endif
	    }
	    free (repository);
	}
    }

    /* If we will be calling history_write, work out the name to pass
       it.  */
    if (strcmp (command_name, "export") != 0 && !pipeout)
    {
	if (tag && date)
	{
	    history_name = xmalloc (strlen (tag) + strlen (date) + 2);
	    sprintf (history_name, "%s:%s", tag, date);
	}
	else if (tag)
	    history_name = tag;
	else
	    history_name = date;
    }

    /*
     * if where was specified (-d) and we have not taken care of it already
     * with the multiple arg stuff, and it was not a simple directory name
     * but rather a path, we strip off everything but the last component and
     * attempt to cd to the indicated place.  where then becomes simply the
     * last component
     */
    if (where != NULL && strchr (where, '/') != NULL)
    {
	char *slash;

	slash = strrchr (where, '/');
	*slash = '\0';

	if ( CVS_CHDIR (where) < 0)
	    error (1, errno, "cannot chdir to %s", where);

	preload_update_dir = xstrdup (where);

	where = slash + 1;
	if (*where == '\0')
	    where = NULL;
    }

    for (i = 0; i < argc; i++)
	err += do_module (db, argv[i], m_type, "Updating", checkout_proc,
			  where, shorten, local, run_module_prog,
			  (char *) NULL);
    close_module (db);
    return (err);
}

static int
safe_location ()
{
    char *current;
    char hardpath[PATH_MAX+5];
    size_t hardpath_len;
    int  x;
    int retval;

#ifdef HAVE_READLINK
    /* FIXME-arbitrary limit: should be retrying this like xgetwd.
       But how does readlink let us know that the buffer was too small?
       (by returning sizeof hardpath - 1?).  */
    x = readlink(CVSroot_directory, hardpath, sizeof hardpath - 1);
#else
    x = -1;
#endif
    if (x == -1)
    {
        strcpy(hardpath, CVSroot_directory);
    }
    else
    {
        hardpath[x] = '\0';
    }
    current = xgetwd ();
    hardpath_len = strlen (hardpath);
    if (strlen (current) >= hardpath_len
	&& strncmp (current, hardpath, hardpath_len) == 0)
    {
	if (/* Current is a subdirectory of hardpath.  */
	    current[hardpath_len] == '/'

	    /* Current is hardpath itself.  */
	    || current[hardpath_len] == '\0')
	    retval = 0;
	else
	    /* It isn't a problem.  For example, current is
	       "/foo/cvsroot-bar" and hardpath is "/foo/cvsroot".  */
	    retval = 1;
    }
    else
	retval = 1;
    free (current);
    return retval;
}

struct dir_to_build
{
    /* What to put in CVS/Repository.  */
    char *repository;
    /* The path to the directory.  */
    char *dirpath;

    struct dir_to_build *next;
};

static int build_dirs_and_chdir PROTO ((struct dir_to_build *list,
					int sticky));

static void build_one_dir PROTO ((char *, char *, int));

static void
build_one_dir (repository, dirpath, sticky)
    char *repository;
    char *dirpath;
    int sticky;
{
    FILE *fp;

    if (!isfile (CVSADM) && strcmp (command_name, "export") != 0)
    {
	/* I suspect that this check could be omitted.  */
	if (!isdir (repository))
	    error (1, 0, "there is no repository %s", repository);

	Create_Admin (".", dirpath, repository,
		      sticky ? (char *) NULL : tag,
		      sticky ? (char *) NULL : date,

		      /* FIXME?  This is a guess.  If it is important
			 for nonbranch to be set correctly here I
			 think we need to write it one way now and
			 then rewrite it later via WriteTag, once
			 we've had a chance to call RCS_nodeisbranch
			 on each file.  */
		      0);

	if (!noexec)
	{
	    fp = open_file (CVSADM_ENTSTAT, "w+");
	    if (fclose (fp) == EOF)
		error (1, errno, "cannot close %s", CVSADM_ENTSTAT);
#ifdef SERVER_SUPPORT
	    if (server_active)
		server_set_entstat (dirpath, repository);
#endif
	}
    }
}

/*
 * process_module calls us back here so we do the actual checkout stuff
 */
/* ARGSUSED */
static int
checkout_proc (pargc, argv, where, mwhere, mfile, shorten,
	       local_specified, omodule, msg)
    int *pargc;
    char **argv;
    char *where;
    char *mwhere;
    char *mfile;
    int shorten;
    int local_specified;
    char *omodule;
    char *msg;
{
    int err = 0;
    int which;
    char *cp;
    char *cp2;
    char *repository;
    char *xwhere = NULL;
    char *oldupdate = NULL;
    char *prepath;

    /*
     * OK, so we're doing the checkout! Our args are as follows: 
     *  argc,argv contain either dir or dir followed by a list of files 
     *  where contains where to put it (if supplied by checkout) 
     *  mwhere contains the module name or -d from module file 
     *  mfile says do only that part of the module
     *  shorten = TRUE says shorten as much as possible 
     *  omodule is the original arg to do_module()
     */

    /* set up the repository (maybe) for the bottom directory */
    repository = xmalloc (strlen (CVSroot_directory) + strlen (argv[0]) + 5);
    (void) sprintf (repository, "%s/%s", CVSroot_directory, argv[0]);

    /* save the original value of preload_update_dir */
    if (preload_update_dir != NULL)
	oldupdate = xstrdup (preload_update_dir);

    /* fix up argv[] for the case of partial modules */
    if (mfile != NULL)
    {
	char *file;

	/* if mfile is really a path, straighten it out first */
	if ((cp = strrchr (mfile, '/')) != NULL)
	{
	    *cp = 0;
	    repository = xrealloc (repository,
				   strlen (repository) + strlen (mfile) + 10);
	    (void) strcat (repository, "/");
	    (void) strcat (repository, mfile);

	    /*
	     * Now we need to fill in the where correctly. if !shorten, tack
	     * the rest of the path onto where if where is filled in
	     * otherwise tack the rest of the path onto mwhere and make that
	     * the where
	     * 
	     * If shorten is enabled, we might use mwhere to set where if 
	     * nobody set it yet, so we'll need to setup mwhere as the last
	     * component of the path we are tacking onto repository
	     */
	    if (!shorten)
	    {
		if (where != NULL)
		{
		    xwhere = xmalloc (strlen (where) + strlen (mfile) + 5);
		    (void) sprintf (xwhere, "%s/%s", where, mfile);
		}
		else
		{
		    xwhere = xmalloc (strlen (mwhere) + strlen (mfile) + 5);
		    (void) sprintf (xwhere, "%s/%s", mwhere, mfile);
		}
		where = xwhere;
	    }
	    else
	    {
		char *slash;

		if ((slash = strrchr (mfile, '/')) != NULL)
		    mwhere = slash + 1;
		else
		    mwhere = mfile;
	    }
	    mfile = cp + 1;
	}

	file = xmalloc (strlen (repository) + strlen (mfile) + 5);
	(void) sprintf (file, "%s/%s", repository, mfile);
	if (isdir (file))
	{

	    /*
	     * The portion of a module was a directory, so kludge up where to
	     * be the subdir, and fix up repository
	     */
	    free (repository);
	    repository = xstrdup (file);

	    /*
	     * At this point, if shorten is not enabled, we make where either
	     * where with mfile concatenated, or if where hadn't been set we
	     * set it to mwhere with mfile concatenated.
	     * 
	     * If shorten is enabled and where hasn't been set yet, then where
	     * becomes mfile
	     */
	    if (!shorten)
	    {
		if (where != NULL)
		{
		    xwhere = xmalloc (strlen (where) + strlen (mfile) + 5);
		    (void) sprintf (xwhere, "%s/%s", where, mfile);
		}
		else
		{
		    xwhere = xmalloc (strlen (mwhere) + strlen (mfile) + 5);
		    (void) sprintf (xwhere, "%s/%s", mwhere, mfile);
		}
		where = xwhere;
	    }
	    else if (where == NULL)
		where = mfile;
	}
	else
	{
	    int i;

	    /*
	     * The portion of a module was a file, so kludge up argv to be
	     * correct
	     */
	    for (i = 1; i < *pargc; i++)/* free the old ones */
		free (argv[i]);
	    /* FIXME: Normally one has to realloc argv to make sure
               argv[1] exists.  But this argv does not points to the
               beginning of an allocated block.  For now we allocate
               at least 4 entries for argv (in line2argv).  */
	    argv[1] = xstrdup (mfile);	/* set up the new one */
	    *pargc = 2;

	    /* where gets mwhere if where isn't set */
	    if (where == NULL)
		where = mwhere;
	}
	free (file);
    }

    /*
     * if shorten is enabled and where isn't specified yet, we pluck the last
     * directory component of argv[0] and make it the where
     */
    if (shorten && where == NULL)
    {
	if ((cp = strrchr (argv[0], '/')) != NULL)
	{
	    xwhere = xstrdup (cp + 1);
	    where = xwhere;
	}
    }

    /* if where is still NULL, use mwhere if set or the argv[0] dir */
    if (where == NULL)
    {
	if (mwhere)
	    where = mwhere;
	else
	{
	    xwhere = xstrdup (argv[0]);
	    where = xwhere;
	}
    }

    if (preload_update_dir != NULL)
    {
	preload_update_dir =
	    xrealloc (preload_update_dir,
		      strlen (preload_update_dir) + strlen (where) + 5);
	strcat (preload_update_dir, "/");
	strcat (preload_update_dir, where);
    }
    else
	preload_update_dir = xstrdup (where);

    /*
     * At this point, where is the directory we want to build, repository is
     * the repository for the lowest level of the path.
     */

    /*
     * If we are sending everything to stdout, we can skip a whole bunch of
     * work from here
     */
    if (!pipeout)
    {
	size_t root_len;
	struct dir_to_build *head;

	/* We need to tell build_dirs not only the path we want it to
	   build, but also the repositories we want it to populate the
	   path with. To accomplish this, we walk the path backwards,
	   one pathname component at a time, constucting a linked
	   list of struct dir_to_build.  */
	prepath = xstrdup (repository);

	/* We don't want to start stripping elements off prepath after we
	   get to CVSROOT.  */
	root_len = strlen (CVSroot_directory);
	if (strncmp (repository, CVSroot_directory, root_len) != 0)
	    error (1, 0, "\
internal error: %s doesn't start with %s in checkout_proc",
		   repository, CVSroot_directory);

	/* We always create at least one directory, which corresponds to
	   the entire strings for WHERE and REPOSITORY.  */
	head = (struct dir_to_build *) xmalloc (sizeof (struct dir_to_build));
	/* Special marker to indicate that we don't want build_dirs_and_chdir
	   to create the CVSADM directory for us.  */
	head->repository = NULL;
	head->dirpath = xstrdup (where);
	head->next = NULL;

	cp = strrchr (where, '/');
	cp2 = strrchr (prepath, '/');

	while (cp != NULL)
	{
	    struct dir_to_build *new;
	    new = (struct dir_to_build *)
		xmalloc (sizeof (struct dir_to_build));
	    new->dirpath = xmalloc (strlen (where));
	    strncpy (new->dirpath, where, cp - where);
	    new->dirpath[cp - where] = '\0';
	    if (cp2 == NULL || cp2 < prepath + root_len)
	    {
		/* Don't walk up past CVSROOT; instead put in CVSNULLREPOS.  */
		new->repository =
		    xmalloc (strlen (CVSroot_directory) + 80);
		(void) sprintf (new->repository, "%s/%s/%s",
				CVSroot_directory,
				CVSROOTADM, CVSNULLREPOS);
		if (!isfile (new->repository))
		{
		    mode_t omask;
		    omask = umask (cvsumask);
		    if (CVS_MKDIR (new->repository, 0777) < 0)
			error (0, errno, "cannot create %s",
			       new->repository);
		    (void) umask (omask);
		}
	    }
	    else
	    {
		new->repository = xmalloc (strlen (prepath));
		strncpy (new->repository, prepath, cp2 - prepath);
		new->repository[cp2 - prepath] = '\0';
	    }
	    new->next = head;
	    head = new;

	    cp = findslash (where, cp - 1);
	    cp2 = findslash (prepath, cp2 - 1);
	}

	/* First build the top-level CVSADM directory.  The value we
	   pass in here for repository is probably wrong; see modules3-7f
	   in the testsuite.  */
	build_one_dir (head->repository != NULL ? head->repository : prepath,
		       ".", *pargc <= 1);

	/*
	 * build dirs on the path if necessary and leave us in the bottom
	 * directory (where if where was specified) doesn't contain a CVS
	 * subdir yet, but all the others contain CVS and Entries.Static
	 * files
	 */
	if (build_dirs_and_chdir (head, *pargc <= 1) != 0)
	{
	    error (0, 0, "ignoring module %s", omodule);
	    free (prepath);
	    err = 1;
	    goto out;
	}

	/* clean up */
	free (prepath);

	/* set up the repository (or make sure the old one matches) */
	if (!isfile (CVSADM))
	{
	    FILE *fp;

	    if (!noexec && *pargc > 1)
	    {
		/* I'm not sure whether this check is redundant.  */
		if (!isdir (repository))
		    error (1, 0, "there is no repository %s", repository);

		Create_Admin (".", preload_update_dir, repository,
			      (char *) NULL, (char *) NULL, 0);
		fp = open_file (CVSADM_ENTSTAT, "w+");
		if (fclose(fp) == EOF)
		    error(1, errno, "cannot close %s", CVSADM_ENTSTAT);
#ifdef SERVER_SUPPORT
		if (server_active)
		    server_set_entstat (where, repository);
#endif
	    }
	    else
	    {
		/* I'm not sure whether this check is redundant.  */
		if (!isdir (repository))
		    error (1, 0, "there is no repository %s", repository);

		Create_Admin (".", preload_update_dir, repository, tag, date,

			      /* FIXME?  This is a guess.  If it is important
				 for nonbranch to be set correctly here I
				 think we need to write it one way now and
				 then rewrite it later via WriteTag, once
				 we've had a chance to call RCS_nodeisbranch
				 on each file.  */
			      0);
	    }
	}
	else
	{
	    char *repos;

	    /* get the contents of the previously existing repository */
	    repos = Name_Repository ((char *) NULL, preload_update_dir);
	    if (fncmp (repository, repos) != 0)
	    {
		error (0, 0, "existing repository %s does not match %s",
		       repos, repository);
		error (0, 0, "ignoring module %s", omodule);
		free (repos);
		err = 1;
		goto out;
	    }
	    free (repos);
	}
    }

    /*
     * If we are going to be updating to stdout, we need to cd to the
     * repository directory so the recursion processor can use the current
     * directory as the place to find repository information
     */
    if (pipeout)
    {
	if ( CVS_CHDIR (repository) < 0)
	{
	    error (0, errno, "cannot chdir to %s", repository);
	    err = 1;
	    goto out;
	}
	which = W_REPOS;
	if (tag != NULL && !tag_validated)
	{
	    tag_check_valid (tag, *pargc - 1, argv + 1, 0, aflag, NULL);
	    tag_validated = 1;
	}
    }
    else
    {
	which = W_LOCAL | W_REPOS;
	if (tag != NULL && !tag_validated)
	{
	    tag_check_valid (tag, *pargc - 1, argv + 1, 0, aflag,
			     repository);
	    tag_validated = 1;
	}
    }

    if (tag != NULL || date != NULL || join_rev1 != NULL)
	which |= W_ATTIC;

    if (! join_tags_validated)
    {
        if (join_rev1 != NULL)
	    tag_check_valid_join (join_rev1, *pargc - 1, argv + 1, 0, aflag,
				  repository);
	if (join_rev2 != NULL)
	    tag_check_valid_join (join_rev2, *pargc - 1, argv + 1, 0, aflag,
				  repository);
	join_tags_validated = 1;
    }

    /*
     * if we are going to be recursive (building dirs), go ahead and call the
     * update recursion processor.  We will be recursive unless either local
     * only was specified, or we were passed arguments
     */
    if (!(local_specified || *pargc > 1))
    {
	if (strcmp (command_name, "export") != 0 && !pipeout)
	    history_write ('O', preload_update_dir, history_name, where,
			   repository);
	else if (strcmp (command_name, "export") == 0 && !pipeout)
	    history_write ('E', preload_update_dir, tag ? tag : date, where,
			   repository);
	err += do_update (0, (char **) NULL, options, tag, date,
			  force_tag_match, 0 /* !local */ ,
			  1 /* update -d */ , aflag, checkout_prune_dirs,
			  pipeout, which, join_rev1, join_rev2,
			  preload_update_dir);
	goto out;
    }

    if (!pipeout)
    {
	int i;
	List *entries;

	/* we are only doing files, so register them */
	entries = Entries_Open (0);
	for (i = 1; i < *pargc; i++)
	{
	    char *line;
	    Vers_TS *vers;
	    struct file_info finfo;

	    memset (&finfo, 0, sizeof finfo);
	    finfo.file = argv[i];
	    /* Shouldn't be used, so set to arbitrary value.  */
	    finfo.update_dir = NULL;
	    finfo.fullname = argv[i];
	    finfo.repository = repository;
	    finfo.entries = entries;
	    /* The rcs slot is needed to get the options from the RCS
               file */
	    finfo.rcs = RCS_parse (finfo.file, repository);

	    vers = Version_TS (&finfo, options, tag, date,
			       force_tag_match, 0);
	    if (vers->ts_user == NULL)
	    {
		line = xmalloc (strlen (finfo.file) + 15);
		(void) sprintf (line, "Initial %s", finfo.file);
		Register (entries, finfo.file,
			  vers->vn_rcs ? vers->vn_rcs : "0",
			  line, vers->options, vers->tag,
			  vers->date, (char *) 0);
		free (line);
	    }
	    freevers_ts (&vers);
	    freercsnode (&finfo.rcs);
	}

	Entries_Close (entries);
    }

    /* Don't log "export", just regular "checkouts" */
    if (strcmp (command_name, "export") != 0 && !pipeout)
	history_write ('O', preload_update_dir, history_name, where,
		       repository);

    /* go ahead and call update now that everything is set */
    err += do_update (*pargc - 1, argv + 1, options, tag, date,
		      force_tag_match, local_specified, 1 /* update -d */,
		      aflag, checkout_prune_dirs, pipeout, which, join_rev1,
		      join_rev2, preload_update_dir);
out:
    free (preload_update_dir);
    preload_update_dir = oldupdate;
    if (xwhere != NULL)
	free (xwhere);
    free (repository);
    return (err);
}

static char *
findslash (start, p)
    char *start;
    char *p;
{
    while (p >= start && *p != '/')
	p--;
    /* FIXME: indexing off the start of the array like this is *NOT*
       OK according to ANSI, and will break some of the time on certain
       segmented architectures.  */
    if (p < start)
	return (NULL);
    else
	return (p);
}

/* Build all the dirs along the path to DIRS with CVS subdirs with appropriate
   repositories.  If ->repository is NULL, do not create a CVSADM directory
   for that subdirectory; just CVS_CHDIR into it.  */
static int
build_dirs_and_chdir (dirs, sticky)
    struct dir_to_build *dirs;
    int sticky;
{
    int retval = 0;
    struct dir_to_build *nextdir;

    while (dirs != NULL)
    {
	char *dir = last_component (dirs->dirpath);

	mkdir_if_needed (dir);
	Subdir_Register (NULL, NULL, dir);
	if (CVS_CHDIR (dir) < 0)
	{
	    error (0, errno, "cannot chdir to %s", dir);
	    retval = 1;
	    goto out;
	}
	if (dirs->repository != NULL)
	{
	    build_one_dir (dirs->repository, dirs->dirpath, sticky);
	    free (dirs->repository);
	}
	nextdir = dirs->next;
	free (dirs->dirpath);
	free (dirs);
	dirs = nextdir;
    }

 out:
    return retval;
}
