/*******************************************************************************

  
  Copyright(c) 1999 - 2004 Intel Corporation. All rights reserved.
  
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
  
  Contact Information:
  Linux NICS <linux.nics@intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
*******************************************************************************/

/**********************************************************************
*                                                                     *
* INTEL CORPORATION                                                   *
*                                                                     *
* This software is supplied under the terms of the license included   *
* above.  All use of this driver must be in accordance with the terms *
* of that license.                                                    *
*                                                                     *
* Module Name:  e100_eeprom.c                                         *
*                                                                     *
* Abstract:     This module contains routines to read and write to a  *
*               serial EEPROM                                         *
*                                                                     *
* Environment:  This file is intended to be specific to the Linux     *
*               operating system.                                     *
*                                                                     *
**********************************************************************/
#include "e100.h"

#define CSR_EEPROM_CONTROL_FIELD(bdp) ((bdp)->scb->scb_eprm_cntrl)

#define CSR_GENERAL_CONTROL2_FIELD(bdp) \
	           ((bdp)->scb->scb_ext.d102_scb.scb_gen_ctrl2)

#define EEPROM_STALL_TIME	4
#define EEPROM_CHECKSUM		((u16) 0xBABA)
#define EEPROM_MAX_WORD_SIZE	256

void e100_eeprom_cleanup(struct e100_private *adapter);
u16 e100_eeprom_calculate_chksum(struct e100_private *adapter);
static void e100_eeprom_write_word(struct e100_private *adapter, u16 reg,
				   u16 data);
void e100_eeprom_write_block(struct e100_private *adapter, u16 start, u16 *data,
			     u16 size);
u16 e100_eeprom_size(struct e100_private *adapter);
u16 e100_eeprom_read(struct e100_private *adapter, u16 reg);

static void shift_out_bits(struct e100_private *adapter, u16 data, u16 count);
static u16 shift_in_bits(struct e100_private *adapter);
static void raise_clock(struct e100_private *adapter, u16 *x);
static void lower_clock(struct e100_private *adapter, u16 *x);
static u16 eeprom_wait_cmd_done(struct e100_private *adapter);
static void eeprom_stand_by(struct e100_private *adapter);

//----------------------------------------------------------------------------------------
// Procedure:   eeprom_set_semaphore
//
// Description: This function set (write 1) Gamla EEPROM semaphore bit (bit 23 word 0x1C in the CSR).
//
// Arguments:
//      Adapter                 - Adapter context
//
// Returns:  true if success
//           else return false 
//
//----------------------------------------------------------------------------------------

inline u8
eeprom_set_semaphore(struct e100_private *adapter)
{
	u16 data = 0;
	unsigned long expiration_time = jiffies + HZ / 100 + 1;

	do {
		// Get current value of General Control 2
		data = readb(&CSR_GENERAL_CONTROL2_FIELD(adapter));

		// Set bit 23 word 0x1C in the CSR.
		data |= SCB_GCR2_EEPROM_ACCESS_SEMAPHORE;
		writeb(data, &CSR_GENERAL_CONTROL2_FIELD(adapter));

		// Check to see if this bit set or not.
		data = readb(&CSR_GENERAL_CONTROL2_FIELD(adapter));

		if (data & SCB_GCR2_EEPROM_ACCESS_SEMAPHORE) {
			return true;
		}

		if (time_before(jiffies, expiration_time))
			yield();
		else
			return false;

	} while (true);
}

//----------------------------------------------------------------------------------------
// Procedure:   eeprom_reset_semaphore
//
// Description: This function reset (write 0) Gamla EEPROM semaphore bit 
//              (bit 23 word 0x1C in the CSR).
//
// Arguments:  struct e100_private * adapter - Adapter context
//----------------------------------------------------------------------------------------

inline void
eeprom_reset_semaphore(struct e100_private *adapter)
{
	u16 data = 0;

	data = readb(&CSR_GENERAL_CONTROL2_FIELD(adapter));
	data &= ~(SCB_GCR2_EEPROM_ACCESS_SEMAPHORE);
	writeb(data, &CSR_GENERAL_CONTROL2_FIELD(adapter));
}

//----------------------------------------------------------------------------------------
// Procedure:   e100_eeprom_size
//
// Description: This routine determines the size of the EEPROM.  This value should be
//              checked for validity - ie. is it too big or too small.  The size returned
//              is then passed to the read/write functions.
//
// Returns:
//      Size of the eeprom, or zero if an error occurred
//----------------------------------------------------------------------------------------
u16
e100_eeprom_size(struct e100_private *adapter)
{
	u16 x, size = 1;	// must be one to accumulate a product

	// if we've already stored this data, read from memory
	if (adapter->eeprom_size) {
		return adapter->eeprom_size;
	}
	// otherwise, read from the eeprom
	// Set EEPROM semaphore.
	if (adapter->rev_id >= D102_REV_ID) {
		if (!eeprom_set_semaphore(adapter))
			return 0;
	}
	// enable the eeprom by setting EECS.
	x = readw(&CSR_EEPROM_CONTROL_FIELD(adapter));
	x &= ~(EEDI | EEDO | EESK);
	x |= EECS;
	writew(x, &CSR_EEPROM_CONTROL_FIELD(adapter));

	// write the read opcode
	shift_out_bits(adapter, EEPROM_READ_OPCODE, 3);

	// experiment to discover the size of the eeprom.  request register zero
	// and wait for the eeprom to tell us it has accepted the entire address.
	x = readw(&CSR_EEPROM_CONTROL_FIELD(adapter));
	do {
		size *= 2;	// each bit of address doubles eeprom size
		x |= EEDO;	// set bit to detect "dummy zero"
		x &= ~EEDI;	// address consists of all zeros

		writew(x, &CSR_EEPROM_CONTROL_FIELD(adapter));
		readw(&(adapter->scb->scb_status));
		udelay(EEPROM_STALL_TIME);
		raise_clock(adapter, &x);
		lower_clock(adapter, &x);

		// check for "dummy zero"
		x = readw(&CSR_EEPROM_CONTROL_FIELD(adapter));
		if (size > EEPROM_MAX_WORD_SIZE) {
			size = 0;
			break;
		}
	} while (x & EEDO);

	// read in the value requested
	(void) shift_in_bits(adapter);
	e100_eeprom_cleanup(adapter);

	// Clear EEPROM Semaphore.
	if (adapter->rev_id >= D102_REV_ID) {
		eeprom_reset_semaphore(adapter);
	}

	return size;
}

//----------------------------------------------------------------------------------------
// Procedure:   eeprom_address_size
//
// Description: determines the number of bits in an address for the eeprom acceptable
//              values are 64, 128, and 256
// Arguments: size of the eeprom
// Returns: bits in an address for that size eeprom
//----------------------------------------------------------------------------------------

static inline int
eeprom_address_size(u16 size)
{
	int isize = size;
	
	return (ffs(isize) - 1);
}

//----------------------------------------------------------------------------------------
// Procedure:   e100_eeprom_read
//
// Description: This routine serially reads one word out of the EEPROM.
//
// Arguments:
//      adapter - our adapter context
//      reg - EEPROM word to read.
//
// Returns:
//      Contents of EEPROM word (reg).
//----------------------------------------------------------------------------------------

u16
e100_eeprom_read(struct e100_private *adapter, u16 reg)
{
	u16 x, data, bits;

	// Set EEPROM semaphore.
	if (adapter->rev_id >= D102_REV_ID) {
		if (!eeprom_set_semaphore(adapter))
			return 0;
	}
	// eeprom size is initialized to zero
	if (!adapter->eeprom_size)
		adapter->eeprom_size = e100_eeprom_size(adapter);

	bits = eeprom_address_size(adapter->eeprom_size);

	// select EEPROM, reset bits, set EECS
	x = readw(&CSR_EEPROM_CONTROL_FIELD(adapter));

	x &= ~(EEDI | EEDO | EESK);
	x |= EECS;
	writew(x, &CSR_EEPROM_CONTROL_FIELD(adapter));

	// write the read opcode and register number in that order
	// The opcode is 3bits in length, reg is 'bits' bits long
	shift_out_bits(adapter, EEPROM_READ_OPCODE, 3);
	shift_out_bits(adapter, reg, bits);

	// Now read the data (16 bits) in from the selected EEPROM word
	data = shift_in_bits(adapter);

	e100_eeprom_cleanup(adapter);

	// Clear EEPROM Semaphore.
	if (adapter->rev_id >= D102_REV_ID) {
		eeprom_reset_semaphore(adapter);
	}

	return data;
}

//----------------------------------------------------------------------------------------
// Procedure:   shift_out_bits
//
// Description: This routine shifts data bits out to the EEPROM.
//
// Arguments:
//      data - data to send to the EEPROM.
//      count - number of data bits to shift out.
//
// Returns: (none)
//----------------------------------------------------------------------------------------

static void
shift_out_bits(struct e100_private *adapter, u16 data, u16 count)
{
	u16 x, mask;

	mask = 1 << (count - 1);
	x = readw(&CSR_EEPROM_CONTROL_FIELD(adapter));
	x &= ~(EEDO | EEDI);

	do {
		x &= ~EEDI;
		if (data & mask)
			x |= EEDI;

		writew(x, &CSR_EEPROM_CONTROL_FIELD(adapter));
		readw(&(adapter->scb->scb_status)); /* flush command to card */
		udelay(EEPROM_STALL_TIME);
		raise_clock(adapter, &x);
		lower_clock(adapter, &x);
		mask = mask >> 1;
	} while (mask);

	x &= ~EEDI;
	writew(x, &CSR_EEPROM_CONTROL_FIELD(adapter));
}

//----------------------------------------------------------------------------------------
// Procedure:   raise_clock
//
// Description: This routine raises the EEPROM's clock input (EESK)
//
// Arguments:
//      x - Ptr to the EEPROM control register's current value
//
// Returns: (none)
//----------------------------------------------------------------------------------------

void
raise_clock(struct e100_private *adapter, u16 *x)
{
	*x = *x | EESK;
	writew(*x, &CSR_EEPROM_CONTROL_FIELD(adapter));
	readw(&(adapter->scb->scb_status)); /* flush command to card */
	udelay(EEPROM_STALL_TIME);
}

//----------------------------------------------------------------------------------------
// Procedure:   lower_clock
//
// Description: This routine lower's the EEPROM's clock input (EESK)
//
// Arguments:
//      x - Ptr to the EEPROM control register's current value
//
// Returns: (none)
//----------------------------------------------------------------------------------------

void
lower_clock(struct e100_private *adapter, u16 *x)
{
	*x = *x & ~EESK;
	writew(*x, &CSR_EEPROM_CONTROL_FIELD(adapter));
	readw(&(adapter->scb->scb_status)); /* flush command to card */
	udelay(EEPROM_STALL_TIME);
}

//----------------------------------------------------------------------------------------
// Procedure:   shift_in_bits
//
// Description: This routine shifts data bits in from the EEPROM.
//
// Arguments:
//
// Returns:
//      The contents of that particular EEPROM word
//----------------------------------------------------------------------------------------

static u16
shift_in_bits(struct e100_private *adapter)
{
	u16 x, d, i;

	x = readw(&CSR_EEPROM_CONTROL_FIELD(adapter));
	x &= ~(EEDO | EEDI);
	d = 0;

	for (i = 0; i < 16; i++) {
		d <<= 1;
		raise_clock(adapter, &x);

		x = readw(&CSR_EEPROM_CONTROL_FIELD(adapter));

		x &= ~EEDI;
		if (x & EEDO)
			d |= 1;

		lower_clock(adapter, &x);
	}

	return d;
}

//----------------------------------------------------------------------------------------
// Procedure:   e100_eeprom_cleanup
//
// Description: This routine returns the EEPROM to an idle state
//----------------------------------------------------------------------------------------

void
e100_eeprom_cleanup(struct e100_private *adapter)
{
	u16 x;

	x = readw(&CSR_EEPROM_CONTROL_FIELD(adapter));

	x &= ~(EECS | EEDI);
	writew(x, &CSR_EEPROM_CONTROL_FIELD(adapter));

	raise_clock(adapter, &x);
	lower_clock(adapter, &x);
}

//**********************************************************************************
// Procedure:   e100_eeprom_update_chksum
//
// Description: Calculates the checksum and writes it to the EEProm. 
//              It calculates the checksum accroding to the formula: 
//                              Checksum = 0xBABA - (sum of first 63 words).
//
//-----------------------------------------------------------------------------------
u16
e100_eeprom_calculate_chksum(struct e100_private *adapter)
{
	u16 idx, xsum_index, checksum = 0;

	// eeprom size is initialized to zero
	if (!adapter->eeprom_size)
		adapter->eeprom_size = e100_eeprom_size(adapter);

	xsum_index = adapter->eeprom_size - 1;
	for (idx = 0; idx < xsum_index; idx++)
		checksum += e100_eeprom_read(adapter, idx);

	checksum = EEPROM_CHECKSUM - checksum;
	return checksum;
}

//----------------------------------------------------------------------------------------
// Procedure:   e100_eeprom_write_word
//
// Description: This routine writes a word to a specific EEPROM location without.
//              taking EEPROM semaphore and updating checksum. 
//              Use e100_eeprom_write_block for the EEPROM update
// Arguments: reg - The EEPROM word that we are going to write to.
//            data - The data (word) that we are going to write to the EEPROM.
//----------------------------------------------------------------------------------------
static void
e100_eeprom_write_word(struct e100_private *adapter, u16 reg, u16 data)
{
	u16 x;
	u16 bits;

	bits = eeprom_address_size(adapter->eeprom_size);

	/* select EEPROM, mask off ASIC and reset bits, set EECS */
	x = readw(&CSR_EEPROM_CONTROL_FIELD(adapter));
	x &= ~(EEDI | EEDO | EESK);
	writew(x, &CSR_EEPROM_CONTROL_FIELD(adapter));
	readw(&(adapter->scb->scb_status)); /* flush command to card */
	udelay(EEPROM_STALL_TIME);
	x |= EECS;
	writew(x, &CSR_EEPROM_CONTROL_FIELD(adapter));

	shift_out_bits(adapter, EEPROM_EWEN_OPCODE, 5);
	shift_out_bits(adapter, reg, (u16) (bits - 2));
	if (!eeprom_wait_cmd_done(adapter))
		return;

	/* write the new word to the EEPROM & send the write opcode the EEPORM */
	shift_out_bits(adapter, EEPROM_WRITE_OPCODE, 3);

	/* select which word in the EEPROM that we are writing to */
	shift_out_bits(adapter, reg, bits);

	/* write the data to the selected EEPROM word */
	shift_out_bits(adapter, data, 16);
	if (!eeprom_wait_cmd_done(adapter))
		return;

	shift_out_bits(adapter, EEPROM_EWDS_OPCODE, 5);
	shift_out_bits(adapter, reg, (u16) (bits - 2));
	if (!eeprom_wait_cmd_done(adapter))
		return;

	e100_eeprom_cleanup(adapter);
}

//----------------------------------------------------------------------------------------
// Procedure:   e100_eeprom_write_block
//
// Description: This routine writes a block of words starting from specified EEPROM 
//              location and updates checksum
// Arguments: reg - The EEPROM word that we are going to write to.
//            data - The data (word) that we are going to write to the EEPROM.
//----------------------------------------------------------------------------------------
void
e100_eeprom_write_block(struct e100_private *adapter, u16 start, u16 *data,
			u16 size)
{
	u16 checksum;
	u16 i;

	if (!adapter->eeprom_size)
		adapter->eeprom_size = e100_eeprom_size(adapter);

	// Set EEPROM semaphore.
	if (adapter->rev_id >= D102_REV_ID) {
		if (!eeprom_set_semaphore(adapter))
			return;
	}

	for (i = 0; i < size; i++) {
		e100_eeprom_write_word(adapter, start + i, data[i]);
	}
	//Update checksum
	checksum = e100_eeprom_calculate_chksum(adapter);
	e100_eeprom_write_word(adapter, (adapter->eeprom_size - 1), checksum);

	// Clear EEPROM Semaphore.
	if (adapter->rev_id >= D102_REV_ID) {
		eeprom_reset_semaphore(adapter);
	}
}

//----------------------------------------------------------------------------------------
// Procedure:   eeprom_wait_cmd_done
//
// Description: This routine waits for the the EEPROM to finish its command.  
//                              Specifically, it waits for EEDO (data out) to go high.
// Returns:     true - If the command finished
//              false - If the command never finished (EEDO stayed low)
//----------------------------------------------------------------------------------------
static u16
eeprom_wait_cmd_done(struct e100_private *adapter)
{
	u16 x;
	unsigned long expiration_time = jiffies + HZ / 100 + 1;

	eeprom_stand_by(adapter);

	do {
		rmb();
		x = readw(&CSR_EEPROM_CONTROL_FIELD(adapter));
		if (x & EEDO)
			return true;
		if (time_before(jiffies, expiration_time))
			yield();
		else
			return false;
	} while (true);
}

//----------------------------------------------------------------------------------------
// Procedure:   eeprom_stand_by
//
// Description: This routine lowers the EEPROM chip select (EECS) for a few microseconds.
//----------------------------------------------------------------------------------------
static void
eeprom_stand_by(struct e100_private *adapter)
{
	u16 x;

	x = readw(&CSR_EEPROM_CONTROL_FIELD(adapter));
	x &= ~(EECS | EESK);
	writew(x, &CSR_EEPROM_CONTROL_FIELD(adapter));
	readw(&(adapter->scb->scb_status)); /* flush command to card */
	udelay(EEPROM_STALL_TIME);
	x |= EECS;
	writew(x, &CSR_EEPROM_CONTROL_FIELD(adapter));
	readw(&(adapter->scb->scb_status)); /* flush command to card */
	udelay(EEPROM_STALL_TIME);
}
