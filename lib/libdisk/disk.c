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
#include <sys/diskslice.h>
#include <sys/diskmbr.h>
#include <paths.h>
#include "libdisk.h"

#include <ctype.h>
#include <errno.h>
#include <assert.h>

#define DOSPTYP_EXTENDED        5
#ifdef DEBUG
#define	DPRINT(x)	warn x
#define	DPRINTX(x)	warnx x
#else
#define	DPRINT(x)
#define	DPRINTX(x)
#endif

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
	default:	return ("??");
	}
};

struct disk *
Open_Disk(const char *name)
{
	return Int_Open_Disk(name);
}

struct disk *
Int_Open_Disk(const char *name)
{
	char *conftxt = NULL;
	struct disk *d;
	size_t txtsize;
	int error, i;
	char *p, *q, *r, *a, *b, *n, *t;
	off_t o, len, off;
	u_int l, s, ty, sc, hd, alt;
	off_t lo[10];

	error = sysctlbyname("kern.geom.conftxt", NULL, &txtsize, NULL, 0);
	if (error) {
		warn("kern.geom.conftxt sysctl not available, giving up!");
		return (NULL);
	}
	conftxt = (char *) malloc(txtsize+1);
	if (conftxt == NULL) {
		DPRINT(("cannot malloc memory for conftxt"));
		return (NULL);
	}
	error = sysctlbyname("kern.geom.conftxt", conftxt, &txtsize, NULL, 0);
	if (error) {
		DPRINT(("error reading kern.geom.conftxt from the system"));
		free(conftxt);
		return (NULL);
	}
	conftxt[txtsize] = '\0';	/* in case kernel bug is still there */

	for (p = conftxt; p != NULL && *p; p = strchr(p, '\n')) {
		if (*p == '\n')
			p++;
		a = strsep(&p, " ");
		if (strcmp(a, "0"))
			continue;

		a = strsep(&p, " ");
		if (strcmp(a, "DISK"))
			continue;

		a = strsep(&p, " ");
		if (strcmp(a, name))
			continue;
		break;
	}

	q = strchr(p, '\n');
	if (q != NULL)
		*q++ = '\0';

	d = (struct disk *)calloc(sizeof *d, 1);
	if(d == NULL)
		return NULL;

	d->name = strdup(name);

	a = strsep(&p, " ");	/* length in bytes */
	o = strtoimax(a, &r, 0);
	if (*r) { printf("BARF %d <%d>\n", __LINE__, *r); exit (0); }

	a = strsep(&p, " ");	/* sectorsize */
	s = strtoul(a, &r, 0);
	if (*r) { printf("BARF %d <%d>\n", __LINE__, *r); exit (0); }

	if (Add_Chunk(d, 0, o / s, name, whole, 0, 0, "-"))
		DPRINT(("Failed to add 'whole' chunk"));

	len = o / s;

	for (;;) {
		a = strsep(&p, " ");
		if (a == NULL)
			break;
		b = strsep(&p, " ");
		o = strtoul(b, &r, 0);
		if (*r) { printf("BARF %d <%d>\n", __LINE__, *r); exit (0); }
		if (!strcmp(a, "hd"))
			d->bios_hd = o;
		else if (!strcmp(a, "sc"))
			d->bios_sect = o;
		else
			printf("HUH ? <%s> <%s>\n", a, b);
	}

	p = q;
	lo[0] = 0;

	for (; p != NULL && *p; p = q) {
		q = strchr(p, '\n');
		if (q != NULL)
			*q++ = '\0';
		a = strsep(&p, " ");	/* Index */
		if (!strcmp(a, "0"))
			break;
		l = strtoimax(a, &r, 0);
		if (*r) { printf("BARF %d <%d>\n", __LINE__, *r); exit (0); }
		t = strsep(&p, " ");	/* Type {SUN, BSD, MBR, GPT} */
		n = strsep(&p, " ");	/* name */
		a = strsep(&p, " ");	/* len */
		len = strtoimax(a, &r, 0);
		if (*r) { printf("BARF %d <%d>\n", __LINE__, *r); exit (0); }
		a = strsep(&p, " ");	/* secsize */
		s = strtoimax(a, &r, 0);
		if (*r) { printf("BARF %d <%d>\n", __LINE__, *r); exit (0); }
		for (;;) {
			a = strsep(&p, " ");
			if (a == NULL)
				break;
			b = strsep(&p, " ");
			o = strtoimax(b, &r, 0);
			if (*r) { printf("BARF %d <%d>\n", __LINE__, *r); exit (0); }
			if (!strcmp(a, "o"))
				off = o;
			else if (!strcmp(a, "i"))
				i = o;
			else if (!strcmp(a, "ty"))
				ty = o;
			else if (!strcmp(a, "sc"))
				sc = o;
			else if (!strcmp(a, "hd"))
				hd = o;
			else if (!strcmp(a, "alt"))
				alt = o;
		}

		/* PLATFORM POLICY BEGIN ------------------------------------- */
		if (platform == p_sparc64 && !strcmp(t, "SUN") && i == 2)
			continue;
		if (platform == p_sparc64 && !strcmp(t, "SUN") &&
		    d->chunks->part->part == NULL) {
			d->bios_hd = hd;
			d->bios_sect = sc;
			o = d->chunks->size / (hd * sc);
			o *= (hd * sc);
			o -= alt * hd * sc;
			if (Add_Chunk(d, 0, o, name, freebsd, 0, 0, "-"))
				DPRINT(("Failed to add 'freebsd' chunk"));
		}
		if (platform == p_alpha && !strcmp(t, "BSD") &&
		    d->chunks->part->part == NULL) {
			o = d->chunks->size;
			if (Add_Chunk(d, 0, d->chunks->size, name, freebsd, 0, 0, "-"))
				DPRINT(("Failed to add 'freebsd' chunk"));
		}
		if (!strcmp(t, "BSD") && i == RAW_PART)
			continue;
		/* PLATFORM POLICY END --------------------------------------- */

		off /= s;
		len /= s;
		off += lo[l - 1];
		lo[l] = off;
		printf("%s [%s] %jd %jd\n", t, n, (intmax_t)(off / s), (intmax_t) (len / s));
		if (!strcmp(t, "SUN"))
			i = Add_Chunk(d, off, len, n, part, 0, 0, 0);
		else if (!strcmp(t, "MBR") && ty == 165)
			i = Add_Chunk(d, off, len, n, freebsd, 0, 0, 0);
		else if (!strcmp(t, "MBR"))
			i = Add_Chunk(d, off, len, n, mbr, 0, 0, 0);
		else if (!strcmp(t, "BSD"))
			i = Add_Chunk(d, off, len, n, part, 0, 0, 0);
		else if (!strcmp(t, "PC98"))
			i = Add_Chunk(d, off, len, n, pc98, 0, 0, 0);
		else if (!strcmp(t, "GPT"))
			i = Add_Chunk(d, off, len, n, gpt, 0, 0, 0);
		else
			{printf("BARF %d\n", __LINE__); exit(0); }
		printf("error = %d\n", i);
	}
	/* PLATFORM POLICY BEGIN ------------------------------------- */
	/* We have a chance to do things on a blank disk here */
printf("c %p\n", d->chunks);
printf("c->p %p\n", d->chunks->part);
printf("c->p->p %p\n", d->chunks->part->part);
	if (platform == p_sparc64 && d->chunks->part->part == NULL) {
printf("HERE %d\n", __LINE__);
		hd = d->bios_hd;
		sc = d->bios_sect;
		o = d->chunks->size / (hd * sc);
		o *= (hd * sc);
		o -= 2 * hd * sc;
printf("HERE %d\n", __LINE__);
		if (Add_Chunk(d, 0, o, name, freebsd, 0, 0, "-"))
			DPRINT(("Failed to add 'freebsd' chunk"));
	}
	/* PLATFORM POLICY END --------------------------------------- */

	return (d);
	i = 0;
}

void
Debug_Disk(struct disk *d)
{
	printf("Debug_Disk(%s)", d->name);
#if 0
	printf("  real_geom=%lu/%lu/%lu", d->real_cyl, d->real_hd, d->real_sect);
#endif
	printf("  bios_geom=%lu/%lu/%lu = %lu\n",
		d->bios_cyl, d->bios_hd, d->bios_sect,
		d->bios_cyl * d->bios_hd * d->bios_sect);
#if   defined(__i386__)
	printf("  boot1=%p, boot2=%p, bootmgr=%p\n",
		d->boot1, d->boot2, d->bootmgr);
#elif defined(__alpha__)
	printf("  boot1=%p, bootmgr=%p\n",
		d->boot1, d->bootmgr);
#elif defined(__ia64__)
	printf("\n");
#else
/* Should be: error "Debug_Disk: unknown arch"; */
#endif
	Debug_Chunk(d->chunks);
}

void
Free_Disk(struct disk *d)
{
	if(d->chunks) Free_Chunk(d->chunks);
	if(d->name) free(d->name);
#ifdef PC98
	if(d->bootipl) free(d->bootipl);
	if(d->bootmenu) free(d->bootmenu);
#else
#if !defined(__ia64__)
	if(d->bootmgr) free(d->bootmgr);
#endif
#endif
#if !defined(__ia64__)
	if(d->boot1) free(d->boot1);
#endif
#if defined(__i386__)
	if(d->boot2) free(d->boot2);
#endif
	free(d);
}

#if 0
void
Collapse_Disk(struct disk *d)
{

	while(Collapse_Chunk(d, d->chunks))
		;
}
#endif

static int
qstrcmp(const void* a, const void* b)
{

	char *str1 = *(char**)a;
	char *str2 = *(char**)b;
	return strcmp(str1, str2);
}

char **
Disk_Names()
{
	int disk_cnt;
	static char **disks;
	int error;
	size_t listsize;
	char *disklist;

	error = sysctlbyname("kern.disks", NULL, &listsize, NULL, 0);
	if (error) {
		warn("kern.disks sysctl not available");
		return NULL;
	}

	disks = malloc(sizeof *disks * (1 + MAX_NO_DISKS));
	if (disks == NULL)
		return NULL;
	disklist = (char *)malloc(listsize + 1);
	if (disklist == NULL) {
		free(disks);
		return NULL;
	}
	memset(disks,0,sizeof *disks * (1 + MAX_NO_DISKS));
	memset(disklist, 0, listsize + 1);
	error = sysctlbyname("kern.disks", disklist, &listsize, NULL, 0);
	if (error) {
		free(disklist);
		free(disks);
		return NULL;
	}
	for (disk_cnt = 0; disk_cnt < MAX_NO_DISKS; disk_cnt++) {
		disks[disk_cnt] = strsep(&disklist, " ");
		if (disks[disk_cnt] == NULL)
			break;
											}
	qsort(disks, disk_cnt, sizeof(char*), qstrcmp);
	return disks;
}

void
Set_Boot_Mgr(struct disk *d, const u_char *b, const size_t s)
{
#if !defined(__ia64__)
#ifdef PC98
	if (bootipl_size % d->sector_size != 0)
		return;
	if (d->bootipl)
		free(d->bootipl);
	if (!bootipl) {
		d->bootipl = NULL;
	} else {
		d->bootipl_size = bootipl_size;
		d->bootipl = malloc(bootipl_size);
		if(!d->bootipl) return;
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
		if(!d->bootmenu) return;
		memcpy(d->bootmenu, bootmenu, bootmenu_size);
	}
#else
	if (s % d->sector_size != 0)
		return;
	if (d->bootmgr)
		free(d->bootmgr);
	if (!b) {
		d->bootmgr = NULL;
	} else {
		d->bootmgr_size = s;
		d->bootmgr = malloc(s);
		if(!d->bootmgr) return;
		memcpy(d->bootmgr, b, s);
	}
#endif
#endif
}

int
Set_Boot_Blocks(struct disk *d, const u_char *b1, const u_char *b2)
{
#if defined(__i386__)
	if (d->boot1) free(d->boot1);
	d->boot1 = malloc(512);
	if(!d->boot1) return -1;
	memcpy(d->boot1, b1, 512);
	if (d->boot2) free(d->boot2);
	d->boot2 = malloc(15 * 512);
	if(!d->boot2) return -1;
	memcpy(d->boot2, b2, 15 * 512);
#elif defined(__alpha__)
	if (d->boot1) free(d->boot1);
	d->boot1 = malloc(15 * 512);
	if(!d->boot1) return -1;
	memcpy(d->boot1, b1, 15 * 512);
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
		case 0:		return "whole";
		case 1:		switch (subtype) {
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
					case 84:	return "OnTrack diskmgr";
					case 100:	return "Netware 2.x";
					case 101:	return "Netware 3.x";
					case 115:	return "SCO UnixWare";
					case 128:	return "Minix 1.1";
					case 129:	return "Minix 1.5";
					case 130:	return "linux_swap";
					case 131:	return "ext2fs";
					case 166:	return "OpenBSD FFS";	/* 0xA6 */
					case 169:	return "NetBSD FFS";	/* 0xA9 */
					case 182:	return "OpenBSD";		/* dedicated */
					case 183:	return "bsd/os";
					case 184:	return "bsd/os swap";
					case 238:	return "EFI GPT";
					case 239:	return "EFI Sys. Part.";
					default:	return "unknown";
				}
		case 2:		return "fat";
		case 3:		switch (subtype) {
					case 165:	return "freebsd";
					default:	return "unknown";
				}
		case 4:		return "extended";
		case 5:		return "part";
		case 6:		return "unused";
		default:	return "unknown";
	}
}
