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
 * This is the main body of the create module.
 *
 */

#include "lib.h"
#include "create.h"

#include <err.h>
#include <libgen.h>
#include <signal.h>
#include <sys/syslimits.h>
#include <sys/wait.h>
#include <unistd.h>

static void sanity_check(void);
static void make_dist(char *, char *, char *, Package *);

static char *home;

int
pkg_perform(char **pkgs)
{
    char *pkg = *pkgs;		/* Only one arg to create */
    char *cp;
    FILE *pkg_in, *fp;
    Package plist;
    int len;
    char *suf;
    int compress = TRUE;	/* default is to compress packages */

    /* Preliminary setup */
    sanity_check();
    if (Verbose && !PlistOnly)
	printf("Creating package %s\n", pkg);
    get_dash_string(&Comment);
    get_dash_string(&Desc);
    if (!strcmp(Contents, "-"))
	pkg_in = stdin;
    else {
	pkg_in = fopen(Contents, "r");
	if (!pkg_in) {
	    cleanup(0);
	    errx(2, __FUNCTION__ ": unable to open contents file '%s' for input", Contents);
	}
    }
    plist.head = plist.tail = NULL;

    /* chop suffix off if already specified, remembering if we want to compress  */
    len = strlen(pkg);
    if (len > 4) {
	if (!strcmp(&pkg[len - 4], ".tgz")) {
	    compress = TRUE;
	    UseBzip2 = FALSE;
	    pkg[len - 4] = '\0';
	}
	else if (!strcmp(&pkg[len - 4], ".tar")) {
	    compress = FALSE;
	    UseBzip2 = FALSE;
	    pkg[len - 4] = '\0';
	}
	else if ((len > 5) && (!strcmp(&pkg[len - 5], ".tbz2"))) {
	    compress = FALSE;
	    UseBzip2 = TRUE;
	    pkg[len - 5] = '\0';
	}
    }
    if (UseBzip2)
	suf = "tbz2";
    else if (compress)
	suf = "tgz";
    else
	suf = "tar";

    /* Add the origin if asked, at the top */
    if (Origin)
	add_plist(&plist, PLIST_COMMENT, strconcat("ORIGIN:", Origin));

    /* Stick the dependencies, if any, at the top */
    if (Pkgdeps) {
	char **deps;
	int i;
	int ndeps = 0;

	if (Verbose && !PlistOnly)
	    printf("Registering depends:");

	/* Count number of dependencies */
	for (cp = Pkgdeps; cp != NULL && *cp != '\0';
			   cp = strpbrk(++cp, " \t\n")) {
	    ndeps++;
	}

	if (ndeps != 0) {
	    /* Create easy to use NULL-terminated list */
	    deps = alloca(sizeof(*deps) * ndeps + 1);
	    if (deps == NULL) {
		errx(2, "%s: alloca() failed", __FUNCTION__);
		/* Not reached */
	    }
	    for (i = 0; Pkgdeps; i++) {
		cp = strsep(&Pkgdeps, " \t\n");
		if (*cp)
		    deps[i] = cp;
	    }
	    deps[ndeps] = NULL;

	    sortdeps(deps);
	    for (i = 0; i < ndeps; i++) {
		add_plist_top(&plist, PLIST_PKGDEP, deps[i]);
		if (Verbose && !PlistOnly)
		    printf(" %s", deps[i]);
	    }
	}

	if (Verbose && !PlistOnly)
	    printf(".\n");
    }

    /* If a SrcDir override is set, add it now */
    if (SrcDir) {
	if (Verbose && !PlistOnly)
	    printf("Using SrcDir value of %s\n", SrcDir);
	add_plist(&plist, PLIST_SRC, SrcDir);
    }

    /* Slurp in the packing list */
    read_plist(&plist, pkg_in);

    /* Prefix should add an @cwd to the packing list */
    if (Prefix)
	add_plist_top(&plist, PLIST_CWD, Prefix);
    /*
     * Run down the list and see if we've named it, if not stick in a name
     * at the top.
     */
    if (find_plist(&plist, PLIST_NAME) == NULL)
	add_plist_top(&plist, PLIST_NAME, basename_of(pkg));

    /*
     * We're just here for to dump out a revised plist for the FreeBSD ports
     * hack.  It's not a real create in progress.
     */
    if (PlistOnly) {
	check_list(home, &plist);
	write_plist(&plist, stdout);
	exit(0);
    }

    /* Make a directory to stomp around in */
    home = make_playpen(PlayPen, 0);
    signal(SIGINT, cleanup);
    signal(SIGHUP, cleanup);

    /* Make first "real contents" pass over it */
    check_list(home, &plist);
    (void) umask(022);	/*
			 * Make sure gen'ed directories, files don't have
			 * group or other write bits.
			 */
    /* copy_plist(home, &plist); */
    /* mark_plist(&plist); */

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
    if (PostInstall) {
	copy_file(home, PostInstall, POST_INSTALL_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, POST_INSTALL_FNAME);
    }
    if (DeInstall) {
	copy_file(home, DeInstall, DEINSTALL_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, DEINSTALL_FNAME);
    }
    if (PostDeInstall) {
	copy_file(home, PostDeInstall, POST_DEINSTALL_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, POST_DEINSTALL_FNAME);
    }
    if (Require) {
	copy_file(home, Require, REQUIRE_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, REQUIRE_FNAME);
    }
    if (Display) {
	copy_file(home, Display, DISPLAY_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, DISPLAY_FNAME);
	add_plist(&plist, PLIST_DISPLAY, DISPLAY_FNAME);
    }
    if (Mtree) {
	copy_file(home, Mtree, MTREE_FNAME);
	add_plist(&plist, PLIST_IGNORE, NULL);
	add_plist(&plist, PLIST_FILE, MTREE_FNAME);
	add_plist(&plist, PLIST_MTREE, MTREE_FNAME);
    }

    /* Finally, write out the packing list */
    fp = fopen(CONTENTS_FNAME, "w");
    if (!fp) {
	cleanup(0);
	errx(2, __FUNCTION__ ": can't open file %s for writing", CONTENTS_FNAME);
    }
    write_plist(&plist, fp);
    if (fclose(fp)) {
	cleanup(0);
	errx(2, __FUNCTION__ ": error while closing %s", CONTENTS_FNAME);
    }

    /* And stick it into a tar ball */
    make_dist(home, pkg, suf, &plist);

    /* Cleanup */
    free(Comment);
    free(Desc);
    free_plist(&plist);
    leave_playpen();
    return TRUE;	/* Success */
}

static void
make_dist(char *home, char *pkg, char *suffix, Package *plist)
{
    char tball[FILENAME_MAX];
    PackingList p;
    int ret;
    char *args[50];	/* Much more than enough. */
    int nargs = 0;
    int pipefds[2];
    FILE *totar;
    pid_t pid;
    char *cname;

    args[nargs++] = "tar";	/* argv[0] */

    if (*pkg == '/')
	snprintf(tball, FILENAME_MAX, "%s.%s", pkg, suffix);
    else
	snprintf(tball, FILENAME_MAX, "%s/%s.%s", home, pkg, suffix);

    args[nargs++] = "-c";
    args[nargs++] = "-f";
    args[nargs++] = tball;
    if (strchr(suffix, 'z')) {	/* Compress/gzip/bzip2? */
	if (UseBzip2) {
	    args[nargs++] = "-y";
	    cname = "bzip'd ";
	}
	else {
	    args[nargs++] = "-z";
	    cname = "gzip'd ";
	}
    } else {
	cname = "";
    }
    if (Dereference)
	args[nargs++] = "-h";
    if (ExcludeFrom) {
	args[nargs++] = "-X";
	args[nargs++] = ExcludeFrom;
    }
    args[nargs++] = "-T";	/* Take filenames from file instead of args. */
    args[nargs++] = "-";	/* Use stdin for the file. */
    args[nargs] = NULL;

    if (Verbose)
	printf("Creating %star ball in '%s'\n", cname, tball);

    /* Set up a pipe for passing the filenames, and fork off a tar process. */
    if (pipe(pipefds) == -1) {
	cleanup(0);
	errx(2, __FUNCTION__ ": cannot create pipe");
    }
    if ((pid = fork()) == -1) {
	cleanup(0);
	errx(2, __FUNCTION__ ": cannot fork process for tar");
    }
    if (pid == 0) {	/* The child */
	dup2(pipefds[0], 0);
	close(pipefds[0]);
	close(pipefds[1]);
	execv("/usr/bin/tar", args);
	cleanup(0);
	errx(2, __FUNCTION__ ": failed to execute tar command");
    }

    /* Meanwhile, back in the parent process ... */
    close(pipefds[0]);
    if ((totar = fdopen(pipefds[1], "w")) == NULL) {
	cleanup(0);
	errx(2, __FUNCTION__ ": fdopen failed");
    }

    fprintf(totar, "%s\n", CONTENTS_FNAME);
    fprintf(totar, "%s\n", COMMENT_FNAME);
    fprintf(totar, "%s\n", DESC_FNAME);

    if (Install)
	fprintf(totar, "%s\n", INSTALL_FNAME);
    if (PostInstall)
	fprintf(totar, "%s\n", POST_INSTALL_FNAME);
    if (DeInstall)
	fprintf(totar, "%s\n", DEINSTALL_FNAME);
    if (PostDeInstall)
	fprintf(totar, "%s\n", POST_DEINSTALL_FNAME);
    if (Require)
	fprintf(totar, "%s\n", REQUIRE_FNAME);
    if (Display)
	fprintf(totar, "%s\n", DISPLAY_FNAME);
    if (Mtree)
	fprintf(totar, "%s\n", MTREE_FNAME);

    for (p = plist->head; p; p = p->next) {
	if (p->type == PLIST_FILE)
	    fprintf(totar, "%s\n", p->name);
	else if (p->type == PLIST_CWD || p->type == PLIST_SRC)
	    fprintf(totar, "-C\n%s\n", p->name);
	else if (p->type == PLIST_IGNORE)
	     p = p->next;
    }

    fclose(totar);
    wait(&ret);
    /* assume either signal or bad exit is enough for us */
    if (ret) {
	cleanup(0);
	errx(2, __FUNCTION__ ": tar command failed with code %d", ret);
    }
}

static void
sanity_check()
{
    if (!Comment) {
	cleanup(0);
	errx(2, __FUNCTION__ ": required package comment string is missing (-c comment)");
    }
    if (!Desc) {
	cleanup(0);
	errx(2,	__FUNCTION__ ": required package description string is missing (-d desc)");
    }
    if (!Contents) {
	cleanup(0);
	errx(2,	__FUNCTION__ ": required package contents list is missing (-f [-]file)");
    }
}


/* Clean up those things that would otherwise hang around */
void
cleanup(int sig)
{
    int in_cleanup = 0;

    if (!in_cleanup) {
	in_cleanup = 1;
    	leave_playpen();
    }
    if (sig)
	exit(1);
}
