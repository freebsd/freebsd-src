/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2018-2020 Alex Richardson <arichardson@FreeBSD.org>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
#pragma once

#include <sys/cdefs.h> /* FreeBSD source assumes sys/types.h includes this */
/*
 * MUSL doesn't define the __intXX_t that FreeBSD does, but many of our headers
 * assume that will always be present. Define them here. We assume !defined
 * __GLIBC__ is musl since musl doesn't have a define to key off of. Thesee
 * typedefs look backwards, but it's not circular because MUSL never defines the
 * __*int*_t. Also, we don't have to work in the kernel, so it's OK to include
 * stdint.h here.
 */
#ifndef __GLIBC__
#include <stdint.h>
typedef int64_t  __int64_t;
typedef int32_t  __int32_t;
typedef int16_t  __int16_t;
typedef int8_t   __int8_t;
typedef uint64_t __uint64_t;
typedef uint32_t __uint32_t;
typedef uint16_t __uint16_t;
typedef uint8_t  __uint8_t;
#endif

#include_next <sys/types.h>

/*
 * stddef.h for both gcc and clang will define __size_t when size_t has
 * been defined (except on *BSD where it doesn't touch __size_t). So if
 * we're building on Linux, we know that if that's not defined, we have
 * to typedef __size_t for FreeBSD's use of __size_t in places to work
 * during bootstrapping.
 */
#ifndef __size_t
typedef __SIZE_TYPE__ __size_t;
#endif
