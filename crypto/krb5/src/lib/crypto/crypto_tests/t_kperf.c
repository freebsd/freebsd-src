/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_kperf.c */
/*
 * Copyright (C) 2009 by the Massachusetts Institute of Technology.
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
 * This file contains a harness to measure the performance improvement
 * of using the the krb5_k functions (which cache derived keys) over
 * the equivalent krb5_c functions which do not.  Sample usages:
 *
 *     ./t_kperf ce aes128-cts 10 100000
 *     ./t_kperf kv aes256-cts 1024 10000
 *
 * The first usage encrypts ('e') a hundred thousand ten-byte blobs
 * with aes128-cts, using the non-caching APIs ('c').  The second
 * usage verifies ('v') ten thousand checksums over 1K blobs with the
 * first available keyed checksum type for aes256-cts, using the
 * caching APIs ('k').  Run commands under "time" to measure how much
 * time is used by the operations.
 */

#include "k5-int.h"

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_keyblock kblock;
    krb5_key key;
    krb5_enctype enctype;
    krb5_cksumtype cktype;
    int blocksize, num_blocks, intf, op, i;
    size_t outlen, cklen;
    krb5_data block;
    krb5_enc_data outblock;
    krb5_checksum sum;
    krb5_boolean val;

    if (argc != 5) {
        fprintf(stderr, "Usage: t_kperf {c|k}{e|d|m|v} type size nblocks\n");
        exit(1);
    }
    intf = argv[1][0];
    assert(intf == 'c' || intf =='k');
    op = argv[1][1];
    ret = krb5_string_to_enctype(argv[2], &enctype);
    assert(!ret);
    blocksize = atoi(argv[3]);
    num_blocks = atoi(argv[4]);

    block.data = "notrandom";
    block.length = 9;
    krb5_c_random_seed(NULL, &block);

    krb5_c_make_random_key(NULL, enctype, &kblock);
    krb5_k_create_key(NULL, &kblock, &key);

    block.length = blocksize;
    block.data = calloc(1, blocksize);

    krb5_c_encrypt_length(NULL, enctype, blocksize, &outlen);
    outblock.enctype = enctype;
    outblock.ciphertext.length = outlen;
    outblock.ciphertext.data = calloc(1, outlen);

    krb5int_c_mandatory_cksumtype(NULL, enctype, &cktype);
    krb5_c_checksum_length(NULL, cktype, &cklen);
    sum.checksum_type = cktype;
    sum.length = cklen;
    sum.contents = calloc(1, cklen);

    /*
     * Decrypting typically involves copying the output after checking the
     * hash, so we need to create a valid ciphertext to correctly measure its
     * performance.
     */
    if (op == 'd')
        krb5_c_encrypt(NULL, &kblock, 0, NULL, &block, &outblock);

    for (i = 0; i < num_blocks; i++) {
        if (intf == 'c') {
            if (op == 'e')
                krb5_c_encrypt(NULL, &kblock, 0, NULL, &block, &outblock);
            else if (op == 'd')
                krb5_c_decrypt(NULL, &kblock, 0, NULL, &outblock, &block);
            else if (op == 'm')
                krb5_c_make_checksum(NULL, cktype, &kblock, 0, &block, &sum);
            else if (op == 'v')
                krb5_c_verify_checksum(NULL, &kblock, 0, &block, &sum, &val);
        } else {
            if (op == 'e')
                krb5_k_encrypt(NULL, key, 0, NULL, &block, &outblock);
            else if (op == 'd')
                krb5_k_decrypt(NULL, key, 0, NULL, &outblock, &block);
            else if (op == 'm')
                krb5_k_make_checksum(NULL, cktype, key, 0, &block, &sum);
            else if (op == 'v')
                krb5_k_verify_checksum(NULL, key, 0, &block, &sum, &val);
        }
    }
    return 0;
}
