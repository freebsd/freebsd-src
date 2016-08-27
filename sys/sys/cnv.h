/*-
 * Copyright (c) 2016 Adam Starak <starak.adam@gmail.com>
 * All rights reserved.
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

#ifndef	_CNV_H_
#define	_CNV_H_

#include <sys/cdefs.h>

#ifndef _KERNEL
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#endif

#ifndef	_NVLIST_T_DECLARED
#define	_NVLIST_T_DECLARED
struct nvlist;

typedef struct nvlist nvlist_t;
#endif

__BEGIN_DECLS

/*
 * The cnvlist_get functions returns value associated with the given cookie.
 * If it returns a pointer, the pointer represents internal buffer and should
 * not be freed by the caller.
 */

bool			 cnvlist_get_bool(void *cookiep);
uint64_t		 cnvlist_get_number(void *cookiep);
const char		*cnvlist_get_string(void *cookiep);
const nvlist_t		*cnvlist_get_nvlist(void *cookiep);
const void		*cnvlist_get_binary(void *cookiep, size_t *sizep);
const bool		*cnvlist_get_bool_array(void *cookiep, size_t *nitemsp);
const uint64_t		*cnvlist_get_number_array(void *cookiep, size_t *nitemsp);
const char * const	*cnvlist_get_string_array(void *cookiep, size_t *nitemsp);
const nvlist_t * const	*cnvlist_get_nvlist_array(void *cookiep, size_t *nitemsp);
#ifndef _KERNEL
int			 cnvlist_get_descriptor(void *cookiep);
const int		*cnvlist_get_descriptor_array(void *cookiep, size_t *nitemsp);
#endif


/*
 * The cnvlist_take functions returns value associated with the given cookie and
 * remove the given entry from the nvlist.
 * The caller is responsible for freeing received data.
 */

bool			  cnvlist_take_bool(nvlist_t *nvl, void *cookiep);
uint64_t		  cnvlist_take_number(nvlist_t *nvl, void *cookiep);
char			 *cnvlist_take_string(nvlist_t *nvl, void *cookiep);
nvlist_t		 *cnvlist_take_nvlist(nvlist_t *nvl, void *cookiep);
void			 *cnvlist_take_binary(nvlist_t *nvl, void *cookiep, size_t *sizep);
bool			 *cnvlist_take_bool_array(nvlist_t *nvl, void *cookiep, size_t *nitemsp);
uint64_t		 *cnvlist_take_number_array(nvlist_t *nvl, void *cookiep, size_t *nitemsp);
char			**cnvlist_take_string_array(nvlist_t *nvl, void *cookiep, size_t *nitemsp);
nvlist_t		**cnvlist_take_nvlist_array(nvlist_t *nvl, void *cookiep, size_t *nitemsp);
#ifndef _KERNEL
int			  cnvlist_take_descriptor(nvlist_t *nvl, void *cookiep);
int			 *cnvlist_take_descriptor_array(nvlist_t *nvl, void *cookiep, size_t *nitemsp);
#endif

/*
 * The cnvlist_free functions removes the given name/value pair from the nvlist based on cookie
 * and frees memory associated with it.
 */

void	cnvlist_free_bool(nvlist_t *nvl, void *cookiep);
void	cnvlist_free_number(nvlist_t *nvl, void *cookiep);
void	cnvlist_free_string(nvlist_t *nvl, void *cookiep);
void	cnvlist_free_nvlist(nvlist_t *nvl, void *cookiep);
void	cnvlist_free_binary(nvlist_t *nvl, void *cookiep);
void	cnvlist_free_descriptor(nvlist_t *nvl, void *cookiep);
void	cnvlist_free_bool_array(nvlist_t *nvl, void *cookiep);
void	cnvlist_free_number_array(nvlist_t *nvl, void *cookiep);
void	cnvlist_free_string_array(nvlist_t *nvl, void *cookiep);
void	cnvlist_free_nvlist_array(nvlist_t *nvl, void *cookiep);
#ifndef _KERNEL
void	cnvlist_free_descriptor(nvlist_t *nvl, void *cookiep);
void	cnvlist_free_descriptor_array(nvlist_t *nvl, void *cookiep);
#endif

__END_DECLS

#endif	/* !_CNV_H_ */
