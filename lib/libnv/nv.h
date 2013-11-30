/*-
 * Copyright (c) 2009-2013 The FreeBSD Foundation
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

#ifndef	_NV_H_
#define	_NV_H_

#include <sys/cdefs.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef	_NVLIST_T_DECLARED
#define	_NVLIST_T_DECLARED
struct nvlist;

typedef struct nvlist nvlist_t;
#endif

#define	NV_NAME_MAX	2048

#define	NV_TYPE_NONE			0

#define	NV_TYPE_NULL			1
#define	NV_TYPE_BOOL			2
#define	NV_TYPE_NUMBER			3
#define	NV_TYPE_STRING			4
#define	NV_TYPE_NVLIST			5
#define	NV_TYPE_DESCRIPTOR		6
#define	NV_TYPE_BINARY			7

/*
 * Perform case-insensitive lookups of provided names.
 */
#define	NV_FLAG_IGNORE_CASE		0x01

nvlist_t	*nvlist_create(int flags);
void		 nvlist_destroy(nvlist_t *nvl);
int		 nvlist_error(const nvlist_t *nvl);
bool		 nvlist_empty(const nvlist_t *nvl);

nvlist_t *nvlist_clone(const nvlist_t *nvl);

void nvlist_dump(const nvlist_t *nvl, int fd);
void nvlist_fdump(const nvlist_t *nvl, FILE *fp);

size_t		 nvlist_size(const nvlist_t *nvl);
void		*nvlist_pack(const nvlist_t *nvl, size_t *sizep);
nvlist_t	*nvlist_unpack(const void *buf, size_t size);

int nvlist_send(int sock, const nvlist_t *nvl);
nvlist_t *nvlist_recv(int sock);
nvlist_t *nvlist_xfer(int sock, nvlist_t *nvl);

const char *nvlist_next(const nvlist_t *nvl, int *typep, void **cookiep);

/*
 * The nvlist_exists functions check if the given name (optionally of the given
 * type) exists on nvlist.
 */

bool nvlist_exists(const nvlist_t *nvl, const char *name);
bool nvlist_exists_type(const nvlist_t *nvl, const char *name, int type);

bool nvlist_exists_null(const nvlist_t *nvl, const char *name);
bool nvlist_exists_bool(const nvlist_t *nvl, const char *name);
bool nvlist_exists_number(const nvlist_t *nvl, const char *name);
bool nvlist_exists_string(const nvlist_t *nvl, const char *name);
bool nvlist_exists_nvlist(const nvlist_t *nvl, const char *name);
bool nvlist_exists_descriptor(const nvlist_t *nvl, const char *name);
bool nvlist_exists_binary(const nvlist_t *nvl, const char *name);

/*
 * The nvlist_add functions add the given name/value pair.
 * If a pointer is provided, nvlist_add will internally allocate memory for the
 * given data (in other words it won't consume provided buffer).
 */

void nvlist_add_null(nvlist_t *nvl, const char *name);
void nvlist_add_bool(nvlist_t *nvl, const char *name, bool value);
void nvlist_add_number(nvlist_t *nvl, const char *name, uint64_t value);
void nvlist_add_string(nvlist_t *nvl, const char *name, const char *value);
void nvlist_add_stringf(nvlist_t *nvl, const char *name, const char *valuefmt, ...) __printflike(3, 4);
void nvlist_add_stringv(nvlist_t *nvl, const char *name, const char *valuefmt, va_list valueap) __printflike(3, 0);
void nvlist_add_nvlist(nvlist_t *nvl, const char *name, const nvlist_t *value);
void nvlist_add_descriptor(nvlist_t *nvl, const char *name, int value);
void nvlist_add_binary(nvlist_t *nvl, const char *name, const void *value, size_t size);

/*
 * The nvlist_move functions add the given name/value pair.
 * The functions consumes provided buffer.
 */

void nvlist_move_string(nvlist_t *nvl, const char *name, char *value);
void nvlist_move_nvlist(nvlist_t *nvl, const char *name, nvlist_t *value);
void nvlist_move_descriptor(nvlist_t *nvl, const char *name, int value);
void nvlist_move_binary(nvlist_t *nvl, const char *name, void *value, size_t size);

/*
 * The nvlist_get functions returns value associated with the given name.
 * If it returns a pointer, the pointer represents internal buffer and should
 * not be freed by the caller.
 */

bool		 nvlist_get_bool(const nvlist_t *nvl, const char *name);
uint64_t	 nvlist_get_number(const nvlist_t *nvl, const char *name);
const char	*nvlist_get_string(const nvlist_t *nvl, const char *name);
const nvlist_t	*nvlist_get_nvlist(const nvlist_t *nvl, const char *name);
int		 nvlist_get_descriptor(const nvlist_t *nvl, const char *name);
const void	*nvlist_get_binary(const nvlist_t *nvl, const char *name, size_t *sizep);

/*
 * The nvlist_take functions returns value associated with the given name and
 * remove the given entry from the nvlist.
 * The caller is responsible for freeing received data.
 */

bool		 nvlist_take_bool(nvlist_t *nvl, const char *name);
uint64_t	 nvlist_take_number(nvlist_t *nvl, const char *name);
char		*nvlist_take_string(nvlist_t *nvl, const char *name);
nvlist_t	*nvlist_take_nvlist(nvlist_t *nvl, const char *name);
int		 nvlist_take_descriptor(nvlist_t *nvl, const char *name);
void		*nvlist_take_binary(nvlist_t *nvl, const char *name, size_t *sizep);

/*
 * The nvlist_free functions removes the given name/value pair from the nvlist
 * and frees memory associated with it.
 */

void nvlist_free(nvlist_t *nvl, const char *name);
void nvlist_free_type(nvlist_t *nvl, const char *name, int type);

void nvlist_free_null(nvlist_t *nvl, const char *name);
void nvlist_free_bool(nvlist_t *nvl, const char *name);
void nvlist_free_number(nvlist_t *nvl, const char *name);
void nvlist_free_string(nvlist_t *nvl, const char *name);
void nvlist_free_nvlist(nvlist_t *nvl, const char *name);
void nvlist_free_descriptor(nvlist_t *nvl, const char *name);
void nvlist_free_binary(nvlist_t *nvl, const char *name);

/*
 * Below are the same functions, but which operate on format strings and
 * variable argument lists.
 */

bool nvlist_existsf(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
bool nvlist_existsf_type(const nvlist_t *nvl, int type, const char *namefmt, ...) __printflike(3, 4);

bool nvlist_existsf_null(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
bool nvlist_existsf_bool(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
bool nvlist_existsf_number(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
bool nvlist_existsf_string(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
bool nvlist_existsf_nvlist(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
bool nvlist_existsf_descriptor(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
bool nvlist_existsf_binary(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);

bool nvlist_existsv(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
bool nvlist_existsv_type(const nvlist_t *nvl, int type, const char *namefmt, va_list nameap) __printflike(3, 0);

bool nvlist_existsv_null(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
bool nvlist_existsv_bool(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
bool nvlist_existsv_number(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
bool nvlist_existsv_string(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
bool nvlist_existsv_nvlist(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
bool nvlist_existsv_descriptor(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
bool nvlist_existsv_binary(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);

void nvlist_addf_null(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
void nvlist_addf_bool(nvlist_t *nvl, bool value, const char *namefmt, ...) __printflike(3, 4);
void nvlist_addf_number(nvlist_t *nvl, uint64_t value, const char *namefmt, ...) __printflike(3, 4);
void nvlist_addf_string(nvlist_t *nvl, const char *value, const char *namefmt, ...) __printflike(3, 4);
void nvlist_addf_nvlist(nvlist_t *nvl, const nvlist_t *value, const char *namefmt, ...) __printflike(3, 4);
void nvlist_addf_descriptor(nvlist_t *nvl, int value, const char *namefmt, ...) __printflike(3, 4);
void nvlist_addf_binary(nvlist_t *nvl, const void *value, size_t size, const char *namefmt, ...) __printflike(4, 5);

void nvlist_addv_null(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
void nvlist_addv_bool(nvlist_t *nvl, bool value, const char *namefmt, va_list nameap) __printflike(3, 0);
void nvlist_addv_number(nvlist_t *nvl, uint64_t value, const char *namefmt, va_list nameap) __printflike(3, 0);
void nvlist_addv_string(nvlist_t *nvl, const char *value, const char *namefmt, va_list nameap) __printflike(3, 0);
void nvlist_addv_nvlist(nvlist_t *nvl, const nvlist_t *value, const char *namefmt, va_list nameap) __printflike(3, 0);
void nvlist_addv_descriptor(nvlist_t *nvl, int value, const char *namefmt, va_list nameap) __printflike(3, 0);
void nvlist_addv_binary(nvlist_t *nvl, const void *value, size_t size, const char *namefmt, va_list nameap) __printflike(4, 0);

void nvlist_movef_string(nvlist_t *nvl, char *value, const char *namefmt, ...) __printflike(3, 4);
void nvlist_movef_nvlist(nvlist_t *nvl, nvlist_t *value, const char *namefmt, ...) __printflike(3, 4);
void nvlist_movef_descriptor(nvlist_t *nvl, int value, const char *namefmt, ...) __printflike(3, 4);
void nvlist_movef_binary(nvlist_t *nvl, void *value, size_t size, const char *namefmt, ...) __printflike(4, 5);

void nvlist_movev_string(nvlist_t *nvl, char *value, const char *namefmt, va_list nameap) __printflike(3, 0);
void nvlist_movev_nvlist(nvlist_t *nvl, nvlist_t *value, const char *namefmt, va_list nameap) __printflike(3, 0);
void nvlist_movev_descriptor(nvlist_t *nvl, int value, const char *namefmt, va_list nameap) __printflike(3, 0);
void nvlist_movev_binary(nvlist_t *nvl, void *value, size_t size, const char *namefmt, va_list nameap) __printflike(4, 0);

bool		 nvlist_getf_bool(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
uint64_t	 nvlist_getf_number(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
const char	*nvlist_getf_string(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
const nvlist_t	*nvlist_getf_nvlist(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
int		 nvlist_getf_descriptor(const nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
const void	*nvlist_getf_binary(const nvlist_t *nvl, size_t *sizep, const char *namefmt, ...) __printflike(3, 4);

bool		 nvlist_getv_bool(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
uint64_t	 nvlist_getv_number(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
const char	*nvlist_getv_string(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
const nvlist_t	*nvlist_getv_nvlist(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
int		 nvlist_getv_descriptor(const nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
const void	*nvlist_getv_binary(const nvlist_t *nvl, size_t *sizep, const char *namefmt, va_list nameap) __printflike(3, 0);

bool		 nvlist_takef_bool(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
uint64_t	 nvlist_takef_number(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
char		*nvlist_takef_string(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
nvlist_t	*nvlist_takef_nvlist(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
int		 nvlist_takef_descriptor(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
void		*nvlist_takef_binary(nvlist_t *nvl, size_t *sizep, const char *namefmt, ...) __printflike(3, 4);

bool		 nvlist_takev_bool(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
uint64_t	 nvlist_takev_number(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
char		*nvlist_takev_string(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
nvlist_t	*nvlist_takev_nvlist(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
int		 nvlist_takev_descriptor(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
void		*nvlist_takev_binary(nvlist_t *nvl, size_t *sizep, const char *namefmt, va_list nameap) __printflike(3, 0);

void nvlist_freef(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
void nvlist_freef_type(nvlist_t *nvl, int type, const char *namefmt, ...) __printflike(3, 4);

void nvlist_freef_null(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
void nvlist_freef_bool(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
void nvlist_freef_number(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
void nvlist_freef_string(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
void nvlist_freef_nvlist(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
void nvlist_freef_descriptor(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);
void nvlist_freef_binary(nvlist_t *nvl, const char *namefmt, ...) __printflike(2, 3);

void nvlist_freev(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
void nvlist_freev_type(nvlist_t *nvl, int type, const char *namefmt, va_list nameap) __printflike(3, 0);

void nvlist_freev_null(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
void nvlist_freev_bool(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
void nvlist_freev_number(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
void nvlist_freev_string(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
void nvlist_freev_nvlist(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
void nvlist_freev_descriptor(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);
void nvlist_freev_binary(nvlist_t *nvl, const char *namefmt, va_list nameap) __printflike(2, 0);

#endif	/* !_NV_H_ */
