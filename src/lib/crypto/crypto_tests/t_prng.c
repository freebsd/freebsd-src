/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_prng.c */
/*
 * Copyright (C) 2001 by the Massachusetts Institute of Technology.
 * All rights reserved.
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
 * This file contains tests for the PRNG code in Kerberos.  It reads
 * an input file, and writes an output file.  It is assumed that the
 * output file will be diffed against expected output to see whether
 * regression tests pass.  The input file has a very primitive format.
 * It is composed of alternating seeds and outputs.  The first line in
 * the file is an integer source id from the krb5_c_randsource enum in
 * krb5.h.  Then an integer seed length is read.  Then that many bytes
 * (encoded in hex) are read; whitespace or newlines may be inserted
 * between bytes.  Then after the seed data is an integer describing
 * how many bytes of output should be written.  Then another source ID
 * and seed length is read.  If the seed length is 0, the source id is
 * ignored and the seed is not seeded.
 */

#include "k5-int.h"
#include <assert.h>

int main () {
    krb5_error_code ret;
    krb5_data input, output;
    unsigned int source_id, seed_length;
    unsigned int i;
    while (1) {
        /* Read source*/
        if (scanf ("%u", &source_id ) == EOF )
            break;
        /* Read seed length*/
        if (scanf ("%u", &seed_length) == EOF)
            break;
        if (seed_length ) {
            unsigned int lc;
            ret = alloc_data(&input, seed_length);
            assert(!ret);
            for (lc = seed_length; lc > 0; lc--) {
                scanf ("%2x",  &i);
                input.data[seed_length-lc] = (unsigned) (i&0xff);
            }
            ret = krb5_c_random_add_entropy (0, source_id, &input);
            assert(!ret);
            free (input.data);
            input.data = NULL;
        }
        if (scanf ("%u", &i) == EOF)
            break;
        if (i) {
            ret = alloc_data(&output, i);
            assert(!ret);
            ret = krb5_c_random_make_octets (0, &output);
            if (ret)
                printf ("failed\n");
            else {
                for (; i > 0; i--) {
                    printf ("%02x",
                            (unsigned int) ((unsigned char ) output.data[output.length-i]));
                }
                printf ("\n");
            }
            free (output.data);
            output.data = NULL;
        }
    }
    return (0);
}
