/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: disks.c,v 1.3 1995/05/06 09:34:11 jkh Exp $
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

static struct {
    struct disk *d;
    struct chunk *c;
    PartInfo *p;
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

static Boolean
check_conflict(char *name)
{
    int i;

    for (i = 0; fbsd_chunk_info[i].d; i++)
	if (fbsd_chunk_info[i].type == PART_FILESYSTEM &&
	    !strcmp(fbsd_chunk_info[i].c->name, name))
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

    j = p = 0;
    for (i = 0; disks[i]; i++) {
	struct chunk *c1;

	if (!disks[i]->chunks)
	    msgFatal("No chunk list found for %s!", disks[i]->name);
	c1 = disks[i]->chunks->part;
	while (c1) {
	    if (c1->type == freebsd) {
		struct chunk *c2 = c1->part;

		fbsd_chunk_info[j].type = PART_SLICE;
		fbsd_chunk_info[j].d = disks[i];
		fbsd_chunk_info[j].c = c1;
		fbsd_chunk_info[j++].p = NULL;
		while (c2) {
		    if (c2->type == part) {
			if (c2->subtype == FS_SWAP)
			    fbsd_chunk_info[j].type = PART_SWAP;
			else
			    fbsd_chunk_info[j].type = PART_FILESYSTEM;
			fbsd_chunk_info[j].d = disks[i];
			fbsd_chunk_info[j].c = c2;
			fbsd_chunk_info[j++].p = c2->private;
		    }
		    c2 = c2->next;
		}
	    }
	    c1 = c1->next;
	}
    }
    fbsd_chunk_info[j].d = NULL;
    fbsd_chunk_info[j].c = NULL;
}

int
get_mountpoint(struct chunk *c)
{
    char *val;
    PartInfo *part;

    val = msgGetInput(c->private,
		      "Please specify mount point for new partition");
    if (val) {
	if (!strcmp(val, "/")) {
	    if (c->flags & CHUNK_PAST_1024) {
msgConfirm("This region cannot be used for your root partition as\nit is past the 1024'th cylinder mark and the system would not be\nable to boot from it.  Please pick another location for your\nroot partition and try again!");
		return 1;
	    }
	    c->flags |= CHUNK_IS_ROOT;
	}
	if (check_conflict(val)) {
	    msgConfirm("You already have a mountpoint for %s assigned!", val);
	    return 1;
	}
	safe_free(c->private);
	part = (PartInfo *)malloc(sizeof(PartInfo));
	strncpy(part->mountpoint, val, FILENAME_MAX);
	part->newfs = TRUE;
	c->private = (void *)part;
	c->private_free = free;
	return 0;
    }
    return 1;
}

static PartType
get_partition_type(struct chunk *c)
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

#define MAX_MOUNT_NAME	12

#define PART_PART_COL	0
#define PART_MOUNT_COL	8
#define PART_SIZE_COL	(PART_MOUNT_COL + MAX_MOUNT_NAME + 4)
#define PART_NEWFS_COL	(PART_SIZE_COL + 8)
#define PART_OFF	40

static void
print_fbsd_chunks(void)
{
    int i, srow, prow, pcol;
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
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_SIZE_COL + (i * PART_OFF),
		 "Size");

	attrset(A_UNDERLINE);
	mvaddstr(CHUNK_PART_START_ROW - 1, PART_NEWFS_COL + (i * PART_OFF),
		 "Newfs");
	attrset(A_NORMAL);
    }
				    
    srow = CHUNK_SLICE_START_ROW;
    prow = CHUNK_PART_START_ROW;

    for (i = 0; fbsd_chunk_info[i].d; i++) {
	if (i == current_chunk)
	    attrset(ColorDisplay ? A_BOLD : A_UNDERLINE);
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
	    char *mountpoint, *newfs;

	    /* Go for two columns */
	    if (prow == (CHUNK_PART_START_ROW + 9))
		pcol = PART_OFF;
	    else
		pcol = 0;
	    mvaddstr(prow, pcol + PART_PART_COL, fbsd_chunk_info[i].c->name);
	    if (fbsd_chunk_info[i].type == PART_FILESYSTEM) {
		if (fbsd_chunk_info[i].p) {
		    mountpoint = fbsd_chunk_info[i].p->mountpoint;
		    newfs = fbsd_chunk_info[i].p->newfs ? "Y" : "N";
		}
		else {
		    mountpoint = " ";
		    newfs = " ";
		}
	    }
	    else {
		mountpoint = "swap";
		newfs = " ";
	    }
	    for (i = 0; i < MAX_MOUNT_NAME && mountpoint[i]; i++)
		mvaddch(prow, pcol + PART_MOUNT_COL + i, mountpoint[i]);
	    mvprintw(prow, pcol + PART_SIZE_COL, "%4dMB",
		     fbsd_chunk_info[i].c->size ?
		     fbsd_chunk_info[i].c->size / 2048 : 0);
	    mvaddstr(prow, pcol + PART_NEWFS_COL, newfs);
	    ++prow;
	}
	if (i == current_chunk)
	    attrset(A_NORMAL);
    }
}

static void
print_command_summary()
{
    int attrs = ColorDisplay ? A_BOLD : A_UNDERLINE;

    mvprintw(19, 0,
	     "The following commands are valid here (upper or lower case):");
    mvprintw(20, 0, "C = Create FreeBSD Partition      D = Delete Partition");
    mvprintw(21, 0, "M = Mount Partition (no newfs)    ESC = Proceed to summary screen");
    mvprintw(22, 0, "The default target will be displayed in ");

    attrset(attrs);
    addstr(ColorDisplay ? "bold" : "underline");
    attrset(A_NORMAL);
    move(0, 0);
}

void
partition_disks(struct disk **disks)
{
    int sz, key = 0;
    Boolean partitioning;
    char *msg = NULL;

    dialog_clear();
    partitioning = TRUE;
    keypad(stdscr, TRUE);
    record_fbsd_chunks(disks);

    while (partitioning) {
	clear();
	print_fbsd_chunks();
	print_command_summary();
	if (msg) {
	    standout(); mvprintw(23, 0, msg); standend();
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
		msg = "Can only create sub-partitions in a master partition (at top)";
		break;
	    }
	    sz = space_free(fbsd_chunk_info[current_chunk].c);
	    if (sz <= FS_MIN_SIZE)
		msg = "Not enough space to create additional FreeBSD partition";
	    else {
		char *val, tmp[20];
		int size;

		snprintf(tmp, 20, "%d", sz);
		val = msgGetInput(tmp, "Please specify size for new FreeBSD partition");
		if (val && (size = strtol(val, 0, 0)) > 0) {
		    PartType type;

		    if (get_mountpoint(fbsd_chunk_info[current_chunk].c))
			break;
		    type = get_partition_type(fbsd_chunk_info[current_chunk].c);
		    if (type == PART_NONE)
			break;
		    Create_Chunk(fbsd_chunk_info[current_chunk].d,
				 fbsd_chunk_info[current_chunk].c->offset +
				 sz - size,
				 size,
				 part,
				 type == PART_SWAP ? FS_SWAP : freebsd,
				 fbsd_chunk_info[current_chunk].c->flags);
		    record_fbsd_chunks(disks);
		}
	    }
	    break;

	case 'D':
	    if (fbsd_chunk_info[current_chunk].type == PART_SLICE) {
		msg = "Use the Master Partition Editor to delete one of these";
		break;
	    }
	    Delete_Chunk(fbsd_chunk_info[current_chunk].d,
			 fbsd_chunk_info[current_chunk].c);
	    break;
	    
	case 27:	/* ESC */
	    partitioning = FALSE;
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
    if (!msgYesNo("Last Chance!  Are you sure you want to write out\nall your changes to disk?")) {
	for (i = 0; disks[i]; i++) {
	    if (contains_root_partition(disks[i]))
		Set_Boot_Blocks(disks[i], boot1, boot2);
	    if (i == 0 && !msgYesNo("Would you like to install a boot manager?\n\nThis will allow you to easily select between other operating systems\non the first disk, or boot from a disk other than the first."))
		Set_Boot_Mgr(disks[i], bteasy17);
	    else if (i == 0 && !msgYesNo("Would you like to remove an existing boot manager?"))
		Set_Boot_Mgr(disks[i], mbr);
#if 0
	    Write_Disk(disks[i]);
#endif
	}
	return 0;
    }
    return 1;
}
