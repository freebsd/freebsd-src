/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id$
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <dialog.h>
#include <ncurses.h>
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
	char *diskname;
	struct ufs_args ufsargs;
	char           *s;
	int             i;

	/*
	 * Extract the disk-name from /etc/fstab, we wrote it ourselves,
	 * so we know how to read it :-)
	 * XXX: Multidisk installs.  We need to mount all partitions.
	 */

	i = open("/etc/fstab", O_RDONLY);
	if (i < 0) {
		Fatal("Couldn't open /etc/fstab");
	}
	read(i, scratch, 100);
	for (s = scratch; *s != ' ' && *s != '\t'; s++);
	s--;
	*s = '\0';
	s = scratch + 5;
	diskname = malloc(strlen(s) + 1);
	if (!diskname) {
		Fatal("malloc failed");
	}
	strcpy(diskname, s);
	close(i);

	sprintf(scratch, "mount -u /dev/%sa /", diskname);
	TellEm(scratch);
	sprintf(scratch, "/dev/%sa", diskname);
	ufsargs.fspec = scratch;
	if (mount(MOUNT_UFS, "/", MNT_UPDATE, (caddr_t) & ufsargs) == -1) {
		sprintf(errmsg, "Failed to mount root read/write: %s\n%s", strerror(errno), ufsargs.fspec);
		Fatal(errmsg);
	}
	sprintf(scratch, "mount /dev/%se /usr", diskname);
	TellEm(scratch);
	sprintf(scratch, "/dev/%se", diskname);
	ufsargs.fspec = scratch;
	if (mount(MOUNT_UFS, "/usr", 0, (caddr_t) & ufsargs) == -1) {
		sprintf(errmsg, "Failed to mount /usr: %s\n%s", strerror(errno), ufsargs.fspec);
		Fatal(errmsg);
	}
	TellEm("mkdir /proc");
	if (mkdir("/proc", S_IRWXU) == -1) {
		sprintf(errmsg, "Couldn't create directory /proc: %s\n",
			strerror(errno));
		Fatal(errmsg);
	}
	TellEm("mkdir /root");
	if (mkdir("/root", S_IRWXU) == -1) {
		sprintf(errmsg, "Couldn't create directory /root: %s\n",
			strerror(errno));
		Fatal(errmsg);
	}
	TellEm("mkdir /var");
	if (mkdir("/var", S_IRWXU) == -1) {
		sprintf(errmsg, "Couldn't create directory /var: %s\n",
			strerror(errno));
		Fatal(errmsg);
	}
	TellEm("mkdir /var/run");
	if (mkdir("/var/run", S_IRWXU) == -1) {
		sprintf(errmsg, "Couldn't create directory /var/run: %s\n",
			strerror(errno));
		Fatal(errmsg);
	}
	sprintf(scratch, "Insert CPIO floppy in floppy drive 0\n");
	dialog_msgbox("Stage 2 installation", scratch, 6, 75, 1);
	ufsargs.fspec = "/dev/fd0a";
	if (mount(MOUNT_UFS, "/mnt", MNT_RDONLY, (caddr_t) & ufsargs) == -1) {
		sprintf(errmsg, "Failed to mount /mnt: %s\n%s", strerror(errno), ufsargs.fspec);
		Fatal(errmsg);
	}
	TellEm("sh -c 'cd / ; gunzip < /mnt/inst2.cpio.gz | cpio -idum'");
	if (exec("/bin/sh", "/bin/sh", "-e", "-c",
		 "cd / ; gunzip < /mnt/inst2.cpio.gz | cpio -idum", 0) == -1)
		Fatal(errmsg);

	TellEm("sh -c 'cd /mnt ; ls install magic | cpio -dump /'");
	if (exec("/bin/sh", "/bin/sh", "-e", "-c",
		 "cd /mnt ; ls magic | cpio -dump /", 0) == -1)
		Fatal(errmsg);

	TellEm("unmount /mnt");
	if (unmount("/mnt", 0) == -1) {
		sprintf(errmsg, "Error unmounting /mnt: %s\n", strerror(errno));
		Fatal(errmsg);
	}
	TellEm("sh -c 'cd /dev ; sh MAKEDEV all'");
	if (exec("/bin/sh", "/bin/sh", "-e", "-c",
		 "PATH=/bin:/sbin:/usr/bin:/usr/sbin; export PATH ; cd /dev ; sh MAKEDEV all", 0) == -1)
		Fatal(errmsg);

	TellEm("unlink /sbin/oinit");
	unlink("/sbin/oinit");
	TellEm("link /stand/sysinstall /sbin/init");
	link("/stand/sysinstall", "/sbin/init");
	clear();
	refresh();
	endwin();
	close(0);
	close(1);
	close(2);
	execl("/sbin/init", "init", 0);
}
