/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
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
#include "savecwd.h"
#include <assert.h>

static char *get_comment PROTO((char *user));
static int add_rev PROTO((char *message, RCSNode *rcs, char *vfile,
			  char *vers));
static int add_tags PROTO((RCSNode *rcs, char *vfile, char *vtag, int targc,
		     char *targv[]));
static int import_descend PROTO((char *message, char *vtag, int targc, char *targv[]));
static int import_descend_dir PROTO((char *message, char *dir, char *vtag,
			       int targc, char *targv[]));
static int process_import_file PROTO((char *message, char *vfile, char *vtag,
				int targc, char *targv[]));
static int update_rcs_file PROTO((char *message, char *vfile, char *vtag, int targc,
			    char *targv[], int inattic));
static void add_log PROTO((int ch, char *fname));

static int repos_len;
static char *vhead;
static char *vbranch;
static FILE *logfp;
static char *repository;
static int conflicts;
static int use_file_modtime;
static char *keyword_opt = NULL;

static const char *const import_usage[] =
{
    "Usage: %s %s [-d] [-k subst] [-I ign] [-m msg] [-b branch]\n",
    "    [-W spec] repository vendor-tag release-tags...\n",
    "\t-d\tUse the file's modification time as the time of import.\n",
    "\t-k sub\tSet default RCS keyword substitution mode.\n",
    "\t-I ign\tMore files to ignore (! to reset).\n",
    "\t-b bra\tVendor branch id.\n",
    "\t-m msg\tLog message.\n",
    "\t-W spec\tWrappers specification line.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int
import (argc, argv)
    int argc;
    char **argv;
{
    char *message = NULL;
    char *tmpfile;
    char *cp;
    int i, c, msglen, err;
    List *ulist;
    Node *p;
    struct logfile_info *li;

    if (argc == -1)
	usage (import_usage);

    ign_setup ();
    wrap_setup ();

    vbranch = xstrdup (CVSBRANCH);
    optind = 0;
    while ((c = getopt (argc, argv, "+Qqdb:m:I:k:W:")) != -1)
    {
	switch (c)
	{
	    case 'Q':
	    case 'q':
#ifdef SERVER_SUPPORT
		/* The CVS 1.5 client sends these options (in addition to
		   Global_option requests), so we must ignore them.  */
		if (!server_active)
#endif
		    error (1, 0,
			   "-q or -Q must be specified before \"%s\"",
			   command_name);
		break;
	    case 'd':
		use_file_modtime = 1;
		break;
	    case 'b':
		free (vbranch);
		vbranch = xstrdup (optarg);
		break;
	    case 'm':
#ifdef FORCE_USE_EDITOR
		use_editor = 1;
#else
		use_editor = 0;
#endif
		message = xstrdup(optarg);
		break;
	    case 'I':
		ign_add (optarg, 0);
		break;
            case 'k':
		/* RCS_check_kflag returns strings of the form -kxx.  We
		   only use it for validation, so we can free the value
		   as soon as it is returned. */
		free (RCS_check_kflag (optarg));
		keyword_opt = optarg;
		break;
	    case 'W':
		wrap_add (optarg, 0);
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
    {
	int j;

	RCS_check_tag (argv[i]);
	for (j = 1; j < i; j++)
	    if (strcmp (argv[j], argv[i]) == 0)
		error (1, 0, "tag `%s' was specified more than once", argv[i]);
    }

    /* XXX - this should be a module, not just a pathname */
    if (! isabsolute (argv[0]))
    {
	if (CVSroot_directory == NULL)
	{
	    error (0, 0, "missing CVSROOT environment variable\n");
	    error (1, 0, "Set it or specify the '-d' option to %s.",
		   program_name);
	}
	repository = xmalloc (strlen (CVSroot_directory) + strlen (argv[0])
			      + 10);
	(void) sprintf (repository, "%s/%s", CVSroot_directory, argv[0]);
	repos_len = strlen (CVSroot_directory);
    }
    else
    {
	repository = xmalloc (strlen (argv[0]) + 5);
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
    vhead = xstrdup (vbranch);
    cp = strrchr (vhead, '.');
    *cp = '\0';

#ifdef CLIENT_SUPPORT
    if (client_active)
    {
	/* For rationale behind calling start_server before do_editor, see
	   commit.c  */
	start_server ();
    }
#endif

    if (use_editor)
    {
	do_editor ((char *) NULL, &message, repository,
		   (List *) NULL);
    }
    do_verify (message, repository);
    msglen = message == NULL ? 0 : strlen (message);
    if (msglen == 0 || message[msglen - 1] != '\n')
    {
	char *nm = xmalloc (msglen + 2);
	*nm = '\0';
	if (message != NULL)
	{
	    (void) strcpy (nm, message);
	    free (message);
	}
	(void) strcat (nm + msglen, "\n");
	message = nm;
    }

#ifdef CLIENT_SUPPORT
    if (client_active)
    {
	int err;

	if (use_file_modtime)
	    send_arg("-d");

	if (vbranch[0] != '\0')
	    option_with_arg ("-b", vbranch);
	if (message)
	    option_with_arg ("-m", message);
	if (keyword_opt != NULL)
	    option_with_arg ("-k", keyword_opt);
	/* The only ignore processing which takes place on the server side
	   is the CVSROOT/cvsignore file.  But if the user specified -I !,
	   the documented behavior is to not process said file.  */
	if (ign_inhibit_server)
	{
	    send_arg ("-I");
	    send_arg ("!");
	}
	wrap_send ();

	{
	    int i;
	    for (i = 0; i < argc; ++i)
		send_arg (argv[i]);
	}

	logfp = stdin;
	client_import_setup (repository);
	err = import_descend (message, argv[1], argc - 2, argv + 2);
	client_import_done ();
	send_to_server ("import\012", 0);
	err += get_responses_and_close ();
	return err;
    }
#endif

    /*
     * Make all newly created directories writable.  Should really use a more
     * sophisticated security mechanism here.
     */
    (void) umask (cvsumask);
    make_directories (repository);

    /* Create the logfile that will be logged upon completion */
    tmpfile = cvs_temp_name ();
    if ((logfp = CVS_FOPEN (tmpfile, "w+")) == NULL)
	error (1, errno, "cannot create temporary file `%s'", tmpfile);
    /* On systems where we can unlink an open file, do so, so it will go
       away no matter how we exit.  FIXME-maybe: Should be checking for
       errors but I'm not sure which error(s) we get if we are on a system
       where one can't unlink open files.  */
    (void) CVS_UNLINK (tmpfile);
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
	    char buf[80];
	    sprintf (buf, "\n%d conflicts created by this import.\n",
		     conflicts);
	    cvs_output (buf, 0);
	    cvs_output ("Use the following command to help the merge:\n\n",
			0);
	    cvs_output ("\t", 1);
	    cvs_output (program_name, 0);
	    cvs_output (" checkout -j", 0);
	    cvs_output (argv[1], 0);
	    cvs_output (":yesterday -j", 0);
	    cvs_output (argv[1], 0);
	    cvs_output (" ", 1);
	    cvs_output (argv[0], 0);
	    cvs_output ("\n\n", 0);
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
	    cvs_output ("\nNo conflicts created by this import\n\n", 0);
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
    li = (struct logfile_info *) xmalloc (sizeof (struct logfile_info));
    li->type = T_TITLE;
    li->tag = xstrdup (vbranch);
    li->rev_old = li->rev_new = NULL;
    p->data = (char *) li;
    (void) addnode (ulist, p);
    Update_Logfile (repository, message, logfp, ulist);
    dellist (&ulist);
    if (fclose (logfp) < 0)
	error (0, errno, "error closing %s", tmpfile);

    /* Make sure the temporary file goes away, even on systems that don't let
       you delete a file that's in use.  */
    if (CVS_UNLINK (tmpfile) < 0 && !existence_error (errno))
	error (0, errno, "cannot remove %s", tmpfile);
    free (tmpfile);

    if (message)
	free (message);
    free (repository);
    free (vbranch);
    free (vhead);

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
    List *dirlist = NULL;

    /* first, load up any per-directory ignore lists */
    ign_add_file (CVSDOTIGNORE, 1);
    wrap_add_file (CVSDOTWRAPPER, 1);

    if ((dirp = CVS_OPENDIR (".")) == NULL)
    {
	err++;
    }
    else
    {
	while ((dp = readdir (dirp)) != NULL)
	{
	    if (strcmp (dp->d_name, ".") == 0 || strcmp (dp->d_name, "..") == 0)
		continue;
#ifdef SERVER_SUPPORT
	    /* CVS directories are created in the temp directory by
	       server.c because it doesn't special-case import.  So
	       don't print a message about them, regardless of -I!.  */
	    if (server_active && strcmp (dp->d_name, CVSADM) == 0)
		continue;
#endif
	    if (ign_name (dp->d_name))
	    {
		add_log ('I', dp->d_name);
		continue;
	    }

	    if (
#ifdef DT_DIR
		(dp->d_type == DT_DIR
		 || (dp->d_type == DT_UNKNOWN && isdir (dp->d_name)))
#else
		isdir (dp->d_name)
#endif
		&& !wrap_name_has (dp->d_name, WRAP_TOCVS)
		)
	    {
		Node *n;

		if (dirlist == NULL)
		    dirlist = getlist();

		n = getnode();
		n->key = xstrdup (dp->d_name);
		addnode(dirlist, n);
	    }
	    else if (
#ifdef DT_DIR
		dp->d_type == DT_LNK || dp->d_type == DT_UNKNOWN &&
#endif
		islink (dp->d_name))
	    {
		add_log ('L', dp->d_name);
		err++;
	    }
	    else
	    {
#ifdef CLIENT_SUPPORT
		if (client_active)
		    err += client_process_import_file (message, dp->d_name,
                                                       vtag, targc, targv,
                                                       repository,
                                                       keyword_opt != NULL &&
                                                       keyword_opt[0] == 'b');
		else
#endif
		    err += process_import_file (message, dp->d_name,
						vtag, targc, targv);
	    }
	}
	(void) closedir (dirp);
    }

    if (dirlist != NULL)
    {
	Node *head, *p;

	head = dirlist->list;
	for (p = head->next; p != head; p = p->next)
	{
	    err += import_descend_dir (message, p->key, vtag, targc, targv);
	}

	dellist(&dirlist);
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
    char *rcs;
    int inattic = 0;

    rcs = xmalloc (strlen (repository) + strlen (vfile) + sizeof (RCSEXT)
		   + 5);
    (void) sprintf (rcs, "%s/%s%s", repository, vfile, RCSEXT);
    if (!isfile (rcs))
    {
	char *attic_name;

	attic_name = xmalloc (strlen (repository) + strlen (vfile) +
			      sizeof (CVSATTIC) + sizeof (RCSEXT) + 10);
	(void) sprintf (attic_name, "%s/%s/%s%s", repository, CVSATTIC,
			vfile, RCSEXT);
	if (!isfile (attic_name))
	{
	    int retval;
	    char *free_opt = NULL;
	    char *our_opt = keyword_opt;

	    free (attic_name);
	    /*
	     * A new import source file; it doesn't exist as a ,v within the
	     * repository nor in the Attic -- create it anew.
	     */
	    add_log ('N', vfile);

#ifdef SERVER_SUPPORT
	    /* The most reliable information on whether the file is binary
	       is what the client told us.  That is because if the client had
	       the wrong idea about binaryness, it corrupted the file, so
	       we might as well believe the client.  */
	    if (server_active)
	    {
		Node *node;
		List *entries;

		/* Reading all the entries for each file is fairly silly, and
		   probably slow.  But I am too lazy at the moment to do
		   anything else.  */
		entries = Entries_Open (0, NULL);
		node = findnode_fn (entries, vfile);
		if (node != NULL)
		{
		    Entnode *entdata = (Entnode *) node->data;
		    if (entdata->type == ENT_FILE)
		    {
			assert (entdata->options[0] == '-'
				&& entdata->options[1] == 'k');
			our_opt = xstrdup (entdata->options + 2);
			free_opt = our_opt;
		    }
		}
		Entries_Close (entries);
	    }
#endif

	    retval = add_rcs_file (message, rcs, vfile, vhead, our_opt,
				   vbranch, vtag, targc, targv,
				   NULL, 0, logfp);
	    if (free_opt != NULL)
		free (free_opt);
	    free (rcs);
	    return retval;
	}
	free (attic_name);
	inattic = 1;
    }

    free (rcs);
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
    char *tocvsPath;
    struct file_info finfo;

    memset (&finfo, 0, sizeof finfo);
    finfo.file = vfile;
    /* Not used, so don't worry about it.  */
    finfo.update_dir = NULL;
    finfo.fullname = finfo.file;
    finfo.repository = repository;
    finfo.entries = NULL;
    finfo.rcs = NULL;
    vers = Version_TS (&finfo, (char *) NULL, vbranch, (char *) NULL,
		       1, 0);
    if (vers->vn_rcs != NULL
	&& !RCS_isdead(vers->srcfile, vers->vn_rcs))
    {
	int different;

	/*
	 * The rcs file does have a revision on the vendor branch. Compare
	 * this revision with the import file; if they match exactly, there
	 * is no need to install the new import file as a new revision to the
	 * branch.  Just tag the revision with the new import tags.
	 * 
	 * This is to try to cut down the number of "C" conflict messages for
	 * locally modified import source files.
	 */
	tocvsPath = wrap_tocvs_process_file (vfile);
	/* FIXME: Why don't we pass tocvsPath to RCS_cmp_file if it is
           not NULL?  */
	different = RCS_cmp_file (vers->srcfile, vers->vn_rcs, "-ko", vfile);
	if (tocvsPath)
	    if (unlink_file_dir (tocvsPath) < 0)
		error (0, errno, "cannot remove %s", tocvsPath);

	if (!different)
	{
	    int retval = 0;

	    /*
	     * The two files are identical.  Just update the tags, print the
	     * "U", signifying that the file has changed, but needs no
	     * attention, and we're done.
	     */
	    if (add_tags (vers->srcfile, vfile, vtag, targc, targv))
		retval = 1;
	    add_log ('U', vfile);
	    freevers_ts (&vers);
	    return (retval);
	}
    }

    /* We may have failed to parse the RCS file; check just in case */
    if (vers->srcfile == NULL ||
	add_rev (message, vers->srcfile, vfile, vers->vn_rcs) ||
	add_tags (vers->srcfile, vfile, vtag, targc, targv))
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
    RCSNode *rcs;
    char *vfile;
    char *vers;
{
    int locked, status, ierrno;
    char *tocvsPath;

    if (noexec)
	return (0);

    locked = 0;
    if (vers != NULL)
    {
	/* Before RCS_lock existed, we were directing stdout, as well as
	   stderr, from the RCS command, to DEVNULL.  I wouldn't guess that
	   was necessary, but I don't know for sure.  */
	/* Earlier versions of this function printed a `fork failed' error
	   when RCS_lock returned an error code.  That's not appropriate
	   now that RCS_lock is librarified, but should the error text be
	   preserved? */
	if (RCS_lock (rcs, vbranch, 1) != 0)
	    return 1;
	locked = 1;
	RCS_rewrite (rcs, NULL, NULL);
    }
    tocvsPath = wrap_tocvs_process_file (vfile);

    status = RCS_checkin (rcs, tocvsPath == NULL ? vfile : tocvsPath,
			  message, vbranch,
			  (RCS_FLAGS_QUIET | RCS_FLAGS_KEEPFILE
			   | (use_file_modtime ? RCS_FLAGS_MODTIME : 0)));
    ierrno = errno;

    if ((tocvsPath != NULL) && (unlink_file_dir (tocvsPath) < 0))
	error (0, errno, "cannot remove %s", tocvsPath);

    if (status)
    {
	if (!noexec)
	{
	    fperror (logfp, 0, status == -1 ? ierrno : 0,
		     "ERROR: Check-in of %s failed", rcs->path);
	    error (0, status == -1 ? ierrno : 0,
		   "ERROR: Check-in of %s failed", rcs->path);
	}
	if (locked)
	{
	    (void) RCS_unlock(rcs, vbranch, 0);
	    RCS_rewrite (rcs, NULL, NULL);
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
    RCSNode *rcs;
    char *vfile;
    char *vtag;
    int targc;
    char *targv[];
{
    int i, ierrno;
    Vers_TS *vers;
    int retcode = 0;
    struct file_info finfo;

    if (noexec)
	return (0);

    if ((retcode = RCS_settag(rcs, vtag, vbranch)) != 0)
    {
	ierrno = errno;
	fperror (logfp, 0, retcode == -1 ? ierrno : 0,
		 "ERROR: Failed to set tag %s in %s", vtag, rcs->path);
	error (0, retcode == -1 ? ierrno : 0,
	       "ERROR: Failed to set tag %s in %s", vtag, rcs->path);
	return (1);
    }
    RCS_rewrite (rcs, NULL, NULL);

    memset (&finfo, 0, sizeof finfo);
    finfo.file = vfile;
    /* Not used, so don't worry about it.  */
    finfo.update_dir = NULL;
    finfo.fullname = finfo.file;
    finfo.repository = repository;
    finfo.entries = NULL;
    finfo.rcs = NULL;
    vers = Version_TS (&finfo, NULL, vtag, NULL, 1, 0);
    for (i = 0; i < targc; i++)
    {
	if ((retcode = RCS_settag (rcs, targv[i], vers->vn_rcs)) == 0)
	    RCS_rewrite (rcs, NULL, NULL);
	else
	{
	    ierrno = errno;
	    fperror (logfp, 0, retcode == -1 ? ierrno : 0,
		     "WARNING: Couldn't add tag %s to %s", targv[i],
		     rcs->path);
	    error (0, retcode == -1 ? ierrno : 0,
		   "WARNING: Couldn't add tag %s to %s", targv[i],
		   rcs->path);
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

static const struct compair comtable[] =
{

/*
 * comtable pairs each filename suffix with a comment leader. The comment
 * leader is placed before each line generated by the $Log keyword. This
 * table is used to guess the proper comment leader from the working file's
 * suffix during initial ci (see InitAdmin()). Comment leaders are needed for
 * languages without multiline comments; for others they are optional.
 *
 * I believe that the comment leader is unused if you are using RCS 5.7, which
 * decides what leader to use based on the text surrounding the $Log keyword
 * rather than a specified comment leader.
 */
    {"a", "-- "},			/* Ada		 */
    {"ada", "-- "},
    {"adb", "-- "},
    {"asm", ";; "},			/* assembler (MS-DOS) */
    {"ads", "-- "},			/* Ada		 */
    {"bas", "' "},    			/* Visual Basic code */
    {"bat", ":: "},			/* batch (MS-DOS) */
    {"body", "-- "},			/* Ada		 */
    {"c", " * "},			/* C		 */
    {"c++", "// "},			/* C++ in all its infinite guises */
    {"cc", "// "},
    {"cpp", "// "},
    {"cxx", "// "},
    {"m", "// "},			/* Objective-C */
    {"cl", ";;; "},			/* Common Lisp	 */
    {"cmd", ":: "},			/* command (OS/2) */
    {"cmf", "c "},			/* CM Fortran	 */
    {"cs", " * "},			/* C*		 */
    {"csh", "# "},			/* shell	 */
    {"dlg", " * "},   			/* MS Windows dialog file */
    {"e", "# "},			/* efl		 */
    {"epsf", "% "},			/* encapsulated postscript */
    {"epsi", "% "},			/* encapsulated postscript */
    {"el", "; "},			/* Emacs Lisp	 */
    {"f", "c "},			/* Fortran	 */
    {"for", "c "},
    {"frm", "' "},    			/* Visual Basic form */
    {"h", " * "},			/* C-header	 */
    {"hh", "// "},			/* C++ header	 */
    {"hpp", "// "},
    {"hxx", "// "},
    {"in", "# "},			/* for Makefile.in */
    {"l", " * "},			/* lex (conflict between lex and
					 * franzlisp) */
    {"mac", ";; "},			/* macro (DEC-10, MS-DOS, PDP-11,
					 * VMS, etc) */
    {"mak", "# "},    			/* makefile, e.g. Visual C++ */
    {"me", ".\\\" "},			/* me-macros	t/nroff	 */
    {"ml", "; "},			/* mocklisp	 */
    {"mm", ".\\\" "},			/* mm-macros	t/nroff	 */
    {"ms", ".\\\" "},			/* ms-macros	t/nroff	 */
    {"man", ".\\\" "},			/* man-macros	t/nroff	 */
    {"1", ".\\\" "},			/* feeble attempt at man pages... */
    {"2", ".\\\" "},
    {"3", ".\\\" "},
    {"4", ".\\\" "},
    {"5", ".\\\" "},
    {"6", ".\\\" "},
    {"7", ".\\\" "},
    {"8", ".\\\" "},
    {"9", ".\\\" "},
    {"p", " * "},			/* pascal	 */
    {"pas", " * "},
    {"pl", "# "},			/* perl	(conflict with Prolog) */
    {"ps", "% "},			/* postscript	 */
    {"psw", "% "},			/* postscript wrap */
    {"pswm", "% "},			/* postscript wrap */
    {"r", "# "},			/* ratfor	 */
    {"rc", " * "},			/* Microsoft Windows resource file */
    {"red", "% "},			/* psl/rlisp	 */
#ifdef sparc
    {"s", "! "},			/* assembler	 */
#endif
#ifdef mc68000
    {"s", "| "},			/* assembler	 */
#endif
#ifdef pdp11
    {"s", "/ "},			/* assembler	 */
#endif
#ifdef vax
    {"s", "# "},			/* assembler	 */
#endif
#ifdef __ksr__
    {"s", "# "},			/* assembler	 */
    {"S", "# "},			/* Macro assembler */
#endif
    {"sh", "# "},			/* shell	 */
    {"sl", "% "},			/* psl		 */
    {"spec", "-- "},			/* Ada		 */
    {"tex", "% "},			/* tex		 */
    {"y", " * "},			/* yacc		 */
    {"ye", " * "},			/* yacc-efl	 */
    {"yr", " * "},			/* yacc-ratfor	 */
    {"", "# "},				/* default for empty suffix	 */
    {NULL, "# "}			/* default for unknown suffix;	 */
/* must always be last		 */
};

static char *
get_comment (user)
    char *user;
{
    char *cp, *suffix;
    char *suffix_path;
    int i;
    char *retval;

    suffix_path = xmalloc (strlen (user) + 5);
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
	if (comtable[i].suffix == NULL)
	{
	    /* Default.  Note we'll always hit this case before we
	       ever return NULL.  */
	    retval = comtable[i].comlead;
	    break;
	}
	if (strcmp (suffix, comtable[i].suffix) == 0)
	{
	    retval = comtable[i].comlead;
	    break;
	}
    }
    free (suffix_path);
    return retval;
}

/* Create a new RCS file from scratch.

   This probably should be moved to rcs.c now that it is called from
   places outside import.c.

   Return value is 0 for success, or nonzero for failure (in which
   case an error message will have already been printed).  */
int
add_rcs_file (message, rcs, user, add_vhead, key_opt,
	      add_vbranch, vtag, targc, targv,
	      desctext, desclen, add_logfp)
    /* Log message for the addition.  Not used if add_vhead == NULL.  */
    char *message;
    /* Filename of the RCS file to create.  */
    char *rcs;
    /* Filename of the file to serve as the contents of the initial
       revision.  Even if add_vhead is NULL, we use this to determine
       the modes to give the new RCS file.  */
    char *user;

    /* Revision number of head that we are adding.  Normally 1.1 but
       could be another revision as long as ADD_VBRANCH is a branch
       from it.  If NULL, then just add an empty file without any
       revisions (similar to the one created by "rcs -i").  */
    char *add_vhead;

    /* Keyword expansion mode, e.g., "b" for binary.  NULL means the
       default behavior.  */
    char *key_opt;

    /* Vendor branch to import to, or NULL if none.  If non-NULL, then
       vtag should also be non-NULL.  */
    char *add_vbranch;
    char *vtag;
    int targc;
    char *targv[];

    /* If non-NULL, description for the file.  If NULL, the description
       will be empty.  */
    char *desctext;
    size_t desclen;

    /* Write errors to here as well as via error (), or NULL if we should
       use only error ().  */
    FILE *add_logfp;
{
    FILE *fprcs, *fpuser;
    struct stat sb;
    struct tm *ftm;
    time_t now;
    char altdate1[MAXDATELEN];
    char *author;
    int i, ierrno, err = 0;
    mode_t mode;
    char *tocvsPath;
    char *userfile;
    char *local_opt = key_opt;
    char *free_opt = NULL;
    mode_t file_type;

    if (noexec)
	return (0);

    /* Note that as the code stands now, the -k option overrides any
       settings in wrappers (whether CVSROOT/cvswrappers, -W, or
       whatever).  Some have suggested this should be the other way
       around.  As far as I know the documentation doesn't say one way
       or the other.  Before making a change of this sort, should think
       about what is best, document it (in cvs.texinfo and NEWS), &c.  */

    if (local_opt == NULL)
    {
	if (wrap_name_has (user, WRAP_RCSOPTION))
	{
	    local_opt = free_opt = wrap_rcsoption (user, 0);
	}
    }

    tocvsPath = wrap_tocvs_process_file (user);
    userfile = (tocvsPath == NULL ? user : tocvsPath);

    /* Opening in text mode is probably never the right thing for the
       server (because the protocol encodes text files in a fashion
       which does not depend on what the client or server OS is, as
       documented in cvsclient.texi), but as long as the server just
       runs on unix it is a moot point.  */

    /* If PreservePermissions is set, then make sure that the file
       is a plain file before trying to open it.  Longstanding (although
       often unpopular) CVS behavior has been to follow symlinks, so we
       maintain that behavior if PreservePermissions is not on.

       NOTE: this error message used to be `cannot fstat', but is now
       `cannot lstat'.  I don't see a way around this, since we must
       stat the file before opening it. -twp */

    if (CVS_LSTAT (userfile, &sb) < 0)
	error (1, errno, "cannot lstat %s", user);
    file_type = sb.st_mode & S_IFMT;

    fpuser = NULL;
    if (!preserve_perms || file_type == S_IFREG)
    {
	fpuser = CVS_FOPEN (userfile,
			    ((local_opt != NULL && strcmp (local_opt, "b") == 0)
			     ? "rb"
			     : "r")
	    );
	if (fpuser == NULL)
	{
	    /* not fatal, continue import */
	    if (add_logfp != NULL)
		fperror (add_logfp, 0, errno,
			 "ERROR: cannot read file %s", userfile);
	    error (0, errno, "ERROR: cannot read file %s", userfile);
	    goto read_error;
	}
    }

    fprcs = CVS_FOPEN (rcs, "w+b");
    if (fprcs == NULL)
    {
	ierrno = errno;
	goto write_error_noclose;
    }

    /*
     * putadmin()
     */
    if (add_vhead != NULL)
    {
	if (fprintf (fprcs, "head     %s;\012", add_vhead) < 0)
	    goto write_error;
    }
    else
    {
	if (fprintf (fprcs, "head     ;\012") < 0)
	    goto write_error;
    }

    if (add_vbranch != NULL)
    {
	if (fprintf (fprcs, "branch   %s;\012", add_vbranch) < 0)
	    goto write_error;
    }
    if (fprintf (fprcs, "access   ;\012") < 0 ||
	fprintf (fprcs, "symbols  ") < 0)
    {
	goto write_error;
    }

    for (i = targc - 1; i >= 0; i--)
    {
	/* RCS writes the symbols backwards */
	assert (add_vbranch != NULL);
	if (fprintf (fprcs, "%s:%s.1 ", targv[i], add_vbranch) < 0)
	    goto write_error;
    }

    if (add_vbranch != NULL)
    {
	if (fprintf (fprcs, "%s:%s", vtag, add_vbranch) < 0)
	    goto write_error;
    }
    if (fprintf (fprcs, ";\012") < 0)
	goto write_error;

    if (fprintf (fprcs, "locks    ; strict;\012") < 0 ||
	/* XXX - make sure @@ processing works in the RCS file */
	fprintf (fprcs, "comment  @%s@;\012", get_comment (user)) < 0)
    {
	goto write_error;
    }

    if (local_opt != NULL)
    {
	if (fprintf (fprcs, "expand   @%s@;\012", local_opt) < 0)
	{
	    goto write_error;
	}
    }

    if (fprintf (fprcs, "\012") < 0)
      goto write_error;

    /* Write the revision(s), with the date and author and so on
       (that is "delta" rather than "deltatext" from rcsfile(5)).  */
    if (add_vhead != NULL)
    {
	if (use_file_modtime)
	    now = sb.st_mtime;
	else
	    (void) time (&now);
	ftm = gmtime (&now);
	(void) sprintf (altdate1, DATEFORM,
			ftm->tm_year + (ftm->tm_year < 100 ? 0 : 1900),
			ftm->tm_mon + 1, ftm->tm_mday, ftm->tm_hour,
			ftm->tm_min, ftm->tm_sec);
	author = getcaller ();

	if (fprintf (fprcs, "\012%s\012", add_vhead) < 0 ||
	fprintf (fprcs, "date     %s;  author %s;  state Exp;\012",
		 altdate1, author) < 0)
	goto write_error;

	if (fprintf (fprcs, "branches") < 0)
	    goto write_error;
	if (add_vbranch != NULL)
	{
	    if (fprintf (fprcs, " %s.1", add_vbranch) < 0)
		goto write_error;
	}
	if (fprintf (fprcs, ";\012") < 0)
	    goto write_error;

	if (fprintf (fprcs, "next     ;\012") < 0)
	    goto write_error;

#ifdef PRESERVE_PERMISSIONS_SUPPORT
	/* Store initial permissions if necessary. */
	if (preserve_perms)
	{
	    if (file_type == S_IFLNK)
	    {
		char *link = xreadlink (userfile);
		if (fprintf (fprcs, "symlink\t@") < 0 ||
		    expand_at_signs (link, strlen (link), fprcs) < 0 ||
		    fprintf (fprcs, "@;\012") < 0)
		    goto write_error;
		free (link);
	    }
	    else
	    {
		if (fprintf (fprcs, "owner\t%u;\012", sb.st_uid) < 0)
		    goto write_error;
		if (fprintf (fprcs, "group\t%u;\012", sb.st_gid) < 0)
		    goto write_error;
		if (fprintf (fprcs, "permissions\t%o;\012",
			     sb.st_mode & 07777) < 0)
		    goto write_error;
		switch (file_type)
		{
		    case S_IFREG: break;
		    case S_IFCHR:
		    case S_IFBLK:
			if (fprintf (fprcs, "special\t%s %lu;\012",
				     (file_type == S_IFCHR
				      ? "character"
				      : "block"),
				     (unsigned long) sb.st_rdev) < 0)
			    goto write_error;
			break;
		    default:
			error (0, 0,
			       "can't import %s: unknown kind of special file",
			       userfile);
		}
	    }
	}
#endif

	if (add_vbranch != NULL)
	{
	    if (fprintf (fprcs, "\012%s.1\012", add_vbranch) < 0 ||
		fprintf (fprcs, "date     %s;  author %s;  state Exp;\012",
			 altdate1, author) < 0 ||
		fprintf (fprcs, "branches ;\012") < 0 ||
		fprintf (fprcs, "next     ;\012") < 0)
		goto write_error;

#ifdef PRESERVE_PERMISSIONS_SUPPORT
	    /* Store initial permissions if necessary. */
	    if (preserve_perms)
	    {
		if (file_type == S_IFLNK)
		{
		    char *link = xreadlink (userfile);
		    if (fprintf (fprcs, "symlink\t@") < 0 ||
			expand_at_signs (link, strlen (link), fprcs) < 0 ||
			fprintf (fprcs, "@;\012") < 0)
			goto write_error;
		    free (link);
		}
		else
		{
		    if (fprintf (fprcs, "owner\t%u;\012", sb.st_uid) < 0 ||
			fprintf (fprcs, "group\t%u;\012", sb.st_gid) < 0 ||
			fprintf (fprcs, "permissions\t%o;\012",
				 sb.st_mode & 07777) < 0)
			goto write_error;
	    
		    switch (file_type)
		    {
			case S_IFREG: break;
			case S_IFCHR:
			case S_IFBLK:
			    if (fprintf (fprcs, "special\t%s %lu;\012",
					 (file_type == S_IFCHR
					  ? "character"
					  : "block"),
					 (unsigned long) sb.st_rdev) < 0)
				goto write_error;
			    break;
			default:
			    error (0, 0,
			      "cannot import %s: special file of unknown type",
			       userfile);
		    }
		}
	    }
#endif

	    if (fprintf (fprcs, "\012") < 0)
		goto write_error;
	}
    }

    /* Now write the description (possibly empty).  */
    if (fprintf (fprcs, "\012desc\012") < 0 ||
	fprintf (fprcs, "@") < 0)
	goto write_error;
    if (desctext != NULL)
    {
	/* The use of off_t not size_t for the second argument is very
	   strange, since we are dealing with something which definitely
	   fits in memory.  */
	if (expand_at_signs (desctext, (off_t) desclen, fprcs) < 0)
	    goto write_error;
    }
    if (fprintf (fprcs, "@\012\012\012") < 0)
	goto write_error;

    /* Now write the log messages and contents for the revision(s) (that
       is, "deltatext" rather than "delta" from rcsfile(5)).  */
    if (add_vhead != NULL)
    {
	if (fprintf (fprcs, "\012%s\012", add_vhead) < 0 ||
	    fprintf (fprcs, "log\012@") < 0)
	    goto write_error;
	if (add_vbranch != NULL)
	{
	    /* We are going to put the log message in the revision on the
	       branch.  So putting it here too seems kind of redundant, I
	       guess (and that is what CVS has always done, anyway).  */
	    if (fprintf (fprcs, "Initial revision\012") < 0)
		goto write_error;
	}
	else
	{
	    if (expand_at_signs (message, (off_t) strlen (message), fprcs) < 0)
		goto write_error;
	}
	if (fprintf (fprcs, "@\012") < 0 ||
	    fprintf (fprcs, "text\012@") < 0)
	{
	    goto write_error;
	}

	/* Now copy over the contents of the file, expanding at signs.
	   If preserve_perms is set, do this only for regular files. */
	if (!preserve_perms || file_type == S_IFREG)
	{
	    char buf[8192];
	    unsigned int len;

	    while (1)
	    {
		len = fread (buf, 1, sizeof buf, fpuser);
		if (len == 0)
		{
		    if (ferror (fpuser))
			error (1, errno, "cannot read file %s for copying",
			       user);
		    break;
		}
		if (expand_at_signs (buf, len, fprcs) < 0)
		    goto write_error;
	    }
	}
	if (fprintf (fprcs, "@\012\012") < 0)
	    goto write_error;
	if (add_vbranch != NULL)
	{
	    if (fprintf (fprcs, "\012%s.1\012", add_vbranch) < 0 ||
		fprintf (fprcs, "log\012@") < 0 ||
		expand_at_signs (message,
				 (off_t) strlen (message), fprcs) < 0 ||
		fprintf (fprcs, "@\012text\012") < 0 ||
		fprintf (fprcs, "@@\012") < 0)
		goto write_error;
	}
    }

    if (fclose (fprcs) == EOF)
    {
	ierrno = errno;
	goto write_error_noclose;
    }
    /* Close fpuser only if we opened it to begin with. */
    if (fpuser != NULL)
    {
	if (fclose (fpuser) < 0)
	    error (0, errno, "cannot close %s", user);
    }

    /*
     * Fix the modes on the RCS files.  The user modes of the original
     * user file are propagated to the group and other modes as allowed
     * by the repository umask, except that all write permissions are
     * turned off.
     */
    mode = (sb.st_mode |
	    (sb.st_mode & S_IRWXU) >> 3 |
	    (sb.st_mode & S_IRWXU) >> 6) &
	   ~cvsumask &
	   ~(S_IWRITE | S_IWGRP | S_IWOTH);
    if (chmod (rcs, mode) < 0)
    {
	ierrno = errno;
	if (add_logfp != NULL)
	    fperror (add_logfp, 0, ierrno,
		     "WARNING: cannot change mode of file %s", rcs);
	error (0, ierrno, "WARNING: cannot change mode of file %s", rcs);
	err++;
    }
    if (tocvsPath)
	if (unlink_file_dir (tocvsPath) < 0)
		error (0, errno, "cannot remove %s", tocvsPath);
    if (free_opt != NULL)
	free (free_opt);
    return (err);

write_error:
    ierrno = errno;
    if (fclose (fprcs) < 0)
	error (0, errno, "cannot close %s", rcs);
write_error_noclose:
    if (fclose (fpuser) < 0)
	error (0, errno, "cannot close %s", user);
    if (add_logfp != NULL)
	fperror (add_logfp, 0, ierrno, "ERROR: cannot write file %s", rcs);
    error (0, ierrno, "ERROR: cannot write file %s", rcs);
    if (ierrno == ENOSPC)
    {
	if (CVS_UNLINK (rcs) < 0)
	    error (0, errno, "cannot remove %s", rcs);
	if (add_logfp != NULL)
	    fperror (add_logfp, 0, 0, "ERROR: out of space - aborting");
	error (1, 0, "ERROR: out of space - aborting");
    }
read_error:
    if (tocvsPath)
	if (unlink_file_dir (tocvsPath) < 0)
	    error (0, errno, "cannot remove %s", tocvsPath);

    if (free_opt != NULL)
	free (free_opt);

    return (err + 1);
}

/*
 * Write SIZE bytes at BUF to FP, expanding @ signs into double @
 * signs.  If an error occurs, return a negative value and set errno
 * to indicate the error.  If not, return a nonnegative value.
 */
int
expand_at_signs (buf, size, fp)
    char *buf;
    off_t size;
    FILE *fp;
{
    register char *cp, *next;

    cp = buf;
    while ((next = memchr (cp, '@', size)) != NULL)
    {
	int len;

	++next;
	len = next - cp;
	if (fwrite (cp, 1, len, fp) != len)
	    return EOF;
	if (putc ('@', fp) == EOF)
	    return EOF;
	cp = next;
	size -= len;
    }

    if (fwrite (cp, 1, size, fp) != size)
	return EOF;

    return 1;
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
	char buf[2];
	buf[0] = ch;
	buf[1] = ' ';
	cvs_output (buf, 2);
	if (repos_len)
	{
	    cvs_output (repository + repos_len + 1, 0);
	    cvs_output ("/", 1);
	}
	else if (repository[0] != '\0')
	{
	    cvs_output (repository, 0);
	    cvs_output ("/", 1);
	}
	cvs_output (fname, 0);
	cvs_output ("\n", 1);
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
    struct saved_cwd cwd;
    char *cp;
    int ierrno, err;
    char *rcs = NULL;

    if (islink (dir))
	return (0);
    if (save_cwd (&cwd))
    {
	fperror (logfp, 0, 0, "ERROR: cannot get working directory");
	return (1);
    }

    /* Concatenate DIR to the end of REPOSITORY.  */
    if (repository[0] == '\0')
    {
	char *new = xstrdup (dir);
	free (repository);
	repository = new;
    }
    else
    {
	char *new = xmalloc (strlen (repository) + strlen (dir) + 10);
	strcpy (new, repository);
	(void) strcat (new, "/");
	(void) strcat (new, dir);
	free (repository);
	repository = new;
    }

#ifdef CLIENT_SUPPORT
    if (!quiet && !client_active)
#else
    if (!quiet)
#endif
	error (0, 0, "Importing %s", repository);

    if ( CVS_CHDIR (dir) < 0)
    {
	ierrno = errno;
	fperror (logfp, 0, ierrno, "ERROR: cannot chdir to %s", repository);
	error (0, ierrno, "ERROR: cannot chdir to %s", repository);
	err = 1;
	goto out;
    }
#ifdef CLIENT_SUPPORT
    if (!client_active && !isdir (repository))
#else
    if (!isdir (repository))
#endif
    {
	rcs = xmalloc (strlen (repository) + sizeof (RCSEXT) + 5);
	(void) sprintf (rcs, "%s%s", repository, RCSEXT);
	if (isfile (repository) || isfile(rcs))
	{
	    fperror (logfp, 0, 0,
		     "ERROR: %s is a file, should be a directory!",
		     repository);
	    error (0, 0, "ERROR: %s is a file, should be a directory!",
		   repository);
	    err = 1;
	    goto out;
	}
	if (noexec == 0 && CVS_MKDIR (repository, 0777) < 0)
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
    if (rcs != NULL)
	free (rcs);
    if ((cp = strrchr (repository, '/')) != NULL)
	*cp = '\0';
    else
	repository[0] = '\0';
    if (restore_cwd (&cwd, NULL))
	error_exit ();
    free_cwd (&cwd);
    return (err);
}
