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
 *	$Id: boot.c,v 1.14 1994/06/16 03:53:27 adam Exp $
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

struct exec head;
int argv[10], esym;
char *name;
char *names[] = {
	"/386bsd", "/o386bsd", "/386bsd.old"
};
#define NUMNAMES	(sizeof(names)/sizeof(char *))

extern int end;
boot(drive)
int drive;
{
	int loadflags, currname = 0;
	char *t;
		
	printf("\n>> FreeBSD BOOT @ 0x%x: %d/%d k of memory  [%s]\n",
		ouraddr,
		argv[7] = memsize(0),
		argv[8] = memsize(1),
		"$Revision: 1.14 $");
	printf("use hd(1,a)/386bsd to boot sd0 when wd0 is also installed\n");
	gateA20();
loadstart:
	/***************************************************************\
	* As a default set it to the first partition of the first	*
	* floppy or hard drive						*
	\***************************************************************/
	part = unit = 0;
	maj = (drive&0x80 ? 0 : 2);		/* a good first bet */
	name = names[currname++];

	loadflags = 0;
	if (currname == NUMNAMES)
		currname = 0;
	getbootdev(&loadflags);
	if (openrd()) {
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
	long int addr0;
	int i;
	static int (*x_entry)() = 0;
	unsigned char	tmpbuf[4096]; /* we need to load the first 4k here */

	argv[3] = 0;
	argv[4] = 0;
	read(&head, sizeof(head));
	if ( N_BADMAG(head)) {
		printf("Invalid format!\n");
		return;
	}

	poff = N_TXTOFF(head);
	/*if(poff==0)
		poff = 32;*/

	startaddr = (int)head.a_entry;
	addr = (startaddr & 0x00ffffff); /* some MEG boundary */
	addr0 = addr;
	printf("Booting %s(%d,%c)%s @ 0x%x\n"
			, devs[maj]
			, unit
			, 'a'+part
			, name
			, addr);
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
	printf("text=0x%x ", head.a_text);
	/********************************************************/
	/* LOAD THE TEXT SEGMENT				*/
	/* don't clobber the first 4k yet (BIOS NEEDS IT) 	*/
	/********************************************************/
	read(tmpbuf,4096);
	addr += 4096; 
	xread(addr, head.a_text - 4096);
	addr += head.a_text - 4096;

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
	if( (addr < ouraddr) && ((addr + head.a_bss) > ouraddr))
	{
		pbzero(addr,ouraddr - (int)addr);
	}
	else
	{
		pbzero(addr,head.a_bss);
	}
	argv[3] = (addr += head.a_bss);

#ifdef LOADSYMS /* not yet, haven't worked this out yet */
	if (addr > 0x100000)
	{
		/********************************************************/
		/*copy in the symbol header				*/
		/********************************************************/
		pcpy(&head.a_syms, addr, sizeof(head.a_syms));
		addr += sizeof(head.a_syms);
	
		/********************************************************/
		/* READ in the symbol table				*/
		/********************************************************/
		printf("symbols=[+0x%x", head.a_syms);
		xread(addr, head.a_syms);
		addr += head.a_syms;
	
		/********************************************************/
		/* Followed by the next integer (another header)	*/
		/* more debug symbols?					*/
		/********************************************************/
		read(&i, sizeof(int));
		pcpy(&i, addr, sizeof(int));
		i -= sizeof(int);
		addr += sizeof(int);
	
	
		/********************************************************/
		/* and that many bytes of (debug symbols?)		*/
		/********************************************************/
		printf("+0x%x] ", i);
		xread(addr, i);
		addr += i;
	}
#endif	LOADSYMS
	/********************************************************/
	/* and note the end address of all this			*/
	/********************************************************/

	argv[4] = ((addr+sizeof(int)-1))&~(sizeof(int)-1);
	printf("total=0x%x ",argv[4]);


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
		i = getchar();
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
	argv[2] = (MAKEBOOTDEV(maj, 0, 0, unit, part)) ;
	argv[5] = (head.a_entry &= 0xfffffff);
	argv[6] = (int) &x_entry;
	argv[0] = 8;
	/****************************************************************/
	/* copy that first page and overwrite any BIOS variables	*/
	/****************************************************************/
	printf("entry point=0x%x\n" ,((int)startaddr) & 0xffffff);
	/* Under no circumstances overwrite precious BIOS variables! */
	pcpy(tmpbuf, addr0, 0x400);
	pcpy(tmpbuf + 0x500, addr0 + 0x500, 4096 - 0x500);
	startprog(((int)startaddr & 0xffffff),argv);
}

char namebuf[100];
getbootdev(howto)
     int *howto;
{
	char c, *ptr = namebuf;
	printf("Boot: [[[%s(%d,%c)]%s][-s][-a][-d]] :- "
			, devs[maj]
			, unit
			, 'a'+part
			, name);
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
					      case 's':
						*howto |= RB_SINGLE; continue;
					      case 'd':
						*howto |= RB_KDB; continue;
					      case 'b':
						*howto |= RB_HALT; continue;
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

