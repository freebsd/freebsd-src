/*
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)mtab_file.c	8.1 (Berkeley) 6/6/93
 *
 * $Id: mtab_file.c,v 5.2.2.1 1992/02/09 15:10:42 jsp beta $
 *
 */

#include "am.h"

#ifdef READ_MTAB_FROM_FILE

#ifdef USE_FCNTL
#include <fcntl.h>
#else
#include <sys/file.h>
#endif /* USE_FCNTL */

#ifdef UPDATE_MTAB

/*
 * Do strict /etc/mtab locking
 */
#define	MTAB_LOCKING

/*
 * Firewall mtab entries
 */
#define	MTAB_STRIPNL

#include <sys/stat.h>
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
#define	NFILE_RETRIES	10 /* seconds */

#ifdef MTAB_LOCKING
#ifdef LOCK_FCNTL
static int lock(fd)
{
	int rc;
	struct flock lk;

	lk.l_type = F_WRLCK;
	lk.l_whence = 0;
	lk.l_start = 0;
	lk.l_len = 0;

again:
	rc = fcntl(fd, F_SETLKW, (caddr_t) &lk);
	if (rc < 0 && (errno == EACCES || errno == EAGAIN)) {
#ifdef DEBUG
		dlog("Blocked, trying to obtain exclusive mtab lock");
#endif /* DEBUG */
		sleep(1);
		goto again;
	}
	return rc;
}
#else
#define lock(fd) (flock((fd), LOCK_EX))
#endif /* LOCK_FCNTL */
#endif /* MTAB_LOCKING */

static FILE *open_locked_mtab(mtab_file, mode, fs)
char *mtab_file;
char *mode;
char *fs;
{
	FILE *mfp = 0;

#ifdef UPDATE_MTAB
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
#ifdef MTAB_LOCKING
	int rc;
	int retries = 0;
	struct stat st_before, st_after;
#endif /* MTAB_LOCKING */

	if (mnt_file) {
#ifdef DEBUG
		dlog("Forced close on %s in read_mtab", mtab_file);
#endif /* DEBUG */
		endmntent(mnt_file);
		mnt_file = 0;
	}

#ifdef MTAB_LOCKING
again:
	if (mfp) {
		endmntent(mfp);
		mfp = 0;
	}

	clock_valid = 0;
	if (stat(mtab_file, &st_before) < 0) {
		plog(XLOG_ERROR, "%s: stat: %m", mtab_file);
		if (errno == ESTALE) {
			/* happens occasionally */
			sleep(1);
			goto again;
		}
		return 0;
	}
#endif /* MTAB_LOCKING */
#endif /* UPDATE_MTAB */

eacces:
	mfp = setmntent(mtab_file, mode);
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

		plog(XLOG_ERROR, "setmntent(\"%s\", \"%s\"): %m", mtab_file, mode);
		return 0;
	}

#ifdef MTAB_LOCKING
#ifdef UPDATE_MTAB
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
		plog(XLOG_ERROR, "Couldn't lock %s: %m", mtab_file);
		endmntent(mfp);
		return 0;
	}
	/*
	 * Now check whether the mtab file has changed under our feet
	 */
	if (stat(mtab_file, &st_after) < 0) {
		plog(XLOG_ERROR, "%s: stat", mtab_file);
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
			racing = (racing+1) & 3;
			/*
			 * Take a nap.  From: Doug Kingston <dpk@morgan.com>
			 */
			tv.tv_sec = 0;
			tv.tv_usec = (mypid & 0x07) << 17;
			if (tv.tv_usec)
				if (select(0, (voidp) 0, (voidp) 0, (voidp) 0, &tv) < 0)
					plog(XLOG_WARNING, "mtab nap failed: %m");

			goto again;
	}
#endif /* UPDATE_MTAB */
#endif /* MTAB_LOCKING */

	return mfp;
}

/*
 * Unlock the mount table
 */
void unlock_mntlist P((void));
void unlock_mntlist()
{
	/*
	 * Release file lock, by closing the file
	 */
	if (mnt_file) {
		endmntent(mnt_file);
		mnt_file = 0;
	}
}

/*
 * Write out a mount list
 */
void rewrite_mtab(mp)
mntlist *mp;
{
	FILE *mfp;
	int error = 0;

	/*
	 * Concoct a temporary name in the same
	 * directory as the target mount table
	 * so that rename() will work.
	 */
	char tmpname[64];
	int retries;
	int tmpfd;
	char *cp;
	char *mcp = mtab;
	cp = strrchr(mcp, '/');
	if (cp) {
		bcopy(mcp, tmpname, cp - mcp);
		tmpname[cp-mcp] = '\0';
	} else {
		plog(XLOG_WARNING, "No '/' in mtab (%s), using \".\" as tmp directory", mtab);
		tmpname[0] = '.'; tmpname[1] = '\0';
	}
	strcat(tmpname, "/mtabXXXXXX");
	mktemp(tmpname);
	retries = 0;
enfile1:
	if ((tmpfd = open(tmpname, O_RDWR|O_CREAT|O_TRUNC, 0644)) < 0) {
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
			if (addmntent(mfp, mp->mnt)) {
				plog(XLOG_ERROR, "Can't write entry to %s", tmpname);
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
	if (rename(tmpname, mtab) < 0) {
		plog(XLOG_ERROR, "rename %s to %s: %m", tmpname, mtab);
		error = 1;
		goto out;
	}

out:
	if (error)
		(void) unlink(tmpname);
}

#ifdef MTAB_STRIPNL
static void mtab_stripnl(s)
char *s;
{
	do {
		s = strchr(s, '\n');
		if (s)
			*s++ = ' ';
	} while (s);
}
#endif /* MTAB_STRIPNL */

/*
 * Append a mntent structure to the
 * current mount table.
 */
void write_mntent(mp)
struct mntent *mp;
{
	int retries = 0;
	FILE *mfp;
enfile:
	mfp = open_locked_mtab(mtab, "a", mp->mnt_dir);
	if (mfp) {
#ifdef MTAB_STRIPNL
		mtab_stripnl(mp->mnt_opts);
#endif /* MTAB_STRIPNL */
		if (addmntent(mfp, mp))
			plog(XLOG_ERROR, "Couldn't write %s: %m", mtab);
		if (fflush(mfp))
			plog(XLOG_ERROR, "Couldn't flush %s: %m", mtab);
		(void) endmntent(mfp);
	} else {
		if (errno == ENFILE && retries < NFILE_RETRIES) {
			sleep(1);
			goto enfile;
		}
		plog(XLOG_ERROR, "setmntent(\"%s\", \"a\"): %m", mtab);
	}
}

#endif /* UPDATE_MTAB */

static struct mntent *mnt_dup(mp)
struct mntent *mp;
{
	struct mntent *new_mp = ALLOC(mntent);

	new_mp->mnt_fsname = strdup(mp->mnt_fsname);
	new_mp->mnt_dir = strdup(mp->mnt_dir);
	new_mp->mnt_type = strdup(mp->mnt_type);
	new_mp->mnt_opts = strdup(mp->mnt_opts);

	new_mp->mnt_freq = mp->mnt_freq;
	new_mp->mnt_passno = mp->mnt_passno;

#ifdef FIXUP_MNTENT_DUP
	/*
	 * Additional fields get dup'ed here
	 */
	FIXUP_MNTENT_DUP(new_mp, mp);
#endif

	return new_mp;
}

/*
 * Read a mount table into memory
 */
mntlist *read_mtab(fs)
char *fs;
{
	mntlist **mpp, *mhp;

	struct mntent *mep;
	FILE *mfp = open_locked_mtab(mtab, "r+", fs);

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
		*mpp = ALLOC(mntlist);

		/*
		 * Copy the data returned by getmntent
		 */
		(*mpp)->mnt = mnt_dup(mep);

		/*
		 * Move to next pointer
		 */
		mpp = &(*mpp)->mnext;
	}
	*mpp = 0;

#ifdef UPDATE_MTAB
	/*
	 * If we are not updating the mount table then we
	 * can free the resources held here, otherwise they
	 * must be held until the mount table update is complete
	 */
	mnt_file = mfp;
#else
	endmntent(mfp);
#endif /* UPDATE_MTAB */

	return mhp;
}

#endif /* READ_MTAB_FROM_FILE */
