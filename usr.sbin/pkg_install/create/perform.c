#ifndef lint
static const char rcsid[] =
	"$Id: perform.c,v 1.36 1997/07/04 04:48:02 jkh Exp $";
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
    char *suffix;  /* What we tack on to the end of the finished package */

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
	if (!pkg_in)
	    cleanup(0), errx(2, "unable to open contents file '%s' for input",
				Contents);
    }
    plist.head = plist.tail = NULL;

    /* Break the package name into base and desired suffix (if any) */
    if ((cp = rindex(pkg, '.')) != NULL) {
	suffix = cp + 1;
	*cp = '\0';
    }
    else
	suffix = "tgz";

    /* Stick the dependencies, if any, at the top */
    if (Pkgdeps) {
	if (Verbose && !PlistOnly)
	    printf("Registering depends:");
	while (Pkgdeps) {
	    cp = strsep(&Pkgdeps, " \t\n");
	    if (*cp) {
		add_plist(&plist, PLIST_PKGDEP, cp);
		if (Verbose && !PlistOnly)
		    printf(" %s", cp);
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

    /* Prefix should override the packing list */
    if (Prefix) {
	delete_plist(&plist, FALSE, PLIST_CWD, NULL);
	add_plist_top(&plist, PLIST_CWD, Prefix);
    }
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
    (void) umask(022);	/* make sure gen'ed directories, files don't have
			   group or other write bits. */
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
    if (!fp)
	cleanup(0), errx(2, "can't open file %s for writing", CONTENTS_FNAME);
    write_plist(&plist, fp);
    if (fclose(fp))
	cleanup(0), errx(2, "error while closing %s", CONTENTS_FNAME);

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
    PackingList p;
    int ret;
    char *args[50];	/* Much more than enough. */
    int nargs = 0;
    int pipefds[2];
    FILE *totar;
    pid_t pid;

    args[nargs++] = "tar";	/* argv[0] */

    if (*pkg == '/')
	snprintf(tball, FILENAME_MAX, "%s.%s", pkg, suffix);
    else
	snprintf(tball, FILENAME_MAX, "%s/%s.%s", home, pkg, suffix);

    args[nargs++] = "-c";
    args[nargs++] = "-f";
    args[nargs++] = tball;
    if (index(suffix, 'z'))	/* Compress/gzip? */
	args[nargs++] = "-z";
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
	printf("Creating gzip'd tar ball in '%s'\n", tball);

    /* Set up a pipe for passing the filenames, and fork off a tar process. */
    if (pipe(pipefds) == -1)
	cleanup(0), errx(2, "cannot create pipe");
    if ((pid = fork()) == -1)
	cleanup(0), errx(2, "cannot fork process for tar");
    if (pid == 0) {	/* The child */
	dup2(pipefds[0], 0);
	close(pipefds[0]);
	close(pipefds[1]);
	execv("/usr/bin/tar", args);
	cleanup(0);
	errx(2, "failed to execute tar command");
    }

    /* Meanwhile, back in the parent process ... */
    close(pipefds[0]);
    if ((totar = fdopen(pipefds[1], "w")) == NULL)
	cleanup(0), errx(2, "fdopen failed");

    fprintf(totar, "%s\n", CONTENTS_FNAME);
    fprintf(totar, "%s\n", COMMENT_FNAME);
    fprintf(totar, "%s\n", DESC_FNAME);

    if (Install)
	fprintf(totar, "%s\n", INSTALL_FNAME);
    if (DeInstall)
	fprintf(totar, "%s\n", DEINSTALL_FNAME);
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
    if (ret)
	cleanup(0), errx(2, "tar command failed with code %d", ret);
}

static void
sanity_check()
{
    if (!Comment)
	cleanup(0), errx(2,
		"required package comment string is missing (-c comment)");
    if (!Desc)
	cleanup(0), errx(2,
		"required package description string is missing (-d desc)");
    if (!Contents)
	cleanup(0), errx(2,
		"required package contents list is missing (-f [-]file)");
}


/* Clean up those things that would otherwise hang around */
void
cleanup(int sig)
{
    leave_playpen(home);
}
