/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_NV_IMPL_H_
#define	_NV_IMPL_H_

#ifndef	_NVPAIR_T_DECLARED
#define	_NVPAIR_T_DECLARED
struct nvpair;

typedef struct nvpair nvpair_t;
#endif

#define	NV_TYPE_NVLIST_UP		255

#define	NV_TYPE_FIRST		NV_TYPE_NULL
#define	NV_TYPE_LAST		NV_TYPE_BINARY

#define	NV_FLAG_BIG_ENDIAN		0x80

int	*nvlist_descriptors(const nvlist_t *nvl, size_t *nitemsp);
size_t	 nvlist_ndescriptors(const nvlist_t *nvl);

nvpair_t *nvlist_first_nvpair(const nvlist_t *nvl);
nvpair_t *nvlist_next_nvpair(const nvlist_t *nvl, const nvpair_t *nvp);
nvpair_t *nvlist_prev_nvpair(const nvlist_t *nvl, const nvpair_t *nvp);

void nvlist_add_nvpair(nvlist_t *nvl, const nvpair_t *nvp);

void nvlist_move_nvpair(nvlist_t *nvl, nvpair_t *nvp);

void nvlist_set_parent(nvlist_t *nvl, nvpair_t *parent);

const nvpair_t *nvlist_get_nvpair(const nvlist_t *nvl, const char *name);

nvpair_t *nvlist_take_nvpair(nvlist_t *nvl, const char *name);

/* Function removes the given nvpair from the nvlist. */
void nvlist_remove_nvpair(nvlist_t *nvl, nvpair_t *nvp);

void nvlist_free_nvpair(nvlist_t *nvl, nvpair_t *nvp);

int nvpair_type(const nvpair_t *nvp);
const char *nvpair_name(const nvpair_t *nvp);

nvpair_t *nvpair_clone(const nvpair_t *nvp);

nvpair_t *nvpair_create_null(const char *name);
nvpair_t *nvpair_create_bool(const char *name, bool value);
nvpair_t *nvpair_create_number(const char *name, uint64_t value);
nvpair_t *nvpair_create_string(const char *name, const char *value);
nvpair_t *nvpair_create_stringf(const char *name, const char *valuefmt, ...) __printflike(2, 3);
nvpair_t *nvpair_create_stringv(const char *name, const char *valuefmt, va_list valueap) __printflike(2, 0);
nvpair_t *nvpair_create_nvlist(const char *name, const nvlist_t *value);
nvpair_t *nvpair_create_descriptor(const char *name, int value);
nvpair_t *nvpair_create_binary(const char *name, const void *value, size_t size);

nvpair_t *nvpair_move_string(const char *name, char *value);
nvpair_t *nvpair_move_nvlist(const char *name, nvlist_t *value);
nvpair_t *nvpair_move_descriptor(const char *name, int value);
nvpair_t *nvpair_move_binary(const char *name, void *value, size_t size);

bool		 nvpair_get_bool(const nvpair_t *nvp);
uint64_t	 nvpair_get_number(const nvpair_t *nvp);
const char	*nvpair_get_string(const nvpair_t *nvp);
const nvlist_t	*nvpair_get_nvlist(const nvpair_t *nvp);
int		 nvpair_get_descriptor(const nvpair_t *nvp);
const void	*nvpair_get_binary(const nvpair_t *nvp, size_t *sizep);

void nvpair_free(nvpair_t *nvp);

const nvpair_t *nvlist_getf_nvpair(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);

const nvpair_t *nvlist_getv_nvpair(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);

nvpair_t *nvlist_takef_nvpair(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);

nvpair_t *nvlist_takev_nvpair(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);

nvpair_t *nvpair_createf_null(const char *namefmt, ...) __printflike(1, 2);
nvpair_t *nvpair_createf_bool(bool value, const char *namefmt, ...) __printflike(2, 3);
nvpair_t *nvpair_createf_number(uint64_t value, const char *namefmt, ...) __printflike(2, 3);
nvpair_t *nvpair_createf_string(const char *value, const char *namefmt, ...) __printflike(2, 3);
nvpair_t *nvpair_createf_nvlist(const nvlist_t *value, const char *namefmt, ...) __printflike(2, 3);
nvpair_t *nvpair_createf_descriptor(int value, const char *namefmt, ...) __printflike(2, 3);
nvpair_t *nvpair_createf_binary(const void *value, size_t size, const char *namefmt, ...) __printflike(3, 4);

nvpair_t *nvpair_createv_null(const char *namefmt, va_list nameap) __printflike(1, 0);
nvpair_t *nvpair_createv_bool(bool value, const char *namefmt, va_list nameap) __printflike(2, 0);
nvpair_t *nvpair_createv_number(uint64_t value, const char *namefmt, va_list nameap) __printflike(2, 0);
nvpair_t *nvpair_createv_string(const char *value, const char *namefmt, va_list nameap) __printflike(2, 0);
nvpair_t *nvpair_createv_nvlist(const nvlist_t *value, const char *namefmt, va_list nameap) __printflike(2, 0);
nvpair_t *nvpair_createv_descriptor(int value, const char *namefmt, va_list nameap) __printflike(2, 0);
nvpair_t *nvpair_createv_binary(const void *value, size_t size, const char *namefmt, va_list nameap) __printflike(3, 0);

nvpair_t *nvpair_movef_string(char *value, const char *namefmt, ...) __printflike(2, 3);
nvpair_t *nvpair_movef_nvlist(nvlist_t *value, const char *namefmt, ...) __printflike(2, 3);
nvpair_t *nvpair_movef_descriptor(int value, const char *namefmt, ...) __printflike(2, 3);
nvpair_t *nvpair_movef_binary(void *value, size_t size, const char *namefmt, ...) __printflike(3, 4);

nvpair_t *nvpair_movev_string(char *value, const char *namefmt, va_list nameap) __printflike(2, 0);
nvpair_t *nvpair_movev_nvlist(nvlist_t *value, const char *namefmt, va_list nameap) __printflike(2, 0);
nvpair_t *nvpair_movev_descriptor(int value, const char *namefmt, va_list nameap) __printflike(2, 0);
nvpair_t *nvpair_movev_binary(void *value, size_t size, const char *namefmt, va_list nameap) __printflike(3, 0);

#endif	/* !_NV_IMPL_H_ */
