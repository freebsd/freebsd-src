#ifndef lint
static const char *rcsid = "$Id: perform.c,v 1.5 1993/09/04 05:06:33 jkh Exp $";
#endif

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * This is the main body of the create module.
 *
 */

#include "lib.h"
#include "create.h"

#include <signal.h>

static void sanity_check(void);
static void make_dist(char *, char *, char *, Package *);

int
pkg_perform(char **pkgs)
{
    char *pkg = *pkgs;		/* Only one arg to create */
    char *home, *cp;
    FILE *pkg_in, *fp;
    Package plist;
    char *suffix;  /* What we tack on to the end of the finished package */

    /* Preliminary setup */
    sanity_check();
    if (Verbose)
	printf("Creating package %s\n", pkg);
    get_dash_string(&Comment);
    get_dash_string(&Desc);
    if (!strcmp(Contents, "-"))
	pkg_in = stdin;
    else {
	pkg_in = fopen(Contents, "r");
	if (!pkg_in)
	    barf("Unable to open contents file '%s' for input.", Contents);
    }
    plist.head = plist.tail = NULL;

    /* Break the package name into base and desired suffix (if any) */
    if ((cp = index(pkg, '.')) != NULL) {
	suffix = cp + 1;
	*cp = '\0';
    }
    else
	suffix = "tgz";

    if (Prefix)
	add_plist(&plist, PLIST_CWD, Prefix);

    /* Slurp in the packing list */
    read_plist(&plist, pkg_in);

    /*
     * Run down the list and see if we've named it, if not stick in a name
     * at the top.
     */
    if (!in_plist(&plist, PLIST_NAME))
	add_plist_top(&plist, PLIST_NAME, pkg);

    /* Make a directory to stomp around in */
    home = make_playpen();
    signal(SIGINT, cleanup);
    signal(SIGHUP, cleanup);

    /* Make first "real contents" pass over it */
    check_list(home, &plist);
    copy_plist(home, &plist);
    mark_plist(&plist);

    /* Now put the release specific items in */
    add_plist(&plist, PLIST_CWD, ".");
    write_file(COMMENT_FNAME, Comment);
    add_plist(&plist, PLIST_IGNORE, NULL);
    add_plist(&plist, PLIST_FILE, COMMENT_FNAME);
    write_file(DESC_FNAME, Desc);
    add_plist(&plist, PLIST_IGNORE, NULL);
    add_plist(&plist, PLIST_FILE, DESC_FNAME);

    if (Install) {
	copy_file(home, Install, INSTALL_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, INSTALL_FNAME);
    }
    if (DeInstall) {
	copy_file(home, DeInstall, DEINSTALL_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, DEINSTALL_FNAME);
    }
    if (Require) {
	copy_file(home, Require, REQUIRE_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, REQUIRE_FNAME);
    }

    /* Run through the list again, picking up extra "local" items */
    check_list(".", &plist);
    copy_plist(".", &plist);
    mark_plist(&plist);

    /* Finally, write out the packing list */
    fp = fopen(CONTENTS_FNAME, "w");
    if (!fp)
	barf("Can't open file %s for writing.", CONTENTS_FNAME);
    write_plist(&plist, fp);
    if (fclose(fp))
	barf("Error while closing %s.", CONTENTS_FNAME);

    /* And stick it into a tar ball */
    make_dist(home, pkg, suffix, &plist);

    /* Cleanup */
    free(Comment);
    free(Desc);
    free_plist(&plist);
    cleanup(0);
    return TRUE;	/* Success */
}

static void
make_dist(char *home, char *pkg, char *suffix, Package *plist)
{
    char tball[FILENAME_MAX];
    char args[10];

    sprintf(tball, "%s/%s.%s", home, pkg, suffix);
    if (index(suffix, 'z'))	/* Compress/gzip? */
	strcpy(args, "z");
    if (Verbose)
	printf("Creating gzip'd tar ball in '%s', contents:\n", tball);
    strcat(args, "cf");
    if (vsystem("tar %s %s .", args, tball))
	barf("tar command failed!");
}

static void
sanity_check()
{
    if (!Comment)
	barf("Required package comment string is missing (-c comment).");
    if (!Desc)
	barf("Required package description string is missing (-d desc).");
    if (!Contents)
	barf("Required package contents list is missing (-f [-]file).");
}


/* Clean up those things that would otherwise hang around */
void
cleanup(int sig)
{
    leave_playpen();
}
