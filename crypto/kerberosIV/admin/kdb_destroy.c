/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Description.
 */

#include "adm_locl.h"

RCSID("$Id: kdb_destroy.c,v 1.9 1998/06/09 19:24:13 joda Exp $");

int
main(int argc, char **argv)
{
    char    answer[10];		/* user input */
#ifdef HAVE_NEW_DB
    char   *file;               /* database file names */
#else
    char   *file1, *file2;	/* database file names */
#endif

    set_progname (argv[0]);

#ifdef HAVE_NEW_DB
    asprintf(&file, "%s.db", DBM_FILE);
    if (file == NULL)
	err (1, "malloc");
#else
    asprintf(&file1, "%s.dir", DBM_FILE);
    asprintf(&file2, "%s.pag", DBM_FILE);
    if (file1 == NULL || file2 == NULL)
	err (1, "malloc");
#endif

    printf("You are about to destroy the Kerberos database ");
    printf("on this machine.\n");
    printf("Are you sure you want to do this (y/n)? ");
    if (fgets(answer, sizeof(answer), stdin) != NULL
	&& (answer[0] == 'y' || answer[0] == 'Y')) {
#ifdef HAVE_NEW_DB
      if (unlink(file) == 0)
#else
	if (unlink(file1) == 0 && unlink(file2) == 0)
#endif
	  {
	    warnx ("Database deleted at %s", DBM_FILE);
	    return 0;
	  }
	else
	    warn ("Database cannot be deleted at %s", DBM_FILE);
    } else
        warnx ("Database not deleted at %s", DBM_FILE);
    return 1;
}
