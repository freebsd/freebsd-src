/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id$
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <ncurses.h>
#include <dialog.h>
#include <errno.h>

#include <sys/stat.h>

#include "sysinstall.h"

void
stage2()
{
	char **p,**q;
	char buf[90];
	char *diskname = "sd0";
	int i;

	for(p = devicename; *p; p++) {
		TellEm("newfs /dev/r%s",*p); 
		strcpy(scratch, "/dev/r");
		strcat(scratch, *p);
		if (exec("/bin/newfs","/bin/newfs", scratch, 0) == -1)
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

	sprintf(scratch,"/dev/%sa		/		ufs		rw 1 1\n",diskname);
    write(i,scratch,strlen(scratch));
	sprintf(scratch,"/dev/%sb		none	swap	sw 0 0\n",diskname);
    write(i,scratch,strlen(scratch));
	sprintf(scratch,"proc		    /proc	procfs	rw 0 0\n");
    write(i,scratch,strlen(scratch));
	sprintf(scratch,"/dev/%se		/usr	ufs		rw 1 2\n",diskname);
    write(i,scratch,strlen(scratch));
	close(i);

	TellEm("unmount /mnt/usr");
	if (unmount("/mnt/usr", 0) == -1) {
		sprintf(errmsg, "Error unmounting /mnt/usr: %s\n", strerror(errno));
		Fatal(errmsg);
	}

	TellEm("unmount /mnt");
	if (unmount("/mnt", 0) == -1) {
		sprintf(errmsg, "Error unmounting /mnt: %s\n", strerror(errno));
		Fatal(errmsg);
	}
}
