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
extern char boot1[];
extern char boot2[];

int
enable_label(int fd)
{ 
	int flag = 1;
	if (ioctl(fd, DIOCWLABEL, &flag) < 0) {
		return(-1);
	}
	return(0);
}

int
disable_label(int fd)
{  
	int flag = 0;
	if (ioctl(fd, DIOCWLABEL, &flag) < 0) {
		return(-1);
	}
	return(0);
}

int
write_bootblocks(int fd, off_t offset, int bbsize)
{
	if (ioctl(fd, DIOCWDINFO, &avail_disklabels[inst_disk]) < 0) {
		Fatal("Failed to write disklabel: %s\n", strerror(errno));
		return(-1);
	}
	return(0);

	if (lseek(fd, (offset * avail_disklabels[inst_disk].d_secsize), SEEK_SET) < 0) {
		sprintf(errmsg, "Couldn't seek to start of partition\n");
		return(-1);
	}

	if (enable_label(fd) == -1)
		return(-1);

	if (write(fd, bootblocks, bbsize) != bbsize) {
		sprintf(errmsg, "Failed to write bootblocks (%p) %d %s\n",
			bootblocks,
			errno, strerror(errno)
			);
		return(-1);
	}

	if (disable_label(fd) == -1)
		return(-1);

	return(0);
}

int
build_bootblocks(struct disklabel *label)
{

	int fd;

	sprintf(scratch, "\nLoading boot code from %s\n", boot1);
	dialog_msgbox(TITLE, scratch, 5, 60, 0);
	fd = open(boot1, O_RDONLY);
	if (fd < 0) {
		sprintf(errmsg, "Couldn't open boot file %s\n", boot1);
		return(-1);
	}

	if (read(fd, bootblocks, MBRSIZE) < 0) {
		sprintf(errmsg, "Couldn't read from boot file %s\n", boot1);
		return(-1); 
	}

	if (close(fd) == -1) {
		sprintf(errmsg, "Couldn't close boot file %s\n", boot1);
		return(-1);
	}

	dialog_clear();
	sprintf(scratch, "\nLoading boot code from %s\n", boot2);
	dialog_msgbox(TITLE, scratch, 5, 60, 0);

	fd = open(boot2, O_RDONLY);
	if (fd < 0) {
		sprintf(errmsg, "Couldn't open boot file %s\n", boot2);
		return(-1);
	}

	if (read(fd, &bootblocks[MBRSIZE],
				(int)(label->d_bbsize - MBRSIZE)) < 0) {
		sprintf(errmsg, "Couldn't read from boot file %s\n", boot2);
		return(-1);
	}

	if (close(fd) == -1) {
		sprintf(errmsg, "Couldn't close boot file %s\n", boot2);
		return(-1);
	}

	dialog_clear();

	/* Copy MBR partition area into bootblocks */

	bcopy(mbr->dospart, &bootblocks[DOSPARTOFF],
	      sizeof(struct dos_partition) * NDOSPART);

	/* Write the disklabel into the bootblocks */

	label->d_checksum = dkcksum(label);
	bcopy(label, &bootblocks[(LABELSECTOR * label->d_secsize) + LABELOFFSET],
			sizeof *label);

	return(0);
}
