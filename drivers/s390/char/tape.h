/***************************************************************************
 *
 *  drivers/s390/char/tape.h
 *    tape device driver for 3480/3490E tapes.
 *
 *  S390 and zSeries version
 *    Copyright (C) 2001 IBM Corporation
 *    Author(s): Carsten Otte <cotte@de.ibm.com>
 *               Tuan Ngo-Anh <ngoanh@de.ibm.com>
 *
 ****************************************************************************
 */

#ifndef _TAPE_H

#define _TAPE_H
#include <linux/config.h>
#include <linux/blkdev.h>

#define  MAX_TAPES                     7        /* Max tapes supported is 7*/
#define TAPE_MAGIC 0xE3C1D7C5       /* is ebcdic-"TAPE" */

typedef enum {
    TS_UNUSED=0, TS_IDLE, TS_DONE, TS_FAILED,
    TS_BLOCK_INIT,
    TS_BSB_INIT,
    TS_BSF_INIT,
    TS_DSE_INIT,
    TS_EGA_INIT,
    TS_FSB_INIT,
    TS_FSF_INIT,
    TS_LDI_INIT,
    TS_LBL_INIT,
    TS_MSE_INIT,
    TS_NOP_INIT,
    TS_RBA_INIT,
    TS_RBI_INIT,
    TS_RBU_INIT,
    TS_RBL_INIT,
    TS_RDC_INIT,
    TS_RFO_INIT,
    TS_RSD_INIT,
    TS_REW_INIT,
    TS_REW_RELEASE_INIT,
    TS_RUN_INIT,
    TS_SEN_INIT,
    TS_SID_INIT,
    TS_SNP_INIT,
    TS_SPG_INIT,
    TS_SWI_INIT,
    TS_SMR_INIT,
    TS_SYN_INIT,
    TS_TIO_INIT,
    TS_UNA_INIT,
    TS_WRI_INIT,
    TS_WTM_INIT,
    TS_NOT_OPER,
    TS_SIZE } tape_stat;

struct _tape_info_t; //Forward declaration

typedef enum {
    TE_START=0, TE_DONE, TE_FAILED, TE_ERROR, TE_OTHER,
    TE_SIZE } tape_events;

typedef void (*tape_disc_shutdown_t) (int);
typedef void (*tape_event_handler_t) (struct _tape_info_t*);
typedef ccw_req_t* (*tape_ccwgen_t)(struct _tape_info_t* ti,int count);
typedef ccw_req_t* (*tape_reqgen_t)(struct request* req,struct _tape_info_t* ti,int tapeblock_major);
typedef ccw_req_t* (*tape_rwblock_t)(const char* data,size_t count,struct _tape_info_t* ti);
typedef void (*tape_freeblock_t)(ccw_req_t* cqr,struct _tape_info_t* ti);
typedef void (*tape_setup_assist_t) (struct _tape_info_t*);
#ifdef CONFIG_DEVFS_FS
typedef void (*tape_devfs_handler_t) (struct _tape_info_t*);
#endif
typedef tape_event_handler_t tape_event_table_t[TS_SIZE][TE_SIZE];
typedef struct _tape_discipline_t {
    unsigned int cu_type;
    tape_setup_assist_t setup_assist;
    tape_event_handler_t error_recovery;
    tape_reqgen_t bread;
    tape_freeblock_t free_bread;
    tape_rwblock_t write_block;
    tape_freeblock_t free_write_block;
    tape_rwblock_t read_block;
    tape_freeblock_t free_read_block;
    tape_ccwgen_t mtfsf;
    tape_ccwgen_t mtbsf;
    tape_ccwgen_t mtfsr;
    tape_ccwgen_t mtbsr;
    tape_ccwgen_t mtweof;
    tape_ccwgen_t mtrew;
    tape_ccwgen_t mtoffl;
    tape_ccwgen_t mtnop;
    tape_ccwgen_t mtbsfm;
    tape_ccwgen_t mtfsfm;
    tape_ccwgen_t mteom;
    tape_ccwgen_t mterase;
    tape_ccwgen_t mtsetdensity;
    tape_ccwgen_t mtseek;
    tape_ccwgen_t mttell;
    tape_ccwgen_t mtsetdrvbuffer;
    tape_ccwgen_t mtlock;
    tape_ccwgen_t mtunlock;
    tape_ccwgen_t mtload;
    tape_ccwgen_t mtunload;
    tape_ccwgen_t mtcompression;
    tape_ccwgen_t mtsetpart;
    tape_ccwgen_t mtmkpart;
    tape_ccwgen_t mtiocget;
    tape_ccwgen_t mtiocpos;
    tape_disc_shutdown_t shutdown;
    int (*discipline_ioctl_overload)(struct inode *,struct file*, unsigned int,unsigned long);
    tape_event_table_t* event_table;
    tape_event_handler_t default_handler;
    struct _tape_info_t* tape; /* pointer for backreference */
    void* next;
} tape_discipline_t  __attribute__ ((aligned(8)));

typedef struct _tape_frontend_t {
    tape_setup_assist_t device_setup;
#ifdef CONFIG_DEVFS_FS
    tape_devfs_handler_t mkdevfstree;
    tape_devfs_handler_t rmdevfstree;
#endif
    void* next;
} tape_frontend_t  __attribute__ ((aligned(8)));


typedef struct _tape_info_t {
    wait_queue_head_t wq;
    s390_dev_info_t devinfo;             /* device info from Common I/O */
    int     wanna_wakeup;
    int     rew_minor;                  /* minor number for the rewinding tape */
    int     nor_minor;                  /* minor number for the nonrewinding tape */
    int     blk_minor;                  /* minor number for the block device */
    devstat_t devstat;	           /* contains irq, devno, status */
    size_t  block_size;             /* block size of tape        */
    int    drive_type;              /* Code indicating type of drive */
    struct file *rew_filp;	           /* backpointer to file structure */
    struct file *nor_filp;
    struct file *blk_filp;
    int tape_state;  /* State of the device. See tape_stat */
    int rc;          /* Return code. */
    tape_discipline_t* discipline;
    request_queue_t request_queue;
    struct request* current_request;
    int blk_retries;
    long position;
    int medium_is_unloaded;  // Becomes true when a unload-type operation was issued, false again when medium-insert was detected
    ccw_req_t* cqr;
    atomic_t bh_scheduled;
    struct tq_struct bh_tq;
#ifdef CONFIG_DEVFS_FS
    devfs_handle_t devfs_dir;             /* devfs handle for tape/DEVNO directory */
    devfs_handle_t devfs_char_dir;        /* devfs handle for tape/DEVNO/char directory */
    devfs_handle_t devfs_block_dir;       /* devfs handle for tape/DEVNO/block directory */
    devfs_handle_t devfs_nonrewinding;    /* devfs handle for tape/DEVNO/char/nonrewinding device */
    devfs_handle_t devfs_rewinding;       /* devfs handle for tape/DEVNO/char/rewinding device */
    devfs_handle_t devfs_disc;            /* devfs handle for tape/DEVNO/block/disc device */
#endif
    void* discdata;
    void* kernbuf;
    void* userbuf;
    void*  next;
} tape_info_t  __attribute__ ((aligned(8)));

/* tape initialisation functions */
int tape_init(void);
int tape_setup (tape_info_t * ti, int irq, int minor);

/* functoins for alloc'ing ccw stuff */
inline  ccw_req_t * tape_alloc_ccw_req (tape_info_t* ti, int cplength, int datasize);
void tape_free_request (ccw_req_t * request);

/* a function for dumping device sense info */
void tape_dump_sense (devstat_t * stat);

#ifdef CONFIG_S390_TAPE_DYNAMIC
/* functions for dyn. dev. attach/detach */
int tape_oper_handler ( int irq, struct _devreg *dreg);
#endif

/* functions for handling the status of a device */
inline void tapestate_set (tape_info_t * ti, int newstate);
inline int tapestate_get (tape_info_t * ti);
void tapestate_event (tape_info_t * ti, int event);
extern char* state_verbose[TS_SIZE];
extern char* event_verbose[TE_SIZE];

/****************************************************************************/

/* Some linked lists for storing plugins and devices */
extern tape_info_t *first_tape_info;
extern tape_discipline_t *first_discipline;
extern tape_frontend_t *first_frontend;

/* The debug area */
#ifdef TAPE_DEBUG
extern debug_info_t *tape_debug_area;
#endif

#endif /* for ifdef tape.h */
