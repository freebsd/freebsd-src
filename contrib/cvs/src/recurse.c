/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * General recursion handler
 * 
 */

#include "cvs.h"
#include "savecwd.h"
#include "fileattr.h"
#include "edit.h"

static int do_dir_proc PROTO((Node * p, void *closure));
static int do_file_proc PROTO((Node * p, void *closure));
static void addlist PROTO((List ** listp, char *key));
static int unroll_files_proc PROTO((Node *p, void *closure));
static void addfile PROTO((List **listp, char *dir, char *file));


/*
 * Local static versions eliminates the need for globals
 */
static FILEPROC fileproc;
static FILESDONEPROC filesdoneproc;
static DIRENTPROC direntproc;
static DIRLEAVEPROC dirleaveproc;
static int which;
static Dtype flags;
static int aflag;
static int readlock;
static int dosrcs;
static char update_dir[PATH_MAX];
static char *repository = NULL;
static List *filelist = NULL; /* holds list of files on which to operate */
static List *dirlist = NULL; /* holds list of directories on which to operate */

struct recursion_frame {
  FILEPROC fileproc;
  FILESDONEPROC filesdoneproc;
  DIRENTPROC direntproc;
  DIRLEAVEPROC dirleaveproc;
  Dtype flags;
  int which;
  int aflag;
  int readlock;
  int dosrcs;
};

/*
 * Called to start a recursive command.
 *
 * Command line arguments dictate the directories and files on which
 * we operate.  In the special case of no arguments, we default to
 * ".".
 *
 * The general algorithm is as follows.
 */
int
start_recursion (fileproc, filesdoneproc, direntproc, dirleaveproc,
		 argc, argv, local, which, aflag, readlock,
		 update_preload, dosrcs, wd_is_repos)
    FILEPROC fileproc;
    FILESDONEPROC filesdoneproc;
    DIRENTPROC 	direntproc;
    DIRLEAVEPROC dirleaveproc;
    int argc;
    char **argv;
    int local;
    int which;
    int aflag;
    int readlock;
    char *update_preload;
    int dosrcs;
    int wd_is_repos;	/* Set if caller has already cd'd to the repository */
{
    int i, err = 0;
    Dtype flags;
    List *files_by_dir = NULL;
    struct recursion_frame frame;

    expand_wild (argc, argv, &argc, &argv);

    if (update_preload == NULL)
	update_dir[0] = '\0';
    else
	(void) strcpy (update_dir, update_preload);

    if (local)
	flags = R_SKIP_DIRS;
    else
	flags = R_PROCESS;

    /* clean up from any previous calls to start_recursion */
    if (repository)
    {
	free (repository);
	repository = (char *) NULL;
    }
    if (filelist)
	dellist (&filelist); /* FIXME-krp: no longer correct. */
/* FIXME-krp: clean up files_by_dir */
    if (dirlist)
	dellist (&dirlist);

    if (argc == 0)
    {

	/*
	 * There were no arguments, so we'll probably just recurse. The
	 * exception to the rule is when we are called from a directory
	 * without any CVS administration files.  That has always meant to
	 * process each of the sub-directories, so we pretend like we were
	 * called with the list of sub-dirs of the current dir as args
	 */
	if ((which & W_LOCAL) && !isdir (CVSADM))
	    dirlist = Find_Directories ((char *) NULL, W_LOCAL);
	else
	    addlist (&dirlist, ".");

	err += do_recursion (fileproc, filesdoneproc, direntproc,
			    dirleaveproc, flags, which, aflag,
			    readlock, dosrcs);
	return(err);
    }


    /*
     * There were arguments, so we have to handle them by hand. To do
     * that, we set up the filelist and dirlist with the arguments and
     * call do_recursion.  do_recursion recognizes the fact that the
     * lists are non-null when it starts and doesn't update them.
     *
     * explicitly named directories are stored in dirlist.
     * explicitly named files are stored in filelist.
     * other possibility is named entities whicha are not currently in
     * the working directory.
     */
    
    for (i = 0; i < argc; i++)
    {
	/* if this argument is a directory, then add it to the list of
	   directories. */

	if (!wrap_name_has (argv[i], WRAP_TOCVS) && isdir (argv[i]))
	    addlist (&dirlist, argv[i]);
	else
	{
	    /* otherwise, split argument into directory and component names. */
	    char *dir;
	    char *comp;
	    char tmp[PATH_MAX];
	    char *file_to_try;

	    /* Now break out argv[i] into directory part (DIR) and file part (COMP).
		   DIR and COMP will each point to a newly malloc'd string.  */
	    dir = xstrdup (argv[i]);
	    comp = last_component (dir);
	    if (comp == dir)
	    {
		/* no dir component.  What we have is an implied "./" */
		dir = xstrdup(".");
	    }
	    else
	    {
		char *p = comp;

		p[-1] = '\0';
		comp = xstrdup (p);
	    }

	    /* if this argument exists as a file in the current
	       working directory tree, then add it to the files list.  */

	    if (wd_is_repos)
	    {
		/* If doing rtag, we've done a chdir to the repository. */
		sprintf (tmp, "%s%s", argv[i], RCSEXT);
		file_to_try = tmp;
	    }
	    else
	      file_to_try = argv[i];

	    if(isfile(file_to_try))
		addfile (&files_by_dir, dir, comp);
	    else if (isdir (dir))
	    {
		if (isdir (CVSADM))
		{
		    /* otherwise, look for it in the repository. */
		    char *save_update_dir;
		    char *repos;
		
		    /* save & set (aka push) update_dir */
		    save_update_dir = xstrdup (update_dir);

		    if (*update_dir != '\0')
			(void) strcat (update_dir, "/");

		    (void) strcat (update_dir, dir);
		
		    /* look for it in the repository. */
		    repos = Name_Repository (dir, update_dir);
		    (void) sprintf (tmp, "%s/%s", repos, comp);
		    free (repos);

		    if (!wrap_name_has (comp, WRAP_TOCVS) && isdir(tmp))
			addlist (&dirlist, argv[i]);
		    else
			addfile (&files_by_dir, dir, comp);

		    (void) sprintf (update_dir, "%s", save_update_dir);
		    free (save_update_dir);
		}
		else
		    addfile (&files_by_dir, dir, comp);
	    }
	    else
		error (1, 0, "no such directory `%s'", dir);

	    free (dir);
	    free (comp);
	}
    }

    /* At this point we have looped over all named arguments and built
       a coupla lists.  Now we unroll the lists, setting up and
       calling do_recursion. */

    frame.fileproc = fileproc;
    frame.filesdoneproc = filesdoneproc;
    frame.direntproc = direntproc;
    frame.dirleaveproc = dirleaveproc;
    frame.flags = flags;
    frame.which = which;
    frame.aflag = aflag;
    frame.readlock = readlock;
    frame.dosrcs = dosrcs;
    err += walklist (files_by_dir, unroll_files_proc, (void *) &frame);

    /* then do_recursion on the dirlist. */
    if (dirlist != NULL)
	err += do_recursion (frame.fileproc, frame.filesdoneproc,
			     frame.direntproc, frame.dirleaveproc,
			     frame.flags, frame.which, frame.aflag,
			     frame.readlock, frame.dosrcs);

    /* Free the data which expand_wild allocated.  */
    for (i = 0; i < argc; ++i)
	free (argv[i]);
    free (argv);

    return (err);
}

/*
 * Implement the recursive policies on the local directory.  This may be
 * called directly, or may be called by start_recursion
 */
int
do_recursion (xfileproc, xfilesdoneproc, xdirentproc, xdirleaveproc,
	      xflags, xwhich, xaflag, xreadlock, xdosrcs)
    FILEPROC xfileproc;
    FILESDONEPROC xfilesdoneproc;
    DIRENTPROC xdirentproc;
    DIRLEAVEPROC xdirleaveproc;
    Dtype xflags;
    int xwhich;
    int xaflag;
    int xreadlock;
    int xdosrcs;
{
    int err = 0;
    int dodoneproc = 1;
    char *srepository;
    List *entries = NULL;

    /* do nothing if told */
    if (xflags == R_SKIP_ALL)
	return (0);

    /* set up the static vars */
    fileproc = xfileproc;
    filesdoneproc = xfilesdoneproc;
    direntproc = xdirentproc;
    dirleaveproc = xdirleaveproc;
    flags = xflags;
    which = xwhich;
    aflag = xaflag;
    readlock = noexec ? 0 : xreadlock;
    dosrcs = xdosrcs;

    /* The fact that locks are not active here is what makes us fail to have
       the

           If someone commits some changes in one cvs command,
	   then an update by someone else will either get all the
	   changes, or none of them.

       property (see node Concurrency in cvs.texinfo).

       The most straightforward fix would just to readlock the whole
       tree before starting an update, but that means that if a commit
       gets blocked on a big update, it might need to wait a *long*
       time.

       A more adequate fix would be a two-pass design for update,
       checkout, etc.  The first pass would go through the repository,
       with the whole tree readlocked, noting what versions of each
       file we want to get.  The second pass would release all locks
       (except perhaps short-term locks on one file at a
       time--although I think RCS already deals with this) and
       actually get the files, specifying the particular versions it wants.

       This could be sped up by separating out the data needed for the
       first pass into a separate file(s)--for example a file
       attribute for each file whose value contains the head revision
       for each branch.  The structure should be designed so that
       commit can relatively quickly update the information for a
       single file or a handful of files (file attributes, as
       implemented in Jan 96, are probably acceptable; improvements
       would be possible such as branch attributes which are in
       separate files for each branch).  */

#if defined(SERVER_SUPPORT) && defined(SERVER_FLOWCONTROL)
    /*
     * Now would be a good time to check to see if we need to stop
     * generating data, to give the buffers a chance to drain to the
     * remote client.  We should not have locks active at this point.
     */
    if (server_active
	/* If there are writelocks around, we cannot pause here.  */
	&& (readlock || noexec))
	server_pause_check();
#endif

    /*
     * Fill in repository with the current repository
     */
    if (which & W_LOCAL)
    {
	if (isdir (CVSADM))
	    repository = Name_Repository ((char *) NULL, update_dir);
	else
	    repository = NULL;
    }
    else
    {
	repository = xmalloc (PATH_MAX);
	(void) getwd (repository);
    }
    srepository = repository;		/* remember what to free */

    fileattr_startdir (repository);

    /*
     * The filesdoneproc needs to be called for each directory where files
     * processed, or each directory that is processed by a call where no
     * directories were passed in.  In fact, the only time we don't want to
     * call back the filesdoneproc is when we are processing directories that
     * were passed in on the command line (or in the special case of `.' when
     * we were called with no args
     */
    if (dirlist != NULL && filelist == NULL)
	dodoneproc = 0;

    /*
     * If filelist or dirlist is already set, we don't look again. Otherwise,
     * find the files and directories
     */
    if (filelist == NULL && dirlist == NULL)
    {
	/* both lists were NULL, so start from scratch */
	if (fileproc != NULL && flags != R_SKIP_FILES)
	{
	    int lwhich = which;

	    /* be sure to look in the attic if we have sticky tags/date */
	    if ((lwhich & W_ATTIC) == 0)
		if (isreadable (CVSADM_TAG))
		    lwhich |= W_ATTIC;

	    /* find the files and fill in entries if appropriate */
	    filelist = Find_Names (repository, lwhich, aflag, &entries);
	}

	/* find sub-directories if we will recurse */
	if (flags != R_SKIP_DIRS)
	    dirlist = Find_Directories (repository, which);
    }
    else
    {
	/* something was passed on the command line */
	if (filelist != NULL && fileproc != NULL)
	{
	    /* we will process files, so pre-parse entries */
	    if (which & W_LOCAL)
		entries = Entries_Open (aflag);
	}
    }

    /* process the files (if any) */
    if (filelist != NULL && fileproc)
    {
	struct file_info finfo_struct;

	/* read lock it if necessary */
	if (readlock && repository && Reader_Lock (repository) != 0)
	    error (1, 0, "read lock failed - giving up");

#ifdef CLIENT_SUPPORT
	/* For the server, we handle notifications in a completely different
	   place (server_notify).  For local, we can't do them here--we don't
	   have writelocks in place, and there is no way to get writelocks
	   here.  */
	if (client_active)
	    notify_check (repository, update_dir);
#endif /* CLIENT_SUPPORT */

	finfo_struct.repository = repository;
	finfo_struct.update_dir = update_dir;
	finfo_struct.entries = entries;
	/* do_file_proc will fill in finfo_struct.file.  */

	/* process the files */
	err += walklist (filelist, do_file_proc, &finfo_struct);

	/* unlock it */
	if (readlock)
	    Lock_Cleanup ();

	/* clean up */
	dellist (&filelist);
    }

    if (entries) 
    {
	Entries_Close (entries);
	entries = NULL;
    }

    /* call-back files done proc (if any) */
    if (dodoneproc && filesdoneproc != NULL)
	err = filesdoneproc (err, repository, update_dir[0] ? update_dir : ".");

    fileattr_write ();
    fileattr_free ();

    /* process the directories (if necessary) */
    if (dirlist != NULL)
	err += walklist (dirlist, do_dir_proc, NULL);
#ifdef notdef
    else if (dirleaveproc != NULL)
	err += dirleaveproc(".", err, ".");
#endif
    dellist (&dirlist);

    /* free the saved copy of the pointer if necessary */
    if (srepository)
    {
	free (srepository);
	repository = (char *) NULL;
    }

    return (err);
}

/*
 * Process each of the files in the list with the callback proc
 */
static int
do_file_proc (p, closure)
    Node *p;
    void *closure;
{
    struct file_info *finfo = (struct file_info *)closure;
    int ret;

    finfo->file = p->key;
    finfo->fullname = xmalloc (strlen (finfo->file)
			       + strlen (finfo->update_dir)
			       + 2);
    finfo->fullname[0] = '\0';
    if (finfo->update_dir[0] != '\0')
    {
	strcat (finfo->fullname, finfo->update_dir);
	strcat (finfo->fullname, "/");
    }
    strcat (finfo->fullname, finfo->file);

    if (dosrcs && repository)
	finfo->rcs = RCS_parse (finfo->file, repository);
    else 
        finfo->rcs = (RCSNode *) NULL;
    ret = fileproc (finfo);

    freercsnode(&finfo->rcs);
    free (finfo->fullname);

    return (ret);
}

/*
 * Process each of the directories in the list (recursing as we go)
 */
static int
do_dir_proc (p, closure)
    Node *p;
    void *closure;
{
    char *dir = p->key;
    char newrepos[PATH_MAX];
    List *sdirlist;
    char *srepository;
    char *cp;
    Dtype dir_return = R_PROCESS;
    int stripped_dot = 0;
    int err = 0;
    struct saved_cwd cwd;

    /* set up update_dir - skip dots if not at start */
    if (strcmp (dir, ".") != 0)
    {
	if (update_dir[0] != '\0')
	{
	    (void) strcat (update_dir, "/");
	    (void) strcat (update_dir, dir);
	}
	else
	    (void) strcpy (update_dir, dir);

	/*
	 * Here we need a plausible repository name for the sub-directory. We
	 * create one by concatenating the new directory name onto the
	 * previous repository name.  The only case where the name should be
	 * used is in the case where we are creating a new sub-directory for
	 * update -d and in that case the generated name will be correct.
	 */
	if (repository == NULL)
	    newrepos[0] = '\0';
	else
	    (void) sprintf (newrepos, "%s/%s", repository, dir);
    }
    else
    {
	if (update_dir[0] == '\0')
	    (void) strcpy (update_dir, dir);

	if (repository == NULL)
	    newrepos[0] = '\0';
	else
	    (void) strcpy (newrepos, repository);
    }

    /* call-back dir entry proc (if any) */
    if (direntproc != NULL)
	dir_return = direntproc (dir, newrepos, update_dir);

    /* only process the dir if the return code was 0 */
    if (dir_return != R_SKIP_ALL)
    {
	/* save our current directory and static vars */
        if (save_cwd (&cwd))
	    exit (EXIT_FAILURE);
	sdirlist = dirlist;
	srepository = repository;
	dirlist = NULL;

	/* cd to the sub-directory */
	if (chdir (dir) < 0)
	    error (1, errno, "could not chdir to %s", dir);

	/* honor the global SKIP_DIRS (a.k.a. local) */
	if (flags == R_SKIP_DIRS)
	    dir_return = R_SKIP_DIRS;

	/* remember if the `.' will be stripped for subsequent dirs */
	if (strcmp (update_dir, ".") == 0)
	{
	    update_dir[0] = '\0';
	    stripped_dot = 1;
	}

	/* make the recursive call */
	err += do_recursion (fileproc, filesdoneproc, direntproc, dirleaveproc,
			    dir_return, which, aflag, readlock, dosrcs);

	/* put the `.' back if necessary */
	if (stripped_dot)
	    (void) strcpy (update_dir, ".");

	/* call-back dir leave proc (if any) */
	if (dirleaveproc != NULL)
	    err = dirleaveproc (dir, err, update_dir);

	/* get back to where we started and restore state vars */
	if (restore_cwd (&cwd, NULL))
	    exit (EXIT_FAILURE);
	free_cwd (&cwd);
	dirlist = sdirlist;
	repository = srepository;
    }

    /* put back update_dir */
    cp = last_component (update_dir);
    if (cp > update_dir)
	cp[-1] = '\0';
    else
	update_dir[0] = '\0';

    return (err);
}

/*
 * Add a node to a list allocating the list if necessary.
 */
static void
addlist (listp, key)
    List **listp;
    char *key;
{
    Node *p;

    if (*listp == NULL)
	*listp = getlist ();
    p = getnode ();
    p->type = FILES;
    p->key = xstrdup (key);
    if (addnode (*listp, p) != 0)
	freenode (p);
}

static void
addfile (listp, dir, file)
    List **listp;
    char *dir;
    char *file;
{
    Node *n;

    /* add this dir. */
    addlist (listp, dir);

    n = findnode (*listp, dir);
    if (n == NULL)
    {
	error (1, 0, "can't find recently added dir node `%s' in start_recursion.",
	       dir);
    }

    n->type = DIRS;
    addlist ((List **) &n->data, file);
    return;
}

static int
unroll_files_proc (p, closure)
    Node *p;
    void *closure;
{
    Node *n;
    struct recursion_frame *frame = (struct recursion_frame *) closure;
    int err = 0;
    List *save_dirlist;
    char *save_update_dir = NULL;
    struct saved_cwd cwd;

    /* if this dir was also an explicitly named argument, then skip
       it.  We'll catch it later when we do dirs. */
    n = findnode (dirlist, p->key);
    if (n != NULL)
	return (0);

    /* otherwise, call dorecusion for this list of files. */
    filelist = (List *) p->data;
    save_dirlist = dirlist;
    dirlist = NULL;

    if (strcmp(p->key, ".") != 0)
    {
        if (save_cwd (&cwd))
	    exit (EXIT_FAILURE);
	if (chdir (p->key) < 0)
	    error (1, errno, "could not chdir to %s", p->key);

	save_update_dir = xstrdup (update_dir);

	if (*update_dir != '\0')
	    (void) strcat (update_dir, "/");

	(void) strcat (update_dir, p->key);
    }

    err += do_recursion (frame->fileproc, frame->filesdoneproc,
			 frame->direntproc, frame->dirleaveproc,
			 frame->flags, frame->which, frame->aflag,
			 frame->readlock, frame->dosrcs);

    if (save_update_dir != NULL)
    {
	(void) strcpy (update_dir, save_update_dir);
	free (save_update_dir);

	if (restore_cwd (&cwd, NULL))
	    exit (EXIT_FAILURE);
	free_cwd (&cwd);
    }

    dirlist = save_dirlist;
    filelist = NULL;
    return(err);
}
