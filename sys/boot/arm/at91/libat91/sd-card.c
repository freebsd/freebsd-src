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

//Private functions
//static void initInts(void);
static void AT91F_MCI_Handler(void);

//* Global Variables
AT91S_MciDeviceFeatures		MCI_Device_Features;
AT91S_MciDeviceDesc		MCI_Device_Desc;
AT91S_MciDevice			MCI_Device;
char				Buffer[BUFFER_SIZE_MCI_DEVICE];

/******************************************************************************
**Error return codes
******************************************************************************/
#define MCI_UNSUPP_SIZE_ERROR		5
#define MCI_UNSUPP_OFFSET_ERROR 6

//*----------------------------------------------------------------------------
//* \fn    AT91F_MCIDeviceWaitReady
//* \brief Wait for MCI Device ready
//*----------------------------------------------------------------------------
static void
AT91F_MCIDeviceWaitReady(unsigned int timeout)
{
	volatile int status;
	
	do
	{
		status = AT91C_BASE_MCI->MCI_SR;
		timeout--;
	}
	while( !(status & AT91C_MCI_NOTBUSY)  && (timeout>0) );	

#if IMP_DEBUG
	if (timeout == 0)
	    printf("Timeout, status is 0x%x\r\n", status);
#endif
	
	//TODO: Make interrupts work!
	AT91F_MCI_Handler();
}

#if 0
int
MCI_write (unsigned dest, char* source, unsigned length)
{
	unsigned sectorLength = MCI_Device.pMCI_DeviceFeatures->Max_Read_DataBlock_Lenfgth;
	unsigned offset = dest % sectorLength;
	AT91S_MCIDeviceStatus status;
	int sizeToWrite;

#if IMP_DEBUG
	printf("\r\n");
#endif

	//See if we are requested to write partial sectors, and have the capability to do so
	if ((length % sectorLength) && !(MCI_Device_Features.Write_Partial))
		//Return error if appropriat
		return MCI_UNSUPP_SIZE_ERROR;

	//See if we are requested to write to anywhere but a sectors' boundary
	//and have the capability to do so
	if ((offset) && !(MCI_Device_Features.Write_Partial))
		//Return error if appropriat
		return MCI_UNSUPP_OFFSET_ERROR;

	//If the address we're trying to write != sector boundary
	if (offset)
	{
		//* Wait MCI Device Ready
		AT91F_MCIDeviceWaitReady(AT91C_MCI_TIMEOUT);

		//Calculate the nr of bytes to write
		sizeToWrite = sectorLength - offset;
		//Do the writing
		status = AT91F_MCI_WriteBlock(&MCI_Device, dest, (unsigned int*)source, sizeToWrite);
		//TODO:Status checking

		//Update counters & pointers
		length -= sizeToWrite;
		dest += sizeToWrite;
		source += sizeToWrite;
	}

	//As long as there is data to write
	while (length)
	{
		//See if we've got at least a sector to write
		if (length > sectorLength) 
			sizeToWrite = sectorLength;
		//Else just write the remainder
		else
			sizeToWrite = length;

		AT91F_MCIDeviceWaitReady(AT91C_MCI_TIMEOUT);
		//Do the writing
		status = AT91F_MCI_WriteBlock(&MCI_Device, dest, (unsigned int*)source, sizeToWrite);
		//TODO:Status checking

		//Update counters & pointers
		length -= sizeToWrite;
		dest += sizeToWrite;
		source += sizeToWrite;
	}

	return 0;
}
#endif

inline static unsigned int
swap(unsigned int a)
{
    return (((a & 0xff) << 24) | ((a & 0xff00) << 8) | ((a & 0xff0000) >> 8)
      | ((a & 0xff000000) >> 24));
}

int
MCI_read(char* dest, unsigned source, unsigned length)
{
	unsigned sectorLength = MCI_Device.pMCI_DeviceFeatures->Max_Read_DataBlock_Length;
	unsigned log2sl = MCI_Device.pMCI_DeviceFeatures->READ_BL_LEN;
	unsigned slmask = ((1 << log2sl) - 1);
//	unsigned sector = (unsigned)source >> log2sl;
	unsigned offset = (unsigned)source & slmask;
	AT91S_MCIDeviceStatus status;
	int sizeToRead;
	unsigned int *walker;

#if IMP_DEBUG
	printf("Reading 0x%x bytes into ARM Addr 0x%x from card offset 0x%x\r\n",
	  length, dest, source);
#endif
	

	//See if we are requested to read partial sectors, and have the capability to do so
	if ((length & slmask) && !(MCI_Device_Features.Read_Partial))
		//Return error if appropriat
		return MCI_UNSUPP_SIZE_ERROR;

	//See if we are requested to read from anywhere but a sectors' boundary
	//and have the capability to do so
	if ((offset) && !(MCI_Device_Features.Read_Partial))
		//Return error if appropriat
		return MCI_UNSUPP_OFFSET_ERROR;

	//If the address we're trying to read != sector boundary
	if (offset) {
		//* Wait MCI Device Ready
		AT91F_MCIDeviceWaitReady(AT91C_MCI_TIMEOUT);

		//Calculate the nr of bytes to read
		sizeToRead = sectorLength - offset;
		//Do the writing
		status = AT91F_MCI_ReadBlock(&MCI_Device, source, (unsigned int*)dest, sizeToRead);
		//TODO:Status checking
		if (status != AT91C_READ_OK) {
#if IMP_DEBUG
		    printf("STATUS is 0x%x\r\n", status);
#endif
		    return -1;
		}
		
		//* Wait MCI Device Ready
		AT91F_MCIDeviceWaitReady(AT91C_MCI_TIMEOUT);
		// Fix erratum in MCI part
		for (walker = (unsigned int *)dest;
		     walker < (unsigned int *)(dest + sizeToRead); walker++)
		    *walker = swap(*walker);

		//Update counters & pointers
		length -= sizeToRead;
		dest += sizeToRead;
		source += sizeToRead;
	}

	//As long as there is data to read
	while (length)
	{
		//See if we've got at least a sector to read
		if (length > sectorLength)
			sizeToRead = sectorLength;
		//Else just write the remainder
		else
			sizeToRead = length;

		AT91F_MCIDeviceWaitReady(AT91C_MCI_TIMEOUT);
		//Do the writing
		status = AT91F_MCI_ReadBlock(&MCI_Device, source, (unsigned int*)dest, sizeToRead);
#if IMP_DEBUG
		printf("Reading 0x%x Addr 0x%x card 0x%x\r\n",
		  sizeToRead, dest, source);
#endif

		//TODO:Status checking
		if (status != AT91C_READ_OK) {
#if IMP_DEBUG
		        printf("STATUS is 0x%x\r\n", status);
#endif
			return -1;
		}

		//* Wait MCI Device Ready
		AT91F_MCIDeviceWaitReady(AT91C_MCI_TIMEOUT);

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
//* \fn    AT91F_CfgDevice
//* \brief This function is used to initialise MMC or SDCard Features
//*----------------------------------------------------------------------------
static void AT91F_CfgDevice(void)
{
	// Init Device Structure

	MCI_Device_Features.Relative_Card_Address 	= 0;
	MCI_Device_Features.Card_Inserted 		= AT91C_SD_CARD_INSERTED;
	MCI_Device_Features.Max_Read_DataBlock_Length	= 0;
	MCI_Device_Features.Max_Write_DataBlock_Length 	= 0;
	MCI_Device_Features.Read_Partial 		= 0;
	MCI_Device_Features.Write_Partial 		= 0;
	MCI_Device_Features.Erase_Block_Enable 		= 0;
	MCI_Device_Features.Sector_Size 		= 0;
	MCI_Device_Features.Memory_Capacity 		= 0;
	MCI_Device_Desc.state				= AT91C_MCI_IDLE;
	MCI_Device_Desc.SDCard_bus_width		= AT91C_MCI_SCDBUS;
	MCI_Device.pMCI_DeviceDesc 			= &MCI_Device_Desc;
	MCI_Device.pMCI_DeviceFeatures 			= &MCI_Device_Features;

}

static void AT91F_MCI_Handler(void)
{
	int status;

//	status = ( AT91C_BASE_MCI->MCI_SR & AT91C_BASE_MCI->MCI_IMR );
	status = AT91C_BASE_MCI->MCI_SR;

	AT91F_MCI_Device_Handler(&MCI_Device,status);
}

//*----------------------------------------------------------------------------
//* \fn    main
//* \brief main function
//*----------------------------------------------------------------------------
int
sdcard_init(void)
{
///////////////////////////////////////////////////////////////////////////////
//  MCI Init : common to MMC and SDCard
///////////////////////////////////////////////////////////////////////////////

	//initInts();

	// Init MCI for MMC and SDCard interface
	AT91F_MCI_CfgPIO();	
	AT91F_MCI_CfgPMC();
	AT91F_PDC_Open(AT91C_BASE_PDC_MCI);

	// Init MCI Device Structures
	AT91F_CfgDevice();

	AT91F_MCI_Configure(AT91C_BASE_MCI,
	    AT91C_MCI_DTOR_1MEGA_CYCLES,
	    AT91C_MCI_MR_PDCMODE,			// 15MHz for MCK = 60MHz (CLKDIV = 1)
	    AT91C_MCI_SDCARD_4BITS_SLOTA);
	
	if (AT91F_MCI_SDCard_Init(&MCI_Device) != AT91C_INIT_OK)
		return 0;
	return 1;
}
