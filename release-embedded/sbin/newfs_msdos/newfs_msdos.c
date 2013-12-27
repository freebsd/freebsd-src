/*
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/fdcio.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAXU16	  0xffff	/* maximum unsigned 16-bit quantity */
#define BPN	  4		/* bits per nibble */
#define NPB	  2		/* nibbles per byte */

#define DOSMAGIC  0xaa55	/* DOS magic number */
#define MINBPS	  512		/* minimum bytes per sector */
#define MAXSPC	  128		/* maximum sectors per cluster */
#define MAXNFT	  16		/* maximum number of FATs */
#define DEFBLK	  4096		/* default block size */
#define DEFBLK16  2048		/* default block size FAT16 */
#define DEFRDE	  512		/* default root directory entries */
#define RESFTE	  2		/* reserved FAT entries */
#define MINCLS12  1U		/* minimum FAT12 clusters */
#define MINCLS16  0x1000U	/* minimum FAT16 clusters */
#define MINCLS32  2U		/* minimum FAT32 clusters */
#define MAXCLS12  0xfedU	/* maximum FAT12 clusters */
#define MAXCLS16  0xfff5U	/* maximum FAT16 clusters */
#define MAXCLS32  0xffffff5U	/* maximum FAT32 clusters */

#define mincls(fat)  ((fat) == 12 ? MINCLS12 :	\
		      (fat) == 16 ? MINCLS16 :	\
				    MINCLS32)

#define maxcls(fat)  ((fat) == 12 ? MAXCLS12 :	\
		      (fat) == 16 ? MAXCLS16 :	\
				    MAXCLS32)

#define mk1(p, x)				\
    (p) = (u_int8_t)(x)

#define mk2(p, x)				\
    (p)[0] = (u_int8_t)(x),			\
    (p)[1] = (u_int8_t)((x) >> 010)

#define mk4(p, x)				\
    (p)[0] = (u_int8_t)(x),			\
    (p)[1] = (u_int8_t)((x) >> 010),		\
    (p)[2] = (u_int8_t)((x) >> 020),		\
    (p)[3] = (u_int8_t)((x) >> 030)

#define argto1(arg, lo, msg)  argtou(arg, lo, 0xff, msg)
#define argto2(arg, lo, msg)  argtou(arg, lo, 0xffff, msg)
#define argto4(arg, lo, msg)  argtou(arg, lo, 0xffffffff, msg)
#define argtox(arg, lo, msg)  argtou(arg, lo, UINT_MAX, msg)

struct bs {
    u_int8_t bsJump[3];			/* bootstrap entry point */
    u_int8_t bsOemName[8];		/* OEM name and version */
} __packed;

struct bsbpb {
    u_int8_t bpbBytesPerSec[2];		/* bytes per sector */
    u_int8_t bpbSecPerClust;		/* sectors per cluster */
    u_int8_t bpbResSectors[2];		/* reserved sectors */
    u_int8_t bpbFATs;			/* number of FATs */
    u_int8_t bpbRootDirEnts[2];		/* root directory entries */
    u_int8_t bpbSectors[2];		/* total sectors */
    u_int8_t bpbMedia;			/* media descriptor */
    u_int8_t bpbFATsecs[2];		/* sectors per FAT */
    u_int8_t bpbSecPerTrack[2];		/* sectors per track */
    u_int8_t bpbHeads[2];		/* drive heads */
    u_int8_t bpbHiddenSecs[4];		/* hidden sectors */
    u_int8_t bpbHugeSectors[4];		/* big total sectors */
} __packed;

struct bsxbpb {
    u_int8_t bpbBigFATsecs[4];		/* big sectors per FAT */
    u_int8_t bpbExtFlags[2];		/* FAT control flags */
    u_int8_t bpbFSVers[2];		/* file system version */
    u_int8_t bpbRootClust[4];		/* root directory start cluster */
    u_int8_t bpbFSInfo[2];		/* file system info sector */
    u_int8_t bpbBackup[2];		/* backup boot sector */
    u_int8_t bpbReserved[12];		/* reserved */
} __packed;

struct bsx {
    u_int8_t exDriveNumber;		/* drive number */
    u_int8_t exReserved1;		/* reserved */
    u_int8_t exBootSignature;		/* extended boot signature */
    u_int8_t exVolumeID[4];		/* volume ID number */
    u_int8_t exVolumeLabel[11]; 	/* volume label */
    u_int8_t exFileSysType[8];		/* file system type */
} __packed;

struct de {
    u_int8_t deName[11];		/* name and extension */
    u_int8_t deAttributes;		/* attributes */
    u_int8_t rsvd[10];			/* reserved */
    u_int8_t deMTime[2];		/* last-modified time */
    u_int8_t deMDate[2];		/* last-modified date */
    u_int8_t deStartCluster[2];		/* starting cluster */
    u_int8_t deFileSize[4];		/* size */
} __packed;

struct bpb {
    u_int bpbBytesPerSec;		/* bytes per sector */
    u_int bpbSecPerClust;		/* sectors per cluster */
    u_int bpbResSectors;		/* reserved sectors */
    u_int bpbFATs;			/* number of FATs */
    u_int bpbRootDirEnts;		/* root directory entries */
    u_int bpbSectors;			/* total sectors */
    u_int bpbMedia;			/* media descriptor */
    u_int bpbFATsecs;			/* sectors per FAT */
    u_int bpbSecPerTrack;		/* sectors per track */
    u_int bpbHeads;			/* drive heads */
    u_int bpbHiddenSecs;		/* hidden sectors */
    u_int bpbHugeSectors; 		/* big total sectors */
    u_int bpbBigFATsecs; 		/* big sectors per FAT */
    u_int bpbRootClust; 		/* root directory start cluster */
    u_int bpbFSInfo; 			/* file system info sector */
    u_int bpbBackup; 			/* backup boot sector */
};

#define BPBGAP 0, 0, 0, 0, 0, 0

static struct {
    const char *name;
    struct bpb bpb;
} const stdfmt[] = {
    {"160",  {512, 1, 1, 2,  64,  320, 0xfe, 1,  8, 1, BPBGAP}},
    {"180",  {512, 1, 1, 2,  64,  360, 0xfc, 2,  9, 1, BPBGAP}},
    {"320",  {512, 2, 1, 2, 112,  640, 0xff, 1,  8, 2, BPBGAP}},
    {"360",  {512, 2, 1, 2, 112,  720, 0xfd, 2,  9, 2, BPBGAP}},
    {"640",  {512, 2, 1, 2, 112, 1280, 0xfb, 2,  8, 2, BPBGAP}},    
    {"720",  {512, 2, 1, 2, 112, 1440, 0xf9, 3,  9, 2, BPBGAP}},
    {"1200", {512, 1, 1, 2, 224, 2400, 0xf9, 7, 15, 2, BPBGAP}},
    {"1232", {1024,1, 1, 2, 192, 1232, 0xfe, 2,  8, 2, BPBGAP}},    
    {"1440", {512, 1, 1, 2, 224, 2880, 0xf0, 9, 18, 2, BPBGAP}},
    {"2880", {512, 2, 1, 2, 240, 5760, 0xf0, 9, 36, 2, BPBGAP}}
};

static const u_int8_t bootcode[] = {
    0xfa,			/* cli		    */
    0x31, 0xc0, 		/* xor	   ax,ax    */
    0x8e, 0xd0, 		/* mov	   ss,ax    */
    0xbc, 0x00, 0x7c,		/* mov	   sp,7c00h */
    0xfb,			/* sti		    */
    0x8e, 0xd8, 		/* mov	   ds,ax    */
    0xe8, 0x00, 0x00,		/* call    $ + 3    */
    0x5e,			/* pop	   si	    */
    0x83, 0xc6, 0x19,		/* add	   si,+19h  */
    0xbb, 0x07, 0x00,		/* mov	   bx,0007h */
    0xfc,			/* cld		    */
    0xac,			/* lodsb	    */
    0x84, 0xc0, 		/* test    al,al    */
    0x74, 0x06, 		/* jz	   $ + 8    */
    0xb4, 0x0e, 		/* mov	   ah,0eh   */
    0xcd, 0x10, 		/* int	   10h	    */
    0xeb, 0xf5, 		/* jmp	   $ - 9    */
    0x30, 0xe4, 		/* xor	   ah,ah    */
    0xcd, 0x16, 		/* int	   16h	    */
    0xcd, 0x19, 		/* int	   19h	    */
    0x0d, 0x0a,
    'N', 'o', 'n', '-', 's', 'y', 's', 't',
    'e', 'm', ' ', 'd', 'i', 's', 'k',
    0x0d, 0x0a,
    'P', 'r', 'e', 's', 's', ' ', 'a', 'n',
    'y', ' ', 'k', 'e', 'y', ' ', 't', 'o',
    ' ', 'r', 'e', 'b', 'o', 'o', 't',
    0x0d, 0x0a,
    0
};

static volatile sig_atomic_t got_siginfo;
static void infohandler(int);

static void check_mounted(const char *, mode_t);
static void getstdfmt(const char *, struct bpb *);
static void getdiskinfo(int, const char *, const char *, int,
			struct bpb *);
static void print_bpb(struct bpb *);
static u_int ckgeom(const char *, u_int, const char *);
static u_int argtou(const char *, u_int, u_int, const char *);
static off_t argtooff(const char *, const char *);
static int oklabel(const char *);
static void mklabel(u_int8_t *, const char *);
static void setstr(u_int8_t *, const char *, size_t);
static void usage(void);

/*
 * Construct a FAT12, FAT16, or FAT32 file system.
 */
int
main(int argc, char *argv[])
{
    static const char opts[] = "@:NB:C:F:I:L:O:S:a:b:c:e:f:h:i:k:m:n:o:r:s:u:";
    const char *opt_B = NULL, *opt_L = NULL, *opt_O = NULL, *opt_f = NULL;
    u_int opt_F = 0, opt_I = 0, opt_S = 0, opt_a = 0, opt_b = 0, opt_c = 0;
    u_int opt_e = 0, opt_h = 0, opt_i = 0, opt_k = 0, opt_m = 0, opt_n = 0;
    u_int opt_o = 0, opt_r = 0, opt_s = 0, opt_u = 0;
    int opt_N = 0;
    int Iflag = 0, mflag = 0, oflag = 0;
    char buf[MAXPATHLEN];
    struct sigaction si_sa;
    struct stat sb;
    struct timeval tv;
    struct bpb bpb;
    struct tm *tm;
    struct bs *bs;
    struct bsbpb *bsbpb;
    struct bsxbpb *bsxbpb;
    struct bsx *bsx;
    struct de *de;
    u_int8_t *img;
    const char *fname, *dtype, *bname;
    ssize_t n;
    time_t now;
    u_int fat, bss, rds, cls, dir, lsn, x, x1, x2;
    int ch, fd, fd1;
    off_t opt_create = 0, opt_ofs = 0;

    while ((ch = getopt(argc, argv, opts)) != -1)
	switch (ch) {
	case '@':
	    opt_ofs = argtooff(optarg, "offset");
	    break;
	case 'N':
	    opt_N = 1;
	    break;
	case 'B':
	    opt_B = optarg;
	    break;
	case 'C':
	    opt_create = argtooff(optarg, "create size");
	    break;
	case 'F':
	    if (strcmp(optarg, "12") &&
		strcmp(optarg, "16") &&
		strcmp(optarg, "32"))
		errx(1, "%s: bad FAT type", optarg);
	    opt_F = atoi(optarg);
	    break;
	case 'I':
	    opt_I = argto4(optarg, 0, "volume ID");
	    Iflag = 1;
	    break;
	case 'L':
	    if (!oklabel(optarg))
		errx(1, "%s: bad volume label", optarg);
	    opt_L = optarg;
	    break;
	case 'O':
	    if (strlen(optarg) > 8)
		errx(1, "%s: bad OEM string", optarg);
	    opt_O = optarg;
	    break;
	case 'S':
	    opt_S = argto2(optarg, 1, "bytes/sector");
	    break;
	case 'a':
	    opt_a = argto4(optarg, 1, "sectors/FAT");
	    break;
	case 'b':
	    opt_b = argtox(optarg, 1, "block size");
	    opt_c = 0;
	    break;
	case 'c':
	    opt_c = argto1(optarg, 1, "sectors/cluster");
	    opt_b = 0;
	    break;
	case 'e':
	    opt_e = argto2(optarg, 1, "directory entries");
	    break;
	case 'f':
	    opt_f = optarg;
	    break;
	case 'h':
	    opt_h = argto2(optarg, 1, "drive heads");
	    break;
	case 'i':
	    opt_i = argto2(optarg, 1, "info sector");
	    break;
	case 'k':
	    opt_k = argto2(optarg, 1, "backup sector");
	    break;
	case 'm':
	    opt_m = argto1(optarg, 0, "media descriptor");
	    mflag = 1;
	    break;
	case 'n':
	    opt_n = argto1(optarg, 1, "number of FATs");
	    break;
	case 'o':
	    opt_o = argto4(optarg, 0, "hidden sectors");
	    oflag = 1;
	    break;
	case 'r':
	    opt_r = argto2(optarg, 1, "reserved sectors");
	    break;
	case 's':
	    opt_s = argto4(optarg, 1, "file system size");
	    break;
	case 'u':
	    opt_u = argto2(optarg, 1, "sectors/track");
	    break;
	default:
	    usage();
	}
    argc -= optind;
    argv += optind;
    if (argc < 1 || argc > 2)
	usage();
    fname = *argv++;
    if (!opt_create && !strchr(fname, '/')) {
	snprintf(buf, sizeof(buf), "%s%s", _PATH_DEV, fname);
	if (!(fname = strdup(buf)))
	    err(1, NULL);
    }
    dtype = *argv;
    if (opt_create) {
	if (opt_N)
	    errx(1, "create (-C) is incompatible with -N");
	fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
	    errx(1, "failed to create %s", fname);
	if (ftruncate(fd, opt_create))
	    errx(1, "failed to initialize %jd bytes", (intmax_t)opt_create);
    } else if ((fd = open(fname, opt_N ? O_RDONLY : O_RDWR)) == -1)
	err(1, "%s", fname);
    if (fstat(fd, &sb))
	err(1, "%s", fname);
    if (opt_create) {
	if (!S_ISREG(sb.st_mode))
	    warnx("warning, %s is not a regular file", fname);
    } else {
	if (!S_ISCHR(sb.st_mode))
	    warnx("warning, %s is not a character device", fname);
    }
    if (!opt_N)
	check_mounted(fname, sb.st_mode);
    if (opt_ofs && opt_ofs != lseek(fd, opt_ofs, SEEK_SET))
	errx(1, "cannot seek to %jd", (intmax_t)opt_ofs);
    memset(&bpb, 0, sizeof(bpb));
    if (opt_f) {
	getstdfmt(opt_f, &bpb);
	bpb.bpbHugeSectors = bpb.bpbSectors;
	bpb.bpbSectors = 0;
	bpb.bpbBigFATsecs = bpb.bpbFATsecs;
	bpb.bpbFATsecs = 0;
    }
    if (opt_h)
	bpb.bpbHeads = opt_h;
    if (opt_u)
	bpb.bpbSecPerTrack = opt_u;
    if (opt_S)
	bpb.bpbBytesPerSec = opt_S;
    if (opt_s)
	bpb.bpbHugeSectors = opt_s;
    if (oflag)
	bpb.bpbHiddenSecs = opt_o;
    if (!(opt_f || (opt_h && opt_u && opt_S && opt_s && oflag))) {
	off_t delta;
	getdiskinfo(fd, fname, dtype, oflag, &bpb);
	bpb.bpbHugeSectors -= (opt_ofs / bpb.bpbBytesPerSec);
	delta = bpb.bpbHugeSectors % bpb.bpbSecPerTrack;
	if (delta != 0) {
	    warnx("trim %d sectors to adjust to a multiple of %d",
		(int)delta, bpb.bpbSecPerTrack);
	    bpb.bpbHugeSectors -= delta;
	}
	if (bpb.bpbSecPerClust == 0) {	/* set defaults */
	    if (bpb.bpbHugeSectors <= 6000)	/* about 3MB -> 512 bytes */
		bpb.bpbSecPerClust = 1;
	    else if (bpb.bpbHugeSectors <= (1<<17)) /* 64M -> 4k */
		bpb.bpbSecPerClust = 8;
	    else if (bpb.bpbHugeSectors <= (1<<19)) /* 256M -> 8k */
		bpb.bpbSecPerClust = 16;
	    else if (bpb.bpbHugeSectors <= (1<<21)) /* 1G -> 16k */
		bpb.bpbSecPerClust = 32;
	    else
		bpb.bpbSecPerClust = 64;		/* otherwise 32k */
	}
    }
    if (!powerof2(bpb.bpbBytesPerSec))
	errx(1, "bytes/sector (%u) is not a power of 2", bpb.bpbBytesPerSec);
    if (bpb.bpbBytesPerSec < MINBPS)
	errx(1, "bytes/sector (%u) is too small; minimum is %u",
	     bpb.bpbBytesPerSec, MINBPS);
    if (!(fat = opt_F)) {
	if (opt_f)
	    fat = 12;
	else if (!opt_e && (opt_i || opt_k))
	    fat = 32;
    }
    if ((fat == 32 && opt_e) || (fat != 32 && (opt_i || opt_k)))
	errx(1, "-%c is not a legal FAT%s option",
	     fat == 32 ? 'e' : opt_i ? 'i' : 'k',
	     fat == 32 ? "32" : "12/16");
    if (opt_f && fat == 32)
	bpb.bpbRootDirEnts = 0;
    if (opt_b) {
	if (!powerof2(opt_b))
	    errx(1, "block size (%u) is not a power of 2", opt_b);
	if (opt_b < bpb.bpbBytesPerSec)
	    errx(1, "block size (%u) is too small; minimum is %u",
		 opt_b, bpb.bpbBytesPerSec);
	if (opt_b > bpb.bpbBytesPerSec * MAXSPC)
	    errx(1, "block size (%u) is too large; maximum is %u",
		 opt_b, bpb.bpbBytesPerSec * MAXSPC);
	bpb.bpbSecPerClust = opt_b / bpb.bpbBytesPerSec;
    }
    if (opt_c) {
	if (!powerof2(opt_c))
	    errx(1, "sectors/cluster (%u) is not a power of 2", opt_c);
	bpb.bpbSecPerClust = opt_c;
    }
    if (opt_r)
	bpb.bpbResSectors = opt_r;
    if (opt_n) {
	if (opt_n > MAXNFT)
	    errx(1, "number of FATs (%u) is too large; maximum is %u",
		 opt_n, MAXNFT);
	bpb.bpbFATs = opt_n;
    }
    if (opt_e)
	bpb.bpbRootDirEnts = opt_e;
    if (mflag) {
	if (opt_m < 0xf0)
	    errx(1, "illegal media descriptor (%#x)", opt_m);
	bpb.bpbMedia = opt_m;
    }
    if (opt_a)
	bpb.bpbBigFATsecs = opt_a;
    if (opt_i)
	bpb.bpbFSInfo = opt_i;
    if (opt_k)
	bpb.bpbBackup = opt_k;
    bss = 1;
    bname = NULL;
    fd1 = -1;
    if (opt_B) {
	bname = opt_B;
	if (!strchr(bname, '/')) {
	    snprintf(buf, sizeof(buf), "/boot/%s", bname);
	    if (!(bname = strdup(buf)))
		err(1, NULL);
	}
	if ((fd1 = open(bname, O_RDONLY)) == -1 || fstat(fd1, &sb))
	    err(1, "%s", bname);
	if (!S_ISREG(sb.st_mode) || sb.st_size % bpb.bpbBytesPerSec ||
	    sb.st_size < bpb.bpbBytesPerSec ||
	    sb.st_size > bpb.bpbBytesPerSec * MAXU16)
	    errx(1, "%s: inappropriate file type or format", bname);
	bss = sb.st_size / bpb.bpbBytesPerSec;
    }
    if (!bpb.bpbFATs)
	bpb.bpbFATs = 2;
    if (!fat) {
	if (bpb.bpbHugeSectors < (bpb.bpbResSectors ? bpb.bpbResSectors : bss) +
	    howmany((RESFTE + (bpb.bpbSecPerClust ? MINCLS16 : MAXCLS12 + 1)) *
		(bpb.bpbSecPerClust ? 16 : 12) / BPN,
		bpb.bpbBytesPerSec * NPB) *
	    bpb.bpbFATs +
	    howmany(bpb.bpbRootDirEnts ? bpb.bpbRootDirEnts : DEFRDE,
		    bpb.bpbBytesPerSec / sizeof(struct de)) +
	    (bpb.bpbSecPerClust ? MINCLS16 : MAXCLS12 + 1) *
	    (bpb.bpbSecPerClust ? bpb.bpbSecPerClust :
	     howmany(DEFBLK, bpb.bpbBytesPerSec)))
	    fat = 12;
	else if (bpb.bpbRootDirEnts || bpb.bpbHugeSectors <
		 (bpb.bpbResSectors ? bpb.bpbResSectors : bss) +
		 howmany((RESFTE + MAXCLS16) * 2, bpb.bpbBytesPerSec) *
		 bpb.bpbFATs +
		 howmany(DEFRDE, bpb.bpbBytesPerSec / sizeof(struct de)) +
		 (MAXCLS16 + 1) *
		 (bpb.bpbSecPerClust ? bpb.bpbSecPerClust :
		  howmany(8192, bpb.bpbBytesPerSec)))
	    fat = 16;
	else
	    fat = 32;
    }
    x = bss;
    if (fat == 32) {
	if (!bpb.bpbFSInfo) {
	    if (x == MAXU16 || x == bpb.bpbBackup)
		errx(1, "no room for info sector");
	    bpb.bpbFSInfo = x;
	}
	if (bpb.bpbFSInfo != MAXU16 && x <= bpb.bpbFSInfo)
	    x = bpb.bpbFSInfo + 1;
	if (!bpb.bpbBackup) {
	    if (x == MAXU16)
		errx(1, "no room for backup sector");
	    bpb.bpbBackup = x;
	} else if (bpb.bpbBackup != MAXU16 && bpb.bpbBackup == bpb.bpbFSInfo)
	    errx(1, "backup sector would overwrite info sector");
	if (bpb.bpbBackup != MAXU16 && x <= bpb.bpbBackup)
	    x = bpb.bpbBackup + 1;
    }
    if (!bpb.bpbResSectors)
	bpb.bpbResSectors = fat == 32 ? 
	    MAX(x, MAX(16384 / bpb.bpbBytesPerSec, 4)) : x;
    else if (bpb.bpbResSectors < x)
	errx(1, "too few reserved sectors (need %d have %d)", x,
	     bpb.bpbResSectors);
    if (fat != 32 && !bpb.bpbRootDirEnts)
	bpb.bpbRootDirEnts = DEFRDE;
    rds = howmany(bpb.bpbRootDirEnts, bpb.bpbBytesPerSec / sizeof(struct de));
    if (!bpb.bpbSecPerClust)
	for (bpb.bpbSecPerClust = howmany(fat == 16 ? DEFBLK16 :
					  DEFBLK, bpb.bpbBytesPerSec);
	     bpb.bpbSecPerClust < MAXSPC &&
	     bpb.bpbResSectors +
	     howmany((RESFTE + maxcls(fat)) * (fat / BPN),
		     bpb.bpbBytesPerSec * NPB) *
	     bpb.bpbFATs +
	     rds +
	     (u_int64_t) (maxcls(fat) + 1) *
	     bpb.bpbSecPerClust <= bpb.bpbHugeSectors;
	     bpb.bpbSecPerClust <<= 1)
	    continue;
    if (fat != 32 && bpb.bpbBigFATsecs > MAXU16)
	errx(1, "too many sectors/FAT for FAT12/16");
    x1 = bpb.bpbResSectors + rds;
    x = bpb.bpbBigFATsecs ? bpb.bpbBigFATsecs : 1;
    if (x1 + (u_int64_t)x * bpb.bpbFATs > bpb.bpbHugeSectors)
	errx(1, "meta data exceeds file system size");
    x1 += x * bpb.bpbFATs;
    x = (u_int64_t)(bpb.bpbHugeSectors - x1) * bpb.bpbBytesPerSec * NPB /
	(bpb.bpbSecPerClust * bpb.bpbBytesPerSec * NPB + fat /
	 BPN * bpb.bpbFATs);
    x2 = howmany((RESFTE + MIN(x, maxcls(fat))) * (fat / BPN),
		 bpb.bpbBytesPerSec * NPB);
    if (!bpb.bpbBigFATsecs) {
	bpb.bpbBigFATsecs = x2;
	x1 += (bpb.bpbBigFATsecs - 1) * bpb.bpbFATs;
    }
    cls = (bpb.bpbHugeSectors - x1) / bpb.bpbSecPerClust;
    x = (u_int64_t)bpb.bpbBigFATsecs * bpb.bpbBytesPerSec * NPB / (fat / BPN) -
	RESFTE;
    if (cls > x)
	cls = x;
    if (bpb.bpbBigFATsecs < x2)
	warnx("warning: sectors/FAT limits file system to %u clusters",
	      cls);
    if (cls < mincls(fat))
	errx(1, "%u clusters too few clusters for FAT%u, need %u", cls, fat,
	    mincls(fat));
    if (cls > maxcls(fat)) {
	cls = maxcls(fat);
	bpb.bpbHugeSectors = x1 + (cls + 1) * bpb.bpbSecPerClust - 1;
	warnx("warning: FAT type limits file system to %u sectors",
	      bpb.bpbHugeSectors);
    }
    printf("%s: %u sector%s in %u FAT%u cluster%s "
	   "(%u bytes/cluster)\n", fname, cls * bpb.bpbSecPerClust,
	   cls * bpb.bpbSecPerClust == 1 ? "" : "s", cls, fat,
	   cls == 1 ? "" : "s", bpb.bpbBytesPerSec * bpb.bpbSecPerClust);
    if (!bpb.bpbMedia)
	bpb.bpbMedia = !bpb.bpbHiddenSecs ? 0xf0 : 0xf8;
    if (fat == 32)
	bpb.bpbRootClust = RESFTE;
    if (bpb.bpbHiddenSecs + bpb.bpbHugeSectors <= MAXU16) {
	bpb.bpbSectors = bpb.bpbHugeSectors;
	bpb.bpbHugeSectors = 0;
    }
    if (fat != 32) {
	bpb.bpbFATsecs = bpb.bpbBigFATsecs;
	bpb.bpbBigFATsecs = 0;
    }
    print_bpb(&bpb);
    if (!opt_N) {
	gettimeofday(&tv, NULL);
	now = tv.tv_sec;
	tm = localtime(&now);
	if (!(img = malloc(bpb.bpbBytesPerSec)))
	    err(1, NULL);
	dir = bpb.bpbResSectors + (bpb.bpbFATsecs ? bpb.bpbFATsecs :
				   bpb.bpbBigFATsecs) * bpb.bpbFATs;
	memset(&si_sa, 0, sizeof(si_sa));
	si_sa.sa_handler = infohandler;
	if (sigaction(SIGINFO, &si_sa, NULL) == -1)
		err(1, "sigaction SIGINFO");
	for (lsn = 0; lsn < dir + (fat == 32 ? bpb.bpbSecPerClust : rds); lsn++) {
	    if (got_siginfo) {
		    fprintf(stderr,"%s: writing sector %u of %u (%u%%)\n",
			fname, lsn,
			(dir + (fat == 32 ? bpb.bpbSecPerClust: rds)),
			(lsn * 100) / (dir +
			    (fat == 32 ? bpb.bpbSecPerClust: rds)));
		    got_siginfo = 0;
	    }
	    x = lsn;
	    if (opt_B &&
		fat == 32 && bpb.bpbBackup != MAXU16 &&
		bss <= bpb.bpbBackup && x >= bpb.bpbBackup) {
		x -= bpb.bpbBackup;
		if (!x && lseek(fd1, opt_ofs, SEEK_SET))
		    err(1, "%s", bname);
	    }
	    if (opt_B && x < bss) {
		if ((n = read(fd1, img, bpb.bpbBytesPerSec)) == -1)
		    err(1, "%s", bname);
		if ((unsigned)n != bpb.bpbBytesPerSec)
		    errx(1, "%s: can't read sector %u", bname, x);
	    } else
		memset(img, 0, bpb.bpbBytesPerSec);
	    if (!lsn ||
		(fat == 32 && bpb.bpbBackup != MAXU16 &&
		 lsn == bpb.bpbBackup)) {
		x1 = sizeof(struct bs);
		bsbpb = (struct bsbpb *)(img + x1);
		mk2(bsbpb->bpbBytesPerSec, bpb.bpbBytesPerSec);
		mk1(bsbpb->bpbSecPerClust, bpb.bpbSecPerClust);
		mk2(bsbpb->bpbResSectors, bpb.bpbResSectors);
		mk1(bsbpb->bpbFATs, bpb.bpbFATs);
		mk2(bsbpb->bpbRootDirEnts, bpb.bpbRootDirEnts);
		mk2(bsbpb->bpbSectors, bpb.bpbSectors);
		mk1(bsbpb->bpbMedia, bpb.bpbMedia);
		mk2(bsbpb->bpbFATsecs, bpb.bpbFATsecs);
		mk2(bsbpb->bpbSecPerTrack, bpb.bpbSecPerTrack);
		mk2(bsbpb->bpbHeads, bpb.bpbHeads);
		mk4(bsbpb->bpbHiddenSecs, bpb.bpbHiddenSecs);
		mk4(bsbpb->bpbHugeSectors, bpb.bpbHugeSectors);
		x1 += sizeof(struct bsbpb);
		if (fat == 32) {
		    bsxbpb = (struct bsxbpb *)(img + x1);
		    mk4(bsxbpb->bpbBigFATsecs, bpb.bpbBigFATsecs);
		    mk2(bsxbpb->bpbExtFlags, 0);
		    mk2(bsxbpb->bpbFSVers, 0);
		    mk4(bsxbpb->bpbRootClust, bpb.bpbRootClust);
		    mk2(bsxbpb->bpbFSInfo, bpb.bpbFSInfo);
		    mk2(bsxbpb->bpbBackup, bpb.bpbBackup);
		    x1 += sizeof(struct bsxbpb);
		}
		bsx = (struct bsx *)(img + x1);
		mk1(bsx->exBootSignature, 0x29);
		if (Iflag)
		    x = opt_I;
		else
		    x = (((u_int)(1 + tm->tm_mon) << 8 |
			  (u_int)tm->tm_mday) +
			 ((u_int)tm->tm_sec << 8 |
			  (u_int)(tv.tv_usec / 10))) << 16 |
			((u_int)(1900 + tm->tm_year) +
			 ((u_int)tm->tm_hour << 8 |
			  (u_int)tm->tm_min));
		mk4(bsx->exVolumeID, x);
		mklabel(bsx->exVolumeLabel, opt_L ? opt_L : "NO_NAME");
		sprintf(buf, "FAT%u", fat);
		setstr(bsx->exFileSysType, buf, sizeof(bsx->exFileSysType));
		if (!opt_B) {
		    x1 += sizeof(struct bsx);
		    bs = (struct bs *)img;
		    mk1(bs->bsJump[0], 0xeb);
		    mk1(bs->bsJump[1], x1 - 2);
		    mk1(bs->bsJump[2], 0x90);
		    setstr(bs->bsOemName, opt_O ? opt_O : "BSD4.4  ",
			   sizeof(bs->bsOemName));
		    memcpy(img + x1, bootcode, sizeof(bootcode));
		    mk2(img + MINBPS - 2, DOSMAGIC);
		}
	    } else if (fat == 32 && bpb.bpbFSInfo != MAXU16 &&
		       (lsn == bpb.bpbFSInfo ||
			(bpb.bpbBackup != MAXU16 &&
			 lsn == bpb.bpbBackup + bpb.bpbFSInfo))) {
		mk4(img, 0x41615252);
		mk4(img + MINBPS - 28, 0x61417272);
		mk4(img + MINBPS - 24, 0xffffffff);
		mk4(img + MINBPS - 20, bpb.bpbRootClust);
		mk2(img + MINBPS - 2, DOSMAGIC);
	    } else if (lsn >= bpb.bpbResSectors && lsn < dir &&
		       !((lsn - bpb.bpbResSectors) %
			 (bpb.bpbFATsecs ? bpb.bpbFATsecs :
			  bpb.bpbBigFATsecs))) {
		mk1(img[0], bpb.bpbMedia);
		for (x = 1; x < fat * (fat == 32 ? 3 : 2) / 8; x++)
		    mk1(img[x], fat == 32 && x % 4 == 3 ? 0x0f : 0xff);
	    } else if (lsn == dir && opt_L) {
		de = (struct de *)img;
		mklabel(de->deName, opt_L);
		mk1(de->deAttributes, 050);
		x = (u_int)tm->tm_hour << 11 |
		    (u_int)tm->tm_min << 5 |
		    (u_int)tm->tm_sec >> 1;
		mk2(de->deMTime, x);
		x = (u_int)(tm->tm_year - 80) << 9 |
		    (u_int)(tm->tm_mon + 1) << 5 |
		    (u_int)tm->tm_mday;
		mk2(de->deMDate, x);
	    }
	    if ((n = write(fd, img, bpb.bpbBytesPerSec)) == -1)
		err(1, "%s", fname);
	    if ((unsigned)n != bpb.bpbBytesPerSec)
		errx(1, "%s: can't write sector %u", fname, lsn);
	}
    }
    return 0;
}

/*
 * Exit with error if file system is mounted.
 */
static void
check_mounted(const char *fname, mode_t mode)
{
    struct statfs *mp;
    const char *s1, *s2;
    size_t len;
    int n, r;

    if (!(n = getmntinfo(&mp, MNT_NOWAIT)))
	err(1, "getmntinfo");
    len = strlen(_PATH_DEV);
    s1 = fname;
    if (!strncmp(s1, _PATH_DEV, len))
	s1 += len;
    r = S_ISCHR(mode) && s1 != fname && *s1 == 'r';
    for (; n--; mp++) {
	s2 = mp->f_mntfromname;
	if (!strncmp(s2, _PATH_DEV, len))
	    s2 += len;
	if ((r && s2 != mp->f_mntfromname && !strcmp(s1 + 1, s2)) ||
	    !strcmp(s1, s2))
	    errx(1, "%s is mounted on %s", fname, mp->f_mntonname);
    }
}

/*
 * Get a standard format.
 */
static void
getstdfmt(const char *fmt, struct bpb *bpb)
{
    u_int x, i;

    x = sizeof(stdfmt) / sizeof(stdfmt[0]);
    for (i = 0; i < x && strcmp(fmt, stdfmt[i].name); i++);
    if (i == x)
	errx(1, "%s: unknown standard format", fmt);
    *bpb = stdfmt[i].bpb;
}

/*
 * Get disk slice, partition, and geometry information.
 */
static void
getdiskinfo(int fd, const char *fname, const char *dtype, __unused int oflag,
	    struct bpb *bpb)
{
    struct disklabel *lp, dlp;
    struct fd_type type;
    off_t ms, hs = 0;

    lp = NULL;

    /* If the user specified a disk type, try to use that */
    if (dtype != NULL) {
	lp = getdiskbyname(dtype);
    }

    /* Maybe it's a floppy drive */
    if (lp == NULL) {
	if (ioctl(fd, DIOCGMEDIASIZE, &ms) == -1) {
	    struct stat st;

	    if (fstat(fd, &st))
		err(1, "cannot get disk size");
	    /* create a fake geometry for a file image */
	    ms = st.st_size;
	    dlp.d_secsize = 512;
	    dlp.d_nsectors = 63;
	    dlp.d_ntracks = 255;
	    dlp.d_secperunit = ms / dlp.d_secsize;
	    lp = &dlp;
	} else if (ioctl(fd, FD_GTYPE, &type) != -1) {
	    dlp.d_secsize = 128 << type.secsize;
	    dlp.d_nsectors = type.sectrac;
	    dlp.d_ntracks = type.heads;
	    dlp.d_secperunit = ms / dlp.d_secsize;
	    lp = &dlp;
	}
    }

    /* Maybe it's a fixed drive */
    if (lp == NULL) {
	if (bpb->bpbBytesPerSec)
	    dlp.d_secsize = bpb->bpbBytesPerSec;
	if (ioctl(fd, DIOCGDINFO, &dlp) == -1) {
	    if (bpb->bpbBytesPerSec == 0 && ioctl(fd, DIOCGSECTORSIZE,
						  &dlp.d_secsize) == -1)
		err(1, "cannot get sector size");

	    dlp.d_secperunit = ms / dlp.d_secsize;

	    if (bpb->bpbSecPerTrack == 0 && ioctl(fd, DIOCGFWSECTORS,
						  &dlp.d_nsectors) == -1) {
		warn("cannot get number of sectors per track");
		dlp.d_nsectors = 63;
	    }
	    if (bpb->bpbHeads == 0 &&
	        ioctl(fd, DIOCGFWHEADS, &dlp.d_ntracks) == -1) {
		warn("cannot get number of heads");
		if (dlp.d_secperunit <= 63*1*1024)
		    dlp.d_ntracks = 1;
		else if (dlp.d_secperunit <= 63*16*1024)
		    dlp.d_ntracks = 16;
		else
		    dlp.d_ntracks = 255;
	    }
	}

	hs = (ms / dlp.d_secsize) - dlp.d_secperunit;
	lp = &dlp;
    }

    if (bpb->bpbBytesPerSec == 0)
	bpb->bpbBytesPerSec = ckgeom(fname, lp->d_secsize, "bytes/sector");
    if (bpb->bpbSecPerTrack == 0)
	bpb->bpbSecPerTrack = ckgeom(fname, lp->d_nsectors, "sectors/track");
    if (bpb->bpbHeads == 0)
	bpb->bpbHeads = ckgeom(fname, lp->d_ntracks, "drive heads");
    if (bpb->bpbHugeSectors == 0)
	bpb->bpbHugeSectors = lp->d_secperunit;
    if (bpb->bpbHiddenSecs == 0)
	bpb->bpbHiddenSecs = hs;
}

/*
 * Print out BPB values.
 */
static void
print_bpb(struct bpb *bpb)
{
    printf("BytesPerSec=%u SecPerClust=%u ResSectors=%u FATs=%u",
	   bpb->bpbBytesPerSec, bpb->bpbSecPerClust, bpb->bpbResSectors,
	   bpb->bpbFATs);
    if (bpb->bpbRootDirEnts)
	printf(" RootDirEnts=%u", bpb->bpbRootDirEnts);
    if (bpb->bpbSectors)
	printf(" Sectors=%u", bpb->bpbSectors);
    printf(" Media=%#x", bpb->bpbMedia);
    if (bpb->bpbFATsecs)
	printf(" FATsecs=%u", bpb->bpbFATsecs);
    printf(" SecPerTrack=%u Heads=%u HiddenSecs=%u", bpb->bpbSecPerTrack,
	   bpb->bpbHeads, bpb->bpbHiddenSecs);
    if (bpb->bpbHugeSectors)
	printf(" HugeSectors=%u", bpb->bpbHugeSectors);
    if (!bpb->bpbFATsecs) {
	printf(" FATsecs=%u RootCluster=%u", bpb->bpbBigFATsecs,
	       bpb->bpbRootClust);
	printf(" FSInfo=");
	printf(bpb->bpbFSInfo == MAXU16 ? "%#x" : "%u", bpb->bpbFSInfo);
	printf(" Backup=");
	printf(bpb->bpbBackup == MAXU16 ? "%#x" : "%u", bpb->bpbBackup);
    }
    printf("\n");
}

/*
 * Check a disk geometry value.
 */
static u_int
ckgeom(const char *fname, u_int val, const char *msg)
{
    if (!val)
	errx(1, "%s: no default %s", fname, msg);
    if (val > MAXU16)
	errx(1, "%s: illegal %s %d", fname, msg, val);
    return val;
}

/*
 * Convert and check a numeric option argument.
 */
static u_int
argtou(const char *arg, u_int lo, u_int hi, const char *msg)
{
    char *s;
    u_long x;

    errno = 0;
    x = strtoul(arg, &s, 0);
    if (errno || !*arg || *s || x < lo || x > hi)
	errx(1, "%s: bad %s", arg, msg);
    return x;
}

/*
 * Same for off_t, with optional skmgpP suffix
 */
static off_t
argtooff(const char *arg, const char *msg)
{
    char *s;
    off_t x;

    errno = 0;
    x = strtoll(arg, &s, 0);
    /* allow at most one extra char */
    if (errno || x < 0 || (s[0] && s[1]) )
	errx(1, "%s: bad %s", arg, msg);
    if (*s) {	/* the extra char is the multiplier */
	switch (*s) {
	default:
	    errx(1, "%s: bad %s", arg, msg);
	    /* notreached */
	
	case 's':		/* sector */
	case 'S':
	    x <<= 9;		/* times 512 */
	    break;

	case 'k':		/* kilobyte */
	case 'K':
	    x <<= 10;		/* times 1024 */
	    break;

	case 'm':		/* megabyte */
	case 'M':
	    x <<= 20;		/* times 1024*1024 */
	    break;

	case 'g':		/* gigabyte */
	case 'G':
	    x <<= 30;		/* times 1024*1024*1024 */
	    break;

	case 'p':		/* partition start */
	case 'P':
	case 'l':		/* partition length */
	case 'L':
	    errx(1, "%s: not supported yet %s", arg, msg);
	    /* notreached */
	}
    }
    return x;
}

/*
 * Check a volume label.
 */
static int
oklabel(const char *src)
{
    int c, i;

    for (i = 0; i <= 11; i++) {
	c = (u_char)*src++;
	if (c < ' ' + !i || strchr("\"*+,./:;<=>?[\\]|", c))
	    break;
    }
    return i && !c;
}

/*
 * Make a volume label.
 */
static void
mklabel(u_int8_t *dest, const char *src)
{
    int c, i;

    for (i = 0; i < 11; i++) {
	c = *src ? toupper(*src++) : ' ';
	*dest++ = !i && c == '\xe5' ? 5 : c;
    }
}

/*
 * Copy string, padding with spaces.
 */
static void
setstr(u_int8_t *dest, const char *src, size_t len)
{
    while (len--)
	*dest++ = *src ? *src++ : ' ';
}

/*
 * Print usage message.
 */
static void
usage(void)
{
	fprintf(stderr,
	    "usage: newfs_msdos [ -options ] special [disktype]\n"
	    "where the options are:\n"
	    "\t-@ create file system at specified offset\n"                         
	    "\t-B get bootstrap from file\n"
	    "\t-C create image file with specified size\n"
	    "\t-F FAT type (12, 16, or 32)\n"
	    "\t-I volume ID\n"
	    "\t-L volume label\n"
	    "\t-N don't create file system: just print out parameters\n"
	    "\t-O OEM string\n"
	    "\t-S bytes/sector\n"
	    "\t-a sectors/FAT\n"
	    "\t-b block size\n"
	    "\t-c sectors/cluster\n"
	    "\t-e root directory entries\n"
	    "\t-f standard format\n"
	    "\t-h drive heads\n"
	    "\t-i file system info sector\n"
	    "\t-k backup boot sector\n"
	    "\t-m media descriptor\n"
	    "\t-n number of FATs\n"
	    "\t-o hidden sectors\n"
	    "\t-r reserved sectors\n"
	    "\t-s file system size (sectors)\n"
	    "\t-u sectors/track\n");
	exit(1);
}

static void
infohandler(int sig __unused)
{

	got_siginfo = 1;
}
