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
#include <machine/console.h>

#include "sysinstall.h"

int
set_termcap()
{
	char           *term;
	extern const char termcap_vt100[];
	extern const char termcap_cons25[];
	extern const char termcap_cons25_m[];

	term = getenv("TERM");
	if (term == NULL) {
		int     color_display;

		if (ioctl(STDERR_FILENO, GIO_COLOR, &color_display) < 0) {
			if (setenv("TERM", "vt100", 1) < 0)
				return -1;
			if (setenv("TERMCAP", termcap_vt100, 1) < 0)
				return -1;
			debug_fd = dup(1);
			on_serial++;
		} else if (color_display) {
			if (setenv("TERM", "cons25", 1) < 0)
				return -1;
			if (setenv("TERMCAP", termcap_cons25, 1) < 0)
				return -1;
		        debug_fd = open("/dev/ttyv1",O_WRONLY);
		} else {
			if (setenv("TERM", "cons25-m", 1) < 0)
				return -1;
			if (setenv("TERMCAP", termcap_cons25_m, 1) < 0)
				return -1;
		        debug_fd = open("/dev/ttyv1",O_WRONLY);
		}
	} else {
		debug_fd = open("sysinstall.debug",
			O_WRONLY|O_CREAT|O_TRUNC,0644);
	}
	return 0;
}
