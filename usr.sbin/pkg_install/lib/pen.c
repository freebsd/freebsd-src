#ifndef lint
static const char *rcsid = "$Id: pen.c,v 1.16 1995/08/26 10:15:18 jkh Exp $";
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
 * Routines for managing the "play pen".
 *
 */

#include "lib.h"
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/mount.h>

/* For keeping track of where we are */
static char Cwd[FILENAME_MAX];
static char Pen[FILENAME_MAX];
extern char *PlayPen;

/* Find a good place to play. */
static char *
find_play_pen(size_t sz)
{
    char *cp;
    struct stat sb;

    if ((cp = getenv("PKG_TMPDIR")) != NULL && stat(cp, &sb) != FAIL && (min_free(cp) >= sz))
	sprintf(Pen, "%s/instmp.XXXXXX", cp);
    else if ((cp = getenv("TMPDIR")) != NULL && stat(cp, &sb) != FAIL && (min_free(cp) >= sz))
	sprintf(Pen, "%s/instmp.XXXXXX", cp);
    else if (stat("/var/tmp", &sb) != FAIL && min_free("/var/tmp") >= sz)
	strcpy(Pen, "/var/tmp/instmp.XXXXXX");
    else if (stat("/tmp", &sb) != FAIL && min_free("/tmp") >= sz)
	strcpy(Pen, "/tmp/instmp.XXXXXX");
    else if ((stat("/usr/tmp", &sb) == SUCCESS | mkdir("/usr/tmp", 01777) == SUCCESS) && min_free("/usr/tmp") >= sz)
	strcpy(Pen, "/usr/tmp/instmp.XXXXXX");
    else barf("Can't find enough temporary space to extract the files, please set\nyour PKG_TMPDIR environment variable to a location with at least %d bytes\nfree.", sz);
    return Pen;
}

/*
 * Make a temporary directory to play in and chdir() to it, returning
 * pathname of previous working directory.
 */
char *
make_playpen(char *pen, size_t sz)
{
    if (!pen)
	PlayPen = find_play_pen(sz);
    else {
	strcpy(Pen, pen);
	PlayPen = Pen;
    }
    if (!getcwd(Cwd, FILENAME_MAX))
	upchuck("getcwd");
    if (!mktemp(Pen))
	barf("Can't mktemp '%s'.", Pen);
    if (mkdir(Pen, 0755) == FAIL)
	barf("Can't mkdir '%s'.", Pen);
    if (Verbose)
    {
	if (!sz)
	    fprintf(stderr, "Free temp space: %d bytes in %s\n", min_free(Pen), Pen);
	else
	    fprintf(stderr, "Projected package size: %d bytes,
free temp space: %d bytes in %s\n", (int)sz, min_free(Pen), Pen);
    }
    if (min_free(Pen) < sz) {
	rmdir(Pen);
	barf("Not enough free space to create `%s'.\nPlease set your PKG_TMPDIR
environment variable to a location with more space and\ntry the command
again.", Pen);
	PlayPen = NULL;
    }
    if (chdir(Pen) == FAIL)
	barf("Can't chdir to '%s'.", Pen);
    return Cwd;
}

/* Convenience routine for getting out of playpen */
void
leave_playpen(void)
{
    void (*oldsig)(int);

    /* Don't interrupt while we're cleaning up */
    oldsig = signal(SIGINT, SIG_IGN);
    if (Cwd[0]) {
	if (chdir(Cwd) == FAIL)
	    barf("Can't chdir back to '%s'.", Cwd);
	if (vsystem("rm -rf %s", Pen))
	    fprintf(stderr, "Couldn't remove temporary dir '%s'\n", Pen);
	Cwd[0] = '\0';
    }
    signal(SIGINT, oldsig);
}

/* Accessor function for telling us where the pen is */
char *
where_playpen(void)
{
    if (Cwd[0])
	return Pen;
    else
	return NULL;
}

size_t
min_free(char *tmpdir)
{
    struct statfs buf;

    if (!tmpdir)
	tmpdir = Pen;
    if (statfs(tmpdir, &buf) != 0) {
	perror("Error in statfs");
	return -1;
    }
    return buf.f_bavail * buf.f_bsize;
}
