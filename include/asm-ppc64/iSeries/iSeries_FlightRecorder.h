#ifndef _ISERIES_FLIGHTRECORDER_H
#define _ISERIES_FLIGHTRECORDER_H
/************************************************************************/
/* File iSeries_FlightRecorder.h created by Allan Trautman Jan 22 2001. */
/************************************************************************/
/* This code supports the pci interface on the IBM iSeries systems.     */
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
/*   Created, Jan 22, 2001                                              */
/*   Added Time stamp methods. Apr 12, 2001                             */
/* End Change Activity                                                  */
/************************************************************************/
/* This is a generic Flight Recorder, simply stuffs line entries into a */
/* buffer for debug purposes.                                           */
/*                                                                      */
/* To use,                                                              */ 
/* 1. Create one, make it global so it isn't on the stack.              */
/*     FlightRecorder  PciFlightRecorder;                               */
/*                                                                      */
/* 2. Optionally create a pointer to it, just makes it easier to use.   */
/*     FlightRecorder* PciFr = &PciFlightRecorder;                      */
/*                                                                      */
/* 3. Initialize with you signature.                                    */
/*          iSeries_Fr_Initialize(PciFr, "Pci Flight Recorder");        */
/*                                                                      */
/* 4. Log entries.                                                      */ 
/*          PciFr->logEntry(PciFr,"In Main");                           */
/*                                                                      */
/* 5. Later, you can find the Flight Recorder by looking in the         */
/*    System.map                                                        */
/************************************************************************/
struct iSeries_FlightRecorder;          /* Forward declares             */
struct rtc_time;
void   logEntry(struct iSeries_FlightRecorder*, char* Text);
void   logTime( struct iSeries_FlightRecorder*, char* Text);
void   logDate( struct iSeries_FlightRecorder*, char* Text);
#define FlightRecorderSize 4096 

/************************************************************************/
/* Generic Flight Recorder Structure                                    */
/************************************************************************/
struct iSeries_FlightRecorder {         /* Structure Defination         */
	char  Signature[16];                /* Eye Catcher                  */
	char* StartingPointer;              /* Buffer Starting Address      */
	char* CurrentPointer;               /* Next Entry Address           */
	int   WrapCount;                    /* Number of Buffer Wraps       */
	void  (*logEntry)(struct iSeries_FlightRecorder*,char*);
	void  (*logTime) (struct iSeries_FlightRecorder*,char*);
	void  (*logDate) (struct iSeries_FlightRecorder*,char*);
	char  Buffer[FlightRecorderSize];
};

typedef struct iSeries_FlightRecorder FlightRecorder;	/* Short Name   */
extern void iSeries_Fr_Initialize(FlightRecorder*, char* Signature);
/************************************************************************/
/* extern void iSeries_LogFr_Entry(  FlightRecorder*, char* Text);      */
/* extern void iSeries_LogFr_Date(   FlightRecorder*, char* Text);      */
/* extern void iSeries_LogFr_Time(   FlightRecorder*, char* Text);      */
/************************************************************************/
/* PCI Flight Recorder Helpers                                          */
/************************************************************************/
extern FlightRecorder* PciFr;            /* Ptr to Pci Fr               */
extern char*           PciFrBuffer;      /* Ptr to Fr Work Buffer       */
#define ISERIES_PCI_FR(buffer)      PciFr->logEntry(PciFr,buffer);
#define ISERIES_PCI_FR_TIME(buffer) PciFr->logTime(PciFr,buffer);
#define ISERIES_PCI_FR_DATE(buffer) PciFr->logDate(PciFr,buffer);

#endif /* _ISERIES_FLIGHTRECORDER_H */
