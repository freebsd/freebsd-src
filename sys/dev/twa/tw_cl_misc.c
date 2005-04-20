/*
 * Copyright (c) 2004-05 Applied Micro Circuits Corporation.
 * Copyright (c) 2004-05 Vinod Kashyap
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

/*
 * AMCC'S 3ware driver for 9000 series storage controllers.
 *
 * Author: Vinod Kashyap
 */


/*
 * Common Layer miscellaneous functions.
 */


#include "tw_osl_share.h"
#include "tw_cl_share.h"
#include "tw_cl_fwif.h"
#include "tw_cl_ioctl.h"
#include "tw_cl.h"
#include "tw_cl_externs.h"
#include "tw_osl_ioctl.h"



/* AEN severity table. */
TW_INT8	*tw_cli_severity_string_table[] = {
	"None",
	TW_CL_SEVERITY_ERROR_STRING,
	TW_CL_SEVERITY_WARNING_STRING,
	TW_CL_SEVERITY_INFO_STRING,
	TW_CL_SEVERITY_DEBUG_STRING,
	""
};



/*
 * Function name:	tw_cli_drain_complete_queue
 * Description:		This function gets called during a controller reset.
 *			It errors back to CAM, all those requests that are
 *			in the complete queue, at the time of the reset.  Any
 *			CL internal requests will be simply freed.
 *
 * Input:		ctlr	-- ptr to CL internal ctlr context
 * Output:		None
 * Return value:	None
 */
TW_VOID
tw_cli_drain_complete_queue(struct tw_cli_ctlr_context *ctlr)
{
	struct tw_cli_req_context	*req;
	struct tw_cl_req_packet		*req_pkt;

	tw_cli_dbg_printf(3, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");

	/* Walk the busy queue. */
	while ((req = tw_cli_req_q_remove_head(ctlr, TW_CLI_COMPLETE_Q))) {
		if (req->flags & TW_CLI_REQ_FLAGS_INTERNAL) {
			/*
			 * It's an internal request.  Set the appropriate
			 * error and call the CL internal callback if there's
			 * one.  If the request originator is polling for
			 * completion, he should be checking req->error to
			 * determine that the request did not go through.
			 * The request originators are responsible for the
			 * clean-up.
			 */
			req->error_code = TW_CL_ERR_REQ_BUS_RESET;
			if (req->tw_cli_callback)
				req->tw_cli_callback(req);
		} else {
			if ((req_pkt = req->orig_req)) {
				/* It's a SCSI request.  Complete it. */
				tw_cli_dbg_printf(2, ctlr->ctlr_handle,
					tw_osl_cur_func(),
					"Completing complete request %p "
					"on reset",
					req);
				req_pkt->status = TW_CL_ERR_REQ_BUS_RESET;
				req_pkt->tw_osl_callback(req->req_handle);
			}
			tw_cli_req_q_insert_tail(req, TW_CLI_FREE_Q);
		}
	}
}



/*
 * Function name:	tw_cli_drain_busy_queue
 * Description:		This function gets called during a controller reset.
 *			It errors back to CAM, all those requests that were
 *			pending with the firmware, at the time of the reset.
 *
 * Input:		ctlr	-- ptr to CL internal ctlr context
 * Output:		None
 * Return value:	None
 */
TW_VOID
tw_cli_drain_busy_queue(struct tw_cli_ctlr_context *ctlr)
{
	struct tw_cli_req_context	*req;
	struct tw_cl_req_packet		*req_pkt;

	tw_cli_dbg_printf(3, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");

	/* Walk the busy queue. */
	while ((req = tw_cli_req_q_remove_head(ctlr, TW_CLI_BUSY_Q))) {
		if (req->flags & TW_CLI_REQ_FLAGS_INTERNAL) {
			/*
			 * It's an internal request.  Set the appropriate
			 * error and call the CL internal callback if there's
			 * one.  If the request originator is polling for
			 * completion, he should be checking req->error to
			 * determine that the request did not go through.
			 * The request originators are responsible for the
			 * clean-up.
			 */
			req->error_code = TW_CL_ERR_REQ_BUS_RESET;
			if (req->tw_cli_callback)
				req->tw_cli_callback(req);
		} else {
			if ((req_pkt = req->orig_req)) {
				/* It's a SCSI request.  Complete it. */
				tw_cli_dbg_printf(2, ctlr->ctlr_handle,
					tw_osl_cur_func(),
					"Completing busy request %p on reset",
					req);
				req_pkt->status = TW_CL_ERR_REQ_BUS_RESET;
				req_pkt->tw_osl_callback(req->req_handle);
			}
			tw_cli_req_q_insert_tail(req, TW_CLI_FREE_Q);
		}
	}
}



/*
 * Function name:	tw_cli_drain_pending_queue
 * Description:		This function gets called during a controller reset.
 *			It errors back to CAM, all those requests that were
 *			in the pending queue, at the time of the reset.
 *
 * Input:		ctlr	-- ptr to CL internal ctlr context
 * Output:		None
 * Return value:	None
 */

TW_VOID
tw_cli_drain_pending_queue(struct tw_cli_ctlr_context *ctlr)
{
	struct tw_cli_req_context	*req;
	struct tw_cl_req_packet		*req_pkt;
    
	tw_cli_dbg_printf(3, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");
	
	/*
	 * Pull requests off the pending queue, and complete them.
	 */
	while ((req = tw_cli_req_q_remove_head(ctlr, TW_CLI_PENDING_Q))) {
		if (req->flags & TW_CLI_REQ_FLAGS_INTERNAL) {
			/*
			 * It's an internal request.  Set the appropriate
			 * error and call the CL internal callback if there's
			 * one.  If the request originator is polling for
			 * completion, he should be checking req->error to
			 * determine that the request did not go through.
			 * The request originators are responsible for the
			 * clean-up.
			 */
			req->error_code = TW_CL_ERR_REQ_BUS_RESET;
			if (req->tw_cli_callback)
				req->tw_cli_callback(req);
		} else {
			if ((req_pkt = req->orig_req)) {
				/* It's an external request.  Complete it. */
				tw_cli_dbg_printf(2, ctlr->ctlr_handle,
					tw_osl_cur_func(),
					"Completing pending request %p "
					"on reset", req);
				req_pkt->status = TW_CL_ERR_REQ_BUS_RESET;
				req_pkt->tw_osl_callback(req->req_handle);
			}
			tw_cli_req_q_insert_tail(req, TW_CLI_FREE_Q);
		}
	}
}



/*
 * Function name:	tw_cli_drain_response_queue
 * Description:		Drain the controller response queue.
 *
 * Input:		ctlr	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
TW_INT32
tw_cli_drain_response_queue(struct tw_cli_ctlr_context *ctlr)
{
	TW_UINT32	resp;
	TW_UINT32	status_reg;

	tw_cli_dbg_printf(4, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");

	for (;;) {
		status_reg = TW_CLI_READ_STATUS_REGISTER(ctlr->ctlr_handle);

		if (tw_cli_check_ctlr_state(ctlr, status_reg))
			return(TW_OSL_EGENFAILURE);

		if (status_reg & TWA_STATUS_RESPONSE_QUEUE_EMPTY)
			return(TW_OSL_ESUCCESS); /* no more response queue entries */

		resp = TW_CLI_READ_RESPONSE_QUEUE(ctlr->ctlr_handle);
	}
}



/*
 * Function name:	tw_cli_drain_aen_queue
 * Description:		Fetches all un-retrieved AEN's posted by fw.
 *
 * Input:		ctlr	-- ptr to CL internal ctlr context
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
TW_INT32
tw_cli_drain_aen_queue(struct tw_cli_ctlr_context *ctlr)
{
	struct tw_cli_req_context	*req;
	struct tw_cl_command_header	*cmd_hdr;
	TW_TIME				end_time;
	TW_UINT16			aen_code;
	TW_INT32			error;

	tw_cli_dbg_printf(4, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");

	for (;;) {
		if ((req = tw_cli_get_request(ctlr
#ifdef TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST
			, TW_CL_NULL
#endif /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */
			)) == TW_CL_NULL) {
			error = TW_OSL_EBUSY;
			break;
		}

#ifdef TW_OSL_DMA_MEM_ALLOC_PER_REQUEST

		req->cmd_pkt = ctlr->cmd_pkt_buf;
		req->cmd_pkt_phys = ctlr->cmd_pkt_phys;
		tw_osl_memzero(req->cmd_pkt,
			sizeof(struct tw_cl_command_header) +
			28 /* max bytes before sglist */);

#endif /* TW_OSL_DMA_MEM_ALLOC_PER_REQUEST */

		req->flags |= TW_CLI_REQ_FLAGS_INTERNAL;
		req->tw_cli_callback = TW_CL_NULL;
		if ((error = tw_cli_send_scsi_cmd(req,
				0x03 /* REQUEST_SENSE */))) {
			tw_cli_dbg_printf(1, ctlr->ctlr_handle,
				tw_osl_cur_func(),
				"Cannot send command to fetch aen");
			break;
		}

		end_time = tw_osl_get_local_time() +
			TW_CLI_REQUEST_TIMEOUT_PERIOD;
		do {
			if ((error = req->error_code))
				/*
				 * This will take care of completion due to
				 * a reset, or a failure in
				 * tw_cli_submit_pending_queue.
				 */
				goto out;

			tw_cli_process_resp_intr(req->ctlr);

			if ((req->state != TW_CLI_REQ_STATE_BUSY) &&
				(req->state != TW_CLI_REQ_STATE_PENDING))
				break;
		} while (tw_osl_get_local_time() <= end_time);

		if (req->state != TW_CLI_REQ_STATE_COMPLETE) {
			error = TW_OSL_ETIMEDOUT;
			break;
		}

		if ((error = req->cmd_pkt->command.cmd_pkt_9k.status)) {
			cmd_hdr = &req->cmd_pkt->cmd_hdr;
			tw_cli_create_ctlr_event(ctlr,
				TW_CL_MESSAGE_SOURCE_CONTROLLER_ERROR,
				cmd_hdr);
			break;
		}

		aen_code = tw_cli_manage_aen(ctlr, req);
		if (aen_code == TWA_AEN_QUEUE_EMPTY)
			break;
		if (aen_code == TWA_AEN_SYNC_TIME_WITH_HOST)
			continue;

		ctlr->state &= ~TW_CLI_CTLR_STATE_INTERNAL_REQ_BUSY;
		tw_cli_req_q_insert_tail(req, TW_CLI_FREE_Q);
	}

out:
	if (req) {
		if (req->data)
			ctlr->state &= ~TW_CLI_CTLR_STATE_INTERNAL_REQ_BUSY;
		tw_cli_req_q_insert_tail(req, TW_CLI_FREE_Q);
	}
	return(error);
}



/*
 * Function name:	tw_cli_find_aen
 * Description:		Reports whether a given AEN ever occurred.
 *
 * Input:		ctlr	-- ptr to CL internal ctlr context
 *			aen_code-- AEN to look for
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
TW_INT32
tw_cli_find_aen(struct tw_cli_ctlr_context *ctlr, TW_UINT16 aen_code)
{
	TW_UINT32	last_index;
	TW_INT32	i;

	tw_cli_dbg_printf(4, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");

	if (ctlr->aen_q_wrapped)
		last_index = ctlr->aen_head;
	else
		last_index = 0;

	i = ctlr->aen_head;
	do {
		i = (i + ctlr->max_aens_supported - 1) %
			ctlr->max_aens_supported;
		if (ctlr->aen_queue[i].aen_code == aen_code)
			return(TW_OSL_ESUCCESS);
	} while (i != last_index);

	return(TW_OSL_EGENFAILURE);
}



/*
 * Function name:	tw_cli_poll_status
 * Description:		Poll for a given status to show up in the firmware
 *			status register.
 *
 * Input:		ctlr	-- ptr to CL internal ctlr context
 *			status	-- status to look for
 *			timeout -- max # of seconds to wait before giving up
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
TW_INT32
tw_cli_poll_status(struct tw_cli_ctlr_context *ctlr, TW_UINT32 status,
	TW_UINT32 timeout)
{
	TW_TIME		end_time;
	TW_UINT32	status_reg;

	tw_cli_dbg_printf(4, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");

	end_time = tw_osl_get_local_time() + timeout;
	do {
		status_reg = TW_CLI_READ_STATUS_REGISTER(ctlr->ctlr_handle);
		if ((status_reg & status) == status)
			/* got the required bit(s) */
			return(TW_OSL_ESUCCESS);

		/*
		 * The OSL should not define TW_OSL_CAN_SLEEP if it calls
		 * tw_cl_deferred_interrupt from within the ISR and not a
		 * lower interrupt level, since, in that case, we might end
		 * up here, and try to sleep (within an ISR).
		 */
#ifndef TW_OSL_CAN_SLEEP
		/* OSL doesn't support sleeping; will spin. */
		tw_osl_delay(1000);
#else /* TW_OSL_CAN_SLEEP */
#if 0
		/* Will spin if initializing, sleep otherwise. */
		if (!(ctlr->state & TW_CLI_CTLR_STATE_ACTIVE))
			tw_osl_delay(1000);
		else
			tw_osl_sleep(ctlr->ctlr_handle,
				&(ctlr->sleep_handle), 1 /* ms */);
#else /* #if 0 */
		/*
		 * Will always spin for now (since reset holds a spin lock).
		 * We could free io_lock after the call to TW_CLI_SOFT_RESET,
		 * so we could sleep here.  To block new requests (since
		 * the lock will have been released) we could use the
		 * ...RESET_IN_PROGRESS flag.  Need to revisit.
		 */
		tw_osl_delay(1000);
#endif /* #if 0 */
#endif /* TW_OSL_CAN_SLEEP */
	} while (tw_osl_get_local_time() <= end_time);

	return(TW_OSL_ETIMEDOUT);
}



/*
 * Function name:	tw_cl_create_event
 * Description:		Creates and queues ctlr/CL/OSL AEN's to be
 *			supplied to user-space tools on request.
 *			Also notifies OS Layer.
 * Input:		ctlr	-- ptr to CL internal ctlr context
 *			queue_event-- TW_CL_TRUE --> queue event;
 *				      TW_CL_FALSE--> don't queue event
 *							(simply notify OSL)
 *			event_src  -- source of event
 *			event_code -- AEN/error code
 *			severity -- severity of event
 *			severity_str--Text description of severity
 *			event_desc -- standard string related to the event/error
 *			event_specific_desc -- format string for additional
 *						info about the event
 *			... -- additional arguments conforming to the format
 *				specified by event_specific_desc
 * Output:		None
 * Return value:	None
 */
TW_VOID
tw_cl_create_event(struct tw_cl_ctlr_handle *ctlr_handle,
	TW_UINT8 queue_event, TW_UINT8 event_src, TW_UINT16 event_code,
	TW_UINT8 severity, TW_UINT8 *severity_str, TW_UINT8 *event_desc,
	TW_UINT8 *event_specific_desc, ...)
{
	struct tw_cli_ctlr_context	*ctlr = ctlr_handle->cl_ctlr_ctxt;
	struct tw_cl_event_packet	event_pkt;
	struct tw_cl_event_packet	*event;
	va_list				ap;

	tw_cli_dbg_printf(8, ctlr_handle, tw_osl_cur_func(), "entered");

	if ((ctlr) && (queue_event)) {
		/* Protect access to ctlr->aen_head. */
		tw_osl_get_lock(ctlr_handle, ctlr->gen_lock);

		/* Queue the event. */
		event = &(ctlr->aen_queue[ctlr->aen_head]);
		tw_osl_memzero(event->parameter_data,
			sizeof(event->parameter_data));

		if (event->retrieved == TW_CL_AEN_NOT_RETRIEVED)
			ctlr->aen_q_overflow = TW_CL_TRUE;
		event->sequence_id = ++(ctlr->aen_cur_seq_id);
		if ((ctlr->aen_head + 1) == ctlr->max_aens_supported) {
			tw_cli_dbg_printf(4, ctlr->ctlr_handle,
				tw_osl_cur_func(), "AEN queue wrapped");
			ctlr->aen_q_wrapped = TW_CL_TRUE;
		}
	} else {
		event = &event_pkt;
		tw_osl_memzero(event, sizeof(struct tw_cl_event_packet));
	}

	event->event_src = event_src;
	event->time_stamp_sec = (TW_UINT32)tw_osl_get_local_time();
	event->aen_code = event_code;
	event->severity = severity;
	tw_osl_strcpy(event->severity_str, severity_str);
	event->retrieved = TW_CL_AEN_NOT_RETRIEVED;

	va_start(ap, event_specific_desc);
	tw_osl_vsprintf(event->parameter_data, event_specific_desc, ap);
	va_end(ap);

	event->parameter_len =
		(TW_UINT8)(tw_osl_strlen(event->parameter_data));
	tw_osl_strcpy(event->parameter_data + event->parameter_len + 1,
		event_desc);
	event->parameter_len += (1 + tw_osl_strlen(event_desc));

	tw_cli_dbg_printf(4, ctlr_handle, tw_osl_cur_func(),
		"event = %x %x %x %x %x %x %x\n %s",
		event->sequence_id,
		event->time_stamp_sec,
		event->aen_code,
		event->severity,
		event->retrieved,
		event->repeat_count,
		event->parameter_len,
		event->parameter_data);

	if ((ctlr) && (queue_event)) {
		ctlr->aen_head =
			(ctlr->aen_head + 1) % ctlr->max_aens_supported;
		/* Free access to ctlr->aen_head. */
		tw_osl_free_lock(ctlr_handle, ctlr->gen_lock);
	}
	tw_osl_notify_event(ctlr_handle, event);
}



/*
 * Function name:	tw_cli_get_request
 * Description:		Gets a request pkt from the free queue.
 *
 * Input:		ctlr	-- ptr to CL internal ctlr context
 *			req_pkt -- ptr to OSL built req_pkt, if there's one
 * Output:		None
 * Return value:	ptr to request pkt	-- success
 *			TW_CL_NULL		-- failure
 */
struct tw_cli_req_context *
tw_cli_get_request(struct tw_cli_ctlr_context *ctlr
#ifdef TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST
	, struct tw_cl_req_packet *req_pkt
#endif /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */
	)
{
	struct tw_cli_req_context	*req;

	tw_cli_dbg_printf(4, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");

#ifdef TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST

	if (req_pkt) {
		if (ctlr->num_free_req_ids == 0) {
			DbgPrint("Out of req_ids!!\n");
			return(TW_CL_NULL);
		}
		ctlr->num_free_req_ids--;
		req = (struct tw_cli_req_context *)(req_pkt->non_dma_mem);
		req->ctlr = ctlr;
		req->request_id = ctlr->free_req_ids[ctlr->free_req_head];
		ctlr->busy_reqs[req->request_id] = req;
		ctlr->free_req_head = (ctlr->free_req_head + 1) %
			(ctlr->max_simult_reqs - 1);
	} else

#endif /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */
	{
		/* Get a free request packet. */
		req = tw_cli_req_q_remove_head(ctlr, TW_CLI_FREE_Q);
	}

	/* Initialize some fields to their defaults. */
	if (req) {
		req->req_handle = TW_CL_NULL;
		req->data = TW_CL_NULL;
		req->length = 0;
		req->data_phys = 0;
		req->state = TW_CLI_REQ_STATE_INIT; /* req being initialized */
		req->flags = 0;
		req->error_code = 0;
		req->orig_req = TW_CL_NULL;
		req->tw_cli_callback = TW_CL_NULL;

#ifndef TW_OSL_DMA_MEM_ALLOC_PER_REQUEST

		/*
		 * Look at the status field in the command packet to see how
		 * it completed the last time it was used, and zero out only
		 * the portions that might have changed.  Note that we don't
		 * care to zero out the sglist.
		 */
		if (req->cmd_pkt->command.cmd_pkt_9k.status)
			tw_osl_memzero(req->cmd_pkt,
				sizeof(struct tw_cl_command_header) +
				28 /* max bytes before sglist */);
		else
			tw_osl_memzero(&(req->cmd_pkt->command),
				28 /* max bytes before sglist */);

#endif /* TW_OSL_DMA_MEM_ALLOC_PER_REQUEST */
	}
	return(req);
}



/*
 * Function name:	tw_cli_dbg_printf
 * Description:		Calls OSL print function if dbg_level is appropriate
 *
 * Input:		dbg_level -- Determines whether or not to print
 *			ctlr_handle -- controller handle
 *			cur_func -- text name of calling function
 *			fmt -- format string for the arguments to follow
 *			... -- variable number of arguments, to be printed
 *				based on the fmt string
 * Output:		None
 * Return value:	None
 */
TW_VOID
tw_cli_dbg_printf(TW_UINT8 dbg_level,
	struct tw_cl_ctlr_handle *ctlr_handle, const TW_INT8 *cur_func,
	TW_INT8 *fmt, ...)
{
#ifdef TW_OSL_DEBUG
	TW_INT8	print_str[256];
	va_list	ap;

	tw_osl_memzero(print_str, 256);
	if (dbg_level <= TW_OSL_DEBUG_LEVEL_FOR_CL) {
		tw_osl_sprintf(print_str, "%s: ", cur_func);

		va_start(ap, fmt);
		tw_osl_vsprintf(print_str + tw_osl_strlen(print_str), fmt, ap);
		va_end(ap);

		tw_osl_strcpy(print_str + tw_osl_strlen(print_str), "\n");
		tw_osl_dbg_printf(ctlr_handle, print_str);
	}
#endif /* TW_OSL_DEBUG */
}



/*
 * Function name:	tw_cli_notify_ctlr_info
 * Description:		Notify OSL of controller info (fw/BIOS versions, etc.).
 *
 * Input:		ctlr	-- ptr to CL internal ctlr context
 * Output:		None
 * Return value:	None
 */
TW_VOID
tw_cli_notify_ctlr_info(struct tw_cli_ctlr_context *ctlr)
{
	TW_INT8		fw_ver[16];
	TW_INT8		bios_ver[16];
	TW_INT32	error[2];
	TW_UINT8	num_ports = 0;

	tw_cli_dbg_printf(5, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");

	/* Get the port count. */
	error[0] = tw_cli_get_param(ctlr, TWA_PARAM_CONTROLLER_TABLE,
			TWA_PARAM_CONTROLLER_PORT_COUNT, &num_ports,
			1, TW_CL_NULL);

	/* Get the firmware and BIOS versions. */
	error[0] = tw_cli_get_param(ctlr, TWA_PARAM_VERSION_TABLE,
			TWA_PARAM_VERSION_FW, fw_ver, 16, TW_CL_NULL);
	error[1] = tw_cli_get_param(ctlr, TWA_PARAM_VERSION_TABLE,
			TWA_PARAM_VERSION_BIOS, bios_ver, 16, TW_CL_NULL);

	tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
		TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
		0x1300, 0x3, TW_CL_SEVERITY_INFO_STRING,
		"Controller details:",
		"%d ports, Firmware %.16s, BIOS %.16s",
		num_ports,
		error[0]?(TW_INT8 *)TW_CL_NULL:fw_ver,
		error[1]?(TW_INT8 *)TW_CL_NULL:bios_ver);
}



/*
 * Function name:	tw_cli_check_ctlr_state
 * Description:		Makes sure that the fw status register reports a
 *			proper status.
 *
 * Input:		ctlr	-- ptr to CL internal ctlr context
 *			status_reg-- value in the status register
 * Output:		None
 * Return value:	0	-- no errors
 *			non-zero-- errors
 */
TW_INT32
tw_cli_check_ctlr_state(struct tw_cli_ctlr_context *ctlr, TW_UINT32 status_reg)
{
	struct tw_cl_ctlr_handle	*ctlr_handle = ctlr->ctlr_handle;
	TW_INT32			error = TW_OSL_ESUCCESS;

	tw_cli_dbg_printf(8, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");

	/* Check if the 'micro-controller ready' bit is not set. */
	if ((status_reg & TWA_STATUS_EXPECTED_BITS) !=
				TWA_STATUS_EXPECTED_BITS) {
		TW_INT8	desc[200];

		tw_osl_memzero(desc, 200);
		tw_cl_create_event(ctlr_handle, TW_CL_TRUE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_EVENT,
			0x1301, 0x1, TW_CL_SEVERITY_ERROR_STRING,
			"Missing expected status bit(s)",
			"status reg = 0x%x; Missing bits: %s",
			status_reg,
			tw_cli_describe_bits (~status_reg &
				TWA_STATUS_EXPECTED_BITS, desc));
		error = TW_OSL_EGENFAILURE;
	}

	/* Check if any error bits are set. */
	if ((status_reg & TWA_STATUS_UNEXPECTED_BITS) != 0) {
		TW_INT8	desc[200];

		tw_osl_memzero(desc, 200);
		tw_cl_create_event(ctlr_handle, TW_CL_TRUE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_EVENT,
			0x1302, 0x1, TW_CL_SEVERITY_ERROR_STRING,
			"Unexpected status bit(s)",
			"status reg = 0x%x Unexpected bits: %s",
			status_reg & TWA_STATUS_UNEXPECTED_BITS,
			tw_cli_describe_bits(status_reg &
				TWA_STATUS_UNEXPECTED_BITS, desc));

		if (status_reg & TWA_STATUS_PCI_PARITY_ERROR_INTERRUPT) {
			tw_cl_create_event(ctlr_handle, TW_CL_TRUE,
				TW_CL_MESSAGE_SOURCE_COMMON_LAYER_EVENT,
				0x1303, 0x1, TW_CL_SEVERITY_ERROR_STRING,
				"PCI parity error: clearing... "
				"Re-seat/move/replace card",
				"status reg = 0x%x %s",
				status_reg,
				tw_cli_describe_bits(status_reg, desc));
			TW_CLI_WRITE_CONTROL_REGISTER(ctlr->ctlr_handle,
				TWA_CONTROL_CLEAR_PARITY_ERROR);

#ifdef TW_OSL_PCI_CONFIG_ACCESSIBLE
			tw_osl_write_pci_config(ctlr->ctlr_handle,
				TW_CLI_PCI_CONFIG_STATUS_OFFSET,
				TWA_PCI_CONFIG_CLEAR_PARITY_ERROR, 2);
#endif /* TW_OSL_PCI_CONFIG_ACCESSIBLE */
		
		}

		if (status_reg & TWA_STATUS_PCI_ABORT_INTERRUPT) {
			tw_cl_create_event(ctlr_handle, TW_CL_TRUE,
				TW_CL_MESSAGE_SOURCE_COMMON_LAYER_EVENT,
				0x1304, 0x1, TW_CL_SEVERITY_ERROR_STRING,
				"PCI abort: clearing... ",
				"status reg = 0x%x %s",
				status_reg,
				tw_cli_describe_bits(status_reg, desc));
			TW_CLI_WRITE_CONTROL_REGISTER(ctlr->ctlr_handle,
				TWA_CONTROL_CLEAR_PCI_ABORT);

#ifdef TW_OSL_PCI_CONFIG_ACCESSIBLE
			tw_osl_write_pci_config(ctlr->ctlr_handle,
				TW_CLI_PCI_CONFIG_STATUS_OFFSET,
				TWA_PCI_CONFIG_CLEAR_PCI_ABORT, 2);
#endif /* TW_OSL_PCI_CONFIG_ACCESSIBLE */

		}

		if (status_reg & TWA_STATUS_QUEUE_ERROR_INTERRUPT) {
			tw_cl_create_event(ctlr_handle, TW_CL_TRUE,
				TW_CL_MESSAGE_SOURCE_COMMON_LAYER_EVENT,
				0x1305, 0x1, TW_CL_SEVERITY_ERROR_STRING,
				"Controller queue error: clearing... ",
				"status reg = 0x%x %s",
				status_reg,
				tw_cli_describe_bits(status_reg, desc));
			TW_CLI_WRITE_CONTROL_REGISTER(ctlr->ctlr_handle,
				TWA_CONTROL_CLEAR_QUEUE_ERROR);
		}

		if (status_reg & TWA_STATUS_SBUF_WRITE_ERROR) {
			tw_cl_create_event(ctlr_handle, TW_CL_TRUE,
				TW_CL_MESSAGE_SOURCE_COMMON_LAYER_EVENT,
				0x1306, 0x1, TW_CL_SEVERITY_ERROR_STRING,
				"SBUF write error: clearing... ",
				"status reg = 0x%x %s",
				status_reg,
				tw_cli_describe_bits(status_reg, desc));
			TW_CLI_WRITE_CONTROL_REGISTER(ctlr->ctlr_handle,
				TWA_CONTROL_CLEAR_SBUF_WRITE_ERROR);
		}

		if (status_reg & TWA_STATUS_MICROCONTROLLER_ERROR) {
			tw_cl_create_event(ctlr_handle, TW_CL_TRUE,
				TW_CL_MESSAGE_SOURCE_COMMON_LAYER_EVENT,
				0x1307, 0x1, TW_CL_SEVERITY_ERROR_STRING,
				"Micro-controller error! ",
				"status reg = 0x%x %s",
				status_reg,
				tw_cli_describe_bits(status_reg, desc));
			error = TW_OSL_EGENFAILURE;
		}
	}
	return(error);
}	



/*
 * Function name:	tw_cli_describe_bits
 * Description:		Given the value of the status register, returns a
 *			string describing the meaning of each set bit.
 *
 * Input:		reg -- status register value
 * Output:		Pointer to a string describing each set bit
 * Return value:	Pointer to the string describing each set bit
 */
TW_INT8	*
tw_cli_describe_bits(TW_UINT32 reg, TW_INT8 *str)
{
	tw_osl_strcpy(str, "[");

	if (reg	& TWA_STATUS_SBUF_WRITE_ERROR)
		tw_osl_strcpy(str, "SBUF_WR_ERR,");
	if (reg & TWA_STATUS_COMMAND_QUEUE_EMPTY)
		tw_osl_strcpy(&str[tw_osl_strlen(str)], "CMD_Q_EMPTY,");
	if (reg & TWA_STATUS_MICROCONTROLLER_READY)
		tw_osl_strcpy(&str[tw_osl_strlen(str)], "MC_RDY,");
	if (reg & TWA_STATUS_RESPONSE_QUEUE_EMPTY)
		tw_osl_strcpy(&str[tw_osl_strlen(str)], "RESP_Q_EMPTY,");
	if (reg & TWA_STATUS_COMMAND_QUEUE_FULL)
		tw_osl_strcpy(&str[tw_osl_strlen(str)], "CMD_Q_FULL,");
	if (reg & TWA_STATUS_RESPONSE_INTERRUPT)
		tw_osl_strcpy(&str[tw_osl_strlen(str)], "RESP_INTR,");
	if (reg & TWA_STATUS_COMMAND_INTERRUPT)
		tw_osl_strcpy(&str[tw_osl_strlen(str)], "CMD_INTR,");
	if (reg & TWA_STATUS_ATTENTION_INTERRUPT)
		tw_osl_strcpy(&str[tw_osl_strlen(str)], "ATTN_INTR,");
	if (reg & TWA_STATUS_HOST_INTERRUPT)
		tw_osl_strcpy(&str[tw_osl_strlen(str)], "HOST_INTR,");
	if (reg & TWA_STATUS_PCI_ABORT_INTERRUPT)
		tw_osl_strcpy(&str[tw_osl_strlen(str)], "PCI_ABRT,");
	if (reg & TWA_STATUS_MICROCONTROLLER_ERROR)
		tw_osl_strcpy(&str[tw_osl_strlen(str)], "MC_ERR,");
	if (reg & TWA_STATUS_QUEUE_ERROR_INTERRUPT)
		tw_osl_strcpy(&str[tw_osl_strlen(str)], "Q_ERR,");
	if (reg & TWA_STATUS_PCI_PARITY_ERROR_INTERRUPT)
		tw_osl_strcpy(&str[tw_osl_strlen(str)], "PCI_PERR");

	tw_osl_strcpy(&str[tw_osl_strlen(str)], "]");
	return(str);
}



#ifdef TW_OSL_DEBUG

/*
 * Function name:	tw_cl_print_ctlr_stats
 * Description:		Prints the current status of the controller.
 *
 * Input:		ctlr_handle-- controller handle
 * Output:		None
 * Return value:	None
 */
TW_VOID
tw_cl_print_ctlr_stats(struct tw_cl_ctlr_handle *ctlr_handle)
{
	struct tw_cli_ctlr_context	*ctlr =
		(struct tw_cli_ctlr_context *)(ctlr_handle->cl_ctlr_ctxt);
	TW_UINT32			status_reg;
	TW_INT8				desc[200];

	tw_cli_dbg_printf(7, ctlr->ctlr_handle, "", "entered");

	/* Print current controller details. */
	tw_cli_dbg_printf(0, ctlr_handle, "", "cl_ctlr_ctxt = %p", ctlr);

	tw_osl_memzero(desc, 200);
	status_reg = TW_CLI_READ_STATUS_REGISTER(ctlr_handle);
	tw_cli_dbg_printf(0, ctlr_handle, "", "status reg = 0x%x %s",
		status_reg, tw_cli_describe_bits(status_reg, desc));

	tw_cli_dbg_printf(0, ctlr_handle, "", "CLq type  current  max");
	tw_cli_dbg_printf(0, ctlr_handle, "", "free      %04d     %04d",
		ctlr->q_stats[TW_CLI_FREE_Q].cur_len,
		ctlr->q_stats[TW_CLI_FREE_Q].max_len);
	tw_cli_dbg_printf(0, ctlr_handle, "", "busy      %04d     %04d",
		ctlr->q_stats[TW_CLI_BUSY_Q].cur_len,
		ctlr->q_stats[TW_CLI_BUSY_Q].max_len);
	tw_cli_dbg_printf(0, ctlr_handle, "", "pending   %04d     %04d",
		ctlr->q_stats[TW_CLI_PENDING_Q].cur_len,
		ctlr->q_stats[TW_CLI_PENDING_Q].max_len);
	tw_cli_dbg_printf(0, ctlr_handle, "", "complete  %04d     %04d",
		ctlr->q_stats[TW_CLI_COMPLETE_Q].cur_len,
		ctlr->q_stats[TW_CLI_COMPLETE_Q].max_len);
	tw_cli_dbg_printf(0, ctlr_handle, "", "AEN queue head %d  tail %d",
			ctlr->aen_head, ctlr->aen_tail);
}	



/*
 * Function name:	tw_cl_reset_stats
 * Description:		Resets CL maintained statistics for the controller.
 *
 * Input:		ctlr_handle-- controller handle
 * Output:		None
 * Return value:	None
 */
TW_VOID
tw_cl_reset_stats(struct tw_cl_ctlr_handle *ctlr_handle)
{
	struct tw_cli_ctlr_context	*ctlr =
		(struct tw_cli_ctlr_context *)(ctlr_handle->cl_ctlr_ctxt);

	tw_cli_dbg_printf(7, ctlr_handle, tw_osl_cur_func(), "entered");
	ctlr->q_stats[TW_CLI_FREE_Q].max_len = 0;
	ctlr->q_stats[TW_CLI_BUSY_Q].max_len = 0;
	ctlr->q_stats[TW_CLI_PENDING_Q].max_len = 0;
	ctlr->q_stats[TW_CLI_COMPLETE_Q].max_len = 0;
}



/*
 * Function name:	tw_cli_print_req_info
 * Description:		Prints CL internal details of a given request.
 *
 * Input:		req	-- ptr to CL internal request context
 * Output:		None
 * Return value:	None
 */
TW_VOID
tw_cl_print_req_info(struct tw_cl_req_handle *req_handle)
{
	struct tw_cli_req_context	*req = req_handle->cl_req_ctxt;
	struct tw_cli_ctlr_context	*ctlr = req->ctlr;
	struct tw_cl_ctlr_handle	*ctlr_handle = ctlr->ctlr_handle;
	struct tw_cl_command_packet	*cmd_pkt = req->cmd_pkt;
	struct tw_cl_command_9k		*cmd9k;
	union tw_cl_command_7k		*cmd7k;
	TW_UINT8			*cdb;
	TW_VOID				*sgl;
	TW_UINT32			sgl_entries;
	TW_UINT32			i;

	tw_cli_dbg_printf(0, ctlr_handle, tw_osl_cur_func(),
		"CL details for request:");
	tw_cli_dbg_printf(0, ctlr_handle, tw_osl_cur_func(),
		"req_handle = %p, ctlr = %p,\n"
		"cmd_pkt = %p, cmd_pkt_phys = 0x%llx,\n"
		"data = %p, length = 0x%x, data_phys = 0x%llx,\n"
		"state = 0x%x, flags = 0x%x, error = 0x%x,\n"
		"orig_req = %p, callback = %p, req_id = 0x%x,\n"
		"next_req = %p, prev_req = %p",
		req_handle, ctlr,
		cmd_pkt, req->cmd_pkt_phys,
		req->data, req->length, req->data_phys,
		req->state, req->flags, req->error_code,
		req->orig_req, req->tw_cli_callback, req->request_id,
		req->link.next, req->link.prev);

	if (req->flags & TW_CLI_REQ_FLAGS_9K) {
		cmd9k = &(cmd_pkt->command.cmd_pkt_9k);
		sgl = cmd9k->sg_list;
		sgl_entries = TW_CL_SWAP16(
			GET_SGL_ENTRIES(cmd9k->lun_h4__sgl_entries));
		tw_cli_dbg_printf(0, ctlr_handle, tw_osl_cur_func(),
			"9K cmd: opcode = 0x%x, unit = 0x%x, req_id = 0x%x,\n"
			"status = 0x%x, sgl_offset = 0x%x, sgl_entries = 0x%x",
			GET_OPCODE(cmd9k->res__opcode),
			cmd9k->unit,
			TW_CL_SWAP16(GET_REQ_ID(cmd9k->lun_l4__req_id)),
			cmd9k->status,
			cmd9k->sgl_offset,
			sgl_entries);

		cdb = (TW_UINT8 *)(cmd9k->cdb);
		tw_cli_dbg_printf(0, ctlr_handle, tw_osl_cur_func(),
			"CDB: %x %x %x %x %x %x %x %x"
			"%x %x %x %x %x %x %x %x",
			cdb[0], cdb[1], cdb[2], cdb[3],
			cdb[4], cdb[5], cdb[6], cdb[7],
			cdb[8], cdb[9], cdb[10], cdb[11],
			cdb[12], cdb[13], cdb[14], cdb[15]);
	} else {
		cmd7k = &(cmd_pkt->command.cmd_pkt_7k);
		sgl = cmd7k->param.sgl;
		sgl_entries = (cmd7k->generic.size -
			GET_SGL_OFF(cmd7k->generic.sgl_off__opcode)) /
			((ctlr->flags & TW_CL_64BIT_ADDRESSES) ? 3 : 2);
		tw_cli_dbg_printf(0, ctlr_handle, tw_osl_cur_func(),
			"7K cmd: opcode = 0x%x, sgl_offset = 0x%x,\n"
			"size = 0x%x, req_id = 0x%x, unit = 0x%x,\n"
			"status = 0x%x, flags = 0x%x, count = 0x%x",
			GET_OPCODE(cmd7k->generic.sgl_off__opcode),
			GET_SGL_OFF(cmd7k->generic.sgl_off__opcode),
			cmd7k->generic.size,
			TW_CL_SWAP16(cmd7k->generic.request_id),
			GET_UNIT(cmd7k->generic.host_id__unit),
			cmd7k->generic.status,
			cmd7k->generic.flags,
			TW_CL_SWAP16(cmd7k->generic.count));
	}

	tw_cli_dbg_printf(0, ctlr_handle, tw_osl_cur_func(), "SG entries:");

	if (ctlr->flags & TW_CL_64BIT_ADDRESSES) {
		struct tw_cl_sg_desc64 *sgl64 = (struct tw_cl_sg_desc64 *)sgl;

		for (i = 0; i < sgl_entries; i++) {
			tw_cli_dbg_printf(0, ctlr_handle, tw_osl_cur_func(),
				"0x%llx  0x%x",
				sgl64[i].address, sgl64[i].length);
		}
	} else {
		struct tw_cl_sg_desc32 *sgl32 = (struct tw_cl_sg_desc32 *)sgl;

		for (i = 0; i < sgl_entries; i++) {
			tw_cli_dbg_printf(0, ctlr_handle, tw_osl_cur_func(),
				"0x%x  0x%x",
				sgl32[i].address, sgl32[i].length);
		}
	}
}

#endif /* TW_OSL_DEBUG */

