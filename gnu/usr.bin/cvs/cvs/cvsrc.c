/*
 *    Copyright (c) 1993 david d zuhn
 *
 *    written by david d `zoo' zuhn while at Cygnus Support
 *
 *    You may distribute under the terms of the GNU General Public License
 *    as specified in the README file that comes with the CVS 1.4 kit.
 *
 */


#include "cvs.h"

#ifndef lint
static char rcsid[] = "$CVSid: @(#)cvsrc.c 1.9 94/09/30 $";
USE(rcsid)
#endif /* lint */

/* this file is to be found in the user's home directory */

#ifndef	CVSRC_FILENAME
#define	CVSRC_FILENAME	".cvsrc"
#endif
char cvsrc[] = CVSRC_FILENAME;

#define	GROW	10

extern char *getenv ();
extern char *strtok ();

void
read_cvsrc (argc, argv)
     int *argc;
     char ***argv;
{
    char *homedir;
    char *homeinit;
    FILE *cvsrcfile;

    char linebuf [MAXLINELEN];
  
    char *optstart;

    int found = 0;

    int i;

    int new_argc;
    int max_new_argv;
    char **new_argv;

    /* don't do anything if argc is -1, since that implies "help" mode */
    if (*argc == -1)
	return;

    /* setup the new options list */

    new_argc = 1;
    max_new_argv = (*argc) + GROW;
    new_argv = (char **) xmalloc (max_new_argv * sizeof (char*));
    new_argv[0] = xstrdup ((*argv)[0]);

    /* determine filename for ~/.cvsrc */

    homedir = getenv ("HOME");
    if (!homedir)
	return;

    homeinit = (char *) xmalloc (strlen (homedir) + strlen (cvsrc) + 10);
    strcpy (homeinit, homedir);
    strcat (homeinit, "/");
    strcat (homeinit, cvsrc);

    /* if it can't be read, there's no point to continuing */

    if (access (homeinit, R_OK) != 0)
    {
	free (homeinit);
	return;
    }

    /* now scan the file until we find the line for the command in question */

    cvsrcfile = open_file (homeinit, "r");
    while (fgets (linebuf, MAXLINELEN, cvsrcfile))
    {
	/* skip over comment lines */
	if (linebuf[0] == '#')
	    continue;

	/* stop if we match the current command */
	if (!strncmp (linebuf, (*argv)[0], strlen ((*argv)[0])))
	{
	    found = 1;
	    break;
	}
    }

    fclose (cvsrcfile);

    if (found)
    {
	/* skip over command in the options line */
	optstart = strtok(linebuf+strlen((*argv)[0]), "\t \n");
      
	do
	{
	    new_argv [new_argc] = xstrdup (optstart);
	    new_argv [new_argc+1] = NULL;
	    new_argc += 1;
	  
	    if (new_argc >= max_new_argv)
	    {
		char **tmp_argv;
		max_new_argv += GROW;
		tmp_argv = (char **) xmalloc (max_new_argv * sizeof (char*));
		for (i = 0; i <= new_argc; i++)
		    tmp_argv[i] = new_argv[i];
		free(new_argv);
		new_argv = tmp_argv;
	    }
	  
	}
	while (optstart = strtok (NULL, "\t \n"));
    }

    /* now copy the remaining arguments */
  
    for (i=1; i < *argc; i++)
    {
	new_argv [new_argc] = (*argv)[i];
	new_argc += 1;
    }

    *argc = new_argc;
    *argv = new_argv;

    free (homeinit);
    return;
}
