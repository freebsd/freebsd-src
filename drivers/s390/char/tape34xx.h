
/***************************************************************************
 *
 *  drivers/s390/char/tape34xx.h
 *    common tape device discipline for 34xx tapes.
 *
 *  S390 and zSeries version
 *    Copyright (C) 2001 IBM Corporation
 *    Author(s): Carsten Otte <cotte@de.ibm.com>
 *               Tuan Ngo-Anh <ngoanh@de.ibm.com>
 *
 ****************************************************************************
 */

#ifndef _TAPE34XX_H

#define _TAPE34XX_H

/*
 * The CCW commands for the Tape type of command.
 */

#define         INVALID_00              0x00    /* Invalid cmd      */
#define         BACKSPACEBLOCK          0x27    /* Back Space block */
#define         BACKSPACEFILE           0x2f    /* Back Space file */
#define         DATA_SEC_ERASE          0x97    /* Data security erase */
#define         ERASE_GAP               0x17    /* Erase Gap */
#define         FORSPACEBLOCK           0x37    /* Forward space block */
#define         FORSPACEFILE            0x3F    /* Forward Space file */
#define         FORCE_STREAM_CNT        0xEB    /* Forced streaming count #   */
#define         NOP                     0x03    /* No operation  */
#define         READ_FORWARD            0x02    /* Read forward */
#define         REWIND                  0x07    /* Rewind */
#define         REWIND_UNLOAD           0x0F    /* Rewind and Unload */
#define         SENSE                   0x04    /* Sense */
#define         NEW_MODE_SET            0xEB    /* Guess it is Mode set */
#define         WRITE_CMD               0x01    /* Write */
#define         WRITETAPEMARK           0x1F    /* Write Tape Mark */

#define         ASSIGN                  0xB7    /* 3420 REJECT,3480 OK  */
#define         CONTROL_ACCESS          0xE3    /* Set high speed */
#define         DIAG_MODE_SET           0x0B    /* 3420 NOP, 3480 REJECT*/
#define         LOAD_DISPLAY            0x9F    /* 3420 REJECT,3480 OK  */
#define         LOCATE                  0x4F    /* 3420 REJ, 3480 NOP   */
#define         LOOP_WRITE_TO_READ      0x8B    /* 3480 REJECT        */
#define         MODE_SET_DB             0xDB    /* 3420 REJECT,3480 OK  */
#define         MODE_SET_C3             0xC3    /* for 3420                */
#define         MODE_SET_CB             0xCB    /* for 3420                */
#define         MODE_SET_D3             0xD3    /* for 3420                */
#define         READ_BACKWARD           0x0C    /*                      */
#define         READ_BLOCK_ID           0x22    /* 3420 REJECT,3480 OK  */
#define         READ_BUFFER             0x12    /* 3420 REJECT,3480 OK  */
#define         READ_BUFF_LOG           0x24    /* 3420 REJECT,3480 OK  */
#define         RELEASE                 0xD4    /* 3420 NOP, 3480 REJECT*/
#define         REQ_TRK_IN_ERROR        0x1B    /* 3420 NOP, 3480 REJECT*/
#define         RESERVE                 0xF4    /* 3420 NOP, 3480 REJECT*/
#define         SENSE_GROUP_ID          0x34    /* 3420 REJECT,3480 OK  */
#define         SENSE_ID                0xE4    /* 3420 REJECT,3480 OK */
#define         READ_DEV_CHAR           0x64    /* Read device characteristics */
#define         SET_DIAGNOSE            0x4B    /* 3420 NOP, 3480 REJECT*/
#define         SET_GROUP_ID            0xAF    /* 3420 REJECT,3480 OK  */
#define         SET_TAPE_WRITE_IMMED    0xC3    /* for 3480                */
#define         SUSPEND                 0x5B    /* 3420 REJ, 3480 NOP   */
#define         SYNC                    0x43    /* Synchronize (flush buffer) */
#define         UNASSIGN                0xC7    /* 3420 REJECT,3480 OK  */
#define         PERF_SUBSYS_FUNC        0x77    /* 3490 CMD */
#define         READ_CONFIG_DATA        0xFA    /* 3490 CMD */
#define         READ_MESSAGE_ID         0x4E    /* 3490 CMD */
#define         READ_SUBSYS_DATA        0x3E    /* 3490 CMD */
#define         SET_INTERFACE_ID        0x73    /* 3490 CMD */

#ifndef MIN
#define MIN(a,b)                ( (a) < (b) ? (a) : (b) )
#endif


#define BLOCKSIZE               4096            /* size of the tape rcds */

#define COMMAND_CHAIN    CCW_FLAG_CC      /* redefine from irq.h */
#define CHANNEL_END      DEV_STAT_CHN_END /* redefine from irq.h */
#define DEVICE_END       DEV_STAT_DEV_END /* redefine from irq.h */
#define UNIT_CHECK       DEV_STAT_UNIT_CHECK  /* redefine from irq.h */
#define UNIT_EXCEPTION   DEV_STAT_UNIT_EXCEP  /* redefine from irq.h */
#define CONTROL_UNIT_END DEV_STAT_CU_END      /* redefine from irq.h */
#define INCORR_LEN       SCHN_STAT_INCORR_LEN /* redefine from irq.h */

#define SENSE_COMMAND_REJECT        0x80
#define SENSE_INTERVENTION_REQUIRED 0x40
#define SENSE_BUS_OUT_CHECK         0x20
#define SENSE_EQUIPMENT_CHECK       0x10
#define SENSE_DATA_CHECK            0x08
#define SENSE_OVERRUN               0x04
#define SENSE_DEFERRED_UNIT_CHECK   0x02
#define SENSE_ASSIGNED_ELSEWHERE    0x01

#define SENSE_LOCATE_FAILURE        0x80
#define SENSE_DRIVE_ONLINE          0x40
#define SENSE_RESERVED              0x20
#define SENSE_RECORD_SEQUENCE_ERR   0x10
#define SENSE_BEGINNING_OF_TAPE     0x08
#define SENSE_WRITE_MODE            0x04
#define SENSE_WRITE_PROTECT         0x02
#define SENSE_NOT_CAPABLE           0x01

#define SENSE_CHANNEL_ADAPTER_CODE  0xE0
#define SENSE_CHANNEL_ADAPTER_LOC   0x10
#define SENSE_REPORTING_CU          0x08
#define SENSE_AUTOMATIC_LOADER      0x04
#define SENSE_TAPE_SYNC_MODE        0x02
#define SENSE_TAPE_POSITIONING      0x01

typedef struct _tape34xx_disc_data_t {
    __u8 modeset_byte;
} tape34xx_disc_data_t  __attribute__ ((packed, aligned(8)));

/* discipline functions */
int tape34xx_ioctl_overload (struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
ccw_req_t * tape34xx_write_block (const char *data, size_t count, tape_info_t * ti);
void tape34xx_free_write_block (ccw_req_t * cqr, tape_info_t * ti);
ccw_req_t * tape34xx_read_block (const char *data, size_t count, tape_info_t * ti);
void  tape34xx_free_read_block (ccw_req_t * cqr, tape_info_t * ti);
void  tape34xx_clear_read_block (ccw_req_t * cqr, tape_info_t * ti);
ccw_req_t * tape34xx_mtfsf (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtbsf (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtfsr (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtbsr (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtweof (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtrew (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtoffl (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtnop (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtbsfm (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtfsfm (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mteom (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mterase (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtsetdensity (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtseek (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mttell (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtsetdrvbuffer (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtlock (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtunlock (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtload (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtunload (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtcompression (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtsetpart (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtmkpart (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtiocget (tape_info_t * ti, int count);
ccw_req_t * tape34xx_mtiocpos (tape_info_t * ti, int count);
ccw_req_t * tape34xx_bread (struct request *req, tape_info_t* ti,int tapeblock_major);
ccw_req_t * tape34xx_bwrite (struct request *req, tape_info_t* ti,int tapeblock_major);
void tape34xx_free_bread (ccw_req_t*,struct _tape_info_t*);
void tape34xx_free_bwrite (ccw_req_t*,struct _tape_info_t*);

/* Event handlers */
void tape34xx_default_handler (tape_info_t * ti);
void tape34xx_unexpect_uchk_handler (tape_info_t * ti);
void tape34xx_unused_done(tape_info_t* ti);
void tape34xx_idle_done(tape_info_t* ti);
void tape34xx_block_done(tape_info_t* ti);
void tape34xx_bsf_init_done(tape_info_t* ti);
void tape34xx_dse_init_done(tape_info_t* ti);
void tape34xx_fsf_init_done(tape_info_t* ti);
void tape34xx_bsb_init_done(tape_info_t* ti);
void tape34xx_fsb_init_done(tape_info_t* ti);
void tape34xx_lbl_init_done(tape_info_t* ti);
void tape34xx_nop_init_done(tape_info_t* ti);
void tape34xx_rfo_init_done(tape_info_t* ti);
void tape34xx_rbi_init_done(tape_info_t* ti);
void tape34xx_rew_init_done(tape_info_t* ti);
void tape34xx_rew_release_init_done(tape_info_t* ti);
void tape34xx_run_init_done(tape_info_t* ti);
void tape34xx_wri_init_done(tape_info_t* ti);
void tape34xx_wtm_init_done(tape_info_t* ti);

extern void schedule_tapeblock_exec_IO (tape_info_t *ti);

// the error recovery stuff:
void tape34xx_error_recovery (tape_info_t* ti);
void tape34xx_error_recovery_has_failed (tape_info_t* ti,int error_id);
void tape34xx_error_recovery_succeded(tape_info_t* ti);
void tape34xx_error_recovery_do_retry(tape_info_t* ti);
void tape34xx_error_recovery_read_opposite (tape_info_t* ti);
void  tape34xx_error_recovery_HWBUG (tape_info_t* ti,int condno);
#endif // _TAPE34XX_H
