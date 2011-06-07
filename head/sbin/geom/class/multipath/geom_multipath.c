/*-
 * Copyright (c) 2006 Mathew Jacob <mjacob@FreeBSD.org>
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
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <libgeom.h>
#include <uuid.h>
#include <geom/multipath/g_multipath.h>

#include "core/geom.h"
#include "misc/subr.h"

uint32_t lib_version = G_LIB_VERSION;
uint32_t version = G_MULTIPATH_VERSION;

static void mp_main(struct gctl_req *, unsigned int);
static void mp_label(struct gctl_req *);
static void mp_clear(struct gctl_req *);
static void mp_add(struct gctl_req *);

struct g_command class_commands[] = {
	{
		"label", G_FLAG_VERBOSE | G_FLAG_LOADKLD, mp_main, G_NULL_OPTS,
		"[-v] name prov ..."
	},
	{
		"add", G_FLAG_VERBOSE | G_FLAG_LOADKLD, mp_main, G_NULL_OPTS,
		"[-v] name prov ..."
	},
	{
		"destroy", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
		"[-v] prov ..."
	},
	{
		"clear", G_FLAG_VERBOSE, mp_main, G_NULL_OPTS,
		"[-v] prov ..."
	},
	{
		"rotate", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
		"[-v] prov ..."
	},
	{
		"getactive", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
		"[-v] prov ..."
	},
	G_CMD_SENTINEL
};

static void
mp_main(struct gctl_req *req, unsigned int flags __unused)
{
	const char *name;

	name = gctl_get_ascii(req, "verb");
	if (name == NULL) {
		gctl_error(req, "No '%s' argument.", "verb");
		return;
	}
	if (strcmp(name, "label") == 0) {
		mp_label(req);
	} else if (strcmp(name, "add") == 0) {
		mp_add(req);
	} else if (strcmp(name, "clear") == 0) {
		mp_clear(req);
	} else {
		gctl_error(req, "Unknown command: %s.", name);
	}
}

static void
mp_label(struct gctl_req *req)
{
	struct g_multipath_metadata md;
	off_t disksiz = 0, msize;
	uint8_t *sector;
	char *ptr;
	uuid_t uuid;
	uint32_t secsize = 0, ssize, status;
	const char *name, *mpname;
	int error, i, nargs;

	nargs = gctl_get_int(req, "nargs");
	if (nargs < 2) {
		gctl_error(req, "wrong number of arguments.");
		return;
	}

	/*
	 * First, check each provider to make sure it's the same size.
	 * This also gets us our size and sectorsize for the metadata.
	 */
	for (i = 1; i < nargs; i++) {
		name = gctl_get_ascii(req, "arg%d", i);
		msize = g_get_mediasize(name);
		ssize = g_get_sectorsize(name);
		if (msize == 0 || ssize == 0) {
			gctl_error(req, "cannot get information about %s: %s.",
			    name, strerror(errno));
			return;
		}
		if (i == 1) {
			secsize = ssize;
			disksiz	= msize;
		} else {
			if (secsize != ssize) {
				gctl_error(req, "%s sector size %u different.",
				    name, ssize);
				return;
			}
			if (disksiz != msize) {
				gctl_error(req, "%s media size %ju different.",
				    name, (intmax_t)msize);
				return;
			}
		}
		
	}

	/*
	 * Generate metadata.
	 */
	strlcpy(md.md_magic, G_MULTIPATH_MAGIC, sizeof(md.md_magic));
	md.md_version = G_MULTIPATH_VERSION;
	mpname = gctl_get_ascii(req, "arg0");
	strlcpy(md.md_name, mpname, sizeof(md.md_name));
	md.md_size = disksiz;
	md.md_sectorsize = secsize;
	uuid_create(&uuid, &status);
	if (status != uuid_s_ok) {
		gctl_error(req, "cannot create a UUID.");
		return;
	}
	uuid_to_string(&uuid, &ptr, &status);
	if (status != uuid_s_ok) {
		gctl_error(req, "cannot stringify a UUID.");
		return;
	}
	strlcpy(md.md_uuid, ptr, sizeof (md.md_uuid));
	free(ptr);

	/*
	 * Clear metadata on initial provider first.
	 */
	name = gctl_get_ascii(req, "arg1");
	error = g_metadata_clear(name, NULL);
	if (error != 0) {
		gctl_error(req, "cannot clear metadata on %s: %s.", name, strerror(error));
		return;
	}

	/*
	 * Allocate a sector to write as metadata.
	 */
	sector = malloc(secsize);
	if (sector == NULL) {
		gctl_error(req, "unable to allocate metadata buffer");
		return;
	}
	memset(sector, 0, secsize);

	/*
	 * encode the metadata
	 */
	multipath_metadata_encode(&md, sector);

	/*
	 * Store metadata on the initial provider.
	 */
	error = g_metadata_store(name, sector, secsize);
	if (error != 0) {
		gctl_error(req, "cannot store metadata on %s: %s.", name, strerror(error));
		return;
	}

	/*
	 * Now add the rest of the providers.
	 */
	error = gctl_change_param(req, "verb", -1, "add");
	if (error) {
		gctl_error(req, "unable to change verb to \"add\": %s.", strerror(error));
		return;
	}
	for (i = 2; i < nargs; i++) {
		error = gctl_change_param(req, "arg1", -1, gctl_get_ascii(req, "arg%d", i));
		if (error) {
			gctl_error(req, "unable to add %s to %s: %s.", gctl_get_ascii(req, "arg%d", i), mpname, strerror(error));
			continue;
		}
		mp_add(req);
	}
}


static void
mp_clear(struct gctl_req *req)
{
	const char *name;
	int error, i, nargs;

	nargs = gctl_get_int(req, "nargs");
	if (nargs < 1) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	for (i = 0; i < nargs; i++) {
		name = gctl_get_ascii(req, "arg%d", i);
		error = g_metadata_clear(name, G_MULTIPATH_MAGIC);
		if (error != 0) {
			fprintf(stderr, "Can't clear metadata on %s: %s.\n",
			    name, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
	}
}

static void
mp_add(struct gctl_req *req)
{
	const char *errstr;

	errstr = gctl_issue(req);
	if (errstr != NULL && errstr[0] != '\0') {
		gctl_error(req, "%s", errstr);
	}
}
