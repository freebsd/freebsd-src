/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: create_chunk.c,v 1.21.2.1 1995/09/20 10:43:02 jkh Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include "libdisk.h"

void
Fixup_FreeBSD_Names(struct disk *d, struct chunk *c)
{
	struct chunk *c1, *c3;
	int j;

	if (!strcmp(c->name, "X")) return;

	/* reset all names to "X" */
	for (c1 = c->part; c1 ; c1 = c1->next) {
		c1->oname = c1->name;
		c1->name = malloc(12);
		if(!c1->name) err(1,"Malloc failed");
		strcpy(c1->name,"X");
	}

	/* Allocate the first swap-partition we find */
	for (c1 = c->part; c1 ; c1 = c1->next) {
		if (c1->type == unused) continue;
		if (c1->subtype != FS_SWAP) continue;
		sprintf(c1->name,"%s%c",c->name,SWAP_PART+'a');
		break;
	}

	/* Allocate the first root-partition we find */
	for (c1 = c->part; c1 ; c1 = c1->next) {
		if (c1->type == unused) continue;
		if (!(c1->flags & CHUNK_IS_ROOT)) continue;
		sprintf(c1->name,"%s%c",c->name,0+'a');
		break;
	}

	/* Try to give them the same as they had before */
	for (c1 = c->part; c1 ; c1 = c1->next) {
		if (strcmp(c1->name,"X")) continue;
		for(c3 = c->part; c3 ; c3 = c3->next)
			if (c1 != c3 && !strcmp(c3->name, c1->oname)) {
				goto newname;
			}
		strcpy(c1->name,c1->oname);
	    newname:
	}


	/* Allocate the rest sequentially */
	for (c1 = c->part; c1 ; c1 = c1->next) {
		const char order[] = "efghabd";
		if (c1->type == unused) continue;
		if (strcmp("X",c1->name)) continue;

		for(j=0;j<strlen(order);j++) {
			sprintf(c1->name,"%s%c",c->name,order[j]);
			for(c3 = c->part; c3 ; c3 = c3->next)
				if (c1 != c3 && !strcmp(c3->name, c1->name))
					goto match;
			break;
		match:
			strcpy(c1->name,"X");
			continue;
		}
	}
	for (c1 = c->part; c1 ; c1 = c1->next) {
		free(c1->oname);
		c1->oname = 0;
	}
}

void
Fixup_Extended_Names(struct disk *d, struct chunk *c)
{
	struct chunk *c1;
	int j=5;

	for (c1 = c->part; c1 ; c1 = c1->next) {
		if (c1->type == unused) continue;
		free(c1->name);
		c1->name = malloc(12);
		if(!c1->name) err(1,"malloc failed");
		sprintf(c1->name,"%ss%d",d->chunks->name,j++);
		if (c1->type == freebsd)
			Fixup_FreeBSD_Names(d,c1);
	}
}

void
Fixup_Names(struct disk *d)
{
	struct chunk *c1, *c2, *c3;
	int i,j;

	c1 = d->chunks;
	for(i=1,c2 = c1->part; c2 ; c2 = c2->next) {
		c2->flags &= ~CHUNK_BSD_COMPAT;
		if (c2->type == unused)
			continue;
		if (strcmp(c2->name,"X"))
			continue;
		c2->oname = malloc(12);
		if(!c2->oname) err(1,"malloc failed");
		for(j=1;j<=NDOSPART;j++) {
			sprintf(c2->oname,"%ss%d",c1->name,j);
			for(c3 = c1->part; c3 ; c3 = c3->next)
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
	}
	for(c2 = c1->part; c2 ; c2 = c2->next) {
		if (c2->type == freebsd) {
			c2->flags |= CHUNK_BSD_COMPAT;
			break;
		}
	}
	for(c2 = c1->part; c2 ; c2 = c2->next) {
		if (c2->type == freebsd)
			Fixup_FreeBSD_Names(d,c2);
		if (c2->type == extended)
			Fixup_Extended_Names(d,c2);
	}
}

int
Create_Chunk(struct disk *d, u_long offset, u_long size, chunk_e type, int subtype, u_long flags)
{
	int i;
	u_long l;

	if(!(flags & CHUNK_FORCE_ALL))
	{
		/* Never use the first track */
		if (!offset) {
			offset += d->bios_sect;
			size -= d->bios_sect;
		}

		/* Always end on cylinder boundary */
		l = (offset+size) % (d->bios_sect * d->bios_hd);
		size -= l;
	}

	i = Add_Chunk(d,offset,size,"X",type,subtype,flags);
	Fixup_Names(d);
	return i;
}

struct chunk *
Create_Chunk_DWIM(struct disk *d, struct chunk *parent , u_long size, chunk_e type, int subtype, u_long flags)
{
	int i;
	struct chunk *c1;
	u_long offset,edge;

	if (!parent)
		parent = d->chunks;
	for (c1=parent->part; c1 ; c1 = c1->next) {
		if (c1->type != unused) continue;
		if (c1->size < size) continue;
		offset = c1->offset;
		goto found;
	}
	warn("Not enough unused space");
	return 0;
    found:
	if (parent->flags & CHUNK_BAD144) {
		edge = c1->end - d->bios_sect - 127;
		if (offset > edge)
			return 0;
		if (offset + size > edge)
			size = edge - offset + 1;
	}
	i = Add_Chunk(d,offset,size,"X",type,subtype,flags);
	if (i) {
		warn("Didn't cut it");
		return 0;
	}
	Fixup_Names(d);
	for (c1=parent->part; c1 ; c1 = c1->next)
		if (c1->offset == offset)
			return c1;
	err(1,"Serious internal trouble");
}

int
MakeDev(struct chunk *c1, char *path)
{
	char *p = c1->name;
	u_long cmaj,bmaj,min,unit,part,slice;
	char buf[BUFSIZ],buf2[BUFSIZ];

	*buf2 = '\0';

	if(!strcmp(p,"X"))
	    return 0;

	if (p[0] == 'w' && p[1] == 'd') {
		bmaj = 0; cmaj = 3;
	} else if (p[0] == 's' && p[1] == 'd') {
		bmaj = 4; cmaj = 13;
	} else {
		return 0;
	}
	p += 2;
	if (!isdigit(*p))
		return 0;
	unit = *p - '0';
	p++;
	if (isdigit(*p)) {
		unit *= 10;
		unit += (*p - '0');
		p++;
	}
	if (!*p) {
		slice = 1;
		part = 2;
		goto done;
	}
	if (*p != 's')
		return 0;
	p++;
	if (!isdigit(*p))
		return 0;
	slice = *p - '0';
	p++;
	if (isdigit(*p)) {
		slice *= 10;
		slice += (*p - '0');
		p++;
	}
	slice = slice+1;
	if (!*p) {
		part = 2;
		if(c1->type == freebsd)
			sprintf(buf2,"%sc",c1->name);
		goto done;
	}
	if (*p < 'a' || *p > 'h')
		return 0;
	part = *p - 'a';
    done:
	if (unit > 32)
		return 0;
	if (slice > 32)
		return 0;
	min = unit * 8 + 65536 * slice + part;
	sprintf(buf,"%s/r%s",path,c1->name);
	unlink(buf);
	if (mknod(buf,S_IFCHR|0640,makedev(cmaj,min)) == -1) {
	    perror("mknod");
	    return 0;
	}
	if (*buf2) {
		sprintf(buf,"%s/r%s",path,buf2);
		unlink(buf);
		if (mknod(buf,S_IFCHR|0640,makedev(cmaj,min)) == -1) {
		    perror("mknod");
		    return 0;
		}
	}
	sprintf(buf,"%s/%s",path,c1->name);
	unlink(buf);
	if (mknod(buf, S_IFBLK|0640, makedev(bmaj,min)) == -1) {
	    perror("mknod");
	    return 0;
	}
	return 1;
}

int
MakeDevChunk(struct chunk *c1,char *path)
{
    int i = 1;

    if (!MakeDev(c1, path))
	return 0;
    if (c1->next) i = MakeDevChunk(c1->next,path);
    if (c1->part) i |= MakeDevChunk(c1->part, path);
    return i;
}

int
MakeDevDisk(struct disk *d,char *path)
{
    return MakeDevChunk(d->chunks,path);
}
