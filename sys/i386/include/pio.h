/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/* 
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00158
 * --------------------         -----   ----------------------
 *
 * 27 May 93	Rodney W. Grimes	Added #ifndef around inb so that the
 *					clash with cpufunc.h does not cause
 *					a compile warning.. this is a hack.
 *					All of this needs cleaned up at 0.1.5
 *
 * HISTORY
 * $Log: pio.h,v $
 * Revision 1.1  1992/05/27  00:48:30  balsup
 * machkern/cor merge
 *
 * Revision 1.1  1991/10/10  20:11:39  balsup
 * Initial revision
 *
 * Revision 2.2  91/04/02  11:52:29  mbj
 * 	[90/08/14            mg32]
 * 
 * 	Now we know how types are factor in.
 * 	Cleaned up a bunch: eliminated ({ for output and flushed unused
 * 	output variables.
 * 	[90/08/14            rvb]
 * 
 * 	This is how its done in gcc:
 * 		Created.
 * 	[90/03/26            rvb]
 * 
 */


#define inl(y) \
({ unsigned long _tmp__; \
	asm volatile("inl %1, %0" : "=a" (_tmp__) : "d" ((unsigned short)(y))); \
	_tmp__; })

#define inw(y) \
({ unsigned short _tmp__; \
	asm volatile(".byte 0x66; inl %1, %0" : "=a" (_tmp__) : "d" ((unsigned short)(y))); \
	_tmp__; })

/*
 * only do this if it has not already be defined.. this is a crock for the
 * patch kit for right now.  Need to clean up all the inx, outx stuff for
 * 0.1.5 to use 1 common header file, that has Bruces fast mode inb/outb
 * stuff in it.  Rgrimes 5/27/93
 */
#ifndef inb
#define inb(y) \
({ unsigned char _tmp__; \
	asm volatile("inb %1, %0" : "=a" (_tmp__) : "d" ((unsigned short)(y))); \
	_tmp__; })
#endif


#define outl(x, y) \
{ asm volatile("outl %0, %1" : : "a" (y) , "d" ((unsigned short)(x))); }


#define outw(x, y) \
{asm volatile(".byte 0x66; outl %0, %1" : : "a" ((unsigned short)(y)) , "d" ((unsigned short)(x))); }


#define outb(x, y) \
{ asm volatile("outb %0, %1" : : "a" ((unsigned char)(y)) , "d" ((unsigned short)(x))); }
