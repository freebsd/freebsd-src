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
 *
 * $Id: pcic_p.h,v 1.2 1997/12/02 22:13:59 nate Exp $
 */

/* PCI/CardBus Device IDs */
#define	PCI_DEVICE_ID_PCIC_CLPD6729	0x11001013ul
#define	PCI_DEVICE_ID_PCIC_CLPD6832	0x11101013ul
#define	PCI_DEVICE_ID_PCIC_O2MICRO	0x673A1217ul
#define	PCI_DEVICE_ID_PCIC_TI1130	0xAC12104Cul
#define	PCI_DEVICE_ID_PCIC_TI1131	0xAC15104Cul

/* CL-PD6832 CardBus defines */
#define	CLPD6832_IO_BASE0		0x002c
#define	CLPD6832_IO_LIMIT0		0x0030
#define	CLPD6832_IO_BASE1		0x0034
#define	CLPD6832_IO_LIMIT1		0x0038
#define	CLPD6832_BRIDGE_CONTROL		0x003c
#define	CLPD6832_LEGACY_16BIT_IOADDR	0x0044
#define	CLPD6832_SOCKET	 		0x004c

/* Configuration constants */
#define	CLPD6832_BCR_MGMT_IRQ_ENA	0x08000000
#define	CLPD6832_BCR_ISA_IRQ		0x00800000
#define	CLPD6832_COMMAND_DEFAULTS	0x00000045
#define	CLPD6832_NUM_REGS		2

/* End of CL-PD6832 defines */
