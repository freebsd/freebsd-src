/*-
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

#include <machine/sysarch.h>

#include <png.h>
#include <stdlib.h>
#include <unistd.h>

#include "imagebox.h"
#include "iboxpriv.h"

extern int png_exec_triggered;

static void read_row_callback(png_structp, png_uint_32, int);
static void read_png_from_fd(png_structp, png_bytep, png_size_t);

void
decode_png(struct ibox_decode_state *ids,
    png_rw_ptr user_read_fn, png_read_status_ptr read_row_fn)
{
	int bit_depth, color_type, interlace_type;
	png_uint_32 r, width, height;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_infop end_info = NULL;
	png_bytep *rows = NULL;

	png_exec_triggered = 0;

	ids->is->times[1] = sysarch(MIPS_GET_COUNT, 0);

	if ((png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
	    NULL, NULL, NULL)) == NULL) {
		ids->is->error = 1;
		goto error;
	}
	if ((info_ptr = png_create_info_struct(png_ptr)) == NULL) {
		ids->is->error = 1;
		goto error;
	}
	if ((end_info = png_create_info_struct(png_ptr)) == NULL) {
		ids->is->error = 1;
		goto error;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		ids->is->error = 1;
		goto error;
	}

	if (read_row_fn != NULL)
		png_set_read_status_fn(png_ptr, read_row_fn);
	else
		png_set_read_status_fn(png_ptr, read_row_callback);

	/*
	 * Reject the image if the parser finds a different size than
	 * our manual parsing did.
	 */
#if 0
	png_set_user_limits(png_ptr, width, height);
#endif

	if (user_read_fn != NULL)
		png_set_read_fn(png_ptr, ids, user_read_fn);
	else
		png_set_read_fn(png_ptr, ids, read_png_from_fd);

	png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
	    &color_type, &interlace_type, NULL, NULL);

	if (width != ids->is->width || height != ids->is->height) {
		ids->is->error = 1;
		goto error;
	}

	png_set_gray_to_rgb(png_ptr);
	png_set_bgr(png_ptr);
	png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
	ids->is->passes_remaining = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	if ((rows = malloc(height*sizeof(png_bytep))) == NULL)
		png_error(png_ptr, "failed to malloc row array");
	for (r = 0; r < height; r++)
		rows[r] = (png_bytep)(ids->buffer + (width * r));

	png_read_rows(png_ptr, rows, NULL, height);

	png_read_end(png_ptr, end_info);

	if (png_exec_triggered) {
		if (ids->is->sb == SB_CAPSICUM)
			ids->is->error = 99;
		for (r = 0; r < ids->is->width * ids->is->height; r++)
			ids->buffer[r] = 0x0000FF00;
	}

error:
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	close(ids->fd);
	ids->is->times[2] = sysarch(MIPS_GET_COUNT, 0);
	free(rows);
}

static void
read_row_callback(png_structp png_ptr, png_uint_32 row, int pass __unused)
{
	struct ibox_decode_state *ids;

	ids = png_get_io_ptr(png_ptr);
	if (ids->is->valid_rows < row)
		ids->is->valid_rows = row;
	if (row == ids->is->height)
		ids->is->passes_remaining--;
}

static void
read_png_from_fd(png_structp png_ptr, png_bytep data, png_size_t length)
{
	struct ibox_decode_state *ids;
	ssize_t rlen;

	ids = png_get_io_ptr(png_ptr);
	rlen = read(ids->fd, data, length);
	if (rlen < 0 || (png_size_t)rlen != length)
		png_error(png_ptr, "read error");
}
