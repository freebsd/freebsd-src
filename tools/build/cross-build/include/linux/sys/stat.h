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
/*
 * glibc shipped with Ubuntu 16.04 doesn't include a definition of
 * struct timespec when sys/stat.h is included.
 */
#define __need_timespec
#include <time.h>

/* <bits/stat.h> contains a member named __unused. */
#include "../__unused_workaround_start.h"
#include_next <sys/stat.h>
#include "../__unused_workaround_end.h"

#define st_atimensec st_atim.tv_nsec
#define st_mtimensec st_mtim.tv_nsec
#define st_ctimensec st_ctim.tv_nsec

#define st_atimespec st_atim
#define st_mtimespec st_mtim
#define st_ctimespec st_ctim

#ifndef S_ISTXT
#define S_ISTXT S_ISVTX
#endif

#ifndef DEFFILEMODE
#define DEFFILEMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
#endif

#ifndef ALLPERMS
#define ALLPERMS (S_ISUID | S_ISGID | S_ISTXT | S_IRWXU | S_IRWXG | S_IRWXO)
#endif

#define	UF_SETTABLE	0x0000ffff
#define	UF_NODUMP	0x00000001
#define	UF_IMMUTABLE	0x00000002
#define	UF_APPEND	0x00000004
#define	UF_OPAQUE	0x00000008
#define	UF_NOUNLINK	0x00000010
#define	UF_SYSTEM	0x00000080
#define	UF_SPARSE	0x00000100
#define	UF_OFFLINE	0x00000200
#define	UF_REPARSE	0x00000400
#define	UF_ARCHIVE	0x00000800
#define	UF_READONLY	0x00001000
#define	UF_HIDDEN	0x00008000
#define	SF_SETTABLE	0xffff0000
#define	SF_ARCHIVED	0x00010000
#define	SF_IMMUTABLE	0x00020000
#define	SF_APPEND	0x00040000
#define	SF_NOUNLINK	0x00100000
#define	SF_SNAPSHOT	0x00200000

/* This include is needed for OpenZFS bootstrap */
#include <sys/mount.h>
