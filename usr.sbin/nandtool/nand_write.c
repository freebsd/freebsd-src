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
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <libgeom.h>
#include <sys/disk.h>
#include <dev/nand/nand_dev.h>
#include "nandtool.h"

int nand_write(struct cmd_param *params)
{
	struct chip_param_io chip_params;
	char *dev, *file;
	int fd = -1, in_fd = -1, ret, err = 0;
	uint8_t *buf = NULL;
	int block_size, mult, pos, done = 0, count, raw;

	raw = param_get_boolean(params, "raw");

	if (!(dev = param_get_string(params, "dev"))) {
		fprintf(stderr, "Please supply 'dev' argument.\n");
		return (EINVAL);
	}

	if (!(file = param_get_string(params, "in"))) {
		fprintf(stderr, "Please supply 'in' argument.\n");
		return (EINVAL);
	}

	if ((fd = g_open(dev, 1)) < 0) {
		perrorf("Cannot open %s", dev);
		return (errno);
	}

	if ((in_fd = open(file, O_RDONLY)) < 0) {
		perrorf("Cannot open file %s", file);
		err = errno;
		goto out;
	}

	if (ioctl(fd, NAND_IO_GET_CHIP_PARAM, &chip_params) == -1) {
		perrorf("Cannot ioctl(NAND_IO_GET_CHIP_PARAM)");
		err = errno;
		goto out;
	}

	block_size = chip_params.page_size * chip_params.pages_per_block;

	if (param_has_value(params, "page")) {
		pos = chip_params.page_size * param_get_int(params, "page");
		mult = chip_params.page_size;
	} else if (param_has_value(params, "block")) {
		pos = block_size * param_get_int(params, "block");
		mult = block_size;
	} else if (param_has_value(params, "pos")) {
		pos = param_get_int(params, "pos");
		mult = 1;
		if (pos % chip_params.page_size) {
			fprintf(stderr, "Position must be page-size "
			    "aligned!\n");
			errno = EINVAL;
			goto out;
		}
	} else {
		fprintf(stderr, "You must specify one of: 'block', 'page',"
		    "'pos' arguments\n");
		errno = EINVAL;
		goto out;
	}

	if (!(param_has_value(params, "count")))
		count = mult;
	else
		count = param_get_int(params, "count") * mult;

	if (!(buf = malloc(chip_params.page_size))) {
		perrorf("Cannot allocate buffer [size %x]",
		    chip_params.page_size);
		err = errno;
		goto out;
	}

	lseek(fd, pos, SEEK_SET);

	while (done < count) {
		if ((ret = read(in_fd, buf, chip_params.page_size)) !=
		    (int32_t)chip_params.page_size) {
			if (ret > 0) {
				/* End of file ahead, truncate here */
				break;
			} else {
				perrorf("Cannot read from %s", file);
				err = errno;
				goto out;
			}
		}

		if ((ret = write(fd, buf, chip_params.page_size)) !=
		    (int32_t)chip_params.page_size) {
			err = errno;
			goto out;
		}

		done += ret;
	}

out:
	if (fd != -1)
		g_close(fd);
	if (in_fd != -1)
		close(in_fd);
	if (buf)
		free(buf);

	return (0);
}

