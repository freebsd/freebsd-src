/*-
 * Copyright (c) 2004 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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

#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <libgeom.h>
#include <geom/concat/g_concat.h>

#include "core/geom.h"
#include "misc/subr.h"


uint32_t lib_version = G_LIB_VERSION;
uint32_t version = G_CONCAT_VERSION;

static void concat_main(struct gctl_req *req, unsigned flags);
static void concat_label(struct gctl_req *req);
static void concat_clear(struct gctl_req *req);

struct g_command class_commands[] = {
	{ "create", G_FLAG_VERBOSE | G_FLAG_LOADKLD, NULL, G_NULL_OPTS },
	{ "destroy", G_FLAG_VERBOSE, NULL,
	    {
		{ 'f', "force", NULL, G_TYPE_NONE },
		G_OPT_SENTINEL
	    }
	},
	{ "label", G_FLAG_VERBOSE | G_FLAG_LOADKLD, concat_main, G_NULL_OPTS },
	{ "clear", G_FLAG_VERBOSE, concat_main, G_NULL_OPTS },
	G_CMD_SENTINEL
};

static int verbose = 0;


void usage(const char *name);
void
usage(const char *name)
{

	fprintf(stderr, "usage: %s create [-v] <name> <dev1> <dev2> [dev3 [...]]\n", name);
	fprintf(stderr, "       %s destroy [-fv] <name> [name2 [...]]\n", name);
	fprintf(stderr, "       %s label [-v] <name> <dev1> <dev2> [dev3 [...]]\n", name);
	fprintf(stderr, "       %s clear [-v] <dev1> [dev2 [...]]\n", name);
}

static void
concat_main(struct gctl_req *req, unsigned flags)
{
	const char *name;

	if ((flags & G_FLAG_VERBOSE) != 0)
		verbose = 1;

	name = gctl_get_asciiparam(req, "verb");
	if (name == NULL) {
		gctl_error(req, "No '%s' argument.", "verb");
		return;
	}
	if (strcmp(name, "label") == 0)
		concat_label(req);
	else if (strcmp(name, "clear") == 0)
		concat_clear(req);
	else
		gctl_error(req, "Unknown command: %s.", name);
}

static void
concat_label(struct gctl_req *req)
{
	struct g_concat_metadata md;
	u_char sector[512];
	const char *name;
	char param[16];
	unsigned i;
	int *nargs, error;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs <= 2) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	/*
	 * Clear last sector first to spoil all components if device exists.
	 */
	for (i = 1; i < (unsigned)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);

		error = g_metadata_clear(name, NULL);
		if (error != 0) {
			gctl_error(req, "Can't store metadata on %s: %s.", name,
			    strerror(error));
			return;
		}
	}

	strlcpy(md.md_magic, G_CONCAT_MAGIC, sizeof(md.md_magic));
	md.md_version = G_CONCAT_VERSION;
	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	strlcpy(md.md_name, name, sizeof(md.md_name));
	md.md_id = arc4random();
	md.md_all = *nargs - 1;

	/*
	 * Ok, store metadata.
	 */
	for (i = 1; i < (unsigned)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);

		md.md_no = i - 1;
		concat_metadata_encode(&md, sector);
		error = g_metadata_store(name, sector, sizeof(sector));
		if (error != 0) {
			fprintf(stderr, "Can't store metadata on %s: %s.", name,
			    strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (verbose)
			printf("Metadata value stored on %s.\n", name);
	}
}

static void
concat_clear(struct gctl_req *req)
{
	const char *name;
	char param[16];
	unsigned i;
	int *nargs, error;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 1) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	for (i = 0; i < (unsigned)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);

		error = g_metadata_clear(name, G_CONCAT_MAGIC);
		if (error != 0) {
			fprintf(stderr, "Can't clear metadata on %s: %s.", name,
			    strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (verbose)
			printf("Metadata cleared on %s.\n", name); 
	}
}
