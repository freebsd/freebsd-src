/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * Status Information
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)status.c 1.48 92/03/31";
#endif

#if __STDC__
static Dtype status_dirproc (char *dir, char *repos, char *update_dir);
static int status_fileproc (char *file, char *update_dir,
			    char *repository, List * entries,
			    List * srcfiles);
static int tag_list_proc (Node * p);
#else
static int tag_list_proc ();
static int status_fileproc ();
static Dtype status_dirproc ();
#endif				/* __STDC__ */

static int local = 0;
static int long_format = 0;

static char *status_usage[] =
{
    "Usage: %s %s [-vlR] [files...]\n",
    "\t-v\tVerbose format; includes tag information for the file\n",
    "\t-l\tProcess this directory only (not recursive).\n",
    "\t-R\tProcess directories recursively.\n",
    NULL
};

int
status (argc, argv)
    int argc;
    char *argv[];
{
    int c;
    int err = 0;

    if (argc == -1)
	usage (status_usage);

    optind = 1;
    while ((c = gnu_getopt (argc, argv, "vlR")) != -1)
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

    /* start the recursion processor */
    err = start_recursion (status_fileproc, (int (*) ()) NULL, status_dirproc,
			   (int (*) ()) NULL, argc, argv, local,
			   W_LOCAL, 0, 1, (char *) NULL, 1);

    return (err);
}

/*
 * display the status of a file
 */
/* ARGSUSED */
static int
status_fileproc (file, update_dir, repository, entries, srcfiles)
    char *file;
    char *update_dir;
    char *repository;
    List *entries;
    List *srcfiles;
{
    Ctype status;
    char *sstat;
    Vers_TS *vers;

    status = Classify_File (file, (char *) NULL, (char *) NULL, (char *) NULL,
			    1, 0, repository, entries, srcfiles, &vers);
    switch (status)
    {
	case T_UNKNOWN:
	    sstat = "Unknown";
	    break;
	case T_CHECKOUT:
	    sstat = "Needs Checkout";
	    break;
	case T_CONFLICT:
	    sstat = "Unresolved Conflict";
	    break;
	case T_ADDED:
	    sstat = "Locally Added";
	    break;
	case T_REMOVED:
	    sstat = "Locally Removed";
	    break;
	case T_MODIFIED:
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
	default:
	    sstat = "Classify Error";
	    break;
    }

    (void) printf ("===================================================================\n");
    if (vers->ts_user == NULL)
	(void) printf ("File: no file %s\t\tStatus: %s\n\n", file, sstat);
    else
	(void) printf ("File: %-17.17s\tStatus: %s\n\n", file, sstat);

    if (vers->vn_user == NULL)
	(void) printf ("    Version:\t\tNo entry for %s\n", file);
    else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
	(void) printf ("    Version:\t\tNew file!\n");
    else
	(void) printf ("    Version:\t\t%s\t%s\n", vers->vn_user,
		       &vers->ts_rcs[25]);

    if (vers->vn_rcs == NULL)
	(void) printf ("    RCS Version:\tNo revision control file\n");
    else
	(void) printf ("    RCS Version:\t%s\t%s\n", vers->vn_rcs,
		       vers->srcfile->path);

    if (vers->entdata)
    {
	Entnode *edata;

	edata = vers->entdata;
	if (edata->tag)
	{
	    if (vers->vn_rcs == NULL)
		(void) printf (
			 "    Sticky Tag:\t\t%s - MISSING from RCS file!\n",
			 edata->tag);
	    else
	    {
		if (isdigit (edata->tag[0]))
		    (void) printf ("    Sticky Tag:\t\t%s\n", edata->tag);
		else
		    (void) printf ("    Sticky Tag:\t\t%s (%s: %s)\n",
				   edata->tag, numdots (vers->vn_rcs) % 2 ?
				   "revision" : "branch", vers->vn_rcs);
	    }
	}
	else
	    (void) printf ("    Sticky Tag:\t\t(none)\n");

	if (edata->date)
	    (void) printf ("    Sticky Date:\t%s\n", edata->date);
	else
	    (void) printf ("    Sticky Date:\t(none)\n");

	if (edata->options && edata->options[0])
	    (void) printf ("    Sticky Options:\t%s\n", edata->options);
	else
	    (void) printf ("    Sticky Options:\t(none)\n");

	if (long_format && vers->srcfile)
	{
	    (void) printf ("\n    Existing Tags:\n");
	    if (vers->srcfile->symbols)
		(void) walklist (vers->srcfile->symbols, tag_list_proc);
	    else
		(void) printf ("\tNo Tags Exist\n");
	}
    }

    (void) printf ("\n");
    freevers_ts (&vers);
    return (0);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
status_dirproc (dir, repos, update_dir)
    char *dir;
    char *repos;
    char *update_dir;
{
    if (!quiet)
	error (0, 0, "Examining %s", update_dir);
    return (R_PROCESS);
}

/*
 * Print out a tag and its type
 */
static int
tag_list_proc (p)
    Node *p;
{
    (void) printf ("\t%-25.25s\t(%s: %s)\n", p->key,
		   numdots (p->data) % 2 ? "revision" : "branch",
		   p->data);
    return (0);
}
