/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 *
 *	from: Mach, unknown, 386BSD patch kit
 *	$Id: pio.h,v 1.3 1993/11/07 17:43:01 wollman Exp $
 */

#ifndef _MACHINE_PIO_H_
#define _MACHINE_PIO_H_ 1

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
#endif /* _MACHINE_PIO_H_ */
