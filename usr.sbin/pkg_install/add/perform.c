#ifndef lint
static const char *rcsid = "$Id: perform.c,v 1.8 1993/09/04 05:06:29 jkh Exp $";
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
 * This is the main body of the add module.
 *
 */

#include "lib.h"
#include "add.h"

#include <signal.h>

static int pkg_do(char *);
static char *find_name(Package *);
static int sanity_check(char *);
static char LogDir[FILENAME_MAX];


int
pkg_perform(char **pkgs)
{
    int i, err_cnt = 0;

    signal(SIGINT, cleanup);
    signal(SIGHUP, cleanup);

    if (AddMode == SLAVE)
	err_cnt = pkg_do(NULL);
    else {
	for (i = 0; pkgs[i]; i++)
	    err_cnt += pkg_do(pkgs[i]);
    }
    return err_cnt;
}

static Package Plist;

/* This is seriously ugly code following.  Written very fast! */
static int
pkg_do(char *pkg)
{
    char pkg_fullname[FILENAME_MAX];
    FILE *cfile;
    char *home;
    int code = 0;

    /* Reset some state */
    if (Plist.head)
	free_plist(&Plist);
    LogDir[0] = '\0';
    if (AddMode == SLAVE) {
	char tmp_dir[FILENAME_MAX];

	fgets(tmp_dir, FILENAME_MAX, stdin);
	tmp_dir[strlen(tmp_dir) - 1] = '\0'; /* pesky newline! */
	if (chdir(tmp_dir) == FAIL) {
	    whinge("pkg_add in SLAVE mode can't chdir to %s.", tmp_dir);
	    return 1;
	}
	read_plist(&Plist, stdin);
    }
    else {
	home = make_playpen(PlayPen);
	if (pkg[0] == '/')	/* full pathname? */
	    strcpy(pkg_fullname, pkg);
	else
	    sprintf(pkg_fullname, "%s/%s", home, pkg);
	if (!fexists(pkg_fullname)) {
	    whinge("Can't open package '%s'.", pkg_fullname);
	    return 1;
	}

	if (unpack(pkg_fullname, NULL))
	    return 1;

	if (sanity_check(pkg_fullname))
	    return 1;

	cfile = fopen(CONTENTS_FNAME, "r");
	if (!cfile) {
	    whinge("Unable to open %s file.", CONTENTS_FNAME);
	    goto fail;
	}
	read_plist(&Plist, cfile);
	fclose(cfile);
	if (Prefix) {
	    /*
	     * If we have a prefix, delete the first one we see and add this
	     * one in place of it.
	     */
	    delete_plist(&Plist, FALSE, PLIST_CWD, NULL);
	    add_plist_top(&Plist, PLIST_CWD, Prefix);
	}
	/* Just to be safe - overridden if package has made a choice */
	else
	    add_plist_top(&Plist, PLIST_CWD, home);
	/* If we're running in MASTER mode, just output the plist and return */
	if (AddMode == MASTER) {
	    printf("%s\n", where_playpen());
	    write_plist(&Plist, stdout);
	    return;
	}
    }
    PkgName = find_name(&Plist);
    if (fexists(REQUIRE_FNAME)) {
	vsystem("chmod +x %s", REQUIRE_FNAME);	/* be sure */
	if (Verbose)
	    printf("Running requirements file first for %s..\n", PkgName);
	if (!Fake && vsystem("%s %s INSTALL", REQUIRE_FNAME, PkgName)) {
	    whinge("Package %s fails requirements - not installed.",
		   pkg_fullname);
	    goto fail;
	}
    }
    if (!NoInstall && fexists(INSTALL_FNAME)) {
	vsystem("chmod +x %s", INSTALL_FNAME);	/* make sure */
	if (Verbose)
	    printf("Running install with PRE-INSTALL for %s..\n", PkgName);
	if (!Fake && vsystem("%s %s PRE-INSTALL", INSTALL_FNAME, PkgName)) {
	    whinge("Install script returned error status.");
	    goto fail;
	}
    }
    extract_plist(home, &Plist);
    if (!NoInstall && fexists(INSTALL_FNAME)) {
	if (Verbose)
	    printf("Running install with POST-INSTALL for %s..\n", PkgName);
	if (!Fake && vsystem("%s %s POST-INSTALL", INSTALL_FNAME, PkgName)) {
	    whinge("Install script returned error status.");
	    goto fail;
	}
    }
    if (!NoRecord && !Fake) {
	char contents[FILENAME_MAX];
	FILE *cfile;

	if (getuid() != 0)
	    whinge("Not running as root - trying to record install anyway.");
	if (!PkgName) {
	    whinge("No package name!  Can't record package, sorry.");
	    code = 1;
	    goto success;	/* well, partial anyway */
	}
	sprintf(LogDir, "%s/%s", LOG_DIR, PkgName);
	if (Verbose)
	    printf("Attempting to record package into %s..\n", LogDir);
	if (make_hierarchy(LogDir)) {
	    whinge("Can't record package into '%s', you're on your own!",
		   LogDir);
	    bzero(LogDir, FILENAME_MAX);
	    code = 1;
	    goto success;	/* close enough for government work */
	}
	if (fexists(DEINSTALL_FNAME))
	    copy_file(".", DEINSTALL_FNAME, LogDir);
	if (fexists(REQUIRE_FNAME))
	    copy_file(".", REQUIRE_FNAME, LogDir);
	sprintf(contents, "%s/%s", LogDir, CONTENTS_FNAME);
	cfile = fopen(contents, "w");
	if (!cfile) {
	    whinge("Can't open new contents file '%s'!  Can't register pkg.",
		   contents);
	    goto success; /* can't log, but still keep pkg */
	}
	write_plist(&Plist, cfile);
	fclose(cfile);
	copy_file(".", DESC_FNAME, LogDir);
	copy_file(".", COMMENT_FNAME, LogDir);
	if (Verbose)
	    printf("Package %s registered in %s\n", PkgName, LogDir);
    }
    goto success;

 fail:
    /* Nuke the whole (installed) show */
    if (!Fake)
	delete_package(FALSE, &Plist);

 success:
    /* delete the packing list contents */
    leave_playpen();

    return code;
}

static int
sanity_check(char *pkg)
{
    if (!fexists(CONTENTS_FNAME)) {
	whinge("Package %s has no CONTENTS file!", pkg);
	return 1;
    }
    if (!fexists(COMMENT_FNAME)) {
	whinge("Package %s has no COMMENT file!", pkg);
	return 1;
    }
    if (!fexists(DESC_FNAME)) {
	whinge("Package %s has no DESC file!", pkg);
	return 1;
    }
    return 0;
}

static char *
find_name(Package *pkg)
{
    PackingList p = pkg->head;

    while (p) {
	if (p->type == PLIST_NAME)
	    return p->name;
	p = p->next;
    }
    return "anonymous";
}

void
cleanup(int signo)
{
    if (signo)
	printf("Signal %d received, cleaning up..\n", signo);
    if (Plist.head) {
	if (!Fake)
	    delete_package(FALSE, &Plist);
	free_plist(&Plist);
    }
    if (!Fake && LogDir[0])
	vsystem("%s -rf %s", REMOVE_CMD, LogDir);
    leave_playpen();
}
