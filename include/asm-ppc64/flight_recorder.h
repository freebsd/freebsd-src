/************************************************************************
 * flight_recorder.h                                                    *
 ************************************************************************
 * This code supports the a generic flight recorder.                    *
 * Copyright (C) 20yy  <Allan H Trautman> <IBM Corp>                    * 
 *                                                                      *
 * This program is free software; you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation; either version 2 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      * 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    * 
 * along with this program; if not, write to the:                       *
 * Free Software Foundation, Inc.,                                      * 
 * 59 Temple Place, Suite 330,                                          * 
 * Boston, MA  02111-1307  USA                                          *
 ************************************************************************
 * See the fight_recorder.c file for useage deails.                     *
 ************************************************************************/
#include <linux/kernel.h>

/************************************************************************
 * Generic Flight Recorder Structure                                    *
 ************************************************************************/
struct flightRecorder {                 /* Structure Defination         */
	char  Signature[8];                /* Eye Catcher                  */
	int   Size;			            /* Size of Flight Recorder      */
	int   Flags;                       /* Format Flags.                */				
	char* StartPointer;                /* Buffer Starting Address      */
	char* EndPointer;                  /* Buffer Ending Address        */
	char* NextPointer;                 /* Next Entry Address           */
	char* WrapPointer;                 /* Point at which buffer wraps  */
	char* Buffer;                      /* Where the data log is.       */
};
typedef struct flightRecorder FlightRecorder;

/************************************************************************
 * Forware declares
 ************************************************************************/
FlightRecorder* alloc_Flight_Recorder(FlightRecorder* FrPtr, char* Signature, int SizeOfFr);
void            fr_Log_Entry(FlightRecorder* LogFr, const char *fmt, ...);
int             fr_Dump(FlightRecorder* Fr, char *Buffer, int BufferLen);

/************************************************************************
 * Sample Macro to make life easier using the flight_recorder. 
 * TestFr is a global value.
 * To use them: TESTFR("Test Loop value is &d",Loop");
 ************************************************************************/
#define LOGFR(...) (fr_Log_Entry(__VA_ARGS__))

