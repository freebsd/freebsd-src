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

/* Image sources. */
#include "../images/chequerboard.png.c"
#include "../images/crest.png.c"
#include "../images/opaque-square.png.c"
#include "../images/translucent-square.png.c"

/**
 * Test program for the CHERI compositor. This collects various statistics about
 * the compositor or the standard frame buffer. It’s designed to be run once
 * against the compositor, and once against the standard frame buffer. In each
 * case, it will output a CSV file of results to standard output. Any
 * informational messages are output to standard error.
 *
 * Tests:
 *  • bg:
 *      Renders a constantly-changing background colour uniformly across the
 *      screen, testing full-screen pixel data bandwidth. It measures the
 *      mean frame rate. On both the CHERI compositor and the standard frame
 *      buffer this is implemented by writing to every pixel on the screen.
 *  • opaque_cfb:
 *      Moves a single opaque image around on a chequered background, testing
 *      pixel blitting performance. It measures the mean time per movement of
 *      the image. On the CHERI compositor, this is implemented by moving a CFB;
 *      on the standard frame buffer it uses software composition/blitting.
 *  • translucent_cfb:
 *      Similar to opaque_cfb, this moves a translucent image around,
 *      so cannot be implemented using blitting. On the CHERI compositor, it is
 *      still implemented by moving a CFB around; on the standard frame buffer,
 *      full alpha composition (the ‘over’ operator) has to be used.
 *  • binary_cfb:
 *      Similar to opaque_cfb, this moves a partially transparent image around,
 *      where each pixel is either fully opaque or fully transparent (i.e.
 *      binary alpha). On the CHERI compositor, it is still implemented by
 *      moving a CFB around; on the standard frame buffer, binary alpha
 *      composition (masking) is used.
 */

/* Device nodes to use for the compositor and frame buffer. */
#define COMPOSITOR_DEV "/dev/compositor_cfb0"
#define FB_DEV "/dev/mtl_pixel0"
#define FB_CONTROL_DEV "/dev/mtl_reg0"

/* Global configuration. FIXME: Retrieve this dynamically. */
static unsigned int screen_width = 800;
static unsigned int screen_height = 480;

/* CHERI compositor state. */
static struct compositor_mapping cheri_compositor = { 0, NULL, 0 };

/* Standard frame buffer state. */
static void *fb_pixels = NULL;
static size_t fb_pixels_length = 0;
static pixman_image_t *fb_pixman = NULL;

/* Whether we're running against the CHERI compositor, or a standard frame
 * buffer. */
typedef enum {
	HARDWARE_COMPOSITOR,
	HARDWARE_FB,
} hardware_t;

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

	printf("test, hardware, n_repeats, total_time, git_id, "
	       "configuration\n");
}

static const char *
hardware_to_string(hardware_t hardware)
{
	switch(hardware) {
	case HARDWARE_COMPOSITOR:
		return "compositor";
	case HARDWARE_FB:
		return "frame_buffer";
	default:
		return NULL;
	}
}

static void
print_csv_results_line(const char *test_name, hardware_t hardware,
                       unsigned int n_repeats, uint64_t total_time,
                       const char *git_id, const char *configuration)
{
	printf("%s, %s, %u, %.5f, %s, %s\n", test_name,
	       hardware_to_string(hardware), n_repeats,
	       total_time / 1000000000.0, git_id, configuration);
}

static int
malloc_cfb_id_and_token(struct chericap **cfb_id_out,
                          struct chericap **cfb_token_out)
{
	if (posix_memalign((void **) cfb_id_out, CHERICAP_SIZE,
	                   sizeof(**cfb_id_out)) != 0) {
		return 1;
	}

	if (posix_memalign((void **) cfb_token_out, CHERICAP_SIZE,
	                   sizeof(**cfb_token_out)) != 0) {
		free(*cfb_id_out);
		return 1;
	}

	return 0;
}

/* ‘bg’ test. See the top of the file for a description. */
static int
test_bg(hardware_t hardware, unsigned int n_repeats, const char *git_id,
        const char *configuration)
{
	uint64_t total_time; /* nanoseconds */

	switch(hardware) {
	case HARDWARE_COMPOSITOR: {
		struct chericap *cfb_id, *cfb_token; /* FIXME: still somehow not aligned properly */
		struct compositor_rgba_pixel pixel;
		unsigned int i;
		struct timespec tp_start, tp_end;
		int r_direction, g_direction, b_direction;

		if (malloc_cfb_id_and_token(&cfb_id, &cfb_token) != 0) {
			fprintf(stderr, "Error allocating CFB ID/token: %s\n",
			        strerror(errno));
			return 1;
		}

		/* Allocate the CFB and get a token for it. */
		if (compositor_allocate_cfb_with_token(&cheri_compositor,
		                                       screen_width,
		                                       screen_height, cfb_id,
		                                       cfb_token) != 0) {
			fprintf(stderr, "Error allocating CFB: %s\n",
			        strerror(errno));
			return 1;
		}

		/* Prepare the colour. */
		pixel.red = 0;
		pixel.green = 0;
		pixel.blue = 0;
		pixel.alpha = 0xff;

		r_direction = g_direction = b_direction = -1;

		/* Write out the tiles. */
		compositor_write_cfb_data_solid(cfb_id, screen_width,
		                                screen_height, &pixel);

		/* Mark the CFB as finished updating. */
		if (compositor_update_cfb(&cheri_compositor, cfb_token, 0,
		                          0, 0, 0, 0, screen_width,
		                          screen_height, 0) != 0) {
			fprintf(stderr, "Error updating CFB: %s\n",
			        strerror(errno));
			return 1;
		}

		/* Continually change the colour, and time n_repeats of it. */
		if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_start) != 0) {
			fprintf(stderr, "Failed to get time: %s\n",
			        strerror(errno));
			return 1;
		}

		for(i = 0; i < n_repeats; i++) {
			/* Smoothly switch between colours by sliding up and
			 * down the red, green and blue components without large
			 * jumps; reverse direction at either end. */
			if (pixel.red <= 0 || pixel.red >= 255)
				r_direction *= -1;
			if (pixel.green <= 2 || pixel.green >= 253)
				g_direction *= -1;
			if (pixel.blue <= 4 || pixel.blue >= 251)
				b_direction *= -1;

			pixel.red += r_direction * 1;
			pixel.green += g_direction * 3;
			pixel.blue += b_direction * 5;

			/* Write out the tiles. */
			compositor_write_cfb_data_solid(cfb_id, screen_width,
			                                screen_height, &pixel);
		}

		if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_end) != 0) {
			fprintf(stderr, "Failed to get time: %s\n",
			        strerror(errno));
			return 1;
		}

		/* Tidy up. */
		compositor_free_cfb(&cheri_compositor, cfb_id);
		free(cfb_id);
		free(cfb_token);

		/* Calculate results. */
		total_time =
			((uint64_t) tp_end.tv_sec * 1000000000 + tp_end.tv_nsec) -
			((uint64_t) tp_start.tv_sec * 1000000000 + tp_start.tv_nsec);

		break;
	}
	case HARDWARE_FB: {
		pixman_color_t pixel;
		pixman_rectangle16_t rect;
		unsigned int i;
		struct timespec tp_start, tp_end;
		int r_direction, g_direction, b_direction;

		/* Prepare the colour. pixman uses 16-bit colour values, but
		 * converts them to 8 bits internally. So just use 8-bit colour
		 * values shifted up a bit. */
		pixel.red = 0 << 8;
		pixel.green = 0 << 8;
		pixel.blue = 0 << 8;
		pixel.alpha = 0xff << 8;

		r_direction = g_direction = b_direction = -1;

		/* Prepare the refresh rectangle: the entire screen. */
		rect.x = 0;
		rect.y = 0;
		rect.width = screen_width;
		rect.height = screen_height;

		/* Continually change the colour, and time n_repeats of it. */
		if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_start) != 0) {
			fprintf(stderr, "Failed to get time: %s\n",
			        strerror(errno));
			return 1;
		}

		for(i = 0; i < n_repeats; i++) {
			/* Smoothly switch between colours by sliding up and
			 * down the red, green and blue components without large
			 * jumps; reverse direction at either end. */
			if (pixel.red <= 0 << 8 || pixel.red >= 255 << 8)
				r_direction *= -1;
			if (pixel.green <= 2 << 8 || pixel.green >= 253 << 8)
				g_direction *= -1;
			if (pixel.blue <= 4 << 8 || pixel.blue >= 251 << 8)
				b_direction *= -1;

			pixel.red += r_direction * (1 << 8);
			pixel.green += g_direction * (3 << 8);
			pixel.blue += b_direction * (5 << 8);

			/* Fill the colour. */
			pixman_image_fill_rectangles(PIXMAN_OP_OVER, fb_pixman,
			                             &pixel, 1, &rect);
		}

		if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_end) != 0) {
			fprintf(stderr, "Failed to get time: %s\n",
			        strerror(errno));
			return 1;
		}

		/* Calculate results. */
		total_time =
			((uint64_t) tp_end.tv_sec * 1000000000 + tp_end.tv_nsec) -
			((uint64_t) tp_start.tv_sec * 1000000000 + tp_start.tv_nsec);

		break;
	}
	default:
		return 1;
	}

	/* Output results informally. */
	fprintf(stderr,
	        " - Wrote out %u frames in %.5fs. Avg: %.5fs.\n",
	        n_repeats, total_time / 1000000000.0,
	        (total_time / n_repeats) / 1000000000.0);

	/* Output results formally. */
	print_csv_results_line("bg", hardware, n_repeats, total_time, git_id,
	                       configuration);

	return 0;
}

/**
 * Data structure exported by the GIMP when converting images to C files. This
 * is copied here from one such C file because it’s not available in a header.
 */
struct gimp_image {
	unsigned int width;
	unsigned int height;
	unsigned int bytes_per_pixel; /* 3:RGB, 4:RGBA */ 
	unsigned char pixel_data[200 * 251 * 4 + 1];
};

#define DIVIDE_ROUND(N, D) (((N) + (D) - 1) / (D))
#define ROUND_UP(N, D) (DIVIDE_ROUND(N, D) * (D))

/* Returns the total time for n_repeats, or -1 on error. */
static int64_t
animate_image_compositor(unsigned int n_repeats, const struct gimp_image *image,
                         unsigned int opaque_x, unsigned int opaque_y,
                         unsigned int opaque_width, unsigned int opaque_height)
{
	struct chericap *background_cfb_id, *background_cfb_token; /* FIXME: still somehow not aligned properly */
	struct chericap *image_cfb_id, *image_cfb_token; /* FIXME: still somehow not aligned properly */
	int image_x, image_y, image_dx, image_dy;
	unsigned int i;
	struct timespec tp_start, tp_end;

	/* Allocate a background. */
	if (malloc_cfb_id_and_token(&background_cfb_id,
	                            &background_cfb_token) != 0) {
		fprintf(stderr, "Error allocating background CFB ID/token: %s\n",
		        strerror(errno));
		return -1;
	}

	/* Allocate the background CFB. */
	if (compositor_allocate_cfb_with_token(&cheri_compositor, screen_width,
	                                       screen_height,
	                                       background_cfb_id,
	                                       background_cfb_token) != 0) {
		fprintf(stderr, "Error allocating background CFB: %s\n",
		        strerror(errno));
		return -1;
	}

	/* Write out the tiles. */
	compositor_write_cfb_data_chequered(background_cfb_id, screen_width,
	                                    screen_height);

	/* Mark the CFB as finished updating. */
	if (compositor_update_cfb(&cheri_compositor,
	                          background_cfb_token, 0, 0, 0, 0, 0,
	                          screen_width, screen_height, 0) != 0) {
		fprintf(stderr, "Error updating background CFB: %s\n",
		        strerror(errno));
		return -1;
	}


	/* Allocate an image. */
	if (malloc_cfb_id_and_token(&image_cfb_id, &image_cfb_token) != 0) {
		fprintf(stderr, "Error allocating image CFB ID/token: %s\n",
		        strerror(errno));
		return -1;
	}

	/* Allocate the CFB. */
	if (compositor_allocate_cfb_with_token(&cheri_compositor,
	                                       ROUND_UP(image->width, TILE_SIZE),
	                                       ROUND_UP(image->height, TILE_SIZE),
	                                       image_cfb_id,
	                                       image_cfb_token) != 0) {
		fprintf(stderr, "Error allocating image CFB: %s\n",
		        strerror(errno));
		return -1;
	}

	/* Write out the tiles. */
	compositor_write_cfb_data_image(image_cfb_id,
	                                image->width, image->height,
	                                image->width, image->height,
	                                (const uint32_t *) ((const void *) image->pixel_data));


	/* Animate forever. */
	image_x = 0; /* start in the top-left corner */
	image_y = 0;
	image_dx = 1; /* start off by moving south-east */
	image_dy = 1;

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_start) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return -1;
	}

	for (i = 0; i < n_repeats; i++) {
		if (compositor_update_cfb(&cheri_compositor,
		                          image_cfb_token, image_x,
		                          image_y, 1, opaque_x, opaque_y,
		                          opaque_width, opaque_height,
		                          0) != 0) {
			fprintf(stderr, "Error updating image CFB: %s\n",
			        strerror(errno));
			return -1;
		}

		image_x += image_dx;
		image_y += image_dy;

		if (image_y + (int) image->height >= (int) screen_height ||
		    image_y <= 0) {
			/* Bounce off the top or bottom edge of the screen. */
			image_dy = -image_dy;
		}
		if (image_x + (int) image->width >= (int) screen_width ||
		    image_x <= 0) {
			/* Bounce off the left or right edge of the screen. */
			image_dx = -image_dx;
		}
	}

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_end) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return -1;
	}

	/* Tidy up. */
	compositor_free_cfb(&cheri_compositor, image_cfb_id);
	compositor_free_cfb(&cheri_compositor, background_cfb_id);
	free(image_cfb_id);
	free(image_cfb_token);
	free(background_cfb_id);
	free(background_cfb_token);

	/* Calculate results. */
	return
		((uint64_t) tp_end.tv_sec * 1000000000 + tp_end.tv_nsec) -
		((uint64_t) tp_start.tv_sec * 1000000000 + tp_start.tv_nsec);
}

static void
fb_repaint_region(pixman_image_t *image_pixman,
                  pixman_region32_t *image_bounding_box,
                  pixman_image_t *output_pixman,
                  pixman_region32_t *region, pixman_region32_t *surf_region,
                  pixman_op_t pixman_op)
{
	pixman_region32_t final_region;
	pixman_box32_t *rects;
	int n_rects, i, src_x, src_y;
	pixman_box32_t *extents;

	/* The final region to be painted is the intersection of
	 * 'region' and 'surf_region'. However, 'region' is in global
	 * coordinates, and 'surf_region' is in surface-local coordinates. */
	pixman_region32_init(&final_region);
	pixman_region32_copy(&final_region, surf_region);

	extents = pixman_region32_extents(image_bounding_box);
	pixman_region32_translate(&final_region, extents->x1, extents->y1);

	/* That's what we need to paint */
	pixman_region32_intersect(&final_region, &final_region, region);

	rects = pixman_region32_rectangles(&final_region, &n_rects);

	for (i = 0; i < n_rects; i++) {
		src_x = rects[i].x1 - extents->x1;
		src_y = rects[i].y1 - extents->y1;
		pixman_image_composite32(pixman_op,
			image_pixman, /* src */
			NULL /* mask */,
			output_pixman, /* dest */
			src_x, src_y, /* src_x, src_y */
			0, 0, /* mask_x, mask_y */
			rects[i].x1, rects[i].y1, /* dest_x, dest_y */
			rects[i].x2 - rects[i].x1, /* width */
			rects[i].y2 - rects[i].y1 /* height */);
	}

	pixman_region32_fini(&final_region);
}

static void
fb_draw_surface(pixman_image_t *image_pixman,
                pixman_region32_t *image_bounding_box, /* in global coords */
                pixman_region32_t *image_opaque, /* in local coordinates */
                pixman_region32_t *image_blend, /* in local coordinates */
                pixman_image_t *output_pixman,
                pixman_region32_t *damage) /* in global coordinates */
{
	/* repaint bounding region in global coordinates: */
	pixman_region32_t repaint;

	pixman_region32_init(&repaint);
	pixman_region32_intersect(&repaint, image_bounding_box, damage);

	if (!pixman_region32_not_empty(&repaint))
		goto out;

	if (pixman_region32_not_empty(image_opaque)) {
		fb_repaint_region(image_pixman, image_bounding_box,
		                  output_pixman, &repaint, image_opaque,
		                  PIXMAN_OP_SRC);
	}

	if (pixman_region32_not_empty(image_blend)) {
		fb_repaint_region(image_pixman, image_bounding_box,
		                  output_pixman, &repaint, image_blend,
		                  PIXMAN_OP_OVER);
	}

out:
	pixman_region32_fini(&repaint);
}

/* Returns the total time for n_repeats, or -1 on error. */
static int64_t
animate_image_fb(unsigned int n_repeats, const struct gimp_image *image,
                 pixman_format_code_t image_format,
                 unsigned int opaque_x, unsigned int opaque_y,
                 unsigned int opaque_width, unsigned int opaque_height)
{
	pixman_image_t *background_pixman, *image_pixman;
	pixman_region32_t damage, image_bounding_box, image_opaque, image_blend;
	pixman_region32_t screen_bounding_box, screen_blend;
	int old_image_x, old_image_y, image_x, image_y, image_dx, image_dy;
	unsigned int i;
	struct timespec tp_start, tp_end;
	void *shadow_buf;
	pixman_image_t *shadow_pixman;

	/* aaaargh. */
	union {
		uint32_t *what_pixman_wants;
		const unsigned char *what_gimp_provides;
	} why_oh_why_cant_people_use_const_correctly;

	/* Create the background image. */
	why_oh_why_cant_people_use_const_correctly.what_gimp_provides = chequerboard_image.pixel_data;
	background_pixman = pixman_image_create_bits(PIXMAN_r8g8b8,
	                                             chequerboard_image.width,
	                                             chequerboard_image.height,
	                                             why_oh_why_cant_people_use_const_correctly.what_pixman_wants,
	                                             chequerboard_image.width * chequerboard_image.bytes_per_pixel);
	pixman_image_set_repeat(background_pixman, PIXMAN_REPEAT_NORMAL);

	/* Create the moving image. */
	why_oh_why_cant_people_use_const_correctly.what_gimp_provides = image->pixel_data;
	image_pixman = pixman_image_create_bits(image_format, image->width,
	                                        image->height,
	                                        why_oh_why_cant_people_use_const_correctly.what_pixman_wants,
	                                        image->width * image->bytes_per_pixel);

	/* Create the shadow buffer. */
	shadow_buf = malloc(screen_width * screen_height *  3);
	shadow_pixman = pixman_image_create_bits(PIXMAN_r8g8b8, screen_width,
	                                         screen_height, shadow_buf,
	                                         screen_width * 3);

	/* Set up some regions, all in local coordinates. */
	pixman_region32_init_rect(&image_bounding_box, 0, 0,
	                          image->width, image->height);
	pixman_region32_init_rect(&image_opaque, opaque_x, opaque_y,
	                          opaque_width, opaque_height);
	/* Blended region is whole surface minus opaque region. */
	pixman_region32_init(&image_blend);
	pixman_region32_subtract(&image_blend, &image_bounding_box,
	                         &image_opaque);

	pixman_region32_init_rect(&screen_bounding_box, 0, 0, screen_width,
	                          screen_height);
	pixman_region32_init_rect(&screen_blend, 0, 0, 0, 0);

	/* Animate forever. */
	old_image_x = image_x = 0; /* start in the top-left corner */
	old_image_y = image_y = 0;
	image_dx = 1; /* start off by moving south-east */
	image_dy = 1;

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_start) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return -1;
	}

	for (i = 0; i < n_repeats; i++) {
		pixman_box32_t *rects;
		unsigned int j, n_rects;

		/* Calculate the damage region. This is initially the whole
		 * screen. */
		pixman_region32_init(&damage);

		if (i == 0) {
			pixman_region32_union_rect(&damage, &damage, 0, 0,
			                           screen_width, screen_height);
		} else {
			pixman_region32_union_rect (&damage, &damage,
			                            image_x, image_y,
			                            image->width,
			                            image->height);
			pixman_region32_union_rect (&damage, &damage,
			                            old_image_x, old_image_y,
			                            image->width,
			                            image->height);
		}

		/* Composite the background. */
		fb_draw_surface(background_pixman, &screen_bounding_box,
		                &screen_bounding_box, &screen_blend,
		                shadow_pixman, &damage);

		/* Composite the image. */
		fb_draw_surface(image_pixman, &image_bounding_box,
		                &image_opaque, &image_blend, shadow_pixman,
		                &damage);

		/* Blit the updated back buffer to the hardware. */
		rects = pixman_region32_rectangles(&damage, &n_rects);

		for (j = 0; j < n_rects; j++) {
			pixman_image_composite32(PIXMAN_OP_SRC,
				shadow_pixman, /* src */
				NULL /* mask */,
				fb_pixman, /* dest */
				rects[j].x1, rects[j].y1, /* src_x, src_y */
				0, 0, /* mask_x, mask_y */
				rects[j].x1, rects[j].y1, /* dest_x, dest_y */
				rects[j].x2 - rects[j].x1, /* width */
				rects[j].y2 - rects[j].y1 /* height */);
		}

		/* Clear the damage region. */
		pixman_region32_fini(&damage);

		/* Compute the image's new position. */
		old_image_x = image_x;
		old_image_y = image_y;
		image_x += image_dx;
		image_y += image_dy;

		pixman_region32_translate(&image_bounding_box, image_dx,
		                          image_dy);

		if (image_y + (int) image->height >= (int) screen_height ||
		    image_y <= 0) {
			/* Bounce off the top or bottom edge of the screen. */
			image_dy = -image_dy;
		}
		if (image_x + (int) image->width >= (int) screen_width ||
		    image_x <= 0) {
			/* Bounce off the left or right edge of the screen. */
			image_dx = -image_dx;
		}
	}

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_end) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return -1;
	}

	/* Tidy up. */
	pixman_region32_fini(&screen_blend);
	pixman_region32_fini(&screen_bounding_box);
	pixman_region32_fini(&image_blend);
	pixman_region32_fini(&image_opaque);
	pixman_region32_fini(&image_bounding_box);

	pixman_image_unref(shadow_pixman);
	free(shadow_buf);
	pixman_image_unref(image_pixman);
	pixman_image_unref(background_pixman);

	/* Calculate results. */
	return
		((uint64_t) tp_end.tv_sec * 1000000000 + tp_end.tv_nsec) -
		((uint64_t) tp_start.tv_sec * 1000000000 + tp_start.tv_nsec);
}

/* ‘opaque_cfb’ test. See the top of the file for a description. */
static int
test_opaque_cfb(hardware_t hardware, unsigned int n_repeats,
                const char *git_id, const char *configuration)
{
	uint64_t total_time; /* nanoseconds */

	switch(hardware) {
	case HARDWARE_COMPOSITOR: {
		total_time = animate_image_compositor(n_repeats,
		                                      (const struct gimp_image *) &opaque_square_image,
		                                      0, 0,
		                                      opaque_square_image.width,
		                                      opaque_square_image.height);

		break;
	}
	case HARDWARE_FB: {
		total_time = animate_image_fb(n_repeats,
		                              (const struct gimp_image *) &opaque_square_image,
		                              PIXMAN_r8g8b8x8, 0, 0,
		                              opaque_square_image.width,
		                              opaque_square_image.height);

		break;
	}
	default:
		return 1;
	}

	/* Output results informally. */
	fprintf(stderr,
	        " - Moved image %u times in %.5fs. Avg: %.5fs.\n",
	        n_repeats, total_time / 1000000000.0,
	        (total_time / n_repeats) / 1000000000.0);

	/* Output results formally. */
	print_csv_results_line("opaque_cfb", hardware, n_repeats, total_time,
	                       git_id, configuration);

	return 0;
}

/* ‘translucent_cfb’ test. See the top of the file for a description. */
static int
test_translucent_cfb(hardware_t hardware, unsigned int n_repeats,
                     const char *git_id, const char *configuration)
{
	uint64_t total_time; /* nanoseconds */

	switch(hardware) {
	case HARDWARE_COMPOSITOR: {
		total_time = animate_image_compositor(n_repeats,
		                                      (const struct gimp_image *) &translucent_square_image,
		                                      0, 0, 0, 0);

		break;
	}
	case HARDWARE_FB: {
		total_time = animate_image_fb(n_repeats,
		                              (const struct gimp_image *) &translucent_square_image,
		                              PIXMAN_r8g8b8a8, 0, 0, 0, 0);

		break;
	}
	default:
		return 1;
	}

	/* Output results informally. */
	fprintf(stderr,
	        " - Moved image %u times in %.5fs. Avg: %.5fs.\n",
	        n_repeats, total_time / 1000000000.0,
	        (total_time / n_repeats) / 1000000000.0);

	/* Output results formally. */
	print_csv_results_line("translucent_cfb", hardware, n_repeats,
	                       total_time, git_id, configuration);

	return 0;
}

/* ‘binary_cfb’ test. See the top of the file for a description. */
static int
test_binary_cfb(hardware_t hardware, unsigned int n_repeats, const char *git_id,
                const char *configuration)
{
	uint64_t total_time; /* nanoseconds */

	switch(hardware) {
	case HARDWARE_COMPOSITOR: {
		total_time = animate_image_compositor(n_repeats,
		                                      (const struct gimp_image *) &crest_image,
		                                      0, 0, crest_image.width, 150);

		break;
	}
	case HARDWARE_FB: {
		total_time = animate_image_fb(n_repeats,
		                              (const struct gimp_image *) &crest_image,
		                              PIXMAN_r8g8b8a8, 0, 0, crest_image.width, 150);

		break;
	}
	default:
		return 1;
	}

	/* Output results informally. */
	fprintf(stderr,
	        " - Moved image %u times in %.5fs. Avg: %.5fs.\n",
	        n_repeats, total_time / 1000000000.0,
	        (total_time / n_repeats) / 1000000000.0);

	/* Output results formally. */
	print_csv_results_line("binary_cfb", hardware, n_repeats, total_time,
	                       git_id, configuration);

	return 0;
}

/* Returns a main()-style error number. */
static int
map_hardware(hardware_t hardware)
{
	switch(hardware) {
	case HARDWARE_COMPOSITOR: {
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

		if (compositor_set_configuration(&cheri_compositor, 800, 480) != 0) {
			fprintf(stderr, "Error setting resolution: %s\n",
			        strerror(errno));
			return 1;
		}

		return 0;
	}
	case HARDWARE_FB: {
		int fd = -1, control_fd = -1;
		void *_control; /* work around cast warnings */
		volatile uint32_t *control;

		/* Open the frame buffer device. */
		fd = open(FB_DEV, O_RDWR);
		if (fd < 0) {
			fprintf(stderr,
			        "Failed to open frame buffer device ‘%s’: %s\n",
			        FB_DEV, strerror(errno));
			return 1;
		}

		/* Map the frame buffer. */
		fb_pixels_length = 800 * 480 * 4; /* bytes */
		fb_pixels = mmap(NULL, fb_pixels_length,
		                 PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

		close(fd);

		if (fb_pixels == MAP_FAILED) {
			fprintf(stderr,
			        "Failed to mmap frame buffer: %s\n",
			        strerror(errno));
			return 1;
		}

		/* Open the control interface and enable pixel output. */
		control_fd = open(FB_CONTROL_DEV, O_RDWR | O_NONBLOCK);
		_control = mmap(NULL, 0x20, PROT_READ | PROT_WRITE, MAP_SHARED,
		                control_fd, 0);
		control = _control;
		close(control_fd);

		if (control == MAP_FAILED) {
			fprintf(stderr, "Failed to mmap control registers: %s\n",
			        strerror(errno));
			munmap(fb_pixels, fb_pixels_length);
			return 1;
		}

		control[0] = (0xff << 8) | (control[0] & 0xff);

		munmap(_control, 0x20);

		/* Create a pixman image to wrap the frame buffer. */
		fb_pixman = pixman_image_create_bits(PIXMAN_b8g8r8x8, 800, 480,
		                                     fb_pixels, 800 * 4);

		return 0;
	}
	default:
		return 1;
	}
}

/* Returns a main()-style error number. */
static int
unmap_hardware(hardware_t hardware)
{
	switch(hardware) {
	case HARDWARE_COMPOSITOR:
		compositor_unmap(&cheri_compositor);
		return 0;
	case HARDWARE_FB: {
		pixman_image_unref(fb_pixman);

		munmap(fb_pixels, fb_pixels_length);
		fb_pixels = NULL;
		fb_pixels_length = 0;

		return 0;
	}
	default:
		return 1;
	}
}

/* Returns a main()-style error number. */
static int
usage(void)
{
	printf("rendering-speed compositor|fb [n_repeats] \n\n");
	printf("This runs a series of performance tests against the given "
	       "hardware, outputting a CSV file of results to standard "
	       "output.\n\n");
	printf("The mandatory first parameter specifies whether the CHERI "
	       "compositor, or the standard frame buffer, should be tested.\n");
	printf("The optional second parameter specifies the number of repeats "
	       "to perform for each test.\n");

	return 1;
}

int
main(int argc, char *argv[])
{
	int retval = 0;
	hardware_t hardware;
	int n_repeats;
	const char *git_id, *configuration;

	if (argc < 4) {
		return usage();
	}

	/* Parse git_id and configuration. */
	git_id = argv[1];
	configuration = argv[2];

	/* Parse hardware specifier input. */
	if (strcmp(argv[3], "compositor") == 0) {
		hardware = HARDWARE_COMPOSITOR;
	} else if (strcmp(argv[3], "fb") == 0) {
		hardware = HARDWARE_FB;
	} else {
		return usage();
	}

	/* Parse the number of repeats (or leave it at the default: -1). */
	if (argc == 5) {
		char *endptr;

		n_repeats = strtol(argv[4], &endptr, 0);
		if (*endptr != '\0' || n_repeats < 1) {
			fprintf(stderr, "Invalid repeat count ‘%s’.\n",
			        argv[4]);
			return usage();
		}
	} else {
		n_repeats = -1;
	}

	/* Initialise the hardware. */
	retval = map_hardware(hardware);
	if (retval != 0)
		return retval;

	/* Output the CSV header. */
	print_csv_header(argc, (const char * const *) argv);

	/* Run the tests. */
	fprintf(stderr, "Running ‘bg’ test…\n");
	retval = test_bg(hardware, (n_repeats != -1) ? n_repeats : 1000, git_id,
	                 configuration);
	if (retval != 0) goto done;

	fprintf(stderr, "Running ‘opaque_cfb’ test…\n");
	retval = test_opaque_cfb(hardware,
	                         (n_repeats != -1) ? n_repeats : 10000, git_id,
	                         configuration);
	if (retval != 0) goto done;

	fprintf(stderr, "Running ‘translucent_cfb’ test…\n");
	retval = test_translucent_cfb(hardware,
	                              (n_repeats != -1) ? n_repeats : 10000,
	                              git_id, configuration);
	if (retval != 0) goto done;

	fprintf(stderr, "Running ‘binary_cfb’ test…\n");
	retval = test_binary_cfb(hardware,
	                         (n_repeats != -1) ? n_repeats : 10000, git_id,
	                         configuration);
	if (retval != 0) goto done;

done:
	unmap_hardware(hardware);

	return retval;
}
