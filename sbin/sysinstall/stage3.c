/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: stage3.c,v 1.3 1994/10/20 19:30:53 ache Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <dialog.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#if 0
#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include "bootarea.h"
#include <ufs/ffs/fs.h>
#endif

#include "sysinstall.h"

void
stage3()
{
	char		pbuf[90];
	char		dbuf[90];
	FILE		*f;

	/*
	 * Extract the disk-name from /etc/fstab, we wrote it ourselves,
	 * so we know how to read it :-)
	 * XXX: Multidisk installs.  We need to mount all partitions.
	 */

	f = fopen("/this_is_hd","r");
	if (!f) {
		Fatal("Couldn't open /this_is_hd");
	}
	while(fgets(dbuf,sizeof pbuf, f)) {
		dbuf[strlen(dbuf)-1] = 0;
		fgets(pbuf,sizeof pbuf, f);
		pbuf[strlen(pbuf)-1] = 0;
		MountUfs(dbuf,0,pbuf,0);
	}
	fclose(f);

	Mkdir("/proc");
	Mkdir("/root");
	Mkdir("/var");
	Mkdir("/var/run");

	sprintf(scratch, "Insert CPIO floppy in floppy drive 0\n");
	dialog_msgbox("Stage 2 installation", scratch, 6, 75, 1);

	MountUfs("/dev/fd0a",0,"/mnt",0);
	TellEm("sh -c 'cd / ; gunzip < /mnt/inst2.cpio.gz | cpio -idum'");

	if (exec(0, "/bin/sh",
		"/bin/sh", "-e", "-c",
		 "cd / ; gunzip < /mnt/inst2.cpio.gz | cpio -idum", 0) == -1)
		Fatal(errmsg);

	TellEm("sh -c 'cd /mnt ; ls install magic | cpio -dump /'");

	if (exec(0, "/bin/sh",
		"/bin/sh", "-e", "-c",
		 "cd /mnt ; ls magic | cpio -dump /", 0) == -1)
		Fatal(errmsg);

	TellEm("unmount /mnt");
	if (unmount("/mnt", 0) == -1) {
		sprintf(errmsg, "Error unmounting /mnt: %s\n", strerror(errno));
		Fatal(errmsg);
	}
	TellEm("sh -c 'cd /dev ; sh MAKEDEV all'");
	if (exec(0, "/bin/sh",
		"/bin/sh", "-e", "-c",
		 "PATH=/bin:/sbin:/usr/bin:/usr/sbin; export PATH ; cd /dev ; sh MAKEDEV all", 0) == -1)
		Fatal(errmsg);

	TellEm("unlink /sbin/oinit");
	unlink("/sbin/oinit");
	TellEm("link /stand/sysinstall /sbin/init");
	link("/stand/sysinstall", "/sbin/init");
	dialog_clear();
	dialog_update();
	end_dialog();
	close(0);
	close(1);
	close(2);
	execl(0,"/sbin/init", "init", 0);
}
