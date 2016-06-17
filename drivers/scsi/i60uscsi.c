/**************************************************************************
 * Initio A100 device driver for Linux.
 *
 * Copyright (c) 1994-1998 Initio Corporation
 * All rights reserved.
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
 * --------------------------------------------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Where this Software is combined with software released under the terms of 
 * the GNU General Public License ("GPL") and the terms of the GPL would require the 
 * combined work to also be released under the terms of the GPL, the terms
 * and conditions of this License will apply in addition to those of the
 * GPL with the exception of any terms or conditions of this License that
 * conflict with, or are expressly prohibited by, the GPL.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *************************************************************************
 *
 * module: i60uscsi.c 
 * DESCRIPTION:
 * 	This is the Linux low-level SCSI driver for Initio INIA100 SCSI host
 * adapters
 *
 * 07/02/98 hl	- v.91n Initial drivers.
 * 09/14/98 hl - v1.01 Support new Kernel.
 * 09/22/98 hl - v1.01a Support reset.
 * 09/24/98 hl - v1.01b Fixed reset.
 * 10/05/98 hl - v1.02 split the source code and release.
 * 12/19/98 bv - v1.02a Use spinlocks for 2.1.95 and up
 * 01/31/99 bv - v1.02b Use mdelay instead of waitForPause
 * 08/08/99 bv - v1.02c Use waitForPause again.
 **************************************************************************/

#include <linux/version.h>
#include <linux/sched.h>
#include <asm/io.h>
#include "i60uscsi.h"

#define JIFFIES_TO_MS(t) ((t) * 1000 / HZ)
#define MS_TO_JIFFIES(j) ((j * HZ) / 1000)

/* ---- INTERNAL FUNCTIONS ---- */
static UCHAR waitChipReady(ORC_HCS * hcsp);
static UCHAR waitFWReady(ORC_HCS * hcsp);
static UCHAR waitFWReady(ORC_HCS * hcsp);
static UCHAR waitSCSIRSTdone(ORC_HCS * hcsp);
static UCHAR waitHDOoff(ORC_HCS * hcsp);
static UCHAR waitHDIset(ORC_HCS * hcsp, UCHAR * pData);
static unsigned short get_FW_version(ORC_HCS * hcsp);
static UCHAR set_NVRAM(ORC_HCS * hcsp, unsigned char address, unsigned char value);
static UCHAR get_NVRAM(ORC_HCS * hcsp, unsigned char address, unsigned char *pDataIn);
static int se2_rd_all(ORC_HCS * hcsp);
static void se2_update_all(ORC_HCS * hcsp);	/* setup default pattern        */
static void read_eeprom(ORC_HCS * hcsp);
static UCHAR load_FW(ORC_HCS * hcsp);
static void setup_SCBs(ORC_HCS * hcsp);
static void initAFlag(ORC_HCS * hcsp);
ORC_SCB *orc_alloc_scb(ORC_HCS * hcsp);

/* ---- EXTERNAL FUNCTIONS ---- */
extern void inia100SCBPost(BYTE * pHcb, BYTE * pScb);

/* ---- INTERNAL VARIABLES ---- */
ORC_HCS orc_hcs[MAX_SUPPORTED_ADAPTERS];
static INIA100_ADPT_STRUCT inia100_adpt[MAX_SUPPORTED_ADAPTERS];
/* set by inia100_setup according to the command line */
int orc_num_scb;

NVRAM nvram, *nvramp = &nvram;
static UCHAR dftNvRam[64] =
{
/*----------header -------------*/
	0x01,			/* 0x00: Sub System Vendor ID 0 */
	0x11,			/* 0x01: Sub System Vendor ID 1 */
	0x60,			/* 0x02: Sub System ID 0        */
	0x10,			/* 0x03: Sub System ID 1        */
	0x00,			/* 0x04: SubClass               */
	0x01,			/* 0x05: Vendor ID 0            */
	0x11,			/* 0x06: Vendor ID 1            */
	0x60,			/* 0x07: Device ID 0            */
	0x10,			/* 0x08: Device ID 1            */
	0x00,			/* 0x09: Reserved               */
	0x00,			/* 0x0A: Reserved               */
	0x01,			/* 0x0B: Revision of Data Structure     */
				/* -- Host Adapter Structure --- */
	0x01,			/* 0x0C: Number Of SCSI Channel */
	0x01,			/* 0x0D: BIOS Configuration 1   */
	0x00,			/* 0x0E: BIOS Configuration 2   */
	0x00,			/* 0x0F: BIOS Configuration 3   */
				/* --- SCSI Channel 0 Configuration --- */
	0x07,			/* 0x10: H/A ID                 */
	0x83,			/* 0x11: Channel Configuration  */
	0x20,			/* 0x12: MAX TAG per target     */
	0x0A,			/* 0x13: SCSI Reset Recovering time     */
	0x00,			/* 0x14: Channel Configuration4 */
	0x00,			/* 0x15: Channel Configuration5 */
				/* SCSI Channel 0 Target Configuration  */
				/* 0x16-0x25                    */
	0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8,
	0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8,
				/* --- SCSI Channel 1 Configuration --- */
	0x07,			/* 0x26: H/A ID                 */
	0x83,			/* 0x27: Channel Configuration  */
	0x20,			/* 0x28: MAX TAG per target     */
	0x0A,			/* 0x29: SCSI Reset Recovering time     */
	0x00,			/* 0x2A: Channel Configuration4 */
	0x00,			/* 0x2B: Channel Configuration5 */
				/* SCSI Channel 1 Target Configuration  */
				/* 0x2C-0x3B                    */
	0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8,
	0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8,
	0x00,			/* 0x3C: Reserved               */
	0x00,			/* 0x3D: Reserved               */
	0x00,			/* 0x3E: Reserved               */
	0x00			/* 0x3F: Checksum               */
};


/***************************************************************************/
static void waitForPause(unsigned amount)
{
	ULONG the_time = jiffies + MS_TO_JIFFIES(amount);
	while (time_before_eq(jiffies, the_time))
		cpu_relax();
}

/***************************************************************************/
UCHAR waitChipReady(ORC_HCS * hcsp)
{
	int i;

	for (i = 0; i < 10; i++) {	/* Wait 1 second for report timeout     */
		if (ORC_RD(hcsp->HCS_Base, ORC_HCTRL) & HOSTSTOP)	/* Wait HOSTSTOP set */
			return (TRUE);
		waitForPause(100);	/* wait 100ms before try again  */
	}
	return (FALSE);
}

/***************************************************************************/
UCHAR waitFWReady(ORC_HCS * hcsp)
{
	int i;

	for (i = 0; i < 10; i++) {	/* Wait 1 second for report timeout     */
		if (ORC_RD(hcsp->HCS_Base, ORC_HSTUS) & RREADY)		/* Wait READY set */
			return (TRUE);
		waitForPause(100);	/* wait 100ms before try again  */
	}
	return (FALSE);
}

/***************************************************************************/
UCHAR waitSCSIRSTdone(ORC_HCS * hcsp)
{
	int i;

	for (i = 0; i < 10; i++) {	/* Wait 1 second for report timeout     */
		if (!(ORC_RD(hcsp->HCS_Base, ORC_HCTRL) & SCSIRST))	/* Wait SCSIRST done */
			return (TRUE);
		waitForPause(100);	/* wait 100ms before try again  */
	}
	return (FALSE);
}

/***************************************************************************/
UCHAR waitHDOoff(ORC_HCS * hcsp)
{
	int i;

	for (i = 0; i < 10; i++) {	/* Wait 1 second for report timeout     */
		if (!(ORC_RD(hcsp->HCS_Base, ORC_HCTRL) & HDO))		/* Wait HDO off */
			return (TRUE);
		waitForPause(100);	/* wait 100ms before try again  */
	}
	return (FALSE);
}

/***************************************************************************/
UCHAR waitHDIset(ORC_HCS * hcsp, UCHAR * pData)
{
	int i;

	for (i = 0; i < 10; i++) {	/* Wait 1 second for report timeout     */
		if ((*pData = ORC_RD(hcsp->HCS_Base, ORC_HSTUS)) & HDI)
			return (TRUE);	/* Wait HDI set */
		waitForPause(100);	/* wait 100ms before try again  */
	}
	return (FALSE);
}

/***************************************************************************/
unsigned short get_FW_version(ORC_HCS * hcsp)
{
	UCHAR bData;
	union {
		unsigned short sVersion;
		unsigned char cVersion[2];
	} Version;

	ORC_WR(hcsp->HCS_Base + ORC_HDATA, ORC_CMD_VERSION);
	ORC_WR(hcsp->HCS_Base + ORC_HCTRL, HDO);
	if (waitHDOoff(hcsp) == FALSE)	/* Wait HDO off   */
		return (FALSE);

	if (waitHDIset(hcsp, &bData) == FALSE)	/* Wait HDI set   */
		return (FALSE);
	Version.cVersion[0] = ORC_RD(hcsp->HCS_Base, ORC_HDATA);
	ORC_WR(hcsp->HCS_Base + ORC_HSTUS, bData);	/* Clear HDI            */

	if (waitHDIset(hcsp, &bData) == FALSE)	/* Wait HDI set   */
		return (FALSE);
	Version.cVersion[1] = ORC_RD(hcsp->HCS_Base, ORC_HDATA);
	ORC_WR(hcsp->HCS_Base + ORC_HSTUS, bData);	/* Clear HDI            */

	return (Version.sVersion);
}

/***************************************************************************/
UCHAR set_NVRAM(ORC_HCS * hcsp, unsigned char address, unsigned char value)
{
	ORC_WR(hcsp->HCS_Base + ORC_HDATA, ORC_CMD_SET_NVM);	/* Write command */
	ORC_WR(hcsp->HCS_Base + ORC_HCTRL, HDO);
	if (waitHDOoff(hcsp) == FALSE)	/* Wait HDO off   */
		return (FALSE);

	ORC_WR(hcsp->HCS_Base + ORC_HDATA, address);	/* Write address */
	ORC_WR(hcsp->HCS_Base + ORC_HCTRL, HDO);
	if (waitHDOoff(hcsp) == FALSE)	/* Wait HDO off   */
		return (FALSE);

	ORC_WR(hcsp->HCS_Base + ORC_HDATA, value);	/* Write value  */
	ORC_WR(hcsp->HCS_Base + ORC_HCTRL, HDO);
	if (waitHDOoff(hcsp) == FALSE)	/* Wait HDO off   */
		return (FALSE);

	return (TRUE);
}

/***************************************************************************/
UCHAR get_NVRAM(ORC_HCS * hcsp, unsigned char address, unsigned char *pDataIn)
{
	unsigned char bData;

	ORC_WR(hcsp->HCS_Base + ORC_HDATA, ORC_CMD_GET_NVM);	/* Write command */
	ORC_WR(hcsp->HCS_Base + ORC_HCTRL, HDO);
	if (waitHDOoff(hcsp) == FALSE)	/* Wait HDO off   */
		return (FALSE);

	ORC_WR(hcsp->HCS_Base + ORC_HDATA, address);	/* Write address */
	ORC_WR(hcsp->HCS_Base + ORC_HCTRL, HDO);
	if (waitHDOoff(hcsp) == FALSE)	/* Wait HDO off   */
		return (FALSE);

	if (waitHDIset(hcsp, &bData) == FALSE)	/* Wait HDI set   */
		return (FALSE);
	*pDataIn = ORC_RD(hcsp->HCS_Base, ORC_HDATA);
	ORC_WR(hcsp->HCS_Base + ORC_HSTUS, bData);	/* Clear HDI    */

	return (TRUE);
}

/***************************************************************************/
void orc_exec_scb(ORC_HCS * hcsp, ORC_SCB * scbp)
{
	scbp->SCB_Status = SCB_POST;
	ORC_WR(hcsp->HCS_Base + ORC_PQUEUE, scbp->SCB_ScbIdx);
	return;
}


/***********************************************************************
 Read SCSI H/A configuration parameters from serial EEPROM
************************************************************************/
int se2_rd_all(ORC_HCS * hcsp)
{
	int i;
	UCHAR *np, chksum = 0;

	np = (UCHAR *) nvramp;
	for (i = 0; i < 64; i++, np++) {	/* <01> */
		if (get_NVRAM(hcsp, (unsigned char) i, np) == FALSE)
			return -1;
//      *np++ = get_NVRAM(hcsp, (unsigned char ) i);
	}

/*------ Is ckecksum ok ? ------*/
	np = (UCHAR *) nvramp;
	for (i = 0; i < 63; i++)
		chksum += *np++;

	if (nvramp->CheckSum != (UCHAR) chksum)
		return -1;
	return 1;
}

/************************************************************************
 Update SCSI H/A configuration parameters from serial EEPROM
*************************************************************************/
void se2_update_all(ORC_HCS * hcsp)
{				/* setup default pattern  */
	int i;
	UCHAR *np, *np1, chksum = 0;

	/* Calculate checksum first   */
	np = (UCHAR *) dftNvRam;
	for (i = 0; i < 63; i++)
		chksum += *np++;
	*np = chksum;

	np = (UCHAR *) dftNvRam;
	np1 = (UCHAR *) nvramp;
	for (i = 0; i < 64; i++, np++, np1++) {
		if (*np != *np1) {
			set_NVRAM(hcsp, (unsigned char) i, *np);
		}
	}
	return;
}

/*************************************************************************
 Function name  : read_eeprom
**************************************************************************/
void read_eeprom(ORC_HCS * hcsp)
{
	if (se2_rd_all(hcsp) != 1) {
		se2_update_all(hcsp);	/* setup default pattern        */
		se2_rd_all(hcsp);	/* load again                   */
	}
}


/***************************************************************************/
UCHAR load_FW(ORC_HCS * hcsp)
{
	U32 dData;
	USHORT wBIOSAddress;
	USHORT i;
	UCHAR *pData, bData;


	bData = ORC_RD(hcsp->HCS_Base, ORC_GCFG);
	ORC_WR(hcsp->HCS_Base + ORC_GCFG, bData | EEPRG);	/* Enable EEPROM programming */
	ORC_WR(hcsp->HCS_Base + ORC_EBIOSADR2, 0x00);
	ORC_WRSHORT(hcsp->HCS_Base + ORC_EBIOSADR0, 0x00);
	if (ORC_RD(hcsp->HCS_Base, ORC_EBIOSDATA) != 0x55) {
		ORC_WR(hcsp->HCS_Base + ORC_GCFG, bData);	/* Disable EEPROM programming */
		return (FALSE);
	}
	ORC_WRSHORT(hcsp->HCS_Base + ORC_EBIOSADR0, 0x01);
	if (ORC_RD(hcsp->HCS_Base, ORC_EBIOSDATA) != 0xAA) {
		ORC_WR(hcsp->HCS_Base + ORC_GCFG, bData);	/* Disable EEPROM programming */
		return (FALSE);
	}
	ORC_WR(hcsp->HCS_Base + ORC_RISCCTL, PRGMRST | DOWNLOAD);	/* Enable SRAM programming */
	pData = (UCHAR *) & dData;
	dData = 0;		/* Initial FW address to 0 */
	ORC_WRSHORT(hcsp->HCS_Base + ORC_EBIOSADR0, 0x10);
	*pData = ORC_RD(hcsp->HCS_Base, ORC_EBIOSDATA);		/* Read from BIOS */
	ORC_WRSHORT(hcsp->HCS_Base + ORC_EBIOSADR0, 0x11);
	*(pData + 1) = ORC_RD(hcsp->HCS_Base, ORC_EBIOSDATA);	/* Read from BIOS */
	ORC_WRSHORT(hcsp->HCS_Base + ORC_EBIOSADR0, 0x12);
	*(pData + 2) = ORC_RD(hcsp->HCS_Base, ORC_EBIOSDATA);	/* Read from BIOS */
	ORC_WR(hcsp->HCS_Base + ORC_EBIOSADR2, *(pData + 2));
	ORC_WRLONG(hcsp->HCS_Base + ORC_FWBASEADR, dData);	/* Write FW address */

	wBIOSAddress = (USHORT) dData;	/* FW code locate at BIOS address + ? */
	for (i = 0, pData = (UCHAR *) & dData;	/* Download the code    */
	     i < 0x1000;	/* Firmware code size = 4K      */
	     i++, wBIOSAddress++) {
		ORC_WRSHORT(hcsp->HCS_Base + ORC_EBIOSADR0, wBIOSAddress);
		*pData++ = ORC_RD(hcsp->HCS_Base, ORC_EBIOSDATA);	/* Read from BIOS */
		if ((i % 4) == 3) {
			ORC_WRLONG(hcsp->HCS_Base + ORC_RISCRAM, dData);	/* Write every 4 bytes */
			pData = (UCHAR *) & dData;
		}
	}

	ORC_WR(hcsp->HCS_Base + ORC_RISCCTL, PRGMRST | DOWNLOAD);	/* Reset program count 0 */
	wBIOSAddress -= 0x1000;	/* Reset the BIOS adddress      */
	for (i = 0, pData = (UCHAR *) & dData;	/* Check the code       */
	     i < 0x1000;	/* Firmware code size = 4K      */
	     i++, wBIOSAddress++) {
		ORC_WRSHORT(hcsp->HCS_Base + ORC_EBIOSADR0, wBIOSAddress);
		*pData++ = ORC_RD(hcsp->HCS_Base, ORC_EBIOSDATA);	/* Read from BIOS */
		if ((i % 4) == 3) {
			if (ORC_RDLONG(hcsp->HCS_Base, ORC_RISCRAM) != dData) {
				ORC_WR(hcsp->HCS_Base + ORC_RISCCTL, PRGMRST);	/* Reset program to 0 */
				ORC_WR(hcsp->HCS_Base + ORC_GCFG, bData);	/*Disable EEPROM programming */
				return (FALSE);
			}
			pData = (UCHAR *) & dData;
		}
	}
	ORC_WR(hcsp->HCS_Base + ORC_RISCCTL, PRGMRST);	/* Reset program to 0   */
	ORC_WR(hcsp->HCS_Base + ORC_GCFG, bData);	/* Disable EEPROM programming */
	return (TRUE);
}

/***************************************************************************/
void setup_SCBs(ORC_HCS * hcsp)
{
	ORC_SCB *pVirScb;
	int i;
	UCHAR j;
	ESCB *pVirEscb;
	PVOID pPhysEscb;
	PVOID tPhysEscb;

	j = 0;
	pVirScb = NULL;
	tPhysEscb = (PVOID) NULL;
	pPhysEscb = (PVOID) NULL;
	/* Setup SCB HCS_Base and SCB Size registers */
	ORC_WR(hcsp->HCS_Base + ORC_SCBSIZE, orc_num_scb);	/* Total number of SCBs */
	/* SCB HCS_Base address 0      */
	ORC_WRLONG(hcsp->HCS_Base + ORC_SCBBASE0, hcsp->HCS_physScbArray);
	/* SCB HCS_Base address 1      */
	ORC_WRLONG(hcsp->HCS_Base + ORC_SCBBASE1, hcsp->HCS_physScbArray);

	/* setup scatter list address with one buffer */
	pVirScb = (ORC_SCB *) hcsp->HCS_virScbArray;
	pVirEscb = (ESCB *) hcsp->HCS_virEscbArray;

	for (i = 0; i < orc_num_scb; i++) {
		pPhysEscb = (PVOID) (hcsp->HCS_physEscbArray + (sizeof(ESCB) * i));
		pVirScb->SCB_SGPAddr = (U32) pPhysEscb;
		pVirScb->SCB_SensePAddr = (U32) pPhysEscb;
		pVirScb->SCB_EScb = pVirEscb;
		pVirScb->SCB_ScbIdx = i;
		pVirScb++;
		pVirEscb++;
	}

	return;
}

/***************************************************************************/
static void initAFlag(ORC_HCS * hcsp)
{
	UCHAR i, j;

	for (i = 0; i < MAX_CHANNELS; i++) {
		for (j = 0; j < 8; j++) {
			hcsp->BitAllocFlag[i][j] = 0xffffffff;
		}
	}
}

/***************************************************************************/
int init_orchid(ORC_HCS * hcsp)
{
	UBYTE *readBytep;
	USHORT revision;
	UCHAR i;

	initAFlag(hcsp);
	ORC_WR(hcsp->HCS_Base + ORC_GIMSK, 0xFF);	/* Disable all interrupt        */
	if (ORC_RD(hcsp->HCS_Base, ORC_HSTUS) & RREADY) {	/* Orchid is ready              */
		revision = get_FW_version(hcsp);
		if (revision == 0xFFFF) {
			ORC_WR(hcsp->HCS_Base + ORC_HCTRL, DEVRST);	/* Reset Host Adapter   */
			if (waitChipReady(hcsp) == FALSE)
				return (-1);
			load_FW(hcsp);	/* Download FW                  */
			setup_SCBs(hcsp);	/* Setup SCB HCS_Base and SCB Size registers */
			ORC_WR(hcsp->HCS_Base + ORC_HCTRL, 0);	/* clear HOSTSTOP       */
			if (waitFWReady(hcsp) == FALSE)
				return (-1);
			/* Wait for firmware ready     */
		} else {
			setup_SCBs(hcsp);	/* Setup SCB HCS_Base and SCB Size registers */
		}
	} else {		/* Orchid is not Ready          */
		ORC_WR(hcsp->HCS_Base + ORC_HCTRL, DEVRST);	/* Reset Host Adapter   */
		if (waitChipReady(hcsp) == FALSE)
			return (-1);
		load_FW(hcsp);	/* Download FW                  */
		setup_SCBs(hcsp);	/* Setup SCB HCS_Base and SCB Size registers */
		ORC_WR(hcsp->HCS_Base + ORC_HCTRL, HDO);	/* Do Hardware Reset &  */

		/*     clear HOSTSTOP  */
		if (waitFWReady(hcsp) == FALSE)		/* Wait for firmware ready      */
			return (-1);
	}

/*------------- get serial EEProm settting -------*/

	read_eeprom(hcsp);

	if (nvramp->Revision != 1)
		return (-1);

	hcsp->HCS_SCSI_ID = nvramp->SCSI0Id;
	hcsp->HCS_BIOS = nvramp->BIOSConfig1;
	hcsp->HCS_MaxTar = MAX_TARGETS;
	readBytep = (UCHAR *) & (nvramp->Target00Config);
	for (i = 0; i < 16; readBytep++, i++) {
		hcsp->TargetFlag[i] = *readBytep;
		hcsp->MaximumTags[i] = orc_num_scb;
	}			/* for                          */

	if (nvramp->SCSI0Config & NCC_BUSRESET) {	/* Reset SCSI bus               */
		hcsp->HCS_Flags |= HCF_SCSI_RESET;
	}
	ORC_WR(hcsp->HCS_Base + ORC_GIMSK, 0xFB);	/* enable RP FIFO interrupt     */
	return (0);
}

/*****************************************************************************
 Function name  : orc_reset_scsi_bus
 Description    : Reset registers, reset a hanging bus and
                  kill active and disconnected commands for target w/o soft reset
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
int orc_reset_scsi_bus(ORC_HCS * pHCB)
{				/* I need Host Control Block Information */
	ULONG flags;

	spin_lock_irqsave(&(pHCB->BitAllocFlagLock), flags);

	initAFlag(pHCB);
	/* reset scsi bus */
	ORC_WR(pHCB->HCS_Base + ORC_HCTRL, SCSIRST);
	if (waitSCSIRSTdone(pHCB) == FALSE) {
		spin_unlock_irqrestore(&(pHCB->BitAllocFlagLock), flags);
		return (SCSI_RESET_ERROR);
	} else {
		spin_unlock_irqrestore(&(pHCB->BitAllocFlagLock), flags);
		return (SCSI_RESET_SUCCESS);
	}
}

/*****************************************************************************
 Function name  : orc_device_reset
 Description    : Reset registers, reset a hanging bus and
                  kill active and disconnected commands for target w/o soft reset
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
int orc_device_reset(ORC_HCS * pHCB, ULONG SCpnt, unsigned int target, unsigned int ResetFlags)
{				/* I need Host Control Block Information */
	ORC_SCB *pScb;
	ESCB *pVirEscb;
	ORC_SCB *pVirScb;
	UCHAR i;
	ULONG flags;

	spin_lock_irqsave(&(pHCB->BitAllocFlagLock), flags);
	pScb = (ORC_SCB *) NULL;
	pVirEscb = (ESCB *) NULL;

	/* setup scatter list address with one buffer */
	pVirScb = (ORC_SCB *) pHCB->HCS_virScbArray;

	initAFlag(pHCB);
	/* device reset */
	for (i = 0; i < orc_num_scb; i++) {
		pVirEscb = pVirScb->SCB_EScb;
		if ((pVirScb->SCB_Status) && (pVirEscb->SCB_Srb == (unsigned char *) SCpnt))
			break;
		pVirScb++;
	}

	if (i == orc_num_scb) {
		printk("Unable to Reset - No SCB Found\n");
		spin_unlock_irqrestore(&(pHCB->BitAllocFlagLock), flags);
		return (SCSI_RESET_NOT_RUNNING);
	}
	if ((pScb = orc_alloc_scb(pHCB)) == NULL) {
		spin_unlock_irqrestore(&(pHCB->BitAllocFlagLock), flags);
		return (SCSI_RESET_NOT_RUNNING);
	}
	pScb->SCB_Opcode = ORC_BUSDEVRST;
	pScb->SCB_Target = target;
	pScb->SCB_HaStat = 0;
	pScb->SCB_TaStat = 0;
	pScb->SCB_Status = 0x0;
	pScb->SCB_Link = 0xFF;
	pScb->SCB_Reserved0 = 0;
	pScb->SCB_Reserved1 = 0;
	pScb->SCB_XferLen = 0;
	pScb->SCB_SGLen = 0;

	pVirEscb->SCB_Srb = 0;
	if (ResetFlags & SCSI_RESET_SYNCHRONOUS) {
		pVirEscb->SCB_Srb = (unsigned char *) SCpnt;
	}
	orc_exec_scb(pHCB, pScb);	/* Start execute SCB            */
	spin_unlock_irqrestore(&(pHCB->BitAllocFlagLock), flags);
	return SCSI_RESET_PENDING;
}


/***************************************************************************/
ORC_SCB *__orc_alloc_scb(ORC_HCS * hcsp)
{
	ORC_SCB *pTmpScb;
	UCHAR Ch;
	ULONG idx;
	UCHAR index;
	UCHAR i;

	Ch = hcsp->HCS_Index;
	for (i = 0; i < 8; i++) {
		for (index = 0; index < 32; index++) {
			if ((hcsp->BitAllocFlag[Ch][i] >> index) & 0x01) {
				hcsp->BitAllocFlag[Ch][i] &= ~(1 << index);
				break;
			}
		}
		idx = index + 32 * i;
		pTmpScb = (PVOID) ((ULONG) hcsp->HCS_virScbArray + (idx * sizeof(ORC_SCB)));
		return (pTmpScb);
	}
	return (NULL);
}

ORC_SCB *orc_alloc_scb(ORC_HCS * hcsp)
{
	ORC_SCB *pTmpScb;
	ULONG flags;

	spin_lock_irqsave(&(hcsp->BitAllocFlagLock), flags);
	pTmpScb = __orc_alloc_scb(hcsp);
	spin_unlock_irqrestore(&(hcsp->BitAllocFlagLock), flags);
	return (pTmpScb);
}


/***************************************************************************/
void orc_release_scb(ORC_HCS * hcsp, ORC_SCB * scbp)
{
	ULONG flags;
	UCHAR Index;
	UCHAR i;
	UCHAR Ch;

	spin_lock_irqsave(&(hcsp->BitAllocFlagLock), flags);
	Ch = hcsp->HCS_Index;
	Index = scbp->SCB_ScbIdx;
	i = Index / 32;
	Index %= 32;
	hcsp->BitAllocFlag[Ch][i] |= (1 << Index);
	spin_unlock_irqrestore(&(hcsp->BitAllocFlagLock), flags);
}


/*****************************************************************************
 Function name	: Addinia100_into_Adapter_table
 Description	: This function will scan PCI bus to get all Orchid card
 Input		: None.
 Output		: None.
 Return		: SUCCESSFUL	- Successful scan
 ohterwise	- No drives founded
*****************************************************************************/
int Addinia100_into_Adapter_table(WORD wBIOS, WORD wBASE, BYTE bInterrupt,
				  BYTE bBus, BYTE bDevice)
{
	unsigned int i, j;

	for (i = 0; i < MAX_SUPPORTED_ADAPTERS; i++) {
		if (inia100_adpt[i].ADPT_BIOS < wBIOS)
			continue;
		if (inia100_adpt[i].ADPT_BIOS == wBIOS) {
			if (inia100_adpt[i].ADPT_BASE == wBASE) {
				if (inia100_adpt[i].ADPT_Bus != 0xFF)
					return (FAILURE);
			} else if (inia100_adpt[i].ADPT_BASE < wBASE)
				continue;
		}
		for (j = MAX_SUPPORTED_ADAPTERS - 1; j > i; j--) {
			inia100_adpt[j].ADPT_BASE = inia100_adpt[j - 1].ADPT_BASE;
			inia100_adpt[j].ADPT_INTR = inia100_adpt[j - 1].ADPT_INTR;
			inia100_adpt[j].ADPT_BIOS = inia100_adpt[j - 1].ADPT_BIOS;
			inia100_adpt[j].ADPT_Bus = inia100_adpt[j - 1].ADPT_Bus;
			inia100_adpt[j].ADPT_Device = inia100_adpt[j - 1].ADPT_Device;
		}
		inia100_adpt[i].ADPT_BASE = wBASE;
		inia100_adpt[i].ADPT_INTR = bInterrupt;
		inia100_adpt[i].ADPT_BIOS = wBIOS;
		inia100_adpt[i].ADPT_Bus = bBus;
		inia100_adpt[i].ADPT_Device = bDevice;
		return (SUCCESSFUL);
	}
	return (FAILURE);
}


/*****************************************************************************
 Function name	: init_inia100Adapter_table
 Description	: This function will scan PCI bus to get all Orchid card
 Input		: None.
 Output		: None.
 Return		: SUCCESSFUL	- Successful scan
 ohterwise	- No drives founded
*****************************************************************************/
void init_inia100Adapter_table(void)
{
	int i;

	for (i = 0; i < MAX_SUPPORTED_ADAPTERS; i++) {	/* Initialize adapter structure */
		inia100_adpt[i].ADPT_BIOS = 0xffff;
		inia100_adpt[i].ADPT_BASE = 0xffff;
		inia100_adpt[i].ADPT_INTR = 0xff;
		inia100_adpt[i].ADPT_Bus = 0xff;
		inia100_adpt[i].ADPT_Device = 0xff;
	}
}

/*****************************************************************************
 Function name  : get_orcPCIConfig
 Description    : 
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
void get_orcPCIConfig(ORC_HCS * pCurHcb, int ch_idx)
{
	pCurHcb->HCS_Base = inia100_adpt[ch_idx].ADPT_BASE;	/* Supply base address  */
	pCurHcb->HCS_BIOS = inia100_adpt[ch_idx].ADPT_BIOS;	/* Supply BIOS address  */
	pCurHcb->HCS_Intr = inia100_adpt[ch_idx].ADPT_INTR;	/* Supply interrupt line */
	return;
}


/*****************************************************************************
 Function name  : abort_SCB
 Description    : Abort a queued command.
	                 (commands that are on the bus can't be aborted easily)
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
int abort_SCB(ORC_HCS * hcsp, ORC_SCB * pScb)
{
	unsigned char bData, bStatus;

	ORC_WR(hcsp->HCS_Base + ORC_HDATA, ORC_CMD_ABORT_SCB);	/* Write command */
	ORC_WR(hcsp->HCS_Base + ORC_HCTRL, HDO);
	if (waitHDOoff(hcsp) == FALSE)	/* Wait HDO off   */
		return (FALSE);

	ORC_WR(hcsp->HCS_Base + ORC_HDATA, pScb->SCB_ScbIdx);	/* Write address */
	ORC_WR(hcsp->HCS_Base + ORC_HCTRL, HDO);
	if (waitHDOoff(hcsp) == FALSE)	/* Wait HDO off   */
		return (FALSE);

	if (waitHDIset(hcsp, &bData) == FALSE)	/* Wait HDI set   */
		return (FALSE);
	bStatus = ORC_RD(hcsp->HCS_Base, ORC_HDATA);
	ORC_WR(hcsp->HCS_Base + ORC_HSTUS, bData);	/* Clear HDI    */

	if (bStatus == 1)	/* 0 - Successfully               */
		return (FALSE);	/* 1 - Fail                     */
	return (TRUE);
}

/*****************************************************************************
 Function name  : inia100_abort
 Description    : Abort a queued command.
	                 (commands that are on the bus can't be aborted easily)
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
int orc_abort_srb(ORC_HCS * hcsp, ULONG SCpnt)
{
	ESCB *pVirEscb;
	ORC_SCB *pVirScb;
	UCHAR i;
	ULONG flags;

	spin_lock_irqsave(&(hcsp->BitAllocFlagLock), flags);

	pVirScb = (ORC_SCB *) hcsp->HCS_virScbArray;

	for (i = 0; i < orc_num_scb; i++, pVirScb++) {
		pVirEscb = pVirScb->SCB_EScb;
		if ((pVirScb->SCB_Status) && (pVirEscb->SCB_Srb == (unsigned char *) SCpnt)) {
			if (pVirScb->SCB_TagMsg == 0) {
				spin_unlock_irqrestore(&(hcsp->BitAllocFlagLock), flags);
				return (SCSI_ABORT_BUSY);
			} else {
				if (abort_SCB(hcsp, pVirScb)) {
					pVirEscb->SCB_Srb = NULL;
					spin_unlock_irqrestore(&(hcsp->BitAllocFlagLock), flags);
					return (SCSI_ABORT_SUCCESS);
				} else {
					spin_unlock_irqrestore(&(hcsp->BitAllocFlagLock), flags);
					return (SCSI_ABORT_NOT_RUNNING);
				}
			}
		}
	}
	spin_unlock_irqrestore(&(hcsp->BitAllocFlagLock), flags);
	return (SCSI_ABORT_NOT_RUNNING);
}

/***********************************************************************
 Routine Description:
	  This is the interrupt service routine for the Orchid SCSI adapter.
	  It reads the interrupt register to determine if the adapter is indeed
	  the source of the interrupt and clears the interrupt at the device.
 Arguments:
	  HwDeviceExtension - HBA miniport driver's adapter data storage
 Return Value:
***********************************************************************/
void orc_interrupt(
			  ORC_HCS * hcsp
)
{
	BYTE bScbIdx;
	ORC_SCB *pScb;

	if (ORC_RD(hcsp->HCS_Base, ORC_RQUEUECNT) == 0) {
		return;		// (FALSE);

	}
	do {
		bScbIdx = ORC_RD(hcsp->HCS_Base, ORC_RQUEUE);

		pScb = (ORC_SCB *) ((ULONG) hcsp->HCS_virScbArray + (ULONG) (sizeof(ORC_SCB) * bScbIdx));
		pScb->SCB_Status = 0x0;

		inia100SCBPost((BYTE *) hcsp, (BYTE *) pScb);
	} while (ORC_RD(hcsp->HCS_Base, ORC_RQUEUECNT));
	return;			//(TRUE);

}				/* End of I1060Interrupt() */
