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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <ctype.h>
#include <err.h>
#include "lib.h"
#include "add.h"


#define STARTSTRING "/usr/bin/tar cf -"
#define TOOBIG(str) \
    (((int)strlen(str) + FILENAME_MAX + where_count > maxargs) ||\
	((int)strlen(str) + FILENAME_MAX + perm_count > maxargs))

#define PUSHOUT(todir) /* push out string */ \
    if (where_count > (int)sizeof(STARTSTRING)-1) { \
	strcat(where_args, "|/usr/bin/tar --unlink -xpf - -C "); \
	strcat(where_args, todir); \
	if (system(where_args)) { \
	    cleanup(0); \
	    errx(2, "%s: can not invoke %ld byte tar pipeline: %s", \
		 __func__, (long)strlen(where_args), where_args); \
	} \
	strcpy(where_args, STARTSTRING); \
	where_count = sizeof(STARTSTRING)-1; \
    } \
    if (perm_count) { \
	apply_perms(todir, perm_args); \
	perm_args[0] = 0;\
	perm_count = 0; \
    }

static void
rollback(const char *name, const char *home, PackingList start, PackingList stop)
{
    PackingList q;
    char try[FILENAME_MAX], bup[FILENAME_MAX];
    const char *dir;

    dir = home;
    for (q = start; q != stop; q = q->next) {
	if (q->type == PLIST_FILE) {
	    snprintf(try, FILENAME_MAX, "%s/%s", dir, q->name);
	    if (make_preserve_name(bup, FILENAME_MAX, name, try) && fexists(bup)) {
		(void)chflags(try, 0);
		(void)unlink(try);
		if (rename(bup, try))
		    warnx("rollback: unable to rename %s back to %s", bup, try);
	    }
	}
	else if (q->type == PLIST_CWD) {
	    if (strcmp(q->name, "."))
		dir = q->name;
	    else
		dir = home;
	}
    }
}

#define add_char(buf, len, pos, ch) do {\
    if ((pos) < (len)) { \
        buf[(pos)] = (ch); \
        buf[(pos) + 1] = '\0'; \
    } \
    ++(pos); \
} while (0)

static int
add_arg(char *buf, int len, const char *str)
{
    int i = 0;

    add_char(buf, len, i, ' ');
    for (; *str != '\0'; ++str) {
	if (!isalnum(*str) && *str != '/' && *str != '.' && *str != '-')
	    add_char(buf, len, i, '\\');
	add_char(buf, len, i, *str);
    }
    return (i);
}

void
extract_plist(const char *home, Package *pkg)
{
    PackingList p = pkg->head;
    char *last_file;
    char *where_args, *perm_args, *last_chdir;
    int maxargs, where_count = 0, perm_count = 0, add_count;
    Boolean preserve;

    maxargs = sysconf(_SC_ARG_MAX) / 2;	/* Just use half the argument space */
    where_args = alloca(maxargs);
    if (!where_args) {
	cleanup(0);
	errx(2, "%s: can't get argument list space", __func__);
    }
    perm_args = alloca(maxargs);
    if (!perm_args) {
	cleanup(0);
	errx(2, "%s: can't get argument list space", __func__);
    }

    strcpy(where_args, STARTSTRING);
    where_count = sizeof(STARTSTRING)-1;
    perm_args[0] = 0;

    last_chdir = 0;
    preserve = find_plist_option(pkg, "preserve") ? TRUE : FALSE;

    /* Reset the world */
    Owner = NULL;
    Group = NULL;
    Mode = NULL;
    last_file = NULL;
    Directory = (char *)home;

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
		snprintf(try, FILENAME_MAX, "%s/%s", Directory, p->name);
		if (fexists(try)) {
		    (void)chflags(try, 0);	/* XXX hack - if truly immutable, rename fails */
		    if (preserve && PkgName) {
			char pf[FILENAME_MAX];

			if (make_preserve_name(pf, FILENAME_MAX, PkgName, try)) {
			    if (rename(try, pf)) {
				warnx(
				"unable to back up %s to %s, aborting pkg_add",
				try, pf);
				rollback(PkgName, home, pkg->head, p);
				return;
			    }
			}
		    }
		}
		if (rename(p->name, try) == 0) {
		    /* try to add to list of perms to be changed and run in bulk. */
		    if (p->name[0] == '/' || TOOBIG(p->name)) {
			PUSHOUT(Directory);
		    }
		    add_count = add_arg(&perm_args[perm_count], maxargs - perm_count, p->name);
		    if (add_count < 0 || add_count >= maxargs - perm_count) {
			cleanup(0);
			errx(2, "%s: oops, miscounted strings!", __func__);
		    }
		    perm_count += add_count;
		}
		else {
		    /* rename failed, try copying with a big tar command */
		    if (last_chdir != Directory) {
			if (last_chdir == NULL) {
			    PUSHOUT(Directory);
			} else {
			    PUSHOUT(last_chdir);
			}
			last_chdir = Directory;
		    }
		    else if (p->name[0] == '/' || TOOBIG(p->name)) {
			PUSHOUT(Directory);
		    }
		    add_count = add_arg(&where_args[where_count], maxargs - where_count, p->name);
		    if (add_count < 0 || add_count >= maxargs - where_count) {
			cleanup(0);
			errx(2, "%s: oops, miscounted strings!", __func__);
		    }
		    where_count += add_count;
		    add_count = add_arg(&perm_args[perm_count], maxargs - perm_count, p->name);
		    if (add_count < 0 || add_count >= maxargs - perm_count) {
			cleanup(0);
			errx(2, "%s: oops, miscounted strings!", __func__);
		    }
		    perm_count += add_count;
		}
	    }
	    break;

	case PLIST_CWD:
	    if (Verbose)
		printf("extract: CWD to %s\n", p->name);
	    PUSHOUT(Directory);
	    if (strcmp(p->name, ".")) {
		if (!Fake && make_hierarchy(p->name) == FAIL) {
		    cleanup(0);
		    errx(2, "%s: unable to cwd to '%s'", __func__, p->name);
		}
		Directory = p->name;
	    }
	    else
		Directory = (char *)home;
	    break;

	case PLIST_CMD:
	    if ((strstr(p->name, "%B") || strstr(p->name, "%F") ||
		 strstr(p->name, "%f")) && last_file == NULL) {
		cleanup(0);
		errx(2, "%s: no last file specified for '%s' command",
		    __func__, p->name);
	    }
	    if (strstr(p->name, "%D") && Directory == NULL) {
		cleanup(0);
		errx(2, "%s: no directory specified for '%s' command",
		    __func__, p->name);
	    }
	    format_cmd(cmd, FILENAME_MAX, p->name, Directory, last_file);
	    PUSHOUT(Directory);
	    if (Verbose)
		printf("extract: execute '%s'\n", cmd);
	    if (!Fake && system(cmd))
		warnx("command '%s' failed", cmd);
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

	default:
	    break;
	}
	p = p->next;
    }
    PUSHOUT(Directory);
}
