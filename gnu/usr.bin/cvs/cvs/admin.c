/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Administration
 * 
 * For now, this is basically a front end for rcs.  All options are passed
 * directly on.
 */

#include "cvs.h"
#ifdef CVS_ADMIN_GROUP
#include <grp.h>
#endif

#ifndef lint
static const char rcsid[] = "$CVSid: @(#)admin.c 1.20 94/09/30 $";
USE(rcsid);
#endif

static Dtype admin_dirproc PROTO((char *dir, char *repos, char *update_dir));
static int admin_fileproc PROTO((char *file, char *update_dir,
			   char *repository, List *entries,
			   List *srcfiles));

static const char *const admin_usage[] =
{
    "Usage: %s %s rcs-options files...\n",
    NULL
};

static int ac;
static char **av;

int
admin (argc, argv)
    int argc;
    char **argv;
{
    int err;
#ifdef CVS_ADMIN_GROUP
    struct group *grp;
#endif
    if (argc <= 1)
	usage (admin_usage);

#ifdef CVS_ADMIN_GROUP
    grp = getgrnam(CVS_ADMIN_GROUP);
     /* skip usage right check if group CVS_ADMIN_GROUP does not exist */
    if (grp != NULL)
    {
	char *me = getcaller();
	char **grnam = grp->gr_mem;
	int denied = 1;
	
	while (*grnam)
	{
	    if (strcmp(*grnam, me) == 0) 
	    {
		denied = 0;
		break;
	    }
	    grnam++;
	}

	if (denied)
	    error (1, 0, "usage is restricted to members of the group %s",
		   CVS_ADMIN_GROUP);
    }
#endif

    wrap_setup ();

    /* skip all optional arguments to see if we have any file names */
    for (ac = 1; ac < argc; ac++)
	if (argv[ac][0] != '-')
	    break;
    argc -= ac;
    av = argv + 1;
    argv += ac;
    ac--;
    if (ac == 0 || argc == 0)
	usage (admin_usage);

#ifdef CLIENT_SUPPORT
    if (client_active)
    {
	int i;

	/* We're the client side.  Fire up the remote server.  */
	start_server ();
	
	ign_setup ();

	for (i = 0; i <= ac; ++i)	/* XXX send -ko too with i = 0 */
	    send_arg (av[i]);

#if 0
	/* FIXME:  We shouldn't have to send current files, but I'm not sure
	   whether it works.  So send the files --
	   it's slower but it works.  */
	send_file_names (argc, argv);
#else
	send_files (argc, argv, 0, 0);
#endif
	if (fprintf (to_server, "admin\n") < 0)
	    error (1, errno, "writing to server");
        return get_responses_and_close ();
    }
#endif /* CLIENT_SUPPORT */

    /* start the recursion processor */
    err = start_recursion (admin_fileproc, (FILESDONEPROC) NULL, admin_dirproc,
			   (DIRLEAVEPROC) NULL, argc, argv, 0,
			   W_LOCAL, 0, 1, (char *) NULL, 1, 0);
    return (err);
}

/*
 * Called to run "rcs" on a particular file.
 */
/* ARGSUSED */
static int
admin_fileproc (file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List *entries;
    List *srcfiles;
{
    Vers_TS *vers;
    char *version;
    char **argv;
    int argc;
    int retcode = 0;

    vers = Version_TS (repository, (char *) NULL, (char *) NULL, (char *) NULL,
		       file, 0, 0, entries, srcfiles);

    version = vers->vn_user;
    if (version == NULL)
	return (0);
    else if (strcmp (version, "0") == 0)
    {
	error (0, 0, "cannot admin newly added file `%s'", file);
	return (0);
    }

    run_setup ("%s%s", Rcsbin, RCS);
    for (argc = ac, argv = av; argc; argc--, argv++)
	run_arg (*argv);
    run_arg (vers->srcfile->path);
    if ((retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL)) != 0)
    {
	if (!quiet)
	    error (0, retcode == -1 ? errno : 0,
		   "%s failed for `%s'", RCS, file);
	return (1);
    }
    return (0);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
admin_dirproc (dir, repos, update_dir)
    char *dir;
    char *repos;
    char *update_dir;
{
    if (!quiet)
	error (0, 0, "Administrating %s", update_dir);
    return (R_PROCESS);
}
