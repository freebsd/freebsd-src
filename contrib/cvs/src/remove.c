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

static int remove_fileproc PROTO((struct file_info *finfo));
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
	send_file_names (argc, argv, SEND_EXPAND_WILD);
	send_files (argc, argv, local, 0);
	send_to_server ("remove\012", 0);
        return get_responses_and_close ();
    }
#endif

    /* start the recursion processor */
    err = start_recursion (remove_fileproc, (FILESDONEPROC) NULL,
                           remove_dirproc, (DIRLEAVEPROC) NULL, argc, argv,
                           local, W_LOCAL, 0, 1, (char *) NULL, 1, 0);

    if (removed_files)
	error (0, 0, "use '%s commit' to remove %s permanently", program_name,
	       (removed_files == 1) ? "this file" : "these files");

    if (existing_files)
	error (0, 0,
	       ((existing_files == 1) ?
		"%d file exists; remove it first" :
		"%d files exist; remove them first"),
	       existing_files);

    return (err);
}

/*
 * remove the file, only if it has already been physically removed
 */
/* ARGSUSED */
static int
remove_fileproc (finfo)
    struct file_info *finfo;
{
    char fname[PATH_MAX];
    Vers_TS *vers;

    if (force)
    {
	if (!noexec)
	{
	    if (unlink (finfo->file) < 0 && ! existence_error (errno))
	    {
		error (0, errno, "unable to remove %s", finfo->fullname);
	    }
	}
	/* else FIXME should probably act as if the file doesn't exist
	   in doing the following checks.  */
    }

    vers = Version_TS (finfo->repository, (char *) NULL, (char *) NULL, (char *) NULL,
		       finfo->file, 0, 0, finfo->entries, finfo->rcs);

    if (vers->ts_user != NULL)
    {
	existing_files++;
	if (!quiet)
	    error (0, 0, "file `%s' still in working directory",
		   finfo->fullname);
    }
    else if (vers->vn_user == NULL)
    {
	if (!quiet)
	    error (0, 0, "nothing known about `%s'", finfo->fullname);
    }
    else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
    {
	/*
	 * It's a file that has been added, but not commited yet. So,
	 * remove the ,t file for it and scratch it from the
	 * entries file.  */
	Scratch_Entry (finfo->entries, finfo->file);
	(void) sprintf (fname, "%s/%s%s", CVSADM, finfo->file, CVSEXT_LOG);
	(void) unlink_file (fname);
	if (!quiet)
	    error (0, 0, "removed `%s'", finfo->fullname);

#ifdef SERVER_SUPPORT
	if (server_active)
	    server_checked_in (finfo->file, finfo->update_dir, finfo->repository);
#endif
    }
    else if (vers->vn_user[0] == '-')
    {
	if (!quiet)
	    error (0, 0, "file `%s' already scheduled for removal",
		   finfo->fullname);
    }
    else
    {
	/* Re-register it with a negative version number.  */
	(void) strcpy (fname, "-");
	(void) strcat (fname, vers->vn_user);
	Register (finfo->entries, finfo->file, fname, vers->ts_rcs, vers->options,
		  vers->tag, vers->date, vers->ts_conflict);
	if (!quiet)
	    error (0, 0, "scheduling `%s' for removal", finfo->fullname);
	removed_files++;

#ifdef SERVER_SUPPORT
	if (server_active)
	    server_checked_in (finfo->file, finfo->update_dir, finfo->repository);
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
