#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/pkg_install/lib/exec.c,v 1.7 1999/08/28 01:18:05 peter Exp $";
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
 * Miscellaneous system routines.
 *
 */

#include <err.h>
#include "lib.h"

/*
 * Unusual system() substitute.  Accepts format string and args,
 * builds and executes command.  Returns exit code.
 */

int
vsystem(const char *fmt, ...)
{
    va_list args;
    char *cmd;
    int ret, maxargs;

    maxargs = sysconf(_SC_ARG_MAX);
    maxargs -= 32;			/* some slop for the sh -c */
    cmd = malloc(maxargs);
    if (!cmd) {
	warnx("vsystem can't alloc arg space");
	return 1;
    }

    va_start(args, fmt);
    if (vsnprintf(cmd, maxargs, fmt, args) > maxargs) {
	warnx("vsystem args are too long");
	return 1;
    }
#ifdef DEBUG
printf("Executing %s\n", cmd);
#endif
    ret = system(cmd);
    va_end(args);
    free(cmd);
    return ret;
}

