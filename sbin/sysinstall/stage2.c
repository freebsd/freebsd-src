/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: stage2.c,v 1.4 1994/10/21 02:14:51 phk Exp $
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
	FILE *f1, *f2;
	int i;

	for(q=mountpoint,p = devicename; *p; p++,q++) {
		if(!strcmp(*q,"swap")) 
			continue;
		TellEm("newfs /dev/r%s",*p); 
		strcpy(pbuf, "/dev/r");
		strcat(pbuf, *p);
		i = exec(0, "/bin/newfs", "/bin/newfs", pbuf, 0);
		if (i) 
			TellEm("Exec(/bin/newfs) failed, code=%d.",i);
	}

	for(q=mountpoint,p = devicename; *p; p++,q++) {
		if(!strcmp(*q,"swap")) 
			continue;
		MountUfs(*p, "/mnt", *q, 1);
	}

	TellEm("cpio -dumpv /mnt < file.list");
	i = exec(1, "/bin/cpio", "/bin/cpio", "-dumpv", "/mnt", 0);
	if (i)
		Fatal("Exec(/bin/cpio) failed, code=%d.",i);

	TellEm("Making /mnt/etc/fstab");
	f1 = fopen("/mnt/etc/fstab","w");
	if(!f1)
		Fatal("Couldn't open /mnt/etc/fstab for writing.");

	/* This file is our own.  It serves several jobs */
	f2 = fopen("/mnt/this_is_hd","w");
	if(!f2) 
		Fatal("Couldn't open /mnt/this_is_hd for writing.");

	TellEm("Writing filesystems");
	for(q=mountpoint,p = devicename; *p; p++,q++) {
		if(!strcmp(*q,"swap")) 
			continue;
		fprintf(f2,"%s\n%s\n",*p,*q);
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
	fclose(f2);

	/* we have to unmount in reverse order */
	for(p = mountpoint; *p; p++) 
		continue;
	TellEm("Unmount disks");
	
	for(p--;p >= mountpoint;p--) {
		if(!strcmp(*p,"swap")) 
			continue;
		strcpy(dbuf,"/mnt");
		strcat(dbuf,*p);
		TellEm("unmount %s",dbuf);
		if (unmount(dbuf, 0) == -1)
			Fatal("Error unmounting %s.",dbuf);
	}
	dialog_msgbox(TITLE,"Remove the floppydisk from the drive, and hit return to reboot from the harddisk",6, 75, 1);
}
