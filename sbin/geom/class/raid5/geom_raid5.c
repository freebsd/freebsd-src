/*-
 * Copyright (c) 2006 Arne Woerner <arne_woerner@yahoo.com>
 * testing + tuning-tricks: veronica@fluffles.net
 * derived from gstripe/gmirror (Pawel Jakub Dawidek <pjd@FreeBSD.org>)
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
__FBSDID("$Id: geom_raid5.c,v 1.33.1.12 2007/11/12 20:24:45 aw Exp aw $");

#include <sys/param.h>
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <libgeom.h>
#include <geom/raid5/g_raid5.h>

#include "core/geom.h"
#include "misc/subr.h"

uint32_t lib_version = G_LIB_VERSION;
uint32_t version = G_RAID5_VERSION;
static intmax_t default_stripesize = 64*1024;

static void raid5_main(struct gctl_req *req, unsigned flags);
static void raid5_clear(struct gctl_req *req);
static void raid5_dump(struct gctl_req *req);
static void raid5_label(struct gctl_req *req);

#ifndef G_TYPE_BOOL
#define G_TYPE_BOOL G_TYPE_NONE
#endif

#if __FreeBSD_version >= 700000
#define GCMD67 NULL,
#else
#define GCMD67 
#endif
struct g_command class_commands[] = {
	{ "clear", G_FLAG_VERBOSE, raid5_main, G_NULL_OPTS, GCMD67
	    "[-v] prov ..."
	},
	{ "destroy", G_FLAG_VERBOSE, NULL,
	    {
		{ 'f', "force", NULL, G_TYPE_BOOL },
		{ 'y', "noyoyo", NULL, G_TYPE_BOOL },
		G_OPT_SENTINEL
	    }, GCMD67
	    "[-fvy] name ..."
	},
	{ "remove", G_FLAG_VERBOSE, NULL, G_NULL_OPTS, GCMD67
	    "[-v] name prov"
	},
	{ "insert", G_FLAG_VERBOSE, NULL,
		{	{ 'h', "hardcode", NULL, G_TYPE_BOOL },
			G_OPT_SENTINEL}, GCMD67
	    "[-hv] name prov"
	},
	{ "configure", G_FLAG_VERBOSE, NULL,
		{	{ 'h', "hardcode", NULL, G_TYPE_BOOL },
			{ 'a', "activate", NULL, G_TYPE_BOOL },
			{ 'c', "cowop", NULL, G_TYPE_BOOL },
			{ 'n', "nohot", NULL, G_TYPE_BOOL },
			{ 'S', "safeop", NULL, G_TYPE_BOOL },
			{ 'R', "rebuild", NULL, G_TYPE_BOOL },
			G_OPT_SENTINEL}, GCMD67
	    "[-RSchnva] name"
	},
	{ "dump", 0, raid5_main, G_NULL_OPTS, GCMD67
	    "prov ..."
	},
	{ "label", G_FLAG_VERBOSE | G_FLAG_LOADKLD, raid5_main,
		{	{ 'c', "cowop", NULL, G_TYPE_BOOL },
			{ 'h', "hardcode", NULL, G_TYPE_BOOL },
			{ 'n', "nohot", NULL, G_TYPE_BOOL },
			{ 's', "stripesize", &default_stripesize, G_TYPE_NUMBER },
			{ 'S', "safeop", NULL, G_TYPE_BOOL },
			G_OPT_SENTINEL}, GCMD67
	    "[-chvn] [-s stripesize] [-S] name prov ..."
	},
	{ "stop", G_FLAG_VERBOSE, NULL,
	    {
		{ 'f', "force", NULL, G_TYPE_BOOL },
		{ 'y', "noyoyo", NULL, G_TYPE_BOOL },
		G_OPT_SENTINEL
	    }, GCMD67
	    "[-fv] name ..."
	},
	G_CMD_SENTINEL
};

static int verbose = 0;

static void
raid5_main(struct gctl_req *req, unsigned flags)
{
	const char *name;

	if ((flags & G_FLAG_VERBOSE) != 0)
		verbose = 1;

	name = gctl_get_ascii(req, "verb");
	if (name == NULL) {
		gctl_error(req, "No '%s' argument.", "verb");
		return;
	}
	if (strcmp(name, "label") == 0)
		raid5_label(req);
	else if (strcmp(name, "clear") == 0)
		raid5_clear(req);
	else if (strcmp(name, "dump") == 0)
		raid5_dump(req);
	else
		gctl_error(req, "Unknown command: %s.", name);
}

static void
raid5_label(struct gctl_req *req)
{
	struct g_raid5_metadata md;
	const char *name;
	int error, i, hardcode, nargs, safeop, nohot, cowop;
	intmax_t stripesize;

	nargs = gctl_get_int(req, "nargs");
	if (nargs < 3) {
		gctl_error(req, "Too few arguments.");
		return;
	}
	nohot = gctl_get_int(req, "nohot");
	hardcode = gctl_get_int(req, "hardcode");
	safeop = gctl_get_int(req, "safeop");
	cowop = gctl_get_int(req, "cowop");
	stripesize = gctl_get_intmax(req, "stripesize");
	if (stripesize > 256*1024) {
		gctl_error(req, "stripesize must be less than 512KB.");
		return;
	}
	if (!powerof2(stripesize)) {
		int cs;
		for (cs=4096; cs < stripesize; cs<<=1);
		gctl_error(req, "Invalid stripe size: %jd, recommended: %d.",
		           stripesize, cs);
		return;
	}


	/*
	 * Clear last sector first to spoil all components if device exists.
	 */
	for (i = 1; i < nargs; i++) {
		name = gctl_get_ascii(req, "arg%d", i);
		error = g_metadata_clear(name, NULL);
		if (error != 0) {
			gctl_error(req, "Can't store metadata on %s: %s.", name,
			    strerror(error));
			return;
		}
	}

	strlcpy(md.md_magic, G_RAID5_MAGIC, sizeof(md.md_magic));
	md.md_version = G_RAID5_VERSION;
	name = gctl_get_ascii(req, "arg0");
	strlcpy(md.md_name, name, sizeof(md.md_name));
	md.md_id = arc4random();
	md.md_all = nargs - 1;
	md.md_stripesize = stripesize;
	md.md_verified = 0;
	md.md_newest = -1;
	md.md_no_hot = nohot;
	md.md_state = nohot ? G_RAID5_STATE_CALM :
	                      (G_RAID5_STATE_HOT|G_RAID5_STATE_VERIFY);
	if (safeop)
		md.md_state |= G_RAID5_STATE_SAFEOP;
	if (cowop)
		md.md_state |= G_RAID5_STATE_COWOP;

	/*
	 * Ok, store metadata.
	 */
	int64_t min = -1;
	int64_t waste = 0;
	for (i = 1; i < nargs; i++) {
		u_char sector[512];
		int64_t pmin;

		name = gctl_get_ascii(req, "arg%d", i);
		md.md_no = i - 1;
		if (!hardcode)
			bzero(md.md_provider, sizeof(md.md_provider));
		else {
			if (strncmp(name, _PATH_DEV, strlen(_PATH_DEV)) == 0)
				name += strlen(_PATH_DEV);
			strlcpy(md.md_provider, name, sizeof(md.md_provider));
		}
		md.md_provsize = g_get_mediasize(name);
		pmin = md.md_provsize - g_get_sectorsize(name);
		waste += pmin % stripesize;
		if (min < 0)
			min = pmin;
		else if (min > pmin) {
			waste += (i-1) * (min - pmin);
			min = pmin;
		} else
			waste += pmin - min;
		if (md.md_provsize == 0) {
			fprintf(stderr, "Can't get mediasize of %s: %s.\n",
			    name, strerror(errno));
			gctl_error(req, "Not fully done.");
			continue;
		}
		raid5_metadata_encode(&md, sector);
		error = g_metadata_store(name, sector, sizeof(sector));
		if (error != 0) {
			fprintf(stderr, "Can't store metadata on %s: %s.\n",
			    name, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (verbose)
			printf("Metadata value stored on %s.\n", name);
	}
	if (waste > 0)
		printf("Wasting %jd bytes (>=%jdGB).\n", waste, waste>>(3*10));
}

static void
raid5_clear(struct gctl_req *req)
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
		error = g_metadata_clear(name, G_RAID5_MAGIC);
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
raid5_metadata_dump(const struct g_raid5_metadata *md)
{

	printf("         Magic string: %s\n", md->md_magic);
	printf("     Metadata version: %u\n", (u_int)md->md_version);
	printf("          Device name: %s\n", md->md_name);
	printf("            Device ID: %u\n", (u_int)md->md_id);
	printf("          Disk number: %u\n", (u_int)md->md_no);
	printf("Total number of disks: %u\n", (u_int)md->md_all);
	printf("        Provider Size: %jd\n", md->md_provsize);
	printf("             Verified: %jd\n", md->md_verified);
	printf("                State: %u\n", (u_int)md->md_state);
	printf("          Stripe size: %u\n", (u_int)md->md_stripesize);
	printf("               Newest: %u\n", (u_int)md->md_newest);
	printf("                NoHot: %s\n", md->md_no_hot?"Yes":"No");
	printf("   Hardcoded provider: %s\n", md->md_provider);
}

static void
raid5_dump(struct gctl_req *req)
{
	struct g_raid5_metadata md, tmpmd;
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
		    G_RAID5_MAGIC);
		if (error != 0) {
			fprintf(stderr, "Can't read metadata from %s: %s.\n",
			    name, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		raid5_metadata_decode((u_char *)&tmpmd, &md);
		printf("Metadata on %s:\n", name);
		raid5_metadata_dump(&md);
		printf("\n");
	}
}
