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
 *	$FreeBSD$
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
#include <stdio.h>
#include <conio.h>
#include <process.h>
#include <memory.h>

#include "bootinfo.h"
#include "protmod.h"
#include "param.h"
#include "boot.h"
#include "reboot.h"

#include "exec.h"

int openrd(char *kernel);
void ufs_read(char *buffer, long count);
void xread(unsigned long addr, long size);

static struct exec head;
static long argv[10];
static char buf[__LDPGSZ];
static long int startaddr;

void pbzero(unsigned long addr, unsigned long size)
{
	long s;

	memset(buf, 0, __LDPGSZ);
	while (size) {
		s = size > __LDPGSZ ? __LDPGSZ : size;
		pm_copy(buf, addr, s);
		size -= s;
		addr += s;
	}
}

static long loadprog(long *hsize)
{
	long addr;	/* physical address.. not directly useable */
	long hmaddress;
	unsigned long pad;
	long i;
	static int (*x_entry)() = 0;

	ufs_read(&head, (long) sizeof(head));
	if (N_BADMAG(head)) {
		printf("Invalid format!\n");
		exit(0);
	}

	startaddr = (long)head.a_entry;
	addr = (startaddr & 0x00ffffffl); /* some MEG boundary */
	printf("Booting @ 0x%lx\n", addr);
	if(addr < 0x100000l)
	{
		printf("Start address too low!\n");
		exit(0);
	}

	poff = N_TXTOFF(head)+head.a_text+head.a_data+head.a_syms;
	ufs_read((void *)&i, sizeof(long));
	*hsize = head.a_text+head.a_data+head.a_bss;
	*hsize = (*hsize+NBPG-1)&~(NBPG-1);
	*hsize += i+4+head.a_syms;
	addr=hmaddress=get_high_memory(*hsize);
	if (!hmaddress) {
		printf("Sorry, can't allocate enough memory!\n");
		exit(0);
	}

	poff = N_TXTOFF(head);

	/********************************************************/
	/* LOAD THE TEXT SEGMENT                                */
	/********************************************************/
	printf("text=0x%lx ", head.a_text);
	xread(addr, head.a_text);
	addr += head.a_text;

	/********************************************************/
	/* Load the Initialised data after the text		*/
	/********************************************************/
	while (addr & CLOFSET)
		pm_copy("\0", addr++, 1);

	printf("data=0x%lx ", head.a_data);
	xread(addr, head.a_data);
	addr += head.a_data;

	/********************************************************/
	/* Skip over the uninitialised data			*/
	/* (but clear it)					*/
	/********************************************************/
	printf("bss=0x%lx ", head.a_bss);
	pbzero(addr, head.a_bss);
	addr += head.a_bss;

	/* Pad to a page boundary. */
	pad = (unsigned long)(addr-hmaddress+(startaddr & 0x00ffffffl)) % NBPG;
	if (pad != 0) {
		pad = NBPG - pad;
		addr += pad;
	}
	bootinfo.bi_symtab = addr-hmaddress+(startaddr & 0x00ffffffl);

	/********************************************************/
	/* Copy the symbol table size				*/
	/********************************************************/
	pm_copy((char *)&head.a_syms, addr, sizeof(head.a_syms));
	addr += sizeof(head.a_syms);

	/********************************************************/
	/* Load the symbol table				*/
	/********************************************************/
	printf("symbols=[+0x%lx+0x%lx+0x%lx", pad, (long) sizeof(head.a_syms),
	       (long) head.a_syms);
	xread(addr, head.a_syms);
	addr += head.a_syms;

	/********************************************************/
	/* Load the string table size				*/
	/********************************************************/
	ufs_read((void *)&i, sizeof(long));
	pm_copy((char *)&i, addr, sizeof(long));
	i -= sizeof(long);
	addr += sizeof(long);

	/********************************************************/
	/* Load the string table				*/
	/********************************************************/
	printf("+0x%x+0x%lx] ", sizeof(long), i);
	xread(addr, i);
	addr += i;

	bootinfo.bi_esymtab = addr-hmaddress+(startaddr & 0x00ffffffl);

	/*
	 * For backwards compatibility, use the previously-unused adaptor
	 * and controller bitfields to hold the slice number.
	 */
	printf("total=0x%lx entry point=0x%lx\n",
		addr-hmaddress+(startaddr & 0x00ffffffl),
		startaddr & 0x00ffffffl);

	return hmaddress;
}

void bsdboot(int drive, long loadflags, char *kernel)
{
	long hmaddress, size, bootdev;

	/***************************************************************\
	* As a default set it to the first partition of the first	*
	* floppy or hard drive						*
	\***************************************************************/
	part = unit = 0;
	maj = (drive&0x80 ? 0 : 2);		/* a good first bet */

	if (openrd(kernel)) {
		printf("Can't find %s\n", kernel);
		exit(0);
	}
	hmaddress = loadprog(&size);
	bootdev = MAKEBOOTDEV(maj, (slice >> 4), slice & 0xf, unit, part);
	startprog(hmaddress, size, ((long)startaddr & 0xffffffl),
			  loadflags | RB_BOOTINFO, bootdev);
}
