/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 */

#include "cvs.h"
#include "getline.h"

/*
 * Parse the INFOFILE file for the specified REPOSITORY.  Invoke CALLPROC for
 * the first line in the file that matches the REPOSITORY, or if ALL != 0, any lines
 * matching "ALL", or if no lines match, the last line matching "DEFAULT".
 *
 * Return 0 for success, -1 if there was not an INFOFILE, and >0 for failure.
 */
int
Parse_Info (infofile, repository, callproc, all)
    char *infofile;
    char *repository;
    CALLPROC callproc;
    int all;
{
    int err = 0;
    FILE *fp_info;
    char *infopath;
    char *line = NULL;
    size_t line_allocated = 0;
    char *default_value = NULL;
    char *expanded_value= NULL;
    int callback_done, line_number;
    char *cp, *exp, *value, *srepos;
    const char *regex_err;

    if (CVSroot_original == NULL)
    {
	/* XXX - should be error maybe? */
	error (0, 0, "CVSROOT variable not set");
	return (1);
    }

    /* find the info file and open it */
    infopath = xmalloc (strlen (CVSroot_directory)
			+ strlen (infofile)
			+ sizeof (CVSROOTADM)
			+ 10);
    (void) sprintf (infopath, "%s/%s/%s", CVSroot_directory,
		    CVSROOTADM, infofile);
    fp_info = CVS_FOPEN (infopath, "r");
    if (fp_info == NULL)
    {
	/* If no file, don't do anything special.  */
	if (!existence_error (errno))
	    error (0, errno, "cannot open %s", infopath);
	return 0;
    }

    /* strip off the CVSROOT if repository was absolute */
    srepos = Short_Repository (repository);

    if (trace)
	(void) fprintf (stderr, "-> ParseInfo(%s, %s, %s)\n",
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

	expanded_value = expand_path (value, infofile, line_number);
	if (!expanded_value)
	{
	    continue;
	}

	/*
	 * At this point, exp points to the regular expression, and value
	 * points to the value to call the callback routine with.  Evaluate
	 * the regular expression against srepos and callback with the value
	 * if it matches.
	 */

	/* save the default value so we have it later if we need it */
	if (strcmp (exp, "DEFAULT") == 0)
	{
	    default_value = xstrdup (expanded_value);
	    continue;
	}

	/*
	 * For a regular expression of "ALL", do the callback always We may
	 * execute lots of ALL callbacks in addition to *one* regular matching
	 * callback or default
	 */
	if (strcmp (exp, "ALL") == 0)
	{
	    if (all)
		err += callproc (repository, expanded_value);
	    else
		error(0, 0, "Keyword `ALL' is ignored at line %d in %s file",
		      line_number, infofile);
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
	err += callproc (repository, expanded_value);
	callback_done = 1;
    }
    if (ferror (fp_info))
	error (0, errno, "cannot read %s", infopath);
    if (fclose (fp_info) < 0)
	error (0, errno, "cannot close %s", infopath);

    /* if we fell through and didn't callback at all, do the default */
    if (callback_done == 0 && default_value != NULL)
	err += callproc (repository, default_value);

    /* free up space if necessary */
    if (default_value != NULL)
	free (default_value);
    if (expanded_value != NULL)
	free (expanded_value);
    free (infopath);
    if (line != NULL)
	free (line);

    return (err);
}
