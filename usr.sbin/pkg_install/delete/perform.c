#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
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
    char **matched, **rb, **rbtmp;
    int errcode, i, j;
    int err_cnt = 0;
    struct reqr_by_entry *rb_entry;
    struct reqr_by_head *rb_list;

    if (MatchType != MATCH_EXACT) {
	matched = matchinstalled(MatchType, pkgs, &errcode);
	if (errcode != 0)
	    return 1;
	    /* Not reached */

	if (matched != NULL)
	    pkgs = matched;
	else switch (MatchType) {
	    case MATCH_GLOB:
		break;
	    case MATCH_ALL:
		warnx("no packages installed");
		return 0;
	    case MATCH_REGEX:
		warnx("no packages match pattern(s)");
		return 1;
	    default:
		break;
	}
    }

    err_cnt += sortdeps(pkgs);
    for (i = 0; pkgs[i]; i++) {
	if (Recursive == TRUE) {
	    errcode = requiredby(pkgs[i], &rb_list, FALSE, TRUE);
	    if (errcode < 0) {
		err_cnt++;
	    } else if (errcode > 0) {
		/*
		 * Copy values from the rb_list queue into argv-like NULL
		 * terminated list because requiredby() uses some static
		 * storage, while pkg_do() below will call this function,
		 * thus blowing our rb_list away.
		 */
		rbtmp = rb = alloca((errcode + 1) * sizeof(*rb));
		if (rb == NULL) {
		    warnx("%s(): alloca() failed", __FUNCTION__);
		    err_cnt++;
		    continue;
		}
		STAILQ_FOREACH(rb_entry, rb_list, link) {
		    *rbtmp = alloca(strlen(rb_entry->pkgname) + 1);
		    if (*rbtmp == NULL) {
			warnx("%s(): alloca() failed", __FUNCTION__);
			err_cnt++;
			continue;
		    }
		    strcpy(*rbtmp, rb_entry->pkgname);
		    rbtmp++;
		}
		*rbtmp = NULL;

		err_cnt += sortdeps(rb);
		for (j = 0; rb[j]; j++)
		    err_cnt += pkg_do(rb[j]);
	    }
	}
	err_cnt += pkg_do(pkgs[i]);
    }

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
    int len;
    /* support for separate pre/post install scripts */
    int new_m = 0;
    char pre_script[FILENAME_MAX] = DEINSTALL_FNAME;
    char post_script[FILENAME_MAX];
    char pre_arg[FILENAME_MAX], post_arg[FILENAME_MAX];
    struct reqr_by_entry *rb_entry;
    struct reqr_by_head *rb_list;

    if (!pkg || !(len = strlen(pkg)))
	return 1;
    if (pkg[len - 1] == '/')
	pkg[len - 1] = '\0';

    /* Reset some state */
    if (Plist.head)
	free_plist(&Plist);

    sprintf(LogDir, "%s/%s", LOG_DIR, pkg);

    if (!fexists(LogDir)) {
	warnx("no such package '%s' installed", pkg);
	return 1;
    }

    if (!getcwd(home, FILENAME_MAX)) {
	cleanup(0);
	errx(2, __FUNCTION__ ": unable to get current working directory!");
    }

    if (chdir(LogDir) == FAIL) {
	warnx("unable to change directory to %s! deinstall failed", LogDir);
	return 1;
    }

    if (Interactive == TRUE) {
	int first, ch;

	(void)fprintf(stderr, "delete %s? ", pkg);
	(void)fflush(stderr);
	first = ch = getchar();
	while (ch != '\n' && ch != EOF)
	    ch = getchar();
	if (first != 'y' && first != 'Y')
	    return 0;
	    /* Not reached */
    }

    if (requiredby(pkg, &rb_list, FALSE, TRUE) < 0)
	return 1;
    if (!STAILQ_EMPTY(rb_list)) {
	warnx("package '%s' is required by these other packages\n"
	      "and may not be deinstalled%s:",
	      pkg, Force ? " (but I'll delete it anyway)" : "");
	STAILQ_FOREACH(rb_entry, rb_list, link)
	    fprintf(stderr, "%s\n", rb_entry->pkgname);
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

    /*
     * Test whether to use the old method of passing tokens to deinstallation
     * scripts, and set appropriate variables..
     */

    if (fexists(POST_DEINSTALL_FNAME)) {
	new_m = 1;
	sprintf(post_script, "%s", POST_DEINSTALL_FNAME);
	pre_arg[0] = '\0';
	post_arg[0] = '\0';
    } else {
	if (fexists(DEINSTALL_FNAME)) {
	    sprintf(post_script, "%s", DEINSTALL_FNAME);
	    sprintf(pre_arg, "DEINSTALL");
	    sprintf(post_arg, "POST-DEINSTALL");
	}
    }

    if (!NoDeInstall && fexists(pre_script)) {
	if (Fake)
	    printf("Would execute de-install script at this point.\n");
	else {
	    vsystem("chmod +x %s", pre_script);	/* make sure */
	    if (vsystem("./%s %s %s", pre_script, pkg, pre_arg)) {
		warnx("deinstall script returned error status");
		if (!Force)
		    return 1;
	    }
	}
    }

    if (chdir(home) == FAIL) {
	cleanup(0);
	errx(2, __FUNCTION__ ": unable to return to working directory %s!", home);
    }

    /*
     * Some packages aren't packed right, so we need to just ignore
     * delete_package()'s status.  Ugh! :-(
     */
    if (delete_package(FALSE, CleanDirs, &Plist) == FAIL)
	warnx(
	"couldn't entirely delete package (perhaps the packing list is\n"
	"incorrectly specified?)");

    if (chdir(LogDir) == FAIL) {
 	warnx("unable to change directory to %s! deinstall failed", LogDir);
 	return 1;
    }

    if (!NoDeInstall && fexists(post_script)) {
 	if (Fake)
 	    printf("Would execute post-deinstall script at this point.\n");
 	else {
 	    vsystem("chmod +x %s", post_script);	/* make sure */
 	    if (vsystem("./%s %s %s", post_script, pkg, post_arg)) {
 		warnx("post-deinstall script returned error status");
 		if (!Force)
 		    return 1;
 	    }
 	}
    }

    if (chdir(home) == FAIL) {
 	cleanup(0);
	errx(2, __FUNCTION__ ": unable to return to working directory %s!", home);
    }

    if (!Fake) {
	if (vsystem("%s -r%c %s", REMOVE_CMD, Force ? 'f' : ' ', LogDir)) {
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
    if (!fexists(CONTENTS_FNAME)) {
	cleanup(0);
	errx(2, __FUNCTION__ ": installed package %s has no %s file!", pkg, CONTENTS_FNAME);
    }
}

void
cleanup(int sig)
{
    if (sig)
	exit(1);
}

static void
undepend(PackingList p, char *pkgname)
{
    char fname[FILENAME_MAX], ftmp[FILENAME_MAX];
    FILE *fpwr;
    int s;
    struct reqr_by_entry *rb_entry;
    struct reqr_by_head *rb_list;


    if (requiredby(p->name, &rb_list, Verbose, FALSE) <= 0)
	return;
    snprintf(fname, sizeof(fname), "%s/%s/%s", LOG_DIR, p->name,
	     REQUIRED_BY_FNAME);
    snprintf(ftmp, sizeof(ftmp), "%s.XXXXXX", fname);
    s = mkstemp(ftmp);
    if (s == -1) {
	warnx("couldn't open temp file '%s'", ftmp);
	return;
    }
    fpwr = fdopen(s, "w");
    if (fpwr == NULL) {
	close(s);
	warnx("couldn't fdopen temp file '%s'", ftmp);
	goto cleanexit;
    }
    STAILQ_FOREACH(rb_entry, rb_list, link)
	if (strcmp(rb_entry->pkgname, pkgname))		/* no match */
	    fputs(rb_entry->pkgname, fpwr), putc('\n', fpwr);
    if (fchmod(s, 0644) == FAIL) {
	warnx("error changing permission of temp file '%s'", ftmp);
	fclose(fpwr);
	goto cleanexit;
    }
    if (fclose(fpwr) == EOF) {
	warnx("error closing temp file '%s'", ftmp);
	goto cleanexit;
    }
    if (rename(ftmp, fname) == -1)
	warnx("error renaming '%s' to '%s'", ftmp, fname);
cleanexit:
    remove(ftmp);
    return;
}
