/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * General recursion handler
 * 
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)recurse.c 1.22 92/04/10";
#endif

#if __STDC__
static int do_dir_proc (Node * p);
static int do_file_proc (Node * p);
static void addlist (List ** listp, char *key);
#else
static int do_file_proc ();
static int do_dir_proc ();
static void addlist ();
#endif				/* __STDC__ */


/*
 * Local static versions eliminates the need for globals
 */
static int (*fileproc) ();
static int (*filesdoneproc) ();
static Dtype (*direntproc) ();
static int (*dirleaveproc) ();
static int which;
static Dtype flags;
static int aflag;
static int readlock;
static int dosrcs;
static char update_dir[PATH_MAX];
static char *repository = NULL;
static List *entries = NULL;
static List *srcfiles = NULL;
static List *filelist = NULL;
static List *dirlist = NULL;

/*
 * Called to start a recursive command Command line arguments are processed
 * if present, otherwise the local directory is processed.
 */
int
start_recursion (fileproc, filesdoneproc, direntproc, dirleaveproc,
		 argc, argv, local, which, aflag, readlock,
		 update_preload, dosrcs)
    int (*fileproc) ();
    int (*filesdoneproc) ();
    Dtype (*direntproc) ();
    int (*dirleaveproc) ();
    int argc;
    char *argv[];
    int local;
    int which;
    int aflag;
    int readlock;
    char *update_preload;
    int dosrcs;
{
    int i, err = 0;
    Dtype flags;

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
    if (entries)
	dellist (&entries);
    if (srcfiles)
	dellist (&srcfiles);
    if (filelist)
	dellist (&filelist);
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
	if ((which & W_LOCAL) && !isdir (CVSADM) && !isdir (OCVSADM))
	    dirlist = Find_Dirs ((char *) NULL, W_LOCAL);
	else
	    addlist (&dirlist, ".");

	err += do_recursion (fileproc, filesdoneproc, direntproc,
			    dirleaveproc, flags, which, aflag,
			    readlock, dosrcs);
    }
    else
    {

	/*
	 * There were arguments, so we have to handle them by hand. To do
	 * that, we set up the filelist and dirlist with the arguments and
	 * call do_recursion.  do_recursion recognizes the fact that the
	 * lists are non-null when it starts and doesn't update them
	 */

	/* look for args with /-s in them */
	for (i = 0; i < argc; i++)
	    if (index (argv[i], '/') != NULL)
		break;

	/* if we didn't find any hard one's, do it the easy way */
	if (i == argc)
	{
	    /* set up the lists */
	    for (i = 0; i < argc; i++)
	    {
		if (isdir (argv[i]))
		    addlist (&dirlist, argv[i]);
		else
		{
		    if (isdir (CVSADM) || isdir (OCVSADM))
		    {
			char *repos;
			char tmp[PATH_MAX];

			repos = Name_Repository ((char *) NULL, update_dir);
			(void) sprintf (tmp, "%s/%s", repos, argv[i]);
			if (isdir (tmp))
			    addlist (&dirlist, argv[i]);
			else
			    addlist (&filelist, argv[i]);
			free (repos);
		    }
		    else
			addlist (&filelist, argv[i]);
		}
	    }

	    /* we aren't recursive if no directories were specified */
	    if (dirlist == NULL)
		local = 1;

	    /* process the lists */
	    err += do_recursion (fileproc, filesdoneproc, direntproc,
				dirleaveproc, flags, which, aflag,
				readlock, dosrcs);
	}
	/* otherwise - do it the hard way */
	else
	{
	    char *cp;
	    char *dir = (char *) NULL;
	    char *comp = (char *) NULL;
	    char *oldupdate = (char *) NULL;
	    char savewd[PATH_MAX];

	    if (getwd (savewd) == NULL)
		error (1, 0, "could not get working directory: %s", savewd);

	    for (i = 0; i < argc; i++)
	    {
		/* split the arg into the dir and component parts */
		dir = xstrdup (argv[i]);
		if ((cp = rindex (dir, '/')) != NULL)
		{
		    *cp = '\0';
		    comp = xstrdup (cp + 1);
		    oldupdate = xstrdup (update_dir);
		    if (update_dir[0] != '\0')
			(void) strcat (update_dir, "/");
		    (void) strcat (update_dir, dir);
		}
		else
		{
		    comp = xstrdup (dir);
		    if (dir)
			free (dir);
		    dir = (char *) NULL;
		}

		/* chdir to the appropriate place if necessary */
		if (dir && chdir (dir) < 0)
		    error (1, errno, "could not chdir to %s", dir);

		/* set up the list */
		if (isdir (comp))
		    addlist (&dirlist, comp);
		else
		{
		    if (isdir (CVSADM) || isdir (OCVSADM))
		    {
			char *repos;
			char tmp[PATH_MAX];

			repos = Name_Repository ((char *) NULL, update_dir);
			(void) sprintf (tmp, "%s/%s", repos, comp);
			if (isdir (tmp))
			    addlist (&dirlist, comp);
			else
			    addlist (&filelist, comp);
			free (repos);
		    }
		    else
			addlist (&filelist, comp);
		}

		/* do the recursion */
		err += do_recursion (fileproc, filesdoneproc, direntproc,
				    dirleaveproc, flags, which,
				    aflag, readlock, dosrcs);

		/* chdir back and fix update_dir if necessary */
		if (dir && chdir (savewd) < 0)
		    error (1, errno, "could not chdir to %s", dir);
		if (oldupdate)
		{
		    (void) strcpy (update_dir, oldupdate);
		    free (oldupdate);
		}

	    }
	    if (dir)
		free (dir);
	    if (comp)
		free (comp);
	}
    }
    return (err);
}

/*
 * Implement the recursive policies on the local directory.  This may be
 * called directly, or may be called by start_recursion
 */
int
do_recursion (xfileproc, xfilesdoneproc, xdirentproc, xdirleaveproc,
	      xflags, xwhich, xaflag, xreadlock, xdosrcs)
    int (*xfileproc) ();
    int (*xfilesdoneproc) ();
    Dtype (*xdirentproc) ();
    int (*xdirleaveproc) ();
    Dtype xflags;
    int xwhich;
    int xaflag;
    int xreadlock;
    int xdosrcs;
{
    int err = 0;
    int dodoneproc = 1;
    char *srepository;

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

    /*
     * Fill in repository with the current repository
     */
    if (which & W_LOCAL)
    {
	if (isdir (CVSADM) || isdir (OCVSADM))
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
	    dirlist = Find_Dirs (repository, which);
    }
    else
    {
	/* something was passed on the command line */
	if (filelist != NULL && fileproc != NULL)
	{
	    /* we will process files, so pre-parse entries */
	    if (which & W_LOCAL)
		entries = ParseEntries (aflag);
	}
    }

    /* process the files (if any) */
    if (filelist != NULL)
    {
	/* read lock it if necessary */
	if (readlock && repository && Reader_Lock (repository) != 0)
	    error (1, 0, "read lock failed - giving up");

	/* pre-parse the source files */
	if (dosrcs && repository)
	    srcfiles = RCS_parsefiles (filelist, repository);
	else
	    srcfiles = (List *) NULL;

	/* process the files */
	err += walklist (filelist, do_file_proc);

	/* unlock it */
	if (readlock)
	    Lock_Cleanup ();

	/* clean up */
	dellist (&filelist);
	dellist (&srcfiles);
	dellist (&entries);
    }

    /* call-back files done proc (if any) */
    if (dodoneproc && filesdoneproc != NULL)
	err = filesdoneproc (err, repository, update_dir[0] ? update_dir : ".");

    /* process the directories (if necessary) */
    if (dirlist != NULL)
	err += walklist (dirlist, do_dir_proc);
#ifdef notdef
    else if (dirleaveproc != NULL)
	err += dirleaveproc(".", err, ".");
#endif
    dellist (&dirlist);

    /* free the saved copy of the pointer if necessary */
    if (srepository)
    {
	(void) free (srepository);
	repository = (char *) NULL;
    }

    return (err);
}

/*
 * Process each of the files in the list with the callback proc
 */
static int
do_file_proc (p)
    Node *p;
{
    if (fileproc != NULL)
	return (fileproc (p->key, update_dir, repository, entries, srcfiles));
    else
	return (0);
}

/*
 * Process each of the directories in the list (recursing as we go)
 */
static int
do_dir_proc (p)
    Node *p;
{
    char *dir = p->key;
    char savewd[PATH_MAX];
    char newrepos[PATH_MAX];
    List *sdirlist;
    char *srepository;
    char *cp;
    Dtype dir_return = R_PROCESS;
    int stripped_dot = 0;
    int err = 0;

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
	if (getwd (savewd) == NULL)
	    error (1, 0, "could not get working directory: %s", savewd);
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
	if (chdir (savewd) < 0)
	    error (1, errno, "could not chdir to %s", savewd);
	dirlist = sdirlist;
	repository = srepository;
    }

    /* put back update_dir */
    if ((cp = rindex (update_dir, '/')) != NULL)
	*cp = '\0';
    else
	update_dir[0] = '\0';

    return (err);
}

/*
 * Add a node to a list allocating the list if necessary
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
    (void) addnode (*listp, p);
}
