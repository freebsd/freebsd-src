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

int
RCS_settag(path, tag, rev)
    const char *path;
    const char *tag;
    const char *rev;
{
    run_setup ("%s%s -q -N%s:%s", Rcsbin, RCS, tag, rev);
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
    run_setup ("%s%s -q -N%s", Rcsbin, RCS, tag);
    run_arg (path);
    return run_exec (RUN_TTY, RUN_TTY, noerr ? DEVNULL : RUN_TTY, RUN_NORMAL);
}

/* set RCS branch to REV */
int
RCS_setbranch(path, rev)
    const char *path;
    const char *rev;
{
    run_setup ("%s%s -q -b%s", Rcsbin, RCS, rev ? rev : "");
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
    run_setup ("%s%s -q -l%s", Rcsbin, RCS, rev ? rev : "");
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
    run_setup ("%s%s -q -u%s", Rcsbin, RCS, rev ? rev : "");
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

    run_setup ("%s%s %s -r%s -r%s %s", Rcsbin, RCS_RCSMERGE,
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
