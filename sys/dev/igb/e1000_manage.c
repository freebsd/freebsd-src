/******************************************************************************

  Copyright (c) 2001-2008, Intel Corporation 
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
/*$FreeBSD$*/

#include "e1000_api.h"
#include "e1000_manage.h"

static u8 e1000_calculate_checksum(u8 *buffer, u32 length);

/**
 *  e1000_calculate_checksum - Calculate checksum for buffer
 *  @buffer: pointer to EEPROM
 *  @length: size of EEPROM to calculate a checksum for
 *
 *  Calculates the checksum for some buffer on a specified length.  The
 *  checksum calculated is returned.
 **/
static u8 e1000_calculate_checksum(u8 *buffer, u32 length)
{
	u32 i;
	u8  sum = 0;

	DEBUGFUNC("e1000_calculate_checksum");

	if (!buffer)
		return 0;

	for (i = 0; i < length; i++)
		sum += buffer[i];

	return (u8) (0 - sum);
}

/**
 *  e1000_mng_enable_host_if_generic - Checks host interface is enabled
 *  @hw: pointer to the HW structure
 *
 *  Returns E1000_success upon success, else E1000_ERR_HOST_INTERFACE_COMMAND
 *
 *  This function checks whether the HOST IF is enabled for command operation
 *  and also checks whether the previous command is completed.  It busy waits
 *  in case of previous command is not completed.
 **/
s32 e1000_mng_enable_host_if_generic(struct e1000_hw * hw)
{
	u32 hicr;
	s32 ret_val = E1000_SUCCESS;
	u8  i;

	DEBUGFUNC("e1000_mng_enable_host_if_generic");

	/* Check that the host interface is enabled. */
	hicr = E1000_READ_REG(hw, E1000_HICR);
	if ((hicr & E1000_HICR_EN) == 0) {
		DEBUGOUT("E1000_HOST_EN bit disabled.\n");
		ret_val = -E1000_ERR_HOST_INTERFACE_COMMAND;
		goto out;
	}
	/* check the previous command is completed */
	for (i = 0; i < E1000_MNG_DHCP_COMMAND_TIMEOUT; i++) {
		hicr = E1000_READ_REG(hw, E1000_HICR);
		if (!(hicr & E1000_HICR_C))
			break;
		msec_delay_irq(1);
	}

	if (i == E1000_MNG_DHCP_COMMAND_TIMEOUT) {
		DEBUGOUT("Previous command timeout failed .\n");
		ret_val = -E1000_ERR_HOST_INTERFACE_COMMAND;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  e1000_check_mng_mode_generic - Generic check management mode
 *  @hw: pointer to the HW structure
 *
 *  Reads the firmware semaphore register and returns true (>0) if
 *  manageability is enabled, else false (0).
 **/
bool e1000_check_mng_mode_generic(struct e1000_hw *hw)
{
	u32 fwsm;

	DEBUGFUNC("e1000_check_mng_mode_generic");

	fwsm = E1000_READ_REG(hw, E1000_FWSM);

	return ((fwsm & E1000_FWSM_MODE_MASK) ==
	        (E1000_MNG_IAMT_MODE << E1000_FWSM_MODE_SHIFT));
}

/**
 *  e1000_enable_tx_pkt_filtering_generic - Enable packet filtering on TX
 *  @hw: pointer to the HW structure
 *
 *  Enables packet filtering on transmit packets if manageability is enabled
 *  and host interface is enabled.
 **/
bool e1000_enable_tx_pkt_filtering_generic(struct e1000_hw *hw)
{
	struct e1000_host_mng_dhcp_cookie *hdr = &hw->mng_cookie;
	u32 *buffer = (u32 *)&hw->mng_cookie;
	u32 offset;
	s32 ret_val, hdr_csum, csum;
	u8 i, len;
	bool tx_filter = TRUE;

	DEBUGFUNC("e1000_enable_tx_pkt_filtering_generic");

	/* No manageability, no filtering */
	if (!hw->mac.ops.check_mng_mode(hw)) {
		tx_filter = FALSE;
		goto out;
	}

	/*
	 * If we can't read from the host interface for whatever
	 * reason, disable filtering.
	 */
	ret_val = hw->mac.ops.mng_enable_host_if(hw);
	if (ret_val != E1000_SUCCESS) {
		tx_filter = FALSE;
		goto out;
	}

	/* Read in the header.  Length and offset are in dwords. */
	len    = E1000_MNG_DHCP_COOKIE_LENGTH >> 2;
	offset = E1000_MNG_DHCP_COOKIE_OFFSET >> 2;
	for (i = 0; i < len; i++) {
		*(buffer + i) = E1000_READ_REG_ARRAY_DWORD(hw,
		                                           E1000_HOST_IF,
		                                           offset + i);
	}
	hdr_csum = hdr->checksum;
	hdr->checksum = 0;
	csum = e1000_calculate_checksum((u8 *)hdr,
	                                E1000_MNG_DHCP_COOKIE_LENGTH);
	/*
	 * If either the checksums or signature don't match, then
	 * the cookie area isn't considered valid, in which case we
	 * take the safe route of assuming Tx filtering is enabled.
	 */
	if (hdr_csum != csum)
		goto out;
	if (hdr->signature != E1000_IAMT_SIGNATURE)
		goto out;

	/* Cookie area is valid, make the final check for filtering. */
	if (!(hdr->status & E1000_MNG_DHCP_COOKIE_STATUS_PARSING))
		tx_filter = FALSE;

out:
	hw->mac.tx_pkt_filtering = tx_filter;
	return tx_filter;
}

/**
 *  e1000_mng_write_dhcp_info_generic - Writes DHCP info to host interface
 *  @hw: pointer to the HW structure
 *  @buffer: pointer to the host interface
 *  @length: size of the buffer
 *
 *  Writes the DHCP information to the host interface.
 **/
s32 e1000_mng_write_dhcp_info_generic(struct e1000_hw * hw, u8 *buffer,
                                      u16 length)
{
	struct e1000_host_mng_command_header hdr;
	s32 ret_val;
	u32 hicr;

	DEBUGFUNC("e1000_mng_write_dhcp_info_generic");

	hdr.command_id = E1000_MNG_DHCP_TX_PAYLOAD_CMD;
	hdr.command_length = length;
	hdr.reserved1 = 0;
	hdr.reserved2 = 0;
	hdr.checksum = 0;

	/* Enable the host interface */
	ret_val = hw->mac.ops.mng_enable_host_if(hw);
	if (ret_val)
		goto out;

	/* Populate the host interface with the contents of "buffer". */
	ret_val = hw->mac.ops.mng_host_if_write(hw, buffer, length,
	                                  sizeof(hdr), &(hdr.checksum));
	if (ret_val)
		goto out;

	/* Write the manageability command header */
	ret_val = hw->mac.ops.mng_write_cmd_header(hw, &hdr);
	if (ret_val)
		goto out;

	/* Tell the ARC a new command is pending. */
	hicr = E1000_READ_REG(hw, E1000_HICR);
	E1000_WRITE_REG(hw, E1000_HICR, hicr | E1000_HICR_C);

out:
	return ret_val;
}

/**
 *  e1000_mng_write_cmd_header_generic - Writes manageability command header
 *  @hw: pointer to the HW structure
 *  @hdr: pointer to the host interface command header
 *
 *  Writes the command header after does the checksum calculation.
 **/
s32 e1000_mng_write_cmd_header_generic(struct e1000_hw * hw,
                                    struct e1000_host_mng_command_header * hdr)
{
	u16 i, length = sizeof(struct e1000_host_mng_command_header);

	DEBUGFUNC("e1000_mng_write_cmd_header_generic");

	/* Write the whole command header structure with new checksum. */

	hdr->checksum = e1000_calculate_checksum((u8 *)hdr, length);

	length >>= 2;
	/* Write the relevant command block into the ram area. */
	for (i = 0; i < length; i++) {
		E1000_WRITE_REG_ARRAY_DWORD(hw, E1000_HOST_IF, i,
		                            *((u32 *) hdr + i));
		E1000_WRITE_FLUSH(hw);
	}

	return E1000_SUCCESS;
}

/**
 *  e1000_mng_host_if_write_generic - Write to the manageability host interface
 *  @hw: pointer to the HW structure
 *  @buffer: pointer to the host interface buffer
 *  @length: size of the buffer
 *  @offset: location in the buffer to write to
 *  @sum: sum of the data (not checksum)
 *
 *  This function writes the buffer content at the offset given on the host if.
 *  It also does alignment considerations to do the writes in most efficient
 *  way.  Also fills up the sum of the buffer in *buffer parameter.
 **/
s32 e1000_mng_host_if_write_generic(struct e1000_hw * hw, u8 *buffer,
                                    u16 length, u16 offset, u8 *sum)
{
	u8 *tmp;
	u8 *bufptr = buffer;
	u32 data = 0;
	s32 ret_val = E1000_SUCCESS;
	u16 remaining, i, j, prev_bytes;

	DEBUGFUNC("e1000_mng_host_if_write_generic");

	/* sum = only sum of the data and it is not checksum */

	if (length == 0 || offset + length > E1000_HI_MAX_MNG_DATA_LENGTH) {
		ret_val = -E1000_ERR_PARAM;
		goto out;
	}

	tmp = (u8 *)&data;
	prev_bytes = offset & 0x3;
	offset >>= 2;

	if (prev_bytes) {
		data = E1000_READ_REG_ARRAY_DWORD(hw, E1000_HOST_IF, offset);
		for (j = prev_bytes; j < sizeof(u32); j++) {
			*(tmp + j) = *bufptr++;
			*sum += *(tmp + j);
		}
		E1000_WRITE_REG_ARRAY_DWORD(hw, E1000_HOST_IF, offset, data);
		length -= j - prev_bytes;
		offset++;
	}

	remaining = length & 0x3;
	length -= remaining;

	/* Calculate length in DWORDs */
	length >>= 2;

	/*
	 * The device driver writes the relevant command block into the
	 * ram area.
	 */
	for (i = 0; i < length; i++) {
		for (j = 0; j < sizeof(u32); j++) {
			*(tmp + j) = *bufptr++;
			*sum += *(tmp + j);
		}

		E1000_WRITE_REG_ARRAY_DWORD(hw, E1000_HOST_IF, offset + i, data);
	}
	if (remaining) {
		for (j = 0; j < sizeof(u32); j++) {
			if (j < remaining)
				*(tmp + j) = *bufptr++;
			else
				*(tmp + j) = 0;

			*sum += *(tmp + j);
		}
		E1000_WRITE_REG_ARRAY_DWORD(hw, E1000_HOST_IF, offset + i, data);
	}

out:
	return ret_val;
}

/**
 *  e1000_enable_mng_pass_thru - Enable processing of ARP's
 *  @hw: pointer to the HW structure
 *
 *  Verifies the hardware needs to allow ARPs to be processed by the host.
 **/
bool e1000_enable_mng_pass_thru(struct e1000_hw *hw)
{
	u32 manc;
	u32 fwsm, factps;
	bool ret_val = FALSE;

	DEBUGFUNC("e1000_enable_mng_pass_thru");

	if (!hw->mac.asf_firmware_present)
		goto out;

	manc = E1000_READ_REG(hw, E1000_MANC);

	if (!(manc & E1000_MANC_RCV_TCO_EN) ||
	    !(manc & E1000_MANC_EN_MAC_ADDR_FILTER))
		goto out;

	if (hw->mac.arc_subsystem_valid) {
		fwsm = E1000_READ_REG(hw, E1000_FWSM);
		factps = E1000_READ_REG(hw, E1000_FACTPS);

		if (!(factps & E1000_FACTPS_MNGCG) &&
		    ((fwsm & E1000_FWSM_MODE_MASK) ==
		     (e1000_mng_mode_pt << E1000_FWSM_MODE_SHIFT))) {
			ret_val = TRUE;
			goto out;
		}
	} else {
		if ((manc & E1000_MANC_SMBUS_EN) &&
		    !(manc & E1000_MANC_ASF_EN)) {
			ret_val = TRUE;
			goto out;
		}
	}

out:
	return ret_val;
}

