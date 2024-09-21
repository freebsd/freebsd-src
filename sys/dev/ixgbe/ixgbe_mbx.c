/******************************************************************************
  SPDX-License-Identifier: BSD-3-Clause

  Copyright (c) 2001-2020, Intel Corporation
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
#include "ixgbe_mbx.h"

static s32 ixgbe_poll_for_msg(struct ixgbe_hw *hw, u16 mbx_id);
static s32 ixgbe_poll_for_ack(struct ixgbe_hw *hw, u16 mbx_id);

/**
 * ixgbe_read_mbx - Reads a message from the mailbox
 * @hw: pointer to the HW structure
 * @msg: The message buffer
 * @size: Length of buffer
 * @mbx_id: id of mailbox to read
 *
 * returns SUCCESS if it successfully read message from buffer
 **/
s32 ixgbe_read_mbx(struct ixgbe_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;

	DEBUGFUNC("ixgbe_read_mbx");

	/* limit read to size of mailbox */
	if (size > mbx->size) {
		ERROR_REPORT3(IXGBE_ERROR_ARGUMENT,
			      "Invalid mailbox message size %u, changing to %u",
			      size, mbx->size);
		size = mbx->size;
	}

	if (mbx->ops[mbx_id].read)
		return mbx->ops[mbx_id].read(hw, msg, size, mbx_id);

	return IXGBE_ERR_CONFIG;
}

/**
 * ixgbe_poll_mbx - Wait for message and read it from the mailbox
 * @hw: pointer to the HW structure
 * @msg: The message buffer
 * @size: Length of buffer
 * @mbx_id: id of mailbox to read
 *
 * returns SUCCESS if it successfully read message from buffer
 **/
s32 ixgbe_poll_mbx(struct ixgbe_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val;

	DEBUGFUNC("ixgbe_poll_mbx");

	if (!mbx->ops[mbx_id].read || !mbx->ops[mbx_id].check_for_msg ||
	    !mbx->timeout)
		return IXGBE_ERR_CONFIG;

	/* limit read to size of mailbox */
	if (size > mbx->size) {
		ERROR_REPORT3(IXGBE_ERROR_ARGUMENT,
			      "Invalid mailbox message size %u, changing to %u",
			      size, mbx->size);
		size = mbx->size;
	}

	ret_val = ixgbe_poll_for_msg(hw, mbx_id);
	/* if ack received read message, otherwise we timed out */
	if (!ret_val)
		return mbx->ops[mbx_id].read(hw, msg, size, mbx_id);

	return ret_val;
}

/**
 * ixgbe_write_mbx - Write a message to the mailbox and wait for ACK
 * @hw: pointer to the HW structure
 * @msg: The message buffer
 * @size: Length of buffer
 * @mbx_id: id of mailbox to write
 *
 * returns SUCCESS if it successfully copied message into the buffer and
 * received an ACK to that message within specified period
 **/
s32 ixgbe_write_mbx(struct ixgbe_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = IXGBE_ERR_MBX;

	DEBUGFUNC("ixgbe_write_mbx");

	/*
	 * exit if either we can't write, release
	 * or there is no timeout defined
	 */
	if (!mbx->ops[mbx_id].write || !mbx->ops[mbx_id].check_for_ack ||
	    !mbx->ops[mbx_id].release || !mbx->timeout)
		return IXGBE_ERR_CONFIG;

	if (size > mbx->size) {
		ret_val = IXGBE_ERR_PARAM;
		ERROR_REPORT2(IXGBE_ERROR_ARGUMENT,
			     "Invalid mailbox message size %u", size);
	} else {
		ret_val = mbx->ops[mbx_id].write(hw, msg, size, mbx_id);
	}

	return ret_val;
}

/**
 * ixgbe_check_for_msg - checks to see if someone sent us mail
 * @hw: pointer to the HW structure
 * @mbx_id: id of mailbox to check
 *
 * returns SUCCESS if the Status bit was found or else ERR_MBX
 **/
s32 ixgbe_check_for_msg(struct ixgbe_hw *hw, u16 mbx_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = IXGBE_ERR_CONFIG;

	DEBUGFUNC("ixgbe_check_for_msg");

	if (mbx->ops[mbx_id].check_for_msg)
		ret_val = mbx->ops[mbx_id].check_for_msg(hw, mbx_id);

	return ret_val;
}

/**
 * ixgbe_check_for_ack - checks to see if someone sent us ACK
 * @hw: pointer to the HW structure
 * @mbx_id: id of mailbox to check
 *
 * returns SUCCESS if the Status bit was found or else ERR_MBX
 **/
s32 ixgbe_check_for_ack(struct ixgbe_hw *hw, u16 mbx_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = IXGBE_ERR_CONFIG;

	DEBUGFUNC("ixgbe_check_for_ack");

	if (mbx->ops[mbx_id].check_for_ack)
		ret_val = mbx->ops[mbx_id].check_for_ack(hw, mbx_id);

	return ret_val;
}

/**
 * ixgbe_check_for_rst - checks to see if other side has reset
 * @hw: pointer to the HW structure
 * @mbx_id: id of mailbox to check
 *
 * returns SUCCESS if the Status bit was found or else ERR_MBX
 **/
s32 ixgbe_check_for_rst(struct ixgbe_hw *hw, u16 mbx_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = IXGBE_ERR_CONFIG;

	DEBUGFUNC("ixgbe_check_for_rst");

	if (mbx->ops[mbx_id].check_for_rst)
		ret_val = mbx->ops[mbx_id].check_for_rst(hw, mbx_id);

	return ret_val;
}

/**
 * ixgbe_clear_mbx - Clear Mailbox Memory
 * @hw: pointer to the HW structure
 * @mbx_id: id of mailbox to write
 *
 * Set VFMBMEM of given VF to 0x0.
 **/
s32 ixgbe_clear_mbx(struct ixgbe_hw *hw, u16 mbx_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = IXGBE_ERR_CONFIG;

	DEBUGFUNC("ixgbe_clear_mbx");

	if (mbx->ops[mbx_id].clear)
		ret_val = mbx->ops[mbx_id].clear(hw, mbx_id);

	return ret_val;
}

/**
 * ixgbe_poll_for_msg - Wait for message notification
 * @hw: pointer to the HW structure
 * @mbx_id: id of mailbox to write
 *
 * returns SUCCESS if it successfully received a message notification
 **/
static s32 ixgbe_poll_for_msg(struct ixgbe_hw *hw, u16 mbx_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	DEBUGFUNC("ixgbe_poll_for_msg");

	if (!countdown || !mbx->ops[mbx_id].check_for_msg)
		return IXGBE_ERR_CONFIG;

	while (countdown && mbx->ops[mbx_id].check_for_msg(hw, mbx_id)) {
		countdown--;
		if (!countdown)
			break;
		usec_delay(mbx->usec_delay);
	}

	if (countdown == 0) {
		ERROR_REPORT2(IXGBE_ERROR_POLLING,
			   "Polling for VF%u mailbox message timedout", mbx_id);
		return IXGBE_ERR_TIMEOUT;
	}

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_poll_for_ack - Wait for message acknowledgment
 * @hw: pointer to the HW structure
 * @mbx_id: id of mailbox to write
 *
 * returns SUCCESS if it successfully received a message acknowledgment
 **/
static s32 ixgbe_poll_for_ack(struct ixgbe_hw *hw, u16 mbx_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	DEBUGFUNC("ixgbe_poll_for_ack");

	if (!countdown || !mbx->ops[mbx_id].check_for_ack)
		return IXGBE_ERR_CONFIG;

	while (countdown && mbx->ops[mbx_id].check_for_ack(hw, mbx_id)) {
		countdown--;
		if (!countdown)
			break;
		usec_delay(mbx->usec_delay);
	}

	if (countdown == 0) {
		ERROR_REPORT2(IXGBE_ERROR_POLLING,
			     "Polling for VF%u mailbox ack timedout", mbx_id);
		return IXGBE_ERR_TIMEOUT;
	}

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_read_mailbox_vf - read VF's mailbox register
 * @hw: pointer to the HW structure
 *
 * This function is used to read the mailbox register dedicated for VF without
 * losing the read to clear status bits.
 **/
static u32 ixgbe_read_mailbox_vf(struct ixgbe_hw *hw)
{
	u32 vf_mailbox = IXGBE_READ_REG(hw, IXGBE_VFMAILBOX);

	vf_mailbox |= hw->mbx.vf_mailbox;
	hw->mbx.vf_mailbox |= vf_mailbox % IXGBE_VFMAILBOX_R2C_BITS;

	return vf_mailbox;
}

static void ixgbe_clear_msg_vf(struct ixgbe_hw *hw)
{
	u32 vf_mailbox = ixgbe_read_mailbox_vf(hw);

	if (vf_mailbox & IXGBE_VFMAILBOX_PFSTS) {
		hw->mbx.stats.reqs++;
		hw->mbx.vf_mailbox &= ~IXGBE_VFMAILBOX_PFSTS;
	}
}

static void ixgbe_clear_ack_vf(struct ixgbe_hw *hw)
{
	u32 vf_mailbox = ixgbe_read_mailbox_vf(hw);

	if (vf_mailbox & IXGBE_VFMAILBOX_PFACK) {
		hw->mbx.stats.acks++;
		hw->mbx.vf_mailbox &= ~IXGBE_VFMAILBOX_PFACK;
	}
}

static void ixgbe_clear_rst_vf(struct ixgbe_hw *hw)
{
	u32 vf_mailbox = ixgbe_read_mailbox_vf(hw);

	if (vf_mailbox & (IXGBE_VFMAILBOX_RSTI | IXGBE_VFMAILBOX_RSTD)) {
		hw->mbx.stats.rsts++;
		hw->mbx.vf_mailbox &= ~(IXGBE_VFMAILBOX_RSTI |
					IXGBE_VFMAILBOX_RSTD);
	}
}

/**
 * ixgbe_check_for_bit_vf - Determine if a status bit was set
 * @hw: pointer to the HW structure
 * @mask: bitmask for bits to be tested and cleared
 *
 * This function is used to check for the read to clear bits within
 * the V2P mailbox.
 **/
static s32 ixgbe_check_for_bit_vf(struct ixgbe_hw *hw, u32 mask)
{
	u32 vf_mailbox = ixgbe_read_mailbox_vf(hw);

	if (vf_mailbox & mask)
		return IXGBE_SUCCESS;

	return IXGBE_ERR_MBX;
}

/**
 * ixgbe_check_for_msg_vf - checks to see if the PF has sent mail
 * @hw: pointer to the HW structure
 * @mbx_id: id of mailbox to check
 *
 * returns SUCCESS if the PF has set the Status bit or else ERR_MBX
 **/
static s32 ixgbe_check_for_msg_vf(struct ixgbe_hw *hw, u16 mbx_id)
{
	UNREFERENCED_1PARAMETER(mbx_id);
	DEBUGFUNC("ixgbe_check_for_msg_vf");

	if (!ixgbe_check_for_bit_vf(hw, IXGBE_VFMAILBOX_PFSTS))
		return IXGBE_SUCCESS;

	return IXGBE_ERR_MBX;
}

/**
 * ixgbe_check_for_ack_vf - checks to see if the PF has ACK'd
 * @hw: pointer to the HW structure
 * @mbx_id: id of mailbox to check
 *
 * returns SUCCESS if the PF has set the ACK bit or else ERR_MBX
 **/
static s32 ixgbe_check_for_ack_vf(struct ixgbe_hw *hw, u16 mbx_id)
{
	UNREFERENCED_1PARAMETER(mbx_id);
	DEBUGFUNC("ixgbe_check_for_ack_vf");

	if (!ixgbe_check_for_bit_vf(hw, IXGBE_VFMAILBOX_PFACK)) {
		/* TODO: should this be autocleared? */
		ixgbe_clear_ack_vf(hw);
		return IXGBE_SUCCESS;
	}

	return IXGBE_ERR_MBX;
}

/**
 * ixgbe_check_for_rst_vf - checks to see if the PF has reset
 * @hw: pointer to the HW structure
 * @mbx_id: id of mailbox to check
 *
 * returns true if the PF has set the reset done bit or else false
 **/
static s32 ixgbe_check_for_rst_vf(struct ixgbe_hw *hw, u16 mbx_id)
{
	UNREFERENCED_1PARAMETER(mbx_id);
	DEBUGFUNC("ixgbe_check_for_rst_vf");

	if (!ixgbe_check_for_bit_vf(hw, IXGBE_VFMAILBOX_RSTI |
					  IXGBE_VFMAILBOX_RSTD)) {
		/* TODO: should this be autocleared? */
		ixgbe_clear_rst_vf(hw);
		return IXGBE_SUCCESS;
	}

	return IXGBE_ERR_MBX;
}

/**
 * ixgbe_obtain_mbx_lock_vf - obtain mailbox lock
 * @hw: pointer to the HW structure
 *
 * return SUCCESS if we obtained the mailbox lock
 **/
static s32 ixgbe_obtain_mbx_lock_vf(struct ixgbe_hw *hw)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;
	s32 ret_val = IXGBE_ERR_MBX;
	u32 vf_mailbox;

	DEBUGFUNC("ixgbe_obtain_mbx_lock_vf");

	if (!mbx->timeout)
		return IXGBE_ERR_CONFIG;

	while (countdown--) {
		/* Reserve mailbox for VF use */
		vf_mailbox = ixgbe_read_mailbox_vf(hw);
		vf_mailbox |= IXGBE_VFMAILBOX_VFU;
		IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, vf_mailbox);

		/* Verify that VF is the owner of the lock */
		if (ixgbe_read_mailbox_vf(hw) & IXGBE_VFMAILBOX_VFU) {
			ret_val = IXGBE_SUCCESS;
			break;
		}

		/* Wait a bit before trying again */
		usec_delay(mbx->usec_delay);
	}

	if (ret_val != IXGBE_SUCCESS) {
		ERROR_REPORT1(IXGBE_ERROR_INVALID_STATE,
				"Failed to obtain mailbox lock");
		ret_val = IXGBE_ERR_TIMEOUT;
	}

	return ret_val;
}

/**
 * ixgbe_release_mbx_lock_dummy - release mailbox lock
 * @hw: pointer to the HW structure
 * @mbx_id: id of mailbox to read
 **/
static void ixgbe_release_mbx_lock_dummy(struct ixgbe_hw *hw, u16 mbx_id)
{
	UNREFERENCED_2PARAMETER(hw, mbx_id);

	DEBUGFUNC("ixgbe_release_mbx_lock_dummy");
}

/**
 * ixgbe_release_mbx_lock_vf - release mailbox lock
 * @hw: pointer to the HW structure
 * @mbx_id: id of mailbox to read
 **/
static void ixgbe_release_mbx_lock_vf(struct ixgbe_hw *hw, u16 mbx_id)
{
	u32 vf_mailbox;

	UNREFERENCED_1PARAMETER(mbx_id);

	DEBUGFUNC("ixgbe_release_mbx_lock_vf");

	/* Return ownership of the buffer */
	vf_mailbox = ixgbe_read_mailbox_vf(hw);
	vf_mailbox &= ~IXGBE_VFMAILBOX_VFU;
	IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, vf_mailbox);
}

/**
 * ixgbe_write_mbx_vf_legacy - Write a message to the mailbox
 * @hw: pointer to the HW structure
 * @msg: The message buffer
 * @size: Length of buffer
 * @mbx_id: id of mailbox to write
 *
 * returns SUCCESS if it successfully copied message into the buffer
 **/
static s32 ixgbe_write_mbx_vf_legacy(struct ixgbe_hw *hw, u32 *msg, u16 size,
				     u16 mbx_id)
{
	s32 ret_val;
	u16 i;

	UNREFERENCED_1PARAMETER(mbx_id);
	DEBUGFUNC("ixgbe_write_mbx_vf_legacy");

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = ixgbe_obtain_mbx_lock_vf(hw);
	if (ret_val)
		return ret_val;

	/* flush msg and acks as we are overwriting the message buffer */
	ixgbe_check_for_msg_vf(hw, 0);
	ixgbe_clear_msg_vf(hw);
	ixgbe_check_for_ack_vf(hw, 0);
	ixgbe_clear_ack_vf(hw);

	/* copy the caller specified message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		IXGBE_WRITE_REG_ARRAY(hw, IXGBE_VFMBMEM, i, msg[i]);

	/* update stats */
	hw->mbx.stats.msgs_tx++;

	/* interrupt the PF to tell it a message has been sent */
	IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, IXGBE_VFMAILBOX_REQ);

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_write_mbx_vf - Write a message to the mailbox
 * @hw: pointer to the HW structure
 * @msg: The message buffer
 * @size: Length of buffer
 * @mbx_id: id of mailbox to write
 *
 * returns SUCCESS if it successfully copied message into the buffer
 **/
static s32 ixgbe_write_mbx_vf(struct ixgbe_hw *hw, u32 *msg, u16 size,
			      u16 mbx_id)
{
	u32 vf_mailbox;
	s32 ret_val;
	u16 i;

	UNREFERENCED_1PARAMETER(mbx_id);

	DEBUGFUNC("ixgbe_write_mbx_vf");

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = ixgbe_obtain_mbx_lock_vf(hw);
	if (ret_val)
		goto out;

	/* flush msg and acks as we are overwriting the message buffer */
	ixgbe_clear_msg_vf(hw);
	ixgbe_clear_ack_vf(hw);

	/* copy the caller specified message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		IXGBE_WRITE_REG_ARRAY(hw, IXGBE_VFMBMEM, i, msg[i]);

	/* update stats */
	hw->mbx.stats.msgs_tx++;

	/* interrupt the PF to tell it a message has been sent */
	vf_mailbox = ixgbe_read_mailbox_vf(hw);
	vf_mailbox |= IXGBE_VFMAILBOX_REQ;
	IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, vf_mailbox);

	/* if msg sent wait until we receive an ack */
	ixgbe_poll_for_ack(hw, mbx_id);

out:
	hw->mbx.ops[mbx_id].release(hw, mbx_id);

	return ret_val;
}

/**
 * ixgbe_read_mbx_vf_legacy - Reads a message from the inbox intended for vf
 * @hw: pointer to the HW structure
 * @msg: The message buffer
 * @size: Length of buffer
 * @mbx_id: id of mailbox to read
 *
 * returns SUCCESS if it successfully read message from buffer
 **/
static s32 ixgbe_read_mbx_vf_legacy(struct ixgbe_hw *hw, u32 *msg, u16 size,
				 u16 mbx_id)
{
	s32 ret_val;
	u16 i;

	DEBUGFUNC("ixgbe_read_mbx_vf_legacy");
	UNREFERENCED_1PARAMETER(mbx_id);

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = ixgbe_obtain_mbx_lock_vf(hw);
	if (ret_val)
		return ret_val;

	/* copy the message from the mailbox memory buffer */
	for (i = 0; i < size; i++)
		msg[i] = IXGBE_READ_REG_ARRAY(hw, IXGBE_VFMBMEM, i);

	/* Acknowledge receipt and release mailbox, then we're done */
	IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, IXGBE_VFMAILBOX_ACK);

	/* update stats */
	hw->mbx.stats.msgs_rx++;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_read_mbx_vf - Reads a message from the inbox intended for vf
 * @hw: pointer to the HW structure
 * @msg: The message buffer
 * @size: Length of buffer
 * @mbx_id: id of mailbox to read
 *
 * returns SUCCESS if it successfully read message from buffer
 **/
static s32 ixgbe_read_mbx_vf(struct ixgbe_hw *hw, u32 *msg, u16 size,
			     u16 mbx_id)
{
	u32 vf_mailbox;
	s32 ret_val;
	u16 i;

	DEBUGFUNC("ixgbe_read_mbx_vf");
	UNREFERENCED_1PARAMETER(mbx_id);

	/* check if there is a message from PF */
	ret_val = ixgbe_check_for_msg_vf(hw, 0);
	if (ret_val != IXGBE_SUCCESS)
		return IXGBE_ERR_MBX_NOMSG;

	ixgbe_clear_msg_vf(hw);

	/* copy the message from the mailbox memory buffer */
	for (i = 0; i < size; i++)
		msg[i] = IXGBE_READ_REG_ARRAY(hw, IXGBE_VFMBMEM, i);

	/* Acknowledge receipt */
	vf_mailbox = ixgbe_read_mailbox_vf(hw);
	vf_mailbox |= IXGBE_VFMAILBOX_ACK;
	IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, vf_mailbox);

	/* update stats */
	hw->mbx.stats.msgs_rx++;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_init_mbx_params_vf - set initial values for vf mailbox
 * @hw: pointer to the HW structure
 *
 * Initializes single set the hw->mbx struct to correct values for vf mailbox
 * Set of legacy functions is being used here
 */
void ixgbe_init_mbx_params_vf(struct ixgbe_hw *hw)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;

	mbx->timeout = IXGBE_VF_MBX_INIT_TIMEOUT;
	mbx->usec_delay = IXGBE_VF_MBX_INIT_DELAY;

	mbx->size = IXGBE_VFMAILBOX_SIZE;

	/* VF has only one mailbox connection, no need for more IDs */
	mbx->ops[0].release = ixgbe_release_mbx_lock_dummy;
	mbx->ops[0].read = ixgbe_read_mbx_vf_legacy;
	mbx->ops[0].write = ixgbe_write_mbx_vf_legacy;
	mbx->ops[0].check_for_msg = ixgbe_check_for_msg_vf;
	mbx->ops[0].check_for_ack = ixgbe_check_for_ack_vf;
	mbx->ops[0].check_for_rst = ixgbe_check_for_rst_vf;
	mbx->ops[0].clear = NULL;

	mbx->stats.msgs_tx = 0;
	mbx->stats.msgs_rx = 0;
	mbx->stats.reqs = 0;
	mbx->stats.acks = 0;
	mbx->stats.rsts = 0;
}

/**
 * ixgbe_upgrade_mbx_params_vf - set initial values for vf mailbox
 * @hw: pointer to the HW structure
 *
 * Initializes the hw->mbx struct to correct values for vf mailbox
 */
void ixgbe_upgrade_mbx_params_vf(struct ixgbe_hw *hw)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;

	mbx->timeout = IXGBE_VF_MBX_INIT_TIMEOUT;
	mbx->usec_delay = IXGBE_VF_MBX_INIT_DELAY;

	mbx->size = IXGBE_VFMAILBOX_SIZE;

	/* VF has only one mailbox connection, no need for more IDs */
	mbx->ops[0].release = ixgbe_release_mbx_lock_vf;
	mbx->ops[0].read = ixgbe_read_mbx_vf;
	mbx->ops[0].write = ixgbe_write_mbx_vf;
	mbx->ops[0].check_for_msg = ixgbe_check_for_msg_vf;
	mbx->ops[0].check_for_ack = ixgbe_check_for_ack_vf;
	mbx->ops[0].check_for_rst = ixgbe_check_for_rst_vf;
	mbx->ops[0].clear = NULL;

	mbx->stats.msgs_tx = 0;
	mbx->stats.msgs_rx = 0;
	mbx->stats.reqs = 0;
	mbx->stats.acks = 0;
	mbx->stats.rsts = 0;
}

static void ixgbe_clear_msg_pf(struct ixgbe_hw *hw, u16 vf_id)
{
	u32 vf_shift = IXGBE_PFMBICR_SHIFT(vf_id);
	s32 index = IXGBE_PFMBICR_INDEX(vf_id);
	u32 pfmbicr;

	pfmbicr = IXGBE_READ_REG(hw, IXGBE_PFMBICR(index));

	if (pfmbicr & (IXGBE_PFMBICR_VFREQ_VF1 << vf_shift))
		hw->mbx.stats.reqs++;

	IXGBE_WRITE_REG(hw, IXGBE_PFMBICR(index),
			IXGBE_PFMBICR_VFREQ_VF1 << vf_shift);
}

static void ixgbe_clear_ack_pf(struct ixgbe_hw *hw, u16 vf_id)
{
	u32 vf_shift = IXGBE_PFMBICR_SHIFT(vf_id);
	s32 index = IXGBE_PFMBICR_INDEX(vf_id);
	u32 pfmbicr;

	pfmbicr = IXGBE_READ_REG(hw, IXGBE_PFMBICR(index));

	if (pfmbicr & (IXGBE_PFMBICR_VFACK_VF1 << vf_shift))
		hw->mbx.stats.acks++;

	IXGBE_WRITE_REG(hw, IXGBE_PFMBICR(index),
			IXGBE_PFMBICR_VFACK_VF1 << vf_shift);
}

static s32 ixgbe_check_for_bit_pf(struct ixgbe_hw *hw, u32 mask, s32 index)
{
	u32 pfmbicr = IXGBE_READ_REG(hw, IXGBE_PFMBICR(index));

	if (pfmbicr & mask) {
		return IXGBE_SUCCESS;
	}

	return IXGBE_ERR_MBX;
}

/**
 * ixgbe_check_for_msg_pf - checks to see if the VF has sent mail
 * @hw: pointer to the HW structure
 * @vf_id: the VF index
 *
 * returns SUCCESS if the VF has set the Status bit or else ERR_MBX
 **/
static s32 ixgbe_check_for_msg_pf(struct ixgbe_hw *hw, u16 vf_id)
{
	u32 vf_shift = IXGBE_PFMBICR_SHIFT(vf_id);
	s32 index = IXGBE_PFMBICR_INDEX(vf_id);

	DEBUGFUNC("ixgbe_check_for_msg_pf");

	if (!ixgbe_check_for_bit_pf(hw, IXGBE_PFMBICR_VFREQ_VF1 << vf_shift,
				    index))
		return IXGBE_SUCCESS;

	return IXGBE_ERR_MBX;
}

/**
 * ixgbe_check_for_ack_pf - checks to see if the VF has ACKed
 * @hw: pointer to the HW structure
 * @vf_id: the VF index
 *
 * returns SUCCESS if the VF has set the Status bit or else ERR_MBX
 **/
static s32 ixgbe_check_for_ack_pf(struct ixgbe_hw *hw, u16 vf_id)
{
	u32 vf_shift = IXGBE_PFMBICR_SHIFT(vf_id);
	s32 index = IXGBE_PFMBICR_INDEX(vf_id);
	s32 ret_val = IXGBE_ERR_MBX;

	DEBUGFUNC("ixgbe_check_for_ack_pf");

	if (!ixgbe_check_for_bit_pf(hw, IXGBE_PFMBICR_VFACK_VF1 << vf_shift,
				    index)) {
		ret_val = IXGBE_SUCCESS;
		/* TODO: should this be autocleared? */
		ixgbe_clear_ack_pf(hw, vf_id);
	}

	return ret_val;
}

/**
 * ixgbe_check_for_rst_pf - checks to see if the VF has reset
 * @hw: pointer to the HW structure
 * @vf_id: the VF index
 *
 * returns SUCCESS if the VF has set the Status bit or else ERR_MBX
 **/
static s32 ixgbe_check_for_rst_pf(struct ixgbe_hw *hw, u16 vf_id)
{
	u32 vf_shift = IXGBE_PFVFLRE_SHIFT(vf_id);
	u32 index = IXGBE_PFVFLRE_INDEX(vf_id);
	s32 ret_val = IXGBE_ERR_MBX;
	u32 vflre = 0;

	DEBUGFUNC("ixgbe_check_for_rst_pf");

	switch (hw->mac.type) {
	case ixgbe_mac_82599EB:
		vflre = IXGBE_READ_REG(hw, IXGBE_PFVFLRE(index));
		break;
	case ixgbe_mac_X550:
	case ixgbe_mac_X550EM_x:
	case ixgbe_mac_X550EM_a:
	case ixgbe_mac_X540:
		vflre = IXGBE_READ_REG(hw, IXGBE_PFVFLREC(index));
		break;
	default:
		break;
	}

	if (vflre & (1 << vf_shift)) {
		ret_val = IXGBE_SUCCESS;
		IXGBE_WRITE_REG(hw, IXGBE_PFVFLREC(index), (1 << vf_shift));
		hw->mbx.stats.rsts++;
	}

	return ret_val;
}

/**
 * ixgbe_obtain_mbx_lock_pf - obtain mailbox lock
 * @hw: pointer to the HW structure
 * @vf_id: the VF index
 *
 * return SUCCESS if we obtained the mailbox lock
 **/
static s32 ixgbe_obtain_mbx_lock_pf(struct ixgbe_hw *hw, u16 vf_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;
	s32 ret_val = IXGBE_ERR_MBX;
	u32 pf_mailbox;

	DEBUGFUNC("ixgbe_obtain_mbx_lock_pf");

	if (!mbx->timeout)
		return IXGBE_ERR_CONFIG;

	while (countdown--) {
		/* Reserve mailbox for PF use */
		pf_mailbox = IXGBE_READ_REG(hw, IXGBE_PFMAILBOX(vf_id));
		pf_mailbox |= IXGBE_PFMAILBOX_PFU;
		IXGBE_WRITE_REG(hw, IXGBE_PFMAILBOX(vf_id), pf_mailbox);

		/* Verify that PF is the owner of the lock */
		pf_mailbox = IXGBE_READ_REG(hw, IXGBE_PFMAILBOX(vf_id));
		if (pf_mailbox & IXGBE_PFMAILBOX_PFU) {
			ret_val = IXGBE_SUCCESS;
			break;
		}

		/* Wait a bit before trying again */
		usec_delay(mbx->usec_delay);
	}

	if (ret_val != IXGBE_SUCCESS) {
		ERROR_REPORT1(IXGBE_ERROR_INVALID_STATE,
			      "Failed to obtain mailbox lock");
		ret_val = IXGBE_ERR_TIMEOUT;
	}

	return ret_val;
}

/**
 * ixgbe_release_mbx_lock_pf - release mailbox lock
 * @hw: pointer to the HW structure
 * @vf_id: the VF index
 **/
static void ixgbe_release_mbx_lock_pf(struct ixgbe_hw *hw, u16 vf_id)
{
	u32 pf_mailbox;

	DEBUGFUNC("ixgbe_release_mbx_lock_pf");

	/* Return ownership of the buffer */
	pf_mailbox = IXGBE_READ_REG(hw, IXGBE_PFMAILBOX(vf_id));
	pf_mailbox &= ~IXGBE_PFMAILBOX_PFU;
	IXGBE_WRITE_REG(hw, IXGBE_PFMAILBOX(vf_id), pf_mailbox);
}

/**
 * ixgbe_write_mbx_pf_legacy - Places a message in the mailbox
 * @hw: pointer to the HW structure
 * @msg: The message buffer
 * @size: Length of buffer
 * @vf_id: the VF index
 *
 * returns SUCCESS if it successfully copied message into the buffer
 **/
static s32 ixgbe_write_mbx_pf_legacy(struct ixgbe_hw *hw, u32 *msg, u16 size,
				     u16 vf_id)
{
	s32 ret_val;
	u16 i;

	DEBUGFUNC("ixgbe_write_mbx_pf_legacy");

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = ixgbe_obtain_mbx_lock_pf(hw, vf_id);
	if (ret_val)
		return ret_val;

	/* flush msg and acks as we are overwriting the message buffer */
	ixgbe_check_for_msg_pf(hw, vf_id);
	ixgbe_clear_msg_pf(hw, vf_id);
	ixgbe_check_for_ack_pf(hw, vf_id);
	ixgbe_clear_ack_pf(hw, vf_id);

	/* copy the caller specified message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		IXGBE_WRITE_REG_ARRAY(hw, IXGBE_PFMBMEM(vf_id), i, msg[i]);

	/* Interrupt VF to tell it a message has been sent and release buffer*/
	IXGBE_WRITE_REG(hw, IXGBE_PFMAILBOX(vf_id), IXGBE_PFMAILBOX_STS);

	/* update stats */
	hw->mbx.stats.msgs_tx++;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_write_mbx_pf - Places a message in the mailbox
 * @hw: pointer to the HW structure
 * @msg: The message buffer
 * @size: Length of buffer
 * @vf_id: the VF index
 *
 * returns SUCCESS if it successfully copied message into the buffer
 **/
static s32 ixgbe_write_mbx_pf(struct ixgbe_hw *hw, u32 *msg, u16 size,
			      u16 vf_id)
{
	u32 pf_mailbox;
	s32 ret_val;
	u16 i;

	DEBUGFUNC("ixgbe_write_mbx_pf");

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = ixgbe_obtain_mbx_lock_pf(hw, vf_id);
	if (ret_val)
		goto out;

	/* flush msg and acks as we are overwriting the message buffer */
	ixgbe_clear_msg_pf(hw, vf_id);
	ixgbe_clear_ack_pf(hw, vf_id);

	/* copy the caller specified message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		IXGBE_WRITE_REG_ARRAY(hw, IXGBE_PFMBMEM(vf_id), i, msg[i]);

	/* Interrupt VF to tell it a message has been sent */
	pf_mailbox = IXGBE_READ_REG(hw, IXGBE_PFMAILBOX(vf_id));
	pf_mailbox |= IXGBE_PFMAILBOX_STS;
	IXGBE_WRITE_REG(hw, IXGBE_PFMAILBOX(vf_id), pf_mailbox);

	/* if msg sent wait until we receive an ack */
	ixgbe_poll_for_ack(hw, vf_id);

	/* update stats */
	hw->mbx.stats.msgs_tx++;

out:
	hw->mbx.ops[vf_id].release(hw, vf_id);

	return ret_val;

}

/**
 * ixgbe_read_mbx_pf_legacy - Read a message from the mailbox
 * @hw: pointer to the HW structure
 * @msg: The message buffer
 * @size: Length of buffer
 * @vf_id: the VF index
 *
 * This function copies a message from the mailbox buffer to the caller's
 * memory buffer.  The presumption is that the caller knows that there was
 * a message due to a VF request so no polling for message is needed.
 **/
static s32 ixgbe_read_mbx_pf_legacy(struct ixgbe_hw *hw, u32 *msg, u16 size,
				    u16 vf_id)
{
	s32 ret_val;
	u16 i;

	DEBUGFUNC("ixgbe_read_mbx_pf_legacy");

	/* lock the mailbox to prevent pf/vf race condition */
	ret_val = ixgbe_obtain_mbx_lock_pf(hw, vf_id);
	if (ret_val != IXGBE_SUCCESS)
		return ret_val;

	/* copy the message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		msg[i] = IXGBE_READ_REG_ARRAY(hw, IXGBE_PFMBMEM(vf_id), i);

	/* Acknowledge the message and release buffer */
	IXGBE_WRITE_REG(hw, IXGBE_PFMAILBOX(vf_id), IXGBE_PFMAILBOX_ACK);

	/* update stats */
	hw->mbx.stats.msgs_rx++;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_read_mbx_pf - Read a message from the mailbox
 * @hw: pointer to the HW structure
 * @msg: The message buffer
 * @size: Length of buffer
 * @vf_id: the VF index
 *
 * This function copies a message from the mailbox buffer to the caller's
 * memory buffer.  The presumption is that the caller knows that there was
 * a message due to a VF request so no polling for message is needed.
 **/
static s32 ixgbe_read_mbx_pf(struct ixgbe_hw *hw, u32 *msg, u16 size,
			     u16 vf_id)
{
	u32 pf_mailbox;
	s32 ret_val;
	u16 i;

	DEBUGFUNC("ixgbe_read_mbx_pf");

	/* check if there is a message from VF */
	ret_val = ixgbe_check_for_msg_pf(hw, vf_id);
	if (ret_val != IXGBE_SUCCESS)
		return IXGBE_ERR_MBX_NOMSG;

	ixgbe_clear_msg_pf(hw, vf_id);

	/* copy the message to the mailbox memory buffer */
	for (i = 0; i < size; i++)
		msg[i] = IXGBE_READ_REG_ARRAY(hw, IXGBE_PFMBMEM(vf_id), i);

	/* Acknowledge the message and release buffer */
	pf_mailbox = IXGBE_READ_REG(hw, IXGBE_PFMAILBOX(vf_id));
	pf_mailbox |= IXGBE_PFMAILBOX_ACK;
	IXGBE_WRITE_REG(hw, IXGBE_PFMAILBOX(vf_id), pf_mailbox);

	/* update stats */
	hw->mbx.stats.msgs_rx++;

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_clear_mbx_pf - Clear Mailbox Memory
 * @hw: pointer to the HW structure
 * @vf_id: the VF index
 *
 * Set VFMBMEM of given VF to 0x0.
 **/
static s32 ixgbe_clear_mbx_pf(struct ixgbe_hw *hw, u16 vf_id)
{
	u16 mbx_size = hw->mbx.size;
	u16 i;

	if (vf_id > 63)
		return IXGBE_ERR_PARAM;

	for (i = 0; i < mbx_size; ++i)
		IXGBE_WRITE_REG_ARRAY(hw, IXGBE_PFMBMEM(vf_id), i, 0x0);

	return IXGBE_SUCCESS;
}

/**
 * ixgbe_init_mbx_params_pf_id - set initial values for pf mailbox
 * @hw: pointer to the HW structure
 * @vf_id: the VF index
 *
 * Initializes single set of the hw->mbx struct to correct values for pf mailbox
 * Set of legacy functions is being used here
 */
void ixgbe_init_mbx_params_pf_id(struct ixgbe_hw *hw, u16 vf_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;

	mbx->ops[vf_id].release = ixgbe_release_mbx_lock_dummy;
	mbx->ops[vf_id].read = ixgbe_read_mbx_pf_legacy;
	mbx->ops[vf_id].write = ixgbe_write_mbx_pf_legacy;
	mbx->ops[vf_id].check_for_msg = ixgbe_check_for_msg_pf;
	mbx->ops[vf_id].check_for_ack = ixgbe_check_for_ack_pf;
	mbx->ops[vf_id].check_for_rst = ixgbe_check_for_rst_pf;
	mbx->ops[vf_id].clear = ixgbe_clear_mbx_pf;
}

/**
 * ixgbe_init_mbx_params_pf - set initial values for pf mailbox
 * @hw: pointer to the HW structure
 *
 * Initializes all sets of the hw->mbx struct to correct values for pf
 * mailbox. One set corresponds to single VF. It also initializes counters
 * and general variables. A set of legacy functions is used by default.
 */
void ixgbe_init_mbx_params_pf(struct ixgbe_hw *hw)
{
	u16 i;
	struct ixgbe_mbx_info *mbx = &hw->mbx;

	/* Ensure we are not calling this function from VF */
	if (hw->mac.type != ixgbe_mac_82599EB &&
	    hw->mac.type != ixgbe_mac_X550 &&
	    hw->mac.type != ixgbe_mac_X550EM_x &&
	    hw->mac.type != ixgbe_mac_X550EM_a &&
	    hw->mac.type != ixgbe_mac_X540)
		return;

	/* Initialize common mailbox settings */
	mbx->timeout = IXGBE_VF_MBX_INIT_TIMEOUT;
	mbx->usec_delay = IXGBE_VF_MBX_INIT_DELAY;
	mbx->size = IXGBE_VFMAILBOX_SIZE;

	/* Initialize counters with zeroes */
	mbx->stats.msgs_tx = 0;
	mbx->stats.msgs_rx = 0;
	mbx->stats.reqs = 0;
	mbx->stats.acks = 0;
	mbx->stats.rsts = 0;

	/* No matter of VF number, we initialize params for all 64 VFs. */
	/* TODO: 1. Add a define for max VF and refactor SHARED to get rid
	 * of magic number for that (63 or 64 depending on use case.)
	 * 2. rewrite the code to dynamically allocate mbx->ops[vf_id] for
	 * certain number of VFs instead of default maximum value of 64 (0..63)
	 */
	for (i = 0; i < 64; i++)
		ixgbe_init_mbx_params_pf_id(hw, i);
}

/**
 * ixgbe_upgrade_mbx_params_pf - Upgrade initial values for pf mailbox
 * @hw: pointer to the HW structure
 * @vf_id: the VF index
 *
 * Initializes the hw->mbx struct to new function set for improved
 * stability and handling of messages.
 */
void ixgbe_upgrade_mbx_params_pf(struct ixgbe_hw *hw, u16 vf_id)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;

	/* Ensure we are not calling this function from VF */
	if (hw->mac.type != ixgbe_mac_82599EB &&
	    hw->mac.type != ixgbe_mac_X550 &&
	    hw->mac.type != ixgbe_mac_X550EM_x &&
	    hw->mac.type != ixgbe_mac_X550EM_a &&
	    hw->mac.type != ixgbe_mac_X540)
		return;

	mbx->timeout = IXGBE_VF_MBX_INIT_TIMEOUT;
	mbx->usec_delay = IXGBE_VF_MBX_INIT_DELAY;
	mbx->size = IXGBE_VFMAILBOX_SIZE;

	mbx->ops[vf_id].release = ixgbe_release_mbx_lock_pf;
	mbx->ops[vf_id].read = ixgbe_read_mbx_pf;
	mbx->ops[vf_id].write = ixgbe_write_mbx_pf;
	mbx->ops[vf_id].check_for_msg = ixgbe_check_for_msg_pf;
	mbx->ops[vf_id].check_for_ack = ixgbe_check_for_ack_pf;
	mbx->ops[vf_id].check_for_rst = ixgbe_check_for_rst_pf;
	mbx->ops[vf_id].clear = ixgbe_clear_mbx_pf;

	mbx->stats.msgs_tx = 0;
	mbx->stats.msgs_rx = 0;
	mbx->stats.reqs = 0;
	mbx->stats.acks = 0;
	mbx->stats.rsts = 0;
}
