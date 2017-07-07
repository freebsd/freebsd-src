/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_oid.c - Test OID manipulation functions */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static struct {
    char *canonical;
    char *variant;
    gss_OID_desc oid;
} tests[] = {
    /* GSS_C_NT_USER_NAME */
    { "{ 1 2 840 113554 1 2 1 1 }", "1.2.840.113554.1.2.1.1",
      { 10, "\x2A\x86\x48\x86\xF7\x12\x01\x02\x01\x01" } },
    /* GSS_C_NT_MACHINE_UID_NAME */
    { "{ 1 2 840 113554 1 2 1 2 }", "1 2 840 113554 1 2 1 2",
      { 10, "\x2A\x86\x48\x86\xF7\x12\x01\x02\x01\x02" } },
    /* GSS_C_NT_STRING_UID_NAME */
    { "{ 1 2 840 113554 1 2 1 3 }", "{1 2 840 113554 1 2 1 3}",
      { 10, "\x2A\x86\x48\x86\xF7\x12\x01\x02\x01\x03" } },
    /* GSS_C_NT_HOSTBASED_SERVICE_X */
    { "{ 1 3 6 1 5 6 2 }", "{  1  3  6  1  5  6  2  }",
      { 6, "\x2B\x06\x01\x05\x06\x02" } },
    /* GSS_C_NT_ANONYMOUS */
    { "{ 1 3 6 1 5 6 3 }", "{ 01 03 06 01 05 06 03 }",
      { 6, "\x2B\x06\x01\x05\x06\x03" } },
    /* GSS_KRB5_NT_PRINCIPAL_NAME */
    { "{ 1 2 840 113554 1 2 2 1 }", " {01 2 840 113554 1 2 2 1  } ",
      { 10, "\x2A\x86\x48\x86\xF7\x12\x01\x02\x02\x01" } },
    /* gss_krb5_nt_principal */
    { "{ 1 2 840 113554 1 2 2 2 }", "{1.2.840.113554.1.2.2.2}",
      { 10, "\x2A\x86\x48\x86\xF7\x12\x01\x02\x02\x02" } },
    /* gss_mech_krb5 */
    { "{ 1 2 840 113554 1 2 2 }", "{ 1.2.840.113554.1.2.2 }",
      { 9, "\x2A\x86\x48\x86\xF7\x12\x01\x02\x02" } },
    /* gss_mech_krb5_old */
    { "{ 1 3 5 1 5 2 }", "001 . 003 . 005 . 001 . 005 . 002",
      { 5, "\x2B\x05\x01\x05\x02" } },
    /* gss_mech_krb5_wrong */
    { "{ 1 2 840 48018 1 2 2 }", "1.2.840.48018.1.2.2 trailing garbage",
      { 9, "\x2A\x86\x48\x82\xF7\x12\x01\x02\x02" } },
    /* gss_mech_iakerb */
    { "{ 1 3 6 1 5 2 5 }", "{ 1 3 6 1 5 2 5 } trailing garbage",
      { 6, "\x2B\x06\x01\x05\x02\x05" } },
    /* SPNEGO */
    { "{ 1 3 6 1 5 5 2 }", "{1 3 6 1 5 5 2} trailing garbage",
      { 6, "\x2B\x06\x01\x05\x05\x02" } },
    /* Edge cases for the first two arcs */
    { "{ 0 0 }", NULL, { 1, "\x00" } },
    { "{ 0 39 }", NULL, { 1, "\x27" } },
    { "{ 1 0 }", NULL, { 1, "\x28" } },
    { "{ 1 39 }", NULL, { 1, "\x4F" } },
    { "{ 2 0 }", NULL, { 1, "\x50" } },
    { "{ 2 40 }", NULL, { 1, "\x78" } },
    { "{ 2 47 }", NULL, { 1, "\x7F" } },
    { "{ 2 48 }", NULL, { 2, "\x81\x00" } },
    { "{ 2 16304 }", NULL, { 3, "\x81\x80\x00" } },
    /* Zero-valued arcs */
    { "{ 0 0 0 }", NULL, { 2, "\x00\x00" } },
    { "{ 0 0 1 0 }", NULL, { 3, "\x00\x01\x00" } },
    { "{ 0 0 128 0 }", NULL, { 4, "\x00\x81\x00\x00 " } },
    { "{ 0 0 0 1 }", NULL, { 3, "\x00\x00\x01" } },
    { "{ 0 0 128 0 1 0 128 }", NULL,
      { 8, "\x00\x81\x00\x00\x01\x00\x81\x00 " } }
};

static char *invalid_strings[] = {
    "",
    "{}",
    "{",
    "}",
    "  ",
    " { } ",
    "x",
    "+1 1",
    "-1.1",
    "1.+0",
    "+0.1",
    "{ 1 garbage }",
    "{ 1 }",
    "{ 0 40 }",
    "{ 1 40 }",
    "{ 1 128 }",
    "{ 1 1",
    "{ 1 2 3 4 +5 }",
    "{ 1.2.-3.4.5 }"
};

static int
oid_equal(gss_OID o1, gss_OID o2)
{
    return o1->length == o2->length &&
        memcmp(o1->elements, o2->elements, o1->length) == 0;
}

int
main()
{
    size_t i;
    OM_uint32 major, minor;
    gss_buffer_desc buf;
    gss_OID oid;
    gss_OID_set set;
    int status = 0, present;

    for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
        /* Check that this test's OID converts to its canonical string form. */
        major = gss_oid_to_str(&minor, &tests[i].oid, &buf);
        check_gsserr("gss_oid_to_str", major, minor);
        if (buf.length != strlen(tests[i].canonical) + 1 ||
            memcmp(buf.value, tests[i].canonical, buf.length) != 0) {
            status = 1;
            printf("test %d: OID converts to %.*s, wanted %s\n", (int)i,
                   (int)buf.length, (char *)buf.value, tests[i].canonical);
        }
        (void)gss_release_buffer(&minor, &buf);

        /* Check that this test's canonical string form converts to its OID. */
        buf.value = tests[i].canonical;
        buf.length = strlen(tests[i].canonical);
        major = gss_str_to_oid(&minor, &buf, &oid);
        check_gsserr("gss_str_to_oid", major, minor);
        if (!oid_equal(oid, &tests[i].oid)) {
            status = 1;
            printf("test %d: %s converts to wrong OID\n", (int)i,
                   tests[i].canonical);
            display_oid("wanted", &tests[i].oid);
            display_oid("actual", oid);
        }
        (void)gss_release_oid(&minor, &oid);

        /* Check that this test's variant string form converts to its OID. */
        if (tests[i].variant == NULL)
            continue;
        buf.value = tests[i].variant;
        buf.length = strlen(tests[i].variant);
        major = gss_str_to_oid(&minor, &buf, &oid);
        check_gsserr("gss_str_to_oid", major, minor);
        if (!oid_equal(oid, &tests[i].oid)) {
            status = 1;
            printf("test %d: %s converts to wrong OID\n", (int)i,
                   tests[i].variant);
            display_oid("wanted", &tests[i].oid);
            display_oid("actual", oid);
        }
        (void)gss_release_oid(&minor, &oid);
    }

    for (i = 0; i < sizeof(invalid_strings) / sizeof(*invalid_strings); i++) {
        buf.value = invalid_strings[i];
        buf.length = strlen(invalid_strings[i]);
        major = gss_str_to_oid(&minor, &buf, &oid);
        if (major == GSS_S_COMPLETE) {
            status = 1;
            printf("invalid %d: %s converted when it should not have\n",
                   (int)i, invalid_strings[i]);
            (void)gss_release_oid(&minor, &oid);
        }
    }

    major = gss_create_empty_oid_set(&minor, &set);
    check_gsserr("gss_create_empty_oid_set", major, minor);
    for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
        major = gss_add_oid_set_member(&minor, &tests[i].oid, &set);
        check_gsserr("gss_add_oid_set_member", major, minor);
    }
    if (set->count != i) {
        status = 1;
        printf("oid set has wrong size: wanted %d, actual %d\n", (int)i,
               (int)set->count);
    }
    for (i = 0; i < set->count; i++) {
        if (!oid_equal(&set->elements[i], &tests[i].oid)) {
            status = 1;
            printf("oid set has wrong element %d\n", (int)i);
            display_oid("wanted", &tests[i].oid);
            display_oid("actual", &set->elements[i]);
        }
        major = gss_test_oid_set_member(&minor, &tests[i].oid, set, &present);
        check_gsserr("gss_test_oid_set_member", major, minor);
        if (!present) {
            status = 1;
            printf("oid set does not contain OID %d\n", (int)i);
            display_oid("wanted", &tests[i].oid);
        }
    }
    (void)gss_release_oid_set(&minor, &set);
    return status;
}
