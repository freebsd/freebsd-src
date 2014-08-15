/*-
 * Copyright (c) 2012 Simon W. Moore
 * Copyright (c) 2012 SRI International
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

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <terasic_mtl.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <libutil.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define TSG_NONE	0x00
#define	TSG_NORTH	0x10
#define	TSG_NORTHEAST	0x12
#define	TSG_EAST	0x14
#define	TSG_SOUTHEAST	0x16
#define	TSG_SOUTH	0x18
#define	TSG_SOUTHWEST	0x1A
#define	TSG_WEST	0x1C
#define	TSG_NORTHWEST	0x1E
#define	TSG_ROTATE_CW	0x28	/* Clockwise */
#define	TSG_ROTATE_CCW	0x29	/* Counter Clockwise */
#define	TSG_CLICK	0x20
#define TSG_DCLICK	0x22	/* Double Click */
#define TSG2_NORTH	0x30
#define TSG2_NORTHEAST	0x32
#define TSG2_EAST	0x34
#define TSG2_SOUTHEAST	0x36
#define TSG2_SOUTH	0x38
#define TSG2_SOUTHWEST	0x3A
#define TSG2_WEST	0x3C
#define	TSG2_NORTHWEST	0x3E
#define TSG2_CLICK	0x40
#define TSG2_ZOOM_IN	0x48
#define TSG2_ZOOM_OUT	0x49

static void
usage(void)
{
	
	printf("usage:	\n");
	printf("	mtlctl fbdump <file>\n");
	printf("	mtlctl fbdumppng <file>\n");
	printf("	mtlctl fbfill <RRGGBB>\n");
	printf("	mtlctl fbloaddump <file>\n");
	printf("	mtlctl fbloadpng <file>\n");
	printf("	mtlctl gesture\n");
	printf("	mtlctl gestures\n");
	exit(1);
}

static const char *
gesturetostr(int gesture)
{

	switch (gesture) {
	case TSG_NONE:
		return "none";
	case TSG_NORTH:
		return "north";
	case TSG_NORTHEAST:
		return "northeast";
	case TSG_EAST:
		return "east";
	case TSG_SOUTHEAST:
		return "southeast";
	case TSG_SOUTH:
		return "south";
	case TSG_SOUTHWEST:
		return "southwest";
	case TSG_WEST:
		return "west";
	case TSG_NORTHWEST:
		return "northwest";
	case TSG_ROTATE_CW:
		return "rotate clockwise";
	case TSG_ROTATE_CCW:
		return "rotate counter-clockwise";
	case TSG_CLICK:
		return "click";
	case TSG_DCLICK:
		return "double click";
	case TSG2_NORTH:
		return "two finger north";
	case TSG2_NORTHEAST:
		return "two finger northeast";
	case TSG2_EAST:
		return "two finger east";
	case TSG2_SOUTHEAST:
		return "two finger southeast";
	case TSG2_SOUTH:
		return "two finger south";
	case TSG2_SOUTHWEST:
		return "two finger southwest";
	case TSG2_WEST:
		return "two finger west";
	case TSG2_NORTHWEST:
		return "two finger northwest";
	case TSG2_CLICK:
		return "two finger click";
	case TSG2_ZOOM_IN:
		return "zoom-in";
	case TSG2_ZOOM_OUT:
		return "zoom-out";
	default:
		return "unknown";
	}
}

static void
print_gesture(void)
{
	static struct tsstate *sp;

	sp = ts_poll(0);
	printf("gesture %s (%02x) ", gesturetostr(sp->ts_gesture),
	    sp->ts_gesture);
	if (sp->ts_x1 >= 0 && sp->ts_y1 >= 0) {
		printf("at (%d, %d) ", sp->ts_x1, sp->ts_y1);
	}
	if (sp->ts_count == 2) {
		printf("and (%d, %d) ", sp->ts_x2, sp->ts_y2);
	}
	printf("count %d\n", sp->ts_count);
}

int
main(int argc, char *argv[] __unused)
{
	int b, fd, i, j;
	u_int32_t *image, pixel;

	if (argc < 2)
		usage();

	fb_init();

	image = malloc(fb_width * fb_height * 4);
	if (image == NULL)
		err(1, "malloc");

	if (strcmp(argv[1], "fbdump") == 0) {
		if (argc != 3)
			usage();
		if ((fd = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC)) == -1)
			err(1, "open");
		fb_save(image);
		for (i = 0; i < fb_width * fb_height * 4; i += b) {
			b = write(fd, image, fb_width * fb_height * 4 - i);
			if (b == -1)
				err(1, "write");
		}
	} else if (strcmp(argv[1], "fbdumppng") == 0) {
		FILE * fp;
		png_structp png_ptr;
		png_infop info_ptr;
		png_byte **row_pointers;
		int pixel_size = 3;
		int depth = 8;

		if (argc != 3)
			usage();
		if ((fd = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC)) == -1)
			err(1, "open");
		if ((fp = fdopen(fd, "w")) == NULL)
			err(1, "fdopen");

		png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		    NULL, NULL, NULL);
		if (png_ptr == NULL)
			errx(1, "png_create_write_struct");
		info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == NULL)
			errx(1, "png_create_info_struct");
		if (setjmp(png_jmpbuf(png_ptr)) != 0)
			errx(1, "png error");
		png_set_IHDR (png_ptr, info_ptr, fb_width, fb_height, depth,
		    PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		    PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		row_pointers = png_malloc(png_ptr,
		    fb_height * sizeof(png_byte *));

		fb_save(image);
		for (j = 0; j < fb_height; j++) {
			row_pointers[j] = png_malloc(png_ptr,
			    sizeof(uint8_t) * fb_width * pixel_size);
			for (i = 0; i < fb_width; i++) {
				pixel = image[i + j * fb_width];
				row_pointers[j][i * 3] = (pixel >> 8) & 0xFF;
				row_pointers[j][i * 3 + 1] = (pixel >> 16) &
				    0xFF;
				row_pointers[j][i * 3 + 2] = (pixel >> 24) &
				    0xFF;
			}
		}

		png_init_io (png_ptr, fp);
		png_set_rows (png_ptr, info_ptr, row_pointers);
		png_write_png (png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
		for (j = 0; j < fb_height; j++)
			png_free (png_ptr, row_pointers[j]);
		png_free (png_ptr, row_pointers);
		fclose(fp);
	} else if (strcmp(argv[1], "fbfill") == 0) {
		char *endptr;
		long color;

		if (argc != 3)
			usage();
		color = strtol(argv[2], &endptr, 16);
		if (*endptr != '\0' || color < 0 || color > 0xFFFFFF) {
			warnx("invalid color %s", argv[2]);
			usage();
		}
		pixel = fb_colour((color >> 16) & 0xFF, (color >> 8) & 0xFF,
		    color & 0xFF);
		fb_fill(pixel);
		/* XXX: A bit too heavyhanded */
		fb_blend(0,0,255,0);
	} else if (strcmp(argv[1], "fbloaddump") == 0) {
		if (argc != 3)
			usage();
		if ((fd = open(argv[2], O_RDONLY)) == -1)
			err(1, "open");
		for (i = 0; i < fb_width * fb_height * 4; i += b) {
			b = read(fd, image, fb_width * fb_height * 4 - i);
			if (b == -1)
				err(1, "read");
		}
		fb_post(image);
		/* XXX: A bit too heavyhanded */
		fb_blend(0,0,255,0);
		close(fd);
	} else if (strcmp(argv[1], "fbloadpng") == 0) {
		if (argc != 3)
			usage();
		if ((fd = open(argv[2], O_RDONLY)) == -1)
			err(1, "open");
		if (read_png_fd(fd, image, fb_width, fb_height) == -1)
			errx(1, "read_png_fd");
		fb_post(image);
		/* XXX: A bit too heavyhanded */
		fb_blend(0,0,255,0);
		close(fd);
	} else if (strcmp(argv[1], "gesture") == 0) {
		print_gesture();
	} else if (strcmp(argv[1], "gestures") == 0) {
		for (;;)
			print_gesture();
	} else
		usage();

	free(image);

	return (0);
}
