/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Tag
 * 
 * Add or delete a symbolic name to an RCS file, or a collection of RCS files.
 * Uses the checked out revision in the current directory.
 */

#include "cvs.h"

#ifndef lint
static const char rcsid[] = "$CVSid: @(#)tag.c 1.60 94/09/30 $";
USE(rcsid);
#endif

static int check_fileproc PROTO((char *file, char *update_dir,
     			 char *repository, List * entries,
			 List * srcfiles));
static int check_filesdoneproc PROTO((int err, char *repos, char *update_dir));
static int pretag_proc PROTO((char *repository, char *filter));
static void masterlist_delproc PROTO((Node *p));
static void tag_delproc PROTO((Node *p));
static int pretag_list_proc PROTO((Node *p, void *closure));

static Dtype tag_dirproc PROTO((char *dir, char *repos, char *update_dir));
static int tag_fileproc PROTO((char *file, char *update_dir,
			 char *repository, List * entries,
			 List * srcfiles));

static char *numtag;
static char *date = NULL;
static char *symtag;
static int delete;			/* adding a tag by default */
static int branch_mode;			/* make an automagic "branch" tag */
static int local;			/* recursive by default */
static int force_tag_match = 1;         /* force tag to match by default */
static int force_tag_move;		/* don't force tag to move by default */

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

static const char *const tag_usage[] =
{
    "Usage: %s %s [-lRF] [-b] [-d] tag [files...]\n",
    "\t-l\tLocal directory only, not recursive.\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-d\tDelete the given Tag.\n",
    "\t-[rD]\tExisting tag or date.\n",
    "\t-f\tForce a head revision if tag etc not found.\n",
    "\t-b\tMake the tag a \"branch\" tag, allowing concurrent development.\n",
    "\t-F\tMove tag if it already exists\n",
    NULL
};

int
tag (argc, argv)
    int argc;
    char **argv;
{
    int c;
    int err = 0;

    if (argc == -1)
	usage (tag_usage);

    optind = 1;
    while ((c = getopt (argc, argv, "FQqlRdr:D:bf")) != -1)
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
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 'd':
		delete = 1;
		break;
            case 'r':
                numtag = optarg;
                break;
            case 'D':
                if (date)
                    free (date);
                date = Make_Date (optarg);
                break;
	    case 'f':
		force_tag_match = 0;
		break;
	    case 'b':
		branch_mode = 1;
		break;
            case 'F':
		force_tag_move = 1;
		break;
	    case '?':
	    default:
		usage (tag_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (argc == 0)
	usage (tag_usage);
    symtag = argv[0];
    argc--;
    argv++;

    if (delete && branch_mode)
	error (0, 0, "warning: -b ignored with -d options");
    RCS_check_tag (symtag);

#ifdef CLIENT_SUPPORT
    if (client_active)
    {
	/* We're the client side.  Fire up the remote server.  */
	start_server ();
	
	ign_setup ();

	if (local)
	    send_arg("-l");
	if (delete)
	    send_arg("-d");
	if (branch_mode)
	    send_arg("-b");
	if (force_tag_move)
	    send_arg("-F");

	send_arg (symtag);

#if 0
	/* FIXME:  We shouldn't have to send current files, but I'm not sure
	   whether it works.  So send the files --
	   it's slower but it works.  */
	send_file_names (argc, argv);
#else
	send_files (argc, argv, local, 0);
#endif
	if (fprintf (to_server, "tag\n") < 0)
	    error (1, errno, "writing to server");
        return get_responses_and_close ();
    }
#endif

    /* check to make sure they are authorized to tag all the 
       specified files in the repository */

    mtlist = getlist();
    err = start_recursion (check_fileproc, check_filesdoneproc,
                           (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL,
                           argc, argv, local, W_LOCAL, 0, 1,
                           (char *) NULL, 1, 0);
    
    if (err)
    {
       error (1, 0, "correct the above errors first!");
    }
     
    /* start the recursion processor */
    err = start_recursion (tag_fileproc, (FILESDONEPROC) NULL, tag_dirproc,
			   (DIRLEAVEPROC) NULL, argc, argv, local,
			   W_LOCAL, 0, 1, (char *) NULL, 1, 0);
    dellist(&mtlist);
    return (err);
}

/* check file that is to be tagged */
/* All we do here is add it to our list */

static int
check_fileproc(file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List * entries;
    List * srcfiles;
{
    char *xdir;
    Node *p;
    Vers_TS *vers;
    
    if (update_dir[0] == '\0')
	xdir = ".";
    else
	xdir = update_dir;
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
    p->key = xstrdup (file);
    p->type = UPDATE;
    p->delproc = tag_delproc;
    vers = Version_TS (repository, (char *) NULL, (char *) NULL, (char *) NULL,
		       file, 0, 0, entries, srcfiles);
    p->data = RCS_getversion(vers->srcfile, numtag, date, force_tag_match, 0);
    if (p->data != NULL)
    {
        int addit = 1;
        char *oversion;
        
        oversion = RCS_getversion (vers->srcfile, symtag, (char *) NULL, 1, 0);
        if (oversion == NULL) 
        {
            if (delete)
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
    freevers_ts(&vers);
    (void) addnode (tlist, p);
    return (0);
}
                         
static int
check_filesdoneproc(err, repos, update_dir)
    int err;
    char *repos;
    char *update_dir;
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
    run_setup("%s %s %s %s",
              filter,
              symtag,
              delete ? "del" : force_tag_move ? "mov" : "add",
              repository);
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
 * Called to tag a particular file (the currently checked out version is
 * tagged with the specified tag - or the specified tag is deleted).
 */
/* ARGSUSED */
static int
tag_fileproc (file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List *entries;
    List *srcfiles;
{
    char *version, *oversion;
    char *nversion = NULL;
    char *rev;
    Vers_TS *vers;
    int retcode = 0;

    vers = Version_TS (repository, (char *) NULL, (char *) NULL, (char *) NULL,
		       file, 0, 0, entries, srcfiles);

    if ((numtag != NULL) || (date != NULL))
    {
        nversion = RCS_getversion(vers->srcfile,
                                  numtag,
                                  date,
                                  force_tag_match, 0);
        if (nversion == NULL)
        {
	    freevers_ts (&vers);
            return (0);
        }
    }
    if (delete)
    {

	/*
	 * If -d is specified, "force_tag_match" is set, so that this call to
	 * Version_Number() will return a NULL version string if the symbolic
	 * tag does not exist in the RCS file.
	 * 
	 * This is done here because it's MUCH faster than just blindly calling
	 * "rcs" to remove the tag... trust me.
	 */

	version = RCS_getversion (vers->srcfile, symtag, (char *) NULL, 1, 0);
	if (version == NULL || vers->srcfile == NULL)
	{
	    freevers_ts (&vers);
	    return (0);
	}
	free (version);

	if ((retcode = RCS_deltag(vers->srcfile->path, symtag, 1)) != 0) 
	{
	    if (!quiet)
		error (0, retcode == -1 ? errno : 0,
		       "failed to remove tag %s from %s", symtag,
		       vers->srcfile->path);
	    freevers_ts (&vers);
	    return (1);
	}

	/* warm fuzzies */
	if (!really_quiet)
	{
	    if (update_dir[0])
		(void) printf ("D %s/%s\n", update_dir, file);
	    else
		(void) printf ("D %s\n", file);
	}

	freevers_ts (&vers);
	return (0);
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
	freevers_ts (&vers);
	return (0);
    }
    else if (strcmp (version, "0") == 0)
    {
	if (!quiet)
	    error (0, 0, "couldn't tag added but un-commited file `%s'", file);
	freevers_ts (&vers);
	return (0);
    }
    else if (version[0] == '-')
    {
	if (!quiet)
	    error (0, 0, "skipping removed but un-commited file `%s'", file);
	freevers_ts (&vers);
	return (0);
    }
    else if (vers->srcfile == NULL)
    {
	if (!quiet)
	    error (0, 0, "cannot find revision control file for `%s'", file);
	freevers_ts (&vers);
	return (0);
    }

    /*
     * As an enhancement for the case where a tag is being re-applied to a
     * large number of files, make one extra call to Version_Number to see if
     * the tag is already set in the RCS file.  If so, check to see if it
     * needs to be moved.  If not, do nothing.  This will likely save a lot of
     * time when simply moving the tag to the "current" head revisions of a
     * module -- which I have found to be a typical tagging operation.
     */
    rev = branch_mode ? RCS_magicrev (vers->srcfile, version) : version;
    oversion = RCS_getversion (vers->srcfile, symtag, (char *) NULL, 1, 0);
    if (oversion != NULL)
    {
       int isbranch = RCS_isbranch (file, symtag, srcfiles);

       /*
	* if versions the same and neither old or new are branches don't have 
	* to do anything
	*/
       if (strcmp (version, oversion) == 0 && !branch_mode && !isbranch)
       {
	  free (oversion);
	  freevers_ts (&vers);
	  return (0);
       }
       
       if (!force_tag_move) {		/* we're NOT going to move the tag */
	  if (update_dir[0])
	     (void) printf ("W %s/%s", update_dir, file);
	  else
	     (void) printf ("W %s", file);

	  (void) printf (" : %s already exists on %s %s", 
			 symtag, isbranch ? "branch" : "version", oversion);
	  (void) printf (" : NOT MOVING tag to %s %s\n", 
			 branch_mode ? "branch" : "version", rev);
	  free (oversion);
	  freevers_ts (&vers);
	  return (0);
       }
       free (oversion);
    }

    if ((retcode = RCS_settag(vers->srcfile->path, symtag, rev)) != 0)
    {
	error (1, retcode == -1 ? errno : 0,
	       "failed to set tag %s to revision %s in %s",
	       symtag, rev, vers->srcfile->path);
	freevers_ts (&vers);
	return (1);
    }

    /* more warm fuzzies */
    if (!really_quiet)
    {
	if (update_dir[0])
	    (void) printf ("T %s/%s\n", update_dir, file);
	else
	    (void) printf ("T %s\n", file);
    }

    freevers_ts (&vers);
    if (nversion != NULL)
    {
        free(nversion);
    }
    return (0);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
tag_dirproc (dir, repos, update_dir)
    char *dir;
    char *repos;
    char *update_dir;
{
    if (!quiet)
	error (0, 0, "%s %s", delete ? "Untagging" : "Tagging", update_dir);
    return (R_PROCESS);
}
