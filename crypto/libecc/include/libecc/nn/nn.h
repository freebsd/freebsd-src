/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#ifndef __NN_H__
#define __NN_H__

#include <libecc/words/words.h>
#include <libecc/nn/nn_config.h>
#include <libecc/utils/utils.h>

/*
 * For a given amount of bytes (resp. bits), return the minimum number
 * of words required to store that amount of bytes (respectively bits).
 */
#define BYTE_LEN_WORDS(nbytes) (((nbytes) + WORD_BYTES - 1) / WORD_BYTES)
#define BIT_LEN_WORDS(nbits) (((nbits) + WORD_BITS - 1) / WORD_BITS)

/*
 * For a given amount of bytes (resp. bits), return the first number of
 * bytes (resp. bits) equal or above to that value which is a multiple
 * of word bytes.
 */
#define BYTE_LEN_CEIL(nbytes) (BYTE_LEN_WORDS(nbytes) * WORD_BYTES)
#define BIT_LEN_CEIL(nbits) (BIT_LEN_WORDS(nbits) * WORD_BITS)

/*
 * Our nn type contains an array of words, which is of a fixed given storage
 * size defined in nn_lib_ecc_config.h.
 *
 * Each word in this array is in local endianness whereas the words
 * in the array are ordered in a little endian way with regard to their
 * indices. That is: the word at index 0 in the array contains the least
 * significant word of the nn.
 *
 * Except explicitly specified (some functions may provide automatic
 * initialization of output params), initialization is usually required
 * before nn can be used.
 *
 * After initialization, the 'wlen' attribute provides at each moment
 * an upper bound on the position of last non-zero word in the array.
 * All words after that point are always guaranteed to be 0 after any
 * manipulation by a function of this module.
 * Functions use this assumption to optimize operations by avoiding to
 * process leading zeros.
 * Nevertheless, some functions still access words past the 'wlen' index
 * and return correct results only if these words are 0.
 *
 * Note that functions with parameters not explicitly marked as const may
 * modify the value of the 'wlen' attribute if they see fit.
 * And indeed most of them set the output 'wlen' attribute to the maximal
 * possible value given the inputs 'wlen' attributes.
 * The most notable exceptions are the logical functions whose result
 * depends on the preset value of the output 'wlen' attribute.
 */
typedef struct {
	word_t val[BIT_LEN_WORDS(NN_MAX_BIT_LEN)];
	word_t magic;
	u8 wlen;
} nn;

typedef nn *nn_t;
typedef const nn *nn_src_t;

ATTRIBUTE_WARN_UNUSED_RET int nn_check_initialized(nn_src_t A);
ATTRIBUTE_WARN_UNUSED_RET int nn_is_initialized(nn_src_t A);
ATTRIBUTE_WARN_UNUSED_RET int nn_zero(nn_t A);
ATTRIBUTE_WARN_UNUSED_RET int nn_one(nn_t A);
ATTRIBUTE_WARN_UNUSED_RET int nn_set_word_value(nn_t A, word_t val);
void nn_uninit(nn_t A);
ATTRIBUTE_WARN_UNUSED_RET int nn_init(nn_t A, u16 len);
ATTRIBUTE_WARN_UNUSED_RET int nn_init_from_buf(nn_t A, const u8 *buf, u16 buflen);
ATTRIBUTE_WARN_UNUSED_RET int nn_cnd_swap(int cnd, nn_t in1, nn_t in2);
ATTRIBUTE_WARN_UNUSED_RET int nn_set_wlen(nn_t A, u8 new_wlen);
ATTRIBUTE_WARN_UNUSED_RET int nn_iszero(nn_src_t A, int *iszero);
ATTRIBUTE_WARN_UNUSED_RET int nn_isone(nn_src_t A, int *isone);
ATTRIBUTE_WARN_UNUSED_RET int nn_isodd(nn_src_t A, int *isodd);
ATTRIBUTE_WARN_UNUSED_RET int nn_cmp_word(nn_src_t in, word_t w, int *cmp);
ATTRIBUTE_WARN_UNUSED_RET int nn_cmp(nn_src_t A, nn_src_t B, int *cmp);
ATTRIBUTE_WARN_UNUSED_RET int nn_copy(nn_t dst_nn, nn_src_t src_nn);
ATTRIBUTE_WARN_UNUSED_RET int nn_normalize(nn_t in1);
ATTRIBUTE_WARN_UNUSED_RET int nn_export_to_buf(u8 *buf, u16 buflen, nn_src_t in_nn);
ATTRIBUTE_WARN_UNUSED_RET int nn_tabselect(nn_t out, u8 idx, nn_src_t *tab, u8 tabsize);

#endif /* __NN_H__ */
