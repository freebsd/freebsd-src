/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: main.c,v 1.9 1994/10/29 10:01:34 phk Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <setjmp.h>
#include <fcntl.h>

#include <dialog.h>

#include <sys/ioctl.h>
#include <sys/reboot.h>

#define EXTERN /* only in main.c */

#include "sysinstall.h"

jmp_buf	jmp_restart;

/*
 * XXX: utils: Mkdir must do "-p".
 * XXX: stage2: do mkdir for msdos-mounts.
 * XXX: label: Import dos-slice.
 * XXX: mbr: edit geometry
 */

extern int alloc_memory();

int
main(int argc, char **argv)
{
	int i;

	/* Are we running as init? */
	if (getpid() == 1) {
		close(0); open("/dev/console",O_RDWR);
		close(1); dup(0);
		close(2); dup(0);
		printf("sysinstall running as init\n\r");
		i = 1;
		ioctl(0,TIOCSPGRP,&i);
		setlogin("root");
		debug_fd = open("/dev/ttyv1",O_WRONLY);
	} else {
		debug_fd = open("sysinstall.debug",
			O_WRONLY|O_CREAT|O_TRUNC,0644);
	}
	if (set_termcap() == -1) {
		Fatal("Can't find terminal entry\n");
	}
	/* XXX too early to use fatal ! */

	/* XXX - libdialog has particularly bad return value checking */
	init_dialog();
	/* If we haven't crashed I guess dialog is running ! */
	dialog_active = 1;

	if (alloc_memory() < 0)
		Fatal("No memory\n");

	setjmp(jmp_restart);  /* XXX Allow people to "restart" */

	if (getenv("STAGE0") || !access("/this_is_boot_flp",R_OK)) {
		stage0();
		stage1();
		stage2();
		reboot(RB_AUTOBOOT);
	} else {
		stage3();
		stage4();
		stage5();
	}
	return 0;
}
