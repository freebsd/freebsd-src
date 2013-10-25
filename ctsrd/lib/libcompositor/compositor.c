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
#include <sys/types.h>
#include <unistd.h>

#include <machine/cheri.h>

#include "compositor.h"

/* Write pixel value P (a struct compositor_rgba_pixel) to pixel I in CFB C. I
 * is given in pixels relative to the base of C. C must be a constant integer
 * giving the number of the capability register holding the CFB capability. */
#define WRITE_PIXEL(C, I, P) \
	CHERI_CSW(P, (I) * PIXEL_SIZE, 0, C)

/**
 * Open and map a compositor device to get access to a memory pool. This will
 * open the device node whose path is given in device_path and mmap() one CFB
 * pool. The FD of the device can then be used to send commands to the
 * compositor to allocate and interact with CFBs in that pool. The FD may be
 * shared with other processes to create a shared pool.
 *
 * Once the FD is closed by all processes, the pool and all its CFBs will be
 * destroyed.
 *
 * The memory mapping of the pool is available as compositor->pixels, but
 * typically should not be used directly. Instead, write to CFBs through the
 * capability returned by compositor_allocate_cfb().
 *
 * device_path: Filename and path of the compositor device node. Typically
 *              “/dev/compositor_cfb0”.
 * compositor_out: Return address for the compositor mapping.
 * Returns: 0 on success, -1 (and errno set) on failure.
 */
int
compositor_map(const char *device_path,
               struct compositor_mapping *compositor_out)
{
	int retval = -1;
	int saved_errno = 0;

	/* Memory map the CFB region. */
	compositor_out->control_fd = open(device_path, O_RDWR);
	if (compositor_out->control_fd < 0) {
		fprintf(stderr, "Error opening compositor FD: %s\n",
		        strerror(errno));
		goto done;
	}

	compositor_out->pixels = mmap(NULL, CHERI_COMPOSITOR_MEM_POOL_LENGTH,
	                              PROT_READ | PROT_WRITE, MAP_SHARED,
	                              compositor_out->control_fd, 0);
	saved_errno = errno;

	if (compositor_out->pixels == MAP_FAILED) {
		compositor_out->pixels = NULL;
		fprintf(stderr, "Error mapping CFB memory: %s\n",
		        strerror(saved_errno));
		goto done;
	}

	/* Length in pixels. */
	compositor_out->pixels_length =
		CHERI_COMPOSITOR_MEM_POOL_LENGTH /
			sizeof(*compositor_out->pixels);

	/* Success! */
	retval = 0;

done:
	if (retval != 0) {
		compositor_unmap(compositor_out);
	}

	return retval;
}

/**
 * Unmap and close a compositor device previously mapped using compositor_map().
 * If the FD for the device has been shared with other processes, this will only
 * destroy CFBs in the compositor's pool once all other processes have closed
 * the FD.
 *
 * compositor: Compositor mapping to destroy.
 */
void
compositor_unmap(struct compositor_mapping *compositor)
{
	if (compositor->pixels != NULL) {
		munmap(compositor->pixels,
		       compositor->pixels_length * sizeof(*compositor->pixels));
		compositor->pixels = NULL;
		compositor->pixels_length = 0;
	}

	if (compositor->control_fd >= 0) {
		close(compositor->control_fd);
		compositor->control_fd = -1;
	}
}

/**
 * Allocate a CFB in the compositor. Width and height are specified in tiles.
 * A capability providing access to the CFB is written to cfb_id_out (which must
 * be 32-byte aligned) on success, and 0 is returned. On failure, -1 is
 * returned, cfb_id_out is cleared and errno is set as appropriate.
 *
 * compositor: Compositor mapping to use.
 * width: Width of the CFB, in tiles. Must be positive (notably, non-zero).
 * height: Height of the CFB, in tiles. Must be positive (notably, non-zero).
 * cfb_id_out: Return address for the CFB's capability. Must be 32-byte aligned.
 * Returns: 0 on success, -1 (and errno set) on failure.
 */
int
compositor_allocate_cfb(struct compositor_mapping *compositor, uint32_t width,
                        uint32_t height, struct chericap *cfb_id_out)
{
	struct cheri_compositor_allocate_cfb req;
	uint32_t tagged;

	req.width = width;
	req.height = height;
	req.cfb_id_out = (caddr_t) cfb_id_out;

	if (ioctl(compositor->control_fd, CHERI_COMPOSITOR_ALLOCATE_CFB,
	          &req) < 0) {
		memset(cfb_id_out, 0, sizeof(*cfb_id_out));
		return -1;
	}

	/* Sanity check that we've received a valid CFB. */
	CHERI_CLC(1, 0, cfb_id_out, 0);
	CHERI_CGETTAG(tagged, 1);
	CHERI_CCLEARTAG(1); /* don't leak it */

	if (!tagged) {
		fprintf(stderr,
		        "allocate_cfb: Received an invalid capability!\n");
		errno = EPERM;
		return -1;
	}

	return 0;
}

/**
 * Allocate a CFB in the compositor and return both its CFB ID and CFB token.
 * The CFB ID is written to cfb_id_out, and the CFB token cfb_token_out (which
 * must both be 32-byte aligned) on success, and 0 is returned. On failure, -1
 * is returned, cfb_id_out and cfb_token_out are cleared, and errno is set as
 * appropriate.
 *
 * See the documentation for compositor_allocate_cfb() for more details.
 *
 * compositor: Compositor mapping to use.
 * width: Width of the CFB, in tiles. Must be positive (notably, non-zero).
 * height: Height of the CFB, in tiles. Must be positive (notably, non-zero).
 * cfb_id_out: Return address for the CFB's ID capability. Must be 32-byte
 * aligned.
 * cfb_token_out: Return address for the CFB's token capability. Must be 32-byte
 * aligned.
 * Returns: 0 on success, -1 (and errno set) on failure.
 */
int
compositor_allocate_cfb_with_token(struct compositor_mapping *compositor,
                                   uint32_t width, uint32_t height,
                                   compositor_cfb_id *cfb_id_out,
                                   compositor_cfb_token *cfb_token_out)
{
	int retval = 0;

	/* Allocate the CFB. */
	retval = compositor_allocate_cfb(compositor, width, height, cfb_id_out);
	if (retval != 0) {
		return retval;
	}

	/* Convert the CFB ID to a CFB token. */
	retval = compositor_cfb_id_to_token(compositor, cfb_id_out,
	                                    cfb_token_out);
	if (retval != 0) {
		/* Free the CFB. */
		compositor_free_cfb(compositor, cfb_id_out);
		return retval;
	}

	/* Success! */
	return 0;
}

/* Convert a pixel in host order (e.g. as given in a char array outputted by the
 * GIMP) with non-premultiplied components to the RGBA format expected by
 * hardware (alpha at the least significant end; premultiplied components). */
static void
host_nonpremultiplied_to_rgba_premultiplied(uint32_t host_pixel,
                                            struct compositor_rgba_pixel *rgba_pixel)
{
	uint32_t _rgba_pixel;
	uint8_t red, green, blue, alpha;

	/* Fix endianness. */
	_rgba_pixel = htole32(host_pixel);

	/* FIXME: There's probably a more efficient way to do this. A
	 * more-caffeinated person may be able to find it. */
	red = (_rgba_pixel >> 0) & 0xff;
	green = (_rgba_pixel >> 8) & 0xff;
	blue = (_rgba_pixel >> 16) & 0xff;
	alpha = (_rgba_pixel >> 24) & 0xff;

	/* Premultiply the pixel. The division by 0xff is for the fixed point
	 * scaling factor present in both the colour and alpha components. */
	rgba_pixel->red = (red * alpha) / 0xff;
	rgba_pixel->green = (green * alpha) / 0xff;
	rgba_pixel->blue = (blue * alpha) / 0xff;
	rgba_pixel->alpha = alpha;
}

#define DIVIDE_ROUND(N, D) (((N) + (D) - 1) / (D))
#define ROUND_UP(N, D) (DIVIDE_ROUND(N, D) * (D))

/** 
 * Load an image with the given width and height into the CFB. The CFB must have
 * at least the same width and height as the image; note that CFBs' dimensions
 * must be multiples of TILE_SIZE, so this function will top-left-align the
 * image and fill the rest of the CFB with transparency. pixels must be at least
 * width×height×4 bytes long, and contain pixel data in premultiplied RGBA in
 * host byte order (e.g. as from a char array outputted by the GIMP).
 * cfb_width, cfb_height, image_width and image_height are given in pixels.
 *
 * cfb_id: CFB's capability.
 * cfb_width: Width of the CFB, in pixels.
 * cfb_height: Height of the CFB, in pixels.
 * image_width: Width of the image, in pixels.
 * image_height: Height of the image, in pixels.
 * host_pixels: Array of pixels in left-to-right, top-to-bottom order, host
 *              endianness.
 */
void
compositor_write_cfb_data_image(const struct chericap *cfb_id,
                                unsigned int cfb_width, unsigned int cfb_height,
                                unsigned int image_width,
                                unsigned int image_height,
                                const uint32_t *host_pixels)
{
	unsigned int x, y; /* pixels */
	unsigned int cfb_row_stride, cfb_column_stride; /* pixels */

	/* Load up the capability. */
	CHERI_CLC(1, 0, cfb_id, 0);

	cfb_row_stride = ROUND_UP(cfb_width, TILE_SIZE);
	cfb_column_stride = ROUND_UP(cfb_height, TILE_SIZE);

	/* FIXME: For the moment, write the data pixel-by-pixel, because it's
	 * simpler. This could be improved in the future to use memcpy() (if
	 * endianness issues don't get in the way). */
	for (y = 0; y < cfb_column_stride; y++) {
		for (x = 0; x < cfb_row_stride; x++) {
			union {
				struct compositor_rgba_pixel c;
				uint32_t v;
			} rgba_pixel;

			/* Have we gone off the end of the image? */
			if (x >= image_width || y >= image_height) {
				rgba_pixel.c.alpha = 0;
				rgba_pixel.c.red = 0;
				rgba_pixel.c.green = 0;
				rgba_pixel.c.blue = 0;
			} else {
				/* Still in the image. */
				host_nonpremultiplied_to_rgba_premultiplied(
					host_pixels[y * image_width + x],
					&rgba_pixel.c);
			}

			/* Write out the pixel. */
			WRITE_PIXEL(1, (y * cfb_row_stride + x), rgba_pixel.v);
		}
	}

	/* Don't leak the capability. */
	CHERI_CCLEARTAG(1);
}

/**
 * Fill a CFB with a two-colour chequered test pattern.
 *
 * cfb_id: CFB's capability.
 * cfb_width: Width of the CFB, in pixels.
 * cfb_height: Height of the CFB, in pixels.
 */
void
compositor_write_cfb_data_chequered(const struct chericap *cfb_id,
                                    unsigned int cfb_width,
                                    unsigned int cfb_height)
{
	unsigned int x, y; /* pixels */
	unsigned int cfb_row_stride, cfb_column_stride; /* pixels */

	/* Load up the capability. */
	CHERI_CLC(1, 0, cfb_id, 0);

	cfb_row_stride = ROUND_UP(cfb_width, TILE_SIZE);
	cfb_column_stride = ROUND_UP(cfb_height, TILE_SIZE);

	for (y = 0; y < cfb_column_stride; y++) {
		for (x = 0; x < cfb_row_stride; x++) {
			/* FIXME: For the moment, access each pixel
			 * individually. In the future it would be nice to use
			 * an optimised memcpy() instead. */
			union {
				struct compositor_rgba_pixel c;
				uint32_t v;
			} pixel;

			/* Have we gone off the end of the CFB? */
			if (x >= cfb_width || y >= cfb_height) {
				pixel.c.alpha = 0;
				pixel.c.red = 0;
				pixel.c.green = 0;
				pixel.c.blue = 0;
			} else {
				pixel.c.alpha = 0xff;
				pixel.c.red = 0xff;
				pixel.c.green = 0xff;
				pixel.c.blue = 0xff;
			}

			if (((x / 8) % 2) == ((y / 8) % 2)) {
				pixel.c.red = 0xb0;
				pixel.c.green = 0xb0;
				pixel.c.blue = 0xb0;
			}

			WRITE_PIXEL(1, (y * cfb_row_stride + x), pixel.v);
		}
	}

	/* Don't leak the capability. */
	CHERI_CCLEARTAG(1);
}

/**
 * Fill a CFB with a solid colour.
 *
 * cfb_id: CFB's capability.
 * cfb_width: Width of the CFB, in pixels.
 * cfb_height: Height of the CFB, in pixels.
 * pixel: Colour to fill the tile with.
 */
void
compositor_write_cfb_data_solid(const struct chericap *cfb_id,
                                unsigned int cfb_width, unsigned int cfb_height,
                                struct compositor_rgba_pixel *pixel)
{
	unsigned int x, y; /* pixels */
	unsigned int cfb_row_stride, cfb_column_stride; /* pixels */
	uint64_t pixels;

	/* Sort out the pixel data to write. */
	pixels = (uint64_t) (((uint32_t) pixel->alpha << 24) |
	                     ((uint32_t) pixel->blue << 16) |
	                     ((uint32_t) pixel->green << 8) |
	                     ((uint32_t) pixel->red << 0)) << 32 |
	         (uint64_t) (((uint32_t) pixel->alpha << 24) |
	                     ((uint32_t) pixel->blue << 16) |
	                     ((uint32_t) pixel->green << 8) |
	                     ((uint32_t) pixel->red << 0));

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
			} else {
				pixel_pair = pixels;
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

/**
 * Update the metadata for a CFB. This sets the CFB's X, Y and Z coordinates,
 * and whether it's currently being updated (i.e. its pixel data modified in
 * memory) and hence shouldn't be rendered.
 *
 * This allows an opaque sub-region of the CFB to be specified, as an
 * optimisation to improve rendering speed. Rendering of layers beneath the
 * opaque region may not be performed, so incorrect specification of the opaque
 * region (i.e. specifying a region which includes at least one non-opaque
 * pixel) may result in incorrect rendering. The opaque region may be empty.
 *
 * Note that this takes a CFB token, rather than a CFB ID, as input to specify
 * which CFB to use. This allows for separation between processes which may
 * modify a CFB's pixel data, and processes which may modify its metadata. A CFB
 * ID can be translated into a CFB token using compositor_cfb_id_to_token().
 *
 * compositor: Compositor mapping to use.
 * cfb_token: CFB's token capability. Must be 32-byte aligned.
 * x_position: New X coordinate for the top-left corner of the CFB, in pixels
 *             relative to the top-left of the screen.
 * x_position: New Y coordinate for the top-left corner of the CFB, in pixels
 *             relative to the top-left of the screen.
 * z_position: New Z coordinate for the CFB. Higher Z coordinates are closer to
 *             the user (i.e. are stacked higher in composition).
 * opaque_x: X coordinate for the top-left corner of the opaque sub-region of
 *           the CFB, in pixels relative to the top-left of the CFB.
 * opaque_y: Y coordinate for the top-left corner of the opaque sub-region of
 *           the CFB, in pixels relative to the top-left of the CFB.
 * opaque_width: Width of the opaque sub-region of the CFB, in pixels. Set this
 *               to 0 to disable the opaque sub-region.
 * opaque_height: Height of the opaque sub-region of the CFB, in pixels. Set
 *                this to 0 to disable the opaque sub-region.
 * update_in_progress: True to disable rendering of this CFB, false to enable
 *                     it. This allows for double-buffering to be implemented in
 *                     software.
 * Returns: 0 on success, -1 (and errno set) on failure.
 */
int
compositor_update_cfb(struct compositor_mapping *compositor,
                      struct chericap *cfb_token,
                      unsigned int x_position, unsigned int y_position,
                      unsigned int z_position, unsigned int opaque_x,
                      unsigned int opaque_y, unsigned int opaque_width,
                      unsigned int opaque_height, boolean_t update_in_progress)
{
	struct cheri_compositor_update_cfb req;

	req.cfb_token = cfb_token;
	req.x_position = x_position;
	req.y_position = y_position;
	req.z_position = z_position;
	req.opaque_x = opaque_x;
	req.opaque_y = opaque_y;
	req.opaque_width = opaque_width;
	req.opaque_height = opaque_height;
	req.update_in_progress = update_in_progress;

	return ioctl(compositor->control_fd, CHERI_COMPOSITOR_UPDATE_CFB, &req);
}

/**
 * Free a CFB. This stops it being rendered and frees the compositor memory
 * which was storing its pixel data.
 *
 * compositor: Compositor mapping to use.
 * cfb_id: CFB's capability. Must be 32-byte aligned.
 * Returns: 0 on success, -1 (and errno set) on failure.
 */
int
compositor_free_cfb(struct compositor_mapping *compositor,
                    struct chericap *cfb_id)
{
	struct cheri_compositor_free_cfb req;

	req.cfb_id = (struct chericap *) cfb_id;

	return ioctl(compositor->control_fd, CHERI_COMPOSITOR_FREE_CFB, &req);
}

/**
 * Translate a CFB ID capability to a CFB token capability. This changes the
 * capability to be physically addressed (and thus independent of any process’
 * virtual address space) and seals it so that it may only be used as a token.
 * To convert a CFB token back into a CFB ID (potentially with respect to a
 * different process’ virtual address space), use compositor_cfb_token_to_id().
 *
 * compositor: Compositor mapping to use.
 * cfb_id: CFB ID to convert. Must be 32-byte aligned.
 * cfb_token: Return address for the resulting CFB token. Must be 32-byte
 * aligned.
 * Returns: 0 on success, -1 (and errno set) on failure.
 */
int
compositor_cfb_id_to_token(struct compositor_mapping *compositor,
                           struct chericap *cfb_id, struct chericap *cfb_token)
{
	struct cheri_compositor_cfb_id_to_token req;

	req.cfb_id = cfb_id;
	req.cfb_token = (caddr_t) cfb_token;

	return ioctl(compositor->control_fd, CHERI_COMPOSITOR_CFB_ID_TO_TOKEN,
	             &req);
}

/**
 * Translate a CFB token capability to a CFB ID capability. This changes the
 * capability to be virtually addressed (and thus tied to a specific process’
 * virtual address space) and unseals it so that it may only be used as an ID.
 * To convert a CFB ID back into a CFB token, use compositor_cfb_id_to_token().
 *
 * compositor: Compositor mapping to use.
 * cfb_token: CFB token to convert. Must be 32-byte aligned.
 * cfb_id: Return address for the resulting CFB ID. Must be 32-byte aligned.
 * Returns: 0 on success, -1 (and errno set) on failure.
 */
int
compositor_cfb_token_to_id(struct compositor_mapping *compositor,
                           struct chericap *cfb_token, struct chericap *cfb_id)
{
	struct cheri_compositor_cfb_token_to_id req;

	req.cfb_token = cfb_token;
	req.cfb_id = (caddr_t) cfb_id;

	return ioctl(compositor->control_fd, CHERI_COMPOSITOR_CFB_TOKEN_TO_ID,
	             &req);
}

/**
 * Set the compositor's configuration. This is currently just the output
 * resolution, in pixels.
 *
 * compositor: Compositor mapping to use.
 * x_resolution: New X output resolution, in pixels. Set to 0 to disable output.
 * y_resolution: New Y output resolution, in pixels. Set to 0 to disable output.
 * Returns: 0 on success, -1 (and errno set) on failure.
 */
int
compositor_set_configuration(struct compositor_mapping *compositor,
                             unsigned int x_resolution,
                             unsigned int y_resolution)
{
	struct cheri_compositor_set_configuration req;

	req.configuration.x_resolution = x_resolution;
	req.configuration.y_resolution = y_resolution;

	return ioctl(compositor->control_fd, CHERI_COMPOSITOR_SET_CONFIGURATION,
	             &req);
}

/**
 * Get hardware parameters of the compositor and attached display. These are
 * immutable.
 *
 * compositor: Compositor mapping to use.
 * parameters_out: Return address for the hardware parameters.
 */
void
compositor_get_parameters(struct compositor_mapping *compositor __unused,
                          struct compositor_parameters *parameters_out)
{
	/* From Section 3.2 of the MTL User Manual, Vol. 11.
	 * http://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&CategoryNo=68&No=653&PartNo=4 */
	#define PIXEL_RATE 33000000 /* Hertz */
	#define HORIZONTAL_PERIOD 1056 /* pixels */
	#define HORIZONTAL_PULSE_WIDTH 30 /* pixels */
	#define HORIZONTAL_BACK_PORCH 16 /* pixels */
	#define HORIZONTAL_FRONT_PORCH 210 /* pixels */
	#define HORIZONTAL_VALID \
		(HORIZONTAL_PERIOD - HORIZONTAL_BACK_PORCH - \
		 HORIZONTAL_FRONT_PORCH - HORIZONTAL_PULSE_WIDTH) /* pixels */
	#define VERTICAL_PERIOD 525 /* lines */
	#define VERTICAL_PULSE_WIDTH 13 /* lines */
	#define VERTICAL_BACK_PORCH 10 /* lines */
	#define VERTICAL_FRONT_PORCH 22 /* lines */
	#define VERTICAL_VALID \
		(VERTICAL_PERIOD - VERTICAL_BACK_PORCH - \
		 VERTICAL_FRONT_PORCH - VERTICAL_PULSE_WIDTH) /* lines */

	/* FIXME: Sometime, it would be nice to get these from hardware. At the
	 * moment, they're hard-coded for the Terasic MTL LCD on the DE4. */
	parameters_out->native_x_resolution = HORIZONTAL_VALID; /* pixels */
	parameters_out->native_y_resolution = VERTICAL_VALID; /* pixels */
	parameters_out->refresh_rate =
		PIXEL_RATE / (HORIZONTAL_PERIOD * VERTICAL_PERIOD); /* Hertz */
	parameters_out->dpi = 96; /* dots per inch */
	parameters_out->subpixel = COMPOSITOR_SUBPIXEL_NONE;
	parameters_out->pixel_format = COMPOSITOR_b8g8r8a8;
}

/**
 * Get performance sample values for the compositor and its memory bus. These
 * are the current values of various performance counters in the hardware,
 * incremented as appropriate since the most recent call to
 * compositor_reset_statistics(). Fetching the counters is only guaranteed to be
 * atomic if done while sampling is paused (using
 * compositor_pause_statistics()).
 *
 * On failure, the outputted statistics are all set to 0.
 *
 * compositor: Compositor mapping to use.
 * statistics_out: Return address for the compositor statistics.
 * Returns: 0 on success, -1 (and errno set) on failure.
 */
int
compositor_get_statistics(struct compositor_mapping *compositor,
                          struct compositor_statistics *statistics_out)
{
	int retval = 0;
	struct cheri_compositor_get_statistics req;
	struct cheri_compositor_statistics stats;
	unsigned int i;

	req.stats_out = (caddr_t) &stats;

	retval = ioctl(compositor->control_fd, CHERI_COMPOSITOR_GET_STATISTICS,
	               &req);
	if (retval != 0) {
		/* Set out parameters to safe values. */
		memset(statistics_out, 0, sizeof(*statistics_out));

		return retval;
	}

	/* Copy out the memory bus statistics. */
	statistics_out->num_read_requests = stats.num_read_requests;
	statistics_out->num_write_requests = stats.num_write_requests;
	statistics_out->num_read_bursts = stats.num_read_bursts;
	statistics_out->num_write_bursts = stats.num_write_bursts;
	statistics_out->num_latency_bins = stats.num_latency_bins;
	statistics_out->latency_bin_lower_bound = stats.latency_bin_lower_bound;
	statistics_out->latency_bin_upper_bound = stats.latency_bin_upper_bound;

	for (i = 0;
	     i < sizeof(statistics_out->latency_bins) /
	         sizeof(*statistics_out->latency_bins);
	     i++) {
		statistics_out->latency_bins[i] = stats.latency_bins[i];
	}

	/* And the compositor statistics. */
	statistics_out->num_compositor_cycles = stats.num_compositor_cycles;
	statistics_out->num_tce_requests = stats.num_tce_requests;
	statistics_out->num_memory_requests = stats.num_memory_requests;
	statistics_out->num_frames = stats.num_frames;

	for (i = 0;
	     i < sizeof(statistics_out->pipeline_stage_cycles) /
	         sizeof(*statistics_out->pipeline_stage_cycles);
	     i++) {
		statistics_out->pipeline_stage_cycles[i] =
			stats.pipeline_stage_cycles[i];
	}

	return retval;
}

/**
 * Pause statistics sampling. This does not change the sample counter values.
 * Sampling can be resumed by calling compositor_unpause_statistics().
 *
 * This is idempotent if sampling is already paused.
 *
 * compositor: Compositor mapping to use.
 * Returns: 0 on success, -1 (and errno set) on failure.
 */
int
compositor_pause_statistics(struct compositor_mapping *compositor)
{
	return ioctl(compositor->control_fd, CHERI_COMPOSITOR_PAUSE_STATISTICS,
	             NULL);
}

/**
 * Unpause statistics sampling. This does not change the sample counter values.
 * Sampling can be paused by calling compositor_pause_statistics().
 *
 * This is idempotent if sampling is already unpaused.
 *
 * compositor: Compositor mapping to use.
 * Returns: 0 on success, -1 (and errno set) on failure.
 */
int
compositor_unpause_statistics(struct compositor_mapping *compositor)
{
	return ioctl(compositor->control_fd,
	             CHERI_COMPOSITOR_UNPAUSE_STATISTICS, NULL);
}

/**
 * Reset statistics sample counters. This does not change the paused state of
 * sampling, and can be called at any time. If called while sampling is
 * unpaused, the counters are not guaranteed to be reset atomically.
 *
 * compositor: Compositor mapping to use.
 * Returns: 0 on success, -1 (and errno set) on failure.
 */
int
compositor_reset_statistics(struct compositor_mapping *compositor)
{
	return ioctl(compositor->control_fd, CHERI_COMPOSITOR_RESET_STATISTICS,
	             NULL);
}
