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

    if (pkgs[0] == NULL || pkgs[1] == NULL)
	return (0);

    for (i = 0; pkgs[i + 1]; i++) {
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
chkifdepends(const char *pkgname1, const char *pkgname2)
{
    char *cp1, *cp2;
    int errcode;
    struct reqr_by_entry *rb_entry;
    struct reqr_by_head *rb_list;

    cp2 = strchr(pkgname2, ':');
    if (cp2 != NULL)
	*cp2 = '\0';
    cp1 = strchr(pkgname1, ':');
    if (cp1 != NULL)
	*cp1 = '\0';

    errcode = 0;
    /* Check that pkgname2 is actually installed */
    if (!isinstalledpkg(pkgname2))
	goto exit;

    errcode = requiredby(pkgname2, &rb_list, FALSE, TRUE);
    if (errcode < 0)
	goto exit;

    errcode = 0;
    STAILQ_FOREACH(rb_entry, rb_list, link) {
	if (strcmp(rb_entry->pkgname, pkgname1) == 0) {	/* match */
	    errcode = 1;
	    break;
	}
    }

exit:
    if (cp1 != NULL)
	*cp1 = ':';
    if (cp2 != NULL)
	*cp2 = ':';
    return errcode;
}

/*
 * Load +REQUIRED_BY file and return a list with names of
 * packages that require package reffered to by `pkgname'.
 *
 * Optionally check that packages listed there are actually
 * installed and filter out those that don't (filter == TRUE).
 *
 * strict argument controls whether the caller want warnings
 * to be emitted when there are some non-fatal conditions,
 * i.e. package doesn't have +REQUIRED_BY file or some packages
 * listed in +REQUIRED_BY don't exist.
 *
 * Result returned in the **list, while return value is equal
 * to the number of entries in the resulting list. Print error
 * message and return -1 on error.
 */
int
requiredby(const char *pkgname, struct reqr_by_head **list, Boolean strict, Boolean filter)
{
    FILE *fp;
    char fbuf[FILENAME_MAX], fname[FILENAME_MAX];
    int retval;
    struct reqr_by_entry *rb_entry;
    static struct reqr_by_head rb_list = STAILQ_HEAD_INITIALIZER(rb_list);

    *list = &rb_list;
    /* Deallocate any previously allocated space */
    while (!STAILQ_EMPTY(&rb_list)) {
	rb_entry = STAILQ_FIRST(&rb_list);
	STAILQ_REMOVE_HEAD(&rb_list, link);
	free(rb_entry);
    }

    if (!isinstalledpkg(pkgname)) {
	if (strict == TRUE)
	    warnx("no such package '%s' installed", pkgname);
	return -1;
    }

    snprintf(fname, sizeof(fname), "%s/%s/%s", LOG_DIR, pkgname,
	     REQUIRED_BY_FNAME);
    fp = fopen(fname, "r");
    if (fp == NULL) {
	/* Probably pkgname doesn't have any packages that depend on it */
	if (strict == TRUE)
	    warnx("couldn't open dependency file '%s'", fname);
	return 0;
    }

    retval = 0;
    while (fgets(fbuf, sizeof(fbuf), fp) != NULL) {
	if (fbuf[strlen(fbuf) - 1] == '\n')
	    fbuf[strlen(fbuf) - 1] = '\0';
	if (filter == TRUE && !isinstalledpkg(fbuf)) {
	    if (strict == TRUE)
		warnx("package '%s' is recorded in the '%s' but isn't "
		      "actually installed", fbuf, fname);
	    continue;
	}
	retval++;
	rb_entry = malloc(sizeof(*rb_entry));
	if (rb_entry == NULL) {
	    warnx("%s(): malloc() failed", __FUNCTION__);
	    retval = -1;
	    break;
	}
	strlcpy(rb_entry->pkgname, fbuf, sizeof(rb_entry->pkgname));
	STAILQ_INSERT_TAIL(&rb_list, rb_entry, link);
    }
    fclose(fp);

    return retval;
}
