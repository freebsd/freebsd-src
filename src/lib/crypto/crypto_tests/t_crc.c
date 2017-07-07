/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_crc.c */
/*
 * Copyright 2002,2005 by the Massachusetts Institute of Technology.
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
 * Sanity checks for CRC32.
 */
#include <sys/times.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crypto_int.h"

#define HEX 1
#define STR 2
struct crc_trial {
    int         type;
    char        *data;
    unsigned long       sum;
};

struct crc_trial trials[] = {
    {HEX, "01", 0x77073096},
    {HEX, "02", 0xee0e612c},
    {HEX, "04", 0x076dc419},
    {HEX, "08", 0x0edb8832},
    {HEX, "10", 0x1db71064},
    {HEX, "20", 0x3b6e20c8},
    {HEX, "40", 0x76dc4190},
    {HEX, "80", 0xedb88320},
    {HEX, "0100", 0x191b3141},
    {HEX, "0200", 0x32366282},
    {HEX, "0400", 0x646cc504},
    {HEX, "0800", 0xc8d98a08},
    {HEX, "1000", 0x4ac21251},
    {HEX, "2000", 0x958424a2},
    {HEX, "4000", 0xf0794f05},
    {HEX, "8000", 0x3b83984b},
    {HEX, "0001", 0x77073096},
    {HEX, "0002", 0xee0e612c},
    {HEX, "0004", 0x076dc419},
    {HEX, "0008", 0x0edb8832},
    {HEX, "0010", 0x1db71064},
    {HEX, "0020", 0x3b6e20c8},
    {HEX, "0040", 0x76dc4190},
    {HEX, "0080", 0xedb88320},
    {HEX, "01000000", 0xb8bc6765},
    {HEX, "02000000", 0xaa09c88b},
    {HEX, "04000000", 0x8f629757},
    {HEX, "08000000", 0xc5b428ef},
    {HEX, "10000000", 0x5019579f},
    {HEX, "20000000", 0xa032af3e},
    {HEX, "40000000", 0x9b14583d},
    {HEX, "80000000", 0xed59b63b},
    {HEX, "00010000", 0x01c26a37},
    {HEX, "00020000", 0x0384d46e},
    {HEX, "00040000", 0x0709a8dc},
    {HEX, "00080000", 0x0e1351b8},
    {HEX, "00100000", 0x1c26a370},
    {HEX, "00200000", 0x384d46e0},
    {HEX, "00400000", 0x709a8dc0},
    {HEX, "00800000", 0xe1351b80},
    {HEX, "00000100", 0x191b3141},
    {HEX, "00000200", 0x32366282},
    {HEX, "00000400", 0x646cc504},
    {HEX, "00000800", 0xc8d98a08},
    {HEX, "00001000", 0x4ac21251},
    {HEX, "00002000", 0x958424a2},
    {HEX, "00004000", 0xf0794f05},
    {HEX, "00008000", 0x3b83984b},
    {HEX, "00000001", 0x77073096},
    {HEX, "00000002", 0xee0e612c},
    {HEX, "00000004", 0x076dc419},
    {HEX, "00000008", 0x0edb8832},
    {HEX, "00000010", 0x1db71064},
    {HEX, "00000020", 0x3b6e20c8},
    {HEX, "00000040", 0x76dc4190},
    {HEX, "00000080", 0xedb88320},
    {STR, "foo", 0x7332bc33},
    {STR, "test0123456789", 0xb83e88d6},
    {STR, "MASSACHVSETTS INSTITVTE OF TECHNOLOGY", 0xe34180f7}
};

#define NTRIALS (sizeof(trials) / sizeof(trials[0]))

#if 0
static void
timetest(unsigned int nblk, unsigned int blksiz)
{
    char *block;
    unsigned int i;
    struct tms before, after;
    unsigned long cksum;

    block = malloc(blksiz * nblk);
    if (block == NULL)
        exit(1);
    for (i = 0; i < blksiz * nblk; i++)
        block[i] = i % 256;
    times(&before);
    for (i = 0; i < nblk; i++) {
        cksum = 0;
        mit_crc32(block + i * blksiz, blksiz, &cksum);
    }

    times(&after);
    printf("shift-8 implementation, %d blocks of %d bytes:\n",
           nblk, blksiz);
    printf("\tu=%ld s=%ld cu=%ld cs=%ld\n",
           (long)(after.tms_utime - before.tms_utime),
           (long)(after.tms_stime - before.tms_stime),
           (long)(after.tms_cutime - before.tms_cutime),
           (long)(after.tms_cstime - before.tms_cstime));

    free(block);
}
#endif

static void gethexstr(char *data, size_t *outlen, unsigned char *outbuf,
                      size_t buflen)
{
    size_t inlen;
    char *cp, buf[3];
    long n;

    inlen = strlen(data);
    *outlen = 0;
    for (cp = data; (size_t) (cp - data) < inlen; cp += 2) {
        strncpy(buf, cp, 2);
        buf[2] = '\0';
        n = strtol(buf, NULL, 16);
        outbuf[(*outlen)++] = n;
        if (*outlen > buflen)
            break;
    }
}

static void
verify(void)
{
    unsigned int i;
    struct crc_trial trial;
    unsigned char buf[4];
    size_t len;
    unsigned long cksum;
    char *typestr;

    for (i = 0; i < NTRIALS; i++) {
        trial = trials[i];
        switch (trial.type) {
        case STR:
            len = strlen(trial.data);
            typestr = "STR";
            cksum = 0;
            mit_crc32(trial.data, len, &cksum);
            break;
        case HEX:
            typestr = "HEX";
            gethexstr(trial.data, &len, buf, 4);
            cksum = 0;
            mit_crc32(buf, len, &cksum);
            break;
        default:
            typestr = "BOGUS";
            fprintf(stderr, "bad trial type %d\n", trial.type);
            exit(1);
        }
        printf("%s: %s \"%s\" = 0x%08lx\n",
               (trial.sum == cksum) ? "OK" : "***BAD***",
               typestr, trial.data, cksum);
    }
}

int
main(void)
{
#if 0
    timetest(64*1024, 1024);
#endif
    verify();
    exit(0);
}
