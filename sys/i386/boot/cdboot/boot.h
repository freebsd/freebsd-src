/*
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	from: Mach, Revision 2.2  92/04/04  11:35:03  rpd
 * $FreeBSD: src/sys/i386/boot/cdboot/boot.h,v 1.3 1999/08/28 00:43:17 peter Exp $
 */
/*
 * Extensions for El Torito CD-ROM booting:
 *
 * Copyright © 1997 Pluto Technologies International, Inc.  Boulder CO
 * Copyright © 1997 interface business GmbH, Dresden.
 *      All rights reserved.
 *
 * This code has been written by Jörg Wunsch, Dresden.
 * Direct comments to <joerg_wunsch@interface-business.de>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/time.h>

/*
 * Specification packet per El Torito, BIOS int 0x13 fn 0x4b00/0x4b01
 */
struct specpacket
{
	u_char size;		/* must be 0x13 */
	u_char mediatype;	/*
				 * 0 - no emulation
				 * 1 - 1.2 MB floppy
				 * 2 - 1.44 MB floppy
				 * 3 - 2.88 MB floppy
				 * 4 - hard disk C:
				 */
	u_char drvno;		/* emulated drive number */
	u_char ctrlindx;	/* controller index, see El Torito */
	u_int32_t lba;		/* LBA of emulated disk drive */
	u_int16_t devspec;	/* device specification, see El Torito */
	u_int16_t ubufseg;	/* user buffer segment */
	u_int16_t loadseg;	/* load segment; 0 => use BIOS default 0x7c0 */
	u_int16_t seccnt;	/* number of auto-loaded (virtual) sectors */
	u_char cyls;		/* same values as in int 0x13, fn 8 */
	u_char secs;
	u_char heads;
};

/*
 * Disk address packet for extended BIOS int 0x13 fn's 0x41...0x48.
 */
struct daddrpacket
{
	u_char size;		/* size of daddrpacket, must be 0x10 */
	u_char reserved1;
	u_char nblocks;		/*
				 * number of 512-byte blocks to transfer,
				 * must be <= 127
				 */
	u_char reserved2;
	u_int16_t boffs;	/* bseg:boffs denominate the transfer buffer */
	u_int16_t bseg;
	u_int32_t lba;		/* actually a 64-bit type, but 64-bit arith */
	u_int32_t lbahigh;	/* is expensive, and we don't really need it */
};

#ifdef DEBUG
# define DPRINTF(x) printf x
#else
# define DPRINTF(x)
#endif


/* asm.S */
#if ASM_ONLY
void real_to_prot(void);
void prot_to_real(void);
#endif
void startprog(unsigned int physaddr, int howto, int bootdev,
	       /* XXX struct bootinfo * */ unsigned int bootinfo);
void pbzero(void *dst, size_t count);
void pcpy(const void *src, void *dst, size_t count);

/* bios.S */

int biosread(int dev, int cyl, int head, int sec, int nsec, void *offset);
int getbootspec(struct specpacket *offset);
int biosreadlba(struct daddrpacket *daddr);
void putc(int c);
int getc(void);
int ischar(void);
int get_diskinfo(int drive);
int memsize(int extended);

/* boot.c */
extern int loadflags;

void boot(int drive);

/* boot2.S */
void boot2(void);

/* cdrom.c */
extern u_int32_t sessionstart;

int devopen(u_int32_t session);
void seek(u_int32_t offs);
int read(u_char *addr, size_t size);
int xread(u_char *addr, size_t size);
int openrd(char *name);

/* io.c */
void gateA20(void);
void printf(const char *format, ...);
void putchar(int c);
void delay1ms(void);
int gets(char *buf);
int strcasecmp(const char *s1, const char *s2);
int strcmp(const char *s1, const char *s2);
void bcopy(const void *from, void *to, size_t len);
void twiddle(void);

/* probe_keyboard.c */
int probe_keyboard(void);

/* serial.S */
void serial_putc(int ch);
int serial_getc(void);
int serial_ischar(void);
void init_serial(void);

/* table.c */
extern char *devs[];
extern unsigned long tw_chars;

/* malloc.c */
void *malloc(size_t size);
void free(void *chunk);

/* linker stuff */
extern void end;
