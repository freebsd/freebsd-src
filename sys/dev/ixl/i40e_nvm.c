/******************************************************************************

  Copyright (c) 2013-2015, Intel Corporation 
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

#include "i40e_prototype.h"

enum i40e_status_code i40e_read_nvm_word_srctl(struct i40e_hw *hw, u16 offset,
					       u16 *data);
enum i40e_status_code i40e_read_nvm_word_aq(struct i40e_hw *hw, u16 offset,
					    u16 *data);
enum i40e_status_code i40e_read_nvm_buffer_srctl(struct i40e_hw *hw, u16 offset,
						 u16 *words, u16 *data);
enum i40e_status_code i40e_read_nvm_buffer_aq(struct i40e_hw *hw, u16 offset,
					      u16 *words, u16 *data);
enum i40e_status_code i40e_read_nvm_aq(struct i40e_hw *hw, u8 module_pointer,
				       u32 offset, u16 words, void *data,
				       bool last_command);

/**
 * i40e_init_nvm_ops - Initialize NVM function pointers
 * @hw: pointer to the HW structure
 *
 * Setup the function pointers and the NVM info structure. Should be called
 * once per NVM initialization, e.g. inside the i40e_init_shared_code().
 * Please notice that the NVM term is used here (& in all methods covered
 * in this file) as an equivalent of the FLASH part mapped into the SR.
 * We are accessing FLASH always thru the Shadow RAM.
 **/
enum i40e_status_code i40e_init_nvm(struct i40e_hw *hw)
{
	struct i40e_nvm_info *nvm = &hw->nvm;
	enum i40e_status_code ret_code = I40E_SUCCESS;
	u32 fla, gens;
	u8 sr_size;

	DEBUGFUNC("i40e_init_nvm");

	/* The SR size is stored regardless of the nvm programming mode
	 * as the blank mode may be used in the factory line.
	 */
	gens = rd32(hw, I40E_GLNVM_GENS);
	sr_size = ((gens & I40E_GLNVM_GENS_SR_SIZE_MASK) >>
			   I40E_GLNVM_GENS_SR_SIZE_SHIFT);
	/* Switching to words (sr_size contains power of 2KB) */
	nvm->sr_size = BIT(sr_size) * I40E_SR_WORDS_IN_1KB;

	/* Check if we are in the normal or blank NVM programming mode */
	fla = rd32(hw, I40E_GLNVM_FLA);
	if (fla & I40E_GLNVM_FLA_LOCKED_MASK) { /* Normal programming mode */
		/* Max NVM timeout */
		nvm->timeout = I40E_MAX_NVM_TIMEOUT;
		nvm->blank_nvm_mode = FALSE;
	} else { /* Blank programming mode */
		nvm->blank_nvm_mode = TRUE;
		ret_code = I40E_ERR_NVM_BLANK_MODE;
		i40e_debug(hw, I40E_DEBUG_NVM, "NVM init error: unsupported blank mode.\n");
	}

	return ret_code;
}

/**
 * i40e_acquire_nvm - Generic request for acquiring the NVM ownership
 * @hw: pointer to the HW structure
 * @access: NVM access type (read or write)
 *
 * This function will request NVM ownership for reading
 * via the proper Admin Command.
 **/
enum i40e_status_code i40e_acquire_nvm(struct i40e_hw *hw,
				       enum i40e_aq_resource_access_type access)
{
	enum i40e_status_code ret_code = I40E_SUCCESS;
	u64 gtime, timeout;
	u64 time_left = 0;

	DEBUGFUNC("i40e_acquire_nvm");

	if (hw->nvm.blank_nvm_mode)
		goto i40e_i40e_acquire_nvm_exit;

	ret_code = i40e_aq_request_resource(hw, I40E_NVM_RESOURCE_ID, access,
					    0, &time_left, NULL);
	/* Reading the Global Device Timer */
	gtime = rd32(hw, I40E_GLVFGEN_TIMER);

	/* Store the timeout */
	hw->nvm.hw_semaphore_timeout = I40E_MS_TO_GTIME(time_left) + gtime;

	if (ret_code)
		i40e_debug(hw, I40E_DEBUG_NVM,
			   "NVM acquire type %d failed time_left=%llu ret=%d aq_err=%d\n",
			   access, time_left, ret_code, hw->aq.asq_last_status);

	if (ret_code && time_left) {
		/* Poll until the current NVM owner timeouts */
		timeout = I40E_MS_TO_GTIME(I40E_MAX_NVM_TIMEOUT) + gtime;
		while ((gtime < timeout) && time_left) {
			i40e_msec_delay(10);
			gtime = rd32(hw, I40E_GLVFGEN_TIMER);
			ret_code = i40e_aq_request_resource(hw,
							I40E_NVM_RESOURCE_ID,
							access, 0, &time_left,
							NULL);
			if (ret_code == I40E_SUCCESS) {
				hw->nvm.hw_semaphore_timeout =
					    I40E_MS_TO_GTIME(time_left) + gtime;
				break;
			}
		}
		if (ret_code != I40E_SUCCESS) {
			hw->nvm.hw_semaphore_timeout = 0;
			i40e_debug(hw, I40E_DEBUG_NVM,
				   "NVM acquire timed out, wait %llu ms before trying again. status=%d aq_err=%d\n",
				   time_left, ret_code, hw->aq.asq_last_status);
		}
	}

i40e_i40e_acquire_nvm_exit:
	return ret_code;
}

/**
 * i40e_release_nvm - Generic request for releasing the NVM ownership
 * @hw: pointer to the HW structure
 *
 * This function will release NVM resource via the proper Admin Command.
 **/
void i40e_release_nvm(struct i40e_hw *hw)
{
	enum i40e_status_code ret_code = I40E_SUCCESS;
	u32 total_delay = 0;

	DEBUGFUNC("i40e_release_nvm");

	if (hw->nvm.blank_nvm_mode)
		return;

	ret_code = i40e_aq_release_resource(hw, I40E_NVM_RESOURCE_ID, 0, NULL);

	/* there are some rare cases when trying to release the resource
	 * results in an admin Q timeout, so handle them correctly
	 */
	while ((ret_code == I40E_ERR_ADMIN_QUEUE_TIMEOUT) &&
	       (total_delay < hw->aq.asq_cmd_timeout)) {
			i40e_msec_delay(1);
			ret_code = i40e_aq_release_resource(hw,
						I40E_NVM_RESOURCE_ID, 0, NULL);
			total_delay++;
	}
}

/**
 * i40e_poll_sr_srctl_done_bit - Polls the GLNVM_SRCTL done bit
 * @hw: pointer to the HW structure
 *
 * Polls the SRCTL Shadow RAM register done bit.
 **/
static enum i40e_status_code i40e_poll_sr_srctl_done_bit(struct i40e_hw *hw)
{
	enum i40e_status_code ret_code = I40E_ERR_TIMEOUT;
	u32 srctl, wait_cnt;

	DEBUGFUNC("i40e_poll_sr_srctl_done_bit");

	/* Poll the I40E_GLNVM_SRCTL until the done bit is set */
	for (wait_cnt = 0; wait_cnt < I40E_SRRD_SRCTL_ATTEMPTS; wait_cnt++) {
		srctl = rd32(hw, I40E_GLNVM_SRCTL);
		if (srctl & I40E_GLNVM_SRCTL_DONE_MASK) {
			ret_code = I40E_SUCCESS;
			break;
		}
		i40e_usec_delay(5);
	}
	if (ret_code == I40E_ERR_TIMEOUT)
		i40e_debug(hw, I40E_DEBUG_NVM, "Done bit in GLNVM_SRCTL not set");
	return ret_code;
}

/**
 * i40e_read_nvm_word - Reads Shadow RAM
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF)
 * @data: word read from the Shadow RAM
 *
 * Reads one 16 bit word from the Shadow RAM using the GLNVM_SRCTL register.
 **/
enum i40e_status_code i40e_read_nvm_word(struct i40e_hw *hw, u16 offset,
					 u16 *data)
{
#ifdef X722_SUPPORT
	if (hw->mac.type == I40E_MAC_X722)
		return i40e_read_nvm_word_aq(hw, offset, data);
#endif
	return i40e_read_nvm_word_srctl(hw, offset, data);
}

/**
 * i40e_read_nvm_word_srctl - Reads Shadow RAM via SRCTL register
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF)
 * @data: word read from the Shadow RAM
 *
 * Reads one 16 bit word from the Shadow RAM using the GLNVM_SRCTL register.
 **/
enum i40e_status_code i40e_read_nvm_word_srctl(struct i40e_hw *hw, u16 offset,
					       u16 *data)
{
	enum i40e_status_code ret_code = I40E_ERR_TIMEOUT;
	u32 sr_reg;

	DEBUGFUNC("i40e_read_nvm_word_srctl");

	if (offset >= hw->nvm.sr_size) {
		i40e_debug(hw, I40E_DEBUG_NVM,
			   "NVM read error: Offset %d beyond Shadow RAM limit %d\n",
			   offset, hw->nvm.sr_size);
		ret_code = I40E_ERR_PARAM;
		goto read_nvm_exit;
	}

	/* Poll the done bit first */
	ret_code = i40e_poll_sr_srctl_done_bit(hw);
	if (ret_code == I40E_SUCCESS) {
		/* Write the address and start reading */
		sr_reg = ((u32)offset << I40E_GLNVM_SRCTL_ADDR_SHIFT) |
			 BIT(I40E_GLNVM_SRCTL_START_SHIFT);
		wr32(hw, I40E_GLNVM_SRCTL, sr_reg);

		/* Poll I40E_GLNVM_SRCTL until the done bit is set */
		ret_code = i40e_poll_sr_srctl_done_bit(hw);
		if (ret_code == I40E_SUCCESS) {
			sr_reg = rd32(hw, I40E_GLNVM_SRDATA);
			*data = (u16)((sr_reg &
				       I40E_GLNVM_SRDATA_RDDATA_MASK)
				    >> I40E_GLNVM_SRDATA_RDDATA_SHIFT);
		}
	}
	if (ret_code != I40E_SUCCESS)
		i40e_debug(hw, I40E_DEBUG_NVM,
			   "NVM read error: Couldn't access Shadow RAM address: 0x%x\n",
			   offset);

read_nvm_exit:
	return ret_code;
}

/**
 * i40e_read_nvm_word_aq - Reads Shadow RAM via AQ
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF)
 * @data: word read from the Shadow RAM
 *
 * Reads one 16 bit word from the Shadow RAM using the GLNVM_SRCTL register.
 **/
enum i40e_status_code i40e_read_nvm_word_aq(struct i40e_hw *hw, u16 offset,
					    u16 *data)
{
	enum i40e_status_code ret_code = I40E_ERR_TIMEOUT;

	DEBUGFUNC("i40e_read_nvm_word_aq");

	ret_code = i40e_read_nvm_aq(hw, 0x0, offset, 1, data, TRUE);
	*data = LE16_TO_CPU(*(__le16 *)data);

	return ret_code;
}

/**
 * i40e_read_nvm_buffer - Reads Shadow RAM buffer
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF).
 * @words: (in) number of words to read; (out) number of words actually read
 * @data: words read from the Shadow RAM
 *
 * Reads 16 bit words (data buffer) from the SR using the i40e_read_nvm_srrd()
 * method. The buffer read is preceded by the NVM ownership take
 * and followed by the release.
 **/
enum i40e_status_code i40e_read_nvm_buffer(struct i40e_hw *hw, u16 offset,
					   u16 *words, u16 *data)
{
#ifdef X722_SUPPORT
	if (hw->mac.type == I40E_MAC_X722)
		return i40e_read_nvm_buffer_aq(hw, offset, words, data);
#endif
	return i40e_read_nvm_buffer_srctl(hw, offset, words, data);
}

/**
 * i40e_read_nvm_buffer_srctl - Reads Shadow RAM buffer via SRCTL register
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF).
 * @words: (in) number of words to read; (out) number of words actually read
 * @data: words read from the Shadow RAM
 *
 * Reads 16 bit words (data buffer) from the SR using the i40e_read_nvm_srrd()
 * method. The buffer read is preceded by the NVM ownership take
 * and followed by the release.
 **/
enum i40e_status_code i40e_read_nvm_buffer_srctl(struct i40e_hw *hw, u16 offset,
						 u16 *words, u16 *data)
{
	enum i40e_status_code ret_code = I40E_SUCCESS;
	u16 index, word;

	DEBUGFUNC("i40e_read_nvm_buffer_srctl");

	/* Loop thru the selected region */
	for (word = 0; word < *words; word++) {
		index = offset + word;
		ret_code = i40e_read_nvm_word_srctl(hw, index, &data[word]);
		if (ret_code != I40E_SUCCESS)
			break;
	}

	/* Update the number of words read from the Shadow RAM */
	*words = word;

	return ret_code;
}

/**
 * i40e_read_nvm_buffer_aq - Reads Shadow RAM buffer via AQ
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF).
 * @words: (in) number of words to read; (out) number of words actually read
 * @data: words read from the Shadow RAM
 *
 * Reads 16 bit words (data buffer) from the SR using the i40e_read_nvm_aq()
 * method. The buffer read is preceded by the NVM ownership take
 * and followed by the release.
 **/
enum i40e_status_code i40e_read_nvm_buffer_aq(struct i40e_hw *hw, u16 offset,
					      u16 *words, u16 *data)
{
	enum i40e_status_code ret_code;
	u16 read_size = *words;
	bool last_cmd = FALSE;
	u16 words_read = 0;
	u16 i = 0;

	DEBUGFUNC("i40e_read_nvm_buffer_aq");

	do {
		/* Calculate number of bytes we should read in this step.
		 * FVL AQ do not allow to read more than one page at a time or
		 * to cross page boundaries.
		 */
		if (offset % I40E_SR_SECTOR_SIZE_IN_WORDS)
			read_size = min(*words,
					(u16)(I40E_SR_SECTOR_SIZE_IN_WORDS -
				      (offset % I40E_SR_SECTOR_SIZE_IN_WORDS)));
		else
			read_size = min((*words - words_read),
					I40E_SR_SECTOR_SIZE_IN_WORDS);

		/* Check if this is last command, if so set proper flag */
		if ((words_read + read_size) >= *words)
			last_cmd = TRUE;

		ret_code = i40e_read_nvm_aq(hw, 0x0, offset, read_size,
					    data + words_read, last_cmd);
		if (ret_code != I40E_SUCCESS)
			goto read_nvm_buffer_aq_exit;

		/* Increment counter for words already read and move offset to
		 * new read location
		 */
		words_read += read_size;
		offset += read_size;
	} while (words_read < *words);

	for (i = 0; i < *words; i++)
		data[i] = LE16_TO_CPU(((__le16 *)data)[i]);

read_nvm_buffer_aq_exit:
	*words = words_read;
	return ret_code;
}

/**
 * i40e_read_nvm_aq - Read Shadow RAM.
 * @hw: pointer to the HW structure.
 * @module_pointer: module pointer location in words from the NVM beginning
 * @offset: offset in words from module start
 * @words: number of words to write
 * @data: buffer with words to write to the Shadow RAM
 * @last_command: tells the AdminQ that this is the last command
 *
 * Writes a 16 bit words buffer to the Shadow RAM using the admin command.
 **/
enum i40e_status_code i40e_read_nvm_aq(struct i40e_hw *hw, u8 module_pointer,
				       u32 offset, u16 words, void *data,
				       bool last_command)
{
	enum i40e_status_code ret_code = I40E_ERR_NVM;
	struct i40e_asq_cmd_details cmd_details;

	DEBUGFUNC("i40e_read_nvm_aq");

	memset(&cmd_details, 0, sizeof(cmd_details));
	cmd_details.wb_desc = &hw->nvm_wb_desc;

	/* Here we are checking the SR limit only for the flat memory model.
	 * We cannot do it for the module-based model, as we did not acquire
	 * the NVM resource yet (we cannot get the module pointer value).
	 * Firmware will check the module-based model.
	 */
	if ((offset + words) > hw->nvm.sr_size)
		i40e_debug(hw, I40E_DEBUG_NVM,
			   "NVM write error: offset %d beyond Shadow RAM limit %d\n",
			   (offset + words), hw->nvm.sr_size);
	else if (words > I40E_SR_SECTOR_SIZE_IN_WORDS)
		/* We can write only up to 4KB (one sector), in one AQ write */
		i40e_debug(hw, I40E_DEBUG_NVM,
			   "NVM write fail error: tried to write %d words, limit is %d.\n",
			   words, I40E_SR_SECTOR_SIZE_IN_WORDS);
	else if (((offset + (words - 1)) / I40E_SR_SECTOR_SIZE_IN_WORDS)
		 != (offset / I40E_SR_SECTOR_SIZE_IN_WORDS))
		/* A single write cannot spread over two sectors */
		i40e_debug(hw, I40E_DEBUG_NVM,
			   "NVM write error: cannot spread over two sectors in a single write offset=%d words=%d\n",
			   offset, words);
	else
		ret_code = i40e_aq_read_nvm(hw, module_pointer,
					    2 * offset,  /*bytes*/
					    2 * words,   /*bytes*/
					    data, last_command, &cmd_details);

	return ret_code;
}

/**
 * i40e_write_nvm_aq - Writes Shadow RAM.
 * @hw: pointer to the HW structure.
 * @module_pointer: module pointer location in words from the NVM beginning
 * @offset: offset in words from module start
 * @words: number of words to write
 * @data: buffer with words to write to the Shadow RAM
 * @last_command: tells the AdminQ that this is the last command
 *
 * Writes a 16 bit words buffer to the Shadow RAM using the admin command.
 **/
enum i40e_status_code i40e_write_nvm_aq(struct i40e_hw *hw, u8 module_pointer,
					u32 offset, u16 words, void *data,
					bool last_command)
{
	enum i40e_status_code ret_code = I40E_ERR_NVM;
	struct i40e_asq_cmd_details cmd_details;

	DEBUGFUNC("i40e_write_nvm_aq");

	memset(&cmd_details, 0, sizeof(cmd_details));
	cmd_details.wb_desc = &hw->nvm_wb_desc;

	/* Here we are checking the SR limit only for the flat memory model.
	 * We cannot do it for the module-based model, as we did not acquire
	 * the NVM resource yet (we cannot get the module pointer value).
	 * Firmware will check the module-based model.
	 */
	if ((offset + words) > hw->nvm.sr_size)
		DEBUGOUT("NVM write error: offset beyond Shadow RAM limit.\n");
	else if (words > I40E_SR_SECTOR_SIZE_IN_WORDS)
		/* We can write only up to 4KB (one sector), in one AQ write */
		DEBUGOUT("NVM write fail error: cannot write more than 4KB in a single write.\n");
	else if (((offset + (words - 1)) / I40E_SR_SECTOR_SIZE_IN_WORDS)
		 != (offset / I40E_SR_SECTOR_SIZE_IN_WORDS))
		/* A single write cannot spread over two sectors */
		DEBUGOUT("NVM write error: cannot spread over two sectors in a single write.\n");
	else
		ret_code = i40e_aq_update_nvm(hw, module_pointer,
					      2 * offset,  /*bytes*/
					      2 * words,   /*bytes*/
					      data, last_command, &cmd_details);

	return ret_code;
}

/**
 * i40e_write_nvm_word - Writes Shadow RAM word
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to write
 * @data: word to write to the Shadow RAM
 *
 * Writes a 16 bit word to the SR using the i40e_write_nvm_aq() method.
 * NVM ownership have to be acquired and released (on ARQ completion event
 * reception) by caller. To commit SR to NVM update checksum function
 * should be called.
 **/
enum i40e_status_code i40e_write_nvm_word(struct i40e_hw *hw, u32 offset,
					  void *data)
{
	DEBUGFUNC("i40e_write_nvm_word");

	*((__le16 *)data) = CPU_TO_LE16(*((u16 *)data));

	/* Value 0x00 below means that we treat SR as a flat mem */
	return i40e_write_nvm_aq(hw, 0x00, offset, 1, data, FALSE);
}

/**
 * i40e_write_nvm_buffer - Writes Shadow RAM buffer
 * @hw: pointer to the HW structure
 * @module_pointer: module pointer location in words from the NVM beginning
 * @offset: offset of the Shadow RAM buffer to write
 * @words: number of words to write
 * @data: words to write to the Shadow RAM
 *
 * Writes a 16 bit words buffer to the Shadow RAM using the admin command.
 * NVM ownership must be acquired before calling this function and released
 * on ARQ completion event reception by caller. To commit SR to NVM update
 * checksum function should be called.
 **/
enum i40e_status_code i40e_write_nvm_buffer(struct i40e_hw *hw,
					    u8 module_pointer, u32 offset,
					    u16 words, void *data)
{
	__le16 *le_word_ptr = (__le16 *)data;
	u16 *word_ptr = (u16 *)data;
	u32 i = 0;

	DEBUGFUNC("i40e_write_nvm_buffer");

	for (i = 0; i < words; i++)
		le_word_ptr[i] = CPU_TO_LE16(word_ptr[i]);

	/* Here we will only write one buffer as the size of the modules
	 * mirrored in the Shadow RAM is always less than 4K.
	 */
	return i40e_write_nvm_aq(hw, module_pointer, offset, words,
				 data, FALSE);
}

/**
 * i40e_calc_nvm_checksum - Calculates and returns the checksum
 * @hw: pointer to hardware structure
 * @checksum: pointer to the checksum
 *
 * This function calculates SW Checksum that covers the whole 64kB shadow RAM
 * except the VPD and PCIe ALT Auto-load modules. The structure and size of VPD
 * is customer specific and unknown. Therefore, this function skips all maximum
 * possible size of VPD (1kB).
 **/
enum i40e_status_code i40e_calc_nvm_checksum(struct i40e_hw *hw, u16 *checksum)
{
	enum i40e_status_code ret_code = I40E_SUCCESS;
	struct i40e_virt_mem vmem;
	u16 pcie_alt_module = 0;
	u16 checksum_local = 0;
	u16 vpd_module = 0;
	u16 *data;
	u16 i = 0;

	DEBUGFUNC("i40e_calc_nvm_checksum");

	ret_code = i40e_allocate_virt_mem(hw, &vmem,
				    I40E_SR_SECTOR_SIZE_IN_WORDS * sizeof(u16));
	if (ret_code)
		goto i40e_calc_nvm_checksum_exit;
	data = (u16 *)vmem.va;

	/* read pointer to VPD area */
	ret_code = i40e_read_nvm_word(hw, I40E_SR_VPD_PTR, &vpd_module);
	if (ret_code != I40E_SUCCESS) {
		ret_code = I40E_ERR_NVM_CHECKSUM;
		goto i40e_calc_nvm_checksum_exit;
	}

	/* read pointer to PCIe Alt Auto-load module */
	ret_code = i40e_read_nvm_word(hw, I40E_SR_PCIE_ALT_AUTO_LOAD_PTR,
				      &pcie_alt_module);
	if (ret_code != I40E_SUCCESS) {
		ret_code = I40E_ERR_NVM_CHECKSUM;
		goto i40e_calc_nvm_checksum_exit;
	}

	/* Calculate SW checksum that covers the whole 64kB shadow RAM
	 * except the VPD and PCIe ALT Auto-load modules
	 */
	for (i = 0; i < hw->nvm.sr_size; i++) {
		/* Read SR page */
		if ((i % I40E_SR_SECTOR_SIZE_IN_WORDS) == 0) {
			u16 words = I40E_SR_SECTOR_SIZE_IN_WORDS;

			ret_code = i40e_read_nvm_buffer(hw, i, &words, data);
			if (ret_code != I40E_SUCCESS) {
				ret_code = I40E_ERR_NVM_CHECKSUM;
				goto i40e_calc_nvm_checksum_exit;
			}
		}

		/* Skip Checksum word */
		if (i == I40E_SR_SW_CHECKSUM_WORD)
			continue;
		/* Skip VPD module (convert byte size to word count) */
		if ((i >= (u32)vpd_module) &&
		    (i < ((u32)vpd_module +
		     (I40E_SR_VPD_MODULE_MAX_SIZE / 2)))) {
			continue;
		}
		/* Skip PCIe ALT module (convert byte size to word count) */
		if ((i >= (u32)pcie_alt_module) &&
		    (i < ((u32)pcie_alt_module +
		     (I40E_SR_PCIE_ALT_MODULE_MAX_SIZE / 2)))) {
			continue;
		}

		checksum_local += data[i % I40E_SR_SECTOR_SIZE_IN_WORDS];
	}

	*checksum = (u16)I40E_SR_SW_CHECKSUM_BASE - checksum_local;

i40e_calc_nvm_checksum_exit:
	i40e_free_virt_mem(hw, &vmem);
	return ret_code;
}

/**
 * i40e_update_nvm_checksum - Updates the NVM checksum
 * @hw: pointer to hardware structure
 *
 * NVM ownership must be acquired before calling this function and released
 * on ARQ completion event reception by caller.
 * This function will commit SR to NVM.
 **/
enum i40e_status_code i40e_update_nvm_checksum(struct i40e_hw *hw)
{
	enum i40e_status_code ret_code = I40E_SUCCESS;
	u16 checksum;
	__le16 le_sum;

	DEBUGFUNC("i40e_update_nvm_checksum");

	ret_code = i40e_calc_nvm_checksum(hw, &checksum);
	le_sum = CPU_TO_LE16(checksum);
	if (ret_code == I40E_SUCCESS)
		ret_code = i40e_write_nvm_aq(hw, 0x00, I40E_SR_SW_CHECKSUM_WORD,
					     1, &le_sum, TRUE);

	return ret_code;
}

/**
 * i40e_validate_nvm_checksum - Validate EEPROM checksum
 * @hw: pointer to hardware structure
 * @checksum: calculated checksum
 *
 * Performs checksum calculation and validates the NVM SW checksum. If the
 * caller does not need checksum, the value can be NULL.
 **/
enum i40e_status_code i40e_validate_nvm_checksum(struct i40e_hw *hw,
						 u16 *checksum)
{
	enum i40e_status_code ret_code = I40E_SUCCESS;
	u16 checksum_sr = 0;
	u16 checksum_local = 0;

	DEBUGFUNC("i40e_validate_nvm_checksum");

	ret_code = i40e_calc_nvm_checksum(hw, &checksum_local);
	if (ret_code != I40E_SUCCESS)
		goto i40e_validate_nvm_checksum_exit;

	/* Do not use i40e_read_nvm_word() because we do not want to take
	 * the synchronization semaphores twice here.
	 */
	i40e_read_nvm_word(hw, I40E_SR_SW_CHECKSUM_WORD, &checksum_sr);

	/* Verify read checksum from EEPROM is the same as
	 * calculated checksum
	 */
	if (checksum_local != checksum_sr)
		ret_code = I40E_ERR_NVM_CHECKSUM;

	/* If the user cares, return the calculated checksum */
	if (checksum)
		*checksum = checksum_local;

i40e_validate_nvm_checksum_exit:
	return ret_code;
}
