/*
 *	dosboot.c		Boot FreeBSD from DOS partition
 *
 *	(C) 1994 by Christian Gusenbauer (cg@fimp01.fim.uni-linz.ac.at)
 *	All Rights Reserved.
 * 
 *	Permission to use, copy, modify and distribute this software and its
 *	documentation is hereby granted, provided that both the copyright
 *	notice and this permission notice appear in all copies of the
 *	software, derivative works or modified versions, and any portions
 *	thereof, and that both notices appear in supporting documentation.
 * 
 *	I ALLOW YOU USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION. I DISCLAIM
 *	ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
 *	USE OF THIS SOFTWARE.
 * 
 *	Parts of this file are
 *	Copyright (c) 1992, 1991 Carnegie Mellon University
 *	All Rights Reserved.
 * 
 *	Permission to use, copy, modify and distribute this software and its
 *	documentation is hereby granted, provided that both the copyright
 *	notice and this permission notice appear in all copies of the
 *	software, derivative works or modified versions, and any portions
 *	thereof, and that both notices appear in supporting documentation.
 * 
 *	CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 *	CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 *	ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 *	Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 *	any improvements or extensions that they make and grant Carnegie Mellon
 *	the rights to redistribute these changes.
 */
#include <bios.h>
#include <stdio.h>
#include <process.h>

#include "protmod.h"
#include "param.h"
#include "boot.h"
#include "bootinfo.h"
#include "reboot.h"

#include "exec.h"

#define BUFSIZE 4096

static struct exec head;
static long argv[10];
static long startaddr;

int biosread(int dev, int track, int head, int sector, int cnt, unsigned char far *buffer)
{
	struct _diskinfo_t di;
	int r;

	di.drive = dev;						/* first hard disk */
	di.head = head;						/* head # */
	di.track = track;					/* track # */
	di.sector = sector+1;				/* sector # */
	di.nsectors = cnt;					/* only 1 sector */
	di.buffer = (void far *) buffer;    /* sector buffer */
	r= _bios_disk(_DISK_READ, &di);
	return r&0xFF00;
}

static void dosxread(FILE *fp, unsigned long addr, long size)
{
	extern char buf[BUFSIZE];

	int count = BUFSIZE;
	while (size > 0l) {
		if (BUFSIZE > size)
			count = (int) size;
		fread(buf, count, 1, fp);
		pm_copy(buf, addr, count);
		size -= count;
		addr += count;
	}
}

static long loadprog(FILE *fp, int howto, long *hsize)
{
	long int addr;	/* physical address.. not directly useable */
	long int hmaddress;
	static int (*x_entry)() = 0;

	argv[3] = 0;
	argv[4] = 0;
	fread(&head, sizeof(head), 1, fp);
	fseek(fp, 4096-sizeof(head), 1);
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

	printf("text=0x%lx ", head.a_text);
	/********************************************************/
	/* LOAD THE TEXT SEGMENT				*/
	/********************************************************/
	dosxread(fp, addr, head.a_text);
	addr += head.a_text;

	/********************************************************/
	/* Load the Initialised data after the text		*/
	/********************************************************/
	while (addr & CLOFSET)
		pm_copy("\0", addr++, 1);

	printf("data=0x%lx ", head.a_data);
	dosxread(fp, addr, head.a_data);
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
	argv[1] = howto;
	argv[2] = (MAKEBOOTDEV(maj, (slice>>4), (slice&0xf), unit, part)) ;
	argv[5] = (head.a_entry &= 0xfffffff);
	argv[6] = (long) &x_entry;
	argv[0] = 8;

	printf("entry point=0x%lx\n" ,((long)startaddr) & 0xffffff);
	return hmaddress;
}

void dosboot(int howto, char *kernel)
{
	long hmaddress, size;
	FILE *fp;

	fp = fopen(kernel, "rb");			/* open kernel for reading */
	if (!fp) {
		fprintf(stderr, "Sorry, can't open %s!\n", kernel);
		return;
	}
	hmaddress = loadprog(fp, howto, &size);
	fclose(fp);
	startprog(hmaddress, size, (startaddr & 0xffffffl), argv);
}

