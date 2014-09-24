/*-
 * Copyright (c) 2012,2014 SRI International
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

#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <png.h>

static void read_png_from_fd(png_structp png_ptr, png_bytep data,
    png_size_t length);

static int
decode_png(int pfd)
{
	png_uint_32 r, width, height;
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_info;
	png_bytep *rows;
	png_bytep buffer;
	png_size_t rowbytes;

	if ((png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
	    NULL, NULL, NULL)) == NULL) {
		close(pfd);
		return(-1);
	}
	if ((info_ptr = png_create_info_struct(png_ptr)) == NULL) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		close(pfd);
		return(-1);
	}
	if ((end_info = png_create_info_struct(png_ptr)) == NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		close(pfd);
		return(-1);
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		close(pfd);
		return(-1);
	}

	/*
	 * Reject the image if the parser finds a different size than
	 * our manual parsing did.
	 */

	png_set_read_fn(png_ptr, &pfd, read_png_from_fd);

	png_read_info(png_ptr, info_ptr);

	height = png_get_image_height(png_ptr, info_ptr);
	width = png_get_image_width(png_ptr, info_ptr);
	rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	printf("width = %d, height = %d\n", width, height);

	printf("bit_depth = %d, color_type = %d\n",
	    png_get_bit_depth(png_ptr, info_ptr), 
	    png_get_color_type(png_ptr, info_ptr));

	png_set_gray_to_rgb(png_ptr);
	/*png_set_bgr(png_ptr);*/
	png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
	png_read_update_info(png_ptr, info_ptr);

	buffer = calloc(height, rowbytes);
	if (buffer == NULL)
		png_error(png_ptr, "can't allocate buffer");
	if ((rows = malloc(height*sizeof(png_bytep))) == NULL)
		png_error(png_ptr, "failed to malloc row array");
	for (r = 0; r < height; r++)
		rows[r] = buffer + rowbytes * r;

	png_read_image(png_ptr, rows);

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	free(rows);
	free(buffer);

	return (0);
}

static void
usage(void)
{
	errx(1, "usage: png_sb_test <file>");
}

static void
read_png_from_fd(png_structp png_ptr, png_bytep data, png_size_t length)
{
	int	pfd;
	ssize_t rlen;

#if 0
	printf("%s: data 0x%p, length 0x%zu\n", __func__, data, length);
#endif

	pfd = *(int *)png_get_io_ptr(png_ptr);
#if 0
	printf("%s: pfd %d\n", __func__, pfd);
#endif
	rlen = read(pfd, data, length);
	if (rlen < 0 || (png_size_t)rlen != length) {
		printf("%s: wanted %zu but got %zd\n", __func__, length, rlen);
		png_error(png_ptr, "read error");
	}
#if 0
	printf("%s: done\n", __func__);
#endif
}

int
main(int argc, char **argv)
{	
	int pfd;

	if (argc != 2)
		usage();

	if ((pfd = open(argv[1], O_RDONLY)) < -1)
		err(1, "open(%s)", argv[1]);

	if (decode_png(pfd) == -1) {
		printf("FAIL: decode_png\n");
		return (1);
	} else
		printf("PASS: decode_png\n");

	return(0);
}
