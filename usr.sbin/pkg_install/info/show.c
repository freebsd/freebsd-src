#ifndef lint
static const char *rcsid = "$Id: show.c,v 1.3 1993/08/26 08:47:07 jkh Exp $";
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
 * 23 Aug 1993
 *
 * Various display routines for the info module.
 *
 */

#include "lib.h"
#include "info.h"

void
show_file(char *title, char *fname)
{
    FILE *fp;
    char line[1024];
    int n;

    printf(title);
    fp = fopen(fname, "r");
    if (!fp) {
	whinge("show_file: Can't open '%s' for reading.", fname);
	return;
    }
    while (n = fread(line, 1, 1024, fp))
	fwrite(line, 1, n, stdout);
    fclose(fp);
    printf("\n");	/* just in case */
}

/* Show a packing list item type.  If type is -1, show all */
void
show_plist(char *title, Package *plist, plist_t type)
{
    PackingList p;
    Boolean ign = FALSE;

    printf(title);
    p = plist->head;
    while (p) {
	if (p->type != type && type != -1) {
	    p = p->next;
	    continue;
	}
	switch(p->type) {
	case PLIST_FILE:
	    if (ign) {
		printf("File: %s (ignored)\n", p->name);
		ign = FALSE;
	    }
	    else
		printf("File: %s\n", p->name);
	    break;
	    
	case PLIST_CWD:
	    printf("\tCWD to %s\n", p->name);
	    break;

	case PLIST_CMD:
	    printf("\tEXEC '%s'\n", p->name);
	    break;

	case PLIST_CHMOD:
	    printf("\tCHMOD to %s\n", p->name ? p->name : "(no default)");
	    break;

	case PLIST_CHOWN:
	    printf("\tCHOWN to %s\n", p->name ? p->name : "(no default)");
	    break;

	case PLIST_CHGRP:
	    printf("\tCHGRP to %s\n", p->name ? p->name : "(no default)");
	    break;

	case PLIST_COMMENT:
	    printf("\tComment: %s\n", p->name);
	    break;

	case PLIST_IGNORE:
	    ign = TRUE;
	    break;

	case PLIST_NAME:
	    printf("\tPackage name: %s\n", p->name);
	    break;

	default:
	    barf("Unknown command type %d (%s)\n", p->type, p->name);
	    break;
	}
	p = p->next;
    }
}

