/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Set Lock
 * 
 * Lock file support for CVS.
 */

#include "cvs.h"

static int readers_exist PROTO((char *repository));
static int set_lock PROTO((char *repository, int will_wait));
static void clear_lock PROTO((void));
static void set_lockers_name PROTO((struct stat *statp));
static int set_writelock_proc PROTO((Node * p, void *closure));
static int unlock_proc PROTO((Node * p, void *closure));
static int write_lock PROTO((char *repository));
static void lock_simple_remove PROTO((char *repository));
static void lock_wait PROTO((char *repository));
static int Check_Owner PROTO((char *lockdir));

static char lockers_name[20];
static char *repository;
static char readlock[PATH_MAX], writelock[PATH_MAX], masterlock[PATH_MAX];
static int cleanup_lckdir;
static List *locklist;

#define L_OK		0		/* success */
#define L_ERROR		1		/* error condition */
#define L_LOCKED	2		/* lock owned by someone else */

/*
 * Clean up all outstanding locks
 */
void
Lock_Cleanup ()
{
    /* clean up simple locks (if any) */
    if (repository != NULL)
    {
	lock_simple_remove (repository);
	repository = (char *) NULL;
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
    lock_simple_remove (p->key);
    return (0);
}

/*
 * Remove the lock files (without complaining if they are not there),
 */
static void
lock_simple_remove (repository)
    char *repository;
{
    char tmp[PATH_MAX];

    if (readlock[0] != '\0')
    {
	(void) sprintf (tmp, "%s/%s", repository, readlock);
	if (unlink (tmp) < 0 && ! existence_error (errno))
	    error (0, errno, "failed to remove lock %s", tmp);
    }

    if (writelock[0] != '\0')
    {
	(void) sprintf (tmp, "%s/%s", repository, writelock);
	if (unlink (tmp) < 0 && ! existence_error (errno))
	    error (0, errno, "failed to remove lock %s", tmp);
    }

    /*
     * Only remove the lock directory if it is ours, note that this does
     * lead to the limitation that one user ID should not be committing
     * files into the same Repository directory at the same time. Oh well.
     */
    if (writelock[0] != '\0' || (readlock[0] != '\0' && cleanup_lckdir)) 
    {
	    (void) sprintf (tmp, "%s/%s", repository, CVSLCK);
    	    if (Check_Owner(tmp))
	    {
#ifdef AFSCVS
		char rmuidlock[PATH_MAX];
		sprintf(rmuidlock, "rm -f %s/uidlock%d", tmp, geteuid() );
		system(rmuidlock);
#endif
	    (void) rmdir (tmp);
	    }
    }
    cleanup_lckdir = 0;
}

/*
 * Check the owner of a lock.  Returns 1 if we own it, 0 otherwise.
 */
static int
Check_Owner(lockdir)
     char *lockdir;
{
  struct stat sb;

#ifdef AFSCVS
  /* In the Andrew File System (AFS), user ids from stat don't match
     those from geteuid().  The AFSCVS code can deal with either AFS or
     non-AFS repositories; the non-AFSCVS code is faster.  */
  char uidlock[PATH_MAX];

  /* Check if the uidlock is in the lock directory */
  sprintf(uidlock, "%s/uidlock%d", lockdir, geteuid() );
  if( stat(uidlock, &sb) != -1)
    return 1;   /* The file exists, therefore we own the lock */
  else
    return 0; 	/* The file didn't exist or some other error.
		 * Assume that we don't own it.
		 */
#else
  if (stat (lockdir, &sb) != -1 && sb.st_uid == geteuid ())
    return 1;
  else
    return 0;
#endif
}  /* end Check_Owner() */


/*
 * Create a lock file for readers
 */
int
Reader_Lock (xrepository)
    char *xrepository;
{
    int err = 0;
    FILE *fp;
    char tmp[PATH_MAX];

    if (noexec)
	return (0);

    /* we only do one directory at a time for read locks! */
    if (repository != NULL)
    {
	error (0, 0, "Reader_Lock called while read locks set - Help!");
	return (1);
    }

    if (readlock[0] == '\0')
      (void) sprintf (readlock, 
#ifdef HAVE_LONG_FILE_NAMES
		"%s.%s.%ld", CVSRFL, hostname,
#else
		"%s.%ld", CVSRFL,
#endif
		(long) getpid ());

    /* remember what we're locking (for lock_cleanup) */
    repository = xrepository;

    /* get the lock dir for our own */
    if (set_lock (xrepository, 1) != L_OK)
    {
	error (0, 0, "failed to obtain dir lock in repository `%s'",
	       xrepository);
	readlock[0] = '\0';
	return (1);
    }

    /* write a read-lock */
    (void) sprintf (tmp, "%s/%s", xrepository, readlock);
    if ((fp = fopen (tmp, "w+")) == NULL || fclose (fp) == EOF)
    {
	error (0, errno, "cannot create read lock in repository `%s'",
	       xrepository);
	readlock[0] = '\0';
	err = 1;
    }

    /* free the lock dir */
    clear_lock();

    return (err);
}

/*
 * Lock a list of directories for writing
 */
static char *lock_error_repos;
static int lock_error;
int
Writer_Lock (list)
    List *list;
{
    if (noexec)
	return (0);

    /* We only know how to do one list at a time */
    if (locklist != (List *) NULL)
    {
	error (0, 0, "Writer_Lock called while write locks set - Help!");
	return (1);
    }

    for (;;)
    {
	/* try to lock everything on the list */
	lock_error = L_OK;		/* init for set_writelock_proc */
	lock_error_repos = (char *) NULL; /* init for set_writelock_proc */
	locklist = list;		/* init for Lock_Cleanup */
	(void) strcpy (lockers_name, "unknown");

	(void) walklist (list, set_writelock_proc, NULL);

	switch (lock_error)
	{
	    case L_ERROR:		/* Real Error */
		Lock_Cleanup ();	/* clean up any locks we set */
		error (0, 0, "lock failed - giving up");
		return (1);

	    case L_LOCKED:		/* Someone already had a lock */
		Lock_Cleanup ();	/* clean up any locks we set */
		lock_wait (lock_error_repos); /* sleep a while and try again */
		continue;

	    case L_OK:			/* we got the locks set */
		return (0);

	    default:
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
    lock_error = write_lock (p->key);
    return (0);
}

/*
 * Create a lock file for writers returns L_OK if lock set ok, L_LOCKED if
 * lock held by someone else or L_ERROR if an error occurred
 */
static int
write_lock (repository)
    char *repository;
{
    int status;
    FILE *fp;
    char tmp[PATH_MAX];

    if (writelock[0] == '\0')
	(void) sprintf (writelock,
#ifdef HAVE_LONG_FILE_NAMES
	    "%s.%s.%ld", CVSWFL, hostname,
#else
	    "%s.%ld", CVSWFL,
#endif
	(long) getpid());

    /* make sure the lock dir is ours (not necessarily unique to us!) */
    status = set_lock (repository, 0);
    if (status == L_OK)
    {
	/* we now own a writer - make sure there are no readers */
	if (readers_exist (repository))
	{
	    /* clean up the lock dir if we created it */
	    if (status == L_OK)
	    {
		clear_lock();
	    }

	    /* indicate we failed due to read locks instead of error */
	    return (L_LOCKED);
	}

	/* write the write-lock file */
	(void) sprintf (tmp, "%s/%s", repository, writelock);
	if ((fp = fopen (tmp, "w+")) == NULL || fclose (fp) == EOF)
	{
	    int xerrno = errno;

	    if (unlink (tmp) < 0 && ! existence_error (errno))
		error (0, errno, "failed to remove lock %s", tmp);

	    /* free the lock dir if we created it */
	    if (status == L_OK)
	    {
		clear_lock();
	    }

	    /* return the error */
	    error (0, xerrno, "cannot create write lock in repository `%s'",
		   repository);
	    return (L_ERROR);
	}
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

    if ((dirp = opendir (repository)) == NULL)
	error (1, 0, "cannot open directory %s", repository);

    errno = 0;
    while ((dp = readdir (dirp)) != NULL)
    {
	if (fnmatch (CVSRFLPAT, dp->d_name, 0) == 0)
	{
#ifdef CVS_FUDGELOCKS
	    time_t now;
	    (void) time (&now);
#endif

	    line = xmalloc (strlen (repository) + strlen (dp->d_name) + 5);
	    (void) sprintf (line, "%s/%s", repository, dp->d_name);
	    if (stat (line, &sb) != -1)
	    {
#ifdef CVS_FUDGELOCKS
		/*
		 * If the create time of the file is more than CVSLCKAGE 
		 * seconds ago, try to clean-up the lock file, and if
		 * successful, re-open the directory and try again.
		 */
		if (now >= (sb.st_ctime + CVSLCKAGE) && unlink (line) != -1)
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

    if ((pw = (struct passwd *) getpwuid (statp->st_uid)) !=
	(struct passwd *) NULL)
    {
	(void) strcpy (lockers_name, pw->pw_name);
    }
    else
	(void) sprintf (lockers_name, "uid%lu", (unsigned long) statp->st_uid);
}

/*
 * Persistently tries to make the directory "lckdir",, which serves as a
 * lock. If the create time on the directory is greater than CVSLCKAGE
 * seconds old, just try to remove the directory.
 */
static int
set_lock (repository, will_wait)
    char *repository;
    int will_wait;
{
    struct stat sb;
    mode_t omask;
#ifdef CVS_FUDGELOCKS
    time_t now;
#endif

    (void) sprintf (masterlock, "%s/%s", repository, CVSLCK);

    /*
     * Note that it is up to the callers of set_lock() to arrange for signal
     * handlers that do the appropriate things, like remove the lock
     * directory before they exit.
     */
    cleanup_lckdir = 0;
    for (;;)
    {
	int status = -1;
	omask = umask (cvsumask);
	SIG_beginCrSect ();
	if (CVS_MKDIR (masterlock, 0777) == 0)
	{
#ifdef AFSCVS
	    char uidlock[PATH_MAX];
	    FILE *fp;

	    sprintf(uidlock, "%s/uidlock%d", masterlock, geteuid() );
	    if ((fp = fopen(uidlock, "w+")) == NULL)
	    {
		/* We failed to create the uidlock,
		   so rm masterlock and leave */
		rmdir(masterlock);
		SIG_endCrSect ();
		status = L_ERROR;
		goto out;
	    }

	    /* We successfully created the uid lock, so close the file */
	    fclose(fp);
#endif
	    cleanup_lckdir = 1;
	    SIG_endCrSect ();
	    status = L_OK;
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
		   repository);
	    return (L_ERROR);
	}

	/*
	 * stat the dir - if it is non-existent, re-try the loop since
	 * someone probably just removed it (thus releasing the lock)
	 */
	if (stat (masterlock, &sb) < 0)
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
#ifdef AFSCVS
	  /* Remove the uidlock first */
	  char rmuidlock[PATH_MAX];
	  sprintf(rmuidlock, "rm -f %s/uidlock%d", masterlock, geteuid() );
	  system(rmuidlock);
#endif
	    if (rmdir (masterlock) >= 0)
		continue;
	}
#endif

	/* set the lockers name */
	set_lockers_name (&sb);

	/* if he wasn't willing to wait, return an error */
	if (!will_wait)
	    return (L_LOCKED);
	lock_wait (repository);
    }
}

/*
 * Clear master lock.  We don't have to recompute the lock name since
 * clear_lock is never called except after a successful set_lock().
 */
static void
clear_lock()
{
#ifdef AFSCVS
  /* Remove the uidlock first */
  char rmuidlock[PATH_MAX];
  sprintf(rmuidlock, "rm -f %s/uidlock%d", masterlock, geteuid() );
  system(rmuidlock);
#endif
    if (rmdir (masterlock) < 0)
	error (0, errno, "failed to remove lock dir `%s'", masterlock);
    cleanup_lckdir = 0;
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
    (void) sleep (CVSLCKSLEEP);
}

static int lock_filesdoneproc PROTO ((int err, char *repository,
				      char *update_dir));
static int fsortcmp PROTO((const Node * p, const Node * q));

static List *lock_tree_list;

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
			   (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, argc,
			   argv, local, W_LOCAL, aflag, 0, (char *) NULL, 0,
			   0);
    sortlist (lock_tree_list, fsortcmp);
    if (Writer_Lock (lock_tree_list) != 0)
	error (1, 0, "lock failed - giving up");
}

void
lock_tree_cleanup ()
{
    Lock_Cleanup ();
    dellist (&lock_tree_list);
}
