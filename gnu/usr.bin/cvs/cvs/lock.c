/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * Set Lock
 * 
 * Lock file support for CVS.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)lock.c 1.42 92/04/10";
#endif

extern char *ctime ();

#if __STDC__
static int readers_exist (char *repository);
static int set_lock (char *lockdir, int will_wait, char *repository);
static void set_lockers_name (struct stat *statp);
static int set_writelock_proc (Node * p);
static int unlock_proc (Node * p);
static int write_lock (char *repository);
static void unlock (char *repository);
static void lock_wait ();
#else
static int unlock_proc ();
static void unlock ();
static int set_writelock_proc ();
static int write_lock ();
static int readers_exist ();
static int set_lock ();
static void set_lockers_name ();
static void lock_wait ();
#endif				/* __STDC__ */

static char lockers_name[20];
static char *repository;
static char readlock[PATH_MAX], writelock[PATH_MAX];
static int cleanup_lckdir;
static List *locklist;

#define L_OK		0		/* success */
#define L_ERROR		1		/* error condition */
#define L_LOCK_OWNED	2		/* lock already owned by us */
#define L_LOCKED	3		/* lock owned by someone else */

/*
 * Clean up all outstanding locks
 */
void
Lock_Cleanup ()
{
    /* clean up simple locks (if any) */
    if (repository != NULL)
    {
	unlock (repository);
	repository = (char *) NULL;
    }

    /* clean up multiple locks (if any) */
    if (locklist != (List *) NULL)
    {
	(void) walklist (locklist, unlock_proc);
	locklist = (List *) NULL;
    }
}

/*
 * walklist proc for removing a list of locks
 */
static int
unlock_proc (p)
    Node *p;
{
    unlock (p->key);
    return (0);
}

/*
 * Remove the lock files (without complaining if they are not there),
 */
static void
unlock (repository)
    char *repository;
{
    char tmp[PATH_MAX];
    struct stat sb;

    if (readlock[0] != '\0')
    {
	(void) sprintf (tmp, "%s/%s", repository, readlock);
	(void) unlink (tmp);
    }

    if (writelock[0] != '\0')
    {
	(void) sprintf (tmp, "%s/%s", repository, writelock);
	(void) unlink (tmp);
    }

    /*
     * Only remove the lock directory if it is ours, note that this does
     * lead to the limitation that one user ID should not be committing
     * files into the same Repository directory at the same time. Oh well.
     */
    (void) sprintf (tmp, "%s/%s", repository, CVSLCK);
    if (stat (tmp, &sb) != -1 && sb.st_uid == geteuid () &&
	(writelock[0] != '\0' || (readlock[0] != '\0' && cleanup_lckdir)))
    {
	(void) rmdir (tmp);
    }
    cleanup_lckdir = 0;
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
	(void) sprintf (readlock, "%s.%d", CVSRFL, getpid ());

    /* remember what we're locking (for lock_cleanup) */
    repository = xrepository;

    /* make sure we clean up on error */
    (void) SIG_register (SIGHUP, Lock_Cleanup);
    (void) SIG_register (SIGINT, Lock_Cleanup);
    (void) SIG_register (SIGQUIT, Lock_Cleanup);
    (void) SIG_register (SIGPIPE, Lock_Cleanup);
    (void) SIG_register (SIGTERM, Lock_Cleanup);

    /* make sure we can write the repository */
    (void) sprintf (tmp, "%s/%s.%d", xrepository, CVSTFL, getpid ());
    if ((fp = fopen (tmp, "w+")) == NULL || fclose (fp) == EOF)
    {
	error (0, errno, "cannot create read lock in repository `%s'",
	       xrepository);
	readlock[0] = '\0';
	(void) unlink (tmp);
	return (1);
    }
    (void) unlink (tmp);

    /* get the lock dir for our own */
    (void) sprintf (tmp, "%s/%s", xrepository, CVSLCK);
    if (set_lock (tmp, 1, xrepository) != L_OK)
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
    (void) sprintf (tmp, "%s/%s", xrepository, CVSLCK);
    if (rmdir (tmp) < 0)
	error (0, errno, "failed to remove lock dir `%s'", tmp);

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

	(void) walklist (list, set_writelock_proc);

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
set_writelock_proc (p)
    Node *p;
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
	(void) sprintf (writelock, "%s.%d", CVSWFL, getpid ());

    /* make sure we clean up on error */
    (void) SIG_register (SIGHUP, Lock_Cleanup);
    (void) SIG_register (SIGINT, Lock_Cleanup);
    (void) SIG_register (SIGQUIT, Lock_Cleanup);
    (void) SIG_register (SIGPIPE, Lock_Cleanup);
    (void) SIG_register (SIGTERM, Lock_Cleanup);

    /* make sure we can write the repository */
    (void) sprintf (tmp, "%s/%s.%d", repository, CVSTFL, getpid ());
    if ((fp = fopen (tmp, "w+")) == NULL || fclose (fp) == EOF)
    {
	error (0, errno, "cannot create write lock in repository `%s'",
	       repository);
	(void) unlink (tmp);
	return (L_ERROR);
    }
    (void) unlink (tmp);

    /* make sure the lock dir is ours (not necessarily unique to us!) */
    (void) sprintf (tmp, "%s/%s", repository, CVSLCK);
    status = set_lock (tmp, 0, repository);
    if (status == L_OK || status == L_LOCK_OWNED)
    {
	/* we now own a writer - make sure there are no readers */
	if (readers_exist (repository))
	{
	    /* clean up the lock dir if we created it */
	    if (status == L_OK)
	    {
		if (rmdir (tmp) < 0)
		    error (0, errno, "failed to remove lock dir `%s'", tmp);
	    }

	    /* indicate we failed due to read locks instead of error */
	    return (L_LOCKED);
	}

	/* write the write-lock file */
	(void) sprintf (tmp, "%s/%s", repository, writelock);
	if ((fp = fopen (tmp, "w+")) == NULL || fclose (fp) == EOF)
	{
	    int xerrno = errno;

	    (void) unlink (tmp);
	    /* free the lock dir if we created it */
	    if (status == L_OK)
	    {
		(void) sprintf (tmp, "%s/%s", repository, CVSLCK);
		if (rmdir (tmp) < 0)
		    error (0, errno, "failed to remove lock dir `%s'", tmp);
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
    char line[MAXLINELEN];
    DIR *dirp;
    struct direct *dp;
    struct stat sb;
    CONST char *regex_err;
    int ret = 0;

#ifdef CVS_FUDGELOCKS
again:
#endif

    if ((dirp = opendir (repository)) == NULL)
	error (1, 0, "cannot open directory %s", repository);

    (void) sprintf (line, "^%s.*", CVSRFL);
    if ((regex_err = re_comp (line)) != NULL)
	error (1, 0, "%s", regex_err);

    while ((dp = readdir (dirp)) != NULL)
    {
	(void) sprintf (line, "%s/%s", repository, dp->d_name);
	if (re_exec (dp->d_name))
	{
#ifdef CVS_FUDGELOCKS
	    time_t now;

	    (void) time (&now);

	    /*
	     * If the create time of the file is more than CVSLCKAGE seconds
	     * ago, try to clean-up the lock file, and if successful, re-open
	     * the directory and try again.
	     */
	    if (stat (line, &sb) != -1)
	    {
		if (now >= (sb.st_ctime + CVSLCKAGE) && unlink (line) != -1)
		{
		    (void) closedir (dirp);
		    goto again;
		}
		set_lockers_name (&sb);
	    }
#else
	    if (stat (line, &sb) != -1)
		set_lockers_name (&sb);
#endif
	    ret = 1;
	    break;
	}
    }
    (void) closedir (dirp);
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
	(void) sprintf (lockers_name, "uid%d", statp->st_uid);
}

/*
 * Persistently tries to make the directory "lckdir",, which serves as a
 * lock. If the create time on the directory is greater than CVSLCKAGE
 * seconds old, just try to remove the directory.
 */
static int
set_lock (lockdir, will_wait, repository)
    char *lockdir;
    int will_wait;
    char *repository;
{
    struct stat sb;
#ifdef CVS_FUDGELOCKS
    time_t now;
#endif

    /*
     * Note that it is up to the callers of set_lock() to arrange for signal
     * handlers that do the appropriate things, like remove the lock
     * directory before they exit.
     */
    cleanup_lckdir = 0;
    for (;;)
    {
	SIG_beginCrSect ();
	if (mkdir (lockdir, 0777) == 0)
	{
	    cleanup_lckdir = 1;
	    SIG_endCrSect ();
	    return (L_OK);
	}
	SIG_endCrSect ();

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
	if (stat (lockdir, &sb) < 0)
	{
	    if (errno == ENOENT)
		continue;

	    error (0, errno, "couldn't stat lock directory `%s'", lockdir);
	    return (L_ERROR);
	}

	/*
	 * if we already own the lock, go ahead and return 1 which means it
	 * existed but we owned it
	 */
	if (sb.st_uid == geteuid () && !will_wait)
	    return (L_LOCK_OWNED);

#ifdef CVS_FUDGELOCKS

	/*
	 * If the create time of the directory is more than CVSLCKAGE seconds
	 * ago, try to clean-up the lock directory, and if successful, just
	 * quietly retry to make it.
	 */
	(void) time (&now);
	if (now >= (sb.st_ctime + CVSLCKAGE))
	{
	    if (rmdir (lockdir) >= 0)
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
