/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Rtag
 * 
 * Add or delete a symbolic name to an RCS file, or a collection of RCS files.
 * Uses the modules database, if necessary.
 */

#include "cvs.h"

static int check_fileproc PROTO ((void *callerdat, struct file_info *finfo));
static int check_filesdoneproc PROTO ((void *callerdat, int err,
				       char *repos, char *update_dir,
				       List *entries));
static int pretag_proc PROTO((char *repository, char *filter));
static void masterlist_delproc PROTO((Node *p));
static void tag_delproc PROTO((Node *p));
static int pretag_list_proc PROTO((Node *p, void *closure));

static Dtype rtag_dirproc PROTO ((void *callerdat, char *dir,
				  char *repos, char *update_dir,
				  List *entries));
static int rtag_fileproc PROTO ((void *callerdat, struct file_info *finfo));
static int rtag_filesdoneproc PROTO ((void *callerdat, int err,
				      char *repos, char *update_dir,
				      List *entries));
static int rtag_proc PROTO((int *pargc, char **argv, char *xwhere,
		      char *mwhere, char *mfile, int shorten,
		      int local_specified, char *mname, char *msg));
static int rtag_delete PROTO((RCSNode *rcsfile));


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

static char *symtag;
static char *numtag;
static int numtag_validated = 0;
static int delete_flag;			/* adding a tag by default */
static int attic_too;			/* remove tag from Attic files */
static int branch_mode;			/* make an automagic "branch" tag */
static char *date;
static int local;			/* recursive by default */
static int force_tag_match = 1;		/* force by default */
static int force_tag_move;              /* don't move existing tags by default */

static const char *const rtag_usage[] =
{
    "Usage: %s %s [-aflRnF] [-b] [-d] [-r tag|-D date] tag modules...\n",
    "\t-a\tClear tag from removed files that would not otherwise be tagged.\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, not recursive\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-n\tNo execution of 'tag program'\n",
    "\t-d\tDelete the given Tag.\n",
    "\t-b\tMake the tag a \"branch\" tag, allowing concurrent development.\n",
    "\t-r rev\tExisting revision/tag.\n",
    "\t-D\tExisting date.\n",
    "\t-F\tMove tag if it already exists\n",	
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int
rtag (argc, argv)
    int argc;
    char **argv;
{
    register int i;
    int c;
    DBM *db;
    int run_module_prog = 1;
    int err = 0;

    if (argc == -1)
	usage (rtag_usage);

    optind = 0;
    while ((c = getopt (argc, argv, "+FanfQqlRdbr:D:")) != -1)
    {
	switch (c)
	{
	    case 'a':
		attic_too = 1;
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
	    case 'd':
		delete_flag = 1;
		break;
	    case 'f':
		force_tag_match = 0;
		break;
	    case 'b':
		branch_mode = 1;
		break;
	    case 'r':
		numtag = optarg;
		break;
	    case 'D':
		if (date)
		    free (date);
		date = Make_Date (optarg);
		break;
	    case 'F':
		force_tag_move = 1;
		break;
	    case '?':
	    default:
		usage (rtag_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;
    if (argc < 2)
	usage (rtag_usage);
    symtag = argv[0];
    argc--;
    argv++;

    if (date && numtag)
	error (1, 0, "-r and -D options are mutually exclusive");
    if (delete_flag && branch_mode)
	error (0, 0, "warning: -b ignored with -d options");
    RCS_check_tag (symtag);

#ifdef CLIENT_SUPPORT
    if (client_active)
    {
	/* We're the client side.  Fire up the remote server.  */
	start_server ();

	ign_setup ();

	if (!force_tag_match)
	    send_arg ("-f");
	if (local)
	    send_arg("-l");
	if (delete_flag)
	    send_arg("-d");
	if (branch_mode)
	    send_arg("-b");
	if (force_tag_move)
	    send_arg("-F");
	if (!run_module_prog)
	    send_arg("-n");
	if (attic_too)
	    send_arg("-a");

	if (numtag)
	    option_with_arg ("-r", numtag);
	if (date)
	    client_senddate (date);

	send_arg (symtag);

	{
	    int i;
	    for (i = 0; i < argc; ++i)
		send_arg (argv[i]);
	}

	send_to_server ("rtag\012", 0);
        return get_responses_and_close ();
    }
#endif

    db = open_module ();
    for (i = 0; i < argc; i++)
    {
	/* XXX last arg should be repository, but doesn't make sense here */
	history_write ('T', (delete_flag ? "D" : (numtag ? numtag : 
		       (date ? date : "A"))), symtag, argv[i], "");
	err += do_module (db, argv[i], TAG,
			  delete_flag ? "Untagging" : "Tagging",
			  rtag_proc, (char *) NULL, 0, 0, run_module_prog,
			  symtag);
    }
    close_module (db);
    return (err);
}

/*
 * callback proc for doing the real work of tagging
 */
/* ARGSUSED */
static int
rtag_proc (pargc, argv, xwhere, mwhere, mfile, shorten, local_specified,
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
    /* Begin section which is identical to patch_proc--should this
       be abstracted out somehow?  */
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
    /* End section which is identical to patch_proc.  */

    if (delete_flag || attic_too || (force_tag_match && numtag))
	which = W_REPOS | W_ATTIC;
    else
	which = W_REPOS;

    if (numtag != NULL && !numtag_validated)
    {
	tag_check_valid (numtag, *pargc - 1, argv + 1, local, 0, NULL);
	numtag_validated = 1;
    }

    /* check to make sure they are authorized to tag all the 
       specified files in the repository */

    mtlist = getlist();
    err = start_recursion (check_fileproc, check_filesdoneproc,
                           (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
                           *pargc - 1, argv + 1, local, which, 0, 1,
                           where, 1);
    
    if (err)
    {
       error (1, 0, "correct the above errors first!");
    }
     
    /* start the recursion processor */
    err = start_recursion (rtag_fileproc, rtag_filesdoneproc, rtag_dirproc,
			   (DIRLEAVEPROC) NULL, NULL,
			   *pargc - 1, argv + 1, local,
			   which, 0, 0, where, 1);
    free (where);
    dellist(&mtlist);

    return (err);
}

/* check file that is to be tagged */
/* All we do here is add it to our list */

static int
check_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    char *xdir;
    Node *p;
    Vers_TS *vers;
    
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
	p->data = (char *) ml;
	p->delproc = masterlist_delproc;
	(void) addnode (mtlist, p);
    }
    /* do tlist */
    p = getnode ();
    p->key = xstrdup (finfo->file);
    p->type = UPDATE;
    p->delproc = tag_delproc;
    vers = Version_TS (finfo, NULL, NULL, NULL, 0, 0);
    p->data = RCS_getversion(vers->srcfile, numtag, date, force_tag_match,
			     (int *) NULL);
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
                addit = 0;
            }
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
    char *repos;
    char *update_dir;
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
pretag_proc(repository, filter)
    char *repository;
    char *filter;
{
    if (filter[0] == '/')
    {
        char *s, *cp;

        s = xstrdup(filter);
        for (cp=s; *cp; cp++)
        {
            if (isspace(*cp))
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
    return (run_exec(RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL|RUN_REALLY));
}

static void
masterlist_delproc(p)
    Node *p;
{
    struct master_lists *ml;

    ml = (struct master_lists *)p->data;
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
 * Called to tag a particular file, as appropriate with the options that were
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

    /* Lock the directory if it is not already locked.  We might be
       able to rely on rtag_dirproc for this.  */

    /* It would be nice to provide consistency with respect to
       commits; however CVS lacks the infrastructure to do that (see
       Concurrency in cvs.texinfo and comment in do_recursion).  We
       can and will prevent simultaneous tag operations from
       interfering with each other, by write locking each directory as
       we enter it, and unlocking it as we leave it.  */

    lock_dir_for_write (finfo->repository);

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
    if (numtag && isdigit (*numtag) && strcmp (numtag, version) != 0)
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
    int retcode;

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

/* Clear any lock we may hold on the current directory.  */

static int
rtag_filesdoneproc (callerdat, err, repos, update_dir, entries)
    void *callerdat;
    int err;
    char *repos;
    char *update_dir;
    List *entries;
{
    Lock_Cleanup ();

    return (err);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
rtag_dirproc (callerdat, dir, repos, update_dir, entries)
    void *callerdat;
    char *dir;
    char *repos;
    char *update_dir;
    List *entries;
{
    if (!quiet)
	error (0, 0, "%s %s", delete_flag ? "Untagging" : "Tagging",
	       update_dir);
    return (R_PROCESS);
}



