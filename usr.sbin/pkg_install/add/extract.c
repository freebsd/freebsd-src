#ifndef lint
static const char *rcsid = "$Id: extract.c,v 1.7.4.1 1997/02/14 01:54:12 jkh Exp $";
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


#define STARTSTRING "tar cf -"
#define TOOBIG(str) ((strlen(str) + 6 + strlen(home) + where_count > maxargs) \
		|| (strlen(str) + 6 + strlen(home) + perm_count > maxargs))

#define PUSHOUT(todir) /* push out string */ \
	if (strlen(where_args) > sizeof(STARTSTRING)-1) { \
		    strcat(where_args, "|tar xf - -C "); \
		    strcat(where_args, todir); \
		    if (system(where_args)) \
			barf("can't invoke tar pipeline"); \
		    strcpy(where_args, STARTSTRING); \
		    where_count = sizeof(STARTSTRING)-1; \
	} \
	if (perm_count) { \
		    apply_perms(todir, perm_args); \
		    perm_args[0] = 0;\
		    perm_count = 0; \
	}

void
extract_plist(char *home, Package *pkg)
{
    PackingList p = pkg->head;
    char *last_file;
    char *where_args, *perm_args, *last_chdir;
    int maxargs, where_count = 0, perm_count = 0, add_count;

    maxargs = sysconf(_SC_ARG_MAX);
    maxargs -= 64;			/* some slop for the tar cmd text,
					   and sh -c */
    where_args = alloca(maxargs);
    if (!where_args)
	barf("can't get argument list space");
    perm_args = alloca(maxargs);
    if (!perm_args)
	barf("can't get argument list space");

    strcpy(where_args, STARTSTRING);
    where_count = sizeof(STARTSTRING)-1;
    perm_args[0] = 0;

    last_chdir = 0;

    /* Reset the world */
    Owner = NULL;
    Group = NULL;
    Mode = NULL;
    last_file = NULL;
    Directory = home;

    /* Do it */
    while (p) {
	char cmd[FILENAME_MAX];

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
		if (rename(p->name, try) == 0) {
		    /* try to add to list of perms to be changed,
		       and run in bulk. */
		    if (p->name[0] == '/' || TOOBIG(p->name))
			PUSHOUT(Directory);
		    add_count = snprintf(&perm_args[perm_count],
					 maxargs - perm_count,
					 "%s ", p->name);
		    if (add_count > maxargs - perm_count)
			barf("oops, miscounted strings!");
		    perm_count += add_count;
		}
		else {
		    /* rename failed, try copying with a big tar command */
		    if (p->name[0] == '/' || TOOBIG(p->name) ||
			last_chdir != Directory) {
			PUSHOUT(last_chdir);
			last_chdir = Directory;
		    }
		    add_count = snprintf(&where_args[where_count],
					 maxargs - where_count,
					 " %s", p->name);
		    if (add_count > maxargs - where_count)
			barf("oops, miscounted strings!");
		    where_count += add_count;
		    add_count = snprintf(&perm_args[perm_count],
					 maxargs - perm_count,
					 "%s ", p->name);
		    if (add_count > maxargs - perm_count)
			barf("oops, miscounted strings!");
		    perm_count += add_count;
		    if (p->name[0] == '/') {
			PUSHOUT(Directory);
		    }
		}
	    }
	    break;

	case PLIST_CWD:
	    if (Verbose)
		printf("extract: CWD to %s\n", p->name);
	    PUSHOUT(Directory);
	    if (strcmp(p->name, ".")) {
		if (!Fake && make_hierarchy(p->name) == FAIL)
		    barf("Unable make directory '%s'.", p->name);
		Directory = p->name;
	    }
	    else
		Directory = home;
	    break;

	case PLIST_CMD:
	    format_cmd(cmd, p->name, Directory, last_file);
	    PUSHOUT(Directory);
	    if (Verbose)
		printf("extract: execute '%s'\n", cmd);
	    if (!Fake && system(cmd))
		whinge("Command '%s' failed.", cmd);
	    break;

	case PLIST_CHMOD:
	    PUSHOUT(Directory);
	    Mode = p->name;
	    break;

	case PLIST_CHOWN:
	    PUSHOUT(Directory);
	    Owner = p->name;
	    break;

	case PLIST_CHGRP:
	    PUSHOUT(Directory);
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
    PUSHOUT(Directory);
}
