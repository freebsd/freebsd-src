/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: kdb_destroy.c,v 4.0 89/01/24 21:49:02 jtkohl Exp $
 *	$Id: kdb_destroy.c,v 1.2 1994/07/19 19:23:49 g89r4222 Exp $
 */

#ifndef	lint
static char rcsid[] =
"$Id: kdb_destroy.c,v 1.2 1994/07/19 19:23:49 g89r4222 Exp $";
#endif	lint

#include <strings.h>
#include <stdio.h>
#include "krb.h"
#include "krb_db.h"

main()
{
    char    answer[10];		/* user input */
    char    dbm[256];		/* database path and name */
    char    dbm1[256];		/* database path and name */
    char   *file1, *file2;	/* database file names */

    strcpy(dbm, DBM_FILE);
    strcpy(dbm1, DBM_FILE);
    file1 = strcat(dbm, ".dir");
    file2 = strcat(dbm1, ".pag");

    printf("You are about to destroy the Kerberos database ");
    printf("on this machine.\n");
    printf("Are you sure you want to do this (y/n)? ");
    fgets(answer, sizeof(answer), stdin);

    if (answer[0] == 'y' || answer[0] == 'Y') {
	if (unlink(file1) == 0 && unlink(file2) == 0)
	    fprintf(stderr, "Database deleted at %s\n", DBM_FILE);
	else
	    fprintf(stderr, "Database cannot be deleted at %s\n",
		    DBM_FILE);
    } else
	fprintf(stderr, "Database not deleted.\n");
}
