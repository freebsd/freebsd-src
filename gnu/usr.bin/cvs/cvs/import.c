/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * "import" checks in the vendor release located in the current directory into
 * the CVS source repository.  The CVS vendor branch support is utilized.
 * 
 * At least three arguments are expected to follow the options:
 *	repository	Where the source belongs relative to the CVSROOT
 *	VendorTag	Vendor's major tag
 *	VendorReleTag	Tag for this particular release
 *
 * Additional arguments specify more Vendor Release Tags.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "$CVSid: @(#)import.c 1.63 94/09/30 $";
USE(rcsid)
#endif

#define	FILE_HOLDER	".#cvsxxx"

static char *get_comment PROTO((char *user));
static int add_rcs_file PROTO((char *message, char *rcs, char *user, char *vtag,
		         int targc, char *targv[]));
static int expand_at_signs PROTO((char *buf, off_t size, FILE *fp));
static int add_rev PROTO((char *message, char *rcs, char *vfile, char *vers));
static int add_tags PROTO((char *rcs, char *vfile, char *vtag, int targc,
		     char *targv[]));
static int import_descend PROTO((char *message, char *vtag, int targc, char *targv[]));
static int import_descend_dir PROTO((char *message, char *dir, char *vtag,
			       int targc, char *targv[]));
static int process_import_file PROTO((char *message, char *vfile, char *vtag,
				int targc, char *targv[]));
static int update_rcs_file PROTO((char *message, char *vfile, char *vtag, int targc,
			    char *targv[], int inattic));
static void add_log PROTO((int ch, char *fname));
static int str2expmode PROTO((char const* expstring));
static int strn2expmode PROTO((char const* expstring, size_t n));

static int repos_len;
static char vhead[50];
static char vbranch[50];
static FILE *logfp;
static char repository[PATH_MAX];
static int conflicts;
static int use_file_modtime;
static char *keyword_opt = NULL;

static char *import_usage[] =
{
    "Usage: %s %s [-Qq] [-d] [-k subst] [-I ign] [-m msg] [-b branch]\n",
    "    repository vendor-tag release-tags...\n",
    "\t-Q\tReally quiet.\n",
    "\t-q\tSomewhat quiet.\n",
    "\t-d\tUse the file's modification time as the time of import.\n",
    "\t-k sub\tSet default RCS keyword substitution mode.\n",
    "\t-I ign\tMore files to ignore (! to reset).\n",
    "\t-b bra\tVendor branch id.\n",
    "\t-m msg\tLog message.\n",
    NULL
};

static char *keyword_usage[] =
{
  "%s %s: invalid RCS keyword expansion mode\n",
  "Valid expansion modes include:\n",
  "   -kkv\tGenerate keywords using the default form.\n",
  "   -kkvl\tLike -kkv, except locker's name inserted.\n",
  "   -kk\tGenerate only keyword names in keyword strings.\n",
  "   -kv\tGenerate only keyword values in keyword strings.\n",
  "   -ko\tGenerate the old keyword string (no changes from checked in file).\n",
  NULL,
};


int
import (argc, argv)
    int argc;
    char *argv[];
{
    char *message = NULL;
    char tmpfile[L_tmpnam+1];
    char *cp;
    int i, c, msglen, err;
    List *ulist;
    Node *p;

    if (argc == -1)
	usage (import_usage);

    ign_setup ();

    (void) strcpy (vbranch, CVSBRANCH);
    optind = 1;
    while ((c = getopt (argc, argv, "Qqdb:m:I:k:")) != -1)
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
		use_file_modtime = 1;
		break;
	    case 'b':
		(void) strcpy (vbranch, optarg);
		break;
	    case 'm':
#ifdef FORCE_USE_EDITOR
		use_editor = TRUE;
#else
		use_editor = FALSE;
#endif
		message = xstrdup(optarg);
		break;
	    case 'I':
		ign_add (optarg, 0);
		break;
            case 'k':
		if (str2expmode(optarg) != -1)
		  keyword_opt = optarg;
		else
		  usage (keyword_usage);
		break;
	    case '?':
	    default:
		usage (import_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;
    if (argc < 3)
	usage (import_usage);

    for (i = 1; i < argc; i++)		/* check the tags for validity */
	RCS_check_tag (argv[i]);

    /* XXX - this should be a module, not just a pathname */
    if (argv[0][0] != '/')
    {
	if (CVSroot == NULL)
	{
	    error (0, 0, "missing CVSROOT environment variable\n");
	    error (1, 0, "Set it or specify the '-d' option to %s.",
		   program_name);
	}
	(void) sprintf (repository, "%s/%s", CVSroot, argv[0]);
	repos_len = strlen (CVSroot);
    }
    else
    {
	(void) strcpy (repository, argv[0]);
	repos_len = 0;
    }

    /*
     * Consistency checks on the specified vendor branch.  It must be
     * composed of only numbers and dots ('.').  Also, for now we only
     * support branching to a single level, so the specified vendor branch
     * must only have two dots in it (like "1.1.1").
     */
    for (cp = vbranch; *cp != '\0'; cp++)
	if (!isdigit (*cp) && *cp != '.')
	    error (1, 0, "%s is not a numeric branch", vbranch);
    if (numdots (vbranch) != 2)
	error (1, 0, "Only branches with two dots are supported: %s", vbranch);
    (void) strcpy (vhead, vbranch);
    cp = strrchr (vhead, '.');
    *cp = '\0';
    if (use_editor)
    {
	do_editor ((char *) NULL, &message, repository,
		   (List *) NULL); 
    }

    msglen = message == NULL ? 0 : strlen (message);
    if (msglen == 0 || message[msglen - 1] != '\n')
    {
	char *nm = xmalloc (msglen + 2);
	if (message != NULL)
	{
	    (void) strcpy (nm, message);
	    free (message);
	}
	(void) strcat (nm + msglen, "\n");
	message = nm;
    }

    /*
     * Make all newly created directories writable.  Should really use a more
     * sophisticated security mechanism here.
     */
    (void) umask (2);
    make_directories (repository);

    /* Create the logfile that will be logged upon completion */
    if ((logfp = fopen (tmpnam (tmpfile), "w+")) == NULL)
	error (1, errno, "cannot create temporary file `%s'", tmpfile);
    (void) unlink (tmpfile);		/* to be sure it goes away */
    (void) fprintf (logfp, "\nVendor Tag:\t%s\n", argv[1]);
    (void) fprintf (logfp, "Release Tags:\t");
    for (i = 2; i < argc; i++)
	(void) fprintf (logfp, "%s\n\t\t", argv[i]);
    (void) fprintf (logfp, "\n");

    /* Just Do It.  */
    err = import_descend (message, argv[1], argc - 2, argv + 2);
    if (conflicts)
    {
	if (!really_quiet)
	{
	    (void) printf ("\n%d conflicts created by this import.\n",
			   conflicts);
	    (void) printf ("Use the following command to help the merge:\n\n");
	    (void) printf ("\t%s checkout -j%s:yesterday -j%s %s\n\n",
			   program_name, argv[1], argv[1], argv[0]);
	}

	(void) fprintf (logfp, "\n%d conflicts created by this import.\n",
			conflicts);
	(void) fprintf (logfp,
			"Use the following command to help the merge:\n\n");
	(void) fprintf (logfp, "\t%s checkout -j%s:yesterday -j%s %s\n\n",
			program_name, argv[1], argv[1], argv[0]);
    }
    else
    {
	if (!really_quiet)
	    (void) printf ("\nNo conflicts created by this import\n\n");
	(void) fprintf (logfp, "\nNo conflicts created by this import\n\n");
    }

    /*
     * Write out the logfile and clean up.
     */
    ulist = getlist ();
    p = getnode ();
    p->type = UPDATE;
    p->delproc = update_delproc;
    p->key = xstrdup ("- Imported sources");
    p->data = (char *) T_TITLE;
    (void) addnode (ulist, p);
    Update_Logfile (repository, message, vbranch, logfp, ulist);
    dellist (&ulist);
    (void) fclose (logfp);

    if (message)
	free (message);

    return (err);
}

/*
 * process all the files in ".", then descend into other directories.
 */
static int
import_descend (message, vtag, targc, targv)
    char *message;
    char *vtag;
    int targc;
    char *targv[];
{
    DIR *dirp;
    struct dirent *dp;
    int err = 0;
    int has_dirs = 0;

    /* first, load up any per-directory ignore lists */
    ign_add_file (CVSDOTIGNORE, 1);

    if ((dirp = opendir (".")) == NULL)
    {
	err++;
    }
    else
    {
	while ((dp = readdir (dirp)) != NULL)
	{
	    if (strcmp (dp->d_name, ".") == 0 || strcmp (dp->d_name, "..") == 0)
		continue;
	    if (ign_name (dp->d_name))
	    {
		add_log ('I', dp->d_name);
		continue;
	    }
	    if (isdir (dp->d_name))
	    {
		has_dirs = 1;
	    }
	    else
	    {
		if (islink (dp->d_name))
		{
		    add_log ('L', dp->d_name);
		    err++;
		}
		else
		{
		    err += process_import_file (message, dp->d_name,
						vtag, targc, targv);
		}
	    }
	}
	(void) closedir (dirp);
    }
    if (has_dirs)
    {
	if ((dirp = opendir (".")) == NULL)
	    err++;
	else
	{
	    while ((dp = readdir (dirp)) != NULL)
	    {
		if (!strcmp(".", dp->d_name) || !strcmp("..", dp->d_name))
		    continue;
		if (!isdir (dp->d_name) || ign_name (dp->d_name))
		    continue;
		err += import_descend_dir (message, dp->d_name,
					   vtag, targc, targv);
		/* need to re-load .cvsignore after each dir traversal */
		ign_add_file (CVSDOTIGNORE, 1);
	    }
	    (void) closedir (dirp);
	}
    }
    return (err);
}

/*
 * Process the argument import file.
 */
static int
process_import_file (message, vfile, vtag, targc, targv)
    char *message;
    char *vfile;
    char *vtag;
    int targc;
    char *targv[];
{
    char attic_name[PATH_MAX];
    char rcs[PATH_MAX];
    int inattic = 0;

    (void) sprintf (rcs, "%s/%s%s", repository, vfile, RCSEXT);
    if (!isfile (rcs))
    {
	(void) sprintf (attic_name, "%s/%s/%s%s", repository, CVSATTIC,
			vfile, RCSEXT);
	if (!isfile (attic_name))
	{

	    /*
	     * A new import source file; it doesn't exist as a ,v within the
	     * repository nor in the Attic -- create it anew.
	     */
	    add_log ('N', vfile);
	    return (add_rcs_file (message, rcs, vfile, vtag, targc, targv));
	}
	inattic = 1;
    }

    /*
     * an rcs file exists. have to do things the official, slow, way.
     */
    return (update_rcs_file (message, vfile, vtag, targc, targv, inattic));
}

/*
 * The RCS file exists; update it by adding the new import file to the
 * (possibly already existing) vendor branch.
 */
static int
update_rcs_file (message, vfile, vtag, targc, targv, inattic)
    char *message;
    char *vfile;
    char *vtag;
    int targc;
    char *targv[];
    int inattic;
{
    Vers_TS *vers;
    int letter;
    int ierrno;
    char *tmpdir;

    vers = Version_TS (repository, (char *) NULL, vbranch, (char *) NULL, vfile,
		       1, 0, (List *) NULL, (List *) NULL);
    if (vers->vn_rcs != NULL)
    {
	char xtmpfile[PATH_MAX];
	int different;
	int retcode = 0;

	tmpdir = getenv ("TMPDIR");
	if (tmpdir == NULL || tmpdir[0] == '\0') 
	  tmpdir = "/tmp";

	(void) sprintf (xtmpfile, "%s/cvs-imp%d", tmpdir, getpid());

	/*
	 * The rcs file does have a revision on the vendor branch. Compare
	 * this revision with the import file; if they match exactly, there
	 * is no need to install the new import file as a new revision to the
	 * branch.  Just tag the revision with the new import tags.
	 * 
	 * This is to try to cut down the number of "C" conflict messages for
	 * locally modified import source files.
	 */
#ifdef HAVE_RCS5
	run_setup ("%s%s -q -f -r%s -p -ko", Rcsbin, RCS_CO, vers->vn_rcs);
#else
	run_setup ("%s%s -q -f -r%s -p", Rcsbin, RCS_CO, vers->vn_rcs);
#endif
	run_arg (vers->srcfile->path);
	if ((retcode = run_exec (RUN_TTY, xtmpfile, RUN_TTY,
				 RUN_NORMAL|RUN_REALLY)) != 0)
	{
	    ierrno = errno;
	    fperror (logfp, 0, retcode == -1 ? ierrno : 0,
		     "ERROR: cannot co revision %s of file %s", vers->vn_rcs,
		     vers->srcfile->path);
	    error (0, retcode == -1 ? ierrno : 0,
		   "ERROR: cannot co revision %s of file %s", vers->vn_rcs,
		   vers->srcfile->path);
	    (void) unlink_file (xtmpfile);
	    return (1);
	}
	different = xcmp (xtmpfile, vfile);
	(void) unlink_file (xtmpfile);
	if (!different)
	{
	    int retval = 0;

	    /*
	     * The two files are identical.  Just update the tags, print the
	     * "U", signifying that the file has changed, but needs no
	     * attention, and we're done.
	     */
	    if (add_tags (vers->srcfile->path, vfile, vtag, targc, targv))
		retval = 1;
	    add_log ('U', vfile);
	    freevers_ts (&vers);
	    return (retval);
	}
    }

    /* We may have failed to parse the RCS file; check just in case */
    if (vers->srcfile == NULL ||
	add_rev (message, vers->srcfile->path, vfile, vers->vn_rcs) ||
	add_tags (vers->srcfile->path, vfile, vtag, targc, targv))
    {
	freevers_ts (&vers);
	return (1);
    }

    if (vers->srcfile->branch == NULL || inattic ||
	strcmp (vers->srcfile->branch, vbranch) != 0)
    {
	conflicts++;
	letter = 'C';
    }
    else
	letter = 'U';
    add_log (letter, vfile);

    freevers_ts (&vers);
    return (0);
}

/*
 * Add the revision to the vendor branch
 */
static int
add_rev (message, rcs, vfile, vers)
    char *message;
    char *rcs;
    char *vfile;
    char *vers;
{
    int locked, status, ierrno;
    int retcode = 0;

    if (noexec)
	return (0);

    locked = 0;
    if (vers != NULL)
    {
	run_setup ("%s%s -q -l%s", Rcsbin, RCS, vbranch);
	run_arg (rcs);
	if ((retcode = run_exec (RUN_TTY, DEVNULL, DEVNULL, RUN_NORMAL)) == 0)
	    locked = 1;
	else if (retcode == -1)
	{
	    error (0, errno, "fork failed");
	    return (1);
	}
    }
    if (link_file (vfile, FILE_HOLDER) < 0)
    {
	if (errno == EEXIST)
	{
	    (void) unlink_file (FILE_HOLDER);
	    (void) link_file (vfile, FILE_HOLDER);
	}
	else
	{
	    ierrno = errno;
	    fperror (logfp, 0, ierrno, "ERROR: cannot create link to %s", vfile);
	    error (0, ierrno, "ERROR: cannot create link to %s", vfile);
	    return (1);
	}
    }
    run_setup ("%s%s -q -f -r%s", Rcsbin, RCS_CI, vbranch);
    run_args ("-m%s", message);
    if (use_file_modtime)
	run_arg ("-d");
    run_arg (rcs);
    status = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
    ierrno = errno;
    rename_file (FILE_HOLDER, vfile);
    if (status)
    {
	if (!noexec)
	{
	    fperror (logfp, 0, status == -1 ? ierrno : 0, "ERROR: Check-in of %s failed", rcs);
	    error (0, status == -1 ? ierrno : 0, "ERROR: Check-in of %s failed", rcs);
	}
	if (locked)
	{
	    run_setup ("%s%s -q -u%s", Rcsbin, RCS, vbranch);
	    run_arg (rcs);
	    (void) run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
	}
	return (1);
    }
    return (0);
}

/*
 * Add the vendor branch tag and all the specified import release tags to the
 * RCS file.  The vendor branch tag goes on the branch root (1.1.1) while the
 * vendor release tags go on the newly added leaf of the branch (1.1.1.1,
 * 1.1.1.2, ...).
 */
static int
add_tags (rcs, vfile, vtag, targc, targv)
    char *rcs;
    char *vfile;
    char *vtag;
    int targc;
    char *targv[];
{
    int i, ierrno;
    Vers_TS *vers;
    int retcode = 0;

    if (noexec)
	return (0);

    run_setup ("%s%s -q -N%s:%s", Rcsbin, RCS, vtag, vbranch);
    run_arg (rcs);
    if ((retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL)) != 0)
    {
	ierrno = errno;
	fperror (logfp, 0, retcode == -1 ? ierrno : 0, 
		 "ERROR: Failed to set tag %s in %s", vtag, rcs);
	error (0, retcode == -1 ? ierrno : 0,
	       "ERROR: Failed to set tag %s in %s", vtag, rcs);
	return (1);
    }
    vers = Version_TS (repository, (char *) NULL, vtag, (char *) NULL, vfile,
		       1, 0, (List *) NULL, (List *) NULL);
    for (i = 0; i < targc; i++)
    {
	run_setup ("%s%s -q -N%s:%s", Rcsbin, RCS, targv[i], vers->vn_rcs);
	run_arg (rcs);
	if ((retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL)) != 0)
	{
	    ierrno = errno;
	    fperror (logfp, 0, retcode == -1 ? ierrno : 0, 
		     "WARNING: Couldn't add tag %s to %s", targv[i], rcs);
	    error (0, retcode == -1 ? ierrno : 0,
		   "WARNING: Couldn't add tag %s to %s", targv[i], rcs);
	}
    }
    freevers_ts (&vers);
    return (0);
}

/*
 * Stolen from rcs/src/rcsfnms.c, and adapted/extended.
 */
struct compair
{
    char *suffix, *comlead;
};

struct compair comtable[] =
{

/*
 * comtable pairs each filename suffix with a comment leader. The comment
 * leader is placed before each line generated by the $Log keyword. This
 * table is used to guess the proper comment leader from the working file's
 * suffix during initial ci (see InitAdmin()). Comment leaders are needed for
 * languages without multiline comments; for others they are optional.
 */
    "a", "-- ",				/* Ada		 */
    "ada", "-- ",
    "adb", "-- ",
    "asm", ";; ",			/* assembler (MS-DOS) */
    "ads", "-- ",			/* Ada		 */
    "bat", ":: ",			/* batch (MS-DOS) */
    "body", "-- ",			/* Ada		 */
    "c", " * ",				/* C		 */
    "c++", "// ",			/* C++ in all its infinite guises */
    "cc", "// ",
    "cpp", "// ",
    "cxx", "// ",
    "cl", ";;; ",			/* Common Lisp	 */
    "cmd", ":: ",			/* command (OS/2) */
    "cmf", "c ",			/* CM Fortran	 */
    "cs", " * ",			/* C*		 */
    "csh", "# ",			/* shell	 */
    "e", "# ",				/* efl		 */
    "epsf", "% ",			/* encapsulated postscript */
    "epsi", "% ",			/* encapsulated postscript */
    "el", "; ",				/* Emacs Lisp	 */
    "f", "c ",				/* Fortran	 */
    "for", "c ",
    "h", " * ",				/* C-header	 */
    "hh", "// ",			/* C++ header	 */
    "hpp", "// ",
    "hxx", "// ",
    "in", "# ",				/* for Makefile.in */
    "l", " * ",				/* lex (conflict between lex and
					 * franzlisp) */
    "mac", ";; ",			/* macro (DEC-10, MS-DOS, PDP-11,
					 * VMS, etc) */
    "me", ".\\\" ",			/* me-macros	t/nroff	 */
    "ml", "; ",				/* mocklisp	 */
    "mm", ".\\\" ",			/* mm-macros	t/nroff	 */
    "ms", ".\\\" ",			/* ms-macros	t/nroff	 */
    "man", ".\\\" ",			/* man-macros	t/nroff	 */
    "1", ".\\\" ",			/* feeble attempt at man pages... */
    "2", ".\\\" ",
    "3", ".\\\" ",
    "4", ".\\\" ",
    "5", ".\\\" ",
    "6", ".\\\" ",
    "7", ".\\\" ",
    "8", ".\\\" ",
    "9", ".\\\" ",
    "p", " * ",				/* pascal	 */
    "pas", " * ",
    "pl", "# ",				/* perl	(conflict with Prolog) */
    "ps", "% ",				/* postscript	 */
    "r", "# ",				/* ratfor	 */
    "red", "% ",			/* psl/rlisp	 */
#ifdef sparc
    "s", "! ",				/* assembler	 */
#endif
#ifdef mc68000
    "s", "| ",				/* assembler	 */
#endif
#ifdef pdp11
    "s", "/ ",				/* assembler	 */
#endif
#ifdef vax
    "s", "# ",				/* assembler	 */
#endif
#ifdef __ksr__
    "s", "# ",				/* assembler	 */
    "S", "# ",				/* Macro assembler */
#endif
    "sh", "# ",				/* shell	 */
    "sl", "% ",				/* psl		 */
    "spec", "-- ",			/* Ada		 */
    "tex", "% ",			/* tex		 */
    "y", " * ",				/* yacc		 */
    "ye", " * ",			/* yacc-efl	 */
    "yr", " * ",			/* yacc-ratfor	 */
    "", "# ",				/* default for empty suffix	 */
    NULL, "# "				/* default for unknown suffix;	 */
/* must always be last		 */
};

static char *
get_comment (user)
    char *user;
{
    char *cp, *suffix;
    char suffix_path[PATH_MAX];
    int i;

    cp = strrchr (user, '.');
    if (cp != NULL)
    {
	cp++;

	/*
	 * Convert to lower-case, since we are not concerned about the
	 * case-ness of the suffix.
	 */
	(void) strcpy (suffix_path, cp);
	for (cp = suffix_path; *cp; cp++)
	    if (isupper (*cp))
		*cp = tolower (*cp);
	suffix = suffix_path;
    }
    else
	suffix = "";			/* will use the default */
    for (i = 0;; i++)
    {
	if (comtable[i].suffix == NULL)	/* default */
	    return (comtable[i].comlead);
	if (strcmp (suffix, comtable[i].suffix) == 0)
	    return (comtable[i].comlead);
    }
}

static int
add_rcs_file (message, rcs, user, vtag, targc, targv)
    char *message;
    char *rcs;
    char *user;
    char *vtag;
    int targc;
    char *targv[];
{
    FILE *fprcs, *fpuser;
    struct stat sb;
    struct tm *ftm;
    time_t now;
    char altdate1[50];
#ifndef HAVE_RCS5
    char altdate2[50];
#endif
    char *author, *buf;
    int i, mode, ierrno, err = 0;

    if (noexec)
	return (0);

    /* open the user file first, before we change the RCS files... */
    fpuser = fopen (user, "r");
    if (fpuser == NULL) {
	/* not fatal, continue the import */
	fperror (logfp, 0, errno, "ERROR: cannot read file %s", user);
	error (0, errno, "ERROR: cannot read file %s", user);
	goto read_error;
    }
    fprcs = fopen (rcs, "w+");
    if (fprcs == NULL) {
	ierrno = errno;
	goto write_error_noclose;
    }

    /*
     * putadmin()
     */
    if (fprintf (fprcs, "head     %s;\n", vhead) == EOF ||
	fprintf (fprcs, "branch   %s;\n", vbranch) == EOF ||
	fprintf (fprcs, "access   ;\n") == EOF ||
	fprintf (fprcs, "symbols  ") == EOF)
    {
	goto write_error;
    }

    for (i = targc - 1; i >= 0; i--)	/* RCS writes the symbols backwards */
	if (fprintf (fprcs, "%s:%s.1 ", targv[i], vbranch) == EOF)
	    goto write_error;

    if (fprintf (fprcs, "%s:%s;\n", vtag, vbranch) == EOF ||
	fprintf (fprcs, "locks    ; strict;\n") == EOF ||
	/* XXX - make sure @@ processing works in the RCS file */
	fprintf (fprcs, "comment  @%s@;\n", get_comment (user)) == EOF)
    {
	goto write_error;
    }

    if (keyword_opt != NULL)
      if (fprintf (fprcs, "expand   @%s@;\n", keyword_opt) == EOF)
	{
	  goto write_error;
	}

    if (fprintf (fprcs, "\n") == EOF)
      goto write_error;

    /*
     * puttree()
     */
    if (fstat (fileno (fpuser), &sb) < 0)
	error (1, errno, "cannot fstat %s", user);
    if (use_file_modtime)
	now = sb.st_mtime;
    else
	(void) time (&now);
#ifdef HAVE_RCS5
    ftm = gmtime (&now);
#else
    ftm = localtime (&now);
#endif
    (void) sprintf (altdate1, DATEFORM,
		    ftm->tm_year + (ftm->tm_year < 100 ? 0 : 1900),
		    ftm->tm_mon + 1, ftm->tm_mday, ftm->tm_hour,
		    ftm->tm_min, ftm->tm_sec);
#ifdef HAVE_RCS5
#define	altdate2 altdate1
#else
    /*
     * If you don't have RCS V5 or later, you need to lie about the ci
     * time, since RCS V4 and earlier insist that the times differ.
     */
    now++;
    ftm = localtime (&now);
    (void) sprintf (altdate2, DATEFORM,
		    ftm->tm_year + (ftm->tm_year < 100 ? 0 : 1900),
		    ftm->tm_mon + 1, ftm->tm_mday, ftm->tm_hour,
		    ftm->tm_min, ftm->tm_sec);
#endif
    author = getcaller ();

    if (fprintf (fprcs, "\n%s\n", vhead) == EOF ||
	fprintf (fprcs, "date     %s;  author %s;  state Exp;\n",
		 altdate1, author) == EOF ||
	fprintf (fprcs, "branches %s.1;\n", vbranch) == EOF ||
	fprintf (fprcs, "next     ;\n") == EOF ||
	fprintf (fprcs, "\n%s.1\n", vbranch) == EOF ||
	fprintf (fprcs, "date     %s;  author %s;  state Exp;\n",
		 altdate2, author) == EOF ||
	fprintf (fprcs, "branches ;\n") == EOF ||
	fprintf (fprcs, "next     ;\n\n") == EOF ||
	/*
	 * putdesc()
	 */
	fprintf (fprcs, "\ndesc\n") == EOF ||
	fprintf (fprcs, "@@\n\n\n") == EOF ||
	/*
	 * putdelta()
	 */
	fprintf (fprcs, "\n%s\n", vhead) == EOF ||
	fprintf (fprcs, "log\n") == EOF ||
	fprintf (fprcs, "@Initial revision\n@\n") == EOF ||
	fprintf (fprcs, "text\n@") == EOF)
    {
	goto write_error;
    }

    if (sb.st_size > 0)
    {
	off_t size;

	size = sb.st_size;
	buf = xmalloc ((int) size);
	if (fread (buf, (int) size, 1, fpuser) != 1)
	    error (1, errno, "cannot read file %s for copying", user);
	if (expand_at_signs (buf, size, fprcs) == EOF)
	{
	    free (buf);
	    goto write_error;
	}
	free (buf);
    }
    if (fprintf (fprcs, "@\n\n") == EOF ||
	fprintf (fprcs, "\n%s.1\n", vbranch) == EOF ||
	fprintf (fprcs, "log\n@") == EOF ||
	expand_at_signs (message, (off_t) strlen (message), fprcs) == EOF ||
	fprintf (fprcs, "@\ntext\n") == EOF ||
	fprintf (fprcs, "@@\n") == EOF)
    {
	goto write_error;
    }
    if (fclose (fprcs) == EOF)
    {
	ierrno = errno;
	goto write_error_noclose;
    }
    (void) fclose (fpuser);

    /*
     * Fix the modes on the RCS files.  They must maintain the same modes as
     * the original user file, except that all write permissions must be
     * turned off.
     */
    mode = sb.st_mode & ~(S_IWRITE | S_IWGRP | S_IWOTH);
    if (chmod (rcs, mode) < 0)
    {
	ierrno = errno;
	fperror (logfp, 0, ierrno,
		 "WARNING: cannot change mode of file %s", rcs);
	error (0, ierrno, "WARNING: cannot change mode of file %s", rcs);
	err++;
    }
    return (err);

write_error:
    ierrno = errno;
    (void) fclose (fprcs);
write_error_noclose:
    (void) fclose (fpuser);
    fperror (logfp, 0, ierrno, "ERROR: cannot write file %s", rcs);
    error (0, ierrno, "ERROR: cannot write file %s", rcs);
    if (ierrno == ENOSPC)
    {
	(void) unlink (rcs);
	fperror (logfp, 0, 0, "ERROR: out of space - aborting");
	error (1, 0, "ERROR: out of space - aborting");
    }
read_error:
    return (err + 1);
}

/*
 * Sigh..  need to expand @ signs into double @ signs
 */
static int
expand_at_signs (buf, size, fp)
    char *buf;
    off_t size;
    FILE *fp;
{
    char *cp, *end;

    for (cp = buf, end = buf + size; cp < end; cp++)
    {
	if (*cp == '@')
	    (void) putc ('@', fp);
	if (putc (*cp, fp) == EOF)
	    return (EOF);
    }
    return (1);
}

/*
 * Write an update message to (potentially) the screen and the log file.
 */
static void
add_log (ch, fname)
    int ch;
    char *fname;
{
    if (!really_quiet)			/* write to terminal */
    {
	if (repos_len)
	    (void) printf ("%c %s/%s\n", ch, repository + repos_len + 1, fname);
	else if (repository[0])
	    (void) printf ("%c %s/%s\n", ch, repository, fname);
	else
	    (void) printf ("%c %s\n", ch, fname);
    }

    if (repos_len)			/* write to logfile */
	(void) fprintf (logfp, "%c %s/%s\n", ch,
			repository + repos_len + 1, fname);
    else if (repository[0])
	(void) fprintf (logfp, "%c %s/%s\n", ch, repository, fname);
    else
	(void) fprintf (logfp, "%c %s\n", ch, fname);
}

/*
 * This is the recursive function that walks the argument directory looking
 * for sub-directories that have CVS administration files in them and updates
 * them recursively.
 * 
 * Note that we do not follow symbolic links here, which is a feature!
 */
static int
import_descend_dir (message, dir, vtag, targc, targv)
    char *message;
    char *dir;
    char *vtag;
    int targc;
    char *targv[];
{
    char cwd[PATH_MAX];
    char *cp;
    int ierrno, err;

    if (islink (dir))
	return (0);
    if (getwd (cwd) == NULL)
    {
	fperror (logfp, 0, 0, "ERROR: cannot get working directory: %s", cwd);
	error (0, 0, "ERROR: cannot get working directory: %s", cwd);
	return (1);
    }
    if (repository[0] == '\0')
	(void) strcpy (repository, dir);
    else
    {
	(void) strcat (repository, "/");
	(void) strcat (repository, dir);
    }
    if (!quiet)
	error (0, 0, "Importing %s", repository);
    if (chdir (dir) < 0)
    {
	ierrno = errno;
	fperror (logfp, 0, ierrno, "ERROR: cannot chdir to %s", repository);
	error (0, ierrno, "ERROR: cannot chdir to %s", repository);
	err = 1;
	goto out;
    }
    if (!isdir (repository))
    {
	if (isfile (repository))
	{
	    fperror (logfp, 0, 0, "ERROR: %s is a file, should be a directory!",
		     repository);
	    error (0, 0, "ERROR: %s is a file, should be a directory!",
		   repository);
	    err = 1;
	    goto out;
	}
	if (noexec == 0 && mkdir (repository, 0777) < 0)
	{
	    ierrno = errno;
	    fperror (logfp, 0, ierrno,
		     "ERROR: cannot mkdir %s -- not added", repository);
	    error (0, ierrno,
		   "ERROR: cannot mkdir %s -- not added", repository);
	    err = 1;
	    goto out;
	}
    }
    err = import_descend (message, vtag, targc, targv);
  out:
    if ((cp = strrchr (repository, '/')) != NULL)
	*cp = '\0';
    else
	repository[0] = '\0';
    if (chdir (cwd) < 0)
	error (1, errno, "cannot chdir to %s", cwd);
    return (err);
}

/* the following code is taken from code in rcs/src/rcssyn.c, and returns a
 * positive value if 'expstring' contains a valid RCS expansion token for
 * the -k option.  If an invalid expansion is named, then return -1.
 */

char const *const expand_names[] = {
	/* These must agree with *_EXPAND in rcs/src/rcsbase.h.  */
	"kv","kvl","k","v","o",
	0
};

static int
str2expmode(s)
     char const *s;
/* Yield expand mode corresponding to S, or -1 if bad.  */
{
	return strn2expmode(s, strlen(s));
}

static int
strn2expmode(s, n)
     char const *s;
     size_t n;
{
  char const *const *p;
  
  for (p = expand_names;  *p;  ++p)
    if (memcmp(*p,s,n) == 0  &&  !(*p)[n])
      return p - expand_names;
  return -1;
}

