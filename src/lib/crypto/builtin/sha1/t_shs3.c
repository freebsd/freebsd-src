/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* test shs code */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "shs.h"

static void process(void);
static void test1(void);
static void test2(void);
static void test3(void);
static void test4(void);
static void test5(void);
static void test6(void);
static void test7(void);

/* When run on a little-endian CPU we need to perform byte reversal on an
   array of longwords.  It is possible to make the code endianness-
   independant by fiddling around with data at the byte level, but this
   makes for very slow code, so we rely on the user to sort out endianness
   at compile time */

static void longReverse( SHS_LONG *buffer, int byteCount )
{
    SHS_LONG value;
    static int init = 0;
    char *cp;

    switch (init) {
    case 0:
        init=1;
        cp = (char *) &init;
        if (*cp == 1) {
            init=2;
            break;
        }
        init=1;
        /* fall through - MSB */
    case 1:
        return;
    }

    byteCount /= sizeof( SHS_LONG );
    while( byteCount-- ) {
        value = *buffer;
        value = ( ( value & 0xFF00FF00L ) >> 8  ) |
            ( ( value & 0x00FF00FFL ) << 8 );
        *buffer++ = ( value << 16 ) | ( value >> 16 );
    }
}

int rc;
int mode;
int Dflag;

int
main(argc,argv)
    int argc;
    char **argv;
{
    char *argp;

    while (--argc > 0) if (*(argp = *++argv)=='-')
                           while (*++argp) switch(*argp)
                                           {
                                           case '1':
                                           case '2':
                                           case '3':
                                           case '4':
                                           case '5':
                                           case '6':
                                           case '7':
                                               if (mode) goto Usage;
                                               mode = *argp;
                                               break;
                                           case 'D':
                                               if (argc <= 1) goto Usage;
                                               --argc;
                                               Dflag = atoi(*++argv);
                                               break;
                                           case '-':
                                               break;
                                           default:
                                               fprintf (stderr,"Bad switch char <%c>\n", *argp);
                                           Usage:
                                               fprintf(stderr, "Usage: t_shs [-1234567] [-D #]\n");
                                               exit(1);
                                           }
        else goto Usage;

    process();
    exit(rc);
}

static void process(void)
{
    switch(mode)
    {
    case '1':
        test1();
        break;
    case '2':
        test2();
        break;
    case '3':
        test3();
        break;
    case '4':
        test4();
        break;
    case '5':
        test5();
        break;
    case '6':
        test6();
        break;
    case '7':
        test7();
        break;
    default:
        test1();
        test2();
        test3();
        test4();
        test5();
        test6();
        test7();
    }
}

#ifndef shsDigest
static unsigned char *
shsDigest(si)
    SHS_INFO *si;
{
    longReverse(si->digest, SHS_DIGESTSIZE);
    return (unsigned char*) si->digest;
}
#endif

unsigned char results1[SHS_DIGESTSIZE] = {
    0xa9,0x99,0x3e,0x36,0x47,0x06,0x81,0x6a,0xba,0x3e,
    0x25,0x71,0x78,0x50,0xc2,0x6c,0x9c,0xd0,0xd8,0x9d};

static void test1(void)
{
    SHS_INFO si[1];
    unsigned char digest[SHS_DIGESTSIZE];
    int failed;
    int i;

    printf("Running SHS test 1 ...\n");
    shsInit(si);
    shsUpdate(si, (SHS_BYTE *) "abc", 3);
    shsFinal(si);
    memcpy(digest, shsDigest(si), SHS_DIGESTSIZE);
    if ((failed = memcmp(digest, results1, SHS_DIGESTSIZE)) != 0)
    {
        fprintf(stderr,"SHS test 1 failed!\n");
        rc = 1;
    }
    printf ("%s, results = ", failed ? "Failed" : "Passed");
    for (i = 0; i < SHS_DIGESTSIZE; ++i)
        printf("%02x",digest[i]);
    if (failed)
    {
        printf ("\n, expected ");
        for (i = 0; i < SHS_DIGESTSIZE; ++i)
            printf("%02x",results1[i]);
    }
    printf("\n");
}

unsigned char results2[SHS_DIGESTSIZE] = {
    0x84,0x98,0x3e,0x44,0x1c,0x3b,0xd2,0x6e,0xba,0xae,
    0x4a,0xa1,0xf9,0x51,0x29,0xe5,0xe5,0x46,0x70,0xf1};

static void test2(void)
{
    SHS_INFO si[1];
    unsigned char digest[SHS_DIGESTSIZE];
    int failed;
    int i;

    printf("Running SHS test 2 ...\n");
    shsInit(si);
    shsUpdate(si,
              (SHS_BYTE *) "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
              56);
    shsFinal(si);
    memcpy(digest, shsDigest(si), SHS_DIGESTSIZE);
    if ((failed = memcmp(digest, results2, SHS_DIGESTSIZE)) != 0)
    {
        fprintf(stderr,"SHS test 2 failed!\n");
        rc = 1;
    }
    printf ("%s, results = ", failed ? "Failed" : "Passed");
    for (i = 0; i < SHS_DIGESTSIZE; ++i)
        printf("%02x",digest[i]);
    if (failed)
    {
        printf ("\n, expected ");
        for (i = 0; i < SHS_DIGESTSIZE; ++i)
            printf("%02x",results2[i]);
    }
    printf("\n");
}

unsigned char results3[SHS_DIGESTSIZE] = {
    0x34,0xaa,0x97,0x3c,0xd4,0xc4,0xda,0xa4,0xf6,0x1e,
    0xeb,0x2b,0xdb,0xad,0x27,0x31,0x65,0x34,0x01,0x6f};

static void test3(void)
{
    SHS_INFO si[1];
    unsigned char digest[SHS_DIGESTSIZE];
    int failed;
    int i;

    printf("Running SHS test 3 ...\n");
    shsInit(si);
    for (i = 0; i < 15625; ++i)
        shsUpdate(si,
                  (SHS_BYTE *) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                  64);
    shsFinal(si);
    memcpy(digest, shsDigest(si), SHS_DIGESTSIZE);
    if ((failed = memcmp(digest, results3, SHS_DIGESTSIZE)) != 0)
    {
        fprintf(stderr,"SHS test 3 failed!\n");
        rc = 1;
    }
    printf ("%s, results = ", failed ? "Failed" : "Passed");
    for (i = 0; i < SHS_DIGESTSIZE; ++i)
        printf("%02x",digest[i]);
    if (failed)
    {
        printf ("\n, expected ");
        for (i = 0; i < SHS_DIGESTSIZE; ++i)
            printf("%02x",results3[i]);
    }
    printf("\n");
}

unsigned char randdata[] = {
    0xfe,0x28,0x79,0x25,0xf5,0x03,0xf9,0x1c,0xcd,0x70,0x7b,0xb0,0x42,0x02,0xb8,0x2f,
    0xf3,0x63,0xa2,0x79,0x8e,0x9b,0x33,0xd7,0x2b,0xc4,0xb4,0xd2,0xcb,0x61,0xec,0xbb,
    0x94,0xe1,0x8f,0x53,0x80,0x55,0xd9,0x90,0xb2,0x03,0x58,0xfa,0xa6,0xe5,0x18,0x57,
    0x68,0x04,0x24,0x98,0x41,0x7e,0x84,0xeb,0xc1,0x39,0xbc,0x1d,0xf7,0x4e,0x92,0x72,
    0x1a,0x5b,0xb6,0x99,0x43,0xa5,0x0a,0x45,0x73,0x55,0xfd,0x57,0x83,0x45,0x36,0x5c,
    0xfd,0x39,0x08,0x6e,0xe2,0x01,0x9a,0x8c,0x4e,0x39,0xd2,0x0d,0x5f,0x0e,0x35,0x15,
    0xb9,0xac,0x5f,0xa1,0x8a,0xe6,0xdd,0x6e,0x68,0x9d,0xf6,0x29,0x95,0xf6,0x7d,0x7b,
    0xd9,0x5e,0xf4,0x67,0x25,0xbd,0xee,0xed,0x53,0x60,0xb0,0x47,0xdf,0xef,0xf4,0x41,
    0xbd,0x45,0xcf,0x5c,0x93,0x41,0x87,0x97,0x82,0x39,0x20,0x66,0xb4,0xda,0xcb,0x66,
    0x93,0x02,0x2e,0x7f,0x94,0x4c,0xc7,0x3b,0x2c,0xcf,0xf6,0x99,0x6f,0x13,0xf1,0xc5,
    0x28,0x2b,0xa6,0x6c,0x39,0x26,0x7f,0x76,0x24,0x4a,0x6e,0x01,0x40,0x63,0xf8,0x00,
    0x06,0x23,0x5a,0xaa,0xa6,0x2f,0xd1,0x37,0xc7,0xcc,0x76,0xe9,0x54,0x1e,0x57,0x73,
    0xf5,0x33,0xaa,0x96,0xbe,0x35,0xcd,0x1d,0xd5,0x7d,0xac,0x50,0xd5,0xf8,0x47,0x2d,
    0xd6,0x93,0x5f,0x6e,0x38,0xd3,0xac,0xd0,0x7e,0xad,0x9e,0xf8,0x87,0x95,0x63,0x15,
    0x65,0xa3,0xd4,0xb3,0x9a,0x6c,0xac,0xcd,0x2a,0x54,0x83,0x13,0xc4,0xb4,0x94,0xfa,
    0x76,0x87,0xc5,0x8b,0x4a,0x10,0x92,0x05,0xd1,0x0e,0x97,0xfd,0xc8,0xfb,0xc5,0xdc,
    0x21,0x4c,0xc8,0x77,0x5c,0xed,0x32,0x22,0x77,0xc1,0x38,0x30,0xd7,0x8e,0x2a,0x70,
    0x72,0x67,0x13,0xe4,0xb7,0x18,0xd4,0x76,0xdd,0x32,0x12,0xf4,0x5d,0xc9,0xec,0xc1,
    0x2c,0x8a,0xfe,0x08,0x6c,0xea,0xf6,0xab,0x5a,0x0e,0x8e,0x81,0x1d,0xc8,0x5a,0x4b,
    0xed,0xb9,0x7f,0x4b,0x67,0xe3,0x65,0x46,0xc9,0xf2,0xab,0x37,0x0a,0x98,0x67,0x5b,
    0xb1,0x3b,0x02,0x91,0x38,0x71,0xea,0x62,0x88,0xae,0xb6,0xdb,0xfc,0x55,0x79,0x33,
    0x69,0x95,0x51,0xb6,0xe1,0x3b,0xab,0x22,0x68,0x54,0xf9,0x89,0x9c,0x94,0xe0,0xe3,
    0xd3,0x48,0x5c,0xe9,0x78,0x5b,0xb3,0x4b,0xba,0xd8,0x48,0xd8,0xaf,0x91,0x4e,0x23,
    0x38,0x23,0x23,0x6c,0xdf,0x2e,0xf0,0xff,0xac,0x1d,0x2d,0x27,0x10,0x45,0xa3,0x2d,
    0x8b,0x00,0xcd,0xe2,0xfc,0xb7,0xdb,0x52,0x13,0xb7,0x66,0x79,0xd9,0xd8,0x29,0x0e,
    0x32,0xbd,0x52,0x6b,0x75,0x71,0x08,0x83,0x1b,0x67,0x28,0x93,0x97,0x97,0x32,0xff,
    0x8b,0xd3,0x98,0xa3,0xce,0x2b,0x88,0x37,0x1c,0xcc,0xa0,0xd1,0x19,0x9b,0xe6,0x11,
    0xfc,0xc0,0x3c,0x4e,0xe1,0x35,0x49,0x29,0x19,0xcf,0x1d,0xe1,0x60,0x74,0xc0,0xe9,
    0xf7,0xb4,0x99,0xa0,0x23,0x50,0x51,0x78,0xcf,0xc0,0xe5,0xc2,0x1c,0x16,0xd2,0x24,
    0x5a,0x63,0x54,0x83,0xaa,0x74,0x3d,0x41,0x0d,0x52,0xee,0xfe,0x0f,0x4d,0x13,0xe1,
    0x27,0x00,0xc4,0xf3,0x2b,0x55,0xe0,0x9c,0x81,0xe0,0xfc,0xc2,0x13,0xd4,0x39,0x09
};

unsigned char results4[SHS_DIGESTSIZE] = {
    0x13,0x62,0xfc,0x87,0x68,0x33,0xd5,0x1d,0x2f,0x0c,
    0x73,0xe3,0xfb,0x87,0x6a,0x6b,0xc3,0x25,0x54,0xfc};

static void test4(void)
{
    SHS_INFO si[1];
    unsigned char digest[SHS_DIGESTSIZE];
    int failed;
    int i;

    printf("Running SHS test 4 ...\n");
    shsInit(si);
    shsUpdate(si, randdata, 19);
    shsFinal(si);
    memcpy(digest, shsDigest(si), SHS_DIGESTSIZE);
    if ((failed = memcmp(digest, results4, SHS_DIGESTSIZE)) != 0)
    {
        fprintf(stderr,"SHS test 4 failed!\n");
        rc = 1;
    }
    printf ("%s, results = ", failed ? "Failed" : "Passed");
    for (i = 0; i < SHS_DIGESTSIZE; ++i)
        printf("%02x",digest[i]);
    if (failed)
    {
        printf ("\n, expected ");
        for (i = 0; i < SHS_DIGESTSIZE; ++i)
            printf("%02x",results4[i]);
    }
    printf("\n");
}

unsigned char results5[SHS_DIGESTSIZE] = {
    0x19,0x4d,0xf6,0xeb,0x8e,0x02,0x6d,0x37,0x58,0x64,
    0xe5,0x95,0x19,0x2a,0xdd,0x1c,0xc4,0x3c,0x24,0x86};

static void test5(void)
{
    SHS_INFO si[1];
    unsigned char digest[SHS_DIGESTSIZE];
    int failed;
    int i;

    printf("Running SHS test 5 ...\n");
    shsInit(si);
    shsUpdate(si, randdata, 19);
    shsUpdate(si, randdata+32, 15);
    shsFinal(si);
    memcpy(digest, shsDigest(si), SHS_DIGESTSIZE);
    if ((failed = memcmp(digest, results5, SHS_DIGESTSIZE)) != 0)
    {
        fprintf(stderr,"SHS test 5 failed!\n");
        rc = 1;
    }
    printf ("%s, results = ", failed ? "Failed" : "Passed");
    for (i = 0; i < SHS_DIGESTSIZE; ++i)
        printf("%02x",digest[i]);
    if (failed)
    {
        printf ("\n, expected ");
        for (i = 0; i < SHS_DIGESTSIZE; ++i)
            printf("%02x",results5[i]);
    }
    printf("\n");
}

unsigned char results6[SHS_DIGESTSIZE] = {
    0x4e,0x16,0x57,0x9d,0x4b,0x48,0xa9,0x1c,0x88,0x72,
    0x83,0xdb,0x88,0xd1,0xea,0x3a,0x45,0xdf,0xa1,0x10};

static void test6(void)
{
    struct {
        unsigned long pad1;
        SHS_INFO si1;
        unsigned long pad2;
        SHS_INFO si2;
        unsigned long pad3;
    } sdata;
    unsigned char digest[SHS_DIGESTSIZE];
    int failed;
    unsigned int i, j;

    printf("Running SHS test 6 ...\n");
    sdata.pad1 = 0x12345678;
    sdata.pad2 = 0x87654321;
    sdata.pad3 = 0x78563412;
    shsInit((&sdata.si2));
    if (sdata.pad2 != 0x87654321) {
        printf ("Overrun #20 %#lx\n",
                sdata.pad2);
        sdata.pad2 = 0x87654321;
    }
    if (sdata.pad3 != 0x78563412) {
        printf ("Overrun #21 %#lx\n",
                sdata.pad3);
        sdata.pad3 = 0x78563412;
    }
    for (i = 0; i < 400; ++i)
    {
        shsInit(&sdata.si1);
        if (sdata.pad1 != 0x12345678) {
            printf ("Overrun #22 %#lx at %d\n",
                    sdata.pad1, i);
            sdata.pad1 = 0x12345678;
        }
        if (sdata.pad2 != 0x87654321) {
            printf ("Overrun #23 %#lx at %d\n",
                    sdata.pad2, i);
            sdata.pad2 = 0x87654321;
        }
        shsUpdate(&sdata.si1, (randdata+sizeof(randdata))-i, i);
        if (sdata.pad1 != 0x12345678) {
            printf ("Overrun #24 %#lx at %d\n",
                    sdata.pad1, i);
            sdata.pad1 = 0x12345678;
        }
        if (sdata.pad2 != 0x87654321) {
            printf ("Overrun #25 %#lx at %d\n",
                    sdata.pad2, i);
            sdata.pad2 = 0x87654321;
        }
        shsFinal(&sdata.si1);
        if (sdata.pad1 != 0x12345678) {
            printf ("Overrun #26 %#lx at %d\n",
                    sdata.pad1, i);
            sdata.pad1 = 0x12345678;
        }
        if (sdata.pad2 != 0x87654321) {
            printf ("Overrun #27 %#lx at %d\n",
                    sdata.pad2, i);
            sdata.pad2 = 0x87654321;
        }
        memcpy(digest, shsDigest(&sdata.si1), SHS_DIGESTSIZE);
        if (Dflag & 1)
        {
            printf ("%d: ", i);
            for (j = 0; j < SHS_DIGESTSIZE; ++j)
                printf("%02x",digest[j]);
            printf("\n");
        }
        shsUpdate((&sdata.si2), digest, SHS_DIGESTSIZE);
        if (sdata.pad2 != 0x87654321) {
            printf ("Overrun #28 %#lx at %d\n",
                    sdata.pad2, i);
            sdata.pad2 = 0x87654321;
        }
        if (sdata.pad3 != 0x78563412) {
            printf ("Overrun #29 %#lx at %d\n",
                    sdata.pad3, i);
            sdata.pad3 = 0x78563412;
        }
        if (Dflag & 2)
            printf ("%d: %08lx%08lx%08lx%08lx%08lx\n",
                    i,
                    (unsigned long) sdata.si2.digest[0],
                    (unsigned long) sdata.si2.digest[1],
                    (unsigned long) sdata.si2.digest[2],
                    (unsigned long) sdata.si2.digest[3],
                    (unsigned long) sdata.si2.digest[4]);
    }
    shsFinal((&sdata.si2));
    if (sdata.pad2 != 0x87654321) {
        printf ("Overrun #30 %#lx\n",
                sdata.pad2);
        sdata.pad2 = 0x87654321;
    }
    if (sdata.pad3 != 0x78563412) {
        printf ("Overrun #31 %#lx\n",
                sdata.pad3);
        sdata.pad3 = 0x78563412;
    }
    memcpy(digest, shsDigest((&sdata.si2)), SHS_DIGESTSIZE);
    if ((failed = memcmp(digest, results6, SHS_DIGESTSIZE)) != 0)
    {
        fprintf(stderr,"SHS test 6 failed!\n");
        rc = 1;
    }
    printf ("%s, results = ", failed ? "Failed" : "Passed");
    for (i = 0; i < SHS_DIGESTSIZE; ++i)
        printf("%02x",digest[i]);
    if (failed)
    {
        printf ("\n, expected ");
        for (i = 0; i < SHS_DIGESTSIZE; ++i)
            printf("%02x",results6[i]);
    }
    printf("\n");
}

unsigned char results7[SHS_DIGESTSIZE] = {
    0x89,0x41,0x65,0xce,0x76,0xc1,0xd1,0xd1,0xc3,0x6f,
    0xab,0x92,0x79,0x30,0x01,0x71,0x63,0x1f,0x74,0xfe};

unsigned int jfsize[] = {0,1,31,32,
                         33,55,56,63,
                         64,65,71,72,
                         73,95,96,97,
                         119,120,123,127};
unsigned int kfsize[] = {0,1,31,32,33,55,56,63};

static void test7(void)
{
    struct {
        unsigned long pad1;
        SHS_INFO si1;
        unsigned long pad2;
        SHS_INFO si2;
        unsigned long pad3;
    } sdata;
    unsigned char digest[SHS_DIGESTSIZE];
    int failed;
    unsigned int i, j, k, l;

    printf("Running SHS test 7 ...\n");
    sdata.pad1 = 0x12345678;
    sdata.pad2 = 0x87654321;
    sdata.pad3 = 0x78563412;
    shsInit((&sdata.si2));
    for (i = 1; i <= 128; ++i)
        for (j = 0; j < 20; ++j)
            for (k = 0; k < 8; ++k)
            {
                shsInit(&sdata.si1);
                shsUpdate(&sdata.si1, (randdata+80+j), i);
                if (sdata.pad1 != 0x12345678) {
                    printf ("Overrun #1 %#lx at %d,%d,%d\n",
                            sdata.pad1, i,j,k);
                    sdata.pad1 = 0x12345678;
                }
                if (sdata.pad2 != 0x87654321) {
                    printf ("Overrun #2 %#lx at %d,%d,%d\n",
                            sdata.pad2, i,j,k);
                    sdata.pad2 = 0x87654321;
                }
                shsUpdate(&sdata.si1, randdata+i, jfsize[j]);
                if (sdata.pad1 != 0x12345678) {
                    printf ("Overrun #3 %#lx at %d,%d,%d\n",
                            sdata.pad1, i,j,k);
                    sdata.pad1 = 0x12345678;
                }
                if (sdata.pad2 != 0x87654321) {
                    printf ("Overrun #4 %#lx at %d,%d,%d\n",
                            sdata.pad2, i,j,k);
                    sdata.pad2 = 0x87654321;
                }
                if (k) shsUpdate(&sdata.si1, randdata+(i^j), kfsize[k]);
                if (sdata.pad1 != 0x12345678) {
                    printf ("Overrun #5 %#lx at %d,%d,%d\n",
                            sdata.pad1, i,j,k);
                    sdata.pad1 = 0x12345678;
                }
                if (sdata.pad2 != 0x87654321) {
                    printf ("Overrun #6 %#lx at %d,%d,%d\n",
                            sdata.pad2, i,j,k);
                    sdata.pad2 = 0x87654321;
                }
                shsFinal(&sdata.si1);
                if (sdata.pad1 != 0x12345678) {
                    printf ("Overrun #7 %#lx at %d,%d,%d\n",
                            sdata.pad1, i,j,k);
                    sdata.pad1 = 0x12345678;
                }
                if (sdata.pad2 != 0x87654321) {
                    printf ("Overrun #8 %#lx at %d,%d,%d\n",
                            sdata.pad2, i,j,k);
                    sdata.pad2 = 0x87654321;
                }
                memcpy(digest, shsDigest(&sdata.si1), SHS_DIGESTSIZE);
                if (Dflag & 1)
                {
                    printf ("%d,%d,%d: ", i, j, k);
                    for (l = 0; l < SHS_DIGESTSIZE; ++l)
                        printf("%02x",digest[l]);
                    printf("\n");
                }
                shsUpdate((&sdata.si2), digest, SHS_DIGESTSIZE);
                if (sdata.pad2 != 0x87654321) {
                    printf ("Overrun #9 %#lx at %d,%d,%d\n",
                            sdata.pad2, i,j,k);
                    sdata.pad2 = 0x87654321;
                }
                if (sdata.pad3 != 0x78563412) {
                    printf ("Overrun #10 %#lx at %d,%d,%d\n",
                            sdata.pad3, i,j,k);
                    sdata.pad3 = 0x78563412;
                }
                if (Dflag & 2)
                    printf ("%d,%d,%d: %08lx%08lx%08lx%08lx%08lx\n",
                            i,j,k,
                            (unsigned long) sdata.si2.digest[0],
                            (unsigned long) sdata.si2.digest[1],
                            (unsigned long) sdata.si2.digest[2],
                            (unsigned long) sdata.si2.digest[3],
                            (unsigned long) sdata.si2.digest[4]);
            }
    shsFinal((&sdata.si2));
    memcpy(digest, shsDigest((&sdata.si2)), SHS_DIGESTSIZE);
    if ((failed = memcmp(digest, results7, SHS_DIGESTSIZE)) != 0)
    {
        fprintf(stderr,"SHS test 7 failed!\n");
        rc = 1;
    }
    printf ("%s, results = ", failed ? "Failed" : "Passed");
    for (i = 0; i < SHS_DIGESTSIZE; ++i)
        printf("%02x",digest[i]);
    if (failed)
    {
        printf ("\n, expected ");
        for (i = 0; i < SHS_DIGESTSIZE; ++i)
            printf("%02x",results7[i]);
    }
    printf("\n");
}
