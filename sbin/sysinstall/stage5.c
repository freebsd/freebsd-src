/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: stage5.c,v 1.5 1994/11/02 09:05:49 jkh Exp $
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
You are now done with the first phase of the installation.  We will,
for now, dump you rather unceremoniously into a shell where you can
then ftp, SLIP, DOS floppy or carrier pigeon the bindist over.  This
will NOT be so unfriendly in the BETA installation, and will lead
instead to a menu offering you various helpful ways of getting the
bindist.  This is all we had time for in the ALPHA, however.  Sorry!
Thank you for your patience!";

void
stage5()
{
	dialog_msgbox(TITLE, msg, strheight(msg) + 4, strwidth(msg) + 4, 1);
	end_dialog();
	dialog_active=0;
	setenv("PATH","/stand",1);
	for(;;)
		exec (2,"/stand/sh","/stand/-sh", 0);
}
