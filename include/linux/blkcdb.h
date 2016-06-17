/*
 * 2.5 Command Descriptor Block (CDB) Block Pre-Handler.
 *
 * Copyright (C) 2001 Andre Hedrick <andre@linux-ide.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of

 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public Licens
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-
 */

#ifndef _LINUX_BLKCDB_H
#define _LINUX_BLKCDB_H

typedef struct cdb_list {
#if 0
        unsigned char cdb_0;
        unsigned char cdb_1;
        unsigned char cdb_2;
        unsigned char cdb_3;
        unsigned char cdb_4;
        unsigned char cdb_5;
        unsigned char cdb_6;
        unsigned char cdb_7;
        unsigned char cdb_8;
        unsigned char cdb_9;
        unsigned char cdb_10;
        unsigned char cdb_11;
        unsigned char cdb_12;
        unsigned char cdb_13;
        unsigned char cdb_14;
        unsigned char cdb_15;
#else
        unsigned char cdb_regs[16];
#endif
} cdb_list_t;

#if 0

typedef cdb_list_t * (queue_proc) (kdev_t dev);

request_queue_t *ide_get_queue (kdev_t dev)
{
        ide_hwif_t *hwif = (ide_hwif_t *)blk_dev[MAJOR(dev)].data;

        return &hwif->drives[DEVICE_NR(dev) & 1].queue;
}

static request_queue_t *sd_find_queue(kdev_t dev)
{
        Scsi_Disk *dpnt;
        int target;
        target = DEVICE_NR(dev);

        dpnt = &rscsi_disks[target];
        if (!dpnt)
                return NULL;    /* No such device */
        return &dpnt->device->request_queue;
}

prebuilder:             NULL,
block_device_operations
struct block_device {

void do_ide_request(request_queue_t *q)

ide_do_request

typedef cdb_list_t (request_cdb_proc) (request_queue_t *q);

typedef cdb_list_t (request_cdb_proc) (request_queue_t *q);
typedef void (request_fn_proc) (request_queue_t *q);

srb

switch (SCpnt->request.cmd)
SCpnt->cmnd[0] = WRITE_6/READ_6;
SCpnt->cmnd[1] = (SCpnt->device->scsi_level <= SCSI_2) ?
			((SCpnt->lun << 5) & 0xe0) : 0;
SCpnt->cmnd[2] = (unsigned char) (block >> 24) & 0xff;
SCpnt->cmnd[3] = (unsigned char) (block >> 16) & 0xff;
SCpnt->cmnd[4] = (unsigned char) (block >> 8) & 0xff;
SCpnt->cmnd[5] = (unsigned char) block & 0xff;
SCpnt->cmnd[6] = 0;
SCpnt->cmnd[7] = (unsigned char) (this_count >> 8) & 0xff;
SCpnt->cmnd[8] = (unsigned char) this_count & 0xff;
SCpnt->cmnd[9] = 0;

#endif

#endif /* _LINUX_BLKCDB_H */

