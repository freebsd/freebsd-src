/*
 * Copyright (c) 2001, Dr Brian Gladman <brg@gladman.uk.net>, Worcester, UK.
 * All rights reserved.
 *
 * TERMS
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted subject to the following conditions:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  3. The copyright holder's name must not be used to endorse or promote
 *     any products derived from this software without his specific prior
 *     written permission.
 *
 * This software is provided 'as is' with no express or implied warranties
 * of correctness or fitness for purpose.
 */

/*
 * Issue Date: 21/01/2002
 *
 * This file contains the definitions required to use AES (Rijndael) in C++.
 */

#ifndef _AESCPP_H
#define _AESCPP_H

#include "aes.h"

class AESclass
{   aes_ctx cx[1];
public:
#if defined(BLOCK_SIZE)
    AESclass()                          { cx->n_blk = BLOCK_SIZE; cx->n_rnd = 0; }
#else
    AESclass(unsigned int blen = 16)    { cx->n_blk = blen; cx->n_rnd = 0; }
#endif
    aes_rval blk_len(unsigned int blen) { return aes_blk_len(blen, cx); }
    aes_rval enc_key(const unsigned char in_key[], unsigned int klen)
            { return aes_enc_key(in_key, klen, cx); }
    aes_rval dec_key(const unsigned char in_key[], unsigned int klen)
            { return aes_dec_key(in_key, klen, cx); }
    aes_rval enc_blk(const unsigned char in_blk[], unsigned char out_blk[])
            { return aes_enc_blk(in_blk, out_blk, cx); }
    aes_rval dec_blk(const unsigned char in_blk[], unsigned char out_blk[])
            { return aes_dec_blk(in_blk, out_blk, cx); }
};

#endif
