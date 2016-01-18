/*-
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 * Copyright (c) 2014 Nathan Whitehorn
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
#include <sys/dirent.h>
#include <machine/elf.h>
#include <machine/stdarg.h>

#include <efi.h>
#include <eficonsctl.h>

#define _PATH_LOADER	"/boot/loader.efi"
#define _PATH_KERNEL	"/boot/kernel/kernel"

#define BSIZEMAX	16384

typedef int putc_func_t(char c, void *arg);

struct sp_data {
	char	*sp_buf;
	u_int	sp_len;
	u_int	sp_size;
};

static const char digits[] = "0123456789abcdef";

static void panic(const char *fmt, ...) __dead2;
static int printf(const char *fmt, ...);
static int putchar(char c, void *arg);
static int vprintf(const char *fmt, va_list ap);
static int vsnprintf(char *str, size_t sz, const char *fmt, va_list ap);

static int __printf(const char *fmt, putc_func_t *putc, void *arg, va_list ap);
static int __putc(char c, void *arg);
static int __puts(const char *s, putc_func_t *putc, void *arg);
static int __sputc(char c, void *arg);
static char *__uitoa(char *buf, u_int val, int base);
static char *__ultoa(char *buf, u_long val, int base);

static int domount(EFI_DEVICE_PATH *device, EFI_BLOCK_IO *blkio, int quiet);
static void load(const char *fname);

EFI_SYSTEM_TABLE *systab;
EFI_HANDLE *image;

static void
bcopy(const void *src, void *dst, size_t len)
{
	const char *s = src;
	char *d = dst;

	while (len-- != 0)
		*d++ = *s++;
}

static void
memcpy(void *dst, const void *src, size_t len)
{
	bcopy(src, dst, len);
}

static void
bzero(void *b, size_t len)
{
	char *p = b;

	while (len-- != 0)
		*p++ = 0;
}

static int
strcmp(const char *s1, const char *s2)
{
	for (; *s1 == *s2 && *s1; s1++, s2++)
		;
	return ((u_char)*s1 - (u_char)*s2);
}

static EFI_GUID BlockIoProtocolGUID = BLOCK_IO_PROTOCOL;
static EFI_GUID DevicePathGUID = DEVICE_PATH_PROTOCOL;
static EFI_GUID LoadedImageGUID = LOADED_IMAGE_PROTOCOL;
static EFI_GUID ConsoleControlGUID = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

static EFI_BLOCK_IO *bootdev;
static EFI_DEVICE_PATH *bootdevpath;
static EFI_HANDLE *bootdevhandle;

EFI_STATUS efi_main(EFI_HANDLE Ximage, EFI_SYSTEM_TABLE* Xsystab)
{
	EFI_HANDLE handles[128];
	EFI_BLOCK_IO *blkio;
	UINTN i, nparts = sizeof(handles);
	EFI_STATUS status;
	EFI_DEVICE_PATH *devpath;
	EFI_BOOT_SERVICES *BS;
	EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;
	char *path = _PATH_LOADER;

	systab = Xsystab;
	image = Ximage;

	BS = systab->BootServices;
	status = BS->LocateProtocol(&ConsoleControlGUID, NULL,
	    (VOID **)&ConsoleControl);
	if (status == EFI_SUCCESS)
		(void)ConsoleControl->SetMode(ConsoleControl,
		    EfiConsoleControlScreenText);

	printf(" \n>> FreeBSD EFI boot block\n");
	printf("   Loader path: %s\n", path);

	status = systab->BootServices->LocateHandle(ByProtocol,
	    &BlockIoProtocolGUID, NULL, &nparts, handles);
	nparts /= sizeof(handles[0]);

	for (i = 0; i < nparts; i++) {
		status = systab->BootServices->HandleProtocol(handles[i],
		    &DevicePathGUID, (void **)&devpath);
		if (EFI_ERROR(status))
			continue;

		while (!IsDevicePathEnd(NextDevicePathNode(devpath)))
			devpath = NextDevicePathNode(devpath);

		status = systab->BootServices->HandleProtocol(handles[i],
		    &BlockIoProtocolGUID, (void **)&blkio);
		if (EFI_ERROR(status))
			continue;

		if (!blkio->Media->LogicalPartition)
			continue;

		if (domount(devpath, blkio, 1) >= 0)
			break;
	}

	if (i == nparts)
		panic("No bootable partition found");

	bootdevhandle = handles[i];
	load(path);

	panic("Load failed");

	return EFI_SUCCESS;
}

static int
dskread(void *buf, u_int64_t lba, int nblk)
{
	EFI_STATUS status;
	int size;

	lba = lba / (bootdev->Media->BlockSize / DEV_BSIZE);
	size = nblk * DEV_BSIZE;
	status = bootdev->ReadBlocks(bootdev, bootdev->Media->MediaId, lba,
	    size, buf);

	if (EFI_ERROR(status))
		return (-1);

	return (0);
}

#include "ufsread.c"

static ssize_t
fsstat(ufs_ino_t inode)
{
#ifndef UFS2_ONLY
	static struct ufs1_dinode dp1;
	ufs1_daddr_t addr1;
#endif
#ifndef UFS1_ONLY
	static struct ufs2_dinode dp2;
#endif
	static struct fs fs;
	static ufs_ino_t inomap;
	char *blkbuf;
	void *indbuf;
	size_t n, nb, size, off, vboff;
	ufs_lbn_t lbn;
	ufs2_daddr_t addr2, vbaddr;
	static ufs2_daddr_t blkmap, indmap;
	u_int u;

	blkbuf = dmadat->blkbuf;
	indbuf = dmadat->indbuf;
	if (!dsk_meta) {
		inomap = 0;
		for (n = 0; sblock_try[n] != -1; n++) {
			if (dskread(dmadat->sbbuf, sblock_try[n] / DEV_BSIZE,
			    SBLOCKSIZE / DEV_BSIZE))
				return -1;
			memcpy(&fs, dmadat->sbbuf, sizeof(struct fs));
			if ((
#if defined(UFS1_ONLY)
			    fs.fs_magic == FS_UFS1_MAGIC
#elif defined(UFS2_ONLY)
			    (fs.fs_magic == FS_UFS2_MAGIC &&
			    fs.fs_sblockloc == sblock_try[n])
#else
			    fs.fs_magic == FS_UFS1_MAGIC ||
			    (fs.fs_magic == FS_UFS2_MAGIC &&
			    fs.fs_sblockloc == sblock_try[n])
#endif
			    ) &&
			    fs.fs_bsize <= MAXBSIZE &&
			    fs.fs_bsize >= sizeof(struct fs))
				break;
		}
		if (sblock_try[n] == -1) {
			printf("Not ufs\n");
			return -1;
		}
		dsk_meta++;
	} else
		memcpy(&fs, dmadat->sbbuf, sizeof(struct fs));
	if (!inode)
		return 0;
	if (inomap != inode) {
		n = IPERVBLK(&fs);
		if (dskread(blkbuf, INO_TO_VBA(&fs, n, inode), DBPERVBLK))
			return -1;
		n = INO_TO_VBO(n, inode);
#if defined(UFS1_ONLY)
		memcpy(&dp1, (struct ufs1_dinode *)blkbuf + n,
		    sizeof(struct ufs1_dinode));
#elif defined(UFS2_ONLY)
		memcpy(&dp2, (struct ufs2_dinode *)blkbuf + n,
		    sizeof(struct ufs2_dinode));
#else
		if (fs.fs_magic == FS_UFS1_MAGIC)
			memcpy(&dp1, (struct ufs1_dinode *)blkbuf + n,
			    sizeof(struct ufs1_dinode));
		else
			memcpy(&dp2, (struct ufs2_dinode *)blkbuf + n,
			    sizeof(struct ufs2_dinode));
#endif
		inomap = inode;
		fs_off = 0;
		blkmap = indmap = 0;
	}
	size = DIP(di_size);
	n = size - fs_off;
	return (n);
}

static struct dmadat __dmadat;

static int
domount(EFI_DEVICE_PATH *device, EFI_BLOCK_IO *blkio, int quiet)
{

	dmadat = &__dmadat;
	bootdev = blkio;
	bootdevpath = device;
	if (fsread(0, NULL, 0)) {
		if (!quiet)
			printf("domount: can't read superblock\n");
		return (-1);
	}
	if (!quiet)
		printf("Succesfully mounted UFS filesystem\n");
	return (0);
}

static void
load(const char *fname)
{
	ufs_ino_t ino;
	EFI_STATUS status;
	EFI_HANDLE loaderhandle;
	EFI_LOADED_IMAGE *loaded_image;
	void *buffer;
	size_t bufsize;

	if ((ino = lookup(fname)) == 0) {
		printf("File %s not found\n", fname);
		return;
	}

	bufsize = fsstat(ino);
	status = systab->BootServices->AllocatePool(EfiLoaderData,
	    bufsize, &buffer);
	fsread(ino, buffer, bufsize);

	/* XXX: For secure boot, we need our own loader here */
	status = systab->BootServices->LoadImage(TRUE, image, bootdevpath,
	    buffer, bufsize, &loaderhandle);
	if (EFI_ERROR(status))
		printf("LoadImage failed with error %lu\n",
		    EFI_ERROR_CODE(status));

	status = systab->BootServices->HandleProtocol(loaderhandle,
	    &LoadedImageGUID, (VOID**)&loaded_image);
	if (EFI_ERROR(status))
		printf("HandleProtocol failed with error %lu\n",
		    EFI_ERROR_CODE(status));

	loaded_image->DeviceHandle = bootdevhandle;

	status = systab->BootServices->StartImage(loaderhandle, NULL, NULL);
	if (EFI_ERROR(status))
		printf("StartImage failed with error %lu\n",
		    EFI_ERROR_CODE(status));
}

static void
panic(const char *fmt, ...)
{
	char buf[128];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	printf("panic: %s\n", buf);
	va_end(ap);

	while (1) {}
}

static int
printf(const char *fmt, ...)
{
	va_list ap;
	int ret;

	/* Don't annoy the user as we probe for partitions */
	if (strcmp(fmt,"Not ufs\n") == 0)
		return 0;

	va_start(ap, fmt);
	ret = vprintf(fmt, ap);
	va_end(ap);
	return (ret);
}

static int
putchar(char c, void *arg)
{
	CHAR16 buf[2];

	if (c == '\n') {
		buf[0] = '\r';
		buf[1] = 0;
		systab->ConOut->OutputString(systab->ConOut, buf);
	}
	buf[0] = c;
	buf[1] = 0;
	systab->ConOut->OutputString(systab->ConOut, buf);
	return (1);
}

static int
vprintf(const char *fmt, va_list ap)
{
	int ret;

	ret = __printf(fmt, putchar, 0, ap);
	return (ret);
}

static int
vsnprintf(char *str, size_t sz, const char *fmt, va_list ap)
{
	struct sp_data sp;
	int ret;

	sp.sp_buf = str;
	sp.sp_len = 0;
	sp.sp_size = sz;
	ret = __printf(fmt, __sputc, &sp, ap);
	return (ret);
}

static int
__printf(const char *fmt, putc_func_t *putc, void *arg, va_list ap)
{
	char buf[(sizeof(long) * 8) + 1];
	char *nbuf;
	u_long ul;
	u_int ui;
	int lflag;
	int sflag;
	char *s;
	int pad;
	int ret;
	int c;

	nbuf = &buf[sizeof buf - 1];
	ret = 0;
	while ((c = *fmt++) != 0) {
		if (c != '%') {
			ret += putc(c, arg);
			continue;
		}
		lflag = 0;
		sflag = 0;
		pad = 0;
reswitch:	c = *fmt++;
		switch (c) {
		case '#':
			sflag = 1;
			goto reswitch;
		case '%':
			ret += putc('%', arg);
			break;
		case 'c':
			c = va_arg(ap, int);
			ret += putc(c, arg);
			break;
		case 'd':
			if (lflag == 0) {
				ui = (u_int)va_arg(ap, int);
				if (ui < (int)ui) {
					ui = -ui;
					ret += putc('-', arg);
				}
				s = __uitoa(nbuf, ui, 10);
			} else {
				ul = (u_long)va_arg(ap, long);
				if (ul < (long)ul) {
					ul = -ul;
					ret += putc('-', arg);
				}
				s = __ultoa(nbuf, ul, 10);
			}
			ret += __puts(s, putc, arg);
			break;
		case 'l':
			lflag = 1;
			goto reswitch;
		case 'o':
			if (lflag == 0) {
				ui = (u_int)va_arg(ap, u_int);
				s = __uitoa(nbuf, ui, 8);
			} else {
				ul = (u_long)va_arg(ap, u_long);
				s = __ultoa(nbuf, ul, 8);
			}
			ret += __puts(s, putc, arg);
			break;
		case 'p':
			ul = (u_long)va_arg(ap, void *);
			s = __ultoa(nbuf, ul, 16);
			ret += __puts("0x", putc, arg);
			ret += __puts(s, putc, arg);
			break;
		case 's':
			s = va_arg(ap, char *);
			ret += __puts(s, putc, arg);
			break;
		case 'u':
			if (lflag == 0) {
				ui = va_arg(ap, u_int);
				s = __uitoa(nbuf, ui, 10);
			} else {
				ul = va_arg(ap, u_long);
				s = __ultoa(nbuf, ul, 10);
			}
			ret += __puts(s, putc, arg);
			break;
		case 'x':
			if (lflag == 0) {
				ui = va_arg(ap, u_int);
				s = __uitoa(nbuf, ui, 16);
			} else {
				ul = va_arg(ap, u_long);
				s = __ultoa(nbuf, ul, 16);
			}
			if (sflag)
				ret += __puts("0x", putc, arg);
			ret += __puts(s, putc, arg);
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			pad = pad * 10 + c - '0';
			goto reswitch;
		default:
			break;
		}
	}
	return (ret);
}

static int
__sputc(char c, void *arg)
{
	struct sp_data *sp;

	sp = arg;
	if (sp->sp_len < sp->sp_size)
		sp->sp_buf[sp->sp_len++] = c;
	sp->sp_buf[sp->sp_len] = '\0';
	return (1);
}

static int
__puts(const char *s, putc_func_t *putc, void *arg)
{
	const char *p;
	int ret;

	ret = 0;
	for (p = s; *p != '\0'; p++)
		ret += putc(*p, arg);
	return (ret);
}

static char *
__uitoa(char *buf, u_int ui, int base)
{
	char *p;

	p = buf;
	*p = '\0';
	do
		*--p = digits[ui % base];
	while ((ui /= base) != 0);
	return (p);
}

static char *
__ultoa(char *buf, u_long ul, int base)
{
	char *p;

	p = buf;
	*p = '\0';
	do
		*--p = digits[ul % base];
	while ((ul /= base) != 0);
	return (p);
}
