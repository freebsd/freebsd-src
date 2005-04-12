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
 * Common Layer initialization functions.
 */


#include "tw_osl_share.h"
#include "tw_cl_share.h"
#include "tw_cl_fwif.h"
#include "tw_cl_ioctl.h"
#include "tw_cl.h"
#include "tw_cl_externs.h"
#include "tw_osl_ioctl.h"


/*
 * Function name:	tw_cl_ctlr_supported
 * Description:		Determines if a controller is supported.
 *
 * Input:		vendor_id -- vendor id of the controller
 *			device_id -- device id of the controller
 * Output:		None
 * Return value:	TW_CL_TRUE-- controller supported
 *			TW_CL_FALSE-- controller not supported
 */
TW_INT32
tw_cl_ctlr_supported(TW_INT32 vendor_id, TW_INT32 device_id)
{
	if ((vendor_id == TW_CL_VENDOR_ID) && (device_id == TW_CL_DEVICE_ID_9K))
		return(TW_CL_TRUE);
	return(TW_CL_FALSE);
}



/*
 * Function name:	tw_cl_get_mem_requirements
 * Description:		Provides info about Common Layer requirements for a
 *			controller, given the controller type (in 'flags').
 * Input:		ctlr_handle -- controller handle
 *			flags -- more info passed by the OS Layer
 *			max_simult_reqs -- maximum # of simultaneous
 *					requests that the OS Layer expects
 *					the Common Layer to support
 *			max_aens -- maximun # of AEN's needed to be supported
 * Output:		alignment -- alignment needed for all DMA'able
 *					buffers
 *			sg_size_factor -- every SG element should have a size
 *					that's a multiple of this number
 *			non_dma_mem_size -- # of bytes of memory needed for
 *					non-DMA purposes
 *			dma_mem_size -- # of bytes of DMA'able memory needed
 *			flash_dma_mem_size -- # of bytes of DMA'able memory
 *					needed for firmware flash, if applicable
 *			per_req_dma_mem_size -- # of bytes of DMA'able memory
 *					needed per request, if applicable
 *			per_req_non_dma_mem_size -- # of bytes of memory needed
 *					per request for non-DMA purposes,
 *					if applicable
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
TW_INT32
tw_cl_get_mem_requirements(struct tw_cl_ctlr_handle *ctlr_handle,
	TW_UINT32 flags, TW_INT32 max_simult_reqs, TW_INT32 max_aens,
	TW_UINT32 *alignment, TW_UINT32 *sg_size_factor,
	TW_UINT32 *non_dma_mem_size, TW_UINT32 *dma_mem_size
#ifdef TW_OSL_FLASH_FIRMWARE
	, TW_UINT32 *flash_dma_mem_size
#endif /* TW_OSL_FLASH_FIRMWARE */
#ifdef TW_OSL_DMA_MEM_ALLOC_PER_REQUEST
	, TW_UINT32 *per_req_dma_mem_size
#endif /* TW_OSL_DMA_MEM_ALLOC_PER_REQUEST */
#ifdef TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST
	, TW_UINT32 *per_req_non_dma_mem_size
#endif /* TW_OSL_N0N_DMA_MEM_ALLOC_PER_REQUEST */
	)
{
	if (max_simult_reqs > TW_CL_MAX_SIMULTANEOUS_REQUESTS) {
		tw_cl_create_event(ctlr_handle, TW_CL_FALSE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
			0x1000, 0x1, TW_CL_SEVERITY_ERROR_STRING,
			"Too many simultaneous requests to support!",
			"requested = %d, supported = %d, error = %d\n",
			max_simult_reqs, TW_CL_MAX_SIMULTANEOUS_REQUESTS,
			TW_OSL_EBIG);
		return(TW_OSL_EBIG);
	}

	*alignment = TWA_ALIGNMENT;
	*sg_size_factor = TWA_SG_ELEMENT_SIZE_FACTOR;

	/*
	 * Total non-DMA memory needed is the sum total of memory needed for
	 * the controller context, request packets (including the 1 needed for
	 * CL internal requests), and event packets.
	 */
#ifdef TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST

	*non_dma_mem_size = sizeof(struct tw_cli_ctlr_context) +
		(sizeof(struct tw_cli_req_context)) +
		(sizeof(struct tw_cl_event_packet) * max_aens);
	*per_req_non_dma_mem_size = sizeof(struct tw_cli_req_context);

#else /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */

	*non_dma_mem_size = sizeof(struct tw_cli_ctlr_context) +
		(sizeof(struct tw_cli_req_context) * (max_simult_reqs + 1)) +
		(sizeof(struct tw_cl_event_packet) * max_aens);

#endif /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */

	/*
	 * Total DMA'able memory needed is the sum total of memory needed for
	 * all command packets (including the 1 needed for CL internal
	 * requests), and memory needed to hold the payload for internal
	 * requests.
	 */
#ifdef TW_OSL_DMA_MEM_ALLOC_PER_REQUEST

	*dma_mem_size = sizeof(struct tw_cl_command_packet) +
		TW_CLI_SECTOR_SIZE;
	*per_req_dma_mem_size = sizeof(struct tw_cl_command_packet);

#else /* TW_OSL_DMA_MEM_ALLOC_PER_REQUEST */

	*dma_mem_size = (sizeof(struct tw_cl_command_packet) *
		(max_simult_reqs + 1)) + (TW_CLI_SECTOR_SIZE);

#endif /* TW_OSL_DMA_MEM_ALLOC_PER_REQUEST */


#ifdef TW_OSL_FLASH_FIRMWARE

	/* Memory needed to hold the firmware image while flashing. */
	*flash_dma_mem_size =
		((tw_cli_fw_img_size / TW_CLI_NUM_FW_IMAGE_CHUNKS) +
		(TWA_SG_ELEMENT_SIZE_FACTOR - 1)) &
		~(TWA_SG_ELEMENT_SIZE_FACTOR - 1);

#endif /* TW_OSL_FLASH_FIRMWARE */

	return(0);
}



/*
 * Function name:	tw_cl_init_ctlr
 * Description:		Initializes driver data structures for the controller.
 *
 * Input:		ctlr_handle -- controller handle
 *			flags -- more info passed by the OS Layer
 *			max_simult_reqs -- maximum # of simultaneous requests
 *					that the OS Layer expects the Common
 *					Layer to support
 *			max_aens -- maximun # of AEN's needed to be supported
 *			non_dma_mem -- ptr to allocated non-DMA memory
 *			dma_mem -- ptr to allocated DMA'able memory
 *			dma_mem_phys -- physical address of dma_mem
 *			flash_dma_mem -- ptr to allocated DMA'able memory
 *				needed for firmware flash, if applicable
 *			flash_dma_mem_phys -- physical address of flash_dma_mem
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
TW_INT32
tw_cl_init_ctlr(struct tw_cl_ctlr_handle *ctlr_handle, TW_UINT32 flags,
	TW_INT32 max_simult_reqs, TW_INT32 max_aens, TW_VOID *non_dma_mem,
	TW_VOID *dma_mem, TW_UINT64 dma_mem_phys
#ifdef TW_OSL_FLASH_FIRMWARE
	, TW_VOID *flash_dma_mem,
	TW_UINT64 flash_dma_mem_phys
#endif /* TW_OSL_FLASH_FIRMWARE */
	)
{
	struct tw_cli_ctlr_context	*ctlr;
	struct tw_cli_req_context	*req;
	TW_UINT8			*free_non_dma_mem;
	TW_INT32			error = TW_OSL_ESUCCESS;
	TW_INT32			i;

	tw_cli_dbg_printf(3, ctlr_handle, tw_osl_cur_func(), "entered");

	if (flags & TW_CL_START_CTLR_ONLY) {
		ctlr = (struct tw_cli_ctlr_context *)
			(ctlr_handle->cl_ctlr_ctxt);
		goto start_ctlr;
	}

	if (max_simult_reqs > TW_CL_MAX_SIMULTANEOUS_REQUESTS) {
		tw_cl_create_event(ctlr_handle, TW_CL_FALSE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
			0x1000, 0x1, TW_CL_SEVERITY_ERROR_STRING,
			"Too many simultaneous requests to support!",
			"requested = %d, supported = %d, error = %d\n",
			max_simult_reqs, TW_CL_MAX_SIMULTANEOUS_REQUESTS,
			TW_OSL_EBIG);
		return(TW_OSL_EBIG);
	}

	if ((non_dma_mem == TW_CL_NULL) || (dma_mem == TW_CL_NULL)
#ifdef TW_OSL_FLASH_FIRMWARE
		|| ((flags & TW_CL_FLASH_FIRMWARE) ?
		(flash_dma_mem == TW_CL_NULL) : TW_CL_FALSE)
#endif /* TW_OSL_FLASH_FIRMWARE */
		) {
		tw_cl_create_event(ctlr_handle, TW_CL_FALSE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
			0x1001, 0x1, TW_CL_SEVERITY_ERROR_STRING,
			"Insufficient memory for Common Layer's internal usage",
			"error = %d\n", TW_OSL_ENOMEM);
		return(TW_OSL_ENOMEM);
	}

#ifdef TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST
	tw_osl_memzero(non_dma_mem, sizeof(struct tw_cli_ctlr_context) +
		sizeof(struct tw_cli_req_context) +
		(sizeof(struct tw_cl_event_packet) * max_aens));
#else /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */
	tw_osl_memzero(non_dma_mem, sizeof(struct tw_cli_ctlr_context) +
		(sizeof(struct tw_cli_req_context) * (max_simult_reqs + 1)) +
		(sizeof(struct tw_cl_event_packet) * max_aens));
#endif /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */

#ifdef TW_OSL_DMA_MEM_ALLOC_PER_REQUEST
	tw_osl_memzero(dma_mem,
		sizeof(struct tw_cl_command_packet) +
		TW_CLI_SECTOR_SIZE);
#else /* TW_OSL_DMA_MEM_ALLOC_PER_REQUEST */
	tw_osl_memzero(dma_mem,
		(sizeof(struct tw_cl_command_packet) *
		(max_simult_reqs + 1)) +
		TW_CLI_SECTOR_SIZE);
#endif /* TW_OSL_DMA_MEM_ALLOC_PER_REQUEST */


	free_non_dma_mem = (TW_UINT8 *)non_dma_mem;

	ctlr = (struct tw_cli_ctlr_context *)free_non_dma_mem;
	free_non_dma_mem += sizeof(struct tw_cli_ctlr_context);

	ctlr_handle->cl_ctlr_ctxt = ctlr;
	ctlr->ctlr_handle = ctlr_handle;

	ctlr->max_simult_reqs = max_simult_reqs + 1;
	ctlr->max_aens_supported = max_aens;
	ctlr->flags = flags;

#ifdef TW_OSL_FLASH_FIRMWARE
	ctlr->flash_dma_mem = flash_dma_mem;
	ctlr->flash_dma_mem_phys = flash_dma_mem_phys;
#endif /* TW_OSL_FLASH_FIRMWARE */

	/* Initialize queues of CL internal request context packets. */
	tw_cli_req_q_init(ctlr, TW_CLI_FREE_Q);
	tw_cli_req_q_init(ctlr, TW_CLI_BUSY_Q);
	tw_cli_req_q_init(ctlr, TW_CLI_PENDING_Q);
	tw_cli_req_q_init(ctlr, TW_CLI_COMPLETE_Q);

	/* Initialize all locks used by CL. */
	ctlr->gen_lock = &(ctlr->gen_lock_handle);
	tw_osl_init_lock(ctlr_handle, "tw_cl_gen_lock", ctlr->gen_lock);
	ctlr->io_lock = &(ctlr->io_lock_handle);
	tw_osl_init_lock(ctlr_handle, "tw_cl_io_lock", ctlr->io_lock);
	/*
	 * If 64 bit cmd pkt addresses are used, we will need to serialize
	 * writes to the hardware (across registers), since existing hardware
	 * will get confused if, for example, we wrote the low 32 bits of the
	 * cmd pkt address, followed by a response interrupt mask to the
	 * control register, followed by the high 32 bits of the cmd pkt
	 * address.  It will then interpret the value written to the control
	 * register as the low cmd pkt address.  So, for this case, we will
	 * only use one lock (io_lock) by making io_lock & intr_lock one and
	 * the same.
	 */
	if (ctlr->flags & TW_CL_64BIT_ADDRESSES)
		ctlr->intr_lock = ctlr->io_lock;
	else {
		ctlr->intr_lock = &(ctlr->intr_lock_handle);
		tw_osl_init_lock(ctlr_handle, "tw_cl_intr_lock",
			ctlr->intr_lock);
	}

	/* Initialize CL internal request context packets. */
	ctlr->req_ctxt_buf = (struct tw_cli_req_context *)free_non_dma_mem;
	free_non_dma_mem += (sizeof(struct tw_cli_req_context) *
		(
#ifndef TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST
		max_simult_reqs +
#endif /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */
		1));

	ctlr->cmd_pkt_buf = (struct tw_cl_command_packet *)dma_mem;
	ctlr->cmd_pkt_phys = dma_mem_phys;

	ctlr->internal_req_data = (TW_UINT8 *)
		(ctlr->cmd_pkt_buf +
		(
#ifndef TW_OSL_DMA_MEM_ALLOC_PER_REQUEST
		max_simult_reqs +
#endif /* TW_OSL_DMA_MEM_ALLOC_PER_REQUEST */
		1));
	ctlr->internal_req_data_phys = ctlr->cmd_pkt_phys +
		(sizeof(struct tw_cl_command_packet) *
		(
#ifndef TW_OSL_DMA_MEM_ALLOC_PER_REQUEST
		max_simult_reqs +
#endif /* TW_OSL_DMA_MEM_ALLOC_PER_REQUEST */
		1));

	for (i = 0;
		i < (
#ifndef TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST
		max_simult_reqs +
#endif /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */
		1); i++) {
		req = &(ctlr->req_ctxt_buf[i]);

#ifndef TW_OSL_DMA_MEM_ALLOC_PER_REQUEST

		req->cmd_pkt = &(ctlr->cmd_pkt_buf[i]);
		req->cmd_pkt_phys = ctlr->cmd_pkt_phys +
			(i * sizeof(struct tw_cl_command_packet));

#endif /* TW_OSL_DMA_MEM_ALLOC_PER_REQUEST */

		req->request_id = i;
		req->ctlr = ctlr;

#ifdef TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST
		req->flags |= TW_CLI_REQ_FLAGS_INTERNAL;
#endif /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */

		/* Insert request into the free queue. */
		tw_cli_req_q_insert_tail(req, TW_CLI_FREE_Q);
	}


#ifdef TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST

	ctlr->free_req_head = i - 1;
	ctlr->free_req_tail = i - 1;

	for (; i < (max_simult_reqs + 1); i++)
		ctlr->free_req_ids[i - 1] = i;

	ctlr->num_free_req_ids = max_simult_reqs;

#endif /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */


	/* Initialize the AEN queue. */
	ctlr->aen_queue = (struct tw_cl_event_packet *)free_non_dma_mem;


start_ctlr:
	/*
	 * Disable interrupts.  Interrupts will be enabled in tw_cli_start_ctlr
	 * (only) if initialization succeeded.
	 */
	tw_cli_disable_interrupts(ctlr);

	/* Initialize the controller. */
	if ((error = tw_cli_start_ctlr(ctlr))) {
		/* Soft reset the controller, and try one more time. */
		tw_cl_create_event(ctlr_handle, TW_CL_FALSE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
			0x1002, 0x1, TW_CL_SEVERITY_ERROR_STRING,
			"Controller initialization failed. Retrying...",
			"error = %d\n", error);
		if ((error = tw_cli_soft_reset(ctlr))) {
			tw_cl_create_event(ctlr_handle, TW_CL_FALSE,
				TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
				0x1003, 0x1, TW_CL_SEVERITY_ERROR_STRING,
				"Controller soft reset failed",
				"error = %d\n", error);
			return(error);
		} else if ((error = tw_cli_start_ctlr(ctlr))) {
			tw_cl_create_event(ctlr_handle, TW_CL_FALSE,
				TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
				0x1004, 0x1, TW_CL_SEVERITY_ERROR_STRING,
				"Controller initialization retry failed",
				"error = %d\n", error);
			return(error);
		}
	}
	/* Notify some info about the controller to the OSL. */
	tw_cli_notify_ctlr_info(ctlr);

	/* Mark the controller as active. */
	ctlr->state |= TW_CLI_CTLR_STATE_ACTIVE;
	return(error);
}



#ifdef TW_OSL_FLASH_FIRMWARE
/*
 * Function name:	tw_cli_flash_firmware
 * Description:		Flashes bundled firmware image onto controller.
 *
 * Input:		ctlr	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
TW_INT32
tw_cli_flash_firmware(struct tw_cli_ctlr_context *ctlr)
{
	struct tw_cli_req_context		*req;
	struct tw_cl_command_header		*cmd_hdr;
	struct tw_cl_command_download_firmware	*cmd;
	TW_UINT32				fw_img_chunk_size;
	TW_UINT32				num_chunks;
	TW_UINT32				this_chunk_size = 0;
	TW_INT32				remaining_img_size = 0;
	TW_INT32				hard_reset_needed = TW_CL_FALSE;
	TW_INT32				error = TW_OSL_EGENFAILURE;
	TW_UINT32				i;

	tw_cli_dbg_printf(3, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");
	if ((req = tw_cli_get_request(ctlr
#ifdef TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST
		, TW_CL_NULL
#endif /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */
		)) == TW_CL_NULL) {
		/* No free request packets available.  Can't proceed. */
		error = TW_OSL_EBUSY;
		goto out;
	}

#ifdef TW_OSL_DMA_MEM_ALLOC_PER_REQUEST

	req->cmd_pkt = ctlr->cmd_pkt_buf;
	req->cmd_pkt_phys = ctlr->cmd_pkt_phys;
	tw_osl_memzero(req->cmd_pkt,
		sizeof(struct tw_cl_command_header) +
		28 /* max bytes before sglist */);

#endif /* TW_OSL_DMA_MEM_ALLOC_PER_REQUEST */

	req->flags |= TW_CLI_REQ_FLAGS_INTERNAL;
	
	/*
	 * Determine amount of memory needed to hold a chunk of the
	 * firmware image.
	 */
	fw_img_chunk_size = ((tw_cli_fw_img_size / TW_CLI_NUM_FW_IMAGE_CHUNKS) +
		(TWA_SG_ELEMENT_SIZE_FACTOR - 1)) &
		~(TWA_SG_ELEMENT_SIZE_FACTOR - 1);

	/* Calculate the actual number of chunks needed. */
	num_chunks = (tw_cli_fw_img_size / fw_img_chunk_size) +
		((tw_cli_fw_img_size % fw_img_chunk_size) ? 1 : 0);

	req->data = ctlr->flash_dma_mem;
	req->data_phys = ctlr->flash_dma_mem_phys;

	remaining_img_size = tw_cli_fw_img_size;

	cmd_hdr = &(req->cmd_pkt->cmd_hdr);
	cmd = &(req->cmd_pkt->command.cmd_pkt_7k.download_fw);

	for (i = 0; i < num_chunks; i++) {
		/* Build a cmd pkt for downloading firmware. */
		tw_osl_memzero(req->cmd_pkt,
			sizeof(struct tw_cl_command_packet));

		cmd_hdr->header_desc.size_header = 128;

		/* sgl_offset (offset in dwords, to sg list) is 2. */
		cmd->sgl_off__opcode =
			BUILD_SGL_OFF__OPCODE(2, TWA_FW_CMD_DOWNLOAD_FIRMWARE);
		cmd->request_id = (TW_UINT8)(TW_CL_SWAP16(req->request_id));
		cmd->unit = 0;
		cmd->status = 0;
		cmd->flags = 0;
		cmd->param = TW_CL_SWAP16(8);	/* prom image */

		if (i != (num_chunks - 1))
			this_chunk_size = fw_img_chunk_size;
		else	 /* last chunk */
			this_chunk_size = remaining_img_size;
	
		remaining_img_size -= this_chunk_size;

		tw_osl_memcpy(req->data, tw_cli_fw_img + (i * fw_img_chunk_size),
			this_chunk_size);

		/*
		 * The next line will effect only the last chunk.
		 */
		req->length = (this_chunk_size +
			(TWA_SG_ELEMENT_SIZE_FACTOR - 1)) &
			~(TWA_SG_ELEMENT_SIZE_FACTOR - 1);

		if (ctlr->flags & TW_CL_64BIT_ADDRESSES) {
			((struct tw_cl_sg_desc64 *)(cmd->sgl))[0].address =
				TW_CL_SWAP64(req->data_phys);
			((struct tw_cl_sg_desc64 *)(cmd->sgl))[0].length =
				TW_CL_SWAP32(req->length);
			cmd->size = 2 + 3;
		} else {
			((struct tw_cl_sg_desc32 *)(cmd->sgl))[0].address =
				TW_CL_SWAP32(req->data_phys);
			((struct tw_cl_sg_desc32 *)(cmd->sgl))[0].length =
				TW_CL_SWAP32(req->length);
			cmd->size = 2 + 2;
		}

		error = tw_cli_submit_and_poll_request(req,
			TW_CLI_REQUEST_TIMEOUT_PERIOD);
		if (error) {
			tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
				TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
				0x1005, 0x1, TW_CL_SEVERITY_ERROR_STRING,
				"Firmware flash request could not be posted",
				"error = %d\n", error);
			if (error == TW_OSL_ETIMEDOUT)
				/* clean-up done by tw_cli_submit_and_poll_request */
				return(error);
			break;
		}
		error = cmd->status;

		if (((i == (num_chunks - 1)) && (error)) ||
			((i != (num_chunks - 1)) &&
			((error = cmd_hdr->status_block.error) !=
			TWA_ERROR_MORE_DATA))) {
				/*
				 * It's either that download of the last chunk
				 * failed, or the download of one of the earlier
				 * chunks failed with an error other than
				 * TWA_ERROR_MORE_DATA.  Report the error.
				 */
				tw_cli_create_ctlr_event(ctlr,
					TW_CL_MESSAGE_SOURCE_CONTROLLER_ERROR,
					cmd_hdr);
				tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
					TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
					0x1006, 0x1, TW_CL_SEVERITY_ERROR_STRING,
					"Firmware flash failed",
					"cmd = 0x%x, chunk # %d, cmd status = %d",
					GET_OPCODE(cmd->sgl_off__opcode),
					i, cmd->status);
				/*
				 * Make a note to hard reset the controller,
				 * so that it doesn't wait for the remaining
				 * chunks.  Don't call the hard reset function
				 * right here, since we have committed to having
				 * only 1 active internal request at a time, and
				 * this request has not yet been freed.
				 */
				hard_reset_needed = TW_CL_TRUE;
				break;
		}
	} /* for */

out:
	if (req)
		tw_cli_req_q_insert_tail(req, TW_CLI_FREE_Q);

	if (hard_reset_needed)
		tw_cli_hard_reset(ctlr);

	return(error);
}



/*
 * Function name:	tw_cli_hard_reset
 * Description:		Hard resets the controller.
 *
 * Input:		ctlr	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
TW_INT32
tw_cli_hard_reset(struct tw_cli_ctlr_context *ctlr)
{
	struct tw_cli_req_context		*req;
	struct tw_cl_command_reset_firmware	*cmd;
	TW_INT32				error;

	tw_cli_dbg_printf(3, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");

	if ((req = tw_cli_get_request(ctlr
#ifdef TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST
		, TW_CL_NULL
#endif /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */
		)) == TW_CL_NULL)
		return(TW_OSL_EBUSY);

#ifdef TW_OSL_DMA_MEM_ALLOC_PER_REQUEST

	req->cmd_pkt = ctlr->cmd_pkt_buf;
	req->cmd_pkt_phys = ctlr->cmd_pkt_phys;
	tw_osl_memzero(req->cmd_pkt,
		sizeof(struct tw_cl_command_header) +
		28 /* max bytes before sglist */);

#endif /* TW_OSL_DMA_MEM_ALLOC_PER_REQUEST */

	req->flags |= TW_CLI_REQ_FLAGS_INTERNAL;

	/* Build a cmd pkt for sending down the hard reset command. */
	req->cmd_pkt->cmd_hdr.header_desc.size_header = 128;
	
	cmd = &(req->cmd_pkt->command.cmd_pkt_7k.reset_fw);
	cmd->res1__opcode =
		BUILD_RES__OPCODE(0, TWA_FW_CMD_HARD_RESET_FIRMWARE);
	cmd->size = 2;
	cmd->request_id = (TW_UINT8)(TW_CL_SWAP16(req->request_id));
	cmd->unit = 0;
	cmd->status = 0;
	cmd->flags = 0;
	cmd->param = 0;	/* don't reload FPGA logic */

	req->data = TW_CL_NULL;
	req->length = 0;

	error = tw_cli_submit_and_poll_request(req,
		TW_CLI_REQUEST_TIMEOUT_PERIOD);
	if (error) {
		tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
			0x1007, 0x1, TW_CL_SEVERITY_ERROR_STRING,
			"Hard reset request could not be posted",
			"error = %d", error);
		if (error == TW_OSL_ETIMEDOUT)
			/* clean-up done by tw_cli_submit_and_poll_request */
			return(error);
		goto out;
	}
	if ((error = cmd->status)) {
		tw_cli_create_ctlr_event(ctlr,
			TW_CL_MESSAGE_SOURCE_CONTROLLER_ERROR,
			&(req->cmd_pkt->cmd_hdr));
		tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
			0x1008, 0x1, TW_CL_SEVERITY_ERROR_STRING,
			"Hard reset request failed",
			"error = %d", error);
	}

out:
	if (req)
		tw_cli_req_q_insert_tail(req, TW_CLI_FREE_Q);
	return(error);
}

#endif /* TW_OSL_FLASH_FIRMWARE */



/*
 * Function name:	tw_cli_start_ctlr
 * Description:		Establishes a logical connection with the controller.
 *			If bundled with firmware, determines whether or not
 *			to flash firmware, based on arch_id, fw SRL (Spec.
 *			Revision Level), branch & build #'s.  Also determines
 *			whether or not the driver is compatible with the
 *			firmware on the controller, before proceeding to work
 *			with it.
 *
 * Input:		ctlr	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
TW_INT32
tw_cli_start_ctlr(struct tw_cli_ctlr_context *ctlr)
{
	TW_UINT16	fw_on_ctlr_srl = 0;
	TW_UINT16	fw_on_ctlr_arch_id = 0;
	TW_UINT16	fw_on_ctlr_branch = 0;
	TW_UINT16	fw_on_ctlr_build = 0;
	TW_UINT32	init_connect_result = 0;
	TW_INT32	error = TW_OSL_ESUCCESS;
#ifdef TW_OSL_FLASH_FIRMWARE
	TW_INT8		fw_flashed = TW_CL_FALSE;
	TW_INT8		fw_flash_failed = TW_CL_FALSE;
#endif /* TW_OSL_FLASH_FIRMWARE */

	tw_cli_dbg_printf(3, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");

	/* Wait for the controller to become ready. */
	if ((error = tw_cli_poll_status(ctlr,
			TWA_STATUS_MICROCONTROLLER_READY,
			TW_CLI_REQUEST_TIMEOUT_PERIOD))) {
		tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
			0x1009, 0x1, TW_CL_SEVERITY_ERROR_STRING,
			"Microcontroller not ready",
			"error = %d", error);
		return(error);
	}
	/* Drain the response queue. */
	if ((error = tw_cli_drain_response_queue(ctlr))) {
		tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
			0x100A, 0x1, TW_CL_SEVERITY_ERROR_STRING,
			"Can't drain response queue",
			"error = %d", error);
		return(error);
	}
	/* Establish a logical connection with the controller. */
	if ((error = tw_cli_init_connection(ctlr,
			(TW_UINT16)(ctlr->max_simult_reqs),
			TWA_EXTENDED_INIT_CONNECT, TWA_CURRENT_FW_SRL,
			TWA_9000_ARCH_ID, TWA_CURRENT_FW_BRANCH,
			TWA_CURRENT_FW_BUILD, &fw_on_ctlr_srl,
			&fw_on_ctlr_arch_id, &fw_on_ctlr_branch,
			&fw_on_ctlr_build, &init_connect_result))) {
		tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
			0x100B, 0x2, TW_CL_SEVERITY_WARNING_STRING,
			"Can't initialize connection in current mode",
			"error = %d", error);
		return(error);
	}

#ifdef TW_OSL_FLASH_FIRMWARE

	if ((ctlr->flags & TW_CL_FLASH_FIRMWARE) &&
		(init_connect_result & TWA_BUNDLED_FW_SAFE_TO_FLASH) &&
		(init_connect_result & TWA_CTLR_FW_RECOMMENDS_FLASH)) {
		/*
		 * The bundled firmware is safe to flash, and the firmware
		 * on the controller recommends a flash.  So, flash!
		 */
		tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
			0x100C, 0x3, TW_CL_SEVERITY_INFO_STRING,
			"Flashing bundled firmware...",
			" ");
		if ((error = tw_cli_flash_firmware(ctlr))) {
			fw_flash_failed = TW_CL_TRUE;
			tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
				TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
				0x100D, 0x2, TW_CL_SEVERITY_WARNING_STRING,
				"Unable to flash bundled firmware. "
				"Attempting to work with fw on ctlr...",
				" ");
		} else {
			tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
				TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
				0x100E, 0x3, TW_CL_SEVERITY_INFO_STRING,
				"Successfully flashed bundled firmware",
				" ");
			fw_flashed = TW_CL_TRUE;
		}
	}

	if (fw_flashed) {
		/* The firmware was flashed.  Have the new image loaded */
		error = tw_cli_hard_reset(ctlr);
		if (error)
			tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
				TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
				0x100F, 0x1, TW_CL_SEVERITY_ERROR_STRING,
				"Could not reset controller after flash!",
				" ");
		else	/* Go through initialization again. */
			error = tw_cli_start_ctlr(ctlr);
		/*
		 * If hard reset of controller failed, we need to return.
		 * Otherwise, the above recursive call to tw_cli_start_ctlr
		 * will have completed the rest of the initialization (starting
		 * from tw_cli_drain_aen_queue below).  Don't do it again.
		 * Just return.
		 */
		return(error);
	} else
#endif /* TW_OSL_FLASH_FIRMWARE */
	{
		/*
		 * Either we are not bundled with a firmware image, or
		 * the bundled firmware is not safe to flash,
		 * or flash failed for some reason.  See if we can at
		 * least work with the firmware on the controller in the
		 * current mode.
		 */
		if (init_connect_result & TWA_CTLR_FW_COMPATIBLE) {
			/* Yes, we can.  Make note of the operating mode. */
			if (init_connect_result & TWA_CTLR_FW_SAME_OR_NEWER) {
				ctlr->working_srl = TWA_CURRENT_FW_SRL;
				ctlr->working_branch = TWA_CURRENT_FW_BRANCH;
				ctlr->working_build = TWA_CURRENT_FW_BUILD;
			} else {
				ctlr->working_srl = fw_on_ctlr_srl;
				ctlr->working_branch = fw_on_ctlr_branch;
				ctlr->working_build = fw_on_ctlr_build;
			}
		} else {
			/*
			 * No, we can't.  See if we can at least work with
			 * it in the base mode.  We should never come here
			 * if firmware has just been flashed.
			 */
			tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
				TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
				0x1010, 0x2, TW_CL_SEVERITY_WARNING_STRING,
				"Driver/Firmware mismatch. "
				"Negotiating for base level...",
				" ");
			if ((error = tw_cli_init_connection(ctlr,
					(TW_UINT16)(ctlr->max_simult_reqs),
					TWA_EXTENDED_INIT_CONNECT,
					TWA_BASE_FW_SRL, TWA_9000_ARCH_ID,
					TWA_BASE_FW_BRANCH, TWA_BASE_FW_BUILD,
					&fw_on_ctlr_srl, &fw_on_ctlr_arch_id,
					&fw_on_ctlr_branch, &fw_on_ctlr_build,
					&init_connect_result))) {
				tw_cl_create_event(ctlr->ctlr_handle,
					TW_CL_FALSE,
					TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
					0x1011, 0x1,
					TW_CL_SEVERITY_ERROR_STRING,
					"Can't initialize connection in "
					"base mode",
					" ");
				return(error);
			}
			if (!(init_connect_result & TWA_CTLR_FW_COMPATIBLE)) {
				/*
				 * The firmware on the controller is not even
				 * compatible with our base mode.  We cannot
				 * work with it.  Bail...
				 */
#ifdef TW_OSL_FLASH_FIRMWARE
				if (fw_flash_failed)
					tw_cl_create_event(ctlr->ctlr_handle,
					TW_CL_FALSE,
					TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
					0x1012, 0x1,
					TW_CL_SEVERITY_ERROR_STRING,
					"Incompatible firmware on controller"
					"...and could not flash bundled "
					"firmware",
					" ");
				else
					tw_cl_create_event(ctlr->ctlr_handle,
					TW_CL_FALSE,
					TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
					0x1013, 0x1,
					TW_CL_SEVERITY_ERROR_STRING,
					"Incompatible firmware on controller"
					"...and bundled firmware not safe to "
					"flash",
					" ");
#endif /* TW_OSL_FLASH_FIRMWARE */
				return(1);
			}
			/*
			 * We can work with this firmware, but only in
			 * base mode.
			 */
			ctlr->working_srl = TWA_BASE_FW_SRL;
			ctlr->working_branch = TWA_BASE_FW_BRANCH;
			ctlr->working_build = TWA_BASE_FW_BUILD;
			ctlr->operating_mode = TWA_BASE_MODE;
		}
	}

	/* Drain the AEN queue */
	if ((error = tw_cli_drain_aen_queue(ctlr)))
		/* 
		 * We will just print that we couldn't drain the AEN queue.
		 * There's no need to bail out.
		 */
		tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
			0x1014, 0x2, TW_CL_SEVERITY_WARNING_STRING,
			"Can't drain AEN queue",
			"error = %d", error);

	/* Enable interrupts. */
	tw_cli_enable_interrupts(ctlr);

	return(TW_OSL_ESUCCESS);
}


/*
 * Function name:	tw_cl_shutdown_ctlr
 * Description:		Closes logical connection with the controller.
 *
 * Input:		ctlr	-- ptr to per ctlr structure
 *			flags	-- more info passed by the OS Layer
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
TW_INT32
tw_cl_shutdown_ctlr(struct tw_cl_ctlr_handle *ctlr_handle, TW_UINT32 flags)
{
	struct tw_cli_ctlr_context	*ctlr =
		(struct tw_cli_ctlr_context *)(ctlr_handle->cl_ctlr_ctxt);
	TW_INT32			error;

	tw_cli_dbg_printf(3, ctlr_handle, tw_osl_cur_func(), "entered");
	/*
	 * Mark the controller as inactive, disable any further interrupts,
	 * and notify the controller that we are going down.
	 */
	ctlr->state &= ~TW_CLI_CTLR_STATE_ACTIVE;

	tw_cli_disable_interrupts(ctlr);

	/* Let the controller know that we are going down. */
	if ((error = tw_cli_init_connection(ctlr, TWA_SHUTDOWN_MESSAGE_CREDITS,
			0, 0, 0, 0, 0, TW_CL_NULL, TW_CL_NULL, TW_CL_NULL,
			TW_CL_NULL, TW_CL_NULL)))
		tw_cl_create_event(ctlr_handle, TW_CL_FALSE,
			TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
			0x1015, 0x1, TW_CL_SEVERITY_ERROR_STRING,
			"Can't close connection with controller",
			"error = %d", error);

	if (flags & TW_CL_STOP_CTLR_ONLY)
		goto ret;

	/* Destroy all locks used by CL. */
	tw_osl_destroy_lock(ctlr_handle, ctlr->gen_lock);
	tw_osl_destroy_lock(ctlr_handle, ctlr->io_lock);
	if (!(ctlr->flags & TW_CL_64BIT_ADDRESSES))
		tw_osl_destroy_lock(ctlr_handle, ctlr->intr_lock);

ret:
	return(error);
}



/*
 * Function name:	tw_cli_init_connection
 * Description:		Sends init_connection cmd to firmware
 *
 * Input:		ctlr		-- ptr to per ctlr structure
 *			message_credits	-- max # of requests that we might send
 *					 down simultaneously.  This will be
 *					 typically set to 256 at init-time or
 *					after a reset, and to 1 at shutdown-time
 *			set_features	-- indicates if we intend to use 64-bit
 *					sg, also indicates if we want to do a
 *					basic or an extended init_connection;
 *
 * Note: The following input/output parameters are valid, only in case of an
 *		extended init_connection:
 *
 *			current_fw_srl		-- srl of fw we are bundled
 *						with, if any; 0 otherwise
 *			current_fw_arch_id	-- arch_id of fw we are bundled
 *						with, if any; 0 otherwise
 *			current_fw_branch	-- branch # of fw we are bundled
 *						with, if any; 0 otherwise
 *			current_fw_build	-- build # of fw we are bundled
 *						with, if any; 0 otherwise
 * Output:		fw_on_ctlr_srl		-- srl of fw on ctlr
 *			fw_on_ctlr_arch_id	-- arch_id of fw on ctlr
 *			fw_on_ctlr_branch	-- branch # of fw on ctlr
 *			fw_on_ctlr_build	-- build # of fw on ctlr
 *			init_connect_result	-- result bitmap of fw response
 * Return value:	0	-- success
 *			non-zero-- failure
 */
TW_INT32
tw_cli_init_connection(struct tw_cli_ctlr_context *ctlr,
	TW_UINT16 message_credits, TW_UINT32 set_features,
	TW_UINT16 current_fw_srl, TW_UINT16 current_fw_arch_id,
	TW_UINT16 current_fw_branch, TW_UINT16 current_fw_build,
	TW_UINT16 *fw_on_ctlr_srl, TW_UINT16 *fw_on_ctlr_arch_id,
	TW_UINT16 *fw_on_ctlr_branch, TW_UINT16 *fw_on_ctlr_build,
	TW_UINT32 *init_connect_result)
{
	struct tw_cli_req_context		*req;
	struct tw_cl_command_init_connect	*init_connect;
	TW_INT32				error = TW_OSL_EBUSY;
    
	tw_cli_dbg_printf(3, ctlr->ctlr_handle, tw_osl_cur_func(), "entered");

	/* Get a request packet. */
	if ((req = tw_cli_get_request(ctlr
#ifdef TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST
		, TW_CL_NULL
#endif /* TW_OSL_NON_DMA_MEM_ALLOC_PER_REQUEST */
		)) == TW_CL_NULL)
		goto out;

#ifdef TW_OSL_DMA_MEM_ALLOC_PER_REQUEST

	req->cmd_pkt = ctlr->cmd_pkt_buf;
	req->cmd_pkt_phys = ctlr->cmd_pkt_phys;
	tw_osl_memzero(req->cmd_pkt,
		sizeof(struct tw_cl_command_header) +
		28 /* max bytes before sglist */);

#endif /* TW_OSL_DMA_MEM_ALLOC_PER_REQUEST */

	req->flags |= TW_CLI_REQ_FLAGS_INTERNAL;

	/* Build the cmd pkt. */
	init_connect = &(req->cmd_pkt->command.cmd_pkt_7k.init_connect);

	req->cmd_pkt->cmd_hdr.header_desc.size_header = 128;

	init_connect->res1__opcode =
		BUILD_RES__OPCODE(0, TWA_FW_CMD_INIT_CONNECTION);
   	init_connect->request_id =
		(TW_UINT8)(TW_CL_SWAP16(req->request_id));
	init_connect->message_credits = TW_CL_SWAP16(message_credits);
	init_connect->features = TW_CL_SWAP32(set_features);
	if (ctlr->flags & TW_CL_64BIT_ADDRESSES)
		init_connect->features |= TWA_64BIT_SG_ADDRESSES;
	if (set_features & TWA_EXTENDED_INIT_CONNECT) {
		/*
		 * Fill in the extra fields needed for an extended
		 * init_connect.
		 */
		init_connect->size = 6;
		init_connect->fw_srl = TW_CL_SWAP16(current_fw_srl);
		init_connect->fw_arch_id = TW_CL_SWAP16(current_fw_arch_id);
		init_connect->fw_branch = TW_CL_SWAP16(current_fw_branch);
		init_connect->fw_build = TW_CL_SWAP16(current_fw_build);
	} else
		init_connect->size = 3;

	/* Submit the command, and wait for it to complete. */
	error = tw_cli_submit_and_poll_request(req,
		TW_CLI_REQUEST_TIMEOUT_PERIOD);
	if (error == TW_OSL_ETIMEDOUT)
		/* Clean-up done by tw_cli_submit_and_poll_request. */
		return(error);
	if (error)
		goto out;
	if ((error = init_connect->status)) {
		tw_cli_create_ctlr_event(ctlr,
			TW_CL_MESSAGE_SOURCE_CONTROLLER_ERROR,
			&(req->cmd_pkt->cmd_hdr));
		goto out;
	}
	if (set_features & TWA_EXTENDED_INIT_CONNECT) {
		*fw_on_ctlr_srl = TW_CL_SWAP16(init_connect->fw_srl);
		*fw_on_ctlr_arch_id = TW_CL_SWAP16(init_connect->fw_arch_id);
		*fw_on_ctlr_branch = TW_CL_SWAP16(init_connect->fw_branch);
		*fw_on_ctlr_build = TW_CL_SWAP16(init_connect->fw_build);
		*init_connect_result = TW_CL_SWAP32(init_connect->result);
	}
	tw_cli_req_q_insert_tail(req, TW_CLI_FREE_Q);
	return(error);

out:
	tw_cl_create_event(ctlr->ctlr_handle, TW_CL_FALSE,
		TW_CL_MESSAGE_SOURCE_COMMON_LAYER_ERROR,
		0x1016, 0x1, TW_CL_SEVERITY_ERROR_STRING,
		"init_connection failed",
		"error = %d", error);
	if (req)
		tw_cli_req_q_insert_tail(req, TW_CLI_FREE_Q);
	return(error);
}


