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
 * $FreeBSD: src/sys/boot/pc98/boot2/boot.c,v 1.6.2.2 2000/07/07 00:11:23 obrien Exp $
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

#define	BOOT_CONFIG_SIZE	512
#define	BOOT_HELP_SIZE		2048
#define	KERNEL_CONFIG_SIZE	512
#define	NAMEBUF_LEN		1024	/* oversized to defend against gets() */

static char boot_config[BOOT_CONFIG_SIZE];
static char boot_help[BOOT_HELP_SIZE];
char *name;
static char kernel_config[KERNEL_CONFIG_SIZE];
static char kernel_config_namebuf[NAMEBUF_LEN + sizeof "config"];
static char linebuf[NAMEBUF_LEN];
static char namebuf[NAMEBUF_LEN];
static struct bootinfo bootinfo;
int loadflags;

static void getbootdev(char *ptr, int *howto);
static void loadprog(void);
static void readfile(char *path, char *buf, size_t nbytes);

/* NORETURN */
void
boot(int drive)
{
	int ret;
#ifdef PC98
	int i;
	unsigned char disk_equips;
#endif

	/* Pick up the story from the Bios on geometry of disks */

#ifdef PC98
	for(ret = 0; ret < 2; ret ++) {
		if (*(unsigned char*)V(0xA155d) & (1 << ret)) {
			bootinfo.bi_bios_geom[ret] = get_diskinfo(ret + 0x80);
		}
	}
#else /* IBM-PC */
	for(ret = 0; ret < N_BIOS_GEOM; ret ++)
		bootinfo.bi_bios_geom[ret] = get_diskinfo(ret + 0x80);
#endif /* PC98 */

	bootinfo.bi_basemem = memsize(0);
	bootinfo.bi_extmem = memsize(1);
	bootinfo.bi_memsizes_valid = 1;

	gateA20();

#ifdef PC98
	/* set machine type to PC98_SYSTEM_PARAMETER */
	machine_check();
#endif /* PC98 */

	/*
	 * The default boot device is the first partition in the
	 * compatibility slice on the boot drive.
	 */
	dosdev = drive;
#ifdef PC98
	maj = (drive&0x70) >> 3;		/* a good first bet */
	if (maj == 4) {		/* sd */
		disk_equips = *(unsigned char *)V(0xA1482);
		unit = 0;
		for (i=0; i<(drive&0x0f); i++) {
			unit += (disk_equips >> i) & 1;
		}
	} else {
		unit = drive & 0x0f;
	}
#else /* IBM-PC */
	maj = 2;
	unit = drive & 0x7f;
#ifdef dontneed
	slice = 0;
	part = 0;
#endif
	if (drive & 0x80) {
		/* Hard drive.  Adjust. */
		maj = 0;
#if BOOT_HD_BIAS > 0
		if (unit >= BOOT_HD_BIAS) {
			/*
			 * The drive is probably a SCSI drive with a unit
			 * number BOOT_HD_BIAS less than the BIOS drive
			 * number.
			 */
			maj = 4;
			unit -= BOOT_HD_BIAS;
		}
#endif
	}
#endif /* PC98 */
	readfile("boot.config", boot_config, BOOT_CONFIG_SIZE);
		name = "/boot/loader";
	if (boot_config[0] != '\0') {
		printf("boot.config: %s", boot_config);
		getbootdev(boot_config, &loadflags);
		if (openrd() != 0)
			name = "kernel";
	}
loadstart:
	/* print this all each time.. (saves space to do so) */
	/* If we have looped, use the previous entries as defaults */
	printf("\r \n>> FreeBSD BOOT @ 0x%x: %d/%d k of memory, %s%s console\n"
	       "Boot default: %d:%s(%d,%c)%s\n"
	       "%s\n"
	       "boot: ",
	       ouraddr, bootinfo.bi_basemem, bootinfo.bi_extmem,
	       (loadflags & RB_SERIAL) ? "serial" : "internal",
	       (loadflags & RB_DUAL) ? "/dual" : "",
#ifdef PC98
	       dosdev & 0x0f, devs[maj], unit, 'a' + part,
#else
	       dosdev & 0x7f, devs[maj], unit, 'a' + part,
#endif
	       name ? name : "*specify_a_kernel_name*",
	       boot_help);

	/*
	 * Ignore flags from previous attempted boot, if any.
	 * XXX this is now too strict.  Settings given in boot.config should
	 * not be changed.
	 */
	loadflags &= (RB_DUAL | RB_SERIAL);

	/*
	 * Be paranoid and make doubly sure that the input buffer is empty.
	 */
	if (loadflags & (RB_DUAL | RB_SERIAL))
		init_serial();

	if (!gets(linebuf))
		putchar('\n');
	else
		getbootdev(linebuf, &loadflags);
	if (name == NULL)
		goto loadstart;
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
	struct exec head;
	int startaddr;
	int addr;	/* physical address.. not directly useable */
	int bootdev;
	int i;
	unsigned pad;
	char *s, *t;

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
#ifdef PC98
			, dosdev & 0x0f
#else
			, dosdev & 0x7f
#endif
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
	while (addr & PAGE_MASK)
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
	/* kzip & kernel will zero their own bss */
	addr += head.a_bss;

	/* Pad to a page boundary. */
	pad = (unsigned)addr & PAGE_MASK;
	if (pad != 0) {
		pad = PAGE_SIZE - pad;
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
	bootinfo.bi_kernelname = (u_int32_t)(name + ouraddr);
	bootinfo.bi_nfs_diskless = 0;
	bootinfo.bi_size = sizeof(bootinfo);
	bootinfo.bi_bios_dev = dosdev;

	/*
	 * Load the kernel config file (if any).  Its name is given by
	 * appending ".config" to the kernel name.  Build the name inline
	 * because no str*() functions are available.  The file has to be
	 * copied to &disklabel for userconfig.  It can't be loaded there
	 * directly because the label is used late in readfile() in some
	 * unusual cases.
	 */
	s = name;
	t = kernel_config_namebuf;
	do
		;
	while ((*t++ = *s++) != '\0');
	s = ".config";
	--t;
	do
		;
	while ((*t++ = *s++) != '\0');
	readfile(kernel_config_namebuf, kernel_config, KERNEL_CONFIG_SIZE);
	pcpy(kernel_config, (char *)&disklabel + ouraddr, KERNEL_CONFIG_SIZE);

	printf("total=0x%x entry point=0x%x\n", addr, startaddr);
	startprog(startaddr, loadflags | RB_BOOTINFO, bootdev,
		  (unsigned)&bootinfo + ouraddr);
}

static void
readfile(char *path, char *buf, size_t nbytes)
{
	int openstatus;

	buf[0] = '\0';
	name = path;
	openstatus = openrd();
	if (openstatus == 0) {
		/* XXX no way to determine file size. */
		read(buf, nbytes);
	}
	buf[nbytes - 1] = '\0';
}

static void
getbootdev(char *ptr, int *howto)
{
	char c;
	int f;
	char *p;

	/* Copy the flags to save some bytes. */
	f = *howto;

	c = *ptr;
	for (;;) {
nextarg:
		while (c == ' ' || c == '\n')
			c = *++ptr;
		if (c == '-')
			while ((c = *++ptr) != '\0') {
				if (c == ' ' || c == '\n')
					goto nextarg;
				if (c == 'a')
					f |= RB_ASKNAME;
				if (c == 'C')
					f |= RB_CDROM;
				if (c == 'c')
					f |= RB_CONFIG;
				if (c == 'D')
					f ^= RB_DUAL;
				if (c == 'd')
					f |= RB_KDB;
				if (c == 'g')
					f |= RB_GDB;
				if (c == 'h')
					f ^= RB_SERIAL;
				if (c == 'P')
					f |= RB_PROBEKBD;
				if (c == 'r')
					f |= RB_DFLTROOT;
				if (c == 's')
					f |= RB_SINGLE;
				if (c == 'v')
					f |= RB_VERBOSE;
			}
		if (c == '\0')
			break;
		p = name = namebuf;
		while (c != '\0' && c != ' ' && c != '\n') {
			*p++ = c;
			c = *++ptr;
		}
		*p = '\0';
	}
	if (f & RB_PROBEKBD) {
		if (probe_keyboard()) {
			f |= RB_DUAL | RB_SERIAL;
			printf("No keyboard found\n");
		} else
			printf("Keyboard found\n");
	}
	if (f & (RB_DUAL | RB_SERIAL))
		init_serial();
	*howto = f;
}
