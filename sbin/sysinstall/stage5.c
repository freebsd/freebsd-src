/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: stage5.c,v 1.7 1994/11/03 00:30:27 ache Exp $
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

#include "sysinstall.h"

static unsigned char msg[] = "
You are now done with the second phase of the installation.  At this point,
FreeBSD is on your hard disk and now we need to go on to the 3rd level
installation, which is to ftp, SLIP, DOS floppy, parallel port or carrier
pigeon the bindist over.  Select OK to proceed with this phase, or CANCEL
to simply drop into the shell.";

void
stage5()
{
	int exec_sh = 0;

	if (!dialog_yesno("End of stage 2", msg,
			  strheight(msg) + 4, strwidth(msg) + 4))
		exec_sh = 1;
	end_dialog();
	dialog_active=0;
	setenv("PATH","/stand",1);
	for(;;) {
		if (exec_sh)
			exec (2,"/stand/sh","/stand/-sh", 0);
		else
			exec (2,"/stand/bininst","/stand/-bininst", 0);
	}
}
