/*+M*************************************************************************
 * Perceptive Solutions, Inc. PSI-240I device driver proc support for Linux.
 *
 * Copyright (c) 1997 Perceptive Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *	File Name:		psi240i.c
 *
 *	Description:	SCSI driver for the PSI240I EIDE interface card.
 *
 *-M*************************************************************************/

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <asm/dma.h>
#include <asm/system.h>
#include <asm/io.h>
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"

#include "psi240i.h"
#include "psi_chip.h"

#include<linux/stat.h>

//#define DEBUG 1

#ifdef DEBUG
#define DEB(x) x
#else
#define DEB(x)
#endif

#define MAXBOARDS 2	/* Increase this and the sizes of the arrays below, if you need more. */

#define	PORT_DATA				0
#define	PORT_ERROR				1
#define	PORT_SECTOR_COUNT		2
#define	PORT_LBA_0				3
#define	PORT_LBA_8				4
#define	PORT_LBA_16				5
#define	PORT_LBA_24				6
#define	PORT_STAT_CMD			7
#define	PORT_SEL_FAIL			8
#define	PORT_IRQ_STATUS			9
#define	PORT_ADDRESS			10
#define	PORT_FAIL				11
#define	PORT_ALT_STAT		   	12

typedef struct
	{
	UCHAR		   	device;				// device code
	UCHAR			byte6;				// device select register image
	UCHAR			spigot;				// spigot number
	UCHAR			expectingIRQ;		// flag for expecting and interrupt
	USHORT			sectors;			// number of sectors per track
	USHORT			heads;				// number of heads
	USHORT			cylinders;			// number of cylinders for this device
	USHORT			spareword;			// placeholder
	ULONG			blocks;				// number of blocks on device
	}	OUR_DEVICE, *POUR_DEVICE;

typedef struct
	{
	USHORT		 ports[13];
	OUR_DEVICE	 device[8];
	Scsi_Cmnd	*pSCmnd;
	IDE_STRUCT	 ide;
	ULONG		 startSector;
	USHORT		 sectorCount;
	Scsi_Cmnd	*SCpnt;
	VOID		*buffer;
	USHORT		 expectingIRQ;
	}	ADAPTER240I, *PADAPTER240I;

#define HOSTDATA(host) ((PADAPTER240I)&host->hostdata)

static struct	Scsi_Host *PsiHost[6] = {NULL,};  /* One for each IRQ level (10-15) */
static			IDENTIFY_DATA	identifyData;
static			SETUP			ChipSetup;

static	USHORT	portAddr[6] = {CHIP_ADRS_0, CHIP_ADRS_1, CHIP_ADRS_2, CHIP_ADRS_3, CHIP_ADRS_4, CHIP_ADRS_5};

/****************************************************************
 *	Name:	WriteData	:LOCAL
 *
 *	Description:	Write data to device.
 *
 *	Parameters:		padapter - Pointer adapter data structure.
 *
 *	Returns:		TRUE if drive does not assert DRQ in time.
 *
 ****************************************************************/
static int WriteData (PADAPTER240I padapter)
	{
	ULONG	timer;
	USHORT *pports = padapter->ports;

	timer = jiffies + TIMEOUT_DRQ;								// calculate the timeout value
	do  {
		if ( inb_p (pports[PORT_STAT_CMD]) & IDE_STATUS_DRQ )
			{
			outsw (pports[PORT_DATA], padapter->buffer, (USHORT)padapter->ide.ide.ide[2] * 256);
			return 0;
			}
		}	while ( time_after(timer, jiffies) );									// test for timeout

	padapter->ide.ide.ides.cmd = 0;									// null out the command byte
	return 1;
	}
/****************************************************************
 *	Name:	IdeCmd	:LOCAL
 *
 *	Description:	Process a queued command from the SCSI manager.
 *
 *	Parameters:		padapter - Pointer adapter data structure.
 *
 *	Returns:		Zero if no error or status register contents on error.
 *
 ****************************************************************/
static UCHAR IdeCmd (PADAPTER240I padapter)
	{
	ULONG	timer;
	USHORT *pports = padapter->ports;
	UCHAR	status;

	outb_p (padapter->ide.ide.ides.spigot, pports[PORT_SEL_FAIL]);	// select the spigot
	outb_p (padapter->ide.ide.ide[6], pports[PORT_LBA_24]);			// select the drive
	timer = jiffies + TIMEOUT_READY;							// calculate the timeout value
	do  {
		status = inb_p (padapter->ports[PORT_STAT_CMD]);
		if ( status & IDE_STATUS_DRDY )
			{
			outb_p (padapter->ide.ide.ide[2], pports[PORT_SECTOR_COUNT]);
			outb_p (padapter->ide.ide.ide[3], pports[PORT_LBA_0]);
			outb_p (padapter->ide.ide.ide[4], pports[PORT_LBA_8]);
			outb_p (padapter->ide.ide.ide[5], pports[PORT_LBA_16]);
			padapter->expectingIRQ = 1;
			outb_p (padapter->ide.ide.ide[7], pports[PORT_STAT_CMD]);

			if ( padapter->ide.ide.ides.cmd == IDE_CMD_WRITE_MULTIPLE )
				return (WriteData (padapter));

			return 0;
			}
		}	while ( time_after(timer, jiffies) );									// test for timeout

	padapter->ide.ide.ides.cmd = 0;									// null out the command byte
	return status;
	}
/****************************************************************
 *	Name:	SetupTransfer	:LOCAL
 *
 *	Description:	Setup a data transfer command.
 *
 *	Parameters:		padapter - Pointer adapter data structure.
 *					drive	 - Drive/head register upper nibble only.
 *
 *	Returns:		TRUE if no data to transfer.
 *
 ****************************************************************/
static int SetupTransfer (PADAPTER240I padapter, UCHAR drive)
	{
	if ( padapter->sectorCount )
		{
		*(ULONG *)padapter->ide.ide.ides.lba = padapter->startSector;
		padapter->ide.ide.ide[6] |= drive;
		padapter->ide.ide.ides.sectors = ( padapter->sectorCount > SECTORSXFER ) ? SECTORSXFER : padapter->sectorCount;
		padapter->sectorCount -= padapter->ide.ide.ides.sectors;	// bump the start and count for next xfer
		padapter->startSector += padapter->ide.ide.ides.sectors;
		return 0;
		}
	else
		{
		padapter->ide.ide.ides.cmd = 0;								// null out the command byte
		padapter->SCpnt = NULL;
		return 1;
		}
	}
/****************************************************************
 *	Name:	DecodeError	:LOCAL
 *
 *	Description:	Decode and process device errors.
 *
 *	Parameters:		pshost - Pointer to host data block.
 *					status - Status register code.
 *
 *	Returns:		The driver status code.
 *
 ****************************************************************/
static ULONG DecodeError (struct Scsi_Host *pshost, UCHAR status)
	{
	PADAPTER240I	padapter = HOSTDATA(pshost);
	UCHAR			error;

	padapter->expectingIRQ = 0;
	padapter->SCpnt = NULL;
	if ( status & IDE_STATUS_WRITE_FAULT )
		{
		return DID_PARITY << 16;
		}
	if ( status & IDE_STATUS_BUSY )
		return DID_BUS_BUSY << 16;

	error = inb_p (padapter->ports[PORT_ERROR]);
	DEB(printk ("\npsi240i error register: %x", error));
	switch ( error )
		{
		case IDE_ERROR_AMNF:
		case IDE_ERROR_TKONF:
		case IDE_ERROR_ABRT:
		case IDE_ERROR_IDFN:
		case IDE_ERROR_UNC:
		case IDE_ERROR_BBK:
		default:
			return DID_ERROR << 16;
		}
	return DID_ERROR << 16;
	}
/****************************************************************
 *	Name:	Irq_Handler	:LOCAL
 *
 *	Description:	Interrupt handler.
 *
 *	Parameters:		irq		- Hardware IRQ number.
 *					dev_id	-
 *					regs	-
 *
 *	Returns:		TRUE if drive is not ready in time.
 *
 ****************************************************************/
static void Irq_Handler (int irq, void *dev_id, struct pt_regs *regs)
	{
	struct Scsi_Host   *shost;			// Pointer to host data block
	PADAPTER240I		padapter;		// Pointer to adapter control structure
	USHORT		 	   *pports;			// I/O port array
	Scsi_Cmnd		   *SCpnt;
	UCHAR				status;
	int					z;

	DEB(printk ("\npsi240i received interrupt\n"));

	shost = PsiHost[irq - 10];
	if ( !shost )
		panic ("Splunge!");

	padapter = HOSTDATA(shost);
	pports = padapter->ports;
	SCpnt = padapter->SCpnt;

	if ( !padapter->expectingIRQ )
		{
		DEB(printk ("\npsi240i Unsolicited interrupt\n"));
		return;
		}
	padapter->expectingIRQ = 0;

	status = inb_p (padapter->ports[PORT_STAT_CMD]);			// read the device status
	if ( status & (IDE_STATUS_ERROR | IDE_STATUS_WRITE_FAULT) )
		goto irqerror;

	DEB(printk ("\npsi240i processing interrupt"));
	switch ( padapter->ide.ide.ides.cmd )							// decide how to handle the interrupt
		{
		case IDE_CMD_READ_MULTIPLE:
			if ( status & IDE_STATUS_DRQ )
				{
				insw (pports[PORT_DATA], padapter->buffer, (USHORT)padapter->ide.ide.ides.sectors * 256);
				padapter->buffer += padapter->ide.ide.ides.sectors * 512;
				if ( SetupTransfer (padapter, padapter->ide.ide.ide[6] & 0xF0) )
					{
					SCpnt->result = DID_OK << 16;
					padapter->SCpnt = NULL;
					SCpnt->scsi_done (SCpnt);
					return;
					}
				if ( !(status = IdeCmd (padapter)) )
					return;
				}
			break;

		case IDE_CMD_WRITE_MULTIPLE:
			padapter->buffer += padapter->ide.ide.ides.sectors * 512;
			if ( SetupTransfer (padapter, padapter->ide.ide.ide[6] & 0xF0) )
				{
				SCpnt->result = DID_OK << 16;
				padapter->SCpnt = NULL;
				SCpnt->scsi_done (SCpnt);
				return;
				}
			if ( !(status = IdeCmd (padapter)) )
				return;
			break;

		case IDE_COMMAND_IDENTIFY:
			{
			PINQUIRYDATA	pinquiryData  = SCpnt->request_buffer;

			if ( status & IDE_STATUS_DRQ )
				{
				insw (pports[PORT_DATA], &identifyData, sizeof (identifyData) >> 1);

				memset (pinquiryData, 0, SCpnt->request_bufflen);		// Zero INQUIRY data structure.
				pinquiryData->DeviceType = 0;
				pinquiryData->Versions = 2;
				pinquiryData->AdditionalLength = 35 - 4;

				// Fill in vendor identification fields.
				for ( z = 0;  z < 20;  z += 2 )
					{
					pinquiryData->VendorId[z]	  = ((UCHAR *)identifyData.ModelNumber)[z + 1];
					pinquiryData->VendorId[z + 1] = ((UCHAR *)identifyData.ModelNumber)[z];
					}

				// Initialize unused portion of product id.
				for ( z = 0;  z < 4;  z++ )
					pinquiryData->ProductId[12 + z] = ' ';

				// Move firmware revision from IDENTIFY data to
				// product revision in INQUIRY data.
				for ( z = 0;  z < 4;  z += 2 )
					{
					pinquiryData->ProductRevisionLevel[z]	 = ((UCHAR *)identifyData.FirmwareRevision)[z + 1];
					pinquiryData->ProductRevisionLevel[z + 1] = ((UCHAR *)identifyData.FirmwareRevision)[z];
					}

				SCpnt->result = DID_OK << 16;
				padapter->SCpnt = NULL;
				SCpnt->scsi_done (SCpnt);
				return;
				}
			break;
			}

		default:
			SCpnt->result = DID_OK << 16;
			padapter->SCpnt = NULL;
			SCpnt->scsi_done (SCpnt);
			return;
		}

irqerror:;
	DEB(printk ("\npsi240i error  Device Status: %X\n", status));
	SCpnt->result = DecodeError (shost, status);
	SCpnt->scsi_done (SCpnt);
	}
static void do_Irq_Handler (int irq, void *dev_id, struct pt_regs *regs)
	{
	unsigned long flags;

	spin_lock_irqsave(&io_request_lock, flags);
	Irq_Handler(irq, dev_id, regs);
	spin_unlock_irqrestore(&io_request_lock, flags);
	}
/****************************************************************
 *	Name:	Psi240i_QueueCommand
 *
 *	Description:	Process a queued command from the SCSI manager.
 *
 *	Parameters:		SCpnt - Pointer to SCSI command structure.
 *					done  - Pointer to done function to call.
 *
 *	Returns:		Status code.
 *
 ****************************************************************/
int Psi240i_QueueCommand (Scsi_Cmnd *SCpnt, void (*done)(Scsi_Cmnd *))
	{
	UCHAR		   *cdb = (UCHAR *)SCpnt->cmnd;					// Pointer to SCSI CDB
	PADAPTER240I	padapter = HOSTDATA(SCpnt->host);			// Pointer to adapter control structure
	POUR_DEVICE		pdev	 = &padapter->device[SCpnt->target];// Pointer to device information
	UCHAR			rc;											// command return code

	SCpnt->scsi_done = done;
	padapter->ide.ide.ides.spigot = pdev->spigot;
	padapter->buffer = SCpnt->request_buffer;
	if (done)
		{
		if ( !pdev->device )
			{
			SCpnt->result = DID_BAD_TARGET << 16;
			done (SCpnt);
			return 0;
			}
		}
	else
		{
		printk("psi240i_queuecommand: %02X: done can't be NULL\n", *cdb);
		return 0;
		}

	switch ( *cdb )
		{
		case SCSIOP_INQUIRY:   					// inquiry CDB
			{
			padapter->ide.ide.ide[6] = pdev->byte6;
			padapter->ide.ide.ides.cmd = IDE_COMMAND_IDENTIFY;
			break;
			}

		case SCSIOP_TEST_UNIT_READY:			// test unit ready CDB
			SCpnt->result = DID_OK << 16;
			done (SCpnt);
			return 0;

		case SCSIOP_READ_CAPACITY:			  	// read capctiy CDB
			{
			PREAD_CAPACITY_DATA	pdata = (PREAD_CAPACITY_DATA)SCpnt->request_buffer;

			pdata->blksiz = 0x20000;
			XANY2SCSI ((UCHAR *)&pdata->blks, pdev->blocks);
			SCpnt->result = DID_OK << 16;
			done (SCpnt);
			return 0;
			}

		case SCSIOP_VERIFY:						// verify CDB
			*(ULONG *)padapter->ide.ide.ides.lba = XSCSI2LONG (&cdb[2]);
			padapter->ide.ide.ide[6] |= pdev->byte6;
			padapter->ide.ide.ide[2] = (UCHAR)((USHORT)cdb[8] | ((USHORT)cdb[7] << 8));
			padapter->ide.ide.ides.cmd = IDE_COMMAND_VERIFY;
			break;

		case SCSIOP_READ:						// read10 CDB
			padapter->startSector = XSCSI2LONG (&cdb[2]);
			padapter->sectorCount = (USHORT)cdb[8] | ((USHORT)cdb[7] << 8);
			SetupTransfer (padapter, pdev->byte6);
			padapter->ide.ide.ides.cmd = IDE_CMD_READ_MULTIPLE;
			break;

		case SCSIOP_READ6:						// read6  CDB
			padapter->startSector = SCSI2LONG (&cdb[1]);
			padapter->sectorCount = cdb[4];
			SetupTransfer (padapter, pdev->byte6);
			padapter->ide.ide.ides.cmd = IDE_CMD_READ_MULTIPLE;
			break;

		case SCSIOP_WRITE:						// write10 CDB
			padapter->startSector = XSCSI2LONG (&cdb[2]);
			padapter->sectorCount = (USHORT)cdb[8] | ((USHORT)cdb[7] << 8);
			SetupTransfer (padapter, pdev->byte6);
			padapter->ide.ide.ides.cmd = IDE_CMD_WRITE_MULTIPLE;
			break;
		case SCSIOP_WRITE6:						// write6  CDB
			padapter->startSector = SCSI2LONG (&cdb[1]);
			padapter->sectorCount = cdb[4];
			SetupTransfer (padapter, pdev->byte6);
			padapter->ide.ide.ides.cmd = IDE_CMD_WRITE_MULTIPLE;
			break;

		default:
			DEB (printk ("psi240i_queuecommand: Unsupported command %02X\n", *cdb));
			SCpnt->result = DID_ERROR << 16;
			done (SCpnt);
			return 0;
		}

	padapter->SCpnt = SCpnt;  									// Save this command data

	rc = IdeCmd (padapter);
	if ( rc )
		{
		padapter->expectingIRQ = 0;
		DEB (printk ("psi240i_queuecommand: %02X, %02X: Device failed to respond for command\n", *cdb, padapter->ide.ide.ides.cmd));
		SCpnt->result = DID_ERROR << 16;
		done (SCpnt);
		return 0;
		}
	DEB (printk("psi240i_queuecommand: %02X, %02X now waiting for interrupt ", *cdb, padapter->ide.ide.ides.cmd));
	return 0;
	}

static void internal_done(Scsi_Cmnd * SCpnt)
	{
	SCpnt->SCp.Status++;
	}
/****************************************************************
 *	Name:	Psi240i_Command
 *
 *	Description:	Process a command from the SCSI manager.
 *
 *	Parameters:		SCpnt - Pointer to SCSI command structure.
 *
 *	Returns:		Status code.
 *
 ****************************************************************/
int Psi240i_Command (Scsi_Cmnd *SCpnt)
	{
	DEB(printk("psi240i_command: ..calling psi240i_queuecommand\n"));

	Psi240i_QueueCommand (SCpnt, internal_done);

    SCpnt->SCp.Status = 0;
	while (!SCpnt->SCp.Status)
		barrier ();
	return SCpnt->result;
	}
/***************************************************************************
 *	Name:			ReadChipMemory
 *
 *	Description:	Read information from controller memory.
 *
 *	Parameters:		psetup	- Pointer to memory image of setup information.
 *					base	- base address of memory.
 *					length	- lenght of data space in bytes.
 *					port	- I/O address of data port.
 *
 *	Returns:		Nothing.
 *
 **************************************************************************/
void ReadChipMemory (void *pdata, USHORT base, USHORT length, USHORT port)
	{
	USHORT	z, zz;
	UCHAR	*pd = (UCHAR *)pdata;
	outb_p (SEL_NONE, port + REG_SEL_FAIL);				// setup data port
	zz = 0;
	while ( zz < length )
		{
		outw_p (base, port + REG_ADDRESS);				// setup address

		for ( z = 0;  z < 8;  z++ )
			{
			if ( (zz + z) < length )
			*pd++ = inb_p (port + z);	// read data byte
			}
		zz += 8;
		base += 8;
		}
	}
/****************************************************************
 *	Name:	Psi240i_Detect
 *
 *	Description:	Detect and initialize our boards.
 *
 *	Parameters:		tpnt - Pointer to SCSI host template structure.
 *
 *	Returns:		Number of adapters found.
 *
 ****************************************************************/
int Psi240i_Detect (Scsi_Host_Template *tpnt)
	{
	int					board;
	int					count = 0;
	int					unit;
	int					z;
	USHORT				port;
	CHIP_CONFIG_N		chipConfig;
	CHIP_DEVICE_N		chipDevice[8];
	struct Scsi_Host   *pshost;
	ULONG				flags;

	for ( board = 0;  board < 6;  board++ )					// scan for I/O ports
		{
		port = portAddr[board];								// get base address to test
		if ( check_region (port, 16) )						// test for I/O addresses available
			continue;										//   nope
		if ( inb_p (port + REG_FAIL) != CHIP_ID )			// do the first test for likley hood that it is us
			continue;
		outb_p (SEL_NONE, port + REG_SEL_FAIL);				// setup EEPROM/RAM access
		outw (0, port + REG_ADDRESS);						// setup EEPROM address zero
		if ( inb_p (port) != 0x55 )							// test 1st byte
			continue;										//   nope
		if ( inb_p (port + 1) != 0xAA )						// test 2nd byte
			continue;										//   nope

		// at this point our board is found and can be accessed.  Now we need to initialize
		// our informatation and register with the kernel.


		ReadChipMemory (&chipConfig, CHIP_CONFIG, sizeof (chipConfig), port);
		ReadChipMemory (&chipDevice, CHIP_DEVICE, sizeof (chipDevice), port);
		ReadChipMemory (&ChipSetup, CHIP_EEPROM_DATA, sizeof (ChipSetup), port);

		if ( !chipConfig.numDrives )						// if no devices on this board
			continue;

		pshost = scsi_register (tpnt, sizeof(ADAPTER240I));
		if(pshost == NULL)
			continue;

		save_flags (flags);
		cli ();
		if ( request_irq (chipConfig.irq, do_Irq_Handler, 0, "psi240i", NULL) )
			{
			printk ("Unable to allocate IRQ for PSI-240I controller.\n");
			restore_flags (flags);
			goto unregister;
			}

		PsiHost[chipConfig.irq - 10] = pshost;
		pshost->unique_id = port;
		pshost->io_port = port;
		pshost->n_io_port = 16;  /* Number of bytes of I/O space used */
		pshost->irq = chipConfig.irq;

		for ( z = 0;  z < 11;  z++ )						// build regester address array
			HOSTDATA(pshost)->ports[z] = port + z;
		HOSTDATA(pshost)->ports[11] = port + REG_FAIL;
		HOSTDATA(pshost)->ports[12] = port + REG_ALT_STAT;
		DEB (printk ("\nPorts ="));
		DEB (for (z=0;z<13;z++) printk(" %#04X",HOSTDATA(pshost)->ports[z]););

		for ( z = 0;  z < chipConfig.numDrives;  ++z )
			{
			unit = chipDevice[z].channel & 0x0F;
			HOSTDATA(pshost)->device[unit].device	 = ChipSetup.setupDevice[unit].device;
			HOSTDATA(pshost)->device[unit].byte6	 = (UCHAR)(((unit & 1) << 4) | 0xE0);
			HOSTDATA(pshost)->device[unit].spigot	 = (UCHAR)(1 << (unit >> 1));
			HOSTDATA(pshost)->device[unit].sectors	 = ChipSetup.setupDevice[unit].sectors;
			HOSTDATA(pshost)->device[unit].heads	 = ChipSetup.setupDevice[unit].heads;
			HOSTDATA(pshost)->device[unit].cylinders = ChipSetup.setupDevice[unit].cylinders;
			HOSTDATA(pshost)->device[unit].blocks	 = ChipSetup.setupDevice[unit].blocks;
			DEB (printk ("\nHOSTDATA->device    = %X", HOSTDATA(pshost)->device[unit].device));
			DEB (printk ("\n          byte6     = %X", HOSTDATA(pshost)->device[unit].byte6));
			DEB (printk ("\n          spigot    = %X", HOSTDATA(pshost)->device[unit].spigot));
			DEB (printk ("\n          sectors   = %X", HOSTDATA(pshost)->device[unit].sectors));
			DEB (printk ("\n          heads     = %X", HOSTDATA(pshost)->device[unit].heads));
			DEB (printk ("\n          cylinders = %X", HOSTDATA(pshost)->device[unit].cylinders));
			DEB (printk ("\n          blocks    = %lX", HOSTDATA(pshost)->device[unit].blocks));
			}

		restore_flags (flags);
		printk("\nPSI-240I EIDE CONTROLLER: at I/O = %x  IRQ = %d\n", port, chipConfig.irq);
		printk("(C) 1997 Perceptive Solutions, Inc. All rights reserved\n\n");
		count++;
		continue;

unregister:;
		scsi_unregister (pshost);
		}
	return count;
	}
/****************************************************************
 *	Name:	Psi240i_Abort
 *
 *	Description:	Process the Abort command from the SCSI manager.
 *
 *	Parameters:		SCpnt - Pointer to SCSI command structure.
 *
 *	Returns:		Allways snooze.
 *
 ****************************************************************/
int Psi240i_Abort (Scsi_Cmnd *SCpnt)
	{
	DEB (printk ("psi240i_abort\n"));
	return SCSI_ABORT_SNOOZE;
	}
/****************************************************************
 *	Name:	Psi240i_Reset
 *
 *	Description:	Process the Reset command from the SCSI manager.
 *
 *	Parameters:		SCpnt - Pointer to SCSI command structure.
 *					flags - Flags about the reset command
 *
 *	Returns:		No active command at this time, so this means
 *					that each time we got some kind of response the
 *					last time through.  Tell the mid-level code to
 *					request sense information in order to decide what
 *					to do next.
 *
 ****************************************************************/
int Psi240i_Reset (Scsi_Cmnd *SCpnt, unsigned int reset_flags)
	{
	return SCSI_RESET_PUNT;
	}

#include "sd.h"

/****************************************************************
 *	Name:	Psi240i_BiosParam
 *
 *	Description:	Process the biosparam request from the SCSI manager to
 *					return C/H/S data.
 *
 *	Parameters:		disk - Pointer to SCSI disk structure.
 *					dev	 - Major/minor number from kernel.
 *					geom - Pointer to integer array to place geometry data.
 *
 *	Returns:		zero.
 *
 ****************************************************************/
int Psi240i_BiosParam (Scsi_Disk *disk, kdev_t dev, int geom[])
	{
	POUR_DEVICE	pdev;

	pdev = &(HOSTDATA(disk->device->host)->device[disk->device->id]);

	geom[0] = pdev->heads;
	geom[1] = pdev->sectors;
	geom[2] = pdev->cylinders;
	return 0;
	}

MODULE_LICENSE("GPL");

/* Eventually this will go into an include file, but this will be later */
static Scsi_Host_Template driver_template = PSI240I;

#include "scsi_module.c"

