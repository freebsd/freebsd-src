/*
 * Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organizations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 *      $FreeBSD$
 */

#ifndef _BTREG_H_
#define _BTREG_H_

#include "bt.h"

/*
 * Mail box defs  etc.
 * these could be bigger but we need the bt_data to fit on a single page..
 */

#define BT_MBX_SIZE     32      /* mail box size  (MAX 255 MBxs) */
                                /* don't need that many really */
#define BT_CCB_MAX      32      /* store up to 32CCBs at any one time */
                                /* in bt742a H/W ( Not MAX ? ) */
#define CCB_HASH_SIZE   32      /* when we have a physical addr. for */
                                /* a ccb and need to find the ccb in */
                                /* space, look it up in the hash table */
#define BT_NSEG		33	/* 
				 * Number of SG segments per command
				 * Max of 2048???
				 */

typedef unsigned long int physaddr;

struct bt_scat_gath {
        unsigned long seg_len;
        physaddr seg_addr;
};

typedef struct bt_mbx_out {     
	physaddr ccb_addr;
	unsigned char dummy[3]; 
	unsigned char cmd;
} BT_MBO;

typedef struct bt_mbx_in {      
	physaddr ccb_addr;    
	unsigned char btstat;   
	unsigned char sdstat;   
	unsigned char dummy;    
	unsigned char stat;     
} BT_MBI;

struct bt_mbx {
	BT_MBO  mbo[BT_MBX_SIZE];
#define		BT_MBO_FREE	0x0	/* MBO intry is free */
#define		BT_MBO_START	0x1	/* MBO activate entry */
#define		BT_MBO_ABORT	0x2	/* MBO abort entry */
	BT_MBI  mbi[BT_MBX_SIZE]; 
#define		BT_MBI_FREE	0x0	/* MBI entry is free */ 
#define		BT_MBI_OK	0x1	/* completed without error */
#define		BT_MBI_ABORT	0x2	/* aborted ccb */
#define		BT_MBI_UNKNOWN	0x3	/* Tried to abort invalid CCB */
#define		BT_MBI_ERROR	0x4	/* Completed with error */
	BT_MBO *tmbo;			/* Target Mail Box out */
	BT_MBI *tmbi;			/* Target Mail Box in */
};


struct bt_ccb {
	unsigned char opcode;
#define		BT_INITIATOR_CCB	0x00	/* SCSI Initiator CCB */
#define		BT_TARGET_CCB		0x01	/* SCSI Target CCB */
#define		BT_INIT_SCAT_GATH_CCB	0x02	/* SCSI Initiator w/sg */
#define		BT_RESET_CCB		0x81	/* SCSI Bus reset */
	unsigned char:3, data_in:1, data_out:1,:3;
	unsigned char scsi_cmd_length;
	unsigned char req_sense_length;
	unsigned long data_length;
	physaddr data_addr;
	unsigned char dummy[2];
	unsigned char host_stat;
#define		BT_OK		0x00	/* cmd ok */
#define		BT_LINK_OK	0x0a	/* Link cmd ok */
#define		BT_LINK_IT	0x0b	/* Link cmd ok + int */
#define		BT_SEL_TIMEOUT	0x11	/* Selection time out */
#define		BT_OVER_UNDER	0x12	/* Data over/under run */
#define		BT_BUS_FREE	0x13	/* Bus dropped at unexpected time */
#define		BT_INV_BUS	0x14	/* Invalid bus phase/sequence */
#define		BT_BAD_MBO	0x15	/* Incorrect MBO cmd */
#define		BT_BAD_CCB	0x16	/* Incorrect ccb opcode */ 
#define		BT_BAD_LINK	0x17	/* Not same values of LUN for links */
#define		BT_INV_TARGET	0x18	/* Invalid target direction */
#define		BT_CCB_DUP	0x19	/* Duplicate CCB received */
#define		BT_INV_CCB	0x1a	/* Invalid CCB or segment list */
#define		BT_ABORTED	42	/* pseudo value from driver */
	unsigned char target_stat;
	unsigned char target;
	unsigned char lun; 
	unsigned char scsi_cmd[12];	/* 12 bytes (bytes only) */
	unsigned char dummy2[1];
	unsigned char link_id;
	physaddr link_addr;
	physaddr sense_ptr;
	struct	scsi_sense_data scsi_sense;
	struct	bt_scat_gath scat_gath[BT_NSEG];
	struct	bt_ccb *next;
	struct	scsi_xfer *xfer;	/* the scsi_xfer for this cmd */
	struct	bt_mbx_out *mbx;	/* pointer to mail box */
	int	flags;
#define		CCB_FREE	0
#define		CCB_ACTIVE	1
#define		CCB_ABORTED	2
	struct bt_ccb *nexthash;	/* if two hash the same */
	physaddr hashkey;		/*physaddr of this ccb */
};

struct bt_data {
	int	bt_base;			/* base port for each board */
	struct	bt_mbx bt_mbx;			/* all our mailboxes */
	struct	bt_ccb *bt_ccb_free;		/* list of free CCBs */
	struct	bt_ccb *ccbhash[CCB_HASH_SIZE];	/* phys to kv hash */
	int	bt_int;				/* int. read off board */
	int	bt_dma;				/* DMA channel read of board */
	int	bt_scsi_dev;			/* adapters scsi id */
	int	numccbs;			/* how many we have malloc'd */
	int	bt_bounce;			/* should we bounce? */
	int	unit;				/* The zero in bt0 */
	struct	scsi_link sc_link;		/* prototype for devs */
};

extern struct bt_data *btdata[NBT];

extern u_long bt_unit;

struct bt_data *bt_alloc __P((int unit, u_long iobase));
void bt_free __P((struct bt_data *bt));
void bt_intr __P((void *arg));
int bt_init __P((struct bt_data *bt)); 
int bt_attach __P((struct bt_data *bt));

#endif	/* _BT_H_ */
