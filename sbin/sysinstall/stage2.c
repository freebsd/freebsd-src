/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: stage2.c,v 1.2 1994/10/20 06:48:40 phk Exp $
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
	int i,j;

	for(q=mountpoint,p = devicename; *p; p++,q++) {
		if(!strcmp(*q,"swap")) 
			continue;
		TellEm("newfs /dev/r%s",*p); 
		strcpy(pbuf, "/dev/r");
		strcat(pbuf, *p);
		if (exec("/bin/newfs","/bin/newfs", pbuf, 0) == -1)
			Fatal(errmsg);
	}

	for(q=mountpoint,p = devicename; *p; p++,q++) {
		MountUfs(*p, "/mnt", *q, 1);
	}

	TellEm("cpio -dumpv /mnt < file.list");
	if (exec("/bin/cpio","/bin/cpio", "-dumpv", "/mnt", 0) == -1)
		Fatal(errmsg);

	TellEm("write /mnt/etc/fstab");
	i = open("/mnt/etc/fstab",O_CREAT|O_TRUNC|O_APPEND|O_WRONLY,0644);
	if(i < 0) {
		Fatal("Couldn't open /mnt/etc/fstab for writing");
	}

	/* This file is our own.  It serves several jobs */
	j = open("/mnt/this_is_hd",O_CREAT|O_TRUNC|O_APPEND|O_WRONLY,0644);
	if(j < 0) {
		Fatal("Couldn't open /mnt/this_is_hd for writing");
	}
	for(q=mountpoint,p = devicename; *p; p++,q++) {
		sprintf(pbuf,"%s\n%s\n",*p,*q);
	        write(j,pbuf,strlen(pbuf));
		if(!strcmp(*q,"swap")) 
			continue;
		sprintf(pbuf,"/dev/%s\t\t%s\tufs rw 1 1\n",*p,*q);
	        write(i,pbuf,strlen(pbuf));
	}
	for(q=mountpoint,p = devicename; *p; p++,q++) {
		if(strcmp(*q,"swap")) 
			continue;
		sprintf(pbuf,"/dev/%s\t\tnone\tswap sw 0 0\n",*p);
		write(i,pbuf,strlen(pbuf));
	}
	close(i);
	write(j,"\n",1);
	close(j);
	sprintf(pbuf,"proc\t\t/proc\tprocfs rw 0 0\n");
	write(i,pbuf,strlen(pbuf));

	/* we have to unmount in reverse order */
	for(p = mountpoint; *p; p++) 
		continue;
	
	for(p--;p >= mountpoint;p--) {
		if(!strcmp(*q,"swap")) 
			continue;
		strcpy(dbuf,"/mnt");
		strcat(dbuf,*p);
		TellEm("unmount %s",dbuf);
		if (unmount("/mnt", 0) == -1)
			Fatal("Error unmounting /mnt: %s", strerror(errno));
	}
}
