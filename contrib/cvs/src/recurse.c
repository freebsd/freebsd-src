/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
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

static char *update_dir;
static char *repository = NULL;
static List *filelist = NULL; /* holds list of files on which to operate */
static List *dirlist = NULL; /* holds list of directories on which to operate */

struct recursion_frame {
    FILEPROC fileproc;
    FILESDONEPROC filesdoneproc;
    DIRENTPROC direntproc;
    DIRLEAVEPROC dirleaveproc;
    void *callerdat;
    Dtype flags;
    int which;
    int aflag;
    int readlock;
    int dosrcs;
};

static int do_recursion PROTO ((struct recursion_frame *frame));

/* I am half tempted to shove a struct file_info * into the struct
   recursion_frame (but then we would need to modify or create a
   recursion_frame for each file), or shove a struct recursion_frame *
   into the struct file_info (more tempting, although it isn't completely
   clear that the struct file_info should contain info about recursion
   processor internals).  So instead use this struct.  */

struct frame_and_file {
    struct recursion_frame *frame;
    struct file_info *finfo;
};

/* Similarly, we need to pass the entries list to do_dir_proc.  */

struct frame_and_entries {
    struct recursion_frame *frame;
    List *entries;
};

/* Start a recursive command.

   Command line arguments (ARGC, ARGV) dictate the directories and
   files on which we operate.  In the special case of no arguments, we
   default to ".".  */
int
start_recursion (fileproc, filesdoneproc, direntproc, dirleaveproc, callerdat,
		 argc, argv, local, which, aflag, readlock,
		 update_preload, dosrcs)
    FILEPROC fileproc;
    FILESDONEPROC filesdoneproc;
    DIRENTPROC 	direntproc;
    DIRLEAVEPROC dirleaveproc;
    void *callerdat;

    int argc;
    char **argv;
    int local;

    /* This specifies the kind of recursion.  There are several cases:

       1.  W_LOCAL is not set but W_REPOS or W_ATTIC is.  The current
       directory when we are called must be the repository and
       recursion proceeds according to what exists in the repository.

       2a.  W_LOCAL is set but W_REPOS and W_ATTIC are not.  The
       current directory when we are called must be the working
       directory.  Recursion proceeds according to what exists in the
       working directory, never (I think) consulting any part of the
       repository which does not correspond to the working directory
       ("correspond" == Name_Repository).

       2b.  W_LOCAL is set and so is W_REPOS or W_ATTIC.  This is the
       weird one.  The current directory when we are called must be
       the working directory.  We recurse through working directories,
       but we recurse into a directory if it is exists in the working
       directory *or* it exists in the repository.  If a directory
       does not exist in the working directory, the direntproc must
       either tell us to skip it (R_SKIP_ALL), or must create it (I
       think those are the only two cases).  */
    int which;

    int aflag;
    int readlock;
    char *update_preload;
    int dosrcs;
{
    int i, err = 0;
    List *files_by_dir = NULL;
    struct recursion_frame frame;

    frame.fileproc = fileproc;
    frame.filesdoneproc = filesdoneproc;
    frame.direntproc = direntproc;
    frame.dirleaveproc = dirleaveproc;
    frame.callerdat = callerdat;
    frame.flags = local ? R_SKIP_DIRS : R_PROCESS;
    frame.which = which;
    frame.aflag = aflag;
    frame.readlock = readlock;
    frame.dosrcs = dosrcs;

    expand_wild (argc, argv, &argc, &argv);

    if (update_preload == NULL)
	update_dir = xstrdup ("");
    else
	update_dir = xstrdup (update_preload);

    /* clean up from any previous calls to start_recursion */
    if (repository)
    {
	free (repository);
	repository = (char *) NULL;
    }
    if (filelist)
	dellist (&filelist); /* FIXME-krp: no longer correct. */
    if (dirlist)
	dellist (&dirlist);

#ifdef SERVER_SUPPORT
    if (server_active)
    {
	for (i = 0; i < argc; ++i)
	    server_pathname_check (argv[i]);
    }
#endif

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
	{
	    dirlist = Find_Directories ((char *) NULL, W_LOCAL, (List *) NULL);
	    /* If there are no sub-directories, there is a certain logic in
	       favor of doing nothing, but in fact probably the user is just
	       confused about what directory they are in, or whether they
	       cvs add'd a new directory.  In the case of at least one
	       sub-directory, at least when we recurse into them we
	       notice (hopefully) whether they are under CVS control.  */
	    if (list_isempty (dirlist))
	    {
		if (update_dir[0] == '\0')
		    error (0, 0, "in directory .:");
		else
		    error (0, 0, "in directory %s:", update_dir);
		error (1, 0,
		       "there is no version here; run '%s checkout' first",
		       program_name);
	    }
	}
	else
	    addlist (&dirlist, ".");

	err += do_recursion (&frame);
	goto out;
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

	    if (!(which & W_LOCAL))
	    {
		/* If doing rtag, we've done a chdir to the repository. */
		file_to_try = xmalloc (strlen (argv[i]) + sizeof (RCSEXT) + 5);
		sprintf (file_to_try, "%s%s", argv[i], RCSEXT);
	    }
	    else
		file_to_try = xstrdup (argv[i]);

	    if (isfile (file_to_try))
		addfile (&files_by_dir, dir, comp);
	    else if (isdir (dir))
	    {
		if ((which & W_LOCAL) && isdir (CVSADM)
#ifdef CLIENT_SUPPORT
		    && !client_active
#endif
		    )
		{
		    /* otherwise, look for it in the repository. */
		    char *tmp_update_dir;
		    char *repos;
		    char *reposfile;

		    tmp_update_dir = xmalloc (strlen (update_dir)
					      + strlen (dir)
					      + 5);
		    strcpy (tmp_update_dir, update_dir);

		    if (*tmp_update_dir != '\0')
			(void) strcat (tmp_update_dir, "/");

		    (void) strcat (tmp_update_dir, dir);

		    /* look for it in the repository. */
		    repos = Name_Repository (dir, tmp_update_dir);
		    reposfile = xmalloc (strlen (repos)
					 + strlen (comp)
					 + 5);
		    (void) sprintf (reposfile, "%s/%s", repos, comp);
		    free (repos);

		    if (!wrap_name_has (comp, WRAP_TOCVS) && isdir (reposfile))
			addlist (&dirlist, argv[i]);
		    else
			addfile (&files_by_dir, dir, comp);

		    free (tmp_update_dir);
		    free (reposfile);
		}
		else
		    addfile (&files_by_dir, dir, comp);
	    }
	    else
		error (1, 0, "no such directory `%s'", dir);

	    free (file_to_try);
	    free (dir);
	    free (comp);
	}
    }

    /* At this point we have looped over all named arguments and built
       a coupla lists.  Now we unroll the lists, setting up and
       calling do_recursion. */

    err += walklist (files_by_dir, unroll_files_proc, (void *) &frame);
    dellist(&files_by_dir);

    /* then do_recursion on the dirlist. */
    if (dirlist != NULL)
	err += do_recursion (&frame);

    /* Free the data which expand_wild allocated.  */
    free_names (&argc, argv);

 out:
    free (update_dir);
    update_dir = NULL;
    return (err);
}

/*
 * Implement the recursive policies on the local directory.  This may be
 * called directly, or may be called by start_recursion
 */
static int
do_recursion (frame)
    struct recursion_frame *frame;
{
    int err = 0;
    int dodoneproc = 1;
    char *srepository;
    List *entries = NULL;
    int should_readlock;

    /* do nothing if told */
    if (frame->flags == R_SKIP_ALL)
	return (0);

    should_readlock = noexec ? 0 : frame->readlock;

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
	&& (should_readlock || noexec))
	server_pause_check();
#endif

    /*
     * Fill in repository with the current repository
     */
    if (frame->which & W_LOCAL)
    {
	if (isdir (CVSADM))
	    repository = Name_Repository ((char *) NULL, update_dir);
	else
	    repository = NULL;
    }
    else
    {
	repository = xgetwd ();
	if (repository == NULL)
	    error (1, errno, "could not get working directory");
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
	if (frame->fileproc != NULL && frame->flags != R_SKIP_FILES)
	{
	    int lwhich = frame->which;

	    /* be sure to look in the attic if we have sticky tags/date */
	    if ((lwhich & W_ATTIC) == 0)
		if (isreadable (CVSADM_TAG))
		    lwhich |= W_ATTIC;

	    /* In the !(which & W_LOCAL) case, we filled in repository
	       earlier in the function.  In the (which & W_LOCAL) case,
	       the Find_Names function is going to look through the
	       Entries file.  If we do not have a repository, that
	       does not make sense, so we insist upon having a
	       repository at this point.  Name_Repository will give a
	       reasonable error message.  */
	    if (repository == NULL)
		repository = Name_Repository ((char *) NULL, update_dir);

	    /* find the files and fill in entries if appropriate */
	    filelist = Find_Names (repository, lwhich, frame->aflag, &entries);
	}

	/* find sub-directories if we will recurse */
	if (frame->flags != R_SKIP_DIRS)
	    dirlist = Find_Directories (repository, frame->which, entries);
    }
    else
    {
	/* something was passed on the command line */
	if (filelist != NULL && frame->fileproc != NULL)
	{
	    /* we will process files, so pre-parse entries */
	    if (frame->which & W_LOCAL)
		entries = Entries_Open (frame->aflag, NULL);
	}
    }

    /* process the files (if any) */
    if (filelist != NULL && frame->fileproc)
    {
	struct file_info finfo_struct;
	struct frame_and_file frfile;

	/* read lock it if necessary */
	if (should_readlock && repository && Reader_Lock (repository) != 0)
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

	frfile.finfo = &finfo_struct;
	frfile.frame = frame;

	/* process the files */
	err += walklist (filelist, do_file_proc, &frfile);

	/* unlock it */
	if (should_readlock)
	    Lock_Cleanup ();

	/* clean up */
	dellist (&filelist);
    }

    /* call-back files done proc (if any) */
    if (dodoneproc && frame->filesdoneproc != NULL)
	err = frame->filesdoneproc (frame->callerdat, err, repository,
				    update_dir[0] ? update_dir : ".",
				    entries);

    fileattr_write ();
    fileattr_free ();

    /* process the directories (if necessary) */
    if (dirlist != NULL)
    {
	struct frame_and_entries frent;

	frent.frame = frame;
	frent.entries = entries;
	err += walklist (dirlist, do_dir_proc, (void *) &frent);
    }
#if 0
    else if (frame->dirleaveproc != NULL)
	err += frame->dirleaveproc (frame->callerdat, ".", err, ".");
#endif
    dellist (&dirlist);

    if (entries) 
    {
	Entries_Close (entries);
	entries = NULL;
    }

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
    struct frame_and_file *frfile = (struct frame_and_file *)closure;
    struct file_info *finfo = frfile->finfo;
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

    if (frfile->frame->dosrcs && repository)
	finfo->rcs = RCS_parse (finfo->file, repository);
    else 
        finfo->rcs = (RCSNode *) NULL;
    ret = frfile->frame->fileproc (frfile->frame->callerdat, finfo);

    freercsnode(&finfo->rcs);
    free (finfo->fullname);

    /* Allow the user to monitor progress with tail -f.  Doing this once
       per file should be no big deal, but we don't want the performance
       hit of flushing on every line like previous versions of CVS.  */
    cvs_flushout ();

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
    struct frame_and_entries *frent = (struct frame_and_entries *) closure;
    struct recursion_frame *frame = frent->frame;
    struct recursion_frame xframe;
    char *dir = p->key;
    char *newrepos;
    List *sdirlist;
    char *srepository;
    char *cp;
    Dtype dir_return = R_PROCESS;
    int stripped_dot = 0;
    int err = 0;
    struct saved_cwd cwd;
    char *saved_update_dir;

    if (fncmp (dir, CVSADM) == 0)
    {
	/* This seems to most often happen when users (beginning users,
	   generally), try "cvs ci *" or something similar.  On that
	   theory, it is possible that we should just silently skip the
	   CVSADM directories, but on the other hand, using a wildcard
	   like this isn't necessarily a practice to encourage (it operates
	   only on files which exist in the working directory, unlike
	   regular CVS recursion).  */

	/* FIXME-reentrancy: printed_cvs_msg should be in a "command
	   struct" or some such, so that it gets cleared for each new
	   command (this is possible using the remote protocol and a
	   custom-written client).  The struct recursion_frame is not
	   far back enough though, some commands (commit at least)
	   will call start_recursion several times.  An alternate solution
	   would be to take this whole check and move it to a new function
	   validate_arguments or some such that all the commands call
	   and which snips the offending directory from the argc,argv
	   vector.  */
	static int printed_cvs_msg = 0;
	if (!printed_cvs_msg)
	{
	    error (0, 0, "warning: directory %s specified in argument",
		   dir);
	    error (0, 0, "\
but CVS uses %s for its own purposes; skipping %s directory",
		   CVSADM, dir);
	    printed_cvs_msg = 1;
	}
	return 0;
    }

    saved_update_dir = update_dir;
    update_dir = xmalloc (strlen (saved_update_dir)
			  + strlen (dir)
			  + 5);
    strcpy (update_dir, saved_update_dir);

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
	    newrepos = xstrdup ("");
	else
	{
	    newrepos = xmalloc (strlen (repository) + strlen (dir) + 5);
	    sprintf (newrepos, "%s/%s", repository, dir);
	}
    }
    else
    {
	if (update_dir[0] == '\0')
	    (void) strcpy (update_dir, dir);

	if (repository == NULL)
	    newrepos = xstrdup ("");
	else
	    newrepos = xstrdup (repository);
    }

    /* Check to see that the CVSADM directory, if it exists, seems to be
       well-formed.  It can be missing files if the user hit ^C in the
       middle of a previous run.  We want to (a) make this a nonfatal
       error, and (b) make sure we print which directory has the
       problem.

       Do this before the direntproc, so that (1) the direntproc
       doesn't have to guess/deduce whether we will skip the directory
       (e.g. send_dirent_proc and whether to send the directory), and
       (2) so that the warm fuzzy doesn't get printed if we skip the
       directory.  */
    if (frame->which & W_LOCAL)
    {
	char *cvsadmdir;

	cvsadmdir = xmalloc (strlen (dir)
			     + sizeof (CVSADM_REP)
			     + sizeof (CVSADM_ENT)
			     + 80);

	strcpy (cvsadmdir, dir);
	strcat (cvsadmdir, "/");
	strcat (cvsadmdir, CVSADM);
	if (isdir (cvsadmdir))
	{
	    strcpy (cvsadmdir, dir);
	    strcat (cvsadmdir, "/");
	    strcat (cvsadmdir, CVSADM_REP);
	    if (!isfile (cvsadmdir))
	    {
		/* Some commands like update may have printed "? foo" but
		   if we were planning to recurse, and don't on account of
		   CVS/Repository, we want to say why.  */
		error (0, 0, "ignoring %s (%s missing)", update_dir,
		       CVSADM_REP);
		dir_return = R_SKIP_ALL;
	    }

	    /* Likewise for CVS/Entries.  */
	    if (dir_return != R_SKIP_ALL)
	    {
		strcpy (cvsadmdir, dir);
		strcat (cvsadmdir, "/");
		strcat (cvsadmdir, CVSADM_ENT);
		if (!isfile (cvsadmdir))
		{
		    /* Some commands like update may have printed "? foo" but
		       if we were planning to recurse, and don't on account of
		       CVS/Repository, we want to say why.  */
		    error (0, 0, "ignoring %s (%s missing)", update_dir,
			   CVSADM_ENT);
		    dir_return = R_SKIP_ALL;
		}
	    }
	}
	free (cvsadmdir);
    }

    /* call-back dir entry proc (if any) */
    if (dir_return == R_SKIP_ALL)
	;
    else if (frame->direntproc != NULL)
	dir_return = frame->direntproc (frame->callerdat, dir, newrepos,
					update_dir, frent->entries);
    else
    {
	/* Generic behavior.  I don't see a reason to make the caller specify
	   a direntproc just to get this.  */
	if ((frame->which & W_LOCAL) && !isdir (dir))
	    dir_return = R_SKIP_ALL;
    }

    free (newrepos);

    /* only process the dir if the return code was 0 */
    if (dir_return != R_SKIP_ALL)
    {
	/* save our current directory and static vars */
        if (save_cwd (&cwd))
	    error_exit ();
	sdirlist = dirlist;
	srepository = repository;
	dirlist = NULL;

	/* cd to the sub-directory */
	if ( CVS_CHDIR (dir) < 0)
	    error (1, errno, "could not chdir to %s", dir);

	/* honor the global SKIP_DIRS (a.k.a. local) */
	if (frame->flags == R_SKIP_DIRS)
	    dir_return = R_SKIP_DIRS;

	/* remember if the `.' will be stripped for subsequent dirs */
	if (strcmp (update_dir, ".") == 0)
	{
	    update_dir[0] = '\0';
	    stripped_dot = 1;
	}

	/* make the recursive call */
	xframe = *frame;
	xframe.flags = dir_return;
	err += do_recursion (&xframe);

	/* put the `.' back if necessary */
	if (stripped_dot)
	    (void) strcpy (update_dir, ".");

	/* call-back dir leave proc (if any) */
	if (frame->dirleaveproc != NULL)
	    err = frame->dirleaveproc (frame->callerdat, dir, err, update_dir,
				       frent->entries);

	/* get back to where we started and restore state vars */
	if (restore_cwd (&cwd, NULL))
	    error_exit ();
	free_cwd (&cwd);
	dirlist = sdirlist;
	repository = srepository;
    }

#if 0
    /* Put back update_dir.  I think this is the same as just setting
       update_dir back to saved_update_dir, but there are a few cases I'm
       not sure about (in particular, if DIR is "." and update_dir is
       not ""), so for conservatism I'm leaving this here.  */
    cp = last_component (update_dir);
    if (cp > update_dir)
	cp[-1] = '\0';
    else
	update_dir[0] = '\0';
    free (saved_update_dir);
#else
    /* The above code is cactus!!! - it doesn't handle descending
       multiple directories at once!  ie: it recurses down several
       dirs and then back up one. This breaks 'diff', 'update',
       'commit', etc.  */
    free (update_dir);
    update_dir = saved_update_dir;
#endif

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
    p->data = NULL;
    save_dirlist = dirlist;
    dirlist = NULL;

    if (strcmp(p->key, ".") != 0)
    {
        if (save_cwd (&cwd))
	    error_exit ();
	if ( CVS_CHDIR (p->key) < 0)
	    error (1, errno, "could not chdir to %s", p->key);

	save_update_dir = update_dir;
	update_dir = xmalloc (strlen (save_update_dir)
				  + strlen (p->key)
				  + 5);
	strcpy (update_dir, save_update_dir);

	if (*update_dir != '\0')
	    (void) strcat (update_dir, "/");

	(void) strcat (update_dir, p->key);
    }

    err += do_recursion (frame);

    if (save_update_dir != NULL)
    {
	free (update_dir);
	update_dir = save_update_dir;

	if (restore_cwd (&cwd, NULL))
	    error_exit ();
	free_cwd (&cwd);
    }

    dirlist = save_dirlist;
    filelist = NULL;
    return(err);
}
