/*
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

/*
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/diskslice.h>
#include <sys/disklabel.h>
#include <sys/dirent.h>
#include <machine/bootinfo.h>
#include <machine/elf.h>

#include <ufs/ffs/fs.h>
#include <ufs/ufs/dinode.h>

#include <stdarg.h>

#include <a.out.h>

#include <btxv86.h>

#include "lib.h"

#define RBX_ASKNAME	0x0	/* -a */
#define RBX_SINGLE	0x1	/* -s */
#define RBX_DFLTROOT	0x5	/* -r */
#define RBX_KDB 	0x6	/* -d */
#define RBX_CONFIG	0xa	/* -c */
#define RBX_VERBOSE	0xb	/* -v */
#define RBX_SERIAL	0xc	/* -h */
#define RBX_CDROM	0xd	/* -C */
#define RBX_GDB 	0xf	/* -g */
#define RBX_DUAL	0x1d	/* -D */
#define RBX_PROBEKBD	0x1e	/* -P */

#define RBX_MASK	0xffff

#define PATH_CONFIG	"/boot.config"
#define PATH_BOOT3	"/boot/loader"
#define PATH_KERNEL	"/kernel"

#define ARGS		0x900
#define NOPT		11
#define BSIZEMAX	8192
#define NDEV		5
#define MEM_BASE	0x12
#define MEM_EXT 	0x15
#define V86_CY(x)	((x) & 1)
#define V86_ZR(x)	((x) & 0x40)

#define DRV_HARD	0x80
#define DRV_MASK	0x7f

#define TYPE_AD		0
#define TYPE_WD		1
#define TYPE_WFD 	2
#define TYPE_FD		3
#define TYPE_DA		4

extern uint32_t _end;

static const char optstr[NOPT] = "DhaCcdgPrsv";
static const unsigned char flags[NOPT] = {
    RBX_DUAL,
    RBX_SERIAL,
    RBX_ASKNAME,
    RBX_CDROM,
    RBX_CONFIG,
    RBX_KDB,
    RBX_GDB,
    RBX_PROBEKBD,
    RBX_DFLTROOT,
    RBX_SINGLE,
    RBX_VERBOSE
};

static const char *const dev_nm[] = {"ad", "wd", "  ", "fd", "da"};
static const unsigned dev_maj[] = {30, 0, 1, 2, 4};

static struct dsk {
    unsigned drive;
    unsigned type;
    unsigned unit;
    unsigned slice;
    unsigned part;
    unsigned start;
    int init;
    int meta;
} dsk;
static char cmd[512];
static char kname[1024];
static uint32_t opts;
static struct bootinfo bootinfo;
static int ls;
static uint32_t fs_off;
static uint8_t ioctrl = 0x1;

void exit(int);
static void load(const char *);
static int parse(char *);
static ino_t lookup(const char *);
static int xfsread(ino_t, void *, size_t);
static ssize_t fsread(ino_t, void *, size_t);
static int dskread(void *, unsigned, unsigned);
static int printf(const char *,...);
static int putchar(int);
static void *memcpy(void *, const void *, size_t);
static void *malloc(size_t);
static uint32_t memsize(int);
static int drvread(void *, unsigned, unsigned);
static int keyhit(unsigned);
static int xputc(int);
static int xgetc(int);
static int getc(int);

static inline void
readfile(const char *fname, void *buf, size_t size)
{
    ino_t ino;

    if ((ino = lookup(fname)))
	fsread(ino, buf, size);
}

static inline int
strcmp(const char *s1, const char *s2)
{
    for (; *s1 == *s2 && *s1; s1++, s2++);
    return (u_char)*s1 - (u_char)*s2;
}

static inline int
fsfind(const char *name, ino_t * ino)
{
    char buf[DEV_BSIZE];
    struct dirent *d;
    char *s;
    ssize_t n;

    fs_off = 0;
    while ((n = fsread(*ino, buf, DEV_BSIZE)) > 0)
	for (s = buf; s < buf + DEV_BSIZE;) {
	    d = (void *)s;
	    if (ls)
		printf("%s ", d->d_name);
	    else if (!strcmp(name, d->d_name)) {
		*ino = d->d_fileno;
		return d->d_type;
	    }
	    s += d->d_reclen;
	}
    if (n != -1 && ls)
	putchar('\n');
    return 0;
}

static inline int
getchar(void)
{
    int c;

    c = xgetc(0);
    if (c == '\r')
	c = '\n';
    return c;
}

static inline void
getstr(char *str, int size)
{
    char *s;
    int c;

    s = str;
    do {
	switch (c = getchar()) {
	case 0:
	    break;
	case '\b':
	case '\177':
	    if (s > str) {
		s--;
		putchar('\b');
		putchar(' ');
	    } else
		c = 0;
	    break;
	case '\n':
	    *s = 0;
	    break;
	default:
	    if (s - str < size - 1)
		*s++ = c;
	}
	if (c)
	    putchar(c);
    } while (c != '\n');
}

static inline uint32_t
drvinfo(int drive)
{
    v86.addr = 0x13;
    v86.eax = 0x800;
    v86.edx = DRV_HARD + drive;
    v86int();
    if (V86_CY(v86.efl))
	return 0x4f010f;
    return ((v86.ecx & 0xc0) << 18) | ((v86.ecx & 0xff00) << 8) |
	   (v86.edx & 0xff00) | (v86.ecx & 0x3f);
}

static inline void
putc(int c)
{
    v86.addr = 0x10;
    v86.eax = 0xe00 | (c & 0xff);
    v86.ebx = 0x7;
    v86int();
}

int
main(void)
{
    int autoboot, i;

    v86.ctl = V86_FLAGS;
    dsk.drive = *(uint8_t *)PTOV(ARGS);
    dsk.type = dsk.drive & DRV_HARD ? TYPE_AD : TYPE_FD;
    dsk.unit = dsk.drive & DRV_MASK;
    dsk.slice = *(uint8_t *)PTOV(ARGS + 1) + 1;
    bootinfo.bi_version = BOOTINFO_VERSION;
    bootinfo.bi_size = sizeof(bootinfo);
    bootinfo.bi_basemem = memsize(MEM_BASE);
    bootinfo.bi_extmem = memsize(MEM_EXT);
    bootinfo.bi_memsizes_valid++;
    for (i = 0; i < N_BIOS_GEOM; i++)
	bootinfo.bi_bios_geom[i] = drvinfo(i);
    autoboot = 2;
    readfile(PATH_CONFIG, cmd, sizeof(cmd));
    if (*cmd) {
	printf("%s: %s", PATH_CONFIG, cmd);
	if (parse(cmd))
	    autoboot = 0;
	*cmd = 0;
    }
    if (autoboot && !*kname) {
	if (autoboot == 2) {
	    memcpy(kname, PATH_BOOT3, sizeof(PATH_BOOT3));
	    if (!keyhit(0x37)) {
		load(kname);
		autoboot = 1;
	    }
	}
	if (autoboot == 1)
	    memcpy(kname, PATH_KERNEL, sizeof(PATH_KERNEL));
    }
    for (;;) {
	printf(" \n>> FreeBSD/i386 BOOT\n"
	       "Default: %u:%s(%u,%c)%s\n"
	       "boot: ",
	       dsk.drive & DRV_MASK, dev_nm[dsk.type], dsk.unit,
	       'a' + dsk.part, kname);
	if (ioctrl & 0x2)
	    sio_flush();
	if (!autoboot || keyhit(0x5a))
	    getstr(cmd, sizeof(cmd));
	else
	    putchar('\n');
	autoboot = 0;
	if (parse(cmd))
	    putchar('\a'); 
	else
	    load(kname);
    }
}

/* XXX - Needed for btxld to link the boot2 binary; do not remove. */
void
exit(int x)
{
}

static void
load(const char *fname)
{
    union {
	struct exec ex;
	Elf32_Ehdr eh;
    } hdr;
    Elf32_Phdr ep[2];
    Elf32_Shdr es[2];
    caddr_t p;
    ino_t ino;
    uint32_t addr, x;
    int fmt, i, j;

    if (!(ino = lookup(fname))) {
	if (!ls)
	    printf("No %s\n", fname);
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
    bootinfo.bi_kernelname = VTOP(fname);
    bootinfo.bi_bios_dev = dsk.drive;
    __exec((caddr_t)addr, RB_BOOTINFO | (opts & RBX_MASK),
	   MAKEBOOTDEV(dev_maj[dsk.type], 0, dsk.slice, dsk.unit, dsk.part),
	   0, 0, 0, VTOP(&bootinfo));
}

static int
parse(char *arg)
{
    char *p, *q;
    int drv, c, i;

    while ((c = *arg++)) {
	if (c == ' ' || c == '\t' || c == '\n')
	    continue;
	for (p = arg; *p && *p != '\n' && *p != ' ' && *p != '\t'; p++);
	if (*p)
	    *p++ = 0;
	if (c == '-') {
	    while ((c = *arg++)) {
		for (i = 0; c != optstr[i]; i++)
		    if (i == NOPT - 1)
			return -1;
		opts ^= 1 << flags[i];
	    }
	    if (opts & 1 << RBX_PROBEKBD) {
		i = *(uint8_t *)PTOV(0x496) & 0x10;
		printf("Keyboard: %s\n", i ? "yes" : "no");
		if (!i)
		    opts |= 1 << RBX_DUAL | 1 << RBX_SERIAL;
		opts &= ~(1 << RBX_PROBEKBD);
	    }
	    ioctrl = opts & 1 << RBX_DUAL ? 0x3 :
		     opts & 1 << RBX_SERIAL ? 0x2 : 0x1;
	    if (ioctrl & 0x2)
	        sio_init();
	} else {
	    for (q = arg--; *q && *q != '('; q++);
	    if (*q) {
		drv = -1;
		if (arg[1] == ':') {
		    if (*arg < '0' || *arg > '9')
			return -1;
		    drv = *arg - '0';
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
		if (arg[1] != ',' || *arg < '0' || *arg > '9')
		    return -1;
		dsk.unit = *arg - '0';
		arg += 2;
		dsk.slice = WHOLE_DISK_SLICE;
		if (arg[1] == ',') {
		    if (*arg < '0' || *arg > '0' + NDOSPART)
			return -1;
		    if ((dsk.slice = *arg - '0'))
			dsk.slice++;
		    arg += 2;
		}
		if (arg[1] != ')' || *arg < 'a' || *arg > 'p')
		    return -1;
		dsk.part = *arg - 'a';
		arg += 2;
		if (drv == -1)
		    drv = dsk.unit;
		dsk.drive = (dsk.type == TYPE_WD ||
			     dsk.type == TYPE_AD ||
			     dsk.type == TYPE_DA ? DRV_HARD : 0) + drv;
		dsk.meta = 0;
		fsread(0, NULL, 0);
	    }
	    if ((i = p - arg - !*(p - 1))) {
		if (i >= sizeof(kname))
		    return -1;
		memcpy(kname, arg, i + 1);
	    }
	}
	arg = p;
    }
    return 0;
}

static ino_t
lookup(const char *path)
{
    char name[MAXNAMLEN + 1];
    const char *s;
    ino_t ino;
    ssize_t n;
    int dt;

    ino = ROOTINO;
    dt = DT_DIR;
    for (;;) {
	if (*path == '/')
	    path++;
	if (!*path)
	    break;
	for (s = path; *s && *s != '/'; s++);
	if ((n = s - path) > MAXNAMLEN)
	    return 0;
	ls = *path == '?' && n == 1 && !*s;
	memcpy(name, path, n);
	name[n] = 0;
	if ((dt = fsfind(name, &ino)) <= 0)
	    break;
	path = s;
    }
    return dt == DT_REG ? ino : 0;
}
static int
xfsread(ino_t inode, void *buf, size_t nbyte)
{
    if (fsread(inode, buf, nbyte) != nbyte) {
	printf("Invalid %s\n", "format");
	return -1;
    }
    return 0;
}

static ssize_t
fsread(ino_t inode, void *buf, size_t nbyte)
{
    static struct fs fs;
    static struct dinode din;
    static char *blkbuf;
    static ufs_daddr_t *indbuf;
    static ino_t inomap;
    static ufs_daddr_t blkmap, indmap;
    static unsigned fsblks;
    char *s;
    ufs_daddr_t lbn, addr;
    size_t n, nb, off;

    if (!dsk.meta) {
	if (!blkbuf)
	    blkbuf = malloc(BSIZEMAX);
	inomap = 0;
	if (dskread(blkbuf, SBOFF / DEV_BSIZE, SBSIZE / DEV_BSIZE))
	    return -1;
	memcpy(&fs, blkbuf, sizeof(fs));
	if (fs.fs_magic != FS_MAGIC) {
	    printf("Not ufs\n");
	    return -1;
	}
	fsblks = fs.fs_bsize >> DEV_BSHIFT;
	dsk.meta++;
    }
    if (!inode)
	return 0;
    if (inomap != inode) {
	if (dskread(blkbuf, fsbtodb(&fs, ino_to_fsba(&fs, inode)),
		    fsblks))
	    return -1;
	din = ((struct dinode *)blkbuf)[inode % INOPB(&fs)];
	inomap = inode;
	fs_off = 0;
	blkmap = indmap = 0;
    }
    s = buf;
    if (nbyte > (n = din.di_size - fs_off))
	nbyte = n;
    nb = nbyte;
    while (nb) {
	lbn = lblkno(&fs, fs_off);
	if (lbn < NDADDR)
	    addr = din.di_db[lbn];
	else {
	    if (indmap != din.di_ib[0]) {
		if (!indbuf)
		    indbuf = malloc(BSIZEMAX);
		if (dskread(indbuf, fsbtodb(&fs, din.di_ib[0]),
			    fsblks))
		    return -1;
		indmap = din.di_ib[0];
	    }
	    addr = indbuf[(lbn - NDADDR) % NINDIR(&fs)];
	}
	n = dblksize(&fs, &din, lbn);
	if (blkmap != addr) {
	    if (dskread(blkbuf, fsbtodb(&fs, addr), n >> DEV_BSHIFT))
		return -1;
	    blkmap = addr;
	}
	off = blkoff(&fs, fs_off);
	n -= off;
	if (n > nb)
	    n = nb;
	memcpy(s, blkbuf + off, n);
	s += n;
	fs_off += n;
	nb -= n;
    }
    return nbyte;
}

static int
dskread(void *buf, unsigned lba, unsigned nblk)
{
    static char *sec;
    struct dos_partition *dp;
    struct disklabel *d;
    unsigned sl, i;

    if (!dsk.meta) {
	if (!sec)
	    sec = malloc(DEV_BSIZE);
	dsk.start = 0;
	if (drvread(sec, DOSBBSECTOR, 1))
	    return -1;
	dp = (void *)(sec + DOSPARTOFF);
	sl = dsk.slice;
	if (sl < BASE_SLICE) {
	    for (i = 0; i < NDOSPART; i++)
		if (dp[i].dp_typ == DOSPTYP_386BSD &&
		    (dp[i].dp_flag & 0x80 || sl < BASE_SLICE)) {
		    sl = BASE_SLICE + i;
		    if (dp[i].dp_flag & 0x80 ||
			dsk.slice == COMPATIBILITY_SLICE)
			break;
		}
	    if (dsk.slice == WHOLE_DISK_SLICE)
		dsk.slice = sl;
	}
	if (sl != WHOLE_DISK_SLICE) {
	    if (sl != COMPATIBILITY_SLICE)
		dp += sl - BASE_SLICE;
	    if (dp->dp_typ != DOSPTYP_386BSD) {
		printf("Invalid %s\n", "slice");
		return -1;
	    }
	    dsk.start = dp->dp_start;
	}
	if (drvread(sec, dsk.start + LABELSECTOR, 1))
		return -1;
	d = (void *)(sec + LABELOFFSET);
	if (d->d_magic != DISKMAGIC || d->d_magic2 != DISKMAGIC) {
	    if (dsk.part != RAW_PART) {
		printf("Invalid %s\n", "label");
		return -1;
	    }
	} else {
	    if (!dsk.init) {
		if (d->d_type == DTYPE_SCSI)
		    dsk.type = TYPE_DA;
		dsk.init++;
	    }
	    if (dsk.part >= d->d_npartitions ||
		!d->d_partitions[dsk.part].p_size) {
		printf("Invalid %s\n", "partition");
		return -1;
	    }
	    dsk.start = d->d_partitions[dsk.part].p_offset;
	}
    }
    return drvread(buf, dsk.start + lba, nblk);
}

static int
printf(const char *fmt,...)
{
    static const char digits[16] = "0123456789abcdef";
    va_list ap;
    char buf[10];
    char *s;
    unsigned r, u;
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
	    case 'x':
		r = c == 'u' ? 10U : 16U;
		u = va_arg(ap, unsigned);
		s = buf;
		do
		    *s++ = digits[u % r];
		while (u /= r);
		while (--s >= buf)
		    putchar(*s);
		continue;
	    }
	}
	putchar(c);
    }
    va_end(ap);
    return 0;
}

static int
putchar(int c)
{
    if (c == '\n')
	xputc('\r');
    return xputc(c);
}

static void *
memcpy(void *dst, const void *src, size_t size)
{
    const char *s;
    char *d;

    for (d = dst, s = src; size; size--)
	*d++ = *s++;
    return dst;
}

static void *
malloc(size_t size)
{
    static uint32_t next;
    void *p;

    if (!next)
	next = roundup2(__base + _end, 0x10000) - __base;
    p = (void *)next;
    next += size;
    return p;
}

static uint32_t
memsize(int type)
{
    v86.addr = type;
    v86.eax = 0x8800;
    v86int();
    return v86.eax;
}

static int
drvread(void *buf, unsigned lba, unsigned nblk)
{
    static unsigned c = 0x2d5c7c2f;

    printf("%c\b", c = c << 8 | c >> 24);
    v86.ctl = V86_ADDR | V86_CALLF | V86_FLAGS;
    v86.addr = 0x704;		/* call to xread in boot1 */
    v86.es = VTOPSEG(buf);
    v86.eax = lba;
    v86.ebx = VTOPOFF(buf);
    v86.ecx = lba >> 16;
    v86.edx = nblk << 8 | dsk.drive;
    v86int();
    v86.ctl = V86_FLAGS;
    if (V86_CY(v86.efl)) {
	printf("Disk error 0x%x (lba=0x%x)\n", v86.eax >> 8 & 0xff,
	       lba);
	return -1;
    }
    return 0;
}

static int
keyhit(unsigned ticks)
{
    uint32_t t0, t1;

    t0 = 0;
    for (;;) {
	if (xgetc(1))
	    return 1;
	t1 = *(uint32_t *)PTOV(0x46c);
	if (!t0)
	    t0 = t1;
	if (t1 < t0 || t1 >= t0 + ticks)
	    return 0;
    }
}

static int
xputc(int c)
{
    if (ioctrl & 0x1)
	putc(c);
    if (ioctrl & 0x2)
	sio_putc(c);
    return c;
}

static int
xgetc(int fn)
{
    for (;;) {
	if (ioctrl & 0x1 && getc(1))
	    return fn ? 1 : getc(0);
	if (ioctrl & 0x2 && sio_ischar())
	    return fn ? 1 : sio_getc();
	if (fn)
	    return 0;
    }
}

static int
getc(int fn)
{
    v86.addr = 0x16;
    v86.eax = fn << 8;
    v86int();
    return fn == 0 ? v86.eax & 0xff : !V86_ZR(v86.efl);
}
