/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: stage2.c,v 1.22 1995/01/30 03:19:54 phk Exp $
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
    char *p, *q;
    char pbuf[90];
    char dbuf[90];
    FILE *f1;
    int i, j;

    memset(Fsize, 0, sizeof Fsize);

    /* Sort in mountpoint order */
    for (i = 1; Fname[i]; i++)
	Fsize[i] = i;
    Fsize[i] = 0;

    for (j = 1; j;)
	for (j = 0, i = 1; Fsize[i+1]; i++) {
	    if (strcmp(Fmount[Fsize[i]], Fmount[Fsize[i+1]]) > 0) {
		j = Fsize[i];
		Fsize[i] = Fsize[i+1];
		Fsize[i + 1] = j;
	    }
	}

    for (j = 1; Fsize[j]; j++) {
	if (strcmp(Ftype[Fsize[j]], "ufs")) 
	    continue;
	p = Fname[Fsize[j]];
	strcpy(pbuf, "/dev/r");
	strcat(pbuf, p);
	if (!Faction[Fsize[j]]) {
		TellEm("fsck -y /dev/r%s",p); 
		i = exec(0, "/stand/fsck", "/stand/fsck", "-y", pbuf, 0);
		if (i) 
		    Fatal("Exec(/stand/fsck) failed, code=%d.",i);
	} else {
		TellEm("newfs /dev/r%s",p); 
		i = exec(0, "/stand/newfs", "/stand/newfs", "-n", "1", pbuf, 0);
		if (i) 
		    Fatal("Exec(/stand/newfs) failed, code=%d.",i);
	}
    }

    for (j = 1; Fsize[j]; j++) {
	if (!strcmp(Ftype[Fsize[j]], "swap"))
	    continue;
	strcpy(dbuf, "/mnt");
	p = Fname[Fsize[j]];
	q = Fmount[Fsize[j]];
	if (strcmp(q, "/"))
	    strcat(dbuf, q);
	if (!strcmp(Ftype[Fsize[j]], "ufs")) {
	    MountUfs(p, dbuf, 1, 0);
	    continue;
        }
	Mkdir(dbuf);
    }

    Mkdir("/mnt/etc", TRUE);
    Mkdir("/mnt/dev", TRUE);
    Mkdir("/mnt/mnt", TRUE);
    Mkdir("/mnt/stand", TRUE);

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

    if (fixit) {
	for (i=0;i<100;i++) {
	    sprintf(pbuf,"/mnt/etc/fstab.before.fixit.%d",i);
	    if (access(pbuf,R_OK)) {
		rename("/mnt/etc/fstab",pbuf);
		break;
	    }
	}
	for (i=0;i<100;i++) {
	    sprintf(pbuf,"/mnt/kernel.before.fixit.%d",i);
	    if (access(pbuf,R_OK)) {
		rename("/mnt/kernel",pbuf);
		break;
	    }
	}
	for (i=0;i<100;i++) {
	    sprintf(pbuf,"/mnt/sbin/init.before.fixit.%d",i);
	    if (access(pbuf,R_OK)) {
		rename("/mnt/sbin/init",pbuf);
		break;
	    }
	}
    }

    TellEm("make /dev entries");
    chdir("/mnt/dev");
    makedevs();
    chdir("/");

    TellEm("Making /mnt/etc/fstab");
    f1 = fopen("/mnt/etc/fstab","w");
    if (!f1)
	Fatal("Couldn't open /mnt/etc/fstab for writing.");

    TellEm("Writing filesystems");
    chdir("/mnt");
    for (j = 1; Fsize[j]; j++) {
	if (!strcmp(Ftype[Fsize[j]],"swap"))
	    fprintf(f1, "/dev/%s\t\tnone\tswap sw 0 0\n", Fname[Fsize[j]]);
	else {
	    fprintf(f1, "/dev/%s\t\t%s\t%s rw 1 1\n",
		    Fname[Fsize[j]], Fmount[Fsize[j]], Ftype[Fsize[j]]);
	    Mkdir(Fmount[Fsize[j]], FALSE);
	}
    }
    chdir("/");
    TellEm("Writing procfs");
    fprintf(f1,"proc\t\t/proc\tprocfs rw 0 0\n");
    fclose(f1);

#if 1   
{
#include <sys/wait.h>

    int ffd, pfd[2];
    int zpid, cpid;
    int i,j,k;

    j = fork();
    if (!j) {
	chroot("/mnt");
	chdir("/");
	retry:
	    while (1) {
		dialog_msgbox(TITLE, 
			      "Insert CPIO floppy in floppy drive 0", -1, -1, 1);
		ffd = open("/dev/rfd0",O_RDONLY);
		if (ffd > 0)
		    break;
		Debug("problems opening /dev/rfd0: %d",errno);
	    }
	    dialog_clear_norefresh();
	    TellEm("cd /stand ; gunzip < /dev/fd0 | cpio -idum");
	    pipe(pfd);
	    zpid = fork();
	    if (!zpid) {
		close(0); dup(ffd); close(ffd);
		close(1); dup(pfd[1]); close(pfd[1]);
		close(pfd[0]);
		i = exec (1,"/stand/gunzip","/stand/gunzip", 0);
		exit(i);
	    }
	    cpid = fork();
	    if (!cpid) {
		close(0); dup(pfd[0]); close(pfd[0]);
		close(ffd);
		close(pfd[1]);
		close(1); open("/dev/null",O_WRONLY);
		chdir("/stand");
		i = exec (1,"/stand/cpio","/stand/cpio","-iduvm", 0);
		exit(i);
	    }
	    close(pfd[0]);
	    close(pfd[1]);
	    close(ffd);
	    i = wait(&j);
	    if (i < 0 || j)
		Fatal("Pid %d, status %d, cpio=%d, gunzip=%d.\nerror:%s",
		      i, j, cpid, zpid, strerror(errno));
	    i = wait(&j);
	    if (i < 0 || j)
		Fatal("Pid %d, status %d, cpio=%d, gunzip=%d.\nerror:%s",
		      i, j, cpid, zpid, strerror(errno));
	    
	    /* bininst.sh MUST be the last file on the floppy */
	    if (access("/stand/OK", R_OK) == -1) {
		AskAbort("CPIO floppy was bad!  Please check media for defects and retry.");
		goto retry;
	    }
	unlink("/stand/OK");
	i = rename ("/stand/kernel","/kernel");
	exit (i);
    }
    i = wait(&k);
    Debug("chroot'er: %d %d %d",i,j,k);
}

#endif

    sync();
    TellEm("Unmount disks");
    for (j = 1; Fsize[j]; j++) 
	continue;
	
    for (j--; j > 0; j--) {
	if (!strcmp(Ftype[Fsize[j]],"swap")) 
	    continue;
	strcpy(dbuf,"/mnt");
	if (strcmp(Fmount[Fsize[j]],"/"))
	    strcat(dbuf, Fmount[Fsize[j]]);
	TellEm("unmount %s", dbuf);
	/* Don't do error-check, we reboot anyway... */
	unmount(dbuf, 0);
    }
    dialog_msgbox(TITLE,"Remove the floppy from the drive\n and hit return to reboot from the hard disk", -1, -1, 1);
    dialog_clear();
}
