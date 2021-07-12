/*-
 * Copyright 2021 Intel Corp
 * Copyright 2021 Rubicon Communications, LLC (Netgate)
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "igc_api.h"

static void igc_reload_nvm_generic(struct igc_hw *hw);

/**
 *  igc_init_nvm_ops_generic - Initialize NVM function pointers
 *  @hw: pointer to the HW structure
 *
 *  Setups up the function pointers to no-op functions
 **/
void igc_init_nvm_ops_generic(struct igc_hw *hw)
{
	struct igc_nvm_info *nvm = &hw->nvm;
	DEBUGFUNC("igc_init_nvm_ops_generic");

	/* Initialize function pointers */
	nvm->ops.init_params = igc_null_ops_generic;
	nvm->ops.acquire = igc_null_ops_generic;
	nvm->ops.read = igc_null_read_nvm;
	nvm->ops.release = igc_null_nvm_generic;
	nvm->ops.reload = igc_reload_nvm_generic;
	nvm->ops.update = igc_null_ops_generic;
	nvm->ops.validate = igc_null_ops_generic;
	nvm->ops.write = igc_null_write_nvm;
}

/**
 *  igc_null_nvm_read - No-op function, return 0
 *  @hw: pointer to the HW structure
 *  @a: dummy variable
 *  @b: dummy variable
 *  @c: dummy variable
 **/
s32 igc_null_read_nvm(struct igc_hw IGC_UNUSEDARG *hw,
			u16 IGC_UNUSEDARG a, u16 IGC_UNUSEDARG b,
			u16 IGC_UNUSEDARG *c)
{
	DEBUGFUNC("igc_null_read_nvm");
	return IGC_SUCCESS;
}

/**
 *  igc_null_nvm_generic - No-op function, return void
 *  @hw: pointer to the HW structure
 **/
void igc_null_nvm_generic(struct igc_hw IGC_UNUSEDARG *hw)
{
	DEBUGFUNC("igc_null_nvm_generic");
	return;
}

/**
 *  igc_null_write_nvm - No-op function, return 0
 *  @hw: pointer to the HW structure
 *  @a: dummy variable
 *  @b: dummy variable
 *  @c: dummy variable
 **/
s32 igc_null_write_nvm(struct igc_hw IGC_UNUSEDARG *hw,
			 u16 IGC_UNUSEDARG a, u16 IGC_UNUSEDARG b,
			 u16 IGC_UNUSEDARG *c)
{
	DEBUGFUNC("igc_null_write_nvm");
	return IGC_SUCCESS;
}

/**
 *  igc_raise_eec_clk - Raise EEPROM clock
 *  @hw: pointer to the HW structure
 *  @eecd: pointer to the EEPROM
 *
 *  Enable/Raise the EEPROM clock bit.
 **/
static void igc_raise_eec_clk(struct igc_hw *hw, u32 *eecd)
{
	*eecd = *eecd | IGC_EECD_SK;
	IGC_WRITE_REG(hw, IGC_EECD, *eecd);
	IGC_WRITE_FLUSH(hw);
	usec_delay(hw->nvm.delay_usec);
}

/**
 *  igc_lower_eec_clk - Lower EEPROM clock
 *  @hw: pointer to the HW structure
 *  @eecd: pointer to the EEPROM
 *
 *  Clear/Lower the EEPROM clock bit.
 **/
static void igc_lower_eec_clk(struct igc_hw *hw, u32 *eecd)
{
	*eecd = *eecd & ~IGC_EECD_SK;
	IGC_WRITE_REG(hw, IGC_EECD, *eecd);
	IGC_WRITE_FLUSH(hw);
	usec_delay(hw->nvm.delay_usec);
}

/**
 *  igc_shift_out_eec_bits - Shift data bits our to the EEPROM
 *  @hw: pointer to the HW structure
 *  @data: data to send to the EEPROM
 *  @count: number of bits to shift out
 *
 *  We need to shift 'count' bits out to the EEPROM.  So, the value in the
 *  "data" parameter will be shifted out to the EEPROM one bit at a time.
 *  In order to do this, "data" must be broken down into bits.
 **/
static void igc_shift_out_eec_bits(struct igc_hw *hw, u16 data, u16 count)
{
	struct igc_nvm_info *nvm = &hw->nvm;
	u32 eecd = IGC_READ_REG(hw, IGC_EECD);
	u32 mask;

	DEBUGFUNC("igc_shift_out_eec_bits");

	mask = 0x01 << (count - 1);
	if (nvm->type == igc_nvm_eeprom_spi)
		eecd |= IGC_EECD_DO;

	do {
		eecd &= ~IGC_EECD_DI;

		if (data & mask)
			eecd |= IGC_EECD_DI;

		IGC_WRITE_REG(hw, IGC_EECD, eecd);
		IGC_WRITE_FLUSH(hw);

		usec_delay(nvm->delay_usec);

		igc_raise_eec_clk(hw, &eecd);
		igc_lower_eec_clk(hw, &eecd);

		mask >>= 1;
	} while (mask);

	eecd &= ~IGC_EECD_DI;
	IGC_WRITE_REG(hw, IGC_EECD, eecd);
}

/**
 *  igc_shift_in_eec_bits - Shift data bits in from the EEPROM
 *  @hw: pointer to the HW structure
 *  @count: number of bits to shift in
 *
 *  In order to read a register from the EEPROM, we need to shift 'count' bits
 *  in from the EEPROM.  Bits are "shifted in" by raising the clock input to
 *  the EEPROM (setting the SK bit), and then reading the value of the data out
 *  "DO" bit.  During this "shifting in" process the data in "DI" bit should
 *  always be clear.
 **/
static u16 igc_shift_in_eec_bits(struct igc_hw *hw, u16 count)
{
	u32 eecd;
	u32 i;
	u16 data;

	DEBUGFUNC("igc_shift_in_eec_bits");

	eecd = IGC_READ_REG(hw, IGC_EECD);

	eecd &= ~(IGC_EECD_DO | IGC_EECD_DI);
	data = 0;

	for (i = 0; i < count; i++) {
		data <<= 1;
		igc_raise_eec_clk(hw, &eecd);

		eecd = IGC_READ_REG(hw, IGC_EECD);

		eecd &= ~IGC_EECD_DI;
		if (eecd & IGC_EECD_DO)
			data |= 1;

		igc_lower_eec_clk(hw, &eecd);
	}

	return data;
}

/**
 *  igc_poll_eerd_eewr_done - Poll for EEPROM read/write completion
 *  @hw: pointer to the HW structure
 *  @ee_reg: EEPROM flag for polling
 *
 *  Polls the EEPROM status bit for either read or write completion based
 *  upon the value of 'ee_reg'.
 **/
s32 igc_poll_eerd_eewr_done(struct igc_hw *hw, int ee_reg)
{
	u32 attempts = 100000;
	u32 i, reg = 0;

	DEBUGFUNC("igc_poll_eerd_eewr_done");

	for (i = 0; i < attempts; i++) {
		if (ee_reg == IGC_NVM_POLL_READ)
			reg = IGC_READ_REG(hw, IGC_EERD);
		else
			reg = IGC_READ_REG(hw, IGC_EEWR);

		if (reg & IGC_NVM_RW_REG_DONE)
			return IGC_SUCCESS;

		usec_delay(5);
	}

	return -IGC_ERR_NVM;
}

/**
 *  igc_acquire_nvm_generic - Generic request for access to EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Set the EEPROM access request bit and wait for EEPROM access grant bit.
 *  Return successful if access grant bit set, else clear the request for
 *  EEPROM access and return -IGC_ERR_NVM (-1).
 **/
s32 igc_acquire_nvm_generic(struct igc_hw *hw)
{
	u32 eecd = IGC_READ_REG(hw, IGC_EECD);
	s32 timeout = IGC_NVM_GRANT_ATTEMPTS;

	DEBUGFUNC("igc_acquire_nvm_generic");

	IGC_WRITE_REG(hw, IGC_EECD, eecd | IGC_EECD_REQ);
	eecd = IGC_READ_REG(hw, IGC_EECD);

	while (timeout) {
		if (eecd & IGC_EECD_GNT)
			break;
		usec_delay(5);
		eecd = IGC_READ_REG(hw, IGC_EECD);
		timeout--;
	}

	if (!timeout) {
		eecd &= ~IGC_EECD_REQ;
		IGC_WRITE_REG(hw, IGC_EECD, eecd);
		DEBUGOUT("Could not acquire NVM grant\n");
		return -IGC_ERR_NVM;
	}

	return IGC_SUCCESS;
}

/**
 *  igc_standby_nvm - Return EEPROM to standby state
 *  @hw: pointer to the HW structure
 *
 *  Return the EEPROM to a standby state.
 **/
static void igc_standby_nvm(struct igc_hw *hw)
{
	struct igc_nvm_info *nvm = &hw->nvm;
	u32 eecd = IGC_READ_REG(hw, IGC_EECD);

	DEBUGFUNC("igc_standby_nvm");

	if (nvm->type == igc_nvm_eeprom_spi) {
		/* Toggle CS to flush commands */
		eecd |= IGC_EECD_CS;
		IGC_WRITE_REG(hw, IGC_EECD, eecd);
		IGC_WRITE_FLUSH(hw);
		usec_delay(nvm->delay_usec);
		eecd &= ~IGC_EECD_CS;
		IGC_WRITE_REG(hw, IGC_EECD, eecd);
		IGC_WRITE_FLUSH(hw);
		usec_delay(nvm->delay_usec);
	}
}

/**
 *  igc_stop_nvm - Terminate EEPROM command
 *  @hw: pointer to the HW structure
 *
 *  Terminates the current command by inverting the EEPROM's chip select pin.
 **/
static void igc_stop_nvm(struct igc_hw *hw)
{
	u32 eecd;

	DEBUGFUNC("igc_stop_nvm");

	eecd = IGC_READ_REG(hw, IGC_EECD);
	if (hw->nvm.type == igc_nvm_eeprom_spi) {
		/* Pull CS high */
		eecd |= IGC_EECD_CS;
		igc_lower_eec_clk(hw, &eecd);
	}
}

/**
 *  igc_release_nvm_generic - Release exclusive access to EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Stop any current commands to the EEPROM and clear the EEPROM request bit.
 **/
void igc_release_nvm_generic(struct igc_hw *hw)
{
	u32 eecd;

	DEBUGFUNC("igc_release_nvm_generic");

	igc_stop_nvm(hw);

	eecd = IGC_READ_REG(hw, IGC_EECD);
	eecd &= ~IGC_EECD_REQ;
	IGC_WRITE_REG(hw, IGC_EECD, eecd);
}

/**
 *  igc_ready_nvm_eeprom - Prepares EEPROM for read/write
 *  @hw: pointer to the HW structure
 *
 *  Setups the EEPROM for reading and writing.
 **/
static s32 igc_ready_nvm_eeprom(struct igc_hw *hw)
{
	struct igc_nvm_info *nvm = &hw->nvm;
	u32 eecd = IGC_READ_REG(hw, IGC_EECD);
	u8 spi_stat_reg;

	DEBUGFUNC("igc_ready_nvm_eeprom");

	if (nvm->type == igc_nvm_eeprom_spi) {
		u16 timeout = NVM_MAX_RETRY_SPI;

		/* Clear SK and CS */
		eecd &= ~(IGC_EECD_CS | IGC_EECD_SK);
		IGC_WRITE_REG(hw, IGC_EECD, eecd);
		IGC_WRITE_FLUSH(hw);
		usec_delay(1);

		/* Read "Status Register" repeatedly until the LSB is cleared.
		 * The EEPROM will signal that the command has been completed
		 * by clearing bit 0 of the internal status register.  If it's
		 * not cleared within 'timeout', then error out.
		 */
		while (timeout) {
			igc_shift_out_eec_bits(hw, NVM_RDSR_OPCODE_SPI,
						 hw->nvm.opcode_bits);
			spi_stat_reg = (u8)igc_shift_in_eec_bits(hw, 8);
			if (!(spi_stat_reg & NVM_STATUS_RDY_SPI))
				break;

			usec_delay(5);
			igc_standby_nvm(hw);
			timeout--;
		}

		if (!timeout) {
			DEBUGOUT("SPI NVM Status error\n");
			return -IGC_ERR_NVM;
		}
	}

	return IGC_SUCCESS;
}

/**
 *  igc_read_nvm_eerd - Reads EEPROM using EERD register
 *  @hw: pointer to the HW structure
 *  @offset: offset of word in the EEPROM to read
 *  @words: number of words to read
 *  @data: word read from the EEPROM
 *
 *  Reads a 16 bit word from the EEPROM using the EERD register.
 **/
s32 igc_read_nvm_eerd(struct igc_hw *hw, u16 offset, u16 words, u16 *data)
{
	struct igc_nvm_info *nvm = &hw->nvm;
	u32 i, eerd = 0;
	s32 ret_val = IGC_SUCCESS;

	DEBUGFUNC("igc_read_nvm_eerd");

	/* A check for invalid values:  offset too large, too many words,
	 * too many words for the offset, and not enough words.
	 */
	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		DEBUGOUT("nvm parameter(s) out of bounds\n");
		return -IGC_ERR_NVM;
	}

	for (i = 0; i < words; i++) {
		eerd = ((offset + i) << IGC_NVM_RW_ADDR_SHIFT) +
		       IGC_NVM_RW_REG_START;

		IGC_WRITE_REG(hw, IGC_EERD, eerd);
		ret_val = igc_poll_eerd_eewr_done(hw, IGC_NVM_POLL_READ);
		if (ret_val)
			break;

		data[i] = (IGC_READ_REG(hw, IGC_EERD) >>
			   IGC_NVM_RW_REG_DATA);
	}

	if (ret_val)
		DEBUGOUT1("NVM read error: %d\n", ret_val);

	return ret_val;
}

/**
 *  igc_write_nvm_spi - Write to EEPROM using SPI
 *  @hw: pointer to the HW structure
 *  @offset: offset within the EEPROM to be written to
 *  @words: number of words to write
 *  @data: 16 bit word(s) to be written to the EEPROM
 *
 *  Writes data to EEPROM at offset using SPI interface.
 *
 *  If igc_update_nvm_checksum is not called after this function , the
 *  EEPROM will most likely contain an invalid checksum.
 **/
s32 igc_write_nvm_spi(struct igc_hw *hw, u16 offset, u16 words, u16 *data)
{
	struct igc_nvm_info *nvm = &hw->nvm;
	s32 ret_val = -IGC_ERR_NVM;
	u16 widx = 0;

	DEBUGFUNC("igc_write_nvm_spi");

	/* A check for invalid values:  offset too large, too many words,
	 * and not enough words.
	 */
	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		DEBUGOUT("nvm parameter(s) out of bounds\n");
		return -IGC_ERR_NVM;
	}

	while (widx < words) {
		u8 write_opcode = NVM_WRITE_OPCODE_SPI;

		ret_val = nvm->ops.acquire(hw);
		if (ret_val)
			return ret_val;

		ret_val = igc_ready_nvm_eeprom(hw);
		if (ret_val) {
			nvm->ops.release(hw);
			return ret_val;
		}

		igc_standby_nvm(hw);

		/* Send the WRITE ENABLE command (8 bit opcode) */
		igc_shift_out_eec_bits(hw, NVM_WREN_OPCODE_SPI,
					 nvm->opcode_bits);

		igc_standby_nvm(hw);

		/* Some SPI eeproms use the 8th address bit embedded in the
		 * opcode
		 */
		if ((nvm->address_bits == 8) && (offset >= 128))
			write_opcode |= NVM_A8_OPCODE_SPI;

		/* Send the Write command (8-bit opcode + addr) */
		igc_shift_out_eec_bits(hw, write_opcode, nvm->opcode_bits);
		igc_shift_out_eec_bits(hw, (u16)((offset + widx) * 2),
					 nvm->address_bits);

		/* Loop to allow for up to whole page write of eeprom */
		while (widx < words) {
			u16 word_out = data[widx];
			word_out = (word_out >> 8) | (word_out << 8);
			igc_shift_out_eec_bits(hw, word_out, 16);
			widx++;

			if ((((offset + widx) * 2) % nvm->page_size) == 0) {
				igc_standby_nvm(hw);
				break;
			}
		}
		msec_delay(10);
		nvm->ops.release(hw);
	}

	return ret_val;
}

/**
 *  igc_read_pba_string_generic - Read device part number
 *  @hw: pointer to the HW structure
 *  @pba_num: pointer to device part number
 *  @pba_num_size: size of part number buffer
 *
 *  Reads the product board assembly (PBA) number from the EEPROM and stores
 *  the value in pba_num.
 **/
s32 igc_read_pba_string_generic(struct igc_hw *hw, u8 *pba_num,
				  u32 pba_num_size)
{
	s32 ret_val;
	u16 nvm_data;
	u16 pba_ptr;
	u16 offset;
	u16 length;

	DEBUGFUNC("igc_read_pba_string_generic");

	if (pba_num == NULL) {
		DEBUGOUT("PBA string buffer was null\n");
		return -IGC_ERR_INVALID_ARGUMENT;
	}

	ret_val = hw->nvm.ops.read(hw, NVM_PBA_OFFSET_0, 1, &nvm_data);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		return ret_val;
	}

	ret_val = hw->nvm.ops.read(hw, NVM_PBA_OFFSET_1, 1, &pba_ptr);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		return ret_val;
	}

	/* if nvm_data is not ptr guard the PBA must be in legacy format which
	 * means pba_ptr is actually our second data word for the PBA number
	 * and we can decode it into an ascii string
	 */
	if (nvm_data != NVM_PBA_PTR_GUARD) {
		DEBUGOUT("NVM PBA number is not stored as string\n");

		/* make sure callers buffer is big enough to store the PBA */
		if (pba_num_size < IGC_PBANUM_LENGTH) {
			DEBUGOUT("PBA string buffer too small\n");
			return IGC_ERR_NO_SPACE;
		}

		/* extract hex string from data and pba_ptr */
		pba_num[0] = (nvm_data >> 12) & 0xF;
		pba_num[1] = (nvm_data >> 8) & 0xF;
		pba_num[2] = (nvm_data >> 4) & 0xF;
		pba_num[3] = nvm_data & 0xF;
		pba_num[4] = (pba_ptr >> 12) & 0xF;
		pba_num[5] = (pba_ptr >> 8) & 0xF;
		pba_num[6] = '-';
		pba_num[7] = 0;
		pba_num[8] = (pba_ptr >> 4) & 0xF;
		pba_num[9] = pba_ptr & 0xF;

		/* put a null character on the end of our string */
		pba_num[10] = '\0';

		/* switch all the data but the '-' to hex char */
		for (offset = 0; offset < 10; offset++) {
			if (pba_num[offset] < 0xA)
				pba_num[offset] += '0';
			else if (pba_num[offset] < 0x10)
				pba_num[offset] += 'A' - 0xA;
		}

		return IGC_SUCCESS;
	}

	ret_val = hw->nvm.ops.read(hw, pba_ptr, 1, &length);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		return ret_val;
	}

	if (length == 0xFFFF || length == 0) {
		DEBUGOUT("NVM PBA number section invalid length\n");
		return -IGC_ERR_NVM_PBA_SECTION;
	}
	/* check if pba_num buffer is big enough */
	if (pba_num_size < (((u32)length * 2) - 1)) {
		DEBUGOUT("PBA string buffer too small\n");
		return -IGC_ERR_NO_SPACE;
	}

	/* trim pba length from start of string */
	pba_ptr++;
	length--;

	for (offset = 0; offset < length; offset++) {
		ret_val = hw->nvm.ops.read(hw, pba_ptr + offset, 1, &nvm_data);
		if (ret_val) {
			DEBUGOUT("NVM Read Error\n");
			return ret_val;
		}
		pba_num[offset * 2] = (u8)(nvm_data >> 8);
		pba_num[(offset * 2) + 1] = (u8)(nvm_data & 0xFF);
	}
	pba_num[offset * 2] = '\0';

	return IGC_SUCCESS;
}





/**
 *  igc_read_mac_addr_generic - Read device MAC address
 *  @hw: pointer to the HW structure
 *
 *  Reads the device MAC address from the EEPROM and stores the value.
 *  Since devices with two ports use the same EEPROM, we increment the
 *  last bit in the MAC address for the second port.
 **/
s32 igc_read_mac_addr_generic(struct igc_hw *hw)
{
	u32 rar_high;
	u32 rar_low;
	u16 i;

	rar_high = IGC_READ_REG(hw, IGC_RAH(0));
	rar_low = IGC_READ_REG(hw, IGC_RAL(0));

	for (i = 0; i < IGC_RAL_MAC_ADDR_LEN; i++)
		hw->mac.perm_addr[i] = (u8)(rar_low >> (i*8));

	for (i = 0; i < IGC_RAH_MAC_ADDR_LEN; i++)
		hw->mac.perm_addr[i+4] = (u8)(rar_high >> (i*8));

	for (i = 0; i < ETH_ADDR_LEN; i++)
		hw->mac.addr[i] = hw->mac.perm_addr[i];

	return IGC_SUCCESS;
}

/**
 *  igc_validate_nvm_checksum_generic - Validate EEPROM checksum
 *  @hw: pointer to the HW structure
 *
 *  Calculates the EEPROM checksum by reading/adding each word of the EEPROM
 *  and then verifies that the sum of the EEPROM is equal to 0xBABA.
 **/
s32 igc_validate_nvm_checksum_generic(struct igc_hw *hw)
{
	s32 ret_val;
	u16 checksum = 0;
	u16 i, nvm_data;

	DEBUGFUNC("igc_validate_nvm_checksum_generic");

	for (i = 0; i < (NVM_CHECKSUM_REG + 1); i++) {
		ret_val = hw->nvm.ops.read(hw, i, 1, &nvm_data);
		if (ret_val) {
			DEBUGOUT("NVM Read Error\n");
			return ret_val;
		}
		checksum += nvm_data;
	}

	if (checksum != (u16) NVM_SUM) {
		DEBUGOUT("NVM Checksum Invalid\n");
		return -IGC_ERR_NVM;
	}

	return IGC_SUCCESS;
}

/**
 *  igc_update_nvm_checksum_generic - Update EEPROM checksum
 *  @hw: pointer to the HW structure
 *
 *  Updates the EEPROM checksum by reading/adding each word of the EEPROM
 *  up to the checksum.  Then calculates the EEPROM checksum and writes the
 *  value to the EEPROM.
 **/
s32 igc_update_nvm_checksum_generic(struct igc_hw *hw)
{
	s32 ret_val;
	u16 checksum = 0;
	u16 i, nvm_data;

	DEBUGFUNC("igc_update_nvm_checksum");

	for (i = 0; i < NVM_CHECKSUM_REG; i++) {
		ret_val = hw->nvm.ops.read(hw, i, 1, &nvm_data);
		if (ret_val) {
			DEBUGOUT("NVM Read Error while updating checksum.\n");
			return ret_val;
		}
		checksum += nvm_data;
	}
	checksum = (u16) NVM_SUM - checksum;
	ret_val = hw->nvm.ops.write(hw, NVM_CHECKSUM_REG, 1, &checksum);
	if (ret_val)
		DEBUGOUT("NVM Write Error while updating checksum.\n");

	return ret_val;
}

/**
 *  igc_reload_nvm_generic - Reloads EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Reloads the EEPROM by setting the "Reinitialize from EEPROM" bit in the
 *  extended control register.
 **/
static void igc_reload_nvm_generic(struct igc_hw *hw)
{
	u32 ctrl_ext;

	DEBUGFUNC("igc_reload_nvm_generic");

	usec_delay(10);
	ctrl_ext = IGC_READ_REG(hw, IGC_CTRL_EXT);
	ctrl_ext |= IGC_CTRL_EXT_EE_RST;
	IGC_WRITE_REG(hw, IGC_CTRL_EXT, ctrl_ext);
	IGC_WRITE_FLUSH(hw);
}


