#ifndef lint
static const char *rcsid = "$Id: pl.c,v 1.4 1994/08/29 16:31:38 adam Exp $";
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
 * Routines for dealing with the packing list.
 *
 */

#include "lib.h"
#include "create.h"

/* Check a list for files that require preconversion */
void
check_list(char *home, Package *pkg)
{
    char cmd[FILENAME_MAX];
    char name[FILENAME_MAX];
    char *where = home;
    char *there = NULL;
    PackingList p = pkg->head;

    while (p) {
	if (p->type == PLIST_CWD)
	    where = p->name;
	else if (p->type == PLIST_IGNORE)
	    p = p->next;
	else if (p->type == PLIST_SRC) {
	    there = p->name;
	}
	else if (p->type == PLIST_FILE) {
	    cmd[0] = '\0';
	    sprintf(name, "%s/%s", there ? there : where, p->name);

	    if (*cmd) {
		if (Verbose)
		    printf("Uncompressing-> %s\n", cmd);
		if (system(cmd))
		    barf("%s failed!", cmd);
		nuke_suffix(p->name);
	    }
	}
	p = p->next;
    }
}   

/*
 * Copy unmarked files in packing list to playpen - marked files
 * have already been copied in an earlier pass through the list.
 */
void
copy_plist(char *home, Package *plist)
{
    PackingList p = plist->head;
    char *where = home;
    char *there = NULL;

    while (p) {
	if (p->type == PLIST_CWD)
	    where = p->name;
	else if (p->type == PLIST_SRC)
	    there = p->name;
	else if (p->type == PLIST_IGNORE)
	    p = p->next;
	else if (p->type == PLIST_FILE && !p->marked) {
	    char fn[FILENAME_MAX];

	    /* First, look for it in the "home" dir */
	    sprintf(fn, "%s/%s", home, p->name);
	    if (fexists(fn))
		copy_hierarchy(home, p->name, FALSE);
	    /*
	     * Otherwise, try along the actual extraction path..
	     */
	    else
		copy_hierarchy(there ? there : where, p->name, FALSE);
	}
	p = p->next;
    }
}
