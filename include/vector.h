/*
 * *****************************************************************************
 *
 * Copyright (c) 2018-2020 Gavin D. Howard and contributors.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************************************************************
 *
 * Definitions for bc vectors (resizable arrays).
 *
 */

#ifndef BC_VECTOR_H
#define BC_VECTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <status.h>

#define BC_VEC_INVALID_IDX (SIZE_MAX)
#define BC_VEC_START_CAP (UINTMAX_C(1)<<5)

typedef unsigned char uchar;

typedef void (*BcVecFree)(void*);

// Forward declaration.
struct BcId;

typedef struct BcVec {
	char *v;
	size_t len;
	size_t cap;
	size_t size;
	BcVecFree dtor;
} BcVec;

void bc_vec_init(BcVec *restrict v, size_t esize, BcVecFree dtor);
void bc_vec_expand(BcVec *restrict v, size_t req);

void bc_vec_npop(BcVec *restrict v, size_t n);
void bc_vec_npopAt(BcVec *restrict v, size_t n, size_t idx);

void bc_vec_push(BcVec *restrict v, const void *data);
void bc_vec_npush(BcVec *restrict v, size_t n, const void *data);
void bc_vec_pushByte(BcVec *restrict v, uchar data);
void bc_vec_pushIndex(BcVec *restrict v, size_t idx);
void bc_vec_string(BcVec *restrict v, size_t len, const char *restrict str);
void bc_vec_concat(BcVec *restrict v, const char *restrict str);
void bc_vec_empty(BcVec *restrict v);

#if BC_ENABLE_HISTORY
void bc_vec_replaceAt(BcVec *restrict v, size_t idx, const void *data);
#endif // BC_ENABLE_HISTORY

void* bc_vec_item(const BcVec *restrict v, size_t idx);
void* bc_vec_item_rev(const BcVec *restrict v, size_t idx);

void bc_vec_clear(BcVec *restrict v);

void bc_vec_free(void *vec);

bool bc_map_insert(BcVec *restrict v, const char *name,
                   size_t idx, size_t *restrict i);
size_t bc_map_index(const BcVec *restrict v, const char *name);

#define bc_vec_pop(v) (bc_vec_npop((v), 1))
#define bc_vec_top(v) (bc_vec_item_rev((v), 0))

#ifndef NDEBUG
#define bc_map_init(v) (bc_vec_init((v), sizeof(BcId), bc_id_free))
#else // NDEBUG
#define bc_map_init(v) (bc_vec_init((v), sizeof(BcId), NULL))
#endif // NDEBUG

#endif // BC_VECTOR_H
