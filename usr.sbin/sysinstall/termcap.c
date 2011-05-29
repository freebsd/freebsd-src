/*
 * Copyright (c) 1994, Paul Richards.
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and sold, in both
 * source and binary form provided that the above copyright and these terms
 * are retained, verbatim, as the first lines of this file.  Under no
 * circumstances is the author responsible for the proper functioning of this
 * software, nor does the author assume any responsibility for damages
 * incurred with its use.
 *
 * $FreeBSD$
 */

#include "sysinstall.h"
#include <stdarg.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/consio.h>

#define VTY_STATUS_LINE    24
#define TTY_STATUS_LINE    23

static void
prompt_term(char **termp, char **termcapp)
{
    char str[80];
    static struct {
	const char *term, *termcap;
    } lookup[] = { { "ansi", termcap_ansi },
		   { "vt100", termcap_vt100 },
		   { "cons25", termcap_cons25 },
		   { "cons25-m", termcap_cons25_m },
		   { "xterm", termcap_xterm },
		   { "cons25w", termcap_cons25w } }; /* must be last */

    if (RunningAsInit) {
	while (1) {
	    int i;

	    printf("\nThese are the predefined terminal types available to\n");
	    printf("sysinstall when running stand-alone.  Please choose the\n");
	    printf("closest match for your particular terminal.\n\n");
	    printf("1 ...................... Standard ANSI terminal.\n");
	    printf("2 ...................... VT100 or compatible terminal.\n");
	    printf("3 ...................... FreeBSD system console (color).\n");
	    printf("4 ...................... FreeBSD system console (monochrome).\n\n");
	    printf("5 ...................... xterm terminal emulator.\n\n");
	    printf("Your choice: (1-5) ");
	    fflush(stdout);
	    fgets(str, sizeof(str), stdin);
	    i = str[0] - '0';
	    if (i > 0 && i < 6) {
		*termp = (char *)lookup[i - 1].term;
		*termcapp = (char *)lookup[i - 1].termcap;
		break;
	    }
	    else
		printf("\007Invalid choice, please try again.\n\n");
	}
    }
    else {
	printf("\nPlease set your TERM variable before running this program.\n");
	printf("Defaulting to an ANSI compatible terminal - please press RETURN\n");
	fgets(str, sizeof(str), stdin);	/* Just to make it interactive */
	*termp = (char *)"ansi";
	*termcapp = (char *)termcap_ansi;
    }
}

int
set_termcap(void)
{
    char           *term;
    int		   stat;
    struct winsize ws;

    term = getenv("TERM");
    stat = ioctl(STDERR_FILENO, GIO_COLOR, &ColorDisplay);

    if (!RunningAsInit) {
	if (isDebug())
	    DebugFD = open("sysinstall.debug", O_WRONLY|O_CREAT|O_TRUNC, 0644);
	else
	    DebugFD = -1;
	if (DebugFD < 0)
	    DebugFD = open("/dev/null", O_RDWR, 0);
    }

    if (!OnVTY || (stat < 0)) {
	if (!term) {
	    char *term, *termcap;

	    prompt_term(&term, &termcap);
	    if (setenv("TERM", term, 1) < 0)
		return -1;
	    if (setenv("TERMCAP", termcap, 1) < 0)
		return -1;
	}
	if (DebugFD < 0)
	    DebugFD = open("/dev/null", O_RDWR, 0);
    }
    else {
	int i, on;

	if (RunningAsInit) {
	    DebugFD = open("/dev/ttyv1", O_WRONLY);
	    if (DebugFD != -1) {
		on = 1;
		i = ioctl(DebugFD, TIOCCONS, (char *)&on);
		msgDebug("ioctl(%d, TIOCCONS, NULL) = %d (%s)\n",
			 DebugFD, i, !i ? "success" : strerror(errno));
	    }
	}

#ifdef PC98
	if (!term) {
	    if (setenv("TERM", "cons25w", 1) < 0)
		return -1;
	    if (setenv("TERMCAP", termcap_cons25w, 1) < 0)
		return -1;
	}
#else
	if (ColorDisplay) {
	    if (!term) {
		if (setenv("TERM", "xterm", 1) < 0)
		    return -1;
		if (setenv("TERMCAP", termcap_xterm, 1) < 0)
		    return -1;
	    }
	}
	else {
	    if (!term) {
		if (setenv("TERM", "vt100", 1) < 0)
		    return -1;
		if (setenv("TERMCAP", termcap_vt100, 1) < 0)
		    return -1;
	    }
	}
#endif
    }
    if (ioctl(0, TIOCGWINSZ, &ws) == -1) {
	msgDebug("Unable to get terminal size - errno %d\n", errno);
	ws.ws_row = 0;
    }
    StatusLine = ws.ws_row ? ws.ws_row - 1: (OnVTY ? VTY_STATUS_LINE : TTY_STATUS_LINE);
    return 0;
}
