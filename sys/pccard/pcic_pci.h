/*
 * Copyright (c) 1997 Ted Faber
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Ted Faber.
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


/* CL-PD6832 CardBus defines */
#define PCI_DEVICE_ID_PCIC_PD6832 0x11101013ul

/* PCI configuration registers */
#define PD6832_IO_BASE0			0x002c
#define PD6832_IO_LIMIT0		0x0030
#define PD6832_IO_BASE1			0x0034
#define PD6832_IO_LIMIT1		0x0038
#define PD6832_BRIDGE_CONTROL		0x003c
#define PD6832_LEGACY_16BIT_IOADDR	0x0044
#define PD6832_SOCKET	 		0x004c

/* Configuration constants */
#define PD6832_BCR_MGMT_IRQ_ENA		0x08000000
#define PD6832_BCR_ISA_IRQ		0x00800000
#define PD6832_COMMAND_DEFAULTS		0x00000045
#define PD6832_NUM_REGS			4

/* End of CL-PD6832 defines */
