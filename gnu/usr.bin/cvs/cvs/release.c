/*
 * Release: "cancel" a checkout in the history log.
 * 
 * - Don't allow release if anything is active - Don't allow release if not
 * above or inside repository. - Don't allow release if ./CVS/Repository is
 * not the same as the directory specified in the module database.
 * 
 * - Enter a line in the history log indicating the "release". - If asked to,
 * delete the local working directory.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)release.c 1.21 92/02/29";
#endif

#if __STDC__
static void release_delete (char *dir);
#else
static void release_delete ();
#endif				/* __STDC__ */

static char *release_usage[] =
{
    "Usage: %s %s [-d] modules...\n",
    "\t-Q\tReally quiet.\n",
    "\t-d\tDelete the given directory.\n",
    "\t-q\tSomewhat quiet.\n",
    NULL
};

static short delete;

int
release (argc, argv)
    int argc;
    char **argv;
{
    FILE *fp;
    register int i, c;
    register char *cp;
    int margc;
    DBM *db;
    datum key, val;
    char *repository, *srepos;
    char **margv, *modargv[MAXFILEPERDIR], line[PATH_MAX];

    if (argc == -1)
	usage (release_usage);
    optind = 1;
    while ((c = gnu_getopt (argc, argv, "Qdq")) != -1)
    {
	switch (c)
	{
	    case 'Q':
		really_quiet = 1;
		/* FALL THROUGH */
	    case 'q':
		quiet = 1;
		break;
	    case 'd':
		delete++;
		break;
	    case '?':
	    default:
		usage (release_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (!(db = open_module ()))
	return (1);
    for (i = 0; i < argc; i++)
    {

	/*
	 * If we are in a repository, do it.  Else if we are in the parent of
	 * a directory with the same name as the module, "cd" into it and
	 * look for a repository there.
	 */
	if (isdir (argv[i]))
	{
	    if (chdir (argv[i]) < 0)
	    {
		if (!really_quiet)
		    error (0, 0, "can't chdir to: %s", argv[i]);
		continue;
	    }
	    if (!isdir (CVSADM) && !isdir (OCVSADM))
	    {
		if (!really_quiet)
		    error (0, 0, "no repository module: %s", argv[i]);
		continue;
	    }
	}
	else
	{
	    if (!really_quiet)
		error (0, 0, "no such directory/module: %s", argv[i]);
	    continue;
	}

	repository = Name_Repository ((char *) NULL, (char *) NULL);
	srepos = Short_Repository (repository);

	/* grab module entry from database and check against short repos */
	key.dptr = argv[i];
	key.dsize = strlen (key.dptr);
	val = dbm_fetch (db, key);
	if (!val.dptr)
	{
	    error (0, 0, "no such module name: %s", argv[i]);
	    continue;
	}
	val.dptr[val.dsize] = '\0';
	if ((cp = index (val.dptr, '#')) != NULL) /* Strip out a comment */
	{
	    do
	    {
		*cp-- = '\0';
	    } while (isspace (*cp));
	}
	(void) sprintf (line, "%s %s", key.dptr, val.dptr);
	line2argv (&margc, modargv, line);
	margv = modargv;

	optind = 1;
	while (gnu_getopt (margc, margv, CVSMODULE_OPTS) != -1)
	     /* do nothing */ ;
	margc -= optind;
	margv += optind;

	if (margc < 1)
	{
	    error (0, 0, "modules file missing directory for key %s value %s",
		   key.dptr, val.dptr);
	    continue;
	}
	if (strcmp (*margv, srepos))
	{
	    error (0, 0, "repository mismatch: module[%s], here[%s]",
		   *margv, srepos);
	    free (repository);
	    continue;
	}

	if (!really_quiet)
	{

	    /*
	     * Now see if there is any reason not to allow a "Release" This
	     * is "popen()" instead of "Popen()" since we don't want "-n" to
	     * stop it.
	     */
#ifdef FREEBSD_DEVELOPER
	    fp = popen ("ncvs -n -q update", "r");
#else
	    fp = popen ("cvs -n -q update", "r");
#endif /* FREEBSD_DEVELOPER */
	    c = 0;
	    while (fgets (line, sizeof (line), fp))
	    {
		if (index ("MARCZ", *line))
		    c++;
		(void) printf (line);
	    }
	    (void) pclose (fp);
	    (void) printf ("You have [%d] altered files in this repository.\n",
			   c);
	    (void) printf ("Are you sure you want to release %smodule `%s': ",
			   delete ? "(and delete) " : "", argv[i]);
	    c = !yesno ();
	    if (c)			/* "No" */
	    {
		(void) fprintf (stderr, "** `%s' aborted by user choice.\n",
				command_name);
		free (repository);
		continue;
	    }
	}

	/*
	 * So, we've passed all the tests, go ahead and release it. First,
	 * log the release, then attempt to delete it.
	 */
	history_write ('F', argv[i], "", argv[i], "");	/* F == Free */
	free (repository);

	if (delete)
	    release_delete (argv[i]);
    }
    close_module (db);
    return (0);
}

/* We want to "rm -r" the repository, but let us be a little paranoid. */
static void
release_delete (dir)
    char *dir;
{
    struct stat st;
    ino_t ino;
    int retcode = 0;

    (void) stat (".", &st);
    ino = st.st_ino;
    (void) chdir ("..");
    (void) stat (dir, &st);
    if (ino != st.st_ino)
    {
	error (0, 0,
	       "Parent dir on a different disk, delete of %s aborted", dir);
	return;
    }
    run_setup ("%s -r", RM);
    run_arg (dir);
    if ((retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL)) != 0)
	error (0, retcode == -1 ? errno : 0, 
	       "deletion of module %s failed.", dir);
}
