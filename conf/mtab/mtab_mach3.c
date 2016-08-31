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
 * File: am-utils/conf/mtab/mtab_mach3.c
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>

#define	NFILE_RETRIES	10	/* number of retries (seconds) */

static FILE *mnt_file;


/*
 * If the system is being trashed by something, then
 * opening mtab may fail with ENFILE.  So, go to sleep
 * for a second and try again. (Yes - this has happened to me.)
 *
 * Note that this *may* block the automounter, oh well.
 * If we get to this state then things are badly wrong anyway...
 *
 * Give the system 10 seconds to recover but then give up.
 * Hopefully something else will exit and free up some file
 * table slots in that time.
 */
#ifdef HAVE_FCNTL_H
static int
lock(int fd)
{
  int rc;
  struct flock lk;

  lk.l_type = F_WRLCK;
  lk.l_whence = 0;
  lk.l_start = 0;
  lk.l_len = 0;

again:
  rc = fcntl(fd, F_SETLKW, (caddr_t) & lk);
  if (rc < 0 && (errno == EACCES || errno == EAGAIN)) {
# ifdef DEBUG
    dlog("Blocked, trying to obtain exclusive mtab lock");
# endif /* DEBUG */
    sleep(1);
    goto again;
  }
  return rc;
}
#else /* not HAVE_FCNTL_H */
# define lock(fd) (flock((fd), LOCK_EX))
#endif /* not HAVE_FCNTL_H */


static FILE *
open_locked_mtab(char *mnttabname, char *mode, char *fs)
{
  FILE *mfp = NULL;

  /*
   * There is a possible race condition if two processes enter
   * this routine at the same time.  One will be blocked by the
   * exclusive lock below (or by the shared lock in setmntent)
   * and by the time the second process has the exclusive lock
   * it will be on the wrong underlying object.  To check for this
   * the mtab file is stat'ed before and after all the locking
   * sequence, and if it is a different file then we assume that
   * it may be the wrong file (only "may", since there is another
   * race between the initial stat and the setmntent).
   *
   * Simpler solutions to this problem are invited...
   */
  int racing = 2;
  int rc;
  int retries = 0;
  struct stat st_before, st_after;

  if (mnt_file) {
#ifdef DEBUG
    dlog("Forced close on %s in read_mtab", mnttabname);
#endif /* DEBUG */
    endmntent(mnt_file);
    mnt_file = NULL;
  }
again:
  if (mfp) {
    endmntent(mfp);
    mfp = NULL;
  }
  if (stat(mnttabname, &st_before) < 0) {
    plog(XLOG_ERROR, "%s: stat: %m", mnttabname);
    if (errno == ESTALE) {
      /* happens occasionally */
      sleep(1);
      goto again;
    }
    /*
     * If 'mnttabname' file does not exist give setmntent() a
     * chance to create it (depending on the mode).
     * Otherwise, bail out.
     */
    else if (errno != ENOENT) {
      return 0;
    }
  }
eacces:
  mfp = setmntent(mnttabname, mode);
  if (!mfp) {
    /*
     * Since setmntent locks the descriptor, it
     * is possible it can fail... so retry if
     * needed.
     */
    if (errno == EACCES || errno == EAGAIN) {
#ifdef DEBUG
      dlog("Blocked, trying to obtain exclusive mtab lock");
#endif /* DEBUG */
      goto eacces;
    } else if (errno == ENFILE && retries++ < NFILE_RETRIES) {
      sleep(1);
      goto eacces;
    }
    plog(XLOG_ERROR, "setmntent(\"%s\", \"%s\"): %m", mnttabname, mode);
    return 0;
  }
  /*
   * At this point we have an exclusive lock on the mount list,
   * but it may be the wrong one so...
   */

  /*
   * Need to get an exclusive lock on the current
   * mount table until we have a new copy written
   * out, when the lock is released in free_mntlist.
   * flock is good enough since the mount table is
   * not shared between machines.
   */
  do
    rc = lock(fileno(mfp));
  while (rc < 0 && errno == EINTR);
  if (rc < 0) {
    plog(XLOG_ERROR, "Couldn't lock %s: %m", mnttabname);
    endmntent(mfp);
    return 0;
  }
  /*
   * Now check whether the mtab file has changed under our feet
   */
  if (stat(mnttabname, &st_after) < 0) {
    plog(XLOG_ERROR, "%s: stat", mnttabname);
    goto again;
  }
  if (st_before.st_dev != st_after.st_dev ||
      st_before.st_ino != st_after.st_ino) {
    struct timeval tv;
    if (racing == 0) {
      /* Sometimes print a warning */
      plog(XLOG_WARNING,
	   "Possible mount table race - retrying %s", fs);
    }
    racing = (racing + 1) & 3;
    /*
     * Take a nap.  From: Doug Kingston <dpk@morgan.com>
     */
    tv.tv_sec = 0;
    tv.tv_usec = (am_mypid & 0x07) << 17;
    if (tv.tv_usec)
      if (select(0, (voidp) 0, (voidp) 0, (voidp) 0, &tv) < 0)
	plog(XLOG_WARNING, "mtab nap failed: %m");

    goto again;
  }
  return mfp;
}


/*
 * Unlock the mount table
 */
void
unlock_mntlist(void)
{
  /*
   * Release file lock, by closing the file
   */
  if (mnt_file) {
    dlog("unlock_mntlist: releasing");
    endmntent(mnt_file);
    mnt_file = NULL;
  }
}


/*
 *      routine to convert notation "/@honeydew" to the notation
 *      honeydew:/ and vice versa (honeydew:/ to /@honeydew)
 *      This lets you put /@honeydew in /etc/fstab without changing
 *      fstab.c and it lets you use EITHER notation on the command line!
 */
static char *
convert(register char *s, char bad, char good)
{
  char *index();
  register char *t, *p;
  register int len1, len2, i;
  char *ptr;

  if ((p = index(s, bad)) == NULL) {
    return (s);
  }
  ptr = t = (char *) xzalloc(MAXPATHLEN * sizeof(char));
  len1 = p - s;
  len2 = strlen(s) - len1 - 1;
  p++;
  for (i = 0; i < len2; i++)
    *t++ = p[i];
  *t++ = good;
  for (i = 0; i < len1; i++)
    *t++ = s[i];
  return (ptr);
}


static
mntprtent3(FILE *mnttabp, register mntent_t *mnt)
{
  char *cvtd = convert(mnt->mnt_fsname, ':', '@');

  dlog("%x:%s:%s:%s:%d:%d:%s:%s:\n",
       mnttabp,
       (cvtd ? cvtd : ""),
       (mnt->mnt_dir ? mnt->mnt_dir : ""),
       (mnt->mnt_opts ? mnt->mnt_opts : ""),
       mnt->mnt_freq,
       mnt->mnt_passno,
       (mnt->mnt_type ? mnt->mnt_type : ""),
       (mnt->mnt_opts2 ? mnt->mnt_opts2 : "")
    );
  fprintf(mnttabp, "%s:%s:%s:%d:%d:%s:%s:\n",
	  (cvtd ? cvtd : ""),
	  (mnt->mnt_dir ? mnt->mnt_dir : ""),
	  (mnt->mnt_opts ? mnt->mnt_opts : ""),
	  mnt->mnt_freq,
	  mnt->mnt_passno,
	  (mnt->mnt_type ? mnt->mnt_type : ""),
	  (mnt->mnt_opts2 ? mnt->mnt_opts2 : "")
    );
  XFREE(cvtd);
  cvtd = NULL;
  return (0);
}


addmntent3(FILE *mnttabp, register mntent_t *mnt)
{
  if (fseek(mnttabp, 0, 2) < 0) {
    return (1);
  }
  mntprtent3(mnttabp, mnt);
  return (0);
}


/*
 * Write out a mount list
 */
void
rewrite_mtab(mntlist *mp, const char *mnttabname)
{
  FILE *mfp;
  int error = 0;
  /*
   * Concoct a temporary name in the same directory as the target mount
   * table so that rename() will work.
   */
  char tmpname[64];
  int retries;
  int tmpfd;
  char *cp;
  char *mcp = mnttabname;

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
    plog(XLOG_ERROR, "Couldn't close tmp file descriptor: %m");

  retries = 0;
enfile2:
  mfp = setmntent(tmpname, "w");
  if (!mfp) {
    if (errno == ENFILE && retries++ < NFILE_RETRIES) {
      sleep(1);
      goto enfile2;
    }
    plog(XLOG_ERROR, "setmntent(\"%s\", \"w\"): %m", tmpname);
    error = 1;
    goto out;
  }
  while (mp) {
    if (mp->mnt) {
      if (addmntent3(mfp, mp->mnt)) {
	plog(XLOG_ERROR, "Can't write entry to %s", tmpname);
	error = 1;
	goto out;
      }
    }
    mp = mp->mnext;
  }

  /*
   * SunOS 4.1 manuals say that the return code from endmntent()
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
enfile:
  mfp = open_locked_mtab(mnttabname, "a", mp->mnt_dir);
  if (mfp) {
    mtab_stripnl(mp->mnt_opts);
    if (addmntent3(mfp, mp))
      plog(XLOG_ERROR, "Couldn't write %s: %m", mnttabname);
    if (fflush(mfp))
      plog(XLOG_ERROR, "Couldn't flush %s: %m", mnttabname);
    (void) endmntent(mfp);
  } else {
    if (errno == ENFILE && retries < NFILE_RETRIES) {
      sleep(1);
      goto enfile;
    }
    plog(XLOG_ERROR, "setmntent(\"%s\", \"a\"): %m", mnttabname);
  }
}


static mntent_t *
mnt_dup(mntent_t *mp)
{
  mntent_t *new_mp = ALLOC(mntent_t);

  new_mp->mnt_fsname = convert(mp->mnt_fsname, '@', ':');

  new_mp->mnt_dir = xstrdup(mp->mnt_dir);
  new_mp->mnt_type = xstrdup(mp->mnt_type);
  new_mp->mnt_opts = xstrdup(mp->mnt_opts);

  new_mp->mnt_freq = mp->mnt_freq;
  new_mp->mnt_passno = mp->mnt_passno;

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
  while (mep = getmntent(mfp)) {
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

  /*
   * If we are not updating the mount table then we
   * can free the resources held here, otherwise they
   * must be held until the mount table update is complete
   */
  mnt_file = mfp;

  return mhp;
}
