/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: disks.c,v 1.28 1995/05/25 18:48:23 jkh Exp $
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

/* Where we start displaying chunk information on the screen */
#define CHUNK_START_ROW		5

/* Where we keep track of MBR chunks */
static struct chunk *chunk_info[10];
static int current_chunk;

static void
record_chunks(Disk *d)
{
    struct chunk *c1;
    int i = 0;
    int last_free = 0;
    if (!d->chunks)
	msgFatal("No chunk list found for %s!", d->name);
    c1 = d->chunks->part;
    current_chunk = 0;
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
print_chunks(Disk *d)
{
    int row;
    int i;

    attrset(A_NORMAL);
    mvaddstr(0, 0, "Disk name:\t");
    clrtobot();
    attrset(A_REVERSE); addstr(d->name); attrset(A_NORMAL);
    attrset(A_REVERSE); mvaddstr(0, 55, "Master Partition Editor"); attrset(A_NORMAL);
    mvprintw(1, 0,
	     "BIOS Geometry:\t%lu cyls/%lu heads/%lu sectors",
	     d->bios_cyl, d->bios_hd, d->bios_sect);
    mvprintw(3, 1, "%10s %10s %10s %8s %8s %8s %8s %8s",
	     "Offset", "Size", "End", "Name", "PType", "Desc",
	     "Subtype", "Flags");
    for (i = 0, row = CHUNK_START_ROW; chunk_info[i]; i++, row++) {
	if (i == current_chunk)
	    attrset(A_REVERSE);
	mvprintw(row, 2, "%10ld %10lu %10lu %8s %8d %8s %8d %6lx",
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
    mvprintw(14, 0, "The following commands are supported (in upper or lower case):");
    mvprintw(16, 0, "A = Use Entire Disk    B = Bad Block Scan     C = Create Partition");
    mvprintw(17, 0, "D = Delete Partition   G = Set BIOS Geometry  S = Set Bootable");
    mvprintw(18, 0, "U = Undo All Changes   ESC = Exit this screen");
    mvprintw(20, 0, "The currently selected partition is displayed in ");
    attrset(A_REVERSE); addstr("reverse"); attrset(A_NORMAL); addstr(" video.");
    mvprintw(21, 0, "Use F1 or ? to get more help, arrow keys to move.");
    move(0, 0);
}

static Disk *
diskPartition(Disk *d)
{
    char *p;
    int key = 0;
    Boolean chunking;
    char *msg = NULL;
    char name[40];

    chunking = TRUE;
    strncpy(name, d->name, 40);
    keypad(stdscr, TRUE);

    clear();
    record_chunks(d);
    while (chunking) {
	print_chunks(d);
	print_command_summary();
	if (msg) {
	    standout(); mvprintw(23, 0, msg); standend();
	    beep();
	    msg = NULL;
	}

	key = toupper(getch());
	switch (key) {

	case '\014':	/* ^L */
	    clear();
	    continue;

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
		if (chunk_info[current_chunk]->flags & CHUNK_BAD144)
		    chunk_info[current_chunk]->flags &= ~CHUNK_BAD144;
		else
		    chunk_info[current_chunk]->flags |= CHUNK_BAD144;
	    break;

	case 'C':
	    if (chunk_info[current_chunk]->type != unused)
		msg = "Partition in use, delete it first or move to an unused one.";
	    else {
		char *val, tmp[20], *cp;
		int size;

		snprintf(tmp, 20, "%d", chunk_info[current_chunk]->size);
		val = msgGetInput(tmp, "Please specify the size for new FreeBSD partition in blocks, or append\na trailing `M' for megabytes (e.g. 20M).");
		if (val && (size = strtol(val, &cp, 0)) > 0) {
		    if (*cp && toupper(*cp) == 'M')
			size *= 2048;
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

	case 'G': {
	    char *val, geometry[80];

	    snprintf(geometry, 80, "%lu/%lu/%lu",
		     d->bios_cyl, d->bios_hd, d->bios_sect);
	    val = msgGetInput(geometry,
"Please specify the new geometry in cyl/hd/sect format.\nDon't forget to use the two slash (/) separator characters!\nIt's not possible to parse the field without them.");
	    if (val) {
		d->bios_cyl = strtol(val, &val, 0);
		d->bios_hd = strtol(val + 1, &val, 0);
		d->bios_sect = strtol(val + 1, 0, 0);
	    }
	}
	    break;

	case 'S':
	    /* Set Bootable */
	    chunk_info[current_chunk]->flags |= CHUNK_ACTIVE;
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
		dialog_clear();
		end_dialog();
		DialogActive = FALSE;
		slice_wizard(d);
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
    dialog_clear();
    variable_set2(DISK_PARTITIONED, "yes");
    return d;
}

static int
partitionHook(char *str)
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
	    msgConfirm("Unable to find disk %s!", str);
	    return 0;
	}
	else if (devs[1])
	    msgConfirm("Bizarre multiple match for %s!", str);
	devs[0]->private = diskPartition((Disk *)devs[0]->private);
	devs[0]->enabled = TRUE;
	str = cp;
    }
    return devs ? 1 : 0;
}

int
diskPartitionEditor(char *str)
{
    DMenu *menu;

    menu = deviceCreateMenu(&MenuDiskDevices, DEVICE_TYPE_DISK, partitionHook);
    if (!menu) {
	msgConfirm("No devices suitable for installation found!\n\nPlease verify that your disk controller (and attached drives) were detected properly.  This can be done by selecting the ``Bootmsg'' option on the main menu and reviewing the boot messages carefully.");
    }
    else {
	dmenuOpenSimple(menu);
	free(menu);
    }
    return 0;
}
