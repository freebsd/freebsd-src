/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: label.c,v 1.53 1996/07/14 01:54:39 jkh Exp $
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

#include "sysinstall.h"
#include <ctype.h>
#include <sys/disklabel.h>
#include <sys/param.h>
#include <sys/sysctl.h>

/*
 * Everything to do with editing the contents of disk labels.
 */

/* A nice message we use a lot in the disklabel editor */
#define MSG_NOT_APPLICABLE	"That option is not applicable here"

/* Where to start printing the freebsd slices */
#define CHUNK_SLICE_START_ROW		2
#define CHUNK_PART_START_ROW		11

/* The smallest filesystem we're willing to create */
#define FS_MIN_SIZE			ONE_MEG

/* The smallest root filesystem we're willing to create */
#define ROOT_MIN_SIZE			20

/* The smallest swap partition we want to create by default */
#define SWAP_MIN_SIZE			16

/* The smallest /usr partition we're willing to create by default */
#define USR_MIN_SIZE			80

/* The smallest /var partition we're willing to create by default */
#define VAR_MIN_SIZE			30

/* The bottom-most row we're allowed to scribble on */
#define CHUNK_ROW_MAX		16


/* All the chunks currently displayed on the screen */
static struct {
    struct chunk *c;
    PartType type;
} label_chunk_info[MAX_CHUNKS + 1];
static int here;

static int ChunkPartStartRow;
static WINDOW *ChunkWin;

static int diskLabel(char *str);

int
diskLabelEditor(dialogMenuItem *self)
{
    Device **devs;
    int i, cnt, enabled;
    char *cp;

    cp = variable_get(VAR_DISK);
    devs = deviceFind(cp, DEVICE_TYPE_DISK);
    cnt = deviceCount(devs);
    if (!cnt) {
	msgConfirm("No disks found!  Please verify that your disk controller is being\n"
		   "properly probed at boot time.  See the Hardware Guide on the\n"
		   "Documentation menu for clues on diagnosing this type of problem.");
	return DITEM_FAILURE;
    }
    for (i = 0, enabled = 0; i < cnt; i++) {
	if (devs[i]->enabled)
	    ++enabled;
    }
    if (!enabled) {
	msgConfirm("No disks have been selected.  Please visit the Partition\n"
		   "editor first to specify which disks you wish to operate on.");
	return DITEM_FAILURE;
    }
    i = diskLabel(devs[0]->name);
    if (DITEM_STATUS(i) != DITEM_FAILURE)
	variable_set2(DISK_LABELLED, "yes");
    return i;
}

int
diskLabelCommit(dialogMenuItem *self)
{
    char *cp;
    int i;

    /* Already done? */
    if ((cp = variable_get(DISK_LABELLED)) && strcmp(cp, "yes"))
	i = DITEM_SUCCESS;
    else if (!cp) {
	msgConfirm("You must assign disk labels before this option can be used.");
	i = DITEM_FAILURE;
    }
    /* The routine will guard against redundant writes, just as this one does */
    else if (DITEM_STATUS(diskPartitionWrite(self)) != DITEM_SUCCESS)
	i = DITEM_FAILURE;
    else if (DITEM_STATUS(installFilesystems(self)) != DITEM_SUCCESS)
	i = DITEM_FAILURE;
    else {
	msgInfo("All filesystem information written successfully.");
	variable_set2(DISK_LABELLED, "written");
	i = DITEM_SUCCESS;
    }
    return i;
}

/* See if we're already using a desired partition name */
static Boolean
check_conflict(char *name)
{
    int i;

    for (i = 0; label_chunk_info[i].c; i++)
	if (label_chunk_info[i].type == PART_FILESYSTEM && label_chunk_info[i].c->private_data
	    && !strcmp(((PartInfo *)label_chunk_info[i].c->private_data)->mountpoint, name))
	    return TRUE;
    return FALSE;
}

/* How much space is in this FreeBSD slice? */
static int
space_free(struct chunk *c)
{
    struct chunk *c1;
    int sz = c->size;

    for (c1 = c->part; c1; c1 = c1->next) {
	if (c1->type != unused)
	    sz -= c1->size;
    }
    if (sz < 0)
	msgFatal("Partitions are larger than actual chunk??");
    return sz;
}

/* Snapshot the current situation into the displayed chunks structure */
static void
record_label_chunks(Device **devs)
{
    int i, j, p;
    struct chunk *c1, *c2;
    Disk *d;

    ChunkPartStartRow = CHUNK_SLICE_START_ROW + 3;
    j = p = 0;
    /* First buzz through and pick up the FreeBSD slices */
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;
	d = (Disk *)devs[i]->private;
	if (!d->chunks)
	    msgFatal("No chunk list found for %s!", d->name);

	/* Put the slice entries first */
	for (c1 = d->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		label_chunk_info[j].type = PART_SLICE;
		label_chunk_info[j].c = c1;
		++j;
		++ChunkPartStartRow;
	    }
	}
    }

    /* Now run through again and get the FreeBSD partition entries */
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;
	d = (Disk *)devs[i]->private;
	/* Then buzz through and pick up the partitions */
	for (c1 = d->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part) {
			if (c2->subtype == FS_SWAP)
			    label_chunk_info[j].type = PART_SWAP;
			else
			    label_chunk_info[j].type = PART_FILESYSTEM;
			label_chunk_info[j].c = c2;
			++j;
		    }
		}
	    }
	    else if (c1->type == fat) {
		label_chunk_info[j].type = PART_FAT;
		label_chunk_info[j].c = c1;
		++j;
	    }
	}
    }
    label_chunk_info[j].c = NULL;
    if (here >= j)
	here = j  ? j - 1 : 0;
    if (ChunkWin) {
	wclear(ChunkWin);
	wrefresh(ChunkWin);
    }
    else
	ChunkWin = newwin(CHUNK_ROW_MAX - ChunkPartStartRow, 76, ChunkPartStartRow, 0);
}

/* A new partition entry */
static PartInfo *
new_part(char *mpoint, Boolean newfs, u_long size)
{
    PartInfo *ret;

    if (!mpoint)
	mpoint = "/change_me";

    ret = (PartInfo *)safe_malloc(sizeof(PartInfo));
    strncpy(ret->mountpoint, mpoint, FILENAME_MAX);
    strcpy(ret->newfs_cmd, "newfs -b 8192 -f 1024");
    ret->newfs = newfs;
    if (!size)
	    return ret;
    return ret;
}

/* Get the mountpoint for a partition and save it away */
static PartInfo *
get_mountpoint(struct chunk *old)
{
    char *val;
    PartInfo *tmp;

    if (old && old->private_data)
	tmp = old->private_data;
    else
	tmp = NULL;
    val = msgGetInput(tmp ? tmp->mountpoint : NULL, "Please specify a mount point for the partition");
    if (!val || !*val) {
	if (!old)
	    return NULL;
	else {
	    free(old->private_data);
	    old->private_data = NULL;
	}
	return NULL;
    }

    /* Is it just the same value? */
    if (tmp && !strcmp(tmp->mountpoint, val))
	return NULL;

    /* Did we use it already? */
    if (check_conflict(val)) {
	msgConfirm("You already have a mount point for %s assigned!", val);
	return NULL;
    }

    /* Is it bogus? */
    if (*val != '/') {
	msgConfirm("Mount point must start with a / character");
	return NULL;
    }

    /* Is it going to be mounted on root? */
    if (!strcmp(val, "/")) {
	if (old)
	    old->flags |= CHUNK_IS_ROOT;
    }
    else if (old)
	old->flags &= ~CHUNK_IS_ROOT;

    safe_free(tmp);
    tmp = new_part(val, TRUE, 0);
    if (old) {
	old->private_data = tmp;
	old->private_free = safe_free;
    }
    return tmp;
}

/* Get the type of the new partiton */
static PartType
get_partition_type(void)
{
    char selection[20];
    int i;
    WINDOW *save = savescr();

    static unsigned char *fs_types[] = {
	"FS",
	"A file system",
	"Swap",
	"A swap partition.",
    };
    dialog_clear();
    i = dialog_menu("Please choose a partition type",
		    "If you want to use this partition for swap space, select Swap.\n"
		    "If you want to put a filesystem on it, choose FS.",
		    -1, -1, 2, 2, fs_types, selection, NULL, NULL);
    restorescr(save);
    if (!i) {
	if (!strcmp(selection, "FS"))
	    return PART_FILESYSTEM;
	else if (!strcmp(selection, "Swap"))
	    return PART_SWAP;
    }
    return PART_NONE;
}

/* If the user wants a special newfs command for this, set it */
static void
getNewfsCmd(PartInfo *p)
{
    char *val;

    val = msgGetInput(p->newfs_cmd,
		      "Please enter the newfs command and options you'd like to use in\n"
		      "creating this file system.");
    if (val)
	strncpy(p->newfs_cmd, val, NEWFS_CMD_MAX);
}

#define MAX_MOUNT_NAME	12

#define PART_PART_COL	0
#define PART_MOUNT_COL	8
#define PART_SIZE_COL	(PART_MOUNT_COL + MAX_MOUNT_NAME + 3)
#define PART_NEWFS_COL	(PART_SIZE_COL + 7)
#define PART_OFF	38

/* stick this all up on the screen */
static void
print_label_chunks(void)
{
    int i, j, srow, prow, pcol;
    int sz;

    attrset(A_REVERSE);
    mvaddstr(0, 25, "FreeBSD Disklabel Editor");
    attrset(A_NORMAL);

    for (i = 0; i < 2; i++) {
	mvaddstr(ChunkPartStartRow - 2, PART_PART_COL + (i * PART_OFF), "Part");
	mvaddstr(ChunkPartStartRow - 1, PART_PART_COL + (i * PART_OFF), "----");

	mvaddstr(ChunkPartStartRow - 2, PART_MOUNT_COL + (i * PART_OFF), "Mount");
	mvaddstr(ChunkPartStartRow - 1, PART_MOUNT_COL + (i * PART_OFF), "-----");

	mvaddstr(ChunkPartStartRow - 2, PART_SIZE_COL + (i * PART_OFF) + 2, "Size");
	mvaddstr(ChunkPartStartRow - 1, PART_SIZE_COL + (i * PART_OFF) + 2, "----");

	mvaddstr(ChunkPartStartRow - 2, PART_NEWFS_COL + (i * PART_OFF), "Newfs");
	mvaddstr(ChunkPartStartRow - 1, PART_NEWFS_COL + (i * PART_OFF), "-----");
    }
    srow = CHUNK_SLICE_START_ROW;
    prow = 0;
    pcol = 0;

    for (i = 0; label_chunk_info[i].c; i++) {
	/* Is it a slice entry displayed at the top? */
	if (label_chunk_info[i].type == PART_SLICE) {
	    sz = space_free(label_chunk_info[i].c);
	    if (i == here)
		attrset(ATTR_SELECTED);
	    mvprintw(srow++, 0, "Disk: %s\tPartition name: %s\tFree: %d blocks (%dMB)",
		     label_chunk_info[i].c->disk->name, label_chunk_info[i].c->name, sz, (sz / ONE_MEG));
	    attrset(A_NORMAL);
	    clrtoeol();
	    move(0, 0);
	    refresh();
	}
	/* Otherwise it's a DOS, swap or filesystem entry in the Chunk window */
	else {
	    char onestr[PART_OFF], num[10], *mountpoint, *newfs;

	    /*
	     * We copy this into a blank-padded string so that it looks like
	     * a solid bar in reverse-video
	     */
	    memset(onestr, ' ', PART_OFF - 1);
	    onestr[PART_OFF - 1] = '\0';
	    /* Go for two columns if we've written one full columns worth */
	    if (prow == (CHUNK_ROW_MAX - ChunkPartStartRow)) {
		pcol = PART_OFF;
		prow = 0;
	    }
	    memcpy(onestr + PART_PART_COL, label_chunk_info[i].c->name, strlen(label_chunk_info[i].c->name));
	    /* If it's a filesystem, display the mountpoint */
	    if (label_chunk_info[i].c->private_data
		&& (label_chunk_info[i].type == PART_FILESYSTEM || label_chunk_info[i].type == PART_FAT))
	        mountpoint = ((PartInfo *)label_chunk_info[i].c->private_data)->mountpoint;
	    else
	        mountpoint = "<none>";

	    /* Now display the newfs field */
	    if (label_chunk_info[i].type == PART_FAT)
	        newfs = "DOS";
	    else if (label_chunk_info[i].c->private_data && label_chunk_info[i].type == PART_FILESYSTEM)
		newfs = ((PartInfo *)label_chunk_info[i].c->private_data)->newfs ? "UFS Y" : "UFS N";
	    else if (label_chunk_info[i].type == PART_SWAP)
		newfs = "SWAP";
	    else
		newfs = "*";
	    for (j = 0; j < MAX_MOUNT_NAME && mountpoint[j]; j++)
		onestr[PART_MOUNT_COL + j] = mountpoint[j];
	    snprintf(num, 10, "%4ldMB", label_chunk_info[i].c->size ? label_chunk_info[i].c->size / ONE_MEG : 0);
	    memcpy(onestr + PART_SIZE_COL, num, strlen(num));
	    memcpy(onestr + PART_NEWFS_COL, newfs, strlen(newfs));
	    onestr[PART_NEWFS_COL + strlen(newfs)] = '\0';
	    if (i == here)
		wattrset(ChunkWin, ATTR_SELECTED);
	    mvwaddstr(ChunkWin, prow, pcol, onestr);
	    wattrset(ChunkWin, A_NORMAL);
	    wrefresh(ChunkWin);
	    move(0, 0);
	    ++prow;
	}
    }
}

static void
print_command_summary()
{
    mvprintw(17, 0, "The following commands are valid here (upper or lower case):");
    mvprintw(18, 0, "C = Create      D = Delete         M = Mount");
    if (!RunningAsInit)
	mvprintw(18, 47, "W = Write");
    mvprintw(19, 0, "N = Newfs Opts  T = Newfs Toggle   U = Undo    Q = Finish");
    mvprintw(20, 0, "A = Auto Defaults for all!");
    mvprintw(22, 0, "Use F1 or ? to get more help, arrow keys to select.");
    move(0, 0);
}

static int
diskLabel(char *str)
{
    int sz, key = 0, first_time = 1;
    Boolean labeling;
    char *msg = NULL;
    PartInfo *p, *oldp;
    PartType type;
    Device **devs;

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    if (!devs) {
	msgConfirm("No disks found!");
	return DITEM_FAILURE;
    }

    labeling = TRUE;
    keypad(stdscr, TRUE);
    record_label_chunks(devs);

    dialog_clear(); clear();
    while (labeling) {
	print_label_chunks();
	if (first_time) {
	    print_command_summary();
	    first_time = 0;
	}
	if (msg) {
	    attrset(title_attr); mvprintw(23, 0, msg); attrset(A_NORMAL);
	    clrtoeol();
	    beep();
	    msg = NULL;
	}
	else {
	    move(23, 0);
	    clrtoeol();
	}
	key = toupper(getch());
	switch (key) {
	    int i;
	    static char _msg[40];

	case '\014':	/* ^L */
	    continue;

	case KEY_UP:
	case '-':
	    if (here != 0)
		--here;
	    else
		while (label_chunk_info[here + 1].c)
		    ++here;
	    break;

	case KEY_DOWN:
	case '+':
	case '\r':
	case '\n':
	    if (label_chunk_info[here + 1].c)
		++here;
	    else
		here = 0;
	    break;

	case KEY_HOME:
	    here = 0;
	    break;

	case KEY_END:
	    while (label_chunk_info[here + 1].c)
		++here;
	    break;

	case KEY_F(1):
	case '?':
	    systemDisplayHelp("partition");
	    break;

	case 'A':
	    if (label_chunk_info[here].type != PART_SLICE) {
		msg = "You can only do this in a disk slice (at top of screen)";
		break;
	    }
	    sz = space_free(label_chunk_info[here].c);
	    if (sz <= FS_MIN_SIZE)
		msg = "Not enough free space to create a new partition in the slice";
	    else {
		struct chunk *tmp;
		int mib[2];
		int physmem;
		size_t size, swsize;
		char *cp;
		Chunk *rootdev, *swapdev, *usrdev, *vardev;

		(void)checkLabels(&rootdev, &swapdev, &usrdev, &vardev);
		if (!rootdev) {
		    cp = variable_get(VAR_ROOT_SIZE);
		    tmp = Create_Chunk_DWIM(label_chunk_info[here].c->disk, label_chunk_info[here].c,
					    (cp ? atoi(cp) : 32) * ONE_MEG, part, FS_BSDFFS,  CHUNK_IS_ROOT);
		    if (!tmp) {
			msgConfirm("Unable to create the root partition. Too big?");
			break;
		    }
		    tmp->private_data = new_part("/", TRUE, tmp->size);
		    tmp->private_free = safe_free;
		    record_label_chunks(devs);
		}

		if (!swapdev) {
		    cp = variable_get(VAR_SWAP_SIZE);
		    if (cp)
			swsize = atoi(cp) * ONE_MEG;
		    else {
			mib[0] = CTL_HW;
			mib[1] = HW_PHYSMEM;
			size = sizeof physmem;
			sysctl(mib, 2, &physmem, &size, (void *)0, (size_t)0);
			swsize = 16 * ONE_MEG + (physmem * 2 / 512);
		    }
		    tmp = Create_Chunk_DWIM(label_chunk_info[here].c->disk, label_chunk_info[here].c,
					    swsize, part, FS_SWAP, 0);
		    if (!tmp) {
			msgConfirm("Unable to create the swap partition. Too big?");
			break;
		    }
		    tmp->private_data = 0;
		    tmp->private_free = safe_free;
		    record_label_chunks(devs);
		}

		if (!vardev) {
		    cp = variable_get(VAR_VAR_SIZE);
		    tmp = Create_Chunk_DWIM(label_chunk_info[here].c->disk, label_chunk_info[here].c,
					    (cp ? atoi(cp) : VAR_MIN_SIZE) * ONE_MEG, part, FS_BSDFFS, 0);
		    if (!tmp) {
			msgConfirm("Less than %dMB free for /var - you will need to\n"
				   "partition your disk manually with a custom install!",
				   (cp ? atoi(cp) : VAR_MIN_SIZE));
			break;
		    }
		    tmp->private_data = new_part("/var", TRUE, tmp->size);
		    tmp->private_free = safe_free;
		    record_label_chunks(devs);
		}

		if (!usrdev) {
		    cp = variable_get(VAR_USR_SIZE);
		    if (cp)
			sz = atoi(cp) * ONE_MEG;
		    else
			sz = space_free(label_chunk_info[here].c);
		    if (!sz || sz < (USR_MIN_SIZE * ONE_MEG)) {
			msgConfirm("Less than %dMB free for /usr - you will need to\n"
				   "partition your disk manually with a custom install!", USR_MIN_SIZE);
			break;
		    }

		    tmp = Create_Chunk_DWIM(label_chunk_info[here].c->disk,
					    label_chunk_info[here].c,
					    sz, part, FS_BSDFFS, 0);
		    if (!tmp) {
			msgConfirm("Unable to create the /usr partition.  Not enough space?\n"
				   "You will need to partition your disk manually with a custom install!");
			break;
		    }
		    tmp->private_data = new_part("/usr", TRUE, tmp->size);
		    tmp->private_free = safe_free;
		    record_label_chunks(devs);
		}
		/* At this point, we're reasonably "labelled" */
		variable_set2(DISK_LABELLED, "yes");
	    }
	    break;
	    
	case 'C':
	    if (label_chunk_info[here].type != PART_SLICE) {
		msg = "You can only do this in a master partition (see top of screen)";
		break;
	    }
	    sz = space_free(label_chunk_info[here].c);
	    if (sz <= FS_MIN_SIZE) {
		msg = "Not enough space to create an additional FreeBSD partition";
		break;
	    }
	    else {
		char *val, *cp;
		int size;
		struct chunk *tmp;
		char osize[80];
		u_long flags = 0;

		sprintf(osize, "%d", sz);
		val = msgGetInput(osize, "Please specify the size for new FreeBSD partition in blocks, or\n"
				  "append a trailing `M' for megabytes (e.g. 20M) or `C' for cylinders.\n\n"
				  "Space free is %d blocks (%dMB)", sz, sz / ONE_MEG);
		if (!val || (size = strtol(val, &cp, 0)) <= 0)
		    break;

		if (*cp) {
		    if (toupper(*cp) == 'M')
			size *= ONE_MEG;
		    else if (toupper(*cp) == 'C')
			size *= (label_chunk_info[here].c->disk->bios_hd * label_chunk_info[here].c->disk->bios_sect);
		}
		if (size <= FS_MIN_SIZE) {
		    msgConfirm("The minimum filesystem size is %dMB", FS_MIN_SIZE / ONE_MEG);
		    break;
		}
		type = get_partition_type();
		if (type == PART_NONE)
		    break;

		if (type == PART_FILESYSTEM) {
		    if ((p = get_mountpoint(NULL)) == NULL)
			break;
		    else if (!strcmp(p->mountpoint, "/"))
			flags |= CHUNK_IS_ROOT;
		    else
			flags &= ~CHUNK_IS_ROOT;
		} else
		    p = NULL;

		if ((flags & CHUNK_IS_ROOT)) {
		    if (!(label_chunk_info[here].c->flags & CHUNK_BSD_COMPAT)) {
			msgConfirm("This region cannot be used for your root partition as the\n"
				   "FreeBSD boot code cannot deal with a root partition created\n"
				   "in that location.  Please choose another location or smaller\n"
				   "size for your root partition and try again!");
			break;
		    }
		    if (size < (ROOT_MIN_SIZE * ONE_MEG)) {
			msgConfirm("Warning: This is smaller than the recommended size for a\n"
				   "root partition.  For a variety of reasons, root\n"
				   "partitions should usually be at least %dMB in size", ROOT_MIN_SIZE);
		    }
		}
		tmp = Create_Chunk_DWIM(label_chunk_info[here].c->disk,
					label_chunk_info[here].c,
					size, part,
					(type == PART_SWAP) ? FS_SWAP : FS_BSDFFS,
					flags);
		if (!tmp) {
		    msgConfirm("Unable to create the partition. Too big?");
		    break;
		}
		if ((flags & CHUNK_IS_ROOT) && (tmp->flags & CHUNK_PAST_1024)) {
		    msgConfirm("This region cannot be used for your root partition as it starts\n"
			       "or extends past the 1024'th cylinder mark and is thus a\n"
			       "poor location to boot from.  Please choose another\n"
			       "location (or smaller size) for your root partition and try again!");
		    Delete_Chunk(label_chunk_info[here].c->disk, tmp);
		    break;
		}
		if (type != PART_SWAP) {
		    /* This is needed to tell the newfs -u about the size */
		    tmp->private_data = new_part(p->mountpoint, p->newfs, tmp->size);
		    safe_free(p);
		}
		else
		    tmp->private_data = p;
		tmp->private_free = safe_free;
		variable_set2(DISK_LABELLED, "yes");
		record_label_chunks(devs);
	    }
	    break;

	case '\177':
	case 'D':	/* delete */
	    if (label_chunk_info[here].type == PART_SLICE) {
		msg = MSG_NOT_APPLICABLE;
		break;
	    }
	    else if (label_chunk_info[here].type == PART_FAT) {
		msg = "Use the Disk Partition Editor to delete DOS partitions";
		break;
	    }
	    Delete_Chunk(label_chunk_info[here].c->disk, label_chunk_info[here].c);
	    variable_set2(DISK_LABELLED, "yes");
	    record_label_chunks(devs);
	    break;

	case 'M':	/* mount */
	    switch(label_chunk_info[here].type) {
	    case PART_SLICE:
		msg = MSG_NOT_APPLICABLE;
		break;

	    case PART_SWAP:
		msg = "You don't need to specify a mountpoint for a swap partition.";
		break;

	    case PART_FAT:
	    case PART_FILESYSTEM:
		oldp = label_chunk_info[here].c->private_data;
		p = get_mountpoint(label_chunk_info[here].c);
		if (p) {
		    if (!oldp)
		    	p->newfs = FALSE;
		    if (label_chunk_info[here].type == PART_FAT
			&& (!strcmp(p->mountpoint, "/") || !strcmp(p->mountpoint, "/usr")
			    || !strcmp(p->mountpoint, "/var"))) {
			msgConfirm("%s is an invalid mount point for a DOS partition!", p->mountpoint);
			strcpy(p->mountpoint, "/bogus");
		    }
		}
		variable_set2(DISK_LABELLED, "yes");
		record_label_chunks(devs);
		break;

	    default:
		msgFatal("Bogus partition under cursor???");
		break;
	    }
	    break;

	case 'N':	/* Set newfs options */
	    if (label_chunk_info[here].c->private_data &&
		((PartInfo *)label_chunk_info[here].c->private_data)->newfs)
		getNewfsCmd(label_chunk_info[here].c->private_data);
	    else
		msg = MSG_NOT_APPLICABLE;
	    break;

	case 'T':	/* Toggle newfs state */
	    if (label_chunk_info[here].type == PART_FILESYSTEM) {
		    PartInfo *pi = ((PartInfo *)label_chunk_info[here].c->private_data);
		    label_chunk_info[here].c->private_data =
			new_part(pi ? pi->mountpoint : NULL, pi ? !pi->newfs : TRUE, label_chunk_info[here].c->size);
		    safe_free(pi);
		    label_chunk_info[here].c->private_free = safe_free;
		    variable_set2(DISK_LABELLED, "yes");
		}
	    else
		msg = MSG_NOT_APPLICABLE;
	    break;

	case 'U':
	    clear();
	    if (msgYesNo("Are you SURE you want to Undo everything?"))
		break;
	    variable_unset(DISK_PARTITIONED);
	    variable_unset(DISK_LABELLED);
	    for (i = 0; devs[i]; i++) {
		Disk *d;

		if (!devs[i]->enabled)
		    continue;
		else if ((d = Open_Disk(devs[i]->name)) != NULL) {
		    Free_Disk(devs[i]->private);
		    devs[i]->private = d;
		    diskPartition(devs[i], d);
		}
	    }
	    record_label_chunks(devs);
	    break;

	case 'W':
	    if (!msgYesNo("You also have the option of doing this later in one final 'commit'\n"
			  "operation, and it should also be noted that this option is NOT for\n"
			  "use during new installations but rather for modifying existing ones.\n\n"
			  "Are you absolutely SURE you want to do this now?")) {
		WINDOW *save = savescr();

		variable_set2(DISK_LABELLED, "yes");
		diskLabelCommit(NULL);
		restorescr(save);
	    }
	    break;

	case '|':
	    if (!msgYesNo("Are you sure you want to go into Wizard mode?\n\n"
			  "This is an entirely undocumented feature which you are not\n"
			  "expected to understand!")) {
		int i;
		Device **devs;
		WINDOW *save = savescr();

		dialog_clear();
		end_dialog();
		DialogActive = FALSE;
		devs = deviceFind(NULL, DEVICE_TYPE_DISK);
		if (!devs) {
		    msgConfirm("Can't find any disk devices!");
		    break;
		}
		for (i = 0; devs[i] && ((Disk *)devs[i]->private); i++) {
		    if (devs[i]->enabled)
		    	slice_wizard(((Disk *)devs[i]->private));
		}
		variable_set2(DISK_LABELLED, "yes");
		DialogActive = TRUE;
		dialog_clear();
		restorescr(save);
		record_label_chunks(devs);
	    }
	    else
		msg = "A most prudent choice!";
	    break;

	case 'Q':
	    labeling = FALSE;
	    break;

	default:
	    beep();
	    sprintf(_msg, "Invalid key %d - Type F1 or ? for help", key);
	    msg = _msg;
	    break;
	}
    }
    return DITEM_SUCCESS | DITEM_RESTORE;
}
