/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: disks.c,v 1.10 1995/05/08 00:56:28 jkh Exp $
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

/*
 * I make some pretty gross assumptions about having a max of 50 chunks
 * total - 8 slices and 42 partitions.  I can't easily display many more
 * than that on the screen at once!
 *
 * For 2.1 I'll revisit this and try to make it more dynamic, but since
 * this will catch 99.99% of all possible cases, I'm not too worried.
 */

#define MAX_CHUNKS	50

/* Where to start printing the freebsd slices */
#define CHUNK_SLICE_START_ROW		2
#define CHUNK_PART_START_ROW		10

/* The smallest filesystem we're willing to create */
#define FS_MIN_SIZE			2048

#define MSG_NOT_APPLICABLE		"That option is not applicable here"

static struct {
    struct disk *d;
    struct chunk *c;
    PartType type;
} fbsd_chunk_info[MAX_CHUNKS + 1];
static int current_chunk;


/* If the given disk has a root partition on it, return TRUE */
static Boolean
contains_root_partition(struct disk *d)
{
    struct chunk *c1;

    if (!d->chunks)
	msgFatal("Disk %s has no chunks!", d->name);
    c1 = d->chunks->part;
    while (c1) {
	if (c1->type == freebsd) {
	    struct chunk *c2 = c1->part;

	    while (c2) {
		if (c2->flags & CHUNK_IS_ROOT)
		    return TRUE;
		c2 = c2->next;
	    }
	}
	c1 = c1->next;
    }
    return FALSE;
}

/* Locate a chunk by position on a specific disk somewhere */
static struct chunk *
find_chunk_by_loc(struct disk *d, u_long offset, u_long size)
{
    struct chunk *c1, *c2;

    if (!d->chunks)
	msgFatal("No chunk list for %s!", d->name);

    for (c1 = d->chunks->part; c1; c1 = c1->next) {
	if (c1->type == freebsd) {
	    if (c1->offset == offset && c1->size == size)
		return c1;
	    for (c2 = c1->part; c2; c2 = c2->next) {
		if (c2->type == part && c2->offset == offset &&
		    c2->size == size)
		    return c2;
	    }
	}
    }
    return NULL;
}

static Boolean
check_conflict(char *name)
{
    int i;

    for (i = 0; fbsd_chunk_info[i].d; i++)
	if (fbsd_chunk_info[i].type == PART_FILESYSTEM &&
	    fbsd_chunk_info[i].c->private &&
	    !strcmp(((PartInfo *)fbsd_chunk_info[i].c->private)->mountpoint,
		    name))
	    return TRUE;
    return FALSE;
}

static int
space_free(struct chunk *c)
{
    struct chunk *c1 = c->part;
    int sz = c->size;

    while (c1) {
	if (c1->type != unused)
	    sz -= c1->size;
	c1 = c1->next;
    }
    if (sz < 0)
	msgFatal("Partitions are larger than actual chunk??");
    return sz;
}

static void
record_fbsd_chunks(struct disk **disks)
{
    int i, j, p;
    struct chunk *c1, *c2;

    j = p = 0;
    for (i = 0; disks[i]; i++) {
	if (!disks[i]->chunks)
	    msgFatal("No chunk list found for %s!", disks[i]->name);

	/* Put the freebsd chunks first */
	for (c1 = disks[i]->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		fbsd_chunk_info[j].type = PART_SLICE;
		fbsd_chunk_info[j].d = disks[i];
		fbsd_chunk_info[j].c = c1;
		++j;
	    }
	}
    }
    for (i = 0; disks[i]; i++) {
	/* Then buzz through and pick up the partitions */
	for (c1 = disks[i]->chunks->part; c1; c1 = c1->next) {
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part) {
			if (c2->subtype == FS_SWAP)
			    fbsd_chunk_info[j].type = PART_SWAP;
			else
			    fbsd_chunk_info[j].type = PART_FILESYSTEM;
			fbsd_chunk_info[j].d = disks[i];
			fbsd_chunk_info[j].c = c2;
			++j;
		    }
		}
	    }
	}
    }
    fbsd_chunk_info[j].d = NULL;
    fbsd_chunk_info[j].c = NULL;
    if (current_chunk >= j)
	current_chunk = j  ? j - 1 : 0;
}

static PartInfo *
new_part(char *mpoint, Boolean newfs)
{
    PartInfo *ret;

    ret = (PartInfo *)safe_malloc(sizeof(PartInfo));
    strncpy(ret->mountpoint, mpoint, FILENAME_MAX);
    strcpy(ret->newfs_cmd, "newfs");
    ret->newfs = newfs;
    return ret;
}

PartInfo *
get_mountpoint(struct chunk *c)
{
    char *val;
    PartInfo *tmp;

    val = msgGetInput(c && c->private ?
		      ((PartInfo *)c->private)->mountpoint : NULL,
		      "Please specify a mount point for the partition");
    if (val) {
	if (check_conflict(val)) {
	    msgConfirm("You already have a mountpoint for %s assigned!", val);
	    return NULL;
	}
	else if (!strcmp(val, "/")) {
	    if (c && c->flags & CHUNK_PAST_1024) {
msgConfirm("This region cannot be used for your root partition as\nit is past the 1024'th cylinder mark and the system would not be\nable to boot from it.  Please pick another location for your\nroot partition and try again!");
		return NULL;
	    }
	    else if (c)
		c->flags |= CHUNK_IS_ROOT;
	}
	else if (c)
	    c->flags &= ~CHUNK_IS_ROOT;
	safe_free(c ? c->private : NULL);
	tmp = new_part(val, TRUE);
	if (c) {
	    c->private = tmp;
	    c->private_free = safe_free;
	}
	return tmp;
    }
    return NULL;
}

static PartType
get_partition_type(void)
{
    char selection[20];
    static unsigned char *fs_types[] = {
	"FS",
	"A file system",
	"Swap",
	"A swap partition.",
    };

    if (!dialog_menu("Please choose a partition type",
		    "If you want to use this partition for swap space, select Swap.\nIf you want to put a filesystem on it, choose FS.", -1, -1, 2, 2, fs_types, selection, NULL, NULL)) {
	if (!strcmp(selection, "FS"))
	    return PART_FILESYSTEM;
	else if (!strcmp(selection, "Swap"))
	    return PART_SWAP;
    }
    return PART_NONE;
}

static void
getNewfsCmd(PartInfo *p)
{
    char *val;

    val = msgGetInput(p->newfs_cmd,
		      "Please enter the newfs command and options you'd like to use in\ncreating this file system.");
    if (val)
	strncpy(p->newfs_cmd, val, NEWFS_CMD_MAX);
}


#define MAX_MOUNT_NAME	12

#define PART_PART_COL	0
#define PART_MOUNT_COL	8
#define PART_SIZE_COL	(PART_MOUNT_COL + MAX_MOUNT_NAME + 3)
#define PART_NEWFS_COL	(PART_SIZE_COL + 7)
#define PART_OFF	38

/* How many mounted partitions to display in column before going to next */
#define CHUNK_COLUMN_MAX	6

static void
print_fbsd_chunks(void)
{
    int i, j, srow, prow, pcol;
    int sz;

    attrset(A_REVERSE);
    mvaddstr(0, 25, "FreeBSD Partition Editor");
    attrset(A_NORMAL);

    for (i = 0; i < 2; i++) {
	attrset(A_UNDERLINE);
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_PART_COL + (i * PART_OFF),
		 "Part");
	attrset(A_NORMAL);

	attrset(A_UNDERLINE);
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_MOUNT_COL + (i * PART_OFF),
		 "Mount");
	attrset(A_NORMAL);

	attrset(A_UNDERLINE);
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_SIZE_COL + (i * PART_OFF) + 2,
		 "Size");
	attrset(A_NORMAL);

	attrset(A_UNDERLINE);
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_NEWFS_COL + (i * PART_OFF),
		 "Newfs");
	attrset(A_NORMAL);
    }
				    
    srow = CHUNK_SLICE_START_ROW;
    prow = CHUNK_PART_START_ROW;
    pcol = 0;

    for (i = 0; fbsd_chunk_info[i].d; i++) {
	if (i == current_chunk)
	    attrset(A_REVERSE);
	/* Is it a slice entry displayed at the top? */
	if (fbsd_chunk_info[i].type == PART_SLICE) {
	    sz = space_free(fbsd_chunk_info[i].c);
	    mvprintw(srow++, 0,
		     "Disk: %s\tPartition name: %s\tFree: %d blocks (%dMB)",
		     fbsd_chunk_info[i].d->name,
		     fbsd_chunk_info[i].c->name, sz, (sz / 2048));
	}
	/* Otherwise it's a swap or filesystem entry, at the bottom */
	else {
	    char onestr[PART_OFF], num[10], *mountpoint, *newfs;

	    memset(onestr, ' ', PART_OFF - 1);
	    onestr[PART_OFF - 1] = '\0';
	    /* Go for two columns */
	    if (prow == (CHUNK_PART_START_ROW + CHUNK_COLUMN_MAX)) {
		pcol = PART_OFF;
		prow = CHUNK_PART_START_ROW;
	    }
	    memcpy(onestr + PART_PART_COL, fbsd_chunk_info[i].c->name,
		   strlen(fbsd_chunk_info[i].c->name));
	    if (fbsd_chunk_info[i].type == PART_FILESYSTEM) {
		if (fbsd_chunk_info[i].c->private) {
		    mountpoint = ((PartInfo *)fbsd_chunk_info[i].c->private)->mountpoint;
		    newfs = ((PartInfo *)fbsd_chunk_info[i].c->private)->newfs ? "Y" : "N";
		}
		else {
		    fbsd_chunk_info[i].c->private = new_part("", FALSE);
		    fbsd_chunk_info[i].c->private_free = safe_free;
		    mountpoint = " ";
		    newfs = "N";
		}
	    }
	    else {
		mountpoint = "swap";
		newfs = " ";
	    }
	    for (j = 0; j < MAX_MOUNT_NAME && mountpoint[j]; j++)
		onestr[PART_MOUNT_COL + j] = mountpoint[j];
	    sprintf(num, "%4ldMB", fbsd_chunk_info[i].c->size ?
		    fbsd_chunk_info[i].c->size / 2048 : 0);
	    memcpy(onestr + PART_SIZE_COL, num, strlen(num));
	    memcpy(onestr + PART_NEWFS_COL, newfs, strlen(newfs));
	    mvaddstr(prow, pcol, onestr);
	    ++prow;
	}
	if (i == current_chunk)
	    attrset(A_NORMAL);
    }
}

static void
print_command_summary()
{
    mvprintw(17, 0,
	     "The following commands are valid here (upper or lower case):");
    mvprintw(19, 0, "C = Create Partition   D = Delete Partition   M = Mount Partition");
    mvprintw(20, 0, "N = Newfs Options      T = Toggle Newfs       ESC = Finish Partitioning");
    mvprintw(21, 0, "The default target will be displayed in ");

    attrset(A_REVERSE);
    addstr("reverse video");
    attrset(A_NORMAL);
    mvprintw(22, 0, "Use F1 or ? to get more help");
    move(0, 0);
}

void
partition_disks(struct disk **disks)
{
    int sz, key = 0;
    Boolean partitioning;
    char *msg = NULL;
    PartInfo *p;
    PartType type;

    dialog_clear();
    partitioning = TRUE;
    keypad(stdscr, TRUE);
    record_fbsd_chunks(disks);

    while (partitioning) {
	clear();
	print_fbsd_chunks();
	print_command_summary();
	if (msg) {
	    attrset(A_REVERSE); mvprintw(23, 0, msg); attrset(A_NORMAL);
	    beep();
	    msg = NULL;
	}
	refresh();
	key = toupper(getch());
	switch (key) {

	case KEY_UP:
	case '-':
	    if (current_chunk != 0)
		--current_chunk;
	    break;

	case KEY_DOWN:
	case '+':
	case '\r':
	case '\n':
	    if (fbsd_chunk_info[current_chunk + 1].d)
		++current_chunk;
	    break;

	case KEY_HOME:
	    current_chunk = 0;
	    break;

	case KEY_END:
	    while (fbsd_chunk_info[current_chunk + 1].d)
		++current_chunk;
	    break;

	case KEY_F(1):
	case '?':
	    systemDisplayFile("partitioning.hlp");
	    break;

	case 'C':
	    if (fbsd_chunk_info[current_chunk].type != PART_SLICE) {
		msg = "You can only do this in a master partition (see top of screen)";
		break;
	    }
	    sz = space_free(fbsd_chunk_info[current_chunk].c);
	    if (sz <= FS_MIN_SIZE)
		msg = "Not enough space to create additional FreeBSD partition";
	    else {
		char *val, *cp, tmp[20];
		int size;

		snprintf(tmp, 20, "%d", sz);
		val = msgGetInput(tmp, "Please specify the size for new FreeBSD partition in blocks, or append\na trailing `M' for megabytes (e.g. 20M).");
		if (val && (size = strtol(val, &cp, 0)) > 0) {
		    struct chunk *tmp;
		    u_long flags = 0;

		    if (*cp && toupper(*cp) == 'M')
			size *= 2048;
		    
		    type = get_partition_type();
		    if (type == PART_NONE)
			break;
		    else if (type == PART_FILESYSTEM) {
			if ((p = get_mountpoint(NULL)) == NULL)
			    break;
			else if (!strcmp(p->mountpoint, "/"))
			    flags |= CHUNK_IS_ROOT;
			else
			    flags &= ~CHUNK_IS_ROOT;
		    }
		    else
			p = NULL;
		    Create_Chunk(fbsd_chunk_info[current_chunk].d,
				 fbsd_chunk_info[current_chunk].c->offset +
				 sz - size,
				 size,
				 part,
				 (type == PART_SWAP) ? FS_SWAP : FS_BSDFFS,
				 flags);
		    tmp = find_chunk_by_loc(fbsd_chunk_info[current_chunk].d,
					    fbsd_chunk_info[current_chunk].c->offset + sz - size, size);
		    if (!tmp)
			msgConfirm("Unable to create the partition.  Too big?");
		    else {
			tmp->private = p;
			tmp->private_free = safe_free;
			record_fbsd_chunks(disks);
		    }
		}
	    }
	    break;

	case 'D':	/* delete */
	    if (fbsd_chunk_info[current_chunk].type == PART_SLICE) {
		msg = MSG_NOT_APPLICABLE;
		break;
	    }
	    Delete_Chunk(fbsd_chunk_info[current_chunk].d,
			 fbsd_chunk_info[current_chunk].c);
	    record_fbsd_chunks(disks);
	    break;

	case 'M':	/* mount */
	    switch(fbsd_chunk_info[current_chunk].type) {
	    case PART_SLICE:
		msg = MSG_NOT_APPLICABLE;
		break;

	    case PART_SWAP:
		msg = "You don't need to specify a mountpoint for a swap partition.";
		break;

	    case PART_FILESYSTEM:
		p = get_mountpoint(fbsd_chunk_info[current_chunk].c);
		if (p) {
		    p->newfs = FALSE;
		    record_fbsd_chunks(disks);
		}
		break;

	    default:
		msgFatal("Bogus partition under cursor???");
		break;
	    }
	    break;

	case 'N':	/* Set newfs options */
	    if (fbsd_chunk_info[current_chunk].c->private &&
		((PartInfo *)fbsd_chunk_info[current_chunk].c->private)->newfs)
		getNewfsCmd(fbsd_chunk_info[current_chunk].c->private);
	    else
		msg = MSG_NOT_APPLICABLE;
	    break;

	case 'T':	/* Toggle newfs state */
	    if (fbsd_chunk_info[current_chunk].c->private)
		((PartInfo *)fbsd_chunk_info[current_chunk].c->private)->newfs = !((PartInfo *)fbsd_chunk_info[current_chunk].c->private)->newfs;
	    else
		msg = MSG_NOT_APPLICABLE;
	    break;

	case 'W':
	    if (!msgYesNo("Are you sure you want to go into Wizard mode?\n\nThis is an entirely undocumented feature which you are not\nexpected to understand!")) {
		int i;

		clear();
		dialog_clear();
		end_dialog();
		DialogActive = FALSE;
		for (i = 0; disks[i]; i++)
		    slice_wizard(disks[i]);
		clear();
		dialog_clear();
		DialogActive = TRUE;
		record_fbsd_chunks(disks);
	    }
	    else
		msg = "A most prudent choice!";
	    break;

	case 27:	/* ESC */
	    partitioning = FALSE;
	    break;

	default:
	    beep();
	    msg = "Type F1 or ? for help";
	    break;
	}
    }
}

int
write_disks(struct disk **disks)
{
    int i;
    extern u_char boot1[], boot2[];
    extern u_char mbr[], bteasy17[];

    dialog_clear();
    for (i = 0; disks[i]; i++) {
	if (contains_root_partition(disks[i]))
	    Set_Boot_Blocks(disks[i], boot1, boot2);
	dialog_clear();
	if (i == 0 && !msgYesNo("Would you like to install a boot manager?\n\nThis will allow you to easily select between other operating systems\non the first disk, or boot from a disk other than the first."))
	    Set_Boot_Mgr(disks[i], bteasy17);
	else {
	    dialog_clear();
	    if (i == 0 && !msgYesNo("Would you like to remove an existing boot manager?"))
		Set_Boot_Mgr(disks[i], mbr);
	}
	dialog_clear();
	if (!msgYesNo("Last Chance!  Are you sure you want to write out\nall these changes to disk?")) {
	    Write_Disk(disks[i]);
	    return 0;
	}
    }
    return 1;
}
