/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: stage2.c,v 1.7 1994/10/26 05:41:00 phk Exp $
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <dialog.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>

#include "sysinstall.h"

void
stage2()
{
	char **p,**q;
	char pbuf[90];
	char dbuf[90];
	FILE *f1;
	int i;

	for(q=mountpoint,p = devicename; *p; p++,q++) {
		if(!strcmp(*q,"swap")) 
			continue;
		TellEm("newfs /dev/r%s",*p); 
		strcpy(pbuf, "/dev/r");
		strcat(pbuf, *p);
		i = exec(0, "/stand/newfs", "/stand/newfs", pbuf, 0);
		if (i) 
			Fatal("Exec(/stand/newfs) failed, code=%d.",i);
	}

	for(q=mountpoint,p = devicename; *p; p++,q++) {
		if(!strcmp(*q,"swap")) 
			continue;
		strcpy(dbuf,"/mnt");
		if(strcmp(*q,"/"))
			strcat(dbuf,*q);
		MountUfs(*p, dbuf, 1, 0);
	}

	Mkdir("/mnt/etc");
	Mkdir("/mnt/dev");
	Mkdir("/mnt/mnt");
	Mkdir("/mnt/stand");

	CopyFile("/stand/sysinstall","/mnt/stand/sysinstall");
	link("/mnt/stand/sysinstall","/mnt/stand/cpio");
	link("/mnt/stand/sysinstall","/mnt/stand/gunzip");
	link("/mnt/stand/sysinstall","/mnt/stand/gzip");
	link("/mnt/stand/sysinstall","/mnt/stand/zcat");
	link("/mnt/stand/sysinstall","/mnt/stand/newfs");
	link("/mnt/stand/sysinstall","/mnt/stand/fsck");
	link("/mnt/stand/sysinstall","/mnt/stand/dialog");
	CopyFile("/kernel","/mnt/kernel");
	TellEm("make /dev entries");
	chdir("/mnt/dev");
	makedevs();
	chdir("/");

	TellEm("Making /mnt/etc/fstab");
	f1 = fopen("/mnt/etc/fstab","w");
	if(!f1)
		Fatal("Couldn't open /mnt/etc/fstab for writing.");

	TellEm("Writing filesystems");
	for(q=mountpoint,p = devicename; *p; p++,q++) {
		if(!strcmp(*q,"swap")) 
			continue;
		fprintf(f1,"/dev/%s\t\t%s\tufs rw 1 1\n",*p,*q);
	}
	TellEm("Writing swap-devs");
	for(q=mountpoint,p = devicename; *p; p++,q++) {
		if(strcmp(*q,"swap")) 
			continue;
		fprintf(f1,"/dev/%s\t\tnone\tswap sw 0 0\n",*p);
	}
	TellEm("Writing procfs");
	fprintf(f1,"proc\t\t/proc\tprocfs rw 0 0\n");
	fclose(f1);

	sync();
	TellEm("Make marker-file");
	i = open("/mnt/stand/need_cpio_floppy",O_CREAT|O_WRONLY|O_TRUNC);
	close(i);
	
	/* we have to unmount in reverse order */
	for(p = mountpoint; *p; p++) 
		continue;

	TellEm("Unmount disks");
	for(p--;p >= mountpoint;p--) {
		if(!strcmp(*p,"swap")) 
			continue;
		strcpy(dbuf,"/mnt");
		if(strcmp(*p,"/"))
			strcat(dbuf,*p);
		TellEm("unmount %s",dbuf);
		/* Don't do error-check, we reboot anyway... */
		unmount(dbuf, 0);
	}
	dialog_msgbox(TITLE,"Remove the floppy from the drive, and hit return to reboot from the hard disk",6, 75, 1);
}
