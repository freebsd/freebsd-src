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
#include <sys/diskmbr.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include "libdisk.h"

static void msgDebug(char *, ...) __printflike(1, 2);

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
	DebugFD = open(_PATH_DEV"ttyv1", O_RDWR);
    dbg = (char *)alloca(FILENAME_MAX);
    strcpy(dbg, "DEBUG: ");
    va_start(args, fmt);
    vsnprintf((char *)(dbg + strlen(dbg)), FILENAME_MAX, fmt, args);
    va_end(args);
    write(DebugFD, dbg, strlen(dbg));
}

int
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

int
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
    int i;
#ifdef __i386__
    struct chunk *c3;
    int j;
#endif
    
    c1 = d->chunks;
    for(i=1,c2 = c1->part; c2 ; c2 = c2->next) {
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
#ifdef PC98
Create_Chunk(struct disk *d, u_long offset, u_long size, chunk_e type, int subtype, u_long flags, const char *sname)
#else
Create_Chunk(struct disk *d, u_long offset, u_long size, chunk_e type, int subtype, u_long flags)
#endif
{
    int i;
    u_long l;
    
    if(!(flags & CHUNK_FORCE_ALL))
    {
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
    
#ifdef PC98
    i = Add_Chunk(d, offset, size, "X", type, subtype, flags, sname);
#else
    i = Add_Chunk(d, offset, size, "X", type, subtype, flags);
#endif
    Fixup_Names(d);
    return i;
}

struct chunk *
Create_Chunk_DWIM(struct disk *d, struct chunk *parent , u_long size, chunk_e type, int subtype, u_long flags)
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
#ifdef PC98
    i = Add_Chunk(d, offset, size, "X", type, subtype, flags, "-");
#else
    i = Add_Chunk(d, offset, size, "X", type, subtype, flags);
#endif
    if (i)
	return 0;
    Fixup_Names(d);
    for (c1=parent->part; c1; c1 = c1->next)
	if (c1->offset == offset)
	    return c1;
    /* barfout(1, "Serious internal trouble"); */
    return 0;
}

int
MakeDev(struct chunk *c1, const char *path)
{
    char *p = c1->name;
    u_long cmaj, min, unit, part, slice;
    char buf[BUFSIZ], buf2[BUFSIZ];
    struct group *grp;
    struct passwd *pwd;
    struct statfs fs;
    uid_t owner;
    gid_t group;

    *buf2 = '\0';
    if (isDebug())
	msgDebug("MakeDev: Called with %s on path %s\n", p, path);
    if (!strcmp(p, "X"))
	return 0;
    if (statfs(path, &fs) != 0) {
#ifdef DEBUG
	warn("statfs(%s) failed\n", path);
#endif
	return 0;
    }
    if (strcmp(fs.f_fstypename, "devfs") == 0) {
	if (isDebug())
	    msgDebug("MakeDev: No need to mknod(2) with DEVFS.\n");
	return 1;
    }

    if (!strncmp(p, "ad", 2))
	cmaj = 116, p += 2;
#ifdef PC98
    else if (!strncmp(p, "wd", 2))
	cmaj = 3, p += 2;
#endif
    else if (!strncmp(p, "wfd", 3))
	cmaj = 87, p += 3;
    else if (!strncmp(p, "afd", 3))
	cmaj = 118, p += 3;
    else if (!strncmp(p, "fla", 3))
	cmaj = 102, p += 3;
    else if (!strncmp(p, "idad", 4))
	cmaj = 109, p += 4;
    else if (!strncmp(p, "mlxd", 4))
	cmaj = 131, p += 4;
    else if (!strncmp(p, "amrd", 4))
	cmaj = 133, p += 4;
    else if (!strncmp(p, "twed", 4))
	cmaj = 147, p += 4;
    else if (!strncmp(p, "aacd", 4))
	cmaj = 151, p += 4;
    else if (!strncmp(p, "ar", 2))	/* ATA RAID */
	cmaj = 157, p += 2;
    else if (!strncmp(p, "da", 2))	/* CAM support */
	cmaj = 13, p += 2;
    else {
	msgDebug("MakeDev: Unknown major/minor for devtype %s\n", p);
	return 0;
    }
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
#ifndef __alpha__
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
#else
    slice = 0;
#endif
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
	msgDebug("MakeDev: Unit %lu, Slice %lu, Part %lu\n", unit, slice, part);
    if (unit > 32)
	return 0;
    if (slice > 32)
	return 0;
    if ((pwd = getpwnam("root")) == NULL) {
	if (isDebug())
	    msgDebug("MakeDev: Unable to lookup user \"root\", using 0.\n");
	owner = 0;
    } else {
	owner = pwd->pw_uid;
    }
    if ((grp = getgrnam("operator")) == NULL) {
	if (isDebug())
	    msgDebug("MakeDev: Unable to lookup group \"operator\", using 5.\n");
	group = 5;
    } else {
	group = grp->gr_gid;
    }
    min = unit * 8 + 65536 * slice + part;
    sprintf(buf, "%s/r%s", path, c1->name);
    unlink(buf);
    if (mknod(buf, S_IFCHR|0640, makedev(cmaj,min)) == -1) {
	msgDebug("mknod of %s returned failure status!\n", buf);
	return 0;
    }
    if (chown(buf, owner, group) == -1) {
	msgDebug("chown of %s returned failure status!\n", buf);
	return 0;
    }
    if (*buf2) {
	sprintf(buf, "%s/r%s", path, buf2);
	unlink(buf);
	if (mknod(buf, S_IFCHR|0640, makedev(cmaj,min)) == -1) {
	    msgDebug("mknod of %s returned failure status!\n", buf);
	    return 0;
	}
	if (chown(buf, owner, group) == -1) {
	    msgDebug("chown of %s returned failure status!\n", buf);
	    return 0;
	}
    }
    sprintf(buf, "%s/%s", path, c1->name);
    unlink(buf);
    if (mknod(buf, S_IFCHR|0640, makedev(cmaj,min)) == -1) {
	msgDebug("mknod of %s returned failure status!\n", buf);
	return 0;
    }
    if (chown(buf, owner, group) == -1) {
	msgDebug("chown of %s returned failure status!\n", buf);
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
