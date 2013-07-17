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
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * Miscellaneous system routines.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"
#include <err.h>

/*
 * Unusual system() substitute.  Accepts format string and args,
 * builds and executes command.  Returns exit code.
 */

int
vsystem(const char *fmt, ...)
{
    va_list args;
    char *cmd;
    long maxargs;
    int ret;

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
	va_end(args);
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

char *
vpipe(const char *fmt, ...)
{
   FILE *fp;
   char *cmd, *rp;
   long maxargs;
   va_list args;

    rp = malloc(MAXPATHLEN);
    if (!rp) {
	warnx("vpipe can't alloc buffer space");
	return NULL;
    }
    maxargs = sysconf(_SC_ARG_MAX);
    maxargs -= 32;			    /* some slop for the sh -c */
    cmd = alloca(maxargs);
    if (!cmd) {
	warnx("vpipe can't alloc arg space");
	return NULL;
    }

    va_start(args, fmt);
    if (vsnprintf(cmd, maxargs, fmt, args) > maxargs) {
	warnx("vsystem args are too long");
	va_end(args);
	return NULL;
    }
#ifdef DEBUG
    fprintf(stderr, "Executing %s\n", cmd);
#endif
    fflush(NULL);
    fp = popen(cmd, "r");
    if (fp == NULL) {
	warnx("popen() failed");
	va_end(args);
	return NULL;
    }
    get_string(rp, MAXPATHLEN, fp);
#ifdef DEBUG
    fprintf(stderr, "Returned %s\n", rp);
#endif
    va_end(args);
    if (pclose(fp) || (strlen(rp) == 0)) {
	free(rp);
	return NULL;
    }
    return rp;
}
