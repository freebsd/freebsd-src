#ifndef lint
static const char *rcsid = "$Id: pen.c,v 1.7 1994/10/14 05:56:15 jkh Exp $";
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
#include <sys/param.h>
#include <sys/mount.h>

/* For keeping track of where we are */
static char Cwd[FILENAME_MAX];
static char Pen[FILENAME_MAX];

static long min_free(char *);

/*
 * Make a temporary directory to play in and chdir() to it, returning
 * pathname of previous working directory.
 */
char *
make_playpen(char *pen, size_t sz)
{
    if (!pen) {
	char *cp;

	if ((cp = getenv("TMPDIR")) != NULL)
	    sprintf(Pen, "%s/instmp.XXXXXX", cp);
	else
	    strcpy(Pen, "/var/tmp/instmp.XXXXXX");
    }
    else
	strcpy(Pen, pen);
    if (!getcwd(Cwd, FILENAME_MAX))
	upchuck("getcwd");
    if (!mktemp(Pen))
	barf("Can't mktemp '%s'.", Pen);
    if (mkdir(Pen, 0755) == FAIL)
	barf("Can't mkdir '%s'.", Pen);
    if (Verbose) {
	if (!sz)
		fprintf(stderr, "Free temp space: %d bytes\n", min_free(Pen));
	else
		fprintf(stderr, "Projected package size: %d bytes, free temp space: %d bytes\n", (int)sz, min_free(Pen));
    }
    if (min_free(Pen) < sz) {
	rmdir(Pen);
	barf("%s doesn't have enough free space.  Please set your TMPDIR\nenvironment variable to a location with more space and\ntry the command again.", Pen);
    }
    if (chdir(Pen) == FAIL)
	barf("Can't chdir to '%s'.", Pen);
    return Cwd;
}

/* Convenience routine for getting out of playpen */
void
leave_playpen(void)
{
    if (Cwd[0]) {
	if (chdir(Cwd) == FAIL)
	    barf("Can't chdir back to '%s'.", Cwd);
	if (vsystem("rm -rf %s", Pen))
	    fprintf(stderr, "Couldn't remove temporary dir '%s'\n", Pen);
	Cwd[0] = '\0';
    }
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

static long min_free(char *tmpdir)
{
    struct statfs buf;

    if (statfs(tmpdir, &buf) != 0) {
	perror("Error in statfs");
	return -1;
    }
    return buf.f_bavail * buf.f_bsize;
}
