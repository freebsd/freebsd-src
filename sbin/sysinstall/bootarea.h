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

#define BOOT_MAGIC 0xAA55
#define ACTIVE 0x80
#define BOOT1 "/usr/mdec/sdboot"
#define BOOT2 "/usr/mdec/bootsd"

/* XXX -- calculate these, this is nasty */
#define DEFFSIZE 1024
#define DEFFRAG 8

extern char *part_type(int);
extern int disk_size(int);
extern int enable_label(int);
extern int disable_label(int);
extern int write_bootblocks(int, off_t, int);
extern int build_bootblocks(struct disklabel *);
extern void build_disklabel(struct disklabel *, int, int);
extern int write_bootarea(int);
extern int read_bootarea(int);

struct bootarea
{
	unsigned char padding[2]; /* force longs to be long aligned */
	unsigned char bootcode[DOSPARTOFF];
	struct dos_partition dospart[4];
	unsigned short signature;
};

struct part_type 
{
 unsigned char type;
 char *name;
};
