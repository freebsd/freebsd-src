/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: utils.c,v 1.6 1994/10/21 04:43:07 ache Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <dialog.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/reboot.h>

#include "sysinstall.h"

void
TellEm(char *fmt, ...)
{
	char *p;
	va_list ap;
	p = Malloc(2048);
	va_start(ap,fmt);
	vsnprintf(p, 2048, fmt, ap);
	va_end(ap);
	dialog_msgbox("Progress", p, 3, 75, 0);
	dialog_clear();
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
	if (dialog_active) {
		dialog_msgbox("Fatal", p, 12, 75, 1);
		end_dialog();
	} else
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
	if (getpid() == 1) {
		clear();
		if (reboot(RB_AUTOBOOT) == -1)
			if (dialog_active) {
				dialog_clear();
				dialog_msgbox(TITLE, "\n\nCan't reboot machine -- hit reset button",
						  5,30,0);
			} else
				fprintf(stderr, "Can't reboot the machine -- hit the reset button");
			while(1);
	} else
		clear();
		refresh();
		end_dialog();
		exit(0);
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
MountUfs(char *device, char *prefix, char *mountpoint, int do_mkdir)
{
	struct ufs_args ufsargs;
	char dbuf[90];
	char pbuf[90];

	memset(&ufsargs,0,sizeof ufsargs);

	if (prefix)
		strcpy(pbuf,prefix);
	else
		strcpy(pbuf,"");

	strcat(pbuf,mountpoint);

	if(do_mkdir && access(pbuf,R_OK)) {
		Mkdir(pbuf);
	}

	strcpy(dbuf,"/dev/");
	strcat(dbuf,device);
	
	TellEm("mount /dev/%s /mnt%s",dbuf,pbuf); 
	ufsargs.fspec = dbuf;
	if (mount(MOUNT_UFS,pbuf, 0, (caddr_t) &ufsargs) == -1) {
		Fatal("Error mounting %s on %s : %s\n",
			dbuf, pbuf, strerror(errno));
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
