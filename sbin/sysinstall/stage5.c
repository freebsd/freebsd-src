/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: stage5.c,v 1.13 1994/11/08 03:41:42 jkh Exp $
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
You are now done with the initial phase of the installation that
gets FreeBSD onto your hard disk.  At this point, we need to go on
to ftp, SLIP, DOS floppy, parallel port or carrier pigeon a bindist
over so that your system is actually usable.  Select _Yes_ to proceed
with this phase, or _No_ to simply drop into a shell.";

void
stage5()
{
	int exec_sh = 1;

	if (!dialog_yesno("End of initial installation", msg, 10, 76))
		exec_sh = 0;
	end_dialog();
	dialog_active=0;
	setenv("PATH","/stand",1);
	for(;;) {
		if (exec_sh)
			exec (2,"/stand/sh","/stand/-sh", 0);
		else
			exec (3,"/stand/bininst","/stand/-bininst", 0);
	}
}
