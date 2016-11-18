/*-
 * Copyright (c) 2014-2016 Robert N. M. Watson
 * Copyright (c) 2014 SRI International
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
#include <sys/stat.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cheri_enter.h>
#include <cheri/cheri_fd.h>
#include <cheri/cheri_invoke.h>
#include <cheri/cheri_system.h>

#include <png.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libpng_sb-helper.h"

register_t invoke(struct cheri_object co __unused, register_t v0 __unused,
    register_t methodnum,
    register_t a0, register_t a1,
    struct cheri_object system_object,
    __capability void *c5 __unused, __capability void *c6 __unused,
    __capability void *c7, __capability void *c8)
    __attribute__((cheri_ccall)); /* XXXRW: Will be ccheri_ccaller. */

static png_structp g_png_ptr;

static void
sb_warning_fn(png_structp png_ptr __unused, png_const_charp warning_message)
{

	printf("png sandbox warning: %s", warning_message);
}

static void
sb_error_fn(png_structp png_ptr __unused, png_const_charp error_message)
{

	printf("png sandbox warning: %s", error_message);
	/* XXX: push to a shared buffer for caller? */
	abort();	/* let parent pick up the pieces */
}

static void
libpng_sb_read_callback(void *psp, png_bytep data, png_size_t length)
{

	cheri_system_user_call_fn(LIBPNG_SB_USERFN_READ_CALLBACK,
	    length, 0, 0, 0, 0, 0, 0,
	    psp, data, NULL, NULL, NULL);
}

static void
libpng_sb_info_callback(void *psp, png_infop info_ptr)
{

	cheri_system_user_call_fn(LIBPNG_SB_USERFN_INFO_CALLBACK,
	    0, 0, 0, 0, 0, 0, 0,
	    psp, info_ptr, NULL, NULL, NULL);
}

static void
libpng_sb_row_callback(void *psp, png_bytep new_row, png_uint_32 row_num,
    int pass)
{

	cheri_system_user_call_fn(LIBPNG_SB_USERFN_ROW_CALLBACK,
	    row_num, pass, 0, 0, 0, 0, 0,
	    psp, new_row, NULL, NULL, NULL);
}

static void
libpng_sb_end_callback(void *psp, png_infop info_ptr)
{

	cheri_system_user_call_fn(LIBPNG_SB_USERFN_END_CALLBACK,
	    0, 0, 0, 0, 0, 0, 0,
	    psp, info_ptr, NULL, NULL, NULL);
}

static void
sb_read_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
	void *io_ptr = png_get_io_ptr(png_ptr);

#if 0
	printf("in sb_read_fn, data base 0x%jx offset 0x%jx length 0x%zx (min len 0x%zx)\n",
	    cheri_getbase(data), cheri_getoffset(data),
	    cheri_getlen(data), length);
#endif

	libpng_sb_read_callback(io_ptr, cheri_csetbounds(data, length), length);
}

static void
sb_info_fn(png_structp png_ptr, png_infop info_ptr)
{
	void *io_ptr = png_get_io_ptr(png_ptr);

	libpng_sb_info_callback(io_ptr, info_ptr);
}

static void
sb_row_fn(png_structp png_ptr, png_bytep new_row, png_uint_32 row_num, int pass)
{
	void *io_ptr = png_get_io_ptr(png_ptr);

	libpng_sb_row_callback(io_ptr, new_row, row_num, pass);
}

static void
sb_end_fn(png_structp png_ptr, png_infop info_ptr)
{
	void *io_ptr = png_get_io_ptr(png_ptr);

	libpng_sb_end_callback(io_ptr, info_ptr);
}

/*
 * Handle assorted libpng library calls.
 */
register_t
invoke(struct cheri_object c __unused, register_t v0 __unused,
    register_t op, register_t a0, register_t a1,
    struct cheri_object system_object __unused,
    __capability void *c5 __unused, __capability void *c6 __unused,
    __capability void *c7, __capability void *c8)
{
	png_infop info_ptr;

	printf("libpng sandbox invoked with op %ld\n", op);

	if (op < 0 || op > LIBPNG_SB_HELPER_MAX_OP) {
		printf("invoked with invalid op %jd", op);
		return (1);
	}

	/*
	 * Select a method.
	 */
	switch (op) {
	case LIBPNG_SB_HELPER_OP_CREATE_READ_STRUCT:
		g_png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
		    NULL, sb_error_fn, sb_warning_fn);
		if (g_png_ptr == NULL)
			return (1);
		return (0);
	case LIBPNG_SB_HELPER_OP_CREATE_INFO_STRUCT:
		info_ptr = png_create_info_struct(g_png_ptr);
#if 0
		printf("created info structure at 0x%jx\n",
		    cheri_getbase(info_ptr) + cheri_getoffset(info_ptr));
#endif
		if (info_ptr == NULL)
			return (1);
		else {
			*(void * __capability *)c7 = info_ptr;
			return (0);
		}

	case LIBPNG_SB_HELPER_OP_READ_INFO:
#if 0
		printf("png_read_info with info_ptr base 0x%jx offset 0x%jx length 0x%zx\n",
		    (uintmax_t)cheri_getbase(c7), cheri_getoffset(c7),
		    cheri_getlen(c7));
#endif
		png_read_info(g_png_ptr, c7);
		return (0);
	case LIBPNG_SB_HELPER_OP_SET_EXPAND_GRAY_1_2_4_TO_8:
		png_set_expand_gray_1_2_4_to_8(g_png_ptr);
		return (0);
	case LIBPNG_SB_HELPER_OP_SET_PALETTE_TO_RGB:
		png_set_palette_to_rgb(g_png_ptr);
		return (0);
	case LIBPNG_SB_HELPER_OP_SET_TRNS_TO_ALPHA:
		png_set_tRNS_to_alpha(g_png_ptr);
		return (0);
	case LIBPNG_SB_HELPER_OP_SET_FILLER:
		png_set_filler(g_png_ptr, a0, a1);
		return (0);
	case LIBPNG_SB_HELPER_OP_SET_STRIP_16:
		png_set_strip_16(g_png_ptr);
		return (0);
	case LIBPNG_SB_HELPER_OP_READ_UPDATE_INFO:
		png_read_update_info(g_png_ptr, c7);
		return (0);
	case LIBPNG_SB_HELPER_OP_READ_IMAGE:
#if 0
		printf("png_read_image with row_pointer base 0x%jx offset 0x%jx length 0x%zx\n",
		    (uintmax_t)cheri_getbase(c7), cheri_getoffset(c7),
		    cheri_getlen(c7));
#endif
		png_read_image(g_png_ptr, c7);
		return (0);
	case LIBPNG_SB_HELPER_OP_PROCESS_DATA:
		png_process_data(g_png_ptr, c7, c8, a0);
		return (0);
	case LIBPNG_SB_HELPER_OP_GET_COLOR_TYPE:
		return (png_get_color_type(g_png_ptr, c7));
	case LIBPNG_SB_HELPER_OP_SET_GRAY_TO_RGB:
		png_set_gray_to_rgb(g_png_ptr);
		return (0);
	case LIBPNG_SB_HELPER_OP_GET_VALID:
		return(png_get_valid(g_png_ptr, c7, a0));
	case LIBPNG_SB_HELPER_OP_GET_ROWBYTES:
		return(png_get_rowbytes(g_png_ptr, c7));
	case LIBPNG_SB_HELPER_OP_GET_IMAGE_WIDTH:
		return(png_get_image_width(g_png_ptr, c7));
	case LIBPNG_SB_HELPER_OP_GET_IMAGE_HEIGHT:
		return(png_get_image_height(g_png_ptr, c7));
	case LIBPNG_SB_HELPER_OP_GET_BIT_DEPTH:
		return(png_get_bit_depth(g_png_ptr, c7));
	case LIBPNG_SB_HELPER_OP_GET_INTERLACE_TYPE:
		return (png_get_interlace_type(g_png_ptr, c7));
	case LIBPNG_SB_HELPER_OP_SET_READ_FN:
#if 0
		printf("sb_read_fn = 0x%jx\n", cheri_getbase(sb_read_fn));
#endif
		png_set_read_fn(g_png_ptr, c7, sb_read_fn);
		return (0);
	case LIBPNG_SB_HELPER_OP_SET_PROGRESSIVE_READ_FN:
		png_set_progressive_read_fn(g_png_ptr, c7,
		    sb_info_fn, sb_row_fn, sb_end_fn);
		return (0);

	default:
		return (2);
	}
}
