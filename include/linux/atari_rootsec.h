#ifndef _LINUX_ATARI_ROOTSEC_H
#define _LINUX_ATARI_ROOTSEC_H

/*
 * linux/include/linux/atari_rootsec.h
 * definitions for Atari Rootsector layout
 * by Andreas Schwab (schwab@ls5.informatik.uni-dortmund.de)
 *
 * modified for ICD/Supra partitioning scheme restricted to at most 12
 * partitions
 * by Guenther Kelleter (guenther@pool.informatik.rwth-aachen.de)
 */

struct partition_info
{
  u_char flg;			/* bit 0: active; bit 7: bootable */
  char id[3];			/* "GEM", "BGM", "XGM", or other */
  u32 st;			/* start of partition */
  u32 siz;			/* length of partition */
};

struct rootsector
{
  char unused[0x156];			/* room for boot code */
  struct partition_info icdpart[8];	/* info for ICD-partitions 5..12 */
  char unused2[0xc];
  u32 hd_siz;				/* size of disk in blocks */
  struct partition_info part[4];
  u32 bsl_st;				/* start of bad sector list */
  u32 bsl_cnt;				/* length of bad sector list */
  u16 checksum;				/* checksum for bootable disks */
} __attribute__ ((__packed__));

#endif /* _LINUX_ATARI_ROOTSEC_H */
