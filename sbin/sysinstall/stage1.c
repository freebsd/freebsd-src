/*
#define DEBUG
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

#include <dialog.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ufs/ffs/fs.h>
#include <machine/console.h>

#include "mbr.h"
#include "bootarea.h"
#include "sysinstall.h"

struct disklabel *avail_disklabels;
int *avail_fds;
unsigned char **options;
unsigned char **avail_disknames;
unsigned char *scratch;
unsigned char *errmsg;
unsigned char *bootblocks;
struct mbr *mbr;

int no_disks = 0;
int inst_disk = 0;
int inst_part = 0;
int whole_disk = 0;
int custom_install = 0;
int dialog_active = 0;

void exit_sysinstall();
void exit_prompt();
extern char *part_type(int);
extern int disk_size(int);

char selection[30];
char boot1[] = BOOT1;
char boot2[] = BOOT2;

int
alloc_memory()
{
	int i;

	scratch = (char *) calloc(SCRATCHSIZE, sizeof(char));
	if (!scratch)
		return(-1);

	errmsg = (char *) calloc(ERRMSGSIZE, sizeof(char));
	if (!errmsg)
		return(-1);

	avail_disklabels = (struct disklabel *) calloc(MAX_NO_DISKS, sizeof(struct disklabel));
	if (!avail_disklabels) 
		return(-1);

	avail_fds = (int *) calloc(MAX_NO_DISKS, sizeof(int));
	if (!avail_fds) 
		return(-1);

	avail_disknames = (unsigned char **) calloc(MAX_NO_DISKS, sizeof(char *));
	if (!avail_disknames)
		return(-1);
	for (i=0;i<MAX_NO_DISKS;i++) {
		avail_disknames[i] = (char *) calloc(15, sizeof(char));
		if (!avail_disknames[i])
			return(-1);
	}

	options = (unsigned char **) calloc(MAX_NO_DISKS, sizeof(char *));
	if (!options)
		return(-1);
	for (i=0;i<MAX_NO_DISKS;i++) {
		options[i] = (char *) calloc(100, sizeof(char));
		if (!options[i])
			return(-1);
	}

	mbr = (struct mbr *) malloc(sizeof(struct mbr));
	if (!mbr)
		return(-1);

	bootblocks = (char *) malloc(BBSIZE);
	if (!bootblocks)
		return(-1);

	return(0);
}

void
free_memory()
{
	int i;

	free(scratch);
	free(errmsg);
	free(avail_disklabels);
	free(avail_fds);

	for (i=0;i<MAX_NO_DISKS;i++)
		free(avail_disknames[i]);
	free(avail_disknames);

	for (i=0;i<MAX_NO_DISKS;i++)
		free(options[i]);
	free(options);

	free(mbr);
	free(bootblocks);
}


void
query_disks()
{
	int i;
	char disk[15];
	char diskname[5];
	struct stat st;
	int fd;

	no_disks = 0;
	for (i=0;i<10;i++) {
		sprintf(diskname,"wd%d",i);
		sprintf(disk,"/dev/r%sd",diskname);
		if ((stat(disk, &st) == 0) && (st.st_mode & S_IFCHR))
			if ((fd = open(disk, O_RDWR)) != -1) {
				avail_fds[no_disks] = fd;
				bcopy(diskname, avail_disknames[no_disks], strlen(diskname));
				if (ioctl(fd, DIOCGDINFO, &avail_disklabels[no_disks++]) == -1)
					no_disks--;
			}
	}

	for (i=0;i<10;i++) {
		sprintf(diskname,"sd%d",i);
		sprintf(disk,"/dev/r%sd",diskname);
		if ((stat(disk, &st) == 0) && (st.st_mode & S_IFCHR))
			if ((fd = open(disk, O_RDWR)) != -1) {
				avail_fds[no_disks] = fd;
				bcopy(diskname, avail_disknames[no_disks], strlen(diskname));
				if (ioctl(fd, DIOCGDINFO, &avail_disklabels[no_disks++]) == -1)
					no_disks--;
			}
	}
}

int
select_disk()
{
	int i;
	int valid;

	do {
		valid = 1;
		sprintf(scratch,"There are %d disks available for installation: ",no_disks);

		for (i=0;i<no_disks;i++) {
			sprintf(options[(i*2)], "%d",i+1);
			sprintf(options[(i*2)+1], "%s, (%dMb) -> %s",avail_disklabels[i].d_typename,disk_size(i),avail_disknames[i]);
		}

		if (dialog_menu("FreeBSD Installation", scratch, 10, 75, 5, no_disks, options, selection)) {
			dialog_clear();
			sprintf(scratch,"\n\n\nYou did not select a valid disk.\n\n");
			AskAbort(scratch);
			valid = 0;
		}
		dialog_clear();
	} while (!valid);
	return(atoi(selection) - 1);
}

int
select_partition(int disk)
{
	int valid;
	int i;
	int choice;

	do {
		valid = 1;

		sprintf(scratch,"Select one of the following areas to install to:");
		sprintf(options[0], "%d", 0);
		sprintf(options[1], "%s, (%dMb)", "Install to entire disk",
				  disk_size(disk));
		for (i=0; i < NDOSPART; i++) {
			sprintf(options[(i*2)+2], "%d",i+1);
			sprintf(options[(i*2)+3], "%s, (%ldMb)", 
			part_type(mbr->dospart[i].dp_typ),
						 mbr->dospart[i].dp_size * 512 / (1024 * 1024));
		}
		if (dialog_menu(TITLE,
			 scratch, 10, 75, 5, 5, options, selection)) {
			sprintf(scratch,"You did not select a valid partition");
			dialog_clear();
			AskAbort(scratch);
			valid = 0;
		}
		dialog_clear();
		choice = atoi(selection) - 1;
		if (choice == -1) {
			whole_disk = 1;
			choice = 0;
			if (dialog_yesno(TITLE, "\n\nInstalling to the whole disk will erase all its current data.\n\nAre you sure you want to do this?", 10, 75)) {
				valid = 0;
				whole_disk = 0;
			}
		}
		dialog_clear();
	} while (!valid);
	
	return(choice);
}

void
stage1()
{
	int i;
	int ok = 0;
	int ready = 0;

	query_disks();

	while (!ready) {
		ready = 1;

		inst_disk = select_disk();

		if (read_mbr(avail_fds[inst_disk], mbr) == -1) {
			sprintf(scratch, "The following error occured will trying\nto read the master boot record:\n\n%s\nIn order to install FreeBSD a new master boot record\nwill have to be written which will mean all current\ndata on the hard disk will be lost.", errmsg);
			ok = 0;
			while (!ok) {	
				AskAbort(scratch);
				if (!dialog_yesno(TITLE, "Are you sure you wish to proceed?",
									  10, 75)) {
					dialog_clear();
					if (clear_mbr(mbr, boot1) == -1) {
						sprintf(scratch, "\n\nCouldn't create new master boot record.\n\n%s", errmsg);
						Fatal(scratch);;
					}
					ok = 1;
				}
				dialog_clear();
			}
		}

		if (custom_install) 
			if (!dialog_yesno(TITLE, "Do you wish to edit the DOS partition table?",
								  10, 75)) {
				dialog_clear();
				edit_mbr(mbr, &avail_disklabels[inst_disk]);
			}

		dialog_clear();
		inst_part = select_partition(inst_disk);

		ok = 0;
		while (!ok) {
			if (build_mbr(mbr, boot1, &avail_disklabels[inst_disk]) != -1) {
				ready = 1;
				ok = 1;
			} else {
				sprintf(scratch, "The DOS partition table is inconsistent.\n\n%s\nDo you wish to edit it by hand?", errmsg);
				if (!dialog_yesno(TITLE, scratch, 10, 75)) {
					dialog_clear();
					edit_mbr(mbr, &avail_disklabels[inst_disk]);
				} else {
					dialog_clear();
					AskAbort("Installation cannot proceed without\na valid master boot record\n");
					ok = 1;
					ready = 0;
				}
			}
			dialog_clear();
		}

		if (ready) {
			default_disklabel(&avail_disklabels[inst_disk],
								mbr->dospart[inst_part].dp_size,
								mbr->dospart[inst_part].dp_start);
			if (build_bootblocks(&avail_disklabels[inst_disk]) == -1)
				Fatal(errmsg);
		}

		/* ready could have been reset above */
		if (ready) {
			if (dialog_yesno(TITLE, "We are now ready to format the hard disk for FreeBSD.\n\nSome or all of the disk will be overwritten during this process.\n\nAre you sure you wish to proceed ?", 10, 75)) {
				dialog_clear();
				AskAbort("Do you want to quit?");
				ready = 0;
			}
			dialog_clear();
		}
	}

	/* Write master boot record and bootblocks */
	if (write_mbr(avail_fds[inst_disk], mbr) == -1)
		Fatal(errmsg);
	if (write_bootblocks(avail_fds[inst_disk],
						  mbr->dospart[inst_part].dp_start,
						  avail_disklabels[inst_disk].d_bbsize) == -1)
		Fatal(errmsg);

	/* close all the open disks */
	for (i=0; i < no_disks; i++)
		if (close(avail_fds[i]) == -1) {
			sprintf(errmsg, "Error on closing file descriptors: %s\n",
					  strerror(errno));
			Fatal(errmsg);
		}
}
