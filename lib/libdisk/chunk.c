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
#include <err.h>
#include "libdisk.h"

static struct chunk *
New_Chunk(void)
{
	struct chunk *c;

	c = malloc(sizeof *c);
	if (c != NULL)
		memset(c, 0, sizeof *c);
	return (c);
}

/* Is c2 completely inside c1 ? */

static int
Chunk_Inside(const struct chunk *c1, const struct chunk *c2)
{
	/* if c1 ends before c2 do */
	if (c1->end < c2->end)
		return 0;
	/* if c1 starts after c2 do */
	if (c1->offset > c2->offset)
		return 0;
	return 1;
}

static struct chunk *
Find_Mother_Chunk(struct chunk *chunks, u_long offset, u_long end,
		  chunk_e type)
{
	struct chunk *c1, *c2, ct;

	ct.offset = offset;
	ct.end = end;
	switch (type) {
	case whole:
		if (Chunk_Inside(chunks, &ct))
			return chunks;
	case extended:
		for (c1 = chunks->part; c1; c1 = c1->next) {
			if (c1->type != type)
				continue;
			if (Chunk_Inside(c1, &ct))
				return c1;
		}
		return 0;
	case freebsd:
		for (c1 = chunks->part; c1; c1 = c1->next) {
			if (c1->type == type)
				if (Chunk_Inside(c1, &ct))
					return c1;
			if (c1->type != extended)
				continue;
			for (c2 = c1->part; c2; c2 = c2->next)
				if (c2->type == type && Chunk_Inside(c2, &ct))
					return c2;
		}
		return 0;
	default:
		warn("Unsupported mother type in Find_Mother_Chunk");
		return 0;
	}
}

void
Free_Chunk(struct chunk *c1)
{
	if(c1 == NULL)
		return;
	if(c1->private_data && c1->private_free)
		(*c1->private_free)(c1->private_data);
	if(c1->part != NULL)
		Free_Chunk(c1->part);
	if(c1->next != NULL)
		Free_Chunk(c1->next);
	if (c1->name != NULL)
		free(c1->name);
	if (c1->sname != NULL)
		free(c1->sname);
	free(c1);
}

struct chunk *
Clone_Chunk(const struct chunk *c1)
{
	struct chunk *c2;

	if(!c1)
		return NULL;
	c2 = New_Chunk();
	if (c2 == NULL)
		return NULL;
	*c2 = *c1;
	if (c1->private_data && c1->private_clone)
		c2->private_data = c2->private_clone(c2->private_data);
	c2->name = strdup(c2->name);
	if (c2->sname != NULL)
		c2->sname = strdup(c2->sname);
	c2->next = Clone_Chunk(c2->next);
	c2->part = Clone_Chunk(c2->part);
	return c2;
}

int
Insert_Chunk(struct chunk *c2, u_long offset, u_long size, const char *name,
	chunk_e type, int subtype, u_long flags, const char *sname)
{
	struct chunk *ct,*cs;

	/* We will only insert into empty spaces */
	if (c2->type != unused)
		return __LINE__;

	ct = New_Chunk();
	if (ct == NULL)
		return __LINE__;
	ct->disk = c2->disk;
	ct->offset = offset;
	ct->size = size;
	ct->end = offset + size - 1;
	ct->type = type;
	if (sname != NULL)
		ct->sname = strdup(sname);
	ct->name = strdup(name);
	ct->subtype = subtype;
	ct->flags = flags;

	if (!Chunk_Inside(c2, ct)) {
		Free_Chunk(ct);
		return __LINE__;
	}

	if (type == freebsd || type == extended) {
		cs = New_Chunk();
		if (cs == NULL)
			return __LINE__;
		cs->disk = c2->disk;
		cs->offset = offset;
		cs->size = size;
		cs->end = offset + size - 1;
		cs->type = unused;
		if (sname != NULL)
			cs->sname = strdup(sname);
		cs->name = strdup("-");
		ct->part = cs;
	}

	/* Make a new chunk for any trailing unused space */
	if (c2->end > ct->end) {
		cs = New_Chunk();
		if (cs == NULL)
			 return __LINE__;
		*cs = *c2;
		cs->disk = c2->disk;
		cs->offset = ct->end + 1;
		cs->size = c2->end - ct->end;
		if (c2->sname != NULL)
			cs->sname = strdup(c2->sname);
		if (c2->name)
			cs->name = strdup(c2->name);
		c2->next = cs;
		c2->size -= c2->end - ct->end;
		c2->end = ct->end;
	}
	/* If no leading unused space just occupy the old chunk */
	if (c2->offset == ct->offset) {
		c2->sname = ct->sname;
		c2->name = ct->name;
		c2->type = ct->type;
		c2->part = ct->part;
		c2->subtype = ct->subtype;
		c2->flags = ct->flags;
		ct->sname = NULL;
		ct->name = NULL;
		ct->part = 0;
		Free_Chunk(ct);
		return 0;
	}
	/* else insert new chunk and adjust old one */
	c2->end = ct->offset - 1;
	c2->size -= ct->size;
	ct->next = c2->next;
	c2->next = ct;
	return 0;
}

int
Add_Chunk(struct disk *d, long offset, u_long size, const char *name,
	  chunk_e type, int subtype, u_long flags, const char *sname)
{
	struct chunk *c1, *c2, ct;
	u_long end = offset + size - 1;
	ct.offset = offset;
	ct.end = end;
	ct.size = size;

	if (type == whole) {
		d->chunks = c1 = New_Chunk();
		if (c1 == NULL)
			return __LINE__;
		c2 = c1->part = New_Chunk();
		if (c2 == NULL)
			 return __LINE__;
		c2->disk = c1->disk = d;
		c2->offset = c1->offset = offset;
		c2->size = c1->size = size;
		c2->end = c1->end = end;
		c1->sname = strdup(sname);
		c2->sname = strdup("-");
		c1->name = strdup(name);
		c2->name = strdup("-");
		c1->type = type;
		c2->type = unused;
		c1->flags = flags;
		c1->subtype = subtype;
		return 0;
	}

	c1 = 0;
	/* PLATFORM POLICY BEGIN ------------------------------------- */
	switch(platform) {
	case p_i386:
		switch (type) {
		case fat:
		case mbr:
		case extended:
		case freebsd:
			c1 = Find_Mother_Chunk(d->chunks, offset, end, whole);
			break;
		case part:
			c1 = Find_Mother_Chunk(d->chunks, offset, end, freebsd);
			break;
		default:
			return(-1);
		}
		break;
	case p_ia64:
		switch (type) {
		case freebsd:
			subtype = 0xa5;
			/* FALL THROUGH */
		case fat:
		case efi:
		case mbr:
			c1 = Find_Mother_Chunk(d->chunks, offset, end, whole);
			break;
		case part:
			c1 = Find_Mother_Chunk(d->chunks, offset, end,
			    freebsd);
			if (!c1)
				c1 = Find_Mother_Chunk(d->chunks, offset, end,
				    whole);
			break;
		default:
			return (-1);
		}
		break;
	case p_pc98:
		switch (type) {
		case fat:
		case pc98:
		case freebsd:
			c1 = Find_Mother_Chunk(d->chunks, offset, end, whole);
			break;
		case part:
			c1 = Find_Mother_Chunk(d->chunks, offset, end, freebsd);
			break;
		default:
			return(-1);
		}
		break;
	case p_sparc64:
	case p_alpha:
		switch (type) {
		case freebsd:
			c1 = Find_Mother_Chunk(d->chunks, offset, end, whole);
			break;
		case part:
			c1 = Find_Mother_Chunk(d->chunks, offset, end, freebsd);
			break;
		default:
			return(-1);
		}
		break;
	default:
		return (-1);
	}
	/* PLATFORM POLICY END ---------------------------------------- */

	if(!c1)
		return __LINE__;
	for(c2 = c1->part; c2; c2 = c2->next) {
		if (c2->type != unused)
			continue;
		if(!Chunk_Inside(c2, &ct))
			continue;
/* PLATFORM POLICY BEGIN ------------------------------------- */
		if (platform == p_sparc64) {
			offset = Prev_Cyl_Aligned(d, offset);
			size = Next_Cyl_Aligned(d, size);
		} else if (platform == p_i386 || platform == p_pc98) {
			if (type != freebsd)
				break;
			if (!(flags & CHUNK_ALIGN))
				break;
			if (offset == d->chunks->offset &&
			    end == d->chunks->end)
				break;

			/* Round down to prev cylinder */
			offset = Prev_Cyl_Aligned(d,offset);
			/* Stay inside the parent */
			if (offset < c2->offset)
				offset = c2->offset;
			/* Round up to next cylinder */
			offset = Next_Cyl_Aligned(d, offset);
			/* Keep one track clear in front of parent */
			if (offset == c1->offset)
				offset = Next_Track_Aligned(d, offset + 1);
			/* Work on the (end+1) */
			size += offset;
			/* Round up to cylinder */
			size = Next_Cyl_Aligned(d, size);
			/* Stay inside parent */
			if ((size-1) > c2->end)
				size = c2->end + 1;
			/* Round down to cylinder */
			size = Prev_Cyl_Aligned(d, size);

			/* Convert back to size */
			size -= offset;
		}
		break;

/* PLATFORM POLICY END ------------------------------------- */
	}
	if (c2 == NULL)
		return (__LINE__);
	return Insert_Chunk(c2, offset, size, name, type, subtype, flags,
			    sname);
}

char *
ShowChunkFlags(struct chunk *c)
{
	static char ret[10];
	int i = 0;

	if (c->flags & CHUNK_ACTIVE)
		ret[i++] = 'A';
	if (c->flags & CHUNK_ALIGN)
		ret[i++] = '=';
	if (c->flags & CHUNK_IS_ROOT)
		ret[i++] = 'R';
	ret[i++] = '\0';

	return ret;
}

static void
Print_Chunk(struct chunk *c1,int offset)
{
	int i;

	if (!c1)
		return;
	for (i = 0; i < offset - 2; i++)
		putchar(' ');
	for (; i < offset; i++)
		putchar('-');
	putchar('>');
	for (; i < 10; i++)
		putchar(' ');
	printf("%p %8ld %8lu %8lu %-8s %-16s %-8s 0x%02x %s",
		c1, c1->offset, c1->size, c1->end, c1->name, c1->sname,
		chunk_name(c1->type), c1->subtype,
		ShowChunkFlags(c1));
	putchar('\n');
	Print_Chunk(c1->part, offset + 2);
	Print_Chunk(c1->next, offset);
}

void
Debug_Chunk(struct chunk *c1)
{

	Print_Chunk(c1,2);
}

int
Delete_Chunk(struct disk *d, struct chunk *c)
{

    return(Delete_Chunk2(d, c, 0));
}

int
Delete_Chunk2(struct disk *d, struct chunk *c, int rflags)
{
	struct chunk *c1 = 0, *c2, *c3;
	chunk_e type = c->type;
	u_long offset = c->offset;

	if(type == whole)
		return 1;
#ifndef PC98
	if (!c1 && (type == freebsd || type == fat || type == unknown))
		c1 = Find_Mother_Chunk(d->chunks, c->offset, c->end, extended);
#endif
	if (!c1 && (type == freebsd || type == fat || type == unknown))
		c1 = Find_Mother_Chunk(d->chunks, c->offset, c->end, whole);
#ifndef PC98
	if (!c1 && type == extended)
		c1 = Find_Mother_Chunk(d->chunks, c->offset, c->end, whole);
#endif
	if (!c1 && type == part)
		c1 = Find_Mother_Chunk(d->chunks, c->offset, c->end, freebsd);
	if (!c1)
		return 1;
	for (c2 = c1->part; c2; c2 = c2->next) {
		if (c2 == c) {
			c2->type = unused;
			c2->subtype = 0;
			c2->flags = 0;
			if (c2->sname != NULL)
				free(c2->sname);
			c2->sname = strdup("-");
			free(c2->name);
			c2->name = strdup("-");
			Free_Chunk(c2->part);
			c2->part =0;
			goto scan;
		}
	}
	return 1;
scan:
	/*
	 * Collapse multiple unused elements together, and attempt
	 * to extend the previous chunk into the freed chunk.
	 *
	 * We only extend non-unused elements which are marked
	 * for newfs (we can't extend working filesystems), and
	 * only if we are called with DELCHUNK_RECOVER.
	 */
	for (c2 = c1->part; c2; c2 = c2->next) {
		if (c2->type != unused) {
			if (c2->offset + c2->size != offset ||
			    (rflags & DELCHUNK_RECOVER) == 0 ||
			    (c2->flags & CHUNK_NEWFS) == 0) {
				continue;
			}
			/* else extend into free area */
		}
		if (!c2->next)
			continue;
		if (c2->next->type != unused)
			continue;
		c3 = c2->next;
		c2->size += c3->size;
		c2->end = c3->end;
		c2->next = c3->next;
		c3->next = 0;
		Free_Chunk(c3);
		goto scan;
	}
	Fixup_Names(d);
	return 0;
}

#if 0
int
Collapse_Chunk(struct disk *d, struct chunk *c1)
{
	struct chunk *c2, *c3;

	if (c1->next && Collapse_Chunk(d, c1->next))
		return 1;

	if (c1->type == unused && c1->next && c1->next->type == unused) {
		c3 = c1->next;
		c1->size += c3->size;
		c1->end = c3->end;
		c1->next = c3->next;
		c3->next = 0;
		Free_Chunk(c3);
		return 1;
	}
	c3 = c1->part;
	if (!c3)
		return 0;
	if (Collapse_Chunk(d, c1->part))
		return 1;

	if (c1->type == whole)
		return 0;

	if (c3->type == unused && c3->size == c1->size) {
		Delete_Chunk(d, c1);
		return 1;
	}
	if (c3->type == unused) {
		c2 = New_Chunk();
		if (c2 == NULL)
			barfout(1, "malloc failed");
		*c2 = *c1;
		c1->next = c2;
		c1->disk = d;
		c1->sname = strdup("-");
		c1->name = strdup("-");
		c1->part = 0;
		c1->type = unused;
		c1->flags = 0;
		c1->subtype = 0;
		c1->size = c3->size;
		c1->end = c3->end;
		c2->offset += c1->size;
		c2->size -= c1->size;
		c2->part = c3->next;
		c3->next = 0;
		Free_Chunk(c3);
		return 1;
	}
	for (c2 = c3; c2->next; c2 = c2->next)
		c3 = c2;
	if (c2 && c2->type == unused) {
		c3->next = 0;
		c2->next = c1->next;
		c1->next = c2;
		c1->size -= c2->size;
		c1->end -= c2->size;
		return 1;
	}

	return 0;
}
#endif
