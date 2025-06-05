/* patchlevel.h */
/*
 * Copyright (C) 2004-2006 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * This is the master file for version stamping purposes.  The
 * checked-in version will contain the correct version information at
 * all times.  Prior to an official release x.y.z,
 * KRB5_MAJOR_RELEASE=x, KRB5_MINOR_RELEASE=y, and KRB5_PATCHLEVEL=z.
 * KRB5_RELTAIL will reflect the release state.  It will be
 * "prerelease" for unreleased code either on the trunk or on a
 * release branch.  It will be undefined for a final release.
 *
 * Immediately following a final release, the release version numbers
 * will be incremented, and KRB5_RELTAIL will revert to "prerelease".
 *
 * KRB5_RELTAG contains the CVS tag name corresponding to the release.
 * KRB5_RELDATE identifies the date of the release.  They should
 * normally be undefined for checked-in code.
 */

/*
 * ==========
 * IMPORTANT:
 * ==========
 *
 * If you are a vendor supplying modified code derived from MIT
 * Kerberos, you SHOULD update KRB5_RELTAIL to identify your
 * organization.
 */
#define KRB5_MAJOR_RELEASE 1
#define KRB5_MINOR_RELEASE 21
#define KRB5_PATCHLEVEL 3
/* #undef KRB5_RELTAIL */
#define KRB5_RELDATE "20240626"
#define KRB5_RELTAG "krb5-1.21.3-final"
