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

/*
 * RFC 3961 and AFS string to key.  These are not standard crypto primitives
 * (RFC 3961 string-to-key is implemented in OpenSSL for historical reasons but
 * it doesn't get weak keys right), so we have to implement them here.
 */

#include <ctype.h>
#include "crypto_int.h"

#undef min
#define min(a,b) ((a)>(b)?(b):(a))

/* Compute a CBC checksum of in (with length len) using the specified key and
 * ivec.  The result is written into out. */
static krb5_error_code
des_cbc_mac(const unsigned char *keybits, const unsigned char *ivec,
            const unsigned char *in, size_t len, unsigned char *out)
{
    krb5_error_code ret;
    krb5_keyblock kb;
    krb5_key key;
    krb5_crypto_iov iov[2];
    unsigned char zero[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    krb5_data outd, ivecd;

    /* Make a key from keybits. */
    kb.magic = KV5M_KEYBLOCK;
    kb.enctype = ENCTYPE_DES_CBC_CRC;
    kb.length = 8;
    kb.contents = (unsigned char *)keybits;
    ret = krb5_k_create_key(NULL, &kb, &key);
    if (ret)
        return ret;

    /* Make iovs for the input data, padding it out to the block size. */
    iov[0].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[0].data = make_data((unsigned char *)in, len);
    iov[1].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[1].data = make_data(zero, krb5_roundup(len, 8) - len);

    /* Make krb5_data structures for the ivec and output. */
    ivecd = make_data((unsigned char *)ivec, 8);
    outd = make_data(out, 8);

    /* Call the cbc_mac operation of the module's DES enc-provider. */
    ret = krb5int_enc_des.cbc_mac(key, iov, 2, &ivecd, &outd);
    krb5_k_free_key(NULL, key);
    return ret;
}

/*** AFS string-to-key constants ***/

/* Initial permutation */
static const char IP[] = {
    58,50,42,34,26,18,10, 2,
    60,52,44,36,28,20,12, 4,
    62,54,46,38,30,22,14, 6,
    64,56,48,40,32,24,16, 8,
    57,49,41,33,25,17, 9, 1,
    59,51,43,35,27,19,11, 3,
    61,53,45,37,29,21,13, 5,
    63,55,47,39,31,23,15, 7,
};

/* Final permutation, FP = IP^(-1) */
static const char FP[] = {
    40, 8,48,16,56,24,64,32,
    39, 7,47,15,55,23,63,31,
    38, 6,46,14,54,22,62,30,
    37, 5,45,13,53,21,61,29,
    36, 4,44,12,52,20,60,28,
    35, 3,43,11,51,19,59,27,
    34, 2,42,10,50,18,58,26,
    33, 1,41, 9,49,17,57,25,
};

/*
 * Permuted-choice 1 from the key bits to yield C and D.
 * Note that bits 8,16... are left out: They are intended for a parity check.
 */
static const char PC1_C[] = {
    57,49,41,33,25,17, 9,
    1,58,50,42,34,26,18,
    10, 2,59,51,43,35,27,
    19,11, 3,60,52,44,36,
};

static const char PC1_D[] = {
    63,55,47,39,31,23,15,
    7,62,54,46,38,30,22,
    14, 6,61,53,45,37,29,
    21,13, 5,28,20,12, 4,
};

/* Sequence of shifts used for the key schedule */
static const char shifts[] = {
    1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1,
};

/* Permuted-choice 2, to pick out the bits from the CD array that generate the
 * key schedule */
static const char PC2_C[] = {
    14,17,11,24, 1, 5,
    3,28,15, 6,21,10,
    23,19,12, 4,26, 8,
    16, 7,27,20,13, 2,
};

static const char PC2_D[] = {
    41,52,31,37,47,55,
    30,40,51,45,33,48,
    44,49,39,56,34,53,
    46,42,50,36,29,32,
};

/* The E bit-selection table */
static const char e[] = {
    32, 1, 2, 3, 4, 5,
    4, 5, 6, 7, 8, 9,
    8, 9,10,11,12,13,
    12,13,14,15,16,17,
    16,17,18,19,20,21,
    20,21,22,23,24,25,
    24,25,26,27,28,29,
    28,29,30,31,32, 1,
};

/* P is a permutation on the selected combination of the current L and key. */
static const char P[] = {
    16, 7,20,21,
    29,12,28,17,
    1,15,23,26,
    5,18,31,10,
    2, 8,24,14,
    32,27, 3, 9,
    19,13,30, 6,
    22,11, 4,25,
};

/*
 * The 8 selection functions.
 * For some reason, they give a 0-origin
 * index, unlike everything else.
 */
static const char S[8][64] = {
    {14, 4,13, 1, 2,15,11, 8, 3,10, 6,12, 5, 9, 0, 7,
     0,15, 7, 4,14, 2,13, 1,10, 6,12,11, 9, 5, 3, 8,
     4, 1,14, 8,13, 6, 2,11,15,12, 9, 7, 3,10, 5, 0,
     15,12, 8, 2, 4, 9, 1, 7, 5,11, 3,14,10, 0, 6,13},

    {15, 1, 8,14, 6,11, 3, 4, 9, 7, 2,13,12, 0, 5,10,
     3,13, 4, 7,15, 2, 8,14,12, 0, 1,10, 6, 9,11, 5,
     0,14, 7,11,10, 4,13, 1, 5, 8,12, 6, 9, 3, 2,15,
     13, 8,10, 1, 3,15, 4, 2,11, 6, 7,12, 0, 5,14, 9},

    {10, 0, 9,14, 6, 3,15, 5, 1,13,12, 7,11, 4, 2, 8,
     13, 7, 0, 9, 3, 4, 6,10, 2, 8, 5,14,12,11,15, 1,
     13, 6, 4, 9, 8,15, 3, 0,11, 1, 2,12, 5,10,14, 7,
     1,10,13, 0, 6, 9, 8, 7, 4,15,14, 3,11, 5, 2,12},

    { 7,13,14, 3, 0, 6, 9,10, 1, 2, 8, 5,11,12, 4,15,
      13, 8,11, 5, 6,15, 0, 3, 4, 7, 2,12, 1,10,14, 9,
      10, 6, 9, 0,12,11, 7,13,15, 1, 3,14, 5, 2, 8, 4,
      3,15, 0, 6,10, 1,13, 8, 9, 4, 5,11,12, 7, 2,14},

    { 2,12, 4, 1, 7,10,11, 6, 8, 5, 3,15,13, 0,14, 9,
      14,11, 2,12, 4, 7,13, 1, 5, 0,15,10, 3, 9, 8, 6,
      4, 2, 1,11,10,13, 7, 8,15, 9,12, 5, 6, 3, 0,14,
      11, 8,12, 7, 1,14, 2,13, 6,15, 0, 9,10, 4, 5, 3},

    {12, 1,10,15, 9, 2, 6, 8, 0,13, 3, 4,14, 7, 5,11,
     10,15, 4, 2, 7,12, 9, 5, 6, 1,13,14, 0,11, 3, 8,
     9,14,15, 5, 2, 8,12, 3, 7, 0, 4,10, 1,13,11, 6,
     4, 3, 2,12, 9, 5,15,10,11,14, 1, 7, 6, 0, 8,13},

    { 4,11, 2,14,15, 0, 8,13, 3,12, 9, 7, 5,10, 6, 1,
      13, 0,11, 7, 4, 9, 1,10,14, 3, 5,12, 2,15, 8, 6,
      1, 4,11,13,12, 3, 7,14,10,15, 6, 8, 0, 5, 9, 2,
      6,11,13, 8, 1, 4,10, 7, 9, 5, 0,15,14, 2, 3,12},

    {13, 2, 8, 4, 6,15,11, 1,10, 9, 3,14, 5, 0,12, 7,
     1,15,13, 8,10, 3, 7, 4,12, 5, 6,11, 0,14, 9, 2,
     7,11, 4, 1, 9,12,14, 2, 0, 6,10,13,15, 3, 5, 8,
     2, 1,14, 7, 4,10, 8,13,15,12, 9, 0, 3, 5, 6,11},
};


/* Set up the key schedule from the key. */
static void
afs_crypt_setkey(char *key, char *E, char (*KS)[48])
{
    int i, j, k, t;
    char C[28], D[28];          /* Used to calculate key schedule. */

    /*
     * First, generate C and D by permuting
     * the key.  The low order bit of each
     * 8-bit char is not used, so C and D are only 28
     * bits apiece.
     */
    for (i = 0; i < 28; i++) {
        C[i] = key[PC1_C[i] - 1];
        D[i] = key[PC1_D[i] - 1];
    }
    /*
     * To generate Ki, rotate C and D according
     * to schedule and pick up a permutation
     * using PC2.
     */
    for (i = 0; i < 16; i++) {
        /* Rotate. */
        for (k = 0; k < shifts[i]; k++) {
            t = C[0];
            for (j = 0; j < 28 - 1; j++)
                C[j] = C[j + 1];
            C[27] = t;
            t = D[0];
            for (j = 0; j < 28 - 1; j++)
                D[j] = D[j + 1];
            D[27] = t;
        }
        /* Get Ki.  Note C and D are concatenated. */
        for (j = 0; j < 24; j++) {
            KS[i][j] = C[PC2_C[j]-1];
            KS[i][j+24] = D[PC2_D[j]-28-1];
        }
    }

    memcpy(E, e, 48);
}

/*
 * The payoff: encrypt a block.
 */

static void
afs_encrypt_block(char *block, char *E, char (*KS)[48])
{
    const long edflag = 0;
    int i, ii;
    int t, j, k;
    char tempL[32];
    char f[32];
    char L[64];                 /* Current block divided into two halves */
    char *const R = &L[32];
    /* The combination of the key and the input, before selection. */
    char preS[48];

    /* First, permute the bits in the input. */
    for (j = 0; j < 64; j++)
        L[j] = block[IP[j] - 1];
    /* Perform an encryption operation 16 times. */
    for (ii = 0; ii < 16; ii++) {
        /* Set direction. */
        i = (edflag) ? 15 - ii : ii;
        /* Save the R array, which will be the new L. */
        memcpy(tempL, R, 32);
        /* Expand R to 48 bits using the E selector; exclusive-or with the
         * current key bits. */
        for (j = 0; j < 48; j++)
            preS[j] = R[E[j] - 1] ^ KS[i][j];
        /*
         * The pre-select bits are now considered in 8 groups of 6 bits each.
         * The 8 selection functions map these 6-bit quantities into 4-bit
         * quantities and the results permuted to make an f(R, K).  The
         * indexing into the selection functions is peculiar; it could be
         * simplified by rewriting the tables.
         */
        for (j = 0; j < 8; j++) {
            t = 6 * j;
            k = S[j][(preS[t + 0] << 5) +
                     (preS[t + 1] << 3) +
                     (preS[t + 2] << 2) +
                     (preS[t + 3] << 1) +
                     (preS[t + 4] << 0) +
                     (preS[t + 5] << 4)];
            t = 4 * j;
            f[t + 0] = (k >> 3) & 1;
            f[t + 1] = (k >> 2) & 1;
            f[t + 2] = (k >> 1) & 1;
            f[t + 3] = (k >> 0) & 1;
        }
        /* The new R is L ^ f(R, K).  The f here has to be permuted first,
         * though. */
        for (j = 0; j < 32; j++)
            R[j] = L[j] ^ f[P[j] - 1];
        /* Finally, the new L (the original R) is copied back. */
        memcpy(L, tempL, 32);
    }
    /* The output L and R are reversed. */
    for (j = 0; j < 32; j++) {
        t = L[j];
        L[j] = R[j];
        R[j] = t;
    }
    /* The final output gets the inverse permutation of the very original. */
    for (j = 0; j < 64; j++)
        block[j] = L[FP[j] - 1];
}

/* iobuf must be at least 16 bytes */
static char *
afs_crypt(const char *pw, const char *salt, char *iobuf)
{
    int i, j, c;
    int temp;
    char block[66];
    char E[48];
    char KS[16][48];            /* Key schedule, generated from key */

    for (i = 0; i < 66; i++)
        block[i] = 0;
    for (i = 0; (c = *pw) != '\0' && i < 64; pw++){
        for(j = 0; j < 7; j++, i++)
            block[i] = (c >> (6 - j)) & 01;
        i++;
    }

    afs_crypt_setkey(block, E, KS);

    for (i = 0; i < 66; i++)
        block[i] = 0;

    for (i = 0; i < 2; i++) {
        c = *salt++;
        iobuf[i] = c;
        if (c > 'Z')
            c -= 6;
        if (c > '9')
            c -= 7;
        c -= '.';
        for (j = 0; j < 6; j++) {
            if ((c >> j) & 01) {
                temp = E[6 * i + j];
                E[6 * i + j] = E[6 * i + j + 24];
                E[6 * i + j + 24] = temp;
            }
        }
    }

    for (i = 0; i < 25; i++)
        afs_encrypt_block(block, E, KS);

    for (i = 0; i < 11; i++) {
        c = 0;
        for (j = 0; j < 6; j++) {
            c <<= 1;
            c |= block[6 * i + j];
        }
        c += '.';
        if (c > '9')
            c += 7;
        if (c > 'Z')
            c += 6;
        iobuf[i + 2] = c;
    }
    iobuf[i + 2] = 0;
    if (iobuf[1] == 0)
        iobuf[1] = iobuf[0];
    return iobuf;
}

static krb5_error_code
afs_s2k_oneblock(const krb5_data *data, const krb5_data *salt,
                 unsigned char *key_out)
{
    unsigned int i;
    unsigned char password[9]; /* trailing nul for crypt() */
    char afs_crypt_buf[16];

    /*
     * Run afs_crypt and use the first eight returned bytes after the copy of
     * the (fixed) salt.
     *
     * Since the returned bytes are alphanumeric, the output is limited to
     * 2**48 possibilities; for each byte, only 64 possible values can be used.
     */

    memset(password, 0, sizeof(password));
    if (salt->length > 0)
        memcpy(password, salt->data, min(salt->length, 8));
    for (i = 0; i < 8; i++) {
        if (isupper(password[i]))
            password[i] = tolower(password[i]);
    }
    for (i = 0; i < data->length; i++)
        password[i] ^= data->data[i];
    for (i = 0; i < 8; i++) {
        if (password[i] == '\0')
            password[i] = 'X';
    }
    password[8] = '\0';
    /* Out-of-bounds salt characters are equivalent to a salt string
     * of "p1". */
    strncpy((char *)key_out,
            (char *)afs_crypt((char *)password, "#~", afs_crypt_buf) + 2, 8);
    for (i = 0; i < 8; i++)
        key_out[i] <<= 1;
    /* Fix up key parity again. */
    k5_des_fixup_key_parity(key_out);
    zap(password, sizeof(password));
    return 0;
}

static krb5_error_code
afs_s2k_multiblock(const krb5_data *data, const krb5_data *salt,
                   unsigned char *key_out)
{
    krb5_error_code ret;
    unsigned char ivec[8], tkey[8], *password;
    size_t pw_len = salt->length + data->length;
    unsigned int i, j;

    /* Do a CBC checksum, twice, and use the result as the new key.  */

    password = malloc(pw_len);
    if (!password)
        return ENOMEM;

    if (data->length > 0)
        memcpy(password, data->data, data->length);
    for (i = data->length, j = 0; j < salt->length; i++, j++) {
        password[i] = salt->data[j];
        if (isupper(password[i]))
            password[i] = tolower(password[i]);
    }

    memcpy(ivec, "kerberos", sizeof(ivec));
    memcpy(tkey, ivec, sizeof(tkey));
    k5_des_fixup_key_parity(tkey);
    ret = des_cbc_mac(tkey, ivec, password, pw_len, tkey);
    if (ret)
        goto cleanup;

    memcpy(ivec, tkey, sizeof(ivec));
    k5_des_fixup_key_parity(tkey);
    ret = des_cbc_mac(tkey, ivec, password, pw_len, key_out);
    if (ret)
        goto cleanup;
    k5_des_fixup_key_parity(key_out);

cleanup:
    zapfree(password, pw_len);
    return ret;
}

static krb5_error_code
afs_s2k(const krb5_data *data, const krb5_data *salt, unsigned char *key_out)
{
    if (data->length <= 8)
        return afs_s2k_oneblock(data, salt, key_out);
    else
        return afs_s2k_multiblock(data, salt, key_out);
}

static krb5_error_code
des_s2k(const krb5_data *pw, const krb5_data *salt, unsigned char *key_out)
{
    union {
        /* 8 "forward" bytes, 8 "reverse" bytes */
        unsigned char uc[16];
        krb5_ui_4 ui[4];
    } temp;
    unsigned int i;
    krb5_ui_4 x, y, z;
    unsigned char *p, *copy;
    size_t copylen;
    krb5_error_code ret;

    /* As long as the architecture is big-endian or little-endian, it
       doesn't matter which it is.  Think of it as reversing the
       bytes, and also reversing the bits within each byte.  But this
       current algorithm is dependent on having four 8-bit char values
       exactly overlay a 32-bit integral type.  */
    if (sizeof(temp.uc) != sizeof(temp.ui)
        || (unsigned char)~0 != 0xFF
        || (krb5_ui_4)~(krb5_ui_4)0 != 0xFFFFFFFF
        || (temp.uc[0] = 1, temp.uc[1] = 2, temp.uc[2] = 3, temp.uc[3] = 4,
            !(temp.ui[0] == 0x01020304
              || temp.ui[0] == 0x04030201)))
        abort();
#define FETCH4(VAR, IDX)        VAR = temp.ui[IDX/4]
#define PUT4(VAR, IDX)          temp.ui[IDX/4] = VAR

    copylen = pw->length + salt->length;
    /* Don't need NUL termination, at this point we're treating it as
       a byte array, not a string.  */
    copy = malloc(copylen);
    if (copy == NULL)
        return ENOMEM;
    if (pw->length > 0)
        memcpy(copy, pw->data, pw->length);
    if (salt->length > 0)
        memcpy(copy + pw->length, salt->data, salt->length);

    memset(&temp, 0, sizeof(temp));
    p = temp.uc;
    /* Handle the fan-fold xor operation by splitting the data into
       forward and reverse sections, and combine them later, rather
       than having to do the reversal over and over again.  */
    for (i = 0; i < copylen; i++) {
        *p++ ^= copy[i];
        if (p == temp.uc+16) {
            p = temp.uc;
#ifdef PRINT_TEST_VECTORS
            {
                int j;
                printf("after %d input bytes:\nforward block:\t", i+1);
                for (j = 0; j < 8; j++)
                    printf(" %02x", temp.uc[j] & 0xff);
                printf("\nreverse block:\t");
                for (j = 8; j < 16; j++)
                    printf(" %02x", temp.uc[j] & 0xff);
                printf("\n");
            }
#endif
        }
    }

#ifdef PRINT_TEST_VECTORS
    if (p != temp.uc) {
        int j;
        printf("at end, after %d input bytes:\nforward block:\t", i);
        for (j = 0; j < 8; j++)
            printf(" %02x", temp.uc[j] & 0xff);
        printf("\nreverse block:\t");
        for (j = 8; j < 16; j++)
            printf(" %02x", temp.uc[j] & 0xff);
        printf("\n");
    }
#endif
#define REVERSE(VAR)                            \
    {                                           \
        krb5_ui_4 old = VAR, temp1 = 0;         \
        int j;                                  \
        for (j = 0; j < 32; j++) {              \
            temp1 = (temp1 << 1) | (old & 1);   \
            old >>= 1;                          \
        }                                       \
        VAR = temp1;                            \
    }

    FETCH4 (x, 8);
    FETCH4 (y, 12);
    /* Ignore high bits of each input byte.  */
    x &= 0x7F7F7F7F;
    y &= 0x7F7F7F7F;
    /* Reverse the bit strings -- after this, y is "before" x.  */
    REVERSE (x);
    REVERSE (y);
#ifdef PRINT_TEST_VECTORS
    {
        int j;
        union { unsigned char uc[4]; krb5_ui_4 ui; } t2;
        printf("after reversal, reversed block:\n\t\t");
        t2.ui = y;
        for (j = 0; j < 4; j++)
            printf(" %02x", t2.uc[j] & 0xff);
        t2.ui = x;
        for (j = 0; j < 4; j++)
            printf(" %02x", t2.uc[j] & 0xff);
        printf("\n");
    }
#endif
    /* Ignored bits are now at the bottom of each byte, where we'll
     * put the parity bits.  Good.  */
    FETCH4 (z, 0);
    z &= 0x7F7F7F7F;
    /* Ignored bits for z are at the top of each byte; fix that.  */
    z <<= 1;
    /* Finish the fan-fold xor for these four bytes.  */
    z ^= y;
    PUT4 (z, 0);
    /* Now do the second four bytes.  */
    FETCH4 (z, 4);
    z &= 0x7F7F7F7F;
    /* Ignored bits for z are at the top of each byte; fix that.  */
    z <<= 1;
    /* Finish the fan-fold xor for these four bytes.  */
    z ^= x;
    PUT4 (z, 4);

#ifdef PRINT_TEST_VECTORS
    {
        int j;
        printf("after reversal, combined block:\n\t\t");
        for (j = 0; j < 8; j++)
            printf(" %02x", temp.uc[j] & 0xff);
        printf("\n");
    }
#endif

#define FIXUP(k) (k5_des_fixup_key_parity(k),                   \
                  k5_des_is_weak_key(k) ? (k[7] ^= 0xF0) : 0)

    /* Now temp.cb is the temporary key, with invalid parity.  */
    FIXUP(temp.uc);

#ifdef PRINT_TEST_VECTORS
    {
        int j;
        printf("after fixing parity and weak keys:\n\t\t");
        for (j = 0; j < 8; j++)
            printf(" %02x", temp.uc[j] & 0xff);
        printf("\n");
    }
#endif

    ret = des_cbc_mac(temp.uc, temp.uc, copy, copylen, temp.uc);
    if (ret)
        goto cleanup;

#ifdef PRINT_TEST_VECTORS
    {
        int j;
        printf("cbc checksum:\n\t\t");
        for (j = 0; j < 8; j++)
            printf(" %02x", temp.uc[j] & 0xff);
        printf("\n");
    }
#endif

    FIXUP(temp.uc);

#ifdef PRINT_TEST_VECTORS
    {
        int j;
        printf("after fixing parity and weak keys:\n\t\t");
        for (j = 0; j < 8; j++)
            printf(" %02x", temp.uc[j] & 0xff);
        printf("\n");
    }
#endif

    memcpy(key_out, temp.uc, 8);

cleanup:
    zap(&temp, sizeof(temp));
    zapfree(copy, copylen);
    return ret;
}

krb5_error_code
krb5int_des_string_to_key(const struct krb5_keytypes *ktp,
                          const krb5_data *string, const krb5_data *salt,
                          const krb5_data *parm, krb5_keyblock *keyblock)
{
    int type;

    if (parm != NULL) {
        if (parm->length != 1)
            return KRB5_ERR_BAD_S2K_PARAMS;
        type = parm->data[0];
        if (type != 0 && type != 1)
            return KRB5_ERR_BAD_S2K_PARAMS;
    } else
        type = 0;

    /* Use AFS string to key if we were told to. */
    if (type == 1)
        return afs_s2k(string, salt, keyblock->contents);

    return des_s2k(string, salt, keyblock->contents);
}
