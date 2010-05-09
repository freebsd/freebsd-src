/*
 *  include/scst.h
 *
 *  Copyright (C) 2004 - 2009 Vladislav Bolkhovitin <vst@vlnb.net>
 *  Copyright (C) 2004 - 2005 Leonid Stoljar
 *  Copyright (C) 2007 - 2009 ID7 Ltd.
 *
 *  Main SCSI target mid-level include file.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, version 2
 *  of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 */

#ifndef __SCST_H
#define __SCST_H

#include <linux/types.h>
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>

#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi.h>

#include <scst_const.h>

#include "scst_sgv.h"

/*
 * Version numbers, the same as for the kernel.
 *
 * Changing it don't forget to change SCST_FIO_REV in scst_vdisk.c
 * and FIO_REV in usr/fileio/common.h as well.
 */
#define SCST_VERSION_CODE	    0x01000101
#define SCST_VERSION(a, b, c, d)    (((a) << 24) + ((b) << 16) + ((c) << 8) + d)
#define SCST_VERSION_STRING	    "1.0.1.1"
#define SCST_INTERFACE_VERSION	    \
		SCST_VERSION_STRING "$Revision: 865 $" SCST_CONST_VERSION

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
#ifndef RHEL_RELEASE_CODE
typedef _Bool bool;
#endif
#define true  1
#define false 0
#endif

#define SCST_LOCAL_NAME			"scst_lcl_drvr"

/*************************************************************
 ** States of command processing state machine. At first,
 ** "active" states, then - "passive" ones. This is to have
 ** more efficient generated code of the corresponding
 ** "switch" statements.
 *************************************************************/

/* Internal parsing */
#define SCST_CMD_STATE_PRE_PARSE     0

/* Dev handler's parse() is going to be called */
#define SCST_CMD_STATE_DEV_PARSE     1

/* Allocation of the cmd's data buffer */
#define SCST_CMD_STATE_PREPARE_SPACE 2

/* Target driver's rdy_to_xfer() is going to be called */
#define SCST_CMD_STATE_RDY_TO_XFER   3

/* Target driver's pre_exec() is going to be called */
#define SCST_CMD_STATE_TGT_PRE_EXEC  4

/* Cmd is going to be sent for execution */
#define SCST_CMD_STATE_SEND_FOR_EXEC 5

/* Cmd is being checked if it should be executed locally */
#define SCST_CMD_STATE_LOCAL_EXEC    6

/* Cmd is ready for execution */
#define SCST_CMD_STATE_REAL_EXEC     7

/* Internal post-exec checks */
#define SCST_CMD_STATE_PRE_DEV_DONE  8

/* Internal MODE SELECT pages related checks */
#define SCST_CMD_STATE_MODE_SELECT_CHECKS 9

/* Dev handler's dev_done() is going to be called */
#define SCST_CMD_STATE_DEV_DONE      10

/* Target driver's xmit_response() is going to be called */
#define SCST_CMD_STATE_PRE_XMIT_RESP 11

/* Target driver's xmit_response() is going to be called */
#define SCST_CMD_STATE_XMIT_RESP     12

/* Cmd finished */
#define SCST_CMD_STATE_FINISHED      13

/* Internal cmd finished */
#define SCST_CMD_STATE_FINISHED_INTERNAL 14

#define SCST_CMD_STATE_LAST_ACTIVE   (SCST_CMD_STATE_FINISHED_INTERNAL+100)

/* A cmd is created, but scst_cmd_init_done() not called */
#define SCST_CMD_STATE_INIT_WAIT     (SCST_CMD_STATE_LAST_ACTIVE+1)

/* LUN translation (cmd->tgt_dev assignment) */
#define SCST_CMD_STATE_INIT          (SCST_CMD_STATE_LAST_ACTIVE+2)

/* Allocation of the cmd's data buffer */
#define SCST_CMD_STATE_PREPROCESS_DONE (SCST_CMD_STATE_LAST_ACTIVE+3)

/* Waiting for data from the initiator (until scst_rx_data() called) */
#define SCST_CMD_STATE_DATA_WAIT     (SCST_CMD_STATE_LAST_ACTIVE+4)

/* Waiting for CDB's execution finish */
#define SCST_CMD_STATE_REAL_EXECUTING (SCST_CMD_STATE_LAST_ACTIVE+5)

/* Waiting for response's transmission finish */
#define SCST_CMD_STATE_XMIT_WAIT     (SCST_CMD_STATE_LAST_ACTIVE+6)

/*************************************************************
 * Can be retuned instead of cmd's state by dev handlers'
 * functions, if the command's state should be set by default
 *************************************************************/
#define SCST_CMD_STATE_DEFAULT        500

/*************************************************************
 * Can be retuned instead of cmd's state by dev handlers'
 * functions, if it is impossible to complete requested
 * task in atomic context. The cmd will be restarted in thread
 * context.
 *************************************************************/
#define SCST_CMD_STATE_NEED_THREAD_CTX 1000

/*************************************************************
 * Can be retuned instead of cmd's state by dev handlers'
 * parse function, if the cmd processing should be stopped
 * for now. The cmd will be restarted by dev handlers itself.
 *************************************************************/
#define SCST_CMD_STATE_STOP           1001

/*************************************************************
 ** States of mgmt command processing state machine
 *************************************************************/

/* LUN translation (mcmd->tgt_dev assignment) */
#define SCST_MCMD_STATE_INIT     0

/* Mgmt cmd is ready for processing */
#define SCST_MCMD_STATE_READY    1

/* Mgmt cmd is being executing */
#define SCST_MCMD_STATE_EXECUTING 2

/* Post check when affected commands done */
#define SCST_MCMD_STATE_POST_AFFECTED_CMDS_DONE 3

/* Target driver's task_mgmt_fn_done() is going to be called */
#define SCST_MCMD_STATE_DONE     4

/* The mcmd finished */
#define SCST_MCMD_STATE_FINISHED 5

/*************************************************************
 ** Constants for "atomic" parameter of SCST's functions
 *************************************************************/
#define SCST_NON_ATOMIC              0
#define SCST_ATOMIC                  1

/*************************************************************
 ** Values for pref_context parameter of scst_cmd_init_done(),
 ** scst_rx_data(), scst_restart_cmd(), scst_tgt_cmd_done()
 ** and scst_cmd_done()
 *************************************************************/

enum scst_exec_context {
	/*
	 * Direct cmd's processing (i.e. regular function calls in the current
	 * context) sleeping is not allowed
	 */
	SCST_CONTEXT_DIRECT_ATOMIC,

	/*
	 * Direct cmd's processing (i.e. regular function calls in the current
	 * context), sleeping is allowed, no restrictions
	 */
	SCST_CONTEXT_DIRECT,

	/* Tasklet or thread context required for cmd's processing */
	SCST_CONTEXT_TASKLET,

	/* Thread context required for cmd's processing */
	SCST_CONTEXT_THREAD,

	/*
	 * Context is the same as it was in previous call of the corresponding
	 * callback. For example, if dev handler's exec() does sync. data
	 * reading this value should be used for scst_cmd_done(). The same is
	 * true if scst_tgt_cmd_done() called directly from target driver's
	 * xmit_response(). Not allowed in scst_cmd_init_done() and
	 * scst_cmd_init_stage1_done().
	 */
	SCST_CONTEXT_SAME
};

/*************************************************************
 ** Values for status parameter of scst_rx_data()
 *************************************************************/

/* Success */
#define SCST_RX_STATUS_SUCCESS       0

/*
 * Data receiving finished with error, so set the sense and
 * finish the command, including xmit_response() call
 */
#define SCST_RX_STATUS_ERROR         1

/*
 * Data receiving finished with error and the sense is set,
 * so finish the command, including xmit_response() call
 */
#define SCST_RX_STATUS_ERROR_SENSE_SET 2

/*
 * Data receiving finished with fatal error, so finish the command,
 * but don't call xmit_response()
 */
#define SCST_RX_STATUS_ERROR_FATAL   3

/*************************************************************
 ** Values for status parameter of scst_restart_cmd()
 *************************************************************/

/* Success */
#define SCST_PREPROCESS_STATUS_SUCCESS       0

/*
 * Command's processing finished with error, so set the sense and
 * finish the command, including xmit_response() call
 */
#define SCST_PREPROCESS_STATUS_ERROR         1

/*
 * Command's processing finished with error and the sense is set,
 * so finish the command, including xmit_response() call
 */
#define SCST_PREPROCESS_STATUS_ERROR_SENSE_SET 2

/*
 * Command's processing finished with fatal error, so finish the command,
 * but don't call xmit_response()
 */
#define SCST_PREPROCESS_STATUS_ERROR_FATAL   3

/* Thread context requested */
#define SCST_PREPROCESS_STATUS_NEED_THREAD   4

/*************************************************************
 ** Values for AEN functions
 *************************************************************/

/*
 * SCSI Asynchronous Event. Parameter contains SCSI sense
 * (Unit Attention). AENs generated only for 2 the following UAs:
 * CAPACITY DATA HAS CHANGED and REPORTED LUNS DATA HAS CHANGED.
 * Other UAs reported regularly as CHECK CONDITION status,
 * because it doesn't look safe to report them using AENs, since
 * reporting using AENs opens delivery race windows even in case of
 * untagged commands.
 */
#define SCST_AEN_SCSI                0

/*************************************************************
 ** Allowed return/status codes for report_aen() callback and
 ** scst_set_aen_delivery_status() function
 *************************************************************/

/* Success */
#define SCST_AEN_RES_SUCCESS         0

/* Not supported */
#define SCST_AEN_RES_NOT_SUPPORTED  -1

/* Failure */
#define SCST_AEN_RES_FAILED         -2

/*************************************************************
 ** Allowed return codes for xmit_response(), rdy_to_xfer()
 *************************************************************/

/* Success */
#define SCST_TGT_RES_SUCCESS         0

/* Internal device queue is full, retry again later */
#define SCST_TGT_RES_QUEUE_FULL      -1

/*
 * It is impossible to complete requested task in atomic context.
 * The cmd will be restarted in thread  context.
 */
#define SCST_TGT_RES_NEED_THREAD_CTX -2

/*
 * Fatal error, if returned by xmit_response() the cmd will
 * be destroyed, if by any other function, xmit_response()
 * will be called with HARDWARE ERROR sense data
 */
#define SCST_TGT_RES_FATAL_ERROR     -3

/*************************************************************
 ** Allowed return codes for dev handler's exec()
 *************************************************************/

/* The cmd is done, go to other ones */
#define SCST_EXEC_COMPLETED          0

/* The cmd should be sent to SCSI mid-level */
#define SCST_EXEC_NOT_COMPLETED      1

/*
 * Thread context is required to execute the command.
 * Exec() will be called again in the thread context.
 */
#define SCST_EXEC_NEED_THREAD        2

/*
 * Set if cmd is finished and there is status/sense to be sent.
 * The status should be not sent (i.e. the flag not set) if the
 * possibility to perform a command in "chunks" (i.e. with multiple
 * xmit_response()/rdy_to_xfer()) is used (not implemented yet).
 * Obsolete, use scst_cmd_get_is_send_status() instead.
 */
#define SCST_TSC_FLAG_STATUS         0x2

/*************************************************************
 ** Additional return code for dev handler's task_mgmt_fn()
 *************************************************************/

/* Regular standard actions for the command should be done */
#define SCST_DEV_TM_NOT_COMPLETED     1

/*************************************************************
 ** Session initialization phases
 *************************************************************/

/* Set if session is being initialized */
#define SCST_SESS_IPH_INITING        0

/* Set if the session is successfully initialized */
#define SCST_SESS_IPH_SUCCESS        1

/* Set if the session initialization failed */
#define SCST_SESS_IPH_FAILED         2

/* Set if session is initialized and ready */
#define SCST_SESS_IPH_READY          3

/*************************************************************
 ** Session shutdown phases
 *************************************************************/

/* Set if session is initialized and ready */
#define SCST_SESS_SPH_READY          0

/* Set if session is shutting down */
#define SCST_SESS_SPH_SHUTDOWN       1

/*************************************************************
 ** Cmd's async (atomic) flags
 *************************************************************/

/* Set if the cmd is aborted and ABORTED sense will be sent as the result */
#define SCST_CMD_ABORTED		0

/* Set if the cmd is aborted by other initiator */
#define SCST_CMD_ABORTED_OTHER		1

/* Set if the cmd is aborted and counted in cmd_done_wait_count */
#define SCST_CMD_DONE_COUNTED		2

/* Set if no response should be sent to the target about this cmd */
#define SCST_CMD_NO_RESP		3

/* Set if the cmd is dead and can be destroyed at any time */
#define SCST_CMD_CAN_BE_DESTROYED	4

/*************************************************************
 ** Tgt_dev's async. flags (tgt_dev_flags)
 *************************************************************/

/* Set if tgt_dev has Unit Attention sense */
#define SCST_TGT_DEV_UA_PENDING		0

/* Set if tgt_dev is RESERVED by another session */
#define SCST_TGT_DEV_RESERVED		1

/* Set if the corresponding context is atomic */
#define SCST_TGT_DEV_AFTER_INIT_WR_ATOMIC	5
#define SCST_TGT_DEV_AFTER_INIT_OTH_ATOMIC	6
#define SCST_TGT_DEV_AFTER_RESTART_WR_ATOMIC	7
#define SCST_TGT_DEV_AFTER_RESTART_OTH_ATOMIC	8
#define SCST_TGT_DEV_AFTER_RX_DATA_ATOMIC	9
#define SCST_TGT_DEV_AFTER_EXEC_ATOMIC		10

#define SCST_TGT_DEV_CLUST_POOL			11

/*************************************************************
 ** Name of the entry in /proc
 *************************************************************/
#define SCST_PROC_ENTRY_NAME         "scsi_tgt"

/*************************************************************
 ** Activities suspending timeout
 *************************************************************/
#define SCST_SUSPENDING_TIMEOUT			(90 * HZ)

/*************************************************************
 ** Kernel cache creation helper
 *************************************************************/
#ifndef KMEM_CACHE
#define KMEM_CACHE(__struct, __flags) kmem_cache_create(#__struct,\
	sizeof(struct __struct), __alignof__(struct __struct),\
	(__flags), NULL, NULL)
#endif

/*************************************************************
 ** Vlaid_mask constants for scst_analyze_sense()
 *************************************************************/

#define SCST_SENSE_KEY_VALID		1
#define SCST_SENSE_ASC_VALID		2
#define SCST_SENSE_ASCQ_VALID		4

#define SCST_SENSE_ASCx_VALID		(SCST_SENSE_ASC_VALID | \
					 SCST_SENSE_ASCQ_VALID)

#define SCST_SENSE_ALL_VALID		(SCST_SENSE_KEY_VALID | \
					 SCST_SENSE_ASC_VALID | \
					 SCST_SENSE_ASCQ_VALID)

/*************************************************************
 *                     TYPES
 *************************************************************/

struct scst_tgt;
struct scst_session;
struct scst_cmd;
struct scst_mgmt_cmd;
struct scst_device;
struct scst_tgt_dev;
struct scst_dev_type;
struct scst_acg;
struct scst_acg_dev;
struct scst_acn;
struct scst_aen;

/*
 * SCST uses 64-bit numbers to represent LUN's internally. The value
 * NO_SUCH_LUN is guaranteed to be different of every valid LUN.
 */
#define NO_SUCH_LUN ((uint64_t)-1)

typedef enum dma_data_direction scst_data_direction;

enum scst_cdb_flags {
	/* SCST_TRANSFER_LEN_TYPE_FIXED must be equiv 1 (FIXED_BIT in cdb) */
	SCST_TRANSFER_LEN_TYPE_FIXED = 0x01,
	SCST_SMALL_TIMEOUT = 0x02,
	SCST_LONG_TIMEOUT = 0x04,
	SCST_UNKNOWN_LENGTH = 0x08,
	SCST_INFO_INVALID = 0x10,
	SCST_VERIFY_BYTCHK_MISMATCH_ALLOWED = 0x20,
};

/*
 * Scsi_Target_Template: defines what functions a target driver will
 * have to provide in order to work with the target mid-level.
 * MUST HAVEs define functions that are expected to be in order to work.
 * OPTIONAL says that there is a choice.
 *
 * Also, pay attention to the fact that a command is BLOCKING or NON-BLOCKING.
 * NON-BLOCKING means that a function returns immediately and will not wait
 * for actual data transfer to finish. Blocking in such command could have
 * negative impact on overall system performance. If blocking is necessary,
 * it is worth to consider creating dedicated thread(s) in target driver, to
 * which the commands would be passed and which would perform blocking
 * operations instead of SCST.
 *
 * If the function allowed to sleep or not is determined by its last
 * argument, which is true, if sleeping is not allowed. In this case,
 * if the function requires sleeping, it  can return
 * SCST_TGT_RES_NEED_THREAD_CTX, and it will be recalled in the thread context,
 * where sleeping is allowed.
 */
struct scst_tgt_template {
	/* public: */

	/*
	 * SG tablesize allows to check whether scatter/gather can be used
	 * or not.
	 */
	int sg_tablesize;

	/*
	 * True, if this target adapter uses unchecked DMA onto an ISA bus.
	 */
	unsigned unchecked_isa_dma:1;

	/*
	 * True, if this target adapter can benefit from using SG-vector
	 * clustering (i.e. smaller number of segments).
	 */
	unsigned use_clustering:1;

	/*
	 * True, if this target adapter doesn't support SG-vector clustering
	 */
	unsigned no_clustering:1;

	/*
	 * True, if corresponding function supports execution in
	 * the atomic (non-sleeping) context
	 */
	unsigned xmit_response_atomic:1;
	unsigned rdy_to_xfer_atomic:1;

	/* True, if the template doesn't need the entry in /proc */
	unsigned no_proc_entry:1;

	/*
	 * This function is equivalent to the SCSI
	 * queuecommand. The target should transmit the response
	 * buffer and the status in the scst_cmd struct.
	 * The expectation is that this executing this command is NON-BLOCKING.
	 *
	 * After the response is actually transmitted, the target
	 * should call the scst_tgt_cmd_done() function of the
	 * mid-level, which will allow it to free up the command.
	 * Returns one of the SCST_TGT_RES_* constants.
	 *
	 * Pay attention to "atomic" attribute of the cmd, which can be get
	 * by scst_cmd_atomic(): it is true if the function called in the
	 * atomic (non-sleeping) context.
	 *
	 * MUST HAVE
	 */
	int (*xmit_response) (struct scst_cmd *cmd);

	/*
	 * This function informs the driver that data
	 * buffer corresponding to the said command have now been
	 * allocated and it is OK to receive data for this command.
	 * This function is necessary because a SCSI target does not
	 * have any control over the commands it receives. Most lower
	 * level protocols have a corresponding function which informs
	 * the initiator that buffers have been allocated e.g., XFER_
	 * RDY in Fibre Channel. After the data is actually received
	 * the low-level driver needs to call scst_rx_data() in order to
	 * continue processing this command.
	 * Returns one of the SCST_TGT_RES_* constants.
	 * This command is expected to be NON-BLOCKING.
	 *
	 * Pay attention to "atomic" attribute of the cmd, which can be get
	 * by scst_cmd_atomic(): it is true if the function called in the
	 * atomic (non-sleeping) context.
	 *
	 * OPTIONAL
	 */
	int (*rdy_to_xfer) (struct scst_cmd *cmd);

	/*
	 * Called to notify the driver that the command is about to be freed.
	 * Necessary, because for aborted commands xmit_response() could not
	 * be called. Could be called on IRQ context.
	 *
	 * OPTIONAL
	 */
	void (*on_free_cmd) (struct scst_cmd *cmd);

	/*
	 * This function allows target driver to handle data buffer
	 * allocations on its own.
	 *
	 * Target driver doesn't have to always allocate buffer in this
	 * function, but if it decide to do it, it must check that
	 * scst_cmd_get_data_buff_alloced() returns 0, otherwise to avoid
	 * double buffer allocation and memory leaks alloc_data_buf() shall
	 * fail.
	 *
	 * Shall return 0 in case of success or < 0 (preferrably -ENOMEM)
	 * in case of error, or > 0 if the regular SCST allocation should be
	 * done. In case of returning successfully,
	 * scst_cmd->tgt_data_buf_alloced will be set by SCST.
	 *
	 * It is possible that both target driver and dev handler request own
	 * memory allocation. In this case, data will be memcpy() between
	 * buffers, where necessary.
	 *
	 * If allocation in atomic context - cf. scst_cmd_atomic() - is not
	 * desired or fails and consequently < 0 is returned, this function
	 * will be re-called in thread context.
	 *
	 * Please note that the driver will have to handle itself all relevant
	 * details such as scatterlist setup, highmem, freeing the allocated
	 * memory, etc.
	 *
	 * OPTIONAL.
	 */
	int (*alloc_data_buf) (struct scst_cmd *cmd);

	/*
	 * This function informs the driver that data
	 * buffer corresponding to the said command have now been
	 * allocated and other preprocessing tasks have been done.
	 * A target driver could need to do some actions at this stage.
	 * After the target driver done the needed actions, it shall call
	 * scst_restart_cmd() in order to continue processing this command.
	 *
	 * Called only if the cmd is queued using scst_cmd_init_stage1_done()
	 * instead of scst_cmd_init_done().
	 *
	 * Returns void, the result is expected to be returned using
	 * scst_restart_cmd().
	 *
	 * This command is expected to be NON-BLOCKING.
	 *
	 * Pay attention to "atomic" attribute of the cmd, which can be get
	 * by scst_cmd_atomic(): it is true if the function called in the
	 * atomic (non-sleeping) context.
	 *
	 * OPTIONAL.
	 */
	void (*preprocessing_done) (struct scst_cmd *cmd);

	/*
	 * This function informs the driver that the said command is about
	 * to be executed.
	 *
	 * Returns one of the SCST_PREPROCESS_* constants.
	 *
	 * This command is expected to be NON-BLOCKING.
	 *
	 * Pay attention to "atomic" attribute of the cmd, which can be get
	 * by scst_cmd_atomic(): it is true if the function called in the
	 * atomic (non-sleeping) context.
	 *
	 * OPTIONAL
	 */
	int (*pre_exec) (struct scst_cmd *cmd);

	/*
	 * This function informs the driver that all affected by the
	 * corresponding task management function commands have beed completed.
	 * No return value expected.
	 *
	 * This function is expected to be NON-BLOCKING.
	 *
	 * Called without any locks held from a thread context.
	 *
	 * OPTIONAL
	 */
	void (*task_mgmt_affected_cmds_done) (struct scst_mgmt_cmd *mgmt_cmd);

	/*
	 * This function informs the driver that the corresponding task
	 * management function has been completed, i.e. all the corresponding
	 * commands completed and freed. No return value expected.
	 *
	 * This function is expected to be NON-BLOCKING.
	 *
	 * Called without any locks held from a thread context.
	 *
	 * MUST HAVE if the target supports task management.
	 */
	void (*task_mgmt_fn_done) (struct scst_mgmt_cmd *mgmt_cmd);

	/*
	 * This function should detect the target adapters that
	 * are present in the system. The function should return a value
	 * >= 0 to signify the number of detected target adapters.
	 * A negative value should be returned whenever there is
	 * an error.
	 *
	 * MUST HAVE
	 */
	int (*detect) (struct scst_tgt_template *tgt_template);

	/*
	 * This function should free up the resources allocated to the device.
	 * The function should return 0 to indicate successful release
	 * or a negative value if there are some issues with the release.
	 * In the current version the return value is ignored.
	 *
	 * MUST HAVE
	 */
	int (*release) (struct scst_tgt *tgt);

	/*
	 * This function is used for Asynchronous Event Notifications.
	 *
	 * Returns one of the SCST_AEN_RES_* constants.
	 * After AEN is sent, target driver must call scst_aen_done() and,
	 * optionally, scst_set_aen_delivery_status().
	 *
	 * This command is expected to be NON-BLOCKING, but can sleep.
	 *
	 * MUST HAVE, if low-level protocol supports AENs.
	 */
	int (*report_aen) (struct scst_aen *aen);

	/*
	 * Those functions can be used to export the driver's statistics and
	 * other infos to the world outside the kernel as well as to get some
	 * management commands from it.
	 *
	 * OPTIONAL
	 */
	int (*read_proc) (struct seq_file *seq, struct scst_tgt *tgt);
	int (*write_proc) (char *buffer, char **start, off_t offset,
		int length, int *eof, struct scst_tgt *tgt);

	/*
	 * Name of the template. Must be unique to identify
	 * the template. MUST HAVE
	 */
	const char name[SCST_MAX_NAME];

	/*
	 * Number of additional threads to the pool of dedicated threads.
	 * Used if xmit_response() or rdy_to_xfer() is blocking.
	 * It is the target driver's duty to ensure that not more, than that
	 * number of threads, are blocked in those functions at any time.
	 */
	int threads_num;

	/* Private, must be inited to 0 by memset() */

	/* List of targets per template, protected by scst_mutex */
	struct list_head tgt_list;

	/* List entry of global templates list */
	struct list_head scst_template_list_entry;

	/* The pointer to the /proc directory entry */
	struct proc_dir_entry *proc_tgt_root;

	/* Device number in /proc */
	int proc_dev_num;
};

struct scst_dev_type {
	/* SCSI type of the supported device. MUST HAVE */
	int type;

	/*
	 * True, if corresponding function supports execution in
	 * the atomic (non-sleeping) context
	 */
	unsigned parse_atomic:1;
	unsigned exec_atomic:1;
	unsigned dev_done_atomic:1;

	/* Set, if no /proc files should be automatically created by SCST */
	unsigned no_proc:1;

	/*
	 * Should be set, if exec() is synchronous. This is a hint to SCST core
	 * to optimize commands order management.
	 */
	unsigned exec_sync:1;

	/*
	 * Called to parse CDB from the cmd and initialize
	 * cmd->bufflen and cmd->data_direction (both - REQUIRED).
	 * Returns the command's next state or SCST_CMD_STATE_DEFAULT,
	 * if the next default state should be used, or
	 * SCST_CMD_STATE_NEED_THREAD_CTX if the function called in atomic
	 * context, but requires sleeping, or SCST_CMD_STATE_STOP if the
	 * command should not be further processed for now. In the
	 * SCST_CMD_STATE_NEED_THREAD_CTX case the function
	 * will be recalled in the thread context, where sleeping is allowed.
	 *
	 * Pay attention to "atomic" attribute of the cmd, which can be get
	 * by scst_cmd_atomic(): it is true if the function called in the
	 * atomic (non-sleeping) context.
	 *
	 * MUST HAVE
	 */
	int (*parse) (struct scst_cmd *cmd);

	/*
	 * Called to execute CDB. Useful, for instance, to implement
	 * data caching. The result of CDB execution is reported via
	 * cmd->scst_cmd_done() callback.
	 * Returns:
	 *  - SCST_EXEC_COMPLETED - the cmd is done, go to other ones
	 *  - SCST_EXEC_NEED_THREAD - thread context is required to execute
	 *	the command. Exec() will be called again in the thread context.
	 *  - SCST_EXEC_NOT_COMPLETED - the cmd should be sent to SCSI
	 *	mid-level.
	 *
	 * Pay attention to "atomic" attribute of the cmd, which can be get
	 * by scst_cmd_atomic(): it is true if the function called in the
	 * atomic (non-sleeping) context.
	 *
	 * If this function provides sync execution, you should set
	 * exec_sync flag and consider to setup dedicated threads by
	 * setting threads_num > 0.
	 *
	 * !! If this function is implemented, scst_check_local_events() !!
	 * !! shall be called inside it just before the actual command's !!
	 * !! execution.                                                 !!
	 *
	 * OPTIONAL, if not set, the commands will be sent directly to SCSI
	 * device.
	 */
	int (*exec) (struct scst_cmd *cmd);

	/*
	 * Called to notify dev handler about the result of cmd execution
	 * and perform some post processing. Cmd's fields is_send_status and
	 * resp_data_len should be set by this function, but SCST offers good
	 * defaults.
	 * Returns the command's next state or SCST_CMD_STATE_DEFAULT,
	 * if the next default state should be used, or
	 * SCST_CMD_STATE_NEED_THREAD_CTX if the function called in atomic
	 * context, but requires sleeping. In the last case, the function
	 * will be recalled in the thread context, where sleeping is allowed.
	 *
	 * Pay attention to "atomic" attribute of the cmd, which can be get
	 * by scst_cmd_atomic(): it is true if the function called in the
	 * atomic (non-sleeping) context.
	 */
	int (*dev_done) (struct scst_cmd *cmd);

	/*
	 * Called to notify dev hander that the command is about to be freed.
	 * Could be called on IRQ context.
	 */
	void (*on_free_cmd) (struct scst_cmd *cmd);

	/*
	 * Called to execute a task management command.
	 * Returns:
	 *  - SCST_MGMT_STATUS_SUCCESS - the command is done with success,
	 *	no firther actions required
	 *  - The SCST_MGMT_STATUS_* error code if the command is failed and
	 *	no further actions required
	 *  - SCST_DEV_TM_NOT_COMPLETED - regular standard actions for the
	 *      command should be done
	 *
	 * Called without any locks held from a thread context.
	 */
	int (*task_mgmt_fn) (struct scst_mgmt_cmd *mgmt_cmd,
		struct scst_tgt_dev *tgt_dev);

	/*
	 * Called when new device is attaching to the dev handler
	 * Returns 0 on success, error code otherwise.
	 */
	int (*attach) (struct scst_device *dev);

	/* Called when new device is detaching from the dev handler */
	void (*detach) (struct scst_device *dev);

	/*
	 * Called when new tgt_dev (session) is attaching to the dev handler.
	 * Returns 0 on success, error code otherwise.
	 */
	int (*attach_tgt) (struct scst_tgt_dev *tgt_dev);

	/* Called when tgt_dev (session) is detaching from the dev handler */
	void (*detach_tgt) (struct scst_tgt_dev *tgt_dev);

	/*
	 * Those functions can be used to export the handler's statistics and
	 * other infos to the world outside the kernel as well as to get some
	 * management commands from it.
	 *
	 * OPTIONAL
	 */
	int (*read_proc) (struct seq_file *seq, struct scst_dev_type *dev_type);
	int (*write_proc) (char *buffer, char **start, off_t offset,
		int length, int *eof, struct scst_dev_type *dev_type);

	/*
	 * Name of the dev handler. Must be unique. MUST HAVE.
	 *
	 * It's SCST_MAX_NAME + few more bytes to match scst_user expectations.
	 */
	char name[SCST_MAX_NAME + 10];

	/*
	 * Number of dedicated threads. If 0 - no dedicated threads will
	 * be created, if <0 - creation of dedicated threads is prohibited.
	 */
	int threads_num;

	struct module *module;

	/* private: */

	/* list entry in scst_dev_type_list */
	struct list_head dev_type_list_entry;

	/* The pointer to the /proc directory entry */
	struct proc_dir_entry *proc_dev_type_root;
};

struct scst_tgt {
	/* List of remote sessions per target, protected by scst_mutex */
	struct list_head sess_list;

	/* List entry of targets per template (tgts_list) */
	struct list_head tgt_list_entry;

	struct scst_tgt_template *tgtt;	/* corresponding target template */

	/*
	 * Maximum SG table size. Needed here, since different cards on the
	 * same target template can have different SG table limitations.
	 */
	int sg_tablesize;

	/* Used for storage of target driver private stuff */
	void *tgt_priv;

	/*
	 * The following fields used to store and retry cmds if
	 * target's internal queue is full, so the target is unable to accept
	 * the cmd returning QUEUE FULL
	 */
	bool retry_timer_active;
	struct timer_list retry_timer;
	atomic_t finished_cmds;
	int retry_cmds;		/* protected by tgt_lock */
	spinlock_t tgt_lock;
	struct list_head retry_cmd_list; /* protected by tgt_lock */

	/* Used to wait until session finished to unregister */
	wait_queue_head_t unreg_waitQ;

	/* Device number in /proc */
	int proc_num;

	/* Name on the default security group ("Default_target_name") */
	char *default_group_name;
};

/* Hash size and hash fn for hash based lun translation */
#define	TGT_DEV_HASH_SHIFT	5
#define	TGT_DEV_HASH_SIZE	(1<<TGT_DEV_HASH_SHIFT)
#define	HASH_VAL(_val)		(_val & (TGT_DEV_HASH_SIZE - 1))

struct scst_session {
	/*
	 * Initialization phase, one of SCST_SESS_IPH_* constants, protected by
	 * sess_list_lock
	 */
	int init_phase;

	struct scst_tgt *tgt;	/* corresponding target */

	/* Used for storage of target driver private stuff */
	void *tgt_priv;

	/*
	 * Hash list of tgt_dev's for this session, protected by scst_mutex
	 * and suspended activity
	 */
	struct list_head sess_tgt_dev_list_hash[TGT_DEV_HASH_SIZE];

	/*
	 * List of cmds in this session. Used to find a cmd in the
	 * session. Protected by sess_list_lock.
	 */
	struct list_head search_cmd_list;

	atomic_t refcnt;		/* get/put counter */

	/*
	 * Alive commands for this session. ToDo: make it part of the common
	 * IO flow control.
	 */
	atomic_t sess_cmd_count;

	spinlock_t sess_list_lock; /* protects search_cmd_list, etc */

	/* Access control for this session and list entry there */
	struct scst_acg *acg;

	/* List entry for the sessions list inside ACG */
	struct list_head acg_sess_list_entry;

	/* Name of attached initiator */
	const char *initiator_name;

	/* List entry of sessions per target */
	struct list_head sess_list_entry;

	/* List entry for the list that keeps session, waiting for the init */
	struct list_head sess_init_list_entry;

	/*
	 * List entry for the list that keeps session, waiting for the shutdown
	 */
	struct list_head sess_shut_list_entry;

	/*
	 * Lists of deffered during session initialization commands.
	 * Protected by sess_list_lock.
	 */
	struct list_head init_deferred_cmd_list;
	struct list_head init_deferred_mcmd_list;

	/*
	 * Shutdown phase, one of SCST_SESS_SPH_* constants, unprotected.
	 * Async. relating to init_phase, must be a separate variable, because
	 * session could be unregistered before async. registration is finished.
	 */
	unsigned long shut_phase;

	/* Used if scst_unregister_session() called in wait mode */
	struct completion *shutdown_compl;

	/*
	 * Functions and data for user callbacks from scst_register_session()
	 * and scst_unregister_session()
	 */
	void *reg_sess_data;
	void (*init_result_fn) (struct scst_session *sess, void *data,
				int result);
	void (*unreg_done_fn) (struct scst_session *sess);

#ifdef CONFIG_SCST_MEASURE_LATENCY /* must be last */
	spinlock_t meas_lock;
	uint64_t scst_time, processing_time;
	unsigned int processed_cmds;
#endif
};

struct scst_cmd_lists {
	spinlock_t cmd_list_lock;
	struct list_head active_cmd_list;
	wait_queue_head_t cmd_list_waitQ;
	struct list_head lists_list_entry;
};

struct scst_cmd {
	/* List entry for below *_cmd_lists */
	struct list_head cmd_list_entry;

	/* Pointer to lists of commands with the lock */
	struct scst_cmd_lists *cmd_lists;

	atomic_t cmd_ref;

	struct scst_session *sess;	/* corresponding session */

	/* Cmd state, one of SCST_CMD_STATE_* constants */
	int state;

	/*************************************************************
	 ** Cmd's flags
	 *************************************************************/

	/*
	 * Set if expected_sn should be incremented, i.e. cmd was sent
	 * for execution
	 */
	unsigned int sent_for_exec:1;

	/* Set if the cmd's action is completed */
	unsigned int completed:1;

	/* Set if we should ignore Unit Attention in scst_check_sense() */
	unsigned int ua_ignore:1;

	/* Set if cmd is being processed in atomic context */
	unsigned int atomic:1;

	/* Set if this command was sent in double UA possible state */
	unsigned int double_ua_possible:1;

	/* Set if this command contains status */
	unsigned int is_send_status:1;

	/* Set if cmd is being retried */
	unsigned int retry:1;

	/* Set if cmd is internally generated */
	unsigned int internal:1;

	/* Set if the device was blocked by scst_inc_on_dev_cmd() (for debug) */
	unsigned int inc_blocking:1;

	/* Set if the device should be unblocked after cmd's finish */
	unsigned int needs_unblocking:1;

	/* Set if scst_dec_on_dev_cmd() call is needed on the cmd's finish */
	unsigned int dec_on_dev_needed:1;

	/*
	 * Set if the target driver wants to alloc data buffers on its own.
	 * In this case alloc_data_buf() must be provided in the target driver
	 * template.
	 */
	unsigned int tgt_need_alloc_data_buf:1;

	/*
	 * Set by SCST if the custom data buffer allocation by the target driver
	 * succeeded.
	 */
	unsigned int tgt_data_buf_alloced:1;

	/* Set if custom data buffer allocated by dev handler */
	unsigned int dh_data_buf_alloced:1;

	/* Set if the target driver called scst_set_expected() */
	unsigned int expected_values_set:1;

	/*
	 * Set if the cmd was delayed by task management debugging code.
	 * Used only if CONFIG_SCST_DEBUG_TM is on.
	 */
	unsigned int tm_dbg_delayed:1;

	/*
	 * Set if the cmd must be ignored by task management debugging code.
	 * Used only if CONFIG_SCST_DEBUG_TM is on.
	 */
	unsigned int tm_dbg_immut:1;

	/*
	 * Set if the SG buffer was modified by scst_set_resp_data_len()
	 */
	unsigned int sg_buff_modified:1;

	/*
	 * Set if scst_cmd_init_stage1_done() called and the target
	 * want that preprocessing_done() will be called
	 */
	unsigned int preprocessing_only:1;

	/* Set if cmd's SN was set */
	unsigned int sn_set:1;

	/* Set if hq_cmd_count was incremented */
	unsigned int hq_cmd_inced:1;

	/*
	 * Set if scst_cmd_init_stage1_done() called and the target wants
	 * that the SN for the cmd won't be assigned until scst_restart_cmd()
	 */
	unsigned int set_sn_on_restart_cmd:1;

	/* Set if the cmd's must not use sgv cache for data buffer */
	unsigned int no_sgv:1;

	/*
	 * Set if target driver may need to call dma_sync_sg() or similar
	 * function before transferring cmd' data to the target device
	 * via DMA.
	 */
	unsigned int may_need_dma_sync:1;

	/* Set if the cmd was done or aborted out of its SN */
	unsigned int out_of_sn:1;

	/* Set if increment expected_sn in cmd->scst_cmd_done() */
	unsigned int inc_expected_sn_on_done:1;

	/* Set if tgt_sn field is valid */
	unsigned int tgt_sn_set:1;

	/* Set if cmd is done */
	unsigned int done:1;

	/* Set if cmd is finished */
	unsigned int finished:1;

	/**************************************************************/

	unsigned long cmd_flags; /* cmd's async flags */

	/* Keeps status of cmd's status/data delivery to remote initiator */
	int delivery_status;

	struct scst_tgt_template *tgtt;	/* to save extra dereferences */
	struct scst_tgt *tgt;		/* to save extra dereferences */
	struct scst_device *dev;	/* to save extra dereferences */

	struct scst_tgt_dev *tgt_dev;	/* corresponding device for this cmd */

	uint64_t lun;			/* LUN for this cmd */

	unsigned long start_time;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
	struct scsi_request *scsi_req;	/* SCSI request */
#endif

	/* List entry for tgt_dev's SN related lists */
	struct list_head sn_cmd_list_entry;

	/* Cmd's serial number, used to execute cmd's in order of arrival */
	unsigned long sn;

	/* The corresponding sn_slot in tgt_dev->sn_slots */
	atomic_t *sn_slot;

	/* List entry for session's search_cmd_list */
	struct list_head search_cmd_list_entry;

	/*
	 * Used to found the cmd by scst_find_cmd_by_tag(). Set by the
	 * target driver on the cmd's initialization time
	 */
	uint64_t tag;

	uint32_t tgt_sn; /* SN set by target driver (for TM purposes) */

	/* CDB and its len */
	uint8_t cdb[SCST_MAX_CDB_SIZE];
	short cdb_len; /* it might be -1 */
	unsigned short ext_cdb_len;
	uint8_t *ext_cdb;

	enum scst_cdb_flags op_flags;
	const char *op_name;

	enum scst_cmd_queue_type queue_type;

	int timeout; /* CDB execution timeout in seconds */
	int retries; /* Amount of retries that will be done by SCSI mid-level */

	/* SCSI data direction, one of SCST_DATA_* constants */
	scst_data_direction data_direction;

	/* Remote initiator supplied values, if any */
	scst_data_direction expected_data_direction;
	int expected_transfer_len;

	/*
	 * Cmd data length. Could be different from bufflen for commands like
	 * VERIFY, which transfer different amount of data (if any), than
	 * processed.
	 */
	int data_len;

	/* Completition routine */
	void (*scst_cmd_done) (struct scst_cmd *cmd, int next_state,
		enum scst_exec_context pref_context);

	struct sgv_pool_obj *sgv;	/* sgv object */

	int bufflen;			/* cmd buffer length */
	struct scatterlist *sg;		/* cmd data buffer SG vector */
	int sg_cnt;			/* SG segments count */

	/* scst_get_sg_buf_[first,next]() support */
	int get_sg_buf_entry_num;

	/*
	 * Response data length in data buffer. This field must not be set
	 * directly, use scst_set_resp_data_len() for that
	 */
	int resp_data_len;

	/*
	 * Used if both target driver and dev handler request own memory
	 * allocation. In other cases, both are equal to sg and sg_cnt
	 * correspondingly.
	 *
	 * If target driver requests own memory allocations, it MUST use
	 * functions scst_cmd_get_tgt_sg*() to get sg and sg_cnt! Otherwise,
	 * it may use functions scst_cmd_get_sg*().
	 */
	struct scatterlist *tgt_sg;
	int tgt_sg_cnt;

	/*
	 * The status fields in case of errors must be set using
	 * scst_set_cmd_error_status()!
	 */
	uint8_t status;		/* status byte from target device */
	uint8_t msg_status;	/* return status from host adapter itself */
	uint8_t host_status;	/* set by low-level driver to indicate status */
	uint8_t driver_status;	/* set by mid-level */

	uint8_t *sense;		/* pointer to sense buffer */
	unsigned short sense_bufflen; /* length of the sense buffer, if any */

	/* Used for storage of target driver private stuff */
	void *tgt_priv;

	/* Used for storage of dev handler private stuff */
	void *dh_priv;

	/*
	 * Used to restore the SG vector if it was modified by
	 * scst_set_resp_data_len()
	 */
	int orig_sg_cnt, orig_sg_entry, orig_entry_len;

	/* Used to retry commands in case of double UA */
	int dbl_ua_orig_resp_data_len, dbl_ua_orig_data_direction;

	/* List corresponding mgmt cmd, if any, protected by sess_list_lock */
	struct list_head mgmt_cmd_list;

	/* List entry for dev's blocked_cmd_list */
	struct list_head blocked_cmd_list_entry;

	struct scst_cmd *orig_cmd; /* Used to issue REQUEST SENSE */

#ifdef CONFIG_SCST_MEASURE_LATENCY /* must be last */
	uint64_t start, pre_exec_finish, post_exec_start;
#endif
};

struct scst_rx_mgmt_params {
	int fn;
	uint64_t tag;
	const uint8_t *lun;
	int lun_len;
	uint32_t cmd_sn;
	int atomic;
	void *tgt_priv;
	unsigned char tag_set;
	unsigned char lun_set;
	unsigned char cmd_sn_set;
};

struct scst_mgmt_cmd_stub {
	struct scst_mgmt_cmd *mcmd;

	/* List entry in cmd->mgmt_cmd_list */
	struct list_head cmd_mgmt_cmd_list_entry;
};

struct scst_mgmt_cmd {
	/* List entry for *_mgmt_cmd_list */
	struct list_head mgmt_cmd_list_entry;

	struct scst_session *sess;

	/* Mgmt cmd state, one of SCST_MCMD_STATE_* constants */
	int state;

	int fn;

	unsigned int completed:1;	/* set, if the mcmd is completed */
	/* Set if device(s) should be unblocked after mcmd's finish */
	unsigned int needs_unblocking:1;
	unsigned int lun_set:1;		/* set, if lun field is valid */
	unsigned int cmd_sn_set:1;	/* set, if cmd_sn field is valid */
	/* set, if scst_mgmt_affected_cmds_done was called */
	unsigned int affected_cmds_done_called:1;

	/*
	 * Number of commands to finish before sending response,
	 * protected by scst_mcmd_lock
	 */
	int cmd_finish_wait_count;

	/*
	 * Number of commands to complete (done) before resetting reservation,
	 * protected by scst_mcmd_lock
	 */
	int cmd_done_wait_count;

	/* Number of completed commands, protected by scst_mcmd_lock */
	int completed_cmd_count;

	uint64_t lun;	/* LUN for this mgmt cmd */
	/* or (and for iSCSI) */
	uint64_t tag;	/* tag of the corresponding cmd */

	uint32_t cmd_sn; /* affected command's highest SN */

	/* corresponding cmd (to be aborted, found by tag) */
	struct scst_cmd *cmd_to_abort;

	/* corresponding device for this mgmt cmd (found by lun) */
	struct scst_tgt_dev *mcmd_tgt_dev;

	/* completition status, one of the SCST_MGMT_STATUS_* constants */
	int status;

	/* Used for storage of target driver private stuff */
	void *tgt_priv;
};

struct scst_device {
	struct scst_dev_type *handler;	/* corresponding dev handler */

	struct scst_mem_lim dev_mem_lim;

	unsigned short type;	/* SCSI type of the device */

	/*************************************************************
	 ** Dev's flags. Updates serialized by dev_lock or suspended
	 ** activity
	 *************************************************************/

	/* Set if dev is RESERVED */
	unsigned short dev_reserved:1;

	/* Set if double reset UA is possible */
	unsigned short dev_double_ua_possible:1;

	/**************************************************************/

	/*************************************************************
	 ** Dev's control mode page related values. Updates serialized
	 ** by scst_block_dev(). It's long to not interfere with the
	 ** above flags.
	 *************************************************************/

	unsigned long queue_alg:4;
	unsigned long tst:3;
	unsigned long tas:1;
	unsigned long swp:1;
	unsigned long d_sense:1;

	/*
	 * Set if device implements own ordered commands management. If not set
	 * and queue_alg is SCST_CONTR_MODE_QUEUE_ALG_RESTRICTED_REORDER,
	 * expected_sn will be incremented only after commands finished.
	 */
	unsigned long has_own_order_mgmt:1;

	/**************************************************************/

	/* Used for storage of dev handler private stuff */
	void *dh_priv;

	/* Used to translate SCSI's cmd to SCST's cmd */
	struct gendisk *rq_disk;

	/* Corresponding real SCSI device, could be NULL for virtual devices */
	struct scsi_device *scsi_dev;

	/* Pointer to lists of commands with the lock */
	struct scst_cmd_lists *p_cmd_lists;

	/* Lists of commands with lock, if dedicated threads are used */
	struct scst_cmd_lists cmd_lists;

	/* Per-device dedicated IO context */
	struct io_context *dev_io_ctx;

	/* How many cmds alive on this dev */
	atomic_t dev_cmd_count;

	/* How many write cmds alive on this dev. Temporary, ToDo */
	atomic_t write_cmd_count;

	spinlock_t dev_lock;		/* device lock */

	/*
	 * How many times device was blocked for new cmds execution.
	 * Protected by dev_lock
	 */
	int block_count;

	/*
	 * How many there are "on_dev" commands, i.e. ones those are being
	 * executed by the underlying SCSI/virtual device.
	 */
	atomic_t on_dev_count;

	struct list_head blocked_cmd_list; /* protected by dev_lock */

	/* Used to wait for requested amount of "on_dev" commands */
	wait_queue_head_t on_dev_waitQ;

	/* A list entry used during TM, protected by scst_mutex */
	struct list_head tm_dev_list_entry;

	/* Virtual device internal ID */
	int virt_id;

	/* Pointer to virtual device name, for convenience only */
	const char *virt_name;

	/* List entry in global devices list */
	struct list_head dev_list_entry;

	/*
	 * List of tgt_dev's, one per session, protected by scst_mutex or
	 * dev_lock for reads and both for writes
	 */
	struct list_head dev_tgt_dev_list;

	/* List of acg_dev's, one per acg, protected by scst_mutex */
	struct list_head dev_acg_dev_list;

	/* List of dedicated threads, protected by scst_mutex */
	struct list_head threads_list;

	/* Device number */
	int dev_num;
};

/*
 * Used to store threads local tgt_dev specific data
 */
struct scst_thr_data_hdr {
	/* List entry in tgt_dev->thr_data_list */
	struct list_head thr_data_list_entry;
	struct task_struct *owner_thr; /* the owner thread */
	atomic_t ref;
	/* Function that will be called on the tgt_dev destruction */
	void (*free_fn) (struct scst_thr_data_hdr *data);
};

/*
 * Used to store per-session specific device information
 */
struct scst_tgt_dev {
	/* List entry in sess->sess_tgt_dev_list_hash */
	struct list_head sess_tgt_dev_list_entry;

	struct scst_device *dev; /* to save extra dereferences */
	uint64_t lun;		 /* to save extra dereferences */

	gfp_t gfp_mask;
	struct sgv_pool *pool;
	int max_sg_cnt;

	unsigned long tgt_dev_flags;	/* tgt_dev's async flags */

	/* Used for storage of dev handler private stuff */
	void *dh_priv;

	/* How many cmds alive on this dev in this session */
	atomic_t tgt_dev_cmd_count;

	/*
	 * Used to execute cmd's in order of arrival, honoring SCSI task
	 * attributes.
	 *
	 * Protected by sn_lock, except expected_sn, which is protected by
	 * itself. Curr_sn must have the same size as expected_sn to
	 * overflow simultaneously.
	 */
	int def_cmd_count;
	spinlock_t sn_lock;
	unsigned long expected_sn;
	unsigned long curr_sn;
	int hq_cmd_count;
	struct list_head deferred_cmd_list;
	struct list_head skipped_sn_list;

	/*
	 * Set if the prev cmd was ORDERED. Size must allow unprotected
	 * modifications independant to the neighbour fields.
	 */
	unsigned long prev_cmd_ordered;

	int num_free_sn_slots; /* if it's <0, then all slots are busy */
	atomic_t *cur_sn_slot;
	atomic_t sn_slots[15];

	/* List of scst_thr_data_hdr and lock */
	spinlock_t thr_data_lock;
	struct list_head thr_data_list;

	/* Per-(device, session) dedicated IO context */
	struct io_context *tgt_dev_io_ctx;

	spinlock_t tgt_dev_lock;	/* per-session device lock */

	/* List of UA's for this device, protected by tgt_dev_lock */
	struct list_head UA_list;

	struct scst_session *sess;	/* corresponding session */
	struct scst_acg_dev *acg_dev;	/* corresponding acg_dev */

	/* list entry in dev->dev_tgt_dev_list */
	struct list_head dev_tgt_dev_list_entry;

	/* internal tmp list entry */
	struct list_head extra_tgt_dev_list_entry;
};

/*
 * Used to store ACG-specific device information, like LUN
 */
struct scst_acg_dev {
	struct scst_device *dev; /* corresponding device */
	uint64_t lun;		/* device's LUN in this acg */
	unsigned int rd_only_flag:1; /* if != 0, then read only */
	struct scst_acg *acg;	/* parent acg */

	/* list entry in dev->dev_acg_dev_list */
	struct list_head dev_acg_dev_list_entry;

	/* list entry in acg->acg_dev_list */
	struct list_head acg_dev_list_entry;
};

/*
 * ACG - access control group. Used to store group related
 * control information.
 */
struct scst_acg {
	/* List of acg_dev's in this acg, protected by scst_mutex */
	struct list_head acg_dev_list;

	/* List of attached sessions, protected by scst_mutex */
	struct list_head acg_sess_list;

	/* List of attached acn's, protected by scst_mutex */
	struct list_head acn_list;

	/* List entry in scst_acg_list */
	struct list_head scst_acg_list_entry;

	/* Name of this acg */
	const char *acg_name;

	/* The pointer to the /proc directory entry */
	struct proc_dir_entry *acg_proc_root;
};

/*
 * ACN - access control name. Used to store names, by which
 * incoming sessions will be assigned to appropriate ACG.
 */
struct scst_acn {
	/* Initiator's name */
	const char *name;
	/* List entry in acg->acn_list */
	struct list_head acn_list_entry;
};

/*
 * Used to store per-session UNIT ATTENTIONs
 */
struct scst_tgt_dev_UA {
	/* List entry in tgt_dev->UA_list */
	struct list_head UA_list_entry;

	/* Set if UA is global for session */
	unsigned int global_UA:1;

	/* Unit Attention sense */
	uint8_t UA_sense_buffer[SCST_SENSE_BUFFERSIZE];
};

/* Used to deliver AENs */
struct scst_aen {
	int event_fn; /* AEN fn */

	struct scst_session *sess;	/* corresponding session */
	uint64_t lun;			/* corresponding LUN in SCSI form */

	union {
		/* SCSI AEN data */
		struct {
			int aen_sense_len;
			uint8_t aen_sense[SCST_STANDARD_SENSE_LEN];
		};
	};

	/* Keeps status of AEN's delivery to remote initiator */
	int delivery_status;
};

#ifndef smp_mb__after_set_bit
/* There is no smp_mb__after_set_bit() in the kernel */
#define smp_mb__after_set_bit()                 smp_mb()
#endif

/*
 * Registers target template
 * Returns 0 on success or appropriate error code otherwise
 */
int __scst_register_target_template(struct scst_tgt_template *vtt,
	const char *version);
static inline int scst_register_target_template(struct scst_tgt_template *vtt)
{
	return __scst_register_target_template(vtt, SCST_INTERFACE_VERSION);
}

/*
 * Unregisters target template
 */
void scst_unregister_target_template(struct scst_tgt_template *vtt);

/*
 * Registers and returns target adapter
 * Returns new target structure on success or NULL otherwise.
 *
 * If parameter "target_name" isn't NULL, then security group with name
 * "Default_##target_name", if created, will be used as the default
 * instead of "Default" one for all initiators not assigned to any other group.
 */
struct scst_tgt *scst_register(struct scst_tgt_template *vtt,
	const char *target_name);

/*
 * Unregisters target adapter
 */
void scst_unregister(struct scst_tgt *tgt);

/*
 * Registers and returns a session
 *
 * Returns new session on success or NULL otherwise
 *
 * Parameters:
 *   tgt    - target
 *   atomic - true, if the function called in the atomic context. If false,
 *	this function will block until the session registration is completed.
 *   initiator_name - remote initiator's name, any NULL-terminated string,
 *      e.g. iSCSI name, which used as the key to found appropriate access
 *      control group. Could be NULL, then "default" group is used.
 *      The groups are set up via /proc interface.
 *   data - any target driver supplied data
 *   result_fn - pointer to the function that will be
 *      asynchronously called when session initialization finishes.
 *      Can be NULL. Parameters:
 *       - sess - session
 *	 - data - target driver supplied to scst_register_session() data
 *       - result - session initialization result, 0 on success or
 *                  appropriate error code otherwise
 *
 * Note: A session creation and initialization is a complex task,
 *       which requires sleeping state, so it can't be fully done
 *       in interrupt context. Therefore the "bottom half" of it, if
 *       scst_register_session() is called from atomic context, will be
 *       done in SCST thread context. In this case scst_register_session()
 *       will return not completely initialized session, but the target
 *       driver can supply commands to this session via scst_rx_cmd().
 *       Those commands processing will be delayed inside SCST until
 *       the session initialization is finished, then their processing
 *       will be restarted. The target driver will be notified about
 *       finish of the session initialization by function result_fn().
 *       On success the target driver could do nothing, but if the
 *       initialization fails, the target driver must ensure that
 *       no more new commands being sent or will be sent to SCST after
 *       result_fn() returns. All already sent to SCST commands for
 *       failed session will be returned in xmit_response() with BUSY status.
 *       In case of failure the driver shall call scst_unregister_session()
 *       inside result_fn(), it will NOT be called automatically.
 */
struct scst_session *scst_register_session(struct scst_tgt *tgt, int atomic,
	const char *initiator_name, void *data,
	void (*result_fn) (struct scst_session *sess, void *data, int result));

/*
 * Unregisters a session.
 * Parameters:
 *   sess - session to be unregistered
 *   wait - if true, instructs to wait until all commands, which
 *      currently is being executed and belonged to the session, finished.
 *      Otherwise, target driver should be prepared to receive
 *      xmit_response() for the session's command after
 *      scst_unregister_session() returns.
 *   unreg_done_fn - pointer to the function that will be
 *      asynchronously called when the last session's command finishes and
 *      the session is about to be completely freed. Can be NULL.
 *      Parameter:
 *       - sess - session
 *
 * Notes:
 *
 * - All outstanding commands will be finished regularly. After
 *   scst_unregister_session() returned no new commands must be sent to
 *   SCST via scst_rx_cmd().
 *
 * - The caller must ensure that no scst_rx_cmd() or scst_rx_mgmt_fn_*() is
 *   called in paralell with scst_unregister_session().
 *
 * - Can be called before result_fn() of scst_register_session() called,
 *   i.e. during the session registration/initialization.
 *
 * - It is highly recommended to call scst_unregister_session() as soon as it
 *   gets clear that session will be unregistered and not to wait until all
 *   related commands finished. This function provides the wait functionality,
 *   but it also starts recovering stuck commands, if there are any.
 *   Otherwise, your target driver could wait for those commands forever.
 */
void scst_unregister_session(struct scst_session *sess, int wait,
	void (*unreg_done_fn) (struct scst_session *sess));

/*
 * Registers dev handler driver
 * Returns 0 on success or appropriate error code otherwise
 */
int __scst_register_dev_driver(struct scst_dev_type *dev_type,
	const char *version);
static inline int scst_register_dev_driver(struct scst_dev_type *dev_type)
{
	return __scst_register_dev_driver(dev_type, SCST_INTERFACE_VERSION);
}

/*
 * Unregisters dev handler driver
 */
void scst_unregister_dev_driver(struct scst_dev_type *dev_type);

/*
 * Registers dev handler driver for virtual devices (eg VDISK)
 * Returns 0 on success or appropriate error code otherwise
 */
int __scst_register_virtual_dev_driver(struct scst_dev_type *dev_type,
	const char *version);
static inline int scst_register_virtual_dev_driver(
	struct scst_dev_type *dev_type)
{
	return __scst_register_virtual_dev_driver(dev_type,
		SCST_INTERFACE_VERSION);
}

/*
 * Unregisters dev handler driver for virtual devices
 */
void scst_unregister_virtual_dev_driver(struct scst_dev_type *dev_type);

/*
 * Creates and sends new command to SCST.
 * Must not be called in parallel with scst_unregister_session() for the
 * same sess. Returns the command on success or NULL otherwise
 */
struct scst_cmd *scst_rx_cmd(struct scst_session *sess,
	const uint8_t *lun, int lun_len, const uint8_t *cdb,
	int cdb_len, int atomic);

/*
 * Notifies SCST that the driver finished its part of the command
 * initialization, and the command is ready for execution.
 * The second argument sets preferred command execition context.
 * See SCST_CONTEXT_* constants for details.
 *
 * !!IMPORTANT!!
 *
 * If cmd->set_sn_on_restart_cmd not set, this function, as well as
 * scst_cmd_init_stage1_done() and scst_restart_cmd(), must not be
 * called simultaneously for the same session (more precisely,
 * for the same session/LUN, i.e. tgt_dev), i.e. they must be
 * somehow externally serialized. This is needed to have lock free fast path in
 * scst_cmd_set_sn(). For majority of targets those functions are naturally
 * serialized by the single source of commands. Only iSCSI immediate commands
 * with multiple connections per session seems to be an exception. For it, some
 * mutex/lock shall be used for the serialization.
 */
void scst_cmd_init_done(struct scst_cmd *cmd,
	enum scst_exec_context pref_context);

/*
 * Notifies SCST that the driver finished the first stage of the command
 * initialization, and the command is ready for execution, but after
 * SCST done the command's preprocessing preprocessing_done() function
 * should be called. The second argument sets preferred command execition
 * context. See SCST_CONTEXT_* constants for details.
 *
 * See also scst_cmd_init_done() comment for the serialization requirements.
 */
static inline void scst_cmd_init_stage1_done(struct scst_cmd *cmd,
	enum scst_exec_context pref_context, int set_sn)
{
	cmd->preprocessing_only = 1;
	cmd->set_sn_on_restart_cmd = !set_sn;
	scst_cmd_init_done(cmd, pref_context);
}

/*
 * Notifies SCST that the driver finished its part of the command's
 * preprocessing and it is ready for further processing.
 * The second argument sets data receiving completion status
 * (see SCST_PREPROCESS_STATUS_* constants for details)
 * The third argument sets preferred command execition context
 * (see SCST_CONTEXT_* constants for details).
 *
 * See also scst_cmd_init_done() comment for the serialization requirements.
 */
void scst_restart_cmd(struct scst_cmd *cmd, int status,
	enum scst_exec_context pref_context);

/*
 * Notifies SCST that the driver received all the necessary data
 * and the command is ready for further processing.
 * The second argument sets data receiving completion status
 * (see SCST_RX_STATUS_* constants for details)
 * The third argument sets preferred command execition context
 * (see SCST_CONTEXT_* constants for details)
 */
void scst_rx_data(struct scst_cmd *cmd, int status,
	enum scst_exec_context pref_context);

/*
 * Notifies SCST that the driver sent the response and the command
 * can be freed now. Don't forget to set the delivery status, if it
 * isn't success, using scst_set_delivery_status() before calling
 * this function. The third argument sets preferred command execition
 * context (see SCST_CONTEXT_* constants for details)
 */
void scst_tgt_cmd_done(struct scst_cmd *cmd,
	enum scst_exec_context pref_context);

/*
 * Creates new management command sends it for execution.
 * Must not be called in parallel with scst_unregister_session() for the
 * same sess. Returns 0 for success, error code otherwise.
 */
int scst_rx_mgmt_fn(struct scst_session *sess,
	const struct scst_rx_mgmt_params *params);

/*
 * Creates new management command using tag and sends it for execution.
 * Can be used for SCST_ABORT_TASK only.
 * Must not be called in parallel with scst_unregister_session() for the
 * same sess. Returns 0 for success, error code otherwise.
 *
 * Obsolete in favor of scst_rx_mgmt_fn()
 */
static inline int scst_rx_mgmt_fn_tag(struct scst_session *sess, int fn,
	uint64_t tag, int atomic, void *tgt_priv)
{
	struct scst_rx_mgmt_params params;

	BUG_ON(fn != SCST_ABORT_TASK);

	memset(&params, 0, sizeof(params));
	params.fn = fn;
	params.tag = tag;
	params.tag_set = 1;
	params.atomic = atomic;
	params.tgt_priv = tgt_priv;
	return scst_rx_mgmt_fn(sess, &params);
}

/*
 * Creates new management command using LUN and sends it for execution.
 * Currently can be used for any fn, except SCST_ABORT_TASK.
 * Must not be called in parallel with scst_unregister_session() for the
 * same sess. Returns 0 for success, error code otherwise.
 *
 * Obsolete in favor of scst_rx_mgmt_fn()
 */
static inline int scst_rx_mgmt_fn_lun(struct scst_session *sess, int fn,
	const uint8_t *lun, int lun_len, int atomic, void *tgt_priv)
{
	struct scst_rx_mgmt_params params;

	BUG_ON(fn == SCST_ABORT_TASK);

	memset(&params, 0, sizeof(params));
	params.fn = fn;
	params.lun = lun;
	params.lun_len = lun_len;
	params.lun_set = 1;
	params.atomic = atomic;
	params.tgt_priv = tgt_priv;
	return scst_rx_mgmt_fn(sess, &params);
}

/*
 * Provides various info about command's CDB.
 *
 * Returns: 0 on success, <0 if command is unknown, >0 if command is invalid.
 */
int scst_get_cdb_info(struct scst_cmd *cmd);

/*
 * Set error SCSI status in the command and prepares it for returning it
 */
void scst_set_cmd_error_status(struct scst_cmd *cmd, int status);

/*
 * Set error in the command and fill the sense buffer
 */
void scst_set_cmd_error(struct scst_cmd *cmd, int key, int asc, int ascq);

/*
 * Sets BUSY or TASK QUEUE FULL status
 */
void scst_set_busy(struct scst_cmd *cmd);

/*
 * Check if sense in the sense buffer, if any, in the correct format. If not,
 * convert it to the correct format.
 */
void scst_check_convert_sense(struct scst_cmd *cmd);

/*
 * Sets initial Unit Attention for sess, replacing default scst_sense_reset_UA
 */
void scst_set_initial_UA(struct scst_session *sess, int key, int asc, int ascq);

/*
 * Notifies SCST core that dev changed its capacity
 */
void scst_capacity_data_changed(struct scst_device *dev);

/*
 * Finds a command based on the supplied tag comparing it with one
 * that previously set by scst_cmd_set_tag().
 * Returns the command on success or NULL otherwise
 */
struct scst_cmd *scst_find_cmd_by_tag(struct scst_session *sess, uint64_t tag);

/*
 * Finds a command based on user supplied data and comparision
 * callback function, that should return true, if the command is found.
 * Returns the command on success or NULL otherwise
 */
struct scst_cmd *scst_find_cmd(struct scst_session *sess, void *data,
			       int (*cmp_fn) (struct scst_cmd *cmd,
					      void *data));

/*
 * Translates SCST's data direction to DMA one
 */
static inline int scst_to_dma_dir(int scst_dir)
{
	return scst_dir;
}

/*
 * Translates SCST data direction to DMA one from the perspective
 * of the target device.
 */
static inline int scst_to_tgt_dma_dir(int scst_dir)
{
	if (scst_dir == SCST_DATA_WRITE)
		return DMA_FROM_DEVICE;
	else if (scst_dir == SCST_DATA_READ)
		return DMA_TO_DEVICE;
	return scst_dir;
}

/*
 * Returns 1, if cmd's CDB is locally handled by SCST and 0 otherwise.
 * Dev handlers parse() and dev_done() not called for such commands.
 */
static inline int scst_is_cmd_local(struct scst_cmd *cmd)
{
	int res = 0;
	switch (cmd->cdb[0]) {
	case REPORT_LUNS:
		res = 1;
	}
	return res;
}

/*
 * Registers a virtual device.
 * Parameters:
 *   dev_type - the device's device handler
 *   dev_name - the new device name, NULL-terminated string. Must be uniq
 *              among all virtual devices in the system. The value isn't
 *              copied, only the reference is stored, so the value must
 *              remain valid during the device lifetime.
 * Returns assinged to the device ID on success, or negative value otherwise
 */
int scst_register_virtual_device(struct scst_dev_type *dev_handler,
	const char *dev_name);

/*
 * Unegisters a virtual device.
 * Parameters:
 *   id - the device's ID, returned by the registration function
 */
void scst_unregister_virtual_device(int id);

/*
 * Get/Set functions for tgt's sg_tablesize
 */
static inline int scst_tgt_get_sg_tablesize(struct scst_tgt *tgt)
{
	return tgt->sg_tablesize;
}

static inline void scst_tgt_set_sg_tablesize(struct scst_tgt *tgt, int val)
{
	tgt->sg_tablesize = val;
}

/*
 * Get/Set functions for tgt's target private data
 */
static inline void *scst_tgt_get_tgt_priv(struct scst_tgt *tgt)
{
	return tgt->tgt_priv;
}

static inline void scst_tgt_set_tgt_priv(struct scst_tgt *tgt, void *val)
{
	tgt->tgt_priv = val;
}

/*
 * Get/Set functions for session's target private data
 */
static inline void *scst_sess_get_tgt_priv(struct scst_session *sess)
{
	return sess->tgt_priv;
}

static inline void scst_sess_set_tgt_priv(struct scst_session *sess,
					      void *val)
{
	sess->tgt_priv = val;
}

/**
 * Returns TRUE if cmd is being executed in atomic context.
 *
 * Note: checkpatch will complain on the use of in_atomic() below. You can
 * safely ignore this warning since in_atomic() is used here only for debugging
 * purposes.
 */
static inline int scst_cmd_atomic(struct scst_cmd *cmd)
{
	int res = cmd->atomic;
#ifdef CONFIG_SCST_EXTRACHECKS
	if (unlikely((in_atomic() || in_interrupt() || irqs_disabled()) &&
		     !res)) {
		printk(KERN_ERR "ERROR: atomic context and non-atomic cmd\n");
		dump_stack();
		cmd->atomic = 1;
		res = 1;
	}
#endif
	return res;
}

static inline enum scst_exec_context __scst_estimate_context(bool direct)
{
	if (in_irq())
		return SCST_CONTEXT_TASKLET;
	else if (irqs_disabled())
		return SCST_CONTEXT_THREAD;
	else
		return direct ? SCST_CONTEXT_DIRECT :
				SCST_CONTEXT_DIRECT_ATOMIC;
}

static inline enum scst_exec_context scst_estimate_context(void)
{
	return __scst_estimate_context(0);
}

static inline enum scst_exec_context scst_estimate_context_direct(void)
{
	return __scst_estimate_context(1);
}

/* Returns cmd's CDB */
static inline const uint8_t *scst_cmd_get_cdb(struct scst_cmd *cmd)
{
	return cmd->cdb;
}

/* Returns cmd's CDB length */
static inline int scst_cmd_get_cdb_len(struct scst_cmd *cmd)
{
	return cmd->cdb_len;
}

/* Returns cmd's extended CDB */
static inline const uint8_t *scst_cmd_get_ext_cdb(struct scst_cmd *cmd)
{
	return cmd->ext_cdb;
}

/* Returns cmd's extended CDB length */
static inline int scst_cmd_get_ext_cdb_len(struct scst_cmd *cmd)
{
	return cmd->ext_cdb_len;
}

/* Sets cmd's extended CDB and its length */
static inline void scst_cmd_set_ext_cdb(struct scst_cmd *cmd,
	uint8_t *ext_cdb, unsigned int ext_cdb_len)
{
	cmd->ext_cdb = ext_cdb;
	cmd->ext_cdb_len = ext_cdb_len;
}

/* Returns cmd's session */
static inline struct scst_session *scst_cmd_get_session(struct scst_cmd *cmd)
{
	return cmd->sess;
}

/* Returns cmd's response data length */
static inline int scst_cmd_get_resp_data_len(struct scst_cmd *cmd)
{
	return cmd->resp_data_len;
}

/* Returns if status should be sent for cmd */
static inline int scst_cmd_get_is_send_status(struct scst_cmd *cmd)
{
	return cmd->is_send_status;
}

/*
 * Returns pointer to cmd's SG data buffer.
 *
 * Usage of this function is not recommended, use scst_get_buf_*()
 * family of functions instead.
 */
static inline struct scatterlist *scst_cmd_get_sg(struct scst_cmd *cmd)
{
	return cmd->sg;
}

/*
 * Returns cmd's sg_cnt.
 *
 * Usage of this function is not recommended, use scst_get_buf_*()
 * family of functions instead.
 */
static inline int scst_cmd_get_sg_cnt(struct scst_cmd *cmd)
{
	return cmd->sg_cnt;
}

/*
 * Returns cmd's data buffer length.
 *
 * In case if you need to iterate over data in the buffer, usage of
 * this function is not recommended, use scst_get_buf_*()
 * family of functions instead.
 */
static inline unsigned int scst_cmd_get_bufflen(struct scst_cmd *cmd)
{
	return cmd->bufflen;
}

/* Returns pointer to cmd's target's SG data buffer */
static inline struct scatterlist *scst_cmd_get_tgt_sg(struct scst_cmd *cmd)
{
	return cmd->tgt_sg;
}

/* Returns cmd's target's sg_cnt */
static inline int scst_cmd_get_tgt_sg_cnt(struct scst_cmd *cmd)
{
	return cmd->tgt_sg_cnt;
}

/* Sets cmd's target's SG data buffer */
static inline void scst_cmd_set_tgt_sg(struct scst_cmd *cmd,
	struct scatterlist *sg, int sg_cnt)
{
	cmd->tgt_sg = sg;
	cmd->tgt_sg_cnt = sg_cnt;
	cmd->tgt_data_buf_alloced = 1;
}

/* Returns cmd's data direction */
static inline scst_data_direction scst_cmd_get_data_direction(
	struct scst_cmd *cmd)
{
	return cmd->data_direction;
}

/* Returns cmd's status byte from host device */
static inline uint8_t scst_cmd_get_status(struct scst_cmd *cmd)
{
	return cmd->status;
}

/* Returns cmd's status from host adapter itself */
static inline uint8_t scst_cmd_get_msg_status(struct scst_cmd *cmd)
{
	return cmd->msg_status;
}

/* Returns cmd's status set by low-level driver to indicate its status */
static inline uint8_t scst_cmd_get_host_status(struct scst_cmd *cmd)
{
	return cmd->host_status;
}

/* Returns cmd's status set by SCSI mid-level */
static inline uint8_t scst_cmd_get_driver_status(struct scst_cmd *cmd)
{
	return cmd->driver_status;
}

/* Returns pointer to cmd's sense buffer */
static inline uint8_t *scst_cmd_get_sense_buffer(struct scst_cmd *cmd)
{
	return cmd->sense;
}

/* Returns cmd's sense buffer length */
static inline int scst_cmd_get_sense_buffer_len(struct scst_cmd *cmd)
{
	return cmd->sense_bufflen;
}

/*
 * Get/Set functions for cmd's target SN
 */
static inline uint64_t scst_cmd_get_tag(struct scst_cmd *cmd)
{
	return cmd->tag;
}

static inline void scst_cmd_set_tag(struct scst_cmd *cmd, uint64_t tag)
{
	cmd->tag = tag;
}

/*
 * Get/Set functions for cmd's target private data.
 * Variant with *_lock must be used if target driver uses
 * scst_find_cmd() to avoid race with it, except inside scst_find_cmd()'s
 * callback, where lock is already taken.
 */
static inline void *scst_cmd_get_tgt_priv(struct scst_cmd *cmd)
{
	return cmd->tgt_priv;
}

static inline void scst_cmd_set_tgt_priv(struct scst_cmd *cmd, void *val)
{
	cmd->tgt_priv = val;
}

void *scst_cmd_get_tgt_priv_lock(struct scst_cmd *cmd);
void scst_cmd_set_tgt_priv_lock(struct scst_cmd *cmd, void *val);

/*
 * Get/Set functions for tgt_need_alloc_data_buf flag
 */
static inline int scst_cmd_get_tgt_need_alloc_data_buf(struct scst_cmd *cmd)
{
	return cmd->tgt_need_alloc_data_buf;
}

static inline void scst_cmd_set_tgt_need_alloc_data_buf(struct scst_cmd *cmd)
{
	cmd->tgt_need_alloc_data_buf = 1;
}

/*
 * Get/Set functions for tgt_data_buf_alloced flag
 */
static inline int scst_cmd_get_tgt_data_buff_alloced(struct scst_cmd *cmd)
{
	return cmd->tgt_data_buf_alloced;
}

static inline void scst_cmd_set_tgt_data_buff_alloced(struct scst_cmd *cmd)
{
	cmd->tgt_data_buf_alloced = 1;
}

/*
 * Get/Set functions for dh_data_buf_alloced flag
 */
static inline int scst_cmd_get_dh_data_buff_alloced(struct scst_cmd *cmd)
{
	return cmd->dh_data_buf_alloced;
}

static inline void scst_cmd_set_dh_data_buff_alloced(struct scst_cmd *cmd)
{
	cmd->dh_data_buf_alloced = 1;
}

/*
 * Get/Set functions for no_sgv flag
 */
static inline int scst_cmd_get_no_sgv(struct scst_cmd *cmd)
{
	return cmd->no_sgv;
}

static inline void scst_cmd_set_no_sgv(struct scst_cmd *cmd)
{
	cmd->no_sgv = 1;
}

/*
 * Get/Set functions for tgt_sn
 */
static inline int scst_cmd_get_tgt_sn(struct scst_cmd *cmd)
{
	BUG_ON(!cmd->tgt_sn_set);
	return cmd->tgt_sn;
}

static inline void scst_cmd_set_tgt_sn(struct scst_cmd *cmd, uint32_t tgt_sn)
{
	cmd->tgt_sn_set = 1;
	cmd->tgt_sn = tgt_sn;
}

/*
 * Returns 1 if the cmd was aborted, so its status is invalid and no
 * reply shall be sent to the remote initiator. A target driver should
 * only clear internal resources, associated with cmd.
 */
static inline int scst_cmd_aborted(struct scst_cmd *cmd)
{
	return test_bit(SCST_CMD_ABORTED, &cmd->cmd_flags) &&
		!test_bit(SCST_CMD_ABORTED_OTHER, &cmd->cmd_flags);
}

/* Returns sense data format for cmd's dev */
static inline bool scst_get_cmd_dev_d_sense(struct scst_cmd *cmd)
{
	return (cmd->dev != NULL) ? cmd->dev->d_sense : 0;
}

/*
 * Get/Set functions for expected data direction, transfer length
 * and its validity flag
 */
static inline int scst_cmd_is_expected_set(struct scst_cmd *cmd)
{
	return cmd->expected_values_set;
}

static inline scst_data_direction scst_cmd_get_expected_data_direction(
	struct scst_cmd *cmd)
{
	return cmd->expected_data_direction;
}

static inline int scst_cmd_get_expected_transfer_len(
	struct scst_cmd *cmd)
{
	return cmd->expected_transfer_len;
}

static inline void scst_cmd_set_expected(struct scst_cmd *cmd,
	scst_data_direction expected_data_direction,
	int expected_transfer_len)
{
	cmd->expected_data_direction = expected_data_direction;
	cmd->expected_transfer_len = expected_transfer_len;
	cmd->expected_values_set = 1;
}

/*
 * Get/clear functions for cmd's may_need_dma_sync
 */
static inline int scst_get_may_need_dma_sync(struct scst_cmd *cmd)
{
	return cmd->may_need_dma_sync;
}

static inline void scst_clear_may_need_dma_sync(struct scst_cmd *cmd)
{
	cmd->may_need_dma_sync = 0;
}

/*
 * Get/set functions for cmd's delivery_status. It is one of
 * SCST_CMD_DELIVERY_* constants. It specifies the status of the
 * command's delivery to initiator.
 */
static inline int scst_get_delivery_status(struct scst_cmd *cmd)
{
	return cmd->delivery_status;
}

static inline void scst_set_delivery_status(struct scst_cmd *cmd,
	int delivery_status)
{
	cmd->delivery_status = delivery_status;
}

/*
 * Get/Set function for mgmt cmd's target private data
 */
static inline void *scst_mgmt_cmd_get_tgt_priv(struct scst_mgmt_cmd *mcmd)
{
	return mcmd->tgt_priv;
}

static inline void scst_mgmt_cmd_set_tgt_priv(struct scst_mgmt_cmd *mcmd,
	void *val)
{
	mcmd->tgt_priv = val;
}

/* Returns mgmt cmd's completition status (SCST_MGMT_STATUS_* constants) */
static inline int scst_mgmt_cmd_get_status(struct scst_mgmt_cmd *mcmd)
{
	return mcmd->status;
}

/* Returns mgmt cmd's TM fn */
static inline int scst_mgmt_cmd_get_fn(struct scst_mgmt_cmd *mcmd)
{
	return mcmd->fn;
}

/*
 * Called by dev handler's task_mgmt_fn() to notify SCST core that mcmd
 * is going to complete asynchronously.
 */
void scst_prepare_async_mcmd(struct scst_mgmt_cmd *mcmd);

/*
 * Called by dev handler to notify SCST core that async. mcmd is completed
 * with status "status".
 */
void scst_async_mcmd_completed(struct scst_mgmt_cmd *mcmd, int status);

/* Returns AEN's fn */
static inline int scst_aen_get_event_fn(struct scst_aen *aen)
{
	return aen->event_fn;
}

/* Returns AEN's session */
static inline struct scst_session *scst_aen_get_sess(struct scst_aen *aen)
{
	return aen->sess;
}

/* Returns AEN's LUN */
static inline uint64_t scst_aen_get_lun(struct scst_aen *aen)
{
	return aen->lun;
}

/* Returns SCSI AEN's sense */
static inline const uint8_t *scst_aen_get_sense(struct scst_aen *aen)
{
	return aen->aen_sense;
}

/* Returns SCSI AEN's sense length */
static inline int scst_aen_get_sense_len(struct scst_aen *aen)
{
	return aen->aen_sense_len;
}

/*
 * Get/set functions for AEN's delivery_status. It is one of
 * SCST_AEN_RES_* constants. It specifies the status of the
 * command's delivery to initiator.
 */
static inline int scst_get_aen_delivery_status(struct scst_aen *aen)
{
	return aen->delivery_status;
}

static inline void scst_set_aen_delivery_status(struct scst_aen *aen,
	int status)
{
	aen->delivery_status = status;
}

/*
 * Notifies SCST that the driver has sent the AEN and it
 * can be freed now. Don't forget to set the delivery status, if it
 * isn't success, using scst_set_aen_delivery_status() before calling
 * this function.
 */
void scst_aen_done(struct scst_aen *aen);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
#ifndef RHEL_RELEASE_CODE
static inline struct page *sg_page(struct scatterlist *sg)
{
	return sg->page;
}

static inline void *sg_virt(struct scatterlist *sg)
{
	return page_address(sg_page(sg)) + sg->offset;
}

static inline void sg_init_table(struct scatterlist *sgl, unsigned int nents)
{
	memset(sgl, 0, sizeof(*sgl) * nents);
}

static inline void sg_assign_page(struct scatterlist *sg, struct page *page)
{
	sg->page = page;
}

static inline void sg_set_page(struct scatterlist *sg, struct page *page,
			       unsigned int len, unsigned int offset)
{
	sg_assign_page(sg, page);
	sg->offset = offset;
	sg->length = len;
}

#endif
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24) */

static inline void sg_clear(struct scatterlist *sg)
{
	memset(sg, 0, sizeof(*sg));
#ifdef CONFIG_DEBUG_SG
	sg->sg_magic = SG_MAGIC;
#endif
}

/*
 * Functions for access to the commands data (SG) buffer,
 * including HIGHMEM environment. Should be used instead of direct
 * access. Returns the mapped buffer length for success, 0 for EOD,
 * negative error code otherwise.
 *
 * "Buf" argument returns the mapped buffer
 *
 * The "put" function unmaps the buffer.
 */
static inline int __scst_get_buf(struct scst_cmd *cmd, uint8_t **buf)
{
	int res = 0;
	struct scatterlist *sg = cmd->sg;
	int i = cmd->get_sg_buf_entry_num;

	*buf = NULL;

	if ((i >= cmd->sg_cnt) || unlikely(sg == NULL))
		goto out;

	*buf = page_address(sg_page(&sg[i]));
	*buf += sg[i].offset;

	res = sg[i].length;
	cmd->get_sg_buf_entry_num++;

out:
	return res;
}

static inline int scst_get_buf_first(struct scst_cmd *cmd, uint8_t **buf)
{
	cmd->get_sg_buf_entry_num = 0;
	cmd->may_need_dma_sync = 1;
	return __scst_get_buf(cmd, buf);
}

static inline int scst_get_buf_next(struct scst_cmd *cmd, uint8_t **buf)
{
	return __scst_get_buf(cmd, buf);
}

static inline void scst_put_buf(struct scst_cmd *cmd, void *buf)
{
	/* Nothing to do */
}

/*
 * Returns approximate higher rounded buffers count that
 * scst_get_buf_[first|next]() return.
 */
static inline int scst_get_buf_count(struct scst_cmd *cmd)
{
	return (cmd->sg_cnt == 0) ? 1 : cmd->sg_cnt;
}

/*
 * Suspends and resumes any activity.
 * Function scst_suspend_activity() doesn't return 0, until there are any
 * active commands (state after SCST_CMD_STATE_INIT). If "interruptible"
 * is true, it returns after SCST_SUSPENDING_TIMEOUT or if it was interrupted
 * by a signal with the coresponding error status < 0. If "interruptible"
 * is false, it will wait virtually forever.
 *
 * New arriving commands stay in that state until scst_resume_activity()
 * is called.
 */
int scst_suspend_activity(bool interruptible);
void scst_resume_activity(void);

/*
 * Main SCST commands processing routing. Must be used only by dev handlers.
 * Argument atomic is true if function called in atomic context.
 */
void scst_process_active_cmd(struct scst_cmd *cmd, bool atomic);

/*
 * Checks if command can be executed (reservations, etc.) or there are local
 * events, like pending UAs. Returns < 0 if command must be aborted, > 0 if
 * there is an event and command should be immediately completed, or 0
 * otherwise.
 *
 * !! Dev handlers implementing exec() callback must call this function there !!
 * !! just before the actual command's execution                              !!
 */
int scst_check_local_events(struct scst_cmd *cmd);

/*
 * Returns the next state of the SCSI target state machine in case if command's
 * completed abnormally.
 */
int scst_get_cmd_abnormal_done_state(const struct scst_cmd *cmd);

/*
 * Sets state of the SCSI target state machine in case if command's completed
 * abnormally.
 */
void scst_set_cmd_abnormal_done_state(struct scst_cmd *cmd);

/*
 * Returns target driver's root entry in SCST's /proc hierarchy.
 * The driver can create own files/directoryes here, which should
 * be deleted in the driver's release().
 */
static inline struct proc_dir_entry *scst_proc_get_tgt_root(
	struct scst_tgt_template *vtt)
{
	return vtt->proc_tgt_root;
}

/*
 * Returns device handler's root entry in SCST's /proc hierarchy.
 * The driver can create own files/directoryes here, which should
 * be deleted in the driver's detach()/release().
 */
static inline struct proc_dir_entry *scst_proc_get_dev_type_root(
	struct scst_dev_type *dtt)
{
	return dtt->proc_dev_type_root;
}

/**
 ** Two library functions and the structure to help the drivers
 ** that use scst_debug.* facilities manage "trace_level" /proc entry.
 ** The functions service "standard" log levels and allow to work
 ** with driver specific levels, which should be passed inside as
 ** NULL-terminated array of struct scst_proc_log's, where:
 **   - val - the level's numeric value
 **   - token - its string representation
 **/

struct scst_proc_log {
	unsigned int val;
	const char *token;
};

int scst_proc_log_entry_read(struct seq_file *seq, unsigned long log_level,
	const struct scst_proc_log *tbl);

int scst_proc_log_entry_write(struct file *file, const char __user *buf,
	unsigned long length, unsigned long *log_level,
	unsigned long default_level, const struct scst_proc_log *tbl);

/*
 * helper data structure and function to create proc entry.
 */
struct scst_proc_data {
	const struct file_operations seq_op;
	int (*show)(struct seq_file *, void *);
	void *data;
};

int scst_single_seq_open(struct inode *inode, struct file *file);

struct proc_dir_entry *scst_create_proc_entry(struct proc_dir_entry *root,
	const char *name, struct scst_proc_data *pdata);

#define SCST_DEF_RW_SEQ_OP(x)                          \
	.seq_op = {                                    \
		.owner          = THIS_MODULE,         \
		.open           = scst_single_seq_open,\
		.read           = seq_read,            \
		.write          = x,                   \
		.llseek         = seq_lseek,           \
		.release        = single_release,      \
	},

/*
 * Adds and deletes (stops) num of global SCST's threads. Returns 0 on
 * success, error code otherwise.
 */
int scst_add_global_threads(int num);
void scst_del_global_threads(int num);

int scst_alloc_sense(struct scst_cmd *cmd, int atomic);
int scst_alloc_set_sense(struct scst_cmd *cmd, int atomic,
	const uint8_t *sense, unsigned int len);

void scst_set_sense(uint8_t *buffer, int len, bool d_sense,
	int key, int asc, int ascq);

/*
 * Returnes true if sense matches to (key, asc, ascq) and false otherwise.
 * Valid_mask is one or several SCST_SENSE_*_VALID constants setting valid
 * (key, asc, ascq) values.
 */
bool scst_analyze_sense(const uint8_t *sense, int len,
	unsigned int valid_mask, int key, int asc, int ascq);

/*
 * Returnes a pseudo-random number for debugging purposes. Available only in
 * the DEBUG build.
 */
unsigned long scst_random(void);

/*
 * Sets response data length for cmd and truncates its SG vector accordingly.
 * The cmd->resp_data_len must not be set directly, it must be set only
 * using this function. Value of resp_data_len must be <= cmd->bufflen.
 */
void scst_set_resp_data_len(struct scst_cmd *cmd, int resp_data_len);

/*
 * Get/put global ref counter that prevents from entering into suspended
 * activities stage, so protects from any global management operations.
 */
void scst_get(void);
void scst_put(void);

/*
 * Cmd ref counters
 */
void scst_cmd_get(struct scst_cmd *cmd);
void scst_cmd_put(struct scst_cmd *cmd);

/*
 * Allocates and returns pointer to SG vector with data size "size".
 * In *count returned the count of entries in the vector.
 * Returns NULL for failure.
 */
struct scatterlist *scst_alloc(int size, gfp_t gfp_mask, int *count);

/* Frees SG vector returned by scst_alloc() */
void scst_free(struct scatterlist *sg, int count);

/*
 * Adds local to the current thread data to tgt_dev
 * (they will be local for the tgt_dev and current thread).
 */
void scst_add_thr_data(struct scst_tgt_dev *tgt_dev,
	struct scst_thr_data_hdr *data,
	void (*free_fn) (struct scst_thr_data_hdr *data));

/* Deletes all local to threads data from tgt_dev */
void scst_del_all_thr_data(struct scst_tgt_dev *tgt_dev);

/* Deletes all local to threads data from all tgt_dev's of the dev */
void scst_dev_del_all_thr_data(struct scst_device *dev);

/* Finds local to the thread data. Returns NULL, if they not found. */
struct scst_thr_data_hdr *__scst_find_thr_data(struct scst_tgt_dev *tgt_dev,
	struct task_struct *tsk);

/* Finds local to the current thread data. Returns NULL, if they not found. */
static inline struct scst_thr_data_hdr *scst_find_thr_data(
	struct scst_tgt_dev *tgt_dev)
{
	return __scst_find_thr_data(tgt_dev, current);
}

static inline void scst_thr_data_get(struct scst_thr_data_hdr *data)
{
	atomic_inc(&data->ref);
}

static inline void scst_thr_data_put(struct scst_thr_data_hdr *data)
{
	if (atomic_dec_and_test(&data->ref))
		data->free_fn(data);
}

/**
 ** Generic parse() support routines.
 ** Done via pointer on functions to avoid unneeded dereferences on
 ** the fast path.
 **/

/* Calculates and returns block shift for the given sector size */
int scst_calc_block_shift(int sector_size);

/* Generic parse() for SBC (disk) devices */
int scst_sbc_generic_parse(struct scst_cmd *cmd,
	int (*get_block_shift)(struct scst_cmd *cmd));

/* Generic parse() for MMC (cdrom) devices */
int scst_cdrom_generic_parse(struct scst_cmd *cmd,
	int (*get_block_shift)(struct scst_cmd *cmd));

/* Generic parse() for MO disk devices */
int scst_modisk_generic_parse(struct scst_cmd *cmd,
	int (*get_block_shift)(struct scst_cmd *cmd));

/* Generic parse() for tape devices */
int scst_tape_generic_parse(struct scst_cmd *cmd,
	int (*get_block_size)(struct scst_cmd *cmd));

/* Generic parse() for changer devices */
int scst_changer_generic_parse(struct scst_cmd *cmd,
	int (*nothing)(struct scst_cmd *cmd));

/* Generic parse() for "processor" devices */
int scst_processor_generic_parse(struct scst_cmd *cmd,
	int (*nothing)(struct scst_cmd *cmd));

/* Generic parse() for RAID devices */
int scst_raid_generic_parse(struct scst_cmd *cmd,
	int (*nothing)(struct scst_cmd *cmd));

/**
 ** Generic dev_done() support routines.
 ** Done via pointer on functions to avoid unneeded dereferences on
 ** the fast path.
 **/

/* Generic dev_done() for block devices */
int scst_block_generic_dev_done(struct scst_cmd *cmd,
	void (*set_block_shift)(struct scst_cmd *cmd, int block_shift));

/* Generic dev_done() for tape devices */
int scst_tape_generic_dev_done(struct scst_cmd *cmd,
	void (*set_block_size)(struct scst_cmd *cmd, int block_size));

/*
 * Issues a MODE SENSE for control mode page data and sets the corresponding
 * dev's parameter from it. Returns 0 on success and not 0 otherwise.
 */
int scst_obtain_device_parameters(struct scst_device *dev);

#endif /* __SCST_H */
