#ifndef lint
static const char *rcsid = "$Id: perform.c,v 1.9 1994/10/14 05:57:49 jkh Exp $";
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
 * This is the main body of the info module.
 *
 */

#include "lib.h"
#include "info.h"

#include <signal.h>

static int pkg_do(char *);

int
pkg_perform(char **pkgs)
{
    int i, err_cnt = 0;

    signal(SIGINT, cleanup);

    /* Overriding action? */
    if (AllInstalled || CheckPkg) {
	if (isdir(LOG_DIR)) {
	    DIR *dirp;
	    struct dirent *dp;

	    dirp = opendir(LOG_DIR);
	    if (dirp) {
		for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
		    if (strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) {
			if (CheckPkg) {
			    if (!strcmp(dp->d_name, CheckPkg))
				return 0;
			}
			else 
			    err_cnt += pkg_do(dp->d_name);
		    }
		}
		(void)closedir(dirp);
		if (CheckPkg)
		    return 1;
	    }
	    else
		++err_cnt;
	} else if (CheckPkg)
	    return 1;			/* no dir -> not installed! */
	    
    }
    for (i = 0; pkgs[i]; i++)
	err_cnt += pkg_do(pkgs[i]);
    return err_cnt;
}

static int
pkg_do(char *pkg)
{
    Boolean installed = FALSE;
    char log_dir[FILENAME_MAX];
    char home[FILENAME_MAX];
    Package plist;
    FILE *fp;

    if (fexists(pkg)) {
	char fname[FILENAME_MAX];
	struct stat sb;

	if (!getcwd(home, FILENAME_MAX))
	    upchuck("getcwd");

	if (pkg[0] == '/')
	    strcpy(fname, pkg);
	else
	    sprintf(fname, "%s/%s", home, pkg);
	/*
	 * Apply a crude heuristic to see how much space the package will
	 * take up once it's unpacked.  I've noticed that most packages
	 * compress an average of 75%, but we're only unpacking the + files so
	 * be very optimistic.
	 */
	if (stat(fname, &sb) == FAIL) {
	    whinge("Can't stat package file '%s'.", fname);
	    return 1;
	}
	(void)make_playpen(PlayPen, sb.st_size / 2);
	if (unpack(fname, "+*")) {
	    whinge("Error during unpacking, no info for '%s' available.", pkg);
	    return 1;
	}
    }
    else {
	sprintf(log_dir, "%s/%s", LOG_DIR, pkg);
	if (!fexists(log_dir)) {
	    whinge("Can't find package '%s' installed or in a file!", pkg);
	    return 1;
	}
	if (chdir(log_dir) == FAIL) {
	    whinge("Can't change directory to '%s'!", log_dir);
	    return 1;
	}
	installed = TRUE;
    }

    /* Suck in the contents list */
    plist.head = plist.tail = NULL;
    fp = fopen(CONTENTS_FNAME, "r");
    if (!fp) {
	whinge("Unable to open %s file.", CONTENTS_FNAME);
	return 1;
    }
    /* If we have a prefix, add it now */
    read_plist(&plist, fp);
    fclose(fp);

    /*
     * Index is special info type that has to override all others to make
     * any sense.
     */
    if (Flags & SHOW_INDEX) {
	char fname[FILENAME_MAX];

	sprintf(fname, "%s\t", pkg);
	show_file(fname, COMMENT_FNAME);
    }
    else {
	/* Start showing the package contents */
	if (!Quiet)
	    printf("%sInformation for %s:\n\n", InfoPrefix, pkg);
	if (Flags & SHOW_COMMENT)
	    show_file("Comment:\n", COMMENT_FNAME);
	if ((Flags & SHOW_REQBY) && !isemptyfile(REQUIRED_BY_FNAME))
	    show_file("Required by:\n", REQUIRED_BY_FNAME);
	if (Flags & SHOW_DESC)
	    show_file("Description:\n", DESC_FNAME);
	if ((Flags & SHOW_DISPLAY) && fexists(DISPLAY_FNAME))
	    show_file("Install notice:\n", DISPLAY_FNAME);
	if (Flags & SHOW_PLIST)
	    show_plist("Packing list:\n", &plist, (plist_t)-1);
	if ((Flags & SHOW_INSTALL) && fexists(INSTALL_FNAME))
	    show_file("Install script:\n", INSTALL_FNAME);
	if ((Flags & SHOW_DEINSTALL) && fexists(DEINSTALL_FNAME))
	    show_file("De-Install script:\n", DEINSTALL_FNAME);
	if ((Flags & SHOW_MTREE) && fexists(MTREE_FNAME))
	    show_file("mtree file:\n", MTREE_FNAME);
	if (Flags & SHOW_PREFIX)
	    show_plist("Prefix(s):\n", &plist, PLIST_CWD);
	if (Flags & SHOW_FILES)
	    show_files("Files:\n", &plist);
	if (!Quiet)
	    puts(InfoPrefix);
    }
    free_plist(&plist);
    leave_playpen();
    return 0;
}

void
cleanup(int sig)
{
    leave_playpen();
}
