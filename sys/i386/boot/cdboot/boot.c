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
 *	from: Mach, [92/04/03  16:51:14  rvb]
 * $FreeBSD: src/sys/i386/boot/cdboot/boot.c,v 1.3 1999/08/28 00:43:17 peter Exp $
 */


/*
  Copyright 1988, 1989, 1990, 1991, 1992
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 * Extensions for El Torito CD-ROM booting:
 *
 * Copyright © 1997 Pluto Technologies International, Inc.  Boulder CO
 * Copyright © 1997 interface business GmbH, Dresden.
 *	All rights reserved.
 *
 * This code was written by Jörg Wunsch, Dresden.
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
#include "boot.h"
#include <a.out.h>
#include <sys/reboot.h>
#include <machine/bootinfo.h>
#ifdef PROBE_KEYBOARD_LOCK
#include <machine/cpufunc.h>
#endif

#define	ouraddr	(BOOTSEG << 4)		/* XXX */

int loadflags;

/*
 * XXX
 * By now, only "cd".  How do we learn from the BIOS we've been booted off
 * an ATAPI CD-ROM?  Do the non-{cd,wcd} drivers implement El Torito booting
 * at all?
 */
static int maj = 6;
static struct specpacket spkt = { 0x13 };
static char *name;
static char namebuf[128];
static struct bootinfo bootinfo;

static void getbootdev(char *ptr, int *howto);
static void loadprog(void);

/* NORETURN */
void
boot(int drive)
{
	int ret, i;

#ifdef PROBE_KEYBOARD
	if (probe_keyboard()) {
		init_serial();
		loadflags |= RB_SERIAL;
		printf("\nNo keyboard found.");
	}
#endif

#ifdef PROBE_KEYBOARD_LOCK
	if (!(inb(0x64) & 0x10)) {
		init_serial();
		loadflags |= RB_SERIAL;
		printf("\nKeyboard locked.");
	}
#endif

#ifdef FORCE_COMCONSOLE
	init_serial();
	loadflags |= RB_SERIAL;
	printf("\nSerial console forced.");
#endif

	/* Pick up the story from the Bios on geometry of disks */

	/*
	 * XXX
	 * Do we need to defer this until we can relinguish the
	 * BIOS emulation?
	 */

	for(ret = 0; ret < N_BIOS_GEOM; ret ++)
		bootinfo.bi_bios_geom[ret] = get_diskinfo(ret + 0x80);

	bootinfo.bi_basemem = memsize(0);
	bootinfo.bi_extmem = memsize(1);
	bootinfo.bi_memsizes_valid = 1;

	gateA20();

	ret = getbootspec(&spkt);
	if (ret != 0) {
		printf("Your BIOS int 0x13 extensions seem to be disabled.\n"
		       "It's impossible to boot a CD-ROM without them.\n"
		       "(BIOS int 0x13 fn 0x4b01 yielded error %d)\n",
		       ret);
		while (1)
			;
	}

	if (devopen(sessionstart) == -1)
		printf("Warning: cannot open default session.\n"
		       "Maybe your BIOS int 0x13 extensions are disabled?\n"
		       "You need them in order to boot a CD-ROM.\n");

	for (;;) {
		
		/*
		 * The El Torito specification stinks.  Not only this
		 * crappy idea of `emulation booting' (and at least
		 * earlier versions of the AHA-2940 BIOS didn't
		 * implement anything else than floppy emulation
		 * booting), but note also that there's absolutely no
		 * way via the BIOS to obtain the starting LBA of your
		 * session.  All you can get ahold of is the LBA of
		 * that funny emulated disk.  Since this one just
		 * happens to be a file hidden inside the ISO9660
		 * filesystem, it is located at a varying offset from
		 * the start of the session.  We therefore allow to
		 * specify the starting block of the session to use in
		 * the boot string, so the operator can specify the
		 * session to boot from.  However, (s)he needs to know
		 * the RBA for the session from the CD-ROM TOC.
		 */
		DPRINTF(("using session at sector %d\n", sessionstart));

		name = "/kernel";
		printf("\n>> FreeBSD CD-ROM BOOT\n"
		       "Usage: [@%d]%s[-abcCdghrsv]\n"
		       "Use ? for file list or press Enter for defaults\n"
		       "\nBoot: ",
		       sessionstart, name);

		loadflags &= RB_SERIAL;	/* clear all, but leave serial console */
		loadflags |= RB_CDROM;	/* ...and default to CD-ROM root. */

		getbootdev(namebuf, &loadflags);

		DPRINTF(("Selected: name=`%s', loadflags=0x%x\n",
			 name, loadflags));

		ret = openrd(name);

		DPRINTF(("openrd() = %d\n", ret));

		if (ret != 0) {
			if (ret > 0)
				printf("Can't find %s\n", name);
			continue;
		}
		loadprog();
	}
}

static void
loadprog(void)
{
	struct exec head;
	u_int32_t startaddr, addr, bootdev;
	int i;
	unsigned pad;

	seek(0);
	if (read((void *)&head, sizeof(head)) == -1 ||
	    N_BADMAG(head)) {
		printf("Invalid format!\n");
		return;
	}

	/*
	 * We assume that the entry address is the same as the lowest text
	 * address and that the kernel startup code handles relocation by
	 * this address rounded down to a multiple of 16M.
	 */
	startaddr = head.a_entry & 0x00FFFFFF;
	addr =  startaddr;
	printf("Booting CD-ROM [@%d]%s @ 0x%x\n", sessionstart, name, addr);
	if(addr < 0x00100000)
	{
		/*
		 * Bail out, instead of risking to damage the BIOS
		 * variables, the loader, or the adapter memory area.
		 * We don't support loading below 1 MB any more.
		 */
		printf("Start address too low\n");
		return;
	}
	printf("text=0x%x ", head.a_text);

	/* load the text segment */
	seek(N_TXTOFF(head));
	if (xread((void *)addr, head.a_text) == -1)
		return;
	addr += head.a_text;

	/* Pad to a page boundary. */
	pad = (unsigned)addr & PAGE_MASK;
	if (pad != 0) {
		pad = PAGE_SIZE - pad;
		pbzero((void *)addr, pad);
		addr += pad;
	}

	/* load the initialised data after the text */
	printf("data=0x%x ", head.a_data);
	if (xread((void *)addr, head.a_data) == -1)
		return;
	addr += head.a_data;

	/* Skip over the uninitialised data (but clear it) */
	printf("bss=0x%x ", head.a_bss);

/*
 * XXX however, we should be checking that we don't load ... into
 * nonexistent memory.  A full symbol table is unlikely to fit on 4MB
 * machines.
 */
	pbzero((void *)addr, head.a_bss);
	addr += head.a_bss;

	/* Pad to a page boundary. */
	pad = (unsigned)addr & PAGE_MASK;
	if (pad != 0) {
		pad = PAGE_SIZE - pad;
		addr += pad;
	}
	bootinfo.bi_symtab = addr;

	/* Copy the symbol table size */
	pcpy(&head.a_syms, (void *)addr, sizeof(head.a_syms));
	addr += sizeof(head.a_syms);

	/* Load the symbol table */
	printf("symbols=[+0x%x+0x%x+0x%x", pad, sizeof(head.a_syms),
	       head.a_syms);
	if (xread((void *)addr, head.a_syms) == -1)
		return;
	addr += head.a_syms;

	/* Load the string table size */
	if (read((void *)&i, sizeof(int)) == -1)
		return;
	pcpy(&i, (void *)addr, sizeof(int));
	i -= sizeof(int);
	addr += sizeof(int);

	/* Load the string table */
	printf("+0x%x+0x%x]\n", sizeof(int), i);
	if (xread((void *)addr, i) == -1)
		return;
	addr += i;

	bootinfo.bi_esymtab = addr;

	/* XXX what else can we say about a CD-ROM? */
	bootdev = MAKEBOOTDEV(maj, 0, 0, 0, 0);

	bootinfo.bi_version = BOOTINFO_VERSION;
	bootinfo.bi_kernelname = (u_int32_t)(name + ouraddr);
	bootinfo.bi_nfs_diskless = 0;
	bootinfo.bi_size = sizeof(bootinfo);
	printf("total=0x%x entry point=0x%x\n", (int)addr, (int)startaddr);
	startprog((int)startaddr, loadflags | RB_BOOTINFO, bootdev,
		  (int)&bootinfo + ouraddr);
}

static void
getbootdev(char *ptr, int *howto)
{
	char c;

	/*
	 * Be paranoid and make doubly sure that the input buffer is empty.
	 */
	if (*howto & RB_SERIAL)
		init_serial();

	if (!gets(ptr)) {
		putchar('\n');
		return;
	}
	while ((c = *ptr) != '\0') {
nextarg:
		while (c == ' ')
			c = *++ptr;
		if (c == '-')
			while ((c = *++ptr) != '\0') {
				if (c == ' ')
					goto nextarg;
				if (c == 'C')
					*howto &= ~RB_CDROM;
				if (c == 'a')
					*howto |= RB_ASKNAME;
				if (c == 'b')
					*howto |= RB_HALT;
				if (c == 'c')
					*howto |= RB_CONFIG;
				if (c == 'd')
					*howto |= RB_KDB;
				if (c == 'h') {
					*howto ^= RB_SERIAL;
					if (*howto & RB_SERIAL)
						init_serial();
					continue;
				}
				if (c == 'g')
					*howto |= RB_GDB;
				if (c == 'r')
					*howto |= RB_DFLTROOT;
				if (c == 's')
					*howto |= RB_SINGLE;
				if (c == 'v')
					*howto |= RB_VERBOSE;
			}
		if (c == '\0')
			return;
		name = ptr;
		while (*++ptr != '\0') {
			if (*ptr == ' ') {
				*ptr++ = '\0';
				break;
			}
		}
	}
}
