#ifndef lint
static const char rcsid[] =
	"$Id: perform.c,v 1.10.2.2 1997/10/09 07:09:28 charnier Exp $";
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

#include <err.h>
#include "lib.h"
#include "delete.h"

static int pkg_do(char *);
static void sanity_check(char *);
static void undepend(PackingList, char *);
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
    PackingList p;
    char *tmp;

    /* Reset some state */
    if (Plist.head)
	free_plist(&Plist);

    sprintf(LogDir, "%s/%s", (tmp = getenv(PKG_DBDIR)) ? tmp : DEF_LOG_DIR,
    	    pkg);
    if (!fexists(LogDir)) {
	warnx("no such package '%s' installed", pkg);
	return 1;
    }
    if (!getcwd(home, FILENAME_MAX))
	cleanup(0), errx(2, "unable to get current working directory!");
    if (chdir(LogDir) == FAIL) {
	warnx("unable to change directory to %s! deinstall failed", LogDir);
	return 1;
    }
    if (!isemptyfile(REQUIRED_BY_FNAME)) {
	char buf[512];
	warnx("package `%s' is required by these other packages\n"
		"and may not be deinstalled%s:",
		pkg, Force ? " (but I'll delete it anyway)" : "" );
	cfile = fopen(REQUIRED_BY_FNAME, "r");
	if (cfile) {
	    while (fgets(buf, sizeof(buf), cfile))
		fprintf(stderr, "%s", buf);
	    fclose(cfile);
	} else
	    warnx("cannot open requirements file `%s'", REQUIRED_BY_FNAME);
	if (!Force)
	    return 1;
    }
    sanity_check(LogDir);
    cfile = fopen(CONTENTS_FNAME, "r");
    if (!cfile) {
	warnx("unable to open '%s' file", CONTENTS_FNAME);
	return 1;
    }
    /* If we have a prefix, add it now */
    if (Prefix)
	add_plist(&Plist, PLIST_CWD, Prefix);
    read_plist(&Plist, cfile);
    fclose(cfile);
    p = find_plist(&Plist, PLIST_CWD);
    if (!p) {
	warnx("package '%s' doesn't have a prefix", pkg);
	return 1;
    }
    setenv(PKG_PREFIX_VNAME, p->name, 1);
    if (fexists(REQUIRE_FNAME)) {
	if (Verbose)
	    printf("Executing 'require' script.\n");
	vsystem("chmod +x %s", REQUIRE_FNAME);	/* be sure */
	if (vsystem("./%s %s DEINSTALL", REQUIRE_FNAME, pkg)) {
	    warnx("package %s fails requirements %s", pkg,
		   Force ? "" : "- not deleted");
	    if (!Force)
		return 1;
	}
    }
    if (!NoDeInstall && fexists(DEINSTALL_FNAME)) {
	if (Fake)
	    printf("Would execute de-install script at this point.\n");
	else {
	    vsystem("chmod +x %s", DEINSTALL_FNAME);	/* make sure */
	    if (vsystem("./%s %s DEINSTALL", DEINSTALL_FNAME, pkg)) {
		warnx("deinstall script returned error status");
		if (!Force)
		    return 1;
	    }
	}
    }
    if (chdir(home) == FAIL)
	cleanup(0), errx(2, "Toto! This doesn't look like Kansas anymore!");
    if (!Fake) {
	/* Some packages aren't packed right, so we need to just ignore delete_package()'s status.  Ugh! :-( */
	if (delete_package(FALSE, CleanDirs, &Plist) == FAIL)
	    warnx(
	"couldn't entirely delete package (perhaps the packing list is\n"
	"incorrectly specified?)");
	if (vsystem("%s -r %s", REMOVE_CMD, LogDir)) {
	    warnx("couldn't remove log entry in %s, deinstall failed", LogDir);
	    if (!Force)
		return 1;
	}
    }
    for (p = Plist.head; p ; p = p->next) {
	if (p->type != PLIST_PKGDEP)
	    continue;
	if (Verbose)
	    printf("Attempting to remove dependency on package `%s'\n", p->name);
	if (!Fake)
	    undepend(p, pkg);
    }
    return 0;
}

static void
sanity_check(char *pkg)
{
    if (!fexists(CONTENTS_FNAME))
	cleanup(0), errx(2, "installed package %s has no %s file!",
			pkg, CONTENTS_FNAME);
}

void
cleanup(int sig)
{
    /* Nothing to do */
    exit(1);
}

static void
undepend(PackingList p, char *pkgname)
{
     char fname[FILENAME_MAX], ftmp[FILENAME_MAX];
     char fbuf[FILENAME_MAX];
     FILE *fp, *fpwr;
     char *tmp;
     int s;

     sprintf(fname, "%s/%s/%s",
	     (tmp = getenv(PKG_DBDIR)) ? tmp : DEF_LOG_DIR,
	     p->name, REQUIRED_BY_FNAME);
     fp = fopen(fname, "r");
     if (fp == NULL) {
	 warnx("couldn't open dependency file `%s'", fname);
	 return;
     }
     sprintf(ftmp, "%s.XXXXXX", fname);
     s = mkstemp(ftmp);
     if (s == -1) {
	 fclose(fp);
	 warnx("couldn't open temp file `%s'", ftmp);
	 return;
     }
     fpwr = fdopen(s, "w");
     if (fpwr == NULL) {
	 close(s);
	 fclose(fp);
	 warnx("couldn't fdopen temp file `%s'", ftmp);
	 remove(ftmp);
	 return;
     }
     while (fgets(fbuf, sizeof(fbuf), fp) != NULL) {
	 if (fbuf[strlen(fbuf)-1] == '\n')
	     fbuf[strlen(fbuf)-1] = '\0';
	 if (strcmp(fbuf, pkgname))		/* no match */
	     fputs(fbuf, fpwr), putc('\n', fpwr);
     }
     (void) fclose(fp);
     if (fchmod(s, 0644) == FAIL) {
	 warnx("error changing permission of temp file `%s'", ftmp);
	 fclose(fpwr);
	 remove(ftmp);
	 return;
     }
     if (fclose(fpwr) == EOF) {
	 warnx("error closing temp file `%s'", ftmp);
	 remove(ftmp);
	 return;
     }
     if (rename(ftmp, fname) == -1)
	 warnx("error renaming `%s' to `%s'", ftmp, fname);
     remove(ftmp);			/* just in case */
     return;
}
