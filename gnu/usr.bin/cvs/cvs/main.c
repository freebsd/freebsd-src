/*
 *    Copyright (c) 1992, Brian Berliner and Jeff Polk
 *    Copyright (c) 1989-1992, Brian Berliner
 *
 *    You may distribute under the terms of the GNU General Public License
 *    as specified in the README file that comes with the CVS 1.4 kit.
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
 *		login		Record user, host, repos, password
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

#if HAVE_KERBEROS
#include <sys/socket.h>
#include <netinet/in.h>
#include <krb.h>
#ifndef HAVE_KRB_GET_ERR_TEXT
#define krb_get_err_text(status) krb_err_txt[status]
#endif
#endif

#ifndef lint
static const char rcsid[] = "$CVSid: @(#)main.c 1.78 94/10/07 $\n";
USE(rcsid);
#endif

char *program_name;
char *program_path;
/*
 * Initialize comamnd_name to "cvs" so that the first call to
 * read_cvsrc tries to find global cvs options.
 */
char *command_name = "cvs";

/*
 * Since some systems don't define this...
 */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN  256
#endif

char hostname[MAXHOSTNAMELEN];

#ifdef AUTH_CLIENT_SUPPORT
int use_authenticating_server = FALSE;
#endif /* AUTH_CLIENT_SUPPORT */
int use_editor = TRUE;
int use_cvsrc = TRUE;
int cvswrite = !CVSREAD_DFLT;
int really_quiet = FALSE;
int quiet = FALSE;
int trace = FALSE;
int noexec = FALSE;
int logoff = FALSE;
mode_t cvsumask = UMASK_DFLT;

char *CurDir;

/*
 * Defaults, for the environment variables that are not set
 */
char *Rcsbin = RCSBIN_DFLT;
char *Editor = EDITOR_DFLT;
char *CVSroot = CVSROOT_DFLT;
#ifdef CVSADM_ROOT
/*
 * The path found in CVS/Root must match $CVSROOT and/or 'cvs -d root'
 */
char *CVSADM_Root = CVSROOT_DFLT;
#endif /* CVSADM_ROOT */

int add PROTO((int argc, char **argv));
int admin PROTO((int argc, char **argv));
int checkout PROTO((int argc, char **argv));
int commit PROTO((int argc, char **argv));
int diff PROTO((int argc, char **argv));
int history PROTO((int argc, char **argv));
int import PROTO((int argc, char **argv));
int cvslog PROTO((int argc, char **argv));
#ifdef AUTH_CLIENT_SUPPORT
int login PROTO((int argc, char **argv));
#endif /* AUTH_CLIENT_SUPPORT */
int patch PROTO((int argc, char **argv));
int release PROTO((int argc, char **argv));
int cvsremove PROTO((int argc, char **argv));
int rtag PROTO((int argc, char **argv));
int status PROTO((int argc, char **argv));
int tag PROTO((int argc, char **argv));
int update PROTO((int argc, char **argv));

const struct cmd
{
    char *fullname;		/* Full name of the function (e.g. "commit") */
    char *nick1;		/* alternate name (e.g. "ci") */
    char *nick2;		/* another alternate names (e.g. "ci") */
    int (*func) ();		/* Function takes (argc, argv) arguments. */
#ifdef CLIENT_SUPPORT
    int (*client_func) ();	/* Function to do it via the protocol.  */
#endif
} cmds[] =

{
#ifdef CLIENT_SUPPORT
#define CMD_ENTRY(n1, n2, n3, f1, f2) { n1, n2, n3, f1, f2 }
#else
#define CMD_ENTRY(n1, n2, n3, f1, f2) { n1, n2, n3, f1 }
#endif

    CMD_ENTRY("add",      "ad",    "new",     add,       client_add),
#ifndef CVS_NOADMIN
    CMD_ENTRY("admin",    "adm",   "rcs",     admin,     client_admin),
#endif
    CMD_ENTRY("checkout", "co",    "get",     checkout,  client_checkout),
    CMD_ENTRY("commit",   "ci",    "com",     commit,    client_commit),
    CMD_ENTRY("diff",     "di",    "dif",     diff,      client_diff),
    CMD_ENTRY("export",   "exp",   "ex",      checkout,  client_export),
    CMD_ENTRY("history",  "hi",    "his",     history,   client_history),
    CMD_ENTRY("import",   "im",    "imp",     import,    client_import),
    CMD_ENTRY("log",      "lo",    "rlog",    cvslog,    client_log),
#ifdef AUTH_CLIENT_SUPPORT
    CMD_ENTRY("login",    "logon", "lgn",     login,     login),
#endif /* AUTH_CLIENT_SUPPORT */
    CMD_ENTRY("rdiff",    "patch", "pa",      patch,     client_rdiff),
    CMD_ENTRY("release",  "re",    "rel",     release,   client_release),
    CMD_ENTRY("remove",   "rm",    "delete",  cvsremove, client_remove),
    CMD_ENTRY("status",   "st",    "stat",    status,    client_status),
    CMD_ENTRY("rtag",     "rt",    "rfreeze", rtag,      client_rtag),
    CMD_ENTRY("tag",      "ta",    "freeze",  tag,       client_tag),
    CMD_ENTRY("update",   "up",    "upd",     update,    client_update),

#ifdef SERVER_SUPPORT
    /*
     * The client_func is also server because we might have picked up a
     * CVSROOT environment variable containing a colon.  The client will send
     * the real root later.
     */
    CMD_ENTRY("server",   "server", "server", server,    server),
#endif
    CMD_ENTRY(NULL, NULL, NULL, NULL, NULL),

#undef CMD_ENTRY
};

static const char *const usg[] =
{
    "Usage: %s [cvs-options] command [command-options] [files...]\n",
    "    Where 'cvs-options' are:\n",
    "        -H           Displays Usage information for command\n",
    "        -Q           Cause CVS to be really quiet.\n",
    "        -q           Cause CVS to be somewhat quiet.\n",
#ifdef AUTH_CLIENT_SUPPORT
    "        -a           Use the authenticating server, not rsh.\n",
#endif /* AUTH_CLIENT_SUPPORT */
    "        -r           Make checked-out files read-only\n",
    "        -w           Make checked-out files read-write (default)\n",
    "        -l           Turn History logging off\n",
    "        -n           Do not execute anything that will change the disk\n",
    "        -t           Show trace of program execution -- Try with -n\n",
    "        -v           CVS version and copyright\n",
    "        -b bindir    Find RCS programs in 'bindir'\n",
    "        -e editor    Use 'editor' for editing log information\n",
    "        -d CVS_root  Overrides $CVSROOT as the root of the CVS tree\n",
    "        -f           Do not use the ~/.cvsrc file\n",
#ifdef CLIENT_SUPPORT
    "        -z #         Use 'gzip -#' for net traffic if possible.\n",
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
#ifdef AUTH_CLIENT_SUPPORT
    "        login        Prompt for password for authenticating server.\n",
#endif /* AUTH_CLIENT_SUPPORT */
    "        rdiff        'patch' format diffs between releases\n",
    "        release      Indicate that a Module is no longer in use\n",
    "        remove       Removes an entry from the repository\n",
    "        status       Status info on the revisions\n",
    "        tag          Add a symbolic tag to checked out version of RCS file\n",
    "        rtag         Add a symbolic tag to the RCS file\n",
    "        update       Brings work tree in sync with repository\n",
    NULL,
};

static RETSIGTYPE
main_cleanup ()
{
    exit (1);
}

static void
error_cleanup ()
{
    Lock_Cleanup();
#ifdef SERVER_SUPPORT
    if (server_active)
	server_cleanup (0);
#endif
}

int
main (argc, argv)
    int argc;
    char **argv;
{
    extern char *version_string;
    extern char *config_string;
    char *cp, *end;
    const struct cmd *cm;
    int c, err = 0;
    static int help = FALSE, version_flag = FALSE;
    int rcsbin_update_env, cvs_update_env = 0;
    char tmp[PATH_MAX];
    static struct option long_options[] =
      {
        {"help", 0, &help, TRUE},
        {"version", 0, &version_flag, TRUE},
        {0, 0, 0, 0}
      };
    /* `getopt_long' stores the option index here, but right now we
        don't use it. */
    int option_index = 0;

    error_set_cleanup (error_cleanup);

/* The IBM TCP/IP library under OS/2 needs to be initialized: */
#ifdef NEED_CALL_SOCKINIT
	if (SockInit () != TRUE)
	{
          fprintf (stderr, "SockInit() failed!\n");
          exit (1);
	}
#endif /* NEED_CALL_SOCKINIT */

    /*
     * Just save the last component of the path for error messages
     */
    program_path = xstrdup (argv[0]);
    program_name = last_component (argv[0]);

    CurDir = xmalloc (PATH_MAX);
#ifndef SERVER_SUPPORT
    if (!getwd (CurDir))
	error (1, 0, "cannot get working directory: %s", CurDir);
#endif

    /*
     * Query the environment variables up-front, so that
     * they can be overridden by command line arguments
     */
    rcsbin_update_env = *Rcsbin;	/* RCSBIN_DFLT must be set */
    cvs_update_env = 0;
    if ((cp = getenv (RCSBIN_ENV)) != NULL)
    {
	Rcsbin = cp;
	rcsbin_update_env = 0;		/* it's already there */
    }
    if ((cp = getenv (EDITOR1_ENV)) != NULL)
 	Editor = cp;
    else if ((cp = getenv (EDITOR2_ENV)) != NULL)
	Editor = cp;
    else if ((cp = getenv (EDITOR3_ENV)) != NULL)
	Editor = cp;
    if ((cp = getenv (CVSROOT_ENV)) != NULL)
    {
	CVSroot = cp;
	cvs_update_env = 0;		/* it's already there */
    }
    if (getenv (CVSREAD_ENV) != NULL)
	cvswrite = FALSE;
    if ((cp = getenv (CVSUMASK_ENV)) != NULL)
    {
	/* FIXME: Should be accepting symbolic as well as numeric mask.  */
	cvsumask = strtol (cp, &end, 8) & 0777;
	if (*end != '\0')
	    error (1, errno, "invalid umask value in %s (%s)",
		CVSUMASK_ENV, cp);
    }

    /*
     * Scan cvsrc file for global options.
     */
    read_cvsrc(&argc, &argv);

    /* This has the effect of setting getopt's ordering to REQUIRE_ORDER,
       which is what we need to distinguish between global options and
       command options.  FIXME: It would appear to be possible to do this
       much less kludgily by passing "+" as the first character to the
       option string we pass to getopt_long.  */
    optind = 1;

    while ((c = getopt_long
            (argc, argv, "Qqrawtnlvb:e:d:Hfz:", long_options, &option_index))
           != EOF)
      {
	switch (c)
          {
            case 0:
                /* getopt_long took care of setting the flag. */ 
                break;
	    case 'Q':
		really_quiet = TRUE;
		/* FALL THROUGH */
	    case 'q':
		quiet = TRUE;
		break;
	    case 'r':
		cvswrite = FALSE;
		break;
#ifdef AUTH_CLIENT_SUPPORT
	    case 'a':
		use_authenticating_server = TRUE;
		break;
#endif /* AUTH_CLIENT_SUPPORT */
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
                version_flag = TRUE;
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
		use_cvsrc = FALSE;      /* this ensure that cvs -H works */
		help = TRUE;
		break;
            case 'f':
		use_cvsrc = FALSE;
		break;
#ifdef CLIENT_SUPPORT
	    case 'z':
		gzip_level = atoi (optarg);
		if (gzip_level <= 0 || gzip_level > 9)
		  error (1, 0,
			 "gzip compression level must be between 1 and 9");
		break;
#endif
	    case '?':
	    default:
                usage (usg);
	}
    }

    if (version_flag == TRUE)
      {
        (void) fputs (version_string, stdout);
        (void) fputs (config_string, stdout);
        (void) sprintf (tmp, "Patch Level: %d\n", PATCHLEVEL);
        (void) fputs (tmp, stdout);
        (void) fputs ("\n", stdout);
        (void) fputs ("Copyright (c) 1993-1994 Brian Berliner\n", stdout);
        (void) fputs ("Copyright (c) 1993-1994 david d `zoo' zuhn\n", stdout);
        (void) fputs ("Copyright (c) 1992, Brian Berliner and Jeff Polk\n", stdout);
        (void) fputs ("Copyright (c) 1989-1992, Brian Berliner\n", stdout);
        (void) fputs ("\n", stdout);
        (void) fputs ("CVS may be copied only under the terms of the GNU General Public License,\n", stdout);
        (void) fputs ("a copy of which can be found with the CVS distribution kit.\n", stdout);
        exit (0);
      }

    argc -= optind;
    argv += optind;
    if (argc < 1)
	usage (usg);

#ifdef HAVE_KERBEROS
    /* If we are invoked with a single argument "kserver", then we are
       running as Kerberos server as root.  Do the authentication as
       the very first thing, to minimize the amount of time we are
       running as root.  */
    if (strcmp (argv[0], "kserver") == 0)
    {
	int status;
	char instance[INST_SZ];
	struct sockaddr_in peer;
	struct sockaddr_in laddr;
	int len;
	KTEXT_ST ticket;
	AUTH_DAT auth;
	char version[KRB_SENDAUTH_VLEN];
	Key_schedule sched;
	char user[ANAME_SZ];
	struct passwd *pw;

	strcpy (instance, "*");
	len = sizeof peer;
	if (getpeername (STDIN_FILENO, (struct sockaddr *) &peer, &len) < 0
	    || getsockname (STDIN_FILENO, (struct sockaddr *) &laddr,
			    &len) < 0)
	{
	    printf ("E Fatal error, aborting.\n\
error %s getpeername or getsockname failed\n", strerror (errno));
	    exit (1);
	}

	status = krb_recvauth (KOPT_DO_MUTUAL, STDIN_FILENO, &ticket, "rcmd",
			       instance, &peer, &laddr, &auth, "", sched,
			       version);
	if (status != KSUCCESS)
	{
	    printf ("E Fatal error, aborting.\n\
error 0 kerberos: %s\n", krb_get_err_text(status));
	    exit (1);
	}

	/* Get the local name.  */
	status = krb_kntoln (&auth, user);
	if (status != KSUCCESS)
	{
	    printf ("E Fatal error, aborting.\n\
error 0 kerberos: can't get local name: %s\n", krb_get_err_text(status));
	    exit (1);
	}

	pw = getpwnam (user);
	if (pw == NULL)
	{
	    printf ("E Fatal error, aborting.\n\
error 0 %s: no such user\n", user);
	    exit (1);
	}

	initgroups (pw->pw_name, pw->pw_gid);
	setgid (pw->pw_gid);
	setuid (pw->pw_uid);
	/* Inhibit access by randoms.  Don't want people randomly
	   changing our temporary tree before we check things in.  */
	umask (077);

#if HAVE_PUTENV
	/* Set LOGNAME and USER in the environment, in case they are
           already set to something else.  */
	{
	    char *env;

	    env = xmalloc (sizeof "LOGNAME=" + strlen (user));
	    (void) sprintf (env, "LOGNAME=%s", user);
	    (void) putenv (env);

	    env = xmalloc (sizeof "USER=" + strlen (user));
	    (void) sprintf (env, "USER=%s", user);
	    (void) putenv (env);
	}
#endif

	/* Pretend we were invoked as a plain server.  */
	argv[0] = "server";
    }
#endif /* HAVE_KERBEROS */


#if defined(AUTH_SERVER_SUPPORT) && defined(SERVER_SUPPORT)
    if (strcmp (argv[0], "pserver") == 0)
    {
      /* Gets username and password from client, authenticates, then
         switches to run as that user and sends an ACK back to the
         client. */
      authenticate_connection ();
      
      /* Pretend we were invoked as a plain server.  */
      argv[0] = "server";
    }
#endif /* AUTH_SERVER_SUPPORT && SERVER_SUPPORT */


#ifdef CVSADM_ROOT
    /*
     * See if we are able to find a 'better' value for CVSroot in the
     * CVSADM_ROOT directory.
     */
#ifdef SERVER_SUPPORT
    if (strcmp (argv[0], "server") == 0 && CVSroot == NULL)
        CVSADM_Root = NULL;
    else
        CVSADM_Root = Name_Root((char *) NULL, (char *) NULL);
#else /* No SERVER_SUPPORT */
    CVSADM_Root = Name_Root((char *) NULL, (char *) NULL);
#endif /* No SERVER_SUPPORT */
    if (CVSADM_Root != NULL)
    {
        if (CVSroot == NULL || !cvs_update_env)
        {
	    CVSroot = CVSADM_Root;
	    cvs_update_env = 1;	/* need to update environment */
        }
#ifdef CLIENT_SUPPORT
        else if (!getenv ("CVS_IGNORE_REMOTE_ROOT"))
#else
        else
#endif
        {
            /*
	     * Now for the hard part, compare the two directories. If they
	     * are not identical, then abort this command.
	     */
            if ((fncmp (CVSroot, CVSADM_Root) != 0) &&
		!same_directories(CVSroot, CVSADM_Root))
	    {
              error (0, 0, "%s value for CVS Root found in %s",
                     CVSADM_Root, CVSADM_ROOT);
              error (0, 0, "does not match command line -d %s setting",
                     CVSroot);
              error (1, 0,
                      "you may wish to try the cvs command again without the -d option ");
	    }
        }
    }
#endif /* CVSADM_ROOT */

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
#ifdef SERVER_SUPPORT
        if (strcmp (command_name, "server") != 0 || CVSroot != NULL)
#endif
	{
	    char path[PATH_MAX];
	    int save_errno;

	    if (!CVSroot || !*CVSroot)
		error (1, 0, "You don't have a %s environment variable",
		       CVSROOT_ENV);
	    (void) sprintf (path, "%s/%s", CVSroot, CVSROOTADM);
	    if (!isaccessible (path, R_OK | X_OK))
	    {
		save_errno = errno;
#ifdef CLIENT_SUPPORT
		if (strchr (CVSroot, ':') == NULL)
		{
#endif
		error (0, 0,
		    "Sorry, you don't have sufficient access to %s", CVSroot);
		error (1, save_errno, "%s", path);
#ifdef CLIENT_SUPPORT
		}
#endif
	    }
	    (void) strcat (path, "/");
	    (void) strcat (path, CVSROOTADM_HISTORY);
	    if (isfile (path) && !isaccessible (path, R_OK | W_OK))
	    {
		save_errno = errno;
		error (0, 0,
		 "Sorry, you don't have read/write access to the history file");
		error (1, save_errno, "%s", path);
	    }
	}
    }

#ifdef SERVER_SUPPORT
    if (strcmp (command_name, "server") == 0)
	/* This is only used for writing into the history file.  Might
	   be nice to have hostname and/or remote path, on the other hand
	   I'm not sure whether it is worth the trouble.  */
	strcpy (CurDir, "<remote>");
    else if (!getwd (CurDir))
	error (1, 0, "cannot get working directory: %s", CurDir);
#endif

#ifdef HAVE_PUTENV
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

	/* make sure we clean up on error */
#ifdef SIGHUP
	(void) SIG_register (SIGHUP, main_cleanup);
	(void) SIG_register (SIGHUP, Lock_Cleanup);
#endif
#ifdef SIGINT
	(void) SIG_register (SIGINT, main_cleanup);
	(void) SIG_register (SIGINT, Lock_Cleanup);
#endif
#ifdef SIGQUIT
	(void) SIG_register (SIGQUIT, main_cleanup);
	(void) SIG_register (SIGQUIT, Lock_Cleanup);
#endif
#ifdef SIGPIPE
	(void) SIG_register (SIGPIPE, main_cleanup);
	(void) SIG_register (SIGPIPE, Lock_Cleanup);
#endif
#ifdef SIGTERM
	(void) SIG_register (SIGTERM, main_cleanup);
	(void) SIG_register (SIGTERM, Lock_Cleanup);
#endif

	gethostname(hostname, sizeof (hostname));

#ifdef HAVE_SETVBUF
	/*
	 * Make stdout line buffered, so 'tail -f' can monitor progress.
	 * Patch creates too much output to monitor and it runs slowly.
	 */
	if (strcmp (cm->fullname, "patch"))
	    (void) setvbuf (stdout, (char *) NULL, _IOLBF, 0);
#endif

	if (use_cvsrc)
	  read_cvsrc(&argc, &argv);

#ifdef CLIENT_SUPPORT
	/* If cvsroot contains a colon, try to do it via the protocol.  */
        {
	    char *p = CVSroot == NULL ? NULL : strchr (CVSroot, ':');
	    if (p)
		err = (*(cm->client_func)) (argc, argv);
	    else
		err = (*(cm->func)) (argc, argv);
	}
#else /* No CLIENT_SUPPORT */
	err = (*(cm->func)) (argc, argv);

#endif /* No CLIENT_SUPPORT */
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
    register const char *const *cpp;
{
    (void) fprintf (stderr, *cpp++, program_name, command_name);
    for (; *cpp; cpp++)
	(void) fprintf (stderr, *cpp);
    exit (1);
}
