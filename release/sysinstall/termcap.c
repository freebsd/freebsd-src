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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <machine/console.h>

#include "sysinstall.h"

int
set_termcap(void)
{
    char           *term;
    int		   stat;

    OnVTY = OnSerial = FALSE;
    if (getpid() != 1)
	DebugFD = open("sysinstall.debug", O_WRONLY|O_CREAT|O_TRUNC, 0644);
    else
	RunningAsInit = TRUE;
    term = getenv("TERM");
    stat = ioctl(STDERR_FILENO, GIO_COLOR, &ColorDisplay);
    if (stat < 0) {
	if (!term) {
	    if (setenv("TERM", "vt100", 1) < 0)
		return -1;
	    if (setenv("TERMCAP", termcap_vt100, 1) < 0)
		return -1;
	}
	if (DebugFD == -1)
	    DebugFD = dup(1);
	OnSerial = TRUE;
    }
    else {
	if (ColorDisplay) {
	    if (!term) {
		if (setenv("TERM", "cons25", 1) < 0)
		    return -1;
		if (setenv("TERMCAP", termcap_cons25, 1) < 0)
		    return -1;
	    }
	}
	else {
	    if (!term) {
		if (setenv("TERM", "cons25-m", 1) < 0)
		    return -1;
		if (setenv("TERMCAP", termcap_cons25_m, 1) < 0)
		    return -1;
	    }
	}
	if (DebugFD == -1)
	    DebugFD = open("/dev/ttyv1", O_WRONLY);
	OnVTY = TRUE;
    }
    return 0;
}
