/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: devices.c,v 1.5 1995/05/05 23:47:38 jkh Exp $
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

/* Where we start displaying chunk information on the screen */
#define CHUNK_START_ROW		5

/* Get all device information for a given device class */
Device *
device_get_all(DeviceType which, int *ndevs)
{
    char **names;
    Device *devs = NULL;

    *ndevs = 0;
    if (which == DEVICE_TYPE_DISK || which == DEVICE_TYPE_ANY) {
	if ((names = Disk_Names()) != NULL) {
	    int i;

	    for (i = 0; names[i]; i++)
		++*ndevs;
	    devs = safe_malloc(sizeof(Device) * (*ndevs + 1));
	    for (i = 0; names[i]; i++) {
		strcpy(devs[i].name, names[i]);
		devs[i].type = DEVICE_TYPE_DISK;
	    }
	    devs[i].name[0] = '\0';
	    free(names);
	}
    }
    /* put detection for other classes here just as soon as I figure out how */
    return devs;
}

static struct chunk *chunk_info[10];
static int current_chunk;

static void
record_chunks(struct disk *d)
{
    struct chunk *c1;
    int i = 0;
    int last_free = 0;
    if (!d->chunks)
	msgFatal("No chunk list found for %s!", d->name);
    c1 = d->chunks->part;
    while (c1) {
	if (c1->type == unused && c1->size > last_free) {
	    last_free = c1->size;
	    current_chunk = i;
	}
	chunk_info[i++] = c1;
	c1 = c1->next;
    }
    chunk_info[i] = NULL;
}

static void
print_chunks(struct disk *d)
{
    int row;
    int i;
    int b_attr = ColorDisplay ? A_BOLD : A_UNDERLINE;

    attrset(A_NORMAL);
    mvaddstr(0, 0, "Disk name:\t");
    attrset(b_attr); addstr(d->name); attrset(A_NORMAL);
    attrset(A_REVERSE); mvaddstr(0, 55, "Master Partition Editor"); attrset(A_NORMAL);
    mvprintw(1, 0,
	     "BIOS Geometry:\t%lu cyls/%lu heads/%lu sectors",
	     d->bios_cyl, d->bios_hd, d->bios_sect);
    mvprintw(3, 1, "%10s %10s %10s %8s %8s %8s %8s %8s",
	     "Offset", "Size", "End", "Name", "PType", "Desc",
	     "Subtype", "Flags");
    for (i = 0, row = CHUNK_START_ROW; chunk_info[i]; i++, row++) {
	if (i == current_chunk)
	    attrset(b_attr);
	mvprintw(row, 2, "%10lu %10lu %10lu %8s %8d %8s %8d %6lx",
		 chunk_info[i]->offset, chunk_info[i]->size,
		 chunk_info[i]->end, chunk_info[i]->name,
		 chunk_info[i]->type, chunk_n[chunk_info[i]->type],
		 chunk_info[i]->subtype, chunk_info[i]->flags);
	if (i == current_chunk)
	    attrset(A_NORMAL);
    }
}

static void
print_command_summary()
{
    int b_attr = ColorDisplay ? A_BOLD : A_UNDERLINE;

    mvprintw(14, 0, "The following commands are supported (in upper or lower case):");
    mvprintw(16, 0, "A = Use Entire Disk    B = Bad Block Scan     C = Create Partition");
    mvprintw(17, 0, "D = Delete Partition   G = Set BIOS Geometry  S = Set Bootable");
    mvprintw(18, 0, "U = Undo All Changes   W = `Wizard' Mode      ESC = Proceed to next screen");
    mvprintw(20, 0, "The currently selected partition is displayed in ");
    attrset(b_attr); addstr(ColorDisplay ? "bold" : "underline"); attrset(A_NORMAL);
    move(0, 0);
}

struct disk *
device_slice_disk(struct disk *d)
{
    char *p;
    int key = 0;
    Boolean chunking;
    char *msg = NULL;
    char name[40];

    dialog_clear();
    chunking = TRUE;
    strncpy(name, d->name, 40);
    keypad(stdscr, TRUE);

    record_chunks(d);
    while (chunking) {
	clear();
	print_chunks(d);
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
	    if (chunk_info[current_chunk + 1])
		++current_chunk;
	    break;

	case KEY_HOME:
	    current_chunk = 0;
	    break;

	case KEY_END:
	    while (chunk_info[current_chunk + 1])
		++current_chunk;
	    break;

	case KEY_F(1):
	case '?':
	    systemDisplayFile("slice.hlp");
	    break;

	case 'A':
	    All_FreeBSD(d);
	    record_chunks(d);
	    break;

	case 'B':
	    if (chunk_info[current_chunk]->type != freebsd)
		msg = "Can only scan for bad blocks in FreeBSD partition.";
	    else if (strncmp(name, "sd", 2) ||
		     !msgYesNo("This typically makes sense only for ESDI, IDE or MFM drives.\nAre you sure you want to do this on a SCSI disk?"))
		chunk_info[current_chunk]->flags |= CHUNK_BAD144;
	    break;

	case 'C':
	    if (chunk_info[current_chunk]->type != unused)
		msg = "Partition in use, delete it first or move to an unused one.";
	    else {
		char *val, tmp[20];
		int size;

		snprintf(tmp, 20, "%d", chunk_info[current_chunk]->size);
		val = msgGetInput(tmp, "Please specify size for new FreeBSD partition");
		if (val && (size = strtol(val, 0, 0)) > 0) {
		    Create_Chunk(d, chunk_info[current_chunk]->offset,
				 size,
				 freebsd,
				 3,
				 (chunk_info[current_chunk]->flags &
				  CHUNK_ALIGN));
		    record_chunks(d);
		}
	    }
	    break;

	case 'D':
	    if (chunk_info[current_chunk]->type == unused)
		msg = "Partition is already unused!";
	    else {
		Delete_Chunk(d, chunk_info[current_chunk]);
		record_chunks(d);
	    }
	    break;

	case 'G':
	    /* Set geometry */
	    break;

	case 'S':
	    /* Set Bootable */
	    break;

	case 'U':
	    Free_Disk(d);
	    d = Open_Disk(name);
	    if (!d)
		msgFatal("Can't reopen disk %s!", name);
	    record_chunks(d);
	    break;

	case 'W':
	    if (!msgYesNo("Are you sure you want to go into Wizard mode?\nNo seat belts whatsoever are provided!")) {
		clear();
		dialog_clear();
		end_dialog();
		DialogActive = FALSE;
		slice_wizard(d);
		clear();
		dialog_clear();
		DialogActive = TRUE;
		record_chunks(d);
	    }
	    else
		msg = "Wise choice!";
	    break;

	case 27:	/* ESC */
	    chunking = FALSE;
	    break;

	default:
	    beep();
	    msg = "Type F1 or ? for help";
	    break;
	}
    }
    p = CheckRules(d);
    if (p) {
	msgConfirm(p);
	free(p);
    }
    clear();
    refresh();
    return d;
}

/*
 * Create a menu listing all the devices in the system.  The pass-in menu
 * is expected to be a "prototype" from which the new menu is cloned.
 */
DMenu *
device_create_disk_menu(DMenu *menu, Device **rdevs, int (*hook)())
{
    Device *devices;
    int numdevs;

    devices = device_get_all(DEVICE_TYPE_DISK, &numdevs);
    *rdevs = devices;
    if (!devices) {
	msgConfirm("No devices suitable for installation found!\n\nPlease verify that your disk controller (and attached drives) were detected properly.  This can be done by selecting the ``Bootmsg'' option on the main menu and reviewing the boot messages carefully.");
	return NULL;
    }
    else {
	Device *start;
	DMenu *tmp;
	int i;

	tmp = (DMenu *)safe_malloc(sizeof(DMenu) +
				   (sizeof(DMenuItem) * (numdevs + 1)));
	bcopy(menu, tmp, sizeof(DMenu));
	for (start = devices, i = 0; start->name[0]; start++, i++) {
	    tmp->items[i].title = start->name;
	    if (!strncmp(start->name, "sd", 2))
		tmp->items[i].prompt = "SCSI disk";
	    else if (!strncmp(start->name, "wd", 2))
		tmp->items[i].prompt = "IDE/ESDI/MFM/ST506 disk";
	    else
		msgFatal("Unknown disk type: %s!", start->name);
	    tmp->items[i].type = DMENU_CALL;
	    tmp->items[i].ptr = hook;
	    tmp->items[i].disabled = FALSE;
	}
	tmp->items[i].type = DMENU_NOP;
	tmp->items[i].title = NULL;
	return tmp;
    }
}
