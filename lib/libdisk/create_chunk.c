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
#include <sys/diskslice.h>
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

static int
Fixup_FreeBSD_Names(struct disk *d, struct chunk *c)
{
	struct chunk *c1, *c3;
	int j;
    
	if (!strcmp(c->name, "X")) return 0;
    
	/* reset all names to "X" */
	for (c1 = c->part; c1; c1 = c1->next) {
		c1->oname = c1->name;
		c1->name = malloc(12);
		if(!c1->name) return -1;
		strcpy(c1->name,"X");
	}
    
	/* Allocate the first swap-partition we find */
	for (c1 = c->part; c1; c1 = c1->next) {
		if (c1->type == unused) continue;
		if (c1->subtype != FS_SWAP) continue;
		sprintf(c1->name, "%s%c", c->name, SWAP_PART + 'a');
		break;
	}
    
	/* Allocate the first root-partition we find */
	for (c1 = c->part; c1; c1 = c1->next) {
		if (c1->type == unused) continue;
		if (!(c1->flags & CHUNK_IS_ROOT)) continue;
		sprintf(c1->name, "%s%c", c->name, 0 + 'a');
		break;
	}
    
	/* Try to give them the same as they had before */
	for (c1 = c->part; c1; c1 = c1->next) {
		if (strcmp(c1->name, "X")) continue;
		for(c3 = c->part; c3 ; c3 = c3->next)
			if (c1 != c3 && !strcmp(c3->name, c1->oname)) {
				    goto newname;
			}
		strcpy(c1->name, c1->oname);
		newname: ;
	}
    
    
	/* Allocate the rest sequentially */
	for (c1 = c->part; c1; c1 = c1->next) {
		const char order[] = "efghabd";
		if (c1->type == unused) continue;
		if (strcmp("X", c1->name)) continue;
	
		for(j = 0; j < strlen(order); j++) {
			sprintf(c1->name, "%s%c", c->name, order[j]);
			for(c3 = c->part; c3 ; c3 = c3->next)
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

static int
Fixup_Extended_Names(struct disk *d, struct chunk *c)
{
	struct chunk *c1;
	int j=5;
    
	for (c1 = c->part; c1; c1 = c1->next) {
		if (c1->type == unused) continue;
		free(c1->name);
		c1->name = malloc(12);
		if(!c1->name) return -1;
		sprintf(c1->name, "%ss%d", d->chunks->name, j++);
		if (c1->type == freebsd)
			if (Fixup_FreeBSD_Names(d, c1) != 0)
				return -1;
	}
	return 0;
}

int
Fixup_Names(struct disk *d)
    {
	struct chunk *c1, *c2;
	#ifdef __i386__
	struct chunk *c3;
	int j;
	#endif

	c1 = d->chunks;
	for(c2 = c1->part; c2 ; c2 = c2->next) {
		c2->flags &= ~CHUNK_BSD_COMPAT;
		if (c2->type == unused)
			continue;
		if (strcmp(c2->name, "X"))
			continue;
#ifdef __i386__
		c2->oname = malloc(12);
		if(!c2->oname) return -1;
		for(j = 1; j <= NDOSPART; j++) {
			sprintf(c2->oname, "%ss%d", c1->name, j);
			for(c3 = c1->part; c3; c3 = c3->next)
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
	for(c2 = c1->part; c2; c2 = c2->next) {
		if (c2->type == freebsd) {
			c2->flags |= CHUNK_BSD_COMPAT;
			break;
		}
	}
	for(c2 = c1->part; c2; c2 = c2->next) {
		if (c2->type == freebsd)
			Fixup_FreeBSD_Names(d, c2);
#ifndef PC98
		if (c2->type == extended)
			Fixup_Extended_Names(d, c2);
#endif
	}
	return 0;
}

int
Create_Chunk(struct disk *d, u_long offset, u_long size, chunk_e type, int subtype, u_long flags, const char *sname)
{
	int i;
	u_long l;
	
	if(!(flags & CHUNK_FORCE_ALL)) {
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
		l = (offset+size) % (d->bios_sect * d->bios_hd);
		size -= l;
	}
	
	i = Add_Chunk(d, offset, size, "X", type, subtype, flags, sname);
	Fixup_Names(d);
	return i;
}

struct chunk *
Create_Chunk_DWIM(struct disk *d, const struct chunk *parent , u_long size, chunk_e type, int subtype, u_long flags)
{
	int i;
	struct chunk *c1;
	u_long offset;
	
	if (!parent)
		parent = d->chunks;
	for (c1=parent->part; c1; c1 = c1->next) {
		if (c1->type != unused) continue;
		if (c1->size < size) continue;
		offset = c1->offset;
		goto found;
	}
	return 0;
     found:
	i = Add_Chunk(d, offset, size, "X", type, subtype, flags, "-");
	if (i)
		return 0;
	Fixup_Names(d);
	for (c1=parent->part; c1; c1 = c1->next)
		if (c1->offset == offset)
			return c1;
	/* barfout(1, "Serious internal trouble"); */
	return 0;
}
