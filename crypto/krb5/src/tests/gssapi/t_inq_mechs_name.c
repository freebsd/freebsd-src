/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_inq_mechs_name.c - Exercise gss_inquire_mechs_for_name */
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

/*
 * Test program to exercise gss_inquire_mechs_for_name by importing a name and
 * reporting the mech OIDs which are reported as being able to process it.
 *
 * Usage: ./t_inq_mechs_name name
 */

#include <stdio.h>

#include "common.h"

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major;
    gss_name_t name;
    gss_OID_set mechs;
    size_t i;

    if (argc != 2) {
        fprintf(stderr, "Usage: t_inq_mechs_for_name name\n");
        return 1;
    }
    name = import_name(argv[1]);
    major = gss_inquire_mechs_for_name(&minor, name, &mechs);
    check_gsserr("gss_inquire_mechs_for_name", major, minor);
    for (i = 0; i < mechs->count; i++)
        display_oid(NULL, &mechs->elements[i]);
    (void)gss_release_oid_set(&minor, &mechs);
    (void)gss_release_name(&minor, &name);
    return 0;
}
