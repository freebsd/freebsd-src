/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Print Log Information
 * 
 * This line exists solely to test some pcl-cvs/ChangeLog stuff.  You
 * can delete it, if indeed it's still here when you read it.  -Karl
 *
 * Prints the RCS "log" (rlog) information for the specified files.  With no
 * argument, prints the log information for all the files in the directory
 * (recursive by default).
 */

#include "cvs.h"

#ifndef lint
static const char rcsid[] = "$CVSid: @(#)log.c 1.44 94/09/30 $";
USE(rcsid);
#endif

static Dtype log_dirproc PROTO((char *dir, char *repository, char *update_dir));
static int log_fileproc PROTO((char *file, char *update_dir, char *repository,
			 List * entries, List * srcfiles));

static const char *const log_usage[] =
{
    "Usage: %s %s [-l] [rlog-options] [files...]\n",
    "\t-l\tLocal directory only, no recursion.\n",
    NULL
};

static int ac;
static char **av;

int
cvslog (argc, argv)
    int argc;
    char **argv;
{
    int i;
    int err = 0;
    int local = 0;

    if (argc == -1)
	usage (log_usage);

    /*
     * All 'log' command options except -l are passed directly on to 'rlog'
     */
    for (i = 1; i < argc && argv[i][0] == '-'; i++)
      if (argv[i][1] == 'l')
	local = 1;

    wrap_setup ();

#ifdef CLIENT_SUPPORT
    if (client_active) {
	/* We're the local client.  Fire up the remote server.  */
	start_server ();
	
	ign_setup ();

	for (i = 1; i < argc && argv[i][0] == '-'; i++)
	  send_arg (argv[i]);

#if 0
/* FIXME:  We shouldn't have to send current files to get log entries, but it
   doesn't work yet and I haven't debugged it.  So send the files --
   it's slower but it works.  gnu@cygnus.com  Apr94  */
	send_file_names (argc - i, argv + i);
#else
	send_files (argc - i, argv + i, local, 0);
#endif

	if (fprintf (to_server, "log\n") < 0)
	    error (1, errno, "writing to server");
        err = get_responses_and_close ();
	return err;
    }

    ac = argc;
    av = argv;
#endif

    err = start_recursion (log_fileproc, (FILESDONEPROC) NULL, log_dirproc,
			   (DIRLEAVEPROC) NULL, argc - i, argv + i, local,
			   W_LOCAL | W_REPOS | W_ATTIC, 0, 1,
			   (char *) NULL, 1, 0);
    return (err);
}


/*
 * Do an rlog on a file
 */
/* ARGSUSED */
static int
log_fileproc (file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List *entries;
    List *srcfiles;
{
    Node *p;
    RCSNode *rcsfile;
    int retcode = 0;

    p = findnode (srcfiles, file);
    if (p == NULL || (rcsfile = (RCSNode *) p->data) == NULL)
    {
	/* no rcs file.  What *do* we know about this file? */
	p = findnode (entries, file);
	if (p != NULL)
	{
	    Entnode *e;
	    
	    e = (Entnode *) p->data;
	    if (e->version[0] == '0' || e->version[1] == '\0')
	    {
		if (!really_quiet)
		    error (0, 0, "%s has been added, but not committed",
			   file);
		return(0);
	    }
	}
	
	if (!really_quiet)
	    error (0, 0, "nothing known about %s", file);
	
	return (1);
    }

    run_setup ("%s%s", Rcsbin, RCS_RLOG);
    {
      int i;
      for (i = 1; i < ac && av[i][0] == '-'; i++)
	  if (av[i][1] != 'l')
	      run_arg (av[i]);
    }
    run_arg (rcsfile->path);

    if (*update_dir)
    {
      char *workfile = xmalloc (strlen (update_dir) + strlen (file) + 2);
      sprintf (workfile, "%s/%s", update_dir, file);
      run_arg (workfile);
      free (workfile);
    }

    if ((retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_REALLY)) == -1)
    {
	error (1, errno, "fork failed for rlog on %s", file);
    }
    return (retcode);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
log_dirproc (dir, repository, update_dir)
    char *dir;
    char *repository;
    char *update_dir;
{
    if (!isdir (dir))
	return (R_SKIP_ALL);

    if (!quiet)
	error (0, 0, "Logging %s", update_dir);
    return (R_PROCESS);
}
