/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * The functions in this file provide an interface for performing 
 * operations directly on RCS files. 
 */

#include "cvs.h"
#include <assert.h>

/* For RCS file PATH, make symbolic tag TAG point to revision REV.
   This validates that TAG is OK for a user to use.  Return value is
   -1 for error (and errno is set to indicate the error), positive for
   error (and an error message has been printed), or zero for success.  */

int
RCS_settag(path, tag, rev)
    const char *path;
    const char *tag;
    const char *rev;
{
    if (strcmp (tag, TAG_BASE) == 0
	|| strcmp (tag, TAG_HEAD) == 0)
    {
	/* Print the name of the tag might be considered redundant
	   with the caller, which also prints it.  Perhaps this helps
	   clarify why the tag name is considered reserved, I don't
	   know.  */
	error (0, 0, "Attempt to add reserved tag name %s", tag);
	return 1;
    }

    run_setup ("%s%s -x,v/ -q -N%s:%s", Rcsbin, RCS, tag, rev);
    run_arg (path);
    return run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
}

/* NOERR is 1 to suppress errors--FIXME it would
   be better to avoid the errors or some cleaner solution.  */
int
RCS_deltag(path, tag, noerr)
    const char *path;
    const char *tag;
    int noerr;
{
    run_setup ("%s%s -x,v/ -q -N%s", Rcsbin, RCS, tag);
    run_arg (path);
    return run_exec (RUN_TTY, RUN_TTY, noerr ? DEVNULL : RUN_TTY, RUN_NORMAL);
}

/* set RCS branch to REV */
int
RCS_setbranch(path, rev)
    const char *path;
    const char *rev;
{
    run_setup ("%s%s -x,v/ -q -b%s", Rcsbin, RCS, rev ? rev : "");
    run_arg (path);
    return run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
}

/* Lock revision REV.  NOERR is 1 to suppress errors--FIXME it would
   be better to avoid the errors or some cleaner solution.  */
int
RCS_lock(path, rev, noerr)
    const char *path;
    const char *rev;
    int noerr;
{
    run_setup ("%s%s -x,v/ -q -l%s", Rcsbin, RCS, rev ? rev : "");
    run_arg (path);
    return run_exec (RUN_TTY, RUN_TTY, noerr ? DEVNULL : RUN_TTY, RUN_NORMAL);
}

/* Unlock revision REV.  NOERR is 1 to suppress errors--FIXME it would
   be better to avoid the errors or some cleaner solution.  */
int
RCS_unlock(path, rev, noerr)
    const char *path;
    const char *rev;
    int noerr;
{
    run_setup ("%s%s -x,v/ -q -u%s", Rcsbin, RCS, rev ? rev : "");
    run_arg (path);
    return run_exec (RUN_TTY, RUN_TTY, noerr ? DEVNULL : RUN_TTY, RUN_NORMAL);
}

/* Merge revisions REV1 and REV2. */
int
RCS_merge(path, options, rev1, rev2)
     const char *path;
     const char *options;
     const char *rev1;
     const char *rev2;
{
    int status;

    /* XXX - Do merge by hand instead of using rcsmerge, due to -k handling */

    run_setup ("%s%s -x,v/ %s -r%s -r%s %s", Rcsbin, RCS_RCSMERGE,
	       options, rev1, rev2, path);
    status = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
#ifndef HAVE_RCS5
    if (status == 0) 
    {
	/* Run GREP to see if there appear to be conflicts in the file */
	run_setup ("%s", GREP);
	run_arg (RCS_MERGE_PAT);
	run_arg (path);
	status = (run_exec (RUN_TTY, DEVNULL, RUN_TTY, RUN_NORMAL) == 0);

    }
#endif
    return status;
}

/* Check out a revision from RCSFILE into WORKFILE, or to standard output
   if WORKFILE is NULL.  If WORKFILE is "", let RCS pick the working file
   name.  TAG is the tag to check out, or NULL if one should check out
   the head of the default branch.  OPTIONS is a string such as
   -kb or -kkv, for keyword expansion options, or NULL if there are none.
   If WORKFILE is NULL, run regardless of noexec; if non-NULL, noexec
   inhibits execution.  SOUT is what to do with standard output
   (typically RUN_TTY).  If FLAGS & RCS_FLAGS_LOCK, lock it.  If
   FLAGS & RCS_FLAGS_FORCE, check out even on top of an existing file.
   If NOERR is nonzero, suppress errors.  */
int
RCS_checkout (rcsfile, workfile, tag, options, sout, flags, noerr)
    char *rcsfile;
    char *workfile;
    char *tag;
    char *options;
    char *sout;
    int flags;
    int noerr;
{
    run_setup ("%s%s -x,v/ -q %s%s", Rcsbin, RCS_CO,
               tag ? "-r" : "", tag ? tag : "");
    if (options != NULL && options[0] != '\0')
	run_arg (options);
    if (workfile == NULL)
	run_arg ("-p");
    if (flags & RCS_FLAGS_LOCK)
	run_arg ("-l");
    if (flags & RCS_FLAGS_FORCE)
	run_arg ("-f");
    run_arg (rcsfile);
    if (workfile != NULL && workfile[0] != '\0')
	run_arg (workfile);
    return run_exec (RUN_TTY, sout, noerr ? DEVNULL : RUN_TTY,
                     workfile == NULL ? (RUN_NORMAL | RUN_REALLY) : RUN_NORMAL);
}

/* Check in to RCSFILE with revision REV (which must be greater than the
   largest revision) and message MESSAGE (which is checked for legality).
   If FLAGS & RCS_FLAGS_DEAD, check in a dead revision.  If NOERR, do not
   report errors.  If FLAGS & RCS_FLAGS_QUIET suppress errors somewhat more
   selectively.  If FLAGS & RCS_FLAGS_MODTIME, use the working file's
   modification time for the checkin time.  WORKFILE is the working file
   to check in from, or NULL to use the usual RCS rules for deriving it
   from the RCSFILE.  */
int
RCS_checkin (rcsfile, workfile, message, rev, flags, noerr)
    char *rcsfile;
    char *workfile;
    char *message;
    char *rev;
    int flags;
    int noerr;
{
    run_setup ("%s%s -x,v/ -f %s%s", Rcsbin, RCS_CI,
	       rev ? "-r" : "", rev ? rev : "");
    if (flags & RCS_FLAGS_DEAD)
	run_arg ("-sdead");
    if (flags & RCS_FLAGS_QUIET)
	run_arg ("-q");
    if (flags & RCS_FLAGS_MODTIME)
	run_arg ("-d");
    run_args ("-m%s", make_message_rcslegal (message));
    if (workfile != NULL)
	run_arg (workfile);
    run_arg (rcsfile);
    return run_exec (RUN_TTY, RUN_TTY, noerr ? DEVNULL : RUN_TTY, RUN_NORMAL);
}
