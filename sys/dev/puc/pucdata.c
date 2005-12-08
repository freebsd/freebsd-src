/*	$NetBSD: pucdata.c,v 1.25 2001/12/16 22:23:01 thorpej Exp $	*/

/*-
 * Copyright (c) 1998, 1999 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * PCI "universal" communications card driver configuration data (used to
 * match/attach the cards).
 */

#include <sys/param.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/sio/sioreg.h>
#include <dev/puc/pucvar.h>

#define COM_FREQ	DEFAULT_RCLK

int puc_config_win877(struct puc_softc *);

const struct puc_device_description puc_devices[] = {

	{   "Sunix SUN1889",
	    {	0x0009,	0x7168,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
	    },
	},

	{   "Diva Serial [GSP] Multiport UART",
	    {	0x103c,	0x1048,	0x103c,	0x1282	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_UART, 0x10, 0x00, 0, PUC_FLAGS_MEMORY },
		{ PUC_PORT_TYPE_UART, 0x10, 0x10, 0, PUC_FLAGS_MEMORY },
		{ PUC_PORT_TYPE_UART, 0x10, 0x38, 0, PUC_FLAGS_MEMORY },
	    },
	},

	{   "Comtrol RocketPort 550/4 RJ45",
	    {	0x11fe,	0x8014,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 4 },
	    },
	},

	{   "Comtrol RocketPort 550/Quad",
	    {	0x11fe,	0x8015,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 4 },
	    },
	},

	{   "Comtrol RocketPort 550/8 RJ11 part A",
	    {	0x11fe,	0x8010,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 4 },
	    },
	},
	{   "Comtrol RocketPort 550/8 RJ11 part B",
	    {	0x11fe,	0x8011,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 4 },
	    },
	},

	{   "Comtrol RocketPort 550/8 Octa part A",
	    {	0x11fe,	0x8012,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 4 },
	    },
	},
	{   "Comtrol RocketPort 550/8 Octa part B",
	    {	0x11fe,	0x8013,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 4 },
	    },
	},

	{   "Comtrol RocketPort 550/8 part A",
	    {	0x11fe,	0x8018,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 4 },
	    },
	},
	{   "Comtrol RocketPort 550/8 part B",
	    {	0x11fe,	0x8019,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 4 },
	    },
	},

	{   "Comtrol RocketPort 550/16 part A",
	    {	0x11fe,	0x8016,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 4 },
	    },
	},
	{   "Comtrol RocketPort 550/16 part B",
	    {	0x11fe,	0x8017,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x20, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x28, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x30, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x38, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x40, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x48, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x50, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x58, COM_FREQ * 4 },
	    },
	},

	/*
	 * XXX no entry because I have no data:
	 * XXX Dolphin Peripherals 4006 (single parallel)
	 */

	/*
	 * Dolphin Peripherals 4014 (dual parallel port) card.  PLX 9050, with
	 * a seemingly-lame EEPROM setup that puts the Dolphin IDs
	 * into the subsystem fields, and claims that it's a
	 * network/misc (0x02/0x80) device.
	 */
	{   "Dolphin Peripherals 4014",
	    {	0x10b5,	0x9050,	0xd84d,	0x6810	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x24, 0x00, 0x00 },
	    },
	},

	/*
	 * XXX Dolphin Peripherals 4025 (single serial)
	 * (clashes with Dolphin Peripherals  4036 (2s variant)
	 */

	/*
	 * Dolphin Peripherals 4035 (dual serial port) card.  PLX 9050, with
	 * a seemingly-lame EEPROM setup that puts the Dolphin IDs
	 * into the subsystem fields, and claims that it's a
	 * network/misc (0x02/0x80) device.
	 */
	{   "Dolphin Peripherals 4035",
	    {	0x10b5,	0x9050,	0xd84d,	0x6808	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/*
	 * Dolphin Peripherals 4036 (dual serial port) card.
	 * (Dolpin 4025 has the same ID but only one port)
	 */
	{   "Dolphin Peripherals 4036",
	    {	0x1409,	0x7168,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
	    },
	},

	/*
	 * XXX no entry because I have no data:
	 * XXX Dolphin Peripherals 4078 (dual serial and single parallel)
	 */


	/*
	 * SIIG Boards.
	 *
	 * SIIG provides documentation for their boards at:
	 * <URL:http://www.siig.com/driver.htm>
	 *
	 * Please excuse the weird ordering, it's the order they
	 * use in their documentation.
	 */

	/*
	 * SIIG "10x" family boards.
	 */

	/* SIIG Cyber Serial PCI 16C550 (10x family): 1S */
	{   "SIIG Cyber Serial PCI 16C550 (10x family)",
	    {	0x131f,	0x1000,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial PCI 16C650 (10x family): 1S */
	{   "SIIG Cyber Serial PCI 16C650 (10x family)",
	    {	0x131f,	0x1001,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial PCI 16C850 (10x family): 1S */
	{   "SIIG Cyber Serial PCI 16C850 (10x family)",
	    {	0x131f,	0x1002,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber I/O PCI 16C550 (10x family): 1S, 1P */
	{   "SIIG Cyber I/O PCI 16C550 (10x family)",
	    {	0x131f,	0x1010,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C650 (10x family): 1S, 1P */
	{   "SIIG Cyber I/O PCI 16C650 (10x family)",
	    {	0x131f,	0x1011,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C850 (10x family): 1S, 1P */
	{   "SIIG Cyber I/O PCI 16C850 (10x family)",
	    {	0x131f,	0x1012,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber Parallel PCI (10x family): 1P */
	{   "SIIG Cyber Parallel PCI (10x family)",
	    {	0x131f,	0x1020,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber Parallel Dual PCI (10x family): 2P */
	{   "SIIG Cyber Parallel Dual PCI (10x family)",
	    {	0x131f,	0x1021,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C550 (10x family): 2S */
	{   "SIIG Cyber Serial Dual PCI 16C550 (10x family)",
	    {	0x131f,	0x1030,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C650 (10x family): 2S */
	{   "SIIG Cyber Serial Dual PCI 16C650 (10x family)",
	    {	0x131f,	0x1031,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C850 (10x family): 2S */
	{   "SIIG Cyber Serial Dual PCI 16C850 (10x family)",
	    {	0x131f,	0x1032,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C550 (10x family): 2S, 1P */
	{   "SIIG Cyber 2S1P PCI 16C550 (10x family)",
	    {	0x131f,	0x1034,	0,	0	},	/* XXX really? */
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C650 (10x family): 2S, 1P */
	{   "SIIG Cyber 2S1P PCI 16C650 (10x family)",
	    {	0x131f,	0x1035,	0,	0	},	/* XXX really? */
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C850 (10x family): 2S, 1P */
	{   "SIIG Cyber 2S1P PCI 16C850 (10x family)",
	    {	0x131f,	0x1036,	0,	0	},	/* XXX really? */
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x20, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C550 (10x family): 4S */
	{   "SIIG Cyber 4S PCI 16C550 (10x family)",
	    {	0x131f,	0x1050,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x24, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber 4S PCI 16C650 (10x family): 4S */
	{   "SIIG Cyber 4S PCI 16C650 (10x family)",
	    {	0x131f,	0x1051,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x24, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber 4S PCI 16C850 (10x family): 4S */
	{   "SIIG Cyber 4S PCI 16C850 (10x family)",
	    {	0x131f,	0x1052,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x24, 0x00, COM_FREQ },
	    },
	},

	/*
	 * SIIG "20x" family boards.
	 */

	/* SIIG Cyber Parallel PCI (20x family): 1P */
	{   "SIIG Cyber Parallel PCI (20x family)",
	    {	0x131f,	0x2020,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber Parallel Dual PCI (20x family): 2P */
	{   "SIIG Cyber Parallel Dual PCI (20x family)",
	    {	0x131f,	0x2021,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 2P1S PCI 16C550 (20x family): 1S, 2P */
	{   "SIIG Cyber 2P1S PCI 16C550 (20x family)",
	    {	0x131f,	0x2040,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 2P1S PCI 16C650 (20x family): 1S, 2P */
	{   "SIIG Cyber 2P1S PCI 16C650 (20x family)",
	    {	0x131f,	0x2041,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 2P1S PCI 16C850 (20x family): 1S, 2P */
	{   "SIIG Cyber 2P1S PCI 16C850 (20x family)",
	    {	0x131f,	0x2042,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00, 0x00 },
		{ PUC_PORT_TYPE_LPT, 0x1c, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber Serial PCI 16C550 (20x family): 1S */
	{   "SIIG Cyber Serial PCI 16C550 (20x family)",
	    {	0x131f,	0x2000,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial PCI 16C650 (20x family): 1S */
	{   "SIIG Cyber Serial PCI 16C650 (20x family)",
	    {	0x131f,	0x2001,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial PCI 16C850 (20x family): 1S */
	{   "SIIG Cyber Serial PCI 16C850 (20x family)",
	    {	0x131f,	0x2002,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber I/O PCI 16C550 (20x family): 1S, 1P */
	{   "SIIG Cyber I/O PCI 16C550 (20x family)",
	    {	0x131f,	0x2010,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C650 (20x family): 1S, 1P */
	{   "SIIG Cyber I/O PCI 16C650 (20x family)",
	    {	0x131f,	0x2011,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber I/O PCI 16C850 (20x family): 1S, 1P */
	{   "SIIG Cyber I/O PCI 16C850 (20x family)",
	    {	0x131f,	0x2012,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x14, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C550 (20x family): 2S */
	{   "SIIG Cyber Serial Dual PCI 16C550 (20x family)",
	    {	0x131f,	0x2030,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C650 (20x family): 2S */
	{   "SIIG Cyber Serial Dual PCI 16C650 (20x family)",
	    {	0x131f,	0x2031,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber Serial Dual PCI 16C850 (20x family): 2S */
	{   "SIIG Cyber Serial Dual PCI 16C850 (20x family)",
	    {	0x131f,	0x2032,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C550 (20x family): 2S, 1P */
	{   "SIIG Cyber 2S1P PCI 16C550 (20x family)",
	    {	0x131f,	0x2060,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C650 (20x family): 2S, 1P */
	{   "SIIG Cyber 2S1P PCI 16C650 (20x family)",
	    {	0x131f,	0x2061,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 2S1P PCI 16C850 (20x family): 2S, 1P */
	{   "SIIG Cyber 2S1P PCI 16C850 (20x family)",
	    {	0x131f,	0x2062,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00, 0x00 },
	    },
	},

	/* SIIG Cyber 4S PCI 16C550 (20x family): 4S */
	{   "SIIG Cyber 4S PCI 16C550 (20x family)",
	    {	0x131f,	0x2050,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber 4S PCI 16C650 (20x family): 4S */
	{   "SIIG Cyber 4S PCI 16C650 (20x family)",
	    {	0x131f,	0x2051,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* SIIG Cyber 4S PCI 16C850 (20x family): 4S */
	{   "SIIG Cyber 4S PCI 16C850 (20x family)",
	    {	0x131f,	0x2052,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* VScom PCI-200L: 2S */
	{   "VScom PCI-200L",
	    {	0x14d2,	0x8020,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
	    },
	},

	/* VScom PCI-400: 4S */
	{   "VScom PCI-400",
	    {	0x10b5,	0x1077,	0x10b5,	0x1077	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 8 },
	    },
	},

	/* VScom PCI-800: 8S */
	{   "VScom PCI-800",
	    {	0x10b5,	0x1076,	0x10b5,	0x1076	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x20, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x28, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x30, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x38, COM_FREQ * 8 },
	    },
	},
	/*
	 * VScom PCI-800H. Uses 8 16950 UART, behind a PCI chips that offers
	 * 4 com port on PCI device 0 and 4 on PCI device 1. PCI device 0 has
	 * device ID 3 and PCI device 1 device ID 4.
	 */
	{   "Titan PCI-800H",
	    {	0x14d2,	0xa003,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 8 },
	    },
	},
	{   "Titan PCI-800H",
	    {	0x14d2,	0xa004,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 8 },
	    },
	},
	{   "Titan PCI-200H",
	    {	0x14d2,	0xa005,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8 },
	    },
	},

	{   "Titan VScom PCI-200HV2",	/* 2S */
	    {	0x14d2,	0xe020,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8 },
	    },
	},
	/*
	 * VScom (Titan?) PCI-800L.  More modern variant of the
	 * PCI-800.  Uses 6 discrete 16550 UARTs, plus another
	 * two of them obviously implemented as macro cells in
	 * the ASIC.  This causes the weird port access pattern
	 * below, where two of the IO port ranges each access
	 * one of the ASIC UARTs, and a block of IO addresses
	 * access the external UARTs.
	 */
	{   "Titan VScom PCI-800L",
	    {	0x14d2,	0x8080,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x18, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x20, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x20, 0x28, COM_FREQ * 8 },
	    },
	},

	/*
	 * NEC PK-UG-X001 K56flex PCI Modem card.
	 * Uses NEC MARTH bridge chip and Rockwell RCVDL56ACF/SP.
	 */
	{   "NEC PK-UG-X001 K56flex PCI Modem",
	    {	0x1033,	0x0074,	0x1033,	0x8014	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* NEC PK-UG-X008 */
	{   "NEC PK-UG-X008",
	    {	0x1033,	0x007d,	0x1033,	0x8012	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers 2SP-PCI */
	{   "Lava Computers 2SP-PCI parallel port",
	    {	0x1407,	0x8000,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00, 0x00 },
	    },
	},

	/* Lava Computers 2SP-PCI and Quattro-PCI serial ports */
	{   "Lava Computers dual serial port",
	    {	0x1407,	0x0100,	0,	0	},
	    {	0xffff,	0xfffc,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers newer Quattro-PCI serial ports */
	{   "Lava Computers Quattro-PCI serial port",
	    {	0x1407,	0x0120,	0,	0	},
	    {	0xffff,	0xfffc,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers DSerial PCI serial ports */
	{   "Lava Computers serial port",
	    {	0x1407,	0x0110,	0,	0	},
	    {	0xffff,	0xfffc,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* Lava Computers Octopus-550 serial ports */
	{   "Lava Computers Octopus-550 8-port serial",
	    {	0x1407,	0x0180,	0,	0	},
	    {	0xffff,	0xfffc,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* US Robotics (3Com) PCI Modems */
	{   "US Robotics (3Com) 3CP5609 PCI 16550 Modem",
	    {	0x12b9,	0x1008,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* Actiontec 56K PCI Master */
	{   "Actiontec 56K PCI Master",
	    {	0x11c1,	0x0480,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/*
	 * Boards with an Oxford Semiconductor chip.
	 *
	 * Oxford Semiconductor provides documentation for their chip at:
	 * <URL:http://www.oxsemi.com/products/uarts/index.html>
	 *
	 * As sold by Kouwell <URL:http://www.kouwell.com/>.
	 * I/O Flex PCI I/O Card Model-223 with 4 serial and 1 parallel ports.
	 */

	/* Oxford Semiconductor OX12PCI840 PCI Parallel port */
	{   "Oxford Semiconductor OX12PCI840 Parallel port",
	    {	0x1415,	0x8403,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00, 0x00 },
	    },
	},

	/* Oxford Semiconductor OX16PCI954 PCI UARTs */
	{   "Oxford Semiconductor OX16PCI954 UARTs",
	    {	0x1415,	0x9501,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OX16PCI954 PCI UARTs */
	{   "Oxford Semiconductor OX16PCI954 UARTs",
	    {	0x1415,	0x950a,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OXCB950 PCI/CardBus UARTs */
	{   "Oxford Semiconductor OXCB950 UART",
	    {	0x1415,	0x950b,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		/* { PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ }, */
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, 16384000 },
	    },
	},

	/*
	 * Oxford Semiconductor OX9160/OX16PCI954 PCI UARTS
	 * Second chip on Exsys EX-41098 8x cards
	 */
	{   "Oxford Semiconductor OX9160/OX16PCI954 UARTs (function 1)",
	    {	0x1415,	0x9511,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ },
	    },
	},

	/* Oxford Semiconductor OX16PCI954 PCI Parallel port */
	{   "Oxford Semiconductor OX16PCI954 Parallel port",
	    {	0x1415,	0x9513,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00, 0x00 },
	    },
	},

	/* NetMos 2S1P PCI 16C650 : 2S, 1P */
	{   "NetMos NM9835 Dual UART and 1284 Printer port",
	    {	0x9710,	0x9835,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x18, 0x00, 0x00 },
	    },
	},

	/* NetMos 4S0P PCI: 4S, 0P */
	{   "NetMos NM9845 Quad UART",
	    {	0x9710,	0x9845,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* NetMos 0S1P PCI: 0S, 1P */
	{   "NetMos NM9805 1284 Printer port",
	    {   0x9710, 0x9805, 0,      0       },
	    {   0xffff, 0xffff, 0,      0       },
	    {
		{ PUC_PORT_TYPE_LPT, 0x10, 0x00, 0x00 },
	    },
	},                            

	/*
	 * This is the Middle Digital, Inc. PCI-Weasel, which
	 * uses a PCI interface implemented in FPGA.
	 */
	{   "Middle Digital, Inc. Weasel serial port",
	    {	0xdeaf,	0x9051,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
	    },
	},

	/* SD-LAB PCI I/O Card 4S2P */
	{   "Syba Tech Ltd. PCI-4S2P-550-ECP",
	    {	0x1592,	0x0781,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x2e8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x2f8, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x10, 0x000, 0x00 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x3e8, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x3f8, COM_FREQ },
		{ PUC_PORT_TYPE_LPT, 0x10, 0x000, 0x00 },
	    },
	    .init = puc_config_win877,
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 4S RS232 */
	{   "Moxa Technologies, Smartio C104H/PCI",
	    {	0x1393,	0x1040,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 4S RS232 */
	{   "Moxa Technologies, Smartio CP-104UL/PCI",
	    {	0x1393,	0x1041,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 4S RS232/422/485 */
	{   "Moxa Technologies, Industio CP-114",
	    {	0x1393,	0x1141,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 8S RS232 */
	{   "Moxa Technologies, C168H/PCI",
	    {	0x1393,	0x1680,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x20, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x28, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x30, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x38, COM_FREQ * 8 },
	    },
	},

	/* Moxa Technologies Co., Ltd. PCI I/O Card 8S RS232 */
	{   "Moxa Technologies, C168U/PCI",
	    {	0x1393,	0x1681,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x20, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x28, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x30, COM_FREQ * 8 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x38, COM_FREQ * 8 },
	    },
	},

	{   "Avlab Technology, PCI IO 2S",
	    {	0x14db,	0x2130,	0,	0	},
	    {	0xffff,	0xfffc,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
	    },
	},

	/* Avlab Technology, Inc. Low Profile PCI 4 Serial: 4S */
	{   "Avlab Low Profile PCI 4 Serial",
	    {	0x14db,	0x2150,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
	    },
	},

	/* Decision Computer Inc, serial ports */
	{   "Decision Computer Inc, PCCOM 4-port serial",
	    {	0x6666,	0x0001,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x18, COM_FREQ },
	    },
	},

	{   "PCCOM dual port RS232/422/485",
	    {	0x6666,	0x0004,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x1c, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x1c, 0x08, COM_FREQ },
	    },
	},

	{   "IC Book Labs Gunboat x2 Low Profile",
	    {   0xb00c, 0x0a1c, 0,      0       },
	    {   0xffff, 0xffff, 0,      0       },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ },
	    },
	},

	{   "IC Book Labs Gunboat x4 Lite",
	    {   0xb00c, 0x021c, 0,      0       },
	    {   0xffff, 0xffff, 0,      0       },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ },
	    },
	    PUC_ILR_TYPE_DIGI, { 0x07 },
	},

	{   "IC Book Labs Gunboat x4 Pro",
	    {   0xb00c, 0x031c, 0,      0       },
	    {   0xffff, 0xffff, 0,      0       },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ },
	    },
	    PUC_ILR_TYPE_DIGI, { 0x07 },
	},

	{   "IC Book Labs Gunboat x4 Low Profile",
	    {   0xb00c, 0x0b1c, 0,      0       },
	    {   0xffff, 0xffff, 0,      0       },
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ },
	    },
	    PUC_ILR_TYPE_DIGI, { 0x07 },
	},

	{   "IC Book Labs Ironclad x8 Lite",
	    {	0xb00c,	0x041c,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x20, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x28, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x30, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x38, COM_FREQ },
	    },
	    PUC_ILR_TYPE_DIGI, { 0x07 },
	},

	{   "IC Book Labs Ironclad x8 Pro",
	    {	0xb00c,	0x051c,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x20, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x28, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x30, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x38, COM_FREQ },
	    },
	    PUC_ILR_TYPE_DIGI, { 0x07 },
	},

	{   "IC Book Labs Dreadnought x16 Lite",
	    {	0xb00c,	0x091c,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x20, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x28, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x30, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x38, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x40, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x48, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x50, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x58, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x60, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x68, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x70, COM_FREQ },
		{ PUC_PORT_TYPE_COM, 0x10, 0x78, COM_FREQ },
	    },
	    PUC_ILR_TYPE_DIGI, { 0x07, 0x47 },
	},

	{   "IC Book Labs Dreadnought x16 Pro",
	    {	0xb00c,	0x081c,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x10, 0x00, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x08, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x10, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x18, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x20, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x28, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x30, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x38, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x40, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x48, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x50, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x58, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x60, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x68, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x70, COM_FREQ * 8, 0x200000 },
		{ PUC_PORT_TYPE_COM, 0x10, 0x78, COM_FREQ * 8, 0x200000 },
	    },
	    PUC_ILR_TYPE_DIGI, { 0x07, 0x47 },
	},

	{   "Cronyx Omega2-PCI",
	    {	0x13a8,	0x0158,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_UART, 0x010, 0x000, COM_FREQ * 8, PUC_FLAGS_MEMORY },
		{ PUC_PORT_TYPE_UART, 0x010, 0x200, COM_FREQ * 8, PUC_FLAGS_MEMORY },
		{ PUC_PORT_TYPE_UART, 0x010, 0x400, COM_FREQ * 8, PUC_FLAGS_MEMORY },
		{ PUC_PORT_TYPE_UART, 0x010, 0x600, COM_FREQ * 8, PUC_FLAGS_MEMORY },
		{ PUC_PORT_TYPE_UART, 0x010, 0x800, COM_FREQ * 8, PUC_FLAGS_MEMORY },
		{ PUC_PORT_TYPE_UART, 0x010, 0xA00, COM_FREQ * 8, PUC_FLAGS_MEMORY },
		{ PUC_PORT_TYPE_UART, 0x010, 0xC00, COM_FREQ * 8, PUC_FLAGS_MEMORY },
		{ PUC_PORT_TYPE_UART, 0x010, 0xE00, COM_FREQ * 8, PUC_FLAGS_MEMORY },
	    },
	},

	/*
	 * Boca Research Turbo Serial 654 (4 serial port) card.
	 * Appears to be the same as Chase Research PLC PCI-FAST4
	 * and Perle PCI-FAST4 Multi-Port serial cards.
	 */
	{   "Boca Research Turbo Serial 654",
	{   0x10b5, 0x9050, 0x12e0, 0x0031  },
	{   0xffff, 0xffff, 0xffff, 0xffff  },
	{
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 4 },
		},
	},

	/*
	 * Boca Research Turbo Serial 658 (8 serial port) card.
	 * Appears to be the same as Chase Research PLC PCI-FAST8
	 * and Perle PCI-FAST8 Multi-Port serial cards.
	 */
	{   "Boca Research Turbo Serial 658",
	{   0x10b5, 0x9050, 0x12e0, 0x0021  },
	{   0xffff, 0xffff, 0xffff, 0xffff  },
	{
		{ PUC_PORT_TYPE_COM, 0x18, 0x00, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x08, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x10, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x18, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x20, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x28, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x30, COM_FREQ * 4 },
		{ PUC_PORT_TYPE_COM, 0x18, 0x38, COM_FREQ * 4 },
		},
	},

	{   "Dell RAC III Virtual UART",
	    {	0x1028,	0x0008,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, COM_FREQ * 128 },
	    },
	},

	{   "Dell RAC IV/ERA Virtual UART",
	    {	0x1028,	0x0012,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x14, 0x00, DEFAULT_RCLK * 128 },
	    },
	},
	{	/* "VScom 200Li" 00=14D2 02=8028 uart@20 uart@+8 */
	    "VScom 200Li",
	    {	0x14d2,	0x8028,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x20, 0x00, DEFAULT_RCLK },
		{ PUC_PORT_TYPE_COM, 0x20, 0x08, DEFAULT_RCLK },
	    },
	},

	{ 0 }
};
