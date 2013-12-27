/*-
 * Copyright (C) 2011 glevand <geoffrey.levand@mail.ru>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <machine/stdarg.h>
#include <stand.h>

#include "bootstrap.h"
#include "ps3bus.h"
#include "ps3devdesc.h"
#include "ps3stor.h"

#define dev_printf(dev, fmt, args...)	\
	printf("%s%d: " fmt "\n", dev->d_dev->dv_name, dev->d_unit, ##args)

#ifdef CD_DEBUG
#define DEBUG(fmt, args...)		printf("%s:%d: " fmt "\n", __func__, __LINE__, ##args)
#else
#define DEBUG(fmt, args...)
#endif

static int ps3cdrom_init(void);
static int ps3cdrom_strategy(void *devdata, int flag, daddr_t dblk,
	size_t size, char *buf, size_t *rsize);
static int ps3cdrom_open(struct open_file *f, ...);
static int ps3cdrom_close(struct open_file *f);
static void ps3cdrom_print(int verbose);

struct devsw ps3cdrom = {
	"cd",
	DEVT_CD,
	ps3cdrom_init,
	ps3cdrom_strategy,
	ps3cdrom_open,
	ps3cdrom_close,
	noioctl,
	ps3cdrom_print,
};

static struct ps3_stordev stor_dev;

static int ps3cdrom_init(void)
{
	int err;

	err = ps3stor_setup(&stor_dev, PS3_DEV_TYPE_STOR_CDROM);
	if (err)
		return err;

	return 0;
}

static int ps3cdrom_strategy(void *devdata, int flag, daddr_t dblk,
	size_t size, char *buf, size_t *rsize)
{
	struct ps3_devdesc *dev = (struct ps3_devdesc *) devdata;
	int err;

	DEBUG("d_unit=%u dblk=%llu size=%u", dev->d_unit, dblk, size);

	if (flag != F_READ) {
		dev_printf(dev, "write operation is not supported!");
		return EROFS;
	}

	if (dblk % (stor_dev.sd_blksize / DEV_BSIZE) != 0)
		return EINVAL;

	dblk /= (stor_dev.sd_blksize / DEV_BSIZE);

	if (size % stor_dev.sd_blksize) {
		dev_printf(dev,
		    "size=%u is not multiple of device block size=%llu", size,
		    stor_dev.sd_blksize);
		return EINVAL;
	}

	if (rsize)
		*rsize = 0;

	err = ps3stor_read_sectors(&stor_dev, dev->d_unit, dblk,
		size / stor_dev.sd_blksize, 0, buf);

	if (!err && rsize)
		*rsize = size;

	if (err)
		dev_printf(dev,
		    "read operation failed dblk=%llu size=%d err=%d", dblk,
		    size, err);

	return err;
}

static int ps3cdrom_open(struct open_file *f, ...)
{
	char buf[2048];
	va_list ap;
	struct ps3_devdesc *dev;
	int err;

	va_start(ap, f);
	dev = va_arg(ap, struct ps3_devdesc *);
	va_end(ap);

	if (dev->d_unit > 0) {
		dev_printf(dev, "attempt to open nonexistent disk");
		return ENXIO;
	}

	err = ps3stor_read_sectors(&stor_dev, dev->d_unit, 16, 1, 0, buf);
	if (err)
		return EIO;

	/* Do not attach if not ISO9660 (workaround for buggy firmware) */
	if (memcmp(buf, "\001CD001", 6) != 0)
		return EIO;

	return 0;
}

static int ps3cdrom_close(struct open_file *f)
{
	return 0;
}

static void ps3cdrom_print(int verbose)
{
}
