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
#include <sys/gpt.h>
#include <paths.h>
#include "libdisk.h"

#include <ctype.h>
#include <errno.h>
#include <assert.h>

#ifdef DEBUG
#define	DPRINT(x)	warn x
#define	DPRINTX(x)	warnx x
#else
#define	DPRINT(x)
#define	DPRINTX(x)
#endif

struct disk *
Int_Open_Disk(const char *name, char *conftxt)
{
	struct disk *d;
	int i;
	char *p, *q, *r, *a, *b, *n, *t, *sn;
	off_t o, len, off;
	u_int l, s, ty, sc, hd, alt;
	off_t lo[10];

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
	len = strtoimax(a, &r, 0);
	if (*r) {
		printf("BARF %d <%d>\n", __LINE__, *r);
		exit (0);
	}

	a = strsep(&p, " ");	/* sectorsize */
	s = strtoul(a, &r, 0);
	if (*r) {
		printf("BARF %d <%d>\n", __LINE__, *r);
		exit (0);
	}

	if (s == 0)
		return (NULL);
	d->sector_size = s;
	len /= s;	/* media size in number of sectors. */

	if (Add_Chunk(d, 0, len, name, whole, 0, 0, "-"))
		DPRINT(("Failed to add 'whole' chunk"));

	for (;;) {
		a = strsep(&p, " ");
		if (a == NULL)
			break;
		b = strsep(&p, " ");
		o = strtoul(b, &r, 0);
		if (*r) {
			printf("BARF %d <%d>\n", __LINE__, *r);
			exit (0);
		}
		if (!strcmp(a, "hd"))
			d->bios_hd = o;
		else if (!strcmp(a, "sc"))
			d->bios_sect = o;
		else
			printf("HUH ? <%s> <%s>\n", a, b);
	}

	/*
	 * Calculate the number of cylinders this disk must have. If we have
	 * an obvious insanity, we set the number of cyclinders to zero.
	 */
	o = d->bios_hd * d->bios_sect;
	d->bios_cyl = (o != 0) ? len / o : 0;

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
		if (*r) {
			printf("BARF %d <%d>\n", __LINE__, *r);
			exit (0);
		}
		t = strsep(&p, " ");	/* Type {SUN, BSD, MBR, PC98, GPT} */
		n = strsep(&p, " ");	/* name */
		a = strsep(&p, " ");	/* len */
		len = strtoimax(a, &r, 0);
		if (*r) {
			printf("BARF %d <%d>\n", __LINE__, *r);
			exit (0);
		}
		a = strsep(&p, " ");	/* secsize */
		s = strtoimax(a, &r, 0);
		if (*r) {
			printf("BARF %d <%d>\n", __LINE__, *r);
			exit (0);
		}
		for (;;) {
			a = strsep(&p, " ");
			if (a == NULL)
				break;
			/* XXX: Slice name may include a space. */
			if (!strcmp(a, "sn")) {
				sn = p;
				break;
			}
			b = strsep(&p, " ");
			o = strtoimax(b, &r, 0);
			if (*r) {
				printf("BARF %d <%d>\n", __LINE__, *r);
				exit (0);
			}
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

		/* PLATFORM POLICY BEGIN ----------------------------------- */
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
			if (Add_Chunk(d, 0, d->chunks->size, name, freebsd,
				      0, 0, "-"))
				DPRINT(("Failed to add 'freebsd' chunk"));
		}
		if (!strcmp(t, "BSD") && i == RAW_PART)
			continue;
		/* PLATFORM POLICY END ------------------------------------- */

		off /= s;
		len /= s;
		off += lo[l - 1];
		lo[l] = off;
		if (!strcmp(t, "SUN"))
			i = Add_Chunk(d, off, len, n, part, 0, 0, 0);
		else if (!strncmp(t, "MBR", 3)) {
			switch (ty) {
			case 0xa5:
				i = Add_Chunk(d, off, len, n, freebsd, ty, 0, 0);
				break;
			case 0x01:
			case 0x04:
			case 0x06:
			case 0x0b:
			case 0x0c:
			case 0x0e:
				i = Add_Chunk(d, off, len, n, fat, ty, 0, 0);
				break;
			case 0xef:	/* EFI */
				i = Add_Chunk(d, off, len, n, efi, ty, 0, 0);
				break;
			default:
				i = Add_Chunk(d, off, len, n, mbr, ty, 0, 0);
				break;
			}
		} else if (!strcmp(t, "BSD"))
			i = Add_Chunk(d, off, len, n, part, ty, 0, 0);
		else if (!strcmp(t, "PC98")) {
			switch (ty & 0x7f) {
			case 0x14:
				i = Add_Chunk(d, off, len, n, freebsd, ty, 0,
					      sn);
				break;
			case 0x20:
			case 0x21:
			case 0x22:
			case 0x23:
			case 0x24:
				i = Add_Chunk(d, off, len, n, fat, ty, 0, sn);
				break;
			default:
				i = Add_Chunk(d, off, len, n, pc98, ty, 0, sn);
				break;
			}
		} else if (!strcmp(t, "GPT"))
			i = Add_Chunk(d, off, len, n, ty, 0, 0, 0);
		else if (!strcmp(t, "BDE"))
			; /* nothing */
		else if (!strcmp(t, "CCD"))
			; /* nothing */
		else {
			printf("BARF %d\n", __LINE__);
			exit(0);
		}
	}
	/* PLATFORM POLICY BEGIN ------------------------------------- */
	/* We have a chance to do things on a blank disk here */
	if (platform == p_sparc64 && d->chunks->part->part == NULL) {
		hd = d->bios_hd;
		sc = d->bios_sect;
		o = d->chunks->size / (hd * sc);
		o *= (hd * sc);
		o -= 2 * hd * sc;
		if (Add_Chunk(d, 0, o, name, freebsd, 0, 0, "-"))
			DPRINT(("Failed to add 'freebsd' chunk"));
	}
	/* PLATFORM POLICY END --------------------------------------- */

	return (d);
	i = 0;
}
