/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	VXGE_OS_PAL_H
#define	VXGE_OS_PAL_H

__EXTERN_BEGIN_DECLS

/* --------------------------- platform switch ------------------------------ */

/* platform specific header */
#include <dev/vxge/vxge-osdep.h>
#define	IN
#define	OUT

#if !defined(VXGE_OS_PLATFORM_64BIT) && !defined(VXGE_OS_PLATFORM_32BIT)
#error "either 32bit or 64bit switch must be defined!"
#endif

#if !defined(VXGE_OS_HOST_BIG_ENDIAN) && !defined(VXGE_OS_HOST_LITTLE_ENDIAN)
#error "either little endian or big endian switch must be defined!"
#endif

#if defined(VXGE_OS_PLATFORM_64BIT)
#define	VXGE_OS_MEMORY_DEADCODE_PAT		0x5a5a5a5a5a5a5a5a
#else
#define	VXGE_OS_MEMORY_DEADCODE_PAT		0x5a5a5a5a
#endif

#if defined(VXGE_DEBUG_ASSERT)

/*
 * vxge_assert
 * @test: C-condition to check
 * @fmt: printf like format string
 *
 * This function implements traditional assert. By default assertions
 * are enabled. It can be disabled by defining VXGE_DEBUG_ASSERT macro in
 * compilation
 * time.
 */
#define	vxge_assert(test) { \
	if (!(test)) vxge_os_bug("bad cond: "#test" at %s:%d\n", \
	__FILE__, __LINE__); }
#else
#define	vxge_assert(test)
#endif	/* end of VXGE_DEBUG_ASSERT */

__EXTERN_END_DECLS

#endif	/* VXGE_OS_PAL_H */
