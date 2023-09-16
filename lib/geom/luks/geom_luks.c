/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2023 Nicolas Provost <dev@npsoft.fr>
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
 */

#include <sys/cdefs.h>
#include <stdio.h>
#include <stdint.h>
#include <libgeom.h>

#include "core/geom.h"


uint32_t lib_version = G_LIB_VERSION;
uint32_t version = 1;

struct g_command class_commands[] = {
	{ "open", G_FLAG_VERBOSE | G_FLAG_LOADKLD, NULL,
	    {
		{ 'h', "hex", NULL, G_TYPE_BOOL },
		{ 'r', "readonly", NULL, G_TYPE_BOOL },
		{ 'o', "onewriter", NULL, G_TYPE_BOOL },
		G_OPT_SENTINEL
	    },
	    "[-h] [-r] [-o] device passphrase"
	},
	{ "format", G_FLAG_VERBOSE | G_FLAG_LOADKLD, NULL,
	    {
		{ 'h', "hex", NULL, G_TYPE_BOOL },
		{ 'y', "yes", NULL, G_TYPE_BOOL },
		G_OPT_SENTINEL
	    },
	    "[-h] -y device scheme passphrase"
	},
	{ "info", G_FLAG_VERBOSE | G_FLAG_LOADKLD, NULL,
	    {
		{ 'q', "quiet", NULL, G_TYPE_BOOL },
		G_OPT_SENTINEL
	    },
	    "[-q] device"
	},
	{ "close", G_FLAG_VERBOSE | G_FLAG_LOADKLD, NULL,
	    {
		G_OPT_SENTINEL
	    },
	    "device"
	},
	G_CMD_SENTINEL
};
