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

#include "bootarea.h"
#include "sysinstall.h"

char boot1[] = BOOT1;
char boot2[] = BOOT2;

struct part_type part_types[] =
{
    {0x00, "unused"} 
   ,{0x01, "Primary DOS with 12 bit FAT"}
   ,{0x02, "XENIX / filesystem"}
   ,{0x03, "XENIX /usr filesystem"} 
   ,{0x04, "Primary DOS with 16 bit FAT"}
   ,{0x05, "Extended DOS"}
   ,{0x06, "Primary 'big' DOS (> 32MB)"}
   ,{0x07, "OS/2 HPFS, QNX or Advanced UNIX"}
   ,{0x08, "AIX filesystem"}
   ,{0x09, "AIX boot partition or Coherent"}
   ,{0x0A, "OS/2 Boot Manager or OPUS"}
   ,{0x10, "OPUS"}
   ,{0x40, "VENIX 286"}
   ,{0x50, "DM"}
   ,{0x51, "DM"}
   ,{0x52, "CP/M or Microport SysV/AT"}
   ,{0x56, "GB"}
   ,{0x61, "Speed"}
   ,{0x63, "ISC UNIX, other System V/386, GNU HURD or Mach"}
   ,{0x64, "Novell Netware 2.xx"}
   ,{0x65, "Novell Netware 3.xx"}
   ,{0x75, "PCIX"}
   ,{0x80, "Minix 1.1 ... 1.4a"}
   ,{0x81, "Minix 1.4b ... 1.5.10"}
   ,{0x82, "Linux"}
   ,{0x93, "Amoeba filesystem"}
   ,{0x94, "Amoeba bad block table"}
   ,{0xA5, "386BSD"}
   ,{0xB7, "BSDI BSD/386 filesystem"}
   ,{0xB8, "BSDI BSD/386 swap"}
   ,{0xDB, "Concurrent CPM or C.DOS or CTOS"}
   ,{0xE1, "Speed"}
   ,{0xE3, "Speed"}
   ,{0xE4, "Speed"}
   ,{0xF1, "Speed"}
   ,{0xF2, "DOS 3.3+ Secondary"}
   ,{0xF4, "Speed"}
   ,{0xFF, "BBT (Bad Blocks Table)"}
};

extern char *bootblocks;
extern struct bootarea *bootarea;

int
enable_label(int fd)
{
	int flag = 1;
	if (ioctl(fd, DIOCWLABEL, &flag) < 0) {
		sprintf(errmsg, "Write enable of disk failed\n");
		return(-1);
	}
	return(0);
}

int
disable_label(int fd)
{
	int flag = 0;
	if (ioctl(fd, DIOCWLABEL, &flag) < 0) {
		sprintf(errmsg, "Write disable of disk failed\n");
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

	fd = open(boot1, O_RDONLY);
	if (fd < 0) {
		sprintf(errmsg, "Couldn't open boot file %s\n", boot1);
		return(-1);
	}

	if (read(fd, bootblocks, (int)label->d_secsize) < 0) {
		sprintf(errmsg, "Couldn't read from boot file %s\n", boot1);
		return(-1);
	}

	if (close(fd) == -1) {
		sprintf(errmsg, "Couldn't close boot file %s\n", boot1);
		return(-1);
	}

	fd = open(boot2, O_RDONLY);
	if (fd < 0) {
		sprintf(errmsg, "Couldn't open boot file %s\n", boot2);
		return(-1);
	}

	if (read(fd, &bootblocks[label->d_secsize], (int)(label->d_bbsize - label->d_secsize)) < 0) {
		sprintf(errmsg, "Couldn't read from boot file %s\n", boot2);
		return(-1);
	}

	if (close(fd) == -1) {
		sprintf(errmsg, "Couldn't close boot file %s\n", boot2);
		return(-1);
	}

	/* Write the disklabel into the bootblocks */

	label->d_checksum = dkcksum(label);
	bcopy(label, &bootblocks[(LABELSECTOR * label->d_secsize) + LABELOFFSET], sizeof *label);

	return(0);
}

int
calc_sects(int size, struct disklabel *label)
{
	int nsects, ncyls;

	nsects = (size * 1024 * 1024) / label->d_secsize;
	ncyls = nsects / label->d_secpercyl;
	nsects = ++ncyls * label->d_secpercyl;

	return(nsects);
}

void
build_disklabel(struct disklabel *label, int avail_sects, int offset)
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
	nsects = calc_sects(DEFROOTSIZE, label);

	label->d_partitions[0].p_size = nsects;
	label->d_partitions[0].p_offset = offset;
	label->d_partitions[0].p_fsize = DEFFSIZE;
	label->d_partitions[0].p_fstype = FS_BSDFFS;
	label->d_partitions[0].p_frag = DEFFRAG;

	avail_sects -= nsects;
	offset += nsects;
	nsects = calc_sects(DEFSWAPSIZE, label);

	label->d_partitions[1].p_size = nsects;
	label->d_partitions[1].p_offset = offset;
	label->d_partitions[1].p_fsize = DEFFSIZE;
	label->d_partitions[1].p_fstype = FS_SWAP;
	label->d_partitions[1].p_frag = DEFFRAG;

	avail_sects -= nsects;
	offset += nsects;
	nsects = calc_sects(DEFUSRSIZE, label);

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

char *
part_type(int type)
{
	int num_types = (sizeof(part_types)/sizeof(struct part_type));
	int next_type = 0;
	struct part_type *ptr = part_types;

	while (next_type < num_types) {
		if(ptr->type == type)
			return(ptr->name);
		ptr++;
		next_type++;
	}
	return("Uknown");
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

int
read_bootarea(int fd)
{
	if (lseek(fd, 0, SEEK_SET) == -1) {
		sprintf(errmsg, "Couldn't seek for bootarea read\n");
		return(-1);
	}
	if (read(fd, &(bootarea->bootcode), 512) == -1) {
		sprintf(errmsg, "Failed to read bootarea\n");
		return(-1);
	}
	/* Validate the bootarea */
	/* XXX -- need to validate contents too */
	if (bootarea->signature != BOOT_MAGIC) {
		sprintf(errmsg, "Bootarea invalid\n");
		return(-1);
	}
	return(0);
}

int
write_bootarea(int fd)
{
	if (lseek(fd, 0, SEEK_SET) == -1) {
		sprintf(errmsg, "Couldn't seek for bootarea write\n");
		return(-1);
	}

	if (enable_label(fd) == -1)
		return(-1);

	if (write(fd, bootarea, sizeof(bootarea)) == -1) {
		sprintf(errmsg, "Failed to write bootarea\n");
		return(-1);
	}

	if (disable_label(fd) == -1)
		return(-1);

	return(0);
}
