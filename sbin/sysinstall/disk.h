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

#define MBRSIZE 512
#define MBR_MAGIC 0xAA55
#define ACTIVE 0x80

/* XXX -- calculate these, this is nasty */
#define DEFFSIZE 1024
#define DEFFRAG 8

/* bootarea.c */
int write_bootblocks(int);
int enable_label(int);
int disable_label(int);
/* label.c */
char *diskname(int);

struct mbr
{
	unsigned char padding[2];
	unsigned char bootcode[DOSPARTOFF];
	struct dos_partition dospart[4];
	unsigned short magic;
};

struct disk {
	struct disklabel lbl;
	struct mbr mbr;
	struct devconf *devconf;
	int selected;
	int inst_part;
	struct fstab mounts[MAXPARTITIONS];
};

extern struct disk disk_list[];

struct part_type 
{
 unsigned char type;
 char *name;
};

#define PARTITION_TYPES \
{ \
    {0x00, "Unused"} \
   ,{0x01, "Primary DOS with 12 bit FAT"} \
   ,{0x02, "XENIX / filesystem"} \
   ,{0x03, "XENIX /usr filesystem"}  \
   ,{0x04, "Primary DOS with 16 bit FAT"} \
   ,{0x05, "Extended DOS"} \
   ,{0x06, "Primary 'big' DOS (> 32MB)"} \
   ,{0x07, "OS/2 HPFS, QNX or Advanced UNIX"} \
   ,{0x08, "AIX filesystem"} \
   ,{0x09, "AIX boot partition or Coherent"} \
   ,{0x0A, "OS/2 Boot Manager or OPUS"} \
   ,{0x10, "OPUS"} \
   ,{0x40, "VENIX 286"} \
   ,{0x50, "DM"} \
   ,{0x51, "DM"} \
   ,{0x52, "CP/M or Microport SysV/AT"} \
   ,{0x56, "GB"} \
   ,{0x61, "Speed"} \
   ,{0x63, "ISC UNIX, other System V/386, GNU HURD or Mach"} \
   ,{0x64, "Novell Netware 2.xx"} \
   ,{0x65, "Novell Netware 3.xx"} \
   ,{0x75, "PCIX"} \
   ,{0x80, "Minix 1.1 ... 1.4a"} \
   ,{0x81, "Minix 1.4b ... 1.5.10"} \
   ,{0x82, "Linux"} \
   ,{0x93, "Amoeba filesystem"} \
   ,{0x94, "Amoeba bad block table"} \
   ,{0xA5, "FreeBSD/NetBSD/386BSD"} \
   ,{0xA7, "NEXTSTEP"} \
   ,{0xB7, "BSDI BSD/386 filesystem"} \
   ,{0xB8, "BSDI BSD/386 swap"} \
   ,{0xDB, "Concurrent CPM or C.DOS or CTOS"} \
   ,{0xE1, "Speed"} \
   ,{0xE3, "Speed"} \
   ,{0xE4, "Speed"} \
   ,{0xF1, "Speed"} \
   ,{0xF2, "DOS 3.3+ Secondary"} \
   ,{0xF4, "Speed"} \
   ,{0xFF, "BBT (Bad Blocks Table)"} \
};
