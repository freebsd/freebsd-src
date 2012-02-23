/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <err.h>
#include <sys/sysctl.h>
#include <sys/stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/uuid.h>
#include <sys/gpt.h>
#include <paths.h>
#include "libdisk.h"

#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <uuid.h>

const enum platform platform =
#if defined (P_DEBUG)
	P_DEBUG
#elif defined (PC98)
	p_pc98
#elif defined(__i386__)
	p_i386
#elif defined(__alpha__)
	p_alpha
#elif defined(__sparc64__)
	p_sparc64
#elif defined(__ia64__)
	p_ia64
#elif defined(__ppc__)
	p_ppc
#elif defined(__amd64__)
	p_amd64
#elif defined(__arm__)
	p_arm
#elif defined(__mips__)
	p_mips
#else
	IHAVENOIDEA
#endif
	;

const char *
chunk_name(chunk_e type)
{
	switch(type) {
	case unused:	return ("unused");
	case mbr:	return ("mbr");
	case part:	return ("part");
	case gpt:	return ("gpt");
	case pc98:	return ("pc98");
	case sun:	return ("sun");
	case freebsd:	return ("freebsd");
	case fat:	return ("fat");
	case spare:	return ("spare");
	case efi:	return ("efi");
	case apple:     return ("apple");
	default:	return ("??");
	}
}

struct disk *
Open_Disk(const char *name)
{
	struct disk *d;
	char *conftxt;
	size_t txtsize;
	int error;

	error = sysctlbyname("kern.geom.conftxt", NULL, &txtsize, NULL, 0);
	if (error) {
		warn("kern.geom.conftxt sysctl not available, giving up!");
		return (NULL);
	}
	conftxt = malloc(txtsize+1);
	if (conftxt == NULL) {
		warn("cannot malloc memory for conftxt");
		return (NULL);
	}
	error = sysctlbyname("kern.geom.conftxt", conftxt, &txtsize, NULL, 0);
	if (error) {
		warn("error reading kern.geom.conftxt from the system");
		free(conftxt);
		return (NULL);
	}
	conftxt[txtsize] = '\0';	/* in case kernel bug is still there */
	d = Int_Open_Disk(name, conftxt);
	free(conftxt);

	return (d);
}

void
Debug_Disk(struct disk *d)
{

	printf("Debug_Disk(%s)", d->name);

#ifndef __ia64__
	printf("  bios_geom=%lu/%lu/%lu = %lu\n",
		d->bios_cyl, d->bios_hd, d->bios_sect,
		d->bios_cyl * d->bios_hd * d->bios_sect);
#if defined(PC98)
	printf("  boot1=%p, boot2=%p, bootipl=%p, bootmenu=%p\n",
		d->boot1, d->boot2, d->bootipl, d->bootmenu);
#elif defined(__i386__) || defined(__amd64__)
	printf("  boot1=%p, boot2=%p, bootmgr=%p\n",
		d->boot1, d->boot2, d->bootmgr);
#elif defined(__alpha__)
	printf("  boot1=%p, bootmgr=%p\n",
		d->boot1, d->bootmgr);
#else
/* Should be: error "Debug_Disk: unknown arch"; */
#endif
#else	/* __ia64__ */
	printf("  media size=%lu, sector size=%lu\n", d->media_size,
	    d->sector_size);
#endif

	Debug_Chunk(d->chunks);
}

void
Free_Disk(struct disk *d)
{
	if (d->chunks)
		Free_Chunk(d->chunks);
	if (d->name)
		free(d->name);
#ifdef PC98
	if (d->bootipl)
		free(d->bootipl);
	if (d->bootmenu)
		free(d->bootmenu);
#else
#if !defined(__ia64__)
	if (d->bootmgr)
		free(d->bootmgr);
#endif
#endif
#if !defined(__ia64__)
	if (d->boot1)
		free(d->boot1);
#endif
#if defined(__i386__) || defined(__amd64__)
	if (d->boot2)
		free(d->boot2);
#endif
	free(d);
}

#if 0
void
Collapse_Disk(struct disk *d)
{

	while (Collapse_Chunk(d, d->chunks))
		;
}
#endif

static int
qstrcmp(const void* a, const void* b)
{
	const char *str1 = *(char* const*)a;
	const char *str2 = *(char* const*)b;

	return strcmp(str1, str2);
}

char **
Disk_Names()
{
	int disk_cnt;
	char **disks;
	int error;
	size_t listsize;
	char *disklist, *disk1, *disk2;

	error = sysctlbyname("kern.disks", NULL, &listsize, NULL, 0);
	if (error) {
		warn("kern.disks sysctl not available");
		return NULL;
	}

	if (listsize == 0)
		return (NULL);

	disks = malloc(sizeof *disks * (1 + MAX_NO_DISKS));
	if (disks == NULL)
		return NULL;
	disk1 = disklist = (char *)malloc(listsize + 1);
	if (disklist == NULL) {
		free(disks);
		return NULL;
	}
	memset(disks,0,sizeof *disks * (1 + MAX_NO_DISKS));
	memset(disklist, 0, listsize + 1);
	error = sysctlbyname("kern.disks", disklist, &listsize, NULL, 0);
	if (error || disklist[0] == 0) {
		free(disklist);
		free(disks);
		return NULL;
	}
	for (disk_cnt = 0; disk_cnt < MAX_NO_DISKS; disk_cnt++) {
		disk2 = strsep(&disk1, " ");
		if (disk2 == NULL)
			break;
		disks[disk_cnt] = strdup(disk2);
		if (disks[disk_cnt] == NULL) {
			for (disk_cnt--; disk_cnt >= 0; disk_cnt--)
				free(disks[disk_cnt]);
			free(disklist);
			free(disks);
			return (NULL);
		}
	}
	qsort(disks, disk_cnt, sizeof(char*), qstrcmp);
	free(disklist);
	return disks;
}

#ifdef PC98
void
Set_Boot_Mgr(struct disk *d, const u_char *bootipl, const size_t bootipl_size,
	const u_char *bootmenu, const size_t bootmenu_size)
#else
void
Set_Boot_Mgr(struct disk *d, const u_char *b, const size_t s)
#endif
{
#if !defined(__ia64__)
#ifdef PC98
	if (d->sector_size == 0)
		return;
	if (bootipl_size % d->sector_size != 0)
		return;
	if (d->bootipl)
		free(d->bootipl);
	if (!bootipl) {
		d->bootipl = NULL;
	} else {
		d->bootipl_size = bootipl_size;
		d->bootipl = malloc(bootipl_size);
		if (!d->bootipl)
			return;
		memcpy(d->bootipl, bootipl, bootipl_size);
	}

	if (bootmenu_size % d->sector_size != 0)
		return;
	if (d->bootmenu)
		free(d->bootmenu);
	if (!bootmenu) {
		d->bootmenu = NULL;
	} else {
		d->bootmenu_size = bootmenu_size;
		d->bootmenu = malloc(bootmenu_size);
		if (!d->bootmenu)
			return;
		memcpy(d->bootmenu, bootmenu, bootmenu_size);
	}
#else
	if (d->sector_size == 0)
		return;
	if (s % d->sector_size != 0)
		return;
	if (d->bootmgr)
		free(d->bootmgr);
	if (!b) {
		d->bootmgr = NULL;
	} else {
		d->bootmgr_size = s;
		d->bootmgr = malloc(s);
		if (!d->bootmgr)
			return;
		memcpy(d->bootmgr, b, s);
	}
#endif
#endif
}

int
Set_Boot_Blocks(struct disk *d, const u_char *b1, const u_char *b2)
{
#if defined(__i386__) || defined(__amd64__)
	if (d->boot1)
		free(d->boot1);
	d->boot1 = malloc(512);
	if (!d->boot1)
		return -1;
	memcpy(d->boot1, b1, 512);
	if (d->boot2)
		free(d->boot2);
	d->boot2 = malloc(15 * 512);
	if (!d->boot2)
		return -1;
	memcpy(d->boot2, b2, 15 * 512);
#elif defined(__alpha__)
	if (d->boot1)
		free(d->boot1);
	d->boot1 = malloc(15 * 512);
	if (!d->boot1)
		return -1;
	memcpy(d->boot1, b1, 15 * 512);
#elif defined(__sparc64__)
	if (d->boot1 != NULL)
		free(d->boot1);
	d->boot1 = malloc(16 * 512);
	if (d->boot1 == NULL)
		return (-1);
	memcpy(d->boot1, b1, 16 * 512);
#elif defined(__ia64__)
	/* nothing */
#else
/* Should be: #error "Set_Boot_Blocks: unknown arch"; */
#endif
	return 0;
}

const char *
slice_type_name( int type, int subtype )
{

	switch (type) {
	case whole:
		return "whole";
	case mbr:
		switch (subtype) {
		case 1:		return "fat (12-bit)";
		case 2:		return "XENIX /";
		case 3:		return "XENIX /usr";
		case 4:         return "fat (16-bit,<=32Mb)";
		case 5:		return "extended DOS";
		case 6:         return "fat (16-bit,>32Mb)";
		case 7:         return "NTFS/HPFS/QNX";
		case 8:         return "AIX bootable";
		case 9:         return "AIX data";
		case 10:	return "OS/2 bootmgr";
		case 11:        return "fat (32-bit)";
		case 12:        return "fat (32-bit,LBA)";
		case 14:        return "fat (16-bit,>32Mb,LBA)";
		case 15:        return "extended DOS, LBA";
		case 18:        return "Compaq Diagnostic";
		case 57:	return "Plan 9";
		case 77:	return "QNX 4.X";
		case 78:	return "QNX 4.X 2nd part";
		case 79:	return "QNX 4.X 3rd part";
		case 84:	return "OnTrack diskmgr";
		case 100:	return "Netware 2.x";
		case 101:	return "Netware 3.x";
		case 115:	return "SCO UnixWare";
		case 128:	return "Minix 1.1";
		case 129:	return "Minix 1.5";
		case 130:	return "linux_swap";
		case 131:	return "ext2fs";
		case 133:	return "linux extended";
		case 166:	return "OpenBSD FFS";	/* 0xA6 */
		case 168:	return "Mac OS-X";
		case 169:	return "NetBSD FFS";	/* 0xA9 */
		case 171:	return "Mac OS-X Boot";
		case 182:	return "OpenBSD";	/* dedicated */
		case 183:	return "bsd/os";
		case 184:	return "bsd/os swap";
		case 191:	return "Solaris (new)";
		case 238:	return "EFI GPT";
		case 239:	return "EFI Sys. Part.";
		default:	return "unknown";
		}
	case fat:
		return "fat";
	case freebsd:
		switch (subtype) {
#ifdef PC98
		case 0xc494:	return "freebsd";
#else
		case 165:	return "freebsd";
#endif
		default:	return "unknown";
		}
	case extended:
		return "extended";
	case part:
		return "part";
	case efi:
		return "efi";
	case unused:
		return "unused";
	default:
		return "unknown";
	}
}
