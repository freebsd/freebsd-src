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
 * 31 July 2001
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"
#include <err.h>

/*
 * Routines to assist with PLIST_FMT_VER numbers in the packing
 * lists.
 *
 * Following is the PLIST_FMT_VER history:
 * 1.0 - Initial revision;
 * 1.1 - When recording/checking checksum of symlink use hash of readlink()
 *	 value instead of the hash of an object this links points to.
 *
 */
int
verscmp(Package *pkg, int major, int minor)
{
    int rval = 0;

    if ((pkg->fmtver_maj < major) || (pkg->fmtver_maj == major &&
	pkg->fmtver_mnr < minor))
	rval = -1;
    else if ((pkg->fmtver_maj > major) || (pkg->fmtver_maj == major &&
	     pkg->fmtver_mnr > minor))
	rval = 1;

    return rval;
}

/*
 * version_of(pkgname, epoch, revision) returns a pointer to the version
 * portion of a package name and the two special components.
 *
 * Jeremy D. Lea.
 */
const char *
version_of(const char *pkgname, int *epoch, int *revision)
{
    char *ch;

    if (pkgname == NULL)
	errx(2, "%s: Passed NULL pkgname.", __func__);
    if (epoch != NULL) {
	if ((ch = strrchr(pkgname, ',')) == NULL)
	    *epoch = 0;
	else
	    *epoch = atoi(&ch[1]);
    }
    if (revision != NULL) {
	if ((ch = strrchr(pkgname, '_')) == NULL)
	    *revision = 0;
	else
	    *revision = atoi(&ch[1]);
    }
    /* Cheat if we are just passed a version, not a valid package name */
    if ((ch = strrchr(pkgname, '-')) == NULL)
	return pkgname;
    else
	return &ch[1];
}

/*
 * version_cmp(pkg1, pkg2) returns -1, 0 or 1 depending on if the version
 * components of pkg1 is less than, equal to or greater than pkg2. No
 * comparison of the basenames is done.
 *
 * The port version is defined by:
 * ${PORTVERSION}[_${PORTREVISION}][,${PORTEPOCH}]
 * ${PORTEPOCH} supersedes ${PORTVERSION} supersedes ${PORTREVISION}.
 * See the commit log for revision 1.349 of ports/Mk/bsd.port.mk
 * for more information.
 *
 * The epoch and revision are defined to be a single number, while the rest
 * of the version should conform to the porting guidelines. It can contain
 * multiple components, separated by a period, including letters.
 *
 * The tests below allow for significantly more latitude in the version
 * numbers than is allowed in the guidelines. No point in wasting user's
 * time enforcing them here. That's what flamewars are for.
 *
 * Jeremy D. Lea.
 */
int
version_cmp(const char *pkg1, const char *pkg2)
{
    const char *c1, *c2, *v1, *v2;
    char *t1, *t2;
    int e1, e2, r1, r2, n1, n2;

    v1 = version_of(pkg1, &e1, &r1);
    v2 = version_of(pkg2, &e2, &r2);
    /* Minor optimisation. */
    if (strcmp(v1, v2) == 0)
	return 0;
    /* First compare epoch. */
    if (e1 != e2)
	return (e1 < e2 ? -1 : 1);
    else {
	/*
	 * We walk down the versions, trying to convert to numbers.
	 * We terminate when we reach an underscore, a comma or the
	 * string terminator, thanks to a nasty trick with strchr().
	 * strtol() conveniently gobbles up the chars it converts.
	 */
	c1 = strchr("_,", v1[0]);
	c2 = strchr("_,", v2[0]);
	while (c1 == NULL && c2 == NULL) {
	    n1 = strtol(v1, &t1, 10);
	    n2 = strtol(v2, &t2, 10);
	    if (n1 != n2)
		return (n1 < n2 ? -1 : 1);
	    /*
	     * The numbers are equal, check for letters. Assume they're
	     * letters purely because strtol() didn't chomp them.
	     */
	    c1 = strchr("_,.", t1[0]);
	    c2 = strchr("_,.", t2[0]);
	    if (c1 == NULL && c2 == NULL) {
		/* Both have letters. Compare them. */
		if (t1[0] != t2[0])
		    return (t1[0] < t2[0] ? -1 : 1);
		/* Boring. The letters are equal. Carry on. */
		v1 = &t1[1], v2 = &t2[1];
	    } else if (c1 == NULL) {
		/*
		 * Letters are strange. After a number, a letter counts
		 * as greater, but after a period it's less.
		 */
		return (isdigit(v1[0]) ? 1 : -1);
	    } else if (c2 == NULL) {
		return (isdigit(v2[0]) ? -1 : 1);
	    } else {
		/* Neither were letters.  Advance over the period. */
		v1 = (t1[0] == '.' ? &t1[1] : t1);
		v2 = (t2[0] == '.' ? &t2[1] : t2);
	    }
	    c1 = strchr("_,", v1[0]);
	    c2 = strchr("_,", v2[0]);
	}
	/* If we got here, check if one version has something left. */
	if (c1 == NULL)
	    return (isdigit(v1[0]) ? 1 : -1);
	if (c2 == NULL)
	    return (isdigit(v2[0]) ? -1 : 1);
	/* We've run out of version. Try the revision... */
	if (r1 != r2)
	    return (r1 < r2 ? -1 : 1);
	else
	    return 0;
    }
}
