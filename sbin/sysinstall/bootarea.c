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

#include <fstab.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dialog.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <ufs/ffs/fs.h>

#include "disk.h"
#include "sysinstall.h"

char boot1[] = BOOT1;
char boot2[] = BOOT2;

int
enable_label(int fd)
{ 
	int flag = 1;
	if (ioctl(fd, DIOCWLABEL, &flag) < 0) 
	    return (-1);
	return (0);
}

int
disable_label(int fd)
{  
	int flag = 0;
	if (ioctl(fd, DIOCWLABEL, &flag) < 0) 
	    return (-1);
	return (0);
}

int
write_bootblocks(int disk)
{
	int fd;
	off_t offset;
	int part = disk_list[disk].inst_part;
	struct disklabel *lbl = &disk_list[disk].lbl;
	unsigned char bootblocks[BBSIZE];

	/* Load MBR boot code */

	if ((fd = open(boot1, O_RDONLY)) == -1) {
		sprintf(errmsg, "Couldn't open boot file %s for bootblocks\n%s\n",
		        boot1, strerror(errno));
		return (-1);
	}

	if (read(fd, bootblocks, MBRSIZE) < 0) {
		sprintf(errmsg, "Couldn't load boot file %s into bootblocks\n%s\n",
				  boot1, strerror(errno));
		return (-1);
	}

	if (close(fd) == -1) {
		sprintf(errmsg, "Couldn't close boot file %s\n%s\n", boot1,
				  strerror(errno));
		return (-1);
	}

	/* Load second level boot code */

	if ((fd = open(boot2, O_RDONLY)) == -1) {
		sprintf(errmsg, "Couldn't open boot file %s for bootblocks\n%s\n",
		        boot2, strerror(errno));
		return (-1);
	}

	if (read(fd, &bootblocks[MBRSIZE], (int)(lbl->d_bbsize - MBRSIZE)) < 0) {
		sprintf(errmsg, "Couldn't load boot file %s into bootblocks\n%s\n",
				  boot2, strerror(errno));
		return (-1);
	}

	if (close(fd) == -1) {
		sprintf(errmsg, "Couldn't close boot file %s\n%s\n", boot2,
				  strerror(errno));
		return (-1);
	}


	/* Copy the current MBR table into the boot blocks */

	bcopy(&disk_list[disk].mbr.dospart, &bootblocks[DOSPARTOFF],
			sizeof(struct dos_partition) * NDOSPART);

	/* Set checksum */
	lbl->d_checksum = 0;
	lbl->d_checksum = dkcksum(lbl);

	/* Copy disklabel into bootblocks */

	bcopy(lbl, &bootblocks[(LABELSECTOR * lbl->d_secsize) + LABELOFFSET],
			sizeof *lbl);

	/* Calculate offset to start of MBR partition we're installing to */

	offset = disk_list[disk].mbr.dospart[part].dp_start;
	offset *= lbl->d_secsize;

	/* Write the boot blocks out to the raw disk */

	if ((fd = open(diskname(disk), O_RDWR)) == -1) {
		sprintf(errmsg, "Couldn't open %s to write bootblocks\n%s\n",
				  scratch,strerror(errno));
		return (-1);
	}

	if (lseek(fd, offset, SEEK_SET) < 0) {
		sprintf(errmsg, "Couldn't seek to bootblocks area %s\n%s\n",
				  scratch, strerror(errno));
		return (-1);
	}

	if (enable_label(fd) == -1)
		return (-1);

	if (write(fd, bootblocks, lbl->d_bbsize) != lbl->d_bbsize) {
		sprintf(errmsg, "Failed to write out bootblocks to %s\n%s\n",
				  scratch, strerror(errno));
		return (-1);
	}

	if (disable_label(fd) == -1)
		return (-1);

   return(0);
}
