/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 *
 * "update" updates the version in the present directory with respect to the RCS
 * repository.  The present version must have been created by "checkout". The
 * user can keep up-to-date by calling "update" whenever he feels like it.
 *
 * The present version can be committed by "commit", but this keeps the version
 * in tact.
 *
 * Arguments following the options are taken to be file names to be updated,
 * rather than updating the entire directory.
 *
 * Modified or non-existent RCS files are checked out and reported as U
 * <user_file>
 *
 * Modified user files are reported as M <user_file>.  If both the RCS file and
 * the user file have been modified, the user file is replaced by the result
 * of rcsmerge, and a backup file is written for the user in .#file.version.
 * If this throws up irreconcilable differences, the file is reported as C
 * <user_file>, and as M <user_file> otherwise.
 *
 * Files added but not yet committed are reported as A <user_file>. Files
 * removed but not yet committed are reported as R <user_file>.
 *
 * If the current directory contains subdirectories that hold concurrent
 * versions, these are updated too.  If the -d option was specified, new
 * directories added to the repository are automatically created and updated
 * as well.
 *
 * $FreeBSD$
 */

#include "cvs.h"
#include "savecwd.h"
#ifdef SERVER_SUPPORT
#include "md5.h"
#endif
#include "watch.h"
#include "fileattr.h"
#include "edit.h"
#include "getline.h"
#include "buffer.h"
#include "hardlink.h"

static int checkout_file PROTO ((struct file_info *finfo, Vers_TS *vers_ts,
				 int adding, int merging, int update_server));
#ifdef SERVER_SUPPORT
static void checkout_to_buffer PROTO ((void *, const char *, size_t));
#endif
#ifdef SERVER_SUPPORT
static int patch_file PROTO ((struct file_info *finfo,
			      Vers_TS *vers_ts, 
			      int *docheckout, struct stat *file_info,
			      unsigned char *checksum));
static void patch_file_write PROTO ((void *, const char *, size_t));
#endif
static int merge_file PROTO ((struct file_info *finfo, Vers_TS *vers));
static int scratch_file PROTO((struct file_info *finfo));
static Dtype update_dirent_proc PROTO ((void *callerdat, char *dir,
					char *repository, char *update_dir,
					List *entries));
static int update_dirleave_proc PROTO ((void *callerdat, char *dir,
					int err, char *update_dir,
					List *entries));
static int update_fileproc PROTO ((void *callerdat, struct file_info *));
static int update_filesdone_proc PROTO ((void *callerdat, int err,
					 char *repository, char *update_dir,
					 List *entries));
#ifdef PRESERVE_PERMISSIONS_SUPPORT
static int get_linkinfo_proc PROTO ((void *callerdat, struct file_info *));
#endif
static void write_letter PROTO ((struct file_info *finfo, int letter));
static void join_file PROTO ((struct file_info *finfo, Vers_TS *vers_ts));

static char *options = NULL;
static char *tag = NULL;
static char *date = NULL;
/* This is a bit of a kludge.  We call WriteTag at the beginning
   before we know whether nonbranch is set or not.  And then at the
   end, once we have the right value for nonbranch, we call WriteTag
   again.  I don't know whether the first call is necessary or not.
   rewrite_tag is nonzero if we are going to have to make that second
   call.  */
static int rewrite_tag;
static int nonbranch;

/* If we set the tag or date for a subdirectory, we use this to undo
   the setting.  See update_dirent_proc.  */
static char *tag_update_dir;

static char *join_rev1, *date_rev1;
static char *join_rev2, *date_rev2;
static int aflag = 0;
static int toss_local_changes = 0;
static int force_tag_match = 1;
static int pull_template = 0;
static int update_build_dirs = 0;
static int update_prune_dirs = 0;
static int pipeout = 0;
#ifdef SERVER_SUPPORT
static int patches = 0;
static int rcs_diff_patches = 0;
#endif
static List *ignlist = (List *) NULL;
static time_t last_register_time;
static const char *const update_usage[] =
{
    "Usage: %s %s [-APdflRp] [-k kopt] [-r rev|-D date] [-j rev]\n",
    "    [-I ign] [-W spec] [files...]\n",
    "\t-A\tReset any sticky tags/date/kopts.\n",
    "\t-P\tPrune empty directories.\n",
    "\t-C\tOverwrite locally modified files with clean repository copies.\n",
    "\t-d\tBuild directories, like checkout does.\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, no recursion.\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-p\tSend updates to standard output (avoids stickiness).\n",
    "\t-k kopt\tUse RCS kopt -k option on checkout.\n",
    "\t-r rev\tUpdate using specified revision/tag (is sticky).\n",
    "\t-D date\tSet date to update from (is sticky).\n",
    "\t-j rev\tMerge in changes made between current revision and rev.\n",
    "\t-I ign\tMore files to ignore (! to reset).\n",
    "\t-W spec\tWrappers specification line.\n",
    "\t-T\tUpdate CVS/Template.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

/*
 * update is the argv,argc based front end for arg parsing
 */
int
update (argc, argv)
    int argc;
    char **argv;
{
    int c, err;
    int local = 0;			/* recursive by default */
    int which;				/* where to look for files and dirs */
    int xpull_template = 0;

    if (argc == -1)
	usage (update_usage);

    ign_setup ();
    wrap_setup ();

    /* parse the args */
    optind = 0;
    while ((c = getopt (argc, argv, "+ApCPflRQTqduk:r:D:j:I:W:")) != -1)
    {
	switch (c)
	{
	    case 'A':
		aflag = 1;
		break;
	    case 'C':
		toss_local_changes = 1;
		break;
	    case 'I':
		ign_add (optarg, 0);
		break;
	    case 'W':
		wrap_add (optarg, 0);
		break;
	    case 'k':
		if (options)
		    free (options);
		options = RCS_check_kflag (optarg);
		break;
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
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
	    case 'T':
		xpull_template = 1;
		break;
	    case 'd':
		update_build_dirs = 1;
		break;
	    case 'f':
		force_tag_match = 0;
		break;
	    case 'r':
		tag = optarg;
		break;
	    case 'D':
		date = Make_Date (optarg);
		break;
	    case 'P':
		update_prune_dirs = 1;
		break;
	    case 'p':
		pipeout = 1;
		noexec = 1;		/* so no locks will be created */
		break;
	    case 'j':
		if (join_rev2)
		    error (1, 0, "only two -j options can be specified");
		if (join_rev1)
		    join_rev2 = optarg;
		else
		    join_rev1 = optarg;
		break;
	    case 'u':
#ifdef SERVER_SUPPORT
		if (server_active)
		{
		    patches = 1;
		    rcs_diff_patches = server_use_rcs_diff ();
		}
		else
#endif
		    usage (update_usage);
		break;
	    case '?':
	    default:
		usage (update_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

#ifdef CLIENT_SUPPORT
    if (client_active) 
    {
	int pass;

	/* The first pass does the regular update.  If we receive at least
	   one patch which failed, we do a second pass and just fetch
	   those files whose patches failed.  */
	pass = 1;
	do
	{
	    int status;

	    start_server ();

	    if (local)
		send_arg("-l");
	    if (update_build_dirs)
		send_arg("-d");
	    if (pipeout)
		send_arg("-p");
	    if (!force_tag_match)
		send_arg("-f");
	    if (aflag)
		send_arg("-A");
	    if (toss_local_changes)
		send_arg("-C");
	    if (update_prune_dirs)
		send_arg("-P");
	    client_prune_dirs = update_prune_dirs;
	    option_with_arg ("-r", tag);
	    if (options && options[0] != '\0')
		send_arg (options);
	    if (date)
		client_senddate (date);
	    if (join_rev1)
		option_with_arg ("-j", join_rev1);
	    if (join_rev2)
		option_with_arg ("-j", join_rev2);
	    wrap_send ();

	    if (failed_patches_count == 0)
	    {
                unsigned int flags = 0;

		/* If the server supports the command "update-patches", that 
		   means that it knows how to handle the -u argument to update,
		   which means to send patches instead of complete files.

		   We don't send -u if failed_patches != NULL, so that the
		   server doesn't try to send patches which will just fail
		   again.  At least currently, the client also clobbers the
		   file and tells the server it is lost, which also will get
		   a full file instead of a patch, but it seems clean to omit
		   -u.  */
		if (supported_request ("update-patches"))
		    send_arg ("-u");

                if (update_build_dirs)
                    flags |= SEND_BUILD_DIRS;

                if (toss_local_changes) {
                    flags |= SEND_NO_CONTENTS;
                    flags |= BACKUP_MODIFIED_FILES;
                }

		/* If noexec, probably could be setting SEND_NO_CONTENTS.
		   Same caveats as for "cvs status" apply.  */

		send_files (argc, argv, local, aflag, flags);
		send_file_names (argc, argv, SEND_EXPAND_WILD);
	    }
	    else
	    {
		int i;

		(void) printf ("%s client: refetching unpatchable files\n",
			       program_name);

		if (toplevel_wd != NULL
		    && CVS_CHDIR (toplevel_wd) < 0)
		{
		    error (1, errno, "could not chdir to %s", toplevel_wd);
		}

		for (i = 0; i < failed_patches_count; i++)
		    if (unlink_file (failed_patches[i]) < 0
			&& !existence_error (errno))
			error (0, errno, "cannot remove %s",
			       failed_patches[i]);
		send_files (failed_patches_count, failed_patches, local,
			    aflag, update_build_dirs ? SEND_BUILD_DIRS : 0);
		send_file_names (failed_patches_count, failed_patches, 0);
		free_names (&failed_patches_count, failed_patches);
	    }

	    send_to_server ("update\012", 0);

	    status = get_responses_and_close ();

	    /* If there are any conflicts, the server will return a
               non-zero exit status.  If any patches failed, we still
               want to run the update again.  We use a pass count to
               avoid an endless loop.  */

	    /* Notes: (1) assuming that status != 0 implies a
	       potential conflict is the best we can cleanly do given
	       the current protocol.  I suppose that trying to
	       re-fetch in cases where there was a more serious error
	       is probably more or less harmless, but it isn't really
	       ideal.  (2) it would be nice to have a testsuite case for the
	       conflict-and-patch-failed case.  */

	    if (status != 0
		&& (failed_patches_count == 0 || pass > 1))
	    {
		if (failed_patches_count > 0)
		    free_names (&failed_patches_count, failed_patches);
		return status;
	    }

	    ++pass;
	} while (failed_patches_count > 0);

	return 0;
    }
#endif

    if (tag != NULL)
	tag_check_valid (tag, argc, argv, local, aflag, "");
    if (join_rev1 != NULL)
        tag_check_valid_join (join_rev1, argc, argv, local, aflag, "");
    if (join_rev2 != NULL)
        tag_check_valid_join (join_rev2, argc, argv, local, aflag, "");

    /*
     * If we are updating the entire directory (for real) and building dirs
     * as we go, we make sure there is no static entries file and write the
     * tag file as appropriate
     */
    if (argc <= 0 && !pipeout)
    {
	if (update_build_dirs)
	{
	    if (unlink_file (CVSADM_ENTSTAT) < 0 && ! existence_error (errno))
		error (1, errno, "cannot remove file %s", CVSADM_ENTSTAT);
#ifdef SERVER_SUPPORT
	    if (server_active)
	    {
		char *repos = Name_Repository (NULL, NULL);
		server_clear_entstat (".", repos);
		free (repos);
	    }
#endif
	}

	/* keep the CVS/Tag file current with the specified arguments */
	if (aflag || tag || date)
	{
	    char *repos = Name_Repository (NULL, NULL);
	    WriteTag ((char *) NULL, tag, date, 0, ".", repos);
	    free (repos);
	    rewrite_tag = 1;
	    nonbranch = 0;
	}
    }

    /* look for files/dirs locally and in the repository */
    which = W_LOCAL | W_REPOS;

    /* look in the attic too if a tag or date is specified */
    if (tag != NULL || date != NULL || joining())
	which |= W_ATTIC;

    /* call the command line interface */
    err = do_update (argc, argv, options, tag, date, force_tag_match,
		     local, update_build_dirs, aflag, update_prune_dirs,
		     pipeout, which, join_rev1, join_rev2, (char *) NULL,
		     xpull_template);

    /* free the space Make_Date allocated if necessary */
    if (date != NULL)
	free (date);

    return (err);
}

/*
 * Command line interface to update (used by checkout)
 */
int
do_update (argc, argv, xoptions, xtag, xdate, xforce, local, xbuild, xaflag,
	   xprune, xpipeout, which, xjoin_rev1, xjoin_rev2, preload_update_dir,
	   xpull_template)
    int argc;
    char **argv;
    char *xoptions;
    char *xtag;
    char *xdate;
    int xforce;
    int local;
    int xbuild;
    int xaflag;
    int xprune;
    int xpipeout;
    int which;
    char *xjoin_rev1;
    char *xjoin_rev2;
    char *preload_update_dir;
    int xpull_template;
{
    int err = 0;
    char *cp;

    /* fill in the statics */
    options = xoptions;
    tag = xtag;
    date = xdate;
    force_tag_match = xforce;
    update_build_dirs = xbuild;
    aflag = xaflag;
    update_prune_dirs = xprune;
    pipeout = xpipeout;
    pull_template = xpull_template;

    /* setup the join support */
    join_rev1 = xjoin_rev1;
    join_rev2 = xjoin_rev2;
    if (join_rev1 && (cp = strchr (join_rev1, ':')) != NULL)
    {
	*cp++ = '\0';
	date_rev1 = Make_Date (cp);
    }
    else
	date_rev1 = (char *) NULL;
    if (join_rev2 && (cp = strchr (join_rev2, ':')) != NULL)
    {
	*cp++ = '\0';
	date_rev2 = Make_Date (cp);
    }
    else
	date_rev2 = (char *) NULL;

#ifdef PRESERVE_PERMISSIONS_SUPPORT
    if (preserve_perms)
    {
	/* We need to do an extra recursion, bleah.  It's to make sure
	   that we know as much as possible about file linkage. */
	hardlist = getlist();
	working_dir = xgetwd();		/* save top-level working dir */

	/* FIXME-twp: the arguments to start_recursion make me dizzy.  This
	   function call was copied from the update_fileproc call that
	   follows it; someone should make sure that I did it right. */
	err = start_recursion (get_linkinfo_proc, (FILESDONEPROC) NULL,
			       (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			       argc, argv, local, which, aflag, 1,
			       preload_update_dir, 1);
	if (err)
	    return (err);

	/* FIXME-twp: at this point we should walk the hardlist
	   and update the `links' field of each hardlink_info struct
	   to list the files that are linked on dist.  That would make
	   it easier & more efficient to compare the disk linkage with
	   the repository linkage (a simple strcmp). */
    }
#endif

    /* call the recursion processor */
    err = start_recursion (update_fileproc, update_filesdone_proc,
			   update_dirent_proc, update_dirleave_proc, NULL,
			   argc, argv, local, which, aflag, 1,
			   preload_update_dir, 1);

    /* see if we need to sleep before returning to avoid time-stamp races */
    if (last_register_time)
    {
	while (time ((time_t *) NULL) == last_register_time)
	    sleep (1);
    }

    return (err);
}

#ifdef PRESERVE_PERMISSIONS_SUPPORT
/*
 * The get_linkinfo_proc callback adds each file to the hardlist
 * (see hardlink.c).
 */

static int
get_linkinfo_proc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    char *fullpath;
    Node *linkp;
    struct hardlink_info *hlinfo;

    /* Get the full pathname of the current file. */
    fullpath = xmalloc (strlen(working_dir) +
			strlen(finfo->fullname) + 2);
    sprintf (fullpath, "%s/%s", working_dir, finfo->fullname);

    /* To permit recursing into subdirectories, files
       are keyed on the full pathname and not on the basename. */
    linkp = lookup_file_by_inode (fullpath);
    if (linkp == NULL)
    {
	/* The file isn't on disk; we are probably restoring
	   a file that was removed. */
	return 0;
    }
    
    /* Create a new, empty hardlink_info node. */
    hlinfo = (struct hardlink_info *)
	xmalloc (sizeof (struct hardlink_info));

    hlinfo->status = (Ctype) 0;	/* is this dumb? */
    hlinfo->checked_out = 0;

    linkp->data = (char *) hlinfo;

    return 0;
}
#endif

/*
 * This is the callback proc for update.  It is called for each file in each
 * directory by the recursion code.  The current directory is the local
 * instantiation.  file is the file name we are to operate on. update_dir is
 * set to the path relative to where we started (for pretty printing).
 * repository is the repository. entries and srcfiles are the pre-parsed
 * entries and source control files.
 * 
 * This routine decides what needs to be done for each file and does the
 * appropriate magic for checkout
 */
static int
update_fileproc (callerdat, finfo)
    void *callerdat;
    struct file_info *finfo;
{
    int retval;
    Ctype status;
    Vers_TS *vers;
    int resurrecting;

    resurrecting = 0;

    status = Classify_File (finfo, tag, date, options, force_tag_match,
			    aflag, &vers, pipeout);

    /* Keep track of whether TAG is a branch tag.
       Note that if it is a branch tag in some files and a nonbranch tag
       in others, treat it as a nonbranch tag.  It is possible that case
       should elicit a warning or an error.  */
    if (rewrite_tag
	&& tag != NULL
	&& finfo->rcs != NULL)
    {
	char *rev = RCS_getversion (finfo->rcs, tag, date, 1, NULL);
	if (rev != NULL
	    && !RCS_nodeisbranch (finfo->rcs, tag))
	    nonbranch = 1;
	if (rev != NULL)
	    free (rev);
    }

    if (pipeout)
    {
	/*
	 * We just return success without doing anything if any of the really
	 * funky cases occur
	 * 
	 * If there is still a valid RCS file, do a regular checkout type
	 * operation
	 */
	switch (status)
	{
	    case T_UNKNOWN:		/* unknown file was explicitly asked
					 * about */
	    case T_REMOVE_ENTRY:	/* needs to be un-registered */
	    case T_ADDED:		/* added but not committed */
		retval = 0;
		break;
	    case T_CONFLICT:		/* old punt-type errors */
		retval = 1;
		break;
	    case T_UPTODATE:		/* file was already up-to-date */
	    case T_NEEDS_MERGE:		/* needs merging */
	    case T_MODIFIED:		/* locally modified */
	    case T_REMOVED:		/* removed but not committed */
	    case T_CHECKOUT:		/* needs checkout */
#ifdef SERVER_SUPPORT
	    case T_PATCH:		/* needs patch */
#endif
		retval = checkout_file (finfo, vers, 0, 0, 0);
		break;

	    default:			/* can't ever happen :-) */
		error (0, 0,
		       "unknown file status %d for file %s", status, finfo->file);
		retval = 0;
		break;
	}
    }
    else
    {
	switch (status)
	{
	    case T_UNKNOWN:		/* unknown file was explicitly asked
					 * about */
	    case T_UPTODATE:		/* file was already up-to-date */
		retval = 0;
		break;
	    case T_CONFLICT:		/* old punt-type errors */
		retval = 1;
		write_letter (finfo, 'C');
		break;
	    case T_NEEDS_MERGE:		/* needs merging */
		retval = merge_file (finfo, vers);
		break;
	    case T_MODIFIED:		/* locally modified */
		retval = 0;
                if (toss_local_changes)
                {
                    char *bakname;
                    bakname = backup_file (finfo->file, vers->vn_user);
                    /* This behavior is sufficiently unexpected to
                       justify overinformativeness, I think. */
#ifdef SERVER_SUPPORT
                    if ((! really_quiet) && (! server_active))
#else /* ! SERVER_SUPPORT */
                    if (! really_quiet)
#endif /* SERVER_SUPPORT */
                        (void) printf ("(Locally modified %s moved to %s)\n",
                                       finfo->file, bakname);
                    free (bakname);

                    /* The locally modified file is still present, but
                       it will be overwritten by the repository copy
                       after this. */
                    status = T_CHECKOUT;
                    retval = checkout_file (finfo, vers, 0, 0, 1);
                }
                else 
                {
                    if (vers->ts_conflict)
                    {
                        char *filestamp;
                        int retcode;

                        /*
                         * If the timestamp has changed and no
                         * conflict indicators are found, it isn't a
                         * 'C' any more.
                         */

#ifdef SERVER_SUPPORT
                        if (server_active)
                            retcode = vers->ts_conflict[0] != '=';
                        else 
                        {
                            filestamp = time_stamp (finfo->file);
                            retcode = strcmp (vers->ts_conflict, filestamp);
                            free (filestamp);
                        }
#else
                        filestamp = time_stamp (finfo->file);
                        retcode = strcmp (vers->ts_conflict, filestamp);
                        free (filestamp);
#endif

                        if (retcode)
                        {
                            /* The timestamps differ.  But if there
                               are conflict markers print 'C' anyway.  */
                            retcode = !file_has_markers (finfo);
                        }

                        if (!retcode)
                        {
                            write_letter (finfo, 'C');
                            retval = 1;
                        }
                        else
                        {
                            /* Reregister to clear conflict flag. */
                            Register (finfo->entries, finfo->file, 
                                      vers->vn_rcs, vers->ts_rcs,
                                      vers->options, vers->tag,
                                      vers->date, (char *)0);
                        }
                    }
                    if (!retval)
                    {
                        write_letter (finfo, 'M');
                        retval = 0;
                    }
                }
		break;
#ifdef SERVER_SUPPORT
	    case T_PATCH:		/* needs patch */
		if (patches)
		{
		    int docheckout;
		    struct stat file_info;
		    unsigned char checksum[16];

		    retval = patch_file (finfo,
					 vers, &docheckout,
					 &file_info, checksum);
		    if (! docheckout)
		    {
		        if (server_active && retval == 0)
			    server_updated (finfo, vers,
					    (rcs_diff_patches
					     ? SERVER_RCS_DIFF
					     : SERVER_PATCHED),
					    file_info.st_mode, checksum,
					    (struct buffer *) NULL);
			break;
		    }
		}
		/* If we're not running as a server, just check the
		   file out.  It's simpler and faster than producing
		   and applying patches.  */
		/* Fall through.  */
#endif
	    case T_CHECKOUT:		/* needs checkout */
		retval = checkout_file (finfo, vers, 0, 0, 1);
		break;
	    case T_ADDED:		/* added but not committed */
		write_letter (finfo, 'A');
		retval = 0;
		break;
	    case T_REMOVED:		/* removed but not committed */
		write_letter (finfo, 'R');
		retval = 0;
		break;
	    case T_REMOVE_ENTRY:	/* needs to be un-registered */
		retval = scratch_file (finfo);
#ifdef SERVER_SUPPORT
		if (server_active && retval == 0)
		{
		    if (vers->ts_user == NULL)
			server_scratch_entry_only ();
		    server_updated (finfo, vers,
				    SERVER_UPDATED, (mode_t) -1,
				    (unsigned char *) NULL,
				    (struct buffer *) NULL);
		}
#endif
		break;
	    default:			/* can't ever happen :-) */
		error (0, 0,
		       "unknown file status %d for file %s", status, finfo->file);
		retval = 0;
		break;
	}
    }

    /* only try to join if things have gone well thus far */
    if (retval == 0 && join_rev1)
	join_file (finfo, vers);

    /* if this directory has an ignore list, add this file to it */
    if (ignlist)
    {
	Node *p;

	p = getnode ();
	p->type = FILES;
	p->key = xstrdup (finfo->file);
	if (addnode (ignlist, p) != 0)
	    freenode (p);
    }

    freevers_ts (&vers);
    return (retval);
}

static void update_ignproc PROTO ((char *, char *));

static void
update_ignproc (file, dir)
    char *file;
    char *dir;
{
    struct file_info finfo;

    memset (&finfo, 0, sizeof (finfo));
    finfo.file = file;
    finfo.update_dir = dir;
    if (dir[0] == '\0')
	finfo.fullname = xstrdup (file);
    else
    {
	finfo.fullname = xmalloc (strlen (file) + strlen (dir) + 10);
	strcpy (finfo.fullname, dir);
	strcat (finfo.fullname, "/");
	strcat (finfo.fullname, file);
    }

    write_letter (&finfo, '?');
    free (finfo.fullname);
}

/* ARGSUSED */
static int
update_filesdone_proc (callerdat, err, repository, update_dir, entries)
    void *callerdat;
    int err;
    char *repository;
    char *update_dir;
    List *entries;
{
    if (rewrite_tag)
    {
	WriteTag (NULL, tag, date, nonbranch, update_dir, repository);
	rewrite_tag = 0;
    }

    /* if this directory has an ignore list, process it then free it */
    if (ignlist)
    {
	ignore_files (ignlist, entries, update_dir, update_ignproc);
	dellist (&ignlist);
    }

    /* Clean up CVS admin dirs if we are export */
    if (strcmp (command_name, "export") == 0)
    {
	/* I'm not sure the existence_error is actually possible (except
	   in cases where we really should print a message), but since
	   this code used to ignore all errors, I'll play it safe.  */
	if (unlink_file_dir (CVSADM) < 0 && !existence_error (errno))
	    error (0, errno, "cannot remove %s directory", CVSADM);
    }
#ifdef SERVER_SUPPORT
    else if (!server_active && !pipeout)
#else
    else if (!pipeout)
#endif /* SERVER_SUPPORT */
    {
        /* If there is no CVS/Root file, add one */
        if (!isfile (CVSADM_ROOT))
	    Create_Root ((char *) NULL, CVSroot_original);
    }

    return (err);
}

/*
 * update_dirent_proc () is called back by the recursion processor before a
 * sub-directory is processed for update.  In this case, update_dirent proc
 * will probably create the directory unless -d isn't specified and this is a
 * new directory.  A return code of 0 indicates the directory should be
 * processed by the recursion code.  A return of non-zero indicates the
 * recursion code should skip this directory.
 */
static Dtype
update_dirent_proc (callerdat, dir, repository, update_dir, entries)
    void *callerdat;
    char *dir;
    char *repository;
    char *update_dir;
    List *entries;
{
    if (ignore_directory (update_dir))
    {
	/* print the warm fuzzy message */
	if (!quiet)
	  error (0, 0, "Ignoring %s", update_dir);
        return R_SKIP_ALL;
    }

    if (!isdir (dir))
    {
	/* if we aren't building dirs, blow it off */
	if (!update_build_dirs)
	    return (R_SKIP_ALL);

	/* Various CVS administrators are in the habit of removing
	   the repository directory for things they don't want any
	   more.  I've even been known to do it myself (on rare
	   occasions).  Not the usual recommended practice, but we
	   want to try to come up with some kind of
	   reasonable/documented/sensible behavior.  Generally
	   the behavior is to just skip over that directory (see
	   dirs test in sanity.sh; the case which reaches here
	   is when update -d is specified, and the working directory
	   is gone but the subdirectory is still mentioned in
	   CVS/Entries).  */
	if (1
#ifdef SERVER_SUPPORT
	    /* In the remote case, the client should refrain from
	       sending us the directory in the first place.  So we
	       want to continue to give an error, so clients make
	       sure to do this.  */
	    && !server_active
#endif
	    && !isdir (repository))
	    return R_SKIP_ALL;

	if (noexec)
	{
	    /* ignore the missing dir if -n is specified */
	    error (0, 0, "New directory `%s' -- ignored", update_dir);
	    return (R_SKIP_ALL);
	}
	else
	{
	    /* otherwise, create the dir and appropriate adm files */

	    /* If no tag or date were specified on the command line,
               and we're not using -A, we want the subdirectory to use
               the tag and date, if any, of the current directory.
               That way, update -d will work correctly when working on
               a branch.

	       We use TAG_UPDATE_DIR to undo the tag setting in
	       update_dirleave_proc.  If we did not do this, we would
	       not correctly handle a working directory with multiple
	       tags (and maybe we should prohibit such working
	       directories, but they work now and we shouldn't make
	       them stop working without more thought).  */
	    if ((tag == NULL && date == NULL) && ! aflag)
	    {
		ParseTag (&tag, &date, &nonbranch);
		if (tag != NULL || date != NULL)
		    tag_update_dir = xstrdup (update_dir);
	    }

	    make_directory (dir);
	    Create_Admin (dir, update_dir, repository, tag, date,
			  /* This is a guess.  We will rewrite it later
			     via WriteTag.  */
			  0,
			  0,
			  1);
	    rewrite_tag = 1;
	    nonbranch = 0;
	    Subdir_Register (entries, (char *) NULL, dir);
	}
    }
    /* Do we need to check noexec here? */
    else if (!pipeout)
    {
	char *cvsadmdir;

	/* The directory exists.  Check to see if it has a CVS
	   subdirectory.  */

	cvsadmdir = xmalloc (strlen (dir) + 80);
	strcpy (cvsadmdir, dir);
	strcat (cvsadmdir, "/");
	strcat (cvsadmdir, CVSADM);

	if (!isdir (cvsadmdir))
	{
	    /* We cannot successfully recurse into a directory without a CVS
	       subdirectory.  Generally we will have already printed
	       "? foo".  */
	    free (cvsadmdir);
	    return R_SKIP_ALL;
	}
	free (cvsadmdir);
    }

    /*
     * If we are building dirs and not going to stdout, we make sure there is
     * no static entries file and write the tag file as appropriate
     */
    if (!pipeout)
    {
	if (update_build_dirs)
	{
	    char *tmp;

	    tmp = xmalloc (strlen (dir) + sizeof (CVSADM_ENTSTAT) + 10);
	    (void) sprintf (tmp, "%s/%s", dir, CVSADM_ENTSTAT);
	    if (unlink_file (tmp) < 0 && ! existence_error (errno))
		error (1, errno, "cannot remove file %s", tmp);
#ifdef SERVER_SUPPORT
	    if (server_active)
		server_clear_entstat (update_dir, repository);
#endif
	    free (tmp);
	}

	/* keep the CVS/Tag file current with the specified arguments */
	if (aflag || tag || date)
	{
	    WriteTag (dir, tag, date, 0, update_dir, repository);
	    rewrite_tag = 1;
	    nonbranch = 0;
	}

	/* keep the CVS/Template file current */
	if (pull_template) 
	{
	    WriteTemplate (dir, update_dir);
	}

	/* initialize the ignore list for this directory */
	ignlist = getlist ();
    }

    /* print the warm fuzzy message */
    if (!quiet)
	error (0, 0, "Updating %s", update_dir);

    return (R_PROCESS);
}

/*
 * update_dirleave_proc () is called back by the recursion code upon leaving
 * a directory.  It will prune empty directories if needed and will execute
 * any appropriate update programs.
 */
/* ARGSUSED */
static int
update_dirleave_proc (callerdat, dir, err, update_dir, entries)
    void *callerdat;
    char *dir;
    int err;
    char *update_dir;
    List *entries;
{
    FILE *fp;

    /* Delete the ignore list if it hasn't already been done.  */
    if (ignlist)
	dellist (&ignlist);

    /* If we set the tag or date for a new subdirectory in
       update_dirent_proc, and we're now done with that subdirectory,
       undo the tag/date setting.  Note that we know that the tag and
       date were both originally NULL in this case.  */
    if (tag_update_dir != NULL && strcmp (update_dir, tag_update_dir) == 0)
    {
	if (tag != NULL)
	{
	    free (tag);
	    tag = NULL;
	}
	if (date != NULL)
	{
	    free (date);
	    date = NULL;
	}
	nonbranch = 0;
	free (tag_update_dir);
	tag_update_dir = NULL;
    }

    /* run the update_prog if there is one */
    /* FIXME: should be checking for errors from CVS_FOPEN and printing
       them if not existence_error.  */
    if (err == 0 && !pipeout && !noexec &&
	(fp = CVS_FOPEN (CVSADM_UPROG, "r")) != NULL)
    {
	char *cp;
	char *repository;
	char *line = NULL;
	size_t line_allocated = 0;

	repository = Name_Repository ((char *) NULL, update_dir);
	if (getline (&line, &line_allocated, fp) >= 0)
	{
	    if ((cp = strrchr (line, '\n')) != NULL)
		*cp = '\0';
	    run_setup (line);
	    run_arg (repository);
	    cvs_output (program_name, 0);
	    cvs_output (" ", 1);
	    cvs_output (command_name, 0);
	    cvs_output (": Executing '", 0);
	    run_print (stdout);
	    cvs_output ("'\n", 0);
	    cvs_flushout ();
	    (void) run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
	}
	else if (ferror (fp))
	    error (0, errno, "cannot read %s", CVSADM_UPROG);
	else
	    error (0, 0, "unexpected end of file on %s", CVSADM_UPROG);

	if (fclose (fp) < 0)
	    error (0, errno, "cannot close %s", CVSADM_UPROG);
	if (line != NULL)
	    free (line);
	free (repository);
    }

    if (strchr (dir, '/') == NULL)
    {
	/* FIXME: chdir ("..") loses with symlinks.  */
	/* Prune empty dirs on the way out - if necessary */
	(void) CVS_CHDIR ("..");
	if (update_prune_dirs && isemptydir (dir, 0))
	{
	    /* I'm not sure the existence_error is actually possible (except
	       in cases where we really should print a message), but since
	       this code used to ignore all errors, I'll play it safe.	*/
	    if (unlink_file_dir (dir) < 0 && !existence_error (errno))
		error (0, errno, "cannot remove %s directory", dir);
	    Subdir_Deregister (entries, (char *) NULL, dir);
	}
    }

    return (err);
}

static int isremoved PROTO ((Node *, void *));

/* Returns 1 if the file indicated by node has been removed.  */
static int
isremoved (node, closure)
    Node *node;
    void *closure;
{
    Entnode *entdata = (Entnode*) node->data;

    /* If the first character of the version is a '-', the file has been
       removed. */
    return (entdata->version && entdata->version[0] == '-') ? 1 : 0;
}

/* Returns 1 if the argument directory is completely empty, other than the
   existence of the CVS directory entry.  Zero otherwise.  If MIGHT_NOT_EXIST
   and the directory doesn't exist, then just return 0.  */
int
isemptydir (dir, might_not_exist)
    char *dir;
    int might_not_exist;
{
    DIR *dirp;
    struct dirent *dp;

    if ((dirp = CVS_OPENDIR (dir)) == NULL)
    {
	if (might_not_exist && existence_error (errno))
	    return 0;
	error (0, errno, "cannot open directory %s for empty check", dir);
	return (0);
    }
    errno = 0;
    while ((dp = readdir (dirp)) != NULL)
    {
	if (strcmp (dp->d_name, ".") != 0
	    && strcmp (dp->d_name, "..") != 0)
	{
	    if (strcmp (dp->d_name, CVSADM) != 0)
	    {
		/* An entry other than the CVS directory.  The directory
		   is certainly not empty. */
		(void) closedir (dirp);
		return (0);
	    }
	    else
	    {
		/* The CVS directory entry.  We don't have to worry about
		   this unless the Entries file indicates that files have
		   been removed, but not committed, in this directory.
		   (Removing the directory would prevent people from
		   comitting the fact that they removed the files!) */
		List *l;
		int files_removed;
		struct saved_cwd cwd;

		if (save_cwd (&cwd))
		    error_exit ();

		if (CVS_CHDIR (dir) < 0)
		    error (1, errno, "cannot change directory to %s", dir);
		l = Entries_Open (0, NULL);
		files_removed = walklist (l, isremoved, 0);
		Entries_Close (l);

		if (restore_cwd (&cwd, NULL))
		    error_exit ();
		free_cwd (&cwd);

		if (files_removed != 0)
		{
		    /* There are files that have been removed, but not
		       committed!  Do not consider the directory empty. */
		    (void) closedir (dirp);
		    return (0);
		}
	    }
	}
	errno = 0;
    }
    if (errno != 0)
    {
	error (0, errno, "cannot read directory %s", dir);
	(void) closedir (dirp);
	return (0);
    }
    (void) closedir (dirp);
    return (1);
}

/*
 * scratch the Entries file entry associated with a file
 */
static int
scratch_file (finfo)
    struct file_info *finfo;
{
    history_write ('W', finfo->update_dir, "", finfo->file, finfo->repository);
    Scratch_Entry (finfo->entries, finfo->file);
    if (unlink_file (finfo->file) < 0 && ! existence_error (errno))
	error (0, errno, "unable to remove %s", finfo->fullname);
    return (0);
}

/*
 * Check out a file.
 */
static int
checkout_file (finfo, vers_ts, adding, merging, update_server)
    struct file_info *finfo;
    Vers_TS *vers_ts;
    int adding;
    int merging;
    int update_server;
{
    char *backup;
    int set_time, retval = 0;
    int status;
    int file_is_dead;
    struct buffer *revbuf;

    backup = NULL;
    revbuf = NULL;

    /* Don't screw with backup files if we're going to stdout, or if
       we are the server.  */
    if (!pipeout
#ifdef SERVER_SUPPORT
	&& ! server_active
#endif
	)
    {
	backup = xmalloc (strlen (finfo->file)
			  + sizeof (CVSADM)
			  + sizeof (CVSPREFIX)
			  + 10);
	(void) sprintf (backup, "%s/%s%s", CVSADM, CVSPREFIX, finfo->file);
	if (isfile (finfo->file))
	    rename_file (finfo->file, backup);
	else
	{
	    /* If -f/-t wrappers are being used to wrap up a directory,
	       then backup might be a directory instead of just a file.  */
	    if (unlink_file_dir (backup) < 0)
	    {
		/* Not sure if the existence_error check is needed here.  */
		if (!existence_error (errno))
		    /* FIXME: should include update_dir in message.  */
		    error (0, errno, "error removing %s", backup);
	    }
	    free (backup);
	    backup = NULL;
	}
    }

    file_is_dead = RCS_isdead (vers_ts->srcfile, vers_ts->vn_rcs);

    if (!file_is_dead)
    {
	/*
	 * if we are checking out to stdout, print a nice message to
	 * stderr, and add the -p flag to the command */
	if (pipeout)
	{
	    if (!quiet)
	    {
		cvs_outerr ("\
===================================================================\n\
Checking out ", 0);
		cvs_outerr (finfo->fullname, 0);
		cvs_outerr ("\n\
RCS:  ", 0);
		cvs_outerr (vers_ts->srcfile->path, 0);
		cvs_outerr ("\n\
VERS: ", 0);
		cvs_outerr (vers_ts->vn_rcs, 0);
		cvs_outerr ("\n***************\n", 0);
	    }
	}

#ifdef SERVER_SUPPORT
	if (update_server
	    && server_active
	    && ! pipeout
	    && ! file_gzip_level
	    && ! joining ()
	    && ! wrap_name_has (finfo->file, WRAP_FROMCVS))
	{
	    revbuf = buf_nonio_initialize ((BUFMEMERRPROC) NULL);
	    status = RCS_checkout (vers_ts->srcfile, (char *) NULL,
				   vers_ts->vn_rcs, vers_ts->vn_tag,
				   vers_ts->options, RUN_TTY,
				   checkout_to_buffer, revbuf);
	}
	else
#endif
	    status = RCS_checkout (vers_ts->srcfile,
				   pipeout ? NULL : finfo->file,
				   vers_ts->vn_rcs, vers_ts->vn_tag,
				   vers_ts->options, RUN_TTY,
				   (RCSCHECKOUTPROC) NULL, (void *) NULL);
    }
    if (file_is_dead || status == 0)
    {
	mode_t mode;

	mode = (mode_t) -1;

	if (!pipeout)
	{
	    Vers_TS *xvers_ts;

	    if (revbuf != NULL && !noexec)
	    {
		struct stat sb;

		/* FIXME: We should have RCS_checkout return the mode.
		   That would also fix the kludge with noexec, above, which
		   is here only because noexec doesn't write srcfile->path
		   for us to stat.  */
		if (stat (vers_ts->srcfile->path, &sb) < 0)
		    error (1, errno, "cannot stat %s",
			   vers_ts->srcfile->path);
		mode = sb.st_mode &~ (S_IWRITE | S_IWGRP | S_IWOTH);
	    }

	    if (cvswrite
		&& !file_is_dead
		&& !fileattr_get (finfo->file, "_watched"))
	    {
		if (revbuf == NULL)
		    xchmod (finfo->file, 1);
		else
		{
		    /* We know that we are the server here, so
                       although xchmod checks umask, we don't bother.  */
		    mode |= (((mode & S_IRUSR) ? S_IWUSR : 0)
			     | ((mode & S_IRGRP) ? S_IWGRP : 0)
			     | ((mode & S_IROTH) ? S_IWOTH : 0));
		}
	    }

	    {
		/* A newly checked out file is never under the spell
		   of "cvs edit".  If we think we were editing it
		   from a previous life, clean up.  Would be better to
		   check for same the working directory instead of
		   same user, but that is hairy.  */

		struct addremove_args args;

		editor_set (finfo->file, getcaller (), NULL);

		memset (&args, 0, sizeof args);
		args.remove_temp = 1;
		watch_modify_watchers (finfo->file, &args);
	    }

	    /* set the time from the RCS file iff it was unknown before */
	    set_time =
		(!noexec
		 && (vers_ts->vn_user == NULL ||
		     strncmp (vers_ts->ts_rcs, "Initial", 7) == 0)
		 && !file_is_dead);

	    wrap_fromcvs_process_file (finfo->file);

	    xvers_ts = Version_TS (finfo, options, tag, date, 
				   force_tag_match, set_time);
	    if (strcmp (xvers_ts->options, "-V4") == 0)
		xvers_ts->options[0] = '\0';

	    if (revbuf != NULL)
	    {
		/* If we stored the file data into a buffer, then we
                   didn't create a file at all, so xvers_ts->ts_user
                   is wrong.  The correct value is to have it be the
                   same as xvers_ts->ts_rcs, meaning that the working
                   file is unchanged from the RCS file.

		   FIXME: We should tell Version_TS not to waste time
		   statting the nonexistent file.

		   FIXME: Actually, I don't think the ts_user value
		   matters at all here.  The only use I know of is
		   that it is printed in a trace message by
		   Server_Register.  */

		if (xvers_ts->ts_user != NULL)
		    free (xvers_ts->ts_user);
		xvers_ts->ts_user = xstrdup (xvers_ts->ts_rcs);
	    }

	    (void) time (&last_register_time);

	    if (file_is_dead)
	    {
		if (xvers_ts->vn_user != NULL)
		{
		    error (0, 0,
			   "warning: %s is not (any longer) pertinent",
 			   finfo->fullname);
		}
		Scratch_Entry (finfo->entries, finfo->file);
#ifdef SERVER_SUPPORT
		if (server_active && xvers_ts->ts_user == NULL)
		    server_scratch_entry_only ();
#endif
		/* FIXME: Rather than always unlink'ing, and ignoring the
		   existence_error, we should do the unlink only if
		   vers_ts->ts_user is non-NULL.  Then there would be no
		   need to ignore an existence_error (for example, if the
		   user removes the file while we are running).  */
		if (unlink_file (finfo->file) < 0 && ! existence_error (errno))
		{
		    error (0, errno, "cannot remove %s", finfo->fullname);
		}
	    }
	    else
		Register (finfo->entries, finfo->file,
			  adding ? "0" : xvers_ts->vn_rcs,
			  xvers_ts->ts_user, xvers_ts->options,
			  xvers_ts->tag, xvers_ts->date,
			  (char *)0); /* Clear conflict flag on fresh checkout */

	    /* fix up the vers structure, in case it is used by join */
	    if (join_rev1)
	    {
		if (vers_ts->vn_user != NULL)
		    free (vers_ts->vn_user);
		if (vers_ts->vn_rcs != NULL)
		    free (vers_ts->vn_rcs);
		vers_ts->vn_user = xstrdup (xvers_ts->vn_rcs);
		vers_ts->vn_rcs = xstrdup (xvers_ts->vn_rcs);
	    }

	    /* If this is really Update and not Checkout, recode history */
	    if (strcmp (command_name, "update") == 0)
		history_write ('U', finfo->update_dir, xvers_ts->vn_rcs, finfo->file,
			       finfo->repository);

	    freevers_ts (&xvers_ts);

	    if (!really_quiet && !file_is_dead)
	    {
		write_letter (finfo, 'U');
	    }
	}

#ifdef SERVER_SUPPORT
	if (update_server && server_active)
	    server_updated (finfo, vers_ts,
			    merging ? SERVER_MERGED : SERVER_UPDATED,
			    mode, (unsigned char *) NULL, revbuf);
#endif
    }
    else
    {
	if (backup != NULL)
	{
	    rename_file (backup, finfo->file);
	    free (backup);
	    backup = NULL;
	}

	error (0, 0, "could not check out %s", finfo->fullname);

	retval = status;
    }

    if (backup != NULL)
    {
	/* If -f/-t wrappers are being used to wrap up a directory,
	   then backup might be a directory instead of just a file.  */
	if (unlink_file_dir (backup) < 0)
	{
	    /* Not sure if the existence_error check is needed here.  */
	    if (!existence_error (errno))
		/* FIXME: should include update_dir in message.  */
		error (0, errno, "error removing %s", backup);
	}
	free (backup);
    }

    return (retval);
}

#ifdef SERVER_SUPPORT

/* This function is used to write data from a file being checked out
   into a buffer.  */

static void
checkout_to_buffer (callerdat, data, len)
     void *callerdat;
     const char *data;
     size_t len;
{
    struct buffer *buf = (struct buffer *) callerdat;

    buf_output (buf, data, len);
}

#endif /* SERVER_SUPPORT */

#ifdef SERVER_SUPPORT

/* This structure is used to pass information between patch_file and
   patch_file_write.  */

struct patch_file_data
{
    /* File name, for error messages.  */
    const char *filename;
    /* File to which to write.  */
    FILE *fp;
    /* Whether to compute the MD5 checksum.  */
    int compute_checksum;
    /* Data structure for computing the MD5 checksum.  */
    struct cvs_MD5Context context;
    /* Set if the file has a final newline.  */
    int final_nl;
};

/* Patch a file.  Runs diff.  This is only done when running as the
 * server.  The hope is that the diff will be smaller than the file
 * itself.
 */
static int
patch_file (finfo, vers_ts, docheckout, file_info, checksum)
    struct file_info *finfo;
    Vers_TS *vers_ts;
    int *docheckout;
    struct stat *file_info;
    unsigned char *checksum;
{
    char *backup;
    char *file1;
    char *file2;
    int retval = 0;
    int retcode = 0;
    int fail;
    FILE *e;
    struct patch_file_data data;

    *docheckout = 0;

    if (noexec || pipeout || joining ())
    {
	*docheckout = 1;
	return 0;
    }

    /* If this file has been marked as being binary, then never send a
       patch.  */
    if (strcmp (vers_ts->options, "-kb") == 0)
    {
	*docheckout = 1;
	return 0;
    }

    /* First check that the first revision exists.  If it has been nuked
       by cvs admin -o, then just fall back to checking out entire
       revisions.  In some sense maybe we don't have to do this; after
       all cvs.texinfo says "Make sure that no-one has checked out a
       copy of the revision you outdate" but then again, that advice
       doesn't really make complete sense, because "cvs admin" operates
       on a working directory and so _someone_ will almost always have
       _some_ revision checked out.  */
    {
	char *rev;

	rev = RCS_gettag (finfo->rcs, vers_ts->vn_user, 1, NULL);
	if (rev == NULL)
	{
	    *docheckout = 1;
	    return 0;
	}
	else
	    free (rev);
    }

    /* If the revision is dead, let checkout_file handle it rather
       than duplicating the processing here.  */
    if (RCS_isdead (vers_ts->srcfile, vers_ts->vn_rcs))
    {
	*docheckout = 1;
	return 0;
    }

    backup = xmalloc (strlen (finfo->file)
		      + sizeof (CVSADM)
		      + sizeof (CVSPREFIX)
		      + 10);
    (void) sprintf (backup, "%s/%s%s", CVSADM, CVSPREFIX, finfo->file);
    if (isfile (finfo->file))
        rename_file (finfo->file, backup);
    else
    {
	if (unlink_file (backup) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", backup);
    }

    file1 = xmalloc (strlen (finfo->file)
		     + sizeof (CVSADM)
		     + sizeof (CVSPREFIX)
		     + 10);
    (void) sprintf (file1, "%s/%s%s-1", CVSADM, CVSPREFIX, finfo->file);
    file2 = xmalloc (strlen (finfo->file)
		     + sizeof (CVSADM)
		     + sizeof (CVSPREFIX)
		     + 10);
    (void) sprintf (file2, "%s/%s%s-2", CVSADM, CVSPREFIX, finfo->file);

    fail = 0;

    /* We need to check out both revisions first, to see if either one
       has a trailing newline.  Because of this, we don't use rcsdiff,
       but just use diff.  */

    e = CVS_FOPEN (file1, "w");
    if (e == NULL)
	error (1, errno, "cannot open %s", file1);

    data.filename = file1;
    data.fp = e;
    data.final_nl = 0;
    data.compute_checksum = 0;

    retcode = RCS_checkout (vers_ts->srcfile, (char *) NULL,
			    vers_ts->vn_user, (char *) NULL,
			    vers_ts->options, RUN_TTY,
			    patch_file_write, (void *) &data);

    if (fclose (e) < 0)
	error (1, errno, "cannot close %s", file1);

    if (retcode != 0 || ! data.final_nl)
	fail = 1;

    if (! fail)
    {
	e = CVS_FOPEN (file2, "w");
	if (e == NULL)
	    error (1, errno, "cannot open %s", file2);

	data.filename = file2;
	data.fp = e;
	data.final_nl = 0;
	data.compute_checksum = 1;
	cvs_MD5Init (&data.context);

	retcode = RCS_checkout (vers_ts->srcfile, (char *) NULL,
				vers_ts->vn_rcs, vers_ts->vn_tag,
				vers_ts->options, RUN_TTY,
				patch_file_write, (void *) &data);

	if (fclose (e) < 0)
	    error (1, errno, "cannot close %s", file2);

	if (retcode != 0 || ! data.final_nl)
	    fail = 1;
	else
	    cvs_MD5Final (checksum, &data.context);
    }	  

    retcode = 0;
    if (! fail)
    {
	char *diff_options;

	/* If the client does not support the Rcs-diff command, we
           send a context diff, and the client must invoke patch.
           That approach was problematical for various reasons.  The
           new approach only requires running diff in the server; the
           client can handle everything without invoking an external
           program.  */
	if (! rcs_diff_patches)
	{
	    /* We use -c, not -u, because that is what CVS has
	       traditionally used.  Kind of a moot point, now that
	       Rcs-diff is preferred, so there is no point in making
	       the compatibility issues worse.  */
	    diff_options = "-c";
	}
	else
	{
	    /* Now that diff is librarified, we could be passing -a if
	       we wanted to.  However, it is unclear to me whether we
	       would want to.  Does diff -a, in any significant
	       percentage of cases, produce patches which are smaller
	       than the files it is patching?  I guess maybe text
	       files with character sets which diff regards as
	       'binary'.  Conversely, do they tend to be much larger
	       in the bad cases?  This needs some more
	       thought/investigation, I suspect.  */

	    diff_options = "-n";
	}
	retcode = diff_exec (file1, file2, diff_options, finfo->file);

	/* A retcode of 0 means no differences.  1 means some differences.  */
	if (retcode != 0
	    && retcode != 1)
	{
	    fail = 1;
	}
	else
	{
#define BINARY "Binary"
	    char buf[sizeof BINARY];
	    unsigned int c;

	    /* Stat the original RCS file, and then adjust it the way
	       that RCS_checkout would.  FIXME: This is an abstraction
	       violation.  */
	    if (CVS_STAT (vers_ts->srcfile->path, file_info) < 0)
		error (1, errno, "could not stat %s", vers_ts->srcfile->path);
	    if (chmod (finfo->file,
		       file_info->st_mode & ~(S_IWRITE | S_IWGRP | S_IWOTH))
		< 0)
		error (0, errno, "cannot change mode of file %s", finfo->file);
	    if (cvswrite
		&& !fileattr_get (finfo->file, "_watched"))
		xchmod (finfo->file, 1);

	    /* Check the diff output to make sure patch will be handle it.  */
	    e = CVS_FOPEN (finfo->file, "r");
	    if (e == NULL)
		error (1, errno, "could not open diff output file %s",
		       finfo->fullname);
	    c = fread (buf, 1, sizeof BINARY - 1, e);
	    buf[c] = '\0';
	    if (strcmp (buf, BINARY) == 0)
	    {
		/* These are binary files.  We could use diff -a, but
		   patch can't handle that.  */
		fail = 1;
	    }
	    fclose (e);
	}
    }

    if (! fail)
    {
        Vers_TS *xvers_ts;

        /* This stuff is just copied blindly from checkout_file.  I
	   don't really know what it does.  */
        xvers_ts = Version_TS (finfo, options, tag, date,
			       force_tag_match, 0);
	if (strcmp (xvers_ts->options, "-V4") == 0)
	    xvers_ts->options[0] = '\0';

	Register (finfo->entries, finfo->file, xvers_ts->vn_rcs,
		  xvers_ts->ts_user, xvers_ts->options,
		  xvers_ts->tag, xvers_ts->date, NULL);

	if (CVS_STAT (finfo->file, file_info) < 0)
	    error (1, errno, "could not stat %s", finfo->file);

	/* If this is really Update and not Checkout, recode history */
	if (strcmp (command_name, "update") == 0)
	    history_write ('P', finfo->update_dir, xvers_ts->vn_rcs, finfo->file,
			   finfo->repository);

	freevers_ts (&xvers_ts);

	if (!really_quiet)
	{
	    write_letter (finfo, 'P');
	}
    }
    else
    {
	int old_errno = errno;		/* save errno value over the rename */

	if (isfile (backup))
	    rename_file (backup, finfo->file);

	if (retcode != 0 && retcode != 1)
	    error (retcode == -1 ? 1 : 0, retcode == -1 ? old_errno : 0,
		   "could not diff %s", finfo->fullname);

	*docheckout = 1;
	retval = retcode;
    }

    if (unlink_file (backup) < 0
	&& !existence_error (errno))
	error (0, errno, "cannot remove %s", backup);
    if (unlink_file (file1) < 0
	&& !existence_error (errno))
	error (0, errno, "cannot remove %s", file1);
    if (unlink_file (file2) < 0
	&& !existence_error (errno))
	error (0, errno, "cannot remove %s", file2);

    free (backup);
    free (file1);
    free (file2);
    return (retval);
}

/* Write data to a file.  Record whether the last byte written was a
   newline.  Optionally compute a checksum.  This is called by
   patch_file via RCS_checkout.  */

static void
patch_file_write (callerdat, buffer, len)
     void *callerdat;
     const char *buffer;
     size_t len;
{
    struct patch_file_data *data = (struct patch_file_data *) callerdat;

    if (fwrite (buffer, 1, len, data->fp) != len)
	error (1, errno, "cannot write %s", data->filename);

    data->final_nl = (buffer[len - 1] == '\n');

    if (data->compute_checksum)
	cvs_MD5Update (&data->context, (unsigned char *) buffer, len);
}

#endif /* SERVER_SUPPORT */

/*
 * Several of the types we process only print a bit of information consisting
 * of a single letter and the name.
 */
static void
write_letter (finfo, letter)
    struct file_info *finfo;
    int letter;
{
    if (!really_quiet)
    {
	char *tag = NULL;
	/* Big enough for "+updated" or any of its ilk.  */
	char buf[80];

	switch (letter)
	{
	    case 'U':
		tag = "updated";
		break;
	    default:
		/* We don't yet support tagged output except for "U".  */
		break;
	}

	if (tag != NULL)
	{
	    sprintf (buf, "+%s", tag);
	    cvs_output_tagged (buf, NULL);
	}
	buf[0] = letter;
	buf[1] = ' ';
	buf[2] = '\0';
	cvs_output_tagged ("text", buf);
	cvs_output_tagged ("fname", finfo->fullname);
	cvs_output_tagged ("newline", NULL);
	if (tag != NULL)
	{
	    sprintf (buf, "-%s", tag);
	    cvs_output_tagged (buf, NULL);
	}
    }
    return;
}

/*
 * Do all the magic associated with a file which needs to be merged
 */
static int
merge_file (finfo, vers)
    struct file_info *finfo;
    Vers_TS *vers;
{
    char *backup;
    int status;
    int retcode = 0;
    int retval;

    /*
     * The users currently modified file is moved to a backup file name
     * ".#filename.version", so that it will stay around for a few days
     * before being automatically removed by some cron daemon.  The "version"
     * is the version of the file that the user was most up-to-date with
     * before the merge.
     */
    backup = xmalloc (strlen (finfo->file)
		      + strlen (vers->vn_user)
		      + sizeof (BAKPREFIX)
		      + 10);
    (void) sprintf (backup, "%s%s.%s", BAKPREFIX, finfo->file, vers->vn_user);

    if (unlink_file (backup) && !existence_error (errno))
	error (0, errno, "unable to remove %s", backup);
    copy_file (finfo->file, backup);
    xchmod (finfo->file, 1);

    if (strcmp (vers->options, "-kb") == 0
	|| wrap_merge_is_copy (finfo->file)
	|| special_file_mismatch (finfo, NULL, vers->vn_rcs))
    {
	/* For binary files, a merge is always a conflict.  Same for
	   files whose permissions or linkage do not match.  We give the
	   user the two files, and let them resolve it.  It is possible
	   that we should require a "touch foo" or similar step before
	   we allow a checkin.  */

	/* TODO: it may not always be necessary to regard a permission
	   mismatch as a conflict.  The working file and the RCS file
	   have a common ancestor `A'; if the working file's permissions
	   match A's, then it's probably safe to overwrite them with the
	   RCS permissions.  Only if the working file, the RCS file, and
	   A all disagree should this be considered a conflict.  But more
	   thought needs to go into this, and in the meantime it is safe
	   to treat any such mismatch as an automatic conflict. -twp */

#ifdef SERVER_SUPPORT
	if (server_active)
	    server_copy_file (finfo->file, finfo->update_dir,
			      finfo->repository, backup);
#endif

	status = checkout_file (finfo, vers, 0, 1, 1);

	/* Is there a better term than "nonmergeable file"?  What we
	   really mean is, not something that CVS cannot or does not
	   want to merge (there might be an external manual or
	   automatic merge process).  */
	error (0, 0, "nonmergeable file needs merge");
	error (0, 0, "revision %s from repository is now in %s",
	       vers->vn_rcs, finfo->fullname);
	error (0, 0, "file from working directory is now in %s", backup);
	write_letter (finfo, 'C');

	history_write ('C', finfo->update_dir, vers->vn_rcs, finfo->file,
		       finfo->repository);
	retval = 0;
	goto out;
    }

    status = RCS_merge(finfo->rcs, vers->srcfile->path, finfo->file,
		       vers->options, vers->vn_user, vers->vn_rcs);
    if (status != 0 && status != 1)
    {
	error (0, status == -1 ? errno : 0,
	       "could not merge revision %s of %s", vers->vn_user, finfo->fullname);
	error (status == -1 ? 1 : 0, 0, "restoring %s from backup file %s",
	       finfo->fullname, backup);
	rename_file (backup, finfo->file);
	retval = 1;
	goto out;
    }

    if (strcmp (vers->options, "-V4") == 0)
	vers->options[0] = '\0';

    /* This file is the result of a merge, which means that it has
       been modified.  We use a special timestamp string which will
       not compare equal to any actual timestamp.  */
    {
	char *cp = 0;

	if (status)
	{
	    (void) time (&last_register_time);
	    cp = time_stamp (finfo->file);
	}
	Register (finfo->entries, finfo->file, vers->vn_rcs,
		  "Result of merge", vers->options, vers->tag,
		  vers->date, cp);
	if (cp)
	    free (cp);
    }

    /* fix up the vers structure, in case it is used by join */
    if (join_rev1)
    {
	if (vers->vn_user != NULL)
	    free (vers->vn_user);
	vers->vn_user = xstrdup (vers->vn_rcs);
    }

#ifdef SERVER_SUPPORT
    /* Send the new contents of the file before the message.  If we
       wanted to be totally correct, we would have the client write
       the message only after the file has safely been written.  */
    if (server_active)
    {
        server_copy_file (finfo->file, finfo->update_dir, finfo->repository,
			  backup);
	server_updated (finfo, vers, SERVER_MERGED,
			(mode_t) -1, (unsigned char *) NULL,
			(struct buffer *) NULL);
    }
#endif

    /* FIXME: the noexec case is broken.  RCS_merge could be doing the
       xcmp on the temporary files without much hassle, I think.  */
    if (!noexec && !xcmp (backup, finfo->file))
    {
	cvs_output (finfo->fullname, 0);
	cvs_output (" already contains the differences between ", 0);
	cvs_output (vers->vn_user, 0);
	cvs_output (" and ", 0);
	cvs_output (vers->vn_rcs, 0);
	cvs_output ("\n", 1);

	history_write ('G', finfo->update_dir, vers->vn_rcs, finfo->file,
		       finfo->repository);
	retval = 0;
	goto out;
    }

    if (status == 1)
    {
	error (0, 0, "conflicts found in %s", finfo->fullname);

	write_letter (finfo, 'C');

	history_write ('C', finfo->update_dir, vers->vn_rcs, finfo->file, finfo->repository);

    }
    else if (retcode == -1)
    {
	error (1, errno, "fork failed while examining update of %s",
	       finfo->fullname);
    }
    else
    {
	write_letter (finfo, 'M');
	history_write ('G', finfo->update_dir, vers->vn_rcs, finfo->file,
		       finfo->repository);
    }
    retval = 0;
 out:
    free (backup);
    return retval;
}

/*
 * Do all the magic associated with a file which needs to be joined
 * (-j option)
 */
static void
join_file (finfo, vers)
    struct file_info *finfo;
    Vers_TS *vers;
{
    char *backup;
    char *t_options;
    int status;

    char *rev1;
    char *rev2;
    char *jrev1;
    char *jrev2;
    char *jdate1;
    char *jdate2;

    jrev1 = join_rev1;
    jrev2 = join_rev2;
    jdate1 = date_rev1;
    jdate2 = date_rev2;

    /* Determine if we need to do anything at all.  */
    if (vers->srcfile == NULL ||
	vers->srcfile->path == NULL)
    {
	return;
    }

    /* If only one join revision is specified, it becomes the second
       revision.  */
    if (jrev2 == NULL)
    {
	jrev2 = jrev1;
	jrev1 = NULL;
	jdate2 = jdate1;
	jdate1 = NULL;
    }

    /* Convert the second revision, walking branches and dates.  */
    rev2 = RCS_getversion (vers->srcfile, jrev2, jdate2, 1, (int *) NULL);

    /* If this is a merge of two revisions, get the first revision.
       If only one join tag was specified, then the first revision is
       the greatest common ancestor of the second revision and the
       working file.  */
    if (jrev1 != NULL)
	rev1 = RCS_getversion (vers->srcfile, jrev1, jdate1, 1, (int *) NULL);
    else
    {
	/* Note that we use vn_rcs here, since vn_user may contain a
           special string such as "-nn".  */
	if (vers->vn_rcs == NULL)
	    rev1 = NULL;
	else if (rev2 == NULL)
	{
	    /* This means that the file never existed on the branch.
               It does not mean that the file was removed on the
               branch: that case is represented by a dead rev2.  If
               the file never existed on the branch, then we have
               nothing to merge, so we just return.  */
	    return;
	}
	else
	    rev1 = gca (vers->vn_rcs, rev2);
    }

    /* Handle a nonexistent or dead merge target.  */
    if (rev2 == NULL || RCS_isdead (vers->srcfile, rev2))
    {
	char *mrev;

	if (rev2 != NULL)
	    free (rev2);

	/* If the first revision doesn't exist either, then there is
           no change between the two revisions, so we don't do
           anything.  */
	if (rev1 == NULL || RCS_isdead (vers->srcfile, rev1))
	{
	    if (rev1 != NULL)
		free (rev1);
	    return;
	}

	/* If we are merging two revisions, then the file was removed
	   between the first revision and the second one.  In this
	   case we want to mark the file for removal.

	   If we are merging one revision, then the file has been
	   removed between the greatest common ancestor and the merge
	   revision.  From the perspective of the branch on to which
	   we ar emerging, which may be the trunk, either 1) the file
	   does not currently exist on the target, or 2) the file has
	   not been modified on the target branch since the greatest
	   common ancestor, or 3) the file has been modified on the
	   target branch since the greatest common ancestor.  In case
	   1 there is nothing to do.  In case 2 we mark the file for
	   removal.  In case 3 we have a conflict.

	   Note that the handling is slightly different depending upon
	   whether one or two join targets were specified.  If two
	   join targets were specified, we don't check whether the
	   file was modified since a given point.  My reasoning is
	   that if you ask for an explicit merge between two tags,
	   then you want to merge in whatever was changed between
	   those two tags.  If a file was removed between the two
	   tags, then you want it to be removed.  However, if you ask
	   for a merge of a branch, then you want to merge in all
	   changes which were made on the branch.  If a file was
	   removed on the branch, that is a change to the file.  If
	   the file was also changed on the main line, then that is
	   also a change.  These two changes--the file removal and the
	   modification--must be merged.  This is a conflict.  */

	/* If the user file is dead, or does not exist, or has been
           marked for removal, then there is nothing to do.  */
	if (vers->vn_user == NULL
	    || vers->vn_user[0] == '-'
	    || RCS_isdead (vers->srcfile, vers->vn_user))
	{
	    if (rev1 != NULL)
		free (rev1);
	    return;
	}

	/* If the user file has been marked for addition, or has been
	   locally modified, then we have a conflict which we can not
	   resolve.  No_Difference will already have been called in
	   this case, so comparing the timestamps is sufficient to
	   determine whether the file is locally modified.  */
	if (strcmp (vers->vn_user, "0") == 0
	    || (vers->ts_user != NULL
		&& strcmp (vers->ts_user, vers->ts_rcs) != 0))
	{
	    if (jdate2 != NULL)
		error (0, 0,
		       "file %s is locally modified, but has been removed in revision %s as of %s",
		       finfo->fullname, jrev2, jdate2);
	    else
		error (0, 0,
		       "file %s is locally modified, but has been removed in revision %s",
		       finfo->fullname, jrev2);

	    /* FIXME: Should we arrange to return a non-zero exit
               status?  */

	    if (rev1 != NULL)
		free (rev1);

	    return;
	}

	/* If only one join tag was specified, and the user file has
           been changed since the greatest common ancestor (rev1),
           then there is a conflict we can not resolve.  See above for
           the rationale.  */
	if (join_rev2 == NULL
	    && strcmp (rev1, vers->vn_user) != 0)
	{
	    if (jdate2 != NULL)
		error (0, 0,
		       "file %s has been modified, but has been removed in revision %s as of %s",
		       finfo->fullname, jrev2, jdate2);
	    else
		error (0, 0,
		       "file %s has been modified, but has been removed in revision %s",
		       finfo->fullname, jrev2);

	    /* FIXME: Should we arrange to return a non-zero exit
               status?  */

	    if (rev1 != NULL)
		free (rev1);

	    return;
	}

	if (rev1 != NULL)
	    free (rev1);

	/* The user file exists and has not been modified.  Mark it
           for removal.  FIXME: If we are doing a checkout, this has
           the effect of first checking out the file, and then
           removing it.  It would be better to just register the
           removal.  */
#ifdef SERVER_SUPPORT
	if (server_active)
	{
	    server_scratch (finfo->file);
	    server_updated (finfo, vers, SERVER_UPDATED, (mode_t) -1,
			    (unsigned char *) NULL, (struct buffer *) NULL);
	}
#endif
	mrev = xmalloc (strlen (vers->vn_user) + 2);
	sprintf (mrev, "-%s", vers->vn_user);
	Register (finfo->entries, finfo->file, mrev, vers->ts_rcs,
		  vers->options, vers->tag, vers->date, vers->ts_conflict);
	free (mrev);
	/* We need to check existence_error here because if we are
           running as the server, and the file is up to date in the
           working directory, the client will not have sent us a copy.  */
	if (unlink_file (finfo->file) < 0 && ! existence_error (errno))
	    error (0, errno, "cannot remove file %s", finfo->fullname);
#ifdef SERVER_SUPPORT
	if (server_active)
	    server_checked_in (finfo->file, finfo->update_dir,
			       finfo->repository);
#endif
	if (! really_quiet)
	    error (0, 0, "scheduling %s for removal", finfo->fullname);

	return;
    }

    /* If the target of the merge is the same as the working file
       revision, then there is nothing to do.  */
    if (vers->vn_user != NULL && strcmp (rev2, vers->vn_user) == 0)
    {
	if (rev1 != NULL)
	    free (rev1);
	free (rev2);
	return;
    }

    /* If rev1 is dead or does not exist, then the file was added
       between rev1 and rev2.  */
    if (rev1 == NULL || RCS_isdead (vers->srcfile, rev1))
    {
	if (rev1 != NULL)
	    free (rev1);
	free (rev2);

	/* If the file does not exist in the working directory, then
           we can just check out the new revision and mark it for
           addition.  */
	if (vers->vn_user == NULL)
	{
	    Vers_TS *xvers;

	    xvers = Version_TS (finfo, vers->options, jrev2, jdate2, 1, 0);

	    /* Reset any keyword expansion option.  Otherwise, when a
	       command like `cvs update -kk -jT1 -jT2' creates a new file
	       (because a file had the T2 tag, but not T1), the subsequent
	       commit of that just-added file effectively would set the
	       admin `-kk' option for that file in the repository.  */
	    options = NULL;

	    /* FIXME: If checkout_file fails, we should arrange to
               return a non-zero exit status.  */
	    status = checkout_file (finfo, xvers, 1, 0, 1);

	    freevers_ts (&xvers);

	    return;
	}

	/* The file currently exists in the working directory, so we
           have a conflict which we can not resolve.  Note that this
           is true even if the file is marked for addition or removal.  */

	if (jdate2 != NULL)
	    error (0, 0,
		   "file %s exists, but has been added in revision %s as of %s",
		   finfo->fullname, jrev2, jdate2);
	else
	    error (0, 0,
		   "file %s exists, but has been added in revision %s",
		   finfo->fullname, jrev2);

	return;
    }

    /* If the two merge revisions are the same, then there is nothing
       to do.  */
    if (strcmp (rev1, rev2) == 0)
    {
	free (rev1);
	free (rev2);
	return;
    }

    /* If there is no working file, then we can't do the merge.  */
    if (vers->vn_user == NULL)
    {
	free (rev1);
	free (rev2);

	if (jdate2 != NULL)
	    error (0, 0,
		   "file %s does not exist, but is present in revision %s as of %s",
		   finfo->fullname, jrev2, jdate2);
	else
	    error (0, 0,
		   "file %s does not exist, but is present in revision %s",
		   finfo->fullname, jrev2);

	/* FIXME: Should we arrange to return a non-zero exit status?  */

	return;
    }

#ifdef SERVER_SUPPORT
    if (server_active && !isreadable (finfo->file))
    {
	int retcode;
	/* The file is up to date.  Need to check out the current contents.  */
	retcode = RCS_checkout (vers->srcfile, finfo->file,
				vers->vn_user, (char *) NULL,
				(char *) NULL, RUN_TTY,
				(RCSCHECKOUTPROC) NULL, (void *) NULL);
	if (retcode != 0)
	    error (1, 0,
		   "failed to check out %s file", finfo->fullname);
    }
#endif

    /*
     * The users currently modified file is moved to a backup file name
     * ".#filename.version", so that it will stay around for a few days
     * before being automatically removed by some cron daemon.  The "version"
     * is the version of the file that the user was most up-to-date with
     * before the merge.
     */
    backup = xmalloc (strlen (finfo->file)
		      + strlen (vers->vn_user)
		      + sizeof (BAKPREFIX)
		      + 10);
    (void) sprintf (backup, "%s%s.%s", BAKPREFIX, finfo->file, vers->vn_user);

    if (unlink_file (backup) < 0
	&& !existence_error (errno))
	error (0, errno, "cannot remove %s", backup);
    copy_file (finfo->file, backup);
    xchmod (finfo->file, 1);

    t_options = vers->options;
#if 0
    if (*t_options == '\0')
	t_options = "-kk";		/* to ignore keyword expansions */
#endif

    /* If the source of the merge is the same as the working file
       revision, then we can just RCS_checkout the target (no merging
       as such).  In the text file case, this is probably quite
       similar to the RCS_merge, but in the binary file case,
       RCS_merge gives all kinds of trouble.  */
    if (vers->vn_user != NULL
	&& strcmp (rev1, vers->vn_user) == 0
	/* See comments above about how No_Difference has already been
	   called.  */
	&& vers->ts_user != NULL
	&& strcmp (vers->ts_user, vers->ts_rcs) == 0

	/* This is because of the worry below about $Name.  If that
	   isn't a problem, I suspect this code probably works for
	   text files too.  */
	&& (strcmp (t_options, "-kb") == 0
	    || wrap_merge_is_copy (finfo->file)))
    {
	/* FIXME: what about nametag?  What does RCS_merge do with
	   $Name?  */
	if (RCS_checkout (finfo->rcs, finfo->file, rev2, NULL, t_options,
			  RUN_TTY, (RCSCHECKOUTPROC)0, NULL) != 0)
	    status = 2;
	else
	    status = 0;

	/* OK, this is really stupid.  RCS_checkout carefully removes
	   write permissions, and we carefully put them back.  But
	   until someone gets around to fixing it, that seems like the
	   easiest way to get what would seem to be the right mode.
	   I don't check CVSWRITE or _watched; I haven't thought about
	   that in great detail, but it seems like a watched file should
	   be checked out (writable) after a merge.  */
	xchmod (finfo->file, 1);

	/* Traditionally, the text file case prints a whole bunch of
	   scary looking and verbose output which fails to tell the user
	   what is really going on (it gives them rev1 and rev2 but doesn't
	   indicate in any way that rev1 == vn_user).  I think just a
	   simple "U foo" is good here; it seems analogous to the case in
	   which the file was added on the branch in terms of what to
	   print.  */
	write_letter (finfo, 'U');
    }
    else if (strcmp (t_options, "-kb") == 0
	     || wrap_merge_is_copy (finfo->file)
	     || special_file_mismatch (finfo, rev1, rev2))
    {
	/* We are dealing with binary files, or files with a
	   permission/linkage mismatch, and real merging would
	   need to take place.  This is a conflict.  We give the user
	   the two files, and let them resolve it.  It is possible
	   that we should require a "touch foo" or similar step before
	   we allow a checkin.  */
	if (RCS_checkout (finfo->rcs, finfo->file, rev2, NULL, t_options,
			  RUN_TTY, (RCSCHECKOUTPROC)0, NULL) != 0)
	    status = 2;
	else
	    status = 0;

	/* OK, this is really stupid.  RCS_checkout carefully removes
	   write permissions, and we carefully put them back.  But
	   until someone gets around to fixing it, that seems like the
	   easiest way to get what would seem to be the right mode.
	   I don't check CVSWRITE or _watched; I haven't thought about
	   that in great detail, but it seems like a watched file should
	   be checked out (writable) after a merge.  */
	xchmod (finfo->file, 1);

	/* Hmm.  We don't give them REV1 anywhere.  I guess most people
	   probably don't have a 3-way merge tool for the file type in
	   question, and might just get confused if we tried to either
	   provide them with a copy of the file from REV1, or even just
	   told them what REV1 is so they can get it themself, but it
	   might be worth thinking about.  */
	/* See comment in merge_file about the "nonmergeable file"
	   terminology.  */
	error (0, 0, "nonmergeable file needs merge");
	error (0, 0, "revision %s from repository is now in %s",
	       rev2, finfo->fullname);
	error (0, 0, "file from working directory is now in %s", backup);
	write_letter (finfo, 'C');
    }
    else
	status = RCS_merge (finfo->rcs, vers->srcfile->path, finfo->file,
			    t_options, rev1, rev2);

    if (status != 0 && status != 1)
    {
	error (0, status == -1 ? errno : 0,
	       "could not merge revision %s of %s", rev2, finfo->fullname);
	error (status == -1 ? 1 : 0, 0, "restoring %s from backup file %s",
	       finfo->fullname, backup);
	rename_file (backup, finfo->file);
    }
    free (rev1);
    free (rev2);

    /* The file has changed, but if we just checked it out it may
       still have the same timestamp it did when it was first
       registered above in checkout_file.  We register it again with a
       dummy timestamp to make sure that later runs of CVS will
       recognize that it has changed.

       We don't actually need to register again if we called
       RCS_checkout above, and we aren't running as the server.
       However, that is not the normal case, and calling Register
       again won't cost much in that case.  */
    {
	char *cp = 0;

	if (status)
	{
	    (void) time (&last_register_time);
	    cp = time_stamp (finfo->file);
	}
	Register (finfo->entries, finfo->file,
		  vers->vn_rcs ? vers->vn_rcs : "0", "Result of merge",
		  vers->options, vers->tag, vers->date, cp);
	if (cp)
	    free(cp);
    }

#ifdef SERVER_SUPPORT
    if (server_active)
    {
	server_copy_file (finfo->file, finfo->update_dir, finfo->repository,
			  backup);
	server_updated (finfo, vers, SERVER_MERGED,
			(mode_t) -1, (unsigned char *) NULL,
			(struct buffer *) NULL);
    }
#endif
    free (backup);
}

/*
 * Report whether revisions REV1 and REV2 of FINFO agree on:
 *   . file ownership
 *   . permissions
 *   . major and minor device numbers
 *   . symbolic links
 *   . hard links
 *
 * If either REV1 or REV2 is NULL, the working copy is used instead.
 *
 * Return 1 if the files differ on these data.
 */

int
special_file_mismatch (finfo, rev1, rev2)
    struct file_info *finfo;
    char *rev1;
    char *rev2;
{
#ifdef PRESERVE_PERMISSIONS_SUPPORT
    struct stat sb;
    RCSVers *vp;
    Node *n;
    uid_t rev1_uid, rev2_uid;
    gid_t rev1_gid, rev2_gid;
    mode_t rev1_mode, rev2_mode;
    unsigned long dev_long;
    dev_t rev1_dev, rev2_dev;
    char *rev1_symlink = NULL;
    char *rev2_symlink = NULL;
    List *rev1_hardlinks = NULL;
    List *rev2_hardlinks = NULL;
    int check_uids, check_gids, check_modes;
    int result;

    /* If we don't care about special file info, then
       don't report a mismatch in any case. */
    if (!preserve_perms)
	return 0;

    /* When special_file_mismatch is called from No_Difference, the
       RCS file has been only partially parsed.  We must read the
       delta tree in order to compare special file info recorded in
       the delta nodes.  (I think this is safe. -twp) */
    if (finfo->rcs->flags & PARTIAL)
	RCS_reparsercsfile (finfo->rcs, NULL, NULL);

    check_uids = check_gids = check_modes = 1;

    /* Obtain file information for REV1.  If this is null, then stat
       finfo->file and use that info. */
    /* If a revision does not know anything about its status,
       then presumably it doesn't matter, and indicates no conflict. */

    if (rev1 == NULL)
    {
	if (islink (finfo->file))
	    rev1_symlink = xreadlink (finfo->file);
	else
	{
#ifdef HAVE_ST_RDEV
	    if (CVS_LSTAT (finfo->file, &sb) < 0)
		error (1, errno, "could not get file information for %s",
		       finfo->file);
	    rev1_uid = sb.st_uid;
	    rev1_gid = sb.st_gid;
	    rev1_mode = sb.st_mode;
	    if (S_ISBLK (rev1_mode) || S_ISCHR (rev1_mode))
		rev1_dev = sb.st_rdev;
#else
	    error (1, 0, "cannot handle device files on this system (%s)",
		   finfo->file);
#endif
	}
	rev1_hardlinks = list_linked_files_on_disk (finfo->file);
    }
    else
    {
	n = findnode (finfo->rcs->versions, rev1);
	vp = (RCSVers *) n->data;

	n = findnode (vp->other_delta, "symlink");
	if (n != NULL)
	    rev1_symlink = xstrdup (n->data);
	else
	{
	    n = findnode (vp->other_delta, "owner");
	    if (n == NULL)
		check_uids = 0;	/* don't care */
	    else
		rev1_uid = strtoul (n->data, NULL, 10);

	    n = findnode (vp->other_delta, "group");
	    if (n == NULL)
		check_gids = 0;	/* don't care */
	    else
		rev1_gid = strtoul (n->data, NULL, 10);

	    n = findnode (vp->other_delta, "permissions");
	    if (n == NULL)
		check_modes = 0;	/* don't care */
	    else
		rev1_mode = strtoul (n->data, NULL, 8);

	    n = findnode (vp->other_delta, "special");
	    if (n == NULL)
		rev1_mode |= S_IFREG;
	    else
	    {
		/* If the size of `ftype' changes, fix the sscanf call also */
		char ftype[16];
		if (sscanf (n->data, "%16s %lu", ftype,
			    &dev_long) < 2)
		    error (1, 0, "%s:%s has bad `special' newphrase %s",
			   finfo->file, rev1, n->data);
		rev1_dev = dev_long;
		if (strcmp (ftype, "character") == 0)
		    rev1_mode |= S_IFCHR;
		else if (strcmp (ftype, "block") == 0)
		    rev1_mode |= S_IFBLK;
		else
		    error (0, 0, "%s:%s unknown file type `%s'",
			   finfo->file, rev1, ftype);
	    }

	    rev1_hardlinks = vp->hardlinks;
	    if (rev1_hardlinks == NULL)
		rev1_hardlinks = getlist();
	}
    }

    /* Obtain file information for REV2. */
    if (rev2 == NULL)
    {
	if (islink (finfo->file))
	    rev2_symlink = xreadlink (finfo->file);
	else
	{
#ifdef HAVE_ST_RDEV
	    if (CVS_LSTAT (finfo->file, &sb) < 0)
		error (1, errno, "could not get file information for %s",
		       finfo->file);
	    rev2_uid = sb.st_uid;
	    rev2_gid = sb.st_gid;
	    rev2_mode = sb.st_mode;
	    if (S_ISBLK (rev2_mode) || S_ISCHR (rev2_mode))
		rev2_dev = sb.st_rdev;
#else
	    error (1, 0, "cannot handle device files on this system (%s)",
		   finfo->file);
#endif
	}
	rev2_hardlinks = list_linked_files_on_disk (finfo->file);
    }
    else
    {
	n = findnode (finfo->rcs->versions, rev2);
	vp = (RCSVers *) n->data;

	n = findnode (vp->other_delta, "symlink");
	if (n != NULL)
	    rev2_symlink = xstrdup (n->data);
	else
	{
	    n = findnode (vp->other_delta, "owner");
	    if (n == NULL)
		check_uids = 0;	/* don't care */
	    else
		rev2_uid = strtoul (n->data, NULL, 10);

	    n = findnode (vp->other_delta, "group");
	    if (n == NULL)
		check_gids = 0;	/* don't care */
	    else
		rev2_gid = strtoul (n->data, NULL, 10);

	    n = findnode (vp->other_delta, "permissions");
	    if (n == NULL)
		check_modes = 0;	/* don't care */
	    else
		rev2_mode = strtoul (n->data, NULL, 8);

	    n = findnode (vp->other_delta, "special");
	    if (n == NULL)
		rev2_mode |= S_IFREG;
	    else
	    {
		/* If the size of `ftype' changes, fix the sscanf call also */
		char ftype[16];
		if (sscanf (n->data, "%16s %lu", ftype,
			    &dev_long) < 2)
		    error (1, 0, "%s:%s has bad `special' newphrase %s",
			   finfo->file, rev2, n->data);
		rev2_dev = dev_long;
		if (strcmp (ftype, "character") == 0)
		    rev2_mode |= S_IFCHR;
		else if (strcmp (ftype, "block") == 0)
		    rev2_mode |= S_IFBLK;
		else
		    error (0, 0, "%s:%s unknown file type `%s'",
			   finfo->file, rev2, ftype);
	    }

	    rev2_hardlinks = vp->hardlinks;
	    if (rev2_hardlinks == NULL)
		rev2_hardlinks = getlist();
	}
    }

    /* Check the user/group ownerships and file permissions, printing
       an error for each mismatch found.  Return 0 if all characteristics
       matched, and 1 otherwise. */

    result = 0;

    /* Compare symlinks first, since symlinks are simpler (don't have
       any other characteristics). */
    if (rev1_symlink != NULL && rev2_symlink == NULL)
    {
	error (0, 0, "%s is a symbolic link",
	       (rev1 == NULL ? "working file" : rev1));
	result = 1;
    }
    else if (rev1_symlink == NULL && rev2_symlink != NULL)
    {
	error (0, 0, "%s is a symbolic link",
	       (rev2 == NULL ? "working file" : rev2));
	result = 1;
    }
    else if (rev1_symlink != NULL)
	result = (strcmp (rev1_symlink, rev2_symlink) == 0);
    else
    {
	/* Compare user ownership. */
	if (check_uids && rev1_uid != rev2_uid)
	{
	    error (0, 0, "%s: owner mismatch between %s and %s",
		   finfo->file,
		   (rev1 == NULL ? "working file" : rev1),
		   (rev2 == NULL ? "working file" : rev2));
	    result = 1;
	}

	/* Compare group ownership. */
	if (check_gids && rev1_gid != rev2_gid)
	{
	    error (0, 0, "%s: group mismatch between %s and %s",
		   finfo->file,
		   (rev1 == NULL ? "working file" : rev1),
		   (rev2 == NULL ? "working file" : rev2));
	    result = 1;
	}
    
	/* Compare permissions. */
	if (check_modes &&
	    (rev1_mode & 07777) != (rev2_mode & 07777))
	{
	    error (0, 0, "%s: permission mismatch between %s and %s",
		   finfo->file,
		   (rev1 == NULL ? "working file" : rev1),
		   (rev2 == NULL ? "working file" : rev2));
	    result = 1;
	}

	/* Compare device file characteristics. */
	if ((rev1_mode & S_IFMT) != (rev2_mode & S_IFMT))
	{
	    error (0, 0, "%s: %s and %s are different file types",
		   finfo->file,
		   (rev1 == NULL ? "working file" : rev1),
		   (rev2 == NULL ? "working file" : rev2));
	    result = 1;
	}
	else if (S_ISBLK (rev1_mode))
	{
	    if (rev1_dev != rev2_dev)
	    {
		error (0, 0, "%s: device numbers of %s and %s do not match",
		       finfo->file,
		       (rev1 == NULL ? "working file" : rev1),
		       (rev2 == NULL ? "working file" : rev2));
		result = 1;
	    }
	}

	/* Compare hard links. */
	if (compare_linkage_lists (rev1_hardlinks, rev2_hardlinks) == 0)
	{
	    error (0, 0, "%s: hard linkage of %s and %s do not match",
		   finfo->file,
		   (rev1 == NULL ? "working file" : rev1),
		   (rev2 == NULL ? "working file" : rev2));
	    result = 1;
	}
    }

    if (rev1_symlink != NULL)
	free (rev1_symlink);
    if (rev2_symlink != NULL)
	free (rev2_symlink);
    if (rev1_hardlinks != NULL)
	dellist (&rev1_hardlinks);
    if (rev2_hardlinks != NULL)
	dellist (&rev2_hardlinks);

    return result;
#else
    return 0;
#endif
}

int
joining ()
{
    return (join_rev1 != NULL);
}
