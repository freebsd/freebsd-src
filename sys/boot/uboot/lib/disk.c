/*-
 * Copyright (C) 2000 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Disk I/O routines using U-Boot - TODO
 */

#include <sys/param.h>
#include <sys/queue.h>

#include <netinet/in.h>
#include <machine/stdarg.h>
#include <stand.h>

#include "bootstrap.h"

static int	d_init(void);
static int	d_strategy(void *devdata, int flag, daddr_t dblk,
		    size_t size, char *buf, size_t *rsize);
static int	d_open(struct open_file *f, ...);
static int	d_close(struct open_file *f);
static int	d_ioctl(struct open_file *f, u_long cmd, void *data);
static void	d_print(int verbose);

struct devsw uboot_disk = {
	"block",
	DEVT_DISK,
	d_init,
	d_strategy,
	d_open,
	d_close,
	d_ioctl,
	d_print
};

struct opened_dev {
	u_int			count;
	SLIST_ENTRY(opened_dev)	link;
};

SLIST_HEAD(, opened_dev) opened_devs = SLIST_HEAD_INITIALIZER(opened_dev);

static int
d_init(void)
{

	return 0;
}

static int
d_strategy(void *devdata, int flag, daddr_t dblk, size_t size, char *buf,
    size_t *rsize)
{

	return (EINVAL);
}

static int
d_open(struct open_file *f, ...)
{

	return (EINVAL);
}

static int
d_close(struct open_file *f)
{

	return (EINVAL);
}

static int
d_ioctl(struct open_file *f, u_long cmd, void *data)
{

	return (EINVAL);
}

static void
d_print(int verbose)
{

	return;
}
