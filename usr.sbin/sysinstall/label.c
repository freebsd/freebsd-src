/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
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

#include "sysinstall.h"
#include <ctype.h>
#include <sys/disklabel.h>
#include <sys/param.h>
#include <sys/sysctl.h>

#define AUTO_HOME	0	/* do not create /home automatically */

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

/*
 * Minimum partition sizes
 */
#if defined(__alpha__) || defined(__ia64__) || defined(__sparc64__)
#define ROOT_MIN_SIZE			40
#else
#define ROOT_MIN_SIZE			30
#endif
#define SWAP_MIN_SIZE			32
#define USR_MIN_SIZE			80
#define VAR_MIN_SIZE			20
#define TMP_MIN_SIZE			20
#define HOME_MIN_SIZE			20

/*
 * Swap size limit for auto-partitioning (4G).
 */
#define SWAP_AUTO_LIMIT_SIZE		4096

/*
 * Default partition sizes.  If we do not have sufficient disk space
 * for this configuration we scale things relative to the NOM vs DEFAULT
 * sizes.  If the disk is larger then /home will get any remaining space.
 */
#define ROOT_DEFAULT_SIZE		128
#define USR_DEFAULT_SIZE		3072
#define VAR_DEFAULT_SIZE		256
#define TMP_DEFAULT_SIZE		256
#define HOME_DEFAULT_SIZE		USR_DEFAULT_SIZE

/*
 * Nominal partition sizes.  These are used to scale the default sizes down
 * when we have insufficient disk space.  If this isn't sufficient we scale
 * down using the MIN sizes instead.
 */
#define ROOT_NOMINAL_SIZE		128
#define USR_NOMINAL_SIZE		512
#define VAR_NOMINAL_SIZE		64
#define TMP_NOMINAL_SIZE		64
#define HOME_NOMINAL_SIZE		USR_NOMINAL_SIZE

/* The bottom-most row we're allowed to scribble on */
#define CHUNK_ROW_MAX			16


/* All the chunks currently displayed on the screen */
static struct {
    struct chunk *c;
    PartType type;
} label_chunk_info[MAX_CHUNKS + 1];
static int here;

/*** with this value we try to track the most recently added label ***/
static int label_focus = 0, pslice_focus = 0;

static int diskLabel(Device *dev);
static int diskLabelNonInteractive(Device *dev);
static char *try_auto_label(Device **devs, Device *dev, int perc, int *req);

static int
labelHook(dialogMenuItem *selected)
{
    Device **devs = NULL;

    devs = deviceFind(selected->prompt, DEVICE_TYPE_DISK);
    if (!devs) {
	msgConfirm("Unable to find disk %s!", selected->prompt);
	return DITEM_FAILURE;
    }
    /* Toggle enabled status? */
    if (!devs[0]->enabled) {
	devs[0]->enabled = TRUE;
	diskLabel(devs[0]);
    }
    else
	devs[0]->enabled = FALSE;
    return DITEM_SUCCESS;
}

static int
labelCheck(dialogMenuItem *selected)
{
    Device **devs = NULL;

    devs = deviceFind(selected->prompt, DEVICE_TYPE_DISK);
    if (!devs || devs[0]->enabled == FALSE)
	return FALSE;
    return TRUE;
}

int
diskLabelEditor(dialogMenuItem *self)
{
    DMenu *menu;
    Device **devs;
    int i, cnt;

    i = 0;
    cnt = diskGetSelectCount(&devs);
    if (cnt == -1) {
	msgConfirm("No disks found!  Please verify that your disk controller is being\n"
		   "properly probed at boot time.  See the Hardware Guide on the\n"
		   "Documentation menu for clues on diagnosing this type of problem.");
	return DITEM_FAILURE;
    }
    else if (cnt) {
	/* Some are already selected */
	if (variable_get(VAR_NONINTERACTIVE) &&
	  !variable_get(VAR_DISKINTERACTIVE))
	    i = diskLabelNonInteractive(NULL);
	else
	    i = diskLabel(NULL);
    }
    else {
	/* No disks are selected, fall-back case now */
	cnt = deviceCount(devs);
	if (cnt == 1) {
	    devs[0]->enabled = TRUE;
	    if (variable_get(VAR_NONINTERACTIVE) &&
	      !variable_get(VAR_DISKINTERACTIVE))
		i = diskLabelNonInteractive(devs[0]);
	    else
		i = diskLabel(devs[0]);
	}
	else {
	    menu = deviceCreateMenu(&MenuDiskDevices, DEVICE_TYPE_DISK, labelHook, labelCheck);
	    if (!menu) {
		msgConfirm("No devices suitable for installation found!\n\n"
			   "Please verify that your disk controller (and attached drives)\n"
			   "were detected properly.  This can be done by pressing the\n"
			   "[Scroll Lock] key and using the Arrow keys to move back to\n"
			   "the boot messages.  Press [Scroll Lock] again to return.");
		i = DITEM_FAILURE;
	    }
	    else {
		i = dmenuOpenSimple(menu, FALSE) ? DITEM_SUCCESS : DITEM_FAILURE;
		free(menu);
	    }
	}
    }
    if (DITEM_STATUS(i) != DITEM_FAILURE) {
	if (variable_cmp(DISK_LABELLED, "written"))
	    variable_set2(DISK_LABELLED, "yes", 0);
    }
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
	variable_set2(DISK_LABELLED, "written", 0);
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
	if ((label_chunk_info[i].type == PART_FILESYSTEM || label_chunk_info[i].type == PART_FAT)
	    && label_chunk_info[i].c->private_data
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
record_label_chunks(Device **devs, Device *dev)
{
    int i, j, p;
    struct chunk *c1, *c2;
    Disk *d;

    j = p = 0;
    /* First buzz through and pick up the FreeBSD slices */
    for (i = 0; devs[i]; i++) {
	if ((dev && devs[i] != dev) || !devs[i]->enabled)
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
    if (here >= j) {
	here = j  ? j - 1 : 0;
    }
}

/* A new partition entry */
static PartInfo *
new_part(char *mpoint, Boolean newfs)
{
    PartInfo *ret;

    if (!mpoint)
	mpoint = "/change_me";

    ret = (PartInfo *)safe_malloc(sizeof(PartInfo));
    sstrncpy(ret->mountpoint, mpoint, FILENAME_MAX);
    strcpy(ret->newfs_cmd, "newfs ");
    strcat(ret->newfs_cmd, variable_get(VAR_NEWFS_ARGS));
    ret->newfs = newfs;
    ret->soft = strcmp(mpoint, "/") ? 1 : 0;
    return ret;
}

/* Get the mountpoint for a partition and save it away */
static PartInfo *
get_mountpoint(struct chunk *old)
{
    char *val;
    PartInfo *tmp;
    Boolean newfs;

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

    newfs = TRUE;
    if (tmp) {
	newfs = tmp->newfs;
    	safe_free(tmp);
    }
    val = string_skipwhite(string_prune(val));
    tmp = new_part(val, newfs);
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
    static unsigned char *fs_types[] = {
	"FS",
	"A file system",
	"Swap",
	"A swap partition.",
    };
    WINDOW *w = savescr();

    i = dialog_menu("Please choose a partition type",
		    "If you want to use this partition for swap space, select Swap.\n"
		    "If you want to put a filesystem on it, choose FS.",
		    -1, -1, 2, 2, fs_types, selection, NULL, NULL);
    restorescr(w);
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
	sstrncpy(p->newfs_cmd, val, NEWFS_CMD_MAX);
}

#define MAX_MOUNT_NAME	9

#define PART_PART_COL	0
#define PART_MOUNT_COL	10
#define PART_SIZE_COL	(PART_MOUNT_COL + MAX_MOUNT_NAME + 3)
#define PART_NEWFS_COL	(PART_SIZE_COL + 8)
#define PART_OFF	38

#define TOTAL_AVAIL_LINES       (10)
#define PSLICE_SHOWABLE          (4)


/* stick this all up on the screen */
static void
print_label_chunks(void)
{
    int  i, j, srow, prow, pcol;
    int  sz;
    char clrmsg[80];
    int ChunkPartStartRow;
    WINDOW *ChunkWin;

    /********************************************************/
    /*** These values are for controling screen resources ***/
    /*** Each label line holds up to 2 labels, so beware! ***/
    /*** strategy will be to try to always make sure the  ***/
    /*** highlighted label is in the active display area. ***/
    /********************************************************/
    int  pslice_max, label_max;
    int  pslice_count, label_count, label_focus_found, pslice_focus_found;

    attrset(A_REVERSE);
    mvaddstr(0, 25, "FreeBSD Disklabel Editor");
    attrset(A_NORMAL);

    /*** Count the number of parition slices ***/
    pslice_count = 0;
    for (i = 0; label_chunk_info[i].c ; i++) {
        if (label_chunk_info[i].type == PART_SLICE)
            ++pslice_count;
    }
    pslice_max = pslice_count;
  
    /*** 4 line max for partition slices ***/
    if (pslice_max > PSLICE_SHOWABLE) {
        pslice_max = PSLICE_SHOWABLE;
    }
    ChunkPartStartRow = CHUNK_SLICE_START_ROW + 3 + pslice_max;
    
    /*** View partition slices modulo pslice_max ***/
    label_max = TOTAL_AVAIL_LINES - pslice_max;

    for (i = 0; i < 2; i++) {
	mvaddstr(ChunkPartStartRow - 2, PART_PART_COL + (i * PART_OFF), "Part");
	mvaddstr(ChunkPartStartRow - 1, PART_PART_COL + (i * PART_OFF), "----");

	mvaddstr(ChunkPartStartRow - 2, PART_MOUNT_COL + (i * PART_OFF), "Mount");
	mvaddstr(ChunkPartStartRow - 1, PART_MOUNT_COL + (i * PART_OFF), "-----");

	mvaddstr(ChunkPartStartRow - 2, PART_SIZE_COL + (i * PART_OFF) + 3, "Size");
	mvaddstr(ChunkPartStartRow - 1, PART_SIZE_COL + (i * PART_OFF) + 3, "----");

	mvaddstr(ChunkPartStartRow - 2, PART_NEWFS_COL + (i * PART_OFF), "Newfs");
	mvaddstr(ChunkPartStartRow - 1, PART_NEWFS_COL + (i * PART_OFF), "-----");
    }
    srow = CHUNK_SLICE_START_ROW;
    prow = 0;
    pcol = 0;

    /*** these variables indicate that the focused item is shown currently ***/
    label_focus_found = 0;
    pslice_focus_found = 0;
   
    label_count = 0;
    pslice_count = 0;
    mvprintw(CHUNK_SLICE_START_ROW - 1, 0, "          ");
    mvprintw(CHUNK_SLICE_START_ROW + pslice_max, 0, "          ");

    ChunkWin = newwin(CHUNK_ROW_MAX - ChunkPartStartRow, 76, ChunkPartStartRow, 0);

    wclear(ChunkWin);
    /*** wrefresh(ChunkWin); ***/

    for (i = 0; label_chunk_info[i].c; i++) {
	/* Is it a slice entry displayed at the top? */
	if (label_chunk_info[i].type == PART_SLICE) {
            /*** This causes the new pslice to replace the previous display ***/
            /*** focus must remain on the most recently active pslice       ***/
            if (pslice_count == pslice_max) {
                if (pslice_focus_found) {
                    /*** This is where we can mark the more following ***/
                    attrset(A_BOLD);
                    mvprintw(CHUNK_SLICE_START_ROW + pslice_max, 0, "***MORE***");
                    attrset(A_NORMAL);
                    continue;
                }
                else {
                    /*** this is where we set the more previous ***/
                    attrset(A_BOLD);
                    mvprintw(CHUNK_SLICE_START_ROW - 1, 0, "***MORE***");
                    attrset(A_NORMAL);
                    pslice_count = 0;
                    srow = CHUNK_SLICE_START_ROW;
                }
            }

	    sz = space_free(label_chunk_info[i].c);
	    if (i == here)
		attrset(ATTR_SELECTED);
            if (i == pslice_focus)
                pslice_focus_found = -1;

	    mvprintw(srow++, 0, 
		     "Disk: %s\tPartition name: %s\tFree: %d blocks (%dMB)",
		     label_chunk_info[i].c->disk->name, label_chunk_info[i].c->name, 
		     sz, (sz / ONE_MEG));
	    attrset(A_NORMAL);
	    clrtoeol();
	    move(0, 0);
	    /*** refresh(); ***/
            ++pslice_count;
	}
	/* Otherwise it's a DOS, swap or filesystem entry in the Chunk window */
	else {
	    char onestr[PART_OFF], num[10], *mountpoint, newfs[10];

	    /*
	     * We copy this into a blank-padded string so that it looks like
	     * a solid bar in reverse-video
	     */
	    memset(onestr, ' ', PART_OFF - 1);
	    onestr[PART_OFF - 1] = '\0';

            /*** Track how many labels have been displayed ***/
            if (label_count == ((label_max - 1 ) * 2)) {
                if (label_focus_found) {
                    continue;
                }
                else {
                    label_count = 0;
                    prow = 0;
                    pcol = 0;
                }
            }

	    /* Go for two columns if we've written one full columns worth */
	    /*** if (prow == (CHUNK_ROW_MAX - ChunkPartStartRow)) ***/
            if (label_count == label_max - 1) {
		pcol = PART_OFF;
		prow = 0;
	    }
	    memcpy(onestr + PART_PART_COL, label_chunk_info[i].c->name, strlen(label_chunk_info[i].c->name));
	    /* If it's a filesystem, display the mountpoint */
	    if (label_chunk_info[i].c->private_data
		&& (label_chunk_info[i].type == PART_FILESYSTEM || label_chunk_info[i].type == PART_FAT))
	        mountpoint = ((PartInfo *)label_chunk_info[i].c->private_data)->mountpoint;
	    else if (label_chunk_info[i].type == PART_SWAP)
		mountpoint = "swap";
	    else
	        mountpoint = "<none>";

	    /* Now display the newfs field */
	    if (label_chunk_info[i].type == PART_FAT)
	        strcpy(newfs, "DOS");
	    else if (label_chunk_info[i].c->private_data && label_chunk_info[i].type == PART_FILESYSTEM) {
		strcpy(newfs, "UFS");
		strcat(newfs,
		    ((PartInfo *)label_chunk_info[i].c->private_data)->soft ?
		      "+S" : "  ");
		strcat(newfs,
		    ((PartInfo *)label_chunk_info[i].c->private_data)->newfs ?
		      " Y" : " N");
	    }
	    else if (label_chunk_info[i].type == PART_SWAP)
		strcpy(newfs, "SWAP");
	    else
		strcpy(newfs, "*");
	    for (j = 0; j < MAX_MOUNT_NAME && mountpoint[j]; j++)
		onestr[PART_MOUNT_COL + j] = mountpoint[j];
	    snprintf(num, 10, "%5ldMB", label_chunk_info[i].c->size ? label_chunk_info[i].c->size / ONE_MEG : 0);
	    memcpy(onestr + PART_SIZE_COL, num, strlen(num));
	    memcpy(onestr + PART_NEWFS_COL, newfs, strlen(newfs));
	    onestr[PART_NEWFS_COL + strlen(newfs)] = '\0';
            if (i == label_focus) {
                label_focus_found = -1;
                wattrset(ChunkWin, A_BOLD);
            }
	    if (i == here)
		wattrset(ChunkWin, ATTR_SELECTED);

            /*** lazy man's way of expensively padding this string ***/
            while (strlen(onestr) < 37)
                strcat(onestr, " ");

	    mvwaddstr(ChunkWin, prow, pcol, onestr);
	    wattrset(ChunkWin, A_NORMAL);
	    move(0, 0);
	    ++prow;
            ++label_count;
	}
    }
    
    /*** this will erase all the extra stuff ***/
    memset(clrmsg, ' ', 37);
    clrmsg[37] = '\0';
   
    while (pslice_count < pslice_max) {
        mvprintw(srow++, 0, clrmsg);
        clrtoeol();
        ++pslice_count;
    }
    while (label_count < (2 * (label_max - 1))) {
        mvwaddstr(ChunkWin, prow++, pcol, clrmsg);
	++label_count;
	if (prow == (label_max - 1)) {
	    prow = 0;
	    pcol = PART_OFF;
	}
    }
    refresh();
    wrefresh(ChunkWin);
}

static void
print_command_summary(void)
{
    mvprintw(17, 0, "The following commands are valid here (upper or lower case):");
    mvprintw(18, 0, "C = Create        D = Delete   M = Mount pt.");
    if (!RunningAsInit)
	mvprintw(18, 47, "W = Write");
    mvprintw(19, 0, "N = Newfs Opts    Q = Finish   S = Toggle SoftUpdates");
    mvprintw(20, 0, "T = Toggle Newfs  U = Undo     A = Auto Defaults    R = Delete+Merge");
    mvprintw(22, 0, "Use F1 or ? to get more help, arrow keys to select.");
    move(0, 0);
}

static void
clear_wins(void)
{
    extern void print_label_chunks();
    clear();
    print_label_chunks();
}

static int
diskLabel(Device *dev)
{
    int sz, key = 0;
    Boolean labeling;
    char *msg = NULL;
    PartInfo *p, *oldp;
    PartType type;
    Device **devs;
    WINDOW *w = savescr();

    label_focus = 0;
    pslice_focus = 0;
    here = 0;

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    if (!devs) {
	msgConfirm("No disks found!");
	restorescr(w);
	return DITEM_FAILURE;
    }
    labeling = TRUE;
    keypad(stdscr, TRUE);
    record_label_chunks(devs, dev);

    clear();
    while (labeling) {
	char *cp;
	int rflags = DELCHUNK_NORMAL;

	print_label_chunks();
	print_command_summary();
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

	refresh();
	key = getch();
	switch (toupper(key)) {
	    int i;
	    static char _msg[40];

	case '\014':	/* ^L */
	    clear_wins();
	    break;

	case '\020':	/* ^P */
	case KEY_UP:
	case '-':
	    if (here != 0)
		--here;
	    else
		while (label_chunk_info[here + 1].c)
		    ++here;
	    break;

	case '\016':	/* ^N */
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
	    clear_wins();
	    break;

	case 'A':
	    if (label_chunk_info[here].type != PART_SLICE) {
		msg = "You can only do this in a disk slice (at top of screen)";
		break;
	    }
	    /*
	     * Generate standard partitions automatically.  If we do not
	     * have sufficient space we attempt to scale-down the size
	     * of the partitions within certain bounds.
	     */
	    {
		int perc;
		int req = 0;

		for (perc = 100; perc > 0; perc -= 5) {
		    req = 0;	/* reset for each loop */
		    if ((msg = try_auto_label(devs, dev, perc, &req)) == NULL)
			break;
		}
		if (msg) {
		    if (req) {
			msgConfirm(msg);
			clear_wins();
			msg = NULL;
		    }
		}
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
		char *val;
		int size;
		struct chunk *tmp;
		char osize[80];
		u_long flags = 0;

		sprintf(osize, "%d", sz);
		val = msgGetInput(osize,
				  "Please specify the partition size in blocks or append a trailing G for\n"
				  "gigabytes, M for megabytes, or C for cylinders.\n"
				  "%d blocks (%dMB) are free.",
				  sz, sz / ONE_MEG);
		if (!val || (size = strtol(val, &cp, 0)) <= 0) {
		    clear_wins();
		    break;
		}

		if (*cp) {
		    if (toupper(*cp) == 'M')
			size *= ONE_MEG;
		    else if (toupper(*cp) == 'G')
			size *= ONE_GIG;
		    else if (toupper(*cp) == 'C')
			size *= (label_chunk_info[here].c->disk->bios_hd * label_chunk_info[here].c->disk->bios_sect);
		}
		if (size <= FS_MIN_SIZE) {
		    msgConfirm("The minimum filesystem size is %dMB", FS_MIN_SIZE / ONE_MEG);
		    clear_wins();
		    break;
		}
		type = get_partition_type();
		if (type == PART_NONE) {
		    clear_wins();
		    beep();
		    break;
		}

		if (type == PART_FILESYSTEM) {
		    if ((p = get_mountpoint(NULL)) == NULL) {
			clear_wins();
			beep();
			break;
		    }
		    else if (!strcmp(p->mountpoint, "/"))
			flags |= CHUNK_IS_ROOT;
		    else
			flags &= ~CHUNK_IS_ROOT;
		}
		else
		    p = NULL;

		if ((flags & CHUNK_IS_ROOT) && (size < (ROOT_MIN_SIZE * ONE_MEG))) {
		    msgConfirm("Warning: This is smaller than the recommended size for a\n"
			       "root partition.  For a variety of reasons, root\n"
			       "partitions should usually be at least %dMB in size", ROOT_MIN_SIZE);
		}
		tmp = Create_Chunk_DWIM(label_chunk_info[here].c->disk,
					label_chunk_info[here].c,
					size, part,
					(type == PART_SWAP) ? FS_SWAP : FS_BSDFFS,
					flags);
		if (!tmp) {
		    msgConfirm("Unable to create the partition. Too big?");
		    clear_wins();
		    break;
		}

#ifdef __alpha__
		/*
		 * SRM requires that the root partition is at the
		 * begining of the disk and cannot boot otherwise. 
		 * Warn Alpha users if they are about to shoot themselves in
		 * the foot in this way.
		 *
		 * Since partitions may not start precisely at offset 0 we
		 * check for a "close to 0" instead. :-(
		 */
		if ((flags & CHUNK_IS_ROOT) && (tmp->offset > 1024)) {
		    msgConfirm("Your root partition `a' does not seem to be the first\n"
			       "partition.  The Alpha's firmware can only boot from the\n"
			       "first partition.  So it is unlikely that your current\n"
			       "disk layout will be bootable boot after installation.\n"
			       "\n"
			       "Please allocate the root partition before allocating\n"
			       "any others.\n");
		}
#endif	/* alpha */

		tmp->private_data = p;
		tmp->private_free = safe_free;
		if (variable_cmp(DISK_LABELLED, "written"))
		    variable_set2(DISK_LABELLED, "yes", 0);
		record_label_chunks(devs, dev);
		clear_wins();
                /* This is where we assign focus to new label so it shows. */
                {
                    int i;
		    label_focus = -1;
                    for (i = 0; label_chunk_info[i].c; ++i) {
                    	if (label_chunk_info[i].c == tmp) {
			    label_focus = i;
			    break;
			}
		    }
		    if (label_focus == -1)
                    	label_focus = i - 1;
                }
	    }
	    break;

	case KEY_DC:
	case 'R':	/* recover space (delete w/ recover) */
	    /*
	     * Delete the partition w/ space recovery.
	     */
	    rflags = DELCHUNK_RECOVER;
	    /* fall through */
	case 'D':	/* delete */
	    if (label_chunk_info[here].type == PART_SLICE) {
		msg = MSG_NOT_APPLICABLE;
		break;
	    }
	    else if (label_chunk_info[here].type == PART_FAT) {
		msg = "Use the Disk Partition Editor to delete DOS partitions";
		break;
	    }
	    Delete_Chunk2(label_chunk_info[here].c->disk, label_chunk_info[here].c, rflags);
	    if (variable_cmp(DISK_LABELLED, "written"))
		variable_set2(DISK_LABELLED, "yes", 0);
	    record_label_chunks(devs, dev);
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
		if (variable_cmp(DISK_LABELLED, "written"))
		    variable_set2(DISK_LABELLED, "yes", 0);
		record_label_chunks(devs, dev);
		clear_wins();
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
	    clear_wins();
	    break;

	case 'S':	/* Toggle soft updates flag */
	    if (label_chunk_info[here].type == PART_FILESYSTEM) {
		PartInfo *pi = ((PartInfo *)label_chunk_info[here].c->private_data);
		if (pi)
		    pi->soft = !pi->soft;
		else
		    msg = MSG_NOT_APPLICABLE;
	    }
	    else
		msg = MSG_NOT_APPLICABLE;
	    break;

	case 'T':	/* Toggle newfs state */
	    if ((label_chunk_info[here].type == PART_FILESYSTEM) &&
	        (label_chunk_info[here].c->private_data)) {
		PartInfo *pi = ((PartInfo *)label_chunk_info[here].c->private_data);
		if (!pi->newfs)
		    label_chunk_info[here].c->flags |= CHUNK_NEWFS;
		else
		    label_chunk_info[here].c->flags &= ~CHUNK_NEWFS;

		label_chunk_info[here].c->private_data =
		    new_part(pi ? pi->mountpoint : NULL, pi ? !pi->newfs : TRUE);
		if (pi && pi->soft)
		    ((PartInfo *)label_chunk_info[here].c->private_data)->soft = 1;
		safe_free(pi);
		label_chunk_info[here].c->private_free = safe_free;
		if (variable_cmp(DISK_LABELLED, "written"))
		    variable_set2(DISK_LABELLED, "yes", 0);
	    }
	    else
		msg = MSG_NOT_APPLICABLE;
	    break;

	case 'U':
	    clear();
	    if (!variable_cmp(DISK_LABELLED, "written")) {
		msgConfirm("You've already written out your changes -\n"
			   "it's too late to undo!");
	    }
	    else if (!msgNoYes("Are you SURE you want to Undo everything?")) {
		variable_unset(DISK_PARTITIONED);
		variable_unset(DISK_LABELLED);
		for (i = 0; devs[i]; i++) {
		    Disk *d;

		    if (!devs[i]->enabled)
			continue;
		    else if ((d = Open_Disk(devs[i]->name)) != NULL) {
			Free_Disk(devs[i]->private);
			devs[i]->private = d;
			diskPartition(devs[i]);
		    }
		}
		record_label_chunks(devs, dev);
	    }
	    clear_wins();
	    break;

	case 'W':
	    if (!variable_cmp(DISK_LABELLED, "written")) {
		msgConfirm("You've already written out your changes - if you\n"
			   "wish to overwrite them, you'll have to restart\n"
			   "sysinstall first.");
	    }
	    else if (!msgNoYes("WARNING:  This should only be used when modifying an EXISTING\n"
			  "installation.  If you are installing FreeBSD for the first time\n"
			  "then you should simply type Q when you're finished here and your\n"
			  "changes will be committed in one batch automatically at the end of\n"
			  "these questions.\n\n"
			  "Are you absolutely sure you want to do this now?")) {
		variable_set2(DISK_LABELLED, "yes", 0);
		diskLabelCommit(NULL);
	    }
	    clear_wins();
	    break;

	case '|':
	    if (!msgNoYes("Are you sure you want to go into Wizard mode?\n\n"
			  "This is an entirely undocumented feature which you are not\n"
			  "expected to understand!")) {
		int i;
		Device **devs;

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
		if (variable_cmp(DISK_LABELLED, "written"))
		    variable_set2(DISK_LABELLED, "yes", 0);
		DialogActive = TRUE;
		record_label_chunks(devs, dev);
		clear_wins();
	    }
	    else
		msg = "A most prudent choice!";
	    break;

	case '\033':	/* ESC */
	case 'Q':
	    labeling = FALSE;
	    break;

	default:
	    beep();
	    sprintf(_msg, "Invalid key %d - Type F1 or ? for help", key);
	    msg = _msg;
	    break;
	}
        if (label_chunk_info[here].type == PART_SLICE)
            pslice_focus = here;
        else
            label_focus = here;
    }
    restorescr(w);
    return DITEM_SUCCESS;
}

static __inline int
requested_part_size(char *varName, int nom, int def, int perc)
{
    char *cp;
    int sz;

    if ((cp = variable_get(varName)) != NULL)
	sz = atoi(cp);
    else
	sz = nom + (def - nom) * perc / 100;
    return(sz * ONE_MEG);
}

/*
 * Attempt to auto-label the disk.  'perc' (0-100) scales
 * the size of the various partitions within appropriate
 * bounds (NOMINAL through DEFAULT sizes).  The procedure
 * succeeds of NULL is returned.  A non-null return message
 * is either a failure-status message (*req == 0), or 
 * a confirmation requestor (*req == 1).  *req is 0 on
 * entry to this call.
 *
 * We autolabel the following partitions:  /, swap, /var, /tmp, /usr,
 * and /home.  /home receives any extra left over disk space. 
 */
static char *
try_auto_label(Device **devs, Device *dev, int perc, int *req)
{
    int sz;
    struct chunk *root_chunk = NULL;
    struct chunk *swap_chunk = NULL;
    struct chunk *usr_chunk = NULL;
    struct chunk *var_chunk = NULL;
    struct chunk *tmp_chunk = NULL;
    struct chunk *home_chunk = NULL;
    int mib[2];
    unsigned long physmem;
    size_t size;
    Chunk *rootdev, *swapdev, *usrdev, *vardev;
    Chunk *tmpdev, *homedev;
    char *msg = NULL;

    sz = space_free(label_chunk_info[here].c);
    if (sz <= FS_MIN_SIZE)
	return("Not enough free space to create a new partition in the slice");

    (void)checkLabels(FALSE, &rootdev, &swapdev, &usrdev,
			&vardev, &tmpdev, &homedev);
    if (!rootdev) {
	sz = requested_part_size(VAR_ROOT_SIZE, ROOT_NOMINAL_SIZE, ROOT_DEFAULT_SIZE, perc);

	root_chunk = Create_Chunk_DWIM(label_chunk_info[here].c->disk,
			    label_chunk_info[here].c, sz, part,
			    FS_BSDFFS,  CHUNK_IS_ROOT | CHUNK_AUTO_SIZE);
	if (!root_chunk) {
	    *req = 1;
	    msg = "Unable to create the root partition. Too big?";
	    goto done;
	}
	root_chunk->private_data = new_part("/", TRUE);
	root_chunk->private_free = safe_free;
	root_chunk->flags |= CHUNK_NEWFS;
	record_label_chunks(devs, dev);
    }
    if (!swapdev) {
	sz = requested_part_size(VAR_SWAP_SIZE, 0, 0, perc);
	if (sz == 0) {
	    int nom;
	    int def;

	    mib[0] = CTL_HW;
	    mib[1] = HW_PHYSMEM;
	    size = sizeof physmem;
	    sysctl(mib, 2, &physmem, &size, (void *)0, (size_t)0);
	    def = 2 * (int)(physmem / 512);
	    if (def < SWAP_MIN_SIZE * ONE_MEG)
		def = SWAP_MIN_SIZE * ONE_MEG;
	    if (def > SWAP_AUTO_LIMIT_SIZE * ONE_MEG)
		def = SWAP_AUTO_LIMIT_SIZE * ONE_MEG;
	    nom = (int)(physmem / 512) / 2;
	    sz = nom + (def - nom) * perc / 100;
	}
	swap_chunk = Create_Chunk_DWIM(label_chunk_info[here].c->disk, 
			    label_chunk_info[here].c, sz, part,
			    FS_SWAP, CHUNK_AUTO_SIZE);
	if (!swap_chunk) {
	    *req = 1;
	    msg = "Unable to create the swap partition. Too big?";
	    goto done;
	}
	swap_chunk->private_data = 0;
	swap_chunk->private_free = safe_free;
	record_label_chunks(devs, dev);
    }
    if (!vardev) {
	sz = requested_part_size(VAR_VAR_SIZE, VAR_NOMINAL_SIZE, VAR_DEFAULT_SIZE, perc);

	var_chunk = Create_Chunk_DWIM(label_chunk_info[here].c->disk, 
				label_chunk_info[here].c, sz, part,
				FS_BSDFFS, CHUNK_AUTO_SIZE);
	if (!var_chunk) {
	    *req = 1;
	    msg = "Not enough free space for /var - you will need to\n"
		   "partition your disk manually with a custom install!";
	    goto done;
	}
	var_chunk->private_data = new_part("/var", TRUE);
	var_chunk->private_free = safe_free;
	var_chunk->flags |= CHUNK_NEWFS;
	record_label_chunks(devs, dev);
    }
    if (!tmpdev && !variable_get(VAR_NO_TMP)) {
	sz = requested_part_size(VAR_TMP_SIZE, TMP_NOMINAL_SIZE, TMP_DEFAULT_SIZE, perc);

	tmp_chunk = Create_Chunk_DWIM(label_chunk_info[here].c->disk, 
				label_chunk_info[here].c, sz, part,
				FS_BSDFFS, CHUNK_AUTO_SIZE);
	if (!tmp_chunk) {
	    *req = 1;
	    msg = "Not enough free space for /tmp - you will need to\n"
		   "partition your disk manually with a custom install!";
	    goto done;
	}
	tmp_chunk->private_data = new_part("/tmp", TRUE);
	tmp_chunk->private_free = safe_free;
	tmp_chunk->flags |= CHUNK_NEWFS;
	record_label_chunks(devs, dev);
    }
    if (!usrdev && !variable_get(VAR_NO_USR)) {
	sz = requested_part_size(VAR_USR_SIZE, USR_NOMINAL_SIZE, USR_DEFAULT_SIZE, perc);
#if AUTO_HOME == 0
	    sz = space_free(label_chunk_info[here].c);
#endif
	if (sz) {
	    if (sz < (USR_MIN_SIZE * ONE_MEG)) {
		*req = 1;
		msg = "Not enough free space for /usr - you will need to\n"
		       "partition your disk manually with a custom install!";
	    }

	    usr_chunk = Create_Chunk_DWIM(label_chunk_info[here].c->disk,
				    label_chunk_info[here].c, sz, part,
				    FS_BSDFFS, CHUNK_AUTO_SIZE);
	    if (!usr_chunk) {
		msg = "Unable to create the /usr partition.  Not enough space?\n"
			   "You will need to partition your disk manually with a custom install!";
		goto done;
	    }
	    usr_chunk->private_data = new_part("/usr", TRUE);
	    usr_chunk->private_free = safe_free;
	    usr_chunk->flags |= CHUNK_NEWFS;
	    record_label_chunks(devs, dev);
	}
    }
#if AUTO_HOME == 1
    if (!homedev && !variable_get(VAR_NO_HOME)) {
	sz = requested_part_size(VAR_HOME_SIZE, HOME_NOMINAL_SIZE, HOME_DEFAULT_SIZE, perc);
	if (sz < space_free(label_chunk_info[here].c))
	    sz = space_free(label_chunk_info[here].c);
	if (sz) {
	    if (sz < (HOME_MIN_SIZE * ONE_MEG)) {
		*req = 1;
		msg = "Not enough free space for /home - you will need to\n"
		       "partition your disk manually with a custom install!";
		goto done;
	    }

	    home_chunk = Create_Chunk_DWIM(label_chunk_info[here].c->disk,
				    label_chunk_info[here].c, sz, part,
				    FS_BSDFFS, CHUNK_AUTO_SIZE);
	    if (!home_chunk) {
		msg = "Unable to create the /home partition.  Not enough space?\n"
			   "You will need to partition your disk manually with a custom install!";
		goto done;
	    }
	    home_chunk->private_data = new_part("/home", TRUE);
	    home_chunk->private_free = safe_free;
	    home_chunk->flags |= CHUNK_NEWFS;
	    record_label_chunks(devs, dev);
	}
    }
#endif

    /* At this point, we're reasonably "labelled" */
    if (variable_cmp(DISK_LABELLED, "written"))
	variable_set2(DISK_LABELLED, "yes", 0);

done:
    if (msg) {
	if (root_chunk)
	    Delete_Chunk(root_chunk->disk, root_chunk);
	if (swap_chunk)
	    Delete_Chunk(swap_chunk->disk, swap_chunk);
	if (var_chunk)
	    Delete_Chunk(var_chunk->disk, var_chunk);
	if (tmp_chunk)
	    Delete_Chunk(tmp_chunk->disk, tmp_chunk);
	if (usr_chunk)
	    Delete_Chunk(usr_chunk->disk, usr_chunk);
	if (home_chunk)
	    Delete_Chunk(home_chunk->disk, home_chunk);
	record_label_chunks(devs, dev);
    }
    return(msg);
}

static int
diskLabelNonInteractive(Device *dev)
{
    char *cp;
    PartType type;
    PartInfo *p;
    u_long flags;
    int i, status;
    Device **devs;
    Disk *d;
    
    status = DITEM_SUCCESS;
    cp = variable_get(VAR_DISK);
    if (!cp) {
	msgConfirm("diskLabel:  No disk selected - can't label automatically.");
	return DITEM_FAILURE;
    }
    devs = deviceFind(cp, DEVICE_TYPE_DISK);
    if (!devs) {
	msgConfirm("diskLabel: No disk device %s found!", cp);
	return DITEM_FAILURE;
    }
    if (dev)
	d = dev->private;
    else
	d = devs[0]->private;
    record_label_chunks(devs, dev);
    for (i = 0; label_chunk_info[i].c; i++) {
	Chunk *c1 = label_chunk_info[i].c;

	if (label_chunk_info[i].type == PART_SLICE) {
	    char name[512];
	    char typ[10], mpoint[50];
	    int entries;

	    for (entries = 1;; entries++) {
		int sz, soft = 0;
		snprintf(name, sizeof name, "%s-%d", c1->name, entries);
		if ((cp = variable_get(name)) == NULL)
		    break;
		if (sscanf(cp, "%s %d %s %d", typ, &sz, mpoint, &soft) < 3) {
		    msgConfirm("For slice entry %s, got an invalid detail entry of: %s",  c1->name, cp);
		    status = DITEM_FAILURE;
		    break;
		} else {
		    Chunk *tmp;

		    flags = 0;
		    if (!strcmp(typ, "swap")) {
			type = PART_SWAP;
			strcpy(mpoint, "SWAP");
		    } else {
			type = PART_FILESYSTEM;
			if (!strcmp(mpoint, "/"))
			    flags |= CHUNK_IS_ROOT;
		    }
		    if (!sz)
			sz = space_free(c1);
		    if (sz > space_free(c1)) {
			msgConfirm("Not enough free space to create partition: %s", mpoint);
			status = DITEM_FAILURE;
			break;
		    }
		    if (!(tmp = Create_Chunk_DWIM(d, c1, sz, part,
			(type == PART_SWAP) ? FS_SWAP : FS_BSDFFS, flags))) {
			msgConfirm("Unable to create from partition spec: %s. Too big?", cp);
			status = DITEM_FAILURE;
			break;
		    } else {
			tmp->private_data = new_part(mpoint, TRUE);
			tmp->private_free = safe_free;
			((PartInfo *)tmp->private_data)->soft = soft;
		    }
		}
	    }
	} else {
	    /* Must be something we can set a mountpoint for */
	    cp = variable_get(c1->name);
	    if (cp) {
		char mpoint[50], do_newfs[8];
		Boolean newfs = FALSE;

		do_newfs[0] = '\0';
		if (sscanf(cp, "%s %s", mpoint, do_newfs) != 2) {
		    msgConfirm("For slice entry %s, got an invalid detail entry of: %s", c1->name, cp);
		    status = DITEM_FAILURE;
		    break;
		}
		newfs = toupper(do_newfs[0]) == 'Y' ? TRUE : FALSE;
		if (c1->private_data) {
		    p = c1->private_data;
		    p->newfs = newfs;
		    strcpy(p->mountpoint, mpoint);
		}
		else {
		    c1->private_data = new_part(mpoint, newfs);
		    c1->private_free = safe_free;
		}
		if (!strcmp(mpoint, "/"))
		    c1->flags |= CHUNK_IS_ROOT;
		else
		    c1->flags &= ~CHUNK_IS_ROOT;
	    }
	}
    }
    if (status == DITEM_SUCCESS)
	variable_set2(DISK_LABELLED, "yes", 0);
    return status;
}
