/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: create_chunk.c,v 1.24 1996/04/29 05:03:01 jkh Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include "libdisk.h"

/* Clone these two from sysinstall because we need our own copies
 * due to link order problems with `crunch'.  Feh!
 */
static int
isDebug()
{
    static int debug = 0;	/* Allow debugger to tweak it */

    return debug;
}

/* Write something to the debugging port */
static void
msgDebug(char *fmt, ...)
{
    va_list args;
    char *dbg;
    static int DebugFD = -1;

    if (DebugFD == -1)
	DebugFD = open("/dev/ttyv1", O_RDWR);
    dbg = (char *)alloca(FILENAME_MAX);
    strcpy(dbg, "DEBUG: ");
    va_start(args, fmt);
    vsnprintf((char *)(dbg + strlen(dbg)), FILENAME_MAX, fmt, args);
    va_end(args);
    write(DebugFD, dbg, strlen(dbg));
}

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
    if (i)
	return 0;
    Fixup_Names(d);
    for (c1=parent->part; c1 ; c1 = c1->next)
	if (c1->offset == offset)
	    return c1;
    err(1,"Serious internal trouble");
}

int
MakeDev(struct chunk *c1, const char *path)
{
    char *p = c1->name;
    u_long cmaj, bmaj, min, unit, part, slice;
    char buf[BUFSIZ], buf2[BUFSIZ];

    *buf2 = '\0';
    if (isDebug())
	msgDebug("MakeDev: Called with %s on path %s\n", p, path);
    if (!strcmp(p, "X"))
	return 0;
    
    if (!strncmp(p, "wd", 2))
	bmaj = 0, cmaj = 3;
    else if (!strncmp(p, "sd", 2))
	bmaj = 4, cmaj = 13;
    else if (!strncmp(p, "od", 2))
	bmaj = 20, cmaj = 70;
    else {
	return 0;
    }
    p += 2;
    if (!isdigit(*p)) {
	msgDebug("MakeDev: Invalid disk unit passed: %s\n", p);
	return 0;
    }
    unit = *p - '0';
    p++;
    if (!*p) {
	slice = 1;
	part = 2;
	goto done;
    }
    else if (isdigit(*p)) {
	unit *= 10;
	unit += (*p - '0');
	p++;
    }
    if (*p != 's') {
	msgDebug("MakeDev: `%s' is not a valid slice delimiter\n", p);
	return 0;
    }
    p++;
    if (!isdigit(*p)) {
	msgDebug("MakeDev: `%s' is an invalid slice number\n", p);
	return 0;
    }
    slice = *p - '0';
    p++;
    if (isdigit(*p)) {
	slice *= 10;
	slice += (*p - '0');
	p++;
    }
    slice = slice + 1;
    if (!*p) {
	part = 2;
	if(c1->type == freebsd)
	    sprintf(buf2, "%sc", c1->name);
	goto done;
    }
    if (*p < 'a' || *p > 'h') {
	msgDebug("MakeDev: `%s' is not a valid partition name.\n", p);
	return 0;
    }
    part = *p - 'a';
 done:
    if (isDebug())
	msgDebug("MakeDev: Unit %d, Slice %d, Part %d\n", unit, slice, part);
    if (unit > 32)
	return 0;
    if (slice > 32)
	return 0;
    min = unit * 8 + 65536 * slice + part;
    sprintf(buf, "%s/r%s", path, c1->name);
    unlink(buf);
    if (mknod(buf, S_IFCHR|0640, makedev(cmaj,min)) == -1) {
	msgDebug("mknod of %s returned failure status!\n", buf);
	return 0;
    }
    if (*buf2) {
	sprintf(buf, "%s/r%s", path, buf2);
	unlink(buf);
	if (mknod(buf, S_IFCHR|0640, makedev(cmaj,min)) == -1) {
	    msgDebug("mknod of %s returned failure status!\n", buf);
	    return 0;
	}
    }
    sprintf(buf, "%s/%s", path, c1->name);
    unlink(buf);
    if (mknod(buf, S_IFBLK|0640, makedev(bmaj,min)) == -1) {
	msgDebug("mknod of %s returned failure status!\n", buf);
	return 0;
    }
    return 1;
}

int
MakeDevChunk(struct chunk *c1, const char *path)
{
    int i;

    i = MakeDev(c1, path);
    if (c1->next)
    	MakeDevChunk(c1->next, path);
    if (c1->part)
    	MakeDevChunk(c1->part, path);
    return i;
}

int
MakeDevDisk(struct disk *d, const char *path)
{
    return MakeDevChunk(d->chunks, path);
}
