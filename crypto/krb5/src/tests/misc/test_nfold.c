/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int inlen, outlen, i;
    unsigned char *instr, *outstr;

    if (argc != 3) {
        fprintf(stderr, "%s: instr outlen\n", argv[0]);
        exit(1);
    }

    instr = (unsigned char *) argv[1];
    inlen = strlen(instr)*8;
    outlen = atoi(argv[2]);
    if (outlen%8) {
        fprintf(stderr, "outlen must be a multiple of 8\n");
        exit(1);
    }

    if ((outstr = (unsigned char *) malloc(outlen/8)) == NULL) {
        fprintf(stderr, "ENOMEM\n");
        exit(1);
    }

    krb5int_nfold(inlen,instr,outlen,outstr);

    printf("%d-fold(",outlen);
    for (i=0; i<(inlen/8); i++)
        printf("%02x",instr[i]);
    printf(") = ");
    for (i=0; i<(outlen/8); i++)
        printf("%02x",outstr[i]);
    printf("\n");

    exit(0);
}
