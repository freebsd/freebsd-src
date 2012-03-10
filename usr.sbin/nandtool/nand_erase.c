/*-
 * Copyright (c) 2010-2012 Semihalf.
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/disk.h>
#include <libgeom.h>
#include <dev/nand/nand_cdev.h>
#include "nandtool.h"

int nand_erase(struct cmd_param *params)
{
	char *dev;
	int fd = -1;
	off_t pos, count, err = 0;
	off_t start, nblocks, i;
	int page_size, block_size, mult;

	if (!(dev = param_get_string(params, "dev"))) {
		fprintf(stderr, "Please supply valid 'dev' parameter.\n");
		return (EINVAL);
	}

	if ((fd = g_open(dev, 1)) < 0) {
		perrorf("Cannot open %s", dev);
		return (errno);
	}

	if ((count = param_get_int(params, "count")) < 0)
		count = 1;

	if (ioctl(fd, DIOCGSECTORSIZE, &page_size)) {
		perrorf("Cannot get page size for %s", dev);
		err = errno;
		goto out;
	}

	if (ioctl(fd, DIOCNBLKSIZE, &block_size)) {
		perrorf("Cannot get block size for %s", dev);
		err = errno;
		goto out;
	}

	if (param_has_value(params, "page")) {
		pos = page_size * param_get_int(params, "page");
		mult = page_size;
	} else if (param_has_value(params, "block")) {
		pos = block_size * param_get_int(params, "block");
		mult = block_size;
	} else if (param_has_value(params, "pos")) {
		pos = param_get_int(params, "pos");
		mult = 1;

	} else {
		/* Erase all chip */
		if (ioctl(fd, DIOCGMEDIASIZE, &count) < 0) {
			err = errno;
			goto out;
		}

		pos = 0;
		mult = 1;
	}

	if (pos % block_size) {
		fprintf(stderr, "Position must be block-size aligned!\n");
		err = errno;
		goto out;
	}

	count *= mult;
	start = pos / block_size;
	nblocks = count / block_size;

	for (i = 0; i < nblocks; i++) {
		if (g_delete(fd, (start + i) * block_size, block_size) < 0) {
			perrorf("Cannot erase block %d - probably a bad block",
			    start + i);
		}
	}

out:
	if (fd)
		g_close(fd);

	return (err);
}

