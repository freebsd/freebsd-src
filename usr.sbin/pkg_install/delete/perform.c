#ifndef lint
static const char *rcsid = "$Id: perform.c,v 1.2 1993/09/04 05:06:39 jkh Exp $";
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
 * This is the main body of the delete module.
 *
 */

#include "lib.h"
#include "delete.h"

static int pkg_do(char *);
static void sanity_check(char *);
static char LogDir[FILENAME_MAX];


int
pkg_perform(char **pkgs)
{
    int i, err_cnt = 0;

    for (i = 0; pkgs[i]; i++)
	err_cnt += pkg_do(pkgs[i]);
    return err_cnt;
}

static Package Plist;

/* This is seriously ugly code following.  Written very fast! */
static int
pkg_do(char *pkg)
{
    FILE *cfile;
    char home[FILENAME_MAX];

    /* Reset some state */
    if (Plist.head)
	free_plist(&Plist);

    sprintf(LogDir, "%s/%s", LOG_DIR, pkg);
    if (!fexists(LogDir)) {
	whinge("No such package '%s' installed.", pkg);
	return 1;
    }
    if (!getcwd(home, FILENAME_MAX))
	barf("Unable to get current working directory!");
    if (chdir(LogDir) == FAIL) {
	whinge("Unable to change directory to %s!  Deinstall failed.", LogDir);
	return 1;
    }
    sanity_check(LogDir);
    if (fexists(REQUIRE_FNAME)) {
	if (Verbose)
	    printf("Executing 'require' script.\n");
	vsystem("chmod +x %s", REQUIRE_FNAME);	/* be sure */
	if (vsystem("%s %s DEINSTALL", REQUIRE_FNAME, pkg)) {
	    whinge("Package %s fails requirements - not deleted.", pkg);
	    return 1;
	}
    }
    cfile = fopen(CONTENTS_FNAME, "r");
    if (!cfile) {
	whinge("Unable to open '%s' file.", CONTENTS_FNAME);
	return 1;
    }
    /* If we have a prefix, add it now */
    if (Prefix)
	add_plist(&Plist, PLIST_CWD, Prefix);
    read_plist(&Plist, cfile);
    fclose(cfile);
    if (!NoDeInstall && fexists(DEINSTALL_FNAME)) {
	if (Fake)
	    printf("Would execute de-install script at this point.\n");
	else {
	    vsystem("chmod +x %s", DEINSTALL_FNAME);	/* make sure */
	    if (vsystem("%s %s DEINSTALL", DEINSTALL_FNAME, pkg)) {
		whinge("De-Install script returned error status.");
		return 1;
	    }
	}
    }
    if (chdir(home) == FAIL)
	barf("Toto!  This doesn't look like Kansas anymore!");
    delete_package(FALSE, &Plist);
    if (!Fake && vsystem("%s -r %s", REMOVE_CMD, LogDir)) {
	whinge("Couldn't remove log entry in %s, de-install failed.", LogDir);
	return 1;
    }
    return 0;
}

static void
sanity_check(char *pkg)
{
    if (!fexists(CONTENTS_FNAME))
	barf("Installed package %s has no %s file!", pkg, CONTENTS_FNAME);
}

void
cleanup(int sig)
{
    /* Nothing to do */
}
