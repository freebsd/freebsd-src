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
#include <string.h>
#include <sys/types.h>
#include <sys/disklabel.h>
#ifdef PC98
#include <sys/diskpc98.h>
#else
#include <sys/diskmbr.h>
#endif
#include "libdisk.h"

int
Track_Aligned(const struct disk *d, u_long offset)
{
#ifndef __ia64__
	if (!d->bios_sect)
		return 1;
	if (offset % d->bios_sect)
		return 0;
#endif /* __ia64__ */
	return 1;
}

u_long
Prev_Track_Aligned(const struct disk *d, u_long offset)
{
#ifndef __ia64__
	if (!d->bios_sect)
		return offset;
	return (offset / d->bios_sect) * d->bios_sect;
#else
	return 1;
#endif
}

u_long
Next_Track_Aligned(const struct disk *d, u_long offset)
{
#ifndef __ia64__
	if (!d->bios_sect)
		return offset;
	return Prev_Track_Aligned(d, offset + d->bios_sect-1);
#else
	return 1;
#endif
}

static int
Cyl_Aligned(const struct disk *d, u_long offset)
{
#ifndef __ia64__
	if (!d->bios_sect || !d->bios_hd)
		return 1;
	if (offset % (d->bios_sect * d->bios_hd))
		return 0;
#endif
	return 1;
}

u_long
Prev_Cyl_Aligned(const struct disk *d, u_long offset)
{
#ifndef __ia64__
	if (!d->bios_sect || !d->bios_hd)
		return offset;
	return (offset / (d->bios_sect * d->bios_hd)) * d->bios_sect *
	    d->bios_hd;
#else
	return 1;
#endif
}

u_long
Next_Cyl_Aligned(const struct disk *d, u_long offset)
{
#ifndef __ia64__
	if (!d->bios_sect || !d->bios_hd)
		return offset;
	return Prev_Cyl_Aligned(d,offset + (d->bios_sect * d->bios_hd) - 1);
#else
	return 1;
#endif
}

/*
 *  Rule#0:
 *	Chunks of type 'whole' can have max NDOSPART children.
 *	Only one of them can have the "active" flag
 */
static void
Rule_000(const struct disk *d, const struct chunk *c, char *msg)
{
#ifdef PC98
	int i = 0;
#else
	int i = 0, j = 0;
#endif
	struct chunk *c1;

	if (c->type != whole)
		return;
	for (c1 = c->part; c1; c1 = c1->next) {
		if (c1->type != unused)
			continue;
#ifndef PC98
		if (c1->flags & CHUNK_ACTIVE)
			j++;
#endif
		i++;
	}
	if (i > NDOSPART)
		sprintf(msg + strlen(msg),
			"%d is too many children of the 'whole' chunk."
			"  Max is %d\n", i, NDOSPART);
#ifndef PC98
	if (j > 1)
		sprintf(msg + strlen(msg),
			"Too many active children of 'whole'");
#endif
}

/*
 * Rule#1:
 *	All children of 'whole' and 'extended'  must be track-aligned.
 *	Exception: the end can be unaligned if it matches the end of 'whole'
 */
static void
Rule_001(const struct disk *d, const struct chunk *c, char *msg)
{
	struct chunk *c1;

	if (c->type != whole && c->type != extended)
		return;
	for (c1 = c->part; c1; c1 = c1->next) {
		if (c1->type == unused)
			continue;
		c1->flags |= CHUNK_ALIGN;
#ifdef PC98
		if (!Cyl_Aligned(d, c1->offset))
#else
		if (!Track_Aligned(d, c1->offset))
#endif
			sprintf(msg + strlen(msg),
#ifdef PC98
				"chunk '%s' [%ld..%ld] does not start"
				" on a cylinder boundary\n",
#else
				"chunk '%s' [%ld..%ld] does not start"
				" on a track boundary\n",
#endif
				c1->name, c1->offset, c1->end);
		if ((c->type == whole || c->end == c1->end)
		    || Cyl_Aligned(d, c1->end + 1))
			;
		else
			sprintf(msg + strlen(msg),
				"chunk '%s' [%ld..%ld] does not end"
				" on a cylinder boundary\n",
				c1->name, c1->offset, c1->end);
	}
}

/*
 * Rule#2:
 *	Max one 'fat' as child of 'whole'
 */
static void
Rule_002(const struct disk *d, const struct chunk *c, char *msg)
{
	int i;
	struct chunk *c1;

	if (c->type != whole)
		return;
	for (i = 0, c1 = c->part; c1; c1 = c1->next) {
		if (c1->type != fat)
			continue;
		i++;
	}
	if (i > 1) {
		sprintf(msg + strlen(msg),
			"Max one 'fat' allowed as child of 'whole'\n");
	}
}

/*
 * Rule#3:
 *	Max one extended as child of 'whole'
 */
static void
Rule_003(const struct disk *d, const struct chunk *c, char *msg)
{
	int i;
	struct chunk *c1;

	if (c->type != whole)
		return;
	for (i = 0, c1 = c->part; c1; c1 = c1->next) {
		if (c1->type != extended)
			continue;
		i++;
	}
	if (i > 1) {
		sprintf(msg + strlen(msg),
			"Max one 'extended' allowed as child of 'whole'\n");
	}
}

/*
 * Rule#4:
 *	Max seven 'part' as children of 'freebsd'
 *	Max one CHUNK_IS_ROOT child per 'freebsd'
 */
static void
Rule_004(const struct disk *d, const struct chunk *c, char *msg)
{
	int i = 0, k = 0;
	struct chunk *c1;

	if (c->type != freebsd)
		return;

	for (c1 = c->part; c1; c1 = c1->next) {
		if (c1->type != part)
			continue;
		if (c1->flags & CHUNK_IS_ROOT)
			k++;
		i++;
	}
	if (i > 7) {
		sprintf(msg + strlen(msg),
			"Max seven partitions per freebsd slice\n");
	}
	if (k > 1) {
		sprintf(msg + strlen(msg),
			"Max one root partition child per freebsd slice\n");
	}
}

static void
Check_Chunk(const struct disk *d, const struct chunk *c, char *msg)
{

	switch (platform) {
	case p_i386:
	case p_amd64:
		Rule_000(d, c, msg);
		Rule_001(d, c, msg);
		Rule_002(d, c, msg);
		Rule_003(d, c, msg);
		Rule_004(d, c, msg);
		if (c->part)
			Check_Chunk(d, c->part, msg);
		if (c->next)
			Check_Chunk(d, c->next, msg);
		break;
	case p_pc98:
		Rule_000(d, c, msg);
		Rule_001(d, c, msg);
		Rule_004(d, c, msg);
		if (c->part)
			Check_Chunk(d, c->part, msg);
		if (c->next)
			Check_Chunk(d, c->next, msg);
		break;
	default:
		break;
	}
}

char *
CheckRules(const struct disk *d)
{
	char msg[BUFSIZ];

	*msg = '\0';
	Check_Chunk(d, d->chunks, msg);
	if (*msg)
		return strdup(msg);
	return 0;
}
