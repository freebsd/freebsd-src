/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: utils.c,v 1.2 1994/10/20 05:00:00 phk Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <dialog.h>
#include <ncurses.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/mount.h>

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
	dialog_msgbox("Fatal", p, 12, 75, 1);
	free(p);
	exit(7);
}

int
AskAbort(char *fmt, ...)
{
	char *p;
	va_list ap;
	int i;

	p = Malloc(2048);
	va_start(ap,fmt);
	vsnprintf(p, 2048, fmt, ap);
	va_end(ap);
	i = dialog_yesno("Abort", p, 12, 75);
	free(p);
	return i;
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

int
exec(char *cmd, char *args, ...)
{
	int pid, w, status;
	char **argv = NULL;
	int arg = 0;
	int no_args = 0;
	va_list ap;
	struct stat dummy;

	if (stat(cmd, &dummy) == -1) {
		sprintf(errmsg, "Executable %s does not exist\n", cmd);
		return(-1);
	}

	va_start(ap, args);
	do {
		if (arg == no_args) {
			no_args += 10;
			if (!(argv = realloc(argv, no_args * sizeof(char *)))) {
				sprintf(errmsg, "Failed to allocate memory during exec of %s\n", cmd);
				return(-1);
			}
			if (arg == 0)
				argv[arg++] = (char *)args;
		}
	} while ((argv[arg++] = va_arg(ap, char *)));
	va_end(ap);

	if ((pid = fork()) == 0) {
		execv(cmd, argv);
		exit(1);
	}
	
	while ((w = wait(&status)) != pid && w != -1)
		;

	free(argv);
	if (w == -1) {
		sprintf(errmsg, "Child process %s terminated abnormally\n", cmd);
		return(-1);
	}

	return(0);
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
		TellEm("mkdir %s",pbuf); 
		if (mkdir(pbuf,S_IRWXU) == -1) {
			Fatal("Couldn't create directory %s: %s\n",
				pbuf,strerror(errno));
		}
	}

	strcpy(dbuf,"/dev/");
	strcat(dbuf,device);
	
	TellEm("mount /dev/%s /mnt%s",dbuf,pbuf); 
	ufsargs.fspec = dbuf;
	if (mount(MOUNT_UFS,pbuf, 0, (caddr_t) &ufsargs) == -1) {
		Fatal("Error mounting %s on : %s\n",
			dbuf, pbuf, strerror(errno));
	}
}
