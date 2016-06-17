#ifndef _ISERIES_IOMMTABLE_H
#define _ISERIES_IOMMTABLE_H
/************************************************************************/
/* File iSeries_IoMmTable.h created by Allan Trautman on Dec 12 2001.   */
/************************************************************************/
/* Interfaces for the write/read Io address translation table.          */
/* Copyright (C) 20yy  Allan H Trautman, IBM Corporation                */
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
/*   Created December 12, 2000                                          */
/*   Ported to ppc64, August 30, 2001                                   */
/* End Change Activity                                                  */
/************************************************************************/

struct pci_dev;
struct iSeries_Device_Node;

extern struct iSeries_Device_Node** iSeries_IoMmTable;
extern u8*                          iSeries_IoBarTable;
extern unsigned long iSeries_Base_Io_Memory;
extern unsigned long iSeries_Max_Io_Memory;
extern unsigned long iSeries_Base_Io_Memory;
extern unsigned long iSeries_IoMmTable_Entry_Size;
/************************************************************************/
/* iSeries_IoMmTable_Initialize                                         */
/************************************************************************/
/* - Initalizes the Address Translation Table and get it ready for use. */
/*   Must be called before any client calls any of the other methods.   */
/*                                                                      */
/* Parameters: None.                                                    */
/*                                                                      */
/* Return: None.                                                        */  
/************************************************************************/
extern  void iSeries_IoMmTable_Initialize(void);
extern  void iSeries_IoMmTable_Status(void);

/************************************************************************/
/* iSeries_allocateDeviceBars                                           */
/************************************************************************/
/* - Allocates ALL pci_dev BAR's and updates the resources with the BAR */
/*   value.  BARS with zero length will not have the resources.  The    */
/*   HvCallPci_getBarParms is used to get the size of the BAR space.    */
/*   It calls iSeries_IoMmTable_AllocateEntry to allocate each entry.   */
/*                                                                      */
/* Parameters:                                                          */
/* pci_dev = Pointer to pci_dev structure that will be mapped to pseudo */
/*           I/O Address.                                               */
/*                                                                      */
/* Return:                                                              */
/*   The pci_dev I/O resources updated with pseudo I/O Addresses.       */
/************************************************************************/
extern  void iSeries_allocateDeviceBars(struct pci_dev* );

/************************************************************************/
/* iSeries_xlateIoMmAddress                                             */
/************************************************************************/
/* - Translates an I/O Memory address to Device Node that has been the  */
/*   allocated the pseudo I/O Address.                                  */
/*                                                                      */
/* Parameters:                                                          */
/* IoAddress = I/O Memory Address.                                      */
/*                                                                      */
/* Return:                                                              */
/*   An iSeries_Device_Node to the device mapped to the I/O address. The*/
/*   BarNumber and BarOffset are valid if the Device Node is returned.  */
/************************************************************************/
extern struct iSeries_Device_Node* iSeries_xlateIoMmAddress(void* IoAddress);

#endif /* _ISERIES_IOMMTABLE_H */
