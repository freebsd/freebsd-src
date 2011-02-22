/*-
 * Copyright (c) 2008-2009 TAKAHASHI Yoshihiro
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
#include <sys/diskpc98.h>
#include <sys/dirent.h>
#include <sys/reboot.h>

#include <machine/bootinfo.h>
#include <machine/cpufunc.h>
#include <machine/elf.h>
#include <machine/psl.h>

#include <stdarg.h>

#include <a.out.h>

#include <btxv86.h>

#include "boot2.h"
#include "lib.h"

#define IO_KEYBOARD	1
#define IO_SERIAL	2

#define SECOND		1	/* Circa that many ticks in a second. */

#define RBX_ASKNAME	0x0	/* -a */
#define RBX_SINGLE	0x1	/* -s */
/* 0x2 is reserved for log2(RB_NOSYNC). */
/* 0x3 is reserved for log2(RB_HALT). */
/* 0x4 is reserved for log2(RB_INITNAME). */
#define RBX_DFLTROOT	0x5	/* -r */
#define RBX_KDB 	0x6	/* -d */
/* 0x7 is reserved for log2(RB_RDONLY). */
/* 0x8 is reserved for log2(RB_DUMP). */
/* 0x9 is reserved for log2(RB_MINIROOT). */
#define RBX_CONFIG	0xa	/* -c */
#define RBX_VERBOSE	0xb	/* -v */
#define RBX_SERIAL	0xc	/* -h */
#define RBX_CDROM	0xd	/* -C */
/* 0xe is reserved for log2(RB_POWEROFF). */
#define RBX_GDB 	0xf	/* -g */
#define RBX_MUTE	0x10	/* -m */
/* 0x11 is reserved for log2(RB_SELFTEST). */
/* 0x12 is reserved for boot programs. */
/* 0x13 is reserved for boot programs. */
#define RBX_PAUSE	0x14	/* -p */
#define RBX_QUIET	0x15	/* -q */
#define RBX_NOINTR	0x1c	/* -n */
/* 0x1d is reserved for log2(RB_MULTIPLE) and is just misnamed here. */
#define RBX_DUAL	0x1d	/* -D */
/* 0x1f is reserved for log2(RB_BOOTINFO). */

/* pass: -a, -s, -r, -d, -c, -v, -h, -C, -g, -m, -p, -D */
#define RBX_MASK	(OPT_SET(RBX_ASKNAME) | OPT_SET(RBX_SINGLE) | \
			OPT_SET(RBX_DFLTROOT) | OPT_SET(RBX_KDB ) | \
			OPT_SET(RBX_CONFIG) | OPT_SET(RBX_VERBOSE) | \
			OPT_SET(RBX_SERIAL) | OPT_SET(RBX_CDROM) | \
			OPT_SET(RBX_GDB ) | OPT_SET(RBX_MUTE) | \
			OPT_SET(RBX_PAUSE) | OPT_SET(RBX_DUAL))

#define PATH_CONFIG	"/boot.config"
#define PATH_BOOT3	"/boot/loader"
#define PATH_KERNEL	"/boot/kernel/kernel"

#define ARGS		0x900
#define NOPT		14
#define NDEV		3
#define V86_CY(x)	((x) & PSL_C)
#define V86_ZR(x)	((x) & PSL_Z)

#define DRV_DISK	0xf0
#define DRV_UNIT	0x0f

#define TYPE_AD		0
#define TYPE_DA		1
#define TYPE_FD		2

#define OPT_SET(opt)	(1 << (opt))
#define OPT_CHECK(opt)	((opts) & OPT_SET(opt))

extern uint32_t _end;

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

static const char *const dev_nm[NDEV] = {"ad", "da", "fd"};
static const unsigned char dev_maj[NDEV] = {30, 4, 2};
static const unsigned char dev_daua[NDEV] = {0x80, 0xa0, 0x90};

static struct dsk {
    unsigned daua;
    unsigned type;
    unsigned disk;
    unsigned unit;
    unsigned head;
    unsigned sec;
    unsigned slice;
    unsigned part;
    unsigned start;
} dsk;
static char cmd[512], cmddup[512];
static char kname[1024];
static uint32_t opts;
static int comspeed = SIOSPD;
static struct bootinfo bootinfo;
static uint8_t ioctrl = IO_KEYBOARD;

void exit(int);
static void load(void);
static int parse(void);
static int xfsread(ino_t, void *, size_t);
static int dskread(void *, unsigned, unsigned);
static void printf(const char *,...);
static void putchar(int);
static uint32_t memsize(void);
static int drvread(void *, unsigned);
static int keyhit(unsigned);
static int xputc(int);
static int xgetc(int);
static int getc(int);

static void memcpy(void *, const void *, int);
static void
memcpy(void *dst, const void *src, int len)
{
    const char *s = src;
    char *d = dst;

    while (len--)
        *d++ = *s++;
}

static inline int
strcmp(const char *s1, const char *s2)
{
    for (; *s1 == *s2 && *s1; s1++, s2++);
    return (unsigned char)*s1 - (unsigned char)*s2;
}

#define	UFS_SMALL_CGBASE
#include "ufsread.c"

static inline int
xfsread(ino_t inode, void *buf, size_t nbyte)
{
    if ((size_t)fsread(inode, buf, nbyte) != nbyte) {
	printf("Invalid %s\n", "format");
	return -1;
    }
    return 0;
}

static inline uint32_t
memsize(void)
{
    return (*(u_char *)PTOV(0x401) * 128 * 1024 +
	*(uint16_t *)PTOV(0x594) * 1024 * 1024);
}

static inline void
getstr(void)
{
    char *s;
    int c;

    s = cmd;
    for (;;) {
	switch (c = xgetc(0)) {
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
	    putchar(c);
	}
    }
}

static inline void
putc(int c)
{

    v86.ctl = V86_ADDR | V86_CALLF | V86_FLAGS;
    v86.addr = PUTCORG;		/* call to putc in boot1 */
    v86.eax = c;
    v86int();
    v86.ctl = V86_FLAGS;
}

static inline int
is_scsi_hd(void)
{

    if ((*(u_char *)PTOV(0x482) >> dsk.unit) & 0x01)
	return 1;

    return 0;
}

static inline void
fix_sector_size(void)
{
    u_char *p;

    p = (u_char *)PTOV(0x460 + dsk.unit * 4);	/* SCSI equipment parameter */

    if ((p[0] & 0x1f) == 7) {		/* SCSI MO */
	if (!(p[3] & 0x30)) {		/* 256B / sector */
	    p[3] |= 0x10;		/* forced set 512B / sector */
	    p[3 + 0xa1000] |= 0x10;
	}
    }
}

static inline uint32_t
get_diskinfo(void)
{

    if (dsk.disk == 0x30) {				/* 1440KB FD */
	/* 80 cylinders, 2 heads, 18 sectors */
	return (80 << 16) | (2 << 8) | 18;
    } else if (dsk.disk == 0x90) {			/* 1200KB FD */
	/* 80 cylinders, 2 heads, 15 sectors */
	return (80 << 16) | (2 << 8) | 15;
    } else if (dsk.disk == 0x80 || is_scsi_hd()) {	/* IDE or SCSI HDD */
	v86.addr = 0x1b;
	v86.eax = 0x8400 | dsk.daua;
	v86int();
	return (v86.ecx << 16) | v86.edx;
    }

    /* SCSI MO or CD */
    fix_sector_size();	/* SCSI MO */

    /* other SCSI devices */
    return (65535 << 16) | (8 << 8) | 32;
}

static void
set_dsk(void)
{
    uint32_t di;

    di = get_diskinfo();

    dsk.head = (di >> 8) & 0xff;
    dsk.sec = di & 0xff;
    dsk.start = 0;
}

#ifdef GET_BIOSGEOM
static uint32_t
bd_getbigeom(int bunit)
{
    int hds = 0;
    int unit = 0x80;		/* IDE HDD */
    u_int addr = 0x55d;

    while (unit < 0xa7) {
	if (*(u_char *)PTOV(addr) & (1 << (unit & 0x0f)))
	    if (hds++ == bunit)
		break;

	if (unit >= 0xA0) {
	    int media = ((unsigned *)PTOV(0x460))[unit & 0x0F] & 0x1F;

	    if (media == 7 && hds++ == bunit)	/* SCSI MO */
		return(0xFFFE0820); /* C:65535 H:8 S:32 */
	}
	if (++unit == 0x84) {
	    unit = 0xA0;	/* SCSI HDD */
	    addr = 0x482;
	}
    }
    if (unit == 0xa7)
	return 0x4F020F;	/* 1200KB FD C:80 H:2 S:15 */
    v86.addr = 0x1b;
    v86.eax = 0x8400 | unit;
    v86int();
    if (v86.efl & 0x1)
	return 0x4F020F;	/* 1200KB FD C:80 H:2 S:15 */
    return ((v86.ecx & 0xffff) << 16) | (v86.edx & 0xffff);
}
#endif

static int
check_slice(void)
{
    struct pc98_partition *dp;
    char *sec;
    unsigned i, cyl;

    sec = dmadat->secbuf;
    cyl = *(uint16_t *)PTOV(ARGS);
    set_dsk();

    if (dsk.type == TYPE_FD)
	return (WHOLE_DISK_SLICE);
    if (drvread(sec, DOSBBSECTOR + 1))
	return (WHOLE_DISK_SLICE);	/* Read error */
    dp = (void *)(sec + DOSPARTOFF);
    for (i = 0; i < NDOSPART; i++) {
	if (dp[i].dp_mid == DOSMID_386BSD) {
	    if (dp[i].dp_scyl <= cyl && cyl <= dp[i].dp_ecyl)
		return (BASE_SLICE + i);
	}
    }

    return (WHOLE_DISK_SLICE);
}

int
main(void)
{
#ifdef GET_BIOSGEOM
    int i;
#endif
    uint8_t autoboot;
    ino_t ino;

    dmadat = (void *)(roundup2(__base + (int32_t)&_end, 0x10000) - __base);
    v86.ctl = V86_FLAGS;
    v86.efl = PSL_RESERVED_DEFAULT | PSL_I;
    dsk.daua = *(uint8_t *)PTOV(0x584);
    dsk.disk = dsk.daua & DRV_DISK;
    dsk.unit = dsk.daua & DRV_UNIT;
    if (dsk.disk == 0x80)
        dsk.type = TYPE_AD;
    else if (dsk.disk == 0xa0)
        dsk.type = TYPE_DA;
    else /* if (dsk.disk == 0x30 || dsk.disk == 0x90) */
        dsk.type = TYPE_FD;
    dsk.slice = check_slice();
#ifdef GET_BIOSGEOM
    for (i = 0; i < N_BIOS_GEOM; i++)
	bootinfo.bi_bios_geom[i] = bd_getbigeom(i);
#endif
    bootinfo.bi_version = BOOTINFO_VERSION;
    bootinfo.bi_size = sizeof(bootinfo);
    bootinfo.bi_basemem = 0;	/* XXX will be filled by loader or kernel */
    bootinfo.bi_extmem = memsize();
    bootinfo.bi_memsizes_valid++;

    /* Process configuration file */

    autoboot = 1;

    if ((ino = lookup(PATH_CONFIG)))
	fsread(ino, cmd, sizeof(cmd));

    if (*cmd) {
	memcpy(cmddup, cmd, sizeof(cmd));
	if (parse())
	    autoboot = 0;
	if (!OPT_CHECK(RBX_QUIET))
	    printf("%s: %s", PATH_CONFIG, cmddup);
	/* Do not process this command twice */
	*cmd = 0;
    }

    /*
     * Try to exec stage 3 boot loader. If interrupted by a keypress,
     * or in case of failure, try to load a kernel directly instead.
     */

    if (autoboot && !*kname) {
	memcpy(kname, PATH_BOOT3, sizeof(PATH_BOOT3));
	if (!keyhit(3*SECOND)) {
	    load();
	    memcpy(kname, PATH_KERNEL, sizeof(PATH_KERNEL));
	}
    }

    /* Present the user with the boot2 prompt. */

    for (;;) {
	if (!autoboot || !OPT_CHECK(RBX_QUIET))
	    printf("\nFreeBSD/pc98 boot\n"
		   "Default: %u:%s(%u,%c)%s\n"
		   "boot: ",
		   dsk.unit, dev_nm[dsk.type], dsk.unit,
		   'a' + dsk.part, kname);
	if (ioctrl & IO_SERIAL)
	    sio_flush();
	if (!autoboot || keyhit(5*SECOND))
	    getstr();
	else if (!autoboot || !OPT_CHECK(RBX_QUIET))
	    putchar('\n');
	autoboot = 0;
	if (parse())
	    putchar('\a');
	else
	    load();
    }
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
    ino_t ino;
    uint32_t addr, x;
    int i, j;
    uint8_t fmt;

    if (!(ino = lookup(kname))) {
	if (!ls)
	    printf("No %s\n", kname);
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
		*(Elf32_Word *)p = es[i].sh_size;
		p += sizeof(es[i].sh_size);
		fs_off = es[i].sh_offset;
		if (xfsread(ino, p, es[i].sh_size))
		    return;
		p += es[i].sh_size;
	    }
	}
	addr = hdr.eh.e_entry & 0xffffff;
	bootinfo.bi_esymtab = VTOP(p);
    }
    bootinfo.bi_kernelname = VTOP(kname);
    bootinfo.bi_bios_dev = dsk.daua;
    __exec((caddr_t)addr, RB_BOOTINFO | (opts & RBX_MASK),
	   MAKEBOOTDEV(dev_maj[dsk.type], dsk.slice, dsk.unit, dsk.part),
	   0, 0, 0, VTOP(&bootinfo));
}

static int
parse()
{
    char *arg = cmd;
    char *ep, *p, *q;
    const char *cp;
    unsigned int drv;
    int c, i, j;

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
		    if (*(uint8_t *)PTOV(0x481) & 0x48) {
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
	    if (ioctrl & IO_SERIAL)
	        sio_init(115200 / comspeed);
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
		if (arg[1] != ',' || dsk.unit > 9)
		    return -1;
		arg += 2;
		dsk.slice = WHOLE_DISK_SLICE;
		if (arg[1] == ',') {
		    dsk.slice = *arg - '0' + 1;
		    if (dsk.slice > NDOSPART + 1)
			return -1;
		    arg += 2;
		}
		if (arg[1] != ')')
		    return -1;
		dsk.part = *arg - 'a';
		if (dsk.part > 7)
		    return (-1);
		arg += 2;
		if (drv == -1)
		    drv = dsk.unit;
		dsk.disk = dev_daua[dsk.type];
		dsk.daua = dsk.disk | dsk.unit;
		dsk_meta = 0;
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
dskread(void *buf, unsigned lba, unsigned nblk)
{
    struct pc98_partition *dp;
    struct disklabel *d;
    char *sec;
    unsigned sl, i;
    u_char *p;

    if (!dsk_meta) {
	sec = dmadat->secbuf;
	set_dsk();
	if (dsk.type == TYPE_FD)
	    goto unsliced;
	if (drvread(sec, DOSBBSECTOR + 1))
	    return -1;
	dp = (void *)(sec + DOSPARTOFF);
	sl = dsk.slice;
	if (sl < BASE_SLICE) {
	    for (i = 0; i < NDOSPART; i++)
		if (dp[i].dp_mid == DOSMID_386BSD) {
		    sl = BASE_SLICE + i;
		    break;
		}
	    dsk.slice = sl;
	}
	if (sl != WHOLE_DISK_SLICE) {
	    dp += sl - BASE_SLICE;
	    if (dp->dp_mid != DOSMID_386BSD) {
		printf("Invalid %s\n", "slice");
		return -1;
	    }
	    dsk.start = dp->dp_scyl * dsk.head * dsk.sec +
		dp->dp_shd * dsk.sec + dp->dp_ssect;
	}
	if (drvread(sec, dsk.start + LABELSECTOR))
		return -1;
	d = (void *)(sec + LABELOFFSET);
	if (d->d_magic != DISKMAGIC || d->d_magic2 != DISKMAGIC) {
	    if (dsk.part != RAW_PART) {
		printf("Invalid %s\n", "label");
		return -1;
	    }
	} else {
	    if (dsk.part >= d->d_npartitions ||
		!d->d_partitions[dsk.part].p_size) {
		printf("Invalid %s\n", "partition");
		return -1;
	    }
	    dsk.start += d->d_partitions[dsk.part].p_offset;
	    dsk.start -= d->d_partitions[RAW_PART].p_offset;
	}
    unsliced: ;
    }
    for (p = buf; nblk; p += 512, lba++, nblk--) {
	if ((i = drvread(p, dsk.start + lba)))
	    return i;
    }
    return 0;
}

static void
printf(const char *fmt,...)
{
    va_list ap;
    char buf[10];
    char *s;
    unsigned u;
    int c;

    va_start(ap, fmt);
    while ((c = *fmt++)) {
	if (c == '%') {
	    c = *fmt++;
	    switch (c) {
	    case 'c':
		putchar(va_arg(ap, int));
		continue;
	    case 's':
		for (s = va_arg(ap, char *); *s; s++)
		    putchar(*s);
		continue;
	    case 'u':
		u = va_arg(ap, unsigned);
		s = buf;
		do
		    *s++ = '0' + u % 10U;
		while (u /= 10U);
		while (--s >= buf)
		    putchar(*s);
		continue;
	    }
	}
	putchar(c);
    }
    va_end(ap);
    return;
}

static void
putchar(int c)
{
    if (c == '\n')
	xputc('\r');
    xputc(c);
}

static int
drvread(void *buf, unsigned lba)
{
    static unsigned c = 0x2d5c7c2f;
    unsigned bpc, x, cyl, head, sec;

    bpc = dsk.sec * dsk.head;
    cyl = lba / bpc;
    x = lba % bpc;
    head = x / dsk.sec;
    sec = x % dsk.sec;

    if (!OPT_CHECK(RBX_QUIET))
	printf("%c\b", c = c << 8 | c >> 24);
    v86.ctl = V86_ADDR | V86_CALLF | V86_FLAGS;
    v86.addr = READORG;		/* call to read in boot1 */
    v86.ecx = cyl;
    v86.edx = (head << 8) | sec;
    v86.edi = lba;
    v86.ebx = 512;
    v86.es = VTOPSEG(buf);
    v86.ebp = VTOPOFF(buf);
    v86int();
    v86.ctl = V86_FLAGS;
    if (V86_CY(v86.efl)) {
	printf("error %u c/h/s %u/%u/%u lba %u\n", v86.eax >> 8 & 0xff,
	       cyl, head, sec, lba);
	return -1;
    }
    return 0;
}

static inline void
delay(void)
{
    int i;

    i = 800;
    do {
	outb(0x5f, 0);	/* about 600ns */
    } while (--i >= 0);
}

static int
keyhit(unsigned sec)
{
    unsigned i;

    if (OPT_CHECK(RBX_NOINTR))
	return 0;
    for (i = 0; i < sec * 1000; i++) {
	if (xgetc(1))
	    return 1;
	delay();
    }
    return 0;
}

static int
xputc(int c)
{
    if (ioctrl & IO_KEYBOARD)
	putc(c);
    if (ioctrl & IO_SERIAL)
	sio_putc(c);
    return c;
}

static int
xgetc(int fn)
{
    if (OPT_CHECK(RBX_NOINTR))
	return 0;
    for (;;) {
	if (ioctrl & IO_KEYBOARD && getc(1))
	    return fn ? 1 : getc(0);
	if (ioctrl & IO_SERIAL && sio_ischar())
	    return fn ? 1 : sio_getc();
	if (fn)
	    return 0;
    }
}

static int
getc(int fn)
{
    v86.addr = 0x18;
    v86.eax = fn << 8;
    v86int();
    if (fn)
	return (v86.ebx >> 8) & 0x01;
    else
	return v86.eax & 0xff;
}
