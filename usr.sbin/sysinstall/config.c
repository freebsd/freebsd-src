/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: config.c,v 1.5 1995/05/24 18:52:47 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer, 
 *    verbatim and that no modifications are made prior to this 
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <sys/disklabel.h>

static Chunk *chunk_list[MAX_CHUNKS];
static int nchunks;

/* arg to sort */
static int
chunk_compare(const void *p1, const void *p2)
{
    Chunk *c1, *c2;

    c1 = (Chunk *)p1;
    c2 = (Chunk *)p2;
    if (!c1->private && !c2->private)
	return 0;
    else if (c1->private && !c2->private)
	return -1;
    else if (!c1->private && c2->private)
	return 1;
    else
	return strcmp(((PartInfo *)c1->private)->mountpoint, ((PartInfo *)c2->private)->mountpoint);
}

static char *
nameof(Chunk *c1)
{
    static char rootname[64];

    /* Our boot blocks can't deal with root partitions on slices - need the compatbility name */
    if (c1->type == part && c1->flags & CHUNK_IS_ROOT) {
	sprintf(rootname, "%sa", c1->disk->name);
	return rootname;
    }
    else
	return c1->name;
}

static char *
mount_point(Chunk *c1)
{
    if (c1->type == PART_FAT || (c1->type == part && c1->subtype != FS_SWAP))
	return ((PartInfo *)c1->private)->mountpoint;
    else if (c1->type == part && c1->subtype == FS_SWAP)
	return "none";
    return "/bogus";
}

static char *
fstype(Chunk *c1)
{
    if (c1->type == PART_FAT)
	return "msdos";
    else if (c1->type == part) {
	if (c1->subtype != FS_SWAP)
	    return "ufs";
	else
	    return "swap";
    }
    return "bogfs";
}

static char *
fstype_short(Chunk *c1)
{
    if (c1->type == part) {
	if (c1->subtype != FS_SWAP)
	    return "rw";
	else
	    return "sw";
    }
    return "bog";
}

static int
seq_num(Chunk *c1)
{
    if (c1->type == part) {
	if (c1->subtype != FS_SWAP)
	    return 1;
	else
	    return 0;
    }
    return -1;
}

void
configFstab(void)
{
    Device **devs;
    Disk *disk;
    FILE *fstab;
    int i, cnt;
    Chunk *c1, *c2;

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    if (!devs) {
	msgConfirm("No disks found!");
	return;
    }

    /* Record all the chunks */
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;
	disk = (Disk *)devs[i]->private;
	if (!disk->chunks)
	    msgFatal("No chunk list found for %s!", disk->name);
	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part)
			chunk_list[nchunks++] = c2;
		}
	    }
	    else if (c1->type == fat)
		chunk_list[nchunks++] = c1;
	}
    }

    /* Sort them puppies! */
    qsort(chunk_list, nchunks, sizeof(Chunk *), chunk_compare);

    fstab = fopen("/etc/fstab", "w");
    if (!fstab) {
	msgConfirm("Unable to create a new /etc/fstab file!\nManual intervention will be required.");
	return;
    }

    /* Go for the burn */
    msgNotify("Generating /etc/fstab file");
    for (i = 0; i < nchunks; i++) {
	fprintf(fstab, "/dev/%s\t\t\t%s\t\t%s %s %d %d\n", nameof(chunk_list[i]), mount_point(chunk_list[i]),
		fstype(chunk_list[i]), fstype_short(chunk_list[i]), seq_num(chunk_list[i]),
		seq_num(chunk_list[i]));
    }

    Mkdir("/proc", NULL);
    fprintf(fstab, "proc\t\t\t/proc\t\tprocfs rw 0 0\n");

    /* Now look for the CDROMs */
    devs = deviceFind(NULL, DEVICE_TYPE_CDROM);
    cnt = deviceCount(devs);

    /* Write the first one out as /cdrom */
    if (cnt) {
	Mkdir("/cdrom", NULL);
	fprintf(fstab, "/dev/%s\t\t\t/cdrom\t\tcd9660 ro 0 0\n", devs[0]->name);
    }

    /* Write the others out as /cdrom<n> */
    for (i = 1; i < cnt; i++) {
	char cdname[10];

	sprintf(cdname, "/cdrom%d", i);
	Mkdir(cdname, NULL);
	fprintf(fstab, "/dev/%s\t\t\t%s\t\tcd9660 ro 0 0\n", devs[i]->name, cdname);
    }
    fclose(fstab);
}

/*
 * This sucks in /etc/sysconfig, substitutes anything needing substitution, then
 * writes it all back out.  It's pretty gross and needs re-writing at some point.
 */
void
configSysconfig(void)
{
#if 0
    FILE *fp;
    char *lines[5001];	/* Some big number we're not likely to ever reach - I'm being really lazy here, I know */
    char line[256];
    Variable *v;
    int i, nlines = 0;

    fp = fopen("/etc/sysconfig", "r");
    if (!fp) {
	msgConfirm("Unable to open /etc/sysconfig file!  Things may work\nrather strangely as a result of this.");
	return;
    }
    for (i = 0; i < 5000; i++) {
	if (!fgets(line, 256, fp))
	    break;
	lines[nlines++] = strdup(line);
    }
    lines[nlines] = NULL;
    for (v = VarHead; v; v = v->next) {
	for (i = 0; i < nlines; i++) {
	}
    }
#endif
}

int
configSaverTimeout(char *str)
{
    char *val;

    val = msgGetInput("60", "Enter time-out period in seconds for screen saver");
    if (val)
	variable_set2("blanktime", val);
    return 0;
}

void
configResolv(void)
{
    static Boolean alreadyDone = FALSE;
    FILE *fp;

    if (alreadyDone)
	return;

    if (!getenv(VAR_DOMAINNAME) || !getenv(VAR_NAMESERVER)) {
	msgConfirm("Warning: You haven't set a domain name or nameserver.  You will need\nto configure your /etc/resolv.conf file manually to fully use network services.");
	return;
    }
    Mkdir("/etc", NULL);
    fp = fopen("/etc/resolv.conf", "w");
    if (!fp) {
	msgConfirm("Unable to open /etc/resolv.conf!  You will need to do this manually.");
	return;
    }
    fprintf(fp, "domain\t%s\n", getenv(VAR_DOMAINNAME));
    fprintf(fp, "nameserver\t%s\n", getenv(VAR_NAMESERVER));
    msgNotify("Wrote /etc/resolv.conf");
    fclose(fp);
    alreadyDone = TRUE;
}

int
configPackages(char *str)
{
    if (!mediaDevice || mediaDevice->type != DEVICE_TYPE_CDROM) {
	if (getpid() == 1) {
	    if (!mediaSetCDROM(NULL))
		return 0;
	    else
		vsystem("pkg_manage /cdrom");
	}
    }
    vsystem("pkg_manage");
    return 0;
}

int
configPorts(char *str)
{
    return 0;
}
