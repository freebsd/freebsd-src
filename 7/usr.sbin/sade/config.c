/*
 * $FreeBSD$
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

#include "sade.h"
#include <sys/disklabel.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>
#include <libdisk.h>
#include <time.h>
#include <kenv.h>

static Chunk *chunk_list[MAX_CHUNKS];
static int nchunks;
static int rootdev_is_od;

/* arg to sort */
static int
chunk_compare(Chunk *c1, Chunk *c2)
{
    if (!c1 && !c2)
	return 0;
    else if (!c1 && c2)
	return 1;
    else if (c1 && !c2)
	return -1;
    else if (!c1->private_data && !c2->private_data)
	return 0;
    else if (c1->private_data && !c2->private_data)
	return 1;
    else if (!c1->private_data && c2->private_data)
	return -1;
    else
	return strcmp(((PartInfo *)(c1->private_data))->mountpoint, ((PartInfo *)(c2->private_data))->mountpoint);
}

static void
chunk_sort(void)
{
    int i, j;

    for (i = 0; i < nchunks; i++) {
	for (j = 0; j < nchunks; j++) {
	    if (chunk_compare(chunk_list[j], chunk_list[j + 1]) > 0) {
		Chunk *tmp = chunk_list[j];

		chunk_list[j] = chunk_list[j + 1];
		chunk_list[j + 1] = tmp;
	    }
	}
    }
}

static void
check_rootdev(Chunk **list, int n)
{
	int i;
	Chunk *c;

	rootdev_is_od = 0;
	for (i = 0; i < n; i++) {
		c = *list++;
		if (c->type == part && (c->flags & CHUNK_IS_ROOT)
		    && strncmp(c->disk->name, "od", 2) == 0)
			rootdev_is_od = 1;
	}
}

static char *
name_of(Chunk *c1)
{
    return c1->name;
}

static char *
mount_point(Chunk *c1)
{
    if (c1->type == part && c1->subtype == FS_SWAP)
	return "none";
    else if (c1->type == part || c1->type == fat || c1->type == efi)
	return ((PartInfo *)c1->private_data)->mountpoint;
    return "/bogus";
}

static char *
fstype(Chunk *c1)
{
    if (c1->type == fat || c1->type == efi)
	return "msdosfs";
    else if (c1->type == part) {
	if (c1->subtype != FS_SWAP)
	    return "ufs";
	else
	    return "swap";
    }
    return "bogus";
}

static char *
fstype_short(Chunk *c1)
{
    if (c1->type == part) {
	if (c1->subtype != FS_SWAP) {
	    if (rootdev_is_od == 0 && strncmp(c1->name, "od", 2) == 0)
		return "rw,noauto";
	    else
		return "rw";
	}
	else
	    return "sw";
    }
    else if (c1->type == fat) {
	if (strncmp(c1->name, "od", 2) == 0)
	    return "ro,noauto";
	else
	    return "ro";
    }
    else if (c1->type == efi)
	return "rw";

    return "bog";
}

static int
seq_num(Chunk *c1)
{
    if (c1->type == part && c1->subtype != FS_SWAP) {
	if (rootdev_is_od == 0 && strncmp(c1->name, "od", 2) == 0)
	    return 0;
	else if (c1->flags & CHUNK_IS_ROOT)
	    return 1;
	else
	    return 2;
    }
    return 0;
}

int
configFstab(dialogMenuItem *self)
{
    Device **devs;
    Disk *disk;
    FILE *fstab;
    int i;
    Chunk *c1, *c2;

	if (file_readable("/etc/fstab"))
	    return DITEM_SUCCESS;
	else {
	    msgConfirm("Attempting to rebuild your /etc/fstab file.  Warning: If you had\n"
		       "any CD devices in use before running %s then they may NOT\n"
		       "be found by this run!", ProgName);
	}

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    if (!devs) {
	msgConfirm("No disks found!");
	return DITEM_FAILURE;
    }

    /* Record all the chunks */
    nchunks = 0;
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;
	disk = (Disk *)devs[i]->private;
	if (!disk->chunks)
	    msgFatal("No chunk list found for %s!", disk->name);
	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
#ifdef __powerpc__
	    if (c1->type == apple) {
#else
	    if (c1->type == freebsd) {
#endif
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part && (c2->subtype == FS_SWAP || c2->private_data))
			chunk_list[nchunks++] = c2;
		}
	    }
	    else if (((c1->type == fat || c1->type == efi || c1->type == part) &&
		    c1->private_data) || (c1->type == part && c1->subtype == FS_SWAP))
		chunk_list[nchunks++] = c1;
	}
    }
    chunk_list[nchunks] = 0;
    chunk_sort();
    
    fstab = fopen("/etc/fstab", "w");
    if (!fstab) {
	msgConfirm("Unable to create a new /etc/fstab file!  Manual intervention\n"
		   "will be required.");
	return DITEM_FAILURE;
    }
    
    check_rootdev(chunk_list, nchunks);
    
    /* Go for the burn */
    msgDebug("Generating /etc/fstab file\n");
    fprintf(fstab, "# Device\t\tMountpoint\tFStype\tOptions\t\tDump\tPass#\n");
    for (i = 0; i < nchunks; i++)
	fprintf(fstab, "/dev/%s\t\t%s\t\t%s\t%s\t\t%d\t%d\n", name_of(chunk_list[i]), mount_point(chunk_list[i]),
		fstype(chunk_list[i]), fstype_short(chunk_list[i]), seq_num(chunk_list[i]), seq_num(chunk_list[i]));
    
    
    fclose(fstab);
    if (isDebug())
	msgDebug("Wrote out /etc/fstab file\n");
    return DITEM_SUCCESS;
}

#if 0
/* Do the work of sucking in a config file.
 * config is the filename to read in.
 * lines is a fixed (max) sized array of char*
 * returns number of lines read.  line contents
 * are malloc'd and must be freed by the caller.
 */
static int
readConfig(char *config, char **lines, int max)
{
    FILE *fp;
    char line[256];
    int i, nlines;

    fp = fopen(config, "r");
    if (!fp)
	return -1;

    nlines = 0;
    /* Read in the entire file */
    for (i = 0; i < max; i++) {
	if (!fgets(line, sizeof line, fp))
	    break;
	lines[nlines++] = strdup(line);
    }
    fclose(fp);
    if (isDebug())
	msgDebug("readConfig: Read %d lines from %s.\n", nlines, config);
    return nlines;
}
#endif

#define MAX_LINES  2000 /* Some big number we're not likely to ever reach - I'm being really lazy here, I know */

#if 0
static void
readConfigFile(char *config, int marked)
{
    char *lines[MAX_LINES], *cp, *cp2;
    int i, nlines;

    nlines = readConfig(config, lines, MAX_LINES);
    if (nlines == -1)
	return;

    for (i = 0; i < nlines; i++) {
	/* Skip the comments & non-variable settings */
	if (lines[i][0] == '#' || !(cp = index(lines[i], '='))) {
	    free(lines[i]);
	    continue;
	}
	*cp++ = '\0';
	/* Find quotes */
	if ((cp2 = index(cp, '"')) || (cp2 = index(cp, '\047'))) {
	    cp = cp2 + 1;
	    cp2 = index(cp, *cp2);
	}
	/* If valid quotes, use it */
	if (cp2) {
	    *cp2 = '\0';
	    /* If we have a legit value, set it */
	    if (strlen(cp))
		variable_set2(lines[i], cp, marked);
	}
	free(lines[i]);
    }
}

/* Load the environment from rc.conf file(s) */
void
configEnvironmentRC_conf(void)
{
    static struct {
	char *fname;
	int marked;
    } configs[] = {
	{ "/etc/defaults/rc.conf", 0 },
	{ "/etc/rc.conf", 0 },
	{ "/etc/rc.conf.local", 0 },
	{ NULL, 0 },
    };
    int i;

    for (i = 0; configs[i].fname; i++) {
	if (file_readable(configs[i].fname))
	    readConfigFile(configs[i].fname, configs[i].marked);
    }
}
#endif

