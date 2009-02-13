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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"
#include <err.h>
#include <md5.h>

/* Add an item to a packing list */
void
add_plist(Package *p, plist_t type, const char *arg)
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
    switch (type) {
    case PLIST_NAME:
	p->name = tmp->name;
	break;

    case PLIST_ORIGIN:
	p->origin = tmp->name;
	break;

    default:
	break;
    }
}

void
add_plist_top(Package *p, plist_t type, const char *arg)
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
find_plist_option(Package *pkg, const char *name)
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
delete_plist(Package *pkg, Boolean all, plist_t type, const char *name)
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
plist_cmd(const char *s, char **arg)
{
    char cmd[FILENAME_MAX + 20];	/* 20 == fudge for max cmd len */
    char *cp;
    const char *sp;

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
	*arg = (char *)sp;
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
    else if (!strcmp(cmd, "noinst"))
	return PLIST_NOINST;
    else if (!strcmp(cmd, "comment")) {
	if (!strncmp(*arg, "ORIGIN:", 7)) {
	    *arg += 7;
	    return PLIST_ORIGIN;
	} else if (!strncmp(*arg, "DEPORIGIN:", 10)) {
	    *arg += 10;
	    return PLIST_DEPORIGIN;
	}
	return PLIST_COMMENT;
    } else if (!strcmp(cmd, "ignore"))
	return PLIST_IGNORE;
    else if (!strcmp(cmd, "ignore_inst"))
	return PLIST_IGNORE_INST;
    else if (!strcmp(cmd, "name"))
	return PLIST_NAME;
    else if (!strcmp(cmd, "display"))
	return PLIST_DISPLAY;
    else if (!strcmp(cmd, "pkgdep"))
	return PLIST_PKGDEP;
    else if (!strcmp(cmd, "conflicts"))
	return PLIST_CONFLICTS;
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
    int cmd, major, minor;

    pkg->fmtver_maj = 1;
    pkg->fmtver_mnr = 0;
    pkg->origin = NULL;
    while (fgets(pline, FILENAME_MAX, fp)) {
	int len = strlen(pline);

	while (len && isspace(pline[len - 1]))
	    pline[--len] = '\0';
	if (!len)
	    continue;
	cp = pline;
	if (pline[0] != CMD_CHAR) {
	    cmd = PLIST_FILE;
	    goto bottom;
	}
	cmd = plist_cmd(pline + 1, &cp);
	if (cmd == FAIL) {
	    warnx("%s: unknown command '%s' (package tools out of date?)",
		__func__, pline);
	    goto bottom;
	}
	if (*cp == '\0') {
	    cp = NULL;
	    goto bottom;
	}
	if (cmd == PLIST_COMMENT && sscanf(cp, "PKG_FORMAT_REVISION:%d.%d\n",
					   &major, &minor) == 2) {
	    pkg->fmtver_maj = major;
	    pkg->fmtver_mnr = minor;
	    if (verscmp(pkg, PLIST_FMT_VER_MAJOR, PLIST_FMT_VER_MINOR) <= 0)
		goto bottom;

	    warnx("plist format revision (%d.%d) is higher than supported"
		  "(%d.%d)", pkg->fmtver_maj, pkg->fmtver_mnr,
		  PLIST_FMT_VER_MAJOR, PLIST_FMT_VER_MINOR);
	    if (pkg->fmtver_maj > PLIST_FMT_VER_MAJOR) {
		cleanup(0);
		exit(2);
	    }
	}
bottom:
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
	    fprintf(fp, "%ccwd %s\n", CMD_CHAR, (plist->name == NULL) ? "" : plist->name);
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

	case PLIST_NOINST:
	    fprintf(fp, "%cnoinst %s\n", CMD_CHAR, plist->name);
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

	case PLIST_CONFLICTS:
	    fprintf(fp, "%cconflicts %s\n", CMD_CHAR, plist->name);
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

	case PLIST_ORIGIN:
	    fprintf(fp, "%ccomment ORIGIN:%s\n", CMD_CHAR, plist->name);
	    break;

	case PLIST_DEPORIGIN:
	    fprintf(fp, "%ccomment DEPORIGIN:%s\n", CMD_CHAR, plist->name);
	    break;

	default:
	    cleanup(0);
	    errx(2, "%s: unknown command type %d (%s)", __func__,
		plist->type, plist->name);
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
    PackingList p;
    const char *Where = ".", *last_file = "";
    Boolean fail = SUCCESS;
    Boolean preserve;
    char tmp[FILENAME_MAX], *name = NULL;
    char *prefix = NULL;

    preserve = find_plist_option(pkg, "preserve") ? TRUE : FALSE;
    for (p = pkg->head; p; p = p->next) {
	switch (p->type)  {
	case PLIST_NAME:
	    name = p->name;
	    break;

	case PLIST_IGNORE:
	    p = p->next;
	    break;

	case PLIST_CWD:
	    if (!prefix)
		prefix = p->name;
	    Where = (p->name == NULL) ? prefix : p->name;
	    if (Verbose)
		printf("Change working directory to %s\n", Where);
	    break;

	case PLIST_UNEXEC:
	    format_cmd(tmp, FILENAME_MAX, p->name, Where, last_file);
	    if (Verbose)
		printf("Execute '%s'\n", tmp);
	    if (!Fake && system(tmp)) {
		warnx("unexec command for '%s' failed", tmp);
		fail = FAIL;
	    }
	    break;

	case PLIST_FILE:
	    last_file = p->name;
	    sprintf(tmp, "%s/%s", Where, p->name);
	    if (isdir(tmp) && fexists(tmp) && !issymlink(tmp)) {
		warnx("cannot delete specified file '%s' - it is a directory!\n"
	   "this packing list is incorrect - ignoring delete request", tmp);
	    }
	    else {
		if (p->next && p->next->type == PLIST_COMMENT && !strncmp(p->next->name, "MD5:", 4)) {
		    char *cp = NULL, buf[33];

		    /*
		     * For packing lists whose version is 1.1 or greater, the md5
		     * hash for a symlink is calculated on the string returned
		     * by readlink().
		     */
		    if (issymlink(tmp) && verscmp(pkg, 1, 0) > 0) {
			int len;
			char linkbuf[FILENAME_MAX];

			if ((len = readlink(tmp, linkbuf, FILENAME_MAX)) > 0)
			     cp = MD5Data((unsigned char *)linkbuf, len, buf);
		    } else if (isfile(tmp) || verscmp(pkg, 1, 1) < 0)
			cp = MD5File(tmp, buf);

		    if (cp != NULL) {
			/* Mismatch? */
			if (strcmp(cp, p->next->name + 4)) {
			    warnx("'%s' fails original MD5 checksum - %s",
				  tmp, Force ? "deleted anyway." : "not deleted.");
			    if (!Force) {
				fail = FAIL;
				continue;
			    }
			}
		    }
		}
		if (Verbose)
		    printf("Delete file %s\n", tmp);
		if (!Fake) {
		    if (delete_hierarchy(tmp, ign_err, nukedirs))
			fail = FAIL;
		    if (preserve && name) {
			char tmp2[FILENAME_MAX];
			    
			if (make_preserve_name(tmp2, FILENAME_MAX, name, tmp)) {
			    if (fexists(tmp2)) {
				if (rename(tmp2, tmp))
				   warn("preserve: unable to restore %s as %s",
					tmp2, tmp);
			    }
			}
		    }
		}
	    }
	    break;

	case PLIST_DIR_RM:
	    sprintf(tmp, "%s/%s", Where, p->name);
	    if (!isdir(tmp) && fexists(tmp)) {
		warnx("cannot delete specified directory '%s' - it is a file!\n"
	"this packing list is incorrect - ignoring delete request", tmp);
	    }
	    else {
		if (Verbose)
		    printf("Delete directory %s\n", tmp);
		if (!Fake && delete_hierarchy(tmp, ign_err, FALSE)) {
		    warnx("unable to completely remove directory '%s'", tmp);
		    fail = FAIL;
		}
	    }
	    last_file = p->name;
	    break;

	default:
	    break;
	}
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
delete_hierarchy(const char *dir, Boolean ign_err, Boolean nukedirs)
{
    char *cp1, *cp2;

    cp1 = cp2 = strdup(dir);
    if (!fexists(dir)) {
	if (!ign_err)
	    warnx("%s '%s' doesn't exist",
		isdir(dir) ? "directory" : "file", dir);
	return !ign_err;
    }
    else if (nukedirs) {
	if (vsystem("%s -r%s %s", REMOVE_CMD, (ign_err ? "f" : ""), dir))
	    return 1;
    }
    else if (isdir(dir) && !issymlink(dir)) {
	if (RMDIR(dir) && !ign_err)
	    return 1;
    }
    else {
	if (REMOVE(dir, ign_err))
	    return 1;
    }

    if (!nukedirs)
	return 0;
    while (cp2) {
	if ((cp2 = strrchr(cp1, '/')) != NULL)
	    *cp2 = '\0';
	if (!isemptydir(dir))
	    return 0;
	if (RMDIR(dir) && !ign_err) {
	    if (!fexists(dir))
		warnx("directory '%s' doesn't exist", dir);
	    else
		return 1;
	}
	/* back up the pathname one component */
	if (cp2) {
	    cp1 = strdup(dir);
	}
    }
    return 0;
}
