/*
 * FreeBSD install - a package for the installation and maintenance
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"
#include <err.h>
#include <stdio.h>

void list_deps(const char *pkgname, char **pkgs, char *listed, 
               char *check_loop, char **newpkgs, int *nrnewpkgs,
               int *err_cnt);

/*
 * Sort given NULL-terminated list of installed packages (pkgs) in
 * such a way that if package A depends on package B then after
 * sorting A will be listed before B no matter how they were
 * originally positioned in the list.
 *
 * Works by performing a recursive depth-first search on the 
 * required-by lists.
 */

int
sortdeps(char **pkgs)
{
    int i, err_cnt=0;
    int nrpkgs, nrnewpkgs;
    char *listed, *check_loop, **newpkgs;
    char *cp;

    if (pkgs[0] == NULL || pkgs[1] == NULL)
	return (0);

    nrpkgs = 0;
    while (pkgs[nrpkgs]) nrpkgs++;
    listed = alloca(nrpkgs);
    if (listed == NULL) {
	warnx("%s(): alloca() failed", __func__);
	return 1;
    }
    bzero(listed,nrpkgs);
    check_loop = alloca(nrpkgs);
    if (check_loop == NULL) {
	warnx("%s(): alloca() failed", __func__);
	return 1;
    }
    bzero(check_loop,nrpkgs);
    newpkgs = alloca(nrpkgs*sizeof(char*));
    if (newpkgs == NULL) {
	warnx("%s(): alloca() failed", __func__);
	return 1;
    }
    nrnewpkgs = 0;

    for (i = 0; pkgs[i]; i++) if (!listed[i]) {
	check_loop[i] = 1;
	cp = strchr(pkgs[i], ':');
	if (cp != NULL)
	    *cp = '\0';
	list_deps(pkgs[i],pkgs,listed,check_loop,newpkgs,&nrnewpkgs,&err_cnt);
	if (cp != NULL)
	    *cp = ':';
	listed[i] = 1;
	newpkgs[nrnewpkgs] = pkgs[i];
	nrnewpkgs++;
    }

    if (nrnewpkgs != nrpkgs) {
	fprintf(stderr,"This shouldn't happen, and indicates a huge error in the code.\n");
	exit(1);
    }
    for (i = 0; i < nrnewpkgs; i++) pkgs[i] = newpkgs[i];

    return err_cnt;
}

/*
 * This recursive function lists the dependencies (that is, the 
 * "required-by"s) for pkgname, putting them into newpkgs.
 */

void list_deps(const char *pkgname, char **pkgs, char *listed, 
               char *check_loop, char **newpkgs, int *nrnewpkgs,
               int *err_cnt) {
    char **rb, **rbtmp;
    char *cp;
    int errcode, i, j;
    struct reqr_by_entry *rb_entry;
    struct reqr_by_head *rb_list;

    if (isinstalledpkg(pkgname) <= 0)
	return;

    errcode = requiredby(pkgname, &rb_list, FALSE, TRUE);
    if (errcode < 0)
	return;
    /*
     * We put rb_list into an argv style NULL terminated list,
     * because requiredby uses some static storage, and list_deps
     * is a recursive function.
     */

    rbtmp = rb = alloca((errcode + 1) * sizeof(*rb));
    if (rb == NULL) {
	warnx("%s(): alloca() failed", __func__);
	(*err_cnt)++;
	return;
    }
    STAILQ_FOREACH(rb_entry, rb_list, link) {
	*rbtmp = alloca(strlen(rb_entry->pkgname) + 1);
	if (*rbtmp == NULL) {
	    warnx("%s(): alloca() failed", __func__);
	    (*err_cnt)++;
	    return;
	}
	strcpy(*rbtmp, rb_entry->pkgname);
	rbtmp++;
    }
    *rbtmp = NULL;

    for (i = 0; rb[i]; i++)
	for (j = 0; pkgs[j]; j++) if (!listed[j]) {
	    cp = strchr(pkgs[j], ':');
	    if (cp != NULL)
		*cp = '\0';
	    if (strcmp(rb[i], pkgs[j]) == 0) { /*match */
		/*
		 * Try to avoid deadlock if package A depends on B which in
		 * turn depends on C and C due to an error depends on A.
		 * It Should Never Happen[tm] in real life.
		 */
		if (check_loop[j]) {
		    warnx("dependency loop detected for package %s", pkgs[j]);
		    (*err_cnt)++;
		}
		else {
		    check_loop[j] = 1;
		    list_deps(pkgs[j],pkgs,listed,check_loop,newpkgs,nrnewpkgs,err_cnt);
		    listed[j] = 1;
		    newpkgs[*nrnewpkgs] = pkgs[j];
		    (*nrnewpkgs)++;
		}
	    }
	    if (cp != NULL)
		*cp = ':';
	}
}

/*
 * Load +REQUIRED_BY file and return a list with names of
 * packages that require package referred to by `pkgname'.
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

    if (isinstalledpkg(pkgname) <= 0) {
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
	if (filter == TRUE && isinstalledpkg(fbuf) <= 0) {
	    if (strict == TRUE)
		warnx("package '%s' is recorded in the '%s' but isn't "
		      "actually installed", fbuf, fname);
	    continue;
	}
	retval++;
	rb_entry = malloc(sizeof(*rb_entry));
	if (rb_entry == NULL) {
	    warnx("%s(): malloc() failed", __func__);
	    retval = -1;
	    break;
	}
	strlcpy(rb_entry->pkgname, fbuf, sizeof(rb_entry->pkgname));
	STAILQ_INSERT_TAIL(&rb_list, rb_entry, link);
    }
    fclose(fp);

    return retval;
}
