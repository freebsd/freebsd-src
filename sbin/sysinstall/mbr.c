/*
 * Copyright (c) 1994, Paul Richards.
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 */

#include <stdio.h>
#include <unistd.h>
#include <dialog.h>
#include <fcntl.h>
#include <fstab.h>

#ifdef __i386__ /* temp measure delete nov 15 1994 */
#define i386 1
#else
#warning FOO
#endif
#include <sys/types.h>
#include <sys/devconf.h>
#include <sys/disklabel.h>
#include <sys/uio.h>

#include "sysinstall.h"
#include "disk.h"
#include "editor.h"
#include "mbr.h"

char *part_type(int);
int write_mbr(char *, struct mbr *);
int read_mbr(char *, struct mbr *);
void show_mbr(struct mbr *);
int clear_mbr(struct mbr *, char *);
int build_mbr(struct mbr *, char *, struct disklabel *);

extern char boot1[];
extern struct disk disk_list[];
struct part_type part_types[] = PARTITION_TYPES
struct field *last_field;

char *
part_type(int type)
{
    int num_types = (sizeof(part_types)/sizeof(struct part_type));
    int next_type = 0;
    struct part_type *ptr = part_types;
    
    while (next_type < num_types) {
	if(ptr->type == type)
	    return(ptr->name);
	ptr++;
	next_type++;
    }
    return("Unknown");
}

int
read_mbr(char *device, struct mbr *mbr)
{
	int fd;

	if ((fd = open(device, O_RDONLY)) == -1) {
		sprintf(errmsg,
		        "Couldn't open device %s to read master boot record\n",
				  device);
		return(-1);
	}

	if (lseek(fd, 0, SEEK_SET) == -1) {
		sprintf(errmsg,
				  "Couldn't seek to track 0 of device %s to read master boot record\n",
				  device);
		return(-1);
	}

	if (read(fd, &(mbr->bootcode), MBRSIZE) == -1) {
		sprintf(errmsg, "Failed to read master boot record from device %s\n",
				  device);
		return(-1);
	}

	if (close(fd) == -1) {
		sprintf(errmsg,
				  "Couldn't close device %s after reading master boot record\n",
				  device);
		return(-1);
	}
	return(0);
}

int
write_mbr(char *device, struct mbr *mbr)
{
	int fd;

	if ((fd = open(device, O_WRONLY)) == -1) {
		sprintf(errmsg,
		        "Couldn't open device %s to write master boot record\n",
				  device);
		return(-1);
	}

	if (lseek(fd, 0, SEEK_SET) == -1) {
		sprintf(errmsg,
				  "Couldn't seek to track 0 of device %s to write master boot record\n",
				  device);
		return(-1);
	}
    
	if (enable_label(fd) == -1) {
		sprintf(errmsg,
				  "Couldn't write enable MBR area of device %s\n",
				  device);
		return(-1);
	}

	if (write(fd, mbr->bootcode, MBRSIZE) == -1) {
		sprintf(errmsg, "Failed to write master boot record to device %s\n",
				  device);
		return(-1);
	}

	if (disable_label(fd) == -1) {
		sprintf(errmsg,
				  "Couldn't write disable MBR area of device %s\n",
				  device);
		return(-1);
	}

	if (close(fd) == -1) {
		sprintf(errmsg,
				  "Couldn't close device %s after reading master boot record\n",
				  device);
		return(-1);
	}

	return(0);
}

int
clear_mbr(struct mbr *mbr, char *bootcode)
{
    int i;
    int fd;
    
    /*
     * Must replace any old bootcode that was read
     * from disk with our bootblocks.
     */
    
	TellEm("Loading MBR code from %s", bootcode);
	fd = open(bootcode, O_RDONLY);
	if (fd < 0) {
		sprintf(errmsg, "Couldn't open boot file %s\n", bootcode);
		return(-1);
	}  
    
	if (read(fd, mbr->bootcode, MBRSIZE) < 0) {
		sprintf(errmsg, "Couldn't read from boot file %s\n", bootcode);
		return(-1);
	}
    
	if (close(fd) == -1) {
		sprintf(errmsg, "Couldn't close boot file %s\n", bootcode);
		return(-1);
	}
    
	/* Create an empty partition table */

	for (i=0; i < NDOSPART; i++) {
		mbr->dospart[i].dp_flag = 0;
		mbr->dospart[i].dp_shd = 0;
		mbr->dospart[i].dp_ssect = 0;
		mbr->dospart[i].dp_scyl = 0;
		mbr->dospart[i].dp_typ = 0;
		mbr->dospart[i].dp_ehd = 0;
		mbr->dospart[i].dp_esect = 0;
		mbr->dospart[i].dp_ecyl = 0;
		mbr->dospart[i].dp_start = 0;
		mbr->dospart[i].dp_size = 0;
	}

	mbr->magic = MBR_MAGIC;
 
	dialog_clear();
	return(0);
}

int
dedicated_mbr(struct mbr *mbr, char *bootcode, struct disklabel *lbl)
{
	struct dos_partition *dp = &mbr->dospart[0];
    
	if (clear_mbr(mbr, bootcode) == -1)
		return(-1);
	dp->dp_scyl = 0;
	dp->dp_shd = 1;
	dp->dp_ssect = 1;
	dp->dp_ecyl = lbl->d_ncylinders - 1;
	dp->dp_ehd = lbl->d_ntracks - 1;
	dp->dp_esect = lbl->d_nsectors;
	dp->dp_start = (dp->dp_scyl * lbl->d_ntracks * lbl->d_nsectors) + 
	(dp->dp_shd * lbl->d_nsectors) +
	dp->dp_ssect - 1;
	dp->dp_size =
	(lbl->d_nsectors * lbl->d_ntracks * lbl->d_ncylinders) - dp->dp_start;
    
	dp->dp_typ = MBR_PTYPE_FreeBSD;
	dp->dp_flag = ACTIVE;

	return(0);
}

int
get_geom_values(int disk)
{
	WINDOW *window;
	struct disklabel *lbl = &disk_list[disk].lbl;
	int key = 0;
	int cur_field = 0;
	int next = 0;
	int done=0;

	struct field field[] = {
{2, 28, 06, 10, 01, 04, 01,  4, 01, "Unset", F_EDIT, 0, 0},
{4, 28, 06, 10, 02, 00, 02,  0, 02, "Unset", F_EDIT, 0, 0},
{6, 28, 06, 10, 03, 01, 03,  1, 03, "Unset", F_EDIT, 0, 0},
{10,  7,  2,  2, 4,  2,  4,  2,  4, "OK", F_BUTTON, 0, 0},
{10, 20,  6,  6,  0,  3,  0,  3,  0, "Cancel", F_BUTTON, 0, 0},
{0, 07, 24, 24, -1, -1, -1, -1, -1, "BIOS geometry parameters", F_TITLE, 0, 0},
{2, 02, 20, 20, -1, -1, -1, -1, -1, "Number of cylinders:", F_TITLE, 0, 0},
{4, 02, 25, 25, -1, -1, -1, -1, -1, "Number of tracks (heads):", F_TITLE, 0, 0},
{6, 02, 18, 18, -1, -1, -1, -1, -1, "Number of sectors:", F_TITLE, 0, 0}
	};

	if (!(window = newwin(14, 40, 5, 20))) {
		sprintf(errmsg, "Failed to open window for geometry editor");
		return (-1);
	};

	keypad(window, TRUE);

	dialog_clear_norefresh();
	draw_box(window, 0, 0, 14, 40, dialog_attr, border_attr);

	done = 0;
	while (!done && (key != ESC)) {
		sprintf(field[0].field, "%ld", lbl->d_ncylinders);
		sprintf(field[1].field, "%ld", lbl->d_ntracks);
		sprintf(field[2].field, "%ld", lbl->d_nsectors);

		disp_fields(window, field, sizeof(field)/sizeof(struct field));
		switch (field[cur_field].type) {
			case F_EDIT:
				key = line_edit(window, field[cur_field].y, field[cur_field].x,
							 field[cur_field].width, field[cur_field].maxlen,
							 item_selected_attr, 1, field[cur_field].field);
				break;
			case F_BUTTON:
				key = button_press(window, field[cur_field]);
				if (!key && !strcmp(field[cur_field].field, "OK")) {
					done = 1;
					continue;
				}
				if (!key && !strcmp(field[cur_field].field, "Cancel")) {
					sprintf(errmsg, "\nUser aborted.\n");
					dialog_clear_norefresh();
					return (-1);
				}
			case F_TOGGLE:
			case F_TITLE:
			default:
				break;
		}

		next = change_field(field[cur_field], key);
		if (next == -1)
			beep();
		else
			cur_field = next;

		lbl->d_ncylinders = atoi(field[0].field);
		lbl->d_ntracks = atoi(field[1].field);
		lbl->d_nsectors = atoi(field[2].field);
	}

	delwin(window);
	refresh();
	return (0);
}

int
edit_mbr(int disk)
{
	WINDOW *window;
	int cur_field;
	int i;
	int ok;
	int next;
	int key = 0;
	struct mbr *mbr = &disk_list[disk].mbr;

	/* Confirm disk parameters */
#ifdef 0
	dialog_msgbox("\nBIOS disk geometry values", "In order to setup the boot area of the disk it is necessary to know the BIOS values for the disk geometry i.e. the number of cylinders, heads and sectors. These values may be different form the real geometry of the disk, depending on whether or not your system uses geometry translation. At this stage it is the entries from the BIOS that are needed. If you do not know these they can be found by rebooting the machine and entering th BIOS setup routine. See you BIOS manual for details.\n", -1, -1, 1)
#endif
	if (get_geom_values(disk) == -1)
		return(-1);

	/* Read MBR from disk */

	sprintf(scratch, "/dev/r%s%dd", disk_list[disk].devconf->dc_name,
											  disk_list[disk].devconf->dc_unit);
	if (read_mbr(scratch, &disk_list[disk].mbr) == -1) {
		sprintf(scratch, "The following error occured while trying\nto read the master boot record:\n\n%s\nIn order to install FreeBSD a new master boot record\nwill have to be written which will mean all current\ndata on the hard disk will be lost.", errmsg);
		ok = 0;
		while (!ok) {
			AskAbort(scratch);
			if (!dialog_yesno(TITLE,
									"\nAre you sure you wish to proceed ?\n",
									-1, -1)) {
				dialog_clear();
				if (dedicated_mbr(mbr, boot1, &disk_list[disk].lbl) == -1) {
					sprintf(scratch, "\nCouldn't create new master boot record.\n\n%s", errmsg);
					return(-1);
				}
				ok = 1;
			}
		}
	}

	sprintf(scratch, "\nDo you wish to dedicate the whole disk to FreeBSD?\n\nDoing so will overwrite any existing data on the disk.\n");
	dialog_clear_norefresh();
	if (!dialog_yesno(TITLE, scratch, -1, -1))
		if (dedicated_mbr(mbr, boot1, &disk_list[disk].lbl) == -1) {
			sprintf(scratch, "\nCouldn't dedicate disk to FreeBSD.\n\n %s", errmsg);
			return(-1);
		}

	if (!(window = newwin(LINES, COLS, 0, 0))) {
		sprintf(errmsg, "Failed to open window for MBR editor\n");
		return (-1); 
	};

	keypad(window, TRUE); 

	dialog_clear_norefresh();
	draw_box(window, 0, 0, LINES, COLS, dialog_attr, border_attr);

	cur_field = 1;
	ok = 0;
	while (!ok && (key != ESC)) {
		for (i=0; i < NDOSPART; i++) {
			sprintf(mbr_field[(i*12)+1].field, "%d", mbr->dospart[i].dp_typ); 
			sprintf(mbr_field[(i*12)+2].field, "%ld", mbr->dospart[i].dp_start);
			sprintf(mbr_field[(i*12)+3].field, "%d", mbr->dospart[i].dp_scyl);
			sprintf(mbr_field[(i*12)+4].field, "%d", mbr->dospart[i].dp_shd);
			sprintf(mbr_field[(i*12)+5].field, "%d", mbr->dospart[i].dp_ssect);
			sprintf(mbr_field[(i*12)+6].field, "%d", 0);
			sprintf(mbr_field[(i*12)+7].field, "%d", mbr->dospart[i].dp_ecyl);
			sprintf(mbr_field[(i*12)+8].field, "%d", mbr->dospart[i].dp_ehd);
			sprintf(mbr_field[(i*12)+9].field, "%d", mbr->dospart[i].dp_esect);
			sprintf(mbr_field[(i*12)+10].field, "%ld", mbr->dospart[i].dp_size);
			sprintf(mbr_field[(i*12)+11].field, "%d", sectstoMb(mbr->dospart[i].dp_size, 512));
			sprintf(mbr_field[(i*12)+12].field, "%d", mbr->dospart[i].dp_flag);
		}

		disp_fields(window, mbr_field, sizeof(mbr_field)/sizeof(struct field));
		switch (mbr_field[cur_field].type) {
			case F_EDIT:
				key = line_edit(window, mbr_field[cur_field].y,
												mbr_field[cur_field].x,
												mbr_field[cur_field].width,
												mbr_field[cur_field].maxlen,
												item_selected_attr, 1,
												mbr_field[cur_field].field);
				/* Propagate changes to MBR */
				for (i=0; i < NDOSPART; i++) {
					mbr->dospart[i].dp_typ = atoi(mbr_field[(i*12)+1].field);
					mbr->dospart[i].dp_start = atoi(mbr_field[(i*12)+2].field);
					mbr->dospart[i].dp_scyl = atoi(mbr_field[(i*12)+3].field);
					mbr->dospart[i].dp_shd = atoi(mbr_field[(i*12)+4].field);
					mbr->dospart[i].dp_ssect = atoi(mbr_field[(i*12)+5].field);
					mbr->dospart[i].dp_ecyl = atoi(mbr_field[(i*12)+7].field);
					mbr->dospart[i].dp_ehd = atoi(mbr_field[(i*12)+8].field);
					mbr->dospart[i].dp_esect = atoi(mbr_field[(i*12)+9].field);
					mbr->dospart[i].dp_size = atoi(mbr_field[(i*12)+10].field);
				}
				break;
			case F_BUTTON:
				key = button_press(window, mbr_field[cur_field]);
				if (!key && !strcmp(mbr_field[cur_field].field, "OK")) {
					ok = 1;
					continue;
				}
				if (!key && !strcmp(mbr_field[cur_field].field, "Cancel")) {
					sprintf(errmsg, "\nUser aborted.\n");
					dialog_clear_norefresh();
					return (-1);
				}
				break;
			case F_TOGGLE:
			case F_TITLE:
			default:
				break;
		}
		next = change_field(mbr_field[cur_field], key); 
		if (next == -1) 
			beep();
		else
			cur_field = next;
	} 

	/* Clear active flags */
	for (i=0; i < NDOSPART; i++)
		mbr->dospart[i].dp_flag = 0;

	/* Find first FreeBSD partition and make it active */

	disk_list[disk].inst_part = -1;
	for (i=0; i < NDOSPART; i++)
		if (mbr->dospart[i].dp_typ == MBR_PTYPE_FreeBSD) {
			disk_list[disk].inst_part = i;
			mbr->dospart[i].dp_flag = ACTIVE;
			break;
		}

	sprintf(scratch, "\nWriting a new master boot record can erase the current disk contents.\n\n                Are you sure you want to write the new MBR?\n");
	dialog_clear_norefresh();
	if (!dialog_yesno("Write new MBR?", scratch, -1, -1)) {
		sprintf(scratch, "/dev/r%s%dd", disk_list[disk].devconf->dc_name,
											     disk_list[disk].devconf->dc_unit);
		if (write_mbr(scratch, mbr) == -1) {
			sprintf(scratch, "\nThe following error occured while trying to write the new MBR.\n\n%s", errmsg);
			return(-1);
		}
	}


	if (disk_list[disk].inst_part == -1) {
		sprintf(errmsg, "\nThere is no space allocated to FreeBSD on %s\n",
				  diskname(disk));
		return (-1);
	}

	delwin(window);
	dialog_clear();
	return (0);
}
