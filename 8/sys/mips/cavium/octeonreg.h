/*	$NetBSD: octeonreg.h,v 1.1 2002/03/07 14:44:04 simonb Exp $	*/

/*
 * Copyright 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Simon Burge for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
	Memory Map

	0000.0000 *	128MB	Typically SDRAM (on Core Board)
	0800.0000 *	256MB	Typically PCI
	1800.0000 *	 62MB	Typically PCI
	1be0.0000 *	  2MB	Typically System controller's internal registers
	1c00.0000 *	 32MB	Typically not used
	1e00.0000	  4MB	Monitor Flash
	1e40.0000	 12MB	reserved
	1f00.0000	 12MB	Switches
				LEDs
				ASCII display
				Soft reset
				FPGA revision number
				CBUS UART (tty2)
				General Purpose I/O
				I2C controller
	1f10.0000 *	 11MB	Typically System Controller specific
	1fc0.0000	  4MB	Maps to Monitor Flash
	1fd0.0000 *	  3MB	Typically System Controller specific

		  * depends on implementation of the Core Board and of software
 */

/*
	CPU interrupts

		NMI	South Bridge or NMI button
		 0	South Bridge INTR
		 1	South Bridge SMI
		 2	CBUS UART (tty2)
		 3	COREHI (Core Card)
		 4	CORELO (Core Card)
		 5	Not used, driven inactive (typically CPU internal timer interrupt

	IRQ mapping (as used by YAMON)

		0	Timer		South Bridge
		1	Keyboard	SuperIO
		2			Reserved by South Bridge (for cascading)
		3	UART (tty1)	SuperIO
		4	UART (tty0)	SuperIO
		5			Not used
		6	Floppy Disk	SuperIO
		7	Parallel Port	SuperIO
		8	Real Time Clock	South Bridge
		9	I2C bus		South Bridge
		10	PCI A,B,eth	PCI slot 1..4, Ethernet
		11	PCI C,audio	PCI slot 1..4, Audio, USB (South Bridge)
			PCI D,USB
		12	Mouse		SuperIO
		13			Reserved by South Bridge
		14	Primary IDE	Primary IDE slot
		15	Secondary IDE	Secondary IDE slot/Compact flash connector
 */

#define	OCTEON_SYSTEMRAM_BASE	0x00000000  /* System RAM:	*/
#define	OCTEON_SYSTEMRAM_SIZE	0x08000000  /*   128 MByte	*/

#define	OCTEON_PCIMEM1_BASE	0x08000000  /* PCI 1 memory:	*/
#define	OCTEON_PCIMEM1_SIZE	0x08000000  /*   128 MByte	*/

#define	OCTEON_PCIMEM2_BASE	0x10000000  /* PCI 2 memory:	*/
#define	OCTEON_PCIMEM2_SIZE	0x08000000  /*   128 MByte	*/

#define	OCTEON_PCIMEM3_BASE	0x18000000  /* PCI 3 memory	*/
#define	OCTEON_PCIMEM3_SIZE	0x03e00000  /*    62 MByte	*/

#define	OCTEON_CORECTRL_BASE	0x1be00000  /* Core control:	*/
#define	OCTEON_CORECTRL_SIZE	0x00200000  /*     2 MByte	*/

#define	OCTEON_RESERVED_BASE1	0x1c000000  /* Reserved:	*/
#define	OCTEON_RESERVED_SIZE1	0x02000000  /*    32 MByte	*/

#define	OCTEON_MONITORFLASH_BASE	0x1e000000  /* Monitor Flash:	*/
#define	OCTEON_MONITORFLASH_SIZE	0x003e0000  /*     4 MByte	*/
#define	OCTEON_MONITORFLASH_SECTORSIZE 0x00010000 /* Sect. = 64 KB */

#define	OCTEON_FILEFLASH_BASE	0x1e3e0000 /* File Flash (for monitor): */
#define	OCTEON_FILEFLASH_SIZE	0x00020000 /*   128 KByte	*/

#define	OCTEON_FILEFLASH_SECTORSIZE 0x00010000 /* Sect. = 64 KB	*/

#define	OCTEON_RESERVED_BASE2	0x1e400000  /* Reserved:	*/
#define	OCTEON_RESERVED_SIZE2	0x00c00000  /*    12 MByte	*/

#define	OCTEON_FPGA_BASE		0x1f000000  /* FPGA:		*/
#define	OCTEON_FPGA_SIZE		0x00c00000  /*    12 MByte	*/

#define	OCTEON_NMISTATUS		(OCTEON_FPGA_BASE + 0x24)
#define	 OCTEON_NMI_SB		 0x2	/* Pending NMI from the South Bridge */
#define	 OCTEON_NMI_ONNMI	 0x1	/* Pending NMI from the ON/NMI push button */

#define	OCTEON_NMIACK		(OCTEON_FPGA_BASE + 0x104)
#define	 OCTEON_NMIACK_ONNMI	 0x1	/* Write 1 to acknowledge ON/NMI */

#define	OCTEON_SWITCH		(OCTEON_FPGA_BASE + 0x200)
#define	 OCTEON_SWITCH_MASK	 0xff	/* settings of DIP switch S2 */

#define	OCTEON_STATUS		(OCTEON_FPGA_BASE + 0x208)
#define	 OCTEON_ST_MFWR		 0x10	/* Monitor Flash is write protected (JP1) */
#define	 OCTEON_S54		 0x08	/* switch S5-4 - set YAMON factory default mode */
#define	 OCTEON_S53		 0x04	/* switch S5-3 */
#define	 OCTEON_BIGEND		 0x02	/* switch S5-2 - big endian mode */

#define	OCTEON_JMPRS		(OCTEON_FPGA_BASE + 0x210)
#define	 OCTEON_JMPRS_PCICLK	 0x1c	/* PCI clock frequency */
#define	 OCTEON_JMPRS_EELOCK	 0x02	/* I2C EEPROM is write protected */

#define	OCTEON_LEDBAR		(OCTEON_FPGA_BASE + 0x408)
#define	OCTEON_ASCIIWORD		(OCTEON_FPGA_BASE + 0x410)
#define	OCTEON_ASCII_BASE	(OCTEON_FPGA_BASE + 0x418)
#define	OCTEON_ASCIIPOS0		0x00
#define	OCTEON_ASCIIPOS1		0x08
#define	OCTEON_ASCIIPOS2		0x10
#define	OCTEON_ASCIIPOS3		0x18
#define	OCTEON_ASCIIPOS4		0x20
#define	OCTEON_ASCIIPOS5		0x28
#define	OCTEON_ASCIIPOS6		0x30
#define	OCTEON_ASCIIPOS7		0x38

#define	OCTEON_SOFTRES		(OCTEON_FPGA_BASE + 0x500)
#define	 OCTEON_GORESET		 0x42	/* write this to OCTEON_SOFTRES for board reset */

/*
 * BRKRES is the number of milliseconds before a "break" on tty will
 * trigger a reset.  A value of 0 will disable the reset.
 */
#define	OCTEON_BRKRES		(OCTEON_FPGA_BASE + 0x508)
#define	 OCTEON_BRKRES_MASK	 0xff

#define	OCTEON_CBUSUART		0x8001180000000800ull
/* 16C550C UART, 8 bit registers on 8 byte boundaries */
/* RXTX    0x00 */
/* INTEN   0x08 */
/* IIFIFO  0x10 */
/* LCTRL   0x18 */
/* MCTRL   0x20 */
/* LSTAT   0x28 */
/* MSTAT   0x30 */
/* SCRATCH 0x38 */
#define	OCTEON_CBUSUART_INTR	2

#define	OCTEON_GPIO_BASE		(OCTEON_FPGA_BASE + 0xa00)
#define	OCTEON_GPOUT		0x0
#define	OCTEON_GPINP		0x8

#define	OCTEON_BOOTROM_BASE	0x1fc00000  /* Boot ROM:	*/
#define	OCTEON_BOOTROM_SIZE	0x00400000  /*     4 MByte	*/

#define	 OCTEON_REVISION	 0x1fc00010
#define	 OCTEON_REV_FPGRV	 0xff0000	/* CBUS FPGA revision */
#define	 OCTEON_REV_CORID	 0x00fc00	/* Core Board ID */
#define	 OCTEON_REV_CORRV	 0x000300	/* Core Board Revision */
#define	 OCTEON_REV_PROID	 0x0000f0	/* Product ID */
#define	 OCTEON_REV_PRORV	 0x00000f	/* Product Revision */

/* PCI definitions */

#define OCTEON_UART0ADR			0x8001180000000800ull
#define OCTEON_UART1ADR         	0x8001180000000C00ull
#define OCTEON_UART_SIZE		0x400

#define OCTEON_MIO_BOOT_BIST_STAT	0x80011800000000F8ull



/**************************
 * To Delete
 */
#define	OCTEON_SOUTHBRIDGE_INTR	   0

#define OCTEON_PCI0_IO_BASE         OCTEON_PCIMEM3_BASE
#define OCTEON_PCI0_ADDR( addr )    (OCTEON_PCI0_IO_BASE + (addr))

#define OCTEON_RTCADR               0x70 // OCTEON_PCI_IO_ADDR8(0x70)
#define OCTEON_RTCDAT               0x71 // OCTEON_PCI_IO_ADDR8(0x71)

#define OCTEON_SMSC_COM1_ADR        0x3f8
#define OCTEON_SMSC_COM2_ADR        0x2f8
#define OCTEON_UARTT0ADR             OCTEON_PCI0_ADDR(OCTEON_SMSC_COM1_ADR)
#define OCTEON_UARTT1ADR             OCTEON_SMSC_COM2_ADR // OCTEON_PCI0_ADDR(OCTEON_SMSC_COM2_ADR)

#define OCTEON_SMSC_1284_ADR        0x378
#define OCTEON_1284ADR              OCTEON_SMSC_1284_ADR // OCTEON_PCI0_ADDR(OCTEON_SMSC_1284_ADR)

#define OCTEON_SMSC_FDD_ADR         0x3f0
#define OCTEON_FDDADR               OCTEON_SMSC_FDD_ADR // OCTEON_PCI0_ADDR(OCTEON_SMSC_FDD_ADR)

#define OCTEON_SMSC_KYBD_ADR        0x60  /* Fixed 0x60, 0x64 */
#define OCTEON_KYBDADR              OCTEON_SMSC_KYBD_ADR // OCTEON_PCI0_ADDR(OCTEON_SMSC_KYBD_ADR)
#define OCTEON_SMSC_MOUSE_ADR       OCTEON_SMSC_KYBD_ADR
#define OCTEON_MOUSEADR             OCTEON_KYBDADR


#define	OCTEON_DMA_PCI_PCIBASE	0x00000000UL
#define	OCTEON_DMA_PCI_PHYSBASE	0x00000000UL
#define	OCTEON_DMA_PCI_SIZE	(256 * 1024 * 1024)

#define	OCTEON_DMA_ISA_PCIBASE	0x00800000UL
#define	OCTEON_DMA_ISA_PHYSBASE	0x00000000UL
#define	OCTEON_DMA_ISA_SIZE	(8 * 1024 * 1024)

#ifndef _LOCORE
void	led_bar(uint8_t);
void	led_display_word(uint32_t);
void	led_display_str(const char *);
void	led_display_char(int, uint8_t);
#endif
