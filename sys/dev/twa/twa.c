/*-
 * Copyright (c) 2003-04 3ware, Inc.
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 * 3ware driver for 9000 series storage controllers.
 *
 * Author: Vinod Kashyap
 */


#include <dev/twa/twa_includes.h>

#ifdef TWA_FLASH_FIRMWARE
static int	twa_flash_firmware(struct twa_softc *sc);
static int	twa_hard_reset(struct twa_softc *sc);
#endif /* TWA_FLASH_FIRMWARE */

static int	twa_init_ctlr(struct twa_softc *sc);
static void	*twa_get_param(struct twa_softc *sc, int table_id,
					int parameter_id, size_t size,
					void (* callback)(struct twa_request *tr));
static int	twa_set_param(struct twa_softc *sc, int table_id, int param_id,
					int param_size, void *data,
					void (* callback)(struct twa_request *tr));
static int	twa_init_connection(struct twa_softc *sc, u_int16_t message_credits,
				u_int32_t set_features, u_int16_t current_fw_srl,
				u_int16_t current_fw_arch_id, u_int16_t current_fw_branch,
				u_int16_t current_fw_build, u_int16_t *fw_on_ctlr_srl,
				u_int16_t *fw_on_ctlr_arch_id, u_int16_t *fw_on_ctlr_branch,
				u_int16_t *fw_on_ctlr_build, u_int32_t *init_connect_result);

static int	twa_wait_request(struct twa_request *req, u_int32_t timeout);
static int	twa_immediate_request(struct twa_request *req, u_int32_t timeout);

static int	twa_done(struct twa_softc *sc);
static int	twa_drain_pending_queue(struct twa_softc *sc);
static void	twa_drain_complete_queue(struct twa_softc *sc);
static int	twa_wait_status(struct twa_softc *sc, u_int32_t status, u_int32_t timeout);
static int	twa_drain_response_queue(struct twa_softc *sc);
static int	twa_check_ctlr_state(struct twa_softc *sc, u_int32_t status_reg);
static int	twa_soft_reset(struct twa_softc *sc);

static void	twa_host_intr(struct twa_softc *sc);
static void	twa_attention_intr(struct twa_softc *sc);
static void	twa_command_intr(struct twa_softc *sc);

static int	twa_fetch_aen(struct twa_softc *sc);
static void	twa_aen_callback(struct twa_request *tr);
static unsigned short	twa_enqueue_aen(struct twa_softc *sc,
				struct twa_command_header *cmd_hdr);
static int	twa_drain_aen_queue(struct twa_softc *sc);
static int	twa_find_aen(struct twa_softc *sc, u_int16_t aen_code);

static void	twa_panic(struct twa_softc *sc, int8_t *reason);

/*
 * Function name:	twa_setup
 * Description:		Initializes driver data structures for the controller.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
int
twa_setup(struct twa_softc *sc)
{
	struct twa_event_packet	*aen_queue;
	int			error = 0;
	int			i;

	twa_dbg_dprint_enter(3, sc);

	/* Initialize request queues. */
	twa_initq_free(sc);
	twa_initq_busy(sc);
	twa_initq_pending(sc);
	twa_initq_complete(sc);

	if (twa_alloc_req_pkts(sc, TWA_Q_LENGTH)) {
		twa_printf(sc, "Failed to allocate request packets.\n");
		return(ENOMEM);
	}

	/* Allocate memory for the AEN queue. */
	if ((aen_queue = malloc(sizeof(struct twa_event_packet) * TWA_Q_LENGTH,
					M_DEVBUF, M_WAITOK)) == NULL) {
		/* 
		 * This should not cause us to return error.  We will only be
		 * unable to support AEN's.  But then, we will have to check
		 * time and again to see if we can support AEN's, if we
		 * continue.  So, we will just return error.
		 */
		twa_printf(sc, "Could not allocate memory for AEN queue.\n");
		return(ENOMEM); /* any unfreed memory will be freed by twa_free */
	}
	/* Initialize the aen queue. */
	bzero(aen_queue, sizeof(struct twa_event_packet) * TWA_Q_LENGTH);
	for (i = 0; i < TWA_Q_LENGTH; i++)
		sc->twa_aen_queue[i] = &(aen_queue[i]);

	/* Disable interrupts. */
	twa_disable_interrupts(sc);

	/* Initialize the controller. */
	if ((error = twa_init_ctlr(sc))) {
		/* Soft reset the controller, and try one more time. */
		twa_printf(sc, "Controller initialization failed. Retrying...\n");
		if ((error = twa_soft_reset(sc)))
			twa_printf(sc, "Controller soft reset failed.\n");
		else
			error = twa_init_ctlr(sc);
	}
	return(error);
}

#ifdef TWA_FLASH_FIRMWARE
/*
 * Function name:	twa_flash_firmware
 * Description:		Flashes bundled firmware image onto controller.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_flash_firmware(struct twa_softc *sc)
{
	struct twa_request			*tr;
	struct twa_command_header		*cmd_hdr;
	struct twa_command_download_firmware	*cmd;
	u_int32_t				fw_img_chunk_size;
	u_int32_t				this_chunk_size = 0;
	u_int32_t				remaining_img_size = 0;
	u_int8_t				*error_str;
	int					error;
	int					i;

	if ((tr = twa_get_request(sc)) == NULL) {
		/* No free request packets available.  Can't proceed. */
		error = EIO;
		goto out;
	}
	tr->tr_cmd_pkt_type |= TWA_CMD_PKT_TYPE_INTERNAL;
	/* Allocate sufficient memory to hold a chunk of the firmware image. */
	fw_img_chunk_size = ((twa_fw_img_size/NUM_FW_IMAGE_CHUNKS) + 511) & ~511;
	if ((tr->tr_data = malloc(fw_img_chunk_size, M_DEVBUF, M_WAITOK)) == NULL) {
		twa_printf (sc, "Could not allocate memory for firmware image.\n"); 
		error = ENOMEM;
		goto out;
	}
	remaining_img_size = twa_fw_img_size;
	cmd_hdr = &(tr->tr_command->cmd_hdr);
	cmd = &(tr->tr_command->command.cmd_pkt_7k.download_fw);

	for (i = 0; i < NUM_FW_IMAGE_CHUNKS; i++) {
		/* Build a cmd pkt for downloading firmware. */
		bzero(tr->tr_command, sizeof(struct twa_command_packet));

		cmd_hdr->header_desc.size_header = 128;
	
		cmd->opcode = TWA_OP_DOWNLOAD_FIRMWARE;
		cmd->sgl_offset = 2;/* offset in dwords, to the beginning of sg list */
		cmd->size = 2;	/* this field will be updated at data map time */
		cmd->request_id = tr->tr_request_id;
		cmd->unit = 0;
		cmd->status = 0;
		cmd->flags = 0;
		cmd->param = 8;	/* prom image */

		if (i != (NUM_FW_IMAGE_CHUNKS - 1))
			this_chunk_size = fw_img_chunk_size;
		else	 /* last chunk */
			this_chunk_size = remaining_img_size;
	
		remaining_img_size -= this_chunk_size;
		bcopy(twa_fw_img + (i * fw_img_chunk_size),
					tr->tr_data, this_chunk_size);

		/*
		 * The next line will effect only the last chunk.
		 */
		tr->tr_length = (this_chunk_size + 511) & ~511;

		tr->tr_flags |= TWA_CMD_DATA_OUT;

		error = twa_immediate_request(tr, TWA_REQUEST_TIMEOUT_PERIOD);
		if (error) {
			twa_printf(sc, "Firmware flash request could not be posted. error = 0x%x\n",
									error);
			if (error == ETIMEDOUT)
				return(error); /* clean-up done by twa_immediate_request */
			break;
		}
		error = cmd->status;
		if (i != (NUM_FW_IMAGE_CHUNKS - 1)) {
			if ((error = cmd_hdr->status_block.error) != TWA_ERROR_MORE_DATA) {
				error_str = 
				&(cmd_hdr->err_desc[strlen(cmd_hdr->err_desc) + 1]);
				if (error_str[0] == '\0')
					error_str = twa_find_msg_string(twa_error_table, error);

				twa_printf(sc, "cmd = 0x%x: ERROR: (0x%02X: 0x%04X): %s: %s\n",
					cmd->opcode,
					TWA_MESSAGE_SOURCE_CONTROLLER_ERROR,
					error,
					error_str,
					cmd_hdr->err_desc);
				twa_printf(sc, "Firmware flash request failed. Intermediate error = 0x%x, i = %x\n",
							cmd->status, i);
				/* Hard reset the controller, so that it doesn't wait for the remaining chunks. */
				twa_hard_reset(sc);
				break;
			}
		} else	 /* last chunk */
			if (error) {
				error_str =
				&(cmd_hdr->err_desc[strlen(cmd_hdr->err_desc) + 1]);
				if (error_str[0] == '\0')
					error_str = twa_find_msg_string(twa_error_table,
						cmd_hdr->status_block.error);

				twa_printf(sc, "cmd = 0x%x: ERROR: (0x%02X: 0x%04X): %s: %s\n",
					cmd->opcode,
					TWA_MESSAGE_SOURCE_CONTROLLER_ERROR,
					cmd_hdr->status_block.error,
					error_str,
					cmd_hdr->err_desc);
				twa_printf(sc, "Firmware flash request failed. error = 0x%x\n", error);
				/* Hard reset the controller, so that it doesn't wait for more chunks. */
				twa_hard_reset(sc);
			}
	} /* for */

	if (tr->tr_data)
		free(tr->tr_data, M_DEVBUF);
out:
	if (tr)
		twa_release_request(tr);
	return(error);
}


/*
 * Function name:	twa_hard_reset
 * Description:		Hard reset the controller.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_hard_reset(struct twa_softc *sc)
{
	struct twa_request			*tr;
	struct twa_command_header		*cmd_hdr;
	struct twa_command_reset_firmware	*cmd;
	int					error;

	if ((tr = twa_get_request(sc)) == NULL)
		return(EIO);
	tr->tr_cmd_pkt_type |= TWA_CMD_PKT_TYPE_INTERNAL;
	/* Build a cmd pkt for sending down the hard reset command. */
	cmd_hdr = &(tr->tr_command->cmd_hdr);
	cmd_hdr->header_desc.size_header = 128;
	
	cmd = &(tr->tr_command->command.cmd_pkt_7k.reset_fw);
	cmd->opcode = TWA_OP_RESET_FIRMWARE;
	cmd->size = 2;	/* this field will be updated at data map time */
	cmd->request_id = tr->tr_request_id;
	cmd->unit = 0;
	cmd->status = 0;
	cmd->flags = 0;
	cmd->param = 0;	/* don't reload FPGA logic */

	tr->tr_data = NULL;
	tr->tr_length = 0;

	error = twa_immediate_request(tr, TWA_REQUEST_TIMEOUT_PERIOD);
	if (error) {
		twa_printf(sc, "Hard reset request could not be posted. error = 0x%x\n",
								error);
		if (error == ETIMEDOUT)
			return(error); /* clean-up done by twa_immediate_request */
		goto out;
	}
	if ((error = cmd->status)) {
		u_int8_t *error_str =
		&(cmd_hdr->err_desc[strlen(cmd_hdr->err_desc) + 1]);

		if (error_str[0] == '\0')
			error_str = twa_find_msg_string(twa_error_table,
					cmd_hdr->status_block.error);

		twa_printf(sc, "cmd = 0x%x: ERROR: (0x%02X: 0x%04X): %s: %s\n",
					cmd->opcode,
					TWA_MESSAGE_SOURCE_CONTROLLER_ERROR,
					cmd_hdr->status_block.error,
					error_str,
					cmd_hdr->err_desc);
		twa_printf(sc, "Hard reset request failed. error = 0x%x\n", error);
	}

out:
	if (tr)
		twa_release_request(tr);
	return(error);
}

#endif /* TWA_FLASH_FIRMWARE */

/*
 * Function name:	twa_init_ctlr
 * Description:		Establishes a logical connection with the controller.
 *			If bundled with firmware, determines whether or not
 *			to flash firmware, based on arch_id, fw SRL (Spec.
 *			Revision Level), branch & build #'s.  Also determines
 *			whether or not the driver is compatible with the
 *			firmware on the controller, before proceeding to work
 *			with it.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_init_ctlr(struct twa_softc *sc)
{
	u_int16_t	fw_on_ctlr_srl = 0;
	u_int16_t	fw_on_ctlr_arch_id = 0;
	u_int16_t	fw_on_ctlr_branch = 0;
	u_int16_t	fw_on_ctlr_build = 0;
	u_int32_t	init_connect_result = 0;
	int		error = 0;
#ifdef TWA_FLASH_FIRMWARE
	int8_t		fw_flashed = FALSE;
	int8_t		fw_flash_failed = FALSE;
#endif /* TWA_FLASH_FIRMWARE */

	twa_dbg_dprint_enter(3, sc);

	/* Wait for the controller to become ready. */
	if (twa_wait_status(sc, TWA_STATUS_MICROCONTROLLER_READY,
					TWA_REQUEST_TIMEOUT_PERIOD)) {
		twa_printf(sc, "Microcontroller not ready.\n");
		return(ENXIO);
	}
	/* Drain the response queue. */
	if (twa_drain_response_queue(sc)) {
		twa_printf(sc, "Can't drain response queue.\n");
		return(1);
	}
	/* Establish a logical connection with the controller. */
	if ((error = twa_init_connection(sc, TWA_INIT_MESSAGE_CREDITS,
			TWA_EXTENDED_INIT_CONNECT, TWA_CURRENT_FW_SRL,
			TWA_9000_ARCH_ID, TWA_CURRENT_FW_BRANCH,
			TWA_CURRENT_FW_BUILD, &fw_on_ctlr_srl,
			&fw_on_ctlr_arch_id, &fw_on_ctlr_branch,
			&fw_on_ctlr_build, &init_connect_result))) {
		twa_printf(sc, "Can't initialize connection in current mode.\n");
		return(error);
	}

#ifdef TWA_FLASH_FIRMWARE

	if ((init_connect_result & TWA_BUNDLED_FW_SAFE_TO_FLASH) &&
		(init_connect_result & TWA_CTLR_FW_RECOMMENDS_FLASH)) {
		/*
		 * The bundled firmware is safe to flash, and the firmware
		 * on the controller recommends a flash.  So, flash!
		 */
		twa_printf(sc, "Flashing bundled firmware...\n");
		if ((error = twa_flash_firmware(sc))) {
			fw_flash_failed = TRUE;
			twa_printf(sc, "Unable to flash bundled firmware.\n");
			twa_printf(sc, "Will see if possible to work with firmware on controller...\n");
		} else {
			twa_printf(sc, "Successfully flashed bundled firmware.\n");
			fw_flashed = TRUE;
		}
	}

	if (fw_flashed) {
		/* The firmware was flashed.  Have the new image loaded */
		error = twa_hard_reset(sc);
		if (error)
			twa_printf(sc, "Could not reset controller after flash!\n");
		else	/* Go through initialization again. */
			error = twa_init_ctlr(sc);
		/*
		 * If hard reset of controller failed, we need to return.
		 * Otherwise, the above recursive call to twa_init_ctlr will
		 * have completed the rest of the initialization (starting
		 * from twa_drain_aen_queue below).  Don't do it again.
		 * Just return.
		 */
		return(error);
	} else
#endif /* TWA_FLASH_FIRMWARE */
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
			sc->working_srl = TWA_CURRENT_FW_SRL;
			sc->working_branch = TWA_CURRENT_FW_BRANCH;
			sc->working_build = TWA_CURRENT_FW_BUILD;
		} else {
			/*
			 * No, we can't.  See if we can at least work with
			 * it in the base mode.  We should never come here
			 * if firmware has just been flashed.
			 */
			twa_printf(sc, "Driver/Firmware mismatch.  Negotiating for base level...\n");
			if ((error = twa_init_connection(sc, TWA_INIT_MESSAGE_CREDITS,
					TWA_EXTENDED_INIT_CONNECT, TWA_BASE_FW_SRL,
					TWA_9000_ARCH_ID, TWA_BASE_FW_BRANCH,
					TWA_BASE_FW_BUILD, &fw_on_ctlr_srl,
					&fw_on_ctlr_arch_id, &fw_on_ctlr_branch,
					&fw_on_ctlr_build, &init_connect_result))) {
				twa_printf(sc, "Can't initialize connection in base mode.\n");
				return(error);
			}
			if (!(init_connect_result & TWA_CTLR_FW_COMPATIBLE)) {
				/*
				 * The firmware on the controller is not even
				 * compatible with our base mode.  We cannot
				 * work with it.  Bail...
				 */
				twa_printf(sc, "Incompatible firmware on controller\n");
#ifdef TWA_FLASH_FIRMWARE
				if (fw_flash_failed)
					twa_printf(sc, "...and could not flash bundled firmware.\n");
				else
					twa_printf(sc, "...and bundled firmware not safe to flash.\n");
#endif /* TWA_FLASH_FIRMWARE */
				return(1);
			}
			/* We can work with this firmware, but only in base mode. */
			sc->working_srl = TWA_BASE_FW_SRL;
			sc->working_branch = TWA_BASE_FW_BRANCH;
			sc->working_build = TWA_BASE_FW_BUILD;
			sc->twa_operating_mode = TWA_BASE_MODE;
		}
	}

	/* Drain the AEN queue */
	if (twa_drain_aen_queue(sc)) {
		/* 
		 * We will just print that we couldn't drain the AEN queue.
		 * There's no need to bail out.
		 */
		twa_printf(sc, "Can't drain AEN queue.\n");
	}

	/* Set controller state to initialized. */
	sc->twa_state &= ~TWA_STATE_SHUTDOWN;

	twa_enable_interrupts(sc);
	twa_dbg_dprint_exit(3, sc);
	return(0);
}


/*
 * Function name:	twa_deinit_ctlr
 * Description:		Close logical connection with the controller.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
int
twa_deinit_ctlr(struct twa_softc *sc)
{
	/*
	 * Mark the controller as shutting down,
	 * and disable any further interrupts.
	 */
	sc->twa_state |= TWA_STATE_SHUTDOWN;
	twa_disable_interrupts(sc);

	/* Let the controller know that we are going down. */
	return(twa_init_connection(sc, TWA_SHUTDOWN_MESSAGE_CREDITS,
					0, 0, 0, 0, 0,
					NULL, NULL, NULL, NULL, NULL));
}


/*
 * Function name:	twa_interrupt
 * Description:		Interrupt handler.  Determines the kind of interrupt,
 *			and calls the appropriate handler.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
void
twa_interrupt(struct twa_softc *sc)
{
	u_int32_t	status_reg;
	int		s;

	s = splcam();
	twa_dbg_dprint_enter(5, sc);

	/* Collect current interrupt status. */
	status_reg = TWA_READ_STATUS_REGISTER(sc);
	if (twa_check_ctlr_state(sc, status_reg))
		return;

	/* Dispatch based on the kind of interrupt. */
	if (status_reg & TWA_STATUS_HOST_INTERRUPT)
		twa_host_intr(sc);
	if (status_reg & TWA_STATUS_ATTENTION_INTERRUPT)
		twa_attention_intr(sc);
	if (status_reg & TWA_STATUS_COMMAND_INTERRUPT)
		twa_command_intr(sc);
	if (status_reg & TWA_STATUS_RESPONSE_INTERRUPT)
		twa_done(sc);
	splx(s);
}


/*
 * Function name:	twa_ioctl
 * Description:		ioctl handler.
 *
 * Input:		sc	-- ptr to per ctlr structure
 *			cmd	-- ioctl cmd
 *			buf	-- ptr to buffer in kernel memory, which is
 *				   a copy of the input buffer in user-space
 * Output:		buf	-- ptr to buffer in kernel memory, which will
 *				   be copied of the output buffer in user-space
 * Return value:	0	-- success
 *			non-zero-- failure
 */
int
twa_ioctl(struct twa_softc *sc, int cmd, void *buf)
{
	struct twa_ioctl_9k	*user_buf = (struct twa_ioctl_9k *)buf;
	struct twa_event_packet	event_buf;
	int32_t			event_index;
	int32_t			start_index;
	int			s;
	int			error = 0;
		
	switch (cmd) {
	case TWA_IOCTL_FIRMWARE_PASS_THROUGH:
	{
		struct twa_command_packet	*cmdpkt;
		struct twa_request 		*tr;
		u_int32_t			data_buf_size_adjusted;


		twa_dbg_dprint(2, sc, "Firmware PassThru");

		/* Get a request packet */
		while ((tr = twa_get_request(sc)) == NULL)
			/*
			 * No free request packets available.  Sleep until
			 * one becomes available.
			 */
			tsleep(&(sc->twa_wait_timeout), PPAUSE, "twioctl", hz);

		/*
		 * Make sure that the data buffer sent to firmware is a 
		 * 512 byte multiple in size.
		 */
  		data_buf_size_adjusted = (user_buf->twa_drvr_pkt.buffer_length + 511) & ~511;
		if ((tr->tr_length = data_buf_size_adjusted)) {
			if ((tr->tr_data = malloc(data_buf_size_adjusted, M_DEVBUF, M_WAITOK)) == NULL) {
				twa_printf(sc, "Could not alloc mem for fw_passthru data_buf.\n");
				error = ENOMEM;
				goto fw_passthru_done;
			}
			/* Copy the payload. */
			if ((error = copyin((void *) (user_buf->pdata), 
					(void *) (tr->tr_data),
					user_buf->twa_drvr_pkt.buffer_length)) != 0) {
				twa_printf (sc, "Could not copyin fw_passthru data_buf.\n"); 
				goto fw_passthru_done;
			}
			tr->tr_flags |= TWA_CMD_DATA_IN | TWA_CMD_DATA_OUT;
		}
		tr->tr_cmd_pkt_type |= TWA_CMD_PKT_TYPE_IOCTL;
		cmdpkt = tr->tr_command;

		/* Copy the command packet. */
		bcopy(&(user_buf->twa_cmd_pkt), cmdpkt,
					sizeof(struct twa_command_packet));
		cmdpkt->command.cmd_pkt_7k.generic.request_id = tr->tr_request_id;

		twa_dbg_dprint(3, sc, "cmd_pkt_7k = %x %x %x %x %x %x %x",
					cmdpkt->command.cmd_pkt_7k.generic.opcode,
					cmdpkt->command.cmd_pkt_7k.generic.sgl_offset,
					cmdpkt->command.cmd_pkt_7k.generic.size,
					cmdpkt->command.cmd_pkt_7k.generic.request_id,
					cmdpkt->command.cmd_pkt_7k.generic.unit,
					cmdpkt->command.cmd_pkt_7k.generic.status,
					cmdpkt->command.cmd_pkt_7k.generic.flags);

		/* Send down the request, and wait for it to complete. */
		if ((error = twa_wait_request(tr, TWA_REQUEST_TIMEOUT_PERIOD))) {
			twa_printf(sc, "fw_passthru request failed. error = 0x%x\n", error);
			if (error == ETIMEDOUT)
				break; /* clean-up done by twa_wait_request */
			goto fw_passthru_done;
		}

		/* Copy the command packet back into user space. */
		bcopy(cmdpkt, &(user_buf->twa_cmd_pkt),
					sizeof(struct twa_command_packet));
	
		/* If there was a payload, copy it back too. */
		if (tr->tr_length)
			error = copyout(tr->tr_data, user_buf->pdata,
					user_buf->twa_drvr_pkt.buffer_length);

fw_passthru_done:
		/* Free resources. */
		if (tr->tr_data)
			free(tr->tr_data, M_DEVBUF);
		if (tr)
			twa_release_request(tr);
		break;
	}


	case TWA_IOCTL_SCAN_BUS:
		/* Request CAM for a bus scan. */
		twa_request_bus_scan(sc);
		break;


	case TWA_IOCTL_GET_FIRST_EVENT:
		twa_dbg_dprint(3, sc, "Get First Event");

		if (sc->twa_aen_queue_wrapped) {
			if (sc->twa_aen_queue_overflow) {
				/*
				 * The aen queue has wrapped, even before some
				 * events have been retrieved.  Let the caller
				 * know that he missed out on some AEN's.
				 */
				user_buf->twa_drvr_pkt.status = TWA_ERROR_AEN_OVERFLOW;
				sc->twa_aen_queue_overflow = FALSE;
			} else
				user_buf->twa_drvr_pkt.status = 0;
			event_index = sc->twa_aen_head;
		} else {
			if (sc->twa_aen_head == sc->twa_aen_tail) {
				user_buf->twa_drvr_pkt.status = TWA_ERROR_AEN_NO_EVENTS;
				break;
			}
			user_buf->twa_drvr_pkt.status = 0;
			event_index = sc->twa_aen_tail;	/* = 0 */
		}
		if ((error = copyout(sc->twa_aen_queue[event_index], user_buf->pdata,
					sizeof(struct twa_event_packet))) != 0)
			twa_printf(sc, "get_first: Could not copyout to event_buf. error = %x\n", error);
		(sc->twa_aen_queue[event_index])->retrieved = TWA_AEN_RETRIEVED;
		break;


	case TWA_IOCTL_GET_LAST_EVENT:
		twa_dbg_dprint(3, sc, "Get Last Event");

		if (sc->twa_aen_queue_wrapped) {
			if (sc->twa_aen_queue_overflow) {
				/*
				 * The aen queue has wrapped, even before some
				 * events have been retrieved.  Let the caller
				 * know that he missed out on some AEN's.
				 */
				user_buf->twa_drvr_pkt.status = TWA_ERROR_AEN_OVERFLOW;
				sc->twa_aen_queue_overflow = FALSE;
			} else
				user_buf->twa_drvr_pkt.status = 0;
		} else {
			if (sc->twa_aen_head == sc->twa_aen_tail) {
				user_buf->twa_drvr_pkt.status = TWA_ERROR_AEN_NO_EVENTS;
				break;
			}
			user_buf->twa_drvr_pkt.status = 0;
		}
		event_index = (sc->twa_aen_head - 1 + TWA_Q_LENGTH) % TWA_Q_LENGTH;
		if ((error = copyout(sc->twa_aen_queue[event_index], user_buf->pdata,
					sizeof(struct twa_event_packet))) != 0)
			twa_printf(sc, "get_last: Could not copyout to event_buf. error = %x\n", error);
		(sc->twa_aen_queue[event_index])->retrieved = TWA_AEN_RETRIEVED;
		break;


	case TWA_IOCTL_GET_NEXT_EVENT:
		twa_dbg_dprint(3, sc, "Get Next Event");

		user_buf->twa_drvr_pkt.status = 0;
		if (sc->twa_aen_queue_wrapped) {
			twa_dbg_dprint(3, sc, "Get Next Event: wrapped");
			if (sc->twa_aen_queue_overflow) {
				/*
				 * The aen queue has wrapped, even before some
				 * events have been retrieved.  Let the caller
				 * know that he missed out on some AEN's.
				 */
				twa_dbg_dprint(2, sc, "Get Next Event: overflow");
				user_buf->twa_drvr_pkt.status = TWA_ERROR_AEN_OVERFLOW;
				sc->twa_aen_queue_overflow = FALSE;
			}
			start_index = sc->twa_aen_head;
		} else {
			if (sc->twa_aen_head == sc->twa_aen_tail) {
				twa_dbg_dprint(3, sc, "Get Next Event: empty queue");
				user_buf->twa_drvr_pkt.status = TWA_ERROR_AEN_NO_EVENTS;
				break;
			}
			start_index = sc->twa_aen_tail;	/* = 0 */
		}
		if ((error = copyin(user_buf->pdata, &event_buf,
				sizeof(struct twa_event_packet))) != 0)
			twa_printf(sc, "get_next: Could not copyin event_buf.\n");

		event_index = (start_index + event_buf.sequence_id -
				(sc->twa_aen_queue[start_index])->sequence_id + 1)
				% TWA_Q_LENGTH;

		twa_dbg_dprint(3, sc, "Get Next Event: si = %x, ei = %x, ebsi = %x, sisi = %x, eisi = %x",
				start_index, event_index, event_buf.sequence_id,
				(sc->twa_aen_queue[start_index])->sequence_id,
				(sc->twa_aen_queue[event_index])->sequence_id);

		if (! ((sc->twa_aen_queue[event_index])->sequence_id >
						event_buf.sequence_id)) {
			if (user_buf->twa_drvr_pkt.status == TWA_ERROR_AEN_OVERFLOW)
				sc->twa_aen_queue_overflow = TRUE; /* so we report the overflow next time */
			user_buf->twa_drvr_pkt.status = TWA_ERROR_AEN_NO_EVENTS;
			break;
		}
		if ((error = copyout(sc->twa_aen_queue[event_index], user_buf->pdata, 
					sizeof(struct twa_event_packet))) != 0)
			twa_printf(sc, "get_next: Could not copyout to event_buf. error = %x\n", error);

		(sc->twa_aen_queue[event_index])->retrieved = TWA_AEN_RETRIEVED;
		break;


	case TWA_IOCTL_GET_PREVIOUS_EVENT:
		twa_dbg_dprint(3, sc, "Get Previous Event");

		user_buf->twa_drvr_pkt.status = 0;
		if (sc->twa_aen_queue_wrapped) {
			if (sc->twa_aen_queue_overflow) {
				/*
				 * The aen queue has wrapped, even before some
				 * events have been retrieved.  Let the caller
				 * know that he missed out on some AEN's.
				 */
				user_buf->twa_drvr_pkt.status = TWA_ERROR_AEN_OVERFLOW;
				sc->twa_aen_queue_overflow = FALSE;
			}
			start_index = sc->twa_aen_head;
		} else {
			if (sc->twa_aen_head == sc->twa_aen_tail) {
				user_buf->twa_drvr_pkt.status = TWA_ERROR_AEN_NO_EVENTS;
				break;
			}
			start_index = sc->twa_aen_tail;	/* = 0 */
		}
		if ((error = copyin(user_buf->pdata, &event_buf,
				sizeof(struct twa_event_packet))) != 0)
			twa_printf(sc, "get_previous: Could not copyin event_buf.\n");

		event_index = (start_index + event_buf.sequence_id -
			(sc->twa_aen_queue[start_index])->sequence_id - 1) % TWA_Q_LENGTH;
		if (! ((sc->twa_aen_queue[event_index])->sequence_id < event_buf.sequence_id)) {
			if (user_buf->twa_drvr_pkt.status == TWA_ERROR_AEN_OVERFLOW)
				sc->twa_aen_queue_overflow = TRUE; /* so we report the overflow next time */
			user_buf->twa_drvr_pkt.status = TWA_ERROR_AEN_NO_EVENTS;
			break;
		}
		if ((error = copyout(sc->twa_aen_queue [event_index], user_buf->pdata,
					sizeof(struct twa_event_packet))) != 0)
			twa_printf(sc, "get_previous: Could not copyout to event_buf. error = %x\n", error);

		(sc->twa_aen_queue[event_index])->retrieved = TWA_AEN_RETRIEVED;
		break;


	case TWA_IOCTL_GET_LOCK:
	{
		struct twa_lock_packet	twa_lock;
		u_int32_t		cur_time;

		cur_time = time_second - (tz_minuteswest * 60) - 
					(wall_cmos_clock ? adjkerntz : 0);
		copyin(user_buf->pdata, &twa_lock,
				sizeof(struct twa_lock_packet));
		s = splcam();
		if ((sc->twa_ioctl_lock.lock == TWA_LOCK_FREE) ||
				(twa_lock.force_flag) ||
				(cur_time >= sc->twa_ioctl_lock.timeout)) {
			twa_dbg_dprint(3, sc, "GET_LOCK: Getting lock!");
			sc->twa_ioctl_lock.lock = TWA_LOCK_HELD;
			sc->twa_ioctl_lock.timeout = cur_time + (twa_lock.timeout_msec / 1000);
			twa_lock.time_remaining_msec = twa_lock.timeout_msec;
			user_buf->twa_drvr_pkt.status = 0;
		} else {
			twa_dbg_dprint(2, sc, "GET_LOCK: Lock already held!");
			twa_lock.time_remaining_msec =
				(sc->twa_ioctl_lock.timeout - cur_time) * 1000;
			user_buf->twa_drvr_pkt.status =
					TWA_ERROR_IOCTL_LOCK_ALREADY_HELD;
		}
		splx(s);
		copyout(&twa_lock, user_buf->pdata,
				sizeof(struct twa_lock_packet));
		break;
	}


	case TWA_IOCTL_RELEASE_LOCK:
		s = splcam();
		if (sc->twa_ioctl_lock.lock == TWA_LOCK_FREE) {
			twa_dbg_dprint(2, sc, "twa_ioctl: RELEASE_LOCK: Lock not held!");
			user_buf->twa_drvr_pkt.status = TWA_ERROR_IOCTL_LOCK_NOT_HELD;
		} else {
			twa_dbg_dprint(3, sc, "RELEASE_LOCK: Releasing lock!");
			sc->twa_ioctl_lock.lock = TWA_LOCK_FREE;
			user_buf->twa_drvr_pkt.status = 0;
		}
		splx(s);
		break;


	case TWA_IOCTL_GET_COMPATIBILITY_INFO:
	{
		struct twa_compatibility_packet	comp_pkt;

		bcopy(TWA_DRIVER_VERSION_STRING, comp_pkt.driver_version,
					sizeof(TWA_DRIVER_VERSION_STRING));
		comp_pkt.working_srl = sc->working_srl;
		comp_pkt.working_branch = sc->working_branch;
		comp_pkt.working_build = sc->working_build;
		user_buf->twa_drvr_pkt.status = 0;

		/* Copy compatibility information to user space. */
		copyout(&comp_pkt, user_buf->pdata,
				min(sizeof(struct twa_compatibility_packet),
					user_buf->twa_drvr_pkt.buffer_length));
		break;
	}

	default:	
		/* Unknown opcode. */
		error = ENOTTY;
	}

	return(error);
}


/*
 * Function name:	twa_enable_interrupts
 * Description:		Enables interrupts on the controller
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
void
twa_enable_interrupts(struct twa_softc *sc)
{
	sc->twa_state |= TWA_STATE_INTR_ENABLED;
	TWA_WRITE_CONTROL_REGISTER(sc,
		TWA_CONTROL_CLEAR_ATTENTION_INTERRUPT |
		TWA_CONTROL_UNMASK_RESPONSE_INTERRUPT |
		TWA_CONTROL_ENABLE_INTERRUPTS);
}


/*
 * Function name:	twa_setup
 * Description:		Disables interrupts on the controller
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
void
twa_disable_interrupts(struct twa_softc *sc)
{
	TWA_WRITE_CONTROL_REGISTER(sc, TWA_CONTROL_DISABLE_INTERRUPTS);
	sc->twa_state &= ~TWA_STATE_INTR_ENABLED;
}



/*
 * Function name:	twa_get_param
 * Description:		Get a firmware parameter.
 *
 * Input:		sc		-- ptr to per ctlr structure
 *			table_id	-- parameter table #
 *			param_id	-- index of the parameter in the table
 *			param_size	-- size of the parameter in bytes
 *			callback	-- ptr to function, if any, to be called
 *					back on completion; NULL if no callback.
 * Output:		None
 * Return value:	ptr to param structure	-- success
 *			NULL			-- failure
 */
static void *
twa_get_param(struct twa_softc *sc, int table_id, int param_id,
		size_t param_size, void (* callback)(struct twa_request *tr))
{
	struct twa_request		*tr;
	struct twa_command_header	*cmd_hdr;
	union twa_command_7k		*cmd;
	struct twa_param_9k		*param = NULL;
	int				error = ENOMEM;

	twa_dbg_dprint_enter(4, sc);

	/* Get a request packet. */
	if ((tr = twa_get_request(sc)) == NULL)
		goto out;
	tr->tr_cmd_pkt_type |= TWA_CMD_PKT_TYPE_INTERNAL;

	/* Allocate memory to read data into. */
	if ((param = (struct twa_param_9k *)
			malloc(TWA_SECTOR_SIZE, M_DEVBUF, M_NOWAIT)) == NULL)
		goto out;
	bzero(param, sizeof(struct twa_param_9k) - 1 + param_size);
	tr->tr_data = param;
	tr->tr_length = TWA_SECTOR_SIZE;
	tr->tr_flags = TWA_CMD_DATA_IN | TWA_CMD_DATA_OUT;

	/* Build the cmd pkt. */
	cmd_hdr = &(tr->tr_command->cmd_hdr);
	cmd_hdr->header_desc.size_header = 128;
	
	cmd = &(tr->tr_command->command.cmd_pkt_7k);
	cmd->param.opcode = TWA_OP_GET_PARAM;
	cmd->param.sgl_offset = 2;
	cmd->param.size = 2;
	cmd->param.request_id = tr->tr_request_id;
	cmd->param.unit = 0;
	cmd->param.param_count = 1;

	/* Specify which parameter we need. */
	param->table_id = table_id | TWA_9K_PARAM_DESCRIPTOR;
	param->parameter_id = param_id;
	param->parameter_size_bytes = param_size;

	/* Submit the command. */
	if (callback == NULL) {
		/* There's no call back; wait till the command completes. */
		error = twa_immediate_request(tr, TWA_REQUEST_TIMEOUT_PERIOD);
		if (error == ETIMEDOUT)
			return(NULL); /* clean-up done by twa_immediate_request */
		if (error)
			goto out;
		if ((error = cmd->param.status)) {
			u_int8_t *error_str =
			&(cmd_hdr->err_desc[strlen(cmd_hdr->err_desc) + 1]);

			if (error_str[0] == '\0')
				error_str = twa_find_msg_string(twa_error_table,
						cmd_hdr->status_block.error);

			twa_printf(sc, "cmd = 0x%x: ERROR: (0x%02X: 0x%04X): %s: %s\n",
					cmd->param.opcode,
					TWA_MESSAGE_SOURCE_CONTROLLER_ERROR,
					cmd_hdr->status_block.error,
					error_str,
					cmd_hdr->err_desc);
			goto out; /* twa_drain_complete_queue will have done the unmapping */
		}
		twa_release_request(tr);
		return(param);
	} else {
		/* There's a call back.  Simply submit the command. */
		tr->tr_callback = callback;
		if ((error = twa_map_request(tr))) {
			twa_printf(tr->tr_sc, "%s: twa_map_request returned 0x%x\n",
						__func__, error);
			goto out;
		}
		return(callback);
	}

out:
	twa_printf(sc, "get_param failed. error = 0x%x\n", error);
	if (param)
		free(param, M_DEVBUF);
	if (tr)
		twa_release_request(tr);
	return(NULL);
}


/*
 * Function name:	twa_set_param
 * Description:		Set a firmware parameter.
 *
 * Input:		sc		-- ptr to per ctlr structure
 *			table_id	-- parameter table #
 *			param_id	-- index of the parameter in the table
 *			param_size	-- size of the parameter in bytes
 *			callback	-- ptr to function, if any, to be called
 *					back on completion; NULL if no callback.
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_set_param(struct twa_softc *sc, int table_id,
			int param_id, int param_size, void *data,
			void (* callback)(struct twa_request *tr))
{
	struct twa_request		*tr;
	struct twa_command_header	*cmd_hdr;
	union twa_command_7k		*cmd;
	struct twa_param_9k		*param = NULL;
	int				error = ENOMEM;

	twa_dbg_dprint_enter(4, sc);

	/* Get a request packet. */
	if ((tr = twa_get_request(sc)) == NULL)
		goto out;
	tr->tr_cmd_pkt_type |= TWA_CMD_PKT_TYPE_INTERNAL;

	/* Allocate memory to send data using. */
	if ((param = (struct twa_param_9k *)
			malloc(TWA_SECTOR_SIZE, M_DEVBUF, M_NOWAIT)) == NULL)
		goto out;
	bzero(param, sizeof(struct twa_param_9k) - 1 + param_size);
	tr->tr_data = param;
	tr->tr_length = TWA_SECTOR_SIZE;
	tr->tr_flags = TWA_CMD_DATA_IN | TWA_CMD_DATA_OUT;

	/* Build the cmd pkt. */
	cmd_hdr = &(tr->tr_command->cmd_hdr);
	cmd_hdr->header_desc.size_header = 128;

	cmd = &(tr->tr_command->command.cmd_pkt_7k);
	cmd->param.opcode = TWA_OP_SET_PARAM;
	cmd->param.sgl_offset = 2;
	cmd->param.size = 2;
	cmd->param.request_id = tr->tr_request_id;
	cmd->param.unit = 0;
	cmd->param.param_count = 1;

	/* Specify which parameter we want to set. */
	param->table_id = table_id | TWA_9K_PARAM_DESCRIPTOR;
	param->parameter_id = param_id;
	param->parameter_size_bytes = param_size;
	bcopy(data, param->data, param_size);

	/* Submit the command. */
	if (callback == NULL) {
		/* There's no call back;  wait till the command completes. */
		error = twa_immediate_request(tr, TWA_REQUEST_TIMEOUT_PERIOD);
		if (error == ETIMEDOUT)
			return(error); /* clean-up done by twa_immediate_request */
		if (error)
			goto out;
		if ((error = cmd->param.status)) {
			u_int8_t *error_str =
			&(cmd_hdr->err_desc[strlen(cmd_hdr->err_desc) + 1]);

			if (error_str[0] == '\0')
				error_str = twa_find_msg_string(twa_error_table,
						cmd_hdr->status_block.error);
			twa_printf(sc, "cmd = 0x%x: ERROR: (0x%02X: 0x%04X): %s: %s\n",
					cmd->param.opcode,
					TWA_MESSAGE_SOURCE_CONTROLLER_ERROR,
					cmd_hdr->status_block.error,
					error_str,
					cmd_hdr->err_desc);
			goto out; /* twa_drain_complete_queue will have done the unmapping */
		}
		free(param, M_DEVBUF);
		twa_release_request(tr);
		return(error);
	} else {
		/* There's a call back.  Simply submit the command. */
		tr->tr_callback = callback;
		if ((error = twa_map_request(tr))) {
			twa_printf(tr->tr_sc, "%s: twa_map_request returned 0x%x\n",
						__func__, error);
			goto out;
		}
		return(0);
	}

out:
	twa_printf(sc, "set_param failed. error = 0x%x\n", error);
	if (param)
		free(param, M_DEVBUF);
	if (tr)
		twa_release_request(tr);
	return(error);
}


/*
 * Function name:	twa_init_connection
 * Description:		Send init_connection cmd to firmware
 *
 * Input:		sc		-- ptr to per ctlr structure
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
static int
twa_init_connection(struct twa_softc *sc, u_int16_t message_credits,
			u_int32_t set_features, u_int16_t current_fw_srl,
			u_int16_t current_fw_arch_id, u_int16_t current_fw_branch,
			u_int16_t current_fw_build, u_int16_t *fw_on_ctlr_srl,
			u_int16_t *fw_on_ctlr_arch_id, u_int16_t *fw_on_ctlr_branch,
			u_int16_t *fw_on_ctlr_build, u_int32_t *init_connect_result)
{
	struct twa_request		*tr;
	struct twa_command_header	*cmd_hdr;
	struct twa_command_init_connect	*init_connect;
	int				error = 1;
    
	twa_dbg_dprint_enter(3, sc);

	/* Get a request packet. */
	if ((tr = twa_get_request(sc)) == NULL)
		goto out;
	tr->tr_cmd_pkt_type |= TWA_CMD_PKT_TYPE_INTERNAL;
	/* Build the cmd pkt. */
	cmd_hdr = &(tr->tr_command->cmd_hdr);
	cmd_hdr->header_desc.size_header = 128;

	init_connect = &(tr->tr_command->command.cmd_pkt_7k.init_connect);
	init_connect->opcode = TWA_OP_INIT_CONNECTION;
   	init_connect->request_id = tr->tr_request_id;
	init_connect->message_credits = message_credits;
	init_connect->features = set_features;
	if (TWA_64BIT_ADDRESSES)
		init_connect->features |= TWA_64BIT_SG_ADDRESSES;
	if (set_features & TWA_EXTENDED_INIT_CONNECT) {
		/* Fill in the extra fields needed for an extended init_connect. */
		init_connect->size = 6;
		init_connect->fw_srl = current_fw_srl;
		init_connect->fw_arch_id = current_fw_arch_id;
		init_connect->fw_branch = current_fw_branch;
		init_connect->fw_build = current_fw_build;
	} else
		init_connect->size = 3;

	/* Submit the command, and wait for it to complete. */
	error = twa_immediate_request(tr, TWA_REQUEST_TIMEOUT_PERIOD);
	if (error == ETIMEDOUT)
		return(error); /* clean-up done by twa_immediate_request */
	if (error)
		goto out;
	if ((error = init_connect->status)) {
		u_int8_t *error_str =
		&(cmd_hdr->err_desc[strlen(cmd_hdr->err_desc) + 1]);

		if (error_str[0] == '\0')
			error_str = twa_find_msg_string(twa_error_table,
					cmd_hdr->status_block.error);
		twa_printf(sc, "cmd = 0x%x: ERROR: (0x%02X: 0x%04X): %s: %s\n",
					init_connect->opcode,
					TWA_MESSAGE_SOURCE_CONTROLLER_ERROR,
					cmd_hdr->status_block.error,
					error_str,
					cmd_hdr->err_desc);
		goto out; /* twa_drain_complete_queue will have done the unmapping */
	}
	if (set_features & TWA_EXTENDED_INIT_CONNECT) {
		*fw_on_ctlr_srl = init_connect->fw_srl;
		*fw_on_ctlr_arch_id = init_connect->fw_arch_id;
		*fw_on_ctlr_branch = init_connect->fw_branch;
		*fw_on_ctlr_build = init_connect->fw_build;
		*init_connect_result = init_connect->result;
	}
	twa_release_request(tr);
	return(error);

out:
	twa_printf(sc, "init_connection failed. error = 0x%x\n", error);
	if (tr)
		twa_release_request(tr);
	return(error);
}



/*
 * Function name:	twa_wait_request
 * Description:		Sends down a firmware cmd, and waits for the completion,
 *			but NOT in a tight loop.
 *
 * Input:		tr	-- ptr to request pkt
 *			timeout -- max # of seconds to wait before giving up
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_wait_request(struct twa_request *tr, u_int32_t timeout)
{
	time_t	end_time;
	int	s;
	int	error;

	twa_dbg_dprint_enter(4, tr->tr_sc);

	tr->tr_flags |= TWA_CMD_SLEEP_ON_REQUEST;
	tr->tr_status = TWA_CMD_BUSY;
	if ((error = twa_map_request(tr))) {
		twa_printf(tr->tr_sc, "%s: twa_map_request returned 0x%x\n",
						__func__, error);
		return(error);
	}

	s = splcam();
	end_time = time_second + timeout;
	while (tr->tr_status != TWA_CMD_COMPLETE) {
		if ((error = tr->tr_error))
			goto err;
		if ((error = tsleep(tr, PRIBIO, "twawait", timeout * hz)) == 0) {
			if ((error = tr->tr_error)) /* possible reset */
				goto err;
			error = (tr->tr_status != TWA_CMD_COMPLETE);
			break;
		}
		tr->tr_flags &= ~TWA_CMD_SLEEP_ON_REQUEST;
		if (error == EWOULDBLOCK) {
			/* Time out! */
			twa_printf(tr->tr_sc, "%s: Request %p timed out.\n",
								__func__, tr);
			/*
			 * We will reset the controller only if the request has
			 * already been submitted, so as to not lose the
			 * request packet.  If a busy request timed out, the
			 * reset will take care of freeing resources.  If a
			 * pending request timed out, we will free resources
			 * for that request, right here.  So, the caller is
			 * expected to NOT cleanup when ETIMEDOUT is returned.
			 */
			if (tr->tr_status != TWA_CMD_PENDING)
				twa_reset(tr->tr_sc);
			else {
				/* Request was never submitted.  Clean up. */
				twa_remove_pending(tr);
				twa_unmap_request(tr);
			}
			if (tr->tr_data)
				free(tr->tr_data, M_DEVBUF);
			twa_release_request(tr);
			splx(s);
			return(ETIMEDOUT);
		}
		/* 
		 * Either the request got completed, or we were woken up by a
		 * signal.  Calculate the new timeout, in case it was the latter.
		 */
		timeout = (end_time - time_second);
	}
	twa_unmap_request(tr);

err:
	splx(s);
	return(error);
}



/*
 * Function name:	twa_immediate_request
 * Description:		Sends down a firmware cmd, and waits for the completion
 *			in a tight loop.
 *
 * Input:		tr	-- ptr to request pkt
 *			timeout -- max # of seconds to wait before giving up
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_immediate_request(struct twa_request *tr, u_int32_t timeout)
{
	time_t	end_time;
	int	error = 0;

	twa_dbg_dprint_enter(4, tr->tr_sc);

	if ((error = twa_map_request(tr))) {
		twa_printf(tr->tr_sc, "%s: twa_map_request returned 0x%x\n",
						__func__, error);
		return(error);
	}

	end_time = time_second + timeout;
	do {
		if ((error = tr->tr_error))
			return(error);
		twa_done(tr->tr_sc);
		if ((tr->tr_status != TWA_CMD_BUSY) &&
			(tr->tr_status != TWA_CMD_PENDING)) {
			twa_unmap_request(tr);
			return(tr->tr_status != TWA_CMD_COMPLETE);
		}
	} while (time_second <= end_time);

	/* Time out! */
	twa_printf(tr->tr_sc, "%s: Request %p timed out.\n", __func__, tr);
	/*
	 * We will reset the controller only if the request has
	 * already been submitted, so as to not lose the
	 * request packet.  If a busy request timed out, the
	 * reset will take care of freeing resources.  If a
	 * pending request timed out, we will free resources
	 * for that request, right here.  So, the caller is
	 * expected to NOT cleanup when ETIMEDOUT is returned.
	 */
	if (tr->tr_status != TWA_CMD_PENDING)
		twa_reset(tr->tr_sc);
	else {
		/* Request was never submitted.  Clean up. */
		twa_remove_pending(tr);
		twa_unmap_request(tr);
		if (tr->tr_data)
			free(tr->tr_data, M_DEVBUF);
		twa_release_request(tr);
	}
	return(ETIMEDOUT);
}



/*
 * Function name:	twa_complete_io
 * Description:		Callback on scsi requests to fw.
 *
 * Input:		tr	-- ptr to request pkt
 * Output:		None
 * Return value:	None
 */
void
twa_complete_io(struct twa_request *tr)
{
	struct twa_softc	*sc = tr->tr_sc;

	twa_dbg_dprint_enter(8, sc);

	if (tr->tr_status != TWA_CMD_COMPLETE)
		twa_panic(sc, "twa_complete_io on incomplete command");
	if (tr->tr_private) /* This is a scsi cmd.  Complete it. */
		twa_scsi_complete(tr);
	twa_release_request(tr);
}


/*
 * Function name:	twa_reset
 * Description:		Soft resets and then initializes the controller;
 *			drains any incomplete requests.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
int
twa_reset(struct twa_softc *sc)
{
	int	s;
	int	error = 0;

	twa_dbg_dprint_enter(2, sc);

	/*
	 * Disable interrupts from the controller, and mask any
	 * accidental entry into our interrupt handler.
	 */
	twa_disable_interrupts(sc);
	s = splcam();
	
	/*
	 * Complete all requests in the complete queue; error back all requests
	 * in the busy queue.  Any internal requests will be simply freed.
	 * Re-submit any requests in the pending queue.
	 */
	twa_drain_complete_queue(sc);
	twa_drain_busy_queue(sc);

	/* Soft reset the controller. */
	if ((error = twa_soft_reset(sc))) {
		twa_printf (sc, "Controller reset failed.\n");
		goto out;
	}

	/* Re-establish logical connection with the controller. */
	if ((error = twa_init_connection(sc, TWA_INIT_MESSAGE_CREDITS,
					0, 0, 0, 0, 0,
					NULL, NULL, NULL, NULL, NULL))) {
		twa_printf(sc, "Can't initialize connection after reset.\n");
		goto out;
	}

	twa_printf(sc, "Controller reset done!\n");

out:
	splx(s);
	/*
	 * Enable interrupts, and also clear attention and response interrupts.
	 */
	twa_enable_interrupts(sc);
	return(error);
}



/*
 * Function name:	twa_soft_reset
 * Description:		Does the actual soft reset.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_soft_reset(struct twa_softc *sc)
{
	u_int32_t	status_reg;

	twa_dbg_dprint_enter(1, sc);

	twa_printf(sc, "Resetting controller...\n");
	TWA_SOFT_RESET(sc);

	if (twa_wait_status(sc, TWA_STATUS_MICROCONTROLLER_READY |
				TWA_STATUS_ATTENTION_INTERRUPT, 30)) {
		twa_printf(sc, "Micro-ctlr not ready/No attn intr after reset.\n");
		return(1);
	}
	TWA_WRITE_CONTROL_REGISTER(sc, TWA_CONTROL_CLEAR_ATTENTION_INTERRUPT);
	if (twa_drain_response_queue(sc)) {
		twa_printf(sc, "Can't drain response queue.\n");
		return(1);
	}
	if (twa_drain_aen_queue(sc)) {
		twa_printf(sc, "Can't drain AEN queue.\n");
		return(1);
	}
	if (twa_find_aen(sc, TWA_AEN_SOFT_RESET)) {
		twa_printf(sc, "Reset not reported by controller.\n");
		return(1);
	}
	status_reg = TWA_READ_STATUS_REGISTER(sc);
	if (TWA_STATUS_ERRORS(status_reg) ||
				twa_check_ctlr_state(sc, status_reg)) {
		twa_printf(sc, "Controller errors detected.\n");
		return(1);
	}
	return(0);
}



/*
 * Function name:	twa_submit_io
 * Description:		Wrapper to twa_start.
 *
 * Input:		tr	-- ptr to request pkt
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
int
twa_submit_io(struct twa_request *tr)
{
	int	error;

	if ((error = twa_start(tr))) {
		if (tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_EXTERNAL) {
			if (error == EBUSY)
				/*
				 * Cmd queue is full.  Freeze the simq to
				 * maintain ccb ordering.  The next ccb that
				 * gets completed will unfreeze the simq.
				 */
				twa_disallow_new_requests(tr->tr_sc);
			else
				/* It's a controller error. */
				twa_printf(tr->tr_sc, "SCSI cmd = 0x%x: ERROR: (0x%02X: 0x%04X)\n",
					tr->tr_command->command.cmd_pkt_9k.cdb[0],
					TWA_MESSAGE_SOURCE_CONTROLLER_ERROR,
					error);
			
			tr->tr_error = error;
			twa_scsi_complete(tr);
		} else {
			if (error == EBUSY)
				error = 0; /* the request will be in the pending queue */
			else {
				twa_printf(tr->tr_sc, "cmd = 0x%x: ERROR: (0x%02X: 0x%04X)\n",
						(tr->tr_cmd_pkt_type == TWA_CMD_PKT_TYPE_9K) ?
						(tr->tr_command->command.cmd_pkt_9k.command.opcode) :
						(tr->tr_command->command.cmd_pkt_7k.generic.opcode),
						TWA_MESSAGE_SOURCE_CONTROLLER_ERROR,
						tr->tr_error);
				tr->tr_error = error;
			}
		}
	}
	return(error);
}



/*
 * Function name:	twa_start
 * Description:		Posts a cmd to firmware.
 *
 * Input:		tr	-- ptr to request pkt
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
int
twa_start(struct twa_request *tr)
{
	struct twa_softc	*sc = tr->tr_sc;
	u_int32_t		status_reg;
	int			s;
	int			error;

	twa_dbg_dprint_enter(10, sc);

	s = splcam();
	/* Check to see if we can post a command. */
	status_reg = TWA_READ_STATUS_REGISTER(sc);
	if ((error = twa_check_ctlr_state(sc, status_reg)))
		goto out;

	if (status_reg & TWA_STATUS_COMMAND_QUEUE_FULL) {
		if ((tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_INTERNAL) ||
			(tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_IOCTL)) {
			if (tr->tr_status != TWA_CMD_PENDING) {
				twa_dbg_dprint(2, sc, "pending internal/ioctl request");
				tr->tr_status = TWA_CMD_PENDING;
				twa_enqueue_pending(tr);
			}
			TWA_WRITE_CONTROL_REGISTER(sc,
					TWA_CONTROL_UNMASK_COMMAND_INTERRUPT);
		}
		error = EBUSY;
	} else {
		/* Mark the request as currently being processed. */
		tr->tr_status = TWA_CMD_BUSY;
		/* Move the request into the busy queue. */
		twa_enqueue_busy(tr);
		/* Cmd queue is not full.  Post the command. */
		TWA_WRITE_COMMAND_QUEUE(sc,
			tr->tr_cmd_phys + sizeof(struct twa_command_header));
	}

out:
	splx(s);
	return(error);
}



/*
 * Function name:	twa_done
 * Description:		Looks for cmd completions from fw; queues cmds completed
 *			by fw into complete queue.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- no ctlr error
 *			non-zero-- ctlr error
 */
static int
twa_done(struct twa_softc *sc)
{
	union twa_response_queue	rq;
	struct twa_request		*tr;
	int				s;
	int				error = 0;
	u_int32_t			status_reg;
    
	twa_dbg_dprint_enter(10, sc);

	s = splcam();
	for (;;) {
		status_reg = TWA_READ_STATUS_REGISTER(sc);
		if ((error = twa_check_ctlr_state(sc, status_reg)))
			break;
		if (status_reg & TWA_STATUS_RESPONSE_QUEUE_EMPTY)
			break;
		/* Response queue is not empty. */
		rq = TWA_READ_RESPONSE_QUEUE(sc);
		tr = sc->twa_lookup[rq.u.response_id];	/* lookup the request */
		if (tr->tr_status != TWA_CMD_BUSY)
			twa_printf(sc, "ERROR: Unposted command completed!! req = %p; status = %d\n",
					tr, tr->tr_status);
		tr->tr_status = TWA_CMD_COMPLETE;
		/* Enqueue request in the complete queue. */
		twa_remove_busy(tr);
		twa_enqueue_complete(tr);
	}
	splx(s);

	/* Complete this, and other requests in the complete queue. */
	twa_drain_complete_queue(sc);
	return(error);
}



/*
 * Function name:	twa_drain_pending_queue
 * Description:		Kick starts any requests in the pending queue.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- all pending requests drained
 *			non-zero-- otherwise
 */
static int
twa_drain_pending_queue(struct twa_softc *sc)
{
	struct twa_request	*tr;
	int			error = 0;
    
	twa_dbg_dprint_enter(10, sc);
	
	/*
	 * Pull requests off the pending queue, and submit them.
	 */
	while ((tr = twa_dequeue_pending(sc)) != NULL) {
		if ((error = twa_start(tr))) {
			if (error == EBUSY) {
				twa_dbg_dprint(2, sc, "Requeueing pending request");
				tr->tr_status = TWA_CMD_PENDING;
				twa_requeue_pending(tr);/* queue at the head */
				break;
			} else {
				twa_printf(sc, "%s: twa_start returned 0x%x\n",
							__func__, error);
				tr->tr_error = error;
				if (tr->tr_flags & TWA_CMD_SLEEP_ON_REQUEST)
					wakeup_one(tr);/* let the caller know it failed */
				error = 0;
			}
		}
	}
	return(error);
}



/*
 * Function name:	twa_drain_complete_queue
 * Description:		Does unmapping for each request completed by fw,
 *			and lets the request originators know of the completion.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
static void
twa_drain_complete_queue(struct twa_softc *sc)
{
	struct twa_request	*tr;
    
	twa_dbg_dprint_enter(10, sc);

	/*
	 * Pull commands off the completed list, dispatch them appropriately.
	 */
	while ((tr = twa_dequeue_complete(sc)) != NULL) {
		/* Unmap the command packet, and any associated data buffer. */
		twa_unmap_request(tr);

		/* Call the callback, if there's one. */
		if (tr->tr_callback)
			tr->tr_callback(tr);
		else
			if (tr->tr_flags & TWA_CMD_SLEEP_ON_REQUEST) {
				/* Wake up the sleeping command originator. */
				twa_dbg_dprint(7, sc, "Waking up originator of request %p", tr);
				wakeup_one(tr);
			}
	}
}



/*
 * Function name:	twa_wait_status
 * Description:		Wait for a given status to show up in the fw status register.
 *
 * Input:		sc	-- ptr to per ctlr structure
 *			status	-- status to look for
 *			timeout -- max # of seconds to wait before giving up
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_wait_status(struct twa_softc *sc, u_int32_t status, u_int32_t timeout)
{
	time_t		end_time;
	u_int32_t	status_reg;

	twa_dbg_dprint_enter(4, sc);

	end_time = time_second + timeout;
	do {
		status_reg = TWA_READ_STATUS_REGISTER(sc);
		if ((status_reg & status) == status)/* got the required bit(s)? */
			return(0);
		DELAY(1000);
	} while (time_second <= end_time);

	return(1);
}



/*
 * Function name:	twa_drain_response_queue
 * Description:		Drain the response queue.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_drain_response_queue(struct twa_softc *sc)
{
	union twa_response_queue	rq;
	u_int32_t			status_reg;

	twa_dbg_dprint_enter(4, sc);

	for (;;) {
		status_reg = TWA_READ_STATUS_REGISTER(sc);
		if (twa_check_ctlr_state(sc, status_reg))
			return(1);
		if (status_reg & TWA_STATUS_RESPONSE_QUEUE_EMPTY)
			return(0); /* no more response queue entries */
		rq = TWA_READ_RESPONSE_QUEUE(sc);
	}
}



/*
 * Function name:	twa_host_intr
 * Description:		This function gets called if we triggered an interrupt.
 *			We don't use it as of now.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
static void
twa_host_intr(struct twa_softc *sc)
{
	twa_dbg_dprint_enter(6, sc);

	TWA_WRITE_CONTROL_REGISTER(sc, TWA_CONTROL_CLEAR_HOST_INTERRUPT);
}



/*
 * Function name:	twa_attention_intr
 * Description:		This function gets called if the fw posted an AEN
 *			(Asynchronous Event Notification).  It fetches
 *			all the AEN's that the fw might have posted.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
static void
twa_attention_intr(struct twa_softc *sc)
{
	int	error;

	twa_dbg_dprint_enter(6, sc);

	TWA_WRITE_CONTROL_REGISTER(sc, TWA_CONTROL_CLEAR_ATTENTION_INTERRUPT);
	if ((error = twa_fetch_aen(sc)))
		twa_printf(sc, "Fetch AEN failed. error = 0x%x\n", error);
}



/*
 * Function name:	twa_command_intr
 * Description:		This function gets called if we hit a queue full
 *			condition earlier, and the fw is now ready for
 *			new cmds.  Submits any pending requests.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
static void
twa_command_intr(struct twa_softc *sc)
{
	twa_dbg_dprint_enter(6, sc);

	TWA_WRITE_CONTROL_REGISTER(sc, TWA_CONTROL_MASK_COMMAND_INTERRUPT);
	/*
	 * Start any requests that might be in the pending queue.
	 * If all requests could not be started because of a queue_full
	 * condition, twa_start will have unmasked the command interrupt.
	 */
	twa_drain_pending_queue(sc);
}



/*
 * Function name:	twa_fetch_aen
 * Description:		Send down a Request Sense cmd to fw to fetch an AEN.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_fetch_aen(struct twa_softc *sc)
{
	struct twa_request	*tr;
	int			error = 0;

	twa_dbg_dprint_enter(4, sc);

	if ((tr = twa_get_request(sc)) == NULL)
		return(EIO);
	tr->tr_cmd_pkt_type |= TWA_CMD_PKT_TYPE_INTERNAL;
	tr->tr_callback = twa_aen_callback;
	if ((error = twa_send_scsi_cmd(tr, 0x03 /* REQUEST_SENSE */))) {
		if (tr->tr_data)
			free(tr->tr_data, M_DEVBUF);
		twa_release_request(tr);
	}
	return(error);
}



/*
 * Function name:	twa_aen_callback
 * Description:		Callback for requests to fetch AEN's.
 *
 * Input:		tr	-- ptr to completed request pkt
 * Output:		None
 * Return value:	None
 */
static void
twa_aen_callback(struct twa_request *tr)
{
	struct twa_softc		*sc = tr->tr_sc;
	struct twa_command_header	*cmd_hdr;
	struct twa_command_9k		*cmd = &(tr->tr_command->command.cmd_pkt_9k);
	u_int8_t			*error_str;
	int				fetch_more_aens = 0;
	int				error;
	int				i;

	twa_dbg_dprint_enter(4, sc);

	twa_dbg_dprint(4, sc, "req_id = 0x%x, status = 0x%x",
				cmd->request_id,
				cmd->status);

	if (tr->tr_error)
		goto out;

	if (! cmd->status) {
		cmd_hdr = (struct twa_command_header *)(tr->tr_data);
		if ((tr->tr_cmd_pkt_type & TWA_CMD_PKT_TYPE_9K) &&
			(cmd->cdb[0] == 0x3 /* REQUEST_SENSE */))
			if (twa_enqueue_aen(sc, cmd_hdr) != TWA_AEN_QUEUE_EMPTY)
				fetch_more_aens = 1;
	} else {
		cmd_hdr = &(tr->tr_command->cmd_hdr);
		error_str = &(cmd_hdr->err_desc[strlen(cmd_hdr->err_desc) + 1]);

		if (error_str[0] == '\0')
			error_str = twa_find_msg_string(twa_error_table,
						cmd_hdr->status_block.error);
		twa_printf(sc, "%s: cmd = 0x%x: ERROR: (0x%02X: 0x%04X): %s: %s\n",
				__func__, cmd->command.opcode,
				TWA_MESSAGE_SOURCE_CONTROLLER_ERROR,
				cmd_hdr->status_block.error,
				error_str,
				cmd_hdr->err_desc);
		twa_dbg_print(2, "sense info: ");
		for (i = 0; i < 18; i++)
			twa_dbg_print(2, "%x\t", tr->tr_command->cmd_hdr.sense_data[i]);
		twa_dbg_print(2, ""); /* print new line */
		for (i = 0; i < 128; i++)
			twa_dbg_print(7, "%x\t", ((int8_t *)(tr->tr_data))[i]);
	}

out:
	if (tr->tr_data)
		free(tr->tr_data, M_DEVBUF);
	twa_release_request(tr);
	if (fetch_more_aens)
		if ((error = twa_fetch_aen(sc)))
			twa_printf(sc, "%s: Fetch AEN failed. error = 0x%x\n",
					__func__, error);
}



/*
 * Function name:	twa_drain_aen_queue
 * Description:		Fetches all un-retrieved AEN's posted by fw.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_drain_aen_queue(struct twa_softc *sc)
{
	struct twa_request		*tr;
	struct twa_command_header	*cmd_hdr;
	time_t				end_time;
	int				error = 0;

	for (;;) {
		if ((tr = twa_get_request(sc)) == NULL) {
			error = EIO;
			break;
		}
		tr->tr_cmd_pkt_type |= TWA_CMD_PKT_TYPE_INTERNAL;
		tr->tr_callback = NULL;
		if ((error = twa_send_scsi_cmd(tr, 0x03 /* REQUEST_SENSE */))) {
			twa_dbg_dprint(1, sc, "Cannot send command to fetch aen");
			break;
		}

		end_time = time_second + TWA_REQUEST_TIMEOUT_PERIOD;
		do {
			twa_done(tr->tr_sc);
			if (tr->tr_status != TWA_CMD_BUSY)
				break;
		} while (time_second <= end_time);

		if (tr->tr_status != TWA_CMD_COMPLETE) {
			error = ETIMEDOUT;
			break;
		}

		if ((error = tr->tr_command->command.cmd_pkt_9k.status))
			break;

		cmd_hdr = (struct twa_command_header *)(tr->tr_data);
		if (twa_enqueue_aen(sc, cmd_hdr) == TWA_AEN_QUEUE_EMPTY)
			break;

		free(tr->tr_data, M_DEVBUF);
		twa_release_request(tr);
	}

	if (tr) {
		if (tr->tr_data)
			free(tr->tr_data, M_DEVBUF);
		twa_release_request(tr);
	}
	return(error);
}



/*
 * Function name:	twa_enqueue_aen
 * Description:		Queues AEN's to be supplied to user-space tools on request.
 *
 * Input:		sc	-- ptr to per ctlr structure
 *			cmd_hdr	-- ptr to hdr of fw cmd pkt, from where the AEN
 *				   details can be retrieved.
 * Output:		None
 * Return value:	AEN code
 */
static unsigned short
twa_enqueue_aen(struct twa_softc *sc, struct twa_command_header *cmd_hdr)
{
	struct twa_event_packet	*event;
	unsigned short		aen_code;
	unsigned long		local_time;
	unsigned long		sync_time;
	u_int8_t		*aen_str;
	int			s;

	twa_dbg_dprint_enter(4, sc);
	s = splcam();
	aen_code = cmd_hdr->status_block.error;

	switch (aen_code) {
	case TWA_AEN_SYNC_TIME_WITH_HOST:
		twa_dbg_dprint(4, sc, "Received AEN_SYNC_TIME");
		/* Calculate time (in seconds) since last Sunday 12.00 AM. */
		local_time = time_second - (tz_minuteswest * 60) -
					(wall_cmos_clock ? adjkerntz : 0);
		sync_time = (local_time - (3 * 86400)) % 604800;
		if (twa_set_param(sc, TWA_PARAM_TIME_TABLE,
					TWA_PARAM_TIME_SchedulerTime, 4,
					&sync_time, twa_aen_callback))
			twa_printf(sc, "Unable to sync time with ctlr!\n");
		break;

	case TWA_AEN_QUEUE_EMPTY:
		twa_dbg_dprint(4, sc, "AEN queue empty");
		break;

	default:
		/* Queue the event. */
		event = sc->twa_aen_queue[sc->twa_aen_head];
		if (event->retrieved == TWA_AEN_NOT_RETRIEVED)
			sc->twa_aen_queue_overflow = TRUE;
		event->severity = cmd_hdr->status_block.substatus_block.severity;
		local_time = time_second - (tz_minuteswest * 60) -
					(wall_cmos_clock ? adjkerntz : 0);
		event->time_stamp_sec = local_time;
		event->aen_code = aen_code;
		event->retrieved = TWA_AEN_NOT_RETRIEVED;
		event->sequence_id = ++(sc->twa_current_sequence_id);

		aen_str = &(cmd_hdr->err_desc[strlen(cmd_hdr->err_desc) + 1]);
		if (aen_str[0] == '\0')
			aen_str = twa_find_msg_string(twa_aen_table, aen_code);


		event->parameter_len = strlen(cmd_hdr->err_desc) +
					strlen(aen_str) + 2;
		bcopy(cmd_hdr->err_desc, event->parameter_data,
					event->parameter_len);

		twa_dbg_dprint(4, sc, "event = %x %x %x %x %x %x %x\n %s %s",
				event->sequence_id,
				event->time_stamp_sec,
				event->aen_code,
				event->severity,
				event->retrieved,
				event->repeat_count,
				event->parameter_len,
				&(event->parameter_data[strlen(cmd_hdr->err_desc) + 1]),
				event->parameter_data);

		twa_dbg_dprint(4, sc, "cmd_hdr = %x %lx %x %x %x %x %zx\n %s %s",
				sc->twa_current_sequence_id,
				local_time,
				cmd_hdr->status_block.error,
				cmd_hdr->status_block.substatus_block.severity,
				TWA_AEN_NOT_RETRIEVED,
				0,
				strlen(cmd_hdr->err_desc),
				&(cmd_hdr->err_desc[strlen(cmd_hdr->err_desc) + 1]),
				cmd_hdr->err_desc);

		/* Print the event. */
		if (event->severity < TWA_AEN_SEVERITY_DEBUG)
			twa_printf(sc,  "%s: (0x%02X: 0x%04X): %s: %s\n",
					twa_aen_severity_table[event->severity],
					TWA_MESSAGE_SOURCE_CONTROLLER_EVENT,
					aen_code,
					aen_str,
					event->parameter_data);

		if ((sc->twa_aen_head + 1) == TWA_Q_LENGTH)
			sc->twa_aen_queue_wrapped = TRUE;
		sc->twa_aen_head = (sc->twa_aen_head + 1) % TWA_Q_LENGTH;
		break;
	} /* switch */
	splx(s);
	return(aen_code);
}



/*
 * Function name:	twa_find_aen
 * Description:		Reports whether a given AEN ever occurred.
 *
 * Input:		sc	-- ptr to per ctlr structure
 *			aen_code-- AEN to look for
 * Output:		None
 * Return value:	0	-- success
 *			non-zero-- failure
 */
static int
twa_find_aen(struct twa_softc *sc, u_int16_t aen_code)
{
	u_int32_t	last_index;
	int		s;
	int		i;

	s = splcam();

	if (sc->twa_aen_queue_wrapped)
		last_index = sc->twa_aen_head;
	else
		last_index = 0;

	i = sc->twa_aen_head;
	do {
		i = (i + TWA_Q_LENGTH - 1) % TWA_Q_LENGTH;
		if ((sc->twa_aen_queue[i])->aen_code == aen_code) {
			splx(s);
			return(0);
		}
	} while (i != last_index);

	splx(s);
	return(1);
}



/*
 * Function name:	twa_find_msg_string
 * Description:		Looks up a given table, and returns the message string
 *			corresponding to a given code (error code or AEN code).
 *
 * Input:		sc	-- ptr to per ctlr structure
 *			code	-- code, the message string corresponding to
 *				   which is to be returned.
 * Output:		None
 * Return value:	ptr to corresponding msg string	-- success
 *			NULL				-- failure
 */
char *
twa_find_msg_string(struct twa_message *table, u_int16_t code)
{
	int	i;

	for (i = 0; table[i].message != NULL; i++)
		if (table[i].code == code)
			return(table[i].message);

	return(table[i].message);
}



/*
 * Function name:	twa_get_request
 * Description:		Gets a request pkt from the free queue.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	ptr to request pkt	-- success
 *			NULL			-- failure
 */
struct twa_request *
twa_get_request(struct twa_softc *sc)
{
	struct twa_request	*tr;

	twa_dbg_dprint_enter(4, sc);

	/* Get a free request packet. */
	tr = twa_dequeue_free(sc);

	/* Initialize some fields to their defaults. */
	if (tr) {
		tr->tr_data = NULL;
		tr->tr_real_data = NULL;
		tr->tr_length = 0;
		tr->tr_real_length = 0;
		tr->tr_status = TWA_CMD_SETUP;/* command is in setup phase */
		tr->tr_flags = 0;
		tr->tr_error = 0;
		tr->tr_private = NULL;
		tr->tr_callback = NULL;
		tr->tr_cmd_pkt_type = 0;

		/*
		 * Look at the status field in the command packet to see how
		 * it completed the last time it was used, and zero out only
		 * the portions that might have changed.  Note that we don't
		 * care to zero out the sglist.
		 */
		if (tr->tr_command->command.cmd_pkt_9k.status)
			bzero(tr->tr_command,
				sizeof(struct twa_command_header) + 28 /* max bytes before sglist */);
		else
			bzero(&(tr->tr_command->command), 28 /* max bytes before sglist */);
	}
	return(tr);
}



/*
 * Function name:	twa_release_request
 * Description:		Puts a request pkt into the free queue.
 *
 * Input:		tr	-- ptr to request pkt to be freed
 * Output:		None
 * Return value:	None
 */
void
twa_release_request(struct twa_request *tr)
{
	twa_dbg_dprint_enter(4, tr->tr_sc);

	twa_enqueue_free(tr);
}



/*
 * Function name:	twa_describe_controller
 * Description:		Describes the controller, in terms of its fw version,
 *			BIOS version etc.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
void
twa_describe_controller(struct twa_softc *sc)
{
	struct twa_param_9k	*p[6];
	u_int8_t		num_ports = 0;

	twa_dbg_dprint_enter(2, sc);

	/* Get the port count. */
	p[0] = twa_get_param(sc, TWA_PARAM_CONTROLLER_TABLE,
				TWA_PARAM_CONTROLLER_PORT_COUNT, 1, NULL);
	if (p[0]) {
		num_ports = *(u_int8_t *)(p[0]->data);
		free(p[0], M_DEVBUF);
	}

	/* Get the firmware and BIOS versions. */
	p[0] = twa_get_param(sc, TWA_PARAM_VERSION_TABLE,
				TWA_PARAM_VERSION_FW, 16, NULL);
	p[1] = twa_get_param(sc, TWA_PARAM_VERSION_TABLE,
				TWA_PARAM_VERSION_BIOS, 16, NULL);

	twa_printf(sc, "%d ports, Firmware %.16s, BIOS %.16s\n",
			num_ports, p[0]?(p[0]->data):NULL, p[1]?(p[1]->data):NULL);
	if (bootverbose) {
		/* Get more versions. */
		p[2] = twa_get_param(sc, TWA_PARAM_VERSION_TABLE,
					TWA_PARAM_VERSION_MONITOR, 16, NULL);
		p[3] = twa_get_param(sc, TWA_PARAM_VERSION_TABLE,
					TWA_PARAM_VERSION_PCBA, 8, NULL);
		p[4] = twa_get_param(sc, TWA_PARAM_VERSION_TABLE,
					TWA_PARAM_VERSION_ATA, 8, NULL);
		p[5] = twa_get_param(sc, TWA_PARAM_VERSION_TABLE,
					TWA_PARAM_VERSION_PCI, 8, NULL);

		twa_printf(sc, "Monitor %.16s, PCB %.8s, Achip %.8s, Pchip %.8s\n",
				p[2]?(p[2]->data):NULL, p[3]?(p[3]->data):NULL,
				p[4]?(p[4]->data):NULL, p[5]?(p[5]->data):NULL);

		if (p[2])
			free(p[2], M_DEVBUF);
		if (p[3])
			free(p[3], M_DEVBUF);
		if (p[4])
			free(p[4], M_DEVBUF);
		if (p[5])
			free(p[5], M_DEVBUF);
	}
	if (p[0])
		free(p[0], M_DEVBUF);
	if (p[1])
		free(p[1], M_DEVBUF);
}



/*
 * Function name:	twa_check_ctlr_state
 * Description:		Makes sure that the fw status register reports a
 *			proper status.
 *
 * Input:		sc		-- ptr to per ctlr structure
 *			status_reg	-- value in the status register
 * Output:		None
 * Return value:	0	-- no errors
 *			non-zero-- errors
 */
static int
twa_check_ctlr_state(struct twa_softc *sc, u_int32_t status_reg)
{
	int		result = 0;
	static time_t	last_warning[2] = {0, 0};

	/* Check if the 'micro-controller ready' bit is not set. */
	if ((status_reg & TWA_STATUS_EXPECTED_BITS) !=
				TWA_STATUS_EXPECTED_BITS) {
		if (time_second > (last_warning[0] + 5)) {
			twa_printf(sc, "Missing expected status bit(s) %b\n",
					~status_reg & TWA_STATUS_EXPECTED_BITS,
					TWA_STATUS_BITS_DESCRIPTION);
			last_warning[0] = time_second;
		}
		result = 1;
	}

	/* Check if any error bits are set. */
	if ((status_reg & TWA_STATUS_UNEXPECTED_BITS) != 0) {
		if (time_second > (last_warning[1] + 5)) {
			twa_printf(sc, "Unexpected status bit(s) %b\n",
					status_reg & TWA_STATUS_UNEXPECTED_BITS,
					TWA_STATUS_BITS_DESCRIPTION);
			last_warning[1] = time_second;
		}
		if (status_reg & TWA_STATUS_PCI_PARITY_ERROR_INTERRUPT) {
			twa_printf(sc, "PCI parity error: clearing... Re-seat/move/replace card.\n");
			TWA_WRITE_CONTROL_REGISTER(sc, TWA_CONTROL_CLEAR_PARITY_ERROR);
			twa_write_pci_config(sc, TWA_PCI_CONFIG_CLEAR_PARITY_ERROR, 2);
		}
		if (status_reg & TWA_STATUS_PCI_ABORT_INTERRUPT) {
			twa_printf(sc, "PCI abort: clearing...\n");
			TWA_WRITE_CONTROL_REGISTER(sc, TWA_CONTROL_CLEAR_PCI_ABORT);
			twa_write_pci_config(sc, TWA_PCI_CONFIG_CLEAR_PCI_ABORT, 2);
		}
		if (status_reg & TWA_STATUS_QUEUE_ERROR_INTERRUPT) {
			twa_printf(sc, "Controller queue error: clearing...\n");
			TWA_WRITE_CONTROL_REGISTER(sc, TWA_CONTROL_CLEAR_PCI_ABORT);
		}
		if (status_reg & TWA_STATUS_SBUF_WRITE_ERROR) {
			twa_printf(sc, "SBUF write error: clearing...\n");
			TWA_WRITE_CONTROL_REGISTER(sc, TWA_CONTROL_CLEAR_SBUF_WRITE_ERROR);
		}
		if (status_reg & TWA_STATUS_MICROCONTROLLER_ERROR) {
			twa_printf(sc, "Micro-controller error!\n");
			result = 1;
		}
	}
	return(result);
}	



/*
 * Function name:	twa_print_controller
 * Description:		Prints the current status of the controller.
 *
 * Input:		sc	-- ptr to per ctlr structure
 * Output:		None
 * Return value:	None
 */
void
twa_print_controller(struct twa_softc *sc)
{
	u_int32_t	status_reg;

	/* Print current controller details. */
	status_reg = TWA_READ_STATUS_REGISTER(sc);
	twa_printf(sc, "status   %b\n", status_reg, TWA_STATUS_BITS_DESCRIPTION);
#ifdef TWA_DEBUG
	twa_printf(sc, "q type    current  max\n");
	twa_printf(sc, "free      %04d     %04d\n",
		sc->twa_qstats[TWAQ_FREE].q_length, sc->twa_qstats[TWAQ_FREE].q_max);
	twa_printf(sc, "busy      %04d     %04d\n",
		sc->twa_qstats[TWAQ_BUSY].q_length, sc->twa_qstats[TWAQ_BUSY].q_max);
	twa_printf(sc, "pending   %04d     %04d\n",
		sc->twa_qstats[TWAQ_PENDING].q_length, sc->twa_qstats[TWAQ_PENDING].q_max);
	twa_printf(sc, "complete  %04d     %04d\n",
		sc->twa_qstats[TWAQ_COMPLETE].q_length, sc->twa_qstats[TWAQ_COMPLETE].q_max);
#endif /* TWA_DEBUG */
	twa_printf(sc, "AEN queue head %d  tail %d\n",
			sc->twa_aen_head, sc->twa_aen_tail);
}	



/*
 * Function name:	twa_panic
 * Description:		Called when something is seriously wrong with the ctlr.
 *			Hits the debugger if the debugger is turned on, else
 *			resets the ctlr.
 *
 * Input:		sc	-- ptr to per ctlr structure
 *			reason	-- string describing what went wrong
 * Output:		None
 * Return value:	None
 */
static void
twa_panic(struct twa_softc *sc, int8_t *reason)
{
	twa_print_controller(sc);
#ifdef TWA_DEBUG
	panic(reason);
#else
	twa_printf(sc, "twa_panic: RESETTING CONTROLLER...\n");
	twa_reset(sc);
#endif
}

