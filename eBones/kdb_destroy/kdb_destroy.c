/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: kdb_destroy.c,v 4.0 89/01/24 21:49:02 jtkohl Exp $
 *	$Id: kdb_destroy.c,v 1.5 1995/08/04 06:35:45 mark Exp $
 */

#ifndef	lint
static char rcsid[] =
"$Id: kdb_destroy.c,v 1.5 1995/08/04 06:35:45 mark Exp $";
#endif	lint

#include <strings.h>
#include <stdio.h>
#include "krb.h"
#include "krb_db.h"

#if defined(__FreeBSD__) || defined(__NetBSD__)
#define	DB
#endif


main()
{
    char    answer[10];		/* user input */
    char    dbm[256];		/* database path and name */
    char    dbm1[256];		/* database path and name */
#ifdef DB
    char   *file;               /* database file names */
#else
    char   *file1, *file2;	/* database file names */
#endif

    strcpy(dbm, DBM_FILE);
#ifdef DB
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
#ifdef DB
	if (unlink(file) == 0)
#else
	if (unlink(file1) == 0 && unlink(file2) == 0)
#endif
	    fprintf(stderr, "Database deleted at %s\n", DBM_FILE);
	else
	    fprintf(stderr, "Database cannot be deleted at %s\n",
		    DBM_FILE);
    } else
	fprintf(stderr, "Database not deleted.\n");
}
