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
#include <dev/nand/nand_cdev.h>
#include "nandtool.h"

int nand_read(struct cmd_param *params)
{
	int fd = -1, out_fd = -1, ret;
	char *dev, *out;
	uint8_t *buf = NULL;
	int pos, done = 0, count, mult, page_size, block_size;
	int err = 0;

	if (!(dev = param_get_string(params, "dev"))) {
		fprintf(stderr, "You must specify 'dev' parameter\n");
		return (EINVAL);
	}

	if ((out = param_get_string(params, "out"))) {
		out_fd = open(out, O_WRONLY|O_CREAT);
		if (out_fd < 0) {
			perrorf("Cannot open %s for writing", out);
			return (EINVAL);
		}
	}

	if ((fd = g_open(dev, 1)) < 0) {
		perrorf("Cannot open %s", dev);
		err = errno;
		goto out;
	}

	if (ioctl(fd, DIOCNBLKSIZE, &block_size) < 0) {
		perrorf("ioctl(DIOCNBLKSIZE) failed");
		err = errno;
		goto out;
	}

	if (ioctl(fd, DIOCGSECTORSIZE, &page_size) < 0) {
		perrorf("ioctl(DIOCGSECTORSIZE) failed");
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
		if (pos % page_size) {
			fprintf(stderr, "Position must be page-size aligned!\n");
			err = errno;
			goto out;
		}
	} else {
		fprintf(stderr, "You must specify one of: 'block', 'page',"
		    "'pos' arguments\n");
		err = errno;
		goto out;
	}

	if (!(param_has_value(params, "count")))
		count = mult;
	else
		count = param_get_int(params, "count") * mult;

	if (!(buf = malloc(page_size))) {
		perrorf("Cannot allocate buffer [size %x]", page_size);
		err = errno;
		goto out;
	}

	lseek(fd, pos, SEEK_SET);

	while (done < count) {
		if ((ret = read(fd, buf, page_size)) != page_size) {
			perrorf("read error (read %d bytes)", ret);
			goto out;
		}

		done += ret;

		if (out_fd != -1) {
			if ((ret = write(out_fd, buf, page_size)) != page_size) {
				perrorf("write error (written %d bytes)", ret);
				err = errno;
				goto out;
			}
		} else
			hexdump(buf, page_size);
	}

out:
	if (fd != -1)
		g_close(fd);
	if (out_fd != -1)
		close(out_fd);
	if (buf)
		free(buf);

	return (err);
}

