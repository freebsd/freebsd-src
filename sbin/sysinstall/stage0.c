/*
 * Copyright (c) 1994, Jordan Hubbard, Paul Richards and Poul-Henning Kamp.
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 *
 * [Note: This file bears almost no resemblance to what was here in an
 * earlier incarnation].
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <dialog.h>

#include "sysinstall.h"

static unsigned char *welcome[] = {
    "1. README",
    "READ THIS FIRST.",
    "2. Release Notes",
     "Read the 2.0 Release Notes (recommended).",
    "3. Troubleshooting",
    "Read this in case of trouble.",
    "4. COPYRIGHT",
    "Read FreeBSD Copyright Information.",
    "5. Install",
    "Proceed with full installation.",
    "6. Fixit",
    "Repair existing installation (`fixit' mode).",
    "7. Quit",
    "Don't do anything, just reboot.",
};

void 
stage0()
{
evil_goto:
    if (dialog_menu("Welcome to FreeBSD!",
		    "Use ALT-F2 and ALT-F1 to toggle between debugging\ninformation screen (ALT-F2) or this dialog screen (ALT-F1)\n\nPlease select one of the following options:", 18, 75, 7, 7, welcome, selection))
	ExitSysinstall();

    switch (atoi(selection)) {
    case 1:	/* View the README */
        ShowFile(README_FILE, "Read Me First");
	goto evil_goto;
	break;

    case 2:	/* View the release notes */
        ShowFile(RELNOTES_FILE, "Release Notes");
	goto evil_goto;
	break;

    case 3:	/* View the troubleshooting file */
        ShowFile(TROUBLE_FILE, "Troubleshooting");
	goto evil_goto;
	break;

    case 4:	/* View copyrights */
        ShowFile(COPYRIGHT_FILE, "COPYRIGHT");
	goto evil_goto;
	break;

    case 5:	/* Proceed (do nothing special, really) */
	break;

    case 6:
	dialog_msgbox("Sorry!", "This feature is not currently implemented.",
		      6, 75, 1);
	goto evil_goto;
	break;

    case 7:
	/* Be neat.. */
	ExitSysinstall();
	break;	/* hope not! :) */
    }
}
