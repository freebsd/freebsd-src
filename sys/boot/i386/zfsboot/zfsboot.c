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
#include <sys/errno.h>
#include <sys/diskmbr.h>
#ifdef GPT
#include <sys/gpt.h>
#endif
#include <sys/reboot.h>
#include <sys/queue.h>

#include <machine/bootinfo.h>
#include <machine/elf.h>

#include <stdarg.h>
#include <stddef.h>

#include <a.out.h>

#include <btxv86.h>

#ifndef GPT
#include "zfsboot.h"
#endif
#include "lib.h"

#define IO_KEYBOARD	1
#define IO_SERIAL	2

#define SECOND		18	/* Circa that many ticks in a second. */

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

/* Hint to loader that we came from ZFS */
#define	KARGS_FLAGS_ZFS		0x4

#define PATH_CONFIG	"/boot.config"
#define PATH_BOOT3	"/boot/loader"
#define PATH_KERNEL	"/boot/kernel/kernel"

#define ARGS		0x900
#define NOPT		14
#define NDEV		3
#define MEM_BASE	0x12
#define MEM_EXT 	0x15
#define V86_CY(x)	((x) & 1)
#define V86_ZR(x)	((x) & 0x40)

#define DRV_HARD	0x80
#define DRV_MASK	0x7f

#define TYPE_AD		0
#define TYPE_DA		1
#define TYPE_MAXHARD	TYPE_DA
#define TYPE_FD		2

#define OPT_SET(opt)	(1 << (opt))
#define OPT_CHECK(opt)	((opts) & OPT_SET(opt))

extern uint32_t _end;

#ifdef GPT
static const uuid_t freebsd_zfs_uuid = GPT_ENT_TYPE_FREEBSD_ZFS;
#endif
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

struct dsk {
    unsigned drive;
    unsigned type;
    unsigned unit;
    unsigned slice;
    unsigned part;
    int init;
    daddr_t start;
};
static char cmd[512];
static char kname[1024];
static uint32_t opts;
static int comspeed = SIOSPD;
static struct bootinfo bootinfo;
static uint32_t bootdev;
static uint8_t ioctrl = IO_KEYBOARD;

/* Buffers that must not span a 64k boundary. */
#define READ_BUF_SIZE	8192
struct dmadat {
	char rdbuf[READ_BUF_SIZE];	/* for reading large things */
	char secbuf[READ_BUF_SIZE];	/* for MBR/disklabel */
};
static struct dmadat *dmadat;

void exit(int);
static void load(void);
static int parse(void);
static void printf(const char *,...);
static void putchar(int);
static uint32_t memsize(void);
static int drvread(struct dsk *, void *, daddr_t, unsigned);
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

static void
strcpy(char *dst, const char *src)
{
    while (*src)
	*dst++ = *src++;
    *dst++ = 0;
}

static void
strcat(char *dst, const char *src)
{
    while (*dst)
	dst++;
    while (*src)
	*dst++ = *src++;
    *dst++ = 0;
}

static int
strcmp(const char *s1, const char *s2)
{
    for (; *s1 == *s2 && *s1; s1++, s2++);
    return (unsigned char)*s1 - (unsigned char)*s2;
}

static const char *
strchr(const char *s, char ch)
{
    for (; *s; s++)
	if (*s == ch)
		return s;
    return 0;
}

static int
memcmp(const void *p1, const void *p2, size_t n)
{
    const char *s1 = (const char *) p1;
    const char *s2 = (const char *) p2;
    for (; n > 0 && *s1 == *s2; s1++, s2++, n--);
    if (n)
        return (unsigned char)*s1 - (unsigned char)*s2;
    else
	return 0;
}

static void
memset(void *p, char val, size_t n)
{
    char *s = (char *) p;
    while (n--)
	*s++ = val;
}

static void *
malloc(size_t n)
{
	static char *heap_next;
	static char *heap_end;

	if (!heap_next) {
		heap_next = (char *) dmadat + sizeof(*dmadat);
		heap_end = (char *) (640*1024);
	}

	char *p = heap_next;
	if (p + n > heap_end) {
		printf("malloc failure\n");
		for (;;)
		    ;
		return 0;
	}
	heap_next += n;
	return p;
}

static size_t
strlen(const char *s)
{
	size_t len = 0;
	while (*s++)
		len++;
	return len;
}

static char *
strdup(const char *s)
{
	char *p = malloc(strlen(s) + 1);
	strcpy(p, s);
	return p;
}

#include "zfsimpl.c"

/*
 * Read from a dnode (which must be from a ZPL filesystem).
 */
static int
zfs_read(spa_t *spa, const dnode_phys_t *dnode, off_t *offp, void *start, size_t size)
{
	const znode_phys_t *zp = (const znode_phys_t *) dnode->dn_bonus;
	size_t n;
	int rc;

	n = size;
	if (*offp + n > zp->zp_size)
		n = zp->zp_size - *offp;
	
	rc = dnode_read(spa, dnode, *offp, start, n);
	if (rc)
		return (-1);
	*offp += n;

	return (n);
}

/*
 * Current ZFS pool
 */
spa_t *spa;

/*
 * A wrapper for dskread that doesn't have to worry about whether the
 * buffer pointer crosses a 64k boundary.
 */
static int
vdev_read(vdev_t *vdev, void *priv, off_t off, void *buf, size_t bytes)
{
	char *p;
	daddr_t lba;
	unsigned int nb;
	struct dsk *dsk = (struct dsk *) priv;

	if ((off & (DEV_BSIZE - 1)) || (bytes & (DEV_BSIZE - 1)))
		return -1;

	p = buf;
	lba = off / DEV_BSIZE;
	while (bytes > 0) {
		nb = bytes / DEV_BSIZE;
		if (nb > READ_BUF_SIZE / DEV_BSIZE)
			nb = READ_BUF_SIZE / DEV_BSIZE;
		if (drvread(dsk, dmadat->rdbuf, lba, nb))
			return -1;
		memcpy(p, dmadat->rdbuf, nb * DEV_BSIZE);
		p += nb * DEV_BSIZE;
		lba += nb;
		bytes -= nb * DEV_BSIZE;
	}

	return 0;
}

static int
xfsread(const dnode_phys_t *dnode, off_t *offp, void *buf, size_t nbyte)
{
    if ((size_t)zfs_read(spa, dnode, offp, buf, nbyte) != nbyte) {
	printf("Invalid %s\n", "format");
	return -1;
    }
    return 0;
}

static inline uint32_t
memsize(void)
{
    v86.addr = MEM_EXT;
    v86.eax = 0x8800;
    v86int();
    return v86.eax;
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
    v86.addr = 0x10;
    v86.eax = 0xe00 | (c & 0xff);
    v86.ebx = 0x7;
    v86int();
}

/*
 * Try to detect a device supported by the legacy int13 BIOS
 */
static int
int13probe(int drive)
{
    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    v86.eax = 0x800;
    v86.edx = drive;
    v86int();
    
    if (!(v86.efl & 0x1) &&				/* carry clear */
	((v86.edx & 0xff) != (drive & DRV_MASK))) {	/* unit # OK */
	if ((v86.ecx & 0x3f) == 0) {			/* absurd sector size */
		return(0);				/* skip device */
	}
	return (1);
    }
    return(0);
}

/*
 * We call this when we find a ZFS vdev - ZFS consumes the dsk
 * structure so we must make a new one.
 */
static struct dsk *
copy_dsk(struct dsk *dsk)
{
    struct dsk *newdsk;

    newdsk = malloc(sizeof(struct dsk));
    *newdsk = *dsk;
    return (newdsk);
}

static void
probe_drive(struct dsk *dsk, spa_t **spap)
{
#ifdef GPT
    struct gpt_hdr hdr;
    struct gpt_ent *ent;
    daddr_t slba, elba;
    unsigned part, entries_per_sec;
#endif
    struct dos_partition *dp;
    char *sec;
    unsigned i;

    /*
     * If we find a vdev on the whole disk, stop here. Otherwise dig
     * out the MBR and probe each slice in turn for a vdev.
     */
    if (vdev_probe(vdev_read, dsk, spap) == 0)
	return;

    sec = dmadat->secbuf;
    dsk->start = 0;

#ifdef GPT
    /*
     * First check for GPT.
     */
    if (drvread(dsk, sec, 1, 1)) {
	return;
    }
    memcpy(&hdr, sec, sizeof(hdr));
    if (memcmp(hdr.hdr_sig, GPT_HDR_SIG, sizeof(hdr.hdr_sig)) != 0 ||
	hdr.hdr_lba_self != 1 || hdr.hdr_revision < 0x00010000 ||
	hdr.hdr_entsz < sizeof(*ent) || DEV_BSIZE % hdr.hdr_entsz != 0) {
	goto trymbr;
    }

    /*
     * Probe all GPT partitions for the presense of ZFS pools. We
     * return the spa_t for the first we find (if requested). This
     * will have the effect of booting from the first pool on the
     * disk.
     */
    entries_per_sec = DEV_BSIZE / hdr.hdr_entsz;
    slba = hdr.hdr_lba_table;
    elba = slba + hdr.hdr_entries / entries_per_sec;
    while (slba < elba) {
	dsk->start = 0;
	if (drvread(dsk, sec, slba, 1))
	    return;
	for (part = 0; part < entries_per_sec; part++) {
	    ent = (struct gpt_ent *)(sec + part * hdr.hdr_entsz);
	    if (memcmp(&ent->ent_type, &freebsd_zfs_uuid,
		     sizeof(uuid_t)) == 0) {
		dsk->start = ent->ent_lba_start;
		if (vdev_probe(vdev_read, dsk, spap) == 0) {
		    /*
		     * We record the first pool we find (we will try
		     * to boot from that one).
		     */
		    spap = 0;

		    /*
		     * This slice had a vdev. We need a new dsk
		     * structure now since the vdev now owns this one.
		     */
		    dsk = copy_dsk(dsk);
		}
	    }
	}
	slba++;
    }
    return;
trymbr:
#endif

    if (drvread(dsk, sec, DOSBBSECTOR, 1))
	return;
    dp = (void *)(sec + DOSPARTOFF);

    for (i = 0; i < NDOSPART; i++) {
	if (!dp[i].dp_typ)
	    continue;
	dsk->start = dp[i].dp_start;
	if (vdev_probe(vdev_read, dsk, spap) == 0) {
	    /*
	     * We record the first pool we find (we will try to boot
	     * from that one.
	     */
	    spap = 0;

	    /*
	     * This slice had a vdev. We need a new dsk structure now
	     * since the vdev now owns this one.
	     */
	    dsk = copy_dsk(dsk);
	}
    }
}

int
main(void)
{
    int autoboot, i;
    dnode_phys_t dn;
    off_t off;
    struct dsk *dsk;

    dmadat = (void *)(roundup2(__base + (int32_t)&_end, 0x10000) - __base);
    v86.ctl = V86_FLAGS;

    dsk = malloc(sizeof(struct dsk));
    dsk->drive = *(uint8_t *)PTOV(ARGS);
    dsk->type = dsk->drive & DRV_HARD ? TYPE_AD : TYPE_FD;
    dsk->unit = dsk->drive & DRV_MASK;
    dsk->slice = *(uint8_t *)PTOV(ARGS + 1) + 1;
    dsk->part = 0;
    dsk->start = 0;
    dsk->init = 0;

    bootinfo.bi_version = BOOTINFO_VERSION;
    bootinfo.bi_size = sizeof(bootinfo);
    bootinfo.bi_basemem = 0;	/* XXX will be filled by loader or kernel */
    bootinfo.bi_extmem = memsize();
    bootinfo.bi_memsizes_valid++;
    bootinfo.bi_bios_dev = dsk->drive;

    bootdev = MAKEBOOTDEV(dev_maj[dsk->type],
			  dsk->slice, dsk->unit, dsk->part),

    /* Process configuration file */

    autoboot = 1;

    zfs_init();

    /*
     * Probe the boot drive first - we will try to boot from whatever
     * pool we find on that drive.
     */
    probe_drive(dsk, &spa);

    /*
     * Probe the rest of the drives that the bios knows about. This
     * will find any other available pools and it may fill in missing
     * vdevs for the boot pool.
     */
    for (i = 0; i < 128; i++) {
	if ((i | DRV_HARD) == *(uint8_t *)PTOV(ARGS))
	    continue;

	if (!int13probe(i | DRV_HARD))
	    break;

	dsk = malloc(sizeof(struct dsk));
	dsk->drive = i | DRV_HARD;
	dsk->type = dsk->drive & TYPE_AD;
	dsk->unit = i;
	dsk->slice = 0;
	dsk->part = 0;
	dsk->start = 0;
	dsk->init = 0;
	probe_drive(dsk, 0);
    }

    /*
     * If we didn't find a pool on the boot drive, default to the
     * first pool we found, if any.
     */
    if (!spa) {
	spa = STAILQ_FIRST(&zfs_pools);
	if (!spa) {
	    printf("No ZFS pools located, can't boot\n");
	    for (;;)
		;
	}
    }

    zfs_mount_pool(spa);

    if (zfs_lookup(spa, PATH_CONFIG, &dn) == 0) {
	off = 0;
	zfs_read(spa, &dn, &off, cmd, sizeof(cmd));
    }

    if (*cmd) {
	if (parse())
	    autoboot = 0;
	if (!OPT_CHECK(RBX_QUIET))
	    printf("%s: %s", PATH_CONFIG, cmd);
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
	    printf("\nFreeBSD/i386 boot\n"
		   "Default: %s:%s\n"
		   "boot: ",
		   spa->spa_name, kname);
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
    dnode_phys_t dn;
    off_t off;
    uint32_t addr, x;
    int fmt, i, j;

    if (zfs_lookup(spa, kname, &dn)) {
	return;
    }
    off = 0;
    if (xfsread(&dn, &off, &hdr, sizeof(hdr)))
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
	off = PAGE_SIZE;
	if (xfsread(&dn, &off, p, hdr.ex.a_text))
	    return;
	p += roundup2(hdr.ex.a_text, PAGE_SIZE);
	if (xfsread(&dn, &off, p, hdr.ex.a_data))
	    return;
	p += hdr.ex.a_data + roundup2(hdr.ex.a_bss, PAGE_SIZE);
	bootinfo.bi_symtab = VTOP(p);
	memcpy(p, &hdr.ex.a_syms, sizeof(hdr.ex.a_syms));
	p += sizeof(hdr.ex.a_syms);
	if (hdr.ex.a_syms) {
	    if (xfsread(&dn, &off, p, hdr.ex.a_syms))
		return;
	    p += hdr.ex.a_syms;
	    if (xfsread(&dn, &off, p, sizeof(int)))
		return;
	    x = *(uint32_t *)p;
	    p += sizeof(int);
	    x -= sizeof(int);
	    if (xfsread(&dn, &off, p, x))
		return;
	    p += x;
	}
    } else {
	off = hdr.eh.e_phoff;
	for (j = i = 0; i < hdr.eh.e_phnum && j < 2; i++) {
	    if (xfsread(&dn, &off, ep + j, sizeof(ep[0])))
		return;
	    if (ep[j].p_type == PT_LOAD)
		j++;
	}
	for (i = 0; i < 2; i++) {
	    p = PTOV(ep[i].p_paddr & 0xffffff);
	    off = ep[i].p_offset;
	    if (xfsread(&dn, &off, p, ep[i].p_filesz))
		return;
	}
	p += roundup2(ep[1].p_memsz, PAGE_SIZE);
	bootinfo.bi_symtab = VTOP(p);
	if (hdr.eh.e_shnum == hdr.eh.e_shstrndx + 3) {
	    off = hdr.eh.e_shoff + sizeof(es[0]) *
		(hdr.eh.e_shstrndx + 1);
	    if (xfsread(&dn, &off, &es, sizeof(es)))
		return;
	    for (i = 0; i < 2; i++) {
		memcpy(p, &es[i].sh_size, sizeof(es[i].sh_size));
		p += sizeof(es[i].sh_size);
		off = es[i].sh_offset;
		if (xfsread(&dn, &off, p, es[i].sh_size))
		    return;
		p += es[i].sh_size;
	    }
	}
	addr = hdr.eh.e_entry & 0xffffff;
    }
    bootinfo.bi_esymtab = VTOP(p);
    bootinfo.bi_kernelname = VTOP(kname);
    __exec((caddr_t)addr, RB_BOOTINFO | (opts & RBX_MASK),
	   bootdev,
	   KARGS_FLAGS_ZFS,
	   (uint32_t) spa->spa_guid,
	   (uint32_t) (spa->spa_guid >> 32),
	   VTOP(&bootinfo));
}

static int
parse()
{
    char *arg = cmd;
    char *ep, *p, *q;
    const char *cp;
    //unsigned int drv;
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
	    if (ioctrl & IO_SERIAL)
	        sio_init(115200 / comspeed);
	} if (c == '?') {
	    dnode_phys_t dn;

	    if (zfs_lookup(spa, arg, &dn) == 0) {
		zap_list(spa, &dn);
	    }
	    return -1;
	} else {
	    arg--;

	    /*
	     * Report pool status if the comment is 'status'. Lets
	     * hope no-one wants to load /status as a kernel.
	     */
	    if (!strcmp(arg, "status")) {
		spa_all_status();
		return -1;
	    }

	    /*
	     * If there is a colon, switch pools.
	     */
	    q = (char *) strchr(arg, ':');
	    if (q) {
		spa_t *newspa;

		*q++ = 0;
		newspa = spa_find_by_name(arg);
		if (newspa) {
		    spa = newspa;
		    zfs_mount_pool(spa);
		} else {
		    printf("\nCan't find ZFS pool %s\n", arg);
		    return -1;
		}
		arg = q;
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

static void
printf(const char *fmt,...)
{
    va_list ap;
    char buf[20];
    char *s;
    unsigned long long u;
    int c;
    int minus;
    int prec;
    int l;
    int len;
    int pad;

    va_start(ap, fmt);
    while ((c = *fmt++)) {
	if (c == '%') {
	    minus = 0;
	    prec = 0;
	    l = 0;
	nextfmt:
	    c = *fmt++;
	    switch (c) {
	    case '-':
		minus = 1;
		goto nextfmt;
	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
		prec = 10 * prec + (c - '0');
		goto nextfmt;
	    case 'c':
		putchar(va_arg(ap, int));
		continue;
	    case 'l':
		l++;
		goto nextfmt;
	    case 's':
		s = va_arg(ap, char *);
		if (prec) {
		    len = strlen(s);
		    if (len < prec)
			pad = prec - len;
		    else
			pad = 0;
		    if (minus)
			while (pad--)
			    putchar(' ');
		    for (; *s; s++)
			putchar(*s);
		    if (!minus)
			while (pad--)
			    putchar(' ');
		} else {
		    for (; *s; s++)
			putchar(*s);
		}
		continue;
	    case 'u':
		switch (l) {
		case 2:
		    u = va_arg(ap, unsigned long long);
		    break;
		case 1:
		    u = va_arg(ap, unsigned long);
		    break;
		default:
		    u = va_arg(ap, unsigned);
		    break;
		}
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

#ifdef GPT
static struct {
	uint16_t len;
	uint16_t count;
	uint16_t seg;
	uint16_t off;
	uint64_t lba;
} packet;
#endif

static int
drvread(struct dsk *dsk, void *buf, daddr_t lba, unsigned nblk)
{
#ifdef GPT
    static unsigned c = 0x2d5c7c2f;

    if (!OPT_CHECK(RBX_QUIET))
	printf("%c\b", c = c << 8 | c >> 24);
    packet.len = 0x10;
    packet.count = nblk;
    packet.seg = VTOPOFF(buf);
    packet.off = VTOPSEG(buf);
    packet.lba = lba + dsk->start;
    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    v86.eax = 0x4200;
    v86.edx = dsk->drive;
    v86.ds = VTOPSEG(&packet);
    v86.esi = VTOPOFF(&packet);
    v86int();
    if (V86_CY(v86.efl)) {
	printf("error %u lba %u\n", v86.eax >> 8 & 0xff, lba);
	return -1;
    }
    return 0;
#else
    static unsigned c = 0x2d5c7c2f;

    lba += dsk->start;
    if (!OPT_CHECK(RBX_QUIET))
	printf("%c\b", c = c << 8 | c >> 24);
    v86.ctl = V86_ADDR | V86_CALLF | V86_FLAGS;
    v86.addr = XREADORG;		/* call to xread in boot1 */
    v86.es = VTOPSEG(buf);
    v86.eax = lba;
    v86.ebx = VTOPOFF(buf);
    v86.ecx = lba >> 32;
    v86.edx = nblk << 8 | dsk->drive;
    v86int();
    v86.ctl = V86_FLAGS;
    if (V86_CY(v86.efl)) {
	printf("error %u lba %u\n", v86.eax >> 8 & 0xff, lba);
	return -1;
    }
    return 0;
#endif
}

static int
keyhit(unsigned ticks)
{
    uint32_t t0, t1;

    if (OPT_CHECK(RBX_NOINTR))
	return 0;
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
    /*
     * The extra comparison against zero is an attempt to work around
     * what appears to be a bug in QEMU and Bochs. Both emulators
     * sometimes report a key-press with scancode one and ascii zero
     * when no such key is pressed in reality. As far as I can tell,
     * this only happens shortly after a reboot.
     */
    v86.addr = 0x16;
    v86.eax = fn << 8;
    v86int();
    return fn == 0 ? v86.eax & 0xff : (!V86_ZR(v86.efl) && (v86.eax & 0xff));
}
