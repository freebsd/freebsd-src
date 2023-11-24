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

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <stdint.h>
#include <stdio.h>

struct pidfh;

__BEGIN_DECLS
int humanize_number(char *buf, size_t len, int64_t bytes, const char *suffix,
    int scale, int flags);
int expand_number(const char *_buf, uint64_t *_num);

int flopen(const char *_path, int _flags, ...);
int flopenat(int dirfd, const char *path, int flags, ...);

char *fparseln(FILE *, size_t *, size_t *, const char[3], int);
__END_DECLS

/* Values for humanize_number(3)'s flags parameter. */
#define HN_DECIMAL 0x01
#define HN_NOSPACE 0x02
#define HN_B 0x04
#define HN_DIVISOR_1000 0x08
#define HN_IEC_PREFIXES 0x10

/* Values for humanize_number(3)'s scale parameter. */
#define HN_GETSCALE 0x10
#define HN_AUTOSCALE 0x20

/*
 * fparseln() specific operation flags.
 */
#define FPARSELN_UNESCESC 0x01
#define FPARSELN_UNESCCONT 0x02
#define FPARSELN_UNESCCOMM 0x04
#define FPARSELN_UNESCREST 0x08
#define FPARSELN_UNESCALL 0x0f
