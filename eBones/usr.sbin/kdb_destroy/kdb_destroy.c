/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: kdb_destroy.c,v 4.0 89/01/24 21:49:02 jtkohl Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$FreeBSD$";
#endif	lint
#endif

#include <unistd.h>
#include <strings.h>
#include <stdio.h>
#include <krb.h>
#include <krb_db.h>

#if defined(__FreeBSD__) || defined(__NetBSD__)
#define	_DBM_
#endif

void
main()
{
    char    answer[10];		/* user input */
#ifdef _DBM_
    char    dbm[256];		/* database path and name */
    char   *file;               /* database file names */
#else
    char    dbm[256];		/* database path and name */
    char    dbm1[256];		/* database path and name */
    char   *file1, *file2;	/* database file names */
#endif

    strcpy(dbm, DBM_FILE);
#ifdef _DBM_
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
#ifdef _DBM_
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
