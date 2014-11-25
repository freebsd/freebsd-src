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

#include <sys/endian.h>

#include <terasic_mtl.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <imagebox.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void
usage(void)
{
	errx(1, "usage: pngsb <file>");
}

int
main(int argc, char **argv)
{	
	int pfd;
	uint32_t i, last_row = 0;
	struct iboxstate *ps;

	if (argc != 2)
		usage();

	fb_init();

	if ((pfd = open(argv[1], O_RDONLY)) < -1)
		err(1, "open(%s)", argv[1]);

	if ((ps = png_read_start(pfd, 800, 480, SB_CHERI)) == NULL)
		err(1, "failed to initialize read of %s", argv[1]);

	/* XXX: do something with the valid parts of the image as it decodes. */
	while(ps->valid_rows < ps->height ) {
		if (last_row != ps->valid_rows) {
			for (i = last_row; i < ps->valid_rows; i++)
				memcpy(__DEVOLATILE(void*,
				     pfbp + (i * fb_width)),
				    __DEVOLATILE(void *,
				    ps->buffer + (i * ps->width)),
				    sizeof(uint32_t) * ps->width);
			last_row = ps->valid_rows;
			printf("valid_rows = %d\n", ps->valid_rows);
		}
	}
	if (last_row != ps->valid_rows) {
		for (i = last_row; i < ps->valid_rows; i++)
			memcpy(__DEVOLATILE(void*,
			     pfbp + (i * fb_width)),
			    __DEVOLATILE(void *,
			    ps->buffer + (i * ps->width)),
			    sizeof(uint32_t) * ps->width);
		last_row = ps->valid_rows;
	}
	printf("valid_rows = %d\n", ps->valid_rows);

	if (png_read_finish(ps) != 0)
		errx(1, "png_read_finish failed");

	iboxstate_free(ps);

	fb_fini();

	return(0);
}
