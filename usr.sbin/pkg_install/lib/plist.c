#ifndef lint
static const char *rcsid = "$Id: plist.c,v 1.14 1995/07/28 01:50:35 ache Exp $";
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
 * General packing list routines.
 *
 */

#include "lib.h"

/* Add an item to a packing list */
void
add_plist(Package *p, plist_t type, char *arg)
{
    PackingList tmp;

    tmp = new_plist_entry();
    tmp->name = copy_string(arg);
    tmp->type = type;

    if (!p->head)
	p->head = p->tail = tmp;
    else {
	tmp->prev = p->tail;
	p->tail->next = tmp;
	p->tail = tmp;
    }
}

void
add_plist_top(Package *p, plist_t type, char *arg)
{
    PackingList tmp;

    tmp = new_plist_entry();
    tmp->name = copy_string(arg);
    tmp->type = type;

    if (!p->head)
	p->head = p->tail = tmp;
    else {
	tmp->next = p->head;
	p->head->prev = tmp;
	p->head = tmp;
    }
}

/* Return the last (most recent) entry in a packing list */
PackingList
last_plist(Package *p)
{
    return p->tail;
}

/* Mark all items in a packing list to prevent iteration over them */
void
mark_plist(Package *pkg)
{
    PackingList p = pkg->head;

    while (p) {
	p->marked = TRUE;
	p = p->next;
    }
}

/* Find a given item in a packing list and, if so, return it (else NULL) */
PackingList
find_plist(Package *pkg, plist_t type)
{
    PackingList p = pkg->head;

    while (p) {
	if (p->type == type)
	    return p;
	p = p->next;
    }
    return NULL;
}

/* Look for a specific boolean option argument in the list */
char *
find_plist_option(Package *pkg, char *name)
{
    PackingList p = pkg->head;

    while (p) {
	if (p->type == PLIST_OPTION && !strcmp(p->name, name))
	    return p->name;
	p = p->next;
    }
    return NULL;
}

/*
 * Delete plist item 'type' in the list (if 'name' is non-null, match it
 * too.)  If 'all' is set, delete all items, not just the first occurance.
 */
void
delete_plist(Package *pkg, Boolean all, plist_t type, char *name)
{
    PackingList p = pkg->head;

    while (p) {
	PackingList pnext = p->next;

	if (p->type == type && (!name || !strcmp(name, p->name))) {
	    free(p->name);
	    if (p->prev)
		p->prev->next = pnext;
	    else
		pkg->head = pnext;
	    if (pnext)
		pnext->prev = p->prev;
	    else
		pkg->tail = p->prev;
	    free(p);
	    if (!all)
		return;
	    p = pnext;
	}
	else
	    p = p->next;
    }
}

/* Allocate a new packing list entry */
PackingList
new_plist_entry(void)
{
    PackingList ret;

    ret = (PackingList)malloc(sizeof(struct _plist));
    bzero(ret, sizeof(struct _plist));
    return ret;
}

/* Free an entire packing list */
void
free_plist(Package *pkg)
{
    PackingList p = pkg->head;

    while (p) {
	PackingList p1 = p->next;

	free(p->name);
	free(p);
	p = p1;
    }
    pkg->head = pkg->tail = NULL;
}

/*
 * For an ascii string denoting a plist command, return its code and
 * optionally its argument(s)
 */
int
plist_cmd(char *s, char **arg)
{
    char cmd[FILENAME_MAX + 20];	/* 20 == fudge for max cmd len */
    char *cp, *sp;

    strcpy(cmd, s);
    str_lowercase(cmd);
    cp = cmd;
    sp = s;
    while (*cp) {
	if (isspace(*cp)) {
	    *cp = '\0';
	    while (isspace(*sp)) /* Never sure if macro, increment later */
		++sp;
	    break;
	}
	++cp, ++sp;
    }
    if (arg)
	*arg = sp;
    if (!strcmp(cmd, "cwd"))
	return PLIST_CWD;
    else if (!strcmp(cmd, "srcdir"))
	return PLIST_SRC;
    else if (!strcmp(cmd, "cd"))
	return PLIST_CWD;
    else if (!strcmp(cmd, "exec"))
	return PLIST_CMD;
    else if (!strcmp(cmd, "unexec"))
	return PLIST_UNEXEC;
    else if (!strcmp(cmd, "mode"))
	return PLIST_CHMOD;
    else if (!strcmp(cmd, "owner"))
	return PLIST_CHOWN;
    else if (!strcmp(cmd, "group"))
	return PLIST_CHGRP;
    else if (!strcmp(cmd, "comment"))
	return PLIST_COMMENT;
    else if (!strcmp(cmd, "ignore"))
	return PLIST_IGNORE;
    else if (!strcmp(cmd, "ignore_inst"))
	return PLIST_IGNORE_INST;
    else if (!strcmp(cmd, "name"))
	return PLIST_NAME;
    else if (!strcmp(cmd, "display"))
	return PLIST_DISPLAY;
    else if (!strcmp(cmd, "pkgdep"))
	return PLIST_PKGDEP;
    else if (!strcmp(cmd, "mtree"))
	return PLIST_MTREE;
    else if (!strcmp(cmd, "dirrm"))
	return PLIST_DIR_RM;
    else if (!strcmp(cmd, "option"))
	return PLIST_OPTION;
    else
	return FAIL;
}

/* Read a packing list from a file */
void
read_plist(Package *pkg, FILE *fp)
{
    char *cp, pline[FILENAME_MAX];
    int cmd;

    while (fgets(pline, FILENAME_MAX, fp)) {
	int len = strlen(pline) - 1;

	while (isspace(pline[len]))
	    pline[len--] = '\0';
	if (len <= 0)
	    continue;
	cp = pline;
	if (pline[0] == CMD_CHAR) {
	    cmd = plist_cmd(pline + 1, &cp);
	    if (cmd == FAIL)
		barf("Bad command '%s'", pline);
	    if (*cp == '\0')
		cp = NULL;
	}
	else
	    cmd = PLIST_FILE;
	add_plist(pkg, cmd, cp);
    }
}

/* Write a packing list to a file, converting commands to ascii equivs */
void
write_plist(Package *pkg, FILE *fp)
{
    PackingList plist = pkg->head;

    while (plist) {
	switch(plist->type) {
	case PLIST_FILE:
	    fprintf(fp, "%s\n", plist->name);
	    break;

	case PLIST_CWD:
	    fprintf(fp, "%ccwd %s\n", CMD_CHAR, plist->name);
	    break;

	case PLIST_SRC:
	    fprintf(fp, "%csrcdir %s\n", CMD_CHAR, plist->name);
	    break;

	case PLIST_CMD:
	    fprintf(fp, "%cexec %s\n", CMD_CHAR, plist->name);
	    break;

	case PLIST_UNEXEC:
	    fprintf(fp, "%cunexec %s\n", CMD_CHAR, plist->name);
	    break;

	case PLIST_CHMOD:
	    fprintf(fp, "%cmode %s\n", CMD_CHAR, plist->name ? plist->name : "");
	    break;

	case PLIST_CHOWN:
	    fprintf(fp, "%cowner %s\n", CMD_CHAR, plist->name ? plist->name : "");
	    break;

	case PLIST_CHGRP:
	    fprintf(fp, "%cgroup %s\n", CMD_CHAR, plist->name ? plist->name : "");
	    break;

	case PLIST_COMMENT:
	    fprintf(fp, "%ccomment %s\n", CMD_CHAR, plist->name);
	    break;

	case PLIST_IGNORE:
	case PLIST_IGNORE_INST:		/* a one-time non-ignored file */
	    fprintf(fp, "%cignore\n", CMD_CHAR);
	    break;

	case PLIST_NAME:
	    fprintf(fp, "%cname %s\n", CMD_CHAR, plist->name);
	    break;

	case PLIST_DISPLAY:
	    fprintf(fp, "%cdisplay %s\n", CMD_CHAR, plist->name);
	    break;

	case PLIST_PKGDEP:
	    fprintf(fp, "%cpkgdep %s\n", CMD_CHAR, plist->name);
	    break;

	case PLIST_MTREE:
	    fprintf(fp, "%cmtree %s\n", CMD_CHAR, plist->name);
	    break;

	case PLIST_DIR_RM:
	    fprintf(fp, "%cdirrm %s\n", CMD_CHAR, plist->name);
	    break;

	case PLIST_OPTION:
	    fprintf(fp, "%coption %s\n", CMD_CHAR, plist->name);
	    break;

	default:
	    barf("Unknown command type %d (%s)\n", plist->type, plist->name);
	    break;
	}
	plist = plist->next;
    }
}

/*
 * Delete the results of a package installation.
 *
 * This is here rather than in the pkg_delete code because pkg_add needs to
 * run it too in cases of failure.
 */
int
delete_package(Boolean ign_err, Boolean nukedirs, Package *pkg)
{
    PackingList p = pkg->head;
    char *Where = ".", *last_file = "";
    Boolean fail = SUCCESS;

    if (!p)
	return FAIL;
    while (p) {
	if (p->type == PLIST_CWD) {
	    Where = p->name;
	    if (Verbose)
		printf("Change working directory to %s\n", Where);
	}
	else if (p->type == PLIST_UNEXEC) {
	    char cmd[FILENAME_MAX];

	    format_cmd(cmd, p->name, Where, last_file);
	    if (Verbose)
		printf("Execute `%s'\n", cmd);
	    if (!Fake && system(cmd)) {
		whinge("unexec command for `%s' failed.", cmd);
		fail = FAIL;
	    }
	}
	else if (p->type == PLIST_IGNORE)
	    p = p->next;
	else if (p->type == PLIST_FILE || p->type == PLIST_DIR_RM) {
	    char full_name[FILENAME_MAX];

	    sprintf(full_name, "%s/%s", Where, p->name);
	    if (isdir(full_name) && p->type == PLIST_FILE) {
		warn("Attempting to delete directory `%s' as a file\n"
		     "This packing list is incorrect - ignoring delete request.\n", full_name);
	    }
	    else {
		if (Verbose)
		    printf("Delete %s %s\n", !isdir(full_name) ? "file" : " directory", full_name);

		if (!Fake && delete_hierarchy(full_name, ign_err, p->type == PLIST_DIR_RM ? FALSE : nukedirs)) {
		    whinge("Unable to completely remove file '%s'", full_name);
		    fail = FAIL;
		}
	    }
	    last_file = p->name;
	}
	p = p->next;
    }
    return fail;
}

#ifdef DEBUG
#define RMDIR(dir) vsystem("%s %s", RMDIR_CMD, dir)
#define REMOVE(dir,ie) vsystem("%s %s%s", REMOVE_CMD, (ie ? "-f " : ""), dir)
#else
#define RMDIR rmdir
#define	REMOVE(file,ie) (remove(file) && !(ie))
#endif

/* Selectively delete a hierarchy */
int
delete_hierarchy(char *dir, Boolean ign_err, Boolean nukedirs)
{
    char *cp1, *cp2;

    cp1 = cp2 = dir;
    if (!fexists(dir)) {
	if (!ign_err)
	    whinge("%s `%s' doesn't really exist.", isdir(dir) ? "Directory" : "File", dir);
    } else if (nukedirs) {
	if (vsystem("%s -r%s %s", REMOVE_CMD, (ign_err ? "f" : ""), dir))
	    return 1;
    } else if (isdir(dir)) {
	if (RMDIR(dir) && !ign_err)
	    return 1;
    } else {
	if (REMOVE(dir, ign_err))
	    return 1;
    }

    if (!nukedirs)
	return 0;
    while (cp2) {
	if ((cp2 = rindex(cp1, '/')) != NULL)
	    *cp2 = '\0';
	if (!isemptydir(dir))
	    return 0;
	if (RMDIR(dir) && !ign_err)
	    if (!fexists(dir))
		whinge("Directory `%s' doesn't really exist.", dir);
	    else
		return 1;
	/* back up the pathname one component */
	if (cp2) {
	    cp1 = dir;
	}
    }
    return 0;
}
