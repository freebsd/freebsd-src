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
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/param.h>
#include <sys/disklabel.h>
#ifdef PC98
#include <sys/diskpc98.h>
#else
#include <sys/diskmbr.h>
#endif
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include "libdisk.h"

struct chunk *New_Chunk(void);

static int
Fixup_FreeBSD_Names(struct chunk *c)
{
	struct chunk *c1, *c3;
	uint j;
    
	if (!strcmp(c->name, "X"))
		return 0;
    
	/* reset all names to "X" */
	for (c1 = c->part; c1; c1 = c1->next) {
		c1->oname = c1->name;
		c1->name = malloc(12);
		if (!c1->name)
			return -1;
		strcpy(c1->name,"X");
	}
    
	/* Allocate the first swap-partition we find */
	for (c1 = c->part; c1; c1 = c1->next) {
		if (c1->type == unused)
			continue;
		if (c1->subtype != FS_SWAP)
			continue;
		sprintf(c1->name, "%s%c", c->name, SWAP_PART + 'a');
		break;
	}
    
	/* Allocate the first root-partition we find */
	for (c1 = c->part; c1; c1 = c1->next) {
		if (c1->type == unused)
			continue;
		if (!(c1->flags & CHUNK_IS_ROOT))
			continue;
		sprintf(c1->name, "%s%c", c->name, 0 + 'a');
		break;
	}
    
	/* Try to give them the same as they had before */
	for (c1 = c->part; c1; c1 = c1->next) {
		if (strcmp(c1->name, "X"))
			continue;
		for (c3 = c->part; c3 ; c3 = c3->next)
			if (c1 != c3 && !strcmp(c3->name, c1->oname))
				goto newname;
		strcpy(c1->name, c1->oname);
	newname:
		;
	}

	/* Allocate the rest sequentially */
	for (c1 = c->part; c1; c1 = c1->next) {
		const char order[] = "defghab";

		if (c1->type == unused)
			continue;
		if (strcmp("X", c1->name))
			continue;
	
		for (j = 0; j < strlen(order); j++) {
			sprintf(c1->name, "%s%c", c->name, order[j]);
			for (c3 = c->part; c3 ; c3 = c3->next)
				if (c1 != c3 && !strcmp(c3->name, c1->name))
					goto match;
			break;
		match:
			strcpy(c1->name, "X");
			continue;
		}
	}
	for (c1 = c->part; c1; c1 = c1->next) {
		free(c1->oname);
		c1->oname = 0;
	}
	return 0;
}

#ifndef PC98
static int
Fixup_Extended_Names(struct chunk *c)
{
	struct chunk *c1;
	int j = 5;
    
	for (c1 = c->part; c1; c1 = c1->next) {
		if (c1->type == unused)
			continue;
		free(c1->name);
		c1->name = malloc(12);
		if (!c1->name)
			return -1;
		sprintf(c1->name, "%ss%d", c->disk->chunks->name, j++);
		if (c1->type == freebsd)
			if (Fixup_FreeBSD_Names(c1) != 0)
				return -1;
	}
	return 0;
}
#endif

#ifdef __powerpc__
static int
Fixup_Apple_Names(struct chunk *c)
{
	struct chunk *c1;

	for (c1 = c->part; c1; c1 = c1->next) {
		if (c1->type == unused)
			continue;
		free(c1->name);
		c1->name = strdup(c->name);
		if (!c1->name)
			return (-1);
	}
	return 0;
}
#endif

int
Fixup_Names(struct disk *d)
{
	struct chunk *c1, *c2;
#if defined(__i386__) || defined(__ia64__) || defined(__amd64__)
	struct chunk *c3;
	int j, max;
#endif

	c1 = d->chunks;
	for (c2 = c1->part; c2 ; c2 = c2->next) {
		if (c2->type == unused)
			continue;
		if (strcmp(c2->name, "X"))
			continue;
#if defined(__i386__) || defined(__ia64__) || defined(__amd64__)
		c2->oname = malloc(12);
		if (!c2->oname)
			return -1;
#ifdef __ia64__
		max = d->gpt_size;
#else
		max = NDOSPART;
#endif
		for (j = 1; j <= max; j++) {
#ifdef __ia64__
			sprintf(c2->oname, "%s%c%d", c1->name,
			    (c1->type == whole) ? 'p' : 's', j);
#else
			sprintf(c2->oname, "%ss%d", c1->name, j);
#endif
			for (c3 = c1->part; c3; c3 = c3->next)
				if (c3 != c2 && !strcmp(c3->name, c2->oname))
					goto match;
			free(c2->name);
			c2->name = c2->oname;
			c2->oname = 0;
			break;
		match:
			continue;
		}
		if (c2->oname)
			free(c2->oname);
#else
		free(c2->name);
		c2->name = strdup(c1->name);
#endif /*__i386__*/
	}
	for (c2 = c1->part; c2; c2 = c2->next) {
		if (c2->type == freebsd)
			Fixup_FreeBSD_Names(c2);
#ifdef __powerpc__
		else if (c2->type == apple)
			Fixup_Apple_Names(c2);
#endif
#ifndef PC98
		else if (c2->type == extended)
			Fixup_Extended_Names(c2);
#endif
	}
	return 0;
}

int
Create_Chunk(struct disk *d, daddr_t offset, daddr_t size, chunk_e type,
	     int subtype, u_long flags, const char *sname)
{
	int i;

#ifndef __ia64__
	if (!(flags & CHUNK_FORCE_ALL)) {
		daddr_t l;
#ifdef PC98
		/* Never use the first cylinder */
		if (!offset) {
			offset += (d->bios_sect * d->bios_hd);
			size -= (d->bios_sect * d->bios_hd);
		}
#else
		/* Never use the first track */
		if (!offset) {
			offset += d->bios_sect;
			size -= d->bios_sect;
		}
#endif /* PC98 */
		
		/* Always end on cylinder boundary */
		l = (offset + size) % (d->bios_sect * d->bios_hd);
		size -= l;
	}
#endif
	
	i = Add_Chunk(d, offset, size, "X", type, subtype, flags, sname);
	Fixup_Names(d);
	return i;
}

struct chunk *
Create_Chunk_DWIM(struct disk *d, struct chunk *parent, daddr_t size,
		  chunk_e type, int subtype, u_long flags)
{
	int i;
	struct chunk *c1;
	daddr_t offset;
	
	if (!parent)
		parent = d->chunks;

	if ((parent->type == freebsd  && type == part && parent->part == NULL) 
	    || (parent->type == apple && type == part && parent->part == NULL)) {
		c1 = New_Chunk();
		if (c1 == NULL)
			return (NULL);
		c1->disk = parent->disk;
		c1->offset = parent->offset;
		c1->size = parent->size;
		c1->end = parent->offset + parent->size - 1;
		c1->type = unused;
		if (parent->sname != NULL)
			c1->sname = strdup(parent->sname);
		c1->name = strdup("-");
		parent->part = c1;
	}
		
	for (c1 = parent->part; c1; c1 = c1->next) {
		if (c1->type != unused)
			continue;
		if (c1->size < size)
			continue;
		offset = c1->offset;
		goto found;
	}
	return 0;
found:
	i = Add_Chunk(d, offset, size, "X", type, subtype, flags, "-");
	if (i)
		return 0;
	Fixup_Names(d);
	for (c1 = parent->part; c1; c1 = c1->next)
		if (c1->offset == offset)
			return c1;
	/* barfout(1, "Serious internal trouble"); */
	return 0;
}
