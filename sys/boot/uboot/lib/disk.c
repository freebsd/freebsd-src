/*-
 * Copyright (c) 2008 Semihalf, Rafal Jaworowski
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
 *
 */

/*
 * Block storage I/O routines for U-Boot
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <machine/stdarg.h>
#include <stand.h>
#include <uuid.h>

#define FSTYPENAMES
#include <sys/disklabel.h>

#include "api_public.h"
#include "bootstrap.h"
#include "glue.h"
#include "libuboot.h"

#define DEBUG
#undef DEBUG

#define stor_printf(fmt, args...) do {			\
    printf("%s%d: ", dev->d_dev->dv_name, dev->d_unit);	\
    printf(fmt, ##args);				\
} while (0)

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

struct gpt_part {
	int		gp_index;
	uuid_t		gp_type;
	uint64_t	gp_start;
	uint64_t	gp_end;
};

struct open_dev {
	int		od_bsize;	/* block size */
	int		od_bstart;	/* start block offset from beginning of disk */
	int		od_type;
#define OD_BSDLABEL	0x0001
#define	OD_GPT		0x0002
	union {
		struct {
			struct disklabel bsdlabel;
		} _bsd;
		struct {
			struct gpt_part	*gpt_partitions;
			int		gpt_nparts;
		} _gpt;
	} _data;
};

#define	od_bsdlabel	_data._bsd.bsdlabel
#define	od_nparts	_data._gpt.gpt_nparts
#define	od_partitions	_data._gpt.gpt_partitions

static int stor_info[UB_MAX_DEV];
static int stor_info_no = 0;
static int stor_opendev(struct open_dev **, struct uboot_devdesc *);
static int stor_closedev(struct uboot_devdesc *);
static int stor_readdev(struct uboot_devdesc *, daddr_t, size_t, char *);
static int stor_open_count = 0;

/* devsw I/F */
static int stor_init(void);
static int stor_strategy(void *, int, daddr_t, size_t, char *, size_t *);
static int stor_open(struct open_file *, ...);
static int stor_close(struct open_file *);
static void stor_print(int);

struct devsw uboot_storage = {
	"disk",
	DEVT_DISK,
	stor_init,
	stor_strategy,
	stor_open,
	stor_close,
	noioctl,
	stor_print
};

static int
stor_init(void)
{
	struct device_info *di;
	int i, found = 0;

	if (devs_no == 0) {
		printf("No U-Boot devices! Really enumerated?\n");
		return (-1);
	}

	for (i = 0; i < devs_no; i++) {
		di = ub_dev_get(i);
		if ((di != NULL) && (di->type & DEV_TYP_STOR)) {
			if (stor_info_no >= UB_MAX_DEV) {
				printf("Too many storage devices: %d\n",
				    stor_info_no);
				return (-1);
			}
			stor_info[stor_info_no++] = i;
			found = 1;
		}
	}

	if (!found) {
		printf("No storage devices\n");
		return (-1);
	}

	debugf("storage devices found: %d\n", stor_info_no);
	return (0);
}

static int
stor_strategy(void *devdata, int rw, daddr_t blk, size_t size, char *buf,
    size_t *rsize)
{
	struct uboot_devdesc *dev = (struct uboot_devdesc *)devdata;
	struct open_dev *od = (struct open_dev *)dev->d_disk.data;
	int bcount, err;

	debugf("od=%p, size=%d, bsize=%d\n", od, size, od->od_bsize);

	if (rw != F_READ) {
		stor_printf("write attempt, operation not supported!\n");
		return (EROFS);
	}

	if (size % od->od_bsize) {
		stor_printf("size=%d not multiple of device block size=%d\n",
		    size, od->od_bsize);
		return (EIO);
	}
	bcount = size / od->od_bsize;

	if (rsize)
		*rsize = 0;

	err = stor_readdev(dev, blk + od->od_bstart, bcount, buf);
	if (!err && rsize)
		*rsize = size;

	return (err);
}

static int
stor_open(struct open_file *f, ...)
{
	va_list ap;
	struct open_dev *od;
	struct uboot_devdesc *dev;
	int err;

	va_start(ap, f);
	dev = va_arg(ap, struct uboot_devdesc *);
	va_end(ap);

	if ((err = stor_opendev(&od, dev)) != 0)
		return (err);

	((struct uboot_devdesc *)(f->f_devdata))->d_disk.data = od;

	return (0);
}

static int
stor_close(struct open_file *f)
{
	struct uboot_devdesc *dev;

	dev = (struct uboot_devdesc *)(f->f_devdata);

	return (stor_closedev(dev));
}

static int
stor_open_gpt(struct open_dev *od, struct uboot_devdesc *dev)
{

	/* TODO */
	return (ENXIO);
}

static int
stor_open_bsdlabel(struct open_dev *od, struct uboot_devdesc *dev)
{
	char *buf;
	struct disklabel *dl;
	int err = 0;

	/* Allocate 1 block */
	buf = malloc(od->od_bsize);
	if (!buf) {
		stor_printf("could not allocate memory for disklabel\n");
		return (ENOMEM);
	}

	/* Read disklabel */
	err = stor_readdev(dev, LABELSECTOR, 1, buf);
	if (err) {
		stor_printf("disklabel read error=%d\n", err);
		err = ERDLAB;
		goto out;
	}
	bcopy(buf + LABELOFFSET, &od->od_bsdlabel, sizeof(struct disklabel));
	dl = &od->od_bsdlabel;

	if (dl->d_magic != DISKMAGIC) {
		stor_printf("no disklabel magic!\n");
		err = EUNLAB;
		goto out;
	}
	od->od_type = OD_BSDLABEL;
	od->od_bstart = dl->d_partitions[dev->d_disk.partition].p_offset;

	debugf("bstart=%d\n", od->od_bstart);

out:
	free(buf);
	return (err);
}

static int
stor_readdev(struct uboot_devdesc *dev, daddr_t blk, size_t size, char *buf)
{
	lbasize_t real_size;
	int err, handle;

	debugf("reading size=%d @ 0x%08x\n", size, (uint32_t)buf);

	handle = stor_info[dev->d_unit];
	err = ub_dev_read(handle, buf, size, blk, &real_size);
	if (err != 0) {
		stor_printf("read failed, error=%d\n", err);
		return (EIO);
	}

	if (real_size != size) {
		stor_printf("real size != size\n");
		err = EIO;
	}

	return (err);
}


static int
stor_opendev(struct open_dev **odp, struct uboot_devdesc *dev)
{
	struct device_info *di;
	struct open_dev *od;
	int err, h;

	h = stor_info[dev->d_unit];

	debugf("refcount=%d\n", stor_open_count);

	/*
	 * There can be recursive open calls from the infrastructure, but at
	 * U-Boot level open the device only the first time.
	 */
	if (stor_open_count > 0)
		stor_open_count++;
	else if ((err = ub_dev_open(h)) != 0) {
		stor_printf("device open failed with error=%d, handle=%d\n",
		    err, h);
		*odp = NULL;
		return (ENXIO);
	}

	if ((di = ub_dev_get(h)) == NULL)
		panic("could not retrieve U-Boot device_info, handle=%d", h);

	if ((od = malloc(sizeof(struct open_dev))) == NULL) {
		stor_printf("could not allocate memory for open_dev\n");
		return (ENOMEM);
	}
	od->od_bsize = di->di_stor.block_size;
	od->od_bstart = 0;
	od->od_type = 0;

	if ((err = stor_open_gpt(od, dev)) != 0)
		err = stor_open_bsdlabel(od, dev);

	if (err != 0)
		free(od);
	else {
		stor_open_count = 1;
		*odp = od;
	}

	return (err);
}

static int
stor_closedev(struct uboot_devdesc *dev)
{
	int err, h;

	free((struct open_dev *)dev->d_disk.data);
	dev->d_disk.data = NULL;

	if (--stor_open_count == 0) {
		h = stor_info[dev->d_unit];
		if ((err = ub_dev_close(h)) != 0) {
			stor_printf("device close failed with error=%d, "
			    "handle=%d\n", err, h);
			return (ENXIO);
		}
	}

	return (0);
}

/* Given a size in 512 byte sectors, convert it to a human-readable number. */
/* XXX stolen from sys/boot/i386/libi386/biosdisk.c, should really be shared */
static char *
display_size(uint64_t size)
{
	static char buf[80];
	char unit;

	size /= 2;
	unit = 'K';
	if (size >= 10485760000LL) {
		size /= 1073741824;
		unit = 'T';
	} else if (size >= 10240000) {
		size /= 1048576;
		unit = 'G';
	} else if (size >= 10000) {
		size /= 1024;
		unit = 'M';
	}
	sprintf(buf, "%.6ld%cB", (long)size, unit);
	return (buf);
}

static void
stor_print_bsdlabel(struct uboot_devdesc *dev, char *prefix, int verbose)
{
	char buf[512], line[80];
	struct disklabel *dl;
	uint32_t off, size;
	int err, i, t;

	/* Read disklabel */
	err = stor_readdev(dev, LABELSECTOR, 1, buf);
	if (err) {
		sprintf(line, "%s%d: disklabel read error=%d\n",
		    dev->d_dev->dv_name, dev->d_unit, err);
		pager_output(line);
		return;
	}
	dl = (struct disklabel *)buf;

	if (dl->d_magic != DISKMAGIC) {
		sprintf(line, "%s%d: no disklabel magic!\n",
		    dev->d_dev->dv_name, dev->d_unit);
		pager_output(line);
		return;
	}

	/* Print partitions info */
	for (i = 0; i < dl->d_npartitions; i++) {
		if ((t = dl->d_partitions[i].p_fstype) < FSMAXTYPES) {

			off = dl->d_partitions[i].p_offset;
			size = dl->d_partitions[i].p_size;
			if (fstypenames[t] == NULL || size == 0)
				continue;

			if ((('a' + i) == 'c') && (!verbose))
				continue;

			sprintf(line, "  %s%c: %s %s (%d - %d)\n", prefix,
			    'a' + i, fstypenames[t], display_size(size),
			    off, off + size);

			pager_output(line);
		}
	}
}

static void
stor_print_one(int i, struct device_info *di, int verbose)
{
	struct uboot_devdesc dev;
	struct open_dev *od;
	char line[80];

	sprintf(line, "\tdisk%d (%s)\n", i, ub_stor_type(di->type));
	pager_output(line);

	dev.d_dev = &uboot_storage;
	dev.d_unit = i;
	dev.d_disk.partition = -1;
	dev.d_disk.data = NULL;

	if (stor_opendev(&od, &dev) == 0) {
		dev.d_disk.data = od;

		if (od->od_type == OD_GPT) {
			/* TODO */

		} else if (od->od_type == OD_BSDLABEL) {
			sprintf(line, "\t\tdisk%d", i);
			stor_print_bsdlabel(&dev, line, verbose);
		}

		stor_closedev(&dev);
	}
}

static void
stor_print(int verbose)
{
	struct device_info *di;
	int i;

	for (i = 0; i < stor_info_no; i++) {
		di = ub_dev_get(stor_info[i]);
		if (di != NULL)
			stor_print_one(i, di, verbose);
	}
}
