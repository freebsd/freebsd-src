/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: disks.c,v 1.72 1996/11/07 16:40:10 jkh Exp $
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

/* Where we start displaying chunk information on the screen */
#define CHUNK_START_ROW		5

/* Where we keep track of MBR chunks */
static struct chunk *chunk_info[16];
static int current_chunk;

static void
record_chunks(Disk *d)
{
    struct chunk *c1 = NULL;
    int i = 0;
    int last_free = 0;

    if (!d->chunks)
	msgFatal("No chunk list found for %s!", d->name);

    for (c1 = d->chunks->part; c1; c1 = c1->next) {
	if (c1->type == unused && c1->size > last_free) {
	    last_free = c1->size;
	    current_chunk = i;
	}
	chunk_info[i++] = c1;
    }
    chunk_info[i] = NULL;
    if (current_chunk >= i)
	current_chunk = i - 1;
}

static int Total;

static void
print_chunks(Disk *d)
{
    int row;
    int i;

    for (i = Total = 0; chunk_info[i]; i++)
	Total += chunk_info[i]->size;
    if (d->bios_cyl > 65536 || d->bios_hd > 256 || d->bios_sect >= 64) {
	dialog_clear_norefresh();
	msgConfirm("WARNING:  A geometry of %d/%d/%d for %s is incorrect.  Using\n"
		   "a more likely geometry.  If this geometry is incorrect or you\n"
		   "are unsure as to whether or not it's correct, please consult\n"
		   "the Hardware Guide in the Documentation submenu or use the\n"
		   " (G)eometry command to change it now.",
	  d->bios_cyl, d->bios_hd, d->bios_sect, d->name);
	Sanitize_Bios_Geom(d);
    }
    attrset(A_NORMAL);
    mvaddstr(0, 0, "Disk name:\t");
    clrtobot();
    attrset(A_REVERSE); addstr(d->name); attrset(A_NORMAL);
    attrset(A_REVERSE); mvaddstr(0, 55, "FDISK Partition Editor"); attrset(A_NORMAL);
    mvprintw(1, 0,
	     "DISK Geometry:\t%lu cyls/%lu heads/%lu sectors = %lu sectors",
	     d->bios_cyl, d->bios_hd, d->bios_sect,
	     d->bios_cyl * d->bios_hd * d->bios_sect);
    mvprintw(3, 1, "%10s %10s %10s %8s %8s %8s %8s %8s",
	     "Offset", "Size", "End", "Name", "PType", "Desc",
	     "Subtype", "Flags");
    for (i = 0, row = CHUNK_START_ROW; chunk_info[i]; i++, row++) {
	if (i == current_chunk)
	    attrset(ATTR_SELECTED);
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
    mvprintw(18, 0, "U = Undo All Changes   Q = Finish");
    if (!RunningAsInit)
	mvprintw(18, 46, "W = Write Changes");
    mvprintw(21, 0, "Use F1 or ? to get more help, arrow keys to select.");
    move(0, 0);
}

static u_char *
getBootMgr(char *dname)
{
    extern u_char mbr[], bteasy17[];
    char str[80];
    char *cp;
    int i = 0;

    cp = variable_get(VAR_BOOTMGR);
    if (!cp) {
	/* Figure out what kind of MBR the user wants */
	sprintf(str, "Install Boot Manager for drive %s?", dname);
	MenuMBRType.title = str;
	i = dmenuOpenSimple(&MenuMBRType, FALSE);
    }
    else {
	if (!strncmp(cp, "boot", 4))
	    BootMgr = 0;
	else if (!strcmp(cp, "standard"))
	    BootMgr = 1;
	else
	    BootMgr = 2;
    }
    if (cp || i) {
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

void
diskPartition(Device *dev, Disk *d)
{
    char *cp, *p;
    int rv, key = 0;
    Boolean chunking;
    char *msg = NULL;
    u_char *mbrContents;
    WINDOW *w = savescr();

    chunking = TRUE;
    keypad(stdscr, TRUE);

    /* Flush both the dialog and curses library views of the screen
       since we don't always know who called us */
    dialog_clear_norefresh(), clear();
    current_chunk = 0;

    /* Set up the chunk array */
    record_chunks(d);

    while (chunking) {
	char *val, geometry[80];
	    
	/* Now print our overall state */
	print_chunks(d);
	print_command_summary();
	if (msg) {
	    attrset(title_attr); mvprintw(23, 0, msg); attrset(A_NORMAL);
	    beep();
	    msg = NULL;
	}
	else {
	    move(23, 0);
	    clrtoeol();
	}

	/* Get command character */
	key = getch();
	switch (toupper(key)) {
	case '\014':	/* ^L (redraw) */
	    clear();
	    msg = NULL;
	    break;
	    
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
	    clear();
	    break;

	case 'A':
	    rv = msgYesNo("Do you want to do this with a true partition entry\n"
			  "so as to remain cooperative with any future possible\n"
			  "operating systems on the drive(s)?");
	    if (rv != 0) {
		rv = !msgYesNo("This is dangerous in that it will make the drive totally\n"
			       "uncooperative with other potential operating systems on the\n"
			       "same disk.  It will lead instead to a totally dedicated disk,\n"
			       "starting at the very first sector, bypassing all BIOS geometry\n"
			       "considerations.  This precludes the existance of any boot\n"
			       "manager or other stuff in sector 0, since the BSD bootstrap\n"
			       "will live there.\n"
			       "You will run into serious trouble with ST-506 and ESDI drives\n"
			       "and possibly some IDE drives (e.g. drives running under the\n"
			       "control of sort of disk manager).  SCSI drives are considerably\n"
			       "less at risk.\n\n"
			       "Do you insist on dedicating the entire disk this way?");
	    }
	    if (rv == -1)
		rv = 0;
	    All_FreeBSD(d, rv);
	    variable_set2(DISK_PARTITIONED, "yes");
	    record_chunks(d);
	    clear();
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
	    clear();
	    break;
	    
	case 'C':
	    if (chunk_info[current_chunk]->type != unused)
		msg = "Partition in use, delete it first or move to an unused one.";
	    else {
		char *val, tmp[20], *cp;
		int size, subtype;
		chunk_e partitiontype;
		
		snprintf(tmp, 20, "%d", chunk_info[current_chunk]->size);
		val = msgGetInput(tmp, "Please specify the size for new FreeBSD partition in blocks\n"
				  "or append a trailing `M' for megabytes (e.g. 20M).");
		if (val && (size = strtol(val, &cp, 0)) > 0) {
		    if (*cp && toupper(*cp) == 'M')
			size *= ONE_MEG;
		    strcpy(tmp, "165");
		    val = msgGetInput(tmp, "Enter type of partition to create:\n\n"
				      "Pressing Enter will choose the default, a native FreeBSD\n"
				      "partition (type 165).  You can choose other types, 6 for a\n"
				      "DOS partition or 131 for a Linux partition, for example.\n\n"
				      "Note:  If you choose a non-FreeBSD partition type, it will not\n"
				      "be formatted or otherwise prepared, it will simply reserve space\n"
				      "for you to use another tool, such as DOS FORMAT, to later format\n"
				      "and use the partition.");
		    if (val && (subtype = strtol(val, NULL, 0)) > 0) {
			if (subtype==165)
			    partitiontype=freebsd;
			else if (subtype==6)
			    partitiontype=fat;
			else
			    partitiontype=unknown;
			Create_Chunk(d, chunk_info[current_chunk]->offset, size, partitiontype, subtype,
				     (chunk_info[current_chunk]->flags & CHUNK_ALIGN));
			variable_set2(DISK_PARTITIONED, "yes");
			record_chunks(d);
		    }
		}
		clear();
	    }
	    break;
	    
	case KEY_DC:
	case 'D':
	    if (chunk_info[current_chunk]->type == unused)
		msg = "Partition is already unused!";
	    else {
		Delete_Chunk(d, chunk_info[current_chunk]);
		variable_set2(DISK_PARTITIONED, "yes");
		record_chunks(d);
	    }
	    break;
	    
	case 'G':
	    snprintf(geometry, 80, "%lu/%lu/%lu", d->bios_cyl, d->bios_hd, d->bios_sect);
	    val = msgGetInput(geometry, "Please specify the new geometry in cyl/hd/sect format.\n"
			      "Don't forget to use the two slash (/) separator characters!\n"
			      "It's not possible to parse the field without them.");
	    if (val) {
		long nc, nh, ns;
		nc = strtol(val, &val, 0);
		nh = strtol(val + 1, &val, 0);
		ns = strtol(val + 1, 0, 0);
		Set_Bios_Geom(d, nc, nh, ns);
	    }
	    clear();
	    break;
	
	case 'S':
	    /* Set Bootable */
	    chunk_info[current_chunk]->flags |= CHUNK_ACTIVE;
	    break;
	
	case 'U':
	    if ((cp = variable_get(DISK_LABELLED)) && !strcmp(cp, "written")) {
		msgConfirm("You've already written this information out - you\n"
			   "can't undo it.");
	    }
	    else if (!msgYesNo("Are you SURE you want to Undo everything?")) {
		d = Open_Disk(d->name);
		if (!d) {
		    msgConfirm("Can't reopen disk %s! Internal state is probably corrupted", d->name);
		    clear();
		    break;
		}
		Free_Disk(dev->private);
		dev->private = d;
		variable_unset(DISK_PARTITIONED);
		variable_unset(DISK_LABELLED);
		record_chunks(d);
	    }
	    clear();
	    break;

	case 'W':
	    if ((cp = variable_get(DISK_LABELLED)) && !strcmp(cp, "written")) {
		msgConfirm("You've already written this information out - if\n"
			   "you wish to overwrite it, you'll have to restart.");
	    }
	    else if (!msgYesNo("WARNING:  This should only be used when modifying an EXISTING\n"
			       "installation.  If you are installing FreeBSD for the first time\n"
			       "then you should simply type Q when you're finished here and your\n"
			       "changes will be committed in one batch automatically at the end of\n"
			       "these questions.\n\n"
			       "Are you absolutely sure you want to do this now?")) {
		variable_set2(DISK_PARTITIONED, "yes");
		
		/* Don't trash the MBR if the first (and therefore only) chunk is marked for a truly dedicated
		 * disk (i.e., the disklabel starts at sector 0), even in cases where the user has requested
		 * booteasy or a "standard" MBR -- both would be fatal in this case.
		 */
		if ((d->chunks->part->flags & CHUNK_FORCE_ALL) != CHUNK_FORCE_ALL
		    && (mbrContents = getBootMgr(d->name)) != NULL)
		    Set_Boot_Mgr(d, mbrContents);
		
		if (DITEM_STATUS(diskPartitionWrite(NULL)) != DITEM_SUCCESS)
		    msgConfirm("Disk partition write returned an error status!");
		else
		    msgConfirm("Wrote FDISK partition information out successfully.");
	    }
	    clear();
	    break;
	    
	case '|':
	    if (!msgYesNo("Are you SURE you want to go into Wizard mode?\n"
			  "No seat belts whatsoever are provided!")) {
		clear();
		refresh();
		slice_wizard(d);
		variable_set2(DISK_PARTITIONED, "yes");
		record_chunks(d);
	    }
	    else
		msg = "Wise choice!";
	    clear();
	    break;

	case 'Q':
	    chunking = FALSE;
	    /* Don't trash the MBR if the first (and therefore only) chunk is marked for a truly dedicated
	     * disk (i.e., the disklabel starts at sector 0), even in cases where the user has requested
	     * booteasy or a "standard" MBR -- both would be fatal in this case.
	     */
	    if ((d->chunks->part->flags & CHUNK_FORCE_ALL) != CHUNK_FORCE_ALL
		&& (mbrContents = getBootMgr(d->name)) != NULL)
		Set_Boot_Mgr(d, mbrContents);
	    break;
	    
	default:
	    beep();
	    msg = "Type F1 or ? for help";
	    break;
	}
    }
    p = CheckRules(d);
    if (p) {
	char buf[FILENAME_MAX];
	
	dialog_clear_norefresh();
        use_helpline("Press F1 to read more about disk partitioning.");
	use_helpfile(systemHelpFile("partition", buf));
	dialog_mesgbox("Disk partitioning warning:", p, -1, -1);
	free(p);
    }
    restorescr(w);
}

static int
partitionHook(dialogMenuItem *selected)
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
	diskPartition(devs[0], (Disk *)devs[0]->private);
    }
    else
	devs[0]->enabled = FALSE;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
partitionCheck(dialogMenuItem *selected)
{
    Device **devs = NULL;

    devs = deviceFind(selected->prompt, DEVICE_TYPE_DISK);
    if (!devs || devs[0]->enabled == FALSE)
	return FALSE;
    return TRUE;
}

int
diskPartitionEditor(dialogMenuItem *self)
{
    DMenu *menu;
    Device **devs;
    int i, cnt;
    char *cp;

    cp = variable_get(VAR_DISK);
    devs = deviceFind(cp, DEVICE_TYPE_DISK);
    cnt = deviceCount(devs);
    if (!cnt) {
	msgConfirm("No disks found!  Please verify that your disk controller is being\n"
		   "properly probed at boot time.  See the Hardware Guide on the\n"
		   "Documentation menu for clues on diagnosing this type of problem.");
	i = DITEM_FAILURE;
    }
    else if (cnt == 1) {
	devs[0]->enabled = TRUE;
	diskPartition(devs[0], (Disk *)devs[0]->private);
	i = DITEM_SUCCESS;
    }
    else {
	menu = deviceCreateMenu(&MenuDiskDevices, DEVICE_TYPE_DISK, partitionHook, partitionCheck);
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
	i = i | DITEM_RECREATE;
    }
    return i;
}

int
diskPartitionWrite(dialogMenuItem *self)
{
    Device **devs;
    char *cp;
    int i;

    if ((cp = variable_get(DISK_PARTITIONED)) && strcmp(cp, "yes"))
	return DITEM_SUCCESS;
    else if (!cp) {
	msgConfirm("You must partition the disk(s) before this option can be used.");
	return DITEM_FAILURE;
    }

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    if (!devs) {
	msgConfirm("Unable to find any disks to write to??");
	return DITEM_FAILURE;
    }
    if (isDebug())
	msgDebug("diskPartitionWrite: Examining %d devices\n", deviceCount(devs));

    for (i = 0; devs[i]; i++) {
	Chunk *c1;
	Disk *d = (Disk *)devs[i]->private;

	if (!devs[i]->enabled)
	    continue;

	Set_Boot_Blocks(d, boot1, boot2);
	msgNotify("Writing partition information to drive %s", d->name);
	if (!Fake && Write_Disk(d)) {
	    msgConfirm("ERROR: Unable to write data to disk %s!", d->name);
	    return DITEM_FAILURE;
	}
	/* Now scan for bad blocks, if necessary */
	for (c1 = d->chunks->part; c1; c1 = c1->next) {
	    if (c1->flags & CHUNK_BAD144) {
		int ret;

		msgNotify("Running bad block scan on partition %s", c1->name);
		if (!Fake) {
		    ret = vsystem("bad144 -v /dev/r%s 1234", c1->name);
		    if (ret)
			msgConfirm("Bad144 init on %s returned status of %d!", c1->name, ret);
		    ret = vsystem("bad144 -v -s /dev/r%s", c1->name);
		    if (ret)
			msgConfirm("Bad144 scan on %s returned status of %d!", c1->name, ret);
		}
	    }
	}
    }
    /* Now it's not "yes", but "written" */
    variable_set2(DISK_PARTITIONED, "written");
    return DITEM_SUCCESS;
}
