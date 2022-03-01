/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Marshall Kirk McKusick <mckusick@mckusick.com>
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

#include <stdio.h>
#include <stdint.h>
#include <libgeom.h>
#include <geom/union/g_union.h>

#include "core/geom.h"

uint32_t lib_version = G_LIB_VERSION;
uint32_t version = G_UNION_VERSION;

struct g_command class_commands[] = {
	{ "create", G_FLAG_LOADKLD, NULL,
	    {
		{ 'o', "offset", "0", G_TYPE_NUMBER },
		{ 's', "size", "0", G_TYPE_NUMBER },
		{ 'S', "secsize", "0", G_TYPE_NUMBER },
		{ 'v', "verbose", NULL, G_TYPE_BOOL },
		{ 'Z', "gunionname", G_VAL_OPTIONAL, G_TYPE_STRING },
		G_OPT_SENTINEL
	    },
	    "[-v] [-o offset] [-s size] [-S secsize] [-Z gunionname] "
	    "upperdev lowerdev"
	},
	{ "destroy", 0, NULL,
	    {
		{ 'f', "force", NULL, G_TYPE_BOOL },
		{ 'v', "verbose", NULL, G_TYPE_BOOL },
		G_OPT_SENTINEL
	    },
	    "[-fv] prov ..."
	},
	{ "reset", 0, NULL,
	    {
		{ 'v', "verbose", NULL, G_TYPE_BOOL },
		G_OPT_SENTINEL
	    },
	    "[-v] prov ..."
	},
	{ "commit", 0, NULL,
	    {
		{ 'f', "force", NULL, G_TYPE_BOOL },
		{ 'r', "reboot", NULL, G_TYPE_BOOL },
		{ 'v', "verbose", NULL, G_TYPE_BOOL },
		G_OPT_SENTINEL
	    },
	    "[-frv] prov ..."
	},
	{ "revert", 0, NULL,
	    {
		{ 'v', "verbose", NULL, G_TYPE_BOOL },
		G_OPT_SENTINEL
	    },
	    "[-v] prov ..."
	},
	G_CMD_SENTINEL
};
