/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/diskslice.h>
#include <sys/disklabel.h>
#include <err.h>
#include "libdisk.h"

int
Track_Aligned(struct disk *d, u_long offset)
{
	if (!d->bios_sect)
		return 1;
	if (offset % d->bios_sect)
		return 0;
	return 1;
}

u_long
Prev_Track_Aligned(struct disk *d, u_long offset)
{
	if (!d->bios_sect)
		return offset;
	return (offset / d->bios_sect) * d->bios_sect;
}

u_long
Next_Track_Aligned(struct disk *d, u_long offset)
{
	if (!d->bios_sect)
		return offset;
	return Prev_Track_Aligned(d,offset + d->bios_sect-1);
}

int
Cyl_Aligned(struct disk *d, u_long offset)
{
	if (!d->bios_sect || !d->bios_hd)
		return 1;
	if (offset % (d->bios_sect * d->bios_hd))
		return 0;
	return 1;
}

u_long
Prev_Cyl_Aligned(struct disk *d, u_long offset)
{
	if (!d->bios_sect || !d->bios_hd)
		return offset;
	return (offset / (d->bios_sect*d->bios_hd)) * d->bios_sect * d->bios_hd;
}

u_long
Next_Cyl_Aligned(struct disk *d, u_long offset)
{
	if (!d->bios_sect || !d->bios_hd)
		return offset;
	return Prev_Cyl_Aligned(d,offset + (d->bios_sect * d->bios_hd)-1);
}

/*
 *  Rule#0:
 *	Chunks of type 'whole' can have max NDOSPART children.
 *	Only one of them can have the "active" flag
 */
void
Rule_000(struct disk *d, struct chunk *c, char *msg)
{
	int i=0,j=0;
	struct chunk *c1;

	if (c->type != whole)
		return;
	for (c1=c->part; c1; c1=c1->next) {
		if (c1->type != unused) continue;
		if (c1->flags & CHUNK_ACTIVE)
			j++;
		i++;
	}
	if (i > NDOSPART)
		sprintf(msg+strlen(msg),
	"%d is too many children of the 'whole' chunk.  Max is %d\n",
			i, NDOSPART);
	if (j > 1)
		sprintf(msg+strlen(msg),
	"Too many active children of 'whole'");
}

/*
 * Rule#1:
 *	All children of 'whole' and 'extended'  must be track-aligned.
 *	Exception: the end can be unaligned if it matches the end of 'whole'
 */
void
Rule_001(struct disk *d, struct chunk *c, char *msg)
{
	int i;
	struct chunk *c1;

	if (c->type != whole && c->type != extended)
		return;
	for (i=0, c1=c->part; c1; c1=c1->next) {
		if (c1->type == unused) continue;
		c1->flags |= CHUNK_ALIGN;
		if (!Track_Aligned(d,c1->offset))
			sprintf(msg+strlen(msg),
		    "chunk '%s' [%ld..%ld] does not start on a track boundary\n",
				c1->name,c1->offset,c1->end);
		if ((c->type == whole || c->end == c1->end)
		    || Cyl_Aligned(d,c1->end+1))
			;
		else
			sprintf(msg+strlen(msg),
		    "chunk '%s' [%ld..%ld] does not end on a cylinder boundary\n",
				c1->name,c1->offset,c1->end);
	}
}

/*
 * Rule#2:
 *	Max one 'fat' as child of 'whole'
 */
void
Rule_002(struct disk *d, struct chunk *c, char *msg)
{
	int i;
	struct chunk *c1;

	if (c->type != whole)
		return;
	for (i=0, c1=c->part; c1; c1=c1->next) {
		if (c1->type != fat)
			continue;
		i++;
	}
	if (i > 1) {
		sprintf(msg+strlen(msg),
		    "Max one 'fat' allowed as child of 'whole'\n");
	}
}

/*
 * Rule#3:
 *	Max one extended as child of 'whole'
 */
void
Rule_003(struct disk *d, struct chunk *c, char *msg)
{
	int i;
	struct chunk *c1;

	if (c->type != whole)
		return;
	for (i=0, c1=c->part; c1; c1=c1->next) {
		if (c1->type != extended)
			continue;
		i++;
	}
	if (i > 1) {
		sprintf(msg+strlen(msg),
		    "Max one 'extended' allowed as child of 'whole'\n");
	}
}

/*
 * Rule#4:
 *	Max seven 'part' as children of 'freebsd'
 *	Max one CHUNK_IS_ROOT child per 'freebsd'
 *	If Bad144, space for table must exist.
 *	If Bad144 & root, bad144 table must be inside 1024
 */
void
Rule_004(struct disk *d, struct chunk *c, char *msg)
{
	int i=0,k=0;
	struct chunk *c1;
	u_long l;

	if (c->type != freebsd)
		return;

	if (c->flags & CHUNK_BAD144) {
		l = c->end - 127 - d->bios_sect + 1;
		for (c1=c->part; c1; c1=c1->next) {
			if (c1->end < l || c1->type == unused)
				continue;
			sprintf(msg+strlen(msg),
    "Blocks %lu to %lu are needed for bad144 information, but isn't unused.\n",
	l, c->end);
			break;
		}
		if (c->flags & CHUNK_PAST_1024) {
			for (c1=c->part; c1; c1=c1->next) {
				if (c1->flags & CHUNK_IS_ROOT) {
					sprintf(msg+strlen(msg),
    "You have assigned root to a slice which uses bad144, and\n  extends past the first 1023 cylinders, and thus cannot be booted from.\n");
					break;
				}
			}
		}
	}

	for (c1=c->part; c1; c1=c1->next) {
		if (c1->type != part)
			continue;
		if (c1->flags & CHUNK_IS_ROOT) {
			k++;
			if (c1->flags & CHUNK_PAST_1024)
				sprintf(msg+strlen(msg),
	    "Root filesystem extends past cylinder 1024, and cannot be booted from\n");
		}
		i++;
	}
	if (i > 7) {
		sprintf(msg+strlen(msg),
		    "Max seven partitions per freebsd slice\n");
	}
	if (k > 1) {
		sprintf(msg+strlen(msg),
		    "Max one root partition child per freebsd slice\n");
	}
}

void
Check_Chunk(struct disk *d, struct chunk *c, char *msg)
{
	Rule_000(d,c,msg);
	Rule_001(d,c,msg);
	Rule_002(d,c,msg);
	Rule_003(d,c,msg);
	Rule_004(d,c,msg);
	if (c->part)
		Check_Chunk(d,c->part,msg);
	if (c->next)
		Check_Chunk(d,c->next,msg);

	if (c->end >= 1024*d->bios_hd*d->bios_sect)
		c->flags |= CHUNK_PAST_1024;
	else
		c->flags &= ~CHUNK_PAST_1024;
}

char *
CheckRules(struct disk *d)
{
	char msg[BUFSIZ];

	*msg = '\0';
	Check_Chunk(d,d->chunks,msg);
	if (*msg)
		return strdup(msg);
	return 0;
}

char *
ChunkCanBeRoot(struct chunk *c)
{
	struct chunk *c1;
	struct disk *d = c->disk;
	char msg[BUFSIZ];

	*msg = '\0';
	if (c->flags & CHUNK_PAST_1024) {
		strcat(msg,
"The root partition must end before cylinder 1024 seen from\n");
		strcat(msg,
"the BIOS' point of view, or it cannot be booted from.\n");
		return strdup(msg);
	}
	for (c1=d->chunks->part;;) {
		for (; c1; c1=c1->next)
			if (c1->offset <= c->offset && c1->end >= c->end)
				break;
		if (!c1) {
			strcat(msg,
"Internal trouble, cannot find this chunk in the chunk-tree\n");
			return strdup(msg);
		}
		if (c1->type == freebsd)
			break;
		c1 = c1->part;
	}

	if (c1->type != freebsd) {
		strcat(msg,
"The root partition must be in a FreeBSD slice, otherwise\n");
		strcat(msg,
"the kernel cannot be booted from it\n");
		return strdup(msg);
	}

	if ((c1->flags & CHUNK_BAD144) && (c1->flags & CHUNK_PAST_1024)) {
		strcat(msg,
"This partition is unsuitable for root, because the FreeBSD slice\n");
		strcat(msg,
"it is inside has bad144 enabled, but the badblock data lives past\n");
		strcat(msg,
"the 1024th cylinder, and the bootblocks cannot get to it there.\n");
		return strdup(msg);
	}
	return NULL;
}
