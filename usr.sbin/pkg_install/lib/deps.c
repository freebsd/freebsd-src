#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
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
 * Maxim Sobolev
 * 14 March 2001
 *
 * Routines used to do various operations with dependencies
 * among installed packages.
 *
 */

#include "lib.h"
#include <err.h>
#include <stdio.h>

/*
 * Sort given NULL-terminated list of installed packages (pkgs) in
 * such a way that if package A depends on package B then after
 * sorting A will be listed before B no matter how they were
 * originally positioned in the list.
 */
int
sortdeps(char **pkgs)
{
    char *tmp;
    int i, j, loop_cnt;
    int err_cnt = 0;

    for (i = 0; pkgs[i]; i++) {
	/*
	 * Check to see if any other package in pkgs[i+1:] depends
	 * on pkgs[i] and swap those two packages if so.
	 */
	loop_cnt = 0;
	for (j = i + 1; pkgs[j]; j++) {
	    if (chkifdepends(pkgs[j], pkgs[i]) == 1) {
		/*
		 * Try to avoid deadlock if package A depends on B which in
		 * turn depends on C and C due to an error depends on A.
		 * Use ugly but simple method, becase it Should Never
		 * Happen[tm] in the real life anyway.
		 */
		if (loop_cnt > 4096) {
		    warnx("dependency loop detected for package %s", pkgs[j]);
		    err_cnt++;
		    break;
		}
		loop_cnt++;
		tmp = pkgs[i];
		pkgs[i] = pkgs[j];
		pkgs[j] = tmp;
		/*
		 * Another iteration requred to check if new pkgs[i]
		 * itself has any packages that depend on it
		 */
		j = i + 1;
	    }
	}
    }
    return err_cnt;
}

/*
 * Check to see if pkgname1 depends on pkgname2.
 * Returns 1 if depends, 0 if not, and -1 if error occured.
 */ 
int
chkifdepends(char *pkgname1, char *pkgname2)
{
    FILE *fp;
    char fname[FILENAME_MAX];
    char fbuf[FILENAME_MAX];
    char *tmp;
    int retval;

    sprintf(fname, "%s/%s/%s", LOG_DIR, pkgname2, REQUIRED_BY_FNAME);
    fp = fopen(fname, "r");
    if (fp == NULL) {
	/* Probably pkgname2 doesn't have any packages that depend on it */
	return 0;
    }

    retval = 0;
    while (fgets(fbuf, sizeof(fbuf), fp) != NULL) {
	if (fbuf[strlen(fbuf)-1] == '\n')
	    fbuf[strlen(fbuf)-1] = '\0';
	if (strcmp(fbuf, pkgname1) == 0) {	/* match */
	    retval = 1;
	    break;
	}
    }

    fclose(fp);
    return retval;
}
