/*
 *    Copyright (c) 1992, Brian Berliner and Jeff Polk
 *    Copyright (c) 1989-1992, Brian Berliner
 *
 *    You may distribute under the terms of the GNU General Public License
 *    as specified in the README file that comes with the CVS 1.3 kit.
 *
 * This is the main C driver for the CVS system.
 *
 * Credit to Dick Grune, Vrije Universiteit, Amsterdam, for writing
 * the shell-script CVS system that this is based on.
 *
 * Usage:
 *	cvs [options] command [options] [files/modules...]
 *
 * Where "command" is composed of:
 *		admin		RCS command
 *		checkout	Check out a module/dir/file
 *		export		Like checkout, but used for exporting sources
 *		update		Brings work tree in sync with repository
 *		commit		Checks files into the repository
 *		diff		Runs diffs between revisions
 *		log		Prints "rlog" information for files
 *		add		Adds an entry to the repository
 *		remove		Removes an entry from the repository
 *		status		Status info on the revisions
 *		rdiff		"patch" format diff listing between releases
 *		tag		Add/delete a symbolic tag to the RCS file
 *		rtag		Add/delete a symbolic tag to the RCS file
 *		import		Import sources into CVS, using vendor branches
 *		release		Indicate that Module is no longer in use.
 *		history		Display history of Users and Modules.
 */

#include "cvs.h"
#include "patchlevel.h"

char rcsid[] = "@(#)main.c 1.64 92/03/31\n";

extern char *getenv ();

char *program_name;
char *command_name = "";

int use_editor = TRUE;
int cvswrite = !CVSREAD_DFLT;
int really_quiet = FALSE;
int quiet = FALSE;
int trace = FALSE;
int noexec = FALSE;
int logoff = FALSE;

char *CurDir;

/*
 * Defaults, for the environment variables that are not set
 */
char *Rcsbin = RCSBIN_DFLT;
char *Editor = EDITOR_DFLT;
char *CVSroot = CVSROOT_DFLT;

#if __STDC__
int add (int argc, char **argv);
int admin (int argc, char **argv);
int checkout (int argc, char **argv);
int commit (int argc, char **argv);
int diff (int argc, char **argv);
int history (int argc, char **argv);
int import (int argc, char **argv);
int cvslog (int argc, char **argv);
int patch (int argc, char **argv);
int release (int argc, char **argv);
int cvsremove (int argc, char **argv);
int rtag (int argc, char **argv);
int status (int argc, char **argv);
int tag (int argc, char **argv);
int update (int argc, char **argv);
#else
int add ();
int admin ();
int checkout ();
int commit ();
int diff ();
int history ();
int import ();
int cvslog ();
int patch ();
int release ();
int cvsremove ();
int rtag ();
int status ();
int tag ();
int update ();
#endif				/* __STDC__ */

#ifdef FREEBSD_DEVELOPER
int freebsd = TRUE;		/* Use the FreeBSD -K flags!! */
#endif

struct cmd
{
    char *fullname;		/* Full name of the function (e.g. "commit") */
    char *nick1;		/* alternate name (e.g. "ci") */
    char *nick2;		/* another alternate names (e.g. "ci") */
    int (*func) ();		/* Function takes (argc, argv) arguments. */
} cmds[] =

{
    { "add", "ad", "new", add },
    { "admin", "adm", "rcs", admin },
    { "checkout", "co", "get", checkout },
    { "commit", "ci", "com", commit },
    { "diff", "di", "dif", diff },
    { "export", "exp", "ex", checkout },
    { "history", "hi", "his", history },
    { "import", "im", "imp", import },
    { "log", "lo", "rlog", cvslog },
    { "rdiff", "patch", "pa", patch },
    { "release", "re", "rel", release },
    { "remove", "rm", "delete", cvsremove },
    { "status", "st", "stat", status },
    { "rtag", "rt", "rfreeze", rtag },
    { "tag", "ta", "freeze", tag },
    { "update", "up", "upd", update },
    { NULL, NULL, NULL, NULL },
};

static char *usg[] =
{
    "Usage: %s [cvs-options] command [command-options] [files...]\n",
    "    Where 'cvs-options' are:\n",
    "        -H           Displays Usage information for command\n",
    "        -Q           Cause CVS to be really quiet.\n",
    "        -q           Cause CVS to be somewhat quiet.\n",
    "        -r           Make checked-out files read-only\n",
    "        -w           Make checked-out files read-write (default)\n",
    "        -l           Turn History logging off\n",
    "        -n           Do not execute anything that will change the disk\n",
    "        -t           Show trace of program execution -- Try with -n\n",
    "        -v           CVS version and copyright\n",
    "        -b bindir    Find RCS programs in 'bindir'\n",
    "        -e editor    Use 'editor' for editing log information\n",
    "        -d CVS_root  Overrides $CVSROOT as the root of the CVS tree\n",
#ifdef FREEBSD_DEVELOPER
    "        -x           Do NOT use the FreeBSD -K default flags\n",
#endif
    "\n",
    "    and where 'command' is:\n",
    "        add          Adds a new file/directory to the repository\n",
    "        admin        Administration front end for rcs\n",
    "        checkout     Checkout sources for editing\n",
    "        commit       Checks files into the repository\n",
    "        diff         Runs diffs between revisions\n",
    "        history      Shows status of files and users\n",
    "        import       Import sources into CVS, using vendor branches\n",
    "        export       Export sources from CVS, similar to checkout\n",
    "        log          Prints out 'rlog' information for files\n",
    "        rdiff        'patch' format diffs between releases\n",
    "        release      Indicate that a Module is no longer in use\n",
    "        remove       Removes an entry from the repository\n",
    "        status       Status info on the revisions\n",
    "        tag          Add a symbolic tag to checked out version of RCS file\n",
    "        rtag         Add a symbolic tag to the RCS file\n",
    "        update       Brings work tree in sync with repository\n",
    NULL,
};

static SIGTYPE
main_cleanup ()
{
    exit (1);
}

int
main (argc, argv)
    int argc;
    char *argv[];
{
    extern char *version_string;
    char *cp;
    struct cmd *cm;
    int c, help = FALSE, err = 0;
    int rcsbin_update_env, cvs_update_env;
    char tmp[PATH_MAX];

    /*
     * Just save the last component of the path for error messages
     */
    if ((program_name = rindex (argv[0], '/')) == NULL)
	program_name = argv[0];
    else
	program_name++;

    CurDir = xmalloc (PATH_MAX);
    if (!getwd (CurDir))
	error (1, 0, "cannot get working directory: %s", CurDir);

    /*
     * Query the environment variables up-front, so that
     * they can be overridden by command line arguments
     */
    rcsbin_update_env = *Rcsbin;	/* RCSBIN_DFLT must be set */
    if ((cp = getenv (RCSBIN_ENV)) != NULL)
    {
	Rcsbin = cp;
	rcsbin_update_env = 0;		/* it's already there */
    }
    if ((cp = getenv (EDITOR_ENV)) != NULL)
	Editor = cp;
    if ((cp = getenv (CVSROOT_ENV)) != NULL)
    {
	CVSroot = cp;
	cvs_update_env = 0;		/* it's already there */
    }
    if (getenv (CVSREAD_ENV) != NULL)
	cvswrite = FALSE;

    optind = 1;
#ifdef FREEBSD_DEVELOPER
    while ((c = gnu_getopt (argc, argv, "Qqrwtnlvb:e:d:Hx")) != -1)
#else
    while ((c = gnu_getopt (argc, argv, "Qqrwtnlvb:e:d:H")) != -1)
#endif /* FREEBSD_DEVELOPER */
    {
	switch (c)
	{
	    case 'Q':
		really_quiet = TRUE;
		/* FALL THROUGH */
	    case 'q':
		quiet = TRUE;
		break;
	    case 'r':
		cvswrite = FALSE;
		break;
	    case 'w':
		cvswrite = TRUE;
		break;
	    case 't':
		trace = TRUE;
		break;
	    case 'n':
		noexec = TRUE;
	    case 'l':			/* Fall through */
		logoff = TRUE;
		break;
	    case 'v':
		(void) fputs (rcsid, stdout);
		(void) fputs (version_string, stdout);
		(void) sprintf (tmp, "Patch Level: %d\n", PATCHLEVEL);
		(void) fputs (tmp, stdout);
		(void) fputs ("\nCopyright (c) 1992, Brian Berliner and Jeff Polk\nCopyright (c) 1989-1992, Brian Berliner\n\nCVS may be copied only under the terms of the GNU General Public License,\na copy of which can be found with the CVS 1.3 distribution kit.\n", stdout);
		exit (0);
		break;
	    case 'b':
		Rcsbin = optarg;
		rcsbin_update_env = 1;	/* need to update environment */
		break;
	    case 'e':
		Editor = optarg;
		break;
	    case 'd':
		CVSroot = optarg;
		cvs_update_env = 1;	/* need to update environment */
		break;
	    case 'H':
		help = TRUE;
		break;
#ifdef FREEBSD_DEVELOPER
	    case 'x':
		freebsd = FALSE;
		break;
#endif /* FREEBSD_DEVELOPER */
	    case '?':
	    default:
		usage (usg);
	}
    }
    argc -= optind;
    argv += optind;
    if (argc < 1)
	usage (usg);

    /*
     * XXX - Compatibility.  This can be removed in the release after CVS 1.3.
     * Try to rename the CVSROOT.adm file to CVSROOT, unless there already is
     * a CVSROOT directory.
     */
    if (CVSroot != NULL)
    {
	char rootadm[PATH_MAX];
	char orootadm[PATH_MAX];

	(void) sprintf (rootadm, "%s/%s", CVSroot, CVSROOTADM);
	if (!isdir (rootadm))
	{
	    (void) sprintf (orootadm, "%s/%s", CVSroot, OCVSROOTADM);
	    if (isdir (orootadm))
		(void) rename (orootadm, rootadm);
	}
	strip_path (CVSroot);
    }

    /*
     * Specifying just the '-H' flag to the sub-command causes a Usage
     * message to be displayed.
     */
    command_name = cp = argv[0];
    if (help == TRUE || (argc > 1 && strcmp (argv[1], "-H") == 0))
	argc = -1;
    else
    {
	/*
	 * Check to see if we can write into the history file.  If not,
	 * we assume that we can't work in the repository.
	 * BUT, only if the history file exists.
	 */
	{
	    char path[PATH_MAX];
	    int save_errno;

	    if (!CVSroot || !*CVSroot)
		error (1, 0, "You don't have a %s environment variable",
		       CVSROOT_ENV);
	    (void) sprintf (path, "%s/%s", CVSroot, CVSROOTADM);
	    if (access (path, R_OK | X_OK))
	    {
		save_errno = errno;
		error (0, 0,
		    "Sorry, you don't have sufficient access to %s", CVSroot);
		error (1, save_errno, "%s", path);
	    }
	    (void) strcat (path, "/");
	    (void) strcat (path, CVSROOTADM_HISTORY);
	    if (isfile (path) && access (path, R_OK | (logoff ? 0 : W_OK)))
	    {
		save_errno = errno;
		error (0, 0,
		 "Sorry, you don't have read/write access to the history file");
		error (1, save_errno, "%s", path);
	    }
	}
    }

#ifndef PUTENV_MISSING
    /* Now, see if we should update the environment with the Rcsbin value */
    if (cvs_update_env)
    {
	char *env;

	env = xmalloc (strlen (CVSROOT_ENV) + strlen (CVSroot) + 1 + 1);
	(void) sprintf (env, "%s=%s", CVSROOT_ENV, CVSroot);
	(void) putenv (env);
	/* do not free env, as putenv has control of it */
    }
    if (rcsbin_update_env)
    {
	char *env;

	env = xmalloc (strlen (RCSBIN_ENV) + strlen (Rcsbin) + 1 + 1);
	(void) sprintf (env, "%s=%s", RCSBIN_ENV, Rcsbin);
	(void) putenv (env);
	/* do not free env, as putenv has control of it */
    }
#endif

    /*
     * If Rcsbin is set to something, make sure it is terminated with
     * a slash character.  If not, add one.
     */
    if (*Rcsbin)
    {
	int len = strlen (Rcsbin);
	char *rcsbin;

	if (Rcsbin[len - 1] != '/')
	{
	    rcsbin = Rcsbin;
	    Rcsbin = xmalloc (len + 2);	/* one for '/', one for NULL */
	    (void) strcpy (Rcsbin, rcsbin);
	    (void) strcat (Rcsbin, "/");
	}
    }

    for (cm = cmds; cm->fullname; cm++)
    {
	if (cm->nick1 && !strcmp (cp, cm->nick1))
	    break;
	if (cm->nick2 && !strcmp (cp, cm->nick2))
	    break;
	if (!strcmp (cp, cm->fullname))
	    break;
    }

    if (!cm->fullname)
	usage (usg);			/* no match */
    else
    {
	command_name = cm->fullname;	/* Global pointer for later use */
	(void) SIG_register (SIGHUP, main_cleanup);
	(void) SIG_register (SIGINT, main_cleanup);
	(void) SIG_register (SIGQUIT, main_cleanup);
	(void) SIG_register (SIGPIPE, main_cleanup);
	(void) SIG_register (SIGTERM, main_cleanup);

#ifndef SETVBUF_MISSING
	/*
	 * Make stdout line buffered, so 'tail -f' can monitor progress.
	 * Patch creates too much output to monitor and it runs slowly.
	 */
	if (strcmp (cm->fullname, "patch"))
	    (void) setvbuf (stdout, (char *) NULL, _IOLBF, 0);
#endif

	err = (*(cm->func)) (argc, argv);
    }
    /*
     * If the command's error count is modulo 256, we need to change it
     * so that we don't overflow the 8-bits we get to report exit status
     */
    if (err && (err % 256) == 0)
	err = 1;
    Lock_Cleanup ();
    return (err);
}

char *
Make_Date (rawdate)
    char *rawdate;
{
    struct tm *ftm;
    time_t unixtime;
    char date[256];			/* XXX bigger than we'll ever need? */
    char *ret;

    unixtime = get_date (rawdate, (struct timeb *) NULL);
    if (unixtime == (time_t) - 1)
	error (1, 0, "Can't parse date/time: %s", rawdate);
#ifdef HAVE_RCS5
    ftm = gmtime (&unixtime);
#else
    ftm = localtime (&unixtime);
#endif
    (void) sprintf (date, DATEFORM,
		    ftm->tm_year + (ftm->tm_year < 100 ? 0 : 1900),
		    ftm->tm_mon + 1, ftm->tm_mday, ftm->tm_hour,
		    ftm->tm_min, ftm->tm_sec);
    ret = xstrdup (date);
    return (ret);
}

void
usage (cpp)
    register char **cpp;
{
    (void) fprintf (stderr, *cpp++, program_name, command_name);
    for (; *cpp; cpp++)
	(void) fprintf (stderr, *cpp);
    exit (1);
}
