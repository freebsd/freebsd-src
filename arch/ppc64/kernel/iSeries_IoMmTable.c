/************************************************************************/
/* This module supports the iSeries I/O Address translation mapping     */
/* Copyright (C) 20yy  <Allan H Trautman> <IBM Corp>                    */
/*                                                                      */
/* This program is free software; you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation; either version 2 of the License, or    */
/* (at your option) any later version.                                  */
/*                                                                      */
/* This program is distributed in the hope that it will be useful,      */ 
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU General Public License for more details.                         */
/*                                                                      */
/* You should have received a copy of the GNU General Public License    */ 
/* along with this program; if not, write to the:                       */
/* Free Software Foundation, Inc.,                                      */ 
/* 59 Temple Place, Suite 330,                                          */ 
/* Boston, MA  02111-1307  USA                                          */
/************************************************************************/
/* Change Activity:                                                     */
/*   Created, December 14, 2000                                         */
/*   Added Bar table for IoMm performance.                              */
/*   Ported to ppc64                                                    */
/*   Added dynamic table allocation                                     */
/* End Change Activity                                                  */
/************************************************************************/
#include <asm/types.h>
#include <asm/resource.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <asm/ppcdebug.h>
#include <asm/flight_recorder.h>
#include <asm/iSeries/HvCallPci.h>
#include <asm/iSeries/iSeries_pci.h>

#include "iSeries_IoMmTable.h"
#include "pci.h"

/*******************************************************************/
/* Table defines                                                   */
/* Each Entry size is 4 MB * 1024 Entries = 4GB I/O address space. */
/*******************************************************************/
#define Max_Entries 1024
unsigned long iSeries_IoMmTable_Entry_Size = 0x0000000000400000; 
unsigned long iSeries_Base_Io_Memory       = 0xE000000000000000;
unsigned long iSeries_Max_Io_Memory        = 0xE000000000000000;
static   long iSeries_CurrentIndex         = 0;

/*******************************************************************/
/* Lookup Tables.                                                  */
/*******************************************************************/
struct iSeries_Device_Node** iSeries_IoMmTable;
u8*                          iSeries_IoBarTable;

/*******************************************************************/
/* Static and Global variables                                     */
/*******************************************************************/
static char*      iSeriesPciIoText     = "iSeries PCI I/O";
static spinlock_t iSeriesIoMmTableLock = SPIN_LOCK_UNLOCKED;

/*******************************************************************/
/* iSeries_IoMmTable_Initialize                                    */
/*******************************************************************/
/* Allocates and initalizes the Address Translation Table and Bar  */
/* Tables to get them ready for use.  Must be called before any    */
/* I/O space is handed out to the device BARs.                     */
/* A follow up method,iSeries_IoMmTable_Status can be called to    */
/* adjust the table after the device BARs have been assiged to     */
/* resize the table.                                               */
/*******************************************************************/
void iSeries_IoMmTable_Initialize(void)
{
	spin_lock(&iSeriesIoMmTableLock);
	iSeries_IoMmTable  = kmalloc(sizeof(void*)*Max_Entries,GFP_KERNEL);
	iSeries_IoBarTable = kmalloc(sizeof(u8)*Max_Entries,   GFP_KERNEL);
	spin_unlock(&iSeriesIoMmTableLock);
	PCIFR("IoMmTable Initialized 0x%p",  iSeries_IoMmTable);
	if(iSeries_IoMmTable == NULL || iSeries_IoBarTable == NULL) {
		panic("PCI: I/O tables allocation failed.\n");
	}
}

/*******************************************************************/
/* iSeries_IoMmTable_AllocateEntry                                 */
/*******************************************************************/
/* Adds pci_dev entry in address translation table                 */
/*******************************************************************/
/* - Allocates the number of entries required in table base on BAR */
/*   size.                                                         */
/* - Allocates starting at iSeries_Base_Io_Memory and increases.   */
/* - The size is round up to be a multiple of entry size.          */
/* - CurrentIndex is incremented to keep track of the last entry.  */
/* - Builds the resource entry for allocated BARs.                 */
/*******************************************************************/
static void iSeries_IoMmTable_AllocateEntry(struct pci_dev* PciDev, int BarNumber)
{
	struct resource* BarResource = &PciDev->resource[BarNumber];
	long             BarSize     = pci_resource_len(PciDev,BarNumber);
	/***********************************************************/
	/* No space to allocate, quick exit, skip Allocation.      */
	/***********************************************************/
	if(BarSize == 0) return;
	/***********************************************************/
	/* Set Resource values.                                    */
	/***********************************************************/
	spin_lock(&iSeriesIoMmTableLock);
	BarResource->name  = iSeriesPciIoText;
	BarResource->start = iSeries_IoMmTable_Entry_Size*iSeries_CurrentIndex;
	BarResource->start+= iSeries_Base_Io_Memory;
	BarResource->end   = BarResource->start+BarSize-1;
	/***********************************************************/
	/* Allocate the number of table entries needed for BAR.    */
	/***********************************************************/
	while (BarSize > 0 ) {
		*(iSeries_IoMmTable +iSeries_CurrentIndex) = (struct iSeries_Device_Node*)PciDev->sysdata;
		*(iSeries_IoBarTable+iSeries_CurrentIndex) = BarNumber;
		BarSize -= iSeries_IoMmTable_Entry_Size;
		++iSeries_CurrentIndex;
	}
	iSeries_Max_Io_Memory = (iSeries_IoMmTable_Entry_Size*iSeries_CurrentIndex)+iSeries_Base_Io_Memory;
	spin_unlock(&iSeriesIoMmTableLock);
}

/*******************************************************************/
/* iSeries_allocateDeviceBars                                      */
/*******************************************************************/
/* - Allocates ALL pci_dev BAR's and updates the resources with the*/
/*   BAR value.  BARS with zero length will have the resources     */
/*   The HvCallPci_getBarParms is used to get the size of the BAR  */
/*   space.  It calls iSeries_IoMmTable_AllocateEntry to allocate  */
/*   each entry.                                                   */
/* - Loops through The Bar resourses(0 - 5) including the ROM      */
/*   is resource(6).                                               */
/*******************************************************************/
void iSeries_allocateDeviceBars(struct pci_dev* PciDev)
{
	struct resource* BarResource;
	int              BarNumber;
	for(BarNumber = 0; BarNumber <= PCI_ROM_RESOURCE; ++BarNumber) {
		BarResource = &PciDev->resource[BarNumber];
		iSeries_IoMmTable_AllocateEntry(PciDev, BarNumber);
    	}
}

/************************************************************************/
/* Translates the IoAddress to the device that is mapped to IoSpace.    */
/* This code is inlined, see the iSeries_pci.c file for the replacement.*/
/************************************************************************/
struct iSeries_Device_Node* iSeries_xlateIoMmAddress(void* IoAddress)
{
	return NULL;	   
}

/************************************************************************
 * Status hook for IoMmTable
 ************************************************************************/
void     iSeries_IoMmTable_Status(void)
{
	PCIFR("IoMmTable......: 0x%p",iSeries_IoMmTable);
	PCIFR("IoMmTable Range: 0x%p to 0x%p",iSeries_Base_Io_Memory,iSeries_Max_Io_Memory);
	return;
}
