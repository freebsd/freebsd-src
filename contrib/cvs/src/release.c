/*
 * Release: "cancel" a checkout in the history log.
 * 
 * - Don't allow release if anything is active - Don't allow release if not
 * above or inside repository. - Don't allow release if ./CVS/Repository is
 * not the same as the directory specified in the module database.
 * 
 * - Enter a line in the history log indicating the "release". - If asked to,
 * delete the local working directory.
 */

#include "cvs.h"

static void release_delete PROTO((char *dir));

static const char *const release_usage[] =
{
    "Usage: %s %s [-d] modules...\n",
    "\t-d\tDelete the given directory.\n",
    NULL
};

static short delete_flag;

/* FIXME: This implementation is cheezy in quite a few ways:

   1.  The whole "cvs update" junk could be checked locally with a
   fairly simple start_recursion/classify_file loop--a win for
   portability, performance, and cleanliness.

   2.  Should be like edit/unedit in terms of working well if disconnected
   from the network, and then sending a delayed notification.

   3.  Way too many network turnarounds.  More than one for each argument.
   Puh-leeze.

   4.  Oh, and as a purely stylistic nit, break this out into separate
   functions for client/local and for server.  Those #ifdefs are a mess.  */

int
release (argc, argv)
    int argc;
    char **argv;
{
    FILE *fp;
    register int i, c;
    char *repository, *srepos;
    char line[PATH_MAX], update_cmd[PATH_MAX];
    char *thisarg;
    int arg_start_idx;
    int err = 0;

#ifdef SERVER_SUPPORT
    if (!server_active)
      {
#endif /* SERVER_SUPPORT */
        if (argc == -1)
          usage (release_usage);
        optind = 1;
        while ((c = getopt (argc, argv, "Qdq")) != -1)
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
#ifdef SERVER_SUPPORT
      }
#endif /* SERVER_SUPPORT */

    /* We're going to run "cvs -n -q update" and check its output; if
     * the output is sufficiently unalarming, then we release with no
     * questions asked.  Else we prompt, then maybe release.
     */
    /* Construct the update command. */
    sprintf (update_cmd, "%s -n -q -d %s update",
             program_path, CVSroot);

#ifdef CLIENT_SUPPORT
    /* Start the server; we'll close it after looping. */
    if (client_active)
      {
	start_server ();
	ign_setup ();
      }
#endif /* CLIENT_SUPPORT */

    /* If !server_active, we already skipped over argv[0] in the "argc
       -= optind;" statement above.  But if server_active, we need to
       skip it now.  */
#ifdef SERVER_SUPPORT
    if (server_active)
      arg_start_idx = 1;
    else
#endif /* SERVER_SUPPORT */
      arg_start_idx = 0;

    for (i = arg_start_idx; i < argc; i++)
    {
      thisarg = argv[i];
        
#ifdef SERVER_SUPPORT
      if (server_active)
      {
        /* Just log the release -- all the interesting stuff happened
         * on the client.
         */
        history_write ('F', thisarg, "", thisarg, "");	/* F == Free */
      }
      else
      {
#endif /* SERVER_SUPPORT */
        
        /*
         * If we are in a repository, do it.  Else if we are in the parent of
         * a directory with the same name as the module, "cd" into it and
         * look for a repository there.
         */
        if (isdir (thisarg))
        {
          if (chdir (thisarg) < 0)
          {
            if (!really_quiet)
              error (0, 0, "can't chdir to: %s", thisarg);
            continue;
          }
          if (!isdir (CVSADM))
          {
            if (!really_quiet)
              error (0, 0, "no repository module: %s", thisarg);
            continue;
          }
	}
	else
        {
          if (!really_quiet)
            error (0, 0, "no such directory: %s", thisarg);
          continue;
	}

	repository = Name_Repository ((char *) NULL, (char *) NULL);
	srepos = Short_Repository (repository);
        
	if (!really_quiet)
	{
          /* The "release" command piggybacks on "update", which
           * does the real work of finding out if anything is not
           * up-to-date with the repository.  Then "release" prompts
           * the user, telling her how many files have been
           * modified, and asking if she still wants to do the
           * release.
           */
          fp = run_popen (update_cmd, "r");
          c = 0;

          while (fgets (line, sizeof (line), fp))
          {
            if (strchr ("MARCZ", *line))
              c++;
            (void) printf (line);
          }

          /* If the update exited with an error, then we just want to
           * complain and go on to the next arg.  Especially, we do
           * not want to delete the local copy, since it's obviously
           * not what the user thinks it is.
           */
          if ((pclose (fp)) != 0)
          {
            error (0, 0, "unable to release `%s'", thisarg);
            continue;
          }

          (void) printf ("You have [%d] altered files in this repository.\n",
                         c);
          (void) printf ("Are you sure you want to release %smodule `%s': ",
                         delete_flag ? "(and delete) " : "", thisarg);
          c = !yesno ();
          if (c)			/* "No" */
          {
            (void) fprintf (stderr, "** `%s' aborted by user choice.\n",
                            command_name);
            free (repository);
            continue;
          }
	}

	if (1
#ifdef SERVER_SUPPORT
	    && !server_active
#endif
#ifdef CLIENT_SUPPORT
	    && !(client_active
		 && (!supported_request ("noop")
		     || !supported_request ("Notify")))
#endif
	    )
	{
	  /* We are chdir'ed into the directory in question.  
	     So don't pass args to unedit.  */
	  int argc = 1;
	  char *argv[3];
	  argv[0] = "dummy";
	  argv[1] = NULL;
	  err += unedit (argc, argv);
	}

#ifdef CLIENT_SUPPORT
        if (client_active)
        {
          send_to_server ("Argument ", 0);
          send_to_server (thisarg, 0);
          send_to_server ("\012", 1);
          send_to_server ("release\012", 0);
        }
        else
        {
#endif /* CLIENT_SUPPORT */
          history_write ('F', thisarg, "", thisarg, ""); /* F == Free */
#ifdef CLIENT_SUPPORT
        } /* else client not active */
#endif /* CLIENT_SUPPORT */
        
        free (repository);
        if (delete_flag) release_delete (thisarg);
        
#ifdef CLIENT_SUPPORT
        if (client_active)
          return get_responses_and_close ();
        else
#endif /* CLIENT_SUPPORT */
          return (0);
        
#ifdef SERVER_SUPPORT
      } /* else server not active */
#endif  /* SERVER_SUPPORT */
    }   /* `for' loop */
    return err;
}


/* We want to "rm -r" the working directory, but let us be a little
   paranoid.  */
static void
release_delete (dir)
    char *dir;
{
    struct stat st;
    ino_t ino;

    (void) stat (".", &st);
    ino = st.st_ino;
    (void) chdir ("..");
    (void) stat (dir, &st);
    if (ino != st.st_ino)
    {
	error (0, 0,
	       "Parent dir on a different disk, delete of %s aborted", dir);
	return;
    }
    /*
     * XXX - shouldn't this just delete the CVS-controlled files and, perhaps,
     * the files that would normally be ignored and leave everything else?
     */
    if (unlink_file_dir (dir) < 0)
	error (0, errno, "deletion of directory %s failed", dir);
}
