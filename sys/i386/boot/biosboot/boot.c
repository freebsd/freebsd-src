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
 *	$Id: boot.c,v 1.30 1995/01/20 07:48:19 wpaul Exp $
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

struct exec head;
struct bootinfo bootinfo;

extern void init_serial(void);
extern int probe_keyboard(void);
int loadflags;
unsigned char loadsyms;

extern int end;
boot(drive)
int drive;
{
	int ret;
	char *t;

	if (probe_keyboard()) {
		init_serial();
		loadflags |= RB_SERIAL;
		printf("\nNo keyboard found.\n");
	}

	/* Pick up the story from the Bios on geometry of disks */

	for(ret = 0; ret < N_BIOS_GEOM; ret ++)
		bootinfo.bi_bios_geom[ret] = get_diskinfo(ret + 0x80);

	bootinfo.bi_basemem = memsize(0);
	bootinfo.bi_extmem = memsize(1);
	bootinfo.bi_memsizes_valid = 1;

	/* This is ugly, but why use 4 printf()s when 1 will do? */
	printf("\n\
>> FreeBSD BOOT @ 0x%x: %d/%d k of memory\n\
Use hd(1,a)/kernel to boot sd0 when wd0 is also installed.\n\
Usage: [[[%s(%d,a)]%s][-s][-r][-a][-c][-d][-D][-b][-v][-h]]\n\
Use ? for file list or simply press Return for defaults\n",
	       ouraddr, bootinfo.bi_basemem, bootinfo.bi_extmem,
	       devs[drive & 0x80 ? 0 : 2], drive & 0x7f, name);

	gateA20();

loadstart:
	/***************************************************************\
	* As a default set it to the first partition of the boot	*
	* floppy or hard drive						*
	\***************************************************************/
	part = 0;
	unit = drive & 0x7f;
	maj = (drive&0x80 ? 0 : 2);		/* a good first bet */

	printf("Boot: ");
	getbootdev(&loadflags);
	ret = openrd();
	if (ret != 0) {
		if (ret > 0)
			printf("Can't find %s\n", name);
		goto loadstart;
	}
/*	if (inode.i_mode&IEXEC)
		loadflags |= RB_KDB;
*/
	loadprog(loadflags);
	goto loadstart;
}

loadprog(howto)
	int		howto;
{
	long int startaddr;
	long int addr;	/* physical address.. not directly useable */
	long int bootdev;
	int i;
#ifdef REDUNDANT
	unsigned char	tmpbuf[4096]; /* we need to load the first 4k here */
#endif

	read(&head, sizeof(head));
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
	printf("Booting %s(%d,%c)%s @ 0x%x\n"
			, devs[maj]
			, unit
			, 'a'+part
			, name
			, addr);
/*
 * With the current scheme of things, addr can never be less than ouraddr,
 * so this next bit of code is largely irrelevant. Taking it out saves lots
 * of space.
 */
#ifdef REDUNDANT 
	if(addr < ouraddr)
	{
		if((addr + head.a_text + head.a_data) > ouraddr)
		{
			printf("kernel overlaps loader\n");
			return;
		}
		if((addr + head.a_text + head.a_data + head.a_bss) > 0xa0000)
		{
			printf("bss exceeds 640k limit\n");
			return;
		}
	}
#endif
	printf("text=0x%x ", head.a_text);
	/********************************************************/
	/* LOAD THE TEXT SEGMENT				*/
#ifdef REDUNDANT
	/* don't clobber the first 4k yet (BIOS NEEDS IT) 	*/
	/********************************************************/
	read(tmpbuf,4096);
	addr += 4096; 
	xread(addr, head.a_text - 4096);
	addr += head.a_text - 4096;
#else
	/* Assume we're loading high, so that the BIOS isn't in the way. */
	xread(addr, head.a_text);
	addr += head.a_text;
#endif

	/********************************************************/
	/* Load the Initialised data after the text		*/
	/********************************************************/
	while (addr & CLOFSET)
                *(char *)addr++ = 0;

	printf("data=0x%x ", head.a_data);
	xread(addr, head.a_data);
	addr += head.a_data;

	/********************************************************/
	/* Skip over the uninitialised data			*/
	/* (but clear it)					*/
	/********************************************************/
	printf("bss=0x%x ", head.a_bss);

/*
 * This doesn't do us any good anymore either.
 * XXX however, we should be checking that we don't load over the top of
 * ourselves or into nonexistent memory.  A full symbol table is unlikely
 * to fit on 4MB machines.
 */
#ifdef REDUNDANT
	if( (addr < ouraddr) && ((addr + head.a_bss) > ouraddr))
	{
		pbzero(addr,ouraddr - (int)addr);
	}
	else
	{
		pbzero(addr,head.a_bss);
	}
#else
	pbzero(addr,head.a_bss);
#endif
	addr += head.a_bss;
	if (loadsyms)
	{
		unsigned pad;

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
		pcpy(&head.a_syms, addr, sizeof(head.a_syms));
		addr += sizeof(head.a_syms);
	
		/********************************************************/
		/* Load the symbol table				*/
		/********************************************************/
		printf("symbols=[+0x%x+0x%x+0x%x", pad, sizeof(head.a_syms),
		       head.a_syms);
		xread(addr, head.a_syms);
		addr += head.a_syms;
	
		/********************************************************/
		/* Load the string table size				*/
		/********************************************************/
		read(&i, sizeof(int));
		pcpy(&i, addr, sizeof(int));
		i -= sizeof(int);
		addr += sizeof(int);
	
		/********************************************************/
		/* Load the string table				*/
		/********************************************************/
		printf("+0x%x+0x%x] ", sizeof(int), i);
		xread(addr, i);
		addr += i;

		bootinfo.bi_esymtab = addr;
	}

	/*
	 * For backwards compatibility, use the previously-unused adaptor
	 * and controller bitfields to hold the slice number.
	 */
	bootdev = MAKEBOOTDEV(maj, (slice >> 4), slice & 0xf, unit, part);

#ifdef REDUNDANT
	/****************************************************************/
	/* copy that first page and overwrite any BIOS variables	*/
	/****************************************************************/
	/* Under no circumstances overwrite precious BIOS variables! */
	pcpy(tmpbuf, startaddr, 0x400);
	pcpy(tmpbuf + 0x500, startaddr + 0x500, 4096 - 0x500);
#endif

	bootinfo.bi_version = BOOTINFO_VERSION;
	bootinfo.bi_kernelname = name + ouraddr;
	bootinfo.bi_nfs_diskless = NULL;
	bootinfo.bi_size = sizeof(bootinfo);
	printf("total=0x%x entry point=0x%x\n", (int)addr, (int)startaddr);
	startprog((int)startaddr, howto | RB_BOOTINFO, bootdev,
		  (int)&bootinfo + ouraddr);
}

#define NAMEBUF_LEN	100

char namebuf[NAMEBUF_LEN];
getbootdev(howto)
     int *howto;
{
	char c, *ptr = namebuf;
	if (gets(namebuf)) {
		while (c=*ptr) {
			while (c==' ')
				c = *++ptr;
			if (!c)
				return;
			if (c=='-')
				while ((c = *++ptr) && c!=' ')
					switch (c) {
					      case 'r':
						*howto |= RB_DFLTROOT; continue;
					      case 'a':
						*howto |= RB_ASKNAME; continue;
					      case 'c':
						*howto |= RB_CONFIG; continue;
					      case 's':
						*howto |= RB_SINGLE; continue;
					      case 'd':
						*howto |= RB_KDB; continue;
					      case 'D':
						loadsyms = 1; continue;
					      case 'b':
						*howto |= RB_HALT; continue;
					      case 'v':
						*howto |= RB_VERBOSE; continue;
					      case 'h':
						*howto ^= RB_SERIAL;
						if (*howto & RB_SERIAL)
							init_serial();
						continue;
					}
			else {
				name = ptr;
				while ((c = *++ptr) && c!=' ');
				if (c)
					*ptr++ = 0;
			}
		}
	} else
		printf("\n");
}
