/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id$
 *
 * Jordan Hubbard
 *
 * My contributions are in the public domain.
 *
 * Parts of this file are also blatently stolen from Poul-Henning Kamp's
 * previous version of sysinstall, and as such fall under his "BEERWARE"
 * license, so buy him a beer if you like it!  Buy him a beer for me, too!
 */

#include "sysinstall.h"
#include <signal.h>
#include <sys/reboot.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

/* Handle interrupt signals (duh!) */
static void
handle_intr(int sig)
{
}

/* Welcome the user to the system */
void
systemWelcome(void)
{
}

/* Initialize system defaults */
void
systemInitialize(int argc, char **argv)
{
    signal(SIGINT, SIG_IGN);
    globalsInit();

    /* Are we running as init? */
    if (getpid() == 1) {
	setsid();
	if (argc > 1 && strchr(argv[1],'C')) {
	    /* Kernel told us that we are on a CDROM root */
	    close(0); open("/bootcd/dev/console", O_RDWR);
	    close(1); dup(0);
	    close(2); dup(0);
	    CpioFD = open("/floppies/cpio.flp", O_RDONLY);
	    OnCDROM = TRUE;
	    chroot("/bootcd");
	} else {
	    close(0); open("/dev/console", O_RDWR);
	    close(1); dup(0);
	    close(2); dup(0);
	}
	msgInfo("%s running as init", argv[0]);

	ioctl(0, TIOCSCTTY, (char *)NULL);
	setlogin("root");
	setbuf(stdin, 0);
	setbuf(stdout, 0);
	setbuf(stderr, 0);
    }

    if (set_termcap() == -1)
	msgFatal("Can't find terminal entry");

    /* XXX - libdialog has particularly bad return value checking */
    init_dialog();
    /* If we haven't crashed I guess dialog is running ! */
    DialogActive = TRUE;

    signal(SIGINT, handle_intr);
}

/* Close down and prepare to exit */
void
systemShutdown(void)
{
    if (DialogActive) {
	end_dialog();
	DialogActive = FALSE;
    }
    /* REALLY exit! */
    if (getpid() == 1)
	reboot(RB_HALT);
}

int
systemExecute(char *command)
{
    int status;

    dialog_clear();
    dialog_update();
    end_dialog();
    DialogActive = FALSE;
    status = system(command);
    DialogActive = TRUE;
    dialog_clear();
    dialog_update();
    return status;
}

