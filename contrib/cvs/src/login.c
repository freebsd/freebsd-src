/*
 * Copyright (c) 1995, Cyclic Software, Bloomington, IN, USA
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with CVS.
 * 
 * Allow user to log in for an authenticating server.
 *
 * $FreeBSD$
 */

#include "cvs.h"
#include "getline.h"

#ifdef AUTH_CLIENT_SUPPORT   /* This covers the rest of the file. */

#ifdef HAVE_GETPASSPHRASE
#define GETPASS getpassphrase
#else
#define GETPASS getpass
#endif

/* There seems to be very little agreement on which system header
   getpass is declared in.  With a lot of fancy autoconfiscation,
   we could perhaps detect this, but for now we'll just rely on
   _CRAY, since Cray is perhaps the only system on which our own
   declaration won't work (some Crays declare the 2#$@% thing as
   varadic, believe it or not).  On Cray, getpass will be declared
   in either stdlib.h or unistd.h.  */
#ifndef _CRAY
extern char *GETPASS ();
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
	/* FIXME?  This message confuses a lot of users, at least
	   on Win95 (which doesn't set HOMEDRIVE and HOMEPATH like
	   NT does).  I suppose the answer for Win95 is to store the
	   passwords in the registry or something (??).  And .cvsrc
	   and such too?  Wonder what WinCVS does (about .cvsrc, the
	   right thing for a GUI is to just store the password in
	   memory only)...  */
	error (1, 0, "could not find out home directory");
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



/*
 * static char *
 * password_entry_parseline (
 *			      const char *cvsroot_canonical,
 *			      const unsigned char warn,
 *			      const int linenumber,
 *			      char *linebuf
 *			     );
 *
 * Internal function used by password_entry_operation.  Parse a single line
 * from a ~/.cvsroot password file and return a pointer to the password if the
 * line refers to the same cvsroot as cvsroot_canonical
 *
 * INPUTS
 *	cvsroot_canonical	the root we are looking for
 *	warn			Boolean: print warnings for invalid lines?
 *	linenumber		the line number for error messages
 *	linebuf			the current line
 *
 * RETURNS
 * 	NULL			if the line doesn't match
 * 	char *password		as a pointer into linebuf
 *
 * NOTES
 *	This function temporarily alters linebuf, so it isn't thread safe when
 *	called on the same linebuf
 */
static char *
password_entry_parseline (cvsroot_canonical, warn, linenumber, linebuf)
    const char *cvsroot_canonical;
    const unsigned char warn;
    const int linenumber;
    char *linebuf;
{
    char *password = NULL;
    char *p;

    /* look for '^/' */
    if (*linebuf == '/')
    {
	/* Yes: slurp '^/\d+\D' and parse the rest of the line according to version number */
	char *q;
	unsigned long int entry_version;

	if (isspace(*(linebuf + 1)))
	    /* special case since strtoul ignores leading white space */
	    entry_version = 0;
	else
	    entry_version = strtoul (linebuf + 1, &q, 10);

	if (q == linebuf + 1)
	    /* no valid digits found by strtoul */
	    entry_version = 0;
	else
	    /* assume a delimiting seperator */
	    q++;

	switch (entry_version)
	{
	    case 1:
		/* this means the same normalize_cvsroot we are using was
		 * used to create this entry.  strcmp is good enough for
		 * us.
		 */
		p = strchr (q, ' ');
		if (p == NULL)
		{
		    if (warn && !really_quiet)
			error (0, 0, "warning: skipping invalid entry in password file at line %d",
				linenumber);
		}
		else
		{
		    *p = '\0';
		    if (strcmp (cvsroot_canonical, q) == 0)
			password = p + 1;
		    *p = ' ';
		}
		break;
	    case ULONG_MAX:
		if (warn && !really_quiet)
		{
		    error (0, errno, "warning: unable to convert version number in password file at line %d",
			    linenumber);
		    error (0, 0, "skipping entry");
		}
		break;
	    case 0:
		if (warn && !really_quiet)
		    error (0, 0, "warning: skipping entry with invalid version string in password file at line %d",
			    linenumber);
		break;
	    default:
		if (warn && !really_quiet)
		    error (0, 0, "warning: skipping entry with unknown version (%lu) in password file at line %d",
			    entry_version, linenumber);
		break;
	}
    }
    else
    {
	/* No: assume:
	 *
	 *	^cvsroot Aencoded_password$
	 *
	 * as header comment specifies and parse accordingly
	 */
	cvsroot_t *tmp_root;
	char *tmp_root_canonical;

	p = strchr (linebuf, ' ');
	if (p == NULL)
	{
	    if (warn && !really_quiet)
		error (0, 0, "warning: skipping invalid entry in password file at line %d", linenumber);
	    return NULL;;
	}

	*p = '\0';
	if ((tmp_root = parse_cvsroot (linebuf)) == NULL)
	{
	    if (warn && !really_quiet)
		error (0, 0, "warning: skipping invalid entry in password file at line %d", linenumber);
	    *p = ' ';
	    return NULL;
	}
	*p = ' ';
	tmp_root_canonical = normalize_cvsroot (tmp_root);
	if (strcmp (cvsroot_canonical, tmp_root_canonical) == 0)
	    password = p + 1;

	free (tmp_root_canonical);
	free_cvsroot_t (tmp_root);
    }

    return password;
}



/*
 * static char *
 * password_entry_operation (
 * 			     password_entry_operation_t operation,
 * 			     cvsroot_t *root,
 * 			     char *newpassword
 * 			    );
 *
 * Search the password file and depending on the value of operation:
 *
 *	Mode				Action
 *	password_entry_lookup		Return the password
 *	password_entry_delete		Delete the entry from the file, if it exists
 *	password_entry_add		Replace the line with the new one, else append it
 *
 * Because the user might be accessing multiple repositories, with
 * different passwords for each one, the format of ~/.cvspass is:
 *
 * [user@]host:[port]/path Aencoded_password
 * [user@]host:[port]/path Aencoded_password
 * ...
 *
 * New entries are always of the form:
 *
 * /1 user@host:port/path Aencoded_password
 *
 * but the old format is supported for backwards compatibility.
 * The entry version string wasn't strictly necessary, but it avoids the
 * overhead of parsing some entries since we know it is already in canonical
 * form and allows room for expansion later, say, if we want to allow spaces
 * and/or other characters to be escaped in the string.  Also, the new entries
 * would have been ignored by old versions of CVS anyhow since those versions
 * didn't know how to parse a port number.
 *
 * The "A" before "encoded_password" is a literal capital A.  It's a
 * version number indicating which form of scrambling we're doing on
 * the password -- someday we might provide something more secure than
 * the trivial encoding we do now, and when that day comes, it would
 * be nice to remain backward-compatible.
 *
 * Like .netrc, the file's permissions are the only thing preventing
 * it from being read by others.  Unlike .netrc, we will not be
 * fascist about it, at most issuing a warning, and never refusing to
 * work.
 *
 * INPUTS
 * 	operation	operation to perform
 * 	root		cvsroot_t to look up
 * 	newpassword	prescrambled new password, for password_entry_add_mode
 *
 * RETURNS
 * 	-1	if password_entry_lookup_mode not specified
 * 	NULL	on failed lookup
 * 	pointer to a copy of the password string otherwise, which the caller is
 * 		responsible for disposing of
 */

typedef enum password_entry_operation_e {
    password_entry_lookup,
    password_entry_delete,
    password_entry_add
} password_entry_operation_t;

static char *
password_entry_operation (operation, root, newpassword)
    password_entry_operation_t operation;
    cvsroot_t *root;
    char *newpassword;
{
    char *passfile;
    FILE *fp;
    char *cvsroot_canonical = NULL;
    char *password = NULL;
    int line_length;
    long line;
    char *linebuf = NULL;
    size_t linebuf_len;
    char *p;
    int save_errno = 0;

    if (root->method != pserver_method)
    {
	error (0, 0, "internal error: can only call password_entry_operation with pserver method");
	error (1, 0, "CVSROOT: %s", root->original);
    }

    /* Yes, the method below reads the user's password file twice when we have
     * to delete an entry.  It's inefficient, but we're not talking about a gig of
     * data here.
     */

    passfile = construct_cvspass_filename ();
    fp = CVS_FOPEN (passfile, "r");
    if (fp == NULL)
    {
	error (0, errno, "warning: failed to open %s for reading", passfile);
	goto process;
    }

    cvsroot_canonical = normalize_cvsroot (root);

    /* Check each line to see if we have this entry already. */
    line = 0;
    while ((line_length = getline (&linebuf, &linebuf_len, fp)) >= 0)
    {
	line++;
	password = password_entry_parseline(cvsroot_canonical, 1, line, linebuf);
	if (password != NULL)
	    /* this is it!  break out and deal with linebuf */
	    break;
    }
    if (line_length < 0 && !feof (fp))
    {
	error (0, errno, "cannot read %s", passfile);
	goto error_exit;
    }
    if (fclose (fp) < 0)
	/* not fatal, unless it cascades */
	error (0, errno, "cannot close %s", passfile);
    fp = NULL;

    /* Utter, total, raving paranoia, I know. */
    chmod (passfile, 0600);

    /* a copy to return or keep around so we can reuse linebuf */
    if (password != NULL)
    {
	/* chomp the EOL */
	p = strchr (password, '\n');
	if (p != NULL)
	    *p = '\0';
	password = xstrdup (password);
    }

process:

    /* might as well return now */
    if (operation == password_entry_lookup)
	goto out;

    /* same here */
    if (operation == password_entry_delete && password == NULL)
    {
	error (0, 0, "Entry not found.");
	goto out;
    }

    /* okay, file errors can simply be fatal from now on since we don't do
     * anything else if we're in lookup mode
     */

    /* copy the file with the entry deleted unless we're in add
     * mode and the line we found contains the same password we're supposed to
     * add
     */
    if (!noexec && password != NULL && (operation == password_entry_delete
	    || (operation == password_entry_add && strcmp (password, newpassword))))
    {
	long found_at = line;
	char *tmp_name;
	FILE *tmp_fp;

	/* open the original file again */
	fp = CVS_FOPEN (passfile, "r");
	if (fp == NULL)
	    error (1, errno, "failed to open %s for reading", passfile);

	/* create and open a temp file */
	if ((tmp_fp = cvs_temp_file (&tmp_name)) == NULL)
	    error (1, errno, "unable to open temp file %s", tmp_name);

	line = 0;
	while ((line_length = getline (&linebuf, &linebuf_len, fp)) >= 0)
	{
	    line++;
	    if (line < found_at
		|| (line != found_at
		    && !password_entry_parseline(cvsroot_canonical, 0, line, linebuf)))
	    {
		if (fprintf (tmp_fp, "%s", linebuf) == EOF)
		{
		    /* try and clean up anyhow */
		    error (0, errno, "fatal error: cannot write %s", tmp_name);
		    if (fclose (tmp_fp) == EOF)
			error (0, errno, "cannot close %s", tmp_name);
		    /* call CVS_UNLINK instead of unlink_file since the file
		     * got created in noexec mode
		     */
		    if (CVS_UNLINK (tmp_name) < 0)
			error (0, errno, "cannot remove %s", tmp_name);
		    /* but quit so we don't remove all the entries from a
		     * user's password file accidentally
		     */
		    error (1, 0, "exiting");
		}
	    }
	}
	if (line_length < 0 && !feof (fp))
	{
	    error (0, errno, "cannot read %s", passfile);
	    goto error_exit;
	}
	if (fclose (fp) < 0)
	    /* not fatal, unless it cascades */
	    error (0, errno, "cannot close %s", passfile);
	if (fclose (tmp_fp) < 0)
	    /* not fatal, unless it cascades */
	    /* FIXME - does copy_file return correct results if the file wasn't
	     * closed? should this be fatal?
	     */
	    error (0, errno, "cannot close %s", tmp_name);

	/* FIXME: rename_file would make more sense (e.g. almost
	 * always faster).
	 *
	 * I don't think so, unless we change the way rename_file works to
	 * attempt a cp/rm sequence when rename fails since rename doesn't
	 * work across file systems and it isn't uncommon to have /tmp
	 * on its own partition.
	 *
	 * For that matter, it's probably not uncommon to have a home
	 * directory on an NFS mount.
	 */
	copy_file (tmp_name, passfile);
	if (CVS_UNLINK (tmp_name) < 0)
	    error (0, errno, "cannot remove %s", tmp_name);
	free (tmp_name);
    }

    /* in add mode, if we didn't find an entry or found an entry with a
     * different password, append the new line
     */
    if (!noexec && operation == password_entry_add
	    && (password == NULL || strcmp (password, newpassword)))
    {
	if ((fp = CVS_FOPEN (passfile, "a")) == NULL)
	    error (1, errno, "could not open %s for writing", passfile);

	if (fprintf (fp, "/1 %s %s\n", cvsroot_canonical, newpassword) == EOF)
	    error (1, errno, "cannot write %s", passfile);
	if (fclose (fp) < 0)
	    error (0, errno, "cannot close %s", passfile);
    }

    /* Utter, total, raving paranoia, I know. */
    chmod (passfile, 0600);

    if (password)
    {
	free (password);
	password = NULL;
    }
    if (linebuf)
	free (linebuf);

out:
    free (cvsroot_canonical);
    free (passfile);
    return password;

error_exit:
    /* just exit when we're not in lookup mode */
    if (operation != password_entry_lookup)
	error (1, 0, "fatal error: exiting");
    /* clean up and exit in lookup mode so we can try a login with a NULL
     * password anyhow in case that's what we would have found
     */
    save_errno = errno;
    if (fp != NULL)
    {
	/* Utter, total, raving paranoia, I know. */
	chmod (passfile, 0600);
	if(fclose (fp) < 0)
	    error (0, errno, "cannot close %s", passfile);
    }
    if (linebuf)
	free (linebuf);
    if (cvsroot_canonical)
	free (cvsroot_canonical);
    free (passfile);
    errno = save_errno;
    return NULL;
}



/* Prompt for a password, and store it in the file "CVS/.cvspass".
 */

static const char *const login_usage[] =
{
    "Usage: %s %s\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int
login (argc, argv)
    int argc;
    char **argv;
{
    char *typed_password;
    char *cvsroot_canonical;

    if (argc < 0)
	usage (login_usage);

    if (current_parsed_root->method != pserver_method)
    {
	error (0, 0, "can only use `login' command with the 'pserver' method");
	error (1, 0, "CVSROOT: %s", current_parsed_root->original);
    }

    cvsroot_canonical = normalize_cvsroot(current_parsed_root);
    printf ("Logging in to %s\n", cvsroot_canonical);
    fflush (stdout);

    if (current_parsed_root->password)
    {
	typed_password = scramble (current_parsed_root->password);
    }
    else
    {
	char *tmp;
	tmp = GETPASS ("CVS password: ");
	/* Must deal with a NULL return value here.  I haven't managed to
	 * disconnect the CVS process from the tty and force a NULL return
	 * in sanity.sh, but the Linux version of getpass is documented
	 * to return NULL when it can't open /dev/tty...
	 */
	if (!tmp) error (1, errno, "login: Failed to read password.");
	typed_password = scramble (tmp);
	memset (tmp, 0, strlen (tmp));
    }

    /* Force get_cvs_password() to use this one (when the client
     * confirms the new password with the server), instead of
     * consulting the file.  We make a new copy because cvs_password
     * will get zeroed by connect_to_server().  */
    cvs_password = xstrdup (typed_password);

    connect_to_pserver (current_parsed_root, NULL, NULL, 1, 0);

    password_entry_operation (password_entry_add, current_parsed_root, typed_password);

    memset (typed_password, 0, strlen (typed_password));
    free (typed_password);

    free (cvs_password);
    free (cvsroot_canonical);
    cvs_password = NULL;

    return 0;
}

/* Returns the _scrambled_ password.  The server must descramble
   before hashing and comparing.  If password file not found, or
   password not found in the file, just return NULL. */
char *
get_cvs_password ()
{
    if (current_parsed_root->password)
	return (scramble(current_parsed_root->password));
 
    /* If someone (i.e., login()) is calling connect_to_pserver() out of
       context, then assume they have supplied the correct, scrambled
       password. */
    if (cvs_password)
	return cvs_password;

    if (getenv ("CVS_PASSWORD") != NULL)
    {
	/* In previous versions of CVS one could specify a password in
	 * CVS_PASSWORD.  This is a bad idea, because in BSD variants
	 * of unix anyone can see the environment variable with 'ps'.
	 * But for users who were using that feature we want to at
	 * least let them know what is going on.  After printing this
	 * warning, we should fall through to the regular error where
	 * we tell them to run "cvs login" (unless they already ran
	 * it, of course).
	 */
	 error (0, 0, "CVS_PASSWORD is no longer supported; ignored");
    }

    if (current_parsed_root->method != pserver_method)
    {
	error (0, 0, "can only call get_cvs_password with pserver method");
	error (1, 0, "CVSROOT: %s", current_parsed_root->original);
    }

    return password_entry_operation (password_entry_lookup, current_parsed_root, NULL);
}

static const char *const logout_usage[] =
{
    "Usage: %s %s\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

/* Remove any entry for the CVSRoot repository found in .cvspass. */
int
logout (argc, argv)
    int argc;
    char **argv;
{
    char *cvsroot_canonical;

    if (argc < 0)
	usage (logout_usage);

    if (current_parsed_root->method != pserver_method)
    {
	error (0, 0, "can only use pserver method with `logout' command");
	error (1, 0, "CVSROOT: %s", current_parsed_root->original);
    }

    /* Hmm.  Do we want a variant of this command which deletes _all_
       the entries from the current .cvspass?  Might be easier to
       remember than "rm ~/.cvspass" but then again if people are
       mucking with HOME (common in Win95 as the system doesn't set
       it), then this variant of "cvs logout" might give a false sense
       of security, in that it wouldn't delete entries from any
       .cvspass files but the current one.  */

    if (!quiet)
    {
	cvsroot_canonical = normalize_cvsroot(current_parsed_root);
	printf ("Logging out of %s\n", cvsroot_canonical);
	fflush (stdout);
	free (cvsroot_canonical);
    }

    password_entry_operation (password_entry_delete, current_parsed_root, NULL);

    return 0;
}

#endif /* AUTH_CLIENT_SUPPORT from beginning of file. */
