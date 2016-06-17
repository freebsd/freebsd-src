
/* Defines for NAND Flash Translation Layer  */
/* (c) 1999 Machine Vision Holdings, Inc.    */
/* Author: David Woodhouse <dwmw2@mvhi.com>  */
/* $Id: nftl.h,v 1.11 2002/06/18 13:54:24 dwmw2 Exp $ */

#ifndef __MTD_NFTL_H__
#define __MTD_NFTL_H__

#ifndef __BOOT__
#include <linux/mtd/mtd.h>
#endif

/* Block Control Information */

struct nftl_bci {
	unsigned char ECCSig[6];
	__u8 Status;
	__u8 Status1;
}__attribute__((packed));

/* Unit Control Information */

struct nftl_uci0 {
	__u16 VirtUnitNum;
	__u16 ReplUnitNum;
	__u16 SpareVirtUnitNum;
	__u16 SpareReplUnitNum;
} __attribute__((packed));

struct nftl_uci1 {
	__u32 WearInfo;
	__u16 EraseMark;
	__u16 EraseMark1;
} __attribute__((packed));

struct nftl_uci2 {
        __u16 FoldMark;
        __u16 FoldMark1;
	__u32 unused;
} __attribute__((packed));

union nftl_uci {
	struct nftl_uci0 a;
	struct nftl_uci1 b;
	struct nftl_uci2 c;
};

struct nftl_oob {
	struct nftl_bci b;
	union nftl_uci u;
};

/* NFTL Media Header */

struct NFTLMediaHeader {
	char DataOrgID[6];
	__u16 NumEraseUnits;
	__u16 FirstPhysicalEUN;
	__u32 FormattedSize;
	unsigned char UnitSizeFactor;
} __attribute__((packed));

#define MAX_ERASE_ZONES (8192 - 512)

#define ERASE_MARK 0x3c69
#define SECTOR_FREE 0xff
#define SECTOR_USED 0x55
#define SECTOR_IGNORE 0x11
#define SECTOR_DELETED 0x00

#define FOLD_MARK_IN_PROGRESS 0x5555

#define ZONE_GOOD 0xff
#define ZONE_BAD_ORIGINAL 0
#define ZONE_BAD_MARKED 7

#ifdef __KERNEL__

/* these info are used in ReplUnitTable */
#define BLOCK_NIL          0xffff /* last block of a chain */
#define BLOCK_FREE         0xfffe /* free block */
#define BLOCK_NOTEXPLORED  0xfffd /* non explored block, only used during mounting */
#define BLOCK_RESERVED     0xfffc /* bios block or bad block */

struct NFTLrecord {
	struct mtd_info *mtd;
	struct semaphore mutex;
	__u16 MediaUnit, SpareMediaUnit;
	__u32 EraseSize;
	struct NFTLMediaHeader MediaHdr;
	int usecount;
	unsigned char heads;
	unsigned char sectors;
	unsigned short cylinders;
	__u16 numvunits;
	__u16 lastEUN;                  /* should be suppressed */
	__u16 numfreeEUNs;
	__u16 LastFreeEUN; 		/* To speed up finding a free EUN */
	__u32 nr_sects;
	int head,sect,cyl;
	__u16 *EUNtable; 		/* [numvunits]: First EUN for each virtual unit  */
	__u16 *ReplUnitTable; 		/* [numEUNs]: ReplUnitNumber for each */
        unsigned int nb_blocks;		/* number of physical blocks */
        unsigned int nb_boot_blocks;	/* number of blocks used by the bios */
        struct erase_info instr;
};

int NFTL_mount(struct NFTLrecord *s);
int NFTL_formatblock(struct NFTLrecord *s, int block);

#ifndef NFTL_MAJOR
#define NFTL_MAJOR 93
#endif

#define MAX_NFTLS 16
#define MAX_SECTORS_PER_UNIT 32
#define NFTL_PARTN_BITS 4

#endif /* __KERNEL__ */

#endif /* __MTD_NFTL_H__ */
