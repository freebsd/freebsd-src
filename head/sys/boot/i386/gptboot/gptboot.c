/*-
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/gpt.h>
#include <sys/dirent.h>
#include <sys/reboot.h>

#include <machine/bootinfo.h>
#include <machine/elf.h>
#include <machine/psl.h>

#include <stdarg.h>

#include <a.out.h>

#include <btxv86.h>

#include "lib.h"
#include "rbx.h"
#include "drv.h"
#include "util.h"
#include "cons.h"
#include "gpt.h"

#define PATH_DOTCONFIG  "/boot.config"
#define PATH_CONFIG	"/boot/config"
#define PATH_BOOT3	"/boot/loader"
#define PATH_KERNEL	"/boot/kernel/kernel"

#define ARGS		0x900
#define NOPT		14
#define NDEV		3
#define MEM_BASE	0x12
#define MEM_EXT 	0x15

#define DRV_HARD	0x80
#define DRV_MASK	0x7f

#define TYPE_AD		0
#define TYPE_DA		1
#define TYPE_MAXHARD	TYPE_DA
#define TYPE_FD		2

extern uint32_t _end;

static const uuid_t freebsd_ufs_uuid = GPT_ENT_TYPE_FREEBSD_UFS;
static const char optstr[NOPT] = "DhaCcdgmnpqrsv"; /* Also 'P', 'S' */
static const unsigned char flags[NOPT] = {
	RBX_DUAL,
	RBX_SERIAL,
	RBX_ASKNAME,
	RBX_CDROM,
	RBX_CONFIG,
	RBX_KDB,
	RBX_GDB,
	RBX_MUTE,
	RBX_NOINTR,
	RBX_PAUSE,
	RBX_QUIET,
	RBX_DFLTROOT,
	RBX_SINGLE,
	RBX_VERBOSE
};
uint32_t opts;

static const char *const dev_nm[NDEV] = {"ad", "da", "fd"};
static const unsigned char dev_maj[NDEV] = {30, 4, 2};

static struct dsk dsk;
static char kname[1024];
static int comspeed = SIOSPD;
static struct bootinfo bootinfo;

void exit(int);
static void load(void);
static int parse(char *, int *);
static int dskread(void *, daddr_t, unsigned);
static uint32_t memsize(void);

#include "ufsread.c"

static inline int
xfsread(ufs_ino_t inode, void *buf, size_t nbyte)
{

	if ((size_t)fsread(inode, buf, nbyte) != nbyte) {
		printf("Invalid %s\n", "format");
		return (-1);
	}
	return (0);
}

static inline uint32_t
memsize(void)
{

	v86.addr = MEM_EXT;
	v86.eax = 0x8800;
	v86int();
	return (v86.eax);
}

static int
gptinit(void)
{

	if (gptread(&freebsd_ufs_uuid, &dsk, dmadat->secbuf) == -1) {
		printf("%s: unable to load GPT\n", BOOTPROG);
		return (-1);
	}
	if (gptfind(&freebsd_ufs_uuid, &dsk, dsk.part) == -1) {
		printf("%s: no UFS partition was found\n", BOOTPROG);
		return (-1);
	}
	dsk_meta = 0;
	return (0);
}

int
main(void)
{
	char cmd[512], cmdtmp[512];
	int autoboot, dskupdated;
	ufs_ino_t ino;

	dmadat = (void *)(roundup2(__base + (int32_t)&_end, 0x10000) - __base);
	v86.ctl = V86_FLAGS;
	v86.efl = PSL_RESERVED_DEFAULT | PSL_I;
	dsk.drive = *(uint8_t *)PTOV(ARGS);
	dsk.type = dsk.drive & DRV_HARD ? TYPE_AD : TYPE_FD;
	dsk.unit = dsk.drive & DRV_MASK;
	dsk.part = -1;
	dsk.start = 0;
	bootinfo.bi_version = BOOTINFO_VERSION;
	bootinfo.bi_size = sizeof(bootinfo);
	bootinfo.bi_basemem = 0;	/* XXX will be filled by loader or kernel */
	bootinfo.bi_extmem = memsize();
	bootinfo.bi_memsizes_valid++;

	/* Process configuration file */

	if (gptinit() != 0)
		return (-1);

	autoboot = 1;
	*cmd = '\0';

	for (;;) {
		*kname = '\0';
		if ((ino = lookup(PATH_CONFIG)) ||
		    (ino = lookup(PATH_DOTCONFIG)))
			fsread(ino, cmd, sizeof(cmd));

		if (*cmd != '\0') {
			memcpy(cmdtmp, cmd, sizeof(cmdtmp));
			if (parse(cmdtmp, &dskupdated))
				break;
			if (dskupdated && gptinit() != 0)
				break;
			if (!OPT_CHECK(RBX_QUIET))
				printf("%s: %s", PATH_CONFIG, cmd);
			*cmd = '\0';
		}

		if (autoboot && keyhit(3)) {
			if (*kname == '\0')
				memcpy(kname, PATH_BOOT3, sizeof(PATH_BOOT3));
			break;
		}
		autoboot = 0;

		/*
		 * Try to exec stage 3 boot loader. If interrupted by a
		 * keypress, or in case of failure, try to load a kernel
		 * directly instead.
		 */
		if (*kname != '\0')
			load();
		memcpy(kname, PATH_BOOT3, sizeof(PATH_BOOT3));
		load();
		memcpy(kname, PATH_KERNEL, sizeof(PATH_KERNEL));
		load();
		gptbootfailed(&dsk);
		if (gptfind(&freebsd_ufs_uuid, &dsk, -1) == -1)
			break;
		dsk_meta = 0;
	}

	/* Present the user with the boot2 prompt. */

	for (;;) {
		if (!OPT_CHECK(RBX_QUIET)) {
			printf("\nFreeBSD/x86 boot\n"
			    "Default: %u:%s(%up%u)%s\n"
			    "boot: ",
			    dsk.drive & DRV_MASK, dev_nm[dsk.type], dsk.unit,
			    dsk.part, kname);
		}
		if (ioctrl & IO_SERIAL)
			sio_flush();
		*cmd = '\0';
		if (keyhit(0))
			getstr(cmd, sizeof(cmd));
		else if (!OPT_CHECK(RBX_QUIET))
			putchar('\n');
		if (parse(cmd, &dskupdated)) {
			putchar('\a');
			continue;
		}
		if (dskupdated && gptinit() != 0)
			continue;
		load();
	}
	/* NOTREACHED */
}

/* XXX - Needed for btxld to link the boot2 binary; do not remove. */
void
exit(int x)
{
}

static void
load(void)
{
    union {
	struct exec ex;
	Elf32_Ehdr eh;
    } hdr;
    static Elf32_Phdr ep[2];
    static Elf32_Shdr es[2];
    caddr_t p;
    ufs_ino_t ino;
    uint32_t addr, x;
    int fmt, i, j;

    if (!(ino = lookup(kname))) {
	if (!ls) {
	    printf("%s: No %s on %u:%s(%up%u)\n", BOOTPROG,
		kname, dsk.drive & DRV_MASK, dev_nm[dsk.type], dsk.unit,
		dsk.part);
	}
	return;
    }
    if (xfsread(ino, &hdr, sizeof(hdr)))
	return;
    if (N_GETMAGIC(hdr.ex) == ZMAGIC)
	fmt = 0;
    else if (IS_ELF(hdr.eh))
	fmt = 1;
    else {
	printf("Invalid %s\n", "format");
	return;
    }
    if (fmt == 0) {
	addr = hdr.ex.a_entry & 0xffffff;
	p = PTOV(addr);
	fs_off = PAGE_SIZE;
	if (xfsread(ino, p, hdr.ex.a_text))
	    return;
	p += roundup2(hdr.ex.a_text, PAGE_SIZE);
	if (xfsread(ino, p, hdr.ex.a_data))
	    return;
	p += hdr.ex.a_data + roundup2(hdr.ex.a_bss, PAGE_SIZE);
	bootinfo.bi_symtab = VTOP(p);
	memcpy(p, &hdr.ex.a_syms, sizeof(hdr.ex.a_syms));
	p += sizeof(hdr.ex.a_syms);
	if (hdr.ex.a_syms) {
	    if (xfsread(ino, p, hdr.ex.a_syms))
		return;
	    p += hdr.ex.a_syms;
	    if (xfsread(ino, p, sizeof(int)))
		return;
	    x = *(uint32_t *)p;
	    p += sizeof(int);
	    x -= sizeof(int);
	    if (xfsread(ino, p, x))
		return;
	    p += x;
	}
    } else {
	fs_off = hdr.eh.e_phoff;
	for (j = i = 0; i < hdr.eh.e_phnum && j < 2; i++) {
	    if (xfsread(ino, ep + j, sizeof(ep[0])))
		return;
	    if (ep[j].p_type == PT_LOAD)
		j++;
	}
	for (i = 0; i < 2; i++) {
	    p = PTOV(ep[i].p_paddr & 0xffffff);
	    fs_off = ep[i].p_offset;
	    if (xfsread(ino, p, ep[i].p_filesz))
		return;
	}
	p += roundup2(ep[1].p_memsz, PAGE_SIZE);
	bootinfo.bi_symtab = VTOP(p);
	if (hdr.eh.e_shnum == hdr.eh.e_shstrndx + 3) {
	    fs_off = hdr.eh.e_shoff + sizeof(es[0]) *
		(hdr.eh.e_shstrndx + 1);
	    if (xfsread(ino, &es, sizeof(es)))
		return;
	    for (i = 0; i < 2; i++) {
		memcpy(p, &es[i].sh_size, sizeof(es[i].sh_size));
		p += sizeof(es[i].sh_size);
		fs_off = es[i].sh_offset;
		if (xfsread(ino, p, es[i].sh_size))
		    return;
		p += es[i].sh_size;
	    }
	}
	addr = hdr.eh.e_entry & 0xffffff;
    }
    bootinfo.bi_esymtab = VTOP(p);
    bootinfo.bi_kernelname = VTOP(kname);
    bootinfo.bi_bios_dev = dsk.drive;
    __exec((caddr_t)addr, RB_BOOTINFO | (opts & RBX_MASK),
	   MAKEBOOTDEV(dev_maj[dsk.type], dsk.part + 1, dsk.unit, 0xff),
	   0, 0, 0, VTOP(&bootinfo));
}

static int
parse(char *cmdstr, int *dskupdated)
{
    char *arg = cmdstr;
    char *ep, *p, *q;
    const char *cp;
    unsigned int drv;
    int c, i, j;

    *dskupdated = 0;
    while ((c = *arg++)) {
	if (c == ' ' || c == '\t' || c == '\n')
	    continue;
	for (p = arg; *p && *p != '\n' && *p != ' ' && *p != '\t'; p++);
	ep = p;
	if (*p)
	    *p++ = 0;
	if (c == '-') {
	    while ((c = *arg++)) {
		if (c == 'P') {
		    if (*(uint8_t *)PTOV(0x496) & 0x10) {
			cp = "yes";
		    } else {
			opts |= OPT_SET(RBX_DUAL) | OPT_SET(RBX_SERIAL);
			cp = "no";
		    }
		    printf("Keyboard: %s\n", cp);
		    continue;
		} else if (c == 'S') {
		    j = 0;
		    while ((unsigned int)(i = *arg++ - '0') <= 9)
			j = j * 10 + i;
		    if (j > 0 && i == -'0') {
			comspeed = j;
			break;
		    }
		    /* Fall through to error below ('S' not in optstr[]). */
		}
		for (i = 0; c != optstr[i]; i++)
		    if (i == NOPT - 1)
			return -1;
		opts ^= OPT_SET(flags[i]);
	    }
	    ioctrl = OPT_CHECK(RBX_DUAL) ? (IO_SERIAL|IO_KEYBOARD) :
		     OPT_CHECK(RBX_SERIAL) ? IO_SERIAL : IO_KEYBOARD;
	    if (ioctrl & IO_SERIAL) {
	        if (sio_init(115200 / comspeed) != 0)
		    ioctrl &= ~IO_SERIAL;
	    }
	} else {
	    for (q = arg--; *q && *q != '('; q++);
	    if (*q) {
		drv = -1;
		if (arg[1] == ':') {
		    drv = *arg - '0';
		    if (drv > 9)
			return (-1);
		    arg += 2;
		}
		if (q - arg != 2)
		    return -1;
		for (i = 0; arg[0] != dev_nm[i][0] ||
			    arg[1] != dev_nm[i][1]; i++)
		    if (i == NDEV - 1)
			return -1;
		dsk.type = i;
		arg += 3;
		dsk.unit = *arg - '0';
		if (arg[1] != 'p' || dsk.unit > 9)
		    return -1;
		arg += 2;
		dsk.part = *arg - '0';
		if (dsk.part < 1 || dsk.part > 9)
		    return -1;
		arg++;
		if (arg[0] != ')')
		    return -1;
		arg++;
		if (drv == -1)
		    drv = dsk.unit;
		dsk.drive = (dsk.type <= TYPE_MAXHARD
			     ? DRV_HARD : 0) + drv;
		*dskupdated = 1;
	    }
	    if ((i = ep - arg)) {
		if ((size_t)i >= sizeof(kname))
		    return -1;
		memcpy(kname, arg, i + 1);
	    }
	}
	arg = p;
    }
    return 0;
}

static int
dskread(void *buf, daddr_t lba, unsigned nblk)
{

	return drvread(&dsk, buf, lba + dsk.start, nblk);
}
