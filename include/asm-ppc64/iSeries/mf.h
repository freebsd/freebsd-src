/*
 * mf.h
 * Copyright (C) 2001  Troy D. Armstrong IBM Corporation
 *
 * This modules exists as an interface between a Linux secondary partition
 * running on an iSeries and the primary partition's Virtual Service
 * Processor (VSP) object.  The VSP has final authority over powering on/off
 * all partitions in the iSeries.  It also provides miscellaneous low-level
 * machine facility type operations.
 *
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifndef MF_H_INCLUDED
#define MF_H_INCLUDED

#include <asm/iSeries/HvTypes.h>
#include <asm/iSeries/HvLpEvent.h>

struct rtc_time;

typedef void (*MFCompleteHandler)( void * clientToken, int returnCode );

extern void mf_allocateLpEvents( HvLpIndex targetLp,
				 HvLpEvent_Type type,
				 unsigned size,
				 unsigned amount,
				 MFCompleteHandler hdlr,
				 void * userToken );

extern void mf_deallocateLpEvents( HvLpIndex targetLp,
				   HvLpEvent_Type type,
				   unsigned count,
				   MFCompleteHandler hdlr,
				   void * userToken );

extern void mf_powerOff( void );

extern void mf_reboot( void );

extern void mf_displaySrc( u32 word );
extern void mf_displayProgress( u16 value );

extern void mf_clearSrc( void );

extern void mf_init( void );

extern void mf_setSide(char side);

extern char mf_getSide(void);

extern void mf_setCmdLine(const char *cmdline, int size, u64 side);

extern int  mf_getCmdLine(char *cmdline, int *size, u64 side);

extern void mf_getSrcHistory(char *buffer, int size);

extern int mf_setVmlinuxChunk(const char *buffer, int size, int offset, u64 side);

extern int mf_getVmlinuxChunk(char *buffer, int *size, int offset, u64 side);

extern int mf_setRtcTime(unsigned long time);

extern int mf_getRtcTime(unsigned long *time);

extern int mf_getRtc( struct rtc_time * tm );

extern int mf_setRtc( struct rtc_time * tm );

#endif /* MF_H_INCLUDED */
