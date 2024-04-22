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
 * This work was supported by Innovate UK project 105694, "Digital Security by
 * Design (DSbD) Technology Platform Prototype".
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

#include_next <sys/param.h>
#include <sys/types.h>

#ifndef BLKDEV_IOSIZE
#define BLKDEV_IOSIZE PAGE_SIZE /* default block device I/O size */
#endif
#ifndef DFLTPHYS
#define DFLTPHYS (64 * 1024) /* default max raw I/O transfer size */
#endif
#ifndef MAXPHYS
#define MAXPHYS (128 * 1024) /* max raw I/O transfer size */
#endif
#ifndef MAXDUMPPGS
#define MAXDUMPPGS (DFLTPHYS / PAGE_SIZE)
#endif

#ifndef MCLSHIFT
#define MCLSHIFT 11 /* convert bytes to mbuf clusters */
#endif

#ifndef MCLBYTES
#define MCLBYTES (1 << MCLSHIFT) /* size of an mbuf cluster */
#endif

#ifndef __PAST_END
#define __PAST_END(array, offset) (((__typeof__(*(array)) *)(array))[offset])
#endif

#ifndef nitems
// https://stackoverflow.com/questions/1598773/is-there-a-standard-function-in-c-that-would-return-the-length-of-an-array/1598827#1598827
#define nitems(x) \
	((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))
#endif

#ifndef rounddown
#define rounddown(x, y) (((x) / (y)) * (y))
#endif
#ifndef rounddown2
#define rounddown2(x, y) ((x) & (~((y)-1))) /* if y is power of two */
#endif
#ifndef roundup
#define roundup(x, y) ((((x) + ((y)-1)) / (y)) * (y)) /* to any y */
#endif
#ifndef roundup2
#define roundup2(x, y) \
	(((x) + ((y)-1)) & (~((y)-1))) /* if y is powers of two */
#endif
#ifndef powerof2
#define powerof2(x) ((((x)-1) & (x)) == 0)
#endif
