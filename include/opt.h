/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2020 Gavin D. Howard and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************************************************************
 *
 * Adapted from https://github.com/skeeto/optparse
 *
 * *****************************************************************************
 *
 * Definitions for getopt_long() replacement.
 *
 */

#ifndef BC_OPT_H
#define BC_OPT_H

#include <stdbool.h>

typedef struct BcOpt {
	char **argv;
	size_t optind;
	int optopt;
	int subopt;
	char *optarg;
} BcOpt;

typedef enum BcOptType {
	BC_OPT_NONE,
	BC_OPT_REQUIRED,
	BC_OPT_BC_ONLY,
	BC_OPT_DC_ONLY,
} BcOptType;

typedef struct BcOptLong {
	const char *name;
	BcOptType type;
	int val;
} BcOptLong;

void bc_opt_init(BcOpt *o, char **argv);

int bc_opt_parse(BcOpt *o, const BcOptLong *longopts);

#define BC_OPT_ISDASHDASH(a) \
	((a) != NULL && (a)[0] == '-' && (a)[1] == '-' && (a)[2] == '\0')

#define BC_OPT_ISSHORTOPT(a) \
	((a) != NULL && (a)[0] == '-' && (a)[1] != '-' && (a)[1] != '\0')

#define BC_OPT_ISLONGOPT(a) \
	((a) != NULL && (a)[0] == '-' && (a)[1] == '-' && (a)[2] != '\0')

#endif // BC_OPT_H
