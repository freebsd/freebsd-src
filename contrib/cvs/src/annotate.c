/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Show last revision where each line modified
 * 
 * Prints the specified files with each line annotated with the revision
 * number where it was last modified.  With no argument, annotates all
 * all the files in the directory (recursive by default).
 */

#include "cvs.h"

/* Options from the command line.  */

static int force_tag_match = 1;
static int force_binary = 0;
static char *tag = NULL;
static int tag_validated;
static char *date = NULL;

static int is_rannotate;

static int annotate_fileproc PROTO ((void *callerdat, struct file_info *));
static int rannotate_proc PROTO((int argc, char **argv, char *xwhere,
				 char *mwhere, char *mfile, int shorten,
				 int local, char *mname, char *msg));

static const char *const annotate_usage[] =
{
    "Usage: %s %s [-lRfF] [-r rev] [-D date] [files...]\n",
    "\t-l\tLocal directory only, no recursion.\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-f\tUse head revision if tag/date not found.\n",
    "\t-F\tAnnotate binary files.\n",
    "\t-r rev\tAnnotate file as of specified revision/tag.\n",
    "\t-D date\tAnnotate file as of specified date.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

/* Command to show the revision, date, and author where each line of a
   file was modified.  */

int
annotate (argc, argv)
    int argc;
    char **argv;
{
    int local = 0;
    int err = 0;
    int c;

    is_rannotate = (strcmp(command_name, "rannotate") == 0);

    if (argc == -1)
	usage (annotate_usage);

    optind = 0;
    while ((c = getopt (argc, argv, "+lr:D:fFR")) != -1)
    {
	switch (c)
	{
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 'r':
	        tag = optarg;
		break;
	    case 'D':
	        date = Make_Date (optarg);
		break;
	    case 'f':
	        force_tag_match = 0;
		break;
	    case 'F':
	        force_binary = 1;
		break;
	    case '?':
	    default:
		usage (annotate_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

#ifdef CLIENT_SUPPORT
    if (current_parsed_root->isremote)
    {
	start_server ();

	if (is_rannotate && !supported_request ("rannotate"))
	    error (1, 0, "server does not support rannotate");

	ign_setup ();

	if (local)
	    send_arg ("-l");
	if (!force_tag_match)
	    send_arg ("-f");
	if (force_binary)
	    send_arg ("-F");
	option_with_arg ("-r", tag);
	if (date)
	    client_senddate (date);
	if (is_rannotate)
	{
	    int i;
	    for (i = 0; i < argc; i++)
		send_arg (argv[i]);
	    send_to_server ("rannotate\012", 0);
	}
	else
	{
	    send_files (argc, argv, local, 0, SEND_NO_CONTENTS);
	    send_file_names (argc, argv, SEND_EXPAND_WILD);
	    send_to_server ("annotate\012", 0);
	}
	return get_responses_and_close ();
    }
#endif /* CLIENT_SUPPORT */

    if (is_rannotate)
    {
	DBM *db;
	int i;
	db = open_module ();
	for (i = 0; i < argc; i++)
	{
	    err += do_module (db, argv[i], MISC, "Annotating", rannotate_proc,
			     (char *) NULL, 0, local, 0, 0, (char *) NULL);
	}
	close_module (db);
    }
    else
    {
	err = rannotate_proc (argc + 1, argv - 1, (char *) NULL,
			 (char *) NULL, (char *) NULL, 0, local, (char *) NULL,
			 (char *) NULL);
    }

    return err;
}
    

static int
rannotate_proc (argc, argv, xwhere, mwhere, mfile, shorten, local, mname, msg)
    int argc;
    char **argv;
    char *xwhere;
    char *mwhere;
    char *mfile;
    int shorten;
    int local;
    char *mname;
    char *msg;
{
    /* Begin section which is identical to patch_proc--should this
       be abstracted out somehow?  */
    char *myargv[2];
    int err = 0;
    int which;
    char *repository;
    char *where;

    if (is_rannotate)
    {
	repository = xmalloc (strlen (current_parsed_root->directory) + strlen (argv[0])
			      + (mfile == NULL ? 0 : strlen (mfile) + 1) + 2);
	(void) sprintf (repository, "%s/%s", current_parsed_root->directory, argv[0]);
	where = xmalloc (strlen (argv[0]) + (mfile == NULL ? 0 : strlen (mfile) + 1)
			 + 1);
	(void) strcpy (where, argv[0]);

	/* if mfile isn't null, we need to set up to do only part of the module */
	if (mfile != NULL)
	{
	    char *cp;
	    char *path;

	    /* if the portion of the module is a path, put the dir part on repos */
	    if ((cp = strrchr (mfile, '/')) != NULL)
	    {
		*cp = '\0';
		(void) strcat (repository, "/");
		(void) strcat (repository, mfile);
		(void) strcat (where, "/");
		(void) strcat (where, mfile);
		mfile = cp + 1;
	    }

	    /* take care of the rest */
	    path = xmalloc (strlen (repository) + strlen (mfile) + 5);
	    (void) sprintf (path, "%s/%s", repository, mfile);
	    if (isdir (path))
	    {
		/* directory means repository gets the dir tacked on */
		(void) strcpy (repository, path);
		(void) strcat (where, "/");
		(void) strcat (where, mfile);
	    }
	    else
	    {
		myargv[0] = argv[0];
		myargv[1] = mfile;
		argc = 2;
		argv = myargv;
	    }
	    free (path);
	}

	/* cd to the starting repository */
	if ( CVS_CHDIR (repository) < 0)
	{
	    error (0, errno, "cannot chdir to %s", repository);
	    free (repository);
	    return (1);
	}
	free (repository);
	/* End section which is identical to patch_proc.  */

	if (force_tag_match && tag != NULL)
	    which = W_REPOS | W_ATTIC;
	else
	    which = W_REPOS;
	repository = NULL;
    }
    else
    {
        where = NULL;
        which = W_LOCAL;
        repository = "";
    }

    if (tag != NULL && !tag_validated)
    {
	tag_check_valid (tag, argc - 1, argv + 1, local, 0, repository);
	tag_validated = 1;
    }

    err = start_recursion (annotate_fileproc, (FILESDONEPROC) NULL,
			   (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			   argc - 1, argv + 1, local, which, 0, 1,
			   where, 1);
    return err;
}


static int
annotate_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    char *expand, *version;

    if (finfo->rcs == NULL)
        return (1);

    if (finfo->rcs->flags & PARTIAL)
        RCS_reparsercsfile (finfo->rcs, (FILE **) NULL, (struct rcsbuffer *) NULL);

    expand = RCS_getexpand (finfo->rcs);
    version = RCS_getversion (finfo->rcs, tag, date, force_tag_match,
			      (int *) NULL);

    if (version == NULL)
        return 0;

    /* Distinguish output for various files if we are processing
       several files.  */
    cvs_outerr ("\nAnnotations for ", 0);
    cvs_outerr (finfo->fullname, 0);
    cvs_outerr ("\n***************\n", 0);

    if (!force_binary && expand && expand[0] == 'b')
    {
        cvs_outerr ("Skipping binary file -- -F not specified.\n", 0);
    }
    else
    {
	RCS_deltas (finfo->rcs, (FILE *) NULL, (struct rcsbuffer *) NULL,
		    version, RCS_ANNOTATE, NULL, NULL, NULL, NULL);
    }
    free (version);
    return 0;
}
