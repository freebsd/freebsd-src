/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/hammer/pp.c */
/*
 * Copyright 1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * For copying and distribution information, please see the file
 * <krb5/copyright.h>.
 */

#include "krb5.h"

void
print_principal(p)
    krb5_principal  p;
{
    char    *buf;
    krb5_error_code retval;

    if (retval = krb5_unparse_name(p, &buf)) {
        com_err("DEBUG: Print_principal", retval,
                "while unparsing name");
        exit(1);
    }
    printf("%s\n", buf);
    free(buf);
}
