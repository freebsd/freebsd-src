#ifndef lint
static const char *rcsid = "$Id: pen.c,v 1.21 1996/06/08 00:46:33 alex Exp $";
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
static char Current[FILENAME_MAX];
static char Previous[FILENAME_MAX];

char *
where_playpen(void)
{
    return Current;
}

/* Find a good place to play. */
static char *
find_play_pen(char *pen, size_t sz)
{
    char *cp;
    struct stat sb;

    if (pen[0] && stat(pen, &sb) != FAIL && (min_free(pen) >= sz))
	return pen;
    else if ((cp = getenv("PKG_TMPDIR")) != NULL && stat(cp, &sb) != FAIL && (min_free(cp) >= sz))
	sprintf(pen, "%s/instmp.XXXXXX", cp);
    else if ((cp = getenv("TMPDIR")) != NULL && stat(cp, &sb) != FAIL && (min_free(cp) >= sz))
	sprintf(pen, "%s/instmp.XXXXXX", cp);
    else if (stat("/var/tmp", &sb) != FAIL && min_free("/var/tmp") >= sz)
	strcpy(pen, "/var/tmp/instmp.XXXXXX");
    else if (stat("/tmp", &sb) != FAIL && min_free("/tmp") >= sz)
	strcpy(pen, "/tmp/instmp.XXXXXX");
    else if ((stat("/usr/tmp", &sb) == SUCCESS || mkdir("/usr/tmp", 01777) == SUCCESS) && min_free("/usr/tmp") >= sz)
	strcpy(pen, "/usr/tmp/instmp.XXXXXX");
    else {
	barf("Can't find enough temporary space to extract the files, please set\n"
	     "your PKG_TMPDIR environment variable to a location with at least %d bytes\n"
	     "free.", sz);
	return NULL;
    }
    return pen;
}

/*
 * Make a temporary directory to play in and chdir() to it, returning
 * pathname of previous working directory.
 */
char *
make_playpen(char *pen, size_t sz)
{
    char *tmp;

    if (!find_play_pen(pen, sz))
	return NULL;

    if (!mktemp(pen)) {
	barf("Can't mktemp '%s'.", pen);
	return NULL;
    }
    if (mkdir(pen, 0755) == FAIL) {
	barf("Can't mkdir '%s'.", pen);
	return NULL;
    }
    if (Verbose) {
	if (sz)
	    fprintf(stderr, "Requested space: %d bytes, free space: %qd bytes in %s\n", (int)sz, min_free(pen), pen);
    }
    if (min_free(pen) < sz) {
	rmdir(pen);
	barf("Not enough free space to create: `%s'\n"
	     "Please set your PKG_TMPDIR environment variable to a location\n"
	     "with more space and\ntry the command again.", pen);
        return NULL;
    }
    if (Current[0])
	strcpy(Previous, Current);
    else if (!getcwd(Previous, FILENAME_MAX)) {
	upchuck("getcwd");
	return NULL;
    }
    if (chdir(pen) == FAIL)
	barf("Can't chdir to '%s'.", pen);
    strcpy(Current, pen);
    return Previous;
}

/* Convenience routine for getting out of playpen */
void
leave_playpen(char *save)
{
    void (*oldsig)(int);

    /* Don't interrupt while we're cleaning up */
    oldsig = signal(SIGINT, SIG_IGN);
    if (Previous[0] && chdir(Previous) == FAIL)
	barf("Can't chdir back to '%s'.", Previous);
    else if (Current[0] && strcmp(Current, Previous)) {
	if (vsystem("rm -rf %s", Current))
	    whinge("Couldn't remove temporary dir '%s'", Current);
	strcpy(Current, Previous);
    }
    if (save)
	strcpy(Previous, save);
    else
	Previous[0] = '\0';
    signal(SIGINT, oldsig);
}

off_t
min_free(char *tmpdir)
{
    struct statfs buf;

    if (statfs(tmpdir, &buf) != 0) {
	perror("Error in statfs");
	return -1;
    }
    return (off_t)buf.f_bavail * (off_t)buf.f_bsize;
}
