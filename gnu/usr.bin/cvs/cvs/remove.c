/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
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
static const char rcsid[] = "$CVSid: @(#)remove.c 1.39 94/10/07 $";
USE(rcsid);
#endif

static int remove_fileproc PROTO((char *file, char *update_dir,
			    char *repository, List *entries,
			    List *srcfiles));
static Dtype remove_dirproc PROTO((char *dir, char *repos, char *update_dir));

static int force;
static int local;
static int removed_files;
static int existing_files;

static const char *const remove_usage[] =
{
    "Usage: %s %s [-flR] [files...]\n",
    "\t-f\tDelete the file before removing it.\n",
    "\t-l\tProcess this directory only (not recursive).\n",
    "\t-R\tProcess directories recursively.\n",
    NULL
};

int
cvsremove (argc, argv)
    int argc;
    char **argv;
{
    int c, err;

    if (argc == -1)
	usage (remove_usage);

    optind = 1;
    while ((c = getopt (argc, argv, "flR")) != -1)
    {
	switch (c)
	{
	    case 'f':
		force = 1;
		break;
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

    wrap_setup ();

#ifdef CLIENT_SUPPORT
    if (client_active) {
	start_server ();
	ign_setup ();
	if (local)
	    send_arg("-l");
	send_files (argc, argv, local, 0);
	if (fprintf (to_server, "remove\n") < 0)
	    error (1, errno, "writing to server");
        return get_responses_and_close ();
    }
#endif

    /* start the recursion processor */
    err = start_recursion (remove_fileproc, (int (*) ()) NULL, remove_dirproc,
			   (int (*) ()) NULL, argc, argv, local,
			   W_LOCAL, 0, 1, (char *) NULL, 1, 0);

    if (removed_files)
	error (0, 0, "use '%s commit' to remove %s permanently", program_name,
	       (removed_files == 1) ? "this file" : "these files");

    if (existing_files)
	error (0, 0,
	       ((existing_files == 1) ?
		"%d file exists; use `%s' to remove it first" :
		"%d files exist; use `%s' to remove them first"),
	       existing_files, RM);

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

    if (force)
    {
	if (!noexec)
	{
	    if (unlink (file) < 0 && ! existence_error (errno))
	    {
		if (update_dir[0] == '\0')
		    error (0, errno, "unable to remove %s", file);
		else
		    error (0, errno, "unable to remove %s/%s", update_dir,
			   file);
	    }
	}
	/* else FIXME should probably act as if the file doesn't exist
	   in doing the following checks.  */
    }

    vers = Version_TS (repository, (char *) NULL, (char *) NULL, (char *) NULL,
		       file, 0, 0, entries, srcfiles);

    if (vers->ts_user != NULL)
    {
	existing_files++;
	if (!quiet)
	    error (0, 0, "file `%s' still in working directory", file);
    }
    else if (vers->vn_user == NULL)
    {
	if (!quiet)
	    error (0, 0, "nothing known about `%s'", file);
    }
    else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
    {
	/*
	 * It's a file that has been added, but not commited yet. So,
	 * remove the ,t file for it and scratch it from the
	 * entries file.  */
	Scratch_Entry (entries, file);
	(void) sprintf (fname, "%s/%s%s", CVSADM, file, CVSEXT_LOG);
	(void) unlink_file (fname);
	if (!quiet)
	    error (0, 0, "removed `%s'", file);

#ifdef SERVER_SUPPORT
	if (server_active)
	    server_checked_in (file, update_dir, repository);
#endif
    }
    else if (vers->vn_user[0] == '-')
    {
	if (!quiet)
	    error (0, 0, "file `%s' already scheduled for removal", file);
    }
    else
    {
	/* Re-register it with a negative version number.  */
	(void) strcpy (fname, "-");
	(void) strcat (fname, vers->vn_user);
	Register (entries, file, fname, vers->ts_rcs, vers->options,
		  vers->tag, vers->date, vers->ts_conflict);
	if (!quiet)
	    error (0, 0, "scheduling `%s' for removal", file);
	removed_files++;

#ifdef SERVER_SUPPORT
	if (server_active)
	    server_checked_in (file, update_dir, repository);
#endif
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
