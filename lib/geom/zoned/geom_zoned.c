/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 voidanix <voidanix@FreeBSD.org>
 */

#include <sys/param.h>

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libgeom.h>
#include <geom/zoned/g_zoned.h>

#include "core/geom.h"
#include "misc/subr.h"

uint32_t lib_version = G_LIB_VERSION;
uint32_t version = G_ZONED_VERSION;

#define GZONED_ZONESIZE		"256M"

static void zoned_main(struct gctl_req *req, unsigned flags);
static void zoned_create(struct gctl_req *req);
static void zoned_clear(struct gctl_req *req);

struct g_command class_commands[] = {
	{ "clear", G_FLAG_VERBOSE, zoned_main, G_NULL_OPTS, "[-v] dev ..." },
	{ "create", G_FLAG_VERBOSE | G_FLAG_LOADKLD, zoned_main,
	    {
		{ 's', "zonesize", GZONED_ZONESIZE, G_TYPE_NUMBER },
		{ 'c', "conventional", "", G_TYPE_STRING },
		{ 'm', "maxopen", "0", G_TYPE_NUMBER },
		{ 'u', "restricted", NULL, G_TYPE_BOOL },
		G_OPT_SENTINEL
	    },
	    "[-uv] [-s zonesize] [-c zone[-zone][,zone[-zone]]...] "
	    "[-m maxopen] dev"
	},
	{ "fault", G_FLAG_VERBOSE, NULL,
	    {
		{ 'z', "zone", NULL, G_TYPE_NUMBER },
		{ 's', "state", NULL, G_TYPE_STRING },
		G_OPT_SENTINEL
	    },
	    "[-v] -z zone -s ro|offline|reset|clear name"
	},
	{ "destroy", G_FLAG_VERBOSE, NULL,
		{
			{ 'f', "force", NULL, G_TYPE_BOOL },
			G_OPT_SENTINEL
		},
		"[-fv] name ..." },
	{ "stop", G_FLAG_VERBOSE, NULL,
		{
			{ 'f', "force", NULL, G_TYPE_BOOL },
			G_OPT_SENTINEL
		},
		"[-fv] name ..." },
	G_CMD_SENTINEL
};

static int verbose = 0;

static void
zoned_main(struct gctl_req *req, unsigned flags)
{
	const char *name;

	if ((flags & G_FLAG_VERBOSE) != 0)
		verbose = 1;

	name = gctl_get_ascii(req, "verb");
	if (name == NULL) {
		gctl_error(req, "No '%s' argument.", "verb");
		return;
	}
	if (strcmp(name, "create") == 0)
		zoned_create(req);
	else if (strcmp(name, "clear") == 0)
		zoned_clear(req);
	else
		gctl_error(req, "Unknown command: %s.", name);
}

/*
 * Parse a conventional-zone specification into md_conv/md_nconv.
 */
static int
zoned_parse_conv(struct gctl_req *req, const char *spec, uint32_t nzones,
    struct g_zoned_metadata *md)
{
	char *buf, *ep, *sp, *tok;
	unsigned long first, last;

	md->md_nconv = 0;
	if (*spec == '\0')
		return (0);
	buf = strdup(spec);
	if (buf == NULL) {
		gctl_error(req, "Cannot allocate memory.");
		return (-1);
	}
	for (tok = strtok_r(buf, ",", &sp); tok != NULL;
	    tok = strtok_r(NULL, ",", &sp)) {
		if (md->md_nconv == G_ZONED_MAXCONV) {
			gctl_error(req,
			    "Too many conventional zone ranges (max %d).",
			    G_ZONED_MAXCONV);
			goto fail;
		}
		errno = 0;
		if (!isdigit((unsigned char)*tok))
			goto syntax;
		first = strtoul(tok, &ep, 10);
		if (ep == tok || errno != 0)
			goto syntax;
		if (*ep == '-') {
			tok = ep + 1;
			if (!isdigit((unsigned char)*tok))
				goto syntax;
			last = strtoul(tok, &ep, 10);
			if (ep == tok || *ep != '\0' || errno != 0)
				goto syntax;
		} else if (*ep == '\0') {
			last = first;
		} else {
			goto syntax;
		}
		if (last < first) {
			gctl_error(req, "Backwards zone range %lu-%lu.",
			    first, last);
			goto fail;
		}
		if (last >= nzones) {
			gctl_error(req,
			    "Zone %lu does not exist (zones 0-%u).", last,
			    nzones - 1);
			goto fail;
		}
		md->md_conv[md->md_nconv].cr_first = first;
		md->md_conv[md->md_nconv].cr_count = last - first + 1;
		md->md_nconv++;
	}
	free(buf);
	return (0);
syntax:
	gctl_error(req, "Invalid conventional zone specification '%s'.",
	    spec);
fail:
	free(buf);
	return (-1);
}

static void
zoned_create(struct gctl_req *req)
{
	struct g_zoned_metadata md;
	u_char sector[512];
	const char *conv, *dev;
	off_t msize, zonesize;
	intmax_t maxopen;
	unsigned int secsize;
	uint32_t i, nconv, nzones;
	int error, nargs;

	bzero(sector, sizeof(sector));
	bzero(&md, sizeof(md));
	nargs = gctl_get_int(req, "nargs");
	if (nargs != 1) {
		gctl_error(req,
		    "Usage: create [-u] [-s zonesize] [-c zones] "
		    "[-m maxopen] dev");
		return;
	}
	zonesize = (off_t)gctl_get_intmax(req, "zonesize");
	conv = gctl_get_ascii(req, "conventional");
	dev = gctl_get_ascii(req, "arg0");

	msize = g_get_mediasize(dev);
	secsize = g_get_sectorsize(dev);
	if (msize == 0 || secsize == 0) {
		gctl_error(req, "Can't get information about %s: %s.", dev,
		    strerror(errno));
		return;
	}
	if (zonesize <= 0 || (zonesize % secsize) != 0) {
		gctl_error(req, "Zone size must be a positive multiple of %u.",
		    secsize);
		return;
	}
	nzones = g_zoned_nzones(msize, zonesize, secsize);
	if (nzones == 0) {
		gctl_error(req, "Zone size %jd is too large for %s.",
		    (intmax_t)zonesize, dev);
		return;
	}
	if (zoned_parse_conv(req, conv, nzones, &md) != 0)
		return;
	maxopen = gctl_get_intmax(req, "maxopen");
	if (maxopen < 0 || maxopen > nzones) {
		gctl_error(req, "Maximum open zones must be between 0 and "
		    "the number of zones (%u).", nzones);
		return;
	}

	strlcpy(md.md_magic, G_ZONED_MAGIC, sizeof(md.md_magic));
	md.md_version = G_ZONED_VERSION;
	md.md_id = arc4random();
	md.md_zonesize = zonesize;
	md.md_nzones = nzones;
	md.md_sectorsize = secsize;
	md.md_provsize = msize;
	md.md_maxopen = (uint32_t)maxopen;
	if (gctl_get_int(req, "restricted"))
		md.md_flags |= G_ZONED_MD_RESTRICTED_READS;

	zoned_metadata_encode(&md, sector);
	error = g_metadata_store(dev, sector, sizeof(sector));
	if (error != 0) {
		gctl_error(req, "Can't store metadata on %s: %s.", dev,
		    strerror(error));
		return;
	}
	if (verbose) {
		nconv = 0;
		for (i = 0; i < md.md_nconv; i++)
			nconv += md.md_conv[i].cr_count;
		printf("Device %s zoned: %u zones of %jd bytes"
		    " (%u conventional).\n", dev, nzones, (intmax_t)zonesize,
		    nconv);
	}
}

static void
zoned_clear(struct gctl_req *req)
{
	const char *name;
	int error, i, nargs;

	nargs = gctl_get_int(req, "nargs");
	if (nargs < 1) {
		gctl_error(req, "Missing device(s).");
		return;
	}

	for (i = 0; i < nargs; i++) {
		name = gctl_get_ascii(req, "arg%d", i);
		error = g_metadata_clear(name, G_ZONED_MAGIC);
		if (error != 0) {
			fprintf(stderr, "Can't clear metadata on %s: %s.\n",
			    name, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (verbose)
			printf("Metadata cleared on %s.\n", name);
	}
}
