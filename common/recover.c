/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)recover.c	10.21 (Berkeley) 9/15/96";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>		/* XXX: param.h may not have included types.h */
#include <sys/queue.h>
#include <sys/stat.h>

/*
 * We include <sys/file.h>, because the open #defines were found there
 * on historical systems.  We also include <fcntl.h> because the open(2)
 * #defines are found there on newer systems.
 */
#include <sys/file.h>

#include <bitstring.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "pathnames.h"

/*
 * Recovery code.
 *
 * The basic scheme is as follows.  In the EXF structure, we maintain full
 * paths of a b+tree file and a mail recovery file.  The former is the file
 * used as backing store by the DB package.  The latter is the file that
 * contains an email message to be sent to the user if we crash.  The two
 * simple states of recovery are:
 *
 *	+ first starting the edit session:
 *		the b+tree file exists and is mode 700, the mail recovery
 *		file doesn't exist.
 *	+ after the file has been modified:
 *		the b+tree file exists and is mode 600, the mail recovery
 *		file exists, and is exclusively locked.
 *
 * In the EXF structure we maintain a file descriptor that is the locked
 * file descriptor for the mail recovery file.  NOTE: we sometimes have to
 * do locking with fcntl(2).  This is a problem because if you close(2) any
 * file descriptor associated with the file, ALL of the locks go away.  Be
 * sure to remember that if you have to modify the recovery code.  (It has
 * been rhetorically asked of what the designers could have been thinking
 * when they did that interface.  The answer is simple: they weren't.)
 *
 * To find out if a recovery file/backing file pair are in use, try to get
 * a lock on the recovery file.
 *
 * To find out if a backing file can be deleted at boot time, check for an
 * owner execute bit.  (Yes, I know it's ugly, but it's either that or put
 * special stuff into the backing file itself, or correlate the files at
 * boot time, neither of which looks like fun.)  Note also that there's a
 * window between when the file is created and the X bit is set.  It's small,
 * but it's there.  To fix the window, check for 0 length files as well.
 *
 * To find out if a file can be recovered, check the F_RCV_ON bit.  Note,
 * this DOES NOT mean that any initialization has been done, only that we
 * haven't yet failed at setting up or doing recovery.
 *
 * To preserve a recovery file/backing file pair, set the F_RCV_NORM bit.
 * If that bit is not set when ending a file session:
 *	If the EXF structure paths (rcv_path and rcv_mpath) are not NULL,
 *	they are unlink(2)'d, and free(3)'d.
 *	If the EXF file descriptor (rcv_fd) is not -1, it is closed.
 *
 * The backing b+tree file is set up when a file is first edited, so that
 * the DB package can use it for on-disk caching and/or to snapshot the
 * file.  When the file is first modified, the mail recovery file is created,
 * the backing file permissions are updated, the file is sync(2)'d to disk,
 * and the timer is started.  Then, at RCV_PERIOD second intervals, the
 * b+tree file is synced to disk.  RCV_PERIOD is measured using SIGALRM, which
 * means that the data structures (SCR, EXF, the underlying tree structures)
 * must be consistent when the signal arrives.
 *
 * The recovery mail file contains normal mail headers, with two additions,
 * which occur in THIS order, as the FIRST TWO headers:
 *
 *	X-vi-recover-file: file_name
 *	X-vi-recover-path: recover_path
 *
 * Since newlines delimit the headers, this means that file names cannot have
 * newlines in them, but that's probably okay.  As these files aren't intended
 * to be long-lived, changing their format won't be too painful.
 *
 * Btree files are named "vi.XXXX" and recovery files are named "recover.XXXX".
 */

#define	VI_FHEADER	"X-vi-recover-file: "
#define	VI_PHEADER	"X-vi-recover-path: "

static int	 rcv_copy __P((SCR *, int, char *));
static void	 rcv_email __P((SCR *, char *));
static char	*rcv_gets __P((char *, size_t, int));
static int	 rcv_mailfile __P((SCR *, int, char *));
static int	 rcv_mktemp __P((SCR *, char *, char *, int));

/*
 * rcv_tmp --
 *	Build a file name that will be used as the recovery file.
 *
 * PUBLIC: int rcv_tmp __P((SCR *, EXF *, char *));
 */
int
rcv_tmp(sp, ep, name)
	SCR *sp;
	EXF *ep;
	char *name;
{
	struct stat sb;
	int fd;
	char *dp, *p, path[MAXPATHLEN];

	/*
	 * !!!
	 * ep MAY NOT BE THE SAME AS sp->ep, DON'T USE THE LATTER.
	 *
	 *
	 * If the recovery directory doesn't exist, try and create it.  As
	 * the recovery files are themselves protected from reading/writing
	 * by other than the owner, the worst that can happen is that a user
	 * would have permission to remove other user's recovery files.  If
	 * the sticky bit has the BSD semantics, that too will be impossible.
	 */
	if (opts_empty(sp, O_RECDIR, 0))
		goto err;
	dp = O_STR(sp, O_RECDIR);
	if (stat(dp, &sb)) {
		if (errno != ENOENT || mkdir(dp, 0)) {
			msgq(sp, M_SYSERR, "%s", dp);
			goto err;
		}
		(void)chmod(dp, S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX);
	}

	/* Newlines delimit the mail messages. */
	for (p = name; *p; ++p)
		if (*p == '\n') {
			msgq(sp, M_ERR,
		    "055|Files with newlines in the name are unrecoverable");
			goto err;
		}

	(void)snprintf(path, sizeof(path), "%s/vi.XXXXXX", dp);
	if ((fd = rcv_mktemp(sp, path, dp, S_IRWXU)) == -1)
		goto err;
	(void)close(fd);

	if ((ep->rcv_path = strdup(path)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		(void)unlink(path);
err:		msgq(sp, M_ERR,
		    "056|Modifications not recoverable if the session fails");
		return (1);
	}

	/* We believe the file is recoverable. */
	F_SET(ep, F_RCV_ON);
	return (0);
}

/*
 * rcv_init --
 *	Force the file to be snapshotted for recovery.
 *
 * PUBLIC: int rcv_init __P((SCR *));
 */
int
rcv_init(sp)
	SCR *sp;
{
	EXF *ep;
	recno_t lno;

	ep = sp->ep;

	/* Only do this once. */
	F_CLR(ep, F_FIRSTMODIFY);

	/* If we already know the file isn't recoverable, we're done. */
	if (!F_ISSET(ep, F_RCV_ON))
		return (0);

	/* Turn off recoverability until we figure out if this will work. */
	F_CLR(ep, F_RCV_ON);

	/* Test if we're recovering a file, not editing one. */
	if (ep->rcv_mpath == NULL) {
		/* Build a file to mail to the user. */
		if (rcv_mailfile(sp, 0, NULL))
			goto err;

		/* Force a read of the entire file. */
		if (db_last(sp, &lno))
			goto err;

		/* Turn on a busy message, and sync it to backing store. */
		sp->gp->scr_busy(sp,
		    "057|Copying file for recovery...", BUSY_ON);
		if (ep->db->sync(ep->db, R_RECNOSYNC)) {
			msgq_str(sp, M_SYSERR, ep->rcv_path,
			    "058|Preservation failed: %s");
			sp->gp->scr_busy(sp, NULL, BUSY_OFF);
			goto err;
		}
		sp->gp->scr_busy(sp, NULL, BUSY_OFF);
	}

	/* Turn off the owner execute bit. */
	(void)chmod(ep->rcv_path, S_IRUSR | S_IWUSR);

	/* We believe the file is recoverable. */
	F_SET(ep, F_RCV_ON);
	return (0);

err:	msgq(sp, M_ERR,
	    "059|Modifications not recoverable if the session fails");
	return (1);
}

/*
 * rcv_sync --
 *	Sync the file, optionally:
 *		flagging the backup file to be preserved
 *		snapshotting the backup file and send email to the user
 *		sending email to the user if the file was modified
 *		ending the file session
 *
 * PUBLIC: int rcv_sync __P((SCR *, u_int));
 */
int
rcv_sync(sp, flags)
	SCR *sp;
	u_int flags;
{
	EXF *ep;
	int fd, rval;
	char *dp, buf[1024];

	/* Make sure that there's something to recover/sync. */
	ep = sp->ep;
	if (ep == NULL || !F_ISSET(ep, F_RCV_ON))
		return (0);

	/* Sync the file if it's been modified. */
	if (F_ISSET(ep, F_MODIFIED)) {
		SIGBLOCK;
		if (ep->db->sync(ep->db, R_RECNOSYNC)) {
			F_CLR(ep, F_RCV_ON | F_RCV_NORM);
			msgq_str(sp, M_SYSERR,
			    ep->rcv_path, "060|File backup failed: %s");
			SIGUNBLOCK;
			return (1);
		}
		SIGUNBLOCK;

		/* REQUEST: don't remove backing file on exit. */
		if (LF_ISSET(RCV_PRESERVE))
			F_SET(ep, F_RCV_NORM);

		/* REQUEST: send email. */
		if (LF_ISSET(RCV_EMAIL))
			rcv_email(sp, ep->rcv_mpath);
	}

	/*
	 * !!!
	 * Each time the user exec's :preserve, we have to snapshot all of
	 * the recovery information, i.e. it's like the user re-edited the
	 * file.  We copy the DB(3) backing file, and then create a new mail
	 * recovery file, it's simpler than exiting and reopening all of the
	 * underlying files.
	 *
	 * REQUEST: snapshot the file.
	 */
	rval = 0;
	if (LF_ISSET(RCV_SNAPSHOT)) {
		if (opts_empty(sp, O_RECDIR, 0))
			goto err;
		dp = O_STR(sp, O_RECDIR);
		(void)snprintf(buf, sizeof(buf), "%s/vi.XXXXXX", dp);
		if ((fd = rcv_mktemp(sp, buf, dp, S_IRUSR | S_IWUSR)) == -1)
			goto err;
		sp->gp->scr_busy(sp,
		    "061|Copying file for recovery...", BUSY_ON);
		if (rcv_copy(sp, fd, ep->rcv_path) ||
		    close(fd) || rcv_mailfile(sp, 1, buf)) {
			(void)unlink(buf);
			(void)close(fd);
			rval = 1;
		}
		sp->gp->scr_busy(sp, NULL, BUSY_OFF);
	}
	if (0) {
err:		rval = 1;
	}

	/* REQUEST: end the file session. */
	if (LF_ISSET(RCV_ENDSESSION) && file_end(sp, NULL, 1))
		rval = 1;

	return (rval);
}

/*
 * rcv_mailfile --
 *	Build the file to mail to the user.
 */
static int
rcv_mailfile(sp, issync, cp_path)
	SCR *sp;
	int issync;
	char *cp_path;
{
	EXF *ep;
	GS *gp;
	struct passwd *pw;
	size_t len;
	time_t now;
	uid_t uid;
	int fd;
	char *dp, *p, *t, buf[4096], mpath[MAXPATHLEN];
	char *t1, *t2, *t3;

	/*
	 * XXX
	 * MAXHOSTNAMELEN is in various places on various systems, including
	 * <netdb.h> and <sys/socket.h>.  If not found, use a large default.
	 */
#ifndef MAXHOSTNAMELEN
#define	MAXHOSTNAMELEN	1024
#endif
	char host[MAXHOSTNAMELEN];

	gp = sp->gp;
	if ((pw = getpwuid(uid = getuid())) == NULL) {
		msgq(sp, M_ERR,
		    "062|Information on user id %u not found", uid);
		return (1);
	}

	if (opts_empty(sp, O_RECDIR, 0))
		return (1);
	dp = O_STR(sp, O_RECDIR);
	(void)snprintf(mpath, sizeof(mpath), "%s/recover.XXXXXX", dp);
	if ((fd = rcv_mktemp(sp, mpath, dp, S_IRUSR | S_IWUSR)) == -1)
		return (1);

	/*
	 * XXX
	 * We keep an open lock on the file so that the recover option can
	 * distinguish between files that are live and those that need to
	 * be recovered.  There's an obvious window between the mkstemp call
	 * and the lock, but it's pretty small.
	 */
	ep = sp->ep;
	if (file_lock(sp, NULL, NULL, fd, 1) != LOCK_SUCCESS)
		msgq(sp, M_SYSERR, "063|Unable to lock recovery file");
	if (!issync) {
		/* Save the recover file descriptor, and mail path. */
		ep->rcv_fd = fd;
		if ((ep->rcv_mpath = strdup(mpath)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			goto err;
		}
		cp_path = ep->rcv_path;
	}

	/*
	 * XXX
	 * We can't use stdio(3) here.  The problem is that we may be using
	 * fcntl(2), so if ANY file descriptor into the file is closed, the
	 * lock is lost.  So, we could never close the FILE *, even if we
	 * dup'd the fd first.
	 */
	t = sp->frp->name;
	if ((p = strrchr(t, '/')) == NULL)
		p = t;
	else
		++p;
	(void)time(&now);
	(void)gethostname(host, sizeof(host));
	len = snprintf(buf, sizeof(buf),
	    "%s%s\n%s%s\n%s\n%s\n%s%s\n%s%s\n%s\n\n",
	    VI_FHEADER, t,			/* Non-standard. */
	    VI_PHEADER, cp_path,		/* Non-standard. */
	    "Reply-To: root",
	    "From: root (Nvi recovery program)",
	    "To: ", pw->pw_name,
	    "Subject: Nvi saved the file ", p,
	    "Precedence: bulk");		/* For vacation(1). */
	if (len > sizeof(buf) - 1)
		goto lerr;
	if (write(fd, buf, len) != len)
		goto werr;

	len = snprintf(buf, sizeof(buf),
	    "%s%.24s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n\n",
	    "On ", ctime(&now), ", the user ", pw->pw_name,
	    " was editing a file named ", t, " on the machine ",
	    host, ", when it was saved for recovery. ",
	    "You can recover most, if not all, of the changes ",
	    "to this file using the -r option to ", gp->progname, ":\n\n\t",
	    gp->progname, " -r ", t);
	if (len > sizeof(buf) - 1) {
lerr:		msgq(sp, M_ERR, "064|Recovery file buffer overrun");
		goto err;
	}

	/*
	 * Format the message.  (Yes, I know it's silly.)
	 * Requires that the message end in a <newline>.
	 */
#define	FMTCOLS	60
	for (t1 = buf; len > 0; len -= t2 - t1, t1 = t2) {
		/* Check for a short length. */
		if (len <= FMTCOLS) {
			t2 = t1 + (len - 1);
			goto wout;
		}

		/* Check for a required <newline>. */
		t2 = strchr(t1, '\n');
		if (t2 - t1 <= FMTCOLS)
			goto wout;

		/* Find the closest space, if any. */
		for (t3 = t2; t2 > t1; --t2)
			if (*t2 == ' ') {
				if (t2 - t1 <= FMTCOLS)
					goto wout;
				t3 = t2;
			}
		t2 = t3;

		/* t2 points to the last character to display. */
wout:		*t2++ = '\n';

		/* t2 points one after the last character to display. */
		if (write(fd, t1, t2 - t1) != t2 - t1)
			goto werr;
	}

	if (issync) {
		rcv_email(sp, mpath);
		if (close(fd)) {
werr:			msgq(sp, M_SYSERR, "065|Recovery file");
			goto err;
		}
	}
	return (0);

err:	if (!issync)
		ep->rcv_fd = -1;
	if (fd != -1)
		(void)close(fd);
	return (1);
}

/*
 *	people making love
 *	never exactly the same
 *	just like a snowflake
 *
 * rcv_list --
 *	List the files that can be recovered by this user.
 *
 * PUBLIC: int rcv_list __P((SCR *));
 */
int
rcv_list(sp)
	SCR *sp;
{
	struct dirent *dp;
	struct stat sb;
	DIR *dirp;
	FILE *fp;
	int found;
	char *p, *t, file[MAXPATHLEN], path[MAXPATHLEN];

	/* Open the recovery directory for reading. */
	if (opts_empty(sp, O_RECDIR, 0))
		return (1);
	p = O_STR(sp, O_RECDIR);
	if (chdir(p) || (dirp = opendir(".")) == NULL) {
		msgq_str(sp, M_SYSERR, p, "recdir: %s");
		return (1);
	}

	/* Read the directory. */
	for (found = 0; (dp = readdir(dirp)) != NULL;) {
		if (strncmp(dp->d_name, "recover.", 8))
			continue;

		/*
		 * If it's readable, it's recoverable.
		 *
		 * XXX
		 * Should be "r", we don't want to write the file.  However,
		 * if we're using fcntl(2), there's no way to lock a file
		 * descriptor that's not open for writing.
		 */
		if ((fp = fopen(dp->d_name, "r+")) == NULL)
			continue;

		switch (file_lock(sp, NULL, NULL, fileno(fp), 1)) {
		case LOCK_FAILED:
			/*
			 * XXX
			 * Assume that a lock can't be acquired, but that we
			 * should permit recovery anyway.  If this is wrong,
			 * and someone else is using the file, we're going to
			 * die horribly.
			 */
			break;
		case LOCK_SUCCESS:
			break;
		case LOCK_UNAVAIL:
			/* If it's locked, it's live. */
			(void)fclose(fp);
			continue;
		}

		/* Check the headers. */
		if (fgets(file, sizeof(file), fp) == NULL ||
		    strncmp(file, VI_FHEADER, sizeof(VI_FHEADER) - 1) ||
		    (p = strchr(file, '\n')) == NULL ||
		    fgets(path, sizeof(path), fp) == NULL ||
		    strncmp(path, VI_PHEADER, sizeof(VI_PHEADER) - 1) ||
		    (t = strchr(path, '\n')) == NULL) {
			msgq_str(sp, M_ERR, dp->d_name,
			    "066|%s: malformed recovery file");
			goto next;
		}
		*p = *t = '\0';

		/*
		 * If the file doesn't exist, it's an orphaned recovery file,
		 * toss it.
		 *
		 * XXX
		 * This can occur if the backup file was deleted and we crashed
		 * before deleting the email file.
		 */
		errno = 0;
		if (stat(path + sizeof(VI_PHEADER) - 1, &sb) &&
		    errno == ENOENT) {
			(void)unlink(dp->d_name);
			goto next;
		}

		/* Get the last modification time and display. */
		(void)fstat(fileno(fp), &sb);
		(void)printf("%.24s: %s\n",
		    ctime(&sb.st_mtime), file + sizeof(VI_FHEADER) - 1);
		found = 1;

		/* Close, discarding lock. */
next:		(void)fclose(fp);
	}
	if (found == 0)
		(void)printf("vi: no files to recover.\n");
	(void)closedir(dirp);
	return (0);
}

/*
 * rcv_read --
 *	Start a recovered file as the file to edit.
 *
 * PUBLIC: int rcv_read __P((SCR *, FREF *));
 */
int
rcv_read(sp, frp)
	SCR *sp;
	FREF *frp;
{
	struct dirent *dp;
	struct stat sb;
	DIR *dirp;
	EXF *ep;
	time_t rec_mtime;
	int fd, found, locked, requested, sv_fd;
	char *name, *p, *t, *rp, *recp, *pathp;
	char file[MAXPATHLEN], path[MAXPATHLEN], recpath[MAXPATHLEN];

	if (opts_empty(sp, O_RECDIR, 0))
		return (1);
	rp = O_STR(sp, O_RECDIR);
	if ((dirp = opendir(rp)) == NULL) {
		msgq_str(sp, M_ERR, rp, "%s");
		return (1);
	}

	name = frp->name;
	sv_fd = -1;
	rec_mtime = 0;
	recp = pathp = NULL;
	for (found = requested = 0; (dp = readdir(dirp)) != NULL;) {
		if (strncmp(dp->d_name, "recover.", 8))
			continue;
		(void)snprintf(recpath,
		    sizeof(recpath), "%s/%s", rp, dp->d_name);

		/*
		 * If it's readable, it's recoverable.  It would be very
		 * nice to use stdio(3), but, we can't because that would
		 * require closing and then reopening the file so that we
		 * could have a lock and still close the FP.  Another tip
		 * of the hat to fcntl(2).
		 *
		 * XXX
		 * Should be O_RDONLY, we don't want to write it.  However,
		 * if we're using fcntl(2), there's no way to lock a file
		 * descriptor that's not open for writing.
		 */
		if ((fd = open(recpath, O_RDWR, 0)) == -1)
			continue;

		switch (file_lock(sp, NULL, NULL, fd, 1)) {
		case LOCK_FAILED:
			/*
			 * XXX
			 * Assume that a lock can't be acquired, but that we
			 * should permit recovery anyway.  If this is wrong,
			 * and someone else is using the file, we're going to
			 * die horribly.
			 */
			locked = 0;
			break;
		case LOCK_SUCCESS:
			locked = 1;
			break;
		case LOCK_UNAVAIL:
			/* If it's locked, it's live. */
			(void)close(fd);
			continue;
		}

		/* Check the headers. */
		if (rcv_gets(file, sizeof(file), fd) == NULL ||
		    strncmp(file, VI_FHEADER, sizeof(VI_FHEADER) - 1) ||
		    (p = strchr(file, '\n')) == NULL ||
		    rcv_gets(path, sizeof(path), fd) == NULL ||
		    strncmp(path, VI_PHEADER, sizeof(VI_PHEADER) - 1) ||
		    (t = strchr(path, '\n')) == NULL) {
			msgq_str(sp, M_ERR, recpath,
			    "067|%s: malformed recovery file");
			goto next;
		}
		*p = *t = '\0';
		++found;

		/*
		 * If the file doesn't exist, it's an orphaned recovery file,
		 * toss it.
		 *
		 * XXX
		 * This can occur if the backup file was deleted and we crashed
		 * before deleting the email file.
		 */
		errno = 0;
		if (stat(path + sizeof(VI_PHEADER) - 1, &sb) &&
		    errno == ENOENT) {
			(void)unlink(dp->d_name);
			goto next;
		}

		/* Check the file name. */
		if (strcmp(file + sizeof(VI_FHEADER) - 1, name))
			goto next;

		++requested;

		/*
		 * If we've found more than one, take the most recent.
		 *
		 * XXX
		 * Since we're using st_mtime, for portability reasons,
		 * we only get a single second granularity, instead of
		 * getting it right.
		 */
		(void)fstat(fd, &sb);
		if (recp == NULL || rec_mtime < sb.st_mtime) {
			p = recp;
			t = pathp;
			if ((recp = strdup(recpath)) == NULL) {
				msgq(sp, M_SYSERR, NULL);
				recp = p;
				goto next;
			}
			if ((pathp = strdup(path)) == NULL) {
				msgq(sp, M_SYSERR, NULL);
				free(recp);
				recp = p;
				pathp = t;
				goto next;
			}
			if (p != NULL) {
				free(p);
				free(t);
			}
			rec_mtime = sb.st_mtime;
			if (sv_fd != -1)
				(void)close(sv_fd);
			sv_fd = fd;
		} else
next:			(void)close(fd);
	}
	(void)closedir(dirp);

	if (recp == NULL) {
		msgq_str(sp, M_INFO, name,
		    "068|No files named %s, readable by you, to recover");
		return (1);
	}
	if (found) {
		if (requested > 1)
			msgq(sp, M_INFO,
	    "069|There are older versions of this file for you to recover");
		if (found > requested)
			msgq(sp, M_INFO,
			    "070|There are other files for you to recover");
	}

	/*
	 * Create the FREF structure, start the btree file.
	 *
	 * XXX
	 * file_init() is going to set ep->rcv_path.
	 */
	if (file_init(sp, frp, pathp + sizeof(VI_PHEADER) - 1, 0)) {
		free(recp);
		free(pathp);
		(void)close(sv_fd);
		return (1);
	}

	/*
	 * We keep an open lock on the file so that the recover option can
	 * distinguish between files that are live and those that need to
	 * be recovered.  The lock is already acquired, just copy it.
	 */
	ep = sp->ep;
	ep->rcv_mpath = recp;
	ep->rcv_fd = sv_fd;
	if (!locked)
		F_SET(frp, FR_UNLOCKED);

	/* We believe the file is recoverable. */
	F_SET(ep, F_RCV_ON);
	return (0);
}

/*
 * rcv_copy --
 *	Copy a recovery file.
 */
static int
rcv_copy(sp, wfd, fname)
	SCR *sp;
	int wfd;
	char *fname;
{
	int nr, nw, off, rfd;
	char buf[8 * 1024];

	if ((rfd = open(fname, O_RDONLY, 0)) == -1)
		goto err;
	while ((nr = read(rfd, buf, sizeof(buf))) > 0)
		for (off = 0; nr; nr -= nw, off += nw)
			if ((nw = write(wfd, buf + off, nr)) < 0)
				goto err;
	if (nr == 0)
		return (0);

err:	msgq_str(sp, M_SYSERR, fname, "%s");
	return (1);
}

/*
 * rcv_gets --
 *	Fgets(3) for a file descriptor.
 */
static char *
rcv_gets(buf, len, fd)
	char *buf;
	size_t len;
	int fd;
{
	int nr;
	char *p;

	if ((nr = read(fd, buf, len - 1)) == -1)
		return (NULL);
	if ((p = strchr(buf, '\n')) == NULL)
		return (NULL);
	(void)lseek(fd, (off_t)((p - buf) + 1), SEEK_SET);
	return (buf);
}

/*
 * rcv_mktemp --
 *	Paranoid make temporary file routine.
 */
static int
rcv_mktemp(sp, path, dname, perms)
	SCR *sp;
	char *path, *dname;
	int perms;
{
	int fd;

	/*
	 * !!!
	 * We expect mkstemp(3) to set the permissions correctly.  On
	 * historic System V systems, mkstemp didn't.  Do it here, on
	 * GP's.
	 *
	 * XXX
	 * The variable perms should really be a mode_t, and it would
	 * be nice to use fchmod(2) instead of chmod(2), here.
	 */
	if ((fd = mkstemp(path)) == -1)
		msgq_str(sp, M_SYSERR, dname, "%s");
	else
		(void)chmod(path, perms);
	return (fd);
}

/*
 * rcv_email --
 *	Send email.
 */
static void
rcv_email(sp, fname)
	SCR *sp;
	char *fname;
{
	struct stat sb;
	char buf[MAXPATHLEN * 2 + 20];

	if (_PATH_SENDMAIL[0] != '/' || stat(_PATH_SENDMAIL, &sb))
		msgq_str(sp, M_SYSERR,
		    _PATH_SENDMAIL, "071|not sending email: %s");
	else {
		/*
		 * !!!
		 * If you need to port this to a system that doesn't have
		 * sendmail, the -t flag causes sendmail to read the message
		 * for the recipients instead of specifying them some other
		 * way.
		 */
		(void)snprintf(buf, sizeof(buf),
		    "%s -t < %s", _PATH_SENDMAIL, fname);
		(void)system(buf);
	}
}
