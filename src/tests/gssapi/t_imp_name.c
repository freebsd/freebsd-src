/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1996, Massachusetts Institute of Technology.
 * All Rights Reserved.
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
 * Simple test program for testing how GSSAPI import name works.  (May
 * be made into a more full-fledged test program later.)
 */

#include <stdio.h>

#include "common.h"

int
main(int argc, char **argv)
{
    const char *name = "host@dcl.mit.edu";
    OM_uint32 major, minor;
    gss_name_t gss_name;
    gss_buffer_desc buf;
    gss_OID name_oid;

    gss_name = import_name(name);

    major = gss_display_name(&minor, gss_name, &buf, &name_oid);
    check_gsserr("gss_display_name", major, minor);
    printf("name is: %.*s\n", (int)buf.length, (char *)buf.value);
    (void)gss_release_buffer(&minor, &buf);

    major = gss_oid_to_str(&minor, name_oid, &buf);
    check_gsserr("gss_oid_to_str", major, minor);
    printf("name type is: %.*s\n", (int)buf.length, (char *)buf.value);
    (void)gss_release_buffer(&minor, &buf);
    (void)gss_release_name(&minor, &gss_name);

    return 0;
}
