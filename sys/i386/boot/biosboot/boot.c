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
 *	$Id: boot.c,v 1.47 1996/03/08 06:29:06 bde Exp $
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

#include <sys/param.h>
#include "boot.h"
#include <a.out.h>
#include <sys/reboot.h>
#include <machine/bootinfo.h>

#define	ouraddr	(BOOTSEG << 4)		/* XXX */

#define NAMEBUF_LEN	(8*1024)

char namebuf[NAMEBUF_LEN];
struct exec head;
struct bootinfo bootinfo;
int loadflags;

static void getbootdev(char *ptr, int *howto);
static void loadprog(void);

/* NORETURN */
void
boot(int drive)
{
	int ret;

#ifdef PROBE_KEYBOARD
	if (probe_keyboard()) {
		init_serial();
		loadflags |= RB_SERIAL;
		printf("\nNo keyboard found.");
	}
#endif

#ifdef FORCE_COMCONSOLE
	init_serial();
	loadflags |= RB_SERIAL;
	printf("\nSerial console forced.");
#endif

	/* Pick up the story from the Bios on geometry of disks */

	for(ret = 0; ret < N_BIOS_GEOM; ret ++)
		bootinfo.bi_bios_geom[ret] = get_diskinfo(ret + 0x80);

	bootinfo.bi_basemem = memsize(0);
	bootinfo.bi_extmem = memsize(1);
	bootinfo.bi_memsizes_valid = 1;

	gateA20();

	/*
	 * The default boot device is the first partition in the
	 * compatibility slice on the boot drive.
	 */
	dosdev = drive;
	maj = 2;
	unit = drive & 0x7f;
#ifdef dontneed
	slice = 0;
	part = 0;
#endif
	if (drive & 0x80) {
		/*
		 * Hard drive.  Adjust.  Guess that the FreeBSD unit number
		 * is the BIOS drive number biased by BOOT_HD_BIAS,
		 */
		maj = 0;
#if BOOT_HD_BIAS > 0
		if (BOOT_HD_BIAS <= unit)
			unit -= BOOT_HD_BIAS;
#endif
	}

loadstart:
	/* print this all each time.. (saves space to do so) */
	/* If we have looped, use the previous entries as defaults */
	printf("\n>> FreeBSD BOOT @ 0x%x: %d/%d k of memory\n"
	       "Usage: [[[%d:][%s](%d,a)]%s][-abcCdhrsv]\n"
	       "Use 1:sd(0,a)kernel to boot sd0 if it is BIOS drive 1\n"
	       "Use ? for file list or press Enter for defaults\n\nBoot: ",
	       ouraddr, bootinfo.bi_basemem, bootinfo.bi_extmem,
	       dosdev & 0x7f, devs[maj], unit, name);

	name = dflname;		/* re-initialize in case of loop */
	loadflags &= RB_SERIAL;	/* clear all, but leave serial console */
	getbootdev(namebuf, &loadflags);
	ret = openrd();
	if (ret != 0) {
		if (ret > 0)
			printf("Can't find %s\n", name);
		goto loadstart;
	}
/*	if (inode.i_mode&IEXEC)
		loadflags |= RB_KDB;
*/
	loadprog();
	goto loadstart;
}

static void
loadprog(void)
{
	long int startaddr;
	long int addr;	/* physical address.. not directly useable */
	long int bootdev;
	int i;
	unsigned pad;

	read((void *)&head, sizeof(head));
	if ( N_BADMAG(head)) {
		printf("Invalid format!\n");
		return;
	}

	poff = N_TXTOFF(head);
	/*if(poff==0)
		poff = 32;*/

	/*
	 * We assume that the entry address is the same as the lowest text
	 * address and that the kernel startup code handles relocation by
	 * this address rounded down to a multiple of 16M.
	 */
	startaddr = head.a_entry & 0x00FFFFFF;
	addr =  startaddr;
	printf("Booting %d:%s(%d,%c)%s @ 0x%x\n"
			, dosdev & 0x7f
			, devs[maj]
			, unit
			, 'a'+part
			, name
			, addr);
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
	/********************************************************/
	/* LOAD THE TEXT SEGMENT				*/
	/********************************************************/
	xread((void *)addr, head.a_text);
	addr += head.a_text;

	/********************************************************/
	/* Load the Initialised data after the text		*/
	/********************************************************/
	while (addr & CLOFSET)
                *(char *)addr++ = 0;

	printf("data=0x%x ", head.a_data);
	xread((void *)addr, head.a_data);
	addr += head.a_data;

	/********************************************************/
	/* Skip over the uninitialised data			*/
	/* (but clear it)					*/
	/********************************************************/
	printf("bss=0x%x ", head.a_bss);

/*
 * XXX however, we should be checking that we don't load ... into
 * nonexistent memory.  A full symbol table is unlikely to fit on 4MB
 * machines.
 */
	pbzero((void *)addr,head.a_bss);
	addr += head.a_bss;

	/* Pad to a page boundary. */
	pad = (unsigned)addr % NBPG;
	if (pad != 0) {
		pad = NBPG - pad;
		addr += pad;
	}
	bootinfo.bi_symtab = addr;

	/********************************************************/
	/* Copy the symbol table size				*/
	/********************************************************/
	pcpy(&head.a_syms, (void *)addr, sizeof(head.a_syms));
	addr += sizeof(head.a_syms);

	/********************************************************/
	/* Load the symbol table				*/
	/********************************************************/
	printf("symbols=[+0x%x+0x%x+0x%x", pad, sizeof(head.a_syms),
	       head.a_syms);
	xread((void *)addr, head.a_syms);
	addr += head.a_syms;

	/********************************************************/
	/* Load the string table size				*/
	/********************************************************/
	read((void *)&i, sizeof(int));
	pcpy(&i, (void *)addr, sizeof(int));
	i -= sizeof(int);
	addr += sizeof(int);

	/********************************************************/
	/* Load the string table				*/
	/********************************************************/
       printf("+0x%x+0x%x]\n", sizeof(int), i);
	xread((void *)addr, i);
	addr += i;

	bootinfo.bi_esymtab = addr;

	/*
	 * For backwards compatibility, use the previously-unused adaptor
	 * and controller bitfields to hold the slice number.
	 */
	bootdev = MAKEBOOTDEV(maj, (slice >> 4), slice & 0xf, unit, part);

	bootinfo.bi_version = BOOTINFO_VERSION;
	bootinfo.bi_kernelname = name + ouraddr;
	bootinfo.bi_nfs_diskless = NULL;
	bootinfo.bi_size = sizeof(bootinfo);
	printf("total=0x%x entry point=0x%x\n", (int)addr, (int)startaddr);
	startprog((int)startaddr, loadflags | RB_BOOTINFO, bootdev,
		  (int)&bootinfo + ouraddr);
}

void
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
					*howto |= RB_CDROM;
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
				}
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
