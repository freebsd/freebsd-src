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
 *	$Id: boot.c,v 1.28 1994/12/18 19:14:13 bde Exp $
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

struct exec head;
struct bootinfo_t bootinfo;
char *name;
char *names[] = {
	"/kernel"
};
#define NUMNAMES	(sizeof(names)/sizeof(char *))

#ifdef COMCONSOLE
extern void init_serial(void);
#endif

extern int end;
boot(drive)
int drive;
{
	int loadflags, currname = 0, ret;
	char *t;
#ifdef COMCONSOLE
	init_serial();
#endif

		
	/* Pick up the story from the Bios on geometry of disks */

	for(ret = 0; ret < N_BIOS_GEOM; ret ++)
		bootinfo.bios_geom[ret] = get_diskinfo(ret + 0x80);

	printf("\n>> FreeBSD BOOT @ 0x%x: %d/%d k of memory\n",
		ouraddr, memsize(0), memsize(1));
	printf("Use hd(1,a)/kernel to boot sd0 when wd0 is also installed.\n");
	printf("Usage: [[[%s(0,a)]%s][-s][-r][-a][-c][-d][-b][-v]]\n",
			devs[(drive & 0x80) ? 0 : 2], names[0]);
	printf("Use ? for file list or simply press Return for defaults\n");
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
	long int total;
	int i;
	unsigned char	tmpbuf[4096]; /* we need to load the first 4k here */

	read(&head, sizeof(head));
	if ( N_BADMAG(head)) {
		printf("Invalid format!\n");
		return;
	}

	poff = N_TXTOFF(head);
	/*if(poff==0)
		poff = 32;*/

	startaddr = (int)head.a_entry & 0x00FFFFFF; /* some MEG boundary */
	addr =  startaddr;
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
	addr += head.a_bss;

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
	total = ((addr+sizeof(int)-1))&~(sizeof(int)-1);
	printf("total=0x%x ", total);

	/*
	 * For backwards compatibility, use the previously-unused adaptor
	 * and controller bitfields to hold the slice number.
	 */
	bootdev = MAKEBOOTDEV(maj, (slice >> 4), slice & 0xf, unit, part);

	/****************************************************************/
	/* copy that first page and overwrite any BIOS variables	*/
	/****************************************************************/
	printf("entry point=0x%x\n" ,(int)startaddr);
	/* Under no circumstances overwrite precious BIOS variables! */
	pcpy(tmpbuf, startaddr, 0x400);
	pcpy(tmpbuf + 0x500, startaddr + 0x500, 4096 - 0x500);
	bootinfo.version=1;
	bootinfo.kernelname=(char *)((int)name + (BOOTSEG<<4));
	bootinfo.nfs_diskless=0;
	startprog((int)startaddr, howto, bootdev, (int)&bootinfo+(BOOTSEG<<4));
	return;
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
					      case 'b':
						*howto |= RB_HALT; continue;
					      case 'v':
						*howto |= RB_VERBOSE; continue;
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

