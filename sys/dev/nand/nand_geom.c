/*-
 * Copyright (C) 2009-2012 Semihalf
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/bio.h>
#include <geom/geom.h>
#include <geom/geom_nand.h>

#include <dev/nand/nand.h>
#include <dev/nand/nandbus.h>
#include <dev/nand/nand_cdev.h>
#include "nand_if.h"
#include "nandbus_if.h"

#define	BIO_NAND_STD	((void *)1)
#define	BIO_NAND_RAW	((void *)2)

static gnand_ioctl_t nand_ioctl;
static gnand_strategy_t nand_strategy;
static gnand_strategy_t nand_strategy_raw;

static int
nand_read(struct nand_chip *chip, uint32_t offset, void *buf, uint32_t len)
{

	nand_debug(NDBG_GEOM, "Read from chip %d [%p] at %d", chip->num, chip,
	    offset);

	return (nand_read_pages(chip, offset, buf, len));
}

static int
nand_write(struct nand_chip *chip, uint32_t offset, void* buf, uint32_t len)
{

	nand_debug(NDBG_GEOM, "Write to chip %d [%p] at %d", chip->num, chip,
	    offset);

	return (nand_prog_pages(chip, offset, buf, len));
}

static int
nand_read_raw(struct nand_chip *chip, uint32_t offset, void *buf, uint32_t len)
{
	nand_debug(NDBG_GEOM, "Raw read from chip %d [%p] at %d", chip->num,
	    chip, offset);

	return (nand_read_pages_raw(chip, offset, buf, len));
}

static int
nand_write_raw(struct nand_chip *chip, uint32_t offset, void *buf, uint32_t len)
{

	nand_debug(NDBG_GEOM, "Raw write to chip %d [%p] at %d", chip->num,
	    chip, offset);

	return (nand_prog_pages_raw(chip, offset, buf, len));
}

static void
nand_strategy(struct bio *bp)
{
	struct nand_chip *chip;

	chip = (struct nand_chip *)bp->bio_nand->d_drv1;

	bp->bio_driver1 = BIO_NAND_STD;

	nand_debug(NDBG_GEOM, "Strategy %s on chip %d [%p]",
	    (bp->bio_cmd & BIO_READ) == BIO_READ ? "READ" :
	    ((bp->bio_cmd & BIO_WRITE) == BIO_WRITE ? "WRITE" :
	    ((bp->bio_cmd & BIO_DELETE) == BIO_DELETE ? "DELETE" : "UNKNOWN")),
	     chip->num, chip);

	mtx_lock(&chip->qlock);
	bioq_insert_tail(&chip->bioq, bp);
	mtx_unlock(&chip->qlock);
	taskqueue_enqueue(chip->tq, &chip->iotask);
}

static void
nand_strategy_raw(struct bio *bp)
{
	struct nand_chip *chip;

	chip = (struct nand_chip *)bp->bio_nand->d_drv1;

	/* Inform taskqueue that it's a raw access */
	bp->bio_driver1 = BIO_NAND_RAW;

	nand_debug(NDBG_GEOM, "Strategy %s on chip %d [%p]",
	    (bp->bio_cmd & BIO_READ) == BIO_READ ? "READ" :
	    ((bp->bio_cmd & BIO_WRITE) == BIO_WRITE ? "WRITE" :
	    ((bp->bio_cmd & BIO_DELETE) == BIO_DELETE ? "DELETE" : "UNKNOWN")),
	     chip->num, chip);

	mtx_lock(&chip->qlock);
	bioq_insert_tail(&chip->bioq, bp);
	mtx_unlock(&chip->qlock);
	taskqueue_enqueue(chip->tq, &chip->iotask);
}

static int
nand_oob_access(struct nand_chip *chip, uint32_t page, uint32_t offset,
    uint32_t len, uint8_t *data, uint8_t write)
{
	struct chip_geom *cg;
	int ret = 0;

	cg = &chip->chip_geom;

	if (!write)
		ret = nand_read_oob(chip, page, data, cg->oob_size);
	else
		ret = nand_prog_oob(chip, page, data, cg->oob_size);

	return (ret);
}

static int
nand_ioctl(struct gnand *disk, u_long cmd, void *data, int fflag,
    struct thread *td)
{
	struct nand_chip *chip;
	struct nand_oob_rw *oob_rw = NULL;
	struct nand_raw_rw *raw_rw = NULL;
	device_t nandbus;
	uint8_t *buf = NULL;
	int ret = 0;
	uint8_t status;

	chip = (struct nand_chip *)disk->d_drv1;
	nandbus = device_get_parent(chip->dev);

	if ((cmd == NAND_IO_RAW_READ) || (cmd == NAND_IO_RAW_PROG)) {
		raw_rw = (struct nand_raw_rw *)data;
		buf = malloc(raw_rw->len, M_NAND, M_WAITOK);
		if (!buf)
			return (ENOMEM);
	}
	switch (cmd) {
	case NAND_IO_ERASE:
		ret = nand_erase_blocks(chip, ((off_t *)data)[0],
		    ((off_t *)data)[1]);
		break;

	case NAND_IO_OOB_READ:
		oob_rw = (struct nand_oob_rw *)data;
		ret = nand_oob_access(chip, oob_rw->page, 0,
		    oob_rw->len, oob_rw->data, 0);
		break;

	case NAND_IO_OOB_PROG:
		oob_rw = (struct nand_oob_rw *)data;
		ret = nand_oob_access(chip, oob_rw->page, 0,
		    oob_rw->len, oob_rw->data, 1);
		break;

	case NAND_IO_GET_STATUS:
		NANDBUS_LOCK(nandbus);
		ret = NANDBUS_GET_STATUS(nandbus, &status);
		if (ret == 0)
			*(uint8_t *)data = status;
		NANDBUS_UNLOCK(nandbus);
		break;

	case NAND_IO_RAW_PROG:
		copyin(oob_rw->data, buf, oob_rw->len);
		ret = nand_prog_pages_raw(chip, raw_rw->off, buf,
		    raw_rw->len);
		break;

	case NAND_IO_RAW_READ:
		ret = nand_read_pages_raw(chip, raw_rw->off, buf,
		    raw_rw->len);
		copyout(buf, raw_rw->data, raw_rw->len);
		break;

	case NAND_IO_GET_CHIP_PARAM:
		nand_get_chip_param(chip, (struct chip_param_io *)data);
		break;

	default:
		printf("Unknown nand_ioctl request \n");
		ret = EIO;
	}

	if (buf)
		free(buf, M_NAND);

	return (ret);
}

static void
nand_io_proc(void *arg, int pending)
{
	struct nand_chip *chip = arg;
	struct bio *bp;
	int err = 0;

	for (;;) {
		mtx_lock(&chip->qlock);
		bp = bioq_takefirst(&chip->bioq);
		mtx_unlock(&chip->qlock);
		if (bp == NULL)
			break;

		if (bp->bio_driver1 == BIO_NAND_STD) {
			if ((bp->bio_cmd & BIO_READ) == BIO_READ) {
				err = nand_read(chip,
				    bp->bio_offset & 0xffffffff,
				    bp->bio_data, bp->bio_bcount);
			} else if ((bp->bio_cmd & BIO_WRITE) == BIO_WRITE) {
				err = nand_write(chip,
				    bp->bio_offset & 0xffffffff,
				    bp->bio_data, bp->bio_bcount);
			}
		} else if (bp->bio_driver1 == BIO_NAND_RAW) {
			if ((bp->bio_cmd & BIO_READ) == BIO_READ) {
				err = nand_read_raw(chip,
				    bp->bio_offset & 0xffffffff,
				    bp->bio_data, bp->bio_bcount);
			} else if ((bp->bio_cmd & BIO_WRITE) == BIO_WRITE) {
				err = nand_write_raw(chip,
				    bp->bio_offset & 0xffffffff,
				    bp->bio_data, bp->bio_bcount);
			}
		} else
			panic("Unknown access type in bio->bio_driver1\n");

		if ((bp->bio_cmd & BIO_READOOB) == BIO_READOOB) {
			err = nand_oob_access(chip, bp->bio_offset /
			    chip->chip_geom.page_size, 0,
			    bp->bio_bcount, bp->bio_data, 0);
		}

		if ((bp->bio_cmd & BIO_WRITEOOB) == BIO_WRITEOOB) {
			err = nand_oob_access(chip, bp->bio_offset /
			    chip->chip_geom.page_size, 0,
			    bp->bio_bcount, bp->bio_data, 1);
		}

		if ((bp->bio_cmd & BIO_DELETE) == BIO_DELETE) {
			nand_debug(NDBG_GEOM, "Delete on chip%d offset %lld "
			    "length %ld\n", chip->num, bp->bio_offset,
			    bp->bio_bcount);
			err = nand_erase_blocks(chip,
			    bp->bio_offset & 0xffffffff,
			    bp->bio_bcount);
		}

		if (err == ECC_CORRECTABLE)
			bp->bio_flags |= BIO_ECC;

		if (err == 0 || err == ECC_CORRECTABLE)
			bp->bio_resid = 0;
		else {
			nand_debug(NDBG_GEOM,"nand_[read|write|erase_blocks] "
			    "error: %d\n", err);

			bp->bio_error = EIO;
			bp->bio_flags |= BIO_ERROR;
			bp->bio_resid = bp->bio_bcount;
		}
		biodone(bp);
	}
}

int
create_geom_disk(struct nand_chip *chip)
{
	struct gnand *disk, *rdisk;
	device_t dev;

	dev = device_get_parent(chip->dev);
	dev = device_get_parent(dev);

	/* Create the disk device */
	disk = gnand_alloc();
	disk->d_strategy = nand_strategy;
	disk->d_ioctl = nand_ioctl;
	disk->d_name = "gnand";
	disk->d_drv1 = chip;
	disk->d_dev = dev;
	disk->d_maxsize = chip->chip_geom.block_size;
	disk->d_sectorsize = chip->chip_geom.page_size;
	disk->d_mediasize = chip->chip_geom.chip_size;
	disk->d_pagesize = chip->chip_geom.page_size;
	disk->d_oobsize = chip->chip_geom.oob_size;
	disk->d_unit = chip->num + 
	    10 * device_get_unit(device_get_parent(chip->dev));

	/* 
	 * When using BBT, make two last blocks of device unavailable
	 * to user (because those are used to store BBT table).
	 */
	if (chip->bbt != NULL)
		disk->d_mediasize -= (2 * chip->chip_geom.block_size);

	disk->d_flags = DISKFLAG_CANDELETE;

	snprintf(disk->d_ident, sizeof(disk->d_ident),
	    "nand: Man:0x%02x Dev:0x%02x", chip->id.man_id, chip->id.dev_id);

	gnand_create(disk);

	/* Create the RAW disk device */
	rdisk = gnand_alloc();
	rdisk->d_strategy = nand_strategy_raw;
	rdisk->d_ioctl = nand_ioctl;
	rdisk->d_name = "gnand.raw";
	rdisk->d_drv1 = chip;
	rdisk->d_dev = dev;
	rdisk->d_maxsize = chip->chip_geom.block_size;
	rdisk->d_sectorsize = chip->chip_geom.page_size;
	rdisk->d_mediasize = chip->chip_geom.chip_size;
	rdisk->d_pagesize = chip->chip_geom.page_size;
	rdisk->d_oobsize = chip->chip_geom.oob_size;
	rdisk->d_unit = chip->num + 
	    10 * device_get_unit(device_get_parent(chip->dev));

	rdisk->d_flags = DISKFLAG_CANDELETE;

	snprintf(rdisk->d_ident, sizeof(disk->d_ident),
	    "nand_raw: Man:0x%02x Dev:0x%02x", chip->id.man_id,
	    chip->id.dev_id);

	gnand_create(rdisk);

	chip->disk = disk;
	chip->rdisk = rdisk;

	mtx_init(&chip->qlock, "NAND I/O lock", NULL, MTX_DEF);
	bioq_init(&chip->bioq);

	TASK_INIT(&chip->iotask, 0, nand_io_proc, chip);
	chip->tq = taskqueue_create("nand_taskq", M_WAITOK,
	    taskqueue_thread_enqueue, &chip->tq);
	taskqueue_start_threads(&chip->tq, 1, PI_DISK, "nand taskq");

	device_printf(chip->dev, "Created gnand%d for chip [0x%0x, 0x%0x]\n",
	    disk->d_unit, chip->id.man_id, chip->id.dev_id);

	return (0);
}

void
destroy_geom_disk(struct nand_chip *chip)
{
	struct bio *bp;

	taskqueue_free(chip->tq);
	gnand_destroy(chip->disk);
	gnand_destroy(chip->rdisk);

	mtx_lock(&chip->qlock);
	for (;;) {
		bp = bioq_takefirst(&chip->bioq);
		if (bp == NULL)
			break;
		bp->bio_error = EIO;
		bp->bio_flags |= BIO_ERROR;
		bp->bio_resid = bp->bio_bcount;

		biodone(bp);
	}
	mtx_unlock(&chip->qlock);

	mtx_destroy(&chip->qlock);
}
