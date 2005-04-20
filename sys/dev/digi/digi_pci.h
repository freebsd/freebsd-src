/*-
 * Copyright (c) 2001 Brian Somers <brian@Awfulhak.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#define	PCI_VENDOR_DIGI		0x114F
#define	PCI_DEVICE_EPC		0x0002
#define	PCI_DEVICE_RIGHTSWITCH	0x0003	/* For testing */
#define	PCI_DEVICE_XEM		0x0004
#define	PCI_DEVICE_XR		0x0005
#define	PCI_DEVICE_CX		0x0006
#define	PCI_DEVICE_XRJ		0x0009	/* Jupiter boards with */
#define	PCI_DEVICE_EPCJ		0x000a	/* PLX 9060 chip for PCI  */
#define	PCI_DEVICE_XR_422	0x0012	/* Xr-422 */
#define	PCI_DEVICE_920_4	0x0026	/* XR-Plus 920 K, 4 port */
#define	PCI_DEVICE_920_8	0x0027	/* XR-Plus 920 K, 8 port */
#define	PCI_DEVICE_920_2	0x0034	/* XR-Plus 920 K, 2 port */

#define	PCIPORT			sc->vmem[0x200000]
