/*creative.h-------------------------------------------------------------------

	Matsushita(Panasonic) / Creative CD-ROM Driver	(matcd)
	Authored by Frank Durda IV

Copyright 1994, 1995, 2002, 2003  Frank Durda IV.  All rights reserved.
"FDIV" is a trademark of Frank Durda IV.

------------------------------------------------------------------------------

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the author nor the names of their contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

------------------------------------------------------------------------------

See matcd.c for Edit History
*/

/* $FreeBSD$
*/

/*----------------------------------------------------------------------------
These are the I/O port mapping offsets and bit assignments used by Creative
Labs in their implementation of the host interface adapter for the Matsushita
CD-ROM drive.  These may be different in the adapter cards (including sound
cards) made by other vendors.  It is unknown if the Creative interface is
based on a reference design provided by Matsushita.  (If Matsushita did
provide a reference design, you would expect adapters made by other vendors
to be more compatible.)

Note that the Matsushita drives are actually capable of using DMA and
generating interrupts, but none of the host adapters provide the circuitry
needed to do DMA or generate interrupts.

See matcd.h for defines related to the drive command set.

------------------------------------------------------------------------------
	Creative Labs (and compatible) host adapter I/O port mapping offsets
----------------------------------------------------------------------------*/

#define NUMPORTS	4		/*Four ports are decoded by the i/f*/

#define	CMD		0		/*Write - commands*/
#define	DATA		0		/*Read - status from drive.  Also for*/
					/*reading data on Creative adapters.*/
#define	PHASE		1		/*Write - switch between data/status*/
#define STATUS		1		/*Read - bus status*/
#define RESET		2		/*Write - reset all attached drives*/
					/*Any value written will reset*/
#define	ALTDATA		2		/*Read - data on non-Creative adaptrs.*/
#define SELECT		3		/*Write - drive select*/


/*	Creative PHASE port bit assignments
*/

#define	PHASENA		1		/*Access data bytes instead of status*/
					/*on shared data/status port*/


/*	Creative STATUS port register bits
*/

#define	DTEN		2		/*When low, in data xfer phase*/
#define STEN		4		/*When low, in status phase*/
#define TEST		1		/*Function is unknown*/


/*	Creative drive SELECT port bit assignments
	Note that on the Creative host adapters, DS0==Bit 1 and
	DS1==Bit 0   (DS is Drive Select).
*/

#define CRDRIVE0	0x00
#define CRDRIVE1	0x02
#define CRDRIVE2	0x01
#define CRDRIVE3	0x03

/*End of creative.h*/

