/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
   SipHash reference C implementation
   Copyright (c) 2012-2014 Jean-Philippe Aumasson
   <jeanphilippe.aumasson@gmail.com>
   Copyright (c) 2012-2014 Daniel J. Bernstein <djb@cr.yp.to>
   To the extent possible under law, the author(s) have dedicated all copyright
   and related and neighboring rights to this software to the public domain
   worldwide. This software is distributed without any warranty.
   You should have received a copy of the CC0 Public Domain Dedication along
   with this software. If not, see
   <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#ifndef APR_SIPHASH_H
#define APR_SIPHASH_H

#include "apr.h"
#include "apu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file apr_siphash.h
 * @brief APR-UTIL siphash library
 *        "SipHash-c-d is a family of pseudorandom functions (a.k.a. keyed
 *        hash functions) optimized for speed on short messages", designed by
 *        Jean-Philippe Aumasson and Daniel J. Bernstein. It generates a 64bit
 *        hash (or MAC) from the message and a 128bit key.
 *        See http://cr.yp.to/siphash/siphash-20120620.pdf for the details,
 *        c is the number of compression rounds, d the number of finalization
 *        rounds; we also define fast implementations for c = 2 with d = 4 (aka
 *        siphash-2-4), and c = 4 with d = 8 (aka siphash-4-8), as recommended
 *        parameters per the authors.
 */

/** size of the siphash digest */
#define APR_SIPHASH_DSIZE 8

/** size of the siphash key */
#define APR_SIPHASH_KSIZE 16


/**
 * @brief Computes SipHash-c-d, producing a 64bit (APR_SIPHASH_DSIZE) hash
 * from a message and a 128bit (APR_SIPHASH_KSIZE) secret key.
 * @param src The message
 * @param len The length of the message
 * @param key The secret key
 * @param c   The number of compression rounds
 * @param d   The number of finalization rounds
 * @return The hash value as a 64bit unsigned integer
 */
APU_DECLARE(apr_uint64_t) apr_siphash(const void *src, apr_size_t len,
                              const unsigned char key[APR_SIPHASH_KSIZE],
                                      unsigned int c, unsigned int d);

/**
 * @brief Computes SipHash-c-d, producing a 64bit (APR_SIPHASH_DSIZE) hash
 * from a message and a 128bit (APR_SIPHASH_KSIZE) secret key, into a possibly
 * unaligned buffer (using the little endian representation as defined by the
 * authors for interoperabilty) usable as a MAC.
 * @param out The output buffer (or MAC)
 * @param src The message
 * @param len The length of the message
 * @param key The secret key
 * @param c   The number of compression rounds
 * @param d   The number of finalization rounds
 * @return The hash value as a 64bit unsigned integer
 */
APU_DECLARE(void) apr_siphash_auth(unsigned char out[APR_SIPHASH_DSIZE],
                                   const void *src, apr_size_t len,
                             const unsigned char key[APR_SIPHASH_KSIZE],
                                   unsigned int c, unsigned int d);

/**
 * @brief Computes SipHash-2-4, producing a 64bit (APR_SIPHASH_DSIZE) hash
 * from a message and a 128bit (APR_SIPHASH_KSIZE) secret key.
 * @param src The message to hash
 * @param len The length of the message
 * @param key The secret key
 * @return The hash value as a 64bit unsigned integer
 */
APU_DECLARE(apr_uint64_t) apr_siphash24(const void *src, apr_size_t len,
                               const unsigned char key[APR_SIPHASH_KSIZE]);

/**
 * @brief Computes SipHash-2-4, producing a 64bit (APR_SIPHASH_DSIZE) hash
 * from a message and a 128bit (APR_SIPHASH_KSIZE) secret key, into a possibly
 * unaligned buffer (using the little endian representation as defined by the
 * authors for interoperabilty) usable as a MAC.
 * @param out The output buffer (or MAC)
 * @param src The message
 * @param len The length of the message
 * @param key The secret key
 * @return The hash value as a 64bit unsigned integer
 */
APU_DECLARE(void) apr_siphash24_auth(unsigned char out[APR_SIPHASH_DSIZE],
                                     const void *src, apr_size_t len,
                               const unsigned char key[APR_SIPHASH_KSIZE]);

/**
 * @brief Computes SipHash-4-8, producing a 64bit (APR_SIPHASH_DSIZE) hash
 * from a message and a 128bit (APR_SIPHASH_KSIZE) secret key.
 * @param src The message
 * @param len The length of the message
 * @param key The secret key
 * @return The hash value as a 64bit unsigned integer
 */
APU_DECLARE(apr_uint64_t) apr_siphash48(const void *src, apr_size_t len,
                               const unsigned char key[APR_SIPHASH_KSIZE]);

/**
 * @brief Computes SipHash-4-8, producing a 64bit (APR_SIPHASH_DSIZE) hash
 * from a message and a 128bit (APR_SIPHASH_KSIZE) secret key, into a possibly
 * unaligned buffer (using the little endian representation as defined by the
 * authors for interoperabilty) usable as a MAC.
 * @param out The output buffer (or MAC)
 * @param src The message
 * @param len The length of the message
 * @param key The secret key
 * @return The hash value as a 64bit unsigned integer
 */
APU_DECLARE(void) apr_siphash48_auth(unsigned char out[APR_SIPHASH_DSIZE],
                                     const void *src, apr_size_t len,
                               const unsigned char key[APR_SIPHASH_KSIZE]);

#ifdef __cplusplus
}
#endif

#endif  /* APR_SIPHASH_H */
