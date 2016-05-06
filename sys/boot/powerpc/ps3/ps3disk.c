/*-
 * Copyright (C) 2008 Semihalf, Rafal Jaworowski
 * Copyright (C) 2009 Semihalf, Piotr Ziecik
 * Copyright (C) 2011 glevand (geoffrey.levand@mail.ru)
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

#include <sys/endian.h>
#include <machine/stdarg.h>
#include <stand.h>
#include <uuid.h>

#define FSTYPENAMES
#include <sys/disklabel.h>
#include <sys/diskmbr.h>
#include <sys/gpt.h>

#include "bootstrap.h"
#include "ps3bus.h"
#include "ps3devdesc.h"
#include "ps3stor.h"

#define dev_printf(dev, fmt, args...)	\
	printf("%s%d: " fmt "\n" , dev->d_dev->dv_name, dev->d_unit, ##args)

#ifdef DISK_DEBUG
#define DEBUG(fmt, args...)		printf("%s:%d: " fmt "\n" , __func__ , __LINE__, ##args)
#else
#define DEBUG(fmt, args...)
#endif

struct open_dev;

static int ps3disk_open_gpt(struct ps3_devdesc *dev, struct open_dev *od);
static void ps3disk_uuid_letoh(uuid_t *uuid);

static int ps3disk_init(void);
static int ps3disk_strategy(void *devdata, int flag, daddr_t dblk,
	size_t offset, size_t size, char *buf, size_t *rsize);
static int ps3disk_open(struct open_file *f, ...);
static int ps3disk_close(struct open_file *f);
static void ps3disk_print(int verbose);

struct devsw ps3disk = {
	"disk",
	DEVT_DISK,
	ps3disk_init,
	ps3disk_strategy,
	ps3disk_open,
	ps3disk_close,
	noioctl,
	ps3disk_print,
};

struct gpt_part {
	int		gp_index;
	uuid_t		gp_type;
	uint64_t	gp_start;
	uint64_t	gp_end;
};

struct open_dev {
	uint64_t od_start;

	union {
		struct {
			int 		nparts;
			struct gpt_part	*parts;
		} gpt;
	} od_kind;
};

#define od_gpt_nparts	od_kind.gpt.nparts
#define od_gpt_parts	od_kind.gpt.parts

static struct ps3_stordev stor_dev;

static int ps3disk_init(void)
{
	int err;

	err = ps3stor_setup(&stor_dev, PS3_DEV_TYPE_STOR_DISK);
	if (err)
		return err;

	return 0;
}

static int ps3disk_strategy(void *devdata, int flag, daddr_t dblk,
    size_t offset, size_t size, char *buf, size_t *rsize)
{
	struct ps3_devdesc *dev = (struct ps3_devdesc *) devdata;
	struct open_dev *od = (struct open_dev *) dev->d_disk.data;
	int err;

	if (flag != F_READ) {
		dev_printf(dev, "write operation is not supported!\n");
		return EROFS;
	}

	if (size % stor_dev.sd_blksize) {
		dev_printf(dev, "size=%u is not multiple of device block size=%llu\n",
			size, stor_dev.sd_blksize);
		return EIO;
	}

	if (rsize)
		*rsize = 0;

	err = ps3stor_read_sectors(&stor_dev, dev->d_unit, od->od_start + dblk,
		size / stor_dev.sd_blksize,  0, buf);

	if (!err && rsize)
		*rsize = size;

	if (err)
		dev_printf(dev, "read operation failed dblk=%llu size=%d err=%d\n",
			dblk, size, err);

	return err;
}

static int ps3disk_open(struct open_file *f, ...)
{
	va_list ap;
	struct ps3_devdesc *dev;
	struct open_dev *od;
	int err;

	va_start(ap, f);
	dev = va_arg(ap, struct ps3_devdesc *);
	va_end(ap);

	od = malloc(sizeof(struct open_dev));
	if (!od) {
		dev_printf(dev, "couldn't allocate memory for new open_dev\n");
		return ENOMEM;
	}

	err = ps3disk_open_gpt(dev, od);

	if (err) {
		dev_printf(dev, "couldn't open GPT disk error=%d\n", err);
		free(od);
	} else {
		((struct ps3_devdesc *) (f->f_devdata))->d_disk.data = od;
	}

	return err;
}

static int ps3disk_close(struct open_file *f)
{
	struct ps3_devdesc *dev = f->f_devdata;
	struct open_dev *od = dev->d_disk.data;

	if (dev->d_disk.ptype == PTYPE_GPT && od->od_gpt_nparts)
		free(od->od_gpt_parts);

	free(od);

	dev->d_disk.data = NULL;

	return 0;
}

static void ps3disk_print(int verbose)
{
}

static int ps3disk_open_gpt(struct ps3_devdesc *dev, struct open_dev *od)
{
	char buf[512];
	struct gpt_hdr *hdr;
	struct gpt_ent *ent;
	daddr_t slba, elba, lba;
	int nparts, eps, i, part, err;

	od->od_gpt_nparts = 0;
	od->od_gpt_parts = NULL;

	err = ps3stor_read_sectors(&stor_dev, dev->d_unit, 0, 1, 0, buf);
	if (err) {
		err = EIO;
		goto out;
	}

	if (le16toh(*((uint16_t *) (buf + DOSMAGICOFFSET))) != DOSMAGIC) {
		err = ENXIO;
		goto out;
	}

	err = ps3stor_read_sectors(&stor_dev, dev->d_unit, 1, 1, 0, buf);
	if (err) {
		err = EIO;
		goto out;
	}

	hdr = (struct gpt_hdr *) buf;

	if (bcmp(hdr->hdr_sig, GPT_HDR_SIG, sizeof(hdr->hdr_sig)) ||
		le64toh(hdr->hdr_lba_self) != 1 || le32toh(hdr->hdr_revision) < 0x00010000 ||
		le32toh(hdr->hdr_entsz) < sizeof(struct gpt_ent) ||
		stor_dev.sd_blksize % le32toh(hdr->hdr_entsz) != 0) {
		err = ENXIO;
		goto out;
	}

	nparts = 0;
	eps = stor_dev.sd_blksize / le32toh(hdr->hdr_entsz);
	slba = le64toh(hdr->hdr_lba_table);
	elba = slba + le32toh(hdr->hdr_entries) / eps;

	for (lba = slba; lba < elba; lba++) {
		err = ps3stor_read_sectors(&stor_dev, dev->d_unit, lba, 1, 0, buf);
		if (err) {
			err = EIO;
			goto out;
		}

		ent = (struct gpt_ent *) buf;

		for (i = 0; i < eps; i++) {
			if (uuid_is_nil(&ent[i].ent_type, NULL) ||
				le64toh(ent[i].ent_lba_start) == 0 ||
				le64toh(ent[i].ent_lba_end) < le64toh(ent[i].ent_lba_start))
				continue;

			nparts++;
		}
	}

	if (nparts) {
		od->od_gpt_nparts = nparts;

		od->od_gpt_parts = malloc(nparts * sizeof(struct gpt_part));
		if (!od->od_gpt_parts) {
			err = ENOMEM;
			goto out;
		}

		for (lba = slba, part = 0; lba < elba; lba++) {
			err = ps3stor_read_sectors(&stor_dev, dev->d_unit, lba, 1, 0, buf);
			if (err) {
				err = EIO;
				goto out;
			}

			ent = (struct gpt_ent *) buf;

			for (i = 0; i < eps; i++) {
				if (uuid_is_nil(&ent[i].ent_type, NULL) ||
					le64toh(ent[i].ent_lba_start) == 0 ||
					le64toh(ent[i].ent_lba_end) < le64toh(ent[i].ent_lba_start))
					continue;

				od->od_gpt_parts[part].gp_index = (lba - slba) * eps + i + 1;
				od->od_gpt_parts[part].gp_type = ent[i].ent_type;
				od->od_gpt_parts[part].gp_start = le64toh(ent[i].ent_lba_start);
				od->od_gpt_parts[part].gp_end = le64toh(ent[i].ent_lba_end);
				ps3disk_uuid_letoh(&od->od_gpt_parts[part].gp_type);
				part++;
			}
		}
	}

	dev->d_disk.ptype = PTYPE_GPT;

	if (od->od_gpt_nparts && !dev->d_disk.pnum)
		dev->d_disk.pnum = od->od_gpt_parts[0].gp_index;

	for (i = 0; i < od->od_gpt_nparts; i++)
		if (od->od_gpt_parts[i].gp_index == dev->d_disk.pnum)
			od->od_start = od->od_gpt_parts[i].gp_start;

	err = 0;

out:

	if (err && od->od_gpt_parts)
		free(od->od_gpt_parts);

	return err;
}

static void ps3disk_uuid_letoh(uuid_t *uuid)
{
	uuid->time_low = le32toh(uuid->time_low);
	uuid->time_mid = le16toh(uuid->time_mid);
	uuid->time_hi_and_version = le16toh(uuid->time_hi_and_version);
}
