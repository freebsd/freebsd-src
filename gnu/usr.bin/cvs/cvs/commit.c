/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * Commit Files
 * 
 * "commit" commits the present version to the RCS repository, AFTER
 * having done a test on conflicts.
 *
 * The call is: cvs commit [options] files...
 * 
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)commit.c 1.84 92/03/31";
#endif

#if __STDC__
static Dtype check_direntproc (char *dir, char *repos, char *update_dir);
static int check_fileproc (char *file, char *update_dir, char *repository,
			   List * entries, List * srcfiles);
static int check_filesdoneproc (int err, char *repos, char *update_dir);
static int checkaddfile (char *file, char *repository, char *tag);
static Dtype commit_direntproc (char *dir, char *repos, char *update_dir);
static int commit_dirleaveproc (char *dir, int err, char *update_dir);
static int commit_fileproc (char *file, char *update_dir, char *repository,
			    List * entries, List * srcfiles);
static int commit_filesdoneproc (int err, char *repository, char *update_dir);
static int finaladd (char *file, char *revision, char *tag, char *repository,
		     List *entries);
static int findmaxrev (Node * p);
static int fsortcmp (Node * p, Node * q);
static int lock_RCS (char *user, char *rcs, char *rev, char *repository);
static int lock_filesdoneproc (int err, char *repository, char *update_dir);
static int lockrcsfile (char *file, char *repository, char *rev);
static int precommit_list_proc (Node * p);
static int precommit_proc (char *repository, char *filter);
static int remove_file (char *file, char *repository, char *tag,
			List *entries);
static void fix_rcs_modes (char *rcs, char *user);
static void fixaddfile (char *file, char *repository);
static void fixbranch (char *file, char *repository, char *branch);
static void unlockrcs (char *file, char *repository);
static void ci_delproc (Node *p);
static void locate_rcs (char *file, char *repository, char *rcs);
#else
static int fsortcmp ();
static int lock_filesdoneproc ();
static int check_fileproc ();
static Dtype check_direntproc ();
static int precommit_list_proc ();
static int precommit_proc ();
static int check_filesdoneproc ();
static int commit_fileproc ();
static int commit_filesdoneproc ();
static Dtype commit_direntproc ();
static int commit_dirleaveproc ();
static int findmaxrev ();
static int remove_file ();
static int finaladd ();
static void unlockrcs ();
static void fixaddfile ();
static void fixbranch ();
static int checkaddfile ();
static int lockrcsfile ();
static int lock_RCS ();
static void fix_rcs_modes ();
static void ci_delproc ();
static void locate_rcs ();
#endif				/* __STDC__ */

struct commit_info
{
    Ctype status;			/* as returned from Classify_File() */
    char *rev;				/* a numeric rev, if we know it */
    char *tag;				/* any sticky tag, or -r option */
};
struct master_lists
{
    List *ulist;			/* list for Update_Logfile */
    List *cilist;			/* list with commit_info structs */
};

static int got_message;
static int run_module_prog = 1;
static int aflag;
static char *tag;
static char *write_dirtag;
static char *logfile;
static List *mulist;
static List *locklist;
static char *message;

static char *commit_usage[] =
{
    "Usage: %s %s [-nRl] [-m msg | -f logfile] [-r rev] files...\n",
    "\t-n\tDo not run the module program (if any).\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-l\tLocal directory only (not recursive).\n",
    "\t-f file\tRead the log message from file.\n",
    "\t-m msg\tLog message.\n",
    "\t-r rev\tCommit to this branch or trunk revision.\n",
    NULL
};

int
commit (argc, argv)
    int argc;
    char *argv[];
{
    int c;
    int err = 0;
    int local = 0;

    if (argc == -1)
	usage (commit_usage);

#ifdef CVS_BADROOT
    /*
     * For log purposes, do not allow "root" to commit files.  If you look
     * like root, but are really logged in as a non-root user, it's OK.
     */
    if (geteuid () == (uid_t) 0)
    {
	struct passwd *pw;

	if ((pw = (struct passwd *) getpwnam (getcaller ())) == NULL)
	    error (1, 0, "you are unknown to this system");
	if (pw->pw_uid == (uid_t) 0)
	    error (1, 0, "cannot commit files as 'root'");
    }
#endif /* CVS_BADROOT */

    message = xmalloc (MAXMESGLEN + 1);
    message[0] = '\0';			/* Null message by default */
    optind = 1;
    while ((c = gnu_getopt (argc, argv, "nlRm:f:r:")) != -1)
    {
	switch (c)
	{
	    case 'n':
		run_module_prog = 0;
		break;
	    case 'm':
#ifdef FORCE_USE_EDITOR
		use_editor = TRUE;
#else
		use_editor = FALSE;
#endif
		if (strlen (optarg) >= (size_t) MAXMESGLEN)
		{
		    error (0, 0, "warning: message too long; truncated!");
		    (void) strncpy (message, optarg, MAXMESGLEN);
		    message[MAXMESGLEN] = '\0';
		}
		else
		    (void) strcpy (message, optarg);
		break;
	    case 'r':
		if (tag)
		    free (tag);
		tag = xstrdup (optarg);
		break;
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 'f':
#ifdef FORCE_USE_EDITOR
		use_editor = TRUE;
#else
		use_editor = FALSE;
#endif
		logfile = optarg;
		break;
	    case '?':
	    default:
		usage (commit_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    /* numeric specified revision means we ignore sticky tags... */
    if (tag && isdigit (*tag))
    {
	aflag = 1;
	/* strip trailing dots */
	while (tag[strlen (tag) - 1] == '.')
	    tag[strlen (tag) - 1] = '\0';
    }

    /* some checks related to the "-f logfile" option */
    if (logfile)
    {
	int n, logfd;

	if (*message)
	    error (1, 0, "cannot specify both a message and a log file");

	if ((logfd = open (logfile, O_RDONLY)) < 0 ||
	    (n = read (logfd, message, MAXMESGLEN)) < 0)
	{
	    error (1, errno, "cannot read log message from %s", logfile);
	}
	(void) close (logfd);
	message[n] = '\0';
    }

    /* XXX - this is not the perfect check for this */
    if (argc <= 0)
	write_dirtag = tag;

    /*
     * Run the recursion processor to find all the dirs to lock and lock all
     * the dirs
     */
    locklist = getlist ();
    err = start_recursion ((int (*) ()) NULL, lock_filesdoneproc,
			   (Dtype (*) ()) NULL, (int (*) ()) NULL, argc,
			   argv, local, W_LOCAL, aflag, 0, (char *) NULL, 0);
    sortlist (locklist, fsortcmp);
    if (Writer_Lock (locklist) != 0)
	error (1, 0, "lock failed - giving up");

    /*
     * Set up the master update list
     */
    mulist = getlist ();

    /*
     * Run the recursion processor to verify the files are all up-to-date
     */
    err = start_recursion (check_fileproc, check_filesdoneproc,
			   check_direntproc, (int (*) ()) NULL, argc,
			   argv, local, W_LOCAL, aflag, 0, (char *) NULL, 1);
    if (err)
    {
	Lock_Cleanup ();
	error (1, 0, "correct above errors first!");
    }

    /*
     * Run the recursion processor to commit the files
     */
    if (noexec == 0)
	err = start_recursion (commit_fileproc, commit_filesdoneproc,
			       commit_direntproc, commit_dirleaveproc,
			       argc, argv, local, W_LOCAL, aflag, 0,
			       (char *) NULL, 1);

    /*
     * Unlock all the dirs and clean up
     */
    Lock_Cleanup ();
    dellist (&mulist);
    dellist (&locklist);
    return (err);
}

/*
 * compare two lock list nodes (for sort)
 */
static int
fsortcmp (p, q)
    Node *p, *q;
{
    return (strcmp (p->key, q->key));
}

/*
 * Create a list of repositories to lock
 */
/* ARGSUSED */
static int
lock_filesdoneproc (err, repository, update_dir)
    int err;
    char *repository;
    char *update_dir;
{
    Node *p;

    p = getnode ();
    p->type = LOCK;
    p->key = xstrdup (repository);
    if (p->key == NULL || addnode (locklist, p) != 0)
	freenode (p);
    return (err);
}

/*
 * Check to see if a file is ok to commit and make sure all files are
 * up-to-date
 */
/* ARGSUSED */
static int
check_fileproc (file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List *entries;
    List *srcfiles;
{
    Ctype status;
    char *xdir;
    Node *p;
    List *ulist, *cilist;
    Vers_TS *vers;
    struct commit_info *ci;
    int save_noexec, save_quiet, save_really_quiet;

    save_noexec = noexec;
    save_quiet = quiet;
    save_really_quiet = really_quiet;
    noexec = quiet = really_quiet = 1;

    /* handle specified numeric revision specially */
    if (tag && isdigit (*tag))
    {
	/* If the tag is for the trunk, make sure we're at the head */
	if (numdots (tag) < 2)
	{
	    status = Classify_File (file, (char *) NULL, (char *) NULL,
				    (char *) NULL, 1, aflag, repository,
				    entries, srcfiles, &vers);
	    if (status == T_UPTODATE)
	    {
		freevers_ts (&vers);
		status = Classify_File (file, tag, (char *) NULL,
					(char *) NULL, 1, aflag, repository,
					entries, srcfiles, &vers);
		if (status == T_REMOVE_ENTRY)
		    status = T_MODIFIED;
	    }
	}
	else
	{
	    char *xtag, *cp;

	    /*
	     * The revision is off the main trunk; make sure we're
	     * up-to-date with the head of the specified branch.
	     */
	    xtag = xstrdup (tag);
	    if ((numdots (xtag) & 1) != 0)
	    {
		cp = rindex (xtag, '.');
		*cp = '\0';
	    }
	    status = Classify_File (file, xtag, (char *) NULL,
				    (char *) NULL, 1, aflag, repository,
				    entries, srcfiles, &vers);
	    if ((status == T_REMOVE_ENTRY || status == T_CONFLICT)
		&& (cp = rindex (xtag, '.')) != NULL)
	    {
		/* pluck one more dot off the revision */
		*cp = '\0';
		freevers_ts (&vers);
		status = Classify_File (file, xtag, (char *) NULL,
					(char *) NULL, 1, aflag, repository,
					entries, srcfiles, &vers);
		if (status == T_UPTODATE || status == T_REMOVE_ENTRY)
		    status = T_MODIFIED;
	    }
	    /* now, muck with vers to make the tag correct */
	    free (vers->tag);
	    vers->tag = xstrdup (tag);
	    free (xtag);
	}
    }
    else
	status = Classify_File (file, tag, (char *) NULL, (char *) NULL,
				1, 0, repository, entries, srcfiles, &vers);
    noexec = save_noexec;
    quiet = save_quiet;
    really_quiet = save_really_quiet;

    switch (status)
    {
	case T_CHECKOUT:
	case T_NEEDS_MERGE:
	case T_CONFLICT:
	case T_REMOVE_ENTRY:
	    error (0, 0, "Up-to-date check failed for `%s'", file);
	    freevers_ts (&vers);
	    return (1);
	case T_MODIFIED:
	case T_ADDED:
	case T_REMOVED:
	    /*
	     * some quick sanity checks; if no numeric -r option specified:
	     *	- can't have a sticky date
	     *	- can't have a sticky tag that is not a branch
	     * Also,
	     *	- if status is T_REMOVED, can't have a numeric tag
	     *	- if status is T_ADDED, rcs file must not exist
	     *	- if status is T_ADDED, can't have a non-trunk numeric rev
	     */
	    if (!tag || !isdigit (*tag))
	    {
		if (vers->date)
		{
		    error (0, 0,
			   "cannot commit with sticky date for file `%s'",
			   file);
		    freevers_ts (&vers);
		    return (1);
		}
		if (status == T_MODIFIED && vers->tag &&
		    !RCS_isbranch (file, vers->tag, srcfiles))
		{
		    error (0, 0,
			   "sticky tag `%s' for file `%s' is not a branch",
			   vers->tag, file);
		    freevers_ts (&vers);
		    return (1);
		}
	    }
	    if (status == T_REMOVED && vers->tag && isdigit (*vers->tag))
	    {
		error (0, 0,
	"cannot remove file `%s' which has a numeric sticky tag of `%s'",
		       file, vers->tag);
		freevers_ts (&vers);
		return (1);
	    }
	    if (status == T_ADDED)
	    {
		char rcs[PATH_MAX];

		locate_rcs (file, repository, rcs);
		if (isreadable (rcs))
		{
		    error (0, 0,
		"cannot add file `%s' when RCS file `%s' already exists",
			   file, rcs);
		    freevers_ts (&vers);
		    return (1);
		}
		if (vers->tag && isdigit (*vers->tag) &&
		    numdots (vers->tag) > 1)
		{
		    error (0, 0,
		"cannot add file `%s' with revision `%s'; must be on trunk",
			   file, vers->tag);
		    freevers_ts (&vers);
		    return (1);
		}
	    }

	    /* done with consistency checks; now, to get on with the commit */
	    if (update_dir[0] == '\0')
		xdir = ".";
	    else
		xdir = update_dir;
	    if ((p = findnode (mulist, xdir)) != NULL)
	    {
		ulist = ((struct master_lists *) p->data)->ulist;
		cilist = ((struct master_lists *) p->data)->cilist;
	    }
	    else
	    {
		struct master_lists *ml;

		ulist = getlist ();
		cilist = getlist ();
		p = getnode ();
		p->key = xstrdup (xdir);
		p->type = UPDATE;
		ml = (struct master_lists *)
		    xmalloc (sizeof (struct master_lists));
		ml->ulist = ulist;
		ml->cilist = cilist;
		p->data = (char *) ml;
		(void) addnode (mulist, p);
	    }

	    /* first do ulist, then cilist */
	    p = getnode ();
	    p->key = xstrdup (file);
	    p->type = UPDATE;
	    p->delproc = update_delproc;
	    p->data = (char *) status;
	    (void) addnode (ulist, p);

	    p = getnode ();
	    p->key = xstrdup (file);
	    p->type = UPDATE;
	    p->delproc = ci_delproc;
	    ci = (struct commit_info *) xmalloc (sizeof (struct commit_info));
	    ci->status = status;
	    if (vers->tag)
		if (isdigit (*vers->tag))
		    ci->rev = xstrdup (vers->tag);
		else
		    ci->rev = RCS_whatbranch (file, vers->tag, srcfiles);
	    else
		ci->rev = (char *) NULL;
	    ci->tag = xstrdup (vers->tag);
	    p->data = (char *) ci;
	    (void) addnode (cilist, p);
	    break;
	case T_UNKNOWN:
	    error (0, 0, "nothing known about `%s'", file);
	    freevers_ts (&vers);
	    return (1);
	case T_UPTODATE:
	    break;
	default:
	    error (0, 0, "Unknown status 0x%x for `%s'", status, file);
	    break;
    }

    freevers_ts (&vers);
    return (0);
}

/*
 * Print warm fuzzies while examining the dirs
 */
/* ARGSUSED */
static Dtype
check_direntproc (dir, repos, update_dir)
    char *dir;
    char *repos;
    char *update_dir;
{
    if (!quiet)
	error (0, 0, "Examining %s", update_dir);

    return (R_PROCESS);
}

/*
 * Walklist proc to run pre-commit checks
 */
static int
precommit_list_proc (p)
    Node *p;
{
    if (p->data == (char *) T_ADDED || p->data == (char *) T_MODIFIED)
	run_arg (p->key);
    return (0);
}

/*
 * Callback proc for pre-commit checking
 */
static List *ulist;
static int
precommit_proc (repository, filter)
    char *repository;
    char *filter;
{
    /* see if the filter is there, only if it's a full path */
    if (filter[0] == '/' && !isfile (filter))
    {
	error (0, errno, "cannot find pre-commit filter `%s'", filter);
	return (1);			/* so it fails! */
    }

    run_setup ("%s %s", filter, repository);
    (void) walklist (ulist, precommit_list_proc);
    return (run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL|RUN_REALLY));
}

/*
 * Run the pre-commit checks for the dir
 */
/* ARGSUSED */
static int
check_filesdoneproc (err, repos, update_dir)
    int err;
    char *repos;
    char *update_dir;
{
    int n;
    Node *p;

    /* find the update list for this dir */
    p = findnode (mulist, update_dir);
    if (p != NULL)
	ulist = ((struct master_lists *) p->data)->ulist;
    else
	ulist = (List *) NULL;

    /* skip the checks if there's nothing to do */
    if (ulist == NULL || ulist->list->next == ulist->list)
	return (err);

    /* run any pre-commit checks */
    if ((n = Parse_Info (CVSROOTADM_COMMITINFO, repos, precommit_proc, 1)) > 0)
    {
	error (0, 0, "Pre-commit check failed");
	err += n;
    }

    return (err);
}

/*
 * Do the work of committing a file
 */
static int maxrev;
static char sbranch[PATH_MAX];

/* ARGSUSED */
static int
commit_fileproc (file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List *entries;
    List *srcfiles;
{
    Node *p;
    int err = 0;
    List *ulist, *cilist;
    struct commit_info *ci;
    char rcs[PATH_MAX];

    if (update_dir[0] == '\0')
	p = findnode (mulist, ".");
    else
	p = findnode (mulist, update_dir);

    /*
     * if p is null, there were file type command line args which were
     * all up-to-date so nothing really needs to be done
     */
    if (p == NULL)
	return (0);
    ulist = ((struct master_lists *) p->data)->ulist;
    cilist = ((struct master_lists *) p->data)->cilist;

    /*
     * At this point, we should have the commit message unless we were called
     * with files as args from the command line.  In that latter case, we
     * need to get the commit message ourselves
     */
    if (use_editor && !got_message)
    {
	got_message = 1;
	do_editor (update_dir, message, repository, ulist);
    }

    p = findnode (cilist, file);
    if (p == NULL)
	return (0);

    ci = (struct commit_info *) p->data;
    if (ci->status == T_MODIFIED)
    {
	if (lockrcsfile (file, repository, ci->rev) != 0)
	{
	    unlockrcs (file, repository);
	    return (1);
	}
    }
    else if (ci->status == T_ADDED)
    {
	if (checkaddfile (file, repository, ci->tag) != 0)
	{
	    fixaddfile (file, repository);
	    return (1);
	}
    }

    /*
     * Add the file for real
     */
    if (ci->status == T_ADDED)
    {
	char *xrev = (char *) NULL;

	if (ci->rev == NULL)
	{
	    /* find the max major rev number in this directory */
	    maxrev = 0;
	    (void) walklist (entries, findmaxrev);
	    if (maxrev == 0)
		maxrev = 1;
	    xrev = xmalloc (20);
	    (void) sprintf (xrev, "%d", maxrev);
	}

	/* XXX - an added file with symbolic -r should add tag as well */
	err = finaladd (file, ci->rev ? ci->rev : xrev, ci->tag,
			  repository, entries);
	if (xrev)
	    free (xrev);
	return (err);
    }

    if (ci->status == T_MODIFIED)
    {
	locate_rcs (file, repository, rcs);
	err = Checkin ('M', file, repository, rcs, ci->rev, ci->tag,
		       message, entries);
	if (err != 0)
	{
	    unlockrcs (file, repository);
	    fixbranch (file, repository, sbranch);
	}
    }

    if (ci->status == T_REMOVED)
	err = remove_file (file, repository, ci->tag, entries);

    return (err);
}

/*
 * Log the commit and clean up the update list
 */
/* ARGSUSED */
static int
commit_filesdoneproc (err, repository, update_dir)
    int err;
    char *repository;
    char *update_dir;
{
    List *ulist, *cilist;
    char *xtag = (char *) NULL;
    Node *p;

    p = findnode (mulist, update_dir);
    if (p != NULL)
    {
	ulist = ((struct master_lists *) p->data)->ulist;
	cilist = ((struct master_lists *) p->data)->cilist;
    }
    else
	return (err);

    got_message = 0;

    /* see if we need to specify a per-directory or -r option tag */
    if (tag == NULL)
	ParseTag (&xtag, (char **) NULL);

    Update_Logfile (repository, message, tag ? tag : xtag, (FILE *) 0, ulist);
    dellist (&ulist);
    dellist (&cilist);
    if (xtag)
	free (xtag);

    if (err == 0 && run_module_prog)
    {
	char *cp;
	FILE *fp;
	char line[MAXLINELEN];
	char *repository;

	/* It is not an error if Checkin.prog does not exist.  */
	if ((fp = fopen (CVSADM_CIPROG, "r")) != NULL)
	{
	    if (fgets (line, sizeof (line), fp) != NULL)
	    {
		if ((cp = rindex (line, '\n')) != NULL)
		    *cp = '\0';
		repository = Name_Repository ((char *) NULL, update_dir);
		run_setup ("%s %s", line, repository);
		(void) printf ("%s %s: Executing '", program_name,
			       command_name);
		run_print (stdout);
		(void) printf ("'\n");
		(void) run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
		free (repository);
	    }
	    (void) fclose (fp);
	}
    }

    return (err);
}

/*
 * Get the log message for a dir and print a warm fuzzy
 */
/* ARGSUSED */
static Dtype
commit_direntproc (dir, repos, update_dir)
    char *dir;
    char *repos;
    char *update_dir;
{
    Node *p;
    List *ulist;
    char *real_repos;

    /* find the update list for this dir */
    p = findnode (mulist, update_dir);
    if (p != NULL)
	ulist = ((struct master_lists *) p->data)->ulist;
    else
	ulist = (List *) NULL;

    /* skip the files as an optimization */
    if (ulist == NULL || ulist->list->next == ulist->list)
	return (R_SKIP_FILES);

    /* print the warm fuzzy */
    if (!quiet)
	error (0, 0, "Committing %s", update_dir);

    /* get commit message */
    if (use_editor)
    {
	got_message = 1;
	real_repos = Name_Repository (dir, update_dir);
	do_editor (update_dir, message, real_repos, ulist);
	free (real_repos);
    }
    return (R_PROCESS);
}

/*
 * Process the post-commit proc if necessary
 */
/* ARGSUSED */
static int
commit_dirleaveproc (dir, err, update_dir)
    char *dir;
    int err;
    char *update_dir;
{
    /* update the per-directory tag info */
    if (err == 0 && write_dirtag != NULL)
	WriteTag ((char *) NULL, write_dirtag, (char *) NULL);

    return (err);
}

/*
 * find the maximum major rev number in an entries file
 */
static int
findmaxrev (p)
    Node *p;
{
    char *cp;
    int thisrev;
    Entnode *entdata;

    entdata = (Entnode *) p->data;
    cp = index (entdata->version, '.');
    if (cp != NULL)
	*cp = '\0';
    thisrev = atoi (entdata->version);
    if (cp != NULL)
	*cp = '.';
    if (thisrev > maxrev)
	maxrev = thisrev;
    return (0);
}

/*
 * Actually remove a file by moving it to the attic
 * XXX - if removing a ,v file that is a relative symbolic link to
 * another ,v file, we probably should add a ".." component to the
 * link to keep it relative after we move it into the attic.
 */
static int
remove_file (file, repository, tag, entries)
    char *file;
    char *repository;
    char *tag;
    List *entries;
{
    int omask;
    int retcode;
    char rcs[PATH_MAX];
    char tmp[PATH_MAX];

    locate_rcs (file, repository, rcs);
    if (tag)
    {
	/* a symbolic tag is specified; just remove the tag from the file */
	run_setup ("%s%s -q -N%s", Rcsbin, RCS, tag);
	run_arg (rcs);
	if ((retcode = run_exec (RUN_TTY, RUN_TTY, DEVNULL, RUN_NORMAL)) != 0)
	{
	    if (!quiet)
		error (0, retcode == -1 ? errno : 0,
		       "failed to remove tag `%s' from `%s'", tag, rcs);
	    return (1);
	}
	return (0);
    }
    else
    {
	/* no symbolic tag specified; really move it into the Attic */
	(void) sprintf (tmp, "%s/%s", repository, CVSATTIC);
	omask = umask (2);
	(void) mkdir (tmp, 0777);
	(void) umask (omask);
	(void) sprintf (tmp, "%s/%s/%s%s", repository, CVSATTIC, file, RCSEXT);

	if ((strcmp (rcs, tmp) == 0 || rename (rcs, tmp) != -1) ||
	    (!isreadable (rcs) && isreadable (tmp)))
	{
	    Scratch_Entry (entries, file);
	    return (0);
	}
    }
    return (1);
}

/*
 * Do the actual checkin for added files
 */
static int
finaladd (file, rev, tag, repository, entries)
    char *file;
    char *rev;
    char *tag;
    char *repository;
    List *entries;
{
    int ret;
    char tmp[PATH_MAX];
    char rcs[PATH_MAX];

    locate_rcs (file, repository, rcs);
    ret = Checkin ('A', file, repository, rcs, rev, tag,
		   message, entries);
    if (ret == 0)
    {
	(void) sprintf (tmp, "%s/%s%s", CVSADM, file, CVSEXT_OPT);
	(void) unlink_file (tmp);
	(void) sprintf (tmp, "%s/%s%s", CVSADM, file, CVSEXT_LOG);
	(void) unlink_file (tmp);
    }
    else
	fixaddfile (file, repository);
    return (ret);
}

/*
 * Unlock an rcs file
 */
static void
unlockrcs (file, repository)
    char *file;
    char *repository;
{
    char rcs[PATH_MAX];
    int retcode = 0;

    locate_rcs (file, repository, rcs);
    run_setup ("%s%s -q -u", Rcsbin, RCS);
    run_arg (rcs);

    if ((retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL)) != 0)
	error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
	       "could not unlock %s", rcs);
}

/*
 * remove a partially added file.  if we can parse it, leave it alone.
 */
static void
fixaddfile (file, repository)
    char *file;
    char *repository;
{
    RCSNode *rcsfile;
    char rcs[PATH_MAX];
    int save_really_quiet;

    locate_rcs (file, repository, rcs);
    save_really_quiet = really_quiet;
    really_quiet = 1;
    if ((rcsfile = RCS_parsercsfile (rcs)) == NULL)
	(void) unlink_file (rcs);
    else
	freercsnode (&rcsfile);
    really_quiet = save_really_quiet;
}

/*
 * put the branch back on an rcs file
 */
static void
fixbranch (file, repository, branch)
    char *file;
    char *repository;
    char *branch;
{
    char rcs[PATH_MAX];
    int retcode = 0;

    if (branch != NULL && branch[0] != '\0')
    {
	locate_rcs (file, repository, rcs);
	run_setup ("%s%s -q -b%s", Rcsbin, RCS, branch);
	run_arg (rcs);
	if ((retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL)) != 0)
	    error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
		   "cannot restore branch to %s for %s", branch, rcs);
    }
}

/*
 * do the initial part of a file add for the named file.  if adding
 * with a tag, put the file in the Attic and point the symbolic tag
 * at the committed revision.
 */
static int
checkaddfile (file, repository, tag)
    char *file;
    char *repository;
    char *tag;
{
    FILE *fp;
    char *cp;
    char rcs[PATH_MAX];
    char fname[PATH_MAX];
    int omask;
    int retcode = 0;

    if (tag)
    {
	(void) sprintf(rcs, "%s/%s", repository, CVSATTIC);
	omask = umask (2);
	(void) mkdir (rcs, 0777);
	(void) umask (omask);
	(void) sprintf (rcs, "%s/%s/%s%s", repository, CVSATTIC, file, RCSEXT);
    }
    else
	locate_rcs (file, repository, rcs);

    run_setup ("%s%s -i", Rcsbin, RCS);
    run_args ("-t%s/%s%s", CVSADM, file, CVSEXT_LOG);
    (void) sprintf (fname, "%s/%s%s", CVSADM, file, CVSEXT_OPT);
    fp = open_file (fname, "r");
    while (fgets (fname, sizeof (fname), fp) != NULL)
    {
	if ((cp = rindex (fname, '\n')) != NULL)
	    *cp = '\0';
	if (*fname)
	    run_arg (fname);
    }
    (void) fclose (fp);
    run_arg (rcs);
    if ((retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL)) != 0)
    {
	error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
	       "could not create %s", rcs);
	return (1);
    }
    fix_rcs_modes (rcs, file);
    return (0);
}

/*
 * Lock the rcs file ``file''
 */
static int
lockrcsfile (file, repository, rev)
    char *file;
    char *repository;
    char *rev;
{
    char rcs[PATH_MAX];

    locate_rcs (file, repository, rcs);
    if (lock_RCS (file, rcs, rev, repository) != 0)
	return (1);
    else
	return (0);
}

/*
 * Attempt to place a lock on the RCS file; returns 0 if it could and 1 if it
 * couldn't.  If the RCS file currently has a branch as the head, we must
 * move the head back to the trunk before locking the file, and be sure to
 * put the branch back as the head if there are any errors.
 */
static int
lock_RCS (user, rcs, rev, repository)
    char *user;
    char *rcs;
    char *rev;
    char *repository;
{
    RCSNode *rcsfile;
    char *branch = NULL;
    int err = 0;

    /*
     * For a specified, numeric revision of the form "1" or "1.1", (or when
     * no revision is specified ""), definitely move the branch to the trunk
     * before locking the RCS file.
     * 
     * The assumption is that if there is more than one revision on the trunk,
     * the head points to the trunk, not a branch... and as such, it's not
     * necessary to move the head in this case.
     */
    if (rev == NULL || (rev && isdigit (*rev) && numdots (rev) < 2))
    {
	if ((rcsfile = RCS_parsercsfile (rcs)) == NULL)
	{
	    /* invalid rcs file? */
	    err = 1;
	}
	else
	{
	    /* rcsfile is valid */
	    branch = xstrdup (rcsfile->branch);
	    freercsnode (&rcsfile);
	    if (branch != NULL)
	    {
		run_setup ("%s%s -q -b", Rcsbin, RCS);
		run_arg (rcs);
		if (run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL) != 0)
		{
		    error (0, 0, "cannot change branch to default for %s",
			   rcs);
		    if (branch)
			free (branch);
		    return (1);
		}
	    }
	    run_setup ("%s%s -q -l", Rcsbin, RCS);
	    run_arg (rcs);
	    err = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
	}
    }
    else
    {
	run_setup ("%s%s -q -l%s", Rcsbin, RCS, rev ? rev : "");
	run_arg (rcs);
	(void) run_exec (RUN_TTY, RUN_TTY, DEVNULL, RUN_NORMAL);
    }

    if (err == 0)
    {
	if (branch)
	{
	    (void) strcpy (sbranch, branch);
	    free (branch);
	}
	else
	    sbranch[0] = '\0';
	return (0);
    }

    /* try to restore the branch if we can on error */
    if (branch != NULL)
	fixbranch (user, repository, branch);

    if (branch)
	free (branch);
    return (1);
}

/*
 * Called when "add"ing files to the RCS respository, as it is necessary to
 * preserve the file modes in the same fashion that RCS does.  This would be
 * automatic except that we are placing the RCS ,v file very far away from
 * the user file, and I can't seem to convince RCS of the location of the
 * user file.  So we munge it here, after the ,v file has been successfully
 * initialized with "rcs -i".
 */
static void
fix_rcs_modes (rcs, user)
    char *rcs;
    char *user;
{
    struct stat sb;

    if (stat (user, &sb) != -1)
	(void) chmod (rcs, (int) sb.st_mode & ~0222);
}

/*
 * free an UPDATE node's data (really nothing to do)
 */
void
update_delproc (p)
    Node *p;
{
    p->data = (char *) NULL;
}

/*
 * Free the commit_info structure in p.
 */
static void
ci_delproc (p)
    Node *p;
{
    struct commit_info *ci;

    ci = (struct commit_info *) p->data;
    if (ci->rev)
	free (ci->rev);
    if (ci->tag)
	free (ci->tag);
    free (ci);
}

/*
 * Find an RCS file in the repository.
 */
static void
locate_rcs (file, repository, rcs)
    char *file;
    char *repository;
    char *rcs;
{
    (void) sprintf (rcs, "%s/%s%s", repository, file, RCSEXT);
    if (!isreadable (rcs))
    {
	(void) sprintf (rcs, "%s/%s/%s%s", repository, CVSATTIC, file, RCSEXT);
	if (!isreadable (rcs))
	    (void) sprintf (rcs, "%s/%s%s", repository, file, RCSEXT);
    }
}
