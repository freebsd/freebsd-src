/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * "update" updates the version in the present directory with respect to the RCS
 * repository.  The present version must have been created by "checkout". The
 * user can keep up-to-date by calling "update" whenever he feels like it.
 * 
 * The present version can be committed by "commit", but this keeps the version
 * in tact.
 * 
 * Arguments following the options are taken to be file names to be updated,
 * rather than updating the entire directory.
 * 
 * Modified or non-existent RCS files are checked out and reported as U
 * <user_file>
 * 
 * Modified user files are reported as M <user_file>.  If both the RCS file and
 * the user file have been modified, the user file is replaced by the result
 * of rcsmerge, and a backup file is written for the user in .#file.version.
 * If this throws up irreconcilable differences, the file is reported as C
 * <user_file>, and as M <user_file> otherwise.
 * 
 * Files added but not yet committed are reported as A <user_file>. Files
 * removed but not yet committed are reported as R <user_file>.
 * 
 * If the current directory contains subdirectories that hold concurrent
 * versions, these are updated too.  If the -d option was specified, new
 * directories added to the repository are automatically created and updated
 * as well.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)update.c 1.83 92/04/10";
#endif

#if __STDC__
static int checkout_file (char *file, char *repository, List *entries,
			  List *srcfiles, Vers_TS *vers_ts, char *update_dir);
static int isemptydir (char *dir);
static int merge_file (char *file, char *repository, List *entries,
		       Vers_TS *vers, char *update_dir);
static int scratch_file (char *file, char *repository, List * entries,
			 char *update_dir);
static Dtype update_dirent_proc (char *dir, char *repository, char *update_dir);
static int update_dirleave_proc (char *dir, int err, char *update_dir);
static int update_file_proc (char *file, char *update_dir, char *repository,
			     List * entries, List * srcfiles);
static int update_filesdone_proc (int err, char *repository, char *update_dir);
static int write_letter (char *file, int letter, char *update_dir);
static void ignore_files (List * ilist, char *update_dir);
static void join_file (char *file, List *srcfiles, Vers_TS *vers_ts,
		       char *update_dir);
#else
static int update_file_proc ();
static int update_filesdone_proc ();
static Dtype update_dirent_proc ();
static int update_dirleave_proc ();
static int isemptydir ();
static int scratch_file ();
static int checkout_file ();
static int write_letter ();
static int merge_file ();
static void ignore_files ();
static void join_file ();
#endif				/* __STDC__ */

static char *options = NULL;
static char *tag = NULL;
static char *date = NULL;
static char *join_rev1, *date_rev1;
static char *join_rev2, *date_rev2;
static char *K_flag;
static int aflag = 0;
static int force_tag_match = 1;
static int update_build_dirs = 0;
static int update_prune_dirs = 0;
static int pipeout = 0;
static List *ignlist = (List *) NULL;

static char *update_usage[] =
{
    "Usage:\n %s %s [-APQdflRpq] [-k kopt] [-r rev|-D date] [-j rev] [-I ign] [files...]\n",
    "\t-A\tReset any sticky tags/date/kopts.\n",
    "\t-P\tPrune empty directories.\n",
    "\t-Q\tReally quiet.\n",
    "\t-d\tBuild directories, like checkout does.\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, no recursion.\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-p\tSend updates to standard output.\n",
    "\t-q\tSomewhat quiet.\n",
    "\t-k kopt\tUse RCS kopt -k option on checkout.\n",
    "\t-r rev\tUpdate using specified revision/tag.\n",
    "\t-D date\tSet date to update from.\n",
    "\t-j rev\tMerge in changes made between current revision and rev.\n",
    "\t-I ign\tMore files to ignore (! to reset).\n",
    "\t-K key\tUse RCS key -K option on checkout.\n",
    NULL
};

/*
 * update is the argv,argc based front end for arg parsing
 */
int
update (argc, argv)
    int argc;
    char *argv[];
{
    int c, err;
    int local = 0;			/* recursive by default */
    int which;				/* where to look for files and dirs */

    if (argc == -1)
	usage (update_usage);

    ign_setup ();

    /* parse the args */
    optind = 1;
    while ((c = gnu_getopt (argc, argv, "ApPflRQqdk:r:D:j:I:K:")) != -1)
    {
	switch (c)
	{
	    case 'A':
		aflag = 1;
		break;
	    case 'I':
		ign_add (optarg, 0);
		break;
	    case 'k':
		if (options)
		    free (options);
		options = RCS_check_kflag (optarg);
		break;
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 'Q':
		really_quiet = 1;
		/* FALL THROUGH */
	    case 'q':
		quiet = 1;
		break;
	    case 'd':
		update_build_dirs = 1;
		break;
	    case 'f':
		force_tag_match = 0;
		break;
	    case 'r':
		tag = optarg;
		break;
	    case 'D':
		date = Make_Date (optarg);
		break;
	    case 'P':
		update_prune_dirs = 1;
		break;
	    case 'p':
		pipeout = 1;
		noexec = 1;		/* so no locks will be created */
		break;
	    case 'j':
		if (join_rev2)
		    error (1, 0, "only two -j options can be specified");
		if (join_rev1)
		    join_rev2 = optarg;
		else
		    join_rev1 = optarg;
		break;
	    case 'K':
		K_flag = optarg;
		break;
	    case '?':
	    default:
		usage (update_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

#ifdef FREEBSD_DEVELOPER
    if (!K_flag && freebsd) {
	/* XXX Note:  The leading -K is not needed, it gets added later! */
	K_flag = "eAuthor,eDate,eHeader,eId,eLocker,eLog,eRCSfile,eRevision,eSource,eState,iFreeBSD";
    }
#endif /* FREEBSD_DEVELOPER */

    /*
     * If we are updating the entire directory (for real) and building dirs
     * as we go, we make sure there is no static entries file and write the
     * tag file as appropriate
     */
    if (argc <= 0 && !pipeout)
    {
	if (update_build_dirs)
	    (void) unlink_file (CVSADM_ENTSTAT);

	/* keep the CVS/Tag file current with the specified arguments */
	if (aflag || tag || date)
	    WriteTag ((char *) NULL, tag, date);
    }

    /* look for files/dirs locally and in the repository */
    which = W_LOCAL | W_REPOS;

    /* look in the attic too if a tag or date is specified */
    if (tag != NULL || date != NULL)
	which |= W_ATTIC;

    /* call the command line interface */
    err = do_update (argc, argv, options, tag, date, force_tag_match,
		     local, update_build_dirs, aflag, update_prune_dirs,
		     pipeout, which, join_rev1, join_rev2,
		     K_flag, (char *) NULL);

    /* free the space Make_Date allocated if necessary */
    if (date != NULL)
	free (date);

    return (err);
}

/*
 * Command line interface to update (used by checkout)
 */
int
do_update (argc, argv, xoptions, xtag, xdate, xforce, local, xbuild, xaflag,
	   xprune, xpipeout, which, xjoin_rev1, xjoin_rev2,
	   xK_flag, preload_update_dir)
    int argc;
    char *argv[];
    char *xoptions;
    char *xtag;
    char *xdate;
    int xforce;
    int local;
    int xbuild;
    int xaflag;
    int xprune;
    int xpipeout;
    int which;
    char *xjoin_rev1;
    char *xjoin_rev2;
    char *xK_flag;
    char *preload_update_dir;
{
    int err = 0;
    char *cp;

    /* fill in the statics */
    options = xoptions;
    tag = xtag;
    date = xdate;
    force_tag_match = xforce;
    update_build_dirs = xbuild;
    aflag = xaflag;
    update_prune_dirs = xprune;
    pipeout = xpipeout;

    K_flag = xK_flag;

    /* setup the join support */
    join_rev1 = xjoin_rev1;
    join_rev2 = xjoin_rev2;
    if (join_rev1 && (cp = index (join_rev1, ':')) != NULL)
    {
	*cp++ = '\0';
	date_rev1 = Make_Date (cp);
    }
    else
	date_rev1 = (char *) NULL;
    if (join_rev2 && (cp = index (join_rev2, ':')) != NULL)
    {
	*cp++ = '\0';
	date_rev2 = Make_Date (cp);
    }
    else
	date_rev2 = (char *) NULL;

    /* call the recursion processor */
    err = start_recursion (update_file_proc, update_filesdone_proc,
			   update_dirent_proc, update_dirleave_proc,
			   argc, argv, local, which, aflag, 1,
			   preload_update_dir, 1);
    return (err);
}

/*
 * This is the callback proc for update.  It is called for each file in each
 * directory by the recursion code.  The current directory is the local
 * instantiation.  file is the file name we are to operate on. update_dir is
 * set to the path relative to where we started (for pretty printing).
 * repository is the repository. entries and srcfiles are the pre-parsed
 * entries and source control files.
 * 
 * This routine decides what needs to be done for each file and does the
 * appropriate magic for checkout
 */
static int
update_file_proc (file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List *entries;
    List *srcfiles;
{
    int retval;
    Ctype status;
    Vers_TS *vers;

    status = Classify_File (file, tag, date, options, force_tag_match,
			    aflag, repository, entries, srcfiles, &vers);
    if (pipeout)
    {
	/*
	 * We just return success without doing anything if any of the really
	 * funky cases occur
	 * 
	 * If there is still a valid RCS file, do a regular checkout type
	 * operation
	 */
	switch (status)
	{
	    case T_UNKNOWN:		/* unknown file was explicitly asked
					 * about */
	    case T_REMOVE_ENTRY:	/* needs to be un-registered */
	    case T_ADDED:		/* added but not committed */
		retval = 0;
		break;
	    case T_CONFLICT:		/* old punt-type errors */
		retval = 1;
		break;
	    case T_UPTODATE:		/* file was already up-to-date */
	    case T_NEEDS_MERGE:		/* needs merging */
	    case T_MODIFIED:		/* locally modified */
	    case T_REMOVED:		/* removed but not committed */
	    case T_CHECKOUT:		/* needs checkout */
		retval = checkout_file (file, repository, entries, srcfiles,
					vers, update_dir);
		break;

	    default:			/* can't ever happen :-) */
		error (0, 0,
		       "unknown file status %d for file %s", status, file);
		retval = 0;
		break;
	}
    }
    else
    {
	switch (status)
	{
	    case T_UNKNOWN:		/* unknown file was explicitly asked
					 * about */
	    case T_UPTODATE:		/* file was already up-to-date */
		retval = 0;
		break;
	    case T_CONFLICT:		/* old punt-type errors */
		retval = 1;
		break;
	    case T_NEEDS_MERGE:	/* needs merging */
		retval = merge_file (file, repository, entries,
				     vers, update_dir);
		break;
	    case T_MODIFIED:		/* locally modified */
		retval = write_letter (file, 'M', update_dir);
		break;
	    case T_CHECKOUT:		/* needs checkout */
		retval = checkout_file (file, repository, entries, srcfiles,
					vers, update_dir);
		break;
	    case T_ADDED:		/* added but not committed */
		retval = write_letter (file, 'A', update_dir);
		break;
	    case T_REMOVED:		/* removed but not committed */
		retval = write_letter (file, 'R', update_dir);
		break;
	    case T_REMOVE_ENTRY:	/* needs to be un-registered */
		retval = scratch_file (file, repository, entries, update_dir);
		break;
	    default:			/* can't ever happen :-) */
		error (0, 0,
		       "unknown file status %d for file %s", status, file);
		retval = 0;
		break;
	}
    }

    /* only try to join if things have gone well thus far */
    if (retval == 0 && join_rev1)
	join_file (file, srcfiles, vers, update_dir);

    /* if this directory has an ignore list, add this file to it */
    if (ignlist)
    {
	Node *p;

	p = getnode ();
	p->type = FILES;
	p->key = xstrdup (file);
	(void) addnode (ignlist, p);
    }

    freevers_ts (&vers);
    return (retval);
}

/*
 * update_filesdone_proc () is used
 */
/* ARGSUSED */
static int
update_filesdone_proc (err, repository, update_dir)
    int err;
    char *repository;
    char *update_dir;
{
    /* if this directory has an ignore list, process it then free it */
    if (ignlist)
    {
	ignore_files (ignlist, update_dir);
	dellist (&ignlist);
    }

    /* Clean up CVS admin dirs if we are export */
    if (strcmp (command_name, "export") == 0)
    {
	run_setup ("%s -fr", RM);
	run_arg (CVSADM);
	(void) run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
    }

#ifdef DO_LINKS
    {
	char lnfile[PATH_MAX];
	FILE *links;

	sprintf(lnfile, "%s/SymLinks", repository);
	links = fopen(lnfile, "r");
	if (links) {
	    char from[PATH_MAX], to[PATH_MAX];

	    /* Read all the link pairs from the symlinks file */
	    while (fgets(to, PATH_MAX, links)) {
		fgets(from, PATH_MAX, links);

		/* Strip off the newlines */
		to[strlen(to) - 1] = '\0';
		from[strlen(from) - 1] = '\0';

		/* Do it */
		if (symlink(from, to) == -1) {
		    error (0, errno, "Unable to create symlink `%s'", to);
		    return 1;
		}
		else if (!quiet)
		    error (0, 0, "Creating symlink %s", to);
	    }
	    fclose(links);
	}
    }
#endif

    return (err);
}

/*
 * update_dirent_proc () is called back by the recursion processor before a
 * sub-directory is processed for update.  In this case, update_dirent proc
 * will probably create the directory unless -d isn't specified and this is a
 * new directory.  A return code of 0 indicates the directory should be
 * processed by the recursion code.  A return of non-zero indicates the
 * recursion code should skip this directory.
 */
static Dtype
update_dirent_proc (dir, repository, update_dir)
    char *dir;
    char *repository;
    char *update_dir;
{
    if (!isdir (dir))
    {
	/* if we aren't building dirs, blow it off */
	if (!update_build_dirs)
	    return (R_SKIP_ALL);

	if (noexec)
	{
	    /* ignore the missing dir if -n is specified */
	    error (0, 0, "New directory `%s' -- ignored", dir);
	    return (R_SKIP_ALL);
	}
	else
	{
	    /* otherwise, create the dir and appropriate adm files */
	    make_directory (dir);
	    Create_Admin (dir, repository, tag, date);
	}
    }

    /*
     * If we are building dirs and not going to stdout, we make sure there is
     * no static entries file and write the tag file as appropriate
     */
    if (!pipeout)
    {
	if (update_build_dirs)
	{
	    char tmp[PATH_MAX];

	    (void) sprintf (tmp, "%s/%s", dir, CVSADM_ENTSTAT);
	    (void) unlink_file (tmp);
	}

	/* keep the CVS/Tag file current with the specified arguments */
	if (aflag || tag || date)
	    WriteTag (dir, tag, date);

	/* initialize the ignore list for this directory */
	ignlist = getlist ();
    }

    /* print the warm fuzzy message */
    if (!quiet)
	error (0, 0, "Updating %s", update_dir);

    return (R_PROCESS);
}

/*
 * update_dirleave_proc () is called back by the recursion code upon leaving
 * a directory.  It will prune empty directories if needed and will execute
 * any appropriate update programs.
 */
/* ARGSUSED */
static int
update_dirleave_proc (dir, err, update_dir)
    char *dir;
    int err;
    char *update_dir;
{
    FILE *fp;

    /* run the update_prog if there is one */
    if (err == 0 && !pipeout && !noexec &&
	(fp = fopen (CVSADM_UPROG, "r")) != NULL)
    {
	char *cp;
	char *repository;
	char line[MAXLINELEN];

	repository = Name_Repository ((char *) NULL, update_dir);
	if (fgets (line, sizeof (line), fp) != NULL)
	{
	    if ((cp = rindex (line, '\n')) != NULL)
		*cp = '\0';
	    run_setup ("%s %s", line, repository);
	    (void) printf ("%s %s: Executing '", program_name, command_name);
	    run_print (stdout);
	    (void) printf ("'\n");
	    (void) run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
	}
	(void) fclose (fp);
	free (repository);
    }

    /* Clean up CVS admin dirs if we are export */
    if (strcmp (command_name, "export") == 0)
    {
	run_setup ("%s -fr", RM);
	run_arg (CVSADM);
	(void) run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
    }

    /* Prune empty dirs on the way out - if necessary */
    (void) chdir ("..");
    if (update_prune_dirs && isemptydir (dir))
    {
	run_setup ("%s -fr", RM);
	run_arg (dir);
	(void) run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
    }

    return (err);
}

/*
 * Returns 1 if the argument directory is completely empty, other than the
 * existence of the CVS directory entry.  Zero otherwise.
 */
static int
isemptydir (dir)
    char *dir;
{
    DIR *dirp;
    struct direct *dp;

    if ((dirp = opendir (dir)) == NULL)
    {
	error (0, 0, "cannot open directory %s for empty check", dir);
	return (0);
    }
    while ((dp = readdir (dirp)) != NULL)
    {
	if (strcmp (dp->d_name, ".") != 0 && strcmp (dp->d_name, "..") != 0 &&
	    strcmp (dp->d_name, CVSADM) != 0 &&
	    strcmp (dp->d_name, OCVSADM) != 0)
	{
	    (void) closedir (dirp);
	    return (0);
	}
    }
    (void) closedir (dirp);
    return (1);
}

/*
 * scratch the Entries file entry associated with a file
 */
static int
scratch_file (file, repository, entries, update_dir)
    char *file;
    char *repository;
    List *entries;
    char *update_dir;
{
    history_write ('W', update_dir, "", file, repository);
    Scratch_Entry (entries, file);
    (void) unlink_file (file);
    return (0);
}

/*
 * check out a file - essentially returns the result of the fork on "co".
 */
static int
checkout_file (file, repository, entries, srcfiles, vers_ts, update_dir)
    char *file;
    char *repository;
    List *entries;
    List *srcfiles;
    Vers_TS *vers_ts;
    char *update_dir;
{
    char backup[PATH_MAX];
    int set_time, retval = 0;
    int retcode = 0;

    /* don't screw with backup files if we're going to stdout */
    if (!pipeout)
    {
	(void) sprintf (backup, "%s/%s%s", CVSADM, CVSPREFIX, file);
	if (isfile (file))
	    rename_file (file, backup);
	else
	    (void) unlink_file (backup);
    }

    run_setup ("%s%s -q -r%s %s %s%s", Rcsbin, RCS_CO, vers_ts->vn_rcs,
	       vers_ts->options, K_flag ? "-K" : "", K_flag ? K_flag : "");

    /*
     * if we are checking out to stdout, print a nice message to stderr, and
     * add the -p flag to the command
     */
    if (pipeout)
    {
	run_arg ("-p");
	if (!quiet)
	{
	    (void) fprintf (stderr, "===================================================================\n");
	    if (update_dir[0])
		(void) fprintf (stderr, "Checking out %s/%s\n",
				update_dir, file);
	    else
		(void) fprintf (stderr, "Checking out %s\n", file);
	    (void) fprintf (stderr, "RCS:  %s\n", vers_ts->srcfile->path);
	    (void) fprintf (stderr, "VERS: %s\n", vers_ts->vn_rcs);
	    (void) fprintf (stderr, "***************\n");
	}
    }

    /* tack on the rcs and maybe the user file */
    run_arg (vers_ts->srcfile->path);
    if (!pipeout)
	run_arg (file);

    if ((retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY,
        (pipeout ? (RUN_NORMAL|RUN_REALLY) : RUN_NORMAL))) == 0)
    {
	if (!pipeout)
	{
	    Vers_TS *xvers_ts;

	    if (cvswrite == TRUE)
		xchmod (file, 1);

	    /* set the time from the RCS file iff it was unknown before */
	    if (vers_ts->vn_user == NULL ||
		strncmp (vers_ts->ts_rcs, "Initial", 7) == 0)
	    {
		set_time = 1;
	    }
	    else
		set_time = 0;

	    xvers_ts = Version_TS (repository, options, tag, date, file,
			      force_tag_match, set_time, entries, srcfiles);
	    if (strcmp (xvers_ts->options, "-V4") == 0)
		xvers_ts->options[0] = '\0';
	    Register (entries, file, xvers_ts->vn_rcs, xvers_ts->ts_user,
		      xvers_ts->options, xvers_ts->tag, xvers_ts->date);

	    /* fix up the vers structure, in case it is used by join */
	    if (join_rev1)
	    {
		if (vers_ts->vn_user != NULL)
		    free (vers_ts->vn_user);
		if (vers_ts->vn_rcs != NULL)
		    free (vers_ts->vn_rcs);
		vers_ts->vn_user = xstrdup (xvers_ts->vn_rcs);
		vers_ts->vn_rcs = xstrdup (xvers_ts->vn_rcs);
	    }

	    /* If this is really Update and not Checkout, recode history */
	    if (strcmp (command_name, "update") == 0)
		history_write ('U', update_dir, xvers_ts->vn_rcs, file,
			       repository);

	    freevers_ts (&xvers_ts);

	    if (!really_quiet)
	    {
		if (update_dir[0])
		    (void) printf ("U %s/%s\n", update_dir, file);
		else
		    (void) printf ("U %s\n", file);
	    }
	}
    }
    else
    {
	int old_errno = errno;		/* save errno value over the rename */

	if (!pipeout && isfile (backup))
	    rename_file (backup, file);

	error (retcode == -1 ? 1 : 0, retcode == -1 ? old_errno : 0,
	       "could not check out %s", file);

	retval = retcode;
    }

    if (!pipeout)
	(void) unlink_file (backup);

    return (retval);
}

/*
 * Several of the types we process only print a bit of information consisting
 * of a single letter and the name.
 */
static int
write_letter (file, letter, update_dir)
    char *file;
    char letter;
    char *update_dir;
{
    if (!really_quiet)
    {
	if (update_dir[0])
	    (void) printf ("%c %s/%s\n", letter, update_dir, file);
	else
	    (void) printf ("%c %s\n", letter, file);
    }
    return (0);
}

/*
 * Do all the magic associated with a file which needs to be merged
 */
static int
merge_file (file, repository, entries, vers, update_dir)
    char *file;
    char *repository;
    List *entries;
    Vers_TS *vers;
    char *update_dir;
{
    char user[PATH_MAX];
    char backup[PATH_MAX];
    int status;
    int retcode = 0;

    /*
     * The users currently modified file is moved to a backup file name
     * ".#filename.version", so that it will stay around for a few days
     * before being automatically removed by some cron daemon.  The "version"
     * is the version of the file that the user was most up-to-date with
     * before the merge.
     */
    (void) sprintf (backup, "%s%s.%s", BAKPREFIX, file, vers->vn_user);
    if (update_dir[0])
	(void) sprintf (user, "%s/%s", update_dir, file);
    else
	(void) strcpy (user, file);

    (void) unlink_file (backup);
    copy_file (file, backup);
    xchmod (file, 1);

    /* XXX - Do merge by hand instead of using rcsmerge, due to -k handling */
    run_setup ("%s%s %s -r%s -r%s", Rcsbin, RCS_RCSMERGE, vers->options,
	       vers->vn_user, vers->vn_rcs);
    run_arg (vers->srcfile->path);
    status = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
    if (status != 0
#ifdef HAVE_RCS5
	&& status != 1
#endif
	)
    {
	error (0, status == -1 ? errno : 0,
	       "could not merge revision %s of %s", vers->vn_user, user);
	error (status == -1 ? 1 : 0, 0, "restoring %s from backup file %s",
	       user, backup);
	rename_file (backup, file);
	return (1);
    }
    /* XXX - Might want to make sure that rcsmerge changed the file */
    if (strcmp (vers->options, "-V4") == 0)
	vers->options[0] = '\0';
    Register (entries, file, vers->vn_rcs, vers->ts_rcs, vers->options,
	      vers->tag, vers->date);

    /* fix up the vers structure, in case it is used by join */
    if (join_rev1)
    {
	if (vers->vn_user != NULL)
	    free (vers->vn_user);
	vers->vn_user = xstrdup (vers->vn_rcs);
    }

    /* possibly run GREP to see if there appear to be conflicts in the file */
    run_setup ("%s -s", GREP);
    run_arg (RCS_MERGE_PAT);
    run_arg (file);
    if (status == 1 ||
	(retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL)) == 0)
    {
	if (!noexec)
	    error (0, 0, "conflicts found in %s", user);

	if (!really_quiet)
	    (void) printf ("C %s\n", user);

	history_write ('C', update_dir, vers->vn_rcs, file, repository);

    }
    else if (retcode == -1)
    {
	error (1, errno, "fork failed while examining update of %s", user);
    }
    else
    {
	if (!really_quiet)
	    (void) printf ("M %s\n", user);
	history_write ('G', update_dir, vers->vn_rcs, file, repository);
    }
    return (0);
}

/*
 * Do all the magic associated with a file which needs to be joined
 * (-j option)
 */
static void
join_file (file, srcfiles, vers, update_dir)
    char *file;
    List *srcfiles;
    Vers_TS *vers;
    char *update_dir;
{
    char user[PATH_MAX];
    char backup[PATH_MAX];
    char *rev, *baserev;
    char *options;
    int status;

    /* determine if we need to do anything at all */
    if (vers->vn_user == NULL || vers->srcfile == NULL ||
	vers->srcfile->path == NULL)
    {
	return;
    }

    /* special handling when two revisions are specified */
    if (join_rev1 && join_rev2)
    {
	rev = RCS_getversion (vers->srcfile, join_rev2, date_rev2, 1);
	if (rev == NULL)
	{
	    if (!quiet && date_rev2 == NULL)
		error (0, 0,
		       "cannot find revision %s in file %s", join_rev2, file);
	    return;
	}

	baserev = RCS_getversion (vers->srcfile, join_rev1, date_rev1, 1);
	if (baserev == NULL)
	{
	    if (!quiet && date_rev1 == NULL)
		error (0, 0,
		       "cannot find revision %s in file %s", join_rev1, file);
	    free (rev);
	    return;
	}

	/*
	 * nothing to do if:
	 *	second revision matches our BASE revision (vn_user) &&
	 *	both revisions are on the same branch
	 */
	if (strcmp (vers->vn_user, rev) == 0 &&
	    numdots (baserev) == numdots (rev))
	{
	    /* might be the same branch.  take a real look */
	    char *dot = rindex (baserev, '.');
	    int len = (dot - baserev) + 1;

	    if (strncmp (baserev, rev, len) == 0)
		return;
	}
    }
    else
    {
	rev = RCS_getversion (vers->srcfile, join_rev1, date_rev1, 1);
	if (rev == NULL)
	    return;
	if (strcmp (rev, vers->vn_user) == 0) /* no merge necessary */
	{
	    free (rev);
	    return;
	}

	baserev = RCS_whatbranch (file, join_rev1, srcfiles);
	if (baserev)
	{
	    char *cp;

	    /* we get a branch -- turn it into a revision, or NULL if trunk */
	    if ((cp = rindex (baserev, '.')) == NULL)
	    {
		free (baserev);
		baserev = (char *) NULL;
	    }
	    else
		*cp = '\0';
	}
    }
    if (baserev && strcmp (baserev, rev) == 0)
    {
	/* they match -> nothing to do */
	free (rev);
	free (baserev);
	return;
    }

    /* OK, so we have a revision and possibly a base revision; continue on */
    
    /*
     * The users currently modified file is moved to a backup file name
     * ".#filename.version", so that it will stay around for a few days
     * before being automatically removed by some cron daemon.  The "version"
     * is the version of the file that the user was most up-to-date with
     * before the merge.
     */
    (void) sprintf (backup, "%s%s.%s", BAKPREFIX, file, vers->vn_user);
    if (update_dir[0])
	(void) sprintf (user, "%s/%s", update_dir, file);
    else
	(void) strcpy (user, file);

    (void) unlink_file (backup);
    copy_file (file, backup);
    xchmod (file, 1);

    options = vers->options;
#ifdef HAVE_RCS5
    if (*options == '\0')
	options = "-kk";		/* to ignore keyword expansions */
#endif

    /* XXX - Do merge by hand instead of using rcsmerge, due to -k handling */
    run_setup ("%s%s %s %s%s -r%s", Rcsbin, RCS_RCSMERGE, options,
	       baserev ? "-r" : "", baserev ? baserev : "", rev);
    run_arg (vers->srcfile->path);
    status = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
    if (status != 0
#ifdef HAVE_RCS5
	&& status != 1
#endif
	)
    {
	error (0, status == -1 ? errno : 0,
	       "could not merge revision %s of %s", rev, user);
	error (status == -1 ? 1 : 0, 0, "restoring %s from backup file %s",
	       user, backup);
	rename_file (backup, file);
    }
    free (rev);
    if (baserev)
	free (baserev);
    return;
}

/*
 * Process the current directory, looking for files not in ILIST and not on
 * the global ignore list for this directory.
 */
static void
ignore_files (ilist, update_dir)
    List *ilist;
    char *update_dir;
{
    DIR *dirp;
    struct direct *dp;
    struct stat sb;
    char *file;
    char *xdir;

    /* we get called with update_dir set to "." sometimes... strip it */
    if (strcmp (update_dir, ".") == 0)
	xdir = "";
    else
	xdir = update_dir;

    dirp = opendir (".");
    if (dirp == NULL)
	return;

    ign_add_file (CVSDOTIGNORE, 1);
    while ((dp = readdir (dirp)) != NULL)
    {
	file = dp->d_name;
	if (strcmp (file, ".") == 0 || strcmp (file, "..") == 0)
	    continue;
	if (findnode (ilist, file) != NULL)
	    continue;
	if (lstat (file, &sb) != -1)
	{
	    if (S_ISDIR (sb.st_mode))
		continue;
#ifdef S_IFLNK
	    if (S_ISLNK (sb.st_mode))
		continue;
#endif
	}
	if (ign_name (file))
	    continue;
	(void) write_letter (file, '?', xdir);
    }
    (void) closedir (dirp);
}
