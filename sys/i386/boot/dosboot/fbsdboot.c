/*
 *	fbsdboot.c		Boot FreeBSD from DOS
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
 */
#include <dos.h>
#include <stdio.h>
#include <process.h>

#include "reboot.h"
#include "boot.h"
#include "bootinfo.h"
#include "dosboot.h"
#include "protmod.h"

#define MAV	1
#define MIV	7

#define ptr2pa(x)	(((((long)(x))&0xffff0000l)>>12l)+(((long)(x))&0xffffl))

static void usage(char *name)
{
	fprintf(stderr, "FreeBSD boot Version %d.%d\n", MAV, MIV);
	fprintf(stderr, "(c) 1994, 1995 Christian Gusenbauer,\n    cg@fimp01.fim.uni-linz.ac.at\n\n");
	fprintf(stderr, "usage: %s [ options ] [ kernelname ]\n", name);
	fprintf(stderr, "where options are:\n");
	fprintf(stderr, "\t-r ... use compiled-in rootdev\n");
	fprintf(stderr, "\t-s ... reboot to single user only\n");
	fprintf(stderr, "\t-a ... ask for file name to reboot from\n");
	fprintf(stderr, "\t-d ... give control to kernel debugger\n");
	fprintf(stderr, "\t-c ... invoke user configuration routing\n");
	fprintf(stderr, "\t-v ... print all potentially useful info\n");
	fprintf(stderr, "\t-C ... use cdrom as root\n");
	fprintf(stderr, "\t-D ... boot a kernel from a DOS medium\n");
	fprintf(stderr, "\t       (default: c:\\kernel)\n");
	exit(1);
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

int main(int argc, char *argv[])
{
	char *kernel="/kernel", *ptr;
	int i, dos=0;
	long howto=0;
	extern unsigned long get_diskinfo(int);

	VCPIboot = 0;
	slice = 0;

	for (i = 1; i < argc; i++) {			/* check arguments */
		if (argv[i][0] != '-') {			/* kernel name */
			kernel = argv[i];
			break;
		}
		ptr = &argv[i][1];
		while (*ptr) {						/* check options */
			switch(*ptr) {
				case 'r': howto |= RB_DFLTROOT; break;
				case 's': howto |= RB_SINGLE; break;
				case 'a': howto |= RB_ASKNAME; break;
				case 'c': howto |= RB_CONFIG; break;
				case 'd': howto |= RB_KDB; break;
				case 'g': howto |= RB_GDB; break;
				case 'v': howto |= RB_VERBOSE; break;
				case 'C': howto |= RB_CDROM; break;
				case 'D': dos = 1; kernel = "c:\\kernel"; break;
				case '?':
				default: usage(argv[0]);
			}
			ptr++;
		}
	}

	bootinfo.bi_version = BOOTINFO_VERSION;
	for (i = 0; i < N_BIOS_GEOM; i++)
		bootinfo.bi_bios_geom[i] = get_diskinfo(0x80+i);
	bootinfo.bi_basemem = memsize(0);
	bootinfo.bi_extmem = memsize(1);
	bootinfo.bi_memsizes_valid = 0;		/* that is not yet valid!! */
	bootinfo.bi_kernelname = (char *) ptr2pa(kernel);
	bootinfo.bi_nfs_diskless = NULL;
	bootinfo.bi_size = sizeof(bootinfo);

	if (dos)
		dosboot(howto, kernel);		/* boot given kernel from DOS partition */
	else
		bsdboot(0x80, howto, kernel);	/* boot from FreeBSD partition */
	return 0;
}
