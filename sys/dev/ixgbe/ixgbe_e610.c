/******************************************************************************
  SPDX-License-Identifier: BSD-3-Clause

  Copyright (c) 2025, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#include "ixgbe_type.h"
#include "ixgbe_e610.h"
#include "ixgbe_x550.h"
#include "ixgbe_common.h"
#include "ixgbe_phy.h"
#include "ixgbe_api.h"

/**
 * ixgbe_init_aci - initialization routine for Admin Command Interface
 * @hw: pointer to the hardware structure
 *
 * Initialize the ACI lock.
 */
void ixgbe_init_aci(struct ixgbe_hw *hw)
{
	ixgbe_init_lock(&hw->aci.lock);
}

/**
 * ixgbe_shutdown_aci - shutdown routine for Admin Command Interface
 * @hw: pointer to the hardware structure
 *
 * Destroy the ACI lock.
 */
void ixgbe_shutdown_aci(struct ixgbe_hw *hw)
{
	ixgbe_destroy_lock(&hw->aci.lock);
}

/**
 * ixgbe_should_retry_aci_send_cmd_execute - decide if ACI command should
 * be resent
 * @opcode: ACI opcode
 *
 * Check if ACI command should be sent again depending on the provided opcode.
 *
 * Return: true if the sending command routine should be repeated,
 * otherwise false.
 */
static bool ixgbe_should_retry_aci_send_cmd_execute(u16 opcode)
{
	switch (opcode) {
	case ixgbe_aci_opc_disable_rxen:
	case ixgbe_aci_opc_get_phy_caps:
	case ixgbe_aci_opc_get_link_status:
	case ixgbe_aci_opc_get_link_topo:
		return true;
	}

	return false;
}

/**
 * ixgbe_aci_send_cmd_execute - execute sending FW Admin Command to FW Admin
 * Command Interface
 * @hw: pointer to the HW struct
 * @desc: descriptor describing the command
 * @buf: buffer to use for indirect commands (NULL for direct commands)
 * @buf_size: size of buffer for indirect commands (0 for direct commands)
 *
 * Admin Command is sent using CSR by setting descriptor and buffer in specific
 * registers.
 *
 * Return: the exit code of the operation.
 * * - IXGBE_SUCCESS - success.
 * * - IXGBE_ERR_ACI_DISABLED - CSR mechanism is not enabled.
 * * - IXGBE_ERR_ACI_BUSY - CSR mechanism is busy.
 * * - IXGBE_ERR_PARAM - buf_size is too big or
 * invalid argument buf or buf_size.
 * * - IXGBE_ERR_ACI_TIMEOUT - Admin Command X command timeout.
 * * - IXGBE_ERR_ACI_ERROR - Admin Command X invalid state of HICR register or
 * Admin Command failed because of bad opcode was returned or
 * Admin Command failed with error Y.
 */
static s32
ixgbe_aci_send_cmd_execute(struct ixgbe_hw *hw, struct ixgbe_aci_desc *desc,
			   void *buf, u16 buf_size)
{
	u32 hicr = 0, tmp_buf_size = 0, i = 0;
	u32 *raw_desc = (u32 *)desc;
	s32 status = IXGBE_SUCCESS;
	bool valid_buf = false;
	u32 *tmp_buf = NULL;
	u16 opcode = 0;

	do {
		hw->aci.last_status = IXGBE_ACI_RC_OK;

		/* It's necessary to check if mechanism is enabled */
		hicr = IXGBE_READ_REG(hw, PF_HICR);
		if (!(hicr & PF_HICR_EN)) {
			status = IXGBE_ERR_ACI_DISABLED;
			break;
		}
		if (hicr & PF_HICR_C) {
			hw->aci.last_status = IXGBE_ACI_RC_EBUSY;
			status = IXGBE_ERR_ACI_BUSY;
			break;
		}
		opcode = desc->opcode;

		if (buf_size > IXGBE_ACI_MAX_BUFFER_SIZE) {
			status = IXGBE_ERR_PARAM;
			break;
		}

		if (buf)
			desc->flags |= IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_BUF);

		/* Check if buf and buf_size are proper params */
		if (desc->flags & IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_BUF)) {
			if ((buf && buf_size == 0) ||
			    (buf == NULL && buf_size)) {
				status = IXGBE_ERR_PARAM;
				break;
			}
			if (buf && buf_size)
				valid_buf = true;
		}

		if (valid_buf == true) {
			if (buf_size % 4 == 0)
				tmp_buf_size = buf_size;
			else
				tmp_buf_size = (buf_size & (u16)(~0x03)) + 4;

			tmp_buf = (u32*)ixgbe_malloc(hw, tmp_buf_size);
			if (!tmp_buf)
				return IXGBE_ERR_OUT_OF_MEM;

			/* tmp_buf will be firstly filled with 0xFF and after
			 * that the content of buf will be written into it.
			 * This approach lets us use valid buf_size and
			 * prevents us from reading past buf area
			 * when buf_size mod 4 not equal to 0.
			 */
			memset(tmp_buf, 0xFF, tmp_buf_size);
			memcpy(tmp_buf, buf, buf_size);

			if (tmp_buf_size > IXGBE_ACI_LG_BUF)
				desc->flags |=
				IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_LB);

			desc->datalen = IXGBE_CPU_TO_LE16(buf_size);

			if (desc->flags & IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_RD)) {
				for (i = 0; i < tmp_buf_size / 4; i++) {
					IXGBE_WRITE_REG(hw, PF_HIBA(i),
						IXGBE_LE32_TO_CPU(tmp_buf[i]));
				}
			}
		}

		/* Descriptor is written to specific registers */
		for (i = 0; i < IXGBE_ACI_DESC_SIZE_IN_DWORDS; i++)
			IXGBE_WRITE_REG(hw, PF_HIDA(i),
					IXGBE_LE32_TO_CPU(raw_desc[i]));

		/* SW has to set PF_HICR.C bit and clear PF_HICR.SV and
		 * PF_HICR_EV
		 */
		hicr = IXGBE_READ_REG(hw, PF_HICR);
		hicr = (hicr | PF_HICR_C) & ~(PF_HICR_SV | PF_HICR_EV);
		IXGBE_WRITE_REG(hw, PF_HICR, hicr);

		/* Wait for sync Admin Command response */
		for (i = 0; i < IXGBE_ACI_SYNC_RESPONSE_TIMEOUT; i += 1) {
			hicr = IXGBE_READ_REG(hw, PF_HICR);
			if ((hicr & PF_HICR_SV) || !(hicr & PF_HICR_C))
				break;

			msec_delay(1);
		}

		/* Wait for async Admin Command response */
		if ((hicr & PF_HICR_SV) && (hicr & PF_HICR_C)) {
			for (i = 0; i < IXGBE_ACI_ASYNC_RESPONSE_TIMEOUT;
			     i += 1) {
				hicr = IXGBE_READ_REG(hw, PF_HICR);
				if ((hicr & PF_HICR_EV) || !(hicr & PF_HICR_C))
					break;

				msec_delay(1);
			}
		}

		/* Read sync Admin Command response */
		if ((hicr & PF_HICR_SV)) {
			for (i = 0; i < IXGBE_ACI_DESC_SIZE_IN_DWORDS; i++) {
				raw_desc[i] = IXGBE_READ_REG(hw, PF_HIDA(i));
				raw_desc[i] = IXGBE_CPU_TO_LE32(raw_desc[i]);
			}
		}

		/* Read async Admin Command response */
		if ((hicr & PF_HICR_EV) && !(hicr & PF_HICR_C)) {
			for (i = 0; i < IXGBE_ACI_DESC_SIZE_IN_DWORDS; i++) {
				raw_desc[i] = IXGBE_READ_REG(hw, PF_HIDA_2(i));
				raw_desc[i] = IXGBE_CPU_TO_LE32(raw_desc[i]);
			}
		}

		/* Handle timeout and invalid state of HICR register */
		if (hicr & PF_HICR_C) {
			status = IXGBE_ERR_ACI_TIMEOUT;
			break;
		} else if (!(hicr & PF_HICR_SV) && !(hicr & PF_HICR_EV)) {
			status = IXGBE_ERR_ACI_ERROR;
			break;
		}

		/* For every command other than 0x0014 treat opcode mismatch
		 * as an error. Response to 0x0014 command read from HIDA_2
		 * is a descriptor of an event which is expected to contain
		 * different opcode than the command.
		 */
		if (desc->opcode != opcode &&
		    opcode != IXGBE_CPU_TO_LE16(ixgbe_aci_opc_get_fw_event)) {
			status = IXGBE_ERR_ACI_ERROR;
			break;
		}

		if (desc->retval != IXGBE_ACI_RC_OK) {
			hw->aci.last_status = (enum ixgbe_aci_err)desc->retval;
			status = IXGBE_ERR_ACI_ERROR;
			break;
		}

		/* Write a response values to a buf */
		if (valid_buf && (desc->flags &
				  IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_BUF))) {
			for (i = 0; i < tmp_buf_size / 4; i++) {
				tmp_buf[i] = IXGBE_READ_REG(hw, PF_HIBA(i));
				tmp_buf[i] = IXGBE_CPU_TO_LE32(tmp_buf[i]);
			}
			memcpy(buf, tmp_buf, buf_size);
		}
	} while (0);

	if (tmp_buf)
		ixgbe_free(hw, tmp_buf);

	return status;
}

/**
 * ixgbe_aci_send_cmd - send FW Admin Command to FW Admin Command Interface
 * @hw: pointer to the HW struct
 * @desc: descriptor describing the command
 * @buf: buffer to use for indirect commands (NULL for direct commands)
 * @buf_size: size of buffer for indirect commands (0 for direct commands)
 *
 * Helper function to send FW Admin Commands to the FW Admin Command Interface.
 *
 * Retry sending the FW Admin Command multiple times to the FW ACI
 * if the EBUSY Admin Command error is returned.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_send_cmd(struct ixgbe_hw *hw, struct ixgbe_aci_desc *desc,
		       void *buf, u16 buf_size)
{
	struct ixgbe_aci_desc desc_cpy;
	enum ixgbe_aci_err last_status;
	bool is_cmd_for_retry;
	u8 *buf_cpy = NULL;
	s32 status;
	u16 opcode;
	u8 idx = 0;

	opcode = IXGBE_LE16_TO_CPU(desc->opcode);
	is_cmd_for_retry = ixgbe_should_retry_aci_send_cmd_execute(opcode);
	memset(&desc_cpy, 0, sizeof(desc_cpy));

	if (is_cmd_for_retry) {
		if (buf) {
			buf_cpy = (u8 *)ixgbe_malloc(hw, buf_size);
			if (!buf_cpy)
				return IXGBE_ERR_OUT_OF_MEM;
		}
		memcpy(&desc_cpy, desc, sizeof(desc_cpy));
	}

	do {
		ixgbe_acquire_lock(&hw->aci.lock);
		status = ixgbe_aci_send_cmd_execute(hw, desc, buf, buf_size);
		last_status = hw->aci.last_status;
		ixgbe_release_lock(&hw->aci.lock);

		if (!is_cmd_for_retry || status == IXGBE_SUCCESS ||
		    (last_status != IXGBE_ACI_RC_EBUSY && status != IXGBE_ERR_ACI_ERROR))
			break;

		if (buf)
			memcpy(buf, buf_cpy, buf_size);
		memcpy(desc, &desc_cpy, sizeof(desc_cpy));

		msec_delay(IXGBE_ACI_SEND_DELAY_TIME_MS);
	} while (++idx < IXGBE_ACI_SEND_MAX_EXECUTE);

	if (buf_cpy)
		ixgbe_free(hw, buf_cpy);

	return status;
}

/**
 * ixgbe_aci_check_event_pending - check if there are any pending events
 * @hw: pointer to the HW struct
 *
 * Determine if there are any pending events.
 *
 * Return: true if there are any currently pending events
 * otherwise false.
 */
bool ixgbe_aci_check_event_pending(struct ixgbe_hw *hw)
{
	u32 ep_bit_mask;
	u32 fwsts;

	ep_bit_mask = hw->bus.func ? GL_FWSTS_EP_PF1 : GL_FWSTS_EP_PF0;

	/* Check state of Event Pending (EP) bit */
	fwsts = IXGBE_READ_REG(hw, GL_FWSTS);
	return (fwsts & ep_bit_mask) ? true : false;
}

/**
 * ixgbe_aci_get_event - get an event from ACI
 * @hw: pointer to the HW struct
 * @e: event information structure
 * @pending: optional flag signaling that there are more pending events
 *
 * Obtain an event from ACI and return its content
 * through 'e' using ACI command (0x0014).
 * Provide information if there are more events
 * to retrieve through 'pending'.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_get_event(struct ixgbe_hw *hw, struct ixgbe_aci_event *e,
			bool *pending)
{
	struct ixgbe_aci_desc desc;
	s32 status;

	if (!e || (!e->msg_buf && e->buf_len) || (e->msg_buf && !e->buf_len))
		return IXGBE_ERR_PARAM;

	ixgbe_acquire_lock(&hw->aci.lock);

	/* Check if there are any events pending */
	if (!ixgbe_aci_check_event_pending(hw)) {
		status = IXGBE_ERR_ACI_NO_EVENTS;
		goto aci_get_event_exit;
	}

	/* Obtain pending event */
	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_fw_event);
	status = ixgbe_aci_send_cmd_execute(hw, &desc, e->msg_buf, e->buf_len);
	if (status)
		goto aci_get_event_exit;

	/* Returned 0x0014 opcode indicates that no event was obtained */
	if (desc.opcode == IXGBE_CPU_TO_LE16(ixgbe_aci_opc_get_fw_event)) {
		status = IXGBE_ERR_ACI_NO_EVENTS;
		goto aci_get_event_exit;
	}

	/* Determine size of event data */
	e->msg_len = MIN_T(u16, IXGBE_LE16_TO_CPU(desc.datalen), e->buf_len);
	/* Write event descriptor to event info structure */
	memcpy(&e->desc, &desc, sizeof(e->desc));

	/* Check if there are any further events pending */
	if (pending) {
		*pending = ixgbe_aci_check_event_pending(hw);
	}

aci_get_event_exit:
	ixgbe_release_lock(&hw->aci.lock);

	return status;
}

/**
 * ixgbe_fill_dflt_direct_cmd_desc - fill ACI descriptor with default values.
 * @desc: pointer to the temp descriptor (non DMA mem)
 * @opcode: the opcode can be used to decide which flags to turn off or on
 *
 * Helper function to fill the descriptor desc with default values
 * and the provided opcode.
 */
void ixgbe_fill_dflt_direct_cmd_desc(struct ixgbe_aci_desc *desc, u16 opcode)
{
	/* zero out the desc */
	memset(desc, 0, sizeof(*desc));
	desc->opcode = IXGBE_CPU_TO_LE16(opcode);
	desc->flags = IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_SI);
}

/**
 * ixgbe_aci_get_fw_ver - get the firmware version
 * @hw: pointer to the HW struct
 *
 * Get the firmware version using ACI command (0x0001).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_get_fw_ver(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_ver *resp;
	struct ixgbe_aci_desc desc;
	s32 status;

	resp = &desc.params.get_ver;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_ver);

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);

	if (!status) {
		hw->fw_branch = resp->fw_branch;
		hw->fw_maj_ver = resp->fw_major;
		hw->fw_min_ver = resp->fw_minor;
		hw->fw_patch = resp->fw_patch;
		hw->fw_build = IXGBE_LE32_TO_CPU(resp->fw_build);
		hw->api_branch = resp->api_branch;
		hw->api_maj_ver = resp->api_major;
		hw->api_min_ver = resp->api_minor;
		hw->api_patch = resp->api_patch;
	}

	return status;
}

/**
 * ixgbe_aci_send_driver_ver - send the driver version to firmware
 * @hw: pointer to the HW struct
 * @dv: driver's major, minor version
 *
 * Send the driver version to the firmware
 * using the ACI command (0x0002).
 *
 * Return: the exit code of the operation.
 * Returns IXGBE_ERR_PARAM, if dv is NULL.
 */
s32 ixgbe_aci_send_driver_ver(struct ixgbe_hw *hw, struct ixgbe_driver_ver *dv)
{
	struct ixgbe_aci_cmd_driver_ver *cmd;
	struct ixgbe_aci_desc desc;
	u16 len;

	cmd = &desc.params.driver_ver;

	if (!dv)
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_driver_ver);

	desc.flags |= IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_RD);
	cmd->major_ver = dv->major_ver;
	cmd->minor_ver = dv->minor_ver;
	cmd->build_ver = dv->build_ver;
	cmd->subbuild_ver = dv->subbuild_ver;

	len = 0;
	while (len < sizeof(dv->driver_string) &&
	       IS_ASCII(dv->driver_string[len]) && dv->driver_string[len])
		len++;

	return ixgbe_aci_send_cmd(hw, &desc, dv->driver_string, len);
}

/**
 * ixgbe_aci_req_res - request a common resource
 * @hw: pointer to the HW struct
 * @res: resource ID
 * @access: access type
 * @sdp_number: resource number
 * @timeout: the maximum time in ms that the driver may hold the resource
 *
 * Requests a common resource using the ACI command (0x0008).
 * Specifies the maximum time the driver may hold the resource.
 * If the requested resource is currently occupied by some other driver,
 * a busy return value is returned and the timeout field value indicates the
 * maximum time the current owner has to free it.
 *
 * Return: the exit code of the operation.
 */
static s32
ixgbe_aci_req_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res,
		  enum ixgbe_aci_res_access_type access, u8 sdp_number,
		  u32 *timeout)
{
	struct ixgbe_aci_cmd_req_res *cmd_resp;
	struct ixgbe_aci_desc desc;
	s32 status;

	cmd_resp = &desc.params.res_owner;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_req_res);

	cmd_resp->res_id = IXGBE_CPU_TO_LE16(res);
	cmd_resp->access_type = IXGBE_CPU_TO_LE16(access);
	cmd_resp->res_number = IXGBE_CPU_TO_LE32(sdp_number);
	cmd_resp->timeout = IXGBE_CPU_TO_LE32(*timeout);
	*timeout = 0;

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);

	/* The completion specifies the maximum time in ms that the driver
	 * may hold the resource in the Timeout field.
	 * If the resource is held by some other driver, the command completes
	 * with a busy return value and the timeout field indicates the maximum
	 * time the current owner of the resource has to free it.
	 */
	if (!status || hw->aci.last_status == IXGBE_ACI_RC_EBUSY)
		*timeout = IXGBE_LE32_TO_CPU(cmd_resp->timeout);

	return status;
}

/**
 * ixgbe_aci_release_res - release a common resource using ACI
 * @hw: pointer to the HW struct
 * @res: resource ID
 * @sdp_number: resource number
 *
 * Release a common resource using ACI command (0x0009).
 *
 * Return: the exit code of the operation.
 */
static s32
ixgbe_aci_release_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res,
		      u8 sdp_number)
{
	struct ixgbe_aci_cmd_req_res *cmd;
	struct ixgbe_aci_desc desc;

	cmd = &desc.params.res_owner;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_release_res);

	cmd->res_id = IXGBE_CPU_TO_LE16(res);
	cmd->res_number = IXGBE_CPU_TO_LE32(sdp_number);

	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_acquire_res - acquire the ownership of a resource
 * @hw: pointer to the HW structure
 * @res: resource ID
 * @access: access type (read or write)
 * @timeout: timeout in milliseconds
 *
 * Make an attempt to acquire the ownership of a resource using
 * the ixgbe_aci_req_res to utilize ACI.
 * In case if some other driver has previously acquired the resource and
 * performed any necessary updates, the IXGBE_ERR_ACI_NO_WORK is returned,
 * and the caller does not obtain the resource and has no further work to do.
 * If needed, the function will poll until the current lock owner timeouts.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_acquire_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res,
		      enum ixgbe_aci_res_access_type access, u32 timeout)
{
#define IXGBE_RES_POLLING_DELAY_MS	10
	u32 delay = IXGBE_RES_POLLING_DELAY_MS;
	u32 res_timeout = timeout;
	u32 retry_timeout = 0;
	s32 status;

	status = ixgbe_aci_req_res(hw, res, access, 0, &res_timeout);

	/* A return code of IXGBE_ERR_ACI_NO_WORK means that another driver has
	 * previously acquired the resource and performed any necessary updates;
	 * in this case the caller does not obtain the resource and has no
	 * further work to do.
	 */
	if (status == IXGBE_ERR_ACI_NO_WORK)
		goto ixgbe_acquire_res_exit;

	/* If necessary, poll until the current lock owner timeouts.
	 * Set retry_timeout to the timeout value reported by the FW in the
	 * response to the "Request Resource Ownership" (0x0008) Admin Command
	 * as it indicates the maximum time the current owner of the resource
	 * is allowed to hold it.
	 */
	retry_timeout = res_timeout;
	while (status && retry_timeout && res_timeout) {
		msec_delay(delay);
		retry_timeout = (retry_timeout > delay) ?
			retry_timeout - delay : 0;
		status = ixgbe_aci_req_res(hw, res, access, 0, &res_timeout);

		if (status == IXGBE_ERR_ACI_NO_WORK)
			/* lock free, but no work to do */
			break;

		if (!status)
			/* lock acquired */
			break;
	}

ixgbe_acquire_res_exit:
	return status;
}

/**
 * ixgbe_release_res - release a common resource
 * @hw: pointer to the HW structure
 * @res: resource ID
 *
 * Release a common resource using ixgbe_aci_release_res.
 */
void ixgbe_release_res(struct ixgbe_hw *hw, enum ixgbe_aci_res_ids res)
{
	u32 total_delay = 0;
	s32 status;

	status = ixgbe_aci_release_res(hw, res, 0);

	/* There are some rare cases when trying to release the resource
	 * results in an admin command timeout, so handle them correctly.
	 */
	while ((status == IXGBE_ERR_ACI_TIMEOUT) &&
	       (total_delay < IXGBE_ACI_RELEASE_RES_TIMEOUT)) {
		msec_delay(1);
		status = ixgbe_aci_release_res(hw, res, 0);
		total_delay++;
	}
}

/**
 * ixgbe_parse_common_caps - Parse common device/function capabilities
 * @hw: pointer to the HW struct
 * @caps: pointer to common capabilities structure
 * @elem: the capability element to parse
 * @prefix: message prefix for tracing capabilities
 *
 * Given a capability element, extract relevant details into the common
 * capability structure.
 *
 * Return: true if the capability matches one of the common capability ids,
 * false otherwise.
 */
static bool
ixgbe_parse_common_caps(struct ixgbe_hw *hw, struct ixgbe_hw_common_caps *caps,
			struct ixgbe_aci_cmd_list_caps_elem *elem,
			const char *prefix)
{
	u32 logical_id = IXGBE_LE32_TO_CPU(elem->logical_id);
	u32 phys_id = IXGBE_LE32_TO_CPU(elem->phys_id);
	u32 number = IXGBE_LE32_TO_CPU(elem->number);
	u16 cap = IXGBE_LE16_TO_CPU(elem->cap);
	bool found = true;

	UNREFERENCED_1PARAMETER(hw);

	switch (cap) {
	case IXGBE_ACI_CAPS_VALID_FUNCTIONS:
		caps->valid_functions = number;
		break;
	case IXGBE_ACI_CAPS_SRIOV:
		caps->sr_iov_1_1 = (number == 1);
		break;
	case IXGBE_ACI_CAPS_VMDQ:
		caps->vmdq = (number == 1);
		break;
	case IXGBE_ACI_CAPS_DCB:
		caps->dcb = (number == 1);
		caps->active_tc_bitmap = logical_id;
		caps->maxtc = phys_id;
		break;
	case IXGBE_ACI_CAPS_RSS:
		caps->rss_table_size = number;
		caps->rss_table_entry_width = logical_id;
		break;
	case IXGBE_ACI_CAPS_RXQS:
		caps->num_rxq = number;
		caps->rxq_first_id = phys_id;
		break;
	case IXGBE_ACI_CAPS_TXQS:
		caps->num_txq = number;
		caps->txq_first_id = phys_id;
		break;
	case IXGBE_ACI_CAPS_MSIX:
		caps->num_msix_vectors = number;
		caps->msix_vector_first_id = phys_id;
		break;
	case IXGBE_ACI_CAPS_NVM_VER:
		break;
	case IXGBE_ACI_CAPS_NVM_MGMT:
		caps->sec_rev_disabled =
			(number & IXGBE_NVM_MGMT_SEC_REV_DISABLED) ?
			true : false;
		caps->update_disabled =
			(number & IXGBE_NVM_MGMT_UPDATE_DISABLED) ?
			true : false;
		caps->nvm_unified_update =
			(number & IXGBE_NVM_MGMT_UNIFIED_UPD_SUPPORT) ?
			true : false;
		caps->netlist_auth =
			(number & IXGBE_NVM_MGMT_NETLIST_AUTH_SUPPORT) ?
			true : false;
		break;
	case IXGBE_ACI_CAPS_MAX_MTU:
		caps->max_mtu = number;
		break;
	case IXGBE_ACI_CAPS_PCIE_RESET_AVOIDANCE:
		caps->pcie_reset_avoidance = (number > 0);
		break;
	case IXGBE_ACI_CAPS_POST_UPDATE_RESET_RESTRICT:
		caps->reset_restrict_support = (number == 1);
		break;
	case IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG0:
	case IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG1:
	case IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG2:
	case IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG3:
	{
		u8 index = cap - IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG0;

		caps->ext_topo_dev_img_ver_high[index] = number;
		caps->ext_topo_dev_img_ver_low[index] = logical_id;
		caps->ext_topo_dev_img_part_num[index] =
			(phys_id & IXGBE_EXT_TOPO_DEV_IMG_PART_NUM_M) >>
			IXGBE_EXT_TOPO_DEV_IMG_PART_NUM_S;
		caps->ext_topo_dev_img_load_en[index] =
			(phys_id & IXGBE_EXT_TOPO_DEV_IMG_LOAD_EN) != 0;
		caps->ext_topo_dev_img_prog_en[index] =
			(phys_id & IXGBE_EXT_TOPO_DEV_IMG_PROG_EN) != 0;
		break;
	}
	case IXGBE_ACI_CAPS_OROM_RECOVERY_UPDATE:
		caps->orom_recovery_update = (number == 1);
		break;
	case IXGBE_ACI_CAPS_NEXT_CLUSTER_ID:
		caps->next_cluster_id_support = (number == 1);
		DEBUGOUT2("%s: next_cluster_id_support = %d\n",
			  prefix, caps->next_cluster_id_support);
		break;
	default:
		/* Not one of the recognized common capabilities */
		found = false;
	}

	return found;
}

/**
 * ixgbe_hweight8 - count set bits among the 8 lowest bits
 * @w: variable storing set bits to count
 *
 * Return: the number of set bits among the 8 lowest bits in the provided value.
 */
static u8 ixgbe_hweight8(u32 w)
{
	u8 hweight = 0, i;

	for (i = 0; i < 8; i++)
		if (w & (1 << i))
			hweight++;

	return hweight;
}

/**
 * ixgbe_hweight32 - count set bits among the 32 lowest bits
 * @w: variable storing set bits to count
 *
 * Return: the number of set bits among the 32 lowest bits in the
 * provided value.
 */
static u8 ixgbe_hweight32(u32 w)
{
	u32 bitMask = 0x1, i;
	u8  bitCnt = 0;

	for (i = 0; i < 32; i++)
	{
		if (w & bitMask)
			bitCnt++;

		bitMask = bitMask << 0x1;
	}

	return bitCnt;
}

/**
 * ixgbe_parse_valid_functions_cap - Parse IXGBE_ACI_CAPS_VALID_FUNCTIONS caps
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @cap: capability element to parse
 *
 * Parse IXGBE_ACI_CAPS_VALID_FUNCTIONS for device capabilities.
 */
static void
ixgbe_parse_valid_functions_cap(struct ixgbe_hw *hw,
				struct ixgbe_hw_dev_caps *dev_p,
				struct ixgbe_aci_cmd_list_caps_elem *cap)
{
	u32 number = IXGBE_LE32_TO_CPU(cap->number);

	UNREFERENCED_1PARAMETER(hw);

	dev_p->num_funcs = ixgbe_hweight32(number);
}

/**
 * ixgbe_parse_vf_dev_caps - Parse IXGBE_ACI_CAPS_VF device caps
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @cap: capability element to parse
 *
 * Parse IXGBE_ACI_CAPS_VF for device capabilities.
 */
static void ixgbe_parse_vf_dev_caps(struct ixgbe_hw *hw,
				    struct ixgbe_hw_dev_caps *dev_p,
				    struct ixgbe_aci_cmd_list_caps_elem *cap)
{
	u32 number = IXGBE_LE32_TO_CPU(cap->number);

	UNREFERENCED_1PARAMETER(hw);

	dev_p->num_vfs_exposed = number;
}

/**
 * ixgbe_parse_vsi_dev_caps - Parse IXGBE_ACI_CAPS_VSI device caps
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @cap: capability element to parse
 *
 * Parse IXGBE_ACI_CAPS_VSI for device capabilities.
 */
static void ixgbe_parse_vsi_dev_caps(struct ixgbe_hw *hw,
				     struct ixgbe_hw_dev_caps *dev_p,
				     struct ixgbe_aci_cmd_list_caps_elem *cap)
{
	u32 number = IXGBE_LE32_TO_CPU(cap->number);

	UNREFERENCED_1PARAMETER(hw);

	dev_p->num_vsi_allocd_to_host = number;
}

/**
 * ixgbe_parse_fdir_dev_caps - Parse IXGBE_ACI_CAPS_FD device caps
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @cap: capability element to parse
 *
 * Parse IXGBE_ACI_CAPS_FD for device capabilities.
 */
static void ixgbe_parse_fdir_dev_caps(struct ixgbe_hw *hw,
				      struct ixgbe_hw_dev_caps *dev_p,
				      struct ixgbe_aci_cmd_list_caps_elem *cap)
{
	u32 number = IXGBE_LE32_TO_CPU(cap->number);

	UNREFERENCED_1PARAMETER(hw);

	dev_p->num_flow_director_fltr = number;
}

/**
 * ixgbe_parse_dev_caps - Parse device capabilities
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @buf: buffer containing the device capability records
 * @cap_count: the number of capabilities
 *
 * Helper device to parse device (0x000B) capabilities list. For
 * capabilities shared between device and function, this relies on
 * ixgbe_parse_common_caps.
 *
 * Loop through the list of provided capabilities and extract the relevant
 * data into the device capabilities structured.
 */
static void ixgbe_parse_dev_caps(struct ixgbe_hw *hw,
				 struct ixgbe_hw_dev_caps *dev_p,
				 void *buf, u32 cap_count)
{
	struct ixgbe_aci_cmd_list_caps_elem *cap_resp;
	u32 i;

	cap_resp = (struct ixgbe_aci_cmd_list_caps_elem *)buf;

	memset(dev_p, 0, sizeof(*dev_p));

	for (i = 0; i < cap_count; i++) {
		u16 cap = IXGBE_LE16_TO_CPU(cap_resp[i].cap);
		bool found;

		found = ixgbe_parse_common_caps(hw, &dev_p->common_cap,
					      &cap_resp[i], "dev caps");

		switch (cap) {
		case IXGBE_ACI_CAPS_VALID_FUNCTIONS:
			ixgbe_parse_valid_functions_cap(hw, dev_p,
							&cap_resp[i]);
			break;
		case IXGBE_ACI_CAPS_VF:
			ixgbe_parse_vf_dev_caps(hw, dev_p, &cap_resp[i]);
			break;
		case IXGBE_ACI_CAPS_VSI:
			ixgbe_parse_vsi_dev_caps(hw, dev_p, &cap_resp[i]);
			break;
		case  IXGBE_ACI_CAPS_FD:
			ixgbe_parse_fdir_dev_caps(hw, dev_p, &cap_resp[i]);
			break;
		default:
			/* Don't list common capabilities as unknown */
			if (!found)
				break;
		}
	}

}

/**
 * ixgbe_parse_vf_func_caps - Parse IXGBE_ACI_CAPS_VF function caps
 * @hw: pointer to the HW struct
 * @func_p: pointer to function capabilities structure
 * @cap: pointer to the capability element to parse
 *
 * Extract function capabilities for IXGBE_ACI_CAPS_VF.
 */
static void ixgbe_parse_vf_func_caps(struct ixgbe_hw *hw,
				     struct ixgbe_hw_func_caps *func_p,
				     struct ixgbe_aci_cmd_list_caps_elem *cap)
{
	u32 logical_id = IXGBE_LE32_TO_CPU(cap->logical_id);
	u32 number = IXGBE_LE32_TO_CPU(cap->number);

	UNREFERENCED_1PARAMETER(hw);

	func_p->num_allocd_vfs = number;
	func_p->vf_base_id = logical_id;
}

/**
 * ixgbe_get_num_per_func - determine number of resources per PF
 * @hw: pointer to the HW structure
 * @max: value to be evenly split between each PF
 *
 * Determine the number of valid functions by going through the bitmap returned
 * from parsing capabilities and use this to calculate the number of resources
 * per PF based on the max value passed in.
 *
 * Return: the number of resources per PF or 0, if no PH are available.
 */
static u32 ixgbe_get_num_per_func(struct ixgbe_hw *hw, u32 max)
{
	u8 funcs;

#define IXGBE_CAPS_VALID_FUNCS_M	0xFF
	funcs = ixgbe_hweight8(hw->dev_caps.common_cap.valid_functions &
			     IXGBE_CAPS_VALID_FUNCS_M);

	if (!funcs)
		return 0;

	return max / funcs;
}

/**
 * ixgbe_parse_vsi_func_caps - Parse IXGBE_ACI_CAPS_VSI function caps
 * @hw: pointer to the HW struct
 * @func_p: pointer to function capabilities structure
 * @cap: pointer to the capability element to parse
 *
 * Extract function capabilities for IXGBE_ACI_CAPS_VSI.
 */
static void ixgbe_parse_vsi_func_caps(struct ixgbe_hw *hw,
				      struct ixgbe_hw_func_caps *func_p,
				      struct ixgbe_aci_cmd_list_caps_elem *cap)
{
	func_p->guar_num_vsi = ixgbe_get_num_per_func(hw, IXGBE_MAX_VSI);
}

/**
 * ixgbe_parse_func_caps - Parse function capabilities
 * @hw: pointer to the HW struct
 * @func_p: pointer to function capabilities structure
 * @buf: buffer containing the function capability records
 * @cap_count: the number of capabilities
 *
 * Helper function to parse function (0x000A) capabilities list. For
 * capabilities shared between device and function, this relies on
 * ixgbe_parse_common_caps.
 *
 * Loop through the list of provided capabilities and extract the relevant
 * data into the function capabilities structured.
 */
static void ixgbe_parse_func_caps(struct ixgbe_hw *hw,
				  struct ixgbe_hw_func_caps *func_p,
				  void *buf, u32 cap_count)
{
	struct ixgbe_aci_cmd_list_caps_elem *cap_resp;
	u32 i;

	cap_resp = (struct ixgbe_aci_cmd_list_caps_elem *)buf;

	memset(func_p, 0, sizeof(*func_p));

	for (i = 0; i < cap_count; i++) {
		u16 cap = IXGBE_LE16_TO_CPU(cap_resp[i].cap);
		ixgbe_parse_common_caps(hw, &func_p->common_cap,
					&cap_resp[i], "func caps");

		switch (cap) {
		case IXGBE_ACI_CAPS_VF:
			ixgbe_parse_vf_func_caps(hw, func_p, &cap_resp[i]);
			break;
		case IXGBE_ACI_CAPS_VSI:
			ixgbe_parse_vsi_func_caps(hw, func_p, &cap_resp[i]);
			break;
		default:
			/* Don't list common capabilities as unknown */
			break;
		}
	}

}

/**
 * ixgbe_aci_list_caps - query function/device capabilities
 * @hw: pointer to the HW struct
 * @buf: a buffer to hold the capabilities
 * @buf_size: size of the buffer
 * @cap_count: if not NULL, set to the number of capabilities reported
 * @opc: capabilities type to discover, device or function
 *
 * Get the function (0x000A) or device (0x000B) capabilities description from
 * firmware and store it in the buffer.
 *
 * If the cap_count pointer is not NULL, then it is set to the number of
 * capabilities firmware will report. Note that if the buffer size is too
 * small, it is possible the command will return IXGBE_ERR_OUT_OF_MEM. The
 * cap_count will still be updated in this case. It is recommended that the
 * buffer size be set to IXGBE_ACI_MAX_BUFFER_SIZE (the largest possible
 * buffer that firmware could return) to avoid this.
 *
 * Return: the exit code of the operation.
 * Exit code of IXGBE_ERR_OUT_OF_MEM means the buffer size is too small.
 */
s32 ixgbe_aci_list_caps(struct ixgbe_hw *hw, void *buf, u16 buf_size,
			u32 *cap_count, enum ixgbe_aci_opc opc)
{
	struct ixgbe_aci_cmd_list_caps *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	cmd = &desc.params.get_cap;

	if (opc != ixgbe_aci_opc_list_func_caps &&
	    opc != ixgbe_aci_opc_list_dev_caps)
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, opc);
	status = ixgbe_aci_send_cmd(hw, &desc, buf, buf_size);

	if (cap_count)
		*cap_count = IXGBE_LE32_TO_CPU(cmd->count);

	return status;
}

/**
 * ixgbe_discover_dev_caps - Read and extract device capabilities
 * @hw: pointer to the hardware structure
 * @dev_caps: pointer to device capabilities structure
 *
 * Read the device capabilities and extract them into the dev_caps structure
 * for later use.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_discover_dev_caps(struct ixgbe_hw *hw,
			    struct ixgbe_hw_dev_caps *dev_caps)
{
	u32 status, cap_count = 0;
	u8 *cbuf = NULL;

	cbuf = (u8*)ixgbe_malloc(hw, IXGBE_ACI_MAX_BUFFER_SIZE);
	if (!cbuf)
		return IXGBE_ERR_OUT_OF_MEM;
	/* Although the driver doesn't know the number of capabilities the
	 * device will return, we can simply send a 4KB buffer, the maximum
	 * possible size that firmware can return.
	 */
	cap_count = IXGBE_ACI_MAX_BUFFER_SIZE /
		    sizeof(struct ixgbe_aci_cmd_list_caps_elem);

	status = ixgbe_aci_list_caps(hw, cbuf, IXGBE_ACI_MAX_BUFFER_SIZE,
				     &cap_count,
				     ixgbe_aci_opc_list_dev_caps);
	if (!status)
		ixgbe_parse_dev_caps(hw, dev_caps, cbuf, cap_count);

	if (cbuf)
		ixgbe_free(hw, cbuf);

	return status;
}

/**
 * ixgbe_discover_func_caps - Read and extract function capabilities
 * @hw: pointer to the hardware structure
 * @func_caps: pointer to function capabilities structure
 *
 * Read the function capabilities and extract them into the func_caps structure
 * for later use.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_discover_func_caps(struct ixgbe_hw *hw,
			     struct ixgbe_hw_func_caps *func_caps)
{
	u32 cap_count = 0;
	u8 *cbuf = NULL;
	s32 status;

	cbuf = (u8*)ixgbe_malloc(hw, IXGBE_ACI_MAX_BUFFER_SIZE);
	if(!cbuf)
		return IXGBE_ERR_OUT_OF_MEM;
	/* Although the driver doesn't know the number of capabilities the
	 * device will return, we can simply send a 4KB buffer, the maximum
	 * possible size that firmware can return.
	 */
	cap_count = IXGBE_ACI_MAX_BUFFER_SIZE /
		    sizeof(struct ixgbe_aci_cmd_list_caps_elem);

	status = ixgbe_aci_list_caps(hw, cbuf, IXGBE_ACI_MAX_BUFFER_SIZE,
				     &cap_count,
				     ixgbe_aci_opc_list_func_caps);
	if (!status)
		ixgbe_parse_func_caps(hw, func_caps, cbuf, cap_count);

	if (cbuf)
		ixgbe_free(hw, cbuf);

	return status;
}

/**
 * ixgbe_get_caps - get info about the HW
 * @hw: pointer to the hardware structure
 *
 * Retrieve both device and function capabilities.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_get_caps(struct ixgbe_hw *hw)
{
	s32 status;

	status = ixgbe_discover_dev_caps(hw, &hw->dev_caps);
	if (status)
		return status;

	return ixgbe_discover_func_caps(hw, &hw->func_caps);
}

/**
 * ixgbe_aci_disable_rxen - disable RX
 * @hw: pointer to the HW struct
 *
 * Request a safe disable of Receive Enable using ACI command (0x000C).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_disable_rxen(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_disable_rxen *cmd;
	struct ixgbe_aci_desc desc;

	UNREFERENCED_1PARAMETER(hw);

	cmd = &desc.params.disable_rxen;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_disable_rxen);

	cmd->lport_num = (u8)hw->bus.func;

	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_aci_get_phy_caps - returns PHY capabilities
 * @hw: pointer to the HW struct
 * @qual_mods: report qualified modules
 * @report_mode: report mode capabilities
 * @pcaps: structure for PHY capabilities to be filled
 *
 * Returns the various PHY capabilities supported on the Port
 * using ACI command (0x0600).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_get_phy_caps(struct ixgbe_hw *hw, bool qual_mods, u8 report_mode,
			   struct ixgbe_aci_cmd_get_phy_caps_data *pcaps)
{
	struct ixgbe_aci_cmd_get_phy_caps *cmd;
	u16 pcaps_size = sizeof(*pcaps);
	struct ixgbe_aci_desc desc;
	s32 status;

	cmd = &desc.params.get_phy;

	if (!pcaps || (report_mode & ~IXGBE_ACI_REPORT_MODE_M))
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_phy_caps);

	if (qual_mods)
		cmd->param0 |= IXGBE_CPU_TO_LE16(IXGBE_ACI_GET_PHY_RQM);

	cmd->param0 |= IXGBE_CPU_TO_LE16(report_mode);
	status = ixgbe_aci_send_cmd(hw, &desc, pcaps, pcaps_size);

	if (status == IXGBE_SUCCESS &&
	    report_mode == IXGBE_ACI_REPORT_TOPO_CAP_MEDIA) {
		hw->phy.phy_type_low = IXGBE_LE64_TO_CPU(pcaps->phy_type_low);
		hw->phy.phy_type_high = IXGBE_LE64_TO_CPU(pcaps->phy_type_high);
		memcpy(hw->link.link_info.module_type, &pcaps->module_type,
			   sizeof(hw->link.link_info.module_type));
	}

	return status;
}

/**
 * ixgbe_phy_caps_equals_cfg - check if capabilities match the PHY config
 * @phy_caps: PHY capabilities
 * @phy_cfg: PHY configuration
 *
 * Helper function to determine if PHY capabilities match PHY
 * configuration
 *
 * Return: true if PHY capabilities match PHY configuration.
 */
bool
ixgbe_phy_caps_equals_cfg(struct ixgbe_aci_cmd_get_phy_caps_data *phy_caps,
			  struct ixgbe_aci_cmd_set_phy_cfg_data *phy_cfg)
{
	u8 caps_mask, cfg_mask;

	if (!phy_caps || !phy_cfg)
		return false;

	/* These bits are not common between capabilities and configuration.
	 * Do not use them to determine equality.
	 */
	caps_mask = IXGBE_ACI_PHY_CAPS_MASK & ~(IXGBE_ACI_PHY_AN_MODE |
					      IXGBE_ACI_PHY_EN_MOD_QUAL);
	cfg_mask = IXGBE_ACI_PHY_ENA_VALID_MASK &
		   ~IXGBE_ACI_PHY_ENA_AUTO_LINK_UPDT;

	if (phy_caps->phy_type_low != phy_cfg->phy_type_low ||
	    phy_caps->phy_type_high != phy_cfg->phy_type_high ||
	    ((phy_caps->caps & caps_mask) != (phy_cfg->caps & cfg_mask)) ||
	    phy_caps->low_power_ctrl_an != phy_cfg->low_power_ctrl_an ||
	    phy_caps->eee_cap != phy_cfg->eee_cap ||
	    phy_caps->eeer_value != phy_cfg->eeer_value ||
	    phy_caps->link_fec_options != phy_cfg->link_fec_opt)
		return false;

	return true;
}

/**
 * ixgbe_copy_phy_caps_to_cfg - Copy PHY ability data to configuration data
 * @caps: PHY ability structure to copy data from
 * @cfg: PHY configuration structure to copy data to
 *
 * Helper function to copy data from PHY capabilities data structure
 * to PHY configuration data structure
 */
void ixgbe_copy_phy_caps_to_cfg(struct ixgbe_aci_cmd_get_phy_caps_data *caps,
				struct ixgbe_aci_cmd_set_phy_cfg_data *cfg)
{
	if (!caps || !cfg)
		return;

	memset(cfg, 0, sizeof(*cfg));
	cfg->phy_type_low = caps->phy_type_low;
	cfg->phy_type_high = caps->phy_type_high;
	cfg->caps = caps->caps;
	cfg->low_power_ctrl_an = caps->low_power_ctrl_an;
	cfg->eee_cap = caps->eee_cap;
	cfg->eeer_value = caps->eeer_value;
	cfg->link_fec_opt = caps->link_fec_options;
	cfg->module_compliance_enforcement =
		caps->module_compliance_enforcement;
}

/**
 * ixgbe_aci_set_phy_cfg - set PHY configuration
 * @hw: pointer to the HW struct
 * @cfg: structure with PHY configuration data to be set
 *
 * Set the various PHY configuration parameters supported on the Port
 * using ACI command (0x0601).
 * One or more of the Set PHY config parameters may be ignored in an MFP
 * mode as the PF may not have the privilege to set some of the PHY Config
 * parameters.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_set_phy_cfg(struct ixgbe_hw *hw,
			  struct ixgbe_aci_cmd_set_phy_cfg_data *cfg)
{
	struct ixgbe_aci_desc desc;
	s32 status;

	if (!cfg)
		return IXGBE_ERR_PARAM;

	/* Ensure that only valid bits of cfg->caps can be turned on. */
	if (cfg->caps & ~IXGBE_ACI_PHY_ENA_VALID_MASK) {
		cfg->caps &= IXGBE_ACI_PHY_ENA_VALID_MASK;
	}

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_set_phy_cfg);
	desc.flags |= IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_RD);

	status = ixgbe_aci_send_cmd(hw, &desc, cfg, sizeof(*cfg));

	if (!status)
		hw->phy.curr_user_phy_cfg = *cfg;

	return status;
}

/**
 * ixgbe_aci_set_link_restart_an - set up link and restart AN
 * @hw: pointer to the HW struct
 * @ena_link: if true: enable link, if false: disable link
 *
 * Function sets up the link and restarts the Auto-Negotiation over the link.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_set_link_restart_an(struct ixgbe_hw *hw, bool ena_link)
{
	struct ixgbe_aci_cmd_restart_an *cmd;
	struct ixgbe_aci_desc desc;

	cmd = &desc.params.restart_an;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_restart_an);

	cmd->cmd_flags = IXGBE_ACI_RESTART_AN_LINK_RESTART;
	if (ena_link)
		cmd->cmd_flags |= IXGBE_ACI_RESTART_AN_LINK_ENABLE;
	else
		cmd->cmd_flags &= ~IXGBE_ACI_RESTART_AN_LINK_ENABLE;

	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_get_media_type_from_phy_type - Gets media type based on phy type
 * @hw: pointer to the HW struct
 *
 * Try to identify the media type based on the phy type.
 * If more than one media type, the ixgbe_media_type_unknown is returned.
 * First, phy_type_low is checked, then phy_type_high.
 * If none are identified, the ixgbe_media_type_unknown is returned
 *
 * Return: type of a media based on phy type in form of enum.
 */
static enum ixgbe_media_type
ixgbe_get_media_type_from_phy_type(struct ixgbe_hw *hw)
{
	struct ixgbe_link_status *hw_link_info;

	if (!hw)
		return ixgbe_media_type_unknown;

	hw_link_info = &hw->link.link_info;
	if (hw_link_info->phy_type_low && hw_link_info->phy_type_high)
		/* If more than one media type is selected, report unknown */
		return ixgbe_media_type_unknown;

	if (hw_link_info->phy_type_low) {
		/* 1G SGMII is a special case where some DA cable PHYs
		 * may show this as an option when it really shouldn't
		 * be since SGMII is meant to be between a MAC and a PHY
		 * in a backplane. Try to detect this case and handle it
		 */
		if (hw_link_info->phy_type_low == IXGBE_PHY_TYPE_LOW_1G_SGMII &&
		    (hw_link_info->module_type[IXGBE_ACI_MOD_TYPE_IDENT] ==
		    IXGBE_ACI_MOD_TYPE_BYTE1_SFP_PLUS_CU_ACTIVE ||
		    hw_link_info->module_type[IXGBE_ACI_MOD_TYPE_IDENT] ==
		    IXGBE_ACI_MOD_TYPE_BYTE1_SFP_PLUS_CU_PASSIVE))
			return ixgbe_media_type_da;

		switch (hw_link_info->phy_type_low) {
		case IXGBE_PHY_TYPE_LOW_1000BASE_SX:
		case IXGBE_PHY_TYPE_LOW_1000BASE_LX:
		case IXGBE_PHY_TYPE_LOW_10GBASE_SR:
		case IXGBE_PHY_TYPE_LOW_10GBASE_LR:
			return ixgbe_media_type_fiber;
		case IXGBE_PHY_TYPE_LOW_10G_SFI_AOC_ACC:
			return ixgbe_media_type_fiber;
		case IXGBE_PHY_TYPE_LOW_100BASE_TX:
		case IXGBE_PHY_TYPE_LOW_1000BASE_T:
		case IXGBE_PHY_TYPE_LOW_2500BASE_T:
		case IXGBE_PHY_TYPE_LOW_5GBASE_T:
		case IXGBE_PHY_TYPE_LOW_10GBASE_T:
			return ixgbe_media_type_copper;
		case IXGBE_PHY_TYPE_LOW_10G_SFI_DA:
			return ixgbe_media_type_da;
		case IXGBE_PHY_TYPE_LOW_1000BASE_KX:
		case IXGBE_PHY_TYPE_LOW_2500BASE_KX:
		case IXGBE_PHY_TYPE_LOW_2500BASE_X:
		case IXGBE_PHY_TYPE_LOW_5GBASE_KR:
		case IXGBE_PHY_TYPE_LOW_10GBASE_KR_CR1:
		case IXGBE_PHY_TYPE_LOW_10G_SFI_C2C:
			return ixgbe_media_type_backplane;
		}
	} else {
		switch (hw_link_info->phy_type_high) {
		case IXGBE_PHY_TYPE_HIGH_10BASE_T:
			return ixgbe_media_type_copper;
		}
	}
	return ixgbe_media_type_unknown;
}

/**
 * ixgbe_update_link_info - update status of the HW network link
 * @hw: pointer to the HW struct
 *
 * Update the status of the HW network link.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_update_link_info(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_phy_caps_data *pcaps;
	struct ixgbe_link_status *li;
	s32 status;

	if (!hw)
		return IXGBE_ERR_PARAM;

	li = &hw->link.link_info;

	status = ixgbe_aci_get_link_info(hw, true, NULL);
	if (status)
		return status;

	if (li->link_info & IXGBE_ACI_MEDIA_AVAILABLE) {
		pcaps = (struct ixgbe_aci_cmd_get_phy_caps_data *)
			ixgbe_malloc(hw, sizeof(*pcaps));
		if (!pcaps)
			return IXGBE_ERR_OUT_OF_MEM;

		status = ixgbe_aci_get_phy_caps(hw, false,
						IXGBE_ACI_REPORT_TOPO_CAP_MEDIA,
						pcaps);

		if (status == IXGBE_SUCCESS)
			memcpy(li->module_type, &pcaps->module_type,
			       sizeof(li->module_type));

		ixgbe_free(hw, pcaps);
	}

	return status;
}

/**
 * ixgbe_get_link_status - get status of the HW network link
 * @hw: pointer to the HW struct
 * @link_up: pointer to bool (true/false = linkup/linkdown)
 *
 * Variable link_up is true if link is up, false if link is down.
 * The variable link_up is invalid if status is non zero. As a
 * result of this call, link status reporting becomes enabled
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_get_link_status(struct ixgbe_hw *hw, bool *link_up)
{
	s32 status = IXGBE_SUCCESS;

	if (!hw || !link_up)
		return IXGBE_ERR_PARAM;

	if (hw->link.get_link_info) {
		status = ixgbe_update_link_info(hw);
		if (status) {
			return status;
		}
	}

	*link_up = hw->link.link_info.link_info & IXGBE_ACI_LINK_UP;

	return status;
}

/**
 * ixgbe_aci_get_link_info - get the link status
 * @hw: pointer to the HW struct
 * @ena_lse: enable/disable LinkStatusEvent reporting
 * @link: pointer to link status structure - optional
 *
 * Get the current Link Status using ACI command (0x607).
 * The current link can be optionally provided to update
 * the status.
 *
 * Return: the link status of the adapter.
 */
s32 ixgbe_aci_get_link_info(struct ixgbe_hw *hw, bool ena_lse,
			    struct ixgbe_link_status *link)
{
	struct ixgbe_aci_cmd_get_link_status_data link_data = { 0 };
	struct ixgbe_aci_cmd_get_link_status *resp;
	struct ixgbe_link_status *li_old, *li;
	struct ixgbe_fc_info *hw_fc_info;
	struct ixgbe_aci_desc desc;
	bool tx_pause, rx_pause;
	u8 cmd_flags;
	s32 status;

	if (!hw)
		return IXGBE_ERR_PARAM;

	li_old = &hw->link.link_info_old;
	li = &hw->link.link_info;
	hw_fc_info = &hw->fc;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_link_status);
	cmd_flags = (ena_lse) ? IXGBE_ACI_LSE_ENA : IXGBE_ACI_LSE_DIS;
	resp = &desc.params.get_link_status;
	resp->cmd_flags = cmd_flags;

	status = ixgbe_aci_send_cmd(hw, &desc, &link_data, sizeof(link_data));

	if (status != IXGBE_SUCCESS)
		return status;

	/* save off old link status information */
	*li_old = *li;

	/* update current link status information */
	li->link_speed = IXGBE_LE16_TO_CPU(link_data.link_speed);
	li->phy_type_low = IXGBE_LE64_TO_CPU(link_data.phy_type_low);
	li->phy_type_high = IXGBE_LE64_TO_CPU(link_data.phy_type_high);
	li->link_info = link_data.link_info;
	li->link_cfg_err = link_data.link_cfg_err;
	li->an_info = link_data.an_info;
	li->ext_info = link_data.ext_info;
	li->max_frame_size = IXGBE_LE16_TO_CPU(link_data.max_frame_size);
	li->fec_info = link_data.cfg & IXGBE_ACI_FEC_MASK;
	li->topo_media_conflict = link_data.topo_media_conflict;
	li->pacing = link_data.cfg & (IXGBE_ACI_CFG_PACING_M |
				      IXGBE_ACI_CFG_PACING_TYPE_M);

	/* update fc info */
	tx_pause = !!(link_data.an_info & IXGBE_ACI_LINK_PAUSE_TX);
	rx_pause = !!(link_data.an_info & IXGBE_ACI_LINK_PAUSE_RX);
	if (tx_pause && rx_pause)
		hw_fc_info->current_mode = ixgbe_fc_full;
	else if (tx_pause)
		hw_fc_info->current_mode = ixgbe_fc_tx_pause;
	else if (rx_pause)
		hw_fc_info->current_mode = ixgbe_fc_rx_pause;
	else
		hw_fc_info->current_mode = ixgbe_fc_none;

	li->lse_ena = !!(resp->cmd_flags & IXGBE_ACI_LSE_IS_ENABLED);

	/* save link status information */
	if (link)
		*link = *li;

	/* flag cleared so calling functions don't call AQ again */
	hw->link.get_link_info = false;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_aci_set_event_mask - set event mask
 * @hw: pointer to the HW struct
 * @port_num: port number of the physical function
 * @mask: event mask to be set
 *
 * Set the event mask using ACI command (0x0613).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_set_event_mask(struct ixgbe_hw *hw, u8 port_num, u16 mask)
{
	struct ixgbe_aci_cmd_set_event_mask *cmd;
	struct ixgbe_aci_desc desc;

	cmd = &desc.params.set_event_mask;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_set_event_mask);

	cmd->event_mask = IXGBE_CPU_TO_LE16(mask);
	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_configure_lse - enable/disable link status events
 * @hw: pointer to the HW struct
 * @activate: bool value deciding if lse should be enabled nor disabled
 * @mask: event mask to be set; a set bit means deactivation of the
 * corresponding event
 *
 * Set the event mask and then enable or disable link status events
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_configure_lse(struct ixgbe_hw *hw, bool activate, u16 mask)
{
	s32 rc;

	rc = ixgbe_aci_set_event_mask(hw, (u8)hw->bus.func, mask);
	if (rc) {
		return rc;
	}

	/* Enabling link status events generation by fw */
	rc = ixgbe_aci_get_link_info(hw, activate, NULL);
	if (rc) {
		return rc;
	}
	return IXGBE_SUCCESS;
}

/**
 * ixgbe_aci_get_netlist_node - get a node handle
 * @hw: pointer to the hw struct
 * @cmd: get_link_topo AQ structure
 * @node_part_number: output node part number if node found
 * @node_handle: output node handle parameter if node found
 *
 * Get the netlist node and assigns it to
 * the provided handle using ACI command (0x06E0).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_get_netlist_node(struct ixgbe_hw *hw,
			       struct ixgbe_aci_cmd_get_link_topo *cmd,
			       u8 *node_part_number, u16 *node_handle)
{
	struct ixgbe_aci_desc desc;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_link_topo);
	desc.params.get_link_topo = *cmd;

	if (ixgbe_aci_send_cmd(hw, &desc, NULL, 0))
		return IXGBE_ERR_NOT_SUPPORTED;

	if (node_handle)
		*node_handle =
			IXGBE_LE16_TO_CPU(desc.params.get_link_topo.addr.handle);
	if (node_part_number)
		*node_part_number = desc.params.get_link_topo.node_part_num;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_find_netlist_node - find a node handle
 * @hw: pointer to the hw struct
 * @node_type_ctx: type of netlist node to look for
 * @node_part_number: node part number to look for
 * @node_handle: output parameter if node found - optional
 *
 * Find and return the node handle for a given node type and part number in the
 * netlist. When found IXGBE_SUCCESS is returned, IXGBE_ERR_NOT_SUPPORTED
 * otherwise. If @node_handle provided, it would be set to found node handle.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_find_netlist_node(struct ixgbe_hw *hw, u8 node_type_ctx,
			    u8 node_part_number, u16 *node_handle)
{
	struct ixgbe_aci_cmd_get_link_topo cmd;
	u8 rec_node_part_number;
	u16 rec_node_handle;
	s32 status;
	u8 idx;

	for (idx = 0; idx < IXGBE_MAX_NETLIST_SIZE; idx++) {
		memset(&cmd, 0, sizeof(cmd));

		cmd.addr.topo_params.node_type_ctx =
			(node_type_ctx << IXGBE_ACI_LINK_TOPO_NODE_TYPE_S);
		cmd.addr.topo_params.index = idx;

		status = ixgbe_aci_get_netlist_node(hw, &cmd,
						    &rec_node_part_number,
						    &rec_node_handle);
		if (status)
			return status;

		if (rec_node_part_number == node_part_number) {
			if (node_handle)
				*node_handle = rec_node_handle;
			return IXGBE_SUCCESS;
		}
	}

	return IXGBE_ERR_NOT_SUPPORTED;
}

/**
 * ixgbe_aci_read_i2c - read I2C register value
 * @hw: pointer to the hw struct
 * @topo_addr: topology address for a device to communicate with
 * @bus_addr: 7-bit I2C bus address
 * @addr: I2C memory address (I2C offset) with up to 16 bits
 * @params: I2C parameters: bit [7] - Repeated start,
 *				      bits [6:5] data offset size,
 *			    bit [4] - I2C address type, bits [3:0] - data size
 *				      to read (0-16 bytes)
 * @data: pointer to data (0 to 16 bytes) to be read from the I2C device
 *
 * Read the value of the I2C pin register using ACI command (0x06E2).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_read_i2c(struct ixgbe_hw *hw,
		       struct ixgbe_aci_cmd_link_topo_addr topo_addr,
		       u16 bus_addr, __le16 addr, u8 params, u8 *data)
{
	struct ixgbe_aci_desc desc = { 0 };
	struct ixgbe_aci_cmd_i2c *cmd;
	u8 data_size;
	s32 status;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_read_i2c);
	cmd = &desc.params.read_write_i2c;

	if (!data)
		return IXGBE_ERR_PARAM;

	data_size = (params & IXGBE_ACI_I2C_DATA_SIZE_M) >>
		    IXGBE_ACI_I2C_DATA_SIZE_S;

	cmd->i2c_bus_addr = IXGBE_CPU_TO_LE16(bus_addr);
	cmd->topo_addr = topo_addr;
	cmd->i2c_params = params;
	cmd->i2c_addr = addr;

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
	if (!status) {
		struct ixgbe_aci_cmd_read_i2c_resp *resp;
		u8 i;

		resp = &desc.params.read_i2c_resp;
		for (i = 0; i < data_size; i++) {
			*data = resp->i2c_data[i];
			data++;
		}
	}

	return status;
}

/**
 * ixgbe_aci_write_i2c - write a value to I2C register
 * @hw: pointer to the hw struct
 * @topo_addr: topology address for a device to communicate with
 * @bus_addr: 7-bit I2C bus address
 * @addr: I2C memory address (I2C offset) with up to 16 bits
 * @params: I2C parameters: bit [4] - I2C address type, bits [3:0] - data size
 *				      to write (0-7 bytes)
 * @data: pointer to data (0 to 4 bytes) to be written to the I2C device
 *
 * Write a value to the I2C pin register using ACI command (0x06E3).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_write_i2c(struct ixgbe_hw *hw,
			struct ixgbe_aci_cmd_link_topo_addr topo_addr,
			u16 bus_addr, __le16 addr, u8 params, u8 *data)
{
	struct ixgbe_aci_desc desc = { 0 };
	struct ixgbe_aci_cmd_i2c *cmd;
	u8 i, data_size;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_write_i2c);
	cmd = &desc.params.read_write_i2c;

	data_size = (params & IXGBE_ACI_I2C_DATA_SIZE_M) >>
		    IXGBE_ACI_I2C_DATA_SIZE_S;

	/* data_size limited to 4 */
	if (data_size > 4)
		return IXGBE_ERR_PARAM;

	cmd->i2c_bus_addr = IXGBE_CPU_TO_LE16(bus_addr);
	cmd->topo_addr = topo_addr;
	cmd->i2c_params = params;
	cmd->i2c_addr = addr;

	for (i = 0; i < data_size; i++) {
		cmd->i2c_data[i] = *data;
		data++;
	}

	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_aci_set_port_id_led - set LED value for the given port
 * @hw: pointer to the HW struct
 * @orig_mode: set LED original mode
 *
 * Set LED value for the given port (0x06E9)
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_set_port_id_led(struct ixgbe_hw *hw, bool orig_mode)
{
	struct ixgbe_aci_cmd_set_port_id_led *cmd;
	struct ixgbe_aci_desc desc;

	cmd = &desc.params.set_port_id_led;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_set_port_id_led);

	cmd->lport_num = (u8)hw->bus.func;
	cmd->lport_num_valid = IXGBE_ACI_PORT_ID_PORT_NUM_VALID;

	if (orig_mode)
		cmd->ident_mode = IXGBE_ACI_PORT_IDENT_LED_ORIG;
	else
		cmd->ident_mode = IXGBE_ACI_PORT_IDENT_LED_BLINK;

	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_aci_set_gpio - set GPIO pin state
 * @hw: pointer to the hw struct
 * @gpio_ctrl_handle: GPIO controller node handle
 * @pin_idx: IO Number of the GPIO that needs to be set
 * @value: SW provide IO value to set in the LSB
 *
 * Set the GPIO pin state that is a part of the topology
 * using ACI command (0x06EC).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_set_gpio(struct ixgbe_hw *hw, u16 gpio_ctrl_handle, u8 pin_idx,
		       bool value)
{
	struct ixgbe_aci_cmd_gpio *cmd;
	struct ixgbe_aci_desc desc;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_set_gpio);
	cmd = &desc.params.read_write_gpio;
	cmd->gpio_ctrl_handle = IXGBE_CPU_TO_LE16(gpio_ctrl_handle);
	cmd->gpio_num = pin_idx;
	cmd->gpio_val = value ? 1 : 0;

	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_aci_get_gpio - get GPIO pin state
 * @hw: pointer to the hw struct
 * @gpio_ctrl_handle: GPIO controller node handle
 * @pin_idx: IO Number of the GPIO that needs to be set
 * @value: IO value read
 *
 * Get the value of a GPIO signal which is part of the topology
 * using ACI command (0x06ED).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_get_gpio(struct ixgbe_hw *hw, u16 gpio_ctrl_handle, u8 pin_idx,
		       bool *value)
{
	struct ixgbe_aci_cmd_gpio *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_gpio);
	cmd = &desc.params.read_write_gpio;
	cmd->gpio_ctrl_handle = IXGBE_CPU_TO_LE16(gpio_ctrl_handle);
	cmd->gpio_num = pin_idx;

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
	if (status)
		return status;

	*value = !!cmd->gpio_val;
	return IXGBE_SUCCESS;
}

/**
 * ixgbe_aci_sff_eeprom - read/write SFF EEPROM
 * @hw: pointer to the HW struct
 * @lport: bits [7:0] = logical port, bit [8] = logical port valid
 * @bus_addr: I2C bus address of the eeprom (typically 0xA0, 0=topo default)
 * @mem_addr: I2C offset. lower 8 bits for address, 8 upper bits zero padding.
 * @page: QSFP page
 * @page_bank_ctrl: configuration of SFF/CMIS paging and banking control
 * @data: pointer to data buffer to be read/written to the I2C device.
 * @length: 1-16 for read, 1 for write.
 * @write: 0 read, 1 for write.
 *
 * Read/write SFF EEPROM using ACI command (0x06EE).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_sff_eeprom(struct ixgbe_hw *hw, u16 lport, u8 bus_addr,
			 u16 mem_addr, u8 page, u8 page_bank_ctrl, u8 *data,
			 u8 length, bool write)
{
	struct ixgbe_aci_cmd_sff_eeprom *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	if (!data || (mem_addr & 0xff00))
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_sff_eeprom);
	cmd = &desc.params.read_write_sff_param;
	desc.flags = IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_RD);
	cmd->lport_num = (u8)(lport & 0xff);
	cmd->lport_num_valid = (u8)((lport >> 8) & 0x01);
	cmd->i2c_bus_addr = IXGBE_CPU_TO_LE16(((bus_addr >> 1) &
					 IXGBE_ACI_SFF_I2CBUS_7BIT_M) |
					((page_bank_ctrl <<
					  IXGBE_ACI_SFF_PAGE_BANK_CTRL_S) &
					 IXGBE_ACI_SFF_PAGE_BANK_CTRL_M));
	cmd->i2c_offset = IXGBE_CPU_TO_LE16(mem_addr & 0xff);
	cmd->module_page = page;
	if (write)
		cmd->i2c_bus_addr |= IXGBE_CPU_TO_LE16(IXGBE_ACI_SFF_IS_WRITE);

	status = ixgbe_aci_send_cmd(hw, &desc, data, length);
	return status;
}

/**
 * ixgbe_aci_prog_topo_dev_nvm - program Topology Device NVM
 * @hw: pointer to the hardware structure
 * @topo_params: pointer to structure storing topology parameters for a device
 *
 * Program Topology Device NVM using ACI command (0x06F2).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_prog_topo_dev_nvm(struct ixgbe_hw *hw,
			struct ixgbe_aci_cmd_link_topo_params *topo_params)
{
	struct ixgbe_aci_cmd_prog_topo_dev_nvm *cmd;
	struct ixgbe_aci_desc desc;

	cmd = &desc.params.prog_topo_dev_nvm;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_prog_topo_dev_nvm);

	memcpy(&cmd->topo_params, topo_params, sizeof(*topo_params));

	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_aci_read_topo_dev_nvm - read Topology Device NVM
 * @hw: pointer to the hardware structure
 * @topo_params: pointer to structure storing topology parameters for a device
 * @start_address: byte offset in the topology device NVM
 * @data: pointer to data buffer
 * @data_size: number of bytes to be read from the topology device NVM
 * Read Topology Device NVM (0x06F3)
 *
 * Read Topology of Device NVM using ACI command (0x06F3).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_read_topo_dev_nvm(struct ixgbe_hw *hw,
			struct ixgbe_aci_cmd_link_topo_params *topo_params,
			u32 start_address, u8 *data, u8 data_size)
{
	struct ixgbe_aci_cmd_read_topo_dev_nvm *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	if (!data || data_size == 0 ||
	    data_size > IXGBE_ACI_READ_TOPO_DEV_NVM_DATA_READ_SIZE)
		return IXGBE_ERR_PARAM;

	cmd = &desc.params.read_topo_dev_nvm;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_read_topo_dev_nvm);

	desc.datalen = IXGBE_CPU_TO_LE16(data_size);
	memcpy(&cmd->topo_params, topo_params, sizeof(*topo_params));
	cmd->start_address = IXGBE_CPU_TO_LE32(start_address);

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
	if (status)
		return status;

	memcpy(data, cmd->data_read, data_size);

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_acquire_nvm - Generic request for acquiring the NVM ownership
 * @hw: pointer to the HW structure
 * @access: NVM access type (read or write)
 *
 * Request NVM ownership.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_acquire_nvm(struct ixgbe_hw *hw,
		      enum ixgbe_aci_res_access_type access)
{
	u32 fla;

	/* Skip if we are in blank NVM programming mode */
	fla = IXGBE_READ_REG(hw, GLNVM_FLA);
	if ((fla & GLNVM_FLA_LOCKED_M) == 0)
		return IXGBE_SUCCESS;

	return ixgbe_acquire_res(hw, IXGBE_NVM_RES_ID, access,
				 IXGBE_NVM_TIMEOUT);
}

/**
 * ixgbe_release_nvm - Generic request for releasing the NVM ownership
 * @hw: pointer to the HW structure
 *
 * Release NVM ownership.
 */
void ixgbe_release_nvm(struct ixgbe_hw *hw)
{
	u32 fla;

	/* Skip if we are in blank NVM programming mode */
	fla = IXGBE_READ_REG(hw, GLNVM_FLA);
	if ((fla & GLNVM_FLA_LOCKED_M) == 0)
		return;

	ixgbe_release_res(hw, IXGBE_NVM_RES_ID);
}


/**
 * ixgbe_aci_read_nvm - read NVM
 * @hw: pointer to the HW struct
 * @module_typeid: module pointer location in words from the NVM beginning
 * @offset: byte offset from the module beginning
 * @length: length of the section to be read (in bytes from the offset)
 * @data: command buffer (size [bytes] = length)
 * @last_command: tells if this is the last command in a series
 * @read_shadow_ram: tell if this is a shadow RAM read
 *
 * Read the NVM using ACI command (0x0701).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_read_nvm(struct ixgbe_hw *hw, u16 module_typeid, u32 offset,
		       u16 length, void *data, bool last_command,
		       bool read_shadow_ram)
{
	struct ixgbe_aci_desc desc;
	struct ixgbe_aci_cmd_nvm *cmd;

	cmd = &desc.params.nvm;

	if (offset > IXGBE_ACI_NVM_MAX_OFFSET)
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_nvm_read);

	if (!read_shadow_ram && module_typeid == IXGBE_ACI_NVM_START_POINT)
		cmd->cmd_flags |= IXGBE_ACI_NVM_FLASH_ONLY;

	/* If this is the last command in a series, set the proper flag. */
	if (last_command)
		cmd->cmd_flags |= IXGBE_ACI_NVM_LAST_CMD;
	cmd->module_typeid = IXGBE_CPU_TO_LE16(module_typeid);
	cmd->offset_low = IXGBE_CPU_TO_LE16(offset & 0xFFFF);
	cmd->offset_high = (offset >> 16) & 0xFF;
	cmd->length = IXGBE_CPU_TO_LE16(length);

	return ixgbe_aci_send_cmd(hw, &desc, data, length);
}

/**
 * ixgbe_aci_erase_nvm - erase NVM sector
 * @hw: pointer to the HW struct
 * @module_typeid: module pointer location in words from the NVM beginning
 *
 * Erase the NVM sector using the ACI command (0x0702).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_erase_nvm(struct ixgbe_hw *hw, u16 module_typeid)
{
	struct ixgbe_aci_desc desc;
	struct ixgbe_aci_cmd_nvm *cmd;
	s32 status;
	__le16 len;

	/* read a length value from SR, so module_typeid is equal to 0 */
	/* calculate offset where module size is placed from bytes to words */
	/* set last command and read from SR values to true */
	status = ixgbe_aci_read_nvm(hw, 0, 2 * module_typeid + 2, 2, &len, true,
				 true);
	if (status)
		return status;

	cmd = &desc.params.nvm;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_nvm_erase);

	cmd->module_typeid = IXGBE_CPU_TO_LE16(module_typeid);
	cmd->length = len;
	cmd->offset_low = 0;
	cmd->offset_high = 0;

	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_aci_update_nvm - update NVM
 * @hw: pointer to the HW struct
 * @module_typeid: module pointer location in words from the NVM beginning
 * @offset: byte offset from the module beginning
 * @length: length of the section to be written (in bytes from the offset)
 * @data: command buffer (size [bytes] = length)
 * @last_command: tells if this is the last command in a series
 * @command_flags: command parameters
 *
 * Update the NVM using the ACI command (0x0703).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_update_nvm(struct ixgbe_hw *hw, u16 module_typeid,
			 u32 offset, u16 length, void *data,
			 bool last_command, u8 command_flags)
{
	struct ixgbe_aci_desc desc;
	struct ixgbe_aci_cmd_nvm *cmd;

	cmd = &desc.params.nvm;

	/* In offset the highest byte must be zeroed. */
	if (offset & 0xFF000000)
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_nvm_write);

	cmd->cmd_flags |= command_flags;

	/* If this is the last command in a series, set the proper flag. */
	if (last_command)
		cmd->cmd_flags |= IXGBE_ACI_NVM_LAST_CMD;
	cmd->module_typeid = IXGBE_CPU_TO_LE16(module_typeid);
	cmd->offset_low = IXGBE_CPU_TO_LE16(offset & 0xFFFF);
	cmd->offset_high = (offset >> 16) & 0xFF;
	cmd->length = IXGBE_CPU_TO_LE16(length);

	desc.flags |= IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_RD);

	return ixgbe_aci_send_cmd(hw, &desc, data, length);
}

/**
 * ixgbe_aci_read_nvm_cfg - read an NVM config block
 * @hw: pointer to the HW struct
 * @cmd_flags: NVM access admin command bits
 * @field_id: field or feature ID
 * @data: buffer for result
 * @buf_size: buffer size
 * @elem_count: pointer to count of elements read by FW
 *
 * Reads a single or multiple feature/field ID and data using ACI command
 * (0x0704).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_read_nvm_cfg(struct ixgbe_hw *hw, u8 cmd_flags,
			   u16 field_id, void *data, u16 buf_size,
			   u16 *elem_count)
{
	struct ixgbe_aci_cmd_nvm_cfg *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	cmd = &desc.params.nvm_cfg;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_nvm_cfg_read);

	cmd->cmd_flags = cmd_flags;
	cmd->id = IXGBE_CPU_TO_LE16(field_id);

	status = ixgbe_aci_send_cmd(hw, &desc, data, buf_size);
	if (!status && elem_count)
		*elem_count = IXGBE_LE16_TO_CPU(cmd->count);

	return status;
}

/**
 * ixgbe_aci_write_nvm_cfg - write an NVM config block
 * @hw: pointer to the HW struct
 * @cmd_flags: NVM access admin command bits
 * @data: buffer for result
 * @buf_size: buffer size
 * @elem_count: count of elements to be written
 *
 * Writes a single or multiple feature/field ID and data using ACI command
 * (0x0705).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_write_nvm_cfg(struct ixgbe_hw *hw, u8 cmd_flags,
			    void *data, u16 buf_size, u16 elem_count)
{
	struct ixgbe_aci_cmd_nvm_cfg *cmd;
	struct ixgbe_aci_desc desc;

	cmd = &desc.params.nvm_cfg;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_nvm_cfg_write);
	desc.flags |= IXGBE_CPU_TO_LE16(IXGBE_ACI_FLAG_RD);

	cmd->count = IXGBE_CPU_TO_LE16(elem_count);
	cmd->cmd_flags = cmd_flags;

	return ixgbe_aci_send_cmd(hw, &desc, data, buf_size);
}

/**
 * ixgbe_nvm_validate_checksum - validate checksum
 * @hw: pointer to the HW struct
 *
 * Verify NVM PFA checksum validity using ACI command (0x0706).
 * If the checksum verification failed, IXGBE_ERR_NVM_CHECKSUM is returned.
 * The function acquires and then releases the NVM ownership.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_nvm_validate_checksum(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_nvm_checksum *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	status = ixgbe_acquire_nvm(hw, IXGBE_RES_READ);
	if (status)
		return status;

	cmd = &desc.params.nvm_checksum;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_nvm_checksum);
	cmd->flags = IXGBE_ACI_NVM_CHECKSUM_VERIFY;

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);

	ixgbe_release_nvm(hw);

	if (!status)
		if (IXGBE_LE16_TO_CPU(cmd->checksum) !=
		    IXGBE_ACI_NVM_CHECKSUM_CORRECT) {
			ERROR_REPORT1(IXGBE_ERROR_INVALID_STATE,
				      "Invalid Shadow Ram checksum");
			status = IXGBE_ERR_NVM_CHECKSUM;
		}

	return status;
}

/**
 * ixgbe_nvm_recalculate_checksum - recalculate checksum
 * @hw: pointer to the HW struct
 *
 * Recalculate NVM PFA checksum using ACI command (0x0706).
 * The function acquires and then releases the NVM ownership.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_nvm_recalculate_checksum(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_nvm_checksum *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	status = ixgbe_acquire_nvm(hw, IXGBE_RES_WRITE);
	if (status)
		return status;

	cmd = &desc.params.nvm_checksum;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_nvm_checksum);
	cmd->flags = IXGBE_ACI_NVM_CHECKSUM_RECALC;

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);

	ixgbe_release_nvm(hw);

	return status;
}

/**
 * ixgbe_nvm_write_activate - NVM activate write
 * @hw: pointer to the HW struct
 * @cmd_flags: flags for write activate command
 * @response_flags: response indicators from firmware
 *
 * Update the control word with the required banks' validity bits
 * and dumps the Shadow RAM to flash using ACI command (0x0707).
 *
 * cmd_flags controls which banks to activate, the preservation level to use
 * when activating the NVM bank, and whether an EMP reset is required for
 * activation.
 *
 * Note that the 16bit cmd_flags value is split between two separate 1 byte
 * flag values in the descriptor.
 *
 * On successful return of the firmware command, the response_flags variable
 * is updated with the flags reported by firmware indicating certain status,
 * such as whether EMP reset is enabled.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_nvm_write_activate(struct ixgbe_hw *hw, u16 cmd_flags,
			     u8 *response_flags)
{
	struct ixgbe_aci_desc desc;
	struct ixgbe_aci_cmd_nvm *cmd;
	s32 status;

	cmd = &desc.params.nvm;
	ixgbe_fill_dflt_direct_cmd_desc(&desc,
					ixgbe_aci_opc_nvm_write_activate);

	cmd->cmd_flags = LO_BYTE(cmd_flags);
	cmd->offset_high = HI_BYTE(cmd_flags);

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
	if (!status && response_flags)
		*response_flags = cmd->cmd_flags;

	return status;
}

/**
 * ixgbe_get_flash_bank_offset - Get offset into requested flash bank
 * @hw: pointer to the HW structure
 * @bank: whether to read from the active or inactive flash bank
 * @module: the module to read from
 *
 * Based on the module, lookup the module offset from the beginning of the
 * flash.
 *
 * Return: the flash offset. Note that a value of zero is invalid and must be
 * treated as an error.
 */
static u32 ixgbe_get_flash_bank_offset(struct ixgbe_hw *hw,
				       enum ixgbe_bank_select bank,
				       u16 module)
{
	struct ixgbe_bank_info *banks = &hw->flash.banks;
	enum ixgbe_flash_bank active_bank;
	bool second_bank_active;
	u32 offset, size;

	switch (module) {
	case E610_SR_1ST_NVM_BANK_PTR:
		offset = banks->nvm_ptr;
		size = banks->nvm_size;
		active_bank = banks->nvm_bank;
		break;
	case E610_SR_1ST_OROM_BANK_PTR:
		offset = banks->orom_ptr;
		size = banks->orom_size;
		active_bank = banks->orom_bank;
		break;
	case E610_SR_NETLIST_BANK_PTR:
		offset = banks->netlist_ptr;
		size = banks->netlist_size;
		active_bank = banks->netlist_bank;
		break;
	default:
		return 0;
	}

	switch (active_bank) {
	case IXGBE_1ST_FLASH_BANK:
		second_bank_active = false;
		break;
	case IXGBE_2ND_FLASH_BANK:
		second_bank_active = true;
		break;
	default:
		return 0;
    }

	/* The second flash bank is stored immediately following the first
	 * bank. Based on whether the 1st or 2nd bank is active, and whether
	 * we want the active or inactive bank, calculate the desired offset.
	 */
	switch (bank) {
	case IXGBE_ACTIVE_FLASH_BANK:
		return offset + (second_bank_active ? size : 0);
	case IXGBE_INACTIVE_FLASH_BANK:
		return offset + (second_bank_active ? 0 : size);
	}

	return 0;
}

/**
 * ixgbe_read_flash_module - Read a word from one of the main NVM modules
 * @hw: pointer to the HW structure
 * @bank: which bank of the module to read
 * @module: the module to read
 * @offset: the offset into the module in bytes
 * @data: storage for the word read from the flash
 * @length: bytes of data to read
 *
 * Read data from the specified flash module. The bank parameter indicates
 * whether or not to read from the active bank or the inactive bank of that
 * module.
 *
 * The word will be read using flat NVM access, and relies on the
 * hw->flash.banks data being setup by ixgbe_determine_active_flash_banks()
 * during initialization.
 *
 * Return: the exit code of the operation.
 */
static s32 ixgbe_read_flash_module(struct ixgbe_hw *hw,
				   enum ixgbe_bank_select bank,
				   u16 module, u32 offset, u8 *data, u32 length)
{
	s32 status;
	u32 start;

	start = ixgbe_get_flash_bank_offset(hw, bank, module);
	if (!start) {
		return IXGBE_ERR_PARAM;
	}

	status = ixgbe_acquire_nvm(hw, IXGBE_RES_READ);
	if (status)
		return status;

	status = ixgbe_read_flat_nvm(hw, start + offset, &length, data, false);

	ixgbe_release_nvm(hw);

	return status;
}

/**
 * ixgbe_read_netlist_module - Read data from the netlist module area
 * @hw: pointer to the HW structure
 * @bank: whether to read from the active or inactive module
 * @offset: offset into the netlist to read from
 * @data: storage for returned word value
 *
 * Read a word from the specified netlist bank.
 *
 * Return: the exit code of the operation.
 */
static s32 ixgbe_read_netlist_module(struct ixgbe_hw *hw,
				     enum ixgbe_bank_select bank,
				     u32 offset, u16 *data)
{
	__le16 data_local;
	s32 status;

	status = ixgbe_read_flash_module(hw, bank, E610_SR_NETLIST_BANK_PTR,
					 offset * sizeof(u16),
					 (u8 *)&data_local,
					 sizeof(u16));
	if (!status)
		*data = IXGBE_LE16_TO_CPU(data_local);

	return status;
}

/**
 * ixgbe_read_nvm_module - Read from the active main NVM module
 * @hw: pointer to the HW structure
 * @bank: whether to read from active or inactive NVM module
 * @offset: offset into the NVM module to read, in words
 * @data: storage for returned word value
 *
 * Read the specified word from the active NVM module. This includes the CSS
 * header at the start of the NVM module.
 *
 * Return: the exit code of the operation.
 */
static s32 ixgbe_read_nvm_module(struct ixgbe_hw *hw,
				 enum ixgbe_bank_select bank,
				  u32 offset, u16 *data)
{
	__le16 data_local;
	s32 status;

	status = ixgbe_read_flash_module(hw, bank, E610_SR_1ST_NVM_BANK_PTR,
					 offset * sizeof(u16),
					 (u8 *)&data_local,
					 sizeof(u16));
	if (!status)
		*data = IXGBE_LE16_TO_CPU(data_local);

	return status;
}

/**
 * ixgbe_get_nvm_css_hdr_len - Read the CSS header length from the
 * NVM CSS header
 * @hw: pointer to the HW struct
 * @bank: whether to read from the active or inactive flash bank
 * @hdr_len: storage for header length in words
 *
 * Read the CSS header length from the NVM CSS header and add the
 * Authentication header size, and then convert to words.
 *
 * Return: the exit code of the operation.
 */
static s32 ixgbe_get_nvm_css_hdr_len(struct ixgbe_hw *hw,
				     enum ixgbe_bank_select bank,
				     u32 *hdr_len)
{
	u16 hdr_len_l, hdr_len_h;
	u32 hdr_len_dword;
	s32 status;

	status = ixgbe_read_nvm_module(hw, bank, IXGBE_NVM_CSS_HDR_LEN_L,
				       &hdr_len_l);
	if (status)
		return status;

	status = ixgbe_read_nvm_module(hw, bank, IXGBE_NVM_CSS_HDR_LEN_H,
				       &hdr_len_h);
	if (status)
		return status;

	/* CSS header length is in DWORD, so convert to words and add
	 * authentication header size
	 */
	hdr_len_dword = hdr_len_h << 16 | hdr_len_l;
	*hdr_len = (hdr_len_dword * 2) + IXGBE_NVM_AUTH_HEADER_LEN;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_read_nvm_sr_copy - Read a word from the Shadow RAM copy in the NVM bank
 * @hw: pointer to the HW structure
 * @bank: whether to read from the active or inactive NVM module
 * @offset: offset into the Shadow RAM copy to read, in words
 * @data: storage for returned word value
 *
 * Read the specified word from the copy of the Shadow RAM found in the
 * specified NVM module.
 *
 * Return: the exit code of the operation.
 */
static s32 ixgbe_read_nvm_sr_copy(struct ixgbe_hw *hw,
				  enum ixgbe_bank_select bank,
				  u32 offset, u16 *data)
{
	u32 hdr_len;
	s32 status;

	status = ixgbe_get_nvm_css_hdr_len(hw, bank, &hdr_len);
	if (status)
		return status;

	hdr_len = ROUND_UP(hdr_len, 32);

	return ixgbe_read_nvm_module(hw, bank, hdr_len + offset, data);
}

/**
 * ixgbe_get_nvm_minsrevs - Get the minsrevs values from flash
 * @hw: pointer to the HW struct
 * @minsrevs: structure to store NVM and OROM minsrev values
 *
 * Read the Minimum Security Revision TLV and extract
 * the revision values from the flash image
 * into a readable structure for processing.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_get_nvm_minsrevs(struct ixgbe_hw *hw,
			   struct ixgbe_minsrev_info *minsrevs)
{
	struct ixgbe_aci_cmd_nvm_minsrev data;
	s32 status;
	u16 valid;

	status = ixgbe_acquire_nvm(hw, IXGBE_RES_READ);
	if (status)
		return status;

	status = ixgbe_aci_read_nvm(hw, IXGBE_ACI_NVM_MINSREV_MOD_ID,
				    0, sizeof(data), &data,
				    true, false);

	ixgbe_release_nvm(hw);

	if (status)
		return status;

	valid = IXGBE_LE16_TO_CPU(data.validity);

	/* Extract NVM minimum security revision */
	if (valid & IXGBE_ACI_NVM_MINSREV_NVM_VALID) {
		u16 minsrev_l = IXGBE_LE16_TO_CPU(data.nvm_minsrev_l);
		u16 minsrev_h = IXGBE_LE16_TO_CPU(data.nvm_minsrev_h);

		minsrevs->nvm = minsrev_h << 16 | minsrev_l;
		minsrevs->nvm_valid = true;
	}

	/* Extract the OROM minimum security revision */
	if (valid & IXGBE_ACI_NVM_MINSREV_OROM_VALID) {
		u16 minsrev_l = IXGBE_LE16_TO_CPU(data.orom_minsrev_l);
		u16 minsrev_h = IXGBE_LE16_TO_CPU(data.orom_minsrev_h);

		minsrevs->orom = minsrev_h << 16 | minsrev_l;
		minsrevs->orom_valid = true;
	}

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_update_nvm_minsrevs - Update minsrevs TLV data in flash
 * @hw: pointer to the HW struct
 * @minsrevs: minimum security revision information
 *
 * Update the NVM or Option ROM minimum security revision fields in the PFA
 * area of the flash. Reads the minsrevs->nvm_valid and minsrevs->orom_valid
 * fields to determine what update is being requested. If the valid bit is not
 * set for that module, then the associated minsrev will be left as is.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_update_nvm_minsrevs(struct ixgbe_hw *hw,
			      struct ixgbe_minsrev_info *minsrevs)
{
	struct ixgbe_aci_cmd_nvm_minsrev data;
	s32 status;

	if (!minsrevs->nvm_valid && !minsrevs->orom_valid) {
		return IXGBE_ERR_PARAM;
	}

	status = ixgbe_acquire_nvm(hw, IXGBE_RES_WRITE);
	if (status)
		return status;

	/* Get current data */
	status = ixgbe_aci_read_nvm(hw, IXGBE_ACI_NVM_MINSREV_MOD_ID, 0,
				    sizeof(data), &data, true, false);
	if (status)
		goto exit_release_res;

	if (minsrevs->nvm_valid) {
		data.nvm_minsrev_l = IXGBE_CPU_TO_LE16(minsrevs->nvm & 0xFFFF);
		data.nvm_minsrev_h = IXGBE_CPU_TO_LE16(minsrevs->nvm >> 16);
		data.validity |=
			IXGBE_CPU_TO_LE16(IXGBE_ACI_NVM_MINSREV_NVM_VALID);
	}

	if (minsrevs->orom_valid) {
		data.orom_minsrev_l = IXGBE_CPU_TO_LE16(minsrevs->orom & 0xFFFF);
		data.orom_minsrev_h = IXGBE_CPU_TO_LE16(minsrevs->orom >> 16);
		data.validity |=
			IXGBE_CPU_TO_LE16(IXGBE_ACI_NVM_MINSREV_OROM_VALID);
	}

	/* Update flash data */
	status = ixgbe_aci_update_nvm(hw, IXGBE_ACI_NVM_MINSREV_MOD_ID, 0,
				      sizeof(data), &data, false,
				      IXGBE_ACI_NVM_SPECIAL_UPDATE);
	if (status)
		goto exit_release_res;

	/* Dump the Shadow RAM to the flash */
	status = ixgbe_nvm_write_activate(hw, 0, NULL);

exit_release_res:
	ixgbe_release_nvm(hw);

	return status;
}

/**
 * ixgbe_get_nvm_srev - Read the security revision from the NVM CSS header
 * @hw: pointer to the HW struct
 * @bank: whether to read from the active or inactive flash bank
 * @srev: storage for security revision
 *
 * Read the security revision out of the CSS header of the active NVM module
 * bank.
 *
 * Return: the exit code of the operation.
 */
static s32 ixgbe_get_nvm_srev(struct ixgbe_hw *hw,
			      enum ixgbe_bank_select bank, u32 *srev)
{
	u16 srev_l, srev_h;
	s32 status;

	status = ixgbe_read_nvm_module(hw, bank, IXGBE_NVM_CSS_SREV_L, &srev_l);
	if (status)
		return status;

	status = ixgbe_read_nvm_module(hw, bank, IXGBE_NVM_CSS_SREV_H, &srev_h);
	if (status)
		return status;

	*srev = srev_h << 16 | srev_l;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_get_nvm_ver_info - Read NVM version information
 * @hw: pointer to the HW struct
 * @bank: whether to read from the active or inactive flash bank
 * @nvm: pointer to NVM info structure
 *
 * Read the NVM EETRACK ID and map version of the main NVM image bank, filling
 * in the nvm info structure.
 *
 * Return: the exit code of the operation.
 */
static s32 ixgbe_get_nvm_ver_info(struct ixgbe_hw *hw,
				  enum ixgbe_bank_select bank,
				  struct ixgbe_nvm_info *nvm)
{
	u16 eetrack_lo, eetrack_hi, ver;
	s32 status;

	status = ixgbe_read_nvm_sr_copy(hw, bank,
					E610_SR_NVM_DEV_STARTER_VER, &ver);
	if (status) {
		return status;
	}

	nvm->major = (ver & E610_NVM_VER_HI_MASK) >> E610_NVM_VER_HI_SHIFT;
	nvm->minor = (ver & E610_NVM_VER_LO_MASK) >> E610_NVM_VER_LO_SHIFT;

	status = ixgbe_read_nvm_sr_copy(hw, bank, E610_SR_NVM_EETRACK_LO,
					&eetrack_lo);
	if (status) {
		return status;
	}
	status = ixgbe_read_nvm_sr_copy(hw, bank, E610_SR_NVM_EETRACK_HI,
					&eetrack_hi);
	if (status) {
		return status;
	}

	nvm->eetrack = (eetrack_hi << 16) | eetrack_lo;

	status = ixgbe_get_nvm_srev(hw, bank, &nvm->srev);

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_get_inactive_nvm_ver - Read Option ROM version from the inactive bank
 * @hw: pointer to the HW structure
 * @nvm: storage for Option ROM version information
 *
 * Reads the NVM EETRACK ID, Map version, and security revision of the
 * inactive NVM bank. Used to access version data for a pending update that
 * has not yet been activated.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_get_inactive_nvm_ver(struct ixgbe_hw *hw, struct ixgbe_nvm_info *nvm)
{
	return ixgbe_get_nvm_ver_info(hw, IXGBE_INACTIVE_FLASH_BANK, nvm);
}

/**
 * ixgbe_get_active_nvm_ver - Read Option ROM version from the active bank
 * @hw: pointer to the HW structure
 * @nvm: storage for Option ROM version information
 *
 * Reads the NVM EETRACK ID, Map version, and security revision of the
 * active NVM bank.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_get_active_nvm_ver(struct ixgbe_hw *hw, struct ixgbe_nvm_info *nvm)
{
	return ixgbe_get_nvm_ver_info(hw, IXGBE_ACTIVE_FLASH_BANK, nvm);
}

/**
 * ixgbe_get_netlist_info
 * @hw: pointer to the HW struct
 * @bank: whether to read from the active or inactive flash bank
 * @netlist: pointer to netlist version info structure
 *
 * Get the netlist version information from the requested bank. Reads the Link
 * Topology section to find the Netlist ID block and extract the relevant
 * information into the netlist version structure.
 *
 * Return: the exit code of the operation.
 */
static s32 ixgbe_get_netlist_info(struct ixgbe_hw *hw,
				  enum ixgbe_bank_select bank,
				  struct ixgbe_netlist_info *netlist)
{
	u16 module_id, length, node_count, i;
	u16 *id_blk;
	s32 status;

	status = ixgbe_read_netlist_module(hw, bank, IXGBE_NETLIST_TYPE_OFFSET,
					   &module_id);
	if (status)
		return status;

	if (module_id != IXGBE_NETLIST_LINK_TOPO_MOD_ID) {
		return IXGBE_ERR_NVM;
	}

	status = ixgbe_read_netlist_module(hw, bank, IXGBE_LINK_TOPO_MODULE_LEN,
					   &length);
	if (status)
		return status;

	/* sanity check that we have at least enough words to store the
	 * netlist ID block
	 */
	if (length < IXGBE_NETLIST_ID_BLK_SIZE) {
		return IXGBE_ERR_NVM;
	}

	status = ixgbe_read_netlist_module(hw, bank, IXGBE_LINK_TOPO_NODE_COUNT,
					   &node_count);
	if (status)
		return status;
	node_count &= IXGBE_LINK_TOPO_NODE_COUNT_M;

	id_blk = (u16 *)ixgbe_calloc(hw, IXGBE_NETLIST_ID_BLK_SIZE,
		     sizeof(*id_blk));
	if (!id_blk)
		return IXGBE_ERR_NO_SPACE;

	/* Read out the entire Netlist ID Block at once. */
	status = ixgbe_read_flash_module(hw, bank, E610_SR_NETLIST_BANK_PTR,
				         IXGBE_NETLIST_ID_BLK_OFFSET(node_count) * sizeof(u16),
				         (u8 *)id_blk,
					 IXGBE_NETLIST_ID_BLK_SIZE * sizeof(u16));
	if (status)
		goto exit_error;

	for (i = 0; i < IXGBE_NETLIST_ID_BLK_SIZE; i++)
		id_blk[i] = IXGBE_LE16_TO_CPU(((__le16 *)id_blk)[i]);

	netlist->major = id_blk[IXGBE_NETLIST_ID_BLK_MAJOR_VER_HIGH] << 16 |
			 id_blk[IXGBE_NETLIST_ID_BLK_MAJOR_VER_LOW];
	netlist->minor = id_blk[IXGBE_NETLIST_ID_BLK_MINOR_VER_HIGH] << 16 |
			 id_blk[IXGBE_NETLIST_ID_BLK_MINOR_VER_LOW];
	netlist->type = id_blk[IXGBE_NETLIST_ID_BLK_TYPE_HIGH] << 16 |
			id_blk[IXGBE_NETLIST_ID_BLK_TYPE_LOW];
	netlist->rev = id_blk[IXGBE_NETLIST_ID_BLK_REV_HIGH] << 16 |
		       id_blk[IXGBE_NETLIST_ID_BLK_REV_LOW];
	netlist->cust_ver = id_blk[IXGBE_NETLIST_ID_BLK_CUST_VER];
	/* Read the left most 4 bytes of SHA */
	netlist->hash = id_blk[IXGBE_NETLIST_ID_BLK_SHA_HASH_WORD(15)] << 16 |
			id_blk[IXGBE_NETLIST_ID_BLK_SHA_HASH_WORD(14)];

exit_error:
	ixgbe_free(hw, id_blk);

	return status;
}

/**
 * ixgbe_get_inactive_netlist_ver
 * @hw: pointer to the HW struct
 * @netlist: pointer to netlist version info structure
 *
 * Read the netlist version data from the inactive netlist bank. Used to
 * extract version data of a pending flash update in order to display the
 * version data.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_get_inactive_netlist_ver(struct ixgbe_hw *hw,
				   struct ixgbe_netlist_info *netlist)
{
	return ixgbe_get_netlist_info(hw, IXGBE_INACTIVE_FLASH_BANK, netlist);
}

/**
 * ixgbe_read_sr_pointer - Read the value of a Shadow RAM pointer word
 * @hw: pointer to the HW structure
 * @offset: the word offset of the Shadow RAM word to read
 * @pointer: pointer value read from Shadow RAM
 *
 * Read the given Shadow RAM word, and convert it to a pointer value specified
 * in bytes. This function assumes the specified offset is a valid pointer
 * word.
 *
 * Each pointer word specifies whether it is stored in word size or 4KB
 * sector size by using the highest bit. The reported pointer value will be in
 * bytes, intended for flat NVM reads.
 *
 * Return: the exit code of the operation.
 */
static s32 ixgbe_read_sr_pointer(struct ixgbe_hw *hw, u16 offset, u32 *pointer)
{
	s32 status;
	u16 value;

	status = ixgbe_read_ee_aci_E610(hw, offset, &value);
	if (status)
		return status;

	/* Determine if the pointer is in 4KB or word units */
	if (value & IXGBE_SR_NVM_PTR_4KB_UNITS)
		*pointer = (value & ~IXGBE_SR_NVM_PTR_4KB_UNITS) * 4 * 1024;
	else
		*pointer = value * 2;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_read_sr_area_size - Read an area size from a Shadow RAM word
 * @hw: pointer to the HW structure
 * @offset: the word offset of the Shadow RAM to read
 * @size: size value read from the Shadow RAM
 *
 * Read the given Shadow RAM word, and convert it to an area size value
 * specified in bytes. This function assumes the specified offset is a valid
 * area size word.
 *
 * Each area size word is specified in 4KB sector units. This function reports
 * the size in bytes, intended for flat NVM reads.
 *
 * Return: the exit code of the operation.
 */
static s32 ixgbe_read_sr_area_size(struct ixgbe_hw *hw, u16 offset, u32 *size)
{
	s32 status;
	u16 value;

	status = ixgbe_read_ee_aci_E610(hw, offset, &value);
	if (status)
		return status;

	/* Area sizes are always specified in 4KB units */
	*size = value * 4 * 1024;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_discover_flash_size - Discover the available flash size.
 * @hw: pointer to the HW struct
 *
 * The device flash could be up to 16MB in size. However, it is possible that
 * the actual size is smaller. Use bisection to determine the accessible size
 * of flash memory.
 *
 * Return: the exit code of the operation.
 */
static s32 ixgbe_discover_flash_size(struct ixgbe_hw *hw)
{
	u32 min_size = 0, max_size = IXGBE_ACI_NVM_MAX_OFFSET + 1;
	s32 status;

	status = ixgbe_acquire_nvm(hw, IXGBE_RES_READ);
	if (status)
		return status;

	while ((max_size - min_size) > 1) {
		u32 offset = (max_size + min_size) / 2;
		u32 len = 1;
		u8 data;

		status = ixgbe_read_flat_nvm(hw, offset, &len, &data, false);
		if (status == IXGBE_ERR_ACI_ERROR &&
		    hw->aci.last_status == IXGBE_ACI_RC_EINVAL) {
			status = IXGBE_SUCCESS;
			max_size = offset;
		} else if (!status) {
			min_size = offset;
		} else {
			/* an unexpected error occurred */
			goto err_read_flat_nvm;
		}
	}

	hw->flash.flash_size = max_size;

err_read_flat_nvm:
	ixgbe_release_nvm(hw);

	return status;
}

/**
 * ixgbe_determine_active_flash_banks - Discover active bank for each module
 * @hw: pointer to the HW struct
 *
 * Read the Shadow RAM control word and determine which banks are active for
 * the NVM, OROM, and Netlist modules. Also read and calculate the associated
 * pointer and size. These values are then cached into the ixgbe_flash_info
 * structure for later use in order to calculate the correct offset to read
 * from the active module.
 *
 * Return: the exit code of the operation.
 */
static s32 ixgbe_determine_active_flash_banks(struct ixgbe_hw *hw)
{
	struct ixgbe_bank_info *banks = &hw->flash.banks;
	u16 ctrl_word;
	s32 status;

	status = ixgbe_read_ee_aci_E610(hw, E610_SR_NVM_CTRL_WORD, &ctrl_word);
	if (status) {
		return status;
	}

	/* Check that the control word indicates validity */
	if ((ctrl_word & IXGBE_SR_CTRL_WORD_1_M) >> IXGBE_SR_CTRL_WORD_1_S !=
	    IXGBE_SR_CTRL_WORD_VALID) {
		return IXGBE_ERR_CONFIG;
	}

	if (!(ctrl_word & IXGBE_SR_CTRL_WORD_NVM_BANK))
		banks->nvm_bank = IXGBE_1ST_FLASH_BANK;
	else
		banks->nvm_bank = IXGBE_2ND_FLASH_BANK;

	if (!(ctrl_word & IXGBE_SR_CTRL_WORD_OROM_BANK))
		banks->orom_bank = IXGBE_1ST_FLASH_BANK;
	else
		banks->orom_bank = IXGBE_2ND_FLASH_BANK;

	if (!(ctrl_word & IXGBE_SR_CTRL_WORD_NETLIST_BANK))
		banks->netlist_bank = IXGBE_1ST_FLASH_BANK;
	else
		banks->netlist_bank = IXGBE_2ND_FLASH_BANK;

	status = ixgbe_read_sr_pointer(hw, E610_SR_1ST_NVM_BANK_PTR,
				       &banks->nvm_ptr);
	if (status) {
		return status;
	}

	status = ixgbe_read_sr_area_size(hw, E610_SR_NVM_BANK_SIZE,
					 &banks->nvm_size);
	if (status) {
		return status;
	}

	status = ixgbe_read_sr_pointer(hw, E610_SR_1ST_OROM_BANK_PTR,
				       &banks->orom_ptr);
	if (status) {
		return status;
	}

	status = ixgbe_read_sr_area_size(hw, E610_SR_OROM_BANK_SIZE,
					 &banks->orom_size);
	if (status) {
		return status;
	}

	status = ixgbe_read_sr_pointer(hw, E610_SR_NETLIST_BANK_PTR,
				       &banks->netlist_ptr);
	if (status) {
		return status;
	}

	status = ixgbe_read_sr_area_size(hw, E610_SR_NETLIST_BANK_SIZE,
					 &banks->netlist_size);
	if (status) {
		return status;
	}

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_init_nvm - initializes NVM setting
 * @hw: pointer to the HW struct
 *
 * Read and populate NVM settings such as Shadow RAM size,
 * max_timeout, and blank_nvm_mode
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_init_nvm(struct ixgbe_hw *hw)
{
	struct ixgbe_flash_info *flash = &hw->flash;
	u32 fla, gens_stat, status;
	u8 sr_size;

	/* The SR size is stored regardless of the NVM programming mode
	 * as the blank mode may be used in the factory line.
	 */
	gens_stat = IXGBE_READ_REG(hw, GLNVM_GENS);
	sr_size = (gens_stat & GLNVM_GENS_SR_SIZE_M) >> GLNVM_GENS_SR_SIZE_S;

	/* Switching to words (sr_size contains power of 2) */
	flash->sr_words = BIT(sr_size) * IXGBE_SR_WORDS_IN_1KB;

	/* Check if we are in the normal or blank NVM programming mode */
	fla = IXGBE_READ_REG(hw, GLNVM_FLA);
	if (fla & GLNVM_FLA_LOCKED_M) { /* Normal programming mode */
		flash->blank_nvm_mode = false;
	} else {
		/* Blank programming mode */
		flash->blank_nvm_mode = true;
		return IXGBE_ERR_NVM_BLANK_MODE;
	}

	status = ixgbe_discover_flash_size(hw);
	if (status) {
		return status;
	}

	status = ixgbe_determine_active_flash_banks(hw);
	if (status) {
		return status;
	}

	status = ixgbe_get_nvm_ver_info(hw, IXGBE_ACTIVE_FLASH_BANK,
					&flash->nvm);
	if (status) {
		return status;
	}

	/* read the netlist version information */
	status = ixgbe_get_netlist_info(hw, IXGBE_ACTIVE_FLASH_BANK,
					&flash->netlist);

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_sanitize_operate - Clear the user data
 * @hw: pointer to the HW struct
 *
 * Clear user data from NVM using ACI command (0x070C).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_sanitize_operate(struct ixgbe_hw *hw)
{
	s32 status;
	u8 values;

	u8 cmd_flags = IXGBE_ACI_SANITIZE_REQ_OPERATE |
		       IXGBE_ACI_SANITIZE_OPERATE_SUBJECT_CLEAR;

	status = ixgbe_sanitize_nvm(hw, cmd_flags, &values);
	if (status)
		return status;
	if ((!(values & IXGBE_ACI_SANITIZE_OPERATE_HOST_CLEAN_DONE) &&
	     !(values & IXGBE_ACI_SANITIZE_OPERATE_BMC_CLEAN_DONE)) ||
	    ((values & IXGBE_ACI_SANITIZE_OPERATE_HOST_CLEAN_DONE) &&
	     !(values & IXGBE_ACI_SANITIZE_OPERATE_HOST_CLEAN_SUCCESS)) ||
	    ((values & IXGBE_ACI_SANITIZE_OPERATE_BMC_CLEAN_DONE) &&
	     !(values & IXGBE_ACI_SANITIZE_OPERATE_BMC_CLEAN_SUCCESS)))
		return IXGBE_ERR_ACI_ERROR;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_sanitize_nvm - Sanitize NVM
 * @hw: pointer to the HW struct
 * @cmd_flags: flag to the ACI command
 * @values: values returned from the command
 *
 * Sanitize NVM using ACI command (0x070C).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_sanitize_nvm(struct ixgbe_hw *hw, u8 cmd_flags, u8 *values)
{
	struct ixgbe_aci_desc desc;
	struct ixgbe_aci_cmd_nvm_sanitization *cmd;
	s32 status;

	cmd = &desc.params.nvm_sanitization;
	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_nvm_sanitization);
	cmd->cmd_flags = cmd_flags;

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
	if (values)
		*values = cmd->values;

	return status;
}

/**
 * ixgbe_read_sr_word_aci - Reads Shadow RAM via ACI
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF)
 * @data: word read from the Shadow RAM
 *
 * Reads one 16 bit word from the Shadow RAM using ixgbe_read_flat_nvm.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_read_sr_word_aci(struct ixgbe_hw  *hw, u16 offset, u16 *data)
{
	u32 bytes = sizeof(u16);
	__le16 data_local;
	s32 status;

	status = ixgbe_read_flat_nvm(hw, offset * sizeof(u16), &bytes,
				     (u8 *)&data_local, true);
	if (status)
		return status;

	*data = IXGBE_LE16_TO_CPU(data_local);
	return IXGBE_SUCCESS;
}

/**
 * ixgbe_read_sr_buf_aci - Reads Shadow RAM buf via ACI
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF)
 * @words: (in) number of words to read; (out) number of words actually read
 * @data: words read from the Shadow RAM
 *
 * Reads 16 bit words (data buf) from the Shadow RAM. Ownership of the NVM is
 * taken before reading the buffer and later released.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_read_sr_buf_aci(struct ixgbe_hw *hw, u16 offset, u16 *words,
			  u16 *data)
{
	u32 bytes = *words * 2, i;
	s32 status;

	status = ixgbe_read_flat_nvm(hw, offset * 2, &bytes, (u8 *)data, true);

	*words = bytes / 2;

	for (i = 0; i < *words; i++)
		data[i] = IXGBE_LE16_TO_CPU(((__le16 *)data)[i]);

	return status;
}

/**
 * ixgbe_read_flat_nvm - Read portion of NVM by flat offset
 * @hw: pointer to the HW struct
 * @offset: offset from beginning of NVM
 * @length: (in) number of bytes to read; (out) number of bytes actually read
 * @data: buffer to return data in (sized to fit the specified length)
 * @read_shadow_ram: if true, read from shadow RAM instead of NVM
 *
 * Reads a portion of the NVM, as a flat memory space. This function correctly
 * breaks read requests across Shadow RAM sectors, prevents Shadow RAM size
 * from being exceeded in case of Shadow RAM read requests and ensures that no
 * single read request exceeds the maximum 4KB read for a single admin command.
 *
 * Returns a status code on failure. Note that the data pointer may be
 * partially updated if some reads succeed before a failure.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_read_flat_nvm(struct ixgbe_hw  *hw, u32 offset, u32 *length,
			u8 *data, bool read_shadow_ram)
{
	u32 inlen = *length;
	u32 bytes_read = 0;
	bool last_cmd;
	s32 status;

	*length = 0;

	/* Verify the length of the read if this is for the Shadow RAM */
	if (read_shadow_ram && ((offset + inlen) >
				(hw->eeprom.word_size * 2u))) {
		return IXGBE_ERR_PARAM;
	}

	do {
		u32 read_size, sector_offset;

		/* ixgbe_aci_read_nvm cannot read more than 4KB at a time.
		 * Additionally, a read from the Shadow RAM may not cross over
		 * a sector boundary. Conveniently, the sector size is also 4KB.
		 */
		sector_offset = offset % IXGBE_ACI_MAX_BUFFER_SIZE;
		read_size = MIN_T(u32,
				  IXGBE_ACI_MAX_BUFFER_SIZE - sector_offset,
				  inlen - bytes_read);

		last_cmd = !(bytes_read + read_size < inlen);

		/* ixgbe_aci_read_nvm takes the length as a u16. Our read_size
		 * is calculated using a u32, but the IXGBE_ACI_MAX_BUFFER_SIZE
		 * maximum size guarantees that it will fit within the 2 bytes.
		 */
		status = ixgbe_aci_read_nvm(hw, IXGBE_ACI_NVM_START_POINT,
					    offset, (u16)read_size,
					    data + bytes_read, last_cmd,
					    read_shadow_ram);
		if (status)
			break;

		bytes_read += read_size;
		offset += read_size;
	} while (!last_cmd);

	*length = bytes_read;
	return status;
}

/**
 * ixgbe_check_sr_access_params - verify params for Shadow RAM R/W operations.
 * @hw: pointer to the HW structure
 * @offset: offset in words from module start
 * @words: number of words to access
 *
 * Check if all the parameters are valid
 * before performing any Shadow RAM read/write operations.
 *
 * Return: the exit code of the operation.
 * * - IXGBE_SUCCESS - success.
 * * - IXGBE_ERR_PARAM - NVM error: offset beyond SR limit or
 * NVM error: tried to access more words then the set limit or
 * NVM error: cannot spread over two sectors.
 */
static s32 ixgbe_check_sr_access_params(struct ixgbe_hw *hw, u32 offset,
					u16 words)
{
	if ((offset + words) > hw->eeprom.word_size) {
		return IXGBE_ERR_PARAM;
	}

	if (words > IXGBE_SR_SECTOR_SIZE_IN_WORDS) {
		/* We can access only up to 4KB (one sector),
		 * in one Admin Command write
		 */
		return IXGBE_ERR_PARAM;
	}

	if (((offset + (words - 1)) / IXGBE_SR_SECTOR_SIZE_IN_WORDS) !=
	    (offset / IXGBE_SR_SECTOR_SIZE_IN_WORDS)) {
		/* A single access cannot spread over two sectors */
		return IXGBE_ERR_PARAM;
	}

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_write_sr_word_aci - Writes Shadow RAM word
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to write
 * @data: word to write to the Shadow RAM
 *
 * Writes a 16 bit word to the Shadow RAM using the admin command.
 * NVM ownership must be acquired before calling this function and released
 * by a caller. To commit SR to NVM update checksum function should be called.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_write_sr_word_aci(struct ixgbe_hw *hw, u32 offset, const u16 *data)
{
	__le16 data_local = IXGBE_CPU_TO_LE16(*data);
	s32 status;

	status = ixgbe_check_sr_access_params(hw, offset, 1);
	if (!status)
		status = ixgbe_aci_update_nvm(hw, 0, BYTES_PER_WORD * offset,
					      BYTES_PER_WORD, &data_local,
					      false, 0);

	return status;
}

/**
 * ixgbe_write_sr_buf_aci - Writes Shadow RAM buf
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM buffer to write
 * @words: number of words to write
 * @data: words to write to the Shadow RAM
 *
 * Writes a 16 bit word to the Shadow RAM using the admin command.
 * NVM ownership must be acquired before calling this function and released
 * by a caller. To commit SR to NVM update checksum function should be called.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_write_sr_buf_aci(struct ixgbe_hw *hw, u32 offset, u16 words,
			   const u16 *data)
{
	__le16 *data_local;
	s32 status;
	void *vmem;
	u32 i;

	vmem = ixgbe_calloc(hw, words, sizeof(u16));
	if (!vmem)
		return IXGBE_ERR_OUT_OF_MEM;
	data_local = (__le16 *)vmem;

	for (i = 0; i < words; i++)
		data_local[i] = IXGBE_CPU_TO_LE16(data[i]);

	/* Here we will only write one buffer as the size of the modules
	 * mirrored in the Shadow RAM is always less than 4K.
	 */
	status = ixgbe_check_sr_access_params(hw, offset, words);
	if (!status)
		status = ixgbe_aci_update_nvm(hw, 0, BYTES_PER_WORD * offset,
					      BYTES_PER_WORD * words,
					      data_local, false, 0);

	ixgbe_free(hw, vmem);

	return status;
}

/**
 * ixgbe_aci_alternate_write - write to alternate structure
 * @hw: pointer to the hardware structure
 * @reg_addr0: address of first dword to be written
 * @reg_val0: value to be written under 'reg_addr0'
 * @reg_addr1: address of second dword to be written
 * @reg_val1: value to be written under 'reg_addr1'
 *
 * Write one or two dwords to alternate structure using ACI command (0x0900).
 * Fields are indicated by 'reg_addr0' and 'reg_addr1' register numbers.
 *
 * Return: 0 on success and error code on failure.
 */
s32 ixgbe_aci_alternate_write(struct ixgbe_hw *hw, u32 reg_addr0,
			      u32 reg_val0, u32 reg_addr1, u32 reg_val1)
{
	struct ixgbe_aci_cmd_read_write_alt_direct *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	cmd = &desc.params.read_write_alt_direct;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_write_alt_direct);
	cmd->dword0_addr = IXGBE_CPU_TO_LE32(reg_addr0);
	cmd->dword1_addr = IXGBE_CPU_TO_LE32(reg_addr1);
	cmd->dword0_value = IXGBE_CPU_TO_LE32(reg_val0);
	cmd->dword1_value = IXGBE_CPU_TO_LE32(reg_val1);

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);

	return status;
}

/**
 * ixgbe_aci_alternate_read - read from alternate structure
 * @hw: pointer to the hardware structure
 * @reg_addr0: address of first dword to be read
 * @reg_val0: pointer for data read from 'reg_addr0'
 * @reg_addr1: address of second dword to be read
 * @reg_val1: pointer for data read from 'reg_addr1'
 *
 * Read one or two dwords from alternate structure using ACI command (0x0902).
 * Fields are indicated by 'reg_addr0' and 'reg_addr1' register numbers.
 * If 'reg_val1' pointer is not passed then only register at 'reg_addr0'
 * is read.
 *
 * Return: 0 on success and error code on failure.
 */
s32 ixgbe_aci_alternate_read(struct ixgbe_hw *hw, u32 reg_addr0,
			     u32 *reg_val0, u32 reg_addr1, u32 *reg_val1)
{
	struct ixgbe_aci_cmd_read_write_alt_direct *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	cmd = &desc.params.read_write_alt_direct;

	if (!reg_val0)
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_read_alt_direct);
	cmd->dword0_addr = IXGBE_CPU_TO_LE32(reg_addr0);
	cmd->dword1_addr = IXGBE_CPU_TO_LE32(reg_addr1);

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);

	if (status == IXGBE_SUCCESS) {
		*reg_val0 = IXGBE_LE32_TO_CPU(cmd->dword0_value);

		if (reg_val1)
			*reg_val1 = IXGBE_LE32_TO_CPU(cmd->dword1_value);
	}

	return status;
}

/**
 * ixgbe_aci_alternate_write_done - check if writing to alternate structure
 * is done
 * @hw: pointer to the HW structure.
 * @bios_mode: indicates whether the command is executed by UEFI or legacy BIOS
 * @reset_needed: indicates the SW should trigger GLOBAL reset
 *
 * Indicates to the FW that alternate structures have been changed.
 *
 * Return: 0 on success and error code on failure.
 */
s32 ixgbe_aci_alternate_write_done(struct ixgbe_hw *hw, u8 bios_mode,
				   bool *reset_needed)
{
	struct ixgbe_aci_cmd_done_alt_write *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	cmd = &desc.params.done_alt_write;

	if (!reset_needed)
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_done_alt_write);
	cmd->flags = bios_mode;

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
	if (!status)
		*reset_needed = (IXGBE_LE16_TO_CPU(cmd->flags) &
				 IXGBE_ACI_RESP_RESET_NEEDED) != 0;

	return status;
}

/**
 * ixgbe_aci_alternate_clear - clear alternate structure
 * @hw: pointer to the HW structure.
 *
 * Clear the alternate structures of the port from which the function
 * is called.
 *
 * Return: 0 on success and error code on failure.
 */
s32 ixgbe_aci_alternate_clear(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_desc desc;
	s32 status;

	ixgbe_fill_dflt_direct_cmd_desc(&desc,
					ixgbe_aci_opc_clear_port_alt_write);

	status = ixgbe_aci_send_cmd(hw, &desc, NULL, 0);

	return status;
}

/**
 * ixgbe_aci_get_internal_data - get internal FW/HW data
 * @hw: pointer to the hardware structure
 * @cluster_id: specific cluster to dump
 * @table_id: table ID within cluster
 * @start: index of line in the block to read
 * @buf: dump buffer
 * @buf_size: dump buffer size
 * @ret_buf_size: return buffer size (returned by FW)
 * @ret_next_cluster: next cluster to read (returned by FW)
 * @ret_next_table: next block to read (returned by FW)
 * @ret_next_index: next index to read (returned by FW)
 *
 * Get internal FW/HW data using ACI command (0xFF08) for debug purposes.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_get_internal_data(struct ixgbe_hw *hw, u16 cluster_id,
				u16 table_id, u32 start, void *buf,
				u16 buf_size, u16 *ret_buf_size,
				u16 *ret_next_cluster, u16 *ret_next_table,
				u32 *ret_next_index)
{
	struct ixgbe_aci_cmd_debug_dump_internals *cmd;
	struct ixgbe_aci_desc desc;
	s32 status;

	cmd = &desc.params.debug_dump;

	if (buf_size == 0 || !buf)
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc,
					ixgbe_aci_opc_debug_dump_internals);

	cmd->cluster_id = IXGBE_CPU_TO_LE16(cluster_id);
	cmd->table_id = IXGBE_CPU_TO_LE16(table_id);
	cmd->idx = IXGBE_CPU_TO_LE32(start);

	status = ixgbe_aci_send_cmd(hw, &desc, buf, buf_size);

	if (!status) {
		if (ret_buf_size)
			*ret_buf_size = IXGBE_LE16_TO_CPU(desc.datalen);
		if (ret_next_cluster)
			*ret_next_cluster = IXGBE_LE16_TO_CPU(cmd->cluster_id);
		if (ret_next_table)
			*ret_next_table = IXGBE_LE16_TO_CPU(cmd->table_id);
		if (ret_next_index)
			*ret_next_index = IXGBE_LE32_TO_CPU(cmd->idx);
	}

	return status;
}

/**
 * ixgbe_validate_nvm_rw_reg - Check that an NVM access request is valid
 * @cmd: NVM access command structure
 *
 * Validates that an NVM access structure is request to read or write a valid
 * register offset. First validates that the module and flags are correct, and
 * then ensures that the register offset is one of the accepted registers.
 *
 * Return: 0 if the register access is valid, out of range error code otherwise.
 */
static s32
ixgbe_validate_nvm_rw_reg(struct ixgbe_nvm_access_cmd *cmd)
{
	u16 i;

	switch (cmd->offset) {
	case GL_HICR:
	case GL_HICR_EN: /* Note, this register is read only */
	case GL_FWSTS:
	case GL_MNG_FWSM:
	case GLNVM_GENS:
	case GLNVM_FLA:
	case GL_FWRESETCNT:
		return 0;
	default:
		break;
	}

	for (i = 0; i <= GL_HIDA_MAX_INDEX; i++)
		if (cmd->offset == (u32)GL_HIDA(i))
			return 0;

	for (i = 0; i <= GL_HIBA_MAX_INDEX; i++)
		if (cmd->offset == (u32)GL_HIBA(i))
			return 0;

	/* All other register offsets are not valid */
	return IXGBE_ERR_OUT_OF_RANGE;
}

/**
 * ixgbe_nvm_access_read - Handle an NVM read request
 * @hw: pointer to the HW struct
 * @cmd: NVM access command to process
 * @data: storage for the register value read
 *
 * Process an NVM access request to read a register.
 *
 * Return: 0 if the register read is valid and successful,
 * out of range error code otherwise.
 */
static s32 ixgbe_nvm_access_read(struct ixgbe_hw *hw,
			struct ixgbe_nvm_access_cmd *cmd,
			struct ixgbe_nvm_access_data *data)
{
	s32 status;

	/* Always initialize the output data, even on failure */
	memset(&data->regval, 0, cmd->data_size);

	/* Make sure this is a valid read/write access request */
	status = ixgbe_validate_nvm_rw_reg(cmd);
	if (status)
		return status;

	DEBUGOUT1("NVM access: reading register %08x\n", cmd->offset);

	/* Read the register and store the contents in the data field */
	data->regval = IXGBE_READ_REG(hw, cmd->offset);

	return 0;
}

/**
 * ixgbe_nvm_access_write - Handle an NVM write request
 * @hw: pointer to the HW struct
 * @cmd: NVM access command to process
 * @data: NVM access data to write
 *
 * Process an NVM access request to write a register.
 *
 * Return: 0 if the register write is valid and successful,
 * out of range error code otherwise.
 */
static s32 ixgbe_nvm_access_write(struct ixgbe_hw *hw,
			struct ixgbe_nvm_access_cmd *cmd,
			struct ixgbe_nvm_access_data *data)
{
	s32 status;

	/* Make sure this is a valid read/write access request */
	status = ixgbe_validate_nvm_rw_reg(cmd);
	if (status)
		return status;

	/* Reject requests to write to read-only registers */
	switch (cmd->offset) {
	case GL_HICR_EN:
		return IXGBE_ERR_OUT_OF_RANGE;
	default:
		break;
	}

	DEBUGOUT2("NVM access: writing register %08x with value %08x\n",
		cmd->offset, data->regval);

	/* Write the data field to the specified register */
	IXGBE_WRITE_REG(hw, cmd->offset, data->regval);

	return 0;
}

/**
 * ixgbe_handle_nvm_access - Handle an NVM access request
 * @hw: pointer to the HW struct
 * @cmd: NVM access command info
 * @data: pointer to read or return data
 *
 * Process an NVM access request. Read the command structure information and
 * determine if it is valid. If not, report an error indicating the command
 * was invalid.
 *
 * For valid commands, perform the necessary function, copying the data into
 * the provided data buffer.
 *
 * Return: 0 if the nvm access request is valid and successful,
 * error code otherwise.
 */
s32 ixgbe_handle_nvm_access(struct ixgbe_hw *hw,
			struct ixgbe_nvm_access_cmd *cmd,
			struct ixgbe_nvm_access_data *data)
{
	switch (cmd->command) {
	case IXGBE_NVM_CMD_READ:
		return ixgbe_nvm_access_read(hw, cmd, data);
	case IXGBE_NVM_CMD_WRITE:
		return ixgbe_nvm_access_write(hw, cmd, data);
	default:
		return IXGBE_ERR_PARAM;
	}
}

/**
 * ixgbe_aci_set_health_status_config - Configure FW health events
 * @hw: pointer to the HW struct
 * @event_source: type of diagnostic events to enable
 *
 * Configure the health status event types that the firmware will send to this
 * PF using ACI command (0xFF20). The supported event types are: PF-specific,
 * all PFs, and global.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_aci_set_health_status_config(struct ixgbe_hw *hw, u8 event_source)
{
	struct ixgbe_aci_cmd_set_health_status_config *cmd;
	struct ixgbe_aci_desc desc;

	cmd = &desc.params.set_health_status_config;

	ixgbe_fill_dflt_direct_cmd_desc(&desc,
				      ixgbe_aci_opc_set_health_status_config);

	cmd->event_source = event_source;

	return ixgbe_aci_send_cmd(hw, &desc, NULL, 0);
}

/**
 * ixgbe_init_ops_E610 - Inits func ptrs and MAC type
 * @hw: pointer to hardware structure
 *
 * Initialize the function pointers and assign the MAC type for E610.
 * Does not touch the hardware.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_init_ops_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_eeprom_info *eeprom = &hw->eeprom;
	struct ixgbe_mac_info *mac = &hw->mac;
	struct ixgbe_phy_info *phy = &hw->phy;
	s32 ret_val;

	ret_val = ixgbe_init_ops_X550(hw);

	/* MAC */
	mac->ops.reset_hw = ixgbe_reset_hw_E610;
	mac->ops.start_hw = ixgbe_start_hw_E610;
	mac->ops.get_media_type = ixgbe_get_media_type_E610;
	mac->ops.get_supported_physical_layer =
		ixgbe_get_supported_physical_layer_E610;
	mac->ops.get_san_mac_addr = NULL;
	mac->ops.set_san_mac_addr = NULL;
	mac->ops.get_wwn_prefix = NULL;
	mac->ops.setup_link = ixgbe_setup_link_E610;
	mac->ops.check_link = ixgbe_check_link_E610;
	mac->ops.get_link_capabilities = ixgbe_get_link_capabilities_E610;
	mac->ops.setup_fc = ixgbe_setup_fc_E610;
	mac->ops.fc_autoneg = ixgbe_fc_autoneg_E610;
	mac->ops.set_fw_drv_ver = ixgbe_set_fw_drv_ver_E610;
	mac->ops.disable_rx = ixgbe_disable_rx_E610;
	mac->ops.setup_eee = ixgbe_setup_eee_E610;
	mac->ops.fw_recovery_mode = ixgbe_fw_recovery_mode_E610;
	mac->ops.fw_rollback_mode = ixgbe_fw_rollback_mode_E610;
	mac->ops.get_fw_tsam_mode = ixgbe_get_fw_tsam_mode_E610;
	mac->ops.get_fw_version = ixgbe_aci_get_fw_ver;
	mac->ops.get_nvm_version = ixgbe_get_active_nvm_ver;
       mac->ops.get_thermal_sensor_data = NULL;
       mac->ops.init_thermal_sensor_thresh = NULL;

	/* PHY */
	phy->ops.init = ixgbe_init_phy_ops_E610;
	phy->ops.identify = ixgbe_identify_phy_E610;
	phy->eee_speeds_supported = IXGBE_LINK_SPEED_10_FULL |
				    IXGBE_LINK_SPEED_100_FULL |
				    IXGBE_LINK_SPEED_1GB_FULL;
	phy->eee_speeds_advertised = phy->eee_speeds_supported;

	/* Additional ops overrides for e610 to go here */
	eeprom->ops.init_params = ixgbe_init_eeprom_params_E610;
	eeprom->ops.read = ixgbe_read_ee_aci_E610;
	eeprom->ops.read_buffer = ixgbe_read_ee_aci_buffer_E610;
	eeprom->ops.write = ixgbe_write_ee_aci_E610;
	eeprom->ops.write_buffer = ixgbe_write_ee_aci_buffer_E610;
	eeprom->ops.calc_checksum = ixgbe_calc_eeprom_checksum_E610;
	eeprom->ops.update_checksum = ixgbe_update_eeprom_checksum_E610;
	eeprom->ops.validate_checksum = ixgbe_validate_eeprom_checksum_E610;
	eeprom->ops.read_pba_string = ixgbe_read_pba_string_E610;

	/* Initialize bus function number */
	hw->mac.ops.set_lan_id(hw);

	return ret_val;
}

/**
 * ixgbe_reset_hw_E610 - Perform hardware reset
 * @hw: pointer to hardware structure
 *
 * Resets the hardware by resetting the transmit and receive units, masks
 * and clears all interrupts, and perform a reset.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_reset_hw_E610(struct ixgbe_hw *hw)
{
	u32 swfw_mask = hw->phy.phy_semaphore_mask;
	u32 ctrl, i;
	s32 status;

	DEBUGFUNC("ixgbe_reset_hw_E610");

	/* Call adapter stop to disable tx/rx and clear interrupts */
	status = hw->mac.ops.stop_adapter(hw);
	if (status != IXGBE_SUCCESS)
		goto reset_hw_out;

	/* flush pending Tx transactions */
	ixgbe_clear_tx_pending(hw);

	status = hw->phy.ops.init(hw);
	if (status != IXGBE_SUCCESS)
		DEBUGOUT1("Failed to initialize PHY ops, STATUS = %d\n",
			  status);
mac_reset_top:
	status = hw->mac.ops.acquire_swfw_sync(hw, swfw_mask);
	if (status != IXGBE_SUCCESS) {
		ERROR_REPORT2(IXGBE_ERROR_CAUTION,
			      "semaphore failed with %d", status);
		return IXGBE_ERR_SWFW_SYNC;
	}
	ctrl = IXGBE_CTRL_RST;
	ctrl |= IXGBE_READ_REG(hw, IXGBE_CTRL);
	IXGBE_WRITE_REG(hw, IXGBE_CTRL, ctrl);
	IXGBE_WRITE_FLUSH(hw);
	hw->mac.ops.release_swfw_sync(hw, swfw_mask);

	/* Poll for reset bit to self-clear indicating reset is complete */
	for (i = 0; i < 10; i++) {
		usec_delay(1);
		ctrl = IXGBE_READ_REG(hw, IXGBE_CTRL);
		if (!(ctrl & IXGBE_CTRL_RST_MASK))
			break;
	}

	if (ctrl & IXGBE_CTRL_RST_MASK) {
		status = IXGBE_ERR_RESET_FAILED;
		ERROR_REPORT1(IXGBE_ERROR_POLLING,
			      "Reset polling failed to complete.\n");
	}
	msec_delay(100);

	/*
	 * Double resets are required for recovery from certain error
	 * conditions.  Between resets, it is necessary to stall to allow time
	 * for any pending HW events to complete.
	 */
	if (hw->mac.flags & IXGBE_FLAGS_DOUBLE_RESET_REQUIRED) {
		hw->mac.flags &= ~IXGBE_FLAGS_DOUBLE_RESET_REQUIRED;
		goto mac_reset_top;
	}

	/* Set the Rx packet buffer size. */
	IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(0), 384 << IXGBE_RXPBSIZE_SHIFT);

	/* Store the permanent mac address */
	hw->mac.ops.get_mac_addr(hw, hw->mac.perm_addr);

	/*
	 * Store MAC address from RAR0, clear receive address registers, and
	 * clear the multicast table.  Also reset num_rar_entries to 128,
	 * since we modify this value when programming the SAN MAC address.
	 */
	hw->mac.num_rar_entries = 128;
	hw->mac.ops.init_rx_addrs(hw);

reset_hw_out:
	return status;
}

/**
 * ixgbe_start_hw_E610 - Prepare hardware for Tx/Rx
 * @hw: pointer to hardware structure
 *
 * Gets firmware version and if API version matches it
 * starts the hardware using the generic start_hw function
 * and the generation start_hw function.
 * Then performs revision-specific operations, if any.
 **/
s32 ixgbe_start_hw_E610(struct ixgbe_hw *hw)
{
	s32 ret_val = IXGBE_SUCCESS;

	ret_val = hw->mac.ops.get_fw_version(hw);
	if (ret_val)
		goto out;

	ret_val = ixgbe_start_hw_generic(hw);
	if (ret_val != IXGBE_SUCCESS)
		goto out;

	ixgbe_start_hw_gen2(hw);

out:
	return ret_val;
}

/**
 * ixgbe_get_media_type_E610 - Gets media type
 * @hw: pointer to the HW struct
 *
 * In order to get the media type, the function gets PHY
 * capabilities and later on use them to identify the PHY type
 * checking phy_type_high and phy_type_low.
 *
 * Return: the type of media in form of ixgbe_media_type enum
 * or ixgbe_media_type_unknown in case of an error.
 */
enum ixgbe_media_type ixgbe_get_media_type_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_phy_caps_data pcaps;
	u64 phy_mask = 0;
	s32 rc;
	u8 i;

	rc = ixgbe_update_link_info(hw);
	if (rc) {
		return ixgbe_media_type_unknown;
	}

	/* If there is no link but PHY (dongle) is available SW should use
	 * Get PHY Caps admin command instead of Get Link Status, find most
	 * significant bit that is set in PHY types reported by the command
	 * and use it to discover media type.
	 */
	if (!(hw->link.link_info.link_info & IXGBE_ACI_LINK_UP) &&
	    (hw->link.link_info.link_info & IXGBE_ACI_MEDIA_AVAILABLE)) {
		/* Get PHY Capabilities */
		rc = ixgbe_aci_get_phy_caps(hw, false,
					    IXGBE_ACI_REPORT_TOPO_CAP_MEDIA,
					    &pcaps);
		if (rc) {
			return ixgbe_media_type_unknown;
		}

		/* Check if there is some bit set in phy_type_high */
		for (i = 64; i > 0; i--) {
			phy_mask = (u64)((u64)1 << (i - 1));
			if ((pcaps.phy_type_high & phy_mask) != 0) {
				/* If any bit is set treat it as PHY type */
				hw->link.link_info.phy_type_high = phy_mask;
				hw->link.link_info.phy_type_low = 0;
				break;
			}
			phy_mask = 0;
		}

		/* If nothing found in phy_type_high search in phy_type_low */
		if (phy_mask == 0) {
			for (i = 64; i > 0; i--) {
				phy_mask = (u64)((u64)1 << (i - 1));
				if ((pcaps.phy_type_low & phy_mask) != 0) {
					/* If any bit is set treat it as PHY type */
					hw->link.link_info.phy_type_high = 0;
					hw->link.link_info.phy_type_low = phy_mask;
					break;
				}
			}
		}

	}

	/* Based on link status or search above try to discover media type */
	hw->phy.media_type = ixgbe_get_media_type_from_phy_type(hw);

	return hw->phy.media_type;
}

/**
 * ixgbe_get_supported_physical_layer_E610 - Returns physical layer type
 * @hw: pointer to hardware structure
 *
 * Determines physical layer capabilities of the current configuration.
 *
 * Return: the exit code of the operation.
 **/
u64 ixgbe_get_supported_physical_layer_E610(struct ixgbe_hw *hw)
{
	u64 physical_layer = IXGBE_PHYSICAL_LAYER_UNKNOWN;
	struct ixgbe_aci_cmd_get_phy_caps_data pcaps;
	u64 phy_type;
	s32 rc;

	rc = ixgbe_aci_get_phy_caps(hw, false, IXGBE_ACI_REPORT_TOPO_CAP_MEDIA,
				    &pcaps);
	if (rc)
		return IXGBE_PHYSICAL_LAYER_UNKNOWN;

	phy_type = IXGBE_LE64_TO_CPU(pcaps.phy_type_low);
	if(phy_type & IXGBE_PHY_TYPE_LOW_10GBASE_T)
		physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_T;
	if(phy_type & IXGBE_PHY_TYPE_LOW_1000BASE_T)
		physical_layer |= IXGBE_PHYSICAL_LAYER_1000BASE_T;
	if(phy_type & IXGBE_PHY_TYPE_LOW_100BASE_TX)
		physical_layer |= IXGBE_PHYSICAL_LAYER_100BASE_TX;
	if(phy_type & IXGBE_PHY_TYPE_LOW_10GBASE_LR)
		physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_LR;
	if(phy_type & IXGBE_PHY_TYPE_LOW_10GBASE_SR)
		physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_SR;
	if(phy_type & IXGBE_PHY_TYPE_LOW_1000BASE_KX)
		physical_layer |= IXGBE_PHYSICAL_LAYER_1000BASE_KX;
	if(phy_type & IXGBE_PHY_TYPE_LOW_10GBASE_KR_CR1)
		physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_KR;
	if(phy_type & IXGBE_PHY_TYPE_LOW_1000BASE_SX)
		physical_layer |= IXGBE_PHYSICAL_LAYER_1000BASE_SX;
	if(phy_type & IXGBE_PHY_TYPE_LOW_2500BASE_KX)
		physical_layer |= IXGBE_PHYSICAL_LAYER_2500BASE_KX;
	if(phy_type & IXGBE_PHY_TYPE_LOW_2500BASE_T)
		physical_layer |= IXGBE_PHYSICAL_LAYER_2500BASE_T;
	if(phy_type & IXGBE_PHY_TYPE_LOW_5GBASE_T)
		physical_layer |= IXGBE_PHYSICAL_LAYER_5000BASE_T;

	phy_type = IXGBE_LE64_TO_CPU(pcaps.phy_type_high);
	if(phy_type & IXGBE_PHY_TYPE_HIGH_10BASE_T)
		physical_layer |= IXGBE_PHYSICAL_LAYER_10BASE_T;

	return physical_layer;
}

/**
 * ixgbe_setup_link_E610 - Set up link
 * @hw: pointer to hardware structure
 * @speed: new link speed
 * @autoneg_wait: true when waiting for completion is needed
 *
 * Set up the link with the specified speed.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_setup_link_E610(struct ixgbe_hw *hw, ixgbe_link_speed speed,
			  bool autoneg_wait)
{
	/* Simply request FW to perform proper PHY setup */
	return hw->phy.ops.setup_link_speed(hw, speed, autoneg_wait);
}

/**
 * ixgbe_check_link_E610 - Determine link and speed status
 * @hw: pointer to hardware structure
 * @speed: pointer to link speed
 * @link_up: true when link is up
 * @link_up_wait_to_complete: bool used to wait for link up or not
 *
 * Determine if the link is up and the current link speed
 * using ACI command (0x0607).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_check_link_E610(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
			  bool *link_up, bool link_up_wait_to_complete)
{
	s32 rc;
	u32 i;

	if (!speed || !link_up)
		return IXGBE_ERR_PARAM;

	/* Set get_link_info flag to ensure that fresh
	 * link information will be obtained from FW
	 * by sending Get Link Status admin command. */
	hw->link.get_link_info = true;

	/* Update link information in adapter context. */
	rc = ixgbe_get_link_status(hw, link_up);
	if (rc)
		return rc;

	/* Wait for link up if it was requested. */
	if (link_up_wait_to_complete && *link_up == false) {
		for (i = 0; i < hw->mac.max_link_up_time; i++) {
			msec_delay(100);
			hw->link.get_link_info = true;
			rc = ixgbe_get_link_status(hw, link_up);
			if (rc)
				return rc;
			if (*link_up)
				break;
		}
	}

	/* Use link information in adapter context updated by the call
	 * to ixgbe_get_link_status() to determine current link speed.
	 * Link speed information is valid only when link up was
	 * reported by FW. */
	if (*link_up) {
		switch (hw->link.link_info.link_speed) {
		case IXGBE_ACI_LINK_SPEED_10MB:
			*speed = IXGBE_LINK_SPEED_10_FULL;
			break;
		case IXGBE_ACI_LINK_SPEED_100MB:
			*speed = IXGBE_LINK_SPEED_100_FULL;
			break;
		case IXGBE_ACI_LINK_SPEED_1000MB:
			*speed = IXGBE_LINK_SPEED_1GB_FULL;
			break;
		case IXGBE_ACI_LINK_SPEED_2500MB:
			*speed = IXGBE_LINK_SPEED_2_5GB_FULL;
			break;
		case IXGBE_ACI_LINK_SPEED_5GB:
			*speed = IXGBE_LINK_SPEED_5GB_FULL;
			break;
		case IXGBE_ACI_LINK_SPEED_10GB:
			*speed = IXGBE_LINK_SPEED_10GB_FULL;
			break;
		default:
			*speed = IXGBE_LINK_SPEED_UNKNOWN;
			break;
		}
	} else {
		*speed = IXGBE_LINK_SPEED_UNKNOWN;
	}

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_get_link_capabilities_E610 - Determine link capabilities
 * @hw: pointer to hardware structure
 * @speed: pointer to link speed
 * @autoneg: true when autoneg or autotry is enabled
 *
 * Determine speed and AN parameters of a link.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_get_link_capabilities_E610(struct ixgbe_hw *hw,
				     ixgbe_link_speed *speed,
				     bool *autoneg)
{
	if (!speed || !autoneg)
		return IXGBE_ERR_PARAM;

	*autoneg = true;
	*speed = hw->phy.speeds_supported;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_cfg_phy_fc - Configure PHY Flow Control (FC) data based on FC mode
 * @hw: pointer to hardware structure
 * @cfg: PHY configuration data to set FC mode
 * @req_mode: FC mode to configure
 *
 * Configures PHY Flow Control according to the provided configuration.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_cfg_phy_fc(struct ixgbe_hw *hw,
		     struct ixgbe_aci_cmd_set_phy_cfg_data *cfg,
		     enum ixgbe_fc_mode req_mode)
{
	struct ixgbe_aci_cmd_get_phy_caps_data* pcaps = NULL;
	s32 status = IXGBE_SUCCESS;
	u8 pause_mask = 0x0;

	if (!cfg)
		return IXGBE_ERR_PARAM;

	switch (req_mode) {
	case ixgbe_fc_auto:
	{
		pcaps = (struct ixgbe_aci_cmd_get_phy_caps_data *)
			ixgbe_malloc(hw, sizeof(*pcaps));
		if (!pcaps) {
			status = IXGBE_ERR_OUT_OF_MEM;
			goto out;
		}

		/* Query the value of FC that both the NIC and the attached
		 * media can do. */
		status = ixgbe_aci_get_phy_caps(hw, false,
			IXGBE_ACI_REPORT_TOPO_CAP_MEDIA, pcaps);
		if (status)
			goto out;

		pause_mask |= pcaps->caps & IXGBE_ACI_PHY_EN_TX_LINK_PAUSE;
		pause_mask |= pcaps->caps & IXGBE_ACI_PHY_EN_RX_LINK_PAUSE;

		break;
	}
	case ixgbe_fc_full:
		pause_mask |= IXGBE_ACI_PHY_EN_TX_LINK_PAUSE;
		pause_mask |= IXGBE_ACI_PHY_EN_RX_LINK_PAUSE;
		break;
	case ixgbe_fc_rx_pause:
		pause_mask |= IXGBE_ACI_PHY_EN_RX_LINK_PAUSE;
		break;
	case ixgbe_fc_tx_pause:
		pause_mask |= IXGBE_ACI_PHY_EN_TX_LINK_PAUSE;
		break;
	default:
		break;
	}

	/* clear the old pause settings */
	cfg->caps &= ~(IXGBE_ACI_PHY_EN_TX_LINK_PAUSE |
		IXGBE_ACI_PHY_EN_RX_LINK_PAUSE);

	/* set the new capabilities */
	cfg->caps |= pause_mask;

out:
	if (pcaps)
		ixgbe_free(hw, pcaps);
	return status;
}

/**
 * ixgbe_setup_fc_E610 - Set up flow control
 * @hw: pointer to hardware structure
 *
 * Set up flow control. This has to be done during init time.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_setup_fc_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_phy_caps_data pcaps = { 0 };
	struct ixgbe_aci_cmd_set_phy_cfg_data cfg = { 0 };
	s32 status;

	/* Get the current PHY config */
	status = ixgbe_aci_get_phy_caps(hw, false,
		IXGBE_ACI_REPORT_ACTIVE_CFG, &pcaps);
	if (status)
		return status;

	ixgbe_copy_phy_caps_to_cfg(&pcaps, &cfg);

	/* Configure the set PHY data */
	status = ixgbe_cfg_phy_fc(hw, &cfg, hw->fc.requested_mode);
	if (status)
		return status;

	/* If the capabilities have changed, then set the new config */
	if (cfg.caps != pcaps.caps) {
		cfg.caps |= IXGBE_ACI_PHY_ENA_AUTO_LINK_UPDT;

		status = ixgbe_aci_set_phy_cfg(hw, &cfg);
		if (status)
			return status;
	}

	return status;
}

/**
 * ixgbe_fc_autoneg_E610 - Configure flow control
 * @hw: pointer to hardware structure
 *
 * Configure Flow Control.
 */
void ixgbe_fc_autoneg_E610(struct ixgbe_hw *hw)
{
	s32 status;

	/* Get current link status.
	 * Current FC mode will be stored in the hw context. */
	status = ixgbe_aci_get_link_info(hw, false, NULL);
	if (status) {
		goto out;
	}

	/* Check if the link is up */
	if (!(hw->link.link_info.link_info & IXGBE_ACI_LINK_UP)) {
		status = IXGBE_ERR_FC_NOT_NEGOTIATED;
		goto out;
	}

	/* Check if auto-negotiation has completed */
	if (!(hw->link.link_info.an_info & IXGBE_ACI_AN_COMPLETED)) {
		status = IXGBE_ERR_FC_NOT_NEGOTIATED;
		goto out;
	}

out:
	if (status == IXGBE_SUCCESS) {
		hw->fc.fc_was_autonegged = true;
	} else {
		hw->fc.fc_was_autonegged = false;
		hw->fc.current_mode = hw->fc.requested_mode;
	}
}

/**
 * ixgbe_set_fw_drv_ver_E610 - Send driver version to FW
 * @hw: pointer to the HW structure
 * @maj: driver version major number
 * @minor: driver version minor number
 * @build: driver version build number
 * @sub: driver version sub build number
 * @len: length of driver_ver string
 * @driver_ver: driver string
 *
 * Send driver version number to Firmware using ACI command (0x0002).
 *
 * Return: the exit code of the operation.
 * IXGBE_SUCCESS - OK
 * IXGBE_ERR_PARAM - incorrect parameters were given
 * IXGBE_ERR_ACI_ERROR - encountered an error during sending the command
 * IXGBE_ERR_ACI_TIMEOUT - a timeout occurred
 * IXGBE_ERR_OUT_OF_MEM - ran out of memory
 */
s32 ixgbe_set_fw_drv_ver_E610(struct ixgbe_hw *hw, u8 maj, u8 minor, u8 build,
			      u8 sub, u16 len, const char *driver_ver)
{
	size_t limited_len = min(len, (u16)IXGBE_DRV_VER_STR_LEN_E610);
	struct ixgbe_driver_ver dv;

	DEBUGFUNC("ixgbe_set_fw_drv_ver_E610");

	if (!len || !driver_ver)
		return IXGBE_ERR_PARAM;

	dv.major_ver = maj;
	dv.minor_ver = minor;
	dv.build_ver = build;
	dv.subbuild_ver = sub;

	memset(dv.driver_string, 0, IXGBE_DRV_VER_STR_LEN_E610);
	memcpy(dv.driver_string, driver_ver, limited_len);

	return ixgbe_aci_send_driver_ver(hw, &dv);
}

/**
 * ixgbe_disable_rx_E610 - Disable RX unit
 * @hw: pointer to hardware structure
 *
 * Disable RX DMA unit on E610 with use of ACI command (0x000C).
 *
 * Return: the exit code of the operation.
 */
void ixgbe_disable_rx_E610(struct ixgbe_hw *hw)
{
	u32 rxctrl;

	DEBUGFUNC("ixgbe_disable_rx_E610");

	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	if (rxctrl & IXGBE_RXCTRL_RXEN) {
		u32 pfdtxgswc;
		s32 status;

		pfdtxgswc = IXGBE_READ_REG(hw, IXGBE_PFDTXGSWC);
		if (pfdtxgswc & IXGBE_PFDTXGSWC_VT_LBEN) {
			pfdtxgswc &= ~IXGBE_PFDTXGSWC_VT_LBEN;
			IXGBE_WRITE_REG(hw, IXGBE_PFDTXGSWC, pfdtxgswc);
			hw->mac.set_lben = true;
		} else {
			hw->mac.set_lben = false;
		}

		status = ixgbe_aci_disable_rxen(hw);

		/* If we fail - disable RX using register write */
		if (status) {
			rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
			if (rxctrl & IXGBE_RXCTRL_RXEN) {
				rxctrl &= ~IXGBE_RXCTRL_RXEN;
				IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, rxctrl);
			}
		}
	}
}

/**
 * ixgbe_setup_eee_E610 - Enable/disable EEE support
 * @hw: pointer to the HW structure
 * @enable_eee: boolean flag to enable EEE
 *
 * Enables/disable EEE based on enable_eee flag.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_setup_eee_E610(struct ixgbe_hw *hw, bool enable_eee)
{
	struct ixgbe_aci_cmd_get_phy_caps_data phy_caps = { 0 };
	struct ixgbe_aci_cmd_set_phy_cfg_data phy_cfg = { 0 };
	u16 eee_cap = 0;
	s32 status;

	status = ixgbe_aci_get_phy_caps(hw, false,
		IXGBE_ACI_REPORT_ACTIVE_CFG, &phy_caps);
	if (status != IXGBE_SUCCESS)
		return status;

	ixgbe_copy_phy_caps_to_cfg(&phy_caps, &phy_cfg);

	phy_cfg.caps |= IXGBE_ACI_PHY_ENA_LINK;
	phy_cfg.caps |= IXGBE_ACI_PHY_ENA_AUTO_LINK_UPDT;

	if (enable_eee) {
		if (phy_caps.phy_type_low & IXGBE_PHY_TYPE_LOW_100BASE_TX)
			eee_cap |= IXGBE_ACI_PHY_EEE_EN_100BASE_TX;
		if (phy_caps.phy_type_low & IXGBE_PHY_TYPE_LOW_1000BASE_T)
			eee_cap |= IXGBE_ACI_PHY_EEE_EN_1000BASE_T;
		if (phy_caps.phy_type_low & IXGBE_PHY_TYPE_LOW_1000BASE_KX)
			eee_cap |= IXGBE_ACI_PHY_EEE_EN_1000BASE_KX;
		if (phy_caps.phy_type_low & IXGBE_PHY_TYPE_LOW_10GBASE_T)
			eee_cap |= IXGBE_ACI_PHY_EEE_EN_10GBASE_T;
		if (phy_caps.phy_type_low & IXGBE_PHY_TYPE_LOW_10GBASE_KR_CR1)
			eee_cap |= IXGBE_ACI_PHY_EEE_EN_10GBASE_KR;
		if (phy_caps.phy_type_high & IXGBE_PHY_TYPE_HIGH_10BASE_T)
			eee_cap |= IXGBE_ACI_PHY_EEE_EN_10BASE_T;
	}

	/* Set EEE capability for particular PHY types */
	phy_cfg.eee_cap = IXGBE_CPU_TO_LE16(eee_cap);

	status = ixgbe_aci_set_phy_cfg(hw, &phy_cfg);

	return status;
}

/**
 * ixgbe_fw_recovery_mode_E610 - Check FW NVM recovery mode
 * @hw: pointer to hardware structure
 *
 * Checks FW NVM recovery mode by
 * reading the value of the dedicated register.
 *
 * Return: true if FW is in recovery mode, otherwise false.
 */
bool ixgbe_fw_recovery_mode_E610(struct ixgbe_hw *hw)
{
	u32 fwsm = IXGBE_READ_REG(hw, GL_MNG_FWSM);

	return !!(fwsm & GL_MNG_FWSM_FW_MODES_RECOVERY_M);
}

/**
 * ixgbe_fw_rollback_mode_E610 - Check FW NVM Rollback
 * @hw: pointer to hardware structure
 *
 * Checks FW NVM Rollback mode by reading the
 * value of the dedicated register.
 *
 * Return: true if FW is in Rollback mode, otherwise false.
 */
bool ixgbe_fw_rollback_mode_E610(struct ixgbe_hw *hw)
{
	u32 fwsm = IXGBE_READ_REG(hw, GL_MNG_FWSM);

	return !!(fwsm & GL_MNG_FWSM_FW_MODES_ROLLBACK_M);
}

/**
 * ixgbe_get_fw_tsam_mode_E610 - Check FW NVM Thermal Sensor Autonomous Mode
 * @hw: pointer to hardware structure
 *
 * Checks Thermal Sensor Autonomous Mode by reading the
 * value of the dedicated register.
 *
 * Return: true if FW is in TSAM, otherwise false.
 */
bool ixgbe_get_fw_tsam_mode_E610(struct ixgbe_hw *hw)
{
	u32 fwsm = IXGBE_READ_REG(hw, IXGBE_FWSM_X550EM_a);

	return !!(fwsm & IXGBE_FWSM_TS_ENABLED);
}

/**
 * ixgbe_init_phy_ops_E610 - PHY specific init
 * @hw: pointer to hardware structure
 *
 * Initialize any function pointers that were not able to be
 * set during init_shared_code because the PHY type was not known.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_init_phy_ops_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;
	struct ixgbe_phy_info *phy = &hw->phy;
	s32 ret_val;

	phy->ops.identify_sfp = ixgbe_identify_module_E610;
	phy->ops.read_reg = NULL; /* PHY reg access is not required */
	phy->ops.write_reg = NULL;
	phy->ops.read_reg_mdi = NULL;
	phy->ops.write_reg_mdi = NULL;
	phy->ops.setup_link = ixgbe_setup_phy_link_E610;
	phy->ops.get_firmware_version = ixgbe_get_phy_firmware_version_E610;
	phy->ops.read_i2c_byte = NULL; /* disabled for E610 */
	phy->ops.write_i2c_byte = NULL; /* disabled for E610 */
	phy->ops.read_i2c_sff8472 = ixgbe_read_i2c_sff8472_E610;
	phy->ops.read_i2c_eeprom = ixgbe_read_i2c_eeprom_E610;
	phy->ops.write_i2c_eeprom = ixgbe_write_i2c_eeprom_E610;
	phy->ops.i2c_bus_clear = NULL; /* do not use generic implementation  */
	phy->ops.check_overtemp = ixgbe_check_overtemp_E610;
	if (mac->ops.get_media_type(hw) == ixgbe_media_type_copper)
		phy->ops.set_phy_power = ixgbe_set_phy_power_E610;
	else
		phy->ops.set_phy_power = NULL;
	phy->ops.enter_lplu = ixgbe_enter_lplu_E610;
	phy->ops.handle_lasi = NULL; /* no implementation for E610 */
	phy->ops.read_i2c_byte_unlocked = NULL; /* disabled for E610 */
	phy->ops.write_i2c_byte_unlocked = NULL; /* disabled for E610 */

	/* TODO: Set functions pointers based on device ID */

	/* Identify the PHY */
	ret_val = phy->ops.identify(hw);
	if (ret_val != IXGBE_SUCCESS)
		return ret_val;

	/* TODO: Set functions pointers based on PHY type */

	return ret_val;
}

/**
 * ixgbe_identify_phy_E610 - Identify PHY
 * @hw: pointer to hardware structure
 * 
 * Determine PHY type, supported speeds and PHY ID.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_identify_phy_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_phy_caps_data pcaps;
	s32 rc;

	/* Set PHY type */
	hw->phy.type = ixgbe_phy_fw;

	rc = ixgbe_aci_get_phy_caps(hw, false, IXGBE_ACI_REPORT_TOPO_CAP_MEDIA,
				    &pcaps);
	if (rc)
		return rc;

	if (!(pcaps.module_compliance_enforcement &
	      IXGBE_ACI_MOD_ENFORCE_STRICT_MODE)) {
		/* Handle lenient mode */
		rc = ixgbe_aci_get_phy_caps(hw, false,
					    IXGBE_ACI_REPORT_TOPO_CAP_NO_MEDIA,
					    &pcaps);
		if (rc)
			return rc;
	}

	/* Determine supported speeds */
	hw->phy.speeds_supported = IXGBE_LINK_SPEED_UNKNOWN;

	if (pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_10BASE_T ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_10M_SGMII)
		hw->phy.speeds_supported |= IXGBE_LINK_SPEED_10_FULL;
	if (pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_100BASE_TX ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_100M_SGMII ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_100M_USXGMII)
		hw->phy.speeds_supported |= IXGBE_LINK_SPEED_100_FULL;
	if (pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_1000BASE_T  ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_1000BASE_SX ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_1000BASE_LX ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_1000BASE_KX ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_1G_SGMII    ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_1G_USXGMII)
		hw->phy.speeds_supported |= IXGBE_LINK_SPEED_1GB_FULL;
	if (pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10GBASE_T       ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10G_SFI_DA      ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10GBASE_SR      ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10GBASE_LR      ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10GBASE_KR_CR1  ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10G_SFI_AOC_ACC ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_10G_SFI_C2C     ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_10G_USXGMII)
		hw->phy.speeds_supported |= IXGBE_LINK_SPEED_10GB_FULL;

	/* 2.5 and 5 Gbps link speeds must be excluded from the
	 * auto-negotiation set used during driver initialization due to
	 * compatibility issues with certain switches. Those issues do not
	 * exist in case of E610 2.5G SKU device (0x57b1).
	 */
	if (!hw->phy.autoneg_advertised &&
	    hw->device_id != IXGBE_DEV_ID_E610_2_5G_T)
		hw->phy.autoneg_advertised = hw->phy.speeds_supported;

	if (pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_2500BASE_T   ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_2500BASE_X   ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_2500BASE_KX  ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_2500M_SGMII ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_2500M_USXGMII)
		hw->phy.speeds_supported |= IXGBE_LINK_SPEED_2_5GB_FULL;

	if (!hw->phy.autoneg_advertised &&
	    hw->device_id == IXGBE_DEV_ID_E610_2_5G_T)
		hw->phy.autoneg_advertised = hw->phy.speeds_supported;

	if (pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_5GBASE_T  ||
	    pcaps.phy_type_low  & IXGBE_PHY_TYPE_LOW_5GBASE_KR ||
	    pcaps.phy_type_high & IXGBE_PHY_TYPE_HIGH_5G_USXGMII)
		hw->phy.speeds_supported |= IXGBE_LINK_SPEED_5GB_FULL;

	/* Set PHY ID */
	memcpy(&hw->phy.id, pcaps.phy_id_oui, sizeof(u32));

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_identify_module_E610 - Identify SFP module type
 * @hw: pointer to hardware structure
 *
 * Identify the SFP module type.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_identify_module_E610(struct ixgbe_hw *hw)
{
	bool media_available;
	u8 module_type;
	s32 rc;

	rc = ixgbe_update_link_info(hw);
	if (rc)
		goto err;

	media_available =
		(hw->link.link_info.link_info &
		 IXGBE_ACI_MEDIA_AVAILABLE) ? true : false;

	if (media_available) {
		hw->phy.sfp_type = ixgbe_sfp_type_unknown;

		/* Get module type from hw context updated by ixgbe_update_link_info() */
		module_type = hw->link.link_info.module_type[IXGBE_ACI_MOD_TYPE_IDENT];

		if ((module_type & IXGBE_ACI_MOD_TYPE_BYTE1_SFP_PLUS_CU_PASSIVE) ||
		    (module_type & IXGBE_ACI_MOD_TYPE_BYTE1_SFP_PLUS_CU_ACTIVE)) {
			hw->phy.sfp_type = ixgbe_sfp_type_da_cu;
		} else if (module_type & IXGBE_ACI_MOD_TYPE_BYTE1_10G_BASE_SR) {
			hw->phy.sfp_type = ixgbe_sfp_type_sr;
		} else if ((module_type & IXGBE_ACI_MOD_TYPE_BYTE1_10G_BASE_LR) ||
			   (module_type & IXGBE_ACI_MOD_TYPE_BYTE1_10G_BASE_LRM)) {
			hw->phy.sfp_type = ixgbe_sfp_type_lr;
		}
		rc = IXGBE_SUCCESS;
	} else {
		hw->phy.sfp_type = ixgbe_sfp_type_not_present;
		rc = IXGBE_ERR_SFP_NOT_PRESENT;
	}
err:
	return rc;
}

/**
 * ixgbe_setup_phy_link_E610 - Sets up firmware-controlled PHYs
 * @hw: pointer to hardware structure
 *
 * Set the parameters for the firmware-controlled PHYs.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_setup_phy_link_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_phy_caps_data pcaps;
	struct ixgbe_aci_cmd_set_phy_cfg_data pcfg;
	u8 rmode = IXGBE_ACI_REPORT_TOPO_CAP_MEDIA;
	u64 sup_phy_type_low, sup_phy_type_high;
	s32 rc;

	rc = ixgbe_aci_get_link_info(hw, false, NULL);
	if (rc) {
		goto err;
	}

	/* If media is not available get default config */
	if (!(hw->link.link_info.link_info & IXGBE_ACI_MEDIA_AVAILABLE))
		rmode = IXGBE_ACI_REPORT_DFLT_CFG;

	rc = ixgbe_aci_get_phy_caps(hw, false, rmode, &pcaps);
	if (rc) {
		goto err;
	}

	sup_phy_type_low = pcaps.phy_type_low;
	sup_phy_type_high = pcaps.phy_type_high;

	/* Get Active configuration to avoid unintended changes */
	rc = ixgbe_aci_get_phy_caps(hw, false, IXGBE_ACI_REPORT_ACTIVE_CFG,
				    &pcaps);
	if (rc) {
		goto err;
	}
	ixgbe_copy_phy_caps_to_cfg(&pcaps, &pcfg);

	/* Set default PHY types for a given speed */
	pcfg.phy_type_low = 0;
	pcfg.phy_type_high = 0;

	if (hw->phy.autoneg_advertised & IXGBE_LINK_SPEED_10_FULL) {
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_10BASE_T;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_10M_SGMII;
	}
	if (hw->phy.autoneg_advertised & IXGBE_LINK_SPEED_100_FULL) {
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_100BASE_TX;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_100M_SGMII;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_100M_USXGMII;
	}
	if (hw->phy.autoneg_advertised & IXGBE_LINK_SPEED_1GB_FULL) {
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_1000BASE_T;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_1000BASE_SX;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_1000BASE_LX;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_1000BASE_KX;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_1G_SGMII;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_1G_USXGMII;
	}
	if (hw->phy.autoneg_advertised & IXGBE_LINK_SPEED_2_5GB_FULL) {
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_2500BASE_T;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_2500BASE_X;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_2500BASE_KX;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_2500M_SGMII;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_2500M_USXGMII;
	}
	if (hw->phy.autoneg_advertised & IXGBE_LINK_SPEED_5GB_FULL) {
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_5GBASE_T;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_5GBASE_KR;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_5G_USXGMII;
	}
	if (hw->phy.autoneg_advertised & IXGBE_LINK_SPEED_10GB_FULL) {
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10GBASE_T;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10G_SFI_DA;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10GBASE_SR;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10GBASE_LR;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10GBASE_KR_CR1;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10G_SFI_AOC_ACC;
		pcfg.phy_type_low  |= IXGBE_PHY_TYPE_LOW_10G_SFI_C2C;
		pcfg.phy_type_high |= IXGBE_PHY_TYPE_HIGH_10G_USXGMII;
	}

	/* Mask the set values to avoid requesting unsupported link types */
	pcfg.phy_type_low &= sup_phy_type_low;
	pcfg.phy_type_high &= sup_phy_type_high;

	if (pcfg.phy_type_high != pcaps.phy_type_high ||
	    pcfg.phy_type_low != pcaps.phy_type_low ||
	    pcfg.caps != pcaps.caps) {
		pcfg.caps |= IXGBE_ACI_PHY_ENA_LINK;
		pcfg.caps |= IXGBE_ACI_PHY_ENA_AUTO_LINK_UPDT;

		rc = ixgbe_aci_set_phy_cfg(hw, &pcfg);
	}

err:
	return rc;
}

/**
 * ixgbe_get_phy_firmware_version_E610 - Gets the PHY Firmware Version
 * @hw: pointer to hardware structure
 * @firmware_version: pointer to the PHY Firmware Version
 * 
 * Determines PHY FW version based on response to Get PHY Capabilities
 * admin command (0x0600).
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_get_phy_firmware_version_E610(struct ixgbe_hw *hw,
					u16 *firmware_version)
{
	struct ixgbe_aci_cmd_get_phy_caps_data pcaps;
	s32 status;

	if (!firmware_version)
		return IXGBE_ERR_PARAM;

	status = ixgbe_aci_get_phy_caps(hw, false,
					IXGBE_ACI_REPORT_ACTIVE_CFG,
					&pcaps);
	if (status)
		return status;

	/* TODO: determine which bytes of the 8-byte phy_fw_ver
	 * field should be written to the 2-byte firmware_version
	 * output argument. */
	memcpy(firmware_version, pcaps.phy_fw_ver, sizeof(u16));

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_read_i2c_sff8472_E610 - Reads 8 bit word over I2C interface
 * @hw: pointer to hardware structure
 * @byte_offset: byte offset at address 0xA2
 * @sff8472_data: value read
 *
 * Performs byte read operation from SFP module's SFF-8472 data over I2C.
 *
 * Return: the exit code of the operation.
 **/
s32 ixgbe_read_i2c_sff8472_E610(struct ixgbe_hw *hw, u8 byte_offset,
				u8 *sff8472_data)
{
	return ixgbe_aci_sff_eeprom(hw, 0, IXGBE_I2C_EEPROM_DEV_ADDR2,
				    byte_offset, 0,
				    IXGBE_ACI_SFF_NO_PAGE_BANK_UPDATE,
				    sff8472_data, 1, false);
}

/**
 * ixgbe_read_i2c_eeprom_E610 - Reads 8 bit EEPROM word over I2C interface
 * @hw: pointer to hardware structure
 * @byte_offset: EEPROM byte offset to read
 * @eeprom_data: value read
 *
 * Performs byte read operation from SFP module's EEPROM over I2C interface.
 *
 * Return: the exit code of the operation.
 **/
s32 ixgbe_read_i2c_eeprom_E610(struct ixgbe_hw *hw, u8 byte_offset,
			       u8 *eeprom_data)
{
	return ixgbe_aci_sff_eeprom(hw, 0, IXGBE_I2C_EEPROM_DEV_ADDR,
				    byte_offset, 0,
				    IXGBE_ACI_SFF_NO_PAGE_BANK_UPDATE,
				    eeprom_data, 1, false);
}

/**
 * ixgbe_write_i2c_eeprom_E610 - Writes 8 bit EEPROM word over I2C interface
 * @hw: pointer to hardware structure
 * @byte_offset: EEPROM byte offset to write
 * @eeprom_data: value to write
 *
 * Performs byte write operation to SFP module's EEPROM over I2C interface.
 *
 * Return: the exit code of the operation.
 **/
s32 ixgbe_write_i2c_eeprom_E610(struct ixgbe_hw *hw, u8 byte_offset,
				u8 eeprom_data)
{
	return ixgbe_aci_sff_eeprom(hw, 0, IXGBE_I2C_EEPROM_DEV_ADDR,
				    byte_offset, 0,
				    IXGBE_ACI_SFF_NO_PAGE_BANK_UPDATE,
				    &eeprom_data, 1, true);
}

/**
 * ixgbe_check_overtemp_E610 - Check firmware-controlled PHYs for overtemp
 * @hw: pointer to hardware structure
 *
 * Get the link status and check if the PHY temperature alarm detected.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_check_overtemp_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_link_status_data link_data = { 0 };
	struct ixgbe_aci_cmd_get_link_status *resp;
	struct ixgbe_aci_desc desc;
	s32 status = IXGBE_SUCCESS;

	if (!hw)
		return IXGBE_ERR_PARAM;

	ixgbe_fill_dflt_direct_cmd_desc(&desc, ixgbe_aci_opc_get_link_status);
	resp = &desc.params.get_link_status;
	resp->cmd_flags = IXGBE_CPU_TO_LE16(IXGBE_ACI_LSE_NOP);

	status = ixgbe_aci_send_cmd(hw, &desc, &link_data, sizeof(link_data));
	if (status != IXGBE_SUCCESS)
		return status;

	if (link_data.ext_info & IXGBE_ACI_LINK_PHY_TEMP_ALARM) {
		ERROR_REPORT1(IXGBE_ERROR_CAUTION,
			      "PHY Temperature Alarm detected");
		status = IXGBE_ERR_OVERTEMP;
	}

	return status;
}

/**
 * ixgbe_set_phy_power_E610 - Control power for copper PHY
 * @hw: pointer to hardware structure
 * @on: true for on, false for off
 *
 * Set the power on/off of the PHY
 * by getting its capabilities and setting the appropriate
 * configuration parameters.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_set_phy_power_E610(struct ixgbe_hw *hw, bool on)
{
	struct ixgbe_aci_cmd_get_phy_caps_data phy_caps = { 0 };
	struct ixgbe_aci_cmd_set_phy_cfg_data phy_cfg = { 0 };
	s32 status;

	status = ixgbe_aci_get_phy_caps(hw, false,
		IXGBE_ACI_REPORT_ACTIVE_CFG, &phy_caps);
	if (status != IXGBE_SUCCESS)
		return status;

	ixgbe_copy_phy_caps_to_cfg(&phy_caps, &phy_cfg);

	if (on) {
		phy_cfg.caps &= ~IXGBE_ACI_PHY_ENA_LOW_POWER;
	} else {
		phy_cfg.caps |= IXGBE_ACI_PHY_ENA_LOW_POWER;
	}

	/* PHY is already in requested power mode */
	if (phy_caps.caps == phy_cfg.caps)
		return IXGBE_SUCCESS;

	phy_cfg.caps |= IXGBE_ACI_PHY_ENA_LINK;
	phy_cfg.caps |= IXGBE_ACI_PHY_ENA_AUTO_LINK_UPDT;

	status = ixgbe_aci_set_phy_cfg(hw, &phy_cfg);

	return status;
}

/**
 * ixgbe_enter_lplu_E610 - Transition to low power states
 * @hw: pointer to hardware structure
 *
 * Configures Low Power Link Up on transition to low power states
 * (from D0 to non-D0). Link is required to enter LPLU so avoid resetting the
 * X557 PHY immediately prior to entering LPLU.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_enter_lplu_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_aci_cmd_get_phy_caps_data phy_caps = { 0 };
	struct ixgbe_aci_cmd_set_phy_cfg_data phy_cfg = { 0 };
	s32 status;

	status = ixgbe_aci_get_phy_caps(hw, false,
		IXGBE_ACI_REPORT_ACTIVE_CFG, &phy_caps);
	if (status != IXGBE_SUCCESS)
		return status;

	ixgbe_copy_phy_caps_to_cfg(&phy_caps, &phy_cfg);

	phy_cfg.low_power_ctrl_an |= IXGBE_ACI_PHY_EN_D3COLD_LOW_POWER_AUTONEG;

	status = ixgbe_aci_set_phy_cfg(hw, &phy_cfg);

	return status;
}

/**
 * ixgbe_init_eeprom_params_E610 - Initialize EEPROM params
 * @hw: pointer to hardware structure
 *
 * Initializes the EEPROM parameters ixgbe_eeprom_info within the
 * ixgbe_hw struct in order to set up EEPROM access.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_init_eeprom_params_E610(struct ixgbe_hw *hw)
{
	struct ixgbe_eeprom_info *eeprom = &hw->eeprom;
	u32 gens_stat;
	u8 sr_size;

	if (eeprom->type == ixgbe_eeprom_uninitialized) {
		eeprom->type = ixgbe_flash;

		gens_stat = IXGBE_READ_REG(hw, GLNVM_GENS);
		sr_size = (gens_stat & GLNVM_GENS_SR_SIZE_M) >>
			  GLNVM_GENS_SR_SIZE_S;

		/* Switching to words (sr_size contains power of 2) */
		eeprom->word_size = BIT(sr_size) * IXGBE_SR_WORDS_IN_1KB;

		DEBUGOUT2("Eeprom params: type = %d, size = %d\n",
			  eeprom->type, eeprom->word_size);
	}

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_read_ee_aci_E610 - Read EEPROM word using the admin command.
 * @hw: pointer to hardware structure
 * @offset: offset of  word in the EEPROM to read
 * @data: word read from the EEPROM
 *
 * Reads a 16 bit word from the EEPROM using the ACI.
 * If the EEPROM params are not initialized, the function
 * initialize them before proceeding with reading.
 * The function acquires and then releases the NVM ownership.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_read_ee_aci_E610(struct ixgbe_hw *hw, u16 offset, u16 *data)
{
	s32 status;

	if (hw->eeprom.type == ixgbe_eeprom_uninitialized) {
		status = ixgbe_init_eeprom_params(hw);
		if (status)
			return status;
	}

	status = ixgbe_acquire_nvm(hw, IXGBE_RES_READ);
	if (status)
		return status;

	status = ixgbe_read_sr_word_aci(hw, offset, data);
	ixgbe_release_nvm(hw);

	return status;
}

/**
 * ixgbe_read_ee_aci_buffer_E610- Read EEPROM word(s) using admin commands.
 * @hw: pointer to hardware structure
 * @offset: offset of  word in the EEPROM to read
 * @words: number of words
 * @data: word(s) read from the EEPROM
 *
 * Reads a 16 bit word(s) from the EEPROM using the ACI.
 * If the EEPROM params are not initialized, the function
 * initialize them before proceeding with reading.
 * The function acquires and then releases the NVM ownership.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_read_ee_aci_buffer_E610(struct ixgbe_hw *hw, u16 offset,
				  u16 words, u16 *data)
{
	s32 status;

	if (hw->eeprom.type == ixgbe_eeprom_uninitialized) {
		status = ixgbe_init_eeprom_params(hw);
		if (status)
			return status;
	}

	status = ixgbe_acquire_nvm(hw, IXGBE_RES_READ);
	if (status)
		return status;

	status = ixgbe_read_sr_buf_aci(hw, offset, &words, data);
	ixgbe_release_nvm(hw);

	return status;
}

/**
 * ixgbe_write_ee_aci_E610 - Write EEPROM word using the admin command.
 * @hw: pointer to hardware structure
 * @offset: offset of  word in the EEPROM to write
 * @data: word write to the EEPROM
 *
 * Write a 16 bit word to the EEPROM using the ACI.
 * If the EEPROM params are not initialized, the function
 * initialize them before proceeding with writing.
 * The function acquires and then releases the NVM ownership.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_write_ee_aci_E610(struct ixgbe_hw *hw, u16 offset, u16 data)
{
	s32 status;

	if (hw->eeprom.type == ixgbe_eeprom_uninitialized) {
		status = ixgbe_init_eeprom_params(hw);
		if (status)
			return status;
	}

	status = ixgbe_acquire_nvm(hw, IXGBE_RES_WRITE);
	if (status)
		return status;

	status = ixgbe_write_sr_word_aci(hw, (u32)offset, &data);
	ixgbe_release_nvm(hw);

	return status;
}

/**
 * ixgbe_write_ee_aci_buffer_E610 - Write EEPROM word(s) using admin commands.
 * @hw: pointer to hardware structure
 * @offset: offset of  word in the EEPROM to write
 * @words: number of words
 * @data: word(s) write to the EEPROM
 *
 * Write a 16 bit word(s) to the EEPROM using the ACI.
 * If the EEPROM params are not initialized, the function
 * initialize them before proceeding with writing.
 * The function acquires and then releases the NVM ownership.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_write_ee_aci_buffer_E610(struct ixgbe_hw *hw, u16 offset,
				   u16 words, u16 *data)
{
	s32 status;

	if (hw->eeprom.type == ixgbe_eeprom_uninitialized) {
		status = ixgbe_init_eeprom_params(hw);
		if (status)
			return status;
	}

	status = ixgbe_acquire_nvm(hw, IXGBE_RES_WRITE);
	if (status)
		return status;

	status = ixgbe_write_sr_buf_aci(hw, (u32)offset, words, data);
	ixgbe_release_nvm(hw);

	return status;
}

/**
 * ixgbe_calc_eeprom_checksum_E610 - Calculates and returns the checksum
 * @hw: pointer to hardware structure
 *
 * Calculate SW Checksum that covers the whole 64kB shadow RAM
 * except the VPD and PCIe ALT Auto-load modules. The structure and size of VPD
 * is customer specific and unknown. Therefore, this function skips all maximum
 * possible size of VPD (1kB).
 * If the EEPROM params are not initialized, the function
 * initializes them before proceeding.
 * The function acquires and then releases the NVM ownership.
 *
 * Return: the negative error code on error, or the 16-bit checksum
 */
s32 ixgbe_calc_eeprom_checksum_E610(struct ixgbe_hw *hw)
{
	bool nvm_acquired = false;
	u16 pcie_alt_module = 0;
	u16 checksum_local = 0;
	u16 checksum = 0;
	u16 vpd_module;
	void *vmem;
	s32 status;
	u16 *data;
	u16 i;

	if (hw->eeprom.type == ixgbe_eeprom_uninitialized) {
		status = ixgbe_init_eeprom_params(hw);
		if (status)
			return status;
	}

	vmem = ixgbe_calloc(hw, IXGBE_SR_SECTOR_SIZE_IN_WORDS, sizeof(u16));
	if (!vmem)
		return IXGBE_ERR_OUT_OF_MEM;
	data = (u16 *)vmem;
	status = ixgbe_acquire_nvm(hw, IXGBE_RES_READ);
	if (status)
		goto ixgbe_calc_sr_checksum_exit;
	nvm_acquired = true;

	/* read pointer to VPD area */
	status = ixgbe_read_sr_word_aci(hw, E610_SR_VPD_PTR, &vpd_module);
	if (status)
		goto ixgbe_calc_sr_checksum_exit;

	/* read pointer to PCIe Alt Auto-load module */
	status = ixgbe_read_sr_word_aci(hw, E610_SR_PCIE_ALT_AUTO_LOAD_PTR,
					&pcie_alt_module);
	if (status)
		goto ixgbe_calc_sr_checksum_exit;

	/* Calculate SW checksum that covers the whole 64kB shadow RAM
	 * except the VPD and PCIe ALT Auto-load modules
	 */
	for (i = 0; i < hw->eeprom.word_size; i++) {
		/* Read SR page */
		if ((i % IXGBE_SR_SECTOR_SIZE_IN_WORDS) == 0) {
			u16 words = IXGBE_SR_SECTOR_SIZE_IN_WORDS;

			status = ixgbe_read_sr_buf_aci(hw, i, &words, data);
			if (status != IXGBE_SUCCESS)
				goto ixgbe_calc_sr_checksum_exit;
		}

		/* Skip Checksum word */
		if (i == E610_SR_SW_CHECKSUM_WORD)
			continue;
		/* Skip VPD module (convert byte size to word count) */
		if (i >= (u32)vpd_module &&
		    i < ((u32)vpd_module + E610_SR_VPD_SIZE_WORDS))
			continue;
		/* Skip PCIe ALT module (convert byte size to word count) */
		if (i >= (u32)pcie_alt_module &&
		    i < ((u32)pcie_alt_module + E610_SR_PCIE_ALT_SIZE_WORDS))
			continue;

		checksum_local += data[i % IXGBE_SR_SECTOR_SIZE_IN_WORDS];
	}

	checksum = (u16)IXGBE_SR_SW_CHECKSUM_BASE - checksum_local;

ixgbe_calc_sr_checksum_exit:
	if(nvm_acquired)
		ixgbe_release_nvm(hw);
	ixgbe_free(hw, vmem);

	if(!status)
		return (s32)checksum;
	else
		return status;
}

/**
 * ixgbe_update_eeprom_checksum_E610 - Updates the EEPROM checksum and flash
 * @hw: pointer to hardware structure
 *
 * After writing EEPROM to Shadow RAM, software sends the admin command
 * to recalculate and update EEPROM checksum and instructs the hardware
 * to update the flash.
 * If the EEPROM params are not initialized, the function
 * initialize them before proceeding.
 * The function acquires and then releases the NVM ownership.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_update_eeprom_checksum_E610(struct ixgbe_hw *hw)
{
	s32 status;

	if (hw->eeprom.type == ixgbe_eeprom_uninitialized) {
		status = ixgbe_init_eeprom_params(hw);
		if (status)
			return status;
	}

	status = ixgbe_nvm_recalculate_checksum(hw);
	if (status)
		return status;
	status = ixgbe_acquire_nvm(hw, IXGBE_RES_WRITE);
	if (status)
		return status;

	status = ixgbe_nvm_write_activate(hw, IXGBE_ACI_NVM_ACTIV_REQ_EMPR,
					  NULL);
	ixgbe_release_nvm(hw);

	return status;
}

/**
 * ixgbe_validate_eeprom_checksum_E610 - Validate EEPROM checksum
 * @hw: pointer to hardware structure
 * @checksum_val: calculated checksum
 *
 * Performs checksum calculation and validates the EEPROM checksum. If the
 * caller does not need checksum_val, the value can be NULL.
 * If the EEPROM params are not initialized, the function
 * initialize them before proceeding.
 * The function acquires and then releases the NVM ownership.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_validate_eeprom_checksum_E610(struct ixgbe_hw *hw, u16 *checksum_val)
{
	u32 status;

	if (hw->eeprom.type == ixgbe_eeprom_uninitialized) {
		status = ixgbe_init_eeprom_params(hw);
		if (status)
			return status;
	}

	status = ixgbe_nvm_validate_checksum(hw);

	if (status)
		return status;

	if (checksum_val) {
		u16 tmp_checksum;
		status = ixgbe_acquire_nvm(hw, IXGBE_RES_READ);
		if (status)
			return status;

		status = ixgbe_read_sr_word_aci(hw, E610_SR_SW_CHECKSUM_WORD,
						&tmp_checksum);
		ixgbe_release_nvm(hw);

		if (!status)
			*checksum_val = tmp_checksum;
	}

	return status;
}

/**
 * ixgbe_get_pfa_module_tlv - Reads sub module TLV from NVM PFA
 * @hw: pointer to hardware structure
 * @module_tlv: pointer to module TLV to return
 * @module_tlv_len: pointer to module TLV length to return
 * @module_type: module type requested
 *
 * Finds the requested sub module TLV type from the Preserved Field
 * Area (PFA) and returns the TLV pointer and length. The caller can
 * use these to read the variable length TLV value.
 *
 * Return: the exit code of the operation.
 */
static s32 ixgbe_get_pfa_module_tlv(struct ixgbe_hw *hw, u16 *module_tlv,
				    u16 *module_tlv_len, u16 module_type)
{
	u16 pfa_len, pfa_ptr, pfa_end_ptr;
	u16 next_tlv;
	s32 status;

	status = ixgbe_read_ee_aci_E610(hw, E610_SR_PFA_PTR, &pfa_ptr);
	if (status != IXGBE_SUCCESS) {
		return status;
	}
	status = ixgbe_read_ee_aci_E610(hw, pfa_ptr, &pfa_len);
	if (status != IXGBE_SUCCESS) {
		return status;
	}
	/* Starting with first TLV after PFA length, iterate through the list
	 * of TLVs to find the requested one.
	 */
	next_tlv = pfa_ptr + 1;
	pfa_end_ptr = pfa_ptr + pfa_len;
	while (next_tlv < pfa_end_ptr) {
		u16 tlv_sub_module_type, tlv_len;

		/* Read TLV type */
		status = ixgbe_read_ee_aci_E610(hw, next_tlv,
						&tlv_sub_module_type);
		if (status != IXGBE_SUCCESS) {
			break;
		}
		/* Read TLV length */
		status = ixgbe_read_ee_aci_E610(hw, next_tlv + 1, &tlv_len);
		if (status != IXGBE_SUCCESS) {
			break;
		}
		if (tlv_sub_module_type == module_type) {
			if (tlv_len) {
				*module_tlv = next_tlv;
				*module_tlv_len = tlv_len;
				return IXGBE_SUCCESS;
			}
			return IXGBE_ERR_INVAL_SIZE;
		}
		/* Check next TLV, i.e. current TLV pointer + length + 2 words
		 * (for current TLV's type and length)
		 */
		next_tlv = next_tlv + tlv_len + 2;
	}
	/* Module does not exist */
	return IXGBE_ERR_DOES_NOT_EXIST;
}

/**
 * ixgbe_read_pba_string_E610 - Reads part number string from NVM
 * @hw: pointer to hardware structure
 * @pba_num: stores the part number string from the NVM
 * @pba_num_size: part number string buffer length
 *
 * Reads the part number string from the NVM.
 *
 * Return: the exit code of the operation.
 */
s32 ixgbe_read_pba_string_E610(struct ixgbe_hw *hw, u8 *pba_num,
			       u32 pba_num_size)
{
	u16 pba_tlv, pba_tlv_len;
	u16 pba_word, pba_size;
	s32 status;
	u16 i;

	status = ixgbe_get_pfa_module_tlv(hw, &pba_tlv, &pba_tlv_len,
					E610_SR_PBA_BLOCK_PTR);
	if (status != IXGBE_SUCCESS) {
		return status;
	}

	/* pba_size is the next word */
	status = ixgbe_read_ee_aci_E610(hw, (pba_tlv + 2), &pba_size);
	if (status != IXGBE_SUCCESS) {
		return status;
	}

	if (pba_tlv_len < pba_size) {
		return IXGBE_ERR_INVAL_SIZE;
	}

	/* Subtract one to get PBA word count (PBA Size word is included in
	 * total size)
	 */
	pba_size--;
	if (pba_num_size < (((u32)pba_size * 2) + 1)) {
		return IXGBE_ERR_PARAM;
	}

	for (i = 0; i < pba_size; i++) {
		status = ixgbe_read_ee_aci_E610(hw, (pba_tlv + 2 + 1) + i,
						&pba_word);
		if (status != IXGBE_SUCCESS) {
			return status;
		}

		pba_num[(i * 2)] = (pba_word >> 8) & 0xFF;
		pba_num[(i * 2) + 1] = pba_word & 0xFF;
	}
	pba_num[(pba_size * 2)] = '\0';

	return status;
}
