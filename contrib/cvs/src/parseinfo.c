/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 *
 * $FreeBSD$
 */

#include "cvs.h"
#include "getline.h"
#include <assert.h>

extern char *logHistory;

/*
 * Parse the INFOFILE file for the specified REPOSITORY.  Invoke CALLPROC for
 * the first line in the file that matches the REPOSITORY, or if ALL != 0, any
 * lines matching "ALL", or if no lines match, the last line matching
 * "DEFAULT".
 *
 * Return 0 for success, -1 if there was not an INFOFILE, and >0 for failure.
 */
int
Parse_Info (infofile, repository, callproc, all)
    const char *infofile;
    const char *repository;
    CALLPROC callproc;
    int all;
{
    int err = 0;
    FILE *fp_info;
    char *infopath;
    char *line = NULL;
    size_t line_allocated = 0;
    char *default_value = NULL;
    int default_line = 0;
    char *expanded_value;
    int callback_done, line_number;
    char *cp, *exp, *value;
    const char *srepos;
    const char *regex_err;

    if (current_parsed_root == NULL)
    {
	/* XXX - should be error maybe? */
	error (0, 0, "CVSROOT variable not set");
	return (1);
    }

    /* find the info file and open it */
    infopath = xmalloc (strlen (current_parsed_root->directory)
			+ strlen (infofile)
			+ sizeof (CVSROOTADM)
			+ 3);
    (void) sprintf (infopath, "%s/%s/%s", current_parsed_root->directory,
		    CVSROOTADM, infofile);
    fp_info = CVS_FOPEN (infopath, "r");
    if (fp_info == NULL)
    {
	/* If no file, don't do anything special.  */
	if (!existence_error (errno))
	    error (0, errno, "cannot open %s", infopath);
	free (infopath);
	return 0;
    }

    /* strip off the CVSROOT if repository was absolute */
    srepos = Short_Repository (repository);

    if (trace)
	(void) fprintf (stderr, "%s-> Parse_Info (%s, %s, %s)\n",
#ifdef SERVER_SUPPORT
			server_active ? "S" : " ",
#else
			"",
#endif
			infopath, srepos, all ? "ALL" : "not ALL");

    /* search the info file for lines that match */
    callback_done = line_number = 0;
    while (getline (&line, &line_allocated, fp_info) >= 0)
    {
	line_number++;

	/* skip lines starting with # */
	if (line[0] == '#')
	    continue;

	/* skip whitespace at beginning of line */
	for (cp = line; *cp && isspace ((unsigned char) *cp); cp++)
	    ;

	/* if *cp is null, the whole line was blank */
	if (*cp == '\0')
	    continue;

	/* the regular expression is everything up to the first space */
	for (exp = cp; *cp && !isspace ((unsigned char) *cp); cp++)
	    ;
	if (*cp != '\0')
	    *cp++ = '\0';

	/* skip whitespace up to the start of the matching value */
	while (*cp && isspace ((unsigned char) *cp))
	    cp++;

	/* no value to match with the regular expression is an error */
	if (*cp == '\0')
	{
	    error (0, 0, "syntax error at line %d file %s; ignored",
		   line_number, infofile);
	    continue;
	}
	value = cp;

	/* strip the newline off the end of the value */
	if ((cp = strrchr (value, '\n')) != NULL)
	    *cp = '\0';

	/*
	 * At this point, exp points to the regular expression, and value
	 * points to the value to call the callback routine with.  Evaluate
	 * the regular expression against srepos and callback with the value
	 * if it matches.
	 */

	/* save the default value so we have it later if we need it */
	if (strcmp (exp, "DEFAULT") == 0)
	{
	    if (default_value != NULL)
	    {
		error (0, 0, "Multiple `DEFAULT' lines (%d and %d) in %s file",
		       default_line, line_number, infofile);
		free (default_value);
	    }
	    default_value = xstrdup(value);
	    default_line = line_number;
	    continue;
	}

	/*
	 * For a regular expression of "ALL", do the callback always We may
	 * execute lots of ALL callbacks in addition to *one* regular matching
	 * callback or default
	 */
	if (strcmp (exp, "ALL") == 0)
	{
	    if (!all)
		error(0, 0, "Keyword `ALL' is ignored at line %d in %s file",
		      line_number, infofile);
	    else if ((expanded_value = expand_path (value, infofile,
                                                    line_number)) != NULL)
	    {
		err += callproc (repository, expanded_value);
		free (expanded_value);
	    }
	    else
		err++;
	    continue;
	}

	if (callback_done)
	    /* only first matching, plus "ALL"'s */
	    continue;

	/* see if the repository matched this regular expression */
	if ((regex_err = re_comp (exp)) != NULL)
	{
	    error (0, 0, "bad regular expression at line %d file %s: %s",
		   line_number, infofile, regex_err);
	    continue;
	}
	if (re_exec (srepos) == 0)
	    continue;				/* no match */

	/* it did, so do the callback and note that we did one */
	if ((expanded_value = expand_path (value, infofile, line_number)) != NULL)
	{
	    err += callproc (repository, expanded_value);
	    free (expanded_value);
	}
	else
	    err++;
	callback_done = 1;
    }
    if (ferror (fp_info))
	error (0, errno, "cannot read %s", infopath);
    if (fclose (fp_info) < 0)
	error (0, errno, "cannot close %s", infopath);

    /* if we fell through and didn't callback at all, do the default */
    if (callback_done == 0 && default_value != NULL)
    {
	if ((expanded_value = expand_path (default_value, infofile, default_line)) != NULL)
	{
	    err += callproc (repository, expanded_value);
	    free (expanded_value);
	}
	else
	    err++;
    }

    /* free up space if necessary */
    if (default_value != NULL)
	free (default_value);
    free (infopath);
    if (line != NULL)
	free (line);

    return (err);
}


/* Parse the CVS config file.  The syntax right now is a bit ad hoc
   but tries to draw on the best or more common features of the other
   *info files and various unix (or non-unix) config file syntaxes.
   Lines starting with # are comments.  Settings are lines of the form
   KEYWORD=VALUE.  There is currently no way to have a multi-line
   VALUE (would be nice if there was, probably).

   CVSROOT is the $CVSROOT directory
   (current_parsed_root->directory might not be set yet, so this
   function takes the cvsroot as a function argument).

   Returns 0 for success, negative value for failure.  Call
   error(0, ...) on errors in addition to the return value.  */
int
parse_config (cvsroot)
    char *cvsroot;
{
    char *infopath;
    FILE *fp_info;
    char *line = NULL;
    size_t line_allocated = 0;
    size_t len;
    char *p;
    /* FIXME-reentrancy: If we do a multi-threaded server, this would need
       to go to the per-connection data structures.  */
    static int parsed = 0;

    /* Authentication code and serve_root might both want to call us.
       Let this happen smoothly.  */
    if (parsed)
	return 0;
    parsed = 1;

    infopath = xmalloc (strlen (cvsroot)
			+ sizeof (CVSROOTADM_CONFIG)
			+ sizeof (CVSROOTADM)
			+ 10);
    if (infopath == NULL)
    {
	error (0, 0, "out of memory; cannot allocate infopath");
	goto error_return;
    }

    strcpy (infopath, cvsroot);
    strcat (infopath, "/");
    strcat (infopath, CVSROOTADM);
    strcat (infopath, "/");
    strcat (infopath, CVSROOTADM_CONFIG);

    fp_info = CVS_FOPEN (infopath, "r");
    if (fp_info == NULL)
    {
	/* If no file, don't do anything special.  */
	if (!existence_error (errno))
	{
	    /* Just a warning message; doesn't affect return
	       value, currently at least.  */
	    error (0, errno, "cannot open %s", infopath);
	}
	free (infopath);
	return 0;
    }

    while (getline (&line, &line_allocated, fp_info) >= 0)
    {
	/* Skip comments.  */
	if (line[0] == '#')
	    continue;

	/* At least for the moment we don't skip whitespace at the start
	   of the line.  Too picky?  Maybe.  But being insufficiently
	   picky leads to all sorts of confusion, and it is a lot easier
	   to start out picky and relax it than the other way around.

	   Is there any kind of written standard for the syntax of this
	   sort of config file?  Anywhere in POSIX for example (I guess
	   makefiles are sort of close)?  Red Hat Linux has a bunch of
	   these too (with some GUI tools which edit them)...

	   Along the same lines, we might want a table of keywords,
	   with various types (boolean, string, &c), as a mechanism
	   for making sure the syntax is consistent.  Any good examples
	   to follow there (Apache?)?  */

	/* Strip the trailing newline.  There will be one unless we
	   read a partial line without a newline, and then got end of
	   file (or error?).  */

	len = strlen (line) - 1;
	if (line[len] == '\n')
	    line[len] = '\0';

	/* Skip blank lines.  */
	if (line[0] == '\0')
	    continue;

	/* The first '=' separates keyword from value.  */
	p = strchr (line, '=');
	if (p == NULL)
	{
	    /* Probably should be printing line number.  */
	    error (0, 0, "syntax error in %s: line '%s' is missing '='",
		   infopath, line);
	    goto error_return;
	}

	*p++ = '\0';

	if (strcmp (line, "RCSBIN") == 0)
	{
	    /* This option used to specify the directory for RCS
	       executables.  But since we don't run them any more,
	       this is a noop.  Silently ignore it so that a
	       repository can work with either new or old CVS.  */
	    ;
	}
	else if (strcmp (line, "SystemAuth") == 0)
	{
	    if (strcmp (p, "no") == 0)
#ifdef AUTH_SERVER_SUPPORT
		system_auth = 0;
#else
		/* Still parse the syntax but ignore the
		   option.  That way the same config file can
		   be used for local and server.  */
		;
#endif
	    else if (strcmp (p, "yes") == 0)
#ifdef AUTH_SERVER_SUPPORT
		system_auth = 1;
#else
		;
#endif
	    else
	    {
		error (0, 0, "unrecognized value '%s' for SystemAuth", p);
		goto error_return;
	    }
	}
	else if (strcmp (line, "tag") == 0) {
		RCS_setlocalid(p);
	}
	else if (strcmp (line, "umask") == 0) {
	    cvsumask = (mode_t)(strtol(p, NULL, 8) & 0777);
	}
        else if (strcmp (line, "dlimit") == 0) {
#ifdef BSD
#include <sys/resource.h>
	    struct rlimit rl;

	    if (getrlimit(RLIMIT_DATA, &rl) != -1) {
		rl.rlim_cur = atoi(p);
		rl.rlim_cur *= 1024;

		(void) setrlimit(RLIMIT_DATA, &rl);
	    }
#endif /* BSD */
	}
	else if (strcmp (line, "PreservePermissions") == 0)
	{
	    if (strcmp (p, "no") == 0)
		preserve_perms = 0;
	    else if (strcmp (p, "yes") == 0)
	    {
#ifdef PRESERVE_PERMISSIONS_SUPPORT
		preserve_perms = 1;
#else
		error (0, 0, "\
warning: this CVS does not support PreservePermissions");
#endif
	    }
	    else
	    {
		error (0, 0, "unrecognized value '%s' for PreservePermissions",
		       p);
		goto error_return;
	    }
	}
	else if (strcmp (line, "TopLevelAdmin") == 0)
	{
	    if (strcmp (p, "no") == 0)
		top_level_admin = 0;
	    else if (strcmp (p, "yes") == 0)
		top_level_admin = 1;
	    else
	    {
		error (0, 0, "unrecognized value '%s' for TopLevelAdmin", p);
		goto error_return;
	    }
	}
	else if (strcmp (line, "LockDir") == 0)
	{
	    if (lock_dir != NULL)
		free (lock_dir);
	    lock_dir = xstrdup (p);
	    /* Could try some validity checking, like whether we can
	       opendir it or something, but I don't see any particular
	       reason to do that now rather than waiting until lock.c.  */
	}
	else if (strcmp (line, "LogHistory") == 0)
	{
	    if (strcmp (p, "all") != 0)
	    {
		logHistory=xmalloc(strlen (p) + 1);
		strcpy (logHistory, p);
	    }
	}
	else if (strcmp (line, "RereadLogAfterVerify") == 0)
	{
	    if (strcmp (p, "no") == 0 || strcmp (p, "never") == 0)
	      RereadLogAfterVerify = LOGMSG_REREAD_NEVER;
	    else if (strcmp (p, "yes") == 0 || strcmp (p, "always") == 0)
	      RereadLogAfterVerify = LOGMSG_REREAD_ALWAYS;
	    else if (strcmp (p, "stat") == 0)
	      RereadLogAfterVerify = LOGMSG_REREAD_STAT;
	}
	else
	{
	    /* We may be dealing with a keyword which was added in a
	       subsequent version of CVS.  In that case it is a good idea
	       to complain, as (1) the keyword might enable a behavior like
	       alternate locking behavior, in which it is dangerous and hard
	       to detect if some CVS's have it one way and others have it
	       the other way, (2) in general, having us not do what the user
	       had in mind when they put in the keyword violates the
	       principle of least surprise.  Note that one corollary is
	       adding new keywords to your CVSROOT/config file is not
	       particularly recommended unless you are planning on using
	       the new features.  */
	    error (0, 0, "%s: unrecognized keyword '%s'",
		   infopath, line);
	    goto error_return;
	}
    }
    if (ferror (fp_info))
    {
	error (0, errno, "cannot read %s", infopath);
	goto error_return;
    }
    if (fclose (fp_info) < 0)
    {
	error (0, errno, "cannot close %s", infopath);
	goto error_return;
    }
    free (infopath);
    if (line != NULL)
	free (line);
    return 0;

 error_return:
    if (infopath != NULL)
	free (infopath);
    if (line != NULL)
	free (line);
    return -1;
}
