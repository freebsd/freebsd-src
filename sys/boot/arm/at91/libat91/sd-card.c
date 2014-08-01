/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software is derived from software provide by Kwikbyte who specifically
 * disclaimed copyright on the code.
 *
 * $FreeBSD$
 */

//*----------------------------------------------------------------------------
//*         ATMEL Microcontroller Software Support  -  ROUSSET  -
//*----------------------------------------------------------------------------
//* The software is delivered "AS IS" without warranty or condition of any
//* kind, either express, implied or statutory. This includes without
//* limitation any warranty or condition with respect to merchantability or
//* fitness for any particular purpose, or against the infringements of
//* intellectual property rights of others.
//*----------------------------------------------------------------------------
//* File Name           : main.c
//* Object              : main application written in C
//* Creation            : FB   21/11/2002
//*
//*----------------------------------------------------------------------------
#include "at91rm9200.h"
#include "lib_AT91RM9200.h"
#include "mci_device.h"
#include "lib.h"
#include "sd-card.h"

#define AT91C_MCI_TIMEOUT       1000000   /* For AT91F_MCIDeviceWaitReady */
#define SD_BLOCK_SIZE           512

//* Global Variables
static AT91S_MciDevice          MCI_Device;

/******************************************************************************
**Error return codes
******************************************************************************/
#define MCI_UNSUPP_SIZE_ERROR   5
#define MCI_UNSUPP_OFFSET_ERROR 6

//*----------------------------------------------------------------------------
//* \fn    MCIDeviceWaitReady
//* \brief Wait for MCI Device ready
//*----------------------------------------------------------------------------
static unsigned int
MCIDeviceWaitReady(unsigned int timeout)
{
	volatile unsigned int status;
	int waitfor;

	if (MCI_Device.state == AT91C_MCI_RX_SINGLE_BLOCK)
		waitfor = AT91C_MCI_RXBUFF;
	else
		waitfor = AT91C_MCI_NOTBUSY;
	do
	{
		status = AT91C_BASE_MCI->MCI_SR;
		timeout--;
	}
	while( !(status & waitfor) && (timeout>0) );	

	status = AT91C_BASE_MCI->MCI_SR;

	// If End of Tx Buffer Empty interrupt occurred
	if (MCI_Device.state == AT91C_MCI_TX_SINGLE_BLOCK && status & AT91C_MCI_TXBUFE) {
		AT91C_BASE_MCI->MCI_IDR = AT91C_MCI_TXBUFE;
 		AT91C_BASE_PDC_MCI->PDC_PTCR = AT91C_PDC_TXTDIS;
		MCI_Device.state = AT91C_MCI_IDLE;
	}	// End of if AT91C_MCI_TXBUFF

	// If End of Rx Buffer Full interrupt occurred
	if (MCI_Device.state == AT91C_MCI_RX_SINGLE_BLOCK && status & AT91C_MCI_RXBUFF) {
		AT91C_BASE_MCI->MCI_IDR = AT91C_MCI_RXBUFF;
 		AT91C_BASE_PDC_MCI->PDC_PTCR = AT91C_PDC_RXTDIS;
		MCI_Device.state = AT91C_MCI_IDLE;
	}	// End of if AT91C_MCI_RXBUFF

	//printf("WaitReady returning status %x\n", status);

	return status;
}

static inline unsigned int
swap(unsigned int v)
{
	unsigned int t1;

	__asm __volatile("eor %1, %0, %0, ror #16\n"
	    		"bic %1, %1, #0x00ff0000\n"
			"mov %0, %0, ror #8\n"
			"eor %0, %0, %1, lsr #8\n"
			 : "+r" (v), "=r" (t1));
	
	return (v);
}

inline static unsigned int
wait_ready()
{
	int status;
	int timeout = AT91C_MCI_TIMEOUT;

	// wait for CMDRDY Status flag to read the response
	do
	{
		status = AT91C_BASE_MCI->MCI_SR;
	} while( !(status & AT91C_MCI_CMDRDY) && (--timeout > 0)  );

	return status;
}

//*----------------------------------------------------------------------------
//* \fn    MCI_SendCommand
//* \brief Generic function to send a command to the MMC or SDCard
//*----------------------------------------------------------------------------
static int
MCI_SendCommand(
	unsigned int Cmd,
	unsigned int Arg)
{
	unsigned int error;
	unsigned int errorMask = AT91C_MCI_SR_ERROR;
	unsigned int opcode = Cmd & 0x3F;

	//printf("SendCmd %d (%x) arg %x\n", opcode, Cmd, Arg);

	// Don't check response CRC on ACMD41 (R3 response type).

	if (opcode == 41)
		errorMask &= ~AT91C_MCI_RCRCE;

	AT91C_BASE_MCI->MCI_ARGR = Arg;
	AT91C_BASE_MCI->MCI_CMDR = Cmd;

	error = wait_ready();

	if ((error & errorMask) != 0) {
		return (1);
	}
	return 0;
}

//*----------------------------------------------------------------------------
//* \fn    MCI_GetStatus
//* \brief Addressed card sends its status register
//*----------------------------------------------------------------------------
static unsigned int
MCI_GetStatus()
{
	if (MCI_SendCommand(SEND_STATUS_CMD, MCI_Device.RCA << 16))
		return 0;
	return (AT91C_BASE_MCI->MCI_RSPR[0]);

}

//*----------------------------------------------------------------------------
//* \fn    MCI_ReadBlock
//* \brief Start the read for a single 512-byte block
//*----------------------------------------------------------------------------
static int
MCI_StartReadBlock(unsigned blknum, void *dataBuffer)
{
        // Init Mode Register
	AT91C_BASE_MCI->MCI_MR |= ((SD_BLOCK_SIZE << 16) | AT91C_MCI_PDCMODE);
	 
	// (PDC) Receiver Transfer Enable
	AT91C_BASE_PDC_MCI->PDC_PTCR = (AT91C_PDC_TXTDIS | AT91C_PDC_RXTDIS);
	AT91C_BASE_PDC_MCI->PDC_RPR  = (unsigned int)dataBuffer;
	AT91C_BASE_PDC_MCI->PDC_RCR  = SD_BLOCK_SIZE / 4;;
	AT91C_BASE_PDC_MCI->PDC_PTCR = AT91C_PDC_RXTEN;

	// SDHC wants block offset, non-HC wants byte offset.
	if (!MCI_Device.IsSDHC)
		blknum *= SD_BLOCK_SIZE;

	// Send the Read single block command
	if (MCI_SendCommand(READ_SINGLE_BLOCK_CMD, blknum)) {
		return AT91C_READ_ERROR;
	}
	MCI_Device.state = AT91C_MCI_RX_SINGLE_BLOCK;

	return 0;
}

//*----------------------------------------------------------------------------
//* \fn    MCI_readblocks
//* \brief Read one or more blocks
//*----------------------------------------------------------------------------
int
MCI_readblocks(char* dest, unsigned blknum, unsigned blkcount)
{
	unsigned int status;
	unsigned int *walker;

	if (MCI_Device.state != AT91C_MCI_IDLE) {
		return 1;
	}

	if ((MCI_GetStatus() & AT91C_SR_READY_FOR_DATA) == 0) {
		return 1;
	}

	// As long as there is data to read
	while (blkcount)
	{
		//Do the reading
		if (MCI_StartReadBlock(blknum, dest))
			return -1;

		// Wait MCI Device Ready
		status = MCIDeviceWaitReady(AT91C_MCI_TIMEOUT);
		if (status & AT91C_MCI_SR_ERROR)
			return 1;

		// Fix erratum in MCI part - endian-swap all data.
		for (walker = (unsigned int *)dest;
		     walker < (unsigned int *)(dest + SD_BLOCK_SIZE); walker++)
		    *walker = swap(*walker);

		// Update counters & pointers
		++blknum;
		--blkcount;
		dest += SD_BLOCK_SIZE;
	}


	return 0;
}

//*----------------------------------------------------------------------------
//* \fn    MCI_read
//* \brief Legacy read function, takes byte offset and length but was always
//*  used to read full blocks; interface preserved for existing boot code.
//*----------------------------------------------------------------------------
int
MCI_read(char* dest, unsigned byteoffset, unsigned length)
{
	return MCI_readblocks(dest, 
	    byteoffset/SD_BLOCK_SIZE, length/SD_BLOCK_SIZE);
}

//*----------------------------------------------------------------------------
//* \fn    MCI_SDCard_SendAppCommand
//* \brief Specific function to send a specific command to the SDCard
//*----------------------------------------------------------------------------
static int
MCI_SDCard_SendAppCommand(
	unsigned int Cmd_App,
	unsigned int Arg)
{
	int status;

	if ((status = MCI_SendCommand(APP_CMD, (MCI_Device.RCA << 16))) == 0)
		status = MCI_SendCommand(Cmd_App,Arg);
	return status;
}

//*----------------------------------------------------------------------------
//* \fn    MCI_GetCSD
//* \brief Asks to the specified card to send its CSD
//*----------------------------------------------------------------------------
static int
MCI_GetCSD(unsigned int rca, unsigned int *response)
{
	if (MCI_SendCommand(SEND_CSD_CMD, (rca << 16)))
		return 1;
	
	response[0] = AT91C_BASE_MCI->MCI_RSPR[0];
	response[1] = AT91C_BASE_MCI->MCI_RSPR[1];
	response[2] = AT91C_BASE_MCI->MCI_RSPR[2];
	response[3] = AT91C_BASE_MCI->MCI_RSPR[3];
    
	return 0;
}

//*----------------------------------------------------------------------------
//* \fn    MCI_SDCard_GetOCR
//* \brief Wait for card to power up and determine whether it's SDHC or not.
//*----------------------------------------------------------------------------
static int
MCI_SDCard_GetOCR()
{
	unsigned int response;
	unsigned int arg = AT91C_MMC_HOST_VOLTAGE_RANGE;
	int          timeout = AT91C_MCI_TIMEOUT;

	// Force card to idle state.

	MCI_SendCommand(GO_IDLE_STATE_CMD, AT91C_NO_ARGUMENT);

	// Begin probe for SDHC by sending CMD8; only v2.0 cards respond to it.
	//
	// Arg is vvpp where vv is voltage range and pp is an arbitrary bit
	// pattern that gets echoed back in the response. The only voltage
	// ranges defined are:
	//   0x01 = 2.7 - 3.6
	//   0x02 = "reserved for low voltage" whatever that means.
	//
	// If the card fails to respond then it's not v2.0. If it responds by
	// echoing back exactly the arg we sent, then it's a v2.0 card and can
	// run at our voltage.  That means that when we send the ACMD41 (in
	// MCI_SDCard_GetOCR) we can include the HCS bit to inquire about SDHC.

	if (MCI_SendCommand(SD_SEND_IF_COND_CMD, 0x01AA) == 0) {
		MCI_Device.IsSDv2 = (AT91C_BASE_MCI->MCI_RSPR[0] == 0x01AA);
	}

	// If we've determined the card supports v2.0 functionality, set the
	// HCS/CCS bit to indicate that we support SDHC.  This will cause a
	// v2.0 card to report whether it is SDHC in the ACMD41 response.

	if (MCI_Device.IsSDv2) {
		arg |= AT91C_CCS;
	}

	// The RCA to be used for CMD55 in Idle state shall be the card's 
	// default RCA=0x0000.

	MCI_Device.RCA = 0x0;

	// Repeat ACMD41 until the card comes out of power-up-busy state.

	do {
		if (MCI_SDCard_SendAppCommand(SDCARD_APP_OP_COND_CMD, arg)) {
			return 1;
		}
		response = AT91C_BASE_MCI->MCI_RSPR[0];
	} while (!(response & AT91C_CARD_POWER_UP_DONE) && (--timeout > 0));

	// A v2.0 card sets CCS (card capacity status) in the response if it's SDHC.

	if (MCI_Device.IsSDv2) {
		MCI_Device.IsSDHC = ((response & AT91C_CCS) == AT91C_CCS);
	}

	return (0);
}

//*----------------------------------------------------------------------------
//* \fn    MCI_SDCard_GetCID
//* \brief Asks to the SDCard on the chosen slot to send its CID
//*----------------------------------------------------------------------------
static int
MCI_SDCard_GetCID(unsigned int *response)
{
	if (MCI_SendCommand(ALL_SEND_CID_CMD, AT91C_NO_ARGUMENT))
		return 1;
	
	response[0] = AT91C_BASE_MCI->MCI_RSPR[0];
	response[1] = AT91C_BASE_MCI->MCI_RSPR[1];
	response[2] = AT91C_BASE_MCI->MCI_RSPR[2];
	response[3] = AT91C_BASE_MCI->MCI_RSPR[3];
    
	return 0;
}

//*----------------------------------------------------------------------------
//* \fn    sdcard_4wire
//* \brief  Set bus width to 1-bit or 4-bit according to the parm.
//*
//* Unlike most functions in this file, the return value from this one is
//* bool-ish; returns 0 on failure, 1 on success.
//*----------------------------------------------------------------------------
int
sdcard_use4wire(int use4wire)
{
	volatile int	ret_value;

	do {
		ret_value=MCI_GetStatus();
	}
	while((ret_value > 0) && ((ret_value & AT91C_SR_READY_FOR_DATA) == 0));

	// If going to 4-wire mode, ask the card to turn off the DAT3 card detect
	// pullup resistor, if going to 1-wire ask it to turn it back on.

	ret_value = MCI_SDCard_SendAppCommand(SDCARD_SET_CLR_CARD_DETECT_CMD, 
					      use4wire ? 0 : 1);
	if (ret_value != AT91C_CMD_SEND_OK)
		return 0;

	// Ask the card to go into the requested mode.

	ret_value = MCI_SDCard_SendAppCommand(SDCARD_SET_BUS_WIDTH_CMD,
					      use4wire ? AT91C_BUS_WIDTH_4BITS : 
					                 AT91C_BUS_WIDTH_1BIT);
	if (ret_value != AT91C_CMD_SEND_OK)
		return 0;

	// Set the MCI device to match the mode we set in the card.

	if (use4wire) {
		MCI_Device.SDCard_bus_width = AT91C_BUS_WIDTH_4BITS;
		AT91C_BASE_MCI->MCI_SDCR |= AT91C_MCI_SCDBUS;
	} else {
		MCI_Device.SDCard_bus_width = AT91C_BUS_WIDTH_1BIT;
		AT91C_BASE_MCI->MCI_SDCR &= ~AT91C_MCI_SCDBUS;
	}

	return 1;
}

//*----------------------------------------------------------------------------
//* \fn    sdcard_init
//* \brief get the mci device ready to read from an SD or SDHC card.
//*
//* Unlike most functions in this file, the return value from this one is
//* bool-ish; returns 0 on failure, 1 on success.
//*----------------------------------------------------------------------------
int
sdcard_init(void)
{
	unsigned int	tab_response[4];
	int i;

	// Init MCI for MMC and SDCard interface
	AT91F_MCI_CfgPIO();	
	AT91F_MCI_CfgPMC();
	AT91F_PDC_Open(AT91C_BASE_PDC_MCI);

	// Init Device Structure
	MCI_Device.state		= AT91C_MCI_IDLE;
	MCI_Device.SDCard_bus_width	= 0;
	MCI_Device.IsSDv2		= 0;
	MCI_Device.IsSDHC		= 0;

	// Reset the MCI and set the bus speed.
	// Using MCK/230 gives a legal (under 400khz) bus speed for the card id
	// sequence for all reasonable master clock speeds.

	AT91C_BASE_MCI->MCI_CR = AT91C_MCI_MCIDIS | 0x80;
	AT91C_BASE_MCI->MCI_IDR = 0xFFFFFFFF;
	AT91C_BASE_MCI->MCI_DTOR = AT91C_MCI_DTOR_1MEGA_CYCLES;
	AT91C_BASE_MCI->MCI_MR = AT91C_MCI_PDCMODE | 114; /* clkdiv 114 = MCK/230 */
	AT91C_BASE_MCI->MCI_SDCR = AT91C_MCI_MMC_SLOTA;
	AT91C_BASE_MCI->MCI_CR = AT91C_MCI_MCIEN|AT91C_MCI_PWSEN;

	// Wait for the card to come out of power-up-busy state by repeatedly
	// sending ACMD41.  This also probes for SDHC versus standard cards.

	for (i = 0; i < 100; i++) {
		if (MCI_SDCard_GetOCR() == 0)
			break;
		if ((i & 0x01) == 0) {
			printf(".");
		}
	}
	if (i >= 100)
		return 0;

	if (MCI_SDCard_GetCID(tab_response))
		return 0;

	// Tell the card to set its address, and remember the result.

	if (MCI_SendCommand(SET_RELATIVE_ADDR_CMD, 0))
		return 0;
	MCI_Device.RCA = (AT91C_BASE_MCI->MCI_RSPR[0] >> 16);

	// After sending CMD3 (set addr) we can increase the clock to full speed.
	// Using MCK/4 gives a legal (under 25mhz) bus speed for all reasonable
	// master clock speeds.

	AT91C_BASE_MCI->MCI_MR = AT91C_MCI_PDCMODE | 1; /* clkdiv 1 = MCK/4 */

	if (MCI_GetCSD(MCI_Device.RCA,tab_response))
		return 0;
	MCI_Device.READ_BL_LEN = (tab_response[1] >> CSD_1_RD_B_LEN_S) &
	    CSD_1_RD_B_LEN_M;

#ifdef REPORT_SIZE
	{
		unsigned int	mult,blocknr;
		// compute MULT
		mult = 1 << ( ((tab_response[2] >> CSD_2_C_SIZE_M_S) &
		    CSD_2_C_SIZE_M_M) + 2 );
		// compute MSB of C_SIZE
		blocknr = ((tab_response[1] >> CSD_1_CSIZE_H_S) &
		    CSD_1_CSIZE_H_M) << 2;
		// compute MULT * (LSB of C-SIZE + MSB already computed + 1) = BLOCKNR
		blocknr = mult * ((blocknr + ((tab_response[2] >> CSD_2_CSIZE_L_S) &
		    CSD_2_CSIZE_L_M)) + 1);
		MCI_Device.Memory_Capacity = (1 << MCI_Device.READ_BL_LEN) * blocknr;
		printf("Found SD card %u bytes\n", MCI_Device.Memory_Capacity);
	}
#endif

	// Select card and set block length for following transfers.

	if (MCI_SendCommand(SEL_DESEL_CARD_CMD, (MCI_Device.RCA)<<16))
		return 0;
	if (MCI_SendCommand(SET_BLOCKLEN_CMD, SD_BLOCK_SIZE))
		return 0;

	return 1;
}
