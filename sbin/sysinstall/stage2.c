/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: stage2.c,v 1.17 1994/11/19 00:17:55 ache Exp $
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fstab.h>
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
    char *p, *q;
    char pbuf[90];
    char dbuf[90];
    FILE *f1;
    int i, j;
    int fidx[MAX_NO_MOUNTS];
    struct fstab *fp;

    if (dialog_yesno("Last Chance!", "Are you sure you want to proceed with the installation?\nLast chance before wiping your hard disk!", -1, -1))
	exit(0);
    /* Sort in mountpoint order */
    memset(fidx, 0, sizeof fidx);

    for (j = 1; j;)
	for (j = 0, i = 0; i < no_mounts; i++) {
	    if (strcmp(mounts[i]->fs_file, mounts[i+1]->fs_file) > 0) {
		fp = mounts[i];
		mounts[i] = mounts[i+1];
		mounts[i+1] = fp;
		j++;
	    }
	}

    for (j = 0; j < no_mounts; j++) {
	if (strcmp(mounts[j]->fs_vfstype, "ufs")) 
	    continue;
	if (!strcmp(mounts[j]->fs_mntops, "YES")) 
	    continue;
	p = mounts[j]->fs_spec;
	TellEm("newfs /dev/r%s",p); 
	strcpy(pbuf, "/dev/r");
	strcat(pbuf, p);
	i = exec(0, "/stand/newfs", "/stand/newfs", "-n", "1", pbuf, 0);
	if (i) 
	    Fatal("Exec(/stand/newfs) failed, code=%d.",i);
	}

    for (j = 0; fidx[j]; j++) {
	strcpy(dbuf, "/mnt");
	p = mounts[j]->fs_spec;
	q = mounts[j]->fs_file;
	if (strcmp(q, "/"))
	    strcat(dbuf, q);
	Mkdir(dbuf);
	if (strcmp(mounts[j]->fs_vfstype, "ufs"))
	    continue;
	MountUfs(p, dbuf, 1, 0);
    }

    Mkdir("/mnt/etc");
    Mkdir("/mnt/dev");
    Mkdir("/mnt/mnt");
    Mkdir("/mnt/stand");

    TellEm("unzipping /stand/sysinstall onto hard disk");
    exec(4, "/stand/gzip", "zcat", 0 );
    Link("/mnt/stand/sysinstall","/mnt/stand/cpio");
    Link("/mnt/stand/sysinstall","/mnt/stand/bad144");
    Link("/mnt/stand/sysinstall","/mnt/stand/gunzip");
    Link("/mnt/stand/sysinstall","/mnt/stand/gzip");
    Link("/mnt/stand/sysinstall","/mnt/stand/zcat");
    Link("/mnt/stand/sysinstall","/mnt/stand/newfs");
    Link("/mnt/stand/sysinstall","/mnt/stand/fsck");
    Link("/mnt/stand/sysinstall","/mnt/stand/dialog");

    CopyFile("/kernel","/mnt/kernel");
    TellEm("make /dev entries");
    chdir("/mnt/dev");
    makedevs();
    chdir("/");

    TellEm("Making /mnt/etc/fstab");
    f1 = fopen("/mnt/etc/fstab","w");
    if (!f1)
	Fatal("Couldn't open /mnt/etc/fstab for writing.");

    TellEm("Writing filesystems");
    for (j = 0; j < no_mounts; j++) {
	if (!strcmp(mounts[j]->fs_vfstype,"swap"))
	    fprintf(f1, "/dev/%s\t\tnone\tswap sw 0 0\n", 
		mounts[j]->fs_spec);
	else
	    fprintf(f1, "/dev/%s\t\t%s\t%s rw 1 1\n",
		    mounts[j]->fs_spec,
	 	    mounts[j]->fs_file,
		    mounts[j]->fs_vfstype);
    }
    TellEm("Writing procfs");
    fprintf(f1,"proc\t\t/proc\tprocfs rw 0 0\n");
    fclose(f1);

    sync();
    TellEm("Make marker file");
    i = open("/mnt/stand/need_cpio_floppy",O_CREAT|O_WRONLY|O_TRUNC);
    close(i);
    
    TellEm("Unmount disks");
    for (j = no_mounts-1; j >= 0; j--) {
	if (strcmp(mounts[j]->fs_vfstype,"ufs")) 
	    continue;
	strcpy(dbuf,"/mnt");
	if (strcmp(mounts[j]->fs_file,"/"))
	    strcat(dbuf, mounts[j]->fs_file);
	TellEm("unmount %s", dbuf);
	/* Don't do error-check, we reboot anyway... */
	unmount(dbuf, 0);
    }
    dialog_msgbox(TITLE,"Remove the floppy from the drive\nand hit return to reboot from the hard disk", -1, -1, 1);
    dialog_clear();
}
