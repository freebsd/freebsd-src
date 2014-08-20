/*-
 * Copyright (c) 2012-2013 Robert N. M. Watson
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
#include <sys/capability.h>
#include <sys/mman.h>

#include <machine/cheri.h>
#include <machine/sysarch.h>

#include <cheri/cheri_memcpy.h>

#include <png.h>
#include <stdlib.h>

#include "imagebox.h"
#include "iboxpriv.h"

int	invoke(register_t a0, register_t a1, register_t a2, register_t a3);

int pngwidth;

static void
cheri_read_data(png_structp png_ptr __unused, png_bytep data, png_size_t length)
{
	struct ibox_decode_state *ids;

	ids = png_get_io_ptr(png_ptr);
	memcpy_fromcap(data, 4, ids->offset, length);
	ids->offset += length;
}

static void
cheri_read_row_callback(png_structp png_ptr __unused, png_uint_32 row __unused,
    int pass __unused)
{
#if 0
	struct ibox_decode_state *ids;

	ids = png_get_io_ptr(png_ptr);

	memcpy_tocap(3, ids->buffer + (pngwidth * (row - 1)),
	    sizeof(uint32_t) * pngwidth * (row - 1),
	    sizeof(uint32_t) * pngwidth);
#endif
}

/*
 * Sandboxed imagebox png reader.  
 * 
 * The output buffer is passed in c1.  The pngfile is accessable via c2.
 * a0 holds the image width, a1 the height, and a2 holds the length of the
 * pngfile (currently unused).
 */
int
invoke(register_t a0, register_t a1, register_t a2 __unused,
    register_t a3 __unused)
{
	struct ibox_decode_state	ids;
	struct iboxstate		is;

	pngwidth = a0;

	is.width = a0;
	is.height = a1;
	is.error = 0;
	is.sb = SB_CHERI;

	ids.fd = -1;
	ids.offset = 0;
	/*
	 * in principle we could update this via a capabilty,
	 * but in practice we can reconstruct it on exit
	 */
	ids.is = &is;
	if ((ids.buffer = malloc(sizeof(uint32_t) * a0 * a1)) == NULL)
		return (1);

	decode_png(&ids, cheri_read_data, cheri_read_row_callback);

	/* Copy the whole image out */
	if (is.error == 0)
		memcpy_tocap(3, ids.buffer, 0, sizeof(uint32_t) * a0 * a1);

	memcpy_tocap(5, (uint32_t*)is.times + 1, 0, sizeof(uint32_t) * 2);

	return (is.error);
}
