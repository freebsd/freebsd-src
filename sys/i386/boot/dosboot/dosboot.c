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

static long loadprog(FILE *fp, long *hsize)
{
	long int addr;	/* physical address.. not directly useable */
	long int hmaddress, pad, i;
	static int (*x_entry)() = 0;

	fread(&head, sizeof(head), 1, fp);
	fseek(fp, 4096-sizeof(head), 1);
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
	fseek(fp, poff, 0);
	fread(&i, sizeof(i), 1, fp);
	*hsize = head.a_text+head.a_data+head.a_bss;
	*hsize = (*hsize+NBPG-1)&~(NBPG-1);
	*hsize += i+4+head.a_syms;
	addr=hmaddress=get_high_memory(*hsize);
	if (!hmaddress) {
		printf("Sorry, can't allocate enough memory!\n");
		exit(0);
	}

	poff = N_TXTOFF(head);
	fseek(fp, poff, 0);

	/********************************************************/
	/* LOAD THE TEXT SEGMENT				*/
	/********************************************************/
	printf("text=0x%lx ", head.a_text);
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
	dosxread(fp, addr, head.a_syms);
	addr += head.a_syms;

	/********************************************************/
	/* Load the string table size				*/
	/********************************************************/
	fread((void *)&i, sizeof(long), 1, fp);
	pm_copy((char *)&i, addr, sizeof(long));
	i -= sizeof(long);
	addr += sizeof(long);

	/********************************************************/
	/* Load the string table				*/
	/********************************************************/
	printf("+0x%x+0x%lx] ", sizeof(long), i);
	dosxread(fp, addr, i);
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

void dosboot(long howto, char *kernel)
{
	long hmaddress, size, bootdev;
	FILE *fp;

	fp = fopen(kernel, "rb");			/* open kernel for reading */
	if (!fp) {
		fprintf(stderr, "Sorry, can't open %s!\n", kernel);
		return;
	}
	hmaddress = loadprog(fp, &size);
	fclose(fp);

	bootdev = MAKEBOOTDEV(maj, (slice >> 4), slice & 0xf, unit, part);
	startprog(hmaddress, size, ((long)startaddr & 0xffffffl),
			  howto | RB_BOOTINFO, bootdev);
}
