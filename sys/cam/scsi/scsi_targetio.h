/*
 * Ioctl definitions for the Target Mode SCSI Proccessor Target driver for CAM.
 *
 * Copyright (c) 1998 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _CAM_SCSI_SCSI_TARGETIO_H_
#define _CAM_SCSI_SCSI_TARGETIO_H_
#ifndef _KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>
#include <cam/cam.h>
#include <cam/cam_ccb.h>

TAILQ_HEAD(ccb_queue, ccb_hdr);

/* Determine and clear exception state in the driver */
typedef enum {
	TARG_EXCEPT_NONE	   = 0x00,
	TARG_EXCEPT_DEVICE_INVALID = 0x01,
	TARG_EXCEPT_BDR_RECEIVED   = 0x02,
	TARG_EXCEPT_BUS_RESET_SEEN = 0x04,
	TARG_EXCEPT_ABORT_SEEN	   = 0x08,
	TARG_EXCEPT_ABORT_TAG_SEEN = 0x10,
	TARG_EXCEPT_UNKNOWN_ATIO   = 0x80
} targ_exception;

#define TARGIOCFETCHEXCEPTION	_IOR('C', 1, targ_exception)
#define TARGIOCCLEAREXCEPTION	_IOW('C', 2, targ_exception)

/*
 * Retreive an Accept Target I/O CCB for a command that is not handled
 * directly by the kernel target driver.
 */
#define TARGIOCFETCHATIO	_IOR('C', 3, struct ccb_accept_tio)

/*
 * Used for responding to incoming ATIO requests.  XPT_CONTINUE_TARG_IO
 * operations are the norm, but ccb types for manipulating the device
 * queue, etc. can also be used if error handling is to be performed by the
 * user land process.
 */
#define TARGIOCCOMMAND		_IOWR('C', 4, union ccb)


typedef enum {
	UA_NONE		= 0x00,
	UA_POWER_ON	= 0x01,
	UA_BUS_RESET	= 0x02,
	UA_BDR		= 0x04
} ua_types;

typedef enum {
	CA_NONE		= 0x00,
	CA_UNIT_ATTN	= 0x01,
	CA_CMD_SENSE	= 0x02
} ca_types;

struct initiator_state {
	ua_types   pending_ua;
	ca_types   pending_ca;
	struct	   scsi_sense_data sense_data;
	struct	   ccb_queue held_queue;
};

struct ioc_initiator_state {
	u_int	initiator_id;
	struct	initiator_state istate;
};

/*
 * Get and Set Contingent Allegiance and Unit Attention state 
 * presented by the target driver.  This is usually used to
 * properly report and error condition in response to an incoming
 * ATIO request handled by the userland process.
 *
 * The initiator_id must be properly initialized in the ioc_initiator_state
 * structure before calling TARGIOCGETISTATE.
 */
#define TARGIOCGETISTATE	_IOWR('C', 6, struct ioc_initiator_state)
#define TARGIOCSETISTATE	_IOW('C', 5, struct ioc_initiator_state)

struct ioc_alloc_unit {
	path_id_t	path_id;
	target_id_t	target_id;
	lun_id_t	lun_id;
	u_int		unit;
};

/*
 * Allocate and Free a target mode instance.  For allocation, the path_id,
 * target_id, and lun_id fields must be set.  On successful completion
 * of the ioctl, the unit field will indicate the unit number of the
 * newly created instance.  For de-allocation, all fields must match
 * an instance in the inactive (i.e. closed) state.
 */
#define TARGCTLIOALLOCUNIT	_IOWR('C', 7, struct ioc_alloc_unit)
#define TARGCTLIOFREEUNIT	_IOW('C', 8, struct ioc_alloc_unit)

/*
 * Set/clear debugging for this target mode instance
 */
#define	TARGIODEBUG		_IOW('C', 9, int)
#endif /* _CAM_SCSI_SCSI_TARGETIO_H_ */
