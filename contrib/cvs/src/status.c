/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Status Information
 */

#include "cvs.h"

static Dtype status_dirproc PROTO ((void *callerdat, char *dir,
				    char *repos, char *update_dir,
				    List *entries));
static int status_fileproc PROTO ((void *callerdat, struct file_info *finfo));
static int tag_list_proc PROTO((Node * p, void *closure));

static int local = 0;
static int long_format = 0;
static RCSNode *xrcsnode;

static const char *const status_usage[] =
{
    "Usage: %s %s [-vlR] [files...]\n",
    "\t-v\tVerbose format; includes tag information for the file\n",
    "\t-l\tProcess this directory only (not recursive).\n",
    "\t-R\tProcess directories recursively.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int
status (argc, argv)
    int argc;
    char **argv;
{
    int c;
    int err = 0;

    if (argc == -1)
	usage (status_usage);

    optind = 0;
    while ((c = getopt (argc, argv, "+vlR")) != -1)
    {
	switch (c)
	{
	    case 'v':
		long_format = 1;
		break;
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case '?':
	    default:
		usage (status_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    wrap_setup ();

#ifdef CLIENT_SUPPORT
    if (client_active) {
      start_server ();

      ign_setup ();

      if (long_format)
	send_arg("-v");
      if (local)
	send_arg("-l");

      send_file_names (argc, argv, SEND_EXPAND_WILD);

      /* For a while, we tried setting SEND_NO_CONTENTS here so this
	 could be a fast operation.  That prevents the
	 server from updating our timestamp if the timestamp is
	 changed but the file is unmodified.  Worse, it is user-visible
	 (shows "locally modified" instead of "up to date" if
	 timestamp is changed but file is not).  And there is no good
	 workaround (you might not want to run "cvs update"; "cvs -n
	 update" doesn't update CVS/Entries; "cvs diff --brief" or
	 something perhaps could be made to work but somehow that
	 seems nonintuitive to me even if so).  Given that timestamps
	 seem to have the potential to get munged for any number of
	 reasons, it seems better to not rely too much on them.  */

      send_files (argc, argv, local, 0, 0);

      send_to_server ("status\012", 0);
      err = get_responses_and_close ();

      return err;
    }
#endif

    /* start the recursion processor */
    err = start_recursion (status_fileproc, (FILESDONEPROC) NULL,
			   status_dirproc, (DIRLEAVEPROC) NULL, NULL,
			   argc, argv, local,
			   W_LOCAL, 0, 1, (char *) NULL, 1);

    return (err);
}

/*
 * display the status of a file
 */
/* ARGSUSED */
static int
status_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    Ctype status;
    char *sstat;
    Vers_TS *vers;

    status = Classify_File (finfo, (char *) NULL, (char *) NULL, (char *) NULL,
			    1, 0, &vers, 0);
    sstat = "Classify Error";
    switch (status)
    {
	case T_UNKNOWN:
	    sstat = "Unknown";
	    break;
	case T_CHECKOUT:
	    sstat = "Needs Checkout";
	    break;
#ifdef SERVER_SUPPORT
	case T_PATCH:
	    sstat = "Needs Patch";
	    break;
#endif
	case T_CONFLICT:
	    /* I _think_ that "unresolved" is correct; that if it has
	       been resolved then the status will change.  But I'm not
	       sure about that.  */
	    sstat = "Unresolved Conflict";
	    break;
	case T_ADDED:
	    sstat = "Locally Added";
	    break;
	case T_REMOVED:
	    sstat = "Locally Removed";
	    break;
	case T_MODIFIED:
	    if (vers->ts_conflict)
		sstat = "File had conflicts on merge";
	    else
		sstat = "Locally Modified";
	    break;
	case T_REMOVE_ENTRY:
	    sstat = "Entry Invalid";
	    break;
	case T_UPTODATE:
	    sstat = "Up-to-date";
	    break;
	case T_NEEDS_MERGE:
	    sstat = "Needs Merge";
	    break;
	case T_TITLE:
	    /* I don't think this case can occur here.  Just print
	       "Classify Error".  */
	    break;
    }

    cvs_output ("\
===================================================================\n", 0);
    if (vers->ts_user == NULL)
    {
	cvs_output ("File: no file ", 0);
	cvs_output (finfo->file, 0);
	cvs_output ("\t\tStatus: ", 0);
	cvs_output (sstat, 0);
	cvs_output ("\n\n", 0);
    }
    else
    {
	char *buf;
	buf = xmalloc (strlen (finfo->file) + strlen (sstat) + 80);
	sprintf (buf, "File: %-17s\tStatus: %s\n\n", finfo->file, sstat);
	cvs_output (buf, 0);
	free (buf);
    }

    if (vers->vn_user == NULL)
    {
	cvs_output ("   Working revision:\tNo entry for ", 0);
	cvs_output (finfo->file, 0);
	cvs_output ("\n", 0);
    }
    else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
	cvs_output ("   Working revision:\tNew file!\n", 0);
#ifdef SERVER_SUPPORT
    else if (server_active)
    {
	cvs_output ("   Working revision:\t", 0);
	cvs_output (vers->vn_user, 0);
	cvs_output ("\n", 0);
    }
#endif
    else
    {
	cvs_output ("   Working revision:\t", 0);
	cvs_output (vers->vn_user, 0);
	cvs_output ("\t", 0);
	cvs_output (vers->ts_rcs, 0);
	cvs_output ("\n", 0);
    }

    if (vers->vn_rcs == NULL)
	cvs_output ("   Repository revision:\tNo revision control file\n", 0);
    else
    {
	cvs_output ("   Repository revision:\t", 0);
	cvs_output (vers->vn_rcs, 0);
	cvs_output ("\t", 0);
	cvs_output (vers->srcfile->path, 0);
	cvs_output ("\n", 0);
    }

    if (vers->entdata)
    {
	Entnode *edata;

	edata = vers->entdata;
	if (edata->tag)
	{
	    if (vers->vn_rcs == NULL)
	    {
		cvs_output ("   Sticky Tag:\t\t", 0);
		cvs_output (edata->tag, 0);
		cvs_output (" - MISSING from RCS file!\n", 0);
	    }
	    else
	    {
		if (isdigit (edata->tag[0]))
		{
		    cvs_output ("   Sticky Tag:\t\t", 0);
		    cvs_output (edata->tag, 0);
		    cvs_output ("\n", 0);
		}
		else
		{
		    char *branch = NULL;

		    if (RCS_nodeisbranch (finfo->rcs, edata->tag))
			branch = RCS_whatbranch(finfo->rcs, edata->tag);

		    cvs_output ("   Sticky Tag:\t\t", 0);
		    cvs_output (edata->tag, 0);
		    cvs_output (" (", 0);
		    cvs_output (branch ? "branch" : "revision", 0);
		    cvs_output (": ", 0);
		    cvs_output (branch ? branch : vers->vn_rcs, 0);
		    cvs_output (")\n", 0);

		    if (branch)
			free (branch);
		}
	    }
	}
	else if (!really_quiet)
	    cvs_output ("   Sticky Tag:\t\t(none)\n", 0);

	if (edata->date)
	{
	    cvs_output ("   Sticky Date:\t\t", 0);
	    cvs_output (edata->date, 0);
	    cvs_output ("\n", 0);
	}
	else if (!really_quiet)
	    cvs_output ("   Sticky Date:\t\t(none)\n", 0);

	if (edata->options && edata->options[0])
	{
	    cvs_output ("   Sticky Options:\t", 0);
	    cvs_output (edata->options, 0);
	    cvs_output ("\n", 0);
	}
	else if (!really_quiet)
	    cvs_output ("   Sticky Options:\t(none)\n", 0);

	if (long_format && vers->srcfile)
	{
	    List *symbols = RCS_symbols(vers->srcfile);

	    cvs_output ("\n   Existing Tags:\n", 0);
	    if (symbols)
	    {
		xrcsnode = finfo->rcs;
		(void) walklist (symbols, tag_list_proc, NULL);
	    }
	    else
		cvs_output ("\tNo Tags Exist\n", 0);
	}
    }

    cvs_output ("\n", 0);
    freevers_ts (&vers);
    return (0);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
status_dirproc (callerdat, dir, repos, update_dir, entries)
    void *callerdat;
    char *dir;
    char *repos;
    char *update_dir;
    List *entries;
{
    if (!quiet)
	error (0, 0, "Examining %s", update_dir);
    return (R_PROCESS);
}

/*
 * Print out a tag and its type
 */
static int
tag_list_proc (p, closure)
    Node *p;
    void *closure;
{
    char *branch = NULL;
    char *buf;

    if (RCS_nodeisbranch (xrcsnode, p->key))
	branch = RCS_whatbranch(xrcsnode, p->key) ;

    buf = xmalloc (80 + strlen (p->key)
		   + (branch ? strlen (branch) : strlen (p->data)));
    sprintf (buf, "\t%-25s\t(%s: %s)\n", p->key,
	     branch ? "branch" : "revision",
	     branch ? branch : p->data);
    cvs_output (buf, 0);
    free (buf);

    if (branch)
	free (branch);

    return (0);
}
