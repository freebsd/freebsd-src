/*
 * Data structures and definitions for the CAM system.
 *
 * Copyright (c) 1997 Justin T. Gibbs.
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

#ifndef _CAM_CAM_H
#define _CAM_CAM_H 1

#ifdef _KERNEL
#include <opt_cam.h>
#endif

#include <sys/cdefs.h>

typedef u_int path_id_t;
typedef u_int target_id_t;
typedef u_int lun_id_t;

#define	CAM_XPT_PATH_ID	((path_id_t)~0)
#define	CAM_BUS_WILDCARD ((path_id_t)~0)
#define	CAM_TARGET_WILDCARD ((target_id_t)~0)
#define	CAM_LUN_WILDCARD ((lun_id_t)~0)

/*
 * Maximum length for a CAM CDB.  
 */
#define CAM_MAX_CDBLEN 16

/*
 * Definition of a CAM peripheral driver entry.  Peripheral drivers instantiate
 * one of these for each device they wish to communicate with and pass it into
 * the xpt layer when they wish to schedule work on that device via the
 * xpt_schedule API.
 */
struct cam_periph;

/*
 * Priority information for a CAM structure.  The generation number is
 * incremented everytime a new entry is entered into the queue giving round
 * robin per priority level scheduling.
 */
typedef struct {
	u_int32_t priority;
#define CAM_PRIORITY_NONE	(u_int32_t)-1
	u_int32_t generation;
	int       index;
#define CAM_UNQUEUED_INDEX	-1
#define CAM_ACTIVE_INDEX	-2	
#define CAM_DONEQ_INDEX		-3	
} cam_pinfo;

/*
 * Macro to compare two generation numbers.  It is used like this:  
 *
 *	if (GENERATIONCMP(a, >=, b))
 *		...;
 *
 * GERERATIONCMP uses modular arithmetic to guard against wraps
 * wraps in the generation number.
 */
#define GENERATIONCMP(x, op, y) ((int32_t)((x) - (y)) op 0)

/* CAM flags XXX Move to cam_periph.h ??? */
typedef enum {
	CAM_FLAG_NONE		= 0x00,
	CAM_EXPECT_INQ_CHANGE	= 0x01,
	CAM_RETRY_SELTO		= 0x02 /* Retry Selection Timeouts */
} cam_flags;

/* CAM  Status field values */
typedef enum {
	CAM_REQ_INPROG,		/* CCB request is in progress */
	CAM_REQ_CMP,		/* CCB request completed without error */
	CAM_REQ_ABORTED,	/* CCB request aborted by the host */
	CAM_UA_ABORT,		/* Unable to abort CCB request */
	CAM_REQ_CMP_ERR,	/* CCB request completed with an error */
	CAM_BUSY,		/* CAM subsytem is busy */
	CAM_REQ_INVALID,	/* CCB request was invalid */
	CAM_PATH_INVALID,	/* Supplied Path ID is invalid */
	CAM_DEV_NOT_THERE,	/* SCSI Device Not Installed/there */
	CAM_UA_TERMIO,		/* Unable to terminate I/O CCB request */
	CAM_SEL_TIMEOUT,	/* Target Selection Timeout */
	CAM_CMD_TIMEOUT,	/* Command timeout */
	CAM_SCSI_STATUS_ERROR,	/* SCSI error, look at error code in CCB */
	CAM_MSG_REJECT_REC,	/* Message Reject Received */
	CAM_SCSI_BUS_RESET,	/* SCSI Bus Reset Sent/Received */
	CAM_UNCOR_PARITY,	/* Uncorrectable parity error occurred */
	CAM_AUTOSENSE_FAIL = 0x10,/* Autosense: request sense cmd fail */
	CAM_NO_HBA,		/* No HBA Detected error */
	CAM_DATA_RUN_ERR,	/* Data Overrun error */
	CAM_UNEXP_BUSFREE,	/* Unexpected Bus Free */
	CAM_SEQUENCE_FAIL,	/* Target Bus Phase Sequence Failure */
	CAM_CCB_LEN_ERR,	/* CCB length supplied is inadequate */
	CAM_PROVIDE_FAIL,	/* Unable to provide requested capability */
	CAM_BDR_SENT,		/* A SCSI BDR msg was sent to target */
	CAM_REQ_TERMIO,		/* CCB request terminated by the host */
	CAM_UNREC_HBA_ERROR,	/* Unrecoverable Host Bus Adapter Error */
	CAM_REQ_TOO_BIG,	/* The request was too large for this host */
	CAM_REQUEUE_REQ,	/*
				 * This request should be requeued to preserve
				 * transaction ordering.  This typically occurs
				 * when the SIM recognizes an error that should
				 * freeze the queue and must place additional
				 * requests for the target at the sim level
				 * back into the XPT queue.
				 */
	CAM_IDE = 0x33,		/* Initiator Detected Error */
	CAM_RESRC_UNAVAIL,	/* Resource Unavailable */
	CAM_UNACKED_EVENT,	/* Unacknowledged Event by Host */
	CAM_MESSAGE_RECV,	/* Message Received in Host Target Mode */
	CAM_INVALID_CDB,	/* Invalid CDB received in Host Target Mode */
	CAM_LUN_INVALID,	/* Lun supplied is invalid */
	CAM_TID_INVALID,	/* Target ID supplied is invalid */
	CAM_FUNC_NOTAVAIL,	/* The requested function is not available */
	CAM_NO_NEXUS,		/* Nexus is not established */
	CAM_IID_INVALID,	/* The initiator ID is invalid */
	CAM_CDB_RECVD,		/* The SCSI CDB has been received */
	CAM_LUN_ALRDY_ENA,	/* The LUN is already enabled for target mode */
	CAM_SCSI_BUSY,		/* SCSI Bus Busy */

	CAM_DEV_QFRZN = 0x40,	/* The DEV queue is frozen w/this err */

				/* Autosense data valid for target */
	CAM_AUTOSNS_VALID = 0x80,
	CAM_RELEASE_SIMQ = 0x100,/* SIM ready to take more commands */
	CAM_SIM_QUEUED   = 0x200,/* SIM has this command in it's queue */

	CAM_STATUS_MASK = 0x3F,	/* Mask bits for just the status # */

				/* Target Specific Adjunct Status */
	CAM_SENT_SENSE = 0x40000000	/* sent sense with status */
} cam_status;

typedef enum {
	CAM_ESF_NONE		= 0x00,
	CAM_ESF_COMMAND		= 0x01,
	CAM_ESF_CAM_STATUS	= 0x02,
	CAM_ESF_PROTO_STATUS	= 0x04,
	CAM_ESF_ALL		= 0xff
} cam_error_string_flags;

typedef enum {
	CAM_EPF_NONE		= 0x00,
	CAM_EPF_MINIMAL		= 0x01,
	CAM_EPF_NORMAL		= 0x02,
	CAM_EPF_ALL		= 0x03,
	CAM_EPF_LEVEL_MASK	= 0x0f
	/* All bits above bit 3 are protocol-specific */
} cam_error_proto_flags;

typedef enum {
	CAM_ESF_PRINT_NONE	= 0x00,
	CAM_ESF_PRINT_STATUS	= 0x10,
	CAM_ESF_PRINT_SENSE	= 0x20
} cam_error_scsi_flags;

struct cam_status_entry
{
	cam_status  status_code;
	const char *status_text;
};

extern const struct cam_status_entry cam_status_table[];
extern const int num_cam_status_entries;
union ccb;

__BEGIN_DECLS
typedef int (cam_quirkmatch_t)(caddr_t, caddr_t);

caddr_t	cam_quirkmatch(caddr_t target, caddr_t quirk_table, int num_entries,
		       int entry_size, cam_quirkmatch_t *comp_func);

void	cam_strvis(u_int8_t *dst, const u_int8_t *src, int srclen, int dstlen);

int	cam_strmatch(const u_int8_t *str, const u_int8_t *pattern, int str_len);
const struct cam_status_entry*
	cam_fetch_status_entry(cam_status status);
#ifdef _KERNEL
char *	cam_error_string(union ccb *ccb, char *str, int str_len,
			 cam_error_string_flags flags,
			 cam_error_proto_flags proto_flags);
void	cam_error_print(union ccb *ccb, cam_error_string_flags flags,
			cam_error_proto_flags proto_flags);
#else /* _KERNEL */
struct cam_device;

char *	cam_error_string(struct cam_device *device, union ccb *ccb, char *str,
			 int str_len, cam_error_string_flags flags,
			 cam_error_proto_flags proto_flags);
void	cam_error_print(struct cam_device *device, union ccb *ccb,
			cam_error_string_flags flags,
			cam_error_proto_flags proto_flags, FILE *ofile);
#endif /* _KERNEL */
__END_DECLS

#ifdef _KERNEL
static __inline void cam_init_pinfo(cam_pinfo *pinfo);

static __inline void cam_init_pinfo(cam_pinfo *pinfo)
{
	pinfo->priority = CAM_PRIORITY_NONE;	
	pinfo->index = CAM_UNQUEUED_INDEX;
}
#endif

#endif /* _CAM_CAM_H */
