/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Difference
 * 
 * Run diff against versions in the repository.  Options that are specified are
 * passed on directly to "rcsdiff".
 * 
 * Without any file arguments, runs diff against all the currently modified
 * files.
 *
 * $FreeBSD$
 */

#include <assert.h>
#include "cvs.h"

enum diff_file
{
    DIFF_ERROR,
    DIFF_ADDED,
    DIFF_REMOVED,
    DIFF_DIFFERENT,
    DIFF_SAME
};

static Dtype diff_dirproc PROTO ((void *callerdat, const char *dir,
                                  const char *pos_repos,
                                  const char *update_dir,
                                  List *entries));
static int diff_filesdoneproc PROTO ((void *callerdat, int err,
                                      const char *repos,
                                      const char *update_dir,
                                      List *entries));
static int diff_dirleaveproc PROTO ((void *callerdat, const char *dir,
                                     int err, const char *update_dir,
                                     List *entries));
static enum diff_file diff_file_nodiff PROTO(( struct file_info *finfo,
					       Vers_TS *vers,
					       enum diff_file, 
					       char **rev1_cache ));
static int diff_fileproc PROTO ((void *callerdat, struct file_info *finfo));
static void diff_mark_errors PROTO((int err));


/* Global variables.  Would be cleaner if we just put this stuff in a
   struct like log.c does.  */

/* Command line tags, from -r option.  Points into argv.  */
static char *diff_rev1, *diff_rev2;
/* Command line dates, from -D option.  Malloc'd.  */
static char *diff_date1, *diff_date2;
static char *diff_join1, *diff_join2;
static char *use_rev1, *use_rev2;
static int have_rev1_label, have_rev2_label;

/* Revision of the user file, if it is unchanged from something in the
   repository and we want to use that fact.  */
static char *user_file_rev;

static char *options;
static char *opts;
static size_t opts_allocated = 1;
static int diff_errors;
static int empty_files = 0;

static const char *const diff_usage[] =
{
    "Usage: %s %s [-lR] [-k kopt] [format_options]\n",
    "    [[-r rev1 | -D date1] [-r rev2 | -D date2]] [files...] \n",
    "\t-l\tLocal directory only, not recursive\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-k kopt\tSpecify keyword expansion mode.\n",
    "\t-D d1\tDiff revision for date against working file.\n",
    "\t-D d2\tDiff rev1/date1 against date2.\n",
    "\t-r rev1\tDiff revision for rev1 against working file.\n",
    "\t-r rev2\tDiff rev1/date1 against rev2.\n",
    "\nformat_options:\n",
    "  -i  --ignore-case  Consider upper- and lower-case to be the same.\n",
    "  -w  --ignore-all-space  Ignore all white space.\n",
    "  -b  --ignore-space-change  Ignore changes in the amount of white space.\n",
    "  -B  --ignore-blank-lines  Ignore changes whose lines are all blank.\n",
    "  -I RE  --ignore-matching-lines=RE  Ignore changes whose lines all match RE.\n",
    "  --binary  Read and write data in binary mode.\n",
    "  -a  --text  Treat all files as text.\n\n",
    "  -c  -C NUM  --context[=NUM]  Output NUM (default 2) lines of copied context.\n",
    "  -u  -U NUM  --unified[=NUM]  Output NUM (default 2) lines of unified context.\n",
    "    -NUM  Use NUM context lines.\n",
    "    -L LABEL  --label LABEL  Use LABEL instead of file name.\n",
    "    -p  --show-c-function  Show which C function each change is in.\n",
    "    -F RE  --show-function-line=RE  Show the most recent line matching RE.\n",
    "  --brief  Output only whether files differ.\n",
    "  -e  --ed  Output an ed script.\n",
    "  -f  --forward-ed  Output something like an ed script in forward order.\n",
    "  -n  --rcs  Output an RCS format diff.\n",
    "  -y  --side-by-side  Output in two columns.\n",
    "    -W NUM  --width=NUM  Output at most NUM (default 130) characters per line.\n",
    "    --left-column  Output only the left column of common lines.\n",
    "    --suppress-common-lines  Do not output common lines.\n",
    "  --ifdef=NAME  Output merged file to show `#ifdef NAME' diffs.\n",
    "  --GTYPE-group-format=GFMT  Similar, but format GTYPE input groups with GFMT.\n",
    "  --line-format=LFMT  Similar, but format all input lines with LFMT.\n",
    "  --LTYPE-line-format=LFMT  Similar, but format LTYPE input lines with LFMT.\n",
    "    LTYPE is `old', `new', or `unchanged'.  GTYPE is LTYPE or `changed'.\n",
    "    GFMT may contain:\n",
    "      %%<  lines from FILE1\n",
    "      %%>  lines from FILE2\n",
    "      %%=  lines common to FILE1 and FILE2\n",
    "      %%[-][WIDTH][.[PREC]]{doxX}LETTER  printf-style spec for LETTER\n",
    "        LETTERs are as follows for new group, lower case for old group:\n",
    "          F  first line number\n",
    "          L  last line number\n",
    "          N  number of lines = L-F+1\n",
    "          E  F-1\n",
    "          M  L+1\n",
    "    LFMT may contain:\n",
    "      %%L  contents of line\n",
    "      %%l  contents of line, excluding any trailing newline\n",
    "      %%[-][WIDTH][.[PREC]]{doxX}n  printf-style spec for input line number\n",
    "    Either GFMT or LFMT may contain:\n",
    "      %%%%  %%\n",
    "      %%c'C'  the single character C\n",
    "      %%c'\\OOO'  the character with octal code OOO\n\n",
    "  -t  --expand-tabs  Expand tabs to spaces in output.\n",
    "  -T  --initial-tab  Make tabs line up by prepending a tab.\n\n",
    "  -N  --new-file  Treat absent files as empty.\n",
    "  -s  --report-identical-files  Report when two files are the same.\n",
    "  --horizon-lines=NUM  Keep NUM lines of the common prefix and suffix.\n",
    "  -d  --minimal  Try hard to find a smaller set of changes.\n",
    "  -H  --speed-large-files  Assume large files and many scattered small changes.\n",
    "\n(Specify the --help global option for a list of other help options)\n",
    NULL
};

/* I copied this array directly out of diff.c in diffutils 2.7, after
   removing the following entries, none of which seem relevant to use
   with CVS:
     --help
     --version (-v)
     --recursive (-r)
     --unidirectional-new-file (-P)
     --starting-file (-S)
     --exclude (-x)
     --exclude-from (-X)
     --sdiff-merge-assist
     --paginate (-l)  (doesn't work with library callbacks)

   I changed the options which take optional arguments (--context and
   --unified) to return a number rather than a letter, so that the
   optional argument could be handled more easily.  I changed the
   --brief and --ifdef options to return numbers, since -q  and -D mean
   something else to cvs diff.

   The numbers 129- that appear in the fourth element of some entries
   tell the big switch in `diff' how to process those options. -- Ian

   The following options, which diff lists as "An alias, no longer
   recommended" have been removed: --file-label --entire-new-file
   --ascii --print.  */

static struct option const longopts[] =
{
    {"ignore-blank-lines", 0, 0, 'B'},
    {"context", 2, 0, 143},
    {"ifdef", 1, 0, 131},
    {"show-function-line", 1, 0, 'F'},
    {"speed-large-files", 0, 0, 'H'},
    {"ignore-matching-lines", 1, 0, 'I'},
    {"label", 1, 0, 'L'},
    {"new-file", 0, 0, 'N'},
    {"initial-tab", 0, 0, 'T'},
    {"width", 1, 0, 'W'},
    {"text", 0, 0, 'a'},
    {"ignore-space-change", 0, 0, 'b'},
    {"minimal", 0, 0, 'd'},
    {"ed", 0, 0, 'e'},
    {"forward-ed", 0, 0, 'f'},
    {"ignore-case", 0, 0, 'i'},
    {"rcs", 0, 0, 'n'},
    {"show-c-function", 0, 0, 'p'},

    /* This is a potentially very useful option, except the output is so
       silly.  It would be much better for it to look like "cvs rdiff -s"
       which displays all the same info, minus quite a few lines of
       extraneous garbage.  */
    {"brief", 0, 0, 145},

    {"report-identical-files", 0, 0, 's'},
    {"expand-tabs", 0, 0, 't'},
    {"ignore-all-space", 0, 0, 'w'},
    {"side-by-side", 0, 0, 'y'},
    {"unified", 2, 0, 146},
    {"left-column", 0, 0, 129},
    {"suppress-common-lines", 0, 0, 130},
    {"old-line-format", 1, 0, 132},
    {"new-line-format", 1, 0, 133},
    {"unchanged-line-format", 1, 0, 134},
    {"line-format", 1, 0, 135},
    {"old-group-format", 1, 0, 136},
    {"new-group-format", 1, 0, 137},
    {"unchanged-group-format", 1, 0, 138},
    {"changed-group-format", 1, 0, 139},
    {"horizon-lines", 1, 0, 140},
    {"binary", 0, 0, 142},
    {0, 0, 0, 0}
};

/* CVS 1.9 and similar versions seemed to have pretty weird handling
   of -y and -T.  In the cases where it called rcsdiff,
   they would have the meanings mentioned below.  In the cases where it
   called diff, they would have the meanings mentioned in "longopts".
   Noone seems to have missed them, so I think the right thing to do is
   just to remove the options altogether (which I have done).

   In the case of -z and -q, "cvs diff" did not accept them even back
   when we called rcsdiff (at least, it hasn't accepted them
   recently).

   In comparing rcsdiff to the new CVS implementation, I noticed that
   the following rcsdiff flags are not handled by CVS diff:

	   -y: perform diff even when the requested revisions are the
		   same revision number
	   -q: run quietly
	   -T: preserve modification time on the RCS file
	   -z: specify timezone for use in file labels

   I think these are not really relevant.  -y is undocumented even in
   RCS 5.7, and seems like a minor change at best.  According to RCS
   documentation, -T only applies when a RCS file has been modified
   because of lock changes; doesn't CVS sidestep RCS's entire lock
   structure?  -z seems to be unsupported by CVS diff, and has a
   different meaning as a global option anyway.  (Adding it could be
   a feature, but if it is left out for now, it should not break
   anything.)  For the purposes of producing output, CVS diff appears
   mostly to ignore -q.  Maybe this should be fixed, but I think it's
   a larger issue than the changes included here.  */

int
diff (argc, argv)
    int argc;
    char **argv;
{
    char tmp[50];
    int c, err = 0;
    int local = 0;
    int which;
    int option_index;

    if (argc == -1)
	usage (diff_usage);

    have_rev1_label = have_rev2_label = 0;

    /*
     * Note that we catch all the valid arguments here, so that we can
     * intercept the -r arguments for doing revision diffs; and -l/-R for a
     * non-recursive/recursive diff.
     */

    /* Clean out our global variables (multiroot can call us multiple
       times and the server can too, if the client sends several
       diff commands).  */
    if (opts == NULL)
    {
	opts_allocated = 1;
	opts = xmalloc (opts_allocated);
    }
    opts[0] = '\0';
    diff_rev1 = NULL;
    diff_rev2 = NULL;
    diff_date1 = NULL;
    diff_date2 = NULL;
    diff_join1 = NULL;
    diff_join2 = NULL;

    optind = 0;
    /* FIXME: This should really be allocating an argv to be passed to diff
     * later rather than strcatting onto the opts variable.  We have some
     * handling routines that can already handle most of the argc/argv
     * maintenance for us and currently, if anyone were to attempt to pass a
     * quoted string in here, it would be split on spaces and tabs on its way
     * to diff.
     */
    while ((c = getopt_long (argc, argv,
	       "+abcdefhilnpstuwy0123456789BHNRTC:D:F:I:L:U:W:k:r:j:",
			     longopts, &option_index)) != -1)
    {
	switch (c)
	{
	    case 'y':
		xrealloc_and_strcat (&opts, &opts_allocated, " --side-by-side");
		break;
	    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	    case 'h': case 'i': case 'n': case 'p': case 's': case 't':
	    case 'u': case 'w':
            case '0': case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9':
	    case 'B': case 'H': case 'T':
		(void) sprintf (tmp, " -%c", (char) c);
		xrealloc_and_strcat (&opts, &opts_allocated, tmp);
		break;
	    case 'L':
		if (have_rev1_label++)
		    if (have_rev2_label++)
		    {
			error (0, 0, "extra -L arguments ignored");
			break;
		    }

	        xrealloc_and_strcat (&opts, &opts_allocated, " -L");
	        xrealloc_and_strcat (&opts, &opts_allocated, optarg);
		break;
	    case 'C': case 'F': case 'I': case 'U': case 'W':
		(void) sprintf (tmp, " -%c", (char) c);
		xrealloc_and_strcat (&opts, &opts_allocated, tmp);
		xrealloc_and_strcat (&opts, &opts_allocated, optarg);
		break;
	    case 131:
		/* --ifdef.  */
		xrealloc_and_strcat (&opts, &opts_allocated, " --ifdef=");
		xrealloc_and_strcat (&opts, &opts_allocated, optarg);
		break;
	    case 129: case 130:           case 132: case 133: case 134:
	    case 135: case 136: case 137: case 138: case 139: case 140:
	    case 141: case 142: case 143: case 145: case 146:
		xrealloc_and_strcat (&opts, &opts_allocated, " --");
		xrealloc_and_strcat (&opts, &opts_allocated,
				     longopts[option_index].name);
		if (longopts[option_index].has_arg == 1
		    || (longopts[option_index].has_arg == 2
			&& optarg != NULL))
		{
		    xrealloc_and_strcat (&opts, &opts_allocated, "=");
		    xrealloc_and_strcat (&opts, &opts_allocated, optarg);
		}
		break;
	    case 'R':
		local = 0;
		break;
	    case 'l':
		local = 1;
		break;
	    case 'k':
		if (options)
		    free (options);
		options = RCS_check_kflag (optarg);
		break;
	    case 'j':
		{
		    char *ptr;
		    char *cpy = strdup(optarg);

		    if ((ptr = strchr(optarg, ':')) != NULL)
			*ptr++ = 0;
		    if (diff_rev2 != NULL || diff_date2 != NULL)
			error (1, 0,
			   "no more than two revisions/dates can be specified");
		    if (diff_rev1 != NULL || diff_date1 != NULL) {
			diff_join2 = cpy;
			diff_rev2 = optarg;
			diff_date2 = ptr ? Make_Date(ptr) : NULL;
		    } else {
			diff_join1 = cpy;
			diff_rev1 = optarg;
			diff_date1 = ptr ? Make_Date(ptr) : NULL;
		    }
		}
		break;
	    case 'r':
		if (diff_rev2 != NULL || diff_date2 != NULL)
		    error (1, 0,
		       "no more than two revisions/dates can be specified");
		if (diff_rev1 != NULL || diff_date1 != NULL)
		    diff_rev2 = optarg;
		else
		    diff_rev1 = optarg;
		break;
	    case 'D':
		if (diff_rev2 != NULL || diff_date2 != NULL)
		    error (1, 0,
		       "no more than two revisions/dates can be specified");
		if (diff_rev1 != NULL || diff_date1 != NULL)
		    diff_date2 = Make_Date (optarg);
		else
		    diff_date1 = Make_Date (optarg);
		break;
	    case 'N':
		empty_files = 1;
		break;
	    case '?':
	    default:
		usage (diff_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    /* make sure options is non-null */
    if (!options)
	options = xstrdup ("");

#ifdef CLIENT_SUPPORT
    if (current_parsed_root->isremote) {
	/* We're the client side.  Fire up the remote server.  */
	start_server ();
	
	ign_setup ();

	if (local)
	    send_arg("-l");
	if (empty_files)
	    send_arg("-N");
	send_option_string (opts);
	if (options[0] != '\0')
	    send_arg (options);
	if (diff_join1)
	    option_with_arg ("-j", diff_join1);
	else if (diff_rev1)
	    option_with_arg ("-r", diff_rev1);
	else if (diff_date1)
	    client_senddate (diff_date1);

	if (diff_join2)
	    option_with_arg ("-j", diff_join2);
	else if (diff_rev2)
	    option_with_arg ("-r", diff_rev2);
	else if (diff_date2)
	    client_senddate (diff_date2);
	send_arg ("--");

	/* Send the current files unless diffing two revs from the archive */
	if (diff_rev2 == NULL && diff_date2 == NULL)
	    send_files (argc, argv, local, 0, 0);
	else
	    send_files (argc, argv, local, 0, SEND_NO_CONTENTS);

	send_file_names (argc, argv, SEND_EXPAND_WILD);

	send_to_server ("diff\012", 0);
        err = get_responses_and_close ();
    } else
#endif
    {
	if (diff_rev1 != NULL)
	    tag_check_valid (diff_rev1, argc, argv, local, 0, "");
	if (diff_rev2 != NULL)
	    tag_check_valid (diff_rev2, argc, argv, local, 0, "");

	which = W_LOCAL;
	if (diff_rev1 != NULL || diff_date1 != NULL)
	    which |= W_REPOS | W_ATTIC;

	wrap_setup ();

	/* start the recursion processor */
	err = start_recursion (diff_fileproc, diff_filesdoneproc, diff_dirproc,
			       diff_dirleaveproc, NULL, argc, argv, local,
			       which, 0, CVS_LOCK_READ, (char *) NULL, 1,
			       (char *) NULL);
    }

    /* clean up */
    free (options);
    options = NULL;

    if (diff_date1 != NULL)
	free (diff_date1);
    if (diff_date2 != NULL)
	free (diff_date2);
    if (diff_join1 != NULL)
	free (diff_join1);
    if (diff_join2 != NULL)
	free (diff_join2);

    return (err);
}

/*
 * Do a file diff
 */
/* ARGSUSED */
static int
diff_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    int status, err = 2;		/* 2 == trouble, like rcsdiff */
    Vers_TS *vers;
    enum diff_file empty_file = DIFF_DIFFERENT;
    char *tmp = NULL;
    char *tocvsPath = NULL;
    char *fname = NULL;
    char *label1;
    char *label2;
    char *rev1_cache = NULL;

    user_file_rev = 0;
    vers = Version_TS (finfo, NULL, NULL, NULL, 1, 0);

    if (diff_rev2 != NULL || diff_date2 != NULL)
    {
	/* Skip all the following checks regarding the user file; we're
	   not using it.  */
    }
    else if (vers->vn_user == NULL)
    {
	/* The file does not exist in the working directory.  */
	if ((diff_rev1 != NULL || diff_date1 != NULL)
	    && vers->srcfile != NULL)
	{
	    /* The file does exist in the repository.  */
	    if (empty_files)
		empty_file = DIFF_REMOVED;
	    else
	    {
		int exists;

		exists = 0;
		/* special handling for TAG_HEAD XXX */
		if (diff_rev1 && strcmp (diff_rev1, TAG_HEAD) == 0)
		{
		    char *head =
			(vers->vn_rcs == NULL
			 ? NULL
			 : RCS_branch_head (vers->srcfile, vers->vn_rcs));
		    exists = head != NULL && !RCS_isdead(vers->srcfile, head);
		    if (head != NULL)
			free (head);
		}
		else
		{
		    Vers_TS *xvers;

		    xvers = Version_TS (finfo, NULL, diff_rev1, diff_date1,
					1, 0);
		    exists = xvers->vn_rcs != NULL && !RCS_isdead(xvers->srcfile, xvers->vn_rcs);
		    freevers_ts (&xvers);
		}
		if (exists)
		    error (0, 0,
			   "%s no longer exists, no comparison available",
			   finfo->fullname);
		goto out;
	    }
	}
	else
	{
	    error (0, 0, "I know nothing about %s", finfo->fullname);
	    goto out;
	}
    }
    else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
    {
	/* The file was added locally.  */
	int exists = 0;

	if (vers->srcfile != NULL)
	{
	    /* The file does exist in the repository.  */

	    if ((diff_rev1 != NULL || diff_date1 != NULL))
	    {
		/* special handling for TAG_HEAD */
		if (diff_rev1 && strcmp (diff_rev1, TAG_HEAD) == 0)
		{
		    char *head =
			(vers->vn_rcs == NULL
			 ? NULL
			 : RCS_branch_head (vers->srcfile, vers->vn_rcs));
		    exists = head != NULL && !RCS_isdead(vers->srcfile, head);
		    if (head != NULL)
			free (head);
		}
		else
		{
		    Vers_TS *xvers;

		    xvers = Version_TS (finfo, NULL, diff_rev1, diff_date1,
					1, 0);
		    exists = xvers->vn_rcs != NULL
		             && !RCS_isdead (xvers->srcfile, xvers->vn_rcs);
		    freevers_ts (&xvers);
		}
	    }
	    else
	    {
		/* The file was added locally, but an RCS archive exists.  Our
		 * base revision must be dead.
		 */
		/* No need to set, exists = 0, here.  That's the default.  */
	    }
	}
	if (!exists)
	{
	    /* If we got here, then either the RCS archive does not exist or
	     * the relevant revision is dead.
	     */
	    if (empty_files)
		empty_file = DIFF_ADDED;
	    else
	    {
		error (0, 0, "%s is a new entry, no comparison available",
		       finfo->fullname);
		goto out;
	    }
	}
    }
    else if (vers->vn_user[0] == '-')
    {
	if (empty_files)
	    empty_file = DIFF_REMOVED;
	else
	{
	    error (0, 0, "%s was removed, no comparison available",
		   finfo->fullname);
	    goto out;
	}
    }
    else
    {
	if (vers->vn_rcs == NULL && vers->srcfile == NULL)
	{
	    error (0, 0, "cannot find revision control file for %s",
		   finfo->fullname);
	    goto out;
	}
	else
	{
	    if (vers->ts_user == NULL)
	    {
		error (0, 0, "cannot find %s", finfo->fullname);
		goto out;
	    }
	    else if (!strcmp (vers->ts_user, vers->ts_rcs)) 
	    {
		/* The user file matches some revision in the repository
		   Diff against the repository (for remote CVS, we might not
		   have a copy of the user file around).  */
		user_file_rev = vers->vn_user;
	    }
	}
    }

    empty_file = diff_file_nodiff( finfo, vers, empty_file, &rev1_cache );
    if( empty_file == DIFF_SAME )
    {
	/* In the server case, would be nice to send a "Checked-in"
	   response, so that the client can rewrite its timestamp.
	   server_checked_in by itself isn't the right thing (it
	   needs a server_register), but I'm not sure what is.
	   It isn't clear to me how "cvs status" handles this (that
	   is, for a client which sends Modified not Is-modified to
	   "cvs status"), but it does.  */
	err = 0;
	goto out;
    }
    else if( empty_file == DIFF_ERROR )
	goto out;

    /* Output an "Index:" line for patch to use */
    cvs_output ("Index: ", 0);
    cvs_output (finfo->fullname, 0);
    cvs_output ("\n", 1);

    tocvsPath = wrap_tocvs_process_file(finfo->file);
    if( tocvsPath != NULL )
    {
	/* Backup the current version of the file to CVS/,,filename */
	fname = xmalloc (strlen (finfo->file)
			 + sizeof CVSADM
			 + sizeof CVSPREFIX
			 + 10);
	sprintf(fname,"%s/%s%s",CVSADM, CVSPREFIX, finfo->file);
	if (unlink_file_dir (fname) < 0)
	    if (! existence_error (errno))
		error (1, errno, "cannot remove %s", fname);
	rename_file (finfo->file, fname);
	/* Copy the wrapped file to the current directory then go to work */
	copy_file (tocvsPath, finfo->file);
    }

    /* Set up file labels appropriate for compatibility with the Larry Wall
     * implementation of patch if the user didn't specify.  This is irrelevant
     * according to the POSIX.2 specification.
     */
    label1 = NULL;
    label2 = NULL;
    if (!have_rev1_label)
    {
	if (empty_file == DIFF_ADDED)
	    label1 =
		make_file_label (DEVNULL, NULL, NULL);
	else
	    label1 =
                make_file_label (finfo->fullname, use_rev1,
                                 vers ? vers->srcfile : NULL);
    }

    if (!have_rev2_label)
    {
	if (empty_file == DIFF_REMOVED)
	    label2 =
		make_file_label (DEVNULL, NULL, NULL);
	else
	    label2 =
                make_file_label (finfo->fullname, use_rev2,
                                 vers ? vers->srcfile : NULL);
    }

    if (empty_file == DIFF_ADDED || empty_file == DIFF_REMOVED)
    {
	/* This is fullname, not file, possibly despite the POSIX.2
	 * specification, because that's the way all the Larry Wall
	 * implementations of patch (are there other implementations?) want
	 * things and the POSIX.2 spec appears to leave room for this.
	 */
	cvs_output ("\
===================================================================\n\
RCS file: ", 0);
	cvs_output (finfo->fullname, 0);
	cvs_output ("\n", 1);

	cvs_output ("diff -N ", 0);
	cvs_output (finfo->fullname, 0);
	cvs_output ("\n", 1);

	if (empty_file == DIFF_ADDED)
	{
	    if (use_rev2 == NULL)
                status = diff_exec (DEVNULL, finfo->file, label1, label2, opts,
                                    RUN_TTY);
	    else
	    {
		int retcode;

		tmp = cvs_temp_name ();
		retcode = RCS_checkout (vers->srcfile, (char *) NULL,
					use_rev2, (char *) NULL,
					(*options
					 ? options
					 : vers->options),
					tmp, (RCSCHECKOUTPROC) NULL,
					(void *) NULL);
		if( retcode != 0 )
		    goto out;

		status = diff_exec (DEVNULL, tmp, label1, label2, opts, RUN_TTY);
	    }
	}
	else
	{
	    int retcode;

	    tmp = cvs_temp_name ();
	    retcode = RCS_checkout (vers->srcfile, (char *) NULL,
				    use_rev1, (char *) NULL,
				    *options ? options : vers->options,
				    tmp, (RCSCHECKOUTPROC) NULL,
				    (void *) NULL);
	    if (retcode != 0)
		goto out;

	    status = diff_exec (tmp, DEVNULL, label1, label2, opts, RUN_TTY);
	}
    }
    else
    {
	status = RCS_exec_rcsdiff(vers->srcfile, opts,
                                  *options ? options : vers->options,
                                  use_rev1, rev1_cache, use_rev2,
                                  label1, label2,
                                  finfo->file);

    }

    if (label1) free (label1);
    if (label2) free (label2);

    switch (status)
    {
	case -1:			/* fork failed */
	    error (1, errno, "fork failed while diffing %s",
		   vers->srcfile->path);
	case 0:				/* everything ok */
	    err = 0;
	    break;
	default:			/* other error */
	    err = status;
	    break;
    }

out:
    if( tocvsPath != NULL )
    {
	if (unlink_file_dir (finfo->file) < 0)
	    if (! existence_error (errno))
		error (1, errno, "cannot remove %s", finfo->file);

	rename_file (fname, finfo->file);
	if (unlink_file (tocvsPath) < 0)
	    error (1, errno, "cannot remove %s", tocvsPath);
	free (fname);
    }

    /* Call CVS_UNLINK() rather than unlink_file() below to avoid the check
     * for noexec.
     */
    if( tmp != NULL )
    {
	if (CVS_UNLINK(tmp) < 0)
	    error (0, errno, "cannot remove %s", tmp);
	free (tmp);
    }
    if( rev1_cache != NULL )
    {
	if( CVS_UNLINK( rev1_cache ) < 0 )
	    error( 0, errno, "cannot remove %s", rev1_cache );
	free( rev1_cache );
    }

    freevers_ts (&vers);
    diff_mark_errors (err);
    return err;
}

/*
 * Remember the exit status for each file.
 */
static void
diff_mark_errors (err)
    int err;
{
    if (err > diff_errors)
	diff_errors = err;
}

/*
 * Print a warm fuzzy message when we enter a dir
 *
 * Don't try to diff directories that don't exist! -- DW
 */
/* ARGSUSED */
static Dtype
diff_dirproc (callerdat, dir, pos_repos, update_dir, entries)
    void *callerdat;
    const char *dir;
    const char *pos_repos;
    const char *update_dir;
    List *entries;
{
    /* XXX - check for dirs we don't want to process??? */

    /* YES ... for instance dirs that don't exist!!! -- DW */
    if (!isdir (dir))
	return (R_SKIP_ALL);

    if (!quiet)
	error (0, 0, "Diffing %s", update_dir);
    return (R_PROCESS);
}

/*
 * Concoct the proper exit status - done with files
 */
/* ARGSUSED */
static int
diff_filesdoneproc (callerdat, err, repos, update_dir, entries)
    void *callerdat;
    int err;
    const char *repos;
    const char *update_dir;
    List *entries;
{
    return (diff_errors);
}

/*
 * Concoct the proper exit status - leaving directories
 */
/* ARGSUSED */
static int
diff_dirleaveproc (callerdat, dir, err, update_dir, entries)
    void *callerdat;
    const char *dir;
    int err;
    const char *update_dir;
    List *entries;
{
    return (diff_errors);
}

/*
 * verify that a file is different
 */
static enum diff_file
diff_file_nodiff( finfo, vers, empty_file, rev1_cache )
    struct file_info *finfo;
    Vers_TS *vers;
    enum diff_file empty_file;
    char **rev1_cache;		/* Cache the content of rev1 if we have to look
				 * it up.
				 */
{
    Vers_TS *xvers;
    int retcode;

    /* free up any old use_rev* variables and reset 'em */
    if (use_rev1)
	free (use_rev1);
    if (use_rev2)
	free (use_rev2);
    use_rev1 = use_rev2 = (char *) NULL;

    if (diff_rev1 || diff_date1)
    {
	/* special handling for TAG_HEAD XXX */
	if (diff_rev1 && strcmp (diff_rev1, TAG_HEAD) == 0)
	{
	    if (vers->vn_rcs != NULL && vers->srcfile != NULL)
		use_rev1 = RCS_branch_head (vers->srcfile, vers->vn_rcs);
	}
	else
	{
	    xvers = Version_TS (finfo, NULL, diff_rev1, diff_date1, 1, 0);
	    if (xvers->vn_rcs != NULL)
		use_rev1 = xstrdup (xvers->vn_rcs);
	    freevers_ts (&xvers);
	}
    }
    if (diff_rev2 || diff_date2)
    {
	/* special handling for TAG_HEAD XXX */
	if (diff_rev2 && strcmp (diff_rev2, TAG_HEAD) == 0)
	{
	    if (vers->vn_rcs != NULL && vers->srcfile != NULL)
		use_rev2 = RCS_branch_head (vers->srcfile, vers->vn_rcs);
	}
	else
	{
	    xvers = Version_TS (finfo, NULL, diff_rev2, diff_date2, 1, 0);
	    if (xvers->vn_rcs != NULL)
		use_rev2 = xstrdup (xvers->vn_rcs);
	    freevers_ts (&xvers);
	}

	if( use_rev1 == NULL || RCS_isdead( vers->srcfile, use_rev1 ) )
	{
	    /* The first revision does not exist.  If EMPTY_FILES is
               true, treat this as an added file.  Otherwise, warn
               about the missing tag.  */
	    if( use_rev2 == NULL || RCS_isdead( vers->srcfile, use_rev2 ) )
		/* At least in the case where DIFF_REV1 and DIFF_REV2
		 * are both numeric (and non-existant (NULL), as opposed to
		 * dead?), we should be returning some kind of error (see
		 * basicb-8a0 in testsuite).  The symbolic case may be more
		 * complicated.
		 */
		return DIFF_SAME;
	    if( empty_files )
		return DIFF_ADDED;
	    if( use_rev1 != NULL )
	    {
		if (diff_rev1)
		{
		    error( 0, 0,
		       "Tag %s refers to a dead (removed) revision in file `%s'.",
		       diff_rev1, finfo->fullname );
		}
		else
		{
		    error( 0, 0,
		       "Date %s refers to a dead (removed) revision in file `%s'.",
		       diff_date1, finfo->fullname );
		}
		error( 0, 0,
		       "No comparison available.  Pass `-N' to `%s diff'?",
		       program_name );
	    }
	    else if (diff_rev1)
		error (0, 0, "tag %s is not in file %s", diff_rev1,
		       finfo->fullname);
	    else
		error (0, 0, "no revision for date %s in file %s",
		       diff_date1, finfo->fullname);
	    return DIFF_ERROR;
	}

	assert( use_rev1 != NULL );
	if( use_rev2 == NULL || RCS_isdead( vers->srcfile, use_rev2 ) )
	{
	    /* The second revision does not exist.  If EMPTY_FILES is
               true, treat this as a removed file.  Otherwise warn
               about the missing tag.  */
	    if (empty_files)
		return DIFF_REMOVED;
	    if( use_rev2 != NULL )
	    {
		if (diff_rev2)
		{
		    error( 0, 0,
		       "Tag %s refers to a dead (removed) revision in file `%s'.",
		       diff_rev2, finfo->fullname );
		}
		else
		{
		    error( 0, 0,
		       "Date %s refers to a dead (removed) revision in file `%s'.",
		       diff_date2, finfo->fullname );
		}
		error( 0, 0,
		       "No comparison available.  Pass `-N' to `%s diff'?",
		       program_name );
	    }
	    else if (diff_rev2)
		error (0, 0, "tag %s is not in file %s", diff_rev2,
		       finfo->fullname);
	    else
		error (0, 0, "no revision for date %s in file %s",
		       diff_date2, finfo->fullname);
	    return DIFF_ERROR;
	}
	/* Now, see if we really need to do the diff.  We can't assume that the
	 * files are different when the revs are.
	 */
	assert( use_rev2 != NULL );
	if( strcmp (use_rev1, use_rev2) == 0 )
	    return DIFF_SAME;
	/* else fall through and do the diff */
    }

    /* If we had a r1/d1 & r2/d2, then at this point we must have a C3P0...
     * err...  ok, then both rev1 & rev2 must have resolved to an existing,
     * live version due to if statement we just closed.
     */
    assert (!(diff_rev2 || diff_date2) || (use_rev1 && use_rev2));

    if ((diff_rev1 || diff_date1) &&
	(use_rev1 == NULL || RCS_isdead (vers->srcfile, use_rev1)))
    {
	/* The first revision does not exist, and no second revision
           was given.  */
	if (empty_files)
	{
	    if (empty_file == DIFF_REMOVED)
		return DIFF_SAME;
	    if( user_file_rev && use_rev2 == NULL )
		use_rev2 = xstrdup( user_file_rev );
	    return DIFF_ADDED;
	}
	if( use_rev1 != NULL )
	{
	    if (diff_rev1)
	    {
		error( 0, 0,
		   "Tag %s refers to a dead (removed) revision in file `%s'.",
		   diff_rev1, finfo->fullname );
	    }
	    else
	    {
		error( 0, 0,
		   "Date %s refers to a dead (removed) revision in file `%s'.",
		   diff_date1, finfo->fullname );
	    }
	    error( 0, 0,
		   "No comparison available.  Pass `-N' to `%s diff'?",
		   program_name );
	}
	else if ( diff_rev1 )
	    error( 0, 0, "tag %s is not in file %s", diff_rev1,
		   finfo->fullname );
	else
	    error( 0, 0, "no revision for date %s in file %s",
		   diff_date1, finfo->fullname );
	return DIFF_ERROR;
    }

    assert( !diff_rev1 || use_rev1 );

    if (user_file_rev)
    {
        /* drop user_file_rev into first unused use_rev */
        if (!use_rev1) 
	    use_rev1 = xstrdup (user_file_rev);
	else if (!use_rev2)
	    use_rev2 = xstrdup (user_file_rev);
	/* and if not, it wasn't needed anyhow */
	user_file_rev = NULL;
    }

    /* Now, see if we really need to do the diff.  We can't assume that the
     * files are different when the revs are.
     */
    if( use_rev1 && use_rev2) 
    {
	if (strcmp (use_rev1, use_rev2) == 0)
	    return DIFF_SAME;
	/* Fall through and do the diff. */
    }
    /* Don't want to do the timestamp check with both use_rev1 & use_rev2 set.
     * The timestamp check is just for the default case of diffing the
     * workspace file against its base revision.
     */
    else if( use_rev1 == NULL
             || ( vers->vn_user != NULL
                  && strcmp( use_rev1, vers->vn_user ) == 0 ) )
    {
	if (empty_file == DIFF_DIFFERENT
	    && vers->ts_user != NULL
	    && strcmp (vers->ts_rcs, vers->ts_user) == 0
	    && (!(*options) || strcmp (options, vers->options) == 0))
	{
	    return DIFF_SAME;
	}
	if (use_rev1 == NULL
	    && (vers->vn_user[0] != '0' || vers->vn_user[1] != '\0'))
	{
	    if (vers->vn_user[0] == '-')
		use_rev1 = xstrdup (vers->vn_user + 1);
	    else
		use_rev1 = xstrdup (vers->vn_user);
	}
    }

    /* If we already know that the file is being added or removed,
       then we don't want to do an actual file comparison here.  */
    if (empty_file != DIFF_DIFFERENT)
	return empty_file;

    /*
     * Run a quick cmp to see if we should bother with a full diff.
     */

    retcode = RCS_cmp_file( vers->srcfile, use_rev1, rev1_cache,
                            use_rev2, *options ? options : vers->options,
			    finfo->file );

    return retcode == 0 ? DIFF_SAME : DIFF_DIFFERENT;
}
