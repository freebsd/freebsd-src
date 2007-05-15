/*-
 * Copyright (c) 2007 Marcel Moolenaar
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
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <readpassphrase.h>
#include <string.h>
#include <strings.h>
#include <libgeom.h>
#include <paths.h>
#include <errno.h>
#include <assert.h>

#include "core/geom.h"
#include "misc/subr.h"

uint32_t lib_version = G_LIB_VERSION;
uint32_t version = 0;

static char optional[] = "";
static char flags[] = "C";

struct g_command class_commands[] = {
	{ "add", 0, NULL, {
		{ 'b', "start", NULL, G_TYPE_STRING },
		{ 's', "size", NULL, G_TYPE_STRING },
		{ 't', "type", NULL, G_TYPE_STRING },
		{ 'i', "index", optional, G_TYPE_STRING },
		{ 'l', "label", optional, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL,
	},
	{ "commit", 0, NULL, G_NULL_OPTS, "geom", NULL },
	{ "create", 0, NULL, {
		{ 's', "scheme", NULL, G_TYPE_STRING },
		{ 'n', "entries", optional, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "provider", NULL
	},
	{ "delete", 0, NULL, {
		{ 'i', "index", NULL, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
	},
	{ "destroy", 0, NULL, {
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL },
	{ "modify", 0, NULL, {
		{ 'i', "index", NULL, G_TYPE_STRING },
		{ 'l', "label", optional, G_TYPE_STRING },
		{ 't', "type", optional, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
	},
	{ "undo", 0, NULL, G_NULL_OPTS, "geom", NULL },
	G_CMD_SENTINEL
};
