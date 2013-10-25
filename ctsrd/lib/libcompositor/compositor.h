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

#ifndef _COMPOSITOR_H_
#define	_COMPOSITOR_H_

/**
 * User-space interface library for the CHERI compositor. This provides a
 * thin, convenient API over the ioctl() interface provided by the CHERI
 * compositor driver. Many of these APIs require the calling process to hold
 * escalated privileges.
 *
 * For an overview of the CHERI compositor, the driver implementation, and
 * general concepts, see the comment at the top of cheri_compositor.h.
 */

#include <stdint.h>
#include <sys/ioccom.h>

/* FIXME: Install this in /usr/include properly, rather than copying it into
 * the libcompositor directory manually. */
#include "cheri_compositor.h"

#include <machine/cheri.h>

/**
 * RGBA pixel data, in the format expected by hardware:
 *  - Premultiplied alpha.
 *  - 8-bit components.
 *  - Alpha at the least significant end.
 */
struct compositor_rgba_pixel {
	uint8_t alpha;
	uint8_t blue;
	uint8_t green;
	uint8_t red;
} __packed;

/**
 * Mapping of a compositor device into memory. This gives the FD and virtual
 * address mapping of a single CFB pool in the compositor, which is allocated
 * to that FD. Multiple mappings of the same compositor device may exist; in
 * which case they will all use different FDs.
 *
 * Direct access to compositor memory can be performed through the pixels field,
 * but it is recommended to use the CFB capabilities returned by
 * compositor_allocate_cfb() instead, to allow for sandboxing.
 *
 * Invariants:
 *  - control_fd is always non-negative and a valid FD.
 *  - pixels is always non-null.
 *  - pixels points to a contiguous region of memory at least 4×pixels_length
 *    long in bytes.
 */
struct compositor_mapping {
	int control_fd;
	struct compositor_rgba_pixel *pixels;
	size_t pixels_length; /* in pixels */
};

/**
 * Hardware parameters for the compositor and attached display. These are
 * immutable, and provided for information only. These parameters are guaranteed
 * to not change for a given compositor and display pairing.
 *
 * native_x_resolution: Display's preferred X resolution, in pixels. If the
 *                      preferred resolution is unknown, this is 0.
 * native_y_resolution: Display's preferred Y resolution, in pixels. If the
 *                      preferred resolution is unknown, this is 0.
 * refresh_rate: Display's maximum refresh rate, in Hertz. If the refresh rate
 *               is unknown, this is 0.
 * dpi: Display's pixel density, in dots per inch. If the DPI is unknown, this
 *      is 0.
 * subpixel: Display's subpixel rendering capability. If unknown, this is
 *           COMPOSITOR_SUBPIXEL_UNKNOWN.
 * pixel_format: Pixel encoding format expected in memory by the compositor.
 */
struct compositor_parameters {
	unsigned int native_x_resolution;
	unsigned int native_y_resolution;
	unsigned int refresh_rate;
	unsigned int dpi;
	enum {
		COMPOSITOR_SUBPIXEL_UNKNOWN,
		COMPOSITOR_SUBPIXEL_NONE,
		COMPOSITOR_SUBPIXEL_HORIZONTAL_RGB,
		COMPOSITOR_SUBPIXEL_HORIZONTAL_BGR,
		COMPOSITOR_SUBPIXEL_VERTICAL_RGB,
		COMPOSITOR_SUBPIXEL_VERTICAL_BGR,
	} subpixel;
	enum {
		COMPOSITOR_b8g8r8a8,
	} pixel_format;
};

/**
 * Performance sampling counter values from the compositor and its memory bus
 * monitor peripheral. These give samples of the current performance of the
 * compositor, counted since the most recent call to
 * compositor_reset_statistics(). The samples are only guaranteed to be fetched
 * atomically if compositor_get_statistics() is called while sampling is paused
 * (using compositor_pause_statistics()).
 *
 * num_read_requests: Number of memory read requests sent by the compositor.
 * num_write_requests: Number of memory write requests sent by the compositor.
 * num_read_bursts: Number of memory read requests with a burst length greater
 *                  than 1 sent by the compositor.
 * num_write_bursts: Number of memory write requests with a burst length greater
 *                   than 1 sent by the compositor.
 * num_latency_bins: Number of latency histogram bins which are valid in
 *                   latency_bins.
 * latency_bin_lower_bound: Inclusive lower bound of latency represented by the
 *                          lowest index in latency_bins, in cycles.
 * latency_bin_upper_bound: Exclusive upper bound of latency represented by the
 *                          highest index in latency_bins, in cycles.
 * latency_bins: Array of latency bins, each containing the number of memory
 *               read request–response pairs received by the compositor with the
 *               given latency, in cycles.
 * num_compositor_cycles: Number of clock cycles the compositor has run for.
 * pipeline_stage_cycles: Array of counters for the number of packets sent
 *                        between each pair of adjacent pipeline stages in the
 *                        compositor. The 0th entry gives the number of packets
 *                        received at the head of the pipeline, and the final
 *                        entry gives the number of packets sent between the
 *                        penultimate and ultimate pipeline stages.
 * num_tce_requests: Number of requests sent by the compositor to a tile cache.
 * num_memory_requests: Number of requests sent by the compositor to external
 *                      memory.
 * num_frames: Number of whole frames rendered by the compositor.
 */
struct compositor_statistics {
	/* Memory bus statistics. */
	uint32_t num_read_requests;
	uint32_t num_write_requests;
	uint32_t num_read_bursts;
	uint32_t num_write_bursts;

	unsigned int num_latency_bins;
	uint8_t latency_bin_lower_bound;
	uint8_t latency_bin_upper_bound;

#define COMPOSITOR_STATISTICS_NUM_LATENCY_BINS 16
	uint32_t latency_bins[COMPOSITOR_STATISTICS_NUM_LATENCY_BINS];

	/* Compositor statistics. */
	uint32_t num_compositor_cycles;
#define COMPOSITOR_STATISTICS_NUM_PIPELINE_STAGES 5
	uint32_t pipeline_stage_cycles
		[CHERI_COMPOSITOR_STATISTICS_NUM_PIPELINE_STAGES + 1];
	uint32_t num_tce_requests;
	uint32_t num_memory_requests;
	uint32_t num_frames;
};

/**
 * CFB ID capability type. This is an unsealed CHERI capability which represents
 * a single CFB with respect to a given process’ virtual address space.
 */
typedef struct chericap compositor_cfb_id;

/**
 * CFB token capability type. This is a sealed CHERI capability which represents
 * a single CFB using physical addressing, irrespective of any process’ virtual
 * address space.
 */
typedef struct chericap compositor_cfb_token;

/* Device functions. */
int compositor_map(const char *device_path,
                   struct compositor_mapping *compositor_out);
void compositor_unmap(struct compositor_mapping *compositor);

int compositor_set_configuration(struct compositor_mapping *compositor,
                                 unsigned int x_resolution,
                                 unsigned int y_resolution);

void compositor_get_parameters(struct compositor_mapping *compositor,
                               struct compositor_parameters *parameters_out);

int compositor_get_statistics(struct compositor_mapping *compositor,
                              struct compositor_statistics *statistics_out);
int compositor_pause_statistics(struct compositor_mapping *compositor);
int compositor_unpause_statistics(struct compositor_mapping *compositor);
int compositor_reset_statistics(struct compositor_mapping *compositor);

/* CFB functions. */
int compositor_allocate_cfb(struct compositor_mapping *compositor,
                            uint32_t width, uint32_t height,
                            compositor_cfb_id *cfb_id_out);
int compositor_allocate_cfb_with_token(struct compositor_mapping *compositor,
                                       uint32_t width, uint32_t height,
                                       compositor_cfb_id *cfb_id_out,
                                       compositor_cfb_token *cfb_token_out);
int compositor_update_cfb(struct compositor_mapping *compositor,
                          compositor_cfb_token *cfb_token,
                          unsigned int x_position, unsigned int y_position,
                          unsigned int z_position, unsigned int opaque_x,
                          unsigned int opaque_y, unsigned int opaque_width,
                          unsigned int opaque_height,
                          boolean_t update_in_progress);
int compositor_free_cfb(struct compositor_mapping *compositor,
                        compositor_cfb_id *cfb_id);

/* CFB capability translation functions. */
int compositor_cfb_id_to_token(struct compositor_mapping *compositor,
                               compositor_cfb_id *cfb_id,
                               compositor_cfb_token *cfb_token);
int compositor_cfb_token_to_id(struct compositor_mapping *compositor,
                               compositor_cfb_token *cfb_token,
                               compositor_cfb_id *cfb_id);

/* Pixel data functions. */
void compositor_write_cfb_data_image(const compositor_cfb_id *cfb_id,
                                     unsigned int cfb_width,
                                     unsigned int cfb_height,
                                     unsigned int image_width,
                                     unsigned int image_height,
                                     const uint32_t *host_pixels);
void compositor_write_cfb_data_chequered(const compositor_cfb_id *cfb_id,
                                         unsigned int cfb_width,
                                         unsigned int cfb_height);
void compositor_write_cfb_data_solid(const compositor_cfb_id *cfb_id,
                                     unsigned int cfb_width,
                                     unsigned int cfb_height,
                                     struct compositor_rgba_pixel *pixel);

#endif /* _COMPOSITOR_H_ */
