/*
 *
 *	Copyright (C) 1994, Paul S. LaFollette, Jr. This software may be used,
 *	modified, copied, distributed, and sold, in both source and binary form
 *	provided that the above copyright and these terms are retained. Under
 *	no circumstances is the author responsible for the proper functioning
 *	of this software, nor does the author assume any responsibility
 *	for damages incurred with its use
 *
 *	$FreeBSD$
 */

/*
 *	Register and bit definitions for CORTEX-I frame grabber
 */

#ifndef _I386_ISA_CTXREG_H_
#define _I386_ISA_CTXREG_H_

	/*  Control Ports (all are write-only) */

#define ctx_cp0 0		/*	offset to control port 0   */
#define ctx_cp1 1		/*	offset to control port 1   */
#define ctx_lutaddr 2		/*	offset to lut address port */
#define ctx_lutdata 3		/*	offset to lut data port	   */

	/*  Status port (read-only but same address as control port 0)  */

#define ctx_status 0		/*	offset to status port 	   */

	/*  Bit assignments for control port 0 */

#define PAGE_SELECT0 1		/* These two bits choose which 1/4 of the   */
#define PAGE_SELECT1 2		/* video memory is accessible to us.        */
#define PAGE_ROTATE 4		/* 0 => horizontal access. 1 => vertical    */
#define ACQUIRE 8		/* set to start frame grab		    */
#define SEE_STORED_VIDEO 16	/* set to allow stored frame to be seen     */
#define LOW_RESOLUTION 32	/* set to enable 256 x 256 mode		    */

	/*  Bit assignments for control port 1 */

#define INTERRUPT_ENABLE 1	/* Allow interrupts (we avoid this bit)     */
#define TRIGGER_ENABLE 2	/* Enable external trigger for frame grab   */
#define LUT_LOAD_ENABLE 4	/* Allow loading of lookup table	    */
#define BLANK_DISPLAY 8		/* Turn off display 		            */
#define AB_SELECT 16		/* Along with HW switch, choose base memory */
#define RAM_ENABLE 32		/* Connect video RAM to computer bus	    */

	/*  Bit assignments for status port */

#define INTERRUPT_STATUS 1	/* Ignored by us			    */
#define ADC_OVERFLOW 2		/* Set if any pixes from camera "too bright"*/
#define FIELD 4			/* 0 or 1 shows which interlace field are in*/
#define VERTICAL_BLANK 8	/* 1 if in vertical blanking interval       */
#define TRIGGERED 16		/* 1 if HW trigger contacts closed	    */
#define ACQUIRING_ACK 32        /* 1 if currently grabbing a frame          */


#endif /* ifndef  _I386_ISA_CTXREG_H_ */
