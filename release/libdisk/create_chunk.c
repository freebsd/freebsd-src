/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: create_chunk.c,v 1.5 1995/04/30 11:04:12 phk Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/types.h>
#include <err.h>
#include "libdisk.h"

void
Fixup_FreeBSD_Names(struct disk *d, struct chunk *c)
{
	struct chunk *c1, *c3;
	int j;
	char *p=0;

	if (!strcmp(c->name, "X")) return;
	for (c1 = c->part; c1 ; c1 = c1->next) {
		if (c1->type == unused) continue;
		if (c1->type == reserved) continue;
		if (strcmp(c1->name, "X")) continue;
		for(j=0;j<8;j++) {
			if (j == 2)
				continue;
			p = malloc(12);
			if(!p) err(1,"malloc failed");
			sprintf(p,"%s%c",c->name,j+'a');
			for(c3 = c->part; c3 ; c3 = c3->next) 
				if (c3 != c1 && !strcmp(c3->name, p))
					goto match;
			free(c1->name);
			c1->name = p;
			p = 0;
			break;
			match:
				continue;
		}
		if(p)
			free(p);
	}
}

void
Fixup_Extended_Names(struct disk *d, struct chunk *c)
{
	struct chunk *c1, *c3;
	int j;
	char *p=0;

	for (c1 = c->part; c1 ; c1 = c1->next) {
		if (c1->type == freebsd)
			Fixup_FreeBSD_Names(d,c1);
		if (c1->type == unused) continue;
		if (c1->type == reserved) continue;
		if (strcmp(c1->name, "X")) continue;
		for(j=5;j<=29;j++) {
			p = malloc(12);
			if(!p) err(1,"malloc failed");
			sprintf(p,"%ss%d",c->name,j);
			for(c3 = c->part; c3 ; c3 = c3->next) 
				if (c3 != c1 && !strcmp(c3->name, p))
					goto match;
			free(c1->name);
			c1->name = p;
			p = 0;
			break;
			match:
				continue;
		}
		if(p)
			free(p);
	}
}

void
Fixup_Names(struct disk *d)
{
	struct chunk *c1, *c2, *c3;
	int i,j;
	char *p=0;

	c1 = d->chunks;
	for(i=1,c2 = c1->part; c2 ; c2 = c2->next) {
		if (c2->type == freebsd)
			Fixup_FreeBSD_Names(d,c2);
		if (c2->type == extended)
			Fixup_Extended_Names(d,c2);
		if (c2->type == unused)
			continue;
		if (c2->type == reserved)
			continue;
		if (strcmp(c2->name,"X"))
			continue;
		p = malloc(12);
		if(!p) err(1,"malloc failed");
		for(j=1;j<=NDOSPART;j++) {
			sprintf(p,"%ss%d",c1->name,j);
			for(c3 = c1->part; c3 ; c3 = c3->next) 
				if (c3 != c2 && !strcmp(c3->name, p))
					goto match;
			free(c2->name);
			c2->name = p;
			p = 0;
			break;
			match:
				continue;
		}
		if(p)
			free(p);
	}
}

int
Create_Chunk(struct disk *d, u_long offset, u_long size, chunk_e type, int subtype, u_long flags)
{
	int i;

	if (type == freebsd)
		subtype = 0xa5;
	i = Add_Chunk(d,offset,size,"X",type,subtype,flags);
	Fixup_Names(d);
	return i;
}
