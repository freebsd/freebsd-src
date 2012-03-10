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
#include <sys/types.h>
#include <sys/disk.h>
#include <dev/nand/nand_cdev.h>
#include "nandtool.h"

int nand_read_oob(struct cmd_param *params)
{
	char *dev, *out;
	int fd = -1, fd_out = -1;
	int err = 0;
	uint8_t *buf = NULL;
	int oobsize, page, pagesize;
	struct nand_oob_request req;

	if ((page = param_get_int(params, "page")) < 0) {
		fprintf(stderr, "You must supply valid 'page' argument.\n");
		return (EINVAL);
	}

	if (!(dev = param_get_string(params, "dev"))){
		fprintf(stderr, "You must supply 'dev' argument.\n");
		return (EINVAL);
	}

	if ((out = param_get_string(params, "out"))) {
		if ((fd_out = open(out, O_WRONLY | O_CREAT)) < 0) {
			perrorf("Cannot open %s", out);
			err = errno;
			goto out;
		}
	}

	if ((fd = g_open(dev, 1)) < 0) {
		perrorf("Cannot open %s", dev);
		err = errno;
		goto out;
	}

	if (ioctl(fd, DIOCGSECTORSIZE, &pagesize)) {
		perrorf("Cannot get page size for %s", dev);
		err = errno;
		goto out;
	}

	if (ioctl(fd, DIOCNOOBSIZE, &oobsize)) {
		perrorf("Cannot get OOB size for %s", dev);
		err = errno;
		goto out;
	}

	buf = malloc(oobsize);
	if (buf == NULL) {
		perrorf("Cannot allocate %d bytes\n", oobsize);
		err = errno;
		goto out;
	}

	req.length = oobsize;
	req.offset = page * pagesize;
	req.ubuf = buf;

	if (ioctl(fd, DIOCNREADOOB, &req)) {
		perrorf("Cannot read OOB from %s", dev);
		err = errno;
		goto out;
	}

	if (fd_out != -1)
		write(fd_out, buf, oobsize);
	else
		hexdump(buf, oobsize);

out:
	if (fd != -1)
		close(fd);
	if (fd_out != -1)
		g_close(fd_out);
	if (buf)
		free(buf);

	return (err);
}

