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
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/disklabel.h>

enum size_units_t { UNIT_BLOCKS, UNIT_KILO, UNIT_MEG, UNIT_SIZE };

#ifdef PC98
#define	SUBTYPE_FREEBSD		50324
#define	SUBTYPE_FAT		37218
#else
#define	SUBTYPE_FREEBSD		165
#define	SUBTYPE_FAT		6
#endif

/* Where we start displaying chunk information on the screen */
#define CHUNK_START_ROW		5

/* Where we keep track of MBR chunks */
static struct chunk *chunk_info[16];
static int current_chunk;

static void	diskPartitionNonInteractive(Device *dev);
static u_char *	bootalloc(char *name, size_t *size);

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
print_chunks(Disk *d, int u)
{
    int row;
    int i;
    int sz;
    char *szstr;

    szstr = (u == UNIT_MEG ? "MB" : (u == UNIT_KILO ? "KB" : "ST"));

    for (i = Total = 0; chunk_info[i]; i++)
	Total += chunk_info[i]->size;
#ifndef PC98
    if (d->bios_cyl > 65536 || d->bios_hd > 256 || d->bios_sect >= 64) {
	dialog_clear_norefresh();
	msgConfirm("WARNING:  A geometry of %lu/%lu/%lu for %s is incorrect.  Using\n"
		   "a more likely geometry.  If this geometry is incorrect or you\n"
		   "are unsure as to whether or not it's correct, please consult\n"
		   "the Hardware Guide in the Documentation submenu or use the\n"
		   "(G)eometry command to change it now.\n\n"
		   "Remember: you need to enter whatever your BIOS thinks the\n"
		   "geometry is!  For IDE, it's what you were told in the BIOS\n"
		   "setup. For SCSI, it's the translation mode your controller is\n"
		   "using.  Do NOT use a ``physical geometry''.",
	  d->bios_cyl, d->bios_hd, d->bios_sect, d->name);
	Sanitize_Bios_Geom(d);
    }
#endif
    attrset(A_NORMAL);
    mvaddstr(0, 0, "Disk name:\t");
    clrtobot();
    attrset(A_REVERSE); addstr(d->name); attrset(A_NORMAL);
    attrset(A_REVERSE); mvaddstr(0, 55, "FDISK Partition Editor"); attrset(A_NORMAL);
    mvprintw(1, 0,
	     "DISK Geometry:\t%lu cyls/%lu heads/%lu sectors = %lu sectors (%luMB)",
	     d->bios_cyl, d->bios_hd, d->bios_sect,
	     d->bios_cyl * d->bios_hd * d->bios_sect,
	     d->bios_cyl * d->bios_hd * d->bios_sect / (1024/512) / 1024);
    mvprintw(3, 0, "%6s %10s(%s) %10s %8s %6s %10s %8s %8s",
	     "Offset", "Size", szstr, "End", "Name", "PType", "Desc",
	     "Subtype", "Flags");
    for (i = 0, row = CHUNK_START_ROW; chunk_info[i]; i++, row++) {
	switch(u) {
	default:	/* fall thru */
	case UNIT_BLOCKS:
	    sz = chunk_info[i]->size;
	    break;
	case UNIT_KILO:
	    sz = chunk_info[i]->size / (1024/512);
	    break;
	case UNIT_MEG:
	    sz = chunk_info[i]->size / (1024/512) / 1024;
	    break;
	}
	if (i == current_chunk)
	    attrset(ATTR_SELECTED);
	mvprintw(row, 0, "%10ld %10lu %10lu %8s %6d %10s %8d\t%-6s",
		 chunk_info[i]->offset, sz,
		 chunk_info[i]->end, chunk_info[i]->name,
		 chunk_info[i]->type, 
		 slice_type_name(chunk_info[i]->type, chunk_info[i]->subtype),
		 chunk_info[i]->subtype, ShowChunkFlags(chunk_info[i]));
	if (i == current_chunk)
	    attrset(A_NORMAL);
    }
}

static void
print_command_summary()
{
    mvprintw(14, 0, "The following commands are supported (in upper or lower case):");
    mvprintw(16, 0, "A = Use Entire Disk   G = set Drive Geometry   C = Create Slice   F = `DD' mode");
    mvprintw(17, 0, "D = Delete Slice      Z = Toggle Size Units    S = Set Bootable   | = Wizard m.");
    mvprintw(18, 0, "T = Change Type       U = Undo All Changes     Q = Finish");
    if (!RunningAsInit)
	mvprintw(18, 47, "W = Write Changes");
    mvprintw(21, 0, "Use F1 or ? to get more help, arrow keys to select.");
    move(0, 0);
}

#ifdef PC98
static void
getBootMgr(char *dname, u_char **bootipl, size_t *bootipl_size,
	   u_char **bootmenu, size_t *bootmenu_size)
{
    static u_char *boot0;
    static size_t boot0_size;
    static u_char *boot05;
    static size_t boot05_size;

    char str[80];
    char *cp;
    int i = 0;

    cp = variable_get(VAR_BOOTMGR);
    if (!cp) {
	/* Figure out what kind of MBR the user wants */
	sprintf(str, "Install Boot Manager for drive %s?", dname);
	MenuMBRType.title = str;
	i = dmenuOpenSimple(&MenuMBRType, FALSE);
    } else {
	if (!strncmp(cp, "boot", 4))
	    BootMgr = 0;
	else
	    BootMgr = 2;
    }
    if (cp || i) {
	switch (BootMgr) {
	case 0:
	    if (!boot0) boot0 = bootalloc("boot0", &boot0_size);
	    *bootipl = boot0;
	    *bootipl_size = boot0_size;
	    if (!boot05) boot05 = bootalloc("boot0.5", &boot05_size);
	    *bootmenu = boot05;
	    *bootmenu_size = boot05_size;
	    return;
	case 2:
	default:
	    break;
	}
    }
    *bootipl = NULL;
    *bootipl_size = 0;
    *bootmenu = NULL;
    *bootmenu_size = 0;
}
#else
static void
getBootMgr(char *dname, u_char **bootCode, size_t *bootCodeSize)
{
#ifndef __alpha__	/* only meaningful on x86 */
    static u_char *mbr, *boot0;
    static size_t mbr_size, boot0_size;
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
	    if (!boot0) boot0 = bootalloc("boot0", &boot0_size);
	    *bootCode = boot0;
	    *bootCodeSize = boot0_size;
	    return;
	case 1:
	    if (!mbr) mbr = bootalloc("mbr", &mbr_size);
	    *bootCode = mbr;
	    *bootCodeSize = mbr_size;
	    return;
	case 2:
	default:
	    break;
	}
    }
#endif
    *bootCode = NULL;
    *bootCodeSize = 0;
}
#endif

int
diskGetSelectCount(Device ***devs)
{
    int i, cnt, enabled;
    char *cp;
    Device **dp;

    cp = variable_get(VAR_DISK);
    dp = *devs = deviceFind(cp, DEVICE_TYPE_DISK);
    cnt = deviceCount(dp);
    if (!cnt)
	return -1;
    for (i = 0, enabled = 0; i < cnt; i++) {
	if (dp[i]->enabled)
	    ++enabled;
    }
    return enabled;
}

void
diskPartition(Device *dev)
{
    char *cp, *p;
    int rv, key = 0;
    Boolean chunking;
    char *msg = NULL;
#ifdef PC98
    u_char *bootipl;
    size_t bootipl_size;
    u_char *bootmenu;
    size_t bootmenu_size;
#else
    u_char *mbrContents;
    size_t mbrSize;
#endif
    WINDOW *w = savescr();
    Disk *d = (Disk *)dev->private;
    int size_unit;

    size_unit = UNIT_BLOCKS;
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
	if (d)
	    print_chunks(d, size_unit);
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
	    
	case '\020':	/* ^P */
	case KEY_UP:
	case '-':
	    if (current_chunk != 0)
		--current_chunk;
	    break;
	    
	case '\016':	/* ^N */
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
	case 'F':	/* Undocumented magic Dangerously Dedicated mode */
#ifdef __alpha__
	    rv = 1;
#else	    /* The rest is only relevant on x86 */
	    cp = variable_get(VAR_DEDICATE_DISK);
	    if (cp && !strcasecmp(cp, "always"))
		rv = 1;
	    else if (toupper(key) == 'A')
		rv = 0;
	    else {
		rv = msgYesNo("Do you want to do this with a true partition entry\n"
			      "so as to remain cooperative with any future possible\n"
			      "operating systems on the drive(s)?\n"
			      "(See also the section about ``dangerously dedicated''\n"
			      "disks in the FreeBSD FAQ.)");
		if (rv == -1)
		    rv = 0;
	    }
#endif
	    All_FreeBSD(d, rv);
	    variable_set2(DISK_PARTITIONED, "yes", 0);
	    record_chunks(d);
	    clear();
	    break;
	    
	case 'C':
	    if (chunk_info[current_chunk]->type != unused)
		msg = "Slice in use, delete it first or move to an unused one.";
	    else {
		char *val, tmp[20], *cp;
		int size;
#ifdef PC98
		char name[16];

		snprintf(name, 16, "%s", "FreeBSD");
		val = msgGetInput(name,
			"Please specify the name for new FreeBSD slice.");
		if (val)
			strncpy(name, val, 16);
#else
		int subtype;
		chunk_e partitiontype;
#endif
		snprintf(tmp, 20, "%lu", chunk_info[current_chunk]->size);
		val = msgGetInput(tmp, "Please specify the size for new FreeBSD slice in blocks\n"
				  "or append a trailing `M' for megabytes (e.g. 20M).");
		if (val && (size = strtol(val, &cp, 0)) > 0) {
		    if (*cp && toupper(*cp) == 'M')
			size *= ONE_MEG;
		    else if (*cp && toupper(*cp) == 'G')
			size *= ONE_GIG;
#ifdef PC98
		    Create_Chunk(d, chunk_info[current_chunk]->offset, size,
			freebsd, 3,
			(chunk_info[current_chunk]->flags & CHUNK_ALIGN),
			name);
		    variable_set2(DISK_PARTITIONED, "yes", 0);
		    record_chunks(d);
#else
		    sprintf(tmp, "%d", SUBTYPE_FREEBSD);
		    val = msgGetInput(tmp, "Enter type of partition to create:\n\n"
				      "Pressing Enter will choose the default, a native FreeBSD\n"
				      "slice (type 165).  You can choose other types, 6 for a\n"
				      "DOS partition or 131 for a Linux partition, for example.\n\n"
				      "Note:  If you choose a non-FreeBSD partition type, it will not\n"
				      "be formatted or otherwise prepared, it will simply reserve space\n"
				      "for you to use another tool, such as DOS FORMAT, to later format\n"
				      "and use the partition.");
		    if (val && (subtype = strtol(val, NULL, 0)) > 0) {
			if (subtype == SUBTYPE_FREEBSD)
			    partitiontype = freebsd;
			else if (subtype == SUBTYPE_FAT)
			    partitiontype = fat;
			else
			    partitiontype = unknown;
#ifdef __alpha__
			if (partitiontype == freebsd && size == chunk_info[current_chunk]->size)
			    All_FreeBSD(d, 1);
			else
#endif
			Create_Chunk(d, chunk_info[current_chunk]->offset, size, partitiontype, subtype,
				     (chunk_info[current_chunk]->flags & CHUNK_ALIGN));
			variable_set2(DISK_PARTITIONED, "yes", 0);
			record_chunks(d);
		    }
#endif /* PC98 */
		}
		clear();
	    }
	    break;
	    
	case KEY_DC:
	case 'D':
	    if (chunk_info[current_chunk]->type == unused)
		msg = "Slice is already unused!";
	    else {
		Delete_Chunk(d, chunk_info[current_chunk]);
		variable_set2(DISK_PARTITIONED, "yes", 0);
		record_chunks(d);
	    }
	    break;
	    
	case 'T':
	    if (chunk_info[current_chunk]->type == unused)
		msg = "Slice is currently unused (use create instead)";
	    else {
		char *val, tmp[20];
		int subtype;
		chunk_e partitiontype;

		sprintf(tmp, "%d", SUBTYPE_FREEBSD);
#ifdef PC98
		val = msgGetInput(tmp, "New partition type:\n\n"
				  "Pressing Enter will choose the default, a native FreeBSD\n"
				  "slice (type 50324).  Other popular values are 37218 for\n"
				  "DOS FAT partition.\n\n"
				  "Note:  If you choose a non-FreeBSD partition type, it will not\n"
				  "be formatted or otherwise prepared, it will simply reserve space\n"
				  "for you to use another tool, such as DOS format, to later format\n"
				  "and actually use the partition.");
#else
		val = msgGetInput(tmp, "New partition type:\n\n"
				  "Pressing Enter will choose the default, a native FreeBSD\n"
				  "slice (type 165).  Other popular values are 6 for\n"
				  "DOS FAT partition, 131 for a Linux ext2fs partition or\n"
				  "130 for a Linux swap partition.\n\n"
				  "Note:  If you choose a non-FreeBSD partition type, it will not\n"
				  "be formatted or otherwise prepared, it will simply reserve space\n"
				  "for you to use another tool, such as DOS format, to later format\n"
				  "and actually use the partition.");
#endif /* PC98 */
		if (val && (subtype = strtol(val, NULL, 0)) > 0) {
		    if (subtype == SUBTYPE_FREEBSD)
			partitiontype = freebsd;
		    else if (subtype == SUBTYPE_FAT)
			partitiontype = fat;
		    else
			partitiontype = unknown;
		    chunk_info[current_chunk]->type = partitiontype;
		    chunk_info[current_chunk]->subtype = subtype;
		}
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
	    if (!variable_cmp(DISK_LABELLED, "written")) {
		msgConfirm("You've already written this information out - you\n"
			   "can't undo it.");
	    }
	    else if (!msgNoYes("Are you SURE you want to Undo everything?")) {
		char cp[BUFSIZ];

		sstrncpy(cp, d->name, sizeof cp);
		Free_Disk(dev->private);
		d = Open_Disk(cp);
		if (!d)
		    msgConfirm("Can't reopen disk %s! Internal state is probably corrupted", cp);
		dev->private = d;
		variable_unset(DISK_PARTITIONED);
		variable_unset(DISK_LABELLED);
		if (d)
		    record_chunks(d);
	    }
	    clear();
	    break;

	case 'W':
	    if (!msgNoYes("WARNING:  This should only be used when modifying an EXISTING\n"
			       "installation.  If you are installing FreeBSD for the first time\n"
			       "then you should simply type Q when you're finished here and your\n"
			       "changes will be committed in one batch automatically at the end of\n"
			       "these questions.  If you're adding a disk, you should NOT write\n"
			       "from this screen, you should do it from the label editor.\n\n"
			       "Are you absolutely sure you want to do this now?")) {
		variable_set2(DISK_PARTITIONED, "yes", 0);

		/*
		 * Don't trash the MBR if the first (and therefore only) chunk
		 * is marked for a truly dedicated disk (i.e., the disklabel
		 * starts at sector 0), even in cases where the user has
		 * requested booteasy or a "standard" MBR -- both would be
		 * fatal in this case.
		 */
		/*
		 * Don't offer to update the MBR on this disk if the first
		 * "real" chunk looks like a FreeBSD "all disk" partition,
		 * or the disk is entirely FreeBSD.
		 */
#ifdef PC98
		if ((d->chunks->part->type != freebsd) ||
		    (d->chunks->part->offset > 1))
		    getBootMgr(d->name, &bootipl, &bootipl_size,
			       &bootmenu, &bootmenu_size);
		else {
		    bootipl = NULL;
		    bootipl_size = 0;
		    bootmenu = NULL;
		    bootmenu_size = 0;
		}
		Set_Boot_Mgr(d, bootipl, bootipl_size, bootmenu, bootmenu_size);
#else
		if ((d->chunks->part->type != freebsd) ||
		    (d->chunks->part->offset > 1))
		    getBootMgr(d->name, &mbrContents, &mbrSize);
		else {
		    mbrContents = NULL;
		    mbrSize = 0;
		}
		Set_Boot_Mgr(d, mbrContents, mbrSize);
#endif

		if (DITEM_STATUS(diskPartitionWrite(NULL)) != DITEM_SUCCESS)
		    msgConfirm("Disk partition write returned an error status!");
		else
		    msgConfirm("Wrote FDISK partition information out successfully.");
	    }
	    clear();
	    break;
	    
	case '|':
	    if (!msgNoYes("Are you SURE you want to go into Wizard mode?\n"
			  "No seat belts whatsoever are provided!")) {
		clear();
		refresh();
		slice_wizard(d);
		variable_set2(DISK_PARTITIONED, "yes", 0);
		record_chunks(d);
	    }
	    else
		msg = "Wise choice!";
	    clear();
	    break;

	case '\033':	/* ESC */
	case 'Q':
	    chunking = FALSE;
	    /*
	     * Don't trash the MBR if the first (and therefore only) chunk
	     * is marked for a truly dedicated disk (i.e., the disklabel
	     * starts at sector 0), even in cases where the user has requested
	     * booteasy or a "standard" MBR -- both would be fatal in this case.
	     */
	    /*
	     * Don't offer to update the MBR on this disk if the first "real"
	     * chunk looks like a FreeBSD "all disk" partition, or the disk is
	     * entirely FreeBSD. 
	     */
	    if ((d->chunks->part->type != freebsd) ||
		(d->chunks->part->offset > 1)) {
		if (variable_cmp(DISK_PARTITIONED, "written")) {
#ifdef PC98
		    getBootMgr(d->name, &bootipl, &bootipl_size,
			&bootmenu, &bootmenu_size);
		    if (bootipl != NULL && bootmenu != NULL)
			Set_Boot_Mgr(d, bootipl, bootipl_size,
			    bootmenu, bootmenu_size);
#else
		    getBootMgr(d->name, &mbrContents, &mbrSize);
		    if (mbrContents != NULL)
			Set_Boot_Mgr(d, mbrContents, mbrSize);
#endif
		}
	    }
	    break;

	case 'Z':
	    size_unit = (size_unit + 1) % UNIT_SIZE;
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
	
        use_helpline("Press F1 to read more about disk slices.");
	use_helpfile(systemHelpFile("partition", buf));
	if (!variable_get(VAR_NO_WARN))
	    dialog_mesgbox("Disk slicing warning:", p, -1, -1);
	free(p);
    }
    restorescr(w);
}

static u_char *
bootalloc(char *name, size_t *size)
{
    char buf[FILENAME_MAX];
    struct stat sb;

    snprintf(buf, sizeof buf, "/boot/%s", name);
    if (stat(buf, &sb) != -1) {
	int fd;

	fd = open(buf, O_RDONLY);
	if (fd != -1) {
	    u_char *cp;

	    cp = malloc(sb.st_size);
	    if (read(fd, cp, sb.st_size) != sb.st_size) {
		free(cp);
		close(fd);
		msgDebug("bootalloc: couldn't read %ld bytes from %s\n", (long)sb.st_size, buf);
		return NULL;
	    }
	    close(fd);
	    if (size != NULL)
		*size = sb.st_size;
	    return cp;
	}
	msgDebug("bootalloc: couldn't open %s\n", buf);
    }
    else
	msgDebug("bootalloc: can't stat %s\n", buf);
    return NULL;
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
	diskPartition(devs[0]);
    }
    else
	devs[0]->enabled = FALSE;
    return DITEM_SUCCESS;
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
    int i, cnt, devcnt;

    cnt = diskGetSelectCount(&devs);
    devcnt = deviceCount(devs);
    if (cnt == -1) {
	msgConfirm("No disks found!  Please verify that your disk controller is being\n"
		   "properly probed at boot time.  See the Hardware Guide on the\n"
		   "Documentation menu for clues on diagnosing this type of problem.");
	return DITEM_FAILURE;
    }
    else if (cnt) {
	/* Some are already selected */
	for (i = 0; i < devcnt; i++) {
	    if (devs[i]->enabled) {
		if (variable_get(VAR_NONINTERACTIVE) &&
		  !variable_get(VAR_DISKINTERACTIVE))
		    diskPartitionNonInteractive(devs[i]);
		else
		    diskPartition(devs[i]);
	    }
	}
    }
    else {
	/* No disks are selected, fall-back case now */
	if (devcnt == 1) {
	    devs[0]->enabled = TRUE;
	    if (variable_get(VAR_NONINTERACTIVE) &&
	      !variable_get(VAR_DISKINTERACTIVE))
		diskPartitionNonInteractive(devs[0]);
	    else
		diskPartition(devs[0]);
	    return DITEM_SUCCESS;
	}
	else {
	    menu = deviceCreateMenu(&MenuDiskDevices, DEVICE_TYPE_DISK, partitionHook, partitionCheck);
	    if (!menu) {
		msgConfirm("No devices suitable for installation found!\n\n"
			   "Please verify that your disk controller (and attached drives)\n"
			   "were detected properly.  This can be done by pressing the\n"
			   "[Scroll Lock] key and using the Arrow keys to move back to\n"
			   "the boot messages.  Press [Scroll Lock] again to return.");
		return DITEM_FAILURE;
	    }
	    else {
		i = dmenuOpenSimple(menu, FALSE) ? DITEM_SUCCESS : DITEM_FAILURE;
		free(menu);
	    }
	    return i;
	}
    }
    return DITEM_SUCCESS;
}

int
diskPartitionWrite(dialogMenuItem *self)
{
    Device **devs;
    int i;

    if (!variable_cmp(DISK_PARTITIONED, "written"))
	return DITEM_SUCCESS;

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    if (!devs) {
	msgConfirm("Unable to find any disks to write to??");
	return DITEM_FAILURE;
    }
    if (isDebug())
	msgDebug("diskPartitionWrite: Examining %d devices\n", deviceCount(devs));
    for (i = 0; devs[i]; i++) {
	Disk *d = (Disk *)devs[i]->private;
	static u_char *boot1;
#ifndef __alpha__
	static u_char *boot2;
#endif

	if (!devs[i]->enabled)
	    continue;

#ifdef __alpha__
	if (!boot1) boot1 = bootalloc("boot1", NULL);
	Set_Boot_Blocks(d, boot1, NULL);
#else
	if (!boot1) boot1 = bootalloc("boot1", NULL);
	if (!boot2) boot2 = bootalloc("boot2", NULL);
	Set_Boot_Blocks(d, boot1, boot2);
#endif

	msgNotify("Writing partition information to drive %s", d->name);
	if (!Fake && Write_Disk(d)) {
	    msgConfirm("ERROR: Unable to write data to disk %s!", d->name);
	    return DITEM_FAILURE;
	}
    }
    /* Now it's not "yes", but "written" */
    variable_set2(DISK_PARTITIONED, "written", 0);
    return DITEM_SUCCESS | DITEM_RESTORE;
}

/* Partition a disk based wholly on which variables are set */
static void
diskPartitionNonInteractive(Device *dev)
{
    char *cp;
    int i, sz, all_disk = 0;
#ifdef PC98
    u_char *bootipl;
    size_t bootipl_size;
    u_char *bootmenu;
    size_t bootmenu_size;
#else
    u_char *mbrContents;
    size_t mbrSize;
#endif
    Disk *d = (Disk *)dev->private;

    record_chunks(d);
    cp = variable_get(VAR_GEOMETRY);
    if (cp) {
	msgDebug("Setting geometry from script to: %s\n", cp);
	d->bios_cyl = strtol(cp, &cp, 0);
	d->bios_hd = strtol(cp + 1, &cp, 0);
	d->bios_sect = strtol(cp + 1, 0, 0);
    }

    cp = variable_get(VAR_PARTITION);
    if (cp) {
	if (!strcmp(cp, "free")) {
	    /* Do free disk space case */
	    for (i = 0; chunk_info[i]; i++) {
		/* If a chunk is at least 10MB in size, use it. */
		if (chunk_info[i]->type == unused && chunk_info[i]->size > (10 * ONE_MEG)) {
#ifdef PC98
		    Create_Chunk(d, chunk_info[i]->offset, chunk_info[i]->size,
				 freebsd, 3,
				 (chunk_info[i]->flags & CHUNK_ALIGN),
				 "FreeBSD");
#else
		    Create_Chunk(d, chunk_info[i]->offset, chunk_info[i]->size,
				 freebsd, 3,
				 (chunk_info[i]->flags & CHUNK_ALIGN));
#endif
		    variable_set2(DISK_PARTITIONED, "yes", 0);
		    break;
		}
	    }
	    if (!chunk_info[i]) {
		msgConfirm("Unable to find any free space on this disk!");
		return;
	    }
	}
	else if (!strcmp(cp, "all")) {
	    /* Do all disk space case */
	    msgDebug("Warning:  Devoting all of disk %s to FreeBSD.\n", d->name);

	    All_FreeBSD(d, FALSE);
	}
	else if (!strcmp(cp, "exclusive")) {
	    /* Do really-all-the-disk-space case */
	    msgDebug("Warning:  Devoting all of disk %s to FreeBSD.\n", d->name);

	    All_FreeBSD(d, all_disk = TRUE);
	}
	else if ((sz = strtol(cp, &cp, 0))) {
	    /* Look for sz bytes free */
	    if (*cp && toupper(*cp) == 'M')
		sz *= ONE_MEG;
	    else if (*cp && toupper(*cp) == 'G')
		sz *= ONE_GIG;
	    for (i = 0; chunk_info[i]; i++) {
		/* If a chunk is at least sz MB, use it. */
		if (chunk_info[i]->type == unused && chunk_info[i]->size >= sz) {
#ifdef PC98
		    Create_Chunk(d, chunk_info[i]->offset, sz, freebsd, 3,
				 (chunk_info[i]->flags & CHUNK_ALIGN),
				 "FreeBSD");
#else
		    Create_Chunk(d, chunk_info[i]->offset, sz, freebsd, 3,
				 (chunk_info[i]->flags & CHUNK_ALIGN));
#endif
		    variable_set2(DISK_PARTITIONED, "yes", 0);
		    break;
		}
	    }
	    if (!chunk_info[i]) {
		msgConfirm("Unable to find %d free blocks on this disk!", sz);
		return;
	    }
	}
	else if (!strcmp(cp, "existing")) {
	    /* Do existing FreeBSD case */
	    for (i = 0; chunk_info[i]; i++) {
		if (chunk_info[i]->type == freebsd)
		    break;
	    }
	    if (!chunk_info[i]) {
		msgConfirm("Unable to find any existing FreeBSD partitions on this disk!");
		return;
	    }
	}
	else {
	    msgConfirm("`%s' is an invalid value for %s - is config file valid?", cp, VAR_PARTITION);
	    return;
	}
	if (!all_disk) {
#ifdef PC98
	    getBootMgr(d->name, &bootipl, &bootipl_size,
		       &bootmenu, &bootmenu_size);
	    Set_Boot_Mgr(d, bootipl, bootipl_size, bootmenu, bootmenu_size);
#else
	    getBootMgr(d->name, &mbrContents, &mbrSize);
	    Set_Boot_Mgr(d, mbrContents, mbrSize);
#endif
	}
	variable_set2(DISK_PARTITIONED, "yes", 0);
    }
}

