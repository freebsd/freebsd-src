/*
 * Copyright (c) KATO Takenori, 1996.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __PC98_PC98_AIC_98_H__
#define __PC98_PC98_AIC_98_H__



/* generic card */
static int aicport_generic[32] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
};

/* PC-9801-100 */
static int aicport_100[32] = {
	0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e,
	0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e,
	0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e,
	0x30, 0x32, 0x34, 0x36, 0x38, 0x3a, 0x3c, 0x3e
};

#define AIC98_GENERIC		0x00
#define AIC98_100			0x01

#define AIC_TYPE98(x)		((x >> 16) & 0xff)

#define SCSISEQ		(iobase + aic->aicport[0x00]) /* SCSI sequence control */
#define SXFRCTL0	(iobase + aic->aicport[0x01]) /* SCSI transfer control 0 */
#define SXFRCTL1	(iobase + aic->aicport[0x02]) /* SCSI transfer control 1 */
#define SCSISIGI	(iobase + aic->aicport[0x03]) /* SCSI signal in */
#define SCSISIGO	(iobase + aic->aicport[0x03]) /* SCSI signal out */
#define SCSIRATE	(iobase + aic->aicport[0x04]) /* SCSI rate control */
#define SCSIID		(iobase + aic->aicport[0x05]) /* SCSI ID */
#define SELID		(iobase + aic->aicport[0x05]) /* Selection/Reselection ID */
#define SCSIDAT		(iobase + aic->aicport[0x06]) /* SCSI Latched Data */
#define SCSIBUS		(iobase + aic->aicport[0x07]) /* SCSI Data Bus*/
#define STCNT0		(iobase + aic->aicport[0x08]) /* SCSI transfer count */
#define STCNT1		(iobase + aic->aicport[0x09)
#define STCNT2		(iobase + aic->aicport[0x0a)
#define CLRSINT0	(iobase + aic->aicport[0x0b]) /* Clear SCSI interrupts 0 */
#define SSTAT0		(iobase + aic->aicport[0x0b]) /* SCSI interrupt status 0 */
#define CLRSINT1	(iobase + aic->aicport[0x0c]) /* Clear SCSI interrupts 1 */
#define SSTAT1		(iobase + aic->aicport[0x0c]) /* SCSI status 1 */
#define SSTAT2		(iobase + aic->aicport[0x0d]) /* SCSI status 2 */
#define SCSITEST	(iobase + aic->aicport[0x0e]) /* SCSI test control */
#define SSTAT3		(iobase + aic->aicport[0x0e]) /* SCSI status 3 */
#define CLRSERR		(iobase + aic->aicport[0x0f]) /* Clear SCSI errors */
#define SSTAT4		(iobase + aic->aicport[0x0f]) /* SCSI status 4 */
#define SIMODE0		(iobase + aic->aicport[0x10]) /* SCSI interrupt mode 0 */
#define SIMODE1		(iobase + aic->aicport[0x11]) /* SCSI interrupt mode 1 */
#define DMACNTRL0	(iobase + aic->aicport[0x12]) /* DMA control 0 */
#define DMACNTRL1	(iobase + aic->aicport[0x13]) /* DMA control 1 */
#define DMASTAT		(iobase + aic->aicport[0x14]) /* DMA status */
#define FIFOSTAT	(iobase + aic->aicport[0x15]) /* FIFO status */
#define DMADATA		(iobase + aic->aicport[0x16]) /* DMA data */
#define DMADATAL	(iobase + aic->aicport[0x16]) /* DMA data low byte */
#define DMADATAH	(iobase + aic->aicport[0x17]) /* DMA data high byte */
#define BRSTCNTRL	(iobase + aic->aicport[0x18]) /* Burst Control */
#define DMADATALONG	(iobase + aic->aicport[0x18)
#define PORTA		(iobase + aic->aicport[0x1a]) /* Port A */
#define PORTB		(iobase + aic->aicport[0x1b]) /* Port B */
#define REV		(iobase + aic->aicport[0x1c]) /* Revision (001 for 6360) */
#define STACK		(iobase + aic->aicport[0x1d]) /* Stack */
#define TEST		(iobase + aic->aicport[0x1e]) /* Test register */
#define ID		(iobase + aic->aicport[0x1f]) /* ID register */
#endif
