/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Set Lock
 * 
 * Lock file support for CVS.
 */

/* The node Concurrency in doc/cvs.texinfo has a brief introduction to
   how CVS locks function, and some of the user-visible consequences of
   their existence.  Here is a summary of why they exist (and therefore,
   the consequences of hacking CVS to read a repository without creating
   locks):

   There are two uses.  One is the ability to prevent there from being
   two writers at the same time.  This is necessary for any number of
   reasons (fileattr code, probably others).  Commit needs to lock the
   whole tree so that nothing happens between the up-to-date check and
   the actual checkin.

   The second use is the ability to ensure that there is not a writer
   and a reader at the same time (several readers are allowed).  Reasons
   for this are:

   * Readlocks ensure that once CVS has found a collection of rcs
   files using Find_Names, the files will still exist when it reads
   them (they may have moved in or out of the attic).

   * Readlocks provide some modicum of consistency, although this is
   kind of limited--see the node Concurrency in cvs.texinfo.

   * Readlocks ensure that the RCS file does not change between
   RCS_parse and RCS_reparsercsfile time.  This one strikes me as
   important, although I haven't thought up what bad scenarios might
   be.

   * Readlocks ensure that we won't find the file in the state in
   which it is in between the calls to add_rcs_file and RCS_checkin in
   commit.c (when a file is being added).  This state is a state in
   which the RCS file parsing routines in rcs.c cannot parse the file.

   * Readlocks ensure that a reader won't try to look at a
   half-written fileattr file (fileattr is not updated atomically).

   (see also the description of anonymous read-only access in
   "Password authentication security" node in doc/cvs.texinfo).

   While I'm here, I'll try to summarize a few random suggestions
   which periodically get made about how locks might be different:

   1.  Check for EROFS.  Maybe useful, although in the presence of NFS
   EROFS does *not* mean that the file system is unchanging.

   2.  Provide a means to put the cvs locks in some directory apart from
   the repository (CVSROOT/locks; a -l option in modules; etc.).

   3.  Provide an option to disable locks for operations which only
   read (see above for some of the consequences).

   4.  Have a server internally do the locking.  Probably a good
   long-term solution, and many people have been working hard on code
   changes which would eventually make it possible to have a server
   which can handle various connections in one process, but there is
   much, much work still to be done before this is feasible.

   5.  Like #4 but use shared memory or something so that the servers
   merely need to all be on the same machine.  This is a much smaller
   change to CVS (it functions much like #2; shared memory might be an
   unneeded complication although it presumably would be faster).  */

#include "cvs.h"

struct lock {
    /* This is the directory in which we may have a lock named by the
       readlock variable, a lock named by the writelock variable, and/or
       a lock named CVSLCK.  The storage is not allocated along with the
       struct lock; it is allocated by the Reader_Lock caller or in the
       case of writelocks, it is just a pointer to the storage allocated
       for the ->key field.  */
    char *repository;
    /* Do we have a lock named CVSLCK?  */
    int have_lckdir;
    /* Note there is no way of knowing whether the readlock and writelock
       exist.  The code which sets the locks doesn't use SIG_beginCrSect
       to set a flag like we do for CVSLCK.  */
};

static void remove_locks PROTO((void));
static int readers_exist PROTO((char *repository));
static int set_lock PROTO ((struct lock *lock, int will_wait));
static void clear_lock PROTO ((struct lock *lock));
static void set_lockers_name PROTO((struct stat *statp));
static int set_writelock_proc PROTO((Node * p, void *closure));
static int unlock_proc PROTO((Node * p, void *closure));
static int write_lock PROTO ((struct lock *lock));
static void lock_simple_remove PROTO ((struct lock *lock));
static void lock_wait PROTO((char *repository));
static void lock_obtained PROTO((char *repository));

/* Malloc'd array containing the username of the whoever has the lock.
   Will always be non-NULL in the cases where it is needed.  */
static char *lockers_name;
/* Malloc'd array specifying name of a readlock within a directory.
   Or NULL if none.  */
static char *readlock;
/* Malloc'd array specifying name of a writelock within a directory.
   Or NULL if none.  */
static char *writelock;
/* Malloc'd array specifying the name of a CVSLCK file (absolute pathname).
   Will always be non-NULL in the cases where it is used.  */
static char *masterlock;
static List *locklist;

#define L_OK		0		/* success */
#define L_ERROR		1		/* error condition */
#define L_LOCKED	2		/* lock owned by someone else */

/* This is the (single) readlock which is set by Reader_Lock.  The
   repository field is NULL if there is no such lock.  */
static struct lock global_readlock;

/* List of locks set by lock_tree_for_write.  This is redundant
   with locklist, sort of.  */
static List *lock_tree_list;

/* If we set locks with lock_dir_for_write, then locked_dir contains
   the malloc'd name of the repository directory which we have locked.
   locked_list is the same thing packaged into a list and is redundant
   with locklist the same way that lock_tree_list is.  */
static char *locked_dir;
static List *locked_list;

/*
 * Clean up all outstanding locks
 */
void
Lock_Cleanup ()
{
    remove_locks ();

    dellist (&lock_tree_list);

    if (locked_dir != NULL)
    {
	dellist (&locked_list);
	free (locked_dir);
	locked_dir = NULL;
	locked_list = NULL;
    }
}

/*
 * Remove locks without discarding the lock information
 */
static void
remove_locks ()
{
    /* clean up simple locks (if any) */
    if (global_readlock.repository != NULL)
    {
	lock_simple_remove (&global_readlock);
	global_readlock.repository = NULL;
    }

    /* clean up multiple locks (if any) */
    if (locklist != (List *) NULL)
    {
	(void) walklist (locklist, unlock_proc, NULL);
	locklist = (List *) NULL;
    }
}

/*
 * walklist proc for removing a list of locks
 */
static int
unlock_proc (p, closure)
    Node *p;
    void *closure;
{
    lock_simple_remove ((struct lock *)p->data);
    return (0);
}

/* Remove the lock files.  */
static void
lock_simple_remove (lock)
    struct lock *lock;
{
    char *tmp;

    /* If readlock is set, the lock directory *might* have been created, but
       since Reader_Lock doesn't use SIG_beginCrSect the way that set_lock
       does, we don't know that.  That is why we need to check for
       existence_error here.  */
    if (readlock != NULL)
    {
	tmp = xmalloc (strlen (lock->repository) + strlen (readlock) + 10);
	(void) sprintf (tmp, "%s/%s", lock->repository, readlock);
	if ( CVS_UNLINK (tmp) < 0 && ! existence_error (errno))
	    error (0, errno, "failed to remove lock %s", tmp);
	free (tmp);
    }

    /* If writelock is set, the lock directory *might* have been created, but
       since write_lock doesn't use SIG_beginCrSect the way that set_lock
       does, we don't know that.  That is why we need to check for
       existence_error here.  */
    if (writelock != NULL)
    {
	tmp = xmalloc (strlen (lock->repository) + strlen (writelock) + 10);
	(void) sprintf (tmp, "%s/%s", lock->repository, writelock);
	if ( CVS_UNLINK (tmp) < 0 && ! existence_error (errno))
	    error (0, errno, "failed to remove lock %s", tmp);
	free (tmp);
    }

    if (lock->have_lckdir)
    {
	tmp = xmalloc (strlen (lock->repository) + sizeof (CVSLCK) + 10);
	(void) sprintf (tmp, "%s/%s", lock->repository, CVSLCK);
	SIG_beginCrSect ();
	if (CVS_RMDIR (tmp) < 0)
	    error (0, errno, "failed to remove lock dir %s", tmp);
	lock->have_lckdir = 0;
	SIG_endCrSect ();
	free (tmp);
    }
}

/*
 * Create a lock file for readers
 */
int
Reader_Lock (xrepository)
    char *xrepository;
{
    int err = 0;
    FILE *fp;
    char *tmp;

    if (noexec || readonlyfs)
	return (0);

    /* we only do one directory at a time for read locks! */
    if (global_readlock.repository != NULL)
    {
	error (0, 0, "Reader_Lock called while read locks set - Help!");
	return (1);
    }

    if (readlock == NULL)
    {
	readlock = xmalloc (strlen (hostname) + sizeof (CVSRFL) + 40);
	(void) sprintf (readlock, 
#ifdef HAVE_LONG_FILE_NAMES
			"%s.%s.%ld", CVSRFL, hostname,
#else
			"%s.%ld", CVSRFL,
#endif
			(long) getpid ());
    }

    /* remember what we're locking (for Lock_Cleanup) */
    global_readlock.repository = xrepository;

    /* get the lock dir for our own */
    if (set_lock (&global_readlock, 1) != L_OK)
    {
	error (0, 0, "failed to obtain dir lock in repository `%s'",
	       xrepository);
	if (readlock != NULL)
	    free (readlock);
	readlock = NULL;
	/* We don't set global_readlock.repository to NULL.  I think this
	   only works because recurse.c will give a fatal error if we return
	   a nonzero value.  */
	return (1);
    }

    /* write a read-lock */
    tmp = xmalloc (strlen (xrepository) + strlen (readlock) + 10);
    (void) sprintf (tmp, "%s/%s", xrepository, readlock);
    if ((fp = CVS_FOPEN (tmp, "w+")) == NULL || fclose (fp) == EOF)
    {
	error (0, errno, "cannot create read lock in repository `%s'",
	       xrepository);
	if (readlock != NULL)
	    free (readlock);
	readlock = NULL;
	err = 1;
    }
    free (tmp);

    /* free the lock dir */
    clear_lock (&global_readlock);

    return (err);
}

/*
 * Lock a list of directories for writing
 */
static char *lock_error_repos;
static int lock_error;

static int Writer_Lock PROTO ((List * list));

static int
Writer_Lock (list)
    List *list;
{
    char *wait_repos;

    if (noexec)
	return (0);

    if (readonlyfs) {
	error (0, 0, "write lock failed - read-only repository");
	return (1);
    }

    /* We only know how to do one list at a time */
    if (locklist != (List *) NULL)
    {
	error (0, 0, "Writer_Lock called while write locks set - Help!");
	return (1);
    }

    wait_repos = NULL;
    for (;;)
    {
	/* try to lock everything on the list */
	lock_error = L_OK;		/* init for set_writelock_proc */
	lock_error_repos = (char *) NULL; /* init for set_writelock_proc */
	locklist = list;		/* init for Lock_Cleanup */
	if (lockers_name != NULL)
	    free (lockers_name);
	lockers_name = xstrdup ("unknown");

	(void) walklist (list, set_writelock_proc, NULL);

	switch (lock_error)
	{
	    case L_ERROR:		/* Real Error */
		if (wait_repos != NULL)
		    free (wait_repos);
		Lock_Cleanup ();	/* clean up any locks we set */
		error (0, 0, "lock failed - giving up");
		return (1);

	    case L_LOCKED:		/* Someone already had a lock */
		remove_locks ();	/* clean up any locks we set */
		lock_wait (lock_error_repos); /* sleep a while and try again */
		wait_repos = xstrdup (lock_error_repos);
		continue;

	    case L_OK:			/* we got the locks set */
	        if (wait_repos != NULL)
		{
		    lock_obtained (wait_repos);
		    free (wait_repos);
		}
		return (0);

	    default:
		if (wait_repos != NULL)
		    free (wait_repos);
		error (0, 0, "unknown lock status %d in Writer_Lock",
		       lock_error);
		return (1);
	}
    }
}

/*
 * walklist proc for setting write locks
 */
static int
set_writelock_proc (p, closure)
    Node *p;
    void *closure;
{
    /* if some lock was not OK, just skip this one */
    if (lock_error != L_OK)
	return (0);

    /* apply the write lock */
    lock_error_repos = p->key;
    lock_error = write_lock ((struct lock *)p->data);
    return (0);
}

/*
 * Create a lock file for writers returns L_OK if lock set ok, L_LOCKED if
 * lock held by someone else or L_ERROR if an error occurred
 */
static int
write_lock (lock)
    struct lock *lock;
{
    int status;
    FILE *fp;
    char *tmp;

    if (writelock == NULL)
    {
	writelock = xmalloc (strlen (hostname) + sizeof (CVSWFL) + 40);
	(void) sprintf (writelock,
#ifdef HAVE_LONG_FILE_NAMES
			"%s.%s.%ld", CVSWFL, hostname,
#else
			"%s.%ld", CVSWFL,
#endif
			(long) getpid());
    }

    /* make sure the lock dir is ours (not necessarily unique to us!) */
    status = set_lock (lock, 0);
    if (status == L_OK)
    {
	/* we now own a writer - make sure there are no readers */
	if (readers_exist (lock->repository))
	{
	    /* clean up the lock dir if we created it */
	    if (status == L_OK)
	    {
		clear_lock (lock);
	    }

	    /* indicate we failed due to read locks instead of error */
	    return (L_LOCKED);
	}

	/* write the write-lock file */
	tmp = xmalloc (strlen (lock->repository) + strlen (writelock) + 10);
	(void) sprintf (tmp, "%s/%s", lock->repository, writelock);
	if ((fp = CVS_FOPEN (tmp, "w+")) == NULL || fclose (fp) == EOF)
	{
	    int xerrno = errno;

	    if ( CVS_UNLINK (tmp) < 0 && ! existence_error (errno))
		error (0, errno, "failed to remove lock %s", tmp);

	    /* free the lock dir if we created it */
	    if (status == L_OK)
	    {
		clear_lock (lock);
	    }

	    /* return the error */
	    error (0, xerrno, "cannot create write lock in repository `%s'",
		   lock->repository);
	    free (tmp);
	    return (L_ERROR);
	}
	free (tmp);
	return (L_OK);
    }
    else
	return (status);
}

/*
 * readers_exist() returns 0 if there are no reader lock files remaining in
 * the repository; else 1 is returned, to indicate that the caller should
 * sleep a while and try again.
 */
static int
readers_exist (repository)
    char *repository;
{
    char *line;
    DIR *dirp;
    struct dirent *dp;
    struct stat sb;
    int ret = 0;

#ifdef CVS_FUDGELOCKS
again:
#endif

    if ((dirp = CVS_OPENDIR (repository)) == NULL)
	error (1, 0, "cannot open directory %s", repository);

    errno = 0;
    while ((dp = readdir (dirp)) != NULL)
    {
	if (CVS_FNMATCH (CVSRFLPAT, dp->d_name, 0) == 0)
	{
#ifdef CVS_FUDGELOCKS
	    time_t now;
	    (void) time (&now);
#endif

	    line = xmalloc (strlen (repository) + strlen (dp->d_name) + 5);
	    (void) sprintf (line, "%s/%s", repository, dp->d_name);
	    if ( CVS_STAT (line, &sb) != -1)
	    {
#ifdef CVS_FUDGELOCKS
		/*
		 * If the create time of the file is more than CVSLCKAGE 
		 * seconds ago, try to clean-up the lock file, and if
		 * successful, re-open the directory and try again.
		 */
		if (now >= (sb.st_ctime + CVSLCKAGE) && CVS_UNLINK (line) != -1)
		{
		    (void) closedir (dirp);
		    free (line);
		    goto again;
		}
#endif
		set_lockers_name (&sb);
	    }
	    else
	    {
		/* If the file doesn't exist, it just means that it disappeared
		   between the time we did the readdir and the time we did
		   the stat.  */
		if (!existence_error (errno))
		    error (0, errno, "cannot stat %s", line);
	    }
	    errno = 0;
	    free (line);

	    ret = 1;
	    break;
	}
	errno = 0;
    }
    if (errno != 0)
	error (0, errno, "error reading directory %s", repository);

    closedir (dirp);
    return (ret);
}

/*
 * Set the static variable lockers_name appropriately, based on the stat
 * structure passed in.
 */
static void
set_lockers_name (statp)
    struct stat *statp;
{
    struct passwd *pw;

    if (lockers_name != NULL)
	free (lockers_name);
    if ((pw = (struct passwd *) getpwuid (statp->st_uid)) !=
	(struct passwd *) NULL)
    {
	lockers_name = xstrdup (pw->pw_name);
    }
    else
    {
	lockers_name = xmalloc (20);
	(void) sprintf (lockers_name, "uid%lu", (unsigned long) statp->st_uid);
    }
}

/*
 * Persistently tries to make the directory "lckdir",, which serves as a
 * lock. If the create time on the directory is greater than CVSLCKAGE
 * seconds old, just try to remove the directory.
 */
static int
set_lock (lock, will_wait)
    struct lock *lock;
    int will_wait;
{
    int waited;
    struct stat sb;
    mode_t omask;
#ifdef CVS_FUDGELOCKS
    time_t now;
#endif

    if (masterlock != NULL)
	free (masterlock);
    masterlock = xmalloc (strlen (lock->repository) + sizeof (CVSLCK) + 10);
    (void) sprintf (masterlock, "%s/%s", lock->repository, CVSLCK);

    /*
     * Note that it is up to the callers of set_lock() to arrange for signal
     * handlers that do the appropriate things, like remove the lock
     * directory before they exit.
     */
    waited = 0;
    lock->have_lckdir = 0;
    for (;;)
    {
	int status = -1;
	omask = umask (cvsumask);
	SIG_beginCrSect ();
	if (CVS_MKDIR (masterlock, 0777) == 0)
	{
	    lock->have_lckdir = 1;
	    SIG_endCrSect ();
	    status = L_OK;
	    if (waited)
	        lock_obtained (lock->repository);
	    goto out;
	}
	SIG_endCrSect ();
      out:
	(void) umask (omask);
	if (status != -1)
	    return status;

	if (errno != EEXIST)
	{
	    error (0, errno,
		   "failed to create lock directory in repository `%s'",
		   lock->repository);
	    return (L_ERROR);
	}

	/* Find out who owns the lock.  If the lock directory is
	   non-existent, re-try the loop since someone probably just
	   removed it (thus releasing the lock).  */
	if (CVS_STAT (masterlock, &sb) < 0)
	{
	    if (existence_error (errno))
		continue;

	    error (0, errno, "couldn't stat lock directory `%s'", masterlock);
	    return (L_ERROR);
	}

#ifdef CVS_FUDGELOCKS
	/*
	 * If the create time of the directory is more than CVSLCKAGE seconds
	 * ago, try to clean-up the lock directory, and if successful, just
	 * quietly retry to make it.
	 */
	(void) time (&now);
	if (now >= (sb.st_ctime + CVSLCKAGE))
	{
	    if (CVS_RMDIR (masterlock) >= 0)
		continue;
	}
#endif

	/* set the lockers name */
	set_lockers_name (&sb);

	/* if he wasn't willing to wait, return an error */
	if (!will_wait)
	    return (L_LOCKED);
	lock_wait (lock->repository);
	waited = 1;
    }
}

/*
 * Clear master lock.  We don't have to recompute the lock name since
 * clear_lock is never called except after a successful set_lock().
 */
static void
clear_lock (lock)
    struct lock *lock;
{
    SIG_beginCrSect ();
    if (CVS_RMDIR (masterlock) < 0)
	error (0, errno, "failed to remove lock dir `%s'", masterlock);
    lock->have_lckdir = 0;
    SIG_endCrSect ();
}

/*
 * Print out a message that the lock is still held, then sleep a while.
 */
static void
lock_wait (repos)
    char *repos;
{
    time_t now;

    (void) time (&now);
    error (0, 0, "[%8.8s] waiting for %s's lock in %s", ctime (&now) + 11,
	   lockers_name, repos);
    /* Call cvs_flusherr to ensure that the user sees this message as
       soon as possible.  */
    cvs_flusherr ();
    (void) sleep (CVSLCKSLEEP);
}

/*
 * Print out a message when we obtain a lock.
 */
static void
lock_obtained (repos)
     char *repos;
{
    time_t now;

    (void) time (&now);
    error (0, 0, "[%8.8s] obtained lock in %s", ctime (&now) + 11, repos);
    /* Call cvs_flusherr to ensure that the user sees this message as
       soon as possible.  */
    cvs_flusherr ();
}

static int lock_filesdoneproc PROTO ((void *callerdat, int err,
				      char *repository, char *update_dir,
				      List *entries));
static int fsortcmp PROTO((const Node * p, const Node * q));

/*
 * Create a list of repositories to lock
 */
/* ARGSUSED */
static int
lock_filesdoneproc (callerdat, err, repository, update_dir, entries)
    void *callerdat;
    int err;
    char *repository;
    char *update_dir;
    List *entries;
{
    Node *p;

    p = getnode ();
    p->type = LOCK;
    p->key = xstrdup (repository);
    p->data = xmalloc (sizeof (struct lock));
    ((struct lock *)p->data)->repository = p->key;
    ((struct lock *)p->data)->have_lckdir = 0;

    /* FIXME-KRP: this error condition should not simply be passed by. */
    if (p->key == NULL || addnode (lock_tree_list, p) != 0)
	freenode (p);
    return (err);
}

/*
 * compare two lock list nodes (for sort)
 */
static int
fsortcmp (p, q)
    const Node *p;
    const Node *q;
{
    return (strcmp (p->key, q->key));
}

void
lock_tree_for_write (argc, argv, local, aflag)
    int argc;
    char **argv;
    int local;
    int aflag;
{
    int err;
    /*
     * Run the recursion processor to find all the dirs to lock and lock all
     * the dirs
     */
    lock_tree_list = getlist ();
    err = start_recursion ((FILEPROC) NULL, lock_filesdoneproc,
			   (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL, argc,
			   argv, local, W_LOCAL, aflag, 0, (char *) NULL, 0);
    sortlist (lock_tree_list, fsortcmp);
    if (Writer_Lock (lock_tree_list) != 0)
	error (1, 0, "lock failed - giving up");
}

/* Lock a single directory in REPOSITORY.  It is OK to call this if
   a lock has been set with lock_dir_for_write; the new lock will replace
   the old one.  If REPOSITORY is NULL, don't do anything.  */
void
lock_dir_for_write (repository)
     char *repository;
{
    if (repository != NULL
	&& (locked_dir == NULL
	    || strcmp (locked_dir, repository) != 0))
    {
	Node *node;

	if (locked_dir != NULL)
	    Lock_Cleanup ();

	locked_dir = xstrdup (repository);
	locked_list = getlist ();
	node = getnode ();
	node->type = LOCK;
	node->key = xstrdup (repository);
	node->data = xmalloc (sizeof (struct lock));
	((struct lock *)node->data)->repository = node->key;
	((struct lock *)node->data)->have_lckdir = 0;

	(void) addnode (locked_list, node);
	Writer_Lock (locked_list);
    }
}
