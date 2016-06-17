#ifndef _ISERIES_VPDINFO_H
#define _ISERIES_VPDINFO_H
/************************************************************************/
/* File iSeries_VpdInfo.h created by Allan Trautman Feb 08 2001.        */
/************************************************************************/
/* This code supports the location data fon on the IBM iSeries systems. */
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
/*   Created, Feg  8, 2001                                              */
/*   Reformatted for Card, March 8, 2001                                 */
/* End Change Activity                                                  */
/************************************************************************/

struct pci_dev; 		/* Forward Declare                      */
/************************************************************************/
/* Location Data extracted from the VPD list and device info.           */
/************************************************************************/
struct LocationDataStruct {	/* Location data structure for device   */
	u16  Bus;		/* iSeries Bus Number		    0x00*/
	u16  Board;		/* iSeries Board                    0x02*/
	u8   FrameId;		/* iSeries spcn Frame Id            0x04*/
	u8   PhbId;		/* iSeries Phb Location             0x05*/
	u16  Card;		/* iSeries Card Slot                0x06*/
	char CardLocation[4];	/* Char format of planar vpd        0x08*/
	u8   AgentId;		/* iSeries AgentId                  0x0C*/
	u8   SecondaryAgentId;	/* iSeries Secondary Agent Id       0x0D*/
	u8   LinuxBus;		/* Linux Bus Number                 0x0E*/
	u8   LinuxDevFn;	/* Linux Device Function            0x0F*/
};
typedef struct LocationDataStruct  LocationData;
#define LOCATION_DATA_SIZE      16

/************************************************************************/
/* Protypes                                                             */
/************************************************************************/
extern LocationData* iSeries_GetLocationData(struct pci_dev* PciDev);
extern int           iSeries_Device_Information(struct pci_dev*,char*, int);

#endif /* _ISERIES_VPDINFO_H */
