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

#include "sysinstall.h"

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
stage1()
{
    int i,j;
    int ret=1;
    int ready = 0;
    int foundroot=0,foundusr=0,foundswap=0;
    char *complaint=0;

    query_disks();

    while (!ready) {
	clear(); standend();
	j = 2;
	if (fixit) {
	    mvprintw(j++, 50, "|Suggested course of action:");
	    mvprintw(j++, 50, "|");
	    mvprintw(j++, 50, "|(F)disk, (W)rite");
	    mvprintw(j++, 50, "|possibly (F)disk, (B)oot"); 
	    mvprintw(j++, 50, "|(D)isklabel, (A)ssign <root>");
	    mvprintw(j++, 50, "|(A)ssign swap");
	    mvprintw(j++, 50, "|(P)roceed"); 
	    mvprintw(j++, 50, "|Reboot");
	    mvprintw(j++, 50, "|Load cpio floppy");
	    mvprintw(j++, 50, "|Choose stand-alone shell");
	    mvprintw(j++, 50, "|");
	    mvprintw(j++, 50, "|Your old kernel, /etc/fstab");
	    mvprintw(j++, 50, "|and /sbin/init files are");
	    mvprintw(j++, 50, "|renamed since they will be");
	    mvprintw(j++, 50, "|replaced from this floppy.");
	} else {
	    mvprintw(j++, 50, "|You should now assign some");
	    mvprintw(j++, 50, "|space to root, swap, and");
	    mvprintw(j++, 50, "|(optionally) /usr partitions"); 
	    mvprintw(j++, 50, "|Root (/) should be a minimum");
	    mvprintw(j++, 50, "|of 18MB with a 30MB /usr");
	    mvprintw(j++, 50, "|or 50MB without a /usr."); 
	    mvprintw(j++, 50, "|Swap space should be a");
	    mvprintw(j++, 50, "|minimum of 12MB or RAM * 2");
	    mvprintw(j++, 50, "|Be sure to also (A)ssign a");
	    mvprintw(j++, 50, "|mount point to each one or");
	    mvprintw(j++, 50, "|it will NOT be enabled."); 
	    mvprintw(j++, 50, "|");
	    mvprintw(j++, 50, "|We suggest that you invoke");
	    mvprintw(j++, 50, "|(F)disk, (W)rite the bootcode");
	    mvprintw(j++, 50, "|then (D)isklabel your disk.");
	    mvprintw(j++, 50, "|If installing on a drive");
	    mvprintw(j++, 50, "|other than 0, also read the");
	    mvprintw(j++, 50, "|TROUBLESHOOTING doc first");
	}

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
	mvprintw(j++, 0, "Filesystems  Type        Size  Action Mountpoint");
	j++;
	for(i = 0; i < MAX_NO_FS; i++) {
	    if(!Fname[i])
		continue;
	    if(!strcmp(Ftype[i],"swap")) {
		mvprintw(j++, 0, "%2d: %-5s    %-5s   %5lu MB  %-6s %-s",
		     i, Fname[i], Ftype[i], Fsize[i], "swap", Fmount[i]);
	    } else {
		mvprintw(j++, 0, "%2d: %-5s    %-5s   %5lu MB  %-6s %-s",
		     i, Fname[i], Ftype[i], Fsize[i], 
			     Faction[i] ? "newfs" : "mount", Fmount[i]);
	    }
	}

	mvprintw(20, 0, "Commands available:");
	mvprintw(21, 0, "(H)elp  (T)utorial  (F)disk  (D)isklabel  (P)roceed  (Q)uit");
	if(complaint) {
		standout();
		mvprintw(22, 0, complaint);
		standend();
		complaint = 0;
	}
	mvprintw(23, 0, "Enter Command> ");
	i = getch();
	switch(i) {
	case 'h': case 'H':
	    clear();
	    mvprintw(0, 0, 
"%s -- Diskspace editor -- Command Help

(T)utorial	- Read a more detailed tutorial on how disklabels, MBRs,
		  etc. work.
(P)roceed	- Proceed with system installation.
(Q)uit		- Don't install anything.
(F)disk		- Enter the FDISK (MBR) editor.
(D)isklabel	- Enter the disklabel editor.

Press any key to return to Diskspace editor...", TITLE);
	    getch();
	    break;
	case 't': case 'T':
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
		complaint = "Please assign mountpoint for '/'";
		break;
	    }
	    if (!foundswap) {
		complaint = "Please assign mountpoint for swap";
		break;
	    }
	    if (!fixit && !foundusr && Fsize[foundroot] < 60) {
		complaint = "Please assign mountpoint for /usr";
		break;
	    }
	    if (dialog_yesno("Last Chance!",
		"Are you sure you want to proceed with the installation?\nLast chance before wiping your hard disk!", -1, -1))
		break;
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

