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
 *
 * $FreeBSD$
 */
#pragma once
#include_next <sys/types.h>
#include <sys/_types.h>
/*
 * elftoolchain includes sys/elf32.h which expects that uint32_t is defined
 * However, it only includes <sys/types.h> and not <stdint.h>
 */
#include <stdint.h>

#if __has_include(<sys/sysmacros.h>)
/* GLibc defines makedev/minor/major in sysmacros.h instead of sys/types.h */
#include <sys/sysmacros.h>
#endif

typedef __UINTPTR_TYPE__ __uintptr_t;
typedef __INTPTR_TYPE__ __intptr_t;

/* needed for gencat */
typedef int __nl_item;

typedef size_t u_register_t;

/* capsicum compat: */
#ifndef _CAP_IOCTL_T_DECLARED
#define _CAP_IOCTL_T_DECLARED
typedef unsigned long cap_ioctl_t;
#endif

#ifndef _CAP_RIGHTS_T_DECLARED
#define _CAP_RIGHTS_T_DECLARED
struct cap_rights;

typedef struct cap_rights cap_rights_t;

/*
 * make.py uses these headers during the bmake bootstrap on Linux only, at
 * which point sys/bitcount.h won't yet exist, so don't include it there.
 *
 * TODO: Untangle this mess.
 */
#if __has_include(<sys/bitcount.h>)
/* Needed for bitstring */
#include <sys/bitcount.h>
#endif

#endif
