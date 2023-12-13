/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1999 John D. Polstra
 * Copyright (c) 1999,2001 Peter Wemm <peter@FreeBSD.org>
 * All rights reserved.
 * Copyright (c) 2023 Jessica Clarke <jrtc27@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYS_LINKER_SET_H_
#define _SYS_LINKER_SET_H_

#include <mach-o/dyld.h>
#include <mach-o/getsect.h>

/*
 * The following macros are used to declare global sets of objects, which
 * are collected by the linker into a `linker_set' as defined below.
 * For Mach-O, this is done by constructing a separate section for each set.
 */

#define	__MAKE_SET_CONST const

/*
 * Private macros, not to be used outside this header file.
 */

/*
 * The userspace address sanitizer inserts redzones around global variables,
 * violating the assumption that linker set elements are packed.
 */
#define	__NOASAN	__nosanitizeaddress

#define __MAKE_SET_QV(set, sym, qv)				\
	static void const * qv					\
	__NOASAN						\
	__set_##set##_sym_##sym __section("__DATA,set_" #set)	\
	__used = &(sym)
#define __MAKE_SET(set, sym)	__MAKE_SET_QV(set, sym, __MAKE_SET_CONST)

static inline __pure2 uint8_t *
__set_getsectiondata(const char *segname, const char *sectname,
    unsigned long *size)
{
	uint32_t image_count, image_index;
	const struct mach_header *mh;
	uint8_t *ret;

	image_count = _dyld_image_count();
	for (image_index = 0; image_index < image_count; ++image_index) {
		mh = _dyld_get_image_header(image_index);
		if (mh == NULL)
			continue;

		ret = getsectiondata((const struct mach_header_64 *)mh,
		    segname, sectname, size);
		if (ret != NULL)
			return (ret);
	}

	return (NULL);
}

#define __SET_RANGE(set)	({					\
	unsigned long __set_size;					\
	char *__set_data;						\
	__set_data = __set_getsectiondata("__DATA",			\
	    "set_" #set, &__set_size);					\
	(struct {							\
		__CONCAT(__typeof_set_,set)	**begin;		\
		__CONCAT(__typeof_set_,set)	**limit;		\
	}){								\
		.begin = (__CONCAT(__typeof_set_,set) **)__set_data,	\
		.limit = (__CONCAT(__typeof_set_,set) **)(__set_data +	\
		    __set_size)						\
	};								\
})

/*
 * Public macros.
 */
#define TEXT_SET(set, sym)	__MAKE_SET(set, sym)
#define DATA_SET(set, sym)	__MAKE_SET(set, sym)
#define DATA_WSET(set, sym)	__MAKE_SET_QV(set, sym, )
#define BSS_SET(set, sym)	__MAKE_SET(set, sym)
#define ABS_SET(set, sym)	__MAKE_SET(set, sym)
#define SET_ENTRY(set, sym)	__MAKE_SET(set, sym)

/*
 * Initialize before referring to a given linker set.
 */
#define SET_DECLARE(set, ptype)					\
	typedef ptype __CONCAT(__typeof_set_,set)

#define SET_BEGIN(set)							\
	(__SET_RANGE(set).begin)
#define SET_LIMIT(set)							\
	(__SET_RANGE(set).limit)

/*
 * Iterate over all the elements of a set.
 *
 * Sets always contain addresses of things, and "pvar" points to words
 * containing those addresses.  Thus is must be declared as "type **pvar",
 * and the address of each set item is obtained inside the loop by "*pvar".
 */
#define SET_FOREACH(pvar, set)						\
	for (pvar = SET_BEGIN(set); pvar < SET_LIMIT(set); pvar++)

#define SET_ITEM(set, i)						\
	((SET_BEGIN(set))[i])

/*
 * Provide a count of the items in a set.
 */
#define SET_COUNT(set)							\
	(SET_LIMIT(set) - SET_BEGIN(set))

#endif	/* _SYS_LINKER_SET_H_ */
