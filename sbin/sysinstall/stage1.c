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
#include "sysinstall.h"
#include "bootarea.h"

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

/* Forward decls */
void exit_sysinstall(void);
void exit_prompt(void);
extern char *part_type(int);
void Fdisk(void);
void DiskLabel(void);

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
    
    avail_disklabels = (struct disklabel *)calloc(MAX_NO_DISKS,
						  sizeof(struct disklabel));
    if (!avail_disklabels) 
	return(-1);

    avail_fds = (int *)calloc(MAX_NO_DISKS, sizeof(int));
    if (!avail_fds) 
	return(-1);
    
    avail_disknames = (unsigned char **)calloc(MAX_NO_DISKS, sizeof(char *));
    if (!avail_disknames)
	return(-1);
    for (i = 0; i < MAX_NO_DISKS; i++) {
	avail_disknames[i] = (char *)calloc(15, sizeof(char));
	if (!avail_disknames[i])
	    return(-1);
    }

    options = (unsigned char **)calloc(MAX_NO_DISKS, sizeof(char *));
    if (!options)
	return(-1);
    for (i = 0; i < MAX_NO_DISKS; i++) {
	options[i] = (char *)calloc(100, sizeof(char));
	if (!options[i])
	    return(-1);
    }

    mbr = (struct mbr *)malloc(sizeof(struct mbr));
    if (!mbr)
	return(-1);

    bootblocks = (char *)malloc(BBSIZE);
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

    for (i = 0; i < MAX_NO_DISKS; i++)
	free(avail_disknames[i]);
    free(avail_disknames);

    for (i = 0; i < MAX_NO_DISKS; i++)
	free(options[i]);
    free(options);

    free(mbr);
    free(bootblocks);
}


char * device_list[] = {"wd","sd",0};

void
query_disks()
{
    int i,j;
    char disk[15];
    char diskname[5];
    struct stat st;
    struct disklabel dl;
    int fd;

    for(i = 0; i < MAX_NO_DISKS; i++)
	if(Dname[i]) {
	    close(Dfd[i]); Dfd[i] = 0;
	    free(Dlbl[i]); Dlbl[i] = 0;
	    free(Dname[i]); Dname[i] = 0;
	}

    Ndisk = 0;

    for (j = 0; device_list[j]; j++) {
	for (i = 0; i < 10; i++) {
	    sprintf(diskname, "%s%d", device_list[j], i);
	    sprintf(disk, "/dev/r%sd", diskname);
	    if (stat(disk, &st) || !(st.st_mode & S_IFCHR))
		continue;
	    if ((fd = open(disk, O_RDWR)) == -1)
		continue;
	    if (ioctl(fd, DIOCGDINFO, &dl) == -1) {
		close(fd);
		continue;
	    }
	    Dlbl[Ndisk] = Malloc(sizeof dl);
	    memcpy(Dlbl[Ndisk], &dl, sizeof dl);
	    Dname[Ndisk] = StrAlloc(diskname);
	    Dfd[Ndisk] = fd;
	    Ndisk++;
	    if(Ndisk == MAX_NO_DISKS)
		return;
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
	sprintf(scratch,"There are %d disks available for installation: ",
		no_disks);

	for (i = 0; i < no_disks; i++) {
	    sprintf(options[(i*2)], "%d",i+1);
	    sprintf(options[(i*2)+1], "%s, (%dMb) -> %s",
		    avail_disklabels[i].d_typename,
		    disk_size(&avail_disklabels[i]),
		    avail_disknames[i]);
	}

	if (dialog_menu("FreeBSD Installation", scratch, 10, 75, 5, no_disks,
			options, selection)) {
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
	whole_disk = 0;

	sprintf(scratch,"Select one of the following areas to install to:");
	sprintf(options[0], "%d", 0);
	sprintf(options[1], "%s, (%dMb)", "Install to entire disk",
		disk_size(&avail_disklabels[inst_disk]));
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

int
stage1()
{
    int i,j;
    int ret=1;
    int ok = 0;
    int ready = 0;
    int foundroot=0,foundusr=0,foundswap=0;
    char *complaint=0;

    query_disks();
    /* 
     * Try to be intelligent about this and assign some mountpoints
     */
#define LEGAL(disk,part) ( 						\
	Dlbl[disk]->d_partitions[part].p_size &&			\
	(Dlbl[disk]->d_partitions[part].p_offset >= 			\
	    Dlbl[disk]->d_partitions[OURPART].p_offset) &&		\
	((Dlbl[disk]->d_partitions[part].p_size + 			\
		Dlbl[disk]->d_partitions[part].p_offset) <=		\
	    (Dlbl[disk]->d_partitions[OURPART].p_offset + 		\
		Dlbl[disk]->d_partitions[OURPART].p_size)))
	
    for(i = 0; i < MAX_NO_DISKS && Dname[i]; i++) {
	if (Dlbl[i]->d_partitions[OURPART].p_size == 0)
	    break;
	if (!foundroot && LEGAL(i,0) && 
		Dlbl[i]->d_partitions[0].p_fstype == FS_BSDFFS) {
	    SetMount(i,0,"/");
	    foundroot++;
	}
	if (!foundswap && LEGAL(i,1) &&
		Dlbl[i]->d_partitions[1].p_fstype == FS_SWAP) {
	    SetMount(i,1,"swap");
	    foundswap++;
	}
	if (!foundusr && LEGAL(i,4) &&
		Dlbl[i]->d_partitions[4].p_fstype == FS_BSDFFS) {
	    SetMount(i,4,"/usr");
	    foundusr++;
	}
    }
    while (!ready) {
	clear(); standend();
	j = 0;
	mvprintw(j++, 0, "%s -- Diskspace editor", TITLE);
	j++;
	mvprintw(j++, 0, "Disks         Total   FreeBSD ");
	j++;
	for(i = 0; i < MAX_NO_DISKS && Dname[i]; i++) {
	    mvprintw(j++, 0, "%2d: %-6s %5lu MB  %5lu MB",
		     i,
		     Dname[i],
		     PartMb(Dlbl[i],RAWPART),
		     PartMb(Dlbl[i],OURPART));
	}
	j++;
	mvprintw(j++, 0, "Filesystems  Type        Size  Mountpoint");
	j++;
	for(i = 0; i < MAX_NO_FS; i++) {
	    if(!Fname[i])
		continue;
	    mvprintw(j++, 0, "%2d: %-5s    %-5s   %5lu MB  %-s",
		     i, Fname[i], Ftype[i], Fsize[i], Fmount[i]);
	}

	mvprintw(21, 0, "Commands available:");
	mvprintw(22, 0, "(H)elp  (F)disk  (D)isklabel  (P)roceed  (Q)uit");
	if(complaint) {
		standout();
		mvprintw(24, 0, complaint);
		standend();
		complaint = 0;
	}
	mvprintw(23, 0, "Enter Command> ");
	i = getch();
	switch(i) {
	case 'h': case 'H':
            ShowFile(HELPME_FILE,"Help file for disklayout");
	    break;
	case 'p': case 'P':
	    foundroot=0,foundusr=0,foundswap=0;
	    for (i = 1; Fmount[i]; i++) {
		if(!strcmp(Fmount[i],"/")) foundroot=i;
		if(!strcmp(Fmount[i],"swap")) foundswap=i;
		if(!strcmp(Fmount[i],"/usr")) foundusr=i;
	    }
	    if (!foundroot) {
		complaint = "You must assign something to mount on '/'";
		break;
	    }
	    if (!foundroot) {
		complaint = "You must assign something to mount on 'swap'";
		break;
	    }
	    if (!foundusr && Fsize[foundroot] < 80) {
		complaint = "You must assign something to mount on '/usr'";
		break;
	    }
	    ret = 0;
	    goto leave;
	case 'q': case 'Q':
	    ret = 1;
	    goto leave;
	case 'f': case 'F':
	    Fdisk();
	    query_disks();
	    break;
	case 'd': case 'D':
	    DiskLabel();
	    break;
	default:
	    beep();
	}
    }
leave:
    clear();
    for (i = 0; Dname[i]; i++)
	close(Dfd[i]);
    return ret;
}
#if 0
	while (!ready) {
	    ready = 1;

	    inst_disk = select_disk();

	    if (read_mbr(avail_fds[inst_disk], mbr) == -1) {
		sprintf(scratch, "The following error occured while trying\nto read the master boot record:\n\n%s\nIn order to install FreeBSD a new master boot record\nwill have to be written which will mean all current\ndata on the hard disk will be lost.", errmsg);
		ok = 0;
		while (!ok) {	
		    AskAbort(scratch);
		    if (!dialog_yesno(TITLE,
				      "Are you sure you wish to proceed?",
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
		dialog_msgbox(TITLE, "This is an experimental disklabel configuration\nmenu. It doesn't perform any validation of the entries\nas yet so BE SURE YOU TYPE THINGS CORRECTLY.\n\n    Hit escape to quit the editor.\n\nThere may be some delay exiting because of a dialog bug", 20,70,1);
		dialog_clear();
		edit_disklabel(&avail_disklabels[inst_disk]);

		build_disklabel(&avail_disklabels[inst_disk]);
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
	if (getenv("STAGE0")) {
	    Fatal("We stop here");
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
#endif

