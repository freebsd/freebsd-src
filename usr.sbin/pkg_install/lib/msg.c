#ifndef lint
static const char *rcsid = "$FreeBSD$";
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
 * Miscellaneous message routines.
 *
 */

#include "lib.h"

/* Die a relatively simple death */
void
upchuck(const char *err)
{
    fprintf(stderr, "Fatal error during execution: ");
    perror(err);
    cleanup(0);
    exit(1);
}

/* Die a more complex death */
void
barf(const char *err, ...)
{
    va_list args;

    va_start(args, err);
    vfprintf(stderr, err, args);
    fputc('\n', stderr);
    va_end(args);
    cleanup(0);
    exit(2);
}

/* Get annoyed about something but don't go to pieces over it */
void
whinge(const char *err, ...)
{
    va_list args;

    va_start(args, err);
    vfprintf(stderr, err, args);
    fputc('\n', stderr);
    va_end(args);
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
    tty = fopen("/dev/tty", "r");
    if (!tty)
	barf("Can't open /dev/tty!\n");
    while (ch != 'Y' && ch != 'N') {
	vfprintf(stderr, msg, args);
	if (def)
	    fprintf(stderr, " [yes]? ");
	else
	    fprintf(stderr, " [no]? ");
	fflush(stderr);
	if (AutoAnswer) {
	    ch = (AutoAnswer == YES) ? 'Y' : 'N';
	    fprintf(stderr, "%c\n", ch);
	}
	else
	    ch = toupper(fgetc(tty));
	if (ch == '\n')
	    ch = (def) ? 'Y' : 'N';
    }
    fclose(tty) ;
    return (ch == 'Y') ? TRUE : FALSE;
}
