/*
 * Copyright (c) 1994, Paul Richards.
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dialog.h>

#include "mbr.h"
#include "sysinstall.h"

extern char *bootblocks;
extern struct mbr *mbr;

int
write_bootblocks(int fd, struct disklabel *lbl)
{
    off_t of = lbl->d_partitions[OURPART].p_offset;

    Debug("Seeking to byte %ld ", of * lbl->d_secsize);
    if (lseek(fd, (of * lbl->d_secsize), SEEK_SET) < 0) {
	    Fatal("Couldn't seek to start of partition\n");
    }

    enable_label(fd);

    if (write(fd, bootblocks, lbl->d_bbsize) != lbl->d_bbsize) {
	    Fatal("Failed to write bootblocks (%p,%d) %d %s\n",
		    bootblocks, lbl->d_bbsize,
		    errno, strerror(errno)
		    );
    }

    disable_label(fd);

    return(0);
}

int
build_bootblocks(int dfd,struct disklabel *label,struct dos_partition *dospart)
{
    int fd;
    off_t of = label->d_partitions[OURPART].p_offset;

    Debug("Loading boot code from %s", boot1);

    fd = open(boot1, O_RDONLY);
    if (fd < 0) 
	Fatal("Couldn't open boot file %s\n", boot1);

    if (read(fd, bootblocks, MBRSIZE) < 0) 
	Fatal("Couldn't read from boot file %s\n", boot1);

    if (close(fd) == -1) 
	Fatal("Couldn't close boot file %s\n", boot1);

    Debug("Loading boot code from %s", boot2);

    fd = open(boot2, O_RDONLY);
    if (fd < 0) 
	Fatal("Couldn't open boot file %s", boot2);

    if (read(fd, &bootblocks[MBRSIZE], (int)(label->d_bbsize - MBRSIZE)) < 0) 
	Fatal("Couldn't read from boot file %s\n", boot2);

    if (close(fd) == -1) 
	Fatal("Couldn't close boot file %s", boot2);

    bcopy(dospart, &bootblocks[DOSPARTOFF],
	  sizeof(struct dos_partition) * NDOSPART);

    label->d_checksum = 0;
    label->d_checksum = dkcksum(label);
    bcopy(label, &bootblocks[(LABELSECTOR * label->d_secsize) + LABELOFFSET],
		    sizeof *label);

    Debug("Seeking to byte %ld ", of * label->d_secsize);

    if (lseek(dfd, (of * label->d_secsize), SEEK_SET) < 0) {
	    Fatal("Couldn't seek to start of partition\n");
    }

    enable_label(dfd);

    if (write(dfd, bootblocks, label->d_bbsize) != label->d_bbsize) {
	    Fatal("Failed to write bootblocks (%p,%d) %d %s\n",
		    bootblocks, label->d_bbsize,
		    errno, strerror(errno)
		    );
    }

    disable_label(dfd);

    return(0);
}
