
#ifndef _ST_H
#define _ST_H

#ifndef _SCSI_H
#include "scsi.h"
#endif
#include <linux/devfs_fs_kernel.h>
#include <linux/completion.h>

/* The tape buffer descriptor. */
typedef struct {
	unsigned char in_use;
	unsigned char dma;	/* DMA-able buffer */
	int buffer_size;
	int buffer_blocks;
	int buffer_bytes;
	int read_pointer;
	int writing;
	int midlevel_result;
	int syscall_result;
	Scsi_Request *last_SRpnt;
	unsigned char *b_data;
	unsigned short use_sg;	/* zero or number of segments for this adapter */
	unsigned short sg_segs;	/* total number of allocated segments */
	unsigned short orig_sg_segs;	/* number of segments allocated at first try */
	unsigned int *sg_lengths;
	struct scatterlist sg[1];	/* MUST BE last item */
} ST_buffer;


/* The tape mode definition */
typedef struct {
	unsigned char defined;
	unsigned char sysv;	/* SYS V semantics? */
	unsigned char do_async_writes;
	unsigned char do_buffer_writes;
	unsigned char do_read_ahead;
	unsigned char defaults_for_writes;
	unsigned char default_compression;	/* 0 = don't touch, etc */
	short default_density;	/* Forced density, -1 = no value */
	int default_blksize;	/* Forced blocksize, -1 = no value */
} ST_mode;

#define ST_NBR_MODE_BITS 2
#define ST_NBR_MODES (1 << ST_NBR_MODE_BITS)
#define ST_MODE_SHIFT (7 - ST_NBR_MODE_BITS)
#define ST_MODE_MASK ((ST_NBR_MODES - 1) << ST_MODE_SHIFT)
#define ST_MAX_TAPES (1 << ST_MODE_SHIFT)

/* The status related to each partition */
typedef struct {
	unsigned char rw;
	unsigned char eof;
	unsigned char at_sm;
	unsigned char last_block_valid;
	u32 last_block_visited;
	int drv_block;		/* The block where the drive head is */
	int drv_file;
} ST_partstat;

#define ST_NBR_PARTITIONS 4

/* The tape drive descriptor */
typedef struct {
	kdev_t devt;
	Scsi_Device *device;
	struct semaphore lock;	/* For serialization */
	struct completion wait;	/* For SCSI commands */
	ST_buffer *buffer;

	/* Drive characteristics */
	unsigned char omit_blklims;
	unsigned char do_auto_lock;
	unsigned char can_bsr;
	unsigned char can_partitions;
	unsigned char two_fm;
	unsigned char fast_mteom;
	unsigned char immediate;
	unsigned char restr_dma;
	unsigned char scsi2_logical;
	unsigned char default_drvbuffer;	/* 0xff = don't touch, value 3 bits */
	unsigned char cln_mode;			/* 0 = none, otherwise sense byte nbr */
	unsigned char cln_sense_value;
	unsigned char cln_sense_mask;
	unsigned char use_pf;			/* Set Page Format bit in all mode selects? */
	unsigned char c_algo;	     /* compression algorithm */
	int tape_type;
	int timeout;		/* timeout for normal commands */
	int long_timeout;	/* timeout for commands known to take long time */

	/* Mode characteristics */
	ST_mode modes[ST_NBR_MODES];
	int current_mode;
	devfs_handle_t de_r[ST_NBR_MODES];  /*  Rewind entries     */
	devfs_handle_t de_n[ST_NBR_MODES];  /*  No-rewind entries  */

	/* Status variables */
	int partition;
	int new_partition;
	int nbr_partitions;	/* zero until partition support enabled */
	ST_partstat ps[ST_NBR_PARTITIONS];
	unsigned char dirty;
	unsigned char ready;
	unsigned char write_prot;
	unsigned char drv_write_prot;
	unsigned char in_use;
	unsigned char blksize_changed;
	unsigned char density_changed;
	unsigned char compression_changed;
	unsigned char drv_buffer;
	unsigned char density;
	unsigned char door_locked;
	unsigned char autorew_dev;   /* auto-rewind device */
	unsigned char rew_at_close;  /* rewind necessary at close */
	unsigned char inited;
	unsigned char cleaning_req;  /* cleaning requested? */
	int block_size;
	int min_block;
	int max_block;
	int recover_count;     /* From tape opening */
	int recover_reg;       /* From last status call */

#if DEBUG
	unsigned char write_pending;
	int nbr_finished;
	int nbr_waits;
	unsigned char last_cmnd[6];
	unsigned char last_sense[16];
#endif
} Scsi_Tape;

/* Bit masks for use_pf */
#define USE_PF      1
#define PF_TESTED   2

/* Values of eof */
#define	ST_NOEOF	0
#define ST_FM_HIT       1
#define ST_FM           2
#define ST_EOM_OK       3
#define ST_EOM_ERROR	4
#define	ST_EOD_1        5
#define ST_EOD_2        6
#define ST_EOD		7
/* EOD hit while reading => ST_EOD_1 => return zero => ST_EOD_2 =>
   return zero => ST_EOD, return ENOSPC */

/* Values of rw */
#define	ST_IDLE		0
#define	ST_READING	1
#define	ST_WRITING	2

/* Values of ready state */
#define ST_READY	0
#define ST_NOT_READY	1
#define ST_NO_TAPE	2

/* Values for door lock state */
#define ST_UNLOCKED	0
#define ST_LOCKED_EXPLICIT 1
#define ST_LOCKED_AUTO  2
#define ST_LOCK_FAILS   3

/* Positioning SCSI-commands for Tandberg, etc. drives */
#define	QFA_REQUEST_BLOCK	0x02
#define	QFA_SEEK_BLOCK		0x0c

/* Setting the binary options */
#define ST_DONT_TOUCH  0
#define ST_NO          1
#define ST_YES         2

#define EXTENDED_SENSE_START  18

#endif
