/*
 * .cvsignore file support contributed by David G. Grubbs <dgg@ksr.com>
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)ignore.c 1.13 92/04/03";
#endif

/*
 * Ignore file section.
 * 
 *	"!" may be included any time to reset the list (i.e. ignore nothing);
 *	"*" may be specified to ignore everything.  It stays as the first
 *	    element forever, unless a "!" clears it out.
 */

static char **ign_list;			/* List of files to ignore in update
					 * and import */
static char **s_ign_list = NULL;
static int ign_count;			/* Number of active entries */
static int s_ign_count = 0;
static int ign_size;			/* This many slots available (plus
					 * one for a NULL) */
static int ign_hold;			/* Index where first "temporary" item
					 * is held */

char *ign_default = ". .. core RCSLOG tags TAGS RCS SCCS .make.state .nse_depinfo #* .#* cvslog.* ,* CVS* .del-* *.a *.o *.so *.Z *~ *.old *.elc *.ln *.bak *.BAK *.orig *.rej";

#define IGN_GROW 16			/* grow the list by 16 elements at a
					 * time */

/*
 * To the "ignore list", add the hard-coded default ignored wildcards above,
 * the wildcards found in $CVSROOT/CVSROOT/cvsignore, the wildcards found in
 * ~/.cvsignore and the wildcards found in the CVSIGNORE environment
 * variable.
 */
void
ign_setup ()
{
    extern char *getenv ();
    struct passwd *pw;
    char file[PATH_MAX];
    char *tmp;

    /* Start with default list and special case */
    tmp = xstrdup (ign_default);
    ign_add (tmp, 0);
    free (tmp);

    /* Then add entries found in repository, if it exists */
    (void) sprintf (file, "%s/%s/%s", CVSroot, CVSROOTADM, CVSROOTADM_IGNORE);
    if (isfile (file))
	ign_add_file (file, 0);

    /* Then add entries found in home dir, (if user has one) and file exists */
    if ((pw = (struct passwd *) getpwuid (getuid ())) && pw->pw_dir)
    {
	(void) sprintf (file, "%s/%s", pw->pw_dir, CVSDOTIGNORE);
	if (isfile (file))
	    ign_add_file (file, 0);
    }

    /* Then add entries found in CVSIGNORE environment variable. */
    ign_add (getenv (IGNORE_ENV), 0);

    /* Later, add ignore entries found in -I arguments */
}

/*
 * Open a file and read lines, feeding each line to a line parser. Arrange
 * for keeping a temporary list of wildcards at the end, if the "hold"
 * argument is set.
 */
void
ign_add_file (file, hold)
    char *file;
    int hold;
{
    FILE *fp;
    char line[1024];

    /* restore the saved list (if any) */
    if (s_ign_list != NULL)
    {
	int i;

	for (i = 0; i < s_ign_count; i++)
	    ign_list[i] = s_ign_list[i];
	ign_count = s_ign_count;
	ign_list[ign_count] = NULL;

	s_ign_count = 0;
	free (s_ign_list);
	s_ign_list = NULL;
    }

    /* is this a temporary ignore file? */
    if (hold)
    {
	/* re-set if we had already done a temporary file */
	if (ign_hold)
	{
	    int i;

	    for (i = ign_hold; i < ign_count; i++)
		free (ign_list[i]);
	    ign_count = ign_hold;
	    ign_list[ign_count] = NULL;
	}
	else
	{
	    ign_hold = ign_count;
	}
    }

    /* load the file */
    if (!(fp = fopen (file, "r")))
	return;
    while (fgets (line, sizeof (line), fp))
	ign_add (line, hold);
    (void) fclose (fp);
}

/* Parse a line of space-separated wildcards and add them to the list. */
void
ign_add (ign, hold)
    char *ign;
    int hold;
{
    if (!ign || !*ign)
	return;

    for (; *ign; ign++)
    {
	char *mark;
	char save;

	/* ignore whitespace before the token */
	if (isspace (*ign))
	    continue;

	/*
	 * if we find a single character !, we must re-set the ignore list
	 * (saving it if necessary).  We also catch * as a special case in a
	 * global ignore file as an optimization
	 */
	if (isspace (*(ign + 1)) && (*ign == '!' || *ign == '*'))
	{
	    if (!hold)
	    {
		/* permanently reset the ignore list */
		int i;

		for (i = 0; i < ign_count; i++)
		    free (ign_list[i]);
		ign_count = 0;
		ign_list[0] = NULL;

		/* if we are doing a '!', continue; otherwise add the '*' */
		if (*ign == '!')
		    continue;
	    }
	    else if (*ign == '!')
	    {
		/* temporarily reset the ignore list */
		int i;

		if (ign_hold)
		{
		    for (i = ign_hold; i < ign_count; i++)
			free (ign_list[i]);
		    ign_hold = 0;
		}
		s_ign_list = (char **) xmalloc (ign_count * sizeof (char *));
		for (i = 0; i < ign_count; i++)
		    s_ign_list[i] = ign_list[i];
		s_ign_count = ign_count;
		ign_count = 0;
		ign_list[0] = NULL;
		continue;
	    }
	}

	/* If we have used up all the space, add some more */
	if (ign_count >= ign_size)
	{
	    ign_size += IGN_GROW;
	    ign_list = (char **) xrealloc ((char *) ign_list,
					   (ign_size + 1) * sizeof (char *));
	}

	/* find the end of this token */
	for (mark = ign; *mark && !isspace (*mark); mark++)
	     /* do nothing */ ;

	save = *mark;
	*mark = '\0';

	ign_list[ign_count++] = xstrdup (ign);
	ign_list[ign_count] = NULL;

	*mark = save;
	if (save)
	    ign = mark;
	else
	    ign = mark - 1;
    }
}

/* Return 1 if the given filename should be ignored by update or import. */
int
ign_name (name)
    char *name;
{
    char **cpp = ign_list;

    if (cpp == NULL)
	return (0);

    while (*cpp)
	if (fnmatch (*cpp++, name, 0) == 0)
	    return (1);
    return (0);
}
