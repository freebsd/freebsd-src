#ifndef lint
static const char *rcsid = "$Id: extract.c,v 1.5 1993/09/04 05:06:26 jkh Exp $";
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
 * This is the package extraction code for the add module.
 *
 */

#include "lib.h"
#include "add.h"

void
extract_plist(char *home, Package *pkg)
{
    PackingList p = pkg->head;
    char *last_file;

    /* Reset the world */
    Owner = NULL;
    Group = NULL;
    Mode = NULL;
    last_file = NULL;
    Directory = home;

    /* Do it */
    while (p) {
	switch(p->type) {
	case PLIST_NAME:
	    PkgName = p->name;
	    if (Verbose)
		printf("extract: Package name is %s\n", p->name);
	    break;

	case PLIST_FILE:
	    last_file = p->name;
	    if (Verbose)
		printf("extract: %s/%s\n", Directory, p->name);
	    if (!Fake) {
		char try[FILENAME_MAX];

		/* first try to rename it into place */
		sprintf(try, "%s/%s", Directory, p->name);
		if (rename(p->name, try) == FAIL)
		    copy_hierarchy(Directory, p->name, TRUE);
		apply_perms(Directory, p->name);
	    }
	    break;

	case PLIST_CWD:
	    if (Verbose)
		printf("extract: CWD to %s\n", p->name);
	    if (strcmp(p->name, ".")) {
		if (!Fake && make_hierarchy(p->name) == FAIL)
		    barf("Unable make directory '%s'.", p->name);
		Directory = p->name;
	    }
	    else
		Directory = home;
	    break;

	case PLIST_CMD:
	    if (Verbose)
		printf("extract: exec cmd '%s' (lastfile = %s)\n", p->name,
		       last_file);
	    if (!Fake && vsystem(p->name, last_file))
		whinge("Command '%s' failed.", p->name);
	    break;

	case PLIST_CHMOD:
	    Mode = p->name;
	    break;

	case PLIST_CHOWN:
	    Owner = p->name;
	    break;

	case PLIST_CHGRP:
	    Group = p->name;
	    break;

	case PLIST_COMMENT:
	    break;

	case PLIST_IGNORE:
	    p = p->next;
	    break;
	}
	p = p->next;
    }
}
