/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: stage4.c,v 1.10 1994/11/18 10:12:56 jkh Exp $
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
#include <sys/types.h>
#include <sys/wait.h>

#include "sysinstall.h"

void
stage4()
{
    int ffd, pfd[2];
    int zpid, cpid;
    int i,j;

    if (access("/stand/need_cpio_floppy",R_OK))
	return;
retry:
    while (1) {
	dialog_msgbox(TITLE, 
		      "Insert CPIO floppy in floppy drive 0", -1, -1, 1);
	ffd = open("/dev/fd0a",O_RDONLY);
	if (ffd > 0)
	    break;
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
    if (access("/stand/bininst.sh", R_OK) == -1) {
	AskAbort("CPIO floppy was bad!  Please check media for defects and retry.");
	goto retry;
    }
    else {
	TellEm("unlink /stand/need_cpio_floppy");
	unlink("/stand/need_cpio_floppy");
    }
}
