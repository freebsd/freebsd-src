#ifndef _B004_H
#define _B004_H

/*
 * b004.h
 *
 * Based on the Linux driver, by
 * Christoph Niemann (niemann@swt.ruhr-uni-bochum.de)
 *
 * Ported to FreeBSD by Luigi Rizzo (luigi@iet.unipi.it)
 * and Lorenzo Vicisano (l.vicisano@iet.unipi.it)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christoph Niemann,
 *      Luigi Rizzo and Lorenzo Vicisano - Dipartimento di Ingegneria
 *              dell'Informazione
 * 4. The names of these contributors may not be used to endorse or promote
 *  products derived from this software without specific prior written
 *  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Works for FreeBSD 1.1.5
 */

#include <sys/ioctl.h>

/*
 * device status FLAGS
 */
#define B004_EXIST 		0x0001	/* Is a B004-Board with at least one
					   Transputer present ? */
#define B004_BUSY  		0x0002	/* Is the B004-board in use ? */

/*
 * IOCTL numbers
 */
#define B004RESET		_IO  ('Q', 0)
				/* Reset transputer(s) */
#define B004WRITEABLE		_IOR ('Q', 1, int)
				/* Return C012 Output Ready */
#define B004READABLE		_IOR ('Q', 2, int)
				/* Return C012 Data Present */
#define B004ANALYSE		_IO  ('Q', 3)
				/* Switch transputer(s) to ANALYSE mode */
#define B004ERROR		_IOR ('Q', 4, int)
				/* Return 1 on ERROR set */
#define B004TIMEOUT		_IOW ('Q', 5, int)
				/* Set TIMEOUT for subsequent writing or
				   reading call, value in ticks, initial
				   0 = no timeout (read/write blocking)
				   "open" sets timeout to 0 */


#define B004_INIT_TIMEOUT	0  /* No timeout yet */

/*
 * Registers DISPLACEMENT
 */
#define B004_IDR_OFFSET		0		/* Input Data Register */
#define B004_ODR_OFFSET		1		/* Output Data Register */
#define B004_ISR_OFFSET		2		/* Input Status Register */
#define B004_OSR_OFFSET		3		/* Output Status Register */
#define B004_RESET_OFFSET	16		/* Reset/Error Register */
#define B004_ERROR_OFFSET	B004_RESET_OFFSET
#define B004_ANALYSE_OFFSET	17		/* Analyse Register */
#define B008_DMA_OFFSET		18		/* B008: DMA request register */
#define B008_INT_OFFSET		19		/* B008: Interrupt control reg */

struct b004_struct {
	int flags;		/* various flags */
	int idr;		/* address of the input data register */
	int odr;		/* address if the output data register */
	int isr;		/* address of the input status register */
	int osr;		/* address of the output status register */
	unsigned int timeout;	/* timeout for writing/reading the link */
	int boardtype;		/* what kind of board is installed */
	void *devfs_token[8][4]; /* tokens for 4 types for 8 ports */
};

/*
 * Id's for the supported boards
 */
#define B004 1
#define B008 2

/*
 * Defines for easier access to the b004_table.
 */
#define B004_F(minor)			b004_table[minor].flags
#define B004_TIMEOUT(minor)		b004_table[minor].timeout
#define B004_BASE(minor)		B004_IDR(minor)
#define B004_IDR(minor)			b004_table[minor].idr
#define B004_ODR(minor)			b004_table[minor].odr
#define B004_ISR(minor)			b004_table[minor].isr
#define B004_OSR(minor)			b004_table[minor].osr
#define B004_WAIT(minor)		b004_table[minor].wait
#define B004_BOARDTYPE(minor)		b004_table[minor].boardtype

/*
 * Additional defines for B008-boards
 */
#define B008_DMA(minor)			b004_table[minor].int
#define B008_INT(minor)			b004_table[minor].dma

/*
 * Number of tries to access isr or osr before reading or writing sleeps
 */
#define B004_MAXTRY 			200

/*
 * Maximum number of bytes to transfer at once
 */
#define B004_MAX_BYTES			2048

/*
 * bit defines for C012 status ports at base + 2/3
 * accessed with B004_IS, B004_OS, which gets the byte...
 */
#define B004_READBYTE	1
#define B004_WRITEBYTE	1

/*
 * bit defines for C012 reset/error port at base + 16
 */
#define B004_ASSERT_RESET	0x01	/* resetting the transputer */
#define B004_DEASSERT_RESET	0x00
#define B004_TEST_ERROR		0x01	/* for testing the transputer's error flag */

/*
 * bit defines for C012 analyse port at base + 17
 */
#define B004_ASSERT_ANALYSE	0x01	/* switch transputer to analyse-mode */
#define B004_DEASSERT_ANALYSE	0x00

#endif
