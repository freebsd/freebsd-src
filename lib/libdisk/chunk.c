/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD: src/lib/libdisk/chunk.c,v 1.21.2.2 2000/07/17 21:24:55 jhb Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <err.h>
#include "libdisk.h"

#define new_chunk() memset(malloc(sizeof(struct chunk)), 0, sizeof(struct chunk))

/* Is c2 completely inside c1 ? */

static int
Chunk_Inside(struct chunk *c1, struct chunk *c2)
{
	/* if c1 ends before c2 do */
	if (c1->end < c2->end)
		return 0;
	/* if c1 starts after c2 do */
	if (c1->offset > c2->offset)
		return 0;
	return 1;
}

struct chunk *
Find_Mother_Chunk(struct chunk *chunks, u_long offset, u_long end, chunk_e type)
{
	struct chunk *c1,*c2,ct;
	ct.offset = offset;
	ct.end = end;
	switch (type) {
		case whole:
			if (Chunk_Inside(chunks,&ct))
				return chunks;
#ifndef PC98
		case extended:
			for(c1=chunks->part;c1;c1=c1->next) {
				if (c1->type != type)
					continue;
				if (Chunk_Inside(c1,&ct))
					return c1;
			}
			return 0;
#endif
		case freebsd:
			for(c1=chunks->part;c1;c1=c1->next) {
				if (c1->type == type)
					if (Chunk_Inside(c1,&ct))
						return c1;
				if (c1->type != extended)
					continue;
				for(c2=c1->part;c2;c2=c2->next)
					if (c2->type == type
					    && Chunk_Inside(c2,&ct))
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
	if(!c1) return;
	if(c1->private_data && c1->private_free)
		(*c1->private_free)(c1->private_data);
	if(c1->part)
		Free_Chunk(c1->part);
	if(c1->next)
		Free_Chunk(c1->next);
	free(c1->name);
#ifdef PC98
	free(c1->sname);
#endif
	free(c1);
}

struct chunk *
Clone_Chunk(struct chunk *c1)
{
	struct chunk *c2;
	if(!c1)
		return 0;
	c2 = new_chunk();
	if (!c2) err(1,"malloc failed");
	*c2 = *c1;
	if (c1->private_data && c1->private_clone)
		c2->private_data = c2->private_clone(c2->private_data);
	c2->name = strdup(c2->name);
#ifdef PC98
	c2->sname = strdup(c2->sname);
#endif
	c2->next = Clone_Chunk(c2->next);
	c2->part = Clone_Chunk(c2->part);
	return c2;
}

int
#ifdef PC98
Insert_Chunk(struct chunk *c2, u_long offset, u_long size, const char *name,
	chunk_e type, int subtype, u_long flags, const char *sname)
#else
Insert_Chunk(struct chunk *c2, u_long offset, u_long size, const char *name,
	chunk_e type, int subtype, u_long flags)
#endif
{
	struct chunk *ct,*cs;

	/* We will only insert into empty spaces */
	if (c2->type != unused)
		return __LINE__;

	ct = new_chunk();
	if (!ct) err(1,"malloc failed");
	memset(ct,0,sizeof *ct);
	ct->disk = c2->disk;
	ct->offset = offset;
	ct->size = size;
	ct->end = offset + size - 1;
	ct->type = type;
#ifdef PC98
	ct->sname = strdup(sname);
#endif
	ct->name = strdup(name);
	ct->subtype = subtype;
	ct->flags = flags;

	if (!Chunk_Inside(c2,ct)) {
		Free_Chunk(ct);
		return __LINE__;
	}

	if(type==freebsd || type==extended) {
		cs = new_chunk();
		if (!cs) err(1,"malloc failed");
		memset(cs,0,sizeof *cs);
		cs->disk = c2->disk;
		cs->offset = offset;
		cs->size = size;
		cs->end = offset + size - 1;
		cs->type = unused;
#ifdef PC98
		cs->sname = strdup(sname);
#endif
		cs->name = strdup("-");
		ct->part = cs;
	}

	/* Make a new chunk for any trailing unused space */
	if (c2->end > ct->end) {
		cs = new_chunk();
		if (!cs) err(1,"malloc failed");
		*cs = *c2;
		cs->disk = c2->disk;
		cs->offset = ct->end + 1;
		cs->size = c2->end - ct->end;
#ifdef PC98
		if(c2->sname)
			cs->sname = strdup(c2->sname);
#endif
		if(c2->name)
			cs->name = strdup(c2->name);
		c2->next = cs;
		c2->size -= c2->end - ct->end;
		c2->end = ct->end;
	}
	/* If no leading unused space just occupy the old chunk */
	if (c2->offset == ct->offset) {
#ifdef PC98
		c2->sname = ct->sname;
#endif
		c2->name = ct->name;
		c2->type = ct->type;
		c2->part = ct->part;
		c2->subtype = ct->subtype;
		c2->flags = ct->flags;
#ifdef PC98
		ct->sname = 0;
#endif
		ct->name = 0;
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
#ifdef PC98
Add_Chunk(struct disk *d, long offset, u_long size, const char *name,
	chunk_e type, int subtype, u_long flags, const char *sname)
#else
Add_Chunk(struct disk *d, long offset, u_long size, const char *name,
	chunk_e type, int subtype, u_long flags)
#endif
{
	struct chunk *c1,*c2,ct;
	u_long end = offset + size - 1;
	ct.offset = offset;
	ct.end = end;
	ct.size = size;

	if (type == whole) {
		d->chunks = c1 = new_chunk();
		if (!c1) err(1,"malloc failed");
		memset(c1,0,sizeof *c1);
		c2 = c1->part = new_chunk();
		if (!c2) err(1,"malloc failed");
		memset(c2,0,sizeof *c2);
		c2->disk = c1->disk = d;
		c2->offset = c1->offset = offset;
		c2->size = c1->size = size;
		c2->end = c1->end = end;
#ifdef PC98
		c1->sname = strdup(sname);
		c2->sname = strdup("-");
#endif
		c1->name = strdup(name);
		c2->name = strdup("-");
		c1->type = type;
		c2->type = unused;
		c1->flags = flags;
		c1->subtype = subtype;
		return 0;
	}
	if (type == freebsd)
#ifdef PC98
		subtype = 0xc494;
#else
		subtype = 0xa5;
#endif
	c1 = 0;
#ifndef PC98
	if(!c1 && (type == freebsd || type == fat || type == unknown))
		c1 = Find_Mother_Chunk(d->chunks,offset,end,extended);
#endif
	if(!c1 && (type == freebsd || type == fat || type == unknown))
		c1 = Find_Mother_Chunk(d->chunks,offset,end,whole);
#ifndef PC98
	if(!c1 && type == extended)
		c1 = Find_Mother_Chunk(d->chunks,offset,end,whole);
#endif
	if(!c1 && type == part)
		c1 = Find_Mother_Chunk(d->chunks,offset,end,freebsd);
	if(!c1)
		return __LINE__;
	for(c2=c1->part;c2;c2=c2->next) {
		if (c2->type != unused)
			continue;
		if(Chunk_Inside(c2,&ct)) {
			if (type != freebsd)
				goto doit;
			if (!(flags & CHUNK_ALIGN))
				goto doit;
			if (offset == d->chunks->offset
			   && end == d->chunks->end)
				goto doit;

			/* Round down to prev cylinder */
			offset = Prev_Cyl_Aligned(d,offset);
			/* Stay inside the parent */
			if (offset < c2->offset)
				offset = c2->offset;
			/* Round up to next cylinder */
			offset = Next_Cyl_Aligned(d,offset);
			/* Keep one track clear in front of parent */
			if (offset == c1->offset)
				offset = Next_Track_Aligned(d,offset+1);

			/* Work on the (end+1) */
			size += offset;
			/* Round up to cylinder */
			size = Next_Cyl_Aligned(d,size);
			/* Stay inside parent */
			if ((size-1) > c2->end)
				size = c2->end+1;
			/* Round down to cylinder */
			size = Prev_Cyl_Aligned(d,size);

			/* Convert back to size */
			size -= offset;

		    doit:
#ifdef PC98
			return Insert_Chunk(c2,offset,size,name,
				type,subtype,flags,sname);
#else
			return Insert_Chunk(c2,offset,size,name,
				type,subtype,flags);
#endif
		}
	}
	return __LINE__;
}

char *
ShowChunkFlags(struct chunk *c)
{
	static char ret[10];

	int i=0;
	if (c->flags & CHUNK_BSD_COMPAT)	ret[i++] = 'C';
	if (c->flags & CHUNK_ACTIVE)		ret[i++] = 'A';
	if (c->flags & CHUNK_ALIGN)		ret[i++] = '=';
	if (c->flags & CHUNK_IS_ROOT)		ret[i++] = 'R';
	ret[i++] = '\0';
	return ret;
}

void
Print_Chunk(struct chunk *c1,int offset)
{
	int i;
	if(!c1) return;
	for(i=0;i<offset-2;i++) putchar(' ');
	for(;i<offset;i++) putchar('-');
	putchar('>');
	for(;i<10;i++) putchar(' ');
#ifdef PC98
	printf("%p %8ld %8lu %8lu %-8s %-16s %-8s 0x%02x %s",
		c1, c1->offset, c1->size, c1->end, c1->name, c1->sname,
#else
	printf("%p %8ld %8lu %8lu %-8s %-8s 0x%02x %s",
		c1, c1->offset, c1->size, c1->end, c1->name,
#endif
		chunk_n[c1->type],c1->subtype,
		ShowChunkFlags(c1));
	putchar('\n');
	Print_Chunk(c1->part,offset + 2);
	Print_Chunk(c1->next,offset);
}

void
Debug_Chunk(struct chunk *c1)
{
	Print_Chunk(c1,2);
}

int
Delete_Chunk(struct disk *d, struct chunk *c)
{
	struct chunk *c1=0,*c2,*c3;
	chunk_e type = c->type;

	if(type == whole)
		return 1;
#ifndef PC98
	if(!c1 && (type == freebsd || type == fat || type == unknown))
		c1 = Find_Mother_Chunk(d->chunks,c->offset,c->end,extended);
#endif
	if(!c1 && (type == freebsd || type == fat || type == unknown))
		c1 = Find_Mother_Chunk(d->chunks,c->offset,c->end,whole);
#ifndef PC98
	if(!c1 && type == extended)
		c1 = Find_Mother_Chunk(d->chunks,c->offset,c->end,whole);
#endif
	if(!c1 && type == part)
		c1 = Find_Mother_Chunk(d->chunks,c->offset,c->end,freebsd);
	if(!c1)
		return 1;
	for(c2=c1->part;c2;c2=c2->next) {
		if (c2 == c) {
			c2->type = unused;
			c2->subtype = 0;
			c2->flags = 0;
#ifdef PC98
			free(c2->sname);
			c2->sname = strdup("-");
#endif
			free(c2->name);
			c2->name = strdup("-");
			Free_Chunk(c2->part);
			c2->part =0;
			goto scan;
		}
	}
	return 1;
    scan:
	for(c2=c1->part;c2;c2=c2->next) {
		if (c2->type != unused)
			continue;
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

	if(c1->next && Collapse_Chunk(d,c1->next))
		return 1;

	if(c1->type == unused && c1->next && c1->next->type == unused) {
		c3 = c1->next;
		c1->size += c3->size;
		c1->end = c3->end;
		c1->next = c3->next;
		c3->next = 0;
		Free_Chunk(c3);
		return 1;
	}
	c3 = c1->part;
	if(!c3)
		return 0;
	if (Collapse_Chunk(d,c1->part))
		return 1;

	if (c1->type == whole)
		return 0;

	if(c3->type == unused && c3->size == c1->size) {
		Delete_Chunk(d,c1);
		return 1;
	}
	if(c3->type == unused) {
		c2 = new_chunk();
		if (!c2) err(1,"malloc failed");
		*c2 = *c1;
		c1->next = c2;
		c1->disk = d;
#ifdef PC98
		c1->sname = strdup("-");
#endif
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
	for(c2=c3;c2->next;c2 = c2->next)
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
