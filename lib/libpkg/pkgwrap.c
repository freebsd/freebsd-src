/*
 * FreeBSD install - a package for the installation and maintenance
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
 * Maxim Sobolev
 * 8 September 2002
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "pkg.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern char **environ;

void
pkg_wrap(long curver, char **argv)
{
    FILE* f;
    char ver[9];			/* Format is: 'YYYYMMDD\0' */
    char buffer[FILENAME_MAX+10];	/* Format is: 'YYYYMMDD <path>' */
    char cmd[FILENAME_MAX+5];		/* Format is: '<path> -PPq' */
    char *path, *cp;
    long ptver, lpver;

    if (getenv("PKG_NOWRAP") != NULL)
	goto nowrap;

    setenv("PKG_NOWRAP", "1", 1);

    /* Get alternative location for package tools. */
    if ((f = fopen(PKG_WRAPCONF_FNAME, "r")) == NULL) {
	goto nowrap;
    } else {
	if (get_string(buffer, FILENAME_MAX+9, f) == NULL) {
	    goto nowrap;
	} else {
	    if ((path = strrchr(buffer, ' ')) == NULL) {
		goto nowrap;
	    } else {
		*path++ = '\0';
	    }
	}
    }

    if ((cp = strrchr(argv[0], '/')) == NULL) {
	cp = argv[0];
    } else {
	cp++;
    }

    /* Get version of the other pkg_install and libpkg */
    snprintf(cmd, FILENAME_MAX+10, "%s/%s -PPq", path, cp);
    if ((f = popen(cmd, "r")) == NULL) {
	perror("popen()");
	goto nowrap;
    } else {
	if (get_string(ver, 9, f) == NULL)
	    goto nowrap; 
	else
	    ptver = strtol(ver, NULL, 10);
	if (get_string(ver, 9, f) == NULL)
	    goto nowrap;
	else
	    lpver = strtol(ver, NULL, 10);
	pclose(f);
    }

    if ((lpver >= LIBPKG_VERSION) && (ptver > curver)) {
	snprintf(cmd, FILENAME_MAX, "%s/%s", path, cp);
	execve(cmd, argv, environ);
    }

nowrap:
    unsetenv("PKG_NOWRAP");
}
