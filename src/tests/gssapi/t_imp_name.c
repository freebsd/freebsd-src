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
#include <string.h>

#include "common.h"

static const char *
oid_str(char type)
{
    switch (type) {
    case 'p': /* GSS_KRB5_NT_PRINCIPAL_NAME */
        return "{ 1 2 840 113554 1 2 2 1 }";
    case 'e': /* GSS_KRB5_NT_ENTERPRISE_NAME */
        return "{ 1 2 840 113554 1 2 2 6 }";
    case 'c': /* GSS_KRB5_NT_X509_CERT */
        return "{ 1 2 840 113554 1 2 2 7 }";
    case 'h': /* GSS_C_NT_HOSTBASED_SERVICE */
        return "{ 1 2 840 113554 1 2 1 4 }";
    }
    return "no_oid";
}

/* Return true if buf has the same contents as str, plus a zero byte if
 * indicated by buf_includes_nullterm. */
static int
buf_eq_str(gss_buffer_t buf, const char *str, int buf_includes_nullterm)
{
    size_t len = strlen(str) + (buf_includes_nullterm ? 1 : 0);

    return (buf->length == len && memcmp(buf->value, str, len) == 0);
}

static void
test_import_name(const char *name)
{
    OM_uint32 major, minor;
    gss_name_t gss_name;
    gss_buffer_desc buf;
    gss_OID name_oid;

    gss_name = import_name(name);

    major = gss_display_name(&minor, gss_name, &buf, &name_oid);
    check_gsserr("gss_display_name", major, minor);
    if (!buf_eq_str(&buf, name + 2, 0))
        errout("wrong name string");
    (void)gss_release_buffer(&minor, &buf);

    major = gss_oid_to_str(&minor, name_oid, &buf);
    check_gsserr("gss_oid_to_str", major, minor);
    if (!buf_eq_str(&buf, oid_str(*name), 1))
        errout("wrong name type");
    (void)gss_release_buffer(&minor, &buf);
    (void)gss_release_name(&minor, &gss_name);
}

int
main(int argc, char **argv)
{
    test_import_name("p:user@MIT.EDU");
    test_import_name("e:enterprise@mit.edu@MIT.EDU");
    test_import_name("h:HOST@dc1.mit.edu");

    return 0;
}
