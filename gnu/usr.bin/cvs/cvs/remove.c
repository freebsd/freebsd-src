/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * Remove a File
 * 
 * Removes entries from the present version. The entries will be removed from
 * the RCS repository upon the next "commit".
 * 
 * "remove" accepts no options, only file names that are to be removed.  The
 * file must not exist in the current directory for "remove" to work
 * correctly.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)remove.c 1.34 92/04/10";
#endif

#if __STDC__
static int remove_fileproc (char *file, char *update_dir,
			    char *repository, List *entries,
			    List *srcfiles);
static Dtype remove_dirproc (char *dir, char *repos, char *update_dir);
#else
static Dtype remove_dirproc ();
static int remove_fileproc ();
#endif

static int local;
static int removed_files;
static int auto_removed_files;

static char *remove_usage[] =
{
    "Usage: %s %s [-lR] [files...]\n",
    "\t-l\tProcess this directory only (not recursive).\n",
    "\t-R\tProcess directories recursively.\n",
    NULL
};

int
cvsremove (argc, argv)
    int argc;
    char *argv[];
{
    int c, err;

    if (argc == -1)
	usage (remove_usage);

    optind = 1;
    while ((c = gnu_getopt (argc, argv, "lR")) != -1)
    {
	switch (c)
	{
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case '?':
	    default:
		usage (remove_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    /* start the recursion processor */
    err = start_recursion (remove_fileproc, (int (*) ()) NULL, remove_dirproc,
			   (int (*) ()) NULL, argc, argv, local,
			   W_LOCAL, 0, 1, (char *) NULL, 1);

    if (removed_files)
	error (0, 0, "use '%s commit' to remove %s permanently", program_name,
	       (removed_files == 1) ? "this file" : "these files");
    else
	if (!auto_removed_files)
	    error (0, 0, "no files removed; use `%s' to remove the file first",
		   RM);

    return (err);
}

/*
 * remove the file, only if it has already been physically removed
 */
/* ARGSUSED */
static int
remove_fileproc (file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List *entries;
    List *srcfiles;
{
    char fname[PATH_MAX];
    Vers_TS *vers;

    vers = Version_TS (repository, (char *) NULL, (char *) NULL, (char *) NULL,
		       file, 0, 0, entries, srcfiles);

    if (vers->ts_user != NULL)
    {
	freevers_ts (&vers);
	return (0);
    }

    if (vers->vn_user == NULL)
    {
	if (!quiet)
	    error (0, 0, "nothing known about %s", file);
	freevers_ts (&vers);
	return (0);
    }

    if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
    {
	/*
	 * It's a file that has been added, but not commited yet. So,
	 * remove the ,p and ,t file for it and scratch it from the
	 * entries file.
	 */
	Scratch_Entry (entries, file);
	(void) sprintf (fname, "%s/%s%s", CVSADM, file, CVSEXT_OPT);
	(void) unlink_file (fname);
	(void) sprintf (fname, "%s/%s%s", CVSADM, file, CVSEXT_LOG);
	(void) unlink_file (fname);
	if (!quiet)
	    error (0, 0, "removed `%s'.", file);
	auto_removed_files++;
    }
    else if (vers->vn_user[0] == '-')
    {
	freevers_ts (&vers);
	return (0);
    }
    else
    {
	/* Re-register it with a negative version number.  */
	(void) strcpy (fname, "-");
	(void) strcat (fname, vers->vn_user);
	Register (entries, file, fname, vers->ts_rcs, vers->options,
		  vers->tag, vers->date);
	if (!quiet)
	{
	    error (0, 0, "scheduling %s for removal", file);
	    removed_files++;
	}
    }

    freevers_ts (&vers);
    return (0);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
remove_dirproc (dir, repos, update_dir)
    char *dir;
    char *repos;
    char *update_dir;
{
    if (!quiet)
	error (0, 0, "Removing %s", update_dir);
    return (R_PROCESS);
}
