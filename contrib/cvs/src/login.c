/*
 * Copyright (c) 1995, Cyclic Software, Bloomington, IN, USA
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with CVS.
 * 
 * Allow user to log in for an authenticating server.
 */

#include "cvs.h"
#include "getline.h"

#ifdef AUTH_CLIENT_SUPPORT   /* This covers the rest of the file. */

/* There seems to be very little agreement on which system header
   getpass is declared in.  With a lot of fancy autoconfiscation,
   we could perhaps detect this, but for now we'll just rely on
   _CRAY, since Cray is perhaps the only system on which our own
   declaration won't work (some Crays declare the 2#$@% thing as
   varadic, believe it or not).  On Cray, getpass will be declared
   in either stdlib.h or unistd.h.  */
#ifndef _CRAY
extern char *getpass ();
#endif

#ifndef CVS_PASSWORD_FILE 
#define CVS_PASSWORD_FILE ".cvspass"
#endif

/* If non-NULL, get_cvs_password() will just return this. */
static char *cvs_password = NULL;

static char *construct_cvspass_filename PROTO ((void));

/* The return value will need to be freed. */
static char *
construct_cvspass_filename ()
{
    char *homedir;
    char *passfile;

    /* Environment should override file. */
    if ((passfile = getenv ("CVS_PASSFILE")) != NULL)
	return xstrdup (passfile);

    /* Construct absolute pathname to user's password file. */
    /* todo: does this work under OS/2 ? */
    homedir = get_homedir ();
    if (! homedir)
    {
	error (1, errno, "could not find out home directory");
	return (char *) NULL;
    }

    passfile =
	(char *) xmalloc (strlen (homedir) + strlen (CVS_PASSWORD_FILE) + 3);
    strcpy (passfile, homedir);
#ifndef NO_SLASH_AFTER_HOME
    /* NO_SLASH_AFTER_HOME is defined for VMS, where foo:[bar]/.cvspass is not
       a legal filename but foo:[bar].cvspass is.  A more clean solution would
       be something more along the lines of a "join a directory to a filename"
       kind of thing....  */
    strcat (passfile, "/");
#endif
    strcat (passfile, CVS_PASSWORD_FILE);

    /* Safety first and last, Scouts. */
    if (isfile (passfile))
	/* xchmod() is too polite. */
	chmod (passfile, 0600);

    return passfile;
}

static const char *const login_usage[] =
{
    "Usage: %s %s\n",
    NULL
};

/* Prompt for a password, and store it in the file "CVS/.cvspass".
 *
 * Because the user might be accessing multiple repositories, with
 * different passwords for each one, the format of ~/.cvspass is:
 *
 * user@host:/path Acleartext_password
 * user@host:/path Acleartext_password
 * ...
 *
 * Of course, the "user@" might be left off -- it's just based on the
 * value of CVSroot.
 *
 * The "A" before "cleartext_password" is a literal capital A.  It's a
 * version number indicating which form of scrambling we're doing on
 * the password -- someday we might provide something more secure than
 * the trivial encoding we do now, and when that day comes, it would
 * be nice to remain backward-compatible.
 *
 * Like .netrc, the file's permissions are the only thing preventing
 * it from being read by others.  Unlike .netrc, we will not be
 * fascist about it, at most issuing a warning, and never refusing to
 * work.
 */
int
login (argc, argv)
    int argc;
    char **argv;
{
    char *passfile;
    FILE *fp;
    char *typed_password, *found_password;
    char *linebuf = (char *) NULL;
    size_t linebuf_len;
    int root_len, already_entered = 0;

    if (argc < 0)
	usage (login_usage);

    if (CVSroot_method != pserver_method)
    {
	error (0, 0, "can only use pserver method with `login' command");
	error (1, 0, "CVSROOT: %s", CVSroot_original);
    }
    
    if (! CVSroot_username)
    {
	error (0, 0, "CVSROOT \"%s\" is not fully-qualified.",
	       CVSroot_original);
	error (1, 0, "Please make sure to specify \"user@host\"!");
    }

    printf ("(Logging in to %s@%s)\n", CVSroot_username, CVSroot_hostname);
    fflush (stdout);

    passfile = construct_cvspass_filename ();
    typed_password = getpass ("CVS password: ");
    typed_password = scramble (typed_password);

    /* Force get_cvs_password() to use this one (when the client
     * confirms the new password with the server), instead of
     * consulting the file.  We make a new copy because cvs_password
     * will get zeroed by connect_to_server().  */

    cvs_password = xstrdup (typed_password);

    if (connect_to_pserver (NULL, NULL, 1) == 0)
    {
	/* The password is wrong, according to the server. */
	error (1, 0, "incorrect password");
    }

    /* IF we have a password for this "[user@]host:/path" already
     *  THEN
     *    IF it's the same as the password we read from the prompt
     *     THEN 
     *       do nothing
     *     ELSE
     *       replace the old password with the new one
     *  ELSE
     *    append new entry to the end of the file.
     */

    root_len = strlen (CVSroot_original);

    /* Yes, the method below reads the user's password file twice.  It's
       inefficient, but we're not talking about a gig of data here. */

    fp = CVS_FOPEN (passfile, "r");
    /* FIXME: should be printing a message if fp == NULL and not
       existence_error (errno).  */
    if (fp != NULL)
    {
	/* Check each line to see if we have this entry already. */
	while (getline (&linebuf, &linebuf_len, fp) >= 0)
        {
          if (strncmp (CVSroot_original, linebuf, root_len) == 0)
            {
		already_entered = 1;
		break;
            }
        }
	fclose (fp);
    }

    if (already_entered)
    {
	/* This user/host has a password in the file already. */

	strtok (linebuf, " ");
	found_password = strtok (NULL, "\n");
	if (strcmp (found_password, typed_password))
        {
	    /* typed_password and found_password don't match, so we'll
	     * have to update passfile.  We replace the old password
	     * with the new one by writing a tmp file whose contents are
	     * exactly the same as passfile except that this one entry
	     * gets typed_password instead of found_password.  Then we
	     * rename the tmp file on top of passfile.
	     */
	    char *tmp_name;
	    FILE *tmp_fp;

	    tmp_name = cvs_temp_name ();
	    if ((tmp_fp = CVS_FOPEN (tmp_name, "w")) == NULL)
            {
		error (1, errno, "unable to open temp file %s", tmp_name);
		return 1;
            }
	    chmod (tmp_name, 0600);

	    fp = CVS_FOPEN (passfile, "r");
	    if (fp == NULL)
            {
		error (1, errno, "unable to open %s", passfile);
		if (linebuf)
		    free (linebuf);
		return 1;
            }
	    /* I'm not paranoid, they really ARE out to get me: */
	    chmod (passfile, 0600);

	    while (getline (&linebuf, &linebuf_len, fp) >= 0)
            {
		if (strncmp (CVSroot_original, linebuf, root_len))
		    fprintf (tmp_fp, "%s", linebuf);
		else
		    fprintf (tmp_fp, "%s %s\n", CVSroot_original,
			     typed_password);

            }
            if (linebuf)
                free (linebuf);
	    fclose (tmp_fp);
	    fclose (fp);
	    copy_file (tmp_name, passfile);
	    unlink_file (tmp_name);
	    chmod (passfile, 0600);
	    free (tmp_name);
        }
    }
    else
    {
	if (linebuf)
	    free (linebuf);
	if ((fp = CVS_FOPEN (passfile, "a")) == NULL)
        {
	    error (1, errno, "could not open %s", passfile);
	    free (passfile);
	    return 1;
        }

	fprintf (fp, "%s %s\n", CVSroot_original, typed_password);
	fclose (fp);
    }

    /* Utter, total, raving paranoia, I know. */
    chmod (passfile, 0600);
    memset (typed_password, 0, strlen (typed_password));
    free (typed_password);

    free (passfile);
    free (cvs_password);
    cvs_password = NULL;
    return 0;
}

/* Returns the _scrambled_ password.  The server must descramble
   before hashing and comparing. */
char *
get_cvs_password ()
{
    int found_it = 0;
    int root_len;
    char *password;
    char *linebuf = (char *) NULL;
    size_t linebuf_len;
    FILE *fp;
    char *passfile;

    /* If someone (i.e., login()) is calling connect_to_pserver() out of
       context, then assume they have supplied the correct, scrambled
       password. */
    if (cvs_password)
	return cvs_password;

    if (getenv ("CVS_PASSWORD") != NULL)
    {
	/* In previous versions of CVS one could specify a password in
	   CVS_PASSWORD.  This is a bad idea, because in BSD variants
	   of unix anyone can see the environment variable with 'ps'.
	   But for users who were using that feature we want to at
	   least let them know what is going on.  After printing this
	   warning, we should fall through to the regular error where
	   we tell them to run "cvs login" (unless they already ran
	   it, of course).  */
	error (0, 0, "CVS_PASSWORD is no longer supported; ignored");
    }

    /* Else get it from the file.  First make sure that the CVSROOT
       variable has the appropriate fields filled in. */

    if (CVSroot_method != pserver_method)
    {
	error (0, 0, "can only call GET_CVS_PASSWORD  with pserver method");
	error (1, 0, "CVSROOT: %s", CVSroot_original);
    }

    if (! CVSroot_username)
    {
	error (0, 0, "CVSROOT \"%s\" is not fully-qualified.",
	       CVSroot_original);
	error (1, 0, "Please make sure to specify \"user@host\"!");
    }

    passfile = construct_cvspass_filename ();
    fp = CVS_FOPEN (passfile, "r");
    if (fp == NULL)
    {
	error (0, errno, "could not open %s", passfile);
	free (passfile);
	error (1, 0, "use \"cvs login\" to log in first");
    }

    root_len = strlen (CVSroot_original);

    /* Check each line to see if we have this entry already. */
    while (getline (&linebuf, &linebuf_len, fp) >= 0)
    {
	if (strncmp (CVSroot_original, linebuf, root_len) == 0)
        {
	    /* This is it!  So break out and deal with linebuf. */
	    found_it = 1;
	    break;
        }
    }

    if (found_it)
    {
	/* linebuf now contains the line with the password. */
	char *tmp;

	strtok (linebuf, " ");
	password = strtok (NULL, "\n");

	/* Give it permanent storage. */
	tmp = xstrdup (password);
	memset (password, 0, strlen (password));
	free (linebuf);
	return tmp;
    }
    else
    {
        if (linebuf)
            free (linebuf);
	error (0, 0, "cannot find password");
	error (1, 0, "use \"cvs login\" to log in first");
    }
    /* NOTREACHED */
    return NULL;
}

static const char *const logout_usage[] =
{
    "Usage: %s %s\n",
    NULL
};

/* Remove any entry for the CVSRoot repository found in "CVS/.cvspass". */
int
logout (argc, argv)
    int argc;
    char **argv;
{
    char *passfile;
    FILE *fp;
    char *tmp_name;
    FILE *tmp_fp;
    char *linebuf = (char *) NULL;
    size_t linebuf_len;
    int root_len, found = 0;

    if (argc < 0)
	usage (logout_usage);

    if (CVSroot_method != pserver_method)
    {
	error (0, 0, "can only use pserver method with `logout' command");
	error (1, 0, "CVSROOT: %s", CVSroot_original);
    }
    
    if (! CVSroot_username)
    {
	error (0, 0, "CVSROOT \"%s\" is not fully-qualified.",
	       CVSroot_original);
	error (1, 0, "Please make sure to specify \"user@host\"!");
    }

    /* Hmm.  Do we want a variant of this command which deletes _all_
       the entries from the current .cvspass?  Might be easier to
       remember than "rm ~/.cvspass" but then again if people are
       mucking with HOME (common in Win95 as the system doesn't set
       it), then this variant of "cvs logout" might give a false sense
       of security, in that it wouldn't delete entries from any
       .cvspass files but the current one.  */

    printf ("(Logging out of %s@%s)\n", CVSroot_username, CVSroot_hostname);
    fflush (stdout);

    /* IF we have a password for this "[user@]host:/path" already
     *  THEN
     *    drop the entry
     *  ELSE
     *    do nothing
     */

    passfile = construct_cvspass_filename ();
    tmp_name = cvs_temp_name ();
    if ((tmp_fp = CVS_FOPEN (tmp_name, "w")) == NULL)
    {
	error (1, errno, "unable to open temp file %s", tmp_name);
	return 1;
    }
    chmod (tmp_name, 0600);

    root_len = strlen (CVSroot_original);

    fp = CVS_FOPEN (passfile, "r");
    if (fp == NULL)
        error (1, errno, "Error opening %s", passfile);

    /* Check each line to see if we have this entry. */
    /* Copy only those lines that do not match this entry */
    while (getline (&linebuf, &linebuf_len, fp) >= 0)
    {
	if (strncmp (CVSroot_original, linebuf, root_len)) 
	    fprintf (tmp_fp, "%s", linebuf);
	else
	    found = TRUE;
    }
    if (linebuf)
        free (linebuf);
    fclose (fp);
    fclose (tmp_fp);

    if (! found) 
    {
	printf ("Entry not found for %s\n", CVSroot_original);
	unlink_file (tmp_name);
    }
    else
    {
	copy_file (tmp_name, passfile);
	unlink_file (tmp_name);
	chmod (passfile, 0600);
    }
    return 0;
}

#endif /* AUTH_CLIENT_SUPPORT from beginning of file. */
