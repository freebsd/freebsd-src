/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: label.c,v 1.33 1995/09/18 16:52:28 peter Exp $
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

/* All the chunks currently displayed on the screen */
static struct {
    struct chunk *c;
    PartType type;
} label_chunk_info[MAX_CHUNKS + 1];
static int here;

static int diskLabel(char *str);
static int scriptLabel(char *str);

static int
labelHook(char *str)
{
    Device **devs = NULL;

    /* Clip garbage off the ends */
    string_prune(str);
    str = string_skipwhite(str);
    /* Try and open all the disks */
    while (str) {
	char *cp;

	cp = index(str, '\n');
	if (cp)
	   *cp++ = 0;
	if (!*str) {
	    beep();
	    return 0;
	}
	devs = deviceFind(str, DEVICE_TYPE_DISK);
	if (!devs) {
	    dialog_clear();
	    msgConfirm("Unable to find disk %s!", str);
	    return 0;
	}
	else if (devs[1]) {
	    dialog_clear();
	    msgConfirm("Bizarre multiple match for %s!", str);
	}
	devs[0]->enabled = TRUE;
	str = cp;
    }
    return devs ? 1 : 0;
}

int
diskLabelEditor(char *str)
{
    Device **devs;
    DMenu *menu;
    int i, cnt;
    char *cp;

    cp = variable_get(VAR_DISK);
    devs = deviceFind(cp, DEVICE_TYPE_DISK);
    cnt = deviceCount(devs);
    if (!cnt) {
	dialog_clear();
	msgConfirm("No disks found!  Please verify that your disk controller is being\n"
		   "properly probed at boot time.  See the Hardware Guide on the\n"
		   "Documentation menu for clues on diagnosing this type of problem.");
	return RET_FAIL;
    }
    else if (cnt == 1 || variable_get(DISK_SELECTED)) {
	devs[0]->enabled = TRUE;
	if (str && !strcmp(str, "script"))
	    i = scriptLabel(str);
	else
	    i = diskLabel(str);
    }
    else {
	menu = deviceCreateMenu(&MenuDiskDevices, DEVICE_TYPE_DISK, labelHook);
	if (!menu) {
	    dialog_clear();
	    msgConfirm("No devices suitable for installation found!\n\n"
		       "Please verify that your disk controller (and attached drives)\n"
		       "were detected properly.  This can be done by pressing the\n"
		       "[Scroll Lock] key and using the Arrow keys to move back to\n"
		       "the boot messages.  Press [Scroll Lock] again to return.");
	    i = RET_FAIL;
	}
	else {
	    if (!dmenuOpenSimple(menu))
		i = RET_FAIL;
	    else
		i = diskLabel(str);
	    free(menu);
	}
    }
    return i;
}

int
diskLabelCommit(char *str)
{
    char *cp;
    int i;

    /* Already done? */
    if ((cp = variable_get(DISK_LABELLED)) && strcmp(cp, "yes"))
	i = RET_SUCCESS;
    else if (!cp) {
	dialog_clear();
	msgConfirm("You must assign disk labels before this option can be used.");
	i = RET_FAIL;
    }
    /* The routine will guard against redundant writes, just as this one does */
    else if (diskPartitionWrite(str) != RET_SUCCESS)
	i = RET_FAIL;
    else if (installFilesystems(str) != RET_SUCCESS)
	i = RET_FAIL;
    else {
	msgInfo("All filesystem information written successfully.");
	variable_set2(DISK_LABELLED, "written");
	i = RET_SUCCESS;
    }
    return i;
}

/* See if we're already using a desired partition name */
static Boolean
check_conflict(char *name)
{
    int i;

    for (i = 0; label_chunk_info[i].c; i++)
	if (label_chunk_info[i].type == PART_FILESYSTEM && label_chunk_info[i].c->private
	    && !strcmp(((PartInfo *)label_chunk_info[i].c->private)->mountpoint, name))
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
}

/* A new partition entry */
static PartInfo *
new_part(char *mpoint, Boolean newfs, u_long size)
{
    PartInfo *ret;
    u_long target, divisor;

    if (!mpoint)
	mpoint = "/change_me";

    ret = (PartInfo *)safe_malloc(sizeof(PartInfo));
    strncpy(ret->mountpoint, mpoint, FILENAME_MAX);
    strcpy(ret->newfs_cmd, "newfs -b 8192 -f 2048");
    ret->newfs = newfs;
    if (!size)
	    return ret;
    for (target = size; target; target--) {
	for (divisor = 4096 ; divisor > 1023; divisor--) {
	    if (!(target % divisor)) {
		sprintf(ret->newfs_cmd + strlen(ret->newfs_cmd), " -u %ld",divisor);
		return ret;
	    }
	}
    }
    return ret;
}

/* Get the mountpoint for a partition and save it away */
static PartInfo *
get_mountpoint(struct chunk *old)
{
    char *val;
    PartInfo *tmp;

    if (old && old->private)
	tmp = old->private;
    else
	tmp = NULL;
    val = msgGetInput(tmp ? tmp->mountpoint : NULL, "Please specify a mount point for the partition");
    if (!val || !*val) {
	if (!old)
	    return NULL;
	else {
	    free(old->private);
	    old->private = NULL;
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
	old->private = tmp;
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

    static unsigned char *fs_types[] = {
	"FS",
	"A file system",
	"Swap",
	"A swap partition.",
    };
    i = dialog_menu("Please choose a partition type",
		    "If you want to use this partition for swap space, select Swap.\n"
		    "If you want to put a filesystem on it, choose FS.",
		    -1, -1, 2, 2, fs_types, selection, NULL, NULL);
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

static int
scriptLabel(char *str)
{
    char *cp;
    PartType type;
    PartInfo *p;
    u_long flags = 0;
    int i, status;
    Device **devs;
    Disk *d;

    status = RET_SUCCESS;
    cp = variable_get(VAR_DISK);
    if (!cp) {
	dialog_clear();
	msgConfirm("scriptLabel:  No disk selected - can't label automatically.");
	return RET_FAIL;
    }

    devs = deviceFind(cp, DEVICE_TYPE_DISK);
    if (!devs) {
	dialog_clear();
	msgConfirm("scriptLabel: No disk device %s found!", cp);
	return RET_FAIL;
    }
    d = devs[0]->private;

    record_label_chunks(devs);
    for (i = 0; label_chunk_info[i].c; i++) {
	Chunk *c1 = label_chunk_info[i].c;

	if (label_chunk_info[i].type == PART_SLICE) {
	    if ((cp = variable_get(c1->name)) != NULL) {
		int sz;
		char typ[10], mpoint[50];

		if (sscanf(cp, "%s %d %s", typ, &sz, mpoint) != 3) {
		    dialog_clear();
		    msgConfirm("For slice entry %s, got an invalid detail entry of: %s",  c1->name, cp);
		    status = RET_FAIL;
		    continue;
		}
		else {
		    Chunk *tmp;

		    if (!strcmp(typ, "swap")) {
			type = PART_SWAP;
			strcpy(mpoint, "<swap>");
		    }
		    else {
			type = PART_FILESYSTEM;
			if (!strcmp(mpoint, "/"))
			    flags |= CHUNK_IS_ROOT;
		    }
		    if (!sz)
			sz = space_free(c1);
		    if (sz > space_free(c1)) {
			dialog_clear();
			msgConfirm("Not enough free space to create partition: %s", mpoint);
			status = RET_FAIL;
			continue;
		    }
		    if (!(tmp = Create_Chunk_DWIM(d, c1, sz, part,
						  (type == PART_SWAP) ? FS_SWAP : FS_BSDFFS, flags))) {
			dialog_clear();
			msgConfirm("Unable to create from partition spec: %s. Too big?", cp);
			status = RET_FAIL;
			break;
		    }
		    else {
			tmp->private = new_part(mpoint, TRUE, sz);
			tmp->private_free = safe_free;
			status = RET_SUCCESS;
		    }
		}
	    }
	}
	else {
	    /* Must be something we can set a mountpoint */
	    cp = variable_get(c1->name);
	    if (cp) {
		char mpoint[50], nwfs[8];
		Boolean newfs = FALSE;

		nwfs[0] = '\0';
		if (sscanf(cp, "%s %s", mpoint, nwfs) != 2) {
		    dialog_clear();
		    msgConfirm("For slice entry %s, got an invalid detail entry of: %s", c1->name, cp);
		    status = RET_FAIL;
		    continue;
		}
		newfs = toupper(nwfs[0]) == 'Y' ? TRUE : FALSE;
		if (c1->private) {
		    p = c1->private;
		    p->newfs = newfs;
		    strcpy(p->mountpoint, mpoint);
		}
		else {
		    c1->private = new_part(mpoint, newfs, 0);
		    c1->private_free = safe_free;
		}
		if (!strcmp(mpoint, "/"))
		    c1->flags |= CHUNK_IS_ROOT;
		else
		    c1->flags &= ~CHUNK_IS_ROOT;
	    }
	}
    }
    if (status == RET_SUCCESS)
	variable_set2(DISK_LABELLED, "yes");
    return status;
}

#define MAX_MOUNT_NAME	12

#define PART_PART_COL	0
#define PART_MOUNT_COL	8
#define PART_SIZE_COL	(PART_MOUNT_COL + MAX_MOUNT_NAME + 3)
#define PART_NEWFS_COL	(PART_SIZE_COL + 7)
#define PART_OFF	38

/* How many mounted partitions to display in column before going to next */
#define CHUNK_COLUMN_MAX	5

/* stick this all up on the screen */
static void
print_label_chunks(void)
{
    int i, j, srow, prow, pcol;
    int sz;

    attrset(A_REVERSE);
    mvaddstr(0, 25, "FreeBSD Disklabel Editor");
    clrtobot();
    attrset(A_NORMAL);

    for (i = 0; i < 2; i++) {
	mvaddstr(CHUNK_PART_START_ROW - 2, PART_PART_COL + (i * PART_OFF), "Part");
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_PART_COL + (i * PART_OFF), "----");

	mvaddstr(CHUNK_PART_START_ROW - 2, PART_MOUNT_COL + (i * PART_OFF), "Mount");
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_MOUNT_COL + (i * PART_OFF), "-----");

	mvaddstr(CHUNK_PART_START_ROW - 2, PART_SIZE_COL + (i * PART_OFF) + 2, "Size");
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_SIZE_COL + (i * PART_OFF) + 2, "----");

	mvaddstr(CHUNK_PART_START_ROW - 2, PART_NEWFS_COL + (i * PART_OFF), "Newfs");
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_NEWFS_COL + (i * PART_OFF), "-----");
    }
    srow = CHUNK_SLICE_START_ROW;
    prow = CHUNK_PART_START_ROW;
    pcol = 0;

    for (i = 0; label_chunk_info[i].c; i++) {
	if (i == here)
	    attrset(A_REVERSE);
	/* Is it a slice entry displayed at the top? */
	if (label_chunk_info[i].type == PART_SLICE) {
	    sz = space_free(label_chunk_info[i].c);
	    mvprintw(srow++, 0, "Disk: %s\tPartition name: %s\tFree: %d blocks (%dMB)",
		     label_chunk_info[i].c->disk->name, label_chunk_info[i].c->name, sz, (sz / ONE_MEG));
	}
	/* Otherwise it's a DOS, swap or filesystem entry, at the bottom */
	else {
	    char onestr[PART_OFF], num[10], *mountpoint, *newfs;

	    /*
	     * We copy this into a blank-padded string so that it looks like
	     * a solid bar in reverse-video
	     */
	    memset(onestr, ' ', PART_OFF - 1);
	    onestr[PART_OFF - 1] = '\0';
	    /* Go for two columns */
	    if (prow == (CHUNK_PART_START_ROW + CHUNK_COLUMN_MAX)) {
		pcol = PART_OFF;
		prow = CHUNK_PART_START_ROW;
	    }
	    memcpy(onestr + PART_PART_COL, label_chunk_info[i].c->name, strlen(label_chunk_info[i].c->name));
	    /* If it's a filesystem, display the mountpoint */
	    if (label_chunk_info[i].c->private
		&& (label_chunk_info[i].type == PART_FILESYSTEM || label_chunk_info[i].type == PART_FAT))
	        mountpoint = ((PartInfo *)label_chunk_info[i].c->private)->mountpoint;
	    else
	        mountpoint = "<none>";

	    /* Now display the newfs field */
	    if (label_chunk_info[i].type == PART_FAT)
	        newfs = "DOS";
	    else if (label_chunk_info[i].c->private && label_chunk_info[i].type == PART_FILESYSTEM)
		newfs = ((PartInfo *)label_chunk_info[i].c->private)->newfs ? "UFS Y" : "UFS N";
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
	    mvaddstr(prow, pcol, onestr);
	    ++prow;
	}
	if (i == here)
	    attrset(A_NORMAL);
    }
}

static void
print_command_summary()
{
    mvprintw(17, 0, "The following commands are valid here (upper or lower case):");
    mvprintw(18, 0, "C = Create      D = Delete         M = Mount   W = Write");
    mvprintw(19, 0, "N = Newfs Opts  T = Newfs Toggle   U = Undo    Q = Finish");
    mvprintw(20, 0, "A = Auto Defaults for all!");
    mvprintw(22, 0, "The default target will be displayed in ");

    attrset(A_REVERSE);
    addstr("reverse");
    attrset(A_NORMAL);
    addstr(" video.");
    mvprintw(23, 0, "Use F1 or ? to get more help, arrow keys to move.");
    move(0, 0);
}

static int
diskLabel(char *str)
{
    int sz, key = 0;
    Boolean labeling;
    char *msg = NULL;
    PartInfo *p, *oldp;
    PartType type;
    Device **devs;

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    if (!devs) {
	dialog_clear();
	msgConfirm("No disks found!");
	return RET_FAIL;
    }

    labeling = TRUE;
    keypad(stdscr, TRUE);
    record_label_chunks(devs);

    dialog_clear(); clear();
    while (labeling) {
	clear();
	print_label_chunks();
	print_command_summary();
	if (msg) {
	    attrset(A_REVERSE); mvprintw(23, 0, msg); attrset(A_NORMAL);
	    clrtoeol();
	    beep();
	    msg = NULL;
	}
	refresh();
	key = toupper(getch());
	switch (key) {
	    int i, cnt;

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
		msg = "You can only do this in a master partition (see top of screen)";
		break;
	    }
	    
	    cnt = i = 0;
	    while (label_chunk_info[i].c)
		if (label_chunk_info[i++].type != PART_SLICE)
		    cnt++;
	    if (cnt == (CHUNK_COLUMN_MAX * 2) + 4) {
		dialog_clear();
		msgConfirm("Sorry, I can't fit any more partitions on the screen!  You can get around\n"
			   "this limitation by partitioning your disks individually rather than all\n"
			   "at once.  This will be fixed just as soon as we get a scrolling partition\n"
			   "box written.  Sorry for the inconvenience!");
		break;
	    }
	    
	    sz = space_free(label_chunk_info[here].c);
	    if (sz <= FS_MIN_SIZE) {
		msg = "Not enough space to create an additional FreeBSD partition";
		break;
	    }
	{
	    struct chunk *tmp;
	    int mib[2];
	    int physmem;
	    size_t size, swsize;
	    char *cp;

	    cp = variable_get(VAR_ROOT_SIZE);
	    tmp = Create_Chunk_DWIM(label_chunk_info[here].c->disk,
				    label_chunk_info[here].c,
				    (cp ? atoi(cp) : 32) * ONE_MEG, part, FS_BSDFFS, 
				    CHUNK_IS_ROOT);
	    
	    if (!tmp) {
		dialog_clear();
		msgConfirm("Unable to create the root partition. Too big?");
		break;
	    }
	    tmp->private = new_part("/", TRUE, tmp->size);
	    tmp->private_free = safe_free;
	    record_label_chunks(devs);
	    
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
	    tmp = Create_Chunk_DWIM(label_chunk_info[here].c->disk,
				    label_chunk_info[here].c,
				    swsize,
				    part, FS_SWAP, 0);
	    if (!tmp) {
		dialog_clear();
		msgConfirm("Unable to create the swap partition. Too big?");
		break;
	    }
	    
	    tmp->private = 0;
	    tmp->private_free = safe_free;
	    record_label_chunks(devs);
	    
	    cp = variable_get(VAR_VAR_SIZE);
	    tmp = Create_Chunk_DWIM(label_chunk_info[here].c->disk,
				    label_chunk_info[here].c,
				    (cp ? atoi(cp) : VAR_MIN_SIZE) * ONE_MEG, part, FS_BSDFFS, 0);
	    if (!tmp) {
		dialog_clear();
		msgConfirm("Less than %dMB free for /var - you will need to\n"
			   "partition your disk manually with a custom install!", (cp ? atoi(cp) : VAR_MIN_SIZE));
		break;
	    }
	    tmp->private = new_part("/var", TRUE, tmp->size);
	    tmp->private_free = safe_free;
	    record_label_chunks(devs);
	    
	    cp = variable_get(VAR_USR_SIZE);
	    if (cp)
		sz = atoi(cp) * ONE_MEG;
	    else
		sz = space_free(label_chunk_info[here].c);
	    if (!sz || sz < (USR_MIN_SIZE * ONE_MEG)) {
		dialog_clear();
		msgConfirm("Less than %dMB free for /usr - you will need to\n"
			   "partition your disk manually with a custom install!", USR_MIN_SIZE);
		break;
	    }

	    tmp = Create_Chunk_DWIM(label_chunk_info[here].c->disk,
				    label_chunk_info[here].c,
				    sz, part, FS_BSDFFS, 0);
	    if (!tmp) {
		dialog_clear();
		msgConfirm("Unable to create the /usr partition.  Not enough space?\n"
			   "You will need to partition your disk manually with a custom install!");
		break;
	    }
	    /* At this point, we're reasonably "labelled" */
	    variable_set2(DISK_LABELLED, "yes");
	    tmp->private = new_part("/usr", TRUE, tmp->size);
	    tmp->private_free = safe_free;
	    record_label_chunks(devs);
	}
	    break;
	    
	case 'C':
	    if (label_chunk_info[here].type != PART_SLICE) {
		msg = "You can only do this in a master partition (see top of screen)";
		break;
	    }
	    else {
		int i, cnt;

		cnt = i = 0;
		while (label_chunk_info[i].c)
		    if (label_chunk_info[i++].type != PART_SLICE)
			cnt++;
		if (cnt == (CHUNK_COLUMN_MAX * 2)) {
		    dialog_clear();
		    msgConfirm("Sorry, I can't fit any more partitions on the screen!  You can get around\n"
			       "this limitation by partitioning your disks individually rather than all\n"
			       "at once.  This will be fixed just as soon as we get a scrolling partition\n"
			       "box written.  Sorry for the inconvenience!");
		    break;
		}
	    }
	    sz = space_free(label_chunk_info[here].c);
	    if (sz <= FS_MIN_SIZE) {
		msg = "Not enough space to create an additional FreeBSD partition";
		break;
	    }
	    {
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
		    tmp->private = new_part(p->mountpoint, p->newfs, tmp->size);
		    tmp->private_free = safe_free;
		    safe_free(p);
		} else {
		    tmp->private = p;
		}
		tmp->private_free = safe_free;
		variable_set2(DISK_LABELLED, "yes");
		record_label_chunks(devs);
	    }
	    break;

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
		oldp = label_chunk_info[here].c->private;
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
	    if (label_chunk_info[here].c->private &&
		((PartInfo *)label_chunk_info[here].c->private)->newfs)
		getNewfsCmd(label_chunk_info[here].c->private);
	    else
		msg = MSG_NOT_APPLICABLE;
	    break;

	case 'T':	/* Toggle newfs state */
	    if (label_chunk_info[here].type == PART_FILESYSTEM) {
		    PartInfo *pi = ((PartInfo *)label_chunk_info[here].c->private);
		    label_chunk_info[here].c->private = new_part(pi ? pi->mountpoint : NULL, pi ? !pi->newfs : TRUE, label_chunk_info[here].c->size);
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
	    for (i = 0; devs[i]; i++) {
		extern void diskPartition(Device *dev, Disk *d);
		Disk *d;

		if (!devs[i]->enabled)
		    continue;
		else if ((d = Open_Disk(devs[i]->name)) != NULL) {
		    Free_Disk(devs[i]->private);
		    devs[i]->private = d;
		    diskPartition(devs[i], d);
		}
	    }
	    variable_unset(DISK_LABELLED);
	    record_label_chunks(devs);
	    break;

	case 'W':
	    if (!msgYesNo("Are you SURE that you wish to make and mount all filesystems\n"
			  "at this time?  You also have the option of doing it later in\n"
			  "one final 'commit' operation, and if you're at all unsure as\n"
			  "to which option to chose, then PLEASE CHOSE NO!  This option\n"
			  "is DANGEROUS if you're not EXACTLY sure what you are doing!")) {
		variable_set2(DISK_LABELLED, "yes");
		clear();
		diskLabelCommit(NULL);
	    }
	    break;

	case '|':
	    if (!msgYesNo("Are you sure you want to go into Wizard mode?\n\n"
			  "This is an entirely undocumented feature which you are not\n"
			  "expected to understand!")) {
		int i;
		Device **devs;

		dialog_clear();
		end_dialog();
		DialogActive = FALSE;
		devs = deviceFind(NULL, DEVICE_TYPE_DISK);
		if (!devs) {
		    dialog_clear();
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
	    msg = "Type F1 or ? for help";
	    break;
	}
    }
    dialog_clear();
    return RET_SUCCESS;
}
