#ifndef lint
static const char *rcsid = "$Id: futil.c,v 1.4 1993/09/04 05:06:27 jkh Exp $";
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
 * Miscellaneous file access utilities.
 *
 */

#include "lib.h"
#include "add.h"

/*
 * Assuming dir is a desired directory name, make it and all intervening
 * directories necessary.
 */

int
make_hierarchy(char *dir)
{
    char *cp1, *cp2;
    
    if (dir[0] == '/')
	cp1 = cp2 = dir + 1;
    else
	cp1 = cp2 = dir;
    while (cp2) {
	if ((cp2 = index(cp1, '/')) !=NULL )
	    *cp2 = '\0';
	if (fexists(dir)) {
	    if (!isdir(dir))
		return FAIL;
	}
	else {
	    if (vsystem("mkdir %s", dir))
		return FAIL;
	    apply_perms(NULL, dir);
	}
	/* Put it back */
	if (cp2) {
	    *cp2 = '/';
	    cp1 = cp2 + 1;
	}
    }
    return SUCCESS;
}

/* Using permission defaults, apply them as necessary */
void
apply_perms(char *dir, char *file)
{
    char fname[FILENAME_MAX];

    if (!dir || *file == '/')	/* absolute path? */
	strcpy(fname, file);
    else
	sprintf(fname, "%s/%s", dir, file);
    if (Mode)
	if (vsystem("chmod -R %s %s", Mode, fname))
	    whinge("Couldn't change mode of '%s' to '%s'.",
		   fname, Mode);
    if (Owner)
	if (vsystem("chown -R %s %s", Owner, fname))
	    whinge("Couldn't change owner of '%s' to '%s'.",
		   fname, Owner);
    if (Group)
	if (vsystem("chgrp -R %s %s", Group, fname))
	    whinge("Couldn't change group of '%s' to '%s'.",
		   fname, Group);
}

