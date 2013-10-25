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
#include <pixman.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/endian.h>
#include <sys/ioccom.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <machine/cheri.h>

#include <compositor.h>

/**
 * Test program for the CHERI compositor. This displays different numbers of
 * full-screen CFBs and measures the number of memory requests made by the
 * compositor, plus their request–response latency, over a 500ms period while
 * displaying this number of layers. This is repeated for 1 to N layers, where N
 * is specified on the command line.
 *
 * The program outputs a CSV file of results to standard output. Any
 * informational messages are output to standard error.
 *
 * The CSV output contains the number of read & write requests made by the
 * compositor (the latter should always be 0), plus the number of
 * request–response pairs which were counted in each of the latency bins
 * exposed by the AvalonSampler peripheral. These bins are currently:
 *  (0, 2), [2, 4), [4, 6), …, [28, 30), [30, ∞).
 */

/* Device node to use for the compositor. */
#define COMPOSITOR_DEV "/dev/compositor_cfb0"

/* Global configuration. FIXME: Retrieve this dynamically. */
static unsigned int screen_width = 800;
static unsigned int screen_height = 480;

/* CHERI compositor state. */
static struct compositor_mapping cheri_compositor = { 0, NULL, 0 };

static void
print_csv_header(int argc, const char * const *argv)
{
	char formatted_date[50];
	time_t _time;
	struct tm *__time;
	int i;

	_time = time(NULL);
	__time = localtime(&_time);
	strftime(formatted_date, sizeof(formatted_date),
	         "%Y-%m-%d %H:%M:%S", __time);

	printf("# Generated on %s by `", formatted_date);

	for (i = 0; i < argc; i++) {
		printf((i > 0) ? " %s" : "%s", argv[i]);
	}
	printf("`\n");

	printf("n_layers, total_time, num_read_requests, num_write_requests, "
	       "latency_bin0, latency_bin1, latency_bin2, latency_bin3, "
	       "latency_bin4, latency_bin5, latency_bin6, latency_bin7, "
	       "latency_bin8, latency_bin9, latency_bin10, latency_bin11, "
	       "latency_bin12, latency_bin13, latency_bin14, latency_bin15, "
	       "git_id, configuration\n");
}

static void
print_csv_results_line(unsigned int n_layers, uint64_t total_time,
                       const struct compositor_statistics *stats,
                       const char *git_id, const char *configuration)
{
	unsigned int i;

	printf("%u, %.5f, %u, %u", n_layers, total_time / 1000000000.0,
	       stats->num_read_requests, stats->num_write_requests);

	for (i = 0; i < COMPOSITOR_STATISTICS_NUM_LATENCY_BINS; i++) {
		printf(", %u", stats->latency_bins[i]);
	}

	printf(", %s, %s\n", git_id, configuration);
}

/* Create and display n_layers full-screen CFBs, with the bottom one being
 * opaque and all others being translucent. Count the number of memory requests
 * made while displaying them for 500ms, and output a CSV results line to
 * stdout. */
static int
show_layers(unsigned int n_layers, const char *git_id,
            const char *configuration)
{
	uint64_t total_time; /* nanoseconds */
	struct compositor_rgba_pixel pixel;
	struct compositor_mapping *compositors;
	struct chericap *cfb_ids, *cfb_tokens;
	unsigned int num_requests;
	struct timespec tp_start, tp_end, ts;
	struct compositor_statistics stats_diff;
	int sleep_result, i;

	/* Allocate n_layers compositors, CFB IDs and CFB tokens. We can't just
	 * use the standard cheri_compositor for everything because the pool
	 * is too small. */
	compositors = malloc(sizeof(*compositors) * n_layers);
	if (compositors == NULL) {
		fprintf(stderr, "Error allocating compositors: %s\n",
		        strerror(errno));
		return 1;
	}

	if (posix_memalign((void **) &cfb_ids, CHERICAP_SIZE,
	                   sizeof(*cfb_ids) * n_layers) != 0) {
		fprintf(stderr, "Error allocating CFB IDs: %s\n",
		        strerror(errno));
		return 1;
	}

	if (posix_memalign((void **) &cfb_tokens, CHERICAP_SIZE,
	                   sizeof(*cfb_tokens) * n_layers) != 0) {
		fprintf(stderr, "Error allocating CFB tokens: %s\n",
		        strerror(errno));
		free(cfb_ids);
		return 1;
	}

	pixel.red = 0;
	pixel.blue = 0;
	pixel.green = 0;
	pixel.alpha = 0x10;

	/* Create n_layers layers, each of a different colour. Make the bottom
	 * layer opaque. Run the loop backwards because it magically improves
	 * the arbitration behaviour between the compositor and CPU, so they're
	 * less likely to conflict for the bus during the test. */
	for (i = n_layers - 1; i >= 0; i--) {
		/* Connect to the compositor. */
		if (compositor_map(COMPOSITOR_DEV, &compositors[i]) != 0) {
			fprintf(stderr, "Error initialising compositor %u: %s\n",
				i, strerror(errno));
			return 1;
		}

		/* Prepare the colour. */
		pixel.red += 10;
		pixel.green += 20;
		pixel.blue += 30;
		if (i == 0) {
			pixel.alpha = 0xff;
		}

		/* Allocate the CFB and get a token for it. */
		if (compositor_allocate_cfb_with_token(&compositors[i],
		                                       screen_width,
		                                       screen_height,
		                                       &cfb_ids[i],
		                                       &cfb_tokens[i]) != 0) {
			fprintf(stderr, "Error allocating CFB: %s\n",
				strerror(errno));
			return 1;
		}

		/* Write out the tiles. */
		compositor_write_cfb_data_solid(&cfb_ids[i], screen_width,
		                                screen_height, &pixel);

		/* Mark the CFB as finished updating. */
		if (compositor_update_cfb(&compositors[i], &cfb_tokens[i], 0,
			                  0, i, 0, 0, screen_width,
			                  screen_height, 0) != 0) {
			fprintf(stderr, "Error updating CFB: %s\n",
				strerror(errno));
			return 1;
		}
	}

	/* Pause and reset statistics. */
	if (compositor_pause_statistics(&cheri_compositor) != 0 ||
	    compositor_reset_statistics(&cheri_compositor) != 0) {
		fprintf(stderr, "Error pausing/resetting statistics: %s\n",
		        strerror(errno));
		return 1;
	}

	/* Sleep for 500ms to give the compositor a few tens of frames to make
	 * memory requests. Time the sleep period as closely as possible, and
	 * only collect statistics (by unpausing) during the sleep period. */
	ts.tv_sec = 0;
	ts.tv_nsec = 500 * 1000 * 1000;

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_start) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return 1;
	}

	compositor_unpause_statistics(&cheri_compositor);
	sleep_result = nanosleep(&ts, NULL);
	compositor_pause_statistics(&cheri_compositor);

	if (sleep_result != 0) {
		fprintf(stderr, "Failed to sleep correctly: %s\n",
		        strerror(errno));
		return 1;
	}

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_end) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return 1;
	}

	/* Grab the final number of requests. */
	if (compositor_get_statistics(&cheri_compositor, &stats_diff) != 0) {
		fprintf(stderr, "Error collecting final statistics: %s\n",
		        strerror(errno));
		return 1;
	}

	/* Tidy up. */
	for (i = 0; i < (int) n_layers; i++) {
		compositor_free_cfb(&compositors[i], &cfb_ids[i]);
		compositor_unmap(&compositors[i]);
	}

	free(cfb_ids);
	free(cfb_tokens);
	free(compositors);

	/* Calculate results. */
	total_time =
		((uint64_t) tp_end.tv_sec * 1000000000 + tp_end.tv_nsec) -
		((uint64_t) tp_start.tv_sec * 1000000000 + tp_start.tv_nsec);

	/* Output results informally. */
	num_requests =
		stats_diff.num_read_requests + stats_diff.num_write_requests;

	fprintf(stderr,
	        " - Made %u requests in %luns.\n",
	        num_requests, total_time);

	/* Output results formally. */
	print_csv_results_line(n_layers, total_time, &stats_diff, git_id,
	                       configuration);

	return 0;
}

/* Returns a main()-style error number. */
static int
map_hardware(void)
{
	/* Initialise the compositor. */
	if (compositor_map(COMPOSITOR_DEV, &cheri_compositor) != 0) {
		fprintf(stderr, "Error initialising the compositor: %s\n",
		        strerror(errno));
		return 1;
	}

	fprintf(stderr, "Opened ‘%s’ as FD %i.\n", COMPOSITOR_DEV,
	        cheri_compositor.control_fd);
	fprintf(stderr, "Mapped ‘%s’ as [%p, %p).\n", COMPOSITOR_DEV,
	        cheri_compositor.pixels,
	        (void *) ((uintptr_t) cheri_compositor.pixels +
	                  CHERI_COMPOSITOR_MEM_POOL_LENGTH));

	if (compositor_set_configuration(&cheri_compositor, screen_width,
	                                 screen_height) != 0) {
		fprintf(stderr, "Error setting resolution: %s\n",
		        strerror(errno));
		return 1;
	}

	return 0;
}

/* Returns a main()-style error number. */
static int
unmap_hardware(void)
{
	compositor_unmap(&cheri_compositor);
	return 0;
}

/* Returns a main()-style error number. */
static int
usage(void)
{
	printf("rendering-bandwidth git_id configuration [n_layers]\n\n");
	printf("This runs a series of memory bandwidth tests against the "
	       "compositor, outputting a CSV file of results to standard "
	       "output.\n\n");
	printf("The mandatory first and second parameters are free-form text "
	       "specifying the git ID and configuration ID of the hardware "
	       "being tested.\n");
	printf("The optional second parameter specifies the maximum number of "
	       "layers to render.\n");

	return 1;
}

int
main(int argc, char *argv[])
{
	int retval = 0;
	int n_layers;
	unsigned int i;
	const char *git_id, *configuration;

	if (argc < 3) {
		return usage();
	}

	/* Parse the git_id and configuration. */
	git_id = argv[1];
	configuration = argv[2];

	/* Parse the number of layers (or leave it at the default: -1). */
	if (argc == 4) {
		char *endptr;

		n_layers = strtol(argv[3], &endptr, 0);
		if (*endptr != '\0' || n_layers < 1) {
			fprintf(stderr, "Invalid layer count ‘%s’.\n", argv[1]);
			return usage();
		}
	} else {
		n_layers = -1;
	}

	/* Defaults. */
	if (n_layers == -1) {
		n_layers = 10;
	}

	/* Initialise the hardware. */
	retval = map_hardware();
	if (retval != 0)
		return retval;

	/* Output the CSV header. */
	print_csv_header(argc, (const char * const *) argv);

	/* Run the tests. */
	for (i = 1; i <= (unsigned int) n_layers; i++) {
		fprintf(stderr, "Running test for %u layers…\n", i);
		retval = show_layers(i, git_id, configuration);
		if (retval != 0) goto done;
	}

done:
	unmap_hardware();

	return retval;
}
