/*creative.h-------------------------------------------------------------------

	Matsushita(Panasonic) / Creative CD-ROM Driver	(matcd)
	Authored by Frank Durda IV

	Copyright 1994, 1995  Frank Durda IV.  All rights reserved.
	"FDIV" is a trademark of Frank Durda IV.


	Redistribution and use in source and binary forms, with or
	without modification, are permitted provided that the following
	conditions are met:
	1.  Redistributions of source code must retain the above copyright
	    notice positioned at the very beginning of this file without
	    modification, all copyright strings, all related programming
	    codes that display the copyright strings, this list of
	    conditions and the following disclaimer.
	2.  Redistributions in binary form must contain all copyright strings
	    and related programming code that display the copyright strings.
	3.  Redistributions in binary form must reproduce the above copyright
	    notice, this list of conditions and the following disclaimer in
	    the documentation and/or other materials provided with the
	    distribution.
	4.  All advertising materials mentioning features or use of this
	    software must display the following acknowledgement:
		"The Matsushita/Panasonic CD-ROM driver  was developed
		 by Frank Durda IV for use with "FreeBSD" and similar
		 operating systems."
	    "Similar operating systems" includes mainly non-profit oriented
	    systems for research and education, including but not restricted
	    to "NetBSD", "386BSD", and "Mach" (by CMU).  The wording of the
	    acknowledgement (in electronic form or printed text) may not be
	    changed without permission from the author.
	5.  Absolutely no warranty of function, fitness or purpose is made
	    by the author Frank Durda IV.
	6.  Neither the name of the author nor the name "FreeBSD" may
	    be used to endorse or promote products derived from this software
	    without specific prior written permission.
	    (The author can be reached at   bsdmail@nemesis.lonestar.org)
	7.  The product containing this software must meet all of these
	    conditions even if it is unsupported, not a complete system
	    and/or does not contain compiled code.
	8.  These conditions will be in force for the full life of the
	    copyright.
	9.  If all the above conditions are met, modifications to other
	    parts of this file may be freely made, although any person
	    or persons making changes do not receive the right to add their
	    name or names to the copyright strings and notices in this
	    software.  Persons making changes are encouraged to insert edit
	    history in matcd.c and to put your name and details of the
	    change there.
	10. You must have prior written permission from the author to
	    deviate from these terms.

	Vendors who produce product(s) containing this code are encouraged
	(but not required) to provide copies of the finished product(s) to
	the author and to correspond with the author about development
	activity relating to this code.   Donations of development hardware
	and/or software are also welcome.  (This is one of the faster ways
	to get a driver developed for a device.)

 	THIS SOFTWARE IS PROVIDED BY THE DEVELOPER(S) ``AS IS'' AND ANY
 	EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 	PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER(S) BE
 	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 	OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 	OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


-----No changes are allowed above this line------------------------------------

See matcd.c for Edit History

	These are the I/O port mapping offsets and bit assignments used
	by Creative Labs in their implementation of the host interface for
	the Matsushita CD-ROM drive.  These may be different in the adapter
	cards (including sound cards) made by other vendors.
	It is unknown if the Creative interface is based on a reference design
	provided by Matsushita (other interface vendors would similar or
	identical if this was the case).

	The drive is actually capable of some things that the Creative
	interface doesn't implement, such as DMA and interrupts.

	See matcd.h for defines related to the Matsushita drive itself.
*/


/*	Creative Labs (and compatible) I/O port mapping offsets
*/

#define NUMPORTS	4		/*Four ports are decoded by the i/f*/

#define	CMD		0		/*Write - commands*/
#define	DATA		0		/*Read - data/status from drive*/
#ifdef PC98
#define	PHASE		0x100	/*Write - switch between data/status*/
#define STATUS		0x100	/*Read - bus status */
#define RESET		0x200	/*Write - reset all attached drives*/
#define	ALTDATA		0x200	/*<20>Read - data on non Creative bds.*/
#define SELECT		0x300	/*Write - drive select*/
#else /* !PC98 */
#define	PHASE		1		/*Write - switch between data/status*/
#define STATUS		1		/*Read - bus status*/
#define RESET		2		/*Write - reset all attached drives*/
					/*Any value written will reset*/
#define	ALTDATA		2		/*<20>Read - data on non Creative bds.*/
#define SELECT		3		/*Write - drive select*/
#endif        /*PC98*/

/*	Creative PHASE port bit assignments
*/

#define	PHASENA		1		/*Access data bytes instead of status*/


/*	Creative STATUS port register bits
*/

#define	DTEN		2		/*When low, in data xfer phase*/
#define STEN		4		/*When low, in status phase*/
#define TEST		1		/*Function is unknown*/


/*	Creative drive SELECT port bit assignments
	Note that in the Creative interface, DS0==Bit 1 and
	DS1==Bit 0   (DS is Drive Select).
*/

#define CRDRIVE0	0x00
#define CRDRIVE1	0x02
#define CRDRIVE2	0x01
#define CRDRIVE3	0x03

/*End of creative.h*/


