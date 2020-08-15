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

/*
 * When building pwd_mkdb we need to use target systems definition of
 * struct passwd. This protects against future changes to struct passwd and
 * is essential to allow cross-building from Linux/macOS hosts since the
 * structure is not compatible there.
 */
#include <stdint.h>
#include <stddef.h>
/*
 * Note: pwd_mkdb always stores uint32_t for all integer fields (including
 * time_t!) so these definitions do not need to match sys/sys/_types.h
 */
typedef	uint32_t	_bootstrap_gid_t;
typedef	uint32_t	_bootstrap_uid_t;
typedef	uint64_t	_bootstrap_time_t;
#define	_GID_T_DECLARED
#define	_TIME_T_DECLARED
#define	_UID_T_DECLARED
#define	_SIZE_T_DECLARED

#define	gid_t	_bootstrap_gid_t
#define	uid_t	_bootstrap_uid_t
#define	time_t	_bootstrap_time_t
#define	passwd	_bootstrap_passwd
#include "../../../include/pwd.h"
#undef gid_t
#undef uid_t
#undef time_t
