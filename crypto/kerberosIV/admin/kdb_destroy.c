/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Description.
 */

#include "adm_locl.h"

RCSID("$Id: kdb_destroy.c,v 1.7 1997/03/31 02:25:21 assar Exp $");

int
main(int argc, char **argv)
{
    char    answer[10];		/* user input */
    char    dbm[256];		/* database path and name */
    char    dbm1[256];		/* database path and name */
#ifdef HAVE_NEW_DB
    char   *file;               /* database file names */
#else
    char   *file1, *file2;	/* database file names */
#endif

    set_progname (argv[0]);

    strcpy(dbm, DBM_FILE);
#ifdef HAVE_NEW_DB
    file = strcat(dbm, ".db");
#else
    strcpy(dbm1, DBM_FILE);
    file1 = strcat(dbm, ".dir");
    file2 = strcat(dbm1, ".pag");
#endif

    printf("You are about to destroy the Kerberos database ");
    printf("on this machine.\n");
    printf("Are you sure you want to do this (y/n)? ");
    fgets(answer, sizeof(answer), stdin);

    if (answer[0] == 'y' || answer[0] == 'Y') {
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
