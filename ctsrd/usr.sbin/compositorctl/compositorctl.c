/*-
 * Copyright (c) 2013 Philip Withnall
 * All rights reserved.
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

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/endian.h>
#include <sys/ioccom.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>

#include <machine/cheri.h>

#include <compositor.h>

/* Source images. */
/* From
 * http://en.wikipedia.org/wiki/File:University_of_Cambridge_coat_of_arms.svg,
 * CC-BY-SA 3.0 unported. */
#include "crest.png.c"

/* Device node to use for the compositor. */
#define COMPOSITOR_DEV "/dev/compositor_cfb0"

/* Labels for the pipeline stages. */
static const char *pipeline_stage_names[] = {
	"TCEQ",
	"TCER",
	"MEMQ",
	"MEMR",
	"CU",
	"OUT",
};

struct compositorctl_command {
	const char *name;
	int (*parse_command_line) (const char **argv, int argc);
	const char *help_parameters;
	const char *help_description;
};

static int usage(void);

static int parse_setres(const char **argc, int argv);
static int parse_print_statistics(const char **argv __unused, int argc);
static int parse_reset_statistics(const char **argv __unused, int argc);
static int parse_fbfill(const char **argc, int argv);
static int parse_cfb(const char **argc, int argv);
static int parse_animate(const char **argv, int argc);
static int parse_allocate_cfb(const char **argv, int argc);
static int parse_write_cfb_data(const char **argv, int argc);
static int parse_update_cfb(const char **argv, int argc);
static int parse_free_cfb(const char **argv, int argc);
static int parse_help(const char **argv, int argc);

/* Commands available to execute. Note that the parse_*() functions take argv
 * and argc with the first two elements ("compositorctl" and the command name)
 * removed. */
static const struct compositorctl_command commands[] = {
	/* High-level commands. */
	{ "help", parse_help, "", "Display this help." },
	{ "setres", parse_setres, "<width> <height>",
	  "Set the resolution in pixels." },
	{ "print_statistics", parse_print_statistics, "",
	  "Print memory usage statistics for the compositor." },
	{ "reset_statistics", parse_reset_statistics, "",
	  "Reset memory usage statistics for the compositor." },
	{ "fbfill", parse_fbfill, "<RRGGBB>",
	  "Fill the screen with uniform colour." },
	{ "cfb", parse_cfb, "<width> <height> <xpos> <ypos> <zpos> <RRGGBBAA>",
	  "Allocate and render a CFB of uniform colour." },
	{ "animate", parse_animate, "",
	  "Run an animation on the screen until the program is killed." },
	/* Low-level commands. */
	{ "allocate_cfb", parse_allocate_cfb, "<width> <height>",
	  "Allocate a CFB (in pixels) and return the CFB ID." },
	{ "write_cfb_data", parse_write_cfb_data,
	  "<CFB base> <CFB length> <pixel data>",
	  "Write replicated pixel tile data to the CFB." },
	{ "update_cfb", parse_update_cfb,
	  "<CFB base> <CFB length> <xpos> <ypos> <zpos> <opaque X> <opaque Y> "
	  "<opaque width> <opaque height> <update>",
	  "Update CFB metadata. Positions are in pixels. "
	  "This will not work by default due to not being able to access the "
	  "CFB." },
	{ "free_cfb", parse_free_cfb, "<CFB base> <CFB length>",
	  "Free CFB metadata." },
};

static struct compositor_mapping default_compositor = { 0, NULL, 0 };

/* Note that state can’t be preserved between invocations of compositorctl, so
 * setres() is pretty useless. */
static unsigned int screen_width = DEFAULT_X_RES; /* in pixels */
static unsigned int screen_height = DEFAULT_Y_RES; /* in pixels */

#define DIVIDE_ROUND(N, D) (((N) + (D) - 1) / (D))
#define ROUND_UP(N, D) (DIVIDE_ROUND(N, D) * (D))

/* Allocate a CFB ID and token on the heap. This shouldn't be necessary once the
 * stack mis-alignment bug is fixed.
 *
 * FIXME: struct chericap is somehow still not aligned properly. */
static int
allocate_cfb_id_and_token(struct chericap **cfb_id_out,
                          struct chericap **cfb_token_out)
{
	if (posix_memalign((void **) cfb_id_out, CHERICAP_SIZE,
	                   sizeof(**cfb_id_out)) != 0 ||
	    (cfb_token_out != NULL &&
	     posix_memalign((void **) cfb_token_out, CHERICAP_SIZE,
	                    sizeof(**cfb_token_out)) != 0)) {
		fprintf(stderr, "Error allocating CFB ID or token: %s\n",
		        strerror(errno));

		*cfb_id_out = NULL;
		if (cfb_token_out != NULL) {
			*cfb_token_out = NULL;
		}

		return 1;
	}

	return 0;
}

/* Parse an unsigned integer from a string passed in on the command line. It
 * will be validated against minimum and maximum, and the whole input string
 * must be consumed. */
static int
parse_unsigned_int(unsigned int *out, const char *in, const char *label,
                   unsigned int minimum, unsigned int maximum)
{
	char *endptr;

	*out = strtoul(in, &endptr, 0);
	if (*endptr != '\0' || *out < minimum || *out > maximum) {
		fprintf(stderr, "Invalid %s ‘%s’.\n", label, in);
		return 1;
	}

	return 0;
}

static int
parse_allocate_cfb(const char **argv, int argc)
{
	struct chericap *cfb_id = NULL;
	unsigned int width, height; /* in pixels */
	int retval = 0;

	if (argc != 2) {
		return usage();
	}

	if (parse_unsigned_int(&width, argv[0], "width", 0, MAX_X_RES) != 0 ||
	    parse_unsigned_int(&height, argv[1], "height", 0, MAX_Y_RES) != 0) {
		return usage();
	}

	if (allocate_cfb_id_and_token(&cfb_id, NULL) != 0) {
		return 1;
	}

	/* Allocate the CFB. */
	printf("Allocating CFB of size %u×%u pixels…\n", width, height);
	fflush(stdout);

	if (compositor_allocate_cfb(&default_compositor, width,
	                            height, cfb_id) != 0) {
		fprintf(stderr, "Error allocating CFB: %s\n", strerror(errno));
		retval = 1;
		goto done;
	}

	printf("Allocated as %p [%p, %p).\n", cfb_id,
	       (void *) cfb_id->c_base,
	       (void *) (cfb_id->c_base + cfb_id->c_length));

done:
	free(cfb_id);

	return retval;
}

static int
parse_write_cfb_data(const char **argv, int argc)
{
	struct chericap *cfb_id = NULL;
	uint64_t cfb_base, cfb_length;
	unsigned int cfb_width, cfb_height;
	uint32_t pixel;
	char *endptr;

	if (argc != 3) {
		return usage();
	}

	cfb_base = strtoull(argv[0], &endptr, 16);
	if (*endptr != '\0') {
		fprintf(stderr, "Invalid CFB base ‘%s’.\n", argv[0]);
		return usage();
	}

	cfb_length = strtoull(argv[1], &endptr, 16);
	if (*endptr != '\0') {
		fprintf(stderr, "Invalid CFB length ‘%s’.\n", argv[1]);
		return usage();
	}

	pixel = strtoul(argv[3], &endptr, 16);
	if (*endptr != '\0') {
		fprintf(stderr, "Invalid pixel ‘%s’.\n", argv[3]);
		return usage();
	}

	/* Set up the CFB. Hacky, but the only real way to expose this low-level
	 * functionality in a stateless way. */
	CHERI_CINCBASE(1, 0, cfb_base);
	CHERI_CSETLEN(1, 1, cfb_length);
	CHERI_CANDPERM(1, 1, CHERI_PERM_STORE | CHERI_PERM_LOAD);

	if (allocate_cfb_id_and_token(&cfb_id, NULL) != 0) {
		return 1;
	}

	CHERI_CSC(1, 0, cfb_id, 0);

	/* Write out the tiles. Make up the CFB’s width and height (which
	 * shouldn’t matter as long as the row stride is OK). */
	printf("Writing CFB data…\n");

	cfb_width =
		cfb_length / sizeof(struct compositor_rgba_pixel) / TILE_SIZE;
	cfb_height = TILE_SIZE;

	compositor_write_cfb_data_solid(cfb_id, cfb_width, cfb_height,
	                                (struct compositor_rgba_pixel *) &pixel);

	/* Tidy up. */
	free(cfb_id);

	return 0;
}

/* Note: This requires hacking around to work. Any CFB allocated by a different
 * compositorctl instance will either:
 *  • be destroyed when that compositorctl instance closes; or
 *  • not be accessible to another compositorctl instance due to using a
 *    different CFB pool.
 * So this function is only useful if compositorctl is hacked to combine it with
 * allocate_cfb in some way. */
static int
parse_update_cfb(const char **argv, int argc)
{
	struct chericap *cfb_id = NULL, *cfb_token = NULL;
	uint64_t cfb_base, cfb_length;
	unsigned int xpos, ypos, zpos, update_in_progress;
	unsigned int opaque_x, opaque_y, opaque_width, opaque_height;
	char *endptr;
	int retval = 0;

	if (argc != 10) {
		return usage();
	}

	cfb_base = strtol(argv[0], &endptr, 16);
	if (*endptr != '\0') {
		fprintf(stderr, "Invalid CFB base ‘%s’.\n", argv[0]);
		return usage();
	}

	cfb_length = strtol(argv[1], &endptr, 16);
	if (*endptr != '\0') {
		fprintf(stderr, "Invalid CFB length ‘%s’.\n", argv[1]);
		return usage();
	}

	if (parse_unsigned_int(&xpos, argv[2], "X coordinate", 0,
	                       MAX_X_RES - 1) != 0 ||
	    parse_unsigned_int(&ypos, argv[3], "Y coordinate", 0,
	                       MAX_Y_RES - 1) != 0 ||
	    parse_unsigned_int(&zpos, argv[4], "Z coordinate", 0, MAX_Z) != 0 ||
	    parse_unsigned_int(&opaque_x, argv[5], "opaque X coordinate", 0,
	                       MAX_X_RES - 1) != 0 ||
	    parse_unsigned_int(&opaque_y, argv[6], "opaque Y coordinate", 0,
	                       MAX_Y_RES - 1) != 0 ||
	    parse_unsigned_int(&opaque_width, argv[7], "opaque width", 0,
	                       MAX_X_RES) != 0 ||
	    parse_unsigned_int(&opaque_height, argv[8], "opaque height", 0,
	                       MAX_Y_RES) != 0 ||
	    parse_unsigned_int(&update_in_progress, argv[9],
	                       "update in progress boolean", 0, 1) != 0) {
		return usage();
	}

	/* Set up the CFB. Hacky, but the only real way to expose this low-level
	 * functionality in a stateless way. */
	CHERI_CINCBASE(1, 0, cfb_base);
	CHERI_CSETLEN(1, 1, cfb_length);
	CHERI_CANDPERM(1, 1, CHERI_PERM_STORE | CHERI_PERM_LOAD);

	if (allocate_cfb_id_and_token(&cfb_id, &cfb_token) != 0) {
		return 1;
	}

	CHERI_CSC(1, 0, cfb_id, 0);

	/* Convert the CFB ID to a token. */
	if (compositor_cfb_id_to_token(&default_compositor, cfb_id,
	                               cfb_token) != 0) {
		fprintf(stderr, "Error converting CFB ID to token: %s\n",
		        strerror(errno));
		retval = 1;
		goto done;
	}

	/* Mark the CFB as finished updating. */
	printf("Updating CFB %p [%p, %p) to position (%u, %u, %u)…\n", cfb_id,
	       (void *) cfb_id->c_base,
	       (void *) (cfb_id->c_base + cfb_id->c_length),
	       xpos, ypos, zpos);
	fflush(stdout);

	if (compositor_update_cfb(&default_compositor, cfb_token, xpos,
	                          ypos, zpos, opaque_x, opaque_y, opaque_width,
	                          opaque_height, update_in_progress) != 0) {
		fprintf(stderr, "Error updating CFB: %s\n", strerror(errno));
		retval = 1;
		goto done;
	}

done:
	free(cfb_token);
	free(cfb_id);

	return retval;
}

static int
parse_free_cfb(const char **argv, int argc)
{
	struct chericap *cfb_id = NULL;
	uint64_t cfb_base, cfb_length;
	char *endptr;
	int retval = 0;

	if (argc != 2) {
		return usage();
	}

	cfb_base = strtol(argv[0], &endptr, 16);
	if (*endptr != '\0') {
		fprintf(stderr, "Invalid CFB base ‘%s’.\n", argv[0]);
		return usage();
	}

	cfb_length = strtol(argv[1], &endptr, 16);
	if (*endptr != '\0') {
		fprintf(stderr, "Invalid CFB length ‘%s’.\n", argv[1]);
		return usage();
	}

	/* Set up the CFB. Hacky, but the only real way to expose this low-level
	 * functionality in a stateless way. */
	CHERI_CINCBASE(1, 0, cfb_base);
	CHERI_CSETLEN(1, 1, cfb_length);
	CHERI_CANDPERM(1, 1, CHERI_PERM_STORE | CHERI_PERM_LOAD);

	if (allocate_cfb_id_and_token(&cfb_id, NULL) != 0) {
		return 1;
	}

	CHERI_CSC(1, 0, cfb_id, 0);

	/* Free the CFB. */
	printf("Freeing CFB %p [%p, %p)…\n", cfb_id,
	       (void *) cfb_id->c_base,
	       (void *) (cfb_id->c_base + cfb_id->c_length));
	fflush(stdout);

	if (compositor_free_cfb (&default_compositor, cfb_id) != 0) {
		fprintf(stderr, "Error freeing CFB: %s\n", strerror(errno));
		retval = 1;
		goto done;
	}

done:
	free(cfb_id);

	return retval;
}

/* Set the compositor's output resolution, in pixels.
 * Returns a main()-style error number. */
static int
setres(unsigned int width, unsigned int height)
{
	if (compositor_set_configuration(&default_compositor, width,
	                                 height) != 0) {
		fprintf(stderr, "Error setting resolution: %s\n",
		        strerror(errno));
		return 1;
	}

	/* Update the stored resolution. */
	screen_width = width;
	screen_height = height;

	return 0;
}

static int
parse_setres(const char **argv, int argc)
{
	unsigned int width, height;

	if (argc == 0) {
		/* Use default resolution. */
		width = DEFAULT_X_RES;
		height = DEFAULT_Y_RES;
	} else if (argc == 2) {
		/* Parse resolution from command line. */
		if (parse_unsigned_int(&width, argv[0], "width", 0,
		                       MAX_X_RES) != 0 ||
		    parse_unsigned_int(&height, argv[1], "height", 0,
		                       MAX_Y_RES) != 0) {
			return usage();
		}
	} else {
		/* Error. */
		return usage();
	}

	return setres(width, height);
}

/* Print statistics about memory usage by the compositor so far.
 * Returns a main()-style error number. */
static int
print_statistics(void)
{
	struct compositor_statistics stats;
	unsigned int i, bin_width;

	/* Request the statistics. */
	if (compositor_get_statistics(&default_compositor, &stats) != 0) {
		fprintf(stderr, "Error getting statistics: %s\n",
		        strerror(errno));
		return 1;
	}

	/* Print the memory bus statistics. */
	printf("Compositor has made %u memory requests (%u reads, %u writes, "
	       "%u burst read packets, %u burst write packets).\n"
	       "Latency distribution:\n",
	       stats.num_read_requests + stats.num_write_requests,
	       stats.num_read_requests, stats.num_write_requests,
	       stats.num_read_bursts, stats.num_write_bursts);

	assert(stats.num_latency_bins > 1 &&
	       stats.num_latency_bins <= sizeof(stats.latency_bins) /
	                                 sizeof(*stats.latency_bins));
	bin_width = (stats.latency_bin_upper_bound -
	             stats.latency_bin_lower_bound) /
	            (stats.num_latency_bins - 1);

	for (i = 0; i < stats.num_latency_bins; i++) {
		printf("  [%2u, %2u) cycles: %u requests\n",
		       stats.latency_bin_lower_bound + bin_width * i,
		       stats.latency_bin_lower_bound + bin_width * (i + 1),
		       stats.latency_bins[i]);
	}

	/* Print the compositor statistics. */
	printf("Compositor has seen %u cycles and outputted %u frames.\n"
	       "%u TCE requests and %u memory requests have been emitted.\n"
	       "Pipeline packet transmissions:\n",
	       stats.num_compositor_cycles, stats.num_frames,
	       stats.num_tce_requests, stats.num_memory_requests);

	for (i = 0; i < COMPOSITOR_STATISTICS_NUM_PIPELINE_STAGES + 1; i++) {
		printf("  %s → %s: %u packets\n",
		    (i == 0) ? "head" : pipeline_stage_names[i - 1],
		    pipeline_stage_names[i], stats.pipeline_stage_cycles[i]);
	}

	return 0;
}

static int
parse_print_statistics(const char **argv __unused, int argc)
{
	if (argc != 0) {
		return usage();
	}

	return print_statistics();
}

/* Reset memory usage statistics for the compositor.
 * Returns a main()-style error number. */
static int
reset_statistics(void)
{
	/* Reset the statistics. */
	if (compositor_reset_statistics(&default_compositor) != 0) {
		fprintf(stderr, "Error resetting statistics: %s\n",
		        strerror(errno));
		return 1;
	}

	return 0;
}

static int
parse_reset_statistics(const char **argv __unused, int argc)
{
	if (argc != 0) {
		return usage();
	}

	return reset_statistics();
}

/* Allocate and render a CFB of a single (RGBA) colour. Returns a main()-style
 * error number. */
static int
cfb(unsigned int width, unsigned int height, unsigned int xpos,
    unsigned int ypos, unsigned int zpos, long colour)
{
	struct chericap *cfb_id = NULL, *cfb_token = NULL;
	struct compositor_rgba_pixel pixel;
	int retval = 0;

	if (allocate_cfb_id_and_token(&cfb_id, &cfb_token) != 0) {
		return 1;
	}

	/* Allocate the CFB. */
	printf("Allocating CFB of size %u×%u pixels…\n", width, height);
	fflush(stdout);

	if (compositor_allocate_cfb_with_token(&default_compositor, width,
	                                       height, cfb_id,
	                                       cfb_token) != 0) {
		fprintf(stderr, "Error allocating CFB: %s\n", strerror(errno));
		retval = 1;
		goto done;
	}

	printf("Allocated as %p [%p, %p) with token %p.\n", cfb_id,
	       (void *) cfb_id->c_base,
	       (void *) (cfb_id->c_base + cfb_id->c_length), cfb_token);

	/* Prepare the colour. */
	pixel.red = (colour >> 24) & 0xff;
	pixel.green = (colour >> 16) & 0xff;
	pixel.blue = (colour >> 8) & 0xff;
	pixel.alpha = (colour >> 0) & 0xff;

	/* Write out the tiles. */
	printf("Writing tile data…\n");
	fflush(stdout);

	compositor_write_cfb_data_solid(cfb_id, width, height, &pixel);

	/* Mark the CFB as finished updating. */
	printf("Updating CFB %p [%p, %p) to position (%u, %u, %u)…\n", cfb_id,
	       (void *) cfb_id->c_base,
	       (void *) (cfb_id->c_base + cfb_id->c_length),
	       xpos, ypos, zpos);
	fflush(stdout);

	if (compositor_update_cfb(&default_compositor, cfb_token, xpos,
	                          ypos, zpos, 0, 0, 0, 0, 0) != 0) {
		fprintf(stderr, "Error updating CFB: %s\n", strerror(errno));
		retval = 1;
		goto done;
	}

	/* Sleep briefly to make it more obvious. */
	printf("Sleeping for 5 seconds…\n");
	sleep(5);

done:
	free(cfb_token);
	free(cfb_id);

	return retval;
}

static int
parse_cfb(const char **argv, int argc)
{
	char *endptr;
	long colour;
	unsigned int width, height, xpos, ypos, zpos;

	if (argc != 6) {
		return usage();
	}

	/* We expect args: width, height, xpos, ypos, zpos, colour */
	if (parse_unsigned_int(&width, argv[0], "width", 0, MAX_X_RES) != 0 ||
	    parse_unsigned_int(&height, argv[1], "height", 0, MAX_Y_RES) != 0 ||
	    parse_unsigned_int(&xpos, argv[2], "X coordinate", 0,
	                       MAX_X_RES - 1) != 0 ||
	    parse_unsigned_int(&ypos, argv[3], "Y coordinate", 0,
	                       MAX_Y_RES - 1) != 0 ||
	    parse_unsigned_int(&zpos, argv[4], "Z coordinate", 0, MAX_Z) != 0) {
		return usage();
	}

	colour = strtol(argv[5], &endptr, 16);
	if (*endptr != '\0' || colour < 0 || colour > 0xFFFFFFFF) {
		fprintf(stderr, "Invalid colour ‘%s’.\n", argv[5]);
		return usage();
	}

	return cfb(width, height, xpos, ypos, zpos, colour);
}

static void
write_cfb_data_with_border(struct chericap *cfb_id, unsigned int cfb_width,
                           unsigned int cfb_height,
                           const struct compositor_rgba_pixel *bg_colour,
                           const struct compositor_rgba_pixel *border_colour)
{
	unsigned int x, y; /* pixels */
	unsigned int cfb_row_stride, cfb_column_stride; /* pixels */
	uint64_t bg_pixels, border_pixels;

	/* Sort out the pixel data to write. */
	bg_pixels = (uint64_t) (((uint32_t) bg_colour->alpha << 24) |
	                        ((uint32_t) bg_colour->blue << 16) |
	                        ((uint32_t) bg_colour->green << 8) |
	                        ((uint32_t) bg_colour->red << 0)) << 32 |
	            (uint64_t) (((uint32_t) bg_colour->alpha << 24) |
	                        ((uint32_t) bg_colour->blue << 16) |
	                        ((uint32_t) bg_colour->green << 8) |
	                        ((uint32_t) bg_colour->red << 0));
	border_pixels = (uint64_t) (((uint32_t) border_colour->alpha << 24) |
	                            ((uint32_t) border_colour->blue << 16) |
	                            ((uint32_t) border_colour->green << 8) |
	                            ((uint32_t) border_colour->red << 0)) << 32 |
	                (uint64_t) (((uint32_t) border_colour->alpha << 24) |
	                            ((uint32_t) border_colour->blue << 16) |
	                            ((uint32_t) border_colour->green << 8) |
	                            ((uint32_t) border_colour->red << 0));

	/* Load up the capability. */
	CHERI_CLC(1, 0, cfb_id, 0);

	cfb_row_stride = ROUND_UP(cfb_width, TILE_SIZE);
	cfb_column_stride = ROUND_UP(cfb_height, TILE_SIZE);

	/* Write out the pixels through the capability. */
	for (y = 0; y < cfb_column_stride; y++) {
		for (x = 0; x < cfb_row_stride; x += 2) {
			uint64_t pixel_pair;

			/* If we've run off the end of the CFB, use alpha. */
			if (x >= cfb_width || y >= cfb_height) {
				pixel_pair = 0x0000000000000000;
			} else if (x == 0 || x == cfb_width - 2 ||
			           y == 0 || y == 1 ||
			           y == cfb_height - 2 || y == cfb_height - 1) {
				pixel_pair = border_pixels;
			} else {
				pixel_pair = bg_pixels;
			}

			/* FIXME: For the moment, access each pair of pixels
			 * individually. In the future it would be nice to use
			 * an optimised memcpy() instead.
			 *
			 * Note that currently this code will misbehave if
			 * cfb_width is not a multiple of 2. */
			CHERI_CSD(pixel_pair,
			          (y * cfb_row_stride + x) * PIXEL_SIZE,
			          0, 1);
		}
	}

	/* Don't leak the capability. */
	CHERI_CCLEARTAG(1);
}

/* Properties of the overlays displayed by animate(), such as their positions
 * and sizes. */
const struct {
	unsigned int x; /* pixels */
	unsigned int y; /* pixels */
	unsigned int z;
	unsigned int width; /* pixels */
	unsigned int height; /* pixels */
} animation_overlays[] = {
	{ 32, 32, 2, 416, 128 },
	{ 64, 64, 8, 128, 416 },
	{ 512, 96, 6, 128, 128 },
	{ 320, 256, 4, 256, 64 },
};

/* Animate a university crest bouncing around the screen, one pixel per loop
 * iteration, animating as fast as possible to see if any tearing or visual
 * artifacts are visible. This uses two CFBs: one static one for the background,
 * and one for the crest, whose position is updated each iteration. Note that
 * the pixel data for both CFBs remains constant. */
static int
animate(void)
{
	struct chericap *background_cfb_id = NULL;
	struct chericap *background_cfb_token = NULL;
	struct chericap *crest_cfb_id = NULL;
	struct chericap *crest_cfb_token = NULL;
	struct chericap *overlay_cfb_ids = NULL;
	struct chericap *overlay_cfb_tokens = NULL;
	int crest_x, crest_y, crest_dx, crest_dy;
	struct compositor_mapping secure_compositor;
	int retval = 0;
	struct compositor_rgba_pixel overlay_bg_colour = {
		.red = 0, .green = 0xa0, .blue = 0, .alpha = 0xa0
	};
	struct compositor_rgba_pixel overlay_border_colour = {
		.red = 0, .green = 0xff, .blue = 0xff, .alpha = 0xff
	};
	unsigned int i;


	/* Initialise a second mapping of the compositor. */
	if (compositor_map(COMPOSITOR_DEV, &secure_compositor) != 0) {
		fprintf(stderr,
		        "Error initialising the secure compositor: %s\n",
		        strerror(errno));
		retval = 1;
		goto done;
	}


	/* Allocate a background. */
	if (allocate_cfb_id_and_token(&background_cfb_id,
	                              &background_cfb_token) != 0) {
		retval = 1;
		goto done;
	}

	/* Allocate the background CFB. */
	printf("Allocating background CFB of size %u×%u pixels…\n",
	       screen_width, screen_height);

	if (compositor_allocate_cfb_with_token(&default_compositor,
	                                       screen_width, screen_height,
	                                       background_cfb_id,
	                                       background_cfb_token) != 0) {
		fprintf(stderr, "Error allocating background CFB: %s\n",
		        strerror(errno));
		retval = 1;
		goto done;
	}

	printf("Allocated as %p [%p, %p) with token %p.\n", background_cfb_id,
	       (void *) background_cfb_id->c_base,
	       (void *) (background_cfb_id->c_base + background_cfb_id->c_length),
	       background_cfb_token);

	/* Write out the tiles. */
	printf("Writing background tile data…\n");
	fflush(stdout);

	compositor_write_cfb_data_chequered(background_cfb_id, screen_width,
	                                    screen_height);

	printf("Finished writing background data\n");
	fflush(stdout);

	/* Mark the CFB as finished updating. */
	if (compositor_update_cfb(&default_compositor,
	                          background_cfb_token, 0, 0, 0, 0, 0,
	                          screen_width, screen_height, 0) != 0) {
		fprintf(stderr, "Error updating background CFB: %s\n",
		        strerror(errno));
		retval = 1;
		goto done;
	}


	/* Allocate a crest. */
	if (allocate_cfb_id_and_token(&crest_cfb_id, &crest_cfb_token) != 0) {
		retval = 1;
		goto done;
	}

	/* Allocate the CFB. */
	printf("Allocating crest CFB…\n");

	if (compositor_allocate_cfb_with_token(&default_compositor,
	                                       ROUND_UP(crest_image.width, TILE_SIZE),
	                                       ROUND_UP(crest_image.height, TILE_SIZE),
	                                       crest_cfb_id,
	                                       crest_cfb_token) != 0) {
		fprintf(stderr, "Error allocating crest CFB: %s\n",
		        strerror(errno));
		retval = 1;
		goto done;
	}

	printf("Allocated as %p [%p, %p) with token %p.\n", crest_cfb_id,
	       (void *) crest_cfb_id->c_base,
	       (void *) (crest_cfb_id->c_base + crest_cfb_id->c_length),
	       crest_cfb_token);

	/* Write out the tiles. */
	printf("Writing crest tile data…\n");

	compositor_write_cfb_data_image(crest_cfb_id,
	                                ROUND_UP(crest_image.width, TILE_SIZE),
	                                ROUND_UP(crest_image.height, TILE_SIZE),
	                                crest_image.width, crest_image.height,
	                                (const uint32_t *) ((const void *) crest_image.pixel_data));


	/* Allocate some overlays. */
	if (posix_memalign((void **) &overlay_cfb_ids, CHERICAP_SIZE,
	                   sizeof(*overlay_cfb_ids)) != 0 ||
	    posix_memalign((void **) &overlay_cfb_tokens, CHERICAP_SIZE,
	                   sizeof(*overlay_cfb_tokens)) != 0) {
		fprintf(stderr, "Error allocating CFB ID or token: %s\n",
		        strerror(errno));
		retval = 1;
		goto done;
	}

	for (i = 0;
	     i < sizeof(animation_overlays) / sizeof(*animation_overlays);
	     i++) {
		unsigned int overlay_width = animation_overlays[i].width;
		unsigned int overlay_height = animation_overlays[i].height;

		/* Allocate the overlay CFB. */
		printf("Allocating overlay CFB of size %u×%u pixels…\n",
		       overlay_width, overlay_height);

		if (compositor_allocate_cfb_with_token(&secure_compositor,
		                                       overlay_width,
		                                       overlay_height,
		                                       &overlay_cfb_ids[i],
		                                       &overlay_cfb_tokens[i]) != 0) {
			fprintf(stderr, "Error allocating overlay CFB: %s\n",
			        strerror(errno));
			retval = 1;
			goto done;
		}

		printf("Allocated as %p [%p, %p) with token %p.\n", &overlay_cfb_ids[i],
		       (void *) overlay_cfb_ids[i].c_base,
		       (void *) (overlay_cfb_ids[i].c_base + overlay_cfb_ids[i].c_length),
		       &overlay_cfb_tokens[i]);

		/* Write out the tiles. */
		printf("Writing overlay tile data to %u…\n",
		       (overlay_width * overlay_height) / (TILE_SIZE * TILE_SIZE));
		fflush(stdout);

		write_cfb_data_with_border(&overlay_cfb_ids[i], overlay_width,
		                           overlay_height, &overlay_bg_colour,
		                           &overlay_border_colour);

		printf("Finished writing overlay data\n");
		fflush(stdout);

		/* Mark the CFB as finished updating. */
		if (compositor_update_cfb(&secure_compositor,
		                          &overlay_cfb_tokens[i],
		                          animation_overlays[i].x,
		                          animation_overlays[i].y,
		                          animation_overlays[i].z,
		                          0, 0, 0, 0, 0) != 0) {
			fprintf(stderr, "Error updating overlay CFB: %s\n",
			        strerror(errno));
			retval = 1;
			goto done;
		}

		/* Change the colours for the next one. */
		overlay_bg_colour.alpha -= 0x20;
		overlay_bg_colour.red =
			MIN(overlay_bg_colour.alpha,
			    overlay_bg_colour.red + 0x30);
		overlay_bg_colour.green =
			MIN(overlay_bg_colour.alpha,
			    overlay_bg_colour.green - 0x30);
		overlay_border_colour.red =
			MIN(overlay_border_colour.alpha,
			    overlay_border_colour.red + 0x30);
		overlay_border_colour.green =
			MIN(overlay_border_colour.alpha,
			    overlay_border_colour.green - 0x30);
	}


	/* Animate forever. */
	printf("Starting animation…\n");

	crest_x = 0; /* start in the top-left corner */
	crest_y = 0;
	crest_dx = 1; /* start off by moving south-east */
	crest_dy = 1;

	while (1) {
		if (compositor_update_cfb(&default_compositor,
		                          crest_cfb_token, crest_x,
		                          crest_y, 5, 0, 0, 0, 0, 0) != 0) {
			fprintf(stderr, "Error updating crest CFB: %s\n",
			        strerror(errno));
			retval = 1;
			goto done;
		}

		crest_x += crest_dx;
		crest_y += crest_dy;

		if (crest_y + (int) crest_image.height >= (int) screen_height ||
		    crest_y <= 0) {
			/* Bounce off the top or bottom edge of the screen. */
			crest_dy = -crest_dy;
		}
		if (crest_x + (int) crest_image.width >= (int) screen_width ||
		    crest_x <= 0) {
			/* Bounce off the left or right edge of the screen. */
			crest_dx = -crest_dx;
		}
	}

done:
	compositor_unmap(&secure_compositor);

	free(overlay_cfb_tokens);
	free(overlay_cfb_ids);
	free(crest_cfb_token);
	free(crest_cfb_id);
	free(background_cfb_token);
	free(background_cfb_id);

	return retval;
}

static int
parse_animate(const char **argv __unused, int argc)
{
	if (argc != 0) {
		return usage();
	}

	return animate();
}

/* Fill the screen with a single (RGB) colour. Returns a main()-style error
 * number. */
static int
fbfill(long colour)
{
	if (cfb(screen_width, screen_height, 0, 0, 0, colour) != 0) {
		return 1;
	}

	/* Sleep for a bit to make it more obvious. */
	printf("Sleeping for 5 seconds…\n");
	sleep(5);

	return 0;
}

static int
parse_fbfill(const char **argv, int argc)
{
	char *endptr;
	long colour;

	if (argc != 1) {
		return usage();
	}

	colour = strtol(argv[0], &endptr, 16);
	if (*endptr != '\0' || colour < 0 || colour > 0xFFFFFF) {
		fprintf(stderr, "Invalid colour ‘%s’.\n", argv[0]);
		return usage();
	}

	return fbfill(colour);
}

/* Returns a main()-style error number. */
static int
usage(void)
{
	unsigned int i;

	printf("compositorctl:\n");

	/* Print available commands. */
	for (i = 0; i < sizeof (commands) / sizeof (*commands); i++) {
		const struct compositorctl_command cmd = commands[i];
		int parameters_width =
			80 /* terminal width */ - 8 /* tab */ -
			strlen("compositorctl ") - strlen(cmd.name) -
			1 /* space */ - strlen(cmd.help_parameters);

		/* Crude formatting. This won't win any awards for style or
		 * presentation. */
		printf("\tcompositorctl %s %-*s\n\t\t%s\n", cmd.name,
		       parameters_width,
		       cmd.help_parameters, cmd.help_description);
	}

	return 1;
}

static int
parse_help(const char **argv __unused, int argc __unused)
{
	/* Deliberately don't validate argc. */
	usage();
	return 0;
}

int
main(int argc, char *argv[])
{
	int retval = 0;
	unsigned int i;

	if (argc < 2) {
		retval = usage();
		goto done;
	}

	/* Initialise the compositor. */
	if (compositor_map(COMPOSITOR_DEV, &default_compositor) != 0) {
		fprintf(stderr, "Error initialising the compositor: %s\n",
		        strerror(errno));
		retval = 1;
		goto done;
	}

	printf("Opened ‘%s’ as FD %i.\n", COMPOSITOR_DEV,
	       default_compositor.control_fd);
	printf("Mapped ‘%s’ as [%p, %p).\n", COMPOSITOR_DEV,
	       default_compositor.pixels,
	       (void *) ((uintptr_t) default_compositor.pixels +
	                 CHERI_COMPOSITOR_MEM_POOL_LENGTH));

	/* Parse command input. */
	for (i = 0; i < sizeof(commands) / sizeof(*commands); i++) {
		const struct compositorctl_command cmd = commands[i];

		if (strcmp(argv[1], cmd.name) == 0) {
			retval =
				cmd.parse_command_line((const char **) argv + 2,
				                       argc - 2);
			goto done;
		}
	}
	
	/* Failed to match a command? */
	retval = usage();

done:
	compositor_unmap(&default_compositor);

	return retval;
}
