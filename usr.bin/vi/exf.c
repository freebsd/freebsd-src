/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#ifndef lint
static char sccsid[] = "@(#)exf.c	8.65 (Berkeley) 1/11/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

/*
 * We include <sys/file.h>, because the flock(2) #defines were
 * found there on historical systems.  We also include <fcntl.h>
 * because the open(2) #defines are found there on newer systems.
 */
#include <sys/file.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "pathnames.h"

/*
 * file_add --
 *	Insert a file name into the FREF list, if it doesn't already
 *	appear in it.
 *
 * !!!
 * The "if it doesn't already appear" changes vi's semantics slightly.  If
 * you do a "vi foo bar", and then execute "next bar baz", the edit of bar
 * will reflect the line/column of the previous edit session.  Historic nvi
 * did not do this.  The change is a logical extension of the change where
 * vi now remembers the last location in any file that it has ever edited,
 * not just the previously edited file.
 */
FREF *
file_add(sp, frp_append, name, ignore)
	SCR *sp;
	FREF *frp_append;
	CHAR_T *name;
	int ignore;
{
	FREF *frp;
	char *p;

	/*
	 * Return it if it already exists.  Note that we test against the
	 * user's current name, whatever that happens to be, including if
	 * it's a temporary file.  If the user is trying to set an argument
	 * list, the ignore argument will be on -- if we're ignoring the
	 * file turn off the ignore bit, so it's back in the argument list.
	 */
	if (name != NULL)
		for (frp = sp->frefq.cqh_first;
		    frp != (FREF *)&sp->frefq; frp = frp->q.cqe_next)
			if ((p = FILENAME(frp)) != NULL && !strcmp(p, name)) {
				if (!ignore)
					F_CLR(frp, FR_IGNORE);
				return (frp);
			}

	/* Allocate and initialize the FREF structure. */
	CALLOC(sp, frp, FREF *, 1, sizeof(FREF));
	if (frp == NULL)
		return (NULL);

	/*
	 * If no file name specified, or if the file name is a request
	 * for something temporary, file_init() will allocate the file
	 * name.  Temporary files are always ignored.
	 */
#define	TEMPORARY_FILE_STRING	"/tmp"
	if (name != NULL && strcmp(name, TEMPORARY_FILE_STRING) &&
	    (frp->name = strdup(name)) == NULL) {
		FREE(frp, sizeof(FREF));
		msgq(sp, M_SYSERR, NULL);
		return (NULL);
	}

	/* Only the initial argument list is "remembered". */
	if (ignore)
		F_SET(frp, FR_IGNORE);

	/* Append into the chain of file names. */
	if (frp_append != NULL) {
		CIRCLEQ_INSERT_AFTER(&sp->frefq, frp_append, frp, q);
	} else
		CIRCLEQ_INSERT_TAIL(&sp->frefq, frp, q);

	return (frp);
}

/*
 * file_first --
 *	Return the first file name for editing, if any.
 */
FREF *
file_first(sp)
	SCR *sp;
{
	FREF *frp;

	/* Return the first file name. */
	for (frp = sp->frefq.cqh_first;
	    frp != (FREF *)&sp->frefq; frp = frp->q.cqe_next)
		if (!F_ISSET(frp, FR_IGNORE))
			return (frp);
	return (NULL);
}

/*
 * file_next --
 *	Return the next file name, if any.
 */
FREF *
file_next(sp, frp)
	SCR *sp;
	FREF *frp;
{
	while ((frp = frp->q.cqe_next) != (FREF *)&sp->frefq)
		if (!F_ISSET(frp, FR_IGNORE))
			return (frp);
	return (NULL);
}

/*
 * file_prev --
 *	Return the previous file name, if any.
 */
FREF *
file_prev(sp, frp)
	SCR *sp;
	FREF *frp;
{
	while ((frp = frp->q.cqe_prev) != (FREF *)&sp->frefq)
		if (!F_ISSET(frp, FR_IGNORE))
			return (frp);
	return (NULL);
}

/*
 * file_unedited --
 *	Return if there are files that aren't ignored and are unedited.
 */
FREF *
file_unedited(sp)
	SCR *sp;
{
	FREF *frp;

	/* Return the next file name. */
	for (frp = sp->frefq.cqh_first;
	    frp != (FREF *)&sp->frefq; frp = frp->q.cqe_next)
		if (!F_ISSET(frp, FR_EDITED | FR_IGNORE))
			return (frp);
	return (NULL);
}

/*
 * file_init --
 *	Start editing a file, based on the FREF structure.  If successsful,
 *	let go of any previous file.  Don't release the previous file until
 *	absolutely sure we have the new one.
 */
int
file_init(sp, frp, rcv_name, force)
	SCR *sp;
	FREF *frp;
	char *rcv_name;
	int force;
{
	EXF *ep;
	RECNOINFO oinfo;
	struct stat sb;
	size_t psize;
	int fd;
	char *p, *oname, tname[MAXPATHLEN];

	/*
	 * Required ep initialization:
	 *	Flush the line caches.
	 *	Default recover mail file fd to -1.
	 *	Set initial EXF flag bits.
	 */
	CALLOC_RET(sp, ep, EXF *, 1, sizeof(EXF));
	ep->c_lno = ep->c_nlines = OOBLNO;
	ep->rcv_fd = -1;
	LIST_INIT(&ep->marks);
	F_SET(ep, F_FIRSTMODIFY);

	/*
	 * If no name or backing file, create a backing temporary file, saving
	 * the temp file name so can later unlink it.  Repoint the name to the
	 * temporary name (we display it to the user until they rename it).
	 * There are some games we play with the FR_FREE_TNAME and FR_NONAME
	 * flags (see ex/ex_file.c) to make sure that the temporary memory gets
	 * free'd up.
	 */
	if ((oname = FILENAME(frp)) == NULL || stat(oname, &sb)) {
		(void)snprintf(tname, sizeof(tname),
		    "%s/vi.XXXXXX", O_STR(sp, O_DIRECTORY));
		if ((fd = mkstemp(tname)) == -1) {
			msgq(sp, M_SYSERR, "Temporary file");
			goto err;
		}
		(void)close(fd);
		if ((frp->tname = strdup(tname)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			(void)unlink(tname);
			goto err;
		}
		oname = frp->tname;
		psize = 4 * 1024;
		F_SET(frp, FR_NEWFILE);
	} else {
		/* Try to keep it at 10 pages or less per file. */
		if (sb.st_size < 40 * 1024)
			psize = 4 * 1024;
		else if (sb.st_size < 320 * 1024)
			psize = 32 * 1024;
		else
			psize = 64 * 1024;

		frp->mtime = sb.st_mtime;

		if (!S_ISREG(sb.st_mode))
			msgq(sp, M_ERR,
			    "Warning: %s is not a regular file.", oname);
	}
	
	/* Set up recovery. */
	memset(&oinfo, 0, sizeof(RECNOINFO));
	oinfo.bval = '\n';			/* Always set. */
	oinfo.psize = psize;
	oinfo.flags = F_ISSET(sp->gp, G_SNAPSHOT) ? R_SNAPSHOT : 0;
	if (rcv_name == NULL) {
		if (rcv_tmp(sp, ep, FILENAME(frp)))
			msgq(sp, M_ERR,
		    "Modifications not recoverable if the system crashes.");
		else
			oinfo.bfname = ep->rcv_path;
	} else if ((ep->rcv_path = strdup(rcv_name)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		goto err;
	} else {
		oinfo.bfname = ep->rcv_path;
		F_SET(ep, F_MODIFIED | F_RCV_ON);
	}

	/* Open a db structure. */
	if ((ep->db = dbopen(rcv_name == NULL ? oname : NULL,
	    O_NONBLOCK | O_RDONLY, DEFFILEMODE, DB_RECNO, &oinfo)) == NULL) {
		msgq(sp, M_SYSERR, rcv_name == NULL ? oname : rcv_name);
		goto err;
	}

	/* Init file marks. */
	if (mark_init(sp, ep))
		goto err;

	/* Start logging. */
	if (log_init(sp, ep))
		goto err;

	/*
	 * The -R flag, or doing a "set readonly" during a session causes
	 * all files edited during the session (using an edit command, or
	 * even using tags) to be marked read-only.  Changing the file name
	 * (see ex/ex_file.c), clears this flag.
	 *
	 * Otherwise, try and figure out if a file is readonly.  This is a
	 * dangerous thing to do.  The kernel is the only arbiter of whether
	 * or not a file is writeable, and the best that a user program can
	 * do is guess.  Obvious loopholes are files that are on a file system
	 * mounted readonly (access catches this one on a few systems), or
	 * alternate protection mechanisms, ACL's for example, that we can't
	 * portably check.  Lots of fun, and only here because users whined.
	 *
	 * !!!
	 * Historic vi displayed the readonly message if none of the file
	 * write bits were set, or if an an access(2) call on the path
	 * failed.  This seems reasonable.  If the file is mode 444, root
	 * users may want to know that the owner of the file did not expect
	 * it to be written.
	 *
	 * Historic vi set the readonly bit if no write bits were set for
	 * a file, even if the access call would have succeeded.  This makes
	 * the superuser force the write even when vi expects that it will
	 * succeed.  I'm less supportive of this semantic, but it's historic
	 * practice and the conservative approach to vi'ing files as root.
	 *
	 * It would be nice if there was some way to update this when the user
	 * does a "^Z; chmod ...".  The problem is that we'd first have to
	 * distinguish between readonly bits set because of file permissions
	 * and those set for other reasons.  That's not too hard, but deciding
	 * when to reevaluate the permissions is trickier.  An alternative
	 * might be to turn off the readonly bit if the user forces a write
	 * and it succeeds.
	 *
	 * XXX
	 * Access(2) doesn't consider the effective uid/gid values.  This
	 * probably isn't a problem for vi when it's running standalone.
	 */
	if (O_ISSET(sp, O_READONLY) || !F_ISSET(frp, FR_NEWFILE) &&
	    (!(sb.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) ||
	    access(FILENAME(frp), W_OK)))
		F_SET(frp, FR_RDONLY);
	else
		F_CLR(frp, FR_RDONLY);

	/*
	 * Close the previous file; if that fails, close the new one
	 * and run for the border.
	 */
	if (sp->ep != NULL && file_end(sp, sp->ep, force)) {
		(void)file_end(sp, ep, 1);
		goto err;
	}

	/*
	 * 4.4BSD supports locking in the open call, other systems don't.
	 * Since the user can't interrupt us between the open and here,
	 * it's a don't care.
	 *
	 * !!!
	 * We need to distinguish a lock not being available for the file
	 * from the file system not supporting locking.  Assume that EAGAIN
	 * or EWOULDBLOCK is the former.  There isn't a portable way to do
	 * this.
	 *
	 * XXX
	 * The locking is flock(2) style, not fcntl(2).  The latter is known
	 * to fail badly on some systems, and its only advantage is that it
	 * occasionally works over NFS.
	 */
	if (flock(ep->db->fd(ep->db), LOCK_EX | LOCK_NB))
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			msgq(sp, M_INFO,
			    "%s already locked, session is read-only", oname);
			F_SET(frp, FR_RDONLY);
		} else
			msgq(sp, M_VINFO, "%s cannot be locked", oname);

	/*
	 * Set the previous file pointer and the alternate file name to be
	 * the file we're about to discard.
	 *
	 * !!!
	 * If the current file was a temporary file, the call to file_end()
	 * unlinked it and free'd the name.  So, there is no previous file,
	 * and there is no alternate file name.  This matches historical
	 * practice, although in historical vi it could only happen as the
	 * result of the initial command, i.e. if vi was execute without a
	 * file name.
	 */
	if (sp->frp != NULL) {
		p = FILENAME(sp->frp);
		if (p == NULL)
			sp->p_frp = NULL;
		else
			sp->p_frp = sp->frp;
		set_alt_name(sp, p);
	}

	/* The new file has now been officially edited. */
	F_SET(frp, FR_EDITED);

	/* Switch... */
	++ep->refcnt;
	sp->ep = ep;
	sp->frp = frp;
	return (0);

err:	if (frp->tname != NULL) {
		(void)unlink(frp->tname);
		free(frp->tname);
		frp->tname = NULL;
	}
	if (ep->rcv_path != NULL) {
		free(ep->rcv_path);
		ep->rcv_path = NULL;
	}
	FREE(ep, sizeof(EXF));
	return (1);
}

/*
 * file_end --
 *	Stop editing a file.
 */
int
file_end(sp, ep, force)
	SCR *sp;
	EXF *ep;
	int force;
{
	FREF *frp;

	/*
	 *
	 * sp->ep MAY NOT BE THE SAME AS THE ARGUMENT ep, SO DON'T USE IT!
	 *
	 * Save the cursor location.
	 *
	 * XXX
	 * It would be cleaner to do this somewhere else, but by the time
	 * ex or vi knows that we're changing files it's already happened.
	 */
	frp = sp->frp;
	frp->lno = sp->lno;
	frp->cno = sp->cno;
	F_SET(frp, FR_CURSORSET);

	/* If multiply referenced, just decrement the count and return. */
	if (--ep->refcnt != 0)
		return (0);

	/* Close the db structure. */
	if (ep->db->close != NULL && ep->db->close(ep->db) && !force) {
		msgq(sp, M_ERR,
		    "%s: close: %s", FILENAME(frp), strerror(errno));
		++ep->refcnt;
		return (1);
	}

	/* COMMITTED TO THE CLOSE.  THERE'S NO GOING BACK... */

	/* Stop logging. */
	(void)log_end(sp, ep);

	/* Free up any marks. */
	mark_end(sp, ep);

	/*
	 * Delete the recovery files, close the open descriptor,
	 * free recovery memory.
	 */
	if (!F_ISSET(ep, F_RCV_NORM)) {
		if (ep->rcv_path != NULL && unlink(ep->rcv_path))
			msgq(sp, M_ERR,
			    "%s: remove: %s", ep->rcv_path, strerror(errno));
		if (ep->rcv_mpath != NULL && unlink(ep->rcv_mpath))
			msgq(sp, M_ERR,
			    "%s: remove: %s", ep->rcv_mpath, strerror(errno));
	}
	if (ep->rcv_fd != -1)
		(void)close(ep->rcv_fd);
	if (ep->rcv_path != NULL)
		free(ep->rcv_path);
	if (ep->rcv_mpath != NULL)
		free(ep->rcv_mpath);

	/*
	 * Unlink any temporary file, file name.  We also turn on the
	 * ignore bit at this point, because it was a "created" file,
	 * not an argument file.
	 */
	if (frp->tname != NULL) {
		if (unlink(frp->tname))
			msgq(sp, M_ERR,
			    "%s: remove: %s", frp->tname, strerror(errno));
		free(frp->tname);
		frp->tname = NULL;

		if (frp->name == NULL && frp->cname == NULL)
			F_SET(frp, FR_IGNORE);
	}
	/* Free the EXF structure. */
	FREE(ep, sizeof(EXF));
	return (0);
}

/*
 * file_write --
 *	Write the file to disk.  Historic vi had fairly convoluted
 *	semantics for whether or not writes would happen.  That's
 *	why all the flags.
 */
int
file_write(sp, ep, fm, tm, name, flags)
	SCR *sp;
	EXF *ep;
	MARK *fm, *tm;
	char *name;
	int flags;
{
	struct stat sb;
	FILE *fp;
	FREF *frp;
	MARK from, to;
	u_long nlno, nch;
	int fd, oflags, rval;
	char *msg;

	/*
	 * Don't permit writing to temporary files.  The problem is that
	 * if it's a temp file, and the user does ":wq", we write and quit,
	 * unlinking the temporary file.  Not what the user had in mind
	 * at all.  This test cannot be forced.
	 */
	frp = sp->frp;
	if (name == NULL && frp->cname == NULL && frp->name == NULL) {
		msgq(sp, M_ERR, "No filename to which to write.");
		return (1);
	}

	/* Can't write files marked read-only, unless forced. */
	if (!LF_ISSET(FS_FORCE) &&
	    name == NULL && F_ISSET(frp, FR_RDONLY)) {
		if (LF_ISSET(FS_POSSIBLE))
			msgq(sp, M_ERR,
			    "Read-only file, not written; use ! to override.");
		else
			msgq(sp, M_ERR,
			    "Read-only file, not written.");
		return (1);
	}

	/* If not forced, not appending, and "writeany" not set ... */
	if (!LF_ISSET(FS_FORCE | FS_APPEND) && !O_ISSET(sp, O_WRITEANY)) {
		/* Don't overwrite anything but the original file. */
		if (name != NULL) {
			if (!stat(name, &sb))
				goto exists;
		} else if (frp->cname != NULL &&
		    !F_ISSET(frp, FR_CHANGEWRITE) && !stat(frp->cname, &sb)) {
			name = frp->cname;
exists:			if (LF_ISSET(FS_POSSIBLE))
				msgq(sp, M_ERR,
		"%s exists, not written; use ! to override.", name);
			else
				msgq(sp, M_ERR,
				    "%s exists, not written.", name);
			return (1);
		}

		/*
		 * Don't write part of any existing file.  Only test for the
		 * original file, the previous test catches anything else.
		 */
		if (!LF_ISSET(FS_ALL) && name == NULL &&
		    frp->cname == NULL && !stat(frp->name, &sb)) {
			if (LF_ISSET(FS_POSSIBLE))
				msgq(sp, M_ERR,
				    "Use ! to write a partial file.");
			else
				msgq(sp, M_ERR, "Partial file, not written.");
			return (1);
		}
	}

	/*
	 * Figure out if the file already exists -- if it doesn't, we display
	 * the "new file" message.  The stat might not be necessary, but we
	 * just repeat it because it's easier than hacking the previous tests.
	 * The information is only used for the user message and modification
	 * time test, so we can ignore the obvious race condition.
	 *
	 * If the user is overwriting a file other than the original file, and
	 * O_WRITEANY was what got us here (neither force nor append was set),
	 * display the "existing file" messsage.  Since the FR_CHANGEWRITE flag
	 * is set on a successful write, the message only appears once when the
	 * user changes a file name.  This is historic practice.
	 *
	 * One final test.  If we're not forcing or appending, and we have a
	 * saved modification time, stop the user if it's been written since
	 * we last edited or wrote it, and make them force it.
	 */
	if (stat(name == NULL ? FILENAME(frp) : name, &sb))
		msg = ": new file";
	else {
		msg = "";
		if (!LF_ISSET(FS_FORCE | FS_APPEND)) {
			if (frp->mtime && sb.st_mtime > frp->mtime) {
				msgq(sp, M_ERR,
			"%s: file modified more recently than this copy%s.",
				    name == NULL ? frp->name : name,
				    LF_ISSET(FS_POSSIBLE) ?
				    "; use ! to override" : "");
				return (1);
			}
			if (name != NULL ||
			    !F_ISSET(frp, FR_CHANGEWRITE) && frp->cname != NULL)
				msg = ": existing file";
		}
	}

	/* We no longer care where the name came from. */
	if (name == NULL)
		name = FILENAME(frp);

	/* Set flags to either append or truncate. */
	oflags = O_CREAT | O_WRONLY;
	if (LF_ISSET(FS_APPEND))
		oflags |= O_APPEND;
	else
		oflags |= O_TRUNC;

	/* Open the file. */
	if ((fd = open(name, oflags, DEFFILEMODE)) < 0) {
		msgq(sp, M_SYSERR, name);
		return (1);
	}

	/* Use stdio for buffering. */
	if ((fp = fdopen(fd, "w")) == NULL) {
		(void)close(fd);
		msgq(sp, M_SYSERR, name);
		return (1);
	}

	/* Build fake addresses, if necessary. */
	if (fm == NULL) {
		from.lno = 1;
		from.cno = 0;
		fm = &from;
		if (file_lline(sp, ep, &to.lno))
			return (1);
		to.cno = 0;
		tm = &to;
	}

	/* Write the file. */
	rval = ex_writefp(sp, ep, name, fp, fm, tm, &nlno, &nch);

	/*
	 * Save the new last modification time -- even if the write fails
	 * we re-init the time if we wrote anything.  That way the user can
	 * clean up the disk and rewrite without having to force it.
	 */
	if (nlno || nch)
		frp->mtime = stat(name, &sb) ? 0 : sb.st_mtime;
	
	/* If the write failed, complain loudly. */
	if (rval) {
		if (!LF_ISSET(FS_APPEND))
			msgq(sp, M_ERR, "%s: WARNING: file truncated!", name);
		return (1);
	}

	/*
	 * Once we've actually written the file, it doesn't matter that the
	 * file name was changed -- if it was, we've already whacked it.
	 */
	F_SET(frp, FR_CHANGEWRITE);

	/* If wrote the entire file, clear the modified bit. */
	if (LF_ISSET(FS_ALL))
		F_CLR(ep, F_MODIFIED);

	msgq(sp, M_INFO, "%s%s: %lu line%s, %lu characters.",
	    name, msg, nlno, nlno == 1 ? "" : "s", nch);

	return (0);
}
