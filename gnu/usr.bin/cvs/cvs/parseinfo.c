/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "$CVSid: @(#)parseinfo.c 1.18 94/09/23 $";
USE(rcsid)
#endif

/*
 * Parse the INFOFILE file for the specified REPOSITORY.  Invoke CALLPROC for
 * each line in the file that matches the REPOSITORY.  
 * Return 0 for success, -1 if there was not an INFOFILE, and >0 for failure.
 */
int
Parse_Info (infofile, repository, callproc, all)
    char *infofile;
    char *repository;
    int (*callproc) ();
    int all;
{
    int err = 0;
    FILE *fp_info;
    char infopath[PATH_MAX];
    char line[MAXLINELEN];
    char *default_value = NULL;
    int callback_done, line_number;
    char *cp, *exp, *value, *srepos;
    CONST char *regex_err;

    if (CVSroot == NULL)
    {
	/* XXX - should be error maybe? */
	error (0, 0, "CVSROOT variable not set");
	return (1);
    }

    /* find the info file and open it */
    (void) sprintf (infopath, "%s/%s/%s", CVSroot,
		    CVSROOTADM, infofile);
    if ((fp_info = fopen (infopath, "r")) == NULL)
	return (0);			/* no file -> nothing special done */

    /* strip off the CVSROOT if repository was absolute */
    srepos = Short_Repository (repository);

    /* search the info file for lines that match */
    callback_done = line_number = 0;
    while (fgets (line, sizeof (line), fp_info) != NULL)
    {
	line_number++;

	/* skip lines starting with # */
	if (line[0] == '#')
	    continue;

	/* skip whitespace at beginning of line */
	for (cp = line; *cp && isspace (*cp); cp++)
	    ;

	/* if *cp is null, the whole line was blank */
	if (*cp == '\0')
	    continue;

	/* the regular expression is everything up to the first space */
	for (exp = cp; *cp && !isspace (*cp); cp++)
	    ;
	if (*cp != '\0')
	    *cp++ = '\0';

	/* skip whitespace up to the start of the matching value */
	while (*cp && isspace (*cp))
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
	    default_value = xstrdup (value);
	    continue;
	}

	/*
	 * For a regular expression of "ALL", do the callback always We may
	 * execute lots of ALL callbacks in addition to one regular matching
	 * callback or default
	 */
	if (strcmp (exp, "ALL") == 0)
	{
	    if (all)
		err += callproc (repository, value);
	    else
		error(0, 0, "Keyword `ALL' is ignored at line %d in %s file",
		      line_number, infofile);
	    continue;
	}

	/* see if the repository matched this regular expression */
	if ((regex_err = re_comp (exp)) != NULL)
	{
	    error (0, 0, "bad regular expression at line %d file %s: %s",
		   line_number, infofile, regex_err);
	    continue;
	}
	if (re_exec (srepos) == 0)
	    continue;			/* no match */

	/* it did, so do the callback and note that we did one */
	err += callproc (repository, value);
	callback_done = 1;
    }
    (void) fclose (fp_info);

    /* if we fell through and didn't callback at all, do the default */
    if (callback_done == 0 && default_value != NULL)
	err += callproc (repository, default_value);

    /* free up space if necessary */
    if (default_value != NULL)
	free (default_value);

    return (err);
}
