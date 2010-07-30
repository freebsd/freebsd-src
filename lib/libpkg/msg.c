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
 * Miscellaneous message routines.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "pkg.h"
#include <err.h>
#include <paths.h>

/* Die a relatively simple death */
void
upchuck(const char *message)
{
    cleanup(0);
    errx(1, "fatal error during execution: %s", message);
}

/*
 * As a yes/no question, prompting from the varargs string and using
 * default if user just hits return.
 */
Boolean
y_or_n(Boolean def, const char *msg, ...)
{
    va_list args;
    int ch = 0;
    FILE *tty;

    va_start(args, msg);
    /*
     * Need to open /dev/tty because file collection may have been
     * collected on stdin
     */
    tty = fopen(_PATH_TTY, "r");
    if (!tty) {
	cleanup(0);
	errx(2, "can't open %s!", _PATH_TTY);
    }
    while (ch != 'Y' && ch != 'N') {
	vfprintf(stderr, msg, args);
	if (def)
	    fprintf(stderr, " [yes]? ");
	else
	    fprintf(stderr, " [no]? ");
	fflush(stderr);
	ch = toupper(fgetc(tty));
	if (ch == '\n')
	    ch = (def) ? 'Y' : 'N';
    }
    fclose(tty) ;
    return (ch == 'Y') ? TRUE : FALSE;
}
