/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: disks.c,v 1.31.2.18 1995/10/19 15:54:57 jkh Exp $
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
static struct chunk *chunk_info[16];
static int current_chunk;

static void
record_chunks(Disk *d)
{
    struct chunk *c1;
    int i = 0;
    int last_free = 0;
    if (!d->chunks)
	msgFatal("No chunk list found for %s!", d->name);
    current_chunk = 0;
    for (c1 = d->chunks->part; c1; c1 = c1->next) {
	if (c1->type == unused && c1->size > last_free) {
	    last_free = c1->size;
	    current_chunk = i;
	}
	chunk_info[i++] = c1;
    }
    chunk_info[i] = NULL;
}

static void
print_chunks(Disk *d)
{
    int row;
    int i;

    if ((!d->bios_cyl || d->bios_cyl > 65536) || (!d->bios_hd || d->bios_hd > 256) || (!d->bios_sect || d->bios_sect >= 64))
	msgConfirm("WARNING:  The detected geometry is incorrect!  Please adjust\n"
		   "it to the correct values manually with the (G)eometry command.\n"
		   "If you are unsure about the correct geometry (which may be\n"
		   "\"translated\"), please consult the Hardware Guide in the\n"
		   "Documentation submenu.");
			  
    attrset(A_NORMAL);
    mvaddstr(0, 0, "Disk name:\t");
    clrtobot();
    attrset(A_REVERSE); addstr(d->name); attrset(A_NORMAL);
    attrset(A_REVERSE); mvaddstr(0, 55, "FDISK Partition Editor"); attrset(A_NORMAL);
    mvprintw(1, 0,
	     "BIOS Geometry:\t%lu cyls/%lu heads/%lu sectors",
	     d->bios_cyl, d->bios_hd, d->bios_sect);
    mvprintw(3, 1, "%10s %10s %10s %8s %8s %8s %8s %8s",
	     "Offset", "Size", "End", "Name", "PType", "Desc",
	     "Subtype", "Flags");
    for (i = 0, row = CHUNK_START_ROW; chunk_info[i]; i++, row++) {
	if (i == current_chunk)
	    attrset(A_REVERSE);
	mvprintw(row, 2, "%10ld %10lu %10lu %8s %8d %8s %8d\t%-6s",
		 chunk_info[i]->offset, chunk_info[i]->size,
		 chunk_info[i]->end, chunk_info[i]->name,
		 chunk_info[i]->type, chunk_n[chunk_info[i]->type],
		 chunk_info[i]->subtype, ShowChunkFlags(chunk_info[i]));
	if (i == current_chunk)
	    attrset(A_NORMAL);
    }
}

static void
print_command_summary()
{
    mvprintw(14, 0, "The following commands are supported (in upper or lower case):");
    mvprintw(16, 0, "A = Use Entire Disk    B = Bad Block Scan     C = Create Partition");
    mvprintw(17, 0, "D = Delete Partition   G = Set Drive Geometry S = Set Bootable");
    mvprintw(18, 0, "U = Undo All Changes   Q = Finish             W = Write Changes");
    mvprintw(20, 0, "The currently selected partition is displayed in ");
    attrset(A_REVERSE); addstr("reverse"); attrset(A_NORMAL); addstr(" video.");
    mvprintw(21, 0, "Use F1 or ? to get more help, arrow keys to move.");
    move(0, 0);
}

void
diskPartition(Device *dev, Disk *d)
{
    char *p;
    int key = 0;
    Boolean chunking;
    char *msg = NULL;

    chunking = TRUE;
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
	    print_command_summary();
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
	    systemDisplayHelp("slice");
	    break;

	case 'A': {
	    int rv;

	    rv = msgYesNo("Do you want to do this with a true partition entry\n"
			  "so as to remain cooperative with any future possible\n"
			  "operating systems on the drive(s)?");
	    if (rv) {
		rv = !msgYesNo("This is dangerous in that it will make the drive totally\n"
			       "uncooperative with other potential operating systems on the\n"
			       "same disk.  It will lead instead to a totally dedicated disk,\n"
			       "starting at the very first sector, bypassing all BIOS geometry\n"
			       "considerations.\n"
			       "You will run into serious trouble with ST-506 and ESDI drives\n"
			       "and possibly some IDE drives (e.g. drives running under the\n"
			       "control of sort of disk manager).  SCSI drives are considerably\n"
			       "less at risk.\n\n"
			       "Do you insist on dedicating the entire disk this way?");
	    }
	    if (rv)
		msgInfo("Well OK, but you can't say you haven't been warned!");
	    All_FreeBSD(d, rv);
	    variable_set2(DISK_PARTITIONED, "yes");
	    record_chunks(d);
	}
	    break;

	case 'B':
	    if (chunk_info[current_chunk]->type != freebsd)
		msg = "Can only scan for bad blocks in FreeBSD partition.";
	    else if (strncmp(d->name, "sd", 2) ||
		     !msgYesNo("This typically makes sense only for ESDI, IDE or MFM drives.\n"
			       "Are you sure you want to do this on a SCSI disk?")) {
		if (chunk_info[current_chunk]->flags & CHUNK_BAD144)
		    chunk_info[current_chunk]->flags &= ~CHUNK_BAD144;
		else
		    chunk_info[current_chunk]->flags |= CHUNK_BAD144;
	    }
	    break;

	case 'C':
	    if (chunk_info[current_chunk]->type != unused)
		msg = "Partition in use, delete it first or move to an unused one.";
	    else {
		char *val, tmp[20], *cp;
		int size;

		snprintf(tmp, 20, "%d", chunk_info[current_chunk]->size);
		val = msgGetInput(tmp, "Please specify the size for new FreeBSD partition in blocks, or append\n"
				  "a trailing `M' for megabytes (e.g. 20M).");
		if (val && (size = strtol(val, &cp, 0)) > 0) {
		    if (*cp && toupper(*cp) == 'M')
			size *= 2048;
		    Create_Chunk(d, chunk_info[current_chunk]->offset, size, freebsd, 3,
				 (chunk_info[current_chunk]->flags & CHUNK_ALIGN));
		    variable_set2(DISK_PARTITIONED, "yes");
		    record_chunks(d);
		}
	    }
	    break;

	case 'D':
	    if (chunk_info[current_chunk]->type == unused)
		msg = "Partition is already unused!";
	    else {
		Delete_Chunk(d, chunk_info[current_chunk]);
		variable_set2(DISK_PARTITIONED, "yes");
		record_chunks(d);
	    }
	    break;

	case 'G': {
	    char *val, geometry[80];

	    snprintf(geometry, 80, "%lu/%lu/%lu", d->bios_cyl, d->bios_hd, d->bios_sect);
	    val = msgGetInput(geometry, "Please specify the new geometry in cyl/hd/sect format.\n"
			      "Don't forget to use the two slash (/) separator characters!\n"
			      "It's not possible to parse the field without them.");
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
	    clear();
	    if (msgYesNo("Are you SURE you want to Undo everything?"))
		break;
	    d = Open_Disk(d->name);
	    if (!d) {
		msgConfirm("Can't reopen disk %s! Internal state is probably corrupted", d->name);
		return;
	    }
	    Free_Disk(dev->private);
	    dev->private = d;
	    variable_unset(DISK_PARTITIONED);
	    record_chunks(d);
	    break;

	case 'W':
	    if (!msgYesNo("Are you sure you want to write this now?  You do also\n"
			  "have the option of not modifying the disk until *all*\n"
			  "configuration information has been entered, at which\n"
			  "point you can do it all at once.  If you're unsure, then\n"
			  "choose No at this dialog.")) {
		variable_set2(DISK_PARTITIONED, "yes");
		clear();
		if (diskPartitionWrite(NULL) != RET_SUCCESS)
		    msgConfirm("Disk partition write returned an error status!");
		else
		    msgInfo("Wrote FDISK partition information out successfully.");
	    }
	    break;

	case '|':
	    if (!msgYesNo("Are you SURE you want to go into Wizard mode?\n"
			  "No seat belts whatsoever are provided!")) {
		dialog_clear();
		end_dialog();
		DialogActive = FALSE;
		slice_wizard(d);
		variable_set2(DISK_PARTITIONED, "yes");
		dialog_clear();
		DialogActive = TRUE;
		record_chunks(d);
	    }
	    else
		msg = "Wise choice!";
	    break;

	case 'Q':
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
	devs[0]->enabled = TRUE;
	diskPartition(devs[0], (Disk *)devs[0]->private);
	str = cp;
    }
    return devs ? 1 : 0;
}

int
diskPartitionEditor(char *str)
{
    DMenu *menu;
    Device **devs;
    int i, cnt;

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    cnt = deviceCount(devs);
    if (!cnt) {
	msgConfirm("No disks found!  Please verify that your disk controller is being\n"
		   "properly probed at boot time.  See the Hardware Guide on the\n"
		   "Documentation menu for clues on diagnosing this type of problem.");
	i = RET_FAIL;
    }
    else if (cnt == 1) {
	devs[0]->enabled = TRUE;
	diskPartition(devs[0], (Disk *)devs[0]->private);
	i = RET_SUCCESS;
    }
    else {
	menu = deviceCreateMenu(&MenuDiskDevices, DEVICE_TYPE_DISK, partitionHook);
	if (!menu) {
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
		i = RET_SUCCESS;
	    free(menu);
	}
    }
    return i;
}

static u_char *
getBootMgr(int disk)
{
    extern u_char mbr[], bteasy17[];
    char str[80];

    /* Figure out what kind of MBR the user wants */
    sprintf(str, "Install Boot Manager for %s drive?", disk == 0 ? "first" :
	    disk == 1 ? "second" : "<illegal>");
    MenuMBRType.title = str;
    if (dmenuOpenSimple(&MenuMBRType)) {
	switch (BootMgr) {
	case 0:
	    return bteasy17;

	case 1:
	    return mbr;

	case 2:
	default:
	    break;
	}
    }
    return NULL;
}

int
diskPartitionWrite(char *str)
{
    extern u_char boot1[], boot2[];
    u_char *mbrContents;
    Device **devs;
    char *cp;
    int i;

    if ((cp = variable_get(DISK_PARTITIONED)) && strcmp(cp, "yes"))
	return RET_SUCCESS;
    else if (!cp) {
	msgConfirm("You must partition the disk(s) before this option can be used.");
	return RET_FAIL;
    }

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    if (!devs) {
	msgConfirm("Unable to find any disks to write to??");
	return RET_FAIL;
    }

    for (i = 0; devs[i]; i++) {
	Chunk *c1;
	Disk *d = (Disk *)devs[i]->private;

	if (!devs[i]->enabled)
	    continue;

 	/* Don't trash the MBR if the first (and therefore only) chunk is marked for a truly dedicated
 	   disk (i.e., the disklabel starts at sector 0), even in cases where the user has requested
 	   booteasy or a "standard" MBR -- both would be fatal in this case. */
 	if (i < 2 && (mbrContents = getBootMgr(i)) != NULL && (d->chunks->part->flags & CHUNK_FORCE_ALL) != CHUNK_FORCE_ALL)
	    Set_Boot_Mgr(d, mbrContents);

	Set_Boot_Blocks(d, boot1, boot2);
	msgNotify("Writing partition information to drive %s", d->name);
	if (Write_Disk(d)) {
	    msgConfirm("ERROR: Unable to write data to disk %s!", d->name);
	    return RET_FAIL;
	}
	/* Now scan for bad blocks, if necessary */
	for (c1 = d->chunks->part; c1; c1 = c1->next) {
	    if (c1->flags & CHUNK_BAD144) {
		int ret;

		msgNotify("Running bad block scan on partition %s", c1->name);
		ret = vsystem("bad144 -v /dev/r%s 1234", c1->name);
		if (ret)
		    msgConfirm("Bad144 init on %s returned status of %d!", c1->name, ret);
		ret = vsystem("bad144 -v -s /dev/r%s", c1->name);
		if (ret)
		    msgConfirm("Bad144 scan on %s returned status of %d!", c1->name, ret);
	    }
	}
    }
    /* Now it's not "yes", but "written" */
    variable_set2(DISK_PARTITIONED, "written");
    return RET_SUCCESS;
}
