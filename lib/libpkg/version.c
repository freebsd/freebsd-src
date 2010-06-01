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

#include "pkg.h"
#include <err.h>

/*
 * This routine could easily go somewhere else.
 *
 */
int
libpkg_version(void)
{
	return LIBPKG_VERSION;
}

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
 * split_version(pkgname, endname, epoch, revision) returns a pointer to
 * the version portion of a package name and the two special components.
 *
 * Syntax is:  ${PORTNAME}-${PORTVERSION}[_${PORTREVISION}][,${PORTEPOCH}]
 *
 * Written by Oliver Eikemeier
 * Based on work of Jeremy D. Lea.
 */
static const char *
split_version(const char *pkgname, const char **endname, unsigned long *epoch, unsigned long *revision)
{
    char *ch;
    const char *versionstr;
    const char *endversionstr;

    if (pkgname == NULL)
	errx(2, "%s: Passed NULL pkgname.", __func__);

    /* Look for the last '-' the the pkgname */
    ch = strrchr(pkgname, '-');
    /* Cheat if we are just passed a version, not a valid package name */
    versionstr = ch ? ch + 1 : pkgname;

    /* Look for the last '_' in the version string, advancing the end pointer */
    ch = strrchr(versionstr, '_');
    if (revision != NULL) {
	*revision = ch ? strtoul(ch + 1, NULL, 10) : 0;
    }
    endversionstr = ch;

    /* Look for the last ',' in the remaining version string */
    ch = strrchr(endversionstr ? endversionstr + 1 : versionstr, ',');
    if (epoch != NULL) {
	*epoch = ch ? strtoul(ch + 1, NULL, 10) : 0;
    }
    if (ch && !endversionstr)
	endversionstr = ch;

    /* set the pointer behind the last character of the version without revision or epoch */
    if (endname)
	*endname = endversionstr ? endversionstr : strrchr(versionstr, '\0');

    return versionstr;
}

/*
 * PORTVERSIONs are composed of components separated by dots. A component
 * consists of a version number, a letter and a patchlevel number. This does
 * not conform to the porter's handbook, but let us formulate rules that
 * fit the current practice and are far simpler than to make decisions
 * based on the order of netters and lumbers. Besides, people use versions
 * like 10b2 in the ports...
 */

typedef struct {
#ifdef __LONG_LONG_SUPPORTED
    long long n;
    long long pl;
#else
    long n;
    long pl;
#endif
    int a;
} version_component;

/*
 * get_component(position, component) gets the value of the next component
 * (number - letter - number triple) and returns a pointer to the next character
 * after any leading separators
 *
 * - components are separated by dots
 * - characters !~ [a-zA-Z0-9.+*] are treated as separators
 *   (1.0:2003.09.16 = 1.0.2003.09.16), this may not be what you expect:
 *   1.0.1:2003.09.16 < 1.0:2003.09.16
 * - consecutive separators are collapsed (10..1 = 10.1)
 * - missing separators are inserted, essentially
 *   letter number letter => letter number . letter (10a1b2 = 10a1.b2)
 * - missing components are assumed to be equal to 0 (10 = 10.0 = 10.0.0)
 * - the letter sort order is: [none], a, b, ..., z; numbers without letters
 *   sort first (10 < 10a < 10b)
 * - missing version numbers (in components starting with a letter) sort as -1
 *   (a < 0, 10.a < 10)
 * - a separator is inserted before the special strings "pl", "alpha", "beta", 
 *   "pre" and "rc".
 * - "pl" sorts before every other letter, "alpha", "beta", "pre" and "rc"
 *   sort as a, b, p and r. (10alpha = 10.a < 10, but 10 < 10a; pl11 < alpha3
 *   < 0.1beta2 = 0.1.b2 < 0.1)
 * - other strings use only the first letter for sorting, case is ignored
 *   (1.d2 = 1.dev2 = 1.Development2)
 * - The special component `*' is guaranteed to be the smallest possible
 *   component (2.* < 2pl1 < 2alpha3 < 2.9f7 < 3.*)
 * - components separated by `+' are handled by version_cmp below
 *
 * Oliver Eikemeier
 */

static const struct {
    const char *name;
    size_t namelen;
    int value;
} stage[] = {
    { "pl",    2,  0        },
    { "alpha", 5, 'a'-'a'+1 },
    { "beta",  4, 'b'-'a'+1 },
    { "pre",   3, 'p'-'a'+1 },
    { "rc",    2, 'r'-'a'+1 },
    { NULL,    0,  -1       }
};

static const char *
get_component(const char *position, version_component *component)
{
    const char *pos = position;
    int hasstage = 0, haspatchlevel = 0;

    if (!pos)
	errx(2, "%s: Passed NULL position.", __func__);

    /* handle version number */
    if (isdigit(*pos)) {
	char *endptr;
#ifdef __LONG_LONG_SUPPORTED
	component->n = strtoll(pos, &endptr, 10);
#else
	component->n = strtol(pos, &endptr, 10);
#endif
	/* should we test for errno == ERANGE? */
	pos = endptr;
    } else if (*pos == '*') {
	component->n = -2;
	do {
	    pos++;
	} while(*pos && *pos != '+');
    } else {
	component->n = -1;
	hasstage = 1;
    }

    /* handle letter */
    if (isalpha(*pos)) {
	int c = tolower(*pos);
	haspatchlevel = 1;
	/* handle special suffixes */
	if (isalpha(pos[1])) {
	    int i;
	    for (i = 0; stage[i].name; i++) {
		if (strncasecmp(pos, stage[i].name, stage[i].namelen) == 0
                    && !isalpha(pos[stage[i].namelen])) {
		    if (hasstage) {
			/* stage to value */
			component->a = stage[i].value;
			pos += stage[i].namelen;
		    } else {
			/* insert dot */
			component->a = 0;
			haspatchlevel = 0;
		    }
		    c = 0;
		    break;
		}
	    }
	}
	/* unhandled above */
	if (c) {
	    /* use the first letter and skip following */
	    component->a = c - 'a' + 1;
	    do {
		++pos;
	    } while (isalpha(*pos));
	}
    } else {
	component->a = 0;
	haspatchlevel = 0;
    }

    if (haspatchlevel) {
	/* handle patch number */
	if (isdigit(*pos)) {
	    char *endptr;
#ifdef __LONG_LONG_SUPPORTED
	    component->pl = strtoll(pos, &endptr, 10);
#else
	    component->pl = strtol(pos, &endptr, 10);
#endif
	    /* should we test for errno == ERANGE? */
	    pos = endptr;
	} else {
	    component->pl = -1;
	}
    } else {
	component->pl = 0;
    }

    /* skip trailing separators */
    while (*pos && !isdigit(*pos) && !isalpha(*pos) && *pos != '+' && *pos != '*') {
	pos++;
    }

    return pos;
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
 * The tests allow for significantly more latitude in the version numbers
 * than is allowed in the guidelines. No point in enforcing them here.
 * That's what portlint is for.
 *
 * Jeremy D. Lea.
 * reimplemented by Oliver Eikemeier
 */
int
version_cmp(const char *pkg1, const char *pkg2)
{
    const char *v1, *v2, *ve1, *ve2;
    unsigned long e1, e2, r1, r2;
    int result = 0;

    v1 = split_version(pkg1, &ve1, &e1, &r1);
    v2 = split_version(pkg2, &ve2, &e2, &r2);

    /* Check epoch, port version, and port revision, in that order. */
    if (e1 != e2) {
	result = (e1 < e2 ? -1 : 1);
    }

    /* Shortcut check for equality before invoking the parsing routines. */
    if (result == 0 && (ve1 - v1 != ve2 - v2 || strncasecmp(v1, v2, ve1 - v1) != 0)) {
	/* Loop over different components (the parts separated by dots).
	 * If any component differs, we have the basis for an inequality. */
	while(result == 0 && (v1 < ve1 || v2 < ve2)) {
	    int block_v1 = 0;
	    int block_v2 = 0;
	    version_component vc1 = {0, 0, 0};
	    version_component vc2 = {0, 0, 0};
	    if (v1 < ve1 && *v1 != '+') {
		v1 = get_component(v1, &vc1);
	    } else {
		block_v1 = 1;
	    }
	    if (v2 < ve2 && *v2 != '+') {
		v2 = get_component(v2, &vc2);
	    } else {
		block_v2 = 1;
	    }
	    if (block_v1 && block_v2) {
		if (v1 < ve1)
		    v1++;
		if (v2 < ve2)
		    v2++;
	    } else if (vc1.n != vc2.n) {
		result = (vc1.n < vc2.n ? -1 : 1);
	    } else if (vc1.a != vc2.a) {
		result = (vc1.a < vc2.a ? -1 : 1);
	    } else if (vc1.pl != vc2.pl) {
		result = (vc1.pl < vc2.pl ? -1 : 1);
	    }
	}
    }

    /* Compare FreeBSD revision numbers. */
    if (result == 0 && r1 != r2) {
	result = (r1 < r2 ? -1 : 1);
    }
    return result;
}
