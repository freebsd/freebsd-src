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
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <ufs/ffs/fs.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <dialog.h>

#include "mbr.h"
#include "bootarea.h"
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
	if (ioctl(fd, DIOCSDINFO, &avail_disklabels[inst_disk]) < 0 &&
	    errno != ENODEV && errno != ENOTTY) {
		sprintf(errmsg, "Failed to change in-core disklabel\n");
		return(-1);
	}

	if (lseek(fd, (offset * avail_disklabels[inst_disk].d_secsize), SEEK_SET) < 0) {
		sprintf(errmsg, "Couldn't seek to start of partition\n");
		return(-1);
	}

	if (enable_label(fd) == -1)
		return(-1);

	if (write(fd, bootblocks, bbsize) != bbsize) {
		sprintf(errmsg, "Failed to write bootblocks\n");
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
	sprintf(scratch, "\nLoading boot code from %s\n", boot1);
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

	/* Copy DOS partition area into bootblocks */

	bcopy(mbr->dospart, &bootblocks[DOSPARTOFF],
	      sizeof(struct dos_partition) * 4);

	dialog_clear();

	/* Write the disklabel into the bootblocks */

	label->d_checksum = dkcksum(label);
	bcopy(label, &bootblocks[(LABELSECTOR * label->d_secsize) + LABELOFFSET],
			sizeof *label);

	return(0);
}

/* Convert a size in Mb to a round number of cylinders */
int
Mb_to_cylbdry(int size, struct disklabel *label)
{
	int nsects, ncyls;

	nsects = (size * 1024 * 1024) / label->d_secsize;
	ncyls = nsects / label->d_secpercyl;
	nsects = ++ncyls * label->d_secpercyl;

	return(nsects);
}

void
default_disklabel(struct disklabel *label, int avail_sects, int offset)
{

	int nsects;

	/* Fill in default label entries */
	label->d_magic = DISKMAGIC;
	bcopy("INSTALLATION",label->d_typename, strlen("INSTALLATION"));
	label->d_rpm = 3600;
	label->d_interleave = 1;
	label->d_trackskew = 0;
	label->d_cylskew = 0;
	label->d_magic2 = DISKMAGIC;
	label->d_checksum = 0;
	label->d_bbsize = BBSIZE;
	label->d_sbsize = SBSIZE;
	label->d_npartitions = 5;

	/* Set up c and d as raw partitions for now */
	label->d_partitions[2].p_size = avail_sects;
	label->d_partitions[2].p_offset = offset;
	label->d_partitions[2].p_fsize = DEFFSIZE; /* XXX */
	label->d_partitions[2].p_fstype = FS_UNUSED;
	label->d_partitions[2].p_frag = DEFFRAG;

	label->d_partitions[3].p_size = label->d_secperunit;
	label->d_partitions[3].p_offset = 0;
	label->d_partitions[3].p_fsize = DEFFSIZE;
	label->d_partitions[3].p_fstype = FS_UNUSED;
	label->d_partitions[3].p_frag = DEFFRAG;

	/* Default root */
	nsects = Mb_to_cylbdry(DEFROOTSIZE, label);

	label->d_partitions[0].p_size = nsects;
	label->d_partitions[0].p_offset = offset;
	label->d_partitions[0].p_fsize = DEFFSIZE;
	label->d_partitions[0].p_fstype = FS_BSDFFS;
	label->d_partitions[0].p_frag = DEFFRAG;

	avail_sects -= nsects;
	offset += nsects;
	nsects = Mb_to_cylbdry(DEFSWAPSIZE, label);

	label->d_partitions[1].p_size = nsects;
	label->d_partitions[1].p_offset = offset;
	label->d_partitions[1].p_fsize = DEFFSIZE;
	label->d_partitions[1].p_fstype = FS_SWAP;
	label->d_partitions[1].p_frag = DEFFRAG;

	avail_sects -= nsects;
	offset += nsects;
	nsects = Mb_to_cylbdry(DEFUSRSIZE, label);

	if (avail_sects > nsects)
		nsects = avail_sects;

	label->d_partitions[4].p_size = nsects;
	label->d_partitions[4].p_offset = offset;
	label->d_partitions[4].p_fsize = DEFFSIZE;
	label->d_partitions[4].p_fstype = FS_BSDFFS;
	label->d_partitions[4].p_frag = DEFFRAG;

#ifdef notyet
	if (custom_install)
		customise_label()
#endif

}

int
disk_size(int disk)
{
	struct disklabel *label = avail_disklabels + disk;
	int size;

	size = label->d_secsize * label->d_nsectors
			 * label->d_ntracks * label->d_ncylinders;
	return(size/1024/1024);
}
