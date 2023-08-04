/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/des/t_verify.c */
/*
 * Copyright 1988, 1990 by the Massachusetts Institute of Technology.
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

/*
 *
 * Program to test the correctness of the DES library
 * implementation.
 *
 * exit returns  0 ==> success
 *              -1 ==> error
 */

#include "k5-int.h"
#include "des_int.h"
#include <stdio.h>
#include "com_err.h"

static void do_encrypt(unsigned char *, unsigned char *);
static void do_decrypt(unsigned char *, unsigned char *);

char *progname;
int nflag = 2;
int vflag;
int mflag;
int zflag;
int pid;
int mit_des_debug;

unsigned char cipher_text[64];
unsigned char clear_text[64] = "Now is the time for all " ;
unsigned char clear_text2[64] = "7654321 Now is the time for ";
unsigned char clear_text3[64] = {2,0,0,0, 1,0,0,0};
unsigned char output[64];
unsigned char zero_text[8] = {0x0,0,0,0,0,0,0,0};
unsigned char msb_text[8] = {0x0,0,0,0, 0,0,0,0x40}; /* to ANSI MSB */
unsigned char *input;

/* 0x0123456789abcdef */
unsigned char default_key[8] = {
    0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef
};
unsigned char key2[8] = { 0x08,0x19,0x2a,0x3b,0x4c,0x5d,0x6e,0x7f };
unsigned char key3[8] = { 0x80,1,1,1,1,1,1,1 };
mit_des_cblock s_key;
unsigned char default_ivec[8] = {
    0x12,0x34,0x56,0x78,0x90,0xab,0xcd,0xef
};
unsigned char *ivec;
unsigned char zero_key[8] = {1,1,1,1,1,1,1,1}; /* just parity bits */

unsigned char cipher1[8] = {
    0x25,0xdd,0xac,0x3e,0x96,0x17,0x64,0x67
};
unsigned char cipher2[8] = {
    0x3f,0xa4,0x0e,0x8a,0x98,0x4d,0x48,0x15
};
unsigned char cipher3[64] = {
    0xe5,0xc7,0xcd,0xde,0x87,0x2b,0xf2,0x7c,
    0x43,0xe9,0x34,0x00,0x8c,0x38,0x9c,0x0f,
    0x68,0x37,0x88,0x49,0x9a,0x7c,0x05,0xf6
};
unsigned char checksum[8] = {
    0x58,0xd2,0xe7,0x7e,0x86,0x06,0x27,0x33
};

unsigned char zresult[8] = {
    0x8c, 0xa6, 0x4d, 0xe9, 0xc1, 0xb1, 0x23, 0xa7
};

unsigned char mresult[8] = {
    0xa3, 0x80, 0xe0, 0x2a, 0x6b, 0xe5, 0x46, 0x96
};


/*
 * Can also add :
 * plaintext = 0, key = 0, cipher = 0x8ca64de9c1b123a7 (or is it a 1?)
 */

mit_des_key_schedule sched;

int
main(argc,argv)
    int argc;
    char *argv[];
{
    /* Local Declarations */
    size_t  in_length;
    int  retval;
    int i, j;

#ifdef WINDOWS
    /* Set screen window buffer to infinite size -- MS default is tiny.  */
    _wsetscreenbuf (fileno (stdout), _WINBUFINF);
#endif
    progname=argv[0];           /* salt away invoking program */

    while (--argc > 0 && (*++argv)[0] == '-')
        for (i=1; argv[0][i] != '\0'; i++) {
            switch (argv[0][i]) {

                /* debug flag */
            case 'd':
                mit_des_debug=3;
                continue;

            case 'z':
                zflag = 1;
                continue;

            case 'm':
                mflag = 1;
                continue;

            default:
                printf("%s: illegal flag \"%c\" ",
                       progname,argv[0][i]);
                exit(1);
            }
        };

    if (argc) {
        fprintf(stderr, "Usage: %s [-dmz]\n", progname);
        exit(1);
    }

    /* do some initialisation */

    /* use known input and key */

    /* ECB zero text zero key */
    if (zflag) {
        input = zero_text;
        mit_des_key_sched(zero_key, sched);
        printf("plaintext = key = 0, cipher = 0x8ca64de9c1b123a7\n");
        do_encrypt(input,cipher_text);
        printf("\tcipher  = (low to high bytes)\n\t\t");
        for (j = 0; j<=7; j++)
            printf("%02x ",cipher_text[j]);
        printf("\n");
        do_decrypt(output,cipher_text);
        if ( memcmp((char *)cipher_text, (char *)zresult, 8) ) {
            printf("verify: error in zero key test\n");
            exit(-1);
        }

        exit(0);
    }

    if (mflag) {
        input = msb_text;
        mit_des_key_sched(key3, sched);
        printf("plaintext = 0x00 00 00 00 00 00 00 40, ");
        printf("key = 0x80 01 01 01 01 01 01 01\n");
        printf("        cipher = 0xa380e02a6be54696\n");
        do_encrypt(input,cipher_text);
        printf("\tcipher  = (low to high bytes)\n\t\t");
        for (j = 0; j<=7; j++) {
            printf("%02x ",cipher_text[j]);
        }
        printf("\n");
        do_decrypt(output,cipher_text);
        if ( memcmp((char *)cipher_text, (char *)mresult, 8) ) {
            printf("verify: error in msb test\n");
            exit(-1);
        }
        exit(0);
    }

    /* ECB mode Davies and Price */
    {
        input = zero_text;
        mit_des_key_sched(key2, sched);
        printf("Examples per FIPS publication 81, keys ivs and cipher\n");
        printf("in hex.  These are the correct answers, see below for\n");
        printf("the actual answers.\n\n");
        printf("Examples per Davies and Price.\n\n");
        printf("EXAMPLE ECB\tkey = 08192a3b4c5d6e7f\n");
        printf("\tclear = 0\n");
        printf("\tcipher = 25 dd ac 3e 96 17 64 67\n");
        printf("ACTUAL ECB\n");
        printf("\tclear \"%s\"\n", input);
        do_encrypt(input,cipher_text);
        printf("\tcipher  = (low to high bytes)\n\t\t");
        for (j = 0; j<=7; j++)
            printf("%02x ",cipher_text[j]);
        printf("\n\n");
        do_decrypt(output,cipher_text);
        if ( memcmp((char *)cipher_text, (char *)cipher1, 8) ) {
            printf("verify: error in ECB encryption\n");
            exit(-1);
        }
        else
            printf("verify: ECB encryption is correct\n\n");
    }

    /* ECB mode */
    {
        mit_des_key_sched(default_key, sched);
        input = clear_text;
        ivec = default_ivec;
        printf("EXAMPLE ECB\tkey = 0123456789abcdef\n");
        printf("\tclear = \"Now is the time for all \"\n");
        printf("\tcipher = 3f a4 0e 8a 98 4d 48 15 ...\n");
        printf("ACTUAL ECB\n\tclear \"%s\"",input);
        do_encrypt(input,cipher_text);
        printf("\n\tcipher      = (low to high bytes)\n\t\t");
        for (j = 0; j<=7; j++) {
            printf("%02x ",cipher_text[j]);
        }
        printf("\n\n");
        do_decrypt(output,cipher_text);
        if ( memcmp((char *)cipher_text, (char *)cipher2, 8) ) {
            printf("verify: error in ECB encryption\n");
            exit(-1);
        }
        else
            printf("verify: ECB encryption is correct\n\n");
    }

    /* CBC mode */
    printf("EXAMPLE CBC\tkey = 0123456789abcdef");
    printf("\tiv = 1234567890abcdef\n");
    printf("\tclear = \"Now is the time for all \"\n");
    printf("\tcipher =\te5 c7 cd de 87 2b f2 7c\n");
    printf("\t\t\t43 e9 34 00 8c 38 9c 0f\n");
    printf("\t\t\t68 37 88 49 9a 7c 05 f6\n");

    printf("ACTUAL CBC\n\tclear \"%s\"\n",input);
    in_length =  strlen((char *)input);
    if ((retval = mit_des_cbc_encrypt((const mit_des_cblock *) input,
                                      (mit_des_cblock *) cipher_text,
                                      (size_t) in_length,
                                      sched,
                                      ivec,
                                      MIT_DES_ENCRYPT))) {
        com_err("des verify", retval, "can't encrypt");
        exit(-1);
    }
    printf("\tciphertext = (low to high bytes)\n");
    for (i = 0; i <= 2; i++) {
        printf("\t\t");
        for (j = 0; j <= 7; j++) {
            printf("%02x ",cipher_text[i*8+j]);
        }
        printf("\n");
    }
    if ((retval = mit_des_cbc_encrypt((const mit_des_cblock *) cipher_text,
                                      (mit_des_cblock *) clear_text,
                                      (size_t) in_length,
                                      sched,
                                      ivec,
                                      MIT_DES_DECRYPT))) {
        com_err("des verify", retval, "can't decrypt");
        exit(-1);
    }
    printf("\tdecrypted clear_text = \"%s\"\n",clear_text);

    if ( memcmp((char *)cipher_text, (char *)cipher3, in_length) ) {
        printf("verify: error in CBC encryption\n");
        exit(-1);
    }
    else
        printf("verify: CBC encryption is correct\n\n");

    printf("EXAMPLE CBC checksum");
    printf("\tkey =  0123456789abcdef\tiv =  1234567890abcdef\n");
    printf("\tclear =\t\t\"7654321 Now is the time for \"\n");
    printf("\tchecksum\t58 d2 e7 7e 86 06 27 33, ");
    printf("or some part thereof\n");
    input = clear_text2;
    mit_des_cbc_cksum(input,cipher_text, strlen((char *)input),
                      sched,ivec);
    printf("ACTUAL CBC checksum\n");
    printf("\t\tencrypted cksum = (low to high bytes)\n\t\t");
    for (j = 0; j<=7; j++)
        printf("%02x ",cipher_text[j]);
    printf("\n\n");
    if ( memcmp((char *)cipher_text, (char *)checksum, 8) ) {
        printf("verify: error in CBC checksum\n");
        exit(-1);
    }
    else
        printf("verify: CBC checksum is correct\n\n");

    exit(0);
}

static void
do_encrypt(in,out)
    unsigned char *in;
    unsigned char *out;
{
    int i, j;
    for (i =1; i<=nflag; i++) {
        mit_des_cbc_encrypt((const mit_des_cblock *)in,
                            (mit_des_cblock *)out,
                            8,
                            sched,
                            zero_text,
                            MIT_DES_ENCRYPT);
        if (mit_des_debug) {
            printf("\nclear %s\n",in);
            for (j = 0; j<=7; j++)
                printf("%02X ",in[j] & 0xff);
            printf("\tcipher ");
            for (j = 0; j<=7; j++)
                printf("%02X ",out[j] & 0xff);
        }
    }
}

static void
do_decrypt(in,out)
    unsigned char *out;
    unsigned char *in;
    /* try to invert it */
{
    int i, j;
    for (i =1; i<=nflag; i++) {
        mit_des_cbc_encrypt((const mit_des_cblock *)out,
                            (mit_des_cblock *)in,
                            8,
                            sched,
                            zero_text,
                            MIT_DES_DECRYPT);
        if (mit_des_debug) {
            printf("clear %s\n",in);
            for (j = 0; j<=7; j++)
                printf("%02X ",in[j] & 0xff);
            printf("\tcipher ");
            for (j = 0; j<=7; j++)
                printf("%02X ",out[j] & 0xff);
        }
    }
}

/*
 * Fake out the DES library, for the purposes of testing.
 */

int
mit_des_is_weak_key(key)
    mit_des_cblock key;
{
    return 0;                           /* fake it out for testing */
}
