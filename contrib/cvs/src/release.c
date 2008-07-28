/*
 * Copyright (C) 1994-2005 The Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Release: "cancel" a checkout in the history log.
 * 
 * - Enter a line in the history log indicating the "release". - If asked to,
 * delete the local working directory.
 */

#include "cvs.h"
#include "savecwd.h"
#include "getline.h"

static const char *const release_usage[] =
{
    "Usage: %s %s [-d] directories...\n",
    "\t-d\tDelete the given directory.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

#ifdef SERVER_SUPPORT
static int release_server PROTO ((int argc, char **argv));

/* This is the server side of cvs release.  */
static int
release_server (argc, argv)
    int argc;
    char **argv;
{
    int i;

    /* Note that we skip argv[0].  */
    for (i = 1; i < argc; ++i)
	history_write ('F', argv[i], "", argv[i], "");
    return 0;
}

#endif /* SERVER_SUPPORT */

/* There are various things to improve about this implementation:

   1.  Using run_popen to run "cvs update" could be replaced by a
   fairly simple start_recursion/classify_file loop--a win for
   portability, performance, and cleanliness.  In particular, there is
   no particularly good way to find the right "cvs".

   2.  The fact that "cvs update" contacts the server slows things down;
   it undermines the case for using "cvs release" rather than "rm -rf".
   However, for correctly printing "? foo" and correctly handling
   CVSROOTADM_IGNORE, we currently need to contact the server.  (One
   idea for how to fix this is to stash a copy of CVSROOTADM_IGNORE in
   the working directories; see comment at base_* in entries.c for a
   few thoughts on that).

   3.  Would be nice to take processing things on the client side one step
   further, and making it like edit/unedit in terms of working well if
   disconnected from the network, and then sending a delayed
   notification.

   4.  Having separate network turnarounds for the "Notify" request
   which we do as part of unedit, and for the "release" itself, is slow
   and unnecessary.  */

int
release (argc, argv)
    int argc;
    char **argv;
{
    FILE *fp;
    int i, c;
    char *line = NULL;
    size_t line_allocated = 0;
    char *update_cmd;
    char *thisarg;
    int arg_start_idx;
    int err = 0;
    short delete_flag = 0;
    struct saved_cwd cwd;

#ifdef SERVER_SUPPORT
    if (server_active)
	return release_server (argc, argv);
#endif

    /* Everything from here on is client or local.  */
    if (argc == -1)
	usage (release_usage);
    optind = 0;
    while ((c = getopt (argc, argv, "+Qdq")) != -1)
    {
	switch (c)
	{
	    case 'Q':
	    case 'q':
		error (1, 0,
		       "-q or -Q must be specified before \"%s\"",
		       cvs_cmd_name);
		break;
	    case 'd':
		delete_flag++;
		break;
	    case '?':
	    default:
		usage (release_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    /* We're going to run "cvs -n -q update" and check its output; if
     * the output is sufficiently unalarming, then we release with no
     * questions asked.  Else we prompt, then maybe release.
     * (Well, actually we ask no matter what.  Our notion of "sufficiently
     * unalarming" doesn't take into account "? foo.c" files, so it is
     * up to the user to take note of them, at least currently
     * (ignore-193 in testsuite)).
     */
    /* Construct the update command.  Be sure to add authentication and
       encryption if we are using them currently, else our child process may
       not be able to communicate with the server.  */
    update_cmd = xmalloc (strlen (program_path)
                        + strlen (current_parsed_root->original)
                        + 1 + 3 + 3 + 16 + 1);
    sprintf (update_cmd, "%s %s%s-n -q -d %s update",
             program_path,
#if defined (CLIENT_SUPPORT) || defined (SERVER_SUPPORT)
             cvsauthenticate ? "-a " : "",
             cvsencrypt ? "-x " : "",
#else
	     "", "",
#endif
             current_parsed_root->original);

#ifdef CLIENT_SUPPORT
    /* Start the server; we'll close it after looping. */
    if (current_parsed_root->isremote)
    {
	start_server ();
	ign_setup ();
    }
#endif /* CLIENT_SUPPORT */

    /* Remember the directory where "cvs release" was invoked because
       all args are relative to this directory and we chdir around.
       */
    if (save_cwd (&cwd))
        error_exit ();

    arg_start_idx = 0;

    for (i = arg_start_idx; i < argc; i++)
    {
	thisarg = argv[i];

        if (isdir (thisarg))
        {
	    if (CVS_CHDIR (thisarg) < 0)
	    {
		if (!really_quiet)
		    error (0, errno, "can't chdir to: %s", thisarg);
		continue;
	    }
	    if (!isdir (CVSADM))
	    {
		if (!really_quiet)
		    error (0, 0, "no repository directory: %s", thisarg);
		if (restore_cwd (&cwd, NULL))
		    error_exit ();
		continue;
	    }
	}
	else
        {
	    if (!really_quiet)
		error (0, 0, "no such directory: %s", thisarg);
	    continue;
	}

	if (!really_quiet)
	{
	    int line_length, status;

	    /* The "release" command piggybacks on "update", which
	       does the real work of finding out if anything is not
	       up-to-date with the repository.  Then "release" prompts
	       the user, telling her how many files have been
	       modified, and asking if she still wants to do the
	       release.  */
	    fp = run_popen (update_cmd, "r");
	    if (fp == NULL)
		error (1, 0, "cannot run command %s", update_cmd);

	    c = 0;

	    while ((line_length = getline (&line, &line_allocated, fp)) >= 0)
	    {
		if (strchr ("MARCZ", *line))
		    c++;
		(void) fputs (line, stdout);
	    }
	    if (line_length < 0 && !feof (fp))
		error (0, errno, "cannot read from subprocess");

	    /* If the update exited with an error, then we just want to
	       complain and go on to the next arg.  Especially, we do
	       not want to delete the local copy, since it's obviously
	       not what the user thinks it is.  */
	    status = pclose (fp);
	    if (status != 0)
	    {
		error (0, 0, "unable to release `%s' (%d)", thisarg, status);
		if (restore_cwd (&cwd, NULL))
		    error_exit ();
		continue;
	    }

	    printf ("You have [%d] altered files in this repository.\n",
		    c);
	    printf ("Are you sure you want to release %sdirectory `%s': ",
		    delete_flag ? "(and delete) " : "", thisarg);
	    c = !yesno ();
	    if (c)			/* "No" */
	    {
		(void) fprintf (stderr, "** `%s' aborted by user choice.\n",
				cvs_cmd_name);
		if (restore_cwd (&cwd, NULL))
		    error_exit ();
		continue;
	    }
	}

        /* Note:  client.c doesn't like to have other code
           changing the current directory on it.  So a fair amount
           of effort is needed to make sure it doesn't get confused
           about the directory and (for example) overwrite
           CVS/Entries file in the wrong directory.  See release-17
           through release-23. */

	if (restore_cwd (&cwd, NULL))
	    error_exit ();

	if (1
#ifdef CLIENT_SUPPORT
	    && !(current_parsed_root->isremote
		 && (!supported_request ("noop")
		     || !supported_request ("Notify")))
#endif
	    )
	{
	    int argc = 2;
	    char *argv[3];
	    argv[0] = "dummy";
	    argv[1] = thisarg;
	    argv[2] = NULL;
	    err += unedit (argc, argv);
            if (restore_cwd (&cwd, NULL))
                error_exit ();
	}

#ifdef CLIENT_SUPPORT
        if (current_parsed_root->isremote)
        {
	    send_to_server ("Argument ", 0);
	    send_to_server (thisarg, 0);
	    send_to_server ("\012", 1);
	    send_to_server ("release\012", 0);
	}
        else
#endif /* CLIENT_SUPPORT */
        {
	    history_write ('F', thisarg, "", thisarg, ""); /* F == Free */
        }

	if (delete_flag)
	{
	    /* FIXME?  Shouldn't this just delete the CVS-controlled
	       files and, perhaps, the files that would normally be
	       ignored and leave everything else?  */

	    if (unlink_file_dir (thisarg) < 0)
		error (0, errno, "deletion of directory %s failed", thisarg);
	}

#ifdef CLIENT_SUPPORT
        if (current_parsed_root->isremote)
        {
	    /* FIXME:
	     * Is there a good reason why get_server_responses() isn't
	     * responsible for restoring its initial directory itself when
	     * finished?
	     */
            err += get_server_responses ();

            if (restore_cwd (&cwd, NULL))
                error_exit ();
        }
#endif /* CLIENT_SUPPORT */
    }

    if (restore_cwd (&cwd, NULL))
	error_exit ();
    free_cwd (&cwd);

#ifdef CLIENT_SUPPORT
    if (current_parsed_root->isremote)
    {
	/* Unfortunately, client.c doesn't offer a way to close
	   the connection without waiting for responses.  The extra
	   network turnaround here is quite unnecessary other than
	   that....  */
	send_to_server ("noop\012", 0);
	err += get_responses_and_close ();
    }
#endif /* CLIENT_SUPPORT */

    free (update_cmd);
    if (line != NULL)
	free (line);
    return err;
}
