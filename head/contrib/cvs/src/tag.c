/*
 * Copyright (C) 1986-2005 The Free Software Foundation, Inc.
 *
 * Portions Copyright (C) 1998-2005 Derek Price, Ximbiot <http://ximbiot.com>,
 *                                  and others.
 *
 * Portions Copyright (C) 1992, Brian Berliner and Jeff Polk
 * Portions Copyright (C) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Tag and Rtag
 * 
 * Add or delete a symbolic name to an RCS file, or a collection of RCS files.
 * Tag uses the checked out revision in the current directory, rtag uses
 * the modules database, if necessary.
 *
 * $FreeBSD$
 */

#include "cvs.h"
#include "savecwd.h"

static int rtag_proc PROTO((int argc, char **argv, char *xwhere,
		      char *mwhere, char *mfile, int shorten,
		      int local_specified, char *mname, char *msg));
static int check_fileproc PROTO ((void *callerdat, struct file_info *finfo));
static int check_filesdoneproc PROTO ((void *callerdat, int err,
                                       const char *repos,
                                       const char *update_dir,
                                       List *entries));
static int pretag_proc PROTO((const char *repository, const char *filter));
static void masterlist_delproc PROTO((Node *p));
static void tag_delproc PROTO((Node *p));
static int pretag_list_proc PROTO((Node *p, void *closure));

static Dtype tag_dirproc PROTO ((void *callerdat, const char *dir,
                                 const char *repos, const char *update_dir,
                                 List *entries));
static int rtag_fileproc PROTO ((void *callerdat, struct file_info *finfo));
static int rtag_delete PROTO((RCSNode *rcsfile));
static int tag_fileproc PROTO ((void *callerdat, struct file_info *finfo));

static char *numtag;			/* specific revision to tag */
static int numtag_validated = 0;
static char *date = NULL;
static char *symtag;			/* tag to add or delete */
static int delete_flag;			/* adding a tag by default */
static int branch_mode;			/* make an automagic "branch" tag */
static int disturb_branch_tags = 0;	/* allow -F,-d to disturb branch tags */
static int force_tag_match = 1;		/* force tag to match by default */
static int force_tag_move;		/* don't force tag to move by default */
static int check_uptodate;		/* no uptodate-check by default */
static int attic_too;			/* remove tag from Attic files */
static int is_rtag;

struct tag_info
{
    Ctype status;
    char *rev;
    char *tag;
    char *options;
};

struct master_lists
{
    List *tlist;
};

static List *mtlist;
static List *tlist;

static const char rtag_opts[] = "+aBbdFflnQqRr:D:";
static const char *const rtag_usage[] =
{
    "Usage: %s %s [-abdFflnR] [-r rev|-D date] tag modules...\n",
    "\t-a\tClear tag from removed files that would not otherwise be tagged.\n",
    "\t-b\tMake the tag a \"branch\" tag, allowing concurrent development.\n",
    "\t-B\tAllows -F and -d to disturb branch tags.  Use with extreme care.\n",
    "\t-d\tDelete the given tag.\n",
    "\t-F\tMove tag if it already exists.\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, not recursive.\n",
    "\t-n\tNo execution of 'tag program'.\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-r rev\tExisting revision/tag.\n",
    "\t-D\tExisting date.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

static const char tag_opts[] = "+BbcdFflQqRr:D:";
static const char *const tag_usage[] =
{
    "Usage: %s %s [-bcdFflR] [-r rev|-D date] tag [files...]\n",
    "\t-b\tMake the tag a \"branch\" tag, allowing concurrent development.\n",
    "\t-B\tAllows -F and -d to disturb branch tags.  Use with extreme care.\n",
    "\t-c\tCheck that working files are unmodified.\n",
    "\t-d\tDelete the given tag.\n",
    "\t-F\tMove tag if it already exists.\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, not recursive.\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-r rev\tExisting revision/tag.\n",
    "\t-D\tExisting date.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int
cvstag (argc, argv)
    int argc;
    char **argv;
{
    int local = 0;			/* recursive by default */
    int c;
    int err = 0;
    int run_module_prog = 1;

    is_rtag = (strcmp (cvs_cmd_name, "rtag") == 0);
    
    if (argc == -1)
	usage (is_rtag ? rtag_usage : tag_usage);

    optind = 0;
    while ((c = getopt (argc, argv, is_rtag ? rtag_opts : tag_opts)) != -1)
    {
	switch (c)
	{
	    case 'a':
		attic_too = 1;
		break;
	    case 'b':
		branch_mode = 1;
		break;
	    case 'B':
		disturb_branch_tags = 1;
		break;
	    case 'c':
		check_uptodate = 1;
		break;
	    case 'd':
		delete_flag = 1;
		break;
            case 'F':
		force_tag_move = 1;
		break;
	    case 'f':
		force_tag_match = 0;
		break;
	    case 'l':
		local = 1;
		break;
	    case 'n':
		run_module_prog = 0;
		break;
	    case 'Q':
	    case 'q':
		/* The CVS 1.5 client sends these options (in addition to
		   Global_option requests), so we must ignore them.  */
		if (!server_active)
		    error (1, 0,
			   "-q or -Q must be specified before \"%s\"",
			   cvs_cmd_name);
		break;
	    case 'R':
		local = 0;
		break;
            case 'r':
                numtag = optarg;
                break;
            case 'D':
                if (date)
                    free (date);
                date = Make_Date (optarg);
                break;
	    case '?':
	    default:
		usage (is_rtag ? rtag_usage : tag_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (argc < (is_rtag ? 2 : 1))
	usage (is_rtag ? rtag_usage : tag_usage);
    symtag = argv[0];
    argc--;
    argv++;

    if (date && numtag)
	error (1, 0, "-r and -D options are mutually exclusive");
    if (delete_flag && branch_mode)
	error (0, 0, "warning: -b ignored with -d options");
    RCS_check_tag (symtag);

#ifdef CLIENT_SUPPORT
    if (current_parsed_root->isremote)
    {
	/* We're the client side.  Fire up the remote server.  */
	start_server ();
	
	ign_setup ();

	if (attic_too)
	    send_arg("-a");
	if (branch_mode)
	    send_arg("-b");
	if (disturb_branch_tags)
	    send_arg("-B");
	if (check_uptodate)
	    send_arg("-c");
	if (delete_flag)
	    send_arg("-d");
	if (force_tag_move)
	    send_arg("-F");
	if (!force_tag_match)
	    send_arg ("-f");
	if (local)
	    send_arg("-l");
	if (!run_module_prog)
	    send_arg("-n");

	if (numtag)
	    option_with_arg ("-r", numtag);
	if (date)
	    client_senddate (date);

	send_arg ("--");

	send_arg (symtag);

	if (is_rtag)
	{
	    int i;
	    for (i = 0; i < argc; ++i)
		send_arg (argv[i]);
	    send_to_server ("rtag\012", 0);
	}
	else
	{
	    send_files (argc, argv, local, 0,

		    /* I think the -c case is like "cvs status", in
		       which we really better be correct rather than
		       being fast; it is just too confusing otherwise.  */
			check_uptodate ? 0 : SEND_NO_CONTENTS);
	    send_file_names (argc, argv, SEND_EXPAND_WILD);
	    send_to_server ("tag\012", 0);
	}

        return get_responses_and_close ();
    }
#endif

    if (is_rtag)
    {
	DBM *db;
	int i;
	db = open_module ();
	for (i = 0; i < argc; i++)
	{
	    /* XXX last arg should be repository, but doesn't make sense here */
	    history_write ('T', (delete_flag ? "D" : (numtag ? numtag : 
			   (date ? date : "A"))), symtag, argv[i], "");
	    err += do_module (db, argv[i], TAG,
			      delete_flag ? "Untagging" : "Tagging",
			      rtag_proc, (char *) NULL, 0, local, run_module_prog,
			      0, symtag);
	}
	close_module (db);
    }
    else
    {
	err = rtag_proc (argc + 1, argv - 1, NULL, NULL, NULL, 0, local, NULL,
			 NULL);
    }

    return (err);
}

/*
 * callback proc for doing the real work of tagging
 */
/* ARGSUSED */
static int
rtag_proc (argc, argv, xwhere, mwhere, mfile, shorten, local_specified,
	   mname, msg)
    int argc;
    char **argv;
    char *xwhere;
    char *mwhere;
    char *mfile;
    int shorten;
    int local_specified;
    char *mname;
    char *msg;
{
    /* Begin section which is identical to patch_proc--should this
       be abstracted out somehow?  */
    char *myargv[2];
    int err = 0;
    int which;
    char *repository;
    char *where;

    if (is_rtag)
    {
	repository = xmalloc (strlen (current_parsed_root->directory) + strlen (argv[0])
			      + (mfile == NULL ? 0 : strlen (mfile) + 1) + 2);
	(void) sprintf (repository, "%s/%s", current_parsed_root->directory, argv[0]);
	where = xmalloc (strlen (argv[0]) + (mfile == NULL ? 0 : strlen (mfile) + 1)
			 + 1);
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
		myargv[0] = argv[0];
		myargv[1] = mfile;
		argc = 2;
		argv = myargv;
	    }
	    free (path);
	}

	/* cd to the starting repository */
	if ( CVS_CHDIR (repository) < 0)
	{
	    error (0, errno, "cannot chdir to %s", repository);
	    free (repository);
	    free (where);
	    return (1);
	}
	/* End section which is identical to patch_proc.  */

	if (delete_flag || force_tag_move || attic_too || numtag)
	    which = W_REPOS | W_ATTIC;
	else
	    which = W_REPOS;
    }
    else
    {
        where = NULL;
        which = W_LOCAL;
        repository = "";
    }

    if (numtag != NULL && !numtag_validated)
    {
	tag_check_valid (numtag, argc - 1, argv + 1, local_specified, 0, repository);
	numtag_validated = 1;
    }

    /* check to make sure they are authorized to tag all the 
       specified files in the repository */

    mtlist = getlist();
    err = start_recursion (check_fileproc, check_filesdoneproc,
                           (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			   argc - 1, argv + 1, local_specified, which, 0,
			   CVS_LOCK_READ, where, 1, repository);
    
    if (err)
    {
       error (1, 0, "correct the above errors first!");
    }
     
    /* It would be nice to provide consistency with respect to
       commits; however CVS lacks the infrastructure to do that (see
       Concurrency in cvs.texinfo and comment in do_recursion).  */

    /* start the recursion processor */
    err = start_recursion (is_rtag ? rtag_fileproc : tag_fileproc,
			   (FILESDONEPROC) NULL, tag_dirproc,
			   (DIRLEAVEPROC) NULL, NULL, argc - 1, argv + 1,
			   local_specified, which, 0, CVS_LOCK_WRITE, where, 1,
			   repository);
    if ( which & W_REPOS ) free ( repository );
    dellist (&mtlist);
    if (where != NULL)
	free (where);
    return (err);
}

/* check file that is to be tagged */
/* All we do here is add it to our list */

static int
check_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    const char *xdir;
    Node *p;
    Vers_TS *vers;
    
    if (check_uptodate) 
    {
	switch (Classify_File (finfo, (char *) NULL, (char *) NULL,
				      (char *) NULL, 1, 0, &vers, 0))
	{
	case T_UPTODATE:
	case T_CHECKOUT:
	case T_PATCH:
	case T_REMOVE_ENTRY:
	    break;
	case T_UNKNOWN:
	case T_CONFLICT:
	case T_NEEDS_MERGE:
	case T_MODIFIED:
	case T_ADDED:
	case T_REMOVED:
	default:
	    error (0, 0, "%s is locally modified", finfo->fullname);
	    freevers_ts (&vers);
	    return (1);
	}
    }
    else
	vers = Version_TS (finfo, NULL, NULL, NULL, 0, 0);
    
    if (finfo->update_dir[0] == '\0')
	xdir = ".";
    else
	xdir = finfo->update_dir;
    if ((p = findnode (mtlist, xdir)) != NULL)
    {
	tlist = ((struct master_lists *) p->data)->tlist;
    }
    else
    {
	struct master_lists *ml;
        
	tlist = getlist ();
	p = getnode ();
	p->key = xstrdup (xdir);
	p->type = UPDATE;
	ml = (struct master_lists *)
	    xmalloc (sizeof (struct master_lists));
	ml->tlist = tlist;
	p->data = ml;
	p->delproc = masterlist_delproc;
	(void) addnode (mtlist, p);
    }
    /* do tlist */
    p = getnode ();
    p->key = xstrdup (finfo->file);
    p->type = UPDATE;
    p->delproc = tag_delproc;
    if (vers->srcfile == NULL)
    {
        if (!really_quiet)
	    error (0, 0, "nothing known about %s", finfo->file);
	freevers_ts (&vers);
	freenode (p);
	return (1);
    }

    /* Here we duplicate the calculation in tag_fileproc about which
       version we are going to tag.  There probably are some subtle races
       (e.g. numtag is "foo" which gets moved between here and
       tag_fileproc).  */
    if (!is_rtag && numtag == NULL && date == NULL)
	p->data = xstrdup (vers->vn_user);
    else
	p->data = RCS_getversion (vers->srcfile, numtag, date,
				  force_tag_match, NULL);

    if (p->data != NULL)
    {
        int addit = 1;
        char *oversion;
        
        oversion = RCS_getversion (vers->srcfile, symtag, (char *) NULL, 1,
				   (int *) NULL);
        if (oversion == NULL) 
        {
            if (delete_flag)
            {
		/* Deleting a tag which did not exist is a noop and
		   should not be logged.  */
                addit = 0;
            }
        }
	else if (delete_flag)
	{
	    free (p->data);
	    p->data = xstrdup (oversion);
	}
        else if (strcmp(oversion, p->data) == 0)
        {
            addit = 0;
        }
        else if (!force_tag_move)
        {
            addit = 0;
        }
        if (oversion != NULL)
        {
            free(oversion);
        }
        if (!addit)
        {
            free(p->data);
            p->data = NULL;
        }
    }
    freevers_ts (&vers);
    (void) addnode (tlist, p);
    return (0);
}
                         
static int
check_filesdoneproc (callerdat, err, repos, update_dir, entries)
    void *callerdat;
    int err;
    const char *repos;
    const char *update_dir;
    List *entries;
{
    int n;
    Node *p;

    p = findnode(mtlist, update_dir);
    if (p != NULL)
    {
        tlist = ((struct master_lists *) p->data)->tlist;
    }
    else
    {
        tlist = (List *) NULL;
    }
    if ((tlist == NULL) || (tlist->list->next == tlist->list))
    {
        return (err);
    }
    if ((n = Parse_Info(CVSROOTADM_TAGINFO, repos, pretag_proc, 1)) > 0)
    {
        error (0, 0, "Pre-tag check failed");
        err += n;
    }
    return (err);
}

static int
pretag_proc (repository, filter)
    const char *repository;
    const char *filter;
{
    if (filter[0] == '/')
    {
        char *s, *cp;

        s = xstrdup(filter);
        for (cp=s; *cp; cp++)
        {
            if (isspace ((unsigned char) *cp))
            {
                *cp = '\0';
                break;
            }
        }
        if (!isfile(s))
        {
            error (0, errno, "cannot find pre-tag filter '%s'", s);
            free(s);
            return (1);
        }
        free(s);
    }
    run_setup (filter);
    run_arg (symtag);
    run_arg (delete_flag ? "del" : force_tag_move ? "mov" : "add");
    run_arg (repository);
    walklist(tlist, pretag_list_proc, NULL);
    return (run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL));
}

static void
masterlist_delproc(p)
    Node *p;
{
    struct master_lists *ml = p->data;

    dellist(&ml->tlist);
    free(ml);
    return;
}

static void
tag_delproc(p)
    Node *p;
{
    if (p->data != NULL)
    {
        free(p->data);
        p->data = NULL;
    }
    return;
}

static int
pretag_list_proc(p, closure)
    Node *p;
    void *closure;
{
    if (p->data != NULL)
    {
        run_arg(p->key);
        run_arg(p->data);
    }
    return (0);
}


/*
 * Called to rtag a particular file, as appropriate with the options that were
 * set above.
 */
/* ARGSUSED */
static int
rtag_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    RCSNode *rcsfile;
    char *version, *rev;
    int retcode = 0;

    /* find the parsed RCS data */
    if ((rcsfile = finfo->rcs) == NULL)
	return (1);

    /*
     * For tagging an RCS file which is a symbolic link, you'd best be
     * running with RCS 5.6, since it knows how to handle symbolic links
     * correctly without breaking your link!
     */

    if (delete_flag)
	return (rtag_delete (rcsfile));

    /*
     * If we get here, we are adding a tag.  But, if -a was specified, we
     * need to check to see if a -r or -D option was specified.  If neither
     * was specified and the file is in the Attic, remove the tag.
     */
    if (attic_too && (!numtag && !date))
    {
	if ((rcsfile->flags & VALID) && (rcsfile->flags & INATTIC))
	    return (rtag_delete (rcsfile));
    }

    version = RCS_getversion (rcsfile, numtag, date, force_tag_match,
			      (int *) NULL);
    if (version == NULL)
    {
	/* If -a specified, clean up any old tags */
	if (attic_too)
	    (void) rtag_delete (rcsfile);

	if (!quiet && !force_tag_match)
	{
	    error (0, 0, "cannot find tag `%s' in `%s'",
		   numtag ? numtag : "head", rcsfile->path);
	    return (1);
	}
	return (0);
    }
    if (numtag
	&& isdigit ((unsigned char) *numtag)
	&& strcmp (numtag, version) != 0)
    {

	/*
	 * We didn't find a match for the numeric tag that was specified, but
	 * that's OK.  just pass the numeric tag on to rcs, to be tagged as
	 * specified.  Could get here if one tried to tag "1.1.1" and there
	 * was a 1.1.1 branch with some head revision.  In this case, we want
	 * the tag to reference "1.1.1" and not the revision at the head of
	 * the branch.  Use a symbolic tag for that.
	 */
	rev = branch_mode ? RCS_magicrev (rcsfile, version) : numtag;
	retcode = RCS_settag(rcsfile, symtag, numtag);
	if (retcode == 0)
	    RCS_rewrite (rcsfile, NULL, NULL);
    }
    else
    {
	char *oversion;
       
	/*
	 * As an enhancement for the case where a tag is being re-applied to
	 * a large body of a module, make one extra call to RCS_getversion to
	 * see if the tag is already set in the RCS file.  If so, check to
	 * see if it needs to be moved.  If not, do nothing.  This will
	 * likely save a lot of time when simply moving the tag to the
	 * "current" head revisions of a module -- which I have found to be a
	 * typical tagging operation.
	 */
	rev = branch_mode ? RCS_magicrev (rcsfile, version) : version;
	oversion = RCS_getversion (rcsfile, symtag, (char *) NULL, 1,
				   (int *) NULL);
	if (oversion != NULL)
	{
	    int isbranch = RCS_nodeisbranch (finfo->rcs, symtag);

	    /*
	     * if versions the same and neither old or new are branches don't
	     * have to do anything
	     */
	    if (strcmp (version, oversion) == 0 && !branch_mode && !isbranch)
	    {
		free (oversion);
		free (version);
		return (0);
	    }
	  
	    if (!force_tag_move)
	    {
		/* we're NOT going to move the tag */
		(void) printf ("W %s", finfo->fullname);

		(void) printf (" : %s already exists on %s %s", 
			       symtag, isbranch ? "branch" : "version",
			       oversion);
		(void) printf (" : NOT MOVING tag to %s %s\n", 
			       branch_mode ? "branch" : "version", rev);
		free (oversion);
		free (version);
		if (branch_mode) free(rev);
		return (0);
	    }
	    else /* force_tag_move is set and... */
		if ((isbranch && !disturb_branch_tags) ||
		    (!isbranch && disturb_branch_tags))
	    {
	        error(0,0, "%s: Not moving %s tag `%s' from %s to %s%s.", 
			finfo->fullname,
			isbranch ? "branch" : "non-branch",
			symtag, oversion, rev,
			isbranch ? "" : " due to `-B' option"); 
		if (branch_mode) free(rev);
		free (oversion);
		free (version);
		return (0);
	    }
	    free (oversion);
	}
	retcode = RCS_settag(rcsfile, symtag, rev);
	if (retcode == 0)
	    RCS_rewrite (rcsfile, NULL, NULL);
    }

    if (retcode != 0)
    {
	error (1, retcode == -1 ? errno : 0,
	       "failed to set tag `%s' to revision `%s' in `%s'",
	       symtag, rev, rcsfile->path);
        if (branch_mode)
	    free (rev);
        free (version);
        return (1);
    }
    if (branch_mode)
	free (rev);
    free (version);
    return (0);
}

/*
 * If -d is specified, "force_tag_match" is set, so that this call to
 * RCS_getversion() will return a NULL version string if the symbolic
 * tag does not exist in the RCS file.
 * 
 * If the -r flag was used, numtag is set, and we only delete the
 * symtag from files that have numtag.
 * 
 * This is done here because it's MUCH faster than just blindly calling
 * "rcs" to remove the tag... trust me.
 */
static int
rtag_delete (rcsfile)
    RCSNode *rcsfile;
{
    char *version;
    int retcode, isbranch;

    if (numtag)
    {
	version = RCS_getversion (rcsfile, numtag, (char *) NULL, 1,
				  (int *) NULL);
	if (version == NULL)
	    return (0);
	free (version);
    }

    version = RCS_getversion (rcsfile, symtag, (char *) NULL, 1,
			      (int *) NULL);
    if (version == NULL)
	return (0);
    free (version);


    isbranch = RCS_nodeisbranch (rcsfile, symtag);
    if ((isbranch && !disturb_branch_tags) || 
	(!isbranch && disturb_branch_tags))
    {
	if (!quiet)
	    error(0, 0,
		"Not removing %s tag `%s' from `%s'%s.", 
		isbranch ? "branch" : "non-branch",
		symtag, rcsfile->path,
		isbranch ? "" : " due to `-B' option"); 
	return (1);
    }

    if ((retcode = RCS_deltag(rcsfile, symtag)) != 0)
    {
	if (!quiet)
	    error (0, retcode == -1 ? errno : 0,
		   "failed to remove tag `%s' from `%s'", symtag,
		   rcsfile->path);
	return (1);
    }
    RCS_rewrite (rcsfile, NULL, NULL);
    return (0);
}


/*
 * Called to tag a particular file (the currently checked out version is
 * tagged with the specified tag - or the specified tag is deleted).
 */
/* ARGSUSED */
static int
tag_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    char *version, *oversion;
    char *nversion = NULL;
    char *rev;
    Vers_TS *vers;
    int retcode = 0;
    int retval = 0;

    vers = Version_TS (finfo, NULL, NULL, NULL, 0, 0);

    if ((numtag != NULL) || (date != NULL))
    {
        nversion = RCS_getversion(vers->srcfile,
                                  numtag,
                                  date,
                                  force_tag_match,
				  (int *) NULL);
        if (nversion == NULL)
	    goto free_vars_and_return;
    }
    if (delete_flag)
    {

	int isbranch;
	/*
	 * If -d is specified, "force_tag_match" is set, so that this call to
	 * RCS_getversion() will return a NULL version string if the symbolic
	 * tag does not exist in the RCS file.
	 * 
	 * This is done here because it's MUCH faster than just blindly calling
	 * "rcs" to remove the tag... trust me.
	 */

	version = RCS_getversion (vers->srcfile, symtag, (char *) NULL, 1,
				  (int *) NULL);
	if (version == NULL || vers->srcfile == NULL)
	    goto free_vars_and_return;

	free (version);

	isbranch = RCS_nodeisbranch (finfo->rcs, symtag);
	if ((isbranch && !disturb_branch_tags) || 
	    (!isbranch && disturb_branch_tags))
	{
	    if (!quiet)
		error(0, 0,
		       "Not removing %s tag `%s' from `%s'%s.", 
			isbranch ? "branch" : "non-branch",
			symtag, vers->srcfile->path,
			isbranch ? "" : " due to `-B' option"); 
	    retval = 1;
	    goto free_vars_and_return;
	}

	if ((retcode = RCS_deltag(vers->srcfile, symtag)) != 0) 
	{
	    if (!quiet)
		error (0, retcode == -1 ? errno : 0,
		       "failed to remove tag %s from %s", symtag,
		       vers->srcfile->path);
	    retval = 1;
	    goto free_vars_and_return;
	}
	RCS_rewrite (vers->srcfile, NULL, NULL);

	/* warm fuzzies */
	if (!really_quiet)
	{
	    cvs_output ("D ", 2);
	    cvs_output (finfo->fullname, 0);
	    cvs_output ("\n", 1);
	}

	goto free_vars_and_return;
    }

    /*
     * If we are adding a tag, we need to know which version we have checked
     * out and we'll tag that version.
     */
    if (nversion == NULL)
    {
        version = vers->vn_user;
    }
    else
    {
        version = nversion;
    }
    if (version == NULL)
    {
	goto free_vars_and_return;
    }
    else if (strcmp (version, "0") == 0)
    {
	if (!quiet)
	    error (0, 0, "couldn't tag added but un-commited file `%s'", finfo->file);
	goto free_vars_and_return;
    }
    else if (version[0] == '-')
    {
	if (!quiet)
	    error (0, 0, "skipping removed but un-commited file `%s'", finfo->file);
	goto free_vars_and_return;
    }
    else if (vers->srcfile == NULL)
    {
	if (!quiet)
	    error (0, 0, "cannot find revision control file for `%s'", finfo->file);
	goto free_vars_and_return;
    }

    /*
     * As an enhancement for the case where a tag is being re-applied to a
     * large number of files, make one extra call to RCS_getversion to see
     * if the tag is already set in the RCS file.  If so, check to see if it
     * needs to be moved.  If not, do nothing.  This will likely save a lot of
     * time when simply moving the tag to the "current" head revisions of a
     * module -- which I have found to be a typical tagging operation.
     */
    rev = branch_mode ? RCS_magicrev (vers->srcfile, version) : version;
    oversion = RCS_getversion (vers->srcfile, symtag, (char *) NULL, 1,
			       (int *) NULL);
    if (oversion != NULL)
    {
	int isbranch = RCS_nodeisbranch (finfo->rcs, symtag);

	/*
	 * if versions the same and neither old or new are branches don't have 
	 * to do anything
	 */
	if (strcmp (version, oversion) == 0 && !branch_mode && !isbranch)
	{
	    free (oversion);
	    if (branch_mode)
		free (rev);
	    goto free_vars_and_return;
	}

	if (!force_tag_move)
	{
	    /* we're NOT going to move the tag */
	    cvs_output ("W ", 2);
	    cvs_output (finfo->fullname, 0);
	    cvs_output (" : ", 0);
	    cvs_output (symtag, 0);
	    cvs_output (" already exists on ", 0);
	    cvs_output (isbranch ? "branch" : "version", 0);
	    cvs_output (" ", 0);
	    cvs_output (oversion, 0);
	    cvs_output (" : NOT MOVING tag to ", 0);
	    cvs_output (branch_mode ? "branch" : "version", 0);
	    cvs_output (" ", 0);
	    cvs_output (rev, 0);
	    cvs_output ("\n", 1);
	    free (oversion);
	    if (branch_mode)
		free (rev);
	    goto free_vars_and_return;
	}
	else 	/* force_tag_move == 1 and... */
		if ((isbranch && !disturb_branch_tags) ||
		    (!isbranch && disturb_branch_tags))
	{
	    error(0,0, "%s: Not moving %s tag `%s' from %s to %s%s.", 
		    finfo->fullname,
		    isbranch ? "branch" : "non-branch",
		    symtag, oversion, rev,
		    isbranch ? "" : " due to `-B' option"); 
	    free (oversion);
	    if (branch_mode)
		free (rev);
	    goto free_vars_and_return;
	}
	free (oversion);
    }

    if ((retcode = RCS_settag(vers->srcfile, symtag, rev)) != 0)
    {
	error (1, retcode == -1 ? errno : 0,
	       "failed to set tag %s to revision %s in %s",
	       symtag, rev, vers->srcfile->path);
	if (branch_mode)
	    free (rev);
	retval = 1;
	goto free_vars_and_return;
    }
    if (branch_mode)
	free (rev);
    RCS_rewrite (vers->srcfile, NULL, NULL);

    /* more warm fuzzies */
    if (!really_quiet)
    {
	cvs_output ("T ", 2);
	cvs_output (finfo->fullname, 0);
	cvs_output ("\n", 1);
    }

 free_vars_and_return:
    if (nversion != NULL)
        free (nversion);
    freevers_ts (&vers);
    return (retval);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
tag_dirproc (callerdat, dir, repos, update_dir, entries)
    void *callerdat;
    const char *dir;
    const char *repos;
    const char *update_dir;
    List *entries;
{

    if (ignore_directory (update_dir))
    {
	/* print the warm fuzzy message */
	if (!quiet)
	  error (0, 0, "Ignoring %s", update_dir);
        return R_SKIP_ALL;
    }

    if (!quiet)
	error (0, 0, "%s %s", delete_flag ? "Untagging" : "Tagging", update_dir);
    return (R_PROCESS);
}

/* Code relating to the val-tags file.  Note that this file has no way
   of knowing when a tag has been deleted.  The problem is that there
   is no way of knowing whether a tag still exists somewhere, when we
   delete it some places.  Using per-directory val-tags files (in
   CVSREP) might be better, but that might slow down the process of
   verifying that a tag is correct (maybe not, for the likely cases,
   if carefully done), and/or be harder to implement correctly.  */

struct val_args {
    char *name;
    int found;
};

static int val_fileproc PROTO ((void *callerdat, struct file_info *finfo));

static int
val_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    RCSNode *rcsdata;
    struct val_args *args = (struct val_args *)callerdat;
    char *tag;

    if ((rcsdata = finfo->rcs) == NULL)
	/* Not sure this can happen, after all we passed only
	   W_REPOS | W_ATTIC.  */
	return 0;

    tag = RCS_gettag (rcsdata, args->name, 1, (int *) NULL);
    if (tag != NULL)
    {
	/* FIXME: should find out a way to stop the search at this point.  */
	args->found = 1;
	free (tag);
    }
    return 0;
}



/* This routine determines whether a tag appears in CVSROOT/val-tags.
 *
 * The val-tags file will be open read-only when IDB is NULL.  Since writes to
 * val-tags always append to it, the lack of locking is okay.  The worst case
 * race condition might misinterpret a partially written "foobar" matched, for
 * instance,  a request for "f", "foo", of "foob".  Such a mismatch would be
 * caught harmlessly later.
 *
 * Before CVS adds a tag to val-tags, it will lock val-tags for write and
 * verify that the tag is still not present to avoid adding it twice.
 *
 * NOTES
 *   This function expects its parent to handle any necessary locking of the
 *   val-tags file.
 *
 * INPUTS
 *   idb	When this value is NULL, the val-tags file is opened in
 *   		in read-only mode.  When present, the val-tags file is opened
 *   		in read-write mode and the DBM handle is stored in *IDB.
 *   name	The tag to search for.
 *
 * OUTPUTS
 *   *idb	The val-tags file opened for read/write, or NULL if it couldn't
 *   		be opened.
 *
 * ERRORS
 *   Exits with an error message if the val-tags file cannot be opened for
 *   read (failure to open val-tags read/write is harmless - see below).
 *
 * RETURNS
 *   true	1. If NAME exists in val-tags.
 *   		2. If IDB is non-NULL and val-tags cannot be opened for write.
 *   		   This allows callers to ignore the harmless inability to
 *   		   update the val-tags cache.
 *   false	If the file could be opened and the tag is not present.
 */
static int is_in_val_tags PROTO((DBM **idb, const char *name));
static int
is_in_val_tags (idb, name)
    DBM **idb;
    const char *name;
{
    DBM *db = NULL;
    char *valtags_filename;
    datum mytag;
    int status;

    /* Casting out const should be safe here - input datums are not
     * written to by the myndbm functions.
     */
    mytag.dptr = (char *)name;
    mytag.dsize = strlen (name);

    valtags_filename = xmalloc (strlen (current_parsed_root->directory)
				+ sizeof CVSROOTADM
				+ sizeof CVSROOTADM_VALTAGS + 3);
    sprintf (valtags_filename, "%s/%s/%s", current_parsed_root->directory,
					   CVSROOTADM, CVSROOTADM_VALTAGS);

    if (idb)
    {
	db = dbm_open (valtags_filename, O_RDWR, 0666);
	if (!db)
	{
	    mode_t omask;

	    if (!existence_error (errno))
	    {
		error (0, errno, "warning: cannot open %s read/write",
		       valtags_filename);
		*idb = NULL;
		return 1;
	    }

	    omask = umask (cvsumask);
	    db = dbm_open (valtags_filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
	    umask (omask);
	    if (!db)
	    {
		error (0, errno, "warning: cannot create %s",
		       valtags_filename);
		*idb = NULL;
		return 1;
	    }

	    *idb = db;
	    return 0;
	}

	*idb = db;
    }
    else
    {
	db = dbm_open (valtags_filename, O_RDONLY, 0444);
	if (!db && !existence_error (errno))
	    error (1, errno, "cannot read %s", valtags_filename);
    }

    /* If the file merely fails to exist, we just keep going and create
       it later if need be.  */

    status = 0;
    if (db)
    {
	datum val;

	val = dbm_fetch (db, mytag);
	if (val.dptr != NULL)
	    /* Found.  The tag is valid.  */
	    status = 1;

	/* FIXME: should check errors somehow (add dbm_error to myndbm.c?).  */

	if (!idb) dbm_close (db);
    }

    free (valtags_filename);
    return status;
}



/* Add a tag to the CVSROOT/val-tags cache.  Establishes a write lock and
 * reverifies that the tag does not exist before adding it.
 */
static void add_to_val_tags PROTO((const char *name));
static void
add_to_val_tags (name)
    const char *name;
{
    DBM *db;
    datum mytag;
    datum value;

    if (noexec) return;

    val_tags_lock (current_parsed_root->directory);

    /* Check for presence again since we have a lock now.  */
    if (is_in_val_tags (&db, name))
    {
	clear_val_tags_lock ();
	if (db)
	    dbm_close (db);
	return;
    }

    /* Casting out const should be safe here - input datums are not
     * written to by the myndbm functions.
     */
    mytag.dptr = (char *)name;
    mytag.dsize = strlen (name);
    value.dptr = "y";
    value.dsize = 1;

    if (dbm_store (db, mytag, value, DBM_REPLACE) < 0)
	error (0, errno, "failed to store %s into val-tags", name);
    dbm_close (db);

    clear_val_tags_lock ();
}



static Dtype val_direntproc PROTO ((void *, const char *, const char *,
                                    const char *, List *));

static Dtype
val_direntproc (callerdat, dir, repository, update_dir, entries)
    void *callerdat;
    const char *dir;
    const char *repository;
    const char *update_dir;
    List *entries;
{
    /* This is not quite right--it doesn't get right the case of "cvs
       update -d -r foobar" where foobar is a tag which exists only in
       files in a directory which does not exist yet, but which is
       about to be created.  */
    if (isdir (dir))
	return R_PROCESS;
    return R_SKIP_ALL;
}

/* Check to see whether NAME is a valid tag.  If so, return.  If not
   print an error message and exit.  ARGC, ARGV, LOCAL, and AFLAG specify
   which files we will be operating on.

   REPOSITORY is the repository if we need to cd into it, or NULL if
   we are already there, or "" if we should do a W_LOCAL recursion.
   Sorry for three cases, but the "" case is needed in case the
   working directories come from diverse parts of the repository, the
   NULL case avoids an unneccesary chdir, and the non-NULL, non-""
   case is needed for checkout, where we don't want to chdir if the
   tag is found in CVSROOTADM_VALTAGS, but there is not (yet) any
   local directory.  */
void
tag_check_valid (name, argc, argv, local, aflag, repository)
    char *name;
    int argc;
    char **argv;
    int local;
    int aflag;
    char *repository;
{
    struct val_args the_val_args;
    struct saved_cwd cwd;
    int which;

    /* Numeric tags require only a syntactic check.  */
    if (isdigit ((unsigned char) name[0]))
    {
	char *p;
	for (p = name; *p != '\0'; ++p)
	{
	    if (!(isdigit ((unsigned char) *p) || *p == '.'))
		error (1, 0, "\
Numeric tag %s contains characters other than digits and '.'", name);
	}
	return;
    }

    /* Special tags are always valid.  */
    if (strcmp (name, TAG_BASE) == 0
	|| strcmp (name, TAG_HEAD) == 0)
	return;

    if (readonlyfs)
	return;

    /* Verify that the tag is valid syntactically.  Some later code once made
     * assumptions about this.
     */
    RCS_check_tag (name);

    if (is_in_val_tags (NULL, name)) return;

    /* We didn't find the tag in val-tags, so look through all the RCS files
       to see whether it exists there.  Yes, this is expensive, but there
       is no other way to cope with a tag which might have been created
       by an old version of CVS, from before val-tags was invented.

       Since we need this code anyway, we also use it to create
       entries in val-tags in general (that is, the val-tags entry
       will get created the first time the tag is used, not when the
       tag is created).  */

    the_val_args.name = name;
    the_val_args.found = 0;

    which = W_REPOS | W_ATTIC;

    if (repository != NULL)
    {
	if (repository[0] == '\0')
	    which |= W_LOCAL;
	else
	{
	    if (save_cwd (&cwd))
		error_exit ();
	    if (CVS_CHDIR (repository) < 0)
		error (1, errno, "cannot change to %s directory", repository);
	}
    }

    start_recursion (val_fileproc, (FILESDONEPROC) NULL,
		     val_direntproc, (DIRLEAVEPROC) NULL,
		     (void *)&the_val_args,
		     argc, argv, local, which, aflag,
		     CVS_LOCK_READ, NULL, 1, repository);
    if (repository != NULL && repository[0] != '\0')
    {
	if (restore_cwd (&cwd, NULL))
	    error_exit ();
	free_cwd (&cwd);
    }

    if (!the_val_args.found)
	error (1, 0, "no such tag %s", name);
    else
	/* The tags is valid but not mentioned in val-tags.  Add it.  */
	add_to_val_tags (name);
}



/*
 * Check whether a join tag is valid.  This is just like
 * tag_check_valid, but we must stop before the colon if there is one.
 */

void
tag_check_valid_join (join_tag, argc, argv, local, aflag, repository)
     char *join_tag;
     int argc;
     char **argv;
     int local;
     int aflag;
     char *repository;
{
    char *c, *s;

    c = xstrdup (join_tag);
    s = strchr (c, ':');
    if (s != NULL)
    {
        if (isdigit ((unsigned char) join_tag[0]))
	    error (1, 0,
		   "Numeric join tag %s may not contain a date specifier",
		   join_tag);

        *s = '\0';
	/* hmmm...  I think it makes sense to allow -j:<date>, but
	 * for now this fixes a bug where CVS just spins and spins (I
	 * think in the RCS code) looking for a zero length tag.
	 */
	if (!*c)
	    error (1, 0,
		   "argument to join may not contain a date specifier without a tag");
    }

    tag_check_valid (c, argc, argv, local, aflag, repository);

    free (c);
}
