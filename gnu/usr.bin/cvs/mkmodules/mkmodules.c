/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * mkmodules
 * 
 * Re-build the modules database for the CVS system.  Accepts one argument,
 * which is the directory that the modules,v file lives in.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "$CVSid: @(#)mkmodules.c 1.45 94/09/30 $";
USE(rcsid)
#endif

#ifndef DBLKSIZ
#define	DBLKSIZ	4096			/* since GNU ndbm doesn't define it */
#endif

char *program_name, *command_name;

char *Rcsbin = RCSBIN_DFLT;
int noexec = 0;				/* Here only to satisfy use in subr.c */
int trace = 0;				/* Here only to satisfy use in subr.c */

static int checkout_file PROTO((char *file, char *temp));
static void make_tempfile PROTO((char *temp));
static void mkmodules_usage PROTO((void));
static void rename_rcsfile PROTO((char *temp, char *real));

#ifndef MY_NDBM
static void rename_dbmfile PROTO((char *temp));
static void write_dbmfile PROTO((char *temp));
#endif				/* !MY_NDBM */


int
main (argc, argv)
    int argc;
    char *argv[];
{
    extern char *getenv ();
    char temp[PATH_MAX];
    char *cp, *last, *fname;
#ifdef MY_NDBM
    DBM *db;
#endif
    FILE *fp;
    char line[512];
    static struct _checkout_file {
       char *filename;
       char *errormsg;
    } *fileptr, filelist[] = {
    {CVSROOTADM_LOGINFO, 
	"no logging of 'cvs commit' messages is done without a %s file"},
    {CVSROOTADM_RCSINFO,
	"a %s file can be used to configure 'cvs commit' templates"},
    {CVSROOTADM_EDITINFO,
	"a %s file can be used to validate log messages"},
    {CVSROOTADM_COMMITINFO,
	"a %s file can be used to configure 'cvs commit' checking"},
    {CVSROOTADM_IGNORE,
	"a %s file can be used to specify files to ignore"},
    {CVSROOTADM_CHECKOUTLIST,
	"a %s file can specify extra CVSROOT files to auto-checkout"},
    {NULL, NULL}};

    /*
     * Just save the last component of the path for error messages
     */
    if ((program_name = strrchr (argv[0], '/')) == NULL)
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

    /* Checkout the files that need it in CVSROOT dir */
    for (fileptr = filelist; fileptr && fileptr->filename; fileptr++) {
	make_tempfile (temp);
	if (checkout_file (fileptr->filename, temp) == 0)
	    rename_rcsfile (temp, fileptr->filename);
#if 0
	/*
	 * If there was some problem other than the file not existing,
	 * checkout_file already printed a real error message.  If the
	 * file does not exist, it is harmless--it probably just means
	 * that the repository was created with an old version of CVS
	 * which didn't have so many files in CVSROOT.
	 */
	else if (fileptr->errormsg)
	    error (0, 0, fileptr->errormsg, fileptr->filename);
#endif
	(void) unlink_file (temp);
    }

    /* Use 'fopen' instead of 'open_file' because we want to ignore error */
    fp = fopen (CVSROOTADM_CHECKOUTLIST, "r");
    if (fp)
    {
	/*
	 * File format:
	 *  [<whitespace>]<filename><whitespace><error message><end-of-line>
	 */
	for (; fgets (line, sizeof (line), fp) != NULL;)
	{
	    if ((last = strrchr (line, '\n')) != NULL)
		*last = '\0';			/* strip the newline */

	    /* Skip leading white space. */
	    for (fname = line; *fname && isspace(*fname); fname++)
		;

	    /* Find end of filename. */
	    for (cp = fname; *cp && !isspace(*cp); cp++)
		;
	    *cp = '\0';

	    make_tempfile (temp);
	    if (checkout_file (fname, temp) == 0)
	    {
		rename_rcsfile (temp, fname);
	    }
	    else
	    {
		for (cp++; cp < last && *last && isspace(*last); cp++)
		    ;
		if (cp < last && *cp)
		    error (0, 0, cp, fname);
	    }
	}
	(void) fclose (fp);
    }

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
	if ((cp = strrchr (line, '\n')) != NULL)
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
    struct stat statbuf;
    char rcs[PATH_MAX];
    
    /* Set "x" bits if set in original. */
    (void) sprintf (rcs, "%s%s", real, RCSEXT);
    statbuf.st_mode = 0; /* in case rcs file doesn't exist, but it should... */
    (void) stat (rcs, &statbuf);

    if (chmod (temp, 0444 | (statbuf.st_mode & 0111)) < 0)
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
