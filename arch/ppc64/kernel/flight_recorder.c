/************************************************************************
 * flight_recorder.c
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
 * This is a simple text based flight recorder.  Useful for logging 
 * information the you may want to retreive at a latter time.  Errors or 
 * debug inforamtion are good examples.   A good method to dump the 
 * information is via the proc file system. 
 *
 * To use. 
 * 1. Create the flight recorder object.  Passing a NULL pointer will 
 *    kmalloc the space for you.  If it is too early for kmalloc, create 
 *    space for the object.   Beware, don't lie about the size, you will
 *    pay for that later. 
 * 		FlightRecorder* TestFr = alloc_Flight_Recorder(NULL,"TestFr",4096);
 *   
 * 2. Log any notable events, initialzation, error conditions, etc. 
 *		LOGFR(TestFr,"5. Stack Variable(10) %d",StackVariable);
 * 
 * 3. Dump the information to a buffer. 
 *		fr_Dump(TestFr, proc_file_buffer, proc_file_buffer_size);
 *
 ************************************************************************/
#include <stdarg.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <asm/string.h>
#include <asm/time.h>
#include <asm/flight_recorder.h>

static  char       LogText[512];
static  int        LogTextIndex;
static  int        LogCount = 0;
static  spinlock_t Fr_Lock;

/************************************************************************
 * Build the log time prefix based on Flags.
 *  00 = No time prefix
 *  01 = Date(mmddyy) Time(hhmmss) prefix
 *  02 = Day(dd) Time(hhmmss) prefix
 *  03 = Time(hhmmss) prefix
 ************************************************************************/
static void fr_Log_Time(FlightRecorder* Fr)
{
	struct  timeval  TimeClock;
	struct  rtc_time LogTime;

	do_gettimeofday(&TimeClock);
	to_tm(TimeClock.tv_sec, &LogTime);

	if (Fr->Flags == 1) {    
		LogTextIndex = sprintf(LogText,"%02d%02d%02d %02d%02d%02d ",
		                       LogTime.tm_mon, LogTime.tm_mday, LogTime.tm_year-2000,
	     	                  LogTime.tm_hour,LogTime.tm_min, LogTime.tm_sec);
	}
	else if (Fr->Flags == 2) {    
		LogTextIndex = sprintf(LogText,"%02d %02d%02d%02d ",
		                       LogTime.tm_mday,
	     	                  LogTime.tm_hour,LogTime.tm_min, LogTime.tm_sec);
	}

	else if (Fr->Flags == 3) {    
		LogTextIndex = sprintf(LogText,"%02d%02d%02d ",
	     	                  LogTime.tm_hour,LogTime.tm_min, LogTime.tm_sec);
	}
	else {
		++LogCount;
		LogTextIndex = sprintf(LogText,"%04d. ",LogCount);
	}
}

/************************************************************************/
/* Log entry into buffer,                                               */
/* ->If entry is going to wrap, log "WRAP" and start at the top.        */
/************************************************************************/
static void fr_Log_Data(FlightRecorder* Fr)
{
	int 	TextLen  = strlen(LogText);
	int 	Residual = ( Fr->EndPointer - Fr->NextPointer)-15;
	if (TextLen > Residual) {
		strcpy(Fr->NextPointer,"WRAP");
		Fr->WrapPointer = Fr->NextPointer + 5;
		Fr->NextPointer = Fr->StartPointer;
	}
	strcpy(Fr->NextPointer,LogText);
	Fr->NextPointer += TextLen+1;
	strcpy(Fr->NextPointer,"<=");
}
/************************************************************************
 * Build the log text, support variable args.
 ************************************************************************/
void fr_Log_Entry(struct flightRecorder* LogFr, const char *fmt, ...)
{
	va_list arg_ptr;
	spin_lock(&Fr_Lock);

	fr_Log_Time(LogFr); 
	va_start(arg_ptr, fmt);
	vsprintf(LogText+LogTextIndex, fmt, arg_ptr);
	va_end(arg_ptr);
	fr_Log_Data(LogFr);

	spin_unlock(&Fr_Lock);

}
/************************************************************************
 * Dump Flight Recorder into buffer.
 * -> Handles the buffer wrapping.
 ************************************************************************/
int fr_Dump(FlightRecorder* Fr, char *Buffer, int BufferLen)
{
	int   LineLen = 0;
	char* StartEntry;
	char* EndEntry;
	spin_lock(&Fr_Lock);
	/****************************************************************
	 * If Buffer has wrapped, find last usable entry to start with.
	 ****************************************************************/
	if (Fr->WrapPointer != NULL) {
		StartEntry  = Fr->NextPointer+3;
		StartEntry += strlen(StartEntry)+1;
		EndEntry    = Fr->WrapPointer;

		while (EndEntry > StartEntry && LineLen < BufferLen) {
			LineLen    += sprintf(Buffer+LineLen,"%s\n",StartEntry);
			StartEntry += strlen(StartEntry) + 1;
		}
	}

	/****************************************************************
	 * Dump from the beginning to the last logged entry
	 ****************************************************************/
	StartEntry = Fr->StartPointer;
	EndEntry   = Fr->NextPointer;
	while (EndEntry > StartEntry && LineLen < BufferLen) {
		LineLen    += sprintf(Buffer+LineLen,"%s\n",StartEntry);
		StartEntry += strlen(StartEntry) + 1;
	}
	spin_unlock(&Fr_Lock);
	return LineLen;
}

/************************************************************************
 * Allocate and Initialized the Flight Recorder
 * -> If no FlightRecorder pointer is passed, the space is kmalloc.
 ************************************************************************/
FlightRecorder* alloc_Flight_Recorder(FlightRecorder* FrPtr, char* Signature, int SizeOfFr)
{
	FlightRecorder* Fr     = FrPtr;				/* Pointer to Object */
	int             FrSize = (SizeOfFr/16)*16;	/* Could be static   */
	if (Fr == NULL)
		Fr = (FlightRecorder*)kmalloc(SizeOfFr, GFP_KERNEL);
	memset(Fr,0,SizeOfFr);
	strcpy(Fr->Signature,Signature);
	Fr->Size         = FrSize;
	Fr->Flags        = 0;
	Fr->StartPointer = (char*)&Fr->Buffer;
	Fr->EndPointer   = (char*)Fr + Fr->Size;
	Fr->NextPointer  = Fr->StartPointer;

	fr_Log_Entry(Fr,"Initialized.");
	return Fr;
}
