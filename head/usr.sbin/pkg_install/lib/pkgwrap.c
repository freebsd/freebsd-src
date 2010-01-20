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

#include "lib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#undef main

#define SEPARATORS " \t"

extern char **environ;

int
main(int argc, char **argv)
{
    FILE *f;
    char buffer[FILENAME_MAX], *cp, *verstr;
    int len;

    if (getenv("PKG_NOWRAP") != NULL)
	goto nowrap;
    f = fopen(PKG_WRAPCONF_FNAME, "r");
    if (f == NULL)
	goto nowrap;
    cp = fgets(buffer, 256, f);
    fclose(f);
    if (cp == NULL)
	goto nowrap;
    len = strlen(cp);
    if (cp[len - 1] == '\n')
	cp[len - 1] = '\0';
    while (strchr(SEPARATORS, *cp) != NULL)
	cp++;
    verstr = cp;
    cp = strpbrk(cp, SEPARATORS);
    if (cp == NULL)
	goto nowrap;
    *cp = '\0';
    for (cp = verstr; *cp != '\0'; cp++)
	if (isdigit(*cp) == 0)
	    goto nowrap;
    if (atoi(verstr) < PKG_INSTALL_VERSION)
	goto nowrap;
    cp++;
    while (*cp != '\0' && strchr(SEPARATORS, *cp) != NULL)
	cp++;
    if (*cp == '\0')
	goto nowrap;
    bcopy(cp, buffer, strlen(cp) + 1);
    cp = strpbrk(buffer, SEPARATORS);
    if (cp != NULL)
	*cp = '\0';
    if (!isdir(buffer))
	goto nowrap;
    cp = strrchr(argv[0], '/');
    if (cp == NULL)
	cp = argv[0];
    else
	cp++;
    strlcat(buffer, "/", sizeof(buffer));
    strlcat(buffer, cp, sizeof(buffer));
    setenv("PKG_NOWRAP", "1", 1);
    execve(buffer, argv, environ);

nowrap:
    unsetenv("PKG_NOWRAP");
    return(real_main(argc, argv));
}
