/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * mkmodules
 * 
 * Re-build the modules database for the CVS system.  Accepts one argument,
 * which is the directory that the modules,v file lives in.
 */

#include "cvs.h"

#undef PATH_MAX
#define PATH_MAX        1024    /* max number of bytes in pathname */

#ifndef lint
static char rcsid[] = "@(#)mkmodules.c 1.39 92/03/31";
#endif

#ifndef DBLKSIZ
#define	DBLKSIZ	4096			/* since GNU ndbm doesn't define it */
#endif

char *program_name, *command_name;

char *Rcsbin = RCSBIN_DFLT;
int noexec = 0;				/* Here only to satisfy use in subr.c */
int trace = 0;				/* Here only to satisfy use in subr.c */

#if __STDC__
static int checkout_file (char *file, char *temp);
static void make_tempfile (char *temp);
static void mkmodules_usage (void);
static void rename_rcsfile (char *temp, char *real);

#ifndef MY_NDBM
static void rename_dbmfile (char *temp);
static void write_dbmfile (char *temp);
#endif				/* !MY_NDBM */

#else				/* !__STDC__ */

static void make_tempfile ();
static int checkout_file ();
static void rename_rcsfile ();
static void mkmodules_usage ();

#ifndef MY_NDBM
static void write_dbmfile ();
static void rename_dbmfile ();
#endif				/* !MY_NDBM */

#endif				/* __STDC__ */

int
main (argc, argv)
    int argc;
    char *argv[];
{
    extern char *getenv ();
    char temp[PATH_MAX];
    char *cp;
#ifdef MY_NDBM
    DBM *db;
#endif

    /*
     * Just save the last component of the path for error messages
     */
    if ((program_name = rindex (argv[0], '/')) == NULL)
	program_name = argv[0];
    else
	program_name++;

    if (argc != 2)
	mkmodules_usage ();

    if ((cp = getenv (RCSBIN_ENV)) != NULL)
	Rcsbin = cp;

    /*
     * If Rcsbin is set to something, make sure it is terminated with a slash
     * character.  If not, add one.
     */
    if (Rcsbin[0] != '\0')
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

    if (chdir (argv[1]) < 0)
	error (1, errno, "cannot chdir to %s", argv[1]);

    /*
     * First, do the work necessary to update the "modules" database.
     */
    make_tempfile (temp);
    switch (checkout_file (CVSROOTADM_MODULES, temp))
    {

	case 0:			/* everything ok */
#ifdef MY_NDBM
	    /* open it, to generate any duplicate errors */
	    if ((db = dbm_open (temp, O_RDONLY, 0666)) != NULL)
		dbm_close (db);
#else
	    write_dbmfile (temp);
	    rename_dbmfile (temp);
#endif
	    rename_rcsfile (temp, CVSROOTADM_MODULES);
	    break;

	case -1:			/* fork failed */
	    (void) unlink_file (temp);
	    exit (1);
	    /* NOTREACHED */

	default:
	    error (0, 0, 
		"'cvs checkout' is less functional without a %s file",
		CVSROOTADM_MODULES);
	    break;
    }					/* switch on checkout_file() */

    (void) unlink_file (temp);

    /*
     * Now, check out the "loginfo" file, so that it is always up-to-date in
     * the CVSROOT directory.
     */
    make_tempfile (temp);
    if (checkout_file (CVSROOTADM_LOGINFO, temp) == 0)
	rename_rcsfile (temp, CVSROOTADM_LOGINFO);
    else
	error (0, 0, 
	"no logging of 'cvs commit' messages is done without a %s file",
	       CVSROOTADM_LOGINFO);
    (void) unlink_file (temp);

    /*
     * Now, check out the "rcsinfo" file, so that it is always up-to-date in
     * the CVSROOT directory.
     */
    make_tempfile (temp);
    if (checkout_file (CVSROOTADM_RCSINFO, temp) == 0)
	rename_rcsfile (temp, CVSROOTADM_RCSINFO);
    else
	error (0, 0, 
	    "a %s file can be used to configure 'cvs commit' templates",
	    CVSROOTADM_RCSINFO);
    (void) unlink_file (temp);

    /*
     * Now, check out the "editinfo" file, so that it is always up-to-date in
     * the CVSROOT directory.
     */
    make_tempfile (temp);
    if (checkout_file (CVSROOTADM_EDITINFO, temp) == 0)
	rename_rcsfile (temp, CVSROOTADM_EDITINFO);
    else
	error (0, 0, 
	       "a %s file can be used to validate log messages",
	       CVSROOTADM_EDITINFO);
    (void) unlink_file (temp);

    /*
     * Now, check out the "commitinfo" file, so that it is always up-to-date
     * in the CVSROOT directory.
     */
    make_tempfile (temp);
    if (checkout_file (CVSROOTADM_COMMITINFO, temp) == 0)
	rename_rcsfile (temp, CVSROOTADM_COMMITINFO);
    else
	error (0, 0, 
	    "a %s file can be used to configure 'cvs commit' checking",
	    CVSROOTADM_COMMITINFO);
    (void) unlink_file (temp);
    return (0);
}

/*
 * Yeah, I know, there are NFS race conditions here.
 */
static void
make_tempfile (temp)
    char *temp;
{
    static int seed = 0;
    int fd;

    if (seed == 0)
	seed = getpid ();
    while (1)
    {
	(void) sprintf (temp, "%s%d", BAKPREFIX, seed++);
	if ((fd = open (temp, O_CREAT|O_EXCL|O_RDWR, 0666)) != -1)
	    break;
	if (errno != EEXIST)
	    error (1, errno, "cannot create temporary file %s", temp);
    }
    if (close(fd) < 0)
	error(1, errno, "cannot close temporary file %s", temp);
}

static int
checkout_file (file, temp)
    char *file;
    char *temp;
{
    char rcs[PATH_MAX];
    int retcode = 0;

    (void) sprintf (rcs, "%s%s", file, RCSEXT);
    if (!isfile (rcs))
	return (1);
    run_setup ("%s%s -q -p", Rcsbin, RCS_CO);
    run_arg (rcs);
    if ((retcode = run_exec (RUN_TTY, temp, RUN_TTY, RUN_NORMAL)) != 0)
    {
	error (0, retcode == -1 ? errno : 0, "failed to check out %s file", file);
    }
    return (retcode);
}

#ifndef MY_NDBM

static void
write_dbmfile (temp)
    char *temp;
{
    char line[DBLKSIZ], value[DBLKSIZ];
    FILE *fp;
    DBM *db;
    char *cp, *vp;
    datum key, val;
    int len, cont, err = 0;

    fp = open_file (temp, "r");
    if ((db = dbm_open (temp, O_RDWR | O_CREAT | O_TRUNC, 0666)) == NULL)
	error (1, errno, "cannot open dbm file %s for creation", temp);
    for (cont = 0; fgets (line, sizeof (line), fp) != NULL;)
    {
	if ((cp = rindex (line, '\n')) != NULL)
	    *cp = '\0';			/* strip the newline */

	/*
	 * Add the line to the value, at the end if this is a continuation
	 * line; otherwise at the beginning, but only after any trailing
	 * backslash is removed.
	 */
	vp = value;
	if (cont)
	    vp += strlen (value);

	/*
	 * See if the line we read is a continuation line, and strip the
	 * backslash if so.
	 */
	len = strlen (line);
	if (len > 0)
	    cp = &line[len - 1];
	else
	    cp = line;
	if (*cp == '\\')
	{
	    cont = 1;
	    *cp = '\0';
	}
	else
	{
	    cont = 0;
	}
	(void) strcpy (vp, line);
	if (value[0] == '#')
	    continue;			/* comment line */
	vp = value;
	while (*vp && isspace (*vp))
	    vp++;
	if (*vp == '\0')
	    continue;			/* empty line */

	/*
	 * If this was not a continuation line, add the entry to the database
	 */
	if (!cont)
	{
	    key.dptr = vp;
	    while (*vp && !isspace (*vp))
		vp++;
	    key.dsize = vp - key.dptr;
	    *vp++ = '\0';		/* NULL terminate the key */
	    while (*vp && isspace (*vp))
		vp++;			/* skip whitespace to value */
	    if (*vp == '\0')
	    {
		error (0, 0, "warning: NULL value for key `%s'", key.dptr);
		continue;
	    }
	    val.dptr = vp;
	    val.dsize = strlen (vp);
	    if (dbm_store (db, key, val, DBM_INSERT) == 1)
	    {
		error (0, 0, "duplicate key found for `%s'", key.dptr);
		err++;
	    }
	}
    }
    dbm_close (db);
    (void) fclose (fp);
    if (err)
    {
	char dotdir[50], dotpag[50];

	(void) sprintf (dotdir, "%s.dir", temp);
	(void) sprintf (dotpag, "%s.pag", temp);
	(void) unlink_file (dotdir);
	(void) unlink_file (dotpag);
	error (1, 0, "DBM creation failed; correct above errors");
    }
}

static void
rename_dbmfile (temp)
    char *temp;
{
    char newdir[50], newpag[50];
    char dotdir[50], dotpag[50];
    char bakdir[50], bakpag[50];

    (void) sprintf (dotdir, "%s.dir", CVSROOTADM_MODULES);
    (void) sprintf (dotpag, "%s.pag", CVSROOTADM_MODULES);
    (void) sprintf (bakdir, "%s%s.dir", BAKPREFIX, CVSROOTADM_MODULES);
    (void) sprintf (bakpag, "%s%s.pag", BAKPREFIX, CVSROOTADM_MODULES);
    (void) sprintf (newdir, "%s.dir", temp);
    (void) sprintf (newpag, "%s.pag", temp);

    (void) chmod (newdir, 0666);
    (void) chmod (newpag, 0666);

    /* don't mess with me */
    SIG_beginCrSect ();

    (void) unlink_file (bakdir);	/* rm .#modules.dir .#modules.pag */
    (void) unlink_file (bakpag);
    (void) rename (dotdir, bakdir);	/* mv modules.dir .#modules.dir */
    (void) rename (dotpag, bakpag);	/* mv modules.pag .#modules.pag */
    (void) rename (newdir, dotdir);	/* mv "temp".dir modules.dir */
    (void) rename (newpag, dotpag);	/* mv "temp".pag modules.pag */

    /* OK -- make my day */
    SIG_endCrSect ();
}

#endif				/* !MY_NDBM */

static void
rename_rcsfile (temp, real)
    char *temp;
    char *real;
{
    char bak[50];

    if (chmod (temp, 0444) < 0)		/* chmod 444 "temp" */
	error (0, errno, "warning: cannot chmod %s", temp);
    (void) sprintf (bak, "%s%s", BAKPREFIX, real);
    (void) unlink_file (bak);		/* rm .#loginfo */
    (void) rename (real, bak);		/* mv loginfo .#loginfo */
    (void) rename (temp, real);		/* mv "temp" loginfo */
}

/*
 * For error() only
 */
void
Lock_Cleanup ()
{
}

static void
mkmodules_usage ()
{
    (void) fprintf (stderr, "Usage: %s modules-directory\n", program_name);
    exit (1);
}
