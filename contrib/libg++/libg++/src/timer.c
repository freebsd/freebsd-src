/* 
Copyright (C) 1990, 1992, 1995 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)

This file is part of the GNU C++ Library.  This library is free
software; you can redistribute it and/or modify it under the terms of
the GNU Library General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.  This library is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifdef __GNUG__
#pragma implementation
#endif

/* Timing functions from Doug Schmidt... */

/* no such thing as "negative time"! */
#define  TIMER_ERROR_VALUE -1.0   

/* If this does not work on your system, change this to #if 0, and 
   report the problem. */

#if 1

#include <_G_config.h>
#include <sys/types.h>
#if _G_HAVE_SYS_RESOURCE
#include <sys/time.h>
#include <sys/resource.h>
#endif
#if !_G_HAVE_SYS_RESOURCE || !defined(RUSAGE_SELF)
#if _G_HAVE_SYS_TIMES
#define USE_TIMES
#include <sys/param.h>
#include <sys/times.h>
#if !defined (HZ) && defined(CLK_TCK)
#define HZ CLK_TCK
#endif
static struct tms Old_Time;
static struct tms New_Time;
#else /* ! _G_HAVE_SYS_TIMES */
#define USE_CLOCK
#include <time.h>
#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC 1
#endif
clock_t Old_Time;
clock_t New_Time;
#endif /* ! _G_HAVE_SYS_TIMES */
#else /* _G_HAVE_SYS_RESOURCE && defined(RUSAGE_SELF) */
static struct rusage Old_Time;
static struct rusage New_Time;
#endif
static int    Timer_Set = 0;

double
start_timer()
{
   Timer_Set = 1;
#ifdef USE_CLOCK
   Old_Time = clock() / CLOCKS_PER_SEC;
   return((double) Old_Time);
#else
#ifdef USE_TIMES
   times(&Old_Time);
   return((double) Old_Time.tms_utime / HZ);
#else
   getrusage(RUSAGE_SELF,&Old_Time);        /* set starting process time */
   return(Old_Time.ru_utime.tv_sec + (Old_Time.ru_utime.tv_usec / 1000000.0));
#endif
#endif
}

/* Returns process time since Last_Time.
   If parameter is 0.0, returns time since the Old_Time was set.
   Returns TIMER_ERROR_VALUE if `start_timer' is not called first.  */

double
return_elapsed_time(Last_Time)
     double Last_Time;
{
   if (!Timer_Set) {
      return(TIMER_ERROR_VALUE);
   }   
   else {
    /* get process time */
#ifdef USE_CLOCK
      New_Time = clock();
#else
#ifdef USE_TIMES
      times(&New_Time);
#else
      getrusage(RUSAGE_SELF,&New_Time);
#endif
#endif
      if (Last_Time == 0.0) {
#ifdef USE_CLOCK
	 return((double) (New_Time - Old_Time) / CLOCKS_PER_SEC);
#else
#ifdef USE_TIMES
	 return((double) (New_Time.tms_utime - Old_Time.tms_utime) / HZ);
#else
         return((New_Time.ru_utime.tv_sec - Old_Time.ru_utime.tv_sec) + 
               ((New_Time.ru_utime.tv_usec - Old_Time.ru_utime.tv_usec) 
                / 1000000.0));
#endif
#endif
      }
      else {
#ifdef USE_CLOCK
	 return((double) New_Time / CLOCKS_PER_SEC - Last_Time);
#else
#ifdef USE_TIMES
	 return((double) New_Time.tms_utime / HZ - Last_Time);
#else
         return((New_Time.ru_utime.tv_sec + 
                (New_Time.ru_utime.tv_usec / 1000000.0)) - Last_Time);
#endif
#endif
      }
   }
}

#ifdef VMS
void sys$gettim(unsigned int*) asm("sys$gettim");

getrusage(int dummy,struct rusage* time){
	double rtime;
	unsigned int systime[2];
	int i;
	sys$gettim(&systime[0]);
	rtime=systime[1];
	for(i=0;i<4;i++) rtime *= 256;
	rtime+= systime[0];
/* we subtract an offset to make sure that the number fits in a long int*/
	rtime=rtime/1.0e+7-4.144e+9;
	time->ru_utime.tv_sec= rtime;
	rtime=(rtime-time->ru_utime.tv_sec)*1.0e6;	
	time->ru_utime.tv_usec= rtime;
}
#endif
#else /* dummy them out */

double start_timer()
{
  return TIMER_ERROR_VALUE;
}

double
return_elapsed_time(Last_Time)
     double Last_Time;
{
  return TIMER_ERROR_VALUE;
}

#endif /* timing stuff */


