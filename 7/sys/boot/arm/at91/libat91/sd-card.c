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

#define AT91C_MCI_TIMEOUT	1000000   /* For AT91F_MCIDeviceWaitReady */
#define BUFFER_SIZE_MCI_DEVICE	512
#define MASTER_CLOCK		60000000

//* Global Variables
AT91S_MciDevice			MCI_Device;
char				Buffer[BUFFER_SIZE_MCI_DEVICE];

/******************************************************************************
**Error return codes
******************************************************************************/
#define MCI_UNSUPP_SIZE_ERROR		5
#define MCI_UNSUPP_OFFSET_ERROR 6

//*----------------------------------------------------------------------------
//* \fn    MCIDeviceWaitReady
//* \brief Wait for MCI Device ready
//*----------------------------------------------------------------------------
static void
MCIDeviceWaitReady(unsigned int timeout)
{
	volatile int status;
	
	do
	{
		status = AT91C_BASE_MCI->MCI_SR;
		timeout--;
	}
	while( !(status & AT91C_MCI_NOTBUSY)  && (timeout>0) );	

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
}

inline static unsigned int
swap(unsigned int a)
{
    return (((a & 0xff) << 24) | ((a & 0xff00) << 8) | ((a & 0xff0000) >> 8)
      | ((a & 0xff000000) >> 24));
}

inline static void
wait_ready()
{
	int status;

	// wait for CMDRDY Status flag to read the response
	do
	{
		status = AT91C_BASE_MCI->MCI_SR;
	} while( !(status & AT91C_MCI_CMDRDY) );
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
	unsigned int	error;

	AT91C_BASE_MCI->MCI_ARGR = Arg;
	AT91C_BASE_MCI->MCI_CMDR = Cmd;

//	printf("CMDR %x ARG %x\n", Cmd, Arg);
	wait_ready();
	// Test error  ==> if crc error and response R3 ==> don't check error
	error = (AT91C_BASE_MCI->MCI_SR) & AT91C_MCI_SR_ERROR;
	if (error != 0) {
		if (error != AT91C_MCI_RCRCE)
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
		return AT91C_CMD_SEND_ERROR;
	return (AT91C_BASE_MCI->MCI_RSPR[0]);
}

//*----------------------------------------------------------------------------
//* \fn    MCI_ReadBlock
//* \brief Read an ENTIRE block or PARTIAL block
//*----------------------------------------------------------------------------
static int
MCI_ReadBlock(int src, unsigned int *dataBuffer, int sizeToRead)
{
//	unsigned log2sl = MCI_Device.READ_BL_LEN;
//	unsigned sectorLength = 1 << log2sl;
	unsigned sectorLength = 512;

	///////////////////////////////////////////////////////////////////////
	if (MCI_Device.state != AT91C_MCI_IDLE)
		return 1;
    
	if ((MCI_GetStatus() & AT91C_SR_READY_FOR_DATA) == 0)
		return 1;

	///////////////////////////////////////////////////////////////////////
      
        // Init Mode Register
	AT91C_BASE_MCI->MCI_MR |= ((sectorLength << 16) | AT91C_MCI_PDCMODE);
	 
	sizeToRead = sizeToRead / 4;

	AT91C_BASE_PDC_MCI->PDC_PTCR = (AT91C_PDC_TXTDIS | AT91C_PDC_RXTDIS);
	AT91C_BASE_PDC_MCI->PDC_RPR  = (unsigned int)dataBuffer;
	AT91C_BASE_PDC_MCI->PDC_RCR  = sizeToRead;

	// Send the Read single block command
	if (MCI_SendCommand(READ_SINGLE_BLOCK_CMD, src))
		return AT91C_READ_ERROR;
	MCI_Device.state = AT91C_MCI_RX_SINGLE_BLOCK;

	// Enable AT91C_MCI_RXBUFF Interrupt
	AT91C_BASE_MCI->MCI_IER = AT91C_MCI_RXBUFF;

	// (PDC) Receiver Transfer Enable
	AT91C_BASE_PDC_MCI->PDC_PTCR = AT91C_PDC_RXTEN;
	
	return 0;
}

int
MCI_read(char* dest, unsigned source, unsigned length)
{
//	unsigned log2sl = MCI_Device.READ_BL_LEN;
//	unsigned sectorLength = 1 << log2sl;
	unsigned sectorLength = 512;
	int sizeToRead;
	unsigned int *walker;

	//As long as there is data to read
	while (length)
	{
		if (length > sectorLength)
			sizeToRead = sectorLength;
		else
			sizeToRead = length;

		MCIDeviceWaitReady(AT91C_MCI_TIMEOUT);
		//Do the reading
		if (MCI_ReadBlock(source,
			(unsigned int*)dest, sizeToRead))
			return -1;

		//* Wait MCI Device Ready
		MCIDeviceWaitReady(AT91C_MCI_TIMEOUT);

		// Fix erratum in MCI part
		for (walker = (unsigned int *)dest;
		     walker < (unsigned int *)(dest + sizeToRead); walker++)
		    *walker = swap(*walker);

		//Update counters & pointers
		length -= sizeToRead;
		dest += sizeToRead;
		source += sizeToRead;
	}

	return 0;
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
	// Send the CMD55 for application specific command
	AT91C_BASE_MCI->MCI_ARGR = (MCI_Device.RCA << 16 );
	AT91C_BASE_MCI->MCI_CMDR = APP_CMD;

	wait_ready();
	// if an error occurs
	if (AT91C_BASE_MCI->MCI_SR & AT91C_MCI_SR_ERROR)
		return (1);
	return (MCI_SendCommand(Cmd_App,Arg));
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
//* \brief Asks to all cards to send their operations conditions
//*----------------------------------------------------------------------------
static int
MCI_SDCard_GetOCR()
{
	unsigned int	response=0x0;

	// The RCA to be used for CMD55 in Idle state shall be the card's default RCA=0x0000.
	MCI_Device.RCA = 0x0;
 	
 	while( (response & AT91C_CARD_POWER_UP_BUSY) != AT91C_CARD_POWER_UP_BUSY ) {
		if (MCI_SDCard_SendAppCommand(SDCARD_APP_OP_COND_CMD,
			AT91C_MMC_HOST_VOLTAGE_RANGE))
			return 1;
		response = AT91C_BASE_MCI->MCI_RSPR[0];
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
//* \fn    MCI_SDCard_SetBusWidth
//* \brief  Set bus width for SDCard
//*----------------------------------------------------------------------------
static int
MCI_SDCard_SetBusWidth()
{
	volatile int	ret_value;
	char			bus_width;

	do {
		ret_value=MCI_GetStatus();
	}
	while((ret_value > 0) && ((ret_value & AT91C_SR_READY_FOR_DATA) == 0));

	// Select Card
	MCI_SendCommand(SEL_DESEL_CARD_CMD, (MCI_Device.RCA)<<16);

	// Set bus width for Sdcard
	if (MCI_Device.SDCard_bus_width == AT91C_MCI_SCDBUS)
		bus_width = AT91C_BUS_WIDTH_4BITS;
	else
		bus_width = AT91C_BUS_WIDTH_1BIT;

	if (MCI_SDCard_SendAppCommand(
	      SDCARD_SET_BUS_WIDTH_CMD,bus_width) != AT91C_CMD_SEND_OK)
		return 1;

	return 0;
}

//*----------------------------------------------------------------------------
//* \fn    main
//* \brief main function
//*----------------------------------------------------------------------------
int
sdcard_init(void)
{
	unsigned int	tab_response[4];
#ifdef REPORT_SIZE
	unsigned int	mult,blocknr;
#endif
	int i;

	// Init MCI for MMC and SDCard interface
	AT91F_MCI_CfgPIO();	
	AT91F_MCI_CfgPMC();
	AT91F_PDC_Open(AT91C_BASE_PDC_MCI);

	// Init Device Structure
	MCI_Device.state		= AT91C_MCI_IDLE;
	MCI_Device.SDCard_bus_width	= AT91C_MCI_SCDBUS;

	//* Reset the MCI
	AT91C_BASE_MCI->MCI_CR = AT91C_MCI_MCIEN | AT91C_MCI_PWSEN;
	AT91C_BASE_MCI->MCI_IDR = 0xFFFFFFFF;
	AT91C_BASE_MCI->MCI_DTOR = AT91C_MCI_DTOR_1MEGA_CYCLES;
	AT91C_BASE_MCI->MCI_MR = AT91C_MCI_PDCMODE;
	AT91C_BASE_MCI->MCI_SDCR = AT91C_MCI_SDCARD_4BITS_SLOTA;
	MCI_SendCommand(GO_IDLE_STATE_CMD, AT91C_NO_ARGUMENT);

	for (i = 0; i < 100; i++) {
		if (!MCI_SDCard_GetOCR(&MCI_Device))
			break;
		printf(".");
	}
	if (i >= 100)
		return 0;
	if (MCI_SDCard_GetCID(tab_response))
		return 0;
	if (MCI_SendCommand(SET_RELATIVE_ADDR_CMD, 0))
		return 0;

	MCI_Device.RCA = (AT91C_BASE_MCI->MCI_RSPR[0] >> 16);
	if (MCI_GetCSD(MCI_Device.RCA,tab_response))
		return 0;
	MCI_Device.READ_BL_LEN = (tab_response[1] >> CSD_1_RD_B_LEN_S) &
	    CSD_1_RD_B_LEN_M;
#ifdef REPORT_SIZE
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
#endif
	if (MCI_SDCard_SetBusWidth())
		return 0;
	if (MCI_SendCommand(SET_BLOCKLEN_CMD, 1 << MCI_Device.READ_BL_LEN))
		return 0;
#ifdef REPORT_SIZE
	printf("Found SD card %u bytes\n", MCI_Device.Memory_Capacity);
#endif
	return 1;
}
