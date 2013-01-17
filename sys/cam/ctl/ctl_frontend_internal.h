/*-
 * Copyright (c) 2004 Silicon Graphics International Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_frontend_internal.h#1 $
 * $FreeBSD$
 */
/*
 * CTL kernel internal frontend target driver.  This allows kernel-level
 * clients to send commands into CTL.
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#ifndef	_CTL_FRONTEND_INTERNAL_H_
#define	_CTL_FRONTEND_INTERNAL_H_

/*
 * These are general metatask error codes.  If the error code is CFI_MT_ERROR, 
 * check any metatask-specific status codes for more detail on the problem.
 */
typedef enum {
	CFI_MT_NONE,
	CFI_MT_PORT_OFFLINE,
	CFI_MT_ERROR,
	CFI_MT_SUCCESS
} cfi_mt_status;

typedef enum {
	CFI_TASK_NONE,
	CFI_TASK_SHUTDOWN,
	CFI_TASK_STARTUP,
	CFI_TASK_BBRREAD
} cfi_tasktype;

struct cfi_task_startstop {
	int total_luns;
	int luns_complete;
	int luns_failed;
};

/*
 * Error code description:
 * CFI_BBR_SUCCESS          - the read was successful
 * CFI_BBR_LUN_UNCONFIG     - CFI probe for this lun hasn't completed
 * CFI_BBR_NO_LUN           - this lun doesn't exist, as far as CFI knows
 * CFI_BBR_NO_MEM           - memory allocation error
 * CFI_BBR_BAD_LEN          - data length isn't a multiple of the blocksize
 * CFI_BBR_RESERV_CONFLICT  - another initiator has this lun reserved, so
 *                            we can't issue I/O at all.
 * CFI_BBR_LUN_STOPPED      - the lun is powered off.
 * CFI_BBR_LUN_OFFLINE_CTL  - the lun is offline from a CTL standpoint
 * CFI_BBR_LUN_OFFLINE_RC   - the lun is offline from a RAIDCore standpoint.
 *                            This is bad, because it basically means we've
 *                            had a double failure on the LUN.
 * CFI_BBR_SCSI_ERROR       - generic SCSI error, see status byte and sense
 *                            data for more resolution if you want it.
 * CFI_BBR_ERROR            - the catch-all error code.
 */
typedef enum {
	CFI_BBR_SUCCESS,
	CFI_BBR_LUN_UNCONFIG,
	CFI_BBR_NO_LUN,
	CFI_BBR_NO_MEM,
	CFI_BBR_BAD_LEN,
	CFI_BBR_RESERV_CONFLICT,
	CFI_BBR_LUN_STOPPED,
	CFI_BBR_LUN_OFFLINE_CTL,
	CFI_BBR_LUN_OFFLINE_RC,
	CFI_BBR_SCSI_ERROR,
	CFI_BBR_ERROR,
} cfi_bbrread_status;

struct cfi_task_bbrread {
	int			lun_num;      /* lun number */
	uint64_t		lba;          /* logical block address */
	int			len;          /* length in bytes */
	cfi_bbrread_status	status;       /* BBR status */
	uint8_t			scsi_status;  /* SCSI status */
	struct scsi_sense_data	sense_data;   /* SCSI sense data */
};

union cfi_taskinfo {
	struct cfi_task_startstop startstop;
	struct cfi_task_bbrread bbrread;
};

struct cfi_metatask;

typedef void (*cfi_cb_t)(void *arg, struct cfi_metatask *metatask);

struct cfi_metatask {
	cfi_tasktype		tasktype;	/* passed to CFI */
	cfi_mt_status		status;		/* returned from CFI */
	union cfi_taskinfo	taskinfo;	/* returned from CFI */
	struct ctl_mem_element	*element;	/* used by CFI, don't touch*/
	cfi_cb_t		callback;	/* passed to CFI */
	void			*callback_arg;	/* passed to CFI */
	STAILQ_ENTRY(cfi_metatask) links;	/* used by CFI, don't touch*/
};

#ifdef _KERNEL

MALLOC_DECLARE(M_CTL_CFI);

/*
 * This is the API for sending meta commands (commands that are sent to more
 * than one LUN) to the internal frontend:
 *  - Allocate a metatask using cfi_alloc_metatask().  can_wait == 0 means
 *    that you're calling from an interrupt context.  can_wait == 1 means
 *    that you're calling from a thread context and don't mind waiting to
 *    allocate memory.
 *  - Setup the task type, callback and callback argument.
 *  - Call cfi_action().
 *  - When the callback comes, note the status and any per-command status
 *    (see the taskinfo union) and then free the metatask with
 *    cfi_free_metatask().
 */
struct cfi_metatask *cfi_alloc_metatask(int can_wait);
void cfi_free_metatask(struct cfi_metatask *metatask);
void cfi_action(struct cfi_metatask *metatask);

#endif /* _KERNEL */

#endif	/* _CTL_FRONTEND_INTERNAL_H_ */

/*
 * vim: ts=8
 */
