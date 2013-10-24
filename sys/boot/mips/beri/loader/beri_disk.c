/*-
 * Copyright (c) 2013 Robert N. M. Watson
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <bootstrap.h>

#include <stand.h>
#include <cfi.h>

static int	beri_disk_init(void);
static int	beri_disk_strategy(void *, int, daddr_t, size_t, char *,
		    size_t *);
static int	beri_disk_open(struct open_file *, ...);
static int	beri_disk_close(struct open_file *);
static int	beri_disk_ioctl(struct open_file *, u_long, void *);
static void	beri_disk_print(int);
static void	beri_disk_cleanup(void);

struct devsw beri_disk = {
	.dv_name = "cfi",
	.dv_type = DEVT_DISK,
	.dv_init = beri_disk_init,
	.dv_strategy = beri_disk_strategy,
	.dv_open = beri_disk_open,
	.dv_close = beri_disk_close,
	.dv_ioctl = beri_disk_ioctl,
	.dv_print = beri_disk_print,
 	.dv_cleanup = beri_disk_cleanup,
};

static int
beri_disk_init(void)
{

	return (0);
}

static int
beri_disk_strategy(void *devdata, int flag, daddr_t dblk, size_t size,
    char *buf, size_t *rsizep)
{
	int error;

	if (flag != F_READ)
		return (EROFS);
	error = cfi_read(buf, dblk, size >> 9);
	if (error == 0)
		*rsizep = size;
	return (error);
}

static int
beri_disk_open(struct open_file *f, ...)
{

	return (0);
}

static int
beri_disk_close(struct open_file *f)
{

	return (0);
}

static int
beri_disk_ioctl(struct open_file *f, u_long cmd, void *data)
{

	return (EINVAL);
}

static void
beri_disk_print(int verbose)
{

	printf("    cfi\n");
}

static void
beri_disk_cleanup(void)
{

}
