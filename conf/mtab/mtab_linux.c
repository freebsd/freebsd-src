/*
 * Copyright (c) 1997-2014 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 * File: am-utils/conf/mtab/mtab_linux.c
 *
 */

/* This file was adapted by Red Hat for Linux from mtab_file.c */

/*
 * The locking code must be kept in sync with that used
 * by the mount command in util-linux, otherwise you'll
 * end with with race conditions leading to a corrupt
 * /etc/mtab, particularly when AutoFS is used on same
 * machine as AMD.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>

#define NFILE_RETRIES   10      /* number of retries (seconds) */
#define LOCK_TIMEOUT    10

#ifdef MOUNT_TABLE_ON_FILE

# define PROC_MOUNTS             "/proc/mounts"

static FILE *mnt_file = NULL;
/* Information about mtab. ------------------------------------*/
static int have_mtab_info = 0;
static int var_mtab_does_not_exist = 0;
static int var_mtab_is_a_symlink = 0;
/* Flag for already existing lock file. */
static int we_created_lockfile = 0;
static int lockfile_fd = -1;


static void
get_mtab_info(void)
{
  struct stat mtab_stat;

  if (!have_mtab_info) {
    if (lstat(MOUNTED, &mtab_stat))
      var_mtab_does_not_exist = 1;
    else if (S_ISLNK(mtab_stat.st_mode))
      var_mtab_is_a_symlink = 1;
    have_mtab_info = 1;
  }
}


static int
mtab_is_a_symlink(void)
{
  get_mtab_info();
  return var_mtab_is_a_symlink;
}


static int
mtab_is_writable()
{
  static int ret = -1;

  /*
   * Should we write to /etc/mtab upon an update?  Probably not if it is a
   * symlink to /proc/mounts, since that would create a file /proc/mounts in
   * case the proc filesystem is not mounted.
   */
  if (mtab_is_a_symlink())
    return 0;

  if (ret == -1) {
    int fd = open(MOUNTED, O_RDWR | O_CREAT, 0644);
    if (fd >= 0) {
      close(fd);
      ret = 1;
    } else
      ret = 0;
  }
  return ret;
}


static void
setlkw_timeout(int sig)
{
  /* nothing, fcntl will fail anyway */
}


/*
 * Create the lock file.
 * The lock file will be removed if we catch a signal or when we exit.
 *
 * The old code here used flock on a lock file /etc/mtab~ and deleted
 * this lock file afterwards.  However, as rgooch remarks, that has a
 * race: a second mount may be waiting on the lock and proceed as
 * soon as the lock file is deleted by the first mount, and immediately
 * afterwards a third mount comes, creates a new /etc/mtab~, applies
 * flock to that, and also proceeds, so that the second and third mount
 * now both are scribbling in /etc/mtab.
 * The new code uses a link() instead of a creat(), where we proceed
 * only if it was us that created the lock, and hence we always have
 * to delete the lock afterwards.  Now the use of flock() is in principle
 * superfluous, but avoids an arbitrary sleep().
 */

/*
 * Where does the link point to?  Obvious choices are mtab and mtab~~.
 * HJLu points out that the latter leads to races.  Right now we use
 * mtab~.<pid> instead.
 */
#define MOUNTED_LOCK "/etc/mtab~"
#define MOUNTLOCK_LINKTARGET           MOUNTED_LOCK "%d"

int
lock_mtab(void)
{
  int tries = 100000, i;
  char *linktargetfile;
  size_t l;
  int rc = 1;

  /*
   * Redhat's original code set a signal handler called "handler()" for all
   * non-ALRM signals.  The handler called unlock_mntlist(), plog'ed the
   * signal name, and then exit(1)!  Never, ever, exit() from inside a
   * utility function.  This messed up Amd's careful signal-handling code,
   * and caused Amd to abort uncleanly only any other "innocent" signal
   * (even simple SIGUSR1), leaving behind a hung Amd mnt point.  That code
   * should have at least restored the signal handlers' states upon a
   * successful mtab unlocking.  Anyway, that handler was unnecessary,
   * because will call unlock_mntlist() properly anyway on exit.
   */
  setup_sighandler(SIGALRM, setlkw_timeout);

  /* somewhat clumsy, but some ancient systems do not have snprintf() */
  /* use 20 as upper bound for the length of %d output */
  l = strlen(MOUNTLOCK_LINKTARGET) + 20;
  linktargetfile = xmalloc(l);
  xsnprintf(linktargetfile, l, MOUNTLOCK_LINKTARGET, getpid());

  i = open(linktargetfile, O_WRONLY|O_CREAT, 0);
  if (i < 0) {
    int errsv = errno;
    /*
     * linktargetfile does not exist (as a file) and we cannot create
     * it. Read-only filesystem?  Too many files open in the system?
     * Filesystem full?
     */
    plog(XLOG_ERROR, "%s: can't create lock file %s: %s "
	 "(use -n flag to override)", __func__,
	 linktargetfile, strerror(errsv));
    goto error;
  }
  close(i);


  /* Repeat until it was us who made the link */
  while (!we_created_lockfile) {
    struct flock flock;
    int errsv, j;

    j = link(linktargetfile, MOUNTED_LOCK);
    errsv = errno;

    if (j < 0 && errsv != EEXIST) {
      (void) unlink(linktargetfile);
      plog(XLOG_ERROR, "can't link lock file %s: %s ",
	   MOUNTED_LOCK, strerror(errsv));
      rc = 0;
      goto error;
    }

    lockfile_fd = open(MOUNTED_LOCK, O_WRONLY);
    if (lockfile_fd < 0) {
      int errsv = errno;
      /* Strange... Maybe the file was just deleted? */
      if (errno == ENOENT && tries-- > 0) {
	if (tries % 200 == 0)
	  usleep(30);
	continue;
      }
      (void) unlink(linktargetfile);
      plog(XLOG_ERROR,"%s: can't open lock file %s: %s ", __func__,
	   MOUNTED_LOCK, strerror(errsv));
      rc = 0;
      goto error;
    }

    flock.l_type = F_WRLCK;
    flock.l_whence = SEEK_SET;
    flock.l_start = 0;
    flock.l_len = 0;

    if (j == 0) {
      /* We made the link. Now claim the lock. */
      if (fcntl(lockfile_fd, F_SETLK, &flock) == -1) {
	int errsv = errno;
	plog(XLOG_ERROR, "%s: Can't lock lock file %s: %s", __func__,
	     MOUNTED_LOCK, strerror(errsv));
	/* proceed, since it was us who created the lockfile anyway */
      }
      we_created_lockfile = 1;
      (void) unlink(linktargetfile);
    } else {
      static int tries = 0;

      /* Someone else made the link. Wait. */
      alarm(LOCK_TIMEOUT);

      if (fcntl(lockfile_fd, F_SETLKW, &flock) == -1) {
	int errsv = errno;
	(void) unlink(linktargetfile);
	plog(XLOG_ERROR, "%s: can't lock lock file %s: %s", __func__,
	     MOUNTED_LOCK, (errno == EINTR) ?
	     "timed out" : strerror(errsv));
	rc = 0;
	goto error;
      }
      alarm(0);
      /*
       * Limit the number of iterations - maybe there
       * still is some old /etc/mtab~
       */
      ++tries;
      if (tries % 200 == 0)
	usleep(30);
      if (tries > 100000) {
	(void) unlink(linktargetfile);
	close(lockfile_fd);
	plog(XLOG_ERROR,
	     "%s: Cannot create link %s; Perhaps there is a stale lock file?",
	     __func__, MOUNTED_LOCK);
	rc = 0;
	goto error;
      }
      close(lockfile_fd);
    }
  }

error:
  XFREE(linktargetfile);

  return rc;
}


static FILE *
open_locked_mtab(const char *mnttabname, char *mode, char *fs)
{
  FILE *mfp = NULL;

  if (mnt_file) {
    dlog("Forced close on %s in read_mtab", mnttabname);
    endmntent(mnt_file);
    mnt_file = NULL;
  }

  if (!mtab_is_a_symlink() &&
      !lock_mtab()) {
    plog(XLOG_ERROR, "%s: Couldn't lock mtab", __func__);
    return 0;
  }

  mfp = setmntent((char *)mnttabname, mode);
  if (!mfp) {
    plog(XLOG_ERROR, "%s: setmntent(\"%s\", \"%s\"): %m", __func__, mnttabname,
	mode);
    return 0;
  }
  return mfp;
}


/*
 * Unlock the mount table
 */
void
unlock_mntlist(void)
{
  if (mnt_file || we_created_lockfile)
    dlog("unlock_mntlist: releasing");
  if (mnt_file) {
    endmntent(mnt_file);
    mnt_file = NULL;
  }
  if (we_created_lockfile) {
    close(lockfile_fd);
    lockfile_fd = -1;
    unlink(MOUNTED_LOCK);
    we_created_lockfile = 0;
  }
}


/*
 * Write out a mount list
 */
void
rewrite_mtab(mntlist *mp, const char *mnttabname)
{
  FILE *mfp;
  int error = 0;
  char tmpname[64];
  int retries;
  int tmpfd;
  char *cp;
  char mcp[128];

  if (!mtab_is_writable()) {
    return;
  }

  /*
   * Concoct a temporary name in the same directory as the target mount
   * table so that rename() will work.
   */
  xstrlcpy(mcp, mnttabname, sizeof(mcp));
  cp = strrchr(mcp, '/');
  if (cp) {
    memmove(tmpname, mcp, cp - mcp);
    tmpname[cp - mcp] = '\0';
  } else {
    plog(XLOG_WARNING, "No '/' in mtab (%s), using \".\" as tmp directory", mnttabname);
    tmpname[0] = '.';
    tmpname[1] = '\0';
  }
  xstrlcat(tmpname, "/mtabXXXXXX", sizeof(tmpname));
  retries = 0;
 enfile1:
#ifdef HAVE_MKSTEMP
  tmpfd = mkstemp(tmpname);
  fchmod(tmpfd, 0644);
#else /* not HAVE_MKSTEMP */
  mktemp(tmpname);
  tmpfd = open(tmpname, O_RDWR | O_CREAT | O_TRUNC, 0644);
#endif /* not HAVE_MKSTEMP */
  if (tmpfd < 0) {
    if (errno == ENFILE && retries++ < NFILE_RETRIES) {
      sleep(1);
      goto enfile1;
    }
    plog(XLOG_ERROR, "%s: open: %m", tmpname);
    return;
  }
  if (close(tmpfd) < 0)
    plog(XLOG_ERROR, "%s: Couldn't close tmp file descriptor: %m", __func__);

  retries = 0;
 enfile2:
  mfp = setmntent(tmpname, "w");
  if (!mfp) {
    if (errno == ENFILE && retries++ < NFILE_RETRIES) {
      sleep(1);
      goto enfile2;
    }
    plog(XLOG_ERROR, "%s: setmntent(\"%s\", \"w\"): %m", __func__, tmpname);
    error = 1;
    goto out;
  }
  while (mp) {
    if (mp->mnt) {
      if (addmntent(mfp, mp->mnt)) {
	plog(XLOG_ERROR, "%s: Can't write entry to %s", __func__, tmpname);
	error = 1;
	goto out;
      }
    }
    mp = mp->mnext;
  }

  /*
   * SunOS 4.1 manuals say that the return code from entmntent()
   * is always 1 and to treat as a void.  That means we need to
   * call fflush() to make sure the new mtab file got written.
   */
  if (fflush(mfp)) {
    plog(XLOG_ERROR, "flush new mtab file: %m");
    error = 1;
    goto out;
  }
  (void) endmntent(mfp);

  /*
   * Rename temporary mtab to real mtab
   */
  if (rename(tmpname, mnttabname) < 0) {
    plog(XLOG_ERROR, "rename %s to %s: %m", tmpname, mnttabname);
    error = 1;
    goto out;
  }
 out:
  if (error)
    (void) unlink(tmpname);
}


static void
mtab_stripnl(char *s)
{
  do {
    s = strchr(s, '\n');
    if (s)
      *s++ = ' ';
  } while (s);
}


/*
 * Append a mntent structure to the
 * current mount table.
 */
void
write_mntent(mntent_t *mp, const char *mnttabname)
{
  int retries = 0;
  FILE *mfp;

  if (!mtab_is_writable()) {
    return;
  }

 enfile:
  mfp = open_locked_mtab(mnttabname, "a", mp->mnt_dir);
  if (mfp) {
    mtab_stripnl(mp->mnt_opts);
    if (addmntent(mfp, mp))
      plog(XLOG_ERROR, "%s: Couldn't write %s: %m", __func__, mnttabname);
    if (fflush(mfp))
      plog(XLOG_ERROR, "%s: Couldn't flush %s: %m", __func__, mnttabname);
    (void) endmntent(mfp);
  } else {
    if (errno == ENFILE && retries < NFILE_RETRIES) {
      sleep(1);
      goto enfile;
    }
    plog(XLOG_ERROR, "%s: setmntent(\"%s\", \"a\"): %m", __func__, mnttabname);
  }

  unlock_mntlist();
}

#endif /* MOUNT_TABLE_ON_FILE */


static mntent_t *
mnt_dup(mntent_t *mp)
{
  mntent_t *new_mp = ALLOC(mntent_t);

  new_mp->mnt_fsname = xstrdup(mp->mnt_fsname);
  new_mp->mnt_dir = xstrdup(mp->mnt_dir);
  new_mp->mnt_type = xstrdup(mp->mnt_type);
  new_mp->mnt_opts = xstrdup(mp->mnt_opts);

  new_mp->mnt_freq = mp->mnt_freq;
  new_mp->mnt_passno = mp->mnt_passno;

#ifdef HAVE_MNTENT_T_MNT_TIME
# ifdef HAVE_MNTENT_T_MNT_TIME_STRING
  new_mp->mnt_time = xstrdup(mp->mnt_time);
# else /* not HAVE_MNTENT_T_MNT_TIME_STRING */
  new_mp->mnt_time = mp->mnt_time;
# endif /* not HAVE_MNTENT_T_MNT_TIME_STRING */
#endif /* HAVE_MNTENT_T_MNT_TIME */

#ifdef HAVE_MNTENT_T_MNT_CNODE
  new_mp->mnt_cnode = mp->mnt_cnode;
#endif /* HAVE_MNTENT_T_MNT_CNODE */

  return new_mp;
}


/*
 * Read a mount table into memory
 */
mntlist *
read_mtab(char *fs, const char *mnttabname)
{
  mntlist **mpp, *mhp;

  mntent_t *mep;

  FILE *mfp = open_locked_mtab(mnttabname, "r+", fs);

  if (!mfp)
    return 0;

  mpp = &mhp;

  /*
   * XXX - In SunOS 4 there is (yet another) memory leak
   * which loses 1K the first time getmntent is called.
   * (jsp)
   */
  while ((mep = getmntent(mfp))) {
    /*
     * Allocate a new slot
     */
    *mpp = ALLOC(struct mntlist);

    /*
     * Copy the data returned by getmntent
     */
    (*mpp)->mnt = mnt_dup(mep);

    /*
     * Move to next pointer
     */
    mpp = &(*mpp)->mnext;
  }
  *mpp = NULL;

#ifdef MOUNT_TABLE_ON_FILE
  /*
   * If we are not updating the mount table then we
   * can free the resources held here, otherwise they
   * must be held until the mount table update is complete
   */
  mnt_file = mfp;
#else /* not MOUNT_TABLE_ON_FILE */
  endmntent(mfp);
#endif /* not MOUNT_TABLE_ON_FILE */

  return mhp;
}
