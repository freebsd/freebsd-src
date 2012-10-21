/*-
 * Copyright (c) 2008 John Hay
 * Copyright (c) 2006 Warner Losh
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
#include <sys/disklabel.h>
#include <sys/diskmbr.h>
#include <sys/dirent.h>
#include <sys/reboot.h>

#include <machine/elf.h>

#include <stdarg.h>

#include "lib.h"
#include "board.h"

#define RBX_ASKNAME	0x0	/* -a */
#define RBX_SINGLE	0x1	/* -s */
/* 0x2 is reserved for log2(RB_NOSYNC). */
/* 0x3 is reserved for log2(RB_HALT). */
/* 0x4 is reserved for log2(RB_INITNAME). */
#define RBX_DFLTROOT	0x5	/* -r */
/* #define RBX_KDB 	0x6	   -d */
/* 0x7 is reserved for log2(RB_RDONLY). */
/* 0x8 is reserved for log2(RB_DUMP). */
/* 0x9 is reserved for log2(RB_MINIROOT). */
#define RBX_CONFIG	0xa	/* -c */
#define RBX_VERBOSE	0xb	/* -v */
/* #define RBX_SERIAL	0xc	   -h */
/* #define RBX_CDROM	0xd	   -C */
/* 0xe is reserved for log2(RB_POWEROFF). */
#define RBX_GDB 	0xf	/* -g */
/* #define RBX_MUTE	0x10	   -m */
/* 0x11 is reserved for log2(RB_SELFTEST). */
/* 0x12 is reserved for boot programs. */
/* 0x13 is reserved for boot programs. */
/* #define RBX_PAUSE	0x14	   -p */
/* #define RBX_QUIET	0x15	   -q */
#define RBX_NOINTR	0x1c	/* -n */
/* 0x1d is reserved for log2(RB_MULTIPLE) and is just misnamed here. */
/* #define RBX_DUAL	0x1d	   -D */
/* 0x1f is reserved for log2(RB_BOOTINFO). */

/* pass: -a, -s, -r, -v, -g */
#define RBX_MASK	(OPT_SET(RBX_ASKNAME) | OPT_SET(RBX_SINGLE) | \
			OPT_SET(RBX_DFLTROOT) | \
			OPT_SET(RBX_VERBOSE) | \
			OPT_SET(RBX_GDB))

#define PATH_DOTCONFIG	"/boot.config"
#define PATH_CONFIG	"/boot/config"
//#define PATH_KERNEL	"/boot/kernel/kernel"
#define PATH_KERNEL	"/boot/kernel/kernel.gz.tramp"

extern uint32_t _end;

#define NOPT		6

#define OPT_SET(opt)	(1 << (opt))
#define OPT_CHECK(opt)	((opts) & OPT_SET(opt))

static const char optstr[NOPT] = "agnrsv";
static const unsigned char flags[NOPT] = {
	RBX_ASKNAME,
	RBX_GDB,
	RBX_NOINTR,
	RBX_DFLTROOT,
	RBX_SINGLE,
	RBX_VERBOSE
};

unsigned board_id; /* board type to pass to kernel, if set by board_* code */
unsigned dsk_start;
static char cmd[512];
static char kname[1024];
static uint32_t opts;
static uint8_t dsk_meta;

static void load(void);
static int parse(void);
static int dskread(void *, unsigned, unsigned);
#ifdef FIXUP_BOOT_DRV
static void fixup_boot_drv(caddr_t, int, int, int);
#endif

#define	UFS_SMALL_CGBASE
#include "ufsread.c"

#ifdef DEBUG
#define	DPRINTF(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define	DPRINTF(fmt, ...)
#endif

static inline int
xfsread(ufs_ino_t inode, void *buf, size_t nbyte)
{
	if ((size_t)fsread(inode, buf, nbyte) != nbyte)
		return -1;
	return 0;
}

static inline void
getstr(int c)
{
	char *s;

	s = cmd;
	if (c == 0)
		c = getc(10000);
	for (;;) {
		switch (c) {
		case 0:
			break;
		case '\177':
		case '\b':
			if (s > cmd) {
				s--;
				printf("\b \b");
			}
			break;
		case '\n':
		case '\r':
			*s = 0;
			return;
		default:
			if (s - cmd < sizeof(cmd) - 1)
				*s++ = c;
			xputchar(c);
		}
		c = getc(10000);
	}
}

int
main(void)
{
	int autoboot, c = 0;
	ufs_ino_t ino;

	dmadat = (void *)(0x20000000 + (16 << 20));
	board_init();

	autoboot = 1;

	/* Process configuration file */
	if ((ino = lookup(PATH_CONFIG)) ||
	    (ino = lookup(PATH_DOTCONFIG)))
		fsread(ino, cmd, sizeof(cmd));

	if (*cmd) {
		if (parse())
			autoboot = 0;
		printf("%s: %s\n", PATH_CONFIG, cmd);
		/* Do not process this command twice */
		*cmd = 0;
	}

	if (*kname == '\0')
		strcpy(kname, PATH_KERNEL);

	/* Present the user with the boot2 prompt. */
	for (;;) {
		printf("\nDefault: %s\nboot: ", kname);
		if (!autoboot ||
		    (OPT_CHECK(RBX_NOINTR) == 0 && (c = getc(2)) != 0))
			getstr(c);
		xputchar('\n');
		autoboot = 0;
		c = 0;
		if (parse())
			xputchar('\a');
		else
			load();
	}
}

static void
load(void)
{
	Elf32_Ehdr eh;
	static Elf32_Phdr ep[2];
	caddr_t p;
	ufs_ino_t ino;
	uint32_t addr;
	int i, j;
#ifdef FIXUP_BOOT_DRV
	caddr_t staddr;
	int klen;

	staddr = (caddr_t)0xffffffff;
	klen = 0;
#endif
	if (!(ino = lookup(kname))) {
		if (!ls)
			printf("No %s\n", kname);
		return;
	}
	if (xfsread(ino, &eh, sizeof(eh)))
		return;
	if (!IS_ELF(eh)) {
		printf("Invalid %s\n", "format");
		return;
	}
	fs_off = eh.e_phoff;
	for (j = i = 0; i < eh.e_phnum && j < 2; i++) {
		if (xfsread(ino, ep + j, sizeof(ep[0])))
			return;
		if (ep[j].p_type == PT_LOAD)
			j++;
	}
	for (i = 0; i < 2; i++) {
		p = (caddr_t)ep[i].p_paddr;
		fs_off = ep[i].p_offset;
#ifdef FIXUP_BOOT_DRV
		if (staddr == (caddr_t)0xffffffff)
			staddr = p;
		klen += ep[i].p_filesz;
#endif
		if (xfsread(ino, p, ep[i].p_filesz))
			return;
	}
	addr = eh.e_entry;
#ifdef FIXUP_BOOT_DRV
	fixup_boot_drv(staddr, klen, bootslice, bootpart);
#endif
	((void(*)(int, int, int, int))addr)(opts & RBX_MASK, board_id, 0, 0);
}

static int
parse()
{
	char *arg = cmd;
	char *ep, *p;
	int c, i;

	while ((c = *arg++)) {
		if (c == ' ' || c == '\t' || c == '\n')
			continue;
		for (p = arg; *p && *p != '\n' && *p != ' ' && *p != '\t'; p++);
		ep = p;
		if (*p)
			*p++ = 0;
		if (c == '-') {
			while ((c = *arg++)) {
				for (i = 0; c != optstr[i]; i++)
					if (i == NOPT - 1)
						return -1;
				opts ^= OPT_SET(flags[i]);
			}
		} else {
			arg--;
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
dskread(void *buf, unsigned lba, unsigned nblk)
{
	struct dos_partition *dp;
	struct disklabel *d;
	char *sec;
	int i;

	if (!dsk_meta) {
		sec = dmadat->secbuf;
		dsk_start = 0;
		if (drvread(sec, DOSBBSECTOR, 1))
			return -1;
		dp = (void *)(sec + DOSPARTOFF);
		for (i = 0; i < NDOSPART; i++) {
			if (dp[i].dp_typ == DOSPTYP_386BSD)
				break;
		}
		if (i == NDOSPART)
			return -1;
		/*
		 * Although dp_start is aligned within the disk
		 * partition structure, DOSPARTOFF is 446, which is
		 * only word (2) aligned, not longword (4) aligned.
		 * Cope by using memcpy to fetch the start of this
		 * partition.
		 */
		memcpy(&dsk_start, &dp[1].dp_start, 4);
		if (drvread(sec, dsk_start + LABELSECTOR, 1))
			return -1;
		d = (void *)(sec + LABELOFFSET);
		if (d->d_magic != DISKMAGIC || d->d_magic2 != DISKMAGIC) {
			printf("Invalid %s\n", "label");
			return -1;
		}
		if (!d->d_partitions[0].p_size) {
			printf("Invalid %s\n", "partition");
			return -1;
		}
		dsk_start += d->d_partitions[0].p_offset;
		dsk_start -= d->d_partitions[RAW_PART].p_offset;
		dsk_meta++;
	}
	return drvread(buf, dsk_start + lba, nblk);
}

#ifdef FIXUP_BOOT_DRV
/*
 * fixup_boot_drv() will try to find the ROOTDEVNAME spec in the kernel
 * and change it to what was specified on the comandline or /boot.conf
 * file or to what was encountered on the disk. It will try to handle 3
 * different disk layouts, raw (dangerously dedicated), slice only and
 * slice + partition. It will look for the following strings in the
 * kernel, but if it is one of the first three, the string in the kernel
 * must use the correct form to match the actual disk layout:
 * - ufs:ad0a
 * - ufs:ad0s1
 * - ufs:ad0s1a
 * - ufs:ROOTDEVNAME
 * In the case of the first three strings, only the "a" at the end and
 * the "1" after the "s" will be modified, if they exist. The string
 * length will not be changed. In the case of the last string, the
 * whole string will be built up and nul, '\0' terminated.
 */
static void
fixup_boot_drv(caddr_t addr, int klen, int bs, int bp)
{
	const u_int8_t op[] = "ufs:ROOTDEVNAME";
	const u_int8_t op2[] = "ufs:ad0";
	u_int8_t *p, *ps;

	DPRINTF("fixup_boot_drv: 0x%x, %d, slice %d, partition %d\n",
	    (int)addr, klen, bs, bp);
	if (bs > 4)
		return;
	if (bp > 7)
		return;
	ps = memmem(addr, klen, op, sizeof(op));
	if (ps != NULL) {
		p = ps + 4;	/* past ufs: */
		DPRINTF("Found it at 0x%x\n", (int)ps);
		p[0] = 'a'; p[1] = 'd'; p[2] = '0';	/* ad0 */
		p += 3;
		if (bs > 0) {
			/* append slice */
			*p++ = 's';
			*p++ = bs + '0';
		}
		if (disk_layout != DL_SLICE) {
			/* append partition */
			*p++ = bp + 'a';
		}
		*p = '\0';
	} else {
		ps = memmem(addr, klen, op2, sizeof(op2) - 1);
		if (ps != NULL) {
			p = ps + sizeof(op2) - 1;
			DPRINTF("Found it at 0x%x\n", (int)ps);
			if (*p == 's') {
				/* fix slice */
				p++;
				*p++ = bs + '0';
			}
			if (*p == 'a')
				*p = bp + 'a';
		}
	}
	if (ps == NULL) {
		printf("Could not locate \"%s\" to fix kernel boot device, "
		     "check ROOTDEVNAME is set\n", op);
		return;
	}
	DPRINTF("Changed boot device to %s\n", ps);
}
#endif
