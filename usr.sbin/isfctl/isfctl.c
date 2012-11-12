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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* XXXBED: should install and include sys/dev/isf.h */
struct isf_range {
	off_t	ir_off;		/* Offset of range to delete (set to 0xFF) */
	size_t	ir_size;	/* Size of range */
};

#define ISF_ERASE	_IOW('I', 1, struct isf_range)

#define ISF_ERASE_BLOCK (128 * 1024)

static enum {UNSET, ERASE} action = UNSET;

static void
usage(void)
{
	fprintf(stderr, "usage: isfctl <device> erase <offset> <size>\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	struct isf_range	ir;
	int			fd, i;
	char			*p, *dev;
	
	if (argc < 2)
		usage();
	argc--; argv++;
	
	if (*argv[0] == '/')
		dev = argv[0];
	else
		asprintf(&dev, "/dev/%s", argv[0]);
	argc--; argv++;
	fd = open(dev, O_RDWR);
	if (fd < 0)
		err(1, "unable to open device -- %s", dev);

	if (strcmp(argv[0], "erase") == 0) {
		if (argc != 3)
			usage();
		action = ERASE;
		ir.ir_off = strtol(argv[1], &p, 0);
		if (*p)
			errx(1, "invalid offset -- %s", argv[2]);
		ir.ir_size = strtol(argv[2], &p, 0);
		if (*p)
			errx(1, "invalid size -- %s", argv[3]);
		/*
		 * If the user requests to delete less than 32K of space
		 * then assume that they want to delete a number of 128K
		 * blocks.
		 */
		if (ir.ir_size < 32 * 1024)
			ir.ir_size *= 128 * 1024;
	}

	switch (action) {
	case ERASE:
		i = ioctl(fd, ISF_ERASE, &ir);
		if (i < 0)
			err(1, "ioctl(%s, %jx, %zx)", dev,
			    (intmax_t)ir.ir_off, ir.ir_size);
		break;
	default:
		usage();
	}

	close(fd);
	return (0);
}
