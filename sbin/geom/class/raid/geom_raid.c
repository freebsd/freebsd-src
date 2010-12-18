/*-
 * Copyright (c) 2004-2009 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * Copyright (c) 2010 Alexander Motin <mav@FreeBSD.org>
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
#include <geom/raid/g_raid.h>
#include <core/geom.h>
#include <misc/subr.h>

uint32_t lib_version = G_LIB_VERSION;
uint32_t version = G_RAID_VERSION;

#define	GRAID_BALANCE		"load"
#define	GRAID_SLICE		"4096"
#define	GRAID_PRIORITY	"0"

#if 0
static void raid_main(struct gctl_req *req, unsigned flags);
static void raid_activate(struct gctl_req *req);
static void raid_clear(struct gctl_req *req);
static void raid_dump(struct gctl_req *req);
static void raid_label(struct gctl_req *req);
#endif

struct g_command class_commands[] = {
/*
	{ "activate", G_FLAG_VERBOSE, raid_main, G_NULL_OPTS,
	    "[-v] name prov ..."
	},
	{ "clear", G_FLAG_VERBOSE, raid_main, G_NULL_OPTS,
	    "[-v] prov ..."
	},
	{ "configure", G_FLAG_VERBOSE, NULL,
	    {
		{ 'a', "autosync", NULL, G_TYPE_BOOL },
		{ 'b', "balance", "", G_TYPE_STRING },
		{ 'd', "dynamic", NULL, G_TYPE_BOOL },
		{ 'f', "failsync", NULL, G_TYPE_BOOL },
		{ 'F', "nofailsync", NULL, G_TYPE_BOOL },
		{ 'h', "hardcode", NULL, G_TYPE_BOOL },
		{ 'n', "noautosync", NULL, G_TYPE_BOOL },
		{ 'p', "priority", "-1", G_TYPE_NUMBER },
		{ 's', "slice", "-1", G_TYPE_NUMBER },
		G_OPT_SENTINEL
	    },
	    "[-adfFhnv] [-b balance] [-s slice] name\n"
	    "[-v] -p priority name prov"
	},
	{ "deactivate", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
	    "[-v] name prov ..."
	},
	{ "dump", 0, raid_main, G_NULL_OPTS,
	    "prov ..."
	},
	{ "forget", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
	    "name ..."
	},
	{ "label", G_FLAG_VERBOSE, raid_main,
	    {
		{ 'b', "balance", GRAID_BALANCE, G_TYPE_STRING },
		{ 'F', "nofailsync", NULL, G_TYPE_BOOL },
		{ 'h', "hardcode", NULL, G_TYPE_BOOL },
		{ 'n', "noautosync", NULL, G_TYPE_BOOL },
		{ 's', "slice", GRAID_SLICE, G_TYPE_NUMBER },
		G_OPT_SENTINEL
	    },
	    "[-Fhnv] [-b balance] [-s slice] name prov ..."
	},
	{ "insert", G_FLAG_VERBOSE, NULL,
	    {
		{ 'h', "hardcode", NULL, G_TYPE_BOOL },
		{ 'i', "inactive", NULL, G_TYPE_BOOL },
		{ 'p', "priority", GRAID_PRIORITY, G_TYPE_NUMBER },
		G_OPT_SENTINEL
	    },
	    "[-hiv] [-p priority] name prov ..."
	},
	{ "rebuild", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
	    "[-v] name prov ..."
	},
	{ "remove", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
	    "[-v] name prov ..."
	},
	{ "stop", G_FLAG_VERBOSE, NULL,
	    {
		{ 'f', "force", NULL, G_TYPE_BOOL },
		G_OPT_SENTINEL
	    },
	    "[-fv] name ..."
	},
*/
	G_CMD_SENTINEL
};

#if 0
static int verbose = 0;

static void
raid_main(struct gctl_req *req, unsigned flags)
{
	const char *name;

	if ((flags & G_FLAG_VERBOSE) != 0)
		verbose = 1;

	name = gctl_get_ascii(req, "verb");
	if (name == NULL) {
		gctl_error(req, "No '%s' argument.", "verb");
		return;
	}
#if 0
	if (strcmp(name, "label") == 0)
		raid_label(req);
	else if (strcmp(name, "clear") == 0)
		raid_clear(req);
	else if (strcmp(name, "dump") == 0)
		raid_dump(req);
	else if (strcmp(name, "activate") == 0)
		raid_activate(req);
	else
#endif
		gctl_error(req, "Unknown command: %s.", name);
}

static void
raid_label(struct gctl_req *req)
{
	struct g_raid_metadata md;
	u_char sector[512];
	const char *str;
	unsigned sectorsize;
	off_t mediasize;
	intmax_t val;
	int error, i, nargs, bal, hardcode;

	nargs = gctl_get_int(req, "nargs");
	if (nargs < 2) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	strlcpy(md.md_magic, G_RAID_MAGIC, sizeof(md.md_magic));
	md.md_version = G_RAID_VERSION;
	str = gctl_get_ascii(req, "arg0");
	strlcpy(md.md_name, str, sizeof(md.md_name));
	md.md_mid = arc4random();
	md.md_all = nargs - 1;
	md.md_mflags = 0;
	md.md_dflags = 0;
	md.md_genid = 0;
	md.md_syncid = 1;
	md.md_sync_offset = 0;
	val = gctl_get_intmax(req, "slice");
	md.md_slice = val;
	str = gctl_get_ascii(req, "balance");
	bal = balance_id(str);
	if (bal == -1) {
		gctl_error(req, "Invalid balance algorithm.");
		return;
	}
	md.md_balance = bal;
	if (gctl_get_int(req, "noautosync"))
		md.md_mflags |= G_RAID_DEVICE_FLAG_NOAUTOSYNC;
	if (gctl_get_int(req, "nofailsync"))
		md.md_mflags |= G_RAID_DEVICE_FLAG_NOFAILSYNC;
	hardcode = gctl_get_int(req, "hardcode");

	/*
	 * Calculate sectorsize by finding least common multiple from
	 * sectorsizes of every disk and find the smallest mediasize.
	 */
	mediasize = 0;
	sectorsize = 0;
	for (i = 1; i < nargs; i++) {
		unsigned ssize;
		off_t msize;

		str = gctl_get_ascii(req, "arg%d", i);
		msize = g_get_mediasize(str);
		ssize = g_get_sectorsize(str);
		if (msize == 0 || ssize == 0) {
			gctl_error(req, "Can't get informations about %s: %s.",
			    str, strerror(errno));
			return;
		}
		msize -= ssize;
		if (mediasize == 0 || (mediasize > 0 && msize < mediasize))
			mediasize = msize;
		if (sectorsize == 0)
			sectorsize = ssize;
		else
			sectorsize = g_lcm(sectorsize, ssize);
	}
	md.md_mediasize = mediasize;
	md.md_sectorsize = sectorsize;
	md.md_mediasize -= (md.md_mediasize % md.md_sectorsize);

	/*
	 * Clear last sector first, to spoil all components if device exists.
	 */
	for (i = 1; i < nargs; i++) {
		str = gctl_get_ascii(req, "arg%d", i);
		error = g_metadata_clear(str, NULL);
		if (error != 0) {
			gctl_error(req, "Can't store metadata on %s: %s.", str,
			    strerror(error));
			return;
		}
	}

	/*
	 * Ok, store metadata (use disk number as priority).
	 */
	for (i = 1; i < nargs; i++) {
		str = gctl_get_ascii(req, "arg%d", i);
		md.md_did = arc4random();
		md.md_priority = i - 1;
		md.md_provsize = g_get_mediasize(str);
		assert(md.md_provsize != 0);
		if (!hardcode)
			bzero(md.md_provider, sizeof(md.md_provider));
		else {
			if (strncmp(str, _PATH_DEV, sizeof(_PATH_DEV) - 1) == 0)
				str += sizeof(_PATH_DEV) - 1;
			strlcpy(md.md_provider, str, sizeof(md.md_provider));
		}
		raid_metadata_encode(&md, sector);
		error = g_metadata_store(str, sector, sizeof(sector));
		if (error != 0) {
			fprintf(stderr, "Can't store metadata on %s: %s.\n",
			    str, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (verbose)
			printf("Metadata value stored on %s.\n", str);
	}
}

static void
raid_clear(struct gctl_req *req)
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
		error = g_metadata_clear(name, G_RAID_MAGIC);
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

static void
raid_dump(struct gctl_req *req)
{
	struct g_raid_metadata md, tmpmd;
	const char *name;
	int error, i, nargs;

	nargs = gctl_get_int(req, "nargs");
	if (nargs < 1) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	for (i = 0; i < nargs; i++) {
		name = gctl_get_ascii(req, "arg%d", i);
		error = g_metadata_read(name, (u_char *)&tmpmd, sizeof(tmpmd),
		    G_RAID_MAGIC);
		if (error != 0) {
			fprintf(stderr, "Can't read metadata from %s: %s.\n",
			    name, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (raid_metadata_decode((u_char *)&tmpmd, &md) != 0) {
			fprintf(stderr, "MD5 hash mismatch for %s, skipping.\n",
			    name);
			gctl_error(req, "Not fully done.");
			continue;
		}
		printf("Metadata on %s:\n", name);
		raid_metadata_dump(&md);
		printf("\n");
	}
}

static void
raid_activate(struct gctl_req *req)
{
	struct g_raid_metadata md, tmpmd;
	const char *name, *path;
	int error, i, nargs;

	nargs = gctl_get_int(req, "nargs");
	if (nargs < 2) {
		gctl_error(req, "Too few arguments.");
		return;
	}
	name = gctl_get_ascii(req, "arg0");

	for (i = 1; i < nargs; i++) {
		path = gctl_get_ascii(req, "arg%d", i);
		error = g_metadata_read(path, (u_char *)&tmpmd, sizeof(tmpmd),
		    G_RAID_MAGIC);
		if (error != 0) {
			fprintf(stderr, "Cannot read metadata from %s: %s.\n",
			    path, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (raid_metadata_decode((u_char *)&tmpmd, &md) != 0) {
			fprintf(stderr,
			    "MD5 hash mismatch for provider %s, skipping.\n",
			    path);
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (strcmp(md.md_name, name) != 0) {
			fprintf(stderr,
			    "Provider %s is not the raid %s component.\n",
			    path, name);
			gctl_error(req, "Not fully done.");
			continue;
		}
		md.md_dflags &= ~G_RAID_DISK_FLAG_INACTIVE;
		raid_metadata_encode(&md, (u_char *)&tmpmd);
		error = g_metadata_store(path, (u_char *)&tmpmd, sizeof(tmpmd));
		if (error != 0) {
			fprintf(stderr, "Cannot write metadata from %s: %s.\n",
			    path, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (verbose)
			printf("Provider %s activated.\n", path);
	}
}
#endif
