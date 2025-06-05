/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/verify/pkey.c */
/*
 * Copyright 1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * For copying and distribution information, please see the file
 * <krb5/copyright.h>.
 *
 */

#include <stdio.h>

void pkey(k)
    unsigned char *k;
{
    int i;
    unsigned int foo;

    for (i = 0 ; i < 8 ; i++) {
        foo = *k++;
        fprintf(stderr, "%x ", foo);
    }
}
