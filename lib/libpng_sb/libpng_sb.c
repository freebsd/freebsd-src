/*-
 * Copyright (c) 2014 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
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

#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cheri_enter.h>
#include <cheri/cheri_fd.h>
#include <cheri/sandbox.h>

#include <assert.h>
#include <err.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sysexits.h>

#include <libpng_sb-helper.h>

#include <png.h>
#include <pnginfo.h>

struct sb_png_struct {
	struct sandbox_object *objectp;
	jmp_buf longjmp_buffer;
	png_error_ptr error_fn;		/* caller error function */
#ifdef PNG_WARNINGS_SUPPORTED
	png_error_ptr warning_fn;	/* caller warning function */
#endif
	png_voidp error_ptr;

	png_rw_ptr read_data_fn;
	png_voidp io_ptr;

	png_progressive_info_ptr info_fn;
	png_progressive_row_ptr row_fn;
	png_progressive_end_ptr end_fn;

	png_infop info_ptr;
};

struct sb_info_struct {
	__capability void *info_cap;
};

#define DPRINTF printf

static struct sandbox_class *classp;

static register_t
libpng_sb_userfn_handler(struct cheri_object system_object,
    register_t methodnum,
    register_t a0, register_t a1, register_t a2, register_t a3,
    register_t a4, register_t a5, register_t a6, register_t a7,
    __capability void *void_cpsp, __capability void *c4,
    __capability void *c5, __capability void *c6, __capability void *c7)
    __attribute__((cheri_ccall));
    /* XXXRW: Will be ccheri_ccallee. */

static register_t
libpng_sb_userfn_handler(struct cheri_object system_object __unused,
    register_t methodnum,
    register_t a0, register_t a1,
    register_t a2 __unused, register_t a3 __unused, register_t a4 __unused,
    register_t a5 __unused, register_t a6 __unused, register_t a7 __unused,
    __capability void *void_cpsp, __capability void *c4,
    __capability void *c5 __unused, __capability void *c6 __unused,
    __capability void *c7 __unused)
{
	struct sb_png_struct *psp = (struct sb_png_struct *)void_cpsp;

#if 0
	printf("%s:  with method %ju\n", __func__, (intmax_t)methodnum);
	printf("%s: psp at %p\n", __func__, psp);
	printf("%s: cpsp base 0x%jx offset 0x%jx length 0x%zx\n",
	    __func__, cheri_getbase(void_cpsp), cheri_getoffset(void_cpsp),
	    cheri_getlen(void_cpsp));
#endif
	
	switch (methodnum) {
	case LIBPNG_SB_USERFN_READ_CALLBACK:
#if 0
		printf("calling read_data_fn at %p\n", psp->read_data_fn);
		printf("data base 0x%jx offset 0x%jx length 0x%zx (rlen 0x%zx)\n",
		    cheri_getbase(c4), cheri_getoffset(c4), cheri_getlen(c4),
		    (size_t)a0);
#endif
		psp->read_data_fn((png_structp)psp, (png_bytep)c4, a0);
		printf("callback complete\n");
		break;
	case LIBPNG_SB_USERFN_INFO_CALLBACK:
		psp->info_fn((png_structp)psp, (png_infop)c4);
		break;
	case LIBPNG_SB_USERFN_ROW_CALLBACK:
		psp->row_fn((png_structp)psp, (png_bytep)c4, a0, a1);
		break;
	case LIBPNG_SB_USERFN_END_CALLBACK:
		psp->end_fn((png_structp)psp, (png_infop)c4);
		break;
	default:
		warnx("unknown user function %jd", methodnum);
		return (-1);
	}

	return (0);
}

static void
init_sb_class()
{

	/* XXX: wrong error handling for library code */
	if (sandbox_class_new("/usr/libexec/libpng_sb-helper",
	    8*1024*1024, &classp) < 0)
		err(EX_OSFILE, "sandbox_class_new");

	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_CREATE_READ_STRUCT,
	    "png_create_read_struct");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_READ_INFO,
	    "png_read_info");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_SET_EXPAND_GRAY_1_2_4_TO_8,
	    "png_set_expand_gray_1_2_4_to_8");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_SET_PALETTE_TO_RGB,
	    "png_set_palette_to_rgb");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_SET_TRNS_TO_ALPHA,
	    "png_set_tRNS_to_ALPHA");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_SET_FILLER,
	    "png_set_filler");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_SET_STRIP_16,
	    "png_set_strip_16");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_READ_UPDATE_INFO,
	    "png_read_update_info");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_READ_IMAGE,
	    "png_read_image");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_PROCESS_DATA,
	    "png_process_data");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_GET_COLOR_TYPE,
	    "png_get_color_type");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_SET_GRAY_TO_RGB,
	    "png_set_gray_to_rgb");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_GET_VALID,
	    "png_get_valid");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_GET_ROWBYTES,
	    "png_get_rowbytes");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_GET_IMAGE_WIDTH,
	    "png_get_image_width");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_GET_IMAGE_HEIGHT,
	    "png_get_image_height");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_GET_BIT_DEPTH,
	    "png_get_bit_depth");
	(void)sandbox_class_method_declare(classp,
	    LIBPNG_SB_HELPER_OP_GET_INTERLACE_TYPE,
	    "png_get_interlace_type");

	cheri_system_user_register_fn(&libpng_sb_userfn_handler);

	DPRINTF("sandbox class created\n");
}

/* Reduce errors and copy-and-paste by standardizing c3-6 */
static register_t
sb_cinvoke(struct sandbox_object *objectp, register_t methodnum,
    register_t a1, register_t a2, register_t a3, register_t a4,
    register_t a5, register_t a6, register_t a7,
    __capability void *c7, __capability void *c8,
    __capability void *c9, __capability void *c10)
{

	return (sandbox_object_cinvoke(objectp,
	    methodnum,
	    a1, a2, a3, a4, a5, a6, a7,
	    sandbox_object_getsystemobject(objectp).co_codecap,
	    sandbox_object_getsystemobject(objectp).co_datacap,
	    cheri_zerocap(), cheri_zerocap(), c7, c8, c9, c10));
}

static inline __capability void *
sb_info_ptr_to_cap(png_const_infop info_ptr)
{
	
	return(((struct sb_info_struct *)info_ptr)->info_cap);
}

png_structp
png_create_read_struct(png_const_charp user_png_ver, png_voidp error_ptr,
    png_error_ptr error_fn, png_error_ptr warning_fn)
{
	struct sb_png_struct *psp;
	register_t v;

	if (strncmp(PNG_LIBPNG_VER_STRING, user_png_ver,
	    sizeof(PNG_LIBPNG_VER_STRING)) != 0) {
		if (warning_fn != NULL)
			warnx("png_create_read_struct called with wrong version"
			    "got '%s' expected '%s'", user_png_ver,
			    PNG_LIBPNG_VER_STRING);
		return (NULL);
	}

	psp = calloc(1, sizeof(struct sb_png_struct));
	if (psp == NULL)
		return (NULL);

	psp->error_ptr = error_ptr;
	psp->error_fn = error_fn;
#ifdef PNG_WARNINGS_SUPPORTED
	psp->warning_fn = warning_fn;
#endif

	if (classp == NULL)
		init_sb_class();
	/* XXX: should use a sandbox pool */
	if (sandbox_object_new(classp, 4*1024*1024, &psp->objectp) < 0)
		err(EX_OSFILE, "sandbox_object_new");

	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_CREATE_READ_STRUCT,
	    0, 0, 0, 0, 0, 0, 0,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v != 0)
		errx(EX_OSFILE,
		    "sb_cinvoke(OP_CREATE_READ_STRUCT) returned 0x%jx",
		    (uintmax_t)v);
	
	return ((png_structp)psp);
}

jmp_buf *
png_set_longjmp_fn(png_structp png_ptr,
    png_longjmp_ptr longjmp_fn __unused, size_t jmp_buf_size)
{
	struct sb_png_struct *ps = (struct sb_png_struct *)png_ptr;

	assert(sizeof(ps->longjmp_buffer) == jmp_buf_size);

	return &ps->longjmp_buffer;
}

png_infop
png_create_info_struct(png_structp png_ptr __unused)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	struct sb_info_struct *pip;

	pip = calloc(1, sizeof(struct sb_info_struct));
	if (pip == NULL)
		return (NULL);

	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_CREATE_INFO_STRUCT,
	    0, 0, 0, 0, 0, 0, 0,
	    cheri_csetbounds((__capability void *)&pip->info_cap, sizeof(__capability void *)),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v != 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn(png_ptr, "png_create_read_info failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
	if (pip->info_cap == (__capability void *)NULL) {
		free(pip);
		return (NULL);
	}
	/* XXX: hope the first one that gets the header read into it */
	if (psp->info_ptr == NULL)
		psp->info_ptr = (png_infop)pip;
	return ((png_infop)pip);
}

void
png_read_info(png_structp png_ptr, png_infop info_ptr)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_READ_INFO,
	    0, 0, 0, 0, 0, 0, 0,
	    sb_info_ptr_to_cap(info_ptr),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v != 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn(png_ptr, "png_read_info failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
}

void
png_set_expand_gray_1_2_4_to_8(png_structp png_ptr)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp,
	    LIBPNG_SB_HELPER_OP_SET_EXPAND_GRAY_1_2_4_TO_8,
	    0, 0, 0, 0, 0, 0, 0,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v != 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn(png_ptr,
		    "png_set_expand_gray_1_2_4_to_8 failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
}

void
png_set_palette_to_rgb(png_structp png_ptr)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp,
	    LIBPNG_SB_HELPER_OP_SET_PALETTE_TO_RGB,
	    0, 0, 0, 0, 0, 0, 0,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v != 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn(png_ptr,
		    "png_set_palette_to_rgb failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
}

void
png_set_tRNS_to_alpha(png_structp png_ptr)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp,
	    LIBPNG_SB_HELPER_OP_SET_TRNS_TO_ALPHA,
	    0, 0, 0, 0, 0, 0, 0,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v != 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn(png_ptr,
		    "png_set_tRNS_to_alpha failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
}

void
png_set_filler(png_structp png_ptr, png_uint_32 filler, int flags)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp,
	    LIBPNG_SB_HELPER_OP_SET_FILLER,
	    filler, flags, 0, 0, 0, 0, 0,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v != 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn(png_ptr, "png_set_filler failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
}

void
png_set_strip_16(png_structp png_ptr)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp,
	    LIBPNG_SB_HELPER_OP_SET_STRIP_16,
	    0, 0, 0, 0, 0, 0, 0,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v != 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn(png_ptr, "png_set_strip_16 failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
}

#if 0
void
png_set_gamma(png_structp png_ptr, double screen_gamma,
    double override_file_gamma)
{
}
#endif

void
png_read_update_info(png_structp png_ptr, png_infop info_ptr)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_READ_UPDATE_INFO,
	    0, 0, 0, 0, 0, 0, 0,
	    sb_info_ptr_to_cap(info_ptr),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v != 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn(png_ptr, "png_read_update_info failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
}

void
png_read_image(png_structp png_ptr, png_bytepp image)
{
	register_t v;
	uint32_t height, i, rowbytes;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	void * __capability *row_pointer; /* XXX-BD was __capability png_bytepp */

#if 0
	printf("%s: psp at %p\n", __func__, psp);
#endif

	height = png_get_image_height(png_ptr, psp->info_ptr);
	rowbytes = png_get_rowbytes(png_ptr, psp->info_ptr);
	printf("%s: allocating %u rows of 0x%u bytes\n", __func__,
	    height, rowbytes);

	row_pointer = calloc(height, sizeof(__capability void *));
	if (row_pointer == NULL) {
		warnx("%s: failed to malloc space for %u rows",
		    __func__, height);
		psp->error_fn(png_ptr, "png_read_image malloc failure");
		errx(1, "%s: error_fn returned", __func__);
	}
	for (i = 0; i < height; i++)
		row_pointer[i] = cheri_ptr(image[i], rowbytes);
	
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_READ_IMAGE,
	    0, 0, 0, 0, 0, 0, 0,
	    (__capability void *)row_pointer,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v != 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn(png_ptr, "png_read_image failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
}

void
png_destroy_read_struct(png_structpp png_ptr_ptr,
    png_infopp info_ptr_ptr, png_infopp end_info_ptr_ptr)
{
	struct sb_png_struct *psp;

	if (png_ptr_ptr == NULL)
		return;
	psp= (struct sb_png_struct *)*png_ptr_ptr;
	if (psp == NULL)
		return;
	if (psp != NULL) {
		/* XXX: should use a sandbox pool */
		sandbox_object_destroy(psp->objectp);
		free(psp);
		*png_ptr_ptr = NULL;
	}

	if (info_ptr_ptr != NULL && *info_ptr_ptr != NULL) {
		free(*info_ptr_ptr);
		*info_ptr_ptr = NULL;
	}

	if (end_info_ptr_ptr != NULL && *end_info_ptr_ptr != NULL) {
		free(*end_info_ptr_ptr);
		*end_info_ptr_ptr = NULL;
	}
}

void
png_set_error_fn(png_structp png_ptr, png_voidp error_ptr,
    png_error_ptr error_fn, png_error_ptr warning_fn)
{
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;

	psp->error_ptr = error_ptr;
	psp->error_fn = error_fn;
#ifdef PNG_WARNINGS_SUPPORTED
	psp->warning_fn = warning_fn;
#endif
}

void
png_set_read_fn(png_structp png_ptr, png_voidp io_ptr,
    png_rw_ptr read_data_fn)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	psp->io_ptr = io_ptr;
	psp->read_data_fn = read_data_fn;

	/*
	 * No arguments required, we're just triggering setup of the
	 * callbacks.
	 */
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_SET_READ_FN,
	    0, 0, 0, 0, 0, 0, 0,
	    cheri_ptr(psp, sizeof(struct sb_png_struct)),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v != 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn(png_ptr,
		    "png_set_progressive_read_fn failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
}

png_voidp
png_get_io_ptr(png_structp png_ptr)
{
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;

	return (psp->io_ptr);
}

/* png_set_progressive_read_fn(info_callback, row_callback, end_callback)*/
void
png_set_progressive_read_fn(png_structp png_ptr,
    png_voidp progressive_ptr, png_progressive_info_ptr info_fn,
    png_progressive_row_ptr row_fn, png_progressive_end_ptr end_fn)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;

	psp->io_ptr = progressive_ptr; /* Yes really */
	psp->info_fn = info_fn;
	psp->row_fn = row_fn;
	psp->end_fn = end_fn;

	
	/*
	 * No arguments required, we're just triggering setup of the
	 * callbacks.
	 */
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_SET_PROGRESSIVE_READ_FN,
	    0, 0, 0, 0, 0, 0, 0,
	    cheri_ptr(psp, sizeof(struct sb_png_struct)),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v != 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn(png_ptr,
		    "png_set_progressive_read_fn failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
}

png_voidp
png_get_progressive_ptr(png_const_structp png_ptr)
{
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;

	return (psp->io_ptr); /* Yes really */
}

void
png_process_data(png_structp png_ptr, png_infop info_ptr,
    png_bytep buffer, png_size_t buffer_size)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_PROCESS_DATA,
	    buffer_size, 0, 0, 0, 0, 0, 0,
	    sb_info_ptr_to_cap(info_ptr),
	    cheri_ptr(buffer, buffer_size), cheri_zerocap(), cheri_zerocap());
	if (v != 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn(png_ptr, "png_process_data failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
}

png_byte
png_get_color_type(png_const_structp png_ptr,
    png_const_infop info_ptr)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_GET_COLOR_TYPE,
	    0, 0, 0, 0, 0, 0, 0,
	    sb_info_ptr_to_cap(info_ptr),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v < 0 || v > 255) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn((png_structp)png_ptr, "png_get_color_type failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
	return (v);
}

void
png_set_gray_to_rgb(png_structp png_ptr)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_SET_GRAY_TO_RGB,
	    0, 0, 0, 0, 0, 0, 0,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v != 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn(png_ptr, "png_set_gray_to_rgb failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
}

png_uint_32
png_get_valid(png_const_structp png_ptr, png_const_infop info_ptr,
    png_uint_32 flag)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_GET_VALID,
	    flag, 0, 0, 0, 0, 0, 0,
	    sb_info_ptr_to_cap(info_ptr),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if ((png_uint_32)v == flag)
		return (flag);
	else if (v == 0)
		return (0);
	else {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn((png_structp)png_ptr,
		    "png_get_valid failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
}

png_size_t
png_get_rowbytes(png_const_structp png_ptr, png_const_infop info_ptr)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_GET_ROWBYTES,
	    0, 0, 0, 0, 0, 0, 0,
	    sb_info_ptr_to_cap(info_ptr),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());

	if (v < 0) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn((png_structp)png_ptr,
		    "png_get_rowbytes failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
	return (v);
}

png_uint_32
png_get_image_width(png_const_structp png_ptr, png_const_infop info_ptr)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_GET_IMAGE_WIDTH,
	    0, 0, 0, 0, 0, 0, 0,
	    sb_info_ptr_to_cap(info_ptr),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v < 0 || v > UINT_MAX) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn((png_structp)png_ptr,
		    "png_get_image_width failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
	return (v);
}

png_uint_32
png_get_image_height(png_const_structp png_ptr, png_const_infop info_ptr)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_GET_IMAGE_HEIGHT,
	    0, 0, 0, 0, 0, 0, 0,
	    sb_info_ptr_to_cap(info_ptr),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v < 0 || v > UINT_MAX) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn((png_structp)png_ptr,
		    "png_get_image_height failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
	return (v);
}

png_byte
png_get_bit_depth(png_const_structp png_ptr, png_const_infop info_ptr)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_GET_BIT_DEPTH,
	    0, 0, 0, 0, 0, 0, 0,
	    sb_info_ptr_to_cap(info_ptr),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v < 0 || v > 255) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn((png_structp)png_ptr,
		    "png_get_bit_depth failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
	return (v);
}

png_byte
png_get_interlace_type(png_const_structp png_ptr, png_const_infop info_ptr)
{
	register_t v;
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	v = sb_cinvoke(psp->objectp, LIBPNG_SB_HELPER_OP_GET_INTERLACE_TYPE,
	    0, 0, 0, 0, 0, 0, 0,
	    sb_info_ptr_to_cap(info_ptr),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap());
	if (v < 0 || v > 255) {
		warnx("%s: sb_cinvoke() returned %jx", __func__, (uintmax_t)v);
		psp->error_fn((png_structp)png_ptr,
		    "png_get_interlace_type failed in sandbox");
		errx(1, "%s: error_fn returned", __func__);
	}
	return (v);
}

#if 0
png_uint_32
png_get_gAMA (png_const_structp png_ptr, png_const_infop info_ptr,
    double *file_gamma)
{
}

png_uint_32
png_get_sRGB(png_const_structp png_ptr, png_const_infop info_ptr,
    int *file_srgb_intent)
{
}
#endif

void png_error
(png_structp png_ptr, png_const_charp error_message)
{
	struct sb_png_struct *psp = (struct sb_png_struct *)png_ptr;
	
	if (psp->error_fn != NULL)
		psp->error_fn(png_ptr, error_message);
	errx(1, "%s", error_message);
}
