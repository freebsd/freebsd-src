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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"
#include "info.h"
#include <err.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <md5.h>

void
show_file(const char *title, const char *fname)
{
    FILE *fp;
    char line[1024];
    int n;

    if (!Quiet)
	printf("%s%s", InfoPrefix, title);
    fp = fopen(fname, "r");
    if (fp == (FILE *) NULL)
	printf("ERROR: show_file: Can't open '%s' for reading!\n", fname);
    else {
	int append_nl = 0;
	while ((n = fread(line, 1, 1024, fp)) != 0)
	    fwrite(line, 1, n, stdout);
	fclose(fp);
	append_nl = (line[n - 1] != '\n');	/* Do we have a trailing \n ? */
	if (append_nl)
	   printf("\n");
    }
    printf("\n");	/* just in case */
}

void
show_index(const char *title, const char *fname)
{
    FILE *fp;
    char line[MAXINDEXSIZE+2];

    strlcpy(line, "???\n", sizeof(line));

    if (!Quiet)
        printf("%s%s", InfoPrefix, title);
    fp = fopen(fname, "r");
    if (fp == (FILE *) NULL) {
        warnx("show_file: can't open '%s' for reading", fname);
    } else {
    	if(fgets(line, MAXINDEXSIZE + 1, fp)) {
		size_t line_length = strlen(line);

		if (line[line_length - 1] != '\n') {	/* Do we have a trailing \n ? */
			line[line_length] = '\n';	/* Add a trailing \n */
			line[line_length + 1] = '\0';	/* Terminate string */
		}
	}
	fclose(fp);
    }
    fputs(line, stdout);
}

/* Show a packing list item type.  If showall is TRUE, show all */
void
show_plist(const char *title, Package *plist, plist_t type, Boolean showall)
{
    PackingList p;
    Boolean ign = FALSE;

    if (!Quiet)
	printf("%s%s", InfoPrefix, title);
    p = plist->head;
    while (p) {
	if (p->type != type && showall != TRUE) {
	    p = p->next;
	    continue;
	}
	switch(p->type) {
	case PLIST_FILE:
	    if (ign) {
		printf(Quiet ? "%s\n" : "File: %s (ignored)\n", p->name);
		ign = FALSE;
	    }
	    else
		printf(Quiet ? "%s\n" : "File: %s\n", p->name);
	    break;

	case PLIST_CWD:
	    printf(Quiet ? "@cwd %s\n" : "\tCWD to %s\n", p->name);
	    break;

	case PLIST_SRC:
	    printf(Quiet ? "@srcdir %s\n" : "\tSRCDIR to %s\n", p->name);
	    break;

	case PLIST_CMD:
	    printf(Quiet ? "@exec %s\n" : "\tEXEC '%s'\n", p->name);
	    break;

	case PLIST_UNEXEC:
	    printf(Quiet ? "@unexec %s\n" : "\tUNEXEC '%s'\n", p->name);
	    break;

	case PLIST_CHMOD:
	    printf(Quiet ? "@chmod %s\n" : "\tCHMOD to %s\n",
		   p->name ? p->name : "(clear default)");
	    break;

	case PLIST_CHOWN:
	    printf(Quiet ? "@chown %s\n" : "\tCHOWN to %s\n",
		   p->name ? p->name : "(clear default)");
	    break;

	case PLIST_CHGRP:
	    printf(Quiet ? "@chgrp %s\n" : "\tCHGRP to %s\n",
		   p->name ? p->name : "(clear default)");
	    break;

	case PLIST_COMMENT:
	    printf(Quiet ? "@comment %s\n" : "\tComment: %s\n", p->name);
	    break;

	case PLIST_IGNORE:
	    ign = TRUE;
	    break;

	case PLIST_IGNORE_INST:
	    printf(Quiet ? "@ignore_inst ??? doesn't belong here.\n" :
		   "\tIgnore next file installation directive (doesn't belong)\n");
	    ign = TRUE;
	    break;

	case PLIST_NAME:
	    printf(Quiet ? "@name %s\n" : "\tPackage name: %s\n", p->name);
	    break;

	case PLIST_DISPLAY:
	    printf(Quiet ? "@display %s\n" : "\tInstall message file: %s\n", p->name);
	    break;

	case PLIST_PKGDEP:
	    printf(Quiet ? "@pkgdep %s\n" : "Dependency: %s\n", p->name);
	    break;

	case PLIST_DEPORIGIN:
	    printf(Quiet ? "@comment DEPORIGIN:%s\n" :
		"\tdependency origin: %s\n", p->name);
	    break;

	case PLIST_CONFLICTS:
	    printf(Quiet ? "@conflicts %s\n" : "Conflicts: %s\n", p->name);
	    break;

	case PLIST_MTREE:
	    printf(Quiet ? "@mtree %s\n" : "\tPackage mtree file: %s\n", p->name);
	    break;

	case PLIST_DIR_RM:
	    printf(Quiet ? "@dirrm %s\n" : "\tDeinstall directory remove: %s\n", p->name);
	    break;

	case PLIST_OPTION:
	    printf(Quiet ? "@option %s\n" :
		"\tOption \"%s\" controlling package installation behaviour\n",
		p->name);
	    break;

	case PLIST_ORIGIN:
	    printf(Quiet ? "@comment ORIGIN:%s\n" :
		"\tPackage origin: %s\n", p->name); 
	    break;

	default:
	    cleanup(0);
	    errx(2, "%s: unknown command type %d (%s)",
		__func__, p->type, p->name);
	    break;
	}
	p = p->next;
    }
}

/* Show all files in the packing list (except ignored ones) */
void
show_files(const char *title, Package *plist)
{
    PackingList p;
    Boolean ign = FALSE;
    const char *dir = ".";

    if (!Quiet)
	printf("%s%s", InfoPrefix, title);
    p = plist->head;
    while (p) {
	switch(p->type) {
	case PLIST_FILE:
	    if (!ign)
		printf("%s/%s\n", dir, p->name);
	    ign = FALSE;
	    break;

	case PLIST_CWD:
	    dir = p->name;
	    break;

	case PLIST_IGNORE:
	    ign = TRUE;
	    break;

        /* Silence GCC in the -Wall mode */
	default:
	    break;
	}
	p = p->next;
    }
}

/* Calculate and show size of all installed package files (except ignored ones) */
void
show_size(const char *title, Package *plist)
{
    PackingList p;
    Boolean ign = FALSE;
    const char *dir = ".";
    struct stat sb;
    char tmp[FILENAME_MAX];
    unsigned long size = 0;
    long blksize;
    int headerlen;
    char *descr;

    descr = getbsize(&headerlen, &blksize);
    if (!Quiet)
	printf("%s%s", InfoPrefix, title);
    for (p = plist->head; p != NULL; p = p->next) {
	switch (p->type) {
	case PLIST_FILE:
	    if (!ign) {
		snprintf(tmp, FILENAME_MAX, "%s/%s", dir, p->name);
		if (!lstat(tmp, &sb)) {
		    size += sb.st_size;
		    if (Verbose)
			printf("%lu\t%s\n", (unsigned long) howmany(sb.st_size, blksize), tmp);
		}
	    }
	    ign = FALSE;
	    break;

	case PLIST_CWD:
	    dir = p->name;
	    break;

	case PLIST_IGNORE:
	    ign = TRUE;
	    break;

	/* Silence GCC in the -Wall mode */	    
	default:
	    break;
	}
    }
    if (!Quiet)
	printf("%lu\t(%s)\n", howmany(size, blksize), descr);
    else
	printf("%lu\n", size);
}

/* Show files that don't match the recorded checksum */
void
show_cksum(const char *title, Package *plist)
{
    PackingList p;
    const char *dir = ".";
    char tmp[FILENAME_MAX];

    if (!Quiet)
	printf("%s%s", InfoPrefix, title);

    for (p = plist->head; p != NULL; p = p->next)
	if (p->type == PLIST_CWD) 
	    dir = p->name;
	else if (p->type == PLIST_FILE) {
	    snprintf(tmp, FILENAME_MAX, "%s/%s", dir, p->name);
	    if (!fexists(tmp))
		warnx("%s doesn't exist\n", tmp);
	    else if (p->next && p->next->type == PLIST_COMMENT &&
	             (strncmp(p->next->name, "MD5:", 4) == 0)) {
		char *cp = NULL, buf[33];

		/*
		 * For packing lists whose version is 1.1 or greater, the md5
		 * hash for a symlink is calculated on the string returned
		 * by readlink().
		 */
		if (issymlink(tmp) && verscmp(plist, 1, 0) > 0) {
		    int len;
		    char linkbuf[FILENAME_MAX];

		    if ((len = readlink(tmp, linkbuf, FILENAME_MAX)) > 0)
			cp = MD5Data((unsigned char *)linkbuf, len, buf);
		} else if (isfile(tmp) || verscmp(plist, 1, 1) < 0)
		    cp = MD5File(tmp, buf);

		if (cp != NULL) {
		    /* Mismatch? */
		    if (strcmp(cp, p->next->name + 4))
			printf("%s fails the original MD5 checksum\n", tmp);
		    else if (Verbose)
			printf("%s matched the original MD5 checksum\n", tmp);
		}
	    }
	}
}

/* Show an "origin" path (usually category/portname) */
void
show_origin(const char *title, Package *plist)
{

    if (!Quiet)
	printf("%s%s", InfoPrefix, title);
    printf("%s\n", plist->origin != NULL ? plist->origin : "");
}

/* Show revision number of the packing list */
void
show_fmtrev(const char *title, Package *plist)
{

    if (!Quiet)
	printf("%s%s", InfoPrefix, title);
    printf("%d.%d\n", plist->fmtver_maj, plist->fmtver_mnr);
}
