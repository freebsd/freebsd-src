/* $FreeBSD$ */

/*
 * Copyright (c) 1998, 2000 by Matthew Jacob
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * There are four PCI slots per MCPCIA PCI bus here, but some are 'hidden'-
 * none seems to be higher than 6 though.
 */
#define	MCPCIA_MAXDEV	6
#define	MCPCIA_MAXSLOT	8

/*
 * Interrupt Stuff for MCPCIA systems.
 *
 * EISA interrupts (at vector 0x800) have to be shared interrupts-
 * and that can be easily managed. All the PCI interrupts are deterministic
 * in that they start at vector 0x900, 0x40 per PCI slot, 0x200 per
 * MCPCIA, 4 MCPCIAs per GCBUS....
 */
#define MCPCIA_EISA_KEYB_IRQ	1
#define MCPCIA_EISA_MOUSE_IRQ	12
#define MCPCIA_VEC_EISA		0x800
#define	MCPCIA_EISA_IRQ		16
#define MCPCIA_VEC_PCI		0x900
#define	MCPCIA_VEC_NCR		0xB40
#define	MCPCIA_NCR_IRQ		16

#define	MCPCIA_VECWIDTH_PER_MCPCIA	0x200
#define	MCPCIA_MID_SHIFT		9
#define	MCPCIA_VECWIDTH_PER_SLOT	0x40
#define	MCPCIA_SLOT_SHIFT		6
#define	MCPCIA_VECWIDTH_PER_INTPIN	0x10
#define	MCPCIA_IRQ_SHIFT		4

/*
 * Special Vectors
 */
#define	MCPCIA_I2C_CVEC		0xA90
#define	MCPCIA_I2C_BVEC		0xAA0

extern void mcpcia_init(int, int);
