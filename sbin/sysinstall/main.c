/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: main.c,v 1.17 1994/11/17 23:36:46 ache Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <setjmp.h>
#include <fcntl.h>
#include <signal.h>

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

void
handle_intr(int sig)
{
	dialog_clear_norefresh();
	dialog_msgbox("User Interrupt",
		      "User interrupted.  Aborting the installation",
		      -1, -1, 1);
	ExitSysinstall();
}

int
main(int argc, char **argv)
{
	signal(SIGINT, SIG_IGN);

	/* Are we running as init? */
	if (getpid() == 1) {
		setsid();
		close(0); open("/dev/console",O_RDWR);
		close(1); dup(0);
		close(2); dup(0);
		printf("sysinstall running as init\n\r");
		ioctl(0,TIOCSCTTY,(char *)NULL);
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

	signal(SIGINT, handle_intr);

	if (getpid() != 1) {
		stage0();
		stage1();
		end_dialog();
		dialog_active=0;
	} else if (!access("/this_is_boot_flp",R_OK)) {
		while(1) {
			stage0();
			if(!stage1())
				break;
		}
		stage2();
		end_dialog();
		dialog_active=0;
		reboot(RB_AUTOBOOT);
	} else {
		stage3();
		stage4();
		stage5();
	}
	return 0;
}
