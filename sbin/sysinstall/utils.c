/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: utils.c,v 1.12 1994/10/26 02:53:15 phk Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <dialog.h>
#include <errno.h>

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/reboot.h>

#include "sysinstall.h"

void
Debug(char *fmt, ...)
{
	char *p;
	va_list ap;
	p = Malloc(2048);
	va_start(ap,fmt);
	vsnprintf(p, 2048, fmt, ap);
	va_end(ap);
	write(debug_fd,"Debug <",7);
	write(debug_fd,p,strlen(p));
	write(debug_fd,">\n\r",3);
	free(p);
}

void
TellEm(char *fmt, ...)
{
	char *p;
	va_list ap;
	p = Malloc(2048);
	va_start(ap,fmt);
	vsnprintf(p, 2048, fmt, ap);
	va_end(ap);
	write(debug_fd,"Progress <",10);
	write(debug_fd,p,strlen(p));
	write(debug_fd,">\n\r",3);
	dialog_msgbox("Progress", p, 3, 75, 0);
	free(p);
}

void
Fatal(char *fmt, ...)
{
	char *p;
	va_list ap;
	p = Malloc(2048);
	va_start(ap,fmt);
	vsnprintf(p, 2048, fmt, ap);
	va_end(ap);
	if (dialog_active)
		dialog_msgbox("Fatal", p, 12, 75, 1);
	else
		fprintf(stderr, "Fatal -- %s", p);
	free(p);
	ExitSysinstall();
}

void
AskAbort(char *fmt, ...)
{
	char *p;
	va_list ap;

	p = Malloc(2048);
	va_start(ap,fmt);
	vsnprintf(p, 2048, fmt, ap);
	va_end(ap);
	strcat(p, "\n\nDo you wish to abort the installation?");
	if (!dialog_yesno("Abort", p, 15, 60)) {
		dialog_clear();
		Abort();
	}
	dialog_clear();
	free(p);
}

void
Abort()
{
	if (dialog_yesno("Exit sysinstall","\n\nAre you sure you want to quit?",
						  10, 40)) {
		dialog_clear();
		return;
	}
	ExitSysinstall();
}

void
ExitSysinstall()
{
	if (dialog_active) {
		clear();
		dialog_update();
	}
	if (getpid() == 1) {
		if (reboot(RB_AUTOBOOT) == -1)
			if (dialog_active) {
				clear();
				dialog_msgbox(TITLE, "\n\nCan't reboot machine -- hit reset button",
						  5,30,0);
			} else
				fprintf(stderr, "Can't reboot the machine -- hit the reset button");
			while(1);
	} else {
		if (dialog_active) {
			end_dialog();
			dialog_active = 0;
		}
		exit(0);
	}
}

void *
Malloc(size_t size)
{
	void *p = malloc(size);
	if (!p) {
		exit(7); /* XXX longjmp bad */
	}
	return p;
}

char *
StrAlloc(char *str) 
{
	char *p;

	p = (char *)Malloc(strlen(str) + 1);
	strcpy(p,str);
	return p;
}

void
MountUfs(char *device, char *mountpoint, int do_mkdir, int flags)
{
	struct ufs_args ufsargs;
	char dbuf[90];

	memset(&ufsargs,0,sizeof ufsargs);

	if(do_mkdir && access(mountpoint,R_OK)) {
		Mkdir(mountpoint);
	}

	strcpy(dbuf,"/dev/");
	strcat(dbuf,device);
	
	TellEm("mount %s %s",dbuf,mountpoint); 
	ufsargs.fspec = dbuf;
	if (mount(MOUNT_UFS, mountpoint, flags, (caddr_t) &ufsargs) == -1) {
		Fatal("Error mounting %s on %s : %s\n",
			dbuf, mountpoint, strerror(errno));
	}
}

void
Mkdir(char *path)
{
	TellEm("mkdir %s",path);
	if (mkdir(path, S_IRWXU) == -1) {
		Fatal("Couldn't create directory %s: %s\n",
			path,strerror(errno));
	}
}

void
CopyFile(char *p1, char *p2)
{
	char buf[BUFSIZ];
	int fd1,fd2;
	int i;
	struct stat st;

	TellEm("Copy %s to %s",p1,p2);
	fd1 = open(p1,O_RDONLY);
	if (fd1 < 0) Fatal("Couldn't open %s: %s\n",p1,strerror(errno));
	fd2 = open(p2,O_TRUNC|O_CREAT|O_WRONLY,0200);
	if (fd2 < 0) Fatal("Couldn't open %s: %s\n",p2,strerror(errno));
	for(;;) {
		i = read(fd1,buf,sizeof buf);
		if (i > 0)
			if (i != write(fd2,buf,i)) {
				Fatal("Write errror on %s: %s\n",
					p2,strerror(errno));
			}
		if (i != sizeof buf)
			break;
	}
	fstat(fd1,&st);
	fchmod(fd2,st.st_mode & 07777);
	fchown(fd2,st.st_uid,st.st_gid);
	close(fd1);
	close(fd2);
}
