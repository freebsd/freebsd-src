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
 *	$Id: boot.c,v 1.9.2.1 1994/05/01 05:14:49 rgrimes Exp $
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

#include "protmod.h"
#include "param.h"
#include "boot.h"
#include "bootinfo.h"
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

static long loadprog(int howto, long *hsize)
{
	long int addr;	/* physical address.. not directly useable */
	long int hmaddress;
	int i;
	static int (*x_entry)() = 0;

	argv[3] = 0;
	argv[4] = 0;
	ufs_read(&head, (long) sizeof(head));
	if ( N_BADMAG(head)) {
		printf("Invalid format!\n");
		exit(0);
	}

	poff = N_TXTOFF(head);

	startaddr = (long)head.a_entry;
	addr = (startaddr & 0x00ffffffl); /* some MEG boundary */
	printf("Booting @ 0x%lx\n", addr);
	if(addr < 0x100000l)
	{
		printf("kernel linked for wrong address!\n");
		printf("Only hope is to link the kernel for > 1MB\n");
		exit(0);
	}

	*hsize = head.a_text+head.a_data+head.a_bss;
	addr=hmaddress=get_high_memory(*hsize);
	if (!hmaddress) {
		printf("Sorry, can't allocate enough memory!\n");
		exit(0);
	}

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
	argv[3] = (addr += head.a_bss);
	argv[3] += -hmaddress+0x100000l;

	/********************************************************/
	/* and note the end address of all this			*/
	/********************************************************/

	addr = addr-hmaddress+0x100000l;
	argv[4] = ((addr+(long) sizeof(long)-1l))&~((long)sizeof(long)-1l);
	printf("total=0x%lx ",argv[4]);

	/*
	 *  We now pass the various bootstrap parameters to the loaded
	 *  image via the argument list
	 *  (THIS IS A BIT OF HISTORY FROM MACH.. LEAVE FOR NOW)
	 *  arg1 = boot flags
	 *  arg2 = boot device
	 *  arg3 = start of symbol table (0 if not loaded)
	 *  arg4 = end of symbol table (0 if not loaded)
	 *  arg5 = transfer address from image
	 *  arg6 = transfer address for next image pointer
	 */
	switch(maj)
	{
	case 2:
		printf("\n\nInsert file system floppy in drive A or B\n");
		printf("Press 'A', 'B' or any other key for the default ");
		printf("%c: ", unit+'A');
		i = _getche();
		if (i=='0' || i=='A' || i=='a')
			unit = 0;
		if (i=='1' || i=='B' || i=='b')
			unit = 1;
		printf("\n");
		break;
	case 4:
		break;
	}
	argv[1] = howto;
	argv[2] = (MAKEBOOTDEV(maj, (slice>>4), (slice&0xf), unit, part)) ;
	argv[5] = (head.a_entry &= 0xfffffff);
	argv[6] = (long) &x_entry;
	argv[0] = 8;

	printf("entry point=0x%lx\n" ,((long)startaddr) & 0xffffff);
	return hmaddress;
}

static unsigned int memsize(int x)
{
	unsigned int rt=0;

	switch (x) {
		case 1:
			_asm {
				mov		bl,1
				mov 	ah,88h
				int		15h
				mov		rt,ax
			}
			break;
		default:
			_asm {
				int		12h
				mov		rt,ax
			}
			break;
	}
	return rt;
}

void bsdboot(int drive, int loadflags, char *kernel)
{
	long hmaddress, size;

	argv[7] = memsize(0);
	argv[8] = memsize(1);

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
	hmaddress = loadprog(loadflags, &size);
	startprog(hmaddress, size, ((long)startaddr & 0xffffffl), argv);
}
